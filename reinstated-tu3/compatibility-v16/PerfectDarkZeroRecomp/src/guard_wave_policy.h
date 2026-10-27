#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace pdz_cheats {
// Scheduling only. The runtime must validate a free, navigable spawn before
// acknowledging it; a failed placement never consumes a wave member.
class GuardWaves {
 public:
  void Reset() {running_=false;pending_=0;until_wave_=0;until_attempt_=0;}
  unsigned Tick(double seconds,bool enabled,bool paused,unsigned alive,
                unsigned per_wave=4,unsigned limit=16,double interval=30) {
    if(!enabled){Reset();return 0;}
    if(paused||!std::isfinite(seconds)||seconds<0||seconds>1)return 0;
    if(!running_){running_=true;pending_=std::min(per_wave,limit);until_wave_=interval;}
    else {
      until_wave_-=seconds;
      if(until_wave_<=0) {
        // No catch-up burst after a long pause or a full arena.
        pending_=std::min(per_wave,limit);until_wave_=std::max(interval,1.0);
      }
    }
    until_attempt_=std::max(0.0,until_attempt_-seconds);
    if(until_attempt_>1e-8||alive>=limit||!pending_)return 0;
    until_attempt_=.25;
    return std::min({pending_,limit-alive,2u});
  }
  void Spawned(unsigned count) {pending_-=std::min(pending_,count);}
 private:
  bool running_=false;
  unsigned pending_=0;
  double until_wave_=0,until_attempt_=0;
};
}
