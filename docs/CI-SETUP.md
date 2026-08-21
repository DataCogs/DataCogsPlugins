# Setting up the release pipeline from scratch

Everything the tag-triggered release job needs, and exactly how to create it.
Done once per repo (or once per fork, if you're adapting this suite). When
you're finished, `gh secret list` should show every secret from the table
in [RELEASING.md](RELEASING.md) — that table is the source of truth.

## 0. Memberships and programs you need first

None of the signing below works without these relationships in place, and
some take days or weeks to approve — start them before anything else:

- **Apple Developer Program** (US$99/year) — required for Developer ID
  certificates and notarization. Without it, macOS builds load only on
  machines with Gatekeeper disabled.
- **Avid developer registration** (free) — gives access to the AAX SDK (JUCE 8
  bundles it, but you still need the agreement) and to the **Pro Tools
  Developer build**, which is the only Pro Tools that loads *unsigned* AAX.
  Essential for testing AAX locally before signing is set up.
- **PACE signing tools via Avid** — retail Pro Tools only loads PACE-signed
  AAX. Request the PACE Tools license through the Avid developer portal as an
  approved AAX developer: it's free that way (list price about US$500) and is
  activated to a physical iLok USB. PACE's *cloud* signing — what CI uses here,
  no hardware — is a separate product at about US$1,000 per year (prices at time of
  writing); we ran it on a trial. Budget for it only if you want hands-free
  signing on runners; the free USB license covers local signing fully.
- **Azure subscription** — for Artifact Signing (~US$10/month); the Windows
  Authenticode layer PACE's wrap configuration requires.
- A private GCS bucket (or equivalent) for the IR library and the PACE
  installers, plus the `gh`, `gcloud` and `az` CLIs authenticated locally.

Throughout: `gh secret set NAME` prompts for the value on stdin, so
credentials never land in your shell history.

## 1. PACE

```sh
gh secret set PACE_ACCOUNT     # your iLok account ID
gh secret set PACE_PASSWORD    # its password (used by iloktool to open the cloud session)
gh secret set PACE_WCGUID      # wrap configuration GUID from your PACE product setup
```

Best practice: use a **dedicated CI iLok account** holding only the PACE Tools
license — iLok Cloud sessions are per-machine, so CI signing with your personal
account will fight your dev machine's session (and a leaked credential exposes
only the signing license).

Then upload the **PACE Code Signing for AAX SDK** installer (from PACE Central)
to your private bucket — PACE's installers are not yours to host publicly:

```sh
gcloud storage cp PACECodeSigningForAAXSDKMac_v6.0.0.zip \
    gs://YOUR-BUCKET/ci-tools/
```

The v6 SDK pkg installs wraptool, the License Service and iLok License Manager
(including `iloktool`) in one shot — it's the only tool the job installs.

## 2. Apple signing certificates

You need two identities in your login keychain, both from the same team:
**Developer ID Application** (signs plugins, and the codesign layer of AAX)
and **Developer ID Installer** (signs the .pkg). Check what you have:

```sh
security find-identity -v | grep "Developer ID"
```

If **Developer ID Installer** is missing:

1. Keychain Access (⌘-Space, "Keychain Access" — not the Passwords app) →
   menu bar → **Keychain Access → Certificate Assistant → Request a
   Certificate From a Certificate Authority…** → enter your email, choose
   **Saved to disk**.
