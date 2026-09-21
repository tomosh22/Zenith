<#
.SYNOPSIS
    Lints the boot unit-test suite for one specific order-dependence pattern:
    a unit that READS a gitignored asset off disk without producing it itself.

.DESCRIPTION
    ZEN-84. The finding this exists for (Behaviour Graph parity program,
    2026-09): deleting the on-disk `.bgraph` before a gate reddened
    `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}` — those units
    loaded an asset a PRIOR tools boot had written. A unit that depends on a
    file a previous run produced is green on a warm tree, red on a fresh clone
    or straight after a clean, and the gate's `ran == baseline` pin counts it
    either way.

    ============================================================
    WHAT A "FINDING" IS HERE
    ============================================================

    An asset path that is the FIRST ARGUMENT of a call that actually loads from
    disk, where:

      * the path is a `"game:..."` / `"engine:..."` string literal, or a named
        `const char*` constant this script could resolve to one (pass 1 below);
      * its extension is one that `.gitignore`'s `**/Assets/**` block really
        ignores — i.e. NOT `.znavmesh` / `.zscen` / `.ztexdecl`, which are
        re-included and therefore TRACKED, and not anything under an
        `Assets/Authored/` directory, which is re-included WHOLESALE (see the
        comment block above `**/Assets/**` in the repo-root .gitignore);
      * and the same file carries neither producer signature (below).

    ============================================================
    TWO PRODUCER SIGNATURES, BOTH AT FILE GRANULARITY
    ============================================================

      1. `Zenith_AssetRegistry::Save(` — the direct write.
      2. `RunAutomation(` **and** `AddStep_*Save|Build(`, TOGETHER — the editor-
         automation route: the file queues a write step and drains the queue
         in-process.

    ★ SIGNATURE 2 NEEDS BOTH HALVES, AND THAT IS THIS TICKET'S OWN DEFECT
    RESTATED. Queuing a step is not producing an asset. `RenderTest.cpp:2460`
    queues `AddStep_GraphBuild(...)` for the tennis graph and the unit-gate boot
    never drains the automation queue — which is precisely why the two tennis
    integration units were reading an asset a PRIOR tools boot had left behind.
    A lint that accepted a queued step as production would have called the bug
    it exists to find "clean". The DRAIN is what makes the write real.

    ★ BOTH SCANS RUN AGAINST A COMMENT-STRIPPED VIEW of the file, and that is a
    correctness fix rather than tidiness. A `//` mention used to count as a
    producer, and the failure was self-defeating: the comment that silenced
    `RenderTest_Tennis.Tests.inl` was the one written to EXPLAIN this ticket's
    fix, and it named `Zenith_AssetRegistry::Save()` because that is the honest
    way to describe what the fixture does — so the better somebody documented an
    order-dependence fix, the more reliably this lint went quiet about it.
    Proved by mutation: deleting the real `Save` at :1571 left the read reported
    `ok`; only rewording the comment at :1544 made it a finding. See
    `Get-CommentStrippedView`, including what the stripper is naive about.

    The one file that carries both today is
    `Zenith/Editor/TerrainEditor/Zenith_TerrainEditor.Tests.inl`, which primes
    `game:Vegetation/GrassTypes.zdata` at :961-966 before reading it at :972 and
    :1010 — deliberately, so the test does not depend on whatever the game
    happened to ship. That is already the FIXED shape of this ticket's defect,
    reached through the queue instead of through `Save`. Three further candidate
    files carry a write step with NO drain and stay flagged.

    ============================================================
    THE READER LIST IS THIS LINT'S WHOLE RECALL BOUNDARY
    ============================================================

    A call is a READ only if it is on this list. Each entry was read before
    being put here, and the verification is recorded so the next person does
    not have to redo it:

      * `AddGraphByAssetPath`      — `Zenith_GraphComponent::AddGraphByAssetPath`
                                     pushes a slot then calls
                                     `InstantiateSlotGraph`, which calls
                                     `Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>`.
                                     Loads.
      * `GetView<T>`               — `Zenith_AssetRegistry::GetView<T>` forwards
                                     to `GetInternal`, which runs the
                                     registered per-type loader. Loads.
      * `Acquire<T>`               — `Zenith_AssetHandle.h:334` forwards to
                                     `AcquireInternal` (`Zenith_AssetRegistry.cpp:611`),
                                     which calls the SAME `GetInternal` and then
                                     `AddRef`s. Loads. It has zero literal-path
                                     call sites in the candidate population
                                     today, so it changes no finding — but
                                     `AssetHandling/CLAUDE.md` documents it as
                                     the PREFERRED owning loader, so a boot unit
                                     written with the recommended API would
                                     otherwise be invisible to this lint.
      * `LoadSerializableAsset`    — declared `Zenith_AssetRegistry.h:44`,
                                     defined `Zenith_AssetRegistry.cpp:782`;
                                     its body calls
                                     `Zenith_DataStream::ReadFromFile`. Loads.
      * `OpenAsset`                — `Zenith_GraphEditorPanel::OpenAsset` calls
                                     `GetView<Zenith_BehaviourGraphAsset>` on
                                     the normalized path. Loads (tolerantly —
                                     see the known false-positive class below).

    ★ `AddMeshByPath` WAS ON THIS LIST AND WAS DROPPED, deliberately.
    `Zenith_ModelAsset::AddMeshByPath` (`Zenith_ModelAsset.cpp:236`) does
    `xBinding.m_xMesh.SetPath(strMeshPath)` and pushes a binding. It touches no
    disk — it is a `SetPath` with a longer name — so a reader list carrying it
    would report a MENTION as a read.

    That is the whole difference between this lint and the one it replaced,
    which reported **243 findings against 27** by flagging any path-shaped
    literal a test file happened to contain. Over the 504 tracked candidate
    files the lint actually scans, the functions receiving a literal asset path
    are led by `Zenith_AssetRegistry::ResolvePath` (26), `std::string` (22) and
    `Zenith_GraphEditorPanel::OpenAsset` (18); `SetPath` itself has **5** call
    sites. (An earlier note here claimed 335, from a sweep that sorted the
    repo with `Get-ChildItem -Recurse` and forgot to exclude `.claude/`, whose
    `worktrees/` hold 1475+ copies of every `.Tests.inl`. The 243→27 improvement
    is unaffected — the old script did exclude `.claude` at top level — but a
    measurement is only as good as the population, which is why this lint drives
    everything from `git ls-files`.)

    ★ `OpenAssetFresh` IS DELIBERATELY NOT A READER. It calls `OpenAsset` and
    then `Clear()`s the definition (`Zenith_EditorPanel_GraphEditor.cpp:2013`):
    it is the boot-REGENERATION entry point, which discards whatever was on
    disk. The regex cannot match it (`OpenAsset` must be followed by `(`).

    ============================================================
    WHAT THIS LINT CANNOT SEE — read this before trusting a CLEAN run
    ============================================================

    * **It cannot follow a path through a variable, a helper or a
      concatenation.** `GetView<T>(strMeshPath)` where `strMeshPath` is a local
      `std::string` built at runtime is INVISIBLE, and most of this repo's
      asset reads in tests are exactly that shape. Pass 1 resolves ONLY a
      `const char*` / `constexpr char*` initialised from a literal in the
      tracked tree; a `std::string` constant, an array element, a function
      parameter, a member and anything assembled at runtime all resolve to
      nothing and are skipped silently. This is a deliberate false NEGATIVE:
      guessing at an unresolvable name is how a lint invents a finding.

    * **A constant NAME that resolves to two different paths anywhere in the
      tree is treated as UNRESOLVABLE and skipped**, and the count of such
      names is printed. Short file-local `namespace {}` constants collide
      freely here (`szPATH`, `szASSET`, `szPREY`), and a global name->path map
      would silently keep whichever file was scanned last. Preferring a wrong
      path over no path is strictly worse than saying nothing.

    * **★ BUT A CLASS-SCOPED CONSTANT IS NOT A COLLISION, AND TREATING IT AS
      ONE MADE THIS LINT MISS ITS OWN MOTIVATING CASE.** `kszGraphAsset` is a
      convention: EIGHT components each declare their own
      (`RenderTest_TennisAgentComponent`, `DPDoor_Component`,
      `DPForge_Component`, `DPItemBase_Component`,
      `DPPauseMenuController_Component`, `DPPlayerController_Component`,
      `DPVillager_Component`, `Priest_Component`). Those are eight distinct C++
      symbols and every use site is qualified
      (`RenderTest_TennisAgentComponent::kszGraphAsset`), so the bare-name rule
      saw one name with eight paths and skipped it — leaving the lint blind to
      the exact line ZEN-84 is about, for a brand new reason.

      Pass 1 therefore keeps TWO maps: the bare-name one (unchanged, collisions
      and all) and a `<Class>::<Name>` one. A QUALIFIED argument resolves
      against the qualified map first and falls back to the bare map; an
      UNQUALIFIED argument only ever reads the bare map. So the bare spelling of
      `kszGraphAsset` stays correctly ambiguous — it appears in the skipped list
      and that is right — while `RenderTest_TennisAgentComponent::kszGraphAsset`
      resolves. A qualified key that genuinely collides (the same class name in
      two headers) is skipped under the same rule.

    * **The enclosing-type scan is a line-anchored heuristic, not a parse**
      (see `Get-EnclosingTypeName`). Its failure mode is chosen: a wrong class
      name produces a key no use site ever spells, so the lint MISSES rather
      than fabricating.

    * **`Zenith_AssetRegistry::Get<T>` is NOT on the reader list because it does
      not exist.** Four documents describe it as the canonical loader
      (`AssetHandling/CLAUDE.md`, `Core/CLAUDE.md`, `Prefab/CLAUDE.md`,
      `Zenith_MeshGeometryAsset.h`'s usage comment) but
      `Zenith_AssetRegistry.h` declares no such member and no `.cpp`/`.inl`
      calls one — the real name is `GetView<T>`. Recorded here rather than
      silently omitted: if `Get<T>` is ever actually added, it belongs on the
      list, and meanwhile those four docs are stale.

    * **`.zscen` is not UNCONDITIONALLY tracked.** `!**/*.zscen` re-includes it,
      but `.gitignore`'s own comment at :129-133 notes that a LATER
      `unit_test_scene.zscen` rule still wins (last-match-wins), so the
      harness-written scene in the exe CWD is ignored after all. A read of that
      one path is therefore wrongly excused by the extension rule. Left as a
      caveat rather than special-cased: no candidate file reads it by literal
      path today, and encoding one filename here would be a rule that drifts
      from `.gitignore` silently.

    * **PASS 1 DOES NOT STRIP COMMENTS — pass 2 does.** A commented-out
      `const char* x = "game:..."` declaration is still folded into the constant
      maps. The likely effect is a spurious COLLISION (two paths for one name),
      which drops a resolution and costs a miss; the unlikely-but-possible
      effect is a live use site resolving to a stale commented path. Left as-is
      deliberately rather than changed unmeasured — the measured constant counts
      (47 bare / 11 qualified) were taken without it. Worth revisiting.

    * **The name map is ORDINAL (case-sensitive).** `szPath` and `szPATH` are
      different symbols in C++ and must be here too: a case-insensitive
      PowerShell hashtable resolves `GetView<T>(szPath)` at
      `Zenith_AssetRegistry.Tests.inl:171` against `szPATH` from
      `Zenith_EditorAutomation.Tests.inl:826` — a fabricated finding naming a
      file that has nothing to do with it.

    * **"Produces it" is ONE textual signature applied at FILE granularity.**
      `Zenith_AssetRegistry::Save(` appearing anywhere in the file marks the
      whole file self-sufficient for EVERY asset path it reads, related or not.
      That under-flags rather than over-flags, which is the right direction
      here. Per-reference dataflow is out of scope.

    * **It does not know which boot config a test runs under.** A unit-gate
      boot, a `--skip-unit-tests` run and a windowed `--automated-test` that
      runs AFTER the editor-automation queue has drained are indistinguishable
      to a text scan — and the third one legitimately reads an asset the same
      boot just produced. That is why this defaults to report-only.

    * **★ KNOWN FALSE-POSITIVE CLASS: the deliberately-ABSENT scratch path.**
      `Zenith_EditorPanel_GraphEditor.Tests.inl` calls `OpenAsset` on ~18
      `game:Graphs/zz_unit_*.bgraph` paths that intentionally do not exist —
      the test exercises `OpenAsset`'s documented "new asset" branch, which
      edits an owned empty definition, and the file's own comment at line 323
      says so: *"A path that does not exist on disk ... No file is ever written
      (Save is never called), so this leaves no artefact behind."* These are
      the OPPOSITE of order-dependent (they depend on the file's absence) and
      they have no producer, so they are reported. No second producer signature
      was added for them, because none exists: inventing a `zz_unit_` naming
      suppression would encode a convention that lives in two files and is
      written down nowhere. Triage them by reading the call site.

    A finding is a LEAD, not a verdict.

.PARAMETER RepoRoot
    Repo root. Defaults to two levels up from this script (Tools/ -> repo root).

.PARAMETER Fatal
    Exit 1 when findings exist. OFF by default (report-only), for the reason
    the false-positive class above gives: this lint's own first run reports
    hits beyond the two units ZEN-84 fixed, and none of those were triaged by
    that ticket. Findings print regardless of this switch.

.PARAMETER Json
    Emit machine-readable output (a single JSON object) instead of the
    human-readable report.

.PARAMETER NoRun
    Define the helpers and return WITHOUT scanning anything. Dot-sourcing a
    script RUNS it, and Tools/Test_CheckBootUnitAssets.ps1 needs the pure
    helpers without a scan of the live checkout. Same shape as
    Tools/tick_gates.ps1's -NoRun.
#>
[CmdletBinding()]
param(
	[string]$RepoRoot = $(Split-Path -Parent (Split-Path -Parent $PSCommandPath)),
	[switch]$Fatal,
	[switch]$Json,
	[switch]$NoRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

# Extensions `.gitignore`'s `**/Assets/**` block RE-INCLUDES, i.e. these are
# TRACKED and a fresh clone has them. Verified against the repo-root .gitignore
# lines 134-139 (`!**/*.znavmesh`, `!**/*.zscen`, `!**/*.ztexdecl`).
$script:TrackedExtensions = @('znavmesh', 'zscen', 'ztexdecl')

# Pass 1: a named `const char*` / `constexpr char*` initialised from a prefixed
# asset-path literal. Matches the declaration this whole ticket turns on --
# `static constexpr const char* kszGraphAsset = "game:Graphs/RenderTest_TennisBrain.bgraph";`
# (Games/RenderTest/Components/RenderTest_TennisAgentComponent.h:65).
$script:ConstantRegex = [regex]'(?:constexpr|const)\s+char\s*\*\s*(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*"((?:game|engine):[^"]+)"'

# Pass 2: a READ. Group 1 = the reader, group 2 = a literal path, group 3 = the
# final segment of a (possibly qualified) identifier. `\s` spans newlines, which
# is required: the motivating call site wraps its argument onto the next line
# (`RenderTest_Tennis.Tests.inl:1599-1600`).
# Group 3 captures the argument's FULL spelling, qualifiers included, because a
# qualified use resolves against a different map than a bare one.
$script:ReaderRegex = [regex]'(?<![A-Za-z0-9_])(AddGraphByAssetPath|GetView\s*<[^<>;{}()]*>|Acquire\s*<[^<>;{}()]*>|LoadSerializableAsset|OpenAsset)\s*\(\s*(?:"((?:game|engine):[^"]*)"|((?:[A-Za-z_][A-Za-z0-9_]*\s*::\s*)*[A-Za-z_][A-Za-z0-9_]*))\s*[,)]'

$script:ProducerRegex = [regex]'Zenith_AssetRegistry\s*::\s*Save\s*\('

# Producer signature 2: the editor-automation route. BOTH halves are required in
# the same file -- see Test-HasProducerSignature for why.
$script:AutomationDrainRegex = [regex]'RunAutomation\s*\('
$script:AutomationWriteStepRegex = [regex]'AddStep_[A-Za-z]*(?:Save|Build)\s*\('

# Comment spans, for the offset-preserving stripper below.
$script:CommentRegex = [regex]'/\*[\s\S]*?\*/|//[^\r\n]*'

# Enclosing-type detection for pass 1 (see Get-EnclosingTypeName). The
# `(?![^\r\n]*;)` lookahead rejects a forward declaration (`class Zenith_Entity;`)
# and a one-line struct, neither of which opens a block this scan should enter.
$script:TypeOpenRegex = [regex]'(?m)^[ \t]*(?:class|struct)[ \t]+([A-Za-z_][A-Za-z0-9_]*)(?![A-Za-z0-9_])(?![^\r\n]*;)'
$script:TypeCloseRegex = [regex]'(?m)^\};'

# ---------------------------------------------------------------------------
# File discovery -- driven by `git ls-files`, NOT Get-ChildItem -Recurse
# ---------------------------------------------------------------------------
# A recursive enumeration of this repo root takes minutes (the previous version
# of this script did it; two prototype runs timed out at 120 s and 590 s). The
# tracked set is also EXACTLY the set this lint means: an untracked build
# artefact is not source anyone can fix.
#
# ★ A LINT THAT SCANS ZERO FILES REPORTS A CLEAN TREE, which is the single worst
# thing this script could do. Every way the file list can come back empty is a
# THROW, never a quiet return.

function Get-TrackedSourceFiles
{
	param([Parameter(Mandatory)][string]$Root)

	$raw = $null
	try
	{
		$raw = & git -C $Root ls-files 2>$null
	}
	catch
	{
		throw "check_boot_unit_assets: could not run 'git' (is it on PATH?): $($_.Exception.Message)"
	}
	if ($LASTEXITCODE -ne 0)
	{
		throw "check_boot_unit_assets: 'git -C $Root ls-files' exited $LASTEXITCODE. Is '$Root' a git repository?"
	}

	$all = @($raw | Where-Object { $_ -and $_.Trim() })
	if ($all.Count -eq 0)
	{
		throw "check_boot_unit_assets: 'git ls-files' listed NO tracked files in '$Root'. Refusing to report a clean tree from an empty scan."
	}

	$result = New-Object System.Collections.Generic.List[string]
	foreach ($p in $all)
	{
		$norm = $p -replace '\\', '/'
		# Vendored third-party trees are not this repo's source.
		if ($norm -match '(?i)(^|/)Middleware/') { continue }
		if ($norm -notmatch '(?i)\.(h|hpp|inl|cpp)$') { continue }
		[void]$result.Add($norm)
	}

	if ($result.Count -eq 0)
	{
		throw "check_boot_unit_assets: '$Root' has tracked files but NO .h/.hpp/.inl/.cpp among them. Refusing to report a clean tree from an empty scan."
	}
	return ,@($result.ToArray())
}

# A boot unit-test file: a `.Tests.inl` suite, or a game's `Tests/*.cpp`
# automated test.
function Test-IsCandidateBootUnitFile
{
	param([Parameter(Mandatory)][string]$RelPath)

	$norm = $RelPath -replace '\\', '/'
	if ($norm -match '(?i)\.Tests\.inl$') { return $true }
	if ($norm -match '(?i)\.cpp$' -and $norm -match '(?i)(^|/)Tests/') { return $true }
	return $false
}

# ---------------------------------------------------------------------------
# Pass 1: named-constant resolution
# ---------------------------------------------------------------------------

# The empty accumulator pass 1 fills. FOUR collections, because a constant has
# two legitimate spellings at a use site and they need separate maps:
#
#   Map / Collisions                   keyed on the BARE name  (`szTOUR_ASSET_PATH`)
#   QualifiedMap / QualifiedCollisions keyed on `<Class>::<Name>`
#
# ★ ORDINAL, NOT A POWERSHELL HASHTABLE. See the header: a case-insensitive map
# resolves `szPath` against a constant named `szPATH`, which is a different C++
# symbol in a different file.
function New-AssetPathConstantAccumulator
{
	return [pscustomobject]@{
		Map                 = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::Ordinal)
		Collisions          = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
		QualifiedMap        = [System.Collections.Generic.Dictionary[string, string]]::new([System.StringComparer]::Ordinal)
		QualifiedCollisions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
	}
}

# The innermost `class`/`struct <Name>` a given offset sits inside, or '' for a
# declaration at namespace scope.
#
# ★ THIS IS A LINE-ANCHORED HEURISTIC, NOT A PARSE, and its failure mode is
# chosen deliberately. It takes the last `class`/`struct <Name>` line before the
# offset and the last column-0 `};` before it, and calls the constant enclosed
# when the declaration is the more recent of the two. It does not track brace
# depth, so it cannot see a `{` inside a string or a comment, and it will get a
# deeply nested or one-line type wrong.
#
# That is tolerable because a WRONG answer produces a key no use site ever
# spells -- a use site must match `<Class>::<Name>` exactly -- so the lint MISSES
# rather than inventing a finding. Zenith's house style (opening brace on its own
# line, class closed by `};` at column 0) is what makes it right in practice.
function Get-EnclosingTypeName
{
	param(
		[Parameter(Mandatory)][AllowEmptyString()][string]$Text,
		[Parameter(Mandatory)][int]$Index
	)

	if ($Index -le 0) { return '' }
	$prefix = $Text.Substring(0, $Index)

	$opens = $script:TypeOpenRegex.Matches($prefix)
	if ($opens.Count -eq 0) { return '' }
	$lastOpen = $opens[$opens.Count - 1]

	$closes = $script:TypeCloseRegex.Matches($prefix)
	$lastCloseIndex = -1
	if ($closes.Count -gt 0) { $lastCloseIndex = $closes[$closes.Count - 1].Index }

	if ($lastOpen.Index -lt $lastCloseIndex) { return '' }
	return $lastOpen.Groups[1].Value
}

# Fold ONE file's text into the accumulator. Incremental on purpose: the run
# block scans every tracked source file in the repo, and holding all their text
# at once is hundreds of MB of strings for no reason.
function Add-AssetPathConstantsFromText
{
	param(
		[Parameter(Mandatory)]$Accumulator,
		[Parameter(Mandatory)][AllowEmptyString()][string]$Text
	)

	if ([string]::IsNullOrEmpty($Text)) { return }

	foreach ($m in $script:ConstantRegex.Matches($Text))
	{
		$name = $m.Groups[1].Value
		$path = $m.Groups[2].Value

		# The qualified key first, when the declaration sits inside a type.
		$typeName = Get-EnclosingTypeName -Text $Text -Index $m.Index
		if (-not [string]::IsNullOrEmpty($typeName))
		{
			Add-OneConstant -Map $Accumulator.QualifiedMap -Collisions $Accumulator.QualifiedCollisions `
				-Key "$typeName::$name" -Path $path
		}

		# ...and the bare name ALWAYS, unchanged. A class-scoped constant is
		# genuinely ambiguous when spelled bare, so it still belongs in the bare
		# collision set; the qualified map is what a qualified use site reads.
		Add-OneConstant -Map $Accumulator.Map -Collisions $Accumulator.Collisions -Key $name -Path $path
	}
}

# One key -> one path, with the collision rule. Shared by the bare and the
# qualified map so the two cannot drift.
function Add-OneConstant
{
	param(
		[Parameter(Mandatory)]$Map,
		[Parameter(Mandatory)]$Collisions,
		[Parameter(Mandatory)][string]$Key,
		[Parameter(Mandatory)][string]$Path
	)

	if ($Collisions.Contains($Key)) { return }

	$existing = ''
	if ($Map.TryGetValue($Key, [ref]$existing))
	{
		if ($existing -cne $Path)
		{
			# ★ Two paths, one key -> UNRESOLVABLE. Drop it entirely rather than
			# keeping whichever file was scanned last.
			[void]$Collisions.Add($Key)
			[void]$Map.Remove($Key)
		}
		return
	}
	$Map[$Key] = $Path
}

# The whole-corpus form, for callers that already hold the texts (the tests).
function New-AssetPathConstantMap
{
	param([string[]]$Texts)

	$acc = New-AssetPathConstantAccumulator
	if ($null -eq $Texts) { return $acc }
	foreach ($text in $Texts)
	{
		if ([string]::IsNullOrEmpty($text)) { continue }
		Add-AssetPathConstantsFromText -Accumulator $acc -Text $text
	}
	return $acc
}

# ---------------------------------------------------------------------------
# Pass 2: read-anchored candidates
# ---------------------------------------------------------------------------

# The file with every comment blanked to SPACES of the same length, newlines
# kept. Pass 2 -- both the producer scan and the read scan -- runs against this
# rather than the raw text.
#
# ★ WHY BLANKING AND NOT DELETING: the view is the same LENGTH as the original,
# so every match offset is unchanged and the line-number computation needs no
# adjustment at all. A stripper that deleted characters would shift every
# reported line below the first comment, which is a lint nobody can triage.
#
# ★ WHY THIS EXISTS. A comment used to count as a producer, and the failure was
# self-defeating rather than merely untidy: the comment that silenced
# `RenderTest_Tennis.Tests.inl` was the one the previous worker wrote to EXPLAIN
# the fix for this very ticket, and it named `Zenith_AssetRegistry::Save()`
# because that is the honest way to describe what the fixture does. So the more
# carefully somebody documented an order-dependence fix, the more reliably this
# lint went quiet about that file. Proved by mutation: deleting the real Save
# call at :1571 left the line reported `ok`; only rewording the COMMENT at :1544
# turned it into a finding.
#
# Reads are stripped too, which kills a second case in the same stroke: a
# commented-out `AddGraphByAssetPath("game:...")` was a live finding.
#
# ★ IT IS NAIVE ABOUT `//` INSIDE A STRING LITERAL, deliberately — no
# tokenising, no raw string literals, no line continuations. A half-correct
# tokeniser is worse than a stated approximation. The consequence is bounded and
# in the safe direction: a string containing `//` blanks the rest of its line, so
# the worst case is a MISSED producer (a finding that should not be there),
# never a fabricated one.
function Get-CommentStrippedView
{
	param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)

	if ([string]::IsNullOrEmpty($Text)) { return $Text }
	return $script:CommentRegex.Replace($Text, { param($m) [regex]::Replace($m.Value, '[^\r\n]', ' ') })
}

function Test-IsTrackedAssetPath
{
	param([Parameter(Mandatory)][AllowEmptyString()][string]$AssetPath)

	# Assets/Authored/** is re-included WHOLESALE (.gitignore:139) -- hand-edited,
	# committed, never bake output, whatever the extension. A prefixed path spells
	# it `engine:Authored/...` because the `Assets/` root is implied by the prefix.
	#
	# ★ `:` IS IN THE LEADING ALTERNATION ON PURPOSE, and leaving it out was a
	# PRE-EXISTING defect (inherited unchanged from the first version of this
	# script, which had no test over it). `Authored` in
	# `engine:Authored/Meshes/StickFigure/Walk.zanim` is preceded by the root
	# prefix's COLON -- neither `^` nor a separator -- so the old
	# `(^|[\\/])Authored` matched nothing and every committed authored clip read
	# was reported as a finding. The trailing `([\\/]|$)` is what keeps this from
	# over-matching `game:Vegetation/AuthoredVein.ztxtr`, which is a genuinely
	# gitignored file and must still be flagged.
	if ($AssetPath -match '(?i)(^|[\\/]|:)Authored([\\/]|$)') { return $true }

	$ext = [System.IO.Path]::GetExtension($AssetPath)
	# No extension -> a directory-style reference, not a file; not this lint's concern.
	if ([string]::IsNullOrEmpty($ext)) { return $true }
	$ext = $ext.TrimStart('.').ToLowerInvariant()
	return ($script:TrackedExtensions -contains $ext)
}

# TWO producer signatures, both at FILE granularity (see the header):
#
#   1. `Zenith_AssetRegistry::Save(`  -- the direct write.
#   2. `RunAutomation(` AND `AddStep_*Save|Build(` TOGETHER -- the editor
#      automation route, where the file queues a write step and then DRAINS the
#      queue in-process.
#
# ★ SIGNATURE 2 REQUIRES BOTH HALVES, AND THAT IS THIS TICKET'S OWN DEFECT
# RESTATED. Queuing a step is not producing an asset: `RenderTest.cpp:2460`
# queues `AddStep_GraphBuild(...)` for the tennis graph, and the unit-gate boot
# never drains the automation queue -- which is exactly why
# `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}` were reading an
# asset a PRIOR tools boot had left behind. A lint that accepted a queued step as
# production would have called the bug it exists to find "clean". The DRAIN is
# what makes the write real.
#
# Measured reach: of the candidate files, exactly ONE carries both halves
# (`Zenith/Editor/TerrainEditor/Zenith_TerrainEditor.Tests.inl`, which primes
# `game:Vegetation/GrassTypes.zdata` at :961-966 before reading it at :972 and
# :1010, deliberately, so the test does not depend on whatever the game shipped).
# Three further files carry a write step with NO drain and stay flagged.
# ★ COMMENTS ARE STRIPPED FIRST, and that is load-bearing rather than tidy --
# see Get-CommentStrippedView for the mutation that proved it. Stripping happens
# INSIDE this function so it is correct however it is called; passing an
# already-stripped view costs one regex scan that finds nothing.
function Test-HasProducerSignature
{
	param([Parameter(Mandatory)][AllowEmptyString()][string]$Text)

	if ([string]::IsNullOrEmpty($Text)) { return $false }
	$code = Get-CommentStrippedView $Text
	if ($script:ProducerRegex.IsMatch($code)) { return $true }
	return ($script:AutomationDrainRegex.IsMatch($code) -and $script:AutomationWriteStepRegex.IsMatch($code))
}

# Every read-anchored candidate in one file. `HasProducer` is carried per
# candidate (it is a property of the FILE) so the caller can split the census
# from the findings without re-reading anything.
function Get-AssetPathReadFindings
{
	param(
		[Parameter(Mandatory)][AllowEmptyString()][string]$Text,
		[Parameter(Mandatory)][string]$RelPath,
		$ConstantMap,
		$QualifiedMap = $null
	)

	$findings = New-Object System.Collections.Generic.List[object]
	if ([string]::IsNullOrEmpty($Text)) { return ,@($findings.ToArray()) }

	# Both scans run against the comment-stripped view. It is the same LENGTH as
	# $Text, so match offsets -- and therefore the line numbers below -- are
	# identical to the raw file's.
	$code = Get-CommentStrippedView $Text

	$hasProducer = Test-HasProducerSignature $code

	foreach ($m in $script:ReaderRegex.Matches($code))
	{
		$reader = ($m.Groups[1].Value -replace '\s+', '')

		$assetPath = $null
		$via = 'literal'

		if ($m.Groups[2].Success)
		{
			$assetPath = $m.Groups[2].Value
		}
		elseif ($m.Groups[3].Success)
		{
			$spelling = ($m.Groups[3].Value -replace '\s+', '')
			$segments = @($spelling -split '::')
			$bare = $segments[$segments.Count - 1]

			# ★ A QUALIFIED USE CONSULTS THE QUALIFIED MAP FIRST. `kszGraphAsset`
			# is a class-scoped CONVENTION -- eight components each declare their
			# own (RenderTest_TennisAgentComponent, DPDoor_Component,
			# DPForge_Component, DPItemBase_Component, DPPauseMenuController_Component,
			# DPPlayerController_Component, DPVillager_Component, Priest_Component)
			# -- so the bare name has eight paths and is correctly skipped as
			# ambiguous, while every use site spells it
			# `RenderTest_TennisAgentComponent::kszGraphAsset` and is not ambiguous
			# at all. Without this the lint is blind to the case it exists for.
			if ($segments.Count -ge 2 -and $null -ne $QualifiedMap)
			{
				# Last TWO segments only: pass 1 knows the immediate enclosing
				# type, never its namespace.
				$qkey = "$($segments[$segments.Count - 2])::$bare"
				$resolvedQ = ''
				if ($QualifiedMap.TryGetValue($qkey, [ref]$resolvedQ))
				{
					$assetPath = $resolvedQ
					$via = $qkey
				}
			}

			# Fall back to the unique-bare-name rule, UNCHANGED. This is what
			# resolves `szZENITH_GRASS_TYPE_TABLE_ASSET_PATH` and
			# `szTOUR_ASSET_PATH`, and what correctly skips `szASSET`. A
			# namespace-qualified constant (no enclosing class, so no qualified
			# key) lands here too.
			if ($null -eq $assetPath -and $null -ne $ConstantMap)
			{
				$resolved = ''
				if ($ConstantMap.TryGetValue($bare, [ref]$resolved))
				{
					$assetPath = $resolved
					$via = $bare
				}
			}
		}

		# A variable, a helper's return, a colliding name, an unknown name:
		# INVISIBLE by design. See the header.
		if ($null -eq $assetPath) { continue }
		if (Test-IsTrackedAssetPath $assetPath) { continue }

		# $code is length-identical to $Text, so this is the raw file's line.
		$lineNo = $code.Substring(0, $m.Index).Split([char]10).Count

		[void]$findings.Add([pscustomobject]@{
			File        = ($RelPath -replace '\\', '/')
			Line        = $lineNo
			AssetPath   = $assetPath
			Reader      = $reader
			Via         = $via
			HasProducer = $hasProducer
		})
	}

	return ,@($findings.ToArray())
}

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------

if ($NoRun) { return }

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).ProviderPath

if (-not $Json) { Write-Host "Scanning tracked source under $RepoRoot (git ls-files) ..." }

$sourceFiles = Get-TrackedSourceFiles -Root $RepoRoot

# Pass 1 reads the WHOLE tracked source tree, not just the test files: the
# constant this ticket turns on (`kszGraphAsset`) lives in a component HEADER,
# and the test file never spells the literal at all. Folded one file at a time
# so the whole corpus is never resident.
$constants = New-AssetPathConstantAccumulator
foreach ($rel in $sourceFiles)
{
	$full = Join-Path $RepoRoot $rel
	$text = Get-Content -LiteralPath $full -Raw -ErrorAction SilentlyContinue
	if ([string]::IsNullOrEmpty($text)) { continue }
	Add-AssetPathConstantsFromText -Accumulator $constants -Text $text
}

$candidates = @($sourceFiles | Where-Object { Test-IsCandidateBootUnitFile $_ })

if (-not $Json)
{
	Write-Host "  tracked source files scanned for constants : $($sourceFiles.Count)"
	Write-Host "  bare-name constants resolved               : $($constants.Map.Count)"
	Write-Host "  bare names SKIPPED as colliding            : $($constants.Collisions.Count)"
	Write-Host "  class-qualified constants resolved         : $($constants.QualifiedMap.Count)"
	Write-Host "  qualified keys SKIPPED as colliding        : $($constants.QualifiedCollisions.Count)"
	Write-Host "  candidate boot unit files                  : $($candidates.Count)"
}

$allCandidateRefs = New-Object System.Collections.Generic.List[object]
foreach ($rel in $candidates)
{
	$full = Join-Path $RepoRoot $rel
	$text = Get-Content -LiteralPath $full -Raw -ErrorAction SilentlyContinue
	if ([string]::IsNullOrEmpty($text)) { continue }
	foreach ($f in (Get-AssetPathReadFindings -Text $text -RelPath $rel -ConstantMap $constants.Map -QualifiedMap $constants.QualifiedMap))
	{
		[void]$allCandidateRefs.Add($f)
	}
}

$findings = @($allCandidateRefs.ToArray() | Where-Object { -not $_.HasProducer })
$withProducer = @($allCandidateRefs.ToArray() | Where-Object { $_.HasProducer })

$fatalMode = [bool]$Fatal
$hasFindings = $findings.Count -gt 0

if ($Json)
{
	$out = [pscustomobject]@{
		# ★ `@(...)` on every collection-valued field. ConvertTo-Json serializes a
		# ONE-element array as a bare object (and a one-element string array as a
		# bare string), so a consumer indexing [0] gets a character or a throw --
		# the repo's `,@(...)` unrolling trap in its JSON form. Forcing the wrap
		# keeps the shape stable at 0, 1 and N.
		FilesScanned            = $sourceFiles.Count
		CandidateFiles          = $candidates.Count
		ConstantsResolved       = $constants.Map.Count
		CollidingConstantNames  = @($constants.Collisions)
		QualifiedResolved       = $constants.QualifiedMap.Count
		CollidingQualifiedKeys  = @($constants.QualifiedCollisions)
		ReadAnchoredCandidates  = $allCandidateRefs.Count
		Findings                = @($findings)
		FindingCount            = $findings.Count
		CoveredByProducer       = $withProducer.Count
		FatalMode               = $fatalMode
		HasFindings             = $hasFindings
	}
	$out | ConvertTo-Json -Depth 6
}
else
{
	Write-Host ''
	Write-Host '============================================================'
	Write-Host ' Zenith boot unit-test asset lint (read-anchored)'
	Write-Host '============================================================'
	Write-Host "Read-anchored candidates      : $($allCandidateRefs.Count)"
	Write-Host "  covered by a Save() in-file : $($withProducer.Count)"
	Write-Host "  FINDINGS (no producer)      : $($findings.Count)"
	Write-Host ''
	Write-Host '--- Findings: a gitignored asset READ with no Save() in the same file ---'
	foreach ($f in ($findings | Sort-Object File, Line))
	{
		Write-Host "FINDING $($f.File):$($f.Line) $($f.AssetPath) <- $($f.Reader) [via $($f.Via)]"
	}
	if ($withProducer.Count -gt 0)
	{
		Write-Host ''
		Write-Host '--- Clean: the same shape, but the file produces an asset itself ---'
		foreach ($f in ($withProducer | Sort-Object File, Line))
		{
			Write-Host "ok      $($f.File):$($f.Line) $($f.AssetPath) <- $($f.Reader) [via $($f.Via)]"
		}
	}
	if ($constants.Collisions.Count -gt 0)
	{
		Write-Host ''
		Write-Host '--- BARE names skipped as UNRESOLVABLE (one name, two paths) ---'
		Write-Host '    (a class-scoped convention like kszGraphAsset belongs here: its'
		Write-Host '     bare spelling IS ambiguous. Qualified uses resolve separately.)'
		foreach ($n in (@($constants.Collisions) | Sort-Object))
		{
			Write-Host "skip    $n"
		}
	}
	Write-Host ''
	Write-Host '============================================================'
	Write-Host ' SUMMARY'
	Write-Host '============================================================'
	Write-Host "Findings                      : $($findings.Count)"
	Write-Host "Mode                          : $(if ($fatalMode) { 'FATAL (-Fatal)' } else { 'report-only (default - see this script''s -Fatal doc comment)' })"
	Write-Host '============================================================'
}

if ($fatalMode -and $hasFindings) { exit 1 }
exit 0
