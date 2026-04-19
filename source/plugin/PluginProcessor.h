#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

#include "dsp/GranularEngine.h"
#include "sample/SampleData.h"
#include "ui/preset/PresetManager.h"

namespace PluginMeta
{
    static constexpr auto title = "Granular Template";
    static constexpr auto subtitle = "Quanta-like synth scaffold";
}

namespace ParameterIDs
{
    static constexpr auto gain = "gain";
    static constexpr auto position = "position";
    static constexpr auto spray = "spray";
    static constexpr auto spread = "spread";
    static constexpr auto grainSizeMs = "grainSizeMs";
    static constexpr auto densityHz = "densityHz";
    static constexpr auto densitySyncEnabled = "densitySyncEnabled";
    static constexpr auto densitySyncDivision = "densitySyncDivision";
    static constexpr auto densitySyncTripletEnabled = "densitySyncTripletEnabled";
    static constexpr auto densitySyncDottedEnabled = "densitySyncDottedEnabled";
    static constexpr auto pitchSemitones = "pitchSemitones";
    static constexpr auto shapeType = "shapeType";
    static constexpr auto lfo1RateHz = "lfo1RateHz";
    static constexpr auto lfo1Depth = "lfo1Depth";
    static constexpr auto lfo1Shape = "lfo1Shape";
    static constexpr auto envAttackMs = "envAttackMs";
    static constexpr auto envHoldMs = "envHoldMs";
    static constexpr auto envDecayMs = "envDecayMs";
    static constexpr auto envSustain = "envSustain";
    static constexpr auto envReleaseMs = "envReleaseMs";
    static constexpr auto softClipEnabled = "softClipEnabled";
    static constexpr auto maxVoices = "maxVoices";
    static constexpr auto trueStereoEnabled = "trueStereoEnabled";
    static constexpr auto interpolationQuality = "interpolationQuality";
    static constexpr auto rootKey = "rootKey";
}

namespace ParameterNames
{
    static constexpr auto gain = "Gain";
    static constexpr auto position = "Position";
    static constexpr auto spray = "Spray";
    static constexpr auto spread = "Spread";
    static constexpr auto grainSizeMs = "Grain Size";
    static constexpr auto densityHz = "Density";
    static constexpr auto densitySyncEnabled = "Sync";
    static constexpr auto densitySyncDivision = "Rate";
    static constexpr auto densitySyncTripletEnabled = "Triplet";
    static constexpr auto densitySyncDottedEnabled = "Dotted";
    static constexpr auto pitchSemitones = "Pitch";
    static constexpr auto shapeType = "Shape";
    static constexpr auto lfo1RateHz = "LFO 1 Rate";
    static constexpr auto lfo1Depth = "LFO 1 Depth";
    static constexpr auto lfo1Shape = "LFO 1 Shape";
    static constexpr auto envAttackMs = "Attack";
    static constexpr auto envHoldMs = "Hold";
    static constexpr auto envDecayMs = "Decay";
    static constexpr auto envSustain = "Sustain";
    static constexpr auto envReleaseMs = "Release";
    static constexpr auto softClipEnabled = "Soft Clip";
    static constexpr auto maxVoices = "Max Voices";
    static constexpr auto trueStereoEnabled = "True Stereo";
    static constexpr auto interpolationQuality = "Interpolation";
    static constexpr auto rootKey = "Root Key";
}

