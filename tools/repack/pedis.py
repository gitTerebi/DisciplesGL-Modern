import sys, pefile, struct, capstone
exe=sys.argv[1]; data=open(exe,'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
va2off=lambda v: pe.get_offset_from_rva(v-base)
md=capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def dis(va, n=80):
    o=va2off(va)
    for i,ins in enumerate(md.disasm(data[o:o+n*8], va)):
        print('  %x: %s %s'%(ins.address, ins.mnemonic, ins.op_str))
        if ins.mnemonic=='ret' or i>=n: break
for a in sys.argv[2:]:
    if a.startswith('vt:'):
        v,cnt=a[3:].split(','); v=int(v,16)
        print('vtable %x:'%v, ['%x'%struct.unpack_from('<I',data,va2off(v)+4*k)[0] for k in range(int(cnt))])
    else:
        va,n=(a.split(',')+['60'])[:2]; print('== %s'%va); dis(int(va,16), int(n))
