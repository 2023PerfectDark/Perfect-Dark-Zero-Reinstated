#include "../src/reload_timing_policy.h"
#include <cassert>
#include <limits>
int main(){
  using namespace pdz_reload;
  assert(Speed(0,17,false,4.,1.)==1.);
  assert(Speed(1,999,false,4.,1.)==1.);
  assert(Speed(2,999,false,4.,1.)==1.);
  assert(Speed(1,17,false,0.,1.)==1.);
  assert(Speed(1,17,false,std::numeric_limits<double>::quiet_NaN(),1.)==1.);
  assert(Speed(1,17,false,4.,1.)>Speed(1,17,true,4.,1.));
  assert(Speed(1,10,false,6.,1.)>1.);
  assert(std::abs(4./Speed(2,17,false,4.,1.)-127./60.)<1e-9);
  assert(std::abs(3./Speed(2,7,false,3.,1.)-91./60.)<1e-9);
  assert(Speed(2,17,false,100.,1.)<=4.);
}
