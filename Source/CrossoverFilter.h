#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

class CrossoverFilter
{
public:
    CrossoverFilter() = default;

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
        spec.numChannels = 2;

        lowpassFilter1.prepare(spec);
        lowpassFilter2.prepare(spec);
        highpassFilter1.prepare(spec);
        highpassFilter2.prepare(spec);

        lowBuffer.setSize(2, samplesPerBlock);
        highBuffer.setSize(2, samplesPerBlock);

        updateCoefficients(currentFreq);
        reset();
    }

    void reset()
    {
        lowpassFilter1.reset();
        lowpassFilter2.reset();
        highpassFilter1.reset();
        highpassFilter2.reset();
    }

    void setFrequency(float freqHz)
    {
        if (std::abs(freqHz - currentFreq) > 0.05f)
        {
            currentFreq = freqHz;
            updateCoefficients(currentFreq);
        }
    }

    // 入力バッファをLowとHighに分割する
    void split(const juce::AudioBuffer<float>& input, int numSamples)
    {
        jassert(input.getNumChannels() >= 2);

        lowBuffer.setSize(2, numSamples, false, false, true);
        highBuffer.setSize(2, numSamples, false, false, true);

        // コピー
        for (int ch = 0; ch < 2; ++ch)
        {
            lowBuffer.copyFrom(ch, 0, input, ch, 0, numSamples);
            highBuffer.copyFrom(ch, 0, input, ch, 0, numSamples);
        }

        // LR4 Lowpass = 2段の2次Butterworth Lowpass (Q = 1 / sqrt(2) ≈ 0.70710678)
        juce::dsp::AudioBlock<float> lowBlock(lowBuffer);
        juce::dsp::ProcessContextReplacing<float> lowContext1(lowBlock);
        lowpassFilter1.process(lowContext1);
        lowpassFilter2.process(lowContext1);

        // LR4 Highpass = 2段の2次Butterworth Highpass
        juce::dsp::AudioBlock<float> highBlock(highBuffer);
        juce::dsp::ProcessContextReplacing<float> highContext1(highBlock);
        highpassFilter1.process(highContext1);
        highpassFilter2.process(highContext1);
    }

    juce::AudioBuffer<float>& getLowBuffer() noexcept { return lowBuffer; }
    const juce::AudioBuffer<float>& getHighBuffer() const noexcept { return highBuffer; }

    // Low（処理済み）とHigh（未処理原音）を再合成してoutputに書き込む
    void combine(juce::AudioBuffer<float>& output, int numSamples)
    {
        jassert(output.getNumChannels() >= 2);
        for (int ch = 0; ch < 2; ++ch)
        {
            output.copyFrom(ch, 0, lowBuffer, ch, 0, numSamples);
            output.addFrom(ch, 0, highBuffer, ch, 0, numSamples);
        }
    }

    // Mid/Side 変換ヘルパー
    static void encodeMidSide(float leftIn, float rightIn, float& midOut, float& sideOut) noexcept
    {
        midOut = 0.5f * (leftIn + rightIn);
        sideOut = 0.5f * (leftIn - rightIn);
    }

    static void decodeMidSide(float midIn, float sideIn, float& leftOut, float& rightOut) noexcept
    {
        leftOut = midIn + sideIn;
        rightOut = midIn - sideIn;
    }

private:
    void updateCoefficients(float freqHz)
    {
        if (currentSampleRate <= 0.0)
            return;

        const float safeFreq = juce::jlimit(10.0f, static_cast<float>(currentSampleRate * 0.49), freqHz);
        constexpr float qVal = 0.7071067811865475f; // 1 / sqrt(2)

        auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, safeFreq, qVal);
        auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, safeFreq, qVal);

        *lowpassFilter1.state = *lpCoeffs;
        *lowpassFilter2.state = *lpCoeffs;
        *highpassFilter1.state = *hpCoeffs;
        *highpassFilter2.state = *hpCoeffs;
    }

    double currentSampleRate = 44100.0;
    float currentFreq = 120.0f;

    // 2chプロセッサ
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowpassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lowpassFilter2;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highpassFilter1;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> highpassFilter2;

    juce::AudioBuffer<float> lowBuffer;
    juce::AudioBuffer<float> highBuffer;
};
