# run_unit_gate.ps1 -- boot a game exe and assert the engine
# unit-test baseline. CI's dedicated unit gate (engine-gate.yml); mirrors the
# tolerance logic of Tools/test_scaffold.ps1: a clean "<N> ran, <N> passed" OR
# the single known layout-sensitive flake (GraphComponent::
# RegistryWideNodeRoundTrip, task_726cc81d) as the SOLE failure.
#
# Usage:  pwsh ./Tools/run_unit_gate.ps1 -Exe <Null-config game exe> [-Baseline N]
#
# The exe MUST be a Null-backend build (Null_vs2022_*): headless is a build
# config now, not a flag. A Vulkan exe hangs in vkEnumeratePhysicalDevices on a
# GPU-less runner, and its unit count differs (it also compiles the Vulkan-only
# backend tests, which the Null build has no equivalent of).
# Exit:   0 = baseline met, 1 = anything else (missing exe, timeout, failures).
#
# ★ -TimeoutSec IS NOW A REAL HANG GUARD, NOT THE RUN LENGTH. The exe is launched
# with --exit-after-unit-tests and terminates ITSELF the moment Zenith_Init returns
# (the boot ZENITH_TEST batch lives inside it), so a healthy run exits on its own
# and costs only what the suite actually takes. Raising the timeout no longer costs
# anything on a passing run; it only widens the window before a genuinely wedged
# boot is killed.
#
# It did NOT used to work that way. The exe was launched with
# `--exit-after-frames 120`, which LOOKS like "run 120 frames then quit" but is
# parsed by Zenith_AutomatedTestRunner and consumed only inside its Stepping phase
# -- so with no --automated-test selection flag the runner is inactive, Tick()
# early-outs, and the flag does nothing whatsoever. The game therefore idled
# forever and the watchdog kill was the ONLY thing ending the process, which meant
# every run -- pass or fail -- burned the entire -TimeoutSec. zm-tests.yml passing
# 600 was 10 minutes per run, always.
#
# Two failure modes still surface as "no 'Unit tests complete' line", and neither
# is a slow suite. Check them BEFORE raising the timeout:
#   * an EMPTY log usually means STATUS_DLL_NOT_FOUND (exit 0xC0000135) -- the exe
#     died before main(). Fix with Repair-ZenithRuntimeDlls (Build/zenith_buildsystem.psm1);
#     `zenith test` heals automatically, this script does not.
#   * a truncated log means the boot genuinely wedged. That is what the guard is for.
# See ZM-D-163.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    # Engine unit count at boot. EXACT equality is asserted below, so this must
    # be bumped in the SAME commit as any change to the registered-test set --
    # adding tests reddens the gate exactly as loudly as deleting them.
    # 1271 -> 1284: +13 for Flux/Flux_SwapchainPolicy.Tests.inl (observed
    # 2026-08-04 on a Null_ Combat build).
    # 1284 -> 1289: +5 CommandLine ParseArgs characterization tests, added with
    # the complexity-gate remediation's table-driven rewrite of
    # Zenith_CommandLine::Parse (observed 2026-08-05 on a Null_ Combat build).
    # 1289 -> 1295: +6 for Flux/Terrain/Flux_Terrain.Tests.inl (the terrain G-buffer
    # pipeline variant 2x2, added when the Wireframe checkbox was found dead under
    # TAA) (observed 2026-08-06 on a Null_ Combat build).
    # 1295 -> 1300: +5 more in the same file -- the FluxTerrainSourceGrid suite for
    # Flux/Terrain/Flux_TerrainSourceGrid.h, added when the terrain exporter was found
    # to bake INCOMPLETE chunks at the positive grid border (127 of 4096 silently
    # dropped from LOW LOD and physics) (observed 2026-08-06 on a Null_ Combat build).
    # 1300 -> 1320: +20 for the FluxGrassTypes suite in Flux/Vegetation/
    # Flux_Grass.Tests.inl -- pure tile-selection/wind/map-sampling/GPU-packing
    # functions (Flux_GrassTypes.h) + pinned integer-hash vectors that the
    # Shaders/Common/Noise.slang GPU mirror is held to; GPU-grass overhaul Phase 1
    # (observed 2026-08-06 on a Null_ Combat build).
    # 1320 -> 1322: +2 FluxBufferReadback units (Flux/Flux_BufferReadback.Tests.inl,
    # hosted by Flux_MaterialTable.cpp) pinning DownloadBufferData's headless
    # zero-fill + exact-size contract; GPU-grass overhaul Phase 2
    # (observed 2026-08-06 on a Null_ Combat build).
    # 1322 -> 1326: +4 TerrainEditor GrassType units (hard-edged dab, 1-byte-texel
    # stroke undo round-trip, GrassType.ztxtr round-trip + absent-file fallback,
    # sculpt-notify latch); GPU-grass overhaul Phase 3
    # (observed 2026-08-06 on a Null_ Combat build).
    # 1326 -> 1337: +11 net for the GPU-grass swap -- 3 member-poking legacy grass
    # tests deleted, 14 added: 4 shader-mirror suites (Bezier/blade-vertex-table/
    # Voronoi-clump/type-gather), 6 FluxGrassImpl (build rejection/all-zero-type/
    # unbuilt-zero/reset-idempotence/mover-cap/headless-readback), 4
    # FluxGrassTypeTable (defaults/clamps incl. bit-classified NaN+Inf replacement/
    # serialize round-trip/fail-safe reject)
    # (observed 2026-08-06 on a Null_ Combat build).
    # 1337 -> 1340: +3 FluxGrassImpl shadow-casting units (enable-flag truth table,
    # active-slot-mask exclusion, grass-before-shadows registration order);
    # GPU-grass overhaul Phase 5 (observed 2026-08-06 on a Null_ Combat build).
    # 1340 -> 1344: +4 displacement re-anchor units (sub-texel anchor stability,
    # one-texel boundary steps, integral texel shift, frame-rate-independent
    # decay); GPU-grass overhaul Phase 6 (observed 2026-08-06 on a Null_ Combat
    # build).
    # 1344 -> 1350: +6 grass-types authoring units (name->field mapping incl.
    # unknown-name rejection, working-copy isolation, automation family
    # end-to-end, save-writes-asset-and-applies, enum-block contiguity);
    # GPU-grass overhaul Phase 7 (observed 2026-08-06 on a Null_ Combat build).
    # 1350 -> 1352: +2 AI debug-draw routing units (master-toggle short-circuit,
    # safe with no AI content) added when the AI/* debug-variable subtree was
    # wired up -- Zenith_AIDebugVariables::Initialise() had no call site, so none
    # of its 20 toggles ever reached the panel, and DebugDrawAllSquads asserted
    # on an un-Initialise()d manager it is now called against every frame
    # (observed 2026-08-07 on a Null_ Combat build).
    # 1352 -> 1367: +15 vertex-codec units (half2/half4/snorm16x4/unorm16x2/
    # unorm8x4 round-trips, unorm8 rounding + clamp, uint8x4 lane order, SNORM10
    # bit-identity against the transcribed legacy packer + pinned words +
    # round-trip, PosQuant box round-trip + degenerate axis, bone-weight
    # renormalisation + zero input, bone-index sentinel/ceiling); compressed-
    # vertex Phase 1 (observed 2026-08-07 on a Null_ Combat build).
    # 1367 -> 1377: +10 reflection vertex-input units (tight-pack offsets/strides in
    # declaration order, independent per-binding packing, empty table, DataStream
    # round-trip of the v6 table + the empty-table shape, adopt-once merge, [VtxFmt]
    # string vocabulary, type inference from the declared field, override validation,
    # storage family/lane table); compressed-vertex Phase 2 T2.a -- reflection sidecar
    # v6 (observed 2026-08-07 on a Null_ Combat build).
    # 1377 -> 1388: +11 baked-vertex-layout units -- 8 Flux_VertexLayoutDesc
    # (element-wise equality over distinct arrays incl. in a constant expression,
    # offset/type/semantic-index/stride/count drift, the canonical empty layout,
    # semantic-vocabulary round-trip) + 3 Codegen emission (table content with
    # STORAGE formats, the null layout, hard failure on an unnamed semantic);
    # compressed-vertex Phase 2 T2.b -- codegen emission + [VtxFmt] annotations
    # (observed 2026-08-07 on a Null_ Combat build).
    # 1388 -> 1400: +12 vertex-layout-validation units (Flux_VertexLayoutValidation
    # .Tests.inl) driving the boot tripwire's pure comparator through every branch
    # real boots never take: the OK path, both null/empty no-input spellings,
    # COUNT / per-binding STRIDE / ELEMENT (offset, type, semantic, semantic
    # index, binding) / unknown-SEMANTIC mismatches; compressed-vertex Phase 2
    # T2.d review hardening (observed 2026-08-08 on a Null_ Combat build).
    # 1400 -> 1415: +15 vertex-packer units (Flux_VertexPacker.Tests.inl) — the
    # whole-layout byte contract over two mixed-format tables, the canonical
    # attribute defaults, (semantic, index) source keying, the 4-lane TANGENT's
    # derived handedness both signs + an authored w winning, SNORM10
    # pre-normalisation + its zero-length fallback, the lane pad/truncate rules,
    # the POSITION quant box, stride-advance across 3 vertices, the no-op shapes,
    # and binding-1 skipping; compressed-vertex Phase 3 T3.a — the packer
    # (observed 2026-08-08 on a Null_ Combat build).
    # 1415 -> 1419: +4 NET in the rewritten mesh-family suite
    # (Flux_VertexInterleave.Tests.inl): 3 old hand-layout tests became 7 —
    # two byte-for-byte memcmp goldens (the static pack and the skin-input build,
    # each against a FROZEN transcription of the interleave loop they replaced,
    # over 7 adversarial meshes: missing attributes, arrays one entry short of the
    # vertex count, degenerate frames, a short bone-weight array, negative zero),
    # the ported layout/defaults expectations, and the bind-pose position override;
    # compressed-vertex Phase 3 T3.b — mesh-family replacement (observed
    # 2026-08-08 on a Null_ Combat build).
    # 1419 -> 1424: +5 NET across the two suites re-pinned by the compression
    # flip. Flux_VertexInterleave.Tests.inl +3: its two memcmp goldens now compare
    # against an INDEPENDENT transcription through Flux_VertexCodec instead of the
    # frozen float32 interleave loops (Phase 4 changes those bytes by design), plus
    # the skin-input/static prefix identity, the mirrored-frame bitangent SIGN, and
    # the procedural mesh-pipeline pack. Flux_Skinning.Tests.inl +2: the packed
    # skin-vertex codec round trip and the one-byte bone sentinel; compressed-vertex
    # Phase 4 T4.a — the mesh compression flip (observed 2026-08-08 on a Null_
    # Combat build).
    # 1424 -> 1431: +7 from the T4.a adversarial-review fix pass. Flux_VertexCodec
    # .Tests.inl +4 (hand-derived half word pins; the frozen Slang-transcription
    # half parity incl. the pinned tie divergence; the bit-classified
    # positive-finite branches; over-unity bone-weight pre-scale).
    # Flux_VertexPacker +1 (half-range position guard, capture-scoped).
    # Flux_VertexInterleave +1 (skin-output encoder vs static packer byte
    # identity). Flux_Skinning +1 (negative-determinant blend flips the bitangent
    # sign). Observed 2026-08-08 on a Null_ Combat build.
    # 1431 -> 1433: +2 Flux_VertexCodec.Tests.inl units pinning the GPU<->CPU
    # agreement of Flux_DequantPosition (Shaders/Common/VertexFormats.slang) — a
    # frozen transcription of the Slang function plus the fixed-function
    # VK_FORMAT_R16G16B16A16_SNORM fetch conversion, swept against the CPU codec
    # over box corners / quantum boundaries / three boxes, and the out-of-box
    # clamp. It gained its first caller with the terrain compression flip;
    # compressed-vertex Phase 5 T5.a (observed 2026-08-08 on a Null_ Combat build).
    # 1433 -> 1437: +4 Flux_Terrain.Tests.inl units from the T5.a adversarial-
    # review fix pass — the TerrainConstants CB fill vs the authored box (values,
    # not just layout), bridge writes landing at the SHADER's reflected offsets
    # (0xCD-sentinel decode), decode->re-encode word idempotence (the sculpt/carve
    # seam-safety property), and the UV write/read round trip incl. the integer
    # snap the sculpt hook stands on (observed 2026-08-08 on a Null_ Combat build).
    [int]$Baseline = 1437,
    [int]$TimeoutSec = 180,
    [string]$LogPath = ""
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Exe)) {
    Write-Error "[unit_gate] executable not found: $Exe"
    exit 1
}

