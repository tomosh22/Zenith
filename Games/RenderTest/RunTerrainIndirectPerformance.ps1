param(
	[Parameter(Mandatory=$true)]
	[ValidateSet("auto", "native", "padded", "single")]
	[string]$Mode,

	[Parameter(Mandatory=$true)]
	[ValidateSet("dense", "horizon", "culled")]
	[string]$CameraCase,

	[ValidateRange(1, 6000)][int]$Warmup = 120,
	[ValidateRange(1, 6000)][int]$Sample = 300,
	[ValidateRange(1, 10)][int]$Repeats = 3,
	[ValidateRange(1, 7200)][int]$TimeoutSeconds = 1800,
	[string]$Exe,
	[string]$ArtifactDir,
	[switch]$DeveloperSanity,
	[switch]$NoBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

if (-not $DeveloperSanity -and ($Warmup -lt 120 -or $Sample -lt 300 -or $Repeats -lt 3)) {
	throw "release protocol requires Warmup >= 120, Sample >= 300, and Repeats >= 3; use -DeveloperSanity only for a non-budgetable launch/parser check"
}

# Deterministic steady-state terrain indirect benchmark. Each repeat is a fresh
# automated-test process. The C++ test writes one raw row for every sampled GPU
# capture; this wrapper validates the run and publishes repeat + aggregate stats.

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Solution = Join-Path $Root "Games\RenderTest\rendertest_win64.sln"
$DefaultExe = Join-Path $Root "Games\RenderTest\Build\output\win64\vulkan_vs2022_release_win64_false\rendertest.exe"
$UsingDefaultExe = [string]::IsNullOrWhiteSpace($Exe)
if ($UsingDefaultExe) { $Exe = $DefaultExe }

function Find-MSBuild {
	$candidates = @(
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
		"C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
	)
	foreach ($candidate in $candidates) {
		if (Test-Path -LiteralPath $candidate) { return $candidate }
	}
	throw "MSBuild.exe was not found"
}

if ($UsingDefaultExe -and -not $NoBuild) {
	$MSBuild = Find-MSBuild
	Write-Host "[TerrainIndirectPerformance] Building optimized Vulkan Tools=False RenderTest"
	& $MSBuild $Solution /t:RenderTest /p:Configuration=Vulkan_vs2022_Release_Win64_False /p:Platform=x64 -maxCpuCount -nologo
	if ($LASTEXITCODE -ne 0) { throw "RenderTest optimized Vulkan build failed (exit $LASTEXITCODE)" }
}
if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
	throw "RenderTest executable is missing: $Exe"
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$OutputDir = Split-Path -Parent $Exe

if ([string]::IsNullOrWhiteSpace($ArtifactDir)) {
	$ArtifactDir = Join-Path $Root "Build\artifacts\rendertest\terrain_indirect\performance"
}
New-Item -ItemType Directory -Force -Path $ArtifactDir | Out-Null
$ArtifactDir = (Resolve-Path -LiteralPath $ArtifactDir).Path

function Get-TextFile([string]$Path) {
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
	return [string](Get-Content -LiteralPath $Path -Raw)
}

function Join-ProcessArguments([string[]]$Arguments) {
	$quoted = foreach ($argument in $Arguments) {
		if ($argument.IndexOf('"') -ge 0) { throw "process argument contains an unsupported quote: $argument" }
		'"' + $argument + '"'
	}
	return ($quoted -join ' ')
}

function Assert-NoFailureMarkers([string]$Text, [string]$Label) {
	$markers = @(
		"VK ERROR", "VK_ERROR", "VUID-", "Validation Error",
		"Synchronization-Violation", "device lost", "DEVICE_LOST",
		"Zenith_Assert", "assertion failed", "terrain will not render",
		"streaming is disabled"
	)
	foreach ($marker in $markers) {
		if ($Text.IndexOf($marker, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
			throw "$Label reported failure marker '$marker'"
		}
	}
}

function Convert-ToInvariantDouble([object]$Value, [string]$Column, [string]$Label) {
	$parsed = 0.0
	if (-not [double]::TryParse([string]$Value,
		[Globalization.NumberStyles]::Float,
		[Globalization.CultureInfo]::InvariantCulture,
		[ref]$parsed)) {
		throw "$Label has a non-numeric '$Column' value: '$Value'"
	}
	if ([double]::IsNaN($parsed) -or [double]::IsInfinity($parsed)) {
		throw "$Label has a non-finite '$Column' value: '$Value'"
	}
	return $parsed
}

function Convert-ToUInt64([object]$Value, [string]$Column, [string]$Label) {
	$parsed = [uint64]0
	if (-not [uint64]::TryParse([string]$Value,
		[Globalization.NumberStyles]::Integer,
		[Globalization.CultureInfo]::InvariantCulture,
		[ref]$parsed)) {
		throw "$Label has an invalid '$Column' value: '$Value'"
	}
	return $parsed
}

function Get-Stats([double[]]$Values) {
	if ($null -eq $Values -or $Values.Count -eq 0) { throw "cannot calculate statistics for an empty sample set" }
	$sorted = [double[]]@($Values | Sort-Object)
	$count = $sorted.Count
	if (($count % 2) -eq 0) {
		$median = ($sorted[($count / 2) - 1] + $sorted[$count / 2]) / 2.0
	} else {
		$median = $sorted[[int][math]::Floor($count / 2)]
	}
	$p95Index = [int][math]::Ceiling(0.95 * $count) - 1
	if ($p95Index -lt 0) { $p95Index = 0 }
	return [pscustomobject][ordered]@{
		count = $count
		median = $median
		p95 = $sorted[$p95Index]
		min = $sorted[0]
		max = $sorted[$count - 1]
	}
}

$MetricColumns = @(
	"reset_gpu_ms", "culling_gpu_ms", "gbuffer_gpu_ms", "total_gpu_ms",
	"reset_cpu_record_ms", "culling_cpu_record_ms", "gbuffer_cpu_record_ms"
)
$CounterColumns = @(
	"indirect_native_delta", "indirect_padded_multi_delta",
	"indirect_padded_single_delta", "indirect_fail_closed_delta",
	"indirect_fixed_delta"
)

function Get-MetricSummary([object[]]$Rows) {
	$summary = [ordered]@{}
	foreach ($column in $MetricColumns) {
		[double[]]$values = @($Rows | ForEach-Object { [double]($_.$column) })
		$summary[$column] = Get-Stats $values
	}
	return [pscustomobject]$summary
}

function Get-CounterSummary([object[]]$Rows) {
	$summary = [ordered]@{}
	foreach ($column in $CounterColumns) {
		[uint64]$sum = 0
		foreach ($row in $Rows) { $sum += [uint64]($row.$column) }
		$summary[$column] = $sum
	}
	return [pscustomobject]$summary
}

function Assert-Telemetry([object]$Counters, [string]$Label) {
	$native = [uint64]$Counters.indirect_native_delta
	$multi = [uint64]$Counters.indirect_padded_multi_delta
	$single = [uint64]$Counters.indirect_padded_single_delta
	$failed = [uint64]$Counters.indirect_fail_closed_delta
	if ($failed -ne 0) { throw "$Label entered FAILED_CLOSED $failed time(s)" }
	if (($native + $multi + $single) -eq 0) {
		throw "$Label recorded no counted-indirect execution telemetry"
	}
	switch ($Mode) {
		"native" {
			if ($native -eq 0 -or $multi -ne 0 -or $single -ne 0) {
				throw "$Label did not exclusively execute native count (native=$native multi=$multi single=$single)"
			}
		}
		"padded" {
			if ($native -ne 0 -or ($multi + $single) -eq 0) {
				throw "$Label did not execute a padded tier exclusively (native=$native multi=$multi single=$single)"
			}
		}
		"single" {
			if ($native -ne 0 -or $multi -ne 0 -or $single -eq 0) {
				throw "$Label did not exclusively execute padded-single (native=$native multi=$multi single=$single)"
			}
		}
		"auto" {
			if ($native -ne 0 -and ($multi + $single) -ne 0) {
				throw "$Label mixed native and padded tiers in auto mode (native=$native multi=$multi single=$single)"
			}
		}
	}
}

# Discovery is a real gate: the optimized Tools=False binary must contain the
# manual-only test even though --all-automated-tests intentionally excludes it.
$ListStdout = Join-Path $ArtifactDir "discovery_stdout.log"
$ListStderr = Join-Path $ArtifactDir "discovery_stderr.log"
Remove-Item -LiteralPath $ListStdout, $ListStderr -Force -ErrorAction SilentlyContinue
$ListProcess = Start-Process -FilePath $Exe `
	-ArgumentList (Join-ProcessArguments @("--list-automated-tests", "--skip-unit-tests", "--skip-tool-exports")) `
	-WorkingDirectory $OutputDir -RedirectStandardOutput $ListStdout `
	-RedirectStandardError $ListStderr -PassThru
if (-not $ListProcess.WaitForExit(120000)) {
	$ListProcess.Kill()
	$ListProcess.WaitForExit()
	throw "--list-automated-tests timed out"
}
$ListProcess.WaitForExit()
if ($ListProcess.ExitCode -ne 0) { throw "--list-automated-tests exited $($ListProcess.ExitCode)" }
$ListText = (Get-TextFile $ListStdout) + "`n" + (Get-TextFile $ListStderr)
Assert-NoFailureMarkers $ListText "test discovery"
if ($ListText -notmatch "(?m)^\s*TerrainIndirectPerformance\s*(?:\[manual\])?\s*$") {
	throw "TerrainIndirectPerformance is not registered in $Exe"
}

$BaseName = "perf_${Mode}_${CameraCase}"
$AllRows = @()
$RepeatReports = @()
$CapabilityLines = @()
$GPUName = ""
$GPUApiVersion = ""
$GPUDriverVersion = ""

for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
	$label = "$Mode/$CameraCase repeat $repeat"
	$rawCsv = Join-Path $ArtifactDir "${BaseName}_repeat${repeat}_raw.csv"
	$resultJson = Join-Path $ArtifactDir "${BaseName}_repeat${repeat}_result.json"
	$stdout = Join-Path $ArtifactDir "${BaseName}_repeat${repeat}_stdout.log"
	$stderr = Join-Path $ArtifactDir "${BaseName}_repeat${repeat}_stderr.log"
	Remove-Item -LiteralPath $rawCsv, $resultJson, $stdout, $stderr -Force -ErrorAction SilentlyContinue

	$arguments = @(
		"--automated-test", "TerrainIndirectPerformance",
		"--indirect-count-mode=$Mode",
		"--terrain-indirect-perf-camera=$CameraCase",
		"--terrain-indirect-perf-warmup=$Warmup",
		"--terrain-indirect-perf-samples=$Sample",
		"--terrain-indirect-perf-output=$rawCsv",
		"--test-results", $resultJson,
		"--fixed-dt", "0.016666667",
		"--skip-unit-tests", "--skip-tool-exports", "--no-imgui-ini"
	)
	if ($DeveloperSanity) { $arguments += "--terrain-indirect-perf-developer-sanity" }

	Write-Host "[TerrainIndirectPerformance] Running $label ($Warmup warmup + $Sample samples)"
	$process = Start-Process -FilePath $Exe -ArgumentList (Join-ProcessArguments $arguments) `
		-WorkingDirectory $OutputDir -RedirectStandardOutput $stdout `
		-RedirectStandardError $stderr -PassThru
	if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
		$process.Kill()
		$process.WaitForExit()
		throw "$label timed out after $TimeoutSeconds seconds"
	}
	$process.WaitForExit()
	$exitCode = $process.ExitCode
	$combinedLog = (Get-TextFile $stdout) + "`n" + (Get-TextFile $stderr)
	Assert-NoFailureMarkers $combinedLog $label
	if ($exitCode -ne 0) { throw "$label exited with code $exitCode" }

	if (-not (Test-Path -LiteralPath $resultJson -PathType Leaf)) {
		throw "$label did not write automated-test result JSON: $resultJson"
	}
	try { $testResult = Get-Content -LiteralPath $resultJson -Raw | ConvertFrom-Json }
	catch { throw "$label wrote invalid automated-test JSON: $($_.Exception.Message)" }
	if (-not $testResult.passed) { throw "$label reported passed=false" }
	if ($testResult.skipped) { throw "$label was skipped" }

	if (-not (Test-Path -LiteralPath $rawCsv -PathType Leaf)) {
		throw "$label did not write raw CSV: $rawCsv"
	}
	try { $rawRows = @(Import-Csv -LiteralPath $rawCsv) }
	catch { throw "$label wrote invalid CSV: $($_.Exception.Message)" }
	if ($rawRows.Count -ne $Sample) {
		throw "$label wrote $($rawRows.Count) samples; expected exactly $Sample"
	}

	$normalRows = @()
	$previousSerial = [uint64]0
	for ($i = 0; $i -lt $rawRows.Count; ++$i) {
		$row = $rawRows[$i]
		if ([int](Convert-ToUInt64 $row.sample_index "sample_index" $label) -ne $i) {
			throw "$label has a non-contiguous sample_index at row $i"
		}
		$serial = Convert-ToUInt64 $row.gpu_capture_serial "gpu_capture_serial" $label
		if ($serial -eq 0 -or ($previousSerial -ne 0 -and $serial -ne ($previousSerial + 1))) {
			throw "$label has a missing/duplicate GPU capture serial at row $i ($previousSerial -> $serial)"
		}
		$previousSerial = $serial

		$normal = [ordered]@{ repeat = $repeat; sample_index = $i; gpu_capture_serial = $serial }
		foreach ($column in $MetricColumns) {
			$value = Convert-ToInvariantDouble $row.$column $column $label
			if ($value -lt 0.0 -or ($column -eq "total_gpu_ms" -and $value -le 0.0)) {
				throw "$label has an empty/negative '$column' sample at row ${i}: $value"
			}
			$normal[$column] = $value
		}
		foreach ($column in $CounterColumns) {
			$normal[$column] = Convert-ToUInt64 $row.$column $column $label
		}
		$normalRows += [pscustomobject]$normal
	}

	$counterSummary = Get-CounterSummary $normalRows
	Assert-Telemetry $counterSummary $label
	$repeatReport = [pscustomobject][ordered]@{
		repeat = $repeat
		sampleCount = $normalRows.Count
		metrics = Get-MetricSummary $normalRows
		indirectTelemetry = $counterSummary
		rawCsv = $rawCsv
		resultJson = $resultJson
		stdout = $stdout
		stderr = $stderr
	}
	$RepeatReports += $repeatReport
	$AllRows += $normalRows

	if ($repeat -eq 1) {
		$logLines = @($combinedLog -split "`r?`n")
		$gpuLine = @($logLines | Where-Object { $_ -match "GPU:\s*" } | Select-Object -First 1)
		$apiLine = @($logLines | Where-Object { $_ -match "GPU API version:\s*" } | Select-Object -First 1)
		$driverLine = @($logLines | Where-Object { $_ -match "GPU driver version:\s*" } | Select-Object -First 1)
		if ($gpuLine.Count -gt 0) { $GPUName = ($gpuLine[0] -replace ".*GPU:\s*", "").Trim() }
		if ($apiLine.Count -gt 0) { $GPUApiVersion = ($apiLine[0] -replace ".*GPU API version:\s*", "").Trim() }
		if ($driverLine.Count -gt 0) { $GPUDriverVersion = ($driverLine[0] -replace ".*GPU driver version:\s*", "").Trim() }
		$CapabilityLines = @($logLines | Where-Object {
			$_ -match "maxDrawIndirectCount|multiDrawIndirect|DrawIndexedIndirectCount|indirect-count-mode|Indirect.*Capabilities"
		})
		if ([string]::IsNullOrWhiteSpace($GPUName) -or
			[string]::IsNullOrWhiteSpace($GPUApiVersion) -or
			[string]::IsNullOrWhiteSpace($GPUDriverVersion)) {
			throw "$label is missing required GPU name/API/driver metadata"
		}
		$capabilityText = $CapabilityLines -join "`n"
		if ($CapabilityLines.Count -eq 0 -or
			$capabilityText -notmatch "DrawIndexedIndirectCount" -or
			$capabilityText -notmatch "multiDrawIndirect" -or
			$capabilityText -notmatch "maxDrawIndirectCount") {
			throw "$label is missing required indirect capability metadata"
		}
	}
	Write-Host "[TerrainIndirectPerformance] $label passed with $($normalRows.Count) samples"
}

