import sys
from PIL import Image
f=sys.argv[1]; xs=[int(a) for a in sys.argv[2:]]
im=Image.open(f).convert('RGB'); W,H=im.size
for x in xs:
    out=[];cur=None
    for y in range(200,H-100):
        p=im.getpixel((x,y)); w=all(c>245 for c in p)
        if not w:
            if cur is None: cur=[y,y,p]
            else: cur[1]=y
        else:
            if cur: out.append(tuple(cur)); cur=None
    print(x,[(r[0],r[1],r[1]-r[0]+1,r[2]) for r in out if r[1]-r[0]>5])
