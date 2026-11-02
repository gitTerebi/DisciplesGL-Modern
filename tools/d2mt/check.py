import bisect, glob, hashlib, json, os, re, struct, sys
import pefile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'src', 'DisciplesGL')
VTABLE_SLOTS = 16


def fieldNames():
    text = open(os.path.join(SRC, 'Hooks.h')).read()
    body = text[text.index('struct AddressSpaceV2 {'):]
    return re.findall(r'DWORD (\w+);', body[:body.index('};')])


def rowHeaders():
    text = open(os.path.join(SRC, 'Hooks.cpp')).read()
    body = text[text.index('const AddressSpaceV2 addressArrayV2[] = {'):]
    pattern = r'NUM\((\d+),\s*(\d+)\),\s*VER\((\d+),\s*(\d+),\s*(\d+),\s*(\d+)\),\s*(0x[0-9A-Fa-f]+)'
    for m in re.finditer(pattern, body[:body.index('\n\t};')]):
        a, b, w, x, y, z = (int(v, 10) for v in m.groups()[:6])
        yield ((a << 16) | b, (w << 16) | x, (y << 16) | z, int(m.group(7), 16))


def rows(dll, names):
    width = 3 + len(names)
    for header in rowHeaders():
        at = dll.find(struct.pack('<4I', *header))
        if at >= 0:
            yield dict(zip(['version', 'ms', 'ls'] + names, struct.unpack_from('<%dI' % width, dll, at)))


def sha256(path):
    return hashlib.sha256(open(path, 'rb').read()).hexdigest()


def checkFolder(snap, allRows, accepted):
    folder = snap['folder']
    exePath = os.path.join(folder, 'Discipl2.exe')
    mssPath = os.path.join(folder, 'Mss32.dll')
    name = os.path.basename(folder)
    if not os.path.exists(exePath) or not os.path.exists(mssPath):
        return ['%s: folder or Mss32.dll missing, skipped' % name], 0
    notes = []
    if sha256(mssPath) != snap['mss32_sha256'] or sha256(exePath) != snap['exe_sha256']:
        notes.append('%s: D2ModdingToolset or the exe changed, rerun: python tools\\d2mt\\snapshot.py "%s"' % (name, folder))

    pe = pefile.PE(exePath, fast_load=True)
    base = pe.OPTIONAL_HEADER.ImageBase
    img = pe.get_memory_mapped_image()
    ms, ls = (int(v, 16) for v in snap['version'])
    row = None
    for r in allRows:
        o = r['check'] + 1 - base
        if r['ms'] == ms and r['ls'] == ls and 0 <= o < len(img) - 4 and struct.unpack_from('<I', img, o)[0] == 0x00CA0000:
            row = r
    if row is None:
        return notes + ['%s: no address row matches this exe' % name], 0

    ranges = [(int(a, 16), int(b, 16)) for a, b in snap['detours']]
    starts = [a for a, _ in ranges]
    rdata = [(base + s.VirtualAddress, base + s.VirtualAddress + s.Misc_VirtualSize) for s in pe.sections if s.Name.startswith(b'.rdata')]

    hits = []
    for field, value in row.items():
        if field in ('version', 'ms', 'ls', 'check') or not value:
            continue
        i = bisect.bisect_right(starts, value) - 1
        if i >= 0 and ranges[i][0] <= value < ranges[i][1]:
            where = 'on the start of' if value == ranges[i][0] else 'inside'
            hits.append((field, '%08X is %s detoured function %08X' % (value, where, ranges[i][0])))
        if any(lo <= value < hi for lo, hi in rdata):
            for slot in range(VTABLE_SLOTS):
                o = value - base + slot * 4
                entry = struct.unpack_from('<I', img, o)[0] if o + 4 <= len(img) else 0
                if entry in starts:
                    hits.append((field, 'slot %d at %08X points to detoured function %08X' % (slot, value + slot * 4, entry)))

    new = 0
    for field, text in hits:
        if field in accepted:
            continue
        notes.append('%s: %s: %s' % (name, field, text))
        new += 1
    notes.append('%s: %d overlaps, %d new' % (name, len(hits), new))
    return notes, new


def main():
    dllPath = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'C4dll-R.dll')
    dll = open(dllPath, 'rb').read()
    allRows = list(rows(dll, fieldNames()))
    accepted = json.load(open(os.path.join(HERE, 'accepted.json')))
    total = 0
    for path in sorted(glob.glob(os.path.join(HERE, '*.json'))):
        if os.path.basename(path) == 'accepted.json':
            continue
        notes, new = checkFolder(json.load(open(path)), allRows, accepted)
        total += new
        for line in notes:
            print('D2MT check: ' + line)
    sys.exit(1 if total else 0)


if __name__ == '__main__':
    main()
