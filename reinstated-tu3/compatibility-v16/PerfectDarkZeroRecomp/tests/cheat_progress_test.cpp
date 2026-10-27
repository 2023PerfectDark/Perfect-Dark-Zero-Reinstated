#include "../src/cheat_progress.h"
#include <cassert>
#include <chrono>
int main() {
  const auto root=std::filesystem::temp_directory_path()/
    ("pdz-progress-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  pdz_results::Summary s{};
  s.completed=true;s.mission=1;s.campaign_style=1;s.target_seconds=240;s.time_cs=23999;
  pdz_cheats::Progress a;a.Configure(root/"a");
  assert(!a.Unlocked());assert(a.Complete(s));assert(a.Unlocked());assert(!a.Complete(s));
  pdz_cheats::Progress reload;reload.Configure(root/"a");assert(reload.Unlocked());
  pdz_cheats::Progress other;other.Configure(root/"b");assert(!other.Unlocked());
  s.completed=false;assert(!other.Complete(s));
  const auto protected_file=root/"b"/"mod-progress"/"alerted-guards.unlock";
  std::filesystem::create_directories(protected_file.parent_path());
  {std::ofstream out(protected_file);out<<"future-version";}
  other.Configure(root/"b");s.completed=true;assert(!other.Complete(s));
  std::ifstream input(protected_file);std::string text;input>>text;assert(text=="future-version");
  input.close();std::filesystem::remove_all(root);
}
