#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "PluginProcessor.h"
#include "BiquadDesign.h"

#include <cmath>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

double toDb (double magnitude) { return 20.0 * std::log10 (magnitude); }

void setParam (ParametricEQAudioProcessor& proc, const juce::String& id, float value)
{
    auto* param = proc.getValueTreeState().getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

/// Steady-state RMS of a sine through the processor. Warm-up blocks let the
/// filters settle; the measurement then spans 20 blocks (10240 samples) so
/// low frequencies cover enough full cycles - single-block RMS of a 60 Hz
/// tone holds 0.64 of a cycle and wobbles nearly a dB with phase.
double sineRmsThroughProcessor (ParametricEQAudioProcessor& proc, double freq)
{
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    double phase = 0.0;
    const double step = 2.0 * juce::MathConstants<double>::pi * freq / kSampleRate;

    double sum = 0.0;
    int measured = 0;
    constexpr int warmupBlocks = 10, measureBlocks = 20;

    for (int block = 0; block < warmupBlocks + measureBlocks; ++block)
    {
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float v = static_cast<float> (std::sin (phase));
            phase += step;
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        proc.processBlock (buffer, midi);

        if (block >= warmupBlocks)
            for (int i = 0; i < kBlockSize; ++i)
            {
                sum += static_cast<double> (buffer.getSample (0, i)) * buffer.getSample (0, i);
                ++measured;
            }
    }
    return std::sqrt (sum / measured);
}
} // namespace

//==============================================================================
// Filter designs pinned against RBJ cookbook theory via the exact
// magnitude evaluation. These are the calibration tests: if one fails, the
// EQ is musically wrong no matter what the plumbing does.
//==============================================================================
TEST_CASE ("Bell magnitude hits its gain at fc and unity far away")
{
    const auto c = eq::design (eq::FilterType::bell, 1000.0, 12.0, 1.0, kSampleRate);

    REQUIRE (toDb (eq::magnitudeAt (c, 1000.0, kSampleRate)) == Catch::Approx (12.0).margin (0.05));
    REQUIRE (toDb (eq::magnitudeAt (c, 20.0, kSampleRate))   == Catch::Approx (0.0).margin (0.15));
    REQUIRE (toDb (eq::magnitudeAt (c, 20000.0, kSampleRate)) == Catch::Approx (0.0).margin (0.35));

    // RBJ bells are boost/cut symmetric: +12 then -12 at the same fc/Q
    // multiply to exact unity, everywhere.
    const auto cut = eq::design (eq::FilterType::bell, 1000.0, -12.0, 1.0, kSampleRate);
    for (double hz : { 100.0, 500.0, 1000.0, 2000.0, 8000.0 })
        REQUIRE (eq::magnitudeAt (c, hz, kSampleRate) * eq::magnitudeAt (cut, hz, kSampleRate)
                 == Catch::Approx (1.0).margin (1e-9));
}

TEST_CASE ("High-pass is Butterworth at Q=0.707: -3 dB at fc, 12 dB/oct below")
{
    const auto c = eq::design (eq::FilterType::highPass, 200.0, 0.0, 0.7071, kSampleRate);

    REQUIRE (toDb (eq::magnitudeAt (c, 200.0, kSampleRate)) == Catch::Approx (-3.01).margin (0.1));
    // Slope: each octave down loses ~12 dB (asymptotically).
    const double oneOctave  = toDb (eq::magnitudeAt (c, 100.0, kSampleRate));
    const double twoOctaves = toDb (eq::magnitudeAt (c, 50.0, kSampleRate));
    REQUIRE (oneOctave - twoOctaves == Catch::Approx (12.0).margin (1.0));
    // And it's genuinely flat in the passband.
    REQUIRE (toDb (eq::magnitudeAt (c, 4000.0, kSampleRate)) == Catch::Approx (0.0).margin (0.05));
}

TEST_CASE ("Shelves plateau at their gain and return to unity")
{
    const auto low = eq::design (eq::FilterType::lowShelf, 200.0, 6.0, 0.7071, kSampleRate);
    REQUIRE (toDb (eq::magnitudeAt (low, 20.0, kSampleRate))    == Catch::Approx (6.0).margin (0.3));
    REQUIRE (toDb (eq::magnitudeAt (low, 10000.0, kSampleRate)) == Catch::Approx (0.0).margin (0.1));
    // fc is the half-gain point for RBJ shelves.
    REQUIRE (toDb (eq::magnitudeAt (low, 200.0, kSampleRate))   == Catch::Approx (3.0).margin (0.3));

    const auto high = eq::design (eq::FilterType::highShelf, 5000.0, -9.0, 0.7071, kSampleRate);
    REQUIRE (toDb (eq::magnitudeAt (high, 18000.0, kSampleRate)) == Catch::Approx (-9.0).margin (0.5));
    REQUIRE (toDb (eq::magnitudeAt (high, 100.0, kSampleRate))   == Catch::Approx (0.0).margin (0.1));
}

