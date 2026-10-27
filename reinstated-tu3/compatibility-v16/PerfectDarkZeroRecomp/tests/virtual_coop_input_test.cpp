#include "../../sdk/include/rex/system/xam/virtual_coop_input.h"
#include <cassert>

int main() {
  using namespace rex::system::xam;
  VirtualCoopInput input;
  assert(input.Read(0).command == VirtualCoopCommand{});
  VirtualCoopCommand fire;
  fire.right_trigger = 255;
  fire.left_y = 32767;
  input.Submit(fire, 1000);
  const auto first = input.Read(1000);
  assert(first.command == fire);
  assert(input.Read(1249).packet == first.packet);
  const auto expired = input.Read(1250);
  assert(expired.command == VirtualCoopCommand{});
  assert(expired.packet != first.packet);
  assert(input.Read(2000).packet == expired.packet);
  input.Submit(fire, 2000);
  input.Submit(fire, 2200);
  assert(input.Read(2400).command == fire);
  input.Reset();
  assert(input.Read(2401).command == VirtualCoopCommand{});
  input.Submit(fire, 3000);
  assert(input.Read(2999).command == VirtualCoopCommand{});
}
