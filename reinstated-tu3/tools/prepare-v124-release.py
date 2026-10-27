"""Promote the user-verified player-list highlight build to installer/update payloads."""
from pathlib import Path
import hashlib, json, shutil, zipfile

root=Path(__file__).resolve().parents[1]
installer=root/'installer'
def digest(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
source=root/'compatibility-v16/ModdedTU3/out/pdzmoddedtu3-next.exe'
assert digest(source)=='9c945738806780e50376ae5db85816b607436f7d79b735e7ffa72a528e705ea1'
manifest=json.loads((installer/'payload.json').read_text())
row=next(r for r in manifest['files'] if r['group']=='tu3' and r['destination']=='PDZReinstatedTU3.exe')
shutil.copy2(source,installer/row['source'])
row.update(size=source.stat().st_size,sha256=digest(source))
manifest['version']='Perfect Dark Zero: Reinstated v124 Release - 2026-10-27 (TU3 base 1.8; game v124)'
target=root/'releases/PDZ-Reinstated-TU3-Update.zip'
backup=root/'releases/PDZ-Reinstated-TU3-Update-v121-backup.zip'
if not backup.exists():shutil.copy2(target,backup)
required={'PDZReinstatedTU3.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt',
          'assets/pdz/ASSETS/PackageData/832751055.pkg',
          'assets/pdz/aid_darkcharacter_guns_klobb_fp_klobb/default.dbm'}
rows=[r for r in manifest['files'] if r['group'] in ('common','tu3') and
      (r['destination'] in required or r['destination'].startswith('mod_assets/')) and
      r['destination']!='mod_assets/updater/source.json']
assert len({r['destination'].lower() for r in rows})==len(rows)
hashes={}
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for r in rows:
        p=installer/r['source'];assert digest(p)==r['sha256']
        hashes[r['destination']]=r['sha256'];z.write(p,'PDZ-Recomp/'+r['destination'])
    z.writestr('PDZ-Recomp/SHA256SUMS.json',json.dumps(hashes,indent=2))
with zipfile.ZipFile(target) as z:assert z.testzip() is None
updater=installer/'payload/common/mod_assets/updater/source.json'
config=json.loads(updater.read_text());config.update(installedTag='PDZInstaller',installedAssetSha256=digest(target))
updater.write_text(json.dumps(config,indent=2)+'\n')
r=next(r for r in manifest['files'] if r['destination']=='mod_assets/updater/source.json')
r.update(size=updater.stat().st_size,sha256=digest(updater))
shutil.copy2(updater,root/'compatibility-v16/ModdedTU3/mod_assets/updater/source.json')
(installer/'payload.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'sha256':digest(target),'files':len(hashes)},indent=2))
