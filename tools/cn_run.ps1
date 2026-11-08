param([Parameter(Mandatory)][string]$Tail, [int]$Max = 400, [string]$File = "m6_gen.txt")
# Measurement helper (T-P7-02 closing session): original -> new obra -> import docs/sample_project/<File> -> ribbon tab Carga, then the steps in $Tail.
# Same recipe as tools/m7_run.ps1; the main window is maximised at (0,0) so screen and window coordinates coincide (aclick / arclick / fshot).
Set-Location D:\Coding\CypeClwon
$base = "click:630,470|wait:3500|click:48,260|wait:3000|click:48,432|wait:4000|click:400,15|wait:500|click:795,749|wait:6500|click:180,15|wait:1200|click:47,412|wait:6000|fshot:oc_a|click:136,36|wait:1000|click:1425,98|wait:3000|click:300,15|wait:500|click:365,53|key:^a|key:D:\Coding\CypeClwon\docs\sample_project\$File|wait:500|click:47,943|wait:3000|click:240,15|wait:500|click:49,132|wait:3500|click:292,36|wait:1500"
& D:\Coding\CypeClwon\tools\drive_original.ps1 -StartupMs 17000 -MaxSeconds $Max -Steps ($base + "|" + $Tail) 6>$null
