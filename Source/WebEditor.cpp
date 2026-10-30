#include "WebEditor.h"
#include <BinaryData.h>
#include <cmath>
#include <cstring>

namespace {
constexpr int graphPoints=256;
constexpr int spectrumPoints=arson::eq::SpectrumDisplay::points;
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
    levelsPre_.fill(-120.f);
    levelsPost_.fill(-120.f);
    prepareWindow();
    lastAnalysisTick_=juce::Time::getMillisecondCounter();
    setSize(900,535);
    processor_.analyzer().setEnabled(true);
    browser_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(25);
}
ArSonEQWebEditor::~ArSonEQWebEditor() {
    stopTimer();
    processor_.analyzer().setEnabled(false);
    for(int i=0;i<arson::eq::kMaxBands;++i)
        for(int j=0;j<3;++j)
            if(gestures_[static_cast<size_t>(i)][static_cast<size_t>(j)])
                if(auto* param=processor_.parameters.getParameter(
                    ArSonEQAudioProcessor::parameterId(i,j==0?"freq":j==1?"gain":"q")))
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
    else if(field=="type") {key="type";value=std::round(juce::jlimit(0.f,8.f,value));}
    else if(field=="dynRange") {key="dynRange";value=juce::jlimit(-12.f,12.f,value);}
    else if(field=="dynThreshold") {key="dynThreshold";value=juce::jlimit(-60.f,0.f,value);}
    else if(field=="route") {key="route";value=std::round(juce::jlimit(0.f,4.f,value));}
    else if(field=="slope") {key="slope";value=std::round(juce::jlimit(0.f,3.f,value));}
    else if(field=="enabled") {key="enabled";value=value>.5f?1.f:0.f;}
    if(!key)return; // strict field allow-list, no arbitrary host parameter access
    if(auto* param=processor_.parameters.getParameter(
        ArSonEQAudioProcessor::parameterId(id,key))) {
        processor_.setPresetModified();
        if(useGesture)param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(value));
        if(useGesture)param->endChangeGesture();
    }
}
void ArSonEQWebEditor::gesture(int id,const juce::var& fields,bool begin) {
    if(id<0||id>=arson::eq::kMaxBands||!fields.isArray())return;
    for(const auto& item:*fields.getArray()) {
        const auto field=item.toString();
        const int n=field=="freq"?0:field=="gain"?1:field=="q"?2:-1;
        if(n<0)continue;
        auto& current=gestures_[static_cast<size_t>(id)][static_cast<size_t>(n)];
        if(current==begin)continue;
        if(auto* param=processor_.parameters.getParameter(
            ArSonEQAudioProcessor::parameterId(id,n==0?"freq":n==1?"gain":"q"))) {
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
    if(kind=="presetLoad" || kind=="presetSave") {
        const int index=static_cast<int>(message.getProperty("index",-1));
        const bool ok=kind=="presetLoad"?
            processor_.loadPreset(index):processor_.saveUserPreset(index);
        auto reply=makeObject();
        property(reply,"ok",ok);
        property(reply,"operation",kind);
        property(reply,"index",index);
        property(reply,"name",ArSonEQAudioProcessor::presetName(index));
        browser_.emitEventIfBrowserIsVisible("eq-preset-result",reply);
        if(ok) { selected_=-1;sendState(true); }
        return;
    }
    if(kind=="spectrum") {
        analyzerVisible_=static_cast<bool>(message.getProperty("enabled",true));
        processor_.analyzer().setEnabled(analyzerVisible_);
        if(!analyzerVisible_) {
            arson::eq::MeterFrame old;
            while(processor_.analyzer().pop(old)) {}
            framesSeen_=0;framesSinceFFT_=0;
        }
        return;
    }
    if(kind=="analyzerSettings") {
        const auto field=message.getProperty("field",{}).toString();
        const auto value=message.getProperty("value",0);
        const int v=static_cast<int>(value);
        if(field=="fft" && v>=10 && v<=13) {
            fftOrder_=v;prepareWindow();framesSinceFFT_=0;
            levelsPre_.fill(-120.f);levelsPost_.fill(-120.f);
        } else if(field=="speed" && v>=0 && v<=2) speed_=v;
        else if(field=="range" && (v==60||v==90||v==120)) range_=v;
        else if(field=="tilt" && (static_cast<double>(value)==0. ||
                static_cast<double>(value)==3. ||
                static_cast<double>(value)==4.5 ||
                static_cast<double>(value)==6.))
            tilt_=static_cast<float>(static_cast<double>(value));
        else if(field=="freeze")freeze_=static_cast<bool>(value);
        else if(field=="pre")showPre_=static_cast<bool>(value);
        else if(field=="post")showPost_=static_cast<bool>(value);
        return;
    }
    // Bounded message-thread transactions: one IPC command for an entire
    // multi-band frame, never any work on the real-time audio callback.
    if(kind=="bulkEdit") {
        const auto values=message.getProperty("entries",{});
        if(!values.isArray()||values.getArray()->size()>168)return;
        const bool useGesture=static_cast<bool>(message.getProperty("gesture",true));
        for(const auto& item:*values.getArray()) {
            if(!item.isObject())continue;
            const int slot=static_cast<int>(item.getProperty("id",-1));
            const auto field=item.getProperty("field",{}).toString();
            const float value=static_cast<float>(static_cast<double>(item.getProperty("value",0.)));
            edit(slot,field,value,useGesture);
        }
        return;
    }
    if(kind=="gestureMany") {
        const auto values=message.getProperty("items",{});
        if(!values.isArray()||values.getArray()->size()>arson::eq::kMaxBands)return;
        const bool begin=static_cast<bool>(message.getProperty("begin",false));
        for(const auto& item:*values.getArray()) {
            if(!item.isObject())continue;
            const int slot=static_cast<int>(item.getProperty("id",-1));
            gesture(slot,item.getProperty("fields",{}),begin);
        }
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
        edit(id,"dynRange",0.f,true);edit(id,"dynThreshold",-24.f,true);
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
        property(item,"dynRange",b.dynamicRangeDb);
        property(item,"dynThreshold",b.thresholdDb);
        bands.add(std::move(item));
    }
    property(value,"bands",std::move(bands));
    property(value,"presetIndex",processor_.currentPreset());
    juce::Array<juce::var> presets;
    for(int i=0;i<ArSonEQAudioProcessor::presetCount;++i)
        presets.add(ArSonEQAudioProcessor::presetName(i));
    property(value,"presets",std::move(presets));
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
void ArSonEQWebEditor::prepareWindow() {
    // Window construction is a one-shot editor/configuration operation, not
    // 4 * FFT-size trigonometry on every 25 Hz paint/update.
    const int N=1<<fftOrder_;
    double sum=0.;
    window_.fill(0.f);
    for(int i=0;i<N;++i) {
        const auto w=0.5f*(1.f-std::cos(
            2.f*juce::MathConstants<float>::pi*i/(N-1)));
        window_[static_cast<size_t>(i)]=w;
        sum+=w;
    }
    coherentAmplitude_=std::max(1.,sum*0.5);
}
void ArSonEQWebEditor::sendSpectrum() {
    arson::eq::MeterFrame f;
    int drained=0;
    float peakLeft=0.f,peakRight=0.f;
    while(drained<static_cast<int>(arson::eq::AnalyzerQueue::capacity) &&
          processor_.analyzer().pop(f)) {
        samples_[0][head_]=f.preL;
        samples_[1][head_]=f.preR;
        samples_[2][head_]=f.postL;
        samples_[3][head_]=f.postR;
        peakLeft=std::max(peakLeft,std::abs(f.postL));
        peakRight=std::max(peakRight,std::abs(f.postR));
        head_=(head_+1)&(maxSize_-1);
        ++framesSeen_;++framesSinceFFT_;++drained;
    }
    // Sample peaks are from real native output, not synthetic UI levels.
    peakL_=std::max(peakLeft,peakL_*0.82f);
    peakR_=std::max(peakRight,peakR_*0.82f);
    if(!ready_||!pageLoaded_||framesSeen_<static_cast<uint64_t>(1<<fftOrder_)
       ||framesSinceFFT_<512)return;
    framesSinceFFT_=0;
    const auto tick=juce::Time::getMillisecondCounter();
    const double elapsed=std::clamp(double(tick-lastAnalysisTick_)/1000.,0.001,0.5);
    lastAnalysisTick_=tick;
    if(!freeze_) {
        const int N=1<<fftOrder_;
        const juce::dsp::FFT& fft=fftOrder_==10?fft10_:fftOrder_==11?fft11_:
                                  fftOrder_==12?fft12_:fft13_;
        const double coherentAmplitude=coherentAmplitude_;
        // CPU/render work happens solely on JUCE message thread.
        for(int channel=0;channel<4;++channel) {
            fftScratch_.fill(0.f);
            for(int i=0;i<N;++i) {
                const auto pos=(head_+maxSize_-static_cast<size_t>(N)+static_cast<size_t>(i))&(maxSize_-1);
                fftScratch_[static_cast<size_t>(i)]=
                    samples_[static_cast<size_t>(channel)][pos]*window_[static_cast<size_t>(i)];
            }
            fft.performFrequencyOnlyForwardTransform(fftScratch_.data());
            std::copy_n(fftScratch_.data(),N/2+1,magnitudes_[static_cast<size_t>(channel)].data());
        }
        const auto sr=processor_.configuredSampleRate();
        const bool fast=speed_==0;
        const auto smoothTime=elapsed*(speed_==2?0.46:1.0);
        for(int i=0;i<spectrumPoints;++i) {
            const double hz=arson::eq::SpectrumDisplay::frequency(i);
            const auto prePower=arson::eq::SpectrumDisplay::samplePower(
                magnitudes_[0],magnitudes_[1],N,sr,hz)/(coherentAmplitude*coherentAmplitude);
            const auto postPower=arson::eq::SpectrumDisplay::samplePower(
                magnitudes_[2],magnitudes_[3],N,sr,hz)/(coherentAmplitude*coherentAmplitude);
            const auto targetPre=arson::eq::SpectrumDisplay::db(prePower);
            const auto targetPost=arson::eq::SpectrumDisplay::db(postPower);
            levelsPre_[static_cast<size_t>(i)]=arson::eq::SpectrumDisplay::smooth(
                levelsPre_[static_cast<size_t>(i)],targetPre,smoothTime,fast);
            levelsPost_[static_cast<size_t>(i)]=arson::eq::SpectrumDisplay::smooth(
                levelsPost_[static_cast<size_t>(i)],targetPost,smoothTime,fast);
        }
    }
    juce::Array<juce::var> before,after;
    for(int i=0;i<spectrumPoints;++i) {
        const auto hz=arson::eq::SpectrumDisplay::frequency(i);
        before.add(static_cast<double>(arson::eq::SpectrumDisplay::tilt(
            levelsPre_[static_cast<size_t>(i)],hz,tilt_)));
        after.add(static_cast<double>(arson::eq::SpectrumDisplay::tilt(
            levelsPost_[static_cast<size_t>(i)],hz,tilt_)));
    }
    auto snapshot=makeObject();
    property(snapshot,"pre",std::move(before));
    property(snapshot,"post",std::move(after));
    property(snapshot,"peakL",static_cast<double>(peakL_));
    property(snapshot,"peakR",static_cast<double>(peakR_));
    property(snapshot,"range",range_);
    property(snapshot,"tilt",static_cast<double>(tilt_));
    property(snapshot,"preVisible",showPre_);
    property(snapshot,"postVisible",showPost_);
    property(snapshot,"frozen",freeze_);
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
    if(analyzerVisible_)sendSpectrum();
}
