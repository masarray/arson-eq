#include "WebEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <cstring>

namespace {
constexpr int graphPoints=256;
constexpr int spectrumPoints=224;
constexpr double lowest=20.0,highest=20000.0;

juce::var makeObject() { return juce::var(new juce::DynamicObject()); }
void property(juce::var& object,const char* key,juce::var value) {
    object.getDynamicObject()->setProperty(juce::Identifier(key),std::move(value));
}
juce::WebBrowserComponent::Options browserOptions(
    std::function<void(const juce::var&)> command,
    std::function<std::optional<juce::WebBrowserComponent::Resource>(const juce::String&)> resource) {
    using Web=juce::WebBrowserComponent;
    const auto cache=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("ArSonEQ-WebView2");
    return Web::Options{}
        .withBackend(Web::Options::Backend::webview2)
        .withWinWebView2Options(Web::Options::WinWebView2{}
            .withUserDataFolder(cache)
            .withStatusBarDisabled()
            .withBackgroundColour(juce::Colour(0xff0b0f16)))
        .withNativeIntegrationEnabled()
        .withEventListener("eq-command",[command=std::move(command)](juce::var message) {
            command(message);
        })
        .withResourceProvider(std::move(resource));
}
} // namespace

bool ArSonEQWebEditor::supported() {
    // Do not silently fall back to legacy Internet Explorer: it can't run this
    // modern Canvas UI and would expose a blank / broken editor.
    using Web=juce::WebBrowserComponent;
    if(juce::SystemStats::getEnvironmentVariable(
        "ARSONEQ_USE_NATIVE_GUI","")=="1")
        return false;
    return Web::areOptionsSupported(Web::Options{}
        .withBackend(Web::Options::Backend::webview2));
}

std::optional<juce::WebBrowserComponent::Resource>
ArSonEQWebEditor::resource(const juce::String& url) {
    const char* ptr=nullptr;
    int length=0;
    juce::String mime;
    if(url=="/"||url=="/index.html") {
        ptr=BinaryData::index_html;length=BinaryData::index_htmlSize;mime="text/html; charset=utf-8";
    } else if(url=="/style.css") {
        ptr=BinaryData::style_css;length=BinaryData::style_cssSize;mime="text/css; charset=utf-8";
    } else if(url=="/app.js") {
        ptr=BinaryData::app_js;length=BinaryData::app_jsSize;mime="text/javascript; charset=utf-8";
    } else return std::nullopt;
    if(ptr==nullptr || length<=0) return std::nullopt;
    juce::WebBrowserComponent::Resource result;
    result.data.resize(static_cast<size_t>(length));
    std::memcpy(result.data.data(),ptr,static_cast<size_t>(length));
    result.mimeType=mime;
    return result;
}
ArSonEQWebEditor::ArSonEQWebEditor(ArSonEQAudioProcessor& p)
    : AudioProcessorEditor(p),processor_(p),
      browser_(browserOptions(
          [this](const juce::var& message){ onCommand(message); },
          [](const juce::String& url){ return resource(url); })) {
    setOpaque(true);
    browser_.onLoaded=[this] {
        pageLoaded_=true;
        if(ready_)sendState(true);
    };
    addAndMakeVisible(browser_);
    setResizable(true,true);
    // Full-canvas EQ fits a normal DAW floating plugin window (~Pro-Q aspect).
    setResizeLimits(760,455,1360,850);
    graphEngine_.prepare(p.configuredSampleRate());
    window_.fill(0.0f);
    for(int i=0;i<fftSize_;++i)
        window_[static_cast<size_t>(i)]=0.5f*(1.0f-
            std::cos(2.0f*juce::MathConstants<float>::pi*i/(fftSize_-1)));
    levelsPre_.fill(-108.f);
    levelsPost_.fill(-108.f);
    setSize(900,535);
    processor_.analyzer().setEnabled(true);
    browser_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(25);
}
ArSonEQWebEditor::~ArSonEQWebEditor() {
    stopTimer();
    processor_.analyzer().setEnabled(false);
    for(int i=0;i<arson::eq::kMaxBands;++i)
        for(int j=0;j<2;++j)
            if(gestures_[static_cast<size_t>(i)][static_cast<size_t>(j)])
                if(auto* param=processor_.parameters.getParameter(
                    ArSonEQAudioProcessor::parameterId(i,j==0?"freq":"gain")))
                    param->endChangeGesture();
}
void ArSonEQWebEditor::resized() { browser_.setBounds(getLocalBounds()); }

