#pragma once

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <string_view>

namespace pdz_physics {

// TU0 Mission 5's authored desk pickup, from its propsetup. Match the authored
// asset and pose, never its live (possibly displaced) position or heap address.
// Other keys, including drops and Surface's guard key, retain native physics.
inline bool IsLabDeskKey(uint32_t mission, uint32_t mode, uint32_t type,
                        uint32_t owner, std::string_view model,
                        const std::array<float, 3>& authored_position) {
  if (mission != 5 || mode != 0 || type != 14 || owner != 0 ||
      model != "aid_darkcharacter_props_niknak_keycard") return false;
  constexpr std::array<float, 3> position{-3541.062f, 787.969f, 2166.362f};
  for (unsigned i = 0; i < position.size(); ++i)
    if (!std::isfinite(authored_position[i]) ||
        std::abs(authored_position[i] - position[i]) > .01f) return false;
  return true;
}

// The game calculates keyframed collider velocity as (target - position) /
// frame_seconds. Its Havok world then advances by solver_seconds, normally
// 1/60, until it reaches the requested frame time. At high frame rates this
// overshoots the target, and later frames repeatedly reverse that velocity.
// Never integrate a collider farther than the interval used to drive it.
// Leave the original substeps in place at 60 FPS and below (including hitches).
inline float SolverStep(float frame_seconds, float solver_seconds) {
  if (!std::isfinite(frame_seconds) || !std::isfinite(solver_seconds) ||
      frame_seconds <= 0.f || solver_seconds <= 0.f)
    return solver_seconds;
  return std::min(frame_seconds, solver_seconds);
}

} // namespace pdz_physics
