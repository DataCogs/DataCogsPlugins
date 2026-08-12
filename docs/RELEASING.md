# Releasing

This repo is a working showcase of hands-free JUCE plugin releases with pure
CMake - no Projucer - including AAX signing with PACE's cloud signing tools.
The whole path from tag to notarized installer runs in CI.

## The flow

```
push to dev  ->  PR to main  ->  CI: build suite + all tests (macOS)
                                 = the merge gate; no duplicate build on push
tag vX.Y.Z   ->  CI: build -> PACE cloud-sign AAX -> fetch IR library
                     -> productbuild suite installer -> notarize + staple
                     -> draft GitHub release with installer + per-plugin zips
```

Branch protection (once the repo is public / on a paid plan) enforces the
gate mechanically: the `macOS` check must pass and the PR must be up to date
with main before merging - so the PR tree is exactly the merge result.

- CI builds the **whole suite** on every PR (not per-plugin path filters):
  the plugins share `common/`, so a change there must prove all three still
  build and pass tests. Docs-only changes skip CI.
- `fail-fast` is on: any platform failure aborts the run.
- Superseded runs are auto-cancelled (`concurrency` group per ref).

## AAX signing: one recipe, two modes

All signing lives in `packaging/build-installer.sh --sign-aax` (and, for dev
builds, `common/cmake/DataCogsAAX.cmake`, which signs the locally installed
bundle after each build):

- **Local / USB iLok**: run the script with a physical iLok connected (or an
  iLok Cloud session open in iLok License Manager) - no password needed.
- **CI / PACE cloud signing** (per PACE's "Code Signing of AAX plug-ins
  utilizing the iLok Cloud" guide): export `PACE_PASSWORD` and the script
  opens the session itself - `iloktool cloud --open` - then signs with
  `wraptool ... --allowsigningservice`, closing the session on exit. The
  password enters the scripts only via the environment (never a script
  argument, never logged); it is passed on to `iloktool --password` and, in
  cloud mode, `wraptool --password` - PACE's interfaces - briefly visible
  in the ephemeral runner's process list. (PACE's guide shows wraptool
  without a password once the session is open, but wraptool v6 demands one
  when credentials aren't in the default keychain, as in CI.)

PACE's constraints to know about: Mac AAX must be signed on a Mac (and
Windows AAX on Windows), and an iLok Cloud session is per-machine - if you
ever parallelise signing jobs, each runner needs its own iLok account.

## Cutting a release

1. Merge `dev` -> `main` and let CI go green.
2. `git tag v0.2.0 && git push origin v0.2.0`
3. CI drafts the GitHub release with the notarized installer attached;
   review and publish.

## Required repository secrets

Step-by-step instructions for creating every one of these — certificates,
CSRs, app-specific passwords, the service account — live in
[CI-SETUP.md](CI-SETUP.md).

| Secret | Purpose |
|---|---|
| `PACE_ACCOUNT` / `PACE_PASSWORD` | iLok account with the Cloud-enabled PACE Tools license |
| `PACE_WCGUID` | wrap configuration GUID for the suite |
| `APPLE_CERT_P12_BASE64` / `APPLE_CERT_PASSWORD` | Developer ID Application + Installer certs (one .p12, base64) |
| `APPLE_TEAM_ID` | Apple team id for codesign/productbuild identities |
| `APPLE_NOTARY_APPLE_ID` / `APPLE_NOTARY_PASSWORD` | notarytool credentials |
| `GCP_SA_KEY` | read access to the private IR library bucket |

## CI tooling

The PACE Code Signing for AAX SDK installer lives in the private bucket at
`gs://datacogs-ir-library/ci-tools/` (PACE's installers aren't ours to host
publicly). The v6 SDK pkg installs wraptool, the License Service and iLok
License Manager (including `iloktool`) in one shot, so it's the only tool
the release job needs beyond the runner image.

## Local installer build

```sh
cmake -S . -B build && cmake --build build -j8
./packaging/build-installer.sh --sign-aax \
    --sign "Developer ID Installer: Your Name (TEAMID)"
```

Produces `dist/DataCogs Plugins-<version>.pkg` with two components: the
plugins and the reverb's IR library (which the plugin finds via its
system-wide search root; users' own libraries always shadow it).

To remove an installed suite: `sudo packaging/uninstall.sh` (add
`--with-irs` to also remove the system IR library). Personal IR libraries
and per-user plugin folders are never touched.

## Windows

The Windows leg mirrors the macOS one: CI builds + tests on `windows-latest`,
and tagged releases run `packaging/windows/build-installer.ps1`, which stages
VST3 + AAX + the IR library and compiles `packaging/windows/DataCogsPlugins.iss`
with Inno Setup (installer targets: `Common Files\VST3`,
`Common Files\Avid\Audio\Plug-Ins`, `ProgramData\DataCogs\Impulse Responses`;
Inno provides the uninstaller in Add/Remove Programs).

AAX signing happens on Windows (PACE requires signing on the target OS): upload
the PACE Code Signing SDK **Windows** installer to
`gs://datacogs-ir-library/ci-tools/PACECodeSigningForAAXSDKWin.zip` and the
release job installs it and cloud-signs the staged bundles; without it the
installer ships unsigned AAX and warns loudly in the log.

## Still to do
- Redistribution review for the third-party IR collections + attribution
  notes for the AI-generated panel artwork.
- Authenticode signing for the Windows installer + binaries (needs a Windows
  code signing certificate; until then SmartScreen will warn on first run).
- Upload the PACE Windows SDK to the bucket so Windows AAX gets cloud-signed.
