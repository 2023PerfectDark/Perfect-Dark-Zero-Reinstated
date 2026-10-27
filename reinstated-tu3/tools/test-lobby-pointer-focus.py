"""Exercise production lobby hover dispatch against native focus/selection contracts."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'compatibility-v16/ModdedTU3/src/native_pointer.cpp').read_text()
start = source.index('    if (lobby_player_row) {', source.index('  if (list_item) {', source.index('trace.stage=7;')))
end = source.index('    call.r3.u64 = list_handle;', source.index('      return;\n    }', start))
branch = source[start:end]
out = root / 'staging/lobby-pointer-test'
out.mkdir(exist_ok=True)
test = r'''
#include <cassert>
#include <cstdint>
#include <vector>
#include <cstdio>
union Reg { uint64_t u64; uint32_t u32; int32_t s32; Reg():u64(0){} };
struct Context { Reg r3,r4; };
static int selected, active, accepts, focus_result, selection_result;
static bool notify;
static std::vector<int> events;
static bool highlight[8];
#define REX_LOAD_U32(address) uint32_t(selected)
void sub_8278F328(Context& c, uint8_t*) {
  events.push_back(1);
  if (focus_result >= 0) {
    highlight[active]=false; active=int(c.r3.u32); highlight[active]=true;
    if(notify) selected=active;
  }
  c.r3.s32=focus_result;
}
void sub_8279AA00(Context& c, uint8_t*) {
  events.push_back(2);
  if(selection_result>=0) selected=int(c.r4.u32);
  c.r3.s32=selection_result;
}
namespace pdz_input { void PointerAccept(bool click) { accepts += click ? 10 : 1; } }
void Hover(int row, bool click) {
  Context call; uint8_t* base=nullptr;
  const bool lobby_player_row=true;
  const uint32_t focused_handle=active, handle=row, focus_user=255;
  const uint32_t list_handle=20, list_object=30;
  const int32_t list_index=row;
  struct {bool click;} pointer{click};
''' + branch + r'''
}
void Reset(){ selected=active=0; accepts=0; focus_result=selection_result=0;
  notify=true; events.clear(); for(auto& h:highlight)h=false; highlight[0]=true; }
int main(){
  Reset();
  // Repeated forward/backward hover clears the previous row; same-row motion
  // does not restart its focus animation or send a second selection event.
  for(int row : {1,2,7,3,0,4}) {
    events.clear(); Hover(row,false); assert(selected==row && active==row);
    int count=0; for(bool h:highlight)count+=h; assert(count==1);
    assert(events==std::vector<int>{1});
    events.clear(); Hover(row,false); assert(events.empty());
  }
  Hover(6,true); assert(selected==6 && active==6 && accepts==22);
  Reset(); notify=false; Hover(3,true);
  assert((events==std::vector<int>{1,2}) && selected==3 && accepts==10);
  Reset(); focus_result=-1; Hover(3,true);
  assert(selected==0 && active==0 && accepts==0 && events==std::vector<int>{1});
  Reset(); notify=false; selection_result=-1; Hover(3,true); assert(accepts==0);
  puts("Lobby hover dispatch passed: single focus, same-row motion, click target, fallback and failure handling.");
}
'''
(out / 'focus.cpp').write_text(test)
compiler = root / 'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
subprocess.run([str(compiler), '-std=c++20', '-D_DLL', '-D_MT', '-Xclang', '--dependent-lib=msvcrt', '-c', str(out/'focus.cpp'), '-o', str(out/'focus.obj')], check=True)
subprocess.run([str(compiler), str(out/'focus.obj'), '-fuse-ld=lld-link', '-Xlinker', '/NODEFAULTLIB:libcmt', '-Xlinker', '/DEFAULTLIB:msvcrt', '-Xlinker', '/DEFAULTLIB:msvcprt', '-Xlinker', '/DEFAULTLIB:ucrt', '-Xlinker', '/DEFAULTLIB:vcruntime', '-o', str(out/'focus.exe')], check=True)
subprocess.run([str(out/'focus.exe')], check=True)