2. [developer.apple.com](https://developer.apple.com/account/resources/certificates/list)
   → Certificates → **+** → pick **Developer ID Installer** (not Application —
   they're different rows in the same list) → upload the CSR → download the
   `.cer`.
3. Import it into the **login** keychain: select "login" in Keychain Access's
   sidebar first, then File → Import Items (double-clicking imports into
   whatever keychain is selected — if you see *"The System Roots keychain
   cannot be modified"*, that's what happened; select "login" and retry).
4. Re-run the `security find-identity` check: the Installer identity should
   now be listed as valid (cert + private key paired).

Export both identities as one file: in Keychain Access → login → My
Certificates, ⌘-click both Developer ID certs → right-click → **Export 2
items…** → PKCS #12 (`certs.p12`) with a password. Then:

```sh
base64 -i certs.p12 | gh secret set APPLE_CERT_P12_BASE64
gh secret set APPLE_CERT_PASSWORD    # the p12 password
rm certs.p12                         # never leave it lying around
gh secret set APPLE_TEAM_ID          # e.g. ABCDE12345 (in your cert names)
```

## 3. Apple notarization

Notarization uses an app-specific password, not your real Apple ID password:
[account.apple.com](https://account.apple.com) → Sign-In and Security →
**App-Specific Passwords** → generate one (name it after this pipeline).

```sh
gh secret set APPLE_NOTARY_APPLE_ID   # your Apple ID email
gh secret set APPLE_NOTARY_PASSWORD   # the app-specific password
```

## 4. Google Cloud (content bucket)

A dedicated read-only service account, so the CI credential can fetch the IR
library and CI tools and nothing else:

```sh
gcloud iam service-accounts create datacogs-plugins-ci \
    --display-name="DataCogs Plugins CI (read-only)"
gcloud storage buckets add-iam-policy-binding gs://YOUR-BUCKET \
    --member="serviceAccount:datacogs-plugins-ci@YOUR-PROJECT.iam.gserviceaccount.com" \
    --role="roles/storage.objectViewer"
gcloud iam service-accounts keys create key.json \
    --iam-account=datacogs-plugins-ci@YOUR-PROJECT.iam.gserviceaccount.com
gh secret set GCP_SA_KEY < key.json
rm key.json
```

(If the IAM binding says the service account "does not exist" seconds after
creating it, that's propagation lag — wait a few seconds and retry.)

## 5. Azure Artifact Signing (Windows Authenticode)

Windows code signing uses **Azure Artifact Signing** (also known as Trusted
Signing): a fully managed service (~US$10/month, Basic tier, 5,000
signatures/month) where the private key lives in Azure and is
non-exportable — no PFX files to protect, no USB token. CI authenticates as
a service principal, which is exempt from Azure's user MFA enforcement (an
unattended workload can't do MFA; that's what workload identities are for).

Setup, in order — the exact path we took. Portal for the human-review
steps, `az` CLI for everything scriptable (install with `brew install
azure-cli`, sign in with `az login --use-device-code`). Start early: step 2
is a human review that took about a week for us.

1. **Create the signing account** (portal): create an *Artifact Signing
   account* (older screens say Trusted Signing) — ours: name `datacogs`,
   resource group `datacogs_rg`, region West US 2, Basic tier. The region
   fixes the endpoint URL: West US 2 is `https://wus2.codesigning.azure.net`.
2. **Identity validation** (portal — the slow step): first grant your own
   user the **Artifact Signing Identity Verifier** role on the account
   (Access control (IAM)), then Objects → Identity validations → New
   Identity → **Public** (Public feeds Public Trust profiles — publicly
   distributed software; Private is enterprise-internal only) → **Organization**
   (company name on signatures; needs a registered entity and a business
   identifier such as an ABN/DUNS, with matching address and phone) or
   **Individual** (faster; your name on signatures).
3. **Certificate profile** (portal, after validation): Objects →
   Certificate profiles → Create → type **Public Trust**, *Program type:
   None* (the "Windows endpoint security platform" option is for
   antimalware vendors), select the validated identity as CN and O, name
   it — ours is `datacogs-public-trust`. The CLI extension can list
   profiles but cannot look up validation IDs, so this stays a portal step:
   `az extension add --name trustedsigning` then
   `az trustedsigning certificate-profile list -g datacogs_rg --account-name datacogs`.
4. **App registration** (portal): Microsoft Entra ID → App registrations →
   New registration (single tenant; ours is `codesigning-app`) →
   Certificates & secrets → New client secret → copy the **Value**
   immediately (not the Secret ID — that's just a record identifier and
   will fail authentication). The IDs are readable later with
   `az ad app list --display-name codesigning-app` and
   `az account show --query tenantId`.
5. **Role assignment** (CLI) — grant the app the signing role, scoped to
   the account only:

```sh
APP=$(az ad app list --display-name codesigning-app --query "[0].appId" -o tsv)
SCOPE=$(az resource show -g datacogs_rg -n datacogs \
    --resource-type Microsoft.CodeSigning/codeSigningAccounts --query id -o tsv)
az role assignment create --assignee "$APP" \
    --role "Artifact Signing Certificate Profile Signer" --scope "$SCOPE"
az role assignment list --scope "$SCOPE" -o table   # verify
```

6. **Set the secrets**:

```sh
gh secret set AZURE_TENANT_ID          # from the app registration
gh secret set AZURE_CLIENT_ID
gh secret set AZURE_CLIENT_SECRET
gh secret set AZURE_SIGNING_ENDPOINT   # e.g. https://wus2.codesigning.azure.net
gh secret set AZURE_SIGNING_ACCOUNT    # e.g. datacogs
gh secret set AZURE_SIGNING_PROFILE    # the certificate profile name
```

The workflow's signing step (azure/artifact-signing-action) activates
automatically once the secrets exist — it signs the Windows installer exe
after the Inno build, with an RFC 3161 timestamp so signatures outlive the
certificate.

Two notes:

- **Hardening option**: the action also supports OIDC federated
  credentials (GitHub's runner authenticates to Azure directly - no stored
  client secret at all). Worth switching to once the pipeline is stable.
- **AAX integration**: wraptool applies the Authenticode layer through a
  signtool wrapper (`packaging/windows/aax-signtool.bat`), following PACE's
  "Azure Digital Signing" tutorial
  (https://docs.paceap.com/fusion-protection/tutorials/azure-digital-signing):
  CI installs a current signtool + the Artifact Signing dlib via NuGet,
  the packaging script writes `metadata.json` from the secrets, and
  wraptool is invoked with `--signtool <wrapper> --signid placeholder`
  (the placeholder is required but unused). Requires .NET 8 exactly and
  x64 for both signtool and the dlib - mismatches fail cryptically.

## 6. Verify and rehearse

```sh
gh secret list        # everything from the RELEASING.md table present?
git tag v0.1.0 && git push origin v0.1.0
```

The tag fires the full pipeline: build → PACE cloud-sign AAX → fetch IR
library → signed installer → notarize + staple → draft GitHub release.
Expect the first run to fail somewhere specific (notarization is the usual
suspect) — each step fails loudly on its own, so fix and re-tag.
