# Model the native linked-list layout and its append/erase/reset operations.
CAP=1024
for base in (0x82E446D8,0x82E43FB8):
    m={}; sentinel=base+1800; extra=0x40000000
    def reset():
        for i in range(150):
            n=base+i*12;m[n+4]=n-12 if i else sentinel;m[n+8]=n+12
        m[sentinel+4]=base+1788;m[base+1808]=base;m[base+1812]=base;m[base+1816]=0
        for i in range(CAP-150):
            n=extra+i*12;m[n+4]=n-12 if i else base+1788;m[n+8]=n+12 if i+1<CAP-150 else sentinel
        m[base+1796]=extra;m[sentinel+4]=extra+(CAP-151)*12
    for cycle in range(3):
        reset(); used=[]
        for i in range(CAP):
            n=m[base+1812];assert n!=sentinel
            m[base+1812]=m[n+8];m[base+1816]+=1;used.append(n)
        assert m[base+1812]==sentinel and len(set(used))==CAP
        for n in used[::2]+used[1::2]:
            boundary=m[base+1812]
            if n!=m[boundary+4]:
                prev,nxt=m[n+4],m[n+8];m[prev+8]=nxt;m[nxt+4]=prev
                tail=m[boundary+4];m[n+4]=tail;m[tail+8]=n;m[n+8]=boundary;m[boundary+4]=n
            m[base+1812]=n;m[base+1816]-=1
        assert m[base+1816]==0
print('Expanded native list: 1024 allocations, mixed-order removal, 3 resets passed for both registries')
