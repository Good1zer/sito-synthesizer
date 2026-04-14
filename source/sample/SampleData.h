#pragma once

#include <atomic>

#include <juce_audio_formats/juce_audio_formats.h>

class SampleData
{
public:
    // Maximum sample size to prevent OOM: 500MB of audio data
    // At 48kHz stereo 32-bit float: ~500MB = ~87 minutes of audio
    static constexpr int64_t maxSampleSizeBytes = 500 * 1024 * 1024;
    
    SampleData();

    void clear();
    bool loadFromFile (const juce::File& file);
    bool loadFromMemory (const void* data, size_t dataSize, const juce::String& nameHint);
    bool writeToMemoryBlock (juce::MemoryBlock& destData) const;
    bool hasSample() const;
    juce::String getSampleName() const;
    double getLengthSeconds() const;
    uint64_t getGeneration() const noexcept { return generation.load (std::memory_order_acquire); }

    template <typename Callback>
    bool withTryReadAccess (Callback&& callback) const
    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);

        if (! lock.isLocked() || audioBuffer.getNumChannels() <= 0 || audioBuffer.getNumSamples() < 2 || sampleRate <= 0.0)
            return false;

        callback (audioBuffer, sampleRate);
        return true;
    }

    template <typename Callback>
    bool withReadAccess (Callback&& callback) const
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);

        if (audioBuffer.getNumChannels() <= 0 || audioBuffer.getNumSamples() < 2 || sampleRate <= 0.0)
            return false;

        callback (audioBuffer, sampleRate);
        return true;
    }

private:
    juce::AudioFormatManager formatManager;
    mutable juce::SpinLock sampleLock;
    juce::AudioBuffer<float> audioBuffer;
    double sampleRate = 0.0;
    juce::String sampleName { "No sample loaded" };
    std::atomic<uint64_t> generation { 0 };
};
