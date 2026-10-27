#pragma once
#include <algorithm>
#include <string>
namespace pdz_leave {
enum class Event {None,Warn,Backup,Final,Hostile};
struct Sequence {
 unsigned warnings=0;bool called=false,final=false,hostile=false;double timer=0;
 Event Tick(double dt,bool inArea,bool arrived){
  if(hostile)return Event::None;
  if(!inArea){warnings=0;called=final=false;timer=0;return Event::None;}
  timer-=std::clamp(dt,0.,.25);
  if(timer>0)return Event::None;
  if(warnings<3){++warnings;timer=5;return Event::Warn;}
  if(!called){called=true;return Event::Backup;}
  if(!final){if(!arrived)return Event::None;final=true;timer=7;return Event::Final;}
  hostile=true;return Event::Hostile;
 }
};
std::string Message();
}
