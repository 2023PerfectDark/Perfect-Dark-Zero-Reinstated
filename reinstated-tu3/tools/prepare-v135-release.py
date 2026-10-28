from pathlib import Path
import hashlib,json,shutil,zipfile
root=Path(__file__).resolve().parents[1]
inst=root/'installer'
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
notes='''Perfect Dark Zero: Reinstated 1.8 - v135
October 28, 2026

PD menu: sliding tabs, smooth size/width sliders, and responsive right-hand hover help. Removed empty preview space. Output resolution offers 240p through 4K and applies without a restart. VSync, render target mode, native 2x anti-aliasing and resolution scale support live changes. The menu/live-setting changes were user-confirmed working; immediate refresh of already loaded localized text remains unverified.

New installs default to VSync off, aspect-ratio preservation off, menu/scripted-camera cap off and Play while shaders compile on. Existing settings are preserved.

Mouse/keyboard defaults now match the creator's saved layout, including Reset to defaults. Binding editor reports successful saves or the exact file error, offers Save bindings now, and protects the previous file if writing fails. Reports of other users' bindings not applying/saving are not yet reproduced; this is not a confirmed fix for every report.

Backpack reload: unlocked toggle in Cheats / Gameplay assists, off by default. Stowed weapons reload from reserves. Ammo policy tests passed; weapon switching still needs an in-game check.

US Rev 1 support remains included: separate DLC setup is skipped and external TU3 is blocked. Original US ISO support, Surface, optional completed-playthrough shader cache, mouse dialog controls and single-row lobby highlights remain included.

Known limits: random bot head/body remains unresolved. Rev 1 and runtime changes have not been exhaustively tested. No guarantee of zero crashes or shader stutter.
'''
for p in [inst/'payload/common/mod_assets/updater/changelog.txt',root/'compatibility-v16/ModdedTU3/mod_assets/updater/changelog.txt']:
    p.write_text(notes,encoding='utf-8')
for name in ['README.txt','VALIDATION.txt']:
    p=inst/name
    p.write_text(notes+'\nPrevious release documentation:\n\n'+p.read_text(encoding='utf-8-sig'),encoding='utf-8')
manifest=json.loads((inst/'payload.json').read_text())
for row in manifest['files']:
    if row['group']=='tu3' and row['destination'] in ['PDZReinstatedTU3.exe','rexruntime.dll','rexgpu-xenos.dll']:
        name='pdzmoddedtu3-next.exe' if row['destination'].endswith('.exe') else row['destination']
        shutil.copy2(root/'compatibility-v16/ModdedTU3/out'/name,inst/row['source'])
    if row['group']=='tu3' or row['destination']=='mod_assets/updater/changelog.txt':
        p=inst/row['source'];row.update(size=p.stat().st_size,sha256=digest(p))
manifest['version']='Perfect Dark Zero: Reinstated v135 - 2026-10-28 (TU3 base 1.8)'
target=root/'releases/PDZ-Reinstated-TU3-Update.zip'
backup=root/'releases/PDZ-Reinstated-TU3-Update-pre-v135.zip'
if not backup.exists():shutil.copy2(target,backup)
required={'PDZReinstatedTU3.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt','assets/pdz/ASSETS/PackageData/832751055.pkg','assets/pdz/aid_darkcharacter_guns_klobb_fp_klobb/default.dbm'}
rows=[r for r in manifest['files'] if r['group'] in ('common','tu3') and (r['destination'] in required or r['destination'].startswith('mod_assets/')) and r['destination']!='mod_assets/updater/source.json']
assert len({r['destination'].lower() for r in rows})==len(rows)
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    hashes={}
    for row in rows:
        p=inst/row['source'];assert digest(p)==row['sha256']
        hashes[row['destination']]=row['sha256'];z.write(p,'PDZ-Recomp/'+row['destination'])
    z.writestr('PDZ-Recomp/SHA256SUMS.json',json.dumps(hashes,indent=2))
with zipfile.ZipFile(target) as z:assert z.testzip() is None
p=inst/'payload/common/mod_assets/updater/source.json'
config=json.loads(p.read_text());config.update(installedTag='PDZInstaller',installedAssetSha256=digest(target));p.write_text(json.dumps(config,indent=2)+'\n')
row=next(r for r in manifest['files'] if r['destination']=='mod_assets/updater/source.json');row.update(size=p.stat().st_size,sha256=digest(p))
shutil.copy2(p,root/'compatibility-v16/ModdedTU3/mod_assets/updater/source.json')
(inst/'payload.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('v135 payload and runtime ZIP verified',flush=True)
