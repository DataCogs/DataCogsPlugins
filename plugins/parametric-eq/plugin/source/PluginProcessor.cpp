#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Frequency knobs travel in log like the display: skew each range so its
// geometric centre sits mid-rotation. Step intervals keep host text
// rendering sane (no interval = 7 decimal places everywhere).
juce::NormalisableRange<float> logHz (float min, float max, float centre)
{
    juce::NormalisableRange<float> range (min, max, 1.0f);
    range.setSkewForCentre (centre);
    return range;
}

// Per-band defaults: the classic channel-strip layout, transparent out of
// the box. Bands 1 and 6 are the HP/LP bookends and start inactive; the
// middle four start active but at 0 dB, which is exact unity.
struct BandDefault { const char* typeName; int type; float freq; bool active; };
constexpr BandDefault kBandDefaults[ParametricEQAudioProcessor::numBands] = {
    { "High-Pass", 3,    30.0f, false },
    { "Low Shelf", 1,   100.0f, true  },
    { "Bell",      0,   400.0f, true  },
    { "Bell",      0,  1500.0f, true  },
    { "High Shelf",2,  6000.0f, true  },
    { "Low-Pass",  4, 18000.0f, false },
};
} // namespace

//==============================================================================
juce::String ParametricEQAudioProcessor::bandParamID (int bandIndex, const char* suffix)
{
    return "band" + juce::String (bandIndex + 1) + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout
ParametricEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::StringArray typeNames { "Bell", "Low Shelf", "High Shelf",
                                        "High-Pass", "Low-Pass", "Notch" };

    for (int i = 0; i < numBands; ++i)
    {
        const auto& d = kBandDefaults[i];
        const auto num = juce::String (i + 1);

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID (bandParamID (i, "Active"), 1), "Band " + num + " Active", d.active));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (bandParamID (i, "Type"), 1), "Band " + num + " Type", typeNames, d.type));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (bandParamID (i, "Freq"), 1), "Band " + num + " Freq",
            logHz (20.0f, 20000.0f, 632.0f), d.freq));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (bandParamID (i, "Gain"), 1), "Band " + num + " Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
        // Q centre at 0.707 = Butterworth, the "no surprises" default for
        // every type; log skew because bandwidth perception is log too.
        auto qRange = juce::NormalisableRange<float> (0.1f, 18.0f, 0.01f);
        qRange.setSkewForCentre (0.707f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (bandParamID (i, "Q"), 1), "Band " + num + " Q", qRange, 0.707f));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("outGain", 1), "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("bypass", 1), "Bypass", false));

    return layout;
}

//==============================================================================
ParametricEQAudioProcessor::ParametricEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, juce::Identifier ("ParametricEQ"), createParameterLayout())
{
    for (int i = 0; i < numBands; ++i)
    {
        auto& band = bands[static_cast<size_t> (i)];
        band.active = parameters.getRawParameterValue (bandParamID (i, "Active"));
        band.type   = parameters.getRawParameterValue (bandParamID (i, "Type"));
        band.freq   = parameters.getRawParameterValue (bandParamID (i, "Freq"));
        band.gain   = parameters.getRawParameterValue (bandParamID (i, "Gain"));
        band.q      = parameters.getRawParameterValue (bandParamID (i, "Q"));
    }

    outGainParameter = parameters.getRawParameterValue ("outGain");
    bypassParameter = dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter ("bypass"));
}

//==============================================================================
const juce::String ParametricEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void ParametricEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  static_cast<juce::uint32> (samplesPerBlock),
                                  static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels())) };
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);
    // dsp::Gain's smoother default-constructs at 0: without this, the
    // first 20 ms after prepare fade in from silence. Snap to the real
    // target before any audio flows.
    outputGain.setGainDecibels (outGainParameter->load());
    outputGain.reset();

    analyzer.prepare (sampleRate);
    reset();
}

void ParametricEQAudioProcessor::reset()
{
    for (auto& band : bands)
        for (auto& s : band.state)
            s.reset();
    outputGain.reset();
}

bool ParametricEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

//==============================================================================
eq::BiquadCoefficients ParametricEQAudioProcessor::getBandCoefficients (int bandIndex,
                                                                        double sampleRate) const
{
    const auto& band = bands[static_cast<size_t> (bandIndex)];
    return eq::design (static_cast<eq::FilterType> (static_cast<int> (band.type->load())),
                       band.freq->load(), band.gain->load(), band.q->load(), sampleRate);
}

bool ParametricEQAudioProcessor::isBandActive (int bandIndex) const
{
    return bands[static_cast<size_t> (bandIndex)].active->load() > 0.5f;
}

