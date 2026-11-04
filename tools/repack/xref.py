import sys, pefile, struct, re
exe=sys.argv[1]; data=open(exe,'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
off2va=lambda o: pe.get_rva_from_offset(o)+base
text=[s for s in pe.sections if s.Name.startswith(b'.text')][0]
lo=text.PointerToRawData; hi=lo+text.SizeOfRawData
for a in sys.argv[2:]:
    v=int(a,16); hits=[]
    for m in re.finditer(re.escape(struct.pack('<I',v)), data[lo:hi]): hits.append('%x'%off2va(lo+m.start()))
    # call rel32
    for m in re.finditer(rb'\xe8', data[lo:hi]):
        o=lo+m.start()
        if o+5>hi: break
        if off2va(o)+5+struct.unpack_from('<i',data,o+1)[0]==v: hits.append('call@%x'%off2va(o))
    print(a, hits[:40])
