//
//  ProcessorTests.cpp
//
//  Behavioral tests of CompressorAudioProcessor::processBlock: steady-state
//  gain reduction against the static curve, ballistics timing, stereo
//  linking, mix/bypass/makeup semantics, and robustness (sample-rate and
//  block-size invariance, silence, hot input, state recall).
//
//  Predictions are derived in-test from the same formulas documented in the
//  DSP headers, with tolerances loose enough to survive the CI matrix
//  (macOS arm64, Windows x64) - these guard against calibration-class
//  regressions (e.g. the historic 2.3x time-constant bug), not exact values.
//

#include <catch2/catch_all.hpp>
#include "TestSignals.h"
#include <algorithm>
#include <cmath>
#include <memory>

using Catch::Matchers::WithinAbs;
using namespace TestSignals;

namespace processor_test
{
    namespace
    {
        // Largest per-sample difference between two equally-sized buffers -
        // buffer comparisons assert once on this instead of once per sample.
        float maxAbsDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
        {
            float maxDiff = 0.0f;
            for (int ch = 0; ch < a.getNumChannels(); ++ch)
                for (int n = 0; n < a.getNumSamples(); ++n)
                    maxDiff = std::max (maxDiff, std::fabs (a.getSample (ch, n) - b.getSample (ch, n)));
            return maxDiff;
        }
    } // namespace

    //==========================================================================
    // Group B: processBlock behavior
    //==========================================================================

    TEST_CASE ("Steady-state gain reduction follows the static curve", "[processor]")
    {
        // -6 dBFS sine, threshold -18, ratio 4, hard knee, equal
        // attack/release (so the detectors settle on their textbook means).
        // Expected output level = input RMS + slope * (detected level - T).
        const double sampleRate = 44100.0;
        const int numSamples = 44100;
        const float amplitude = 0.5f;
        const float threshold = -18.0f;
        const float slope = 1.0f / 4.0f - 1.0f; // -0.75 at ratio 4

        const float inputRmsDb = Compressor::amplitudeToDb (amplitude / std::sqrt (2.0f));

        struct Config
        {
            const char* name;
            float topology; // 0 Classic, 1 Log-Domain
            float detector; // 0 Peak, 1 RMS
            float detectedDb;
            float toleranceDb;
        };

        const Config configs[] = {
            // Classic/RMS: detector converges to sqrt(mean(x^2)) = A/sqrt(2).
            { "Classic/RMS", 0.0f, 1.0f, Compressor::amplitudeToDb (amplitude / std::sqrt (2.0f)), 0.75f },
            // Classic/Peak: detector converges to mean(|x|) = 2A/pi.
            { "Classic/Peak", 0.0f, 0.0f, Compressor::amplitudeToDb (2.0f * amplitude / 3.14159265f), 0.75f },
            // Log-Domain: instantaneous peak detection, the smoother's release
            // stage rides the crests, so the gain sits near the static curve
            // at the sine's PEAK level.
            { "Log-Domain", 1.0f, 1.0f, Compressor::amplitudeToDb (amplitude), 1.0f },
        };

        for (const auto& config : configs)
        {
            INFO (config.name);
            auto ji = juce::ScopedJuceInitialiser_GUI();
            auto processor = std::make_unique<CompressorAudioProcessor>();
            setParam (*processor, "threshold", threshold);
            setParam (*processor, "ratio", 4.0f);
            setParam (*processor, "kneeWidth", 0.0f);
            setParam (*processor, "attack", 50.0f);
            setParam (*processor, "release", 50.0f);
            setParam (*processor, "topology", config.topology);
            setParam (*processor, "detector", config.detector);
            processor->prepareToPlay (sampleRate, 512);

            auto buffer = sineBuffer (2, numSamples, 1000.0, sampleRate, (double) amplitude);
            processInPlace (*processor, buffer, 512);

            const float expectedOutDb = inputRmsDb + slope * (config.detectedDb - threshold);
            const float measuredOutDb = rmsDb (buffer, 0, numSamples - 17640, 17640); // last 400 ms
            CHECK_THAT (measuredOutDb, WithinAbs ((double) expectedOutDb, (double) config.toleranceDb));
        }
    }

