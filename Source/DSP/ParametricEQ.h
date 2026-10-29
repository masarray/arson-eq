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

enum class Type : int { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch };
enum class Route : int { Stereo, Left, Right, Mid, Side };

struct Band {
    bool enabled = false;
    Type type = Type::Bell;
    Route route = Route::Stereo;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slope = 12;
};
inline bool same(const Band& a, const Band& b) noexcept {
    return a.enabled == b.enabled && a.type == b.type && a.route == b.route
        && a.frequency == b.frequency && a.gainDb == b.gainDb
        && a.q == b.q && a.slope == b.slope;
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
    const int count = cut ? stageCount(band.slope) : 1;
    for (int i=0;i<count;++i) {
        const double q = cut ? butterworthQ(count,i) : band.q;
        result[static_cast<size_t>(i)] = coefficients(band.type,band.frequency,band.gainDb,q,sr);
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

class Engine {
public:
    void prepare(double rate) noexcept {
        sampleRate_ = finiteClamp(rate,48000.0,8000.0,384000.0);
        for (int i=0;i<kMaxBands;++i) {
            auto& slot=slots_[static_cast<size_t>(i)];
            slot.state={};
            slot.current=slot.target=makeStages(slot.band,sampleRate_);
            slot.remaining=0;
            slot.active=slot.band.enabled;
            slot.stages=configuredStages(slot.band);
        }
    }
    double sampleRate() const noexcept { return sampleRate_; }
    Band band(int index) const noexcept { return valid(index) ? slots_[static_cast<size_t>(index)].band : Band{}; }
    bool setBand(int index, Band value) noexcept {
        if (!valid(index)) return false;
        value.frequency=finiteClamp(value.frequency,1000.0,20.0,0.45*sampleRate_);
        value.gainDb=finiteClamp(value.gainDb,0.0,-24.0,24.0);
        value.q=finiteClamp(value.q,1.0,0.1,24.0);
        value.slope=stageCount(value.slope)*12;
        if (static_cast<int>(value.type)<0 || static_cast<int>(value.type)>5) value.type=Type::Bell;
        if (static_cast<int>(value.route)<0 || static_cast<int>(value.route)>4) value.route=Route::Stereo;
        auto& slot=slots_[static_cast<size_t>(index)];
        if (same(slot.band,value)) return false;
        slot.band=value;
        slot.stages=std::max(slot.stages,configuredStages(value));
        slot.target=makeStages(value,sampleRate_);
        slot.remaining=std::max(1,static_cast<int>(std::ceil(sampleRate_*0.008)));
        slot.active=true; // keep processing during smooth disable transition
        return true;
    }
    void reset() noexcept {
        for (auto& slot:slots_) {
            slot.state={};
            slot.current=slot.target=makeStages(slot.band,sampleRate_);
            slot.remaining=0;slot.active=slot.band.enabled;
            slot.stages=configuredStages(slot.band);
        }
    }
    // Called once per stereo sample: no allocations, no transcendental math.
    void process(double& left, double& right) noexcept {
        for (auto& slot:slots_) {
            if (!slot.active) continue;
            advance(slot);
            switch (slot.band.route) {
                case Route::Stereo: left=filter(slot,left,0);right=filter(slot,right,1);break;
                case Route::Left: left=filter(slot,left,0);break;
                case Route::Right: right=filter(slot,right,1);break;
                case Route::Mid: {
                    const auto mid=0.5*(left+right);
                    const auto d=filter(slot,mid,0)-mid;
                    left+=d;right+=d;break;
                }
                case Route::Side: {
                    const auto side=0.5*(left-right);
                    const auto d=filter(slot,side,0)-side;
                    left+=d;right-=d;break;
                }
            }
        }
    }
    void processMono(double& sample) noexcept {
        for (auto& slot:slots_) {
            if (!slot.active) continue;
            advance(slot);
            if (slot.band.route==Route::Right || slot.band.route==Route::Side) continue;
            sample=filter(slot,sample,0);
        }
    }
    // Exact steady-state transfer for a single selected band (not temporal smoothing).
    double bandResponseDb(int index, double frequency) const noexcept {
        if (!valid(index) || !slots_[static_cast<size_t>(index)].band.enabled) return 0.0;
        const auto& s=slots_[static_cast<size_t>(index)];
        const auto coeff=makeStages(s.band,sampleRate_);
        double db=0;
        for (const auto& c:coeff) db+=magnitudeDb(c,frequency,sampleRate_);
        return db;
    }
    // Useful for a stereo EQ curve. Mid/Side/one-channel responses are exposed
    // separately through bandResponseDb and must not be misrepresented as a
    // single universal L/R curve.
    double stereoResponseDb(double frequency) const noexcept {
        double db=0;
        for (int i=0;i<kMaxBands;++i)
            if (slots_[static_cast<size_t>(i)].band.enabled
                && slots_[static_cast<size_t>(i)].band.route==Route::Stereo)
                db+=bandResponseDb(i,frequency);
        return db;
    }
private:
    struct State { double z1=0,z2=0; };
    struct Slot {
        Band band{};
        std::array<Coeff,kMaxStages> current{},target{};
        std::array<std::array<State,kMaxStages>,2> state{};
        int remaining=0;
        int stages=0;
        bool active=false;
    };
    static int configuredStages(const Band& b) noexcept {
        if (!b.enabled) return 0;
        return b.type==Type::LowCut || b.type==Type::HighCut ? stageCount(b.slope) : 1;
    }
    static bool valid(int index) noexcept { return index>=0 && index<kMaxBands; }
    static void advance(Slot& s) noexcept {
        if (s.remaining<=0) return;
        const auto r=static_cast<double>(s.remaining);
        for (int i=0;i<s.stages;++i) {
            auto& c=s.current[static_cast<size_t>(i)];
            const auto& t=s.target[static_cast<size_t>(i)];
            c.b0+=(t.b0-c.b0)/r; c.b1+=(t.b1-c.b1)/r;
            c.b2+=(t.b2-c.b2)/r; c.a1+=(t.a1-c.a1)/r; c.a2+=(t.a2-c.a2)/r;
        }
        --s.remaining;
        if (s.remaining==0) {
            const int next=configuredStages(s.band);
            for (int lane=0;lane<2;++lane)
                for(int i=next;i<s.stages;++i)
                    s.state[static_cast<size_t>(lane)][static_cast<size_t>(i)]={};
            s.stages=next;
            if (!s.band.enabled) { s.active=false;s.state={}; }
        }
    }
    static double filter(Slot& s, double input, int lane) noexcept {
        auto value=input;
        for (int i=0;i<s.stages;++i) {
            const auto& c=s.current[static_cast<size_t>(i)];
            auto& z=s.state[static_cast<size_t>(lane)][static_cast<size_t>(i)];
            const auto out=c.b0*value+z.z1;
            const auto next1=c.b1*value-c.a1*out+z.z2;
            const auto next2=c.b2*value-c.a2*out;
            if (!std::isfinite(out) || !std::isfinite(next1) || !std::isfinite(next2)) {
                z={};value=0.0;
            } else {
                // Prevent long-lived denormals on silence without extra allocation.
                z.z1=std::abs(next1)<1e-24 ? 0.0 : next1;
                z.z2=std::abs(next2)<1e-24 ? 0.0 : next2;
                value=out;
            }
        }
        return value;
    }
    double sampleRate_=48000.0;
    std::array<Slot,kMaxBands> slots_{};
};
} // namespace arson::eq