namespace ParameterDefaults
{
    static constexpr float gainDb = -6.0f;
    static constexpr float position = 0.5f;
    static constexpr float spray = 0.0f;
    static constexpr float spread = 0.35f;
    static constexpr float grainSizeMs = 120.0f;
    static constexpr float densityHz = 8.0f;
    static constexpr bool densitySyncEnabled = false;
    static constexpr int densitySyncDivision = 4; // 1/16 (straight)
    static constexpr bool densitySyncTripletEnabled = false;
    static constexpr bool densitySyncDottedEnabled = false;
    static constexpr float pitchSemitones = 0.0f;
    static constexpr float shapeType = 33.0f;
    static constexpr float lfo1RateHz = 0.35f;
    static constexpr float lfo1Depth = 1.0f;
    static constexpr float lfo1Shape = 0.0f;
    static constexpr float envAttackMs = 12.0f;
    static constexpr float envHoldMs = 0.0f;
    static constexpr float envDecayMs = 220.0f;
    static constexpr float envSustain = 0.82f;
    static constexpr float envReleaseMs = 260.0f;
    static constexpr bool softClipEnabled = true;
    static constexpr int maxVoices = 8;
    static constexpr bool trueStereoEnabled = true;
    static constexpr int interpolationQuality = 0; // 0 = Linear, 1 = Cubic (High Quality)
    static constexpr int rootKey = 0; // pitch class: C
}

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }
    PresetManager& getPresetManager() { return presetManager; }
    bool loadSample (const juce::File& file);
    bool hasLoadedSample() const;
    juce::String getLoadedSampleName() const;
    double getLoadedSampleLengthSeconds() const;
    uint64_t getLoadedSampleGeneration() const noexcept;
    std::vector<float> getWaveformPreview (int numPoints) const;
    int copyGrainVisuals (GranularEngine::GrainVisual* dest, int maxToCopy) const noexcept;
    float getLfo1AssignmentAmount (const juce::String& parameterID) const noexcept;
    void setLfo1AssignmentAmount (const juce::String& parameterID, float amount);
    bool isLfo1AssignmentBypassed (const juce::String& parameterID) const noexcept;
    void setLfo1AssignmentBypassed (const juce::String& parameterID, bool shouldBeBypassed);
    void removeLfo1Assignment (const juce::String& parameterID);
    std::array<float, 8> getLfo1AssignmentAmounts() const noexcept;
    static int getModulationTargetIndexForParameter (const juce::String& parameterID) noexcept;
    static const char* getModulationTargetParameterID (int index) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void rebuildWaveformCache (int numPoints) const;
    void ensureModulationStateTree();
    void restoreModulationAssignmentsFromState();
    float evaluateLfo1Sample() const noexcept;
    void refreshSampleInfoCache();

    juce::AudioProcessorValueTreeState parameters;
    PresetManager presetManager;
    SampleData sampleData;
    GranularEngine granularEngine;

    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* positionParam = nullptr;
    std::atomic<float>* sprayParam = nullptr;
    std::atomic<float>* spreadParam = nullptr;
    std::atomic<float>* grainSizeParam = nullptr;
    std::atomic<float>* densityParam = nullptr;
    std::atomic<float>* densitySyncEnabledParam = nullptr;
    std::atomic<float>* densitySyncDivisionParam = nullptr;
    std::atomic<float>* densitySyncTripletEnabledParam = nullptr;
    std::atomic<float>* densitySyncDottedEnabledParam = nullptr;
    std::atomic<float>* pitchParam = nullptr;
    std::atomic<float>* shapeTypeParam = nullptr;
    std::atomic<float>* lfo1RateParam = nullptr;
    std::atomic<float>* lfo1DepthParam = nullptr;
    std::atomic<float>* lfo1ShapeParam = nullptr;
    std::atomic<float>* envAttackParam = nullptr;
    std::atomic<float>* envHoldParam = nullptr;
    std::atomic<float>* envDecayParam = nullptr;
    std::atomic<float>* envSustainParam = nullptr;
    std::atomic<float>* envReleaseParam = nullptr;
    std::atomic<float>* softClipEnabledParam = nullptr;
    std::atomic<float>* maxVoicesParam = nullptr;
    std::atomic<float>* trueStereoEnabledParam = nullptr;
    std::atomic<float>* interpolationQualityParam = nullptr;
    std::atomic<float>* rootKeyParam = nullptr;

    juce::CriticalSection modulationStateLock;
    juce::ValueTree modulationStateTree;
    std::array<std::atomic<float>, 8> lfo1AssignmentAmounts;
    std::array<std::atomic<bool>, 8> lfo1AssignmentBypassed;
    double currentSampleRate = 44100.0;
    std::atomic<float> lfo1Phase { 0.0f };

    mutable juce::SpinLock waveformCacheLock;
    mutable std::vector<float> waveformCache;
    mutable int waveformCachePoints = 0;
    mutable uint64_t waveformCacheGeneration = 0;

    // UI-facing sample info cache (avoid taking SampleData lock during paint).
    mutable juce::SpinLock sampleInfoLock;
    juce::String cachedSampleName { "No sample loaded" };
    std::atomic<double> cachedSampleLengthSeconds { 0.0 };
    std::atomic<bool> cachedHasSample { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
