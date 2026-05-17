# dp_compare_runs.ps1 -- quantitative side-by-side comparison of N
# telemetry recordings.
#
# Loads each JSON export, computes per-run path lengths, frame-flag
# counts, and event histograms, then prints an N-column table.
#
# Usage:
#   # default: line up the 4 personality recordings
#   pwsh ./Tools/dp_compare_runs.ps1
#   # custom set:
#   pwsh ./Tools/dp_compare_runs.ps1 -Paths a.json,b.json -Labels A,B

[CmdletBinding()]
param(
    [string[]]$Paths = @(
        "build/dp_telemetry/personality_Human.json",
        "build/dp_telemetry/personality_Stealth.json",
        "build/dp_telemetry/personality_Speedrun.json",
        "build/dp_telemetry/personality_Reckless.json"
    ),
    [string[]]$Labels = @("Human", "Stealth", "Speedrun", "Reckless")
)

$ErrorActionPreference = 'Stop'

# StateFlags constants (must mirror DPTelemetry::StateFlags).
$FLAG_POSSESSED = 1
$FLAG_SPRINT    = 2
$FLAG_QUIET     = 4
$FLAG_ALIVE     = 8
$FLAG_HOLDING   = 16
$FLAG_PRIEST    = 128

function Summarize($path) {
    $tel = Get-Content $path -Raw | ConvertFrom-Json
    $frames = @($tel.frames)
    $events = @($tel.events)
    $h = $tel.header

    $byEnt = @{}
    foreach ($f in $frames) {
        foreach ($e in $f.entities) {
            $k = "$($e.id.idx):$($e.id.gen)"
            if (-not $byEnt.ContainsKey($k)) {
                $byEnt[$k] = @{
                    samples   = New-Object 'System.Collections.Generic.List[object]'
                    flagsSeen = 0
                }
            }
            $byEnt[$k].samples.Add(@{ t = $f.t; pos = $e.pos; flags = [int]$e.flags })
            $byEnt[$k].flagsSeen = $byEnt[$k].flagsSeen -bor [int]$e.flags
        }
    }

    $totalDist     = 0.0
    $possessedDist = 0.0
    $priestDist    = 0.0
    $sprintSamples = 0
    $quietSamples  = 0
    $possSamples   = 0
    $holdSamples   = 0
    $deadSamples   = 0
    $priestCount   = 0
    $villagerCount = 0

    foreach ($k in $byEnt.Keys) {
        $entry = $byEnt[$k]
        $samples = $entry.samples
        $isPriest = ($entry.flagsSeen -band $FLAG_PRIEST) -ne 0
        if ($isPriest) { $priestCount++ } else { $villagerCount++ }

        $d = 0.0
        $possD = 0.0
        for ($i = 1; $i -lt $samples.Count; $i++) {
            # pos is serialised as [x, y, z] in the JSON exporter.
            $a = $samples[$i - 1].pos
            $b = $samples[$i].pos
            $dx = [double]$b[0] - [double]$a[0]
            $dz = [double]$b[2] - [double]$a[2]
            $step = [math]::Sqrt($dx * $dx + $dz * $dz)
            $d += $step
            if (($samples[$i].flags -band $FLAG_POSSESSED) -ne 0) {
                $possD += $step
            }
        }
        $totalDist += $d
        if ($isPriest) { $priestDist += $d } else { $possessedDist += $possD }

        foreach ($s in $samples) {
            $f = $s.flags
            if (($f -band $FLAG_POSSESSED) -ne 0) { $possSamples++ }
            if (($f -band $FLAG_SPRINT)    -ne 0) { $sprintSamples++ }
            if (($f -band $FLAG_QUIET)     -ne 0) { $quietSamples++ }
            if (($f -band $FLAG_HOLDING)   -ne 0) { $holdSamples++ }
            if (($f -band $FLAG_ALIVE)     -eq 0 -and -not $isPriest) { $deadSamples++ }
        }
    }

    $byName = @{}
    foreach ($e in $events) {
        if (-not $byName.ContainsKey($e.name)) { $byName[$e.name] = 0 }
        $byName[$e.name]++
    }

    $duration = if ($frames.Count -gt 0) { [double]$frames[-1].t } else { 0.0 }

    return @{
        Header      = $h
        Frames      = $frames.Count
        Events      = $events.Count
        Entities    = $byEnt.Count
        Villagers   = $villagerCount
        Priests     = $priestCount
        DurationS   = $duration
        TotalDistM  = [int]$totalDist
        PossDistM   = [int]$possessedDist
        PriestDistM = [int]$priestDist
        PossSamples = $possSamples
        SprintSamp  = $sprintSamples
        QuietSamp   = $quietSamples
        HoldSamp    = $holdSamples
        DeadSamp    = $deadSamples
        EventNames  = $byName
    }
}

