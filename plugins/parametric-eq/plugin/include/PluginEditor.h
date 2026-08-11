#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DataCogsLookAndFeel.h"
#include "PluginProcessor.h"
#include "ResponseCurveComponent.h"

/**
 * Curve display on top (the primary interface - most EQ moves happen by
 * dragging nodes), a control strip for the selected band underneath. The
 * strip's attachments are torn down and rebuilt when the selection
 * changes: one set of controls, six targets.
 */
class ParametricEQAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ParametricEQAudioProcessorEditor (ParametricEQAudioProcessor&);
    ~ParametricEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Jokey engraved pictogram between label and knob (freq = radio tower,
    // gain = gym gains, ...), keyed by parameter ID. A real component (not
    // just editor paint) so it can carry a tooltip explaining what the
    // parameter actually does.
    class ParameterIcon : public juce::Component,
                          public juce::SettableTooltipClient
    {
    public:
        void setIcon (const juce::String& parameterID);
        void paint (juce::Graphics&) override;

    private:
        juce::String icon;
    };

    void attachToBand (int band);

    ParametricEQAudioProcessor& processorRef;
    DataCogsLookAndFeel lookAndFeel;
    juce::Image logo;   // DataCogs gears, from DataCogsAssets

    ResponseCurveComponent curve;

    // Selected-band strip.
    juce::TextButton bandButtons[ParametricEQAudioProcessor::numBands];
    juce::ToggleButton activeButton { "On" };
    juce::ComboBox typeBox;
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::Label freqLabel, gainLabel, qLabel;
    ParameterIcon freqIcon, gainIcon, qIcon;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        freqAttachment, gainAttachment, qAttachment;

    juce::Slider outGainSlider;
    juce::Label outGainLabel;
    ParameterIcon outGainIcon;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttachment;
    // House convention across the DataCogs plugins: title on the left,
    // top row right-aligned as Presets... -> Reset -> Bypass.
    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::TextButton resetButton { "Reset" };
    juce::ComboBox presetBox;

    // Shows the tooltips of any SettableTooltipClient in this editor
    // (currently the parameter icons).
    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessorEditor)
};
