source tools/cl_lib.sh; source tools/cl3_lib.sh
H() { echo "amove:$2,652|wait:1500|cshot:$1,800,600,560,200"; }
T=""
for i in 1 2 3 4 5 6 7 8 9 10; do T="$T|key:{TAB}|wait:300|cshot:cl3_tab$i,810,312,310,420"; done
S="$OPEN|$(H cl3_tl1 847)|$(H cl3_tl2 900)|aclick:831,552|wait:400|$(H cl3_tg1 847)|$(H cl3_tg2 900)|aclick:843,427|wait:500$T|key:{F1}|wait:3500|fshot:cl3_f1"
powershell -File tools/cl_run.ps1 -Tail "$S" -Max 500
