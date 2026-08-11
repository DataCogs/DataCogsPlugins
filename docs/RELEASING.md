# Releasing

This repo is a working showcase of hands-free JUCE plugin releases with pure
CMake - no Projucer - including AAX signing with PACE's cloud signing tools.
The whole path from tag to notarized installer runs in CI.

## The flow

```
push to dev  ->  PR / merge to main  ->  CI: build suite + all tests (macOS, Windows)
tag vX.Y.Z   ->  CI: build -> PACE cloud-sign AAX -> fetch IR library
                     -> productbuild suite installer -> notarize + staple
                     -> draft GitHub release with installer + per-plugin zips
```

- CI builds the **whole suite** on every merge (not per-plugin path filters):
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
  password only ever travels via the environment, never argv.

PACE's constraints to know about: Mac AAX must be signed on a Mac (and
Windows AAX on Windows), and an iLok Cloud session is per-machine - if you
ever parallelise signing jobs, each runner needs its own iLok account.

## Cutting a release

1. Merge `dev` -> `main` and let CI go green.
2. `git tag v0.2.0 && git push origin v0.2.0`
3. CI drafts the GitHub release with the notarized installer attached;
   review and publish.

## Required repository secrets

| Secret | Purpose |
|---|---|
| `PACE_TOOLS_URL` | fetchable copy of the PACE Code Signing for AAX SDK (wraptool) installer |
| `ILOK_TOOLS_URL` | fetchable copy of the iLok License Support installer (iloktool) |
| `PACE_ACCOUNT` / `PACE_PASSWORD` | iLok account with the Cloud-enabled PACE Tools license |
| `PACE_WCGUID` | wrap configuration GUID for the suite |
| `APPLE_CERT_P12_BASE64` / `APPLE_CERT_PASSWORD` | Developer ID Application + Installer certs (one .p12, base64) |
| `APPLE_TEAM_ID` | Apple team id for codesign/productbuild identities |
| `APPLE_NOTARY_APPLE_ID` / `APPLE_NOTARY_PASSWORD` | notarytool credentials |
| `GCP_SA_KEY` | read access to the private IR library bucket |

## Local installer build

```sh
cmake -S . -B build && cmake --build build -j8
./packaging/build-installer.sh --sign-aax \
    --sign "Developer ID Installer: Your Name (TEAMID)"
```

Produces `dist/DataCogs Plugins-<version>.pkg` with two components: the
plugins and the reverb's IR library (which the plugin finds via its
system-wide search root; users' own libraries always shadow it).

## Still to do

- Windows installer (Inno Setup) - Windows currently ships per-plugin zips.
- Final license decision (GPL-family expected, given JUCE's terms).
- Redistribution review for the third-party IR collections + attribution
  notes for the AI-generated panel artwork.
