#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "IRGenerator.h"
#include "IRTools.h"

#include <cmath>

namespace
{
// The wet-path filters hard-bypass at the ends of their travel so the
// defaults are genuinely transparent. A first-order filter "at the edge of
// hearing" is NOT transparent - a 20 kHz TPT lowpass at fs=48k still costs
// ~2 dB at 10 kHz - so parking the knob at the end must skip the filter
// entirely, not just push the cutoff out.
constexpr float kLowCutBypassHz  = 20.5f;    // <= this: skip the highpass
constexpr float kHighCutBypassHz = 19500.0f; // >= this: skip the lowpass

constexpr float kMaxPreDelayMs = 250.0f;

// Guard against pathological files: a 30 s IR at 96 kHz is already ~23 MB
// per channel as floats and far beyond any real room.
constexpr double kMaxIrSeconds = 30.0;

// Debounce for Size/Decay knob drags: each change rebuilds and reloads the
// IR (allocation + background convolution restart), so wait for the knob to
// settle rather than rebuilding per tick.
constexpr int kIrRebuildDebounceMs = 120;

// Frequency perception is logarithmic, so the filter knobs get a skew that
// puts the musically useful range mid-rotation. Every range also gets a
// step interval: the interval drives AudioParameterFloat's default text
// formatting (no interval = 7 decimal places in every host generic view),
// and slider attachments delegate text rendering to the parameter.
juce::NormalisableRange<float> skewed (float min, float max, float step, float centre)
{
    juce::NormalisableRange<float> range (min, max, step);
    range.setSkewForCentre (centre);
    return range;
}
} // namespace

//==============================================================================
ConvolutionReverbAudioProcessor::ConvolutionReverbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, juce::Identifier ("ConvolutionReverb"),
                  {std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("mix", 1), "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), DEFAULT_MIX),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("preDelay", 1), "Pre-Delay", juce::NormalisableRange<float> (0.0f, kMaxPreDelayMs, 0.5f), DEFAULT_PREDELAY),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("lowCut", 1), "Low Cut", skewed (20.0f, 1000.0f, 1.0f, 150.0f), DEFAULT_LOW_CUT),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("highCut", 1), "High Cut", skewed (1000.0f, 20000.0f, 10.0f, 6000.0f), DEFAULT_HIGH_CUT),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("outGain", 1), "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), DEFAULT_OUT_GAIN),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("size", 1), "Size", juce::NormalisableRange<float> (50.0f, 200.0f, 1.0f), DEFAULT_SIZE),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("decay", 1), "Decay", juce::NormalisableRange<float> (25.0f, 200.0f, 1.0f), DEFAULT_DECAY),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("early", 1), "Early", juce::NormalisableRange<float> (-60.0f, 12.0f, 0.5f), DEFAULT_EARLY),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("tail", 1), "Tail", juce::NormalisableRange<float> (-60.0f, 12.0f, 0.5f), DEFAULT_TAIL),
                   std::make_unique<juce::AudioParameterFloat> (juce::ParameterID ("width", 1), "Width", juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), DEFAULT_WIDTH),
                   std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("reverse", 1), "Reverse", false),
                   std::make_unique<juce::AudioParameterBool> (juce::ParameterID ("bypass", 1), "Bypass", false)})
{
    mixParameter      = parameters.getRawParameterValue ("mix");
    preDelayParameter = parameters.getRawParameterValue ("preDelay");
    lowCutParameter   = parameters.getRawParameterValue ("lowCut");
    highCutParameter  = parameters.getRawParameterValue ("highCut");
    outGainParameter  = parameters.getRawParameterValue ("outGain");
    sizeParameter     = parameters.getRawParameterValue ("size");
    decayParameter    = parameters.getRawParameterValue ("decay");
    earlyParameter    = parameters.getRawParameterValue ("early");
    tailParameter     = parameters.getRawParameterValue ("tail");
    widthParameter    = parameters.getRawParameterValue ("width");
    reverseParameter  = dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter ("reverse"));
    bypassParameter   = dynamic_cast<juce::AudioParameterBool*> (parameters.getParameter ("bypass"));

    // IR-domain parameters rebuild the IR off the audio thread (see
    // parameterChanged); everything else is read directly in processBlock.
    for (auto* id : { "size", "decay", "early", "tail", "reverse" })
        parameters.addParameterListener (id, this);

    lowCutFilter.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    highCutFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

    // Ship a sound out of the box: the synthetic hall loads immediately so
    // the plugin reverberates before the user ever touches "Load IR".
    // Convolution accepts IRs before prepare() and resamples on the fly, so
    // constructor-time loading is safe.
    loadBuiltInImpulseResponse();
}

