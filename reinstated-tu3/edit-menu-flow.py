from pathlib import Path
p=Path('PerfectDarkZeroRecomp/src/native_pointer.cpp');s=p.read_text();s=s.replace('  static uint64_t last_motion = 0;\n  if (last_motion == pointer.motion && !pointer.click) return;','  // Recheck a stationary pointer: native scene/controller updates can move focus.');s=s.replace('  call.r4.u64 = REX_LOAD_U32(0x82D356C4);\n  sub_82779A50(call, base);\n  uint32_t focused_root', '  const uint32_t focused_handle = call.r3.u32;\n  call.r4.u64 = REX_LOAD_U32(0x82D356C4);\n  sub_82779A50(call, base);\n  uint32_t focused_root');s=s.replace('  last_motion = pointer.motion;','');s=s.replace('  call.r3.u64 = handle; call.r4.u64 = focus_user;\n  sub_8277F5C8(call, base);', '  if (focused_handle == handle) {\n    pdz_input::PointerAccept(pointer.click);\n    return;\n  }\n  call.r3.u64 = handle; call.r4.u64 = focus_user;\n  sub_8277F5C8(call, base);');p.write_text(s)
p=Path('PerfectDarkZeroRecomp/src/native_campaign.cpp');s=p.read_text().replace('#include <map>','#include <map>\n#include <chrono>\n#include <algorithm>');s=s.replace('bool busy = false;', '''bool busy = false;
bool return_to_style=false, mission_mode_seen=false;
enum class StyleFade { None, Open, Close, In };
StyleFade style_fade=StyleFade::None;
std::chrono::steady_clock::time_point fade_start;
uint32_t fade_control=0, fade_handle=0;
void Alpha(PPCContext& call,uint8_t* base,float alpha) {
  if(!scene)return;
  call.r3.u64=scene;call.f1.f64=alpha;sub_823AEBE0(call,base);
}''');s=s.replace('bool Open(PPCContext& call, uint32_t control, uint32_t handle, uint8_t* base) {','bool OpenNow(PPCContext& call, uint32_t control, uint32_t handle, uint8_t* base) {');needle='}\n}\n\nnamespace pdz_campaign {\nbool BackKey';replacement='''}
bool Open(PPCContext& call,uint32_t control,uint32_t handle,uint8_t* base) {
  const auto out=call.r1.u32+80;REX_STORE_U32(out,0);
  call.r3.u64=handle;call.r4.u64=out;sub_8277A6F0(call,base);
  scene=REX_LOAD_U32(out);
  if(!scene)return false;
  fade_control=control;fade_handle=handle;
  style_fade=StyleFade::Open;fade_start=std::chrono::steady_clock::now();
  return true;
}
void FadeTick(PPCContext& call,uint8_t* base) {
  if(style_fade==StyleFade::None)return;
  const float t=std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now()-fade_start).count()/.18f,0.f,1.f);
  Alpha(call,base,style_fade==StyleFade::In?t:1.f-t);
  if(t<1.f)return;
  if(style_fade==StyleFade::In){style_fade=StyleFade::None;return;}
  if(style_fade==StyleFade::Open)OpenNow(call,fade_control,fade_handle,base);
  else Restore(call,base);
  Alpha(call,base,0.f);style_fade=StyleFade::In;fade_start=std::chrono::steady_clock::now();
}
}

namespace pdz_campaign {
bool BackKey''';assert needle in s;s=s.replace(needle,replacement)
s=s.replace('  if (!choosing || busy) return;\n  auto call = ctx;', '  if (busy) return;\n  auto call = ctx;');s=s.replace('  if (cancel) { busy = true; Restore(call, base); busy = false; return; }', '''  busy=true;FadeTick(call,base);busy=false;
  if (cancel && style_fade==StyleFade::None) {
    cancel=false;style_fade=StyleFade::Close;fade_start=std::chrono::steady_clock::now();
  }''');s=s.replace('  const bool portrait = id == u"level pic" || id == u"level pic1";', '''  if(return_to_style&&!choosing&&style_fade==StyleFade::None) {
    if(id==u"Solo")mission_mode_seen=true;
    if(id==u"MissionMode"&&mission_mode_seen) {
      return_to_style=false;mission_mode_seen=false;
      call.r3.u64=rendered_handle;sub_8276C0F8(call,base);
      busy=true;OpenNow(call,mission_control,call.r3.u32,base);busy=false;
    }
  }
  if(!choosing)return;
  const bool portrait = id == u"level pic" || id == u"level pic1";''')
s=s.replace('      if (choosing) {\n        if (id == u"BackButton") { Restore(call, base); consumed = true; }', '''      if(style_fade!=StyleFade::None) {consumed=true;}
      else if (choosing) {
        if (id == u"BackButton") { cancel=true; consumed = true; }''');s=s.replace('            REXCVAR_SET(pdz_campaign_rules, rules[i]);', '            return_to_style=true;mission_mode_seen=false;\n            REXCVAR_SET(pdz_campaign_rules, rules[i]);');p.write_text(s)
