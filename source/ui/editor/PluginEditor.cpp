#include "PluginEditor.h"

#include <jive_core/jive_core.h>

namespace
{
juce::ValueTree makeLabel (const juce::String& text, const juce::String& id = {})
{
    juce::ValueTree label { "Text" };
    label.setProperty ("text", text, nullptr);

    if (id.isNotEmpty())
        label.setProperty ("id", id, nullptr);

    return label;
}

juce::ValueTree makeKnob (const juce::String& id,
                         const juce::String& title,
                         const juce::String& min,
                         const juce::String& max,
                         const juce::String& interval)
{
    return juce::ValueTree {
        "Component",
        {
            { "class", "control" },
            { "width", 120 },
            { "height", 128 },
            { "align-items", "centre" },
        },
        {
            juce::ValueTree {
                "Knob",
                {
                    { "id", id },
                    { "class", "knob" },
                    { "width", 86 },
                    { "height", 86 },
                    { "min", min },
                    { "max", max },
                    { "interval", interval },
                },
            },
            makeLabel (title),
        },
    };
}

juce::ValueTree makeToggle (const juce::String& id, const juce::String& text)
{
    return juce::ValueTree {
        "Checkbox",
        {
            { "id", id },
            { "text", text },
            { "height", 28 },
            { "width", 170 },
        },
    };
}

juce::String formatSampleStatus (AudioPluginAudioProcessor& processor)
{
    if (! processor.hasLoadedSample())
        return "No sample loaded";

    return processor.getLoadedSampleName()
           + "  ("
           + juce::String (processor.getLoadedSampleLengthSeconds(), 2)
           + " s)";
}
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      editorView (createEditorView())
{
    processorRef.getPresetManager().addListener (this);

    rootItem = interpreter.interpret (editorView);
    jassert (rootItem != nullptr);

    if (rootItem == nullptr)
    {
        setSize (860, 560);
        return;
    }

    interpreter.listenTo (*rootItem);
    rootComponent = rootItem->getComponent().get();

    if (rootComponent != nullptr)
        addAndMakeVisible (*rootComponent);

    const auto width = static_cast<int> (editorView.getProperty ("width", 860));
    const auto height = static_cast<int> (editorView.getProperty ("height", 560));
    setSize (width, height);

    attachParameters();
    updateSampleStatus();
    currentPresetChanged (processorRef.getPresetManager().getCurrentPresetName());
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    processorRef.getPresetManager().removeListener (this);
}

void AudioPluginAudioProcessorEditor::resized()
{
    if (rootComponent != nullptr)
        rootComponent->setBounds (getLocalBounds());
}

bool AudioPluginAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (hasSupportedSampleExtension (juce::File (path)))
            return true;

    return false;
}

void AudioPluginAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File file (path);
        if (! hasSupportedSampleExtension (file))
            continue;

        if (processorRef.loadSample (file))
        {
            updateSampleStatus();
            return;
        }
    }

    updateSampleStatus();
}

void AudioPluginAudioProcessorEditor::presetListChanged()
{
}

void AudioPluginAudioProcessorEditor::currentPresetChanged (const juce::String& presetName)
{
    setTextNodeContent ("preset-name", presetName.isNotEmpty() ? presetName : "Init");
}

