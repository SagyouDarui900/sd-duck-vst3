#include "PluginEditor.h"
#include <iomanip>
#include <sstream>

// ==============================================================================
// HorizontalBarSlider 実装
// ==============================================================================
HorizontalBarSlider::HorizontalBarSlider(juce::RangedAudioParameter* parameter,
                                         const juce::String& labelText,
                                         const juce::String& unitSuffix)
    : param(parameter), label(labelText), suffix(unitSuffix)
{
    if (param != nullptr)
        param->addListener(this);
}

HorizontalBarSlider::~HorizontalBarSlider()
{
    if (param != nullptr)
        param->removeListener(this);
}

void HorizontalBarSlider::updateParameter(juce::RangedAudioParameter* newParam)
{
    if (param != nullptr)
        param->removeListener(this);

    param = newParam;

    if (param != nullptr)
        param->addListener(this);

    repaint();
}

void HorizontalBarSlider::setEnabledState(bool shouldBeEnabled)
{
    if (isEnabled != shouldBeEnabled)
    {
        isEnabled = shouldBeEnabled;
        repaint();
    }
}

void HorizontalBarSlider::parameterValueChanged(int, float)
{
    juce::MessageManager::callAsync([this]() { repaint(); });
}

juce::String HorizontalBarSlider::getFormattedValue() const
{
    if (param == nullptr)
        return {};

    const float val = param->convertFrom0to1(param->getValue());

    if (suffix == "dB")
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (val >= 0.0f ? "+" : "") << val << " dB";
        return juce::String(ss.str());
    }
    else if (suffix == "ms")
    {
        std::ostringstream ss;
        if (val < 100.0f)
            ss << std::fixed << std::setprecision(1) << val << " ms";
        else
            ss << static_cast<int>(std::round(val)) << " ms";
        return juce::String(ss.str());
    }
    else if (suffix == "%")
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << val << " %";
        return juce::String(ss.str());
    }
    else if (suffix == "Hz")
    {
        return juce::String(static_cast<int>(std::round(val))) + " Hz";
    }
    else if (label == "Curve")
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (val >= 0.0f ? "+" : "") << val;
        return juce::String(ss.str());
    }

    return param->getCurrentValueAsText() + " " + suffix;
}

void HorizontalBarSlider::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // 背景
    g.setColour(theme.bgControl());
    g.fillRect(bounds);

    // プログレスバー塗りつぶし (有効時のみ)
    if (isEnabled && param != nullptr)
    {
        const float normVal = param->getValue();
        const int fillWidth = static_cast<int>(std::round(bounds.getWidth() * normVal));
        if (fillWidth > 0)
        {
            g.setColour(theme.barFill());
            g.fillRect(bounds.withWidth(fillWidth));
        }
    }

    // 枠線
    g.setColour(isDragging ? theme.borderFocus() : theme.borderDark());
    g.drawRect(bounds, 1);

    // テキスト描画 (左: ラベル, 右: 数値)
    const int pad = 8;
    auto textArea = bounds.reduced(pad, 0);

    // ラベル (Segoe UI 12.5px Bold で高視認性)
    g.setFont(juce::FontOptions("Segoe UI", 12.5f, juce::Font::bold));
    g.setColour(isEnabled ? theme.textSecondary() : theme.textDisabled());
    g.drawText(label, textArea, juce::Justification::centredLeft, true);

    // 数値 (Consolas 12.5px 等幅)
    if (isEnabled && param != nullptr)
    {
        g.setFont(juce::FontOptions("Consolas", 12.5f, juce::Font::plain));
        g.setColour(theme.textPrimary());
        g.drawText(getFormattedValue(), textArea, juce::Justification::centredRight, true);
    }
}

void HorizontalBarSlider::resized()
{
    if (inlineEditor != nullptr)
        inlineEditor->setBounds(getLocalBounds().reduced(2));
}

