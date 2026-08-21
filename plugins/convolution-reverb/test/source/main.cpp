#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"
#include "IRGenerator.h"
#include "IRTools.h"

#include <cmath>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

void setParam (ConvolutionReverbAudioProcessor& proc, const char* id, float value)
{
    auto* param = proc.getValueTreeState().getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

/// juce::dsp::Convolution loads IRs on a background thread and swaps the
/// engine in atomically during process(). Tests therefore poll: run the
/// probe, check the expectation, and if the old engine was still active,
/// sleep and retry. `probe` must reset the processor itself if it needs a
/// clean slate.
bool retryUntil (const std::function<bool()>& probe, int attempts = 200, int sleepMs = 10)
{
    for (int i = 0; i < attempts; ++i)
    {
        if (probe())
            return true;
        juce::Thread::sleep (sleepMs);
    }
    return false;
}

/// Loads a single-sample unit impulse as the IR (unnormalised). Convolution
/// with a delta is the identity, so with mix=100%, pre-delay 0 and the
/// filters parked at their bypass positions, the whole wet path must pass
/// audio through bit-nearly-exactly - this pins down every piece of
/// plumbing around the convolution engine in one test.
void loadUnitImpulseIr (ConvolutionReverbAudioProcessor& proc)
{
    juce::AudioBuffer<float> delta (2, 1);
    delta.setSample (0, 0, 1.0f);
    delta.setSample (1, 0, 1.0f);
    proc.loadImpulseResponseBuffer (std::move (delta), kSampleRate, false);
}
} // namespace

//==============================================================================
TEST_CASE ("Parameters exist with documented defaults")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    REQUIRE (proc.mixParameter->load()      == Catch::Approx (30.0f));
    REQUIRE (proc.preDelayParameter->load() == Catch::Approx (0.0f));
    REQUIRE (proc.lowCutParameter->load()   == Catch::Approx (20.0f));
    REQUIRE (proc.highCutParameter->load()  == Catch::Approx (20000.0f));
    // Approx's default epsilon is relative, which is meaningless against an
    // expected value of 0 - and the -24..+12 dB range doesn't round-trip
    // 0 dB exactly through the normalised representation (comes back ~1e-6).
    REQUIRE (proc.outGainParameter->load()  == Catch::Approx (0.0f).margin (1e-3));
    REQUIRE (proc.sizeParameter->load()     == Catch::Approx (100.0f));
    REQUIRE (proc.decayParameter->load()    == Catch::Approx (100.0f));
    REQUIRE (proc.bypassParameter != nullptr);
    REQUIRE_FALSE (proc.bypassParameter->get());

    // The built-in IR must be loaded from construction so the plugin makes
    // sound out of the box, and the tail must reflect its length.
    REQUIRE (proc.getTailLengthSeconds() > 2.0);
    REQUIRE (proc.getCurrentIrName() == "Built-in Hall");
}

//==============================================================================
TEST_CASE ("Unit impulse IR makes the wet path an identity")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    setParam (proc, "mix", 100.0f); // fully wet: output should equal conv(x, delta) = x
    proc.prepareToPlay (kSampleRate, kBlockSize);
    loadUnitImpulseIr (proc);

    juce::MidiBuffer midi;

    const bool converged = retryUntil ([&]
    {
        proc.reset();
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);
        buffer.setSample (1, 0, 1.0f);
        proc.processBlock (buffer, midi);
        // Old (built-in hall) engine smears the impulse; the delta engine
        // returns it intact.
        return std::abs (buffer.getSample (0, 0) - 1.0f) < 1e-3f;
    });
    REQUIRE (converged);

    // Now assert identity properly on a deterministic pseudo-random block.
    proc.reset();
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::Random random (42);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> expected;
    expected.makeCopyOf (buffer);

    proc.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (expected.getSample (ch, i)).margin (1e-3));
}

