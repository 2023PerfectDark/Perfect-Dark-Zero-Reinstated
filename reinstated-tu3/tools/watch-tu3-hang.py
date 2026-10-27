"""Read-only, bounded hang capture for an already-running test game."""
import ctypes as c
from ctypes import wintypes as w
import datetime, json, pathlib, sys, time

pid = int(sys.argv[1])
destination = pathlib.Path(sys.argv[2])
destination.mkdir(parents=True, exist_ok=True)
kernel = c.WinDLL('kernel32', use_last_error=True)
user = c.WinDLL('user32', use_last_error=True)
advapi = c.WinDLL('advapi32', use_last_error=True)
dbghelp = c.WinDLL('dbghelp', use_last_error=True)
kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
kernel.OpenProcess.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
dbghelp.MiniDumpWriteDump.argtypes = [w.HANDLE, w.DWORD, w.HANDLE, w.DWORD, c.c_void_p, c.c_void_p, c.c_void_p]
dbghelp.MiniDumpWriteDump.restype = w.BOOL
user.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, c.c_size_t, c.c_ssize_t, w.UINT, w.UINT, c.POINTER(c.c_size_t)]
user.SendMessageTimeoutW.restype = c.c_ssize_t
user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
user.GetWindowThreadProcessId.restype = w.DWORD
user.IsWindowVisible.argtypes = [w.HWND]
user.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
callback_type = c.WINFUNCTYPE(w.BOOL, w.HWND, c.c_ssize_t)
user.EnumWindows.argtypes = [callback_type, c.c_ssize_t]

class ThreadEntry(c.Structure):
    _fields_ = [('size', w.DWORD), ('usage', w.DWORD), ('id', w.DWORD),
                ('pid', w.DWORD), ('priority', w.LONG), ('delta', w.LONG), ('flags', w.DWORD)]
class LockInfo(c.Structure):
    _fields_ = [('name', w.WCHAR * 128), ('timeout', c.c_int64), ('alertable', w.BOOL)]
class ThreadInfo(c.Structure):
    _fields_ = [('pid', w.DWORD), ('tid', w.DWORD), ('wait_ms', w.DWORD), ('switches', w.DWORD)]
class NodeInfo(c.Union):
    _fields_ = [('lock', LockInfo), ('thread', ThreadInfo)]
class Node(c.Structure):
    _fields_ = [('kind', c.c_int), ('status', c.c_int), ('info', NodeInfo)]
assert c.sizeof(Node) == 280
kernel.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
kernel.CreateToolhelp32Snapshot.restype = w.HANDLE
kernel.Thread32First.argtypes = [w.HANDLE, c.POINTER(ThreadEntry)]
kernel.Thread32Next.argtypes = [w.HANDLE, c.POINTER(ThreadEntry)]
advapi.OpenThreadWaitChainSession.argtypes = [w.DWORD, c.c_void_p]
advapi.OpenThreadWaitChainSession.restype = w.HANDLE
advapi.CloseThreadWaitChainSession.argtypes = [w.HANDLE]
advapi.GetThreadWaitChain.argtypes = [w.HANDLE, c.c_size_t, w.DWORD, w.DWORD,
                                    c.POINTER(w.DWORD), c.POINTER(Node), c.POINTER(w.BOOL)]

def windows():
    result = []
    @callback_type
    def visit(hwnd, unused):
        owner = w.DWORD()
        tid = user.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            name = c.create_unicode_buffer(512)
            user.GetWindowTextW(hwnd, name, len(name))
            if name.value:
                result.append((hwnd, tid, name.value))
        return True
    user.EnumWindows(visit, 0)
    return result

def waits():
    result = []
    snapshot = kernel.CreateToolhelp32Snapshot(4, 0)
    session = advapi.OpenThreadWaitChainSession(0, None)
    if not session:
        raise c.WinError(c.get_last_error())
    try:
        entry = ThreadEntry(size=c.sizeof(ThreadEntry))
        more = kernel.Thread32First(snapshot, c.byref(entry))
        while more:
            if entry.pid == pid:
                nodes = (Node * 16)()
                count, cycle = w.DWORD(16), w.BOOL()
                ok = advapi.GetThreadWaitChain(session, 0, 7, entry.id,
                                               c.byref(count), nodes, c.byref(cycle))
                row = dict(tid=entry.id, ok=bool(ok), cycle=bool(cycle.value), nodes=[])
                if ok:
                    for node in nodes[:count.value]:
                        data = dict(kind=node.kind, status=node.status)
                        if node.kind == 8:  # WctThreadType
                            data.update(pid=node.info.thread.pid, tid=node.info.thread.tid,
                                        wait_ms=node.info.thread.wait_ms, switches=node.info.thread.switches)
                        else:
                            data.update(name=node.info.lock.name)
                        row['nodes'].append(data)
                else:
                    row['error'] = c.get_last_error()
                result.append(row)
            more = kernel.Thread32Next(snapshot, c.byref(entry))
    finally:
        advapi.CloseThreadWaitChainSession(session)
        kernel.CloseHandle(snapshot)
    return result

process = kernel.OpenProcess(0x100000 | 0x410, False, pid)
if not process:
    raise c.WinError(c.get_last_error())
try:
    baseline = dict(time=datetime.datetime.now().isoformat(), pid=pid, windows=windows(), waits=waits())
    (destination/'baseline.json').write_text(json.dumps(baseline, indent=2))
    print('Watching existing game for up to 15 minutes; no input or process changes.', flush=True)
    misses = 0
    deadline = time.monotonic() + 900
    while time.monotonic() < deadline and kernel.WaitForSingleObject(process, 0) == 258:
        current = windows()
        healthy = True
        for hwnd, tid, title in current:
            answer = c.c_size_t()
            if not user.SendMessageTimeoutW(hwnd, 0, 0, 0, 0x22, 700, c.byref(answer)):
                healthy = False
        misses = 0 if healthy or not current else misses + 1
        if misses >= 4:
            stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
            report = dict(time=stamp, pid=pid, windows=current, waits=waits())
            (destination/(stamp+'.json')).write_text(json.dumps(report, indent=2))
            dump = destination/(stamp+'.dmp')
            handle = kernel.CreateFileW(str(dump.resolve()), 0x40000000, 0, None, 1, 0x80, None)
            try:
                ok = dbghelp.MiniDumpWriteDump(process, pid, handle, 0x1000, None, None, None)
                print('Captured hang:', stamp, 'stack dump:', bool(ok), 'error:', c.get_last_error() if not ok else 0, flush=True)
            finally:
                kernel.CloseHandle(handle)
            break
        time.sleep(1)
    else:
        print('Watch ended without a sustained window stall.', flush=True)
finally:
    kernel.CloseHandle(process)
