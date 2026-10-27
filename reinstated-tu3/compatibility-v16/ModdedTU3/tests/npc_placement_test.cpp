#include "../src/npc_placement_policy.h"
#include <cassert>
#include <limits>
int main() {
  using namespace pdz_population;
  const std::array<Point,2> occupied{{{0,0,0},{140,0,0}}};
  assert(!Separated({0,0,0},occupied));
  assert(!Separated({90,0,0},occupied));
  assert(Separated({0,0,140},occupied));
  assert(!Separated({0,179,0},occupied));
  assert(Separated({0,200,0},occupied));
  assert(!Separated({std::numeric_limits<float>::quiet_NaN(),0,0},occupied));
  assert(!Separated(Candidate({0,0,0},0),occupied));
  assert(Separated(Candidate({0,0,0},1),occupied));
  for(unsigned i=0;i<32;++i)assert(Finite(Candidate({10,20,30},i)));
}
