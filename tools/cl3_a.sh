source tools/cl_lib.sh; source tools/cl3_lib.sh
S="$OPEN|$(mk cl3_vx 885 "$(vtyp 5 15)")|$DEL|$REOPEN|$(mk cl3_vy 927 "$(vtyp 5 15)")|fshot:cl3_chk"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 300
