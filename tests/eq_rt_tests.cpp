#include "../Source/DSP/ParametricEQ.h"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <random>
#include <type_traits>
static std::atomic<unsigned long long> allocationCount{0};
void* operator new(std::size_t size) {
    if(void* p=std::malloc(size)) {
        allocationCount.fetch_add(1,std::memory_order_relaxed);
        return p;
    }
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) {
    if(void* p=std::malloc(n)) {
        allocationCount.fetch_add(1,std::memory_order_relaxed);
        return p;
    }
    throw std::bad_alloc();
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace arson::eq;
static void check(bool v,const char* why) {
    if(!v) {std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}
}
static void settle(Engine& e,int samples=1600) {
    for(int i=0;i<samples;++i) { double a=0,b=0;e.process(a,b); }
}
static double magnitudeAt(Type type,Route route,double hz,double testHz) {
    Engine e;e.prepare(48000);
    Band b;b.enabled=true;b.type=type;b.route=route;
    b.frequency=hz;b.gainDb=9;b.q=1.6;
    e.setBand(0,b);settle(e);
    const int n=48000;
    double realIn=0,imagIn=0,realOut=0,imagOut=0;
    for(int i=0;i<n;++i) {
        const auto theta=2*kPi*testHz*i/48000.0;
        const double x=0.02*std::sin(theta);
        double y=x,z=x;
        e.process(y,z);
        if(i>=n/2) {
            realIn+=x*std::cos(theta);imagIn+=x*std::sin(theta);
            realOut+=y*std::cos(theta);imagOut+=y*std::sin(theta);
        }
    }
    return 20*std::log10(std::hypot(realOut,imagOut)/std::hypot(realIn,imagIn));
}
static void responses() {
    for(const auto f:{100.0,1000.0,5000.0}) {
        Engine e;e.prepare(48000);
        Band b;b.enabled=true;b.type=Type::Bell;b.gainDb=9;
        b.frequency=f;b.q=1.6;
        e.setBand(0,b);
        const double measured=magnitudeAt(Type::Bell,Route::Stereo,f,f);
        const double expected=e.bandResponseDb(0,f);
        check(std::abs(measured-expected)<0.13,"analytic response equals steady sine response");
    }
    Engine e;e.prepare(48000);
    Band b;b.enabled=true;b.type=Type::Bell;b.gainDb=12;b.frequency=1500;b.q=2;
    b.route=Route::Stereo;e.setBand(0,b);
    auto m=e.matrixResponse(1500);
    check(std::abs(m.ll-m.rr)<1e-9,"stereo L/R symmetry");
    check(std::abs(m.lr)<1e-9,"stereo no crossfeed");
    b.route=Route::Mid;e.setBand(0,b);
    m=e.matrixResponse(1500);
    check(std::abs(m.ll-m.rr)<1e-9,"mid balanced diagonal");
    check(std::abs(m.lr)>0.1 && std::abs(m.lr-m.rl)<1e-9,"mid crossfeed");
    const auto mid=m;
    b.route=Route::Side;e.setBand(0,b);
    m=e.matrixResponse(1500);
    check(std::abs(mid.lr+m.lr)<1e-9,"side crossfeed opposite mid");
    // Two-bands matrix is not generally commutative; preserve DSP order.
    Band a=b;a.route=Route::Left;a.gainDb=-6;a.frequency=3000;
    e.setBand(1,a);
    const auto m2=e.matrixResponse(1500);
    check(std::abs(m2.ll-m.ll)>1e-3,"matrix cascades asymmetric L band");
}
static void noAllocAudioPath() {
    Engine e;e.prepare(48000);
    std::mt19937 gen(22);
    for(int i=0;i<kMaxBands;++i) {
        Band b;b.enabled=true;b.frequency=80.0+i*585.0;
        b.type=static_cast<Type>(i%6);
        b.route=static_cast<Route>(i%5);
        b.gainDb=static_cast<double>(i%11)-5;
        b.q=0.4+static_cast<double>(i%7);
        b.slope=(i%4+1)*12;
        e.setBand(i,b);
    }
    const auto before=allocationCount.load(std::memory_order_relaxed);
    double peak=0.0;
    for(int i=0;i<64000;++i) {
        double l=0.003*std::sin(2*kPi*361.0*i/48000.0);
        double r=0.003*std::sin(2*kPi*739.0*i/48000.0);
        if(i%53==0) {
            const int id=(i/53)%kMaxBands;
            auto b=e.band(id);
            b.frequency=150.0+(i*47)%17250;
            b.gainDb=static_cast<double>(i%31)-15;
            b.q=0.1+(i%31)*0.7;
            if(i%371==0) {
                b.type=static_cast<Type>((i/371)%6);
                b.route=static_cast<Route>((i/371)%5);
                b.enabled=((i/371)%3!=0);
                b.slope=12*(1+(i/371)%4);
            }
            e.setBand(id,b);
        }
        e.process(l,r);
        check(std::isfinite(l) && std::isfinite(r),"stress samples must be finite");
        peak=std::max(peak,std::max(std::abs(l),std::abs(r)));
        check(peak<1e4,"stress runaway guard");
    }
    const auto after=allocationCount.load(std::memory_order_relaxed);
    check(before==after,"audio callback allocates no memory (24-band automation stress)");
}
static void topologyContinuity() {
    Engine e;e.prepare(48000);
    Band b;b.enabled=true;b.type=Type::Bell;b.gainDb=12;b.frequency=1000;
    b.q=8;b.route=Route::Stereo;
    e.setBand(0,b);
    constexpr double hz=997.0,amp=0.025;
    double previous=0.0,maxStep=0.0;
    for(int i=0;i<48000;++i) {
        if(i==12000) {b.route=Route::Side;e.setBand(0,b);}
        if(i==16000) {b.route=Route::Mid;e.setBand(0,b);}
        if(i==20000) {b.slope=48;b.type=Type::LowCut;e.setBand(0,b);}
        if(i==24000) {b.type=Type::HighCut;e.setBand(0,b);}
        if(i==28000) {b.enabled=false;e.setBand(0,b);}
        if(i==32000) {b.enabled=true;b.route=Route::Stereo;b.type=Type::Bell;e.setBand(0,b);}
        if(i>=36000 && i<40000 && i%19==0) {
            b.route=static_cast<Route>((i/19)%5);
            b.type=static_cast<Type>((i/19)%6);
            e.setBand(0,b);
        }
        double l=amp*std::sin(2*kPi*hz*i/48000.0);
        double r=l;
        e.process(l,r);
        maxStep=std::max(maxStep,std::abs(l-previous));
        previous=l;
        check(std::isfinite(l)&&std::isfinite(r),"automated topology is finite");
    }
    check(maxStep<0.22,"topology change bounded sample discontinuity");
    std::printf("RT topology max adjacent step: %.7f\n",maxStep);
}
int main() {
    static_assert(std::is_nothrow_move_constructible<Engine>::value,"DSP no-throw");
    responses();
    topologyContinuity();
    noAllocAudioPath();
    std::puts("ArSonEQ RT/transfer tests passed");
}
