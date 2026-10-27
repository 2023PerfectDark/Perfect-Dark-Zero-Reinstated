/**
 * @file        audio/audio_output_fallback_test.cpp
 * @brief       Tests for the audio output's fallback order and the silent
 *              output's pacing (no audio device needed)
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ForzaHorizon1-ReImagined
 *              (forza-recomp/tests/overlay-sdk/audio_output_fallback_test.cpp,
 *              MIT).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <chrono>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <audio/sdl/audio_output_fallback.h>

using namespace rex::audio::sdl;
using namespace std::chrono_literals;

namespace {

const OutputRequest kGameMix{48000, kOutputFormatF32, 6};
const OutputRequest kStereoF32{48000, kOutputFormatF32, 2};
const OutputRequest kStereoS16{48000, kOutputFormatS16, 2};

// OpenOutputInOrder with a device that refuses the first `refusals` attempts.
struct Run {
  std::vector<OutputRequest> asked;
  std::vector<int> rounds;
  std::vector<std::chrono::milliseconds> waits;
  int orders = 0;
  bool opened = false;
};

Run OpenWithRefusals(int refusals) {
  Run run;
  const OutputRequest device{44100, kOutputFormatS16, 2};
  run.opened = OpenOutputInOrder(
      [&] {
        ++run.orders;
        return OutputOpenOrder(device);
      },
      [&](const OutputAttempt& attempt, int round) {
        run.asked.push_back(attempt.request);
        run.rounds.push_back(round);
        return int(run.asked.size()) > refusals;
      },
      [&](std::chrono::milliseconds delay) { run.waits.push_back(delay); });
  return run;
}

}  // namespace

TEST_CASE("The audio output is asked for in fallback order", "[audio][fallback]") {
  // Without a device format: the game's mix, then plain stereo, float and 16-bit.
  auto order = OutputOpenOrder(std::nullopt);
  REQUIRE(order.size() == 3);
  CHECK(order[0].request == kGameMix);
  CHECK(order[1].request == kStereoF32);
  CHECK(order[2].request == kStereoS16);

  // A 192 kHz, 32-bit, 7.1 device: its own format right after the game's mix.
  const OutputRequest device{192000, 0x8020, 8};
  order = OutputOpenOrder(device);
  REQUIRE(order.size() == 4);
  CHECK(order[0].request == kGameMix);
  CHECK(order[1].request == device);
  CHECK((order[2].request == kStereoF32 && order[3].request == kStereoS16));

  // A device format equal to another attempt is asked for once.
  order = OutputOpenOrder(kStereoF32);
  REQUIRE(order.size() == 3);
  CHECK((order[1].request == kStereoF32 && order[2].request == kStereoS16));

  // A format that makes no sense is left out.
  CHECK(OutputOpenOrder(OutputRequest{0, kOutputFormatF32, 2}).size() == 3);
  CHECK(OutputOpenOrder(OutputRequest{48000, kOutputFormatF32, 0}).size() == 3);
  CHECK(OutputOpenOrder(OutputRequest{48000, 0, 2}).size() == 3);
}

TEST_CASE("Start-up tries every format twice before the silent output", "[audio][fallback]") {
  Run run = OpenWithRefusals(0);
  CHECK((run.opened && run.asked.size() == 1 && run.asked[0] == kGameMix));
  CHECK((run.waits.empty() && run.orders == 1));

  run = OpenWithRefusals(1);
  CHECK((run.opened && run.asked.size() == 2));
  CHECK(run.asked[1] == OutputRequest{44100, kOutputFormatS16, 2});

  // Every format refused once: the whole order again after the delay, the order rebuilt.
  run = OpenWithRefusals(4);
  CHECK((run.opened && run.asked.size() == 5));
  CHECK((run.waits.size() == 1 && run.waits[0] == kOutputRetryDelay));
  CHECK(run.orders == 2);
  CHECK((run.rounds[3] == 0 && run.rounds[4] == 1 && run.asked[4] == kGameMix));

  // Nothing opens: two rounds of four, then the caller runs the silent output.
  run = OpenWithRefusals(1000);
  CHECK((!run.opened && run.asked.size() == 8));
  CHECK(run.waits.size() == 1);
}

TEST_CASE("REX_AUDIO_FAIL_OPEN counts are parsed strictly", "[audio][fallback]") {
  CHECK(ParseFailOpenCount(nullptr) == 0);
  CHECK(ParseFailOpenCount("") == 0);
  CHECK(ParseFailOpenCount("0") == 0);
  CHECK(ParseFailOpenCount("3") == 3);
  CHECK(ParseFailOpenCount("all") == UINT64_MAX);
  CHECK((ParseFailOpenCount("x") == 0 && ParseFailOpenCount("2x") == 0));
  CHECK(ParseFailOpenCount("-1") == 0);
}

TEST_CASE("The silent output is paced like a 48 kHz device", "[audio][fallback]") {
  using Clock = SilentClock::Clock;
  const Clock::time_point t0{};
  const auto at = [&](std::chrono::nanoseconds d) {
    return t0 + std::chrono::ceil<Clock::duration>(d);
  };

  SilentClock pace(48000, 256, 200ms);
  pace.Start(t0);
  CHECK(pace.Take(t0) == 0);
  // One frame of the game (256 samples) lasts 5.333 ms.
  CHECK(pace.NextDue() == at(5333334ns));
  CHECK(pace.Take(at(5333334ns)) == 256);
  CHECK(pace.NextDue() == at(10666667ns));

  // Uneven wake-ups (a coarse timer) still add up to exactly 48000 a second, over ten minutes.
  pace.Start(t0);
  uint64_t total = 0;
  std::chrono::nanoseconds t{0};
  uint32_t step = 1;
  while (t < 600s) {
    step = step * 1103515245u + 12345u;
    t += std::chrono::microseconds(1000 + (step >> 8) % 30000);  // 1 to 31 ms late
    total += pace.Take(at(t));
  }
  const uint64_t expected =
      uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t).count()) * 48000 /
      1000000000;
  CHECK(total == expected);

  // A gap past 200 ms (suspended, a slow reopen): start over, do not ask for the backlog.
  pace.Start(t0);
  CHECK(pace.Take(at(100ms)) == 4800);
  CHECK(pace.Take(at(1100ms)) == 0);
  CHECK(pace.Take(at(1110ms)) == 480);
}
