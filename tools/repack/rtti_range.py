import sys, re, pefile, struct
exe=sys.argv[1]; lo=int(sys.argv[2],16); hi=int(sys.argv[3],16)
data=open(exe,'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
off2va=lambda o: pe.get_rva_from_offset(o)+base
for m in re.finditer(rb'\.\?A[UV][^\x00]+\x00', data):
    td=off2va(m.start()-8)
    if not (lo<=td<hi): continue
    n=m.group()[:-1].decode('latin1')
    if 'Functor' in n: continue
    vts=[]
    for c in re.finditer(re.escape(struct.pack('<I',td)), data):
        col=c.start()-12
        if struct.unpack_from('<I',data,col)[0]!=0: continue
        offs=struct.unpack_from('<I',data,col+4)[0]
        for v in re.finditer(re.escape(struct.pack('<I',off2va(col))), data):
            vts.append('off%d vt=%x'%(offs, off2va(v.start())+4))
    print('%x'%td, n[:110], vts)
