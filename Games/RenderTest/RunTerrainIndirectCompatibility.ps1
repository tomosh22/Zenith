param(
	[switch]$NoBuild,
	[switch]$SkipABCompare,
	[int]$TimeoutSeconds = 600
)

$ErrorActionPreference = "Stop"

# ============================================================================
# RunTerrainIndirectCompatibility.ps1 — Phase 7 three-process wrapper for
# the TerrainIndirectCompatibility automated test (the graphics A/B gate).
#
# Launches the test in THREE separate processes with different
# --indirect-count-mode overrides (auto, padded, single), each in its own
# process so worker recording never sees a mutable backend mode. Each arm
# writes terrain_<mode>.tga + result_<mode>.json + stdout/stderr logs to
# Build/artifacts/rendertest/terrain_indirect/.
#
# After all three finish, the wrapper asserts:
#   * the test is registered in --list-automated-tests;
#   * each process exited 0, passed=true, skipped=false;
#   * no log contains VK ERROR / VUID- / validation / assertion /
#     device loss / retired "streaming disabled" wording;
#   * each TGA artifact exists, and its terrain-only debug sentinel passes
#     the test's fixed non-vacuity floor;
#   * the TELEMETRY line names the tier selected from actual capabilities +
#     request, and exactly that one tier has a non-zero recorder counter;
#   * (unless -SkipABCompare) RGB-only mean/p99.9/max delta inside the logged inset
#     editor viewport between auto/padded and auto/single stays within the
#     checked-in frozen budgets (alpha and UI chrome are excluded), and the
#     terrain-sentinel masks must have low symmetric difference / high IoU.
#
# A/B BUDGETS: the limits are CHECKED-IN constants (not self-derived from
# the first run). The plan forbids deriving the threshold from the fallback
# candidate under test. A clean checkout's first run therefore compares
# against the same frozen numbers CI uses, not against itself.
#
# Usage:
#   pwsh Games\RenderTest\RunTerrainIndirectCompatibility.ps1
#   pwsh Games\RenderTest\RunTerrainIndirectCompatibility.ps1 -NoBuild
# ============================================================================

# ---- Checked-in frozen A/B budgets (2026-08-13 Vulkan baseline) ----
# These are FROZEN limits, NOT derived from the fallback candidate. The mean
# and p99.9 budgets cover per-channel RGB absolute differences normalized to
# [0,1]. The sentinel-mask gates directly compare terrain silhouettes, so a
# localized hole cannot hide in unrelated viewport pixels. TAA is forced off;
# the remaining allowance covers process-to-process vegetation/raster edges.
# Observed auto-vs-padded: mean=.005407, p99.9=.333333, max=.490196,
# maskXor=.015744, IoU=.941559. Auto-vs-single: .001697, .227451, .454902,
# .004201, .984061. Alpha never contributes.
$fAB_MEAN_BUDGET = 0.02
$fAB_P999_BUDGET = 0.40
$fAB_MAX_BUDGET  = 0.75
$fAS_MEAN_BUDGET  = 0.02
$fAS_P999_BUDGET  = 0.40
$fAS_MAX_BUDGET   = 0.75
$fMASK_XOR_BUDGET = 0.03
$fMASK_IOU_FLOOR  = 0.90
$iSENTINEL_MIN_RB = 128
$iSENTINEL_MAX_G = 64
$iSENTINEL_MAX_RB_DELTA = 32

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Solution = Join-Path $Root "Games\RenderTest\rendertest_win64.sln"
$Exe = Join-Path $Root "Games\RenderTest\Build\output\win64\vulkan_vs2022_debug_win64_true\rendertest.exe"

function Find-MSBuild {
	$candidates = @(
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
	)
	foreach ($c in $candidates) { if (Test-Path $c) { return $c } }
	throw "MSBuild.exe not found."
}

if (-not $NoBuild) {
	$msbuild = Find-MSBuild
	& $msbuild $Solution /t:RenderTest /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount -nologo
	if ($LASTEXITCODE -ne 0) { throw "RenderTest Vulkan_True build failed (exit $LASTEXITCODE)" }
}
if (-not (Test-Path $Exe)) { throw "RenderTest Vulkan exe missing: $Exe" }

