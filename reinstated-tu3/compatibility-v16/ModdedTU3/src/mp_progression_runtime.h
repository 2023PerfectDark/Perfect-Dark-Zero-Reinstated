#pragma once
#include "mp_career_store.h"
#include <mutex>
#include <thread>
#include <chrono>
#include <deque>
#include <random>

namespace pdz_progression {
struct Notice {std::string name;uint32_t xp;};
struct MatchXp {Rank before,after;};
class Runtime {
 public:
  ~Runtime(){Shutdown();}
  void Configure(const std::filesystem::path& root) {
    Shutdown();std::lock_guard lock(mutex_);
    path_=root/"mod-progress"/"mp-career-v1.bin";
    profiles_={};dirty_=false;error_.clear();notices_.clear();levels_.clear();matches_.clear();match_active_=false;next_match_=true;
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
  void BeginMatch(){std::lock_guard lock(mutex_);if(!next_match_)return;
    match_start_=profiles_[0].rank;match_active_=true;next_match_=false;}
  void UnloadMatch(){std::lock_guard lock(mutex_);next_match_=true;}
  void FinishMatch(){std::lock_guard lock(mutex_);if(!match_active_)return;
    matches_.push_back({match_start_,profiles_[0].rank});match_active_=false;}
  bool NextMatch(MatchXp& m){std::lock_guard lock(mutex_);if(matches_.empty())return false;
    m=matches_.front();matches_.pop_front();return true;}
  bool NextLevel(Rank& r){std::lock_guard lock(mutex_);if(levels_.empty())return false;
    r=levels_.front();levels_.pop_front();return true;}
  void Record(unsigned slot,Event event) {
    std::lock_guard lock(mutex_);if(slot>=32||!writable_)return;
    auto& c=profiles_[slot];if(c.last_event==UINT64_MAX)return;
    const auto before=c.rank;
    event.sequence=c.last_event+1;
    const auto now=uint64_t(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for(auto id:Apply(c,event,now))if(slot==0)notices_.push_back({Achievements()[id].name,Achievements()[id].xp});
    if(slot==0)for(auto level=before.level+1;level<=c.rank.level;++level){auto r=c.rank;r.level=level;levels_.push_back(r);}
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
  std::deque<Rank> levels_;
  std::deque<MatchXp> matches_;
  Rank match_start_{};
  bool match_active_=false,next_match_=true;
  std::string error_;
  bool dirty_=false,writable_=false;
  std::jthread worker_;
};
inline Runtime career;
}
