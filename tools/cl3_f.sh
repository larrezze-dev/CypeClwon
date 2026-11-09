source tools/cl_lib.sh; source tools/cl3_lib.sh
PP="aclick:771,97|wait:1500|aclick:893,524|wait:300|aclick:919,564|wait:1500|aclick:1100,330|wait:500|amove:1900,820|aclick:1900,820|wait:800|arclick:1490,600|wait:2500"
S="$PP|$OPEN|$(mk cl3_two1 885 "$(vtyp 5 15)")|$REOPEN|$(mk cl3_two2 843 "$(f1 5)")|$EDITOPEN|fshot:cl3_ed2|aclick:1074,707|wait:800|aclick:697,97|wait:1200|aclick:1490,600|wait:800|aclick:1490,600|wait:800|arclick:1490,600|wait:2000|fshot:cl3_bd_a|aclick:1490,600|wait:800|arclick:1490,600|wait:2000|fshot:cl3_bd_b"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 500
