#pragma once
#include "mission_summary_policy.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace pdz_cheats {
// Separate, per-profile progress. Updating the mod never ships this file.
class Progress {
 public:
  void Configure(const std::filesystem::path& root) {
    std::lock_guard lock(mutex_);
    file_=root/"mod-progress"/"alerted-guards.unlock";
    unlocked_=false; error_.clear();
    std::ifstream input(file_,std::ios::binary);
    std::string line;
    if(input && std::getline(input,line)) {
      unlocked_=line=="PDZ_ALERTED_GUARDS_V1";
      if(!unlocked_)error_="Unrecognized cheat progress; existing file preserved.";
    }
  }
  bool Unlocked() const {std::lock_guard lock(mutex_);return unlocked_;}
  std::string Error() const {std::lock_guard lock(mutex_);return error_;}
  bool Complete(const pdz_results::Summary& summary) {
    std::lock_guard lock(mutex_);
    if(unlocked_ || !error_.empty() || file_.empty() ||
       !pdz_results::EarnedAlertedGuards(summary))return false;
    try {
      std::filesystem::create_directories(file_.parent_path());
      // Never truncate an existing or unrecognized progress file.
      if(std::filesystem::exists(file_)) {
        error_="Cheat progress changed on disk; existing file preserved.";return false;
      }
      const auto temporary=std::filesystem::path(file_.wstring()+L".tmp");
      {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
       out<<"PDZ_ALERTED_GUARDS_V1\n";out.flush();
       if(!out)throw std::runtime_error("Could not save cheat unlock.");}
      std::filesystem::rename(temporary,file_);
      unlocked_=true;return true;
    } catch(const std::exception& e) {error_=e.what();return false;}
  }
 private:
  mutable std::mutex mutex_;
  std::filesystem::path file_;
  bool unlocked_=false;
  std::string error_;
};
inline Progress progress;
}
