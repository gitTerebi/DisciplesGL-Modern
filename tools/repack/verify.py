import io, json, os, struct, sys
import numpy as np
from PIL import Image

IEND = b'\x00\x00\x00\x00IEND\xaeB`\x82'


def cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin1'), e + 1


def load_ff(path):
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
    files = {}
    for _ in range(n):
        name = d[q:q + 256].split(b'\0')[0].decode('latin1')
        fid = struct.unpack_from('<i', d, q + 256)[0]
        q += 260
        files.setdefault(name.upper(), fid)
    io_, _ = recs[files['-INDEX.OPT']]
    cnt = struct.unpack_from('<i', d, io_)[0]
    q = io_ + 4
    index = {}
    anim_names = []
    for _ in range(cnt):
        iid = struct.unpack_from('<i', d, q)[0]
        nm, q = cstr(d, q + 4)
        q += 8
        if iid == -1:
            anim_names.append(nm.upper())
        else:
            index.setdefault(nm.upper(), []).append(iid)
    mo, msz = recs[files['-IMAGES.OPT']]
    q = mo
    end = mo + msz - 1
    frames = {}
    while q < end:
        tidx = d[q]
        alg = struct.unpack_from('<h', d, q + 1)[0]
        pal = d[q + 11:q + 11 + 1024]
        q += 11 + 1024
        fc = struct.unpack_from('<i', d, q)[0]
        q += 4
        for _ in range(fc):
            nm, q = cstr(d, q)
            pc, w, h = struct.unpack_from('<iii', d, q)
            q += 12
            parts = [struct.unpack_from('<6i', d, q + i * 24) for i in range(pc)]
            q += pc * 24
            frames.setdefault(nm.upper(), (w, h, parts, tidx, alg, pal))
    anims = {}
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
        anims = dict(zip(anim_names, blocks))
    return d, recs, index, frames, anims


def opacity(alg, tidx, pal):
    op = np.zeros(256, np.uint8)
    for i in range(256):
        r, g, b = pal[i * 4], pal[i * 4 + 1], pal[i * 4 + 2]
        if alg <= 255:
            brush = (r & 0xf8) == 0xf8 and (g & 0xf8) == 0 and (b & 0xf8) == 0xf8
            op[i] = 0 if brush else alg
        elif alg in (300, 400):
            op[i] = i
        else:
            op[i] = 255
    op[tidx] = 0
    return op


def render(ff, name):
    d, recs, index, frames, _ = ff
    w, h, parts, tidx, alg, pal = frames[name]
    fid = index[name][0]
    off, size = recs[fid]
    data = d[off:off + size]
    if not data.endswith(IEND):
        data += IEND
    img = Image.open(io.BytesIO(data))
    assert img.mode == 'P', img.mode
    idx = np.array(img)
    rgb = np.array(img.convert('RGB'))
    op = opacity(alg, tidx, pal)
    canvas = np.zeros((h, w, 4), np.uint8)
    for cx, cy, ax, ay, pw, ph in parts:
        sub_i = idx[ay:ay + ph, ax:ax + pw]
        sub_rgb = rgb[ay:ay + ph, ax:ax + pw]
        x0, y0 = max(cx, 0), max(cy, 0)
        x1, y1 = min(cx + pw, w), min(cy + ph, h)
        if x1 <= x0 or y1 <= y0:
            continue
        a = op[sub_i[y0 - cy:y1 - cy, x0 - cx:x1 - cx]]
        canvas[y0:y1, x0:x1, :3] = np.where(a[..., None] > 0, sub_rgb[y0 - cy:y1 - cy, x0 - cx:x1 - cx], 0)
        canvas[y0:y1, x0:x1, 3] = a
    return canvas, alg in (300, 400)


def bbox(canvas):
    ys, xs = np.nonzero(canvas[..., 3])
    if len(xs) == 0:
        return (0, 0, 1, 1)
    return (int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1))


def check(ff, name, meta, webp_path, rows):
    canvas, additive = render(ff, name)
    x, y, w, h = bbox(canvas)
    got = (meta['OffsetX'], meta['OffsetY'], meta['Width'], meta['Height'])
    img = Image.open(webp_path).convert('RGBA')
    up = np.array(img).astype(np.float32)
    ok_box = got == (x, y, w, h)
    ok_px = img.size == (meta['PixelWidth'], meta['PixelHeight']) == (2 * w, 2 * h)
    src = canvas[y:y + h, x:x + w].astype(np.float32)
    down = up.reshape(h, 2, w, 2, 4).mean(axis=(1, 3)) if ok_px else None
    if down is not None and not additive:
        alpha_err = float(np.abs(down[..., 3] - src[..., 3]).mean())
        opaque = src[..., 3] == 255
        rgb_err = float(np.abs(down[..., :3] - src[..., :3])[opaque].mean()) if opaque.any() else 0.0
    elif down is not None:
        alpha_err = float(down[..., 3].max())
        rgb_err = float(np.abs(down[..., :3] - src[..., :3]).mean())
    else:
        alpha_err = rgb_err = -1.0
    rows.append((name, (x, y, w, h), got, img.size, ok_box, ok_px, additive, round(alpha_err, 2), round(rgb_err, 2)))


def main():
    ff_paths = sys.argv[1].split(';')
    readback = sys.argv[2]
    manifest = json.load(open(os.path.join(readback, 'manifest.json'), encoding='utf8'))
    ffs = [load_ff(p) for p in ff_paths]

    def find(name):
        for ff in ffs:
            if name in ff[3] or name in ff[4]:
                return ff
        return None

    rows = []
    unknown = 0
    for s in manifest['Sprites']:
        ff = find(s['Id'])
        if ff is None:
            unknown += 1
            continue
        check(ff, s['Id'], s['Metadata'], os.path.join(readback, s['Id'] + '.webp'), rows)
    frame_rows = []
    order_ok = 0
    for a in manifest['Animations']:
        ff = find(a['Id'])
        names = ff[4][a['Id']]
        if len(names) == len(a['Frames']):
            order_ok += 1
        for i, (fname, fr) in enumerate(zip(names, a['Frames'])):
            check(ff, fname, fr['Metadata'], os.path.join(readback, '%s#%d.webp' % (a['Id'], i)), frame_rows)

    for label, rs in (('sprites', rows), ('animation frames', frame_rows)):
        bad = [r for r in rs if not (r[4] and r[5])]
        normal = [r for r in rs if not r[6] and r[7] >= 0]
        print('%s: %d checked, %d with wrong box or pixel size, %d additive' % (label, len(rs), len(bad), sum(r[6] for r in rs)))
        if normal:
            print('  2x->1x box-downsample vs source: mean alpha err %.2f (max %.2f), mean RGB err on opaque px %.2f (max %.2f)' % (
                np.mean([r[7] for r in normal]), max(r[7] for r in normal), np.mean([r[8] for r in normal]), max(r[8] for r in normal)))
        for r in bad[:10]:
            print('  BAD', r)
    print('not rebuilt (standalone files):', unknown)
    print('animations with game frame count:', order_ok, 'of', len(manifest['Animations']))
    print('examples (name, source box x,y,w,h, manifest box, webp size):')
    for r in rows[:3] + rows[-3:] + frame_rows[:3]:
        print('  ', r[0], r[1], r[2], r[3])


main()
