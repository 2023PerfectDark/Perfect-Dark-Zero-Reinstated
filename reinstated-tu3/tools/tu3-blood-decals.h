#pragma once
#include <rex/ppc/context.h>
namespace pdz_blood {
bool AllocatingTexture();
bool Cap(PPCContext, uint8_t*, uint32_t, unsigned, float);
void Hit(PPCContext&, uint8_t*, bool gore = false);
}
