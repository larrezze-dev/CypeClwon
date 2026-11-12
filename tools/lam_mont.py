# Stack the legend crops docs/screenshots/<tag>_<k>.png (and the title crop <tag>_<k>_t.png) into <tag>_legs.png
import sys
from PIL import Image
tag, keys = sys.argv[1], sys.argv[2:]
d = "D:/Coding/CypeClwon/docs/screenshots/"
rows = []
for k in keys:
    t = Image.open(f"{d}{tag}_{k}_t.png"); l = Image.open(f"{d}{tag}_{k}.png")
    rows += [t.crop((0, 0, 700, 24)), l]
w = max(r.width for r in rows); h = sum(r.height for r in rows)
o = Image.new("RGB", (w, h), "white"); y = 0
for r in rows: o.paste(r, (0, y)); y += r.height
o.save(f"{d}{tag}_legs.png")
