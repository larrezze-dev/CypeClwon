import sys
from PIL import Image
# usage: cl_mont.py out.png cols name1 name2 ... (screenshots in docs/screenshots)
out, cols, names = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
ims = [Image.open(f'D:/Coding/CypeClwon/docs/screenshots/{n}.png') for n in names]
w = max(i.width for i in ims); h = max(i.height for i in ims)
rows = (len(ims) + cols - 1) // cols
m = Image.new('RGB', (w*cols, h*rows), (255, 0, 255))
for k, i in enumerate(ims): m.paste(i, ((k % cols)*w, (k//cols)*h))
m.save(out)