void ParametricEQAudioProcessor::resetParametersToDefaults()
{
    for (auto* p : getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
    currentProgram = 0;
}

//==============================================================================
const std::vector<ParametricEQAudioProcessor::Preset>&
ParametricEQAudioProcessor::getFactoryPresets()
{
    // Types: 0 Bell, 1 LowShelf, 2 HighShelf, 3 HighPass, 4 LowPass, 5 Notch.
    // Starting points, not prescriptions - every one is meant to be
    // reshaped by ear. Gains are deliberately modest (broadcast "mixing
    // preset" curves, not demo-mode smiley faces); Telephone is the one
    // deliberate caricature.
    static const std::vector<Preset> presets = {
        { "Default", {
            { false, 3,    30.0f,  0.0f, 0.707f },
            { true,  1,   100.0f,  0.0f, 0.707f },
            { true,  0,   400.0f,  0.0f, 0.707f },
            { true,  0,  1500.0f,  0.0f, 0.707f },
            { true,  2,  6000.0f,  0.0f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Vocal Clarity", {                          // HP the rumble, dip the
            { true,  3,    90.0f,  0.0f, 0.707f },    // mud, lift presence + air
            { true,  0,   250.0f, -2.5f, 1.0f   },
            { true,  0,   700.0f, -1.5f, 1.4f   },
            { true,  0,  3200.0f,  2.5f, 1.0f   },
            { true,  2,  9000.0f,  2.0f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Kick Punch", {                             // thump, de-box, click
            { true,  3,    28.0f,  0.0f, 0.707f },
            { true,  0,    62.0f,  3.5f, 1.1f   },
            { true,  0,   350.0f, -4.0f, 1.2f   },
            { true,  0,  3800.0f,  4.0f, 1.4f   },
            { true,  2, 10000.0f, -1.5f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Bass Control", {                           // solidity without boom
            { true,  3,    32.0f,  0.0f, 0.707f },
            { true,  0,    90.0f,  2.5f, 0.9f   },
            { true,  0,   300.0f, -2.0f, 1.0f   },
            { true,  0,   800.0f,  1.5f, 1.2f   },
            { true,  2,  6000.0f, -2.0f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Acoustic Sparkle", {                       // body dip, string shine
            { true,  3,    80.0f,  0.0f, 0.707f },
            { true,  0,   220.0f, -2.5f, 1.0f   },
            { true,  0,  1000.0f, -1.0f, 1.0f   },
            { true,  0,  5000.0f,  2.0f, 1.0f   },
            { true,  2, 10000.0f,  2.5f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Drum Bus", {                               // weight + air, less honk
            { false, 3,    30.0f,  0.0f, 0.707f },
            { true,  1,   100.0f,  1.5f, 0.707f },
            { true,  0,   450.0f, -1.5f, 1.0f   },
            { true,  0,  1500.0f,  0.0f, 0.707f },
            { true,  2,  9000.0f,  2.0f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Master Polish", {                          // the gentlest of tilts
            { true,  3,    20.0f,  0.0f, 0.707f },
            { true,  1,    80.0f,  1.0f, 0.707f },
            { true,  0,   350.0f, -0.5f, 0.8f   },
            { true,  0,  2500.0f,  0.5f, 0.8f   },
            { true,  2, 11000.0f,  1.5f, 0.707f },
            { false, 4, 18000.0f,  0.0f, 0.707f } }, 0.0f },
        { "Telephone", {                              // band-limited FX caricature
            { true,  3,   600.0f,  0.0f, 1.2f   },
            { false, 1,   100.0f,  0.0f, 0.707f },
            { true,  0,  1800.0f,  4.0f, 1.4f   },
            { false, 0,  1500.0f,  0.0f, 0.707f },
            { false, 2,  6000.0f,  0.0f, 0.707f },
            { true,  4,  3000.0f,  0.0f, 1.2f   } }, 0.0f },
    };
    return presets;
}

int ParametricEQAudioProcessor::getNumPrograms()
{
    return static_cast<int> (getFactoryPresets().size());
}

int ParametricEQAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

const juce::String ParametricEQAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return {};
    return presets[static_cast<size_t> (index)].name;
}

void ParametricEQAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    currentProgram = index;
    const auto& preset = presets[static_cast<size_t> (index)];

    auto apply = [this] (const juce::String& paramID, float value)
    {
        if (auto* param = parameters.getParameter (paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    for (int i = 0; i < numBands; ++i)
    {
        const auto& b = preset.bands[i];
        apply (bandParamID (i, "Active"), b.active ? 1.0f : 0.0f);
        apply (bandParamID (i, "Type"), static_cast<float> (b.type));
        apply (bandParamID (i, "Freq"), b.freq);
        apply (bandParamID (i, "Gain"), b.gain);
        apply (bandParamID (i, "Q"), b.q);
    }
    apply ("outGain", preset.outGain);
}

//==============================================================================
void ParametricEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    if (numSamples == 0 || numChannels == 0)
        return;

    if (bypassParameter->get())
    {
        // Passthrough; clear filter state on the transition so re-engaging
        // doesn't replay a stale filter tail.
        if (! wasBypassed)
            reset();
        wasBypassed = true;
        return;
    }
    wasBypassed = false;

    for (auto& band : bands)
    {
        const bool active = band.active->load() > 0.5f;
        const int type = static_cast<int> (band.type->load());

        // Reset only on discontinuous changes (see class comment).
        if (type != band.lastType || (active && ! band.wasActive))
            for (auto& s : band.state)
                s.reset();
        band.lastType = type;
        band.wasActive = active;

        if (! active)
            continue;

        band.coefficients = eq::design (static_cast<eq::FilterType> (type),
                                        band.freq->load(), band.gain->load(),
                                        band.q->load(), currentSampleRate);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* samples = buffer.getWritePointer (ch);
            auto& state = band.state[static_cast<size_t> (ch)];
            for (int i = 0; i < numSamples; ++i)
                samples[i] = state.processSample (samples[i], band.coefficients);
        }
    }

    outputGain.setGainDecibels (outGainParameter->load());
    auto block = juce::dsp::AudioBlock<float> (buffer)
                     .getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                     .getSubBlock (0, static_cast<size_t> (numSamples));
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    // Post-EQ spectrum: the display should show what you hear.
    analyzer.pushSamples (buffer.getReadPointer (0),
                          numChannels > 1 ? buffer.getReadPointer (1) : nullptr,
                          numSamples);
}

//==============================================================================
void ParametricEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ParametricEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* ParametricEQAudioProcessor::createEditor()
{
    return new ParametricEQAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ParametricEQAudioProcessor();
}
