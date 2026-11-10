param([int]$X,[int]$Y,[string]$Kind="dbl")
# Raw absolute mouse helper: dbl = double left click, ldown/lup, rdbl.
Add-Type -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y); [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,UIntPtr e);' -Name W -Namespace U2
[void][U2.W]::SetCursorPos($X,$Y); Start-Sleep -Milliseconds 150
[U2.W]::mouse_event(1,1,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [U2.W]::mouse_event(1,[uint32]4294967295,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 250
if ($Kind -eq "dbl") { for($i=0;$i -lt 2;$i++){ [U2.W]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [U2.W]::mouse_event(4,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 80 } }
if ($Kind -eq "left") { [U2.W]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [U2.W]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
if ($Kind -eq "right") { [U2.W]::mouse_event(8,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [U2.W]::mouse_event(16,0,0,0,[UIntPtr]::Zero) }
Start-Sleep -Milliseconds 1200
