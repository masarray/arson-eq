#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "PluginProcessor.h"

class ArSonEQEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ArSonEQEditor(ArSonEQAudioProcessor&);
    ~ArSonEQEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    void updateSpectrum();
    void drawSpectrum(juce::Graphics&,juce::Rectangle<float>);
    void rebuildResponseCurves();
    juce::Rectangle<float> graph() const;
    int hitBand(juce::Point<float>) const;
    float xForHz(double) const;
    float yForDb(double) const;
    double hzForX(float) const;
    double dbForY(float) const;
    void setField(int slot,const char* field,float value,bool withGesture=true);
    void beginDrag();
    void endDrag();
    void drawModeButtons(juce::Graphics&);
    bool modeClick(juce::Point<float>);
    ArSonEQAudioProcessor& processor_;
    arson::eq::Engine graphEngine_;
    std::array<arson::eq::Band,arson::eq::kMaxBands> cached_{};
    static constexpr int fftOrder_=12;
    static constexpr int fftSize_=1<<fftOrder_;
    juce::dsp::FFT fft_;
    std::array<float,fftSize_> preHistory_{},postHistory_{},window_{};
    std::array<float,fftSize_*2> fftPre_{},fftPost_{};
    std::array<float,fftSize_/2+1> levelsPre_{},levelsPost_{};
    size_t historyPosition_=0;
    uint64_t framesSeen_=0;
    bool spectrumReady_=false;
    juce::Path curveLeft_,curveRight_,curveCrossfeed_;
    int selected_=-1,dragging_=-1;
    bool gestureOpen_=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArSonEQEditor)
};
