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
# 2. Bucket per-entity samples. Key = packed entityID string "idx:gen".
#    Each sample carries the position + flags so state-flag-coded path
#    segments + role detection both work off the same structure.
# ----------------------------------------------------------------------
# StateFlags bit constants -- mirror DPTelemetry::StateFlags from
# Games/DevilsPlayground/Source/DPTelemetry.h.
$FLAG_POSSESSED      = 1
$FLAG_SPRINTING      = 2
$FLAG_WALK_QUIET     = 4
$FLAG_ALIVE          = 8
$FLAG_HOLDING_ITEM   = 16
$FLAG_PRIEST_SUSP    = 32
$FLAG_PRIEST_PURSUE  = 64

$paths = @{}    # key -> ArrayList of @{ X; Z; Flags; FrameIdx }
$idxOfKey = @{} # key -> first-seen sort index (preserves insertion order)
$nextIdx  = 0

foreach ($frame in $frames) {
    foreach ($e in $frame.entities) {
        $key = "$($e.id.idx):$($e.id.gen)"
        if (-not $paths.ContainsKey($key)) {
            $paths[$key] = New-Object System.Collections.ArrayList
            $idxOfKey[$key] = $nextIdx
            $nextIdx++
        }
        # pos is [x, y, z]; project on the X-Z plane (top-down).
        $sample = @{
            X = [double]$e.pos[0]
            Z = [double]$e.pos[2]
            Flags = [int]$e.flags
            FrameIdx = [int]$frame.frame
        }
        [void]$paths[$key].Add($sample)
    }
}

# Role detection. Walks each entity's full sample history to compute a
# logical role + an OR of every state flag seen. Used for both the
# legend labels and the "possessed villager gets a thick path" emphasis.
$entityMeta = @{}  # key -> @{ Role; FlagUnion; SortKey; LegendLabel }

$villagerCounter  = 0
$unknownCounter   = 0
$keysByIndex = @($idxOfKey.Keys | Sort-Object { $idxOfKey[$_] })

# First pass: classify each entity + record the first frame it had the
# Possessed flag (used to number possessions in temporal order).
foreach ($key in $keysByIndex) {
    $flagUnion = 0
    $firstPossessedFrame = [int]::MaxValue
    foreach ($s in $paths[$key]) {
        $flagUnion = $flagUnion -bor $s.Flags
        if (($s.Flags -band $FLAG_POSSESSED) -ne 0 -and $s.FrameIdx -lt $firstPossessedFrame) {
            $firstPossessedFrame = $s.FrameIdx
        }
    }

    $role = 'Other'
    if (($flagUnion -band $FLAG_POSSESSED) -ne 0) {
        $role = 'Possessed'
    }
    elseif (($flagUnion -band ($FLAG_PRIEST_SUSP -bor $FLAG_PRIEST_PURSUE)) -ne 0) {
        $role = 'Priest'
    }
    elseif (($flagUnion -band $FLAG_ALIVE) -ne 0) {
        $role = 'Villager'
    }
    else {
        $role = 'Unknown'
    }

    $entityMeta[$key] = @{
        Role                  = $role
        FlagUnion             = $flagUnion
        FirstPossessedFrame   = $firstPossessedFrame
        LegendLabel           = ''  # filled below
        SortKey               = $idxOfKey[$key]
    }
}

# Second pass: number Possessed entities in temporal order (whichever
# villager got the Possessed flag earliest is "#1"); other roles in
# entity-index order.
$possessedSorted = @($keysByIndex |
    Where-Object { $entityMeta[$_].Role -eq 'Possessed' } |
    Sort-Object { $entityMeta[$_].FirstPossessedFrame })

$possessedCounter = 0
foreach ($key in $possessedSorted) {
    $possessedCounter++
    $entityMeta[$key].LegendLabel = "Possessed #$possessedCounter"
}

