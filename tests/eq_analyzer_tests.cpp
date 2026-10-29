#include "../Source/DSP/AnalyzerQueue.h"
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
using namespace arson::eq;
static void require(bool ok,const char* msg) {
    if(!ok){std::fprintf(stderr,"FAILED: %s\n",msg);std::exit(1);}
}
int main() {
    AnalyzerQueue ring;
    MeterFrame frame;
    for(uint32_t i=0;i<AnalyzerQueue::capacity;++i)
        require(ring.push(static_cast<float>(i),-static_cast<float>(i)),"buffer accepts capacity");
    require(!ring.push(777.f,-777.f),"full ring drops telemetry");
    for(uint32_t i=0;i<AnalyzerQueue::capacity;++i) {
        require(ring.pop(frame),"drain full ring");
        require(frame.before==static_cast<float>(i) && frame.after==-frame.before,"FIFO ordered pairs");
    }
    require(!ring.pop(frame),"empty returns false");
    constexpr int N=300000;
    std::atomic<bool> finished{false};
    std::atomic<int> pushed{0};
    std::thread producer([&] {
        for(int i=0;i<N;++i)
            if(ring.push(static_cast<float>(i),-static_cast<float>(i)))
                pushed.fetch_add(1,std::memory_order_relaxed);
        finished.store(true,std::memory_order_release);
    });
    int consumed=0,previous=-1;
    while(true) {
        if(!ring.pop(frame)) {
            if(finished.load(std::memory_order_acquire)) break;
            std::this_thread::yield();continue;
        }
        const int index=static_cast<int>(frame.before);
        require(index>previous,"SPSC increasing order (dropped frames okay)");
        require(frame.after==-frame.before,"SPSC frame pair uncorrupted");
        previous=index;++consumed;
    }
    producer.join();
    // Above loop may have consumed one frame inside its condition; verify no residual.
    while(ring.pop(frame)) {
        require(static_cast<int>(frame.before)>previous,"tail ordering");
        previous=static_cast<int>(frame.before);
        ++consumed;
    }
    require(consumed>0,"consumer receives samples");
    std::printf("SPSC received %d / %d frames (non-blocking drop allowed)\n",consumed,N);
}
