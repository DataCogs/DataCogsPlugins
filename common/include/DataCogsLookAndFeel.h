//
//  DataCogsLookAndFeel.h
//  DataCogs house look, shared by every plugin in the suite.
//
//  Machined-steel palette lifted from the gears logo: graphite ground,
//  gunmetal panels, silver text, polished-steel accent. Rotary sliders
//  become cog-toothed knobs that visibly turn with the value; labels are
//  stamped into the faceplate; slider value boxes become recessed milled
//  plates; combo boxes become selector plates with a steel chevron.
//
//  Per-plugin UI (parameter icons, meters, layout) stays in each plugin -
//  this class only owns what every DataCogs plugin shares.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DataCogsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DataCogsLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    // Palette shared by editors, meters and icons so each UI reads as one
    // piece - and so all plugins in the suite read as one family.
    static const juce::Colour background, panel, outline, text, textDim, accent, level;
};