TEST_CASE ("Notch nulls fc and leaves the rest alone")
{
    const auto c = eq::design (eq::FilterType::notch, 1000.0, 0.0, 4.0, kSampleRate);
    REQUIRE (toDb (eq::magnitudeAt (c, 1000.0, kSampleRate)) < -60.0);
    REQUIRE (toDb (eq::magnitudeAt (c, 500.0, kSampleRate))  == Catch::Approx (0.0).margin (0.3));
    REQUIRE (toDb (eq::magnitudeAt (c, 2000.0, kSampleRate)) == Catch::Approx (0.0).margin (0.3));
}

//==============================================================================
// End-to-end through the processor.
//==============================================================================
TEST_CASE ("Default settings are transparent")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ParametricEQAudioProcessor proc;
    proc.prepareToPlay (kSampleRate, kBlockSize);

    // Defaults: HP/LP inactive, four gain bands active at 0 dB. A 0 dB
    // bell/shelf has A=1, which collapses the cookbook maths to b==a: the
    // filter is exact unity, so this must pass sample-exactly.
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::Random random (11);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);

    juce::AudioBuffer<float> expected;
    expected.makeCopyOf (buffer);

    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < kBlockSize; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (expected.getSample (ch, i)).margin (1e-6));
}

TEST_CASE ("A +12 dB bell boosts a sine at fc by 12 dB and spares one far away")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ParametricEQAudioProcessor proc;

    setParam (proc, "band3Freq", 1000.0f);
    setParam (proc, "band3Gain", 12.0f);
    setParam (proc, "band3Q", 1.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    const double atFc = sineRmsThroughProcessor (proc, 1000.0);
    proc.reset();
    const double farAway = sineRmsThroughProcessor (proc, 60.0);

    const double unity = 1.0 / std::sqrt (2.0); // RMS of a full-scale sine
    REQUIRE (toDb (atFc / unity) == Catch::Approx (12.0).margin (0.2));
    REQUIRE (toDb (farAway / unity) == Catch::Approx (0.0).margin (0.2));
}

TEST_CASE ("An active high-pass guts a sub-fc sine")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ParametricEQAudioProcessor proc;

    setParam (proc, "band1Active", 1.0f);
    setParam (proc, "band1Freq", 120.0f); // band 1 defaults to High-Pass type
    proc.prepareToPlay (kSampleRate, kBlockSize);

    const double below = sineRmsThroughProcessor (proc, 30.0); // two octaves under
    const double unity = 1.0 / std::sqrt (2.0);
    REQUIRE (toDb (below / unity) < -20.0);
}

TEST_CASE ("Factory presets apply full band snapshots")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ParametricEQAudioProcessor proc;

    REQUIRE (proc.getNumPrograms() == static_cast<int> (
                 ParametricEQAudioProcessor::getFactoryPresets().size()));

    int telephone = -1;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        if (proc.getProgramName (i) == "Telephone")
            telephone = i;
    REQUIRE (telephone >= 0);

    proc.setCurrentProgram (telephone);
    REQUIRE (proc.getCurrentProgram() == telephone);

    auto& apvts = proc.getValueTreeState();
    // Band 1 becomes a 600 Hz high-pass, band 6 an active 3 kHz low-pass.
    REQUIRE (apvts.getRawParameterValue ("band1Active")->load() > 0.5f);
    REQUIRE (apvts.getRawParameterValue ("band1Freq")->load() == Catch::Approx (600.0f).margin (2.0));
    REQUIRE (apvts.getRawParameterValue ("band6Active")->load() > 0.5f);
    REQUIRE (static_cast<int> (apvts.getRawParameterValue ("band6Type")->load()) == 4);
    REQUIRE (apvts.getRawParameterValue ("band6Freq")->load() == Catch::Approx (3000.0f).margin (10.0));

    // And it audibly band-limits: 100 Hz is gutted, 1.8 kHz passes boosted.
    proc.prepareToPlay (kSampleRate, kBlockSize);
    const double low = sineRmsThroughProcessor (proc, 100.0);
    proc.reset();
    const double mid = sineRmsThroughProcessor (proc, 1800.0);
    const double unity = 1.0 / std::sqrt (2.0);
    REQUIRE (toDb (low / unity) < -25.0);
    REQUIRE (toDb (mid / unity) > 2.0);

    // Default preset restores transparency.
    proc.setCurrentProgram (0);
    REQUIRE (apvts.getRawParameterValue ("band3Gain")->load() == Catch::Approx (0.0f).margin (0.1));
    REQUIRE (apvts.getRawParameterValue ("band1Active")->load() < 0.5f);
}

TEST_CASE ("Bypass passes audio through untouched and reset restores defaults")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    ParametricEQAudioProcessor proc;

    setParam (proc, "band3Gain", 18.0f);
    setParam (proc, "bypass", 1.0f);
    proc.prepareToPlay (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::Random random (5);
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

    proc.resetParametersToDefaults();
    REQUIRE (proc.getValueTreeState().getRawParameterValue ("band3Gain")->load()
             == Catch::Approx (0.0f).margin (0.1));
    REQUIRE_FALSE (proc.bypassParameter->get());
}
