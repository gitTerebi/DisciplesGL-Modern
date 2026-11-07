"""Build <game>\\ff4k-v1.galleanpack: one 4x image per .PNG record of every .ff in Imgs\\ and Interf\\.

python -I tools\\repack\\ffpack.py <game folder> <assets-v1.galleanpack> [--own PATH] [--out PATH] [--workers N] [--only A.FF,B.FF] [--limit N]

Sheet records: every -IMAGES.OPT part of the record takes its frame's 4K canvas (Gallean, then own-v1, then an
entry named like the record itself, read over the record at 0,0) at the part's canvas rect, scaled 4x, and writes
it to the part's sheet rect, scaled 4x: the inverse of Gallean's ImagesExtractor.BuildImage. Plain records take
the entry named like the record. Each part is shrunk back to 1x and compared with the 1x; a part that fails, or
has no source, keeps a Lanczos upscale of the 1x. Alpha is 0 wherever the 1x is transparent.
"""
import io, json, os, struct, sys, time, collections, argparse, mmap, warnings
import multiprocessing as mp
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
warnings.filterwarnings('ignore', 'Palette images with Transparency')
IEND = b'\x00\x00\x00\x00IEND\xaeB`\x82'
SCALE = 4
LOSSY = dict(lossless=False, quality=85, method=4, exact=True)
LOSSLESS = dict(lossless=True, quality=100, method=4, exact=True)

MAX_MISS = 0.05
MAX_EXTRA = 0.05
EDGE = 2
MAX_RGB = 40.0
MIN_OPAQUE = 16
ADD_LIT = 24


def load_game_name():
    src = open(os.path.join(HERE, 'repack.py')).read()
    src = src[:src.rindex('\nmain()')]
    src = src.replace("src = open(os.path.join(HERE, 'ffscan.py')).read().split('root = sys.argv[1]')[0]", "src = ''")
    src = src.replace("load_ff = ns['load']", '')
    ns = {'__file__': os.path.join(HERE, 'repack.py')}
    exec(src, ns)
    return ns['game_name'], ns['CANVAS'], ns['CENTRE'], ns['SPRITE']


def cstr(d, p):
    e = d.find(b'\0', p)
    return d[p:e].decode('latin1'), e + 1


