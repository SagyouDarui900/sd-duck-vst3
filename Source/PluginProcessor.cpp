#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::String SDDuckAudioProcessor::paramMode         = "mode";
const juce::String SDDuckAudioProcessor::paramSyncRate     = "sync_rate";
const juce::String SDDuckAudioProcessor::paramScDetect     = "sc_detect";
const juce::String SDDuckAudioProcessor::paramThreshold    = "threshold";
const juce::String SDDuckAudioProcessor::paramDepth        = "depth";
const juce::String SDDuckAudioProcessor::paramAttack       = "attack";
const juce::String SDDuckAudioProcessor::paramHold         = "hold";
const juce::String SDDuckAudioProcessor::paramRelease      = "release";
const juce::String SDDuckAudioProcessor::paramCurve        = "curve";
const juce::String SDDuckAudioProcessor::paramSplitEnabled = "split_enabled";
const juce::String SDDuckAudioProcessor::paramSplitFreq    = "split_freq";
const juce::String SDDuckAudioProcessor::paramChannelMode  = "channel_mode";
const juce::String SDDuckAudioProcessor::paramMidiNote     = "midi_note";
const juce::String SDDuckAudioProcessor::paramMidiVelSens  = "midi_vel_sens";
const juce::String SDDuckAudioProcessor::paramScHpf        = "sc_hpf";
const juce::String SDDuckAudioProcessor::paramScLpf        = "sc_lpf";
const juce::String SDDuckAudioProcessor::paramScListen     = "sc_listen";
const juce::String SDDuckAudioProcessor::paramMix          = "mix";

SDDuckAudioProcessor::SDDuckAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput  ("Input",      juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output",     juce::AudioChannelSet::stereo(), true)
        .withInput  ("Side Chain", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

SDDuckAudioProcessor::~SDDuckAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout SDDuckAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Mode: Sync / Audio / MIDI
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(paramMode, 1), "Mode",
        juce::StringArray { "Sync", "Audio", "MIDI" }, 0));

    // Sync Rate: 1/1, 1/2, 1/4, 1/8, 1/16, 1/4T, 1/8D
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(paramSyncRate, 1), "Rate",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8D" }, 2)); // デフォルト 1/4

    // SC Detect: Follow / One-Shot
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(paramScDetect, 1), "Detect Type",
        juce::StringArray { "Follow", "One-Shot" }, 1)); // デフォルト One-Shot

    // Threshold: -60.0 〜 0.0 dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramThreshold, 1), "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -18.0f));

    // Depth: 0.0 〜 100.0 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramDepth, 1), "Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 80.0f));

    // Attack: 0.0 〜 100.0 ms (対数スキュー: 中央値 10.0 ms)
    juce::NormalisableRange<float> attackRange(0.0f, 100.0f, 0.1f);
    attackRange.setSkewForCentre(10.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramAttack, 1), "Attack", attackRange, 2.0f));

    // Hold: 0.0 〜 500.0 ms (対数スキュー: 中央値 50.0 ms)
    juce::NormalisableRange<float> holdRange(0.0f, 500.0f, 0.1f);
    holdRange.setSkewForCentre(50.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramHold, 1), "Hold", holdRange, 20.0f));

    // Release: 10.0 〜 1000.0 ms (対数スキュー: 中央値 150.0 ms)
    juce::NormalisableRange<float> releaseRange(10.0f, 1000.0f, 0.1f);
    releaseRange.setSkewForCentre(150.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramRelease, 1), "Release", releaseRange, 150.0f));

    // Curve: -1.0 〜 +1.0
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramCurve, 1), "Curve",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

    // Split Enabled: Off / On
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(paramSplitEnabled, 1), "Split Active", false));

    // Split Freq: 30.0 〜 500.0 Hz
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramSplitFreq, 1), "Split Freq",
        juce::NormalisableRange<float>(30.0f, 500.0f, 0.1f), 120.0f));

    // Target Channel: Stereo / Mid Only
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(paramChannelMode, 1), "Target",
        juce::StringArray { "Stereo", "Mid Only" }, 0));

    // MIDI Note: 0 〜 128 (0 = Any Note, 1〜128 = Note 0〜127)
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(paramMidiNote, 1), "MIDI Note", 0, 128, 0));

    // MIDI Vel Sens: 0.0 〜 100.0 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramMidiVelSens, 1), "Vel Sens",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    // SC HPF: 20.0 〜 2000.0 Hz
    juce::NormalisableRange<float> hpfRange(20.0f, 2000.0f, 0.1f);
    hpfRange.setSkewForCentre(150.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramScHpf, 1), "SC HPF", hpfRange, 20.0f));

    // SC LPF: 200.0 〜 20000.0 Hz
    juce::NormalisableRange<float> lpfRange(200.0f, 20000.0f, 1.0f);
    lpfRange.setSkewForCentre(2000.0f);
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramScLpf, 1), "SC LPF", lpfRange, 20000.0f));

    // SC Listen: Off / On
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(paramScListen, 1), "SC Listen", false));

    // Mix: 0.0 〜 100.0 %
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramMix, 1), "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    return { params.begin(), params.end() };
}