void HorizontalBarSlider::mouseDown(const juce::MouseEvent& e)
{
    if (!isEnabled || param == nullptr)
        return;

    if (e.mods.isRightButtonDown())
    {
        inlineEditor = std::make_unique<juce::TextEditor>();
        inlineEditor->setBounds(getLocalBounds().reduced(2));
        inlineEditor->setFont(juce::FontOptions("Consolas", 12.0f, juce::Font::plain));
        inlineEditor->setColour(juce::TextEditor::backgroundColourId, theme.bgCanvas());
        inlineEditor->setColour(juce::TextEditor::textColourId, theme.textPrimary());
        inlineEditor->setColour(juce::TextEditor::outlineColourId, theme.borderFocus());

        const float val = param->convertFrom0to1(param->getValue());
        inlineEditor->setText(juce::String(val, 2), false);
        inlineEditor->selectAll();

        inlineEditor->onReturnKey = [this]()
        {
            const float parsed = inlineEditor->getText().getFloatValue();
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(parsed));
            param->endChangeGesture();
            inlineEditor.reset();
            repaint();
        };

        inlineEditor->onEscapeKey = [this]()
        {
            inlineEditor.reset();
            repaint();
        };

        inlineEditor->onFocusLost = [this]()
        {
            inlineEditor.reset();
            repaint();
        };

        addAndMakeVisible(inlineEditor.get());
        inlineEditor->grabKeyboardFocus();
        return;
    }

    if (e.mods.isCtrlDown())
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
        repaint();
        return;
    }

    isDragging = true;
    dragStartValue = param->getValue();
    dragStartPos = e.getPosition();
    param->beginChangeGesture();
    repaint();
}

void HorizontalBarSlider::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging || param == nullptr)
        return;

    const int deltaX = e.getPosition().x - dragStartPos.x;
    const int deltaY = dragStartPos.y - e.getPosition().y;

    // より大きく動かした軸（XまたはY）を採用し、斜め操作時の相殺を防止
    const int effectiveDelta = (std::abs(deltaX) >= std::abs(deltaY)) ? deltaX : deltaY;

    const float delta = static_cast<float>(effectiveDelta) / (e.mods.isShiftDown() ? 1500.0f : 200.0f);
    const float newVal = juce::jlimit(0.0f, 1.0f, dragStartValue + delta);

    param->setValueNotifyingHost(newVal);
    repaint();
}

void HorizontalBarSlider::mouseUp(const juce::MouseEvent&)
{
    if (isDragging && param != nullptr)
    {
        isDragging = false;
        param->endChangeGesture();
        repaint();
    }
}

void HorizontalBarSlider::mouseDoubleClick(const juce::MouseEvent&)
{
    if (!isEnabled || param == nullptr)
        return;

    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getDefaultValue());
    param->endChangeGesture();
    repaint();
}

void HorizontalBarSlider::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (!isEnabled || param == nullptr)
        return;

    const float step = (wheel.isReversed ? -1.0f : 1.0f) * (wheel.deltaY > 0 ? 0.02f : -0.02f);
    const float current = param->getValue();
    param->beginChangeGesture();
    param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, current + step));
    param->endChangeGesture();
    repaint();
}

// ==============================================================================
// VisualizerComponent 実装
// ==============================================================================
VisualizerComponent::VisualizerComponent(SDDuckAudioProcessor& p)
    : processor(p)
{
    history.resize(maxHistoryPoints);
}

void VisualizerComponent::setTimebaseIndex(int index)
{
    timebaseIdx = index;
    rebuildBackgroundCache();
    repaint();
}

void VisualizerComponent::rebuildBackgroundCache()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    backgroundCache = juce::Image(juce::Image::RGB, w, h, true);
    juce::Graphics g(backgroundCache);

    g.setColour(theme.bgCanvas());
    g.fillRect(0, 0, w, h);

    const float dbLevels[] = { 0.0f, -6.0f, -12.0f, -24.0f };
    g.setFont(juce::FontOptions("Consolas", 10.0f, juce::Font::plain));

    for (float db : dbLevels)
    {
        const float norm = -db / 24.0f;
        const int y = static_cast<int>(std::round(norm * (h - 1)));

        g.setColour(theme.gridLine());
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(w));

        g.setColour(theme.textDisabled());
        juce::String text = juce::String(static_cast<int>(db)) + "dB";
        g.drawText(text, 6, std::max(0, y - 12), 40, 12, juce::Justification::bottomLeft);
    }

    g.setColour(theme.gridLine());
    constexpr int numVerticalLines = 8;
    for (int i = 1; i < numVerticalLines; ++i)
    {
        const int x = (w * i) / numVerticalLines;
        g.drawVerticalLine(x, 0.0f, static_cast<float>(h));
    }
}