ConvolutionReverbAudioProcessor::~ConvolutionReverbAudioProcessor()
{
    for (auto* id : { "size", "decay", "early", "tail", "reverse" })
        parameters.removeParameterListener (id, this);
    stopTimer();
    cancelPendingUpdate();
}

//==============================================================================
void ConvolutionReverbAudioProcessor::parameterChanged (const juce::String&, float)
{
    // May arrive on the audio thread (host automation) - hop to the message
    // thread, where the debounce timer and the rebuild are allowed to live.
    triggerAsyncUpdate();
}

void ConvolutionReverbAudioProcessor::handleAsyncUpdate()
{
    startTimer (kIrRebuildDebounceMs); // restarting an active timer extends it
}

void ConvolutionReverbAudioProcessor::timerCallback()
{
    stopTimer();
    rebuildAndLoadIr();
}

//==============================================================================
const juce::String ConvolutionReverbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

double ConvolutionReverbAudioProcessor::getTailLengthSeconds() const
{
    return irLengthSeconds.load (std::memory_order_relaxed)
           + static_cast<double> (preDelayParameter->load()) * 0.001;
}

//==============================================================================
juce::File ConvolutionReverbAudioProcessor::getIrLibraryFolder()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile ("Library/Audio/Impulse Responses/DataCogs");
#else
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("DataCogs/Impulse Responses");
#endif
}

std::vector<juce::File> ConvolutionReverbAudioProcessor::getIrLibraryRoots()
{
#if JUCE_MAC
    return { getIrLibraryFolder(),
             juce::File ("/Library/Audio/Impulse Responses/DataCogs") };
#else
    return { getIrLibraryFolder(),
             juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                 .getChildFile ("DataCogs/Impulse Responses") };
#endif
}

juce::File ConvolutionReverbAudioProcessor::resolveIrLibraryFile (const juce::String& relPath)
{
    for (const auto& root : getIrLibraryRoots())
    {
        auto file = root.getChildFile (relPath);
        if (file.existsAsFile())
            return file;
    }
    return getIrLibraryFolder().getChildFile (relPath);
}

const std::vector<ConvolutionReverbAudioProcessor::Preset>&
ConvolutionReverbAudioProcessor::getFactoryPresets()
{
    // name              mix   pre   size  decay early tail  width lowCut highCut  out  rev  IR (library-relative)
    static const std::vector<Preset> presets = {
        { "Default",        30,    0,  100,  100,    0,    0,  100,    20,  20000,   0, false, "" },
        { "Vocal Hall",     22,   30,  100,   75,    0,    0,  100,   150,   8000,   0, false, "Voxengo IMreverbs/Scala Milan Opera Hall.wav" },
        { "Tight Chamber",  16,   10,   65,   60,    0,   -3,  110,   120,  10000,   0, false, "OpenAIR/St Patricks Church Patrington.wav" },
        { "Drum Room",      14,    0,   70,   50,    0,   -6,  140,    80,  12000,   0, false, "Voxengo IMreverbs/Nice Drum Room.wav" },
        { "Cathedral",      30,   20,  100,  100,    0,    0,  100,   100,  20000,   0, false, "OpenAIR/York Minster.wav" },
        { "Golden Hall",    25,   20,  100,   95,    0,    0,  100,   100,  12000,   0, false, "Voxengo IMreverbs/Musikvereinsaal.wav" },
        { "Parking Garage", 20,    0,   90,   80,    0,    0,  120,    60,   9000,   0, false, "Voxengo IMreverbs/Parking Garage.wav" },
        { "The Silo",       25,   10,  110,   90,    0,    0,  100,   120,   8000,   0, false, "Voxengo IMreverbs/In The Silo.wav" },
        { "Prehistoric Cave", 28,  15,   85,   85,    0,    0,  100,    80,   7000,   0, false, "Voxengo IMreverbs/Small Prehistoric Cave.wav" },
        { "Huge Ambience",  35,   40,  130,  120,  -60,    0,  150,   150,   6000,   0, false, "OpenAIR/Hamilton Mausoleum.wav" },
        { "Deep Space",     40,   40,  130,  120,  -60,    0,  150,   150,   6000,   0, false, "Voxengo IMreverbs/Deep Space.wav" },
        { "Reverse Swell",  45,  100,  100,  100,    0,    0,  100,    20,  20000,   0, true,  "" },
    };
    return presets;
}

