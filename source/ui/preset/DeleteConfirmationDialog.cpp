#include "DeleteConfirmationDialog.h"
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
const auto warningColour = juce::Colour (0xffff9800);

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

DeleteConfirmationDialog::DeleteConfirmationDialog (const juce::String& presetName,
                                                    std::function<void()> onDelete,
                                                    std::function<void()> onCancel)
    : presetName (presetName),
      onDeleteCallback (std::move (onDelete)),
      onCancelCallback (std::move (onCancel))
{
    setSize (420, 220);
    
    // Create LookAndFeel
    lookAndFeel = std::make_unique<SitoLookAndFeel>();

    // Delete button with icon
    auto deleteIcon = loadSVGFromMemory (BinaryData::delete_svg, BinaryData::delete_svgSize);
    auto deleteIconOver = loadSVGFromMemory (BinaryData::delete_svg, BinaryData::delete_svgSize);
    if (deleteIcon)
        deleteIcon->replaceColour (juce::Colours::black, warningColour);
    if (deleteIconOver)
        deleteIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    deleteButton.setImages (deleteIcon.get(), deleteIconOver.get(), deleteIconOver.get());
    deleteButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    deleteButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    deleteButton.setLookAndFeel (lookAndFeel.get());
    deleteButton.onClick = [this]
    {
        if (onDeleteCallback)
            onDeleteCallback();
    };
    addAndMakeVisible (deleteButton);

    // Cancel button with icon
    auto cancelIcon = loadSVGFromMemory (BinaryData::cancel_svg, BinaryData::cancel_svgSize);
    auto cancelIconOver = loadSVGFromMemory (BinaryData::cancel_svg, BinaryData::cancel_svgSize);
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

DeleteConfirmationDialog::~DeleteConfirmationDialog()
{
    deleteButton.setLookAndFeel (nullptr);
    cancelButton.setLookAndFeel (nullptr);
}

void DeleteConfirmationDialog::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Main panel
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds.reduced (12.0f), 18.0f);

    // Border
    g.setColour (cardBorderColour.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (12.0f), 18.0f, 1.0f);

    // Title
    auto contentBounds = bounds.reduced (24.0f, 20.0f);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.setColour (textPrimary);
    g.drawText ("Delete Preset?", contentBounds.removeFromTop (28).toNearestInt(), juce::Justification::centredLeft);
    
    contentBounds.removeFromTop (8);

    // Message with preset name
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.setColour (textSecondary);
    const auto message = "Are you sure you want to delete '" + presetName + "'?\nThis cannot be undone.";
    g.drawText (message, contentBounds.removeFromTop (50).toNearestInt(), juce::Justification::topLeft);
}

void DeleteConfirmationDialog::resized()
{
    auto bounds = getLocalBounds().reduced (24, 20);

    // Skip title and message area
    bounds.removeFromTop (28);
    bounds.removeFromTop (8);
    bounds.removeFromTop (50);
    bounds.removeFromTop (12);

    // Buttons
    auto buttonArea = bounds.removeFromTop (36);
    auto cancelArea = buttonArea.removeFromRight (100);
    buttonArea.removeFromRight (8);
    auto deleteArea = buttonArea.removeFromRight (100);

    deleteButton.setBounds (deleteArea);
    cancelButton.setBounds (cancelArea);
}