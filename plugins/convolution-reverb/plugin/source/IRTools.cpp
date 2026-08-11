#include "IRTools.h"

#include <cmath>
#include <vector>

namespace ir
{

double measureRt60 (const float* h, int numSamples, double sampleRate)
{
    if (h == nullptr || numSamples <= 1 || sampleRate <= 0.0)
        return 0.0;

    // Backward-integrated energy: edc[i] = sum of h^2 from i to the end.
    // Accumulated in double - a 15 s 48 kHz IR is 720k additions and float
    // accumulation would lose the tail entirely.
    std::vector<double> edc (static_cast<size_t> (numSamples));
    double acc = 0.0;
    for (int i = numSamples - 1; i >= 0; --i)
    {
        acc += static_cast<double> (h[i]) * h[i];
        edc[static_cast<size_t> (i)] = acc;
    }

    if (acc <= 0.0)
        return 0.0; // silent buffer

    // First sample index where the remaining energy has dropped `db` below
    // the total. EDC is monotonically non-increasing so a linear scan from
    // the front finds the earliest crossing.
    const double total = edc[0];
    auto firstIndexBelow = [&] (double db) -> int
    {
        const double target = total * std::pow (10.0, db / 10.0);
        for (int i = 0; i < numSamples; ++i)
            if (edc[static_cast<size_t> (i)] <= target)
                return i;
        return -1;
    };

    const int t5 = firstIndexBelow (-5.0);
    if (t5 < 0)
        return 0.0;

    // T20: the -5..-25 dB span covers 20 dB, so RT60 = 3x its duration.
    if (const int t25 = firstIndexBelow (-25.0); t25 > t5)
        return 3.0 * (t25 - t5) / sampleRate;

    // Fallback T10 for IRs whose noise floor sits above -25 dB.
    if (const int t15 = firstIndexBelow (-15.0); t15 > t5)
        return 6.0 * (t15 - t5) / sampleRate;

    return 0.0;
}

void reshapeDecay (float* h, int numSamples, double sampleRate,
                   double measuredRt60, double scale)
{
    if (h == nullptr || numSamples <= 0 || sampleRate <= 0.0
        || measuredRt60 <= 0.0 || scale <= 0.0)
        return;

    if (std::abs (scale - 1.0) < 1e-6)
        return; // exact no-op, keep the untouched IR bit-identical

    // Decay rates add: k = 3 ln10 / RT60 (the -60 dB definition, see
    // IRGenerator.h). The corrective envelope is exp(-(k_target - k_now) t).
    const double threeLn10 = 3.0 * std::log (10.0); // 6.9078
    const double deltaRate = threeLn10 * (1.0 / (measuredRt60 * scale) - 1.0 / measuredRt60);

    // Clamp growth (scale > 1 => deltaRate < 0 => rising envelope) so the
    // capture's noise floor can't be boosted without bound.
    constexpr double kMaxBoostDb = 40.0;
    const double maxExponent = kMaxBoostDb * std::log (10.0) / 20.0;

    for (int i = 0; i < numSamples; ++i)
    {
        double exponent = -deltaRate * (static_cast<double> (i) / sampleRate);
        if (exponent > maxExponent)
            exponent = maxExponent;
        h[i] = static_cast<float> (h[i] * std::exp (exponent));
    }
}

void applyEarlyTailGains (float* h, int numSamples, double sampleRate,
                          float earlyGain, float tailGain)
{
    if (h == nullptr || numSamples <= 0 || sampleRate <= 0.0)
        return;

    // Onset = first sample within 20 dB of the peak; measured IRs lead with
    // recording-chain noise well below that, the direct sound is at/near
    // the peak.
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max (peak, std::abs (h[i]));
    if (peak <= 0.0f)
        return;

    const float onsetThreshold = peak * 0.1f; // -20 dB
    int onset = 0;
    while (onset < numSamples && std::abs (h[onset]) < onsetThreshold)
        ++onset;

    const int boundary    = onset + static_cast<int> (kEarlyWindowSeconds * sampleRate);
    const int fadeSamples = std::max (1, static_cast<int> (kSplitFadeSeconds * sampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        // f: 0 in the early region, raised-cosine 0->1 across the fade,
        // 1 in the tail; gain interpolates between the two levels.
        double f;
        if (i < boundary)
            f = 0.0;
        else if (i >= boundary + fadeSamples)
            f = 1.0;
        else
            f = 0.5 * (1.0 - std::cos (3.14159265358979323846
                                       * (i - boundary) / fadeSamples));

        h[i] = static_cast<float> (h[i] * (earlyGain + (tailGain - earlyGain) * f));
    }
}

} // namespace ir
