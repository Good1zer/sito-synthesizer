#pragma once

#include <array>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>

#include "sample/SampleData.h"

class GranularEngine
{
public:
    struct GrainVisual
    {
        float positionNormalized = 0.0f; // 0..1 in the source sample
        float strength = 0.0f;           // 0..1-ish (envelope * velocity)
        float pan = 0.0f;                // -1..1 (L..R)
    };

    static constexpr int maxGrainVisuals = 128;
    static constexpr int maxVoicesSupported = 8;

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();
    void setMaxVoices (int newMax) noexcept;

    void handleMidiEvent (const juce::MidiMessage& message);
    void render (juce::AudioBuffer<float>& outputBuffer,
                 const SampleData& sampleData,
                 float gainDb,
                 float positionNormalized,
                 float sprayNormalized,
                 float spreadNormalized,
                 float grainSizeMs,
                 float densityHz,
                 float pitchSemitones,
                 float shapeMorph,
                 bool softClipEnabled,
                 bool trueStereoEnabled,
                 bool useCubicInterpolation);

    int copyGrainVisuals (GrainVisual* dest, int maxToCopy) const noexcept;

private:
    struct Grain
    {
        bool isActive = false;
        double sourceSamplePosition = 0.0;
        double sourceIncrement = 1.0;
        int ageSamples = 0;
        int durationSamples = 0;
        float amplitude = 0.0f;
        float panL = 1.0f;
        float panR = 1.0f;
    };

    struct Voice
    {
        bool isActive = false;
        bool isSpawning = false;
        int midiNote = -1;
        float velocity = 0.0f;
        double grainSpawnAccumulator = 0.0;
        int64_t startStamp = 0;
        std::array<Grain, 24> grains;
    };

    static constexpr size_t maxVoices = static_cast<size_t> (maxVoicesSupported);
    static constexpr int envelopeTableSize = 256;

    void spawnGrain (Voice& voice,
                     const juce::AudioBuffer<float>& sampleBuffer,
                     double loadedSampleRate,
                     float positionNormalized,
                     float sprayNormalized,
                     float spreadNormalized,
                     float grainSizeMs,
                     float pitchSemitones);

    float getEnvelopeValue (const Grain& grain, float shapeMorph) const;

    bool voiceHasActiveGrains (const Voice& voice) const;

    void rebuildEnvelopeTables();
    static int countActiveGrains (const std::array<Voice, maxVoices>& voices);

    std::array<Voice, maxVoices> voices;
    int voiceLimit = maxVoicesSupported;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 0;
    juce::Random rng;
    int64_t voiceStampCounter = 0;

    // Block-rate smoothing to avoid sudden bursts (clicks and CPU spikes).
    float smoothedGainDb = 0.0f;
    float smoothedPosition = 0.5f;
    float smoothedSpray = 0.0f;
    float smoothedSpread = 0.0f;
    float smoothedGrainSizeMs = 120.0f;
    float smoothedDensityHz = 8.0f;
    float smoothedPitchSemitones = 0.0f;
    float smoothedShapeMorph = 1.0f;

    std::array<std::array<float, envelopeTableSize>, 4> envelopeTables {};

    std::array<GrainVisual, maxGrainVisuals> grainVisuals {};
    std::atomic<int> grainVisualCount { 0 };
};
