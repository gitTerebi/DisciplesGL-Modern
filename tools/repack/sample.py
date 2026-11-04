import sys, json, collections
p, out = sys.argv[1], sys.argv[2]
j=json.load(open(out+'/manifest.json','rb'))
seen=collections.OrderedDict()
for kind in ('Sprites','Animations'):
    for e in j[kind]:
        c=e['Id'].split('/')[0]
        if c not in seen: seen[c]=[]
        if len(seen[c])<3: seen[c].append(e['Id'])
for c,ids in seen.items(): print(c, ids)
want={'unit-face','item-icon','battle-unit','stack-unit','tree','ground'}
picks=[]
for kind in ('Sprites','Animations'):
    for e in j[kind]:
        c=e['Id'].split('/')[0]
        if c in want:
            want.discard(c)
            fr=e if kind=='Sprites' else e['Frames'][len(e['Frames'])//2]
            picks.append((e['Id'],fr))
f=open(p,'rb')
for i,fr in picks:
    f.seek(32+fr['Offset']); d=f.read(fr['Length'])
    n=out+'/'+i.replace('/','_')+'.webp'; open(n,'wb').write(d)
    print('wrote',n,fr['Metadata'])
