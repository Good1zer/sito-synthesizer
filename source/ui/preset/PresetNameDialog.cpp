#include "PresetNameDialog.h"
#include "../lookandfeel/SitoLookAndFeel.h"
#include "BinaryData.h"

namespace
{
const auto backgroundColour = SitoLookAndFeel::getBackgroundColour();
const auto panelColour = SitoLookAndFeel::getPanelColour();
const auto cardColour = SitoLookAndFeel::getCardColour();
const auto cardBorderColour = SitoLookAndFeel::getCardBorderColour();
const auto accentColour = SitoLookAndFeel::getAccentColour();
const auto accentGlowColour = SitoLookAndFeel::getAccentGlowColour();
const auto textPrimary = juce::Colour (0xfff7efff);
const auto textSecondary = juce::Colour (0xffccbce2);
const auto textMuted = juce::Colour (0xff9b8db5);

std::unique_ptr<juce::Drawable> loadSVGFromMemory (const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return nullptr;

    const juce::String svgText (static_cast<const char*> (data), size);
    auto xml = juce::parseXML (svgText);
    
    if (xml == nullptr)
        return nullptr;

    return juce::Drawable::createFromSVG (*xml);
}
}

PresetNameDialog::PresetNameDialog (const juce::String& title,
                                    const juce::String& message,
                                    const juce::String& defaultName,
                                    std::function<void (const juce::String&)> onSave,
                                    std::function<void()> onCancel)
    : onSaveCallback (std::move (onSave)),
      onCancelCallback (std::move (onCancel))
{
    setSize (400, 220);
    
    // Create LookAndFeel
    lookAndFeel = std::make_unique<SitoLookAndFeel>();

    // Title
    titleLabel.setText (title, juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, textPrimary);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // Message
    messageLabel.setText (message, juce::dontSendNotification);
    messageLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    messageLabel.setColour (juce::Label::textColourId, textSecondary);
    messageLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (messageLabel);

    // Name editor
    nameEditor.setText (defaultName);
    nameEditor.setFont (juce::Font (juce::FontOptions (14.0f)));
    nameEditor.setColour (juce::TextEditor::backgroundColourId, cardColour.withAlpha (0.6f));
    nameEditor.setColour (juce::TextEditor::outlineColourId, cardBorderColour.withAlpha (0.4f));
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, accentColour.withAlpha (0.6f));
    nameEditor.setColour (juce::TextEditor::textColourId, textPrimary);
    nameEditor.setColour (juce::TextEditor::highlightColourId, accentColour.withAlpha (0.3f));
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.onReturnKey = [this]
    {
        if (onSaveCallback && nameEditor.getText().isNotEmpty())
            onSaveCallback (nameEditor.getText());
    };
    nameEditor.onEscapeKey = [this]
    {
        if (onCancelCallback)
            onCancelCallback();
    };
    addAndMakeVisible (nameEditor);

    // Save button with icon
    auto saveIcon = loadSVGFromMemory (BinaryData::save_svg, BinaryData::save_svgSize);
    auto saveIconOver = loadSVGFromMemory (BinaryData::save_svg, BinaryData::save_svgSize);
    if (saveIcon)
        saveIcon->replaceColour (juce::Colours::black, textSecondary);
    if (saveIconOver)
        saveIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    saveButton.setImages (saveIcon.get(), saveIconOver.get(), saveIconOver.get());
    saveButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    saveButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    saveButton.setLookAndFeel (lookAndFeel.get());
    saveButton.onClick = [this]
    {
        if (onSaveCallback && nameEditor.getText().isNotEmpty())
            onSaveCallback (nameEditor.getText());
    };
    addAndMakeVisible (saveButton);

    // Cancel button with icon
    auto cancelIcon = loadSVGFromMemory (BinaryData::close_svg, BinaryData::close_svgSize);
    auto cancelIconOver = loadSVGFromMemory (BinaryData::close_svg, BinaryData::close_svgSize);
    if (cancelIcon)
        cancelIcon->replaceColour (juce::Colours::black, textSecondary);
    if (cancelIconOver)
        cancelIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    cancelButton.setImages (cancelIcon.get(), cancelIconOver.get(), cancelIconOver.get());
    cancelButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    cancelButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    cancelButton.setLookAndFeel (lookAndFeel.get());
    cancelButton.onClick = [this]
    {
        if (onCancelCallback)
            onCancelCallback();
    };
    addAndMakeVisible (cancelButton);
}

PresetNameDialog::~PresetNameDialog()
{
    saveButton.setLookAndFeel (nullptr);
    cancelButton.setLookAndFeel (nullptr);
}

void PresetNameDialog::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Main panel
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds.reduced (12.0f), 18.0f);

    // Border
    g.setColour (cardBorderColour.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (12.0f), 18.0f, 1.0f);
}

void PresetNameDialog::resized()
{
    auto bounds = getLocalBounds().reduced (24, 20);

    // Title
    titleLabel.setBounds (bounds.removeFromTop (32));
    bounds.removeFromTop (12);

    // Message
    messageLabel.setBounds (bounds.removeFromTop (20));
    bounds.removeFromTop (12);

    // Name editor
    nameEditor.setBounds (bounds.removeFromTop (36));
    bounds.removeFromTop (20);

    // Buttons
    auto buttonArea = bounds.removeFromTop (36);
    auto cancelArea = buttonArea.removeFromRight (100);
    buttonArea.removeFromRight (8);
    auto saveArea = buttonArea.removeFromRight (100);

    saveButton.setBounds (saveArea);
    cancelButton.setBounds (cancelArea);
}

void PresetNameDialog::visibilityChanged()
{
    if (isVisible())
    {
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }
}
