#include "../src/mp_progression_policy.h"
#include "../src/mp_career_store.h"
#include <cassert>
#include <iostream>
using namespace pdz_progression;
int main() {
  Rank r;
  assert(RequiredXp(1)==538);
  assert(!Prestige(r) && !ResetCareer(r));
  AwardXp(r,537); assert(r.level==1 && r.level_xp==537);
  AwardXp(r,1); assert(r.level==2 && r.level_xp==0);
  for(unsigned prestige=0;prestige<=5;++prestige) {
    AwardXp(r,1000000000);
    assert(r.level==50 && r.prestige==prestige && !r.master);
    assert(!CanResetCareer(r));
    const auto total=r.lifetime_xp;
    assert(Prestige(r));
    assert(r.level==1 && r.lifetime_xp==total);
  }
  assert(r.master && r.prestige==5 && !CanResetCareer(r));
  while(r.level<49) AwardXp(r,RequiredXp(r.level));
  assert(!CanResetCareer(r) && !ResetCareer(r));
  AwardXp(r,RequiredXp(r.level));
  assert(r.level==50 && CanResetCareer(r));
  auto resetAt50=r;
  assert(ResetCareer(resetAt50));
  assert(resetAt50.level==1 && resetAt50.prestige==0 && !resetAt50.master);
  AwardXp(r,RequiredXp(r.level));
  assert(r.level==51 && CanResetCareer(r));
  AwardXp(r,4000000000ull);
  assert(r.level==1000 && CanResetCareer(r) && !Prestige(r));
  assert(ResetCareer(r));
  assert(r.level==1 && r.prestige==0 && !r.master && !r.lifetime_xp);
  r.level=0; AwardXp(r,100); assert(r.lifetime_xp==0);
  assert(Achievements().size()==39);
  unsigned rewardTotal=0;for(const auto& a:Achievements())rewardTotal+=a.xp;
  assert(rewardTotal==43500);
  Career c;Event e;e.sequence=1;e.xp=25;e.kills=1;e.weapon=37;
  e.counters[unsigned(Counter::SniperKills)]=105;
  auto unlocked=Apply(c,e,1700000000);
  assert(unlocked.size()==2 && c.rank.lifetime_xp==1225 && c.kills==1);
  assert(Apply(c,e,1700000001).empty() && c.kills==1);
  ++e.sequence;e.counters={};Apply(c,e,1700000001);
  assert(c.kills==2 && c.rank.lifetime_xp==1250);
  c.rank.level=50;c.rank.level_xp=0;assert(Prestige(c.rank));
  assert(c.counters[unsigned(Counter::SniperKills)]==105 && c.unlocked[27]);
  Careers profiles{};profiles[0]=c;profiles[1].rank.level=48;
  auto bytes=Encode(profiles);Careers loaded;
  assert(Decode(bytes,loaded) && loaded[0].kills==2 && loaded[1].rank.level==48);
  assert(loaded[0].unlocked==c.unlocked);
  bytes[100]^=1;assert(!Decode(bytes,loaded));bytes.pop_back();assert(!Decode(bytes,loaded));
  c.rank={5,50,true,0,1250};auto kept=c;
  assert(Reset(kept,false)&&kept.unlocked==c.unlocked && kept.kills==0);
  assert(Reset(c,true)&&c.unlocked[27]==0&&c.counters[unsigned(Counter::SniperKills)]==0);
  e.sequence=3;e.counters[unsigned(Counter::SniperKills)]=10;
  assert(Apply(c,e,1700000002).size()==1);
  const auto testPath=std::filesystem::temp_directory_path()/"pdz-mp-career-policy-test"/"career.bin";
  assert(SaveCareers(testPath,profiles));assert(LoadCareers(testPath,loaded));
  assert(Encode(loaded)==Encode(profiles));std::filesystem::remove(testPath);
  std::cout << "Progression tier policy checks passed\n";
}
