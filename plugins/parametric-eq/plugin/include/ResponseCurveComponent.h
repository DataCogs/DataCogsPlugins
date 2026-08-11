#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

/**
 * The EQ's main display: post-EQ spectrum behind, combined frequency
 * response in front, one draggable node per band.
 *
 * Interaction (the Pro-Q conventions everyone's hands already know):
 *   - drag a node        -> frequency (x, log) and gain (y)
 *   - mouse wheel on it  -> Q
 *   - double-click       -> toggle the band active/inactive
 *   - click empty space  -> select the nearest band
 *
 * Dragging writes parameters through begin/endChangeGesture so host
 * automation records correctly; the curve itself repaints from the same
 * parameter atomics the audio thread reads, so it can never disagree with
 * the sound.
 */
class ResponseCurveComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit ResponseCurveComponent (ParametricEQAudioProcessor&);
    ~ResponseCurveComponent() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    int getSelectedBand() const { return selectedBand; }
    void setSelectedBand (int band);
    std::function<void (int)> onBandSelected;

    // Display range: 20 Hz..20 kHz log, ±26 dB (a little past the ±24 knobs).
    static constexpr float minDb = -26.0f, maxDb = 26.0f;
    static constexpr double minHz = 20.0, maxHz = 20000.0;

private:
    void timerCallback() override;

    float frequencyToX (double hz) const;
    double xToFrequency (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    juce::Point<float> nodePosition (int band) const;
    int findNodeAt (juce::Point<float> position) const;

    void beginBandGesture (int band);
    void endBandGesture();

    ParametricEQAudioProcessor& processor;
    int selectedBand = 2; // band 3, the first "workhorse" bell
    int draggedBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseCurveComponent)
};
