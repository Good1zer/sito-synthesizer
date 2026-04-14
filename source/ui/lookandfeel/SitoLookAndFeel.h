#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * SitoLookAndFeel - Custom look and feel for the Sito granular synthesizer
 * 
 * Provides a modern, dark UI aesthetic inspired by Minimal Audio Current 2
 * with deep backgrounds, rich purple-blue accents, and clean typography.
 */
class SitoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SitoLookAndFeel();
    
    // Color scheme getters - inspired by Minimal Audio Current 2
    // Deep, rich dark palette with premium feel
    static juce::Colour getBackgroundColour() noexcept { return juce::Colour (0xff0f0f12); }
    static juce::Colour getPanelColour() noexcept { return juce::Colour (0xff17171c); }
    static juce::Colour getCardColour() noexcept { return juce::Colour (0xff1d1d24); }
    static juce::Colour getCardBorderColour() noexcept { return juce::Colour (0xff2a2a35); }
    static juce::Colour getAccentColour() noexcept { return juce::Colour (0xff8b5cf6); }
    static juce::Colour getAccentGlowColour() noexcept { return juce::Colour (0xffa78bfa); }
    static juce::Colour getTextPrimaryColour() noexcept { return juce::Colour (0xffe8e8ed); }
    static juce::Colour getTextSecondaryColour() noexcept { return juce::Colour (0xffa0a0a8); }
    static juce::Colour getTextMutedColour() noexcept { return juce::Colour (0xff7a7a85); }
    static juce::Colour getPanelBorderColour() noexcept { return juce::Colour (0xff252530); }
    
    // Browser-specific colors
    static juce::Colour getBrowserBackgroundColour() noexcept { return juce::Colour (0xff0a0a0d); }
    static juce::Colour getBrowserPanelColour() noexcept { return juce::Colour (0xff14141a); }
    static juce::Colour getBrowserTextMutedColour() noexcept { return juce::Colour (0xff8a8a95); }
    
    // Font getters
    static juce::Font getPresetBarFont() { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    static juce::Font getDefaultPopupMenuFont() { return juce::Font (juce::FontOptions (13.0f, juce::Font::bold)); }
    static juce::Font getLabelFont() { return juce::Font (juce::FontOptions (11.0f)); }
    
    // Layout constants
    static float getCornerRadius() noexcept { return 8.0f; }
    static float getSmallCornerRadius() noexcept { return 7.0f; }
    static float getLargeCornerRadius() noexcept { return 22.0f; }
    static float getXLargeCornerRadius() noexcept { return 26.0f; }
    
    // JUCE LookAndFeel overrides
    void drawRotarySlider (juce::Graphics&,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&,
                           juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&,
                               juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&,
                       int width,
                       int height,
                       bool isButtonDown,
                       int buttonX,
                       int buttonY,
                       int buttonW,
                       int buttonH,
                       juce::ComboBox&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&,
                                             int width,
                                             int height,
                                             const juce::PopupMenu::Options&) override;
    
    void drawPopupMenuItem (juce::Graphics&,
                            const juce::Rectangle<int>& area,
                            bool isSeparator,
                            bool isActive,
                            bool isHighlighted,
                            bool isTicked,
                            bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;
    
    juce::Font getPopupMenuFont() override;
    
    void getIdealPopupMenuItemSize (const juce::String& text,
                                    bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth,
                                    int& idealHeight) override;
    
    int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override;
};
