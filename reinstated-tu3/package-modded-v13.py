"""Package the isolated v1.3 port using the previously audited mod-media inventory."""
from pathlib import Path
import hashlib
import json
import tomllib
import zipfile

root = Path(__file__).resolve().parent
repo = root / 'compatibility-v13/PerfectDarkZeroRecomp'
sdk = root / 'compatibility-v13/sdk'
build = repo / 'out/build/win-amd64-release'
archive = root / 'releases/PDZ-Recomp-v1.3-With-Mods-No-Base-Game.zip'
inventory = root / 'releases/PDZ-Recomp-2026-10-05-With-Mods.zip'
if not inventory.exists():
    inventory = root / 'releases/PDZ-Recomp-v1.3-With-Mods.zip'
with zipfile.ZipFile(inventory) as z:
    files = {n.removeprefix('PDZ-Recomp/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
files.pop('SHA256SUMS.json', None)
for name in ['perfectdarkzerorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'gamecontrollerdb.txt']:
    files[name] = (build / name).read_bytes()
files['settings/mapping.toml'] = (repo / 'settings/mapping.toml').read_bytes()
hardware = files['settings/hardware.toml'].decode()
hardware = hardware.split('# v1.3 runtime and camera defaults.')[0]
hardware = hardware.replace('pdz_surface_intro_test = false', 'pdz_surface_intro_test = true')
hardware += '''
# v1.3 runtime and camera defaults. F7 toggles the camera; V remains melee.
depth_bias_shader_offset = true
pdz_gpu_wait_mode = 1
pdz_tp_enable = false
pdz_tp_offset_x = -80.0
pdz_tp_offset_y = 10.0
pdz_tp_offset_z = -320.0
'''
files['settings/hardware.toml'] = hardware.encode()
readme = files['README.txt'].decode().replace('Windows x64 - 2026-10-05', 'v1.3 - Windows x64 - 2026-10-05')
readme = readme.replace('F6 settings; F9 local profiles.', 'F6 settings; F7 third-person camera; F9 local profiles.')
readme = readme.split('\nV1.3 PORT')[0]
readme += '''
V1.3 PORT
Built from PerfectDarkZeroRecomp tag 1.3 (99ddda11) with its pinned
ReXGlue runtime (bd833a2a), retaining the existing custom mods.
Use the EXE and BOTH runtime DLLs together; do not mix versions.
Surface uses Mission 4 as its host and is enabled, but not unlocked by default.
The third-person camera is suspended during Surface intro/outro cameras.
This package is a complete modded build, not a plugin for an unchanged EXE.
2026-10-06: fixed the null current-entry read in native callback 82561A30
identified in the Mission 0 crash dump. Mission replay still needs validation.
Upstream: https://github.com/nikolaygorb/PerfectDarkZeroRecomp/releases/tag/1.3
'''
files['README.txt'] = readme.encode()
files['licenses/PerfectDarkZeroRecomp-LICENSE'] = (repo / 'LICENSE').read_bytes()
files['licenses/ReXGlue-LICENSE'] = (sdk / 'LICENSE').read_bytes()
for dep in (sdk / 'thirdparty').iterdir():
    if dep.is_dir():
        for p in dep.iterdir():
            if p.is_file() and (p.name.upper().startswith(('LICENSE', 'COPYING', 'NOTICE'))):
                files[f'licenses/runtime/{dep.name}/{p.name}'] = p.read_bytes()
config = tomllib.loads(hardware)
assert config['pdz_surface_host_mission'] == 4
assert config['pdz_surface_test'] and config['pdz_surface_mission_slot']
assert config['pdz_surface_intro_test']
assert not config['pdz_surface_intro_pose_inspection']
assert not config['pdz_unlock_all_missions'] and not config['pdz_surface_debug_unlock']
assert config['gpu_plugin'] == 'xenos'
assert tomllib.loads(files['settings/mapping.toml'].decode())['bind_third_person'] == 'F7'
assert not any(n.lower().endswith(('.xex', '.iso', '.xwb', '.cmd')) for n in files)
files['SHA256SUMS.json'] = json.dumps({n: hashlib.sha256(b).hexdigest() for n, b in files.items()}, indent=2).encode()
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for n, b in files.items():
        z.writestr('PDZ-Recomp/' + n, b)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for n, b in files.items():
        assert z.read('PDZ-Recomp/' + n) == b
    # A separate clean copy is used for startup testing; it is never repackaged.
    z.extractall(root / 'compatibility-v13/package-test')
print(f'{archive}\n{archive.stat().st_size / 1024**2:.1f} MiB; {len(files)} files verified')
print('SHA256 ' + hashlib.sha256(archive.read_bytes()).hexdigest())
