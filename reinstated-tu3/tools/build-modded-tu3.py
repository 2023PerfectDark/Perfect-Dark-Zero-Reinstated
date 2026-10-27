"""Build the isolated modded TU3 port; never launch it or replace its installed executable with --next."""
from pathlib import Path
import concurrent.futures, hashlib, json, shlex, subprocess, shutil, sys
root=Path(__file__).resolve().parent.parent
project=root/'compatibility-v16/ModdedTU3'
stock=root/'compatibility-v16/StockTU3'
old=root/'compatibility-v16/PerfectDarkZeroRecomp/out/build/win-amd64-release'
out=project/'out';out.mkdir(exist_ok=True)
binary='pdzmoddedtu3-next' if '--next' in sys.argv else 'pdzmoddedtu3'
lines=(old/'build.ninja').read_text().splitlines()
includes=next(x.split(' = ',1)[1] for x in lines if x.startswith('  INCLUDES = '))
inc=shlex.split(includes)
inc=[x for x in inc if not x.startswith('-I'+(root/'compatibility-v16/PerfectDarkZeroRecomp').as_posix()) or '/out/' in x]
inc+=['-I'+project.as_posix(),'-I'+(project/'src').as_posix(),'-I'+stock.as_posix(),'-I'+(stock/'generated/default').as_posix()]
flags=['-O2','-DNDEBUG','-std=c++23','-D_DLL','-D_MT','-Xclang','--dependent-lib=msvcrt','-march=x86-64-v2','-msse4.1','-fasync-exceptions','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-DSPDLOG_COMPILED_LIB','-DSPDLOG_FMT_EXTERNAL','-DREX_HAS_VULKAN=1','-DREX_HAS_D3D12=1','-DREXGLUE_BUILD_CONFIG="Release"']
clang=str(root/'clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang++.exe')
headers=b''.join(p.read_bytes() for p in sorted((project/'src').rglob('*')) if p.suffix in {'.h','.inl'})
headers+=b"".join(p.read_bytes() for p in sorted((root/"compatibility-v16/sdk/include").rglob("*.h")))
header_hash=hashlib.sha256(headers).hexdigest()
sources=list((project/'src').rglob('*.cpp'))
sources+=list((project/'generated-patched').glob('*.cpp'))
def compile(src):
    obj=out/(src.name+'.obj');stamp=out/(src.name+'.sha256')
    digest=hashlib.sha256(src.read_bytes()+header_hash.encode()+repr(inc+flags).encode()).hexdigest()
    if obj.exists() and stamp.exists() and stamp.read_text()==digest:return obj
    result=subprocess.run([clang,*flags,*inc,'-c',str(src),'-o',str(obj)],capture_output=True,text=True)
    (out/(src.name+'.log')).write_text(result.stdout+result.stderr)
    if result.returncode:raise RuntimeError(src.name+' failed: '+result.stderr[:6000])
    stamp.write_text(digest);print('Compiled',src.name,flush=True);return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:objects=list(pool.map(compile,sources))
replaced={p.name for p in (project/'generated-patched').glob('*.cpp')}
for source in (stock/'generated/default').glob('*.cpp'):
    if source.name in replaced:continue
    obj=stock/'out'/(source.name+'.obj')
    assert obj.exists() and obj.stat().st_mtime>=source.stat().st_mtime, f'Stale stock object: {obj}'
    objects.append(obj)
objects.append(stock/'out/retail_interactive_music.cpp.obj')
link=next(x for x in lines if x.startswith('build perfectdarkzerorecomp.exe:')).split(' | ')[0]
for item in link.split()[3:]:
    if item.endswith('.obj') and ('rexglue-sdk/' in item or '/sdk/src/ui/' in item):
        p=old/item;assert p.exists(),p;objects.append(p)
resource=out/'PDZReinstatedTU3.res'
subprocess.run(['rc.exe','/nologo','/fo',str(resource),str(project/'src/perfectdarkzerorecomp.rc')],cwd=project/'src',check=True)
objects.append(resource)
index=next(i for i,x in enumerate(lines) if x.startswith('build perfectdarkzerorecomp.exe:'))
libs=next(x.split(' = ',1)[1] for x in lines[index:] if x.startswith('  LINK_LIBRARIES = '))
winhttp=Path('C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64/WinHttp.Lib')
assert winhttp.exists()
args=[*map(str,objects),*shlex.split(libs),str(winhttp),'-fuse-ld=lld-link','-Xlinker','/subsystem:windows','-Xlinker','/MAP:'+str(out/(binary+'.map')),'-o',str(out/(binary+'.exe'))]
rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args))
subprocess.run([clang,'@'+str(rsp)],check=True)
for name in ('rexruntime.dll','rexgpu-xenos.dll'):
    source=root/'compatibility-v16/sdk/out/win-amd64'/name;target=out/name
    if not target.exists() or hashlib.sha256(source.read_bytes()).digest()!=hashlib.sha256(target.read_bytes()).digest():shutil.copy2(source,target)
# RepoRoot resolves to ModdedTU3 (it has assets/), not the workspace root.
# Ship the voice assets beside that assets directory on every successful build.
voice_source=root/'mod_assets/death-voices'
voice_target=project/'mod_assets/death-voices'
voice_manifest=json.loads((voice_source/'manifest.json').read_text())
for item in voice_manifest:
    relative=Path(item['language'])/(item['clip']+'.wav')
    source=voice_source/relative;target=voice_target/relative
    assert source.is_file(),f'Missing death voice: {source}'
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or hashlib.sha256(source.read_bytes()).digest()!=hashlib.sha256(target.read_bytes()).digest():shutil.copy2(source,target)
shutil.copy2(voice_source/'manifest.json',voice_target/'manifest.json')
print(f'Staged {len(voice_manifest)} death-voice assets in runtime root',flush=True)
print('Modded TU3 compilation/link completed; runtime validation still required',flush=True)
