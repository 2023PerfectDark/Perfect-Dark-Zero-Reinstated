from pathlib import Path
import struct,re,sys,bisect
b=Path(sys.argv[1]).read_bytes();u32=lambda p:struct.unpack_from('<I',b,p)[0];u64=lambda p:struct.unpack_from('<Q',b,p)[0]
streams={u32(p):(u32(p+4),u32(p+8)) for p in range(u32(12),u32(12)+12*u32(8),12)}
mods=[];r=streams[4][1]
for p in range(r+4,r+4+108*u32(r),108):
 name=u32(p+20);name=b[name+4:name+4+u32(name)].decode('utf-16-le');mods.append((u64(p),u32(p+8),name))
symbols=[];maptext=Path(sys.argv[2]).read_text()
preferred=int(re.search(r'Preferred load address is ([0-9a-fA-F]+)',maptext)[1],16)
for line in maptext.splitlines():
 m=re.match(r'\s+[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{16})',line)
 if m:symbols.append((int(m[2],16)-preferred,m[1]))
symbols.sort();offsets=[x[0] for x in symbols]
def sym(a):
 for base,size,name in mods:
  if base<=a<base+size:
   off=a-base
   if 'pdzmoddedtu3' in name.lower():
    i=bisect.bisect_right(offsets,off)-1
    if i>=0:return f'{Path(name).name}+{off:X} {symbols[i][1]}+{off-symbols[i][0]:X}'
   return f'{Path(name).name}+{off:X}'
 return None
e=streams[6][1];context=u32(e+164);rsp=u64(context+152);rip=u64(context+248)
print('Exception',hex(u32(e+8)),'address',hex(u64(e+24)),sym(u64(e+24)))
print('RIP',hex(rip),sym(rip),'RSP',hex(rsp))
for i,name in enumerate(('rax','rcx','rdx','rbx','rsp','rbp','rsi','rdi','r8','r9','r10','r11','r12','r13','r14','r15')):print(name,hex(u64(context+120+i*8)))
segments=[]
if 5 in streams:
 r=streams[5][1]
 for p in range(r+4,r+4+16*u32(r),16):segments.append((u64(p),u32(p+8),u32(p+12)))
if 9 in streams:
 r=streams[9][1];data=u64(r+8)
 for p in range(r+16,r+16+16*u64(r),16):
  a,size=u64(p),u64(p+8);segments.append((a,size,data));data+=size
def readmem(address,length):
 for a,size,data in segments:
  if a<=address and address+length<=a+size:return b[data+address-a:data+address-a+length]
 raise ValueError(hex(address)+' absent from dump')
try:
 ppc=u64(context+176) # rdi points to PPCContext in this generated function.
 regs=struct.unpack('<32Q',readmem(ppc,256))
 print('Guest registers',[(('r3' if i==0 else 'r0' if i==1 else 'r1' if i==2 else 'r2' if i==3 else 'r'+str(i)),hex(x)) for i,x in enumerate(regs)])
 definition=regs[31]&0xffffffff
 values=struct.unpack('>42I',readmem(0x100000000+definition,168))
 print('Definition',hex(definition),[hex(x) for x in values])
 for off in (4,16,32,44,52,80,84,88,92,96):
  try:print('name',off,readmem(0x100000000+values[off//4],96).split(b'\0')[0])
  except ValueError:pass
except ValueError as error:print(error)
for a,size,data in segments:
 if a<=rsp<a+size:
  start=data+rsp-a
  for p in range(start,min(data+size,start+8192),8):
   address=u64(p);s=sym(address)
   if s:print(f'stack+{p-start:04X}',hex(address),s)
