//
//  EnvelopeDetector.h
//  Compressor - Shared Code
//
//  Created by Mark Daunt on 28/1/18.
//
//  A one-pole envelope follower: the compressor's answer to the question
//  "how loud is the signal *right now*?"
//
//  Lineage: originally derived from Will Pirkle, "Designing Audio Effect
//  Plugins in C++" (and the AudioKit port of that code). This rewrite keeps
//  the topology but fixes two calibration bugs in that lineage:
//
//    1. The attack/release constants were base-10 logs (log10(0.01) = -2.0)
//       used inside expf(), which expects natural logs (ln(0.01) = -4.605).
//       Result: every time constant ran ln(10) = 2.3x slower than labeled.
//
//    2. "RMS" mode took the square root per-sample *before* smoothing,
//       i.e. sqrt(x^2) = |x|, which is exactly peak mode. A true RMS
//       detector is Root of the Mean of the Square: the averaging (Mean)
//       must happen between the Square and the Root.
//
// ============================================================================
//  THEORY: the one-pole smoother and where the coefficient formula comes from
// ============================================================================
//
//  The recurrence (one multiply, two adds per sample):
//
//      y[n] = a * y[n-1] + (1 - a) * x[n],      0 < a < 1
//
//  is the discrete RC circuit. To see how fast it moves, feed it a step
//  (x jumps 0 -> 1) and track the *remaining error* e[n] = 1 - y[n]:
//
//      e[n] = a * e[n-1]   =>   e[n] = a^n * e[0]
//
//  Pure geometric decay: the error shrinks by a factor of `a` every sample,
//  the discrete twin of the capacitor's e^(-t/tau). Since an exponential
//  never actually arrives, an "attack time" must be defined as "time to get
//  within some fraction eps of the target". Requiring
//
//      a^N = eps      where N = t_ms * fs * 0.001   (samples in t_ms)
//
//  and solving for the coefficient gives the formula used below:
//
//      a = eps^(1/N) = exp( ln(eps) / (t_ms * fs * 0.001) )
//
//  The fraction eps is pure convention, and the two industry conventions are
//  provided as TimeConstant::Digital and TimeConstant::Analog (see the
//  constants at the bottom of the class).
//
class EnvelopeDetector
{
public:
    // ------------------------------------------------------------------
    // Rectifier choice: what statistic of the waveform do we track?
    //
    //  Peak: smooth |x|. Follows instantaneous excursions. The right choice
    //        for limiting / overload protection, and for a deliberately
    //        "grabby" character; fidgety on dense program material because
    //        peaks are much spikier than perceived loudness.
    //
    //  RMS:  smooth x^2, then sqrt at the output. Tracks the signal's
    //        energy, which correlates far better with perceived loudness
    //        (this is the dbx 160 family character). Smooth and forgiving
    //        on vocals, bass, and full mixes.
    //
    //        Caveat worth knowing: the smoothed x^2 of a tone ripples at
    //        twice the tone's frequency, and with attack faster than
    //        release the branching smoother rides the *crests* of that
    //        ripple, so the estimate sits a few dB above the true RMS
    //        (measured: ~1.6 dB high on a sine with 5 ms / 50 ms). Only
    //        equal attack and release make it converge to the textbook
    //        mean - every classic RMS compressor shares this bias, and it
    //        just reads as a slightly eager threshold.
    // ------------------------------------------------------------------
    enum class Mode
    {
        Peak,
        RMS
    };

    // Which finish line defines "the attack time has elapsed"?
    // Same exponential curve either way - only the definition of "done"
    // changes, so Analog and Digital differ in speed by a fixed factor
    // ln(0.01)/ln(1/e) = 4.6.
    enum class TimeConstant
    {
        Digital, // envelope has covered 99%   of the step (error eps = 1%)
        Analog   // envelope has covered 63.2% of the step (error eps = 1/e),
                 // i.e. one RC time constant - how analog gear is specced
    };

    EnvelopeDetector() = default;

    // Set the sample rate and reset state. Call from prepareToPlay().
    // Re-derives the coefficients, since `a` bakes the sample rate in.
    void prepare (float sampleRateIn);

    // Zero the envelope (e.g. on transport start) without touching settings.
    void reset();

    void setAttackTime (float attackMsIn);   // ms, per the active convention
    void setReleaseTime (float releaseMsIn); // ms, per the active convention
    void setMode (Mode modeIn)                       { mode = modeIn; }
    void setTimeConstant (TimeConstant conventionIn); // recomputes coefficients

    float getAttackTime()  const { return attackMs; }
    float getReleaseTime() const { return releaseMs; }

    // Process one sample; returns the current envelope as a LINEAR AMPLITUDE
    // (same units as the input, regardless of mode - RMS mode square-roots
    // internally). Callers wanting dB take 20*log10 of this. Keeping the
    // output in one consistent domain is what makes the downstream gain
    // computer's threshold knob mean what it says.
    float processSample (float x);

