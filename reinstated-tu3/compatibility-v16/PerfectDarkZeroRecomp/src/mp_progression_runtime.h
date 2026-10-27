#pragma once
#include "mp_career_store.h"
#include <mutex>
#include <thread>
#include <chrono>
#include <deque>
#include <random>

namespace pdz_progression {
struct Notice {std::string name;uint32_t xp;};
class Runtime {
 public:
  ~Runtime(){Shutdown();}
  void Configure(const std::filesystem::path& root) {
    Shutdown();std::lock_guard lock(mutex_);
    path_=root/"mod-progress"/"mp-career-v1.bin";
    profiles_={};dirty_=false;error_.clear();notices_.clear();
    std::error_code ec;const bool exists=std::filesystem::exists(path_,ec);
    writable_=!ec&&(!exists||LoadCareers(path_,profiles_));
    if(!writable_)error_="Career save could not be read. It has been preserved; progression saving is disabled.";
    worker_=std::jthread([this](std::stop_token stop){while(!stop.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));Flush();
    }});
  }
  void Shutdown(){if(worker_.joinable()){worker_.request_stop();worker_.join();}Flush();}
  Career Snapshot(unsigned slot=0){std::lock_guard lock(mutex_);return profiles_[slot<32?slot:0];}
  std::string Error(){std::lock_guard lock(mutex_);return error_;}
  bool NextNotice(Notice& notice){std::lock_guard lock(mutex_);if(notices_.empty())return false;
    notice=notices_.front();notices_.pop_front();return true;}
  void Record(unsigned slot,Event event) {
    std::lock_guard lock(mutex_);if(slot>=32||!writable_)return;
    auto& c=profiles_[slot];if(c.last_event==UINT64_MAX)return;
    event.sequence=c.last_event+1;
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for(auto id:Apply(c,event,now))if(slot==0)notices_.push_back({Achievements()[id].name,Achievements()[id].xp});
    dirty_=true;
  }
  bool EnterPrestige(unsigned slot=0){std::lock_guard lock(mutex_);if(slot>=32||!writable_)return false;
    if(!Prestige(profiles_[slot].rank))return false;dirty_=true;return true;}
  bool ResetProfile(bool achievements,unsigned slot=0){std::lock_guard lock(mutex_);if(slot>=32||!writable_)return false;
    if(!Reset(profiles_[slot],achievements))return false;dirty_=true;return true;}
 private:
  void Flush() {
    // Serialize on the worker; never perform filesystem writes on the game's hit path.
    std::lock_guard io(io_mutex_);Careers snapshot;std::filesystem::path path;
    {std::lock_guard lock(mutex_);if(!dirty_||!writable_||path_.empty())return;snapshot=profiles_;path=path_;dirty_=false;}
    const bool ok=SaveCareers(path,snapshot);
    std::lock_guard lock(mutex_);
    if(!ok){dirty_=true;error_="Could not save MP career. Retrying; keep the game open until this clears.";}
    else error_.clear();
  }
  std::mutex mutex_,io_mutex_;
  Careers profiles_{};
  std::filesystem::path path_;
  std::deque<Notice> notices_;
  std::string error_;
  bool dirty_=false,writable_=false;
  std::jthread worker_;
};
inline Runtime career;
}
