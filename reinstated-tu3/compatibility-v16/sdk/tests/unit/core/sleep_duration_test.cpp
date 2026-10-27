/**
 * @file        core/sleep_duration_test.cpp
 * @brief       Tests that rex::thread::Sleep and AlertableSleep wait at least as long as asked
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>

#include <rex/thread.h>

// Only lower bounds: how much longer a sleep takes depends on the machine's
// load. Before the high-resolution timer, Windows cut a 1.5 ms sleep to 1 ms.
// A timer may fire a little before the steady clock agrees, hence the slack.
constexpr auto kSlack = std::chrono::microseconds(200);
TEST_CASE("Sleep waits at least the time asked, below a millisecond too", "[thread]") {
  using clock = std::chrono::steady_clock;
  for (auto duration : {std::chrono::microseconds(1500), std::chrono::microseconds(2500),
                        std::chrono::microseconds(10000)}) {
    const auto start = clock::now();
    rex::thread::Sleep(duration);
    CHECK(clock::now() - start >= duration - kSlack);
  }
}

TEST_CASE("AlertableSleep without an APC sleeps the time asked", "[thread]") {
  using clock = std::chrono::steady_clock;
  const auto duration = std::chrono::microseconds(2500);
  const auto start = clock::now();
  CHECK(rex::thread::AlertableSleep(duration) == rex::thread::SleepResult::kSuccess);
  CHECK(clock::now() - start >= duration - kSlack);
}