    // ln(eps) for the two conventions. NOTE: natural logs! The historic
    // Pirkle/AudioKit values (-2.0 and -0.4353) are the base-10 logs of the
    // same fractions, which made everything ln(10) = 2.3x slower than spec.
    // Public so other smoothers (DecoupledSmoother) share one definition.
    static constexpr float kDigitalTC = -4.60517019f; // ln(0.01)
    static constexpr float kAnalogTC  = -1.0f;        // ln(1/e)

private:
    // Recompute `a` from the stored ms values (see derivation above).
    void updateCoefficients();

    float sampleRate   { 44100.0f };
    float attackMs     { 10.0f };  // the user-facing times, kept separate
    float releaseMs    { 100.0f }; // from the derived coefficients so they
    float attackCoeff  { 0.0f };   // can always be re-derived (the original
    float releaseCoeff { 0.0f };   // code overwrote the ms with the coeff)
    float envelope     { 0.0f };   // y[n-1]; power domain (x^2) in RMS mode

    Mode mode                 { Mode::RMS };
    TimeConstant timeConstant { TimeConstant::Digital };
};

//==============================================================================
// Sidechain high-pass filter: two cascaded one-pole high-passes
// (~12 dB/oct) applied to the DETECTION path only - the audio itself is
// never filtered.
//
// Why it exists: low frequencies carry most of a mix's energy, so without
// this the kick and bass drive the detector and every kick hit ducks the
// entire mix - the classic "pumping" complaint. Rolling lows off the
// sidechain makes the compressor respond to the mids/highs where the ear
// judges loudness, while bass still gets compressed (it receives the same
// gain as everything else - it just stops *deciding* the gain). This is
// the "sidechain HPF" switch on SSL-style bus compressors.
//
// Each stage is a one-pole lowpass subtracted from its input:
//
//     lp += k * (x - lp);   hp = x - lp;      k = 1 - e^(-2*pi*fc/fs)
//
// (k comes from the same impulse-invariant RC derivation as the envelope
// coefficients - see the EnvelopeDetector header - with the "time
// constant" expressed as a corner frequency, tau = 1/(2*pi*fc).)
// Cascading two stages steepens 6 dB/oct to ~12 dB/oct, enough to stop a
// 60 Hz kick fundamental from dominating a 120 Hz-cutoff sidechain.
class SidechainFilter
{
public:
    void prepare (float sampleRateIn);
    void reset();

    // Corner frequency in Hz; 0 (or negative) bypasses the filter exactly,
    // so the default preserves bit-identical behavior with no filter.
    void setCutoff (float cutoffHzIn);

    float processSample (float x);

private:
    float sampleRate { 44100.0f };
    float cutoffHz   { 0.0f };
    float k          { 0.0f };
    float lp1 { 0.0f }, lp2 { 0.0f }; // the two lowpass states
};

//==============================================================================
// Smooth DECOUPLED one-pole smoother - Giannoulis/Massberg/Reiss Eq. 17,
// their recommended ballistics for smoothing the gain-reduction signal in
// the log-domain topology (see Compressor.h).
//
// Two cascaded stages instead of one branching filter:
//
//   y1[n] = max( x[n],  aR * y1[n-1] + (1 - aR) * x[n] )   release stage
//   yL[n] = aA * yL[n-1] + (1 - aA) * y1[n]                attack stage
//
// The release stage rides the input's peaks and can only fall at the
// release rate; the attack stage then lowpasses that trajectory. Because
// no coefficient ever switches mid-stream, the output's SLOPE is
// continuous at attack/release transitions - the branching detector's
// coefficient swap puts a corner in the gain curve at every transition,
// which is audible as extra harmonic distortion (the paper's Fig. 9 THD
// comparison shows decoupled beating branching, smoothed or not).
//
// The trade documented in the paper (Fig. 5): the two stages interact, so
// the measured release time comes out near tauA + tauR rather than tauR
// exactly. Buying artifact-free transitions with slightly approximate
// time labels is the right trade for a "musical" setting.
//
// Intended input is a NON-NEGATIVE control signal (gain reduction in dB),
// per the paper's log-domain placement - not the audio itself.
class DecoupledSmoother
{
public:
    void prepare (float sampleRateIn);
    void reset();
    void setAttackTime (float attackMsIn);
    void setReleaseTime (float releaseMsIn);

    float processSample (float x);

private:
    void updateCoefficients();

    float sampleRate   { 44100.0f };
    float attackMs     { 10.0f };
    float releaseMs    { 100.0f };
    float attackCoeff  { 0.0f };
    float releaseCoeff { 0.0f };
    float releaseState { 0.0f }; // y1: peak-riding release stage
    float smoothState  { 0.0f }; // yL: final smoothed output
};
