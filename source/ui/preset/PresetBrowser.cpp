#include "PresetBrowser.h"
#include "BinaryData.h"

namespace
{
// Use SitoLookAndFeel color scheme for consistency
const auto backgroundColour = SitoLookAndFeel::getBrowserBackgroundColour();
const auto panelColour = SitoLookAndFeel::getBrowserPanelColour();
const auto cardColour = SitoLookAndFeel::getCardColour();
const auto cardBorderColour = SitoLookAndFeel::getCardBorderColour();
const auto accentColour = SitoLookAndFeel::getAccentColour();
const auto accentGlowColour = SitoLookAndFeel::getAccentGlowColour();
const auto textPrimary = SitoLookAndFeel::getTextPrimaryColour();
const auto textSecondary = SitoLookAndFeel::getTextSecondaryColour();
const auto textMuted = SitoLookAndFeel::getBrowserTextMutedColour();

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

PresetBrowser::PresetBrowser (PresetManager& manager)
    : presetManager (manager)
{
    setSize (520, 600);
    
    // Search box
    searchBox.setTextToShowWhenEmpty ("Search presets...", textMuted);
    searchBox.setFont (juce::Font (juce::FontOptions (14.0f)));
    searchBox.setColour (juce::TextEditor::backgroundColourId, cardColour.withAlpha (0.6f));
    searchBox.setColour (juce::TextEditor::outlineColourId, cardBorderColour.withAlpha (0.4f));
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, accentColour.withAlpha (0.6f));
    searchBox.setColour (juce::TextEditor::textColourId, textPrimary);
    searchBox.onTextChange = [this]
    {
        searchFilter = searchBox.getText();
        updateFilteredPresets();
        repaint();
    };
    addAndMakeVisible (searchBox);
    
    // Save button with icon
    auto saveIcon = loadSVGFromMemory (BinaryData::save_as_svg, BinaryData::save_as_svgSize);
    auto saveIconOver = loadSVGFromMemory (BinaryData::save_as_svg, BinaryData::save_as_svgSize);
    if (saveIcon)
        saveIcon->replaceColour (juce::Colours::black, textSecondary);
    if (saveIconOver)
        saveIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    saveButton.setImages (saveIcon.get(), saveIconOver.get(), saveIconOver.get());
    saveButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    saveButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    saveButton.onClick = [this] { showSaveDialog(); };
    addAndMakeVisible (saveButton);
    
    // Close button with icon  
    auto closeIcon = loadSVGFromMemory (BinaryData::close_svg, BinaryData::close_svgSize);
    auto closeIconOver = loadSVGFromMemory (BinaryData::close_svg, BinaryData::close_svgSize);
    if (closeIcon)
        closeIcon->replaceColour (juce::Colours::black, textSecondary);
    if (closeIconOver)
        closeIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    closeButton.setImages (closeIcon.get(), closeIconOver.get(), closeIconOver.get());
    closeButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible (closeButton);
    
    presetManager.addListener (this);
    refreshPresetList();
    
    startTimerHz (30);
}

