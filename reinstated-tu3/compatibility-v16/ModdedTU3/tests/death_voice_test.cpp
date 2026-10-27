#include "../src/death_voice_policy.h"
#include <cassert>
#include <limits>
int main(){
 using namespace death_voice;
 assert(KnockoutAnimation("aid_anim_chars_knockout_fallright_downidle")==1);
 assert(KnockoutAnimation("aid_anim_chars_knockout_fallright_getup")==-1);
 assert(KnockoutAnimation("aid_anim_chars_death_normal_stomach_23fallontoback")==0);
 assert(KnockoutAnimation("aid_anim_chars_jackdark_cell_idle_loop")==0);
 assert(Dismemberment(true,100,0));
 assert(!Dismemberment(true,0,0));
 assert(!Dismemberment(false,100,0));
 assert(!Dismemberment(true,100,20));
 assert(!Dismemberment(true,100,std::numeric_limits<float>::quiet_NaN()));
 assert(RouteDeath("elite1_long_scream",true,false)==Route::Defer);
 assert(RouteDeath("elite1_long_scream",false,true)==Route::Suppress);
 assert(RouteDeath("elite1_alert",true,false)==Route::Native);
 assert(RouteDeath("",true,false,true)==Route::Defer);
 assert(Fatal(1,100,0,false));
 assert(!Fatal(1,100,0,true));
}
