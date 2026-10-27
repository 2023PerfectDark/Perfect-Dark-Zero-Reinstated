#include "generated/default/pdzstocktu3_init.h"
#include "klobb_weapon.h"
#include "klobb_ui.h"
#include "klobb_reload_policy.h"
#include "mission_weapon_policy.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include "utils.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <bit>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <SDL3/SDL.h>
#include <algorithm>
#include <vector>

// Enable after the separately named native model has passed visual validation.
// Presence of a loose file alone does not certify its rig/material conversion.
REXCVAR_DEFINE_BOOL(pdz_klobb_pickup, true, "Gameplay", "Enable the native mission-only Surface Klobb pickup");

REX_EXTERN(__imp__sub_824F2C10);
REX_EXTERN(__imp__sub_824F1BA0);
REX_EXTERN(__imp__sub_824F1C38);
REX_EXTERN(__imp__sub_824F2C68);
REX_EXTERN(__imp__sub_824F40D0);
REX_EXTERN(__imp__sub_824F4910);
REX_EXTERN(__imp__sub_82247EB8);

namespace {
constexpr uint32_t kRecordBytes = 2196;
// aid_xml_guns_cmp's binary XML header declares a 0x4d4-byte structure.
constexpr uint32_t kDefinitionBytes = 1236;
constexpr uint32_t kStockId = 17; // CMP150 gameplay, sound and animation donor.
constexpr auto& kModel = pdz_klobb::kModelName;
constexpr uint32_t kUiTable = 0x82D27CC8;
constexpr uint32_t kUiRow = kUiTable + pdz_mission_weapons::kKlobb * 20;
constexpr float kPickupX = -13896.972616f;
constexpr float kPickupY = 166.666664f;
constexpr float kPickupZ = -17367.503626f;

struct Registration {
  uint32_t allocation = 0;
  uint32_t record = 0;
  uint32_t definition = 0;
  uint32_t manager = 0;
  uint32_t source = 0;
  uint32_t resource = 0;
  uint32_t references = 0;
  std::array<uint32_t, 5> ui{};
  uint32_t donor_package = 0;
} klobb;
bool collected = false;
bool preparing = false;
thread_local bool loading_model = false;
std::chrono::steady_clock::time_point next_attempt{};
std::string_view prepare_failure{};
unsigned prepare_attempts = 0;
unsigned grant_attempts = 0;
unsigned reference_events = 0;
unsigned proximity_events = 0;
unsigned action_events = 0;
bool traced_alternate_block = false;
int proximity_state = -1;
uint32_t last_grant_id = UINT32_MAX;
unsigned trace_lines = 0;

// Independent of the user's console log level. Bound output for the process,
// and additionally suppress repeated per-second failure stages below.
template <typename... Args>
void Trace(Args&&... args) noexcept {
  static_assert(sizeof...(args) > 0);
  try {
    if (trace_lines >= 256) return;
    const auto directory = utils::RepoRoot() / "out/surface";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return;
    std::ofstream log(directory / "klobb-runtime.log", std::ios::app);
    if (!log) return;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    log << now << ' ';
    (log << ... << args);
    log << '\n';
    ++trace_lines;
  } catch (...) {
    // Diagnostics must not alter native grant/cleanup behavior.
  }
}
bool PrepareFailed(std::string_view stage, uint32_t a = 0, uint32_t b = 0) {
  if (prepare_failure != stage) {
    Trace("prepare-failed stage=", stage, " a=0x", std::hex, a, " b=0x", b);
    prepare_failure = stage;
  }
  return false;
}

bool Readable(uint32_t address, uint32_t size) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !address || uint64_t(address) + size > 0x100000000ull) return false;
  for (uint64_t at = address; at < uint64_t(address) + size;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(at));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(at), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    at = (at / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
bool Writable(uint32_t address, uint32_t size) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !Readable(address, size)) return false;
  for (uint64_t at = address; at < uint64_t(address) + size;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(at));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(at), &protection) ||
        !(protection & rex::memory::kMemoryProtectWrite)) return false;
    at = (at / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
bool UiSlotAvailable(uint8_t* base) {
  if (!Writable(kUiRow, 20) || !Readable(kUiTable + kStockId * 20, 20)) return false;
  // Supported retail has an unused, entirely zero row39. Do not overwrite a
  // different mod's registration or broaden the native table's bounds.
  for (unsigned i = 0; i < 5; ++i)
    if (REX_LOAD_U32(kUiRow + i * 4)) return false;
  return true;
}
void PublishUi(uint8_t* base) {
  klobb.ui = {pdz_klobb_ui::kNameText, pdz_klobb_ui::kDescriptionText,
              REX_LOAD_U32(kUiTable + kStockId * 20 + 8),
              REX_LOAD_U32(kUiTable + kStockId * 20 + 12),
              REX_LOAD_U32(kUiTable + kStockId * 20 + 16)};
  for (unsigned i = 0; i < klobb.ui.size(); ++i)
    REX_STORE_U32(kUiRow + i * 4, klobb.ui[i]);
}
void RestoreUi(uint8_t* base, const Registration& old) {
  if (!Writable(kUiRow, 20)) return;
  for (unsigned i = 0; i < old.ui.size(); ++i)
    if (REX_LOAD_U32(kUiRow + i * 4) != old.ui[i]) return;
  for (unsigned i = 0; i < old.ui.size(); ++i) REX_STORE_U32(kUiRow + i * 4, 0);
}
bool Scratch(PPCContext& call) {
  if (call.r1.u32 < 4096 || !Readable(call.r1.u32 - 4096, 4096)) return false;
  call.r1.u32 -= 2048;
  return true;
}
uint32_t WeaponId(uint8_t* base, uint32_t weapon) {
  if (!Readable(weapon, 864) || REX_LOAD_U32(weapon + 12) != 4) return 0;
  const auto definition = REX_LOAD_U32(weapon + 860);
  return Readable(definition, 1148) ? REX_LOAD_U32(definition + 1144) : 0;
}
bool PrivateRecord(uint32_t record) {
  return record && record == klobb.record && Readable(record, kRecordBytes);
}
bool KlobbAction(uint8_t* base, uint32_t action) {
  if (!pdz_input::Supported() || !Readable(action, 32)) return false;
  const auto weapon = REX_LOAD_U32(action + 28);
  return WeaponId(base, weapon) == pdz_mission_weapons::kKlobb &&
         Readable(weapon, 1252) && PrivateRecord(REX_LOAD_U32(weapon + 1248));
}

struct ReloadAudio {
  SDL_AudioStream* stream=nullptr;
  std::vector<uint8_t> pcm;
  bool attempted=false, initialized=false;
  uint32_t last_action=0;
  std::chrono::steady_clock::time_point last_play{};
  void Stop() {
    if(stream)SDL_DestroyAudioStream(stream);
    if(initialized)SDL_QuitSubSystem(SDL_INIT_AUDIO);
    stream=nullptr;initialized=false;attempted=false;pcm.clear();
    last_action=0;last_play={};
  }
  bool Ready() {
    if(attempted)return stream!=nullptr;
    attempted=true;
    const auto file=utils::RepoRoot()/"mod_assets/surface/klobb-reload.wav";
    std::error_code error;
    const auto bytes=std::filesystem::file_size(file,error);
    if(error||bytes>262144)return false;
    SDL_AudioSpec spec{};uint8_t* data=nullptr;uint32_t length=0;
    if(!SDL_LoadWAV(file.string().c_str(),&spec,&data,&length))return false;
    const bool valid=spec.format==SDL_AUDIO_S16LE&&spec.channels==1&&
                     spec.freq==44100&&length==27536*2;
    if(valid)pcm.assign(data,data+length);
    SDL_free(data);
    if(!valid||!SDL_InitSubSystem(SDL_INIT_AUDIO))return false;
    initialized=true;
    stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
    if(!stream){SDL_QuitSubSystem(SDL_INIT_AUDIO);initialized=false;return false;}
    Trace("reload-audio-ready GoldenEye GUN_RIFLECOCK_SFX samples=27536");
    return true;
  }
  bool Play(uint32_t action,float gain) {
    const auto now=std::chrono::steady_clock::now();
    // The gun/hand event tracks can report the same cue in one update.
    if(last_action==action&&now-last_play<std::chrono::milliseconds(100))return true;
    SDL_ClearAudioStream(stream);
    SDL_SetAudioStreamGain(stream,std::clamp(gain,0.f,1.f));
    if(!SDL_PutAudioStreamData(stream,pcm.data(),int(pcm.size()))||
       !SDL_FlushAudioStream(stream)||!SDL_ResumeAudioStreamDevice(stream))return false;
    last_action=action;last_play=now;
    Trace("reload-audio-play original-goldeneye action=0x",std::hex,action);
    return true;
  }
} reload_audio;

// Resource loaders return a handle; record+8 contains the unwrapped descriptor.
// This is the same release operation used by the retail weapon-record disposer.
void ReleaseResource(PPCContext& ctx, uint8_t* base, uint32_t resource) {
  if (!Readable(resource, 12)) return;
  const auto fn = REX_LOAD_U32(0x82E42260);
  if (fn < 0x82000000 || fn >= 0x83000000) return;
  ctx.r3.u64 = REX_LOAD_U32(resource + 4);
  REX_CALL_INDIRECT_FUNC(fn);
}

void Dispose(PPCContext& ctx, uint8_t* base) {
  if (!klobb.allocation) return;
  auto call = ctx;
  if (!Scratch(call)) {
    Trace("dispose-failed stage=guest-stack sp=0x", std::hex, ctx.r1.u32);
    REXLOG_WARN("Klobb: unable to obtain guest stack for resource cleanup");
    return;
  }
  const auto old = klobb;
  Trace("dispose record=0x", std::hex, old.record, " source=0x", old.source,
        " resource=0x", old.resource, std::dec, " references=", old.references);
  if (old.references)
    REXLOG_WARN("Klobb: manager teardown with {} outstanding weapon references", old.references);
  klobb = {};
  RestoreUi(base, old);
  ReleaseResource(call, base, old.resource);
  if (Readable(old.source, kRecordBytes)) {
    call.r3.u64 = old.source;
    __imp__sub_824F2C68(call, base); // Exactly the one lease acquired in Prepare.
  }
  ReleaseResource(call, base, old.donor_package);
  rex::Runtime::instance()->memory()->SystemHeapFree(old.allocation);
  Trace("dispose-complete");
}

bool Prepare(PPCContext& ctx, uint8_t* base) {
  const auto manager = REX_LOAD_U32(0x82D6BFD8 + 48);
  if (klobb.record) {
    if (manager != klobb.manager)
      return PrepareFailed("manager-changed", klobb.manager, manager);
    if (!Readable(klobb.record, kRecordBytes))
      return PrepareFailed("registered-record-unreadable", klobb.record);
    prepare_failure = {};
    return true;
  }
  if (preparing) return PrepareFailed("recursive-prepare");
  if (!Readable(manager, 25)) return PrepareFailed("manager-unreadable", manager);
  if (!UiSlotAvailable(base)) return PrepareFailed("ui-slot-unavailable", kUiRow);
  if (!pdz_klobb_ui::RegisterText(base)) return PrepareFailed("ui-text-allocation");
  std::error_code error;
  if (!std::filesystem::is_regular_file(
          utils::RepoRoot() / "mod_assets/models" / kModel / "default.dbm", error) || error)
    return PrepareFailed("model-file-unavailable", uint32_t(error.value()));
  auto call = ctx;
  if (!Scratch(call)) return PrepareFailed("guest-stack", ctx.r1.u32);
  call.r3.u64 = kStockId;
  __imp__sub_824F2C10(call, base);
  const auto source = call.r3.u32;
  if (!Readable(source, kRecordBytes)) return PrepareFailed("stock-record-unreadable", source);
  const auto source_definition = REX_LOAD_U32(source);
  if (!Readable(source_definition, kDefinitionBytes) ||
      REX_LOAD_U32(source_definition + 1144) != kStockId)
    return PrepareFailed("stock-definition-invalid", source_definition);

  // Surface does not preload CMP150. Retail's record initializer assumes its
  // package/model already exists and dereferences a null descriptor otherwise.
  // Load the same package name retail formats, then verify the model before
  // entering that initializer. Never publish a partially loaded weapon.
  struct Lease {
    PPCContext& call; uint8_t* base; uint32_t handle = 0;
    ~Lease() { ReleaseResource(call, base, handle); }
  } donor_package{call, base}, donor_model{call, base};
  if (!REX_LOAD_U32(source + 8)) {
    const auto package_name = call.r1.u32 + 576;
    const auto descriptor_out = call.r1.u32 + 848;
    call.r3.u64 = package_name;
    call.r4.u64 = 0x820BC778; // Retail weapon-package format.
    call.r5.u64 = REX_LOAD_U32(0x82D25B30 + kStockId * 4);
    sub_824F52E8(call, base);
    REX_STORE_U32(descriptor_out, 0);
    call.r3.u64 = package_name;
    call.r4.u64 = descriptor_out;
    sub_821F4148(call, base);
    donor_package.handle = call.r3.u32;
    if (!Readable(donor_package.handle, 12) || !REX_LOAD_U32(descriptor_out))
      return PrepareFailed("donor-package-unavailable");
    REX_STORE_U32(descriptor_out, 0);
    call.r3.u64 = REX_LOAD_U32(source_definition + 1148);
    call.r4.u64 = descriptor_out;
    sub_821F4148(call, base);
    donor_model.handle = call.r3.u32;
    if (!Readable(donor_model.handle, 12) ||
        !Readable(REX_LOAD_U32(descriptor_out), 40))
      return PrepareFailed("donor-model-unavailable");
    Trace("donor-preflight-success package=0x", std::hex, donor_package.handle,
          " model=0x", donor_model.handle);
  }

  const auto allocation = rex::Runtime::instance()->memory()->SystemHeapAlloc(
      kRecordBytes + kDefinitionBytes + sizeof(kModel), 16);
  if (!allocation) return PrepareFailed("record-allocation");
  const bool trace_attempt = ++prepare_attempts <= 3;
  const auto definition = allocation + kRecordBytes;
  const auto name = definition + kDefinitionBytes;
  std::memcpy(base + name, kModel, sizeof(kModel));
  const auto out = call.r1.u32 + 512;
  REX_STORE_U32(out, 0);
  call.r3.u64 = name;
  call.r4.u64 = out;
  if (trace_attempt)
    Trace("model-load-enter attempt=", prepare_attempts, " manager=0x", std::hex,
          manager, " source=0x", source, " allocation=0x", allocation);
  {
    // The loose-file path hook is allowed only for this exact asset, and only
    // while this native call is in progress on the game thread.
    struct Loading {
      Loading() { loading_model = true; }
      ~Loading() { loading_model = false; }
    } loading;
    sub_821F4148(call, base);
  }
  const auto resource = call.r3.u32;
  const auto descriptor = REX_LOAD_U32(out);
  if (trace_attempt)
    Trace("model-load-return resource=0x", std::hex, resource, " descriptor=0x", descriptor);
  if (!Readable(resource, 32) || !Readable(descriptor, 40) ||
      REX_LOAD_U32(resource + 8) != descriptor) {
    ReleaseResource(call, base, resource);
    rex::Runtime::instance()->memory()->SystemHeapFree(allocation);
    REXLOG_WARN("Klobb: separate native model could not be loaded");
    return PrepareFailed("native-model-load", resource, descriptor);
  }

  preparing = true;
  // Loading an already loaded record retains it. If needed, retail initializes
  // all gun/hand animations, effects and model dependencies first. We borrow
  // those fields under this lease; never dispose them through the copied record.
  call.r3.u64 = source;
  call.r4.u64 = 1;
  if (trace_attempt)
    Trace("stock-load-enter source=0x", std::hex, source);
  __imp__sub_824F1C38(call, base);
  preparing = false;
  if (trace_attempt)
    Trace("stock-load-return model=0x", std::hex, REX_LOAD_U32(source + 8),
          std::dec, " references=", REX_LOAD_U32(source + 1760));
  if (!Readable(REX_LOAD_U32(source + 8), 40) ||
      !REX_LOAD_U32(source + 1760)) {
    ReleaseResource(call, base, resource);
    rex::Runtime::instance()->memory()->SystemHeapFree(allocation);
    return PrepareFailed("native-stock-load", REX_LOAD_U32(source + 8),
                         REX_LOAD_U32(source + 1760));
  }
  // CMP's one-handed reload is used by its dual-wield presentation. Require
  // both native clips before publishing the custom record; never substitute
  // an inventory partner or change the player's actual dual-wield state.
  const auto reload_gun = REX_LOAD_U32(source + 80);
  const auto reload_hand = REX_LOAD_U32(source + 188);
  if (!Readable(reload_gun, 12) || !Readable(reload_hand, 12)) {
    call.r3.u64 = source;
    __imp__sub_824F2C68(call, base);
    ReleaseResource(call, base, resource);
    rex::Runtime::instance()->memory()->SystemHeapFree(allocation);
    return PrepareFailed("cmp-one-hand-reload-unavailable", reload_gun, reload_hand);
  }
  std::memcpy(base + allocation, base + source, kRecordBytes);
  std::memcpy(base + definition, base + source_definition, kDefinitionBytes);
  REX_STORE_U32(allocation, definition);
  REX_STORE_U32(allocation + 4, 0); // No donor secondary definition.
  REX_STORE_U32(allocation + 8, descriptor);
  REX_STORE_U32(allocation + 12, 0); // Native world-model fallback uses this DBM.
  REX_STORE_U32(allocation + 1760, 0);
  REX_STORE_U32(definition + 1144, pdz_mission_weapons::kKlobb);
  REX_STORE_U32(definition + 1148, name);
  REX_STORE_U32(definition + 1152, name);
  REX_STORE_U16(definition + 154, 20);
  // 824E6E28 selects normal reload through record+40/+148; its one-hand
  // branch selects +80/+188 and shares the same definition+324/+328 timing.
  // Override both gun and hand clips in this private copy, leaving CMP150's
  // original record, firing animations and all other weapons unchanged.
  REX_STORE_U32(allocation + 40, reload_gun);
  REX_STORE_U32(allocation + 148, reload_hand);
  REX_STORE_U32(definition + 392, REX_LOAD_U32(source_definition + 424));
  REX_STORE_U32(definition + 500, REX_LOAD_U32(source_definition + 532));
  klobb = {allocation, allocation, definition, manager, source, resource, 0};
  klobb.donor_package = donor_package.handle;
  donor_package.handle = 0;
  PublishUi(base);
  prepare_failure = {};
  Trace("prepare-success id=", pdz_mission_weapons::kKlobb, " record=0x", std::hex,
        allocation, " definition=0x", definition, " model=0x", descriptor,
        std::dec, " donor-id=", kStockId, " stock-references=", REX_LOAD_U32(source + 1760),
        " one-hand-reload-gun=0x", std::hex, reload_gun, " hand=0x", reload_hand);
  REXLOG_INFO("Klobb: registered mission-only ID {}, record {:08X}, model {:08X}",
              pdz_mission_weapons::kKlobb, allocation, descriptor);
  return true;
}
}

