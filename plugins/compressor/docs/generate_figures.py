#!/usr/bin/env python3
"""Generate the README figures in docs/img/.

Each figure ports the exact recurrences from plugin/source/ (one-pole
envelope follower, static gain curve, sidechain HP, decoupled smoother),
so the plots show the shipped math, not an idealisation. Run from the
repo root with matplotlib installed:

    python3 docs/generate_figures.py [--png-dir DIR]
"""

import argparse
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# Chart chrome (light mode; the palette and inks follow the validated
# reference palette used across DataCogs docs).
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
BLUE = "#2a78d6"
ORANGE = "#eb6834"
AQUA = "#1baf7a"

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "font.size": 9.5,
    "axes.edgecolor": AXIS,
    "axes.labelcolor": INK2,
    "xtick.color": MUTED,
    "ytick.color": MUTED,
    "axes.titlecolor": INK,
    "axes.titlesize": 11,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.6,
    "axes.axisbelow": True,
    "legend.frameon": False,
    "legend.labelcolor": INK2,
    "svg.fonttype": "none",  # keep text as text, small files
})


def new_axes(width=7.0, height=3.8):
    fig, ax = plt.subplots(figsize=(width, height))
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    return fig, ax


def save(fig, name, out_dirs):
    fig.tight_layout()
    for out_dir, ext in out_dirs:
        fig.savefig(os.path.join(out_dir, f"{name}.{ext}"))
    plt.close(fig)


# --- the shipped DSP, ported line for line ----------------------------------

DIGITAL_TC = math.log(0.01)  # ln(eps), the "99% covered" convention
ANALOG_TC = -1.0


def compute_gain_db(level_db, threshold, ratio, knee, max_reduction_db=40.0):
    """Compressor::computeGainDb (Giannoulis et al. Eq. 4)."""
    slope = 1.0 / ratio - 1.0
    overshoot = level_db - threshold
    if 2.0 * overshoot <= -knee:
        return 0.0
    if 2.0 * overshoot >= knee:
        return max(slope * overshoot, -max_reduction_db)
    knee_pos = overshoot + knee * 0.5
    return max(slope * knee_pos * knee_pos / (2.0 * knee), -max_reduction_db)


def envelope(signal, fs, attack_ms, release_ms, mode="peak", tc=DIGITAL_TC):
    """EnvelopeDetector::processSample over a whole signal."""
    a_att = math.exp(tc / (attack_ms * fs * 0.001))
    a_rel = math.exp(tc / (release_ms * fs * 0.001))
    env = 0.0
    out = np.empty(len(signal))
    for n, x in enumerate(signal):
        rectified = x * x if mode == "rms" else abs(x)
        coeff = a_att if rectified > env else a_rel
        env = coeff * (env - rectified) + rectified
        out[n] = math.sqrt(env) if mode == "rms" else env
    return out


# --- figure 1: static gain curve --------------------------------------------

def fig_static_curve(out_dirs):
    fig, ax = new_axes()
    T, R = -18.0, 4.0
    level = np.linspace(-40, 0, 801)

    ax.plot(level, level, color=AXIS, lw=1.2, ls=(0, (4, 3)), label="1:1 reference")
    hard = [l + compute_gain_db(l, T, R, 0.0) for l in level]
    soft = [l + compute_gain_db(l, T, R, 10.0) for l in level]
    rng = [l + compute_gain_db(l, T, 20.0, 0.0, max_reduction_db=6.0) for l in level]
    ax.plot(level, soft, color=ORANGE, lw=2, label="soft knee (W = 10)")
    # plotted second so it sits on top where the two coincide outside the knee
    ax.plot(level, hard, color=BLUE, lw=2, label="hard knee (W = 0)")
    ax.plot(level, rng, color=AQUA, lw=2, label="range-capped (20:1, 6 dB)")

    ax.axvspan(T - 5, T + 5, color=ORANGE, alpha=0.07, lw=0)
    ax.axvline(T, color=AXIS, lw=0.8, ls=(0, (2, 2)))
    ax.annotate("threshold −18 dB", (T, -39), ha="center", va="bottom",
                color=MUTED, fontsize=8.5)
    ax.annotate("knee window\nT ± W/2", (T, -25.8), ha="center", color=MUTED,
                fontsize=8.5)

    ax.annotate("1:1", (-4.5, -3.2), color=MUTED, fontsize=8.5)
    ax.annotate("soft knee (W = 10) rounds the corner;\nidentical to hard outside the window",
                (-19.2, -19.4), (-36.5, -19.5), color=INK2, fontsize=8.5, va="center",
                arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.8))
    ax.annotate("4:1 compression line", (-5.5, -16.6), color=INK2, fontsize=8.5,
                ha="center", va="top")
    ax.annotate("range cap reached — max 6 dB of GR,\nslope returns to 1:1",
                (-8, -14), (-17.5, -8.2), color=INK2, fontsize=8.5, va="center",
                arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.8))

    ax.set_xlim(-40, 0)
    ax.set_ylim(-40, 0)
    ax.set_xlabel("input level (dBFS)")
    ax.set_ylabel("output level (dBFS)")
    ax.set_title("Static gain curve — what the [dsp] curve tests pin down")
    ax.legend(loc="upper left", fontsize=8.5)
    save(fig, "static-curve", out_dirs)


