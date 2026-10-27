#include "generated/default/perfectdarkzerorecomp_init.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <random>

namespace {
using KeyId = std::array<uint8_t, 8>;
using Key = std::array<uint8_t, 16>;
std::mutex key_mutex;
std::map<KeyId, Key> keys;
bool Range(uint32_t address, uint32_t length, bool write = false) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !address || uint64_t(address) + length > 0x100000000ull) return false;
  for (uint64_t a = address; a < uint64_t(address) + length;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(a)); uint32_t protect = 0;
    if (!heap || !heap->QueryProtect(uint32_t(a), &protect) ||
        !(protect & rex::memory::kMemoryProtectRead) ||
        (write && !(protect & rex::memory::kMemoryProtectWrite))) return false;
    a = (a / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
}

// Local Combat Arena uses the same XNet key lifecycle as network sessions.
// SDK stubs left r3=1, causing PDZ to abort immediately with "Lost network
// connection". Retain real per-session key data; this does not add transport
// encryption, remote peer discovery, Xbox Live or network authentication.
REX_HOOK_RAW(sub_827639F0) {
  const auto id = ctx.r3.u32, key = ctx.r4.u32;
  if (!Range(id, 8, true) || !Range(key, 16, true)) { ctx.r3.u64 = 10022; return; }
  try {
    std::random_device random;
    for (unsigned i = 0; i < 8; ++i) base[id + i] = uint8_t(random());
    // System-link session ID, matching the retail XNKID type bit convention.
    base[id] = (base[id] & 0x0f) | 0x80;
    for (unsigned i = 0; i < 16; ++i) base[key + i] = uint8_t(random());
    ctx.r3.u64 = 0;
    REXLOG_INFO("PDZ local session key created");
  } catch (...) { ctx.r3.u64 = 10055; }
}
REX_HOOK_RAW(sub_82763A00) {
  const auto id_address = ctx.r3.u32, key_address = ctx.r4.u32;
  if (!Range(id_address, 8) || !Range(key_address, 16)) { ctx.r3.u64 = 10022; return; }
  KeyId id; Key key;
  std::memcpy(id.data(), base + id_address, id.size());
  std::memcpy(key.data(), base + key_address, key.size());
  std::lock_guard lock(key_mutex);
  if (keys.size() >= 64 && !keys.contains(id)) { ctx.r3.u64 = 10055; return; }
  keys[id] = key; ctx.r3.u64 = 0;
}
REX_HOOK_RAW(sub_82763A10) {
  if (!Range(ctx.r3.u32, 8)) { ctx.r3.u64 = 10022; return; }
  KeyId id; std::memcpy(id.data(), base + ctx.r3.u32, id.size());
  std::lock_guard lock(key_mutex); keys.erase(id); ctx.r3.u64 = 0;
}
