#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

/**
 * Post-EQ spectrum for the editor's display, using the standard JUCE
 * pattern: the audio thread drops mono-summed samples into a fixed FIFO
 * and raises a flag when a full FFT frame is ready; the editor's timer
 * windows + transforms it on the message thread. The hand-off is a single
 * atomic flag - if the UI is slow a frame is simply skipped, and the audio
 * thread never waits on anything.
 *
 * 4096 points at 48 kHz = ~11.7 Hz per bin: enough to separate bass notes
 * on screen, cheap enough to transform at 30 Hz without a thought.
 */
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder; // 4096

    SpectrumAnalyzer();

    void prepare (double sampleRate);

    /// Audio thread: push one block, summed to mono (an EQ display wants
    /// overall energy, and one FFT is cheaper than two).
    void pushSamples (const float* left, const float* right, int numSamples) noexcept;

    /// Message thread: if a frame is pending, transform it and update the
    /// smoothed display bins. Returns true when the display changed.
    bool refresh();

    /// Smoothed magnitude in dB for bin i (0..fftSize/2).
    float getBinDb (int bin) const { return displayDb[static_cast<size_t> (bin)]; }
    double getBinFrequency (int bin) const { return bin * currentSampleRate / fftSize; }
    double getSampleRate() const { return currentSampleRate; }

private:
    std::array<float, fftSize> fifo {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, static_cast<size_t> (fftSize / 2 + 1)> displayDb {};
    int fifoIndex = 0;
    std::atomic<bool> frameReady { false };

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    double currentSampleRate = 48000.0;
};