//==============================================================================
TEST_CASE ("Pre-delay shifts the wet signal by the expected sample count")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    constexpr float preDelayMs = 100.0f;
    constexpr int expectedDelay = static_cast<int> (preDelayMs * 0.001 * kSampleRate); // 4800

    setParam (proc, "mix", 100.0f);
    setParam (proc, "preDelay", preDelayMs);
    proc.prepareToPlay (kSampleRate, kBlockSize);
    loadUnitImpulseIr (proc);

    juce::MidiBuffer midi;
    const int totalBlocks = expectedDelay / kBlockSize + 3;

    const bool found = retryUntil ([&]
    {
        proc.reset();
        std::vector<float> collected;
        collected.reserve (static_cast<size_t> (totalBlocks * kBlockSize));

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                collected.push_back (buffer.getSample (0, i));
        }

        int peakIndex = 0;
        float peak = 0.0f;
        for (size_t i = 0; i < collected.size(); ++i)
            if (std::abs (collected[i]) > peak)
            {
                peak = std::abs (collected[i]);
                peakIndex = static_cast<int> (i);
            }

        // Linear-interpolating delay line: peak lands within a sample of
        // the target and keeps most of its amplitude.
        return peak > 0.5f && std::abs (peakIndex - expectedDelay) <= 1;
    });

    REQUIRE (found);
}

//==============================================================================
TEST_CASE ("Generated IR decays 60 dB over RT60")
{
    ir::BuiltInIrSpec spec;
    spec.rt60Seconds = 1.5f;
    spec.frequencyDependent = false; // damping removes extra HF energy and
                                     // would bias the broadband measurement
    auto channels = ir::generateNoiseTailIr (spec, 2);
    REQUIRE (channels.size() == 2);

    const auto& h = channels[0];
    const int rt60Samples = static_cast<int> (spec.rt60Seconds * spec.sampleRate);
    REQUIRE (static_cast<int> (h.size()) > rt60Samples);

    // RMS over 10 ms windows at t=0 and t=RT60. The envelope is exact but
    // the noise makes any finite window statistical, hence the tolerance.
    const int window = static_cast<int> (0.010 * spec.sampleRate);
    auto rmsAt = [&] (int start)
    {
        double sum = 0.0;
        for (int i = start; i < start + window; ++i)
            sum += static_cast<double> (h[static_cast<size_t> (i)]) * h[static_cast<size_t> (i)];
        return std::sqrt (sum / window);
    };

    const double dbDrop = 20.0 * std::log10 (rmsAt (rt60Samples - window) / rmsAt (0));
    REQUIRE (dbDrop == Catch::Approx (-60.0).margin (4.0));

    // Channels must be decorrelated (different seeds) - normalised cross-
    // correlation at lag 0 should be near zero, nowhere near 1.
    double dot = 0.0, e0 = 0.0, e1 = 0.0;
    for (size_t i = 0; i < h.size(); ++i)
    {
        dot += static_cast<double> (channels[0][i]) * channels[1][i];
        e0 += static_cast<double> (channels[0][i]) * channels[0][i];
        e1 += static_cast<double> (channels[1][i]) * channels[1][i];
    }
    REQUIRE (std::abs (dot / std::sqrt (e0 * e1)) < 0.1);
}

//==============================================================================
TEST_CASE ("Schroeder RT60 measurement recovers the generator's RT60")
{
    ir::BuiltInIrSpec spec;
    spec.rt60Seconds = 1.5f;
    spec.frequencyDependent = false; // damping shortens the broadband decay
    auto channels = ir::generateNoiseTailIr (spec, 1);

    const double measured = ir::measureRt60 (channels[0].data(),
                                             static_cast<int> (channels[0].size()),
                                             spec.sampleRate);

    // T20 fit on exact exponential decay: the residual error is the noise
    // statistics of the finite windows, so a 10% band is comfortable.
    REQUIRE (measured == Catch::Approx (1.5).epsilon (0.10));
}

//==============================================================================
TEST_CASE ("Decay reshape halves the measured RT60")
{
    ir::BuiltInIrSpec spec;
    spec.rt60Seconds = 1.5f;
    spec.frequencyDependent = false;
    auto channels = ir::generateNoiseTailIr (spec, 1);
    const int n = static_cast<int> (channels[0].size());

    const double before = ir::measureRt60 (channels[0].data(), n, spec.sampleRate);
    REQUIRE (before > 0.0);

    ir::reshapeDecay (channels[0].data(), n, spec.sampleRate, before, 0.5);

    const double after = ir::measureRt60 (channels[0].data(), n, spec.sampleRate);
    REQUIRE (after == Catch::Approx (before * 0.5).epsilon (0.10));
}

