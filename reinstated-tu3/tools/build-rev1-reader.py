"""Build the installer-only Rev 1 image reader against the shipped SDK."""
from pathlib import Path
import shlex, subprocess, shutil
root=Path(__file__).resolve().parent.parent
sdk=root/'compatibility-v16/sdk/out/win-amd64'
lines=(root/'compatibility-v16/PerfectDarkZeroRecomp/out/build/win-amd64-release/build.ninja').read_text().splitlines()
inc=shlex.split(next(x.split(' = ',1)[1] for x in lines if x.startswith('  INCLUDES = ')))
compiler=root/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe'
obj=root/'staging/rev1-image.obj'
flags=['-O2','-std=c++23','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-DSPDLOG_COMPILED_LIB','-DSPDLOG_FMT_EXTERNAL']
subprocess.run([str(compiler),*flags,*inc,'-c',str(root/'installer/rev1-image.cpp'),'-o',str(obj)],check=True)
subprocess.run([str(compiler),str(obj),str(sdk/'rexruntime.lib'),str(sdk/'fmt.lib'),str(sdk/'spdlog.lib'),'-fuse-ld=lld-link','-o',str(root/'installer/tools/rev1-image.exe')],check=True)
shutil.copy2(sdk/'rexruntime.dll',root/'installer/tools/rexruntime.dll')
