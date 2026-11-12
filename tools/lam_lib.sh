# Helpers for the shell (lamina) measurement pass. source tools/lam_lib.sh (after tools/sv_lib.sh)
# lam_calc : Calculo > Calcular for an obra without bars (dialog without "Comprobar las barras")
lam_calc() { bash tools/da.sh "click:419,36|wait:800|click:115,72|wait:3000|aclick:843,601|wait:${1:-15000}" 120 1; }
# lam_view : open the isovalues viewer (Calculo > Isovalores) with Isovalores radio, then arm Esfuerzos > Consultar
lam_view() { bash tools/da.sh "click:419,36|wait:800|aclick:388,68|wait:3500|aclick:38,240|wait:2500" 60 1; }
# lam_sel CAT ITEM : CAT 0=Desplazamientos 1=Esfuerzos 2=Tensiones ; ITEM index from 0 of the second combo
lam_sel() { local y1=$((303 + $1*15)); local y2=$((335 + $2*15)); bash tools/da.sh "aclick:200,283|wait:800|aclick:60,$y1|wait:2200|aclick:200,316|wait:800|aclick:60,$y2|wait:2500" 60 1; }
# lam_q X Y NAME : arm Consultar and click a point of the viewer, crop of the tooltip
lam_q() { bash tools/da.sh "aclick:113,33|wait:700|aclick:155,57|wait:700|amove:$(( $1 - 40 )),$(( $2 - 30 ))|wait:300|aclick:$1,$2|wait:1500|cshot:$3,$(( $1 - 150 )),$(( $2 - 130 )),420,200" 60 1; }
# lam_close : close the viewer (Archivo > Terminar)
lam_close() { bash tools/da.sh "aclick:24,33|wait:800|aclick:65,65|wait:2500" 30 1; }
# lam_leg NAME : crop of the colour legend (extremes) + window title
lam_leg() { bash tools/da.sh "cshot:${1}_t,0,0,700,24|cshot:${1},0,978,1920,38" 30 1; }
# lam_mesh S : Propiedades > Discretizacion, maximum triangle side S (m) for the lamina at 1491,520 (main window must be active, no viewer open)
lam_mesh() { bash tools/da.sh "click:220,36|wait:1000|aclick:972,68|wait:1200|amove:1491,520|wait:500|aclick:1491,520|wait:700|arclick:1491,520|wait:2500|aclick:933,561|rkey:^a|rkey:$1|rkey:{TAB}|wait:400|aclick:696,604|wait:1500|key:{ESC}|wait:500" 60 1; }
# lam_cap TAG : in an open viewer (scale already 0) capture legends of Dz,Mx,My,Mxy and centre values of Dz,Mx,My
lam_cap() { local TAG=$1 k a b c
  for k in "0 2 d" "1 3 mx" "1 4 my" "1 5 mxy"; do read a b c <<< "$k"; lam_sel $a $b; lam_leg lam_${TAG}_leg_$c; done
  for k in "0 2 d" "1 3 mx" "1 4 my"; do read a b c <<< "$k"; lam_sel $a $b; lam_q 960 800 lam_${TAG}_c_$c; done; bash tools/da.sh "fshot:lam_${TAG}_full" 30 1; }
# lam_run S TAG : full mesh run: set mesh, calc, view (scale 0), capture, close
lam_run() { lam_mesh $1; lam_calc; lam_view; bash tools/da.sh "aclick:290,115|rkey:^a|rkey:0|rkey:{TAB}|wait:1500" 30 1; lam_cap $2; lam_close; }
# bq NAME X Y : in the open Esfuerzos panel (Solo seleccionadas + Consultar ticked) click a band/bar point and RMB, crop of the tooltip
bq() { bash tools/da.sh "amove:$2,$3|wait:400|aclick:$2,$3|wait:600|aclick:$2,$3|wait:600|arclick:$2,$3|wait:2200|cshot:$1,$(( $2 - 160 )),$(( $3 - 300 )),640,420" 90 1; }
# lam_legs TAG : in an open viewer (scale 0) capture the legend extremes of Dz, Mx, My, Mxy, Qx(Cortante X), Qy into one montage tag_legs.png
lam_legs() { local TAG=$1 k a b c
  for k in "0 2 d" "1 3 mx" "1 4 my" "1 5 mxy" "1 7 vx" "1 8 vy"; do read a b c <<< "$k"; lam_sel $a $b; lam_leg ${TAG}_$c; done
  python -I tools/lam_mont.py ${TAG} d mx my mxy vx vy; }
# lam_fact TAG F1..F8 : Propiedades > Factores de rigidez (order Fxx Fxy Fyy Mxx Mxy Myy Vxz Vyz) on the lamina, calculate, capture legends
lam_fact() { local TAG=$1; shift; local s="click:220,36|wait:900|click:975,98|wait:1500|amove:1491,520|wait:400|aclick:1491,520|wait:700|arclick:1491,520|wait:2500" y=412 v
  for v in "$@"; do s="$s|aclick:1010,$y|rkey:^a|rkey:$v|rkey:{TAB}|wait:200"; y=$((y+29)); done
  bash tools/da.sh "$s|wait:500|cshot:${TAG}_dlg,870,360,190,330|aclick:923,659|wait:1500|key:{ESC}" 60 1
  lam_calc 15000; lam_view; bash tools/da.sh "aclick:290,115|rkey:^a|rkey:0|rkey:{TAB}|wait:1500" 30 1; lam_legs $TAG; lam_close; }
# lam_legsm TAG : membrane variant of lam_legs (Dx, Dz, Axil X, Axil Y, Axil XY, Cortante X)
lam_legsm() { local TAG=$1 k a b c
  for k in "0 0 ux" "0 2 uz" "1 0 nx" "1 1 ny" "1 2 nxy" "1 7 vx"; do read a b c <<< "$k"; lam_sel $a $b; lam_leg ${TAG}_$c; done
  python -I tools/lam_mont.py ${TAG} ux uz nx ny nxy vx; }
# lam_factm TAG F1..F8 : lam_fact with the membrane legends
lam_factm() { local TAG=$1; shift; local s="click:220,36|wait:900|click:975,98|wait:1500|amove:1329,580|wait:400|aclick:1329,580|wait:700|arclick:1329,580|wait:2500" y=412 v
  for v in "$@"; do s="$s|aclick:1010,$y|rkey:^a|rkey:$v|rkey:{TAB}|wait:200"; y=$((y+29)); done
  bash tools/da.sh "$s|wait:500|cshot:${TAG}_dlg,870,360,190,330|aclick:923,659|wait:1500|key:{ESC}" 60 1
  lam_calc 15000; lam_view; bash tools/da.sh "aclick:290,115|rkey:^a|rkey:0|rkey:{TAB}|wait:1500" 30 1; lam_legsm $TAG; lam_close; }
