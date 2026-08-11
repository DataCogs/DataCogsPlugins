#include "Matchers.h"

juce::Array<float> calculateBinEnergies(const juce::dsp::FFT *forwardFFT, juce::AudioBuffer<float> in)
{
    auto *inRead = in.getArrayOfReadPointers();

    std::array<float, fft_size> inFifo;
    std::array<float, fft_size * 2> inFftData;
    juce::Array<float> binEnergies;
    int fifoIndex = 0;

    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        for (int sam = 0; sam < in.getNumSamples(); ++sam)
        {
            float inSample = inRead[ch][sam];
            if (fifoIndex == fft_size)
            {
                std::fill(inFftData.begin(), inFftData.end(), 0.0f);
                std::copy(inFifo.begin(), inFifo.end(), inFftData.begin());
                forwardFFT->performFrequencyOnlyForwardTransform(inFftData.data(), true);

                for (size_t fftIndex = 0; fftIndex < inFftData.size() / 2; fftIndex++)
                {
                    binEnergies.set(static_cast<int>(fftIndex), binEnergies[static_cast<int>(fftIndex)] + inFftData[fftIndex]);
                }

                fifoIndex = 0;
            }
            inFifo[static_cast<size_t>(fifoIndex)] = inSample;
            fifoIndex++;
        }
    }
    return binEnergies;
}

AudioBufferMatcher::AudioBufferMatcher(juce::AudioBuffer<float> other) : otherBuffer(other) {}

bool AudioBufferMatcher::match(juce::AudioBuffer<float> const &in) const
{
    auto *inRead = in.getArrayOfReadPointers();
    auto *othRead = otherBuffer.getArrayOfReadPointers();

    if (in.getNumChannels() != otherBuffer.getNumChannels() || in.getNumSamples() != otherBuffer.getNumSamples())
    {
        return false;
    }

    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        for (int sam = 0; sam < in.getNumSamples(); ++sam)
        {
            // ~-80 dBFS: loose enough to absorb cross-platform float
            // divergence (MSVC/x86 vs clang/arm64) against the golden file,
            // tight enough to catch any intentional DSP change.
            constexpr float tolerance = 1.0e-4f;
            if (std::fabs(inRead[ch][sam] - othRead[ch][sam]) > tolerance)
            {
                return false;
            }
        }
    }

    return true;
}

std::string AudioBufferMatcher::describe() const
{
    std::ostringstream ss;
    ss << "AudioBuffers are the same";
    return ss.str();
}

AudioBufferMatcher AudioBuffersMatch(juce::AudioBuffer<float> other)
{
    return {other};
}

AudioBufferEnergyMatcher::AudioBufferEnergyMatcher(juce::AudioBuffer<float> other)
    : otherBuffer(other), forwardFFT(fft_order), window(fft_size, juce::dsp::WindowingFunction<float>::WindowingMethod::hann) {}

bool AudioBufferEnergyMatcher::match(juce::AudioBuffer<float> const &in) const
{
    // auto* inRead = in.getArrayOfReadPointers();
    // auto* othRead = otherBuffer.getArrayOfReadPointers();

    juce::Array<float> inEnergies = calculateBinEnergies(&forwardFFT, in);
    juce::Array<float> othEnergies = calculateBinEnergies(&forwardFFT, otherBuffer);

    float inPower = 0;
    float othPower = 0;

    for (int i = 0; i < inEnergies.size(); i++)
    {
        inPower += inEnergies[i];
        othPower += othEnergies[i];
    }

    return inPower < othPower;
}

std::string AudioBufferEnergyMatcher::describe() const
{
    std::ostringstream ss;
    ss << "Other buffer has higher total energy";
    return ss.str();
}

AudioBufferEnergyMatcher AudioBufferHigherEnergy(juce::AudioBuffer<float> other)
{
    return {other};
}

AudioBufferMaxEnergyMatcher::AudioBufferMaxEnergyMatcher(juce::Array<float> other)
    : maxEnergies(other), forwardFFT(fft_order), window(fft_size, juce::dsp::WindowingFunction<float>::WindowingMethod::hann) {}

bool AudioBufferMaxEnergyMatcher::match(juce::AudioBuffer<float> const &in) const
{
    juce::Array<float> binEnergies = calculateBinEnergies(&forwardFFT, in);

    for (int i = 0; i < maxEnergies.size(); i++)
    {
        if (juce::approximatelyEqual(maxEnergies[i], -1.0f))
        {
            continue;
        }
        if (binEnergies[i] > maxEnergies[i])
        {
            return false;
        }
    }

    return true;
}

std::string AudioBufferMaxEnergyMatcher::describe() const
{
    std::ostringstream ss;
    ss << "Energies too high";
    return ss.str();
}

AudioBufferMaxEnergyMatcher AudioBufferCheckMaxEnergy(juce::Array<float> maxEnergiesArray)
{
    return {maxEnergiesArray};
}

AudioBufferMinEnergyMatcher::AudioBufferMinEnergyMatcher(juce::Array<float> other)
    : minEnergies(other), forwardFFT(fft_order), window(fft_size, juce::dsp::WindowingFunction<float>::WindowingMethod::hann) {}

bool AudioBufferMinEnergyMatcher::match(juce::AudioBuffer<float> const &in) const
{
    juce::Array<float> binEnergies = calculateBinEnergies(&forwardFFT, in);

    for (int i = 0; i < minEnergies.size(); i++)
    {
        if (juce::approximatelyEqual(minEnergies[i], -1.0f))
        {
            continue;
        }
        if (binEnergies[i] < minEnergies[i])
        {
            return false;
        }
    }

    return true;
}

std::string AudioBufferMinEnergyMatcher::describe() const
{
    std::ostringstream ss;
    ss << "Energies too low";
    return ss.str();
}

AudioBufferMinEnergyMatcher AudioBufferCheckMinEnergy(juce::Array<float> minEnergiesArray)
{
    return {minEnergiesArray};
}
