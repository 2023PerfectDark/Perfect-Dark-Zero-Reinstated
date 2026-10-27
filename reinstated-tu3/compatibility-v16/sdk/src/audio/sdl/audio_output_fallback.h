#pragma once
/**
 * @file        audio/sdl/audio_output_fallback.h
 * @brief       Fallback order of the audio output and pacing of the silent output
 *
 * @copyright   Copyright (c) 2026 ReXGlue fork contributors
 *              All rights reserved.
 *              Ported for the ReXGlue fork from ForzaHorizon1-ReImagined SDK
 *              patch 0103 (MIT, Copyright (c) 2026 Dyotson and Forza Horizon 1:
 *              ReImagined contributors).
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

// Private runtime logic of FallbackAudioDriver; never included by SDK clients. It has no SDL in
// it, so the order and the pacing can be tested without an audio device
// (tests/unit/audio/audio_output_fallback_test.cpp).

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>
#include <vector>

namespace rex::audio::sdl {

// SDL_AUDIO_S16LE and SDL_AUDIO_F32LE (checked against SDL where the driver uses them).
inline constexpr uint16_t kOutputFormatS16 = 0x8010;
inline constexpr uint16_t kOutputFormatF32 = 0x8120;

// Start-up asks for the default output twice, kOutputRetryDelay apart (a device another program
// holds may be free again); while the silent output runs it asks again every kOutputReopenInterval.
inline constexpr int kOutputOpenRounds = 2;
inline constexpr std::chrono::milliseconds kOutputRetryDelay{500};
inline constexpr std::chrono::seconds kOutputReopenInterval{5};

// What one attempt asks SDL to open the device with. The game's frames are always handed over as
// 48 kHz float, 5.1 or stereo; SDL converts them to whatever the device was opened with.
struct OutputRequest {
  int freq = 0;
  uint16_t format = 0;
  int channels = 0;
  bool operator==(const OutputRequest&) const = default;
};

struct OutputAttempt {
  OutputRequest request;
  const char* what;  // for the log
};

inline bool IsUsableDeviceFormat(const OutputRequest& format) {
  return format.freq >= 8000 && format.freq <= 768000 && format.channels >= 1 &&
         format.channels <= 8 && format.format != 0;
}

// The order the default output is asked for. `device` is the format the device reports as its
// own (SDL_GetAudioDeviceFormat), when it reports one. Asked for its own rate, WASAPI initializes
// the shared stream with the device's mix format as it is; asked for another rate, SDL adds
// automatic conversion to the request, which some devices refuse.
inline std::vector<OutputAttempt> OutputOpenOrder(const std::optional<OutputRequest>& device) {
  std::vector<OutputAttempt> order;
  const auto add = [&order](const OutputRequest& request, const char* what) {
    for (const OutputAttempt& attempt : order) {
      if (attempt.request == request) {
        return;
      }
    }
    order.push_back({request, what});
  };
  add({48000, kOutputFormatF32, 6}, "48 kHz float 5.1 (the game's mix)");
  if (device && IsUsableDeviceFormat(*device)) {
    add(*device, "the device's own format");
  }
  add({48000, kOutputFormatF32, 2}, "48 kHz float stereo");
  add({48000, kOutputFormatS16, 2}, "48 kHz 16-bit stereo");
  return order;
}

// The start-up sequence: every attempt of the order, then, after wait(kOutputRetryDelay), all of
// them again (the order is rebuilt: the default device may have changed). try_open(attempt,
// round) returns true when the device opened. Returns whether one did.
template <typename MakeOrder, typename TryOpen, typename Wait>
bool OpenOutputInOrder(MakeOrder&& make_order, TryOpen&& try_open, Wait&& wait) {
  for (int round = 0; round < kOutputOpenRounds; ++round) {
    if (round) {
      wait(kOutputRetryDelay);
    }
    for (const OutputAttempt& attempt : make_order()) {
      if (try_open(attempt, round)) {
        return true;
      }
    }
  }
  return false;
}

// REX_AUDIO_FAIL_OPEN: a number N makes the first N attempts to open the output fail as a refused
// device does, "all" makes every attempt fail. Unset, empty, 0 or anything else: none.
inline uint64_t ParseFailOpenCount(const char* value) {
  if (!value || !*value) {
    return 0;
  }
  if (std::string_view(value) == "all") {
    return UINT64_MAX;
  }
  char* end = nullptr;
  const unsigned long long count = std::strtoull(value, &end, 10);
  return (*end == '\0' && value[0] != '-') ? count : 0;
}

// Paces the silent output like a device that plays `rate` sample frames a second. Take() hands
// out the frames that came due since the last call, counted from a fixed start, so rounding never
// adds up. After a gap longer than `max_gap` (the process was suspended, a reopen attempt took
// long) it starts over instead of asking for the whole gap at once, as a device that ran dry
// plays on from where it is rather than catching up.
class SilentClock {
 public:
  using Clock = std::chrono::steady_clock;

  SilentClock(uint32_t rate, uint32_t period, std::chrono::nanoseconds max_gap)
      : rate_(rate), period_(period), max_gap_frames_(FramesIn(max_gap)) {}

  void Start(Clock::time_point now) {
    start_ = now;
    taken_ = 0;
  }

  uint64_t Take(Clock::time_point now) {
    if (now <= start_) {
      return 0;
    }
    const uint64_t due = FramesIn(now - start_);
    if (due - taken_ > max_gap_frames_) {
      Start(now);
      return 0;
    }
    const uint64_t frames = due - taken_;
    taken_ = due;
    // Move the start on by whole seconds (exactly `rate_` frames each) to keep the numbers small.
    if (taken_ >= rate_) {
      const uint64_t seconds = taken_ / rate_;
      start_ += std::chrono::seconds(seconds);
      taken_ -= seconds * rate_;
    }
    return frames;
  }

  // When the next `period` frames are due: the time to wake up next.
  Clock::time_point NextDue() const {
    const uint64_t frames = taken_ + period_;
    const uint64_t ns =
        (frames / rate_) * 1000000000ull + ((frames % rate_) * 1000000000ull + rate_ - 1) / rate_;
    return start_ + std::chrono::ceil<Clock::duration>(std::chrono::nanoseconds(ns));
  }

 private:
  uint64_t FramesIn(std::chrono::nanoseconds span) const {
    const uint64_t ns = span.count() > 0 ? uint64_t(span.count()) : 0;
    return (ns / 1000000000ull) * rate_ + (ns % 1000000000ull) * rate_ / 1000000000ull;
  }

  uint32_t rate_;
  uint32_t period_;
  uint64_t max_gap_frames_;
  Clock::time_point start_{};
  uint64_t taken_ = 0;
};

}  // namespace rex::audio::sdl
