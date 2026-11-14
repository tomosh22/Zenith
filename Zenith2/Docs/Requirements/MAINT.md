# Requirements — MAINT Engine maintenance

## Purpose

This catalogue breaks the MAINT inventory items (`Zenith2/Docs/Requirements/Inventory.md` → "MAINT Engine maintenance") into testable requirements `Z2-R-MAINT-NNN`. Each names the inventory items it covers, the stage whose acceptance it serves (its fix version; a ticket may be scheduled earlier by the pull-forward rule), its classification, its governing decisions and budget rows, the artefact that proves it, and the headings of `Zenith2/Docs/Design/MAINT.md` and its companion files that specify it; the design's "Requirement index" is the reverse map. MAINT owns every requirement here. Where the work edits a protected file (the libgraph baseline, the complexity profile, the category `reviewPaths`) it is carried by a PROC `needs-human` ticket, and where only a human can perform it (a release, a roll, a rehearsal, a calibration) by a `needs-human` ticket; that is a property of the ticket, not of ownership.

## Requirements

### Z2-R-MAINT-001 Supported surfaces are a closed list

- **Statement:** The supported API is exactly the fourteen surfaces S1 to S14 of the supported-surfaces table, each with its consumers, owner, version identity and window; a surface absent from the table carries no compatibility window.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the table in the design; reviewed at the Stage 2 acceptance against `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind"
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Supported surfaces"

### Z2-R-MAINT-002 No C++ game API, plugin ABI or mod API

- **Statement:** Zenith2 offers no C++ API for games, no binary plugin ABI and no third-party mod API; games reach the engine through Luau, data formats and tool command lines alone.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-024, Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** the unsupported-surfaces table; `SourceLayout` refuses compiled sources under `Games/` ([Z2-N-ARCH-205])
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Supported surfaces"

### Z2-R-MAINT-003 One stability class per contract block

- **Statement:** Every contract block of a public header has exactly one stability class (stable, experimental or internal), decided by its Version clauses and the internal name rules N1 to N4.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Live_Classification`, `Pwsh_Maint_Classification_TwoClasses` in the Test step
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"

### Z2-R-MAINT-004 Shipped status is computed from the effective since and MAJOR

- **Statement:** A stable block is shipped exactly when its effective since stage, `max(since, 2)`, is below the engine MAJOR, and provisional otherwise; a `since` stage below 2 counts as 2, and the status is computed and never written.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_ProvisionalNotListed`, `Pwsh_Maint_Surface_Unlisted`, `Pwsh_Maint_Classification_SinceBeforeFirstVersion`, `Pwsh_Maint_Surface_EarlySinceListedAtFirstRoll`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"

### Z2-R-MAINT-005 Experimental and internal declarations change with callers migrated

- **Statement:** An experimental or internal declaration may change or be removed in any diff that migrates every in-repo caller in the same diff, with no deprecation.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the Build and Test steps of the changing diff, with `Sanitizers -Preset linux-tsan` when an `Internal/Linux/` file includes the changed header; review for semantic callers
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"; Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-006 Experimental clause names a graduation stage and consumers

- **Statement:** An experimental block carries `experimental until Z2 Stage <G> (consumers <targets>)` with since ≤ G ≤ 7 and at least one existing consumer target.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_MalformedExperimental`, `_GraduatesBeforeSince`, `_UnknownConsumer`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"

### Z2-R-MAINT-007 Experimental headers are homogeneous

- **Statement:** A header that holds an experimental block holds no stable or internal block.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_MixedHeader`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"

### Z2-R-MAINT-008 Experimental consumers are enforced

- **Statement:** An experimental header is included only from sources of its library, its test targets and the consumers its clause lists.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_ExperimentalIncludedOutside`, `_ExperimentalIncludedByTests`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"

### Z2-R-MAINT-009 Experimental blocks expire

- **Statement:** An experimental block whose graduation stage G is below the engine MAJOR fails the check, and the roll that ends stage G refuses while one remains.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_Expired`, `Pwsh_Maint_Roll_RefusesExpired`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation lifecycle"

### Z2-R-MAINT-010 Internal name rules

- **Statement:** Test-access headers, tool-access headers, backend factory headers and module entry headers are internal by rule N1 to N4 without a clause, and an internal clause on them is reported as redundant; generated public headers are not internal and keep the class of their generator's contract blocks.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_RedundantInternal`, `_InternalIncludedByRuntime`, `_GeneratedHeaderNotInternal`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"

### Z2-R-MAINT-011 Internal headers have restricted consumers

- **Statement:** An internal header is included only from the composition root, tools, tests, benchmarks, integration suites and its own library.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_InternalIncludedByModule`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"

### Z2-R-MAINT-012 Shipped stable blocks are never reclassified

- **Statement:** A shipped stable block never gains an experimental or internal clause or an internal name; leaving the stable surface goes through deprecation.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_ShippedMadeExperimental`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"; Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-013 Existing Version forms stay valid

- **Statement:** The clause grammar reads the text after `since Z2 Stage <S>` up to the next `; ` as free text, and every segment that does not begin with `experimental` or `deprecated` and is not `internal` as free text, so every existing Version form of the designs is valid unchanged.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Classification_ExistingFreeForms`, `Pwsh_Maint_Live_Classification`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"

### Z2-R-MAINT-014 Luau bindings carry stability, since and deprecation

- **Statement:** Every binding descriptor records its introduction stage, its stability and its deprecation, and all three are inputs of the binding-set hash.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** SCRIPT's `Test_Script_Bindings_VersionValidatedAtSeal` and `Test_Script_Bindings_ListingWritten` (row SC30) and `Pwsh_Maint_Live_Surface` over `bindings.json`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Luau API classification"

### Z2-R-MAINT-015 Deprecated Luau calls are diagnosed

- **Statement:** A call from Luau to a deprecated binding produces one load-time diagnostic per module and line naming the replacement and the removal stage, as a warning and never a Luau error.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Script_Binding_DeprecatedCallDiagnosed` (SCRIPT, rows SH14 and SC30)
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Luau API classification"

### Z2-R-MAINT-016 The Luau surface listing is the union over repository games

- **Statement:** The `luau` section of the API surface listing is the union of the shipped stable functions of every repository game's `bindings.json`.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_Valid` extended by T10 with a two-game fixture
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Luau API classification"; Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-017 Compatibility windows per surface

- **Statement:** Every change to a supported surface follows its row of the compatibility-window table: what is allowed when, what the same diff must contain, and what follows.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the window table; the review of every diff touching a reviewPaths file
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-018 Configuration keys are never renamed