def read_ff(path):
    with open(path, 'rb') as fh:
        d = mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ)
    assert d[:4] == b'MQDB', path
    recs = {}
    p = 28
    while p < len(d) - 1:
        assert d[p:p + 4] == b'MQRC', (path, p)
        rid, size, stored = struct.unpack_from('<iii', d, p + 8)
        recs[rid] = (p + 28, size)
        p += 28 + stored
    off, _ = recs[2]
    n = struct.unpack_from('<i', d, off)[0]
    q = off + 4
    files = {}
    for _ in range(n):
        name = d[q:q + 256].split(b'\0')[0].decode('latin1').upper()
        fid = struct.unpack_from('<i', d, q + 256)[0]
        q += 260
        if fid in recs:
            files.setdefault(name, fid)
    id_files = {fid: name for name, fid in files.items()}
    blocks, anims = [], {}
    if '-INDEX.OPT' in files and '-IMAGES.OPT' in files:
        io_, _ = recs[files['-INDEX.OPT']]
        cnt = struct.unpack_from('<i', d, io_)[0]
        q = io_ + 4
        ids_by_name = collections.defaultdict(list)
        anim_names = []
        for _ in range(cnt):
            iid = struct.unpack_from('<i', d, q)[0]
            nm, q = cstr(d, q + 4)
            q += 8
            nm = nm.upper()
            if iid == -1:
                anim_names.append(nm)
            elif iid in id_files:
                ids_by_name[nm].append(iid)
        mo, msz = recs[files['-IMAGES.OPT']]
        q = mo
        end = mo + msz - 1
        while q < end:
            tidx = d[q]
            alg = struct.unpack_from('<h', d, q + 1)[0]
            sx, sy = struct.unpack_from('<ii', d, q + 3)
            pal = bytes(d[q + 11:q + 11 + 1024])
            q += 11 + 1024
            fc = struct.unpack_from('<i', d, q)[0]
            q += 4
            frames = []
            for _ in range(fc):
                nm, q = cstr(d, q)
                pc, w, h = struct.unpack_from('<iii', d, q)
                q += 12
                parts = [struct.unpack_from('<6i', d, q + i * 24) for i in range(pc)]
                q += pc * 24
                frames.append((nm.upper(), w, h, parts))
            cands = list(ids_by_name.get(frames[0][0], []))
            for f in frames:
                ids = ids_by_name.get(f[0], [])
                cands = [c for c in cands if c in ids]
            fid = None
            if len(cands) == 1:
                fid = cands[0]
            else:
                for c in cands:
                    if struct.unpack_from('>ii', d, recs[c][0] + 16) == (sx, sy):
                        fid = c
                        break
            blocks.append({'tidx': tidx, 'alg': alg, 'pal': pal, 'frames': frames, 'fid': fid,
                           'file': id_files.get(fid)})
        if '-ANIMS.OPT' in files:
            ao, asz = recs[files['-ANIMS.OPT']]
            q = ao
            end = ao + asz - 1
            lists = []
            while q < end:
                fc = struct.unpack_from('<i', d, q)[0]
                q += 4
                fl = []
                for _ in range(fc):
                    nm, q = cstr(d, q)
                    fl.append(nm.upper())
                lists.append(fl)
            assert len(lists) == len(anim_names), path
            anims = dict(zip(anim_names, lists))
    pngs = {name: fid for name, fid in files.items() if name.endswith('.PNG')}
    return {'d': d, 'recs': recs, 'pngs': pngs, 'blocks': blocks, 'anims': anims}


def read_manifest(path):
    with open(path, 'rb') as f:
        head = f.read(32)
        assert head[:11] == b'GALLEANPACK' and struct.unpack_from('<i', head, 12)[0] == 27, path
        off, ln = struct.unpack_from('<qq', head, 16)
        f.seek(off)
        return head, json.loads(f.read(ln))