if ($LogPath -eq "") {
    $root = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'Build/artifacts' }
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $LogPath = Join-Path $root 'unit_gate_boot.log'
}

# Boot far enough for units-at-boot to complete, then exit -- which the exe now
# does BY ITSELF via --exit-after-unit-tests. The watchdog below stays as a guard
# against a genuinely wedged boot; on a healthy run it never fires.
#
# Deliberately NO --skip-tool-exports: the engine unit suite includes asset-export
# tests (ProceduralTree::*, StickFigure*) that reload GenerateTestAssets output from
# disk. Zenith/Assets/ is gitignored, so on a from-scratch CI checkout that output
# only exists if this boot generates it; --skip-tool-exports left those tests loading
# missing assets and wedging the units-at-boot gate. The exports are CPU-only and
# backend-independent. This script is invoked ONLY by engine-gate.yml, so the change is
# scoped here -- the game test gates (cb/dp-tests) keep --skip-tool-exports via their
# own harness (they run --skip-unit-tests, so they need no generated engine assets).
Write-Host "[unit_gate] Booting $Exe (baseline $Baseline, timeout ${TimeoutSec}s)..." -ForegroundColor Cyan
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = (Resolve-Path $Exe).Path
$psi.Arguments = '--exit-after-unit-tests'
$psi.WorkingDirectory = Split-Path (Resolve-Path $Exe).Path
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
$stdout = $proc.StandardOutput.ReadToEndAsync()
$stderr = $proc.StandardError.ReadToEndAsync()
if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
    # A healthy run exits on its own (--exit-after-unit-tests). Reaching here means
    # the boot wedged, or died before main() -- see the DLL note in the header.
    Write-Host "[unit_gate] timeout after ${TimeoutSec}s; killing (boot did not self-terminate)" -ForegroundColor Yellow
    try { $proc.Kill() } catch { }
}
$outText = $stdout.Result + "`n" + $stderr.Result
[System.IO.File]::WriteAllText($LogPath, $outText, (New-Object System.Text.UTF8Encoding($false)))

