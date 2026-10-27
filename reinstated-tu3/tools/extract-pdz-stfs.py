"""Extract read-only STFS packages, verifying every extracted block's SHA-1."""
import hashlib, json, pathlib, struct, sys, zipfile

source, target = map(pathlib.Path, sys.argv[1:3])
if source.suffix.lower() == '.zip':
    with zipfile.ZipFile(source) as z:
        names = [n for n in z.namelist() if not n.endswith('/')]
        assert len(names) == 1
        name = pathlib.PurePosixPath(names[0]).name
        data = z.read(names[0])
else:
    name, data = source.name, source.read_bytes()
be = lambda p, n=4: int.from_bytes(data[p:p+n], 'big')
assert data[:4] in (b'LIVE', b'PIRS')
assert data[0x37b] & 1, 'Only read-only STFS supported'
base = (be(0x340) + 4095) & ~4095
def block(i):
    physical, level = i, 170
    for _ in range(3):
        physical += (i + level) // level
        if i < level: break
        level *= 170
    raw = data[base+physical*4096:base+(physical+1)*4096]
    hblock = 0 if i < 170 else (i//170)*171 + i//28900 + 1 + (i >= 28900)
    off = base+hblock*4096+(i%170)*24
    assert len(raw) == 4096 and hashlib.sha1(raw).digest() == data[off:off+20], f'Invalid block {i}'
    return raw, be(off+20) & 0xffffff
table = int.from_bytes(data[0x37e:0x381], 'little')
tables = int.from_bytes(data[0x37c:0x37e], 'little')
entries = []
for _ in range(tables):
    raw, table = block(table)
    for off in range(0,4096,64):
        e=raw[off:off+64]
        if not e[0]: break
        entries.append(e)
paths, files = [], []
target.mkdir(parents=True, exist_ok=True)
for e in entries:
    leaf=e[:e[40]&63].decode('utf-8')
    assert leaf not in ('.','..') and not any(c in leaf for c in '/\\:')
    parent=int.from_bytes(e[50:52], 'big')
    rel=pathlib.Path(leaf) if parent==65535 else paths[parent]/leaf
    paths.append(rel)
    out=target/rel
    if e[40]&128:
        out.mkdir(parents=True,exist_ok=True)
        continue
    size=int.from_bytes(e[52:56],'big')
    idx=int.from_bytes(e[47:50],'little')
    content=bytearray()
    while len(content)<size:
        raw,idx=block(idx)
        content.extend(raw[:min(4096,size-len(content))])
    out.parent.mkdir(parents=True,exist_ok=True)
    assert not out.exists(), out
    out.write_bytes(content)
    files.append({'path':rel.as_posix(),'size':size,'sha256':hashlib.sha256(content).hexdigest()})
info={'package':name,'package_sha256':hashlib.sha256(data).hexdigest(),'title_id':f'{be(0x360):08X}', 'version':f'{be(0x358):08X}', 'base_version':f'{be(0x35c):08X}', 'display_name':data[0x411:0x511].decode('utf-16-be').split('\0')[0], 'files':files}
(target.parent/(target.name+'-manifest.json')).write_text(json.dumps(info,indent=2))
print(json.dumps(info,indent=2))
