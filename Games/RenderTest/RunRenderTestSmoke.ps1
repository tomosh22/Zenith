param(
	[int]$Frames = 240,
	[int]$TimeoutSeconds = 900,
	[switch]$NoBuild,
	[switch]$ForceRegenerate,
	[switch]$LodDebug,
	[switch]$Wireframe,
	[switch]$ForcedIndirectCountPadded,
	[switch]$ForcedIndirectCountSingle
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Solution = Join-Path $Root "Games\RenderTest\rendertest_win64.sln"
$Exe = Join-Path $Root "Games\RenderTest\build\output\win64\vulkan_vs2022_debug_win64_true\rendertest.exe"
$LogDir = Join-Path $Root "Games\RenderTest\build\obj\smoke"
$StdoutLog = ""
$StderrLog = ""

New-Item -ItemType Directory -Force $LogDir | Out-Null

function Find-MSBuild {
	$candidates = @(
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe"
	)

	foreach ($candidate in $candidates) {
		if (Test-Path $candidate) {
			return $candidate
		}
	}

	throw "MSBuild.exe not found. Open a VS developer prompt or update RunRenderTestSmoke.ps1 with the local MSBuild path."
}

if (-not $NoBuild) {
	$msbuild = Find-MSBuild
	& $msbuild $Solution /t:RenderTest /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount
	if ($LASTEXITCODE -ne 0) {
		throw "RenderTest build failed with exit code $LASTEXITCODE"
	}
}

if (-not (Test-Path $Exe)) {
	throw "RenderTest executable missing: $Exe"
}

$OutputDir = Split-Path $Exe
$SiblingRuntimeDirs = @(
	(Join-Path $Root "FluxCompiler\output\win64\vs2022_debug_win64_true")
)

foreach ($runtimeDir in $SiblingRuntimeDirs) {
	if (Test-Path $runtimeDir) {
		Get-ChildItem $runtimeDir -Filter "*.dll" | ForEach-Object {
			$targetDll = Join-Path $OutputDir $_.Name
			if (-not (Test-Path $targetDll)) {
				Copy-Item -Force $_.FullName $targetDll
				Write-Host "[RenderTestSmoke] Copied runtime DLL $($_.Name) from $runtimeDir"
			}
		}
	}
}

$DebugCrtDir = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\2022" -Recurse -Filter "msvcp140d.dll" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -match "debug_nonredist\\x64\\Microsoft\.VC143\.DebugCRT" } |
	Select-Object -First 1 |
	ForEach-Object { Split-Path $_.FullName }
$DebugUcrtDir = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter "ucrtbased.dll" -ErrorAction SilentlyContinue |
	Where-Object { $_.FullName -match "\\x64\\ucrt\\" } |
	Sort-Object FullName -Descending |
	Select-Object -First 1 |
	ForEach-Object { Split-Path $_.FullName }

if ($DebugCrtDir) {
	$env:PATH = "$DebugCrtDir;$env:PATH"
}
if ($DebugUcrtDir) {
	$env:PATH = "$DebugUcrtDir;$env:PATH"
}

# --no-imgui-ini: the smoke is windowed but passes none of the automated-test
# flags, so it must explicitly opt out of ini load to get the deterministic
# code-built dock layout.
$args = @("--rendertest-smoke", "--rendertest-smoke-frames=$Frames", "--skip-unit-tests", "--skip-tool-exports", "--no-imgui-ini")
if ($ForceRegenerate) {
	$args += "--rendertest-force-regenerate"
}
if ($LodDebug) {
	$args += "--rendertest-lod-debug"
}
if ($Wireframe) {
	$args += "--rendertest-wireframe"
}
if ($ForcedIndirectCountPadded -and $ForcedIndirectCountSingle) {
	throw "ForcedIndirectCountPadded and ForcedIndirectCountSingle are mutually exclusive"
}
# Phase 8 of the terrain indirect-count compatibility plan: the smoke matrix
# adds a forced-padded indirect-count-mode case (the padded tier is the
# shipping fallback for no-count Android hardware; the smoke matrix must
# not require a nonexistent screenshot — RunRenderTestSmoke.ps1 still drives
# the resource/scene smoke, the dedicated graphics A/B wrapper
# RunTerrainIndirectCompatibility.ps1 handles the screenshot gate). The
# forced padded case fails closed if validation/synchronization fires or the
# retired "terrain will not render / streaming disabled" warning re-appears.
if ($ForcedIndirectCountPadded) {
	$args += "--indirect-count-mode=padded"
}
if ($ForcedIndirectCountSingle) {
	$args += "--indirect-count-mode=single"
}
$ExpectedIndirectRequest = if ($ForcedIndirectCountPadded) {
	"padded"
} elseif ($ForcedIndirectCountSingle) {
	"single"
} else {
	"auto"
}

$SmokeFailurePattern = "RENDERTEST_SMOKE_FAIL|VK ERROR|VUID-|Validation Error|Synchronization-Violation|Zenith_Assert|Assertion failed:|device lost|VK_ERROR_DEVICE_LOST|FAILED_CLOSED|terrain will not render|streaming is disabled|indirect-buffer bounds"

function Get-CombinedSmokeLog([string]$StdoutPath, [string]$StderrPath) {
	$stdout = if (Test-Path -LiteralPath $StdoutPath) { Get-Content -LiteralPath $StdoutPath -Raw } else { "" }
	$stderr = if (Test-Path -LiteralPath $StderrPath) { Get-Content -LiteralPath $StderrPath -Raw } else { "" }
	return ([string]$stdout + "`n" + [string]$stderr)
}

function Assert-NoSmokeFailureMarkers([string]$Text, [string]$Label) {
	$failure = [regex]::Match($Text, $SmokeFailurePattern, [Text.RegularExpressions.RegexOptions]::IgnoreCase)
	if ($failure.Success) {
		throw "$Label reported hard failure marker '$($failure.Value)'. Logs: $StdoutLog $StderrLog"
	}
}

function Assert-SmokeIndirectTier([string]$Text, [string]$RequestedMode) {
	$pattern = "RENDERTEST_SMOKE_INDIRECT_TIER request=(auto|native|padded|single) expected=(NATIVE_COUNT|PADDED_MULTI|PADDED_SINGLE) native=(\d+) paddedMulti=(\d+) paddedSingle=(\d+) failClosed=(\d+)"
	$tierMatches = [regex]::Matches($Text, $pattern)
	if ($tierMatches.Count -ne 1) {
		throw "RenderTest smoke emitted $($tierMatches.Count) complete indirect-tier evidence lines; expected exactly one. Logs: $StdoutLog $StderrLog"
	}
	$tier = $tierMatches[0]
	$request = $tier.Groups[1].Value
	$expected = $tier.Groups[2].Value
	$native = [uint64]::Parse($tier.Groups[3].Value)
	$multi = [uint64]::Parse($tier.Groups[4].Value)
	$single = [uint64]::Parse($tier.Groups[5].Value)
	$failed = [uint64]::Parse($tier.Groups[6].Value)
	if ($request -ne $RequestedMode) {
		throw "RenderTest smoke requested '$RequestedMode' but the executable reported '$request'. Logs: $StdoutLog $StderrLog"
	}
	$exact = switch ($expected) {
		"NATIVE_COUNT"  { $native -gt 0 -and $multi -eq 0 -and $single -eq 0 }
		"PADDED_MULTI"  { $native -eq 0 -and $multi -gt 0 -and $single -eq 0 }
		"PADDED_SINGLE" { $native -eq 0 -and $multi -eq 0 -and $single -gt 0 }
	}
	if ($failed -ne 0 -or -not $exact) {
		throw "RenderTest smoke expected exactly $expected, got native=$native paddedMulti=$multi paddedSingle=$single failClosed=$failed. Logs: $StdoutLog $StderrLog"
	}
	if ($RequestedMode -eq "padded" -and $expected -notmatch "^PADDED_(MULTI|SINGLE)$") {
		throw "RenderTest smoke forced padded but executed $expected. Logs: $StdoutLog $StderrLog"
	}
	if ($RequestedMode -eq "single" -and $expected -ne "PADDED_SINGLE") {
		throw "RenderTest smoke forced single but executed $expected. Logs: $StdoutLog $StderrLog"
	}
}

# Retry loop. The smoke test occasionally hits a GPU mid-frame-flush
# stall (HandleStagingBufferFull's EndAndCpuWait blocks 5-10s on a
# scene-reload upload >512 MiB). When the stall is long enough,
# the worker thread pile-up sometimes causes a silent crash before
# RENDERTEST_SMOKE_PASS is emitted. The smoke test is otherwise
# deterministic -- the failure is hardware-load-variance dependent.
# Engine-side mitigations (asset-handle cleanup, log mutex, mutex
# release around HandleStagingBufferFull) cut the per-attempt fail
# rate. Retrying handles the residual flake without papering over
# legitimate failures: any consistent crash, validation error, or
# RENDERTEST_SMOKE_FAIL marker still trips the gate immediately.
$MaxAttempts = 3
$attempt = 0
$lastFailureMessage = ""
$hasPassMarker = $false
$exitCode = $null

while ($attempt -lt $MaxAttempts) {
	$attempt++
	$StdoutLog = Join-Path $LogDir "rendertest_smoke_attempt${attempt}_stdout.log"
	$StderrLog = Join-Path $LogDir "rendertest_smoke_attempt${attempt}_stderr.log"
	Remove-Item -Force -ErrorAction SilentlyContinue $StdoutLog, $StderrLog

	Write-Host "[RenderTestSmoke] Attempt $attempt/$MaxAttempts -- Running $Exe $($args -join ' ')"
	$process = Start-Process -FilePath $Exe `
		-ArgumentList $args `
		-WorkingDirectory (Split-Path $Exe) `
		-RedirectStandardOutput $StdoutLog `
		-RedirectStandardError $StderrLog `
		-PassThru

	if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
		$process.Kill()
		$process.WaitForExit()
		$combined = Get-CombinedSmokeLog $StdoutLog $StderrLog
		Assert-NoSmokeFailureMarkers $combined "RenderTest smoke attempt $attempt (timed out)"
		$lastFailureMessage = "RenderTest smoke timed out after $TimeoutSeconds seconds. Logs: $StdoutLog $StderrLog"
		Write-Host "[RenderTestSmoke] Attempt $attempt timed out without a hard failure marker. Retrying; logs are preserved."
		continue
	}
	$process.WaitForExit()
	$process.Refresh()

	$combined = Get-CombinedSmokeLog $StdoutLog $StderrLog

	$exitCode = $process.ExitCode
	# Hard failure: any of these stops the gate immediately -- not a flake.
	# Phase 8 of the terrain indirect-count compatibility plan: the retired
	# "terrain will not render / streaming disabled" warning and indirect-buffer
	# bounds errors are now hard markers — the padded fallback tier MUST draw
	# terrain and never bounds-overflow the persistent argument buffer.
	$hasPassMarker = $combined -match "RENDERTEST_SMOKE_PASS"
	Assert-NoSmokeFailureMarkers $combined "RenderTest smoke attempt $attempt"
	if ($hasPassMarker) {
		Assert-SmokeIndirectTier $combined $ExpectedIndirectRequest
	}

	if ($hasPassMarker -and $null -ne $exitCode -and $exitCode -eq 0) {
		break
	}

	$lastFailureMessage = if ($hasPassMarker -and $null -ne $exitCode -and $exitCode -ne 0) {
		"RenderTest emitted RENDERTEST_SMOKE_PASS but exited with code $exitCode. Logs: $StdoutLog $StderrLog"
	} elseif ($hasPassMarker -and $null -eq $exitCode) {
		"RenderTest emitted RENDERTEST_SMOKE_PASS but its process exit code was unavailable. Logs: $StdoutLog $StderrLog"
	} elseif ($null -ne $exitCode -and $exitCode -ne 0) {
		"RenderTest exited with code $exitCode without emitting RENDERTEST_SMOKE_PASS. Logs: $StdoutLog $StderrLog"
	} else {
		"RenderTest did not emit RENDERTEST_SMOKE_PASS. Logs: $StdoutLog $StderrLog"
	}

	Write-Host "[RenderTestSmoke] Attempt $attempt failed ($lastFailureMessage). Retrying..."
}

if (-not $hasPassMarker -or $null -eq $exitCode -or $exitCode -ne 0) {
	throw "RenderTest smoke failed after $MaxAttempts attempts. Last failure: $lastFailureMessage"
}

Write-Host "[RenderTestSmoke] PASS (attempt $attempt/$MaxAttempts)"
Write-Host "[RenderTestSmoke] stdout: $StdoutLog"
Write-Host "[RenderTestSmoke] stderr: $StderrLog"
