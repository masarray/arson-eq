#include "../Source/DSP/ParametricEQ.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
using namespace arson::eq;
static void require(bool ok,const char* msg) {
    if (!ok) { std::fprintf(stderr,"FAILED: %s\n",msg); std::exit(1); }
}
static double responseFor(Band b,double f) {
    Engine e;e.prepare(48000);e.setBand(0,b);
    return e.bandResponseDb(0,f);
}
int main() {
    Engine eq;
    eq.prepare(48000);
    double l=0.14,r=-0.27;eq.process(l,r);
    require(l==0.14&&r==-0.27,"empty processor is bit-transparent");
    Band b; b.enabled=true;b.type=Type::Bell;b.frequency=1000;b.gainDb=6;b.q=1;
    require(std::abs(responseFor(b,1000)-6.0)<0.02,"bell center gain");
    b.gainDb=-12;
    require(std::abs(responseFor(b,1000)+12.0)<0.02,"bell center cut");
    b.gainDb=0;
    require(std::abs(responseFor(b,1000))<1e-8,"flat bell");
    b.type=Type::LowCut;b.frequency=1000;b.slope=24;
    require(responseFor(b,100)<-70,"24 dB/oct deep low cut");
    require(std::abs(responseFor(b,1000)+3.0103)<0.12,"Butterworth cut crossover");
    b.type=Type::HighCut;
    require(responseFor(b,10000)<-65,"24 dB/oct high cut");
    b.type=Type::LowShelf;b.gainDb=6;
    require(responseFor(b,60)>5.5 && std::abs(responseFor(b,12000))<0.1,"low shelf");
    b.type=Type::HighShelf;
    require(responseFor(b,12000)>5.5 && std::abs(responseFor(b,60))<0.1,"high shelf");
    b.type=Type::Notch;b.frequency=1200;
    require(responseFor(b,1200)<-80,"notch center");
    // M3 filter shapes: 6 legacy IDs stable, additions are 6/7/8.
    b.enabled=true;b.route=Route::Stereo;b.frequency=1000;b.q=1.0;
    b.type=Type::BandPass;b.gainDb=0;
    require(std::abs(responseFor(b,1000))<0.15,"bandpass center near unity");
    require(responseFor(b,80)<-12&&responseFor(b,15000)<-12,"bandpass rejects extremes");
    b.type=Type::TiltShelf;b.gainDb=6;
    require(responseFor(b,100)<-2.0&&responseFor(b,10000)>2.0,"tilt signed opposite shelves");
    b.type=Type::FlatTilt;
    require(responseFor(b,100)<-2.0&&responseFor(b,10000)>2.0,"flat tilt shelves");
    // Native real-time dynamics: independent detector and dynamic filter branch,
    // sample-domain threshold should not change static response when inactive.
    auto levelOf=[](double range,double threshold) {
        Engine engine;engine.prepare(48000);
        Band dyn;dyn.enabled=true;dyn.type=Type::Bell;dyn.frequency=1000;
        dyn.gainDb=0;dyn.q=1.;dyn.dynamicRangeDb=range;dyn.thresholdDb=threshold;
        engine.setBand(0,dyn);engine.prepare(48000);
        double power=0;
        for(int i=0;i<48000;++i){
            double x=0.5*std::sin(2*kPi*1000*i/48000);
            engine.processMono(x);
            if(i>=24000)power+=x*x;
            require(std::isfinite(x),"dynamic stays finite");
        }
        return std::sqrt(power/24000.);
    };
    const double dry=levelOf(0.,-40.),duck=levelOf(-6.,-40.);
    const double under=levelOf(-6.,0.);
    require(duck<dry*.82,"dynamic bell attenuates strong band signal");
    require(std::abs(under-dry)<.015,"high manual threshold leaves weak signal unchanged");
    // Existing VST parameter slots never change their filter type meanings.
    require(static_cast<int>(Type::Notch)==5&&static_cast<int>(Type::BandPass)==6,
            "filter type index backward compatibility");
    eq.prepare(48000);
    b.type=Type::Bell;b.gainDb=12;b.frequency=1000;b.route=Route::Mid;
    eq.setBand(0,b);
    // Allow the 8ms smoothing transition to settle.
    for(int i=0;i<2000;++i){double x=0,y=0;eq.process(x,y);}
    l=0.1;r=-0.1;eq.process(l,r);
    require(std::abs(l-0.1)<1e-8 && std::abs(r+0.1)<1e-8,"mid band leaves pure side untouched");
    b.route=Route::Side;eq.setBand(0,b);
    for(int i=0;i<2000;++i){double x=0,y=0;eq.process(x,y);}
    l=0.1;r=0.1;eq.process(l,r);
    require(std::abs(l-0.1)<1e-8 && std::abs(r-0.1)<1e-8,"side band leaves pure mid untouched");
    for(int sr:{44100,48000,96000,192000}) {
        eq.prepare(sr);
        std::mt19937 rng(42);std::uniform_real_distribution<double> dist(-1.0,1.0);
        for(int i=0;i<kMaxBands;++i) {
            Band c; c.enabled=true;c.type=static_cast<Type>(i%6);
            c.route=static_cast<Route>(i%5);
            c.frequency=20+((i*977)%19000);c.gainDb=static_cast<double>(i%21)-10;
            c.q=0.1+(i%12)*0.95;c.slope=12+12*(i%4);
            eq.setBand(i,c);
        }
        for(int i=0;i<sr;++i) {
            double x=dist(rng)*0.1,y=dist(rng)*0.1;eq.process(x,y);
            require(std::isfinite(x)&&std::isfinite(y),"random stress: finite");
            require(std::abs(x)<1e6 && std::abs(y)<1e6,"random stress: bounded");
        }
        eq.reset();
        l=1;r=1;eq.process(l,r);
        require(std::isfinite(l)&&std::isfinite(r),"sample rate reset");
    }
    // Invalid values must be sanitized at the boundary.
    b.frequency=std::numeric_limits<double>::quiet_NaN();
    b.q=std::numeric_limits<double>::infinity();
    b.gainDb=-std::numeric_limits<double>::infinity();
    eq.setBand(0,b);
    require(std::isfinite(eq.band(0).frequency)&&std::isfinite(eq.band(0).q)
        &&std::isfinite(eq.band(0).gainDb),"invalid controls sanitized");
    std::puts("ArSonEQ DSP tests passed");
}
