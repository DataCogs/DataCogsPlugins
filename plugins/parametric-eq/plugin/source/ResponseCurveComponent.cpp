#include "ResponseCurveComponent.h"
#include "DataCogsLookAndFeel.h"

namespace
{
constexpr float kNodeRadius = 6.0f;
constexpr float kNodeHitRadius = 12.0f;

// One colour per band so node, curve segment ownership and the strip below
// can agree. Derived from the house palette's level blue by hue rotation:
// six distinct hues that keep its muted saturation, so the set sits in the
// machined-steel look instead of shouting over it.
juce::Colour bandColour (int band)
{
    return DataCogsLookAndFeel::level.withRotatedHue (
        static_cast<float> (band) / static_cast<float> (ParametricEQAudioProcessor::numBands));
}
} // namespace

//==============================================================================
ResponseCurveComponent::ResponseCurveComponent (ParametricEQAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
    setOpaque (true);
}

ResponseCurveComponent::~ResponseCurveComponent()
{
    endBandGesture();
}

void ResponseCurveComponent::timerCallback()
{
    // Analyzer frames arrive from the audio thread; parameters can change
    // from anywhere. 30 Hz repaint covers both without dirty-tracking.
    processor.getAnalyzer().refresh();
    repaint();
}

//==============================================================================
float ResponseCurveComponent::frequencyToX (double hz) const
{
    const double proportion = std::log (hz / minHz) / std::log (maxHz / minHz);
    return static_cast<float> (proportion) * static_cast<float> (getWidth());
}

double ResponseCurveComponent::xToFrequency (float x) const
{
    const double proportion = juce::jlimit (0.0, 1.0, static_cast<double> (x) / getWidth());
    return minHz * std::pow (maxHz / minHz, proportion);
}

float ResponseCurveComponent::dbToY (float db) const
{
    return juce::jmap (db, maxDb, minDb, 0.0f, static_cast<float> (getHeight()));
}

float ResponseCurveComponent::yToDb (float y) const
{
    return juce::jmap (y, 0.0f, static_cast<float> (getHeight()), maxDb, minDb);
}

juce::Point<float> ResponseCurveComponent::nodePosition (int band) const
{
    auto& apvts = processor.getValueTreeState();
    const float freq = apvts.getRawParameterValue (
        ParametricEQAudioProcessor::bandParamID (band, "Freq"))->load();
    const int type = static_cast<int> (apvts.getRawParameterValue (
        ParametricEQAudioProcessor::bandParamID (band, "Type"))->load());

    // Gainless types (HP/LP/notch) pin their node to the 0 dB line; the
    // vertical axis means nothing for them.
    float db = 0.0f;
    if (type <= 2) // bell, shelves
        db = apvts.getRawParameterValue (
            ParametricEQAudioProcessor::bandParamID (band, "Gain"))->load();

    return { frequencyToX (freq), dbToY (db) };
}

int ResponseCurveComponent::findNodeAt (juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = kNodeHitRadius;
    for (int band = 0; band < ParametricEQAudioProcessor::numBands; ++band)
    {
        const float distance = nodePosition (band).getDistanceFrom (position);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = band;
        }
    }
    return best;
}

void ResponseCurveComponent::setSelectedBand (int band)
{
    selectedBand = juce::jlimit (0, ParametricEQAudioProcessor::numBands - 1, band);
    if (onBandSelected)
        onBandSelected (selectedBand);
    repaint();
}

//==============================================================================
void ResponseCurveComponent::beginBandGesture (int band)
{
    auto& apvts = processor.getValueTreeState();
    apvts.getParameter (ParametricEQAudioProcessor::bandParamID (band, "Freq"))->beginChangeGesture();
    apvts.getParameter (ParametricEQAudioProcessor::bandParamID (band, "Gain"))->beginChangeGesture();
    draggedBand = band;
}

void ResponseCurveComponent::endBandGesture()
{
    if (draggedBand < 0)
        return;
    auto& apvts = processor.getValueTreeState();
    apvts.getParameter (ParametricEQAudioProcessor::bandParamID (draggedBand, "Freq"))->endChangeGesture();
    apvts.getParameter (ParametricEQAudioProcessor::bandParamID (draggedBand, "Gain"))->endChangeGesture();
    draggedBand = -1;
}

