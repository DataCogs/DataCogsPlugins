#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    displayDb.fill (-100.0f);
}

void SpectrumAnalyzer::prepare (double sampleRate)
{
    currentSampleRate = sampleRate;
    fifo.fill (0.0f);
    displayDb.fill (-100.0f);
    fifoIndex = 0;
    frameReady.store (false, std::memory_order_release);
}

void SpectrumAnalyzer::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float mono = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];

        if (fifoIndex == fftSize)
        {
            // Frame complete: publish it unless the UI still owes us a
            // read of the previous one (then this frame is just dropped -
            // a display can afford that, an audio thread can't afford a lock).
            if (! frameReady.load (std::memory_order_acquire))
            {
                std::copy (fifo.begin(), fifo.end(), fftData.begin());
                frameReady.store (true, std::memory_order_release);
            }
            fifoIndex = 0;
        }

        fifo[static_cast<size_t> (fifoIndex++)] = mono;
    }
}

bool SpectrumAnalyzer::refresh()
{
    if (! frameReady.load (std::memory_order_acquire))
        return false;

    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    frameReady.store (false, std::memory_order_release);

    // Hann window coherent gain is 0.5, and JUCE's frequency-only transform
    // returns raw bin magnitudes, so normalise by fftSize/4 for a 0 dBFS
    // sine to read ~0 dB.
    const float normalise = static_cast<float> (fftSize) * 0.25f;

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const float magnitude = fftData[static_cast<size_t> (bin)] / normalise;
        const float db = juce::Decibels::gainToDecibels (magnitude, -100.0f);

        // Rise instantly, decay smoothly - the same ballistics as a level
        // meter, so transients register but the display doesn't flicker.
        auto& shown = displayDb[static_cast<size_t> (bin)];
        shown = db > shown ? db : shown * 0.92f + db * 0.08f;
    }

    return true;
}