//==============================================================================
TEST_CASE ("Size 200% doubles the arrival time of a delayed delta IR")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    // IR = delta at sample 4800 (100 ms at 48k). With Size 200% the engine
    // is told the IR was recorded at 24k, so its resampler stretches it to
    // 200 ms - the echo must arrive at ~9600 samples. This exercises the
    // whole rebuild pipeline: master copy, rate relabelling, reload.
    constexpr int deltaPos = 4800;
    setParam (proc, "mix", 100.0f);
    setParam (proc, "size", 200.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> irBuf (2, deltaPos + 1);
    irBuf.clear();
    irBuf.setSample (0, deltaPos, 1.0f);
    irBuf.setSample (1, deltaPos, 1.0f);
    proc.loadImpulseResponseBuffer (std::move (irBuf), kSampleRate, false);

    constexpr int expectedArrival = deltaPos * 2;
    juce::MidiBuffer midi;
    const int totalBlocks = expectedArrival / kBlockSize + 4;

    const bool found = retryUntil ([&]
    {
        proc.reset();
        std::vector<float> collected;
        collected.reserve (static_cast<size_t> (totalBlocks * kBlockSize));

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                collected.push_back (buffer.getSample (0, i));
        }

        int peakIndex = 0;
        float peak = 0.0f;
        for (size_t i = 0; i < collected.size(); ++i)
            if (std::abs (collected[i]) > peak)
            {
                peak = std::abs (collected[i]);
                peakIndex = static_cast<int> (i);
            }

        // The engine's resampler smears a delta into a windowed sinc, so
        // allow a generous position band and a modest amplitude floor.
        return peak > 0.2f && std::abs (peakIndex - expectedArrival) <= 32;
    });

    REQUIRE (found);
}

//==============================================================================
TEST_CASE ("Early/Tail gains shape the expected IR regions")
{
    constexpr double fs = 48000.0;
    constexpr int n = 48000;
    std::vector<float> h (n, 1.0f); // constant "IR": onset at sample 0

    // Tail off, early untouched: everything past boundary+fade must be
    // exactly silent, everything before the boundary exactly untouched.
    ir::applyEarlyTailGains (h.data(), n, fs, 1.0f, 0.0f);

    const int boundary = static_cast<int> (ir::kEarlyWindowSeconds * fs); // 3840
    const int fadeEnd  = boundary + static_cast<int> (ir::kSplitFadeSeconds * fs);

    for (int i = 0; i < boundary; ++i)
        REQUIRE (h[static_cast<size_t> (i)] == 1.0f);
    for (int i = fadeEnd; i < n; ++i)
        REQUIRE (h[static_cast<size_t> (i)] == 0.0f);
    // And the crossfade is monotonic - no discontinuity spliced in.
    for (int i = boundary; i < fadeEnd - 1; ++i)
        REQUIRE (h[static_cast<size_t> (i + 1)] <= h[static_cast<size_t> (i)] + 1e-6f);
}

//==============================================================================
TEST_CASE ("Width 0 collapses the wet field to mono")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    setParam (proc, "mix", 100.0f);
    setParam (proc, "width", 0.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);
    loadUnitImpulseIr (proc);

    juce::MidiBuffer midi;

    // A left-only impulse through the identity IR is maximally "side"; with
    // width 0 both output channels must carry the mid signal, 0.5 each.
    const bool converged = retryUntil ([&]
    {
        proc.reset();
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f); // left only
        proc.processBlock (buffer, midi);
        return std::abs (buffer.getSample (0, 0) - 0.5f) < 1e-3f
            && std::abs (buffer.getSample (1, 0) - 0.5f) < 1e-3f;
    });
    REQUIRE (converged);
}

