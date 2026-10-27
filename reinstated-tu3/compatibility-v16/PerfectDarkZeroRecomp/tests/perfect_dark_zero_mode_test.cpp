#include "../src/perfect_dark_zero_mode.h"
#include <cassert>
#include <chrono>
int main() {
  using namespace pdz_custom;
  std::array<std::array<unsigned char,8>,14> records{};
  assert(!DarkCampaignComplete(records));
  for(auto& mission:records)mission[2]=1;
  assert(!DarkCampaignComplete(records)); // Perfect Agent is insufficient.
  for(unsigned i=0;i<14;++i)records[i][i%2?7:3]=1;
  assert(DarkCampaignComplete(records)); // Solo and co-op Dark Agent count.
  for(unsigned missing=0;missing<14;++missing){
    auto partial=records;partial[missing][3]=partial[missing][7]=0;
    assert(!DarkCampaignComplete(partial));
  }
  const auto root=std::filesystem::temp_directory_path()/
    ("pdz-mode-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Configure(root);assert(!Unlocked());
  std::filesystem::create_directories(root/"mod-progress");
  const auto file=root/"mod-progress"/"personal-pdz-mode.unlock";
  {std::ofstream out(file);out<<"PDZ_PERSONAL_CUSTOM_MODE_V1\n";}
  Configure(root);assert(Unlocked());
  Configure(root/"other-user");assert(!Unlocked());
  campaign_unlocked=true;assert(Unlocked());
  Configure(root/"other-user");assert(!Unlocked()); // No cross-profile leakage.
  {std::ofstream out(file);out<<"unknown-future-format\n";}
  Configure(root);assert(!Unlocked());
  {std::ifstream input(file);std::string line;std::getline(input,line);assert(line=="unknown-future-format");}
  std::filesystem::remove(file);std::filesystem::remove(root/"mod-progress");std::filesystem::remove(root);
}
