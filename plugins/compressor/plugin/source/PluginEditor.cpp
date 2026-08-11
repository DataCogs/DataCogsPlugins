//
//  PluginEditor.cpp
//  Compressor - Plugin Editor
//
//  See PluginEditor.h for the design rules (attachments for parameters,
//  polled atomics for metering).
//

#include "PluginEditor.h"
#include "DataCogsAssets.h"

//==============================================================================
namespace
{
// Jokey engraved pictograms, one per parameter: threshold = carrying the
// bride over it, attack = the knife guy, release = letting the fish go,
// makeup = lipstick, range = home on the. Drawn on a 24x24 grid and
// scaled into place; stroke-only "etched faceplate" style so the gags
// whisper rather than shout. Keyed by parameter ID.
void drawParameterIcon (juce::Graphics& g, const juce::String& icon, juce::Rectangle<float> area)
{
    const float scale = juce::jmin (area.getWidth(), area.getHeight()) / 24.0f;

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::scale (scale).translated (
        area.getCentreX() - 12.0f * scale, area.getCentreY() - 12.0f * scale));
    g.setColour (DataCogsLookAndFeel::textDim);

    const juce::PathStrokeType stroke (1.5f, juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);
    juce::Path p;

    if (icon == "threshold")            // over it, through the door
    {
        // Classic carry pose: groom upright facing us, bride horizontal
        // across his arms, legs kicked up, dress draping down in front.
        p.addRectangle (17.0f, 3.0f, 5.0f, 18.0f);                  // the door
        p.addEllipse (6.4f, 3.0f, 3.2f, 3.2f);                      // groom, facing us
        p.startNewSubPath (8.0f, 7.6f);  p.lineTo (8.0f, 15.0f);    // upright body
        p.startNewSubPath (8.0f, 15.0f); p.lineTo (6.0f, 21.0f);
        p.startNewSubPath (8.0f, 15.0f); p.lineTo (10.0f, 21.0f);
        p.startNewSubPath (8.0f, 11.5f); p.lineTo (4.6f, 9.6f);     // arms under the bride
        p.startNewSubPath (8.0f, 11.5f); p.lineTo (11.6f, 9.6f);
        p.startNewSubPath (2.6f, 9.4f);  p.lineTo (13.0f, 9.4f);    // bride across his arms
        p.addEllipse (11.8f, 6.6f, 3.0f, 3.0f);                     // her head, next to his
        p.startNewSubPath (4.0f, 9.4f);  p.lineTo (2.2f, 6.2f);     // both legs kicked up
        p.startNewSubPath (5.6f, 9.4f);  p.lineTo (4.0f, 6.6f);
        g.strokePath (p, stroke);
        juce::Path dress;                                           // what sells the gag
        dress.addTriangle (6.4f, 9.4f, 11.2f, 9.4f, 8.6f, 15.4f);
        g.fillPath (dress);
        juce::Path veil;                                            // hanging down behind her head
        veil.startNewSubPath (13.2f, 6.8f);
        veil.quadraticTo (16.4f, 8.4f, 16.0f, 13.4f);               // outer edge drapes down
        veil.quadraticTo (14.2f, 10.6f, 12.4f, 9.6f);               // back up to the head
        veil.closeSubPath();
        g.setColour (DataCogsLookAndFeel::textDim.withAlpha (0.55f));
        g.fillPath (veil);
    }
    else if (icon == "ratio")           // big weight : little weight
    {
        p.startNewSubPath (4.0f, 9.5f);   p.lineTo (20.0f, 12.5f);  // tilted beam
        p.startNewSubPath (12.0f, 11.2f); p.lineTo (12.0f, 20.0f);
        p.startNewSubPath (7.5f, 20.0f);  p.lineTo (16.5f, 20.0f);
        p.addRectangle (1.8f, 10.0f, 4.6f, 4.6f);
        p.addRectangle (18.6f, 13.0f, 2.6f, 2.6f);
        g.strokePath (p, stroke);
    }
    else if (icon == "attack")          // the shower scene
    {
        p.addEllipse (6.2f, 4.2f, 3.6f, 3.6f);
        p.startNewSubPath (8.0f, 8.0f);  p.lineTo (8.0f, 15.5f);
        p.startNewSubPath (8.0f, 15.5f); p.lineTo (5.0f, 21.0f);
        p.startNewSubPath (8.0f, 15.5f); p.lineTo (11.0f, 21.0f);
        p.startNewSubPath (8.0f, 10.0f); p.lineTo (13.0f, 6.5f);    // raised arm
        g.strokePath (p, stroke);
        juce::Path blade;
        blade.addTriangle (12.6f, 6.6f, 20.0f, 0.4f, 16.4f, 8.4f);  // a proper knife
        g.fillPath (blade);
    }
    else if (icon == "release")         // this one's going back
    {
        juce::Path fish;
        fish.addEllipse (-4.0f, -2.1f, 8.0f, 4.2f);
        fish.startNewSubPath (-3.2f, 0.0f);
        fish.lineTo (-6.2f, -2.6f); fish.lineTo (-6.2f, 2.6f);
        fish.closeSubPath();
        fish.applyTransform (juce::AffineTransform::rotation (-0.5f).translated (10.0f, 9.0f));
        g.strokePath (fish, stroke);
        g.fillEllipse (12.2f, 6.4f, 1.3f, 1.3f);                    // eye
        p.startNewSubPath (2.0f, 20.0f);                            // water
        p.quadraticTo (5.0f, 18.2f, 8.0f, 20.0f);
        p.quadraticTo (11.0f, 21.8f, 14.0f, 20.0f);
        p.quadraticTo (17.0f, 18.2f, 20.0f, 20.0f);
        p.startNewSubPath (17.0f, 4.2f); p.lineTo (19.8f, 2.6f);    // off it goes
        p.startNewSubPath (17.6f, 7.2f); p.lineTo (20.8f, 6.4f);
        g.strokePath (p, stroke);
    }
    else if (icon == "kneeWidth")       // it's a knee
    {
        // Chunky bent leg, plus the pharmacy-leaflet "ouch" strokes
        // radiating from the joint - that's what makes it read as a knee.
        p.startNewSubPath (3.5f, 4.5f);
        p.lineTo (12.5f, 10.5f);        // thigh
        p.lineTo (8.5f, 19.5f);         // shin
        p.lineTo (15.0f, 20.5f);        // foot
        g.strokePath (p, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        juce::Path ouch;
        ouch.startNewSubPath (16.0f, 6.0f);  ouch.lineTo (14.4f, 8.4f);
        ouch.startNewSubPath (18.6f, 9.6f);  ouch.lineTo (16.0f, 10.6f);
        ouch.startNewSubPath (18.8f, 13.8f); ouch.lineTo (16.2f, 13.0f);
        g.strokePath (ouch, stroke);
    }
    else if (icon == "makeupGain")      // puckered lips, mid-kiss
    {
        juce::Path lips;
        lips.startNewSubPath (2.5f, 12.0f);                         // left corner
        lips.quadraticTo (5.5f, 5.4f, 9.8f, 7.6f);                  // left bump
        lips.quadraticTo (12.0f, 9.2f, 14.2f, 7.6f);                // cupid's bow dip
        lips.quadraticTo (18.5f, 5.4f, 21.5f, 12.0f);               // right bump
        lips.quadraticTo (12.0f, 19.8f, 2.5f, 12.0f);               // full lower lip
        lips.closeSubPath();
        g.setColour (DataCogsLookAndFeel::text);                  // brighter, it pops
        g.fillPath (lips);

        juce::Path kissLine;                                        // the pucker
        kissLine.startNewSubPath (2.5f, 12.0f);
        kissLine.lineTo (21.5f, 12.0f);
        g.setColour (DataCogsLookAndFeel::background);
        g.strokePath (kissLine, juce::PathStrokeType (1.2f));
    }
    else if (icon == "mix")             // the kitchen kind
    {
        p.startNewSubPath (4.5f, 21.0f); p.lineTo (19.5f, 21.0f);   // base
        p.startNewSubPath (6.5f, 21.0f); p.lineTo (6.5f, 9.0f);     // column
        p.addRoundedRectangle (5.5f, 4.5f, 13.0f, 4.5f, 2.0f);      // head
        p.startNewSubPath (14.0f, 9.0f); p.lineTo (14.0f, 12.0f);   // shaft
        p.addEllipse (12.4f, 12.0f, 3.2f, 4.2f);                    // whisk
        p.addCentredArc (14.0f, 15.0f, 5.0f, 5.0f, 0.0f,            // bowl
                         juce::MathConstants<float>::halfPi,
                         juce::MathConstants<float>::pi * 1.5f, true);
        g.strokePath (p, stroke);
    }
    else if (icon == "range")           // home, home on the
    {
        p.addCentredArc (10.0f, 10.5f, 6.5f, 7.5f, 0.0f,            // canopy
                         -juce::MathConstants<float>::halfPi,
                         juce::MathConstants<float>::halfPi, true);
        p.addRectangle (2.0f, 10.5f, 17.0f, 3.5f);                  // bed
        p.startNewSubPath (19.0f, 12.2f); p.lineTo (23.0f, 14.5f);  // tongue, for the oxen
        p.addEllipse (3.2f, 14.0f, 7.6f, 7.6f);                     // big rear wheel
        p.addEllipse (14.4f, 16.4f, 5.2f, 5.2f);                    // small front wheel
        // spokes - what says "wagon" is the spoked wheels
        p.startNewSubPath (7.0f, 14.6f);  p.lineTo (7.0f, 21.0f);
        p.startNewSubPath (3.8f, 17.8f);  p.lineTo (10.2f, 17.8f);
        p.startNewSubPath (17.0f, 16.9f); p.lineTo (17.0f, 21.1f);
        p.startNewSubPath (14.9f, 19.0f); p.lineTo (19.1f, 19.0f);
        g.strokePath (p, stroke);
    }
}

// Hover text for the icons: what the parameter actually does, in plain
// words - the icons carry the joke, the tooltips carry the manual.
juce::String parameterTooltip (const juce::String& parameterID)
{
    if (parameterID == "threshold")
        return "Threshold: the level the signal must cross before compression starts. "
               "Lower it to compress more of the signal.";
    if (parameterID == "ratio")
        return "Ratio: how hard the signal is squeezed once it crosses the threshold. "
               "At 4:1, every 4 dB over the threshold comes out as just 1 dB.";
    if (parameterID == "attack")
        return "Attack: how quickly the compressor clamps down when the signal crosses "
               "the threshold. Slower attacks let transients punch through.";
    if (parameterID == "release")
        return "Release: how quickly the gain recovers once the signal falls back below "
               "the threshold. Slower releases sound smoother; faster ones pump.";
    if (parameterID == "kneeWidth")
        return "Knee: how gradually compression fades in around the threshold. "
               "0 dB is a hard corner; wider knees ease in more gently.";
    if (parameterID == "makeupGain")
        return "Makeup: output gain applied after compression, to bring the "
               "squashed signal back up to level.";
    if (parameterID == "mix")
        return "Mix: blend of compressed and dry signal. Below 100 % you get "
               "parallel (New York) compression.";
    if (parameterID == "range")
        return "Range: a ceiling on gain reduction - the compressor will never "
               "pull the level down by more than this, however hot the input.";
    return {};
}
} // namespace

