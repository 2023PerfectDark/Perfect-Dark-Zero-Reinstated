#include "weapon_challenge.h"
#include "mp_characters.h"
#include "generated/default/perfectdarkzerorecomp_init.h"
#include "game_cvars.h"
#include "holster_logic.h"
#include "mousehook.h"
#include "surface_runtime.h"
#include "campaign_gore.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>

namespace {
constexpr uint32_t kAttachmentVtable = 0x820C7A80;
struct Attachment { uint32_t object{}, owner{}, weapon{}, model{}; };
std::array<Attachment, 4> attachments{};
Attachment intro_pistol{};
std::array<uint32_t,11> intro_hidden_weapon_models{};
bool intro_pistol_attempted=false;
thread_local uint32_t cosmetic_in_call = 0;
struct CosmeticCall {
  uint32_t previous = cosmetic_in_call;
  explicit CosmeticCall(uint32_t object) { cosmetic_in_call = object; }
  ~CosmeticCall() { cosmetic_in_call = previous; }
};
bool IsCosmetic(uint32_t object) {
  if (!object) return false;
  if (object == cosmetic_in_call) return true;
  if (object == intro_pistol.object) return true;
  for (const auto& slot : attachments) if (slot.object == object) return true;
  return false;
}

bool Readable(uint32_t address, uint32_t bytes) {
  auto* rt = rex::Runtime::instance();
  if (!rt || !address || !bytes || uint64_t(address) + bytes > 0x100000000ull)
    return false;
  for (uint64_t p = address; p < uint64_t(address) + bytes;) {
    auto* heap = rt->memory()->LookupHeap(uint32_t(p));
    uint32_t protection = 0;
    if (!heap || !heap->QueryProtect(uint32_t(p), &protection) ||
        !(protection & rex::memory::kMemoryProtectRead)) return false;
    p = (p / heap->page_size() + 1) * heap->page_size();
  }
  return true;
}
float Float(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(REX_LOAD_U32(address));
}
void StoreFloat(uint8_t* base, uint32_t address, float value) {
  REX_STORE_U32(address, std::bit_cast<uint32_t>(value));
}
bool Valid(uint8_t* base, const Attachment& slot) {
  return Readable(slot.object, 1168) &&
      REX_LOAD_U32(slot.object) == kAttachmentVtable &&
      REX_LOAD_U32(slot.object + 92) == slot.owner;
}
void Remove(PPCContext& ctx, uint8_t* base, Attachment& slot) {
  const auto previous = slot;
  slot = {};
  if (!Valid(base, previous)) return;
  auto call = ctx;
  call.r3.u64 = previous.object;
  call.r4.u64 = 1;
  const CosmeticCall cosmetic(previous.object);
  sub_825884F8(call, base); // Native destructor, parent unlink and allocation free.
}
void Clear(PPCContext& ctx, uint8_t* base) {
  for (auto& slot : attachments) Remove(ctx, base, slot);
}

bool Inventory(PPCContext& ctx, uint8_t* base, uint32_t player,
               std::array<pdz_holsters::Weapon, 13>& weapons, size_t& count);

void UpdateIntroPistol(PPCContext& ctx,uint8_t* base){
  if(!pdz_surface::IntroCameraActive()){
    intro_hidden_weapon_models={};
    Remove(ctx,base,intro_pistol);intro_pistol_attempted=false;return;
  }
  const auto local=REX_LOAD_U32(0x82E35468);
  if(!Readable(local,956))return;
  const auto player=REX_LOAD_U32(local+952);
  if(!Readable(player,7820))return;
  size_t hidden=0;
  for(const uint32_t offset:{4076u,4080u}){
    const auto weapon=REX_LOAD_U32(player+offset);
    intro_hidden_weapon_models[hidden++]=Readable(weapon,76)?REX_LOAD_U32(weapon+72):0;
    intro_hidden_weapon_models[hidden++]=Readable(weapon,1084)?REX_LOAD_U32(weapon+1080):0;
  }
  for(const uint32_t offset:{5272u,7796u,7800u,7804u,7808u,7812u,7816u})
    intro_hidden_weapon_models[hidden++]=REX_LOAD_U32(player+offset);
  if(intro_pistol_attempted)return;
  const auto model=REX_LOAD_U32(player+72);
  if(!Readable(model,4))return;
  const auto definition=REX_LOAD_U32(model);
  if(!Readable(definition,16))return;
  const auto skeleton=REX_LOAD_U32(definition+12);
  if(!Readable(skeleton,56))return;
  constexpr uint32_t joint=58; // Joanna's right wrist.
  const auto bind=REX_LOAD_U32(skeleton+52)+joint*36+12;
  if(!Readable(bind,12))return;
  intro_pistol_attempted=true;
  auto call=ctx;
  if (call.r1.u32 < 2048 || !Readable(call.r1.u32 - 2048, 2048)) {
    intro_pistol_attempted=false;
    return;
  }
  call.r1.u32-=2048;
  const auto name=call.r1.u32+512,out=call.r1.u32+128;
  auto load=[&](const char* text){
    std::memcpy(base+name,text,std::strlen(text)+1);REX_STORE_U32(out,0);
    call.r3.u64=name;call.r4.u64=out;sub_821ED078(call,base);
      // r3 is the resource handle; the out parameter is its unwrapped data.
      // Native attachment constructors require the handle, not the descriptor.
      return call.r3.u32;
  };
  std::array<pdz_holsters::Weapon,13> weapons{};
  size_t count=0;
  Inventory(ctx,base,player,weapons,count);
  const auto selected=pdz_holsters::SelectIntroWeapon(std::span(weapons).first(count));
  // P9P is named Walther P99 in the retail assets. It remains a cosmetic prop;
  // neither the fallback nor the selected gun changes inventory or ammunition.
  const auto resource=selected.resource?selected.resource:
      load("aid_darkcharacter_guns_waltherp99_fp_waltherp99");
  REXLOG_INFO("Surface intro: one-handed prop resource {:08X}, inventory weapon {:08X}, P9P fallback {}",
              resource,selected.object,!selected.object);
  if(!Readable(resource,12)||!Readable(REX_LOAD_U32(resource+8),40)){
    REXLOG_WARN("Surface intro: cutscene Walther resource unavailable");return;
  }
  std::array<float,16> matrix{};
  // The first-person Walther points along +Z; Joanna's right-hand bind
  // skeleton points along -X. Put the grip in her palm, not above the wrist.
  matrix[1]=-1.f;matrix[6]=1.f;matrix[8]=-1.f;matrix[15]=1.f;
  for(unsigned k=0;k<3;++k)matrix[12+k]=Float(base,bind+k*4);
  matrix[12]-=13.f; // Advance along the barrel so the grip meets her palm.
  const auto transform=call.r1.u32+256;
  for(unsigned k=0;k<16;++k)StoreFloat(base,transform+k*4,matrix[k]);
  call.r3.u64=128;call.r4.u64=1168;call.r5.u64=10;sub_82395398(call,base);
  const auto object=call.r3.u32;if(!object)return;
  const CosmeticCall cosmetic(object);
  call.r4.u64=resource;call.r5.u64=player;call.r6.u64=joint;call.r7.u64=transform;
  sub_82588A48(call,base);
  intro_pistol={object,player,selected.object,REX_LOAD_U32(resource+8)};
  if (!Valid(base,intro_pistol)) { intro_pistol={}; return; }
  // Native costume attachments inherit their owner's first-person culling.
  // A cinematic hand prop must remain independently visible to the fly-in camera.
  REX_STORE_U8(object+966,0);
  const auto prop_model=REX_LOAD_U32(object+72);
  if (!Readable(prop_model,512)) { Remove(ctx,base,intro_pistol); return; }
  // Render this attachment in world space, without the first-person gun flag.
  for(unsigned frame=0;frame<2;++frame){
    REX_STORE_U32(prop_model+472+frame*4,1);
    REX_STORE_U32(prop_model+260+frame*80,0);
  }
  call.r3.u64=object;call.r4.u64=0;sub_82508EA8(call,base);
  call.r3.u64=object;sub_82588D28(call,base);
  REXLOG_INFO("Surface intro: attached right-hand cinematic prop {:08X}; starting inventory preserved",object);
}

uint32_t Resource(PPCContext& ctx, uint8_t* base, uint32_t descriptor) {
  if (!Readable(descriptor, 40)) return 0;
  auto call = ctx;
  call.r3.u64 = descriptor;
  sub_821EDB38(call, base); // Find the existing resource; never load a new asset.
  const auto resource = call.r3.u32;
  if (!Readable(resource, 32) || REX_LOAD_U32(resource + 8) != descriptor) return 0;
  return resource;
}

bool Inventory(PPCContext& ctx, uint8_t* base, uint32_t player,
               std::array<pdz_holsters::Weapon, 13>& weapons, size_t& count) {
  const auto inventory = player + 5164;
  const auto list = REX_LOAD_U32(inventory);
  const auto storage = REX_LOAD_U32(inventory + 48);
  if (!Readable(list, 12) || !Readable(storage, 13 * 8)) return false;
  const auto nodes = REX_LOAD_U32(list);
  const auto head = REX_LOAD_U32(list + 8);
  if (!head) return true;
  if (!Readable(nodes, 32 * 8) || head < nodes ||
      uint64_t(head) >= uint64_t(nodes) + 32 * 8 || (head - nodes) % 8) return false;
  const auto left = REX_LOAD_U32(player + 4076), right = REX_LOAD_U32(player + 4080);
  uint32_t visited = 0;
  int index = (head - nodes) / 8;
  while (index != -1) {
    if (index < 0 || index >= 32 || (visited & (1u << index))) return false;
    visited |= 1u << index;
    const auto node = nodes + index * 8;
    const auto data = REX_LOAD_U32(node + 4);
    if (data < storage || uint64_t(data) >= uint64_t(storage) + 104 ||
        (data - storage) % 8) return false;
    if (REX_LOAD_U32(data + 4) == 1) {
      if (count >= weapons.size()) return false;
      const auto weapon = REX_LOAD_U32(data);
      if (!Readable(weapon, 864)) return false;
      const auto definition = REX_LOAD_U32(weapon + 860);
      const auto instance = REX_LOAD_U32(weapon + 72);
      if (!Readable(definition, 1148)) return false;
      const auto handedness = REX_LOAD_U8(definition + 201); // sub_82574AC0.
      if (handedness <= 1 && Readable(instance, 4)) {
        const auto model = REX_LOAD_U32(instance);
        weapons[count++] = {weapon, model, Resource(ctx, base, model),
                            handedness == 1, weapon == left || weapon == right};
      }
    }
    index = int16_t(REX_LOAD_U16(node + 2));
  }
  return true;
}

bool Transform(uint8_t* base, uint32_t character_model, size_t slot,
               std::array<float, 16>& matrix, uint32_t& joint) {
  if (!Readable(character_model, 64)) return false;
  const auto descriptor = REX_LOAD_U32(character_model);
  if (!Readable(descriptor, 40) || REX_LOAD_U32(descriptor + 8) != 109) return false;
  const auto skeleton = REX_LOAD_U32(descriptor + 12);
  if (!Readable(skeleton, 56)) return false;
  joint = slot < 2 ? 9 : (slot == 2 ? 3 : 2); // Thorax, right/left thigh.
  const auto bind = REX_LOAD_U32(skeleton + 52) + joint * 36 + 12;
  if (!Readable(bind, 12)) return false;
  const float x = Float(base, bind), y = Float(base, bind + 4), z = Float(base, bind + 8);
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
  matrix = {};
  matrix[15] = 1.f;
  if (slot < 2) {
    // Barrel down, stock above the shoulder, with two opposing diagonals.
    // Roll the mesh onto its side: weapon X (width) points away from the
    // back, keeping its Y/Z profile, grip and magazine parallel to the back.
    const float diagonal = slot == 0 ? -.38f : .38f;
    const float upright = std::sqrt(1.f - diagonal * diagonal);
    matrix[2] = -1.f;
    matrix[4] = upright; matrix[5] = diagonal;
    matrix[8] = diagonal; matrix[9] = -upright;
    matrix[12] = x + (slot == 0 ? -9.f : 9.f);
    matrix[13] = y + 8.f;
    matrix[14] = z - (slot == 0 ? 13.f : 19.f);
  } else {
    // Barrel follows the thigh. Weapon X points outward so the flat side,
    // rather than the grip/slide profile, rests against the leg.
    const float side = slot == 2 ? -1.f : 1.f;
    // Match the left thigh's grip direction on the right as well, rotating
    // around the barrel axis without changing the attachment position.
    matrix[0] = 1.f; matrix[6] = 1.f; matrix[9] = -1.f;
    matrix[12] = x + side * 10.f;
    matrix[13] = y - 9.f;
    matrix[14] = z + 1.f;
  }
  return true;
}

void Create(PPCContext& ctx, uint8_t* base, size_t index, uint32_t player,
            uint32_t character_model, const pdz_holsters::Weapon& weapon) {
  std::array<float, 16> matrix{};
  uint32_t joint{};
  if (!Transform(base, character_model, index, matrix, joint)) return;
  auto call = ctx;
  // Reserve guest stack space for the matrix beyond the ABI parameter area.
  if (call.r1.u32 < 256 || !Readable(call.r1.u32 - 256, 256)) return;
  call.r1.u32 -= 256;
  REX_STORE_U32(call.r1.u32, ctx.r1.u32);
  const auto transform = call.r1.u32 + 128;
  for (size_t i = 0; i < matrix.size(); ++i) StoreFloat(base, transform + i * 4, matrix[i]);
  call.r3.u64 = 128; call.r4.u64 = 1168; call.r5.u64 = 10;
  sub_82395398(call, base); // Same aligned allocation as native costume attachments.
  const auto object = call.r3.u32;
  if (!object) return;
  call.r4.u64 = weapon.resource;
  call.r5.u64 = player;
  call.r6.u64 = joint;
  call.r7.u64 = transform;
  const CosmeticCall cosmetic(object);
  sub_82588A48(call, base);
  attachments[index] = {object, player, weapon.object, weapon.model};
  if (!Valid(base,attachments[index])) { attachments[index]={}; return; }
  if (!Readable(REX_LOAD_U32(object+72),512)) {
    Remove(ctx,base,attachments[index]);
    return;
  }
  // Native callers activate attachments immediately after construction (for
  // example sub_8223CAE0). We create after the entity tick, so waiting for the
  // next update leaves an inactive object in this frame's visibility pass.
  // That path queries collision properties even though this copy has no body.
  call.r3.u64 = object;
  call.r4.u64 = 0;
  sub_82508EA8(call, base);
  call.r3.u64 = object;
  sub_82588D28(call, base); // Set its pose and bounds before the first render.
  REXLOG_INFO("PDZ holster {}: weapon {:08X}, model {:08X}, attachment {:08X}, joint {}",
              index, weapon.object, weapon.model, object, joint);
}

void Update(PPCContext& ctx, uint8_t* base) {
  if (!pdz_input::Supported() || !REXCVAR_GET(pdz_visible_holsters)) {
    Clear(ctx, base); return;
  }
  const auto session = REX_LOAD_U32(0x82D6B2B0);
  const auto local = REX_LOAD_U32(0x82E35468);
  if (!Readable(session, 40) || REX_LOAD_U32(session + 36) != 0 ||
      !Readable(local, 956)) { Clear(ctx, base); return; }
  const auto player = REX_LOAD_U32(local + 952);
  if (!Readable(player, 6584)) { Clear(ctx, base); return; }
  const auto health = REX_LOAD_U32(player + 6560);
  if (!Readable(health, 972) || !(Float(base, health + 968) > 0.f)) {
    Clear(ctx, base); return;
  }
  const auto model = REX_LOAD_U32(player + 72);
  auto active = ctx;
  active.r3.u64 = player;
  sub_82508DF0(active, base);
  if (!(active.r3.u32 & 0xff) || !Readable(model, 68) ||
      !REX_LOAD_U32(model + 60)) { Clear(ctx, base); return; }
  std::array<pdz_holsters::Weapon, 13> weapons{};
  size_t count = 0;
  if (!Inventory(ctx, base, player, weapons, count)) { Clear(ctx, base); return; }
  const auto layout = pdz_holsters::Select(std::span(weapons).first(count));
  for (size_t i = 0; i < attachments.size(); ++i) {
    auto& slot = attachments[i];
    const auto& desired = layout[i];
    if (slot.object && (!Valid(base, slot) || slot.owner != player ||
        slot.weapon != desired.object || slot.model != desired.model)) Remove(ctx, base, slot);
    if (!slot.object && desired.object) Create(ctx, base, i, player, model, desired);
  }
}
} // namespace

