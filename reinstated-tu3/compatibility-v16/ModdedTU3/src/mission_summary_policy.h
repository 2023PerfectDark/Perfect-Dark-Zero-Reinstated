#pragma once
#include <cstdint>
#include <string>
#include <cstdio>

namespace pdz_results {
// Times in native result records are centiseconds; mission targets are seconds.
struct Summary {
  unsigned mission{}, difficulty{}, campaign_style{}, mode{};
  bool completed{};
  uint32_t time_cs{}, best_cs{}, target_seconds{};
  uint32_t kills{}, shots{}, head_hits{}, body_hits{}, limb_hits{};
  bool newly_unlocked{};
};
inline bool EarnedAlertedGuards(const Summary& s) {
  return s.completed && s.mode == 0 && s.mission == 1 &&
      s.campaign_style == 1 && s.difficulty == 0 && s.time_cs > 0 &&
      s.target_seconds > 0 && uint64_t(s.time_cs) < uint64_t(s.target_seconds) * 100;
}
inline double Accuracy(const Summary& s) {
  if (!s.shots) return 0;
  const auto hits = uint64_t(s.head_hits) + s.body_hits + s.limb_hits;
  return double(hits) * 100.0 / s.shots;
}
inline std::string Clock(uint64_t centiseconds) {
  char text[48];
  std::snprintf(text, sizeof(text), "%llu:%02llu.%02llu",
      static_cast<unsigned long long>(centiseconds / 6000),
      static_cast<unsigned long long>((centiseconds / 100) % 60),
      static_cast<unsigned long long>(centiseconds % 100));
  return text;
}
}
