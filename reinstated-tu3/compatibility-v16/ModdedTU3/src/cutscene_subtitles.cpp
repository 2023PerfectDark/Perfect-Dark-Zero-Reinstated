#include "generated/default/pdzstocktu3_init.h"
#include "cutscene_subtitles.h"
#include "game_cvars.h"
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <imgui.h>
#include <windows.h>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>

namespace {
std::mutex mutex;
std::string caption;
uint32_t voice=0;
bool cinematic=false;
ImFont* subtitleFont=nullptr;
bool Readable(uint32_t p,uint32_t n) {
  auto* rt=rex::Runtime::instance();
  if(!rt||!p||uint64_t(p)+n>0x100000000ull)return false;
  for(uint64_t a=p;a<uint64_t(p)+n;){
    auto* heap=rt->memory()->LookupHeap(uint32_t(a));uint32_t flags=0;
    if(!heap||!heap->QueryProtect(uint32_t(a),&flags)||!(flags&rex::memory::kMemoryProtectRead))return false;
    a=(a/heap->page_size()+1)*heap->page_size();
  }return true;
}
std::string Text(uint8_t* base,uint32_t p) {
  std::wstring wide;
  for(unsigned i=0;i<2048;++i){
    if(!Readable(p+i*2,2))return {};
    auto c=REX_LOAD_U16(p+i*2);if(!c)break;
    if(i==2047)return {};
    wide.push_back(c==13?L' ':wchar_t(c));
  }
  int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);
  if(size<=0)return {};
  std::string s(size,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),int(wide.size()),s.data(),size,nullptr,nullptr);return s;
}
}
void pdz_subtitles::Initialize(ImFontAtlas* atlas){
  subtitleFont=nullptr;
  char directory[MAX_PATH]{};
  if(GetWindowsDirectoryA(directory,MAX_PATH)){
    const auto path=std::string(directory)+"/Fonts/segoeui.ttf";
    // The renderer uses a pre-baked atlas. ImGui's default Latin range omits
    // curly quotes, dashes and ellipses used by the native subtitle tables.
    static const ImWchar ranges[]={0x0020,0x024F,0x2000,0x206F,0};
    subtitleFont=atlas->AddFontFromFileTTF(path.c_str(),32.f,nullptr,ranges);
  }
}
void pdz_subtitles::Tick(uint8_t* base,bool active) {
  std::lock_guard lock(mutex);cinematic=active;
  if(!active||!voice||!Readable(voice,344)) {caption.clear();voice=0;return;}
  auto state=REX_LOAD_U32(voice+340);
  if(state==6||state==8){caption.clear();voice=0;}
}
REX_EXTERN(__imp__sub_82269100);
REX_HOOK_RAW(sub_82269100) {
  const auto id=ctx.r3.u32,caller=uint32_t(ctx.lr);
  uint32_t sound=0;
  {std::lock_guard lock(mutex);
    if(cinematic){
      if(caller==0x82221DA8)sound=ctx.r29.u32;
      else if(caller==0x8262CA40&&Readable(ctx.r31.u32,8))sound=REX_LOAD_U32(ctx.r31.u32+4);
    }
  }
  if(sound&&id&&id<0x70000&&Readable(sound,344)){
    auto call=ctx;call.r1.u32-=512;call.r3.u64=id;
    sub_8239BCD8(call,base);
    auto text=Text(base,call.r3.u32);
    if(!text.empty()){
      std::lock_guard lock(mutex);caption=std::move(text);voice=sound;
      REXLOG_INFO("Cutscene subtitle: id={:08X} voice={:08X}",id,sound);
    }
  }
  __imp__sub_82269100(ctx,base);
}
void pdz_subtitles::Draw(){
  if(!REXCVAR_GET(pdz_cutscene_subtitles))return;
  std::string text;{std::lock_guard lock(mutex);text=caption;}
  if(text.empty())return;
  auto* vp=ImGui::GetMainViewport();auto* font=subtitleFont?subtitleFont:ImGui::GetFont();
  float size=std::max(18.f,vp->Size.y*.043f),width=vp->Size.x*.84f;
  if(!font->GetFontBaked(size))font=ImGui::GetFont();
  if(!font||!font->GetFontBaked(size))return;
  std::vector<std::string> lines;
  const char* p=text.c_str();const char* end=p+text.size();
  while(p<end){
    const char* next=font->CalcWordWrapPosition(size,p,end,width);
    if(next<=p)break;
    lines.emplace_back(p,next);
    p=next;while(p<end&&(*p==' '||*p=='\n'||*p=='\r'))++p;
  }
  auto* draw=ImGui::GetForegroundDrawList();
  float y=vp->Pos.y+vp->Size.y*.945f-lines.size()*size*1.15f;
  float outline=std::max(1.f,size*.045f);
  for(auto& line:lines){
    auto extent=font->CalcTextSizeA(size,100000.f,0,line.c_str());
    ImVec2 pos(vp->Pos.x+(vp->Size.x-extent.x)*.5f,y);
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(dx||dy)
      draw->AddText(font,size,{pos.x+dx*outline,pos.y+dy*outline},IM_COL32(0,0,0,230),line.c_str());
    draw->AddText(font,size,pos,IM_COL32(255,255,255,255),line.c_str());y+=size*1.15f;
  }
}

