from pathlib import Path
import zipfile,hashlib
root=Path(__file__).resolve().parents[1]
folder=root/'releases/Perfect Dark Zero Reinstated v135 Download'
assert {p.name for p in folder.iterdir()}=={'PDZ Setup.exe','README.txt','VALIDATION.txt'}
archive=root/'releases/Perfect.Dark.Zero.Reinstated.v135.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in folder.iterdir():z.write(p,p.name)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
files=[archive,root/'releases/PDZ-Reinstated-TU3-Update.zip']
lines=[]
for p in files:
    with p.open('rb') as f:lines.append(hashlib.file_digest(f,'sha256').hexdigest()+'  '+p.name)
(root/'releases/SHA256SUMS-v135.txt').write_text('\n'.join(lines)+'\n')
print('\n'.join(lines),flush=True)
