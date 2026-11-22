#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DuckingEngine.h"
#include "CrossoverFilter.h"
#include <array>
#include <atomic>

// UI用ダウンサンプルパケット
struct VisualPacket
{
    float prePeak = 0.0f;
    float postPeak = 0.0f;
    float minGain = 1.0f;
    float scPeakDb = -100.0f;
    bool isSidechainActive = false;
    double ppqPosition = 0.0;
    double bpm = 120.0;
    bool isPlaying = false;
};

// SPSC ロックフリーリングバッファ (容量 2048 パケット)
template <typename T, size_t Capacity>
class LockFreeFifo
{
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

    void push(const T& item) noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_acquire);

        // バッファ満杯時は SPSC 安全のため readIndex を変更せずパケットをドロップ
        if (currentWrite - currentRead >= Capacity)
            return;

        buffer[currentWrite & (Capacity - 1)] = item;
        writeIndex.store(currentWrite + 1, std::memory_order_release);
    }

    bool pop(T& item) noexcept
    {
        const size_t currentRead = readIndex.load(std::memory_order_relaxed);
        const size_t currentWrite = writeIndex.load(std::memory_order_acquire);

        if (currentRead == currentWrite)
            return false;

        item = buffer[currentRead & (Capacity - 1)];
        readIndex.store(currentRead + 1, std::memory_order_release);
        return true;
    }

    void reset() noexcept
    {
        readIndex.store(0, std::memory_order_relaxed);
        writeIndex.store(0, std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> buffer{};
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
};

class SDDuckAudioProcessor : public juce::AudioProcessor
{
public:
    SDDuckAudioProcessor();
    ~SDDuckAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SD-Duck"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    LockFreeFifo<VisualPacket, 2048>& getVisualFifo() noexcept { return visualFifo; }

    float getLastSidechainPeakDb() const noexcept { return lastScPeakDb.load(std::memory_order_relaxed); }
    bool getIsSidechainActive() const noexcept { return lastScActive.load(std::memory_order_relaxed); }

    bool getIsLightTheme() const noexcept { return isLightTheme.load(std::memory_order_relaxed); }
    void setIsLightTheme(bool isLight) noexcept { isLightTheme.store(isLight, std::memory_order_relaxed); }

    // パラメータID定数
    static const juce::String paramMode;
    static const juce::String paramSyncRate;
    static const juce::String paramScDetect;
    static const juce::String paramThreshold;
    static const juce::String paramDepth;
    static const juce::String paramAttack;
    static const juce::String paramHold;
    static const juce::String paramRelease;
    static const juce::String paramCurve;
    static const juce::String paramSplitEnabled;
    static const juce::String paramSplitFreq;
    static const juce::String paramChannelMode;
    static const juce::String paramMidiNote;
    static const juce::String paramMidiVelSens;
    static const juce::String paramScHpf;
    static const juce::String paramScLpf;
    static const juce::String paramScListen;
    static const juce::String paramMix;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static double getRateMultiplier(int rateIndex);

    juce::AudioProcessorValueTreeState apvts;
    DuckingEngine duckingEngine;
    CrossoverFilter crossoverFilter;

    // パケット集計用
    LockFreeFifo<VisualPacket, 2048> visualFifo;
    std::atomic<bool> isLightTheme { false };
    std::atomic<float> lastScPeakDb { -100.0f };
    std::atomic<bool> lastScActive { false };
    int sampleAccumulator = 0;
    float currentPacketPrePeak = 0.0f;
    float currentPacketPostPeak = 0.0f;
    float currentPacketMinGain = 1.0f;
    float currentPacketScPeak = 0.0f;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> filteredScBuffer;
    std::vector<float> sampleGains;
    std::vector<float> sampleScAmps;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDDuckAudioProcessor)
};
