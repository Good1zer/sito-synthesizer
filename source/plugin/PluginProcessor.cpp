#include "PluginProcessor.h"
#include "ui/editor/PluginEditor.h"
#include "dsp/DSPUtils.h"

#include <cmath>

namespace
{
constexpr auto modulationStateType = "MODULATION";
constexpr auto modulationAssignmentType = "LFO1_ASSIGN";
constexpr auto modulationTargetProperty = "target";
constexpr auto modulationAmountProperty = "amount";
constexpr auto modulationBypassedProperty = "bypassed";
constexpr auto embeddedSampleDataProperty = "embeddedSampleData";
constexpr auto embeddedSampleNameProperty = "embeddedSampleName";

std::unique_ptr<juce::RangedAudioParameter> makeFloatParameter (const juce::ParameterID& id,
                                                                const juce::String& name,
                                                                juce::NormalisableRange<float> range,
                                                                float defaultValue,
                                                                const juce::String& suffix = {})
{
    auto attributes = juce::AudioParameterFloatAttributes();

    if (suffix.isNotEmpty())
    {
        attributes = attributes.withStringFromValueFunction (
            [suffix] (float value, int)
            {
                return juce::String (value, 1) + " " + suffix;
            });
    }

    return std::make_unique<juce::AudioParameterFloat> (id, name, range, defaultValue, attributes);
}

constexpr std::array<const char*, 8> modulationTargets
{
    ParameterIDs::position,
    ParameterIDs::spray,
    ParameterIDs::grainSizeMs,
    ParameterIDs::densityHz,
    ParameterIDs::shapeType,
    ParameterIDs::pitchSemitones,
    ParameterIDs::spread,
    ParameterIDs::gain
};

}

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout()),
      presetManager (*this)
{
    for (auto& amount : lfo1AssignmentAmounts)
        amount.store (0.0f);
    for (auto& bypassed : lfo1AssignmentBypassed)
        bypassed.store (false);

    gainParam = parameters.getRawParameterValue (ParameterIDs::gain);
    positionParam = parameters.getRawParameterValue (ParameterIDs::position);
    sprayParam = parameters.getRawParameterValue (ParameterIDs::spray);
    spreadParam = parameters.getRawParameterValue (ParameterIDs::spread);
    grainSizeParam = parameters.getRawParameterValue (ParameterIDs::grainSizeMs);
    densityParam = parameters.getRawParameterValue (ParameterIDs::densityHz);
    densitySyncEnabledParam = parameters.getRawParameterValue (ParameterIDs::densitySyncEnabled);
    densitySyncDivisionParam = parameters.getRawParameterValue (ParameterIDs::densitySyncDivision);
    densitySyncTripletEnabledParam = parameters.getRawParameterValue (ParameterIDs::densitySyncTripletEnabled);
    densitySyncDottedEnabledParam = parameters.getRawParameterValue (ParameterIDs::densitySyncDottedEnabled);
    pitchParam = parameters.getRawParameterValue (ParameterIDs::pitchSemitones);
    shapeTypeParam = parameters.getRawParameterValue (ParameterIDs::shapeType);
    lfo1RateParam = parameters.getRawParameterValue (ParameterIDs::lfo1RateHz);
    lfo1DepthParam = parameters.getRawParameterValue (ParameterIDs::lfo1Depth);
    lfo1ShapeParam = parameters.getRawParameterValue (ParameterIDs::lfo1Shape);
    envAttackParam = parameters.getRawParameterValue (ParameterIDs::envAttackMs);
    envHoldParam = parameters.getRawParameterValue (ParameterIDs::envHoldMs);
    envDecayParam = parameters.getRawParameterValue (ParameterIDs::envDecayMs);
    envSustainParam = parameters.getRawParameterValue (ParameterIDs::envSustain);
    envReleaseParam = parameters.getRawParameterValue (ParameterIDs::envReleaseMs);
    softClipEnabledParam = parameters.getRawParameterValue (ParameterIDs::softClipEnabled);
    maxVoicesParam = parameters.getRawParameterValue (ParameterIDs::maxVoices);
    trueStereoEnabledParam = parameters.getRawParameterValue (ParameterIDs::trueStereoEnabled);
    interpolationQualityParam = parameters.getRawParameterValue (ParameterIDs::interpolationQuality);
    rootKeyParam = parameters.getRawParameterValue (ParameterIDs::rootKey);

    ensureModulationStateTree();
    restoreModulationAssignmentsFromState();
}

juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (makeFloatParameter (
        { ParameterIDs::gain, 1 },
        ParameterNames::gain,
        { -24.0f, 12.0f, 0.01f },
        ParameterDefaults::gainDb,
        "dB"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::position, 1 },
        ParameterNames::position,
        { 0.0f, 1.0f, 0.001f },
        ParameterDefaults::position));

    params.push_back (makeFloatParameter (
        { ParameterIDs::spray, 1 },
        ParameterNames::spray,
        { 0.0f, 1.0f, 0.001f },
        ParameterDefaults::spray));

    params.push_back (makeFloatParameter (
        { ParameterIDs::spread, 1 },
        ParameterNames::spread,
        { 0.0f, 1.0f, 0.001f },
        ParameterDefaults::spread));

    params.push_back (makeFloatParameter (
        { ParameterIDs::grainSizeMs, 1 },
        ParameterNames::grainSizeMs,
        { 10.0f, 500.0f, 0.1f },
        ParameterDefaults::grainSizeMs,
        "ms"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::densityHz, 1 },
        ParameterNames::densityHz,
        { 1.0f, 40.0f, 0.1f },
        ParameterDefaults::densityHz,
        "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::densitySyncEnabled, 1 },
        ParameterNames::densitySyncEnabled,
        ParameterDefaults::densitySyncEnabled));

    // 0..9 mapped in editor/processor to musical divisions (1/1 .. 1/32 + dotted/triplet variants).
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::densitySyncDivision, 1 },
        ParameterNames::densitySyncDivision,
        0,
        17,
        ParameterDefaults::densitySyncDivision));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::densitySyncTripletEnabled, 1 },
        ParameterNames::densitySyncTripletEnabled,
        ParameterDefaults::densitySyncTripletEnabled));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::densitySyncDottedEnabled, 1 },
        ParameterNames::densitySyncDottedEnabled,
        ParameterDefaults::densitySyncDottedEnabled));

    params.push_back (makeFloatParameter (
        { ParameterIDs::pitchSemitones, 1 },
        ParameterNames::pitchSemitones,
        { -24.0f, 24.0f, 0.01f },
        ParameterDefaults::pitchSemitones,
        "st"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::shapeType, 1 },
        ParameterNames::shapeType,
        { 0.0f, 100.0f, 1.0f },
        ParameterDefaults::shapeType));

    params.push_back (makeFloatParameter (
        { ParameterIDs::lfo1RateHz, 1 },
        ParameterNames::lfo1RateHz,
        { 0.01f, 20.0f, 0.001f },
        ParameterDefaults::lfo1RateHz,
        "Hz"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::lfo1Depth, 1 },
        ParameterNames::lfo1Depth,
        { 0.0f, 1.0f, 0.001f },
        ParameterDefaults::lfo1Depth));

    params.push_back (makeFloatParameter (
        { ParameterIDs::lfo1Shape, 1 },
        ParameterNames::lfo1Shape,
        { 0.0f, 100.0f, 1.0f },
        ParameterDefaults::lfo1Shape));

    params.push_back (makeFloatParameter (
        { ParameterIDs::envAttackMs, 1 },
        ParameterNames::envAttackMs,
        { 0.0f, 4000.0f, 1.0f },
        ParameterDefaults::envAttackMs,
        "ms"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::envHoldMs, 1 },
        ParameterNames::envHoldMs,
        { 0.0f, 2000.0f, 1.0f },
        ParameterDefaults::envHoldMs,
        "ms"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::envDecayMs, 1 },
        ParameterNames::envDecayMs,
        { 0.0f, 4000.0f, 1.0f },
        ParameterDefaults::envDecayMs,
        "ms"));

    params.push_back (makeFloatParameter (
        { ParameterIDs::envSustain, 1 },
        ParameterNames::envSustain,
        { 0.0f, 1.0f, 0.001f },
        ParameterDefaults::envSustain));

    params.push_back (makeFloatParameter (
        { ParameterIDs::envReleaseMs, 1 },
        ParameterNames::envReleaseMs,
        { 0.0f, 6000.0f, 1.0f },
        ParameterDefaults::envReleaseMs,
        "ms"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::softClipEnabled, 1 },
        ParameterNames::softClipEnabled,
        ParameterDefaults::softClipEnabled));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::maxVoices, 1 },
        ParameterNames::maxVoices,
        1,
        GranularEngine::maxVoicesSupported,
        ParameterDefaults::maxVoices));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::trueStereoEnabled, 1 },
        ParameterNames::trueStereoEnabled,
        ParameterDefaults::trueStereoEnabled));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::interpolationQuality, 1 },
        ParameterNames::interpolationQuality,
        juce::StringArray { "Linear", "Cubic (High Quality)" },
        ParameterDefaults::interpolationQuality));

    // Root key: pitch class, where 0 = C, 1 = C#, ... 11 = B.
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParameterIDs::rootKey, 1 },
        ParameterNames::rootKey,
        0,
        11,
        ParameterDefaults::rootKey));

    return { params.begin(), params.end() };
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() = default;

