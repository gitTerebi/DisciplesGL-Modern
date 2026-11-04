import sys, re, pefile, struct, hashlib
for exe in sys.argv[1:]:
    data = open(exe, 'rb').read(); pe = pefile.PE(data=data); base = pe.OPTIONAL_HEADER.ImageBase
    off2va = lambda o: pe.get_rva_from_offset(o) + base
    va2off = lambda v: pe.get_offset_from_rva(v - base)
    def vts(name):
        out = []
        for m in re.finditer(re.escape(name) + rb'\x00', data):
            td = off2va(m.start() - 8)
            for c in re.finditer(re.escape(struct.pack('<I', td)), data):
                col = c.start() - 12
                if struct.unpack_from('<I', data, col)[0] != 0: continue
                colva = off2va(col)
                for v in re.finditer(re.escape(struct.pack('<I', colva)), data):
                    out.append(off2va(v.start()) + 4)
        return out
    print(exe, hashlib.sha256(data).hexdigest()[:8])
    for nm in (b'.?AVImage@CImgStore2@@', b'.?AVCFFFactory@'):
        if nm.endswith(b'@'):
            m = re.search(re.escape(nm) + rb'[^\x00]*', data); nm = m.group()
        for vt in vts(nm):
            print('  ', nm[:30], 'vt %x' % vt, ['%x' % struct.unpack_from('<I', data, va2off(vt) + 4 * k)[0] for k in range(6)])
