#include "../src/model_sync_check.h"
#include <cassert>
#include <map>
#include <iostream>
#include <vector>
int main(){
 std::map<uint32_t,uint32_t> words;
 std::vector<std::pair<uint32_t,uint32_t>> blocks{{100,52},{200,12},{1000,8},{2000,8},{3000,80},{4000,8},{5000,96},{6000,8},{7000,96},{8000,80},{9000,96},{10000,96}};
 auto access=[&](uint32_t a,uint32_t n,bool){for(auto [p,s]:blocks)if(a>=p&&uint64_t(a)+n<=uint64_t(p)+s)return true;return false;};
 auto read=[&](uint32_t a){assert(access(a,4,false));return words[a];};
 auto ready=[&]{return pdz_model_sync::NodeTablesReady(100,1000,2000,read,access);};
 words={{148,200},{112,0},{200,2},{204,3000},{208,8000},
  {3000,99},{3040,99},{8000,99},{8040,99},{1004,4000},{2004,6000},
  {4000,5000},{4004,9000},{6000,7000},{6004,10000},
  {5000,111},{5048,112},{9000,113},{9048,114}};
 assert(ready());
 for(auto at:{1004u,4000u,4004u,2004u,6000u,6004u}){
  auto old=words[at];words[at]=0;assert(!ready());words[at]=old;
 }
 // A float masquerading as a pointer (seen in dump 9256) must fail safely.
 words[1004]=0x3f800000;assert(!ready());words[1004]=4000;
 words[200]=0x7fffffff;assert(!ready());words[200]=2;
 words[112]=0xffffffff;assert(!ready());words[112]=0;
 // Native optional arrays and optional source materials can legitimately be null.
 words[204]=words[208]=0;words[1004]=words[2004]=0;assert(ready());
 words[204]=3000;words[1004]=4000;words[5000]=words[5048]=0;assert(ready());
 words[200]=0;assert(ready());
 std::cout<<"PASS: missing source/destination tables, both material sets, overflow, float-pointer corruption and valid optional nulls\n";
}