juce::ValueTree AudioPluginAudioProcessorEditor::createEditorView()
{
    return juce::ValueTree {
        "Component",
        {
            { "id", "editor-root" },
            { "width", 860 },
            { "height", 560 },
            { "padding", "18" },
            { "gap", 12 },
            { "style",
              new jive::Object {
                  { "background", "#101219" },
                  { "foreground", "#d6dde8" },
                  { "font-size", 14 },
                  { "font-family", "Inter" },
                  { ".section",
                    new jive::Object {
                        { "background", "#171b24" },
                        { "border-radius", 8 },
                        { "border", "#2e3442" },
                    } },
                  { ".header",
                    new jive::Object {
                        { "font-size", 22 },
                        { "font-weight", "bold" },
                    } },
                  { ".sub",
                    new jive::Object {
                        { "foreground", "#8f9bb0" },
                    } },
              } },
        },
        {
            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "height", 72 },
                    { "padding", "12 14" },
                    { "justify-content", "space-between" },
                    { "align-items", "centre" },
                    { "flex-direction", "row" },
                },
                {
                    juce::ValueTree {
                        "Component",
                        {
                            { "gap", 2 },
                        },
                        {
                            makeLabel ("SITO", "title"),
                            makeLabel ("Granular Synth", "sample-status"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "align-items", "flex-end" },
                            { "gap", 2 },
                        },
                        {
                            makeLabel ("Preset", "preset-caption"),
                            makeLabel ("Init", "preset-name"),
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "padding", 12 },
                    { "flex-grow", 1.0 },
                    { "gap", 14 },
                },
                {
                    makeLabel ("Source"),
                    juce::ValueTree {
                        "Component",
                        {
                            { "flex-direction", "row" },
                            { "flex-wrap", "wrap" },
                            { "gap", 10 },
                        },
                        {
                            makeKnob (ParameterIDs::position, "Position", "0", "1", "0.001"),
                            makeKnob (ParameterIDs::spray, "Spray", "0", "1", "0.001"),
                            makeKnob (ParameterIDs::grainSizeMs, "Size", "10", "500", "0.1"),
                            makeKnob (ParameterIDs::densityHz, "Density", "1", "40", "0.1"),
                            makeKnob (ParameterIDs::pitchSemitones, "Pitch", "-24", "24", "0.01"),
                            makeKnob (ParameterIDs::shapeType, "Shape", "0", "100", "1"),
                            makeKnob (ParameterIDs::spread, "Spread", "0", "1", "0.001"),
                            makeKnob (ParameterIDs::gain, "Gain", "-24", "12", "0.01"),
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "padding", 12 },
                    { "gap", 10 },
                },
                {
                    makeLabel ("Modulation + Envelope"),
                    juce::ValueTree {
                        "Component",
                        {
                            { "flex-direction", "row" },
                            { "flex-wrap", "wrap" },
                            { "gap", 10 },
                        },
                        {
                            makeKnob (ParameterIDs::lfo1RateHz, "LFO Rate", "0.01", "20", "0.001"),
                            makeKnob (ParameterIDs::lfo1Depth, "LFO Depth", "0", "1", "0.001"),
                            makeKnob (ParameterIDs::lfo1Shape, "LFO Shape", "0", "100", "1"),
                            makeKnob (ParameterIDs::envAttackMs, "Attack", "0", "4000", "1"),
                            makeKnob (ParameterIDs::envHoldMs, "Hold", "0", "2000", "1"),
                            makeKnob (ParameterIDs::envDecayMs, "Decay", "0", "4000", "1"),
                            makeKnob (ParameterIDs::envSustain, "Sustain", "0", "1", "0.001"),
                            makeKnob (ParameterIDs::envReleaseMs, "Release", "0", "4000", "1"),
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "padding", 12 },
                    { "gap", 10 },
                },
                {
                    makeLabel ("Settings"),
                    juce::ValueTree {
                        "Component",
                        {
                            { "flex-direction", "row" },
                            { "gap", 16 },
                            { "align-items", "centre" },
                            { "flex-wrap", "wrap" },
                        },
                        {
                            makeToggle (ParameterIDs::softClipEnabled, "Soft Clip"),
                            makeToggle (ParameterIDs::trueStereoEnabled, "True Stereo"),
                            makeToggle (ParameterIDs::densitySyncEnabled, "Density Sync"),
                            makeToggle (ParameterIDs::densitySyncTripletEnabled, "Triplet"),
                            makeToggle (ParameterIDs::densitySyncDottedEnabled, "Dotted"),
                            makeKnob (ParameterIDs::densitySyncDivision, "Division", "0", "17", "1"),
                            makeKnob (ParameterIDs::maxVoices, "Voices", "1", "16", "1"),
                            juce::ValueTree {
                                "ComboBox",
                                {
                                    { "id", ParameterIDs::interpolationQuality },
                                    { "width", 180 },
                                    { "height", 28 },
                                },
                                {
                                    juce::ValueTree { "Option", { { "text", "Linear" } } },
                                    juce::ValueTree { "Option", { { "text", "Cubic (High Quality)" } } },
                                },
                            },
                            juce::ValueTree {
                                "ComboBox",
                                {
                                    { "id", ParameterIDs::rootKey },
                                    { "width", 140 },
                                    { "height", 28 },
                                },
                                {
                                    juce::ValueTree { "Option", { { "text", "C" } } },
                                    juce::ValueTree { "Option", { { "text", "C#" } } },
                                    juce::ValueTree { "Option", { { "text", "D" } } },
                                    juce::ValueTree { "Option", { { "text", "D#" } } },
                                    juce::ValueTree { "Option", { { "text", "E" } } },
                                    juce::ValueTree { "Option", { { "text", "F" } } },
                                    juce::ValueTree { "Option", { { "text", "F#" } } },
                                    juce::ValueTree { "Option", { { "text", "G" } } },
                                    juce::ValueTree { "Option", { { "text", "G#" } } },
                                    juce::ValueTree { "Option", { { "text", "A" } } },
                                    juce::ValueTree { "Option", { { "text", "A#" } } },
                                    juce::ValueTree { "Option", { { "text", "B" } } },
                                },
                            },
                        },
                    },
                },
            },
        },
    };
}