$OutputDir = Split-Path $Exe
$SiblingRuntimeDirs = @((Join-Path $Root "FluxCompiler\output\win64\vulkan_vs2022_debug_win64_true"))
foreach ($d in $SiblingRuntimeDirs) {
	if (Test-Path $d) {
		Get-ChildItem $d -Filter "*.dll" | ForEach-Object {
			$t = Join-Path $OutputDir $_.Name
			if (-not (Test-Path $t)) { Copy-Item -Force $_.FullName $t }
		}
	}
}

$DebugCrtDir = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\2022" -Recurse -Filter "msvcp140d.dll" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -match "debug_nonredist\\x64\\Microsoft\.VC143\.DebugCRT" } |
	Select-Object -First 1 | ForEach-Object { Split-Path $_.FullName }
$DebugUcrtDir = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter "ucrtbased.dll" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -match "\\x64\\ucrt\\" } |
	Sort-Object FullName -Descending | Select-Object -First 1 | ForEach-Object { Split-Path $_.FullName }
if ($DebugCrtDir)  { $env:PATH = "$DebugCrtDir;$env:PATH" }
if ($DebugUcrtDir) { $env:PATH = "$DebugUcrtDir;$env:PATH" }

$ArtifactDir = Join-Path $Root "Build\artifacts\rendertest\terrain_indirect"
New-Item -ItemType Directory -Force $ArtifactDir | Out-Null

# 1) Discovery
$listArgs = @("--list-automated-tests", "--skip-unit-tests", "--skip-tool-exports")
Write-Host "[TerrainIndirectCompatibility] Verifying test is registered"
$listLog = Join-Path $ArtifactDir "list.log"
& $Exe @listArgs *>&1 | Tee-Object -FilePath $listLog | Out-Null
if ($LASTEXITCODE -ne 0) { throw "--list-automated-tests exited $LASTEXITCODE" }
$listText = Get-Content $listLog -Raw
if ($listText -notmatch "TerrainIndirectCompatibility") {
	throw "TerrainIndirectCompatibility is NOT in --list-automated-tests"
}
Write-Host "[TerrainIndirectCompatibility] OK: registered"

function Join-ProcessArguments([string[]]$Arguments) {
	$quoted = foreach ($argument in $Arguments) {
		if ($argument.IndexOf('"') -ge 0) { throw "process argument contains an unsupported quote: $argument" }
		'"' + $argument + '"'
	}
	return ($quoted -join ' ')
}

