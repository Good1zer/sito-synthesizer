#include "PluginEditor.h"
#include "dsp/DSPUtils.h"

#include <cmath>

#include "BinaryData.h"

namespace
{
// Use SitoLookAndFeel color scheme throughout the editor
const auto backgroundColour = SitoLookAndFeel::getBackgroundColour();
const auto panelColour = SitoLookAndFeel::getPanelColour();
const auto panelBorderColour = SitoLookAndFeel::getPanelBorderColour();
const auto cardColour = SitoLookAndFeel::getCardColour();
const auto cardBorderColour = SitoLookAndFeel::getCardBorderColour();
const auto accentColour = SitoLookAndFeel::getAccentColour();
const auto accentGlowColour = SitoLookAndFeel::getAccentGlowColour();
const auto textPrimary = SitoLookAndFeel::getTextPrimaryColour();
const auto textSecondary = SitoLookAndFeel::getTextSecondaryColour();
const auto textMuted = SitoLookAndFeel::getTextMutedColour();

juce::Slider* findParentSlider (juce::Component* c)
{
    while (c != nullptr)
    {
        if (auto* s = dynamic_cast<juce::Slider*> (c))
            return s;

        c = c->getParentComponent();
    }

    return nullptr;
}

juce::Component* findAssistComponent (juce::Component* c)
{
    while (c != nullptr)
    {
        const auto* button = dynamic_cast<juce::Button*> (c);
        if (button != nullptr)
            return c;

        c = c->getParentComponent();
    }

    return nullptr;
}

juce::String getAssistTextForComponent (const juce::Component* c)
{
    if (c == nullptr)
        return {};

    const auto name = c->getName();
    if (name == "chip_page_sample")      return "Open sample page";
    if (name == "chip_page_modulation")  return "Open modulation page";
    if (name == "chip_page_settings")    return "Open settings page";
    if (name == "chip_lfo1")     return "Drag LFO 1 onto a bottom control to assign it";
    if (name == "chip_mode")   return "Switch Density between free Hz and host BPM sync";
    if (name == "chip_trip")   return "Enable triplet note divisions for BPM mode";
    if (name == "chip_dot")    return "Enable dotted note divisions for BPM mode";
    if (name == "voicesdrag")  return "Drag up or down to change polyphony";
    if (name == "softclip")    return "Output safety soft clip";
    if (name == "truestereo")  return "Use true stereo sample playback when available";
    return {};
}

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

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("SITO", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, textPrimary);
    addAndMakeVisible (titleLabel);

    sampleStatusLabel.setVisible (false);
    
    // Register as preset listener
    processorRef.getPresetManager().addListener (this);
    
    // Update preset name display
    currentPresetChanged (processorRef.getPresetManager().getCurrentPresetName());

    sampleHintLabel.setText ("Drop WAV / AIFF / FLAC / MP3 here",
                             juce::dontSendNotification);
    sampleHintLabel.setJustificationType (juce::Justification::centredLeft);
    sampleHintLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    sampleHintLabel.setColour (juce::Label::textColourId, textPrimary);
    sampleHintLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sampleHintLabel);

    rootKeyLabel.setText ("ROOT", juce::dontSendNotification);
    rootKeyLabel.setJustificationType (juce::Justification::centredLeft);
    rootKeyLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    rootKeyLabel.setColour (juce::Label::textColourId, textSecondary.withAlpha (0.82f));
    rootKeyLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (rootKeyLabel);

    sourceGroup.setText ("Source");
    sourceGroup.setColour (juce::GroupComponent::outlineColourId, juce::Colours::transparentBlack);
    sourceGroup.setColour (juce::GroupComponent::textColourId, textSecondary);
    addAndMakeVisible (sourceGroup);
    sourceGroup.setVisible (false);

    grainGroup.setText ("Grain");
    grainGroup.setColour (juce::GroupComponent::outlineColourId, juce::Colours::transparentBlack);
    grainGroup.setColour (juce::GroupComponent::textColourId, textSecondary);
    addAndMakeVisible (grainGroup);
    grainGroup.setVisible (false);

    outputGroup.setText ("Output");
    outputGroup.setColour (juce::GroupComponent::outlineColourId, juce::Colours::transparentBlack);
    outputGroup.setColour (juce::GroupComponent::textColourId, textSecondary);
    addAndMakeVisible (outputGroup);
    outputGroup.setVisible (false);

    configureKnob (positionSlider, positionLabel, "Position");
    configureKnob (spraySlider, sprayLabel, "Spray");
    configureKnob (grainSizeSlider, grainSizeLabel, "Size");
    configureKnob (densitySlider, densityLabel, "Density");
    configureKnob (densitySyncSlider, densitySyncLabel, "Rate"); // hidden driver
    configureKnob (densityRateSlider, densityRateLabel, "Rate");
    configureKnob (pitchSlider, pitchLabel, "Pitch");
    configureKnob (gainSlider, gainLabel, "Gain");
    configureKnob (shapeSlider, shapeLabel, "Shape");
    configureKnob (spreadSlider, spreadLabel, "Spread");
    configureKnob (lfo1RateSlider, lfo1RateLabel, "Rate");
    configureKnob (lfo1DepthSlider, lfo1DepthLabel, "Depth");
    configureKnob (lfo1ShapeSlider, lfo1ShapeLabel, "Shape");
    configureKnob (lfo1AmountSlider, lfo1AmountLabel, "Amount");
    lfo1AmountSlider.setDoubleClickReturnValue (true, 0.0);

    auto shapeTextFromValue = [] (double v) -> juce::String
    {
        const auto clamped = juce::jlimit (0.0, 100.0, v);
        if (clamped < 33.0) return "Sine > Saw";
        if (clamped < 66.0) return "Saw > Triangle";
        if (clamped < 100.0) return "Triangle > Square";
        return "Square";
    };

    shapeSlider.textFromValueFunction = shapeTextFromValue;
    lfo1ShapeSlider.textFromValueFunction = shapeTextFromValue;

    auto& params = processorRef.getParameters();
    positionAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::position, positionSlider);
    sprayAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::spray, spraySlider);
    grainSizeAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::grainSizeMs, grainSizeSlider);
    densityAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::densityHz, densitySlider);
    densitySyncAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::densitySyncDivision, densitySyncSlider);
    pitchAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::pitchSemitones, pitchSlider);
    gainAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::gain, gainSlider);
    shapeAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::shapeType, shapeSlider);
    spreadAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::spread, spreadSlider);
    lfo1RateAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::lfo1RateHz, lfo1RateSlider);
    lfo1DepthAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::lfo1Depth, lfo1DepthSlider);
    lfo1ShapeAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::lfo1Shape, lfo1ShapeSlider);

    // Density sync toggle (in Grain header)
    densitySyncButton.setName ("chip_mode");
    densitySyncButton.setButtonText ("HZ");
    densitySyncButton.setClickingTogglesState (true);
    densitySyncButton.setLookAndFeel (&lookAndFeel);
    densitySyncButton.setTooltip ("Sync Density to host tempo");
    addAndMakeVisible (densitySyncButton);
    densitySyncEnabledAttachment = std::make_unique<ButtonAttachment> (params, ParameterIDs::densitySyncEnabled, densitySyncButton);

    // Tempo-synced Density UI (Serum-like).
    // Driver (attached) slider is hidden, and a visible knob shows only enabled options.
    densitySyncSlider.setVisible (false);
    densitySyncLabel.setVisible (false);

    densityTripletButton.setName ("chip_trip");
    densityTripletButton.setButtonText ("TRIP");
    densityTripletButton.setClickingTogglesState (true);
    densityTripletButton.setLookAndFeel (&lookAndFeel);
    densityTripletButton.setTooltip ("Enable triplet rate options");
    addAndMakeVisible (densityTripletButton);
    densitySyncTripletEnabledAttachment = std::make_unique<ButtonAttachment> (params, ParameterIDs::densitySyncTripletEnabled, densityTripletButton);

    densityDottedButton.setName ("chip_dot");
    densityDottedButton.setButtonText ("DOT");
    densityDottedButton.setClickingTogglesState (true);
    densityDottedButton.setLookAndFeel (&lookAndFeel);
    densityDottedButton.setTooltip ("Enable dotted rate options");
    addAndMakeVisible (densityDottedButton);
    densitySyncDottedEnabledAttachment = std::make_unique<ButtonAttachment> (params, ParameterIDs::densitySyncDottedEnabled, densityDottedButton);

    auto codeToLabel = [] (int code) -> juce::String
    {
        static constexpr const char* bases[] { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" };
        const auto c = juce::jlimit (0, 17, code);
        if (c < 6) return bases[c];
        if (c < 12) return juce::String (bases[c - 6]) + ".";
        return juce::String (bases[c - 12]) + "T";
    };

    auto rebuildRateCodes = [this]
    {
        densityRateCodeCount = 0;
        const auto dot = densityDottedButton.getToggleState();
        const auto trip = densityTripletButton.getToggleState();

        for (int base = 0; base < 6; ++base)
        {
            if (densityRateCodeCount < static_cast<int> (densityRateCodes.size()))
                densityRateCodes[static_cast<size_t> (densityRateCodeCount++)] = base;
            if (dot && densityRateCodeCount < static_cast<int> (densityRateCodes.size()))
                densityRateCodes[static_cast<size_t> (densityRateCodeCount++)] = 6 + base;
            if (trip && densityRateCodeCount < static_cast<int> (densityRateCodes.size()))
                densityRateCodes[static_cast<size_t> (densityRateCodeCount++)] = 12 + base;
        }

        densityRateSlider.setRange (0.0, juce::jmax (0, densityRateCodeCount - 1), 1.0);
        densityRateSlider.setNumDecimalPlacesToDisplay (0);
    };

    auto codeToIndex = [this] (int code) -> int
    {
        for (int i = 0; i < densityRateCodeCount; ++i)
            if (densityRateCodes[static_cast<size_t> (i)] == code)
                return i;
        return 0;
    };

    auto ensureDriverInEnabledList = [this]
    {
        const int code = juce::jlimit (0, 17, static_cast<int> (std::round (densitySyncSlider.getValue())));
        bool ok = false;
        for (int i = 0; i < densityRateCodeCount; ++i)
            ok = ok || (densityRateCodes[static_cast<size_t> (i)] == code);

        if (ok) return;

        const int base = (code < 6) ? code : ((code < 12) ? (code - 6) : (code - 12));
        densitySyncSlider.setValue (base, juce::sendNotificationSync);
    };

    auto syncVisibleFromDriver = [this, codeToIndex]
    {
        const int code = juce::jlimit (0, 17, static_cast<int> (std::round (densitySyncSlider.getValue())));
        const int idx = codeToIndex (code);
        const juce::ScopedValueSetter<bool> guard (isUpdatingDensityRateUI, true);
        densityRateSlider.setValue (idx, juce::dontSendNotification);
    };

    densityRateSlider.textFromValueFunction = [this, codeToLabel] (double v)
    {
        const int idx = juce::jlimit (0, juce::jmax (0, densityRateCodeCount - 1), static_cast<int> (std::round (v)));
        return codeToLabel (densityRateCodes[static_cast<size_t> (idx)]);
    };

    densityRateSlider.valueFromTextFunction = [this, codeToLabel] (const juce::String& text)
    {
        const auto t = text.trim().toUpperCase();
        for (int i = 0; i < densityRateCodeCount; ++i)
        {
            if (t == codeToLabel (densityRateCodes[static_cast<size_t> (i)]).toUpperCase())
                return static_cast<double> (i);
        }

        return text.getDoubleValue();
    };

    densityRateSlider.onValueChange = [this]
    {
        if (isUpdatingDensityRateUI) return;

        const int idx = juce::jlimit (0, juce::jmax (0, densityRateCodeCount - 1),
                                      static_cast<int> (std::round (densityRateSlider.getValue())));
        const int code = densityRateCodes[static_cast<size_t> (idx)];
        densitySyncSlider.setValue (code, juce::sendNotificationSync);
    };

    densitySyncSlider.onValueChange = [this, syncVisibleFromDriver]
    {
        syncVisibleFromDriver();
    };

    densityTripletButton.onStateChange = [this, rebuildRateCodes, ensureDriverInEnabledList, syncVisibleFromDriver]
    {
        rebuildRateCodes();
        ensureDriverInEnabledList();
        syncVisibleFromDriver();
        repaint();
    };

    densityDottedButton.onStateChange = [this, rebuildRateCodes, ensureDriverInEnabledList, syncVisibleFromDriver]
    {
        rebuildRateCodes();
        ensureDriverInEnabledList();
        syncVisibleFromDriver();
        repaint();
    };

    auto updateDensityMode = [this, rebuildRateCodes, ensureDriverInEnabledList, syncVisibleFromDriver]
    {
        const auto syncOn = densitySyncButton.getToggleState();
        densitySlider.setVisible (! syncOn);
        densityLabel.setVisible (! syncOn);
        densityRateSlider.setVisible (syncOn);
        densityRateLabel.setVisible (syncOn);
        densityTripletButton.setVisible (syncOn);
        densityDottedButton.setVisible (syncOn);

        densitySyncButton.setButtonText (syncOn ? "BPM" : "HZ");

        if (syncOn)
        {
            rebuildRateCodes();
            ensureDriverInEnabledList();
            syncVisibleFromDriver();
        }

        updateModulationAmountDisplay();
        refreshModulationSliderDecorations();
        resized();
        repaint();
    };

    densitySyncButton.onStateChange = updateDensityMode;
    updateDensityMode();

    auto configurePageButton = [this] (juce::ToggleButton& button,
                                       const juce::String& name,
                                       const juce::String& text,
                                       Page page)
    {
        button.setName (name);
        button.setButtonText (text);
        button.setRadioGroupId (1001);
        button.setClickingTogglesState (true);
        button.setLookAndFeel (&lookAndFeel);
        button.onClick = [this, page]
        {
            currentPage = page;
            updatePageVisibility();
            resized();
            repaint();
        };
        addAndMakeVisible (button);
    };

    configurePageButton (samplePageButton, "chip_page_sample", "SAMPLE", Page::sample);
    configurePageButton (modulationPageButton, "chip_page_modulation", "MODULATION", Page::modulation);
    configurePageButton (settingsPageButton, "chip_page_settings", "SETTINGS", Page::settings);
    
    // Modern inline preset bar with SVG icons
    auto chevronLeft = loadSVGIcon (BinaryData::chevron_left_svg, BinaryData::chevron_left_svgSize);
    auto chevronLeftOver = loadSVGIcon (BinaryData::chevron_left_svg, BinaryData::chevron_left_svgSize);
    if (chevronLeft)
        chevronLeft->replaceColour (juce::Colours::black, textSecondary);
    if (chevronLeftOver)
        chevronLeftOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    presetPrevButton.setImages (chevronLeft.get(), chevronLeftOver.get(), chevronLeftOver.get());
    presetPrevButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    presetPrevButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    presetPrevButton.onClick = [this]
    {
        const auto presets = processorRef.getPresetManager().getAllPresets();
        const auto currentName = processorRef.getPresetManager().getCurrentPresetName();
        
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].name == currentName && i > 0)
            {
                processorRef.getPresetManager().loadPreset (presets[i - 1].file);
                break;
            }
        }
    };
    addAndMakeVisible (presetPrevButton);
    
    auto chevronRight = loadSVGIcon (BinaryData::chevron_right_svg, BinaryData::chevron_right_svgSize);
    auto chevronRightOver = loadSVGIcon (BinaryData::chevron_right_svg, BinaryData::chevron_right_svgSize);
    if (chevronRight)
        chevronRight->replaceColour (juce::Colours::black, textSecondary);
    if (chevronRightOver)
        chevronRightOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    presetNextButton.setImages (chevronRight.get(), chevronRightOver.get(), chevronRightOver.get());
    presetNextButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    presetNextButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    presetNextButton.onClick = [this]
    {
        const auto presets = processorRef.getPresetManager().getAllPresets();
        const auto currentName = processorRef.getPresetManager().getCurrentPresetName();
        
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].name == currentName && i < presets.size() - 1)
            {
                processorRef.getPresetManager().loadPreset (presets[i + 1].file);
                break;
            }
        }
    };
    addAndMakeVisible (presetNextButton);
    
    presetNameLabel.setJustificationType (juce::Justification::centred);
    presetNameLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    presetNameLabel.setColour (juce::Label::textColourId, textPrimary);
    presetNameLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    presetNameLabel.setEditable (true, true, false);
    presetNameLabel.onTextChange = [this]
    {
        const auto newName = presetNameLabel.getText();
        if (newName.isNotEmpty() && newName != processorRef.getPresetManager().getCurrentPresetName())
        {
            processorRef.getPresetManager().savePreset (newName);
        }
    };
    addAndMakeVisible (presetNameLabel);

