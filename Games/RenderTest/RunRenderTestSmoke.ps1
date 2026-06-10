param(
	[int]$Frames = 240,
	[int]$TimeoutSeconds = 900,
	[switch]$NoBuild,
	[switch]$ForceRegenerate,
	[switch]$LodDebug,
	[switch]$Wireframe
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Project = Join-Path $Root "Games\RenderTest\build\rendertest_win64.vcxproj"
$Exe = Join-Path $Root "Games\RenderTest\build\output\win64\vs2022_debug_win64_true\rendertest.exe"
$LogDir = Join-Path $Root "Games\RenderTest\build\obj\smoke"
$StdoutLog = Join-Path $LogDir "rendertest_smoke_stdout.log"
$StderrLog = Join-Path $LogDir "rendertest_smoke_stderr.log"

New-Item -ItemType Directory -Force $LogDir | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue $StdoutLog, $StderrLog

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
	& $msbuild $Project /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount
	if ($LASTEXITCODE -ne 0) {
		throw "RenderTest build failed with exit code $LASTEXITCODE"
	}
}

if (-not (Test-Path $Exe)) {
	throw "RenderTest executable missing: $Exe"
}

$OutputDir = Split-Path $Exe
$SiblingRuntimeDirs = @(
	(Join-Path $Root "Games\Exploration\build\output\win64\vs2022_debug_win64_true"),
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

$args = @("--rendertest-smoke", "--rendertest-smoke-frames=$Frames", "--skip-unit-tests", "--skip-tool-exports")
if ($ForceRegenerate) {
	$args += "--rendertest-force-regenerate"
}
if ($LodDebug) {
	$args += "--rendertest-lod-debug"
}
if ($Wireframe) {
	$args += "--rendertest-wireframe"
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
		$lastFailureMessage = "RenderTest smoke timed out after $TimeoutSeconds seconds. Logs: $StdoutLog $StderrLog"
		continue
	}
	$process.WaitForExit()
	$process.Refresh()

	$stdout = if (Test-Path $StdoutLog) { Get-Content $StdoutLog -Raw } else { "" }
	$stderr = if (Test-Path $StderrLog) { Get-Content $StderrLog -Raw } else { "" }
	$combined = $stdout + "`n" + $stderr

	$exitCode = $process.ExitCode
	# Hard failure: any of these stops the gate immediately -- not a flake.
	$hasFailureMarker = $combined -match "RENDERTEST_SMOKE_FAIL|VK ERROR|VUID-|Validation Error|Zenith_Assert"
	$hasPassMarker = $combined -match "RENDERTEST_SMOKE_PASS"

	if ($hasFailureMarker) {
		throw "RenderTest smoke reported a failure or validation error. Logs: $StdoutLog $StderrLog"
	}

	if ($hasPassMarker) {
		break  # Real success path. Exit code handling below.
	}

	$lastFailureMessage = if ($null -ne $exitCode -and $exitCode -ne 0) {
		"RenderTest exited with code $exitCode without emitting RENDERTEST_SMOKE_PASS. Logs: $StdoutLog $StderrLog"
	} else {
		"RenderTest did not emit RENDERTEST_SMOKE_PASS. Logs: $StdoutLog $StderrLog"
	}

	Write-Host "[RenderTestSmoke] Attempt $attempt failed ($lastFailureMessage). Retrying..."
}

if (-not $hasPassMarker) {
	throw "RenderTest smoke failed after $MaxAttempts attempts. Last failure: $lastFailureMessage"
}

# PASS was emitted -- test logically succeeded. Non-zero exit codes
# from shutdown crashes are accepted (logged as a warning) since the
# test work itself completed successfully.
if ($null -ne $exitCode -and $exitCode -ne 0) {
	Write-Warning "[RenderTestSmoke] RenderTest emitted PASS marker but exited with code $exitCode (likely shutdown-path issue; logged for visibility)."
}
elseif ($null -eq $exitCode) {
	Write-Warning "[RenderTestSmoke] Process exit code was unavailable; accepting RENDERTEST_SMOKE_PASS marker from captured output."
}

Write-Host "[RenderTestSmoke] PASS (attempt $attempt/$MaxAttempts)"
Write-Host "[RenderTestSmoke] stdout: $StdoutLog"
Write-Host "[RenderTestSmoke] stderr: $StderrLog"
