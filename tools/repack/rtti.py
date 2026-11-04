import sys, re, pefile, struct, hashlib
exe=sys.argv[1]; pat=re.compile(sys.argv[2].encode(), re.I)
data=open(exe,'rb').read(); print('sha256', hashlib.sha256(data).hexdigest())
pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
def off2va(o): return pe.get_rva_from_offset(o)+base
def va2off(v): return pe.get_offset_from_rva(v-base)
rd=[s for s in pe.sections if s.Name.startswith(b'.rdata')][0]
names=[]
for m in re.finditer(rb'\.\?A[UV][A-Za-z0-9_@?$]+@@\x00', data):
    n=m.group()[:-1]
    if pat.search(n): names.append((m.start(), n))
for o,n in names:
    td=off2va(o-8)  # TypeDescriptor: vfptr, spare, name
    # find COLs referencing td: COL = sig, offset, cdOffset, pTypeDescriptor, pClassHierarchy
    tdb=struct.pack('<I',td); vts=[]
    for c in re.finditer(re.escape(tdb), data):
        col=c.start()-12
        sig,offs,cd=struct.unpack_from('<III',data,col)
        if sig!=0: continue
        colva=off2va(col)
        for v in re.finditer(re.escape(struct.pack('<I',colva)), data):
            vts.append((offs, off2va(v.start())+4))
    print(n.decode(), 'TD=%x'%td, ['off%d vt=%x'%(a,b) for a,b in vts])
