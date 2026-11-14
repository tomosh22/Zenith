# MAINT design — tests

This file is part of the MAINT design family (entry: `Zenith2/Docs/Design/MAINT.md`). It names every MAINT test, the case each asserts, its fixtures, how it is registered and which gate step or acceptance run executes it.

## Scope of this file

| Kind | Section |
|---|---|
| how the tests are registered | "Test registration" |
| the ZenithCore doctest cases of the engine version header | "C++ tests" |
| the PowerShell checks over the committed tree | "Live checks" |
| the rules over committed research records | "Research record rules" |
| the PowerShell checks over fixtures, one or more per rule | "PowerShell check tests" |
| the fixture directories and their non-source extension | "Fixture layout" |
| the checks only an acceptance runs | "Acceptance checks" |
| the step that runs each | "Which gate step runs each" |

## Test registration

<!-- elaboration -->
| Kind | Registered with | Name form | Labels | Presets | Runner |
|---|---|---|---|---|---|
| doctest case | doctest discovery of `ZenithCore_Tests` ([Z2-N-ARCH-179]) | `ZenithCore/EngineVersion/<Behaviour>` (ID `Test_EngineVersion_<Behaviour>`) | ZenithCore's `TEST_LABELS` | `win`, `win-asan`, `linux-tsan` | `ZenithCore_Tests` |
| PowerShell check | SHIP's `zenith_register_pwsh_test` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_register_script_test") in a MAINT block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `Pwsh_Maint_<Family>_<Case>` | `z2-pwsh`, `maint` | `win` | `Zenith2/Tools/Maintenance/Tests/Maintenance.Tests.ps1 -Case <Family>_<Case>` |

Every PowerShell case:

1. empties and creates `${ZENITH_OUT}/test-fixtures/MAINT/<Case>/` and works only there and in read-only reads of the repository;
2. for a fixture case, copies `Zenith2/Tools/Maintenance/Tests/Fixtures/<Family>/<Case>/` there, stripping the `.fixture` suffix of every file name that carries one ("Fixture layout"), and runs the module function under test with `-Repo` pointing at the copy;
3. compares the set of rule IDs in the result's findings with the case's `expect.json` (`{ "schema": "zenith2.maint-fixture", "schemaVersion": 1, "exitCode": <n>, "rules": [ … ] }`), and for a live case asserts exit 0 over the real tree;
4. exits 0 on pass and 1 with one line `FAIL <Case>: <reason>` otherwise.

`Maintenance.Tests.ps1 -List` prints every case name the script implements, one per line, byte-ordered. The MAINT block registers every case with one `zenith_register_pwsh_test` call per case, and the case `Pwsh_Maint_Registry_AllCasesRegistered` compares `-List` with the `Pwsh_Maint_*` names of `ctest --show-only=json-v1` in the `win` tree and fails (`REG-UNREGISTERED`, `REG-UNKNOWN`) on any case implemented and not registered or registered and not implemented, so a new case is registered in the diff that adds it.

## C++ tests

File `Zenith2/Engine/ZenithCore/Tests/Zenith_EngineVersion_Tests.cpp`.

| Test ID | Asserts |
|---|---|
| `Test_EngineVersion_ConstantsAgree` | `Zenith_FormatEngineVersion(Zenith_GetEngineVersion(), buffer)` produces exactly `szZENITH_ENGINE_VERSION`; `bZENITH_ENGINE_VERSION_PRERELEASE == false` implies `uZENITH_ENGINE_VERSION_DEV == 0`; `uZENITH_ENGINE_VERSION_MINOR == 0` |
| `Test_EngineVersion_ParseRelease` | `"2.0.0"` and `"6.0.3"` parse to the expected members with `m_bPrerelease == false` |
| `Test_EngineVersion_ParseDevelopment` | `"3.0.0-dev.17"` parses with `m_bPrerelease == true`, `m_uDevCounter == 17`; `"3.0.0-dev.0"` with counter 0 |
| `Test_EngineVersion_ParseLimits` | `"65535.65535.65535-dev.65534"` parses; `"65536.0.0"` returns `COMPONENT_RANGE`; `"1.0.0-dev.65535"` returns `DEV_COUNTER_RANGE` |
| `Test_EngineVersion_ParseMalformed_ReturnsError` | each of `""`, `"1"`, `"1.0"`, `"1.0.0."`, `"01.0.0"`, `"1.00.0"`, `"1.0.0-dev"`, `"1.0.0-dev."`, `"1.0.0-dev.01"`, `"1.0.0-rc.1"`, `"1.0.0 "`, `"1.0.0-dev.1x"`, `"a.b.c"`, a 28-byte string of digits and dots returns `MALFORMED` and never asserts |
| `Test_EngineVersion_FormatRoundTrips` | for 48 values (every combination of major ∈ {0, 2, 7, 65535}, patch ∈ {0, 1, 65535}, prerelease with counters {0, 1, 65534} and release), format then parse returns the same value |
| `Test_EngineVersion_FormatIntoSmallBuffer_ReturnsError` | a 5-byte destination for `"2.0.0"` (length 5, NUL needed) returns `BUFFER_TOO_SMALL` and leaves the destination's bytes unchanged; a 6-byte destination succeeds with length 5 |
| `Test_EngineVersion_KeyOrdersByPrecedence` | for the ordered list `0.0.0-dev.0 < 0.0.0-dev.1 < 0.0.0 < 0.0.1 < 2.0.0-dev.0 < 2.0.0-dev.65534 < 2.0.0 < 2.0.1 < 3.0.0-dev.0`, every pair's keys and `Zenith_CompareEngineVersions` agree with the list order |
| `Test_EngineVersion_ToStringCoversEveryError` | `Zenith_ToString` returns a distinct non-empty string for each of the four enumerators |

