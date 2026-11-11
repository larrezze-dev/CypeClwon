param([Parameter(Mandatory)][string]$File)
# Restarts the ORIGINAL (kills any running instance) opening a sample obra of docs/sample_project; waits for the window.
Get-Process | Where-Object { $_.ProcessName -like "CYPE 3D*" } | Stop-Process -Force
Start-Sleep 3
$e = (Get-ChildItem "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe").FullName
Start-Process -FilePath $e -ArgumentList ("D:\Coding\CypeClwon\docs\sample_project\" + $File) | Out-Null
Start-Sleep 24
