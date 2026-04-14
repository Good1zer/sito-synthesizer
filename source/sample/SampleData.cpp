#include "SampleData.h"

SampleData::SampleData()
{
    formatManager.registerBasicFormats();
}

void SampleData::clear()
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    audioBuffer.setSize (0, 0);
    sampleRate = 0.0;
    sampleName = "No sample loaded";
    generation.fetch_add (1, std::memory_order_release);
}

bool SampleData::loadFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    // === SAMPLE SIZE LIMIT CHECK ===
    // Calculate memory required: channels * samples * sizeof(float)
    // This prevents OOM crashes when loading huge files
    const auto numChannels = reader->numChannels;
    const auto numSamples = reader->lengthInSamples;
    const auto bytesRequired = static_cast<int64_t> (numChannels) * 
                               static_cast<int64_t> (numSamples) * 
                               static_cast<int64_t> (sizeof (float));
    
    // Safety check: reject files that would exceed memory limit
    if (bytesRequired > maxSampleSizeBytes)
    {
        jassert (false); // Alert in debug builds
        DBG ("Sample too large: " << bytesRequired << " bytes (limit: " << maxSampleSizeBytes << " bytes)");
        return false;
    }

    juce::AudioBuffer<float> newBuffer (static_cast<int> (numChannels),
                                        static_cast<int> (numSamples));

    reader->read (&newBuffer,
                  0,
                  static_cast<int> (reader->lengthInSamples),
                  0,
                  true,
                  true);

    const juce::SpinLock::ScopedLockType lock (sampleLock);
    audioBuffer = std::move (newBuffer);
    sampleRate = reader->sampleRate;
    sampleName = file.getFileName();
    generation.fetch_add (1, std::memory_order_release);
    return true;
}

bool SampleData::loadFromMemory (const void* data, size_t dataSize, const juce::String& nameHint)
{
    if (data == nullptr || dataSize == 0)
        return false;

    auto input = std::make_unique<juce::MemoryInputStream> (data, dataSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (input)));

    if (reader == nullptr)
        return false;

    // === SAMPLE SIZE LIMIT CHECK ===
    // Calculate memory required: channels * samples * sizeof(float)
    const auto numChannels = reader->numChannels;
    const auto numSamples = reader->lengthInSamples;
    const auto bytesRequired = static_cast<int64_t> (numChannels) * 
                               static_cast<int64_t> (numSamples) * 
                               static_cast<int64_t> (sizeof (float));
    
    // Safety check: reject samples that would exceed memory limit
    if (bytesRequired > maxSampleSizeBytes)
    {
        jassert (false); // Alert in debug builds
        DBG ("Sample too large: " << bytesRequired << " bytes (limit: " << maxSampleSizeBytes << " bytes)");
        return false;
    }

    juce::AudioBuffer<float> newBuffer (static_cast<int> (numChannels),
                                        static_cast<int> (numSamples));

    reader->read (&newBuffer,
                  0,
                  static_cast<int> (reader->lengthInSamples),
                  0,
                  true,
                  true);

    const juce::SpinLock::ScopedLockType lock (sampleLock);
    audioBuffer = std::move (newBuffer);
    sampleRate = reader->sampleRate;
    sampleName = nameHint.isNotEmpty() ? nameHint : "Embedded Sample";
    generation.fetch_add (1, std::memory_order_release);
    return true;
}

bool SampleData::writeToMemoryBlock (juce::MemoryBlock& destData) const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);

    if (audioBuffer.getNumChannels() <= 0 || audioBuffer.getNumSamples() < 2 || sampleRate <= 0.0)
        return false;

    destData.reset();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (destData, false);
    juce::WavAudioFormat wavFormat;
    const auto options = juce::AudioFormatWriterOptions()
                             .withSampleRate (sampleRate)
                             .withNumChannels (audioBuffer.getNumChannels())
                             .withBitsPerSample (24);

    std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (stream, options));

    if (writer == nullptr)
    {
        destData.reset();
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer (audioBuffer, 0, audioBuffer.getNumSamples()))
    {
        writer.reset();
        destData.reset();
        return false;
    }

    writer.reset();
    return destData.getSize() > 0;
}

bool SampleData::hasSample() const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return audioBuffer.getNumChannels() > 0 && audioBuffer.getNumSamples() >= 2 && sampleRate > 0.0;
}

juce::String SampleData::getSampleName() const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return sampleName;
}

double SampleData::getLengthSeconds() const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);

    if (audioBuffer.getNumChannels() <= 0 || audioBuffer.getNumSamples() < 2 || sampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (audioBuffer.getNumSamples()) / sampleRate;
}
