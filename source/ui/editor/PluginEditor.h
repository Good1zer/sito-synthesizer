#pragma once

#include "plugin/PluginProcessor.h"

#include <jive_layouts/jive_layouts.h>

class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              public juce::FileDragAndDropTarget,
                                              public PresetManager::Listener
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void presetListChanged() override;
    void currentPresetChanged (const juce::String& presetName) override;

private:
    static juce::ValueTree createEditorView();

    static bool hasSupportedSampleExtension (const juce::File& file);
    static juce::ValueTree findNodeWithID (const juce::ValueTree& root, const juce::Identifier& id);

    void attachParameters();
    void setTextNodeContent (const juce::Identifier& id, const juce::String& text);
    void updateSampleStatus();

    AudioPluginAudioProcessor& processorRef;
    jive::Interpreter interpreter;
    juce::ValueTree editorView;
    std::unique_ptr<jive::GuiItem> rootItem;
    juce::Component* rootComponent = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
