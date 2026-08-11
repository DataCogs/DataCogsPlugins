# Convolution Reverb

A DataCogs audio plugin (AU / VST3 / Standalone) built with JUCE 8. Convolves
the input with a measured or synthetic room impulse response using
`juce::dsp::Convolution` (non-uniform partitioned FFT, zero latency,
background IR loading and resampling).

## Signal flow

```
input ─┬─ dry ─────────────────────────────────────────────────────────┐
       └─ [IR: early/tail gains, decay reshape, size, reverse]         │
          convolution → pre-delay → low cut → high cut → width ──┴─ equal-power mix → output gain
```

Controls come in two kinds:

- **IR-domain** (Size, Decay, Early, Tail, Reverse): these transform the
  impulse response itself and reload the engine. Always applied to an
  untouched master copy of the loaded IR (they never compound), rebuilt on
  the message thread behind a 120 ms debounce, and crossfaded in by the
  engine — safe to turn during playback.
- **Runtime** (everything else): processed sample-by-sample on the wet path,
  smoothed, no reload.

## Parameters

| Control | Range | What it does |
|---|---|---|
| Mix | 0–100% | Equal-power dry/wet crossfade. Dry and wet are decorrelated so powers add; the sin/cos law keeps loudness constant where a linear fade would dip ~3 dB at 50%. |
| Pre-Delay | 0–250 ms | Delays the whole wet path, separating the direct sound from the reverb onset. |
| Size | 50–200% | Stretches the IR by relabelling its sample rate, so the engine's resampler scales length *and* modal frequencies together — what "a bigger room" means physically. |
| Decay | 25–200% | Rescales the room's RT60. The current RT60 is measured by Schroeder backward integration (T20 fit) and a corrective exponential envelope applied — the modal character is untouched, only the energy slope changes. Above 100% the capture's noise floor comes up too (clamped at +40 dB); that's physics, not a bug. |
| Width | 0–200% | Mid/side scaling of the wet path only. 0% collapses the reverb to mono without touching the dry image; 200% doubles the side energy. |
| Early | −60…+12 dB | Level of the IR's early-reflection region: the first ~80 ms after the detected direct-sound onset. −60 dB = off (exact silence). |
| Tail | −60…+12 dB | Level of everything after the early region. The two regions are joined by a 10 ms raised-cosine crossfade so no discontinuity is spliced into the IR. |
| Low Cut | 20–1000 Hz | First-order highpass on the wet path. Hard-bypassed at 20 Hz (a filter "parked at the edge" is not transparent, so the end of travel skips it entirely). |
| High Cut | 1–20 kHz | First-order lowpass ("damping") on the wet path. Hard-bypassed at 20 kHz. |
| Output | −24…+12 dB | Wet+dry output trim, ramped. |
| Reverse | toggle | Mirrors the shaped IR in time — the room swells *into* the sound. |
| Bypass | toggle | True passthrough, also exposed to the host via `getBypassParameter()`; the wet chain is reset on the transition so no stale tail bursts out on re-engage. |

## Use cases

**Vocal in a mix.** Start ~20% Mix, 20–40 ms Pre-Delay (keeps consonants dry
and up front while the word blooms behind), Low Cut ~150 Hz so the reverb
doesn't muddy the low mids, High Cut ~8 kHz to keep sibilance out of the
tail. A church/hall IR at Decay 75% often beats hunting for a shorter room.

**Drums.** EchoThief's stairwells/underpasses at 10–15% Mix add "real place"
glue. Pull Tail down (−6 to −12 dB) to keep the room's attack but lose the
wash; or Early −60 dB + Tail 0 dB for pure bloom that ducks behind
transients. Width 130–150% widens the kit without touching the close mics.

**Push a source back on the "stage".** Lower Early a few dB and raise Mix
slightly — more diffuse energy relative to direct reads as distance. The
opposite (Early up, Tail down) pulls it forward and dries the sustain.

**Make one room into many.** A single good hall IR + Size is a room
collection: Size 70% ≈ chamber, 100% = as captured, 150–200% ≈ cathedral of
the same character. Size shifts the modal frequencies as a real resize
would; Decay then trims the tail length independently.

**Sound design.** Reverse + long Pre-Delay = classic pre-verb swells.
Hamilton Mausoleum (15 s tail) at Size 200% / Decay 200% is an instant
ambient pad-maker; automate High Cut downward for a "closing door" effect.

**Mono-compatibility checks.** Width 0% makes the reverb itself mono — if
the mix still sounds spacious, the spaciousness was coming from somewhere
you didn't think it was.

