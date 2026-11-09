import sys
from PIL import Image
out=sys.argv[1]; ims=[Image.open(p) for p in sys.argv[2:]]
w=sum(i.width for i in ims); h=max(i.height for i in ims)
m=Image.new('RGB',(w,h),'white'); x=0
for i in ims: m.paste(i,(x,0)); x+=i.width
m.save(out)
