import struct, sys, os, json

def cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin1'), e + 1

def load(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'MQDB'
    recs = {}
    p = 28
    while p < len(d) - 1:
        assert d[p:p+4] == b'MQRC', (path, p)
        rid, size, stored = struct.unpack_from('<iii', d, p + 8)
        recs[rid] = (p + 28, size)
        p += 28 + stored
    off, _ = recs[2]
    n = struct.unpack_from('<i', d, off)[0]
    files = {}
    q = off + 4
    for _ in range(n):
        name = d[q:q+256].split(b'\0')[0].decode('latin1')
        fid = struct.unpack_from('<i', d, q + 256)[0]
        q += 260
        if fid in recs:
            files[name.upper()] = (fid,) + recs[fid]
    stills, anims, frames = {}, {}, set()
    if '-IMAGES.OPT' in files and '-INDEX.OPT' in files:
        _, io, isz = files['-INDEX.OPT']
        cnt = struct.unpack_from('<i', d, io)[0]; q = io + 4
        index = []
        for _ in range(cnt):
            iid = struct.unpack_from('<i', d, q)[0]; nm, q = cstr(d, q + 4); q += 8
            index.append((iid, nm.upper()))
        _, mo, msz = files['-IMAGES.OPT']
        q = mo; end = mo + msz - 1
        while q < end:
            sx, sy = struct.unpack_from('<ii', d, q + 3); q += 11 + 1024
            fc = struct.unpack_from('<i', d, q)[0]; q += 4
            for _ in range(fc):
                nm, q = cstr(d, q)
                pc, w, h = struct.unpack_from('<iii', d, q); q += 12 + pc * 24
                stills.setdefault(nm.upper(), (w, h))
        if '-ANIMS.OPT' in files:
            _, ao, asz = files['-ANIMS.OPT']
            q = ao; end = ao + asz - 1; blocks = []
            while q < end:
                fc = struct.unpack_from('<i', d, q)[0]; q += 4; fl = []
                for _ in range(fc):
                    nm, q = cstr(d, q); fl.append(nm.upper())
                blocks.append(fl)
            names = [nm for iid, nm in index if iid == -1]
            assert len(names) == len(blocks), path
            for nm, fl in zip(names, blocks):
                anims[nm] = [stills.get(f, (0, 0)) for f in fl]
                frames.update(fl)
    raw = sorted(n for n in files if not n.startswith('-') and n not in stills)
    return {'stills': {k: v for k, v in stills.items() if k not in frames}, 'anims': anims, 'raw': raw}

root = sys.argv[1]
out = {}
for sub in ('Imgs', 'Interf'):
    for f in sorted(os.listdir(os.path.join(root, sub))):
        if f.lower().endswith('.ff'):
            r = load(os.path.join(root, sub, f))
            out[f] = r
            print(f, 'stills', len(r['stills']), 'anims', len(r['anims']), 'raw', len(r['raw']))
json.dump(out, open(sys.argv[2], 'w'))
