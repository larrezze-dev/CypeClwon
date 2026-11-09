source tools/cl_lib.sh; source tools/cl3_lib.sh
S="$OPEN|$(mk cl3_g0 843 "$(f1 5)")|aclick:136,36|wait:1500|aclick:752,68|wait:1200|aclick:1100,330|wait:500|amove:1900,820|aclick:1900,820|wait:800|fshot:cl3_lb1|arclick:1490,600|wait:2500|fshot:cl3_lb2|key:{ENTER}|wait:1500|fshot:cl3_lb3|aclick:292,36|wait:1500|aclick:633,80|wait:1200|fshot:cl3_lb4"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 400
