#include "../src/mission_summary_policy.h"
#include <cassert>
#include <limits>
int main() {
  using namespace pdz_results;
  Summary s{};
  s.completed = true; s.mission = 1; s.campaign_style = 1;
  s.target_seconds = 240; s.time_cs = 23999;
  assert(EarnedAlertedGuards(s));
  s.time_cs = 24000; assert(!EarnedAlertedGuards(s));
  s.time_cs = 24001; assert(!EarnedAlertedGuards(s));
  s.time_cs = 0; assert(!EarnedAlertedGuards(s));
  s.time_cs = 23999;
  s.completed = false; assert(!EarnedAlertedGuards(s)); s.completed = true;
  s.mode = 2; assert(!EarnedAlertedGuards(s)); s.mode = 0;
  s.mission = 2; assert(!EarnedAlertedGuards(s)); s.mission = 1;
  s.difficulty = 1; assert(!EarnedAlertedGuards(s)); s.difficulty = 0;
  s.campaign_style = 0; assert(!EarnedAlertedGuards(s)); s.campaign_style = 1;
  s.target_seconds = 0; assert(!EarnedAlertedGuards(s));
  s.target_seconds = 300; s.time_cs = 29999; assert(EarnedAlertedGuards(s));
  assert(Clock(23999) == "3:59.99"); assert(Clock(24000) == "4:00.00");
  assert(Accuracy(s) == 0);
  s.shots = 20; s.head_hits = 2; s.body_hits = 6; s.limb_hits = 2;
  assert(Accuracy(s) == 50);
}