#if SITO_ENABLE_INSPECTOR
    inspectorButton.setName ("chip_inspector");
    inspectorButton.setButtonText ("INSPECT");
    inspectorButton.setClickingTogglesState (false);
    inspectorButton.setLookAndFeel (&lookAndFeel);
    inspectorButton.setTooltip ("Toggle melatonin inspector");
    inspectorButton.onClick = [this]
    {
        toggleInspector();
    };
    addAndMakeVisible (inspectorButton);
#endif
    
    auto saveIcon = loadSVGIcon (BinaryData::save_svg, BinaryData::save_svgSize);
    auto saveIconOver = loadSVGIcon (BinaryData::save_svg, BinaryData::save_svgSize);
    if (saveIcon)
        saveIcon->replaceColour (juce::Colours::black, textSecondary);
    if (saveIconOver)
        saveIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    presetSaveButton.setImages (saveIcon.get(), saveIconOver.get(), saveIconOver.get());
    presetSaveButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    presetSaveButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    presetSaveButton.onClick = [this]
    {
        const auto currentName = processorRef.getPresetManager().getCurrentPresetName();
        if (currentName.isEmpty() || currentName.startsWith ("Untitled"))
        {
            // Save As - generate unique name
            const auto uniqueName = processorRef.getPresetManager().generateUniquePresetName ("Untitled Preset");
            processorRef.getPresetManager().savePreset (uniqueName);
        }
        else
        {
            // Save over existing
            processorRef.getPresetManager().savePreset (currentName);
        }
    };
    addAndMakeVisible (presetSaveButton);
    
    auto menuIcon = loadSVGIcon (BinaryData::more_vert_svg, BinaryData::more_vert_svgSize);
    auto menuIconOver = loadSVGIcon (BinaryData::more_vert_svg, BinaryData::more_vert_svgSize);
    if (menuIcon)
        menuIcon->replaceColour (juce::Colours::black, textSecondary);
    if (menuIconOver)
        menuIconOver->replaceColour (juce::Colours::black, accentGlowColour);
    
    presetMenuButton.setImages (menuIcon.get(), menuIconOver.get(), menuIconOver.get());
    presetMenuButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    presetMenuButton.setColour (juce::DrawableButton::backgroundOnColourId, juce::Colours::transparentBlack);
    presetMenuButton.onClick = [this]
    {
        if (! presetBrowser)
        {
            presetBrowser = std::make_unique<PresetBrowser> (processorRef.getPresetManager());
            presetBrowser->onClose = [this]
            {
                presetBrowser->setVisible (false);
            };
            addChildComponent (presetBrowser.get());
        }
        
        presetBrowser->setVisible (! presetBrowser->isVisible());
        presetBrowser->toFront (true);
        presetBrowser->setBounds (getLocalBounds().withSizeKeepingCentre (520, 600));
    };
    addAndMakeVisible (presetMenuButton);

    lfo1SourceButton.setName ("chip_lfo1");
    lfo1SourceButton.setButtonText ("LFO 1");
    lfo1SourceButton.setClickingTogglesState (true);
    lfo1SourceButton.setLookAndFeel (&lookAndFeel);
    lfo1SourceButton.setToggleState (true, juce::dontSendNotification);
    lfo1SourceButton.onClick = [this]
    {
        lfo1SourceButton.setToggleState (true, juce::dontSendNotification);
        repaint();
    };
    addAndMakeVisible (lfo1SourceButton);

    softClipButton.setName ("softclip");
    softClipButton.setButtonText ({});
    softClipButton.setClickingTogglesState (true);
    softClipButton.setLookAndFeel (&lookAndFeel);
    softClipButton.setTooltip ("Output safety (soft clip)");
    addAndMakeVisible (softClipButton);
    softClipAttachment = std::make_unique<ButtonAttachment> (params, ParameterIDs::softClipEnabled, softClipButton);

    trueStereoButton.setName ("truestereo");
    trueStereoButton.setButtonText ({});
    trueStereoButton.setClickingTogglesState (true);
    trueStereoButton.setLookAndFeel (&lookAndFeel);
    trueStereoButton.setTooltip ("True-stereo sample playback");
    addAndMakeVisible (trueStereoButton);
    trueStereoAttachment = std::make_unique<ButtonAttachment> (params, ParameterIDs::trueStereoEnabled, trueStereoButton);

    interpolationQualityCombo.addItem ("Linear", 1);
    interpolationQualityCombo.addItem ("Cubic (High Quality)", 2);
    interpolationQualityCombo.setTooltip ("Sample interpolation quality");
    interpolationQualityCombo.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (interpolationQualityCombo);
    interpolationQualityAttachment = std::make_unique<ComboBoxAttachment> (params, ParameterIDs::interpolationQuality, interpolationQualityCombo);

    // Root key combo - just note names without octaves
    const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
    {
        rootKeyCombo.addItem (noteNames[i], i + 1);
    }
    rootKeyCombo.setJustificationType (juce::Justification::centredLeft);
    rootKeyCombo.setColour (juce::ComboBox::textColourId, textPrimary);
    rootKeyCombo.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    rootKeyCombo.setColour (juce::ComboBox::arrowColourId, textSecondary);
    rootKeyCombo.setTooltip ("Root key of the sample");
    rootKeyCombo.setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (rootKeyCombo);
    rootKeyAttachment = std::make_unique<ComboBoxAttachment> (params, ParameterIDs::rootKey, rootKeyCombo);

    configureKnob (maxVoicesSlider, maxVoicesLabel, "Voices");
    maxVoicesSlider.setNumDecimalPlacesToDisplay (0);
    maxVoicesAttachment = std::make_unique<SliderAttachment> (params, ParameterIDs::maxVoices, maxVoicesSlider);
    voicesValueLabel.setName ("voicesdrag");
    voicesValueLabel.setJustificationType (juce::Justification::centred);
    voicesValueLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    voicesValueLabel.setColour (juce::Label::textColourId, juce::Colours::transparentBlack);
    voicesValueLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    voicesValueLabel.setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    voicesValueLabel.setInterceptsMouseClicks (true, false);
    voicesValueLabel.setEditable (false, false, false);
    addAndMakeVisible (voicesValueLabel);

    maxVoicesSlider.onValueChange = [this]
    {
        updateVoicesValueDisplay();
        repaint();
    };
    updateVoicesValueDisplay();

    // The waveform overlays are painted by the editor (not by the individual knobs),
    // so we must repaint the whole editor when these values change.
    auto repaintEditor = [this] { repaint(); };
    positionSlider.onValueChange = repaintEditor;
    spraySlider.onValueChange = repaintEditor;

    // Also repaint while hovering/dragging any knob, so the hover-value overlay stays fresh.
    auto wrapHoverRepaint = [this] (juce::Slider& s)
    {
        const auto prev = s.onValueChange;
        s.onValueChange = [this, &s, prev]
        {
            if (prev) prev();
            if (hoveredSlider == &s || s.isMouseOverOrDragging())
                repaint();
        };
    };

    wrapHoverRepaint (positionSlider);
    wrapHoverRepaint (spraySlider);
    wrapHoverRepaint (grainSizeSlider);
    wrapHoverRepaint (densitySlider);
    wrapHoverRepaint (densitySyncSlider);
    wrapHoverRepaint (densityRateSlider);
    wrapHoverRepaint (shapeSlider);
    wrapHoverRepaint (pitchSlider);
    wrapHoverRepaint (spreadSlider);
    wrapHoverRepaint (gainSlider);
    wrapHoverRepaint (lfo1RateSlider);
    wrapHoverRepaint (lfo1DepthSlider);
    wrapHoverRepaint (lfo1ShapeSlider);
    wrapHoverRepaint (lfo1AmountSlider);

    lfo1AmountSlider.setRange (-1.0, 1.0, 0.001);
    lfo1AmountSlider.setNumDecimalPlacesToDisplay (2);
    lfo1AmountSlider.textFromValueFunction = [] (double v)
    {
        const auto percent = static_cast<int> (std::round (juce::jlimit (-1.0, 1.0, v) * 100.0));
        return juce::String (percent) + "%";
    };
    lfo1AmountSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto percent = juce::jlimit (-100.0, 100.0, text.upToFirstOccurrenceOf ("%", false, false).getDoubleValue());
        return percent / 100.0;
    };
    lfo1AmountSlider.onValueChange = [this]
    {
        if (selectedModulationTarget < 0)
            return;

        if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (selectedModulationTarget))
        {
            processorRef.setLfo1AssignmentAmount (parameterID, static_cast<float> (lfo1AmountSlider.getValue()));
            refreshModulationSliderDecorations();
            repaint();
        }
    };

    selectedModulationTarget = AudioPluginAudioProcessor::getModulationTargetIndexForParameter (ParameterIDs::position);
    updateModulationAmountDisplay();
    refreshModulationSliderDecorations();

    // Receive mouse-move events from child components (sliders), so we can show hover values.
    addMouseListener (this, true);
    setWantsKeyboardFocus (true);

    // Lightweight UI animation for grain playheads and overlays.
    startTimerHz (20);

    updateSampleStatus();
    updatePageVisibility();
    setSize (920, 720);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    processorRef.getPresetManager().removeListener (this);
    
    positionSlider.setLookAndFeel (nullptr);
    spraySlider.setLookAndFeel (nullptr);
    grainSizeSlider.setLookAndFeel (nullptr);
    densitySlider.setLookAndFeel (nullptr);
    densitySyncSlider.setLookAndFeel (nullptr);
    densityRateSlider.setLookAndFeel (nullptr);
    pitchSlider.setLookAndFeel (nullptr);
    gainSlider.setLookAndFeel (nullptr);
    shapeSlider.setLookAndFeel (nullptr);
    spreadSlider.setLookAndFeel (nullptr);
    lfo1RateSlider.setLookAndFeel (nullptr);
    lfo1DepthSlider.setLookAndFeel (nullptr);
    lfo1ShapeSlider.setLookAndFeel (nullptr);
    lfo1AmountSlider.setLookAndFeel (nullptr);
    maxVoicesSlider.setLookAndFeel (nullptr);
    lfo1SourceButton.setLookAndFeel (nullptr);
    softClipButton.setLookAndFeel (nullptr);
    trueStereoButton.setLookAndFeel (nullptr);
    samplePageButton.setLookAndFeel (nullptr);
    modulationPageButton.setLookAndFeel (nullptr);
    settingsPageButton.setLookAndFeel (nullptr);
