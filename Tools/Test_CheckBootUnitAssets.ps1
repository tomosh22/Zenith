# Test_CheckBootUnitAssets.ps1 -- assertions for Tools/check_boot_unit_assets.ps1's
# pure helpers.
#
# A plain assert script, matching Tools/Test_TickGates.ps1 and
# Tools/zagent/Test-ZagentClient.ps1: the only PowerShell this repo requires is
# the `pwsh` every gate already needs.
#
# EVERY assertion below carries a comment naming the SINGLE source change that
# would make it fail. An assertion nobody can mutation-check is decoration.
#
# The three things it exists to hold down are the three that bit while the lint
# was being written:
#
#   * PowerShell hashtables are CASE-INSENSITIVE, so a name->path map built with
#     `@{}` resolves `GetView<T>(szPath)` against a constant named `szPATH` --
#     a different C++ symbol in a different file, i.e. a fabricated finding.
#   * Short constant names COLLIDE across files (`szPATH`, `szASSET`), and a
#     global map silently keeps whichever file was scanned last.
#   * A lint that scans ZERO files reports a CLEAN TREE. Every empty-scan path
#     must throw.
#
# Usage: pwsh -NoProfile -File Tools/Test_CheckBootUnitAssets.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
# -NoRun: dot-sourcing a script RUNS it, and this harness must not scan the live
# checkout (slow, and its result would depend on the tree rather than on the
# fixtures). Same shape as Test_TickGates.ps1's dot-source of tick_gates.ps1.
. (Join-Path $PSScriptRoot 'check_boot_unit_assets.ps1') -RepoRoot $repoRoot -NoRun

$script:failures = 0
$script:count = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    $ok = $false
    try { $ok = [bool](& $Body) } catch { Write-Host "       threw: $($_.Exception.Message)" -ForegroundColor Yellow }
    if ($ok) { Write-Host "  ok   $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL $Name" -ForegroundColor Red; $script:failures++ }
}

function New-Map { param([string[]]$Texts) return (New-AssetPathConstantMap -Texts $Texts) }

Write-Host "`n=== Test-IsCandidateBootUnitFile ===" -ForegroundColor Cyan

Assert-That 'a .Tests.inl suite is a candidate' {
    # Fails if the `\.Tests\.inl$` branch is removed -- which would drop every
    # engine unit suite in the repo and report a clean tree.
    Test-IsCandidateBootUnitFile 'Games/RenderTest/Components/RenderTest_Tennis.Tests.inl'
}

Assert-That 'a game Tests/*.cpp automated test is a candidate' {
    # Fails if the `(^|/)Tests/` directory branch is removed.
    Test-IsCandidateBootUnitFile 'Games/RenderTest/Tests/SceneAssetIntegrity.cpp'
}

Assert-That '** a production .cpp is NOT a candidate -- the scan is scoped to tests' {
    # Fails the moment the .cpp branch stops requiring a Tests/ directory
    # segment, which would pull every engine TU into the scan.
    -not (Test-IsCandidateBootUnitFile 'Zenith/Flux/Vegetation/Flux_Grass.cpp')
}

Assert-That 'a plain header is NOT a candidate, even though pass 1 reads it' {
    # Pass 1 scans ALL tracked source for constants; pass 2 must not. Fails if
    # the two file sets are ever collapsed into one.
    -not (Test-IsCandidateBootUnitFile 'Games/RenderTest/Components/RenderTest_TennisAgentComponent.h')
}

Write-Host "`n=== New-AssetPathConstantMap (pass 1) ===" -ForegroundColor Cyan

Assert-That 'resolves the exact declaration this ticket turns on' {
    # `static constexpr const char* kszGraphAsset = "game:..."`, verbatim from
    # RenderTest_TennisAgentComponent.h:65. Fails if ConstantRegex ever requires
    # `constexpr` to sit immediately before `char` (it sits before `const` here).
    $r = New-Map @('	static constexpr const char* kszGraphAsset = "game:Graphs/RenderTest_TennisBrain.bgraph";')
    ($r.Map.Count -eq 1) -and ($r.Map['kszGraphAsset'] -eq 'game:Graphs/RenderTest_TennisBrain.bgraph')
}

Assert-That 'resolves a plain `const char*` too, not only a constexpr one' {
    # Fails if the `(?:constexpr|const)` alternation loses its `const` branch.
    $r = New-Map @('const char* szThing = "engine:Meshes/Thing.zmesh";')
    $r.Map['szThing'] -eq 'engine:Meshes/Thing.zmesh'
}

Assert-That '** THE MAP IS ORDINAL: szPath and szPATH stay two symbols' {
    # ★ The trap. Fails the moment the Dictionary is replaced by `@{}` or by a
    # Dictionary built without StringComparer::Ordinal: a case-insensitive map
    # sees the second declaration as a redefinition of the first, marks the name
    # colliding and drops BOTH -- so Map.Count would be 0 and Collisions 1.
    # These are real symbols in this repo (Zenith_AssetRegistry.Tests.inl:171
    # reads `szPath`; Zenith_EditorAutomation.Tests.inl:826 reads `szPATH`).
    $r = New-Map @(
        'const char* szPath = "game:Graphs/Lower.bgraph";',
        'const char* szPATH = "game:Graphs/Upper.bgraph";'
    )
    ($r.Collisions.Count -eq 0) -and ($r.Map.Count -eq 2) -and
    ($r.Map['szPath'] -eq 'game:Graphs/Lower.bgraph') -and
    ($r.Map['szPATH'] -eq 'game:Graphs/Upper.bgraph')
}

