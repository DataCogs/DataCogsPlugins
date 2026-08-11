# Parametric EQ

A DataCogs audio plugin (AU / VST3 / Standalone) built with JUCE 8. Six-band
parametric EQ with a post-EQ spectrum analyzer and a draggable response
curve — the third leg of the mixing tripod alongside the DataCogs
Compressor and Convolution Reverb.

## Bands

Six bands, each one RBJ-cookbook biquad (`plugin/include/BiquadDesign.h`
has the derivations), each switchable between **Bell / Low Shelf / High
Shelf / High-Pass / Low-Pass / Notch** — so the classic channel chain
(HP → low shelf → two bells → high shelf → LP) fits with a band to spare.

Defaults are the channel-strip layout and are exactly transparent: bands
2–5 active at 0 dB (a 0 dB RBJ bell/shelf is mathematically unity), HP/LP
bookends present but inactive.

| Band param | Range | Notes |
|---|---|---|
| Freq | 20 Hz–20 kHz | log knob travel, matches the display |
| Gain | ±24 dB | bell/shelves only; ignored by HP/LP/notch |
| Q | 0.1–18 | 0.707 default = Butterworth for HP/LP, "no surprises" everywhere |
| Type / Active | — | per band |

Plus **Output** trim (±24 dB, ramped), host-visible **Bypass**, and
**Reset** (all parameters to defaults).

## The display

- **Drag a node** — frequency and gain (gain only for bell/shelf types).
- **Mouse wheel on a node** — Q, multiplicative so it feels even across
  the log range.
- **Double-click a node** — toggle the band on/off.
- **Click empty space** — select the nearest band for the strip below.

The spectrum behind the curve is **post-EQ** (you see what you hear),
4096-point FFT at 30 Hz with meter-style ballistics (rise fast, decay
slow), handed from the audio thread through a single atomic flag — frames
drop rather than block when the UI lags.

## Architecture notes

- Coefficients are recomputed **once per block** from the parameter
  atomics — six designs of trig is noise next to the filtering, and it
  makes automation accurate with zero listener machinery.
- Filter state is double-precision TDF2 (low-frequency high-Q bands stay
  clean); state resets only on type/active changes, never on knob moves.
- The DSP core (`BiquadDesign`) is JUCE-free and pinned by tests against
  cookbook theory via exact complex-plane magnitude evaluation, including
  the boost/cut symmetry identity (+12 dB then −12 dB multiplies to exact
  unity).

## Build & test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8          # also copies AU/VST3 into ~/Library/Audio/Plug-Ins
cd build && ctest --output-on-failure
auval -v aufx DCeq DCog          # AU end-to-end validation
```

First configure downloads JUCE 8.0.14 and Catch2 into `libs/` via CPM.

Ableton note: after replacing binaries, fully quit Live and Option-click
Rescan; wrench icon opens the custom editor.

## References

- Bristow-Johnson, "Cookbook formulae for audio EQ biquad filter
  coefficients" (the Audio EQ Cookbook).
- Zölzer, *DAFX: Digital Audio Effects*, ch. 2 (filter design).
- JUCE spectrum-analyser tutorial (FFT/FIFO display pattern).
