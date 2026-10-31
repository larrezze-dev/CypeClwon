# Regenerates .tasks.html from .tasks.md (run from the repository root).
$root = Split-Path -Parent $PSScriptRoot
$rows = Get-Content "$root\.tasks.md" -Encoding UTF8 | Where-Object { $_ -match '^\| T-' } | ForEach-Object {
    $c = $_.Trim('|').Split('|') | ForEach-Object { $_.Trim() }
    [pscustomobject]@{ Id = $c[0]; Phase = [int]$c[1]; Task = $c[2]; State = $c[3]; Note = $c[4] }
}
$total = $rows.Count
$done = ($rows | Where-Object State -eq '[x]').Count
$wip = ($rows | Where-Object State -eq '[o]').Count
$todo = $total - $done - $wip
$pct = if ($total) { [math]::Round(100 * $done / $total, 1) } else { 0 }
$phaseNames = @{0='Investigación y plan';1='Esqueleto GUI';2='Inicio y asistentes';3='Modelo y ficheros';4='Viewport 3D';5='Geometría';6='Propiedades';7='Cargas';8='Cálculo lineal';9='Normas';10='Pandeo/modal/sismo';11='No lineal/pushover';12='Fuego';13='Armaduras';14='Uniones';15='Cimentación';16='Listados/planos';17='BIM/Ventana';18='QA y deploy';19='Idiomas';20='Marketing'}
$bars = ($rows | Group-Object Phase | Sort-Object { [int]$_.Name } | ForEach-Object {
    $n = $_.Count; $d = ($_.Group | Where-Object State -eq '[x]').Count; $w = ($_.Group | Where-Object State -eq '[o]').Count
    $p = [math]::Round(100 * $d / $n); $pw = [math]::Round(100 * $w / $n)
    "<tr><td>F$($_.Name) · $($phaseNames[[int]$_.Name])</td><td><div class='bar'><i class='d' style='width:$p%'></i><i class='w' style='width:$pw%'></i></div></td><td>$d/$n</td></tr>"
}) -join "`n"
$list = ($rows | ForEach-Object {
    $cls = switch ($_.State) { '[x]' {'ok'} '[o]' {'wip'} default {'todo'} }
    "<tr class='$cls'><td>$($_.Id)</td><td>F$($_.Phase)</td><td>$([System.Net.WebUtility]::HtmlEncode($_.Task))</td><td>$($_.State)</td><td>$([System.Net.WebUtility]::HtmlEncode($_.Note))</td></tr>"
}) -join "`n"
$stamp = Get-Date -Format 'yyyy-MM-dd HH:mm'
$html = @"
<!doctype html><html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dashboard CypeClwon</title>
<style>
:root{--bg:#fff;--fg:#1d1d1f;--mut:#6b6b70;--line:#e2e2e6;--ok:#2e7d32;--wip:#f9a825;--todo:#c9c9ce;--acc:#b3261e}
@media(prefers-color-scheme:dark){:root{--bg:#161618;--fg:#ececf0;--mut:#9a9aa2;--line:#2c2c30;--todo:#3a3a40}}
body{margin:0;padding:24px 16px;background:var(--bg);color:var(--fg);font:14px/1.4 system-ui,sans-serif;max-width:1100px;margin-inline:auto}
h1{margin:0 0 4px;color:var(--acc)} .mut{color:var(--mut)}
.kpi{display:flex;gap:12px;flex-wrap:wrap;margin:16px 0}.kpi div{border:1px solid var(--line);border-radius:8px;padding:10px 16px;min-width:110px}
.kpi b{display:block;font-size:24px}
table{border-collapse:collapse;width:100%;margin:12px 0}td,th{border-bottom:1px solid var(--line);padding:5px 8px;text-align:left;vertical-align:top}
.bar{background:var(--todo);height:12px;border-radius:6px;overflow:hidden;display:flex;min-width:200px}.bar i{display:block;height:100%}.d{background:var(--ok)}.w{background:var(--wip)}
tr.ok td:nth-child(4){color:var(--ok)}tr.wip td:nth-child(4){color:var(--wip)}
details{margin-top:16px}summary{cursor:pointer;font-weight:600}
.tw{overflow-x:auto}
</style></head><body>
<h1>CypeClwon – Clon de CYPE 3D</h1><div class="mut">Actualizado $stamp · generado desde .tasks.md</div>
<div class="kpi"><div><b>$pct%</b>completado</div><div><b>$done</b>finalizadas</div><div><b>$wip</b>en marcha</div><div><b>$todo</b>pendientes</div><div><b>$total</b>tareas</div></div>
<h2>Avance por fase</h2><table>$bars</table>
<details open><summary>Todas las tareas</summary><div class="tw"><table><tr><th>ID</th><th>Fase</th><th>Tarea</th><th>Estado</th><th>Comentarios</th></tr>
$list</table></div></details></body></html>
"@
Set-Content "$root\.tasks.html" $html -Encoding UTF8
"Dashboard: $done/$total ($pct%)"