//==============================================================================
TEST_CASE ("Reverse mirrors the IR in time")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    // IR: delta at sample 0 in a 4800-sample buffer. Reversed, the delta
    // sits at sample 4799, so an input impulse must arrive ~100 ms late.
    constexpr int irLen = 4800;
    setParam (proc, "mix", 100.0f);
    setParam (proc, "reverse", 1.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> irBuf (2, irLen);
    irBuf.clear();
    irBuf.setSample (0, 0, 1.0f);
    irBuf.setSample (1, 0, 1.0f);
    proc.loadImpulseResponseBuffer (std::move (irBuf), kSampleRate, false);

    constexpr int expectedArrival = irLen - 1;
    juce::MidiBuffer midi;
    const int totalBlocks = expectedArrival / kBlockSize + 3;

    const bool found = retryUntil ([&]
    {
        proc.reset();
        std::vector<float> collected;
        collected.reserve (static_cast<size_t> (totalBlocks * kBlockSize));

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                collected.push_back (buffer.getSample (0, i));
        }

        int peakIndex = 0;
        float peak = 0.0f;
        for (size_t i = 0; i < collected.size(); ++i)
            if (std::abs (collected[i]) > peak)
            {
                peak = std::abs (collected[i]);
                peakIndex = static_cast<int> (i);
            }

        return peak > 0.5f && std::abs (peakIndex - expectedArrival) <= 1;
    });

    REQUIRE (found);
}

//==============================================================================
namespace
{
/// The definitive end-to-end check: convolution with a Dirac delta is the
/// identity *of the IR* - feed the plugin a single unit sample and the
/// output must be the impulse response itself, sample for sample. Run
/// against real measured rooms (repo fixtures, see test/fixtures/README.md)
/// this exercises the whole pipeline - buffer load, engine, wet path - with
/// nothing synthetic about the data.
void runDiracGoldenTest (const juce::File& fixture)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (fixture));
    REQUIRE (reader != nullptr);

    const int n = static_cast<int> (reader->lengthInSamples);
    const double fileRate = reader->sampleRate;
    juce::AudioBuffer<float> h (static_cast<int> (reader->numChannels), n);
    REQUIRE (reader->read (&h, 0, n, 0, true, true));
    const int numCh = juce::jmin (2, h.getNumChannels());

    ConvolutionReverbAudioProcessor proc;
    setParam (proc, "mix", 100.0f); // fully wet; all other defaults are transparent

    // Prepared at the file's own rate so the engine does no resampling and
    // the comparison can be sample-exact.
    proc.prepareToPlay (fileRate, kBlockSize);

    {
        juce::AudioBuffer<float> copy;
        copy.makeCopyOf (h);
        proc.loadImpulseResponseBuffer (std::move (copy), fileRate, false /* no normalise */);
    }

    juce::MidiBuffer midi;
    const int totalBlocks = n / kBlockSize + 2;
    std::vector<std::vector<float>> collected (2);

    auto renderDirac = [&]
    {
        proc.reset();
        for (auto& c : collected)
        {
            c.clear();
            c.reserve (static_cast<size_t> (totalBlocks * kBlockSize));
        }
        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, 0, 1.0f); // the Dirac
            proc.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlockSize; ++i)
                    collected[static_cast<size_t> (ch)].push_back (buffer.getSample (ch, i));
        }
    };

    // Wait out the background IR swap: normalised correlation of the first
    // few blocks against the file flips from "some other room" to ~1.
    const bool converged = retryUntil ([&]
    {
        renderDirac();
        const int m = juce::jmin (n, 4 * kBlockSize);
        double dot = 0.0, outEnergy = 0.0, irEnergy = 0.0;
        for (int i = 0; i < m; ++i)
        {
            const double o = collected[0][static_cast<size_t> (i)];
            const double e = h.getSample (0, i);
            dot += o * e;
            outEnergy += o * o;
            irEnergy += e * e;
        }
        return outEnergy > 0.0 && irEnergy > 0.0
            && dot / std::sqrt (outEnergy * irEnergy) > 0.99;
    });
    REQUIRE (converged);

    // The strict assertion: every sample of every channel matches the file,
    // within single-precision partitioned-FFT error (measured ~1e-5 for
    // IRs this long; 2e-3 leaves margin while still catching any scaling,
    // channel-swap or time-alignment bug).
    for (int ch = 0; ch < numCh; ++ch)
    {
        float maxError = 0.0f;
        for (int i = 0; i < n; ++i)
            maxError = juce::jmax (maxError,
                                   std::abs (collected[static_cast<size_t> (ch)][static_cast<size_t> (i)]
                                             - h.getSample (ch, i)));
        INFO (fixture.getFileName() << " channel " << ch << " max error " << maxError);
        REQUIRE (maxError < 2e-3f);
    }
}
} // namespace

