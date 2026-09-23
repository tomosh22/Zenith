# lane_pins.ps1 -- resolve a lane's pin-site conflicts after
# `git merge --no-ff master` into `agent/<KEY>` fails.
# =============================================================================
# The two pin sites a lane's merge is expected to conflict on are
# Tools/unit_baselines.json (a row this lane bumped, that master also bumped
# for a different game) and Games/<G>/Docs/Status.md (the LIVE PIN block and
# the +N history area). Both are resolved textually, never by re-deriving a
# fresh measurement -- these values are PROVISIONAL; the tick's next
# `-Phase measure -ReuseGateList` re-observes and overwrites every row the
# lane can actually measure. This script only needs a value good enough to
# commit the merge.
#
# Exit 0: every conflicted path was an allowed pin site, resolved, and
#         `git add`ed -- the caller commits the merge.
# Exit 2: could not resolve (no MERGE_HEAD, a conflict outside the allowed
#         pin sites, a delete/modify conflict, a conflict below Status.md's
#         first '## ' heading, malformed conflict markers, diff3/zdiff3-style
#         conflict markers ('|||||||'), or a LIVE PIN numbers-line conflict
#         that does not match a supported shape) -- NOTHING is written. Never
#         runs `git merge --abort` or `git commit`.
#
# Usage:
#   pwsh -NoProfile -File Tools/lane_pins.ps1 [-Repo <path>]
#   (-Repo defaults to the current directory.)
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Repo = (Get-Location).Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-LanePinsFail {
    param([string]$Message)
    Write-Host "[lane_pins] $Message" -ForegroundColor Red
}

# =============================================================================
# Preconditions.
# =============================================================================

function Test-LanePinsMergeInProgress {
    param([Parameter(Mandatory)][string]$RepoRoot)
    Push-Location -LiteralPath $RepoRoot
    try {
        & git rev-parse -q --verify MERGE_HEAD *> $null
        return ($LASTEXITCODE -eq 0)
    }
    finally { Pop-Location }
}

function Get-LanePinsConflictedPaths {
    param([Parameter(Mandatory)][string]$RepoRoot)
    Push-Location -LiteralPath $RepoRoot
    try {
        $out = & git diff --name-only --diff-filter=U 2>$null
        if ($LASTEXITCODE -ne 0) { return , @() }
        return , @($out | Where-Object { $_ })
    }
    finally { Pop-Location }
}

function Test-LanePinsAllowedPath {
    param([Parameter(Mandatory)][string]$Path)
    $normalized = $Path -replace '\\', '/'
    if ($normalized -eq 'Tools/unit_baselines.json') { return $true }
    if ($normalized -match '^Games/[^/]+/Docs/Status\.md$') { return $true }
    return $false
}

function Get-LanePinsStageCount {
    param([Parameter(Mandatory)][string]$RepoRoot, [Parameter(Mandatory)][string]$Path)
    Push-Location -LiteralPath $RepoRoot
    try {
        $out = & git ls-files -u -- $Path 2>$null
        if ($LASTEXITCODE -ne 0) { return 0 }
        return @($out | Where-Object { $_ }).Count
    }
    finally { Pop-Location }
}

function Get-LanePinsStageBlobShas {
    # {1=base sha; 2=ours sha; 3=theirs sha} from `git ls-files -u`.
    param([Parameter(Mandatory)][string]$RepoRoot, [Parameter(Mandatory)][string]$Path)
    Push-Location -LiteralPath $RepoRoot
    try {
        $out = & git ls-files -u -- $Path 2>$null
    }
    finally { Pop-Location }
    $result = @{}
    foreach ($line in @($out | Where-Object { $_ })) {
        # "<mode> <sha> <stage>\t<path>"
        if ($line -match '^\S+\s+(\S+)\s+([123])\t') {
            $result[[int]$Matches[2]] = $Matches[1]
        }
    }
    return $result
}

