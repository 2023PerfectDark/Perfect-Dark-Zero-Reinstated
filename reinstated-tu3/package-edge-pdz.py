from pathlib import Path
import subprocess
import shutil
import hashlib

root = Path(__file__).resolve().parent
src = root / 'xenia-edge-mousehook'
out = root / 'edge-pdz-package'
shutil.copy2(src / 'build/bin/Windows/Release/xenia_edge.exe', out)
shutil.copy2(src / 'LICENSE', out / 'LICENSE')
shutil.copy2(root / 'mousehook-reference/LICENSE', out / 'LICENSE-MOUSEHOOK')
shutil.copy2(root / 'compatibility-v13/PerfectDarkZeroRecomp/LICENSE', out / 'LICENSE-RECOMP')
shutil.copytree(Path(r'H:\Perfect Dark Zero Xbox360\xenia_edge_windows\D3D12'), out / 'D3D12', dirs_exist_ok=True)
patch = subprocess.check_output(['git', 'diff', '--binary', '--', 'src'], cwd=src)
new = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', 'src'], cwd=src).decode().splitlines()
for file in new:
    diff = subprocess.run(['git', 'diff', '--no-index', '--binary', '--', '/dev/null', file], cwd=src, stdout=subprocess.PIPE)
    if diff.returncode != 1:
        raise RuntimeError(f'Cannot export {file}')
    patch += diff.stdout
(out / 'source-port.patch').write_bytes(patch)
edge_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=src).decode().strip()
hook_commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root / 'mousehook-reference').decode().strip()
(out / 'BUILD-INFO.txt').write_text(f'Custom PDZ Edge mousehook + gameplay build\nEdge: {edge_commit}\nMousehook source: {hook_commit}\nGameplay source: local compatibility-v13/PerfectDarkZeroRecomp gameplay mods\nGameplay support: PDZ TU0 build 09.11.05.0052 only\nCompiler: MSVC 14.50 x64 Release\nBuilt with xenia-build.py build --config=Release --target=xenia-app\nSource patch applies on top of the Edge commit above.\n', encoding='utf-8')
manifest = []
for file in sorted(out.rglob('*')):
    if file.is_file() and file.name != 'SHA256SUMS.txt':
        manifest.append(f'{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.relative_to(out).as_posix()}')
(out / 'SHA256SUMS.txt').write_text('\n'.join(manifest) + '\n', encoding='utf-8')
print(f'Packaged {len(manifest)} files in {out}')
