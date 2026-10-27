#pragma once
#include <cstdint>
struct PPCContext;
namespace pdz_death_voice {
struct Shot {uint32_t actor=0,model=0,body=0,region=0;float health=0;};
bool KnockedOut(uint8_t*,uint32_t actor);
void PrepareKnockout(PPCContext,uint8_t*,uint32_t actor);
void Begin(const Shot&);
void End(PPCContext, uint8_t*,const Shot&,float,bool,bool dismembered=false);
void Reset();
void Forget(uint32_t model);
void Transfer(uint8_t*,uint32_t actor,uint32_t corpse);
void Tick(PPCContext,uint8_t*);
}
