#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <initializer_list>
#include <rex/cvar.h>
#include "shader_load_gate.h"

REXCVAR_DECLARE(bool, pdz_pause_for_shaders);

namespace pdz_shaders {
struct Progress {
  uint64_t started = 0, finished = 0;
  uint64_t pending() const { return started > finished ? started - finished : 0; }
};
inline Progress Read() {
  Progress p;
  using Query = void (*)(uint64_t*, uint64_t*);
  const auto module = GetModuleHandleW(L"rexgpu-xenos.dll");
  if (module) for (const char* symbol : {"PDZVulkanShaderProgress", "PDZD3D12ShaderProgress"}) {
    const auto query = reinterpret_cast<Query>(GetProcAddress(module, symbol));
    Progress backend;
    if (query) query(&backend.started, &backend.finished);
    p.started += backend.started; p.finished += backend.finished;
  }
  return p;
}
inline std::atomic<bool> waiting{false}, cancelled{false};
inline std::atomic<uint64_t> batch_start{0};
inline std::atomic<bool> loading_screen{false};
inline LoadGate load_gate;
inline std::mutex gate_mutex;
inline void SetMap(int mode, int id, const char* name) {
  using Set = void (*)(int, int, const char*);
  const auto module = GetModuleHandleW(L"rexgpu-xenos.dll");
  const auto set = module ? reinterpret_cast<Set>(GetProcAddress(module, "PDZSetShaderMap")) : nullptr;
  if (set) set(mode, id, name);
  std::lock_guard lock(gate_mutex);
  load_gate.Reset();
  loading_screen = id >= 0;
  waiting = false;
  cancelled = false;
}
inline bool HoldLoadingScreen() {
  std::lock_guard lock(gate_mutex);
  if (!REXCVAR_GET(pdz_pause_for_shaders) || cancelled.load()) {
    waiting = false; loading_screen = false; return false;
  }
  const auto p = Read();
  if (!load_gate.active) batch_start = p.finished;
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  const bool hold = load_gate.Hold(p.started, p.finished, uint64_t(now));
  waiting = hold && p.pending() != 0;
  if (!hold) loading_screen = false;
  return hold;
}

// Called on the guest presentation path, never on the UI or GPU thread.
// Workers and the independent host presenter remain free to finish and repaint.
inline void Wait() {
  if (loading_screen.load()) return; // The native loading gate owns this wait.
  const auto first = Read();
  if (!first.pending()) { cancelled = false; return; }
  if (!REXCVAR_GET(pdz_pause_for_shaders) || cancelled.load()) return;
  batch_start = first.finished;
  waiting = true;
  while (Read().pending() && !cancelled.load() && REXCVAR_GET(pdz_pause_for_shaders))
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  waiting = false;
}
}
