"""Targeted host regressions for the holster/animation crash repair."""
from pathlib import Path
import re, subprocess
r=Path(__file__).resolve().parent.parent
p=r/'compatibility-v16/ModdedTU3'
out=p/'out/tests/v110';out.mkdir(parents=True,exist_ok=True)
s=(r/'compatibility-v16/StockTU3/generated/default/pdzstocktu3_recomp.187.cpp').read_text()
start=s.index('DEFINE_REX_FUNC(sub_8271F7B8) {')
old=s[start:s.index('\nDEFINE_REX_FUNC(',start+1)].strip()
new=(p/'src/animation_channel_guard.cpp').read_text()
new=new[new.index('REX_HOOK_RAW(sub_8271F7B8) {'):].strip()
# Ensure the test exercises the actual production sampler and its only change
# is the added null-channel branch, not an accidentally altered native routine.
unpatched=re.sub(r'\t// A missing primary channel.*?\n\t}\n','',new,flags=re.S)
assert unpatched.replace('REX_HOOK_RAW','DEFINE_REX_FUNC',1)==old
(out/'animation_channel_old.inc').write_text(old.replace('sub_8271F7B8','old_sampler',1))
(out/'animation_channel_new.inc').write_text(new.replace('sub_8271F7B8','new_sampler',1))
compiler=r/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
for name in ('animation_channel_harness','holster_stability_test'):
    exe=out/(name+'.exe')
    obj=out/(name+'.obj')
    subprocess.run([str(compiler),'-std=c++23','-O1','-D_DLL','-D_MT',
        '-Xclang','--dependent-lib=msvcrt','-I'+str(out),str(p/'tests'/(name+'.cpp')),
        '-c','-o',str(obj)],check=True)
    subprocess.run([str(compiler),str(obj),
        '-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt',
        '-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt',
        '-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
