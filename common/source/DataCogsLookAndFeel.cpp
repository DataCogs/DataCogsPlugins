//
//  DataCogsLookAndFeel.cpp
//  DataCogs house look - see the header for what belongs here (and what
//  deliberately doesn't).
//

#include "DataCogsLookAndFeel.h"

#include <cmath>

//==============================================================================
// Palette: machined steel, sampled from the DataCogs gears logo. Graphite
// ground, gunmetal panels, silver text, polished-steel accent; the meters
// keep a cool steel-blue so level and gain reduction stay distinguishable.
const juce::Colour DataCogsLookAndFeel::background { 0xff141619 };
const juce::Colour DataCogsLookAndFeel::panel      { 0xff22262b };
const juce::Colour DataCogsLookAndFeel::outline    { 0xff414952 };
const juce::Colour DataCogsLookAndFeel::text       { 0xffd9dde2 };
const juce::Colour DataCogsLookAndFeel::textDim    { 0xff89909a };
const juce::Colour DataCogsLookAndFeel::accent     { 0xffc2cedb };
const juce::Colour DataCogsLookAndFeel::level      { 0xff7fa8c9 };

DataCogsLookAndFeel::DataCogsLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, textDim);

    // In-place value editing happens in a TextEditor overlaid on the value
    // plate - colour it like the plate so editing reads as "typing into
    // the same recess", not a stock white box.
    setColour (juce::TextEditor::backgroundColourId, background.darker (0.35f));
    setColour (juce::TextEditor::textColourId, accent);
    setColour (juce::TextEditor::outlineColourId, outline.withAlpha (0.55f));
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::Label::backgroundWhenEditingColourId, background.darker (0.35f));
    setColour (juce::Label::textWhenEditingColourId, accent);

    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);

    setColour (juce::TooltipWindow::backgroundColourId, panel.brighter (0.06f));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);

    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, background);
}

void DataCogsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle,
                                              float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcThickness = 3.0f;

    // Background track: the full sweep, drawn dim.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (outline);
    g.strokePath (track, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc: start of sweep up to the current position.
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Knob body: a cog, after the DataCogs logo. The teeth rotate with the
    // value so the gear visibly turns as the knob is dragged. Lit from a
    // fixed top-left so the whole thing reads as machined metal: drop
    // shadow underneath, extruded tooth sides, domed face, recessed bore.
    const auto toothTip  = radius - 5.0f;          // keep clear of the arc track
    const auto bodyRadius = radius * 0.72f;
    const auto bodyRect = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    const int numTeeth = 10;
    const float toothW = bodyRadius * 0.36f;
    // JUCE angles: 0 = 12 o'clock, clockwise. Light sits up-left of centre.
    const float lightAngle = -juce::MathConstants<float>::pi / 4.0f;

    auto toothPath = [&] (int i)
    {
        const float toothAngle = angle + (float) i * juce::MathConstants<float>::twoPi / (float) numTeeth;
        juce::Path tooth;
        tooth.addRectangle (-toothW / 2.0f, -toothTip, toothW, toothTip - bodyRadius + 2.0f);
        tooth.applyTransform (juce::AffineTransform::rotation (toothAngle)
                                  .translated (centre.x, centre.y));
        return std::pair<juce::Path, float> (std::move (tooth), toothAngle);
    };

    // Soft shadow under the whole cog silhouette.
    juce::Path cog;
    for (int i = 0; i < numTeeth; ++i)
        cog.addPath (toothPath (i).first);
    cog.addEllipse (bodyRect);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f),
                      juce::roundToInt (radius * 0.25f),
                      { juce::roundToInt (radius * 0.05f) + 1, juce::roundToInt (radius * 0.09f) + 1 })
        .drawForPath (g, cog);

    // Teeth: a dark extruded side face offset away from the light, then a
    // top face whose brightness follows how squarely it faces the light.
    for (int i = 0; i < numTeeth; ++i)
    {
        auto [tooth, toothAngle] = toothPath (i);

        juce::Path side (tooth);
        side.applyTransform (juce::AffineTransform::translation (1.6f, 2.2f));
        g.setColour (juce::Colour (0xff0e1013));
        g.fillPath (side);

        const float facing = 0.5f + 0.5f * std::cos (toothAngle - lightAngle);
        g.setColour (panel.brighter (0.10f + 0.55f * facing));
        g.fillPath (tooth);
    }

    // Domed face: radial gradient off-centre toward the light.
    const auto highlightCentre = centre.getPointOnCircumference (bodyRadius * 0.55f, lightAngle);
    juce::ColourGradient dome (panel.brighter (0.85f), highlightCentre.x, highlightCentre.y,
                               panel.brighter (0.02f),
                               centre.x - bodyRadius * 0.9f * std::sin (lightAngle),
                               centre.y + bodyRadius * 0.9f * std::cos (lightAngle), true);
    dome.addColour (0.45, panel.brighter (0.35f));
    g.setGradientFill (dome);
    g.fillEllipse (bodyRect);

    // Rim: bright arc on the lit side, dark arc opposite - the bevel.
    auto rimArc = [&] (float centreAngle, juce::Colour colour)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, bodyRadius - 0.7f, bodyRadius - 0.7f, 0.0f,
                           centreAngle - 1.4f, centreAngle + 1.4f, true);
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    };
    g.setColour (outline.darker (0.4f));
    g.drawEllipse (bodyRect, 1.0f);
    rimArc (lightAngle, juce::Colours::white.withAlpha (0.30f));
    rimArc (lightAngle + juce::MathConstants<float>::pi, juce::Colours::black.withAlpha (0.35f));

    // Hub bore, like the logo's open gear centres: recessed, so the inner
    // shadow falls on the lit side and the far lip catches the light.
    const auto boreRadius = bodyRadius * 0.32f;
    const auto boreRect = juce::Rectangle<float> (boreRadius * 2.0f, boreRadius * 2.0f).withCentre (centre);
    g.setColour (background.darker (0.35f));
    g.fillEllipse (boreRect);
    juce::Path boreShadow;
    boreShadow.addCentredArc (centre.x, centre.y, boreRadius - 0.6f, boreRadius - 0.6f, 0.0f,
                              lightAngle - 1.5f, lightAngle + 1.5f, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (boreShadow, juce::PathStrokeType (1.6f));
    juce::Path boreLip;
    boreLip.addCentredArc (centre.x, centre.y, boreRadius + 0.5f, boreRadius + 0.5f, 0.0f,
                           lightAngle + juce::MathConstants<float>::pi - 1.3f,
                           lightAngle + juce::MathConstants<float>::pi + 1.3f, true);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.strokePath (boreLip, juce::PathStrokeType (1.2f));

    const auto pointerEnd = centre.getPointOnCircumference (bodyRadius - 2.0f, angle);
    const auto pointerStart = centre.getPointOnCircumference (boreRadius + 2.0f, angle);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine ({ pointerStart.translated (1.0f, 1.4f), pointerEnd.translated (1.0f, 1.4f) }, 2.4f);
    g.setColour (accent);
    g.drawLine ({ pointerStart, pointerEnd }, 2.4f);
}