double SDDuckAudioProcessor::getRateMultiplier(int rateIndex)
{
    // Choice: 1/1, 1/2, 1/4, 1/8, 1/16, 1/4T, 1/8D
    switch (rateIndex)
    {
        case 0: return 4.0;              // 1/1
        case 1: return 2.0;              // 1/2
        case 2: return 1.0;              // 1/4
        case 3: return 0.5;              // 1/8
        case 4: return 0.25;             // 1/16
        case 5: return 2.0 / 3.0;        // 1/4T
        case 6: return 0.75;             // 1/8D
        default: return 1.0;
    }
}

bool SDDuckAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // メイン出力はStereo必須
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // メイン入力はStereo必須
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // サイドチェイン入力は無効またはStereo
    const auto sc = layouts.getChannelSet(true, 1);
    if (sc != juce::AudioChannelSet::disabled() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SDDuckAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    duckingEngine.prepare(sampleRate, samplesPerBlock);
    crossoverFilter.prepare(sampleRate, samplesPerBlock);

    dryBuffer.setSize(2, samplesPerBlock);
    filteredScBuffer.setSize(2, samplesPerBlock);

    visualFifo.reset();
    sampleAccumulator = 0;
    currentPacketPrePeak = 0.0f;
    currentPacketPostPeak = 0.0f;
    currentPacketMinGain = 1.0f;
    currentPacketScPeak = 0.0f;
}

void SDDuckAudioProcessor::releaseResources()
{
    duckingEngine.reset();
    crossoverFilter.reset();
}

void SDDuckAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // 未使用チャンネルのクリア
    for (int i = 2; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    // パラメータ取得
    const auto mode = static_cast<DuckingEngine::TriggerMode>(static_cast<int>(*apvts.getRawParameterValue(paramMode)));
    const int syncRateIdx = static_cast<int>(*apvts.getRawParameterValue(paramSyncRate));
    const auto scDetect = static_cast<DuckingEngine::DetectType>(static_cast<int>(*apvts.getRawParameterValue(paramScDetect)));
    const float thresholdDb = *apvts.getRawParameterValue(paramThreshold);
    const float depthNormalized = (*apvts.getRawParameterValue(paramDepth)) * 0.01f;
    const float attackMs = *apvts.getRawParameterValue(paramAttack);
    const float holdMs = *apvts.getRawParameterValue(paramHold);
    const float releaseMs = *apvts.getRawParameterValue(paramRelease);
    const float curve = *apvts.getRawParameterValue(paramCurve);
    const bool splitEnabled = *apvts.getRawParameterValue(paramSplitEnabled) > 0.5f;
    const float splitFreq = *apvts.getRawParameterValue(paramSplitFreq);
    const int channelMode = static_cast<int>(*apvts.getRawParameterValue(paramChannelMode)); // 0: Stereo, 1: Mid Only
    const int midiNoteParam = static_cast<int>(*apvts.getRawParameterValue(paramMidiNote));
    const float midiVelSens = (*apvts.getRawParameterValue(paramMidiVelSens)) * 0.01f;
    const float scHpf = *apvts.getRawParameterValue(paramScHpf);
    const float scLpf = *apvts.getRawParameterValue(paramScLpf);
    const bool scListen = *apvts.getRawParameterValue(paramScListen) > 0.5f;
    const float mix = (*apvts.getRawParameterValue(paramMix)) * 0.01f;

    // PlayHead タイムライン情報取得
    double bpm = 120.0;
    double ppqPosition = 0.0;
    bool isPlaying = false;

    if (auto* playHead = getPlayHead())
    {
        if (auto posOpt = playHead->getPosition())
        {
            if (posOpt->getBpm().hasValue())
                bpm = *posOpt->getBpm();
            if (posOpt->getPpqPosition().hasValue())
                ppqPosition = *posOpt->getPpqPosition();
            isPlaying = posOpt->getIsPlaying();
        }
    }

    const double rateMultiplier = getRateMultiplier(syncRateIdx);
    const double sr = getSampleRate();
    const float samplePeriod = static_cast<float>(1.0 / (sr > 0.0 ? sr : 44100.0));
    const float attackSec = std::max(0.0001f, attackMs * 0.001f);
    const float holdSec = std::max(0.0f, holdMs * 0.001f);
    const float releaseSec = std::max(0.001f, releaseMs * 0.001f);
    const float thresholdLinear = std::pow(10.0f, thresholdDb / 20.0f);
    const float hysteresisLinear = std::pow(10.0f, (thresholdDb - 3.0f) / 20.0f);
    const float attCoeff = std::exp(-samplePeriod / std::max(0.0001f, attackSec * 0.33f));
    const float relCoeff = std::exp(-samplePeriod / std::max(0.001f, releaseSec * 0.33f));
    const double ppqAdvancePerSample = (sr > 0.0) ? ((bpm / 60.0) / sr) : 0.0;

    // サイドチェイン入力の準備 (ch 2, 3)
    filteredScBuffer.setSize(2, numSamples, false, false, true);
    if (totalNumInputChannels >= 4)
    {
        filteredScBuffer.copyFrom(0, 0, buffer, 2, 0, numSamples);
        filteredScBuffer.copyFrom(1, 0, buffer, 3, 0, numSamples);
    }
    else
    {
        filteredScBuffer.clear();
    }

    // SC フィルタ適用
    duckingEngine.updateSidechainFilters(scHpf, scLpf);
    duckingEngine.filterSidechainBuffer(filteredScBuffer, numSamples);

    // SC Listen モード: サイドチェイン音声をメイン出力へスルー出力しつつステータス・ビジュアライザを更新
    if (scListen)
    {
        buffer.copyFrom(0, 0, filteredScBuffer, 0, 0, numSamples);
        buffer.copyFrom(1, 0, filteredScBuffer, 1, 0, numSamples);

        const float* scL = filteredScBuffer.getReadPointer(0);
        const float* scR = filteredScBuffer.getReadPointer(1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float scAmp = std::max(std::abs(scL[sample]), std::abs(scR[sample]));
            const double samplePpq = ppqPosition + static_cast<double>(sample) * ppqAdvancePerSample;

            currentPacketPrePeak = std::max(currentPacketPrePeak, scAmp);
            currentPacketPostPeak = std::max(currentPacketPostPeak, scAmp);
            currentPacketMinGain = 1.0f;
            currentPacketScPeak = std::max(currentPacketScPeak, scAmp);

            sampleAccumulator++;
            if (sampleAccumulator >= 128)
            {
                VisualPacket packet;
                packet.prePeak = currentPacketPrePeak;
                packet.postPeak = currentPacketPostPeak;
                packet.minGain = 1.0f;

                const float scDb = (currentPacketScPeak > 1e-5f) ? (20.0f * std::log10(currentPacketScPeak)) : -100.0f;
                const bool scActive = (currentPacketScPeak > 0.00003f); // 約 -90dB
                packet.scPeakDb = scDb;
                packet.isSidechainActive = scActive;
                packet.ppqPosition = samplePpq;
                packet.bpm = bpm;
                packet.isPlaying = isPlaying;

                lastScPeakDb.store(scDb, std::memory_order_relaxed);
                lastScActive.store(scActive, std::memory_order_relaxed);

                visualFifo.push(packet);

                sampleAccumulator = 0;
                currentPacketPrePeak = 0.0f;
                currentPacketPostPeak = 0.0f;
                currentPacketMinGain = 1.0f;
                currentPacketScPeak = 0.0f;
            }
        }
        return;
    }

    // Dry信号を未処理原音（ch 0, 1）からバックアップ
    dryBuffer.setSize(2, numSamples, false, false, true);
    dryBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    dryBuffer.copyFrom(1, 0, buffer, 1, 0, numSamples);

    // Split-Band 準備
    if (splitEnabled)
    {
        crossoverFilter.setFrequency(splitFreq);
        crossoverFilter.split(buffer, numSamples);
    }

    // 処理対象バッファのポインタ設定 (Split時はLowBuffer、通常時はMainBuffer)
    juce::AudioBuffer<float>& targetBuffer = splitEnabled ? crossoverFilter.getLowBuffer() : buffer;
    float* leftChannel = targetBuffer.getWritePointer(0);
    float* rightChannel = targetBuffer.getWritePointer(1);

    const float* scLeft = filteredScBuffer.getReadPointer(0);
    const float* scRight = filteredScBuffer.getReadPointer(1);

    const float* dryLeft = dryBuffer.getReadPointer(0);
    const float* dryRight = dryBuffer.getReadPointer(1);

    // MIDI イテレータ
    auto midiIt = midiMessages.begin();

    // サンプル単位の処理ループ
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 現在サンプル位置でのMIDIノートオン確認
        while (midiIt != midiMessages.end() && (*midiIt).samplePosition <= sample)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
            {
                const int noteNumber = msg.getNoteNumber();
                // midiNoteParam == 0 (Any) または (midiNoteParam - 1 == noteNumber)
                if (midiNoteParam == 0 || (midiNoteParam - 1 == noteNumber))
                {
                    const float vel = static_cast<float>(msg.getVelocity());
                    const float effectiveDepth = depthNormalized * ((1.0f - midiVelSens) + midiVelSens * (vel / 127.0f));
                    duckingEngine.triggerOneShot(effectiveDepth);
                }
            }
            ++midiIt;
        }

        // サイドチェインサンプル（L/Rの最大振幅）
        const float scSampleL = scLeft[sample];
        const float scSampleR = scRight[sample];
        const float scSample = std::max(std::abs(scSampleL), std::abs(scSampleR));

        // テンポ同期のサンプル毎PPQ進捗計算
        const double samplePpq = ppqPosition + static_cast<double>(sample) * ppqAdvancePerSample;

        // ゲイン係数算出（事前計算済みの係数による高速処理）
        const float gain = duckingEngine.processSample(
            mode, scDetect, depthNormalized,
            attackSec, holdSec, releaseSec, curve,
            scSample, thresholdLinear, hysteresisLinear, thresholdDb,
            attCoeff, relCoeff,
            samplePpq, bpm, rateMultiplier, isPlaying);

        // Pre波形ピーク（処理前DryのL/R絶対値）
        const float preAmp = std::max(std::abs(dryLeft[sample]), std::abs(dryRight[sample]));

        // ゲイン適用 (Stereo / Mid-Only)
        if (channelMode == 1) // Mid Only
        {
            float mid = 0.0f;
            float side = 0.0f;
            CrossoverFilter::encodeMidSide(leftChannel[sample], rightChannel[sample], mid, side);
            mid *= gain;
            CrossoverFilter::decodeMidSide(mid, side, leftChannel[sample], rightChannel[sample]);
        }
        else // Stereo
        {
            leftChannel[sample] *= gain;
            rightChannel[sample] *= gain;
        }

        const float postAmp = std::max(std::abs(leftChannel[sample]), std::abs(rightChannel[sample]));

        // パケット用集計
        currentPacketPrePeak = std::max(currentPacketPrePeak, preAmp);
        currentPacketPostPeak = std::max(currentPacketPostPeak, postAmp);
        currentPacketMinGain = std::min(currentPacketMinGain, gain);
        currentPacketScPeak = std::max(currentPacketScPeak, scSample);

        sampleAccumulator++;
        if (sampleAccumulator >= 128)
        {
            VisualPacket packet;
            packet.prePeak = currentPacketPrePeak;
            packet.postPeak = currentPacketPostPeak;
            packet.minGain = currentPacketMinGain;

            const float scDb = (currentPacketScPeak > 1e-5f) ? (20.0f * std::log10(currentPacketScPeak)) : -100.0f;
            const bool scActive = (currentPacketScPeak > 0.00003f); // 約 -90dB
            packet.scPeakDb = scDb;
            packet.isSidechainActive = scActive;
            packet.ppqPosition = samplePpq;
            packet.bpm = bpm;
            packet.isPlaying = isPlaying;

            lastScPeakDb.store(scDb, std::memory_order_relaxed);
            lastScActive.store(scActive, std::memory_order_relaxed);

            visualFifo.push(packet);

            // リセット
            sampleAccumulator = 0;
            currentPacketPrePeak = 0.0f;
            currentPacketPostPeak = 0.0f;
            currentPacketMinGain = 1.0f;
            currentPacketScPeak = 0.0f;
        }
    }

    // Split-Band 再合成
    if (splitEnabled)
    {
        crossoverFilter.combine(buffer, numSamples);
    }

    // Dry / Wet ミックス
    if (mix < 0.999f)
    {
        const float wetGain = mix;
        const float dryGain = 1.0f - mix;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            const auto* dry = dryBuffer.getReadPointer(ch);
            for (int s = 0; s < numSamples; ++s)
            {
                out[s] = dryGain * dry[s] + wetGain * out[s];
            }
        }
    }
}

juce::AudioProcessorEditor* SDDuckAudioProcessor::createEditor()
{
    return new SDDuckAudioProcessorEditor(*this);
}

void SDDuckAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SDDuckAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

// プラグインインスタンス生成関数
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SDDuckAudioProcessor();
}
