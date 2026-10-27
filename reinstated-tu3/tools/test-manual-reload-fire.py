from pathlib import Path
r=Path.cwd();p=r/'compatibility-v16/ModdedTU3';o=p/'out/tests/v114';o.mkdir(parents=True,exist_ok=True)
s=(p/'src/manual_reload_fire.cpp').read_text();s='\n'.join(x for x in s.splitlines() if not x.startswith('#include'))
h='''#include <cstdint>
#include <cassert>
#include <unordered_map>
#include <cstdio>
static bool supported=true, enabled=true;
namespace pdz_input { bool Supported(){return supported;} }
#define REXCVAR_GET(x) enabled
static std::unordered_map<uint32_t,uint32_t> mem;
#define REX_LOAD_U32(x) mem.at(x)
'''
h+=s+'''
int main(){
 mem={{0x101c,0x2000},{0x82E36228,0x3000},{0x205c,0x4000},{0x33c4,0x4000}};
 for(int e=0;e<2;++e) for(int s=0;s<2;++s) for(int local=0;local<2;++local){
 enabled=e;supported=s;mem[0x205c]=local?0x4000:0x5000;
 const auto before=mem;
 assert(pdz_block_empty_fire_reload(nullptr,0x1000)==bool(e&&s&&local));
 assert(mem==before);
 }
 puts("Production Fire reload gate: option off/on, supported image, local/NPC and unchanged memory passed.");
}
'''
(o/'fire.cpp').write_text(h)
import subprocess
c=r/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
subprocess.run([str(c),'-std=c++23','-O1','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt',str(o/'fire.cpp'),'-c','-o',str(o/'fire.obj')],check=True)
subprocess.run([str(c),str(o/'fire.obj'),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt','-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(o/'fire.exe')],check=True)
subprocess.run([str(o/'fire.exe')],check=True)
f=(p/'generated-patched/pdzstocktu3_recomp.86.cpp').read_text()
assert f.count('if (pdz_block_empty_fire_reload')==1
assert 'loc_8223B300:\n\tif (pdz_block_empty_fire_reload(base, ctx.r31.u32)) goto loc_8223B480;' in f
assert 'case 10:\n\t\tgoto loc_8223B368;' in f
assert 'pdz_tp_enable' not in (p/'src/player_blink.cpp').read_text()
print('Fire gate isolated from explicit/ongoing reload; cover blinking no longer requires custom camera toggle.')