# 2) Helper: run one arm in its own process.
function Invoke-ModeArm {
	param([string]$Mode, [string]$ResultsPath, [string]$StdoutPath, [string]$StderrPath)
	$armArgs = @(
		"--automated-test", "TerrainIndirectCompatibility",
		"--indirect-count-mode=$Mode",
		"--skip-unit-tests",
		"--skip-tool-exports",
		"--test-results", $ResultsPath,
		"--fixed-dt", "0.01666",
		"--taa=0"
	)
	Write-Host "[TerrainIndirectCompatibility] Running arm mode=${Mode}"
	$process = Start-Process -FilePath $Exe -ArgumentList (Join-ProcessArguments $armArgs) `
		-WorkingDirectory $OutputDir `
		-RedirectStandardOutput $StdoutPath `
		-RedirectStandardError $StderrPath -PassThru
	if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
		$process.Kill()
		throw "TerrainIndirectCompatibility ${Mode} timed out after $TimeoutSeconds seconds"
	}
	$process.WaitForExit()
	$exitCode = $process.ExitCode
	$stdout = Get-Content $StdoutPath -Raw
	$stderr = Get-Content $StderrPath -Raw
	$combined = $stdout + "`n" + $stderr

	# Hard-fail markers
	$failMarkers = @("VK ERROR", "VUID-", "Validation Error", "Synchronization-Violation",
		"Zenith_Assert", "Assertion failed:", "device lost", "VK_ERROR_DEVICE_LOST", "FAILED_CLOSED", "indirect-buffer bounds",
		"terrain will not render", "streaming is disabled")
	foreach ($m in $failMarkers) {
		if ($combined -match [regex]::Escape($m)) {
			throw "TerrainIndirectCompatibility ${Mode} reported failure marker '${m}' — see ${StdoutPath}"
		}
	}

	# Result JSON: passed=true, skipped=false, exit 0.
	if (-not (Test-Path $ResultsPath)) {
		throw "TerrainIndirectCompatibility ${Mode} results JSON missing: ${ResultsPath}"
	}
	$json = Get-Content $ResultsPath -Raw | ConvertFrom-Json
	if (-not $json.passed) {
		throw "TerrainIndirectCompatibility ${Mode} FAILED (passed=false in ${ResultsPath})"
	}
	if ($json.skipped) {
		throw "TerrainIndirectCompatibility ${Mode} was SKIPPED (skipped=true in ${ResultsPath})"
	}
	if ($exitCode -ne 0) {
		throw "TerrainIndirectCompatibility ${Mode} exited with code $exitCode"
	}

	# Screenshot artifact exists + non-trivial size.
	$shotPath = Join-Path $ArtifactDir "terrain_${Mode}.tga"
	if (-not (Test-Path $shotPath)) {
		throw "TerrainIndirectCompatibility ${Mode} screenshot artifact missing: ${shotPath}"
	}
	$shotLen = (Get-Item $shotPath).Length
	if ($shotLen -lt 1024) {
		throw "TerrainIndirectCompatibility ${Mode} screenshot too small (${shotLen} bytes): ${shotPath}"
	}

	# Scrape TELEMETRY line (per-mode, the test logs one) to verify the
	# expected tier RAN — not just that the CLI flag was set.
	$telemLine = ($stdout -split "`n" | Where-Object { $_ -match "TELEMETRY mode=${Mode}" } | Select-Object -First 1)
	$sentinel  = ($stdout -split "`n" | Where-Object { $_ -match "SENTINEL mode=${Mode} " } | Select-Object -First 1)
	$cropLine  = ($stdout -split "`n" | Where-Object { $_ -match "CROP mode=${Mode} " } | Select-Object -First 1)

	# Parse the telemetry nativeCount and paddedSingle counters from the line.
	$telemNative = -1
	$telemMulti  = -1
	$telemSingle = -1
	$telemExpected = ""
	if ($telemLine) {
		if ($telemLine -match "expected=([A-Z_]+)") { $telemExpected = $Matches[1] }
		if ($telemLine -match "native=(\d+)")      { $telemNative = [int]$Matches[1] }
		if ($telemLine -match "paddedMulti=(\d+)")  { $telemMulti  = [int]$Matches[1] }
		if ($telemLine -match "paddedSingle=(\d+)") { $telemSingle = [int]$Matches[1] }
	}
	if (-not $telemLine -or -not $telemExpected -or
		$telemNative -lt 0 -or $telemMulti -lt 0 -or $telemSingle -lt 0) {
		throw "TerrainIndirectCompatibility ${Mode}: missing/incomplete TELEMETRY line"
	}
	if (-not $sentinel) {
		throw "TerrainIndirectCompatibility ${Mode}: missing terrain SENTINEL evidence"
	}

	[int[]]$crop = @()
	if ($cropLine -and $cropLine -match "x0=(\d+) y0=(\d+) x1=(\d+) y1=(\d+) width=(\d+) height=(\d+)") {
		$crop = [int[]]@(
			[int]$Matches[1], [int]$Matches[2], [int]$Matches[3],
			[int]$Matches[4], [int]$Matches[5], [int]$Matches[6])
	}
	if ($crop.Count -ne 6 -or $crop[2] -le $crop[0] -or $crop[3] -le $crop[1]) {
		throw "TerrainIndirectCompatibility ${Mode}: missing/invalid viewport CROP metadata"
	}

	Write-Host "[TerrainIndirectCompatibility] ${Mode}: exit=0 passed=true skipped=false shot=${shotLen}B"
	Write-Host "  $($sentinel.Trim())"
	Write-Host "  $($cropLine.Trim())"
	if ($telemLine) { Write-Host "  $($telemLine.Trim())" }

	return @{ Sentinel = $sentinel; ShotPath = $shotPath; Telemetry = $telemLine;
	          TelemExpected = $telemExpected; TelemNative = $telemNative;
	          TelemMulti = $telemMulti; TelemSingle = $telemSingle; Crop = $crop }
}

