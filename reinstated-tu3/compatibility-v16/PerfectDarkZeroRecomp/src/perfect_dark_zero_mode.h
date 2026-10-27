#pragma once
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

struct PPCContext;
namespace pdz_custom {
bool Active(unsigned char* base);
bool HoldFire(PPCContext& ctx,unsigned char* base,unsigned actor);
void ScalePlayerDamage(PPCContext& ctx,unsigned char* base);
void Fire(PPCContext& ctx,unsigned char* base);
void Forget(unsigned actor);
void Reset();
// Unlock is derived from the selected native profile, never a shipped default.
inline std::atomic<bool> campaign_unlocked{false};
inline std::atomic<bool> personal_unlocked{false};
inline bool Unlocked() { return campaign_unlocked.load() || personal_unlocked.load(); }
inline bool DarkCampaignComplete(const std::array<std::array<unsigned char,8>,14>& records) {
  for(const auto& mission:records)if(!mission[3]&&!mission[7])return false;
  return true;
}
inline void Configure(const std::filesystem::path& root) {
  std::ifstream input(root/"mod-progress"/"personal-pdz-mode.unlock");
  std::string line;
  personal_unlocked=input&&std::getline(input,line)&&line=="PDZ_PERSONAL_CUSTOM_MODE_V1";
  campaign_unlocked=false;
}
}
