//
//  PluginEditor.h
//  Compressor - Plugin Editor
//
//  Custom UI: six rotary knobs, a control strip (presets, topology,
//  detector mode, bypass) and stereo input/GR/output metering.
//
//  The two rules that keep a plugin UI honest:
//
//  1. Parameters flow through APVTS attachments, never directly. Slider,
//     ComboBox and Button attachments keep control <-> parameter in sync
//     in both directions, handle host automation, begin/end gesture
//     notification and undo - all the plumbing hand-rolled listeners get
//     subtly wrong. (The preset box is the one non-parameter control: it
//     drives the program API, which applies a parameter snapshot.)
//
//  2. The audio thread is never touched from here. The meters POLL atomics
//     the processor writes (30 Hz timer); nothing in this file calls into
//     the DSP, takes locks the audio thread shares, or allocates on its
//     behalf.
//

#pragma once

#include "PluginProcessor.h"
#include "DataCogsLookAndFeel.h"


//==============================================================================
// Stereo metering: input peak L/R, linked gain reduction, output peak L/R.
//
// The level bars run bottom-up over a -60..0 dB range; the GR bar hangs
// top-down (no compression = empty), because that is how gain reduction
// reads on hardware. GR is a single bar by design: detection is stereo-
// linked, so both channels always receive the identical gain - separate
// L/R GR bars would just draw the same value twice.
class StereoMeter : public juce::Component,
                    private juce::Timer
{
public:
    explicit StereoMeter (CompressorAudioProcessor& processorIn);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    // Peak-style ballistics for all bars: rise instantly so nothing is
    // missed, decay smoothly so the eye can follow.
    static void ballistic (float& displayed, float target)
    {
        if (target > displayed)
            displayed = target;
        else
            displayed += (target - displayed) * 0.18f;
    }

    void drawLevelBar (juce::Graphics&, juce::Rectangle<float> area, float levelDb) const;

    CompressorAudioProcessor& processor;
    float inDb[2]  { -60.0f, -60.0f };
    float outDb[2] { -60.0f, -60.0f };
    float grDb     { 0.0f };
    static constexpr float maxGrDb = 24.0f;   // GR scale bottom
    static constexpr float minLevelDb = -60.0f; // level scale floor
};

//==============================================================================
class CompressorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit CompressorAudioProcessorEditor (CompressorAudioProcessor&);
    ~CompressorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Jokey engraved pictogram between label and knob (threshold = bride
    // carried over one, attack = knife guy, ...), keyed by parameter ID.
    // A real component (not just editor paint) so it can carry a tooltip
    // explaining what the parameter actually does.
    class ParameterIcon : public juce::Component,
                          public juce::SettableTooltipClient
    {
    public:
        void setIcon (const juce::String& parameterID);
        void paint (juce::Graphics&) override;

    private:
        juce::String icon;
    };

    // A labelled knob bound to one parameter. The attachment must outlive
    // neither the slider nor the parameter tree, which is why it lives
    // beside the slider it serves and is destroyed with the editor.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        ParameterIcon icon;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& parameterID,
                    const juce::String& title, const juce::String& suffix);
    void setupChoiceBox (juce::ComboBox&, const juce::String& parameterID,
                         std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&,
                         juce::Label& caption, const juce::String& captionText);

    CompressorAudioProcessor& processorRef;
    DataCogsLookAndFeel lookAndFeel;
    juce::Image logo;   // DataCogs gears, from DataCogsAssets

    Knob threshold, ratio, attack, release, knee, makeup, mix, range;

    std::array<Knob*, 8> knobList() noexcept
    {
        return { &threshold, &ratio, &attack, &release, &knee, &makeup, &mix, &range };
    }

    // Control strip
    juce::ComboBox presetBox;    // drives the program API (not a parameter)
    juce::ComboBox topologyBox;  // Classic / Log-Domain
    juce::ComboBox detectorBox;  // Peak / RMS (Classic topology only)
    juce::ComboBox scHpfBox;     // sidechain high-pass: Off / 60 / 120 / 250 Hz
    juce::Label topologyCaption, detectorCaption, scHpfCaption;
    // House convention across the DataCogs plugins: title on the left,
    // top row right-aligned as Presets... -> Reset -> Bypass.
    juce::ToggleButton bypassButton { "Bypass" };
    juce::TextButton resetButton { "Reset" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> topologyAttachment, detectorAttachment, scHpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    StereoMeter meter;

    // Shows the tooltips of any SettableTooltipClient in this editor
    // (currently the parameter icons).
    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompressorAudioProcessorEditor)
};
