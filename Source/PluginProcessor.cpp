#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

juce::String ArSonEQAudioProcessor::parameterId(int band, const char* field) {
    return "b" + juce::String(band).paddedLeft('0', 2) + "_" + field;
}

juce::AudioProcessorValueTreeState::ParameterLayout ArSonEQAudioProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray types{"Bell","Low Shelf","High Shelf","Low Cut","High Cut","Notch"};
    const juce::StringArray routes{"Stereo","Left","Right","Mid","Side"};
    const juce::StringArray slopes{"12 dB/oct","24 dB/oct","36 dB/oct","48 dB/oct"};
    juce::NormalisableRange<float> freq(20.0f, 20000.0f);
    freq.setSkewForCentre(1000.0f);
    for (int i=0; i<arson::eq::kMaxBands; ++i) {
        const auto name=juce::String("Band ") + juce::String(i+1) + " ";
        const auto id=[i](const char* field) { return juce::ParameterID(parameterId(i,field),1); };
        layout.add(std::make_unique<juce::AudioParameterBool>(id("enabled"),name+"Enabled",false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id("type"),name+"Type",types,0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id("route"),name+"Route",routes,0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id("freq"),name+"Frequency",freq,1000.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id("gain"),name+"Gain",
            juce::NormalisableRange<float>(-24.0f,24.0f,0.01f),0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id("q"),name+"Q",
            juce::NormalisableRange<float>(0.1f,24.0f,0.0f,0.35f),1.0f));
        layout.add(std::make_unique<juce::AudioParameterChoice>(id("slope"),name+"Cut Slope",slopes,0));
    }
    return layout;
}

ArSonEQAudioProcessor::ArSonEQAudioProcessor()
: AudioProcessor(BusesProperties()
    .withInput("Input",juce::AudioChannelSet::stereo(),true)
    .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  parameters(*this,nullptr,"PARAMETERS",makeLayout()) {
    for (int i=0;i<arson::eq::kMaxBands;++i) {
        auto& p=raw_[static_cast<size_t>(i)];
        p.enabled=parameters.getRawParameterValue(parameterId(i,"enabled"));
        p.type=parameters.getRawParameterValue(parameterId(i,"type"));
        p.route=parameters.getRawParameterValue(parameterId(i,"route"));
        p.freq=parameters.getRawParameterValue(parameterId(i,"freq"));
        p.gain=parameters.getRawParameterValue(parameterId(i,"gain"));
        p.q=parameters.getRawParameterValue(parameterId(i,"q"));
        p.slope=parameters.getRawParameterValue(parameterId(i,"slope"));
        jassert(p.enabled && p.type && p.route && p.freq && p.gain && p.q && p.slope);
    }
    engine_.prepare(48000.0);
}
bool ArSonEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto in=layout.getMainInputChannelSet();
    return (in==juce::AudioChannelSet::mono() || in==juce::AudioChannelSet::stereo())
        && layout.getMainOutputChannelSet()==in;
}
void ArSonEQAudioProcessor::prepareToPlay(double rate,int) {
    preparedRate_.store(rate,std::memory_order_relaxed);
    engine_.prepare(rate);
    syncBands();
    engine_.reset(); // initial state takes effect without fade-in
}
arson::eq::Band ArSonEQAudioProcessor::readBand(int i) const noexcept {
    arson::eq::Band b;
    if (i<0 || i>=arson::eq::kMaxBands) return b;
    const auto& p=raw_[static_cast<size_t>(i)];
    b.enabled=p.enabled->load(std::memory_order_relaxed)>0.5f;
    b.type=static_cast<arson::eq::Type>(static_cast<int>(std::lround(p.type->load(std::memory_order_relaxed))));
    b.route=static_cast<arson::eq::Route>(static_cast<int>(std::lround(p.route->load(std::memory_order_relaxed))));
    b.frequency=p.freq->load(std::memory_order_relaxed);
    b.gainDb=p.gain->load(std::memory_order_relaxed);
    b.q=p.q->load(std::memory_order_relaxed);
    b.slope=12*(1+static_cast<int>(std::lround(p.slope->load(std::memory_order_relaxed))));
    return b;
}
void ArSonEQAudioProcessor::syncBands() noexcept {
    for (int i=0;i<arson::eq::kMaxBands;++i) engine_.setBand(i,readBand(i));
}
template<typename Sample>
void ArSonEQAudioProcessor::processAudio(juce::AudioBuffer<Sample>& audio) {
    juce::ScopedNoDenormals noDenormals;
    syncBands();
    const int channels=audio.getNumChannels(),frames=audio.getNumSamples();
    if (channels<1) return;
    const bool tap=analyzer_.isEnabled();
    auto* left=audio.getWritePointer(0);
    if (channels==1) {
        for (int s=0;s<frames;++s) {
            double x=left[s];
            const float before=static_cast<float>(x);
            engine_.processMono(x);left[s]=static_cast<Sample>(x);
            if (tap) analyzer_.push(before,static_cast<float>(x));
        }
    } else {
        auto* right=audio.getWritePointer(1);
        for (int s=0;s<frames;++s) {
            double l=left[s],r=right[s];
            const float before=tap?static_cast<float>(0.5*(l+r)):0.0f;
            engine_.process(l,r);
            left[s]=static_cast<Sample>(l);right[s]=static_cast<Sample>(r);
            if (tap) analyzer_.push(before,static_cast<float>(0.5*(l+r)));
        }
        for (int ch=2;ch<channels;++ch) audio.clear(ch,0,frames);
    }
}
void ArSonEQAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { processAudio(b); }
void ArSonEQAudioProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer&) { processAudio(b); }
void ArSonEQAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state=parameters.copyState();
    state.setProperty("arsoneq_schema",1,nullptr);
    if (auto xml=state.createXml()) copyXmlToBinary(*xml,destination);
}
void ArSonEQAudioProcessor::setStateInformation(const void* data,int size) {
    if (auto xml=getXmlFromBinary(data,size)) {
        if (xml->hasTagName(parameters.state.getType().toString()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}
juce::AudioProcessorEditor* ArSonEQAudioProcessor::createEditor() {
    return new ArSonEQEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArSonEQAudioProcessor(); }