void VisualizerComponent::resized()
{
    rebuildBackgroundCache();
}

juce::Colour VisualizerComponent::getGrColor(float grDb) const
{
    if (grDb >= -6.0f)
    {
        return theme.grLow();
    }
    else if (grDb >= -15.0f)
    {
        const float t = (grDb - (-6.0f)) / (-9.0f);
        return theme.grLow().interpolatedWith(theme.grMid(), juce::jlimit(0.0f, 1.0f, t));
    }
    else
    {
        const float t = (grDb - (-15.0f)) / (-9.0f);
        return theme.grMid().interpolatedWith(theme.grHigh(), juce::jlimit(0.0f, 1.0f, t));
    }
}

void VisualizerComponent::updateFromFifo()
{
    auto& fifo = processor.getVisualFifo();
    VisualPacket packet;
    bool hasNewData = false;

    float minGainFound = 1.0f;

    while (fifo.pop(packet))
    {
        hasNewData = true;
        history[historyIndex].prePeak = packet.prePeak;
        history[historyIndex].postPeak = packet.postPeak;
        history[historyIndex].minGain = packet.minGain;

        minGainFound = std::min(minGainFound, packet.minGain);

        historyIndex = (historyIndex + 1) % maxHistoryPoints;

        if (packet.prePeak < 0.0001f && !packet.isPlaying)
            silenceFrames++;
        else
            silenceFrames = 0;
    }

    if (minGainFound < 0.999f)
    {
        const float grDb = 20.0f * std::log10(std::max(0.00001f, minGainFound));
        currentMaxGrDb = grDb;

        if (grDb < peakHoldGrDb)
        {
            peakHoldGrDb = grDb;
            peakHoldFrames = 45;
        }
    }
    else
    {
        currentMaxGrDb = 0.0f;
    }

    if (peakHoldFrames > 0)
    {
        peakHoldFrames--;
    }
    else
    {
        peakHoldGrDb = std::min(0.0f, peakHoldGrDb + 0.4f);
    }

    if (silenceFrames > 60)
    {
        isFrozen = true;
        return;
    }

    isFrozen = false;
    if (hasNewData)
        repaint();
}

void VisualizerComponent::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    if (backgroundCache.isValid())
        g.drawImageAt(backgroundCache, 0, 0);

    const int pointsToShow = juce::jlimit(50, maxHistoryPoints, w);

    juce::Path prePath;
    juce::Path postPath;
    juce::Path grPath;
    juce::Path diffAreaPath;

    bool firstPoint = true;

    for (int i = 0; i < pointsToShow; ++i)
    {
        const int idx = (static_cast<int>(historyIndex) - pointsToShow + i + maxHistoryPoints) % maxHistoryPoints;
        const auto& pt = history[idx];

        const float x = (static_cast<float>(i) / static_cast<float>(pointsToShow - 1)) * static_cast<float>(w);

        const float grDb = (pt.minGain > 0.00001f) ? (20.0f * std::log10(pt.minGain)) : -100.0f;
        const float grNorm = juce::jlimit(0.0f, 1.0f, -grDb / 24.0f);
        const float grY = grNorm * static_cast<float>(h);

        const float postAmp = juce::jlimit(0.0f, 1.0f, pt.postPeak);
        const float postY = static_cast<float>(h) - (postAmp * static_cast<float>(h) * 0.85f);

        if (firstPoint)
        {
            grPath.startNewSubPath(x, grY);
            postPath.startNewSubPath(x, static_cast<float>(h));
            postPath.lineTo(x, postY);

            diffAreaPath.startNewSubPath(x, 0.0f);
            diffAreaPath.lineTo(x, grY);
            firstPoint = false;
        }
        else
        {
            grPath.lineTo(x, grY);
            postPath.lineTo(x, postY);
            diffAreaPath.lineTo(x, grY);
        }
    }

    if (!firstPoint)
    {
        diffAreaPath.lineTo(static_cast<float>(w), 0.0f);
        diffAreaPath.closeSubPath();

        const juce::Colour fillColor = getGrColor(peakHoldGrDb).withAlpha(theme.isLight ? 0.40f : 0.35f);
        g.setColour(fillColor);
        g.fillPath(diffAreaPath);
    }

    postPath.lineTo(static_cast<float>(w), static_cast<float>(h));
    postPath.closeSubPath();
    g.setColour(theme.wavePost().withAlpha(theme.isLight ? 0.35f : 0.25f));
    g.fillPath(postPath);
    g.setColour(theme.wavePost());
    g.strokePath(postPath, juce::PathStrokeType(1.0f));

    g.setColour(getGrColor(peakHoldGrDb));
    g.strokePath(grPath, juce::PathStrokeType(1.5f));

    g.setColour(theme.borderDark());
    g.drawRect(getLocalBounds(), 1);
}

