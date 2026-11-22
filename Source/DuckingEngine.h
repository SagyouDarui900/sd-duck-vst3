#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

class DuckingEngine
{
public:
    enum class TriggerMode
    {
        Sync = 0,
        Audio,
        MIDI
    };

    enum class DetectType
    {
        Follow = 0,
        OneShot
    };

    DuckingEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;
        samplePeriod = static_cast<float>(1.0 / (currentSampleRate > 0.0 ? currentSampleRate : 44100.0));

        // 時定数 tau = 1.0ms のワンポールローパススムージングフィルタ係数を事前計算
        constexpr float tau = 0.001f; // 1.0 ms
        smoothAlpha = std::exp(-samplePeriod / tau);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 2;

        scHpf.prepare(spec);
        scLpf.prepare(spec);

        reset();
    }

    void reset()
    {
        scHpf.reset();
        scLpf.reset();
        currentGain = 1.0f;
        targetGain = 1.0f;
        triggerStartGain = 1.0f;
        oneShotTimeSec = 100.0; // 終了状態
        envelopeFollower = 0.0f;
        lastTriggerTimeSec = -1.0;
        isArmReady = true;
        activeDepth = 0.8f;
        lastHpf = -1.0f;
        lastLpf = -1.0f;
    }

    static float applyCurve(float x, float curve) noexcept
    {
        x = juce::jlimit(0.0f, 1.0f, x);
        if (std::abs(curve) < 0.001f)
            return x;

        if (curve > 0.0f)
        {
            // 指数カーブ: 最初ゆっくり、最後に急復帰
            const float power = 1.0f + 3.0f * curve;
            return std::pow(x, power);
        }
        else
        {
            // 対数カーブ: 最初急激、後半なだらか
            const float power = 1.0f + 3.0f * (-curve);
            return 1.0f - std::pow(1.0f - x, power);
        }
    }

    // SC HPF / LPF フィルタ係数更新 (キャッシュによる毎ブロックのヒープ確保排除)
    void updateSidechainFilters(float hpfHz, float lpfHz)
    {
        if (currentSampleRate <= 0.0)
            return;

        const float safeHpf = juce::jlimit(20.0f, 2000.0f, hpfHz);
        // 帯域逆転防止: effective_lpf = max(sc_lpf, sc_hpf + 10.0 Hz)
        const float safeLpf = juce::jlimit(safeHpf + 10.0f, 20000.0f, std::max(lpfHz, safeHpf + 10.0f));

        constexpr float qVal = 0.70710678f;
        if (std::abs(safeHpf - lastHpf) > 0.1f)
        {
            lastHpf = safeHpf;
            *scHpf.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, safeHpf, qVal);
        }
        if (std::abs(safeLpf - lastLpf) > 0.1f)
        {
            lastLpf = safeLpf;
            *scLpf.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeLpf, qVal);
        }
    }

    // サイドチェイン入力のフィルタリング
    void filterSidechainBuffer(juce::AudioBuffer<float>& scBuffer, int numSamples)
    {
        juce::dsp::AudioBlock<float> block(scBuffer);
        auto subBlock = block.getSubBlock(0, static_cast<size_t>(numSamples));
        juce::dsp::ProcessContextReplacing<float> context(subBlock);
        scHpf.process(context);
        scLpf.process(context);
    }

    // ワンショットのトリガー開始 (現在ゲインを記録して音量跳ね上がりを防止)
    void triggerOneShot(float customDepth = -1.0f)
    {
        oneShotTimeSec = 0.0;
        triggerStartGain = currentGain;
        if (customDepth >= 0.0f)
            activeDepth = customDepth;
    }

    // サンプル単位のゲイン計算（高効率・インライン化）
    float processSample(TriggerMode mode,
                        DetectType detectType,
                        float depth,
                        float attackSec,
                        float holdSec,
                        float releaseSec,
                        float curve,
                        float scSample,
                        float thresholdLinear,
                        float hysteresisLinear,
                        float thresholdDb,
                        float attCoeff,
                        float relCoeff,
                        double ppqPos,
                        double bpm,
                        double rateBeats,
                        bool isPlaying)
    {
        if (mode == TriggerMode::Sync)
        {
            if (!isPlaying || bpm <= 0.0)
            {
                // 再生停止時は目標ゲインを1.0にセットし、スムージング経由で約3〜5msで収束
                targetGain = 1.0f;
            }
            else
            {
                // ppqPos から現在の周期内位相 p in [0, 1) を算出
                const double cycleLengthInBeats = std::max(0.0625, rateBeats);
                double phaseInBeats = std::fmod(ppqPos, cycleLengthInBeats);
                if (phaseInBeats < 0.0)
                    phaseInBeats += cycleLengthInBeats;

                const double phase = phaseInBeats / cycleLengthInBeats; // [0, 1)
                const double secondsPerBeat = 60.0 / bpm;
                const double cycleDurationSec = cycleLengthInBeats * secondsPerBeat;
                const double t = phase * cycleDurationSec;

                activeDepth = depth;
                targetGain = evaluateEnvelope(static_cast<float>(t), attackSec, holdSec, releaseSec, curve, activeDepth, 1.0f);
            }
        }
        else if (mode == TriggerMode::Audio)
        {
            const float absVal = std::abs(scSample);

            if (detectType == DetectType::OneShot)
            {
                // ヒステリシス (-3 dB) と不応期 (30 ms) をリニア値で高速判定
                constexpr double refractoryPeriodSec = 0.030; // 30ms

                if (oneShotTimeSec >= refractoryPeriodSec && absVal < hysteresisLinear)
                {
                    isArmReady = true;
                }

                if (isArmReady && absVal >= thresholdLinear)
                {
                    isArmReady = false;
                    triggerOneShot(depth);
                }

                targetGain = evaluateEnvelope(static_cast<float>(oneShotTimeSec), attackSec, holdSec, releaseSec, curve, activeDepth, triggerStartGain);
                oneShotTimeSec += samplePeriod;
            }
            else // Follow モード
            {
                // 事前計算済みの係数による高速エンベロープフォロワー
                if (absVal > envelopeFollower)
                    envelopeFollower = absVal + attCoeff * (envelopeFollower - absVal);
                else
                    envelopeFollower = absVal + relCoeff * (envelopeFollower - absVal);

                if (envelopeFollower <= thresholdLinear)
                {
                    targetGain = 1.0f;
                }
                else
                {
                    const float envDb = (envelopeFollower > 1e-5f) ? (20.0f * std::log10(envelopeFollower)) : -100.0f;
                    const float overDb = envDb - thresholdDb;
                    // 6dB超過でフルDepthに到達する高感度レスポンス
                    const float rawRatio = juce::jlimit(0.0f, 1.0f, overDb / 6.0f);
                    const float curvedRatio = applyCurve(rawRatio, -curve);
                    targetGain = 1.0f - (depth * curvedRatio);
                }
            }
        }
        else if (mode == TriggerMode::MIDI)
        {
            targetGain = evaluateEnvelope(static_cast<float>(oneShotTimeSec), attackSec, holdSec, releaseSec, curve, activeDepth, triggerStartGain);
            oneShotTimeSec += samplePeriod;
        }

        // 事前計算済み smoothAlpha による高速ワンポールスムージング (1.0ms)
        currentGain = targetGain + smoothAlpha * (currentGain - targetGain);

        return currentGain;
    }