void ArSonEQWebEditor::setField(int id,const char* field,float value) {
    if(id<0||id>=arson::eq::kMaxBands||!std::isfinite(value))return;
    if(auto* param=processor_.parameters.getParameter(
        ArSonEQAudioProcessor::parameterId(id,field))) {
        const float normal=param->convertTo0to1(value);
        param->beginChangeGesture();
        param->setValueNotifyingHost(normal);
        param->endChangeGesture();
    }
}
void ArSonEQWebEditor::edit(int id,const juce::String& field,float value,bool useGesture) {
    if(id<0||id>=arson::eq::kMaxBands||!std::isfinite(value))return;
    const char* key=nullptr;
    if(field=="freq") {key="freq";value=juce::jlimit(20.f,20000.f,value);}
    else if(field=="gain") {key="gain";value=juce::jlimit(-24.f,24.f,value);}
    else if(field=="q") {key="q";value=juce::jlimit(.1f,24.f,value);}
    else if(field=="type") {key="type";value=std::round(juce::jlimit(0.f,5.f,value));}
    else if(field=="route") {key="route";value=std::round(juce::jlimit(0.f,4.f,value));}
    else if(field=="slope") {key="slope";value=std::round(juce::jlimit(0.f,3.f,value));}
    else if(field=="enabled") {key="enabled";value=value>.5f?1.f:0.f;}
    if(!key)return; // strict field allow-list, no arbitrary host parameter access
    if(auto* param=processor_.parameters.getParameter(
        ArSonEQAudioProcessor::parameterId(id,key))) {
        if(useGesture)param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(value));
        if(useGesture)param->endChangeGesture();
    }
}
void ArSonEQWebEditor::gesture(int id,const juce::var& fields,bool begin) {
    if(id<0||id>=arson::eq::kMaxBands||!fields.isArray())return;
    for(const auto& item:*fields.getArray()) {
        const auto field=item.toString();
        const int n=field=="freq"?0:field=="gain"?1:-1;
        if(n<0)continue;
        auto& current=gestures_[static_cast<size_t>(id)][static_cast<size_t>(n)];
        if(current==begin)continue;
        if(auto* param=processor_.parameters.getParameter(
            ArSonEQAudioProcessor::parameterId(id,n==0?"freq":"gain"))) {
            if(begin)param->beginChangeGesture();
            else param->endChangeGesture();
            current=begin;
        }
    }
}
void ArSonEQWebEditor::onCommand(const juce::var& message) {
    if(!message.isObject())return;
    const auto kind=message.getProperty("kind",{}).toString();
    if(kind=="ready") {
        ready_=true;
        if(pageLoaded_)sendState(true);
        return;
    }
    const int id=static_cast<int>(message.getProperty("id",-1));
    if(kind=="select") {
        selected_=id>=0&&id<arson::eq::kMaxBands?id:-1;
        sendState(true);
    } else if(kind=="begin"||kind=="end") {
        gesture(id,message.getProperty("fields",{}),kind=="begin");
    } else if(kind=="edit") {
        const float value=static_cast<float>(
            static_cast<double>(message.getProperty("value",0.0)));
        edit(id,message.getProperty("field",{}).toString(),value,
            static_cast<bool>(message.getProperty("gesture",true)));
    } else if(kind=="add") {
        const float freq=static_cast<float>(static_cast<double>(message.getProperty("freq",1000.0)));
        const float gain=static_cast<float>(static_cast<double>(message.getProperty("gain",0.0)));
        if(id<0||id>=arson::eq::kMaxBands)return;
        setField(id,"type",0.f);setField(id,"route",0.f);
        setField(id,"q",1.f);setField(id,"slope",0.f);
        edit(id,"freq",freq,true);edit(id,"gain",gain,true);
        edit(id,"enabled",1.f,true);
        selected_=id;
        sendState(true);
    }
}
void ArSonEQWebEditor::sendState(bool response) {
    if(!ready_||!pageLoaded_)return;
    auto value=makeObject();
    property(value,"rate",processor_.configuredSampleRate());
    juce::Array<juce::var> bands;
    for(int i=0;i<arson::eq::kMaxBands;++i) {
        const auto b=processor_.readBand(i);
        auto item=makeObject();
        property(item,"enabled",b.enabled);
        property(item,"type",static_cast<int>(b.type));
        property(item,"route",static_cast<int>(b.route));
        property(item,"freq",b.frequency);
        property(item,"gain",b.gainDb);
        property(item,"q",b.q);
        property(item,"slope",arson::eq::stageCount(b.slope)-1);
        bands.add(std::move(item));
    }
    property(value,"bands",std::move(bands));
    if(response) {
        auto curves=makeObject();
        juce::Array<juce::var> l,r,c,b;
        for(int i=0;i<graphPoints;++i) {
            const double hz=lowest*std::pow(highest/lowest,
                static_cast<double>(i)/(graphPoints-1));
            const auto matrix=graphEngine_.matrixResponse(hz);
            const auto db=[](const std::complex<double>& v) {
                return juce::jlimit(-120.0,40.0,20.0*std::log10(
                    std::max(1.0e-20,std::abs(v))));
            };
            l.add(db(matrix.ll));r.add(db(matrix.rr));
            c.add(db(matrix.lr));
            b.add(selected_>=0?graphEngine_.bandResponseDb(selected_,hz):0.0);
        }
        property(curves,"l",std::move(l));
        property(curves,"r",std::move(r));
        property(curves,"cross",std::move(c));
        property(curves,"band",std::move(b));
        property(value,"curve",std::move(curves));
    }
    browser_.emitEventIfBrowserIsVisible("eq-state",value);
}
void ArSonEQWebEditor::sendSpectrum() {
    arson::eq::MeterFrame f;
    int drained=0;
    while(drained<arson::eq::AnalyzerQueue::capacity &&
        processor_.analyzer().pop(f)) {
        pre_[head_]=f.before;post_[head_]=f.after;
        head_=(head_+1)&(fftSize_-1);
        ++framesSeen_;++framesSinceFFT_;++drained;
    }
    if(!ready_||!pageLoaded_||framesSeen_<fftSize_||framesSinceFFT_<1024)return;
    framesSinceFFT_=0;
    for(int i=0;i<fftSize_;++i) {
        const auto pos=(head_+static_cast<size_t>(i))&(fftSize_-1);
        fftPre_[static_cast<size_t>(i)]=pre_[pos]*window_[static_cast<size_t>(i)];
        fftPost_[static_cast<size_t>(i)]=post_[pos]*window_[static_cast<size_t>(i)];
    }
    fft_.performFrequencyOnlyForwardTransform(fftPre_.data());
    fft_.performFrequencyOnlyForwardTransform(fftPost_.data());
    const auto sr=processor_.configuredSampleRate();
    juce::Array<juce::var> before,after;
    for(int i=0;i<spectrumPoints;++i) {
        const double hz=lowest*std::pow(highest/lowest,
            static_cast<double>(i)/(spectrumPoints-1));
        const int index=juce::jlimit(1,fftSize_/2,static_cast<int>(
            std::round(hz*fftSize_/sr)));
        const auto k=static_cast<size_t>(index);
        const auto magnitude=[](float x) {
            return 20.0f*std::log10(std::max(x*4.f/fftSize_,1.0e-8f));
        };
        const auto a=juce::jlimit(-108.f,0.f,magnitude(fftPre_[k]));
        const auto b=juce::jlimit(-108.f,0.f,magnitude(fftPost_[k]));
        levelsPre_[k]=.67f*levelsPre_[k]+.33f*a;
        levelsPost_[k]=.67f*levelsPost_[k]+.33f*b;
        before.add(static_cast<double>(levelsPre_[k]));
        after.add(static_cast<double>(levelsPost_[k]));
    }
    auto snapshot=makeObject();
    property(snapshot,"pre",std::move(before));
    property(snapshot,"post",std::move(after));
    browser_.emitEventIfBrowserIsVisible("eq-spectrum",snapshot);
}
void ArSonEQWebEditor::timerCallback() {
    bool changed=false;
    const auto rate=processor_.configuredSampleRate();
    if(graphEngine_.sampleRate()!=rate) {
        graphEngine_.prepare(rate);changed=true;
    }
    for(int i=0;i<arson::eq::kMaxBands;++i) {
        const auto b=processor_.readBand(i);
        if(!arson::eq::same(cache_[static_cast<size_t>(i)],b)) {
            cache_[static_cast<size_t>(i)]=b;
            graphEngine_.setBand(i,b);
            changed=true;
        }
    }
    if(changed)sendState(true);
    sendSpectrum();
}