    namespace
    {
        // Feed a DC step of 1.0 through the Classic/Peak path and return the
        // sample where the gain-reduction trajectory crosses 99% of its final
        // value. With a constant input the applied gain is recoverable
        // exactly per sample (out = in * gain, in = 1), so the trajectory is
        // the unit-level envelope math with the whole processor in the loop.
        int measureAttackCrossing (double sampleRate, float attackMs)
        {
            auto processor = std::make_unique<CompressorAudioProcessor>();
            setParam (*processor, "threshold", -18.0f);
            setParam (*processor, "ratio", 4.0f);
            setParam (*processor, "kneeWidth", 0.0f);
            setParam (*processor, "attack", attackMs);
            setParam (*processor, "release", 200.0f);
            setParam (*processor, "detector", 0.0f); // Peak
            processor->prepareToPlay (sampleRate, 512);

            const int numSamples = (int) (sampleRate / 2.0);
            auto buffer = constantBuffer (2, numSamples, 1.0f);
            processInPlace (*processor, buffer, 512);

            const float finalGrDb = -Compressor::amplitudeToDb (buffer.getSample (0, numSamples - 1));
            for (int n = 0; n < numSamples; ++n)
                if (-Compressor::amplitudeToDb (buffer.getSample (0, n)) >= 0.99f * finalGrDb)
                    return n + 1;
            return -1;
        }

        // Predicted crossing for the digital convention: the detector step
        // response is e(n) = 1 - 0.01^(n/N), and the 99%-of-final-GR point
        // maps back through the static curve to a target envelope value.
        // Linear in its argument, so it works in samples or milliseconds.
        float predictedAttackCrossing (float attackTime)
        {
            const float slope = 0.75f;
            const float finalGrDb = slope * 18.0f; // envelope -> 1, threshold -18
            const float targetEnv = std::pow (10.0f, (0.99f * finalGrDb / slope - 18.0f) / 20.0f);
            return attackTime * std::log (1.0f - targetEnv) / std::log (0.01f);
        }
    } // namespace

