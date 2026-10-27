#include "../src/input_logic.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace pdz_input;
  // Wheel zoom clamps at the Xbox trigger limits in both directions.
  assert(AimPressure(128, 1) == 144);
  assert(AimPressure(128, -1) == 112);
  assert(AimPressure(250, 10) == 255);
  assert(AimPressure(40, -10) == 32);
  assert(AimPressure(128, 0) == 128);
  // Offhand firing must not depend on the single-weapon zoom setting.
  for (int pressure : {32, 64, 128, 255}) {
    assert(MergeMouseAimTrigger(0, true, true, pressure) == 255);
    assert(MergeMouseAimTrigger(0, true, false, pressure) == pressure);
    assert(MergeMouseAimTrigger(0, false, true, pressure) == 0);
    assert(MergeMouseAimTrigger(173, false, true, pressure) == 173);
    assert(MergeMouseAimTrigger(255, true, false, pressure) == 255);
  }
  // Repeated wheel events must have a release edge between presses, even at
  // high guest poll rates, and preserve their original direction/order.
  ButtonPulses pulses;
  pulses.Push(1); pulses.Push(1); pulses.Push(2);
  assert(pulses.Poll(0) == 1);
  assert(pulses.Poll(1) == 1);
  assert(pulses.Poll(79) == 1);
  assert(pulses.Poll(80) == 0);
  assert(pulses.Poll(129) == 0);
  assert(pulses.Poll(130) == 1);
  assert(pulses.Poll(210) == 0);
  assert(pulses.Poll(260) == 2);
  // Focus loss or a mode transition must drop both held and queued actions.
  pulses.Clear();
  assert(pulses.Poll(261) == 0);
  assert(pulses.Poll(1000) == 0);
  // Poll gaps cannot turn two events into one continuously held button.
  pulses.Push(0x1000); pulses.Push(0x1000);
  assert(pulses.Poll(1100) == 0x1000);
  assert(pulses.Poll(2100) == 0);
  assert(pulses.Poll(2150) == 0x1000);
  std::array<int, 13> sizes{};
  sizes.fill(1);
  auto layout = PaginateInventory(sizes, 13);
  assert(layout.pages == 4 && layout.starts[3] == 12 && layout.starts[4] == 13);
  sizes.fill(3);
  layout = PaginateInventory(sizes, 13);
  assert(layout.pages == 13 && layout.starts[12] == 12 && layout.starts[13] == 13);
  sizes = {3, 1, 2, 2, 1};
  layout = PaginateInventory(sizes, 5);
  assert(layout.pages == 3 && layout.starts[1] == 2 && layout.starts[2] == 4);
  assert(PaginateInventory(sizes, 0).pages == 1);
  std::cout << "Input logic tests passed\n";
}
