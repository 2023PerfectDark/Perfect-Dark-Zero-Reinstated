#pragma once
#include <rex/ppc/context.h>
namespace pdz_campaign {
void Tick(PPCContext& ctx, uint8_t* base, uint32_t rendered_handle);
bool HideForCharacterPicker(PPCContext& ctx,uint8_t* base,uint32_t handle);
bool BackKey(uint16_t key, uint16_t flags);
void ReleasePortraits(PPCContext& call, uint8_t* base);
}
