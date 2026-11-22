#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace UIColors
{
    const juce::Colour Bg_Main       = juce::Colour::fromString("#FF2B2B2B");
    const juce::Colour Bg_Panel      = juce::Colour::fromString("#FF333333");
    const juce::Colour Bg_Canvas     = juce::Colour::fromString("#FF1E1E1E");
    const juce::Colour Bg_Control    = juce::Colour::fromString("#FF222222");
    const juce::Colour Bg_Hover      = juce::Colour::fromString("#FF3D3D3D");
    const juce::Colour Border_Dark   = juce::Colour::fromString("#FF181818");
    const juce::Colour Border_Light  = juce::Colour::fromString("#FF4A4A4A");
    const juce::Colour Border_Focus  = juce::Colour::fromString("#FF3BA596");
    const juce::Colour Grid_Line     = juce::Colour::fromString("#FF2A2A2A");
    const juce::Colour Text_Primary  = juce::Colour::fromString("#FFE0E0E0");
    const juce::Colour Text_Secondary= juce::Colour::fromString("#FF9E9E9E");
    const juce::Colour Text_Disabled = juce::Colour::fromString("#FF5A5A5A");
    const juce::Colour Accent_Teal   = juce::Colour::fromString("#FF2D9384");
    const juce::Colour Bar_Fill      = juce::Colour::fromString("#FF265E56");
    const juce::Colour Wave_Post     = juce::Colour::fromString("#FFC8D6D5");
    const juce::Colour GR_Low        = juce::Colour::fromString("#FF45B8AC");
    const juce::Colour GR_Mid        = juce::Colour::fromString("#FFD99B26");
    const juce::Colour GR_High       = juce::Colour::fromString("#FFD9483B");
    const juce::Colour Status_Warn   = juce::Colour::fromString("#FFC94A38");
}

// 水平バーゲージスライダー
class HorizontalBarSlider : public juce::Component,
                            private juce::AudioProcessorParameter::Listener
{
public:
    HorizontalBarSlider(juce::RangedAudioParameter* parameter,
                        const juce::String& labelText,
                        const juce::String& unitSuffix = "");
    ~HorizontalBarSlider() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setEnabledState(bool shouldBeEnabled);
    void setCustomLabel(const juce::String& newLabel) { label = newLabel; repaint(); }
    void updateParameter(juce::RangedAudioParameter* newParam);

private:
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

    juce::String getFormattedValue() const;

    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    juce::String suffix;
    bool isEnabled = true;
    bool isDragging = false;
    float dragStartValue = 0.0f;
    juce::Point<int> dragStartPos;

    std::unique_ptr<juce::TextEditor> inlineEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizontalBarSlider)
};

// メインビジュアライザーコンポーネント
class VisualizerComponent : public juce::Component
{
public:
    VisualizerComponent(SDDuckAudioProcessor& processor);
    ~VisualizerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateFromFifo();
    void setTimebaseIndex(int index);

private:
    void rebuildBackgroundCache();
    juce::Colour getGrColor(float grDb) const;

    SDDuckAudioProcessor& processor;
    juce::Image backgroundCache;

    // 波形描画履歴リングバッファ
    struct HistoryPoint
    {
        float prePeak = 0.0f;
        float postPeak = 0.0f;
        float minGain = 1.0f;
    };

    std::vector<HistoryPoint> history;
    size_t historyIndex = 0;
    int maxHistoryPoints = 2048;

    float currentMaxGrDb = 0.0f;
    int timebaseIdx = 0; // 0: 1 Beat, 1: 2 Beats, 2: 1 Bar, 3: 500 ms, 4: 1000 ms

    int silenceFrames = 0;
    bool isFrozen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerComponent)
};

// プラグインEditor本体
class SDDuckAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit SDDuckAudioProcessorEditor(SDDuckAudioProcessor&);
    ~SDDuckAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModeDependencies();

    SDDuckAudioProcessor& audioProcessor;

    // Top Bar
    juce::TextButton btnModeSync { "SYNC" };
    juce::TextButton btnModeAudio { "AUDIO" };
    juce::TextButton btnModeMidi { "MIDI" };

    juce::ComboBox comboSyncRate;
    juce::ComboBox comboScDetect;
    juce::ComboBox comboMidiNote;

    juce::Label lblScStatus;
    juce::TextButton btnScListen { "MON" };

    // Visualizer
    VisualizerComponent visualizer;
    juce::ComboBox comboTimebase;
    juce::Label lblMaxGr;

    // Control Panel Sliders
    std::unique_ptr<HorizontalBarSlider> sliderDepth;
    std::unique_ptr<HorizontalBarSlider> sliderAttack;
    std::unique_ptr<HorizontalBarSlider> sliderHold;
    std::unique_ptr<HorizontalBarSlider> sliderRelease;
    std::unique_ptr<HorizontalBarSlider> sliderCurve;
    std::unique_ptr<HorizontalBarSlider> sliderMix;
    std::unique_ptr<HorizontalBarSlider> sliderThreshOrVel;
    std::unique_ptr<HorizontalBarSlider> sliderScHpf;
    std::unique_ptr<HorizontalBarSlider> sliderScLpf;

    // Split 複合セル
    juce::TextButton btnSplitToggle { "OFF" };
    std::unique_ptr<HorizontalBarSlider> sliderSplitFreq;

    // Target (Stereo / Mid Only) ボタン
    juce::TextButton btnTargetChannel { "Stereo" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDDuckAudioProcessorEditor)
};
