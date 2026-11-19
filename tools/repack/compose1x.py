import struct, sys
from PIL import Image
ff=sys.argv[1]; out=sys.argv[2]; names=[a.upper() for a in sys.argv[3:]]
d=open(ff,'rb').read()
recs={}; p=28
while p < len(d)-1:
    rid,size,stored=struct.unpack_from('<iii',d,p+8); recs[rid]=(p+28,size); p+=28+stored
off,_=recs[2]; n=struct.unpack_from('<i',d,off)[0]; q=off+4; files={}
for _ in range(n):
    name=d[q:q+256].split(b'\0')[0].decode('latin1'); fid=struct.unpack_from('<i',d,q+256)[0]; q+=260
    if fid in recs: files[name.upper()]=recs[fid]
def cstr(p):
    e=d.index(b'\0',p); return d[p:e].decode('latin1'), e+1
import io
raws={k:v for k,v in files.items() if k.endswith('.PNG')}
sheets={}
for k,(o,s) in raws.items():
    im=Image.open(io.BytesIO(d[o:o+s])); sheets.setdefault(im.size,[]).append(k)
mo,msz=files['-IMAGES.OPT']; q=mo; end=mo+msz-1; imgs={}
while q<end:
    key=d[q:q+3]; sx,sy=struct.unpack_from('<ii',d,q+3); q+=11+1024
    fc=struct.unpack_from('<i',d,q)[0]; q+=4
    for _ in range(fc):
        nm,q=cstr(q); pc,w,h=struct.unpack_from('<iii',d,q); q+=12
        parts=[struct.unpack_from('<6i',d,q+i*24) for i in range(pc)]; q+=pc*24
        imgs[nm.upper()]=(sx,sy,w,h,parts)
canvas=Image.new('RGBA',(640,600),(255,0,255,255))
for nm in names:
    sx,sy,w,h,parts=imgs[nm]
    cands=sheets.get((sx,sy),[])
    pick=[c for c in cands if c[:3]==nm[:3]] or cands
    print(nm,'sheet',(sx,sy),cands)
    o,s=raws[pick[0]]; sh=Image.open(io.BytesIO(d[o:o+s])).convert('RGBA')
    px=sh.load()
    for (x,y,a,b,pw,ph) in parts:
        tile=sh.crop((a,b,a+pw,b+ph))
        t=tile.load()
        for yy in range(ph):
            for xx in range(pw):
                c=t[xx,yy]
                if c[:3]!=(255,0,255): canvas.putpixel((x+xx,y+yy),c)
canvas.save(out)
