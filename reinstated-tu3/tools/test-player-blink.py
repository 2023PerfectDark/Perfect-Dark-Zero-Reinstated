from pathlib import Path
import subprocess
r=Path.cwd();p=r/'compatibility-v16/ModdedTU3';o=p/'out/tests/v113';o.mkdir(parents=True,exist_ok=True)
c=r/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
subprocess.run([str(c),'-std=c++23','-O1','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt',str(p/'tests/player_blink_test.cpp'),'-c','-o',str(o/'blink.obj')],check=True)
subprocess.run([str(c),str(o/'blink.obj'),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt','-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(o/'blink.exe')],check=True)
subprocess.run([str(o/'blink.exe')],check=True)
a=(r/'compatibility-v16/StockTU3/generated/default/pdzstocktu3_recomp.115.cpp').read_text();b=(p/'generated-patched/pdzstocktu3_recomp.115.cpp').read_text()
start='DEFINE_REX_FUNC(sub_8269F7B0)'
assert a.split(start)[0]==b.removeprefix('#include "player_blink.h"\n').split(start)[0]
assert a.split(start)[1].split('\nDEFINE_REX_FUNC(',1)[1]==b.split(start)[1].split('\nDEFINE_REX_FUNC(',1)[1]
f=b.split(start)[1].split('\nDEFINE_REX_FUNC(',1)[0]
assert '// lwz r11,232(r11)' in f and '// lwz r11,64(r31)' in f
assert 'REX_STORE_U32(ctx.r31' not in f
print('Native dead/hidden actor checks retained; no actor writes; all other functions unchanged.')
