#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "DSP/ParametricEQ.h"
#include "DSP/AnalyzerQueue.h"

class ArSonEQAudioProcessor final : public juce::AudioProcessor {
public:
    ArSonEQAudioProcessor();
    ~ArSonEQAudioProcessor() override = default;
    const juce::String getName() const override { return "ArSonEQ"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::String parameterId(int band, const char* field);
    arson::eq::Band readBand(int index) const noexcept;
    // Presets owned by native JUCE, never by volatile WebView localStorage.
    static constexpr int factoryPresetCount=8, userPresetCount=8;
    static constexpr int presetCount=factoryPresetCount+userPresetCount;
    static juce::String presetName(int index);
    bool loadPreset(int index);             // GUI/message thread only
    bool saveUserPreset(int index);         // 8..15 only, never audio thread
    int currentPreset() const noexcept { return currentPreset_; }
    void setPresetModified() noexcept { currentPreset_=-1; }
    double configuredSampleRate() const noexcept { return preparedRate_.load(std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState parameters;
    arson::eq::AnalyzerQueue& analyzer() noexcept { return analyzer_; }
    // GUI-only telemetry: block-latched native dynamic wet values, never UI timers/FFT
    // inside processBlock. Relaxed atomic read/write is sufficient for meters.
    float dynamicActivity(int band) const noexcept {
        return band>=0&&band<arson::eq::kMaxBands?
            dynamics_[static_cast<size_t>(band)].load(std::memory_order_relaxed):0.f;
    }

private:
    struct ParamRefs {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* route = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* dynRange = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void syncBands() noexcept;
    template<typename Sample> void processAudio(juce::AudioBuffer<Sample>&);
    std::array<ParamRefs, arson::eq::kMaxBands> raw_{};
    arson::eq::Engine engine_;
    arson::eq::AnalyzerQueue analyzer_;
    std::atomic<double> preparedRate_{48000.0};
    std::array<std::atomic<float>,arson::eq::kMaxBands> dynamics_{};
    int currentPreset_=-1;
    juce::File userPresetFile(int slot) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArSonEQAudioProcessor)
};
