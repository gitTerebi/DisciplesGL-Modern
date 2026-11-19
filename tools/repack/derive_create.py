import sys, re, pefile, struct, hashlib
def run(exe, still_site, anim_site):
    data=open(exe,'rb').read(); pe=pefile.PE(data=data); base=pe.OPTIONAL_HEADER.ImageBase
    off2va=lambda o: pe.get_rva_from_offset(o)+base; va2off=lambda v: pe.get_offset_from_rva(v-base)
    text=[s for s in pe.sections if s.Name.startswith(b'.text')][0]; lo=text.PointerToRawData; hi=lo+text.SizeOfRawData
    calls={}
    for m in re.finditer(rb'\xe8', data[lo:hi]):
        o=lo+m.start()
        if o+5>hi: break
        t=off2va(o)+5+struct.unpack_from('<i',data,o+1)[0]
        calls.setdefault(t,[]).append(off2va(o))
    targets=sorted(calls)
    import bisect
    def entry(site):
        i=bisect.bisect_right(targets, site)-1
        return targets[i]
    sf=entry(still_site); af=entry(anim_site)
    sc=calls.get(sf,[]); ac=calls.get(af,[])
    print('#',exe.split('/')[-2], hashlib.sha256(data).hexdigest()[:16])
    print('  still func %x callers %s | anim func %x callers %s'%(sf,[hex(x) for x in sc],af,[hex(x) for x in ac]))
    for s in sc:
        for a in ac:
            if abs(s-a)<0x40:
                cf=entry(min(s,a)); print('  create func %x callers %s'%(cf,[hex(x) for x in calls.get(cf,[])]))
run("G:/games/Disciples 2 - Rise of the Elves/Discipl2.exe",0x52dc06,0x52dd56)
run("G:/games/Disciples 2 - Dzieje/Discipl2.exe",0x52e663,0x52e798)
run("G:/games/Disciples 2 - Semga/Discipl2.exe",0x52e663,0x52e798)
run("G:/games/Disciples 2/Discipl2.exe",0x529c8b,0x529dbd)