Assert-That '** ONE NAME, TWO PATHS -> UNRESOLVABLE: dropped AND recorded' {
    # ★ The second trap. Fails if the collision branch is changed to keep the
    # first or the last writer -- which is exactly how a lint invents a finding
    # that names a file having nothing to do with the call site.
    $r = New-Map @(
        'const char* szASSET = "game:Graphs/A.bgraph";',
        'const char* szASSET = "game:Graphs/B.bgraph";'
    )
    ($r.Map.Count -eq 0) -and ($r.Collisions.Count -eq 1) -and ($r.Collisions.Contains('szASSET'))
}

Assert-That 'the SAME name with the SAME path in two files is NOT a collision' {
    # A header included twice, or an identical constant in two suites, must stay
    # resolvable. Fails if collision detection ever keys on "seen twice" rather
    # than on the PATH differing.
    $r = New-Map @(
        'const char* szSame = "game:Graphs/Same.bgraph";',
        'const char* szSame = "game:Graphs/Same.bgraph";'
    )
    ($r.Collisions.Count -eq 0) -and ($r.Map['szSame'] -eq 'game:Graphs/Same.bgraph')
}

Assert-That 'two paths differing only in CASE are still a collision' {
    # Fails if the `-cne` path comparison is relaxed to `-ne`. Asset paths are
    # compared byte-for-byte by Zenith_AssetRegistry::NormalizeAssetPath's
    # consumers, so two casings are two different assets to this lint too.
    $r = New-Map @(
        'const char* szCase = "game:Graphs/Thing.bgraph";',
        'const char* szCase = "game:Graphs/THING.bgraph";'
    )
    ($r.Collisions.Count -eq 1) -and ($r.Map.Count -eq 0)
}

Assert-That 'a third sighting of an already-colliding name does not resurrect it' {
    # Fails if the `$collisions.Contains($name)` early-continue is removed: the
    # third declaration would re-insert the name into the map.
    $r = New-Map @(
        'const char* szTri = "game:A.bgraph";',
        'const char* szTri = "game:B.bgraph";',
        'const char* szTri = "game:C.bgraph";'
    )
    ($r.Map.Count -eq 0) -and ($r.Collisions.Count -eq 1)
}

Assert-That '** the INCREMENTAL fold is the path the run block takes, and matches' {
    # ★ Every assertion above drives New-AssetPathConstantMap, but the run block
    # folds one file at a time through Add-AssetPathConstantsFromText so the whole
    # corpus is never resident. Without this row the TESTED code and the SHIPPED
    # code could diverge silently. Fails if New-AssetPathConstantMap ever grows
    # its own copy of the collision logic instead of delegating to the fold.
    $texts = @(
        'const char* szKeep = "game:Graphs/Keep.bgraph";',
        'const char* szDup  = "game:Graphs/A.bgraph";',
        'const char* szDup  = "game:Graphs/B.bgraph";',
        "class Holder`n{`n	static constexpr const char* kszM = `"game:Graphs/M.bgraph`";`n};"
    )
    $acc = New-AssetPathConstantAccumulator
    foreach ($t in $texts) { Add-AssetPathConstantsFromText -Accumulator $acc -Text $t }
    $agg = New-Map $texts
    ($acc.Map.Count -eq $agg.Map.Count) -and ($acc.Map.Count -eq 2) -and
    ($acc.Map['szKeep'] -eq $agg.Map['szKeep']) -and
    ($acc.Collisions.Count -eq $agg.Collisions.Count) -and ($acc.Collisions.Contains('szDup')) -and
    ($acc.QualifiedMap.Count -eq $agg.QualifiedMap.Count) -and ($acc.QualifiedMap.Count -eq 1) -and
    ($acc.QualifiedMap['Holder::kszM'] -eq 'game:Graphs/M.bgraph')
}

Assert-That 'a fresh accumulator is empty in ALL FOUR collections' {
    # Fails if New-AssetPathConstantAccumulator is ever seeded with defaults,
    # which would resolve names no file in the tree actually declares -- and
    # fails if a collection is dropped from it (a missing QualifiedMap would
    # throw here rather than silently disabling qualified resolution).
    $acc = New-AssetPathConstantAccumulator
    ($acc.Map.Count -eq 0) -and ($acc.Collisions.Count -eq 0) -and
    ($acc.QualifiedMap.Count -eq 0) -and ($acc.QualifiedCollisions.Count -eq 0)
}

Write-Host "`n=== Get-AssetPathReadFindings (pass 2) -- what IS a read ===" -ForegroundColor Cyan

$emptyMap = (New-Map @()).Map

Assert-That 'a LITERAL path passed to a reader is flagged' {
    # The base case. Fails if AddGraphByAssetPath leaves the reader list.
    $t = 'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Brain.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].AssetPath -eq 'game:Graphs/Brain.bgraph') -and
    ($f[0].Reader -eq 'AddGraphByAssetPath') -and ($f[0].Via -eq 'literal')
}

Assert-That 'a GetView<T> read is flagged, template arguments and all' {
    # Fails if the `GetView\s*<[^<>;{}()]*>` alternative is narrowed so it no
    # longer spans a type argument.
    $t = 'Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>("game:Graphs/X.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].Reader -eq 'GetView<Zenith_BehaviourGraphAsset>')
}

