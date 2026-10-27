#include "../src/guard_wave_policy.h"
#include <cassert>
#include <limits>
unsigned Simulate(unsigned fps) {
  pdz_cheats::GuardWaves waves;unsigned spawned=0;
  for(unsigned frame=0;frame<fps*65;++frame) {
    const auto count=waves.Tick(1.0/fps,true,false,0);
    waves.Spawned(count);spawned+=count;
  }
  return spawned;
}
int main() {
  assert(Simulate(30)==12);assert(Simulate(60)==12);assert(Simulate(180)==12);
  pdz_cheats::GuardWaves waves;
  assert(waves.Tick(.01,true,true,0)==0);
  assert(waves.Tick(.01,true,false,16)==0);
  assert(waves.Tick(.3,true,false,15)==1);
  // Failed placement: no Spawned acknowledgement; retry remains possible.
  assert(waves.Tick(.3,true,false,15)==1);
  waves.Spawned(1);
  assert(waves.Tick(.3,true,false,0)==2);waves.Spawned(2);
  assert(waves.Tick(.3,true,false,0)==1);waves.Spawned(1);
  assert(waves.Tick(.3,true,false,0)==0);
  assert(waves.Tick(60,true,false,0)==0);
  assert(waves.Tick(std::numeric_limits<double>::quiet_NaN(),true,false,0)==0);
  assert(waves.Tick(.01,false,false,0)==0);
  assert(waves.Tick(.01,true,false,0)==2);
}
