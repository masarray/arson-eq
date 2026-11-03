// Original, JUCE-free spectral display mathematics. Analysis uses power,
// never time-domain L+R downmix. Tilt and smoothing are display-only.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
namespace arson::eq {
struct SpectrumDisplay {
    static constexpr int points=384, maxFft=8192;
    static constexpr double loHz=20.,hiHz=20000.;
    static constexpr float floorDb=-120.f;
    static double power(float magnitudeL,float magnitudeR,float coherentAmplitude) noexcept {
        const double scale=std::max(1.e-12,double(coherentAmplitude));
        const double l=double(magnitudeL)/scale, r=double(magnitudeR)/scale;
        return 0.5*(l*l+r*r);
    }
    static float db(double linearPower) noexcept {
        return float(std::clamp(10.*std::log10(std::max(linearPower,1.e-12)),
                                -120.,24.));
    }
    static double frequency(int index) noexcept {
        return loHz*std::pow(hiHz/loHz,double(index)/(points-1));
    }
    static float tilt(float value,double frequencyHz,float dbPerOctave) noexcept {
        return value+dbPerOctave*float(std::log2(std::max(1.,frequencyHz)/1000.));
    }
    static float smooth(float previous,float next,double elapsedSec,bool fast=false) noexcept {
        const double seconds=next>previous?(fast?0.025:0.060):(fast?0.14:0.35);
        const double k=std::exp(-std::clamp(elapsedSec,0.,1.)/seconds);
        return float(std::clamp(k*previous+(1.-k)*next,-120.,24.));
    }
    // Interpolate spectral *power* across adjacent FFT bins to avoid nearest-bin
    // stair-stepping, while staying faithful to the physical FFT resolution.
    template<typename Magnitudes>
    static double samplePower(const Magnitudes& left,const Magnitudes& right,
                              int fftSize,double sampleRate,double hz) noexcept {
        const double fractional=std::clamp(hz*fftSize/std::max(1.,sampleRate),
                                            1.,double(fftSize/2));
        const int lower=int(fractional),upper=std::min(lower+1,fftSize/2);
        const double mix=fractional-lower;
        // Caller supplies coherent amplitude normalization (Hann sum/2).
        const double a=0.5*(left[lower]*left[lower]+right[lower]*right[lower]);
        const double b=0.5*(left[upper]*left[upper]+right[upper]*right[upper]);
        return a+(b-a)*mix;
    }
};
}
