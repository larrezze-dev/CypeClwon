param(
    [Parameter(Mandatory)][string]$Steps,
    [string]$OutDir = "D:\Coding\CypeClwon\docs\screenshots",
    [string]$Exe = "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe",
    [int]$StartupMs = 6000,
    [int]$MaxSeconds = 120
)
# Drives the ORIGINAL application for research captures (only its window is captured, via PrintWindow).
# Steps are separated by '|':  click:x,y  (window-relative, left click)   dclick:x,y   key:^{TAB} (SendKeys syntax)
#                              wait:ms   wheel:n (notches, negative = down, at the cursor)   shot:name  (saves <OutDir>\<name>.png of the foreground window of the process)
# Run only after the user has freed the workspace and given an explicit OK.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class D {
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr h, uint cmd);
 // Topmost visible window of the process (dialogs are owned windows and sit above the main one in Z order).
 public static IntPtr Top(uint pid) { IntPtr found = IntPtr.Zero;
   EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
   return found; }
 public struct R { public int L,T,Rt,B; } }
"@
function Shot($p, $name) {
    $h = [D]::Top([uint32]$p.Id)
    if ($h -eq [IntPtr]::Zero) { throw "no visible window for $name" }
    $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.Rt - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [void][D]::PrintWindow($h, $dc, 2); $g.ReleaseHdc($dc); $g.Dispose()
    $bmp.Save((Join-Path $OutDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Log "saved $name ($($bmp.Width)x$($bmp.Height))"
}
function SShot($p, $name) {
    # Real screen capture of the window rectangle (PrintWindow does not capture the OpenGL area reliably).
    $h = [D]::Top([uint32]$p.Id); [void][D]::SetForegroundWindow($h); Start-Sleep -Milliseconds 300
    $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r)
    $bmp = New-Object System.Drawing.Bitmap ($r.Rt - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size); $g.Dispose()
    $bmp.Save((Join-Path $OutDir "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Log "saved screen $name"
}
function Click($p, $x, $y, $double) {
    $h = [D]::Top([uint32]$p.Id)
    $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r)
    [void][D]::SetForegroundWindow($h)
    [void][D]::SetCursorPos($r.L + $x, $r.T + $y)
    Start-Sleep -Milliseconds 120
    [D]::mouse_event(1, 1, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [D]::mouse_event(1, [uint32]4294967295, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
    $n = if ($double) { 2 } else { 1 }
    for ($i = 0; $i -lt $n; $i++) { [D]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); [D]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80 }
}
$log = "D:\Coding\CypeClwon\build\drive_original.log"
function Log($m) { $line = "{0:HH:mm:ss} {1}" -f (Get-Date), $m; Add-Content -Path $log -Value $line; Write-Host $line }
Set-Content -Path $log -Value ""
[void][D]::SetProcessDPIAware()
$deadline = (Get-Date).AddSeconds($MaxSeconds)
$Exe = (Resolve-Path -LiteralPath $Exe -ErrorAction SilentlyContinue).Path; if (-not $Exe) { $Exe = (Get-ChildItem "D:\CYPE Ingenieros\Versi*n 2027\CYPE 3D\CYPE 3D.exe").FullName }
Log "launching $Exe"
$p = Start-Process -FilePath $Exe -PassThru
try {
    Start-Sleep -Milliseconds $StartupMs
    if ($p.HasExited) { throw "the application exited during startup (code $($p.ExitCode))" }
    foreach ($step in $Steps.Split('|')) {
        if ((Get-Date) -gt $deadline) { throw "time limit of $MaxSeconds s exceeded before step '$step'" }
        if ($p.HasExited) { throw "the application exited before step '$step'" }
        Log "step $step"
        $kind, $arg = $step.Split(':', 2)
        switch ($kind) {
            'wait'   { Start-Sleep -Milliseconds ([int]$arg) }
            'click'  { $xy = $arg.Split(','); Click $p ([int]$xy[0]) ([int]$xy[1]) $false; Start-Sleep -Milliseconds 700 }
            'dclick' { $xy = $arg.Split(','); Click $p ([int]$xy[0]) ([int]$xy[1]) $true; Start-Sleep -Milliseconds 700 }
            'move'   { $xy = $arg.Split(','); $h = [D]::Top([uint32]$p.Id); $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r); [void][D]::SetCursorPos($r.L + [int]$xy[0], $r.T + [int]$xy[1]); [D]::mouse_event(1, 1, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [D]::mouse_event(1, [uint32]4294967295, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120; Start-Sleep -Milliseconds 900 }
            'wheel'  { [void][D]::SetForegroundWindow([D]::Top([uint32]$p.Id)); $d = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]([int]$arg * 120)), 0); [D]::mouse_event(0x0800, 0, 0, $d, [UIntPtr]::Zero); Start-Sleep -Milliseconds 700 }
            'key'    { [void][D]::SetForegroundWindow([D]::Top([uint32]$p.Id)); [System.Windows.Forms.SendKeys]::SendWait($arg); Start-Sleep -Milliseconds 600 }
            'drag'   { $a = $arg.Split(','); $f = @{L=@(2,4); R=@(8,16); M=@(32,64)}[$a[0]]
                       $h = [D]::Top([uint32]$p.Id); $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r); [void][D]::SetForegroundWindow($h)
                       $x1 = [int]$a[1]; $y1 = [int]$a[2]; $x2 = [int]$a[3]; $y2 = [int]$a[4]
                       [void][D]::SetCursorPos($r.L + $x1, $r.T + $y1); Start-Sleep -Milliseconds 200
                       [D]::mouse_event(1, 1, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [D]::mouse_event(1, [uint32]4294967295, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
                       [D]::mouse_event($f[0], 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 150
                       for ($i = 1; $i -le 25; $i++) { [void][D]::SetCursorPos($r.L + $x1 + [int](($x2 - $x1) * $i / 25), $r.T + $y1 + [int](($y2 - $y1) * $i / 25)); Start-Sleep -Milliseconds 60 }
                       Start-Sleep -Milliseconds 300; if ($a.Count -gt 5) { SShot $p $a[5] }; [D]::mouse_event($f[1], 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 700 }
            'mclick' { $a = $arg.Split(','); $vk = @{ctrl=0x11; shift=0x10}[$a[0]]; $h = [D]::Top([uint32]$p.Id); $r = New-Object D+R; [void][D]::GetWindowRect($h, [ref]$r); [void][D]::SetForegroundWindow($h)
                       [void][D]::SetCursorPos($r.L + [int]$a[1], $r.T + [int]$a[2]); Start-Sleep -Milliseconds 150; [D]::mouse_event(1, 1, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [D]::mouse_event(1, [uint32]4294967295, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 120
                       [D]::keybd_event([byte]$vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 100
                       [D]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); [D]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 100
                       [D]::keybd_event([byte]$vk, 0, 2, [UIntPtr]::Zero); Start-Sleep -Milliseconds 700 }
            'sshot'  { SShot $p $arg }
            'shot'   { Shot $p $arg }
        }
    }
    Log "DONE"
} catch {
    Log "FAILED: $_"
    exit 1
} finally {
    if (-not $p.HasExited) { Stop-Process $p -Force }
}
