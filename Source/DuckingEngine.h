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
        oneShotTimeSec = 100.0; // 終了状態
        envelopeFollower = 0.0f;
        lastTriggerTimeSec = -1.0;
        isArmReady = true;
        activeDepth = 0.8f;
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

    // SC HPF / LPF フィルタ係数更新
    void updateSidechainFilters(float hpfHz, float lpfHz)
    {
        if (currentSampleRate <= 0.0)
            return;

        const float safeHpf = juce::jlimit(20.0f, 2000.0f, hpfHz);
        // 帯域逆転防止: effective_lpf = max(sc_lpf, sc_hpf + 10.0 Hz)
        const float safeLpf = juce::jlimit(safeHpf + 10.0f, 20000.0f, std::max(lpfHz, safeHpf + 10.0f));

        constexpr float qVal = 0.70710678f;
        auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, safeHpf, qVal);
        auto lpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeLpf, qVal);

        *scHpf.state = *hpfCoeffs;
        *scLpf.state = *lpfCoeffs;
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

    // ワンショットのトリガー開始
    void triggerOneShot(float customDepth = -1.0f)
    {
        oneShotTimeSec = 0.0;
        if (customDepth >= 0.0f)
            activeDepth = customDepth;
    }

    // サンプル単位のゲイン計算
    // ppqPos: 現在の拍位置
    // bpm: テンポ
    // rateBeats: 1/4なら1.0, 1/8なら0.5など
    // isPlaying: 再生中か
    float processSample(TriggerMode mode,
                        DetectType detectType,
                        float depth,
                        float attackMs,
                        float holdMs,
                        float releaseMs,
                        float curve,
                        float scSample,
                        float thresholdDb,
                        double ppqPos,
                        double bpm,
                        double rateBeats,
                        bool isPlaying)
    {
        const float attackSec = std::max(0.0001f, attackMs * 0.001f);
        const float holdSec = std::max(0.0f, holdMs * 0.001f);
        const float releaseSec = std::max(0.001f, releaseMs * 0.001f);
        const float samplePeriod = static_cast<float>(1.0 / (currentSampleRate > 0.0 ? currentSampleRate : 44100.0));

        if (mode == TriggerMode::Sync)
        {
            if (!isPlaying || bpm <= 0.0)
            {
                // 再生停止時は目標ゲインを1.0にセット
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
                targetGain = evaluateEnvelope(static_cast<float>(t), attackSec, holdSec, releaseSec, curve, activeDepth);
            }
        }
        else if (mode == TriggerMode::Audio)
        {
            const float absVal = std::abs(scSample);
            // レベルをdB換算
            const float scDb = (absVal > 1e-5f) ? (20.0f * std::log10(absVal)) : -100.0f;

            if (detectType == DetectType::OneShot)
            {
                // ヒステリシス (-3 dB) と不応期 (30 ms)
                constexpr float hysteresisDb = 3.0f;
                constexpr double refractoryPeriodSec = 0.030; // 30ms

                if (oneShotTimeSec >= refractoryPeriodSec && scDb < (thresholdDb - hysteresisDb))
                {
                    isArmReady = true;
                }

                if (isArmReady && scDb >= thresholdDb)
                {
                    isArmReady = false;
                    triggerOneShot(depth);
                }

                targetGain = evaluateEnvelope(static_cast<float>(oneShotTimeSec), attackSec, holdSec, releaseSec, curve, activeDepth);
                oneShotTimeSec += samplePeriod;
            }
            else // Follow モード
            {
                // エンベロープフォロワー
                const float attCoeff = std::exp(-samplePeriod / attackSec);
                const float relCoeff = std::exp(-samplePeriod / releaseSec);

                if (absVal > envelopeFollower)
                    envelopeFollower = absVal + attCoeff * (envelopeFollower - absVal);
                else
                    envelopeFollower = absVal + relCoeff * (envelopeFollower - absVal);

                const float envDb = (envelopeFollower > 1e-5f) ? (20.0f * std::log10(envelopeFollower)) : -100.0f;

                if (envDb <= thresholdDb)
                {
                    targetGain = 1.0f;
                }
                else
                {
                    // 閾値を超えた量に応じたゲインリダクション
                    const float overDb = envDb - thresholdDb;
                    // 最大24dB超過で指定Depthに到達
                    const float ratio = juce::jlimit(0.0f, 1.0f, overDb / 24.0f);
                    targetGain = 1.0f - (depth * ratio);
                }
            }
        }
        else if (mode == TriggerMode::MIDI)
        {
            // MIDIモードは processBlock でのノートオン時に triggerOneShot が呼ばれる
            targetGain = evaluateEnvelope(static_cast<float>(oneShotTimeSec), attackSec, holdSec, releaseSec, curve, activeDepth);
            oneShotTimeSec += samplePeriod;
        }

        // 時定数 tau = 1.0ms のワンポールローパススムージングフィルタ
        // alpha = exp(-dt / tau)
        constexpr float tau = 0.001f; // 1.0 ms
        const float smoothAlpha = std::exp(-samplePeriod / tau);
        currentGain = targetGain + smoothAlpha * (currentGain - targetGain);

        return currentGain;
    }

private:
    static float evaluateEnvelope(float t, float attackSec, float holdSec, float releaseSec, float curve, float depth) noexcept
    {
        if (t < attackSec)
        {
            // アタック: 1.0 -> (1.0 - depth)
            const float progress = t / attackSec;
            return 1.0f - (depth * progress);
        }
        else if (t < (attackSec + holdSec))
        {
            // ホールド: (1.0 - depth)
            return 1.0f - depth;
        }
        else if (t < (attackSec + holdSec + releaseSec))
        {
            // リリース: (1.0 - depth) -> 1.0
            const float relProgress = (t - (attackSec + holdSec)) / releaseSec;
            const float curvedProgress = applyCurve(relProgress, curve);
            return (1.0f - depth) + (depth * curvedProgress);
        }
        else
        {
            // 終了: 1.0
            return 1.0f;
        }
    }

    double currentSampleRate = 44100.0;
    float currentGain = 1.0f;
    float targetGain = 1.0f;
    float activeDepth = 0.8f;

    double oneShotTimeSec = 100.0;
    double lastTriggerTimeSec = -1.0;
    bool isArmReady = true;
    float envelopeFollower = 0.0f;

    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> scHpf;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> scLpf;
};
