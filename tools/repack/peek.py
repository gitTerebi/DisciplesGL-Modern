"""Show what a .galleanpack holds under given names, and save those images.

python -I tools\\repack\\peek.py <pack> <out folder> NAME [NAME ...]

Each NAME is looked up exactly, with and without .PNG, case-insensitive, and as
the last part of an asset id (so a Gallean id like unit-event-portrait/G000UU5017
is found too). Every match is printed with its full manifest entry, and its
image is written to <out folder> as <pack>-<id>.webp, unchanged.
"""
import sys, os, json, struct

pack, out = sys.argv[1], sys.argv[2]
want = set()
for n in sys.argv[3:]:
    n = n.upper()
    want.add(n)
    want.add(n[:-4] if n.endswith('.PNG') else n + '.PNG')

os.makedirs(out, exist_ok=True)
tag = os.path.basename(pack).split('.')[0]
with open(pack, 'rb') as f:
    h = f.read(32)
    off, ln = struct.unpack_from('<qq', h, 16)
    f.seek(off)
    m = json.loads(f.read(ln))
    found = 0
    for kind in ('Sprites', 'Animations'):
        for e in m[kind]:
            eid = e['Id']
            if eid.upper() not in want and eid.split('/')[-1].upper() not in want:
                continue
            found += 1
            frames = e['Frames'] if kind == 'Animations' else [e]
            print('==', kind, eid, 'frames', len(frames))
            for k, fr in enumerate(frames):
                print('   frame', k, json.dumps(fr.get('Metadata')), 'offset', fr['Offset'], 'length', fr['Length'])
                f.seek(32 + fr['Offset'])
                safe = eid.replace('/', '_').replace('\\', '_')
                name = '%s-%s%s.webp' % (tag, safe, '' if len(frames) == 1 else '-%d' % k)
                with open(os.path.join(out, name), 'wb') as o:
                    o.write(f.read(fr['Length']))
                if k >= 3 and len(frames) > 4:
                    print('   ...', len(frames) - 4, 'more frames not saved')
                    break
    print(tag, 'matches', found)
