"""Collect additional relocation evidence for static tables and caller sites."""
from pathlib import Path
import bisect, difflib, importlib.util, json, re, struct
ROOT=Path(__file__).resolve().parent.parent
P=ROOT/'compatibility-v16/ModdedTU3'
spec=importlib.util.spec_from_file_location('mapping',ROOT/'tools/map-tu3-mod-functions.py')
mapping=importlib.util.module_from_spec(spec);spec.loader.exec_module(mapping)
review=json.loads((P/'function-map-review.json').read_text())
reloc=json.loads((P/'relocation-review.json').read_text())
anchors={int(a,16):int(b,16) for a,b in review['anchors'].items()}
anchors.update({int(a,16):int(b,16) for a,b in reloc['functions'].items()})
old_data=(ROOT/'staging/audio-base-data.bin').read_bytes()
new_data=(ROOT/'staging/audio-tu3-data.bin').read_bytes()
result={}
for literal in reloc['unresolved_addresses']:
    a=int(literal,16)
    if 0x82000000<=a<0x82140000:
        words=struct.unpack_from('>12I',old_data,a-0x82000000)
        mapped=[(i,anchors[x]) for i,x in enumerate(words[:8]) if x in anchors]
        votes={}
        for i,b in mapped:
            token=struct.pack('>I',b);pos=-1
            while True:
                pos=new_data.find(token,pos+1)
                if pos<0:break
                candidate=pos-i*4
                if candidate<0 or candidate+32>len(new_data):continue
                score=sum(struct.unpack_from('>I',new_data,candidate+j*4)[0]==v for j,v in mapped)
                if score>=3:votes[candidate]=score
        ranked=sorted(votes,key=votes.get,reverse=True)
        result[literal]={'kind':'static function-pointer table',
            'old_words':[f'{x:08X}' for x in words],
            'candidates':[{'new':f'{0x82000000+x:08X}','matching_pointers':votes[x]} for x in ranked[:4]]}
old=mapping.load(ROOT/'compatibility-v16/PerfectDarkZeroRecomp/generated/default')
new=mapping.load(ROOT/'compatibility-v16/StockTU3/generated/default',ROOT/'staging/tu3-audio-original-generated')
old_order=sorted(old);new_order=sorted(new);ordered=sorted(anchors)
for literal in reloc['unresolved_addresses']:
    a=int(literal,16)
    if not 0x82140000<=a<0x82CE62AC:continue
    f=old_order[bisect.bisect_right(old_order,a)-1]
    ix=(a-f)//4-1
    if ix<0 or ix>=len(old[f]['asm']):continue
    i=bisect.bisect_left(ordered,f)
    neighbors=ordered[max(0,i-2):i+2]
    pool=set()
    for n in neighbors:
        estimate=f+anchors[n]-n
        pool.update(new_order[bisect.bisect_left(new_order,estimate-0x1800):bisect.bisect_right(new_order,estimate+0x1800)])
    scored=[]
    for b in pool:
        if abs(len(old[f]['asm'])-len(new[b]['asm']))>max(20,len(old[f]['asm'])*.5):continue
        scored.append((difflib.SequenceMatcher(None,old[f]['norm'],new[b]['norm'],autojunk=False).ratio(),b))
    scored.sort(reverse=True)
    row={'kind':'return address','old_function':f'{f:08X}','old_instruction':old[f]['asm'][ix],'candidates':[]}
    for score,b in scored[:2]:
        for block in difflib.SequenceMatcher(None,old[f]['norm'],new[b]['norm'],autojunk=False).get_matching_blocks():
            if block.a<=ix<block.a+block.size:
                j=block.b+ix-block.a
                row['candidates'].append({'function':f'{b:08X}','new':f'{b+4*(j+1):08X}',
                    'score':score,'new_instruction':new[b]['asm'][j]});break
    result[literal]=row
(P/'additional-address-evidence.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