# --- figure 2: envelope step response ---------------------------------------

def fig_envelope_step(out_dirs):
    fig, ax = new_axes()
    fs, attack_ms = 96000.0, 10.0
    t_ms = np.arange(int(0.030 * fs)) / fs * 1000.0
    step = np.ones(len(t_ms))

    digital = envelope(step, fs, attack_ms, attack_ms, tc=DIGITAL_TC)
    analog = envelope(step, fs, attack_ms, attack_ms, tc=ANALOG_TC)
    bug = envelope(step, fs, attack_ms, attack_ms, tc=math.log10(0.01))

    ax.plot(t_ms, digital, color=BLUE, lw=2)
    ax.plot(t_ms, analog, color=AQUA, lw=2)
    ax.plot(t_ms, bug, color=ORANGE, lw=2, ls=(0, (4, 2)))

    ax.axvline(attack_ms, color=AXIS, lw=0.8, ls=(0, (2, 2)))
    ax.plot([attack_ms], [0.99], "o", color=BLUE, ms=6,
            markeredgecolor=SURFACE, markeredgewidth=1.5)
    ax.plot([attack_ms], [1 - 1 / math.e], "o", color=AQUA, ms=6,
            markeredgecolor=SURFACE, markeredgewidth=1.5)

    ax.annotate("99% at the labeled time (digital)", (11.2, 0.905),
                color=INK2, fontsize=8.5)
    ax.annotate("63.2% (analog, one RC τ)", (10.6, 0.60), color=INK2, fontsize=8.5)
    ax.annotate("historic base-10 bug:\nsame label, 2.3× slower", (17.5, 0.80),
                color=INK2, fontsize=8.5)
    ax.annotate("labeled attack\n(10 ms)", (attack_ms, 0.06), ha="center",
                color=MUTED, fontsize=8.5)

    ax.set_xlim(0, 30)
    ax.set_ylim(0, 1.05)
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("envelope (step response)")
    ax.set_title("Envelope step response — the a$^N$ = ε calibration the tests assert")
    ax.legend(["Digital (ε = 1%)", "Analog (ε = 1/e)", "base-10 bug (regression foil)"],
              loc="lower right", fontsize=8.5)
    save(fig, "envelope-step", out_dirs)


# --- figure 3: RMS vs Peak on a sine ----------------------------------------

def fig_rms_vs_peak(out_dirs):
    fig, ax = new_axes()
    fs, dur = 44100.0, 0.30
    t = np.arange(int(dur * fs)) / fs
    x = np.sin(2 * np.pi * 1000.0 * t)

    rms = envelope(x, fs, 50.0, 50.0, mode="rms")
    peak = envelope(x, fs, 50.0, 50.0, mode="peak")
    d = 20  # computed at full rate; the smooth envelopes plot fine decimated
    ax.plot(t[::d] * 1000, rms[::d], color=BLUE, lw=2)
    ax.plot(t[::d] * 1000, peak[::d], color=ORANGE, lw=2)

    for y, label in ((1 / math.sqrt(2), "A/√2 ≈ 0.707 (true RMS)"),
                     (2 / math.pi, "2A/π ≈ 0.637 (rectified mean)")):
        ax.axhline(y, color=AXIS, lw=0.8, ls=(0, (2, 2)))
        ax.annotate(label, (296, y + 0.012), ha="right", color=MUTED, fontsize=8.5)

    ax.annotate("RMS mode", (120, 0.73), color=INK2, fontsize=8.5)
    ax.annotate("Peak mode", (120, 0.605), color=INK2, fontsize=8.5)

    ax.set_xlim(0, 300)
    ax.set_ylim(0, 0.8)
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("detector output (linear)")
    ax.set_title("RMS vs Peak on a full-scale 1 kHz sine (equal 50 ms attack/release)")
    ax.legend(["RMS: sqrt of smoothed x²", "Peak: smoothed |x|"],
              loc="lower right", fontsize=8.5)
    save(fig, "rms-vs-peak", out_dirs)


