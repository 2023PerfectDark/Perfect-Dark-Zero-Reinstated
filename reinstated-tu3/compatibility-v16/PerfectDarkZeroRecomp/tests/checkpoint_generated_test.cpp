#include <rex/ppc/context.h>
#include <rex/system/checkpoint_continuation.h>
#include <rex/system/checkpoint_context.h>
#include "../src/debug_checkpoint_slot.h"
#include <iostream>
#include <stdexcept>
void checkpoint_82001000(PPCContext&, uint8_t*);
void checkpoint_82001040(PPCContext&, uint8_t*);
void CheckpointIndirect(uint32_t address, PPCContext& ctx, uint8_t* base) {
  if (address != 0x82001040) throw std::runtime_error("Restored wrong indirect call target");
  checkpoint_82001040(ctx, base);
}
using namespace rex::runtime::checkpoint;
void Check(bool ok) { if (!ok) throw std::runtime_error("Generated continuation regression"); }
void TestContextCodec() {
  PPCContext original{};
  original.r0.u64=0x0123456789abcdefull; original.r31.u64=0xfedcba9876543210ull;
  original.f0.u64=0x7ff8000000000011ull; original.f31.u64=0x8000000000000000ull;
  for (unsigned i=0;i<16;++i) { original.v0.u8[i]=uint8_t(i+1); original.v127.u8[i]=uint8_t(255-i); }
  original.cr0.set_raw(9); original.cr7.set_raw(6);
  original.xer.ca=1; original.xer.ov=1; original.xer.so=1;
  original.reserved.u64=0x1020304050607080ull; original.lr=0x82000018;
  original.ctr.u64=0x82000100; original.msr=0x200a000;
  original.last_indirect_target=0x82000200; original.vscr_sat=1;
  auto bytes=ContextCodec::Encode(original);
  Check(bytes.size()==ContextCodec::kSize);
  PPCContext restored{};
  Check(ContextCodec::Decode(bytes,restored));
  Check(restored.r0.u64==original.r0.u64 && restored.r31.u64==original.r31.u64);
  Check(restored.f0.u64==original.f0.u64 && restored.f31.u64==original.f31.u64);
  Check(!std::memcmp(restored.v0.u8,original.v0.u8,16));
  Check(!std::memcmp(restored.v127.u8,original.v127.u8,16));
  Check(ContextCodec::Encode(restored)==bytes);
  for(size_t length=0;length<bytes.size();++length) {
    Check(!ContextCodec::Decode(std::span(bytes).first(length),restored));
    Check(ContextCodec::Encode(restored)==bytes);
  }
  auto invalid=bytes; invalid.back()=2;
  Check(!ContextCodec::Decode(invalid,restored));
  Check(ContextCodec::Encode(restored)==bytes);
}
int main(int argc, char** argv) {
  if (argc != 3) return 1;
  TestContextCodec();
  Execution execution;
  Binding binding(execution);
  PPCContext ctx{}; ctx.r6.u64 = 10;
  pdz_checkpoint::Slot slot(argv[2]);
  std::array<uint8_t,32> build{}; build[0] = 0x47;
  bool captured = false;
  if (std::string(argv[1]) == "capture") {
    execution.safe_point = [&](Execution& running) {
      auto state = running.Capture();
      if (state.frames.back().function != 0x82001040 || captured) return;
      captured = true;
      pdz_checkpoint::Snapshot snapshot;
      snapshot.mission = 1; snapshot.build = build;
      snapshot.payload = ContextCodec::Encode(ctx);
      Check(snapshot.payload.size() == ContextCodec::kSize);
      auto frames = state.Encode();
      snapshot.payload.insert(snapshot.payload.end(), frames.begin(), frames.end());
      Check(slot.Save(snapshot));
    };
  } else if (std::string(argv[1]) == "restore") {
    pdz_checkpoint::Snapshot snapshot;
    Check(slot.Load(build, snapshot) && snapshot.payload.size() >= ContextCodec::kSize);
    Check(ContextCodec::Decode(std::span(snapshot.payload).first(ContextCodec::kSize), ctx));
    Continuation state;
    Check(Continuation::Decode(std::span(snapshot.payload).subspan(ContextCodec::kSize), state));
    execution.Restore(std::move(state));
  } else return 1;
  checkpoint_82001000(ctx, nullptr);
  Check(ctx.r3.u64 == 91 && ctx.r5.u64 == 1 && ctx.r6.u64 == 17);
  Check(execution.RestoreConsumed() && execution.idle());
  Check(std::string(argv[1]) != "capture" || captured);
  std::cout << argv[1] << ": generated PPC result91, side effect executed once.\n";
}
