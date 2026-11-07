param([Parameter(Mandatory)][string]$Tail, [int]$Max = 260, [switch]$Plain)
# Measurement helper (T-P6-07 closing session): original -> new obra -> import m6_lam.txt -> Propiedades,
# then (unless -Plain) Bandas de integracion -> select the plate by its contour -> RMB (dialog open), then the steps in $Tail.
Set-Location D:\Coding\CypeClwon
$base = "click:630,470|wait:3500|click:48,260|wait:3000|click:48,432|wait:4000|click:400,15|wait:500|click:795,749|wait:3500|click:180,15|wait:800|click:47,412|wait:5000|click:136,36|wait:800|click:1425,98|wait:2500|click:300,15|wait:500|click:365,53|key:^a|key:D:\Coding\CypeClwon\docs\sample_project\m6_lam.txt|wait:500|click:47,943|wait:3000|click:240,15|wait:500|click:49,132|wait:3500|click:220,36|wait:1500"
$sel = "|click:870,95|wait:1000|click:1653,516|wait:800|click:1653,516|wait:800|arclick:1653,516|wait:3000"
if ($Plain) { $sel = "" }
& D:\Coding\CypeClwon\tools\drive_original.ps1 -StartupMs 17000 -MaxSeconds $Max -Steps ($base + $sel + "|" + $Tail) 6>$null