namespace pdz_klobb {
bool HandleReloadSound(PPCContext& ctx,uint8_t* base) {
  const auto action=ctx.r3.u32,event=ctx.r4.u32;
  // Native 822339B0 event8 resolves event+8 as a cue name, then plays it
  // through the weapon controller. Other events include ammo refill and FX.
  if(!KlobbAction(base,action)||!Readable(event,12)||REX_LOAD_U16(event+4)!=8)return false;
  const auto weapon=REX_LOAD_U32(action+28),local=REX_LOAD_U32(0x82E36228);
  if(!Readable(local,968)||REX_LOAD_U32(weapon+92)!=REX_LOAD_U32(local+964))return false;
  const auto name=REX_LOAD_U32(event+8);
  std::string cue;
  for(uint32_t i=0;i<128;++i){
    if(!Readable(name+i,1))return false;
    const char c=char(base[name+i]);
    if(!c)break;
    cue+=c;
  }
  if(cue.size()==128)return false;
  const auto kind=KlobbReloadCue(pdz_mission_weapons::kKlobb, REX_LOAD_U32(action+20), cue);
  if(kind==ReloadCue::Native||!reload_audio.Ready())return false;
  if(kind==ReloadCue::Original){
    // Use PDZ's own effects-category volume calculation, including fades.
    auto volume=ctx;volume.r1.u32-=512;
    if(!Writable(volume.r1.u32-16,32))return false;
    volume.r3.u64=13;volume.r4.u64=REX_LOAD_U8(0x82E67C48+13*32+28);
    sub_821FC158(volume,base);
    if(!reload_audio.Play(action,float(volume.r3.u32)/255.f))return false;
  }
  ctx.r3.u64=0; // Native sound-only event returns r27=0.
  return true;
}
bool LoadingModel() { return loading_model; }
bool Collected() { return collected; }
void ResetPickup() {
  reload_audio.Stop();
  if (proximity_state >= 0 || grant_attempts || collected)
    Trace("pickup-reset collected=", collected, " grant-attempts=", grant_attempts);
  collected = false;
  next_attempt = {};
  prepare_failure = {};
  prepare_attempts = grant_attempts = reference_events = proximity_events = action_events = 0;
  proximity_state = -1;
  last_grant_id = UINT32_MAX;
  traced_alternate_block = false;
}

void Tick(PPCContext& ctx, uint8_t* base, uint32_t player) {
  if (!REXCVAR_GET(pdz_klobb_pickup) || collected || preparing ||
      !pdz_input::Supported() || !pdz_surface::Selected(base) ||
      pdz_surface::IntroCameraActive() || pdz_surface::EndingCameraActive() ||
      !Readable(player, 6584)) return;
  const auto health = REX_LOAD_U32(player + 6560);
  if (!Readable(health, 992) ||
      !(std::bit_cast<float>(REX_LOAD_U32(health + 980)) > 0.f)) return;
  const auto x = std::bit_cast<float>(REX_LOAD_U32(player + 48));
  const auto y = std::bit_cast<float>(REX_LOAD_U32(player + 52));
  const auto z = std::bit_cast<float>(REX_LOAD_U32(player + 56));
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
  const auto distance = std::hypot(x - kPickupX, z - kPickupZ);
  const auto height = std::abs(y - kPickupY);
  const bool in_range = distance <= 110.f && height <= 130.f;
  if (proximity_state != int(in_range)) {
    proximity_state = int(in_range);
    if (++proximity_events <= 8)
      Trace("pickup-range inside=", in_range, " player=0x", std::hex, player,
            std::dec, " position=", x, ',', y, ',', z,
            " horizontal=", distance, " vertical=", height);
  }
  if (!in_range) return;
  const auto now = std::chrono::steady_clock::now();
  if (now < next_attempt) return;
  next_attempt = now + std::chrono::seconds(1);
  if (!Prepare(ctx, base)) return;
  auto call = ctx;
  if (!Scratch(call)) {
    PrepareFailed("grant-guest-stack", ctx.r1.u32);
    return;
  }
  call.r3.u64 = player;
  call.r4.u64 = pdz_mission_weapons::kKlobb;
  call.r5.u64 = 0;
  call.r6.u64 = 1;
  call.r7.u64 = 0;
  call.r8.u64 = 0;
  const bool trace_grant = ++grant_attempts <= 3;
  if (trace_grant)
    Trace("grant-enter attempt=", grant_attempts, " player=0x", std::hex, player,
          " record=0x", klobb.record, std::dec, " references=", klobb.references);
  sub_8254C378(call, base);
  const auto weapon = call.r3.u32;
  const auto id = WeaponId(base, weapon);
  if (trace_grant || last_grant_id != id)
    Trace("grant-return attempt=", grant_attempts, " weapon=0x", std::hex, weapon,
          std::dec, " id=", id, " references=", klobb.references);
  last_grant_id = id;
  // A full inventory may reject the native grant. Leave the visible pickup in
  // place and retry later; never delete a user's weapon to make room.
  if (id != pdz_mission_weapons::kKlobb) return;
  call.r3.u64 = weapon;
  call.r4.u64 = 120;
  call.r5.u64 = 1;
  sub_82583F80(call, base);
  call.r3.u64 = weapon;
  call.r4.u64 = 20;
  call.r5.u64 = 1;
  sub_82583D38(call, base);
  call.r3.u64 = player;
  call.r4.u64 = weapon;
  call.r5.u64 = 0;
  Trace("equip-enter weapon=0x", std::hex, weapon, std::dec,
        " magazine-grant=20 reserve-grant=120 references=", klobb.references);
  sub_8254DCD8(call, base);
  collected = true;
  Trace("pickup-complete weapon=0x", std::hex, weapon, std::dec,
        " id=", pdz_mission_weapons::kKlobb, " references=", klobb.references);
  REXLOG_INFO("Klobb: acquired native weapon {:08X}; 20-round magazine, mission-only ID {}",
              weapon, pdz_mission_weapons::kKlobb);
}
}

