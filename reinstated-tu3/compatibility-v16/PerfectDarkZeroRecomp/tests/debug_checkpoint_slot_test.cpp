#include "../src/debug_checkpoint_slot.h"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
  using namespace pdz_checkpoint;
  const auto root = std::filesystem::temp_directory_path() /
      ("pdz-checkpoint-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Slot slot(root);
  Snapshot first{1, 0, 1, {}, {1, 2, 3, 4}}, loaded;
  first.build[0] = 42;
  assert(slot.Save(first));
  assert(slot.Load(first.build, loaded) && loaded.payload == first.payload);
  auto second = first; second.mission = 5; second.payload = {8, 7};
  assert(slot.Save(second));
  assert(slot.Load(first.build, loaded) && loaded.mission == 5 && loaded.payload == second.payload);
  auto invalid = second; invalid.payload.clear();
  assert(!slot.Save(invalid));
  assert(slot.Load(first.build, loaded) && loaded.mission == 5);
  auto wrong_build = first.build; wrong_build[1] = 1;
  assert(!slot.Load(wrong_build, loaded) && loaded.mission == 5);
  const auto file = root / "debug-checkpoint" / "single-mission.bin";
  // An interrupted temporary write cannot replace the committed slot.
  { std::ofstream out(file.string() + ".tmp"); out << "partial"; }
  assert(slot.Load(first.build, loaded) && loaded.mission == 5);
  { std::fstream out(file, std::ios::binary | std::ios::in | std::ios::out);
    out.seekp(60); out.put(0); }
  assert(!slot.Load(first.build, loaded) && loaded.mission == 5);
  assert(slot.Save(second));
  std::filesystem::resize_file(file, 16);
  assert(!slot.Load(first.build, loaded));
  std::filesystem::remove_all(root);
  std::cout << "Checkpoint slot: replacement, build isolation, interruption and corruption checks passed.\n";
}
