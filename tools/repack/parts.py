import struct, sys
ff=sys.argv[1]; want=set(a.upper() for a in sys.argv[2:])
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
mo,msz=files['-IMAGES.OPT']; q=mo; end=mo+msz-1; bi=0
while q<end:
    sx,sy=struct.unpack_from('<ii',d,q+3); q+=11+1024
    fc=struct.unpack_from('<i',d,q)[0]; q+=4
    for _ in range(fc):
        nm,q=cstr(q); pc,w,h=struct.unpack_from('<iii',d,q); q+=12
        parts=[struct.unpack_from('<6i',d,q+i*24) for i in range(pc)]; q+=pc*24
        if nm.upper() in want:
            print('block',bi,'sheet',sx,sy,nm,w,h,'parts',pc)
            for pt in parts: print('   dst %d,%d src %d,%d size %dx%d'%pt)
    bi+=1
