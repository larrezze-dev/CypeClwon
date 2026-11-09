import sys
from PIL import Image
out=sys.argv[1]; cols=int(sys.argv[2]); ims=[Image.open(p) for p in sys.argv[3:]]
w=max(i.width for i in ims); h=max(i.height for i in ims); rows=(len(ims)+cols-1)//cols
m=Image.new('RGB',(w*cols,h*rows),'white')
for k,i in enumerate(ims): m.paste(i,((k%cols)*w,(k//cols)*h))
m.save(out)
