source tools/cl_lib.sh; source tools/cl3_lib.sh
PP="aclick:771,97|wait:1500|aclick:893,524|wait:300|aclick:919,564|wait:1500|aclick:1100,330|wait:500|amove:1900,820|aclick:1900,820|wait:800|arclick:1490,600|wait:2500"
CS="cshot:cl3_dlg_gl,810,312,310,420"
S="$PP|$OPEN|aclick:831,552|wait:500|$CS|$ACC|wait:1200|cshot:cl3_gl1,1100,340,820,520|$DEL|$REOPEN|aclick:831,552|wait:300|aclick:900,652|wait:500|cshot:cl3_dlg_gl2,810,312,310,420|$ACC|wait:1200|cshot:cl3_gl2,1100,340,820,520|$DEL|$REOPEN|aclick:831,552|wait:300|aclick:900,652|wait:300|aclick:885,427|wait:600|cshot:cl3_dlg_gl3,810,312,310,420|$ACC|wait:1200|cshot:cl3_gl3,1100,340,820,520"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 500
