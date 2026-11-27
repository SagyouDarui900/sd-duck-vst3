#include "PluginEditor.h"
#include <iomanip>
#include <sstream>

// ボタン / コンボボックスの描画は sdduck::RetroLookAndFeel（SD-Verb と共通）が行う。
// SDDuckLookAndFeel は削除した。

// ==============================================================================
// HorizontalBarSlider 実装
// ==============================================================================
HorizontalBarSlider::HorizontalBarSlider(juce::RangedAudioParameter* parameter,
                                         const juce::String& labelText,
                                         const juce::String& unitSuffix,
                                         const sdduck::RetroLookAndFeel* laf)
    : param(parameter), label(labelText), suffix(unitSuffix), lookAndFeel(laf)
{
    if (param != nullptr)
        param->addListener(this);
}

juce::FontOptions HorizontalBarSlider::getHeavyFontOptions(float height) const
{
    if (lookAndFeel != nullptr)
        return lookAndFeel->getHeavyFontOptions(height);
    return juce::FontOptions(height, juce::Font::bold);
}

juce::FontOptions HorizontalBarSlider::getNumericFontOptions(float height) const
{
    if (lookAndFeel != nullptr)
        return lookAndFeel->getNumericFontOptions(height);
    return juce::FontOptions(height, juce::Font::bold);
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
    juce::Component::SafePointer<HorizontalBarSlider> safeThis(this);
    juce::MessageManager::callAsync([safeThis]()
    {
        if (safeThis != nullptr)
            safeThis->repaint();
    });
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

    // 枠線 (通常: textMain 1px、ドラッグ中: mainAccent 1.5px)
    if (isDragging)
    {
        g.setColour(theme.borderFocus());
        g.drawRect(bounds, 1.5f);
    }
    else
    {
        g.setColour(theme.borderDark());
        g.drawRect(bounds, 1);
    }

    // テキスト描画 (左: ラベル, 右: 数値)
    const int pad = 8;
    auto textArea = bounds.reduced(pad, 0);

    // ラベル (Heavy 12.5px)
    g.setFont(getHeavyFontOptions(12.5f));
    g.setColour(isEnabled ? theme.textSecondary() : theme.textDisabled());
    g.drawText(label, textArea, juce::Justification::centredLeft, true);

    // 数値 (Numeric 12.5px: 無効時も設定値を確認できるよう薄く描画)
    if (param != nullptr)
    {
        g.setFont(getNumericFontOptions(12.5f));
        g.setColour(isEnabled ? theme.textPrimary() : theme.textDisabled());
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
        inlineEditor->setFont(getNumericFontOptions(12.0f));
        inlineEditor->setColour(juce::TextEditor::backgroundColourId, theme.bgCanvas());
        inlineEditor->setColour(juce::TextEditor::textColourId, theme.textPrimary());
        inlineEditor->setColour(juce::TextEditor::outlineColourId, theme.borderFocus());

        const float val = param->convertFrom0to1(param->getValue());
        inlineEditor->setText(juce::String(val, 2), false);
        inlineEditor->selectAll();

        juce::Component::SafePointer<HorizontalBarSlider> safeThis(this);
        auto safeDestroy = [safeThis]()
        {
            juce::MessageManager::callAsync([safeThis]()
            {
                if (safeThis != nullptr)
                {
                    safeThis->inlineEditor.reset();
                    safeThis->repaint();
                }
            });
        };

        inlineEditor->onReturnKey = [this, safeDestroy]()
        {
            if (inlineEditor != nullptr)
            {
                const auto text = inlineEditor->getText().trim();
                if (text.containsAnyOf("0123456789"))
                {
                    const float parsed = text.getFloatValue();
                    param->beginChangeGesture();
                    param->setValueNotifyingHost(param->convertTo0to1(parsed));
                    param->endChangeGesture();
                }
            }
            safeDestroy();
        };

        inlineEditor->onEscapeKey = safeDestroy;
        inlineEditor->onFocusLost = safeDestroy;

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
VisualizerComponent::VisualizerComponent(SDDuckAudioProcessor& p, const sdduck::RetroLookAndFeel& laf)
    : processor(p), lookAndFeel(&laf)
{
    history.resize(maxHistoryPoints);
    coordsCache.resize(static_cast<size_t>(maxHistoryPoints));
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
    if (lookAndFeel != nullptr)
        g.setFont(lookAndFeel->getNumericFontOptions(10.0f));
    else
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

    for (float db : dbLevels)
    {
        const float norm = -db / 24.0f;
        const int y = static_cast<int>(std::round(norm * (h - 1)));

        g.setColour(theme.gridLine());
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(w));

        g.setColour(theme.gridLabel());
        juce::String text = juce::String(static_cast<int>(db)) + "dB";
        // -24dB は左下の comboTimebase (幅110px, x=8) と重ならないよう x=122 に配置
        const int textX = (db <= -23.0f) ? 122 : 6;
        g.drawText(text, textX, std::max(0, y - 13), 42, 12, juce::Justification::bottomLeft);
    }

    g.setColour(theme.gridLine());
    int numVerticalLines = 4;
    if (timebaseIdx == 1) numVerticalLines = 8;
    else if (timebaseIdx == 3 || timebaseIdx == 4) numVerticalLines = 5;

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

        if (packet.bpm > 0.0)
            lastBpm = packet.bpm;

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

    // 約1.0秒相当 (345パケット以上) でフリーズ休止
    if (silenceFrames > 345)
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

    // タイムベースに応じた表示パケット数の算出
    const double sr = processor.getSampleRate();
    const double sampleRate = (sr > 0.0) ? sr : 44100.0;
    const double effectiveBpm = (lastBpm > 0.0) ? lastBpm : 120.0;

    double targetTimeSec = 0.5;
    if (timebaseIdx == 0)      targetTimeSec = 60.0 / effectiveBpm;       // 1 Beat
    else if (timebaseIdx == 1) targetTimeSec = 120.0 / effectiveBpm;      // 2 Beats
    else if (timebaseIdx == 2) targetTimeSec = 240.0 / effectiveBpm;      // 1 Bar (4 Beats)
    else if (timebaseIdx == 3) targetTimeSec = 0.5;                       // 500 ms
    else if (timebaseIdx == 4) targetTimeSec = 1.0;                       // 1000 ms

    const int calculatedPoints = static_cast<int>(std::round((targetTimeSec * sampleRate) / 128.0));
    const int pointsToShow = juce::jlimit(16, maxHistoryPoints, calculatedPoints);

    juce::Path postPath;

    for (int i = 0; i < pointsToShow; ++i)
    {
        const int idx = (static_cast<int>(historyIndex) - pointsToShow + i + maxHistoryPoints) % maxHistoryPoints;
        const auto& pt = history[idx];

        coordsCache[i].x = (static_cast<float>(i) / static_cast<float>(pointsToShow - 1)) * static_cast<float>(w);

        const float grDb = (pt.minGain > 0.00001f) ? (20.0f * std::log10(pt.minGain)) : -100.0f;
        coordsCache[i].grDb = grDb;
        const float grNorm = juce::jlimit(0.0f, 1.0f, -grDb / 24.0f);
        coordsCache[i].yGr = grNorm * static_cast<float>(h);

        const float preAmp = juce::jlimit(0.0f, 1.0f, pt.prePeak);
        coordsCache[i].yPre = static_cast<float>(h) - (preAmp * static_cast<float>(h) * 0.85f);

        const float postAmp = juce::jlimit(0.0f, 1.0f, pt.postPeak);
        coordsCache[i].yPost = static_cast<float>(h) - (postAmp * static_cast<float>(h) * 0.85f);

        if (i == 0)
        {
            postPath.startNewSubPath(coordsCache[i].x, static_cast<float>(h));
            postPath.lineTo(coordsCache[i].x, coordsCache[i].yPost);
        }
        else
        {
            postPath.lineTo(coordsCache[i].x, coordsCache[i].yPost);
        }
    }

    // 1. 残存波形（Post波形）の塗りつぶし（subDecor、不透明）
    if (pointsToShow > 1)
    {
        postPath.lineTo(coordsCache[pointsToShow - 1].x, static_cast<float>(h));
        postPath.closeSubPath();

        g.setColour(theme.palette.subDecor);
        g.fillPath(postPath);
    }

    // 2. Pre波形とPost波形の間の差分領域（削られた量）をセグメントごとに動的カラーで塗りつぶし（GR色、不透明）
    for (int i = 0; i < pointsToShow - 1; ++i)
    {
        const auto& p0 = coordsCache[i];
        const auto& p1 = coordsCache[i + 1];

        const float segGrDb = std::min(p0.grDb, p1.grDb);

        // Pre波形がPost波形より上（振幅が大きい＝音が削られている）の場合に差分を描画
        if (p0.yPre < p0.yPost || p1.yPre < p1.yPost)
        {
            // ポリゴンのねじれ（Post座標がPre座標を上回る逆転）を防止
            const float y0Post = std::max(p0.yPre, p0.yPost);
            const float y1Post = std::max(p1.yPre, p1.yPost);

            juce::Path diffSegment;
            diffSegment.startNewSubPath(p0.x, y0Post);
            diffSegment.lineTo(p0.x, p0.yPre);
            diffSegment.lineTo(p1.x, p1.yPre);
            diffSegment.lineTo(p1.x, y1Post);
            diffSegment.closeSubPath();

            g.setColour(getGrColor(segGrDb));
            g.fillPath(diffSegment);
        }
    }

    // 3. 残存波形（Post波形）の輪郭線（textMain、1.5px）
    if (pointsToShow > 1)
    {
        g.setColour(theme.wavePost());
        g.strokePath(postPath, juce::PathStrokeType(1.5f));
    }

    // 4. 上端のGR折れ線（時間軸動的カラー、線幅1.5px）
    for (int i = 0; i < pointsToShow - 1; ++i)
    {
        const auto& p0 = coordsCache[i];
        const auto& p1 = coordsCache[i + 1];
        const float segGrDb = std::min(p0.grDb, p1.grDb);

        g.setColour(getGrColor(segGrDb));
        g.drawLine(p0.x, p0.yGr, p1.x, p1.yGr, 1.5f);
    }

    g.setColour(theme.borderDark());
    g.drawRect(getLocalBounds(), 1.5f);
}

// ==============================================================================
// SDDuckAudioProcessorEditor 実装
// ==============================================================================
SDDuckAudioProcessorEditor::SDDuckAudioProcessorEditor(SDDuckAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p, lookAndFeel)
{
    // 既存パケットを安全に読み捨て (SPSC スレッド安全な FIFO フラッシュ)
    VisualPacket dummy;
    while (audioProcessor.getVisualFifo().pop(dummy)) {}

    currentTheme.setThemeId(static_cast<sdduck::ThemeId>(juce::jlimit(0, 3, audioProcessor.getSavedThemeId())));

    setLookAndFeel(&lookAndFeel);

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
        updateModeDependencies();
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

    lblScStatus.setFont(lookAndFeel.getNumericFontOptions(11.0f));
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

    // テーマ切り替えボタン (表示は切り替え先: LIGHT/DARK、ORG/CYN)
    btnThemeToggle.onClick = [this]()
    {
        const int id = static_cast<int>(currentTheme.themeId) ^ 1;
        currentTheme.setThemeId(static_cast<sdduck::ThemeId>(id));
        applyTheme();
    };
    addAndMakeVisible(btnThemeToggle);

    btnColorTheme.onClick = [this]()
    {
        const int id = static_cast<int>(currentTheme.themeId) ^ 2;
        currentTheme.setThemeId(static_cast<sdduck::ThemeId>(id));
        applyTheme();
    };
    addAndMakeVisible(btnColorTheme);

    addAndMakeVisible(visualizer);

    comboTimebase.addItemList({ "View: 1 Beat", "View: 2 Beats", "View: 1 Bar", "View: 500 ms", "View: 1000 ms" }, 1);
    comboTimebase.setSelectedId(1, juce::dontSendNotification);
    comboTimebase.onChange = [this]()
    {
        visualizer.setTimebaseIndex(comboTimebase.getSelectedId() - 1);
    };
    addAndMakeVisible(comboTimebase);

    lblMaxGr.setFont(lookAndFeel.getNumericFontOptions(11.0f));
    lblMaxGr.setText("Max GR:  0.0 dB", juce::dontSendNotification);
    lblMaxGr.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(lblMaxGr);

    sliderDepth   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramDepth), "Depth", "%", &lookAndFeel);
    sliderAttack  = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramAttack), "Attack", "ms", &lookAndFeel);
    sliderHold    = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramHold), "Hold", "ms", &lookAndFeel);
    sliderRelease = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramRelease), "Release", "ms", &lookAndFeel);
    sliderCurve   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramCurve), "Curve", "", &lookAndFeel);
    sliderMix     = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramMix), "Mix", "%", &lookAndFeel);

    sliderThreshOrVel = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramThreshold), "Threshold", "dB", &lookAndFeel);
    sliderScHpf   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramScHpf), "SC HPF", "Hz", &lookAndFeel);
    sliderScLpf   = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramScLpf), "SC LPF", "Hz", &lookAndFeel);

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
    btnSplitToggle.setButtonText(initialSplit ? "Split: ON" : "Split: OFF");
    btnSplitToggle.onClick = [this]()
    {
        const bool active = btnSplitToggle.getToggleState();
        btnSplitToggle.setButtonText(active ? "Split: ON" : "Split: OFF");
        sliderSplitFreq->setEnabledState(active);
        if (auto* param = audioProcessor.getAPVTS().getParameter(SDDuckAudioProcessor::paramSplitEnabled))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(active ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };
    addAndMakeVisible(btnSplitToggle);

    sliderSplitFreq = std::make_unique<HorizontalBarSlider>(apvts.getParameter(SDDuckAudioProcessor::paramSplitFreq), "Split Freq", "Hz", &lookAndFeel);
    sliderSplitFreq->setEnabledState(initialSplit);
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

    setSize(680, 390);
    setResizable(true, true);
    setResizeLimits(620, 340, 1920, 1080);

    startTimerHz(60);
}

