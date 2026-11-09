source tools/cl_lib.sh; source tools/cl3_lib.sh
PP="aclick:771,97|wait:1500|aclick:893,524|wait:300|aclick:919,564|wait:1500|aclick:1100,330|wait:500|amove:1900,820|aclick:1900,820|wait:800|arclick:1490,600|wait:2500|fshot:cl3_pp_off"
S="$PP|$OPEN|$(mk cl3_vx 885 "$(vtyp 5 15)")|$DEL|$REOPEN|$(mk cl3_vy 927 "$(vtyp 5 15)")|$DEL|$REOPEN|$(mk cl3_t 969 "$(f1 20)")|$DEL|$REOPEN|$(mk cl3_r 1011 "$(f1 0.0005)")|$DEL|$REOPEN|$(mk cl3_u 843 "$(f1 5)")"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 500
