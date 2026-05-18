# dp_proclevel_visualise.ps1 -- render a top-down PNG of a procgen LevelLayout.
#
# Reads the JSON exported by DPProcLevel::ExportLayoutJson (per-seed file
# under %TEMP%, or wherever Test_ProcLevel_BSP wrote it) and draws:
#   * world bounds (the 100x100 m rectangle)
#   * each room as a filled OBB (rotated by yawRadians)
#   * each corridor as a line between its two door points
#   * door points as small markers
#   * a caption with seed + room count
#
# Uses the same world->image projection convention as
# dp_telemetry_visualise.ps1 (Z flipped so +Z is screen up) so PNGs from
# the two tools sit side-by-side without coordinate confusion.
#
# Usage:
#   pwsh ./Tools/dp_proclevel_visualise.ps1
#   pwsh ./Tools/dp_proclevel_visualise.ps1 -JsonPath C:\path\to\layout.json
#   pwsh ./Tools/dp_proclevel_visualise.ps1 -OutPath my.png -Width 2048 -Height 2048
#
# ASCII-only script body (Windows PowerShell 5.1 + pwsh 7+).

[CmdletBinding()]
param(
    [string]$JsonPath = "$env:TEMP/dp_proclevel_seed_0.json",
    [string]$OutPath  = "",
    [int]$Width       = 1024,
    [int]$Height      = 1024,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path $JsonPath)) {
    Write-Error "Layout JSON not found: $JsonPath"
    exit 1
}

if ($OutPath -eq "") {
    $dir   = Split-Path -Parent $JsonPath
    $stem  = [IO.Path]::GetFileNameWithoutExtension($JsonPath)
    $OutPath = Join-Path $dir "$stem.png"
}

Add-Type -AssemblyName System.Drawing

if (-not $Quiet) { Write-Host "Loading $JsonPath..." -ForegroundColor Cyan }
$layout = Get-Content $JsonPath -Raw | ConvertFrom-Json

$rooms      = if ($layout.rooms)      { @($layout.rooms) }      else { @() }
$doors      = if ($layout.doorPoints) { @($layout.doorPoints) } else { @() }
$corridors  = if ($layout.corridors)  { @($layout.corridors) }  else { @() }

if (-not $Quiet) { Write-Host "  + $($rooms.Count) rooms, $($doors.Count) doors, $($corridors.Count) corridors" -ForegroundColor DarkGray }

# ----------------------------------------------------------------------
# World <-> image projection
# ----------------------------------------------------------------------
$bounds = $layout.header.bounds
$minX = [double]$bounds.minX
$maxX = [double]$bounds.maxX
$minZ = [double]$bounds.minZ
$maxZ = [double]$bounds.maxZ
$worldW = $maxX - $minX
$worldH = $maxZ - $minZ
$worldAspect = $worldW / $worldH

# 10% padding inside the image so room edges don't kiss the canvas
$pad = 0.05
$drawW = [int]($Width  * (1.0 - 2 * $pad))
$drawH = [int]($Height * (1.0 - 2 * $pad))
# Letterbox to maintain world aspect ratio
if ($drawW / $drawH -gt $worldAspect) {
    $drawW = [int]($drawH * $worldAspect)
}
else {
    $drawH = [int]($drawW / $worldAspect)
}
$offsetX = [int](($Width  - $drawW) / 2)
$offsetY = [int](($Height - $drawH) / 2)

function Project([double]$wx, [double]$wz) {
    $u = ($wx - $minX) / $worldW
    $v = 1.0 - (($wz - $minZ) / $worldH)  # flip Z so +Z is up on screen
    return @([int]($offsetX + $u * $drawW), [int]($offsetY + $v * $drawH))
}

# ----------------------------------------------------------------------
# Render
# ----------------------------------------------------------------------
$bmp = New-Object System.Drawing.Bitmap($Width, $Height)
$g   = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

# Background
$bgColor = [System.Drawing.Color]::FromArgb(20, 22, 28)
$g.Clear($bgColor)

# World-bounds rectangle
$borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(80, 85, 95), 1.5)
$g.DrawRectangle($borderPen, $offsetX, $offsetY, $drawW, $drawH)
$borderPen.Dispose()

# Per-room colour palette -- subtle pastel hues so adjacent rooms read
# as distinct regions but the whole map feels coherent. Cycles for runs
# with >palette-size rooms.
$roomPalette = @(
    @(80, 130, 180), @(180, 110, 90), @(110, 170, 110), @(180, 160, 90),
    @(160, 100, 170), @(100, 170, 170), @(200, 130, 150), @(150, 150, 200)
)

