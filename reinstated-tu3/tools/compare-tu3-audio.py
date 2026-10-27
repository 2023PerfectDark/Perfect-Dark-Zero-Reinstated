from pathlib import Path
import re, difflib, json
root=Path(__file__).resolve().parent.parent
def load(folder):
    result={}
    for p in folder.glob('*recomp.*.cpp'):
        for m in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{(.*?)(?=\nDEFINE_REX_FUNC|\Z)',p.read_text(),re.S):
            asm=re.findall(r'^\s*// (.*)',m[2],re.M)
            norm=[re.sub(r'0x[0-9a-f]+','ADDR',x) for x in asm]
            result[int(m[1],16)]=(str(p),asm,norm)
    return result
old=load(root/'compatibility-v16/PerfectDarkZeroRecomp/generated/default')
new=load(root/'compatibility-v16/StockTU3/generated/default')
report=[]
for addr,(_,asm,norm) in sorted(old.items()):
    if not 0x821F0000<=addr<0x821FB000 or len(asm)<15:continue
    candidates=[]
    for n,(_,a,b) in new.items():
        if not 0x821E0000<=n<0x82220000:continue
        if abs(len(a)-len(asm))>max(20,len(asm)*.3):continue
        score=difflib.SequenceMatcher(None,norm,b,autojunk=False).ratio()
        if score>.6:candidates.append((score,n))
    if not candidates:continue
    score,n=max(candidates)
    report.append({'old':hex(addr),'new':hex(n),'score':score,'file':new[n][0],'diff':list(difflib.unified_diff(asm,new[n][1],n=2))})
(root/'staging/tu3-audio-comparison.json').write_text(json.dumps(report,indent=2))
print('Mapped',len(report),'functions')
