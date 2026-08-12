# DataCogs Plugins

A suite of audio plugins (AU / VST3 / AAX / Standalone) built with [JUCE 8](https://juce.com) —
free, open source, and written to be read.

It's also a working reference for **modern JUCE infrastructure without the Projucer**:
the whole suite is pure CMake, wired for professional-grade CI and hands-free releases —
every merge builds all plugins and runs every test suite, and release packaging is fully
automated, including AAX signing via PACE's cloud signing tools (showcased here on a
trial licence). It grew out of real production pain: converting a plugin company's
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
- **It's open to everyone.** Use the plugins in your sessions, read the code to learn how
  compressors and EQs actually work, fork it, or contribute back. The point is that good
  audio tools — and the knowledge inside them — shouldn't be gated.
- **Transparency is a security property.** Audio plugins are native code you invite onto
  your machine, and closed-source plugins ask you to trust an opaque binary produced by
  an opaque process. Here you can audit not just the DSP but the exact pipeline that
  compiled, signed and packaged the installer you download — the CI logs of every
  release build are public.

## The plugins

| Plugin | What it is | Docs |
|---|---|---|
| **DataCogs Compressor** | Dynamic range compressor: Classic and Log-Domain topologies, Peak/RMS detection, sidechain HPF, soft knee, parallel mix | [README](plugins/compressor/README.MD) |
| **DataCogs Parametric EQ** | Parametric equaliser | [README](plugins/parametric-eq/README.md) |
| **DataCogs Convolution Reverb** | Convolution reverb | [README](plugins/convolution-reverb/README.md) |

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

The release pipeline — CI, PACE cloud AAX signing, notarized suite installer —
is documented in [docs/RELEASING.md](docs/RELEASING.md), with a from-scratch
setup walkthrough (certificates, secrets, service accounts) in
[docs/CI-SETUP.md](docs/CI-SETUP.md). Agent/contributor build rules live in
[AGENTS.md](AGENTS.md).

### A note on AAX / Pro Tools

JUCE 8 bundles the AAX SDK, so the AAX targets build out of the box. Retail Pro Tools
only loads PACE-signed AAX, which requires an Avid/PACE developer setup; unsigned local
builds load in Pro Tools Developer builds only.

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
  Windows — hence a macOS packaging job (a Windows one lands with the Windows
  installer).
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
- **Retry cheaply.** After fixing *secrets*, use the run's "Re-run failed
  jobs" — it reuses the built artifacts and current secret values. Script or
  workflow changes need a fresh tag: the tagged commit is what CI executes.

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

Thanks to Derek at [PACE](https://paceap.com) for supplying the 30-day cloud
signing trial that made it possible to prove the AAX signing pipeline in
public.

## License

**GNU AGPLv3** — see [LICENSE](LICENSE). This matches the terms under which the
suite uses [JUCE](https://juce.com)'s open-source license option. In short: use,
study, modify and redistribute freely; if you distribute builds (or serve users
over a network) from modified sources, those sources must be available under the
same terms. The DataCogs name and gears logo identify this project's official
builds and aren't covered by the code license.
