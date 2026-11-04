import struct, sys
def cstr(d, p):
    e = d.index(b'\0', p); return d[p:e].decode('latin1'), e + 1
path, target = sys.argv[1], sys.argv[2].upper()
d = open(path, 'rb').read()
recs = {}; p = 28
while p < len(d) - 1:
    rid, size, stored = struct.unpack_from('<iii', d, p + 8); recs[rid] = (p + 28, size); p += 28 + stored
off, _ = recs[2]; n = struct.unpack_from('<i', d, off)[0]; q = off + 4; files = {}; byid = {}
for _ in range(n):
    name = d[q:q+256].split(b'\0')[0].decode('latin1'); fid = struct.unpack_from('<i', d, q + 256)[0]; q += 260
    files[name.upper()] = fid; byid[fid] = name
io, isz = recs[files['-INDEX.OPT']]
cnt = struct.unpack_from('<i', d, io)[0]; q = io + 4; index = {}
for _ in range(cnt):
    iid = struct.unpack_from('<i', d, q)[0]; nm, q = cstr(d, q + 4); q += 8
    index.setdefault(nm.upper(), []).append(iid)
mo, msz = recs[files['-IMAGES.OPT']]; q = mo; end = mo + msz - 1
while q < end:
    sx, sy = struct.unpack_from('<ii', d, q + 3); q += 11 + 1024
    fc = struct.unpack_from('<i', d, q)[0]; q += 4; frames = []
    for _ in range(fc):
        nm, q = cstr(d, q); pc, w, h = struct.unpack_from('<iii', d, q); q += 12 + pc * 24; frames.append(nm.upper())
    if target in frames:
        print('block size', sx, sy, 'frames', len(frames))
        for f in frames:
            ids = index.get(f, [])
            print('  ', f, [(i, byid.get(i, '?')) for i in ids])