#if SITO_ENABLE_INSPECTOR
    inspectorButton.setLookAndFeel (nullptr);
#endif
    densitySyncButton.setLookAndFeel (nullptr);
    densityTripletButton.setLookAndFeel (nullptr);
    densityDottedButton.setLookAndFeel (nullptr);
    interpolationQualityCombo.setLookAndFeel (nullptr);
    rootKeyCombo.setLookAndFeel (nullptr);
    presetPrevButton.setLookAndFeel (nullptr);
    presetNextButton.setLookAndFeel (nullptr);
    presetSaveButton.setLookAndFeel (nullptr);
    presetMenuButton.setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    const auto sampleGeneration = processorRef.getLoadedSampleGeneration();
    if (sampleGeneration != lastSeenSampleGeneration)
    {
        lastSeenSampleGeneration = sampleGeneration;
        updateSampleStatus();
        updatePageVisibility();
        resized();
    }

    const auto rateHz = juce::jlimit (0.01f, 20.0f, static_cast<float> (lfo1RateSlider.getValue()));
    lfoPreviewPhase += rateHz / 20.0f;
    lfoPreviewPhase -= std::floor (lfoPreviewPhase);
    
    // Waveform pulse animation: 1.5 Hz cycle (0.667 seconds per cycle)
    waveformPulsePhase += 1.5f / 20.0f;
    waveformPulsePhase -= std::floor (waveformPulsePhase);
    
    // Update knob animation states (hover transitions and arc animations)
    const auto easeOutFactor = 0.2f; // 150ms ease-out at 20fps
    for (auto& [slider, state] : knobAnimationStates)
    {
        if (slider == nullptr)
            continue;
        
        const auto targetHover = slider->isMouseOverOrDragging() ? 1.0f : 0.0f;
        state.hoverAlpha += (targetHover - state.hoverAlpha) * easeOutFactor;
        
        // Arc animation: increment phase for smooth arc drawing
        state.arcAnimationPhase += 0.05f;
        if (state.arcAnimationPhase > 1.0f)
            state.arcAnimationPhase -= 1.0f;
    }
    
    // Update assignment workflow feedback animations
    if (isDraggingModulationSource)
    {
        // Fade in drag line and target feedback
        assignmentWorkflowState.dragLineAlpha += (1.0f - assignmentWorkflowState.dragLineAlpha) * easeOutFactor;
        assignmentWorkflowState.targetPulsePhase += 0.08f;
        if (assignmentWorkflowState.targetPulsePhase > 1.0f)
            assignmentWorkflowState.targetPulsePhase -= 1.0f;
        
        // Scale up hovered target handle
        if (hoveredModulationTarget >= 0)
        {
            auto& handleState = modulationHandleStates[hoveredModulationTarget];
            handleState.scaleAmount += (1.3f - handleState.scaleAmount) * easeOutFactor;
            handleState.glowAlpha += (0.8f - handleState.glowAlpha) * easeOutFactor;
        }
    }
    else
    {
        // Fade out drag line and reset feedback
        assignmentWorkflowState.dragLineAlpha += (0.0f - assignmentWorkflowState.dragLineAlpha) * easeOutFactor;
        
        // Reset all handle scales
        for (auto& [index, handleState] : modulationHandleStates)
        {
            handleState.scaleAmount += (1.0f - handleState.scaleAmount) * easeOutFactor;
            handleState.glowAlpha += (0.0f - handleState.glowAlpha) * easeOutFactor;
        }
    }
    
    // Page transition animation (150ms ease-out cubic)
    if (pageTransitionState.isTransitioning)
    {
        pageTransitionState.fadeAlpha += 0.133f; // 150ms at 20fps
        if (pageTransitionState.fadeAlpha >= 1.0f)
        {
            pageTransitionState.fadeAlpha = 1.0f;
            pageTransitionState.isTransitioning = false;
        }
        repaint();
    }
    
    refreshModulationSliderDecorations();

    // Keep preset browser on top if visible
    if (presetBrowser && presetBrowser->isVisible())
        presetBrowser->toFront (true);

    if (currentPage == Page::sample)
        repaint (sampleDropZone);
    else
        repaint (pageContentZone);

    if (currentPage != Page::settings)
        repaint (controlsZone);
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto outerBounds = getLocalBounds().toFloat().reduced (18.0f);

    g.setColour (panelColour);
    g.fillRoundedRectangle (outerBounds, 26.0f);

    // Material-ish minimalism: no extra borders or background gradients.

    if (currentPage == Page::sample)
    {
    const auto sampleZone = sampleDropZone.toFloat();
    
    // Premium waveform display with layered depth
    g.setColour ((isDraggingSample ? accentColour.withAlpha (0.18f) : cardColour.withAlpha (0.98f)));
    g.fillRoundedRectangle (sampleZone, 22.0f);
    
    // Inner waveform panel for visual separation
    auto innerPanel = sampleZone.reduced (2.0f);
    g.setColour (juce::Colour (0xff14141a).withAlpha (0.72f));
    g.fillRoundedRectangle (innerPanel, 20.0f);

    // Refined outline with better definition
    g.setColour ((isDraggingSample ? accentGlowColour : cardBorderColour).withAlpha (isDraggingSample ? 0.88f : 0.24f));
    g.drawRoundedRectangle (sampleZone, 22.0f, isDraggingSample ? 1.6f : 0.8f);

    // Waveform area
    auto waveformArea = getWaveformArea();
    auto waveformMidY = waveformArea.getCentreY();
    const auto focusPosition = hoveredSlider == &positionSlider || isDraggingWaveformPosition;
    const auto focusSpray = hoveredSlider == &spraySlider || isDraggingWaveformSpray;
    const auto focusSize = hoveredSlider == &grainSizeSlider;
    const auto focusDensity = hoveredSlider == &densitySlider
        || hoveredSlider == &densitySyncSlider
        || hoveredSlider == &densityRateSlider;
    const auto focusSpread = hoveredSlider == &spreadSlider;

    juce::Path waveformPath;
    const auto preview = processorRef.getWaveformPreview (64);

    if (! preview.empty())
    {
        waveformPath.startNewSubPath (waveformArea.getX(), waveformMidY);

        for (size_t i = 0; i < preview.size(); ++i)
        {
            const auto x = waveformArea.getX() + waveformArea.getWidth() * (static_cast<float> (i) / static_cast<float> (juce::jmax<size_t> (1, preview.size() - 1)));
            const auto peak = juce::jlimit (0.0f, 1.0f, preview[i]);
            const auto y = waveformMidY - peak * waveformArea.getHeight() * 0.34f;
            waveformPath.lineTo (x, y);
        }

        for (size_t i = preview.size(); i-- > 0;)
        {
            const auto x = waveformArea.getX() + waveformArea.getWidth() * (static_cast<float> (i) / static_cast<float> (juce::jmax<size_t> (1, preview.size() - 1)));
            const auto peak = juce::jlimit (0.0f, 1.0f, preview[i]);
            const auto y = waveformMidY + peak * waveformArea.getHeight() * 0.34f;
            waveformPath.lineTo (x, y);
        }

        waveformPath.closeSubPath();

        g.setColour (accentColour.withAlpha (0.16f));
        g.fillPath (waveformPath);

        juce::Path centreLine;
        centreLine.startNewSubPath (waveformArea.getX(), waveformMidY);

        for (size_t i = 0; i < preview.size(); ++i)
        {
            const auto x = waveformArea.getX() + waveformArea.getWidth() * (static_cast<float> (i) / static_cast<float> (juce::jmax<size_t> (1, preview.size() - 1)));
            const auto peak = juce::jlimit (0.0f, 1.0f, preview[i]);
            const auto y = waveformMidY - peak * waveformArea.getHeight() * 0.34f;
            centreLine.lineTo (x, y);
        }

        // Outer glow pass with pulse animation
        const auto pulseAlpha = 0.4f + 0.15f * std::sin (waveformPulsePhase * juce::MathConstants<float>::twoPi);
        g.setColour (accentGlowColour.withAlpha (pulseAlpha));
        g.strokePath (centreLine, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Main waveform stroke with increased thickness
        g.setColour (accentGlowColour.withAlpha (0.72f));
        g.strokePath (centreLine, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Position + Spray overlay (very important UX for granular)
    {
        const auto position = static_cast<float> (positionSlider.getValue());
        const auto spray = static_cast<float> (spraySlider.getValue());
        const auto x = waveformArea.getX() + waveformArea.getWidth() * position;
        const auto halfWidth = juce::jmax (2.0f, waveformArea.getWidth() * spray * 0.5f);
        const auto band = juce::Rectangle<float> (x - halfWidth,
                                                  waveformArea.getY(),
                                                  halfWidth * 2.0f,
                                                  waveformArea.getHeight())
                              .getIntersection (waveformArea);

        if (spray > 0.0001f)
        {
            g.setColour (accentColour.withAlpha ((focusSpray || focusPosition) ? 0.12f : 0.07f));
            g.fillRoundedRectangle (band, 10.0f);

            g.setColour (accentGlowColour.withAlpha ((focusSpray || focusPosition) ? 0.70f : 0.45f));
            g.drawLine (band.getX(), waveformArea.getY(), band.getX(), waveformArea.getBottom(),
                        (focusSpray || focusPosition) ? 1.4f : 1.0f);
            g.drawLine (band.getRight(), waveformArea.getY(), band.getRight(), waveformArea.getBottom(),
                        (focusSpray || focusPosition) ? 1.4f : 1.0f);
        }

        g.setColour (accentGlowColour.withAlpha (focusPosition ? 0.95f : 0.75f));
        g.drawLine (x, waveformArea.getY(), x, waveformArea.getBottom(), focusPosition ? 2.0f : 1.4f);
    }

    // Grain size overlay (duration mapped into a window width in the source sample).
    if (processorRef.hasLoadedSample())
    {
        const auto lengthSeconds = processorRef.getLoadedSampleLengthSeconds();
        if (lengthSeconds > 0.001)
        {
            const auto position = static_cast<float> (positionSlider.getValue());
            const auto grainSizeSeconds = static_cast<float> (grainSizeSlider.getValue() * 0.001);
            const auto widthNormalized = juce::jlimit (0.002f, 0.95f, grainSizeSeconds / static_cast<float> (lengthSeconds));

            const auto centerX = waveformArea.getX() + waveformArea.getWidth() * position;
            const auto halfWidth = juce::jmax (2.0f, waveformArea.getWidth() * widthNormalized * 0.5f);
            const auto win = juce::Rectangle<float> (centerX - halfWidth,
                                                     waveformArea.getY(),
                                                     halfWidth * 2.0f,
                                                     waveformArea.getHeight())
                                 .getIntersection (waveformArea);

            g.setColour (accentColour.withAlpha (focusSize ? 0.085f : 0.045f));
            g.fillRoundedRectangle (win, 12.0f);
            g.setColour (accentGlowColour.withAlpha (focusSize ? 0.38f : 0.18f));
            g.drawRoundedRectangle (win, 12.0f, focusSize ? 1.3f : 1.0f);
        }
    }

    // Density overlay (sparks proportional to grains/sec around the play area).
    {
        const auto position = static_cast<float> (positionSlider.getValue());
        const auto spray = static_cast<float> (spraySlider.getValue());

        float densityHz = static_cast<float> (densitySlider.getValue());

        if (densitySyncButton.getToggleState())
        {
            double bpm = 120.0;
            if (auto* playHead = processorRef.getPlayHead())
                if (auto pos = playHead->getPosition())
                    if (auto hostBpm = pos->getBpm())
                        bpm = *hostBpm;

            const int code = juce::jlimit (0, 17, static_cast<int> (std::round (densitySyncSlider.getValue())));
            const double baseBeats[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
            const double beats = (code < 6)
                ? baseBeats[code]
                : (code < 12 ? baseBeats[code - 6] * 1.5 : baseBeats[code - 12] * (2.0 / 3.0));
            densityHz = static_cast<float> ((bpm / 60.0) * (1.0 / juce::jmax (1.0e-6, beats)));
        }

        const auto numDots = juce::jlimit (4, 36, static_cast<int> (std::round (densityHz)));

        const auto centerX = waveformArea.getX() + waveformArea.getWidth() * position;
        const auto halfWidth = juce::jmax (6.0f, waveformArea.getWidth() * spray * 0.5f);
        const auto y = waveformArea.getY() + 12.0f;

        g.setColour (accentGlowColour.withAlpha (focusDensity ? 0.34f : 0.20f));
        for (int i = 0; i < numDots; ++i)
        {
            const auto t = (static_cast<float> (i) + 0.5f) / static_cast<float> (numDots);
            const auto x = centerX + (t - 0.5f) * halfWidth * 2.0f;
            if (x < waveformArea.getX() || x > waveformArea.getRight())
                continue;

            const auto r = juce::jmap (t, focusDensity ? 1.9f : 1.2f, focusDensity ? 3.0f : 2.2f);
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }
    }

    // Active grain playheads (snapshot from engine).
    if (processorRef.hasLoadedSample())
    {
        std::array<GranularEngine::GrainVisual, GranularEngine::maxGrainVisuals> visuals {};
        const auto n = processorRef.copyGrainVisuals (visuals.data(), static_cast<int> (visuals.size()));

        for (int i = 0; i < n; ++i)
        {
            const auto& v = visuals[static_cast<size_t> (i)];
            const auto x = waveformArea.getX() + waveformArea.getWidth() * v.positionNormalized;
            const auto alpha = juce::jlimit (0.0f, 1.0f, 0.10f + v.strength * 0.55f);
            const auto thickness = 1.0f + v.strength * 1.5f;

            const auto panTint = (v.pan >= 0.0f)
                ? accentColour.interpolatedWith (juce::Colour (0xff69bfe0), juce::jlimit (0.0f, 1.0f, v.pan))
                : accentColour.interpolatedWith (juce::Colour (0xffff7aa8), juce::jlimit (0.0f, 1.0f, -v.pan));

            g.setColour (panTint.withAlpha (focusSpread ? juce::jlimit (0.0f, 1.0f, alpha + 0.18f) : alpha));
            g.drawLine (x, waveformArea.getY(), x, waveformArea.getBottom(), focusSpread ? thickness + 0.8f : thickness);
        }
    }

    // Bottom edit bar
    if (processorRef.hasLoadedSample())
    {
        const auto infoBarBg = sampleEditBarZone.toFloat();

        g.setColour (juce::Colour (0xff1a1a22).withAlpha (0.65f));
        g.fillRoundedRectangle (infoBarBg, 8.0f);
        g.setColour (cardBorderColour.withAlpha (0.12f));
        g.drawRoundedRectangle (infoBarBg, 8.0f, 0.6f);
    }

    // Root key and length info in the bottom-right corner of the sample canvas.
    if (processorRef.hasLoadedSample())
    {
        const auto lengthSeconds = processorRef.getLoadedSampleLengthSeconds();
        const auto lengthText = juce::String (lengthSeconds, 2) + " s";

        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
        g.setColour (textSecondary.withAlpha (0.75f));
        g.drawFittedText (lengthText, sampleLengthZone, juce::Justification::centredRight, 1);
    }

    }

    if (currentPage != Page::settings)
    {
        // Controls strip background (single, calm container) with refined styling
        {
            const auto controls = controlsZone.toFloat();
            g.setColour (cardColour.withAlpha (0.82f));
            g.fillRoundedRectangle (controls, 22.0f);
            
            // Subtle border for definition
            g.setColour (cardBorderColour.withAlpha (0.18f));
            g.drawRoundedRectangle (controls, 22.0f, 0.9f);
        }

        // Section titles (layout computed in resized()).
        {
            const auto controls = controlsZone.toFloat();
            const auto y1 = static_cast<float> (sourceHeaderZone.getBottom() + 6);
            const auto y2 = controls.getBottom() - 12.0f;
            const auto xA = static_cast<float> (grainHeaderZone.getX() - 10);
            const auto xB = static_cast<float> (outputHeaderZone.getX() - 10);

            g.setFont (juce::Font (juce::FontOptions (12.8f, juce::Font::bold)));
            g.setColour (textSecondary.withAlpha (0.68f));
            g.drawFittedText ("Source", sourceHeaderZone, juce::Justification::centredLeft, 1);
            g.drawFittedText ("Grain", grainHeaderZone, juce::Justification::centredLeft, 1);
            g.drawFittedText ("Output", outputHeaderZone, juce::Justification::centredLeft, 1);

            g.setColour (cardBorderColour.withAlpha (0.14f));
            g.drawLine (xA, y1, xA, y2, 0.9f);
            g.drawLine (xB, y1, xB, y2, 0.9f);
        }
    }

    if (currentPage == Page::modulation)
    {
        const auto panel = pageContentZone.toFloat();
        
        // Premium transition: ease-out cubic + slide
        const auto rawAlpha = pageTransitionState.isTransitioning ? pageTransitionState.fadeAlpha : 1.0f;
        const auto easeOutCubic = 1.0f - std::pow(1.0f - rawAlpha, 3.0f);
        const auto slideOffset = (1.0f - easeOutCubic) * 12.0f;
        
        auto transitionedPanel = panel.translated(slideOffset, 0.0f);
        
        g.setColour (cardColour.withAlpha (0.96f * easeOutCubic));
        g.fillRoundedRectangle (transitionedPanel, 22.0f);
        g.setColour (cardBorderColour.withAlpha (0.22f * easeOutCubic));
        g.drawRoundedRectangle (transitionedPanel, 22.0f, 0.9f);

        auto content = panel.reduced (32.0f, 28.0f);
        auto heading = content.removeFromTop (26.0f);
        content.removeFromTop (26.0f);

        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.setColour (textPrimary.withAlpha (0.94f));
        g.drawFittedText ("Modulation", heading.toNearestInt(), juce::Justification::centredLeft, 1);

        auto left = content.removeFromLeft (juce::jmin (320.0f, content.getWidth() * 0.46f));
        auto right = content.reduced (16.0f, 0.0f);

        auto shapePanel = left.removeFromTop (juce::jmin (220.0f, left.getHeight()));
        g.setColour (juce::Colour (0xff1b1423).withAlpha (0.92f));
        g.fillRoundedRectangle (shapePanel, 18.0f);
        g.setColour (cardBorderColour.withAlpha (0.22f));
        g.drawRoundedRectangle (shapePanel, 18.0f, 0.9f);

        auto previewArea = shapePanel.reduced (24.0f, 24.0f);
        auto previewTitle = previewArea.removeFromTop (18.0f);
        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.setColour (textSecondary.withAlpha (0.74f));
        g.drawFittedText ("Shape Preview", previewTitle.toNearestInt(), juce::Justification::centredLeft, 1);

        auto lfoArea = previewArea.reduced (0.0f, 8.0f);
        juce::Path lfoPath;
        const auto lfoMidY = lfoArea.getCentreY();
        const auto lfoAmp = lfoArea.getHeight() * 0.30f;
        const auto shapeValue = static_cast<float> (lfo1ShapeSlider.getValue());
        for (int i = 0; i < 96; ++i)
        {
            const auto t = static_cast<float> (i) / 95.0f;
            const auto x = lfoArea.getX() + lfoArea.getWidth() * t;
            const auto previewPhase = std::fmod (lfoPreviewPhase + t, 1.0f);
            const auto y = lfoMidY - evaluateLfoPreview (previewPhase, shapeValue) * lfoAmp;
            if (i == 0)
                lfoPath.startNewSubPath (x, y);
            else
                lfoPath.lineTo (x, y);
        }

        // Center line
        g.setColour (accentColour.withAlpha (0.10f));
        g.drawLine (lfoArea.getX(), lfoMidY, lfoArea.getRight(), lfoMidY, 1.0f);
        
        // Glow behind waveform
        g.setColour (accentGlowColour.withAlpha (0.15f));
        g.strokePath (lfoPath, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        // Main waveform
        g.setColour (accentGlowColour.withAlpha (0.90f));
        g.strokePath (lfoPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Animated dot at current value (at the start of the waveform)
        const auto currentValue = evaluateLfoPreview (lfoPreviewPhase, shapeValue);
        const auto dotX = lfoArea.getX();
        const auto dotY = lfoMidY - currentValue * lfoAmp;
        
        // Dot glow
        g.setColour (accentGlowColour.withAlpha (0.35f));
        g.fillEllipse (dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f);
        
        // Dot core
        g.setColour (accentGlowColour);
        g.fillEllipse (dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
        
        // Vertical indicator line
        g.setColour (accentGlowColour.withAlpha (0.22f));
        g.drawLine (dotX, lfoArea.getY(), dotX, lfoArea.getBottom(), 1.0f);

    }
    else if (currentPage == Page::settings)
    {
        const auto panel = pageContentZone.toFloat();
        
        // Premium transition: ease-out cubic + slide
        const auto rawAlpha = pageTransitionState.isTransitioning ? pageTransitionState.fadeAlpha : 1.0f;
        const auto easeOutCubic = 1.0f - std::pow(1.0f - rawAlpha, 3.0f);
        const auto slideOffset = (1.0f - easeOutCubic) * 12.0f;
        
        auto transitionedPanel = panel.translated(slideOffset, 0.0f);
        
        g.setColour (cardColour.withAlpha (0.96f * easeOutCubic));
        g.fillRoundedRectangle (transitionedPanel, 22.0f);
        g.setColour (cardBorderColour.withAlpha (0.22f * easeOutCubic));
        g.drawRoundedRectangle (transitionedPanel, 22.0f, 0.9f);

        auto content = panel.reduced (32.0f, 28.0f);
        auto heading = content.removeFromTop (26.0f);
        auto subtitle = content.removeFromTop (18.0f);
        content.removeFromTop (20.0f);

        g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        g.setColour (textPrimary.withAlpha (0.94f));
        g.drawFittedText ("Settings", heading.toNearestInt(), juce::Justification::centredLeft, 1);

        g.setFont (juce::Font (juce::FontOptions (12.6f, juce::Font::plain)));
        g.setColour (textSecondary.withAlpha (0.62f));
        g.drawFittedText ("Engine and utility controls", subtitle.toNearestInt(), juce::Justification::centredLeft, 1);

        auto drawSettingText = [&] (juce::Rectangle<float> row,
                                    const juce::String& title,
                                    const juce::String& description)
        {
            auto textArea = row;
            textArea.removeFromRight (140.0f);
            auto titleArea = textArea.removeFromTop (18.0f);
            auto descriptionArea = textArea.removeFromTop (14.0f);

            g.setFont (juce::Font (juce::FontOptions (13.2f, juce::Font::bold)));
            g.setColour (textPrimary.withAlpha (0.88f));
            g.drawFittedText (title, titleArea.toNearestInt(), juce::Justification::centredLeft, 1);

            g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::plain)));
            g.setColour (textSecondary.withAlpha (0.58f));
            g.drawFittedText (description, descriptionArea.toNearestInt(), juce::Justification::centredLeft, 1);
        };

        auto row1 = content.removeFromTop (54.0f);
        auto row2 = content.removeFromTop (54.0f);
        auto row3 = content.removeFromTop (54.0f);
        auto row4 = content.removeFromTop (54.0f);

        drawSettingText (row1, "True Stereo", "Read left and right channels independently when possible");
        drawSettingText (row2, "Soft Clip", "Catch output peaks with gentle saturation");
        drawSettingText (row3, "Voices", "Polyphony budget and voice stealing behavior");
        drawSettingText (row4, "Interpolation", "Sample playback quality (Linear is fast, Cubic is smoother)");

        g.setColour (cardBorderColour.withAlpha (0.14f));
        g.drawLine (row1.getX(), row1.getBottom(), row1.getRight(), row1.getBottom(), 1.0f);
        g.drawLine (row2.getX(), row2.getBottom(), row2.getRight(), row2.getBottom(), 1.0f);
        g.drawLine (row3.getX(), row3.getBottom(), row3.getRight(), row3.getBottom(), 1.0f);
        g.drawLine (row4.getX(), row4.getBottom(), row4.getRight(), row4.getBottom(), 1.0f);

        const auto voicesField = voicesValueLabel.getBounds().toFloat();
        const auto voicesHot = hoveredAssistComponent == &voicesValueLabel || isDraggingVoicesValue;
        g.setColour (juce::Colour (0xff221a2b).withAlpha (voicesHot ? 0.92f : 0.82f));
        g.fillRoundedRectangle (voicesField, 7.0f);
        g.setColour ((voicesHot ? accentColour : cardBorderColour).withAlpha (voicesHot ? 0.32f : 0.14f));
        g.drawRoundedRectangle (voicesField, 7.0f, 1.0f);

        auto valueArea = voicesField.reduced (12.0f, 0.0f);
        auto glyphArea = valueArea.removeFromLeft (10.0f);
        auto arrowsArea = valueArea.removeFromRight (10.0f);

        g.setColour (textSecondary.withAlpha (voicesHot ? 0.42f : 0.22f));
        for (int i = 0; i < 3; ++i)
        {
            const auto y = glyphArea.getCentreY() - 5.0f + static_cast<float> (i) * 4.0f;
            g.fillEllipse (glyphArea.getCentreX() - 0.85f, y, 1.7f, 1.7f);
        }

        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.setColour (voicesHot ? accentGlowColour.withAlpha (0.96f) : textPrimary.withAlpha (0.90f));
        g.drawFittedText (juce::String (static_cast<int> (std::round (maxVoicesSlider.getValue()))),
                          valueArea.toNearestInt(),
                          juce::Justification::centred,
                          1);

        juce::Path upArrow;
        upArrow.startNewSubPath (arrowsArea.getCentreX(), arrowsArea.getY() + 5.0f);
        upArrow.lineTo (arrowsArea.getX() + 3.5f, arrowsArea.getY() + 8.5f);
        upArrow.lineTo (arrowsArea.getRight() - 3.5f, arrowsArea.getY() + 8.5f);
        upArrow.closeSubPath();

        juce::Path downArrow;
        downArrow.startNewSubPath (arrowsArea.getCentreX(), arrowsArea.getBottom() - 5.0f);
        downArrow.lineTo (arrowsArea.getX() + 3.5f, arrowsArea.getBottom() - 8.5f);
        downArrow.lineTo (arrowsArea.getRight() - 3.5f, arrowsArea.getBottom() - 8.5f);
        downArrow.closeSubPath();

        g.setColour (textSecondary.withAlpha (voicesHot ? 0.42f : 0.22f));
        g.fillPath (upArrow);
        g.fillPath (downArrow);
    }
}

void AudioPluginAudioProcessorEditor::resized()
{
    constexpr int outerMargin = 28;
    constexpr int titleH = 32;
    constexpr int presetBarH = 26;
    constexpr int tabsH = 26;
    constexpr int controlsH = 206;
    
    auto bounds = getLocalBounds().reduced (outerMargin, 22);

    // Top → down flow
    titleLabel.setBounds (bounds.removeFromTop (titleH));
    bounds.removeFromTop (4);

    // Preset bar - symmetric layout
    auto presetBar = bounds.removeFromTop (presetBarH);
    bounds.removeFromTop (8);
    {
        constexpr int presetButtonW = 32;
        constexpr int presetSaveW = 50;
        constexpr int presetMenuW = 32;
        constexpr int inspectorW = 92;
        constexpr int presetGap = 6;
        
        presetPrevButton.setBounds (presetBar.removeFromLeft (presetButtonW));
        presetBar.removeFromLeft (presetGap);
        presetNextButton.setBounds (presetBar.removeFromLeft (presetButtonW));
        presetBar.removeFromLeft (presetGap);
        
        presetMenuButton.setBounds (presetBar.removeFromRight (presetMenuW));
        presetBar.removeFromRight (presetGap);
        presetSaveButton.setBounds (presetBar.removeFromRight (presetSaveW));
        presetBar.removeFromRight (presetGap);
#if SITO_ENABLE_INSPECTOR
        inspectorButton.setBounds (presetBar.removeFromRight (inspectorW));
        presetBar.removeFromRight (presetGap);
#endif
        
        presetNameLabel.setBounds (presetBar);
    }

    // Tabs - centered cluster
    topTabsZone = bounds.removeFromTop (tabsH);
    bounds.removeFromTop (12);
    {
        auto tabsRow = topTabsZone;
        tabsRow.removeFromLeft (tabsRow.getWidth() / 4);
        tabsRow.removeFromRight (tabsRow.getWidth() / 3);

        constexpr int tabGap = 12;
        const auto tabW = (tabsRow.getWidth() - tabGap * 2) / 3;

        samplePageButton.setBounds (tabsRow.removeFromLeft (tabW));
        tabsRow.removeFromLeft (tabGap);
        modulationPageButton.setBounds (tabsRow.removeFromLeft (tabW));
        tabsRow.removeFromLeft (tabGap);
        settingsPageButton.setBounds (tabsRow.removeFromLeft (tabW));
    }

    // Settings page
    if (currentPage == Page::settings)
    {
        pageContentZone = bounds;
        sampleDropZone = {};
        sampleWaveformZone = {};
        sampleNameZone = {};
        sampleEditBarZone = {};
        sampleLengthZone = {};
        controlsZone = {};
        sampleHintLabel.setBounds ({});
        rootKeyLabel.setBounds ({});
        rootKeyCombo.setBounds ({});
        
        positionSlider.setBounds ({});
        spraySlider.setBounds ({});
        grainSizeSlider.setBounds ({});
        densitySlider.setBounds ({});
        densitySyncSlider.setBounds ({});
        densityRateSlider.setBounds ({});
        shapeSlider.setBounds ({});
        pitchSlider.setBounds ({});
        spreadSlider.setBounds ({});
        gainSlider.setBounds ({});
        
        positionLabel.setBounds ({});
        sprayLabel.setBounds ({});
        grainSizeLabel.setBounds ({});
        densityLabel.setBounds ({});
        densityRateLabel.setBounds ({});
        shapeLabel.setBounds ({});
        pitchLabel.setBounds ({});
        spreadLabel.setBounds ({});
        gainLabel.setBounds ({});
        
        densitySyncButton.setBounds ({});
        densityTripletButton.setBounds ({});
        densityDottedButton.setBounds ({});
        
        lfo1SourceButton.setBounds ({});
        lfo1RateSlider.setBounds ({});
        lfo1DepthSlider.setBounds ({});
        lfo1ShapeSlider.setBounds ({});
        lfo1AmountSlider.setBounds ({});
        lfo1RateLabel.setBounds ({});
        lfo1DepthLabel.setBounds ({});
        lfo1ShapeLabel.setBounds ({});
        lfo1AmountLabel.setBounds ({});

        auto content = pageContentZone.reduced (32, 28);
        content.removeFromTop (64);

        constexpr int rowH = 54;
        auto row1 = content.removeFromTop (rowH);
        auto row2 = content.removeFromTop (rowH);
        auto row3 = content.removeFromTop (rowH);
        auto row4 = content.removeFromTop (rowH);

        trueStereoButton.setBounds (row1.removeFromRight (132));
        softClipButton.setBounds (row2.removeFromRight (132));
        voicesValueLabel.setBounds (row3.removeFromRight (132));
        interpolationQualityCombo.setBounds (row4.removeFromRight (180));
        maxVoicesSlider.setBounds ({});
        return;
    }

    // Controls at bottom
    auto controlsArea = bounds.removeFromBottom (controlsH);
    bounds.removeFromBottom (14);

    pageContentZone = bounds;
    sampleDropZone = bounds;
    sampleWaveformZone = {};
    sampleNameZone = {};
    sampleEditBarZone = {};
    sampleLengthZone = {};
    
    // Sample page header
    if (processorRef.hasLoadedSample() && currentPage == Page::sample)
    {
        auto content = sampleDropZone.reduced (20, 16);
        sampleNameZone = content.removeFromTop (18);
        auto nameRow = sampleNameZone;
        nameRow.removeFromLeft (8);
        sampleHintLabel.setBounds (nameRow);

        content.removeFromTop (8);
        sampleEditBarZone = content.removeFromBottom (32);
        content.removeFromBottom (8);
        sampleWaveformZone = content;

        auto editBar = sampleEditBarZone.reduced (10, 4);
        rootKeyLabel.setBounds (editBar.removeFromLeft (40));
        editBar.removeFromLeft (8);
        rootKeyCombo.setBounds (editBar.removeFromLeft (84));
        sampleLengthZone = editBar.removeFromRight (76);
    }
    else
    {
        auto dropContent = sampleDropZone.reduced (24, 20);
        sampleHintLabel.setBounds (dropContent.removeFromTop (32));
        rootKeyLabel.setBounds ({});
        rootKeyCombo.setBounds ({});
        sampleWaveformZone = sampleDropZone.reduced (18, 26);
        sampleWaveformZone.removeFromTop (38);
    }

    controlsZone = controlsArea;

    // Controls strip
    auto strip = controlsArea.reduced (18, 14);
    auto headerRow = strip.removeFromTop (20);
    strip.removeFromTop (6);
    auto labelRow = strip.removeFromTop (16);
    auto knobRow = strip.removeFromTop (96);
    auto buttonRow = strip.removeFromTop (22);

    // 8 slots flow
    constexpr int slotGap = 10;
    const int slotW = (knobRow.getWidth() - slotGap * 7) / 8;
    const int knobSize = juce::jlimit (62, 86, slotW - 6);

    auto placeKnob = [&] (juce::Rectangle<int>& labelRowRef, juce::Rectangle<int>& knobRowRef,
                          juce::Slider& slider, juce::Label& label, bool isPrimary)
    {
        label.setBounds (labelRowRef.removeFromLeft (slotW));
        labelRowRef.removeFromLeft (slotGap);
        
        auto slot = knobRowRef.removeFromLeft (slotW);
        knobRowRef.removeFromLeft (slotGap);
        
        slider.setBounds (slot);
        
        if (knobAnimationStates.find (&slider) == knobAnimationStates.end())
            knobAnimationStates[&slider] = KnobAnimationState();
        knobAnimationStates[&slider].isPrimary = isPrimary;
        slider.getProperties().set ("isPrimary", isPrimary);
    };

    auto labelRowCopy = labelRow;
    auto knobRowCopy = knobRow;
    
    placeKnob (labelRowCopy, knobRowCopy, positionSlider, positionLabel, true);
    placeKnob (labelRowCopy, knobRowCopy, spraySlider, sprayLabel, false);
    placeKnob (labelRowCopy, knobRowCopy, grainSizeSlider, grainSizeLabel, true);
    
    auto densityLabelSlot = labelRowCopy.removeFromLeft (slotW);
    labelRowCopy.removeFromLeft (slotGap);
    auto densityKnobSlot = knobRowCopy.removeFromLeft (slotW);
    knobRowCopy.removeFromLeft (slotGap);
    
    densityLabel.setBounds (densityLabelSlot);
    densitySyncLabel.setBounds (densityLabelSlot);
    densityRateLabel.setBounds (densityLabelSlot);
    
    densitySlider.setBounds (densityKnobSlot);
    densitySyncSlider.setBounds (densityKnobSlot);
    densityRateSlider.setBounds (densityKnobSlot);
    
    if (knobAnimationStates.find (&densitySlider) == knobAnimationStates.end())
        knobAnimationStates[&densitySlider] = KnobAnimationState();
    knobAnimationStates[&densitySlider].isPrimary = true;
    densitySlider.getProperties().set ("isPrimary", true);
    
    if (knobAnimationStates.find (&densitySyncSlider) == knobAnimationStates.end())
        knobAnimationStates[&densitySyncSlider] = KnobAnimationState();
    knobAnimationStates[&densitySyncSlider].isPrimary = true;
    densitySyncSlider.getProperties().set ("isPrimary", true);
    
    if (knobAnimationStates.find (&densityRateSlider) == knobAnimationStates.end())
        knobAnimationStates[&densityRateSlider] = KnobAnimationState();
    knobAnimationStates[&densityRateSlider].isPrimary = true;
    densityRateSlider.getProperties().set ("isPrimary", true);
    
    placeKnob (labelRowCopy, knobRowCopy, shapeSlider, shapeLabel, false);
    placeKnob (labelRowCopy, knobRowCopy, pitchSlider, pitchLabel, false);
    placeKnob (labelRowCopy, knobRowCopy, spreadSlider, spreadLabel, false);
    placeKnob (labelRowCopy, knobRowCopy, gainSlider, gainLabel, false);

    // Header zones
    auto headerCopy = headerRow;
    sourceHeaderZone = headerCopy.removeFromLeft (slotW * 2 + slotGap);
    headerCopy.removeFromLeft (slotGap);
    grainHeaderZone = headerCopy.removeFromLeft (slotW * 3 + slotGap * 2);
    headerCopy.removeFromLeft (slotGap);
    outputHeaderZone = headerCopy;

    // Density buttons
    {
        auto buttonCopy = buttonRow;
        buttonCopy.removeFromLeft (slotW * 3 + slotGap * 3);
        constexpr int chipGap = 6;
        constexpr int modeW = 52;
        constexpr int modW = 46;
        constexpr int totalChipW = modeW + chipGap + modW + chipGap + modW;
        auto densityButtonArea = buttonCopy.removeFromLeft (totalChipW).reduced (0, 2);

        if (densitySyncButton.getToggleState())
        {
            densitySyncButton.setBounds (densityButtonArea.removeFromLeft (modeW));
            densityButtonArea.removeFromLeft (chipGap);
            densityTripletButton.setBounds (densityButtonArea.removeFromLeft (modW));
            densityButtonArea.removeFromLeft (chipGap);
            densityDottedButton.setBounds (densityButtonArea.removeFromLeft (modW));
        }
        else
        {
            densitySyncButton.setBounds (densityButtonArea.removeFromLeft (modeW));
            densityTripletButton.setBounds ({});
            densityDottedButton.setBounds ({});
        }
    }

    // Modulation page
    if (currentPage == Page::modulation)
    {
        auto content = pageContentZone.reduced (32, 28);
        content.removeFromTop (24);
        content.removeFromTop (16);
        content.removeFromTop (24);

        auto leftCol = content.removeFromLeft (320);
        content.removeFromLeft (16);

        lfo1SourceButton.setBounds (content.removeFromTop (24).removeFromLeft (68));
        content.removeFromTop (16);

        auto modKnobRow = content.removeFromTop (132);
        constexpr int modGap = 12;
        const int modSlotW = (modKnobRow.getWidth() - modGap * 2) / 3;
        const int modKnobSize = juce::jlimit (62, 84, modSlotW - 12);

        auto placeModKnob = [&] (juce::Rectangle<int>& row, juce::Slider& slider, juce::Label& label)
        {
            auto slot = row.removeFromLeft (modSlotW);
            row.removeFromLeft (modGap);
            label.setBounds (slot.removeFromTop (16));
            slider.setBounds (slot);
        };

        placeModKnob (modKnobRow, lfo1RateSlider, lfo1RateLabel);
        placeModKnob (modKnobRow, lfo1DepthSlider, lfo1DepthLabel);
        placeModKnob (modKnobRow, lfo1ShapeSlider, lfo1ShapeLabel);
    }
    else
    {
        lfo1SourceButton.setBounds ({});
        lfo1RateSlider.setBounds ({});
        lfo1DepthSlider.setBounds ({});
        lfo1ShapeSlider.setBounds ({});
        lfo1AmountSlider.setBounds ({});
        lfo1RateLabel.setBounds ({});
        lfo1DepthLabel.setBounds ({});
        lfo1ShapeLabel.setBounds ({});
        lfo1AmountLabel.setBounds ({});
    }
}

void AudioPluginAudioProcessorEditor::updatePageVisibility()
{
    // Trigger page transition if page changed
    if (currentPage != pageTransitionState.targetPage)
    {
        pageTransitionState.targetPage = currentPage;
        pageTransitionState.isTransitioning = true;
        pageTransitionState.fadeAlpha = 0.0f;
    }
    
    samplePageButton.setToggleState (currentPage == Page::sample, juce::dontSendNotification);
    modulationPageButton.setToggleState (currentPage == Page::modulation, juce::dontSendNotification);
    settingsPageButton.setToggleState (currentPage == Page::settings, juce::dontSendNotification);

    const auto onSettings = currentPage == Page::settings;
    const auto onWorkPage = currentPage == Page::sample || currentPage == Page::modulation;

    sampleHintLabel.setVisible (currentPage == Page::sample);
    rootKeyLabel.setVisible (currentPage == Page::sample && processorRef.hasLoadedSample());
    rootKeyCombo.setVisible (currentPage == Page::sample && processorRef.hasLoadedSample());

    positionSlider.setVisible (onWorkPage);
    spraySlider.setVisible (onWorkPage);
    grainSizeSlider.setVisible (onWorkPage);
    densitySlider.setVisible (onWorkPage && ! densitySyncButton.getToggleState());
    densityRateSlider.setVisible (onWorkPage && densitySyncButton.getToggleState());
    densityLabel.setVisible (onWorkPage && ! densitySyncButton.getToggleState());
    densityRateLabel.setVisible (onWorkPage && densitySyncButton.getToggleState());
    shapeSlider.setVisible (onWorkPage);
    pitchSlider.setVisible (onWorkPage);
    spreadSlider.setVisible (onWorkPage);
    gainSlider.setVisible (onWorkPage);

    positionLabel.setVisible (onWorkPage);
    sprayLabel.setVisible (onWorkPage);
    grainSizeLabel.setVisible (onWorkPage);
    shapeLabel.setVisible (onWorkPage);
    pitchLabel.setVisible (onWorkPage);
    spreadLabel.setVisible (onWorkPage);
    gainLabel.setVisible (onWorkPage);

    densitySyncButton.setVisible (onWorkPage);
    densityTripletButton.setVisible (onWorkPage && densitySyncButton.getToggleState());
    densityDottedButton.setVisible (onWorkPage && densitySyncButton.getToggleState());

    const auto onModulation = currentPage == Page::modulation;
    lfo1SourceButton.setVisible (onModulation);
    lfo1RateSlider.setVisible (onModulation);
    lfo1DepthSlider.setVisible (onModulation);
    lfo1ShapeSlider.setVisible (onModulation);
    lfo1AmountSlider.setVisible (false);
    lfo1RateLabel.setVisible (onModulation);
    lfo1DepthLabel.setVisible (onModulation);
    lfo1ShapeLabel.setVisible (onModulation);
    lfo1AmountLabel.setVisible (false);

    trueStereoButton.setVisible (onSettings);
    softClipButton.setVisible (onSettings);
    maxVoicesSlider.setVisible (false);
    maxVoicesLabel.setVisible (false);
    voicesValueLabel.setVisible (onSettings);
    interpolationQualityCombo.setVisible (onSettings);

    if (! onModulation)
    {
        isDraggingModulationSource = false;
        isDraggingModulationHandle = false;
        hoveredModulationTarget = -1;
        draggingModulationTarget = -1;
    }

    updateModulationAmountDisplay();
    refreshModulationSliderDecorations();
}

void AudioPluginAudioProcessorEditor::updateVoicesValueDisplay()
{
    voicesValueLabel.setText (juce::String (static_cast<int> (std::round (maxVoicesSlider.getValue()))),
                              juce::dontSendNotification);
}

void AudioPluginAudioProcessorEditor::updateModulationAmountDisplay()
{
    if (selectedModulationTarget < 0)
    {
        lfo1AmountSlider.setEnabled (false);
        lfo1AmountSlider.setValue (0.0, juce::dontSendNotification);
        return;
    }

    lfo1AmountSlider.setEnabled (true);

    if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (selectedModulationTarget))
        lfo1AmountSlider.setValue (processorRef.getLfo1AssignmentAmount (parameterID), juce::dontSendNotification);
}

bool AudioPluginAudioProcessorEditor::isAssignableSlider (const juce::Slider* slider) const
{
    return getParameterIDForSlider (slider).isNotEmpty();
}

juce::String AudioPluginAudioProcessorEditor::getParameterIDForSlider (const juce::Slider* slider) const
{
    if (slider == &positionSlider)   return ParameterIDs::position;
    if (slider == &spraySlider)      return ParameterIDs::spray;
    if (slider == &grainSizeSlider)  return ParameterIDs::grainSizeMs;
    if (slider == &densitySlider || slider == &densityRateSlider || slider == &densitySyncSlider) return ParameterIDs::densityHz;
    if (slider == &shapeSlider)      return ParameterIDs::shapeType;
    if (slider == &pitchSlider)      return ParameterIDs::pitchSemitones;
    if (slider == &spreadSlider)     return ParameterIDs::spread;
    if (slider == &gainSlider)       return ParameterIDs::gain;
    return {};
}

juce::Slider* AudioPluginAudioProcessorEditor::getSliderForModulationTarget (int index) const
{
    if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (index))
    {
        const juce::String id (parameterID);
        if (id == ParameterIDs::position)      return const_cast<juce::Slider*> (&positionSlider);
        if (id == ParameterIDs::spray)         return const_cast<juce::Slider*> (&spraySlider);
        if (id == ParameterIDs::grainSizeMs)   return const_cast<juce::Slider*> (&grainSizeSlider);
        if (id == ParameterIDs::densityHz)     return densitySyncButton.getToggleState()
                                                        ? const_cast<juce::Slider*> (&densityRateSlider)
                                                        : const_cast<juce::Slider*> (&densitySlider);
        if (id == ParameterIDs::shapeType)     return const_cast<juce::Slider*> (&shapeSlider);
        if (id == ParameterIDs::pitchSemitones)return const_cast<juce::Slider*> (&pitchSlider);
        if (id == ParameterIDs::spread)        return const_cast<juce::Slider*> (&spreadSlider);
        if (id == ParameterIDs::gain)          return const_cast<juce::Slider*> (&gainSlider);
    }

    return nullptr;
}

juce::Slider* AudioPluginAudioProcessorEditor::getAssignableSliderAt (juce::Point<float> position) const
{
    const std::array<juce::Slider*, 8> sliders
    {
        const_cast<juce::Slider*> (&positionSlider),
        const_cast<juce::Slider*> (&spraySlider),
        const_cast<juce::Slider*> (&grainSizeSlider),
        densitySyncButton.getToggleState() ? const_cast<juce::Slider*> (&densityRateSlider)
                                           : const_cast<juce::Slider*> (&densitySlider),
        const_cast<juce::Slider*> (&shapeSlider),
        const_cast<juce::Slider*> (&pitchSlider),
        const_cast<juce::Slider*> (&spreadSlider),
        const_cast<juce::Slider*> (&gainSlider)
    };

    for (auto* slider : sliders)
        if (slider != nullptr && slider->isShowing() && slider->getBounds().toFloat().contains (position))
            return slider;

    return nullptr;
}

juce::Rectangle<float> AudioPluginAudioProcessorEditor::getModulationHandleBoundsForTarget (int index) const
{
    if (auto* slider = getSliderForModulationTarget (index))
    {
        auto bounds = slider->getBounds().toFloat();
        auto handle = juce::Rectangle<float> (18.0f, 18.0f);
        handle.setCentre (bounds.getRight() - 4.0f, bounds.getY() + 10.0f);
        return handle;
    }

    return {};
}

int AudioPluginAudioProcessorEditor::getModulationHandleTargetAt (juce::Point<float> position) const
{
    for (int index = 0; index < 8; ++index)
    {
        const auto amount = processorRef.getLfo1AssignmentAmount (juce::String (AudioPluginAudioProcessor::getModulationTargetParameterID (index)));
        if (std::abs (amount) <= 0.001f)
            continue;

        if (getModulationHandleBoundsForTarget (index).contains (position))
            return index;
    }

    return -1;
}

void AudioPluginAudioProcessorEditor::selectModulationTargetSlider (juce::Slider* slider)
{
    const auto parameterID = getParameterIDForSlider (slider);
    const auto index = AudioPluginAudioProcessor::getModulationTargetIndexForParameter (parameterID);
    if (index < 0)
        return;

    selectedModulationTarget = index;
    updateModulationAmountDisplay();
    refreshModulationSliderDecorations();
    repaint();
}

float AudioPluginAudioProcessorEditor::getLfoPreviewValue() const noexcept
{
    const auto shape = static_cast<float> (lfo1ShapeSlider.getValue());
    const auto depth = static_cast<float> (lfo1DepthSlider.getValue());
    return evaluateLfoPreview (lfoPreviewPhase, shape) * depth;
}

float AudioPluginAudioProcessorEditor::evaluateLfoPreview (float phase, float shape) noexcept
{
    return SitoDSP::evaluateShapeWaveform (phase, shape);
}

void AudioPluginAudioProcessorEditor::refreshModulationSliderDecorations()
{
    const std::array<juce::Slider*, 8> sliders
    {
        &positionSlider,
        &spraySlider,
        &grainSizeSlider,
        &densitySlider,
        &shapeSlider,
        &pitchSlider,
        &spreadSlider,
        &gainSlider
    };

    const auto preview = getLfoPreviewValue();

    for (auto* slider : sliders)
    {
        const auto parameterID = getParameterIDForSlider (slider);
        const auto amount = processorRef.getLfo1AssignmentAmount (parameterID);
        const auto targetIndex = AudioPluginAudioProcessor::getModulationTargetIndexForParameter (parameterID);
        const auto bypassed = processorRef.isLfo1AssignmentBypassed (parameterID);
        slider->getProperties().set ("modAmount", amount);
        slider->getProperties().set ("modPreview", preview);
        slider->getProperties().set ("modBypassed", bypassed);
        slider->getProperties().set ("modSelected",
                                     currentPage == Page::modulation
                                         && (targetIndex == selectedModulationTarget || targetIndex == hoveredModulationTarget));
        
        // Pass animation state to look and feel
        if (knobAnimationStates.find (slider) != knobAnimationStates.end())
        {
            const auto& state = knobAnimationStates[slider];
            slider->getProperties().set ("hoverAlpha", state.hoverAlpha);
            slider->getProperties().set ("arcAnimationPhase", state.arcAnimationPhase);
            slider->getProperties().set ("isPrimary", state.isPrimary);
        }
    }

    densityRateSlider.getProperties().set ("modAmount", processorRef.getLfo1AssignmentAmount (ParameterIDs::densityHz));
    densityRateSlider.getProperties().set ("modPreview", preview);
    densityRateSlider.getProperties().set ("modBypassed", processorRef.isLfo1AssignmentBypassed (ParameterIDs::densityHz));
    densityRateSlider.getProperties().set ("modSelected",
                                           currentPage == Page::modulation
                                               && (selectedModulationTarget == AudioPluginAudioProcessor::getModulationTargetIndexForParameter (ParameterIDs::densityHz)
                                                   || hoveredModulationTarget == AudioPluginAudioProcessor::getModulationTargetIndexForParameter (ParameterIDs::densityHz)));
    
    // Pass animation state for density rate slider
    if (knobAnimationStates.find (&densityRateSlider) != knobAnimationStates.end())
    {
        const auto& state = knobAnimationStates[&densityRateSlider];
        densityRateSlider.getProperties().set ("hoverAlpha", state.hoverAlpha);
        densityRateSlider.getProperties().set ("arcAnimationPhase", state.arcAnimationPhase);
        densityRateSlider.getProperties().set ("isPrimary", state.isPrimary);
    }
}

bool AudioPluginAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (currentPage != Page::sample)
        return false;

    if (files.isEmpty())
        return false;

    const auto file = juce::File (files[0]);
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".mp3";
}

