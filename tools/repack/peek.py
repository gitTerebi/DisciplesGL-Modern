"""Show what a .galleanpack holds under given names, and save those images.

python -I tools\\repack\\peek.py <pack> <out folder> NAME [NAME ...]

Each NAME is looked up exactly, with and without .PNG, case-insensitive, and as
the last part of an asset id (so a Gallean id like unit-event-portrait/G000UU5017
is found too). Every match is printed with its full manifest entry, and its
image is written to <out folder> as <pack>-<id>.webp, unchanged.

A NAME starting with @ is a set:
  @battle-bg   the game's battle and city backgrounds (Wrapper.ff <TERRAIN>_<n>_BG,
               990x600 at 1x) and Gallean's own battle-background ids (950-wide
               scenes, a different canvas). Run it on gamenames-v1, own-v1 and
               assets-v1: the entries' Width/Height/PixelWidth/PixelHeight/Offset
               against the 1x 990x600 settle the battle background shift.
  @portraits   event portraits and faces seen in the logs (G000UU5017, G000UU0013 ...)

  python -I tools\\repack\\peek.py gamenames-v1.galleanpack logs\\peek @battle-bg
"""
import sys, os, json, struct

TERRAINS = {'human': 2, 'undead': 2, 'heretic': 3, 'dwarf': 2, 'neutral': 4, 'elf': 2, 'water': 1}
SETS = {
    '@BATTLE-BG': ['%s_%d_BG' % (r, k) for r in ('HUMAN', 'UNDEAD', 'HERETIC', 'DWARF', 'NEUTRAL', 'ELF', 'GREENSKIN', 'WATER', 'CITY', 'RUIN', 'BOAT')
                   for k in range(5)] +
                  ['battle-background/terrain/%s/%d' % (t, k) for t, n in TERRAINS.items() for k in range(n)] +
                  ['battle-background/boat/%d' % k for k in range(2)] + ['battle-background/city/%d' % k for k in range(5)] +
                  ['battle-background/ruin/%d' % k for k in range(3)] +
                  ['battle-background/capital/%s' % r for r in ('human', 'undead', 'heretic', 'dwarf', 'elf')],
    '@PORTRAITS': ['G000UU5017', 'G000UU0013', 'G000UU0009', 'G000UU0021', 'G000UU0070', 'G000UU5017FACE.PNG', 'G000UU5017FACEB.PNG',
                   'unit-event-portrait/g000uu5017', 'unit-event-portrait/g000uu0013'],
}

pack, out = sys.argv[1], sys.argv[2]
want = set()
names = []
for n in sys.argv[3:]:
    names += SETS.get(n.upper(), [n])
for n in names:
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
