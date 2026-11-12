# Helpers for the diaphragm/spring/constraint measurement pass (RE-ANALYST, 2026-11-12). source tools/rr_lib.sh
# rr_calc NAME : Calculo > Calcular (NL dialog layout: Aceptar 746,740, "Comprobar las barras" 721,678 forced unticked), report shot, close report.
rr_calc() { bash tools/da.sh "aclick:419,36|wait:1000|aclick:115,72|wait:3000|fshot:${1}_dlg|aclick:810,739|wait:12000|fshot:$1|aclick:1292,405|wait:1000" 90 1; }
# rr_nl_disp NAME ITEM : open Desplazamientos, pick "Combinaciones no lineales", item ITEM (0-based), click the bar and capture the whole screen
rr_nl_disp() { local y=$((398 + $2*20)); bash tools/da.sh "aclick:290,68|wait:2500|aclick:225,347|wait:1000|aclick:80,389|wait:1500|aclick:225,378|wait:1000|aclick:60,$y|wait:1500|aclick:${3:-1329},${4:-678}|wait:2500|fshot:$1" 60 1; }
# rr_calc_lin : same as rr_calc for a linear-only obra (Aceptar 843,627)
rr_calc_lin() { bash tools/da.sh "aclick:419,36|wait:1000|aclick:115,72|wait:3000|aclick:843,627|wait:12000|fshot:$1|aclick:1292,405|wait:1000" 90 1; }