Assert-That 'an Acquire<T> read is flagged -- the PREFERRED owning loader' {
    # Zenith_AssetHandle.h:334 -> AcquireInternal (Zenith_AssetRegistry.cpp:611)
    # -> the SAME GetInternal as GetView, plus an AddRef. Zero literal-path call
    # sites today, so this changes no finding -- it is here because
    # AssetHandling/CLAUDE.md documents Acquire<T> as the preferred loader, and a
    # future boot unit written with the recommended API must not be invisible.
    # Fails if the Acquire alternative leaves ReaderRegex.
    $t = 'MaterialHandle xH = Zenith_AssetRegistry::Acquire<Zenith_MaterialAsset>("game:Materials/M.zmtrl");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].Reader -eq 'Acquire<Zenith_MaterialAsset>')
}

Assert-That '** A `SetPath` MENTION IS NOT A READ -- the 243-finding regression' {
    # ★ The whole reason this lint was rewritten. `SetPath` has 335 call sites
    # across the candidate files and touches no disk. Fails the moment any
    # in-memory setter is added to the reader list.
    # ★ BOTH LINES PUT THE PATH FIRST, which is what makes them discriminating:
    # ReaderRegex only matches a FIRST argument, so a third-argument setter
    # (`SetTexturePath(2u, MAP_ALBEDO, "game:...")`, the real spelling) would be
    # ignored by position rather than by the reader list, and the row would pass
    # even with that function ADDED as a reader. Fails iff a non-reader is added
    # to the list.
    $t = 'xTable.SetTexturePath("game:Vegetation/Vein.ztxtr", 2u, MAP_ALBEDO);' + "`n" +
         'xBuilder.SetGraphName("game:Graphs/zz_unit_named.bgraph");' + "`n" +
         'xModel.SetSkeletonPath("game:Skeletons/rig.zskel");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '** `AddMeshByPath` IS NOT A READ -- it is a SetPath with a longer name' {
    # Zenith_ModelAsset.cpp:236 is `xBinding.m_xMesh.SetPath(strMeshPath)` and a
    # PushBack. It was on the proposed reader list and was dropped after reading
    # it. Fails if anyone puts it back without re-reading the implementation.
    $t = 'pxModel->AddMeshByPath("game:Meshes/body.zmesh", xMats);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '** `OpenAssetFresh` IS NOT A READ -- it opens and then Clear()s' {
    # Zenith_EditorPanel_GraphEditor.cpp:2013 is the boot-REGENERATION entry
    # point: it discards whatever was on disk. Fails if the `OpenAsset`
    # alternative ever stops requiring a `(` immediately after the name (e.g. a
    # bare substring grep), which would match OpenAssetFresh and GetOpenAssetPath.
    $t = 'Zenith_GraphEditorPanel::OpenAssetFresh("game:Graphs/Regen.bgraph");' + "`n" +
         'const char* p = Zenith_GraphEditorPanel::GetOpenAssetPath();'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That 'a plain `OpenAsset` call IS a read, so the exclusion above is not blanket' {
    # Pairs with the previous assertion: if `OpenAsset` were dropped from the
    # reader list entirely, that one would pass vacuously. This one fails then.
    $t = 'Zenith_GraphEditorPanel::OpenAsset("game:Graphs/Real.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].Reader -eq 'OpenAsset')
}

Write-Host "`n=== Get-AssetPathReadFindings -- constant resolution at the call site ===" -ForegroundColor Cyan

Assert-That '** a CONSTANT-resolved read is flagged, and names the constant' {
    # ★ The motivating case: RenderTest_Tennis.Tests.inl never spells the
    # literal -- it passes RenderTest_TennisAgentComponent::kszGraphAsset, whose
    # literal lives in a header. Fails if pass 1's map stops being consulted at
    # the call site, i.e. the lint goes back to seeing literals only.
    $r = New-Map @('	static constexpr const char* kszGraphAsset = "game:Graphs/RenderTest_TennisBrain.bgraph";')
    $t = 'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(kszGraphAsset);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'RenderTest_Tennis.Tests.inl' -ConstantMap $r.Map
    (@($f).Count -eq 1) -and
    ($f[0].AssetPath -eq 'game:Graphs/RenderTest_TennisBrain.bgraph') -and
    ($f[0].Via -eq 'kszGraphAsset')
}

Assert-That '** the QUALIFIED, LINE-WRAPPED call site resolves (the real shape)' {
    # RenderTest_Tennis.Tests.inl:1599-1600 wraps the argument onto the next line
    # AND qualifies it with the class name. Fails if `\s*` after the `(` is
    # tightened to `[ \t]*` (a newline would stop matching), or if the
    # `(?:Ident\s*::\s*)*` qualifier prefix is dropped from the identifier
    # alternative.
    $r = New-Map @("class RenderTest_TennisAgentComponent`n{`npublic:`n	static constexpr const char* kszGraphAsset = `"game:Graphs/RenderTest_TennisBrain.bgraph`";`n};")
    $t = "xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(`n" +
         "	RenderTest_TennisAgentComponent::kszGraphAsset);"
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'RenderTest_Tennis.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and ($f[0].Via -eq 'RenderTest_TennisAgentComponent::kszGraphAsset')
}

Write-Host "`n=== class-scoped constants (the kszGraphAsset convention) ===" -ForegroundColor Cyan
# ★ EIGHT components each declare `static constexpr const char* kszGraphAsset`:
# RenderTest_TennisAgentComponent, DPDoor_Component, DPForge_Component,
# DPItemBase_Component, DPPauseMenuController_Component,
# DPPlayerController_Component, DPVillager_Component, Priest_Component. They are
# eight distinct C++ symbols, every use site is qualified, and a bare-name map
# saw one name with eight paths and skipped it -- which made this lint blind to
# the very line ZEN-84 exists for, for a brand new reason.

function New-ClassScopedFixture {
    # Two classes, same member name, different paths -- the real shape.
    return @(
        "class RenderTest_TennisAgentComponent`n{`npublic:`n	static constexpr const char* kszGraphAsset = `"game:Graphs/RenderTest_TennisBrain.bgraph`";`n};",
        "class DPDoor_Component`n{`npublic:`n	static constexpr const char* kszGraphAsset = `"game:Graphs/DP_Door.bgraph`";`n};"
    )
}

