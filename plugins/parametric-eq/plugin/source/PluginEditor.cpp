#include "PluginEditor.h"
#include "DataCogsAssets.h"

namespace
{
constexpr int kEditorWidth = 900;
constexpr int kCurveHeight = 330;
constexpr int kHeaderHeight = 40;  // house standard: matches the Compressor's header
constexpr int kIconRowHeight = 34; // house standard: pictogram row between label and knob
constexpr int kEditorHeight = kHeaderHeight + kCurveHeight + 130 + kIconRowHeight + 30;

// Jokey engraved pictograms, one per knob: freq = a radio tower on the
// air, gain = gym gains, Q = the billiards kind of cue, output = the wall
// kind of outlet. Drawn on a 24x24 grid and scaled into place; stroke-only
// "etched faceplate" style so the gags whisper rather than shout. Keyed by
// parameter ID.
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

    if (icon == "freq")                 // broadcasting on all frequencies
    {
        p.startNewSubPath (7.0f, 21.5f);  p.lineTo (11.2f, 5.0f);    // tower legs
        p.startNewSubPath (17.0f, 21.5f); p.lineTo (12.8f, 5.0f);
        p.startNewSubPath (8.4f, 16.5f);  p.lineTo (15.6f, 16.5f);   // cross braces
        p.startNewSubPath (9.7f, 11.0f);  p.lineTo (14.3f, 11.0f);
        // Waves rippling out both sides of the beacon.
        p.addCentredArc (12.0f, 3.4f, 3.6f, 3.6f, 0.0f, 0.8f, 2.3f, true);
        p.addCentredArc (12.0f, 3.4f, 6.2f, 6.2f, 0.0f, 0.8f, 2.3f, true);
        p.addCentredArc (12.0f, 3.4f, 3.6f, 3.6f, 0.0f, -0.8f, -2.3f, true);
        p.addCentredArc (12.0f, 3.4f, 6.2f, 6.2f, 0.0f, -0.8f, -2.3f, true);
        g.strokePath (p, stroke);
        g.fillEllipse (10.6f, 2.0f, 2.8f, 2.8f);                     // the beacon
    }
    else if (icon == "gain")            // the financial kind of gains
    {
        p.startNewSubPath (2.5f, 8.5f);                              // the bill, mid-flutter
        p.quadraticTo (7.0f, 6.4f, 12.0f, 8.2f);
        p.quadraticTo (17.0f, 10.0f, 21.5f, 7.9f);
        p.lineTo (21.5f, 15.5f);
        p.quadraticTo (17.0f, 17.6f, 12.0f, 15.8f);
        p.quadraticTo (7.0f, 14.0f, 2.5f, 16.1f);
        p.closeSubPath();
        g.strokePath (p, stroke);
        g.fillEllipse (8.9f, 8.9f, 6.2f, 6.2f);                      // the seal
        juce::Path dollar;                                           // and the point of it
        dollar.startNewSubPath (13.5f, 10.3f);
        dollar.cubicTo (10.3f, 9.8f, 10.3f, 12.0f, 12.0f, 12.0f);
        dollar.cubicTo (13.7f, 12.0f, 13.7f, 14.2f, 10.5f, 13.7f);
        dollar.startNewSubPath (12.0f, 9.3f);
        dollar.lineTo (12.0f, 14.7f);
        g.setColour (DataCogsLookAndFeel::background);
        g.strokePath (dollar, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
    }
    else if (icon == "q")               // Q branch, MI6 - gadgets are down the hall
    {
        // 00-agent pictogram: trilby, shades, bowtie and tux lapels.
        juce::Path hat;
        hat.addRoundedRectangle (7.0f, 3.2f, 10.0f, 4.8f, 1.6f);     // trilby crown
        hat.addRectangle (4.0f, 7.4f, 16.0f, 1.8f);                  // brim
        g.fillPath (hat);
        g.fillRoundedRectangle (8.2f, 10.4f, 7.6f, 2.4f, 1.2f);      // the shades
        juce::Path bowtie;
        bowtie.addTriangle (9.0f, 14.2f, 9.0f, 16.8f, 11.5f, 15.5f);
        bowtie.addTriangle (15.0f, 14.2f, 15.0f, 16.8f, 12.5f, 15.5f);
        g.fillPath (bowtie);
        p.startNewSubPath (6.6f, 14.2f); p.lineTo (10.2f, 17.6f);    // tux lapels
        p.lineTo (10.6f, 21.0f);
        p.startNewSubPath (17.4f, 14.2f); p.lineTo (13.8f, 17.6f);
        p.lineTo (13.4f, 21.0f);
        p.startNewSubPath (6.6f, 14.2f); p.lineTo (5.2f, 21.0f);     // jacket sides
        p.startNewSubPath (17.4f, 14.2f); p.lineTo (18.8f, 21.0f);
        g.strokePath (p, stroke);
    }
    else if (icon == "outGain")         // an output you can plug into
    {
        // A US wall outlet: two slots and a ground hole, pulling the
        // classic surprised face.
        p.addRoundedRectangle (5.0f, 3.0f, 14.0f, 18.0f, 2.0f);      // faceplate
        g.strokePath (p, stroke);
        g.fillRect (8.6f, 7.6f, 1.8f, 4.0f);                         // the slots (eyes)
        g.fillRect (13.6f, 7.6f, 1.8f, 4.0f);
        g.fillEllipse (10.6f, 14.4f, 2.8f, 3.2f);                    // ground hole (mouth)
        juce::Path screw;                                            // faceplate screw
        screw.addEllipse (11.3f, 4.4f, 1.4f, 1.4f);
        g.strokePath (screw, juce::PathStrokeType (0.8f));
    }
}

// Hover text for the icons: what the parameter actually does, in plain
// words - the icons carry the joke, the tooltips carry the manual.
juce::String parameterTooltip (const juce::String& parameterID)
{
    if (parameterID == "freq")
        return "Frequency: where on the spectrum the selected band works, 20 Hz to "
               "20 kHz. Dragging the band's node in the display is the same move.";
    if (parameterID == "gain")
        return "Gain: how much the selected band boosts or cuts, up to +/-24 dB. "
               "Only Bell and Shelf bands use it - pass and notch filters have no gain.";
    if (parameterID == "q")
        return "Q: how narrow the selected band is. Low Q brushes a broad range of "
               "frequencies; high Q picks out just one. 0.707 is the neutral default.";
    if (parameterID == "outGain")
        return "Output: level trim after all six bands, to put back (or take off) "
               "whatever level the EQ changed overall.";
    return {};
}
} // namespace

