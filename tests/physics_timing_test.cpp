#include "../src/physics_timing.h"

#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

// Regression model of the actual call sequence: 825D50C8/824DA9B8 derive a
// target collider velocity from the game frame interval; 829B61D0's virtual
// simulation accumulates frame time and integrates by its supplied substep.
struct MovingCollider {
  // Start slightly displaced, as a contact can push a body off its pose.
  double time{}, simulated{}, target{}, position{.05}, max_error{};
  void Frame(float dt, bool fix) {
    constexpr float native_step = 1.f / 60.f;
    const double step = fix ? pdz_physics::SolverStep(dt, native_step) : native_step;
    time += dt;
    target += 2.0 * dt;
    const double velocity = (target - position) / dt;
    while (simulated + 1e-9 < time) {
      position += velocity * step;
      simulated += step;
    }
    max_error = std::max(max_error, std::abs(position - target));
  }
};

int main() {
  constexpr auto key_model = "aid_darkcharacter_props_niknak_keycard";
  constexpr std::array<float, 3> key_position{-3541.062f, 787.969f, 2166.362f};
  assert(pdz_physics::IsLabDeskKey(5, 0, 14, 0, key_model, key_position));
  for (unsigned mission : {0, 4, 6, 14})
    assert(!pdz_physics::IsLabDeskKey(mission, 0, 14, 0, key_model, key_position));
  assert(!pdz_physics::IsLabDeskKey(5, 1, 14, 0, key_model, key_position));
  assert(!pdz_physics::IsLabDeskKey(5, 0, 8, 0, key_model, key_position));
  assert(!pdz_physics::IsLabDeskKey(5, 0, 14, 0x10000, key_model, key_position));
  assert(!pdz_physics::IsLabDeskKey(5, 0, 14, 0, "another_key", key_position));
  for (unsigned axis = 0; axis < 3; ++axis) {
    auto moved = key_position;
    moved[axis] += 1.f;
    assert(!pdz_physics::IsLabDeskKey(5, 0, 14, 0, key_model, moved));
    moved[axis] = std::numeric_limits<float>::quiet_NaN();
    assert(!pdz_physics::IsLabDeskKey(5, 0, 14, 0, key_model, moved));
  }
  constexpr float step = 1.f / 60.f;
  // Native slow-frame/substep behavior and invalid inputs stay unchanged.
  assert(pdz_physics::SolverStep(1.f / 30.f, step) == step);
  assert(pdz_physics::SolverStep(.1f, step) == step);
  assert(pdz_physics::SolverStep(step, step) == step);
  assert(pdz_physics::SolverStep(0.f, step) == step);
  assert(pdz_physics::SolverStep(-1.f, step) == step);
  assert(pdz_physics::SolverStep(std::numeric_limits<float>::infinity(), step) == step);
  assert(pdz_physics::SolverStep(std::numeric_limits<float>::quiet_NaN(), step) == step);

  for (int fps : {60, 90, 120, 144, 165, 240, 360}) {
    MovingCollider collider;
    for (int i = 0; i < fps * 10; ++i) collider.Frame(1.f / fps, true);
    // Ten seconds remains ten seconds, without rate-dependent overshoot.
    assert(std::abs(collider.position - 20.) < .0001);
    assert(collider.max_error < .0001);
  }
  MovingCollider jitter;
  for (int i = 0; i < 1000; ++i)
    for (float dt : std::array{1.f / 90, 1.f / 240, 1.f / 144, 1.f / 60})
      jitter.Frame(dt, true);
  assert(jitter.max_error < .0001);

  // Confirm that this scenario actually exposes the old overshoot.
  MovingCollider original;
  for (int i = 0; i < 240; ++i) original.Frame(1.f / 240, false);
  assert(original.max_error > 1.0);
  std::cout << "Physics timing tests passed (60-360 FPS and variable frame times)\n";
}
