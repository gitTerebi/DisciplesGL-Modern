import contextlib, io, os, runpy, sys

ff_json, manifest_json, out_path = sys.argv[1:4]
here = os.path.dirname(os.path.abspath(__file__))
sys.argv = ['coverage.py', ff_json, manifest_json]
with contextlib.redirect_stdout(io.StringIO()):
    g = runpy.run_path(os.path.join(here, 'coverage.py'))

names, anims, stills, raws, man = g['names'], g['anims'], g['stills'], g['raws'], g['man']
covered = set()
entries = 0
for sec in ('Animations', 'Sprites'):
    for e in man[sec]:
        cand = names(e['Id'])
        if cand is None:
            continue
        for n in cand:
            if n in anims or n in stills or n in raws:
                covered.add(n)
                entries += 1
                break

with open(out_path, 'w', newline='\n') as f:
    for n in sorted(covered):
        f.write(n + '\n')
print('pack entries mapped', entries, 'distinct game names', len(covered))
