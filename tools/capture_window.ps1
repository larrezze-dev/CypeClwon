param([Parameter(Mandatory)][string]$Exe, [Parameter(Mandatory)][string]$Out, [string]$ExtraArgs = "", [int]$WaitMs = 2500)
# Launches the app, captures ONLY its window via PrintWindow (never the full screen), then closes it.
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
 public struct R { public int L,T,Rt,B; } }
"@
$qt = Join-Path (Split-Path -Parent $PSScriptRoot) ".toolchain\Qt\6.8.3\msvc2022_64\bin"
$env:PATH = "$qt;$env:PATH"
$p = if ($ExtraArgs) { Start-Process $Exe -ArgumentList $ExtraArgs -PassThru } else { Start-Process $Exe -PassThru }
Start-Sleep -Milliseconds $WaitMs
$p.Refresh(); $h = $p.MainWindowHandle
$r = New-Object W+R; [void][W]::GetWindowRect($h, [ref]$r)
$bmp = New-Object System.Drawing.Bitmap ($r.Rt-$r.L), ($r.B-$r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[void][W]::PrintWindow($h, $dc, 2); $g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
Stop-Process $p -Force