function Get-LanePinsBlobText {
    # Raw blob bytes, via a shell redirection so PowerShell's pipeline never
    # re-splits or re-joins the text (which would lose the per-line CRLF/LF
    # distinction the line-ending rule below depends on), then read back with
    # [IO.File]::ReadAllText per the binding line-ending/encoding rule.
    param([Parameter(Mandatory)][string]$RepoRoot, [Parameter(Mandatory)][string]$Sha)
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("lanepins_" + [guid]::NewGuid().ToString('N') + '.blob')
    Push-Location -LiteralPath $RepoRoot
    try {
        cmd /c "git cat-file blob $Sha > `"$tmp`"" 2>$null
        if ($LASTEXITCODE -ne 0) { throw "[lane_pins] 'git cat-file blob $Sha' failed" }
    }
    finally { Pop-Location }
    try {
        return [IO.File]::ReadAllText($tmp)
    }
    finally {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
}

# =============================================================================
# Line-ending / encoding (binding rule): split into lines while recording each
# line's OWN terminator, edit only target lines' text, rejoin with the
# original terminators, write UTF-8 without BOM.
# =============================================================================

function Split-LanePinsLines {
    # Returns a List[PSCustomObject{Text; Terminator}] -- Terminator is
    # "`r`n", "`r", "`n", or "" for a final line with no trailing newline.
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)
    $lines = New-Object System.Collections.Generic.List[object]
    $len = $Text.Length
    $i = 0
    $start = 0
    while ($i -lt $len) {
        $ch = $Text[$i]
        if ($ch -eq "`r") {
            if ($i + 1 -lt $len -and $Text[$i + 1] -eq "`n") {
                $lines.Add([PSCustomObject]@{ Text = $Text.Substring($start, $i - $start); Terminator = "`r`n" })
                $i += 2
            }
            else {
                $lines.Add([PSCustomObject]@{ Text = $Text.Substring($start, $i - $start); Terminator = "`r" })
                $i += 1
            }
            $start = $i
        }
        elseif ($ch -eq "`n") {
            $lines.Add([PSCustomObject]@{ Text = $Text.Substring($start, $i - $start); Terminator = "`n" })
            $i += 1
            $start = $i
        }
        else {
            $i += 1
        }
    }
    if ($start -lt $len) {
        $lines.Add([PSCustomObject]@{ Text = $Text.Substring($start, $len - $start); Terminator = '' })
    }
    return $lines
}

function Join-LanePinsLines {
    param([Parameter(Mandatory)]$Lines)
    $sb = New-Object System.Text.StringBuilder
    foreach ($l in $Lines) {
        [void]$sb.Append($l.Text)
        [void]$sb.Append($l.Terminator)
    }
    return $sb.ToString()
}

function Write-LanePinsFileUtf8NoBom {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][AllowEmptyString()][string]$Text)
    $enc = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $Text, $enc)
}

# =============================================================================
# Tools/unit_baselines.json: start from THEIRS, and for every numeric pin row
# (matched structurally, never a hard-coded key list) compute
# theirs + (ours - base), edited textually -- never a JSON round trip, which
# would reorder the $-comment rows.
# =============================================================================

$script:LanePinsRowPattern = '^(\s*"([A-Za-z0-9_]+)"\s*:\s*)(\d+)(\s*,?\s*)$'

function Get-LanePinsBaselineRows {
    # {name -> {LineIndex; Prefix; Value; Suffix}}, scanning only inside the
    # `"baselines": { ... }` object (brace-depth tracked, not a fixed line
    # range) so a $-comment string value never matches (it isn't a bare digit
    # run) and nothing outside that object is ever touched.
    param([Parameter(Mandatory)]$Lines)
    $rows = [ordered]@{}
    $inBaselines = $false
    $depth = 0
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $t = $Lines[$i].Text
        if (-not $inBaselines) {
            if ($t -match '"baselines"\s*:\s*\{') { $inBaselines = $true; $depth = 1 }
            continue
        }
        if ($t -match $script:LanePinsRowPattern) {
            $rows[$Matches[2]] = [PSCustomObject]@{
                LineIndex = $i
                Prefix    = $Matches[1]
                Value     = [int]$Matches[3]
                Suffix    = $Matches[4]
            }
        }
        $opens = ([regex]::Matches($t, '\{')).Count
        $closes = ([regex]::Matches($t, '\}')).Count
        $depth += ($opens - $closes)
        if ($depth -le 0) { break }
    }
    return $rows
}

