"""Exercise the real PowerShell installer in isolated fixtures; never launch a game."""
import hashlib, json, pathlib, subprocess, tempfile

script = pathlib.Path(__file__).resolve().parents[1] / 'mod_assets/updater/update.ps1'
def run(root, action, release=None):
    quoted = lambda p: "'" + str(p).replace("'", "''") + "'"
    command = "function Start-Process { param($FilePath,$WorkingDirectory) }; & " + quoted(script)
    command += ' -Action ' + action + ' -Root ' + quoted(root) + ' -GameProcessId 2147483647'
    if release is not None:
        command = 'function Invoke-RestMethod { '+quoted(json.dumps(release))+' | ConvertFrom-Json }; '+command
    return subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-Command',command],capture_output=True,text=True)

with tempfile.TemporaryDirectory(prefix='pdz-update-test-') as temp:
    root = pathlib.Path(temp)
    source = root/'mod_assets/updater/source.json'; source.parent.mkdir(parents=True)
    source.write_text(json.dumps(dict(repository='',asset='test.zip',installedTag='',installedPublishedAt='2026-01-01T00:00:00Z')))
    assert run(root,'Check').returncode == 0
    assert (root/'.pdz-update/status.txt').read_text(encoding='utf-8-sig').startswith('unconfigured')
    work=root/'.pdz-update'; staged=work/'staged'; staged.mkdir()
    files=[]
    for name in ['perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll',
                 'out/surface/terrain.pdzsurf','out/surface/mission-music.wav',
                 'mod_assets/surface/mission-placeholder.png']:
        (root/name).parent.mkdir(parents=True,exist_ok=True)
        (staged/name).parent.mkdir(parents=True,exist_ok=True)
        (root/name).write_bytes(b'original '+name.encode())
        content=b'updated '+name.encode(); (staged/name).write_bytes(content)
        files.append(dict(name=name,sha256=hashlib.sha256(content).hexdigest()))
    preserved={
        'settings/hardware.toml':b'graphics = "custom"\r\n',
        'settings/mapping.toml':b'controls = "custom"\r\n',
        'settings/custom.toml':b'custom = true\n',
        'saves/profile':b'personal save',
        'profiles/player.json':b'{"name":"local player"}',
    }
    for name,data in preserved.items():
        (root/name).parent.mkdir(parents=True,exist_ok=True)
        (root/name).write_bytes(data)
    plan=dict(tag='test-update',published='2026-10-08T00:00:00Z',files=files)
    # Fail after replacing the executable: rollback must restore it.
    files[1]['sha256']='0'*64; (work/'plan.json').write_text(json.dumps(plan))
    assert run(root,'Install').returncode != 0
    assert (root/'perfectdarkzerorecomp.exe').read_bytes().startswith(b'original ')
    assert not (work/'success.json').exists()
    files[1]['sha256']=hashlib.sha256((staged/'rexruntime.dll').read_bytes()).hexdigest()
    (work/'plan.json').write_text(json.dumps(plan))
    result=run(root,'Install'); assert result.returncode == 0,(result.stdout,result.stderr,(work/'status.txt').read_text(encoding='utf-8-sig'))
    assert (work/'success.json').exists()
    for entry in files: assert hashlib.sha256((root/entry['name']).read_bytes()).hexdigest()==entry['sha256']
    for name,data in preserved.items(): assert (root/name).read_bytes()==data,name
    # Protected paths and traversal cannot be introduced by the plan either.
    for name in ['../escape.exe',*preserved,'assets/default.xex','mod_assets/../../escape','mod_assets/updater/source.json','mod_assets/NUL.txt','mod_assets/a:stream','out/surface/runtime.log','out/other.bin']:
        (work/'plan.json').write_text(json.dumps(dict(plan,files=[dict(name=name,sha256='0'*64)])))
        assert run(root,'Install').returncode != 0,name
    config=json.loads(source.read_text(encoding='utf-8-sig'))
    config['repository']='2023PerfectDark/PerfectDarkZeroRecomp'; source.write_text(json.dumps(config))
    release=dict(draft=False,prerelease=False,tag_name='next',published_at='2026-10-15T00:00:00Z',assets=[dict(name='test.zip',state='uploaded',digest='sha256:'+'a'*64,size=100,browser_download_url='https://github.com/2023PerfectDark/Perfect-Dark-Zero-Reinstated/releases/download/next/test.zip')])
    assert run(root,'Check',release).returncode==0
    assert (work/'status.txt').read_text(encoding='utf-8-sig').startswith('available')
    for url in ('https://example.com/test.zip','https://github.com/another/repository/releases/download/next/test.zip','http://github.com/2023PerfectDark/Perfect-Dark-Zero-Reinstated/releases/download/next/test.zip'):
        release['assets'][0]['browser_download_url']=url
        assert run(root,'Check',release).returncode!=0,url
    print('PASS: install, rollback, settings/save/profile preservation, protected paths, renamed repository and rejected foreign/insecure downloads')

