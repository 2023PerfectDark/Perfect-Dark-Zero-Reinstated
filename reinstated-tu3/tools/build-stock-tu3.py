"""Build isolated TU3 code against the already-built v1.6 SDK objects."""
from pathlib import Path
import concurrent.futures, re, shlex, subprocess, shutil, hashlib
root=Path(__file__).resolve().parent.parent
project=root/'compatibility-v16/StockTU3'
old=root/'compatibility-v16/PerfectDarkZeroRecomp/out/build/win-amd64-release'
out=project/'out';out.mkdir(exist_ok=True)
# TU3 mission-count routine: verified against its native session mode and
# registered mission count. Only expose campaign entries, never fake progress.
mission_source=project/'generated/default/pdzstocktu3_recomp.99.cpp'
mission_text=mission_source.read_text()
marker='DEFINE_REX_FUNC(sub_82402458) {\n\tREX_FUNC_PROLOGUE();'
patch='''
    // STOCK_TU3_UNLOCK_CAMPAIGN: solo/co-op only; preserve save records.
    const uint32_t session = REX_LOAD_U32(0x82D6BFD8);
    if (session >= 0x40000000u && session < 0x80000000u) {
        const uint32_t mode = REX_LOAD_U32(session + 40);
        const uint32_t count = REX_LOAD_U32(0x82D6C2C8);
        if (mode <= 1 && count >= 1 && count <= 14) {
            ctx.r3.u64 = count;
            return;
        }
    }
'''
if 'STOCK_TU3_UNLOCK_CAMPAIGN' not in mission_text:
    assert mission_text.count(marker)==1
    mission_source.write_text(mission_text.replace(marker,marker+patch))
ninja=(old/'build.ninja').read_text()
lines=ninja.splitlines()
includes=next(x.split(' = ',1)[1] for x in lines if x.startswith('  INCLUDES = '))
inc=shlex.split(includes)
inc=[x for x in inc if not x.startswith('-I'+(root/'compatibility-v16/PerfectDarkZeroRecomp').as_posix()) or '/out/' in x]
inc+=['-I'+project.as_posix(),'-I'+(project/'src').as_posix(),'-I'+(project/'generated/default').as_posix()]
flags=['-O2','-DNDEBUG','-std=c++23','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-march=x86-64-v2','-msse4.1','-fasync-exceptions','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-DSPDLOG_COMPILED_LIB','-DSPDLOG_FMT_EXTERNAL','-DREX_HAS_VULKAN=1','-DREX_HAS_D3D12=1','-DREXGLUE_BUILD_CONFIG="Release"']
clang=str(root/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe')
sources=list((project/'generated/default').glob('*.cpp'))+list((project/'src').glob('*.cpp'))
assert len(sources)>5, 'Code generation must finish first'
sdk_hash=hashlib.sha256(b"".join(p.read_bytes() for p in sorted((root/"compatibility-v16/sdk/include").rglob("*.h")))).hexdigest()

def compile(src):
    obj=out/(src.name+'.obj')
    stamp=out/(src.name+'.sdk-sha256')
    if obj.exists() and obj.stat().st_mtime>src.stat().st_mtime and stamp.exists() and stamp.read_text()==sdk_hash:return obj
    result=subprocess.run([clang,*flags,*inc,'-c',str(src),'-o',str(obj)],capture_output=True,text=True)
    (out/(src.name+'.log')).write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(src.name+' failed: '+result.stderr[:2000])
    stamp.write_text(sdk_hash)
    print('Compiled',src.name,flush=True)
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:objects=list(pool.map(compile,sources))
link=next(x for x in lines if x.startswith('build perfectdarkzerorecomp.exe:')).split(' | ')[0]
for item in link.split()[3:]:
    if item.endswith('.obj') and ('rexglue-sdk/' in item or '/sdk/src/ui/' in item):
        p=old/item
        assert p.exists(),p
        objects.append(p)
index=next(i for i,x in enumerate(lines) if x.startswith('build perfectdarkzerorecomp.exe:'))
libs=next(x.split(' = ',1)[1] for x in lines[index:] if x.startswith('  LINK_LIBRARIES = '))
args=[*map(str,objects),*shlex.split(libs),'-fuse-ld=lld-link','-Xlinker','/subsystem:windows','-o',str(out/'pdzstocktu3.exe')]
rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args))
subprocess.run([clang,'@'+str(rsp)],check=True)
for name in ('rexruntime.dll','rexgpu-xenos.dll'):
    shutil.copy2(root/'compatibility-v16/sdk/out/win-amd64'/name,out/name)
print('Stock TU3 build completed',flush=True)