    TEST_CASE ("Attack and release timing through processBlock", "[processor]")
    {
        const double sampleRate = 44100.0;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        const float attackMs = 50.0f;
        const float releaseMs = 200.0f;
        const float attackSamples = (float) (attackMs * sampleRate / 1000.0);
        const float releaseSamples = (float) (releaseMs * sampleRate / 1000.0);

        // --- Attack: DC step 0 -> 1.
        const int attackCrossing = measureAttackCrossing (sampleRate, attackMs);
        REQUIRE (attackCrossing > 0);
        const float predictedAttack = predictedAttackCrossing (attackSamples);
        CHECK ((float) attackCrossing > 0.8f * predictedAttack);
        CHECK ((float) attackCrossing < 1.2f * predictedAttack);

        // --- Release: step 1.0 -> 0.01 (-40 dB, below threshold), measure
        // how long the gain reduction takes to die away.
        auto processor = std::make_unique<CompressorAudioProcessor>();
        setParam (*processor, "threshold", -18.0f);
        setParam (*processor, "ratio", 4.0f);
        setParam (*processor, "kneeWidth", 0.0f);
        setParam (*processor, "attack", attackMs);
        setParam (*processor, "release", releaseMs);
        setParam (*processor, "detector", 0.0f); // Peak
        processor->prepareToPlay (sampleRate, 512);

        const int loudSamples = 22050;
        const int quietSamples = 35280;
        juce::AudioBuffer<float> buffer (2, loudSamples + quietSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                buffer.setSample (ch, n, n < loudSamples ? 1.0f : 0.01f);
        processInPlace (*processor, buffer, 512);

        int releaseCrossing = -1;
        for (int n = loudSamples; n < buffer.getNumSamples(); ++n)
        {
            const float grDb = -Compressor::amplitudeToDb (buffer.getSample (0, n) / 0.01f);
            if (grDb <= 0.05f)
            {
                releaseCrossing = n - loudSamples + 1;
                break;
            }
        }
        REQUIRE (releaseCrossing > 0);

        // Predicted: envelope decays from ~1 toward 0.01; GR ends when it
        // falls back through the level where the curve reads 0.05 dB of GR.
        const float slope = 0.75f;
        const float targetEnv = std::pow (10.0f, (0.05f / slope - 18.0f) / 20.0f);
        const float predictedRelease = releaseSamples
                                       * std::log ((targetEnv - 0.01f) / (1.0f - 0.01f))
                                       / std::log (0.01f);
        CHECK ((float) releaseCrossing > 0.8f * predictedRelease);
        CHECK ((float) releaseCrossing < 1.2f * predictedRelease);
    }

    TEST_CASE ("Stereo link applies one shared gain to both channels", "[processor]")
    {
        // Loud sine left, quiet sine right. Unlinked, the right channel
        // (below threshold on its own) would pass untouched; linked, it must
        // be attenuated by exactly as many dB as the left.
        const double sampleRate = 44100.0;
        const int numSamples = 44100;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        auto processor = std::make_unique<CompressorAudioProcessor>();
        setParam (*processor, "threshold", -18.0f);
        setParam (*processor, "ratio", 4.0f);
        setParam (*processor, "attack", 50.0f);
        setParam (*processor, "release", 50.0f);
        processor->prepareToPlay (sampleRate, 512);

        juce::AudioBuffer<float> buffer (2, numSamples);
        for (int n = 0; n < numSamples; ++n)
        {
            const double s = std::sin (juce::MathConstants<double>::twoPi * 1000.0 * n / sampleRate);
            buffer.setSample (0, n, (float) (0.9 * s));
            buffer.setSample (1, n, (float) (0.09 * s));
        }
        const juce::AudioBuffer<float> input (buffer); // keep the dry copy

        processInPlace (*processor, buffer, 512);

        const int start = numSamples - 17640; // last 400 ms
        const float attenuationL = rmsDb (input, 0, start, 17640) - rmsDb (buffer, 0, start, 17640);
        const float attenuationR = rmsDb (input, 1, start, 17640) - rmsDb (buffer, 1, start, 17640);

        CHECK (attenuationR > 5.0f); // the quiet side is being ducked by the loud side
        CHECK_THAT (attenuationR, WithinAbs ((double) attenuationL, 0.1));
    }

    TEST_CASE ("Mix: 0% is a dry passthrough, 50% is a linear blend", "[processor]")
    {
        const double sampleRate = 44100.0;
        const int numSamples = 44100;
        auto ji = juce::ScopedJuceInitialiser_GUI();
        const auto input = sineBuffer (2, numSamples, 1000.0, sampleRate, 0.5);

        auto runWithMix = [&] (float mixPercent)
        {
            auto processor = std::make_unique<CompressorAudioProcessor>();
            setParam (*processor, "threshold", -40.0f);
            setParam (*processor, "ratio", 10.0f);
            setParam (*processor, "attack", 5.0f);
            setParam (*processor, "release", 50.0f);
            setParam (*processor, "makeupGain", 3.0f); // makes wet clearly distinct from dry
            setParam (*processor, "mix", mixPercent);
            processor->prepareToPlay (sampleRate, 512);

            auto buffer = input;
            processInPlace (*processor, buffer, 512);
            return buffer;
        };

        const auto wet = runWithMix (100.0f);
        const auto half = runWithMix (50.0f);
        const auto dry = runWithMix (0.0f);

        // 0%: the untouched input, sample for sample.
        CHECK (maxAbsDifference (dry, input) < 1.0e-6f);

        // Sanity: the wet path is genuinely different from the dry one.
        CHECK (maxAbsDifference (wet, input) > 0.01f);

        // 50%: exactly halfway between dry and the full wet path.
        juce::AudioBuffer<float> blended (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < numSamples; ++n)
                blended.setSample (ch, n, 0.5f * wet.getSample (ch, n) + 0.5f * input.getSample (ch, n));
        CHECK (maxAbsDifference (half, blended) < 1.0e-5f);
    }

    TEST_CASE ("Bypass is an exact passthrough", "[processor]")
    {
        const double sampleRate = 44100.0;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        auto processor = std::make_unique<CompressorAudioProcessor>();
        setParam (*processor, "threshold", -40.0f); // settings that would compress hard...
        setParam (*processor, "ratio", 20.0f);
        setParam (*processor, "makeupGain", 6.0f);
        setParam (*processor, "bypass", 1.0f); // ...but bypass wins
        processor->prepareToPlay (sampleRate, 512);

        auto buffer = sineBuffer (2, 8192, 440.0, sampleRate, 0.9);
        const juce::AudioBuffer<float> input (buffer);
        processInPlace (*processor, buffer, 512);

        CHECK (maxAbsDifference (buffer, input) == 0.0f);
    }

    TEST_CASE ("Makeup gain is applied to the compressed signal", "[processor]")
    {
        // Ratio 1 turns the compressor into a pure gain stage, so the output
        // must be the input scaled by exactly the makeup amount. The first
        // block ramps from the previous makeup value; check from block 2 on.
        const double sampleRate = 44100.0;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        auto processor = std::make_unique<CompressorAudioProcessor>();
        setParam (*processor, "ratio", 1.0f);
        setParam (*processor, "makeupGain", 6.0f);
        processor->prepareToPlay (sampleRate, 512);

        auto buffer = sineBuffer (2, 2048, 220.0, sampleRate, 0.25);
        const juce::AudioBuffer<float> input (buffer);
        processInPlace (*processor, buffer, 512);

        const float gain = Compressor::dbToAmplitude (processor->makeupGainParameter->load());
        float maxDiff = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 512; n < buffer.getNumSamples(); ++n)
                maxDiff = std::max (maxDiff, std::fabs (buffer.getSample (ch, n)
                                                        - input.getSample (ch, n) * gain));
        CHECK (maxDiff < 1.0e-5f);
    }