void CompressorAudioProcessorEditor::ParameterIcon::setIcon (const juce::String& parameterID)
{
    icon = parameterID;
    setTooltip (parameterTooltip (parameterID));
}

void CompressorAudioProcessorEditor::ParameterIcon::paint (juce::Graphics& g)
{
    drawParameterIcon (g, icon, getLocalBounds().toFloat().reduced (0.0f, 1.0f));
}

//==============================================================================
StereoMeter::StereoMeter (CompressorAudioProcessor& processorIn)
    : processor (processorIn)
{
    startTimerHz (30);
}

void StereoMeter::timerCallback()
{
    ballistic (inDb[0],  juce::jlimit (minLevelDb, 0.0f, processor.getInputPeakDb (0)));
    ballistic (inDb[1],  juce::jlimit (minLevelDb, 0.0f, processor.getInputPeakDb (1)));
    ballistic (outDb[0], juce::jlimit (minLevelDb, 0.0f, processor.getOutputPeakDb (0)));
    ballistic (outDb[1], juce::jlimit (minLevelDb, 0.0f, processor.getOutputPeakDb (1)));
    ballistic (grDb,     juce::jlimit (0.0f, maxGrDb, processor.getGainReductionDb()));
    repaint();
}

void StereoMeter::drawLevelBar (juce::Graphics& g, juce::Rectangle<float> area, float levelDb) const
{
    g.setColour (DataCogsLookAndFeel::background);
    g.fillRoundedRectangle (area, 2.0f);

    // -60..0 dB mapped bottom-up.
    const float fraction = juce::jlimit (0.0f, 1.0f, (levelDb - minLevelDb) / -minLevelDb);
    auto bar = area.withTop (area.getBottom() - fraction * area.getHeight());
    g.setColour (DataCogsLookAndFeel::level);
    g.fillRoundedRectangle (bar, 2.0f);
}

void StereoMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (DataCogsLookAndFeel::panel);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (DataCogsLookAndFeel::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    auto area = bounds.reduced (8.0f, 6.0f);
    auto labelStrip = area.removeFromBottom (14.0f);

    // Three groups: IN (two bars), GR (one wide bar + scale), OUT (two bars).
    const float groupGap = 8.0f;
    const float grWidth = area.getWidth() * 0.34f;
    auto inArea  = area.removeFromLeft ((area.getWidth() - grWidth) / 2.0f - groupGap / 2.0f);
    auto outArea = area.removeFromRight (inArea.getWidth());
    auto grArea  = area.reduced (groupGap / 2.0f, 0.0f);

    // Input / output pairs.
    auto barPair = [&] (juce::Rectangle<float> group, const float* levels)
    {
        auto left = group.removeFromLeft (group.getWidth() / 2.0f).reduced (1.5f, 0.0f);
        auto right = group.reduced (1.5f, 0.0f);
        drawLevelBar (g, left, levels[0]);
        drawLevelBar (g, right, levels[1]);
    };
    barPair (inArea, inDb);
    barPair (outArea, outDb);

    // GR bar with its dB scale. Hangs from the top: silence = empty.
    auto grScale = grArea.removeFromRight (14.0f);
    g.setColour (DataCogsLookAndFeel::background);
    g.fillRoundedRectangle (grArea, 2.0f);

    g.setFont (juce::FontOptions (9.0f));
    for (float db = 0.0f; db <= maxGrDb; db += 6.0f)
    {
        const float yPos = grArea.getY() + (db / maxGrDb) * grArea.getHeight();
        g.setColour (DataCogsLookAndFeel::outline);
        g.drawHorizontalLine ((int) yPos, grArea.getX(), grArea.getRight());
        g.setColour (DataCogsLookAndFeel::textDim);
        g.drawText (juce::String ((int) db), (int) grScale.getX() + 2, (int) yPos - 4,
                    (int) grScale.getWidth(), 9, juce::Justification::centredLeft);
    }

    auto grBar = grArea.withHeight ((grDb / maxGrDb) * grArea.getHeight());
    g.setColour (DataCogsLookAndFeel::accent);
    g.fillRoundedRectangle (grBar, 2.0f);

    // Group labels.
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.setColour (DataCogsLookAndFeel::textDim);
    g.drawText ("IN",  inArea.withY (labelStrip.getY()).withHeight (labelStrip.getHeight()).toNearestInt(), juce::Justification::centred);
    g.drawText ("GR",  grArea.withY (labelStrip.getY()).withHeight (labelStrip.getHeight()).toNearestInt(), juce::Justification::centred);
    g.drawText ("OUT", outArea.withY (labelStrip.getY()).withHeight (labelStrip.getHeight()).toNearestInt(), juce::Justification::centred);
}