$unitsLine = ($outText -split "`n" | Where-Object { $_ -match 'Unit tests complete' } | Select-Object -Last 1)
if (-not $unitsLine) {
    Write-Error "[unit_gate] no 'Unit tests complete' line in boot output (log: $LogPath)"
    exit 1
}
Write-Host "[unit_gate] $($unitsLine.Trim())"

# Parse "<ran> ran, <passed> passed, <failed> failed[, <skipped> skipped]". The
# harness reports ran == total-registered (a ZENITH_SKIP moves a test from the
# passed bucket to the skipped bucket, never off the ran count). Deliberate skips
# are expected and are never a failure: RegistryWideNodeRoundTrip is quarantined
# (task_726cc81d intermittent heap corruption), so a clean boot is
# "<N> ran, <N-1> passed, 0 failed, 1 skipped".
if ($unitsLine -notmatch '(\d+)\s+ran,\s+(\d+)\s+passed,\s+(\d+)\s+failed(?:,\s+(\d+)\s+skipped)?') {
    Write-Error "[unit_gate] could not parse the tally from '$($unitsLine.Trim())' (log: $LogPath)"
    exit 1
}
$ran     = [int]$Matches[1]
$passed  = [int]$Matches[2]
$failed  = [int]$Matches[3]
$skipped = if ($Matches[4]) { [int]$Matches[4] } else { 0 }
$skipNote = if ($skipped -gt 0) { " ($skipped skipped)" } else { "" }

# The full registered suite must have run (guards against tests silently vanishing).
$fullSuite = ($ran -eq $Baseline)

if ($fullSuite -and $failed -eq 0) {
    Write-Host "[unit_gate] PASS ($passed/$Baseline passed, 0 failed$skipNote)" -ForegroundColor Green
    exit 0
}
Write-Error "[unit_gate] baseline NOT met (wanted $Baseline ran, 0 failed; got '$($unitsLine.Trim())'; log: $LogPath)"
exit 1
