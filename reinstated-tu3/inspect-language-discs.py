import pathlib,struct,json
root=pathlib.Path('H:/Perfect Dark Zero Xbox360')
for folder in ['Perfect Dark Zero (Europe) (En,Es,It)','Perfect Dark Zero (Europe) (En,Fr,De)','Perfect Dark Zero (Japan, Korea) (En,Ja,Zh,Ko)']:
 p=next((root/folder).glob('*.iso'))
 with p.open('rb') as f:
  for base in [0,0xFD90000,0x2080000]:
   f.seek(base+0x10000); h=f.read(2048)
   if h.startswith(b'MICROSOFT*XBOX*MEDIA'):break
  else: raise Exception(str(p))
  sec,size=struct.unpack_from('<II',h,20)
  entries=[]
  def walk(sec,size,prefix):
   f.seek(base+sec*2048); data=f.read(size);seen=set()
   def node(off):
    if off in seen or off+14>len(data):return
    seen.add(off);l,r,s,n,a,z=struct.unpack_from('<HHIIBB',data,off);name=data[off+14:off+14+z].decode('ascii');path=prefix+'/'+name
    if l:node(l*4)
    if a&16:walk(s,n,path)
    else:entries.append((path,s,n))
    if r:node(r*4)
   node(0)
  walk(sec,size,'')
  out=pathlib.Path('language-disc-index-'+folder.replace(',','').replace(' ','_')+'.json');out.write_text(json.dumps({'iso':str(p),'base':base,'files':entries}))
  print(out, sorted(set(x[0].split('/')[4] for x in entries if '/loc/' in x[0].lower() and len(x[0].split('/'))>4)),len(entries))


