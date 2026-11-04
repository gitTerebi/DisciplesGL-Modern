import sys, json, struct, collections
p, out = sys.argv[1], sys.argv[2]
with open(p,'rb') as f:
    h=f.read(32)
    fmt=struct.unpack_from('<i',h,12)[0]; off,ln=struct.unpack_from('<qq',h,16)
    f.seek(off); m=f.read(ln)
open(out+'/manifest.json','wb').write(m)
j=json.loads(m)
print('format',fmt,'manifest',ln,'bytes')
print({k:(v if not isinstance(v,list) else len(v)) for k,v in j.items()})
def cat(i): 
    parts=i.split('/'); return parts[0]
for kind in ('Sprites','Animations','Sounds'):
    c=collections.Counter(cat(e['Id']) for e in j[kind])
    print('==',kind); [print('  ',k,n) for k,n in sorted(c.items())]
s=j['Sprites'][0]; print(json.dumps(s)[:600])
a=j['Animations'][0]; print(json.dumps({**a,'Frames':a['Frames'][:1]})[:800])
sc=collections.Counter()
for e in j['Sprites']: sc[e['Metadata']['PixelWidth']//max(1,e['Metadata']['Width'])]+=1
for e in j['Animations']:
    for fr in e['Frames']: sc[fr['Metadata']['PixelWidth']//max(1,fr['Metadata']['Width'])]+=1
print('pixel scales',sc)