function Resolve-LanePinsUnitBaselines {
    param([Parameter(Mandatory)][string]$RepoRoot, [Parameter(Mandatory)]$Blobs)

    $baseText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[1]
    $oursText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[2]
    $theirsText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[3]

    $baseLines = Split-LanePinsLines -Text $baseText
    $oursLines = Split-LanePinsLines -Text $oursText
    $theirsLines = Split-LanePinsLines -Text $theirsText

    $baseRows = Get-LanePinsBaselineRows -Lines $baseLines
    $oursRows = Get-LanePinsBaselineRows -Lines $oursLines
    $theirsRows = Get-LanePinsBaselineRows -Lines $theirsLines
    if ($theirsRows.Count -eq 0) {
        throw "[lane_pins] Tools/unit_baselines.json: no numeric baseline rows found on theirs' side"
    }

    # Start from THEIRS -- edit its own line list in place.
    $result = New-Object System.Collections.Generic.List[object]
    foreach ($l in $theirsLines) { $result.Add([PSCustomObject]@{ Text = $l.Text; Terminator = $l.Terminator }) }

    foreach ($name in $theirsRows.Keys) {
        $theirsRow = $theirsRows[$name]
        $newValue = $theirsRow.Value
        if ($oursRows.Contains($name) -and $baseRows.Contains($name)) {
            $delta = $oursRows[$name].Value - $baseRows[$name].Value
            $newValue = $theirsRow.Value + $delta
        }
        if ($newValue -ne $theirsRow.Value) {
            $result[$theirsRow.LineIndex].Text = "$($theirsRow.Prefix)$newValue$($theirsRow.Suffix)"
        }
    }

    return Join-LanePinsLines -Lines $result
}

# =============================================================================
# Games/<G>/Docs/Status.md: work from the ON-DISK conflicted file's own
# <<<<<<< / ======= / >>>>>>> markers (default conflict style; no |||||||
# section) -- never reconstruct hunks from stage blobs, EXCEPT for the LIVE
# PIN numbers themselves, which need a base value the on-disk two-way markers
# cannot show, so those four numbers alone come from the stage blobs' own
# (unconflicted, single-version) LIVE PIN pairs.
# =============================================================================

function Get-LanePinsConflictBlocks {
    # One entry per <<<<<<< ... ======= ... >>>>>>> span:
    # {StartIndex; OursStart; OursEnd; SepIndex; TheirsStart; TheirsEnd; EndIndex}
    # (Start > End means an empty side.)
    param([Parameter(Mandatory)]$Lines)
    $blocks = New-Object System.Collections.Generic.List[object]
    $i = 0
    while ($i -lt $Lines.Count) {
        if ($Lines[$i].Text -match '^<<<<<<< ') {
            $startIdx = $i
            $sepIdx = -1
            $endIdx = -1
            $j = $i + 1
            while ($j -lt $Lines.Count) {
                if ($sepIdx -lt 0 -and $Lines[$j].Text -match '^=======\s*$') { $sepIdx = $j }
                elseif ($Lines[$j].Text -match '^>>>>>>> ') { $endIdx = $j; break }
                $j++
            }
            if ($sepIdx -lt 0 -or $endIdx -lt 0) {
                throw "[lane_pins] malformed conflict markers starting at line $($startIdx + 1)"
            }
            $blocks.Add([PSCustomObject]@{
                    StartIndex  = $startIdx
                    OursStart   = $startIdx + 1
                    OursEnd     = $sepIdx - 1
                    SepIndex    = $sepIdx
                    TheirsStart = $sepIdx + 1
                    TheirsEnd   = $endIdx - 1
                    EndIndex    = $endIdx
                })
            $i = $endIdx + 1
        }
        else {
            $i++
        }
    }
    return $blocks
}

