#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

// Objective controller for the custom Cold Storage mission. The native level
// adapter must supply validated world events; this is not wired to retail
// missions or the front end until the custom geometry/physics loader is ready.
namespace pdz_surface {
// Native profile records: four solo and four co-op difficulty flags per mission.
// Only the original campaign is required; Surface must never require itself.
inline bool CampaignComplete(const std::array<std::array<uint8_t,8>,14>& records) {
  for(const auto& mission:records){
    bool completed=false;
    for(const auto difficulty:mission)completed|=difficulty!=0;
    if(!completed)return false;
  }
  return true;
}
enum class Objective : uint8_t { Dish, SafeKey, Plans, Recording, Ventilation, Count };
enum class Status : uint8_t { Incomplete, Complete, Failed };
enum class Result : uint8_t { Active, Success, Failure };
enum class Event : uint8_t {
  CabinKeyPickedUp, CabinUnlocked, SafeKeyPickedUp, SafeUnlocked, PlansPickedUp,
  DishSwitchedOff, DishControlsDestroyed, EnterHelipadObservationArea,
  MeetingSpeakerKilled, VentLockDestroyed0, VentLockDestroyed1,
  VentLockDestroyed2, VentLockDestroyed3, EnterVentilation, PlayerDied,
  FellIntoVentilation
};
inline constexpr std::array<std::string_view, 5> kObjectiveNames{
  "Power down communications dish", "Obtain safe key", "Steal building plans",
  "Record the helipad meeting", "Enter base via ventilation tower"
};
struct Snapshot {
  std::array<Status, 5> objectives{};
  Result result = Result::Active;
  bool cabin_key = false, cabin_open = false, safe_open = false, meeting_started = false;
  uint8_t vent_locks = 0;
  double recorded_seconds = 0;
};
struct Listening {
  bool audioscope_active = false;
  bool target_is_meeting_speaker = false;
  bool in_range = false;
  bool unobstructed = false;
  bool conversation_playing = false;
  bool paused_or_cutscene = false;
};
class Mission {
 public:
  static constexpr double kRecordingSeconds = 18;
  const Snapshot& State() const { return state_; }
  Status Get(Objective o) const {
    const auto index = static_cast<size_t>(o);
    return index < state_.objectives.size() ? state_.objectives[index] : Status::Failed;
  }
  void Reset() { state_ = {}; }
  // The adapter advances the briefing speech only while it can be recorded;
  // interrupted listening resumes the current phrase rather than skipping it.
  void TickRecording(double elapsed_seconds, const Listening& input) {
    if (state_.result != Result::Active || !state_.meeting_started ||
        Get(Objective::Recording) != Status::Incomplete || input.paused_or_cutscene ||
        !input.audioscope_active || !input.target_is_meeting_speaker || !input.in_range ||
        !input.unobstructed || !input.conversation_playing ||
        !std::isfinite(elapsed_seconds) || elapsed_seconds <= 0 || elapsed_seconds > 0.5)
      return;
    state_.recorded_seconds += elapsed_seconds;
    if (state_.recorded_seconds >= kRecordingSeconds - 1e-8) {
      state_.recorded_seconds = kRecordingSeconds;
      Complete(Objective::Recording);
    }
  }
  bool Apply(Event event) {
    if (state_.result != Result::Active) return false;
    switch (event) {
      case Event::CabinKeyPickedUp: state_.cabin_key = true; break;
      case Event::CabinUnlocked:
        if (!state_.cabin_key) return false;
        state_.cabin_open = true; break;
      case Event::SafeKeyPickedUp:
        if (!state_.cabin_open) return false;
        Complete(Objective::SafeKey); break;
      case Event::SafeUnlocked:
        if (Get(Objective::SafeKey) != Status::Complete) return false;
        state_.safe_open = true; break;
      case Event::PlansPickedUp:
        if (!state_.safe_open) return false;
        Complete(Objective::Plans); break;
      case Event::DishSwitchedOff: Complete(Objective::Dish); break;
      case Event::DishControlsDestroyed:
        // Original Surface script ai_40 engages emergency power even if the
        // terminal is destroyed AFTER switching off. Keep that failure rule.
        Fail(Objective::Dish); break;
      case Event::EnterHelipadObservationArea: state_.meeting_started = true; break;
      case Event::MeetingSpeakerKilled:
        if (Get(Objective::Recording) != Status::Complete) Fail(Objective::Recording);
        break;
      case Event::VentLockDestroyed0: case Event::VentLockDestroyed1:
      case Event::VentLockDestroyed2: case Event::VentLockDestroyed3:
        state_.vent_locks |= uint8_t(1u << (int(event) - int(Event::VentLockDestroyed0)));
        break;
      case Event::EnterVentilation:
        if (state_.vent_locks != 15) return false;
        [[fallthrough]];
      // A confirmed physical crossing of the open shaft is already entry;
      // it does not claim that any locks or other objectives were completed.
      case Event::FellIntoVentilation:
        Complete(Objective::Ventilation);
        state_.result = Result::Success;
        for (auto status : state_.objectives)
          if (status != Status::Complete) state_.result = Result::Failure;
        break;
      case Event::PlayerDied: state_.result = Result::Failure; break;
      default: return false;
    }
    return true;
  }
  // A checkpoint restores only mission-local state. The existing campaign
  // rules determine whether death can retry, and how health is recovered.
  bool Restore(const Snapshot& saved) {
    if (saved.result != Result::Active || saved.vent_locks > 15 ||
        !std::isfinite(saved.recorded_seconds) || saved.recorded_seconds < 0 ||
        saved.recorded_seconds > kRecordingSeconds) return false;
    for (auto status : saved.objectives)
      if (status != Status::Incomplete && status != Status::Complete) return false;
    const auto complete = [&](Objective o) { return saved.objectives[size_t(o)] == Status::Complete; };
    if ((saved.cabin_open && !saved.cabin_key) ||
        (complete(Objective::SafeKey) && !saved.cabin_open) ||
        (saved.safe_open && !complete(Objective::SafeKey)) ||
        (complete(Objective::Plans) && !saved.safe_open) ||
        (saved.recorded_seconds > 0 && !saved.meeting_started) ||
        (complete(Objective::Recording) != (saved.recorded_seconds == kRecordingSeconds)) ||
        complete(Objective::Ventilation)) return false;
    state_ = saved;
    return true;
  }
 private:
  Snapshot state_{};
  void Complete(Objective o) { state_.objectives[size_t(o)] = Status::Complete; }
  void Fail(Objective o) {
    state_.objectives[size_t(o)] = Status::Failed;
    state_.result = Result::Failure;
  }
};
} // namespace pdz_surface
