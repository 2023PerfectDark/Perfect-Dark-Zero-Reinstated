#pragma once
#include <string_view>

namespace pdz_klobb {
enum class ReloadCue { Native, Suppress, Original };
constexpr bool CueEquals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i=0;i<a.size();++i) {
    const char c=a[i]>='A'&&a[i]<='Z'?char(a[i]+'a'-'A'):a[i];
    if(c!=b[i])return false;
  }
  return true;
}
constexpr ReloadCue KlobbReloadCue(unsigned weapon, unsigned animation, std::string_view name) {
  if(weapon!=39||(animation!=6&&animation!=16))return ReloadCue::Native;
  constexpr std::string_view prefix="xact_soundbank_guns_";
  if(name.size()>prefix.size()&&CueEquals(name.substr(0,prefix.size()),prefix))
    name.remove_prefix(prefix.size());
  if(CueEquals(name,"cmp_clipin"))return ReloadCue::Original;
  // Both native CMP 1handreload tracks carry RAISECHAMBER at time0.4.
  // Replace it only during reload; the equip animation keeps its own cue.
  if(CueEquals(name,"cmp_raisechamber"))return ReloadCue::Original;
  if(CueEquals(name,"cmp_clipout")||CueEquals(name,"cmp_cliphand")||
     CueEquals(name,"cmp_chamber"))return ReloadCue::Suppress;
  // Preserve firing and unrelated animation sounds.
  return ReloadCue::Native;
}
}
