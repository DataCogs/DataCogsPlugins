#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Compressor.h"
#include <vector>
//==============================================================================
/**
 * Dynamic range compression implementation based on ideas and code
 * examples from Will Pirkle's book Designing Audio Effect Plugins in C++
 */

class CompressorAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    CompressorAudioProcessor();
    ~CompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    // Factory presets, exposed through JUCE's program API so hosts can list
    // them, and driven by the editor's preset box. A preset is just a
    // parameter snapshot applied through the APVTS (so automation, undo and
    // state save/recall all see the change like any other edit).
    struct Preset
    {
        const char* name;
        float threshold, ratio, attack, release, kneeWidth, makeupGain;
        int detector; // 0 = Peak, 1 = RMS
        int scHpf;    // 0 = Off, 1 = 60 Hz, 2 = 120 Hz, 3 = 250 Hz
        float mix;    // % wet
        float range;  // max gain reduction, dB
    };
    static const std::vector<Preset>& getFactoryPresets();

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    // Handing the host a bypass parameter lets it drive bypass sample-
    // accurately and show it in its own UI; we honor it in processBlock.
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    /// Sets every parameter back to its default through the host-visible
    /// path (drives the editor's Reset button - a DataCogs house-standard
    /// control across the plugin suite).
    void resetParametersToDefaults()
    {
        for (auto* p : getParameters())
            p->setValueNotifyingHost (p->getDefaultValue());
    }

    double samplerate;

    const float DEFAULT_THRESHOLD = -18.0f; // dB
    const float DEFAULT_RATIO = 4.0f;
    const float DEFAULT_ATTACK = 125.0f;    // mSeconds
    const float DEFAULT_RELEASE = 125.0f;   // mSeconds
    const float DEFAULT_KNEE_WIDTH = 1.0f;  // dB
    const float DEFAULT_MAKEUP_GAIN = 0.0f; // dB (the old comment said
                                            // "linear" but the value has
                                            // always been used as dB; a
                                            // compressor should also default
                                            // to not adding gain)
    const float DEFAULT_MIX = 100.0f;       // % wet (100 = fully compressed)
    const float DEFAULT_RANGE = 40.0f;      // dB max GR (40 = effectively off)

    std::unique_ptr<Compressor> compressor;

    // Gain applied at the end of the last processed block, written by the
    // audio thread and polled by the editor's meter timer. Atomic because
    // the two threads never synchronise any other way - the audio thread
    // must never block on the UI, so lock-free hand-off is the rule for
    // anything meters read.
    std::atomic<float> currentGain { 1.0f };

    // Convenience for meters: gain reduction as a positive dB amount.
    float getGainReductionDb() const noexcept
    {
        return -Compressor::amplitudeToDb (currentGain.load (std::memory_order_relaxed));
    }

    // Per-channel block peak levels for the editor's stereo meters, in dB.
    // Same lock-free hand-off pattern as currentGain.
    float getInputPeakDb (int channel) const noexcept
    {
        return Compressor::amplitudeToDb (inputPeak[channel & 1].load (std::memory_order_relaxed));
    }
    float getOutputPeakDb (int channel) const noexcept
    {
        return Compressor::amplitudeToDb (outputPeak[channel & 1].load (std::memory_order_relaxed));
    }

    // Expose the parameter tree so the editor can attach controls to it.
    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    std::atomic<float> *thresholdParameter = nullptr;
    std::atomic<float> *ratioParameter = nullptr;
    std::atomic<float> *attackParameter = nullptr;
    std::atomic<float> *releaseParameter = nullptr;
    std::atomic<float> *kneeWidthParameter = nullptr;
    std::atomic<float> *makeupGainParameter = nullptr;
    std::atomic<float> *topologyParameter = nullptr;  // 0 Classic, 1 Log-Domain
    std::atomic<float> *detectorParameter = nullptr;  // 0 Peak, 1 RMS
    std::atomic<float> *scHpfParameter = nullptr;     // choice index, see kScHpfFrequencies
    std::atomic<float> *mixParameter = nullptr;       // % wet
    std::atomic<float> *rangeParameter = nullptr;     // dB max GR
    juce::AudioParameterBool *bypassParameter = nullptr;

    float previousMakeupGain = 0.0f; // dB, must match DEFAULT_MAKEUP_GAIN

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak[2] { 0.0f, 0.0f };
    std::atomic<float> outputPeak[2] { 0.0f, 0.0f };
    int currentProgram = 0;

    // Pre-compression copy for the wet/dry mix, sized once in
    // prepareToPlay - never allocate on the audio thread.
    juce::AudioBuffer<float> dryBuffer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorAudioProcessor)
};
