# dp_telemetry_visualise.ps1 -- render a top-down PNG of a telemetry
# recording. Reads the JSON exporter output (header + frames + events)
# and draws each entity's path through the scene + event markers.
#
# Purpose: visual review of bot playthroughs. Catches "bot stood in a
# corner" or "priest never moved" failure modes that pass the existing
# analyzer criteria but aren't really playing the game.
#
# Usage:
#   pwsh ./Tools/dp_telemetry_visualise.ps1
#   pwsh ./Tools/dp_telemetry_visualise.ps1 -JsonPath build/dp_telemetry/bot_playthrough.json
#   pwsh ./Tools/dp_telemetry_visualise.ps1 -OutPath my.png -Width 2048 -Height 2048
#
# Output: PNG at <OutPath> (default: <JsonDir>/<JsonName>.png).
#
# Implementation: System.Drawing.Bitmap + Graphics (built into .NET on
# Windows; no external installs needed). World-to-image projection uses
# the bounding box of every sample's entity positions, padded 10%, with
# an aspect-correct fit-to-screen mapping.
#
# Event markers: filled circles at the position of the entityA (if
# present in the most recent FrameSample) at the event's frame. Each
# marker labelled with the event's `name` field.
#
# ASCII-only script body so Windows PowerShell 5.1 + pwsh 7+ parse it
# without UTF-8 mojibake. See run_dp_tests.ps1 preamble for the rationale.

[CmdletBinding()]
param(
    [string]$JsonPath = "build/dp_telemetry/bot_playthrough.json",
    [string]$OutPath  = "",
    [int]$Width       = 1024,
    [int]$Height      = 1024,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

# ----------------------------------------------------------------------
# 0. Pre-flight.
# ----------------------------------------------------------------------
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path $JsonPath)) {
    Write-Error "Telemetry JSON not found: $JsonPath"
    exit 1
}

if ($OutPath -eq "") {
    $dir   = Split-Path -Parent $JsonPath
    $stem  = [IO.Path]::GetFileNameWithoutExtension($JsonPath)
    $OutPath = Join-Path $dir "$stem.png"
}

Add-Type -AssemblyName System.Drawing

# ----------------------------------------------------------------------
# 1. Load telemetry.
# ----------------------------------------------------------------------
if (-not $Quiet) { Write-Host "Loading $JsonPath..." -ForegroundColor Cyan }
$telemetry = Get-Content $JsonPath -Raw | ConvertFrom-Json

$frames = if ($telemetry.frames) { @($telemetry.frames) } else { @() }
$events = if ($telemetry.events) { @($telemetry.events) } else { @() }

if ($frames.Count -eq 0) {
    Write-Error "Telemetry has no frame samples; nothing to render."
    exit 1
}

# ----------------------------------------------------------------------
# 2. Bucket per-entity paths. Key = packed entityID string "idx:gen".
# ----------------------------------------------------------------------
$paths = @{}  # key -> [System.Collections.ArrayList] of @(x, z)

foreach ($frame in $frames) {
    foreach ($e in $frame.entities) {
        $key = "$($e.id.idx):$($e.id.gen)"
        if (-not $paths.ContainsKey($key)) {
            $paths[$key] = New-Object System.Collections.ArrayList
        }
        # pos is [x, y, z]; we project on the X-Z plane (top-down).
        [void]$paths[$key].Add(@([double]$e.pos[0], [double]$e.pos[2]))
    }
}

