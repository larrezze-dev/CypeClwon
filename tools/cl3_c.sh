source tools/cl_lib.sh; source tools/cl3_lib.sh
PP="aclick:771,97|wait:1500|aclick:893,524|wait:300|aclick:919,564|wait:1500|aclick:1100,330|wait:500|amove:1900,820|aclick:1900,820|wait:800|arclick:1490,600|wait:2500"
S="$PP|$OPEN|$(mk cl3_vx2 885 "$(vtyp 5 15)")|$(zm cl3_vx_n1 1167)|$(zm cl3_vx_n3 1815)|$DEL|$REOPEN|$(mk cl3_t2 969 "$(f1 20)")|$(zm cl3_t_n1 1167)|$(zm cl3_t_n3 1815)|$DEL|$REOPEN|$(mk cl3_r2 1011 "$(f1 0.0005)")|$(zm cl3_r_n1 1167)|$(zm cl3_r_n3 1815)"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 500
