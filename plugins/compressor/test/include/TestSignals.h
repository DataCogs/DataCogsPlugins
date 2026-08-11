#pragma once

#include <catch2/catch_all.hpp>
#include "PluginProcessor.h"

// Signal generation, measurement and parameter plumbing shared by the
// processor-level tests (ProcessorTests.cpp). The pure DSP tests
// (DspUnitTests.cpp) deliberately avoid JUCE types and roll their own
// std::vector signals instead.
namespace TestSignals
{
    // Same waveform on every channel.
    inline juce::AudioBuffer<float> sineBuffer (int numChannels, int numSamples,
                                                double freqHz, double sampleRate,
                                                double amplitude)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int n = 0; n < numSamples; ++n)
        {
            const float v = (float) (amplitude * std::sin (juce::MathConstants<double>::twoPi
                                                           * freqHz * (double) n / sampleRate));
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample (ch, n, v);
        }
        return buffer;
    }

    inline juce::AudioBuffer<float> constantBuffer (int numChannels, int numSamples, float value)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int n = 0; n < numSamples; ++n)
                buffer.setSample (ch, n, value);
        return buffer;
    }

    // Set a parameter through the host-visible path (normalised + notify),
    // exactly like the preset code in PluginProcessor does. Choice and bool
    // parameters take their index / 0-1 as the value.
    inline void setParam (CompressorAudioProcessor& processor, const char* paramID, float value)
    {
        auto* param = processor.getValueTreeState().getParameter (paramID);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Run the whole buffer through processBlock in place, in fixed-size
    // chunks - including the final partial chunk.
    inline void processInPlace (CompressorAudioProcessor& processor,
                                juce::AudioBuffer<float>& buffer, int blockSize)
    {
        juce::MidiBuffer midi;
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const int length = juce::jmin (blockSize, buffer.getNumSamples() - start);
            juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(),
                                            buffer.getNumChannels(), start, length);
            processor.processBlock (block, midi);
        }
    }

    inline float rmsDb (const juce::AudioBuffer<float>& buffer, int channel,
                        int startSample, int numSamples)
    {
        return Compressor::amplitudeToDb (buffer.getRMSLevel (channel, startSample, numSamples));
    }
} // namespace TestSignals
