#include "../Source/DSP/AnalyzerQueue.h"
#include "../Source/DSP/SpectrumDisplay.h"
#include <atomic>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace arson::eq;
static void require(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
int main(){
    AnalyzerQueue q;MeterFrame f;
    require(q.push(0.6f,-0.6f,0.3f,-0.3f),"preserve anti-phase pcm");
    require(q.pop(f)&&f.preL==0.6f&&f.preR==-0.6f&&f.postL==0.3f&&f.postR==-0.3f,"stereo frame integrity");
    for(uint32_t i=0;i<AnalyzerQueue::capacity;++i)
        require(q.push(float(i),-float(i),float(i+1),-float(i+1)),"full capacity");
    require(!q.push(9.f,8.f,7.f,6.f),"visual overflow drops");
    for(uint32_t i=0;i<AnalyzerQueue::capacity;++i){
        require(q.pop(f)&&f.preL==float(i)&&f.preR==-float(i)&&
                f.postL==float(i+1)&&f.postR==-float(i+1),"FIFO four channels");
    }
    require(!q.pop(f),"empty");
    require(q.push(1.f,2.f)&&q.pop(f)&&f.preL==f.preR&&f.postL==f.postR,"mono contract");
    const double anti=SpectrumDisplay::power(0.5f,-0.5f,1.f);
    const double mono=SpectrumDisplay::power(0.5f,0.5f,1.f);
    require(std::abs(anti-mono)<1.e-8&&anti>0.2499,"anti-phase does not cancel stereo power");
    require(std::abs(SpectrumDisplay::db(1.)-0.)<0.001,"0dB power");
    require(std::abs(SpectrumDisplay::db(0.25)+6.0206)<0.02,"-6dB power");
    require(std::abs(SpectrumDisplay::tilt(-40.f,2000.,4.5f)+35.5)<0.02,"visual tilt 4.5dB/oct");
    require(std::abs(SpectrumDisplay::tilt(-40.f,1000.,6.f)+40.)<0.02,"tilt pivot");
    require(SpectrumDisplay::smooth(-120.f,-20.f,0.04)>-120.f,"attack progresses");
    std::array<float,4097> spectrumL{},spectrumR{};
    spectrumL[100]=2.f;spectrumR[100]=-2.f;
    require(SpectrumDisplay::samplePower(spectrumL,spectrumR,8192,48000.,100.*48000./8192.)>3.99,
            "equal anti-phase energy after FFT");
    // Multithread SPSC: no frame tearing, ordered samples, no blocking.
    constexpr int N=200000;std::atomic<bool> done{false};
    std::thread writer([&]{for(int i=0;i<N;++i)q.push(float(i),float(-i),float(i+1),float(-i-1));done.store(true);});
    int count=0,last=-1;
    while(true){
        if(q.pop(f)){
            require(int(f.preL)>last&&f.preR==-f.preL&&f.postL==f.preL+1.f,"SPSC no tearing");
            last=int(f.preL);++count;continue;
        }
        if(done.load(std::memory_order_acquire))break;
        std::this_thread::yield();
    }
    writer.join();
    while(q.pop(f)){
        require(int(f.preL)>last&&f.preR==-f.preL,"SPSC ordered tail");
        last=int(f.preL);++count;
    }
    require(count>0,"SPSC progress");
    std::printf("Stereo analyzer / power / SPSC PASS: %d frames read\n",count);
}
