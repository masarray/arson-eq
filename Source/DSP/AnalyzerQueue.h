// Lock-free SPSC PCM telemetry. Exactly one audio producer, one UI consumer.
// No allocation/FFT/locks in the audio callback; dropped visual frames never block audio.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace arson::eq {
struct MeterFrame { float preL=0.f,preR=0.f,postL=0.f,postR=0.f; };
class AnalyzerQueue {
public:
    static constexpr uint32_t capacity=16384;
    void setEnabled(bool value) noexcept { enabled_.store(value,std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled_.load(std::memory_order_acquire); }
    bool push(float preL,float preR,float postL,float postR) noexcept {
        const auto w=write_.load(std::memory_order_relaxed);
        if(w-read_.load(std::memory_order_acquire)>=capacity) return false;
        frames_[w&(capacity-1)]={preL,preR,postL,postR};
        write_.store(w+1,std::memory_order_release);
        return true;
    }
    // Compatibility: mono input is represented as dual mono telemetry.
    bool push(float before,float after) noexcept {
        return push(before,before,after,after);
    }
    bool pop(MeterFrame& out) noexcept {
        const auto r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire))return false;
        out=frames_[r&(capacity-1)];
        read_.store(r+1,std::memory_order_release);
        return true;
    }
private:
    static_assert((capacity&(capacity-1))==0,"ring size power of two");
    std::array<MeterFrame,capacity> frames_{};
    std::atomic<uint32_t> write_{0},read_{0};
    std::atomic<bool> enabled_{false};
};
}