// Keep custom IDs outside the retail array and its fixed-size menu/save loops.
REX_HOOK_RAW(sub_824F2C10) {
  if (pdz_input::Supported() && ctx.r3.u32 == pdz_mission_weapons::kKlobb && klobb.record) {
    ctx.r3.u64 = klobb.record;
    return;
  }
  __imp__sub_824F2C10(ctx, base);
}
REX_HOOK_RAW(sub_824F1BA0) {
  if (pdz_input::Supported() && ctx.r3.u32 == pdz_mission_weapons::kKlobb && klobb.record) {
    ctx.r3.u64 = klobb.record;
    return;
  }
  __imp__sub_824F1BA0(ctx, base);
}
REX_HOOK_RAW(sub_824F4910) {
  // This argument selects a native first-person action class, not the weapon's
  // identity. Retail's ID39 entry is only a fallback base action; CMP150 has
  // its own constructor. Keep the owned weapon/definition at ID39 throughout.
  if (pdz_input::Supported() && ctx.r4.u32 == pdz_mission_weapons::kKlobb &&
      Readable(ctx.r3.u32, 16)) {
    const auto weapon = REX_LOAD_U32(ctx.r3.u32 + 12);
    if (WeaponId(base, weapon) == pdz_mission_weapons::kKlobb &&
        Readable(weapon, 1252) && PrivateRecord(REX_LOAD_U32(weapon + 1248))) {
      ctx.r4.u64 = kStockId;
      if (++action_events <= 8)
        Trace("action-class donor-id=", kStockId, " weapon-id=", pdz_mission_weapons::kKlobb,
              " weapon=0x", std::hex, weapon);
    }
  }
  __imp__sub_824F4910(ctx, base);
}
REX_HOOK_RAW(sub_82247EB8) {
  // CMP action vtable+68 is the alternate-press handler. Retail sets +46 and
  // +1396, begins holo-open through vtable+284, then swaps visibility groups.
  // Reject only Klobb before any of those mutations. Primary press (+60),
  // release, reload, player aim/FOV and the original CMP150 remain native.
  if (KlobbAction(base, ctx.r3.u32)) {
    if (!traced_alternate_block) {
      Trace("alternate-blocked action=0x", std::hex, ctx.r3.u32,
            " weapon-id=", std::dec, pdz_mission_weapons::kKlobb);
      traced_alternate_block = true;
    }
    ctx.r3.u64 = 0; // Same unhandled result as retail's ineligible-state path.
    return;
  }
  __imp__sub_82247EB8(ctx, base);
}
REX_HOOK_RAW(sub_824F1C38) {
  if (PrivateRecord(ctx.r3.u32)) {
    ++klobb.references;
    REX_STORE_U32(klobb.record + 1760, klobb.references);
    if (++reference_events <= 12)
      Trace("record-retain references=", klobb.references, " return-address=0x", std::hex, ctx.lr);
    return;
  }
  __imp__sub_824F1C38(ctx, base);
}
REX_HOOK_RAW(sub_824F2C68) {
  if (PrivateRecord(ctx.r3.u32)) {
    if (klobb.references) --klobb.references;
    else REXLOG_WARN("Klobb: unbalanced native record release");
    REX_STORE_U32(klobb.record + 1760, klobb.references);
    if (++reference_events <= 12)
      Trace("record-release references=", klobb.references, " return-address=0x", std::hex, ctx.lr);
    return;
  }
  __imp__sub_824F2C68(ctx, base);
}
REX_HOOK_RAW(sub_824F40D0) {
  // Retail invokes this only after level entities have been destroyed, before
  // releasing their gun definitions and the manager's record allocation.
  Dispose(ctx, base);
  pdz_klobb::ResetPickup();
  __imp__sub_824F40D0(ctx, base);
}
