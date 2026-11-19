import json, sys, collections, struct, os
for n in ('v2','rote','dzieje','semga'):
    d = json.load(open('cov/%s.json' % n))
    still_owner = collections.defaultdict(list); anim_owner = collections.defaultdict(list)
    exts = collections.Counter(); slash = 0; both = 0
    for f, r in d.items():
        for k in r['stills']: still_owner[k].append(f)
        for k in r['anims']: anim_owner[k].append(f)
        for k in r['raw']: exts[os.path.splitext(k)[1]] += 1
        both += len(set(r['stills']) & set(r['anims']))
        slash += sum('/' in k or chr(92) in k for k in list(r['stills']) + list(r['anims']) + r['raw'])
    sd = {k: v for k, v in still_owner.items() if len(v) > 1}
    ad = {k: v for k, v in anim_owner.items() if len(v) > 1}
    cross = set(still_owner) & set(anim_owner)
    print(n, 'stills', len(still_owner), 'anims', len(anim_owner), 'dupStill', len(sd), 'dupAnim', len(ad), 'still&anim', len(cross), 'sameff', both, 'slash', slash, dict(exts))
    print('   ', list(sd.items())[:4], list(ad.items())[:3])
