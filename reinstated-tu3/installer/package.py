"""Assemble the reviewed installer files, without local test inputs or logs."""
from pathlib import Path
import argparse, hashlib, json, shutil, zipfile, subprocess, os, struct

src = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument('--name', default='Perfect Dark Zero Reinstated Installer dPAL 2026-10-19 v12')
parser.add_argument('--setup', default='PDZ Setup.exe')
args = parser.parse_args()
manifest = json.loads((src / 'payload.json').read_text())
# Every install needs the helper and its source, even before an update is published.
for name in ('source.json', 'update.ps1'):
    rows = [r for r in manifest['files'] if r['destination'] == 'mod_assets/updater/' + name]
    if len(rows) != 1 or rows[0]['group'] != 'common':
        parser.error('Missing updater payload: ' + name)
    path = src / rows[0]['source']
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != rows[0]['sha256']:
        parser.error('Invalid updater payload: ' + name)
source = json.loads((src / 'payload/common/mod_assets/updater/source.json').read_text())
if (source.get('repository') != '2023PerfectDark/Perfect-Dark-Zero-Reinstated' or
        source.get('asset') != 'PDZ-Reinstated-TU3-Update.zip'):
    parser.error('Updater must target the Reinstated TU3 release channel')
# The optional seed is TU3-only and must never include local driver binaries,
# settings, saves, or map-load logs from the creator's profile.
shader_names = {'4D5307D3.xsh', '4D5307D3.rov.d3d12.xpso',
                '4D5307D3.rtv.d3d12.xpso', '4D5307D3.fbo.vk.xpso'}
shader_rows = [row for row in manifest['files'] if row['group'] == 'shaders-tu3']
expected_shader_destinations = {'user/cache/shaders/shareable/' + n for n in shader_names}
if (len(shader_rows) != len(shader_names) or
        {row['destination'] for row in shader_rows} != expected_shader_destinations):
    parser.error('The optional TU3 shader cache must contain exactly the four reviewed shareable files')
for row in manifest['files']:
    relative = row['destination'].replace('\\', '/').lower()
    if relative.startswith(('user/', 'settings/', 'cache/')) and row not in shader_rows:
        parser.error('Personal runtime state must not be bundled: ' + row['destination'])
# Surface is a shipped mission, not an optional development asset. Fail before
# creating a release if a refreshed payload has accidentally dropped it.
required_surface = ('terrain.pdzsurf', 'mission-music.wav', 'cabin-floor.rgba',
                    'cabin-wall.rgba', 'klobb-body.rgba')
for name in required_surface:
    destination = 'mod_assets/surface/' + name
    rows = [row for row in manifest['files'] if row['destination'] == destination]
    if len(rows) != 1 or rows[0]['group'] != 'common':
        parser.error('Surface must be included in every installation: ' + destination)
    row = rows[0]
    asset = src / row['source']
    if not asset.is_file() or asset.stat().st_size != row['size']:
        parser.error('Missing or incomplete Surface asset: ' + str(asset))
    with asset.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != row['sha256']:
            parser.error('Surface asset hash mismatch: ' + str(asset))
terrain_row = next(row for row in manifest['files'] if row['destination'] == 'mod_assets/surface/terrain.pdzsurf')
terrain = (src / terrain_row['source']).read_bytes()
if len(terrain) < 64 or terrain[:8] != b'PDZSURF1':
    parser.error('Invalid Surface terrain header')
version, = struct.unpack_from('>I', terrain, 44)
pickup, = struct.unpack_from('>I', terrain, 60)
triangles, = struct.unpack_from('>I', terrain, 12)
if version < 4 or not (pickup & 65535) or (pickup >> 16) + (pickup & 65535) > triangles:
    parser.error('Surface terrain is missing the reviewed Klobb/scenery geometry')
barrels = [row for row in manifest['files'] if row['destination'] == 'assets/pdz/ASSETS/PackageData/832751055.pkg']
if len(barrels) != 1 or barrels[0]['group'] != 'common':
    parser.error('Surface native red-barrel package is missing')
if Path(args.name).name != args.name or '/' in args.name or '\\' in args.name or args.name in ('.', '..'):
    parser.error('--name must be a single folder name')
dest = src.parent / 'releases' / args.name
dest.mkdir(exist_ok=False)
for name in ('art',):
    shutil.copytree(src / name, dest / name)
for name in ('payload.json', 'retail-index.json', 'text.json', 'README.txt', 'VALIDATION.txt'):
    shutil.copy2(src / name, dest / name)
shutil.copy2(src / args.setup, dest / 'PDZ Setup.exe')
for item in json.loads((src/'payload.json').read_text())['files']:
    target=dest/item['source']; target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(src/item['source'],target)
art = json.loads((src / 'art.json').read_text())
art.pop('missing', None)
(dest / 'art.json').write_text(json.dumps(art, indent=2), encoding='utf-8')
(dest / 'tools').mkdir()
for name in ('extract-xiso.exe', 'LICENSE.TXT'):
    shutil.copy2(src / 'tools' / name, dest / 'tools' / name)
for file in json.loads((dest / 'payload.json').read_text())['files']:
    p = dest / file['source']
    assert p.stat().st_size == file['size'], p
    with p.open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == file['sha256'], p
for path in list(art['roles'].values()) + art['comics']:
    assert (dest / path).is_file(), path
for path in ('mission-select.wav', 'dpal/left.png', 'dpal/right.png', 'dpal/xbox.png',
             'audio/select.wav', 'audio/back.wav', 'audio/cursor.wav', 'audio/error.wav', 'audio/checkbox.wav'):
    assert (dest / 'art' / path).is_file(), path
archive = dest.with_suffix('.zip')
with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for p in sorted(dest.rglob('*')):
        if p.is_file():
            z.write(p, dest.name + '/' + p.relative_to(dest).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
with archive.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
archive.with_suffix('.zip.sha256').write_text(digest + '  ' + archive.name + '\n')
# Also ship one self-contained installer; keep ZIP/folder as build artifacts.
single = dest.parent / (args.name + '.exe')
csc = Path(os.environ.get('WINDIR', r'C:\Windows')) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
subprocess.run([str(csc), '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
                '/out:' + str(single), '/resource:' + str(archive) + ',payload.zip',
                '/r:System.Windows.Forms.dll', '/r:System.Drawing.dll',
                '/r:System.IO.Compression.dll', str(src / 'Bootstrap.cs')], check=True)
with single.open('rb') as stream:
    single_digest = hashlib.file_digest(stream, 'sha256').hexdigest()
single.with_suffix('.exe.sha256').write_text(single_digest + '  ' + single.name + '\n')
# User-facing distribution: only the installer and the two readable documents.
distribution = dest.parent / (args.name + ' Download')
distribution.mkdir(exist_ok=False)
shutil.copy2(single, distribution / 'PDZ Setup.exe')
for name in ('README.txt', 'VALIDATION.txt'):
    shutil.copy2(src / name, distribution / name)
print(json.dumps({'folder': str(dest), 'zip': str(archive),
                  'bytes': archive.stat().st_size, 'sha256': digest,
                  'installer_exe': str(single), 'installer_sha256': single_digest,
                  'distribution': str(distribution)}, indent=2))