function Get-LanePinsBlockSlice {
    param([Parameter(Mandatory)]$Lines, [Parameter(Mandatory)][int]$Start, [Parameter(Mandatory)][int]$End)
    if ($Start -gt $End) { return , @() }
    return , @($Lines[$Start..$End])
}

function Get-LanePinsLivePinNumbersFromText {
    # The 4 numbers (ZM boot, engine boot, RenderTest, registry) from a
    # single, unconflicted, full document's first "LIVE PIN" line + the line
    # immediately after it -- $null if the shape is not there.
    param([Parameter(Mandatory)][string]$Text)
    $normalized = $Text -replace "`r`n", "`n" -replace "`r", "`n"
    $lines = $normalized -split "`n"
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match 'LIVE PIN') {
            if ($i + 1 -ge $lines.Count) { return $null }
            $numsLine = $lines[$i + 1]
            $found = [regex]::Matches($numsLine, '\d+')
            if ($found.Count -lt 4) { return $null }
            return @($found | Select-Object -First 4 | ForEach-Object { [int]$_.Value })
        }
    }
    return $null
}

function Set-LanePinsUpdatedDate {
    param([Parameter(Mandatory)][string]$Text, [Parameter(Mandatory)][string]$NewDate)
    return [regex]::Replace($Text, '(?<=\(UPDATED\s)[0-9]{4}-[0-9]{2}-[0-9]{2}(?=\))', [System.Text.RegularExpressions.MatchEvaluator] { param($m) $NewDate })
}

function Set-LanePinsDigitRunsInOrder {
    # Replaces each `\d+` run in $Text, in left-to-right order, with the
    # corresponding entry of $NewValues -- every other character (labels,
    # backticks, punctuation) is left untouched.
    param([Parameter(Mandatory)][string]$Text, [Parameter(Mandatory)][int[]]$NewValues)
    $found = [regex]::Matches($Text, '\d+')
    if ($found.Count -ne $NewValues.Count) {
        throw "[lane_pins] expected $($NewValues.Count) digit run(s) in '$Text', found $($found.Count)"
    }
    $sb = New-Object System.Text.StringBuilder
    $pos = 0
    for ($k = 0; $k -lt $found.Count; $k++) {
        $m = $found[$k]
        [void]$sb.Append($Text.Substring($pos, $m.Index - $pos))
        [void]$sb.Append([string]$NewValues[$k])
        $pos = $m.Index + $m.Length
    }
    [void]$sb.Append($Text.Substring($pos))
    return $sb.ToString()
}

function Test-LanePinsIsNumbersLine {
    # Structural detector for a LIVE PIN numbers line -- "boot `<digits>`"
    # plus a "registry" field, matched independently so either can appear
    # anywhere in the given text (a single line, or a whole block's combined
    # text). Matches the real Status.md numbers line and the test fixture's
    # synthetic one; never matches the header line alone (no "boot `" run).
    param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)
    return ($Text -match 'boot\s*`\d+') -and ($Text -match 'registry')
}

