# DataCogs Plugins

A suite of audio plugins (AU / VST3 / AAX / Standalone) built with [JUCE 8](https://juce.com) —
free, open source, and written to be read.

It's also a working reference for **modern JUCE infrastructure without the Projucer**:
the whole suite is pure CMake, wired for professional-grade CI and hands-free releases —
every pull request builds all plugins and runs every test suite, and a single git tag
produces signed, notarized installers for macOS and Windows with no human in the loop —
including AAX signing via PACE's cloud signing service and Windows Authenticode via
Azure Artifact Signing. It grew out of real production pain: converting a plugin company's
manual Projucer build-and-package workflow — four-plus hours per release — into an
automated CMake pipeline, as a side project that ended up saving hours every release.
The gotchas from that journey (and this one) are documented throughout this repo. If
you're wrestling a manual Projucer flow into automated CI, this repo is meant to be
copied from.

## Why this exists

Most commercial plugins are black boxes: you get the sound, but not the *how*. This suite
takes the opposite bet — that a plugin can be **professional quality and completely
transparent** at the same time:

- **The DSP is documented where it lives.** One-pole coefficient derivations, dB conventions,
  knee math, detector ballistics — the theory is written up inline in the headers, next to
  the code that implements it. Each plugin's README covers its design decisions and
  listening tests.
- **Everything is testable and tested.** The DSP classes are plain C++, JUCE-independent
  where possible, with Catch2 suites that pin down the maths — not just "it compiles".
  Audio goes through the real processors and the output is checked against what the
  theory says: a sweep through the compressor is compared sample-by-sample with a
  known-good render, the EQ's bell must boost exactly at its centre frequency, a
  Schroeder measurement must recover the reverb's requested RT60, and a Dirac impulse
  through the convolution engine must reproduce real measured rooms sample-exactly.
- **It's open to everyone.** Use the plugins in your sessions, read the code to learn how
  compressors and EQs actually work, fork it, or contribute back. The point is that good
  audio tools — and the knowledge inside them — shouldn't be gated.
- **Transparency is a security property.** Audio plugins are native code you invite onto
  your machine, and closed-source plugins ask you to trust an opaque binary produced by
  an opaque process. Here you can audit not just the DSP but the exact pipeline that
  compiled, signed and packaged the installer you download — the CI logs of every
  release build are public.

## The plugins

- **DataCogs Compressor** — dynamic range compressor: Classic and Log-Domain
  topologies, Peak/RMS detection, sidechain HPF, Giannoulis/Massberg/Reiss soft knee,
  parallel mix, range-capped gain reduction. [README](plugins/compressor/README.MD)
- **DataCogs EQ** — six-band parametric EQ of RBJ-cookbook biquads (bell, shelves,
  passes, notch) under a draggable response curve with a post-EQ spectrum analyser.
  [README](plugins/parametric-eq/README.md)
- **DataCogs Reverb** — zero-latency partitioned convolution over measured and
  synthetic IRs, IR-domain size/decay/early-tail controls, reverse, and a bundled IR
  library with artwork. [README](plugins/convolution-reverb/README.md)

All three share the DataCogs house look — machined-steel theme, cog knobs, engraved
faceplate labels — implemented once in [`common/`](common/) and reused by every plugin.

## Building

Requires CMake 3.22+ and a C++20 compiler. Dependencies (JUCE 8, Catch2) are fetched
automatically via CPM on first configure.

```sh
cmake -S . -B build                    # configure (downloads deps on first run)
cmake --build build -j8                # build the whole suite
ctest --test-dir build                 # run all tests
```

Individual formats per plugin are available as targets, e.g.
`CompressorPlugin_AU`, `ParametricEQPlugin_VST3`, `ConvolutionReverbPlugin_Standalone`.

By default (`COPY_PLUGIN_AFTER_BUILD=ON`) each built format is installed into the
system plugin folders. CI and packaging builds pass `-DCOPY_PLUGIN_AFTER_BUILD=OFF`.

The release pipeline — CI, PACE cloud AAX signing on both platforms, Azure
Authenticode signing, notarized macOS pkg and Inno Setup Windows installer —
is documented in [docs/RELEASING.md](docs/RELEASING.md), with a from-scratch
setup walkthrough (certificates, secrets, service accounts) in
[docs/CI-SETUP.md](docs/CI-SETUP.md). Agent/contributor build rules live in
[AGENTS.md](AGENTS.md).

### Memberships you need to ship this for real

Building is free; *shipping* signed plugins needs four relationships, and the
approvals take time — start them first (details in [docs/CI-SETUP.md](docs/CI-SETUP.md)):

- **Apple Developer Program** (US$99/yr) — Developer ID certificates and notarization.
- **Avid developer registration** (free) — the AAX SDK agreement (JUCE 8 bundles the
  SDK itself) and the **Pro Tools Developer build**, the only Pro Tools that loads
  *unsigned* AAX. That's how you test AAX locally before signing exists.
- **PACE signing tools** — retail Pro Tools loads only PACE-signed AAX. The standard
  PACE Tools license (activated to a physical iLok USB) lists at about US$500 but is
  **free when requested through the Avid developer portal** as an approved AAX
  developer. The *cloud* signing shown here — no iLok hardware, usable on CI runners —
  is a separate PACE product at about US$1,000 per year (prices at time of writing; a trial in
  our case). So the only recurring cost this pipeline actually needs beyond Apple and
  Azure is that cloud option, and it's optional: the same scripts sign from the free
  USB license on a developer machine.
- **Azure subscription** (~US$10/month) — Artifact Signing provides the Windows
  Authenticode layer that PACE's wrap configuration requires.

## PACE cloud signing: best practices

This repo signs AAX in CI with **PACE cloud signing** — no physical iLok on the
runner — following PACE's guide *"Code Signing of AAX plug-ins utilizing the iLok
Cloud without use of a physical iLok USB key for continuous integration (CI)
environments"* (available from [PACE support](mailto:support@paceap.com); see also
PACE's [wraptool reference](https://docs.paceap.com/fusion-protection/reference/wraptool/overview)
and [SDK installation docs](https://docs.paceap.com/fusion-protection/getting-started/shared/installation)
covering silent installs for CI).

The flow, as implemented in [`packaging/build-installer.sh`](packaging/build-installer.sh)
and [`.github/workflows/ci.yml`](.github/workflows/ci.yml):

1. Install the **PACE Code Signing for AAX SDK** on the runner - the v6 pkg
   installs `wraptool`, the License Service and iLok License Manager
   (which provides `iloktool`) in one shot.
2. Open the cloud session headlessly: `iloktool cloud --open --account … --password …`
3. Sign with `wraptool sign … --allowsigningservice`
4. **Close the session on exit** (`iloktool cloud --close`), success or failure —
   sessions otherwise stay open indefinitely.

Lessons worth copying:

- **Use a dedicated iLok account for CI.** An iLok Cloud session is per-machine and
  cannot be shared: if CI signs with your personal account, it will fight your
  development machine's session. A CI-only account (holding just the PACE Tools
  license) also limits what a leaked credential exposes — ask PACE to deposit the
  license to a separate account for CI.