SDDuckAudioProcessorEditor::~SDDuckAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SDDuckAudioProcessorEditor::applyTheme()
{
    lookAndFeel.setTheme(currentTheme.themeId);
    audioProcessor.setSavedThemeId(static_cast<int>(currentTheme.themeId));

    // ボタン表示は切り替え先を示す (LIGHT/DARK、ORG/CYN)
    btnThemeToggle.setButtonText(currentTheme.isLight() ? "DARK" : "LIGHT");
    btnColorTheme.setButtonText(currentTheme.isCyan() ? "ORG" : "CYN");

    comboSyncRate.sendLookAndFeelChange();
    comboScDetect.sendLookAndFeelChange();
    comboMidiNote.sendLookAndFeelChange();
    comboTimebase.sendLookAndFeelChange();

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

    // ボタンの色は RetroLookAndFeel がトグル状態から描く。個別の setColour はしない。
    lblMaxGr.setColour(juce::Label::backgroundColourId, currentTheme.bgCanvas());
    lblMaxGr.setColour(juce::Label::textColourId, currentTheme.textPrimary());
    lblMaxGr.setColour(juce::Label::outlineColourId, currentTheme.borderDark());

    repaint();
}

void SDDuckAudioProcessorEditor::updateModeDependencies()
{
    auto& apvts = audioProcessor.getAPVTS();
    const int mode = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMode));
    const int scDetect = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramScDetect));

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
        sliderThreshOrVel->updateParameter(apvts.getParameter(SDDuckAudioProcessor::paramThreshold));
        sliderHold->setEnabledState(true);
        sliderScHpf->setEnabledState(false);
        sliderScLpf->setEnabledState(false);
    }
    else if (mode == 1) // Audio
    {
        sliderThreshOrVel->setEnabledState(true);
        sliderThreshOrVel->setCustomLabel("Threshold");
        sliderThreshOrVel->updateParameter(apvts.getParameter(SDDuckAudioProcessor::paramThreshold));
        // Follow モード時は Hold を無効化、One-Shot のみ有効化
        sliderHold->setEnabledState(scDetect == 1);
        sliderScHpf->setEnabledState(true);
        sliderScLpf->setEnabledState(true);
    }
    else // MIDI
    {
        sliderThreshOrVel->setEnabledState(true);
        sliderThreshOrVel->setCustomLabel("Vel Sens");
        sliderThreshOrVel->updateParameter(apvts.getParameter(SDDuckAudioProcessor::paramMidiVelSens));
        sliderHold->setEnabledState(true);
        sliderScHpf->setEnabledState(false);
        sliderScLpf->setEnabledState(false);
    }
}

