#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
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
    static constexpr int fftOrder_=12;
    static constexpr int fftSize_=1<<fftOrder_;
    ArSonEQAudioProcessor& processor_;
    LocalBrowser browser_;
    arson::eq::Engine graphEngine_;
    std::array<arson::eq::Band,arson::eq::kMaxBands> cache_{};
    std::array<std::array<bool,2>,arson::eq::kMaxBands> gestures_{};
    juce::dsp::FFT fft_{fftOrder_};
    std::array<float,fftSize_> pre_{},post_{},window_{};
    std::array<float,2*fftSize_> fftPre_{},fftPost_{};
    std::array<float,fftSize_/2+1> levelsPre_{},levelsPost_{};
    size_t head_=0;
    uint64_t framesSeen_=0;
    int framesSinceFFT_=0;
    int selected_=-1;
    bool ready_=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArSonEQWebEditor)
};