Assert-That '** a QUALIFIED use resolves to ITS OWN class''s path' {
    # ★ The fix. Fails if the qualified map is removed, if the use site stops
    # consulting it BEFORE the bare map, or if Get-EnclosingTypeName stops
    # returning the innermost class -- any of which puts the ticket's own case
    # back in the skipped list.
    $r = New-Map (New-ClassScopedFixture)
    $t = 'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(RenderTest_TennisAgentComponent::kszGraphAsset);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and
    ($f[0].AssetPath -eq 'game:Graphs/RenderTest_TennisBrain.bgraph') -and
    ($f[0].Via -eq 'RenderTest_TennisAgentComponent::kszGraphAsset')
}

Assert-That '** the SIBLING class resolves to the OTHER path -- not the first seen' {
    # The discriminating half: with a single shared key, or with "keep the first
    # writer", this would come back with the RenderTest path. Fails if the
    # qualified key ever stops including the class name.
    $r = New-Map (New-ClassScopedFixture)
    $t = 'xDoor.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(DPDoor_Component::kszGraphAsset);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and ($f[0].AssetPath -eq 'game:Graphs/DP_Door.bgraph')
}

Assert-That 'the BARE name stays correctly ambiguous, and a bare use is still skipped' {
    # The bare-name rule is UNCHANGED, deliberately: `kszGraphAsset` spelled bare
    # really does have eight meanings. Fails if class-scoped constants were made
    # to bypass the bare collision set -- which would let an unqualified use
    # resolve to whichever class was scanned last.
    $r = New-Map (New-ClassScopedFixture)
    $t = 'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(kszGraphAsset);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    ($r.Collisions.Contains('kszGraphAsset')) -and ($r.QualifiedMap.Count -eq 2) -and (@($f).Count -eq 0)
}

Assert-That 'a UNIQUE bare name still resolves unqualified -- no working finding lost' {
    # The regression guard for szZENITH_GRASS_TYPE_TABLE_ASSET_PATH /
    # szTOUR_ASSET_PATH, which resolve today. Fails if bare names were switched
    # to file-scoped-or-qualified-only while adding the qualified map.
    $r = New-Map @('	static constexpr const char* szTOUR_ASSET_PATH = "game:Tour/Thing.zmesh";')
    $t = 'GetView<Zenith_MeshAsset>(szTOUR_ASSET_PATH);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and ($f[0].Via -eq 'szTOUR_ASSET_PATH')
}

Assert-That 'a qualified use with NO class key falls back to the bare map' {
    # A namespace-qualified constant has no enclosing class, so it has no
    # qualified key. Fails if the fallback is removed -- which would drop every
    # `SomeNamespace::szConst` use site that resolves today.
    $r = New-Map @('const char* szNsConst = "game:Graphs/Ns.bgraph";')
    $t = 'GetView<A>(SomeNamespace::szNsConst);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and ($f[0].Via -eq 'szNsConst')
}

Assert-That 'a QUALIFIED key that genuinely collides is skipped under the same rule' {
    # Same class name in two headers with different paths: still ambiguous, so
    # still skipped. Fails if the qualified map were given its own weaker rule
    # (e.g. last-writer-wins) instead of sharing Add-OneConstant with the bare
    # map -- which is the whole reason the collision logic is one function.
    $r = New-Map @(
        "class Dup`n{`n	static constexpr const char* kszG = `"game:Graphs/One.bgraph`";`n};",
        "class Dup`n{`n	static constexpr const char* kszG = `"game:Graphs/Two.bgraph`";`n};"
    )
    $t = 'GetView<A>(Dup::kszG);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    ($r.QualifiedCollisions.Contains('Dup::kszG')) -and ($r.QualifiedMap.Count -eq 0) -and (@($f).Count -eq 0)
}

Assert-That '** a FORWARD DECLARATION is not an enclosing type' {
    # The forward declaration sits AFTER the closed class ON PURPOSE, so it is
    # the last `class ...` line before the constant and the ONLY thing the
    # lookahead can be tested against. Fails iff the `(?![^\r\n]*;)` lookahead
    # is dropped, at which point `class Zenith_Entity;` becomes the enclosing
    # type and every constant below a forward declaration is keyed under a name
    # no use site spells.
    #
    # (An earlier version of this row put the forward declaration FIRST, which
    # made it vacuous for the mutation its own comment named: `class Closed` was
    # the last open either way and the column-0 `};` decided the answer, so
    # dropping the lookahead did not red it.)
    $t = "class Closed`n{`n};`nclass Zenith_Entity;`nconst char* szFree = `"game:A.zmesh`";"
    $idx = $t.IndexOf('szFree')
    (Get-EnclosingTypeName -Text $t -Index $idx) -eq ''
}

Assert-That 'a CLOSED class is not an enclosing type either -- the other half' {
    # The close-scan half, now tested separately from the lookahead half. Fails
    # if the column-0 `};` scan is removed, at which point a constant AFTER a
    # class is keyed as though it were inside it.
    $t = "class Closed`n{`n};`nconst char* szFree = `"game:A.zmesh`";"
    $idx = $t.IndexOf('szFree')
    (Get-EnclosingTypeName -Text $t -Index $idx) -eq ''
}

