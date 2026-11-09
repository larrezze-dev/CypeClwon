# Step builders for tanda 3 (T-P7-04). source tools/cl_lib.sh first.
# mk NAME TYPEBTNX FIELDSTEPS : in the open Nueva dialog choose type, fill fields, Aceptar, capture 3D area
mk() { echo "aclick:$2,427|wait:600|move:700,300|wait:300|$3|$ACC|wait:1200|cshot:$1,1100,340,820,520"; }
# del : Borrar mode, select lamina load, RMB, then back to Nueva and reopen dialog
DEL="aclick:697,97|wait:1200|aclick:1490,600|wait:800|aclick:1490,600|wait:800|arclick:1490,600|wait:1500|aclick:633,80|wait:1200"
# zm NAME X : zoom in at the lamina corner (X,597), capture 300x350 around it, zoom back
zm() { echo "amove:$2,597|wheel:6|wait:800|cshot:$1,$(( $2 - 150 )),430,300,350|wheel:-6|wait:500"; }
