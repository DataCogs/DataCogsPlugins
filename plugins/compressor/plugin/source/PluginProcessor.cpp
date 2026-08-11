#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// The sidechain HPF choices; index 0 must stay "Off" (exact bypass).
constexpr float kScHpfFrequencies[] = { 0.0f, 60.0f, 120.0f, 250.0f };

// Attack/Release perception is logarithmic - the difference between 1 ms
// and 5 ms is enormous, between 800 ms and 900 ms negligible - so the
// knobs get a skew that puts the workhorse values near the centre of
// rotation instead of cramming them into the first few degrees. Skew only
// changes the KNOB-to-value mapping (and automation lane shape); the
// values themselves, and any saved state, are unchanged.
//
// Every range also gets a step interval. This matters beyond taste: the
// interval drives AudioParameterFloat's default text formatting (no
// interval = 7 decimal places in every text box and host generic view),
// and the APVTS slider attachments delegate text rendering to the
// parameter, so the parameter is the only place precision can be fixed.
juce::NormalisableRange<float> skewed (float min, float max, float step, float centre)
{
    juce::NormalisableRange<float> range (min, max, step);
    range.setSkewForCentre (centre);
    return range;
}
} // namespace

//==============================================================================
CompressorAudioProcessor::CompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      parameters(*this, nullptr, juce::Identifier("Compressor"),
                 {std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("threshold", 1), "Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), DEFAULT_THRESHOLD),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("ratio", 1), "Ratio", skewed(1.0f, 20.0f, 0.1f, 4.0f), DEFAULT_RATIO),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("attack", 1), "Attack", skewed(0.1f, 1000.0f, 0.1f, 15.0f), DEFAULT_ATTACK),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("release", 1), "Release", skewed(10.0f, 5000.0f, 1.0f, 150.0f), DEFAULT_RELEASE),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("kneeWidth", 1), "Knee Width", juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), DEFAULT_KNEE_WIDTH),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("makeupGain", 1), "Makeup Gain", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), DEFAULT_MAKEUP_GAIN),
                  std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("topology", 1), "Topology", juce::StringArray{"Classic", "Log-Domain"}, 0),
                  std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("detector", 1), "Detector", juce::StringArray{"Peak", "RMS"}, 1),
                  std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false),
                  std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("scHpf", 1), "Sidechain HPF", juce::StringArray{"Off", "60 Hz", "120 Hz", "250 Hz"}, 0),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("mix", 1), "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), DEFAULT_MIX),
                  std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("range", 1), "Range", juce::NormalisableRange<float>(0.0f, 40.0f, 0.5f), DEFAULT_RANGE)})
#endif
{
    thresholdParameter = parameters.getRawParameterValue("threshold");
    ratioParameter = parameters.getRawParameterValue("ratio");
    attackParameter = parameters.getRawParameterValue("attack");
    releaseParameter = parameters.getRawParameterValue("release");
    kneeWidthParameter = parameters.getRawParameterValue("kneeWidth");
    makeupGainParameter = parameters.getRawParameterValue("makeupGain");
    topologyParameter = parameters.getRawParameterValue("topology");
    detectorParameter = parameters.getRawParameterValue("detector");
    bypassParameter = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter("bypass"));
    scHpfParameter = parameters.getRawParameterValue("scHpf");
    mixParameter = parameters.getRawParameterValue("mix");
    rangeParameter = parameters.getRawParameterValue("range");
}

CompressorAudioProcessor::~CompressorAudioProcessor()
{
}

//==============================================================================
const juce::String CompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CompressorAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool CompressorAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool CompressorAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double CompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

