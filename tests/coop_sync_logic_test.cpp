#include "../src/coop_sync_logic.h"
#include <cassert>
#include <limits>
#include <utility>
using namespace pdz_coop;

struct Fixture {
  SyncPolicy policy;
  SyncCandidate candidates[4] = {
      {30, {0, 1.7f, 8}, true, true, true},
      {20, {2, 1.7f, 8}, true, true, true},
      {40, {4, 1.7f, 8}, true, true, true},
      {50, {6, 1.7f, 8}, true, true, true}};
  SyncInput input;
  Fixture() {
    input.player = 1; input.ally = 2; input.headFocus = 30;
    input.group = candidates; input.groupCount = 2;
    input.enabled = input.playerAlive = input.allyAlive = true;
  }
  SyncDecision Tick(double seconds = 0) {
    input.elapsedSeconds = seconds;
    return policy.Tick(input);
  }
  SyncDecision Arm() {
    assert(!Tick().active);
    for (int i = 0; i < 4; ++i) assert(!Tick(.5).active);
    return Tick(.001);
  }
};

int main() {
  // Strictly more than two seconds, from actual elapsed time. A first sample
  // must never credit time before focus began, even after a long game stall.
  {
    Fixture f;
    assert(!f.Tick(100).active);
    assert(!f.Tick().active);
    assert(!f.Tick(.125).active);
    assert(!f.Tick(.375).active);
    for (int i = 0; i < 3; ++i) assert(!f.Tick(.5).active);
    assert(!f.Tick().active);
    auto decision = f.Tick(.001);
    assert(decision.active && decision.target == 20 && !decision.fire);
    assert(decision.target != f.input.headFocus);
    assert(f.policy.Target() == 20);
  }
  // Three-enemy order changes neither the selected enemy nor accumulated dwell.
  {
    Fixture f; f.input.groupCount = 3;
    assert(!f.Tick().active);
    for (int i = 0; i < 3; ++i) assert(!f.Tick(.5).active);
    std::swap(f.candidates[0], f.candidates[2]);
    assert(!f.Tick(.5).active);
    auto decision = f.Tick(.001);
    assert(decision.active && decision.target == 20);
    f.candidates[1].head.x = 9;
    assert(f.Tick().targetHead.x == 9);
  }
  // Readiness belongs to the assigned target. No input key state or waiting
  // alone is sufficient, and a missed event cannot be fired later when ready.
  {
    Fixture f; assert(f.Arm().active);
    f.input.confirmedPlayerShot = true;
    assert(!f.Tick().fire);
    f.input.confirmedPlayerShot = false;
    f.input.botReady = true; f.input.botAimTarget = 20;
    assert(!f.Tick(.5).fire);
    f.input.confirmedPlayerShot = true; f.input.botAimTarget = 30;
    assert(!f.Tick().fire);
    f.input.botAimTarget = 20;
    assert(f.Tick().fire);
    assert(!f.Tick().fire);
    f.input.confirmedPlayerShot = false;
    assert(!f.Tick(.5).fire);
    f.input.confirmedPlayerShot = true;
    assert(!f.Tick().fire);
    f.input.headFocus = 0;
    assert(!f.Tick().active);
    f.input.headFocus = 30;
    assert(f.Arm().fire);
  }
  // Each interruption discards both dwell and permission to use stale targets.
  for (int interruption = 0; interruption < 17; ++interruption) {
    Fixture f; assert(f.Arm().active);
    f.input.confirmedPlayerShot = f.input.botReady = true;
    f.input.botAimTarget = 20;
    switch (interruption) {
      case 0: f.input.enabled = false; break;
      case 1: f.input.playerAlive = false; break;
      case 2: f.input.allyAlive = false; break;
      case 3: f.input.paused = true; break;
      case 4: f.input.reviving = true; break;
      case 5: f.input.weaponSwitching = true; break;
      case 6: f.input.headFocus = 0; break;
      case 7: f.candidates[1].alive = false; break;
      case 8: f.candidates[1].hostile = false; break;
      case 9: f.candidates[1].visible = false; break;
      case 10: f.candidates[0].alive = false; break;
      case 11: f.candidates[0].visible = false; break;
      case 12: f.candidates[1].head.x = std::numeric_limits<float>::quiet_NaN(); break;
      case 13: f.input.group = nullptr; break;
      case 14: f.input.headFocus = 900; break;
      case 15: f.input.player = 0; break;
      case 16: f.input.ally = f.input.player; break;
    }
    const auto decision = f.Tick();
    assert(!decision.active && !decision.target && !decision.fire);
    Fixture fresh;
    f.input = fresh.input;
    assert(f.Arm().active);
  }
  // Changing the focused enemy, group membership, either partner, or the native
  // actor generation requires a complete new dwell, even on a firing frame.
  for (int change = 0; change < 6; ++change) {
    Fixture f; assert(f.Arm().active);
    f.input.confirmedPlayerShot = f.input.botReady = true;
    f.input.botAimTarget = 20;
    switch (change) {
      case 0: f.input.headFocus = 20; break;
      case 1: f.input.groupCount = 3; break;
      case 2: f.candidates[1].actor += Actor(1) << 32; break;
      case 3: f.input.player = 3; break;
      case 4: f.input.ally = 4; break;
      case 5: f.candidates[1].actor = 21; break;
    }
    const auto changed = f.Tick(.5);
    assert(!changed.active && !changed.fire && !changed.target);
    f.input.confirmedPlayerShot = false;
    for (int i = 0; i < 4; ++i) assert(!f.Tick(.5).active);
    const auto rearmed = f.Tick(.001);
    assert(rearmed.active && rearmed.target != f.input.headFocus && !rearmed.fire);
  }
  {
    Fixture f; f.input.groupCount = 3; assert(f.Arm().active);
    f.policy.Forget(40); // An unassigned member's death invalidates the group.
    assert(!f.policy.Target());
    assert(f.Arm().active);
    f.policy.Forget(999); // An unrelated actor must not disrupt coordination.
    assert(f.Tick().active);
    f.policy.Forget(20);
    assert(f.Arm().active);
    f.policy.Reset();
    assert(!f.Tick().active);
  }
  for (unsigned count : {0u, 1u, 4u, 999u}) {
    Fixture f; f.input.groupCount = count;
    assert(!f.Tick().active);
    for (int i = 0; i < 5; ++i) assert(!f.Tick(.5).active);
  }
  for (Actor invalid : {Actor(0), Actor(1), Actor(2), Actor(30)}) {
    Fixture f; f.candidates[1].actor = invalid;
    assert(!f.Tick().active);
    for (int i = 0; i < 5; ++i) assert(!f.Tick(.5).active);
  }
  for (double elapsed : {-1., .500001, std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
    Fixture f; assert(f.Arm().active);
    assert(!f.Tick(elapsed).active);
    assert(!f.policy.Target());
    assert(f.Arm().active);
  }
  // Hip-fire and ADS both supply their real ray to this same geometry function.
  assert(HeadRayFocus({}, {0, 0, 1}, {0, 0, 10}));
  assert(HeadRayFocus({}, {0, 0, 100}, {.19f, 0, 10}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {.21f, 0, 10}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {0, 0, -1}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {0, 0, 81}));
  assert(HeadRayFocus({}, {0, 0, 1}, {.3f, 0, 100}, 100, .4f));
  assert(!HeadRayFocus({}, {0, 0, 1}, {.3f, 0, 100}, 99, .4f));
  assert(!HeadRayFocus({}, {}, {0, 0, 10}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {0, 0, 10}, 0));
  assert(!HeadRayFocus({}, {0, 0, 1}, {0, 0, 10}, 80, -1));
  const float nan = std::numeric_limits<float>::quiet_NaN();
  assert(!HeadRayFocus({nan, 0, 0}, {0, 0, 1}, {0, 0, 10}));
  assert(!HeadRayFocus({}, {nan, 0, 1}, {0, 0, 10}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {nan, 0, 10}));
  assert(!HeadRayFocus({}, {0, 0, 1}, {0, 0, 10}, nan));
}