void ParametricEQAudioProcessorEditor::ParameterIcon::setIcon (const juce::String& parameterID)
{
    icon = parameterID;
    setTooltip (parameterTooltip (parameterID));
}

void ParametricEQAudioProcessorEditor::ParameterIcon::paint (juce::Graphics& g)
{
    drawParameterIcon (g, icon, getLocalBounds().toFloat().reduced (0.0f, 1.0f));
}

//==============================================================================
ParametricEQAudioProcessorEditor::ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), curve (p)
{
    setLookAndFeel (&lookAndFeel);

    logo = juce::ImageCache::getFromMemory (DataCogsAssets::datacogs_logo_png,
                                            DataCogsAssets::datacogs_logo_pngSize);

    curve.onBandSelected = [this] (int band) { attachToBand (band); };
    addAndMakeVisible (curve);

    for (int i = 0; i < ParametricEQAudioProcessor::numBands; ++i)
    {
        bandButtons[i].setButtonText (juce::String (i + 1));
        bandButtons[i].onClick = [this, i] { curve.setSelectedBand (i); };
        addAndMakeVisible (bandButtons[i]);
    }

    addAndMakeVisible (activeButton);

    typeBox.addItemList ({ "Bell", "Low Shelf", "High Shelf", "High-Pass", "Low-Pass", "Notch" }, 1);
    addAndMakeVisible (typeBox);

    auto setupSlider = [this] (juce::Slider& slider, juce::Label& label, ParameterIcon& icon,
                               const juce::String& text, const juce::String& iconID)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        addAndMakeVisible (slider);
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, DataCogsLookAndFeel::text);
        addAndMakeVisible (label);
        icon.setIcon (iconID);
        addAndMakeVisible (icon);
    };
    setupSlider (freqSlider, freqLabel, freqIcon, "Freq", "freq");
    setupSlider (gainSlider, gainLabel, gainIcon, "Gain", "gain");
    setupSlider (qSlider, qLabel, qIcon, "Q", "q");
    setupSlider (outGainSlider, outGainLabel, outGainIcon, "Output", "outGain");

    outGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getValueTreeState(), "outGain", outGainSlider);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getValueTreeState(), "bypass", bypassButton);
    addAndMakeVisible (bypassButton);

    resetButton.onClick = [this]
    {
        processorRef.resetParametersToDefaults();
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (resetButton);

    presetBox.setTextWhenNothingSelected ("Presets...");
    for (int i = 0; i < processorRef.getNumPrograms(); ++i)
        presetBox.addItem (processorRef.getProgramName (i), i + 1);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            processorRef.setCurrentProgram (index);
    };
    addAndMakeVisible (presetBox);

    attachToBand (curve.getSelectedBand());
    setSize (kEditorWidth, kEditorHeight);
}

