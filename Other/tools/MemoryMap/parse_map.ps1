# Parse ARM Compiler 5 (Keil MDK) .map into a memory budget report (regions + per-object top N).
# AC5 map layout relied upon:
#   "    Execution Region <NAME> (Exec base: 0x..., Load base: 0x..., Size: 0x..., Max: 0x..., ...)"
#   "      0xEXEC    0xLOAD|-    0xSIZE    Data|Zero|Code|Common  ...  .section  object.o"
# NOTE: the memory-analysis skill's own parser does NOT handle AC5 maps (verified: garbage output),
#       hence this script. Usage examples at the bottom of Docs/01-map/CAPACITY-BASELINE.md.
param(
    [string]$Map = "MDK-ARM\SkyStar_BSP_HAL\SkyStar_BSP_HAL.map",
    [int]$Top = 14,
    [string]$OutJson = "",
    [string]$Regions = "ER_IROM1,RW_IRAM1,RW_IRAM2,RW_IRAM_CCM"
)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $Map)) { Write-Error "map not found: $Map"; exit 1 }

$regionRe = 'Execution Region (\S+) \(Exec base:\s*(0x[0-9a-fA-F]+), Load base:.*?Size:\s*(0x[0-9a-fA-F]+), Max:\s*(0x[0-9a-fA-F]+)'
$sectRe   = '^\s+0x[0-9a-fA-F]{8}\s+(?:-|0x[0-9a-fA-F]{8})\s+(0x[0-9a-fA-F]{6,8})\s+(Data|Zero|Code|Common)\b'

$meta = @{}
$rows = New-Object System.Collections.ArrayList
$cur  = $null
foreach ($l in (Get-Content $Map)) {
    if ($l -match $regionRe) {
        $cur = $matches[1]
        $meta[$cur] = [pscustomobject]@{ Base = $matches[2]; Used = [Convert]::ToInt64($matches[3], 16); Max = [Convert]::ToInt64($matches[4], 16) }
        if (-not $rows.Contains) { }   # no-op, keeps PS happy
        continue
    }
    if ($cur -and $l -match $sectRe) {
        [void]$rows.Add([pscustomobject]@{ Region = $cur; Object = (($l -split '\s+')[-1]); Size = [Convert]::ToInt64($matches[1], 16) })
    }
}

$report = [ordered]@{ map = (Resolve-Path $Map).Path; generated = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"); regions = [ordered]@{} }
$ramUsed = 0

foreach ($r in ($Regions -split ',')) {
    $r = $r.Trim()
    if (-not $meta.ContainsKey($r)) { continue }
    $m = $meta[$r]
    $free = $m.Max - $m.Used
    if ($r -ne "ER_IROM1") { $ramUsed += $m.Used }
    ""
    "=== {0}  base={1}  used={2}  max={3}  FREE={4}  ({5}% used)" -f $r, $m.Base, $m.Used, $m.Max, $free, [math]::Round(100.0 * $m.Used / $m.Max, 1)

    $grouped = $rows | Where-Object { $_.Region -eq $r } | Group-Object -Property Object |
        Sort-Object -Property { ($_.Group | Measure-Object -Property Size -Sum).Sum } -Descending |
        Select-Object -First $Top
    foreach ($g in $grouped) {
        "  {0,9} B  {1}" -f ($g.Group | Measure-Object -Property Size -Sum).Sum, $g.Name
    }
    $report.regions[$r] = [ordered]@{
        base = $m.Base; used = $m.Used; max = $m.Max; free = $free
        top  = @($grouped | ForEach-Object { @{ bytes = [int64](($_.Group | Measure-Object -Property Size -Sum).Sum); object = $_.Name } })
    }
}

""
"TOTAL RAM used (RW_IRAM1+RW_IRAM2+RW_IRAM_CCM) = $ramUsed B"

if ($OutJson -ne "") {
    $report | ConvertTo-Json -Depth 6 | Set-Content -Path $OutJson -Encoding UTF8
    "snapshot written: $OutJson"
}
