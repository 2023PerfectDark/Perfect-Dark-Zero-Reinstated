#pragma once
#include <algorithm>
#include <cmath>

namespace pdz_nightclub {
enum class SuspicionStage { Greeting, Suspicious, Aiming, Hostile };

// Per-guard exposure. Leaving pauses suspicion; only a mission reset clears it.
struct Suspicion {
  double seconds{};
  bool hostile{};
  SuspicionStage Stage() const {
    if (hostile || seconds >= 15.) return SuspicionStage::Hostile;
    if (seconds >= 10.) return SuspicionStage::Aiming;
    if (seconds >= 6.) return SuspicionStage::Suspicious;
    return SuspicionStage::Greeting;
  }
  SuspicionStage Update(double elapsed, bool nearby, bool unarmed,
                        bool playing, bool attacked) {
    if (!playing) return Stage();
    if (attacked || (nearby && !unarmed)) hostile = true;
    if (!hostile && nearby && unarmed && std::isfinite(elapsed) && elapsed > 0.)
      seconds = std::min(15., seconds + elapsed);
    if (seconds >= 15.) hostile = true;
    return Stage();
  }
};
}
