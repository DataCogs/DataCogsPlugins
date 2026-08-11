#ifndef MATCHERS_H
#define MATCHERS_H

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <catch2/catch_all.hpp>
#include "Settings.h"

juce::Array<float> calculateBinEnergies(const juce::dsp::FFT* forwardFFT, juce::AudioBuffer<float> in);

class AudioBufferMatcher : public Catch::Matchers::MatcherBase<juce::AudioBuffer<float>> {
    juce::AudioBuffer<float> otherBuffer;
public:
    AudioBufferMatcher(juce::AudioBuffer<float> other);

    bool match(juce::AudioBuffer<float> const& in) const override;

    std::string describe() const override;
};

AudioBufferMatcher AudioBuffersMatch(juce::AudioBuffer<float> other);

class AudioBufferEnergyMatcher : public Catch::Matchers::MatcherBase<juce::AudioBuffer<float>> {
    juce::AudioBuffer<float> otherBuffer;
public:
    AudioBufferEnergyMatcher(juce::AudioBuffer<float> other);

    bool match(juce::AudioBuffer<float> const& in) const override;

    std::string describe() const override;
private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
};

AudioBufferEnergyMatcher AudioBufferHigherEnergy(juce::AudioBuffer<float> other);

class AudioBufferMaxEnergyMatcher : public Catch::Matchers::MatcherBase<juce::AudioBuffer<float>> {
    juce::Array<float> maxEnergies;
public:
    AudioBufferMaxEnergyMatcher(juce::Array<float> other);

    bool match(juce::AudioBuffer<float> const& in) const override;

    std::string describe() const override;
private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
};

AudioBufferMaxEnergyMatcher AudioBufferCheckMaxEnergy(juce::Array<float> maxEnergiesArray);

class AudioBufferMinEnergyMatcher : public Catch::Matchers::MatcherBase<juce::AudioBuffer<float>> {
    juce::Array<float> minEnergies;
public:
    AudioBufferMinEnergyMatcher(juce::Array<float> other);

    bool match(juce::AudioBuffer<float> const& in) const override;

    std::string describe() const override;
private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
};

AudioBufferMinEnergyMatcher AudioBufferCheckMinEnergy(juce::Array<float> minEnergiesArray);

#endif // MATCHERS_H