void pdz_surface::RefreshIntroProp(PPCContext& ctx,uint8_t* base) {
  if(!IntroCameraActive() || !Valid(base,intro_pistol))return;
  auto call=ctx;
  call.r3.u64=intro_pistol.object;
  sub_82588D28(call,base);
  const auto model=REX_LOAD_U32(intro_pistol.object+72);
  // Validate the native scenegraph pool before deriving its visibility entry.
  if(model<0x82E735D0u || (model-0x82E735D0u)%512u ||
     (model-0x82E735D0u)/512u>=2500u || !Readable(model,512))return;
  const auto entry=0x82D72B00u+((model-0x82E735D0u)/512u)*8u;
  if(!Readable(entry,8))return;
  REX_STORE_U32(entry+4,REX_LOAD_U32(entry+4)|0x1ffu);
}

REX_EXTERN(__imp__sub_8250D020);
// Earlier visibility-only prototype. No bone/palette edits: third-person
// anatomy and shadows retain native transforms. Kept opt-in at user request.
REX_EXTERN(__imp__sub_82598E70);
REX_EXTERN(__imp__sub_82406880);
REX_HOOK_RAW(sub_82406880) {
  if(pdz_surface::IntroCameraActive() && Valid(base,intro_pistol) &&
     ctx.r3.u32==REX_LOAD_U32(intro_pistol.object+72))ctx.r5.u32=1;
  if(pdz_surface::IntroCameraActive() || pdz_surface::EndingCameraActive()){
    const auto local=REX_LOAD_U32(0x82E35468);
    if(Readable(local,956)){
      const auto player=REX_LOAD_U32(local+952);
      if(Readable(player,4108) &&
         (ctx.r3.u32==REX_LOAD_U32(player+72) || ctx.r3.u32==REX_LOAD_U32(player+4104)))
        ctx.r5.u32&=~0x40u; // Own-player first-person material/head exclusion.
    }

  }
  __imp__sub_82406880(ctx,base);
}
REX_HOOK_RAW(sub_82598E70) {
  const auto player=ctx.r3.u32;
  __imp__sub_82598E70(ctx,base);
  pdz_challenge::Tick(ctx,base,player);
  if (!pdz_input::Supported() ||
      (!REXCVAR_GET(pdz_first_person_body) && !pdz_surface::EndingCameraActive() &&
       !pdz_surface::IntroCameraActive())) return;
  const auto local=REX_LOAD_U32(0x82E35468);
  if (!Readable(local,956) || REX_LOAD_U32(local+952)!=player || !Readable(player,6568)) return;
  const auto owner=REX_LOAD_U32(player+6560);
  if (!Readable(owner,1152)) return;
  const auto view=REX_LOAD_U32(owner+1148);
  if (!Readable(view,24)) return;
  const auto camera=REX_LOAD_U32(view+20);
  if (!Readable(camera,68) || REX_LOAD_U32(camera+16)!=player) return;
  // Match the retail visibility writer's double-buffer index:
  // lis r28,0x82FC; lwz r28,-14736(r28) => 0x82FBC670.
  const auto index=REX_LOAD_U32(camera+64), frame=REX_LOAD_U32(0x82FBC670);
  if(index>=4 || frame>1)return;
  if(pdz_surface::IntroCameraActive() || pdz_surface::EndingCameraActive()){
    // Retail first-person arms and both weapon LODs have independent visibility
    // entries. Clear only this camera's colour bit after the native update;
    // the following gameplay frame restores them through the same update.
    for(const uint32_t offset : {5272u,7796u,7800u,7804u,7808u,7812u,7816u}){
      if(!Readable(player+offset,4))continue;
      const auto fp_model=REX_LOAD_U32(player+offset);
      if(fp_model<0x82E735D0 || (fp_model-0x82E735D0)%512 || !Readable(fp_model,512))continue;
      const uint64_t entry=0x82D72B00ull+uint64_t((fp_model-0x82E735D0)/512)*8;
      if(entry>0xFFFFFFFFull || !Readable(uint32_t(entry),4))continue;
      REX_STORE_U32(uint32_t(entry),REX_LOAD_U32(uint32_t(entry))&~(1u<<(frame*8+index)));
    }
  }
    if(pdz_surface::IntroCameraActive()){
      for(const uint32_t offset:{4076u,4080u}){
        const auto weapon=REX_LOAD_U32(player+offset);
        if(!Readable(weapon,76))continue;
        const auto model=REX_LOAD_U32(weapon+72);
        if(model<0x82E735D0 || (model-0x82E735D0)%512 || !Readable(model,512))continue;
        const uint64_t entry=0x82D72B00ull+uint64_t((model-0x82E735D0)/512)*8;
        if(entry>0xFFFFFFFFull || !Readable(uint32_t(entry),4))continue;
        REX_STORE_U32(uint32_t(entry),REX_LOAD_U32(uint32_t(entry))&~(0x11u<<(frame*8+index)));
      }
    }
  // Joanna's head is a separate scenegraph at +4104 (Jodark_head).
  // Retail hides both models in first person, so revealing +72 alone leaves
  // a headless body. Reveal the head only for external cinematic cameras.
  const bool cinematic=pdz_surface::IntroCameraActive() || pdz_surface::EndingCameraActive();
  for(const uint32_t offset : {72u,4104u}){
    if(offset==4104u && !cinematic)continue;
    const auto model=REX_LOAD_U32(player+offset);
    if(model<0x82E735D0 || (model-0x82E735D0)%512 || !Readable(model,512))continue;
    const uint64_t at=0x82D72B00ull+uint64_t((model-0x82E735D0)/512)*8;
    if(at>0xFFFFFFFFull || !Readable(uint32_t(at),4))continue;
    const auto address=uint32_t(at);
    REX_STORE_U32(address,REX_LOAD_U32(address)|(1u<<(frame*8+index)));
  }
}

