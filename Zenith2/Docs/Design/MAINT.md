# MAINT design — Engine maintenance

This is the entry document of the MAINT (Engine maintenance) design family. MAINT owns the supported API and the version policy, the deprecation lifecycle and the removal of deprecated APIs, dependency-upgrade rehearsals, subsystem ownership, the upkeep of the libgraph baseline and the complexity profile, and keeping the representative sample games building. It ships one small engine header, a PowerShell maintenance module with its checks, five committed data files, and the procedures every stage acceptance runs. The requirement catalogue is `Zenith2/Docs/Requirements/MAINT.md`.

## Purpose and scope

### What MAINT owns

| Capability | Inventory item | Specified in |
|---|---|---|
| the supported API: the surfaces, their stability classes, the clause grammar | Z2-I-MAINT-01 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Supported surfaces", "Stability classes", "Version field clauses" |
| the version policy: the engine version, its changelog, the compatibility windows of every surface | Z2-I-MAINT-02 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Engine version", "Engine changelog", "Version state machine", "Compatibility windows" |
| dependency-upgrade rehearsals | Z2-I-MAINT-03 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Dependency-upgrade rehearsals" |
| deprecated-API removal | Z2-I-MAINT-04 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation lifecycle", "Deprecation checks", "API surface listing" |
| subsystem ownership | Z2-I-MAINT-05 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Subsystem ownership" |
| libgraph baseline upkeep | Z2-I-MAINT-06 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Baseline re-pinning" |
| complexity-profile upkeep | Z2-I-MAINT-07 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration", "Complexity exceptions" |
| representative sample games kept building | Z2-I-MAINT-08 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample games", "Build matrix" |
| gate-headroom triage ([Z2-N-BUDGET-353]) | Z2-I-MAINT-06, Z2-I-MAINT-07 (gate upkeep) | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Gate headroom triage" |

### Exclusions

