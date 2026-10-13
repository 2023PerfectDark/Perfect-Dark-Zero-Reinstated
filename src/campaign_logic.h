#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>

namespace pdz_campaign {
struct ShieldDamage { float health, shield; };
inline ShieldDamage ModernShieldDamage(float health, float shield, float remaining,
                                      bool sniperBullet) {
  if (!std::isfinite(health) || !std::isfinite(shield) ||
      !std::isfinite(remaining) || health < 0 || shield < 0 || remaining < 0)
    return {health, shield};
  const float total = health + shield;
  if (!std::isfinite(total)) return {health, shield};
  if (sniperBullet) return {total, 0};
  const float absorbed = std::min(remaining, total);
  return {total - absorbed, absorbed};
}

// The mission scripts set level-state bit 0x40 at their original checkpoint.
// Consume an edge even outside Classic, so changing rules cannot grant a heal.
class CheckpointRecovery {
 public:
  void ResetMission() { state_.store(2); }
  bool Observe(bool reached) {
    const unsigned previous = state_.exchange(2u | unsigned(reached));
    return previous == 2 && reached;
  }
 private:
  std::atomic<unsigned> state_{0}; // unknown until the first observation
};

inline float CheckpointHealth(float current, float ceiling) {
  // Never revive, remove permanent injury, or reduce an already higher value.
  if (!std::isfinite(current) || !std::isfinite(ceiling) || current <= 0 ||
      ceiling <= current || ceiling > 100) return current;
  return ceiling;
}

inline float AdvanceShockTimer(float elapsed, float dt) {
  // Match the native five-second timer without performing automatic recovery.
  if (!std::isfinite(elapsed) || !std::isfinite(dt) || elapsed < 0 || dt <= 0)
    return elapsed;
  return elapsed < 5.f ? std::min(5.f, elapsed + dt) : elapsed;
}

inline bool HideClassicHealth(float elapsed, float dt) {
  return std::isfinite(elapsed) && std::isfinite(dt) && elapsed >= 0 && dt >= 0 &&
         elapsed + dt >= 3.f;
}
} // namespace pdz_campaign