## The IR library

A built-in synthetic hall (Moorer-style exponentially decaying Gaussian
noise with progressive HF damping — see `plugin/include/IRGenerator.h` for
the derivations) loads at construction, so the plugin makes sound out of
the box with no files installed at all.

### Where IRs live

The picker scans this folder recursively at editor-open:

- **macOS:** `~/Library/Audio/Impulse Responses/DataCogs/`
- **elsewhere:** `~/Documents/DataCogs/Impulse Responses/`

Nested folders become the dropdown's display names (`EchoThief/Venues/…`).
*Load IR…* opens anything from any location without installing it.

### Adding a new IR

1. Drop a `.wav` / `.aif` / `.aiff` / `.flac` file anywhere under the
   library folder (subfolders encouraged — they organise the dropdown).
   Any sample rate and bit depth; mono, stereo, or multichannel (channels
   beyond the first two are ignored for now); up to 30 s.
2. Reopen the plugin editor (the list is rescanned when the editor opens).

Loudness is normalised on load, silence is trimmed, and the file is
resampled to the session rate automatically — a phone recording of a
stairwell clap works, though a proper sine-sweep deconvolution sounds
better. The full path of the chosen IR is saved in the session; if the
file is missing on recall (moved project, other machine), the built-in
hall is substituted so the session still makes sound.

### Adding a photo for the UI panel

Put an image next to the IR with the **same basename**:

```
Cathedral.wav
Cathedral.jpg        ← shown in the editor's photo panel
```

`.jpg`, `.jpeg` and `.png` are checked in that order. No image → a quiet
"no photo" placeholder. This is how the six OpenAIR photos work, and it's
the convention to use for your own captures: photograph the space when you
sample it.

### Favourites

The ★ button next to the picker bookmarks the current library IR;
favourites get their own section at the top of the dropdown. They're
stored as one relative path per line in `favourites.txt` inside the
library folder — human-editable, shared by every plugin instance and
format, and it travels with the IR collection if you sync it to another
machine. (IRs loaded from outside the library can't be favourited — there's
no stable path to remember them by.)

### Presets

Seven factory presets (Vocal Hall, Drum Room, Cathedral, Huge Ambience,
Reverse Swell, …) are exposed through the JUCE program API — hosts list
them natively — and through the preset box in the editor. Presets that
reference an OpenAIR IR fall back to the built-in hall if the pack isn't
installed. For user presets, save via your DAW (the plugin state is
complete: every knob plus the IR path), e.g. Live's "Save as Default" or
a rack preset.

### Installed packs

~160 IRs were installed locally on 2026-07-05 (not part of this repo):
OpenAIR (University of York, CC BY 4.0 — six famous spaces incl. Hamilton
Mausoleum and York Minster, with photos), EchoThief (~115 real-world
spaces), and Voxengo IMreverbs (~38 classics). See `ATTRIBUTION.md` in the
library folder — fine to *use* freely, but don't redistribute or bundle
into an installer without re-checking each pack's license.

### Backup / installing on another machine

The whole library (~66 MB: IRs, photos, favourites, attribution) lives in a
**private** GCS bucket, `gs://datacogs-ir-library` (project `datacogs-dev`,
australia-southeast1) — private because the packs' licenses permit personal
use, not public redistribution.

```sh
./scripts/install-irs.sh          # new machine: pull the library down
./scripts/sync-irs.sh             # push local additions/favourites up
./scripts/sync-irs.sh --delete    # mirror exactly (bucket-side deletes)
```

Both are non-destructive by default (rsync semantics, nothing deleted).
Needs `gcloud auth login` + `gcloud config set project datacogs-dev` first.

## Build & test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8          # also copies AU/VST3 into ~/Library/Audio/Plug-Ins
cd build && ctest --output-on-failure
auval -v aufx Cvrb DCog          # AU end-to-end validation
```

First configure downloads JUCE 8.0.14 and Catch2 into `libs/` via CPM.

Ableton note: after replacing binaries, fully quit Live and Option-click
Rescan; use the wrench icon to open the custom editor, the unfold arrow for
Live's generic sliders.

## References

- Moorer, "About This Reverberation Business", Computer Music Journal 1979.
- Schroeder, "New Method of Measuring Reverberation Time", JASA 1965.
- Gardner, "Efficient Convolution without Input-Output Delay", JAES 1995.
- Wefers, "Partitioned convolution algorithms for real-time auralization", 2015.
- Välimäki et al., "Fifty Years of Artificial Reverberation", IEEE TASLP 2012.
