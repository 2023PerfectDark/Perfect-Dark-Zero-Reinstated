#pragma once
#include <filesystem>
#include <cstdint>
struct PPCContext;
namespace pdz_position {
void Configure(const std::filesystem::path& root);
void Shutdown();
bool Enabled();
void Toggle();
void Clear();
void ResetLevel();
void CheckpointReached(uint8_t* base);
void Tick(PPCContext& ctx,uint8_t* base,uint32_t player);
}
