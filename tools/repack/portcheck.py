import sys, os, json, struct, io
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
ns = {}; exec(open(os.path.join(HERE, 'ffscan.py')).read().split('root = sys.argv[1]')[0], ns)
game = sys.argv[1]
cur = ns['load'](os.path.join(game, 'Interf', 'Interf.ff'))
old = ns['load'](os.path.join(game, 'Interf', 'Interf.ff.poc-backup'))
def names(r): return {**{k: v for k, v in r['stills'].items()}, **{k: v for k, v in r['anims'].items()}}
c, o = names(cur), names(old)
changed = sorted(k for k in c if k not in o or c[k] != o[k])
print('ported/changed', len(changed), changed[:60])
p = os.path.join(game, sys.argv[2])
with open(p, 'rb') as f:
    h = f.read(32); off, ln = struct.unpack_from('<qq', h, 16); f.seek(off); m = json.loads(f.read(ln))
    idx = {}
    for kind in ('Sprites', 'Animations'):
        for e in m[kind]:
            idx[e['Id'].split('/')[-1].upper()] = e['Frames'][0] if kind == 'Animations' else e
    for k in changed:
        e = idx.get(k)
        if not e:
            print('%-32s NOT IN PACK' % k); continue
        f.seek(32 + e['Offset']); im = Image.open(io.BytesIO(f.read(e['Length']))).convert('RGBA')
        px = im.getdata(); n = len(px)
        a = sum(q[3] for q in px) / n; rgb = sum(q[0] + q[1] + q[2] for q in px) / n / 3
        md = e['Metadata']
        print('%-32s %4dx%-4d off %d,%d  alpha %.0f  rgb %.0f' % (k, md['Width'], md['Height'], md['OffsetX'], md['OffsetY'], a, rgb))
