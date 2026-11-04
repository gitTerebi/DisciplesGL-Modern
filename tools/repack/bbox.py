import struct, json, sys
ff, packp = sys.argv[1], sys.argv[2]
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
imgs={}
if '-IMAGES.OPT' in files:
    mo,msz=files['-IMAGES.OPT']; q=mo; end=mo+msz-1
    while q<end:
        q+=11+1024; fc=struct.unpack_from('<i',d,q)[0]; q+=4
        for _ in range(fc):
            nm,q=cstr(q); pc,w,h=struct.unpack_from('<iii',d,q); q+=12
            parts=[struct.unpack_from('<6i',d,q+i*24) for i in range(pc)]; q+=pc*24
            imgs[nm.upper()]=(w,h,parts)
with open(packp,'rb') as f:
    hh=f.read(32); o,l=struct.unpack_from('<qq',hh,16); f.seek(o); j=json.loads(f.read(l))
pk={}
for e in j['Sprites']: pk[e['Id'].upper()]=[e['Metadata']]
for e in j['Animations']: pk[e['Id'].upper()]=[fr['Metadata'] for fr in e['Frames']]
anims={}
if '-ANIMS.OPT' in files and '-INDEX.OPT' in files:
    io,isz=files['-INDEX.OPT']; cnt=struct.unpack_from('<i',d,io)[0]; q=io+4; index=[]
    for _ in range(cnt):
        iid=struct.unpack_from('<i',d,q)[0]; nm,q=cstr(q+4); q+=8; index.append((iid,nm.upper()))
    ao,asz=files['-ANIMS.OPT']; q=ao; end=ao+asz-1; blocks=[]
    while q<end:
        fc=struct.unpack_from('<i',d,q)[0]; q+=4; fl=[]
        for _ in range(fc):
            nm,q=cstr(q); fl.append(nm.upper())
        blocks.append(fl)
    names=[nm for iid,nm in index if iid==-1]
    anims=dict(zip(names,blocks))
prefix=sys.argv[3] if len(sys.argv)>3 else ''
def check(name, frames):
    m=pk.get(name)
    if not m: return
    for k,fn in enumerate(frames):
        if fn not in imgs or k>=len(m): continue
        w,h,parts=imgs[fn]
        if not parts: continue
        x0=min(p[0] for p in parts); y0=min(p[1] for p in parts); x1=max(p[0]+p[4] for p in parts); y1=max(p[1]+p[5] for p in parts)
        md=m[k]; pb=(md['OffsetX'],md['OffsetY'],md['OffsetX']+md['Width'],md['OffsetY']+md['Height'])
        if (x0,y0,x1,y1)!=pb:
            print('%-28s f%-3d canvas %dx%d 1x bbox %s pack %s d=(%+d,%+d,%+d,%+d)'%(name,k,w,h,(x0,y0,x1,y1),pb,pb[0]-x0,pb[1]-y0,pb[2]-x1,pb[3]-y1))
for nm in sorted(imgs):
    if nm.startswith(prefix) and nm in pk: check(nm,[nm])
for nm in sorted(anims):
    if nm.startswith(prefix): check(nm,anims[nm])
