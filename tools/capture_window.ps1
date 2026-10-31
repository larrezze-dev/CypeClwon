param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Out, [string]$ExtraArgs = "", [int]$WaitMs = 2500)
# Launches the app, captures ONLY its window via PrintWindow (never the full screen), then closes it.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
 // First visible top-level window owned by the process (works for dialogs that are not the "main" window).
 public static IntPtr FindFor(uint pid) { IntPtr found = IntPtr.Zero;
   EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
   return found; }
 public struct R { public int L,T,Rt,B; } }
"@
$qt = Join-Path (Split-Path -Parent $PSScriptRoot) ".toolchain\Qt\6.8.3\msvc2022_64\bin"
$env:PATH = "$qt;$env:PATH"
$p = if ($ExtraArgs) { Start-Process $Exe -ArgumentList $ExtraArgs -PassThru } else { Start-Process $Exe -PassThru }
Start-Sleep -Milliseconds $WaitMs
$p.Refresh(); $h = [W]::FindFor([uint32]$p.Id)
if ($h -eq [IntPtr]::Zero) { Stop-Process $p -Force; throw 'No visible window found' }
$r = New-Object W+R; [void][W]::GetWindowRect($h, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.Rt-$r.L), ($r.B-$r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[void][W]::PrintWindow($h, $dc, 2); $g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Stop-Process $p -Force
