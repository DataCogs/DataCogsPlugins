#include "IRGenerator.h"

#include <cmath>
#include <random>

namespace ir
{

std::vector<std::vector<float>> generateNoiseTailIr (const BuiltInIrSpec& spec,
                                                     int numChannels)
{
    const double fs = spec.sampleRate;

    // 1.15 * RT60 leaves 15% of headroom past the -60 dB point; the level
    // down there is below -60 dB, i.e. below most 16-bit noise floors, so
    // cutting after a short fade is inaudible.
    const int numSamples = static_cast<int> (std::ceil (1.15 * spec.rt60Seconds * fs));
    const int fadeSamples = std::max (1, numSamples / 32); // last ~3.5%

    // Envelope: g(t) = exp(-6.9078 t / RT60), see header for the derivation.
    // Computed incrementally as a per-sample multiplier so the loop stays a
    // single multiply instead of an exp() per sample:
    //   g[n+1]/g[n] = exp(-3 ln10 / (RT60 * fs))
    const double envelopeStep =
        std::exp (-3.0 * std::log (10.0) / (static_cast<double> (spec.rt60Seconds) * fs));

    std::vector<std::vector<float>> channels;
    channels.reserve (static_cast<size_t> (numChannels));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Different seed per channel -> decorrelated tails. Interaural
        // decorrelation is what reads as "spacious"; identical tails would
        // collapse to a mono reverb image (Blauert, Spatial Hearing).
        std::mt19937 rng (spec.seed + static_cast<std::uint32_t> (ch) * 7919u);
        std::normal_distribution<float> gauss (0.0f, 1.0f);

        std::vector<float> h (static_cast<size_t> (numSamples));

        double envelope = 1.0;
        float lpState = 0.0f;

        // Damping sweep runs in log-frequency (perceptually linear): the
        // cutoff glides from dampingStartHz to dampingEndHz across the tail.
        const double logStart = std::log (static_cast<double> (spec.dampingStartHz));
        const double logEnd   = std::log (static_cast<double> (spec.dampingEndHz));

        for (int n = 0; n < numSamples; ++n)
        {
            float x = gauss (rng);

            if (spec.frequencyDependent)
            {
                const double frac = static_cast<double> (n) / numSamples;
                const double cutoff = std::exp (logStart + frac * (logEnd - logStart));
                // One-pole lowpass: y[n] = y[n-1] + (1-a)(x[n] - y[n-1]),
                // a = exp(-2 pi fc / fs).
                const float a = static_cast<float> (std::exp (-2.0 * 3.14159265358979323846 * cutoff / fs));
                lpState += (1.0f - a) * (x - lpState);
                x = lpState;
            }

            h[static_cast<size_t> (n)] = x * static_cast<float> (envelope);
            envelope *= envelopeStep;
        }

        // Raised-cosine fade-out over the last few samples: the tail is
        // ~-62 dB there already, but a hard truncation of noise still puts a
        // tiny broadband edge in the IR; the fade removes it for free.
        for (int n = 0; n < fadeSamples; ++n)
        {
            const size_t idx = static_cast<size_t> (numSamples - fadeSamples + n);
            const float w = 0.5f * (1.0f + std::cos (3.14159265358979323846f * n / fadeSamples));
            h[idx] *= w;
        }

        channels.push_back (std::move (h));
    }

    return channels;
}

} // namespace ir