void ResponseCurveComponent::mouseDown (const juce::MouseEvent& e)
{
    const int hit = findNodeAt (e.position);
    if (hit >= 0)
    {
        setSelectedBand (hit);
        beginBandGesture (hit);
    }
    else
    {
        // Empty space: select the horizontally-nearest band, no drag.
        int nearest = 0;
        float bestDx = std::numeric_limits<float>::max();
        for (int band = 0; band < ParametricEQAudioProcessor::numBands; ++band)
        {
            const float dx = std::abs (nodePosition (band).x - e.position.x);
            if (dx < bestDx)
            {
                bestDx = dx;
                nearest = band;
            }
        }
        setSelectedBand (nearest);
    }
}

void ResponseCurveComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggedBand < 0)
        return;

    auto& apvts = processor.getValueTreeState();
    auto* freqParam = apvts.getParameter (
        ParametricEQAudioProcessor::bandParamID (draggedBand, "Freq"));
    auto* gainParam = apvts.getParameter (
        ParametricEQAudioProcessor::bandParamID (draggedBand, "Gain"));

    const float hz = static_cast<float> (xToFrequency (e.position.x));
    freqParam->setValueNotifyingHost (freqParam->convertTo0to1 (hz));

    const int type = static_cast<int> (apvts.getRawParameterValue (
        ParametricEQAudioProcessor::bandParamID (draggedBand, "Type"))->load());
    if (type <= 2) // vertical axis only means gain for bell/shelves
    {
        const float db = juce::jlimit (-24.0f, 24.0f, yToDb (e.position.y));
        gainParam->setValueNotifyingHost (gainParam->convertTo0to1 (db));
    }
}

void ResponseCurveComponent::mouseUp (const juce::MouseEvent&)
{
    endBandGesture();
}

void ResponseCurveComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = findNodeAt (e.position);
    if (hit < 0)
        return;
    auto* activeParam = processor.getValueTreeState().getParameter (
        ParametricEQAudioProcessor::bandParamID (hit, "Active"));
    activeParam->setValueNotifyingHost (activeParam->getValue() > 0.5f ? 0.0f : 1.0f);
}

void ResponseCurveComponent::mouseWheelMove (const juce::MouseEvent& e,
                                             const juce::MouseWheelDetails& wheel)
{
    const int hit = findNodeAt (e.position);
    const int band = hit >= 0 ? hit : selectedBand;

    // Multiplicative Q so the wheel feels uniform across the (log) range.
    auto* qParam = processor.getValueTreeState().getParameter (
        ParametricEQAudioProcessor::bandParamID (band, "Q"));
    auto* qRaw = processor.getValueTreeState().getRawParameterValue (
        ParametricEQAudioProcessor::bandParamID (band, "Q"));
    const float factor = std::pow (1.25f, wheel.deltaY > 0 ? 1.0f : -1.0f);
    if (wheel.deltaY != 0.0f)
        qParam->setValueNotifyingHost (qParam->convertTo0to1 (qRaw->load() * factor));
}