//==============================================================================
CompressorAudioProcessorEditor::CompressorAudioProcessorEditor (CompressorAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), meter (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (DataCogsAssets::datacogs_logo_png,
                                            DataCogsAssets::datacogs_logo_pngSize);

    setupKnob (threshold, "threshold",  "Threshold", " dB");
    setupKnob (ratio,     "ratio",      "Ratio",     ":1");
    setupKnob (attack,    "attack",     "Attack",    " ms");
    setupKnob (release,   "release",    "Release",   " ms");
    setupKnob (knee,      "kneeWidth",  "Knee",      " dB");
    setupKnob (makeup,    "makeupGain", "Makeup",    " dB");
    setupKnob (mix,       "mix",        "Mix",       " %");
    setupKnob (range,     "range",      "Range",     " dB");

    // Preset box: not a parameter - it drives the program API, which
    // applies a whole parameter snapshot at once.
    for (int i = 0; i < (int) CompressorAudioProcessor::getFactoryPresets().size(); ++i)
        presetBox.addItem (CompressorAudioProcessor::getFactoryPresets()[(size_t) i].name, i + 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            processorRef.setCurrentProgram (index);
    };
    addAndMakeVisible (presetBox);

    setupChoiceBox (topologyBox, "topology", topologyAttachment, topologyCaption, "Topology");
    setupChoiceBox (detectorBox, "detector", detectorAttachment, detectorCaption, "Detector");
    setupChoiceBox (scHpfBox, "scHpf", scHpfAttachment, scHpfCaption, "Sidechain HPF");

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getValueTreeState(), "bypass", bypassButton);
    addAndMakeVisible (bypassButton);

    resetButton.onClick = [this]
    {
        processorRef.resetParametersToDefaults();
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (resetButton);

    addAndMakeVisible (meter);

    // Sized so the knob cells have barely more height than width - any
    // taller and dead space opens up between the pictograms and the knobs
    // (the rotary drawing centres itself vertically in its cell).
    setSize (960, 310);
}

