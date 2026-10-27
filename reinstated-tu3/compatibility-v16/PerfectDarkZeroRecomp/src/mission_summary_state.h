#pragma once
#include "mission_summary_policy.h"
#include <atomic>
#include <mutex>
namespace pdz_results {
inline std::mutex mutex;
inline Summary summary;
inline std::atomic<bool> pending{false};
inline std::atomic<bool> dismiss{false};
inline Summary Snapshot() { std::lock_guard lock(mutex); return summary; }
}
