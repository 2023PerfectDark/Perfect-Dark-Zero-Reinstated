#include "../src/spread_policy.h"
#include <cassert>
#include <iostream>
int main(){
 pdz_spread::State s;
 assert(s.Apply(1,0,2,3,1,0)==5);
 for(int i=0;i<100;i++)assert(s.Apply(1,2,2,i,1,i*.1)==2);
 s={};double first=s.Apply(1,1,2,3,1,0),v=first;
 for(int i=1;i<=30;i++) {double n=s.Apply(1,1,2,3,1,i*.1);assert(n>=v&&n-v<.2);v=n;}
 assert(v>first&&v<5);
 assert(s.Apply(1,1,2,3,1,3)==v);
 double aimed=s.Apply(1,1,2,3,2,3.1);assert(aimed<first);
 assert(s.Apply(1,1,2,0,1,20)<2.001);
 assert(s.Apply(2,1,2,3,1,20.1)==2);
 assert(s.Apply(2,2,2,3,1,20.2)==2);
 assert(s.Apply(2,0,2,3,1,20.3)==5);
 std::cout<<"Spread policy tests passed\n";
}