# --- figure 4: sidechain HPF response ---------------------------------------

def fig_sidechain_hpf(out_dirs):
    fig, ax = new_axes()
    fs, fc = 44100.0, 120.0
    k = 1.0 - math.exp(-2.0 * math.pi * fc / fs)

    freqs = np.logspace(math.log10(20), math.log10(20000), 400)
    w = 2 * np.pi * freqs / fs
    z = np.exp(1j * w)
    lp = k / (1 - (1 - k) / z)          # one-pole lowpass stage
    hp = 1 - lp                          # hp = x - lp
    mag_db = 20 * np.log10(np.abs(hp) ** 2)  # two cascaded stages

    ax.plot(freqs, mag_db, color=BLUE, lw=2)

    # The bounds the [dsp] response test asserts at its three probe tones.
    for f, lo, hi in ((60.0, -17.0, -11.5), (120.0, -7.5, -5.0), (1000.0, -0.6, 0.1)):
        ax.plot([f, f], [lo, hi], color=INK2, lw=1.4)
        for y in (lo, hi):
            ax.plot([f * 0.93, f * 1.075], [y, y], color=INK2, lw=1.4)
    ax.annotate("asserted bounds at the\n60 / 120 / 1000 Hz probes",
                (63, -21.5), color=INK2, fontsize=8.5)

    ax.axvline(fc, color=AXIS, lw=0.8, ls=(0, (2, 2)))
    ax.annotate("cutoff 120 Hz", (fc * 1.06, -29), color=MUTED, fontsize=8.5)

    ax.set_xscale("log")
    ax.set_xlim(20, 20000)
    ax.set_ylim(-30, 2)
    ax.set_xticks([20, 60, 120, 250, 1000, 5000, 20000])
    ax.set_xticklabels(["20", "60", "120", "250", "1k", "5k", "20k"])
    ax.set_xlabel("frequency (Hz)")
    ax.set_ylabel("magnitude (dB)")
    ax.set_title("Sidechain HPF at 120 Hz — two cascaded one-poles (~12 dB/oct)")
    save(fig, "sidechain-hpf", out_dirs)


# --- figure 5: gain-reduction trajectory (the processBlock timing test) -----

def fig_gr_trajectory(out_dirs):
    fig, ax = new_axes()
    fs = 44100.0
    attack_ms, release_ms = 50.0, 200.0
    T, R = -18.0, 4.0

    step_at = int(0.5 * fs)
    x = np.full(int(0.75 * fs), 0.01)
    x[:step_at] = 1.0

    env = envelope(x, fs, attack_ms, release_ms, mode="peak")
    gr = np.array([-compute_gain_db(20 * math.log10(max(e, 1e-6)), T, R, 0.0)
                   for e in env])
    t_ms = np.arange(len(x)) / fs * 1000.0

    # Predicted crossings (the same closed forms the test derives) ±20%.
    slope = 1.0 - 1.0 / R
    gr_final = slope * 18.0
    e99 = 10 ** ((0.99 * gr_final / slope - 18.0) / 20.0)
    attack_cross = attack_ms * math.log(1 - e99) / math.log(0.01)
    e_rel = 10 ** ((0.05 / slope - 18.0) / 20.0)
    release_cross = release_ms * math.log((e_rel - 0.01) / 0.99) / math.log(0.01)

    ax.axvspan(0.8 * attack_cross, 1.2 * attack_cross, color=BLUE, alpha=0.10, lw=0)
    ax.axvspan(500 + 0.8 * release_cross, 500 + 1.2 * release_cross,
               color=BLUE, alpha=0.10, lw=0)
    ax.plot(t_ms, gr, color=BLUE, lw=2)
    ax.axhline(0.99 * gr_final, color=AXIS, lw=0.8, ls=(0, (2, 2)))

    ax.annotate("input: 0 dBFS step", (250, 12.6), ha="center", color=MUTED, fontsize=8.5)
    ax.annotate("step down to −40 dBFS", (628, 12.6), ha="center", color=MUTED, fontsize=8.5)
    ax.annotate("99% of final GR —\ncrossing must land in the band\n(predicted 0.84 × attack)",
                (105, 9.7), color=INK2, fontsize=8.5)
    ax.annotate("GR back below 0.05 dB\n(predicted 0.46 × release)",
                (632, 4.5), color=INK2, fontsize=8.5)

    ax.set_xlim(0, 750)
    ax.set_ylim(0, 14.5)
    ax.set_xlabel("time (ms)")
    ax.set_ylabel("gain reduction (dB)")
    ax.set_title("Ballistics through processBlock — DC-step gain trajectory, ±20% acceptance bands")
    save(fig, "gr-trajectory", out_dirs)


