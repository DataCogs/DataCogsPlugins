#include <catch2/catch_all.hpp>
#include "Helpers.h"
#include "Matchers.h"
#include "PluginProcessor.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
// #include "ImageProcessing.h"

// https://www.youtube.com/watch?v=ExwMJquKIhE
// https://tobanteaudio.gitbook.io/juce-cookbook/testing/unit_test
// https://ejaaskel.dev/unit-testing-audio-processors-with-juce-catch2/
// https://github.com/ejaaskel/FilterUnitTest/blob/main/Tests/Matchers.h

namespace plugin_test
{
    TEST_CASE("AudioBuffer equals itself", "[dummy]")
    {
        juce::AudioBuffer<float> *buffer = Helpers::generateAudioSampleBuffer();
        CHECK_THAT(*buffer,
                   AudioBuffersMatch(*buffer));
    }

    TEST_CASE("AudioBuffer is not higher than itself", "[dummy]")
    {
        juce::AudioBuffer<float> *buffer = Helpers::generateAudioSampleBuffer();
        CHECK_THAT(*buffer,
                   !AudioBufferHigherEnergy(*buffer));
    }

    TEST_CASE("Read and write buffer", "[dummy]")
    {
        juce::AudioBuffer<float> *buffer = Helpers::generateAudioSampleBuffer();
        Helpers::writeBufferToFile(buffer, "test_file");
        juce::AudioBuffer<float> *readBuffer = Helpers::readBufferFromFile("test_file");
        CHECK_THAT(*buffer,
                   AudioBuffersMatch(*readBuffer));
        juce::File test_file("test_file");
        test_file.deleteFile();
    }

    TEST_CASE("Plugin instance name", "[name]")
    {
        auto x = juce::ScopedJuceInitialiser_GUI();
        CompressorAudioProcessor *processor(new CompressorAudioProcessor);
        CHECK_THAT(processor->getName().toStdString(),
                   Catch::Matchers::Equals("DataCogs Compressor"));

        // REQUIRE(true);
        delete processor;
    }

    TEST_CASE("Compression Result", "[parameters]")
    {
        int samplesPerBlock = 4096;
        int sampleRate = 44100;
        auto x = juce::ScopedJuceInitialiser_GUI();
        // MyCompressorAudioProcessor *testPluginProcessor(new MyCompressorAudioProcessor);
        auto testPluginProcessor = std::make_unique<CompressorAudioProcessor>();
        juce::MemoryMappedAudioFormatReader *reader = Helpers::readSineSweep();
        juce::AudioBuffer<float> *buffer = new juce::AudioBuffer<float>(reader->numChannels, reader->lengthInSamples);
        reader->read(buffer->getArrayOfWritePointers(), 1, 0, reader->lengthInSamples);
        juce::AudioBuffer<float> originalBuffer(*buffer);
        // Helpers::writeBufferToWavFile(buffer, "/Users/markdaunt/dev/processed.wav");

        // Dismiss the partial chunk for now
        int chunkAmount = buffer->getNumSamples() / samplesPerBlock;
        // ImageProcessing::drawAudioBufferImage(&originalBuffer, "Filter");
        // Helpers::writeBufferToWavFile(&originalBuffer, "/Users/markdaunt/dev/original.wav");

        juce::MidiBuffer midiBuffer;

        testPluginProcessor->prepareToPlay(sampleRate, samplesPerBlock);

        // Process the sine sweep, one chunk at a time
        for (int i = 0; i < chunkAmount; i++)
        {
            juce::AudioBuffer<float> processBuffer(buffer->getNumChannels(), samplesPerBlock);
            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
            {
                processBuffer.copyFrom(0, 0, *buffer, ch, i * samplesPerBlock, samplesPerBlock);
            }

            testPluginProcessor->processBlock(processBuffer, midiBuffer);
            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
            {
                buffer->copyFrom(0, i * samplesPerBlock, processBuffer, ch, 0, samplesPerBlock);
            }
        }
        // ImageProcessing::drawAudioBufferImage(&originalBuffer, "Postfilter");
        // Helpers::writeBufferToWavFile(buffer, "/Users/markdaunt/dev/processed2.wav");
        // const std::filesystem::path file{"example.txt"};
        // std::clog << std::filesystem::absolute(file) << '\n';
        // std::filesystem::path cwd = std::filesystem::current_path().parent_path() / "samples/processed_buffer";
        std::string path = Helpers::getAbsoultePath("samples/processed_buffer");

        // Golden-file regression check: the processed sweep must match the
        // stored reference sample-for-sample. After an *intentional* DSP
        // change, regenerate the reference by running this test once with
        // COMPRESSOR_REGEN_GOLDEN=1 in the environment, then verify a plain
        // run passes.
        if (std::getenv("COMPRESSOR_REGEN_GOLDEN") != nullptr)
        {
            Helpers::writeBufferToFile(buffer, path);
            WARN("Regenerated golden reference: " + path);
        }
        else
        {
            auto compareBuffer = Helpers::readBufferFromFile(path);
            CHECK_THAT(*compareBuffer,
                       AudioBuffersMatch(*buffer));
        }

        // I guess programming C++ like this in the year 2022 isn't a good idea to do publicly
        delete buffer;
        delete reader;
        // delete testPluginProcessor;
    }

}