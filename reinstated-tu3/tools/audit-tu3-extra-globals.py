from pathlib import Path
import importlib.util,json,difflib,bisect,pickle
R=Path(__file__).resolve().parent.parent
P=R/'compatibility-v16/ModdedTU3'
s=importlib.util.spec_from_file_location('m',R/'tools/map-tu3-mod-functions.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
cache=P/'assembly-cache.pickle'
if cache.exists():old,new=pickle.loads(cache.read_bytes())
else:
 old=m.load(R/'compatibility-v16/PerfectDarkZeroRecomp/generated/default')
 new=m.load(R/'compatibility-v16/StockTU3/generated/default',R/'staging/tu3-audio-original-generated')
 cache.write_bytes(pickle.dumps((old,new)))
od=(R/'staging/audio-base-data.bin').read_bytes();nd=(R/'staging/audio-tu3-data.bin').read_bytes()
review=json.loads((P/'function-map-review.json').read_text())
mapping={int(a,16):int(b,16) for a,b in review['anchors'].items()}
mapping.update({int(a,16):int(b,16) for a,b in json.loads((P/'relocation-review.json').read_text())['functions'].items()})
ordered=sorted(mapping); target=sorted(new);report={}
for value in ('820B47AC','820B6450','820B652C','820C93A0','82D356EC'):
 a=int(value,16);rows=[]
 for f,left in old.items():
  indexes=[i for i,x in left['globals'].items() if x==a]
  if not indexes:continue
  if f in mapping:pool=[mapping[f]]
  else:
   i=bisect.bisect_left(ordered,f);estimate=f+mapping[ordered[max(0,i-1)]]-ordered[max(0,i-1)]
   pool=target[bisect.bisect_left(target,estimate-0x2000):bisect.bisect_right(target,estimate+0x2000)]
  scores=[]
  for b in pool:
   right=new[b];same_data=0
   for block in difflib.SequenceMatcher(None,left['norm'],right['norm'],autojunk=False).get_matching_blocks():
    for i in range(block.a,block.a+block.size):
     x=left['globals'].get(i,0)-0x82000000;y=right['globals'].get(block.b+i-block.a,0)-0x82000000
     if 0<=x<len(od)-32 and 0<=y<len(nd)-32 and len(set(od[x:x+32]))>4 and od[x:x+32]==nd[y:y+32]:same_data+=1
   score=difflib.SequenceMatcher(None,left['norm'],right['norm'],autojunk=False).ratio()
   scores.append((score,same_data,b))
  scores.sort(reverse=True)
  for score,same_data,b in scores[:1]:
   sm=difflib.SequenceMatcher(None,left['norm'],new[b]['norm'],autojunk=False)
   for i in indexes:
    for block in sm.get_matching_blocks():
     j=block.b+i-block.a
     if block.a<=i<block.a+block.size and j in new[b]['globals']:
      rows.append({'old_function':f'{f:08X}','new_function':f'{b:08X}','score':score,'same_data':same_data,'new':f'{new[b]["globals"][j]:08X}','instruction':left['asm'][i]})
 report[value]=rows
(P/'extra-global-evidence.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
