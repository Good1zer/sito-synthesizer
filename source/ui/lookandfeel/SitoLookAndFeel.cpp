#include "SitoLookAndFeel.h"
#include "dsp/DSPUtils.h"
#include <cmath>

SitoLookAndFeel::SitoLookAndFeel()
{
    // Set default colors for the look and feel
    setColour (juce::ResizableWindow::backgroundColourId, getBackgroundColour());
    setColour (juce::DocumentWindow::backgroundColourId, getBackgroundColour());
    setColour (juce::ComboBox::textColourId, getTextPrimaryColour());
    setColour (juce::ComboBox::arrowColourId, getTextSecondaryColour());
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
}

void SitoLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                        int x,
                                        int y,
                                        int width,
                                        int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (10.0f, 8.0f);

    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    
    // Apply hover scale animation (1.0 → 1.05x)
    const auto hoverAlpha = static_cast<float> (slider.getProperties().getWithDefault ("hoverAlpha", 0.0f));
    const auto scale = 1.0f + (hoverAlpha * 0.05f);
    const auto scaledDiameter = diameter * scale;
    
    const auto knobBounds = juce::Rectangle<float> (scaledDiameter, scaledDiameter).withCentre (
        juce::Point<float> (bounds.getCentreX(), bounds.getCentreY() - 4.0f));
    const auto centre = knobBounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto trackRadius = radius - 7.0f;
    const auto angle = juce::jmap (sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
    const auto isActive = slider.isMouseOverOrDragging();
    const auto activeAccent = isActive ? getAccentGlowColour() : getAccentColour();
    const auto modAmount = static_cast<float> (slider.getProperties().getWithDefault ("modAmount", 0.0f));
    const auto modPreview = static_cast<float> (slider.getProperties().getWithDefault ("modPreview", 0.0f));
    const auto modSelected = static_cast<bool> (slider.getProperties().getWithDefault ("modSelected", false));
    const auto modBypassed = static_cast<bool> (slider.getProperties().getWithDefault ("modBypassed", false));
    const auto isPrimary = static_cast<bool> (slider.getProperties().getWithDefault ("isPrimary", false));

    // Premium knob background with subtle gradient feel
    g.setColour (juce::Colour (0xff1a1a20));
    g.fillEllipse (knobBounds);

    // Soft outer glow for depth - differentiated for primary controls
    const auto glowIntensity = isPrimary ? 0.35f : 0.25f;
    const auto glowRadius = isPrimary ? 16.0f : 12.0f;
    g.setColour (activeAccent.withAlpha (isActive ? glowIntensity : glowIntensity * 0.5f));
    g.fillEllipse (knobBounds.expanded (glowRadius * 0.2f));

    // Refined knob border - thinner and more elegant
    g.setColour (getCardBorderColour().withAlpha (isActive ? 0.55f : 0.35f));
    g.drawEllipse (knobBounds, 0.8f);

    // Inner highlight for premium feel
    g.setColour (juce::Colour (0xffffffff).withAlpha (isActive ? 0.08f : 0.04f));
    g.drawEllipse (knobBounds.reduced (2.0f), 1.2f);

    // Subtle inner shadow
    g.setColour (juce::Colour (0xff000000).withAlpha (0.15f));
    g.drawEllipse (knobBounds.reduced (5.0f), 6.0f);

    // Value arc - differentiated thickness for primary controls
    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y,
                            trackRadius, trackRadius,
                            0.0f,
                            rotaryStartAngle,
                            angle,
                            true);

    g.setColour (activeAccent);
    const auto arcThickness = isPrimary ? 4.5f : 3.5f;
    g.strokePath (valueArc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Modulation visualization
    if (std::abs (modAmount) > 0.001f)
    {
        const auto currentNormalized = juce::jlimit (0.0f, 1.0f, sliderPosProportional + (modAmount * modPreview * 0.5f));
        const auto currentAngle = juce::jmap (currentNormalized, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        const auto modColour = (modAmount >= 0.0f ? getAccentGlowColour() : juce::Colour (0xffff7aa8))
                                   .withAlpha (modBypassed ? 0.28f : (modSelected ? 0.92f : 0.72f));
        const auto modRadius = trackRadius + 5.5f;

        juce::Path modulationArc;
        modulationArc.addCentredArc (centre.x, centre.y,
                                     modRadius, modRadius,
                                     0.0f,
                                     juce::jmin (angle, currentAngle),
                                     juce::jmax (angle, currentAngle),
                                     true);

        g.setColour (modColour.withAlpha (modBypassed ? 0.08f : (modSelected ? 0.22f : 0.12f)));
        g.drawEllipse (knobBounds.expanded (5.5f), 0.8f);
        g.setColour (modColour);
        g.strokePath (modulationArc,
                      juce::PathStrokeType (modSelected ? 2.8f : 2.0f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
        
        // MOD badge indicator in top-right corner
        auto badgeArea = juce::Rectangle<float> (knobBounds.getRight() - 20.0f, knobBounds.getY(), 20.0f, 14.0f);
        
        g.setColour (getAccentColour().withAlpha (0.8f));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        
        g.setColour (getTextPrimaryColour());
        g.setFont (juce::Font (8.0f, juce::Font::bold));
        g.drawFittedText ("MOD", badgeArea.toNearestInt(), juce::Justification::centred, 1);
        
        // Pulse glow around knob when modulation is active
        float pulsePhase = std::fmod (static_cast<float>(juce::Time::getMillisecondCounterHiRes()) / 1500.0f, 1.0f);
        float pulseAlpha = 0.2f + 0.3f * std::sin (pulsePhase * juce::MathConstants<float>::twoPi);
        
        g.setColour (getAccentGlowColour().withAlpha (pulseAlpha));
        g.drawEllipse (knobBounds.expanded (4.0f), 1.5f);
    }

    // Special visualization for shape parameter
    if (slider.getName() == "shape")
    {
        auto curveBounds = knobBounds.reduced (radius * 0.50f, radius * 0.52f);
        juce::Path shapePath;

        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (knobBounds.reduced (radius * 0.34f).toNearestInt());

        const auto shapeValue = juce::jlimit (0.0f, 100.0f, static_cast<float> (slider.getValue()));
        const auto centreY = curveBounds.getCentreY();
        const auto amplitude = curveBounds.getHeight() * 0.46f;

        for (int i = 0; i < 64; ++i)
        {
            const auto phase = static_cast<float> (i) / 63.0f;
            const auto waveform = SitoDSP::evaluateShapeWaveform (phase, shapeValue);

            const auto px = curveBounds.getX() + curveBounds.getWidth() * phase;
            const auto py = centreY - amplitude * waveform;

            if (i == 0)
                shapePath.startNewSubPath (px, py);
            else
                shapePath.lineTo (px, py);
        }

        g.setColour (activeAccent.withAlpha (0.14f));
        g.strokePath (shapePath, juce::PathStrokeType (juce::jlimit (2.5f, 5.5f, radius * 0.14f),
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        g.setColour (activeAccent.withAlpha (0.95f));
        g.strokePath (shapePath, juce::PathStrokeType (juce::jlimit (1.0f, 1.7f, radius * 0.045f),
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }
}

void SitoLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);

    auto bounds = button.getLocalBounds().toFloat();
    const auto isHot = shouldDrawButtonAsHighlighted || button.isMouseOverOrDragging();
    const auto isDown = shouldDrawButtonAsDown || button.isDown();
    const auto corner = getCornerRadius();

    // Background
    g.setColour (juce::Colour (0xff1c1523).withAlpha (isHot ? 0.88f : 0.74f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), corner);

    // Outline
    g.setColour (getCardBorderColour().withAlpha (isDown ? 0.65f : (isHot ? 0.45f : 0.25f)));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner, 1.0f);

    // Subtle glow on hover
    if (isHot)
    {
        g.setColour (getAccentColour().withAlpha (0.08f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), corner);
    }
}

void SitoLookAndFeel::drawToggleButton (juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    auto bounds = button.getLocalBounds().toFloat();
    const auto isOn = button.getToggleState();
    const auto isHot = shouldDrawButtonAsHighlighted || button.isMouseOverOrDragging();
    const auto kind = button.getName();
    const auto isModeChip = kind == "chip_mode";
    const auto isModifierChip = kind == "chip_trip" || kind == "chip_dot";
    const auto isPageChip = kind.startsWith ("chip_page_");

    const auto enabledAlpha = button.isEnabled() ? 1.0f : 0.40f;
    
    // Enhanced styling for page tabs
    juce::Colour bg, outline, icon;
    float corner = (isModeChip || isModifierChip) ? 9.0f : getCornerRadius();
    
    if (isPageChip)
    {
        corner = 10.0f;
        bg = (isOn ? getAccentColour().interpolatedWith (getCardColour(), 0.68f)
                   : juce::Colour (0xff1a1a20))
                .withAlpha ((isHot ? 0.92f : (isOn ? 0.88f : 0.62f)) * enabledAlpha);
        outline = (isOn ? getAccentColour() : getCardBorderColour())
                      .withAlpha ((isOn ? 0.72f : (isHot ? 0.35f : 0.18f)) * enabledAlpha);
        icon = (isOn ? getTextPrimaryColour() : getTextSecondaryColour())
                   .withAlpha ((isOn ? 0.96f : (isHot ? 0.68f : 0.52f)) * enabledAlpha);
    }
    else
    {
        bg = ((isModeChip || isModifierChip) && isOn ? getAccentColour().interpolatedWith (getCardColour(), 0.76f)
                                 : juce::Colour (0xff1c1523))
                 .withAlpha ((isHot ? 0.88f : 0.74f) * enabledAlpha);
        outline = (isOn ? getAccentColour() : getCardBorderColour())
                      .withAlpha ((isOn ? ((isModeChip || isModifierChip) ? 0.95f : 0.72f) : (isHot ? 0.45f : 0.25f)) * enabledAlpha);
        icon = (isOn ? getTextPrimaryColour() : (isModifierChip ? getTextPrimaryColour() : getTextSecondaryColour()))
                   .withAlpha ((isOn ? 0.96f : (isModifierChip ? (isHot ? 0.78f : 0.62f) : (isHot ? 0.75f : 0.55f))) * enabledAlpha);
    }

    g.setColour (bg);
    g.fillRoundedRectangle (bounds.reduced (1.0f), corner);

    g.setColour (outline);
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner, isPageChip ? (isOn ? 1.2f : 0.9f) : (isModeChip ? 1.15f : 1.0f));
    
    // Subtle glow on active page tab
    if (isPageChip && isOn)
    {
        g.setColour (getAccentColour().withAlpha (0.12f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), corner);
    }

    // Chip buttons (mode selection)
    if (kind.startsWith ("chip_"))
    {
        const auto textArea = bounds.toNearestInt().reduced (isModifierChip ? 3 : 4, 0);
        g.setFont (juce::Font (juce::FontOptions (isModeChip ? 12.8f : 11.4f, juce::Font::bold)));
        g.setColour (icon);
        g.drawFittedText (button.getButtonText(), textArea, juce::Justification::centred, 1);
        return;
    }

    // Icon drawing for toggle buttons
    auto iconArea = bounds.reduced (6.0f);
    const auto cx = iconArea.getCentreX();
    const auto top = iconArea.getY();
    const auto bottom = iconArea.getBottom();
    const auto w = iconArea.getWidth();
    const auto h = iconArea.getHeight();

    g.setColour (icon);

    if (kind == "softclip")
    {
        // Shield icon with checkmark when enabled
        juce::Path shield;
        shield.startNewSubPath (cx, top);
        shield.lineTo (iconArea.getX() + w * 0.18f, top + h * 0.18f);
        shield.lineTo (iconArea.getX() + w * 0.18f, top + h * 0.54f);
        shield.quadraticTo (cx, bottom, iconArea.getRight() - w * 0.18f, top + h * 0.54f);
        shield.lineTo (iconArea.getRight() - w * 0.18f, top + h * 0.18f);
        shield.closeSubPath();
        g.strokePath (shield, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (isOn)
        {
            juce::Path tick;
            tick.startNewSubPath (iconArea.getX() + w * 0.36f, top + h * 0.55f);
            tick.lineTo (iconArea.getX() + w * 0.47f, top + h * 0.67f);
            tick.lineTo (iconArea.getX() + w * 0.66f, top + h * 0.42f);
            g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else if (kind == "densync")
    {
        // Metronome icon
        juce::Path metro;
        metro.startNewSubPath (iconArea.getX() + w * 0.50f, top + h * 0.10f);
        metro.lineTo (iconArea.getX() + w * 0.22f, bottom - h * 0.08f);
        metro.lineTo (iconArea.getRight() - w * 0.22f, bottom - h * 0.08f);
        metro.closeSubPath();

        g.strokePath (metro, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (iconArea.getX() + w * 0.50f, top + h * 0.22f, iconArea.getX() + w * 0.62f, top + h * 0.46f, 1.7f);

        if (isOn)
            g.fillEllipse (iconArea.getX() + w * 0.62f - 1.8f, top + h * 0.46f - 1.8f, 3.6f, 3.6f);
    }
    else if (kind == "settings")
    {
        // Gear icon
        const auto rOuter = juce::jmin (w, h) * 0.28f;
        const auto rInner = rOuter * 0.42f;
        for (int i = 0; i < 8; ++i)
        {
            const auto angle = juce::MathConstants<float>::twoPi * (static_cast<float> (i) / 8.0f);
            const auto x1 = cx + std::cos (angle) * (rOuter - 1.0f);
            const auto y1 = iconArea.getCentreY() + std::sin (angle) * (rOuter - 1.0f);
            const auto x2 = cx + std::cos (angle) * (rOuter + 2.5f);
            const auto y2 = iconArea.getCentreY() + std::sin (angle) * (rOuter + 2.5f);
            g.drawLine (x1, y1, x2, y2, 1.4f);
        }

        g.drawEllipse (cx - rOuter, iconArea.getCentreY() - rOuter, rOuter * 2.0f, rOuter * 2.0f, 1.6f);
        g.fillEllipse (cx - rInner, iconArea.getCentreY() - rInner, rInner * 2.0f, rInner * 2.0f);
    }
    else
    {
        // Default stereo/link icon
        const auto r = juce::jmin (w, h) * 0.18f;
        const auto lx = iconArea.getX() + w * 0.34f;
        const auto rx = iconArea.getX() + w * 0.66f;
        const auto cy = iconArea.getCentreY();
        g.drawEllipse (lx - r, cy - r, r * 2.0f, r * 2.0f, 1.7f);
        g.drawEllipse (rx - r, cy - r, r * 2.0f, r * 2.0f, 1.7f);
        g.drawLine (lx + r, cy, rx - r, cy, 1.7f);
    }
}

void SitoLookAndFeel::drawComboBox (juce::Graphics& g,
                                    int width,
                                    int height,
                                    bool isButtonDown,
                                    int buttonX,
                                    int buttonY,
                                    int buttonW,
                                    int buttonH,
                                    juce::ComboBox& box)
{
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));
    const auto isHot = box.isMouseOver() || isButtonDown;
    const auto corner = getSmallCornerRadius();

    // Background
    g.setColour (juce::Colour (0xff1c1523).withAlpha (isHot ? 0.88f : 0.74f));
    g.fillRoundedRectangle (bounds, corner);

    // Outline
    g.setColour (getCardBorderColour().withAlpha (isHot ? 0.45f : 0.25f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    // Arrow
    auto arrowZone = bounds.removeFromRight (static_cast<float> (height)).reduced (static_cast<float> (height) * 0.3f);
    juce::Path arrow;
    arrow.startNewSubPath (arrowZone.getX(), arrowZone.getY() + arrowZone.getHeight() * 0.35f);
    arrow.lineTo (arrowZone.getCentreX(), arrowZone.getBottom() - arrowZone.getHeight() * 0.35f);
    arrow.lineTo (arrowZone.getRight(), arrowZone.getY() + arrowZone.getHeight() * 0.35f);

    g.setColour (getTextSecondaryColour().withAlpha (isHot ? 0.75f : 0.55f));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SitoLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height));

    g.fillAll (juce::Colour (0xff1a1422));
    g.setColour (juce::Colour (0xff1a1422).withAlpha (0.98f));
    g.fillRoundedRectangle (bounds, 10.0f);
}

void SitoLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                                          int width,
                                                          int height,
                                                          const juce::PopupMenu::Options&)
{
    drawPopupMenuBackground (g, width, height);
}

void SitoLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                         const juce::Rectangle<int>& area,
                                         bool isSeparator,
                                         bool isActive,
                                         bool isHighlighted,
                                         bool isTicked,
                                         bool hasSubMenu,
                                         const juce::String& text,
                                         const juce::String& shortcutKeyText,
                                         const juce::Drawable* icon,
                                         const juce::Colour* textColourToUse)
{
    juce::ignoreUnused (icon, shortcutKeyText);

    if (isSeparator)
    {
        auto line = area.toFloat().reduced (10.0f, 0.0f);
        g.setColour (getCardBorderColour().withAlpha (0.32f));
        g.drawLine (line.getX(), line.getCentreY(), line.getRight(), line.getCentreY(), 1.0f);
        return;
    }

    auto item = area.toFloat().reduced (4.0f, 4.0f);
    const auto textColour = textColourToUse != nullptr ? *textColourToUse : getTextPrimaryColour();
    const auto fg = isActive ? textColour : getTextSecondaryColour().withAlpha (0.42f);

    if (isHighlighted && isActive)
    {
        g.setColour (getAccentColour().interpolatedWith (getCardColour(), 0.78f).withAlpha (0.95f));
        g.fillRoundedRectangle (item, getSmallCornerRadius());
    }

    auto content = item.reduced (10.0f, 0.0f);
    auto tickArea = content.removeFromLeft (16.0f);
    auto arrowArea = content.removeFromRight (14.0f);

    if (isTicked)
    {
        juce::Path tick;
        tick.startNewSubPath (tickArea.getX() + 3.0f, tickArea.getCentreY());
        tick.lineTo (tickArea.getCentreX() - 0.5f, tickArea.getBottom() - 4.0f);
        tick.lineTo (tickArea.getRight() - 2.0f, tickArea.getY() + 4.0f);
        g.setColour (getAccentGlowColour().withAlpha (isActive ? 0.95f : 0.45f));
        g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setFont (getPopupMenuFont());
    g.setColour (isHighlighted && isActive ? getTextPrimaryColour() : fg);
    g.drawFittedText (text, content.toNearestInt(), juce::Justification::centredLeft, 1);

    if (hasSubMenu)
    {
        juce::Path arrow;
        arrow.startNewSubPath (arrowArea.getX() + 4.0f, arrowArea.getY() + 4.0f);
        arrow.lineTo (arrowArea.getRight() - 3.0f, arrowArea.getCentreY());
        arrow.lineTo (arrowArea.getX() + 4.0f, arrowArea.getBottom() - 4.0f);
        g.setColour (isHighlighted && isActive ? getTextPrimaryColour().withAlpha (0.92f) : getTextSecondaryColour().withAlpha (0.70f));
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::Font SitoLookAndFeel::getPopupMenuFont()
{
    return getDefaultPopupMenuFont();
}

void SitoLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text,
                                                 bool isSeparator,
                                                 int standardMenuItemHeight,
                                                 int& idealWidth,
                                                 int& idealHeight)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);

    if (isSeparator)
        return;

    idealHeight = juce::jmax (idealHeight, 28);
    idealWidth += 10;
}

int SitoLookAndFeel::getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&)
{
    return 0;
}