//==============================================================================
void DataCogsLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.isBeingEdited())
        return;   // the overlaid TextEditor draws itself

    const auto justification = label.getJustificationType();
    const auto alpha = label.isEnabled() ? 1.0f : 0.5f;

    // Slider value boxes read as recessed milled plates: dark inset,
    // shadow lip at the top, bright engraved digits.
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
    {
        auto plate = label.getLocalBounds().toFloat().reduced (2.0f, 0.5f);
        g.setColour (background.darker (0.35f));
        g.fillRoundedRectangle (plate, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawLine (plate.getX() + 3.0f, plate.getY() + 1.2f,
                    plate.getRight() - 3.0f, plate.getY() + 1.2f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawLine (plate.getX() + 3.0f, plate.getBottom() - 1.0f,
                    plate.getRight() - 3.0f, plate.getBottom() - 1.0f, 1.0f);
        g.setColour (outline.withAlpha (0.55f));
        g.drawRoundedRectangle (plate, 3.0f, 1.0f);

        g.setFont (label.getFont());
        g.setColour (accent.withAlpha (alpha));
        g.drawText (label.getText(), plate.reduced (3.0f, 0.0f), justification, false);
        return;
    }

    // Everything else is stamped into the faceplate: uppercase,
    // letterspaced, with a faint light catch on the lower edge of the
    // "engraving" (light comes from the top left, as on the knobs).
    const auto area = label.getBorderSize().subtractedFrom (label.getLocalBounds());
    const auto stamped = label.getText().toUpperCase();
    const auto font = label.getFont().withExtraKerningFactor (0.06f);
    g.setFont (font);

    g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
    g.drawText (stamped, area.translated (0, 1), justification, false);
    g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
    g.drawText (stamped, area, justification, false);
}

void DataCogsLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                          int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    // Selector plate: brushed vertical gradient with a machined top sheen.
    juce::ColourGradient face (panel.brighter (0.12f), 0.0f, 0.0f,
                               panel.darker (0.18f), 0.0f, (float) height, false);
    g.setGradientFill (face);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine (bounds.getX() + 3.0f, bounds.getY() + 1.2f,
                bounds.getRight() - 3.0f, bounds.getY() + 1.2f, 1.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Steel chevron instead of the stock arrow.
    const auto arrowZone = bounds.removeFromRight (26.0f);
    juce::Path chevron;
    chevron.startNewSubPath (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f);
    chevron.lineTo (arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    chevron.lineTo (arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f);
    g.setColour (accent.withAlpha (box.isEnabled() ? 0.9f : 0.4f));
    g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

