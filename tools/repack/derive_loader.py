import sys, re, pefile, struct, capstone
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
for exe, create in [("Disciples 2 - Rise of the Elves",0x52e0e7),("Disciples 2 - Dzieje",0x52eb29),("Disciples 2 - Semga",0x52eb29),("Disciples 2",0x52a14e)]:
    data=open(f"G:/games/{exe}/Discipl2.exe",'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
    off2va=lambda o: pe.get_rva_from_offset(o)+base; va2off=lambda v: pe.get_offset_from_rva(v-base)
    rd=lambda v: struct.unpack_from('<I',data,va2off(v))[0]
    m=re.search(rb'\.\?AVCImgFactoryAnimLoaderWeakDup@@\x00', data); td=off2va(m.start()-8); vt=None
    for c in re.finditer(re.escape(struct.pack('<I',td)), data):
        col=c.start()-12
        if struct.unpack_from('<III',data,col)[0]!=0: continue
        for v in re.finditer(re.escape(struct.pack('<I',off2va(col))), data): vt=off2va(v.start())+4
    slot=vt+4; fn=rd(slot)
    print('#',exe,'weakdup vt=%x slot=%x fn=%x'%(vt,slot,fn))
    o=va2off(fn)
    for i in md.disasm(data[o:o+40],fn):
        print('    %x: %s %s'%(i.address,i.mnemonic,i.op_str))
        if i.mnemonic in('ret','jmp'): break
    o=va2off(create)
    for k,i in enumerate(md.disasm(data[o:o+120],create)):
        if i.mnemonic=='call' and 'dword ptr' in i.op_str:
            print('  create first vcall at %x: %s'%(i.address,i.op_str)); break
