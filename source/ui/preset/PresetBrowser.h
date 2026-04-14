#pragma once

#include "PresetManager.h"
#include "PresetNameDialog.h"
#include "DeleteConfirmationDialog.h"
#include "../lookandfeel/SitoLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * PresetBrowser - Modern, elegant preset browser UI
 * 
 * Features:
 * - Clean list view with factory/user separation
 * - Search/filter functionality
 * - Save, Delete, Rename operations
 * - Keyboard navigation
 * - Premium aesthetic matching Minimal Audio / Baby Audio style
 */
class PresetBrowser : public juce::Component,
                      public PresetManager::Listener,
                      private juce::Timer
{
public:
    explicit PresetBrowser (PresetManager& manager);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent& event) override;
    
    // PresetManager::Listener
    void presetListChanged() override;
    void currentPresetChanged (const juce::String& presetName) override;
    
    std::function<void()> onClose;

private:
    void timerCallback() override;
    void refreshPresetList();
    void updateFilteredPresets();
    void showSaveDialog();
    void showRenameDialog (const PresetManager::PresetInfo& preset);
    void showDeleteConfirmation (const PresetManager::PresetInfo& preset);
    int getPresetIndexAt (juce::Point<int> position) const;
    juce::Rectangle<int> getPresetBounds (int index) const;
    
    PresetManager& presetManager;
    SitoLookAndFeel lookAndFeel;
    
    juce::TextEditor searchBox;
    juce::DrawableButton saveButton { "save", juce::DrawableButton::ImageFitted };
    juce::DrawableButton closeButton { "close", juce::DrawableButton::ImageFitted };
    
    std::unique_ptr<PresetNameDialog> nameDialog;
    std::unique_ptr<DeleteConfirmationDialog> deleteConfirmationDialog;
    
    std::vector<PresetManager::PresetInfo> allPresets;
    std::vector<PresetManager::PresetInfo> filteredPresets;
    juce::String searchFilter;
    
    int hoveredPresetIndex = -1;
    int selectedPresetIndex = -1;
    juce::Rectangle<int> presetListBounds;
    
    static constexpr int presetItemHeight = 36;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
