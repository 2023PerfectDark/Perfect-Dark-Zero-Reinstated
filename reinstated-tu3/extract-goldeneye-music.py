import argparse, json, mmap, struct, pathlib, re

parser=argparse.ArgumentParser()
parser.add_argument('iso')
parser.add_argument('--extract', action='store_true')
args=parser.parse_args()
out=pathlib.Path('extracted/GoldenEye-Reloaded-music')
out.mkdir(parents=True,exist_ok=True)
with open(args.iso,'rb') as source, mmap.mmap(source.fileno(),0,access=mmap.ACCESS_READ) as image:
    base=next((p for p in (0,0xfb20,0x20600,0x2080000,0xfd90000)
               if image[p+65536:p+65556]==b'MICROSOFT*XBOX*MEDIA'),None)
    if base is None: raise ValueError('Xbox filesystem not found')
    sector,size=struct.unpack_from('<II',image,base+65556)
    files=[]
    def directory(sector,size,prefix,depth=0):
        if depth>50 or size>32*1024*1024: raise ValueError('Invalid directory')
        start=base+sector*2048
        if start+size>len(image): raise ValueError('Directory outside image')
        seen=set(); pending=[0]
        while pending:
            ordinal=pending.pop(); pos=ordinal*4
            if ordinal in seen: raise ValueError('Directory cycle')
            seen.add(ordinal)
            if pos+14>size: raise ValueError('Directory entry outside bounds')
            left,right,s,n,attributes,length=struct.unpack_from('<HHIIBB',image,start+pos)
            if left==65535:
                if pos%2048: pending.append((pos//2048+1)*512)
                continue
            if pos+14+length>size: raise ValueError('Invalid filename length')
            name=image[start+pos+14:start+pos+14+length].decode('cp1252')
            if not name or name in ('.','..') or any(c in name for c in '/\\:'): raise ValueError('Unsafe filename')
            path=prefix/name
            if left: pending.append(left)
            if right: pending.append(right)
            if attributes&16:
                if n: directory(s,n,path,depth+1)
            else:
                offset=base+s*2048
                if offset+n>len(image): raise ValueError('File outside image')
                files.append(dict(path=path.as_posix(),offset=offset,size=n))
    directory(sector,size,pathlib.PurePosixPath())
    (out/'iso-index.json').write_text(json.dumps(files,indent=2))
    selected=[f for f in files if re.search(r'music|\.fsb$|\.xwb$|\.wem$|\.ogg$|\.xma$|\.mp3$',f['path'],re.I)]
    print(json.dumps(selected,indent=2))
    print('Indexed',len(files),'files; candidates',len(selected))
    if args.extract:
        for entry in selected:
            target=out/'original'/entry['path'];target.parent.mkdir(parents=True,exist_ok=True)
            if target.exists() and target.stat().st_size==entry['size']: continue
            with target.open('wb') as dest:
                for position in range(entry['offset'],entry['offset']+entry['size'],1024*1024):
                    dest.write(image[position:min(position+1024*1024,entry['offset']+entry['size'])])