| Not MAINT's | Owner | Why |
|---|---|---|
| the manifest, the update tool, the release pipeline, release lines and `release.json`, upgrade guides | SHIP (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency updates"; `Zenith2/Docs/Design/SHIP/Release.md` → "Release manifest", "Upgrade guides") | SHIP performs the upgrade and the release; MAINT decides when an upgrade is rehearsed and what versions mean |
| games, `game.json`, `zenith_add_game`, the game build | MOD (`Zenith2/Docs/Design/MOD.md` → "Repository games") | MAINT checks that the set stays representative and due games exist |
| the gate scripts, the libgraph and complexity file formats, the protected edits of both files | PROC (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph", "Complexity") | protected paths; MAINT supplies procedure, evidence and decisions |
| the libgraph and complexity gates as CI requirements | DEVT (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "Gates in CI") | DEVT's Z2-R-DEVT-103 and -104 |
| the Luau binding descriptor, the deprecated-call diagnostic and its test | SCRIPT (cross-area note MT2; row SH14) | MAINT states the policy; SCRIPT implements the mechanism |
| schema and authoring-document migration | ECS (`Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"), SCENE (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration algorithms") | MAINT's windows cite them |
| `Zenith_Result`, the error taxonomy, API-only header conventions | CORE (`Zenith2/Docs/Contracts.md` → "Errors and results"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy") | MAINT's one header follows them |
| save and replay compatibility | SAVE (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Migration pipeline") | cited by the windows |
| modding, dynamic plugins, a C++ game API | excluded (Z2-D-075, Z2-D-024, Z2-D-002) | "Supported surfaces" lists them as unsupported |

### Inventory items covered

Z2-I-MAINT-01 to Z2-I-MAINT-08, all required; the coverage table is `Zenith2/Docs/Requirements/MAINT.md` → "Coverage of inventory items".

### Document family

| File | Holds | Its headings |
|---|---|---|
| `Zenith2/Docs/Design/MAINT.md` (this file) | the skeleton, the public surface, formats index, state machines index, error contract, diagnostics, failure handling, stage plan, decisions, requirement index | the headings of this file |
| `Zenith2/Docs/Design/MAINT/Versioning.md` | supported surfaces, stability classes, clauses, engine version, changelog, windows, deprecation, the API surface listing, Luau classification, release and roll | Scope of this file · Supported surfaces · Stability classes · Version field clauses · Engine version · Engine changelog · Version state machine · Compatibility windows · Deprecation lifecycle · Deprecation checks · API surface listing · Luau API classification · Release and roll procedures |
| `Zenith2/Docs/Design/MAINT/GateData.md` | libgraph baseline and complexity profile upkeep | Scope of this file · Libgraph checker inputs · Baseline re-pinning · Surface growth report · Libgraph tool changes · Complexity checker inputs · Complexity calibration · Calibration rules · Complexity measurement harness · Calibration record · Complexity exceptions · Stage 0 contents |
| `Zenith2/Docs/Design/MAINT/Upkeep.md` | ownership, rehearsals, sample games, headroom, acceptance steps | Scope of this file · Subsystem ownership · Ownership file · Ownership resolution · Ownership checks · Dependency-upgrade rehearsals · Rehearsal registry · Rehearsal plan · Rehearsal procedure · Rehearsal record · Sample games · Sample games file · Build matrix · Sample game checks · Gate headroom triage · Stage acceptance and mid-sprint steps |
| `Zenith2/Docs/Design/MAINT/Tests.md` | every MAINT test | Scope of this file · Test registration · C++ tests · Live checks · Research record rules · PowerShell check tests · Fixture layout · Acceptance checks · Which gate step runs each |

### Reconciliations with the core documents

<!-- elaboration -->
| # | Point | MAINT's choice | Core follow-up |
|---|---|---|---|
| C1 | [Z2-N-CONTRACT-522], [Z2-N-CONTRACT-523] require a deprecation for every source-API removal; the backlog needs freedom to change unshipped declarations | stable (shipped or provisional), experimental and internal classes, an effective `since` of `max(since, 2)`, and the `Version` clause grammar; whether a provisional declaration may change without a deprecation is flagged proposal MAINT-1, pending Q97, and until it is ruled [Z2-N-CONTRACT-523] binds every public declaration as written | cross-document row X384 |
| C2 | [Z2-N-CONTRACT-529] "any engine version change triggers a re-cook" | for cooked data the engine version is `uZENITH_ENGINE_COOK_VERSION`; the engine semantic version moves without re-cooking | row X385 |
| C3 | [Z2-N-CONTRACT-492] lists the `reviewPaths` | MAINT's five data files join it | row X386 |
| C4 | Architecture's "Ownership per directory" lists no MAINT path | `Tools/Maintenance/` and the three engine-version files are MAINT's | row X387 |
| C5 | `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" | MAINT defines no misuse code, so it reserves no block (a block is reserved only by an area that defines a code, ruling MISUSE-BLOCKS-NEW-AREAS-3) | row X388, withdrawn |
| C6 | [Z2-N-CONTRACT-419] makes recalibration a MAINT ticket while the profile is protected | from the Stage 4 acceptance on, a MAINT `needs-human` ruling ticket per stage, then a PROC `needs-human` apply ticket blocked by it and drafted by PROC | row X389 |
| C7 | the MAINT checks add test time inside the `Test` step | a pending budget row for the maintenance check suite's wall time | row X390 |

## Dependencies

### Areas MAINT depends on

| Area | What MAINT relies on | Cited heading |
|---|---|---|
| (contracts) | contract blocks, the compatibility table, the review and change rules, the test naming | `Zenith2/Docs/Contracts.md` → "Contract blocks per type", "Compatibility policy per kind", "Review and change management", "Tests and code hygiene gates" |
| (architecture) | the ownership table, the library graph, the build presets, the `declared_deps.json` record | `Zenith2/Docs/Architecture.md` → "Repository layout", "Library dependency graph"; `Zenith2/Docs/Architecture/BuildSystem.md` → "Presets", "The declared_deps.json record" |
| (roadmap) | the epics table, the stage acceptance shape | `Zenith2/Docs/Roadmap.md` → "Board conventions" |
| CORE | `Zenith_Types.h`, `Zenith_Result.h`, `Zenith_Span.h`, `Zenith_StringView.h`, the error-enumeration conventions; hosting MAINT's header file in ZenithCore (cross-area note MT1) | `Zenith2/Docs/Design/CORE.md` → "Public root headers"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Results and the error taxonomy" |
| SHIP | `zenith_register_pwsh_test`, the update tool, the manifest's sections and owner tokens, `Enter-Zenith2Env.ps1` | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_register_script_test"; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency updates", "Owner values" |
| PROC | the gate formats, the libgraph outputs, the re-pin through `Run-Committed.ps1`, the analyzer's entity dump, the acceptance and mid-sprint tickets and their apply tickets, the format conventions (cross-area note MT3) | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule", "complexity_profile.json", "Libgraph outputs"; `Zenith2/Docs/Design/PROC.md` → "Format conventions" |
| MOD | the repository games, `game.json`, game test registration | `Zenith2/Docs/Design/MOD.md` → "Repository games"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration" |
| SCRIPT | the binding descriptor's classification members, the deprecated-call warning ZS1808, `bindings.json` and `Zenith_ScriptEngineVersionHash()` (cross-area note MT2, adopted, row SC30) | `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Binding stability and deprecation", "Deprecated call sites", "Binding listing" |
| ECS, SCENE, SAVE, ASSET | the migration and versioning mechanisms the windows cite | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration algorithms"; `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Migration pipeline"; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Constants and fault points" |
| DEVT | the libgraph and complexity CI requirements MAINT's data serves | `Zenith2/Docs/Design/DEVT/QualityTools.md` → "Gates in CI" |

<!-- assumption: pending CORE -->
ZenithCore hosts `Zenith_EngineVersion.h`, its `.cpp` and its doctest file, and lists the header among its public root headers (cross-area note MT1). Until CORE's document lists it, MAINT T1 is held draft on that CORE edit.
<!-- /assumption -->

PROC carries MAINT's acceptance steps A1 to A5, the review record of every acceptance commit that changes a `reviewPaths` file and the mid-sprint steps (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure"), drafts the per-stage complexity apply tickets Z2T-PROC-062, -073, -083 and -095 and puts `Zenith2/Tools/Maintenance/*.json` in `reviewPaths` (`Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map", "Z2T-PROC-016 PROC-API edit"), defines the re-pin command (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N -WriteBaseline") and the `surface.json` and `surface_delta.json` schemas (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs").

The analyzer's `--dump-entities` option and its `complexity-entities` schema (cross-area note MT3 (j)) are PROC's Z2T-PROC-004 (`Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities"), which MAINT T7's measurement harness is blocked by.

<!-- assumption: pending PROC -->
If Q97 is ruled (a), the Backlog option `File-Backlog.ps1 --mark-stale --keys <list>` (cross-area note MT7) is a PROC request.
<!-- /assumption -->

<!-- assumption: pending SHIP -->
SHIP's upgrade pair cites the rehearsal record of the version it adopts, and its upgrade guide reads `engine_changelog.json` (cross-area note MT4).
<!-- /assumption -->

<!-- assumption: pending MOD -->
MOD writes the engine version into the startup composition dump and the first log record (cross-area note MT5).
<!-- /assumption -->

### Libraries and third-party dependencies

`Zenith_EngineVersion.h` is part of `ZenithCore` and adds no dependency to it (it includes only ZenithCore headers). The PowerShell module depends on the pinned `pwsh` and `git` (SHIP's checked prerequisites) and on `python` and the analyzer for the measurement harness alone. MAINT adds no manifest entry.

### Interfaces MAINT declares for other areas

- **The `Version` clause grammar** (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"): every area writes stability and deprecation in its contract blocks in this form.
- **`Get-Zenith2Owner`** ("Public API" below): the tick orchestrator, PROC's mid-sprint and acceptance tickets and SHIP's known-issues list resolve an owning area and epic from a path.
- **The changelog entry rule**: every area's ticket that makes a compatibility change appends a `compat` entry and bumps the header in its own diff.
- **The plan file** of the roll: the removals, graduations and rehearsals the next stage owes, per owner.

### Areas that depend on MAINT

Every area, through the clause grammar and the changelog rule; PROC, through the acceptance steps and the reports; SHIP, through the rehearsal records an upgrade cites and the changelog an upgrade guide reads (cross-area note MT4); SCRIPT, through the engine version key (cross-area note MT2).

## Library and file layout

### Files owned by MAINT

| Path | Kind | Created by |
|---|---|---|
| `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h` | public header inside ZenithCore | MAINT T1 |
| `Zenith2/Engine/ZenithCore/Internal/Zenith_EngineVersion.cpp` | ZenithCore source | MAINT T1 |
| `Zenith2/Engine/ZenithCore/Tests/Zenith_EngineVersion_Tests.cpp` | doctest file, part of `ZenithCore_Tests` | MAINT T1 |
| `Zenith2/Tools/Maintenance/Zenith2Maintenance.psm1` | the maintenance module | MAINT T2 |
| `Zenith2/Tools/Maintenance/Test-Zenith2Maintenance.ps1` | the check CLI | MAINT T2 |
| `Zenith2/Tools/Maintenance/Get-Zenith2Owner.ps1` | ownership lookup CLI | MAINT T2 |
| `Zenith2/Tools/Maintenance/Set-Zenith2EngineRelease.ps1`, `Invoke-Zenith2VersionRoll.ps1`, `Update-Zenith2ApiSurface.ps1` | release and roll | MAINT T4 |
| `Zenith2/Tools/Maintenance/New-Zenith2RehearsalRecord.ps1`, `Get-Zenith2RehearsalPlan.ps1` | rehearsals | MAINT T6 |
| `Zenith2/Tools/Maintenance/Measure-Zenith2Complexity.ps1` | calibration harness | MAINT T7 |
| `Zenith2/Tools/Maintenance/New-Zenith2SurfaceGrowthReport.ps1` | surface growth report | MAINT T7 |
| `Zenith2/Tools/Maintenance/engine_changelog.json` | data | MAINT T2 |
| `Zenith2/Tools/Maintenance/subsystem_owners.json` | data | MAINT T2 |
| `Zenith2/Tools/Maintenance/api_surface.json` | data (written by the roll and the Stage 7 release step) | MAINT T4 (empty: `shippedThrough` null) |
| `Zenith2/Tools/Maintenance/rehearsals.json` | data | MAINT T6 |
| `Zenith2/Tools/Maintenance/sample_games.json` | data | MAINT T5 |
| `Zenith2/Tools/Maintenance/Tests/Maintenance.Tests.ps1`, `Tests/Fixtures/**` (header and source fixtures named `*.h.fixture`, `*.cpp.fixture` and so on) | tests | with each ticket |
| the MAINT block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | test registration | MAINT T2, extended by each ticket |
| `Zenith2/Docs/Research/Maintenance/`, `Upgrades/`, `Complexity/`, `Libgraph/`, `GateHeadroom/` | committed records | the procedures |

`Zenith2/Tools/Maintenance/` holds no `CMakeLists.txt`, so no target globs it. Its fixtures include header and source text, but every such file carries the suffix `.fixture` and the test runner strips it when it copies a fixture tree under `${ZENITH_OUT}` (`Zenith2/Docs/Design/MAINT/Tests.md` → "Fixture layout"), so no tracked file under the directory ends in `.h`, `.hpp`, `.inl`, `.cpp` or `.c`, and `SourceLayout`, `Format` and `ForbiddenCheck`, which scan `Zenith2/Tools/**` apart from `Zenith2/Tools/Gates/Tests/**`, find no source there; `Pwsh_Maint_Live_FixturesNotSources` keeps it so.

### Supported matrices

| Target or tool | Where it exists |
|---|---|
| `Zenith_EngineVersion.h` and its `.cpp` | every preset ZenithCore supports (`ZENITH_ALL`) |
| its doctest cases | every `ZENITH_HOST` preset (`win`, `win-asan`, `linux-tsan`) |
| the PowerShell module, scripts and `Pwsh_Maint_*` cases | the `win` preset's test registration (`zenith_register_pwsh_test` registers in `win` alone) and any Windows host running `pwsh` |

## Public API

### API surface kinds

MAINT's public surface is one C++ header, PowerShell functions and scripts, five JSON formats, and the `Version` clause grammar. Zenith Hungarian applies to the C++ header; PowerShell names follow the approved `Verb-Noun` form with the `Zenith2` noun prefix, as PROC's do.

### Zenith_EngineVersion.h

The full header with its three contract blocks is `Zenith2/Docs/Design/MAINT/Versioning.md` → "Zenith_EngineVersion header". Its declarations:

| Declaration | Signature |
|---|---|
| value type | `struct Zenith_EngineVersion { const uint16 m_uMajor = 0; const uint16 m_uMinor = 0; const uint16 m_uPatch = 0; const uint16 m_uDevCounter = 0; const bool m_bPrerelease = false; }` |
| error enumeration | `enum Zenith_EngineVersionError : uint8 { ENGINE_VERSION_ERROR_MALFORMED = 1, ENGINE_VERSION_ERROR_COMPONENT_RANGE, ENGINE_VERSION_ERROR_DEV_COUNTER_RANGE, ENGINE_VERSION_ERROR_BUFFER_TOO_SMALL }` |
| constants | `uZENITH_ENGINE_VERSION_MAJOR`, `_MINOR`, `_PATCH`, `_DEV` (`uint16`), `bZENITH_ENGINE_VERSION_PRERELEASE` (`bool`), `szZENITH_ENGINE_VERSION` (`const char*`), `uZENITH_ENGINE_VERSION_MAX_DEV` (65534), `uZENITH_ENGINE_VERSION_MAX_TEXT_BYTES` (28) |
| current version | `constexpr Zenith_EngineVersion Zenith_GetEngineVersion()` |
| ordering key | `constexpr uint64 Zenith_GetEngineVersionKey(const Zenith_EngineVersion& xVersion)` |
| comparison | `constexpr sint32 Zenith_CompareEngineVersions(const Zenith_EngineVersion& xA, const Zenith_EngineVersion& xB)` |
| text form | `Zenith_Result<uint32, Zenith_EngineVersionError> Zenith_FormatEngineVersion(const Zenith_EngineVersion& xVersion, Zenith_Span<char> xDestination)` |
| parsing | `Zenith_Result<Zenith_EngineVersion, Zenith_EngineVersionError> Zenith_ParseEngineVersion(Zenith_StringView strText)` |
| error names | `constexpr const char* Zenith_ToString(Zenith_EngineVersionError eError)` |

### PowerShell module functions

`Zenith2/Tools/Maintenance/Zenith2Maintenance.psm1` (every function takes `-Repo <path>`, default the repository root resolved from the module's own location, so the fixtures run against a copied tree):

```powershell
Get-Zenith2EngineVersionState [-Repo <string>]
    # { Major; Minor; Patch; Dev; Prerelease; Text; State = 'DEV' | 'RELEASE'; HeaderPath }
Test-Zenith2EngineVersionHeader [-Repo <string>]                         # findings VER-*
Test-Zenith2EngineChangelog [-Repo <string>]                             # findings CHG-*
Get-Zenith2PublicBlocks [-Repo <string>]
    # [ { Library; Header; Line; Kind; Name; Since; Class = 'stable'|'experimental'|'internal';
    #     ClassRule; ExperimentalUntil; Consumers; Deprecation = { Stage; Replacement; RemovedIn } | $null;
    #     Deviations = [ { Line; Stage; Replacement; RemovedIn } ]; Attributes = [ { Line; Replacement; RemovedIn } ] } ]
Test-Zenith2ApiClassification [-Repo <string>] [-DeclaredDeps <string>]  # findings CLS-*
Test-Zenith2Deprecations [-Repo <string>]                                # findings DEP-*
Test-Zenith2ApiSurface [-Repo <string>]                                  # findings SUR-*
Update-Zenith2ApiSurface -ShippedThrough <int> [-Repo <string>]          # rewrites api_surface.json (roll step 3)
Get-Zenith2Owner -Path <string> [-OwnersFile <string>]                   # { Owner; Epic; Row; ViaFallback }
Test-Zenith2Ownership [-Repo <string>] [-DeclaredDeps <string>] [-Severity Test|Acceptance]   # findings OWN-*
Test-Zenith2Rehearsals [-Repo <string>] [-Acceptance] [-Stage <int>]     # findings REH-*
Get-Zenith2RehearsalPlan -Stage <int> [-Repo <string>]                   # plan items ("Rehearsal plan")
Test-Zenith2SampleGames [-Repo <string>] [-Severity Test|Acceptance] [-AsOfMajor <int>]      # findings SG-*
Get-Zenith2ComplexityProposal -Entities <pscustomobject> -Profile <pscustomobject>           # proposal and audit (CPX-*)
Test-Zenith2ResearchRecords [-Repo <string>]                             # findings RES-*
Test-Zenith2FixtureNames [-Repo <string>]                                # findings FIX-*
New-Zenith2MaintenanceFinding -Rule <string> -File <string> -Line <int> -Message <string> [-Severity fail|report]
Write-Zenith2MaintenanceReport -Check <string> -Findings <object[]> [-Out <string>]           # writes the check report
```

A finding is `{ file, line, rule, severity, message }`, the shape of PROC's gate report findings (`Zenith2/Docs/Design/PROC.md` → "The gate step report"), so a reader of either sees the same records.

### PowerShell commands

| Command | Parameters | Purpose |
|---|---|---|
| `Test-Zenith2Maintenance.ps1` | `-Check Version\|Changelog\|Classification\|Deprecations\|Surface\|Ownership\|Rehearsals\|SampleGames\|Records\|Fixtures\|All` · `-Acceptance -Stage <0..7>` · `-Report` · `[-Repo <path>]` · `[-Json]` | run checks, the acceptance set, or write the status report |
| `Get-Zenith2Owner.ps1` | `-Path <repo path>` · `[-Json]` | owner lookup |
| `Set-Zenith2EngineRelease.ps1` | `-Stage <2..7>` · `-Ticket <Draft-ID or board key>` | the release step |
| `Invoke-Zenith2VersionRoll.ps1` | `-Stage <2..6>` · `-Ticket <Draft-ID or board key>` · `[-PlanOut <path>]` | the roll |
| `Update-Zenith2ApiSurface.ps1` | `-ShippedThrough <int>` · `[-DryRun]` | the listing rebuild (the roll and the Stage 7 release step call the module function; the script exists for a human dry run) |
| `Get-Zenith2RehearsalPlan.ps1` | `-Stage <int>` · `[-Json]` | the rehearsal plan |
| `New-Zenith2RehearsalRecord.ps1` | `-Name <entry>` · `-From <version>` · `-To <version>` | scaffold a record |
| `Measure-Zenith2Complexity.ps1` | `-Out <dir>` · `[-ProfileFile <path>]` · `[-Repo <path>]` | calibration measurement |
| `New-Zenith2SurfaceGrowthReport.ps1` | `-Stage <int>` · `[-Out <dir>]` | the stage's surface growth report |

### Script exit codes

| Code | Name | Meaning |
|---|---|---|
| 0 | `MAINT_EXIT_OK` | success; for a check, no `fail` finding |
| 1 | `MAINT_EXIT_FINDINGS` | at least one `fail` finding |
| 2 | `MAINT_EXIT_USAGE` | a missing, unknown or out-of-range parameter, or a path outside `Zenith2/` |
| 3 | `MAINT_EXIT_INPUT` | an input is missing, unreadable, not UTF-8, not JSON, of an unknown `schema` or of a newer `schemaVersion` |
| 4 | `MAINT_EXIT_PRECONDITION` | the release or roll preconditions failed, or a write was rolled back |
| 5 | `MAINT_EXIT_GIT` | a `git` command the script needs failed |

### Handle kinds used

None. MAINT defines no handle and stores none.

## Data structures and formats

### Formats index

| Format | `schema` | Specified in |
|---|---|---|
| `engine_changelog.json` | `zenith2.engine-changelog` | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Changelog schema" |
| `api_surface.json` | `zenith2.api-surface` | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Surface listing schema" |
| the roll's plan file | `zenith2.maintenance-plan` | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Roll step" |
| `subsystem_owners.json` | `zenith2.subsystem-owners` | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership file" |
| `rehearsals.json` | `zenith2.rehearsals` | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal registry" |
| `Rehearsal.json` | `zenith2.upgrade-rehearsal` | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal record" |
| `sample_games.json` | `zenith2.sample-games` | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample games file" |
| `surface_growth.json` | `zenith2.surface-growth-report` | `Zenith2/Docs/Design/MAINT/GateData.md` → "Surface growth report" |
| the analyzer's entity dump (PROC's option, assumed) | `complexity-entities` | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity measurement harness" |
| `calibration.json` | `zenith2.complexity-calibration` | `Zenith2/Docs/Design/MAINT/GateData.md` → "Calibration record" |
| check report | `zenith2.maintenance-check` | "Check reports" below |
| status report | `zenith2.maintenance-status` | "Status report" below |
| test fixture expectation | `zenith2.maint-fixture` | `Zenith2/Docs/Design/MAINT/Tests.md` → "Test registration" |
| the engine version text | — (text) | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Text form" |

Every JSON format follows PROC's conventions: UTF-8 without BOM, LF, four-space indentation, `schema` and `schemaVersion` first, arrays a tool writes byte-ordered by their key members, repo-relative `/` paths, `comment` members as comments (`Zenith2/Docs/Design/PROC.md` → "Format conventions"). JSON carries no endianness; the one binary form MAINT defines, the ordering key, is an in-memory `uint64` that is never written to a file by MAINT (a consumer that stores it, such as SCRIPT's `.zlbc` header, stores it little-endian as its own format states).

### In-memory layout

`Zenith_EngineVersion` is 10 bytes of members (four `uint16` and one `bool`), padded to 10 or 12 by the compiler; its layout is never serialized (the text form and the key are its persistent forms), so `sizeof` carries no contract.

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| version text parse and format, the ordering key | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Text form", "Ordering key" |
| `Version` field segmentation and clause parsing | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Clause grammar" |
| attribute and contract-text pairing | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation markers", "Deprecation checks" |
| listing rebuild at the roll | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Roll step" |
| owner resolution | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership resolution" |
| Architecture table parse | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership checks" |
| rehearsal due stages and plan | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal registry", "Rehearsal plan" |
| calibration proposal and exception audit | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration", "Calibration rules", "Complexity exceptions" |
| surface growth selection | `Zenith2/Docs/Design/MAINT/GateData.md` → "Surface growth report" |

### Public block extraction

`Get-Zenith2PublicBlocks` reads every root `.h` and `.inl` of each `Zenith2/Engine/<Lib>/` and of `Zenith2/Player/`, one file at a time, line by line, O(total lines):

1. A line matching `^/// @contract (interface|type|value|container|enum|functions) (\S+)\s*$` opens a block; lines until `^/// @endcontract\s*$` are the block body.
2. In the body, `^/// Version: (.*)$` starts the Version text and each following `^///   (.*)$` line appends a space and its text; the next `^/// [A-Z][a-z ]+: ` line ends it.
3. The block's declaration line is the first line after the close line that is not blank and not a `template<` line; the block's region extends to the line before the next open line or the end of file.
4. In the region, each `^\s*/// @deviation Version: (.*)$` line is a deviation, attached to the next declaration line.
5. Each occurrence of `[[deprecated(` is an attribute; its string literal is read by position from `("` to the matching `")]]`.
6. The class is the clause class, else the name rule of the header (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes"; rule N3 and N4 read the library's `CMakeLists.txt` for `KIND BACKEND` and `KIND MODULE`), else `stable`.
7. `Since` is the effective since stage, `max(<since of the clause>, 2)`, because the engine version first exists in Stage 2; every rule reads this value.

The check families consume this list; no family parses a header twice.

### State machines

| Machine | Specified in |
|---|---|
| the engine version (`DEV` / `RELEASE`) | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine" |
| one public declaration (`PROVISIONAL` … `REMOVED`) | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Declaration state machine" |

### Rehearsal ticket state machine

<!-- elaboration -->
| State | Event | Next state | Action |
|---|---|---|---|
| planned (a plan item) | the human files the TASK under the entry's owner epic (a bundle TASK for several entries of one owner that share a build set) | To Do | `needs-human`; the queue never claims it |
| To Do | the human starts it | In Progress | procedure R1 to R4 in a scratch worktree |
| In Progress | R5 to R8 complete | In Progress | the record is written |
| In Progress | verdict `adopt` or `adopt-with-changes` | Done | the registry row is updated; SHIP's upgrade pair is filed with the record cited |
| In Progress | verdict `no-newer` (upstream has no release newer than the pin) | Done | the record carries `upstreamSource`, `newestUpstream` and `checkedOn` and passes `REH-NONEWER`; the registry row records the stage; no build or measurement runs |
| In Progress | verdict `defer` | Done | `revisitStage` set; the row's due stage follows the cadence |
| In Progress | verdict `reject` | Done | a flagged proposal or ruling ticket when a security fix is blocked |
| In Progress | the human cannot complete it (no reference hardware available, a download unavailable) | Blocked | a BUG on the MAINT epic naming the missing input |

## Threading lifetime and error contract

### Thread affinity

| Item | Affinity |
|---|---|
| `Zenith_EngineVersion` and the constants | `IMMUTABLE` |
| `Zenith_ParseEngineVersion`, `Zenith_FormatEngineVersion`, the constexpr functions | `ANY_THREAD`; pure, no state, no allocation |
| the PowerShell module and scripts | one process; no background job outlives a command; the scripts are never run concurrently with a lane (the release and roll run with every lane idle, Z2-D-083) |

### Frame-graph nodes

None. Nothing MAINT ships runs in the frame.

### Lock-order entries

None. MAINT takes no `Zenith_Mutex`.

### Interruption and cancellation

The release and roll write every target through `<path>.z2tmp` and rename, and restore every target from `git` on any failure after the first rename (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Roll step"); a check that is interrupted writes nothing tracked (its reports live under `${ZENITH_OUT}/maintenance/`).

### Error enumerations

`Zenith_EngineVersionError`:

| Enumerator | Value | Returned when |
|---|---|---|
| `ENGINE_VERSION_ERROR_MALFORMED` | 1 | the text breaks the grammar (empty, too long, a leading zero, a missing component, an unknown suffix, trailing bytes) |
| `ENGINE_VERSION_ERROR_COMPONENT_RANGE` | 2 | a MAJOR, MINOR or PATCH value above 65535 |
| `ENGINE_VERSION_ERROR_DEV_COUNTER_RANGE` | 3 | a dev counter above 65534 |
| `ENGINE_VERSION_ERROR_BUFFER_TOO_SMALL` | 4 | the format destination holds fewer than length + 1 bytes |

The PowerShell side reports through exit codes ("Script exit codes") and finding rule IDs (the `VER`, `CHG`, `CLS`, `DEP`, `SUR`, `OWN`, `REH`, `SG`, `CPX`, `RES`, `FIX`, `REG` families of the design files, the two input findings `MAINT-NO-CONFIGURE` and `MAINT-ENCODING`, and `ACC-*` of `Zenith2/Docs/Design/MAINT/Tests.md` → "Acceptance checks").

### Luau errors versus asserts

MAINT binds nothing to Luau, raises no Luau error and defines no misuse code, so it reserves no block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" (ruling MISUSE-BLOCKS-NEW-AREAS-3; row X388 withdrawn). The Luau deprecated-call diagnostic is SCRIPT's and is a diagnostic, never a Luau error. In C++, parse input never reaches an assert; formatting a value no parse could produce (`m_bPrerelease` false with a non-zero dev counter, or a counter above 65534) is a programmer error (`ZENITH_ASSERT`).

## Diagnostics and profiling

### Check reports

Each check family run by `Test-Zenith2Maintenance.ps1` writes `${ZENITH_OUT}/maintenance/<Check>.json`:

```json
{
    "schema": "zenith2.maintenance-check",
    "schemaVersion": 1,
    "check": "Deprecations",
    "engineVersion": "3.0.0-dev.4",
    "acceptanceStage": null,
    "findings": [ { "file": "Zenith2/Engine/ZenithCore/Zenith_Arena.h", "line": 41, "rule": "DEP-OVERDUE", "severity": "fail", "message": "…" } ],
    "counts": { "fail": 1, "report": 0 }
}
```

Each finding is also printed as `Maintenance: <file>:<line>: <rule>: <message>`, and the last line is `Maintenance: PASS` or `Maintenance: FAIL (<n> finding(s)); report <path>`.

### Status report

`Test-Zenith2Maintenance.ps1 -Report` writes `${ZENITH_OUT}/maintenance/status.json` (`schema` `zenith2.maintenance-status`) and `status.md`, the mid-sprint check's attachment:

| Section | Content |
|---|---|
| `## Engine version` | the header version, its state, the last changelog entry |
| `## Deprecations` | per state of the declaration machine: due for removal this stage (with owner), overdue, deprecated this stage |
| `## Experimental` | blocks expiring at this stage's roll, with owner |
| `## Rehearsals` | rows due by this stage's acceptance and their tickets' presence in the plan |
| `## Ownership` | paths resolving through a fallback, stale rows |
| `## Sample games` | games due at the next roll that do not exist yet, pending modules and backends |
| `## Gate data` | the last baseline pin (`acceptedAt`, `headSha`), the last calibration record |

### Profiler and telemetry

None: the C++ functions are cheap pure functions with no zone; the scripts are host tools outside the engine. The engine version reaches the startup composition dump and the first log record through MOD (cross-area note MT5).

## Authoring and cooking

MAINT has no editor surface and no cook step. Its authored inputs are its five JSON data files and the `Version` text of every contract block:

| Input | Edited by |
|---|---|
| `engine_changelog.json`, the engine version header | the ticket that makes a compatibility change (a `compat` entry); the human's release and roll |
| `api_surface.json` | created empty by T4; then the roll and the Stage 7 release step alone |
| `subsystem_owners.json` | the ticket that adds or moves a library, tool, game or owned directory; the acceptance ownership audit |
| `rehearsals.json` | rehearsal tickets; the roll (missing rows) |
| `sample_games.json` | the ticket that changes MOD's repository-game tables |
| contract block `Version` text | every ticket that adds or changes a public type |

Each data file is a `reviewPaths` file (row X386), so every edit of it is reviewed. The engine version touches cooking indirectly only through SCRIPT's bytecode header, if SCRIPT adopts the key ("Engine version hash for bytecode" in `Zenith2/Docs/Design/MAINT/Versioning.md`).

## Failure handling

<!-- elaboration -->
| Failure | Defined behaviour |
|---|---|
| a data file is missing | its check family exits 3 naming the file; a missing file that a family creates (`api_surface.json` before T4) is a failure only from the ticket that creates it on |
| a data file is malformed, not UTF-8, carries a BOM, has an unknown `schema` or a newer `schemaVersion` | exit 3 with `<file>: <reason>`; nothing is written |
| `${ZENITH_OUT}/declared_deps.json` is absent (the tree was never configured) | the families that need it (`CLS-CONSUMERS`, `CLS-INCLUDE`, `OWN-TARGETS`) report one `report` finding `MAINT-NO-CONFIGURE` and skip those rules; acceptance treats the absence as exit 3 |
| the engine version header is unparseable | every family that needs MAJOR exits 3 with `VER-LINES` printed |
| a public header is not valid UTF-8 | a `fail` finding `MAINT-ENCODING` at line 0 for that header; the other headers are still scanned |
| a release or roll precondition fails | exit 4 naming each failed item; nothing is written |
| a release or roll write fails half-way | every target restored from `git`; exit 4 |
| `git` is missing or a `git` command fails | exit 5 with the command and its output |
| the pinned `python` or the analyzer is missing for the measurement harness, or the analyzer has no `--dump-entities` option | exit 3 naming the missing piece; no calibration file is written |
| a rehearsal finds no upstream release newer than the pin | verdict `no-newer` with its evidence members; no build or measurement; the row counts as rehearsed in the stage |
| a manifest entry has no rehearsal row at an acceptance | the report finding `REH-ROWS-MISSING`; the roll adds the row |
| a rehearsal cannot measure (no reference hardware) | the ticket goes Blocked with a BUG naming the missing input; the registry row stays unchanged, so the entry remains due |
| a stage acceptance finds an overdue removal or an expired experimental block | the roll refuses (exit 4); the acceptance waits for the removal or graduation ticket |
| allocation failure in the C++ functions | not applicable: they allocate nothing |
| device loss | not applicable |
| a budget breach | the maintenance check suite's wall time is measured against its pending row (row X390); a breach is a `MAINT` headroom-triage input like any other step timing |

## Test strategy

Every test is named, with the case it asserts, in `Zenith2/Docs/Design/MAINT/Tests.md`:

| Group | Run by |
|---|---|
| C++ doctest cases of `Zenith_EngineVersion.h` | `Test` (`ZenithCore_Tests`), sanitizer presets |
| live checks over the committed tree | `Test` (`win`) |
| fixture checks, at least one per rule ID, plus release, roll, plan, proposal, report, input and exit-code cases | `Test` (`win`) |
| the registration self-check (`Pwsh_Maint_Registry_AllCasesRegistered`) | `Test` (`win`) |
| acceptance checks, each with a failing fixture case | the stage acceptance |

Fixtures are text-only trees under `Zenith2/Tools/Maintenance/Tests/Fixtures/`, with header and source fixtures carrying the `.fixture` suffix; git repositories the release and roll cases need are created at test time; no fixture records a digest; no test pins a count.

## Budget rows

| Row | Relationship |
|---|---|
| Z2-B-PROC-011 (Test step timeout) | the MAINT checks run inside the `Test` step |
| Z2-B-PROC-002, Z2-B-PROC-003 (warm phase caps) | the headroom triage ticket restores them ([Z2-N-BUDGET-353]) |
| Z2-B-PROC-014, Z2-B-PROC-015 (Libgraph and Complexity step timeouts) | the gate data MAINT keeps is read inside them |
| Z2-B-PROC-008, Z2-B-PROC-009, Z2-B-PROC-010 (build step timeouts) | every repository game builds inside them |
| Z2-B-PROC-030 (integration smoke frame count) | the CI smoke boots every workload and flagship game |
| the maintenance check suite wall time (pending; producer: MAINT T8, a `needs-human` ticket that measures the suite's cold and warm time on the gate host and allocates the row; row X390) | the target the MAINT checks stay within: every `Pwsh_Maint_*` case together, warm, on the gate host |

MAINT owns no other row. The budget rows a rehearsal measures belong to the owning area of the rehearsed dependency.

## Stage plan

### Stage 0

No MAINT loop ticket: Z2-D-128 makes the scaffold the last Stage 0 ticket. MAINT's Stage 0 content is carried by others:
- PROC's Z2T-PROC-010 lands `libgraph_baseline.json` with `"libraries": []` and `complexity_profile.json` with the starting thresholds MAINT agrees to (`Zenith2/Docs/Design/MAINT/GateData.md` → "Stage 0 contents");
- the filer creates one epic per area code at A6, which is subsystem ownership at board level (Z2-I-MAINT-05);
- the Stage 0 and Stage 1 mid-sprint checks run the epic audit of step M4.

### Stage 1

No MAINT ticket. The engine version header lands in Stage 2 so that MAJOR equals the stage in which it lands; Stage 1 is architecture validation and ships no stable API, and code pulled forward into the Stage 1 sprint carries `since Z2 Stage 2` and is provisional until the Stage 2 acceptance. Blocks already written `since Z2 Stage 0` or `since Z2 Stage 1` keep their text: every MAINT rule reads the effective since stage `max(since, 2)`, so they are provisional until the Stage 2 acceptance and listed at the first roll.

### Stage 2

<!-- elaboration -->
| # | MAINT ticket | Contents | `blocked_by` |
|---|---|---|---|
| T1 | engine version header (kind `api` plus its implementation; small and atomic) | `Zenith_EngineVersion.h` at `2.0.0-dev.0`, its `.cpp`, its nine doctest cases | the CORE ticket that creates ZenithCore's `Zenith_Result.h`, `Zenith_Span.h`, `Zenith_StringView.h` |
| T2 | maintenance module, changelog and ownership | `Zenith2Maintenance.psm1` (version, changelog, ownership, fixture-name functions), `Test-Zenith2Maintenance.ps1`, `Get-Zenith2Owner.ps1`, `engine_changelog.json` (the first `roll` entry), `subsystem_owners.json` (directory and root-file rows), the MAINT registry block, `Maintenance.Tests.ps1 -List`, the `VER`, `CHG`, `OWN`, `FIX`, `REG`, input and exit-code cases and their live checks | T1; SHIP's `zenith_register_pwsh_test` ticket (Z2T-SHIP-001) |
| T3 | classification and deprecation checks | `Get-Zenith2PublicBlocks`, `CLS` and `DEP` families, their fixtures and live checks | T2 |
| T4 | surface listing, release and roll | `api_surface.json` (`shippedThrough` null), `SUR` family, `Update-Zenith2ApiSurface`, `Set-Zenith2EngineRelease.ps1`, `Invoke-Zenith2VersionRoll.ps1`, the release and roll cases | T3 |
| T5 | sample games | `sample_games.json`, the `SG` family and cases | T2; MOD's `Games/Minimal` ticket |
| T6 | rehearsal registry | `rehearsals.json` (one row per entry pinned so far, toolchain-sourced entries under their toolchain key), `REH` family, `Get-Zenith2RehearsalPlan`, `New-Zenith2RehearsalRecord` | T2 |
| T7 | calibration harness and surface growth report | `Measure-Zenith2Complexity.ps1`, `Get-Zenith2ComplexityProposal`, `New-Zenith2SurfaceGrowthReport.ps1`, `Test-Zenith2ResearchRecords`, `CPX`, `RES` and report cases; blocked by Z2T-PROC-004, which adds `--dump-entities` (cross-area note MT3 (j)) | T2 |
| T8 | maintenance-check suite budget row | measures the warm and cold time of every `Pwsh_Maint_*` case on the gate host and allocates the row (`needs-human`) | T3 to T7 |

API-first: T1's header is the only C++ any other MAINT ticket depends on, and it lands before them. The Stage 2 acceptance runs MAINT's first release (`2.0.0`) and roll (`3.0.0-dev.0`) with the starting complexity thresholds: no calibration runs before the Stage 4 acceptance (`Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration").

### Stage 3 to Stage 5

- Each stage: the rehearsal TASKs of the roll's plan (each on the entry's owner epic), the removal and graduation TASKs of the plan (each on its owner's epic), and headroom triage when triggered.
- Stage 4 to Stage 7: one calibration ruling ticket per stage, drafted by MAINT: T9 (`Z2 S4 complexity calibration`), T11 (S5), T12 (S6), T13 (S7), each `needs-human`, kind `decision`, run at that stage's acceptance (step A3), blocked by T7; each is followed by a PROC `needs-human` apply ticket that PROC drafts, blocked by it.
- Stage 4: the flagship slices become due (`firstBuildStage` 4) at the Stage 4 acceptance.

Stage 3: T10, the Luau classification ticket: the `luau[]` section of the listing and the Luau `SUR` and `DEP` rules over `bindings.json`, blocked by the three SCRIPT tickets that implement the descriptor members, the deprecated-call warning, and `bindings.json` with the version hash (cross-area note MT2, adopted by SCRIPT, row SC30).

### Stage 6

The deprecation-history and pre-qualification checks bind (`ACC-DEPRECATION-HISTORY`, `ACC-REHEARSALS` at N = 6): every deprecation logged with a removal stage below 6 is recorded removed in the listing, a removal due in Stage 6 itself is absent from the tree (`ACC-DEP-DUE`), and every manifest entry was rehearsed in Stage 5 or 6. The Stage 6 acceptance's release is the first public release's engine version, `6.0.<P>`.

### Stage 7

The same procedures; the Stage 7 acceptance runs the release step, which also lists the Stage 7 declarations, and no roll ("Version state machine" and "Deprecations at Stage 7" in `Zenith2/Docs/Design/MAINT/Versioning.md`). After `7.0.0` every change of a listed declaration follows [Z2-N-CONTRACT-523] and bumps PATCH alone (ruling MAINT-R1-RULINGS R7).

## Decisions

### Settled decisions governing MAINT

| ID | What it settles for MAINT |
|---|---|
| Z2-D-002 | games contain no C++, so the C++ API has in-repo consumers alone |
| Z2-D-007, Z2-D-042 | libgraph and the complexity gate are hard gates whose data MAINT keeps |
| Z2-D-012 | budget changes need a decision and a workload version bump; a rehearsal never re-budgets |
| Z2-D-022 | a settled choice is challenged only as a flagged proposal |
| Z2-D-024 | modules are static: no binary compatibility surface |
| Z2-D-047 | one contract block per type, which carries the `Version` clauses |
| Z2-D-048 | the per-kind compatibility policy MAINT's windows elaborate |
| Z2-D-049 | diff-derived API review, which covers MAINT's data files |
| Z2-D-065 | upgrades invalidate evidence through the fingerprint |
| Z2-D-082 | third-party pins and confinement |
| Z2-D-083 | acceptance runs outside the loop with lanes idle (the release and roll run there) |
| Z2-D-084 | measurement decisions split into a harness ticket and a ruling ticket (calibration) |
| Z2-D-085, Z2-D-097 | living documents and rulings in the ticket's own diff |
| Z2-D-128 | the scaffold is the last Stage 0 ticket, so MAINT has no Stage 0 loop ticket |
| Z2-D-129 | revalidation tickets absorb the rewrite cost the deprecation window gives time for |

### Open decisions MAINT depends on

None governs a MAINT ticket. The selection decisions add registry rows when ruled: D-SEL-HEAP, D-SEL-HASH and D-SEL-JSON (open, Stage 1); D-SEL-NAV, D-SEL-TEXT and D-SEL-UIRT (open, Stage 1/2); D-SEL-CODEC, D-SEL-VIDEO, D-SEL-CRASH and D-SEL-2DPHYS (open, Stage 2); D-SEL-FBX (open, Stage 4); D-SEL-UPSCALER (open, Stage 5).

### Rulings MAINT tickets append

| Ruling | Appended by |
|---|---|
| each complexity calibration (`Zenith2 complexity profile calibration at Stage <N>`) | the calibration ruling ticket of each stage from Stage 4 to Stage 7 (T9, T11, T12, T13) |
| each complexity exception admission | the admitting ruling ticket |
| a rehearsal verdict `reject` that blocks a security fix | its ruling ticket |

Each takes the next free `Z2-D-` ID in its own diff (Z2-D-097).

### Flagged proposals raised by MAINT

| # | Concern | Status |
|---|---|---|
| MAINT-1 | whether a provisional declaration (stable, effective `since` ≥ MAJOR) may change incompatibly without a deprecation, which narrows Z2-D-048 and [Z2-N-CONTRACT-523]; the same ruling decides whether an existing stable block may be re-classed experimental or internal (by default it may not: only a new declaration starts experimental or internal); if accepted, the narrowing covers both and also covers drafted, unexecuted tickets that cite the declaration, which the diff lists from a full-body search (`Zenith2/Docs/Design/MAINT/Versioning.md` → "Provisional changes pending Q97") | pending Q97; [Z2-N-CONTRACT-523] as written is the default until it is ruled |

## Requirement index

| Requirement | Design headings |
|---|---|
| Z2-R-MAINT-001, -002, -038 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Supported surfaces" |
| Z2-R-MAINT-003, -004, -010 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes" |
| Z2-R-MAINT-005 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Compatibility windows" |
| Z2-R-MAINT-006, -008, -011, -013 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses" |
| Z2-R-MAINT-007 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes" |
| Z2-R-MAINT-009 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation lifecycle" |
| Z2-R-MAINT-012 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "API surface listing" |
| Z2-R-MAINT-014, -015, -050 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Luau API classification" |
| Z2-R-MAINT-016 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Luau API classification"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Release and roll procedures" |
| Z2-R-MAINT-017 to -019, -037 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Compatibility windows" |
| Z2-R-MAINT-020 to -024, -035, -036 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Engine version" |
| Z2-R-MAINT-025, -026 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version numbers"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine" |
| Z2-R-MAINT-027, -028, -030 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Engine changelog" |
| Z2-R-MAINT-029, -034 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine" |
| Z2-R-MAINT-031, -033, -046, -110 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Release and roll procedures" |
| Z2-R-MAINT-032 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Release and roll procedures"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine" |
| Z2-R-MAINT-039 to -041 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation checks" |
| Z2-R-MAINT-042, -047, -111, -112 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation lifecycle" |
| Z2-R-MAINT-043 to -045, -124 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecation checks" |
| Z2-R-MAINT-048 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version field clauses"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "API surface listing" |
| Z2-R-MAINT-049, -108 | `Zenith2/Docs/Design/MAINT/Tests.md` → "Acceptance checks" |
| Z2-R-MAINT-051 to -056, -125 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "API surface listing" |
| Z2-R-MAINT-057 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Subsystem ownership"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership checks" |
| Z2-R-MAINT-058 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Subsystem ownership" |
| Z2-R-MAINT-059 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership file" |
| Z2-R-MAINT-060 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership resolution" |
| Z2-R-MAINT-061 to -063 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership checks" |
| Z2-R-MAINT-064, -066 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Baseline re-pinning" |
| Z2-R-MAINT-065 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Surface growth report" |
| Z2-R-MAINT-067 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Libgraph tool changes" |
| Z2-R-MAINT-068 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Stage 0 contents" |
| Z2-R-MAINT-069 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Scope of this file" |
| Z2-R-MAINT-070 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Libgraph checker inputs"; `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity checker inputs" |
| Z2-R-MAINT-071 to -073, -078 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration" |
| Z2-R-MAINT-074 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity measurement harness" |
| Z2-R-MAINT-075 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Calibration record" |
| Z2-R-MAINT-076, -077 | `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity exceptions" |
| Z2-R-MAINT-079, -080 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Dependency-upgrade rehearsals" |
| Z2-R-MAINT-081, -082, -089, -090 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal registry" |
| Z2-R-MAINT-083 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal plan" |
| Z2-R-MAINT-084 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal plan"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps" |
| Z2-R-MAINT-085 to -087, -114, -120 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal procedure" |
| Z2-R-MAINT-088 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal record" |
| Z2-R-MAINT-091, -096 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample games file" |
| Z2-R-MAINT-092 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample game checks" |
| Z2-R-MAINT-093 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample game checks"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample games" |
| Z2-R-MAINT-094, -095 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Build matrix" |
| Z2-R-MAINT-097 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Gate headroom triage" |
| Z2-R-MAINT-098, -099 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps" |
| Z2-R-MAINT-100 | this file → "Diagnostics and profiling" |
| Z2-R-MAINT-101 | this file → "Public API"; this file → "Diagnostics and profiling" |
| Z2-R-MAINT-102 | this file → "Failure handling" |
| Z2-R-MAINT-103 | this file → "Failure handling"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Release and roll procedures" |
| Z2-R-MAINT-104 | this file → "Threading lifetime and error contract" |
| Z2-R-MAINT-105, -117 | `Zenith2/Docs/Design/MAINT/Tests.md` → "Test registration" |
| Z2-R-MAINT-106 | `Zenith2/Docs/Design/MAINT/Tests.md` → "PowerShell check tests" |
| Z2-R-MAINT-107 | this file → "Authoring and cooking" |
| Z2-R-MAINT-109 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Provisional changes pending Q97"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Compatibility windows" |
| Z2-R-MAINT-113 | `Zenith2/Docs/Design/MAINT/Tests.md` → "Acceptance checks"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership checks" |
| Z2-R-MAINT-115 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership checks"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership file" |
| Z2-R-MAINT-116 | `Zenith2/Docs/Design/MAINT/Tests.md` → "Fixture layout" |
| Z2-R-MAINT-118 | `Zenith2/Docs/Design/MAINT/Tests.md` → "Research record rules" |
| Z2-R-MAINT-119 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal procedure"; `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal registry" |
| Z2-R-MAINT-121 | `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal rules" |
| Z2-R-MAINT-122 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Release step"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecations at Stage 7" |
| Z2-R-MAINT-123 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Deprecations at Stage 7"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine" |
| Z2-R-MAINT-126 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "API surface listing"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Roll step" |
| Z2-R-MAINT-127 | `Zenith2/Docs/Design/MAINT/Versioning.md` → "Stability classes"; `Zenith2/Docs/Design/MAINT/Versioning.md` → "Provisional changes pending Q97" |
