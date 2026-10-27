#include "../src/gore_policy.h"
#include "../src/death_voice_policy.h"
#include <cassert>
int main(){
 using namespace gore_policy;
 unsigned heads=0,arms=0;
 for(unsigned r=0;r<100;++r){heads+=ImpactPart(27,1,r,0)==Part::Head;arms+=ImpactPart(27,3,r,0)==Part::LeftArm;}
 assert(heads==10&&arms==25);
 assert(ImpactPart(27,3,0,1)==Part::RightArm);
 assert(ImpactPart(27,14,99,0)==Part::LeftLeg);
 assert(ImpactPart(27,19,99,0)==Part::RightLeg);
 assert(ImpactPart(27,6,99,0)==Part::LeftArm);
 assert(ImpactPart(27,0,0,0)==Part::None);
 assert(ImpactPart(10,1,99,0)==Part::Head);
 assert(Allowed(27,Part::LeftLeg,100));
 assert(!Allowed(27,Part::None,100));
}
