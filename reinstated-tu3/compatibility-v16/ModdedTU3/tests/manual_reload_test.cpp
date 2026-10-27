#include "../src/manual_reload_policy.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
union Register { uint64_t u64; uint32_t u32; Register():u64(0){} };
struct PPCContext { Register r3; uint64_t lr=0; };
bool enabled=false,supported=true;
unsigned calls=0;
constexpr uint32_t player=0x300000;
namespace pdz_input { bool Supported(){return supported;} }
bool LocalPlayer(uint8_t*,uint32_t address){return address==player;}
#define REXCVAR_GET(name) enabled
#define REX_HOOK_RAW(name) void name(PPCContext& ctx,uint8_t* base)
void __imp__sub_825556E0(PPCContext& ctx,uint8_t*){++calls;ctx.r3.u64=1;}
#include "manual_reload_hook.inc"
int main(){
  std::array<uint8_t,128> memory{};memory.fill(0xA5);const auto before=memory;
  auto check=[&](bool option,bool target,bool game,uint32_t caller,bool blocked){
    enabled=option;supported=game;calls=0;
    PPCContext ctx;ctx.lr=caller;ctx.r3.u64=(target?player:player+0x1000)+228;
    sub_825556E0(ctx,memory.data());
    assert(calls==unsigned(!blocked));assert(ctx.r3.u64==unsigned(!blocked));
    assert(ctx.lr==caller);assert(memory==before);
  };
  for(bool option:{false,true})for(bool target:{false,true})for(bool game:{false,true})
    check(option,target,game,0x8223A4A8,option&&target&&game);
  // Other native consumers and manual/queued reload paths must pass through.
  for(uint32_t caller:{0x821A97E0u,0x821A9270u,0x825C79F4u,0x8223A4D4u,0x8223A584u,0u})
    check(true,true,true,caller,false);
  enabled=true;supported=true;calls=0;PPCContext invalid;
  invalid.lr=0x8223A4A8;invalid.r3.u64=0;
  sub_825556E0(invalid,memory.data());assert(calls==1);
  std::cout<<"Manual reload: production hook blocks only local automatic reload; toggle-off, nonlocal, unrelated calls and memory preservation passed\n";
}