foreach ($key in $keysByIndex) {
    if ($entityMeta[$key].LegendLabel -ne '') { continue }
    switch ($entityMeta[$key].Role) {
        'Priest'   { $entityMeta[$key].LegendLabel = 'Priest' }
        'Villager' {
            $villagerCounter++
            $entityMeta[$key].LegendLabel = "Villager #$villagerCounter"
        }
        'Unknown'  {
            $unknownCounter++
            # In current packing, the priest comes through with flags=0
            # because Test_DPHeuristicBotPlaythrough's EmitPositionSample
            # doesn't pack priest state flags (TBD per the test's TODO).
            $entityMeta[$key].LegendLabel = "Unknown / priest? #$unknownCounter"
        }
        default    { $entityMeta[$key].LegendLabel = 'Other' }
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

foreach ($pathSamples in $paths.Values) {
    foreach ($s in $pathSamples) {
        if ($s.X -lt $minX) { $minX = $s.X }
        if ($s.X -gt $maxX) { $maxX = $s.X }
        if ($s.Z -lt $minZ) { $minZ = $s.Z }
        if ($s.Z -gt $maxZ) { $maxZ = $s.Z }
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

$keys = $keysByIndex   # sorted by first-seen order so colour assignment is stable

# Pre-build colours indexed by entity. Stash on the meta entry so the
# legend later reads the same RGB without rerunning the modulo.
for ($keyIdx = 0; $keyIdx -lt $keys.Count; ++$keyIdx) {
    $rgb = $entityColours[$keyIdx % $entityColours.Count]
    $entityMeta[$keys[$keyIdx]].RGB = $rgb
}

# State-flag colour overrides for path segments. Sprint wins ties with
# walk-quiet (matches the in-game DPVillager precedence).
$sprintRGB = @(255,  80,  80)   # red
$quietRGB  = @( 80, 170, 255)   # blue
function FlagsSegmentRGB([int]$flagsA, [int]$flagsB, [int[]]$defaultRGB) {
    $combined = $flagsA -bor $flagsB
    if (($combined -band $FLAG_SPRINTING) -ne 0)  { return $sprintRGB }
    if (($combined -band $FLAG_WALK_QUIET) -ne 0) { return $quietRGB }
    return $defaultRGB
}

# Pass 1: non-emphasised entities (everything that's NOT the possessed
# villager). Drawn first so the possessed villager's path lands on top.
function DrawEntityPath($key, [bool]$bEmphasis) {
    $meta = $entityMeta[$key]
    $rgb  = $meta.RGB
    $width = if ($bEmphasis) { 2.6 } else { 1.5 }

    $samples = $paths[$key]
    if ($samples.Count -lt 1) { return }

    if ($samples.Count -ge 2) {
        $prev = Project $samples[0].X $samples[0].Z
        $prevFlags = $samples[0].Flags
        for ($i = 1; $i -lt $samples.Count; ++$i) {
            $cur = Project $samples[$i].X $samples[$i].Z
            $segRGB = FlagsSegmentRGB $prevFlags $samples[$i].Flags $rgb
            $pen = New-Object System.Drawing.Pen(
                [System.Drawing.Color]::FromArgb(230, $segRGB[0], $segRGB[1], $segRGB[2]),
                $width)
            $g.DrawLine($pen, $prev[0], $prev[1], $cur[0], $cur[1])
            $pen.Dispose()
            # Direction-arrow every ~25 segments along a long path so the
            # eye can follow time without crowding short paths with arrows.
            if ($bEmphasis -and (($i % 25) -eq 0)) {
                DrawArrowHead $prev $cur $segRGB
            }
            $prev = $cur
            $prevFlags = $samples[$i].Flags
        }
    }

    # Start-point ring (filled circle + thin outline).
    $sp = Project $samples[0].X $samples[0].Z
    $startBrush = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2]))
    $g.FillEllipse($startBrush, $sp[0] - 4, $sp[1] - 4, 8, 8)
    $startBrush.Dispose()
}

# Tiny filled arrowhead at the END of a segment, pointing toward $cur.
function DrawArrowHead($prev, $cur, [int[]]$rgb) {
    $dx = $cur[0] - $prev[0]
    $dy = $cur[1] - $prev[1]
    $len = [math]::Sqrt($dx * $dx + $dy * $dy)
    if ($len -lt 1.0) { return }
    $nx = $dx / $len
    $ny = $dy / $len
    $size = 6.0
    # Triangle: tip = cur; base = cur - n * size, splayed by perpendicular * size/2.
    $tipX = $cur[0]
    $tipY = $cur[1]
    $baseCx = $cur[0] - $nx * $size
    $baseCy = $cur[1] - $ny * $size
    $perpx = -$ny * ($size / 2.0)
    $perpy =  $nx * ($size / 2.0)
    $pt1 = New-Object System.Drawing.PointF([float]$tipX, [float]$tipY)
    $pt2 = New-Object System.Drawing.PointF([float]($baseCx + $perpx), [float]($baseCy + $perpy))
    $pt3 = New-Object System.Drawing.PointF([float]($baseCx - $perpx), [float]($baseCy - $perpy))
    [System.Drawing.PointF[]]$arrowPts = @($pt1, $pt2, $pt3)
    $brush = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(240, $rgb[0], $rgb[1], $rgb[2]))
    $g.FillPolygon($brush, $arrowPts)
    $brush.Dispose()
}

