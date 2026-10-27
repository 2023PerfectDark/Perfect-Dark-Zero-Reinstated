from pathlib import Path
import zipfile,hashlib,json,tomllib
root=Path.cwd(); repo=root/'PerfectDarkZeroRecomp'; build=repo/'out/build/win-amd64-release'
out=root/'releases';out.mkdir(exist_ok=True)
archive=out/'PDZ-Recomp-2026-10-05-No-Game-Assets.zip'
files={n:(build/n).read_bytes() for n in ['perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt']}
files['settings/mapping.toml']=(repo/'settings/mapping.toml').read_bytes()
files['settings/hardware.toml']=b'''# Portable defaults; no personal paths or saved progress.
log_level = "info"
gpu_plugin = "xenos"
window_width = 1280
window_height = 720
pdz_dub_language = 0
pdz_surface_test = false
pdz_surface_mission_slot = false
pdz_surface_intro_test = false
pdz_surface_intro_pose_inspection = false
pdz_unlock_all_missions = false
pdz_coop_ally = false
'''
files['Launch.cmd']=b'''@echo off\r\ncd /d "%~dp0"\r\nif not exist "assets\\default.xex" (\r\n  echo Add your extracted Perfect Dark Zero game files to the assets folder first.\r\n  echo See README.txt for instructions.\r\n  pause\r\n  exit /b 1\r\n)\r\nperfectdarkzerorecomp.exe --game_data_root="assets" --user_data_root="userdata"\r\n'''
files['assets/PUT-YOUR-GAME-FILES-HERE.txt']=b'Place your own extracted game files here: default.xex and the pdz folder. No game files are included.\n'
files['README.txt']=b'''PDZ Recomp - Windows x64 - 2026-10-05

INSTALL
1. Extract this entire ZIP into a new folder.
2. Supply your own compatible original retail TU0 Perfect Dark Zero files.
   Copy the extracted disc contents into assets, including:
     assets/default.xex
     assets/pdz/ASSETS/...
   A disc ISO must be extracted first. Do not copy only default.xex.
3. Run Launch.cmd. Saves are created in the package's userdata folder.

CONTROLS
WASD move; mouse aim; LMB fire; RMB aim; E interact/select; R reload;
1 weapon; I inspection; F6 settings; F9 local profiles.
Controller input is also supported. See settings/mapping.toml for bindings.

VOICE LANGUAGES
F6 > Voice language: English, Deutsch, Japanese, Spanish (Latin America),
Spanish (Europe), French, Italian. Reload the mission after changing.
Each dub requires the corresponding voice banks from your own regional disc:
assets/pdz/ASSETS/Audio/english, german, japanese, latam, spanish, french,
or italian. No voice banks are included. Missing dubs may fall back to English.
Menus and subtitles retain their existing language.

CONTENTS / LIMITATIONS
Contains the current modified executable, runtime DLLs, controller mappings,
and fresh settings. No game dump, ISO, XEX, voice banks, textures, models,
Surface map/music, achievement artwork/sounds, saves, profiles, logs, unlock
requests, or personal configuration is included. Custom Surface is disabled
because its separate map assets are absent. Achievement artwork/sounds are
not present in this asset-free package. The executable includes experimental
features from the tested build; this is not a fully validated public release.

Keep the two runtime DLLs beside the executable. Runtime and mousehook
license notices are in licenses. SHA256SUMS.json lists the packaged files.
'''
files['licenses/LICENSE.mousehook']=(repo/'LICENSE.mousehook').read_bytes()
files['licenses/ReXGlue-LICENSE']=(root/'rexglue-reference/LICENSE').read_bytes()
files['licenses/SDL3-LICENSE.txt']=(repo/'rexglue/win-amd64/licenses/SDL3/LICENSE.txt').read_bytes()
for name in ['settings/hardware.toml','settings/mapping.toml']:tomllib.loads(files[name].decode('utf-8-sig'))
files['SHA256SUMS.json']=json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in files.items()},indent=2).encode()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,b in files.items():z.writestr('PDZ-Recomp/'+n,b)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 assert len(z.namelist())==len(files)
 for n,b in files.items():assert z.read('PDZ-Recomp/'+n)==b
print(str(archive));print(f'{archive.stat().st_size/1024/1024:.1f} MiB; {len(files)} verified files')
print('SHA256 '+hashlib.sha256(archive.read_bytes()).hexdigest())

