#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class SitoLookAndFeel;

class DeleteConfirmationDialog : public juce::Component
{
public:
    DeleteConfirmationDialog (const juce::String& presetName,
                              std::function<void()> onDelete,
                              std::function<void()> onCancel);
    ~DeleteConfirmationDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String presetName;
    juce::DrawableButton deleteButton { "delete", juce::DrawableButton::ImageFitted };
    juce::DrawableButton cancelButton { "cancel", juce::DrawableButton::ImageFitted };
    
    std::unique_ptr<SitoLookAndFeel> lookAndFeel;

    std::function<void()> onDeleteCallback;
    std::function<void()> onCancelCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeleteConfirmationDialog)
};
