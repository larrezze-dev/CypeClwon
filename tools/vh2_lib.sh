# Helpers for T-P7-07 pending measurements. source tools/vh2_lib.sh
# panel_close : close the floating "Propiedades - Hipotesis vista" panel if present (fullscreen: red bar at y=5; restored: red bar at y=120)
panel_close() {
  bash tools/da.sh "fshot:vh_tmp" 30 1 >/dev/null
  local st
  st=$(python -c "
from PIL import Image
im=Image.open('docs/screenshots/vh_tmp.png').convert('RGB')
red=lambda p: p[0]>150 and p[1]<60 and p[2]<60
if red(im.getpixel((800,120))) and red(im.getpixel((300,120))): print('mid')
elif red(im.getpixel((800,5))) and im.getpixel((1500,700))[0] in range(235,246) and im.getpixel((1500,900))[0] in range(235,246): print('full')
else: print('none')")
  if false; then :
  elif [ "$st" = mid ]; then bash tools/da.sh "aclick:1708,120|wait:1500" 30 1 >/dev/null; fi
  echo "panel: $st"
}
shot1() { bash tools/da.sh "fshot:$1" 30 1 >/dev/null; python -c "
from PIL import Image
im=Image.open('docs/screenshots/$1.png'); im.resize((960,540)).save('docs/screenshots/$1s.png')"; }
