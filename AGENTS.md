# Agent / contributor guide

JUCE 8 audio plugin suite (AU / VST3 / AAX / Standalone), pure CMake, C++20.
Three plugins — Compressor, Parametric EQ, Convolution Reverb — sharing one
house UI. Each plugin's README documents its DSP design; read it before
touching that plugin's DSP.

## Layout

- `common/` — shared UI: `DataCogsLookAndFeel` (palette, cog knobs, stamped
  labels, recessed value plates, machined combos) exposed as the `DataCogsUI`
  INTERFACE library, plus the gears logo as the `DataCogsAssets` binary-data
  target (`#include "DataCogsAssets.h"`).
- `common/cmake/DataCogsAAX.cmake` — shared PACE AAX signing recipe.
- `plugins/{compressor,parametric-eq,convolution-reverb}/` — one directory per
  plugin: `plugin/` (sources) and `test/` (Catch2). The compressor is the
  reference implementation of the house style (parameter icon components with
  tooltips, logo header).
- `packaging/` — macOS suite installer builder (see docs/RELEASING.md).
- `libs/` — CPM-fetched dependencies (JUCE, Catch2), created at configure
  time; gitignored, never edit.

## Build & test

```sh
cmake -S . -B build                    # first run downloads JUCE + Catch2
cmake --build build -j8                # whole suite
ctest --test-dir build                 # all tests - keep this green
```

- One plugin only: `-DBUILD_PARAMETRIC_EQ=OFF` etc., or single targets like
  `cmake --build build --target CompressorPlugin_AU`.
- `COPY_PLUGIN_AFTER_BUILD` defaults ON (installs into the system plugin
  folders; needs local permissions). CI and packaging pass `=OFF`.

## Hard rules

- Parameter IDs, ranges, skews and step intervals are deliberate DSP/UX
  decisions - never change them unless the change is the explicit task.
- Plugin codes (`DCog` + `MBC1`/`DCeq`/`Cvrb`) must never change: DAW
  sessions reference them. DAW-visible names are the `PRODUCT_NAME`s.
- DSP classes stay plain C++ (JUCE-independent where possible) and are what
  the Catch2 tests pin down. If you change DSP behaviour, update the inline
  theory notes and the tests together.
- UI work goes through `common/` so the suite stays coherent; per-plugin UI
  (parameter icons, meters, layout) stays in the plugin.
- After renaming any `PRODUCT_NAME`, delete stale old-name bundles from the
  system plugin folders, or DAWs will list duplicates.

## Style

- JUCE conventions: space before `(`, `juce::` spelled out, comments explain
  constraints the code can't show.
- Icons: stroke pictograms on a 24x24 grid in `DataCogsLookAndFeel::textDim`,
  one filled element per icon; tooltips carry the plain-English manual.
