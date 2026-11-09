# Step builders for the lamina-load measurements (T-P7-04). Source it, then compose steps.
OPEN="aclick:633,80|wait:1500|aclick:1490,600|wait:800|aclick:1490,600|wait:800|arclick:1490,600|wait:2500"
typ() { echo "aclick:$1,$2|wait:300|key:{TAB}|key:+{TAB}|key:^a|key:^a|key:0|key:^a|key:$3"; }
ACC="aclick:858,707|wait:1200"
# variable types: vtyp INITIAL FINAL (final first, then back with +TAB: the first field does not take the usual typing recipe)
vtyp() { echo "$(typ 929 496 $2)|key:+{TAB}|key:^a|key:$1|key:{TAB}|key:{TAB}"; }
REOPEN="aclick:1490,600|wait:800|aclick:1490,600|wait:800|arclick:1490,600|wait:2500"
# case NAME TYPEBUTTONX FIELDSTEPS : choose type, fill fields, Aceptar, capture the box, dismiss it, Cancel, reopen
case_() { echo "aclick:$2,427|wait:600|move:700,300|wait:300|$3|$ACC|cshot:$1,760,420,480,200|key:{ENTER}|wait:500|aclick:1074,707|wait:800|$REOPEN"; }
# keyboard recipe measured: after clicking a type button the first TAB reaches the first field, the second TAB the second field (then the axes radios)
f1() { echo "key:{TAB}|key:^a|key:$1"; }
f2() { echo "key:{TAB}|key:^a|key:$1|key:{TAB}|key:^a|key:$2"; }
EDITOPEN="aclick:697,66|wait:1200|aclick:1490,600|wait:800|aclick:1490,600|wait:800|arclick:1490,600|wait:2500"
# edit NAME TYPEBUTTONX FIELDSTEPS : Editar the lamina load, set type/fields, accept, leave the mode and capture the 3D area
edit_() { echo "$EDITOPEN|aclick:$2,427|wait:500|move:700,300|wait:300|$3|$ACC|wait:600|aclick:633,80|wait:1200|cshot:$1,1100,340,820,520"; }
