#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "BiquadDesign.h"
#include "SpectrumAnalyzer.h"

#include <array>
#include <atomic>

/**
 * Six-band parametric EQ. Every band is one RBJ biquad (see BiquadDesign.h)
 * and can be any of Bell / Low Shelf / High Shelf / High-Pass / Low-Pass /
 * Notch, so the classic channel chain - HP, low shelf, two bells, high
 * shelf, LP - fits with a band to spare.
 *
 * Coefficients are recomputed once per block straight from the parameter
 * atomics: six designs' worth of trig per block is measurement-noise
 * against the filtering itself, and it makes automation sample-block
 * accurate with no listener/message-thread machinery at all. Band state is
 * reset only when a band's *type* changes or it toggles active - a
 * discontinuity is unavoidable there anyway - never for freq/gain/Q moves,
 * which glide.
 */
class ParametricEQAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 6;

    //==============================================================================
    ParametricEQAudioProcessor();
    ~ParametricEQAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    // Factory presets via the program API (hosts list them natively) and
    // the editor's preset box. Each is a full six-band snapshot applied
    // through the APVTS like any user edit.
    struct BandPreset { bool active; int type; float freq, gain, q; };
    struct Preset
    {
        const char* name;
        BandPreset bands[numBands];
        float outGain;
    };
    static const std::vector<Preset>& getFactoryPresets();

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /// Parameter IDs are "band1Active", "band1Type", "band1Freq",
    /// "band1Gain", "band1Q" ... "band6Q" - this builds them.
    static juce::String bandParamID (int bandIndex, const char* suffix);

    /// The band's current design, for the editor's response curve. Reads
    /// the same atomics the audio thread reads, so it's always live.
    eq::BiquadCoefficients getBandCoefficients (int bandIndex, double sampleRate) const;
    bool isBandActive (int bandIndex) const;

    void resetParametersToDefaults();

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }
    SpectrumAnalyzer& getAnalyzer() { return analyzer; }

    std::atomic<float>* outGainParameter = nullptr;
    juce::AudioParameterBool* bypassParameter = nullptr;

private:
    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;

    struct Band
    {
        std::atomic<float>* active = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;

        eq::BiquadCoefficients coefficients;
        std::array<eq::BiquadState, 2> state; // per channel
        int lastType = -1;
        bool wasActive = false;
    };
    std::array<Band, numBands> bands;

    juce::dsp::Gain<float> outputGain;
    SpectrumAnalyzer analyzer;
    double currentSampleRate = 48000.0;
    bool wasBypassed = false;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessor)
};
