"""Run the real PowerShell helper against isolated update-source fixtures."""
from pathlib import Path
import json, subprocess, tempfile, hashlib

root = Path(__file__).resolve().parents[1]
helper = root / 'installer/payload/common/mod_assets/updater/update.ps1'
config = json.loads(helper.with_name('source.json').read_text())

def release(tag, asset, date='2026-10-27T00:00:00Z', **flags):
    return dict(tag_name=tag, published_at=date, draft=False, prerelease=False,
                assets=[dict(name=asset, state='uploaded', digest='sha256:'+'a'*64,
                browser_download_url='https://github.com/'+config['repository']+'/releases/download/'+tag+'/'+asset)], **flags)

cases = [
    ('missing-settings', None, True, [], 'unconfigured'),
    ('missing-tu3', config, False, [], 'unconfigured'),
    ('no-releases', config, True, [], 'current'),
    ('old-retail-only', config, True, [release('old', 'PDZ-Recomp-Modded-Update.zip')], 'current'),
    ('new-tu3', config, True, [release('new', config['asset'])], 'available'),
    ('older-build', config, True, [release(config['installedTag'], config['asset'], '2026-10-25T00:00:00Z')], 'current'),
]
draft = release('draft', config['asset']); draft['draft'] = True
pre = release('pre', config['asset']); pre['prerelease'] = True
cases.append(('unpublished-tu3', config, True, [draft, pre], 'current'))
same_hash=dict(config,installedAssetSha256='a'*64)
cases.append(('same-digest',same_hash,True,[release('new',config['asset'])],'current'))
in_place=release(config['installedTag'],config['asset'],'2026-10-01T00:00:00Z')
in_place['assets'][0]['updated_at']='2026-10-27T00:00:00Z'
cases.append(('updated-existing-release',config,True,[in_place],'available'))
for name, settings, binary, releases, expected in cases:
    with tempfile.TemporaryDirectory(prefix='pdz-updater-', dir=root/'staging') as temp:
        path = Path(temp)
        if settings:
            dest=path/'mod_assets/updater/source.json'; dest.parent.mkdir(parents=True)
            dest.write_text(json.dumps(settings))
        if binary: (path/'PDZReinstatedTU3.exe').write_bytes(b'test')
        (path/'releases.json').write_text(json.dumps(releases))
        quote=lambda p: "'"+str(p).replace("'", "''")+"'"
        wrapper=path/'test.ps1'
        wrapper.write_text("function Invoke-RestMethod { param($Uri,$Headers,$TimeoutSec) Get-Content -Raw "+quote(path/'releases.json')+" | ConvertFrom-Json }\n& "+quote(helper)+" -Action Check -Root "+quote(path))
        result=subprocess.run(['powershell','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(wrapper)],capture_output=True,text=True)
        status=(path/'.pdz-update/status.txt').read_text(encoding='utf-8-sig').splitlines()
        assert result.returncode == 0 and status[0] == expected, (name,result.stderr,status)
        print(name+': '+status[0])
for invalid in (False,True):
    with tempfile.TemporaryDirectory(prefix='pdz-install-',dir=root/'staging') as temp:
        path=Path(temp); (path/'mod_assets/updater').mkdir(parents=True)
        (path/'mod_assets/updater/source.json').write_text(json.dumps(config))
        (path/'PDZReinstatedTU3.exe').write_bytes(b'old exe')
        (path/'rexruntime.dll').write_bytes(b'old dll')
        (path/'settings').mkdir(); (path/'settings/hardware.toml').write_text('keep-me')
        staged=path/'.pdz-update/staged';staged.mkdir(parents=True)
        files=[]
        for name in ('PDZReinstatedTU3.exe','rexruntime.dll','rexgpu-xenos.dll'):
            data=('new '+name).encode(); (staged/name).write_bytes(data)
            files.append(dict(name=name,sha256=hashlib.sha256(data).hexdigest()))
        if invalid: files[1]['sha256']='0'*64
        (path/'.pdz-update/plan.json').write_text(json.dumps(dict(tag='test',published='2026-10-27T00:00:00Z',digest='b'*64,files=files)))
        quote=lambda p: "'"+str(p).replace("'","''")+"'"
        wrapper=path/'test.ps1'
        wrapper.write_text("function Start-Process { param($FilePath,$WorkingDirectory,$WindowStyle) }\n& "+quote(helper)+" -Action Install -GameProcessId 2147483647 -Root "+quote(path)+"\nexit $LASTEXITCODE")
        result=subprocess.run(['powershell','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(wrapper)],capture_output=True,text=True)
        status=(path/'.pdz-update/status.txt').read_text(encoding='utf-8-sig').splitlines()
        assert status[0] == ('error' if invalid else 'installed'), (status,result.stderr)
        assert (path/'PDZReinstatedTU3.exe').read_bytes()==(b'old exe' if invalid else b'new PDZReinstatedTU3.exe')
        assert (path/'settings/hardware.toml').read_text()=='keep-me'
        assert result.returncode==(1 if invalid else 0)
        print(('rollback' if invalid else 'install')+': passed')
print('All 11 updater checks passed.')
