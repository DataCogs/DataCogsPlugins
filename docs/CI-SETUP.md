# Setting up the release pipeline from scratch

Everything the tag-triggered release job needs, and exactly how to create it.
Done once per repo (or once per fork, if you're adapting this suite). When
you're finished, `gh secret list` should show every secret from the table
in [RELEASING.md](RELEASING.md) — that table is the source of truth.

Prerequisites: an Apple Developer Program membership, a PACE account with the
Cloud-enabled PACE Tools license (see [PACE support](mailto:support@paceap.com)),
a private GCS bucket, and the `gh` and `gcloud` CLIs authenticated locally.

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

## 5. Verify and rehearse

```sh
gh secret list        # everything from the RELEASING.md table present?
git tag v0.1.0 && git push origin v0.1.0
```

The tag fires the full pipeline: build → PACE cloud-sign AAX → fetch IR
library → signed installer → notarize + staple → draft GitHub release.
Expect the first run to fail somewhere specific (notarization is the usual
suspect) — each step fails loudly on its own, so fix and re-tag.
