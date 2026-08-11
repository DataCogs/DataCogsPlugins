//
//  Compressor.h
//  Compressor - Shared Code
//
//  Created by Mark Daunt on 18/2/18.
//
//  Feed-forward dynamic range compressor. Every compressor is two machines
//  glued together:
//
//    1. The DETECTOR (sidechain): turns the fast-moving waveform into a
//       slow-moving level estimate. Lives in EnvelopeDetector.
//
//    2. The GAIN COMPUTER: a static level-in -> gain-out curve defined by
//       threshold, ratio and knee, working entirely in dB. Lives here
//       (computeGainDb).
//
//  The glue between them is a unit convention: the gain computer expects a
//  genuine signal level in dB, so the detector output (linear amplitude) is
//  converted with 20*log10 in exactly one place (amplitudeToDb below).
//
//  Topology follows Will Pirkle, "Designing Audio Effect Plugins in C++";
//  the soft knee follows Giannoulis, Massberg & Reiss, "Digital Dynamic
//  Range Compressor Design - A Tutorial and Analysis", JAES 60(6), 2012
//  (the standard reference for compressor design - worth reading in full).
//

#pragma once

#include "EnvelopeDetector.h"
#include <cmath>
#include <vector>

class Compressor
{
public:
    // ------------------------------------------------------------------
    // Where the ballistics live - the deepest fork in compressor design
    // (Giannoulis et al. Fig. 7):
    //
    //  Classic:   smooth the LEVEL, then compute gain from it (Fig. 7a).
    //             Per-channel envelope followers in the linear domain feed
    //             the static curve. The traditional Pirkle/analog-RMS
    //             arrangement: time constants describe how fast the level
    //             estimate moves. Costs: a small attack lag (the detector
    //             must charge up before the gain computer sees anything),
    //             and releases that sound heavier the deeper the
    //             compression, because an exponential in the linear domain
    //             is not exponential in dB.
    //
    //  LogDomain: compute gain INSTANTLY, then smooth the GAIN (Fig. 7c,
    //             Eq. 23) - the paper's recommended design. Instantaneous
    //             linked peak -> dB -> static curve -> smooth the
    //             gain-reduction signal with the decoupled smoother
    //             (Eq. 17). No attack lag, the envelope returns to zero on
    //             its own (no threshold dependence in the release), and
    //             trajectories are exponential in dB - which matches
    //             roughly-logarithmic hearing, so releases sound even at
    //             any depth. Their Table 1 envelope-fidelity scores favor
    //             this on every source except (marginally) vocals.
    //
    //  With LogDomain, the Detector mode (peak/RMS) is not used: level
    //  sensing is instantaneous by design and all smoothing happens after
    //  the gain computer.
    // ------------------------------------------------------------------
    enum class Topology
    {
        Classic,
        LogDomain
    };

    explicit Compressor (int numChannelsIn);

    // Call from prepareToPlay: sets sample rate on all detectors (which
    // re-derives their coefficients) and resets envelopes.
    void prepare (float sampleRate);

    void setAttack (float attackMs);   // ms, forwarded to detectors + gain smoother
    void setRelease (float releaseMs); // ms, forwarded to detectors + gain smoother
    float getAttack() const;
    float getRelease() const;
    int getNumChannels() const { return (int) detectors.size(); }

    // Switching topology or detector mode resets the ballistics state:
    // the state variables mean different things on each side of the switch
    // (linear level vs power vs gain-reduction dB), so carrying a value
    // across would give one garbage transient instead of a clean restart.
    void setTopology (Topology topologyIn);
    void setDetectorMode (EnvelopeDetector::Mode modeIn);

    // Sidechain high-pass corner in Hz (0 = off). Detection path only -
    // see SidechainFilter for why this kills bass pumping.
    void setSidechainCutoff (float cutoffHz);