- **Be precise about credential exposure.** The account password enters the
  scripts only via environment variables — never as a script argument, never
  logged. It is passed as an argv flag only to PACE's own tools (`iloktool
  cloud --open --password …` and, in cloud mode, `wraptool --password …` —
  v6 demands it when credentials aren't in the default keychain, doc
  examples notwithstanding), briefly visible in the ephemeral runner's
  process list. Secrets are exposed solely to the
  tag-triggered release job, and GitHub never provides secrets to fork PRs.
- **Sign on the matching OS.** Mac AAX must be signed on a Mac, Windows AAX on
  Windows — hence separate macOS and Windows packaging jobs.
- **One cloud session per account — even across CI jobs.** The two packaging jobs
  share one CI iLok account, so they run serialized; in parallel, the second session
  would kick the first out mid-sign.
- **A green run must mean fully signed.** Early on, missing signing tools produced
  a *warning* and an unsigned installer — and a green checkmark. The packaging
  scripts now run with `REQUIRE_SIGNED=1` in CI and hard-fail up front if anything
  needed for complete signing is missing. Silence masked as success is how unsigned
  builds slip out.
- **Keep a non-cloud fallback.** The same script signs from a physical USB iLok
  (or a locally opened cloud session) with zero flags changed — releases don't
  stop if cloud access lapses.
- **Support both dev and CI in one recipe.** Local builds sign the installed
  bundle post-build (`common/cmake/DataCogsAAX.cmake`); CI signs during packaging
  (`--sign-aax`). One wrap-config GUID covers the whole suite.
- **wraptool signs AAX — nothing else.** The AU and VST3 bundles still need a
  regular `codesign --options runtime --timestamp` with your Developer ID
  Application cert before packaging, or notarization will reject the installer
  for every unsigned binary in it.
- **The trial ends; the pipeline doesn't.** PACE's cloud signing here runs on a
  time-limited trial account. When it lapses, the same scripts sign from a local
  iLok on a developer machine and the installers are attached to the release by
  hand — the cloud-signing code stays as the reference. See the sunset plan in
  [docs/RELEASING.md](docs/RELEASING.md).

### Windows AAX: Authenticode through Azure Artifact Signing

PACE's wrap configuration requires a platform digital signature on the AAX — Apple
codesign on macOS, **Authenticode on Windows**. We use
[Azure Artifact Signing](https://learn.microsoft.com/azure/trusted-signing/) (formerly
Trusted Signing, ~US$10/month), where the private key never leaves Azure. That
rules out wraptool's `--keyfile`/`--signid`, which want a key in hand; instead,
following PACE's
[Azure Digital Signing tutorial](https://docs.paceap.com/fusion-protection/tutorials/azure-digital-signing),
wraptool calls a small signtool **wrapper** (`packaging/windows/aax-signtool.bat`)
that re-invokes a current `signtool.exe` with the Azure dlib — see
[docs/CI-SETUP.md](docs/CI-SETUP.md) for the setup. Lessons:

- The SDK-bundled `signtool.exe` is too old; CI NuGet-installs
  `Microsoft.Windows.SDK.BuildTools` and `Microsoft.ArtifactSigning.Client`, x64
  only — an architecture mismatch fails with misleading certificate errors, and
  a .NET runtime other than 8 fails silently.
- `--signid` is still mandatory even though the wrapper ignores it; any
  placeholder satisfies wraptool.
- PACE's published wrapper snippet reads its temp file from the *current working
  directory*, but wraptool invokes the wrapper from elsewhere — resolve the file
  beside the script. Its `echo` also writes a CRLF that the path regex must strip.
- Identity validation for a Public Trust certificate profile can take hours to
  weeks and wants a business identifier — start it first.

### Troubleshooting notarization

- **`HTTP 401 Invalid credentials`** — the password must be an *app-specific
  password* from account.apple.com (format `xxxx-xxxx-xxxx-xxxx`), and the
  Apple ID must be the one that owns your team. Validate the exact trio
  locally before burning a CI run:
  ```sh
  xcrun notarytool history --apple-id "you@example.com" \
      --password "xxxx-xxxx-xxxx-xxxx" --team-id "TEAMID"
  ```
- **`status: Invalid`** — Apple rejected the archive's contents. The verdict
  names every offending binary; fetch it with the submission id from the CI
  log:
  ```sh
  xcrun notarytool log <submission-id> --apple-id "you@example.com" \
      --password "xxxx-xxxx-xxxx-xxxx" --team-id "TEAMID"
  ```
  Typical findings: "binary is not signed" / "no secure timestamp" — some
  binary missed the hardened-runtime Developer ID codesign (see above).
- **wraptool: "neither --keyfile nor --signid"** (Windows) — the wrap config
  demands an Authenticode layer; see the Azure section above.
- **wraptool Error 38 / ErrorSigningTarget** — you're signing an already-wrapped
  binary; sign the unwrapped build output.
- **Retry cheaply.** After fixing *secrets*, use the run's "Re-run failed
  jobs" — it reuses the built artifacts and current secret values. Script or
  workflow changes need a fresh tag: the tagged commit is what CI executes.

## Related projects

- [Pamplejuce](https://github.com/sudara/pamplejuce) — the best-known JUCE + CMake
  template: Catch2, pluginval, notarization, Azure Trusted Signing, GitHub Actions.
  It's a scaffold with a pass-through plugin in it — the shape of a pipeline, to be
  filled in. This repo is the other thing: a finished product going through one —
  real DSP, tests that push audio through it against known-good output, and the
  AAX/PACE last mile templates leave out.
- [KoalaDSP's Azure code-signing guide](https://github.com/koaladsp/KoalaDocs/blob/master/azure-code-signing-for-plugin-developers.md)
  — the origin of the wraptool-plus-signtool-wrapper technique PACE now documents.
- [Moonbase's CI for audio plugins](https://moonbase.sh/articles/continuous-integration-for-audio-plugins-tips-tricks-gotchas/)
  and [code-signing round-up](https://moonbase.sh/articles/code-signing-audio-plugins-in-2025-a-round-up/) — broader context on the landscape.

## Contributing

Issues and pull requests are welcome — bug reports, DSP improvements, listening-test
results, new presets, documentation fixes, ports. Two ground rules:

1. **Keep the transparency.** If you change DSP behaviour, update the inline theory
   notes and the tests that pin it down.
2. **Match the house style.** UI work should go through the shared look-and-feel in
   `common/` so the suite stays coherent.

## Acknowledgements

Developed by [DataCogs](https://datacogs.com), with AI assistance from
Anthropic's Claude for UI implementation, build/CI wiring, and documentation.

Thanks to Derek and the support team at [PACE](https://paceap.com) for the cloud
signing trial, the dedicated CI account, and the Azure Digital Signing tutorial that
answered the hard question — all of which made it possible to prove the AAX pipeline
in public.

The compressor began as an assignment on the Digital Audio Effects course at Queen
Mary University of London, taught by Josh Reiss; its soft knee is the Giannoulis,
Massberg & Reiss design, implemented from the paper.

## License

**GNU AGPLv3** — see [LICENSE](LICENSE). This matches the terms under which the
suite uses [JUCE](https://juce.com)'s open-source license option. In short: use,
study, modify and redistribute freely; if you distribute builds (or serve users
over a network) from modified sources, those sources must be available under the
same terms. The DataCogs name and gears logo identify this project's official
builds and aren't covered by the code license.