# Rooms (filled OBB)
for ($i = 0; $i -lt $rooms.Count; ++$i) {
    $room = $rooms[$i]
    $cx  = [double]$room.cx
    $cz  = [double]$room.cz
    $hx  = [double]$room.hx
    $hz  = [double]$room.hz
    $yaw = [double]$room.yaw
    $cosY = [math]::Cos($yaw)
    $sinY = [math]::Sin($yaw)
    # Same R_y rotation matrix the telemetry visualiser uses (see
    # PR #95). Local (lx, lz) -> world (lx*cos + lz*sin, -lx*sin + lz*cos).
    $corners = @(
        @(-$hx, -$hz), @( $hx, -$hz), @( $hx,  $hz), @(-$hx,  $hz)
    )
    $worldPts = @()
    foreach ($lc in $corners) {
        $wx = $cx + $lc[0] * $cosY + $lc[1] * $sinY
        $wz = $cz - $lc[0] * $sinY + $lc[1] * $cosY
        $pt = Project $wx $wz
        $worldPts += New-Object System.Drawing.Point($pt[0], $pt[1])
    }
    $rgb = $roomPalette[$i % $roomPalette.Count]
    # Fill with ~50% alpha so corridors + doors stay readable.
    $fill = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(140, $rgb[0], $rgb[1], $rgb[2]))
    $stroke = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2]), 2.0)
    $g.FillPolygon($fill, [System.Drawing.Point[]]$worldPts)
    $g.DrawPolygon($stroke, [System.Drawing.Point[]]$worldPts)
    $fill.Dispose()
    $stroke.Dispose()

    # Room id label at the room centre
    $centreImg = Project $cx $cz
    $labelFont = New-Object System.Drawing.Font('Consolas', 11.0, [System.Drawing.FontStyle]::Bold)
    $labelBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.DrawString("R$($room.id)", $labelFont, $labelBrush, ($centreImg[0] - 10), ($centreImg[1] - 10))
    $labelFont.Dispose()
    $labelBrush.Dispose()
}

# Corridors (line between two door points)
$corridorPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(220, 220, 220, 230), 2.0)
foreach ($c in $corridors) {
    if ($c.doorA -lt 0 -or $c.doorA -ge $doors.Count) { continue }
    if ($c.doorB -lt 0 -or $c.doorB -ge $doors.Count) { continue }
    $dA = $doors[$c.doorA]
    $dB = $doors[$c.doorB]
    $pA = Project ([double]$dA.x) ([double]$dA.z)
    $pB = Project ([double]$dB.x) ([double]$dB.z)
    $g.DrawLine($corridorPen, $pA[0], $pA[1], $pB[0], $pB[1])
}
$corridorPen.Dispose()

# Door points (small dots) -- drawn last so they sit on top of rooms +
# corridors.
$doorFill = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 255, 230, 100))
$doorStroke = New-Object System.Drawing.Pen([System.Drawing.Color]::Black, 1.0)
foreach ($d in $doors) {
    $p = Project ([double]$d.x) ([double]$d.z)
    $g.FillEllipse($doorFill, ($p[0] - 4), ($p[1] - 4), 8, 8)
    $g.DrawEllipse($doorStroke, ($p[0] - 4), ($p[1] - 4), 8, 8)
}
$doorFill.Dispose()
$doorStroke.Dispose()

# Caption (top-left)
$capFont = New-Object System.Drawing.Font('Consolas', 14.0, [System.Drawing.FontStyle]::Regular)
$capBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(220, 220, 230))
$caption = "seed=$($layout.header.seed)  rooms=$($rooms.Count)  doors=$($doors.Count)  corridors=$($corridors.Count)"
$g.DrawString($caption, $capFont, $capBrush, 10, 8)
$boundsCaption = "world bounds: x=[{0:F1},{1:F1}]  z=[{2:F1},{3:F1}]  ({4:F1}m x {5:F1}m)" -f $minX, $maxX, $minZ, $maxZ, $worldW, $worldH
$g.DrawString($boundsCaption, $capFont, $capBrush, 10, 30)
$capFont.Dispose()
$capBrush.Dispose()

# Save
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()

if (-not $Quiet) {
    $size = [int]((Get-Item $OutPath).Length / 1024)
    Write-Host "Wrote $OutPath ($size KB)" -ForegroundColor Green
}