- **Statement:** A shipped configuration key is never renamed; it is retired after one fix version whose schema description begins `deprecated Z2 Stage <D>, use <key>`, and a retired key degrades to the unknown-key warning.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** review of the registering diff; PLAT's `Test_Config_UnknownKeysWarnedWithSource`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-019 Player and tool options keep a deprecation window

- **Statement:** A shipped Player or tool option that changes keeps working for one fix version and prints `warning: <option> is deprecated since Z2 Stage <D>; use <replacement>; removed in Z2 Stage <D+1>` once per run, then is removed.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the owning tool's tests of the warning line; review
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-020 Engine version header constants

- **Statement:** `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h` declares `Zenith_EngineVersion`, `Zenith_EngineVersionError`, the six version constants and the five functions with the signatures of the design.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-034, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineVersion_ConstantsAgree`; `ApiContractCheck` and `HeaderSelfContained` over the header
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-021 Engine version text parses

- **Statement:** `Zenith_ParseEngineVersion` accepts exactly the text grammar `major.minor.patch[-dev.K]` with components ≤ 65535 and K ≤ 65534.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineVersion_ParseRelease`, `_ParseDevelopment`, `_ParseLimits`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-022 Malformed version text is rejected without asserting

- **Statement:** Every text outside the grammar returns `ENGINE_VERSION_ERROR_MALFORMED`, `COMPONENT_RANGE` or `DEV_COUNTER_RANGE` and never reaches an assert.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineVersion_ParseMalformed_ReturnsError`, run under win-asan and linux-tsan
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-023 Version formatting into a caller buffer

- **Statement:** `Zenith_FormatEngineVersion` writes the text and a NUL and returns the length, and with too small a destination writes nothing and returns `ENGINE_VERSION_ERROR_BUFFER_TOO_SMALL`.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineVersion_FormatRoundTrips`, `_FormatIntoSmallBuffer_ReturnsError`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-024 Ordering key equals precedence

- **Statement:** The ordering key and `Zenith_CompareEngineVersions` order versions exactly as semantic-version precedence over the grammar, a pre-release before its release.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineVersion_KeyOrdersByPrecedence`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-025 MAJOR is the stage and MINOR is zero

- **Statement:** The engine MAJOR is the stage of the fix version under development or just released, and MINOR is 0 throughout Stages 0 to 7.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Version_NonZeroMinor`, `Pwsh_Maint_Live_Changelog` (`CHG-STAGE`)
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version numbers"; Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-026 Dev counter counts compatibility events

- **Statement:** During development the version carries `-dev.K`, and K grows by exactly one for each `compat` changelog entry.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_SkippedCounter`, `Pwsh_Maint_Changelog_Valid`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version numbers"; Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-027 Compatibility events are listed and logged

- **Statement:** Each change of the compatibility-event table appends one `compat` entry listing every item of the diff and rewrites the header in the same diff.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Live_Changelog`, `Pwsh_Maint_Changelog_CompatWithoutItems`; review for the kinds no check detects
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine changelog"

### Z2-R-MAINT-028 Engine changelog schema

- **Statement:** `engine_changelog.json` obeys the changelog schema, its member rules and the item vocabulary; an entry's `ticket` is a Draft-ID or a board key.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_BadSchema`, `_BadDraftId`, `_BoardKeyValid`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine changelog"

### Z2-R-MAINT-029 Changelog transitions follow the version state machine

- **Statement:** The changelog's entries form a path of the version state machine from its first roll, each producing exactly its next version.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_FirstNotRoll`, `_CompatInRelease`, `_RollFromDev`, `_StageMismatch`, `_ReleasedMismatch`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-030 Header and changelog agree

- **Statement:** The last changelog entry's version equals `szZENITH_ENGINE_VERSION`, and the numeric constants equal its components.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_HeaderBehind`, `Pwsh_Maint_Version_TextDisagrees`, `Pwsh_Maint_Live_Version`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine changelog"

### Z2-R-MAINT-031 Release step at the start of each acceptance

- **Statement:** The first step of each stage acceptance from Stage 2 sets the engine version to `N.0.0` and appends the `release` entry, refusing a dirty tree, a wrong stage or failing checks; at Stage 7 it also lists the Stage 7 declarations.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Release_SetsReleaseVersion`, `_RefusesDirtyTree`, `_RefusesWrongStage`, `_Stage7ListsSurface`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-032 Release fixes bump PATCH

- **Statement:** Every diff that lands while the engine version has no pre-release suffix and changes a file under `Zenith2/Engine/`, `Player/`, `Shaders/`, `CMake/`, `ThirdParty/` or `Games/`, or a non-test source or script under `Zenith2/Tools/`, increments PATCH and appends a `release-fix` entry; the acceptance records under `Zenith2/Docs/`, the MAINT data files, the two gate data files and the diffs carrying the `release` and `roll` entries are exempt.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_Valid` (a release-fix entry); review of acceptance-time fixes
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"; Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-033 Roll at the end of each acceptance

- **Statement:** The last step of each stage acceptance from Stage 2 to Stage 6 rebuilds the API surface listing and sets the version to `<N+1>.0.0-dev.0`.
- **Covers:** Z2-I-MAINT-02, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_Valid`, `_InterruptedRestores`, `_AddsMissingRehearsalRows`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-034 No roll after Stage 7

- **Statement:** The Stage 7 acceptance runs the release step and no roll; the roll script refuses from `RELEASE(7, P)`.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S7
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_RefusesStage7`, `Pwsh_Maint_Changelog_RollAfterStage7`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-035 Engine version shown at startup

- **Statement:** The startup composition dump and the first log record of every run name `szZENITH_ENGINE_VERSION`.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the MOD ticket of cross-area note MT5 and its startup-report test
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-036 Engine version key offered for bytecode

- **Statement:** MAINT offers `Zenith_GetEngineVersionKey(Zenith_GetEngineVersion())` as the producer of the `.zlbc` engine version hash, which SCRIPT adopts or declines in its own design.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** SCRIPT's `Test_Script_Cook_EngineVersionHashProduced` (row SC30)
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Engine version"

### Z2-R-MAINT-037 Cooked data re-cooks on the cook version

- **Statement:** For cooked data the engine version of the compatibility policy is `uZENITH_ENGINE_COOK_VERSION`; a compatibility event that leaves cooked bytes unchanged triggers no re-cook.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** cross-document row X385 applied to Contracts; ASSET's cooked-version refusal tests
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-038 Version identity per surface

- **Statement:** Each supported surface names its version identity (the engine version, a schema or format version, a hash) in the supported-surfaces table, or states that it has none, and the formats whose identity is an integer change it by one per step.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** the table; review
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Supported surfaces"

### Z2-R-MAINT-039 Deprecation attribute form