bool AudioPluginAudioProcessorEditor::hasSupportedSampleExtension (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".flac" || ext == ".mp3";
}

juce::ValueTree AudioPluginAudioProcessorEditor::findNodeWithID (const juce::ValueTree& root,
                                                                  const juce::Identifier& id)
{
    if (! root.isValid())
        return {};

    if (root.getProperty ("id").toString() == id.toString())
        return root;

    for (const auto child : root)
    {
        if (auto found = findNodeWithID (child, id); found.isValid())
            return found;
    }

    return {};
}

void AudioPluginAudioProcessorEditor::attachParameters()
{
    if (rootItem == nullptr)
        return;

    auto& params = processorRef.getParameters();

    const juce::StringArray parameterIDs {
        ParameterIDs::position,
        ParameterIDs::spray,
        ParameterIDs::grainSizeMs,
        ParameterIDs::densityHz,
        ParameterIDs::densitySyncEnabled,
        ParameterIDs::densitySyncDivision,
        ParameterIDs::densitySyncTripletEnabled,
        ParameterIDs::densitySyncDottedEnabled,
        ParameterIDs::pitchSemitones,
        ParameterIDs::shapeType,
        ParameterIDs::lfo1RateHz,
        ParameterIDs::lfo1Depth,
        ParameterIDs::lfo1Shape,
        ParameterIDs::envAttackMs,
        ParameterIDs::envHoldMs,
        ParameterIDs::envDecayMs,
        ParameterIDs::envSustain,
        ParameterIDs::envReleaseMs,
        ParameterIDs::softClipEnabled,
        ParameterIDs::maxVoices,
        ParameterIDs::trueStereoEnabled,
        ParameterIDs::interpolationQuality,
        ParameterIDs::rootKey,
        ParameterIDs::spread,
        ParameterIDs::gain,
    };

    for (const auto& parameterID : parameterIDs)
    {
        auto* item = jive::findItemWithID (*rootItem, parameterID);
        auto* parameter = params.getParameter (parameterID);

        if (item != nullptr && parameter != nullptr)
            item->attachToParameter (parameter, nullptr);
    }
}

void AudioPluginAudioProcessorEditor::setTextNodeContent (const juce::Identifier& id, const juce::String& text)
{
    auto node = findNodeWithID (editorView, id);
    if (! node.isValid())
        return;

    node.setProperty ("text", text, nullptr);
}

void AudioPluginAudioProcessorEditor::updateSampleStatus()
{
    setTextNodeContent ("sample-status", formatSampleStatus (processorRef));
}