CompressorAudioProcessorEditor::~CompressorAudioProcessorEditor()
{
    // The LookAndFeel is a member, destroyed with this editor - JUCE must
    // not be left holding a pointer to it.
    setLookAndFeel (nullptr);
}

void CompressorAudioProcessorEditor::setupKnob (Knob& knob, const juce::String& parameterID,
                                                const juce::String& title, const juce::String& suffix)
{
    knob.icon.setIcon (parameterID);
    addAndMakeVisible (knob.icon);

    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // 22px tall so the in-place editor's text isn't crushed against the
    // plate edges when a value is double-clicked for editing.
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 22);
    knob.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (knob.slider);

    knob.label.setText (title, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    knob.label.setColour (juce::Label::textColourId, DataCogsLookAndFeel::text);
    addAndMakeVisible (knob.label);

    // The attachment is created LAST, once the slider exists and is
    // configured: it immediately pushes the parameter's current value and
    // range into the slider. It also installs a textFromValueFunction that
    // delegates ALL value formatting to the parameter - so display
    // precision is controlled by the parameter ranges' step intervals (see
    // the processor), not by any Slider setting. Slider::
    // setNumDecimalPlacesToDisplay is silently ignored once an attachment
    // exists; don't be the next person to try it.
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getValueTreeState(), parameterID, knob.slider);
}