TEST_CASE ("Dirac impulse reproduces real measured IRs sample-exactly")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // ctest runs with WORKING_DIRECTORY = test/ (see catch_discover_tests).
    const auto fixtures = juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures");
    runDiracGoldenTest (fixtures.getChildFile ("Usina del Arte Symphony Hall.wav"));
    runDiracGoldenTest (fixtures.getChildFile ("Lady Chapel St Albans.wav"));
}

//==============================================================================
TEST_CASE ("Factory presets apply through the program API")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    REQUIRE (proc.getNumPrograms() == static_cast<int> (
                 ConvolutionReverbAudioProcessor::getFactoryPresets().size()));

    // "Reverse Swell" uses the built-in IR, so it applies fully on any
    // machine regardless of which IR packs are installed.
    int reverseSwell = -1;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        if (proc.getProgramName (i) == "Reverse Swell")
            reverseSwell = i;
    REQUIRE (reverseSwell >= 0);

    proc.setCurrentProgram (reverseSwell);

    REQUIRE (proc.getCurrentProgram() == reverseSwell);
    REQUIRE (proc.mixParameter->load() == Catch::Approx (45.0f).margin (0.5));
    REQUIRE (proc.preDelayParameter->load() == Catch::Approx (100.0f).margin (0.5));
    REQUIRE (proc.reverseParameter->get());
    REQUIRE (proc.getCurrentIrName() == "Built-in Hall");

    // And back: "Default" restores the documented defaults.
    proc.setCurrentProgram (0);
    REQUIRE (proc.mixParameter->load() == Catch::Approx (30.0f).margin (0.5));
    REQUIRE_FALSE (proc.reverseParameter->get());
}

//==============================================================================
TEST_CASE ("Reset to defaults restores every parameter")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    // Mangle a representative spread, including both bools.
    setParam (proc, "mix", 87.0f);
    setParam (proc, "preDelay", 140.0f);
    setParam (proc, "size", 173.0f);
    setParam (proc, "decay", 40.0f);
    setParam (proc, "early", -22.0f);
    setParam (proc, "tail", 6.0f);
    setParam (proc, "width", 15.0f);
    setParam (proc, "lowCut", 400.0f);
    setParam (proc, "highCut", 3000.0f);
    setParam (proc, "outGain", -12.0f);
    setParam (proc, "reverse", 1.0f);
    setParam (proc, "bypass", 1.0f);

    proc.resetParametersToDefaults();

    REQUIRE (proc.mixParameter->load()      == Catch::Approx (30.0f).margin (0.5));
    REQUIRE (proc.preDelayParameter->load() == Catch::Approx (0.0f).margin (0.5));
    REQUIRE (proc.sizeParameter->load()     == Catch::Approx (100.0f).margin (0.5));
    REQUIRE (proc.decayParameter->load()    == Catch::Approx (100.0f).margin (0.5));
    REQUIRE (proc.earlyParameter->load()    == Catch::Approx (0.0f).margin (0.5));
    REQUIRE (proc.tailParameter->load()     == Catch::Approx (0.0f).margin (0.5));
    REQUIRE (proc.widthParameter->load()    == Catch::Approx (100.0f).margin (0.5));
    REQUIRE (proc.lowCutParameter->load()   == Catch::Approx (20.0f).margin (0.5));
    REQUIRE (proc.highCutParameter->load()  == Catch::Approx (20000.0f).margin (1.0));
    REQUIRE (proc.outGainParameter->load()  == Catch::Approx (0.0f).margin (0.1));
    REQUIRE_FALSE (proc.reverseParameter->get());
    REQUIRE_FALSE (proc.bypassParameter->get());
}

