#pragma once

#include <cstdint>
#include <vector>

/**
 * Synthetic impulse-response generator - plain C++, no JUCE dependency, so it
 * compiles standalone (clang++ -I plugin/include ...) for quick calibration
 * checks against theory, and unit-tests without pulling in the framework.
 *
 * The model is the classic one from Moorer, "About This Reverberation
 * Business" (CMJ 1979): the late field of a real room is statistically
 * indistinguishable from exponentially decaying white noise. Schroeder's
 * frequency-domain view says the same thing - beyond the mixing time the
 * mode density is so high that the response at any instant is Gaussian.
 * So a perfectly serviceable "hall" IR is:
 *
 *     h(t) = n(t) * g(t),   n ~ N(0,1),   g(t) = 10^(-3 t / RT60)
 *
 * The envelope derivation: RT60 is defined as the time for the level to
 * fall 60 dB, i.e. the amplitude to fall by a factor 10^(-60/20) = 10^-3.
 * Exponential decay with that boundary condition is
 *
 *     g(t) = 10^(-3 t / RT60) = exp(-(3 ln 10 / RT60) * t)
 *          = exp(-6.9078 t / RT60)
 *
 * Real rooms also decay faster at high frequencies (air absorption plus
 * carpets/drapes soak up treble first). We fake that with a first-order
 * lowpass run over the tail whose cutoff glides downward over time - early
 * samples keep their sparkle, the tail darkens progressively. One-pole
 * coefficient per the usual mapping a = exp(-2*pi*fc/fs).
 */
namespace ir
{

struct BuiltInIrSpec
{
    double sampleRate          = 48000.0;
    float  rt60Seconds         = 2.2f;     // decay time to -60 dB
    bool   frequencyDependent  = true;     // sweep a lowpass down the tail
    float  dampingStartHz      = 16000.0f; // lowpass cutoff at t = 0
    float  dampingEndHz        = 2500.0f;  // lowpass cutoff at t = end
    std::uint32_t seed         = 0x44436F67; // "DCog" - deterministic output
};

/// Generates numChannels decorrelated noise tails (per-channel seed offset).
/// Length is 1.15 * RT60 with a short raised-cosine fade at the very end so
/// truncation never clicks. Channels are NOT normalised - the convolution
/// engine's Normalise option owns loudness matching.
std::vector<std::vector<float>> generateNoiseTailIr (const BuiltInIrSpec& spec,
                                                     int numChannels);

} // namespace ir