PresetBrowser::~PresetBrowser()
{
    presetManager.removeListener (this);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    // Background with subtle gradient
    g.fillAll (backgroundColour);
    
    auto bounds = getLocalBounds().toFloat();
    
    // Main panel
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds.reduced (12.0f), 18.0f);
    
    // Header
    auto header = bounds.reduced (24.0f, 20.0f).removeFromTop (32.0f);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.setColour (textPrimary);
    g.drawText ("Presets", header.toNearestInt(), juce::Justification::centredLeft);
    
    // Preset list background
    if (! presetListBounds.isEmpty())
    {
        auto listArea = presetListBounds.toFloat();
        g.setColour (cardColour.withAlpha (0.5f));
        g.fillRoundedRectangle (listArea, 12.0f);
        g.setColour (cardBorderColour.withAlpha (0.3f));
        g.drawRoundedRectangle (listArea, 12.0f, 1.0f);
    }
    
    // Draw presets
    if (filteredPresets.empty())
    {
        auto emptyArea = presetListBounds.toFloat().reduced (20.0f);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.setColour (textMuted);
        
        if (searchFilter.isNotEmpty())
            g.drawText ("No presets match your search", emptyArea.toNearestInt(), juce::Justification::centred);
        else
            g.drawText ("No presets available\nClick 'Save As...' to create your first preset", emptyArea.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        const auto currentPresetName = presetManager.getCurrentPresetName();
        juce::String lastCategory;
        
        for (size_t i = 0; i < filteredPresets.size(); ++i)
        {
            const auto& preset = filteredPresets[i];
            const auto itemBounds = getPresetBounds (static_cast<int> (i));
            
            if (itemBounds.isEmpty())
                continue;
            
            // Category separator
            const juce::String category = preset.isFactory ? "Factory" : "User";
            if (category != lastCategory)
            {
                lastCategory = category;
                
                if (i > 0)
                {
                    auto sepBounds = itemBounds.toFloat();
                    sepBounds = sepBounds.withY (sepBounds.getY() - 8.0f).withHeight (1.0f);
                    g.setColour (cardBorderColour.withAlpha (0.2f));
                    g.fillRect (sepBounds.reduced (12.0f, 0.0f));
                }
                
                auto catBounds = itemBounds.toFloat().withHeight (20.0f).translated (0.0f, -22.0f);
                g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
                g.setColour (textMuted.withAlpha (0.7f));
                g.drawText (category.toUpperCase(), catBounds.reduced (16.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
            }
            
            const auto isHovered = (static_cast<int> (i) == hoveredPresetIndex);
            const auto isSelected = (static_cast<int> (i) == selectedPresetIndex);
            const auto isCurrent = (preset.name == currentPresetName);
            
            auto itemArea = itemBounds.toFloat().reduced (8.0f, 2.0f);
            
            // Background
            if (isSelected || isHovered)
            {
                g.setColour (accentColour.withAlpha (isSelected ? 0.18f : 0.08f));
                g.fillRoundedRectangle (itemArea, 8.0f);
            }
            
            // Border for current preset
            if (isCurrent)
            {
                g.setColour (accentGlowColour.withAlpha (0.5f));
                g.drawRoundedRectangle (itemArea, 8.0f, 1.5f);
            }
            
            // Preset name
            auto textArea = itemArea.reduced (12.0f, 0.0f);
            g.setFont (juce::Font (juce::FontOptions (14.0f, isCurrent ? juce::Font::bold : juce::Font::plain)));
            g.setColour (isCurrent ? accentGlowColour : (isHovered ? textPrimary : textSecondary));
            g.drawText (preset.name, textArea.toNearestInt(), juce::Justification::centredLeft);
            
            // Factory badge
            if (preset.isFactory)
            {
                auto badgeArea = textArea.removeFromRight (50.0f);
                g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                g.setColour (textMuted.withAlpha (0.5f));
                g.drawText ("FACTORY", badgeArea.toNearestInt(), juce::Justification::centredRight);
            }
        }
    }
}

void PresetBrowser::resized()
{
    auto bounds = getLocalBounds().reduced (24, 20);
    
    // Header
    bounds.removeFromTop (32);
    bounds.removeFromTop (16);
    
    // Search box
    auto searchArea = bounds.removeFromTop (36);
    searchBox.setBounds (searchArea);
    bounds.removeFromTop (12);
    
    // Buttons at bottom
    auto buttonArea = bounds.removeFromBottom (36);
    auto closeArea = buttonArea.removeFromRight (100);
    buttonArea.removeFromRight (8);
    auto saveArea = buttonArea.removeFromRight (120);
    
    saveButton.setBounds (saveArea);
    closeButton.setBounds (closeArea);
    
    bounds.removeFromBottom (12);
    
    // Preset list
    presetListBounds = bounds;
}

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        refreshPresetList();
        searchBox.clear();
        searchBox.grabKeyboardFocus();
    }
}

