from pathlib import Path
import hashlib,json,shutil
root=Path(__file__).resolve().parent
repo=root.parent
old=repo/'releases/PDZ-Recomp-v1.6-Modded-Update-2026-10-18/PDZ-Recomp'
new=repo/'compatibility-v16/ModdedTU3'
files=[]
for name in ('terrain.pdzsurf','mission-music.wav','cabin-floor.rgba','cabin-wall.rgba','klobb-body.rgba'):
    required=new/'mod_assets/surface'/name
    if not required.is_file():
        raise RuntimeError('Surface is required in the installer payload: '+str(required))
def add(src,dest,group):
    assert src.is_file(),src
    out=root/'payload'/group/dest
    out.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(src,out)
    if dest == 'mod_assets/updater/changelog.txt':
        text = out.read_text(encoding='utf-8-sig')
        out.write_text(text.replace('Perfect Dark Zero Reinstated', 'Perfect Dark Zero: Reinstated'), encoding='utf-8')
    files.append(dict(source=out.relative_to(root).as_posix(),destination=dest,group=group,size=out.stat().st_size,sha256=hashlib.file_digest(out.open('rb'),'sha256').hexdigest()))
for name in ('perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll'):
    add(old/name,name,'retail')
for name in ('pdzmoddedtu3.exe','rexruntime.dll','rexgpu-xenos.dll'):
    add(new/'out'/name,'PDZReinstatedTU3.exe' if name=='pdzmoddedtu3.exe' else name,'tu3')
for base in ('assets','mod_assets','licenses'):
    for src in sorted((old/base).rglob('*')):
        if not src.is_file():continue
        rel=src.relative_to(old).as_posix()
        if base=='mod_assets':continue
        # Only the existing mod release's asset allowlist, never a full game disc.
        newer=new/rel
        add(newer if newer.is_file() else src,rel,'common')
for src in sorted((new/'mod_assets').rglob('*')):
    if src.is_file():
        rel=src.relative_to(new).as_posix()
        if rel.endswith('/CHANGELOG-INSTRUCTIONS.txt'):continue
        if rel.startswith('mod_assets/licenses/'):rel=rel.replace('mod_assets/licenses/','licenses/mod/',1)
        add(src,rel,'common')
for name in ('gamecontrollerdb.txt','LICENSE','LICENSE.mousehook'):
    add(old/name,name,'common')
add(root/'PDZ Bonus DVD.exe','PDZ Bonus DVD.exe','bonus')
vlc=root/'tools/vlc-portable/vlc-3.0.24'
for src in sorted(vlc.rglob('*')):
    if src.is_file():add(src,'bonus/vlc/'+src.relative_to(vlc).as_posix(),'bonus')
(root/'payload.json').write_text(json.dumps(dict(version='Perfect Dark Zero: Reinstated installer 2026-10-19',files=files),indent=2))
for group in ('common','retail','tu3'):
    print(group,len([f for f in files if f['group']==group]),sum(f['size'] for f in files if f['group']==group))
