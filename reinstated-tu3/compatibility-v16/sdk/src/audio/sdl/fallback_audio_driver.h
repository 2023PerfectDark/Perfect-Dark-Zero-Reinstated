#pragma once
/**
 * @file        audio/sdl/fallback_audio_driver.h
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

// Private to the runtime; never included by SDK clients (sdl_audio_driver.h stays as it is).

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <rex/audio/sdl/sdl_audio_driver.h>

#include "audio_output_fallback.h"

namespace rex::audio::sdl {

// An SDLAudioDriver that always starts, so the game never stops for want of a sound device.
// Before it, a default output SDL could not open (#244: WASAPI refused to initialize the audio
// client) failed XAudioRegisterRenderDriverClient, and the game's audio start-up ended in
// KeBugCheckEx.
//
// Start() asks for the default output in OutputOpenOrder()'s formats, twice. When none opens, a
// silent output takes the device's place: a thread pulls the driver's audio stream at 48 kHz, so
// the same callback takes the game's frames off the queue and releases the client semaphore at
// the rate a device would, and nothing is played. Every kOutputReopenInterval it asks for the
// device again; when one opens, the device takes over the stream of frames.
class FallbackAudioDriver : public SDLAudioDriver {
 public:
  FallbackAudioDriver(memory::Memory* memory, rex::thread::Semaphore* semaphore);
  ~FallbackAudioDriver() override;

  // In place of SDLAudioDriver::Initialize and Shutdown.
  void Start();
  void Stop();

 private:
  // One pass through OutputOpenOrder(); on success the device stream is open and playing.
  bool OpenDevice(bool quiet, std::string* first_error);
  bool OpenDeviceWith(const OutputAttempt& attempt, bool quiet, std::string* error);
  void StartSilentOutput(const std::string& reason, bool reopen);
  void SilentOutputMain(bool reopen);

  SDL_AudioStream* silent_stream_ = nullptr;
  std::thread silent_thread_;
  std::mutex silent_mutex_;
  std::condition_variable silent_wake_;
  bool silent_stop_ = false;
};

}  // namespace rex::audio::sdl