const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lfo1Phase.store (0.0f, std::memory_order_release);
    granularEngine.prepare (sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    lfo1Phase.store (0.0f, std::memory_order_release);
    granularEngine.reset();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    granularEngine.setMaxVoices (static_cast<int> (maxVoicesParam->load()));

    for (const auto metadata : midiMessages)
        granularEngine.handleMidiEvent (metadata.getMessage());

    // Density: either free Hz or tempo-synced to host.
    float densityHzUsed = densityParam->load();
    if (densitySyncEnabledParam->load() >= 0.5f)
    {
        double bpm = 120.0;
        if (auto* hostPlayHead = getPlayHead())
            if (auto pos = hostPlayHead->getPosition())
                if (auto hostBpm = pos->getBpm())
                    bpm = *hostBpm;

        const auto divIndex = static_cast<int> (densitySyncDivisionParam->load());

        auto divisionBeats = [] (int index) -> double
        {
            // 0..5 straight: 1/1..1/32
            // 6..11 dotted:  1/1..1/32 (x 1.5)
            // 12..17 triplet:1/1..1/32 (x 2/3)
            const double baseBeats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
            const auto clamped = juce::jlimit (0, 17, index);

            if (clamped < 6)
                return baseBeats[clamped];

            if (clamped < 12)
                return baseBeats[clamped - 6] * 1.5;

            return baseBeats[clamped - 12] * (2.0 / 3.0);
        };

        const auto beats = divisionBeats (divIndex);
        const auto grainsPerSecond = (bpm / 60.0) * (1.0 / juce::jmax (1.0e-6, beats));
        densityHzUsed = static_cast<float> (juce::jlimit (0.1, 1200.0, grainsPerSecond));
    }

    const auto lfoDepth = juce::jlimit (0.0f, 1.0f, lfo1DepthParam->load());
    const auto lfoValue = evaluateLfo1Sample() * lfoDepth;

    auto applyModulation = [this, lfoValue] (const char* parameterID,
                                             float baseValue,
                                             float minValue,
                                             float maxValue,
                                             float bipolarRange) -> float
    {
        if (isLfo1AssignmentBypassed (parameterID))
            return juce::jlimit (minValue, maxValue, baseValue);

        const auto amount = getLfo1AssignmentAmount (parameterID);
        const auto modulated = baseValue + amount * lfoValue * bipolarRange;
        return juce::jlimit (minValue, maxValue, modulated);
    };

    const auto gainDbUsed = applyModulation (ParameterIDs::gain, gainParam->load(), -24.0f, 12.0f, 18.0f);
    const auto positionUsed = applyModulation (ParameterIDs::position, positionParam->load(), 0.0f, 1.0f, 0.5f);
    const auto sprayUsed = applyModulation (ParameterIDs::spray, sprayParam->load(), 0.0f, 1.0f, 0.5f);
    const auto spreadUsed = applyModulation (ParameterIDs::spread, spreadParam->load(), 0.0f, 1.0f, 0.5f);
    const auto grainSizeMsUsed = applyModulation (ParameterIDs::grainSizeMs, grainSizeParam->load(), 10.0f, 500.0f, 245.0f);
    densityHzUsed = applyModulation (ParameterIDs::densityHz, densityHzUsed, 0.1f, 1200.0f, 60.0f);
    
    // Root key defines sample's native pitch class. Example: root = F means
    // pressing C should transpose sample down 5 semitones, while pressing F
    // plays it back unshifted.
    const auto rootKeySemitone = static_cast<float> (juce::jlimit (0, 11, static_cast<int> (rootKeyParam->load())));
    const auto pitchSemitonesUsed = applyModulation (ParameterIDs::pitchSemitones,
                                                     pitchParam->load(),
                                                     -24.0f,
                                                     24.0f,
                                                     24.0f);
    const auto pitchWithRoot = pitchSemitonesUsed - rootKeySemitone;
    const auto useHighQualityInterpolation = interpolationQualityParam->load() >= 0.5f
        || std::abs (pitchWithRoot) > 0.01f;
    
    const auto shapeTypeUsed = applyModulation (ParameterIDs::shapeType, shapeTypeParam->load(), 0.0f, 100.0f, 50.0f);

    granularEngine.render (buffer,
                           sampleData,
                           gainDbUsed,
                           positionUsed,
                           sprayUsed,
                           spreadUsed,
                           grainSizeMsUsed,
                           densityHzUsed,
                           pitchWithRoot,
                           (shapeTypeUsed / 100.0f) * 3.0f,
                           juce::jmax (0.0f, envAttackParam->load()),
                           juce::jmax (0.0f, envHoldParam->load()),
                           juce::jmax (0.0f, envDecayParam->load()),
                           juce::jlimit (0.0f, 1.0f, envSustainParam->load()),
                           juce::jmax (0.0f, envReleaseParam->load()),
                           softClipEnabledParam->load() >= 0.5f,
                           trueStereoEnabledParam->load() >= 0.5f,
                           useHighQualityInterpolation);

    const auto phaseDelta = static_cast<float> ((juce::jmax (0.01f, lfo1RateParam->load()) * buffer.getNumSamples())
                                                / juce::jmax (1.0, currentSampleRate));
    auto phase = lfo1Phase.load (std::memory_order_acquire);
    phase += phaseDelta;
    phase -= std::floor (phase);
    lfo1Phase.store (phase, std::memory_order_release);
}

bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    ensureModulationStateTree();
    auto state = parameters.copyState();

    if (sampleData.hasSample())
    {
        juce::MemoryBlock embeddedSampleData;
        if (sampleData.writeToMemoryBlock (embeddedSampleData))
        {
            state.setProperty (embeddedSampleDataProperty, embeddedSampleData, nullptr);
            state.setProperty (embeddedSampleNameProperty, sampleData.getSampleName(), nullptr);
        }
    }
    else
    {
        state.removeProperty (embeddedSampleDataProperty, nullptr);
        state.removeProperty (embeddedSampleNameProperty, nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        auto restoredState = juce::ValueTree::fromXml (*xmlState);

        for (auto child : restoredState)
        {
            if (child.getProperty ("id").toString() != ParameterIDs::rootKey)
                continue;

            const auto rawRootKey = static_cast<int> (std::round (static_cast<double> (child.getProperty ("value"))));
            const auto normalizedRootKey = juce::jlimit (0, 11, ((rawRootKey % 12) + 12) % 12);
            child.setProperty ("value", normalizedRootKey, nullptr);
            break;
        }

        parameters.replaceState (restoredState);

        ensureModulationStateTree();
        restoreModulationAssignmentsFromState();

        const auto embeddedSampleData = parameters.state.getProperty (embeddedSampleDataProperty);
        const auto embeddedSampleName = parameters.state.getProperty (embeddedSampleNameProperty).toString();

        if (const auto* memoryBlock = embeddedSampleData.getBinaryData())
        {
            sampleData.loadFromMemory (memoryBlock->getData(), memoryBlock->getSize(), embeddedSampleName);
            granularEngine.reset();
        }
        else
        {
            sampleData.clear();
            granularEngine.reset();
        }

        refreshSampleInfoCache();
    }
}

bool AudioPluginAudioProcessor::loadSample (const juce::File& file)
{
    const auto loaded = sampleData.loadFromFile (file);

    if (loaded)
    {
        granularEngine.reset();
        refreshSampleInfoCache();
    }

    return loaded;
}

float AudioPluginAudioProcessor::getLfo1AssignmentAmount (const juce::String& parameterID) const noexcept
{
    const auto index = getModulationTargetIndexForParameter (parameterID);
    return index >= 0 ? lfo1AssignmentAmounts[static_cast<size_t> (index)].load() : 0.0f;
}

