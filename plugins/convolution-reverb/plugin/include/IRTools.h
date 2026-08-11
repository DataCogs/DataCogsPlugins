#pragma once

/**
 * IR measurement and reshaping - plain C++, no JUCE dependency, same policy
 * as IRGenerator: compiles standalone for calibration checks and unit-tests
 * without the framework.
 *
 * These two functions are the guts of the "Decay" control. The trick for
 * changing the decay time of an *arbitrary measured* IR:
 *
 *  1. Measure its current RT60. Schroeder's backward integration (JASA
 *     1965) turns one noisy squared impulse response into the smooth
 *     ensemble-average energy decay curve:
 *
 *         EDC(t) = integral from t to infinity of h^2(tau) dtau
 *
 *     i.e. "energy remaining after t". On a dB plot this is close to a
 *     straight line for a well-behaved room; the slope gives RT60. Per
 *     ISO 3382 practice we fit the -5..-25 dB span (T20, x3) so neither
 *     the direct sound nor the noise floor biases the estimate.
 *
 *  2. Multiply the IR by a corrective exponential. Exponential decays
 *     compose by adding rates: if the room decays at rate k1 = 6.9078/T
 *     (see IRGenerator.h for the RT60 derivation) and we want rate
 *     k2 = 6.9078/(scale*T), multiplying by exp(-(k2-k1) t) gets there
 *     exactly - no spectral processing needed, the modal structure of the
 *     room is untouched, only its energy envelope changes.
 *
 * scale > 1 (longer decay) means a *growing* corrective envelope, which
 * also amplifies the capture's noise floor - physics, not a bug. The boost
 * is clamped (kMaxBoostDb) so a long IR can't pull its tail noise up to
 * full scale.
 */
namespace ir
{

/// RT60 in seconds estimated from the Schroeder EDC, T20 method (-5..-25 dB
/// fit, extrapolated x3), falling back to T10 (x6) for short/noisy IRs.
/// Returns 0 if the IR is too short or silent to measure.
double measureRt60 (const float* h, int numSamples, double sampleRate);

/// In-place: rescales the IR's decay time by `scale` (0.5 = half as long,
/// 2.0 = twice as long) given its measured RT60. No-op on degenerate input.
void reshapeDecay (float* h, int numSamples, double sampleRate,
                   double measuredRt60, double scale);

/// In-place: applies separate gains to the early and late parts of the IR.
///
/// The perceptual split: early reflections (the first ~80 ms after the
/// direct sound) carry the room's *geometry* - distance, wall proximity,
/// stage placement - while the late tail carries its *size and character*.
/// Mixing them independently is most of Altiverb's workflow: pull the tail
/// down for clarity, or the earlies down for a "further away" sound.
///
/// The direct-sound onset is detected as the first sample within 20 dB of
/// the IR's peak (captures usually lead with near-silence); the boundary
/// sits kEarlyWindowSeconds after it, and the two regions are joined by a
/// raised-cosine crossfade so no discontinuity is inserted into the IR.
void applyEarlyTailGains (float* h, int numSamples, double sampleRate,
                          float earlyGain, float tailGain);

constexpr double kEarlyWindowSeconds = 0.080;
constexpr double kSplitFadeSeconds   = 0.010;

} // namespace ir
