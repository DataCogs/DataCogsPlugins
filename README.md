# DataCogs Plugins

A suite of audio plugins (AU / VST3 / AAX / Standalone) built with [JUCE 8](https://juce.com) —
free, open source, and written to be read.

It's also a working reference for **modern JUCE infrastructure without the Projucer**:
the whole suite is pure CMake, wired for professional-grade CI and hands-free releases —
every merge builds all plugins and runs every test suite, and release packaging is fully
automated, including AAX signing via PACE's cloud signing tools (showcased here on a
trial licence). If you're setting up serious plugin release automation, this repo is
meant to be copied from.

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
is documented in [docs/RELEASING.md](docs/RELEASING.md). Agent/contributor
build rules live in [AGENTS.md](AGENTS.md).

### A note on AAX / Pro Tools

JUCE 8 bundles the AAX SDK, so the AAX targets build out of the box. Retail Pro Tools
only loads PACE-signed AAX, which requires an Avid/PACE developer setup; unsigned local
builds load in Pro Tools Developer builds only.

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

## License

To be finalised before first release. The suite is built on JUCE, whose open-source
terms (AGPLv3) constrain the choice; expect this repo to carry a GPL-family license.
