"""Regression for the v111 draw-palette lifetime and material-table repair."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parent.parent
p=r/'compatibility-v16/ModdedTU3'
out=p/'out/tests/v111';out.mkdir(parents=True,exist_ok=True)
for tag,path in [('old',p/'tests/fixtures/gore_palette_pre_v111.inc'),('new',p/'src/campaign_gore.cpp')]:
    s=path.read_text();start=s.index('struct HeadPalette {');end=s.index('\n};',start)+3
    # Relocate the model pool constant into the harness's checked local memory.
    (out/(tag+'_palette.inc')).write_text(s[start:end].replace('const auto definition=headless_bodies[slot];','const auto definition=headless_bodies[slot].load();').replace('0x82E75160u','0x1000u').replace('HeadPalette',tag+'_HeadPalette'))
compiler=r/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
for name in ('gore_palette_scope_test','model_node_tables_test'):
    obj=out/(name+'.obj');exe=out/(name+'.exe')
    subprocess.run([str(compiler),'-std=c++23','-O1','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt',
        '-I'+str(out),str(p/'tests'/(name+'.cpp')),'-c','-o',str(obj)],check=True)
    subprocess.run([str(compiler),str(obj),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt',
        '-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt',
        '-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
