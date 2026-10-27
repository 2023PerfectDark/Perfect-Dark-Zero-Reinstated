#pragma once
#include "mp_career.h"
#include <filesystem>
#include <fstream>
#include <span>
#ifdef _WIN32
#include <windows.h>
#endif
namespace pdz_progression {
using Careers=std::array<Career,32>;
inline uint32_t CareerCrc(std::span<const uint8_t> bytes) {
  uint32_t crc=~0u;
  for(auto byte:bytes){crc^=byte;for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
  return ~crc;
}
inline std::vector<uint8_t> Encode(const Careers& profiles) {
  std::vector<uint8_t> bytes{'P','D','M','P',1,0,0,0};
  auto put=[&](uint64_t v){for(unsigned i=0;i<8;++i)bytes.push_back(uint8_t(v>>(8*i)));};
  for(const auto& c:profiles) {
    put(c.rank.prestige);put(c.rank.level);put(c.rank.master);put(c.rank.level_xp);put(c.rank.lifetime_xp);
    for(auto v:c.counters)put(v);for(auto v:c.unlocked)put(v);
    put(c.kills);put(c.deaths);put(c.career_resets);put(c.last_event);
    for(auto v:c.weapon_kills)put(v);
  }
  const auto crc=CareerCrc(bytes);for(unsigned i=0;i<4;++i)bytes.push_back(uint8_t(crc>>(8*i)));
  return bytes;
}
inline bool Decode(std::span<const uint8_t> bytes,Careers& output) {
  constexpr size_t size=8+32*(5+14+39+4+64)*8+4;
  if(bytes.size()!=size||bytes[0]!='P'||bytes[1]!='D'||bytes[2]!='M'||bytes[3]!='P'||bytes[4]!=1||bytes[5]||bytes[6]||bytes[7])return false;
  uint32_t crc=0;for(unsigned i=0;i<4;++i)crc|=uint32_t(bytes[size-4+i])<<(8*i);
  if(CareerCrc(bytes.first(size-4))!=crc)return false;
  size_t at=8;auto get=[&]{uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(bytes[at++])<<(8*i);return v;};
  Careers parsed;
  for(auto& c:parsed) {
    const auto prestige=get(),level=get(),master=get();
    if(prestige>5||level>1000||master>1)return false;
    c.rank.prestige=uint32_t(prestige);c.rank.level=uint32_t(level);c.rank.master=master!=0;
    c.rank.level_xp=get();c.rank.lifetime_xp=get();if(!Valid(c.rank))return false;
    for(auto& v:c.counters)v=get();for(auto& v:c.unlocked)v=get();
    c.kills=get();c.deaths=get();c.career_resets=get();c.last_event=get();
    for(auto& v:c.weapon_kills)v=get();
    for(unsigned i=0;i<Achievements().size();++i)
      if(c.unlocked[i]&&c.counters[unsigned(Achievements()[i].counter)]<Achievements()[i].target)return false;
  }
  output=parsed;return true;
}
inline bool SaveCareers(const std::filesystem::path& path,const Careers& profiles) {
  const auto bytes=Encode(profiles);std::error_code ec;
  std::filesystem::create_directories(path.parent_path(),ec);if(ec)return false;
  auto temporary=path;temporary+=".tmp";
  {std::ofstream f(temporary,std::ios::binary|std::ios::trunc);if(!f)return false;
   f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.flush();if(!f)return false;}
#ifdef _WIN32
  return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
  std::filesystem::rename(temporary,path,ec);return !ec;
#endif
}
inline bool LoadCareers(const std::filesystem::path& path,Careers& profiles) {
  std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)return false;
  const auto n=f.tellg();if(n!=8+32*(5+14+39+4+64)*8+4)return false;
  std::vector<uint8_t> bytes(static_cast<size_t>(n));f.seekg(0);
  f.read(reinterpret_cast<char*>(bytes.data()),bytes.size());return f&&Decode(bytes,profiles);
}
}