- **Statement:** Every deprecation attribute in Zenith2 code reads exactly `[[deprecated("use <R>; removed in Z2 Stage <N>")]]`.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_BadMessage`, `Pwsh_Maint_Live_Deprecations`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-040 Attribute and contract text are paired

- **Statement:** Every deprecation attribute is matched by a deprecated clause in its block or a `@deviation Version: deprecated` line, with the same replacement and removal stage, and every such contract text has its attributes.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_AttributeWithoutClause`, `_ClauseWithoutAttribute`, `_FunctionsBlockPartial`, `_ReplacementMismatch`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-041 Deprecation stage is the landing MAJOR

- **Statement:** A deprecation states D equal to the engine MAJOR when it lands, D ≥ `min(effective since, MAJOR)`, and the removal stage D + 1, so a pulled-forward block can always be deprecated.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_FutureStage`, `_RemovalNotNext`, `_BeforeSince`, `_PulledForwardBlock`, `Pwsh_Maint_Surface_BackdatedDeprecation`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-042 The deprecating diff

- **Statement:** A deprecating diff adds the replacement and migrates every in-repo caller in the same diff.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** the Build step (warnings as errors fail any unmigrated caller; `CompileFail_Ship_DeprecatedUseIsError`), with `Sanitizers -Preset linux-tsan` when an `Internal/Linux/` file includes the changed header; review for semantic callers
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation lifecycle"

### Z2-R-MAINT-043 Replacement is declared

- **Statement:** The replacement a deprecation names occurs in a declaration context of some public header or Internal source other than the deprecated declaration's own line.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_ReplacementMissing`, `_ReplacementOnlySelf`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-044 Shipped deprecations are logged

- **Statement:** A deprecation of a shipped block made in the current stage appears as a `cpp` `deprecated` item of a `compat` entry of the current MAJOR.
- **Covers:** Z2-I-MAINT-02, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_ShippedNotLogged`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-045 No overdue deprecation

- **Statement:** No declaration carries a deprecation whose removal stage is below the engine MAJOR, and the roll refuses while one is due.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_Overdue`, `_RemovalDueStillPresent`, `Pwsh_Maint_Roll_RefusesOverdue`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-046 Removal tickets come from the roll

- **Statement:** The roll writes one removal item per declaration deprecated in the stage it closes, naming the owning area and epic, and the human files one maintenance TASK per item in fix version D + 1.
- **Covers:** Z2-I-MAINT-04, Z2-I-MAINT-05
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_Valid` (the plan's removals); the acceptance ticket's filed TASKs
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-047 The removal diff

- **Statement:** A removal lands in stage D + 1, deletes the declaration, its markers and every reference, appends a `removed` compat item and leaves the listing to the next roll.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_PermittedRemoval`; the Build step
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation lifecycle"

### Z2-R-MAINT-048 Member-level deprecation

- **Statement:** A member, single function or enumerator is deprecated through a `@deviation Version: deprecated` line directly above it and its attribute; member removal is confirmed by review and the build of every caller.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_MemberDeviation`; `ApiContractCheck` fixtures for a deviation line above a data member and an enumerator (PROC, cross-area note MT3)
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Version field clauses"; Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-049 Deprecation history at Stage 6

- **Statement:** At each acceptance, and binding for the programme at Stage 6, every deprecation logged with a removal stage below the accepted stage is recorded removed in the listing, and one due in the accepted stage is absent from the tree.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** acceptance checks `ACC-DEPRECATION-HISTORY` and `ACC-DEP-DUE` in the Stage 6 acceptance record; `Pwsh_Maint_Acceptance_DeprecationHistory_Fails`, `_DeprecationHistory_DueThisStagePasses`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Acceptance checks"

### Z2-R-MAINT-050 Luau bindings are removed after one fix version

- **Statement:** A shipped stable Luau binding is removed only in the fix version after its deprecation, under the same listing rules as C++ blocks.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** the Luau `SUR` and `DEP` rules of T10 at the Stage 6 acceptance
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Luau API classification"

### Z2-R-MAINT-051 API surface listing schema

- **Statement:** `api_surface.json` lists every shipped stable C++ block and Luau binding with since, deprecation and removal stages, byte-ordered, under the listing schema.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_Valid`, `_SchemaBad`, `Pwsh_Maint_Live_Surface`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-052 Only the roll and the Stage 7 release write the listing

- **Statement:** Other than its first creation by T4, the API surface listing is rewritten by the roll and the Stage 7 release step alone, and every other edit of it is reviewed through reviewPaths.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** the roll cases; cross-document row X386
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-053 Shipped removal needs a shipped deprecation

- **Statement:** A listed shipped block, identified by library and name, disappears only when its record shows a deprecation shipped in an earlier stage.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_ShippedRemovedWithoutDeprecation`, `_PermittedRemoval`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-054 No unlisted shipped block

- **Statement:** Every stable block with a since stage at or below the listing's `shippedThrough` has a record.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_Unlisted`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-055 Since and class drift is refused

- **Statement:** A record whose block's since stage changed, or whose block became experimental or internal, fails the check.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_SinceEdited`, `_ShippedMadeExperimental`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-056 Back-dated deprecations are refused

- **Statement:** A deprecation of a shipped block whose stage disagrees with the listing's record, or with the current MAJOR for an unrecorded one, fails the check.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_BackdatedDeprecation`, `_DeprecationChanged`, `_Undeprecated`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-057 Every area has its epic

- **Statement:** Each of the 41 area codes has one epic `Z2 <CODE> — <Area name>` on the Z2 board.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-044
- **Budgets:** none
- **Acceptance evidence:** `zagent report epic --project Z2` in the Stage 0 acceptance; from Stage 2 `Pwsh_Maint_Ownership_EpicMismatch`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Subsystem ownership"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"

### Z2-R-MAINT-058 Owner responsibilities are routed

- **Statement:** Maintenance tickets, refactors owed, removals, graduations, rehearsals, regressions, exceptions and surface growth are routed to the owning area's epic resolved from their path.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the plan items' `owner` and `epic` in `Pwsh_Maint_Roll_Valid`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Subsystem ownership"

### Z2-R-MAINT-059 Ownership file schema

- **Statement:** `subsystem_owners.json` holds the 41 areas, one row per owned path, the fallback steward and the manifest token owners under its schema.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_Valid`, `_UnknownArea`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership file"

### Z2-R-MAINT-060 Owner resolution by longest prefix

- **Statement:** `Get-Zenith2Owner` resolves any `Zenith2/` path to the longest matching row, or to the fallback with `ViaFallback` set.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_ResolveLongestPrefix`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership resolution"