void SDDuckAudioProcessorEditor::timerCallback()
{
    visualizer.updateFromFifo();

    auto& apvts = audioProcessor.getAPVTS();

    // APVTS と GUI コントロールの同期 (DAW オートメーション・プリセット切替対応)
    const int apvtsMode = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMode));
    const bool isModeMismatch = (apvtsMode == 0 && !btnModeSync.getToggleState()) ||
                                (apvtsMode == 1 && !btnModeAudio.getToggleState()) ||
                                (apvtsMode == 2 && !btnModeMidi.getToggleState());
    if (isModeMismatch)
    {
        updateModeDependencies();
    }

    const int apvtsSyncRateId = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramSyncRate)) + 1;
    if (comboSyncRate.getSelectedId() != apvtsSyncRateId)
        comboSyncRate.setSelectedId(apvtsSyncRateId, juce::dontSendNotification);

    const int apvtsScDetectId = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramScDetect)) + 1;
    if (comboScDetect.getSelectedId() != apvtsScDetectId)
    {
        comboScDetect.setSelectedId(apvtsScDetectId, juce::dontSendNotification);
        updateModeDependencies();
    }

    const int apvtsMidiNoteId = static_cast<int>(*apvts.getRawParameterValue(SDDuckAudioProcessor::paramMidiNote)) + 1;
    if (comboMidiNote.getSelectedId() != apvtsMidiNoteId)
        comboMidiNote.setSelectedId(apvtsMidiNoteId, juce::dontSendNotification);

    const float maxGrDb = visualizer.getCurrentMaxGrDb();
    std::ostringstream ss;
    ss << "Max GR: ";
    if (maxGrDb > -0.05f)
        ss << " 0.0 dB";
    else
        ss << std::fixed << std::setprecision(1) << maxGrDb << " dB";
    lblMaxGr.setText(ss.str(), juce::dontSendNotification);

    const int mode = apvtsMode;
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
            lblScStatus.setColour(juce::Label::textColourId, currentTheme.accent());
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

    const bool splitActive = *apvts.getRawParameterValue(SDDuckAudioProcessor::paramSplitEnabled) > 0.5f;
    btnSplitToggle.setToggleState(splitActive, juce::dontSendNotification);
    btnSplitToggle.setButtonText(splitActive ? "Split: ON" : "Split: OFF");
    sliderSplitFreq->setEnabledState(splitActive);
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
    constexpr int controlPanelHeight = 114; // 38px * 3行
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

    // 右: SC Status & SC MON & THEME (右端から ORG/CYN, LIGHT/DARK, MON の順)
    constexpr int colorBtnWidth = 50;
    constexpr int themeBtnWidth = 50;
    constexpr int monBtnWidth = 45;
    btnColorTheme.setBounds(w - colorBtnWidth, 0, colorBtnWidth, topBarHeight);
    btnThemeToggle.setBounds(w - colorBtnWidth - themeBtnWidth, 0, themeBtnWidth, topBarHeight);
    btnScListen.setBounds(w - colorBtnWidth - themeBtnWidth - monBtnWidth, 0, monBtnWidth, topBarHeight);
    lblScStatus.setBounds(w - colorBtnWidth - themeBtnWidth - monBtnWidth - 140, 0, 135, topBarHeight);

    // --- Visualizer ---
    visualizer.setBounds(0, topBarHeight, w, visualizerHeight);

    comboTimebase.setBounds(8, topBarHeight + visualizerHeight - 26, 110, 20);
    lblMaxGr.setBounds(w - 130, topBarHeight + visualizerHeight - 26, 122, 20);

    // --- Control Panel (3行 x 4列 = 12セル均等配置, 114px) ---
    auto controlArea = juce::Rectangle<int>(0, topBarHeight + visualizerHeight, w, controlPanelHeight);

    const int colWidth = w / 4;
    const int rowHeight = controlPanelHeight / 3;

    auto getCellBounds = [&](int row, int col) -> juce::Rectangle<int>
    {
        const int x = col * colWidth;
        const int cellW = (col == 3) ? (w - x) : colWidth;
        const int y = controlArea.getY() + row * rowHeight;
        const int cellH = (row == 2) ? (controlArea.getBottom() - y) : rowHeight;
        return juce::Rectangle<int>(x, y, cellW, cellH);
    };

    // 行 0: 基本 / アタック / スプリットON・OFF / SC HPF
    sliderDepth->setBounds(getCellBounds(0, 0));
    sliderAttack->setBounds(getCellBounds(0, 1));
    btnSplitToggle.setBounds(getCellBounds(0, 2));
    sliderScHpf->setBounds(getCellBounds(0, 3));

    // 行 1: 閾値・感度 / ホールド / スプリット周波数 / SC LPF
    sliderThreshOrVel->setBounds(getCellBounds(1, 0));
    sliderHold->setBounds(getCellBounds(1, 1));
    sliderSplitFreq->setBounds(getCellBounds(1, 2));
    sliderScLpf->setBounds(getCellBounds(1, 3));

    // 行 2: カーブ / リリース / ターゲット / ミックス
    sliderCurve->setBounds(getCellBounds(2, 0));
    sliderRelease->setBounds(getCellBounds(2, 1));
    btnTargetChannel.setBounds(getCellBounds(2, 2));
    sliderMix->setBounds(getCellBounds(2, 3));
}