$autoResultsPath   = Join-Path $ArtifactDir "result_auto.json"
$paddedResultsPath = Join-Path $ArtifactDir "result_padded.json"
$singleResultsPath = Join-Path $ArtifactDir "result_single.json"
$autoStdout   = Join-Path $ArtifactDir "auto_stdout.log"
$autoStderr   = Join-Path $ArtifactDir "auto_stderr.log"
$paddedStdout = Join-Path $ArtifactDir "padded_stdout.log"
$paddedStderr = Join-Path $ArtifactDir "padded_stderr.log"
$singleStdout = Join-Path $ArtifactDir "single_stdout.log"
$singleStderr = Join-Path $ArtifactDir "single_stderr.log"
foreach ($p in @($autoResultsPath, $paddedResultsPath, $singleResultsPath,
                 $autoStdout, $autoStderr, $paddedStdout, $paddedStderr,
                 $singleStdout, $singleStderr)) {
	Remove-Item -Force -ErrorAction SilentlyContinue $p
}

# 3) Run all three arms.
$autoInfo   = Invoke-ModeArm -Mode "auto"   -ResultsPath $autoResultsPath   -StdoutPath $autoStdout   -StderrPath $autoStderr
$paddedInfo = Invoke-ModeArm -Mode "padded" -ResultsPath $paddedResultsPath -StdoutPath $paddedStdout -StderrPath $paddedStderr
$singleInfo = Invoke-ModeArm -Mode "single" -ResultsPath $singleResultsPath -StdoutPath $singleStdout -StderrPath $singleStderr

# 3a) Telemetry: C++ computes the expected tier from the actual usable device
# capabilities, TOTAL_CHUNKS request, ZERO_PADDED policy, and immutable CLI
# request. Independently require exactly that logged tier here as well.
#
# The A/B gate is only evidence if the three arms ran three DIFFERENT tiers.
# On a device without usable native count, auto legitimately resolves to
# PADDED_MULTI — and then auto-vs-padded compares the fallback against ITSELF
# and passes vacuously with two byte-identical captures. Likewise, a device
# without multiDrawIndirect makes the padded arm resolve to PADDED_SINGLE,
# which is the same tier the single arm forces. So this wrapper hard-requires
# the exact per-arm tier rather than accepting whatever the selector produced:
#   auto -> NATIVE_COUNT, padded -> PADDED_MULTI, single -> PADDED_SINGLE.
# A device that cannot supply all three is not a valid host for this gate and
# must fail loudly here; the C++ test's own selector-derived check still covers
# the "this arm ran the tier the selector chose" half.
$RequiredTierByMode = @{
	"auto"   = "NATIVE_COUNT"
	"padded" = "PADDED_MULTI"
	"single" = "PADDED_SINGLE"
}
function Assert-ExactTier {
	param([string]$Mode, $Info)
	$expected = $Info.TelemExpected
	$n = $Info.TelemNative
	$m = $Info.TelemMulti
	$s = $Info.TelemSingle
	$exact = $false
	switch ($expected) {
		"NATIVE_COUNT"  { $exact = ($n -gt 0 -and $m -eq 0 -and $s -eq 0) }
		"PADDED_MULTI"  { $exact = ($n -eq 0 -and $m -gt 0 -and $s -eq 0) }
		"PADDED_SINGLE" { $exact = ($n -eq 0 -and $m -eq 0 -and $s -gt 0) }
		default { throw "TerrainIndirectCompatibility ${Mode}: illegal expected tier '${expected}'" }
	}
	if (-not $exact) {
		throw "TerrainIndirectCompatibility ${Mode}: expected exactly ${expected}, got native=${n} paddedMulti=${m} paddedSingle=${s}"
	}
	$required = $RequiredTierByMode[$Mode]
	if ($expected -ne $required) {
		throw ("TerrainIndirectCompatibility ${Mode}: this gate requires the ${required} tier, but the arm ran ${expected}. " +
			"The three arms must run three DISTINCT tiers or the A/B comparison degenerates into comparing a fallback against itself. " +
			"auto=NATIVE_COUNT needs usable vkCmdDrawIndexedIndirectCount within maxDrawIndirectCount; " +
			"padded=PADDED_MULTI needs multiDrawIndirect. This host satisfies neither requirement for the ${Mode} arm, " +
			"so it cannot produce the evidence this gate exists to collect.")
	}
}
Assert-ExactTier -Mode "auto"   -Info $autoInfo
Assert-ExactTier -Mode "padded" -Info $paddedInfo
Assert-ExactTier -Mode "single" -Info $singleInfo
$distinctTiers = @($autoInfo.TelemExpected, $paddedInfo.TelemExpected, $singleInfo.TelemExpected) | Select-Object -Unique
if ($distinctTiers.Count -ne 3) {
	throw "TerrainIndirectCompatibility: the three arms did not run three distinct tiers ($($distinctTiers -join ', ')) — the A/B comparison would compare a fallback against itself"
}
Write-Host "[TerrainIndirectCompatibility] Telemetry: all arms ran their required distinct tiers (auto=NATIVE_COUNT, padded=PADDED_MULTI, single=PADDED_SINGLE)"