# Pass 1: draw all non-possessed paths first (background layer).
foreach ($key in $keys) {
    if ($entityMeta[$key].Role -eq 'Possessed') { continue }
    DrawEntityPath $key $false
}

# Pass 2: possessed villager's path on top, with arrowheads + thicker stroke.
foreach ($key in $keys) {
    if ($entityMeta[$key].Role -ne 'Possessed') { continue }
    DrawEntityPath $key $true
}

# ----------------------------------------------------------------------
# 5. Event markers -- per-type shape + colour. Each gameplay-milestone
# event gets its own distinguishable glyph so the viewer can read the
# sequence of milestones at a glance without staring at labels.
# ----------------------------------------------------------------------
$labelFont  = New-Object System.Drawing.Font('Consolas', 10.0)
$labelBrush = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(230, 255, 255, 255))
$markerOutlinePen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(230, 255, 255, 255), 1.0)

# Lookup table: event name -> @{ Shape; RGB }.
#   Shape values: 'Circle', 'CircleOutline', 'Square', 'Diamond',
#                 'Triangle', 'X', 'Star', 'Dot'.
$eventStyles = @{
    'ItemPickup'        = @{ Shape = 'Circle';        RGB = @(110, 220, 110) }   # green filled
    'ItemDrop'          = @{ Shape = 'CircleOutline'; RGB = @(110, 220, 110) }   # green outline
    'Interact'          = @{ Shape = 'Dot';           RGB = @(180, 180, 180) }   # small grey
    'InteractionBegin'  = @{ Shape = 'Triangle';      RGB = @(180, 180, 200) }   # light triangle
    'InteractionEnd'    = @{ Shape = 'Triangle';      RGB = @(200, 200, 180) }   # light triangle (slight tint)
    'InteractionCancel' = @{ Shape = 'X';             RGB = @(180, 180, 180) }
    'VillagerDied'      = @{ Shape = 'X';             RGB = @(255,  80,  80) }   # red X
    'Victory'           = @{ Shape = 'Star';          RGB = @(255, 215,   0) }   # gold star
    'RunLost'           = @{ Shape = 'X';             RGB = @(190,  40,  40) }   # dark red X
    'BellRing'          = @{ Shape = 'CircleOutline'; RGB = @(255, 240, 100) }   # yellow outline (sound)
    'PriestStateChange' = @{ Shape = 'Square';        RGB = @(180, 100, 220) }   # purple square
    'PossessedSwitched' = @{ Shape = 'Circle';        RGB = @(240, 240, 240) }   # white-ish
    'PossessionChanged' = @{ Shape = 'Circle';        RGB = @(255, 255, 255) }   # bright white
    'Possession'        = @{ Shape = 'Circle';        RGB = @(220, 220, 220) }   # legacy alias
    'Unpossession'      = @{ Shape = 'CircleOutline'; RGB = @(180, 180, 180) }   # legacy alias
    'DoorOpened'        = @{ Shape = 'Square';        RGB = @(160, 110,  70) }   # brown
    'ChestOpened'       = @{ Shape = 'Square';        RGB = @(220, 160,  60) }   # tan-brown
    'ForgeCrafted'      = @{ Shape = 'Triangle';      RGB = @(255, 130,  40) }   # orange flame
    'ObjectivePlaced'   = @{ Shape = 'Diamond';       RGB = @( 90, 170, 255) }   # blue diamond
}