// ==============================================================================
// SDDuckAudioProcessorEditor 実装
// ==============================================================================
SDDuckAudioProcessorEditor::SDDuckAudioProcessorEditor(SDDuckAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    auto& apvts = audioProcessor.getAPVTS();

    auto setupModeButton = [this](juce::TextButton& btn, int modeValue)
    {
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(1001);
        btn.onClick = [this, modeValue]()
        {
            if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramMode))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(modeValue)));
                param->endChangeGesture();
            }
            updateModeDependencies();
        };
        addAndMakeVisible(btn);
    };

    setupModeButton(btnModeSync, 0);
    setupModeButton(btnModeAudio, 1);
    setupModeButton(btnModeMidi, 2);

    const int initialMode = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMode));
    if (initialMode == 0) btnModeSync.setToggleState(true, juce::dontSendNotification);
    else if (initialMode == 1) btnModeAudio.setToggleState(true, juce::dontSendNotification);
    else btnModeMidi.setToggleState(true, juce::dontSendNotification);

    comboSyncRate.addItemList({ "Rate: 1/1", "Rate: 1/2", "Rate: 1/4", "Rate: 1/8", "Rate: 1/16", "Rate: 1/4T", "Rate: 1/8D" }, 1);
    comboSyncRate.setSelectedId(static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramSyncRate)) + 1, juce::dontSendNotification);
    comboSyncRate.onChange = [this]()
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramSyncRate))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(comboSyncRate.getSelectedId() - 1)));
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(comboSyncRate);

    comboScDetect.addItemList({ "Detect: Follow", "Detect: One-Shot" }, 1);
    comboScDetect.setSelectedId(static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramScDetect)) + 1, juce::dontSendNotification);
    comboScDetect.onChange = [this]()
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramScDetect))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(comboScDetect.getSelectedId() - 1)));
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(comboScDetect);

    comboMidiNote.addItem("Note: Any", 1);
    for (int note = 0; note < 128; ++note)
    {
        comboMidiNote.addItem("Note: " + juce::MidiMessage::getMidiNoteName(note, true, true, 3) + " (" + juce::String(note) + ")", note + 2);
    }
    comboMidiNote.setSelectedId(static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMidiNote)) + 1, juce::dontSendNotification);
    comboMidiNote.onChange = [this]()
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramMidiNote))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(comboMidiNote.getSelectedId() - 1)));
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(comboMidiNote);

    lblScStatus.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
    lblScStatus.setText("SC: ACTIVE", juce::dontSendNotification);
    addAndMakeVisible(lblScStatus);

    btnScListen.setClickingTogglesState(true);
    btnScListen.onClick = [this]()
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramScListen))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(btnScListen.getToggleState() ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(btnScListen);

    // テーマ切り替えボタン
    btnThemeToggle.onClick = [this]()
    {
        currentTheme.isLight = !currentTheme.isLight;
        btnThemeToggle.setButtonText(currentTheme.isLight ? "DARK" : "LIGHT");
        applyTheme();
    };
    addAndMakeVisible(btnThemeToggle);

    addAndMakeVisible(visualizer);

    comboTimebase.addItemList({ "View: 1 Beat", "View: 2 Beats", "View: 1 Bar", "View: 500 ms", "View: 1000 ms" }, 1);
    comboTimebase.setSelectedId(1, juce::dontSendNotification);
    comboTimebase.onChange = [this]()
    {
        visualizer.setTimebaseIndex(comboTimebase.getSelectedId() - 1);
    };
    addAndMakeVisible(comboTimebase);

    lblMaxGr.setFont(juce::FontOptions("Consolas", 11.0f, juce::Font::plain));
    lblMaxGr.setText("Max GR:  0.0 dB", juce::dontSendNotification);
    lblMaxGr.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblMaxGr);

    sliderDepth   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramDepth), "Depth", "%");
    sliderAttack  = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramAttack), "Attack", "ms");
    sliderHold    = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramHold), "Hold", "ms");
    sliderRelease = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramRelease), "Release", "ms");
    sliderCurve   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramCurve), "Curve", "");
    sliderMix     = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramMix), "Mix", "%");

    sliderThreshOrVel = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramThreshold), "Threshold", "dB");
    sliderScHpf   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramScHpf), "SC HPF", "Hz");
    sliderScLpf   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramScLpf), "SC LPF", "Hz");

    addAndMakeVisible(sliderDepth.get());
    addAndMakeVisible(sliderAttack.get());
    addAndMakeVisible(sliderHold.get());
    addAndMakeVisible(sliderRelease.get());
    addAndMakeVisible(sliderCurve.get());
    addAndMakeVisible(sliderMix.get());
    addAndMakeVisible(sliderThreshOrVel.get());
    addAndMakeVisible(sliderScHpf.get());
    addAndMakeVisible(sliderScLpf.get());

    btnSplitToggle.setClickingTogglesState(true);
    const bool initialSplit = *apvts.getRawParameterValue(SDDuckAudioProcessor::paramSplitEnabled) > 0.5f;
    btnSplitToggle.setToggleState(initialSplit, juce::dontSendNotification);
    btnSplitToggle.setButtonText(initialSplit ? "ON" : "OFF");
    btnSplitToggle.onClick = [this]()
    {
        const bool active = btnSplitToggle.getToggleState();
        btnSplitToggle.setButtonText(active ? "ON" : "OFF");
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramSplitEnabled))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(active ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(btnSplitToggle);

    sliderSplitFreq = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramSplitFreq), "Low Split", "Hz");
    addAndMakeVisible(sliderSplitFreq.get());

    const int initialTarget = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramChannelMode));
    btnTargetChannel.setButtonText(initialTarget == 0 ? "Target: Stereo" : "Target: Mid Only");
    btnTargetChannel.onClick = [this]()
    {
        auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramChannelMode);
        if (param == nullptr) return;

        const int current = static_cast<int>(param->convertFrom0to1(param->getValue()));
        const int next = (current + 1) % 2;
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
        param->endChangeGesture();
        btnTargetChannel.setButtonText(next == 0 ? "Target: Stereo" : "Target: Mid Only");
    };
    addAndMakeVisible(btnTargetChannel);

    updateModeDependencies();
    applyTheme();

    setSize(680, 380);
    setResizable(true, true);
    setResizeLimits(500, 300, 1920, 1080);

    startTimerHz(60);
}

