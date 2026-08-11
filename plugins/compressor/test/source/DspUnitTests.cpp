//
//  DspUnitTests.cpp
//
//  Direct tests of the plain-C++ DSP units: the static gain curve
//  (Compressor::computeGainDb), the envelope follower ballistics, the
//  sidechain high-pass, and the decoupled gain smoother. Expected values
//  come from the derivations documented in Compressor.h / EnvelopeDetector.h;
//  several cases are regression guards for the two historic calibration bugs
//  called out there (base-10-vs-natural-log time constants, and
//  sqrt-before-smoothing collapsing RMS into peak mode).
//
//  No JUCE types here: these classes are JUCE-independent and the tests
//  keep them that way.
//

#include <catch2/catch_all.hpp>
#include "Compressor.h"
#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace dsp_test
{
    constexpr double kPi = 3.14159265358979323846;

    static std::vector<float> makeSine (int numSamples, double freqHz, double sampleRate, double amplitude)
    {
        std::vector<float> v ((size_t) numSamples);
        for (int n = 0; n < numSamples; ++n)
            v[(size_t) n] = (float) (amplitude * std::sin (2.0 * kPi * freqHz * (double) n / sampleRate));
        return v;
    }

    //==========================================================================
    // Static gain curve
    //==========================================================================

    TEST_CASE ("Static curve: hard knee is the textbook piecewise-linear curve", "[dsp]")
    {
        Compressor c { 1 };
        c.threshold = -18.0f;
        c.ratio = 4.0f;
        c.kneeWidth = 0.0f; // hard knee (the APVTS minimum) must not divide by zero
        c.maxReductionDb = 40.0f;

        // Below and at threshold: unity gain.
        CHECK (c.computeGainDb (-60.0f) == 0.0f);
        CHECK (c.computeGainDb (-18.0f) == 0.0f);

        // Above threshold: gain = (1/R - 1) * overshoot.
        CHECK_THAT (c.computeGainDb (-6.0f), WithinAbs (-9.0, 1.0e-4));  // -0.75 * 12
        CHECK_THAT (c.computeGainDb (0.0f), WithinAbs (-13.5, 1.0e-4)); // -0.75 * 18
    }

    TEST_CASE ("Static curve: ratio 1 is unity gain everywhere", "[dsp]")
    {
        Compressor c { 1 };
        c.threshold = -18.0f;
        c.ratio = 1.0f;
        c.kneeWidth = 6.0f;

        for (float level = -60.0f; level <= 6.0f; level += 0.5f)
            CHECK (c.computeGainDb (level) == 0.0f);
    }

    TEST_CASE ("Static curve: soft knee midpoint, edges and continuity", "[dsp]")
    {
        Compressor c { 1 };
        c.threshold = -18.0f;
        c.ratio = 4.0f;
        c.kneeWidth = 6.0f;

        const float slope = 1.0f / c.ratio - 1.0f; // -0.75

        // Knee midpoint (level = T): Eq. 4 parabola gives slope * W/8.
        CHECK_THAT (c.computeGainDb (-18.0f), WithinAbs ((double) (slope * 6.0f / 8.0f), 1.0e-4));

        // Both knee edges match the outer segments in value.
        CHECK_THAT (c.computeGainDb (-21.0f), WithinAbs (0.0, 1.0e-4));                    // T - W/2
        CHECK_THAT (c.computeGainDb (-15.0f), WithinAbs ((double) (slope * 3.0f), 1.0e-4)); // T + W/2

        // Sweep the whole range: the gain must be continuous (bounded by the
        // maximum curve slope), non-increasing, and the output level
        // (level + gain) must be non-decreasing. Accumulate the worst
        // violations and assert once.
        const float step = 0.01f;
        float previousGain = c.computeGainDb (-60.0f);
        float worstJump = 0.0f, worstGainRise = 0.0f, worstOutputDrop = 0.0f;
        for (float level = -60.0f + step; level <= 0.0f; level += step)
        {
            const float gain = c.computeGainDb (level);
            worstJump = std::max (worstJump, std::fabs (gain - previousGain));
            worstGainRise = std::max (worstGainRise, gain - previousGain);
            worstOutputDrop = std::max (worstOutputDrop, previousGain - gain - step);
            previousGain = gain;
        }
        CHECK (worstJump <= std::fabs (slope) * step + 1.0e-4f);
        CHECK (worstGainRise <= 1.0e-5f);
        CHECK (worstOutputDrop <= 1.0e-5f);

        // First derivative sweeps 0 -> slope across the knee: numerically
        // differentiate just outside each edge and at the midpoint.
        auto derivative = [&c] (float level)
        {
            const float h = 0.001f;
            return (c.computeGainDb (level + h) - c.computeGainDb (level - h)) / (2.0f * h);
        };
        CHECK_THAT (derivative (-21.1f), WithinAbs (0.0, 0.01));                  // below the knee
        CHECK_THAT (derivative (-18.0f), WithinAbs ((double) (slope / 2.0f), 0.01)); // midpoint
        CHECK_THAT (derivative (-14.9f), WithinAbs ((double) slope, 0.01));       // above the knee
    }

    TEST_CASE ("Static curve: range caps the gain reduction", "[dsp]")
    {
        Compressor c { 1 };
        c.threshold = -18.0f;
        c.ratio = 20.0f;
        c.kneeWidth = 0.0f;
        c.maxReductionDb = 6.0f;

        // 0 dB input at 20:1 wants -17.1 dB of gain; the range floors it at -6.
        CHECK (c.computeGainDb (0.0f) == -6.0f);

        for (float level = -60.0f; level <= 12.0f; level += 0.25f)
            CHECK (c.computeGainDb (level) >= -c.maxReductionDb);
    }

    //==========================================================================
    // Envelope detector ballistics
    //==========================================================================

    TEST_CASE ("Envelope detector: digital times satisfy the a^N = eps convention", "[dsp]")
    {
        // Regression guard for the base-10-vs-natural-log bug: with the wrong
        // constant every time runs ln(10) = 2.3x slower than labeled.
        for (float sampleRate : { 44100.0f, 96000.0f })
        {
            EnvelopeDetector d;
            d.setMode (EnvelopeDetector::Mode::Peak);
            d.prepare (sampleRate);
            d.setAttackTime (10.0f);
            d.setReleaseTime (100.0f);

            const int attackSamples = (int) std::lround (10.0f * sampleRate / 1000.0f);
            const int releaseSamples = (int) std::lround (100.0f * sampleRate / 1000.0f);

            // Step 0 -> 1: after exactly the attack time, 99% covered.
            float env = 0.0f;
            for (int n = 0; n < attackSamples; ++n)
                env = d.processSample (1.0f);
            CHECK_THAT (env, WithinAbs (0.99, 0.005));

            // Charge fully, then step 1 -> 0: after the release time, 1% left.
            for (int n = 0; n < attackSamples * 5; ++n)
                env = d.processSample (1.0f);
            for (int n = 0; n < releaseSamples; ++n)
                env = d.processSample (0.0f);
            CHECK_THAT (env, WithinAbs (0.01, 0.005));
        }
    }

    TEST_CASE ("Envelope detector: analog convention reaches 63.2% in one time constant", "[dsp]")
    {
        EnvelopeDetector d;
        d.setMode (EnvelopeDetector::Mode::Peak);
        d.prepare (48000.0f);
        d.setTimeConstant (EnvelopeDetector::TimeConstant::Analog);
        d.setAttackTime (10.0f);
        d.setReleaseTime (100.0f);

        float env = 0.0f;
        const int attackSamples = 480; // 10 ms at 48 kHz
        for (int n = 0; n < attackSamples; ++n)
            env = d.processSample (1.0f);
        CHECK_THAT (env, WithinAbs (0.6321, 0.01)); // 1 - 1/e
    }

    TEST_CASE ("Envelope detector: RMS tracks energy, Peak tracks the rectified mean", "[dsp]")
    {
        // Regression guard for the sqrt-before-smoothing bug, which made RMS
        // mode identical to peak mode. With EQUAL attack and release the
        // branching smoother is a plain one-pole, so on a steady sine:
        //   RMS  -> sqrt(mean(x^2)) = A/sqrt(2) = 0.7071
        //   Peak -> mean(|x|)       = 2A/pi     = 0.6366
        const float sampleRate = 44100.0f;
        const auto signal = makeSine (44100, 1000.0, (double) sampleRate, 1.0);

        auto steadyStateMean = [&] (EnvelopeDetector::Mode mode)
        {
            EnvelopeDetector d;
            d.setMode (mode);
            d.prepare (sampleRate);
            d.setAttackTime (50.0f);
            d.setReleaseTime (50.0f);

            double sum = 0.0;
            int count = 0;
            for (int n = 0; n < (int) signal.size(); ++n)
            {
                const float env = d.processSample (signal[(size_t) n]);
                if (n >= (int) signal.size() - 4410) // average the last 100 ms
                {
                    sum += (double) env;
                    ++count;
                }
            }
            return (float) (sum / count);
        };

        const float rmsMean = steadyStateMean (EnvelopeDetector::Mode::RMS);
        const float peakMean = steadyStateMean (EnvelopeDetector::Mode::Peak);

        CHECK_THAT (rmsMean, WithinAbs (0.7071, 0.01));
        CHECK_THAT (peakMean, WithinAbs (0.6366, 0.01));
        CHECK (std::fabs (rmsMean - peakMean) > 0.04f); // the bug makes these equal
    }

    TEST_CASE ("Envelope detector: fast-attack RMS sits ~1.6 dB above true RMS", "[dsp]")
    {
        // Characterization of the documented crest-riding bias (see the RMS
        // caveat in EnvelopeDetector.h, measured at 5 ms / 50 ms).
        const float sampleRate = 44100.0f;
        const auto signal = makeSine (44100, 1000.0, (double) sampleRate, 1.0);

        EnvelopeDetector d;
        d.setMode (EnvelopeDetector::Mode::RMS);
        d.prepare (sampleRate);
        d.setAttackTime (5.0f);
        d.setReleaseTime (50.0f);

        double sum = 0.0;
        int count = 0;
        for (int n = 0; n < (int) signal.size(); ++n)
        {
            const float env = d.processSample (signal[(size_t) n]);
            if (n >= (int) signal.size() - 4410)
            {
                sum += (double) env;
                ++count;
            }
        }

        const float biasDb = Compressor::amplitudeToDb ((float) (sum / count)) - (-3.0103f);
        CHECK (biasDb > 1.0f);
        CHECK (biasDb < 2.2f);
    }

    //==========================================================================
    // Sidechain high-pass filter
    //==========================================================================

    TEST_CASE ("Sidechain filter: cutoff 0 is an exact bypass", "[dsp]")
    {
        SidechainFilter f;
        f.prepare (44100.0f);
        f.setCutoff (0.0f);

        const auto signal = makeSine (4096, 100.0, 44100.0, 0.9);
        for (float x : signal)
            CHECK (f.processSample (x) == x); // bit-exact, as documented
    }

    TEST_CASE ("Sidechain filter: ~12 dB/oct highpass around the cutoff", "[dsp]")
    {
        // Two cascaded one-poles: ~6 dB down at the corner, ~14 dB an octave
        // below, transparent well above. Steady-state RMS over the last half
        // of a one-second probe tone.
        const float sampleRate = 44100.0f;

        auto attenuationDb = [sampleRate] (double probeHz)
        {
            SidechainFilter f;
            f.prepare (sampleRate);
            f.setCutoff (120.0f);

            const int numSamples = 44100;
            const auto in = makeSine (numSamples, probeHz, (double) sampleRate, 1.0);

            double sumSq = 0.0;
            int count = 0;
            for (int n = 0; n < numSamples; ++n)
            {
                const float out = f.processSample (in[(size_t) n]);
                if (n >= numSamples / 2)
                {
                    sumSq += (double) out * (double) out;
                    ++count;
                }
            }
            const float outRmsDb = Compressor::amplitudeToDb ((float) std::sqrt (sumSq / count));
            return -3.0103f - outRmsDb; // input RMS of a full-scale sine is -3.01 dB
        };

        const float atCutoff = attenuationDb (120.0);
        const float octaveBelow = attenuationDb (60.0);
        const float wellAbove = attenuationDb (1000.0);

        CHECK (atCutoff > 5.0f);
        CHECK (atCutoff < 7.5f);
        CHECK (octaveBelow > 11.5f);
        CHECK (octaveBelow < 17.0f);
        CHECK (wellAbove < 0.6f);
        CHECK (octaveBelow > atCutoff + 5.0f); // the slope actually steepens downward
    }

    //==========================================================================
    // Decoupled gain smoother
    //==========================================================================

    TEST_CASE ("Decoupled smoother: attack and release trajectories", "[dsp]")
    {
        const float sampleRate = 48000.0f;
        const int attackSamples = 480;   // 10 ms
        const int releaseSamples = 4800; // 100 ms

        DecoupledSmoother s;
        s.prepare (sampleRate);
        s.setAttackTime (10.0f);
        s.setReleaseTime (100.0f);

        // Step up: the release stage passes the step instantly (the max), so
        // the attack stage alone governs - 99% covered at the attack time.
        int riseCrossing = -1;
        for (int n = 0; n < attackSamples * 4; ++n)
        {
            if (s.processSample (1.0f) >= 0.99f)
            {
                riseCrossing = n + 1;
                break;
            }
        }
        REQUIRE (riseCrossing > 0);
        CHECK (riseCrossing > (int) (0.9f * (float) attackSamples));
        CHECK (riseCrossing < (int) (1.1f * (float) attackSamples));

        // Step down: the cascade decays a little slower than the release
        // stage alone (the paper's tauA + tauR interaction) - the 1% point
        // lands just past the labeled release time.
        int fallCrossing = -1;
        for (int n = 0; n < releaseSamples * 3; ++n)
        {
            if (s.processSample (0.0f) <= 0.01f)
            {
                fallCrossing = n + 1;
                break;
            }
        }
        REQUIRE (fallCrossing > 0);
        CHECK (fallCrossing > (int) (0.95f * (float) releaseSamples));
        CHECK (fallCrossing < (int) (1.35f * (float) releaseSamples));
    }
} // namespace dsp_test
