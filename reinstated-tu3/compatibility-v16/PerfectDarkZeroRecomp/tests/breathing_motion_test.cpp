#include "../src/mod/breathing_motion.h"
#include <cassert>
#include <cmath>
int main(){
 using breathing_motion::State;
 State a,b;
 for(int i=0;i<600;++i)a.Step(1.f/60,400,true,false);
 for(int i=0;i<1200;++i)b.Step(1.f/120,400,true,false);
 assert(std::abs(a.exertion-b.exertion)<.001f);
 assert(a.exertion>.99f);
 for(int i=0;i<1800;++i)a.Step(1.f/60,0,true,false);
 assert(a.exertion<.002f);
 auto off=a.Step(1.f/60,400,false,false);
 assert(off.pitch==0&&off.roll==0&&a.phase==0&&a.exertion==0);
 State full,zoom;
 for(int i=0;i<120;++i){auto f=full.Step(1.f/60,0,true,false);auto z=zoom.Step(1.f/60,0,true,true);
 assert(std::abs(f.pitch*.22f-z.pitch)<.000001f);assert(std::abs(f.roll)<.0033f);}
 for(unsigned id:{0u,2u,7u,11u,15u,16u})assert(!breathing_motion::SlowRecoveryWeapon(id));
 for(unsigned id:{1u,3u,10u,26u,33u,37u})assert(breathing_motion::SlowRecoveryWeapon(id));
 State light,heavy,heavy120;light.exertion=heavy.exertion=heavy120.exertion=1;
 for(int i=0;i<600;++i){light.Step(1.f/60,0,true,false,false);heavy.Step(1.f/60,0,true,false,true);}
 for(int i=0;i<1200;++i)heavy120.Step(1.f/120,0,true,false,true);
 assert(light.exertion<.12f&&heavy.exertion>.40f);
 assert(std::abs(heavy.exertion-heavy120.exertion)<.001f);
 auto paused=b.Step(0,0,true,false);assert(paused.pitch==0&&paused.roll==0);
 State front,back,left,right;
 for(int i=0;i<300;++i){
   const auto f=front.Step(1.f/60,250,true,false,false,1,0);
   const auto b=back.Step(1.f/60,250,true,false,false,-1,0);
   const auto l=left.Step(1.f/60,250,true,false,false,0,-1);
   const auto r=right.Step(1.f/60,250,true,false,false,0,1);
   assert(f.roll==0&&b.roll==0);
   assert(l.roll<0&&r.roll>0&&std::abs(l.roll+r.roll)<.000001f);
 }
 State noSound,withSound;
 for(int i=0;i<120;++i){
   const auto quiet=noSound.Step(1.f/60,250,true,false,false,1,0);
   const auto hit=withSound.Step(1.f/60,250,true,false,false,1,0,i==30);
   if(i<30||i>46)assert(std::abs(quiet.pitch-hit.pitch)<.000001f);
   if(i==34)assert(hit.pitch<quiet.pitch-.001f);
   assert(hit.roll==0);
 }
 State transition;
 auto initial=transition.Step(1.f/60,0,true,false);
 assert(std::abs(initial.roll)<.0001f);
 for(int i=0;i<120;++i)transition.Step(1.f/60,250,true,false,false,0,1);
 const float before=transition.roll;
 const auto reverse=transition.Step(1.f/60,250,true,false,false,0,-1);
 assert(reverse.roll>0&&std::abs(reverse.roll-before)<.0003f);
 for(int i=0;i<120;++i)transition.Step(1.f/60,250,true,false,false,1,0);
 assert(std::abs(transition.roll)<.000001f);
}