### Z2-R-MAINT-061 Ownership agrees with Architecture, Inventory and Roadmap

- **Statement:** The ownership file agrees with the Architecture ownership table, the Inventory's area codes and the Roadmap's epic titles.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_ArchitectureDisagrees`, `_EpicMismatch`, `_UnderscoreBackendRows`, `_ThirdPartyCMakeRow`, `_BareSiblingPaths`, `_GlobRow`, `_WorkloadCharactersAnim`, `_MultiOwnerRow`, `_PerSuiteChildOtherOwner`, `_AggregateLeadingCode`; `Pwsh_Maint_Live_Ownership` over the real Architecture table
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"

### Z2-R-MAINT-062 Every target and directory has an owner

- **Statement:** Every declared target and every top-level Zenith2 directory resolves to an owner, and a fallback resolution or a stale row is reported between acceptances.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_TargetOnFallback`, `_DirectoryUnowned`, `_StaleRow`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"

### Z2-R-MAINT-063 Every dependency has an owning area

- **Statement:** Every manifest dependency resolves, through its owner library or token, to an owning area.
- **Covers:** Z2-I-MAINT-05, Z2-I-MAINT-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_DependencyUnowned`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"

### Z2-R-MAINT-064 Baseline re-pinned at each acceptance after review

- **Statement:** Each stage acceptance re-pins `libgraph_baseline.json` from the accepted tree through `Run-Committed.ps1` alone, after the Libgraph step passes without ticket context and the surface growth report, with its disposition, is committed (the re-pin runs on a clean tree).
- **Covers:** Z2-I-MAINT-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the acceptance record with `surface_growth.md`; acceptance check `ACC-SURFACE-REPORT`; `Pwsh_Maint_Acceptance_SurfaceReport_Fails`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Baseline re-pinning"

### Z2-R-MAINT-065 Surface growth report

- **Statement:** Each re-pin commits `Zenith2/Docs/Research/Libgraph/stage-<N>/surface_growth.json` and `.md` with every library's added and removed pairs, owner and consumers, the consumers before the stage read from the previous stage's report (none at the first report).
- **Covers:** Z2-I-MAINT-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_SurfaceReport_FromLibgraphOutputs`, `_FirstReportNoConsumerCriterion`, `Pwsh_Maint_Records_Incomplete`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Surface growth report"

### Z2-R-MAINT-066 Largest growth gets a disposition

- **Statement:** Every library whose inbound surface grew by more than 25 % or 100 pairs since the previous pin, or gained a consumer library, gets an acceptance sentence or a refactor ticket in the report's disposition.
- **Covers:** Z2-I-MAINT-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_SurfaceReport_FromLibgraphOutputs` (selection); `Pwsh_Maint_Live_Reports` (`RES-INCOMPLETE`)
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Baseline re-pinning"

### Z2-R-MAINT-067 Libgraph tool changes keep the baseline meaningful

- **Statement:** A PROC ticket that changes libgraph attaches the surfaces before and after and is followed by a re-pin before any growth it causes is reviewed.
- **Covers:** Z2-I-MAINT-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the PROC ticket's evidence (cross-area note MT3)
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Libgraph tool changes"

### Z2-R-MAINT-068 Stage 0 gate data contents

- **Statement:** The baseline lands at Stage 0 with no library and the profile with the starting thresholds 25, 30, 5, 150 and 1,500 and an empty exception list.
- **Covers:** Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-010's diff and the Stage 0 acceptance
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Stage 0 contents"

### Z2-R-MAINT-069 MAINT loop tickets never edit gate data

- **Statement:** No MAINT loop ticket edits `libgraph_baseline.json` or `complexity_profile.json`; every change to them is a `needs-human` edit carrying MAINT's evidence.
- **Covers:** Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `zagent guard` over every MAINT ticket; [Z2-N-CONTRACT-178]
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Scope of this file"

### Z2-R-MAINT-070 Gate checker inputs are named

- **Statement:** The inputs the Libgraph and Complexity steps read, their producers and who keeps them true are listed.
- **Covers:** Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S0
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the two input tables; reviewed with PROC's structural-gates design
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Libgraph checker inputs"; Zenith2/Docs/Design/MAINT/GateData.md → "Complexity checker inputs"

### Z2-R-MAINT-071 Complexity calibrated from the measured distribution

- **Statement:** From Stage 4, each stage acceptance recalibrates the complexity thresholds from the distribution measured on the accepted tree; Stages 2 and 3 keep the starting thresholds.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the calibration record and its decision entry; acceptance check `ACC-CALIBRATION`, `Pwsh_Maint_Acceptance_Calibration_Fails`, `_CalibrationNotDueBeforeStage4`, `_Calibration_DecisionMissing`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity calibration"

### Z2-R-MAINT-072 Calibration never loosens, keeps headroom and respects floors

- **Statement:** A calibrated threshold is `min(current, max(floor, ceil(observedMax × 1.25)))` with the floors of the design.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Complexity_ProposalNeverLoosens`, `_ProposalHeadroom`, `_ProposalRespectsFloor`, `_ProposalFileLines`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity calibration"

### Z2-R-MAINT-073 Calibration never reds a passing tree and never tightens a failing one

- **Statement:** Every non-exception entity of a tree that passes the current profile passes the calibrated thresholds, and a metric the measured tree already fails keeps its current threshold.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Complexity_PassingTreeStaysGreen`, `_ProposalNeverLoosens`; the PROC apply ticket's Complexity step run
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity calibration"

### Z2-R-MAINT-074 Complexity measurement harness

- **Statement:** `Measure-Zenith2Complexity.ps1` runs the analyzer with the gate's invocation and its entity dump option, computes nearest-rank percentiles and proposals over every measured entity, and writes the record and the proposed profile.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Complexity_PercentilesNearestRank`, `_ProposedProfileMinimalDiff`, `_NoDumpOption`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity measurement harness"

### Z2-R-MAINT-075 Calibration record

- **Statement:** Each calibration commits `Zenith2/Docs/Research/Complexity/stage-<N>/calibration.json` and `calibration.md` with the per-metric distribution, proposals, exception actions, the lower-case SHA-256 of the profile it was generated from and of the proposed profile, and the ruling's decision ID, which the ruling ticket writes.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Live_Reports`, `Pwsh_Maint_Complexity_BaseShaRecorded`, `Pwsh_Maint_Records_WrongDirectory`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Calibration record"

### Z2-R-MAINT-076 Exception admission

- **Statement:** A complexity exception is admitted only with criteria A1 to A4 stated, a limit of at most `ceil(measured × 1.25)`, and a decision entry.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the admitting ruling ticket's DecisionLog entry; PROC's `CX-ExceptionNoDecision` fixture
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity exceptions"