if ($Paths.Count -ne $Labels.Count) {
    Write-Error "Paths.Count ($($Paths.Count)) != Labels.Count ($($Labels.Count))"
    exit 1
}

# Summarise all runs.
$summaries = @()
for ($i = 0; $i -lt $Paths.Count; $i++) {
    if (-not (Test-Path $Paths[$i])) {
        Write-Error "Missing $($Paths[$i])"
        exit 1
    }
    $summaries += Summarize $Paths[$i]
}

# Column-width tunings.
$labelCol = 26
$valCol   = 12
$fmt = "{0,-${labelCol}}"
for ($i = 0; $i -lt $Labels.Count; $i++) { $fmt += " {$($i+1),${valCol}}" }

# Header row.
$headerVals = @("") + $Labels
Write-Host ($fmt -f $headerVals) -ForegroundColor Cyan
$sepCols = @("-" * $labelCol)
for ($i = 0; $i -lt $Labels.Count; $i++) { $sepCols += "-" * $valCol }
Write-Host ($fmt -f $sepCols)

function Row($label, [object[]]$values) {
    $arr = @($label) + $values
    Write-Host ($fmt -f $arr)
}

# Helpers to project a single field across summaries.
function ProjectField($key) {
    $out = @()
    foreach ($s in $summaries) { $out += $s[$key] }
    return $out
}
function ProjectHeaderField($key) {
    $out = @()
    foreach ($s in $summaries) { $out += $s.Header.$key }
    return $out
}

Row "seed"               (ProjectHeaderField 'seed')
Row "sceneName"          (ProjectHeaderField 'sceneName')
Row "fixedDt"            (ProjectHeaderField 'fixedDt')
Row "samplePeriodFrames" (ProjectHeaderField 'samplePeriodFrames')
Row "frames recorded"    (ProjectField 'Frames')
$durations = @()
foreach ($s in $summaries) { $durations += [math]::Round($s.DurationS, 1) }
Row "duration (s)"       $durations
Row "entities tracked"   (ProjectField 'Entities')
Row "  villagers"        (ProjectField 'Villagers')
Row "  priests"          (ProjectField 'Priests')
Row "events total"       (ProjectField 'Events')
Row "total path (m)"     (ProjectField 'TotalDistM')
Row "possessed path (m)" (ProjectField 'PossDistM')
Row "priest path (m)"    (ProjectField 'PriestDistM')
Row "possessed samples"  (ProjectField 'PossSamples')
Row "sprint samples"     (ProjectField 'SprintSamp')
Row "walk-quiet samples" (ProjectField 'QuietSamp')
Row "holding-item smps"  (ProjectField 'HoldSamp')
Row "dead samples"       (ProjectField 'DeadSamp')

Write-Host ""
$evHeaderVals = @("EVENT") + $Labels
Write-Host ($fmt -f $evHeaderVals) -ForegroundColor Cyan
Write-Host ($fmt -f $sepCols)

# Union of event names across all summaries.
$allNames = @()
foreach ($s in $summaries) { $allNames += @($s.EventNames.Keys) }
$allNames = $allNames | Select-Object -Unique | Sort-Object

foreach ($n in $allNames) {
    $vals = @()
    foreach ($s in $summaries) {
        $v = if ($s.EventNames.ContainsKey($n)) { $s.EventNames[$n] } else { 0 }
        $vals += $v
    }
    Row $n $vals
}
Write-Host ""