void ConvolutionReverbAudioProcessor::resetParametersToDefaults()
{
    // Every parameter knows its own default in normalised form; applying
    // via setValueNotifyingHost keeps hosts and attachments in sync, and
    // the IR-domain parameter listeners schedule the rebuild as usual.
    for (auto* p : getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
}

int ConvolutionReverbAudioProcessor::getNumPrograms()
{
    return static_cast<int> (getFactoryPresets().size());
}

int ConvolutionReverbAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

const juce::String ConvolutionReverbAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return {};
    return presets[static_cast<size_t> (index)].name;
}

void ConvolutionReverbAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    currentProgram = index;
    const auto& p = presets[static_cast<size_t> (index)];

    // Applied through the parameter interface (normalised + notify) so the
    // host sees each change exactly as if the user turned the knob.
    auto apply = [this] (const char* paramID, float value)
    {
        if (auto* param = parameters.getParameter (paramID))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    apply ("mix", p.mix);
    apply ("preDelay", p.preDelay);
    apply ("size", p.size);
    apply ("decay", p.decay);
    apply ("early", p.early);
    apply ("tail", p.tail);
    apply ("width", p.width);
    apply ("lowCut", p.lowCut);
    apply ("highCut", p.highCut);
    apply ("outGain", p.outGain);
    apply ("reverse", p.reverse ? 1.0f : 0.0f);

    // The preset's IR, if it names one and it's installed; otherwise keep
    // what's loaded for file-less presets, or fall back to the built-in.
    if (p.irRelPath[0] == '\0')
    {
        loadBuiltInImpulseResponse();
    }
    else
    {
        const auto file = resolveIrLibraryFile (p.irRelPath);
        if (! (file.existsAsFile() && loadImpulseResponseFile (file)))
            loadBuiltInImpulseResponse();
    }
}

//==============================================================================
void ConvolutionReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32> (juce::jmax (1, getTotalNumOutputChannels()));

    convolution.prepare (spec);
    preDelayLine.prepare (spec);
    preDelayLine.setMaximumDelayInSamples (
        static_cast<int> (std::ceil (kMaxPreDelayMs * 0.001 * sampleRate)) + 8);
    lowCutFilter.prepare (spec);
    highCutFilter.prepare (spec);
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);
    // dsp::Gain's smoother default-constructs at 0: without this, the
    // first 20 ms after prepare fade in from silence.
    outputGain.setGainDecibels (outGainParameter->load());
    outputGain.reset();

    // Scratch buffer for the wet path, sized here - never in processBlock.
    wetBuffer.setSize (static_cast<int> (spec.numChannels), samplesPerBlock);

    // 50 ms parameter smoothing; snap to the current values so there is no
    // fade-in transient on transport start.
    mixSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.setCurrentAndTargetValue (mixParameter->load() * 0.01f);
    preDelaySamplesSmoothed.reset (sampleRate, 0.05);
    preDelaySamplesSmoothed.setCurrentAndTargetValue (
        static_cast<float> (preDelayParameter->load() * 0.001 * sampleRate));
    widthSmoothed.reset (sampleRate, 0.05);
    widthSmoothed.setCurrentAndTargetValue (widthParameter->load() * 0.01f);

    // The default Convolution engine is the zero-latency variant, so this
    // reports 0 - but ask rather than assume, so switching engines later
    // can't silently lie to the host.
    setLatencySamples (static_cast<int> (convolution.getLatency()));
}