if (-not $Quiet) {
    Write-Host ("Loaded {0} frames, {1} events, {2} distinct entities" `
        -f $frames.Count, $events.Count, $paths.Count) -ForegroundColor Cyan
}

# ----------------------------------------------------------------------
# 3. Compute world bounds (with 10% padding).
# ----------------------------------------------------------------------
$minX = [double]::PositiveInfinity
$maxX = [double]::NegativeInfinity
$minZ = [double]::PositiveInfinity
$maxZ = [double]::NegativeInfinity

foreach ($pathPoints in $paths.Values) {
    foreach ($pt in $pathPoints) {
        if ($pt[0] -lt $minX) { $minX = $pt[0] }
        if ($pt[0] -gt $maxX) { $maxX = $pt[0] }
        if ($pt[1] -lt $minZ) { $minZ = $pt[1] }
        if ($pt[1] -gt $maxZ) { $maxZ = $pt[1] }
    }
}

if ($minX -ge $maxX -or $minZ -ge $maxZ) {
    Write-Error "Degenerate world bounds: x=[$minX,$maxX] z=[$minZ,$maxZ]. All entities at the same point?"
    exit 1
}

$padX = ($maxX - $minX) * 0.10
$padZ = ($maxZ - $minZ) * 0.10
$minX -= $padX; $maxX += $padX
$minZ -= $padZ; $maxZ += $padZ

# Aspect-correct fit. Keep the world's aspect ratio; letterbox if needed.
$worldW = $maxX - $minX
$worldH = $maxZ - $minZ
$worldAspect = $worldW / $worldH
$imageAspect = $Width / $Height
if ($worldAspect -gt $imageAspect) {
    # World is wider than image -- letterbox vertically.
    $drawW = $Width
    $drawH = [int]($Width / $worldAspect)
    $offsetX = 0
    $offsetY = [int](($Height - $drawH) / 2)
} else {
    $drawH = $Height
    $drawW = [int]($Height * $worldAspect)
    $offsetX = [int](($Width - $drawW) / 2)
    $offsetY = 0
}

function Project([double]$wx, [double]$wz) {
    $u = ($wx - $minX) / $worldW
    $v = 1.0 - (($wz - $minZ) / $worldH)  # flip Z so +Z is up on screen
    return @([int]($offsetX + $u * $drawW), [int]($offsetY + $v * $drawH))
}

# ----------------------------------------------------------------------
# 4. Render.
# ----------------------------------------------------------------------
$bmp = New-Object System.Drawing.Bitmap($Width, $Height)
$g   = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# Background.
$bgColor = [System.Drawing.Color]::FromArgb(20, 22, 28)
$g.Clear($bgColor)

# Drawing area outline (the "world rectangle").
$borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(60, 65, 75), 1.0)
$g.DrawRectangle($borderPen, $offsetX, $offsetY, $drawW, $drawH)
$borderPen.Dispose()

# Per-entity colour palette. Hash entity-id into a fixed palette of 17
# distinguishable hues + cycle for overflow. Priest gets a fixed bright
# red because there's typically only one and it's visually load-bearing.
$entityColours = @(
    @(255, 220,  50),   # yellow
    @(100, 200, 255),   # sky blue
    @(255, 130,  80),   # orange
    @(150, 255, 130),   # lime
    @(255, 180, 220),   # pink
    @(160, 130, 255),   # violet
    @(255, 255, 255),   # white
    @( 80, 220, 200),   # teal
    @(220, 220, 100),   # mustard
    @(180, 180, 180),   # grey
    @(255, 100, 100),   # salmon
    @(120, 220, 120),   # mint
    @(220, 120, 220),   # magenta
    @(120, 180, 220),   # ice
    @(220, 180, 120),   # tan
    @(180, 255, 200),   # honeydew
    @(255, 200, 100)    # peach
)

$keys = @($paths.Keys | Sort-Object)
$keyIdx = 0
foreach ($key in $keys) {
    $rgb = $entityColours[$keyIdx % $entityColours.Count]
    $pen = New-Object System.Drawing.Pen(
        [System.Drawing.Color]::FromArgb(220, $rgb[0], $rgb[1], $rgb[2]),
        1.6)

    $pts = $paths[$key]
    if ($pts.Count -ge 2) {
        $prev = Project $pts[0][0] $pts[0][1]
        for ($i = 1; $i -lt $pts.Count; ++$i) {
            $cur = Project $pts[$i][0] $pts[$i][1]
            $g.DrawLine($pen, $prev[0], $prev[1], $cur[0], $cur[1])
            $prev = $cur
        }
    }
    # Start-point dot so we can tell where each entity began.
    if ($pts.Count -ge 1) {
        $startBrush = New-Object System.Drawing.SolidBrush(
            [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2]))
        $sp = Project $pts[0][0] $pts[0][1]
        $g.FillEllipse($startBrush, $sp[0] - 3, $sp[1] - 3, 6, 6)
        $startBrush.Dispose()
    }
    $pen.Dispose()
    ++$keyIdx
}

# ----------------------------------------------------------------------
# 5. Event markers.
# ----------------------------------------------------------------------
$eventBrush = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(220, 255,  90,  90))
$eventPen   = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(220, 255, 255, 255), 1.0)
$labelFont  = New-Object System.Drawing.Font('Consolas', 10.0)
$labelBrush = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(220, 255, 255, 255))

# Each event marker is positioned at the entityA's location in the
# nearest preceding frame sample. If entityA is invalid or not found,
# fall back to centring the marker on the world bounds.
function FindEntityPosAtFrame($entityKey, $eventFrame) {
    foreach ($frame in $frames) {
        if ($frame.frame -gt $eventFrame) { break }
        foreach ($e in $frame.entities) {
            if ("$($e.id.idx):$($e.id.gen)" -eq $entityKey) {
                $lastX = [double]$e.pos[0]; $lastZ = [double]$e.pos[2]
            }
        }
    }
    if ($null -ne $lastX) { return @($lastX, $lastZ) }
    return $null
}

foreach ($evt in $events) {
    $entityKey = "$($evt.payload.entityA.idx):$($evt.payload.entityA.gen)"
    $pos = FindEntityPosAtFrame $entityKey $evt.frame
    if ($null -eq $pos) {
        # Fallback: middle of the world bounds.
        $pos = @(($minX + $maxX) / 2.0, ($minZ + $maxZ) / 2.0)
    }
    $p = Project $pos[0] $pos[1]
    $g.FillEllipse($eventBrush, $p[0] - 5, $p[1] - 5, 10, 10)
    $g.DrawEllipse($eventPen,   $p[0] - 5, $p[1] - 5, 10, 10)
    $label = if ($evt.PSObject.Properties.Name -contains 'name') { [string]$evt.name } else { "evt$($evt.type)" }
    $g.DrawString($label, $labelFont, $labelBrush, [float]($p[0] + 8), [float]($p[1] - 6))
}

# ----------------------------------------------------------------------
# 6. Header overlay (scene, seed, frame/event counts).
# ----------------------------------------------------------------------
$headerFont = New-Object System.Drawing.Font('Consolas', 12.0, [System.Drawing.FontStyle]::Bold)
$headerBrush = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(255, 230, 230, 230))
$headerText = "scene={0}  seed=0x{1:X}  frames={2}  events={3}" `
    -f $telemetry.header.sceneName, [int64]$telemetry.header.seed, $frames.Count, $events.Count
$g.DrawString($headerText, $headerFont, $headerBrush, 10.0, 10.0)

# World bounds caption.
$boundsText = "world bounds: x=[{0:F1},{1:F1}]  z=[{2:F1},{3:F1}]  ({4:F1}m x {5:F1}m)" `
    -f $minX, $maxX, $minZ, $maxZ, ($maxX - $minX), ($maxZ - $minZ)
$g.DrawString($boundsText, $labelFont, $labelBrush, 10.0, 32.0)

# ----------------------------------------------------------------------
# 7. Save + cleanup.
# ----------------------------------------------------------------------
$eventBrush.Dispose()
$eventPen.Dispose()
$labelFont.Dispose()
$labelBrush.Dispose()
$headerFont.Dispose()
$headerBrush.Dispose()
$g.Dispose()

$outDir = Split-Path -Parent $OutPath
if ($outDir -ne "" -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

if (-not $Quiet) {
    Write-Host "Wrote $OutPath ($([int]((Get-Item $OutPath).Length / 1024)) KB)" -ForegroundColor Green
}
exit 0
