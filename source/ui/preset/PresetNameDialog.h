#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class SitoLookAndFeel;

class PresetNameDialog : public juce::Component
{
public:
    PresetNameDialog (const juce::String& title,
                      const juce::String& message,
                      const juce::String& defaultName,
                      std::function<void (const juce::String&)> onSave,
                      std::function<void()> onCancel);
    ~PresetNameDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    juce::Label titleLabel;
    juce::Label messageLabel;
    juce::TextEditor nameEditor;
    juce::DrawableButton saveButton { "save", juce::DrawableButton::ImageFitted };
    juce::DrawableButton cancelButton { "cancel", juce::DrawableButton::ImageFitted };
    
    std::unique_ptr<SitoLookAndFeel> lookAndFeel;

    std::function<void (const juce::String&)> onSaveCallback;
    std::function<void()> onCancelCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetNameDialog)
};
