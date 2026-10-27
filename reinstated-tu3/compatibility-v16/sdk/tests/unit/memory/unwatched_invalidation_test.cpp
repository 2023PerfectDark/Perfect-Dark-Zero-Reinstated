/**
 * @file        memory/unwatched_invalidation_test.cpp
 * @brief       Physical heap Protect/Release raise invalidation callbacks without a watch
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <vector>

#include <rex/system/xmemory.h>

#include "test_memory.h"

namespace {

struct Invalidation {
  uint32_t start;
  uint32_t length;
};

std::pair<uint32_t, uint32_t> RecordInvalidation(void* context, uint32_t physical_address_start,
                                                 uint32_t length, bool /*exact_range*/) {
  static_cast<std::vector<Invalidation>*>(context)->push_back({physical_address_start, length});
  return {0, UINT32_MAX};
}

bool Covers(const std::vector<Invalidation>& calls, uint32_t start, uint32_t length) {
  for (const Invalidation& call : calls) {
    if (call.start <= start && uint64_t(call.start) + call.length >= uint64_t(start) + length) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Physical heap invalidates unwatched ranges made writable or freed",
          "[memory][physical]") {
  auto& memory = rex::testing::GetTestMemory();
  auto* heap = const_cast<rex::memory::BaseHeap*>(memory.LookupHeap(0xA0000000));
  REQUIRE(heap != nullptr);

  std::vector<Invalidation> calls;
  void* handle = memory.RegisterPhysicalMemoryInvalidationCallback(RecordInvalidation, &calls);

  // Read-only, and nothing ever enabled access callbacks on it: no watch is
  // armed, as for a buffer the GPU read while the guest kept it read-only.
  constexpr uint32_t kSize = 0x10000;
  uint32_t addr = 0;
  REQUIRE(heap->Alloc(kSize, kSize,
                      rex::memory::kMemoryAllocationReserve | rex::memory::kMemoryAllocationCommit,
                      rex::memory::kMemoryProtectRead, false, &addr));
  const uint32_t physical = addr - 0xA0000000;
  calls.clear();

  SECTION("Making the range writable announces a write") {
    REQUIRE(heap->Protect(addr, kSize,
                          rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite));
    CHECK(Covers(calls, physical, kSize));
  }

  SECTION("Marking the range read-only does not") {
    REQUIRE(heap->Protect(addr, kSize, rex::memory::kMemoryProtectRead));
    CHECK(calls.empty());
  }

  calls.clear();
  heap->Release(addr, nullptr);
  CHECK(Covers(calls, physical, kSize));

  memory.UnregisterPhysicalMemoryInvalidationCallback(handle);
}
