# Helpers for the TK-105 / TK-108 measurement pass (original solver). source tools/tk_lib.sh ; also source tools/sv_lib.sh first.
# View of sv_c6.ed3 after zoom-fit: N1 (577,406), N2 (1416,826), N3 (1836,616). Results panel lives in the left strip.
# tk_support X Y ICONX ICONY : Propiedades > Vinculacion exterior, pick the node at X,Y, RMB, choose the icon, Aceptar, ESC
tk_support() { bash tools/da.sh "aclick:507,249|wait:500|click:220,36|wait:900|aclick:240,80|wait:1500|amove:$1,$2|wait:400|aclick:$1,$2|wait:600|arclick:$1,$2|wait:2500|aclick:$3,$4|wait:700|aclick:717,704|wait:1000|key:{ESC}|wait:500" 60 1; }
# tk_calc NAME : Calculo > Calcular (dialog: Comprobar las barras already unticked), shot of the outcome before closing it
tk_calc() { bash tools/da.sh "click:419,36|wait:800|click:115,72|wait:3000|aclick:843,626|wait:9000|fshot:$1" 90 1; }
# tk_reportclose : close the "Informe final de calculo" window
tk_reportclose() { bash tools/da.sh "aclick:1292,405|wait:800" 30 1; }
# tk_node NAME X Y : Desplazamientos panel + click a node => tooltip with Dx..Gz
tk_node() { bash tools/da.sh "aclick:290,68|wait:2500|aclick:$2,$3|wait:2500|fshot:$1" 60 1; }
