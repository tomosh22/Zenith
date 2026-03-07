# Fix AGDE vcxproj files after Sharpmake generation
# Fix C++ standard from cpp20 to cpp2a

$files = Get-ChildItem -Path "$PSScriptRoot\.." -Recurse -Filter '*_agde.vcxproj'

foreach ($f in $files) {
    Write-Host "Processing: $($f.FullName)"
    $content = Get-Content $f.FullName -Raw
    $content = $content -replace '<CppLanguageStandard>cpp20</CppLanguageStandard>', '<CppLanguageStandard>cpp2a</CppLanguageStandard>'
    Set-Content $f.FullName $content -Encoding UTF8 -NoNewline
    Write-Host "  Done"
}

Write-Host "`nAll AGDE vcxproj files updated."