void PresetBrowser::timerCallback()
{
    // Update hover state
    if (isMouseOver (true))
    {
        const auto mousePos = getMouseXYRelative();
        const auto newHoveredIndex = getPresetIndexAt (mousePos);
        
        if (newHoveredIndex != hoveredPresetIndex)
        {
            hoveredPresetIndex = newHoveredIndex;
            repaint();
        }
    }
    else if (hoveredPresetIndex >= 0)
    {
        hoveredPresetIndex = -1;
        repaint();
    }
}

void PresetBrowser::refreshPresetList()
{
    allPresets = presetManager.getAllPresets();
    updateFilteredPresets();
    
    // Update selected index to match current preset
    const auto currentName = presetManager.getCurrentPresetName();
    selectedPresetIndex = -1;
    
    for (size_t i = 0; i < filteredPresets.size(); ++i)
    {
        if (filteredPresets[i].name == currentName)
        {
            selectedPresetIndex = static_cast<int> (i);
            break;
        }
    }
    
    repaint();
}

void PresetBrowser::updateFilteredPresets()
{
    filteredPresets.clear();
    
    const auto filter = searchFilter.toLowerCase();
    
    for (const auto& preset : allPresets)
    {
        if (filter.isEmpty() || preset.name.toLowerCase().contains (filter))
            filteredPresets.push_back (preset);
    }
}

void PresetBrowser::showSaveDialog()
{
    auto uniqueName = presetManager.generateUniquePresetName ("Untitled Preset");
    
    nameDialog = std::make_unique<PresetNameDialog> (
        "Save Preset",
        "Enter a name for your preset:",
        uniqueName,
        [this] (const juce::String& name)
        {
            // Save callback
            if (presetManager.savePreset (name))
            {
                refreshPresetList();
            }
            // Defer dialog deletion to avoid use-after-free
            juce::MessageManager::callAsync ([this]()
            {
                nameDialog.reset();
                repaint();
            });
        },
        [this]()
        {
            // Cancel callback - defer dialog deletion to avoid use-after-free
            juce::MessageManager::callAsync ([this]()
            {
                nameDialog.reset();
                repaint();
            });
        });
    
    nameDialog->setBounds (getLocalBounds().withSizeKeepingCentre (400, 220));
    addAndMakeVisible (nameDialog.get());
    nameDialog->toFront (true);
}

void PresetBrowser::mouseDown (const juce::MouseEvent& event)
{
    const auto clickedIndex = getPresetIndexAt (event.getPosition());
    
    if (clickedIndex < 0 || clickedIndex >= static_cast<int> (filteredPresets.size()))
        return;
    
    const auto& preset = filteredPresets[static_cast<size_t> (clickedIndex)];
    
    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
    {
        // Right-click: show context menu
        juce::PopupMenu menu;
        menu.setLookAndFeel (&lookAndFeel);
        menu.addItem (1, "Load Preset");
        
        if (! preset.isFactory)
        {
            menu.addSeparator();
            menu.addItem (2, "Rename...");
            menu.addItem (3, "Delete...");
        }
        
        const auto itemBounds = getPresetBounds (clickedIndex);
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea (localAreaToGlobal (itemBounds))
                                .withParentComponent (this),
            [this, preset] (int result)
            {
                if (result == 1)
                {
                    presetManager.loadPreset (preset.file);
                }
                else if (result == 2)
                {
                    showRenameDialog (preset);
                }
                else if (result == 3)
                {
                    showDeleteConfirmation (preset);
                }
            });
    }
    else
    {
        // Left-click: load preset
        presetManager.loadPreset (preset.file);
    }
}

