#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

struct ThemeColors
{
    bool isLight = false;

    juce::Colour bgMain() const       { return isLight ? juce::Colour::fromString("#FFEAEAEA") : juce::Colour::fromString("#FF2B2B2B"); }
    juce::Colour bgPanel() const      { return isLight ? juce::Colour::fromString("#FFDFDFDF") : juce::Colour::fromString("#FF333333"); }
    juce::Colour bgCanvas() const     { return isLight ? juce::Colour::fromString("#FFF5F5F5") : juce::Colour::fromString("#FF1E1E1E"); }
    juce::Colour bgControl() const    { return isLight ? juce::Colour::fromString("#FFE2E2E2") : juce::Colour::fromString("#FF222222"); }
    juce::Colour bgHover() const      { return isLight ? juce::Colour::fromString("#FFD4D4D4") : juce::Colour::fromString("#FF3D3D3D"); }
    juce::Colour borderDark() const   { return isLight ? juce::Colour::fromString("#FFB8B8B8") : juce::Colour::fromString("#FF181818"); }
    juce::Colour borderLight() const  { return isLight ? juce::Colour::fromString("#FFC8C8C8") : juce::Colour::fromString("#FF4A4A4A"); }
    juce::Colour borderFocus() const  { return isLight ? juce::Colour::fromString("#FF2D9384") : juce::Colour::fromString("#FF3BA596"); }
    juce::Colour gridLine() const     { return isLight ? juce::Colour::fromString("#FFE0E0E0") : juce::Colour::fromString("#FF2A2A2A"); }
    juce::Colour textPrimary() const  { return isLight ? juce::Colour::fromString("#FF1A1A1A") : juce::Colour::fromString("#FFF0F0F0"); }
    juce::Colour textSecondary() const{ return isLight ? juce::Colour::fromString("#FF555555") : juce::Colour::fromString("#FFB8B8B8"); }
    juce::Colour textDisabled() const { return isLight ? juce::Colour::fromString("#FF888888") : juce::Colour::fromString("#FF787878"); }
    juce::Colour gridLabel() const    { return isLight ? juce::Colour::fromString("#FF666666") : juce::Colour::fromString("#FF8A9292"); }
    juce::Colour accentTeal() const   { return isLight ? juce::Colour::fromString("#FF228274") : juce::Colour::fromString("#FF2D9384"); }
    juce::Colour barFill() const      { return isLight ? juce::Colour::fromString("#FF88C2B8") : juce::Colour::fromString("#FF265E56"); }
    juce::Colour wavePost() const     { return isLight ? juce::Colour::fromString("#FF4F6765") : juce::Colour::fromString("#FFC8D6D5"); }
    juce::Colour grLow() const        { return isLight ? juce::Colour::fromString("#FF278D82") : juce::Colour::fromString("#FF45B8AC"); }
    juce::Colour grMid() const        { return isLight ? juce::Colour::fromString("#FFBD7A11") : juce::Colour::fromString("#FFD99B26"); }
    juce::Colour grHigh() const       { return isLight ? juce::Colour::fromString("#FFB83126") : juce::Colour::fromString("#FFD9483B"); }
    juce::Colour statusWarn() const   { return isLight ? juce::Colour::fromString("#FFC03222") : juce::Colour::fromString("#FFC94A38"); }
};

// フラット直角 (角丸0px) カスタム LookAndFeel
class SDDuckLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SDDuckLookAndFeel(const ThemeColors& t) : theme(t)
    {
        applyThemeColours();
    }

    void setTheme(const ThemeColors& t)
    {
        theme = t;
        applyThemeColours();
    }

    void applyThemeColours()
    {
        setColour(juce::ComboBox::backgroundColourId, theme.bgControl());
        setColour(juce::ComboBox::textColourId, theme.textPrimary());
        setColour(juce::ComboBox::outlineColourId, theme.borderDark());
        setColour(juce::PopupMenu::backgroundColourId, theme.bgControl());
        setColour(juce::PopupMenu::textColourId, theme.textPrimary());
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme.accentTeal());
        setColour(juce::PopupMenu::highlightedTextColourId, theme.textPrimary());
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

private:
    ThemeColors theme;
};

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
    void setTheme(const ThemeColors& newTheme) { theme = newTheme; repaint(); }

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

    ThemeColors theme;
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
    SDDuckLookAndFeel lookAndFeel { currentTheme };

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
