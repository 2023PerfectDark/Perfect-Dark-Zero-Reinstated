#pragma once
#include <cstdint>

struct PPCContext;
namespace pdz_klobb {
inline constexpr char kModelName[] = "aid_darkcharacter_guns_klobb_fp_klobb";
// True only while the native resource loader is resolving this loose model.
bool LoadingModel();
// Call only from the active Surface gameplay update. Successful acquisition
// uses the retail inventory, ammunition, action and equip paths.
void Tick(PPCContext& ctx, uint8_t* base, uint32_t player);
bool Collected();
// Handles only the private local Klobb's named reload sound events. Returns
// false to retain native sound if the original GoldenEye sample is unavailable.
bool HandleReloadSound(PPCContext& ctx, uint8_t* base);
// Reset the cabin pickup for a new level. Resource lifetime is tied to the
// native weapon manager, not the background renderer's lifetime.
void ResetPickup();
}
