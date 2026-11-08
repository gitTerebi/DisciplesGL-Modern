"""Combine per-game 2x packs from ffpack.py into one DisciplesGL-2x.pack.

python -I tools\\repack\\merge.py <out> V2=<v2 pack> V3=<v3 pack>

An entry every game has with identical bytes is stored once under its plain name. Every other entry is stored per
game as "<GAME>:<name>", and the DLL keeps only the entries of the game it runs in.
"""
import json
import struct
import sys
import time


def load(path):
    f = open(path, 'rb')
    head = f.read(32)
    offset, length = struct.unpack('<qq', head[16:32])
    f.seek(offset)
    manifest = json.loads(f.read(length))
    assert manifest.get('Format') == 'BC7-ZSTD', path + ' is not a BC7-ZSTD pack'
    return f, {s['Id']: s for s in manifest['Sprites']}


def read(f, sprite):
    f.seek(sprite['Offset'] + 32)
    return f.read(sprite['Length'])


def main(out, games):
    packs = {game: load(path) for game, path in games}
    ids = sorted(set().union(*(entries for _, entries in packs.values())))
    sprites = []
    shared = split = 0
    with open(out, 'wb') as o:
        o.write(b'\0' * 32)
        pos = 0

        def add(name, sprite, data):
            nonlocal pos
            o.write(data)
            sprites.append(dict(sprite, Id=name, Offset=pos, Length=len(data)))
            pos += len(data)

        for i in ids:
            have = [(game, f, entries[i]) for game, (f, entries) in packs.items() if i in entries]
            blobs = [read(f, s) for _, f, s in have]
            if len(have) == len(packs) and all(b == blobs[0] for b in blobs):
                add(i, have[0][2], blobs[0])
                shared += 1
            else:
                for (game, _, s), data in zip(have, blobs):
                    add('%s:%s' % (game, i), s, data)
                    split += 1
        manifest = json.dumps({'CreatedAt': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'CreatedBy': 'merge.py',
                               'Format': 'BC7-ZSTD', 'Games': [g for g, _ in games], 'Sprites': sprites},
                              separators=(',', ':')).encode('utf8')
        o.write(manifest)
        o.seek(0)
        o.write(b'DGLPACK\0\0\0\0\0' + struct.pack('<iqq', 1, 32 + pos, len(manifest)))
    print('%d shared entries, %d per-game entries, %.2f GB' % (shared, split, (32 + pos + len(manifest)) / 2**30))


if __name__ == '__main__':
    main(sys.argv[1], [arg.split('=', 1) for arg in sys.argv[2:]])
