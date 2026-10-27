#include "../src/player_blink.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main() {
  assert(pdz_blink::AngleAt(4.0)==0.f);
  assert(std::abs(pdz_blink::AngleAt(4.4)-.75f)<.00001f);
  assert(pdz_blink::AngleAt(4.5)==0.f);
  for(int fps : {30,60,120,240,360}) {
    int previous=0, blinks=0, active=0;
    for(int frame=0;frame<fps*45;++frame) {
      const float a=pdz_blink::AngleAt(double(frame)/fps);
      assert(a>=0.f && a<=.750001f);
      const int now=a>.000001f;
      if(now&&!previous)++blinks;
      active+=now;previous=now;
      // A second eye evaluated at the same simulation instant agrees exactly.
      assert(a==pdz_blink::AngleAt(double(frame)/fps));
    }
    assert(blinks==10);
    assert(std::abs(double(active)/fps-2.0)<=10.0/fps+.0001);
  }
  puts("Blink timing: 10 blinks in 45 seconds at 30/60/120/240/360 FPS; 200 ms duration, bounded angle, synchronized eyes passed.");
}
