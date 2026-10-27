import struct,re
from pathlib import Path
b=Path(r'C:\Users\Owner\AppData\Local\CrashDumps\xenia_edge.exe.28652.dmp').read_bytes()
u32=lambda p:struct.unpack_from('<I',b,p)[0]
u64=lambda p:struct.unpack_from('<Q',b,p)[0]
s={u32(p):(u32(p+4),u32(p+8)) for p in range(u32(12),u32(12)+12*u32(8),12)}
e=s[6][1]; c=u32(e+164); rsp=u64(c+152)
print('streams',s.keys(),'rsp',hex(rsp))
if 16 in s:
 r=s[16][1]; h,z,n=u32(r),u32(r+4),u64(r+8)
 for p in range(r+h,r+h+z*n,z):
  start,size=u64(p),u64(p+24)
  if start<rsp+0x200000 and start+size>rsp-0x20000: print('region',hex(start),hex(size),'state',hex(u32(p+32)),'protect',hex(u32(p+36)))
syms=[]
lines=Path('edge-symbols.txt').read_text().splitlines()
for i,l in enumerate(lines):
 m=re.search('addr = 0001:(\\d+)',l)
 if m:syms.append((int(m[1])+0x1000,lines[i-1]))
syms.sort()
for address in [0x2500cc,0xc3690]:
 print('symbol',hex(address),[x for x in syms if x[0]<=address][-1:])
if 3 in s:
 r=s[3][1]
 for p in range(r+4,r+4+48*u32(r),48):
  if u32(p)==u32(e):print('thread stack',hex(u64(p+24)),hex(u32(p+32)))
