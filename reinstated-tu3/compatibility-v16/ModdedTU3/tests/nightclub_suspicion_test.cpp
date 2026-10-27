#include "../src/nightclub_suspicion.h"
#include <cassert>
#include <limits>
using namespace pdz_nightclub;
int main() {
  Suspicion s;
  assert(s.Update(6.,true,true,true,false)==SuspicionStage::Suspicious);
  assert(s.Update(60.,false,true,true,false)==SuspicionStage::Suspicious);
  assert(s.Update(4.,true,true,true,false)==SuspicionStage::Aiming);
  assert(s.Update(60.,true,true,false,false)==SuspicionStage::Aiming);
  assert(s.Update(5.,true,true,true,false)==SuspicionStage::Hostile);
  assert(s.Update(60.,false,true,true,false)==SuspicionStage::Hostile);
  Suspicion armed,attacked,invalid;
  assert(armed.Update(.1,true,false,true,false)==SuspicionStage::Hostile);
  assert(attacked.Update(.1,false,true,true,true)==SuspicionStage::Hostile);
  assert(invalid.Update(std::numeric_limits<double>::quiet_NaN(),true,true,true,false)==SuspicionStage::Greeting);
  for (int fps : {30,60,180,240}) {
    Suspicion timed;
    for(int frame=0;frame<fps*16;++frame) timed.Update(1./fps,true,true,true,false);
    assert(timed.Stage()==SuspicionStage::Hostile);
  }
}
