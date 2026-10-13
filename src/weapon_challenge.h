#pragma once
#include <cstdint>
struct PPCContext;
#include <array>
#include <string_view>
namespace pdz_challenge {
struct Weapon { unsigned id; std::u16string_view name; unsigned reserve; };
inline constexpr Weapon weapons[]{
 {2,u"P9P",800},{7,u"FALCON",800},{11,u"DY357 MAGNUM",800},{15,u"MAGSEC 4",800},
 {16,u"PSYCHOSIS GUN",800},{3,u"DW-P5",800},{5,u"UGL LIBERATOR",800},{17,u"CMP150",800},
 // Retail names differ from the internal model names: rcp120 is Plasma Rifle;
 // flachete is RCP-90. Keep selection and reserve limits on the retail IDs.
 {20,u"RCP-90",800},{9,u"FAC-16",400},{1,u"KSI-74",400},{13,u"SUPERDRAGON",400},
 {8,u"LAPTOP GUN",400},{10,u"M60",400},{18,u"PLASMA RIFLE",400},{12,u"DEF-12 SHOTGUN",400},
 {23,u"MAYHEM",400},{37,u"JACKAL",400},{38,u"SHOCKWAVE",400},
 {26,u"COMBAT SHIELD",400},{27,u"VIBLADE",400},{33,u"FRAG GRENADE",400},
 {30,u"FLASHBANG",400},{34,u"MULTI-MINE",400},{35,u"HAWK BOOMERANG",400}
};
inline constexpr const Weapon* Find(unsigned id) { for(const auto& w:weapons)if(w.id==id)return &w;return nullptr; }
void Select(unsigned id);
unsigned Selected();
unsigned Limit(uint8_t* base);
void ForceLoadout(uint8_t* base,uint32_t profile);
void Tick(PPCContext& ctx,uint8_t* base,uint32_t player);
void Reset();
}
