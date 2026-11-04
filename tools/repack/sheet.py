import sys, glob, os
from PIL import Image
d=sys.argv[1]; ims=[Image.open(p).convert('RGBA') for p in sorted(glob.glob(d+'/*.webp')) if 'ground' not in p]
H=max(i.height for i in ims); W=sum(i.width for i in ims)+10*len(ims)
s=Image.new('RGBA',(W,H),(60,60,60,255)); x=0
for i in ims: s.alpha_composite(i,(x,0)); x+=i.width+10
s.save(d+'/sheet.png'); print(s.size)