## Live checks

Each runs the named family's function over the committed tree and asserts no `fail` finding. All are registered PowerShell cases.

| Test | Family function | What the live tree must satisfy |
|---|---|---|
| `Pwsh_Maint_Live_Version` | `Test-Zenith2EngineVersionHeader` | the header has exactly the six constants of "Zenith_EngineVersion header" with values that agree (rule `VER-*`) |
| `Pwsh_Maint_Live_Changelog` | `Test-Zenith2EngineChangelog` | every `CHG-*` rule |
| `Pwsh_Maint_Live_Classification` | `Test-Zenith2ApiClassification` | every `CLS-*` rule over every public header |
| `Pwsh_Maint_Live_Deprecations` | `Test-Zenith2Deprecations` | every `DEP-*` rule over the scan set |
| `Pwsh_Maint_Live_Surface` | `Test-Zenith2ApiSurface` | every `SUR-*` rule |
| `Pwsh_Maint_Live_Ownership` | `Test-Zenith2Ownership` | every `OWN-*` rule at `Test` severity; skips `OWN-TARGETS` with the report finding `MAINT-NO-CONFIGURE` when `${ZENITH_OUT}/declared_deps.json` is absent |
| `Pwsh_Maint_Live_Rehearsals` | `Test-Zenith2Rehearsals` | `REH-SCHEMA`, `REH-RECORDS`, `REH-NONEWER` |
| `Pwsh_Maint_Live_SampleGames` | `Test-Zenith2SampleGames` | every `SG-*` rule at `Test` severity |
| `Pwsh_Maint_Live_Reports` | `Test-Zenith2ResearchRecords` | every `RES-*` rule over `Zenith2/Docs/Research/` |
| `Pwsh_Maint_Live_FixturesNotSources` | `Test-Zenith2FixtureNames` | `git ls-files Zenith2/Tools/Maintenance` lists no path ending `.h`, `.hpp`, `.inl`, `.cpp` or `.c`, and none ending `_CompileFail.cpp.in`, which PROC's source set also counts (rule `FIX-SOURCE`) |

The `VER` rules read the header by text: one line each `constexpr uint16 uZENITH_ENGINE_VERSION_<NAME> = <decimal>;` for MAJOR, MINOR, PATCH, DEV, one `constexpr bool bZENITH_ENGINE_VERSION_PRERELEASE = <true|false>;`, one `constexpr const char* szZENITH_ENGINE_VERSION = "<text>";`; `VER-LINES` fails a missing or duplicated line, `VER-AGREE` a text that differs from the numbers, `VER-MINOR` a non-zero MINOR, `VER-DEV` a non-zero DEV with PRERELEASE false.

## Research record rules

| Rule ID | Rule |
|---|---|
| RES-SCHEMA | every `surface_growth.json`, `calibration.json` and `Rehearsal.json` under `Zenith2/Docs/Research/` parses with its `schema` and `schemaVersion` 1, and sits in the directory its schema names (`Libgraph/stage-<N>/`, `Complexity/stage-<N>/`, `Upgrades/<name>/<from>-to-<to>/`) |
| RES-INCOMPLETE | no committed `surface_growth.md`, `calibration.md` or `Rehearsal.md` contains the line `Written by the release operator.` or `Decision: written by the ruling.`, no committed `calibration.json` has `decision: null`, every `surface_growth.md` has a non-empty `## Disposition`, and each JSON record has its Markdown twin |

## PowerShell check tests

One fixture case per rule at least; each fixture is a minimal tree under `Fixtures/<Family>/<Case>/` that triggers exactly the listed rules.