if ($AllRows.Count -ne ($Sample * $Repeats)) {
	throw "aggregate sample set is incomplete: $($AllRows.Count) vs $($Sample * $Repeats)"
}
$AggregateCounters = Get-CounterSummary $AllRows
Assert-Telemetry $AggregateCounters "$Mode/$CameraCase aggregate"

$CombinedCsv = Join-Path $ArtifactDir "${BaseName}.csv"
$OutputJson = Join-Path $ArtifactDir "${BaseName}.json"
$AllRows | Export-Csv -LiteralPath $CombinedCsv -NoTypeInformation -Encoding UTF8

$lowerExe = $Exe.ToLowerInvariant()
$classification = "customDiagnostic"
$budgetEligible = $false
if ($DeveloperSanity) {
	$classification = "developerSanity"
} elseif ($lowerExe -match "vulkan_vs2022_release_win64_false") {
	$classification = "optimizedRuntime"
	$budgetEligible = $true
} elseif ($lowerExe -match "release_win64_true") {
	$classification = "optimizedToolsDiagnostic"
} elseif ($lowerExe -match "debug") {
	$classification = "debugDiagnostic"
}

$report = [pscustomobject][ordered]@{
	schemaVersion = 1
	benchmark = "TerrainIndirectPerformance"
	mode = $Mode
	cameraCase = $CameraCase
	fixedDtSeconds = 0.016666667
	warmupFramesPerRepeat = $Warmup
	sampleFramesPerRepeat = $Sample
	repeats = $Repeats
	totalSampleFrames = $AllRows.Count
	developerSanity = [bool]$DeveloperSanity
	executable = $Exe
	measurementClassification = $classification
	budgetEligible = $budgetEligible
	budgetNote = if ($budgetEligible) {
		"Optimized Vulkan Tools=False measurement; budgets still require an explicit ratification change."
	} else {
		"Diagnostic measurement only; do not ratify shipping budgets from this executable."
	}
	hardware = [pscustomobject][ordered]@{
		gpuName = $GPUName
		apiVersion = $GPUApiVersion
		driverVersion = $GPUDriverVersion
	}
	capabilities = @($CapabilityLines | Select-Object -Unique)
	perRepeat = $RepeatReports
	aggregate = [pscustomobject][ordered]@{
		metrics = Get-MetricSummary $AllRows
		indirectTelemetry = $AggregateCounters
	}
	combinedCsv = $CombinedCsv
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $OutputJson -Encoding UTF8
# Parse our own final artifact so truncated/invalid JSON cannot be reported as success.
$null = Get-Content -LiteralPath $OutputJson -Raw | ConvertFrom-Json

Write-Host "[TerrainIndirectPerformance] PASS $Mode/$CameraCase"
Write-Host "  JSON: $OutputJson"
Write-Host "  CSV:  $CombinedCsv"
Write-Host "  classification=$classification budgetEligible=$budgetEligible"
