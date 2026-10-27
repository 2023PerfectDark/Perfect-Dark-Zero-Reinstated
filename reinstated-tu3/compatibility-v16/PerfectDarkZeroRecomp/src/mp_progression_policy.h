#pragma once
#include <cstdint>
#include <limits>

namespace pdz_progression {
// Prestige zero is the initial career. Prestige five must also reach 50
// before entering Master, whose independent level range is 1..1000.
struct Rank {
  uint32_t prestige = 0;
  uint32_t level = 1;
  bool master = false;
  uint64_t level_xp = 0;
  uint64_t lifetime_xp = 0;
};
inline uint64_t RequiredXp(uint32_t level) {
  return 500ull + 35ull * level + 3ull * level * level;
}
inline bool Valid(const Rank& r) {
  return r.prestige <= 5 && r.level >= 1 &&
    r.level <= (r.master ? 1000u : 50u) &&
    (!r.master || r.prestige == 5) &&
    r.level_xp < RequiredXp(r.level);
}
inline bool CanPrestige(const Rank& r) {
  return Valid(r) && !r.master && r.level == 50;
}
inline bool CanResetCareer(const Rank& r) {
  return Valid(r) && r.master && r.level >= 50;
}
inline uint64_t SaturatingAdd(uint64_t a, uint64_t b) {
  return b > std::numeric_limits<uint64_t>::max() - a ?
    std::numeric_limits<uint64_t>::max() : a + b;
}
inline void AwardXp(Rank& r, uint64_t xp) {
  if (!Valid(r)) return;
  r.lifetime_xp = SaturatingAdd(r.lifetime_xp, xp);
  const auto cap = r.master ? 1000u : 50u;
  while (r.level < cap && xp) {
    const auto remaining = RequiredXp(r.level) - r.level_xp;
    if (xp < remaining) { r.level_xp += xp; return; }
    xp -= remaining;
    r.level_xp = 0;
    ++r.level;
  }
  // XP earned at a rank cap remains lifetime XP, not a bank for skipping
  // the next prestige. Achievement eligibility is stored independently.
  if (r.level == cap) r.level_xp = 0;
}
inline bool Prestige(Rank& r) {
  if (!CanPrestige(r)) return false;
  if (r.prestige == 5) r.master = true;
  else ++r.prestige;
  r.level = 1;
  r.level_xp = 0;
  return true;
}
inline bool ResetCareer(Rank& r) {
  if (!CanResetCareer(r)) return false;
  r = Rank{};
  return true;
}
}
