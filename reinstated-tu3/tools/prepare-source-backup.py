"""Collect explicitly selected source trees; never stage the live game or user data."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess
parser=argparse.ArgumentParser()
parser.add_argument('--installer-version', default='v124')
parser.add_argument('--game-version', default='v123')
args=parser.parse_args()
if not all(v.startswith('v') and v[1:].isdigit() for v in (args.installer_version,args.game_version)):
    parser.error('Versions must be v followed by digits')
root=Path(__file__).resolve().parents[1]
checksums=root/('releases/SHA256SUMS-'+args.installer_version+'.txt')
if not checksums.is_file(): raise FileNotFoundError('Build and validate the release checksum manifest before backing up source')
repo=root/'staging/reinstated-github-backup'; dest=repo/'reinstated-tu3'
dest.mkdir(exist_ok=True)
suffixes={'.cpp','.h','.hpp','.inl','.c','.cmake','.toml','.json','.py','.ps1','.cmd','.cs','.mjs','.md','.txt','.ico','.rc'}
inventory={}
def copy(p, rel=None):
    relative=Path(rel) if rel else p.relative_to(root)
    target=dest/relative;target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(p,target)
    inventory[relative.as_posix()]=hashlib.sha256(p.read_bytes()).hexdigest()
def tree(relative):
    base=root/relative
    if not base.exists(): return
    for p in sorted(base.rglob('*')):
        if p.is_file() and p.suffix.lower() in suffixes and not any(x in p.parts for x in ('.git','__pycache__','out','user')):
            copy(p)
for relative in ('compatibility-v16/ModdedTU3/src','compatibility-v16/ModdedTU3/tests',
                 'compatibility-v16/ModdedTU3/generated-patched','compatibility-v16/StockTU3/src',
                 'compatibility-v16/StockTU3/generated','compatibility-v16/PerfectDarkZeroRecomp/src',
                 'compatibility-v16/PerfectDarkZeroRecomp/tests','tools','tests'):
    tree(relative)
for p in (root/'installer').iterdir():
    if p.is_file() and p.suffix.lower() in {'.cs','.cpp','.py','.cmd','.json','.txt','.mjs'} and not p.name.startswith(('test-','stock-')):
        copy(p)
for p in root.glob('*.cmd'): copy(p)
for p in root.glob('*.py'): copy(p)
for base in ('compatibility-v16/StockTU3','compatibility-v16/ModdedTU3'):
    for p in (root/base).glob('*.toml'): copy(p)
    for p in (root/base).glob('CMake*'): copy(p)
for p in (root/'installer/payload/common/mod_assets/updater').iterdir():
    if p.suffix in ('.ps1','.json','.txt'): copy(p)
sdk=root/'compatibility-v16/sdk'
copy(sdk/'LICENSE')
copy(checksums, 'release/'+checksums.name)
metadata={}
for label, path in [('sdk',sdk),('upstream_game',root/'compatibility-v16/PerfectDarkZeroRecomp')]:
    revision=subprocess.run(['git','-C',str(path),'rev-parse','--verify','HEAD'],capture_output=True,text=True)
    origin=subprocess.run(['git','-C',str(path),'remote','get-url','origin'],capture_output=True,text=True)
    metadata[label]={
        'commit':revision.stdout.strip() if revision.returncode==0 else None,
        'origin':origin.stdout.strip() if origin.returncode==0 else None}
patch=dest/'compatibility-v16/sdk-working-tree.patch';patch.parent.mkdir(parents=True,exist_ok=True)
patch.write_bytes(subprocess.check_output(['git','-C',str(sdk),'diff','--binary','HEAD']))
inventory[patch.relative_to(dest).as_posix()]=hashlib.sha256(patch.read_bytes()).hexdigest()
for raw in subprocess.check_output(['git','-C',str(sdk),'ls-files','--others','--exclude-standard','-z']).decode().split('\0'):
    if raw and (sdk/raw).suffix in suffixes: copy(sdk/raw)
lib=sdk/'thirdparty/libmspack'
if lib.exists():
    patch=dest/'compatibility-v16/libmspack-working-tree.patch'
    patch.write_bytes(subprocess.check_output(['git','-C',str(lib),'diff','--binary','HEAD']))
    metadata['libmspack_commit']=subprocess.check_output(['git','-C',str(lib),'rev-parse','HEAD'],text=True).strip()
(dest/'BASELINES.json').write_text(json.dumps(metadata,indent=2)+'\n')
(dest/'SOURCE-SHA256.json').write_text(json.dumps(inventory,indent=2)+'\n')
(dest/'.gitattributes').write_text('* -text\n')
(dest/'README.md').write_text(f'''# Reinstated TU3 source backup — {args.installer_version} installer / {args.game_version} game

This snapshot preserves the current TU3 modifications, generated TU3 translation,
tests, installer sources, packaging tools, and the SDK working-tree patch.
It sits alongside the older repository tree so that older work is preserved.
`BASELINES.json` identifies the SDK and game baseline commits. Apply
`compatibility-v16/sdk-working-tree.patch` to that SDK baseline; the separately
included SDK files are additions. A libmspack patch is included for its recorded baseline.

The release assets contain the installer and runtime update. The installer embeds
the reviewed playthrough shader seed as an optional choice. Personal saves,
profiles, live settings, crash dumps, and logs are excluded from this snapshot.
Original game disc inputs, compilers, and compiled build intermediates are not included.

The build scripts preserve the working Windows layout and local paths. A clean
machine needs the matching SDK dependencies, LLVM 20.1.8, Visual Studio tools,
Windows SDK, and legally supplied game/TU3 inputs; regenerate the baseline CMake
build and stock objects before running `tools/build-modded-tu3.py --next`.
This is a source backup, not a verified one-command clean-machine build.

`installer/VALIDATION.txt` records test coverage and outstanding gameplay checks.
The installer accepts the original US disc and US Rev 1. Rev 1 compatibility
uses verified source/output images; tools/make-rev1-compatibility-patch.py
documents patch generation from locally supplied inputs.
''',encoding='utf-8')
print(json.dumps({'snapshot':str(dest),'files':len(inventory),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