#include "coop_ally.h"
#include "nightclub_population.h"
REX_HOOK_RAW(sub_8250D020) {
  pdz_surface::Tick(ctx, base);
  __imp__sub_8250D020(ctx, base);
  UpdateIntroPistol(ctx,base);
  Update(ctx, base); // Entity iteration has finished; safe to link/unlink children.
  pdz_coop::Tick(ctx,base);
  pdz_population::Tick(ctx,base);
}

REX_EXTERN(__imp__sub_82407870);
REX_EXTERN(__imp__sub_82510DB8);
REX_HOOK_RAW(sub_82510DB8) {
  const auto object=ctx.r3.u32,model=ctx.r4.u32;
  __imp__sub_82510DB8(ctx,base);
  pdz_mp::HideArmor(base,object,model);
  if(!pdz_surface::IntroCameraActive() || object!=intro_pistol.object ||
     !Valid(base,intro_pistol) || model!=REX_LOAD_U32(object+72))return;
  // Keep both render buffers visible. Clearing the other buffer here made the
  // prop disappear when the renderer consumed the preceding frame.
  // Validate the native scenegraph pool before deriving its visibility entry.
  if(model<0x82E735D0u || (model-0x82E735D0u)%512u ||
     (model-0x82E735D0u)/512u>=2500u || !Readable(model,512))return;
  const auto entry=0x82D72B00u+((model-0x82E735D0u)/512u)*8u;
  if(!Readable(entry,8))return;
  const auto frame=REX_LOAD_U32(0x82FBC670)&1;
  REX_STORE_U32(model+260+frame*80,0);
  REX_STORE_U32(entry,(REX_LOAD_U32(entry)&~0xffffu)|0x0101u);
}
REX_HOOK_RAW(sub_82407870) {
  const auto model=ctx.r3.u32;
  __imp__sub_82407870(ctx,base);
  pdz_gore::HideHead(base,model);
  if(!pdz_surface::IntroCameraActive() || !model)return;
  bool hide=false;
  for(const auto candidate:intro_hidden_weapon_models)hide|=candidate==model;
  const bool prop=Valid(base,intro_pistol) && REX_LOAD_U32(intro_pistol.object+72)==model;
  if((!hide && !prop) || model<0x82E735D0 || (model-0x82E735D0)%512)return;
  // Validate the native scenegraph pool before deriving its visibility entry.
  if(model<0x82E735D0u || (model-0x82E735D0u)%512u ||
     (model-0x82E735D0u)/512u>=2500u || !Readable(model,512))return;
  const auto entry=0x82D72B00u+((model-0x82E735D0u)/512u)*8u;
  if(!Readable(entry,8))return;
  if(Readable(entry,4)){
    REX_STORE_U32(entry,(REX_LOAD_U32(entry)&~0xffffu)|(prop?0x0101u:0u));
  }
}

