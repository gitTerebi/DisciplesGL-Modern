import sys, re, pefile, struct, hashlib, capstone
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for exe in sys.argv[1:]:
    data=open(exe,'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
    off2va=lambda o: pe.get_rva_from_offset(o)+base; va2off=lambda v: pe.get_offset_from_rva(v-base)
    rd32=lambda v: struct.unpack_from('<I',data,va2off(v))[0]
    print('#', exe, hashlib.sha256(data).hexdigest()[:16])
    def vt_of(namepat):
        m=re.search(namepat, data)
        if not m: return None
        td=off2va(m.start()-8)
        for c in re.finditer(re.escape(struct.pack('<I',td)), data):
            col=c.start()-12
            if struct.unpack_from('<III',data,col)[0]!=0: continue
            for v in re.finditer(re.escape(struct.pack('<I',off2va(col))), data): return off2va(v.start())+4
    vt=vt_of(rb'\.\?AVFactoryImage2@\?%[^\x00]*ImgFactory\.cpp')
    if vt is None: print('  no FactoryImage2 RTTI'); continue
    render=rd32(vt+0x14); dtor=rd32(vt)
    print('  FactoryImage2 vt=%x render=%x dtor=%x'%(vt,render,dtor))
    text=[s for s in pe.sections if s.Name.startswith(b'.text')][0]; lo=text.PointerToRawData; hi=lo+text.SizeOfRawData
    # constructor: writes vt imm
    for m in re.finditer(re.escape(struct.pack('<I',vt)), data[lo:hi]):
        print('  vt write at %x'%off2va(lo+m.start()-2))
    # builder: function that calls ctor; find ctor start by scanning back for 'mov eax, imm; call' EH prolog
    w=off2va(lo+[m.start() for m in re.finditer(re.escape(struct.pack('<I',vt)), data[lo:hi])][0])
    # search back for b8 xx xx xx xx e8 (EH prolog) within 0x80
    o=va2off(w)
    for k in range(o-0x80,o):
        if data[k]==0xb8 and data[k+5]==0xe8: ctor=off2va(k)
    callers=[]
    for m in re.finditer(rb'\xe8', data[lo:hi]):
        oo=lo+m.start()
        if off2va(oo)+5+struct.unpack_from('<i',data,oo+1)[0]==ctor: callers.append(off2va(oo))
    print('  ctor=%x callers=%s'%(ctor,['%x'%c for c in callers]))
    for c in callers:
        o=va2off(c)
        for k in range(o-0xc0,o):
            if data[k]==0xb8 and data[k+5]==0xe8: b=off2va(k)
        bc=[]
        for m in re.finditer(rb'\xe8', data[lo:hi]):
            oo=lo+m.start()
            if off2va(oo)+5+struct.unpack_from('<i',data,oo+1)[0]==b: bc.append('%x'%off2va(oo))
        print('  builder=%x builder callers=%s'%(b,bc))
    ins=list(md.disasm(data[va2off(render):va2off(render)+0x100], render))
    calls=[(i.address,i.op_str) for i in ins if i.mnemonic=='call']
    print('  render calls', calls[:4], 'ret', [i.op_str for i in ins if i.mnemonic=='ret'][:1])
