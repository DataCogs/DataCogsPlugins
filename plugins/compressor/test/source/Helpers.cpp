#include "Helpers.h"
#include <filesystem>
#include <iostream>

juce::AudioBuffer<float> *Helpers::generateAudioSampleBuffer(int channels, int samples)
{
    juce::AudioBuffer<float> *buffer = new juce::AudioBuffer<float>(channels, samples);

    // Fill with random values ranging from -1 to 1
    for (int i = 0; i < channels; i++)
    {
        for (int j = 0; j < samples; j++)
        {
            buffer->setSample(i, j, -1.0f + juce::Random::getSystemRandom().nextFloat() * 2.0f);
        }
    }

    return buffer;
}

juce::AudioBuffer<float> *Helpers::generateIncreasingAudioSampleBuffer(int channels, int samples)
{
    juce::AudioBuffer<float> *buffer = new juce::AudioBuffer<float>(channels, samples);

    // Fill with increasing values ranging from -1 to 1
    for (int i = 0; i < channels; i++)
    {
        for (int j = 0; j < samples; j++)
        {
            buffer->setSample(i, j, juce::jmap((float)(j % 1001), 0.0f, 1000.0f, -1.0f, 1.0f));
        }
    }

    return buffer;
}

std::string Helpers::getAbsoultePath(std::string relPath)
{
    // std::filesystem::path cwd = std::filesystem::current_path();
    // std::filesystem::path absPath = cwd / relPath;
    // return absPath.string();
    auto baseDir = std::filesystem::current_path();
    while (baseDir.has_parent_path())
    {
        auto combinePath = baseDir / relPath;
        if (std::filesystem::exists(combinePath))
        {
            return combinePath.string();
        }
        baseDir = baseDir.parent_path();
    }
    throw std::runtime_error("File not found: " + relPath);
}

juce::MemoryMappedAudioFormatReader *Helpers::readSineSweep()
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    juce::AudioFormat *audioFormat = formatManager.getDefaultFormat();
    // std::filesystem::path cwd = std::filesystem::current_path().parent_path() / "samples/sin_1000_vary.wav";
    std::string relPath = "samples/sin_1000_vary.wav";
    std::string absPath = Helpers::getAbsoultePath(relPath);

    // std::ofstream file(cwd.string());
    // file.close();
    juce::MemoryMappedAudioFormatReader *reader = audioFormat->createMemoryMappedReader(juce::File(absPath));
    reader->mapEntireFile();

    return reader;
}

void Helpers::writeBufferToFile(juce::AudioBuffer<float> *buffer, juce::String path)
{
    juce::File bufferFile = juce::File(path);
    juce::FileOutputStream output(bufferFile);

    // FileOutputStream appends to an existing file by default; start clean
    // so rewriting a reference file replaces it instead of growing it.
    output.setPosition(0);
    output.truncate();

    output.setNewLineString("\n");

    output.writeInt(buffer->getNumChannels());
    output.writeInt(buffer->getNumSamples());

    for (int i = 0; i < buffer->getNumChannels(); i++)
    {
        for (int j = 0; j < buffer->getNumSamples(); j++)
        {
            output.writeFloat(buffer->getSample(i, j));
        }
    }

    output.flush();
}

juce::AudioBuffer<float> *Helpers::readBufferFromFile(juce::String path)
{
    juce::File bufferFile = juce::File(path);

    juce::FileInputStream input(bufferFile);

    int numChannels = input.readInt();
    int numSamples = input.readInt();

    juce::AudioBuffer<float> *buffer = new juce::AudioBuffer<float>(numChannels, numSamples);
    for (int i = 0; i < buffer->getNumChannels(); i++)
    {
        for (int j = 0; j < buffer->getNumSamples(); j++)
        {
            buffer->setSample(i, j, input.readFloat());
        }
    }
    return buffer;
}

void Helpers::writeBufferToWavFile(juce::AudioBuffer<float> *buffer, juce::String path)
{
    juce::File outputFile(path);
    if (!outputFile.create().wasOk())
    {
        std::cerr << "Error creating file: " << path.toStdString() << std::endl;
        return;
    }

    std::unique_ptr<juce::OutputStream> outStream = outputFile.createOutputStream();
    if (!outStream)
    {
        std::cerr << "Error creating file output stream for: " << path.toStdString() << std::endl;
        return;
    }

    // JUCE 9: createWriterFor takes an AudioFormatWriterOptions and moves the
    // stream into the writer, which flushes and closes it on destruction.
    juce::WavAudioFormat format;
    auto writer = format.createWriterFor (outStream,
                                          juce::AudioFormatWriterOptions{}
                                              .withSampleRate (44100.0)
                                              .withNumChannels (buffer->getNumChannels())
                                              .withBitsPerSample (32));

    if (!writer)
    {
        std::cerr << "Error creating writer for: " << path.toStdString() << std::endl;
        return;
    }

    writer->writeFromAudioSampleBuffer(*buffer, 0, buffer->getNumSamples());

    std::cout << "Wrote buffer to file: " << path.toStdString() << std::endl;
}
