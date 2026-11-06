#include "../Source/DSP/ConsoleChannelEQ.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

// Allocation tripwire covers DSP process() only; setup remains control-side.
static std::atomic<bool> audioActive{false};
static std::atomic<unsigned> audioAllocations{0};
void* operator new(std::size_t n) {
    if (audioActive.load(std::memory_order_relaxed))
        audioAllocations.fetch_add(1,std::memory_order_relaxed);
    if (void* p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    if (audioActive.load(std::memory_order_relaxed))
        audioAllocations.fetch_add(1,std::memory_order_relaxed);
    if (void* p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using arson::eq::console::ConsoleChannelEQ;
using arson::eq::console::Parameters;
using arson::eq::console::Slot;

static void check(bool yes,const char* reason) {
    if (!yes) { std::fprintf(stderr,"Console EQ FAILED: %s\n",reason); std::exit(1); }
}
static void enable(ConsoleChannelEQ& eq,Slot slot,double hz,double db=0,int slope=24) {
    Parameters p=ConsoleChannelEQ::defaultParameters(slot);
    p.enabled=true;p.frequencyHz=hz;p.gainDb=db;p.slopeDbPerOct=slope;
    check(eq.setBand(slot,p),"valid frequency / gain / slope must be accepted");
}

int main() {
    static_assert(ConsoleChannelEQ::addedLatencySamples() == 0,
        "console EQ must not introduce block or lookahead latency");
    static_assert(arson::eq::console::kSlots == 9,"HPF, shelf, 5 PEQ, shelf, LPF");
    ConsoleChannelEQ eq;
    eq.prepare(48000.0);
    for(unsigned k=0;k<9;++k) {
        const auto slot=static_cast<Slot>(k);
        check(!eq.parameters(slot).enabled,"every slot is bypassed by default");
        check(ConsoleChannelEQ::isValid(slot),"slot ID stable");
    }
    check(!ConsoleChannelEQ::isValid(static_cast<Slot>(9)),"reject out of range slot");
    check(!eq.setEnabled(static_cast<Slot>(9),true),"reject invalid enable");
    check(eq.parameters(Slot::HighPass).frequencyHz==80.0,"HPF factory default");
    check(eq.parameters(Slot::LowPass).frequencyHz==18000.0,"LPF factory default");
    float left[4]{0.5f,-0.25f,0.0f,0.123f};
    float right[4]{-0.25f,0.5f,0.33f,0.001f};
    eq.processStereo(left,right,4);
    check(left[0]==0.5f && left[3]==0.123f &&
          right[0]==-0.25f && right[2]==0.33f,"flat default bit transparent");
    auto invalid=ConsoleChannelEQ::defaultParameters(Slot::Bell1);
    invalid.enabled=true;invalid.q=0.0;
    check(!eq.setBand(Slot::Bell1,invalid),"invalid Q fail closed");
    invalid.q=1.;invalid.frequencyHz=30000.;
    check(!eq.setBand(Slot::Bell1,invalid),"frequency above Nyquist guard fails");
    invalid.frequencyHz=1000.;invalid.gainDb=19.;
    check(!eq.setBand(Slot::Bell1,invalid),"excess gain fails");
    invalid.gainDb=6.;
    check(eq.setBand(Slot::Bell1,invalid),"valid bell accepted");
    check(std::abs(eq.bandResponseDb(Slot::Bell1,1000.)-6.)<0.05,
          "PEQ bell has intended +6dB center response");
    check(eq.bandResponseDb(Slot::Bell1,100.)<0.3,"bell has localized response");
    enable(eq,Slot::HighPass,100.,0.,24);
    check(eq.bandResponseDb(Slot::HighPass,20.)<-45.0,
          "HPF 24dB/oct attenuates subbass");
    check(eq.bandResponseDb(Slot::HighPass,5000.)>-0.01,
          "HPF preserves high frequencies");
    enable(eq,Slot::LowPass,1000.,0.,24);
    check(eq.bandResponseDb(Slot::LowPass,5000.)<-50.,
          "LPF 24dB/oct rejects high frequencies");
    check(eq.bandResponseDb(Slot::LowPass,80.)>-0.01,
          "LPF preserves low frequencies");
    Parameters hp=eq.parameters(Slot::HighPass);hp.slopeDbPerOct=36;
    check(!eq.setBand(Slot::HighPass,hp),"only 12/24/48 slopes exposed by console");
    hp.slopeDbPerOct=48;
    check(eq.setBand(Slot::HighPass,hp),"48dB/oct cutoff available");
    check(eq.bandResponseDb(Slot::HighPass,20.)<-90.0,
          "48dB/oct HPF has steep attenuation");

    // A minimum-phase IIR acts at the current sample, no buffering or
    // lookahead. This does NOT imply zero ASIO device roundtrip/group delay.
    ConsoleChannelEQ impulseEq;impulseEq.prepare(48000.);
    enable(impulseEq,Slot::HighPass,80.);
    float impulse[8]{1.,0,0,0,0,0,0,0};
    impulseEq.processMono(impulse,8);
    check(std::isfinite(impulse[0]) && std::abs(impulse[0])>0.0001f,
          "sample-0 impulse output, no block-delayed audio");

    // Stereo L/R stay independent for stereo-routed static filters.
    ConsoleChannelEQ stereo;stereo.prepare(48000.);
    enable(stereo,Slot::Bell3,1000.,6.);
    std::vector<float> l(2048),r(2048);
    for(std::size_t i=0;i<l.size();++i) {
        l[i]=float(0.2*std::sin(2*arson::eq::kPi*1000*i/48000.));
        r[i]=-l[i];
    }
    stereo.processStereo(l.data(),r.data(),l.size());
    for(std::size_t i=0;i<l.size();++i) {
        check(std::isfinite(l[i]) && std::isfinite(r[i]),"finite rendered PCM");
        check(std::abs(l[i]+r[i])<1e-6f,"stereo symmetric, no cross-channel leak");
    }
    // Alias-safe mono path: one pointer must never be processed twice.
    ConsoleChannelEQ monoA,monoB;
    monoA.prepare(48000.);monoB.prepare(48000.);
    enable(monoA,Slot::Bell2,500.,-6.);
    enable(monoB,Slot::Bell2,500.,-6.);
    float a[16]{},b[16]{};
    for(int i=0;i<16;++i) a[i]=b[i]=0.1f*float(i%3-1);
    monoA.processStereo(a,a,16);
    monoB.processMono(b,16);
    for(int i=0;i<16;++i) check(a[i]==b[i],"alias-safe mono equals mono");

    // No heap allocation on the render path at 32/64/128/512 frames.
    std::vector<float> memL(512,0.1f),memR(512,-0.1f);
    for(std::size_t frames: {std::size_t(32),std::size_t(64),
                             std::size_t(128),std::size_t(512)}) {
        audioActive.store(true,std::memory_order_relaxed);
        stereo.processStereo(memL.data(),memR.data(),frames);
        audioActive.store(false,std::memory_order_relaxed);
    }
    check(audioAllocations.load(std::memory_order_relaxed)==0,
          "no audio-thread heap allocations");
    check(stereo.setEnabled(Slot::Bell3,false),"bypass command accepted");
    // Allow the existing 10ms dual-path bypass ramp to complete.
    float silence[1024]{};
    stereo.processMono(silence,1024);
    float clean[1]{0.25f};
    stereo.processMono(clean,1);
    check(std::abs(clean[0]-0.25f)<0.001f,"bypassed band transparent after fade");

    // Retain fixed slots and finite values over common live sample rates.
    for(double rate: {44100.,48000.,96000.}) {
        ConsoleChannelEQ instance;
        instance.prepare(rate);
        enable(instance,Slot::Bell4,3000.,-6.);
        float pcm[128]{};
        pcm[0]=1.0f;
        instance.processMono(pcm,128);
        for(float v:pcm)check(std::isfinite(v),"common sample rates stable");
    }
    std::puts("ArSonEQ isolated 9-slot console EQ tests passed");
}