//==============================================================================
namespace
{
/// Scale-invariant, shift-tolerant similarity: the peak normalised
/// cross-correlation over lags [0, maxLag). Shift tolerance matters because
/// the engine trims leading silence from file-loaded IRs; scale invariance
/// because it normalises loudness. Same room -> ~1 at some lag; different
/// rooms are noise-like and stay near 0 at every lag.
double peakCrossCorrelation (const std::vector<float>& a, const std::vector<float>& b,
                             int compareLength, int maxLag)
{
    double best = 0.0;
    for (int lag = 0; lag < maxLag; ++lag)
    {
        double dot = 0.0, ea = 0.0, eb = 0.0;
        for (int i = 0; i < compareLength; ++i)
        {
            const double x = a[static_cast<size_t> (i)];
            const double y = b[static_cast<size_t> (i + lag)];
            dot += x * y;
            ea += x * x;
            eb += y * y;
        }
        if (ea > 0.0 && eb > 0.0)
            best = juce::jmax (best, std::abs (dot) / std::sqrt (ea * eb));
    }
    return best;
}
} // namespace

TEST_CASE ("Switching IR files changes the rendered response")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    // This is exactly what the editor's IR picker does per arrow step:
    // loadImpulseResponseFile(), nothing else. If two different rooms come
    // out sounding identical, the bug is in this pipeline; if this test is
    // green, the engine is fine and any "all rooms sound the same" report
    // is environmental (stale host cache, bypass engaged, etc).
    const auto fixtures = juce::File::getCurrentWorkingDirectory().getChildFile ("fixtures");
    const auto roomA = fixtures.getChildFile ("Usina del Arte Symphony Hall.wav");
    const auto roomB = fixtures.getChildFile ("Lady Chapel St Albans.wav");

    setParam (proc, "mix", 100.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::MidiBuffer midi;
    constexpr int renderLength = 48000; // 1 s of response is plenty to fingerprint a room
    const int totalBlocks = renderLength / kBlockSize + 1;

    auto renderDirac = [&] (std::vector<float>& out)
    {
        proc.reset();
        out.clear();
        out.reserve (static_cast<size_t> (totalBlocks * kBlockSize));
        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (buffer.getSample (0, i));
        }
    };

    // Reference: room A's own samples, for the convergence probe.
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (roomA));
    REQUIRE (reader != nullptr);
    std::vector<float> fileA (static_cast<size_t> (renderLength + 4800), 0.0f);
    {
        juce::AudioBuffer<float> tmp (static_cast<int> (reader->numChannels),
                                      juce::jmin ((int) reader->lengthInSamples, renderLength + 4800));
        REQUIRE (reader->read (&tmp, 0, tmp.getNumSamples(), 0, true, true));
        for (int i = 0; i < tmp.getNumSamples(); ++i)
            fileA[static_cast<size_t> (i)] = tmp.getSample (0, i);
    }

    // Load room A and wait until the render matches the file (the load is
    // asynchronous; trim may shift it by up to the lag window).
    REQUIRE (proc.loadImpulseResponseFile (roomA));
    std::vector<float> outA;
    const bool convergedA = retryUntil ([&]
    {
        renderDirac (outA);
        return peakCrossCorrelation (outA, fileA, 24000, 4800) > 0.9;
    });
    REQUIRE (convergedA);

    // "Arrow" to room B: the render must stop resembling room A.
    REQUIRE (proc.loadImpulseResponseFile (roomB));
    std::vector<float> outB;
    const bool changed = retryUntil ([&]
    {
        renderDirac (outB);
        double energy = 0.0;
        for (float v : outB)
            energy += static_cast<double> (v) * v;
        if (energy <= 1e-6)
            return false; // still silent / swapping
        std::vector<float> padded (outB);
        padded.resize (outB.size() + 4800, 0.0f);
        return peakCrossCorrelation (outA, padded, 24000, 4800) < 0.5;
    });
    REQUIRE (changed);
}