    //==========================================================================
    // Group C: robustness & invariance
    //==========================================================================

    TEST_CASE ("Attack timing is sample-rate invariant", "[robustness]")
    {
        // The labeled attack is in milliseconds; the measured time must not
        // depend on the sample rate (an ms/samples mix-up would show here).
        auto ji = juce::ScopedJuceInitialiser_GUI();
        const float attackMs = 50.0f;
        const float predictedMs = predictedAttackCrossing (attackMs);

        float minMs = 1.0e9f, maxMs = 0.0f;
        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            const int crossing = measureAttackCrossing (sampleRate, attackMs);
            REQUIRE (crossing > 0);
            const float measuredMs = (float) crossing * 1000.0f / (float) sampleRate;

            CHECK (measuredMs > 0.8f * predictedMs);
            CHECK (measuredMs < 1.2f * predictedMs);

            minMs = std::min (minMs, measuredMs);
            maxMs = std::max (maxMs, measuredMs);
        }
        CHECK (maxMs / minMs < 1.1f);
    }

    TEST_CASE ("Output is invariant to the processing block size", "[robustness]")
    {
        const double sampleRate = 44100.0;
        const int numSamples = 44100;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        // Amplitude-modulated sine, so the compressor is actively working.
        juce::AudioBuffer<float> input (2, numSamples);
        for (int n = 0; n < numSamples; ++n)
        {
            const double t = (double) n / sampleRate;
            const double amp = 0.2 + 0.6 * std::abs (std::sin (juce::MathConstants<double>::twoPi * 3.0 * t));
            const float v = (float) (amp * std::sin (juce::MathConstants<double>::twoPi * 220.0 * t));
            input.setSample (0, n, v);
            input.setSample (1, n, v);
        }

        auto runWithBlockSize = [&] (int blockSize)
        {
            auto processor = std::make_unique<CompressorAudioProcessor>();
            setParam (*processor, "threshold", -30.0f);
            setParam (*processor, "ratio", 8.0f);
            setParam (*processor, "attack", 5.0f);
            setParam (*processor, "release", 80.0f);
            processor->prepareToPlay (sampleRate, blockSize);

            auto buffer = input;
            processInPlace (*processor, buffer, blockSize);
            return buffer;
        };

        const auto small = runWithBlockSize (32);
        const auto odd = runWithBlockSize (441);
        const auto large = runWithBlockSize (4096);

        CHECK (maxAbsDifference (small, large) < 1.0e-6f);
        CHECK (maxAbsDifference (odd, large) < 1.0e-6f);
    }

    TEST_CASE ("Silence in, silence out - including after a loud passage", "[robustness]")
    {
        const double sampleRate = 44100.0;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        auto processor = std::make_unique<CompressorAudioProcessor>();
        setParam (*processor, "threshold", -40.0f);
        setParam (*processor, "ratio", 10.0f);
        processor->prepareToPlay (sampleRate, 512);

        // Pure silence.
        auto silence = constantBuffer (2, 22050, 0.0f);
        processInPlace (*processor, silence, 512);
        CHECK (silence.getMagnitude (0, silence.getNumSamples()) == 0.0f);

        // Loud passage, then silence: the tail must be exactly zero while the
        // envelope decays through subnormal territory.
        juce::AudioBuffer<float> buffer (2, 44100);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 44100; ++n)
                buffer.setSample (ch, n, n < 8820
                    ? (float) (0.9 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * n / sampleRate))
                    : 0.0f);
        processInPlace (*processor, buffer, 512);
        CHECK (buffer.getMagnitude (8820, 44100 - 8820) == 0.0f);
    }

    TEST_CASE ("Hot input stays finite and bounded", "[robustness]")
    {
        // Float busses routinely exceed 0 dBFS; +12 dBFS in must not produce
        // NaN/Inf or runaway levels in either topology.
        const double sampleRate = 44100.0;
        auto ji = juce::ScopedJuceInitialiser_GUI();

        for (float topology : { 0.0f, 1.0f })
        {
            INFO ("topology " << topology);
            auto processor = std::make_unique<CompressorAudioProcessor>();
            setParam (*processor, "threshold", -18.0f);
            setParam (*processor, "ratio", 20.0f);
            setParam (*processor, "topology", topology);
            processor->prepareToPlay (sampleRate, 512);

            auto buffer = sineBuffer (2, 22050, 1000.0, sampleRate, 4.0); // +12 dBFS
            processInPlace (*processor, buffer, 512);

            bool allFinite = true;
            float maxAbs = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int n = 0; n < buffer.getNumSamples(); ++n)
                {
                    const float v = buffer.getSample (ch, n);
                    allFinite = allFinite && std::isfinite (v);
                    maxAbs = std::max (maxAbs, std::fabs (v));
                }
            }
            CHECK (allFinite);
            CHECK (maxAbs <= 8.0f);
        }
    }

    TEST_CASE ("State save and recall restores every parameter", "[robustness]")
    {
        auto ji = juce::ScopedJuceInitialiser_GUI();
        const char* paramIds[] = { "threshold", "ratio", "attack", "release",
                                   "kneeWidth", "makeupGain", "topology", "detector",
                                   "bypass", "scHpf", "mix", "range" };

        auto source = std::make_unique<CompressorAudioProcessor>();
        setParam (*source, "threshold", -33.5f);
        setParam (*source, "ratio", 7.3f);
        setParam (*source, "attack", 22.2f);
        setParam (*source, "release", 333.0f);
        setParam (*source, "kneeWidth", 4.5f);
        setParam (*source, "makeupGain", -3.3f);
        setParam (*source, "topology", 1.0f);
        setParam (*source, "detector", 0.0f);
        setParam (*source, "bypass", 1.0f);
        setParam (*source, "scHpf", 2.0f);
        setParam (*source, "mix", 42.0f);
        setParam (*source, "range", 12.5f);

        juce::MemoryBlock state;
        source->getStateInformation (state);

        auto restored = std::make_unique<CompressorAudioProcessor>();
        restored->setStateInformation (state.getData(), (int) state.getSize());

        for (const char* id : paramIds)
        {
            INFO (id);
            // Compare against what the source actually stored (ranges snap
            // to their step grids), not the raw requested value.
            const float expected = source->getValueTreeState().getRawParameterValue (id)->load();
            const float actual = restored->getValueTreeState().getRawParameterValue (id)->load();
            CHECK_THAT (actual, WithinAbs ((double) expected, 1.0e-4));
        }
    }
} // namespace processor_test
