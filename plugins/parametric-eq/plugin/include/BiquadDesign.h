#pragma once

/**
 * Biquad filter design and evaluation - plain C++, no JUCE dependency, so it
 * compiles standalone for calibration checks against theory and unit-tests
 * without the framework (same policy as the Compressor's DSP core).
 *
 * All formulas are from Robert Bristow-Johnson's "Audio EQ Cookbook", the
 * de-facto standard for musical parametric EQ. The common intermediates:
 *
 *     w0    = 2 pi f0 / fs        (centre frequency in radians/sample)
 *     alpha = sin(w0) / (2 Q)     (bandwidth term)
 *     A     = 10^(gainDb / 40)    (amplitude - note /40, not /20: the gain
 *                                  is split evenly between poles and zeros,
 *                                  which is what makes RBJ bells symmetric
 *                                  in boost and cut)
 *
 * Every design normalises by a0, so downstream code only stores/uses five
 * coefficients (b0 b1 b2, a1 a2) and the transfer function is
 *
 *     H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)
 *
 * Q conventions worth remembering:
 *   - highPass/lowPass: Q = 0.7071 (1/sqrt2) is Butterworth - maximally
 *     flat, -3 dB at f0, 12 dB/oct. Higher Q adds a resonant bump at f0.
 *   - bell: Q relates to bandwidth as BW = f0/Q; RBJ's alpha uses Q
 *     directly so the curve stays symmetric on a log axis.
 *   - shelves: RBJ's "shelf slope" S is fixed at 1 here (gentlest slope
 *     with no overshoot), which is what most channel EQs ship.
 */
namespace eq
{

enum class FilterType
{
    bell = 0,   // must match the order of the "Type" choice parameter
    lowShelf,
    highShelf,
    highPass,
    lowPass,
    notch
};

struct BiquadCoefficients
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0; // numerator (zeros)
    double a1 = 0.0, a2 = 0.0;           // denominator (poles), a0 normalised to 1
};

/// Designs one RBJ biquad. gainDb is ignored by highPass/lowPass/notch.
BiquadCoefficients design (FilterType type, double freqHz, double gainDb,
                           double q, double sampleRate);

/// |H(e^{jw})| at freqHz - the exact magnitude response, used by the UI
/// curve, and by tests to pin the designs against cookbook theory.
double magnitudeAt (const BiquadCoefficients& c, double freqHz, double sampleRate);

/// Per-channel filter state, Transposed Direct Form II: best float
/// behaviour of the direct forms (single accumulation point) and only two
/// state variables. State is kept in double so a low-frequency, high-Q
/// band doesn't dissolve into quantisation noise.
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    inline float processSample (float x, const BiquadCoefficients& c) noexcept
    {
        const double in = static_cast<double> (x);
        const double y = c.b0 * in + s1;
        s1 = c.b1 * in - c.a1 * y + s2;
        s2 = c.b2 * in - c.a2 * y;
        return static_cast<float> (y);
    }

    void reset() noexcept { s1 = s2 = 0.0; }
};

} // namespace eq