Assert-That 'Get-EnclosingTypeName finds the class a member actually sits in' {
    # Pairs with the previous row so it cannot pass by the function always
    # returning ''. Fails if TypeOpenRegex stops capturing the type name.
    $t = "class Holder`n{`npublic:`n	static constexpr const char* kszX = `"game:A.zmesh`";`n};"
    $idx = $t.IndexOf('kszX')
    (Get-EnclosingTypeName -Text $t -Index $idx) -eq 'Holder'
}

Assert-That 'a `struct` encloses just as a `class` does' {
    # Fails if the `(?:class|struct)` alternation loses its struct branch.
    $t = "struct Holder2`n{`n	static constexpr const char* kszY = `"game:B.zmesh`";`n};"
    $idx = $t.IndexOf('kszY')
    (Get-EnclosingTypeName -Text $t -Index $idx) -eq 'Holder2'
}

Write-Host "`n=== back to unqualified resolution ===" -ForegroundColor Cyan

Assert-That '** A COLLIDING NAME IS SKIPPED, NOT GUESSED' {
    # ★ Pass 1 dropped the name, so pass 2 must find nothing rather than reach
    # for a neighbour's path. Fails if the call site ever falls back to "any
    # constant with this name" or to a case-insensitive lookup.
    $r = New-Map @(
        'const char* szASSET = "game:Graphs/A.bgraph";',
        'const char* szASSET = "game:Graphs/B.bgraph";'
    )
    $t = 'Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(szASSET);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $r.Map
    @($f).Count -eq 0
}

