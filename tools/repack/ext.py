import sys, json, struct, io
from PIL import Image
p, out = sys.argv[1], sys.argv[2]
names = set(n.upper() for n in sys.argv[3:])
with open(p,'rb') as f:
    h=f.read(32); off,ln=struct.unpack_from('<qq',h,16); f.seek(off); j=json.loads(f.read(ln))
    for kind in ('Sprites','Animations'):
        for e in j[kind]:
            if e['Id'].upper() in names or any(n.endswith('*') and e['Id'].upper().startswith(n[:-1]) for n in names):
                frs=[e] if kind=='Sprites' else e['Frames']
                for k,fr in enumerate(frs):
                    f.seek(32+fr['Offset']); im=Image.open(io.BytesIO(f.read(fr['Length'])))
                    n=f"{out}/{e['Id']}_{k}.png"; im.save(n); print(n, im.size, im.mode, fr['Metadata'])
