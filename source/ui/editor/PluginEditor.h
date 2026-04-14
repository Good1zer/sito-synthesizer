#pragma once

#include <array>
#include <limits>

#include "plugin/PluginProcessor.h"
#include "ui/lookandfeel/SitoLookAndFeel.h"
#include "ui/preset/PresetBrowser.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              public juce::FileDragAndDropTarget,
                                              public PresetManager::Listener,
                                              private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragMove (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    
    // PresetManager::Listener
    void presetListChanged() override;
    void currentPresetChanged (const juce::String& presetName) override;

private:
    enum class Page
    {
        sample,
        modulation,
        settings
    };

    struct KnobAnimationState
    {
        float hoverAlpha = 0.0f;
        float arcAnimationPhase = 0.0f;
        bool isPrimary = false;
    };

    void timerCallback() override;
    void updateSampleStatus();
    void configureKnob (juce::Slider& slider, juce::Label& label, const juce::String& text);
    juce::Rectangle<float> getWaveformArea() const;
    std::unique_ptr<juce::Drawable> loadSVGIcon (const void* data, size_t size);
    bool beginWaveformInteraction (const juce::MouseEvent&);
    void updateWaveformInteraction (const juce::MouseEvent&);
    void updatePageVisibility();
    void updateVoicesValueDisplay();
    void updateModulationAmountDisplay();
    bool isAssignableSlider (const juce::Slider* slider) const;
    juce::String getParameterIDForSlider (const juce::Slider* slider) const;
    juce::Slider* getSliderForModulationTarget (int index) const;
    juce::Slider* getAssignableSliderAt (juce::Point<float> position) const;
    juce::Rectangle<float> getModulationHandleBoundsForTarget (int index) const;
    int getModulationHandleTargetAt (juce::Point<float> position) const;
    void selectModulationTargetSlider (juce::Slider* slider);
    float getLfoPreviewValue() const noexcept;
    static float evaluateLfoPreview (float phase, float shape) noexcept;
    void refreshModulationSliderDecorations();

    AudioPluginAudioProcessor& processorRef;
    SitoLookAndFeel lookAndFeel;
    juce::Rectangle<int> sampleDropZone;
    juce::Rectangle<int> controlsZone;
    juce::Rectangle<int> pageContentZone;
    juce::Rectangle<int> sourceHeaderZone;
    juce::Rectangle<int> grainHeaderZone;
    juce::Rectangle<int> outputHeaderZone;
    juce::Rectangle<int> topTabsZone;
    bool isDraggingSample = false;
    juce::Slider* hoveredSlider = nullptr;
    juce::Component* hoveredAssistComponent = nullptr;
    bool isDraggingWaveformPosition = false;
    bool isDraggingWaveformSpray = false;
    bool isDraggingVoicesValue = false;
    bool isDraggingModulationSource = false;
    bool isDraggingModulationHandle = false;
    float voicesDragStartY = 0.0f;
    double voicesDragStartValue = 0.0;
    juce::Point<float> modulationDragPosition;
    float modulationHandleDragStartY = 0.0f;
    float modulationHandleStartAmount = 0.0f;
     float lfoPreviewPhase = 0.0f;
     float waveformPulsePhase = 0.0f;
     int selectedModulationTarget = -1;
    int hoveredModulationTarget = -1;
    int draggingModulationTarget = -1;
    Page currentPage = Page::sample;
    uint64_t lastSeenSampleGeneration = std::numeric_limits<uint64_t>::max();

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label sampleStatusLabel;
    juce::Label sampleHintLabel;

    juce::GroupComponent sourceGroup;
    juce::GroupComponent grainGroup;
    juce::GroupComponent outputGroup;

    juce::Slider positionSlider;
    juce::Slider spraySlider;
    juce::Slider grainSizeSlider;
    juce::Slider densitySlider;
    juce::Slider densitySyncSlider; // hidden param driver (0..17)
    juce::Slider densityRateSlider; // visible in BPM mode (0..N-1)
    juce::Slider pitchSlider;
    juce::Slider gainSlider;
    juce::Slider shapeSlider;
    juce::Slider spreadSlider;
    juce::Slider lfo1RateSlider;
    juce::Slider lfo1DepthSlider;
    juce::Slider lfo1ShapeSlider;
    juce::Slider lfo1AmountSlider;
    juce::ToggleButton samplePageButton;
    juce::ToggleButton modulationPageButton;
    juce::ToggleButton settingsPageButton;
    juce::DrawableButton presetPrevButton { "prev", juce::DrawableButton::ImageFitted };
    juce::DrawableButton presetNextButton { "next", juce::DrawableButton::ImageFitted };
    juce::Label presetNameLabel;
    juce::DrawableButton presetSaveButton { "save", juce::DrawableButton::ImageFitted };
    juce::DrawableButton presetMenuButton { "menu", juce::DrawableButton::ImageFitted };
    juce::ToggleButton lfo1SourceButton;
    juce::ToggleButton softClipButton;
    juce::ToggleButton trueStereoButton;
    juce::ToggleButton densitySyncButton;
    juce::ToggleButton densityTripletButton;
    juce::ToggleButton densityDottedButton;
    juce::Slider maxVoicesSlider;
    juce::ComboBox interpolationQualityCombo;

    juce::Label positionLabel;
    juce::Label sprayLabel;
    juce::Label grainSizeLabel;
    juce::Label densityLabel;
    juce::Label densitySyncLabel; // hidden driver label
    juce::Label densityRateLabel; // visible in BPM mode
    juce::Label pitchLabel;
    juce::Label gainLabel;
    juce::Label shapeLabel;
    juce::Label spreadLabel;
    juce::Label lfo1RateLabel;
    juce::Label lfo1DepthLabel;
    juce::Label lfo1ShapeLabel;
    juce::Label lfo1AmountLabel;
    juce::Label maxVoicesLabel;
    juce::Label voicesValueLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> positionAttachment;
    std::unique_ptr<SliderAttachment> sprayAttachment;
    std::unique_ptr<SliderAttachment> grainSizeAttachment;
    std::unique_ptr<SliderAttachment> densityAttachment;
    std::unique_ptr<SliderAttachment> densitySyncAttachment;
    std::unique_ptr<SliderAttachment> pitchAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> shapeAttachment;
    std::unique_ptr<SliderAttachment> spreadAttachment;
    std::unique_ptr<SliderAttachment> lfo1RateAttachment;
    std::unique_ptr<SliderAttachment> lfo1DepthAttachment;
    std::unique_ptr<SliderAttachment> lfo1ShapeAttachment;
    std::unique_ptr<ButtonAttachment> softClipAttachment;
    std::unique_ptr<ButtonAttachment> trueStereoAttachment;
    std::unique_ptr<ButtonAttachment> densitySyncEnabledAttachment;
    std::unique_ptr<ButtonAttachment> densitySyncTripletEnabledAttachment;
    std::unique_ptr<ButtonAttachment> densitySyncDottedEnabledAttachment;
    std::unique_ptr<SliderAttachment> maxVoicesAttachment;
    std::unique_ptr<ComboBoxAttachment> interpolationQualityAttachment;

    std::array<int, 18> densityRateCodes {};
    int densityRateCodeCount = 0;
    bool isUpdatingDensityRateUI = false;
    
    // Knob animation states for visual hierarchy
    std::map<juce::Slider*, KnobAnimationState> knobAnimationStates;
    
    std::unique_ptr<PresetBrowser> presetBrowser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