const std::vector<CompressorAudioProcessor::Preset>& CompressorAudioProcessor::getFactoryPresets()
{
    // name              thresh  ratio  attack  release  knee  makeup  det  hpf   mix    range
    static const std::vector<Preset> presets = {
        { "Default",      -18.0f,  4.0f, 125.0f,  125.0f, 1.0f,  0.0f,  1,   0, 100.0f, 40.0f },
        { "Vocal Smooth", -22.0f,  3.0f,  15.0f,  180.0f, 6.0f,  4.0f,  1,   0, 100.0f,  8.0f },
        { "Drum Punch",   -15.0f,  5.0f,  30.0f,   90.0f, 2.0f,  3.0f,  0,   1,  70.0f, 40.0f },
        { "Bus Glue",     -20.0f,  2.0f,  30.0f,  250.0f, 8.0f,  2.0f,  1,   2, 100.0f, 40.0f },
        { "Bass Control", -20.0f,  4.0f,   8.0f,  150.0f, 4.0f,  2.0f,  1,   0, 100.0f, 40.0f },
        { "Limiter",       -8.0f, 20.0f,   0.5f,   60.0f, 0.5f,  0.0f,  0,   0, 100.0f, 40.0f },
        { "Master Gentle", -24.0f, 1.8f,  40.0f,  400.0f, 10.0f, 1.0f,  1,   2, 100.0f, 40.0f },
    };
    return presets;
}

int CompressorAudioProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

int CompressorAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void CompressorAudioProcessor::setCurrentProgram(int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    currentProgram = index;
    const auto& p = presets[(size_t) index];

    // Applied through the parameter interface (normalised + notify) so the
    // host sees each change exactly as if the user turned the knob.
    auto apply = [this](const char* paramID, float value)
    {
        if (auto* param = parameters.getParameter(paramID))
            param->setValueNotifyingHost(param->convertTo0to1(value));
    };

    apply("threshold", p.threshold);
    apply("ratio", p.ratio);
    apply("attack", p.attack);
    apply("release", p.release);
    apply("kneeWidth", p.kneeWidth);
    apply("makeupGain", p.makeupGain);
    apply("detector", (float) p.detector);
    apply("scHpf", (float) p.scHpf);
    apply("mix", p.mix);
    apply("range", p.range);
    // Deliberately not touching topology or bypass: topology is an
    // algorithm A/B for the whole session, not a musical setting, and a
    // preset that silently un-bypasses would be hostile.
}

const juce::String CompressorAudioProcessor::getProgramName(int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return {};
    return presets[(size_t) index].name;
}

void CompressorAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName); // factory presets are read-only
}

//==============================================================================
void CompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    samplerate = sampleRate;

    // Allocate the wet/dry scratch buffer now; processBlock must not.
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

    compressor = std::make_unique<Compressor>(getTotalNumOutputChannels());
    compressor->threshold = *thresholdParameter;
    compressor->ratio = *ratioParameter;
    compressor->setAttack(*attackParameter);
    compressor->setRelease(*releaseParameter);
    compressor->kneeWidth = *kneeWidthParameter;
    compressor->makeupGain = *makeupGainParameter;

    // prepare() last: it bakes the sample rate into the detector
    // coefficients and resets the envelopes, so it must see the final
    // attack/release times.
    compressor->prepare((float)sampleRate);
}

void CompressorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

        // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void CompressorAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    midiMessages.clear();

    const int numChannels = juce::jmin(buffer.getNumChannels(), compressor->getNumChannels());
    const int numSamples = buffer.getNumSamples();

    // Publish input peaks for the meters before anything touches the
    // buffer. Mono sources mirror into both meter slots.
    float inPk[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        inPk[ch] = buffer.getMagnitude(ch, 0, numSamples);
    if (numChannels == 1)
        inPk[1] = inPk[0];
    inputPeak[0].store(inPk[0], std::memory_order_relaxed);
    inputPeak[1].store(inPk[1], std::memory_order_relaxed);

    // Host- or UI-driven bypass: hand the audio through untouched, keep the
    // meters honest (out = in, no gain reduction).
    if (bypassParameter != nullptr && bypassParameter->get())
    {
        currentGain.store(1.0f, std::memory_order_relaxed);
        outputPeak[0].store(inPk[0], std::memory_order_relaxed);
        outputPeak[1].store(inPk[1], std::memory_order_relaxed);
        return;
    }

    // Pick up parameter changes once per block. Attack/release go through
    // the setters because changing them means re-deriving the detector
    // coefficients, not just storing a number.
    //
    // Note these are per-block steps, not smoothed ramps: a hard automation
    // jump in threshold/ratio can produce a small gain step ("zipper").
    // Mild here because the detector's own smoothing dominates; per-sample
    // parameter smoothing (juce::SmoothedValue) is the upgrade if it ever
    // becomes audible.
    compressor->threshold = *thresholdParameter;
    compressor->ratio = *ratioParameter;
    compressor->kneeWidth = *kneeWidthParameter;
    if (compressor->getAttack() != *attackParameter)
        compressor->setAttack(*attackParameter);
    if (compressor->getRelease() != *releaseParameter)
        compressor->setRelease(*releaseParameter);
    compressor->setTopology(*topologyParameter >= 0.5f ? Compressor::Topology::LogDomain
                                                       : Compressor::Topology::Classic);
    compressor->setDetectorMode(*detectorParameter >= 0.5f ? EnvelopeDetector::Mode::RMS
                                                           : EnvelopeDetector::Mode::Peak);
    const int scHpfIndex = juce::jlimit(0, 3, (int) *scHpfParameter);
    compressor->setSidechainCutoff(kScHpfFrequencies[scHpfIndex]);
    compressor->maxReductionDb = *rangeParameter;

    // Snapshot the dry signal for the wet/dry blend. Only the channels we
    // actually process; the buffer was sized in prepareToPlay.
    const float mix = juce::jlimit(0.0f, 1.0f, *mixParameter * 0.01f);
    const bool blending = mix < 1.0f;
    if (blending)
        for (int ch = 0; ch < juce::jmin(numChannels, dryBuffer.getNumChannels()); ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, juce::jmin(numSamples, dryBuffer.getNumSamples()));

    // Samples on the outer loop, channels inside: the stereo link needs to
    // see every channel's level at each instant before it can compute the
    // one shared gain for that instant.
    auto *const *channelData = buffer.getArrayOfWritePointers();
    float lastGain = 1.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        const float gain = compressor->processSampleLinked(channelData, numChannels, n);

        for (int channel = 0; channel < numChannels; ++channel)
            channelData[channel][n] *= gain;

        lastGain = gain;
    }

    // Expose the block's final gain for the editor's meter. One shared
    // value, since detection is linked. Relaxed ordering is enough: the
    // meter only wants a recent value, not a synchronised one.
    currentGain.store(lastGain, std::memory_order_relaxed);

    // Makeup gain, ramped across the block when the parameter moved so a
    // jump can't produce an audible step.
    if (*makeupGainParameter == previousMakeupGain)
    {
        buffer.applyGain(Compressor::dbToAmplitude(*makeupGainParameter));
    }
    else
    {
        buffer.applyGainRamp(0, numSamples,
                             Compressor::dbToAmplitude(previousMakeupGain),
                             Compressor::dbToAmplitude(*makeupGainParameter));
        previousMakeupGain = *makeupGainParameter;
    }

    // Wet/dry blend AFTER makeup gain: "wet" means the full compressed-and-
    // made-up path, so at 50% mix you hear classic parallel (New York)
    // compression - the crushed copy tucked under the untouched original.
    // At 100% (the default) this block is skipped and the math is exactly
    // the wet path, bit-for-bit.
    if (blending)
    {
        const int blendSamples = juce::jmin(numSamples, dryBuffer.getNumSamples());
        for (int ch = 0; ch < juce::jmin(numChannels, dryBuffer.getNumChannels()); ++ch)
        {
            buffer.applyGain(ch, 0, blendSamples, mix);
            buffer.addFrom(ch, 0, dryBuffer, ch, 0, blendSamples, 1.0f - mix);
        }
    }

    // Publish post-processing peaks for the output meters.
    float outPk[2] = { 0.0f, 0.0f };
    for (int ch = 0; ch < juce::jmin(numChannels, 2); ++ch)
        outPk[ch] = buffer.getMagnitude(ch, 0, numSamples);
    if (numChannels == 1)
        outPk[1] = outPk[0];
    outputPeak[0].store(outPk[0], std::memory_order_relaxed);
    outputPeak[1].store(outPk[1], std::memory_order_relaxed);
}

//==============================================================================
bool CompressorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *CompressorAudioProcessor::createEditor()
{
    return new CompressorAudioProcessorEditor(*this);
}

//==============================================================================
void CompressorAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CompressorAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new CompressorAudioProcessor();
}