### Z2-R-MAINT-077 Exception audit

- **Statement:** Each calibration removes exceptions whose entity is gone or under threshold and tightens the rest to `ceil(measured × 1.25)` when that is lower.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Complexity_ExceptionAudit`
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity exceptions"

### Z2-R-MAINT-078 Calibration is applied by PROC after MAINT's ruling

- **Statement:** A calibration at the acceptance of each of Stages 4 to 7 is a MAINT `needs-human` ruling ticket drafted by MAINT, followed by a PROC `needs-human` apply ticket drafted by PROC and blocked by it, which refuses when the profile no longer matches the recorded hash; the acceptance refuses a calibration whose proposed profile was not applied.
- **Covers:** Z2-I-MAINT-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the two tickets' link on the board; `Pwsh_Maint_Acceptance_Calibration_ProfileNotApplied`; cross-document row X389
- **Design:** Zenith2/Docs/Design/MAINT/GateData.md → "Complexity calibration"

### Z2-R-MAINT-079 Rehearsal tiers and cadence

- **Statement:** Every registry row belongs to the runtime, tools or toolchain tier, rehearsed every one, two and two stages respectively, and runtime entries within the stage of an affecting security advisory.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_DueComputation`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Dependency-upgrade rehearsals"

### Z2-R-MAINT-080 Measured entries

- **Statement:** The entries marked `measure` are measured with the owning area's budget harness on reference hardware before and after, only when the rehearsal changes the pinned version.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** each measured rehearsal record's `measurements`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Dependency-upgrade rehearsals"

### Z2-R-MAINT-081 Rehearsal registry schema

- **Statement:** `rehearsals.json` holds one row per non-null toolchain key and per manifest entry without `fromToolchain`, with tier, measure flag, owner, pin stage and last rehearsal; a toolchain-sourced entry is rehearsed under its toolchain key's row alone.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_Valid`, `_BadVerdict`, `_FromToolchainRow`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal registry"

### Z2-R-MAINT-082 Rehearsal due stages

