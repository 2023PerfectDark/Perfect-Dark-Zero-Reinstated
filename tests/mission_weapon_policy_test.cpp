#include "../src/mission_weapon_policy.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace pdz_mission_weapons;
  // Preserve stock slots exactly, including duplicates, gaps and flag IDs.
  for (const auto& stock : {Loadout{2, 7, 3, 0}, Loadout{7, 7, 0, 0},
                           Loadout{2, 0, 7, 0}, Loadout{40, 41, 42, 43}})
    assert(PersistentLoadout(stock) == stock);
  for (unsigned slot = 0; slot < 4; ++slot) {
    Loadout live{2, 7, 3, 11};
    live[slot] = kKlobb;
    const auto snapshot = live;
    const auto saved = PersistentLoadout(live);
    assert(live == snapshot);
    unsigned out = 0;
    for (auto id : live) if (id != kKlobb) assert(saved[out++] == id);
    assert(saved[3] == 0);
  }
  assert((PersistentLoadout({39, 39, 7, 39}) == Loadout{7, 0, 0, 0}));
  assert((PersistentLoadout({39, 39, 39, 39}) == Loadout{}));
  assert((PersistentLoadout({0, 39, 7, 0}) == Loadout{7, 0, 0, 0}));
  std::cout << "Mission-only weapon save policy tests passed\n";
}
