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
# Use -IntervalMs 60 or lower for anything short, and treat "I did not capture
# it" as unproven rather than absent.
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
    [string]$OutRoot = "Build\artifacts\shots"
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
[void][ZmCap]::SetWindowPos($h, [IntPtr]::Zero, 0, 0, $WindowW, $WindowH, 0x0040)
Start-Sleep -Milliseconds 700
[void][ZmCap]::SetForegroundWindow($h)

# ---- Detect the viewport origin rather than assuming it -------------------
# Grab one full-client frame, then walk in from the left/top until the flat
# editor grey stops. A tools build docks the view; a non-tools build has no
# chrome at all and this correctly resolves to (0,0).
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
$midY = [int]($ch * 0.5)
for ($x = 0; $x -lt [math]::Min(900, $cw - 8); $x++) {
    $a = $probe.GetPixel($x, $midY); $b = $probe.GetPixel($x + 6, $midY)
    if (([math]::Abs($a.R - $b.R) + [math]::Abs($a.G - $b.G) + [math]::Abs($a.B - $b.B)) -gt 14) { $viewX = $x; break }
}
$midX = [int]($cw * 0.75)
for ($y = 0; $y -lt [math]::Min(300, $ch - 8); $y++) {
    $c = $probe.GetPixel($midX, $y)
    if ($c.R -gt 70 -or $c.G -gt 70 -or $c.B -gt 70) { $viewY = $y; break }
}
$probe.Dispose()
"[capture] viewport origin detected at ($viewX,$viewY) in a ${cw}x${ch} client"

# ---- Capture loop ---------------------------------------------------------
$n = 0
while (-not $proc.HasExited -and $sw.Elapsed.TotalSeconds -lt $MaxSeconds) {
    try {
        $game.Refresh()
        if ($game.HasExited) { break }
        if (-not [ZmCap]::GetClientRect($h, [ref]$r)) { break }
        $vw = ($r.R - $r.L) - $viewX; $vh = ($r.B - $r.T) - $viewY
        if ($vw -gt 32 -and $vh -gt 32) {
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
