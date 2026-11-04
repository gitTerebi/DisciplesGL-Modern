import struct, sys, os


def cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin1'), e + 1


def scan(path, covered):
    d = open(path, 'rb').read()
    recs = {}
    p = 28
    while p < len(d) - 1:
        rid, size, stored = struct.unpack_from('<iii', d, p + 8)
        recs[rid] = (p + 28, size)
        p += 28 + stored
    off, _ = recs[2]
    n = struct.unpack_from('<i', d, off)[0]
    q = off + 4
    table = []
    for _ in range(n):
        name = d[q:q + 256].split(b'\0')[0].decode('latin1').upper()
        fid = struct.unpack_from('<i', d, q + 256)[0]
        q += 260
        table.append((name, fid))
    files = {}
    for name, fid in table:
        files.setdefault(name, fid)
    stills, anims, used = {}, {}, set()
    if '-INDEX.OPT' in files and '-IMAGES.OPT' in files:
        io_, _ = recs[files['-INDEX.OPT']]
        cnt = struct.unpack_from('<i', d, io_)[0]
        q = io_ + 4
        names = []
        for _ in range(cnt):
            iid = struct.unpack_from('<i', d, q)[0]
            nm, q = cstr(d, q + 4)
            q += 8
            if iid == -1:
                names.append(nm.upper())
            else:
                used.add(iid)
        mo, msz = recs[files['-IMAGES.OPT']]
        q = mo
        end = mo + msz - 1
        while q < end:
            q += 11 + 1024
            fc = struct.unpack_from('<i', d, q)[0]
            q += 4
            for _ in range(fc):
                nm, q = cstr(d, q)
                pc, w, h = struct.unpack_from('<iii', d, q)
                q += 12 + pc * 24
                stills.setdefault(nm.upper(), w * h)
        if '-ANIMS.OPT' in files:
            ao, asz = recs[files['-ANIMS.OPT']]
            q = ao
            end = ao + asz - 1
            blocks = []
            while q < end:
                fc = struct.unpack_from('<i', d, q)[0]
                q += 4
                fl = []
                for _ in range(fc):
                    nm, q = cstr(d, q)
                    fl.append(nm.upper())
                blocks.append(fl)
            for nm, fl in zip(names, blocks):
                anims.setdefault(nm, fl)
    frames = {f for fl in anims.values() for f in fl}
    standalone = {}
    for name, fid in table:
        if name.startswith('-') or not name.endswith('.PNG') or fid in used or name in stills or name in standalone:
            continue
        o, s = recs[fid]
        w, h = struct.unpack_from('>ii', d, o + 16)
        standalone[name] = w * h
    out = {'unc': 0, 'cov': 0, 'frames_unc': 0, 'frames_cov': 0, 'px_unc': 0, 'px_cov': 0}

    def add(name, nframes, px):
        k = 'cov' if name in covered else 'unc'
        out[k] += 1
        out['frames_' + k] += nframes
        out['px_' + k] += px

    for nm, px in stills.items():
        if nm not in frames and nm not in anims:
            add(nm, 1, px)
    for nm, px in standalone.items():
        add(nm, 1, px)
    for nm, fl in anims.items():
        add(nm, len(fl), sum(stills.get(f, 0) for f in fl))
    return out


root = sys.argv[1]
covered = set(open(sys.argv[2]).read().split())
total = {}
for spec in sys.argv[3:]:
    sub, ff = spec.split('/')
    r = scan(os.path.join(root, sub, ff), covered)
    print('%-12s uncovered %5d entries %6d frames %7.1f Mpx canvas | covered (need pixel check) %5d entries %6d frames %7.1f Mpx' % (
        ff, r['unc'], r['frames_unc'], r['px_unc'] / 1e6, r['cov'], r['frames_cov'], r['px_cov'] / 1e6))
    for k, v in r.items():
        total[k] = total.get(k, 0) + v
print('TOTAL        uncovered %5d entries %6d frames %7.1f Mpx canvas | covered (need pixel check) %5d entries %6d frames %7.1f Mpx' % (
    total['unc'], total['frames_unc'], total['px_unc'] / 1e6, total['cov'], total['frames_cov'], total['px_cov'] / 1e6))
