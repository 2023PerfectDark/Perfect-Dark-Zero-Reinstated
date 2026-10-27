import json,re
for function in json.load(open('recomp-index.json')):
    regs={};hits=[]
    for line in function['asm'].splitlines():
        m=re.fullmatch(r'lis (r\d+),(-?\d+)',line)
        if m: regs[m[1]]=(int(m[2])<<16)&0xffffffff;continue
        m=re.fullmatch(r'addi (r\d+),(r\d+),(-?\d+)',line)
        if m:
            value=regs.get(m[2]);regs[m[1]]=((value+int(m[3]))&0xffffffff) if isinstance(value,int) else None;continue
        m=re.fullmatch(r'mr (r\d+),(r\d+)',line)
        if m: regs[m[1]]=regs.get(m[2]);continue
        m=re.fullmatch(r'(lwz|lbz|stw|stb) (r\d+),(-?\d+)\((r\d+)\)',line)
        if m:
            op,target,off,base=m.groups();value=regs.get(base);off=int(off)
            if value=='session' and off==24:hits.append(line)
            if op in ('lwz','lbz'):
                regs[target]='session' if isinstance(value,int) and ((value+off)&0xffffffff)==0x82d6b2b0 else None
            continue
        if line.startswith(('bl ','bctrl')):
            for n in range(3,13):regs.pop('r'+str(n),None)
        else:
            m=re.match(r'\w+ (r\d+),',line)
            if m:regs.pop(m[1],None)
    if hits:print(function['name'], '; '.join(hits))
