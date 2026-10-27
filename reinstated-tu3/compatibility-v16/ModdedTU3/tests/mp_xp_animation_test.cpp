#define NOMINMAX
#include "../src/mp_progression_runtime.h"
#include "../src/mp_xp_animation.h"
#include <cassert>
int main(){
 using namespace pdz_progression;
 Rank a,b;a.level_xp=500;b=a;AwardXp(b,100);
 auto f=AnimateXp(a,b,0);assert(f.level==1&&f.fraction>.9f);
 f=AnimateXp(a,b,1.1);assert(f.level==2&&f.fraction==0);
 f=AnimateXp(a,b,3);assert(f.complete&&f.level==2&&f.fraction>0);
 b=a;AwardXp(b,5000);f=AnimateXp(a,b,100);assert(f.level==b.level&&f.complete);
 Rank cap;cap.level=49;cap.level_xp=RequiredXp(49)-1;b=cap;AwardXp(b,1000);
 f=AnimateXp(cap,b,10);assert(f.level==50&&f.fraction==1);
 Runtime r;auto root=std::filesystem::temp_directory_path()/"pdz-xp-animation-test";
 std::filesystem::remove_all(root);r.Configure(root);r.BeginMatch();
 Event e;e.xp=600;r.Record(0,e);r.BeginMatch();r.FinishMatch();r.FinishMatch();
 MatchXp m;assert(r.NextMatch(m)&&m.before.level==1&&m.after.level==2);assert(!r.NextMatch(m));
 Rank n;assert(r.NextLevel(n)&&n.level==2);assert(!r.NextLevel(n));
 r.UnloadMatch();r.BeginMatch();
 r.FinishMatch();assert(r.NextMatch(m)&&m.before.lifetime_xp==600&&m.after.lifetime_xp==600);
 r.Shutdown();std::filesystem::remove_all(root);
}

