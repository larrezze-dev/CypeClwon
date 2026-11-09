source tools/cl_lib.sh; source tools/cl3_lib.sh
S="$OPEN|$(mk cl3_g0 843 "$(f1 5)")|aclick:136,36|wait:1500|fshot:cl3_geo"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 400