ParametricEQAudioProcessorEditor::~ParametricEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void ParametricEQAudioProcessorEditor::attachToBand (int band)
{
    auto& apvts = processorRef.getValueTreeState();

    // Attachments must die before being repointed - destruction order is
    // the whole reason these are unique_ptrs.
    activeAttachment.reset();
    typeAttachment.reset();
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();

    activeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParametricEQAudioProcessor::bandParamID (band, "Active"), activeButton);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        apvts, ParametricEQAudioProcessor::bandParamID (band, "Type"), typeBox);
    freqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParametricEQAudioProcessor::bandParamID (band, "Freq"), freqSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParametricEQAudioProcessor::bandParamID (band, "Gain"), gainSlider);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParametricEQAudioProcessor::bandParamID (band, "Q"), qSlider);

    for (int i = 0; i < ParametricEQAudioProcessor::numBands; ++i)
        bandButtons[i].setToggleState (i == band, juce::dontSendNotification);
}

//==============================================================================
void ParametricEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (DataCogsLookAndFeel::background);

    auto header = getLocalBounds().removeFromTop (kHeaderHeight);

    // Gears logo, then the wordmark: bright "DATACOGS", dim "PARAMETRIC EQ".
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
    g.drawText ("PARAMETRIC EQ", brand, juce::Justification::centredLeft);

    // House-standard header separator (all DataCogs plugins). Drawn one
    // pixel INSIDE the header band: the response curve is an opaque child
    // whose bounds start exactly at kHeaderHeight, so a line at the usual
    // bottom edge would be painted over and never seen.
    g.setColour (DataCogsLookAndFeel::outline);
    g.drawHorizontalLine (kHeaderHeight - 1, 0.0f, static_cast<float> (getWidth()));
}

void ParametricEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header row: title painted on the left, then the house-standard
    // right-aligned trio shared by all DataCogs plugins.
    auto topRow = area.removeFromTop (kHeaderHeight).reduced (14, 7);
    bypassButton.setBounds (topRow.removeFromRight (80));
    topRow.removeFromRight (8);
    resetButton.setBounds (topRow.removeFromRight (64));
    topRow.removeFromRight (8);
    presetBox.setBounds (topRow.removeFromRight (170));

    curve.setBounds (area.removeFromTop (kCurveHeight));

    auto strip = area.reduced (14, 8);

    auto buttonColumn = strip.removeFromLeft (120);
    const int buttonHeight = buttonColumn.getHeight() / 3;
    for (int i = 0; i < ParametricEQAudioProcessor::numBands; ++i)
    {
        auto row = juce::Rectangle<int> (buttonColumn.getX() + (i % 2) * 60,
                                         buttonColumn.getY() + (i / 2) * buttonHeight,
                                         56, buttonHeight - 4);
        bandButtons[i].setBounds (row);
    }

    strip.removeFromLeft (12);
    auto controlColumn = strip.removeFromLeft (140);
    activeButton.setBounds (controlColumn.removeFromTop (26));
    controlColumn.removeFromTop (6);
    typeBox.setBounds (controlColumn.removeFromTop (26));

    strip.removeFromLeft (12);
    auto knobArea = strip;
    const int cell = knobArea.getWidth() / 4;
    juce::Slider* sliders[] = { &freqSlider, &gainSlider, &qSlider, &outGainSlider };
    juce::Label* labels[] = { &freqLabel, &gainLabel, &qLabel, &outGainLabel };
    ParameterIcon* icons[] = { &freqIcon, &gainIcon, &qIcon, &outGainIcon };
    for (int i = 0; i < 4; ++i)
    {
        auto cellArea = knobArea.removeFromLeft (cell);
        labels[i]->setBounds (cellArea.removeFromTop (16));
        icons[i]->setBounds (cellArea.removeFromTop (kIconRowHeight));
        // House-standard knob geometry, same as the compressor: a 94 px
        // wide slider box (22 px of it the value plate) draws the 82 px cog.
        sliders[i]->setBounds (cellArea.withSizeKeepingCentre (
            juce::jmin (cellArea.getWidth(), 94), juce::jmin (cellArea.getHeight(), 116)));
    }
}
