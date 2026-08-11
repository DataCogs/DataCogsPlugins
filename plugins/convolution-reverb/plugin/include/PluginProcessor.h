#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

/**
 * Convolution reverb built on juce::dsp::Convolution, which supplies the
 * hard parts - non-uniform partitioned FFT convolution (Gardner 1995,
 * Wefers 2015), background IR loading, resampling to the session rate and
 * loudness normalisation - leaving this processor to own the musical
 * plumbing around it:
 *
 *   input -> [dry path] ------------------------------------.
 *         -> [convolution -> pre-delay -> low cut -> damping] -> equal-power
 *                                                              mix -> gain
 *
 * All wet-path stages are LTI, so their order is mathematically irrelevant;
 * convolution runs first because it is the only stage with internal
 * latency bookkeeping. The default Convolution engine is the zero-latency
 * (uniform head partition) variant, so setLatencySamples() reports 0 - the
 * honest number. Zero-latency costs more CPU than a fixed-latency engine;
 * that trade can be revisited if CPU becomes a problem.
 */
class ConvolutionReverbAudioProcessor : public juce::AudioProcessor,
                                        private juce::AudioProcessorValueTreeState::Listener,
                                        private juce::AsyncUpdater,
                                        private juce::Timer
{
public:
    //==============================================================================
    ConvolutionReverbAudioProcessor();
    ~ConvolutionReverbAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    // The host uses this to know how long to keep pulling audio after the
    // input stops - i.e. the reverb tail: IR length plus pre-delay.
    double getTailLengthSeconds() const override;

    //==============================================================================
    // Factory presets, exposed through JUCE's program API so hosts can list
    // them, and driven by the editor's preset box. A preset is a parameter
    // snapshot applied through the APVTS (so automation, undo and state
    // save/recall all see the change like any other edit) plus an optional
    // library IR, referenced relative to the IR library folder and skipped
    // gracefully when the file isn't installed.
    struct Preset
    {
        const char* name;
        float mix, preDelay, size, decay, early, tail, width, lowCut, highCut, outGain;
        bool reverse;
        const char* irRelPath; // "" = built-in hall
    };
    static const std::vector<Preset>& getFactoryPresets();

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    /// The user's own IR library (also where the favourites file lives).
    /// Always writable; personal captures and favourites go here.
    static juce::File getIrLibraryFolder();

    /// All library roots, in shadowing order: the user's own library first,
    /// then the system-wide one the suite installer writes. A file with the
    /// same relative path in both is taken from the user library.
    static std::vector<juce::File> getIrLibraryRoots();

    /// Resolves a library-relative path (preset IR references, favourites)
    /// against the roots; returns the first existing match, or the path
    /// under the user library if none exists (so errors name a sane place).
    static juce::File resolveIrLibraryFile (const juce::String& relPath);

    /// Sets every parameter (including Reverse and Bypass) back to its
    /// default, through the host-visible path. The loaded IR is left alone -
    /// this is "make the dials sane again", not "unload my room".
    void resetParametersToDefaults();

    // Handing the host a bypass parameter lets it drive bypass sample-
    // accurately and show it in its own UI; we honor it in processBlock.
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // IR management. All of these hand the data to juce::dsp::Convolution,
    // which copies/queues it and finishes loading on its own background
    // thread - safe to call from the message thread while audio runs; the
    // engine swap happens atomically inside process().

    /// Load an IR from an audio file (wav/aiff/flac). Returns false if the
    /// file can't be opened as audio. Trims leading/trailing silence and
    /// normalises so IRs of wildly different lengths land at comparable
    /// loudness.
    bool loadImpulseResponseFile (const juce::File& file);

    /// Regenerate and load the built-in synthetic hall (Moorer-style
    /// exponentially decaying noise - see IRGenerator.h).
    void loadBuiltInImpulseResponse();

    /// Raw buffer entry point, used by the built-in IR and by tests (a unit
    /// impulse IR with normalise=false makes the whole wet path an identity,
    /// which pins down the plumbing sample-exactly).
    void loadImpulseResponseBuffer (juce::AudioBuffer<float>&& buffer,
                                    double bufferSampleRate, bool normalise);

    juce::String getCurrentIrName() const { return irName; }
    juce::File getCurrentIrFile() const { return irFile; }

    /// Re-applies Size/Decay to the master IR and reloads the engine,
    /// immediately and synchronously. Normal operation goes through the
    /// debounced parameter-listener path instead; this exists so tests can
    /// exercise the transform pipeline deterministically (no message loop).
    void applyIrTransformsNow() { rebuildAndLoadIr(); }

    //==============================================================================
    // Expose the parameter tree so the editor can attach controls to it.
    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    const float DEFAULT_MIX       = 30.0f;    // % wet
    const float DEFAULT_PREDELAY  = 0.0f;     // ms
    const float DEFAULT_LOW_CUT   = 20.0f;    // Hz (== bypass, see kLowCutBypassHz)
    const float DEFAULT_HIGH_CUT  = 20000.0f; // Hz (== bypass, see kHighCutBypassHz)
    const float DEFAULT_OUT_GAIN  = 0.0f;     // dB
    const float DEFAULT_SIZE      = 100.0f;   // % stretch of the IR
    const float DEFAULT_DECAY     = 100.0f;   // % of the IR's measured RT60
    const float DEFAULT_EARLY     = 0.0f;     // dB, early-reflections region
    const float DEFAULT_TAIL      = 0.0f;     // dB, late-tail region
    const float DEFAULT_WIDTH     = 100.0f;   // % wet stereo width

    std::atomic<float>* mixParameter      = nullptr; // % wet
    std::atomic<float>* preDelayParameter = nullptr; // ms
    std::atomic<float>* lowCutParameter   = nullptr; // Hz, wet path HPF
    std::atomic<float>* highCutParameter  = nullptr; // Hz, wet path LPF (damping)
    std::atomic<float>* outGainParameter  = nullptr; // dB
    std::atomic<float>* sizeParameter     = nullptr; // % IR stretch
    std::atomic<float>* decayParameter    = nullptr; // % RT60 rescale
    std::atomic<float>* earlyParameter    = nullptr; // dB, IR early region
    std::atomic<float>* tailParameter     = nullptr; // dB, IR tail region
    std::atomic<float>* widthParameter    = nullptr; // % wet M/S width
    juce::AudioParameterBool* reverseParameter = nullptr;
    juce::AudioParameterBool* bypassParameter = nullptr;

private:
    //==============================================================================
    // Size/Decay transform the IR itself, which means reloading the
    // convolution engine - an allocating operation that must never run on
    // the audio thread, and shouldn't run per-tick while a knob drags.
    // Chain: parameterChanged (any thread) -> triggerAsyncUpdate ->
    // handleAsyncUpdate (message thread) -> debounce timer -> rebuild.
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;

    /// Stashes a new master IR (the untransformed original) plus its
    /// bookkeeping, then rebuilds. Message thread only.
    void setMasterIr (juce::AudioBuffer<float>&& buffer, double bufferSampleRate,
                      bool normalise, bool trim,
                      const juce::File& sourceFile, const juce::String& name);

    /// Applies Decay (envelope reshape) and Size (sample-rate relabelling -
    /// the engine's resampler does the actual stretch) to a copy of the
    /// master IR and hands it to the convolution engine. Message thread only.
    void rebuildAndLoadIr();

    juce::AudioProcessorValueTreeState parameters;

    juce::dsp::Convolution convolution;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayLine;
    juce::dsp::FirstOrderTPTFilter<float> lowCutFilter;   // highpass on the wet path
    juce::dsp::FirstOrderTPTFilter<float> highCutFilter;  // lowpass ("damping")
    juce::dsp::Gain<float> outputGain;

    // Zipper-noise control on the parameters that modulate per sample.
    juce::SmoothedValue<float> mixSmoothed;             // 0..1
    juce::SmoothedValue<float> preDelaySamplesSmoothed; // samples
    juce::SmoothedValue<float> widthSmoothed;           // side-channel gain

    // Pre-convolution copy for the dry/wet mix, sized once in prepareToPlay
    // - never allocate on the audio thread.
    juce::AudioBuffer<float> wetBuffer;

    double currentSampleRate = 48000.0;
    bool wasBypassed = false;
    int currentProgram = 0;

    // Written on the message thread when an IR loads, read by
    // getTailLengthSeconds (any thread) - hence atomic.
    std::atomic<double> irLengthSeconds { 0.0 };

    // Message-thread-only bookkeeping for the editor and saved state.
    juce::File irFile;
    juce::String irName { "Built-in Hall" };

    // The untransformed IR as loaded, kept so Size/Decay are always applied
    // to the original (re-applying a stretch to an already-stretched IR
    // would compound). Message thread only.
    juce::AudioBuffer<float> masterIr;
    double masterIrRate = 48000.0;
    double masterIrRt60 = 0.0; // Schroeder-measured, cached per IR load
    bool masterIrNormalise = true;
    bool masterIrTrim = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionReverbAudioProcessor)
};