function Resolve-LanePinsStatusMd {
    param([Parameter(Mandatory)][string]$RepoRoot, [Parameter(Mandatory)][string]$RelPath, [Parameter(Mandatory)]$Blobs, [Parameter(Mandatory)][string]$TodayDate)

    $fullPath = Join-Path $RepoRoot ($RelPath -replace '/', '\')
    $onDiskText = [IO.File]::ReadAllText($fullPath)
    $lines = Split-LanePinsLines -Text $onDiskText

    # diff3/zdiff3 conflict style adds a '|||||||' base section that
    # Get-LanePinsConflictBlocks does not know how to skip -- it would be
    # silently absorbed into ours' slice. Refuse outright rather than
    # classify a block from garbled text.
    foreach ($l in $lines) {
        if ($l.Text -match '^\|{7}') {
            throw "[lane_pins] ${RelPath}: diff3/zdiff3-style conflict markers ('|||||||') are not supported"
        }
    }

    $headingIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Text -match '^## ') { $headingIdx = $i; break }
    }
    if ($headingIdx -lt 0) {
        throw "[lane_pins] ${RelPath}: no '## ' heading found; cannot verify every conflict sits above it"
    }

    $blocks = Get-LanePinsConflictBlocks -Lines $lines
    if ($blocks.Count -eq 0) {
        throw "[lane_pins] ${RelPath}: listed as conflicted but no conflict markers were found on disk"
    }
    foreach ($b in $blocks) {
        if ($b.EndIndex -ge $headingIdx) {
            throw "[lane_pins] ${RelPath}: conflict at line $($b.StartIndex + 1) does not lie above the first '## ' heading (line $($headingIdx + 1))"
        }
    }

    $baseText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[1]
    $oursText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[2]
    $theirsText = Get-LanePinsBlobText -RepoRoot $RepoRoot -Sha $Blobs[3]
    $baseLP = Get-LanePinsLivePinNumbersFromText -Text $baseText
    $oursLP = Get-LanePinsLivePinNumbersFromText -Text $oursText
    $theirsLP = Get-LanePinsLivePinNumbersFromText -Text $theirsText

    # The document's first LIVE PIN header line, if any -- when two lanes
    # bump pins the same day the header text is IDENTICAL on both sides, so
    # git narrows the conflict to the numbers line alone and the header
    # never appears inside any block's <<<<<<< / ======= / >>>>>>> span.
    $firstLivePinHeaderIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Text -match 'LIVE PIN \(UPDATED') { $firstLivePinHeaderIdx = $i; break }
    }

    # Work on a mutable copy of the whole line list; splice blocks from LAST to
    # FIRST so earlier blocks' indices stay valid across the edit.
    $result = New-Object System.Collections.Generic.List[object]
    foreach ($l in $lines) { $result.Add([PSCustomObject]@{ Text = $l.Text; Terminator = $l.Terminator }) }

    $livePinSeen = $false
    $lastUpdatedSeen = $false

    for ($bi = $blocks.Count - 1; $bi -ge 0; $bi--) {
        $b = $blocks[$bi]
        $oursSlice = Get-LanePinsBlockSlice -Lines $lines -Start $b.OursStart -End $b.OursEnd
        $theirsSlice = Get-LanePinsBlockSlice -Lines $lines -Start $b.TheirsStart -End $b.TheirsEnd
        $combinedText = (@($oursSlice + $theirsSlice) | ForEach-Object { $_.Text }) -join "`n"

        # Header-outside LIVE PIN shape: the line directly above this
        # block's <<<<<<< is the document's first LIVE PIN header line (so
        # the header itself sits OUTSIDE the conflict, identical on both
        # sides), and each side of the block is exactly one numbers line.
        $isHeaderOutsideLivePin = ($b.StartIndex -gt 0) -and
        (($b.StartIndex - 1) -eq $firstLivePinHeaderIdx) -and
        ($oursSlice.Count -eq 1) -and ($theirsSlice.Count -eq 1) -and
        (Test-LanePinsIsNumbersLine -Text $oursSlice[0].Text) -and
        (Test-LanePinsIsNumbersLine -Text $theirsSlice[0].Text)

        if ($combinedText -match '\*\*Last updated:\*\*') {
            if ($lastUpdatedSeen) {
                throw "[lane_pins] ${RelPath}: more than one 'Last updated' conflict block"
            }
            $lastUpdatedSeen = $true
            $template = if ($theirsSlice.Count -gt 0) { $theirsSlice[$theirsSlice.Count - 1] } else { $oursSlice[$oursSlice.Count - 1] }
            if ($null -eq $template -or $template.Text -notmatch '^(\*\*Last updated:\*\*\s*).*$') {
                throw "[lane_pins] ${RelPath}: could not parse the 'Last updated' line template"
            }
            $newText = $Matches[1] + $TodayDate
            $replacement = , ([PSCustomObject]@{ Text = $newText; Terminator = $template.Terminator })
        }
        elseif ($combinedText -match 'LIVE PIN') {
            if ($livePinSeen) {
                throw "[lane_pins] ${RelPath}: more than one LIVE PIN conflict block"
            }
            $livePinSeen = $true
            if ($theirsSlice.Count -lt 2) {
                throw "[lane_pins] ${RelPath}: LIVE PIN conflict block has fewer than 2 lines on theirs' side"
            }
            if ($null -eq $baseLP -or $null -eq $oursLP -or $null -eq $theirsLP -or
                $baseLP.Count -ne 4 -or $oursLP.Count -ne 4 -or $theirsLP.Count -ne 4) {
                throw "[lane_pins] ${RelPath}: could not extract exactly 4 LIVE PIN numbers from base/ours/theirs"
            }
            $headerLine = $theirsSlice[0]
            $numbersLine = $theirsSlice[1]
            $newNumbers = @(0, 1, 2, 3 | ForEach-Object { $theirsLP[$_] + ($oursLP[$_] - $baseLP[$_]) })
            $newHeaderText = Set-LanePinsUpdatedDate -Text $headerLine.Text -NewDate $TodayDate
            $newNumbersText = Set-LanePinsDigitRunsInOrder -Text $numbersLine.Text -NewValues $newNumbers
            $replacement = @(
                [PSCustomObject]@{ Text = $newHeaderText; Terminator = $headerLine.Terminator },
                [PSCustomObject]@{ Text = $newNumbersText; Terminator = $numbersLine.Terminator }
            )
        }
        elseif ($isHeaderOutsideLivePin) {
            if ($livePinSeen) {
                throw "[lane_pins] ${RelPath}: more than one LIVE PIN conflict block"
            }
            $livePinSeen = $true
            if ($null -eq $baseLP -or $null -eq $oursLP -or $null -eq $theirsLP -or
                $baseLP.Count -ne 4 -or $oursLP.Count -ne 4 -or $theirsLP.Count -ne 4) {
                throw "[lane_pins] ${RelPath}: could not extract exactly 4 LIVE PIN numbers from base/ours/theirs"
            }
            $numbersLine = $theirsSlice[0]
            $newNumbers = @(0, 1, 2, 3 | ForEach-Object { $theirsLP[$_] + ($oursLP[$_] - $baseLP[$_]) })
            $newNumbersText = Set-LanePinsDigitRunsInOrder -Text $numbersLine.Text -NewValues $newNumbers
            $replacement = , ([PSCustomObject]@{ Text = $newNumbersText; Terminator = $numbersLine.Terminator })
            # The header line sits OUTSIDE this block, directly above it --
            # safe to mutate $result by absolute index here because blocks
            # are processed in DESCENDING StartIndex order, so every index
            # below the current block's StartIndex is still aligned with the
            # original $lines.
            $result[$b.StartIndex - 1].Text = Set-LanePinsUpdatedDate -Text $result[$b.StartIndex - 1].Text -NewDate $TodayDate
        }
        elseif (Test-LanePinsIsNumbersLine -Text $combinedText) {
            # Looks like a LIVE PIN numbers-line conflict but not in a shape
            # this script knows how to resolve (e.g. the numbers line and an
            # adjacent line changed together in one hunk) -- refuse rather
            # than guess.
            throw "[lane_pins] ${RelPath}: LIVE PIN numbers-line conflict at line $($b.StartIndex + 1) does not match a supported shape"
        }
        else {
            # Every other conflict block (e.g. the +N history entries): union
            # -- ours' lines, then theirs' lines, verbatim. When ours ends
            # with a Markdown blockquote paragraph and theirs begins with
            # one, insert a blank line between them so they don't render as
            # one glued paragraph.
            if ($oursSlice.Count -gt 0 -and $theirsSlice.Count -gt 0 -and
                $oursSlice[$oursSlice.Count - 1].Text -match '^>' -and
                $theirsSlice[0].Text -match '^>') {
                $blankLine = [PSCustomObject]@{ Text = ''; Terminator = $oursSlice[$oursSlice.Count - 1].Terminator }
                $replacement = @($oursSlice + , $blankLine + $theirsSlice | ForEach-Object { [PSCustomObject]@{ Text = $_.Text; Terminator = $_.Terminator } })
            }
            else {
                $replacement = @($oursSlice + $theirsSlice | ForEach-Object { [PSCustomObject]@{ Text = $_.Text; Terminator = $_.Terminator } })
            }
        }

        $count = $b.EndIndex - $b.StartIndex + 1
        $result.RemoveRange($b.StartIndex, $count)
        if ($replacement.Count -gt 0) {
            $result.InsertRange($b.StartIndex, [object[]]$replacement)
        }
    }

    return Join-LanePinsLines -Lines $result
}

