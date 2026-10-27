#pragma once
#include <cstdint>
struct PPCContext;
namespace pdz_population {
void Created(uint8_t* base,uint32_t actor,uint32_t setup);
void Destroyed(uint32_t actor);
void LevelEnded();
void Tick(PPCContext& ctx,uint8_t* base);
bool FriendlyOverride(uint8_t* base,uint32_t a,uint32_t b,bool native_friend,bool& result);
bool HoldFire(uint8_t* base,uint32_t shooter);
}
