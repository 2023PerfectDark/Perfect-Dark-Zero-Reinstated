#include "../src/weapon_challenge.h"
#include "../src/campaign_logic.h"
#include <cassert>
#include <limits>
#include <iostream>

int main() {
  using namespace pdz_campaign;
  CheckpointRecovery checkpoint;
  // An already reached checkpoint on first observation is not a new crossing.
  assert(!checkpoint.Observe(true));
  checkpoint.ResetMission();
  float current = 20, permanent_ceiling = 50;
  assert(!checkpoint.Observe(false));
  assert(checkpoint.Observe(true));
  current = CheckpointHealth(current, permanent_ceiling);
  assert(current == 50 && permanent_ceiling == 50);
  // Taking more damage after crossing must not turn the checkpoint into regen.
  current = 10;
  for (int i = 0; i < 1200; ++i) {
    if (checkpoint.Observe(true)) current = CheckpointHealth(current, permanent_ceiling);
  }
  assert(current == 10);
  checkpoint.ResetMission();
  assert(checkpoint.Observe(true));
  assert(!checkpoint.Observe(true));
  // A newly cleared/reached flag can mark the next original checkpoint.
  assert(!checkpoint.Observe(false));
  assert(checkpoint.Observe(true));
  assert(CheckpointHealth(0, 50) == 0);
  assert(CheckpointHealth(-10, 50) == -10);
  assert(CheckpointHealth(75, 50) == 75);
  assert(CheckpointHealth(20, 101) == 20);
  assert(CheckpointHealth(20, std::numeric_limits<float>::quiet_NaN()) == 20);
  assert(CheckpointHealth(20, 100) == 100); // purely shock damage
  // Time advances without a heal, pauses freeze it, and another hit resets it.
  float elapsed = 0;
  for (int i = 0; i < 1200; ++i) elapsed = AdvanceShockTimer(elapsed, 1.f / 60);
  assert(elapsed == 5 && current == 10);
  assert(AdvanceShockTimer(1, 0) == 1);
  assert(AdvanceShockTimer(0, 0.5f) == 0.5f);
  assert(!HideClassicHealth(2.9f, 0.05f));
  assert(HideClassicHealth(2.99f, 0.02f));
  assert(!HideClassicHealth(0, 0.02f));
  assert(!HideClassicHealth(2, std::numeric_limits<float>::quiet_NaN()));
  // Shield protection, breaking-hit overflow, unshielded hits, sniper bypass.
  auto hit = ModernShieldDamage(12, 8, 50, false);
  assert(hit.health == 0 && hit.shield == 20);
  hit = ModernShieldDamage(12, 8, 5, false);
  assert(hit.health == 15 && hit.shield == 5);
  hit = ModernShieldDamage(20, 0, 0, false);
  assert(hit.health == 20 && hit.shield == 0);
  hit = ModernShieldDamage(12, 8, 50, true);
  assert(hit.health == 20 && hit.shield == 0);
  hit = ModernShieldDamage(0, 0, 50, false);
  assert(hit.health == 0 && hit.shield == 0);
  // The challenge catalog has unique playable IDs and the requested reserves.
  for (const auto& weapon : pdz_challenge::weapons) {
    assert(!weapon.name.empty());
    assert(weapon.reserve == 400 || weapon.reserve == 800);
    unsigned matches = 0;
    for (const auto& other : pdz_challenge::weapons) matches += other.id == weapon.id;
    assert(matches == 1);
  }
  assert(pdz_challenge::Find(3)->reserve == 800); // DW-P5
  assert(pdz_challenge::Find(2)->reserve == 800); // P9P
  assert(pdz_challenge::Find(23)->reserve == 400); // Mayhem
  assert(pdz_challenge::Find(37)->reserve == 400); // Jackal
  assert(!pdz_challenge::Find(0));
  assert(!pdz_challenge::Find(40)); // Multiplayer objective flags are not weapons.
  std::cout << "Campaign health tests passed\n";
}
