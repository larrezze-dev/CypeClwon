# Helpers for T-P8-05 measurements in the original (attach driver). source tools/sv_lib.sh
# sv_calc : Calculo > Calcular (dialog remembers "no comprobar"), close report. Requires no floating panel on top.
sv_calc() { bash tools/da.sh "click:419,36|wait:800|click:115,72|wait:3000|aclick:843,626|wait:9000|aclick:1291,404|wait:1500" 90 1; }
# sv_disp NAME BARX BARY : Desplazamientos panel, click bar, full screen shot
sv_disp() { bash tools/da.sh "aclick:290,68|wait:2500|aclick:${2:-1329},${3:-678}|wait:2500|fshot:$1" 60 1; }
sv_closepanel() { bash tools/da.sh "aclick:501,251|wait:800" 30 1; }
# sv_relaunch : close any running original and start a fresh one (never leaves the app running at the end of the session: call sv_close)
sv_relaunch() { powershell -NoProfile -Command 'Get-Process | ? {$_.ProcessName -like "CYPE 3D*"} | Stop-Process -Force'; sleep 3; powershell -NoProfile -Command '$e=(Get-ChildItem "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe").FullName; Start-Process -FilePath $e | Out-Null; Start-Sleep 26'; }
sv_close() { powershell -NoProfile -Command 'Get-Process | ? {$_.ProcessName -like "CYPE 3D*"} | Stop-Process -Force'; }
# sv_new FILE : empty obra + import text file (docs/sample_project/FILE) ; ends on the Geometria tab with the model in view
SV_BASE="click:630,470|wait:3500|click:48,260|wait:3000|click:48,432|wait:4000|click:400,15|wait:500|click:795,749|wait:16000|aclick:799,666|wait:7000"
sv_new() { bash tools/da.sh "$SV_BASE" 120 1; bash tools/da.sh "click:136,36|wait:1000|click:1425,98|wait:3000|click:300,15|wait:500|click:365,53|key:^a|key:D:/Coding/CypeClwon/docs/sample_project/${1}|wait:500|click:47,943|wait:3000|click:240,15|wait:500|click:49,132|wait:3500" 120 1; }
# crop NAME X Y W H : screen crop to docs/screenshots/NAME.png
crop() { bash tools/da.sh "cshot:$1,$2,$3,$4,$5" 30 1 >/dev/null; }
# sv_desc A Iy Iz It E : describe all bars (window selection) as generic section. AV=Avy Avz. Geometria tab > Barras > Seccion
sv_desc() { # args: A AvY AvZ Iy Iz It E nu
  local setf; setf(){ echo "aclick:$1,$2|rkey:^a|rkey:$3|rkey:{TAB}|wait:300"; }
  bash tools/da.sh "click:136,36|wait:800|click:299,97|wait:1000|aclick:700,250|wait:400|amove:1850,950|aclick:1850,950|wait:600|arclick:1850,950|wait:600|arclick:1850,950|wait:2500|aclick:788,258|wait:1500|$(setf 590 511 $1)|$(setf 590 540 $2)|$(setf 590 569 $3)|$(setf 590 598 $4)|$(setf 590 627 $5)|$(setf 590 656 $6)|$(setf 862 511 $7)|$(setf 862 540 ${8:-0.3})|$(setf 862 598 ${9:-0.01})|amove:700,800|fshot:sv_desc_chk" 150 1; }
# sv_panel : the floating results panel opens maximised on a fresh obra: restore, narrow and move it to the left strip (x 8..528)
sv_panel() { bash tools/da.sh "aclick:1848,13|wait:1200|drag:L,1544,300,520,300|wait:800|drag:L,100,16,-85,149|wait:1000" 60 1; }
# sv_eff NAME X Y : in the Esfuerzos panel (already open, left strip) tick all 6 laws + "solo seleccionadas" + "consultar", select the bar at X,Y and RMB; shot of the tooltip area
sv_eff() { bash tools/da.sh "aclick:29,346|wait:500|aclick:45,368|wait:500|aclick:61,401|aclick:61,430|aclick:61,459|aclick:61,488|aclick:61,517|aclick:61,546|wait:500|aclick:$2,$3|wait:600|aclick:$2,$3|wait:600|arclick:$2,$3|wait:2500|cshot:$1,$(( $2 - 160 )),$(( $3 - 300 )),640,420" 90 1; }