void AudioPluginAudioProcessorEditor::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    isDraggingSample = isInterestedInFileDrag (files) && sampleDropZone.contains (x, y);
    repaint();
}

void AudioPluginAudioProcessorEditor::fileDragMove (const juce::StringArray& files, int x, int y)
{
    const auto shouldHighlight = isInterestedInFileDrag (files) && sampleDropZone.contains (x, y);

    if (shouldHighlight != isDraggingSample)
    {
        isDraggingSample = shouldHighlight;
        repaint();
    }
}

void AudioPluginAudioProcessorEditor::fileDragExit (const juce::StringArray& files)
{
    juce::ignoreUnused (files);
    isDraggingSample = false;
    repaint();
}

void AudioPluginAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    isDraggingSample = false;

    if (files.isEmpty())
        return;

    if (processorRef.loadSample (juce::File (files[0])))
    {
        lastSeenSampleGeneration = processorRef.getLoadedSampleGeneration();
        updateSampleStatus();
        updatePageVisibility();
        resized();
        repaint();
    }
}

void AudioPluginAudioProcessorEditor::updateSampleStatus()
{
    if (! processorRef.hasLoadedSample())
    {
        sampleHintLabel.setText ("Drop WAV / AIFF / FLAC / MP3 here",
                                 juce::dontSendNotification);
        return;
    }

    sampleHintLabel.setText (processorRef.getLoadedSampleName().upToFirstOccurrenceOf (".", false, false),
                             juce::dontSendNotification);
}

