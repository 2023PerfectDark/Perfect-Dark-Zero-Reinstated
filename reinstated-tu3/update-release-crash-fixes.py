"""Update existing release inventories with the user-verified crash-fix build."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import tomllib
import zipfile

root = Path(__file__).resolve().parent
build = root / 'compatibility-v13/PerfectDarkZeroRecomp/out/build/win-amd64-release'
installed = Path('H:/Perfect Dark Zero Xbox360/PDZrecomp-win64 (3)/PDZ-Recomp')
backups = root / 'crash-investigation/release-backups-2026-10-06'
backups.mkdir(exist_ok=True)
binaries = ('perfectdarkzerorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll')
payload = {name: (installed / name).read_bytes() for name in binaries}
for name, data in payload.items():
    assert data == (build / name).read_bytes(), f'Build differs from tested installation: {name}'

notes = '''CRASH FIXES - 2026-10-06 EVENING (America/New_York)
Includes the current installed build confirmed working by the tester.
- Restored missing native virtual functions 0x8223D550 and 0x8223D568.
- Guarded model command rendering against null instance data (guest read 0x20).
  Invalid draws are skipped and logged; the source of the missing data is still
  under investigation. The affected encounter was successfully retested.
- Internal resolution defaults to 1x for the GPU-hang diagnostic configuration.
  This setting is not a confirmed fix for all GPU/driver hangs.
Keep the executable and both runtime DLLs together when updating.
Native co-op partner development remains disabled by default.
No base-game files, personal settings, saves, profiles, or logs were added.
'''

archives = [root / 'releases' / name for name in (
    'PDZ-Recomp-v1.3-With-Mods-No-Base-Game.zip',
    'PDZ-Recomp-v1.3-With-Mods.zip')]
for archive in archives:
    inventory = archive if archive.exists() else backups / archive.name
    with zipfile.ZipFile(inventory) as z:
        assert z.testzip() is None
        files = {n.removeprefix('PDZ-Recomp/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
    backup = backups / archive.name
    if not backup.exists():
        shutil.copy2(archive, backup)
    files.pop('SHA256SUMS.json', None)
    files.update(payload)
    hardware = files['settings/hardware.toml'].decode()
    for key, value in [('resolution_scale', '1'), ('pdz_coop_native_partner', 'false')]:
        hardware = re.sub(rf'(?m)^{key}\s*=.*\n?', '', hardware)
        hardware += f'\n{key} = {value}\n'
    config = tomllib.loads(hardware)
    assert config['resolution_scale'] == 1 and not config['pdz_coop_native_partner']
    assert config['pdz_surface_intro_test'] and config['pdz_surface_host_mission'] == 4
    assert not config['pdz_unlock_all_missions'] and not config['pdz_surface_debug_unlock']
    files['settings/hardware.toml'] = hardware.encode()
    files['CRASH-FIXES-2026-10-06.txt'] = notes.encode()
    readme = files['README.txt'].decode()
    if 'CRASH-FIXES-2026-10-06.txt' not in readme:
        readme += '\nUpdated 2026-10-06 evening: see CRASH-FIXES-2026-10-06.txt.\n'
    files['README.txt'] = readme.encode()
    assert not any(n.lower().endswith(('.xex', '.iso', '.xwb', '.cmd', '.dmp', '.log')) for n in files)
    assert not any(part.lower() in ('userdata', 'native-coop-userdata', 'logs', 'profiles')
                   for n in files for part in Path(n).parts)
    hashes = {n: hashlib.sha256(b).hexdigest() for n, b in files.items()}
    files['SHA256SUMS.json'] = json.dumps(hashes, indent=2).encode()
    temporary = archive.with_suffix('.zip.tmp')
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for n, data in files.items():
            z.writestr('PDZ-Recomp/' + n, data)
    with zipfile.ZipFile(temporary) as z:
        assert z.testzip() is None
        assert len(z.namelist()) == len(files)
        for n, expected in hashes.items():
            assert hashlib.sha256(z.read('PDZ-Recomp/' + n)).hexdigest() == expected, n
    temporary.replace(archive)
    print(f'{archive.name}: {len(files)} files verified; {archive.stat().st_size / 1024**2:.1f} MiB')
    print('SHA256 ' + hashlib.sha256(archive.read_bytes()).hexdigest())
