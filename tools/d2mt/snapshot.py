import bisect, ctypes, ctypes.wintypes as wt, hashlib, json, os, re, struct, subprocess, sys, time
import pefile

HERE = os.path.dirname(os.path.abspath(__file__))

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
psapi = ctypes.WinDLL('psapi', use_last_error=True)
k32.OpenProcess.restype = wt.HANDLE
k32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
psapi.EnumProcessModulesEx.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.DWORD]
psapi.GetModuleBaseNameW.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.LPWSTR, wt.DWORD]


class ModuleInfo(ctypes.Structure):
    _fields_ = [('base', ctypes.c_void_p), ('size', wt.DWORD), ('entry', ctypes.c_void_p)]


psapi.GetModuleInformation.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(ModuleInfo), wt.DWORD]


def sha256(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def slug(folder):
    name = re.sub(r'[^a-z0-9]+', '-', os.path.basename(os.path.normpath(folder)).lower()).strip('-')
    return re.sub(r'^disciples-2-?', '', name) or 'disciples-2'


def modules(handle):
    mods = (ctypes.c_void_p * 1024)()
    need = wt.DWORD()
    psapi.EnumProcessModulesEx(handle, mods, ctypes.sizeof(mods), ctypes.byref(need), 1)
    found = {}
    for m in mods[:need.value // ctypes.sizeof(ctypes.c_void_p)]:
        name = ctypes.create_unicode_buffer(260)
        psapi.GetModuleBaseNameW(handle, m, name, 260)
        info = ModuleInfo()
        psapi.GetModuleInformation(handle, m, ctypes.byref(info), ctypes.sizeof(info))
        found[name.value.lower()] = (info.base, info.size)
    return found


def read(handle, addr, size):
    buf = ctypes.create_string_buffer(size)
    got = ctypes.c_size_t()
    k32.ReadProcessMemory(handle, addr, buf, size, ctypes.byref(got))
    return buf.raw[:got.value]


def detours(pe, img, base, mem, target):
    text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
    lo = text.VirtualAddress
    hi = lo + text.Misc_VirtualSize - 5
    starts = []
    i = mem.find(b'\xE9', lo)
    while 0 <= i < hi:
        if mem[i:i + 5] != img[i:i + 5]:
            dest = (base + i + 5 + struct.unpack_from('<i', mem, i + 1)[0]) & 0xFFFFFFFF
            if target[0] <= dest < target[1]:
                starts.append(base + i)
        i = mem.find(b'\xE9', i + 1)
    return starts


def functionStarts(pe, img, base):
    text = next(s for s in pe.sections if s.Name.startswith(b'.text'))
    tlo = base + text.VirtualAddress
    thi = tlo + text.Misc_VirtualSize
    starts = set()
    i = img.find(b'\xE8', text.VirtualAddress)
    end = text.VirtualAddress + text.Misc_VirtualSize - 5
    while 0 <= i < end:
        dest = (base + i + 5 + struct.unpack_from('<i', img, i + 1)[0]) & 0xFFFFFFFF
        if tlo <= dest < thi:
            starts.add(dest)
        i = img.find(b'\xE8', i + 1)
    for s in pe.sections:
        if s.Name.startswith(b'.rdata'):
            for o in range(s.VirtualAddress, s.VirtualAddress + s.Misc_VirtualSize - 4, 4):
                v = struct.unpack_from('<I', img, o)[0]
                if tlo <= v < thi:
                    starts.add(v)
    return sorted(starts)


def main():
    folder = sys.argv[1]
    timeout = float(sys.argv[2]) if len(sys.argv) > 2 else 30
    exePath = os.path.join(folder, 'Discipl2.exe')
    mssPath = os.path.join(folder, 'Mss32.dll')
    pe = pefile.PE(exePath)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()

    proc = subprocess.Popen([exePath], cwd=folder)
    try:
        handle = k32.OpenProcess(0x0410, False, proc.pid)
        found = []
        stable = 0
        deadline = time.time() + timeout
        while time.time() < deadline and stable < 4:
            time.sleep(0.5)
            mods = modules(handle)
            if 'discipl2.exe' not in mods or 'mss32.dll' not in mods:
                continue
            mss = mods['mss32.dll']
            mem = read(handle, base, mods['discipl2.exe'][1])
            now = detours(pe, img, base, mem, (mss[0], mss[0] + mss[1]))
            stable = stable + 1 if now and now == found else 0
            found = now
    finally:
        proc.kill()

    if not found:
        sys.exit('No D2ModdingToolset detours found in %s' % folder)

    starts = functionStarts(pe, img, base)
    ranges = []
    for f in found:
        i = bisect.bisect_right(starts, f)
        ranges.append(['%08X' % f, '%08X' % (starts[i] if i < len(starts) else f + 5)])

    info = pe.VS_FIXEDFILEINFO[0]
    out = os.path.join(HERE, slug(folder) + '.json')
    json.dump({
        'folder': folder,
        'exe_sha256': sha256(exePath),
        'mss32_sha256': sha256(mssPath),
        'version': ['%08X' % info.FileVersionMS, '%08X' % info.FileVersionLS],
        'detours': ranges,
    }, open(out, 'w'), indent=1)
    print('%s: %d detours -> %s' % (folder, len(ranges), out))


if __name__ == '__main__':
    main()