//==============================================================================
TEST_CASE ("Normalised IR plays the wet path at unity loudness")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    // Regression for "every IR sounds quiet and alike": JUCE's
    // Normalise::yes scales each IR to a total energy of 0.125^2, which put
    // the whole wet path ~18 dB under the dry signal until the wet-path
    // makeup gain compensated it back to unit energy. For decorrelated
    // noise-like input, steady-state wet RMS ~= input RMS * sqrt(IR energy),
    // so at 100% mix the output must sit near the input level.
    const auto fixture = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("fixtures")
                             .getChildFile ("Lady Chapel St Albans.wav");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (fixture));
    REQUIRE (reader != nullptr);
    const double fileRate = reader->sampleRate;
    const int fileLen = static_cast<int> (reader->lengthInSamples);
    juce::AudioBuffer<float> h (static_cast<int> (reader->numChannels), fileLen);
    REQUIRE (reader->read (&h, 0, fileLen, 0, true, true));
    std::vector<float> fileSamples (static_cast<size_t> (fileLen) + 9600, 0.0f);
    for (int i = 0; i < fileLen; ++i)
        fileSamples[static_cast<size_t> (i)] = h.getSample (0, i);

    setParam (proc, "mix", 100.0f);
    // At the file's own rate the engine skips resampling; normalisation
    // happens either way, so this only removes an unrelated variable.
    proc.prepareToPlay (fileRate, kBlockSize);
    REQUIRE (proc.loadImpulseResponseFile (fixture)); // the normalising path

    juce::MidiBuffer midi;

    // Wait out the async engine swap: the Dirac render must correlate with
    // this room's own samples (scale-invariant, trim-tolerant).
    const int renderLen = static_cast<int> (fileRate);
    const int totalBlocks = renderLen / kBlockSize + 1;
    std::vector<float> out;
    const bool converged = retryUntil ([&]
    {
        proc.reset();
        out.clear();
        out.reserve (static_cast<size_t> (totalBlocks * kBlockSize));
        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::AudioBuffer<float> buffer (2, kBlockSize);
            buffer.clear();
            if (b == 0)
            {
                buffer.setSample (0, 0, 1.0f);
                buffer.setSample (1, 0, 1.0f);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < kBlockSize; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        return peakCrossCorrelation (out, fileSamples,
                                     juce::jmin (fileLen - 9600, renderLen / 2), 9600) > 0.9;
    });
    REQUIRE (converged);

    // 3 s of deterministic noise; compare output vs input power over the
    // final second, well past the reverb's build-up.
    juce::Random rng (42);
    const int noiseBlocks = static_cast<int> (3.0 * fileRate) / kBlockSize;
    double outSum = 0.0, inSum = 0.0;
    proc.reset();
    for (int b = 0; b < noiseBlocks; ++b)
    {
        juce::AudioBuffer<float> buffer (2, kBlockSize);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlockSize; ++i)
                buffer.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);
        juce::AudioBuffer<float> input;
        input.makeCopyOf (buffer);
        proc.processBlock (buffer, midi);
        if (b > 2 * noiseBlocks / 3)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < kBlockSize; ++i)
                {
                    outSum += static_cast<double> (buffer.getSample (ch, i)) * buffer.getSample (ch, i);
                    inSum  += static_cast<double> (input.getSample (ch, i)) * input.getSample (ch, i);
                }
    }

    const double wetDb = 10.0 * std::log10 (outSum / inSum);
    INFO ("steady-state wet level vs dry input: " << wetDb << " dB");
    // Unit IR energy is the target; the band allows for trim, the split of
    // energy between channels and finite-window statistics - while staying
    // far above the -18 dB failure this test locks out.
    REQUIRE (wetDb > -4.0);
    REQUIRE (wetDb < 2.0);
}

//==============================================================================
TEST_CASE ("Bypass passes audio through untouched")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ConvolutionReverbAudioProcessor proc;

    setParam (proc, "bypass", 1.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::Random random (7);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> expected;
    expected.makeCopyOf (buffer);

    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            REQUIRE (buffer.getSample (ch, i) == expected.getSample (ch, i));
}