SDDuckAudioProcessorEditor::~SDDuckAudioProcessorEditor()
{
    stopTimer();
}

void SDDuckAudioProcessorEditor::applyTheme()
{
    // 各コンポーネントへテーマ色を伝達
    visualizer.setTheme(currentTheme);

    auto updateSliderTheme = [this](HorizontalBarSlider* s)
    {
        if (s != nullptr)
            s->setTheme(currentTheme);
    };

    updateSliderTheme(sliderDepth.get());
    updateSliderTheme(sliderAttack.get());
    updateSliderTheme(sliderHold.get());
    updateSliderTheme(sliderRelease.get());
    updateSliderTheme(sliderCurve.get());
    updateSliderTheme(sliderMix.get());
    updateSliderTheme(sliderThreshOrVel.get());
    updateSliderTheme(sliderScHpf.get());
    updateSliderTheme(sliderScLpf.get());
    updateSliderTheme(sliderSplitFreq.get());

    auto applyBtnStyle = [this](juce::TextButton& btn)
    {
        btn.setColour(juce::TextButton::buttonColourId, currentTheme.bgPanel());
        btn.setColour(juce::TextButton::buttonOnColourId, currentTheme.accentTeal());
        btn.setColour(juce::TextButton::textColourOffId, currentTheme.textSecondary());
        btn.setColour(juce::TextButton::textColourOnId, currentTheme.textPrimary());
    };

    applyBtnStyle(btnModeSync);
    applyBtnStyle(btnModeAudio);
    applyBtnStyle(btnModeMidi);
    applyBtnStyle(btnScListen);
    applyBtnStyle(btnThemeToggle);

    btnSplitToggle.setColour(juce::TextButton::buttonColourId, currentTheme.bgControl());
    btnSplitToggle.setColour(juce::TextButton::buttonOnColourId, currentTheme.accentTeal());
    btnSplitToggle.setColour(juce::TextButton::textColourOffId, currentTheme.textSecondary());
    btnSplitToggle.setColour(juce::TextButton::textColourOnId, currentTheme.textPrimary());

    btnTargetChannel.setColour(juce::TextButton::buttonColourId, currentTheme.bgControl());
    btnTargetChannel.setColour(juce::TextButton::textColourOffId, currentTheme.textPrimary());

    lblMaxGr.setColour(juce::Label::backgroundColourId, currentTheme.bgCanvas().withAlpha(0.85f));
    lblMaxGr.setColour(juce::Label::textColourId, currentTheme.textPrimary());
    lblMaxGr.setColour(juce::Label::outlineColourId, currentTheme.borderDark());

    repaint();
}

