import json, sys, collections, importlib.util
spec = importlib.util.spec_from_file_location('cov', 'scripts/coverage.py')
sys.argv = ['x', 'cov/rote.json', 'pack/manifest.json']
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    cov = importlib.util.module_from_spec(spec); spec.loader.exec_module(cov)
res = collections.defaultdict(lambda: collections.Counter())
ex = {}
for sec in ('Animations', 'Sprites'):
    for e in cov.man[sec]:
        i = e['Id']; c = i.split('/')[0]
        cand = cov.names(i)
        if not cand: continue
        dims = None
        for n in cand:
            if n in cov.anims: dims = cov.anims[n][1]; break
            if n in cov.stills: dims = [cov.stills[n][1]]; break
        if not dims: continue
        frs = e['Frames'] if sec == 'Animations' else [e]
        for k, f in enumerate(frs[:len(dims)]):
            md = f.get('Metadata', {}); W, H = dims[k]
            ox, oy, w, h = md.get('OffsetX', 0), md.get('OffsetY', 0), md.get('Width', 0), md.get('Height', 0)
            if ox == 0 and oy == 0 and w == W and h == H: kind = 'full'
            elif ox >= 0 and oy >= 0 and ox + w <= W and oy + h <= H: kind = 'canvas'
            elif ox >= -(W // 2) - 1 and oy >= -(H // 2) - 1 and ox + w <= W - W // 2 + 1 and oy + h <= H - H // 2 + 1: kind = 'centre'
            else: kind = 'other'
            res[c][kind] += 1
            ex.setdefault((c, kind), (i, k, W, H, ox, oy, w, h, md.get('Additive')))
for c in res:
    print('%-30s %s' % (c, dict(res[c])))
    for kind in res[c]:
        if kind in ('other', 'centre', 'canvas'): print('        ', kind, ex[(c, kind)])