function ResolveStyle($name) {
    if ($null -ne $name -and $eventStyles.ContainsKey($name)) {
        return $eventStyles[$name]
    }
    # Default for any new / unrecognised event type.
    return @{ Shape = 'Circle'; RGB = @(255, 90, 90) }
}

function DrawEventMarker($cx, $cy, $style) {
    $rgb = $style.RGB
    $fillBrush = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(230, $rgb[0], $rgb[1], $rgb[2]))
    $sz = 5
    switch ($style.Shape) {
        'Circle'        {
            $g.FillEllipse($fillBrush, $cx - $sz, $cy - $sz, $sz * 2, $sz * 2)
            $g.DrawEllipse($markerOutlinePen, $cx - $sz, $cy - $sz, $sz * 2, $sz * 2)
        }
        'CircleOutline' {
            $coloredPen = New-Object System.Drawing.Pen(
                [System.Drawing.Color]::FromArgb(230, $rgb[0], $rgb[1], $rgb[2]), 2.0)
            $g.DrawEllipse($coloredPen, $cx - $sz, $cy - $sz, $sz * 2, $sz * 2)
            $coloredPen.Dispose()
        }
        'Square'        {
            $g.FillRectangle($fillBrush, $cx - $sz, $cy - $sz, $sz * 2, $sz * 2)
            $g.DrawRectangle($markerOutlinePen, $cx - $sz, $cy - $sz, $sz * 2, $sz * 2)
        }
        'Diamond'       {
            [System.Drawing.PointF[]]$pts = @(
                (New-Object System.Drawing.PointF([float]$cx, [float]($cy - $sz - 1))),
                (New-Object System.Drawing.PointF([float]($cx + $sz + 1), [float]$cy)),
                (New-Object System.Drawing.PointF([float]$cx, [float]($cy + $sz + 1))),
                (New-Object System.Drawing.PointF([float]($cx - $sz - 1), [float]$cy))
            )
            $g.FillPolygon($fillBrush, $pts)
            $g.DrawPolygon($markerOutlinePen, $pts)
        }
        'Triangle'      {
            [System.Drawing.PointF[]]$pts = @(
                (New-Object System.Drawing.PointF([float]$cx, [float]($cy - $sz - 1))),
                (New-Object System.Drawing.PointF([float]($cx + $sz + 1), [float]($cy + $sz))),
                (New-Object System.Drawing.PointF([float]($cx - $sz - 1), [float]($cy + $sz)))
            )
            $g.FillPolygon($fillBrush, $pts)
            $g.DrawPolygon($markerOutlinePen, $pts)
        }
        'X'             {
            $coloredPen = New-Object System.Drawing.Pen(
                [System.Drawing.Color]::FromArgb(230, $rgb[0], $rgb[1], $rgb[2]), 2.5)
            $g.DrawLine($coloredPen, $cx - $sz, $cy - $sz, $cx + $sz, $cy + $sz)
            $g.DrawLine($coloredPen, $cx - $sz, $cy + $sz, $cx + $sz, $cy - $sz)
            $coloredPen.Dispose()
        }
        'Star'          {
            # 5-point star. Compute outer + inner vertices.
            $rawPts = @()
            for ($i = 0; $i -lt 10; ++$i) {
                $r = if ($i % 2 -eq 0) { $sz + 2 } else { ($sz + 2) * 0.45 }
                $theta = ($i / 10.0) * 2.0 * [math]::PI - [math]::PI / 2.0
                $rawPts += (New-Object System.Drawing.PointF(
                    [float]($cx + $r * [math]::Cos($theta)),
                    [float]($cy + $r * [math]::Sin($theta))))
            }
            [System.Drawing.PointF[]]$pts = $rawPts
            $g.FillPolygon($fillBrush, $pts)
            $g.DrawPolygon($markerOutlinePen, $pts)
        }
        'Dot'           {
            $g.FillEllipse($fillBrush, $cx - 2, $cy - 2, 4, 4)
        }
    }
    $fillBrush.Dispose()
}

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

