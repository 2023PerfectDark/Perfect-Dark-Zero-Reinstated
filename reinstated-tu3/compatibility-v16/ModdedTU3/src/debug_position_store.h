#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <span>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

namespace pdz_position {
// A position bookmark, deliberately separate from native saves/checkpoints.
struct Bookmark {
  bool enabled=false, present=false;
  uint32_t mission=0, difficulty=0, rules=0;
  std::array<float,5> pose{}; // xyz, yaw, pitch; no guest pointers.
  bool checkpoint=false; // Native checkpoint reached in this mission attempt.
};
inline bool Valid(const Bookmark& b) {
  if(!b.present)return true;
  if(b.mission>14 || b.difficulty>3 || b.rules>2)return false;
  for(float f:b.pose)if(!std::isfinite(f)||std::abs(f)>1e7f)return false;
  return true;
}
inline bool Matches(const Bookmark& a,const Bookmark& b) {
  return a.present&&a.mission==b.mission&&a.difficulty==b.difficulty&&a.rules==b.rules;
}
inline bool ResumeCheckpoint(const Bookmark& saved,const Bookmark& current,bool pending,unsigned mode) {
  // Surface uses a custom mission controller, not the host mission's checkpoint.
  return pending&&saved.enabled&&saved.checkpoint&&mode==0&&saved.mission<14&&Matches(saved,current);
}
class Store {
 public:
  explicit Store(const std::filesystem::path& root):file_(root/"mod-progress"/"debug-position.bin"){}
  bool Save(const Bookmark& b) const {
    if(!Valid(b))return false;
    std::array<uint32_t,12> words{0x505a4450,2,uint32_t(b.enabled),uint32_t(b.present)|(b.present&&b.checkpoint?2u:0u),b.mission,b.difficulty,b.rules};
    for(unsigned i=0;i<5;++i)words[7+i]=std::bit_cast<uint32_t>(b.pose[i]);
    std::array<uint8_t,52> bytes{};
    for(unsigned i=0;i<12;++i)Put(bytes,i*4,words[i]);
    Put(bytes,48,Crc(std::span(bytes).first(48)));
    std::error_code ec;std::filesystem::create_directories(file_.parent_path(),ec);if(ec)return false;
    auto tmp=file_;tmp+=".tmp";
    {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.flush();if(!out)return false;out.close();if(!out)return false;}
#ifdef _WIN32
    return MoveFileExW(tmp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    std::filesystem::rename(tmp,file_,ec);return !ec;
#endif
  }
  bool Load(Bookmark& result) const {
    std::error_code ec;if(std::filesystem::file_size(file_,ec)!=52||ec)return false;
    std::array<uint8_t,52> bytes{};std::ifstream in(file_,std::ios::binary);
    if(!in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return false;
    const auto version=Get(bytes,4),flags=Get(bytes,12);
    if(Get(bytes,0)!=0x505a4450||(version!=1&&version!=2)||Get(bytes,8)>1||
       (version==1?flags>1:(flags!=0&&flags!=1&&flags!=3))||Get(bytes,48)!=Crc(std::span(bytes).first(48)))return false;
    Bookmark b{bool(Get(bytes,8)),bool(flags&1),Get(bytes,16),Get(bytes,20),Get(bytes,24)};
    b.checkpoint=version==2&&(flags&2);
    for(unsigned i=0;i<5;++i)b.pose[i]=std::bit_cast<float>(Get(bytes,28+i*4));
    if(!Valid(b))return false;result=b;return true;
  }
 private:
  std::filesystem::path file_;
  static void Put(std::span<uint8_t> b,unsigned p,uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(8*i));}
  static uint32_t Get(std::span<const uint8_t> b,unsigned p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(b[p+i])<<(8*i);return v;}
  static uint32_t Crc(std::span<const uint8_t> b){uint32_t c=~0u;for(auto v:b){c^=v;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
};
}
