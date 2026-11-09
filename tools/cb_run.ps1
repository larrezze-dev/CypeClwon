param([Parameter(Mandatory)][string]$Tail, [int]$Max = 400, [switch]$NoOpen)
# Measurement helper (T-P7-03): original -> new obra -> import m6_gen.txt -> Carga tab; unless -NoOpen: Barras > Nueva, select bar 1 (X) with two LMB + RMB (dialog open), then $Tail.
Set-Location D:\Coding\CypeClwon
$open = "click:421,80|wait:1500|aclick:1328,677|wait:800|aclick:1328,677|wait:800|arclick:1328,677|wait:2500"
if ($NoOpen) { $open = "" }
$tail = if ($open) { $open + "|" + $Tail } else { $Tail }
& D:\Coding\CypeClwon\tools\cn_run.ps1 -Tail $tail -Max $Max 6>$null
