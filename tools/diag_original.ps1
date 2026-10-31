$ErrorActionPreference = "Stop"
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class Dg {
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out R r);
 public struct R { public int L,T,Rt,B; }
 public static List<string> All() { var l = new List<string>();
  EnumWindows((h, x) => { if (!IsWindowVisible(h)) return true; uint p; GetWindowThreadProcessId(h, out p);
   var sb = new StringBuilder(256); GetWindowText(h, sb, 256); R r; GetWindowRect(h, out r);
   if (sb.Length > 0) l.Add(p + "|" + sb + "|" + (r.Rt-r.L) + "x" + (r.B-r.T)); return true; }, IntPtr.Zero);
  return l; } }
"@
$exe = (Get-ChildItem "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe").FullName
$log = "D:\Coding\CypeClwon\build\diag_original.log"
Set-Content $log ""
$p = Start-Process -FilePath $exe -PassThru
Add-Content $log "started pid $($p.Id)"
try {
    foreach ($t in 3, 6, 10, 15) {
        Start-Sleep -Seconds ($t - $(if ($t -eq 3) { 0 } else { @{6=3;10=6;15=10}[$t] }))
        Add-Content $log "--- t=$t s; main process alive=$(-not $p.HasExited)"
        Get-Process | Where-Object { $_.ProcessName -like "CYPE*" -or $_.ProcessName -like "*launcher*" } | ForEach-Object { Add-Content $log "proc $($_.Id) $($_.ProcessName) '$($_.MainWindowTitle)'" }
        [Dg]::All() | Where-Object { $_ -match "CYPE|Licen|Iniciar|Aviso" } | ForEach-Object { Add-Content $log "win $_" }
    }
} finally {
    Get-Process | Where-Object { $_.ProcessName -like "CYPE*" } | ForEach-Object { Stop-Process $_ -Force }
}
Add-Content $log "DONE"
