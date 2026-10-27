"""Launch PDZ under Windows debug events; capture fatal exceptions and exit status."""
import ctypes as C, ctypes.wintypes as W, pathlib, time, json
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'staging/surface-cabin-capture';out.mkdir(exist_ok=True)
k=C.WinDLL('kernel32',use_last_error=True);d=C.WinDLL('dbghelp',use_last_error=True)
class SI(C.Structure):
 _fields_=[('cb',W.DWORD),('reserved',W.LPWSTR),('desktop',W.LPWSTR),('title',W.LPWSTR),('x',W.DWORD),('y',W.DWORD),('xs',W.DWORD),('ys',W.DWORD),('xc',W.DWORD),('yc',W.DWORD),('fill',W.DWORD),('flags',W.DWORD),('show',W.WORD),('cb2',W.WORD),('reserved2',C.c_void_p),('stdin',W.HANDLE),('stdout',W.HANDLE),('stderr',W.HANDLE)]
class PI(C.Structure):_fields_=[('process',W.HANDLE),('thread',W.HANDLE),('pid',W.DWORD),('tid',W.DWORD)]
class EVENT(C.Structure):_fields_=[('code',W.DWORD),('pid',W.DWORD),('tid',W.DWORD),('align',W.DWORD),('data',C.c_ubyte*160)]
k.CreateProcessW.argtypes=[W.LPCWSTR,W.LPWSTR,C.c_void_p,C.c_void_p,W.BOOL,W.DWORD,C.c_void_p,W.LPCWSTR,C.POINTER(SI),C.POINTER(PI)]
k.WaitForDebugEvent.argtypes=[C.POINTER(EVENT),W.DWORD];k.ContinueDebugEvent.argtypes=[W.DWORD,W.DWORD,W.DWORD]
k.CloseHandle.argtypes=[W.HANDLE]
d.MiniDumpWriteDump.argtypes=[W.HANDLE,W.DWORD,W.HANDLE,W.DWORD,C.c_void_p,C.c_void_p,C.c_void_p]
exe=pathlib.Path('H:/Perfect Dark Zero Xbox360/Perfect Dark Zero Reinstated/PDZReinstatedTU3.exe')
si=SI();si.cb=C.sizeof(si);pi=PI()
if not k.CreateProcessW(str(exe),C.create_unicode_buffer('"'+str(exe)+'"'),None,None,False,2,None,str(exe.parent),C.byref(si),C.byref(pi)):raise C.WinError(C.get_last_error())
log=out/('events-'+str(pi.pid)+'.jsonl')
def record(**v):
 v['time']=time.strftime('%Y-%m-%d %H:%M:%S')
 with log.open('a') as f:f.write(json.dumps(v)+'\n')
 print(json.dumps(v),flush=True)
record(event='started',pid=pi.pid)
count=0
while True:
 e=EVENT()
 if not k.WaitForDebugEvent(C.byref(e),1000):continue
 b=bytes(e.data);status=0x10002;finish=False
 if e.code==1:
  code=int.from_bytes(b[:4],'little');address=int.from_bytes(b[16:24],'little');first=int.from_bytes(b[152:156],'little')
  if code not in (0x80000003,0x80000004):
   status=0x80010001
   fatal=not first or code in (0xC0000409,0xC0000602)
   if fatal or count<30:record(event='exception',code=hex(code),address=hex(address),first_chance=first,thread=e.tid)
   count+=1
   if fatal:
    import msvcrt
    path=out/('PDZ-'+str(pi.pid)+'-'+str(e.tid)+'.dmp')
    with path.open('wb') as f:
     ok=d.MiniDumpWriteDump(pi.process,pi.pid,msvcrt.get_osfhandle(f.fileno()),0x1041,None,None,None)
    record(event='dump',path=str(path),success=bool(ok),error=C.get_last_error())
 elif e.code==5:
  record(event='exit',code=hex(int.from_bytes(b[:4],'little')));finish=True
 elif e.code in (3,6):
  handle=int.from_bytes(b[:8],'little')
  if handle:k.CloseHandle(handle)
 k.ContinueDebugEvent(e.pid,e.tid,status)
 if finish:break
k.CloseHandle(pi.thread);k.CloseHandle(pi.process)