def build_sources(game, gallean, own, game_name, CANVAS, CENTRE, SPRITE):
    archives = []
    for sub in ('Imgs', 'Interf'):
        folder = os.path.join(game, sub)
        for f in sorted(os.listdir(folder)):
            if f.lower().endswith('.ff'):
                archives.append((f.upper(), os.path.join(folder, f), read_ff(os.path.join(folder, f))))

    frame_dims, frame_file, file_frames = {}, {}, collections.defaultdict(list)
    anims, stills, files = collections.defaultdict(list), collections.defaultdict(list), collections.defaultdict(list)
    for an, path, ff in archives:
        in_anims = set(f for v in ff['anims'].values() for f in v)
        for b in ff['blocks']:
            for nm, w, h, parts in b['frames']:
                frame_dims.setdefault((an, nm), (w, h))
                if b['file'] and (an, nm) not in frame_file:
                    frame_file[(an, nm)] = b['file']
                    file_frames[(an, b['file'])].append(nm)
                if nm not in in_anims and an not in stills[nm]:
                    stills[nm].append(an)
        for k in ff['anims']:
            anims[k].append(an)
        for name in ff['pngs']:
            files[name].append(an)
    anim_frames = {(an, k): v for an, _, ff in archives for k, v in ff['anims'].items()}
    spell_effects = collections.defaultdict(dict)
    for an, _, ff in archives:
        for k in ff['anims']:
            if len(k) == 15 and k.startswith('G000SS'):
                spell_effects[k[:12]][an] = k

    frame_src = collections.defaultdict(list)
    file_src = collections.defaultdict(list)
    stats = collections.Counter()

    def src(pack, fr, cat, origin, cx=0, cy=0):
        md = fr['Metadata']
        return {'pack': pack, 'off': fr['Offset'], 'len': fr['Length'], 'w': md['Width'], 'h': md['Height'],
                'x': md['OffsetX'] + cx, 'y': md['OffsetY'] + cy, 'cat': cat, 'centre': origin == CENTRE,
                'add': bool(md.get('Additive'))}

    def variants(name):
        return [name, name[:-4] if name.endswith('.PNG') else name + '.PNG']

    def centre(origin, an, fname):
        w, h = frame_dims.get((an, fname), (0, 0))
        return (w // 2, h // 2) if origin == CENTRE else (0, 0)

    def add_named(pack, cat, origin, name, entry, anim):
        frs = entry['Frames'] if anim else [entry]
        if any(f['Metadata'].get('PixelScale', 2) < 2 for f in frs):
            stats['%s: entry at 1x, not used' % cat] += 1
            return
        for n in variants(name):
            if n in anims or (n in spell_effects and cat == 'world-spell-effect'):
                found = [(an, n) for an in anims.get(n, [])] or [(an, k) for an, k in spell_effects[n].items()]
                for an, k in found:
                    fl = anim_frames[(an, k)]
                    for i, fname in enumerate(fl[:len(frs)]):
                        frame_src[(an, fname)].append(src(pack, frs[i], cat, origin, *centre(origin, an, fname)))
                    if len(frs) != len(fl):
                        stats['%s: animation with another frame count' % cat] += 1
                stats['%s: animation' % cat] += 1
                for an in stills.get(n, []):
                    if an not in anims.get(n, []):
                        frame_src[(an, n)].append(src(pack, frs[0], cat, origin, *centre(origin, an, n)))
                        stats['%s: animation frame 0 for a still of that name' % cat] += 1
                return
            if n in stills:
                for an in stills[n]:
                    frame_src[(an, n)].append(src(pack, frs[0], cat, origin, *centre(origin, an, n)))
                stats['%s: frame' % cat] += 1
                return
        for n in variants(name):
            if n in files:
                for an in files[n]:
                    file_src[(an, n)].append(src(pack, frs[0], cat, origin))
                stats['%s: record file' % cat] += 1
                return
        stats['%s: no such name in this game' % cat] += 1

    def add_composite(pack, cat, name, entry):
        if entry['Metadata'].get('PixelScale', 2) < 2:
            stats['%s: entry at 1x, not used' % cat] += 1
            return
        for n in variants(name):
            keys = [(an, frame_file[(an, n)]) for an in stills.get(n, []) if (an, n) in frame_file]
            keys += [(an, n) for an in files.get(n, [])]
            if not keys:
                continue
            for key in keys:
                if key in file_frames:
                    for fname in file_frames[key]:
                        frame_src[(key[0], fname)].append(src(pack, entry, cat, CANVAS))
                    stats['%s: composite of a record\'s frames' % cat] += 1
                else:
                    file_src[key].append(src(pack, entry, cat, CANVAS))
                    stats['%s: record file' % cat] += 1
            return
        stats['%s: no such name in this game' % cat] += 1

    _, gm = read_manifest(gallean)
    for kind in ('Sprites', 'Animations'):
        anim = kind == 'Animations'
        for e in gm[kind]:
            pid = e['Id']
            cat = pid.split('/')[0]
            if cat == 'unit-event-portrait':
                add_composite(0, cat, pid.split('/')[1].upper(), e)
                continue
            name, origin = game_name(pid)
            if not name:
                stats['%s: no game name' % cat] += 1
                continue
            add_named(0, cat, origin, name, e, anim)
    del gm

    if own:
        _, om = read_manifest(own)
        for kind in ('Sprites', 'Animations'):
            anim = kind == 'Animations'
            for e in om[kind]:
                add_named(1, 'own-v1', CANVAS, e['Id'].split('/')[-1].upper(), e, anim)
    return archives, frame_src, file_src, stats


PACKS = []
HANDLES = {}


def init_worker(packs):
    global PACKS
    PACKS = packs


def blob(pack, off, ln):
    h = HANDLES.get(pack)
    if h is None:
        h = HANDLES[pack] = open(PACKS[pack], 'rb')
    h.seek(32 + off)
    return h.read(ln)


def opacity(alg, tidx, pal):
    p = np.frombuffer(pal, np.uint8).reshape(256, 4)
    if alg <= 255:
        brush = ((p[:, 0] & 0xf8) == 0xf8) & ((p[:, 1] & 0xf8) == 0) & ((p[:, 2] & 0xf8) == 0xf8)
        op = np.where(brush | (alg == 0), 0, 255).astype(np.uint8)
    elif alg in (300, 400):
        op = np.arange(256, dtype=np.uint8)
    else:
        op = np.full(256, 255, np.uint8)
    op[tidx] = 0
    return op


def brush_alpha(rgb):
    brush = ((rgb[..., 0] & 0xf8) == 0xf8) & ((rgb[..., 1] & 0xf8) == 0) & ((rgb[..., 2] & 0xf8) == 0xf8)
    return np.where(brush, 0, 255).astype(np.uint8)


def resize_premul(arr, w, h):
    return np.asarray(Image.fromarray(arr, 'RGBA').convert('RGBa').resize((w, h), Image.LANCZOS).convert('RGBA'))


def plain_up(rgb, a):
    h, w = a.shape
    cov = np.dstack([rgb, np.where(a > 0, 255, 0).astype(np.uint8)])
    out = resize_premul(cov, w * SCALE, h * SCALE).copy()
    out[..., 3] = np.repeat(np.repeat(a, SCALE, 0), SCALE, 1)
    return out


EXACT_ARCHIVES = {'GRBORDER.FF'}
SKIP_RECORDS = {'ISOTERRN.FF/FOG.PNG', 'ISOTERRN.FF/FOGCORNERS.PNG'}


def exact_up(rgb, a):
    return np.repeat(np.repeat(np.dstack([rgb, a]), SCALE, 0), SCALE, 1)


def shrink(c):
    h, w = c.shape[0] // SCALE, c.shape[1] // SCALE
    f = c.reshape(h, SCALE, w, SCALE, 4).astype(np.float32)
    al = f[..., 3]
    asum = al.sum((1, 3))
    rgb = (f[..., :3] * al[..., None]).sum((1, 3)) / np.maximum(asum, 1)[..., None]
    return rgb, asum / (SCALE * SCALE)


def measure(cand, rgb1, a1, additive=False):
    if additive:
        rgb4 = cand[..., :3].reshape(a1.shape[0], SCALE, a1.shape[1], SCALE, 3).astype(np.float32).mean((1, 3))
        ref = rgb1.astype(np.float32) * (a1 > 0)[..., None]
        lit = (ref.max(-1) > ADD_LIT) | (rgb4.max(-1) > ADD_LIT)
        return [int(lit.sum()), 0, 0, float(np.abs(rgb4 - ref)[lit].sum()) / 3, int(lit.sum())]
    rgb4, a4 = shrink(cand)
    op = a1 > 0
    good = op & (a4 >= 0.5 * a1.astype(np.float32))
    miss = ~grow(~op, EDGE) & (a4 < 0.25 * a1.astype(np.float32))
    extra = ~grow(op, EDGE) & (a4 >= 128)
    err = float(np.abs(rgb4 - rgb1.astype(np.float32))[good].sum()) / 3
    return [int(op.sum()), int(miss.sum()), int(extra.sum()), err, int(good.sum())]


def grow(m, r):
    h, w = m.shape
    p = np.pad(m, r, mode='edge')
    out = np.zeros_like(m)
    for dy in range(2 * r + 1):
        for dx in range(2 * r + 1):
            out |= p[dy:dy + h, dx:dx + w]
    return out


def verdict(n, miss, extra, err, ng):
    if n == 0:
        return None
    if miss > MAX_MISS * n:
        return 'misses %d of %d opaque 1x pixels' % (miss, n)
    if extra > MAX_EXTRA * n:
        return 'opaque on %d 1x-transparent pixels (%d opaque)' % (extra, n)
    if err / max(ng, 1) > MAX_RGB:
        return 'colour differs by %.1f on average' % (err / max(ng, 1))
    return None


def decode_entry(s, cache):
    key = (s['pack'], s['off'])
    if key not in cache:
        data = blob(s['pack'], s['off'], s['len'])
        arr = np.asarray(Image.open(io.BytesIO(data)).convert('RGBA'))
        if s['add']:
            arr = arr.copy()
            arr[..., 3] = 255
        w, h = s['w'] * SCALE, s['h'] * SCALE
        if arr.shape[1] != w or arr.shape[0] != h:
            arr = resize_premul(arr, w, h)
        cache[key] = (arr, b'VP8L' in data[:64])
    return cache[key]


def cut(arr, ex, ey, ew, eh, cx, cy, pw, ph):
    out = np.zeros((ph * SCALE, pw * SCALE, 4), np.uint8)
    x0, y0 = max(cx, ex), max(cy, ey)
    x1, y1 = min(cx + pw, ex + ew), min(cy + ph, ey + eh)
    if x1 > x0 and y1 > y0:
        out[(y0 - cy) * SCALE:(y1 - cy) * SCALE, (x0 - cx) * SCALE:(x1 - cx) * SCALE] = \
            arr[(y0 - ey) * SCALE:(y1 - ey) * SCALE, (x0 - ex) * SCALE:(x1 - ex) * SCALE]
    return out


def label(s):
    return '%s %s' % ('gallean' if s['pack'] == 0 else 'own-v1', s['cat'])


def do_record(task):
    tid, path, roff, rsize, name, blocks, fsrc = task
    with open(path, 'rb') as fh:
        fh.seek(roff)
        data = fh.read(rsize)
    if not data.endswith(IEND):
        data += IEND
    img = Image.open(io.BytesIO(data))
    img.load()
    W, H = img.size
    rgb = np.asarray(img.convert('RGB'))
    idx = np.asarray(img) if img.mode == 'P' else None
    sheet = bool(blocks) and idx is not None
    alpha = opacity(blocks[0]['alg'], blocks[0]['tidx'], blocks[0]['pal'])[idx] if sheet else brush_alpha(rgb)
    file_cands = [dict(s, x=s['x'] + (W // 2 if s['centre'] else 0), y=s['y'] + (H // 2 if s['centre'] else 0), ident=True)
                  for s in fsrc]
    r = {'tid': tid, 'name': name, 'w': W, 'h': H, 'parts': 0, 'parts4k': 0, 'nosrc': 0, 'failed': 0,
         'fails': [], 'src': collections.Counter()}
    cache = {}
    lossless_all = True
    writes = []

    def place(s, cx, cy, sx, sy, pw, ph):
        if s.get('ident'):
            return cut(decode_entry(s, cache)[0], s['x'], s['y'], s['w'], s['h'], sx, sy, pw, ph)
        return cut(decode_entry(s, cache)[0], s['x'], s['y'], s['w'], s['h'], cx, cy, pw, ph)

    if sheet:
        done = np.zeros((H, W), bool)
        for b in blocks:
            op = opacity(b['alg'], b['tidx'], b['pal'])
            additive = b['alg'] in (300, 400)
            gain = 255.0 / b['alg'] if 0 < b['alg'] < 255 else None
            for fname, fw, fh_, parts, srcs in b['frames']:
                todo = []
                for (cx, cy, sx, sy, pw, ph) in parts:
                    if pw <= 0 or ph <= 0 or sx < 0 or sy < 0 or sx + pw > W or sy + ph > H:
                        continue
                    if done[sy:sy + ph, sx:sx + pw].all():
                        continue
                    a1 = op[idx[sy:sy + ph, sx:sx + pw]]
                    alpha[sy:sy + ph, sx:sx + pw] = a1
                    todo.append((cx, cy, sx, sy, pw, ph, a1))
                if not todo:
                    continue
                counted = sum(1 for p in todo if (p[6] > 0).any())
                r['parts'] += counted
                cands = list(srcs) + file_cands
                if not cands:
                    r['nosrc'] += counted
                    continue
                chosen = None
                first_bad = None
                for s in cands:
                    res = []
                    tot = [0, 0, 0, 0.0, 0]
                    for p in todo:
                        c = place(s, *p[:6])
                        m = measure(c, rgb[p[3]:p[3] + p[5], p[2]:p[2] + p[4]], p[6] if not gain else (p[6] / gain).astype(np.uint8), additive)
                        res.append((p, c, m))
                        tot = [u + v for u, v in zip(tot, m)]
                    frame_v = verdict(*tot)
                    ok, bad = [], []
                    for p, c, m in res:
                        v = verdict(*m) if m[0] >= MIN_OPAQUE else (frame_v if m[0] else None)
                        (bad if v else ok).append((p, c, m, v))
                    if any(m[0] for _, _, m, _ in ok) or not bad:
                        chosen = (s, ok, bad)
                        break
                    if first_bad is None:
                        first_bad = (s, bad)
                if chosen is None:
                    s, bad = first_bad
                    extra = '' if len(cands) == 1 else ' (and %d other source(s) failed too)' % (len(cands) - 1)
                    rest = counted - len([p for p, c, m, v in bad if (p[6] > 0).any()])
                    r['failed'] += rest
                    if rest:
                        r['fails'].append('%s: %d more part(s) that passed alone, left plain because no part of the frame passed'
                                          % (fname, rest))
                else:
                    s, ok, bad = chosen
                    extra = ''
                    lossless_all &= decode_entry(s, cache)[1]
                    for p, c, m, v in ok:
                        cx, cy, sx, sy, pw, ph, a1 = p
                        if additive:
                            c[..., 3] = np.repeat(np.repeat(a1, SCALE, 0), SCALE, 1)
                        elif gain:
                            c[..., 3] = np.minimum(c[..., 3] * gain + 0.5, 255).astype(np.uint8)
                        writes.append((sx, sy, c, done[sy:sy + ph, sx:sx + pw].copy()))
                        done[sy:sy + ph, sx:sx + pw] = True
                        if (a1 > 0).any():
                            r['parts4k'] += 1
                            r['src'][label(s)] += 1
                for p, c, m, v in bad:
                    r['failed'] += 1 if (p[6] > 0).any() else 0
                    r['fails'].append('%s part canvas %d,%d %dx%d sheet %d,%d: %s: %s%s'
                                      % (fname, p[0], p[1], p[4], p[5], p[2], p[3], label(s), v, extra))
    else:
        r['parts'] = 1 if (alpha > 0).any() else 0
        if not file_cands:
            r['nosrc'] = r['parts']
        bad = []
        for s in file_cands:
            c = place(s, 0, 0, 0, 0, W, H)
            v = verdict(*measure(c, rgb, alpha))
            if v:
                bad.append('whole record: %s: %s' % (label(s), v))
                continue
            writes.append((0, 0, c, np.zeros((H, W), bool)))
            lossless_all &= decode_entry(s, cache)[1]
            if r['parts']:
                r['parts4k'] = 1
                r['src'][label(s)] += 1
            bad = []
            break
        if bad and r['parts']:
            r['failed'] = 1
            r['fails'] = bad

    out = plain_up(rgb, alpha)
    for sx, sy, c, skip in writes:
        dst = out[sy * SCALE:sy * SCALE + c.shape[0], sx * SCALE:sx * SCALE + c.shape[1]]
        if skip.any():
            keep = np.repeat(np.repeat(~skip, SCALE, 0), SCALE, 1)
            dst[keep] = c[keep]
        else:
            dst[...] = c
    out[np.repeat(np.repeat(alpha == 0, SCALE, 0), SCALE, 1)] = 0
    exact = os.path.basename(path).upper() in EXACT_ARCHIVES
    if exact:
        out = exact_up(rgb, alpha)
    lossless = exact or (bool(r['parts4k']) and lossless_all)
    buf = io.BytesIO()
    Image.fromarray(out, 'RGBA').save(buf, 'WEBP', **(LOSSLESS if lossless else LOSSY))
    r['webp'] = buf.getvalue()
    r['lossless'] = lossless
    r['src'] = dict(r['src'])
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('game')
    ap.add_argument('gallean')
    ap.add_argument('--own')
    ap.add_argument('--out')
    ap.add_argument('--workers', type=int, default=os.cpu_count())
    ap.add_argument('--only')
    ap.add_argument('--limit', type=int, default=0)
    a = ap.parse_args()
    t0 = time.time()
    own = a.own or os.path.join(a.game, 'own-v1.galleanpack')
    own = own if os.path.exists(own) else None
    out = a.out or os.path.join(a.game, 'ff4k-v1.galleanpack')
    report_path = os.path.splitext(out)[0] + '-report.txt'
    assert not os.path.exists(out), 'refusing to overwrite ' + out

    archives, frame_src, file_src, stats = build_sources(a.game, a.gallean, own, *load_game_name())
    only = set(x.strip().upper() for x in a.only.split(',')) if a.only else None

    tasks, order = [], []
    for an, path, ff in archives:
        if only and an not in only:
            continue
        by_fid = collections.defaultdict(list)
        for b in ff['blocks']:
            if b['fid'] is not None:
                by_fid[b['fid']].append(b)
        names = sorted(ff['pngs'])[:a.limit or None]
        for name in names:
            if (an.upper() + '/' + name.upper()) in SKIP_RECORDS:
                continue
            fid = ff['pngs'][name]
            roff, rsize = ff['recs'][fid]
            blocks = [dict(b, frames=[(nm, w, h, parts, frame_src.get((an, nm), [])) for nm, w, h, parts in b['frames']])
                      for b in by_fid.get(fid, [])]
            tasks.append((len(tasks), path, roff, rsize, name, blocks, file_src.get((an, name), [])))
            order.append(an)
        ff['d'].close()
    print('sources ready: %d frame names, %d record names, %d records to build (%.0fs)'
          % (len(frame_src), len(file_src), len(tasks), time.time() - t0), flush=True)

    tmp = out + '.tmp'
    results = [None] * len(tasks)
    pos = 0
    run = sorted(tasks, key=lambda t: -t[3])
    with open(tmp, 'wb') as o:
        o.write(b'\0' * 32)
        with mp.Pool(a.workers, init_worker, ([a.gallean, own],)) as pool:
            for i, r in enumerate(pool.imap_unordered(do_record, run, chunksize=1)):
                data = r.pop('webp')
                o.write(data)
                r['offset'], r['length'] = pos, len(data)
                pos += len(data)
                results[r['tid']] = r
                if (i + 1) % 250 == 0:
                    print('  %d/%d records, %.2f GB, %.0fs' % (i + 1, len(tasks), pos / 1e9, time.time() - t0), flush=True)
        sprites = []
        per_arch = collections.OrderedDict()
        for an, r in zip(order, results):
            sprites.append({'Id': '%s/%s' % (an, r['name']),
                            'Metadata': {'Width': r['w'], 'Height': r['h'], 'PixelWidth': r['w'] * SCALE,
                                         'PixelHeight': r['h'] * SCALE, 'OffsetX': 0, 'OffsetY': 0,
                                         'Additive': False, 'PixelScale': SCALE},
                            'Offset': r['offset'], 'Length': r['length'], 'Transforms': [], 'Layers': []})
            per_arch.setdefault(an, []).append(r)
        manifest = json.dumps({'CreatedAt': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'CreatedBy': 'ffpack.py',
                               'Sparse': False, 'Sprites': sprites, 'Animations': [], 'Sounds': []},
                              separators=(',', ':')).encode('utf8')
        o.write(manifest)
        o.seek(0)
        o.write(b'GALLEANPACK\0' + struct.pack('<iqq', 27, 32 + pos, len(manifest)))
    os.replace(tmp, out)
    elapsed = time.time() - t0
    write_report(report_path, a, out, own, per_arch, stats, elapsed)
    print('wrote %s (%.2f GB) and %s in %.0fs' % (out, os.path.getsize(out) / 1e9, report_path, elapsed))


def write_report(path, a, out, own, per_arch, stats, elapsed):
    L = ['ffpack.py report for %s' % a.game,
         'pack %s, %d bytes; sources %s, %s; built in %.0f s' % (out, os.path.getsize(out), a.gallean, own, elapsed),
         'check per part (shrunk to 1x): fails when the 4K leaves uncovered over %d%% as many pixels as the part has opaque 1x '
         'pixels, counting only pixels over %d px inside the 1x silhouette; when the 4K is opaque on over %d%% as many pixels '
         'more than %d px outside it; or when the mean RGB difference is over %.0f. Parts under %d opaque pixels are judged '
         'by their frame\'s totals. Additive (aura/glow) parts compare RGB only.'
         % (MAX_MISS * 100, EDGE, MAX_EXTRA * 100, EDGE, MAX_RGB, MIN_OPAQUE),
         '',
         '%-14s %7s %7s %7s %7s | %8s %8s %8s %8s | %9s | %s' % ('archive', 'records', 'full4K', 'partly', 'plain',
                                                                'parts', '4K', 'failed', 'no src', 'MB', '4K parts by source')]
    T = collections.Counter()
    for an, rs in per_arch.items():
        c = collections.Counter()
        srcs = collections.Counter()
        for r in rs:
            c['records'] += 1
            if r['parts'] and r['parts4k'] >= r['parts']:
                c['full'] += 1
            elif r['parts4k']:
                c['partly'] += 1
            else:
                c['plain'] += 1
            for k in ('parts', 'parts4k', 'failed', 'nosrc', 'lossless', 'length'):
                c[k] += r[k]
            srcs.update(r['src'])
        T.update(c)
        L.append('%-14s %7d %7d %7d %7d | %8d %8d %8d %8d | %9.1f | %s' % (an, c['records'], c['full'], c['partly'], c['plain'], c['parts'],
                 c['parts4k'], c['failed'], c['nosrc'], c['length'] / 1e6, ', '.join('%s %d' % kv for kv in srcs.most_common())))
    L.append('%-14s %7d %7d %7d %7d | %8d %8d %8d %8d | %9.1f' % ('total', T['records'], T['full'], T['partly'], T['plain'],
                                                        T['parts'], T['parts4k'], T['failed'], T['nosrc'], T['length'] / 1e6))
    L += ['',
          'full4K: every part with an opaque 1x pixel took 4K art. partly: some did. plain: none did.',
          'parts: parts with an opaque 1x pixel (a plain record counts as one part). failed: parts whose 4K failed the check.',
          'no src: parts with no 4K source at all. parts = 4K + failed + no src. MB: encoded size in the pack.',
          'records encoded lossless (every 4K source lossless): %d; lossy q85: %d' % (T['lossless'], T['records'] - T['lossless']),
          '',
          'Source mapping (Gallean and own-v1 entries):']
    for k, v in sorted(stats.items()):
        L.append('  %-72s %d' % (k, v))
    L += ['', 'Parts that failed the check (they keep the plain upscale):']
    for an, rs in per_arch.items():
        for r in rs:
            for f in r['fails']:
                L.append('  %s/%s: %s' % (an, r['name'], f))
    open(path, 'w', encoding='utf8').write('\n'.join(L) + '\n')


if __name__ == '__main__':
    main()