void SDDuckAudioProcessorEditor::updateModeDependencies()
{
    auto& apvts = audioProcessor.getAPVTS();
    const int mode = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMode));

    btnModeSync.setToggleState(mode == 0, juce::dontSendNotification);
    btnModeAudio.setToggleState(mode == 1, juce::dontSendNotification);
    btnModeMidi.setToggleState(mode == 2, juce::dontSendNotification);

    comboSyncRate.setVisible(mode == 0);
    comboScDetect.setVisible(mode == 1);
    comboMidiNote.setVisible(mode == 2);

    if (mode == 0) // Sync
    {
        sliderThreshOrVel->setEnabledState(false);
        sliderThreshOrVel->setCustomLabel("Threshold");
        sliderScHpf->setEnabledState(false);
        sliderScLpf->setEnabledState(false);
    }
    else if (mode == 1) // Audio
    {
        sliderThreshOrVel->setEnabledState(true);
        sliderThreshOrVel->setCustomLabel("Threshold");
        sliderThreshOrVel->updateParameter(apvts.getParameter(SDDuckAudioProcessor::paramThreshold));
        sliderScHpf->setEnabledState(true);
        sliderScLpf->setEnabledState(true);
    }
    else // MIDI
    {
        sliderThreshOrVel->setEnabledState(true);
        sliderThreshOrVel->setCustomLabel("Vel Sens");
        sliderThreshOrVel->updateParameter(apvts.getParameter(SDDuckAudioProcessor::paramMidiVelSens));
        sliderScHpf->setEnabledState(false);
        sliderScLpf->setEnabledState(false);
    }
}

void SDDuckAudioProcessorEditor::timerCallback()
{
    visualizer.updateFromFifo();

    const float maxGrDb = visualizer.getCurrentMaxGrDb();
    std::ostringstream ss;
    ss << "Max GR: ";
    if (maxGrDb > -0.05f)
        ss << " 0.0 dB";
    else
        ss << std::fixed << std::setprecision(1) << maxGrDb << " dB";
    lblMaxGr.setText(ss.str(), juce::dontSendNotification);

    auto& apvts = audioProcessor.getAPVTS();
    const int mode = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMode));
    const bool scListen = *apvts.getRawParameterValue(SDDuckAudioProcessor::paramScListen) > 0.5f;
    btnScListen.setToggleState(scListen, juce::dontSendNotification);

    if (mode == 1) // Audioモード
    {
        const bool scActive = audioProcessor.getIsSidechainActive();
        const float scPeak = audioProcessor.getLastSidechainPeakDb();
        if (scActive)
        {
            std::ostringstream scSs;
            scSs << "SC: ACTIVE (" << std::fixed << std::setprecision(1) << scPeak << "dB)";
            lblScStatus.setText(scSs.str(), juce::dontSendNotification);
            lblScStatus.setColour(juce::Label::textColourId, currentTheme.accentTeal());
        }
        else
        {
            lblScStatus.setText("SC: NO SIGNAL", juce::dontSendNotification);
            lblScStatus.setColour(juce::Label::textColourId, currentTheme.statusWarn());
        }
    }
    else
    {
        lblScStatus.setText("SC: READY", juce::dontSendNotification);
        lblScStatus.setColour(juce::Label::textColourId, currentTheme.textSecondary());
    }

    const int currentTarget = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramChannelMode));
    btnTargetChannel.setButtonText(currentTarget == 0 ? "Target: Stereo" : "Target: Mid Only");
}

void SDDuckAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(currentTheme.bgMain());
}

void SDDuckAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    constexpr int topBarHeight = 28;
    constexpr int controlPanelHeight = 110;
    const int visualizerHeight = h - topBarHeight - controlPanelHeight;

    // --- Top Bar (28px) ---
    // 左: モード切替タブ (SYNC / AUDIO / MIDI)
    constexpr int btnWidth = 55;
    btnModeSync.setBounds(0, 0, btnWidth, topBarHeight);
    btnModeAudio.setBounds(btnWidth, 0, btnWidth, topBarHeight);
    btnModeMidi.setBounds(btnWidth * 2, 0, btnWidth, topBarHeight);

    // 中央: モード別設定 (Rate / Detect / Note)
    const int centerWidth = 140;
    const int centerX = (w - centerWidth) / 2;
    comboSyncRate.setBounds(centerX, 2, centerWidth, topBarHeight - 4);
    comboScDetect.setBounds(centerX, 2, centerWidth, topBarHeight - 4);
    comboMidiNote.setBounds(centerX, 2, centerWidth, topBarHeight - 4);

    // 右: SC Status & SC MON & THEME
    constexpr int themeBtnWidth = 50;
    constexpr int monBtnWidth = 45;
    btnThemeToggle.setBounds(w - themeBtnWidth, 0, themeBtnWidth, topBarHeight);
    btnScListen.setBounds(w - themeBtnWidth - monBtnWidth, 0, monBtnWidth, topBarHeight);
    lblScStatus.setBounds(w - themeBtnWidth - monBtnWidth - 140, 0, 135, topBarHeight);

    // --- Visualizer ---
    visualizer.setBounds(0, topBarHeight, w, visualizerHeight);

    comboTimebase.setBounds(8, topBarHeight + visualizerHeight - 26, 110, 20);
    lblMaxGr.setBounds(w - 130, topBarHeight + visualizerHeight - 26, 122, 20);

    // --- Control Panel (3行 x 4列, 110px) ---
    auto controlArea = juce::Rectangle<int>(0, topBarHeight + visualizerHeight, w, controlPanelHeight);

    const int colWidth = w / 4;
    const int rowHeight = controlPanelHeight / 3;

    auto getCellBounds = [&](int row, int col) -> juce::Rectangle<int>
    {
        return juce::Rectangle<int>(col * colWidth, controlArea.getY() + row * rowHeight, colWidth, rowHeight);
    };

    // 行 0
    sliderDepth->setBounds(getCellBounds(0, 0));
    sliderAttack->setBounds(getCellBounds(0, 1));

    auto splitCell = getCellBounds(0, 2);
    btnSplitToggle.setBounds(splitCell.getX(), splitCell.getY(), 40, splitCell.getHeight());
    sliderSplitFreq->setBounds(splitCell.getX() + 40, splitCell.getY(), splitCell.getWidth() - 40, splitCell.getHeight());

    sliderMix->setBounds(getCellBounds(0, 3));

    // 行 1
    sliderThreshOrVel->setBounds(getCellBounds(1, 0));
    sliderHold->setBounds(getCellBounds(1, 1));
    btnTargetChannel.setBounds(getCellBounds(1, 2));

    // 行 2
    sliderScHpf->setBounds(getCellBounds(2, 0));
    sliderRelease->setBounds(getCellBounds(2, 1));
    sliderCurve->setBounds(getCellBounds(2, 2));
    sliderScLpf->setBounds(getCellBounds(2, 3));
}
