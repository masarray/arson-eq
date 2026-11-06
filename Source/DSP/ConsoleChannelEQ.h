// SPDX-License-Identifier: AGPL-3.0-only
// ArSonMix console profile over the existing ArSonEQ minimum-phase DSP.
// This has NO JUCE, WebView, VST3 host, allocations, or new audio callback.
#pragma once

#include "ParametricEQ.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace arson::eq::console {

enum class Slot : std::uint8_t {
    HighPass = 0, LowShelf, Bell1, Bell2, Bell3, Bell4, Bell5, HighShelf, LowPass
};

constexpr std::size_t kSlots = 9;

// Value-only control contract. Send on the owner's real-time control queue;
// never mutate a live ConsoleChannelEQ from a GUI/network thread.
struct Parameters {
    bool enabled = false;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.7071067811865476;
    int slopeDbPerOct = 24;
};

class ConsoleChannelEQ final {
public:
    ConsoleChannelEQ() noexcept {
        engine_.prepare(48000.0);
        resetToFlat();
        // There is no fade or enabled filter in a newly created EQ.
        everEnabled_ = false;
    }
    ConsoleChannelEQ(const ConsoleChannelEQ&) = delete;
    ConsoleChannelEQ& operator=(const ConsoleChannelEQ&) = delete;

    static constexpr int addedLatencySamples() noexcept { return 0; }
    static constexpr bool isValid(Slot slot) noexcept {
        return static_cast<unsigned>(slot) < kSlots;
    }
    static constexpr bool isCut(Slot slot) noexcept {
        return slot == Slot::HighPass || slot == Slot::LowPass;
    }
    static Type typeFor(Slot slot) noexcept {
        switch (slot) {
            case Slot::HighPass: return Type::LowCut;
            case Slot::LowShelf: return Type::LowShelf;
            case Slot::HighShelf: return Type::HighShelf;
            case Slot::LowPass: return Type::HighCut;
            default: return Type::Bell;
        }
    }
    static Parameters defaultParameters(Slot slot) noexcept {
        Parameters p;
        switch (slot) {
            case Slot::HighPass: p.frequencyHz=80.0; break;
            case Slot::LowShelf: p.frequencyHz=160.0; break;
            case Slot::Bell1: p.frequencyHz=250.0; break;
            case Slot::Bell2: p.frequencyHz=500.0; break;
            case Slot::Bell3: p.frequencyHz=1200.0; break;
            case Slot::Bell4: p.frequencyHz=3200.0; break;
            case Slot::Bell5: p.frequencyHz=7500.0; break;
            case Slot::HighShelf: p.frequencyHz=10000.0; break;
            case Slot::LowPass: p.frequencyHz=18000.0; break;
            default: break;
        }
        return p;
    }

    // Call before publishing this instance to the real-time owner.
    // Retains settings, resets filter history, and adopts the new sample rate.
    void prepare(double sampleRate) noexcept {
        engine_.prepare(sampleRate);
        const double maxHz = maximumFrequency();
        for (std::size_t i=0; i<kSlots; ++i) {
            const Slot slot=static_cast<Slot>(i);
            Band band=engine_.band(static_cast<int>(i));
            if (band.frequency > maxHz) {
                band.frequency=maxHz;
                engine_.setBand(static_cast<int>(i),band);
            }
        }
        // Keep the process path enabled if there are outstanding fades.
        // Re-preparing disabled channels is harmless; it never restarts a device.
    }

    // Ownership: ONLY the audio thread at a block boundary, or the preparing
    // control thread while the instance is not published. No simultaneous access.
    bool setBand(Slot slot, const Parameters& p) noexcept {
        if (!isValid(slot) || !std::isfinite(p.frequencyHz) ||
            !std::isfinite(p.gainDb) || !std::isfinite(p.q) ||
            p.frequencyHz < 20.0 || p.frequencyHz > maximumFrequency() ||
            p.q < 0.1 || p.q > 24.0 ||
            p.gainDb < -18.0 || p.gainDb > 18.0) return false;
        if (isCut(slot) && p.slopeDbPerOct != 12 &&
            p.slopeDbPerOct != 24 && p.slopeDbPerOct != 48) return false;
        const int index=static_cast<int>(slot);
        Band band=engine_.band(index);
        band.enabled=p.enabled;
        band.type=typeFor(slot);  // immutable slot topology, never arbitrary VST band mapping
        band.route=Route::Stereo;
        band.frequency=p.frequencyHz;
        band.gainDb=isCut(slot) ? 0.0 : p.gainDb;
        band.q=p.q;
        band.slope=isCut(slot) ? p.slopeDbPerOct : 12;
        band.dynamicRangeDb=0.0; // Static, zero-lookahead console EQ only
        band.thresholdDb=-24.0;
        if (band.enabled) everEnabled_=true;
        engine_.setBand(index,band);
        return true;
    }

    bool setEnabled(Slot slot, bool enabled) noexcept {
        if (!isValid(slot)) return false;
        const auto p=parameters(slot);
        Parameters updated=p;
        updated.enabled=enabled;
        return setBand(slot,updated);
    }

    Parameters parameters(Slot slot) const noexcept {
        if (!isValid(slot)) return {};
        const Band b=engine_.band(static_cast<int>(slot));
        return {b.enabled,b.frequency,b.gainDb,b.q,b.slope};
    }

    void resetToFlat() noexcept {
        for (std::size_t i=0; i<kSlots; ++i) {
            const Slot slot=static_cast<Slot>(i);
            auto p=defaultParameters(slot);
            p.frequencyHz=std::min(p.frequencyHz,maximumFrequency());
            setBand(slot,p);
        }
        // Do not clear everEnabled_ here: bypass transitions need to finish.
    }

    // In-place, sample-synchronous minimum-phase IIR. No lookahead, no
    // buffering, no extra callback and no allocation; process each ChannelId
    // with its own distinct object. Never call concurrently on one instance.
    void processStereo(float* left, float* right, std::size_t frames) noexcept {
        if (!left || !right || frames == 0 || !everEnabled_) return;
        if (left == right) { processMono(left,frames); return; }
        for (std::size_t i=0; i<frames; ++i) {
            double l=left[i],r=right[i];
            engine_.process(l,r);
            left[i]=static_cast<float>(l);
            right[i]=static_cast<float>(r);
        }
    }
    void processMono(float* mono, std::size_t frames) noexcept {
        if (!mono || frames == 0 || !everEnabled_) return;
        for (std::size_t i=0; i<frames; ++i) {
            double v=mono[i];
            engine_.processMono(v);
            mono[i]=static_cast<float>(v);
        }
    }

    // For a UI snapshot on the owning/control thread only; do NOT read
    // engine_ concurrently with processing. Aggregate on the UI thread.
    double bandResponseDb(Slot slot, double frequencyHz) const noexcept {
        if (!isValid(slot) || !std::isfinite(frequencyHz) ||
            frequencyHz <= 0.0 || frequencyHz >= engine_.sampleRate()*0.5)
            return 0.0;
        return engine_.bandResponseDb(static_cast<int>(slot),frequencyHz);
    }

private:
    double maximumFrequency() const noexcept {
        return std::min(20000.0,engine_.sampleRate()*0.45);
    }

    Engine engine_;
    bool everEnabled_=false;
};

} // namespace arson::eq::console
