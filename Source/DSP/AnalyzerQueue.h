// Single-producer audio-thread / single-consumer editor-thread telemetry.
// Never blocks or allocates on the real-time thread. Old frames are dropped
// when the editor falls behind; audio processing is never delayed.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace arson::eq {
struct MeterFrame { float before=0.0f, after=0.0f; };
class AnalyzerQueue {
public:
    static constexpr uint32_t capacity=16384;
    void setEnabled(bool value) noexcept {
        enabled_.store(value,std::memory_order_release);
    }
    bool isEnabled() const noexcept {
        return enabled_.load(std::memory_order_acquire);
    }
    bool push(float before,float after) noexcept {
        const auto write=write_.load(std::memory_order_relaxed);
        if(write-read_.load(std::memory_order_acquire)>=capacity)
            return false;
        frames_[write & (capacity-1)]={before,after};
        write_.store(write+1,std::memory_order_release);
        return true;
    }
    bool pop(MeterFrame& sample) noexcept {
        const auto read=read_.load(std::memory_order_relaxed);
        if(read==write_.load(std::memory_order_acquire)) return false;
        sample=frames_[read & (capacity-1)];
        read_.store(read+1,std::memory_order_release);
        return true;
    }
private:
    static_assert((capacity & (capacity-1))==0,"power-of-two ring");
    std::array<MeterFrame,capacity> frames_{};
    std::atomic<uint32_t> write_{0};
    std::atomic<uint32_t> read_{0};
    std::atomic<bool> enabled_{false};
};
} // namespace arson::eq
