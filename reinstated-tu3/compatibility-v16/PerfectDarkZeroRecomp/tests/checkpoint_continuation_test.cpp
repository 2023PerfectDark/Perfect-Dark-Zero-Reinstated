#include "../../sdk/include/rex/system/checkpoint_continuation.h"
#include "../src/debug_checkpoint_slot.h"
#include <array>
#include <cassert>
#include <iostream>
#include <string>

using namespace rex::runtime::checkpoint;
// These functions have the shape required of resumable generated PPC code:
// explicit guest continuation IDs and local register values. The import at
// the leaf has an observable side effect that must never execute twice.
struct World { uint32_t purchases = 0, ammo = 10, result = 0; } world;
Execution execution;
Continuation captured;
World captured_world;
bool saving = false;

template<class T> std::span<uint8_t> Bytes(T& value) {
  static_assert(std::is_trivially_copyable_v<T>);
  return {reinterpret_cast<uint8_t*>(&value), sizeof(value)};
}
void Leaf() {
  struct Locals { uint32_t bonus = 0; } locals;
  Frame frame(execution, 0x82003000, Bytes(locals));
  switch (frame.resume_point()) {
    case 0: break;
    case 0x82003010: goto resume;
    default: throw std::runtime_error("Unknown leaf continuation");
  }
  ++world.purchases;
  world.ammo += 7;
  locals.bonus = 3;
resume:
  frame.At(0x82003010);
  if (saving) { captured = execution.Capture(); captured_world = world; }
  world.result += world.ammo + locals.bonus;
}
void Middle() {
  struct Locals { uint32_t multiplier = 0; } locals;
  Frame frame(execution, 0x82002000, Bytes(locals));
  switch (frame.resume_point()) {
    case 0: break;
    case 0x82002010: goto resume;
    default: throw std::runtime_error("Unknown middle continuation");
  }
  locals.multiplier = 4;
resume:
  frame.At(0x82002010, 0x82003000);
  Leaf();
  world.result *= locals.multiplier;
}
void Root() {
  struct Locals { uint32_t add = 0; } locals;
  Frame frame(execution, 0x82001000, Bytes(locals));
  switch (frame.resume_point()) {
    case 0: break;
    case 0x82001010: goto resume;
    default: throw std::runtime_error("Unknown root continuation");
  }
  locals.add = 11;
resume:
  frame.At(0x82001010, 0x82002000);
  Middle();
  world.result += locals.add;
}

void ValidateWorld() {
  assert(world.purchases == 1 && world.ammo == 17 && world.result == 91);
  assert(execution.RestoreConsumed() && execution.idle());
}
int main(int argc, char** argv) {
  const std::filesystem::path profile = argc > 2 ? argv[2] : ".";
  if (argc > 1 && std::string(argv[1]) == "capture") {
    saving = true;
    Root();
    ValidateWorld();
    pdz_checkpoint::Snapshot snapshot;
    snapshot.mission = 1;
    snapshot.build[0] = 0x43;
    snapshot.payload.assign(Bytes(captured_world).begin(), Bytes(captured_world).end());
    auto bytes = captured.Encode();
    snapshot.payload.insert(snapshot.payload.end(), bytes.begin(), bytes.end());
    assert(pdz_checkpoint::Slot(profile).Save(snapshot));
    std::cout << "Captured three nested frames; original process exits.\n";
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "restore") {
    pdz_checkpoint::Snapshot snapshot;
    std::array<uint8_t, 32> build{}; build[0] = 0x43;
    assert(pdz_checkpoint::Slot(profile).Load(build, snapshot));
    assert(snapshot.payload.size() > sizeof(World));
    std::memcpy(&world, snapshot.payload.data(), sizeof(world));
    Continuation continuation;
    assert(Continuation::Decode(std::span(snapshot.payload).subspan(sizeof(World)), continuation));
    execution.Restore(std::move(continuation));
    Root();
    ValidateWorld();
    std::cout << "Fresh process resumed all three frames; no repeated side effect.\n";
    return 0;
  }
  saving = true; Root(); ValidateWorld(); saving = false;
  const auto encoded = captured.Encode();
  for (size_t length = 0; length < encoded.size(); ++length) {
    Continuation output = captured;
    assert(!Continuation::Decode(std::span(encoded).first(length), output));
    assert(output.Encode() == encoded);
  }
  auto extra = encoded; extra.push_back(0);
  Continuation output;
  assert(!Continuation::Decode(extra, output));
  auto excessive = encoded; excessive[4] = excessive[5] = 255;
  assert(!Continuation::Decode(excessive, output));
  world = captured_world;
  assert(Continuation::Decode(encoded, output));
  execution.Restore(output); Root(); ValidateWorld();
  // A mismatch must not consume a frame or mutate the caller's local storage.
  Execution mismatch; mismatch.Restore(captured);
  uint32_t untouched = 123;
  try { Frame wrong(mismatch, 0xBAD, Bytes(untouched)); assert(false); }
  catch (const std::runtime_error&) { assert(untouched == 123 && mismatch.idle()); }
  // Never serialize across an unadapted native hook, even with valid frames.
  Execution native;
  Frame live(native, 0x82004000, Bytes(untouched)); live.At(0x82004004);
  { Execution::NativeScope blocked(native);
    try { (void)native.Capture(); assert(false); } catch (const std::runtime_error&) {} }
  assert(native.Capture().frames.size() == 1);
  // A host thread may execute two guest fibers. Their call stacks must remain
  // distinct, and nested bindings must restore the previous execution.
  Execution fiber_a, fiber_b;
  { Binding a(fiber_a);
    assert(&Current()==&fiber_a);
    { Binding b(fiber_b); assert(&Current()==&fiber_b); }
    assert(&Current()==&fiber_a);
  }
  try { (void)Current(); assert(false); } catch(const std::runtime_error&) {}
  std::cout << "Continuation validation and same-process replay passed.\n";
}
