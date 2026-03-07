# Fix AGDE vcxproj files after Sharpmake generation
# 1. Fix C++ standard from cpp20 to cpp2a
# 2. Inject UBSan compiler flags for debug configurations

$files = Get-ChildItem -Path "$PSScriptRoot\.." -Recurse -Filter '*_agde.vcxproj'

foreach ($f in $files) {
    Write-Host "Processing: $($f.FullName)"
    $lines = Get-Content $f.FullName
    $out = @()
    $inDebugItemDef = $false
    $addedUBSan = $false

    foreach ($line in $lines) {
        # Fix C++ standard
        $line = $line -replace '<CppLanguageStandard>cpp20</CppLanguageStandard>', '<CppLanguageStandard>cpp2a</CppLanguageStandard>'
        $out += $line

        # Track debug ItemDefinitionGroup
        if ($line -match 'ItemDefinitionGroup.*Debug') {
            $inDebugItemDef = $true
            $addedUBSan = $false
        }

        # Inject UBSan after <ClCompile> in debug configs
        if ($inDebugItemDef -and $line -match '^\s*<ClCompile>' -and -not $addedUBSan) {
            $out += '      <AdditionalOptions>-fsanitize=undefined -fno-sanitize=alignment -fno-sanitize-recover=all -fno-omit-frame-pointer %(AdditionalOptions)</AdditionalOptions>'
            $addedUBSan = $true
            $inDebugItemDef = $false
        }
    }

    Set-Content $f.FullName $out -Encoding UTF8
    Write-Host "  Done"
}

Write-Host "`nAll AGDE vcxproj files updated."