REX_EXTERN(__imp__sub_8258A470);
REX_HOOK_RAW(sub_8258A470) {
  // Cosmetic copies share the native mesh resource, but get no collision parts.
  if (!IsCosmetic(ctx.r3.u32)) __imp__sub_8258A470(ctx, base);
}

REX_EXTERN(__imp__sub_82509030);
REX_HOOK_RAW(sub_82509030) {
  // Attachment vtable +100 is the native "has physics" query. Skipping body
  // creation alone leaves its default true response, so activation/rendering
  // can dereference the missing body. Keep spatial/render activation intact.
  if (IsCosmetic(ctx.r3.u32)) { ctx.r3.u64 = 0; return; }
  __imp__sub_82509030(ctx, base);
}

REX_EXTERN(__imp__sub_82588CC0);
REX_HOOK_RAW(sub_82588CC0) {
  // Parent teardown owns its children. Forget our handles before native freeing
  // so retries and level changes cannot leave pointers into reused guest memory.
  const CosmeticCall cosmetic(IsCosmetic(ctx.r3.u32) ? ctx.r3.u32 : cosmetic_in_call);
  for (auto& slot : attachments) if (slot.object == ctx.r3.u32) slot = {};
  if(intro_pistol.object==ctx.r3.u32){intro_pistol={};intro_pistol_attempted=false;}
  __imp__sub_82588CC0(ctx, base);
}



