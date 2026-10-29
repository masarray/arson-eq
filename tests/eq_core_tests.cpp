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
