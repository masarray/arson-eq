#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "DSP/SpectrumDisplay.h"
#include <cstdint>

// Local-only WebView2/C++ bridge. Audio remains completely native and unaware
// of its editor. No HTTP requests, Web Audio DSP, React, or JS in processBlock.
class ArSonEQWebEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer {
public:
    explicit ArSonEQWebEditor(ArSonEQAudioProcessor&);
    ~ArSonEQWebEditor() override;
    void resized() override;
    static bool supported();
private:
    class LocalBrowser final : public juce::WebBrowserComponent {
    public:
        std::function<void()> onLoaded;
        void pageFinishedLoading(const juce::String&) override {
            if(onLoaded)onLoaded();
        }
        explicit LocalBrowser(const juce::WebBrowserComponent::Options& options)
            : juce::WebBrowserComponent(options) {}
        bool pageAboutToLoad(const juce::String& url) override {
            return url.startsWith(getResourceProviderRoot());
        }
    };
    static std::optional<juce::WebBrowserComponent::Resource> resource(const juce::String&);
    void onCommand(const juce::var&);
    void edit(int, const juce::String&, float, bool gesture);
    void gesture(int, const juce::var&, bool begin);
    void setField(int, const char*, float);
    void sendState(bool response);
    void sendSpectrum();
    void timerCallback() override;
    static constexpr int maxSize_=8192;
    static constexpr int points_=arson::eq::SpectrumDisplay::points;
    ArSonEQAudioProcessor& processor_;
    LocalBrowser browser_;
    arson::eq::Engine graphEngine_;
    std::array<arson::eq::Band,arson::eq::kMaxBands> cache_{};
    std::array<std::array<bool,3>,arson::eq::kMaxBands> gestures_{};
    juce::dsp::FFT fft10_{10},fft11_{11},fft12_{12},fft13_{13};
    // Fixed native UI-side storage, 4 real channels: PreL, PreR, PostL, PostR.
    std::array<std::array<float,maxSize_>,4> samples_{};
    std::array<float,2*maxSize_> fftScratch_{};
    std::array<std::array<float,maxSize_/2+1>,4> magnitudes_{};
    std::array<float,points_> levelsPre_{},levelsPost_{};
    size_t head_=0;
    uint64_t framesSeen_=0;
    int framesSinceFFT_=0,fftOrder_=12,speed_=1,range_=90;
    float tilt_=4.5f,peakL_=0.f,peakR_=0.f;
    bool freeze_=false,showPre_=true,showPost_=true;
    juce::uint32 lastAnalysisTick_=0;
    int selected_=-1;
    bool analyzerVisible_=true;
    bool ready_=false;
    bool pageLoaded_=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArSonEQWebEditor)
};