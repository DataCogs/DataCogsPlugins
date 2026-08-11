#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DataCogsLookAndFeel.h"
#include "PluginProcessor.h"

/**
 * Ten rotary knobs (with the house pictogram-and-tooltip strip) bound
 * through APVTS attachments (the attachment installs a
 * textFromValueFunction that delegates formatting to the parameter -
 * precision comes from the range's step interval), an IR picker fed from
 * the local IR library folder, and a free-file loader.
 */
class ConvolutionReverbAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ConvolutionReverbAudioProcessorEditor (ConvolutionReverbAudioProcessor&);
    ~ConvolutionReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Where the plugin looks for installed IR wavs (delegates to the
    /// processor, which shares the location with the factory presets).
    static juce::File getIrLibraryFolder();
    static juce::String libraryRelativePath (const juce::File&);

    /// Favourites persist across sessions and plugin instances as
    /// library-relative paths, one per line, in favourites.txt inside the
    /// IR library folder - human-editable, and it travels with the IRs.
    static juce::StringArray loadFavourites();
    static void saveFavourites (const juce::StringArray& relPaths);

private:
    // Jokey engraved pictogram between label and knob (pre-delay = snail,
    // decay = tooth with a cavity, ...), keyed by parameter ID. A real
    // component (not just editor paint) so it can carry a tooltip
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

    void setupKnob (juce::Slider& slider, juce::Label& label, ParameterIcon& icon,
                    const juce::String& text, const juce::String& paramID,
                    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment);
    void refreshIrList();
    void openIrFileChooser();
    void toggleFavouriteForCurrentIr();
    void updateFavouriteButton();

    /// Shows a photo of the loaded space, Altiverb-style: an image file
    /// sitting next to the IR with the same basename (.jpg/.jpeg/.png) is
    /// picked up automatically, so users can photograph their own captures.
    void updateIrPhoto();

    ConvolutionReverbAudioProcessor& processorRef;

    // Declared before the components so it outlives them; the editor also
    // detaches it in the destructor per LookAndFeel rules.
    DataCogsLookAndFeel lookAndFeel;
    juce::Image logo;   // DataCogs gears, from DataCogsAssets

    juce::Slider mixSlider, preDelaySlider, sizeSlider, decaySlider, widthSlider,
        earlySlider, tailSlider, lowCutSlider, highCutSlider, gainSlider;
    juce::Label mixLabel, preDelayLabel, sizeLabel, decayLabel, widthLabel,
        earlyLabel, tailLabel, lowCutLabel, highCutLabel, gainLabel;
    ParameterIcon mixIcon, preDelayIcon, sizeIcon, decayIcon, widthIcon,
        earlyIcon, tailIcon, lowCutIcon, highCutIcon, gainIcon;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAttachment, preDelayAttachment, sizeAttachment, decayAttachment, widthAttachment,
        earlyAttachment, tailAttachment, lowCutAttachment, highCutAttachment, gainAttachment;

    juce::ToggleButton bypassButton { "Bypass" };
    juce::ToggleButton reverseButton { "Reverse" };
    juce::TextButton resetButton { "Reset" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        bypassAttachment, reverseAttachment;

    juce::ComboBox irBox;
    std::vector<juce::File> irFiles; // parallel to irBox item ids (id = index + 2)
    juce::TextButton loadIrButton { "Load IR..." };
    juce::TextButton favouriteButton; // star toggle for the current IR
    juce::ComboBox presetBox;         // factory presets via the program API
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::ImageComponent irPhoto;
    juce::Label noPhotoLabel; // shown when the IR has no sibling image

    // Shows the tooltips of any SettableTooltipClient in this editor
    // (the parameter icons, plus the buttons that set one).
    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionReverbAudioProcessorEditor)
};
