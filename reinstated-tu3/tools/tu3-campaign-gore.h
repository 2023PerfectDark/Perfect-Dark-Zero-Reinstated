#pragma once
#include <cstdint>
struct PPCContext;
namespace pdz_gore {
void CorpseHit(PPCContext& ctx,uint8_t* base);
bool Campaign(uint8_t* base);
void HideHead(uint8_t* base,uint32_t model);
unsigned RemovedParts(uint8_t* base,uint32_t model);
void AdoptParts(uint8_t* base,uint32_t model,uint32_t head,unsigned mask);
void Reset();
}
