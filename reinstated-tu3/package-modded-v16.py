from pathlib import Path
import zipfile, hashlib, json
root=Path.cwd();repo=root/'compatibility-v16/PerfectDarkZeroRecomp';sdk=root/'compatibility-v16/sdk';build=repo/'out/build/win-amd64-release'
installed=Path('H:/Perfect Dark Zero Xbox360/PDZrecomp-win64 (3)/PDZ-Recomp')
release=root/'releases';archive=release/'PDZ-Recomp-v1.6-Modded-Update-2026-10-18.zip'
files={}
def add(name,path):
 assert not any(p in Path(name).parts for p in ['..','settings','profiles','saves','userdata','mod-progress','logs','imports'])
 files[name]=path.read_bytes()
for name in ['perfectdarkzerorecomp.exe','rexruntime.dll','rexgpu-xenos.dll','gamecontrollerdb.txt']:
 add(name,build/name)
 if name.endswith(('.exe','.dll')):assert files[name]==(installed/name).read_bytes(),f'Installed mismatch: {name}'
for name in ['LICENSE','LICENSE.mousehook']:add(name,repo/name)
add('licenses/ReXGlue-LICENSE',sdk/'LICENSE')
for dep in (sdk/'thirdparty').iterdir():
 if dep.is_dir():
  for p in dep.iterdir():
   if p.is_file() and p.name.upper().startswith(('LICENSE','COPYING','NOTICE')):add('licenses/runtime/'+dep.name+'/'+p.name,p)
for p in (installed/'mod_assets').rglob('*'):
 if not p.is_file():continue
 rel=p.relative_to(installed).as_posix()
 if p.name in ['source.json','sources.json','changelog.txt'] or p.name.startswith('runoff-'):continue
 if p.suffix.lower() not in ['.png','.jpg','.rgba','.bin','.wav','.txt','.md','.ps1','.dbm']:continue
 add(rel,p)
# Include loose character overrides only; never the original disc's ASSETS tree.
for directory in (installed/'assets/pdz').iterdir():
 if not directory.is_dir() or not directory.name.startswith('aid_') or 'major' in directory.name:continue
 p=directory/'default.dbm'
 if p.is_file():add(p.relative_to(installed).as_posix(),p)
for name in ['terrain.pdzsurf','mission-music.wav']:add('out/surface/'+name,installed/'out/surface'/name)
notes='''Perfect Dark Zero Reinstated - v1.6 modded update
Prepared 2026-10-18. Local release candidate; not published.

INSTALL
Close the game. Back up your installation, then merge the PDZ-Recomp folder
into your existing installation and replace matching files. Keep the EXE and
both runtime DLLs together. Original compatible game files are still required.
This archive does not contain the full base game, an ISO, or default.xex.
Do not delete your settings, profiles, saves or mod-progress folders.
Use Launch Stable.cmd: it disables background shader compilation to avoid the
bright corruption seen while missions loaded. It does not force a renderer.
Direct3D 12 and Vulkan remain selectable; renderer/driver coverage is incomplete.
Existing saved settings continue to control optional gameplay features.

INCLUDED
- v1.6 runtime with the current mouse/input and gameplay modifications.
- Rain in Missions 1/2; snow in Missions 4/6; screen droplets and surface audio.
- Breathing, footstep camera motion and smoothed head-roll transitions.
- Current Nightclub population, civilian dancing and guard-suspicion behavior.
- DarkOps bot team, scoreboard and round-life corrections.
- PD-style mission summary, cheats and multiplayer progression menus.
- Mouse-operated native sliders and settings arrows.
- d-PAL notification hides after five seconds; messages remain accessible.
- Short-dress Joanna head with the approved beauty mark.
- Debug Position remains available for restoring position after game closure.

EXCLUDED
Major, the discarded concept outfit, development projects, diagnostic tools,
logs, symbols, memory dumps, personal settings, saves, profiles and unlocks.
No personal Perfect Dark Zero mode unlock is supplied. Other users must earn it.
Normal application error logging may still occur; no debug configuration is shipped.

VALIDATION / LIMITS
The binaries match the installed build. Applicable policy tests passed, including
position storage, input, profiles, progression, bot waves, reload timing, camera
motion and notification timeout. Two legacy full-checkpoint tests reference SDK
headers absent from v1.6 and were not run. Full mission/multiplayer regression
and clean-install gameplay testing are incomplete. This is not a claim of full
checkpoint serialization or a guarantee against all crashes.

SHA256SUMS.json covers every payload file except itself. This update contains
modified character/media resources and retains available license notices.
'''
files['UPDATE.txt']=notes.encode()
files['mod_assets/updater/changelog.txt']=notes.encode()
files['Launch Stable.cmd']=b'@echo off\r\ncd /d "%~dp0"\r\nstart "" perfectdarkzerorecomp.exe --async_shader_compilation=false\r\n'
# Verify approved cosmetic and absent development artifacts.
head='assets/pdz/aid_darkcharacter_xenonheads_jo_shortdress_head/default.dbm'
assert files[head]==(root/'mp-character-assets/aid_darkcharacter_xenonheads_jo_shortdress_head/default.dbm').read_bytes()
for n in files:
 assert not any(s in n.lower() for s in ['major','concept-outfit','personal-pdz','diagnostic','debug','source.json'])
 assert not n.lower().endswith(('.iso','.xex','.pdb','.map','.log','.dmp','.exe.bak'))
hashes={n:hashlib.sha256(data).hexdigest() for n,data in sorted(files.items())}
files['SHA256SUMS.json']=json.dumps(hashes,indent=2).encode()
assert not archive.exists(),'Refusing to overwrite an existing release'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,data in sorted(files.items()):z.writestr('PDZ-Recomp/'+n,data)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 for n,digest in hashes.items():assert hashlib.sha256(z.read('PDZ-Recomp/'+n)).hexdigest()==digest,n
sha=hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.zip.sha256').write_text(sha+'  '+archive.name+'\n')
(release/'v1.6-release-validation.json').write_text(json.dumps({'archive':archive.name,'sha256':sha,'files':len(files),'bytes':archive.stat().st_size,'tests':'24 policy suites plus notification timer passed','legacyTestsNotApplicable':['checkpoint_continuation','debug_checkpoint_slot'],'installedBinariesMatch':True,'published':False},indent=2))
print(archive);print(f'{len(files)} verified files; {archive.stat().st_size/2**20:.1f} MiB');print(sha)
