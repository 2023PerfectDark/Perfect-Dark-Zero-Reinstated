"""Package the installed preventive fixes without changing release defaults."""
from pathlib import Path
import hashlib
import json
import re
import tomllib
import shutil
import zipfile

root = Path(__file__).resolve().parent
build = root / 'compatibility-v13/PerfectDarkZeroRecomp/out/build/win-amd64-release'
installed = Path('H:/Perfect Dark Zero Xbox360/PDZrecomp-win64 (3)/PDZ-Recomp')
backups = root / 'crash-investigation/release-backups-2026-10-07'
backups.mkdir(exist_ok=True)
payload = {name: (build / name).read_bytes() for name in
           ('perfectdarkzerorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll')}
for name, data in payload.items():
    assert data == (installed / name).read_bytes(), f'Installed build differs: {name}'

note_name = 'PREVENTIVE-FIXES-2026-10-07.txt'
notes = '''PREVENTIVE RECOMP FIXES - 2026-10-07 (America/New_York)
- Validate cinematic weapon scratch-stack space before writing temporary data.
- Validate attachment construction and model pointers before cosmetic updates.
- Bound cinematic scenegraph visibility indices to the native model pool.
- Skip optional gore team queries when guest scratch-stack space is invalid,
  while preserving the original native hit handling.
- Enable Surface's native snow walking/running material by default and include
  its setting explicitly in release configs (previous releases omitted it).
Build and focused holster/campaign tests passed. Gameplay retesting is pending.
This is preventive hardening, not a claim that all possible crashes are fixed.
Earlier release fixes and all existing settings/assets are retained.
Keep the executable and both runtime DLLs together when updating.
No Xenia Edge binaries, game assets, personal settings, saves or logs were added.
'''
for name in ('PDZ-Recomp-v1.3-With-Mods-No-Base-Game.zip',
             'PDZ-Recomp-v1.3-With-Mods.zip'):
    archive = root / 'releases' / name
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        original = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    if not (backups / name).exists():
        shutil.copy2(archive, backups / name)
    files = dict(original)
    files.pop('PDZ-Recomp/SHA256SUMS.json', None)
    files.update({'PDZ-Recomp/' + n: data for n, data in payload.items()})
    files['PDZ-Recomp/' + note_name] = notes.encode()
    hardware_name = 'PDZ-Recomp/settings/hardware.toml'
    hardware = files[hardware_name].decode()
    key = 'pdz_surface_snow_material_test'
    before = tomllib.loads(hardware)
    if re.search(rf'(?m)^{key}\s*=', hardware):
        hardware = re.sub(rf'(?m)^{key}\s*=.*$', key + ' = true', hardware)
    else:
        hardware += '\n' + key + ' = true\n'
    after = tomllib.loads(hardware)
    assert after == {**before, key: True}
    files[hardware_name] = hardware.encode()
    readme = files['PDZ-Recomp/README.txt'].decode()
    if note_name not in readme:
        readme += '\nUpdated 2026-10-07: see ' + note_name + '.\n'
    files['PDZ-Recomp/README.txt'] = readme.encode()
    allowed = {'PDZ-Recomp/' + n for n in payload} | {
        'PDZ-Recomp/' + note_name, 'PDZ-Recomp/README.txt', 'PDZ-Recomp/SHA256SUMS.json', hardware_name}
    assert all(files[n] == data for n, data in original.items() if n not in allowed)
    assert not any(n.lower().endswith(('.xex', '.iso', '.xwb', '.cmd', '.dmp', '.log')) for n in files)
    hashes = {n.removeprefix('PDZ-Recomp/'): hashlib.sha256(data).hexdigest()
              for n, data in files.items()}
    files['PDZ-Recomp/SHA256SUMS.json'] = json.dumps(hashes, indent=2).encode()
    temporary = archive.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for n, data in files.items():
            z.writestr(n, data)
    with zipfile.ZipFile(temporary) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == set(files)
        for n, digest in hashes.items():
            assert hashlib.sha256(z.read('PDZ-Recomp/' + n)).hexdigest() == digest, n
    temporary.replace(archive)
    print(f'{name}: {len(files)} files verified; existing defaults preserved')
