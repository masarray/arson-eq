#include "PluginProcessor.h"
#include "PluginEditor.h"
#if JUCE_WINDOWS
#include "WebEditor.h"
#endif
#include <cmath>

juce::String ArSonEQAudioProcessor::parameterId(int band, const char* field) {
    return "b" + juce::String(band).paddedLeft('0', 2) + "_" + field;
}

juce::AudioProcessorValueTreeState::ParameterLayout ArSonEQAudioProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray types{"Bell","Low Shelf","High Shelf","Low Cut","High Cut","Notch",
        "Band Pass","Tilt Shelf","Flat Tilt"};
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
        // Append new IDs; never renumber legacy b00..b23 filter controls.
        layout.add(std::make_unique<juce::AudioParameterFloat>(id("dynRange"),name+"Dynamic Range",
            juce::NormalisableRange<float>(-12.0f,12.0f,0.01f),0.f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(id("dynThreshold"),name+"Dynamic Threshold",
            juce::NormalisableRange<float>(-60.f,0.f,0.1f),-24.f));
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
        p.dynRange=parameters.getRawParameterValue(parameterId(i,"dynRange"));
        p.dynThreshold=parameters.getRawParameterValue(parameterId(i,"dynThreshold"));
        jassert(p.enabled && p.type && p.route && p.freq && p.gain && p.q && p.slope
            && p.dynRange && p.dynThreshold);
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
    b.dynamicRangeDb=p.dynRange->load(std::memory_order_relaxed);
    b.thresholdDb=p.dynThreshold->load(std::memory_order_relaxed);
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
            const float preL=tap?static_cast<float>(l):0.f;
            const float preR=tap?static_cast<float>(r):0.f;
            engine_.process(l,r);
            left[s]=static_cast<Sample>(l);right[s]=static_cast<Sample>(r);
            if (tap) analyzer_.push(preL,preR,static_cast<float>(l),static_cast<float>(r));
        }
        for (int ch=2;ch<channels;++ch) audio.clear(ch,0,frames);
    }
    // One bounded telemetry publication per audio block, never per audio sample.
    for(int band=0;band<arson::eq::kMaxBands;++band)
        dynamics_[static_cast<size_t>(band)].store(engine_.dynamicActivity(band),
                                                  std::memory_order_relaxed);
}
void ArSonEQAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { processAudio(b); }
void ArSonEQAudioProcessor::processBlock(juce::AudioBuffer<double>& b,juce::MidiBuffer&) { processAudio(b); }
void ArSonEQAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state=parameters.copyState();
    state.setProperty("arsoneq_schema",2,nullptr);
    if (auto xml=state.createXml()) copyXmlToBinary(*xml,destination);
}
void ArSonEQAudioProcessor::setStateInformation(const void* data,int size) {
    if (auto xml=getXmlFromBinary(data,size)) {
        if (xml->hasTagName(parameters.state.getType().toString())) {
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
            currentPreset_=-1;
        }
    }
}
juce::AudioProcessorEditor* ArSonEQAudioProcessor::createEditor() {
#if JUCE_WINDOWS
    // Prefer MaSonKuPik-inspired zero-framework WebView2 GUI. Never force
    // Internet Explorer as a fallback; retain the proven JUCE native editor.
    if (ArSonEQWebEditor::supported())
        return new ArSonEQWebEditor(*this);
#endif
    return new ArSonEQEditor(*this);
}

namespace {
struct FactoryBand {
    int slot; int type; double hz,gain,q; int route=0; double dyn=0,thr=-24;
};
void applyValue(juce::AudioProcessorValueTreeState& state,int slot,
                const char* field,float value) {
    auto* p=state.getParameter(ArSonEQAudioProcessor::parameterId(slot,field));
    if(!p)return;
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(value));
    p->endChangeGesture();
}
} // namespace

