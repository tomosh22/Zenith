# capture_viewport.ps1 -- windowed screenshot evidence, cropped to the GAME
# VIEWPORT ONLY (no editor chrome).
#
# AgentBriefing.md section "Windowed screenshot evidence" describes the
# SetWindowPos + CopyFromScreen recipe but no script implemented it, so every
# visual gate re-invented one. This is that script.
#
# ★ WHY THE CROP MATTERS. A tools build docks the game view INSIDE the editor,
# so a naive full-window capture is ~30% hierarchy panel and a shrunken game
# view. Worse, the viewport origin is NOT a constant -- the editor layout scales
# with window size, so an offset measured at one window size is wrong at
# another. This script DETECTS the viewport edges per run instead of assuming
# them (the editor panel is a flat grey; the viewport is not).
#
# Usage:
#   pwsh -File Tools\capture_viewport.ps1 -TestName ZM_RivalVesperAuthored_Test
#   pwsh -File Tools\capture_viewport.ps1 -TestName X -IntervalMs 60   # fast beats
#
# ★ SAMPLING RATE IS THE DIFFERENCE BETWEEN EVIDENCE AND A MISS. The harness
# pins dt to 1/30 s. A 0.35 s beat is ~11 frames; the rival's approach walk is
# ~43 frames (~1.4 s). At 250 ms you sample 1 frame in 8 and will MISS them.
#
# ★★ AND -IntervalMs IS A FLOOR THIS SCRIPT CANNOT REACH AT USABLE RESOLUTIONS.
# This header used to say "use -IntervalMs 60 or lower for anything short". It
# cannot. MEASURED 2026-07-30: -IntervalMs 40 delivered an ACTUAL 206 ms at
# 2560x1440, and 81 ms at 1280x800 -- CopyFromScreen plus PNG encode of a
# multi-megapixel frame dominates the loop, so the parameter is advisory and the
# real rate is set by resolution. A 0.35 s beat therefore gets 1-2 samples even
# when you asked for 8, which is exactly how ZM-D-168's "zero marker frames"
# null result happened, and how ZM-D-169 reproduced it twice more.
#
# ★ SO: FOR ANY BEAT SHORTER THAN ABOUT A SECOND, DO NOT USE THIS SCRIPT TO
# DECIDE PRESENCE. Use the frame-exact engine path instead --
# Flux_Screenshot::RequestDump(path) from inside the frame you care about,
# consumed once per EndFrame by Zenith_Vulkan_Swapchain (a no-op on the Null
# backend, so gate the assertion on Zenith_IsNullRenderer()). See
# ZM_RivalVesperAuthored_Test and ZM_NpcRenderedPalette_Test.
# This script remains the right tool for WATCHING a run and for long beats.
#
# Either way, treat "I did not capture it" as unproven rather than absent -- and
# if you scan the output for a colour, derive the predicate from bytes the engine
# actually wrote, never from the colour you submitted (ZM-D-169: an unlit gold
# marker submitting linear (1.0, 0.82, 0.08) lands at RGB(208, 182, 97)).
#
# Output: Build/artifacts/shots/<TestName>/f####.png (git-ignored, never committed).
# ASCII-only body; pwsh 7.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TestName,
    [int]$IntervalMs = 100,
    [int]$MaxSeconds = 420,
    [int]$WindowW = 2560,
    [int]$WindowH = 1440,
    [string]$OutRoot = "Build\artifacts\shots",
    [switch]$FullClient
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class ZmCap {
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

$dir = Join-Path $repo (Join-Path $OutRoot $TestName)
if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $dir | Out-Null

Get-Process zenithmon -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500

$log = Join-Path $dir "run.log"
$proc = Start-Process -FilePath "pwsh" `
    -ArgumentList @("-NoProfile", "-File", "Tools\zenith.ps1", "test", "Zenithmon", "--filter", $TestName) `
    -PassThru -RedirectStandardOutput $log -WindowStyle Hidden

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$game = $null
while ($sw.Elapsed.TotalSeconds -lt $MaxSeconds -and -not $proc.HasExited) {
    $game = Get-Process zenithmon -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($game) { break }
    Start-Sleep -Milliseconds 150
}
if (-not $game) {
    Write-Error "[capture] no game window appeared for $TestName (test may have exited first)"
    if (-not $proc.HasExited) { $proc.Kill() }
    exit 2
}

$h = $game.MainWindowHandle
# CopyFromScreen reads the composed desktop, so merely foregrounding is not
# sufficient when the invoking app reclaims focus. Keep the game above other
# windows for the lifetime of this short-lived capture process.
[void][ZmCap]::SetWindowPos($h, [IntPtr](-1), 0, 0, $WindowW, $WindowH, 0x0040)
Start-Sleep -Milliseconds 700
[void][ZmCap]::SetForegroundWindow($h)

# ---- Detect the viewport origin rather than assuming it -------------------
# Grab one full-client frame and find the strongest edge that is sustained
# across the perpendicular axis. Sampling a median rejects scene geometry and
# text, while taking the global peak avoids mistaking an earlier editor toolbar
# separator for the viewport. A non-tools build (or an uncertain result) falls
# back to the full client instead of applying a speculative crop.
$r = New-Object ZmCap+RECT
[void][ZmCap]::GetClientRect($h, [ref]$r)
$cw = $r.R - $r.L; $ch = $r.B - $r.T
$tl = New-Object ZmCap+POINT
[void][ZmCap]::ClientToScreen($h, [ref]$tl)

$probe = New-Object System.Drawing.Bitmap $cw, $ch
$pg = [System.Drawing.Graphics]::FromImage($probe)
$pg.CopyFromScreen($tl.X, $tl.Y, 0, 0, (New-Object System.Drawing.Size($cw, $ch)))
$pg.Dispose()

$viewX = 0; $viewY = 0
$edgeSpan = 6
$sampleCount = 31
$minimumSustainedScore = 24.0

$xFirst = 32
$xLast = [int][math]::Min($cw * 0.45, $cw - $edgeSpan - 1)
$xSampleFirstY = [int]($ch * 0.22)
$xSampleLastY = $ch - 20
$xScores = [double[]]::new($cw)
$bestX = 0; $bestXScore = 0.0
for ($x = $xFirst; $x -le $xLast; $x++) {
	$values = [int[]]::new($sampleCount)
	for ($i = 0; $i -lt $sampleCount; $i++) {
		$sampleY = [int][math]::Round($xSampleFirstY + (($xSampleLastY - $xSampleFirstY) * $i / ($sampleCount - 1)))
		$a = $probe.GetPixel($x, $sampleY); $b = $probe.GetPixel($x + $edgeSpan, $sampleY)
		$values[$i] = [math]::Abs($a.R - $b.R) + [math]::Abs($a.G - $b.G) + [math]::Abs($a.B - $b.B)
	}
	[Array]::Sort($values)
	$xScores[$x] = $values[[int]($sampleCount / 2)]
	if ($xScores[$x] -gt $bestXScore) { $bestXScore = $xScores[$x]; $bestX = $x }
}

$hasXEdge = $bestXScore -ge $minimumSustainedScore
if ($hasXEdge) {
	# Comparing x with x+edgeSpan produces a short high-score band immediately
	# before the boundary. Its right edge + 1 is the first viewport pixel.
	$peakBandScore = [math]::Max($minimumSustainedScore, $bestXScore * 0.65)
	$edgeEnd = $bestX
	while ($edgeEnd -lt $xLast -and $xScores[$edgeEnd + 1] -ge $peakBandScore) { $edgeEnd++ }
	$viewX = $edgeEnd + 1
}

$yFirst = 24
$yLast = [int][math]::Min($ch * 0.28, $ch - $edgeSpan - 1)
$ySampleFirstX = if ($viewX -gt 0) {
	$viewX + [int][math]::Max(32, ($cw - $viewX) * 0.08)
} else {
	[int]($cw * 0.35)
}
$ySampleLastX = $cw - 24
$yScores = [double[]]::new($ch)
$bestY = 0; $bestYScore = 0.0
for ($y = $yFirst; $y -le $yLast; $y++) {
	$values = [int[]]::new($sampleCount)
	for ($i = 0; $i -lt $sampleCount; $i++) {
		$sampleX = [int][math]::Round($ySampleFirstX + (($ySampleLastX - $ySampleFirstX) * $i / ($sampleCount - 1)))
		$c = $probe.GetPixel($sampleX, $y); $d = $probe.GetPixel($sampleX, $y + $edgeSpan)
		$values[$i] = [math]::Abs($c.R - $d.R) + [math]::Abs($c.G - $d.G) + [math]::Abs($c.B - $d.B)
	}
	[Array]::Sort($values)
	$yScores[$y] = $values[[int]($sampleCount / 2)]
	if ($yScores[$y] -gt $bestYScore) { $bestYScore = $yScores[$y]; $bestY = $y }
}

$hasYEdge = $bestYScore -ge $minimumSustainedScore
if ($hasYEdge) {
	$peakBandScore = [math]::Max($minimumSustainedScore, $bestYScore * 0.65)
	$edgeEnd = $bestY
	while ($edgeEnd -lt $yLast -and $yScores[$edgeEnd + 1] -ge $peakBandScore) { $edgeEnd++ }
	$viewY = $edgeEnd + 1
}

if (-not ($hasXEdge -and $hasYEdge) -or $FullClient) { $viewX = 0; $viewY = 0 }
$probe.Dispose()
"[capture] viewport origin detected at ($viewX,$viewY) in a ${cw}x${ch} client (edge scores: X=$bestXScore, Y=$bestYScore)"

# ---- Capture loop ---------------------------------------------------------
$n = 0
while (-not $proc.HasExited -and $sw.Elapsed.TotalSeconds -lt $MaxSeconds) {
    try {
        $game.Refresh()
        if ($game.HasExited) { break }
        if (-not [ZmCap]::GetClientRect($h, [ref]$r)) { break }
        $vw = ($r.R - $r.L) - $viewX; $vh = ($r.B - $r.T) - $viewY
        if ($vw -gt 32 -and $vh -gt 32) {
            # ClientToScreen mutates the point in place. Reusing the previous
            # screen coordinate makes the capture origin drift every frame.
            $tl.X = 0; $tl.Y = 0
            [void][ZmCap]::ClientToScreen($h, [ref]$tl)
            $bmp = New-Object System.Drawing.Bitmap $vw, $vh
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($tl.X + $viewX, $tl.Y + $viewY, 0, 0, (New-Object System.Drawing.Size($vw, $vh)))
            $bmp.Save((Join-Path $dir ("f{0:D4}.png" -f $n)), [System.Drawing.Imaging.ImageFormat]::Png)
            $g.Dispose(); $bmp.Dispose()
            $n++
        }
    } catch { }
    Start-Sleep -Milliseconds $IntervalMs
}

if (-not $proc.HasExited) { $proc.WaitForExit(20000) }
Get-Process zenithmon -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$summary = Select-String -Path $log -Pattern "Summary:" -ErrorAction SilentlyContinue |
           Select-Object -Last 1 -ExpandProperty Line
"[capture] $n frames -> $dir"
"[capture] $summary"
