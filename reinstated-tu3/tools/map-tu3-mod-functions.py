"""Match compiled functions for the TU3 port; report evidence, never patch code.

Unique normalized instruction matches seed the map. Matching call graphs then
locate changed callees for review. Candidate matches are not approved hooks.
"""
from pathlib import Path
from collections import defaultdict, Counter
import bisect
import difflib
import json
import re

ROOT = Path(__file__).resolve().parent.parent
TARGET = ROOT / 'compatibility-v16/ModdedTU3'
def load(folder, backup=None):
    result = {}
    for path in folder.glob('*recomp.*.cpp'):
        read = backup / path.name if backup and (backup / path.name).exists() else path
        text = read.read_text()
        for match in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{(.*?)(?=\nDEFINE_REX_FUNC|\Z)', text, re.S):
            address = int(match[1], 16)
            asm = re.findall(r'^\s*// ([a-z][a-z0-9.]*\s.*|blr\s*)$', match[2], re.M)
            if not asm:
                continue
            calls = [int(x,16) for x in re.findall(r'\bsub_([0-9A-F]{8})\(ctx, base\)', match[2])]
            normalized = []
            absolute = {}
            global_refs = {}
            for instruction in asm:
                instruction = instruction.rstrip()
                original = instruction
                lis = re.fullmatch(r'lis r(\d+),(-?\d+)', instruction)
                if lis:
                    absolute[int(lis[1])] = (int(lis[2]) << 16) & 0xffffffff
                else:
                    mem = re.fullmatch(r'(\w+) ([^,]+),(-?\d+)\(r(\d+)\)', instruction)
                    add = re.fullmatch(r'addi r(\d+),r(\d+),(-?\d+)', instruction)
                    if mem and int(mem[4]) in absolute:
                        addr=(absolute[int(mem[4])]+int(mem[3]))&0xffffffff
                        if 0x82000000<=addr<0x84000000:
                            global_refs[len(normalized)]=addr
                            instruction=f'{mem[1]} {mem[2]},GLOBAL(r{mem[4]})'
                    if add and int(add[2]) in absolute:
                        addr=(absolute[int(add[2])]+int(add[3]))&0xffffffff
                        if 0x82000000<=addr<0x84000000:
                            global_refs[len(normalized)]=addr
                            instruction=f'addi r{add[1]},r{add[2]},GLOBAL'
                            absolute[int(add[1])]=addr
                    dest=re.match(r'(\w+) r(\d+),',original)
                    if dest and not (add and 'GLOBAL' in instruction) and not dest[1].startswith(('st','cmp')):
                        absolute.pop(int(dest[2]),None)
                instruction = re.sub(r'0x[0-9a-f]+', 'ADDR', instruction)
                # Addresses are reconstructed by lis plus signed low-half.
                # Keep all other immediates/offsets to expose layout changes.
                instruction = re.sub(r'^(lis r\d+),-?\d+$', r'\1,HIGH', instruction)
                normalized.append(instruction)
                if original.startswith(('bl ', 'bctrl')):
                    for register in range(3,13):absolute.pop(register,None)
            imports = re.findall(r'\b(__imp__(?!sub_)[A-Za-z0-9_]+)\(ctx, base\)', match[2])
            normalized.extend('IMPORT '+name for name in imports)
            result[address] = {'asm': asm, 'norm': normalized, 'calls': calls, 'globals':global_refs,
                               'file': path.name, 'line': text.count('\n',0,match.start())+1}
    return result

def run():
    old = load(ROOT/'compatibility-v16/PerfectDarkZeroRecomp/generated/default')
    new = load(ROOT/'compatibility-v16/StockTU3/generated/default', ROOT/'staging/tu3-audio-original-generated')
    oi, ni = defaultdict(list), defaultdict(list)
    for address, f in old.items(): oi[tuple(f['norm'])].append(address)
    for address, f in new.items(): ni[tuple(f['norm'])].append(address)
    matches, evidence = {}, {}
    for key, addresses in oi.items():
        if (len(key)>=8 or any(x.startswith('IMPORT ') for x in key)) and len(addresses)==1 and len(ni[key])==1:
            matches[addresses[0]]=ni[key][0]
            evidence[addresses[0]]='unique instruction sequence (branch targets/high halves normalized)'
    # Every aligned call site is evidence for its callee, not just a guessed
    # address delta. Accept only reciprocal, unambiguous, exact-body matches.
    candidates = defaultdict(Counter)
    for iteration in range(5):
        candidates = defaultdict(Counter)
        for a,b in matches.items():
            left,right=old[a],new[b]
            if len(left['calls'])!=len(right['calls']):continue
            for c,d in zip(left['calls'],right['calls']):
                if c in old and d in new:candidates[c][d]+=1
        added=0
        used=set(matches.values())
        for a, votes in candidates.items():
            if a in matches or len(votes)!=1:continue
            b,count=votes.most_common(1)[0]
            if b in used:continue
            if old[a]['norm']==new[b]['norm']:
                matches[a]=b;used.add(b);added+=1
                evidence[a]=f'identical normalized body + {count} aligned caller(s)'
        if not added:break
    inventory=json.loads((TARGET/'port-inventory.json').read_text())
    needed={int(x,16) for x in inventory['function_references']}
    report=[]
    anchors=sorted(matches)
    new_order=sorted(new)
    for a in sorted(needed):
        row={'old':f'{a:08X}', 'files':inventory['function_references'][f'{a:08X}'],
             'hook':f'{a:08X}' in inventory['hooks']}
        if a not in old:
            row['status']='missing original function';report.append(row);continue
        if a in matches:
            b=matches[a]
            row.update(new=f'{b:08X}',status='instruction match',evidence=evidence[a])
        else:
            pool=set(candidates[a])
            i=bisect.bisect_left(anchors,a)
            near=anchors[max(0,i-2):i+2]
            estimates=[a+matches[x]-x for x in near]
            for estimate in estimates:
                lo=bisect.bisect_left(new_order,estimate-0x1800)
                hi=bisect.bisect_right(new_order,estimate+0x1800)
                pool.update(new_order[lo:hi])
            ranked=[]
            left=old[a]['norm']
            for b in pool:
                right=new[b]['norm']
                if abs(len(left)-len(right))>max(12,len(left)*.5):continue
                score=difflib.SequenceMatcher(None,left,right,autojunk=False).ratio()
                ranked.append((score,candidates[a][b],b))
            ranked.sort(reverse=True)
            row['status']='needs review'
            row['candidates']=[{'new':f'{b:08X}','score':round(score,5),'caller_votes':v} for score,v,b in ranked[:3]]
            if ranked:
                score,v,b=ranked[0]
                row['diff']=list(difflib.unified_diff(old[a]['asm'],new[b]['asm'],n=2))
                row['semantic_diff']=list(difflib.unified_diff(old[a]['norm'],new[b]['norm'],n=1))
        report.append(row)
    globals_map=defaultdict(Counter)
    for a,b in matches.items():
        for index,address in old[a]['globals'].items():
            if index in new[b]['globals']:globals_map[address][new[b]['globals'][index]]+=1
    code_literals={}
    old_order=sorted(old)
    for literal,files in inventory['address_literals'].items():
        address=int(literal,16)
        if files==['blood_decal_pixels.inl'] or not 0x82140000<=address<0x82CE62AC:continue
        i=bisect.bisect_right(old_order,address)-1
        if i<0:continue
        a=old_order[i]
        index=(address-a)//4
        if (address-a)%4 or index>len(old[a]['asm']):continue
        b=matches.get(a)
        if b is None:
            row=next((r for r in report if r['old']==f'{a:08X}'),None)
            if row and row.get('candidates'):b=int(row['candidates'][0]['new'],16)
        if b is None:continue
        if address==a:
            code_literals[literal]={'new':f'{b:08X}','function':f'{a:08X}','kind':'function identity'}
            continue
        # lr names the instruction AFTER a call. Map that call, not a nearby
        # instruction that could have moved to a different basic block.
        point=index-1
        for block in difflib.SequenceMatcher(None,old[a]['norm'],new[b]['norm'],autojunk=False).get_matching_blocks():
            if block.a<=point<block.a+block.size:
                target=b+4*(block.b+point-block.a+1)
                code_literals[literal]={'new':f'{target:08X}','function':f'{a:08X}',
                    'kind':'aligned preceding instruction','instruction':old[a]['asm'][point]}
                break
    out={'instruction_matches':len(matches),'required':len(needed),
         'matched_required':sum(r['status']=='instruction match' for r in report),
         'functions':report,'anchors':{f'{a:08X}':f'{b:08X}' for a,b in sorted(matches.items())},
         'code_literal_candidates':code_literals,
         'global_candidates':{f'{a:08X}':{f'{b:08X}':votes for b,votes in counts.items()} for a,counts in sorted(globals_map.items())}}
    (TARGET/'function-map-review.json').write_text(json.dumps(out,indent=2))
    # Small reusable data set for inspecting only mod dependencies.
    (TARGET/'required-function-evidence.json').write_text(json.dumps({
        f'{a:08X}':{'old':old.get(a), 'new':{f'{b:08X}':new[b] for b in
        ([matches[a]] if a in matches else [int(c['new'],16) for c in next(r for r in report if r['old']==f'{a:08X}').get('candidates',[])])}}
        for a in sorted(needed)},indent=2))
    print({k:v for k,v in out.items() if k not in {'functions','anchors','global_candidates'}})

if __name__=='__main__':run()