    // ------------------------------------------------------------------
    // The static gain curve (the "gain computer"). Input: sidechain level
    // in dB. Output: gain to apply, in dB (always <= 0).
    //
    // Hard-knee compression is a piecewise-linear curve in dB space:
    //
    //     gain = 0                              below threshold
    //     gain = (1/R - 1) * (level - T)        above threshold
    //
    // where (1/R - 1) is the gain slope: 0 at ratio 1:1, approaching -1
    // (limiting) as R -> infinity. The corner at T is audible on material
    // that hovers around threshold - the compressor audibly "switches on".
    //
    // The soft knee replaces the corner with a quadratic (parabolic) blend
    // over the window [T - W/2, T + W/2] (Giannoulis et al., Eq. 4):
    //
    //     gain = (1/R - 1) * (level - T + W/2)^2 / (2W)      inside the knee
    //
    // Why this exact curve: it is the unique parabola that matches the two
    // straight segments in BOTH value and first derivative at BOTH knee
    // edges (gain 0 with slope 0 at the bottom; the full compression line
    // with slope (1/R - 1) at the top). Check by differentiating:
    //
    //     d(gain)/d(level) = (1/R - 1) * (level - T + W/2) / W
    //
    // which sweeps linearly from 0 to (1/R - 1) across the knee - the
    // effective ratio glides smoothly from 1:1 to R rather than snapping.
    // (The previous implementation interpolated the *slope* linearly and
    // clamped one knee edge to 0 dBFS, which skewed the knee off-centre
    // whenever threshold > -W/2.)
    // ------------------------------------------------------------------
    float computeGainDb (float levelDb) const;

    // ------------------------------------------------------------------
    // Per-sample entry point: run every channel's detector, STEREO-LINK by
    // taking the loudest envelope, and return one linear gain to apply to
    // all channels. Returns gain in [0, 1].
    //
    // Why link: if each channel is compressed independently, a transient on
    // one side ducks only that side and the stereo image lurches toward the
    // other. Linking trades a little per-channel optimality for a stable
    // image - the right default for anything but deliberate dual-mono use.
    // Taking the max (rather than the average) means the hotter channel
    // fully drives the compression, the more conservative choice.
    // ------------------------------------------------------------------
    // numChannels guards against the host handing us fewer channels than we
    // prepared for; only the first min(numChannels, prepared) are read.
    float processSampleLinked (const float* const* channelData, int numChannels, int sampleIndex);

    // dB <-> linear, kept next to each other so the 20-vs-10 question has
    // one authoritative answer: these operate on AMPLITUDES, so the factor
    // is 20 (dB is defined on power ratios, power ~ amplitude^2, and
    // 10*log10(a^2) = 20*log10(a)). Anything already squared must use 10 -
    // or better, stay out of the dB conversion business entirely, which is
    // why EnvelopeDetector returns amplitudes for every mode.
    static float amplitudeToDb (float amplitude)
    {
        // Floor at -120 dB: below any musical signal, avoids log(0) = -inf,
        // and gives the gain computer a finite "silence" to relax from.
        constexpr float minAmplitude = 1.0e-6f; // 20*log10(1e-6) = -120 dB
        return 20.0f * std::log10 (amplitude > minAmplitude ? amplitude : minAmplitude);
    }

    static float dbToAmplitude (float db)
    {
        return std::pow (10.0f, db * 0.05f); // 10^(dB/20)
    }

    // Parameters, all in dB except ratio. Brace-initialised to the plugin
    // defaults - the original left these (and the analog/digital time
    // constant flag) uninitialised, so the compression character depended
    // on whatever garbage happened to be in memory. Never leave a DSP
    // parameter to chance.
    float threshold  { -18.0f }; // dB: level where compression begins
    float ratio      { 4.0f };   // R:1 above the knee
    float kneeWidth  { 1.0f };   // dB spanned by the quadratic blend
    float makeupGain { 0.0f };   // dB, applied by the caller after compression

    // Ceiling on gain reduction ("range"). With aggressive threshold/ratio
    // settings this stops the compressor ever slamming past a chosen depth
    // - the trick behind transparent vocal leveling: set the curve hot for
    // consistency, cap the damage. 40 dB is effectively "no limit".
    // Applied inside computeGainDb, so in the log-domain topology the cap
    // is smoothed like everything else (no corner in the gain signal).
    float maxReductionDb { 40.0f };

private:
    void resetBallistics();

    std::vector<EnvelopeDetector> detectors;   // Classic: one per channel, linked at detect time
    std::vector<SidechainFilter> scFilters;    // per channel, ahead of detection (both topologies)
    DecoupledSmoother gainSmoother;            // LogDomain: smooths gain reduction in dB
    Topology topology { Topology::Classic };
    EnvelopeDetector::Mode detectorMode { EnvelopeDetector::Mode::RMS };
    float attackMs  { 10.0f };
    float releaseMs { 100.0f };
};
