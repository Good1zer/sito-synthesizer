#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * PresetManager - Clean, modern preset management for SITO
 * 
 * Features:
 * - Save/Load/Delete/Rename presets
 * - Factory and User preset separation
 * - XML-based storage using JUCE's AudioProcessorValueTreeState
 * - Automatic preset directory management
 */
class PresetManager
{
public:
    struct PresetInfo
    {
        juce::String name;
        juce::File file;
        bool isFactory = false;
        juce::Time modifiedTime;
    };

    explicit PresetManager (juce::AudioProcessor& processor);
    ~PresetManager() = default;

    // === Core Preset Operations ===
    bool savePreset (const juce::String& presetName);
    bool loadPreset (const juce::File& presetFile);
    bool deletePreset (const juce::File& presetFile);
    bool renamePreset (const juce::File& oldFile, const juce::String& newName);
    
    // === Preset Discovery ===
    std::vector<PresetInfo> getAllPresets() const;
    std::vector<PresetInfo> getUserPresets() const;
    std::vector<PresetInfo> getFactoryPresets() const;
    
    // === Current Preset Tracking ===
    juce::String getCurrentPresetName() const { return currentPresetName; }
    bool hasCurrentPreset() const { return currentPresetFile.existsAsFile(); }
    juce::File getCurrentPresetFile() const { return currentPresetFile; }
    void setCurrentPreset (const juce::File& file, const juce::String& name);
    void clearCurrentPreset();
    
    // === Directory Management ===
    juce::File getUserPresetsDirectory() const { return userPresetsDir; }
    juce::File getFactoryPresetsDirectory() const { return factoryPresetsDir; }
    
    // === Utilities ===
    juce::String generateUniquePresetName (const juce::String& baseName = "Untitled Preset") const;
    bool isValidPresetName (const juce::String& name) const;
    
    // === Listeners ===
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void presetListChanged() {}
        virtual void currentPresetChanged (const juce::String& /*presetName*/) {}
    };
    
    void addListener (Listener* listener);
    void removeListener (Listener* listener);

private:
    void initializeDirectories();
    void notifyPresetListChanged();
    void notifyCurrentPresetChanged();
    juce::String sanitizePresetName (const juce::String& name) const;
    
    juce::AudioProcessor& processor;
    juce::File userPresetsDir;
    juce::File factoryPresetsDir;
    juce::File currentPresetFile;
    juce::String currentPresetName;
    juce::ListenerList<Listener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
