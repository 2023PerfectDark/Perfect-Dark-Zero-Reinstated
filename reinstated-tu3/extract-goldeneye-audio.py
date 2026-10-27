import json, pathlib, struct, collections
root=pathlib.Path('extracted/GoldenEye-Reloaded-music')
index={e['path'].upper():e for e in json.loads((root/'iso-index.json').read_text())}
manifest=[];counts=collections.Counter()
with open(r'H:\Perfect Dark Zero Xbox360\GoldenEye 007 - Reloaded (USA, Europe) (En,Fr,De,Es,It).iso','rb') as iso:
    for bank in ('FILE_COM','FILE_USA','FILELIST'):
        header=index[f'DATA/{bank}.BIN'];iso.seek(header['offset']);table=iso.read(header['size'])
        version,_,total=struct.unpack_from('>III',table)
        assert version==13 and 20+total*36<=len(table)
        offsets=collections.defaultdict(int)
        for i in range(total):
            archive,size=struct.unpack_from('>II',table,20+i*36)
            entry=index[f'DATA/{bank}.{archive:03}'];offset=offsets[archive];offsets[archive]+=size
            assert offset+size<=entry['size']
            absolute=entry['offset']+offset;iso.seek(absolute);head=iso.read(min(size,4096))
            magic=head[:4];counts[magic.decode('ascii',errors='replace')]+=1
            if magic!=b'MUSX':continue
            subtype=head[0x800:0x804].decode('ascii',errors='replace')
            dest=root/'original'/bank/f'{i:04}_{head[0x800:0x804].hex()}.musx';dest.parent.mkdir(parents=True,exist_ok=True)
            if not dest.exists() or dest.stat().st_size!=size:
                iso.seek(absolute)
                with dest.open('wb') as output:
                    remaining=size
                    while remaining:
                        data=iso.read(min(remaining,1024*1024));assert data
                        output.write(data);remaining-=len(data)
            manifest.append(dict(bank=bank,index=i,archive=archive,offset=offset,size=size,subtype=subtype,path=str(dest)))
(root/'audio-manifest.json').write_text(json.dumps(manifest,indent=2))
print('Extracted',len(manifest),'audio containers;',collections.Counter(x['subtype'] for x in manifest))
print('File types:',ascii(counts))


