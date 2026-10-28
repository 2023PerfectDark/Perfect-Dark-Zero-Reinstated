#include "../src/backpack_reload_policy.h"
constexpr bool Check() {
  using pdz_backpack::Reload;
  auto a=Reload(7,30,100);
  if(a.magazine!=30 || a.reserve!=77)return false;
  a=Reload(0,30,5);
  if(a.magazine!=5 || a.reserve!=0)return false;
  a=Reload(0,30,0);
  if(a.magazine || a.reserve)return false;
  a=Reload(30,30,90);
  if(a.magazine!=30 || a.reserve!=90)return false;
  a=Reload(40,30,90);
  if(a.magazine!=40 || a.reserve!=90)return false;
  a=Reload(0,30,65535);
  if(a.magazine!=30 || a.reserve!=65535)return false;
  // Two stowed hands share a reserve; neither may duplicate ammunition.
  a=Reload(0,20,25);
  auto b=Reload(0,20,a.reserve);
  return a.magazine==20 && b.magazine==5 && b.reserve==0;
}
static_assert(Check());
int main(){return Check()?0:1;}
