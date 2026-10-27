import pathlib,re,json
root=pathlib.Path('PerfectDarkZeroRecomp')
asset=pathlib.Path(r'H:\Perfect Dark Zero Xbox360\PDZrecomp-win64 (3)\PDZrecomp-win64\assets\pdz\ASSETS\loc\english\aid\strings\text\general\default.str')
strings=re.findall(r'[\x20-\x7e]{2,}',asset.read_bytes().decode('utf-16-le',errors='ignore'))
start=strings.index('KSI-74')-1
native=strings[start:start+41]
assert native[0]=='Null' and native[40]=='FLAG'
aliases={'DY357 MAGNUM':'MAGNUM','LAPTOP GUN':'LAPTOP','PLASMA RIFLE':'PLASMA','RCP-90':'RCP90','MAYHEM':'ROCKET LAUNCHER'}
entries=re.findall(r'\{(\d+),u"([^"]+)",(\d+)\}',(root/'src/weapon_challenge.h').read_text())
registry=json.loads(pathlib.Path('native-weapon-registry.json').read_text(encoding='utf-8-sig'))
expected={x['id'] for x in registry if x['id']<40}
assert len(entries)==len(expected)==25
assert {int(i) for i,n,r in entries}==expected
rows=[]
for i,name,reserve in entries:
    i,reserve=int(i),int(reserve)
    assert aliases.get(name,name)==native[i],(i,name,native[i])
    assert reserve==(800 if i in {2,7,11,15,16,3,5,17,20} else 400),(name,reserve)
    rows.append({'id':i,'challenge':name,'native_label':native[i],'reserve':reserve})
pathlib.Path('weapon-challenge-audit.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS: all 25 entries match native English weapon IDs; unique IDs, playable registry coverage, and reserve limits verified.')