juce::String ArSonEQAudioProcessor::presetName(int index) {
    static const char* labels[]{
        "Init / Flat","Clean Vocal","Vocal Presence","Warm Vocal",
        "De-Esser Dynamic","Tight Kick","Balanced Master","Air & Sparkle",
        "User 01","User 02","User 03","User 04",
        "User 05","User 06","User 07","User 08"
    };
    return index>=0&&index<presetCount?juce::String(labels[index]):juce::String("Custom");
}
juce::File ArSonEQAudioProcessor::userPresetFile(int index) const {
    auto directory=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ArSon Audio").getChildFile("ArSonEQ").getChildFile("Presets");
    return directory.getChildFile("User-"+juce::String(index-factoryPresetCount+1).paddedLeft('0',2)+".xml");
}
bool ArSonEQAudioProcessor::saveUserPreset(int index) {
    if(index<factoryPresetCount||index>=presetCount)return false;
    const auto file=userPresetFile(index);
    if(file.getParentDirectory().createDirectory().failed())return false;
    auto state=parameters.copyState();
    state.setProperty("arsoneq_schema",2,nullptr);
    auto xml=state.createXml();
    if(xml==nullptr)return false;
    const bool ok=xml->writeTo(file);
    if(ok)currentPreset_=index;
    return ok;
}
bool ArSonEQAudioProcessor::loadPreset(int index) {
    if(index<0||index>=presetCount)return false;
    if(index>=factoryPresetCount) {
        const auto file=userPresetFile(index);
        if(!file.existsAsFile()||file.getSize()>256*1024)return false;
        const auto xml=juce::XmlDocument::parse(file);
        if(xml==nullptr||!xml->hasTagName(parameters.state.getType().toString()))return false;
        const auto tree=juce::ValueTree::fromXml(*xml);
        if(!tree.isValid())return false;
        parameters.replaceState(tree);
        currentPreset_=index;
        return true;
    }
    // Reset to a neutral, deterministic set before applying selected recipe.
    // All changes are on the message thread; audio callback only reads atomics.
    for(int i=0;i<arson::eq::kMaxBands;++i) {
        applyValue(parameters,i,"enabled",0.f);
        applyValue(parameters,i,"type",0.f);
        applyValue(parameters,i,"route",0.f);
        applyValue(parameters,i,"freq",1000.f);
        applyValue(parameters,i,"gain",0.f);
        applyValue(parameters,i,"q",1.f);
        applyValue(parameters,i,"slope",0.f);
        applyValue(parameters,i,"dynRange",0.f);
        applyValue(parameters,i,"dynThreshold",-24.f);
    }
    const auto add=[this](const FactoryBand& b) {
        applyValue(parameters,b.slot,"enabled",1.f);
        applyValue(parameters,b.slot,"type",static_cast<float>(b.type));
        applyValue(parameters,b.slot,"freq",static_cast<float>(b.hz));
        applyValue(parameters,b.slot,"gain",static_cast<float>(b.gain));
        applyValue(parameters,b.slot,"q",static_cast<float>(b.q));
        applyValue(parameters,b.slot,"route",static_cast<float>(b.route));
        applyValue(parameters,b.slot,"dynRange",static_cast<float>(b.dyn));
        applyValue(parameters,b.slot,"dynThreshold",static_cast<float>(b.thr));
    };
    switch(index) {
    case 0: break; // flat
    case 1: add({0,3,75,0,0.707});add({1,0,260,-2.2,1.1});add({2,0,3400,1.7,.9});break;
    case 2: add({0,3,85,0,.707});add({1,0,310,-2.5,1.2});
            add({2,0,2800,2.5,.95});add({3,2,11000,1.5,.65});break;
    case 3: add({0,1,140,2.2,.7});add({1,0,320,-1.8,1.15});
            add({2,2,9600,1.2,.65});break;
    case 4: add({0,3,90,0,.707});add({1,0,6800,0,3.0,0,-7,-34});
            add({2,0,3100,1.,.9});break;
    case 5: add({0,3,30,0,.707});add({1,0,65,2.5,1.2});
            add({2,0,290,-3.0,1.3});add({3,0,3600,1.5,.9});break;
    case 6: add({0,1,80,1.0,.7});add({1,0,260,-1.1,1.0});
            add({2,0,2700,0.8,.85});add({3,2,13500,1.3,.7});break;
    case 7: add({0,3,75,0,.707});add({1,2,14000,2.8,.7});
            add({2,0,4900,1.4,.8});break;
    default:break;
    }
    currentPreset_=index;
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ArSonEQAudioProcessor(); }
