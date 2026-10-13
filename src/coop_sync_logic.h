#pragma once
#include "coop_ally_logic.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace pdz_coop {

inline bool SyncFinite(Position value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// Positions, radius and range use the same units (metres in the native adapter).
// The direction may be unnormalised. Occlusion must be checked by the caller.
inline bool HeadRayFocus(Position origin, Position direction, Position head,
                         float maxRange = 80.f, float radius = .2f) {
  if (!SyncFinite(origin) || !SyncFinite(direction) || !SyncFinite(head) ||
      !std::isfinite(maxRange) || !std::isfinite(radius) || maxRange <= 0 || radius <= 0)
    return false;
  const double lengthSquared = double(direction.x) * direction.x +
      double(direction.y) * direction.y + double(direction.z) * direction.z;
  if (lengthSquared <= 0) return false;
  const double inverseLength = 1. / std::sqrt(lengthSquared);
  const double dx = direction.x * inverseLength;
  const double dy = direction.y * inverseLength;
  const double dz = direction.z * inverseLength;
  const double hx = double(head.x) - origin.x;
  const double hy = double(head.y) - origin.y;
  const double hz = double(head.z) - origin.z;
  const double along = hx * dx + hy * dy + hz * dz;
  if (along < 0 || along > maxRange) return false;
  const double ox = hx - along * dx;
  const double oy = hy - along * dy;
  const double oz = hz - along * dz;
  return ox * ox + oy * oy + oz * oz <= double(radius) * radius;
}

struct SyncCandidate {
  Actor actor{};
  Position head{};
  bool alive{}, hostile{}, visible{};
};

struct SyncInput {
  Actor player{}, ally{}, headFocus{};
  const SyncCandidate* group{};
  unsigned groupCount{};
  // Time since the preceding sample. A gap over .5 s invalidates continuity.
  // The first sample of a new lock starts at zero: its preceding interval does
  // not establish uninterrupted head focus. Shot hooks may provide zero.
  double elapsedSeconds{};
  bool enabled{}, playerAlive{}, allyAlive{};
  bool paused{}, reviving{}, weaponSwitching{};
  // A verified native player discharge event, never a trigger/key/button state.
  bool confirmedPlayerShot{};
  bool botReady{};
  Actor botAimTarget{};
};

struct SyncDecision {
  bool active{};
  Actor target{};
  Position targetHead{};
  bool fire{};
};

class SyncPolicy {
 public:
  Actor Target() const { return focusSeconds_ > 2. ? target_ : 0; }

  void Reset() {
    player_ = ally_ = focus_ = target_ = 0;
    group_ = {};
    groupCount_ = 0;
    focusSeconds_ = 0;
    fired_ = false;
  }

  void Forget(Actor actor) {
    if (!actor) return;
    if (actor == player_ || actor == ally_ ||
        std::find(group_.begin(), group_.end(), actor) != group_.end()) Reset();
  }

  SyncDecision Tick(const SyncInput& input) {
    if (!input.enabled || !input.playerAlive || !input.allyAlive ||
        input.paused || input.reviving || input.weaponSwitching ||
        !input.player || !input.ally || input.player == input.ally ||
        !input.headFocus || !input.group ||
        (input.groupCount != 2 && input.groupCount != 3) ||
        !std::isfinite(input.elapsedSeconds) || input.elapsedSeconds < 0 ||
        input.elapsedSeconds > .5) {
      Reset();
      return {};
    }

    std::array<Actor, 3> currentGroup{};
    bool containsFocus = false;
    for (unsigned i = 0; i < input.groupCount; ++i) {
      const auto& candidate = input.group[i];
      if (!candidate.actor || candidate.actor == input.player || candidate.actor == input.ally ||
          !candidate.alive || !candidate.hostile || !candidate.visible || !SyncFinite(candidate.head)) {
        Reset();
        return {};
      }
      currentGroup[i] = candidate.actor;
      containsFocus |= candidate.actor == input.headFocus;
    }
    std::sort(currentGroup.begin(), currentGroup.begin() + input.groupCount);
    if (!containsFocus || std::adjacent_find(currentGroup.begin(),
        currentGroup.begin() + input.groupCount) != currentGroup.begin() + input.groupCount) {
      Reset();
      return {};
    }

    if (player_ != input.player || ally_ != input.ally || focus_ != input.headFocus ||
        groupCount_ != input.groupCount || group_ != currentGroup) {
      Reset();
      player_ = input.player;
      ally_ = input.ally;
      focus_ = input.headFocus;
      group_ = currentGroup;
      groupCount_ = input.groupCount;
      // Stable assignment under native enumeration/order changes. Full handles
      // include allocation generations, so actor recycling cannot retain a lock.
      for (unsigned i = 0; i < groupCount_; ++i) {
        if (group_[i] != focus_) { target_ = group_[i]; break; }
      }
      return {};
    }

    // No frame-count approximation or per-frame time clamp. Saturating after
    // the threshold prevents overflow without changing dwell timing.
    focusSeconds_ = std::min(3., focusSeconds_ + input.elapsedSeconds);
    if (focusSeconds_ <= 2.) return {};

    const SyncCandidate* target = nullptr;
    for (unsigned i = 0; i < input.groupCount; ++i)
      if (input.group[i].actor == target_) target = &input.group[i];
    if (!target) { Reset(); return {}; }

    const bool fire = !fired_ && input.confirmedPlayerShot && input.botReady &&
                      input.botAimTarget == target_;
    if (fire) fired_ = true;
    return {true, target_, target->head, fire};
  }

 private:
  Actor player_{}, ally_{}, focus_{}, target_{};
  std::array<Actor, 3> group_{};
  unsigned groupCount_{};
  double focusSeconds_{};
  bool fired_{};
};

} // namespace pdz_coop
