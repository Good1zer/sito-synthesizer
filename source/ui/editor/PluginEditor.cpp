#include "PluginEditor.h"

#include <jive_core/jive_core.h>

namespace
{
constexpr std::array<const char*, 8> modulationTargetLabels
{
    "Position",
    "Spray",
    "Size",
    "Density",
    "Shape",
    "Pitch",
    "Spread",
    "Gain"
};

juce::ValueTree makeLabel (const juce::String& text,
                           const juce::String& id = {},
                           const juce::String& className = {})
{
    juce::ValueTree label { "Text" };
    label.setProperty ("text", text, nullptr);

    if (id.isNotEmpty())
        label.setProperty ("id", id, nullptr);

    if (className.isNotEmpty())
        label.setProperty ("class", className, nullptr);

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
            makeLabel (title, {}, "control-label"),
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

juce::ValueTree makeButton (const juce::String& id, const juce::String& text)
{
    return juce::ValueTree {
        "Button",
        {
            { "id", id },
            { "width", 92 },
            { "height", 32 },
            { "padding", "8 14" },
        },
        {
            makeLabel (text),
        },
    };
}

juce::ValueTree makePageButton (const juce::String& id, const juce::String& text, bool toggled = false)
{
    auto button = makeButton (id, text);
    button.setProperty ("class", "nav-button", nullptr);
    button.setProperty ("toggleable", true, nullptr);
    button.setProperty ("toggle-on-click", false, nullptr);
    button.setProperty ("toggled", toggled, nullptr);
    button.setProperty ("width", 126, nullptr);
    return button;
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
    lastSeenSampleGeneration = processorRef.getLoadedSampleGeneration();
    editorView.addListener (this);
    updateSampleStatus();
    currentPresetChanged (processorRef.getPresetManager().getCurrentPresetName());

    if (auto clearButton = findNodeWithID (editorView, "mod-clear-button"); clearButton.isValid())
    {
        clearAssignmentEvent = std::make_unique<jive::Event> (clearButton, "on-click");
        clearAssignmentEvent->onTrigger = [this]
        {
            if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (selectedModulationTarget))
            {
                processorRef.removeLfo1Assignment (parameterID);
                syncModulationRoutingUi();
            }
        };
    }

    if (auto node = findNodeWithID (editorView, "page-sample-button"); node.isValid())
    {
        samplePageEvent = std::make_unique<jive::Event> (node, "on-click");
        samplePageEvent->onTrigger = [this]
        {
            currentPage = Page::sample;
            syncPageVisibility();
        };
    }

    if (auto node = findNodeWithID (editorView, "page-modulation-button"); node.isValid())
    {
        modulationPageEvent = std::make_unique<jive::Event> (node, "on-click");
        modulationPageEvent->onTrigger = [this]
        {
            currentPage = Page::modulation;
            syncPageVisibility();
        };
    }

    if (auto node = findNodeWithID (editorView, "page-envelope-button"); node.isValid())
    {
        envelopePageEvent = std::make_unique<jive::Event> (node, "on-click");
        envelopePageEvent->onTrigger = [this]
        {
            currentPage = Page::envelope;
            syncPageVisibility();
        };
    }

    if (auto node = findNodeWithID (editorView, "page-settings-button"); node.isValid())
    {
        settingsPageEvent = std::make_unique<jive::Event> (node, "on-click");
        settingsPageEvent->onTrigger = [this]
        {
            currentPage = Page::settings;
            syncPageVisibility();
        };
    }

    syncPageVisibility();
    syncModulationRoutingUi();
    startTimerHz (10);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    stopTimer();
    editorView.removeListener (this);
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
    updateSampleStatus();
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
                  { "foreground", "#dde4f2" },
                  { "font-size", 14 },
                  { "font-family", "Inter" },
                  { "Text",
                    new jive::Object {
                        { "foreground", "#dde4f2" },
                    } },
                  { ".section",
                    new jive::Object {
                        { "background", "#161a22" },
                        { "border-radius", 18 },
                        { "border", "#2a3040" },
                        { "padding", 16 },
                        { "gap", 10 },
                    } },
                  { ".hero",
                    new jive::Object {
                        { "background", "#131726" },
                        { "border-radius", 18 },
                        { "border", "#333a51" },
                        { "padding", 18 },
                        { "gap", 8 },
                    } },
                  { ".surface",
                    new jive::Object {
                        { "background", "#131722" },
                        { "border-radius", 16 },
                        { "border", "#2a3142" },
                        { "padding", 14 },
                        { "gap", 10 },
                    } },
                  { ".brand",
                    new jive::Object {
                        { "font-size", 28 },
                        { "font-weight", "bold" },
                        { "foreground", "#f5f7ff" },
                    } },
                  { ".eyebrow",
                    new jive::Object {
                        { "foreground", "#9aa6be" },
                        { "font-size", 12 },
                    } },
                  { ".status",
                    new jive::Object {
                        { "foreground", "#c9d2e6" },
                        { "font-size", 15 },
                    } },
                  { ".sub",
                    new jive::Object {
                        { "foreground", "#8f9bb0" },
                        { "font-size", 13 },
                    } },
                  { ".page-title",
                    new jive::Object {
                        { "font-size", 22 },
                        { "font-weight", "bold" },
                        { "foreground", "#f5f7ff" },
                    } },
                  { ".panel-title",
                    new jive::Object {
                        { "font-size", 16 },
                        { "font-weight", "bold" },
                        { "foreground", "#eef2ff" },
                    } },
                  { ".nav-button",
                    new jive::Object {
                        { "background", "#161a22" },
                        { "border-radius", 12 },
                        { "border", "#2d3342" },
                        { "foreground", "#cdd5e3" },
                    } },
                  { "Button",
                    new jive::Object {
                        { "background", "#1b2030" },
                        { "border-radius", 10 },
                        { "border", "#3b4260" },
                        { "padding", "8 12" },
                    } },
                  { "Knob",
                    new jive::Object {
                        { "background", "#121622" },
                        { "border-radius", 999 },
                        { "border", "#414866" },
                    } },
                  { "ComboBox",
                    new jive::Object {
                        { "background", "#121723" },
                        { "border-radius", 10 },
                        { "border", "#414866" },
                        { "foreground", "#f0f4ff" },
                    } },
                  { "Checkbox",
                    new jive::Object {
                        { "foreground", "#d8e0ef" },
                    } },
                  { ".control",
                    new jive::Object {
                        { "background", "#141924" },
                        { "border-radius", 12 },
                        { "padding", 10 },
                        { "gap", 10 },
                    } },
                  { ".control-label",
                    new jive::Object {
                        { "foreground", "#cfd8ea" },
                        { "font-size", 13 },
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
                            { "gap", 4 },
                        },
                        {
                            makeLabel ("SITO", "title", "brand"),
                            makeLabel ("Granular motion instrument", {}, "eyebrow"),
                            makeLabel ("No sample loaded", "sample-status", "status"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "hero" },
                            { "align-items", "flex-end" },
                            { "padding", "8 12" },
                            { "gap", 2 },
                        },
                        {
                            makeLabel ("Preset", "preset-caption", "eyebrow"),
                            makeLabel ("Init", "preset-name", "panel-title"),
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "flex-direction", "row" },
                    { "gap", 12 },
                    { "align-items", "centre" },
                    { "justify-content", "centre" },
                },
                {
                    makePageButton ("page-sample-button", "SAMPLE", true),
                    makePageButton ("page-modulation-button", "MODULATION"),
                    makePageButton ("page-envelope-button", "ENVELOPE"),
                    makePageButton ("page-settings-button", "SETTINGS"),
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "id", "page-sample" },
                    { "padding", 12 },
                    { "flex-grow", 1.0 },
                    { "gap", 14 },
                },
                {
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "hero" },
                        },
                        {
                            makeLabel ("Sample", "sample-page-title", "page-title"),
                            makeLabel ("Drop sample here or load from preset. Granular source controls live below.", "sample-page-subtitle", "sub"),
                            makeLabel ("No sample loaded", "sample-hero-status", "status"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                        },
                        {
                            makeLabel ("Source Controls", {}, "panel-title"),
                            makeLabel ("Core grain source, timing, pitch, spread, output level.", {}, "sub"),
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
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "id", "page-modulation" },
                    { "padding", 12 },
                    { "flex-grow", 1.0 },
                    { "gap", 10 },
                    { "visible", false },
                },
                {
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "hero" },
                        },
                        {
                            makeLabel ("Modulation", "modulation-page-title", "page-title"),
                            makeLabel ("LFO shape and routing. Clean parity step before richer visual modulation canvas.", "modulation-page-subtitle", "sub"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                        },
                        {
                            makeLabel ("LFO Shape", {}, "panel-title"),
                            makeLabel ("Speed, depth, waveform morph.", {}, "sub"),
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
                                },
                            },
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                            { "padding", 12 },
                            { "gap", 10 },
                        },
                        {
                            makeLabel ("LFO Routing", {}, "panel-title"),
                            makeLabel ("Assign LFO 1 to one target at a time.", "mod-status", "sub"),
                            juce::ValueTree {
                                "Component",
                                {
                                    { "flex-direction", "row" },
                                    { "flex-wrap", "wrap" },
                                    { "gap", 12 },
                                    { "align-items", "centre" },
                                },
                                {
                                    juce::ValueTree {
                                        "ComboBox",
                                        {
                                            { "id", "mod-target-select" },
                                            { "width", 160 },
                                            { "height", 28 },
                                            { "selected", 0 },
                                        },
                                        {
                                            juce::ValueTree { "Option", { { "text", "Position" } } },
                                            juce::ValueTree { "Option", { { "text", "Spray" } } },
                                            juce::ValueTree { "Option", { { "text", "Size" } } },
                                            juce::ValueTree { "Option", { { "text", "Density" } } },
                                            juce::ValueTree { "Option", { { "text", "Shape" } } },
                                            juce::ValueTree { "Option", { { "text", "Pitch" } } },
                                            juce::ValueTree { "Option", { { "text", "Spread" } } },
                                            juce::ValueTree { "Option", { { "text", "Gain" } } },
                                        },
                                    },
                                    makeKnob ("mod-amount", "Amount", "-1", "1", "0.001"),
                                    makeToggle ("mod-bypass", "Bypass"),
                                    makeButton ("mod-clear-button", "Clear"),
                                },
                            },
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "id", "page-envelope" },
                    { "padding", 12 },
                    { "flex-grow", 1.0 },
                    { "gap", 10 },
                    { "visible", false },
                },
                {
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "hero" },
                        },
                        {
                            makeLabel ("Envelope", "envelope-page-title", "page-title"),
                            makeLabel ("AHDSR note contour. Functional now; richer envelope visualization comes later.", "envelope-page-subtitle", "sub"),
                            makeLabel ("AHDSR", "envelope-hero-title", "panel-title"),
                            makeLabel ("Attack, hold, decay, sustain, release shape each note before grain tail fades.", "envelope-hero-subtitle", "sub"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                        },
                        {
                            makeLabel ("Envelope Controls", {}, "panel-title"),
                            makeLabel ("Dial note contour in one place, separate from sample and modulation pages.", {}, "sub"),
                            juce::ValueTree {
                                "Component",
                                {
                                    { "flex-direction", "row" },
                                    { "flex-wrap", "wrap" },
                                    { "gap", 10 },
                                },
                                {
                                    makeKnob (ParameterIDs::envAttackMs, "Attack", "0", "4000", "1"),
                                    makeKnob (ParameterIDs::envHoldMs, "Hold", "0", "2000", "1"),
                                    makeKnob (ParameterIDs::envDecayMs, "Decay", "0", "4000", "1"),
                                    makeKnob (ParameterIDs::envSustain, "Sustain", "0", "1", "0.001"),
                                    makeKnob (ParameterIDs::envReleaseMs, "Release", "0", "4000", "1"),
                                },
                            },
                        },
                    },
                },
            },

            juce::ValueTree {
                "Component",
                {
                    { "class", "section" },
                    { "id", "page-settings" },
                    { "padding", 12 },
                    { "flex-grow", 1.0 },
                    { "gap", 10 },
                    { "visible", false },
                },
                {
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "hero" },
                        },
                        {
                            makeLabel ("Settings", "settings-page-title", "page-title"),
                            makeLabel ("Engine utilities and playback behavior.", "settings-page-subtitle", "sub"),
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                        },
                        {
                            makeLabel ("Playback", {}, "panel-title"),
                            makeLabel ("Voice count, sync behavior, clipper, stereo, interpolation.", {}, "sub"),
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
                                    juce::ValueTree {
                                        "ComboBox",
                                        {
                                            { "id", ParameterIDs::densitySyncDivision },
                                            { "width", 160 },
                                            { "height", 28 },
                                        },
                                        {
                                            juce::ValueTree { "Option", { { "text", "1/1" } } },
                                            juce::ValueTree { "Option", { { "text", "1/2" } } },
                                            juce::ValueTree { "Option", { { "text", "1/4" } } },
                                            juce::ValueTree { "Option", { { "text", "1/8" } } },
                                            juce::ValueTree { "Option", { { "text", "1/16" } } },
                                            juce::ValueTree { "Option", { { "text", "1/32" } } },
                                            juce::ValueTree { "Option", { { "text", "1/1." } } },
                                            juce::ValueTree { "Option", { { "text", "1/2." } } },
                                            juce::ValueTree { "Option", { { "text", "1/4." } } },
                                            juce::ValueTree { "Option", { { "text", "1/8." } } },
                                            juce::ValueTree { "Option", { { "text", "1/16." } } },
                                            juce::ValueTree { "Option", { { "text", "1/32." } } },
                                            juce::ValueTree { "Option", { { "text", "1/1T" } } },
                                            juce::ValueTree { "Option", { { "text", "1/2T" } } },
                                            juce::ValueTree { "Option", { { "text", "1/4T" } } },
                                            juce::ValueTree { "Option", { { "text", "1/8T" } } },
                                            juce::ValueTree { "Option", { { "text", "1/16T" } } },
                                            juce::ValueTree { "Option", { { "text", "1/32T" } } },
                                        },
                                    },
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
                                },
                            },
                        },
                    },
                    juce::ValueTree {
                        "Component",
                        {
                            { "class", "surface" },
                        },
                        {
                            makeLabel ("Pitch Reference", {}, "panel-title"),
                            makeLabel ("Choose root note for imported sample transposition.", {}, "sub"),
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
    const auto status = formatSampleStatus (processorRef);
    setTextNodeContent ("sample-status", status);
    setTextNodeContent ("sample-hero-status", status);
}

void AudioPluginAudioProcessorEditor::syncPageVisibility()
{
    auto setPageState = [this] (const juce::String& pageID,
                                const juce::String& buttonID,
                                Page page,
                                const juce::String& bg,
                                const juce::String& border)
    {
        if (auto pageNode = findNodeWithID (editorView, pageID); pageNode.isValid())
            pageNode.setProperty ("visible", currentPage == page, nullptr);

        if (auto buttonNode = findNodeWithID (editorView, buttonID); buttonNode.isValid())
        {
            const auto active = currentPage == page;
            buttonNode.setProperty ("toggled", active, nullptr);
            buttonNode.setProperty ("background", active ? bg : "#161a22", nullptr);
            buttonNode.setProperty ("border", active ? border : "#2d3342", nullptr);
            buttonNode.setProperty ("foreground", active ? "#f5f7ff" : "#cdd5e3", nullptr);
        }
    };

    setPageState ("page-sample", "page-sample-button", Page::sample, "#6f57d9", "#9d8cff");
    setPageState ("page-modulation", "page-modulation-button", Page::modulation, "#3f6ed8", "#79a8ff");
    setPageState ("page-envelope", "page-envelope-button", Page::envelope, "#d26755", "#ffab8d");
    setPageState ("page-settings", "page-settings-button", Page::settings, "#3c915d", "#83d8a2");
}

void AudioPluginAudioProcessorEditor::syncModulationRoutingUi()
{
    const juce::ScopedValueSetter<bool> syncing (isSyncingModulationUi, true);

    selectedModulationTarget = juce::jlimit (0,
                                             static_cast<int> (modulationTargetLabels.size()) - 1,
                                             selectedModulationTarget);

    if (auto targetNode = findNodeWithID (editorView, "mod-target-select"); targetNode.isValid())
        targetNode.setProperty ("selected", selectedModulationTarget, nullptr);

    if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (selectedModulationTarget))
    {
        if (auto amountNode = findNodeWithID (editorView, "mod-amount"); amountNode.isValid())
            amountNode.setProperty ("value", processorRef.getLfo1AssignmentAmount (parameterID), nullptr);

        if (auto bypassNode = findNodeWithID (editorView, "mod-bypass"); bypassNode.isValid())
            bypassNode.setProperty ("toggled", processorRef.isLfo1AssignmentBypassed (parameterID), nullptr);

        const auto amount = processorRef.getLfo1AssignmentAmount (parameterID);
        const auto percent = static_cast<int> (std::round (juce::jlimit (-1.0f, 1.0f, amount) * 100.0f));
        const auto bypassed = processorRef.isLfo1AssignmentBypassed (parameterID);
        setTextNodeContent ("mod-status",
                            juce::String (modulationTargetLabels[static_cast<size_t> (selectedModulationTarget)])
                                + ": "
                                + juce::String (percent)
                                + "% "
                                + (bypassed ? "(bypassed)" : ""));
    }
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    const auto sampleGeneration = processorRef.getLoadedSampleGeneration();
    if (sampleGeneration != lastSeenSampleGeneration)
    {
        lastSeenSampleGeneration = sampleGeneration;
        updateSampleStatus();
    }

    syncModulationRoutingUi();
}

