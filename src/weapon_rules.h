#pragma once
#include <cstdint>

namespace pdz_weapons {
void PollWallReload(uint32_t player, bool held);
void RequestInspection(uint32_t weapon, uint32_t offhand = 0);
void ClearInspection();
uint32_t ReserveLimit(uint8_t* base, uint32_t record);
uint32_t SecondaryHudSlots(uint8_t* base, uint32_t native);
double Spread(uint8_t* base, uint32_t player, double original);
}