juce::Rectangle<float> AudioPluginAudioProcessorEditor::getWaveformArea() const
{
    if (! sampleWaveformZone.isEmpty())
        return sampleWaveformZone.toFloat();

    auto waveformArea = sampleDropZone.toFloat().reduced (18.0f, 26.0f);
    waveformArea.removeFromTop (38.0f);
    return waveformArea;
}

bool AudioPluginAudioProcessorEditor::beginWaveformInteraction (const juce::MouseEvent& e)
{
    if (currentPage != Page::sample)
        return false;

    if (! processorRef.hasLoadedSample())
        return false;

    if (findParentSlider (e.eventComponent) != nullptr)
        return false;

    if (dynamic_cast<juce::Button*> (e.eventComponent) != nullptr)
        return false;

    const auto waveformArea = getWaveformArea();
    const auto pos = e.getEventRelativeTo (this).position;

    if (! waveformArea.contains (pos))
        return false;

    isDraggingWaveformSpray = e.mods.isAltDown() || e.mods.isRightButtonDown();
    isDraggingWaveformPosition = ! isDraggingWaveformSpray;
    updateWaveformInteraction (e);
    return true;
}

void AudioPluginAudioProcessorEditor::updateWaveformInteraction (const juce::MouseEvent& e)
{
    const auto waveformArea = getWaveformArea();
    const auto pos = e.getEventRelativeTo (this).position;
    const auto normalizedX = juce::jlimit (0.0f, 1.0f, (pos.x - waveformArea.getX()) / juce::jmax (1.0f, waveformArea.getWidth()));

    if (isDraggingWaveformPosition)
    {
        hoveredSlider = &positionSlider;
        positionSlider.setValue (normalizedX, juce::sendNotificationSync);
        repaint();
        return;
    }

    if (isDraggingWaveformSpray)
    {
        hoveredSlider = &spraySlider;
        const auto center = static_cast<float> (positionSlider.getValue());
        const auto distance = std::abs (normalizedX - center);
        const auto spray = juce::jlimit (0.0f, 1.0f, distance * 2.0f);
        spraySlider.setValue (spray, juce::sendNotificationSync);
        repaint();
    }
}

