/**
 * @file        audio/sdl/fallback_audio_driver.cpp
 * @brief       SDL audio driver that always starts: fallback formats, then a silent output
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

// FallbackAudioDriver: the default output in fallback formats, or a silent output paced like a
// device while it is asked for again (fallback_audio_driver.h).

#include "fallback_audio_driver.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <optional>

#include <rex/logging.h>
#include <rex/thread.h>
#include <SDL3/SDL.h>

namespace rex::audio::sdl {

static_assert(kOutputFormatS16 == SDL_AUDIO_S16LE);
static_assert(kOutputFormatF32 == SDL_AUDIO_F32LE);

namespace {

// The silent output takes its frames as SDL's devices do at 48 kHz: 1024 sample frames (four of
// the game's frames) every 21.3 ms. A gap longer than kSilentMaxGap restarts its clock instead of
// being caught up.
constexpr int kSilentPeriodFrames = 1024;
constexpr std::chrono::milliseconds kSilentMaxGap{200};

// REX_AUDIO_FAIL_OPEN (audio_output_fallback.h): a refused open, for tests.
bool TakeTestFailure() {
  static std::atomic<uint64_t> remaining{ParseFailOpenCount(std::getenv("REX_AUDIO_FAIL_OPEN"))};
  uint64_t count = remaining.load();
  while (count) {
    if (count == UINT64_MAX || remaining.compare_exchange_weak(count, count - 1)) {
      return true;
    }
  }
  return false;
}

// The default output's own format as SDL reports it (the device's mix format on WASAPI).
std::optional<OutputRequest> DefaultDeviceFormat() {
  SDL_AudioSpec spec = {};
  if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr)) {
    return std::nullopt;
  }
  return OutputRequest{spec.freq, static_cast<uint16_t>(spec.format), spec.channels};
}

}  // namespace

FallbackAudioDriver::FallbackAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore)
    : SDLAudioDriver(memory, semaphore) {}

FallbackAudioDriver::~FallbackAudioDriver() {
  Stop();
}

void FallbackAudioDriver::Start() {
  // Set audio category for proper OS audio handling
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");

  // Set app name for audio device identification
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "rexglue");

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    // No audio driver of SDL's works here; asking again would not change that.
    StartSilentOutput(std::string("SDL found no audio driver: ") + SDL_GetError(), false);
    return;
  }
  sdl_initialized_ = true;

  std::string first_error;
  std::optional<OutputRequest> device;
  bool device_logged = false;
  const auto make_order = [&] {
    device = DefaultDeviceFormat();
    return OutputOpenOrder(device);
  };
  const auto try_open = [&](const OutputAttempt& attempt, int round) {
    std::string error;
    if (OpenDeviceWith(attempt, round > 0, &error)) {
      if (!first_error.empty()) {
        REXAPU_WARN("audio output opened with {} ({} Hz, {} ch, format 0x{:04X})", attempt.what,
                    attempt.request.freq, attempt.request.channels, attempt.request.format);
      }
      return true;
    }
    if (first_error.empty()) {
      first_error = error;
    }
    if (!device_logged) {
      device_logged = true;
      if (device) {
        REXAPU_WARN("default audio output reports {} Hz, {} ch, format 0x{:04X}", device->freq,
                    device->channels, device->format);
      } else {
        REXAPU_WARN("default audio output reports no format: {}", SDL_GetError());
      }
    }
    return false;
  };
  const auto wait = [](std::chrono::milliseconds delay) { std::this_thread::sleep_for(delay); };
  if (OpenOutputInOrder(make_order, try_open, wait)) {
    return;
  }
  StartSilentOutput(first_error, true);
}

bool FallbackAudioDriver::OpenDevice(bool quiet, std::string* first_error) {
  for (const OutputAttempt& attempt : OutputOpenOrder(DefaultDeviceFormat())) {
    std::string error;
    if (OpenDeviceWith(attempt, quiet, &error)) {
      return true;
    }
    if (first_error->empty()) {
      *first_error = error;
    }
  }
  return false;
}

bool FallbackAudioDriver::OpenDeviceWith(const OutputAttempt& attempt, bool quiet,
                                         std::string* error) {
  const OutputRequest& request = attempt.request;
  const auto refused = [&](std::string reason) {
    if (quiet) {
      REXAPU_DEBUG("audio output: {} ({} Hz, {} ch, format 0x{:04X}) refused: {}", attempt.what,
                   request.freq, request.channels, request.format, reason);
    } else {
      REXAPU_WARN("audio output: {} ({} Hz, {} ch, format 0x{:04X}) refused: {}", attempt.what,
                  request.freq, request.channels, request.format, reason);
    }
    *error = std::move(reason);
    return false;
  };
  if (TakeTestFailure()) {
    return refused("refused for a test (REX_AUDIO_FAIL_OPEN)");
  }

  SDL_AudioSpec spec = {};
  spec.freq = request.freq;
  spec.format = static_cast<SDL_AudioFormat>(request.format);
  spec.channels = request.channels;
  SDL_AudioStream* stream =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, SDLCallback, this);
  if (!stream) {
    return refused(std::string("SDL_OpenAudioDeviceStream() failed: ") + SDL_GetError());
  }
  SDL_AudioDeviceID sdl_device = SDL_GetAudioStreamDevice(stream);
  if (!sdl_device) {
    std::string reason = std::string("SDL_GetAudioStreamDevice() failed: ") + SDL_GetError();
    SDL_DestroyAudioStream(stream);
    return refused(std::move(reason));
  }
  SDL_AudioSpec obtained_spec = spec;
  if (!SDL_GetAudioDeviceFormat(sdl_device, &obtained_spec, nullptr)) {
    REXAPU_WARN("SDL_GetAudioDeviceFormat() failed: {}", SDL_GetError());
    obtained_spec = spec;
  }

  // The game's frames go to the stream as 48 kHz float whatever the device was opened with: 5.1
  // for a device with more than two channels, else the stereo fold (a 1-channel device gets the
  // stereo fold too, then SDL collapses it to mono; a 5.1 stream would use SDL's own downmix).
  // The device is paused until it is resumed below, so the callback has not run yet.
  const uint8_t channels = obtained_spec.channels <= 2 ? 2 : 6;
  SDL_AudioSpec source_spec = {};
  source_spec.freq = static_cast<int>(frame_frequency_);
  source_spec.format = SDL_AUDIO_F32LE;
  source_spec.channels = channels;
  if (!SDL_SetAudioStreamFormat(stream, &source_spec, nullptr)) {
    std::string reason = std::string("SDL_SetAudioStreamFormat() failed: ") + SDL_GetError();
    SDL_DestroyAudioStream(stream);
    return refused(std::move(reason));
  }
  // Set before the device runs the callback; put back if it does not start, as the silent
  // output (when this is a reopen attempt) goes on pulling its stereo stream.
  const uint8_t previous_channels = sdl_device_channels_;
  sdl_device_channels_ = channels;

  // The endpoint layout decides which mix the callback runs, and it is the
  // first thing worth knowing when a report says the balance is wrong on one
  // speaker setup and right on another.
  const char* device_name = SDL_GetAudioDeviceName(sdl_device);
  REXAPU_INFO("audio endpoint '{}': {} ch, {} Hz, format 0x{:04X}; submitting {} ch",
              device_name ? device_name : "?", obtained_spec.channels, obtained_spec.freq,
              static_cast<uint32_t>(obtained_spec.format), static_cast<int>(channels));

  if (!SDL_ResumeAudioDevice(sdl_device)) {
    std::string reason = std::string("SDL_ResumeAudioDevice() failed: ") + SDL_GetError();
    SDL_DestroyAudioStream(stream);
    sdl_device_channels_ = previous_channels;
    return refused(std::move(reason));
  }
  sdl_stream_ = stream;
  return true;
}

void FallbackAudioDriver::StartSilentOutput(const std::string& reason, bool reopen) {
  REXAPU_WARN("No audio output could be opened ({}); the game runs without sound.{}", reason,
              reopen ? " Asking for it again every 5 s." : "");
  // The stream a device would pull, unbound: the silent output pulls it instead, and the
  // callback queues and releases the game's frames as it does for a device.
  SDL_AudioSpec spec = {};
  spec.freq = static_cast<int>(frame_frequency_);
  spec.format = SDL_AUDIO_F32LE;
  spec.channels = 2;
  sdl_device_channels_ = 2;
  silent_stream_ = SDL_CreateAudioStream(&spec, &spec);
  if (!silent_stream_ || !SDL_SetAudioStreamGetCallback(silent_stream_, SDLCallback, this)) {
    REXAPU_ERROR("the silent audio output could not start: {}", SDL_GetError());
    if (silent_stream_) {
      SDL_DestroyAudioStream(silent_stream_);
      silent_stream_ = nullptr;
    }
    return;
  }
  silent_thread_ = std::thread(&FallbackAudioDriver::SilentOutputMain, this, reopen);
}

void FallbackAudioDriver::SilentOutputMain(bool reopen) {
  rex::thread::set_current_thread_name("Silent Audio");
  using Clock = SilentClock::Clock;
  SilentClock pace(frame_frequency_, kSilentPeriodFrames, kSilentMaxGap);
  const Clock::time_point since = Clock::now();
  pace.Start(since);
  Clock::time_point next_reopen = since + kOutputReopenInterval;
  std::array<float, 2 * kSilentPeriodFrames> discard;

  std::unique_lock<std::mutex> lock(silent_mutex_);
  while (!silent_stop_) {
    Clock::time_point wake = pace.NextDue();
    if (reopen && next_reopen < wake) {
      wake = next_reopen;
    }
    if (silent_wake_.wait_until(lock, wake, [this] { return silent_stop_; })) {
      break;
    }
    lock.unlock();

    Clock::time_point now = Clock::now();
    for (uint64_t frames = pace.Take(now); frames > 0;) {
      const int chunk = static_cast<int>(std::min<uint64_t>(frames, kSilentPeriodFrames));
      SDL_GetAudioStreamData(silent_stream_, discard.data(),
                             chunk * 2 * static_cast<int>(sizeof(float)));
      frames -= chunk;
    }

    if (reopen && now >= next_reopen) {
      std::string error;
      if (OpenDevice(true, &error)) {
        // The device's callback takes the queued frames from here on.
        REXAPU_WARN("audio output opened after {} s without sound",
                    std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - since).count());
        return;
      }
      next_reopen = Clock::now() + kOutputReopenInterval;
    }
    lock.lock();
  }
}

void FallbackAudioDriver::Stop() {
  {
    std::lock_guard<std::mutex> lock(silent_mutex_);
    silent_stop_ = true;
  }
  silent_wake_.notify_all();
  if (silent_thread_.joinable()) {
    silent_thread_.join();
  }
  if (silent_stream_) {
    SDL_DestroyAudioStream(silent_stream_);
    silent_stream_ = nullptr;
  }
  // The device stream, SDL's audio subsystem and the frame buffers.
  SDLAudioDriver::Shutdown();
}

}  // namespace rex::audio::sdl