# =============================================================================
# Main.
# =============================================================================

function Invoke-LanePins {
    param([Parameter(Mandatory)][string]$RepoRoot)

    if (-not (Test-LanePinsMergeInProgress -RepoRoot $RepoRoot)) {
        Write-LanePinsFail "no MERGE_HEAD in '$RepoRoot' -- nothing to resolve"
        return 2
    }

    $conflicted = Get-LanePinsConflictedPaths -RepoRoot $RepoRoot
    if ($conflicted.Count -eq 0) {
        Write-LanePinsFail "MERGE_HEAD is present but no conflicted paths were reported"
        return 2
    }

    $disallowed = @($conflicted | Where-Object { -not (Test-LanePinsAllowedPath -Path $_) })
    if ($disallowed.Count -gt 0) {
        Write-LanePinsFail "conflict(s) outside the allowed pin sites: $($disallowed -join ', ')"
        return 2
    }

    foreach ($path in $conflicted) {
        $n = Get-LanePinsStageCount -RepoRoot $RepoRoot -Path $path
        if ($n -ne 3) {
            Write-LanePinsFail "'$path' does not have exactly 3 stage entries (found $n) -- looks like a delete/modify conflict"
            return 2
        }
    }

    $today = Get-Date -Format 'yyyy-MM-dd'
    $resolvedTexts = @{}

    try {
        foreach ($path in $conflicted) {
            $blobs = Get-LanePinsStageBlobShas -RepoRoot $RepoRoot -Path $path
            if (-not ($blobs.Contains(1) -and $blobs.Contains(2) -and $blobs.Contains(3))) {
                throw "[lane_pins] '$path': could not read all three stage blob shas"
            }
            $normalized = $path -replace '\\', '/'
            if ($normalized -eq 'Tools/unit_baselines.json') {
                $resolvedTexts[$path] = Resolve-LanePinsUnitBaselines -RepoRoot $RepoRoot -Blobs $blobs
            }
            else {
                $resolvedTexts[$path] = Resolve-LanePinsStatusMd -RepoRoot $RepoRoot -RelPath $normalized -Blobs $blobs -TodayDate $today
            }
        }
    }
    catch {
        Write-LanePinsFail $_.Exception.Message
        return 2
    }

    # Nothing written until every conflict is known resolvable.
    foreach ($path in $conflicted) {
        $fullPath = Join-Path $RepoRoot ($path -replace '/', '\')
        Write-LanePinsFileUtf8NoBom -Path $fullPath -Text $resolvedTexts[$path]
    }

    Push-Location -LiteralPath $RepoRoot
    try {
        foreach ($path in $conflicted) {
            & git add -- $path
            if ($LASTEXITCODE -ne 0) {
                Write-LanePinsFail "'git add -- $path' failed after the file was written -- the working tree may be in a partial state"
                return 2
            }
        }
    }
    finally { Pop-Location }

    Write-Host "[lane_pins] resolved $($conflicted.Count) conflict(s): $($conflicted -join ', ')" -ForegroundColor Green
    return 0
}

$Repo = (Resolve-Path -LiteralPath $Repo).Path
exit (Invoke-LanePins -RepoRoot $Repo)