//==============================================================================
void ResponseCurveComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll (DataCogsLookAndFeel::background);

    // ---- Grid: decade frequencies and 6 dB lines ----
    g.setColour (DataCogsLookAndFeel::outline.withAlpha (0.4f));
    for (double hz : { 30.0, 50.0, 100.0, 200.0, 300.0, 500.0, 1000.0,
                       2000.0, 3000.0, 5000.0, 10000.0, 15000.0 })
        g.drawVerticalLine (juce::roundToInt (frequencyToX (hz)), 0.0f, bounds.getHeight());
    for (float db : { -24.0f, -18.0f, -12.0f, -6.0f, 6.0f, 12.0f, 18.0f, 24.0f })
        g.drawHorizontalLine (juce::roundToInt (dbToY (db)), 0.0f, bounds.getWidth());

    g.setColour (DataCogsLookAndFeel::textDim);
    g.setFont (juce::FontOptions (10.0f));
    for (double hz : { 100.0, 1000.0, 10000.0 })
        g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String ((int) hz),
                    juce::roundToInt (frequencyToX (hz)) + 3, getHeight() - 14, 40, 12,
                    juce::Justification::left);

    // 0 dB line, slightly brighter - it's the reference everything hangs off.
    g.setColour (DataCogsLookAndFeel::outline);
    g.drawHorizontalLine (juce::roundToInt (dbToY (0.0f)), 0.0f, bounds.getWidth());

    // ---- Spectrum (post-EQ), filled, behind the curve ----
    {
        auto& analyzer = processor.getAnalyzer();
        juce::Path spectrum;
        spectrum.startNewSubPath (0.0f, bounds.getBottom());
        const int numBins = SpectrumAnalyzer::fftSize / 2;
        for (int bin = 1; bin <= numBins; ++bin)
        {
            const double hz = analyzer.getBinFrequency (bin);
            if (hz < minHz || hz > maxHz)
                continue;
            // Analyzer floor -100 dB mapped onto the display's dB axis
            // compressed 4:1 so the spectrum lives in the lower half and
            // the EQ curve stays readable above it.
            const float db = analyzer.getBinDb (bin);
            const float y = dbToY (juce::jmap (db, -100.0f, 0.0f, minDb, 6.0f));
            spectrum.lineTo (frequencyToX (hz), juce::jlimit (0.0f, bounds.getBottom(), y));
        }
        spectrum.lineTo (bounds.getRight(), bounds.getBottom());
        spectrum.closeSubPath();
        g.setColour (DataCogsLookAndFeel::level.withAlpha (0.25f));
        g.fillPath (spectrum);
    }

    // ---- Combined response curve ----
    {
        const double fs = 48000.0; // display-only design rate; curves are
                                   // rate-invariant in the audible band
        std::array<eq::BiquadCoefficients, ParametricEQAudioProcessor::numBands> coefficients;
        std::array<bool, ParametricEQAudioProcessor::numBands> active {};
        for (int band = 0; band < ParametricEQAudioProcessor::numBands; ++band)
        {
            active[static_cast<size_t> (band)] = processor.isBandActive (band);
            if (active[static_cast<size_t> (band)])
                coefficients[static_cast<size_t> (band)] = processor.getBandCoefficients (band, fs);
        }

        juce::Path curve;
        for (int x = 0; x < getWidth(); ++x)
        {
            const double hz = xToFrequency (static_cast<float> (x));
            double magnitude = 1.0;
            for (int band = 0; band < ParametricEQAudioProcessor::numBands; ++band)
                if (active[static_cast<size_t> (band)])
                    magnitude *= eq::magnitudeAt (coefficients[static_cast<size_t> (band)], hz, fs);

            const float db = juce::Decibels::gainToDecibels (static_cast<float> (magnitude), minDb);
            const float y = juce::jlimit (-10.0f, bounds.getHeight() + 10.0f, dbToY (db));
            if (x == 0)
                curve.startNewSubPath (0.0f, y);
            else
                curve.lineTo (static_cast<float> (x), y);
        }
        g.setColour (DataCogsLookAndFeel::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    // ---- Band nodes ----
    for (int band = 0; band < ParametricEQAudioProcessor::numBands; ++band)
    {
        const auto position = nodePosition (band);
        const bool active = processor.isBandActive (band);
        const auto colour = bandColour (band).withAlpha (active ? 1.0f : 0.35f);

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (kNodeRadius * 2, kNodeRadius * 2).withCentre (position));
        if (band == selectedBand)
        {
            g.setColour (DataCogsLookAndFeel::text);
            g.drawEllipse (juce::Rectangle<float> (kNodeRadius * 2 + 6, kNodeRadius * 2 + 6)
                               .withCentre (position), 1.5f);
        }
        g.setColour (DataCogsLookAndFeel::background);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText (juce::String (band + 1),
                    juce::Rectangle<float> (kNodeRadius * 2, kNodeRadius * 2).withCentre (position),
                    juce::Justification::centred);
    }
}