# 4) A/B pixel-level comparison against CHECKED-IN frozen budgets.
if (-not $SkipABCompare) {
	function Compare-Shots {
		param([string]$A, [string]$B, [string]$Label, [int[]]$CropA, [int[]]$CropB)
		if (-not (Test-Path $A) -or -not (Test-Path $B)) {
			throw "[TerrainIndirectCompatibility] ${Label}: missing TGA artifact (${A} / ${B})"
		}
		$bytesA = [System.IO.File]::ReadAllBytes($A)
		$bytesB = [System.IO.File]::ReadAllBytes($B)
		if ($bytesA.Length -lt 18 -or $bytesB.Length -lt 18 -or
			$bytesA[1] -ne 0 -or $bytesB[1] -ne 0 -or
			$bytesA[2] -ne 2 -or $bytesB[2] -ne 2 -or
			$bytesA[16] -ne 32 -or $bytesB[16] -ne 32) {
			throw "[TerrainIndirectCompatibility] ${Label}: expected uncompressed 32-bit true-colour TGA artifacts"
		}
		$widthA  = [int]$bytesA[12] + ([int]$bytesA[13] -shl 8)
		$heightA = [int]$bytesA[14] + ([int]$bytesA[15] -shl 8)
		$widthB  = [int]$bytesB[12] + ([int]$bytesB[13] -shl 8)
		$heightB = [int]$bytesB[14] + ([int]$bytesB[15] -shl 8)
		if ($widthA -ne $widthB -or $heightA -ne $heightB -or
			$CropA.Count -ne 6 -or $CropB.Count -ne 6) {
			throw "[TerrainIndirectCompatibility] ${Label}: image dimensions/crop metadata differ"
		}
		for ($i = 0; $i -lt 6; ++$i) {
			if ($CropA[$i] -ne $CropB[$i]) {
				throw "[TerrainIndirectCompatibility] ${Label}: viewport crops differ between arms"
			}
		}
		if ($CropA[4] -ne $widthA -or $CropA[5] -ne $heightA -or
			$CropA[0] -lt 0 -or $CropA[1] -lt 0 -or
			$CropA[2] -gt $widthA -or $CropA[3] -gt $heightA -or
			$CropA[2] -le $CropA[0] -or $CropA[3] -le $CropA[1]) {
			throw "[TerrainIndirectCompatibility] ${Label}: logged viewport crop is outside the TGA"
		}

		$offsetA = 18 + [int]$bytesA[0]
		$offsetB = 18 + [int]$bytesB[0]
		$requiredA = $offsetA + (4 * $widthA * $heightA)
		$requiredB = $offsetB + (4 * $widthB * $heightB)
		if ($bytesA.Length -lt $requiredA -or $bytesB.Length -lt $requiredB) {
			throw "[TerrainIndirectCompatibility] ${Label}: truncated TGA pixel payload"
		}

		$uSum = 0L
		$uMax = 0
		$uChannels = 0L
		$histogram = [long[]]::new(256)
		$uMaskXor = 0L
		$uMaskIntersection = 0L
		$uMaskUnion = 0L
		for ($y = $CropA[1]; $y -lt $CropA[3]; ++$y) {
			for ($x = $CropA[0]; $x -lt $CropA[2]; ++$x) {
				$pixelA = $offsetA + (4 * (($y * $widthA) + $x))
				$pixelB = $offsetB + (4 * (($y * $widthB) + $x))
				$bA = [int]$bytesA[$pixelA]
				$gA = [int]$bytesA[$pixelA + 1]
				$rA = [int]$bytesA[$pixelA + 2]
				$bB = [int]$bytesB[$pixelB]
				$gB = [int]$bytesB[$pixelB + 1]
				$rB = [int]$bytesB[$pixelB + 2]
				$maskA = $rA -ge $iSENTINEL_MIN_RB -and $bA -ge $iSENTINEL_MIN_RB -and
					$gA -le $iSENTINEL_MAX_G -and [math]::Abs($rA - $bA) -le $iSENTINEL_MAX_RB_DELTA
				$maskB = $rB -ge $iSENTINEL_MIN_RB -and $bB -ge $iSENTINEL_MIN_RB -and
					$gB -le $iSENTINEL_MAX_G -and [math]::Abs($rB - $bB) -le $iSENTINEL_MAX_RB_DELTA
				if ($maskA -or $maskB) { ++$uMaskUnion }
				if ($maskA -and $maskB) { ++$uMaskIntersection }
				if ($maskA -xor $maskB) { ++$uMaskXor }
				# Compare B, G, R only. Alpha is intentionally excluded: a
				# constant opaque byte must not dilute real colour differences.
				for ($c = 0; $c -lt 3; ++$c) {
					$d = [math]::Abs([int]$bytesA[$pixelA + $c] - [int]$bytesB[$pixelB + $c])
					$uSum += $d
					++$histogram[$d]
					++$uChannels
					if ($d -gt $uMax) { $uMax = $d }
				}
			}
		}
		$fMean = [double]$uSum / [double]$uChannels / 255.0
		$fMax  = [double]$uMax / 255.0
		$uP999Target = [long][math]::Ceiling([double]$uChannels * 0.999)
		$uCumulative = 0L
		$iP999 = 255
		for ($i = 0; $i -lt 256; ++$i) {
			$uCumulative += $histogram[$i]
			if ($uCumulative -ge $uP999Target) { $iP999 = $i; break }
		}
		$fP999 = [double]$iP999 / 255.0
		$uPixels = [long](($CropA[2] - $CropA[0]) * ($CropA[3] - $CropA[1]))
		$fMaskXor = [double]$uMaskXor / [double]$uPixels
		$fMaskIoU = if ($uMaskUnion -gt 0) {
			[double]$uMaskIntersection / [double]$uMaskUnion
		} else { 0.0 }
		Write-Host "[TerrainIndirectCompatibility] ${Label} viewport compare: meanDelta=$($fMean.ToString('F6')) p99.9Delta=$($fP999.ToString('F6')) maxDelta=$($fMax.ToString('F6')) maskXor=$($fMaskXor.ToString('F6')) maskIoU=$($fMaskIoU.ToString('F6')) crop=[$($CropA[0]),$($CropA[1]))-[$($CropA[2]),$($CropA[3]))"
		return @{ meanDelta = $fMean; p999Delta = $fP999; maxDelta = $fMax;
		          maskXor = $fMaskXor; maskIoU = $fMaskIoU }
	}

	$autoShot   = $autoInfo.ShotPath
	$paddedShot = $paddedInfo.ShotPath
	$singleShot = $singleInfo.ShotPath

	# auto vs padded — main A/B gate.
	$abResult = Compare-Shots -A $autoShot -B $paddedShot -Label "auto-vs-padded" `
		-CropA $autoInfo.Crop -CropB $paddedInfo.Crop
	if ($abResult.meanDelta -gt $fAB_MEAN_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-padded meanDelta=$($abResult.meanDelta.ToString('F6')) exceeds frozen budget $fAB_MEAN_BUDGET — missing terrain, holes, or ghost chunks. Artifacts: $autoShot vs $paddedShot"
	}
	if ($abResult.p999Delta -gt $fAB_P999_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-padded p99.9Delta=$($abResult.p999Delta.ToString('F6')) exceeds frozen budget $fAB_P999_BUDGET — localized corruption. Artifacts: $autoShot vs $paddedShot"
	}
	if ($abResult.maxDelta -gt $fAB_MAX_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-padded maxDelta=$($abResult.maxDelta.ToString('F6')) exceeds frozen budget $fAB_MAX_BUDGET — localized corruption. Artifacts: $autoShot vs $paddedShot"
	}
	if ($abResult.maskXor -gt $fMASK_XOR_BUDGET -or $abResult.maskIoU -lt $fMASK_IOU_FLOOR) {
		throw "[TerrainIndirectCompatibility] auto-vs-padded terrain mask mismatch: xor=$($abResult.maskXor.ToString('F6')) (max $fMASK_XOR_BUDGET), IoU=$($abResult.maskIoU.ToString('F6')) (min $fMASK_IOU_FLOOR)"
	}
	Write-Host "[TerrainIndirectCompatibility] A/B PASS auto-vs-padded mean=$($abResult.meanDelta.ToString('F6')) p99.9=$($abResult.p999Delta.ToString('F6')) max=$($abResult.maxDelta.ToString('F6')) maskXor=$($abResult.maskXor.ToString('F6')) IoU=$($abResult.maskIoU.ToString('F6'))"

	# auto vs single — PADDED_SINGLE must also draw the same terrain.
	$asResult = Compare-Shots -A $autoShot -B $singleShot -Label "auto-vs-single" `
		-CropA $autoInfo.Crop -CropB $singleInfo.Crop
	if ($asResult.meanDelta -gt $fAS_MEAN_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-single meanDelta=$($asResult.meanDelta.ToString('F6')) exceeds frozen budget $fAS_MEAN_BUDGET — the PADDED_SINGLE tier is not spec-equivalent. Artifacts: $autoShot vs $singleShot"
	}
	if ($asResult.p999Delta -gt $fAS_P999_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-single p99.9Delta=$($asResult.p999Delta.ToString('F6')) exceeds frozen budget $fAS_P999_BUDGET — localized corruption. Artifacts: $autoShot vs $singleShot"
	}
	if ($asResult.maxDelta -gt $fAS_MAX_BUDGET) {
		throw "[TerrainIndirectCompatibility] auto-vs-single maxDelta=$($asResult.maxDelta.ToString('F6')) exceeds frozen budget $fAS_MAX_BUDGET — localized corruption in single tier. Artifacts: $autoShot vs $singleShot"
	}
	if ($asResult.maskXor -gt $fMASK_XOR_BUDGET -or $asResult.maskIoU -lt $fMASK_IOU_FLOOR) {
		throw "[TerrainIndirectCompatibility] auto-vs-single terrain mask mismatch: xor=$($asResult.maskXor.ToString('F6')) (max $fMASK_XOR_BUDGET), IoU=$($asResult.maskIoU.ToString('F6')) (min $fMASK_IOU_FLOOR)"
	}
	Write-Host "[TerrainIndirectCompatibility] A/B PASS auto-vs-single mean=$($asResult.meanDelta.ToString('F6')) p99.9=$($asResult.p999Delta.ToString('F6')) max=$($asResult.maxDelta.ToString('F6')) maskXor=$($asResult.maskXor.ToString('F6')) IoU=$($asResult.maskIoU.ToString('F6'))"
}

Write-Host "[TerrainIndirectCompatibility] ALL ARMS PASS"
Write-Host "  artifacts: $ArtifactDir"
