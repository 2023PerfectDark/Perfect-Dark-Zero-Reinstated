"""Read-only inspection of the isolated TU3 test process."""
import ctypes,struct,sys,json
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.restype=ctypes.c_void_p
k.ReadProcessMemory.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.CloseHandle.argtypes=[ctypes.c_void_p]
h=k.OpenProcess(0x1010,False,int(sys.argv[1]))
if not h:raise ctypes.WinError(ctypes.get_last_error())
def read(a,size):
 b=ctypes.create_string_buffer(size);n=ctypes.c_size_t()
 if not k.ReadProcessMemory(h,0x100000000+a,b,size,ctypes.byref(n)):raise ctypes.WinError(ctypes.get_last_error())
 return b.raw
try:
 rows=[]
 for i in range(6):
  config=struct.unpack('>12I',read(0x82D28458+i*48,48))
  row={'heap':i,'type':config[2],'bytes':config[4],'config':[hex(x) for x in config]}
  if config[6]:row['runtime']=list(struct.unpack('>32I',read(config[6],128)))
  rows.append(row)
 print(json.dumps(rows,indent=2))
 u32=lambda a:struct.unpack('>I',read(a,4))[0]
 def string(a):
  if not a:return ''
  return read(a,192).split(b'\0')[0].decode('ascii',errors='replace')
 manager=u32(0x82D6BFE0);vector=u32(manager);items=u32(vector);count=u32(vector+8)
 for i in range(min(count,256)):
  definition=u32(items+i*4);body=u32(definition)
  if body not in (6,50,80,102,103,109,110,111):continue
  fields={str(off):string(u32(definition+off)) for off in (4,16,32,44,52)}
  print('body',body,hex(definition),fields)
finally:k.CloseHandle(h)