Assert-That 'an UNRESOLVABLE identifier (a runtime std::string) is skipped silently' {
    # The deliberate false negative. Fails if an unresolved name is ever
    # reported with a null/empty AssetPath instead of being dropped.
    $t = 'Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(strMeshPath);'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Write-Host "`n=== Get-AssetPathReadFindings -- the TRACKED-asset exclusions ===" -ForegroundColor Cyan

Assert-That '** a .zscen read is NOT flagged -- .gitignore re-includes it' {
    # .gitignore:137 `!**/*.zscen`. A tracked file is present on a fresh clone,
    # so reading one is not order-dependent. Fails if 'zscen' leaves
    # $script:TrackedExtensions.
    $t = 'Zenith_AssetRegistry::GetView<Zenith_Prefab>("game:Scenes/RenderTest.zscen");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '.znavmesh and .ztexdecl are excluded for the same reason' {
    # .gitignore:136 and :138. Fails if either leaves $script:TrackedExtensions.
    $t = 'GetView<A>("game:Nav/Town.znavmesh"); GetView<B>("engine:TextureUsage.ztexdecl");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '** an Assets/Authored/ path is NOT flagged -- re-included WHOLESALE' {
    # .gitignore:139 `!**/Assets/Authored/**`, scoped by DIRECTORY not extension,
    # so a .zanim under it IS tracked while one anywhere else is not.
    #
    # ★ This caught a PRE-EXISTING defect with no coverage (inherited from the
    # first version of this script): `Authored` here is preceded by the root
    # prefix's COLON, so the old `(^|[\\/])Authored` matched nothing at all and
    # every committed authored clip read was reported. Fails if `:` leaves the
    # leading alternation of Test-IsTrackedAssetPath's regex.
    $t = 'GetView<Zenith_AnimationAsset>("engine:Authored/Meshes/StickFigure/Walk.zanim");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '** but `AuthoredVein.ztxtr` IS flagged -- the : fix must not over-match' {
    # The discriminating half of the pair. `game:Vegetation/AuthoredVein.ztxtr`
    # is a real gitignored bake output whose FILENAME merely begins "Authored";
    # it is not under an Authored/ directory. Fails if the trailing `([\\/]|$)`
    # separator requirement is dropped while widening the leading alternation,
    # which would silently excuse a whole family of genuine findings.
    $t = 'GetView<Zenith_TextureAsset>("game:Vegetation/AuthoredVein.ztxtr");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].AssetPath -eq 'game:Vegetation/AuthoredVein.ztxtr')
}

Assert-That 'a `game:`-rooted Authored path is excluded too, not just `engine:`' {
    # Both roots carry Authored/ (Zenith_AnimationDocument::PromoteToAuthoredOverride
    # keeps the source's prefix). Fails if the exclusion is ever special-cased to
    # one prefix.
    $t = 'GetView<Zenith_AnimationAsset>("game:Authored/Meshes/Npc/Wave.zanim");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That 'the SAME .zanim OUTSIDE Authored/ IS flagged' {
    # Pairs with the previous assertion so it cannot pass by `.zanim` having
    # quietly become a tracked extension. Fails if 'zanim' is ever added to
    # $script:TrackedExtensions.
    $t = 'GetView<Zenith_AnimationAsset>("engine:Meshes/StickFigure/Walk.zanim");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].AssetPath -eq 'engine:Meshes/StickFigure/Walk.zanim')
}

Assert-That 'an extension-less, directory-style reference is not flagged' {
    # `ResolvePath("game:Authored")`-shaped strings name a directory, not a file.
    # Fails if the IsNullOrEmpty($ext) guard is removed.
    $t = 'GetView<Zenith_Prefab>("game:Graphs");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Write-Host "`n=== producer detection (file granularity) ===" -ForegroundColor Cyan

Assert-That '** a file with a Save() signature is CLEAN -- HasProducer is true' {
    # This is the property that makes clause 2 of ZEN-84 demonstrable: the tennis
    # fixture's EnsureTennisBrainGraphAssetAuthored() calls
    # Zenith_AssetRegistry::Save, so its read is covered. Fails if ProducerRegex
    # stops matching the qualified `Zenith_AssetRegistry::Save(` spelling.
    $t = 'Zenith_AssetRegistry::Save(pxAsset, "game:Graphs/Brain.bgraph");' + "`n" +
         'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Brain.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].HasProducer -eq $true)
}

Assert-That '** REMOVE the Save and the SAME read becomes a finding' {
    # ★ The mutation the whole lint exists to catch, exercised directly: the two
    # texts differ by one line. Fails if HasProducer is ever hard-coded, or if
    # the run block stops keying its finding set on it.
    $t = 'xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Brain.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].HasProducer -eq $false)
}

Assert-That 'Test-HasProducerSignature tolerates whitespace around the ::' {
    # Fails if ProducerRegex loses its `\s*` around `::` and `(`.
    (Test-HasProducerSignature 'Zenith_AssetRegistry :: Save (pxAsset);') -and
    (-not (Test-HasProducerSignature 'pxModel->Export(strPath);'))
}

Assert-That '** signature 2: a queued write step AND a drain is a producer' {
    # Zenith/Editor/TerrainEditor/Zenith_TerrainEditor.Tests.inl:961-966 primes
    # game:Vegetation/GrassTypes.zdata before reading it at :972 and :1010 --
    # already the FIXED shape of this ticket's defect, reached through the
    # automation queue instead of through Save. Fails if the automation
    # signature is removed, putting those two false positives back.
    $t = "Zenith_EditorAutomation xPrime;`nxPrime.AddStep_GrassTypesSave();`nRunAutomation(xPrime);"
    Test-HasProducerSignature $t
}

Assert-That '** BOTH HALVES ARE REQUIRED: a queued step with NO drain is NOT a producer' {
    # ★ The discriminating case, and this ticket's own defect restated:
    # RenderTest.cpp:2460 queues AddStep_GraphBuild(...) for the tennis graph and
    # the unit-gate boot never drains the queue -- which is exactly why the two
    # tennis units were reading a prior boot's asset. Fails the moment the
    # `-and` becomes an `-or`, which would excuse three further candidate files
    # and would have called the bug this lint exists to find "clean".
    -not (Test-HasProducerSignature 'xSteps.AddStep_GraphBuild("game:Graphs/Brain.bgraph", &BuildFn);')
}

Assert-That 'a drain with NO write step is NOT a producer either' {
    # The other half of the `-and`. Fails if the write-step half is dropped,
    # which would excuse every file that merely runs the automation queue.
    -not (Test-HasProducerSignature 'RunAutomation(xSteps);')
}

Write-Host "`n=== comments are not code (the mutation-proved defect) ===" -ForegroundColor Cyan
# ★ A `//` mention of Zenith_AssetRegistry::Save used to count as a producer, and
# the failure was self-defeating: the comment that silenced
# RenderTest_Tennis.Tests.inl was the one written to EXPLAIN this ticket's fix,
# and it named the signature because that is the honest way to describe what the
# fixture does. Proved by mutation against the real tree -- deleting the real
# Save at :1571 left the read reported `ok`; only rewording the comment at :1544
# made it a finding.

Assert-That '** a Save mentioned ONLY in a // comment is NOT a producer' {
    # Exactly the mutation above. Fails if Test-HasProducerSignature stops
    # stripping comments -- at which point any file whose prose names the
    # signature is silently excused.
    -not (Test-HasProducerSignature "// Zenith_AssetRegistry::Save() -- see Zenith_EditorAutomation.cpp's GRAPH_BUILD")
}

Assert-That '** a Save mentioned ONLY in a /* */ block comment is NOT a producer' {
    # Fails if the comment regex loses its block-comment alternative, which is
    # the spelling a file-header explanation is most likely to use.
    -not (Test-HasProducerSignature "/*`n * The fixture calls Zenith_AssetRegistry::Save(pxAsset, path).`n */")
}

Assert-That '** a REAL call on a line that also carries a // comment IS a producer' {
    # The over-stripping guard: the previous two rows would pass just as well if
    # the stripper blanked whole lines, or the whole file. Fails if the `//`
    # alternative ever stops being anchored to the comment's own start.
    Test-HasProducerSignature 'Zenith_AssetRegistry::Save(pxAsset, szPath);   // writes the graph'
}

Assert-That '** a COMMENTED-OUT read is not a finding' {
    # The second case the same strip fixes. Fails if Get-AssetPathReadFindings
    # matches against the raw text instead of the stripped view.
    $t = '// xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Old.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    @($f).Count -eq 0
}

Assert-That '** LINE NUMBERS SURVIVE STRIPPING -- the delete-based-stripper trap' {
    # ★ The row most likely to save someone later. A stripper that DELETED
    # comment characters instead of blanking them to spaces would shift every
    # line below the first comment; here the block comment spans three lines, so
    # a delete-based stripper reports line 2 instead of 5 and every finding in
    # the repo below a file header becomes untriageable. Fails the moment
    # Get-CommentStrippedView stops preserving length.
    $t = "/* one`n   two`n   three */`n// four`nGetView<A>(`"game:Graphs/X.bgraph`");"
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].Line -eq 5)
}

Assert-That 'Get-CommentStrippedView preserves LENGTH exactly -- the invariant above' {
    # Asserted directly as well as through its consequence, because this is the
    # single property the line numbers rest on. Fails on any delete-based or
    # collapse-based rewrite.
    $t = "code(); /* a`nb */ more();`n// tail`nend();"
    $v = Get-CommentStrippedView $t
    ($v.Length -eq $t.Length) -and
    # newlines kept, so the line structure is identical...
    (($v.Split([char]10).Count) -eq ($t.Split([char]10).Count)) -and
    # ...and the code either side of the block comment survives.
    ($v -match 'code\(\);') -and ($v -match 'more\(\);') -and ($v -match 'end\(\);') -and
    (-not ($v -match 'tail'))
}

Assert-That 'the automation producer is comment-stripped too, not just the Save one' {
    # Both signatures go through the same stripped view. Fails if only
    # ProducerRegex were given the stripped text while the automation pair kept
    # reading raw.
    -not (Test-HasProducerSignature "// RunAutomation(xPrime); AddStep_GrassTypesSave();")
}

Assert-That 'the write-step pattern matches Save AND Build, and not an arbitrary step' {
    # Fails if the `(?:Save|Build)` alternation is narrowed to one verb, or
    # widened to any AddStep_ at all (which would excuse a file that merely adds
    # an entity).
    $save  = "RunAutomation(x);`nx.AddStep_MaterialSave();"
    $build = "RunAutomation(x);`nx.AddStep_GraphBuild(p, f);"
    $other = "RunAutomation(x);`nx.AddStep_CreateEntity(`"Probe`");"
    (Test-HasProducerSignature $save) -and (Test-HasProducerSignature $build) -and
    (-not (Test-HasProducerSignature $other))
}

Write-Host "`n=== line numbers, shapes and empty cases ===" -ForegroundColor Cyan

Assert-That 'the reported line number is the line the call STARTS on' {
    # A finding whose line number is wrong is a finding nobody can triage.
    # Fails if the offset->line conversion drops its +1 (Split().Count already
    # yields 1 for offset 0) or splits on the wrong character.
    $t = "// one`n// two`nGetView<A>(`"game:Graphs/X.bgraph`");"
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 1) -and ($f[0].Line -eq 3)
}

Assert-That 'a ONE-finding result is an ARRAY, so .Count is not $null' {
    # `,@($arr)` on the return: a PowerShell function UNROLLS a returned
    # collection, so a single finding would come back as a bare object and
    # `.Count -gt 0` would be false -- the run block would report a clean tree
    # with one finding in hand. Same hazard as ZagentClient's `,@(...)` returns.
    $t = 'GetView<A>("game:Graphs/X.bgraph");'
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    ($f -is [array]) -and ($f.Count -eq 1)
}

Assert-That 'an empty file yields an EMPTY ARRAY, not $null' {
    # Fails if the early return hands back $null, which would throw under
    # StrictMode in the run block's foreach.
    $f = Get-AssetPathReadFindings -Text '' -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    ($null -ne $f) -and ($f -is [array]) -and ($f.Count -eq 0)
}

Assert-That 'two reads in one file yield two findings, both with their own line' {
    # Fails if the matcher ever stops iterating all matches (e.g. Match instead
    # of Matches).
    $t = "GetView<A>(`"game:Graphs/X.bgraph`");`nGetView<B>(`"game:Graphs/Y.bgraph`");"
    $f = Get-AssetPathReadFindings -Text $t -RelPath 'A.Tests.inl' -ConstantMap $emptyMap
    (@($f).Count -eq 2) -and ($f[0].Line -eq 1) -and ($f[1].Line -eq 2)
}

Write-Host "`n=== Get-TrackedSourceFiles -- an empty scan must THROW ===" -ForegroundColor Cyan
# ★ A lint that scans zero files reports a clean tree. That is the single worst
# failure mode this script has, so every route to an empty list is a throw.

function New-GitFixture {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("bootassets_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'test' 2>$null | Out-Null
    return $dir
}

Assert-That '** a NON-git directory THROWS rather than scanning nothing' {
    # Fails if the $LASTEXITCODE check is dropped: `git ls-files` outside a repo
    # prints nothing and this would return an empty list, i.e. a green run over
    # zero files.
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("bootassets_nogit_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    try {
        $threw = $false
        try { $null = Get-TrackedSourceFiles -Root $dir } catch { $threw = $true }
        $threw
    } finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** a repo with tracked files but NO source THROWS too' {
    # A distinct route to the same disaster: the raw list is non-empty, so the
    # first guard passes, and the extension filter empties it. Fails if the
    # post-filter emptiness check is removed.
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'README.md') -Value '# hi' -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -m init --quiet 2>$null | Out-Null
        $threw = $false
        try { $null = Get-TrackedSourceFiles -Root $repo } catch { $threw = $true }
        $threw
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a real repo returns its tracked source, with Middleware/ excluded' {
    # Fails if the Middleware exclusion is dropped (vendored Jolt/ImGui/freetype
    # would flood pass 1 with foreign constants) -- or if it is widened enough to
    # swallow the repo's own files, which the Tests/ row below would catch.
    $repo = New-GitFixture
    try {
        New-Item -ItemType Directory -Path (Join-Path $repo 'Games/Probe/Tests') -Force | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $repo 'Middleware/vendored') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Probe.cpp') -Value '// t' -Encoding utf8
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Probe.h') -Value '// h' -Encoding utf8
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/notes.md') -Value '# n' -Encoding utf8
        Set-Content -LiteralPath (Join-Path $repo 'Middleware/vendored/third.cpp') -Value '// v' -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -m init --quiet 2>$null | Out-Null

        $files = Get-TrackedSourceFiles -Root $repo
        (@($files).Count -eq 2) -and
        ($files -contains 'Games/Probe/Tests/Probe.cpp') -and
        ($files -contains 'Games/Probe/Probe.h') -and
        (-not ($files -contains 'Middleware/vendored/third.cpp')) -and
        (-not ($files -contains 'Games/Probe/notes.md'))
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host "`n=== end to end over a two-file fixture ===" -ForegroundColor Cyan

Assert-That '** the ticket''s own scenario, UNFIXED -- and the comment must NOT rescue it' {
    # The two passes composed, on the exact shape ZEN-84 is about, with the class
    # wrapper the real header has AND the explanatory comment the real file
    # carries at :1544. That comment is the hard part: it names
    # Zenith_AssetRegistry::Save because that is the honest way to describe the
    # fixture, and it used to suppress this finding on its own -- so this row
    # pins the behaviour the mutation exposed rather than the easy version.
    # Fails if comment stripping is removed from the producer scan, OR if
    # qualified resolution regresses (the finding vanishes entirely).
    $header = "class RenderTest_TennisAgentComponent`n{`npublic:`n	static constexpr const char* kszGraphAsset = `"game:Graphs/RenderTest_TennisBrain.bgraph`";`n};"
    $test = "// Zenith_AssetRegistry::Save() -- see Zenith_EditorAutomation.cpp's GRAPH_BUILD`n" +
            "ZENITH_TEST(RenderTestTennis, Integration_LoadWiring)`n" +
            "{`n" +
            "	xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(`n" +
            "		RenderTest_TennisAgentComponent::kszGraphAsset);`n" +
            "}"
    $r = New-Map @($header, $test)
    $f = Get-AssetPathReadFindings -Text $test -RelPath 'Games/RenderTest/Components/RenderTest_Tennis.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    (@($f).Count -eq 1) -and
    ($f[0].HasProducer -eq $false) -and
    ($f[0].Line -eq 4) -and
    ($f[0].AssetPath -eq 'game:Graphs/RenderTest_TennisBrain.bgraph') -and
    ($f[0].Reader -eq 'AddGraphByAssetPath') -and
    ($f[0].Via -eq 'RenderTest_TennisAgentComponent::kszGraphAsset')
}

Assert-That '** ...and FIXED: the fixture''s Save moves that same line to `ok`' {
    # ★ THE ACCEPTANCE CHECK. This is the state the tree is in NOW: a previous
    # worker added EnsureTennisBrainGraphAssetAuthored(), which builds the graph
    # and writes it with Zenith_AssetRegistry::Save, so the read is covered and
    # the line belongs in the `ok` section rather than the findings. The pair of
    # rows differs by exactly the Save line. Fails if the qualified resolution
    # regresses (the line disappears entirely, silently) OR if the producer
    # detection regresses (it comes back as a finding) -- the two ways this
    # ticket's own case can go wrong.
    # Identical to the row above except for the one REAL Save line -- the
    # explanatory comment is present in BOTH halves, which is what makes the pair
    # a test of the call rather than of the prose.
    $header = "class RenderTest_TennisAgentComponent`n{`npublic:`n	static constexpr const char* kszGraphAsset = `"game:Graphs/RenderTest_TennisBrain.bgraph`";`n};"
    $test = "// Zenith_AssetRegistry::Save() -- see Zenith_EditorAutomation.cpp's GRAPH_BUILD`n" +
            "static void EnsureTennisBrainGraphAssetAuthored()`n" +
            "{`n" +
            "	Zenith_AssetRegistry::Save(pxAsset, RenderTest_TennisAgentComponent::kszGraphAsset);`n" +
            "}`n" +
            "ZENITH_TEST(RenderTestTennis, Integration_LoadWiring)`n" +
            "{`n" +
            "	xE.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(`n" +
            "		RenderTest_TennisAgentComponent::kszGraphAsset);`n" +
            "}"
    $r = New-Map @($header, $test)
    $f = Get-AssetPathReadFindings -Text $test -RelPath 'Games/RenderTest/Components/RenderTest_Tennis.Tests.inl' -ConstantMap $r.Map -QualifiedMap $r.QualifiedMap
    # Exactly ONE read-anchored candidate: `Save` is not on the reader list, so
    # its path argument is not a read. Asserted as `-ge 1` plus an exact count on
    # the reader itself, so adding a reader later cannot make this row vacuous.
    (@($f).Count -ge 1) -and
    (@($f | Where-Object { -not $_.HasProducer }).Count -eq 0) -and
    (@($f | Where-Object { $_.Reader -eq 'AddGraphByAssetPath' }).Count -eq 1)
}

Write-Host ''
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
