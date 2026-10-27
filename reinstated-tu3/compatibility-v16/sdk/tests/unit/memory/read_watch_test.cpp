/**
 * @file        memory/read_watch_test.cpp
 * @brief       Read watches on physical memory (rex/system/xmemory_read_watch.h)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

#include <rex/memory.h>
#include <rex/platform.h>
#include <rex/system/xmemory.h>
#include <rex/system/xmemory_read_watch.h>
#include <rex/thread.h>

#include "test_memory.h"

namespace {

struct ReadWatchLog {
  rex::memory::Memory* memory = nullptr;
  std::vector<std::pair<uint32_t, uint32_t>> calls;
  // Written at fill_physical by the callback when not 0, as a data provider
  // would.
  uint32_t marker = 0;
  uint32_t fill_physical = 0;
  // What the callback asks to unwatch along with the touched page.
  std::pair<uint32_t, uint32_t> unwatch = {0, 0};
};

std::pair<uint32_t, uint32_t> OnRead(void* context, std::unique_lock<std::recursive_mutex>&,
                                     uint32_t physical_address_start, uint32_t length) {
  auto& log = *static_cast<ReadWatchLog*>(context);
  log.calls.emplace_back(physical_address_start, length);
  if (log.marker) {
    std::memcpy(log.memory->TranslatePhysical(log.fill_physical), &log.marker, sizeof(log.marker));
  }
  return log.unwatch;
}

bool IsWatched(rex::memory::Memory& memory, uint32_t physical_address) {
  auto lock = rex::thread::global_critical_region::AcquireDirect();
  return rex::memory::IsPhysicalMemoryReadWatched(memory, physical_address);
}

rex::memory::PageAccess HostAccess(rex::memory::Memory& memory, uint32_t guest_address) {
  size_t length = rex::memory::page_size();
  rex::memory::PageAccess access = rex::memory::PageAccess::kNoAccess;
  rex::memory::QueryProtect(memory.TranslateVirtual(guest_address), length, access);
  return access;
}

bool Covers(const std::vector<std::pair<uint32_t, uint32_t>>& calls, uint32_t address) {
  for (const auto& [start, length] : calls) {
    if (address >= start && address - start < length) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Physical memory read watch", "[memory][physical]") {
  auto& memory = rex::testing::GetTestMemory();
  auto* heap = const_cast<rex::memory::BaseHeap*>(memory.LookupHeap(0xA0000000));
  REQUIRE(heap != nullptr);
  const uint32_t page = uint32_t(rex::memory::page_size());

  ReadWatchLog log;
  log.memory = &memory;
  void* handle = rex::memory::RegisterPhysicalMemoryReadCallback(OnRead, &log);

  constexpr uint32_t kSize = 0x10000;
  uint32_t addr = 0;
  REQUIRE(heap->Alloc(kSize, kSize,
                      rex::memory::kMemoryAllocationReserve | rex::memory::kMemoryAllocationCommit,
                      rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite, false,
                      &addr));
  const uint32_t physical = addr - 0xA0000000;
  bool released = false;

  rex::memory::EnablePhysicalMemoryReadWatch(memory, physical, kSize);
  CHECK(IsWatched(memory, physical));
  CHECK(IsWatched(memory, physical + kSize - 1));
  CHECK(HostAccess(memory, addr) == rex::memory::PageAccess::kNoAccess);
  CHECK(log.calls.empty());

  SECTION("Disabling runs the callbacks and gives the pages back their access") {
    rex::memory::DisablePhysicalMemoryReadWatch(memory, physical, page);
    CHECK(Covers(log.calls, physical));
    CHECK_FALSE(IsWatched(memory, physical));
    CHECK(HostAccess(memory, addr) == rex::memory::PageAccess::kReadWrite);
    // The callback asked for nothing more: the next page stays watched.
    CHECK(IsWatched(memory, physical + page));
    CHECK(HostAccess(memory, addr + page) == rex::memory::PageAccess::kNoAccess);
  }

  SECTION("A protection change keeps the watch; making it writable drops it quietly") {
    REQUIRE(heap->Protect(addr, kSize, rex::memory::kMemoryProtectRead));
    CHECK(IsWatched(memory, physical));
    CHECK(HostAccess(memory, addr) == rex::memory::PageAccess::kNoAccess);
    REQUIRE(heap->Protect(addr, kSize,
                          rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite));
    CHECK_FALSE(IsWatched(memory, physical));
    CHECK(HostAccess(memory, addr) != rex::memory::PageAccess::kNoAccess);
    CHECK(log.calls.empty());
  }

  SECTION("Releasing the memory drops the watch without the callbacks") {
    REQUIRE(heap->Release(addr, nullptr));
    released = true;
    CHECK_FALSE(IsWatched(memory, physical));
    CHECK(log.calls.empty());
  }

#if REX_PLATFORM_WIN32
  // On POSIX, Catch2 takes over SIGSEGV while a test runs, so the fault would
  // never reach the memory system's handler.
  SECTION("The guest's first read runs the callbacks and sees what they put there") {
    log.marker = 0xA5A55A5Au;
    log.fill_physical = physical + 0x10;
    log.unwatch = {physical, kSize};
    auto* value = reinterpret_cast<volatile uint32_t*>(memory.TranslateVirtual(addr + 0x10));
    CHECK(*value == 0xA5A55A5Au);
    CHECK(log.calls.size() == 1);
    CHECK_FALSE(IsWatched(memory, physical));
    CHECK_FALSE(IsWatched(memory, physical + kSize - 1));

    // A write to a watched page is an access too: the callbacks run first.
    log.marker = 0;
    rex::memory::EnablePhysicalMemoryReadWatch(memory, physical, kSize);
    *value = 7;
    CHECK(log.calls.size() == 2);
    CHECK(*value == 7);
    CHECK_FALSE(IsWatched(memory, physical));
  }
#endif

  if (!released) {
    rex::memory::UnregisterPhysicalMemoryReadCallback(handle);
    handle = nullptr;
    rex::memory::DisablePhysicalMemoryReadWatch(memory, physical, kSize);
    heap->Release(addr, nullptr);
  }
  if (handle) {
    rex::memory::UnregisterPhysicalMemoryReadCallback(handle);
  }
}
