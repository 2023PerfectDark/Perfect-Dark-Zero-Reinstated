#include "../src/surface_mission.h"
#include "../src/surface_ending.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace pdz_surface;
void KeysAndPlans(Mission& m) {
  assert(!m.Apply(Event::PlansPickedUp));
  assert(!m.Apply(Event::SafeKeyPickedUp));
  assert(!m.Apply(Event::CabinUnlocked));
  m.Apply(Event::CabinKeyPickedUp); m.Apply(Event::CabinUnlocked);
  m.Apply(Event::SafeKeyPickedUp); m.Apply(Event::SafeUnlocked);
  m.Apply(Event::PlansPickedUp);
}
void Locks(Mission& m) {
  m.Apply(Event::VentLockDestroyed0); m.Apply(Event::VentLockDestroyed1);
  m.Apply(Event::VentLockDestroyed2); m.Apply(Event::VentLockDestroyed3);
}
int main() {
  VentEntry shaft;
  assert(!shaft.Update({13716,0,-10939})); // Walking beneath it cannot finish.
  assert(!shaft.Update({13716,830,-10400})); // Roof arms the crossing.
  assert(!shaft.Update({13716,830,-10939})); // Above the rim is not entry.
  assert(shaft.Update({13716,820,-10939}));
  assert(!shaft.Update({13716,700,-10939})); // One ending per load.
  VentEntry outside;
  outside.Update({13716,830,-10400});
  assert(!outside.Update({15000,790,-10939})); // Outer-edge fall is not extraction.
  assert(!outside.Update({13716,790,-10939}));
  assert(EndingFade(0)==0 && EndingFade(1.7)>.49f && EndingFade(2.5)==1);
  Mission entered;
  assert(entered.Apply(Event::FellIntoVentilation));
  assert(entered.Get(Objective::Ventilation)==Status::Complete);
  assert(entered.Get(Objective::Dish)==Status::Incomplete);
  assert(entered.State().vent_locks==0 && entered.State().result==Result::Failure);
  std::array<std::array<uint8_t,8>,14> completion{};
  assert(!CampaignComplete(completion));
  for(auto& mission:completion)mission[0]=1;
  assert(CampaignComplete(completion));
  for(unsigned missing=0;missing<14;++missing){
    auto partial=completion;partial[missing]={};
    assert(!CampaignComplete(partial));
  }
  for(unsigned difficulty=0;difficulty<8;++difficulty){
    completion={};
    for(auto& mission:completion)mission[difficulty]=1;
    assert(CampaignComplete(completion));
  }
  const Listening valid{true, true, true, true, true, false};
  for (int fps : {30, 60, 144, 240}) {
    Mission m;
    m.TickRecording(0.1, valid); assert(m.State().recorded_seconds == 0);
    m.Apply(Event::DishSwitchedOff); // Dish first must not block intel.
    m.Apply(Event::EnterHelipadObservationArea);
    for (int i = 0; i < 5 * fps; ++i) m.TickRecording(1.0/fps, valid);
    const double partial = m.State().recorded_seconds;
    auto paused = valid; paused.paused_or_cutscene = true;
    auto blocked = valid; blocked.unobstructed = false;
    auto wrong_target = valid; wrong_target.target_is_meeting_speaker = false;
    for (int i = 0; i < 100; ++i) {
      m.TickRecording(0.1, paused); m.TickRecording(0.1, blocked);
      m.TickRecording(0.1, wrong_target);
    }
    m.TickRecording(std::numeric_limits<double>::quiet_NaN(), valid);
    m.TickRecording(1000, valid); m.TickRecording(-1, valid);
    assert(m.State().recorded_seconds == partial);
    Mission checkpoint; assert(checkpoint.Restore(m.State()));
    for (int i = 0; i < 13 * fps; ++i) checkpoint.TickRecording(1.0/fps, valid);
    assert(checkpoint.Get(Objective::Recording) == Status::Complete);
    checkpoint.Apply(Event::MeetingSpeakerKilled);
    assert(checkpoint.State().result == Result::Active);
    KeysAndPlans(checkpoint);
    assert(!checkpoint.Apply(Event::EnterVentilation));
    Locks(checkpoint); checkpoint.Apply(Event::EnterVentilation);
    assert(checkpoint.State().result == Result::Success);
    assert(!checkpoint.Apply(Event::DishControlsDestroyed));
    checkpoint.Reset(); assert(checkpoint.State().recorded_seconds == 0);
    assert(checkpoint.State().result == Result::Active);
  }
  Mission terminal;
  terminal.Apply(Event::DishSwitchedOff);
  terminal.Apply(Event::DishControlsDestroyed);
  assert(terminal.Get(Objective::Dish) == Status::Failed);
  assert(!terminal.Apply(Event::DishSwitchedOff));
  Mission witness; witness.Apply(Event::MeetingSpeakerKilled);
  assert(witness.State().result == Result::Failure);
  Mission premature; Locks(premature); premature.Apply(Event::EnterVentilation);
  assert(premature.State().result == Result::Failure);
  Mission restore; Snapshot bad;
  bad.safe_open = true; assert(!restore.Restore(bad));
  bad = {}; bad.recorded_seconds = 1; assert(!restore.Restore(bad));
  bad = {}; bad.objectives[0] = Status(9); assert(!restore.Restore(bad));
  bad = {}; bad.result = Result::Success; assert(!restore.Restore(bad));
  assert(restore.State().result == Result::Active);
  std::cout << "Surface objective and checkpoint tests passed\n";
}