void AudioPluginAudioProcessorEditor::valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged,
                                                                const juce::Identifier& property)
{
    if (isSyncingModulationUi)
        return;

    const auto id = treeWhosePropertyHasChanged.getProperty ("id").toString();

    if (id == "mod-target-select" && property == juce::Identifier { "selected" })
    {
        selectedModulationTarget = juce::jlimit (0,
                                                 static_cast<int> (modulationTargetLabels.size()) - 1,
                                                 static_cast<int> (treeWhosePropertyHasChanged.getProperty ("selected", 0)));
        syncModulationRoutingUi();
        return;
    }

    const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (selectedModulationTarget);
    if (parameterID == nullptr)
        return;

    if (id == "mod-amount" && property == juce::Identifier { "value" })
    {
        processorRef.setLfo1AssignmentAmount (parameterID,
                                              static_cast<float> (treeWhosePropertyHasChanged.getProperty ("value", 0.0)));
        syncModulationRoutingUi();
        return;
    }

    if (id == "mod-bypass" && property == juce::Identifier { "toggled" })
    {
        processorRef.setLfo1AssignmentBypassed (parameterID,
                                                static_cast<bool> (treeWhosePropertyHasChanged.getProperty ("toggled", false)));
        syncModulationRoutingUi();
    }
}
