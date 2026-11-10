# Helpers for T-P7-05 tanda 3 (paños / cargas superficiales). source tools/cp_lib.sh
# cp_launch : start the original (never killed by the attach driver) and wait for the window
cp_launch() { powershell -NoProfile -Command '$e=(Get-ChildItem "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe").FullName; Start-Process -FilePath $e | Out-Null; Start-Sleep 26'; }
# BASEPRE : wizard empty obra up to the Carga tab (steps are window-relative; the window is maximised at 0,0)
BASEPRE="click:630,470|wait:3500|click:48,260|wait:3000|click:48,432|wait:4000|click:400,15|wait:500|click:795,749|wait:6500|click:180,15|wait:1200|click:47,412|wait:6000"
# IMPORT FILE : Geometria import text file, then Carga tab
cp_import() { echo "click:136,36|wait:1000|click:1425,98|wait:3000|click:300,15|wait:500|click:365,53|key:^a|key:D:/Coding/CypeClwon/docs/sample_project/${1}|wait:500|click:47,943|wait:3000|click:240,15|wait:500|click:49,132|wait:3500|click:292,36|wait:1500"; }
# cp_close : close the original: Alt+F4 then No on the save prompt
cp_close() { powershell -NoProfile -Command 'Get-Process | ? {$_.ProcessName -like "CYPE 3D*"} | Stop-Process -Force'; }
# tooltip NAME X Y : hover and capture region below the cursor
tt() { echo "amove:900,700|wait:500|amove:$2,$3|wait:1500|cshot:cp_tt_$1,$(( $2 - 60 )),$(( $3 + 15 )),780,300"; }
