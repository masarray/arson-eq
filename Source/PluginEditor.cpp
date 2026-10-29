#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour bg(0xff11151c), grid(0xff252d37), ink(0xffc9d3e0);
const juce::Colour accent(0xff52c9df), selectedColor(0xfff2bf79);
constexpr double minHz=20.0,maxHz=20000.0,dbRange=18.0;
constexpr const char* typeNames[]{"Bell","Low Sh","High Sh","Low Cut","High Cut","Notch"};
constexpr const char* routeNames[]{"Stereo","L","R","Mid","Side"};
}
ArSonEQEditor::ArSonEQEditor(ArSonEQAudioProcessor& owner)
    : juce::AudioProcessorEditor(owner),processor_(owner) {
    setResizable(true,true);
    setResizeLimits(680,420,1440,900);
    setSize(930,570);
    setWantsKeyboardFocus(true);
    graphEngine_.prepare(processor_.configuredSampleRate());
    // Repaints only on parameter changes; never animates idle hover or FFT.
    startTimerHz(20);
    timerCallback();
}
juce::Rectangle<float> ArSonEQEditor::graph() const {
    return juce::Rectangle<float>(48.f,73.f,
        static_cast<float>(getWidth())-76.f,static_cast<float>(getHeight())-215.f);
}
float ArSonEQEditor::xForHz(double freq) const {
    const auto r=graph();
    return r.getX()+static_cast<float>(std::log(std::clamp(freq,minHz,maxHz)/minHz)
        /std::log(maxHz/minHz))*r.getWidth();
}
float ArSonEQEditor::yForDb(double db) const {
    const auto r=graph();
    return r.getCentreY()-static_cast<float>(std::clamp(db,-dbRange,dbRange)/dbRange)
        *r.getHeight()*0.5f;
}
double ArSonEQEditor::hzForX(float x) const {
    const auto r=graph();
    return minHz*std::pow(maxHz/minHz,std::clamp(
        static_cast<double>((x-r.getX())/r.getWidth()),0.0,1.0));
}
double ArSonEQEditor::dbForY(float y) const {
    const auto r=graph();
    return std::clamp(static_cast<double>((r.getCentreY()-y)*2/r.getHeight())*dbRange,
        -dbRange,dbRange);
}
void ArSonEQEditor::timerCallback() {
    bool changed=false;
    const auto sr=processor_.configuredSampleRate();
    if (graphEngine_.sampleRate()!=sr) { graphEngine_.prepare(sr);changed=true; }
    for(int i=0;i<arson::eq::kMaxBands;++i) {
        const auto b=processor_.readBand(i);
        if(!arson::eq::same(cached_[static_cast<size_t>(i)],b)) {
            cached_[static_cast<size_t>(i)]=b;
            graphEngine_.setBand(i,b);
            changed=true;
        }
    }
    if(changed) repaint();
}
void ArSonEQEditor::paint(juce::Graphics& g) {
    g.fillAll(bg);
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(17.0f).withStyle("Plain")));
    g.drawText("ARSON EQ",juce::Rectangle<int>(23,15,190,32),juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff748391));g.setFont(11.f);
    g.drawText("PARAMETRIC  /  ZERO LATENCY",juce::Rectangle<int>(205,20,280,22),
        juce::Justification::centredLeft);
    const auto r=graph();
    g.setColour(juce::Colour(0xff161c24));
    g.fillRoundedRectangle(r,6.f);
    const double ticks[]{20,50,100,200,500,1000,2000,5000,10000,20000};
    for(const auto f:ticks) {
        const auto x=xForHz(f);
        g.setColour(grid);g.drawVerticalLine(juce::roundToInt(x),r.getY(),r.getBottom());
        g.setColour(juce::Colour(0xff8b98a8));g.setFont(10.f);
        g.drawText(f>=1000?juce::String(f/1000.0, f==2000||f==5000||f==10000||f==20000?0:1)+"k"
            :juce::String(static_cast<int>(f)),
            juce::Rectangle<float>(x-19,r.getBottom()+4,38,15),juce::Justification::centred);
    }
    for(int db=-18;db<=18;db+=6) {
        const auto y=yForDb(db);
        g.setColour(db==0?juce::Colour(0xff596879):grid);
        g.drawHorizontalLine(juce::roundToInt(y),r.getX(),r.getRight());
        g.setColour(juce::Colour(0xff8b98a8));g.setFont(10.f);
        g.drawText((db>0?"+":"")+juce::String(db),
            juce::Rectangle<float>(7,y-7,32,15),juce::Justification::centredRight);
    }
    juce::Path curve;
    const int points=std::min(400,std::max(120,static_cast<int>(r.getWidth()/2)));
    for(int i=0;i<=points;++i) {
        const auto x=r.getX()+r.getWidth()*static_cast<float>(i)/points;
        const auto y=yForDb(graphEngine_.stereoResponseDb(hzForX(x)));
        if(i==0) curve.startNewSubPath(x,y);else curve.lineTo(x,y);
    }
    g.setColour(accent.withAlpha(0.9f));
    g.strokePath(curve,juce::PathStrokeType(2.f));
    for(int i=0;i<arson::eq::kMaxBands;++i) {
        const auto& b=cached_[static_cast<size_t>(i)];
        if(!b.enabled) continue;
        const auto x=xForHz(b.frequency);
        const auto y=yForDb(b.type==arson::eq::Type::LowCut
            ||b.type==arson::eq::Type::HighCut?0.0:b.gainDb);
        g.setColour(i==selected_?selectedColor:accent.withAlpha(0.78f));
        g.fillEllipse(x-5,y-5,10,10);
        if(i==selected_) { g.setColour(ink);g.drawEllipse(x-8,y-8,16,16,1.f); }
    }
    g.setColour(juce::Colour(0xff7e91a0));g.setFont(11.f);
    g.drawText("Double-click: add / bypass   ·   Drag: frequency + gain   ·   Wheel: Q",
        juce::Rectangle<int>(48,getHeight()-116,getWidth()-96,22),
        juce::Justification::centredLeft);
    drawModeButtons(g);
}
void ArSonEQEditor::drawModeButtons(juce::Graphics& g) {
    const int y=getHeight()-81;
    g.setColour(juce::Colour(0xff7e91a0));g.setFont(11.f);
    if(selected_<0) {
        g.drawText("Select an EQ node to edit its shape and channel.",
            juce::Rectangle<int>(48,y,getWidth()-60,48),juce::Justification::centredLeft);
        return;
    }
    const auto& b=cached_[static_cast<size_t>(selected_)];
    g.drawText("B"+juce::String(selected_+1)+"   "+juce::String(b.frequency,1)+" Hz   "
        +juce::String(b.gainDb,1)+" dB   Q "+juce::String(b.q,2),
        juce::Rectangle<int>(48,y-30,getWidth()-60,25),juce::Justification::centredLeft);
    for(int i=0;i<6;++i) {
        const juce::Rectangle<float> box(48.f+i*84.f,static_cast<float>(y),80.f,27.f);
        const bool active=static_cast<int>(b.type)==i;
        g.setColour(active?juce::Colour(0xff255060):juce::Colour(0xff202832));
        g.fillRoundedRectangle(box,4.f);
        g.setColour(active?accent:ink);g.setFont(11.f);
        g.drawText(typeNames[i],box,juce::Justification::centred);
    }
    for(int i=0;i<5;++i) {
        const juce::Rectangle<float> box(48.f+i*67.f,static_cast<float>(y+32),63.f,25.f);
        const bool active=static_cast<int>(b.route)==i;
        g.setColour(active?juce::Colour(0xff255060):juce::Colour(0xff202832));
        g.fillRoundedRectangle(box,4.f);
        g.setColour(active?accent:ink);g.setFont(11.f);
        g.drawText(routeNames[i],box,juce::Justification::centred);
    }
    g.setColour(juce::Colour(0xff738799));g.setFont(10.f);
    g.drawText("Cut slope: wheel on node",juce::Rectangle<int>(
        410,y+33,getWidth()-450,22),juce::Justification::centredRight);
}
bool ArSonEQEditor::modeClick(juce::Point<float> pos) {
    if(selected_<0) return false;
    const int y=getHeight()-81;
    for(int i=0;i<6;++i)
        if(juce::Rectangle<float>(48.f+i*84.f,static_cast<float>(y),80.f,27.f).contains(pos)) {
            setField(selected_,"type",static_cast<float>(i));return true;
        }
    for(int i=0;i<5;++i)
        if(juce::Rectangle<float>(48.f+i*67.f,static_cast<float>(y+32),63.f,25.f).contains(pos)) {
            setField(selected_,"route",static_cast<float>(i));return true;
        }
    return false;
}
int ArSonEQEditor::hitBand(juce::Point<float> pos) const {
    for(int i=arson::eq::kMaxBands-1;i>=0;--i) {
        const auto& b=cached_[static_cast<size_t>(i)];
        if(!b.enabled) continue;
        const auto x=xForHz(b.frequency), y=yForDb(
            b.type==arson::eq::Type::LowCut||b.type==arson::eq::Type::HighCut?0:b.gainDb);
        if(std::hypot(pos.x-x,pos.y-y)<13.f) return i;
    }
    return -1;
}
void ArSonEQEditor::setField(int slot,const char* field,float value,bool gesture) {
    if(slot<0) return;
    if(auto* p=processor_.parameters.getParameter(ArSonEQAudioProcessor::parameterId(slot,field))) {
        if(gesture) p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(value));
        if(gesture) p->endChangeGesture();
    }
    timerCallback();
}
void ArSonEQEditor::beginDrag() {
    if(dragging_<0||gestureOpen_) return;
    for(const char* field:{"freq","gain"})
        if(auto* p=processor_.parameters.getParameter(
            ArSonEQAudioProcessor::parameterId(dragging_,field))) p->beginChangeGesture();
    gestureOpen_=true;
}
void ArSonEQEditor::endDrag() {
    if(!gestureOpen_) return;
    for(const char* field:{"freq","gain"})
        if(auto* p=processor_.parameters.getParameter(
            ArSonEQAudioProcessor::parameterId(dragging_,field))) p->endChangeGesture();
    gestureOpen_=false;
}
void ArSonEQEditor::mouseDown(const juce::MouseEvent& e) {
    const auto pos=e.position;
    if(modeClick(pos)) return;
    selected_=hitBand(pos);dragging_=selected_;
    if(dragging_>=0) beginDrag();
    repaint();
}
void ArSonEQEditor::mouseDrag(const juce::MouseEvent& e) {
    if(dragging_<0 || !gestureOpen_) return;
    setField(dragging_,"freq",static_cast<float>(hzForX(e.position.x)),false);
    const auto t=cached_[static_cast<size_t>(dragging_)].type;
    if(t!=arson::eq::Type::LowCut&&t!=arson::eq::Type::HighCut)
        setField(dragging_,"gain",static_cast<float>(dbForY(e.position.y)),false);
}
void ArSonEQEditor::mouseUp(const juce::MouseEvent&) {
    endDrag(); dragging_=-1;
}
void ArSonEQEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if(!graph().contains(e.position)) return;
    const auto hit=hitBand(e.position);
    if(hit>=0) {
        setField(hit,"enabled",0.f);if(selected_==hit) selected_=-1;
    } else {
        for(int i=0;i<arson::eq::kMaxBands;++i) {
            if(cached_[static_cast<size_t>(i)].enabled) continue;
            setField(i,"type",0.f);
            setField(i,"freq",static_cast<float>(hzForX(e.position.x)));
            setField(i,"gain",static_cast<float>(dbForY(e.position.y)));
            setField(i,"q",1.f);
            setField(i,"route",0.f);
            setField(i,"enabled",1.f);
            selected_=i;break;
        }
    }
    repaint();
}
bool ArSonEQEditor::keyPressed(const juce::KeyPress& k) {
    if(selected_<0) return false;
    if(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey) {
        setField(selected_,"enabled",0.f);selected_=-1;return true;
    }
    return false;
}
void ArSonEQEditor::mouseWheelMove(const juce::MouseEvent& e,
    const juce::MouseWheelDetails& wheel) {
    const auto hit=hitBand(e.position);
    const int slot=hit>=0?hit:selected_;
    if(slot<0) return;
    const auto& b=cached_[static_cast<size_t>(slot)];
    if(b.type==arson::eq::Type::LowCut||b.type==arson::eq::Type::HighCut) {
        const int slope=std::clamp((b.slope/12-1)+(wheel.deltaY>0?1:-1),0,3);
        setField(slot,"slope",static_cast<float>(slope));
    } else {
        setField(slot,"q",static_cast<float>(
            std::clamp(b.q*(wheel.deltaY>0?1.15:1/1.15),0.1,24.0)));
    }
}
