# t5 helper: t5s NAME  -> run da steps then fshot NAME and write half-size copy
t5() { bash tools/da.sh "$1|wait:600|fshot:$2" ${3:-90} 1; python -c "
from PIL import Image
im=Image.open('docs/screenshots/$2.png'); im.resize((960,540)).save('docs/screenshots/$2s.png')"; }