# Track which (x,y) bands we've used so subsequent event labels at the
# same screen position stagger vertically rather than overlapping.
$labelBuckets = @{}
function StaggerLabelY($cx, $cy) {
    $bucketKey = "{0}:{1}" -f [int]($cx / 12), [int]($cy / 18)
    if (-not $labelBuckets.ContainsKey($bucketKey)) {
        $labelBuckets[$bucketKey] = 0
    } else {
        $labelBuckets[$bucketKey]++
    }
    return $cy - 6 + ($labelBuckets[$bucketKey] * 14)
}

# Sort events by frame so the timeline is built in time order.
$sortedEvents = @($events | Sort-Object { [int]$_.frame })

foreach ($evt in $sortedEvents) {
    $entityKey = "$($evt.payload.entityA.idx):$($evt.payload.entityA.gen)"
    $pos = FindEntityPosAtFrame $entityKey $evt.frame
    if ($null -eq $pos) {
        # Fallback: middle of the world bounds.
        $pos = @(($minX + $maxX) / 2.0, ($minZ + $maxZ) / 2.0)
    }
    $p = Project $pos[0] $pos[1]
    $rawName = if ($evt.PSObject.Properties.Name -contains 'name') { [string]$evt.name } else { "evt$($evt.type)" }
    $style = ResolveStyle $rawName
    DrawEventMarker $p[0] $p[1] $style
    $label = ("{0}  f={1} t={2:F1}s" -f $rawName, [int]$evt.frame, [double]$evt.t)
    $labelY = StaggerLabelY $p[0] $p[1]
    $g.DrawString($label, $labelFont, $labelBrush, [float]($p[0] + 8), [float]$labelY)
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
# 5b. Timeline strip (top-of-image, under the header). Horizontal bar
#     spanning the drawing area. Each event plotted as a thin vertical
#     tick at x = (event.frame / maxFrame) * drawW. Provides a
#     time-axis view of the same events the spatial map shows.
# ----------------------------------------------------------------------
$maxFrame = 1
foreach ($frame in $frames) {
    if ([int]$frame.frame -gt $maxFrame) { $maxFrame = [int]$frame.frame }
}
foreach ($evt in $events) {
    if ([int]$evt.frame -gt $maxFrame) { $maxFrame = [int]$evt.frame }
}
$tlY      = 78   # sits below the two header lines (top y=10 + y=32) with breathing room
$tlX0     = $offsetX
$tlX1     = $offsetX + $drawW
$tlPen    = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(180, 90, 100, 120), 1.0)
$g.DrawLine($tlPen, $tlX0, $tlY, $tlX1, $tlY)
$g.DrawLine($tlPen, $tlX0, $tlY - 4, $tlX0, $tlY + 4)
$g.DrawLine($tlPen, $tlX1, $tlY - 4, $tlX1, $tlY + 4)
$tlPen.Dispose()
$tlLabelFont = New-Object System.Drawing.Font('Consolas', 8.5)
$tlLabelBrush = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(200, 200, 200, 210))
$tlT0 = "0 s"
$tlT1 = "{0:F1} s" -f ($maxFrame * $telemetry.header.fixedDt)
$g.DrawString($tlT0, $tlLabelFont, $tlLabelBrush, [float]($tlX0 - 2), [float]($tlY + 6))
$g.DrawString($tlT1, $tlLabelFont, $tlLabelBrush, [float]($tlX1 - 40), [float]($tlY + 6))
$g.DrawString("Timeline (events over t)", $tlLabelFont, $tlLabelBrush,
    [float]($tlX0 + ($tlX1 - $tlX0) / 2 - 70), [float]($tlY - 18))