void PresetBrowser::showRenameDialog (const PresetManager::PresetInfo& preset)
{
    if (preset.isFactory)
        return; // Can't rename factory presets
    
    nameDialog = std::make_unique<PresetNameDialog> (
        "Rename Preset",
        "Enter new name for preset:",
        preset.name,
        [this, preset] (const juce::String& newName)
        {
            // Rename callback
            if (presetManager.renamePreset (preset.file, newName))
            {
                refreshPresetList();
            }
            // Defer dialog deletion to avoid use-after-free
            juce::MessageManager::callAsync ([this]()
            {
                nameDialog.reset();
                repaint();
            });
        },
        [this]()
        {
            // Cancel callback
            juce::MessageManager::callAsync ([this]()
            {
                nameDialog.reset();
                repaint();
            });
        });
    
    nameDialog->setBounds (getLocalBounds().withSizeKeepingCentre (400, 220));
    addAndMakeVisible (nameDialog.get());
    nameDialog->toFront (true);
}

void PresetBrowser::showDeleteConfirmation (const PresetManager::PresetInfo& preset)
{
    if (preset.isFactory)
        return; // Can't delete factory presets
    
    deleteConfirmationDialog = std::make_unique<DeleteConfirmationDialog> (
        preset.name,
        [this, preset]()
        {
            if (presetManager.deletePreset (preset.file))
            {
                refreshPresetList();
            }
            // Defer dialog deletion to avoid use-after-free
            juce::MessageManager::callAsync ([this]()
            {
                deleteConfirmationDialog.reset();
                repaint();
            });
        },
        [this]()
        {
            // Cancel callback - defer dialog deletion to avoid use-after-free
            juce::MessageManager::callAsync ([this]()
            {
                deleteConfirmationDialog.reset();
                repaint();
            });
        });
    
    deleteConfirmationDialog->setBounds (getLocalBounds().withSizeKeepingCentre (420, 200));
    addAndMakeVisible (deleteConfirmationDialog.get());
    deleteConfirmationDialog->toFront (true);
}

int PresetBrowser::getPresetIndexAt (juce::Point<int> position) const
{
    if (! presetListBounds.contains (position))
        return -1;
    
    int currentY = presetListBounds.getY();
    juce::String lastCategory;
    
    for (size_t i = 0; i < filteredPresets.size(); ++i)
    {
        const auto& preset = filteredPresets[i];
        const juce::String category = preset.isFactory ? "Factory" : "User";
        
        // Add space for category header
        if (category != lastCategory)
        {
            lastCategory = category;
            if (i > 0)
                currentY += 8; // Separator space
            currentY += 22; // Category header space
        }
        
        const auto itemBounds = juce::Rectangle<int> (presetListBounds.getX(),
                                                       currentY,
                                                       presetListBounds.getWidth(),
                                                       presetItemHeight);
        
        if (itemBounds.contains (position))
            return static_cast<int> (i);
        
        currentY += presetItemHeight;
    }
    
    return -1;
}

juce::Rectangle<int> PresetBrowser::getPresetBounds (int index) const
{
    if (index < 0 || index >= static_cast<int> (filteredPresets.size()))
        return {};
    
    int currentY = presetListBounds.getY();
    juce::String lastCategory;
    
    for (int i = 0; i <= index; ++i)
    {
        const auto& preset = filteredPresets[static_cast<size_t> (i)];
        const juce::String category = preset.isFactory ? "Factory" : "User";
        
        // Add space for category header
        if (category != lastCategory)
        {
            lastCategory = category;
            if (i > 0)
                currentY += 8; // Separator space
            currentY += 22; // Category header space
        }
        
        if (i == index)
        {
            return juce::Rectangle<int> (presetListBounds.getX(),
                                         currentY,
                                         presetListBounds.getWidth(),
                                         presetItemHeight);
        }
        
        currentY += presetItemHeight;
    }
    
    return {};
}

void PresetBrowser::presetListChanged()
{
    refreshPresetList();
}

void PresetBrowser::currentPresetChanged (const juce::String& presetName)
{
    juce::ignoreUnused (presetName);
    refreshPresetList();
}
