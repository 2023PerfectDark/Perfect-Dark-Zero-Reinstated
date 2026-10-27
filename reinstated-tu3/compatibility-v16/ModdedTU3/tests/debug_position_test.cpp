#include "../src/debug_position_store.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace pdz_position;
int main(int argc,char** argv){
  assert(argc==3);std::filesystem::path root=argv[2];Store store(root);
  Bookmark expected{true,true,1,0,1,{100,200,300,0.5f,-0.1f}};
  expected.checkpoint=true;
  if(std::string(argv[1])=="save"){assert(store.Save(expected));return 0;}
  Bookmark b;assert(store.Load(b));assert(b.enabled&&Matches(b,expected)&&b.pose==expected.pose&&b.checkpoint);
  assert(ResumeCheckpoint(b,expected,true,0));
  assert(!ResumeCheckpoint(b,expected,false,0));
  for(unsigned mode=1;mode<4;++mode)assert(!ResumeCheckpoint(b,expected,true,mode));
  auto plain=b;plain.checkpoint=false;assert(!ResumeCheckpoint(plain,expected,true,0));
  plain=b;plain.enabled=false;assert(!ResumeCheckpoint(plain,expected,true,0));
  plain=b;plain.mission=14;assert(!ResumeCheckpoint(plain,plain,true,0));
  auto wrong=expected;wrong.mission=5;assert(!Matches(b,wrong));wrong=expected;wrong.difficulty=2;assert(!Matches(b,wrong));
  wrong=expected;wrong.rules=2;assert(!Matches(b,wrong));
  assert(!ResumeCheckpoint(b,wrong,true,0));
  auto bad=expected;bad.pose[0]=std::numeric_limits<float>::quiet_NaN();assert(!store.Save(bad));assert(store.Load(b)&&b.pose==expected.pose);
  // New mission replaces the only slot; it cannot restore the prior mission.
  expected.mission=5;expected.pose[0]=-99;expected.checkpoint=false;assert(store.Save(expected));assert(store.Load(b)&&Matches(b,expected)&&b.pose==expected.pose&&!b.checkpoint);
  expected.enabled=false;assert(store.Save(expected));assert(store.Load(b)&&!b.enabled);
  const auto path=root/"mod-progress"/"debug-position.bin";
  std::ifstream in(path,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});in.close();
  auto write=[&](std::string_view data){std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(data.data(),data.size());};
  for(size_t n=0;n<bytes.size();++n){write(std::string_view(bytes).substr(0,n));b={};assert(!store.Load(b)&&!b.present);}
  for(size_t n=0;n<bytes.size();++n){auto corrupt=bytes;corrupt[n]^=0x40;write(corrupt);assert(!store.Load(b));}
  write(bytes);assert(store.Load(b)&&!b.enabled&&b.mission==5);
  // Existing v1 bookmarks migrate without inventing checkpoint progress.
  auto legacy=bytes;legacy[4]=1;
  uint32_t crc=~0u;for(unsigned i=0;i<48;++i){crc^=uint8_t(legacy[i]);for(int n=0;n<8;++n)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));}crc=~crc;
  for(unsigned i=0;i<4;++i)legacy[48+i]=char(crc>>(8*i));
  write(legacy);assert(store.Load(b)&&b.present&&!b.checkpoint&&b.pose==expected.pose);
  b.checkpoint=true;assert(store.Save(b));assert(store.Load(b)&&b.checkpoint);
  expected.present=false;assert(store.Save(expected));assert(store.Load(b)&&!b.present);
  std::cout<<"Position/checkpoint persistence: separate-process restore, native checkpoint policy, v1 migration, mission isolation, replacement, disable, clear, invalid values, truncation and corruption passed.\n";
}