void ConvolutionReverbAudioProcessor::releaseResources() {}

void ConvolutionReverbAudioProcessor::reset()
{
    convolution.reset();
    preDelayLine.reset();
    lowCutFilter.reset();
    highCutFilter.reset();
    outputGain.reset();
}

bool ConvolutionReverbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono or stereo, and in == out (a reverb that changes channel count is
    // a different product).
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

//==============================================================================
void ConvolutionReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), wetBuffer.getNumChannels());

    if (numSamples == 0 || numChannels == 0)
        return;

    // Host bypass: with zero latency, honest bypass is a plain passthrough.
    // The wet chain is reset on the transition so a stale tail can't burst
    // out when the user re-engages seconds later.
    if (bypassParameter->get())
    {
        if (! wasBypassed)
            reset();
        wasBypassed = true;
        return;
    }
    wasBypassed = false;

    // ---- Wet path: copy input, then convolution -> pre-delay -> filters ----
    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    auto wetBlock = juce::dsp::AudioBlock<float> (wetBuffer)
                        .getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                        .getSubBlock (0, static_cast<size_t> (numSamples));

    convolution.process (juce::dsp::ProcessContextReplacing<float> (wetBlock));

    // Pre-delay, smoothed per sample. Linear interpolation pitch-bends
    // briefly while the knob moves - the classic analog-delay artifact -
    // which is musically acceptable for a set-and-forget parameter.
    preDelaySamplesSmoothed.setTargetValue (
        static_cast<float> (preDelayParameter->load() * 0.001 * currentSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        preDelayLine.setDelay (preDelaySamplesSmoothed.getNextValue());
        for (int ch = 0; ch < numChannels; ++ch)
        {
            preDelayLine.pushSample (ch, wetBuffer.getSample (ch, i));
            wetBuffer.setSample (ch, i, preDelayLine.popSample (ch));
        }
    }

    // Wet-path tone shaping. Hard-bypassed at the ends of the knob travel
    // (see kLowCutBypassHz above); the filters are reset while bypassed so
    // re-engaging starts from silence, not a year-old state.
    const float lowCutHz  = lowCutParameter->load();
    const float highCutHz = highCutParameter->load();

    if (lowCutHz > kLowCutBypassHz)
    {
        lowCutFilter.setCutoffFrequency (lowCutHz);
        lowCutFilter.process (juce::dsp::ProcessContextReplacing<float> (wetBlock));
    }
    else
    {
        lowCutFilter.reset();
    }

    if (highCutHz < kHighCutBypassHz)
    {
        highCutFilter.setCutoffFrequency (highCutHz);
        highCutFilter.process (juce::dsp::ProcessContextReplacing<float> (wetBlock));
    }
    else
    {
        highCutFilter.reset();
    }

    // ---- Wet stereo width (mid/side) ----
    // S is scaled, M untouched: width 0 collapses the wet field to mono
    // (M survives, so level holds), 200% doubles the side energy. Applied
    // to the wet path only - the dry image is the artist's business.
    if (numChannels == 2)
    {
        widthSmoothed.setTargetValue (widthParameter->load() * 0.01f);
        auto* left  = wetBuffer.getWritePointer (0);
        auto* right = wetBuffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float w = widthSmoothed.getNextValue();
            const float mid  = 0.5f * (left[i] + right[i]);
            const float side = 0.5f * (left[i] - right[i]) * w;
            left[i]  = mid + side;
            right[i] = mid - side;
        }
    }

    // ---- Equal-power dry/wet mix ----
    // Dry and (late) wet are decorrelated, so their POWERS add, not their
    // amplitudes. The sin/cos law keeps total power constant across the
    // knob: dry*cos(m*pi/2) + wet*sin(m*pi/2), since cos^2 + sin^2 = 1.
    // A linear crossfade would dip ~3 dB at 50%.
    mixSmoothed.setTargetValue (mixParameter->load() * 0.01f);

    // Makeup for the engine's IR normalisation (see wetMakeupGain). All
    // wet-path stages are linear, so applying it at the mix is equivalent
    // to applying it anywhere upstream.
    const float makeup = wetMakeupGain.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mixSmoothed.getNextValue();
        const float wetGain = makeup * std::sin (m * juce::MathConstants<float>::halfPi);
        const float dryGain = std::cos (m * juce::MathConstants<float>::halfPi);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = buffer.getSample (ch, i);
            const float wet = wetBuffer.getSample (ch, i);
            buffer.setSample (ch, i, dryGain * dry + wetGain * wet);
        }
    }

    // ---- Output trim (ramped by dsp::Gain, so no zipper) ----
    outputGain.setGainDecibels (outGainParameter->load());
    auto outBlock = juce::dsp::AudioBlock<float> (buffer)
                        .getSubsetChannelBlock (0, static_cast<size_t> (numChannels))
                        .getSubBlock (0, static_cast<size_t> (numSamples));
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (outBlock));
}

