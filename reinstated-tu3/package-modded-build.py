from pathlib import Path
import zipfile,hashlib,json,tomllib
# Reuse the explicit executable/runtime whitelist and clean launch settings.
source=Path('package-public-build.py').read_text()
exec(source[:source.index("for name in ['settings/hardware.toml'")])
files.pop('Launch.cmd', None)
archive=out/'PDZ-Recomp-2026-10-05-With-Mods-No-Base-Game.zip'
for folder,extensions in [('mod_assets/campaign',{'.png'}),('mod_assets/surface',{'.png','.jpg'}),('settings/achievement-art',{'.rgba'})]:
 for p in (repo/folder).iterdir():
  if p.suffix in extensions:files[p.relative_to(repo).as_posix()]=p.read_bytes()
for name in ['out/surface/terrain.pdzsurf','out/surface/mission-music.wav','settings/achievement-regular.mp3','settings/achievement-rare.mp3']:
 files[name]=(repo/name).read_bytes()
# This is the custom Surface barrel package, not the stock PackageData directory.
files['assets/pdz/ASSETS/PackageData/832751055.pkg']=Path('H:/Perfect Dark Zero Xbox360/PDZrecomp-win64 (3)/PDZrecomp-win64/assets/pdz/ASSETS/PackageData/832751055.pkg').read_bytes()
hardware=files['settings/hardware.toml'].decode().replace('pdz_surface_test = false','pdz_surface_test = true').replace('pdz_surface_mission_slot = false','pdz_surface_mission_slot = true')
hardware+='pdz_surface_collision_test = true\npdz_surface_objectives_test = true\npdz_surface_host_mission = 4\npdz_surface_debug_unlock = false\npdz_surface_daytime = false\ngpu_allow_invalid_fetch_constants = true\nclear_memory_page_state = false\nreadback_memexport = false\nreadback_memexport_fast = false\nrender_target_path_d3d12 = "rtv"\n'
files['settings/hardware.toml']=hardware.encode()
files['README.txt']=b'''PDZ Recomp with Mods - Windows x64 - 2026-10-05

INSTALL
1. Extract this ZIP into a new folder.
2. Copy your own compatible original retail TU0 Perfect Dark Zero disc dump
   into assets: assets/default.xex and assets/pdz/ASSETS/... are required.
   Keep the custom 832751055.pkg already supplied in PackageData.
3. Double-click perfectdarkzerorecomp.exe. No launcher or command-line flags required.

INCLUDED
Current modified executable and runtime DLLs; gameplay/menu/input mods;
Surface terrain and music, native barrel mod package, campaign portraits,
mission preview images, achievement artwork and regular/rare sounds.
Surface is enabled as custom mission 14. Its objectives remain unfinished.
Existing experimental functionality remains as in the current tested build.

NOT INCLUDED
The base game, default.xex, disc images, stock asset folders, regional dub
banks, personal settings, saves, profiles, logs, or achievement unlock requests.
The Klobb is not a finished imported weapon in this package.

CONTROLS
WASD move; mouse aim; LMB fire; RMB aim; E interact/select; R reload;
1 weapon; I inspection; F6 settings; F9 local profiles. Controllers supported.

VOICES
F6 > Voice language. Reload the mission after changing. Supply the matching
voice banks from your own regional disc under assets/pdz/ASSETS/Audio:
english, german, japanese, latam, spanish, french, italian.
Menus and subtitles retain their existing language.

Mod media is included at the pack creator's request; base-game files are not.
Achievement presentation references https://codepen.io/uenify/pen/KxzKVd .
Runtime/mousehook license notices are in licenses. SHA256SUMS.json records
all packaged files. This modified test build is not a fully validated release.
'''
for name in ['settings/hardware.toml','settings/mapping.toml']:tomllib.loads(files[name].decode('utf-8-sig'))
files['SHA256SUMS.json']=json.dumps({n:hashlib.sha256(b).hexdigest() for n,b in files.items()},indent=2).encode()
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,b in files.items():z.writestr('PDZ-Recomp/'+n,b)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for n,b in files.items():assert z.read('PDZ-Recomp/'+n)==b
 assert not any(n.lower().endswith(('.xex','.iso','.xwb')) for n in z.namelist())
print(archive);print(f'{archive.stat().st_size/1024/1024:.1f} MiB; {len(files)} files verified')