void CompressorAudioProcessorEditor::setupChoiceBox (juce::ComboBox& box, const juce::String& parameterID,
                                                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment,
                                                     juce::Label& caption, const juce::String& captionText)
{
    caption.setText (captionText, juce::dontSendNotification);
    caption.setFont (juce::FontOptions (10.0f));
    addAndMakeVisible (caption);

    // Items must be added in choice order with sequential IDs starting at 1
    // - that is the contract the ComboBoxAttachment maps onto the
    // parameter's 0-based choice index.
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            processorRef.getValueTreeState().getParameter (parameterID)))
    {
        int id = 1;
        for (const auto& item : choice->choices)
            box.addItem (item, id++);
    }

    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getValueTreeState(), parameterID, box);
}

void CompressorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (DataCogsLookAndFeel::background);

    auto header = getLocalBounds().removeFromTop (40);

    // Gears logo, then the wordmark: bright "DATACOGS", dim "COMPRESSOR".
    auto brand = header.withTrimmedLeft (14);
    if (logo.isValid())
    {
        const int logoHeight = 28;
        const int logoWidth = logoHeight * logo.getWidth() / logo.getHeight();
        g.drawImageWithin (logo, brand.getX(), header.getCentreY() - logoHeight / 2,
                           logoWidth, logoHeight, juce::RectanglePlacement::centred);
        brand.removeFromLeft (logoWidth + 10);
    }

    const juce::Font brandFont { juce::FontOptions (15.0f, juce::Font::bold) };
    g.setFont (brandFont);
    g.setColour (DataCogsLookAndFeel::text);
    g.drawText ("DATACOGS", brand, juce::Justification::centredLeft);

    juce::GlyphArrangement measure;
    measure.addLineOfText (brandFont, "DATACOGS", 0.0f, 0.0f);
    brand.removeFromLeft ((int) std::ceil (measure.getBoundingBox (0, -1, true).getWidth()) + 8);
    g.setColour (DataCogsLookAndFeel::textDim);
    g.drawText ("COMPRESSOR", brand, juce::Justification::centredLeft);

    g.setColour (DataCogsLookAndFeel::outline);
    g.drawHorizontalLine (header.getBottom(), 0.0f, (float) getWidth());
}

void CompressorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header row: title painted on the left, then the house-standard
    // right-aligned trio shared by all DataCogs plugins.
    auto headerRow = area.removeFromTop (40).reduced (12, 7);
    bypassButton.setBounds (headerRow.removeFromRight (80));
    headerRow.removeFromRight (8);
    resetButton.setBounds (headerRow.removeFromRight (64));
    headerRow.removeFromRight (8);
    presetBox.setBounds (headerRow.removeFromRight (170));

    // Control strip: topology | detector | sidechain HPF.
    // Each combo gets a small caption on top so nothing is a mystery box.
    auto strip = area.removeFromTop (52).reduced (12, 3);
    auto captionRow = strip.removeFromTop (14);
    auto placeBox = [&] (juce::ComboBox& box, juce::Label& caption, int width)
    {
        box.setBounds (strip.removeFromLeft (width).reduced (0, 2));
        caption.setBounds (captionRow.removeFromLeft (width));
        strip.removeFromLeft (10);
        captionRow.removeFromLeft (10);
    };
    placeBox (topologyBox, topologyCaption, 130);
    placeBox (detectorBox, detectorCaption, 100);
    placeBox (scHpfBox, scHpfCaption, 110);

    area.reduce (12, 10);

    // Meter block on the right; knobs share the rest equally.
    meter.setBounds (area.removeFromRight (170));
    area.removeFromRight (12);

    const auto knobs = knobList();
    const int knobWidth = area.getWidth() / (int) knobs.size();

    for (auto* knob : knobs)
    {
        auto cell = area.removeFromLeft (knobWidth);
        knob->label.setBounds (cell.removeFromTop (18));
        knob->icon.setBounds (cell.removeFromTop (34));
        knob->slider.setBounds (cell);
    }
}