void AudioPluginAudioProcessor::setLfo1AssignmentAmount (const juce::String& parameterID, float amount)
{
    const auto index = getModulationTargetIndexForParameter (parameterID);
    if (index < 0)
        return;

    const auto clamped = juce::jlimit (-1.0f, 1.0f, amount);
    lfo1AssignmentAmounts[static_cast<size_t> (index)].store (clamped);

    const juce::ScopedLock lock (modulationStateLock);
    ensureModulationStateTree();

    auto assignment = modulationStateTree.getChildWithProperty (modulationTargetProperty, parameterID);
    if (! assignment.isValid())
    {
        assignment = juce::ValueTree (modulationAssignmentType);
        assignment.setProperty (modulationTargetProperty, parameterID, nullptr);
        modulationStateTree.appendChild (assignment, nullptr);
    }

    assignment.setProperty (modulationAmountProperty, clamped, nullptr);
}

bool AudioPluginAudioProcessor::isLfo1AssignmentBypassed (const juce::String& parameterID) const noexcept
{
    const auto index = getModulationTargetIndexForParameter (parameterID);
    return index >= 0 ? lfo1AssignmentBypassed[static_cast<size_t> (index)].load() : false;
}

void AudioPluginAudioProcessor::setLfo1AssignmentBypassed (const juce::String& parameterID, bool shouldBeBypassed)
{
    const auto index = getModulationTargetIndexForParameter (parameterID);
    if (index < 0)
        return;

    lfo1AssignmentBypassed[static_cast<size_t> (index)].store (shouldBeBypassed);

    const juce::ScopedLock lock (modulationStateLock);
    ensureModulationStateTree();

    auto assignment = modulationStateTree.getChildWithProperty (modulationTargetProperty, parameterID);
    if (! assignment.isValid())
    {
        assignment = juce::ValueTree (modulationAssignmentType);
        assignment.setProperty (modulationTargetProperty, parameterID, nullptr);
        assignment.setProperty (modulationAmountProperty, 0.0f, nullptr);
        modulationStateTree.appendChild (assignment, nullptr);
    }

    assignment.setProperty (modulationBypassedProperty, shouldBeBypassed, nullptr);
}

void AudioPluginAudioProcessor::removeLfo1Assignment (const juce::String& parameterID)
{
    setLfo1AssignmentAmount (parameterID, 0.0f);
    setLfo1AssignmentBypassed (parameterID, false);
}

std::array<float, 8> AudioPluginAudioProcessor::getLfo1AssignmentAmounts() const noexcept
{
    std::array<float, 8> amounts {};
    for (size_t i = 0; i < amounts.size(); ++i)
        amounts[i] = lfo1AssignmentAmounts[i].load();
    return amounts;
}

int AudioPluginAudioProcessor::getModulationTargetIndexForParameter (const juce::String& parameterID) noexcept
{
    for (size_t i = 0; i < modulationTargets.size(); ++i)
        if (parameterID == modulationTargets[i])
            return static_cast<int> (i);

    return -1;
}

const char* AudioPluginAudioProcessor::getModulationTargetParameterID (int index) noexcept
{
    return juce::isPositiveAndBelow (index, static_cast<int> (modulationTargets.size()))
        ? modulationTargets[static_cast<size_t> (index)]
        : nullptr;
}

void AudioPluginAudioProcessor::ensureModulationStateTree()
{
    if (modulationStateTree.isValid() && modulationStateTree.getParent() == parameters.state)
        return;

    modulationStateTree = parameters.state.getChildWithName (modulationStateType);

    if (! modulationStateTree.isValid())
    {
        modulationStateTree = juce::ValueTree (modulationStateType);
        parameters.state.appendChild (modulationStateTree, nullptr);
    }

    for (const auto* target : modulationTargets)
    {
        auto assignment = modulationStateTree.getChildWithProperty (modulationTargetProperty, target);
        if (! assignment.isValid())
        {
            assignment = juce::ValueTree (modulationAssignmentType);
            assignment.setProperty (modulationTargetProperty, target, nullptr);
            assignment.setProperty (modulationAmountProperty, 0.0f, nullptr);
            assignment.setProperty (modulationBypassedProperty, false, nullptr);
            modulationStateTree.appendChild (assignment, nullptr);
        }
    }
}

void AudioPluginAudioProcessor::restoreModulationAssignmentsFromState()
{
    const juce::ScopedLock lock (modulationStateLock);
    ensureModulationStateTree();

    for (size_t i = 0; i < modulationTargets.size(); ++i)
    {
        const auto* target = modulationTargets[i];
        const auto assignment = modulationStateTree.getChildWithProperty (modulationTargetProperty, target);
        const auto amount = assignment.isValid()
            ? static_cast<float> (assignment.getProperty (modulationAmountProperty, 0.0f))
            : 0.0f;
        const auto bypassed = assignment.isValid()
            ? static_cast<bool> (assignment.getProperty (modulationBypassedProperty, false))
            : false;

        lfo1AssignmentAmounts[i].store (juce::jlimit (-1.0f, 1.0f, amount));
        lfo1AssignmentBypassed[i].store (bypassed);
    }
}

