#pragma once

#include "PluginProcessor.h"
#include "ui/RetroLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// SD-Verb と共通のパレットを使うテーマ色。旧 ThemeColors のメソッド名を残し、
// 返値は sdduck::ColorPalette への対応表で決める。
//   bgMain / bgPanel / bgCanvas / bgControl -> baseBg
//   bgHover / borderLight / gridLine / textDisabled -> subDecor
//   borderDark / textPrimary / textSecondary / gridLabel / wavePost -> textMain
//   borderFocus / accent / barFill / grLow -> mainAccent
//   grMid -> 固定 0xFFD99B26、grHigh -> 固定 0xFFD9483B（Verb パレット外の固定色）
//   statusWarn -> ライト 0xFFC03222 / ダーク 0xFFC94A38
struct ThemeColors
{
    sdduck::ThemeId themeId = sdduck::ThemeId::DarkOrange;
    sdduck::ColorPalette palette = sdduck::getPalette(themeId);

    void setThemeId(sdduck::ThemeId id)
    {
        themeId = id;
        palette = sdduck::getPalette(id);
    }

    bool isLight() const noexcept { return (static_cast<int>(themeId) & 1) == 0; }
    bool isCyan() const noexcept  { return (static_cast<int>(themeId) & 2) != 0; }

    juce::Colour bgMain() const       { return palette.baseBg; }
    juce::Colour bgPanel() const      { return palette.baseBg; }
    juce::Colour bgCanvas() const     { return palette.baseBg; }
    juce::Colour bgControl() const    { return palette.baseBg; }
    juce::Colour bgHover() const      { return palette.subDecor; }
    juce::Colour borderDark() const   { return palette.textMain; }
    juce::Colour borderLight() const  { return palette.subDecor; }
    juce::Colour borderFocus() const  { return palette.mainAccent; }
    juce::Colour gridLine() const     { return palette.subDecor; }
    juce::Colour textPrimary() const  { return palette.textMain; }
    juce::Colour textSecondary() const{ return palette.textMain; }
    juce::Colour textDisabled() const { return palette.subDecor; }
    juce::Colour gridLabel() const    { return palette.textMain; }
    juce::Colour accent() const       { return palette.mainAccent; }
    juce::Colour barFill() const      { return palette.mainAccent; }
    juce::Colour wavePost() const     { return palette.textMain; }
    juce::Colour grLow() const        { return palette.mainAccent; }
    juce::Colour grMid() const        { return juce::Colour(0xFFD99B26); }
    juce::Colour grHigh() const       { return juce::Colour(0xFFD9483B); }
    juce::Colour statusWarn() const   { return isLight() ? juce::Colour(0xFFC03222) : juce::Colour(0xFFC94A38); }
};

// 水平バーゲージスライダー
class HorizontalBarSlider : public juce::Component,
                            private juce::AudioProcessorParameter::Listener
{
public:
    HorizontalBarSlider(juce::RangedAudioParameter* parameter,
                        const juce::String& labelText,
                        const juce::String& unitSuffix = "",
                        const sdduck::RetroLookAndFeel* lookAndFeel = nullptr);
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
    void setTheme(const ThemeColors& newTheme) { theme = newTheme; repaint(); }

private:
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {}

    juce::String getFormattedValue() const;
    juce::FontOptions getHeavyFontOptions(float height) const;
    juce::FontOptions getNumericFontOptions(float height) const;

    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    juce::String suffix;
    bool isEnabled = true;
    bool isDragging = false;
    float dragStartValue = 0.0f;
    juce::Point<int> dragStartPos;

    ThemeColors theme;
    const sdduck::RetroLookAndFeel* lookAndFeel = nullptr;
    std::unique_ptr<juce::TextEditor> inlineEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HorizontalBarSlider)
};

// メインビジュアライザーコンポーネント
class VisualizerComponent : public juce::Component
{
public:
    VisualizerComponent(SDDuckAudioProcessor& processor, const sdduck::RetroLookAndFeel& laf);
    ~VisualizerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateFromFifo();
    void setTimebaseIndex(int index);
    float getCurrentMaxGrDb() const noexcept { return peakHoldGrDb; }
    void setTheme(const ThemeColors& newTheme)
    {
        theme = newTheme;
        rebuildBackgroundCache();
        repaint();
    }

private:
    void rebuildBackgroundCache();
    juce::Colour getGrColor(float grDb) const;

    SDDuckAudioProcessor& processor;
    const sdduck::RetroLookAndFeel* lookAndFeel = nullptr;
    juce::Image backgroundCache;
    ThemeColors theme;

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
    float peakHoldGrDb = 0.0f;
    int peakHoldFrames = 0;
    int timebaseIdx = 0; // 0: 1 Beat, 1: 2 Beats, 2: 1 Bar, 3: 500 ms, 4: 1000 ms

    // 描画座標キャッシュ (ヒープ確保排除)
    struct PointCoord
    {
        float x = 0.0f;
        float yPre = 0.0f;
        float yPost = 0.0f;
        float yGr = 0.0f;
        float grDb = 0.0f;
    };
    std::vector<PointCoord> coordsCache;

    int silenceFrames = 0;
    bool isFrozen = false;
    double lastBpm = 120.0;

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
    void applyTheme();

    SDDuckAudioProcessor& audioProcessor;
    ThemeColors currentTheme;
    sdduck::RetroLookAndFeel lookAndFeel;

    // Top Bar
    juce::TextButton btnModeSync { "SYNC" };
    juce::TextButton btnModeAudio { "AUDIO" };
    juce::TextButton btnModeMidi { "MIDI" };

    juce::ComboBox comboSyncRate;
    juce::ComboBox comboScDetect;
    juce::ComboBox comboMidiNote;

    juce::Label lblScStatus;
    juce::TextButton btnScListen { "MON" };
    juce::TextButton btnThemeToggle { "LIGHT" };
    juce::TextButton btnColorTheme { "CYN" };

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