# Event ticks above the line, coloured by event style.
foreach ($evt in $sortedEvents) {
    $rawName = if ($evt.PSObject.Properties.Name -contains 'name') { [string]$evt.name } else { "evt$($evt.type)" }
    $style = ResolveStyle $rawName
    $rgb = $style.RGB
    $u = if ($maxFrame -gt 0) { [int]$evt.frame / [double]$maxFrame } else { 0.0 }
    $tickX = $tlX0 + [int]($u * ($tlX1 - $tlX0))
    $coloredPen = New-Object System.Drawing.Pen(
        [System.Drawing.Color]::FromArgb(220, $rgb[0], $rgb[1], $rgb[2]), 2.0)
    $g.DrawLine($coloredPen, $tickX, $tlY - 6, $tickX, $tlY + 6)
    $coloredPen.Dispose()
}
$tlLabelFont.Dispose()
$tlLabelBrush.Dispose()

# ----------------------------------------------------------------------
# 6a. Distance scale bar (bottom-left). Pick a "nice" round number
#     for the legend metres -- 5, 10, 20, 25, 50 depending on world size.
# ----------------------------------------------------------------------
$scaleCandidates = @(5, 10, 20, 25, 50, 100)
$scaleMetres = 10
foreach ($c in $scaleCandidates) {
    # Want the bar to be roughly 100-200 px wide on screen.
    $pxWidth = ($c / $worldW) * $drawW
    if ($pxWidth -ge 100 -and $pxWidth -le 200) { $scaleMetres = $c; break }
}
$scalePxWidth = [int](($scaleMetres / $worldW) * $drawW)
$scaleX0 = 20
$scaleY  = $Height - 35
$scalePen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(220, 230, 230, 230), 2.0)
$g.DrawLine($scalePen, $scaleX0, $scaleY, $scaleX0 + $scalePxWidth, $scaleY)
$g.DrawLine($scalePen, $scaleX0, $scaleY - 5, $scaleX0, $scaleY + 5)
$g.DrawLine($scalePen, $scaleX0 + $scalePxWidth, $scaleY - 5, $scaleX0 + $scalePxWidth, $scaleY + 5)
$scalePen.Dispose()
$scaleText = "${scaleMetres} m"
$g.DrawString($scaleText, $labelFont, $labelBrush, [float]($scaleX0 + $scalePxWidth / 2 - 12), [float]($scaleY + 5))

# ----------------------------------------------------------------------
# 6b. Legend (bottom-right). Translucent panel + one row per entity
#     (sorted: possessed first, then priest, then villagers, then others).
#     Also documents the state-flag colour overrides (sprint, walk-quiet).
# ----------------------------------------------------------------------
$rolePriority = @{ 'Possessed' = 0; 'Priest' = 1; 'Villager' = 2; 'Unknown' = 3; 'Other' = 4 }
$legendEntries = @($keys |
    Sort-Object { $rolePriority[$entityMeta[$_].Role] }, { $idxOfKey[$_] })