- **Statement:** An entry is due at stage `(last stage or pin stage) + cadence` and overdue at an acceptance at or past that stage without a rehearsal in it.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_DueComputation`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal registry"

### Z2-R-MAINT-083 Rehearsal plan from the roll

- **Statement:** The roll lists every rehearsal due by the next stage's acceptance, and the human files one `needs-human` maintenance TASK per item, or one bundle TASK for several entries of one owner that share a build set, under the entry's owner epic.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_Valid` (the plan's rehearsals)
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal plan"

### Z2-R-MAINT-084 Advisory rehearsals

- **Statement:** Each mid-sprint integration check reviews upstream security advisories of runtime entries and files an `advisory` rehearsal for each affected pinned version.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the mid-sprint ticket's evidence (step M2)
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal plan"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Stage acceptance and mid-sprint steps"

### Z2-R-MAINT-085 Rehearsal procedure in a scratch worktree

- **Statement:** A rehearsal follows steps R1 to R10 in a detached scratch worktree and never changes the manifest on master.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** each rehearsal ticket's record and diff, which touches only the record and the registry
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"

### Z2-R-MAINT-086 Measured rehearsals use reference hardware

- **Statement:** A measured rehearsal measures its rows on the reference desktop and the S24 per each row's procedure, as a `needs-human` step.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the record's `measurements` with `hardware` (`reference-desktop` or `s24`) and `backend`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"

### Z2-R-MAINT-087 Rehearsal verdicts lead to SHIP's upgrade

- **Statement:** An `adopt` or `adopt-with-changes` verdict files SHIP's upgrade pair citing the record.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the rehearsal ticket's follow-ups (cross-area note MT4)
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"

### Z2-R-MAINT-088 Rehearsal record

- **Statement:** Each rehearsal commits `Rehearsal.json` and `Rehearsal.md` under `Zenith2/Docs/Research/Upgrades/<name>/<from>-to-<to>/` with builds, suites, measurements, adaptations and verdict, and no test count.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_RecordScaffold`, `Pwsh_Maint_Live_Reports`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal record"

### Z2-R-MAINT-089 Overdue rehearsals fail the acceptance

- **Statement:** A stage acceptance fails while any registry row is overdue at that stage.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** acceptance check `ACC-REHEARSALS`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal registry"

### Z2-R-MAINT-090 Every entry rehearsed before qualification

- **Statement:** At the Stage 6 acceptance every registry row was rehearsed in Stage 5 or Stage 6.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_PreQualification`; `ACC-REHEARSALS` at Stage 6
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal registry"

### Z2-R-MAINT-091 Sample games file

- **Statement:** `sample_games.json` lists every repository game with owner, role, first build stage and platforms, and the pending modules and backends.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_SampleGames_Valid`, `_SchemaUnordered`, `_OwnerDiffers`, `_Unlisted`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Sample games file"

### Z2-R-MAINT-092 Due games exist with their platforms and a gametest

- **Statement:** A game whose first build stage is below MAJOR has its `game.json` and `CMakeLists.txt`, its listed platforms and at least one gametest.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_SampleGames_DueGameMissing`, `_NotYetDue`, `_PlatformsDiffer`, `_NoGametest`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Sample game checks"

### Z2-R-MAINT-093 Every module and backend is selected by a game

- **Statement:** Every module library and every ECS and physics backend is selected by at least one repository game, or is pending with a stage not yet passed.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-039, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_SampleGames_ModuleUnselected`, `_ModulePending`, `_BackendUnselected`, `_PendingStale`; `ACC-PENDING`, `Pwsh_Maint_Acceptance_Pending_Fails`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Sample game checks"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Sample games"

### Z2-R-MAINT-094 Build matrix

- **Statement:** Every repository game builds on the build presets of the build-matrix table, through the gate steps, the acceptance or device runs it names.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the Build steps of every diff ([Z2-N-ARCH-029]); the acceptance record
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Build matrix"

### Z2-R-MAINT-095 A breaking change fixes the game in the same diff

- **Statement:** A change that breaks a repository game's build or boot fixes the game in the same diff and never removes it from the build.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the Build and Test steps of that diff (`Game_<Name>_<Stem>`)
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Build matrix"

### Z2-R-MAINT-096 Workload and flagship games are due by Stages 3 and 4

- **Statement:** The five workload games are due from the Stage 3 acceptance and the three flagship slices from the Stage 4 acceptance.
- **Covers:** Z2-I-MAINT-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** acceptance check `ACC-SAMPLE-GAMES` at Stages 3 and 4; `Pwsh_Maint_Acceptance_SampleGames_Fails`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Sample games file"

### Z2-R-MAINT-097 Gate headroom triage

- **Statement:** A warm phase above 80 % of its cap on three consecutive tickets gets a `needs-human` MAINT triage TASK whose triage note files one follow-up per cause.
- **Covers:** Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-002, Z2-B-PROC-003
- **Acceptance evidence:** the triage note under `Zenith2/Docs/Research/GateHeadroom/`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Gate headroom triage"

### Z2-R-MAINT-098 MAINT acceptance steps

- **Statement:** Each stage acceptance from Stage 2 runs MAINT's release, surface growth review and re-pin, calibration (from Stage 4), acceptance checks and roll, in that order.
- **Covers:** Z2-I-MAINT-02, Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the acceptance ticket's evidence
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Stage acceptance and mid-sprint steps"

### Z2-R-MAINT-099 MAINT mid-sprint steps

- **Statement:** Each mid-sprint integration check attaches the maintenance status report, reviews advisories and due work, and runs the ownership audit.
- **Covers:** Z2-I-MAINT-03, Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the mid-sprint ticket's evidence
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Stage acceptance and mid-sprint steps"

### Z2-R-MAINT-100 Maintenance status report

- **Statement:** `Test-Zenith2Maintenance.ps1 -Report` writes `status.json` and `status.md` with the seven sections of the design.
- **Covers:** Z2-I-MAINT-02, Z2-I-MAINT-03, Z2-I-MAINT-04, Z2-I-MAINT-05, Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Status_ReportSections`
- **Design:** Zenith2/Docs/Design/MAINT.md → "Diagnostics and profiling"

### Z2-R-MAINT-101 Check reports and exit codes

- **Statement:** Every check writes its `zenith2.maintenance-check` report, prints its findings and verdict line, and exits with the six exit codes of the design.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_CheckReport_Shape`; every `Pwsh_Maint_*` case asserts the exit code
- **Design:** Zenith2/Docs/Design/MAINT.md → "Public API"; Zenith2/Docs/Design/MAINT.md → "Diagnostics and profiling"

### Z2-R-MAINT-102 Missing and malformed inputs

- **Statement:** A missing or malformed data file exits 3 naming it, an absent `declared_deps.json` skips the rules that need it with a report finding, and nothing is written.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_BadSchema`, `Pwsh_Maint_Input_MissingFile_Exit3`, `_Bom_Exit3`, `_NewerSchema_Exit3`, `_HeaderNotUtf8`, `Pwsh_Maint_Ownership_NoConfigure`, `Pwsh_Maint_Usage_PathOutsideZenith2_Exit2`, `Pwsh_Maint_Git_Missing_Exit5`
- **Design:** Zenith2/Docs/Design/MAINT.md → "Failure handling"

### Z2-R-MAINT-103 Release and roll are atomic

- **Statement:** The release and roll write every file through a temporary file and rename, and restore every target on any failure.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_InterruptedRestores`, `Pwsh_Maint_Release_RefusesDirtyTree`
- **Design:** Zenith2/Docs/Design/MAINT.md → "Failure handling"; Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-104 No Luau error and no misuse block

- **Statement:** MAINT raises no Luau error, defines no misuse code and therefore reserves no block of the misuse catalogue's code ranges.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** the misuse catalogue's code ranges list no MAINT block (row X388 withdrawn); review of every MAINT diff
- **Design:** Zenith2/Docs/Design/MAINT.md → "Threading lifetime and error contract"

### Z2-R-MAINT-105 MAINT checks run in the Test step

- **Statement:** Every MAINT PowerShell case is registered through `zenith_register_pwsh_test` with labels `z2-pwsh` and `maint` and runs in the Test step in the win preset.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-02, Z2-I-MAINT-04, Z2-I-MAINT-05, Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** `ctest --show-only=json-v1` of the win tree lists every `Pwsh_Maint_*` name
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Test registration"

### Z2-R-MAINT-106 One fixture per rule

- **Statement:** Every rule ID of the MAINT check families and every acceptance check has at least one fixture case that triggers it.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-02, Z2-I-MAINT-04, Z2-I-MAINT-05, Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the fixture-case table; the `Pwsh_Maint_Acceptance_<Id>_Fails` cases
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "PowerShell check tests"

### Z2-R-MAINT-107 MAINT data files are reviewed

- **Statement:** The five MAINT data files are reviewPaths files, so every edit of them needs an independent review.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** cross-document row X386 and the PROC edit of the category reviewPaths
- **Design:** Zenith2/Docs/Design/MAINT.md → "Authoring and cooking"

### Z2-R-MAINT-108 MAINT acceptance checks

- **Statement:** The acceptance checks pass at each stage acceptance from Stage 2, after the surface growth review, the re-pin and the calibration and before the roll.
- **Covers:** Z2-I-MAINT-03, Z2-I-MAINT-04, Z2-I-MAINT-05, Z2-I-MAINT-06, Z2-I-MAINT-07, Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Test-Zenith2Maintenance.ps1 -Acceptance -Stage <N>` in the acceptance record; `Pwsh_Maint_Acceptance_State_Fails`, `_DepDue_Fails`, `_ExperimentalDue_Fails`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Acceptance checks"

### Z2-R-MAINT-109 Provisional declarations follow the deprecation window pending Q97

- **Statement:** Until Q97 (flagged proposal MAINT-1) is ruled, an incompatible change or removal of a provisional declaration goes through the deprecation lifecycle as [Z2-N-CONTRACT-523] reads; if the narrowing is accepted, such a diff migrates every in-repo caller and lists every drafted, unexecuted ticket that cites the declaration for revalidation.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** review-confirmed, not mechanical: the reviewer checks the changing diff against the window table and, under the narrowing, that its commit message body lists the drafted tickets the stated full-body search finds; `--mark-stale` reaches the tickets citing the edited design section, and the rest need the requested `--mark-stale --keys` option
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Provisional changes pending Q97"; Zenith2/Docs/Design/MAINT/Versioning.md → "Compatibility windows"

### Z2-R-MAINT-110 The roll records the released version

- **Statement:** The roll appends a `roll` entry whose `released` names the released version and the commit it was released at.
- **Covers:** Z2-I-MAINT-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Roll_Valid`, `Pwsh_Maint_Changelog_ReleasedMismatch`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release and roll procedures"

### Z2-R-MAINT-111 The deprecating diff moves the body and the tests

- **Statement:** A deprecating diff rewrites the deprecated body over the replacement and moves the deprecated declaration's tests to the replacement.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** review of the deprecating diff; the Test step
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation lifecycle"

### Z2-R-MAINT-112 The deprecating diff logs and documents

- **Statement:** A deprecating diff appends its changelog item and edits the owning design section in the same diff.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_ShippedNotLogged`; review
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation lifecycle"

### Z2-R-MAINT-113 Fallback ownership is refused at an acceptance

- **Statement:** At a stage acceptance no target or directory resolves through a fallback and no ownership row is stale.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** acceptance check `ACC-OWNERSHIP`; `Pwsh_Maint_Acceptance_Ownership_Fails`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Acceptance checks"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"

### Z2-R-MAINT-114 Deferred and rejected verdicts

- **Statement:** A `defer` verdict sets a revisit stage that the registry row records and that brings the row's due stage forward when it is earlier than the cadence, and a `reject` verdict that blocks a security fix raises a ruling ticket.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_RevisitStageLowersDue`; review of the rehearsal ticket's follow-ups
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"

### Z2-R-MAINT-115 Root files are owned

- **Statement:** Every tracked file directly under `Zenith2/` has a file row in the ownership file.
- **Covers:** Z2-I-MAINT-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Ownership_RootFileUnowned`, `Pwsh_Maint_Live_Ownership`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership checks"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Ownership file"

### Z2-R-MAINT-116 Fixtures are never sources

- **Statement:** No tracked file under `Zenith2/Tools/Maintenance/` ends in `.h`, `.hpp`, `.inl`, `.cpp`, `.c` or `_CompileFail.cpp.in`; header and source fixtures carry the `.fixture` suffix, which the runner strips when it copies a fixture tree.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Live_FixturesNotSources`, `Pwsh_Maint_Fixtures_SourceNameRefused`, `_CompileFailNameRefused`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Fixture layout"

### Z2-R-MAINT-117 Every implemented case is registered

- **Statement:** Every case `Maintenance.Tests.ps1 -List` prints is registered in the win tree, and every registered `Pwsh_Maint_*` name is implemented.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-02, Z2-I-MAINT-04, Z2-I-MAINT-05, Z2-I-MAINT-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** `Pwsh_Maint_Registry_AllCasesRegistered`, `_UnregisteredCase`, `_UnknownRegistration`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Test registration"

### Z2-R-MAINT-118 Research records are complete and in place

- **Statement:** Every committed surface growth, calibration and rehearsal record parses with its schema, sits in the directory its schema names, has its Markdown twin, and holds no operator placeholder.
- **Covers:** Z2-I-MAINT-03, Z2-I-MAINT-06, Z2-I-MAINT-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Records_Incomplete`, `_WrongDirectory`, `Pwsh_Maint_Live_Reports`
- **Design:** Zenith2/Docs/Design/MAINT/Tests.md → "Research record rules"

### Z2-R-MAINT-119 No-newer rehearsals discharge a due row

- **Statement:** A rehearsal whose upstream has no release newer than the pin records the verdict `no-newer` with the upstream source, the newest upstream version seen and the check time, no build or measurement, and a `fromVersion` equal to the pin; the row counts as rehearsed in that stage. The upstream check itself is human evidence and is not computed.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_NoNewerSatisfies`, `_NoNewerShapeBad`, `_NoNewerPinDiffers` (`REH-NONEWER`)
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"; Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal registry"

### Z2-R-MAINT-120 Rehearsal builds are limited to reaching presets

- **Statement:** A rehearsal builds and tests only the presets that reach the entry's platforms and usage.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** the record's `builds`, with `skipped` for excluded presets
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal procedure"

### Z2-R-MAINT-121 Overdue rehearsal rows are a test failure at acceptance

- **Statement:** At the acceptance of stage S a registry row with `dueBy ≤ S` that was not rehearsed in stage S fails `REH-OVERDUE`, a duplicate, `fromToolchain` or stale row fails `REH-ROWS`, and a missing row is only reported (`REH-ROWS-MISSING`), because the roll adds it.
- **Covers:** Z2-I-MAINT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Rehearsals_Overdue`, `_MissingRowReported`, `Pwsh_Maint_Acceptance_Rehearsals_Fails`, `_MissingRowDoesNotBlockRoll`
- **Design:** Zenith2/Docs/Design/MAINT/Upkeep.md → "Rehearsal rules"

### Z2-R-MAINT-122 Stage 7 declarations are listed at the Stage 7 release

- **Statement:** The Stage 7 release step adds every stable Stage 7 block to the API surface listing with `shippedThrough` 7 and the listing's `engineVersion` `7.0.0`, without changing the version.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S7
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Release_Stage7ListsSurface`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Release step"; Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecations at Stage 7"

### Z2-R-MAINT-123 Changes after the Stage 7 release follow the deprecation window

- **Statement:** After `7.0.0` a compatibility change appends a `compat` entry that advances PATCH alone, and a deprecation at MAJOR 7 names removal stage 8.
- **Covers:** Z2-I-MAINT-02, Z2-I-MAINT-04
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Changelog_CompatAfterStage7Release`, `Pwsh_Maint_Deprecation_Stage7RemovalEight`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecations at Stage 7"; Zenith2/Docs/Design/MAINT/Versioning.md → "Version state machine"

### Z2-R-MAINT-124 Deprecation scan set

- **Statement:** The deprecation checks scan tracked `.h`, `.hpp`, `.inl` and `.cpp` files under `Zenith2/Engine/`, `Player/` and `Tools/` except the two test fixture trees, read the root headers for pairing and stages, refuse the attribute elsewhere, and ignore compile-fail sources.
- **Covers:** Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Deprecation_InInternal`, `_CompileFailFixtureIgnored`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Deprecation checks"

### Z2-R-MAINT-125 Header moves and name reuse are reported

- **Statement:** A listed block found in another root header of its library keeps its record and is reported as moved, and a removed name declared again is reported.
- **Covers:** Z2-I-MAINT-01, Z2-I-MAINT-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_HeaderMoved`, `_NameReused`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"

### Z2-R-MAINT-126 The Luau listing waits for bindings

- **Statement:** A roll that finds no `bindings.json` for some repository game leaves the Luau section unchanged and prints a report finding.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Maint_Surface_LuauNotAvailable`
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "API surface listing"; Zenith2/Docs/Design/MAINT/Versioning.md → "Roll step"

### Z2-R-MAINT-127 Existing stable blocks are not re-classed pending Q97

- **Statement:** Until Q97 is ruled, an experimental or internal clause is given only to a declaration the diff introduces; giving one to a stable block present in an earlier commit is an incompatible change of a stable declaration and follows the deprecation lifecycle.
- **Covers:** Z2-I-MAINT-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** review of every diff that adds a class clause (no tree check sees a block's history)
- **Design:** Zenith2/Docs/Design/MAINT/Versioning.md → "Stability classes"; Zenith2/Docs/Design/MAINT/Versioning.md → "Provisional changes pending Q97"

## Coverage of inventory items

| Item | Classification | Requirements |
|---|---|---|
| Z2-I-MAINT-01 | required | Z2-R-MAINT-001, Z2-R-MAINT-002, Z2-R-MAINT-003, Z2-R-MAINT-004, Z2-R-MAINT-005, Z2-R-MAINT-006, Z2-R-MAINT-007, Z2-R-MAINT-008, Z2-R-MAINT-009, Z2-R-MAINT-010, Z2-R-MAINT-011, Z2-R-MAINT-012, Z2-R-MAINT-013, Z2-R-MAINT-014, Z2-R-MAINT-015, Z2-R-MAINT-016, Z2-R-MAINT-017, Z2-R-MAINT-018, Z2-R-MAINT-019, Z2-R-MAINT-039, Z2-R-MAINT-040, Z2-R-MAINT-051, Z2-R-MAINT-052, Z2-R-MAINT-054, Z2-R-MAINT-055, Z2-R-MAINT-101, Z2-R-MAINT-102, Z2-R-MAINT-104, Z2-R-MAINT-105, Z2-R-MAINT-106, Z2-R-MAINT-107, Z2-R-MAINT-109, Z2-R-MAINT-116, Z2-R-MAINT-117, Z2-R-MAINT-122, Z2-R-MAINT-125, Z2-R-MAINT-126, Z2-R-MAINT-127 |
| Z2-I-MAINT-02 | required | Z2-R-MAINT-017, Z2-R-MAINT-020, Z2-R-MAINT-021, Z2-R-MAINT-022, Z2-R-MAINT-023, Z2-R-MAINT-024, Z2-R-MAINT-025, Z2-R-MAINT-026, Z2-R-MAINT-027, Z2-R-MAINT-028, Z2-R-MAINT-029, Z2-R-MAINT-030, Z2-R-MAINT-031, Z2-R-MAINT-032, Z2-R-MAINT-033, Z2-R-MAINT-034, Z2-R-MAINT-035, Z2-R-MAINT-036, Z2-R-MAINT-037, Z2-R-MAINT-038, Z2-R-MAINT-044, Z2-R-MAINT-098, Z2-R-MAINT-100, Z2-R-MAINT-101, Z2-R-MAINT-103, Z2-R-MAINT-105, Z2-R-MAINT-106, Z2-R-MAINT-110, Z2-R-MAINT-117, Z2-R-MAINT-123 |
| Z2-I-MAINT-03 | required | Z2-R-MAINT-063, Z2-R-MAINT-079, Z2-R-MAINT-080, Z2-R-MAINT-081, Z2-R-MAINT-082, Z2-R-MAINT-083, Z2-R-MAINT-084, Z2-R-MAINT-085, Z2-R-MAINT-086, Z2-R-MAINT-087, Z2-R-MAINT-088, Z2-R-MAINT-089, Z2-R-MAINT-090, Z2-R-MAINT-099, Z2-R-MAINT-100, Z2-R-MAINT-108, Z2-R-MAINT-114, Z2-R-MAINT-118, Z2-R-MAINT-119, Z2-R-MAINT-120, Z2-R-MAINT-121 |
| Z2-I-MAINT-04 | required | Z2-R-MAINT-033, Z2-R-MAINT-039, Z2-R-MAINT-040, Z2-R-MAINT-041, Z2-R-MAINT-042, Z2-R-MAINT-043, Z2-R-MAINT-044, Z2-R-MAINT-045, Z2-R-MAINT-046, Z2-R-MAINT-047, Z2-R-MAINT-048, Z2-R-MAINT-049, Z2-R-MAINT-050, Z2-R-MAINT-051, Z2-R-MAINT-053, Z2-R-MAINT-056, Z2-R-MAINT-100, Z2-R-MAINT-105, Z2-R-MAINT-106, Z2-R-MAINT-108, Z2-R-MAINT-111, Z2-R-MAINT-112, Z2-R-MAINT-116, Z2-R-MAINT-117, Z2-R-MAINT-122, Z2-R-MAINT-123, Z2-R-MAINT-124, Z2-R-MAINT-125 |
| Z2-I-MAINT-05 | required | Z2-R-MAINT-046, Z2-R-MAINT-057, Z2-R-MAINT-058, Z2-R-MAINT-059, Z2-R-MAINT-060, Z2-R-MAINT-061, Z2-R-MAINT-062, Z2-R-MAINT-063, Z2-R-MAINT-099, Z2-R-MAINT-100, Z2-R-MAINT-102, Z2-R-MAINT-105, Z2-R-MAINT-106, Z2-R-MAINT-107, Z2-R-MAINT-108, Z2-R-MAINT-113, Z2-R-MAINT-115, Z2-R-MAINT-117 |
| Z2-I-MAINT-06 | required | Z2-R-MAINT-064, Z2-R-MAINT-065, Z2-R-MAINT-066, Z2-R-MAINT-067, Z2-R-MAINT-068, Z2-R-MAINT-069, Z2-R-MAINT-070, Z2-R-MAINT-097, Z2-R-MAINT-098, Z2-R-MAINT-108, Z2-R-MAINT-118 |
| Z2-I-MAINT-07 | required | Z2-R-MAINT-068, Z2-R-MAINT-069, Z2-R-MAINT-070, Z2-R-MAINT-071, Z2-R-MAINT-072, Z2-R-MAINT-073, Z2-R-MAINT-074, Z2-R-MAINT-075, Z2-R-MAINT-076, Z2-R-MAINT-077, Z2-R-MAINT-078, Z2-R-MAINT-097, Z2-R-MAINT-098, Z2-R-MAINT-108, Z2-R-MAINT-118 |
| Z2-I-MAINT-08 | required | Z2-R-MAINT-091, Z2-R-MAINT-092, Z2-R-MAINT-093, Z2-R-MAINT-094, Z2-R-MAINT-095, Z2-R-MAINT-096, Z2-R-MAINT-100, Z2-R-MAINT-105, Z2-R-MAINT-106, Z2-R-MAINT-108, Z2-R-MAINT-117 |

## Excluded and deferred

No MAINT requirement is excluded or deferred. Two boundaries are recorded: a C++ API for games, a binary plugin ABI and a third-party mod API are not supported surfaces (Z2-D-002, Z2-D-024, Z2-D-075; Z2-R-MAINT-002), so no maintenance policy exists for them; and the programme defines no fix version after `Z2 Stage 7`, so no roll follows the Stage 7 acceptance (Z2-R-MAINT-034) and a Stage 7 deprecation is removed in the fix version the human ruling that creates it opens.