# --- figure 6: the golden-test signal, before and after --------------------

def fig_golden_waveform(out_dirs):
    """samples/sin_1000_vary.wav through the default (golden-test) settings.

    The input is a 1 kHz sine whose amplitude steps every 250 ms. At the
    default 125 ms attack/release (digital convention: 99% settled in
    125 ms) each segment holds one complete ballistic phase, so the
    output envelope shows the attack sag / release swell settling flat
    exactly halfway through each segment — the labeled time made visible.
    """
    import wave

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with wave.open(os.path.join(repo, "samples", "sin_1000_vary.wav")) as w:
        fs = float(w.getframerate())
        x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16) / 32768.0

    # Default parameters (the golden test's settings).
    T, R, W, atk_ms = -18.0, 4.0, 1.0, 125.0

    env = envelope(x, fs, atk_ms, atk_ms, mode="rms")
    gain = np.array([10 ** (compute_gain_db(20 * math.log10(max(e, 1e-6)), T, R, W) / 20)
                     for e in env])
    y = x * gain

    def waveform(ax, signal, color, bins=1000, t0=0.0, t1=None, ylim=0.85):
        t1 = len(signal) / fs if t1 is None else t1
        i0, i1 = int(t0 * fs), int(t1 * fs)
        edges = np.linspace(i0, i1, bins + 1, dtype=int)
        lo = [signal[a:b].min() for a, b in zip(edges[:-1], edges[1:])]
        hi = [signal[a:b].max() for a, b in zip(edges[:-1], edges[1:])]
        mid = (edges[:-1] + edges[1:]) / 2 / fs
        ax.fill_between(mid, lo, hi, color=color, lw=0)
        ax.set_ylim(-ylim, ylim)
        ax.set_xlim(t0, t1)

    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(7.0, 8.2),
                                        height_ratios=[1, 1, 1.5])
    for ax in (ax1, ax2, ax3):
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)

    waveform(ax1, x, MUTED)
    ax1.set_title("Input: 1 kHz sine, amplitude stepping every 250 ms", fontsize=10)
    ax1.set_ylabel("amplitude")

    # ±0.365 crops the initial attack-from-silence spike at t = 0 but gives
    # the per-segment ballistic shapes room to read.
    waveform(ax2, y, BLUE, ylim=0.365)
    ax2.set_title("Output at default settings (125 ms attack/release)", fontsize=10)
    ax2.set_ylabel("amplitude")

    waveform(ax3, y, BLUE, bins=900, t0=3.9, t1=4.8, ylim=0.40)
    for boundary in (4.0, 4.25, 4.5):
        ax3.axvline(boundary + 0.125, ymin=0.10, ymax=0.82,
                    color=AXIS, lw=0.9, ls=(0, (1.5, 2)))
    ax3.annotate("attack: overshoot on the step\nup, sag as GR catches up",
                 (4.02, 0.272), (3.925, 0.335), color=INK2, fontsize=8.5,
                 arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.8))
    ax3.annotate("dotted: +125 ms after each step —\nthe envelope has settled by the labeled time",
                 (4.79, 0.335), ha="right", color=INK2, fontsize=8.5)
    ax3.annotate("release: dip on the step down,\nswell as the gain recovers",
                 (4.55, -0.225), (4.79, -0.34), ha="right", color=INK2, fontsize=8.5,
                 arrowprops=dict(arrowstyle="-", color=MUTED, lw=0.8))
    ax3.set_title("Zoom 3.9–4.8 s: one attack and one release phase per 250 ms segment",
                  fontsize=10)
    ax3.set_xlabel("time (s)")
    ax3.set_ylabel("amplitude")

    save(fig, "golden-waveform", out_dirs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--png-dir", help="also write PNG copies here")
    args = parser.parse_args()

    img_dir = os.path.join(os.path.dirname(__file__), "img")
    os.makedirs(img_dir, exist_ok=True)
    out_dirs = [(img_dir, "svg")]
    if args.png_dir:
        os.makedirs(args.png_dir, exist_ok=True)
        out_dirs.append((args.png_dir, "png"))

    fig_static_curve(out_dirs)
    fig_envelope_step(out_dirs)
    fig_rms_vs_peak(out_dirs)
    fig_sidechain_hpf(out_dirs)
    fig_gr_trajectory(out_dirs)
    fig_golden_waveform(out_dirs)
    print(f"wrote 6 figures to {img_dir}")


if __name__ == "__main__":
    main()