# State-flag legend rows added at the END so they appear visually
# separated from the entity rows (after a divider).
$legendRowHeight = 16
$legendPadding   = 8
# Rows: entities + 1 divider + 2 state-flag rows + 1 divider + 1 "Event marker" row.
$legendRowCount  = $legendEntries.Count + 4
$legendWidth     = 260   # widened so "Possessed #N  n=NNN" fits without clipping
$legendHeight    = $legendPadding * 2 + $legendRowHeight * $legendRowCount
$legendX         = $Width  - $legendWidth - 10
$legendY         = $Height - $legendHeight - 10

# Panel background (translucent dark).
$legendBg = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(180, 12, 14, 18))
$g.FillRectangle($legendBg, $legendX, $legendY, $legendWidth, $legendHeight)
$legendBg.Dispose()
$legendBorderPen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(140, 100, 110, 130), 1.0)
$g.DrawRectangle($legendBorderPen, $legendX, $legendY, $legendWidth, $legendHeight)
$legendBorderPen.Dispose()

$legendFont = New-Object System.Drawing.Font('Consolas', 9.5)

$row = 0
foreach ($key in $legendEntries) {
    $meta = $entityMeta[$key]
    $rgb  = $meta.RGB
    $cy   = $legendY + $legendPadding + $row * $legendRowHeight + 8
    # Swatch (filled circle) matching the path stroke.
    $brush = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2]))
    $g.FillEllipse($brush, $legendX + $legendPadding, $cy - 5, 10, 10)
    $brush.Dispose()
    # Label.
    $samplesCount = $paths[$key].Count
    $rowLabel = "{0,-22}  n={1}" -f $meta.LegendLabel, $samplesCount
    $g.DrawString($rowLabel, $legendFont, $labelBrush,
        [float]($legendX + $legendPadding + 16),
        [float]($cy - 7))
    ++$row
}

# Divider line.
$dividerPen = New-Object System.Drawing.Pen(
    [System.Drawing.Color]::FromArgb(150, 100, 110, 130), 1.0)
$divY1 = $legendY + $legendPadding + $row * $legendRowHeight + 4
$g.DrawLine($dividerPen,
    $legendX + $legendPadding, $divY1,
    $legendX + $legendWidth - $legendPadding, $divY1)
$row++

# State-flag legend rows.
$flagLegend = @(
    @{ RGB = $sprintRGB; Label = 'Sprint frames'    },
    @{ RGB = $quietRGB;  Label = 'Walk-quiet frames' }
)
foreach ($entry in $flagLegend) {
    $cy = $legendY + $legendPadding + $row * $legendRowHeight + 8
    $rgb = $entry.RGB
    $brush = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2]))
    $g.FillRectangle($brush, $legendX + $legendPadding, $cy - 4, 12, 4)
    $brush.Dispose()
    $g.DrawString($entry.Label, $legendFont, $labelBrush,
        [float]($legendX + $legendPadding + 16),
        [float]($cy - 7))
    ++$row
}

# Second divider.
$divY2 = $legendY + $legendPadding + $row * $legendRowHeight + 4
$g.DrawLine($dividerPen,
    $legendX + $legendPadding, $divY2,
    $legendX + $legendWidth - $legendPadding, $divY2)
$dividerPen.Dispose()
$row++

# Event marker row.
$cy = $legendY + $legendPadding + $row * $legendRowHeight + 8
$evtBrushLegend = New-Object System.Drawing.SolidBrush(
    [System.Drawing.Color]::FromArgb(220, 255, 90, 90))
$g.FillEllipse($evtBrushLegend, $legendX + $legendPadding, $cy - 5, 10, 10)
$evtBrushLegend.Dispose()
$g.DrawString("Game event", $legendFont, $labelBrush,
    [float]($legendX + $legendPadding + 16),
    [float]($cy - 7))

$legendFont.Dispose()

# ----------------------------------------------------------------------
# 7. Save + cleanup.
# ----------------------------------------------------------------------
$markerOutlinePen.Dispose()
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