//==============================================================================
bool ConvolutionReverbAudioProcessor::loadImpulseResponseFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats(); // wav, aiff, flac, ogg, mp3 (per platform)

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0)
        return false;

    // The whole file is read into memory (rather than handed to Convolution
    // as a path) because Size/Decay need the original samples to transform.
    const int numSamples = static_cast<int> (juce::jmin (
        reader->lengthInSamples,
        static_cast<juce::int64> (kMaxIrSeconds * reader->sampleRate)));

    // B-format/multichannel captures: keep the first two channels. reader->
    // read() requires the destination to cover all source channels, so read
    // fully, then shrink.
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), numSamples);
    if (! reader->read (&buffer, 0, numSamples, 0, true, true))
        return false;

    if (buffer.getNumChannels() > 2)
        buffer.setSize (2, numSamples, true /* keep content */);

    setMasterIr (std::move (buffer), reader->sampleRate,
                 true /* normalise */, true /* trim */,
                 file, file.getFileNameWithoutExtension());
    return true;
}

void ConvolutionReverbAudioProcessor::loadBuiltInImpulseResponse()
{
    // Generated at a fixed 48 kHz; Convolution resamples to the session
    // rate, so the generator stays deterministic across hosts.
    ir::BuiltInIrSpec spec;
    auto channels = ir::generateNoiseTailIr (spec, 2);

    const int numSamples = static_cast<int> (channels[0].size());
    juce::AudioBuffer<float> buffer (2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        buffer.copyFrom (ch, 0, channels[static_cast<size_t> (ch)].data(), numSamples);

    setMasterIr (std::move (buffer), spec.sampleRate,
                 true /* normalise */, false /* trim */,
                 juce::File(), "Built-in Hall");
}

void ConvolutionReverbAudioProcessor::loadImpulseResponseBuffer (juce::AudioBuffer<float>&& buffer,
                                                                 double bufferSampleRate,
                                                                 bool normalise)
{
    setMasterIr (std::move (buffer), bufferSampleRate,
                 normalise, false /* trim */,
                 juce::File(), "(buffer)");
}

void ConvolutionReverbAudioProcessor::setMasterIr (juce::AudioBuffer<float>&& buffer,
                                                   double bufferSampleRate,
                                                   bool normalise, bool trim,
                                                   const juce::File& sourceFile,
                                                   const juce::String& name)
{
    masterIr = std::move (buffer);
    masterIrRate = bufferSampleRate;
    masterIrNormalise = normalise;
    masterIrTrim = trim;
    irFile = sourceFile;
    irName = name;

    // Measured once per load: the Decay control needs the room's own RT60
    // as its reference point, and it doesn't change under Size/Decay
    // rebuilds (those always start from this master copy).
    masterIrRt60 = ir::measureRt60 (masterIr.getReadPointer (0),
                                    masterIr.getNumSamples(), masterIrRate);

    rebuildAndLoadIr();
}

void ConvolutionReverbAudioProcessor::rebuildAndLoadIr()
{
    if (masterIr.getNumSamples() == 0)
        return;

    const double sizeFactor  = static_cast<double> (sizeParameter->load()) / 100.0;
    const double decayFactor = static_cast<double> (decayParameter->load()) / 100.0;

    // Always transform a fresh copy of the master - transforms don't stack.
    juce::AudioBuffer<float> shaped;
    shaped.makeCopyOf (masterIr);

    // Decay: exponential envelope correction against the measured RT60
    // (see IRTools.h). Skipped when it can't be measured (e.g. a delta IR).
    if (masterIrRt60 > 0.0)
        for (int ch = 0; ch < shaped.getNumChannels(); ++ch)
            ir::reshapeDecay (shaped.getWritePointer (ch), shaped.getNumSamples(),
                              masterIrRate, masterIrRt60, decayFactor);

    // Early/Tail: independent gains on the two perceptual regions of the
    // IR (see IRTools.h). -60 dB on the knob means "off", mapped to exact
    // silence rather than a -60 dB residue.
    const float earlyDb = earlyParameter->load();
    const float tailDb  = tailParameter->load();
    if (std::abs (earlyDb) > 0.01f || std::abs (tailDb) > 0.01f)
    {
        auto dbToGain = [] (float db)
        { return db <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain (db); };

        for (int ch = 0; ch < shaped.getNumChannels(); ++ch)
            ir::applyEarlyTailGains (shaped.getWritePointer (ch), shaped.getNumSamples(),
                                     masterIrRate, dbToGain (earlyDb), dbToGain (tailDb));
    }

    // Reverse: the classic tape-era effect - the room swells INTO the
    // sound. Applied after the envelope shaping so Decay still describes
    // the (now mirrored) energy slope.
    if (reverseParameter->get())
        shaped.reverse (0, shaped.getNumSamples());

    // Size: relabel the sample rate instead of resampling ourselves. Telling
    // the engine the IR was recorded at rate/sizeFactor makes its own
    // resampler stretch the response by sizeFactor - length AND modal
    // frequencies scale together, which is exactly what "a bigger room"
    // means physically (and what Altiverb's Size does).
    const double claimedRate = masterIrRate / sizeFactor;

    irLengthSeconds.store (shaped.getNumSamples() / masterIrRate * sizeFactor,
                           std::memory_order_relaxed);

    convolution.loadImpulseResponse (std::move (shaped), claimedRate,
                                     juce::dsp::Convolution::Stereo::yes,
                                     masterIrTrim ? juce::dsp::Convolution::Trim::yes
                                                  : juce::dsp::Convolution::Trim::no,
                                     masterIrNormalise ? juce::dsp::Convolution::Normalise::yes
                                                       : juce::dsp::Convolution::Normalise::no);

    // Normalise::yes leaves the IR at a total energy of 0.125^2 (see the
    // header comment on wetMakeupGain); x8 brings it back to unit energy so
    // the wet path plays at dry loudness. Kept as makeup here - rather than
    // Normalise::no plus our own scaling - because the engine normalises
    // AFTER its internal resampling, so the level stays right across Size
    // changes and session/file rate mismatches.
    wetMakeupGain.store (masterIrNormalise ? 8.0f : 1.0f, std::memory_order_relaxed);
}

//==============================================================================
void ConvolutionReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Parameters live in the APVTS; the IR path rides along as a property
    // on the same tree so one blob restores the whole plugin.
    auto state = parameters.copyState();
    state.setProperty ("irPath", irFile.getFullPathName(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ConvolutionReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const juce::String irPath = state.getProperty ("irPath", juce::String());

    parameters.replaceState (state);

    // Restore the IR last. If the file has vanished (moved project, other
    // machine) fall back to the built-in so the session still makes sound.
    if (irPath.isNotEmpty() && loadImpulseResponseFile (juce::File (irPath)))
        return;

    loadBuiltInImpulseResponse();
}

//==============================================================================
juce::AudioProcessorEditor* ConvolutionReverbAudioProcessor::createEditor()
{
    return new ConvolutionReverbAudioProcessorEditor (*this);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConvolutionReverbAudioProcessor();
}
