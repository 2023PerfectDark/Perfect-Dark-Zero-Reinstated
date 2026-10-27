"""Exercise the production reload gate and verify its native call-site contract."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parent.parent
p=r/'compatibility-v16/ModdedTU3'
out=p/'out/tests/v112';out.mkdir(parents=True,exist_ok=True)
source=(p/'src/weapon_rules.cpp').read_text()
hook=source.split('REX_HOOK_RAW(sub_825556E0) {',1)[1].split('\n}\n',1)[0]
(out/'manual_reload_hook.inc').write_text('REX_HOOK_RAW(sub_825556E0) {'+hook+'\n}\n')
native=(r/'compatibility-v16/StockTU3/generated/default/pdzstocktu3_recomp.119.cpp').read_text()
automatic=native.split('loc_8223A424:',1)[1].split('loc_8223A4D4:',1)[0]
assert '// lhz r10,1316(r11)' in automatic # Magazine query.
assert '// lhz r11,1318(r11)' in automatic # Reserve fallback.
assert 'ctx.lr = 0x8223A4A8;\n\tsub_825556E0(ctx, base);' in automatic
assert automatic.index('sub_825556E0(ctx, base);')<automatic.index('// lwz r11,52(r11)')
manual=(r/'compatibility-v16/StockTU3/generated/default/pdzstocktu3_recomp.143.cpp').read_text().split('DEFINE_REX_FUNC(sub_82586D80)',1)[1].split('DEFINE_REX_FUNC',1)[0]
assert '// lwz r11,52(r11)' in manual and '825556E0' not in manual
compiler=r/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
obj=out/'manual_reload.obj';exe=out/'manual_reload.exe'
subprocess.run([str(compiler),'-std=c++23','-O1','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-I'+str(out),str(p/'tests/manual_reload_test.cpp'),'-c','-o',str(obj)],check=True)
subprocess.run([str(compiler),str(obj),'-fuse-ld=lld-link','-Xlinker','/NODEFAULTLIB:libcmt','-Xlinker','/DEFAULTLIB:msvcrt','-Xlinker','/DEFAULTLIB:msvcprt','-Xlinker','/DEFAULTLIB:ucrt','-Xlinker','/DEFAULTLIB:vcruntime','-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('Native empty-magazine gate and separate manual request verified; in-game validation pending.')