void AudioPluginAudioProcessorEditor::configureKnob (juce::Slider& slider,
                                                     juce::Label& label,
                                                     const juce::String& text)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setNumDecimalPlacesToDisplay (1);
    slider.setScrollWheelEnabled (false);
    slider.setLookAndFeel (&lookAndFeel);
    slider.setName (text.toLowerCase());
    if (slider.getName() == "shape")
    {
        slider.setNumDecimalPlacesToDisplay (0);
    }
    else if (slider.getName() == "spray")
    {
        slider.setNumDecimalPlacesToDisplay (0);
        slider.textFromValueFunction = [] (double v)
        {
            const auto percent = static_cast<int> (std::round (juce::jlimit (0.0, 1.0, v) * 100.0));
            return juce::String (percent);
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            const auto percent = juce::jlimit (0.0, 100.0, text.getDoubleValue());
            return percent / 100.0;
        };
    }
    else if (slider.getName() == "spread")
    {
        slider.setNumDecimalPlacesToDisplay (0);
        slider.textFromValueFunction = [] (double v)
        {
            const auto percent = static_cast<int> (std::round (juce::jlimit (0.0, 1.0, v) * 100.0));
            return juce::String (percent);
        };
        slider.valueFromTextFunction = [] (const juce::String& text)
        {
            const auto percent = juce::jlimit (0.0, 100.0, text.getDoubleValue());
            return percent / 100.0;
        };
    }
    addAndMakeVisible (slider);

    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    label.setColour (juce::Label::textColourId, textSecondary);
    addAndMakeVisible (label);
}

void AudioPluginAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    // Don't render modulation handles if preset browser is visible (z-order fix)
    const auto presetBrowserVisible = presetBrowser && presetBrowser->isVisible();
    
    if (currentPage != Page::settings && !presetBrowserVisible)
    {
        for (int index = 0; index < 8; ++index)
        {
            const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (index);
            if (parameterID == nullptr)
                continue;

            const auto amount = processorRef.getLfo1AssignmentAmount (parameterID);
            const auto showHandle = std::abs (amount) > 0.001f
                || index == draggingModulationTarget
                || (isDraggingModulationSource && getSliderForModulationTarget (index) != nullptr);

            if (! showHandle)
                continue;

            const auto handle = getModulationHandleBoundsForTarget (index);
            if (handle.isEmpty())
                continue;

            const auto isHot = index == hoveredModulationTarget || index == draggingModulationTarget || index == selectedModulationTarget;
            const auto bypassed = processorRef.isLfo1AssignmentBypassed (parameterID);
            const auto tint = amount >= 0.0f ? accentGlowColour : juce::Colour (0xffff7aa8);
            const auto normalized = juce::jlimit (0.0f, 1.0f, std::abs (amount));

            // Get animation state for this handle
            auto handleState = modulationHandleStates[index];
            const auto scaledHandle = handle.withSizeKeepingCentre (handle.getWidth() * handleState.scaleAmount,
                                                                     handle.getHeight() * handleState.scaleAmount);

            g.setColour (juce::Colour (0xff1b1423).withAlpha (0.96f));
            g.fillEllipse (scaledHandle);
            
            // Enhanced glow during assignment workflow
            g.setColour (tint.withAlpha (bypassed ? 0.34f : (isHot ? 0.95f : 0.72f) + handleState.glowAlpha * 0.3f));
            g.drawEllipse (scaledHandle, isHot ? 1.6f : 1.2f);

            juce::Path amountArc;
            amountArc.addCentredArc (scaledHandle.getCentreX(),
                                     scaledHandle.getCentreY(),
                                     scaledHandle.getWidth() * 0.34f,
                                     scaledHandle.getHeight() * 0.34f,
                                     0.0f,
                                     -juce::MathConstants<float>::halfPi,
                                     -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * normalized,
                                     true);
            g.setColour (tint.withAlpha (bypassed ? 0.28f : (isHot ? 0.95f : 0.78f)));
            g.strokePath (amountArc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (tint.withAlpha (bypassed ? 0.36f : 0.95f));
            g.fillEllipse (scaledHandle.getCentreX() - 2.0f, scaledHandle.getCentreY() - 2.0f, 4.0f, 4.0f);

            if (bypassed)
            {
                g.setColour (tint.withAlpha (0.50f));
                g.drawLine (scaledHandle.getX() + 4.0f, scaledHandle.getBottom() - 4.0f,
                            scaledHandle.getRight() - 4.0f, scaledHandle.getY() + 4.0f, 1.2f);
            }
        }
    }

    if (isDraggingModulationSource)
    {
        const auto sourceBounds = lfo1SourceButton.getBounds().toFloat();
        const auto start = juce::Point<float> (sourceBounds.getCentreX(), sourceBounds.getCentreY());

        juce::Path dragPath;
        dragPath.startNewSubPath (start);
        dragPath.quadraticTo ((start.x + modulationDragPosition.x) * 0.5f,
                              juce::jmin (start.y, modulationDragPosition.y) - 24.0f,
                              modulationDragPosition.x,
                              modulationDragPosition.y);

        g.setColour (accentGlowColour.withAlpha (0.24f));
        g.strokePath (dragPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accentGlowColour.withAlpha (0.86f));
        g.strokePath (dragPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (accentGlowColour.withAlpha (0.95f));
        g.fillEllipse (modulationDragPosition.x - 4.0f, modulationDragPosition.y - 4.0f, 8.0f, 8.0f);
    }

    if (hoveredSlider != nullptr
        && (hoveredSlider->isMouseOverOrDragging()
            || (isDraggingWaveformPosition && hoveredSlider == &positionSlider)
            || (isDraggingWaveformSpray && hoveredSlider == &spraySlider)))
    {
        const auto text = hoveredSlider->getTextFromValue (hoveredSlider->getValue());

        auto bubble = hoveredSlider->getBounds().toFloat().removeFromBottom (18.0f).reduced (10.0f, 0.0f);
        bubble = bubble.withHeight (18.0f);

        g.setColour (juce::Colour (0xff1d1825).withAlpha (0.92f));
        g.fillRoundedRectangle (bubble, 6.0f);

        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
        g.setColour (textPrimary.withAlpha (0.95f));
        g.drawFittedText (text, bubble.toNearestInt(), juce::Justification::centred, 1);
        return;
    }

    if (hoveredAssistComponent == nullptr)
        return;

    if (! hoveredAssistComponent->isShowing()
        || ! hoveredAssistComponent->isMouseOverOrDragging())
        return;

    const auto text = getAssistTextForComponent (hoveredAssistComponent);
    if (text.isEmpty())
        return;

    auto target = hoveredAssistComponent->getBoundsInParent().toFloat();
    auto bubble = juce::Rectangle<float> (0.0f,
                                          0.0f,
                                          juce::jlimit (120.0f, 220.0f, 18.0f + static_cast<float> (text.length()) * 6.2f),
                                          22.0f)
                      .withCentre (juce::Point<float> (target.getCentreX(), target.getY() - 14.0f));

    const auto safeArea = getLocalBounds().toFloat().reduced (18.0f);
    if (bubble.getX() < safeArea.getX())
        bubble.setX (safeArea.getX());
    if (bubble.getRight() > safeArea.getRight())
        bubble.setX (safeArea.getRight() - bubble.getWidth());
    if (bubble.getY() < safeArea.getY())
        bubble = bubble.withY (target.getBottom() + 8.0f);

    g.setColour (juce::Colour (0xff1d1825).withAlpha (0.96f));
    g.fillRoundedRectangle (bubble, 7.0f);
    g.setColour (cardBorderColour.withAlpha (0.42f));
    g.drawRoundedRectangle (bubble, 7.0f, 1.0f);

    g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::plain)));
    g.setColour (textPrimary.withAlpha (0.95f));
    g.drawFittedText (text, bubble.toNearestInt().reduced (8, 0), juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto eventPosition = e.getEventRelativeTo (this).position;

    if (e.mods.isPopupMenu())
    {
        int menuTarget = getModulationHandleTargetAt (eventPosition);
        juce::Rectangle<int> menuBounds;

        if (menuTarget >= 0)
            menuBounds = getModulationHandleBoundsForTarget (menuTarget).toNearestInt();
        else if (auto* slider = findParentSlider (e.eventComponent); isAssignableSlider (slider))
        {
            const auto parameterID = getParameterIDForSlider (slider);
            menuTarget = AudioPluginAudioProcessor::getModulationTargetIndexForParameter (parameterID);
            menuBounds = slider->getBounds();
        }

        if (menuTarget >= 0)
            if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (menuTarget))
                if (std::abs (processorRef.getLfo1AssignmentAmount (parameterID)) > 0.001f)
                {
                    juce::PopupMenu rootMenu;
                    juce::PopupMenu modulationsMenu;
                    juce::PopupMenu lfoMenu;
                    const auto bypassed = processorRef.isLfo1AssignmentBypassed (parameterID);

                    rootMenu.setLookAndFeel (&lookAndFeel);
                    modulationsMenu.setLookAndFeel (&lookAndFeel);
                    lfoMenu.setLookAndFeel (&lookAndFeel);

                    lfoMenu.addItem (1, "Bypass", true, bypassed);
                    lfoMenu.addItem (2, "Remove");
                    modulationsMenu.addSubMenu ("LFO 1", lfoMenu);
                    rootMenu.addSubMenu ("Modulations", modulationsMenu);

                    selectedModulationTarget = menuTarget;
                    updateModulationAmountDisplay();
                    refreshModulationSliderDecorations();

                    rootMenu.showMenuAsync (juce::PopupMenu::Options()
                                                .withTargetScreenArea (localAreaToGlobal (menuBounds))
                                                .withParentComponent (this),
                                            [this, parameter = juce::String (parameterID)] (int result)
                                            {
                                                if (result == 1)
                                                    processorRef.setLfo1AssignmentBypassed (parameter, ! processorRef.isLfo1AssignmentBypassed (parameter));
                                                else if (result == 2)
                                                    processorRef.removeLfo1Assignment (parameter);

                                                updateModulationAmountDisplay();
                                                refreshModulationSliderDecorations();
                                                repaint();
                                            });
                    return;
                }
    }

    if (currentPage != Page::settings)
    {
        const auto handleTarget = getModulationHandleTargetAt (eventPosition);
        if (handleTarget >= 0)
        {
            draggingModulationTarget = handleTarget;
            selectedModulationTarget = handleTarget;
            isDraggingModulationHandle = true;
            modulationHandleDragStartY = eventPosition.y;
            if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (handleTarget))
                modulationHandleStartAmount = processorRef.getLfo1AssignmentAmount (parameterID);
            updateModulationAmountDisplay();
            refreshModulationSliderDecorations();
            repaint();
            return;
        }
    }

    if (currentPage == Page::modulation && e.eventComponent == &lfo1SourceButton)
    {
        isDraggingModulationSource = true;
        modulationDragPosition = eventPosition;
        hoveredModulationTarget = -1;
        refreshModulationSliderDecorations();
        repaint();
        return;
    }

    if (currentPage == Page::settings && e.eventComponent == &voicesValueLabel)
    {
        isDraggingVoicesValue = true;
        voicesDragStartY = eventPosition.y;
        voicesDragStartValue = maxVoicesSlider.getValue();
        hoveredAssistComponent = &voicesValueLabel;
        return;
    }

    if (currentPage == Page::sample)
        beginWaveformInteraction (e);
}

void AudioPluginAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (isDraggingModulationHandle)
    {
        const auto position = e.getEventRelativeTo (this).position;
        const auto deltaY = modulationHandleDragStartY - position.y;
        const auto sensitivity = e.mods.isShiftDown() ? 220.0f : 120.0f;
        const auto amount = juce::jlimit (-1.0f, 1.0f, modulationHandleStartAmount + (deltaY / sensitivity));

        if (const auto* parameterID = AudioPluginAudioProcessor::getModulationTargetParameterID (draggingModulationTarget))
        {
            processorRef.setLfo1AssignmentAmount (parameterID, amount);
            selectedModulationTarget = draggingModulationTarget;
            updateModulationAmountDisplay();
            refreshModulationSliderDecorations();
            repaint();
        }
        return;
    }

    if (isDraggingModulationSource)
    {
        modulationDragPosition = e.getEventRelativeTo (this).position;
        hoveredModulationTarget = -1;

        if (auto* slider = getAssignableSliderAt (modulationDragPosition))
            hoveredModulationTarget = AudioPluginAudioProcessor::getModulationTargetIndexForParameter (getParameterIDForSlider (slider));

        refreshModulationSliderDecorations();
        repaint();
        return;
    }

    if (isDraggingVoicesValue)
    {
        const auto currentY = e.getEventRelativeTo (this).position.y;
        const auto deltaY = voicesDragStartY - currentY;
        const auto step = static_cast<int> (std::round (deltaY / 18.0f));
        const auto newValue = juce::jlimit (maxVoicesSlider.getMinimum(),
                                            maxVoicesSlider.getMaximum(),
                                            voicesDragStartValue + static_cast<double> (step));
        maxVoicesSlider.setValue (newValue, juce::sendNotificationSync);
        return;
    }

    if (isDraggingWaveformPosition || isDraggingWaveformSpray)
    {
        updateWaveformInteraction (e);
        return;
    }

    if (currentPage == Page::sample)
        beginWaveformInteraction (e);
}

void AudioPluginAudioProcessorEditor::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (isDraggingModulationHandle)
    {
        isDraggingModulationHandle = false;
        draggingModulationTarget = -1;
        refreshModulationSliderDecorations();
        repaint();
        return;
    }

    if (isDraggingModulationSource)
    {
        if (auto* slider = getAssignableSliderAt (e.getEventRelativeTo (this).position))
        {
            selectModulationTargetSlider (slider);
            const auto parameterID = getParameterIDForSlider (slider);
            if (std::abs (processorRef.getLfo1AssignmentAmount (parameterID)) <= 0.001f)
                processorRef.setLfo1AssignmentAmount (parameterID, 0.35f);
            updateModulationAmountDisplay();
        }

        isDraggingModulationSource = false;
        hoveredModulationTarget = -1;
        refreshModulationSliderDecorations();
        repaint();
        return;
    }

    isDraggingVoicesValue = false;
    const auto wasDraggingWaveform = isDraggingWaveformPosition || isDraggingWaveformSpray;
    isDraggingWaveformPosition = false;
    isDraggingWaveformSpray = false;

    if (wasDraggingWaveform)
        repaint();
}

void AudioPluginAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    if (isDraggingWaveformPosition || isDraggingWaveformSpray || isDraggingVoicesValue || isDraggingModulationSource)
        return;

    const auto hoveredHandleTarget = getModulationHandleTargetAt (e.getEventRelativeTo (this).position);
    auto* slider = findParentSlider (e.eventComponent);
    auto* assist = slider == nullptr ? findAssistComponent (e.eventComponent) : nullptr;
    auto changed = false;

    if (hoveredHandleTarget != hoveredModulationTarget)
    {
        hoveredModulationTarget = hoveredHandleTarget;
        changed = true;
    }

    if (slider != hoveredSlider)
    {
        hoveredSlider = slider;
        changed = true;
    }

    if (assist != hoveredAssistComponent)
    {
        hoveredAssistComponent = assist;
        changed = true;
    }

    if (changed)
    {
        repaint();
    }
}

void AudioPluginAudioProcessorEditor::mouseExit (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (isDraggingWaveformPosition || isDraggingWaveformSpray || isDraggingVoicesValue || isDraggingModulationSource)
        return;

    const auto hadHoverState = hoveredSlider != nullptr || hoveredAssistComponent != nullptr || hoveredModulationTarget >= 0;
    hoveredSlider = nullptr;
    hoveredAssistComponent = nullptr;
    hoveredModulationTarget = -1;

    if (hadHoverState)
        repaint();
}

bool AudioPluginAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
#if SITO_ENABLE_INSPECTOR
#if JUCE_WINDOWS
    const auto inspectorToggleModifierDown = key.getModifiers().isCtrlDown();
#else
    const auto inspectorToggleModifierDown = key.getModifiers().isCommandDown();
#endif

    if (inspectorToggleModifierDown && (key.getTextCharacter() == 'i' || key.getTextCharacter() == 'I'))
    {
        toggleInspector();
        return true;
    }
#endif

    if (key == juce::KeyPress::escapeKey)
    {
        isDraggingModulationSource = false;
        isDraggingModulationHandle = false;
        hoveredModulationTarget = -1;
        draggingModulationTarget = -1;
        refreshModulationSliderDecorations();
        repaint();
        return true;
    }

    return false;
}

void AudioPluginAudioProcessorEditor::toggleInspector()
{
#if SITO_ENABLE_INSPECTOR
    if (inspector == nullptr)
    {
        inspector = std::make_unique<melatonin::Inspector> (*this, false);
        inspector->onClose = [this]()
        {
            inspectorButton.setToggleState (false, juce::dontSendNotification);
            inspector.reset();
        };
    }

    const auto shouldShowInspector = ! inspector->isVisible();

    if (shouldShowInspector)
    {
        inspector->setVisible (true);
        inspector->toggle (true);
        inspector->toFront (true);
    }
    else
    {
        inspector->toggle (false);
        inspector->setVisible (false);
    }

    inspectorButton.setToggleState (shouldShowInspector, juce::dontSendNotification);
#endif
}

void AudioPluginAudioProcessorEditor::presetListChanged()
{
    // Preset list has changed, no action needed in editor
}

void AudioPluginAudioProcessorEditor::currentPresetChanged (const juce::String& presetName)
{
    if (presetName.isEmpty())
        presetNameLabel.setText ("No Preset", juce::dontSendNotification);
    else
        presetNameLabel.setText (presetName, juce::dontSendNotification);
    
    repaint();
}

std::unique_ptr<juce::Drawable> AudioPluginAudioProcessorEditor::loadSVGIcon (const void* data, size_t size)
{
    return loadSVGFromMemory (data, size);
}
