"""Prepare the v121 installer metadata and the separately checksummed TU3 update."""
from pathlib import Path
import json, hashlib, shutil, zipfile
root=Path(__file__).resolve().parents[1]; installer=root/'installer'
manifest=json.loads((installer/'payload.json').read_text())
updater=installer/'payload/common/mod_assets/updater'
target=root/'releases/PDZ-Reinstated-TU3-Update.zip'
required={'PDZReinstatedTU3.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt',
          'assets/pdz/ASSETS/PackageData/832751055.pkg',
          'assets/pdz/aid_darkcharacter_guns_klobb_fp_klobb/default.dbm'}
rows=[r for r in manifest['files'] if r['group'] in ('common','tu3') and
      (r['destination'] in required or r['destination'].startswith('mod_assets/')) and
      r['destination']!='mod_assets/updater/source.json']
assert len({r['destination'].lower() for r in rows})==len(rows)
hashes={}
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for row in rows:
        p=installer/row['source']; data=p.read_bytes()
        hashes[row['destination']]=hashlib.sha256(data).hexdigest()
        z.writestr('PDZ-Recomp/'+row['destination'], data)
    z.writestr('PDZ-Recomp/SHA256SUMS.json',json.dumps(hashes,indent=2))
with zipfile.ZipFile(target) as z: assert z.testzip() is None
digest=hashlib.sha256(target.read_bytes()).hexdigest()
config=json.loads((updater/'source.json').read_text())
config.update(installedTag='PDZInstaller',installedAssetSha256=digest)
(updater/'source.json').write_text(json.dumps(config,indent=2)+'\n')
for name in ('source.json','update.ps1'):
    p=updater/name; dest='mod_assets/updater/'+name
    rows_for_file=[r for r in manifest['files'] if r['destination']==dest]
    if rows_for_file: row=rows_for_file[0]
    else:
        row={'source':'payload/common/'+dest,'destination':dest,'group':'common'}
        manifest['files'].append(row)
    row.update(size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
    shutil.copy2(p,root/'compatibility-v16/ModdedTU3/mod_assets/updater'/name)
manifest['version']='Perfect Dark Zero: Reinstated v121 Release Candidate - 2026-10-26 (TU3 base 1.8; game v120)'
(installer/'payload.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name in ('README.txt','VALIDATION.txt'):
    p=installer/name; text=p.read_text().replace('Release candidate v120 -','Release candidate v121 -')
    note=('UPDATER AND PACKAGING v121\n'
          'The installer now includes its update source configuration and targets\n'
          '2023PerfectDark/Perfect-Dark-Zero-Reinstated. Only compatible TU3 update\n'
          'packages are accepted. Older retail packages cannot replace this build.\n'
          'Updated assets on the installer release are identified by their SHA256\n'
          'digest and upload time, even when the release tag stays the same.\n'
          'The game executable is unchanged from v120; gameplay limits below remain.\n\n')
    if 'UPDATER AND PACKAGING v121' not in text:
        text=text.replace('TU3 base 1.8; game build v120\n\n','TU3 base 1.8; game build v120\n\n'+note)
    p.write_text(text)
report={'update':str(target),'files':len(hashes),'bytes':target.stat().st_size,'sha256':digest}
(root/'staging/v121-update-package.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
