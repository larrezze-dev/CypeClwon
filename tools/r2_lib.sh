# Helpers for the fourth measurement pass (RE-ANALYST, 2026-11-12). source tools/sv_lib.sh tools/rr_lib.sh tools/r2_lib.sh first.
# r2_new FILE : relaunch the original, new obra "nuevo" (answers the overwrite prompt), import docs/sample_project/FILE, ends on the Geometria tab
r2_new() {
  sv_relaunch
  bash tools/da.sh "click:630,470|wait:3500|click:48,260|wait:3000|aclick:840,572|wait:3500|click:48,432|wait:4000|click:400,15|wait:500|click:795,749|wait:16000|aclick:799,666|wait:7000" 120 1
  bash tools/da.sh "click:136,36|wait:1000|click:1425,98|wait:3000|click:300,15|wait:500|click:365,53|key:^a|key:D:/Coding/CypeClwon/docs/sample_project/${1}|wait:500|click:47,943|wait:3000|click:240,15|wait:500|click:49,132|wait:3500" 120 1
}
# r2_desc : generic section A=100 Avy=Avz=40 Iyy=1e4 Izz=5e3 It=8e3 E=210000 nu=0.3 gamma=0.01 for all bars, accept
r2_desc() { sv_desc 100 40 40 10000 5000 8000 210000 0.3 0.01; bash tools/da.sh "aclick:49,942|wait:2500" 30 1; }
# r2_sup X Y ICONX ICONY : Propiedades > Vinculacion exterior on node at X,Y with the given icon, Aceptar, ESC
r2_sup() { bash tools/da.sh "aclick:1200,13|wait:400|click:220,36|wait:900|aclick:240,80|wait:1500|amove:$1,$2|wait:400|aclick:$1,$2|wait:600|arclick:$1,$2|wait:2500|aclick:$3,$4|wait:700|aclick:717,704|wait:1000|key:{ESC}|wait:500" 60 1; }
# r2_act : activate the main window by clicking its title bar
r2_act() { bash tools/da.sh "aclick:1200,13|wait:400" 20 1; }
# r2_calc NAME : Calculo > Calcular (linear obra, Comprobar las barras ticked by default -> unticked), report shot NAME, close report
r2_calc() { bash tools/da.sh "aclick:1200,13|wait:500|aclick:419,36|wait:1000|aclick:115,72|wait:3000|aclick:880,408|wait:400|aclick:818,565|wait:500|aclick:843,627|wait:12000|fshot:$1|aclick:1292,405|wait:1000" 90 1; }
# r2_tip NAME X Y : open Desplazamientos (first time) is NOT done here; click a bar with the panel open, full screen shot
r2_tip() { bash tools/da.sh "aclick:1400,13|wait:500|amove:$(( $2 - 10 )),$(( $3 - 10 ))|wait:400|aclick:$2,$3|wait:2500|fshot:$1" 30 1; }
# r2_diaf : full build of the three-column diaphragm obra (rr_diaf.txt) with Fx=30, Fy=20 at (1,2) on D1 (view after import: wheel -6 at 1160,600)
r2_diaf() { local F=${1:-rr_diaf.txt}
  r2_new $F
  bash tools/da.sh "amove:1160,600|wait:300|wheel:-6|wait:1500" 30 1
  r2_desc
  # bases: window over the three base nodes
  bash tools/da.sh "aclick:1200,13|wait:400|click:220,36|wait:900|aclick:240,80|wait:1500|aclick:1100,580|wait:500|amove:1400,700|wait:300|amove:1700,760|wait:300|aclick:1700,760|wait:600|arclick:1700,760|wait:2500|aclick:751,376|wait:700|aclick:717,704|wait:1000|key:{ESC}|wait:500" 60 1
  # diaphragm editor
  bash tools/da.sh "aclick:40,68|wait:3000|aclick:1157,279|wait:1500|aclick:20,46|wait:1500|aclick:150,95|wait:800|aclick:1275,71|key:D1|key:{TAB}|wait:600|aclick:604,132|wait:1500|aclick:1100,179|key:30|key:{TAB}|wait:300|key:20|key:{TAB}|wait:300|key:1|key:{TAB}|wait:300|key:2|key:{TAB}|wait:500|fshot:r2_diaf_editor|aclick:43,1013|wait:2000" 90 1
  # assign: the panel opens maximised, restore + narrow + move it, then window over the three tops
  bash tools/da.sh "aclick:1200,13|wait:400|aclick:53,98|wait:2000|aclick:1848,13|wait:1200|drag:L,1544,300,520,300|wait:800|drag:L,100,16,-85,149|wait:1000|aclick:1200,13|wait:500|aclick:1100,330|wait:500|amove:1400,450|wait:300|amove:1700,560|wait:300|aclick:1700,560|wait:600|arclick:1700,560|wait:2500|fshot:r2_diaf_done" 90 1
}