<!-- elaboration -->
| Test | Fixture holds | Expected rules |
|---|---|---|
| `Pwsh_Maint_Version_MissingLine` | a header without the DEV line | `VER-LINES` |
| `Pwsh_Maint_Version_TextDisagrees` | numbers `2.0.0` dev 3 prerelease, text `"2.0.0-dev.2"` | `VER-AGREE` |
| `Pwsh_Maint_Version_NonZeroMinor` | MINOR 1 and the matching text | `VER-MINOR` |
| `Pwsh_Maint_Version_DevWithoutPrerelease` | DEV 2, PRERELEASE false | `VER-DEV` |
| `Pwsh_Maint_Changelog_Valid` | a history over stages 2 and 3: roll 2.0.0-dev.0, two compat, release 2.0.0, release-fix 2.0.1, roll 3.0.0-dev.0, header 3.0.0-dev.0 | none, exit 0 |
| `Pwsh_Maint_Changelog_BadSchema` | an unknown member `kinds` | `CHG-SCHEMA` |
| `Pwsh_Maint_Changelog_FirstNotRoll` | a first `compat` entry | `CHG-FIRST` |
| `Pwsh_Maint_Changelog_CompatInRelease` | a `compat` after `release` at stage 3 | `CHG-TRANSITION` |
| `Pwsh_Maint_Changelog_CompatAfterStage7Release` | a `compat` after `release` 7.0.0, versioned 7.0.1 | none, exit 0 |
| `Pwsh_Maint_Changelog_RollFromDev` | a `roll` directly after a `compat` | `CHG-TRANSITION` |
| `Pwsh_Maint_Changelog_SkippedCounter` | compat entries `-dev.1` then `-dev.3` | `CHG-VERSION-STEP` |
| `Pwsh_Maint_Changelog_StageMismatch` | `stage` 3 on a `2.0.0-dev.1` entry | `CHG-STAGE` |
| `Pwsh_Maint_Changelog_HeaderBehind` | last entry `2.0.0-dev.4`, header `2.0.0-dev.3` | `CHG-HEADER` |
| `Pwsh_Maint_Changelog_BadDraftId` | `ticket` `Z2T-MAINT-1` | `CHG-DRAFTID` |
| `Pwsh_Maint_Changelog_BoardKeyValid` | a `compat` entry with `ticket` `Z2-1432` (a human-filed removal TASK) | none, exit 0 |
| `Pwsh_Maint_Changelog_CompatWithoutItems` | a `compat` with `items: []` | `CHG-ITEM` |
| `Pwsh_Maint_Changelog_ReleasedMismatch` | a `roll` whose `released.version` is `2.0.0` after `2.0.1` | `CHG-RELEASED` |
| `Pwsh_Maint_Changelog_RollAfterStage7` | a `roll` from `RELEASE(7, 0)` | `CHG-TRANSITION` |
| `Pwsh_Maint_Classification_ExistingFreeForms` | blocks with `since Z2 Stage 2; type version 1`, `since Z2 Stage 2. Source API; deprecations per …`, `since Z2 Stage 2 (registered by the API-only slice; behaviour since Stage 4)`, `since Z2 Stage 3: …`, `since Z2 Stage 2 as …`, `since Z2 Stage 2, …` | none, exit 0 |
| `Pwsh_Maint_Classification_SinceBeforeFirstVersion` | a `since Z2 Stage 1` block at MAJOR 2 | none; `Get-Zenith2PublicBlocks` reports it provisional (effective since 2) |
| `Pwsh_Maint_Classification_NoSince` | `Version: Stage 2` | `CLS-SINCE` |
| `Pwsh_Maint_Classification_MalformedExperimental` | `experimental until Stage 4` | `CLS-PREFIX` |
| `Pwsh_Maint_Classification_TwoClasses` | `internal; internal` | `CLS-ONE` |
| `Pwsh_Maint_Classification_ExperimentalAndDeprecated` | both clauses | `CLS-EXCLUSIVE` |
| `Pwsh_Maint_Classification_GraduatesBeforeSince` | `since Z2 Stage 4; experimental until Z2 Stage 3 (consumers ZenithFlux)` | `CLS-GRADUATE` |
| `Pwsh_Maint_Classification_Expired` | `experimental until Z2 Stage 2` with header MAJOR 3 | `CLS-EXPIRED` |
| `Pwsh_Maint_Classification_UnknownConsumer` | consumer `ZenithNope` absent from the fixture's `declared_deps.json` | `CLS-CONSUMERS` |
| `Pwsh_Maint_Classification_MixedHeader` | one experimental and one stable block in one header | `CLS-HOMOGENEOUS` |
| `Pwsh_Maint_Classification_ExperimentalIncludedOutside` | a `ZenithAudio/Internal/X.cpp` including an experimental `ZenithFlux` header whose consumers are `ZenithUI` | `CLS-INCLUDE` |
| `Pwsh_Maint_Classification_ExperimentalIncludedByTests` | the declaring library's `Tests/` including it | none, exit 0 |
| `Pwsh_Maint_Classification_InternalIncludedByModule` | a `ZenithAI/Internal/X.cpp` including `ZenithRuntime/Zenith_RuntimeTestAccess.h` | `CLS-INCLUDE` |
| `Pwsh_Maint_Classification_InternalIncludedByRuntime` | `ZenithRuntime` including a backend factory header | none, exit 0 |
| `Pwsh_Maint_Classification_GeneratedHeaderNotInternal` | a `ZenithUI/Internal/X.cpp` including `Flux/Shaders/Flux_UI.generated.h` from a fixture `out/generated/` tree | none, exit 0 (public-generated headers are outside the scan) |
| `Pwsh_Maint_Classification_RedundantInternal` | an `internal` clause in a `...TestAccess.h` | `CLS-REDUNDANT` (report; exit 0) |
| `Pwsh_Maint_Deprecation_Valid` | a type block deprecated with D = MAJOR, its attribute, a replacement declared in another header, a changelog item | none, exit 0 |
| `Pwsh_Maint_Deprecation_BadMessage` | `[[deprecated("old")]]` | `DEP-MESSAGE`, `DEP-PAIRED` |
| `Pwsh_Maint_Deprecation_AttributeWithoutClause` | the attribute on a type whose block has no clause and no deviation line | `DEP-PAIRED` |
| `Pwsh_Maint_Deprecation_ClauseWithoutAttribute` | a deprecated clause on a type without the attribute | `DEP-PAIRED` |
| `Pwsh_Maint_Deprecation_FunctionsBlockPartial` | a deprecated `functions` block of three function declarations, none with `noexcept` and one split over two lines, where one of the three lacks the attribute | `DEP-PAIRED` |
| `Pwsh_Maint_Deprecation_MemberDeviation` | a member function with a matching `@deviation Version: deprecated …` line two lines above its attribute | none, exit 0 |
| `Pwsh_Maint_Deprecation_ReplacementMismatch` | clause `use Zenith_A`, attribute `use Zenith_B` | `DEP-PAIRED` |
| `Pwsh_Maint_Deprecation_FutureStage` | D = MAJOR + 1 | `DEP-STAGES` |
| `Pwsh_Maint_Deprecation_RemovalNotNext` | D = 2, removal 4 | `DEP-STAGES` |
| `Pwsh_Maint_Deprecation_BeforeSince` | `since Z2 Stage 3`, D = 2, header MAJOR 3 | `DEP-STAGES` |
| `Pwsh_Maint_Deprecation_PulledForwardBlock` | `since Z2 Stage 4`, D = 3, header MAJOR 3 | none, exit 0 (D = `min(4, 3)`) |
| `Pwsh_Maint_Deprecation_Stage7RemovalEight` | D = 7, removal 8, header `7.0.1` | none, exit 0 |
| `Pwsh_Maint_Deprecation_Overdue` | D = 2, removal 3, header MAJOR 4 | `DEP-OVERDUE` |
| `Pwsh_Maint_Deprecation_RemovalDueStillPresent` | D = 2, removal 3, header MAJOR 3 | none, exit 0 (removal is due this stage) |
| `Pwsh_Maint_Deprecation_ReplacementMissing` | replacement `Zenith_Nowhere` | `DEP-REPLACEMENT` |
| `Pwsh_Maint_Deprecation_ReplacementOnlySelf` | replacement equal to a name that occurs only in the deprecated declaration's own line | `DEP-REPLACEMENT` |
| `Pwsh_Maint_Deprecation_ShippedNotLogged` | a listed shipped block deprecated with D = MAJOR and no changelog item | `DEP-LOGGED` |
| `Pwsh_Maint_Deprecation_InInternal` | the attribute in `ZenithCore/Internal/X.cpp` | `DEP-ONLY-ZENITH2` |
| `Pwsh_Maint_Deprecation_CompileFailFixtureIgnored` | a `Zenith2/CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in` with SHIP's deprecated declaration | none, exit 0 |
| `Pwsh_Maint_Surface_Valid` | a listing with three records matching the fixture tree | none, exit 0 |
| `Pwsh_Maint_Surface_SchemaBad` | records out of byte order | `SUR-SCHEMA` |
| `Pwsh_Maint_Surface_ShippedRemovedWithoutDeprecation` | a record with `deprecatedIn: null` whose block is absent | `SUR-SHIPPED-MISSING` |
| `Pwsh_Maint_Surface_PermittedRemoval` | a record with `deprecatedIn: 2`, MAJOR 3, block absent | none (report `SUR-SHIPPED-MISSING` pending), exit 0 |
| `Pwsh_Maint_Surface_Unlisted` | a stable block with `since Z2 Stage 2`, `shippedThrough` 2, no record | `SUR-UNLISTED` |
| `Pwsh_Maint_Surface_ProvisionalNotListed` | a block with `since Z2 Stage 4`, `shippedThrough` 2, no record | none, exit 0 |
| `Pwsh_Maint_Surface_EarlySinceListedAtFirstRoll` | a `since Z2 Stage 1` block, `shippedThrough` null, then the roll of Stage 2 | before: none; after: one record with `since` 2 |
| `Pwsh_Maint_Surface_SinceEdited` | record `since` 2, block `since` 3 | `SUR-SINCE` |
| `Pwsh_Maint_Surface_ShippedMadeExperimental` | a recorded block now experimental | `SUR-CLASS` |
| `Pwsh_Maint_Surface_BackdatedDeprecation` | record `deprecatedIn: null`, block deprecated with D = MAJOR − 1 | `SUR-DEPRECATION` |
| `Pwsh_Maint_Surface_DeprecationChanged` | record `deprecatedIn: 2`, block D = 3 | `SUR-DEPRECATION` |
| `Pwsh_Maint_Surface_Undeprecated` | record `deprecatedIn: 2`, block not deprecated | `SUR-UNDEPRECATED` (report; exit 0) |
| `Pwsh_Maint_Surface_NameReused` | a record with `removedIn: 3` whose name is declared again | `SUR-REUSED` (report; exit 0) |
| `Pwsh_Maint_Surface_HeaderMoved` | a recorded block found in another root header of its library | `SUR-MOVED` (report; exit 0) |
| `Pwsh_Maint_Surface_LuauNotAvailable` | a roll over a fixture with no `bindings.json` | `SUR-LUAU-NOT-AVAILABLE` (report); `luau[]` unchanged |
| `Pwsh_Maint_Ownership_Valid` | an owners file, an Architecture extract, a Roadmap extract, an Inventory extract and a `declared_deps.json`, all consistent | none, exit 0 |
| `Pwsh_Maint_Ownership_UnknownArea` | a row owned by `MAINTX` | `OWN-SCHEMA` |
| `Pwsh_Maint_Ownership_EpicMismatch` | MAINT's epic `Z2 MAINT — Maintenance` | `OWN-AREAS` |
| `Pwsh_Maint_Ownership_ArchitectureDisagrees` | `Zenith2/Engine/ZenithAI/` owned by NAV in the file and AI in the table | `OWN-ARCH` |
| `Pwsh_Maint_Ownership_UnderscoreBackendRows` | a table row `` `Engine/ZenithCompression/`, `_Zlib/`, `_Zstd/` `` and rows for `ZenithCompression/`, `ZenithCompression_Zlib/`, `ZenithCompression_Zstd/` (not `ZenithCompression_Zlib_Zstd/`) | none, exit 0 |
| `Pwsh_Maint_Ownership_ThirdPartyCMakeRow` | the row `` `Tools/Setup/`, `ThirdParty/`, `CMake/` `` SHIP and rows `Zenith2/Tools/Setup/`, `Zenith2/ThirdParty/`, `Zenith2/CMake/` owned by SHIP | none, exit 0 (no `Zenith2/Tools/ThirdParty/`) |
| `Pwsh_Maint_Ownership_BareSiblingPaths` | the row `` `Games/OpenWorldSlice/`, `CinematicSlice/`, `Game2D/` `` "GAME, CINE, TWOD respectively" and rows mapping the three games to GAME, CINE, TWOD | none, exit 0 |
| `Pwsh_Maint_Ownership_GlobRow` | the row `` `Games/Workload*/` `` "RPROD (rendering workloads), ECS (WorkloadDense), UI (WorkloadUI2D)", a MOD workload-games extract, five workload directories, one of them owned by AUDIO in the file | `OWN-ARCH` |
| `Pwsh_Maint_Ownership_WorkloadCharactersAnim` | the same row and extract, `WorkloadCharacters` owned by ANIM in the file and the extract, the other four as the extract gives | none, exit 0 (MOD's table decides; the cell omits ANIM) |
| `Pwsh_Maint_Ownership_MultiOwnerRow` | the row `` `Tests/` `` "the area of each suite (Lifecycle: GAME; Determinism: ECS)" with both suites owned accordingly | none, exit 0 |
| `Pwsh_Maint_Ownership_PerSuiteChildOtherOwner` | the same row and a suite `Zenith2/Tests/NetLoopback/` owned by NET, a code the cell does not name | none, exit 0 (an aggregate row checks no child against its examples) |
| `Pwsh_Maint_Ownership_AggregateLeadingCode` | the row `` `Benchmarks/` `` "RESEARCH (per harness: …)" and `Zenith2/Benchmarks/` owned by RPROD in the file | `OWN-ARCH` |
| `Pwsh_Maint_Ownership_TargetOnFallback` | a library `ZenithNew` with no row | `OWN-TARGETS` (report; exit 0) |
| `Pwsh_Maint_Ownership_DirectoryUnowned` | a directory `Zenith2/Tests/NewSuite/` with no row | `OWN-DIRECTORIES` (report; exit 0) |
| `Pwsh_Maint_Ownership_DependencyUnowned` | a manifest entry owned by an undeclared target | `OWN-DEPENDENCIES` |
| `Pwsh_Maint_Ownership_StaleRow` | a row for a missing directory | `OWN-STALE` (report; exit 0) |
| `Pwsh_Maint_Ownership_RootFileUnowned` | a tracked `Zenith2/.clang-format` with no file row | `OWN-ROOT-FILES` |
| `Pwsh_Maint_Ownership_ResolveLongestPrefix` | rows `Zenith2/Engine/ZenithFlux/` (RPROD) and `Zenith2/Engine/ZenithFlux/Internal/Environment/` (ENV) | `Get-Zenith2Owner` returns ENV for `…/Internal/Environment/Zenith_Sky.cpp`, RPROD for `…/Internal/Graph/X.cpp`, MAINT via fallback for `Zenith2/Unknown/x` |
| `Pwsh_Maint_Ownership_NoConfigure` | the fixture without `out/declared_deps.json` | `MAINT-NO-CONFIGURE` (report; exit 0) |
| `Pwsh_Maint_Rehearsals_Valid` | a registry with one row per fixture manifest entry and two records | none, exit 0 |
| `Pwsh_Maint_Rehearsals_NoNewerShapeBad` | a `no-newer` record with one `builds` entry and `newestUpstream` `5.3.0` beside `fromVersion` `5.2.0` | `REH-NONEWER` |
| `Pwsh_Maint_Rehearsals_NoNewerPinDiffers` | a `no-newer` record whose `fromVersion` differs from the fixture manifest's pin at `baseCommit` (the case's git repository) | `REH-NONEWER` |
| `Pwsh_Maint_Rehearsals_MissingRowReported` | a manifest entry with no registry row, `-Acceptance -Stage 3` | `REH-ROWS-MISSING` (report; exit 0) |
| `Pwsh_Maint_Rehearsals_RevisitStageLowersDue` | a tools row with `last.stage` 2, verdict `defer`, `revisitStage` 3 | `Get-Zenith2RehearsalPlan -Stage 3` lists it (cadence alone would give 4) |
| `Pwsh_Maint_Rehearsals_BadVerdict` | `verdict` `maybe` | `REH-SCHEMA` |
| `Pwsh_Maint_Rehearsals_FromToolchainRow` | a `slang` row under `section: dependencies` beside its toolchain row | `REH-ROWS` |
| `Pwsh_Maint_Rehearsals_RecordMissing` | `last.record` pointing at no file | `REH-RECORDS` |
| `Pwsh_Maint_Rehearsals_NoNewerSatisfies` | a runtime row due at stage 4 whose stage-4 record is `no-newer` | `Test-Zenith2Rehearsals -Acceptance -Stage 4` passes |
| `Pwsh_Maint_Rehearsals_Overdue` | a runtime row with `last.stage` 2 at the acceptance of stage 4 | `REH-OVERDUE` |
| `Pwsh_Maint_Rehearsals_DueComputation` | rows with tiers runtime, tools, toolchain and `last` null or set | `Get-Zenith2RehearsalPlan -Stage 4` lists exactly the rows whose `dueBy ≤ 4`, each with the right `reason` and owner epic |
| `Pwsh_Maint_Rehearsals_PreQualification` | a tools row rehearsed in Stage 3 | `Get-Zenith2RehearsalPlan -Stage 6` lists it with `pre-qualification`; `Test-Zenith2Rehearsals -Acceptance -Stage 6` fails `REH-OVERDUE` until a Stage 5 or 6 record exists |
| `Pwsh_Maint_Rehearsals_RecordScaffold` | a fixture manifest and header | `New-Zenith2RehearsalRecord` writes a record whose filled members equal the fixture's values and whose remaining members are empty |
| `Pwsh_Maint_SampleGames_Valid` | a games list matching four fixture games | none, exit 0 |
| `Pwsh_Maint_SampleGames_SchemaUnordered` | `games[]` out of byte order | `SG-SCHEMA` |
| `Pwsh_Maint_SampleGames_OwnerDiffers` | listed owner `RPROD`, owners file `GAME` | `SG-OWNER` |
| `Pwsh_Maint_SampleGames_DueGameMissing` | a game with `firstBuildStage` 2, MAJOR 3, no directory | `SG-DUE` |
| `Pwsh_Maint_SampleGames_NotYetDue` | the same game with MAJOR 2 | none, exit 0 |
| `Pwsh_Maint_SampleGames_PlatformsDiffer` | listed `android`, `windows`; `game.json` `windows` | `SG-PLATFORMS` |
| `Pwsh_Maint_SampleGames_NoGametest` | a due game without `Tests/*.gametest.json` | `SG-GAMETESTS` |
| `Pwsh_Maint_SampleGames_ModuleUnselected` | a `KIND MODULE MODULE_NAME Audio` library no game selects, not pending | `SG-MODULES` |
| `Pwsh_Maint_SampleGames_ModulePending` | the same module pending until MAJOR | none, exit 0 |
| `Pwsh_Maint_SampleGames_BackendUnselected` | a physics backend `PhysX` no game selects | `SG-BACKENDS` |
| `Pwsh_Maint_SampleGames_Unlisted` | a `Games/Extra/game.json` absent from `games[]` | `SG-UNLISTED` (report; exit 0) |
| `Pwsh_Maint_SampleGames_PendingStale` | a pending module some game selects | `SG-PENDING-STALE` (report; exit 0) |
| `Pwsh_Maint_Complexity_ProposalNeverLoosens` | entities whose observed max is above the current thresholds (an on-demand measurement of a failing tree) | `new = current` for those metrics (`CPX-NEVER-LOOSEN`, `CPX-FAILING-TREE`) |
| `Pwsh_Maint_Complexity_ProposalHeadroom` | max-cc observed 17, current 25 | `new` 22 (`ceil(17 × 1.25)`; `CPX-HEADROOM`) |
| `Pwsh_Maint_Complexity_ProposalRespectsFloor` | max-cc observed 6, current 25 | `new` 15 (`CPX-FLOOR`) |
| `Pwsh_Maint_Complexity_ProposalFileLines` | max-file-loc observed 1,042, current 1,500 | `new` 1,303 |
| `Pwsh_Maint_Complexity_PassingTreeStaysGreen` | an entity file that passes the current profile | every entity passes the proposed profile (`CPX-PASSING-TREE`) |
| `Pwsh_Maint_Complexity_PercentilesNearestRank` | 200 hand-written cc values | p50, p90, p99, p99.5 and max equal the values derivable by hand |
| `Pwsh_Maint_Complexity_ExceptionAudit` | four exceptions: gone, now under threshold, tightened, kept | actions `removed-entity-gone`, `removed-under-threshold`, `tightened` with `ceil(measured × 1.25)`, `kept` (`CPX-AUDIT`) |
| `Pwsh_Maint_Complexity_ProposedProfileMinimalDiff` | a profile file and a proposal | `complexity_profile.proposed.json` differs from the input only in `fail_on` values and `exceptions` (`CPX-MINIMAL-DIFF`) |
| `Pwsh_Maint_Complexity_BaseShaRecorded` | a profile file | `calibration.json`'s `baseSha256` equals the file's SHA-256 computed by .NET in the case (`CPX-BASE`) |
| `Pwsh_Maint_Complexity_NoDumpOption` | a stand-in analyzer script without `--dump-entities` | the harness exits 3 with the message of "Complexity measurement harness" |
| `Pwsh_Maint_SurfaceReport_FromLibgraphOutputs` | hand-written `surface.json`, `surface_delta.json` in the assumed PROC schemas, a baseline and a previous report | `New-Zenith2SurfaceGrowthReport` writes the JSON of "Surface growth report" with the expected `largestGrowth` flags (a 30 % grower, a 120-pair grower and a library with a new consumer flagged; a 10-pair grower not) |
| `Pwsh_Maint_SurfaceReport_FirstReportNoConsumerCriterion` | the same without a previous report | `consumersBefore` null and only the size criteria flag |
| `Pwsh_Maint_Records_Incomplete` | a committed report still holding the operator line | `RES-INCOMPLETE` |
| `Pwsh_Maint_Records_WrongDirectory` | a `calibration.json` under `Complexity/4.0.0/` | `RES-SCHEMA` |
| `Pwsh_Maint_Release_SetsReleaseVersion` | a fixture git repository at `DEV(3, 5)` | `Set-Zenith2EngineRelease -Stage 3` rewrites the header to `3.0.0` and appends a valid `release` entry; nothing else changes |
| `Pwsh_Maint_Release_Stage7ListsSurface` | a fixture repository at `DEV(7, 2)` with one stable block since 7 | the header reads `7.0.0` and the listing gains the since-7 block with `shippedThrough` 7 |
| `Pwsh_Maint_Release_RefusesDirtyTree` | the same repository with an untracked file | exit 4, files unchanged |
| `Pwsh_Maint_Release_RefusesWrongStage` | `-Stage 4` at `DEV(3, 5)` | exit 4 |
| `Pwsh_Maint_Roll_Valid` | a fixture repository at `RELEASE(3, 1)` with one stable block since 3, one deprecated block D = 3, one block removed after D = 2, one experimental block G = 4 | the header reads `4.0.0-dev.0`; the listing gains the since-3 block, records `deprecatedIn: 3`, sets `removedIn: 3`; the plan lists the D = 3 removal and the G = 4 graduation with their owner epics; the roll entry's `released` is `{ "3.0.1", <HEAD> }` |
| `Pwsh_Maint_Roll_RefusesOverdue` | the same with a block D = 2 still present | exit 4 naming `DEP-OVERDUE`; files unchanged |
| `Pwsh_Maint_Roll_RefusesExpired` | an experimental block with G = 3 | exit 4 naming `CLS-EXPIRED` |
| `Pwsh_Maint_Roll_RefusesStage7` | `RELEASE(7, 0)` | exit 4 |
| `Pwsh_Maint_Roll_InterruptedRestores` | a fault hook (`ZENITH2_MAINT_FAULT_AFTER_RENAME=1` read by the script's test seam) failing after the first rename | every target restored; exit 4 |
| `Pwsh_Maint_Roll_AddsMissingRehearsalRows` | a manifest entry with no registry row | the roll adds the row with `pinnedStage` 4 |
| `Pwsh_Maint_Status_ReportSections` | a fixture tree with one item of each kind | `-Report` writes `status.json` (`zenith2.maintenance-status`) and `status.md` with the seven sections of `Zenith2/Docs/Design/MAINT.md` → "Diagnostics and profiling", in order, each listing the fixture's item |
| `Pwsh_Maint_CheckReport_Shape` | a fixture with one `fail` and one `report` finding | `<Check>.json` has the members of "Check reports" in order, `counts` `{ "fail": 1, "report": 1 }`, and the verdict line `Maintenance: FAIL (1 finding(s)); report <path>` |
| `Pwsh_Maint_Input_MissingFile_Exit3` | a tree without `engine_changelog.json` | exit 3 naming the file |
| `Pwsh_Maint_Input_Bom_Exit3` | `subsystem_owners.json` with a UTF-8 byte-order mark | exit 3 |
| `Pwsh_Maint_Input_NewerSchema_Exit3` | `sample_games.json` with `schemaVersion` 2 | exit 3 naming both versions |
| `Pwsh_Maint_Input_HeaderNotUtf8` | a root header with an invalid UTF-8 byte | `MAINT-ENCODING` at line 0; the other headers are scanned |
| `Pwsh_Maint_Usage_PathOutsideZenith2_Exit2` | `Get-Zenith2Owner.ps1 -Path Tools/x` | exit 2 |
| `Pwsh_Maint_Git_Missing_Exit5` | the roll with `PATH` pointing at a `git.cmd` stand-in that exits 128 | exit 5 naming the command |
| `Pwsh_Maint_Fixtures_SourceNameRefused` | a fixture tree file named `X.h` (not `.h.fixture`) given to `Test-Zenith2FixtureNames` | `FIX-SOURCE` |
| `Pwsh_Maint_Fixtures_CompileFailNameRefused` | a fixture tree file named `X_CompileFail.cpp.in` (not `.cpp.in.fixture`) | `FIX-SOURCE` |
| `Pwsh_Maint_Registry_AllCasesRegistered` | the live `-List` output and the `win` tree's CTest listing | no `REG-UNREGISTERED` or `REG-UNKNOWN` finding |
| `Pwsh_Maint_Registry_UnregisteredCase` | a fixture `-List` output naming `Version_Extra` and a fixture CTest JSON without `Pwsh_Maint_Version_Extra` | `REG-UNREGISTERED` |
| `Pwsh_Maint_Registry_UnknownRegistration` | a fixture CTest JSON naming `Pwsh_Maint_Version_Gone` that the fixture `-List` output lacks | `REG-UNKNOWN` |
| `Pwsh_Maint_Acceptance_DepDue_Fails` | a deprecation with removal 3 at the acceptance of stage 3 | `ACC-DEP-DUE` |
| `Pwsh_Maint_Acceptance_ExperimentalDue_Fails` | an experimental block with G = 3 at the acceptance of stage 3 | `ACC-EXPERIMENTAL-DUE` |
| `Pwsh_Maint_Acceptance_Ownership_Fails` | a library resolving through the fallback | `ACC-OWNERSHIP` |
| `Pwsh_Maint_Acceptance_SampleGames_Fails` | a workload game missing at the acceptance of stage 3 | `ACC-SAMPLE-GAMES` |
| `Pwsh_Maint_Acceptance_Pending_Fails` | a pending module with `selectedByStage` 3 at the acceptance of stage 3 | `ACC-PENDING` |
| `Pwsh_Maint_Acceptance_Rehearsals_Fails` | an overdue registry row | `ACC-REHEARSALS` |
| `Pwsh_Maint_Acceptance_MissingRowDoesNotBlockRoll` | a fixture repository at `RELEASE(3, 0)` whose manifest has an entry with no registry row, every other check passing | `-Acceptance -Stage 3` exits 0 with `REH-ROWS-MISSING`; the roll then runs and adds the row |
| `Pwsh_Maint_Acceptance_SurfaceReport_Fails` | no `Libgraph/stage-3/surface_growth.md` | `ACC-SURFACE-REPORT` |
| `Pwsh_Maint_Acceptance_Calibration_Fails` | no `Complexity/stage-4/calibration.json` at the acceptance of stage 4 | `ACC-CALIBRATION` |
| `Pwsh_Maint_Acceptance_Calibration_ProfileNotApplied` | a complete stage-4 record whose `decision` heads a DecisionLog extract entry, `proposedSha256` ≠ `baseSha256`, and a profile still equal to the base | `ACC-CALIBRATION` |
| `Pwsh_Maint_Acceptance_Calibration_DecisionMissing` | the same record applied, with `decision` naming no heading of the extract | `ACC-CALIBRATION` |
| `Pwsh_Maint_Acceptance_CalibrationNotDueBeforeStage4` | no calibration record at the acceptance of stage 3 | none for `ACC-CALIBRATION` |
| `Pwsh_Maint_Acceptance_DeprecationHistory_Fails` | a changelog `deprecated` item with `removedIn` 3 whose listing record has `removedIn: null` at the acceptance of stage 4 | `ACC-DEPRECATION-HISTORY` |
| `Pwsh_Maint_Acceptance_DeprecationHistory_DueThisStagePasses` | a changelog `deprecated` item with `removedIn` 3, its listing record `removedIn: null`, the declaration absent, at the acceptance of stage 3 | none for `ACC-DEPRECATION-HISTORY` or `ACC-DEP-DUE` |
| `Pwsh_Maint_Acceptance_State_Fails` | the header at `DEV(3, 2)` for `-Acceptance -Stage 3` | `ACC-STATE` |

The fault hook of `Pwsh_Maint_Roll_InterruptedRestores` is an environment variable read by `Invoke-Zenith2VersionRoll.ps1` alone; a PowerShell tool script is not shipping code, so [Z2-N-CONTRACT-415] does not apply to it.

## Fixture layout

```text
Zenith2/Tools/Maintenance/Tests/
	Maintenance.Tests.ps1                 the runner: -Case <Family>_<Case>, -List
	Fixtures/<Family>/<Case>/             a minimal repository tree for that case:
		Zenith2/Engine/<Lib>/<Header>.h.fixture          hand-written public headers (text only; never compiled)
		Zenith2/Engine/<Lib>/Internal/<File>.cpp.fixture
		Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h.fixture
		Zenith2/Tools/Maintenance/*.json    the data files the case needs
		out/declared_deps.json              where the case needs one (the case passes it explicitly)
		Docs/Architecture.ownership.md      the extract of the ownership table the OWN cases parse
		Docs/MOD.games.md                   the extract of MOD's workload-games and flagship-slices tables
		Docs/DecisionLog.extract.md         the `### Z2-D-` headings the ACC-CALIBRATION cases read
		expect.json
```

Every fixture file whose name would end in a compiled-language extension (`.h`, `.hpp`, `.inl`, `.cpp`, `.c`, `.cpp.in`) carries the further suffix `.fixture`, and the runner strips that suffix when it copies the tree into `${ZENITH_OUT}/test-fixtures/MAINT/<Case>/`. So no fixture is a source to `SourceLayout`, `Format` or `ForbiddenCheck`, which scan `Zenith2/Tools/**` and exempt only `Zenith2/Tools/Gates/Tests/**` (`Zenith2/Docs/Design/PROC.md` → "The protected gate directory"); `Pwsh_Maint_Live_FixturesNotSources` keeps it so.

Fixtures are text files only, per PROC's rule (`Zenith2/Docs/Design/PROC.md` → "Fixture rule"). The release and roll cases create their git repository at test time (`git init`, `git add`, `git commit` with a fixed author and date in the case's directory), so no git object is committed. No fixture holds a digest; the `released.commit` the roll case checks is read back with `git rev-parse HEAD` in the case.

## Acceptance checks

`Test-Zenith2Maintenance.ps1 -Acceptance -Stage <N>` runs every live check at acceptance severity, then these, and exits 1 when any fails. It is step A4 of the acceptance, after the surface growth report and the re-pin (A2) and the calibration (A3), and the roll's precondition re-runs it.

<!-- elaboration -->
| ID | Check |
|---|---|
| ACC-DEP-DUE | no declaration carries a deprecation with removal stage ≤ N (removals of the accepted stage are done) |
| ACC-EXPERIMENTAL-DUE | no experimental block has G ≤ N |
| ACC-OWNERSHIP | `OWN-TARGETS`, `OWN-DIRECTORIES` and `OWN-STALE` at failure severity: nothing resolves through a fallback, no row is stale |
| ACC-SAMPLE-GAMES | `SG-UNLISTED` at failure severity; every game with `firstBuildStage ≤ N` is due-complete (`SG-DUE`, `SG-PLATFORMS`, `SG-GAMETESTS` evaluated as if MAJOR were N + 1) |
| ACC-PENDING | no `pending` item has `selectedByStage ≤ N` |
| ACC-REHEARSALS | `REH-ROWS` and `REH-OVERDUE` at stage N; at N = 6 every row has `last.stage ≥ 5`; a `no-newer` record that passes `REH-NONEWER` counts; a missing row is the report finding `REH-ROWS-MISSING`, never a failure, because the roll adds it |
| ACC-SURFACE-REPORT | `Zenith2/Docs/Research/Libgraph/stage-<N>/surface_growth.md` and `.json` exist and are complete |
| ACC-CALIBRATION | for N ≥ 4, `Zenith2/Docs/Research/Complexity/stage-<N>/calibration.json` exists; its `decision` member is not null and `Zenith2/Docs/DecisionLog.md` has a heading `### <decision> …`; and the PROC apply ticket ran: the SHA-256 of `Zenith2/Tools/Gates/complexity_profile.json` (encoding of `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration") equals the record's `proposedSha256`, which differs from `baseSha256` unless the proposal changed nothing |
| ACC-DEPRECATION-HISTORY | for every `compat` item `deprecated` with `removedIn < N` in the changelog, the listing records `removedIn` at that stage, or the declaration was provisional when deprecated (not listed); an item with `removedIn = N` needs only the declaration's absence (`ACC-DEP-DUE`), because the roll that ends stage N writes its `removedIn` |
| ACC-STATE | the header state is `RELEASE(N, P)` |

## Which gate step runs each

| Tests | Run by | When |
|---|---|---|
| the C++ tests | `Test` (`ZenithCore_Tests`) and, in their presets, `Sanitizers -Preset win-asan` and `-Preset linux-tsan` | every diff the categories select; `ZenithCore` is in `Zenith2Memory`'s paths |
| every `Pwsh_Maint_*` case | `Test` in the `win` preset | every Z2 diff |
| the acceptance checks | the human, as step A4 of the stage acceptance (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps"), and the roll's precondition | once per stage |
