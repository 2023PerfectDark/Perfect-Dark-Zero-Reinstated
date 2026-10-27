"""Test production row reconciliation, including pre-existing stale highlights."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
s = (root/'compatibility-v16/ModdedTU3/src/native_pointer.cpp').read_text()
start = s.index('void SyncPlayerRowVisual(')
helper = s[start:s.index('\n}\n}', start)+2]
out = root/'staging/player-row-test'
out.mkdir(exist_ok=True)
test = r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <cstdio>
union Reg { uint64_t u64; uint32_t u32; int32_t s32; Reg():u64(0){} };
struct PPCContext {Reg r1,r3,r4;};
std::map<uint32_t,uint32_t> mem, visual, indices, enabled, drawn;
std::map<uint32_t,std::u16string> ids;
uint32_t focus=101; int calls=0; bool visible=true, fail=false;
bool Range(uint32_t p,uint32_t){return p!=0;}
bool Visible(uint32_t,uint8_t*){return visible;}
std::u16string Id(uint32_t p,uint8_t*){return ids[p];}
#define REX_LOAD_U32(p) mem[p]
#define REX_STORE_U32(p,v) (mem[p]=(v))
void sub_8277BE58(PPCContext&,uint8_t*){}
void sub_827897B0(PPCContext& c,uint8_t*){c.r3.u64=500;}
void sub_82789E48(PPCContext& c,uint8_t*){c.r3.u64=indices[c.r3.u32];}
void sub_82789AE8(PPCContext& c,uint8_t*){c.r3.u64=focus;}
void sub_8279ACD8(PPCContext& c,uint8_t*){c.r3.u64=enabled[c.r3.u32];}
void sub_8278DDB0(PPCContext& c,uint8_t*){mem[c.r4.u32]=visual[c.r3.u32];c.r3.u64=0;}
void sub_8278DF38(PPCContext& c,uint8_t*){++calls;if(!fail)drawn[c.r3.u32]=c.r4.u32;c.r3.s32=fail?-1:0;}
''' + helper + r'''
void Render(){for(uint32_t i=0;i<8;++i){PPCContext c;c.r1.u64=9000;SyncPlayerRowVisual(c,nullptr,100+i,1000+i*256);}}
void Check(unsigned selected){int count=0;for(auto [h,state]:drawn)count+=state==1;assert(count==1 && drawn[100+selected]==1);}
int main(){
 ids[3000]=u"listPlayers";ids[4000]=u"aid_darkxui_frontend_multi_gamelobby_owner";
 mem[3040]=4000;mem[3000]=50;mem[584]=1;mem[588]=16;
 for(uint32_t i=0;i<8;++i){ids[1000+i*256]=u"control_ListItem";mem[1040+i*256]=3000;
   visual[100+i]=200+i;indices[100+i]=i;enabled[100+i]=1;drawn[100+i]=1;}
 Render();Check(1);assert(calls==8);Render();assert(calls==8);
 focus=106;mem[584]=6;Render();Check(6);
 // Recreated visual and recycled row indices must not reuse a stale state.
 ++visual[103];drawn[103]=1;Render();Check(6);
 for(uint32_t i=0;i<8;++i)indices[100+i]=i+8;
 mem[584]=14;Render();Check(6);
 enabled[106]=0;Render();assert(drawn[106]==3);
 enabled[106]=1;fail=true;Render();assert(drawn[106]==3);
 fail=false;Render();Check(6);
 focus=0;Render();for(auto [h,state]:drawn)assert(state==0);
 ids[3000]=u"listPlayerInfo";ids[4000]=u"aid_darkxui_frontend_multi_playerlist";
 focus=102;mem[584]=10;Render();Check(2);
 int previous=calls;ids[4000]=u"unrelated_scene";focus=104;Render();assert(calls==previous);
 ids[4000]=u"aid_darkxui_frontend_multi_playerlist";visible=false;Render();assert(calls==previous);
 puts("Player row visuals passed: stale highlights, stationary frames, selection, recycled skins, scrolling, disabled rows, retry, focus loss, both screens and scope guards.");
}
'''
(out/'visual.cpp').write_text(test)
c=root/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
subprocess.run([str(c),'-std=c++20','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-c',str(out/'visual.cpp'),'-o',str(out/'visual.obj')],check=True)
subprocess.run([str(c),str(out/'visual.obj'),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt','-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(out/'visual.exe')],check=True)
subprocess.run([str(out/'visual.exe')],check=True)
