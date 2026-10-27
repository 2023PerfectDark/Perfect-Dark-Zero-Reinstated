/**
 * Unit tests for the WinSystemClock to steady_clock conversion saturating
 * instead of overflowing (xenia-canary 3494b56a0).
 *
 * Ported for the ReXGlue fork.
 */

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include <rex/chrono/chrono_steady_cast.h>

using rex::chrono::WinSystemClock;

TEST_CASE("A due time at the NT epoch converts to the past", "[chrono]") {
  // Guests pass an absolute due time of 0 to mean "already elapsed".
  auto steady =
      std::chrono::clock_cast<std::chrono::steady_clock>(WinSystemClock::from_file_time(0));
  CHECK(steady <= std::chrono::steady_clock::now());
}

TEST_CASE("A near due time still converts precisely", "[chrono]") {
  auto due = WinSystemClock::now() + std::chrono::seconds(10);
  auto steady = std::chrono::clock_cast<std::chrono::steady_clock>(due);
  auto from_now = steady - std::chrono::steady_clock::now();
  CHECK(from_now > std::chrono::seconds(9));
  CHECK(from_now <= std::chrono::seconds(10));
}
