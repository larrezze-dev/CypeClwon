param([Parameter(Mandatory)][string]$Tail, [int]$Max = 400, [string]$File = "m6_lam.txt")
# Measurement helper (T-P7-04): original -> new obra -> import m6_lam.txt (4 nodes, 4 bars, 1 lamina) -> Carga tab, then $Tail.
Set-Location D:\Coding\CypeClwon
& D:\Coding\CypeClwon\tools\cn_run.ps1 -Tail $Tail -Max $Max -File $File 6>$null