float AudioPluginAudioProcessor::evaluateLfo1Sample() const noexcept
{
    return SitoDSP::evaluateShapeWaveform (lfo1Phase.load (std::memory_order_acquire),
                                           lfo1ShapeParam != nullptr ? lfo1ShapeParam->load() : 0.0f);
}

void AudioPluginAudioProcessor::refreshSampleInfoCache()
{
    const auto hasSample = sampleData.hasSample();

    {
        const juce::SpinLock::ScopedLockType infoLock (sampleInfoLock);
        cachedSampleName = hasSample ? sampleData.getSampleName() : "No sample loaded";
    }

    cachedHasSample.store (hasSample, std::memory_order_release);
    cachedSampleLengthSeconds.store (hasSample ? sampleData.getLengthSeconds() : 0.0, std::memory_order_release);
    rebuildWaveformCache (64);
}

bool AudioPluginAudioProcessor::hasLoadedSample() const
{
    return cachedHasSample.load (std::memory_order_acquire);
}

juce::String AudioPluginAudioProcessor::getLoadedSampleName() const
{
    const juce::SpinLock::ScopedLockType lock (sampleInfoLock);
    return cachedSampleName;
}

double AudioPluginAudioProcessor::getLoadedSampleLengthSeconds() const
{
    return cachedSampleLengthSeconds.load (std::memory_order_acquire);
}

uint64_t AudioPluginAudioProcessor::getLoadedSampleGeneration() const noexcept
{
    return sampleData.getGeneration();
}

std::vector<float> AudioPluginAudioProcessor::getWaveformPreview (int numPoints) const
{
    if (numPoints <= 0)
        return {};

    const auto gen = sampleData.getGeneration();

    {
        const juce::SpinLock::ScopedLockType lock (waveformCacheLock);
        if (waveformCachePoints == numPoints
            && waveformCacheGeneration == gen
            && static_cast<int> (waveformCache.size()) == numPoints)
            return waveformCache;
    }

    rebuildWaveformCache (numPoints);

    const juce::SpinLock::ScopedLockType lock (waveformCacheLock);
    if (waveformCachePoints == numPoints && static_cast<int> (waveformCache.size()) == numPoints)
        return waveformCache;

    return std::vector<float> (static_cast<size_t> (numPoints), 0.0f);
}

void AudioPluginAudioProcessor::rebuildWaveformCache (int numPoints) const
{
    if (numPoints <= 0)
        return;

    std::vector<float> preview (static_cast<size_t> (numPoints), 0.0f);
    const auto gen = sampleData.getGeneration();

    // Blocking read access: we do this rarely (on load / cache miss), not per-frame.
    sampleData.withReadAccess ([&] (const juce::AudioBuffer<float>& buffer, double)
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        if (numChannels <= 0 || numSamples <= 0)
            return;

        const auto* ch0 = buffer.getReadPointer (0);
        const auto* ch1 = buffer.getReadPointer (juce::jmin (1, numChannels - 1));
        const bool hasSecond = (numChannels > 1);

        for (int i = 0; i < numPoints; ++i)
        {
            const auto start = static_cast<int> ((static_cast<int64_t> (i) * numSamples) / numPoints);
            const auto end = static_cast<int> ((static_cast<int64_t> (i + 1) * numSamples) / numPoints);

            float peak = 0.0f;
            const auto stop = juce::jmax (start + 1, end);

            for (int sample = start; sample < stop; ++sample)
            {
                peak = juce::jmax (peak, std::abs (ch0[sample]));
                if (hasSecond)
                    peak = juce::jmax (peak, std::abs (ch1[sample]));
            }

            preview[static_cast<size_t> (i)] = peak;
        }
    });

    const juce::SpinLock::ScopedLockType lock (waveformCacheLock);
    waveformCache = std::move (preview);
    waveformCachePoints = numPoints;
    waveformCacheGeneration = gen;
}

int AudioPluginAudioProcessor::copyGrainVisuals (GranularEngine::GrainVisual* dest, int maxToCopy) const noexcept
{
    return granularEngine.copyGrainVisuals (dest, maxToCopy);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
