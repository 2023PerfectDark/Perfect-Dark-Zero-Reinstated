#include "../src/coop_ally_logic.h"
#include <cassert>
#include <limits>
using namespace pdz_coop;
int main() {
  RescuePolicy rescue;
  assert(rescue.Down(Partner::Ally));
  for(int i=0;i<9;++i)assert(!rescue.Advance(Partner::Ally,true,true,.3f));
  // Letting go or moving out of reach cancels the uninterrupted revive hold.
  assert(!rescue.Advance(Partner::Ally,false,true,.3f));
  for(int i=0;i<9;++i)assert(!rescue.Advance(Partner::Ally,true,true,.3f));
  assert(!rescue.Advance(Partner::Ally,true,false,.3f));
  for(int i=0;i<11 && rescue.IsDown(Partner::Ally);++i)
    rescue.Advance(Partner::Ally,true,true,.3f);
  assert(!rescue.IsDown(Partner::Ally));
  assert(rescue.Down(Partner::Player));
  assert(!rescue.Down(Partner::Ally));
  assert(rescue.Wiped());
  assert(!rescue.Advance(Partner::Player,true,true,100.f));
  rescue.Reset();
  assert(!rescue.Wiped()&&!rescue.IsDown(Partner::Player));
  assert(rescue.Down(Partner::Ally));
  assert(!rescue.Down(Partner::Player));
  assert(rescue.Wiped());
  rescue.Reset();
  assert(rescue.Down(Partner::Player));
  assert(!rescue.Advance(Partner::Player,true,true,std::numeric_limits<float>::quiet_NaN()));
  for(int i=0;i<11 && rescue.IsDown(Partner::Player);++i)
    rescue.Advance(Partner::Player,true,true,.3f);
  assert(!rescue.IsDown(Partner::Player));
  AllyPolicy p; p.Reset(1,2);
  p.FiredAt(3,1,true); assert(!p.CanEngage(3));
  p.SpottedPlayer(3,true); assert(!p.CanEngage(3));
  assert(p.EnemyMayTargetAlly(3));
  p.FiredAt(3,2,true); assert(!p.CanEngage(3));
  p.FiredAt(3,1,true); assert(p.CanEngage(3));
  p.SpottedPlayer(4,true); assert(!p.CanEngage(4));
  p.SpottedPlayer(5,false); p.FiredAt(5,1,false); assert(!p.CanEngage(5));
  AllyPolicy::Candidate enemies[]={{4,{1,0,0},true,true,true},{3,{4,0,0},true,true,true}};
  auto d=p.Tick({},{1,0,0},true,true,enemies,2);
  assert(d.order==Order::Defend && d.target==3);
  enemies[1].visible=false;
  assert(p.Tick({},{1,0,0},true,true,enemies,2).order==Order::Hold);
  assert(p.Tick({},{6,0,0},true,true,nullptr,0).order==Order::Follow);
  assert(p.Tick({},{4,0,0},true,true,nullptr,0).order==Order::Follow);
  assert(p.Tick({},{2,0,0},true,true,nullptr,0).order==Order::Hold);
  // Nearby in plan view is not reached when Joanna is on a stair landing.
  assert(p.Tick({0,2,0},{1,0,0},true,true,nullptr,0).order==Order::Follow);
  assert(p.Tick({0,.6f,0},{1,0,0},true,true,nullptr,0).order==Order::Follow);
  assert(p.Tick({0,.2f,0},{1,0,0},true,true,nullptr,0).order==Order::Hold);
  enemies[1].visible=true;
  assert(p.Tick({},{13,0,0},true,true,enemies,2).order==Order::Follow);
  p.Forget(3); assert(!p.CanEngage(3));
  p.Reset(1,2); assert(!p.EnemyMayTargetAlly(4));
  for(Actor a=3;a<131;++a) p.SpottedPlayer(a,true);
  p.SpottedPlayer(999,true); p.FiredAt(999,1,true); assert(!p.CanEngage(999));
  assert(p.Tick({},{},false,true,enemies,2).order==Order::Hold);
  assert(p.Tick({},{std::numeric_limits<float>::quiet_NaN(),0,0},true,true,enemies,2).order==Order::Hold);
  p.Forget(1); p.SpottedPlayer(3,true); p.FiredAt(3,1,true); assert(!p.CanEngage(3));
}
