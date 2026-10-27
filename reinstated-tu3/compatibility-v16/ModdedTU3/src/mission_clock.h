#pragma once
#include <atomic>
#include <cstdint>
namespace pdz_mission_clock {
inline std::atomic<int> milliseconds{-1};
// Game-thread accumulator; only the atomic display snapshot crosses threads.
inline double elapsed=0.0;
void Tick(uint8_t* base);
}


