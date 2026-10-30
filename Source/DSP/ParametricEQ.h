// ArSonEQ native DSP core: C++17, no JUCE, allocations, or locks on the audio path.
// New implementation informed by standard RBJ Audio EQ Cookbook biquads and
// MaSonKuPik's 12-48 dB/oct Butterworth Q-stage selections.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>

namespace arson::eq {
constexpr int kMaxBands = 24;
constexpr int kMaxStages = 4;
constexpr double kPi = 3.1415926535897932384626433832795;

enum class Type : int { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, FlatTilt };
enum class Route : int { Stereo, Left, Right, Mid, Side };

struct Band {
    bool enabled = false;
    Type type = Type::Bell;
    Route route = Route::Stereo;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slope = 12;
    double dynamicRangeDb = 0.0; // signed additional gain, -12..+12
    double thresholdDb = -24.0; // band-limited trigger dBFS, -60..0
};
inline bool same(const Band& a, const Band& b) noexcept {
    return a.enabled == b.enabled && a.type == b.type && a.route == b.route
        && a.frequency == b.frequency && a.gainDb == b.gainDb
        && a.q == b.q && a.slope == b.slope
        && a.dynamicRangeDb == b.dynamicRangeDb && a.thresholdDb == b.thresholdDb;
}
inline double finiteClamp(double x, double fallback, double low, double high) noexcept {
    return std::clamp(std::isfinite(x) ? x : fallback, low, high);
}
struct Coeff {
    double b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0;
};
inline Coeff identity() noexcept { return {}; }
inline Coeff coefficients(Type type, double freq, double gainDb, double q, double sr) noexcept {
    freq = finiteClamp(freq, 1000.0, 20.0, 0.45 * sr);
    gainDb = finiteClamp(gainDb, 0.0, -24.0, 24.0);
    q = finiteClamp(q, 1.0, 0.1, 24.0);
    const auto w = 2.0 * kPi * freq / sr;
    const auto s = std::sin(w), c = std::cos(w);
    const auto A = std::pow(10.0, gainDb / 40.0);
    const auto alpha = s / (2.0 * q);
    // RBJ shelving slope S is bounded independently of bell Q to avoid
    // unstable/extremely resonant shelves.
    const auto S = std::clamp(q, 0.25, 1.0);
    const auto shelfAlpha = 0.5 * s * std::sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
    const auto root = 2.0 * std::sqrt(A) * shelfAlpha;
    double b0=1,b1=0,b2=0,a0=1,a1=0,a2=0;
    switch (type) {
        case Type::Bell:
            b0=1+alpha*A; b1=-2*c; b2=1-alpha*A;
            a0=1+alpha/A; a1=-2*c; a2=1-alpha/A; break;
        case Type::BandPass:
            // RBJ constant 0 dB peak bandpass, numerator = alpha.
            b0=alpha; b1=0; b2=-alpha;
            a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case Type::TiltShelf:
        case Type::FlatTilt:
            // These are assembled as two shelves in makeStages().
            return identity();
        case Type::Notch:
            b0=1; b1=-2*c; b2=1;
            a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case Type::LowCut:
            b0=(1+c)/2; b1=-(1+c); b2=(1+c)/2;
            a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case Type::HighCut:
            b0=(1-c)/2; b1=1-c; b2=(1-c)/2;
            a0=1+alpha; a1=-2*c; a2=1-alpha; break;
        case Type::LowShelf:
            b0=A*((A+1)-(A-1)*c+root);
            b1=2*A*((A-1)-(A+1)*c);
            b2=A*((A+1)-(A-1)*c-root);
            a0=(A+1)+(A-1)*c+root;
            a1=-2*((A-1)+(A+1)*c);
            a2=(A+1)+(A-1)*c-root; break;
        case Type::HighShelf:
            b0=A*((A+1)+(A-1)*c+root);
            b1=-2*A*((A-1)+(A+1)*c);
            b2=A*((A+1)+(A-1)*c-root);
            a0=(A+1)-(A-1)*c+root;
            a1=2*((A-1)-(A+1)*c);
            a2=(A+1)-(A-1)*c-root; break;
    }
    if (!std::isfinite(a0) || std::abs(a0) < 1e-15) return identity();
    const double inv = 1.0 / a0;
    Coeff result{b0*inv,b1*inv,b2*inv,a1*inv,a2*inv};
    if (!std::isfinite(result.b0) || !std::isfinite(result.a1)
        || !std::isfinite(result.a2)) return identity();
    return result;
}
inline int stageCount(int slope) noexcept {
    return slope >= 48 ? 4 : slope >= 36 ? 3 : slope >= 24 ? 2 : 1;
}
inline double butterworthQ(int stages, int index) noexcept {
    constexpr double qs[4][4] = {
        {0.7071067811865476, 0, 0, 0},
        {0.541196100146197, 1.306562964876377, 0, 0},
        {0.517638090205041, 0.707106781186548, 1.931851652578137, 0},
        {0.509795579104159, 0.601344886935045, 0.899976223136416, 2.562915447741506}
    };
    return qs[std::clamp(stages,1,4)-1][std::clamp(index,0,3)];
}
inline std::array<Coeff,kMaxStages> makeStages(Band band, double sr) noexcept {
    std::array<Coeff,kMaxStages> result{};
    if (!band.enabled) return result;
    const bool cut = band.type == Type::LowCut || band.type == Type::HighCut;
    const bool tilt = band.type == Type::TiltShelf || band.type == Type::FlatTilt;
    const int count = cut ? stageCount(band.slope) : tilt ? 2 : 1;
    for (int i=0;i<count;++i) {
        const double q = cut ? butterworthQ(count,i) : band.q;
        if (tilt) {
            const bool lo = i==0;
            // Equal and opposite shelves, smooth neutral pivot around frequency.
            // FlatTilt uses a gentler shelf transition than TiltShelf.
            const double tiltQ = band.type==Type::FlatTilt ? 0.70710678 : 0.50;
            result[static_cast<size_t>(i)] = coefficients(
                lo?Type::LowShelf:Type::HighShelf,band.frequency,
                lo?-0.5*band.gainDb:0.5*band.gainDb,tiltQ,sr);
        } else result[static_cast<size_t>(i)] = coefficients(band.type,band.frequency,band.gainDb,q,sr);
    }
    return result;
}
inline double magnitudeDb(const Coeff& c, double freq, double sr) noexcept {
    const auto theta = 2.0*kPi*freq/sr;
    const auto z = std::polar(1.0,-theta), z2=z*z;
    const auto num = c.b0 + c.b1*z + c.b2*z2;
    const auto den = 1.0 + c.a1*z + c.a2*z2;
    return 20.0*std::log10(std::max(std::abs(num)/std::max(std::abs(den),1e-20),1e-20));
}


struct StereoMatrix {
    std::complex<double> ll{1.0,0.0}, lr{0.0,0.0};
    std::complex<double> rl{0.0,0.0}, rr{1.0,0.0};
};

class Engine {
public:
    void prepare(double rate) noexcept {
        sampleRate_ = finiteClamp(rate, 48000.0, 8000.0, 384000.0);
        for (auto& s : slots_) {
            s.primary = makePath(s.requested);
            s.secondary = {};
            s.fadeRemaining = 0;
            s.fadeLength = 0;
        }
    }
    double sampleRate() const noexcept { return sampleRate_; }
    // Audio-thread-owned current detector activity; publish once per block
    // via separate relaxed atomics in the processor, never read Engine cross-thread.
    float dynamicActivity(int index) const noexcept {
        if(!valid(index))return 0.f;
        const auto& slot=slots_[static_cast<size_t>(index)];
        const auto activity=[](const Path& p) noexcept {
            return p.dynamicEnabled?std::max(p.activity[0],p.activity[1]):0.;
        };
        return static_cast<float>(std::clamp(std::max(activity(slot.primary),
            slot.fadeRemaining?activity(slot.secondary):0.),0.,1.));
    }
    Band band(int index) const noexcept {
        return valid(index) ? slots_[static_cast<size_t>(index)].requested : Band{};
    }
    bool setBand(int index, Band value) noexcept {
        if (!valid(index)) return false;
        value.frequency = finiteClamp(value.frequency, 1000.0, 20.0, 0.45 * sampleRate_);
        value.gainDb = finiteClamp(value.gainDb, 0.0, -24.0, 24.0);
        value.q = finiteClamp(value.q, 1.0, 0.1, 24.0);
        value.slope = stageCount(value.slope) * 12;
        value.dynamicRangeDb=finiteClamp(value.dynamicRangeDb,0.,-12.,12.);
        value.thresholdDb=finiteClamp(value.thresholdDb,-24.,-60.,0.);
        if (static_cast<int>(value.type) < 0 || static_cast<int>(value.type) > 8)
            value.type = Type::Bell;
        if (static_cast<int>(value.route) < 0 || static_cast<int>(value.route) > 4)
            value.route = Route::Stereo;
        auto& s = slots_[static_cast<size_t>(index)];
        if (same(s.requested, value)) return false;
        s.requested = value;
        // During a topology crossfade, keep BOTH live branches untouched.
        // Only the latest request is queued; the next change starts after fade.
        // Restarting fades midway creates sharp discontinuities on automation.
        if (s.fadeRemaining == 0) apply(s, value);
        return true;
    }
    void reset() noexcept { prepare(sampleRate_); }
    void process(double& left, double& right) noexcept {
        for (auto& s : slots_) {
            if (s.fadeRemaining == 0) {
                if (!s.primary.band.enabled) continue;
                processStereoPath(s.primary, left, right);
            } else {
                const double inL=left,inR=right;
                double oldL=inL,oldR=inR,newL=inL,newR=inR;
                if (s.primary.band.enabled) processStereoPath(s.primary,oldL,oldR);
                if (s.secondary.band.enabled) processStereoPath(s.secondary,newL,newR);
                const double t=1.0-static_cast<double>(s.fadeRemaining-1)/s.fadeLength;
                const double smooth=t*t*(3.0-2.0*t);
                left=oldL+(newL-oldL)*smooth;
                right=oldR+(newR-oldR)*smooth;
                finishFade(s);
            }
        }
    }
    void processMono(double& sample) noexcept {
        for (auto& s : slots_) {
            if (s.fadeRemaining == 0) {
                if (s.primary.band.enabled) processMonoPath(s.primary,sample);
            } else {
                double oldValue=sample,newValue=sample;
                if (s.primary.band.enabled) processMonoPath(s.primary,oldValue);
                if (s.secondary.band.enabled) processMonoPath(s.secondary,newValue);
                const double t=1.0-static_cast<double>(s.fadeRemaining-1)/s.fadeLength;
                const double smooth=t*t*(3.0-2.0*t);
                sample=oldValue+(newValue-oldValue)*smooth;
                finishFade(s);
            }
        }
    }
    double bandResponseDb(int index, double frequency) const noexcept {
        if (!valid(index)) return 0.0;
        const Band b=slots_[static_cast<size_t>(index)].requested;
        if (!b.enabled) return 0.0;
        const auto coeff=makeStages(b,sampleRate_);
        const auto count=stageCountFor(b);
        double db=0.0;
        for (int i=0;i<count;++i)
            db+=magnitudeDb(coeff[static_cast<size_t>(i)],frequency,sampleRate_);
        return db;
    }
    // The actual steady-state 2x2 matrix includes crossfeed from M/S.
    // A graph showing only stereo-routed bands would conceal active filters.
    StereoMatrix matrixResponse(double frequency,
        const std::array<float,kMaxBands>* dynamicActivity=nullptr) const noexcept {
        StereoMatrix total;
        if (!std::isfinite(frequency) || frequency<=0.0 || frequency>=sampleRate_*0.5)
            return total;
        for(int slotIndex=0;slotIndex<kMaxBands;++slotIndex) {
            const auto& slot=slots_[static_cast<size_t>(slotIndex)];
            const Band b=slot.requested;
            if (!b.enabled) continue;
            std::complex<double> h{1.0,0.0};
            const auto stages=makeStages(b,sampleRate_);
            const auto z=std::polar(1.0,-2.0*kPi*frequency/sampleRate_);
            const auto z2=z*z;
            for(int k=0;k<stageCountFor(b);++k) {
                const auto& c=stages[static_cast<size_t>(k)];
                const auto numerator=c.b0+c.b1*z+c.b2*z2;
                const auto denominator=1.0+c.a1*z+c.a2*z2;
                h*=numerator/denominator;
            }
            if(dynamicActivity!=nullptr && std::abs(b.dynamicRangeDb)>0.0001 &&
               (b.type==Type::Bell||b.type==Type::LowShelf||b.type==Type::HighShelf)) {
                const double wet=std::clamp(double((*dynamicActivity)[static_cast<size_t>(slotIndex)]),0.,1.);
                if(wet>0.0001) {
                    const auto c=coefficients(b.type,b.frequency,b.dynamicRangeDb,b.q,sampleRate_);
                    const auto dynH=(c.b0+c.b1*z+c.b2*z2)/(1.0+c.a1*z+c.a2*z2);
                    h*=1.0+wet*(dynH-1.0);
                }
            }
            StereoMatrix f;
            const auto one=std::complex<double>(1.0,0.0);
            switch (b.route) {
                case Route::Stereo: f.ll=h; f.rr=h; break;
                case Route::Left: f.ll=h; break;
                case Route::Right: f.rr=h; break;
                case Route::Mid:
                    f.ll=(h+one)*0.5;f.rr=f.ll;
                    f.lr=(h-one)*0.5;f.rl=f.lr;break;
                case Route::Side:
                    f.ll=(h+one)*0.5;f.rr=f.ll;
                    f.lr=(one-h)*0.5;f.rl=f.lr;break;
            }
            StereoMatrix next;
            next.ll=f.ll*total.ll+f.lr*total.rl;
            next.lr=f.ll*total.lr+f.lr*total.rr;
            next.rl=f.rl*total.ll+f.rr*total.rl;
            next.rr=f.rl*total.lr+f.rr*total.rr;
            total=next;
        }
        return total;
    }
    // Graph target: diagonal feed responses L->L and R->R.
    // Nonzero lr/rl should be visualised separately as stereo crossfeed.
    std::array<double,2> channelResponseDb(double frequency) const noexcept {
        const auto m=matrixResponse(frequency);
        const auto toDb=[](const std::complex<double>& z) noexcept {
            return 20.0*std::log10(std::max(std::abs(z),1e-20));
        };
        return {toDb(m.ll),toDb(m.rr)};
    }
    double stereoResponseDb(double frequency) const noexcept {
        const auto r=channelResponseDb(frequency);
        return (r[0]+r[1])*0.5;
    }
private:
    struct State { double z1=0.0,z2=0.0; };
    struct Path {
        Band band{};
        std::array<Coeff,kMaxStages> coefficients{};
        std::array<Coeff,kMaxStages> target{};
        std::array<std::array<State,kMaxStages>,2> state{};
        int stages=0;
        int morphRemaining=0;
        Coeff detector{}, dynamicFilter{};
        std::array<State,2> detectorState{}, dynamicState{};
        std::array<double,2> envelope{};
        std::array<double,2> activity{};
        double thresholdLinear=0.0630957;
        double attack=0.,release=0.;
        bool dynamicEnabled=false;
    };
    struct Slot {
        Band requested{};
        Path primary{},secondary{};
        int fadeRemaining=0,fadeLength=0;
    };
    static bool valid(int index) noexcept { return index>=0 && index<kMaxBands; }
    static int stageCountFor(const Band& b) noexcept {
        if(!b.enabled) return 0;
        return b.type==Type::LowCut || b.type==Type::HighCut ? stageCount(b.slope):
               (b.type==Type::TiltShelf || b.type==Type::FlatTilt) ? 2 : 1;
    }
    static bool sameTopology(const Band& a,const Band& b) noexcept {
        return a.enabled==b.enabled && a.type==b.type
            && a.route==b.route && stageCountFor(a)==stageCountFor(b)
            && a.dynamicRangeDb==b.dynamicRangeDb && a.thresholdDb==b.thresholdDb
            // A dynamic detector MUST follow band frequency/Q automation.
            // Force the existing click-free dual-path topology crossfade.
            && (std::abs(a.dynamicRangeDb)<0.0001 ||
                (a.frequency==b.frequency && a.q==b.q));
    }
    Path makePath(const Band& b) const noexcept {
        Path p;
        p.band=b;
        p.stages=stageCountFor(b);
        p.coefficients=p.target=makeStages(b,sampleRate_);
        p.dynamicEnabled=b.enabled && std::abs(b.dynamicRangeDb)>0.0001 &&
            (b.type==Type::Bell || b.type==Type::LowShelf || b.type==Type::HighShelf);
        if(p.dynamicEnabled) {
            p.detector=coefficients(Type::BandPass,b.frequency,0.0,
                std::clamp(b.q,0.3,12.0),sampleRate_);
            p.dynamicFilter=coefficients(b.type,b.frequency,b.dynamicRangeDb,b.q,sampleRate_);
            p.thresholdLinear=std::pow(10.,b.thresholdDb/20.);
            p.attack=std::exp(-1./(0.012*sampleRate_));
            p.release=std::exp(-1./(0.100*sampleRate_));
        }
        return p;
    }
    void apply(Slot& s,const Band& value) noexcept {
        if (sameTopology(s.primary.band,value)) {
            s.primary.band=value;
            if (value.enabled) {
                s.primary.target=makeStages(value,sampleRate_);
                s.primary.morphRemaining=std::max(1,static_cast<int>(sampleRate_*0.012));
            }
            return;
        }
        // Only topology changes run two paths. This also smooths bypass,
        // hard cut-slope switches and L/R/M/S routing changes.
        s.secondary=makePath(value);
        s.fadeLength=std::max(1,static_cast<int>(sampleRate_*0.010));
        s.fadeRemaining=s.fadeLength;
    }
    void finishFade(Slot& s) noexcept {
        if (--s.fadeRemaining!=0) return;
        s.primary=s.secondary;
        s.secondary={};
        if (!same(s.requested,s.primary.band))
            apply(s,s.requested);
    }
    static void advance(Path& p) noexcept {
        if(p.morphRemaining==0) return;
        const double remain=static_cast<double>(p.morphRemaining);
        for(int i=0;i<p.stages;++i) {
            auto& c=p.coefficients[static_cast<size_t>(i)];
            const auto& t=p.target[static_cast<size_t>(i)];
            c.b0+=(t.b0-c.b0)/remain;c.b1+=(t.b1-c.b1)/remain;
            c.b2+=(t.b2-c.b2)/remain;c.a1+=(t.a1-c.a1)/remain;
            c.a2+=(t.a2-c.a2)/remain;
        }
        --p.morphRemaining;
    }
    static double runBiquad(const Coeff& c, State& z, double x) noexcept {
        const double y=c.b0*x+z.z1;
        const double z1=c.b1*x-c.a1*y+z.z2;
        const double z2=c.b2*x-c.a2*y;
        if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){z={};return 0.;}
        z.z1=std::abs(z1)<1.e-24?0.:z1;
        z.z2=std::abs(z2)<1.e-24?0.:z2;
        return y;
    }
    static double runDynamic(Path& p,double pre,double post,int lane) noexcept {
        if(!p.dynamicEnabled)return post;
        const auto k=static_cast<size_t>(lane);
        const double trigger=runBiquad(p.detector,p.detectorState[k],pre);
        const double sq=trigger*trigger;
        // Energy-following envelope: level is root mean squared per lane.
        const double c=sq>p.envelope[k]?p.attack:p.release;
        p.envelope[k]=c*p.envelope[k]+(1.-c)*sq;
        const double rms=std::sqrt(std::max(0.,p.envelope[k]));
        const double threshold=std::max(p.thresholdLinear,1.e-5);
        // Soft onset: starts 6dB below threshold, reaches full at +6dB.
        const double wet=std::clamp((rms/threshold-0.5)/1.5,0.,1.);
        p.activity[k]=wet;
        const double filtered=runBiquad(p.dynamicFilter,p.dynamicState[k],post);
        return post+(filtered-post)*wet;
    }
    static double filter(Path& p,double sample,int lane) noexcept {
        double x=sample;
        for(int i=0;i<p.stages;++i) {
            const auto& c=p.coefficients[static_cast<size_t>(i)];
            auto& z=p.state[static_cast<size_t>(lane)][static_cast<size_t>(i)];
            const double y=c.b0*x+z.z1;
            const double z1=c.b1*x-c.a1*y+z.z2;
            const double z2=c.b2*x-c.a2*y;
            if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)) {
                z={};x=0.0;
            } else {
                z.z1=std::abs(z1)<1e-24?0.0:z1;
                z.z2=std::abs(z2)<1e-24?0.0:z2;
                x=y;
            }
        }
        return x;
    }
    static void processStereoPath(Path& p,double& l,double& r) noexcept {
        advance(p);
        switch(p.band.route) {
            case Route::Stereo: {
                const double a=l,b=r;
                l=runDynamic(p,a,filter(p,l,0),0);
                r=runDynamic(p,b,filter(p,r,1),1);break;
            }
            case Route::Left: {const double a=l;l=runDynamic(p,a,filter(p,l,0),0);break;}
            case Route::Right: {const double a=r;r=runDynamic(p,a,filter(p,r,1),1);break;}
            case Route::Mid: {
                const auto mid=(l+r)*0.5;
                const auto delta=runDynamic(p,mid,filter(p,mid,0),0)-mid;
                l+=delta;r+=delta;break;
            }
            case Route::Side: {
                const auto side=(l-r)*0.5;
                const auto delta=runDynamic(p,side,filter(p,side,0),0)-side;
                l+=delta;r-=delta;break;
            }
        }
    }
    static void processMonoPath(Path& p,double& x) noexcept {
        advance(p);
        if(p.band.route!=Route::Right && p.band.route!=Route::Side) {
            const double pre=x;x=runDynamic(p,pre,filter(p,x,0),0);
        }
    }
    double sampleRate_=48000.0;
    std::array<Slot,kMaxBands> slots_{};
};
} // namespace arson::eq