private:
    static float evaluateEnvelope(float t, float attackSec, float holdSec, float releaseSec, float curve, float depth, float startGain = 1.0f) noexcept
    {
        const float targetMin = 1.0f - depth;
        if (t < attackSec && attackSec > 1e-6f)
        {
            const float progress = t / attackSec;
            if (startGain > targetMin)
                return startGain - (startGain - targetMin) * progress;
            else
                return targetMin;
        }
        else if (t < (attackSec + holdSec))
        {
            return targetMin;
        }
        else if (t < (attackSec + holdSec + releaseSec) && releaseSec > 1e-6f)
        {
            const float relProgress = (t - (attackSec + holdSec)) / releaseSec;
            const float curvedProgress = applyCurve(relProgress, curve);
            return targetMin + (depth * curvedProgress);
        }
        else
        {
            return 1.0f;
        }
    }

    double currentSampleRate = 44100.0;
    float samplePeriod = 1.0f / 44100.0f;
    float smoothAlpha = 0.867f;

    float currentGain = 1.0f;
    float targetGain = 1.0f;
    float triggerStartGain = 1.0f;
    float activeDepth = 0.8f;

    double oneShotTimeSec = 100.0;
    double lastTriggerTimeSec = -1.0;
    bool isArmReady = true;
    float envelopeFollower = 0.0f;

    float lastHpf = -1.0f;
    float lastLpf = -1.0f;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> scHpf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> scLpf;
};
