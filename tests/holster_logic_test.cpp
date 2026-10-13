#include "../src/holster_logic.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace pdz_holsters;
  std::array<Weapon, 7> inventory{{
      {1, 101, 201, false, true}, {2, 102, 202, true, false},
      {3, 103, 203, false, false}, {4, 104, 204, true, false},
      {5, 105, 205, true, false}, {6, 106, 206, false, false},
      {7, 107, 207, false, false}}};
  auto layout = Select(inventory);
  assert(layout[0].object == 2 && layout[1].object == 4);
  assert(layout[2].object == 3 && layout[3].object == 6);
  // Switching from a pistol to the first long gun replaces both holsters.
  inventory[0].equipped = false; inventory[1].equipped = true;
  layout = Select(inventory);
  assert(layout[0].object == 4 && layout[1].object == 5);
  assert(layout[2].object == 1 && layout[3].object == 3);
  // Both hands are excluded when dual wielding; missing models are skipped.
  inventory[2].equipped = true; inventory[3].model = 0;
  layout = Select(inventory);
  assert(layout[0].object == 5 && layout[1].object == 0);
  assert(layout[2].object == 1 && layout[3].object == 6);
  inventory[6] = inventory[0];
  layout = Select(inventory);
  assert(layout[3].object == 6);
  assert(Select({})[0].object == 0);
  assert(SelectIntroWeapon(inventory).object == 3); // Equipped pistol wins.
  inventory[2].equipped=false;
  assert(SelectIntroWeapon(inventory).object == 1); // Rifle equipped: use stored pistol.
  for(auto& weapon:inventory)weapon.two_handed=true;
  assert(SelectIntroWeapon(inventory).object == 0); // Only long guns: P9P fallback.
  assert(SelectIntroWeapon({}).object == 0);
  std::cout << "Holster selection tests passed\n";
}
