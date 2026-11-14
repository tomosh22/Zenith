# MAINT design — supported API, versions and deprecation

This file is part of the MAINT design family (entry: `Zenith2/Docs/Design/MAINT.md`). It specifies the supported API surfaces, the stability classes of public declarations and how a contract block states them, the engine version and its changelog, the compatibility windows of every surface, the deprecation lifecycle and its checks, the committed API surface listing, the Luau-facing classification, and the release and roll procedures that move the engine version at each stage acceptance.

## Scope of this file

| Concern | Section |
|---|---|
| what a game, a tool or another library may rely on, per surface | "Supported surfaces" |
| stable, provisional, experimental and internal declarations | "Stability classes" |
| how a contract block's `Version` field records the class and a deprecation | "Version field clauses" |
| the engine version header, its text form and its ordering key | "Engine version" |
| the committed record of every version step | "Engine changelog" |
| how the version moves through a stage | "Version state machine" |
| the window each surface keeps | "Compatibility windows" |
| marker, diagnostic, removal stage | "Deprecation lifecycle", "Deprecation checks" |
| the shipped stable surface that removal checks compare against | "API surface listing" |
| bindings and the generated `.d.luau` | "Luau API classification" |
| the human steps at each stage acceptance | "Release and roll procedures" |

<!-- informative -->
Every C++ consumer of a Zenith2 public header lives in this repository (games contain no C++, Z2-D-002; modules are static libraries, Z2-D-024), so a broken caller is always visible to the same diff. The deprecation window is still worth its cost for a reason particular to this programme: the whole backlog is drafted before it runs (Z2-D-033, Z2-D-070), and thousands of drafted tickets name exact declarations. A shipped declaration that disappears without a fix version of overlap invalidates every drafted ticket that cites it at once; a deprecation keeps those tickets executable for one fix version while revalidation (Z2-D-129) rewrites them. Before a declaration ships, no executed ticket depends on it, while unexecuted drafted tickets of the current and following stages may cite it by name; those are revalidated through the filer's reconcile mode (`File-Backlog.ps1 --mark-stale`, Z2-D-102). Whether a provisional declaration may therefore change without a deprecation window narrows Z2-D-048 and [Z2-N-CONTRACT-523], so it is the human question Q97 (flagged proposal MAINT-1): the classes below record the distinction, and until the ruling the window binds every public declaration as [Z2-N-CONTRACT-523] reads ("Provisional changes pending Q97").
<!-- /informative -->

## Supported surfaces

The supported API is the union of the surfaces in the table. A surface absent from the table is not supported: no window applies to it and it may change in any diff that migrates its in-repo users.

<!-- elaboration -->
| # | Surface | Consumers | Owner of the surface | Version identity | Window (summary; full rules in "Compatibility windows") | Specified in |
|---|---|---|---|---|---|---|
| S1 | C++ public headers: the root `.h` and `.inl` files of every `Zenith2/Engine/<Lib>/` and of `Zenith2/Player/`, and generated public headers | engine libraries, modules, backends, tools, tests, benchmarks, integration suites; never a game | the owning area of the header (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership file") | the engine version plus the contract block's `since Z2 Stage <N>` | shipped stable: deprecated in fix version D, removed in D+1; provisional: the same until Q97 rules (pending Q97, "Stability classes"); experimental and internal: none | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind"; this file → "Stability classes" |
| S2 | Luau bindings (`Zenith.*` namespaces) and the generated `.d.luau`, `Zenith.luau` and `Types.luau` | game scripts under `Games/<Name>/Scripts/`, engine Luau modules | the owning area of the binding's library; SCRIPT for the binding mechanism | the engine version; the binding-set hash for bytecode | shipped stable bindings: deprecation diagnostic for one fix version, then removed | `Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types"; this file → "Luau API classification" |
| S3 | `game.json` | game maintainers, `zenith_add_game`, `ZenithNewGame` | MOD | `schemaVersion` (integer) | the configure accepts the current version alone; `ZenithNewGame --migrate` migrates every earlier shipped version | `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "game.json migration" |
| S4 | data-defined component schemas `*.schema.json` | game maintainers, ZenithReflect, the cook, `.d.luau` generation | ECS | per-schema `version`; field IDs never reused | migrated from every shipped version | `Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration" |
| S5 | authoring documents (the types of [Z2-N-CONTRACT-552]) | the editor, the cook, `ZenithContent`, game maintainers | the owning area of each document type | per-type `uint32` schema version | migrated from every shipped version; a newer version opens read-only ([Z2-N-CONTRACT-537]) | `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration algorithms" |
| S6 | cooked data | the runtime | ASSET (container) and each kind's owner (payload) | the kind's format version plus `uZENITH_ENGINE_COOK_VERSION` and codec IDs | exact match; any change re-cooks ([Z2-N-CONTRACT-528], [Z2-N-CONTRACT-529]) | `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Constants and fault points" |
| S7 | cooked script bytecode `.zlbc` | the runtime | SCRIPT | container version, Luau pin hash, engine version hash, binding-set hash | exact match; re-cooked on any change ([Z2-N-SCRIPT-504], [Z2-N-SCRIPT-505]) | `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" |
| S8 | saves | players' devices | SAVE | `(major, minor)` save version | every shipped minor of the current major; another major refused ([Z2-N-CONTRACT-524]) | `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Migration pipeline" |
| S9 | replays | tests, the editor, players' devices | SAVE | replay format version | every format version of the release, per SAVE | `Zenith2/Docs/Requirements/SAVE/Stage4Plus.md` → "Z2-R-SAVE-119 Replay readers accept every format version of the release" |
| S10 | configuration keys and layer files | players' `user.config.json`, game configuration layers, the command line | CORE (store), PLAT (sources), each registering area (its keys) | the key name and its schema entry | a shipped key is never renamed; a retired key degrades to the unknown-key warning and stays preserved in the user file | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer" |
| S11 | the Player command line | test harnesses, workload runners, players | MOD | the engine version | a shipped option keeps working with a deprecation warning for one fix version, then is removed | `Zenith2/Docs/Design/MOD.md` → "Zenith_CommandLine.h" |
| S12 | tool command lines and scripts: `ZenithNewGame`, `ZenithCook`, `ZenithContent`, `ZenithDevTool`, `ZenithShaderCompiler`, `Setup-Zenith2.ps1` and the other `Zenith2/Tools/Setup/` scripts, `Zenith2/Tools/Package/` scripts, `Zenith2/Tools/Maintenance/` scripts | game maintainers, CI, the release operator | the owning area of each tool | the engine version | as S11 | each tool's design |
| S13 | packaging and update formats: the mount manifest, the package and update manifests, `release.json` | the runtime of installed builds, the release pipeline | PLAT, PACK, SHIP | each format's own `schemaVersion` or header version | per owner; patches compatible within one release line ([Z2-N-CONTRACT-528]) | `Zenith2/Docs/Design/PACK/Formats.md` → "Binary format registry"; `Zenith2/Docs/Design/SHIP/Release.md` → "Release manifest" |
| S14 | modules | the composition root | MOD | — | no binary compatibility surface exists: modules are statically linked (Z2-D-024) | `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" |

Surfaces that are not supported, each with its reason:

| Not supported | Reason |
|---|---|
| a C++ API for games | a game contains no C++ (Z2-D-002) |
| a binary plugin ABI | there are no dynamic plugins (Z2-D-024) |
| a third-party mod API | modding is excluded (Z2-D-075) |
| gate step command lines and gate data files | PROC's protected interface (`Zenith2/Docs/Design/PROC.md` → "Gate step command lines"), versioned by PROC tickets, not by this policy |
| `Internal/` headers and every symbol they declare | private to the library and its tests ([Z2-N-CONTRACT-024]) |
| anything under `Zenith2/out/` except the generated public headers of S1 and the generated Luau files of S2 | build outputs |

## Stability classes

Every contract block of a public header (a type block or a `functions` block, [Z2-N-CONTRACT-111], [Z2-N-CONTRACT-112]) has exactly one stability class. The class decides whether the compatibility window of surface S1 applies.

<!-- elaboration -->
| Class | How it is determined | May change incompatibly | Consumers allowed | Listed in `api_surface.json` |
|---|---|---|---|---|
| `stable` and shipped | no class clause, and the block's effective `since` stage is below the engine version's MAJOR (its stage was accepted, "Version state machine") | only through the deprecation lifecycle | any target the layering allows | yes, from the roll that ships it |
| `stable` and provisional | no class clause, and effective `since` ≥ MAJOR (its stage is not accepted yet; pulled-forward work carries a `since` above the stage it lands in) | pending Q97: by default only through the deprecation lifecycle, as [Z2-N-CONTRACT-523] reads; under Q97 (a), in any diff that migrates every in-repo caller and lists every drafted ticket that cites it ("Provisional changes pending Q97") | any target the layering allows | no |
| `experimental` | the clause `experimental until Z2 Stage <G> (consumers <targets>)` | yes, in any diff that migrates every in-repo caller; it is never deprecated | the targets named in the clause, the declaring library and its test targets | no |
| `internal` | the clause `internal`, or the name rules below | yes, in any diff that migrates every in-repo caller | the composition root, tools, tests, benchmarks and integration suites, and the declaring library | no |

Name rules for `internal` (the clause is then optional and, when written, redundant):

| Rule | Headers it covers | Why internal |
|---|---|---|
| N1 | a root header whose file name ends `TestAccess.h` | test access types; `TestSeparationCheck` already confines them ([Z2-N-CONTRACT-414]) |
| N2 | a root header whose file name ends `ToolAccess.h` | tool access types, confined the same way |
| N3 | every root header of a `KIND BACKEND` library | a backend exports its factory alone, consumed by the composition root and test, benchmark and tool executables ([Z2-N-CONTRACT-042], [Z2-N-ARCH-169]) |
| N4 | `Zenith_Module_<MODULE_NAME>.h` of a `KIND MODULE` library | the two module entry points, called by the generated composition TU alone ([Z2-N-ARCH-168]) |

Generated public headers (`*.generated.h` under `${ZENITH_OUT}/generated/`, attributed to `ZenithShadersGenerated` and included by `ZenithFlux`, `ZenithUI`, `Zenith2D`, `ZenithStreaming`, `ZenithVFX` and the other libraries that declare it) are **public-generated**, not internal: each carries the contract blocks its generator emits ([Z2-N-CONTRACT-114]) with their own class, they are reviewed as generated public API outputs ([Z2-N-CONTRACT-492]), and they lie outside every MAINT scan because they are not under `Zenith2/Engine/*/`; their compatibility follows the Slang source that generates them.

Rules of the classes:

- The **effective `since` stage** of a block is `max(since, 2)`. The engine version first exists in Stage 2, so the blocks whose text says `since Z2 Stage 0` or `since Z2 Stage 1` (about 46 in the CORE, MOD and SCRIPT designs, whose texts stay as written) are provisional until the Stage 2 acceptance and ship at its roll. Every rule of this family that reads `since` (`CLS-GRADUATE`, `DEP-STAGES`, the `SUR` rules, the roll) reads the effective stage.
- An `experimental` header holds experimental blocks alone: a header that holds one experimental block holds no stable or internal block, so the consumer rule can be checked per `#include` line.
- `G` is a stage number with `since ≤ G ≤ 7`. While MAJOR ≤ G the block may stay experimental; by the roll that ends stage G it is either graduated (the clause removed, so it becomes stable and ships at that roll) or removed.
- The `consumers` list names CMake targets (`declared_deps.json` names: libraries, tools, `Zenith_Bench_<Name>`, `Zenith_Integration_<Name>`), byte-ordered, comma and space separated, at least one entry.
- A shipped stable block is never reclassified as experimental or internal; leaving the stable surface goes through the deprecation lifecycle.
- Pending Q97: until it is ruled, an experimental or internal clause may be given only to a NEW declaration, in the diff that introduces it. Giving one to a stable block that exists in an earlier commit (shipped or provisional) is an incompatible change of a stable declaration and follows [Z2-N-CONTRACT-523]: it needs the deprecation lifecycle. No tree check can see a block's history, so the reviewer confirms it for every diff that adds a class clause ("Provisional changes pending Q97"). Under Q97 (a) a provisional block may become experimental or internal in any diff that meets the conditions of that section; a shipped block never may.
- An experimental or internal block may become stable in any diff (graduation).
- A declaration inside a class or a `functions` block inherits the block's class; there is no member-level class.
- The classification is checked by the MAINT checks of `Zenith2/Docs/Design/MAINT/Tests.md` → "PowerShell check tests" (family `CLS`), which run in the `Test` gate step; the clause grammar's eventual home in `ApiContractCheck` is a PROC change (cross-area note MT3).

### Provisional changes pending Q97

- Until the human rules Q97 (flagged proposal MAINT-1), [Z2-N-CONTRACT-523] binds every public declaration as written: an incompatible change of a provisional declaration goes through the deprecation lifecycle like a shipped one. The listing-based checks (`SUR-*`, `DEP-LOGGED`) see shipped blocks alone, so for a provisional declaration the window is confirmed by review.
- Until Q97 is ruled, re-classing an existing stable block as experimental or internal is likewise such a change ("Rules of the classes"); only a declaration the diff introduces may start experimental or internal.
- If Q97 is ruled (a), a provisional declaration may change incompatibly, or be re-classed experimental or internal, without a deprecation, in a diff that (1) migrates every in-repo caller; (2) edits the owning design section that specifies the declaration (Z2-D-085), which is what makes `File-Backlog.ps1 --mark-stale` (Z2-D-102) mark every drafted ticket whose `Cites:` reach that section; and (3) lists, in its own commit message body under the line `Drafted tickets citing <name>:`, every drafted, unexecuted ticket whose body names the declaration anywhere (Goal, Definition of Done, Implementation Contract, Validation, Compatibility, `Cites:`, `Nouns:`), found by the full-body search `zagent docs search "<name>"` over `Zenith2/Docs/Board/tickets/`. The list is a review aid confirmed by the reviewer, not a mechanical guarantee; a ticket on it that `--mark-stale` did not reach is marked by the orchestrator, which needs the Backlog option `--mark-stale --keys <list>` requested of PROC conditionally on Q97 (a) (cross-area note MT7).
- If Q97 is ruled (b), the provisional class keeps only its listing role (a provisional block is not listed) and the S1 window binds it.

<!-- informative -->
"Provisional" is computed, never written: a declaration's `since` stage and the current engine MAJOR decide it. Pulled-forward work (a Stage 3 ticket scheduled in the Stage 2 sprint) carries `since Z2 Stage 3` and therefore stays provisional until Stage 3 is accepted, which matches the fact that no Stage 3 ticket has been revalidated against it yet. This is the meaning of "shipped in a fix version" that [Z2-N-CONTRACT-467] already uses for error enumerators.
<!-- /informative -->

## Version field clauses

The `Version` field of a contract block ([Z2-N-CONTRACT-117]) is read as one string: the text after `/// Version: ` with each continuation line (`///   ` then text, [Z2-N-CONTRACT-115]) appended after a single space.

### Clause grammar

<!-- elaboration -->
```
version-text   := since-part { "; " segment }
since-part     := "since Z2 Stage " stage [ free-tail ]
segment        := class-clause | deprecated-clause | free-segment
class-clause   := experimental-clause | "internal"
experimental-clause := "experimental until Z2 Stage " stage " (consumers " target { ", " target } ")"
deprecated-clause   := "deprecated Z2 Stage " stage ", use " replacement ", removed Z2 Stage " removal-stage
removal-stage  := stage | "8"            (8 only for a deprecation made at MAJOR 7, "Deprecations at Stage 7")
free-segment   := any text that does not begin with "experimental", "deprecated" or equal "internal"
free-tail      := any text that does not contain "; " (the existing forms "since Z2 Stage 2. Source API", "since Z2 Stage 2 (registered by the API-only slice)", "since Z2 Stage 3: …", "since Z2 Stage 2 as …" and "since Z2 Stage 2, …")
stage          := "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7"
target         := ^Zenith[A-Za-z0-9_]+$
replacement    := ^[A-Za-z_][A-Za-z0-9_]*(::~?[A-Za-z_][A-Za-z0-9_]*)*(\(\))?$
```

The segments are split at every `"; "` (semicolon and one space). A class clause and a deprecated clause may stand in any segment position after the since part; free segments may stand anywhere. Existing `Version` texts of every design ("since Z2 Stage 2; type version 1", "since Z2 Stage 2; values stable") are free segments and are valid unchanged.

### Clause rules

| Rule ID | Rule | Finding text |
|---|---|---|
| CLS-SINCE | the field begins with `since Z2 Stage <stage>` | `<header>:<line>: CLS-SINCE: Version of <block> does not begin with "since Z2 Stage <0-7>"` |
| CLS-PREFIX | a segment that begins with `experimental` or `deprecated`, or equals `internal`, matches its clause production exactly | `<header>:<line>: CLS-PREFIX: segment "<text>" of <block> is not a well-formed <clause> clause` |
| CLS-ONE | at most one class clause and at most one deprecated clause per block | `CLS-ONE: <block> carries <n> <clause> clauses` |
| CLS-EXCLUSIVE | a class clause and a deprecated clause never appear in the same block (experimental and internal declarations change without deprecation) | `CLS-EXCLUSIVE: <block> is <class> and deprecated` |
| CLS-GRADUATE | for an experimental clause, `since ≤ G` | `CLS-GRADUATE: <block> graduates at Stage <G> before its since stage <N>` |
| CLS-EXPIRED | for an experimental clause, `G ≥ MAJOR` | `CLS-EXPIRED: <block> was experimental until Z2 Stage <G>; graduate it (remove the clause) or remove it` |
| CLS-CONSUMERS | every consumer target exists in `declared_deps.json` | `CLS-CONSUMERS: <block> names unknown consumer <target>` |
| CLS-HOMOGENEOUS | a header with an experimental block has no other class | `CLS-HOMOGENEOUS: <header> mixes experimental and <class> blocks` |
| CLS-INCLUDE | an experimental header is included only from sources of its library, its test targets and its listed consumers; an internal header (by clause or rule N1 to N4) only from the composition root (`ZenithRuntime`, `ZenithPlayer`, game composition TUs), tools, tests, benchmarks, integration suites and its own library | `<source>:<line>: CLS-INCLUDE: <header> is <class> and <target> may not include it` |
| CLS-REDUNDANT | an `internal` clause on a header covered by a name rule | report only: `CLS-REDUNDANT: <header> is internal by rule <Nn>` |

The `consumers` list of rule CLS-INCLUDE maps each target to its source directory through `declared_deps.json`'s `dir` member, and a source belongs to a target when its path starts with that directory; a test target's sources are the library's `Tests/` subtree.

### Deprecation markers

A deprecation has two textual parts that must agree:

1. **The attribute** on the declaration: `[[deprecated("use <R>; removed in Z2 Stage <D+1>")]]` ([Z2-N-CONTRACT-522]), placed where C++20 accepts it: `class [[deprecated("…")]] Zenith_X`, `struct [[deprecated("…")]] Zenith_X`, `enum [[deprecated("…")]] Zenith_X : uint8`, before a function's declaration specifiers, or after an enumerator name (`VALUE [[deprecated("…")]] = 3`).
2. **The contract text**: for a whole type or a whole `functions` block, the deprecated clause in its `Version` field; for one member, one function of a `functions` block, or one enumerator, the deviation line `/// @deviation Version: deprecated Z2 Stage <D>, use <R>, removed Z2 Stage <D+1>` directly above the declaration ([Z2-N-CONTRACT-120]).

Rules:

- `D` is the engine MAJOR at the time the deprecating diff lands, which is the stage of the sprint the ticket runs in; a worker reads it from `uZENITH_ENGINE_VERSION_MAJOR` in `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h`, never from the ticket's fix version (a pulled-forward ticket lands in an earlier stage than its fix version).
- `R` names the replacement by its qualified C++ name; a removed entry point with no one-to-one replacement names the type or function a caller now uses, never prose.
- Whole-block deprecation puts the attribute on the type's declaration line, or, for a `functions` block, on the declaration line of every function declaration between that block's close line and the next `/// @contract` line.
- A **function declaration** is found in the comment- and literal-stripped text by its shape, never by an exception specification or an attribute. At namespace and class scope the scanner splits the code into statements, each running from the end of the previous `;`, `{` or `}` to the next `;` or `{`; it skips every function body and every `enum` body by brace matching. A statement is a function declaration when, outside `[[ … ]]` attribute brackets, a `(` comes before every top-level `=` that is not part of an `operator` name, and the statement does not begin with `static_assert`, `using`, `typedef`, `friend class` or `friend struct`; a `{` that ends a function declaration opens a function body, any other `{` a scope. Its **declaration line** is the line of its first token after any `template<…>` parameter list, so a declaration split over several lines counts once, and an attribute placed before its declaration specifiers sits on that line.

`ApiContractCheck` step 7 accepts a `@deviation` line directly above a member or function declaration ([Z2-N-CONTRACT-120]), and PROC's step covers every member a public type declares, a function, a data member and an enumerator, with passing self-tests for a deviation line above a data member and above an enumerator (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "ApiContractCheck"). So a member function, a data member and an enumerator are each deprecated with a deviation line.

## Engine version

### Zenith_EngineVersion header

The header is a MAINT-owned file inside ZenithCore (cross-area note MT1); its implementation file is `Zenith2/Engine/ZenithCore/Internal/Zenith_EngineVersion.cpp` and its tests are `Zenith2/Engine/ZenithCore/Tests/Zenith_EngineVersion_Tests.cpp`.

```cpp
// Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract value Zenith_EngineVersion
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None. Components are decimal semantic-version numbers.
/// Errors: None. Construction never fails; out-of-range text is rejected by Zenith_ParseEngineVersion.
/// Version: since Z2 Stage 2. The member layout is not serialized; the text form and the key are the
///   persistent forms (Zenith2/Docs/Design/MAINT/Versioning.md "Engine version").
/// @endcontract
struct Zenith_EngineVersion
{
	const uint16 m_uMajor = 0;          // the stage of the fix version under development or released
	const uint16 m_uMinor = 0;          // 0 in Stages 0 to 7 (Zenith2/Docs/Design/MAINT/Versioning.md "Version numbers")
	const uint16 m_uPatch = 0;          // release fixes after the release step
	const uint16 m_uDevCounter = 0;     // K of "-dev.K"; 0 whenever m_bPrerelease is false
	const bool m_bPrerelease = false;    // true: the text carries "-dev.K"

	friend constexpr bool operator==(const Zenith_EngineVersion&, const Zenith_EngineVersion&) = default;
};

/// @contract enum Zenith_EngineVersionError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Returned by Zenith_ParseEngineVersion and Zenith_FormatEngineVersion; see "Error enumeration".
/// Version: since Z2 Stage 2; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_EngineVersionError : uint8
{
	ENGINE_VERSION_ERROR_MALFORMED = 1,          // text not of the grammar
	ENGINE_VERSION_ERROR_COMPONENT_RANGE,        // a MAJOR, MINOR or PATCH component above 65535
	ENGINE_VERSION_ERROR_DEV_COUNTER_RANGE,      // a dev counter above uZENITH_ENGINE_VERSION_MAX_DEV
	ENGINE_VERSION_ERROR_BUFFER_TOO_SMALL,       // the destination cannot hold the text and its NUL
};

/// @contract functions Zenith_EngineVersionFunctions
/// Ownership: constexpr constants and pure functions; no state.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Parse and Format return Zenith_Result<…, Zenith_EngineVersionError>; nothing asserts on input text.
/// Version: since Z2 Stage 2. The constants change only as Zenith2/Docs/Design/MAINT/Versioning.md
///   "Version state machine" states; every change appends an entry to Zenith2/Tools/Maintenance/engine_changelog.json.
/// @endcontract
constexpr uint16 uZENITH_ENGINE_VERSION_MAJOR = 2;
constexpr uint16 uZENITH_ENGINE_VERSION_MINOR = 0;
constexpr uint16 uZENITH_ENGINE_VERSION_PATCH = 0;
constexpr uint16 uZENITH_ENGINE_VERSION_DEV = 0;
constexpr bool bZENITH_ENGINE_VERSION_PRERELEASE = true;
constexpr const char* szZENITH_ENGINE_VERSION = "2.0.0-dev.0";
constexpr uint16 uZENITH_ENGINE_VERSION_MAX_DEV = 65534;
constexpr uint32 uZENITH_ENGINE_VERSION_MAX_TEXT_BYTES = 28;   // "65535.65535.65535-dev.65534" (27) plus NUL

constexpr Zenith_EngineVersion Zenith_GetEngineVersion();
constexpr uint64 Zenith_GetEngineVersionKey(const Zenith_EngineVersion& xVersion);
constexpr sint32 Zenith_CompareEngineVersions(const Zenith_EngineVersion& xA, const Zenith_EngineVersion& xB);
Zenith_Result<uint32, Zenith_EngineVersionError> Zenith_FormatEngineVersion(
	const Zenith_EngineVersion& xVersion, Zenith_Span<char> xDestination);
Zenith_Result<Zenith_EngineVersion, Zenith_EngineVersionError> Zenith_ParseEngineVersion(
	Zenith_StringView strText);
constexpr const char* Zenith_ToString(Zenith_EngineVersionError eError);
```

The constexpr bodies are inline in the header; `Zenith_FormatEngineVersion` and `Zenith_ParseEngineVersion` are defined in the `Internal/` file.

The members of `Zenith_EngineVersion` are `const`, as an `IMMUTABLE` type's are: a value is aggregate-initialised once (`Zenith_GetEngineVersion` returns one braced list, and `Zenith_ParseEngineVersion` reads every component into locals and builds its result from them at the end) and then only copied, compared and read, so no code assigns to one; each `const` member keeps its default member initializer, so the type stays default-constructible (`Zenith_Result<Zenith_EngineVersion, Zenith_EngineVersionError>` requires that of its `T`) and an aggregate (the braced list still initialises every member), and it stays trivially copyable, which its `value` contract kind requires.

The constant values shown are the values the first MAINT ticket writes ("Stage plan" of `Zenith2/Docs/Design/MAINT.md`); each step of "Version state machine" rewrites the five numeric constants and `szZENITH_ENGINE_VERSION` together, in one diff.

### Version numbers

<!-- elaboration -->
| Component | Meaning | Changed by |
|---|---|---|
| MAJOR | the stage of the fix version under development (`Z2 Stage <MAJOR>`), or of the fix version just released | the roll (sets MAJOR + 1) |
| MINOR | always 0 in this programme | nothing in Stages 0 to 7; a maintenance branch of a released fix version would increment it, and Zenith2 has none (Z2 runs `direct` on one tree, Z2-D-086) |
| PATCH | the number of release fixes made after the release step of the current MAJOR | each `release-fix` changelog entry (+1) |
| `-dev.K` | present while the stage is under development; K counts the compatibility events of the stage | each `compat` changelog entry (K + 1); the release step removes the suffix |

Every fix version is a MAJOR because each may remove what its predecessor deprecated (S1's window), which semantic versioning classes as an incompatible change. The development builds between two releases are semantic-version pre-releases of the next MAJOR, which carry no compatibility promise between one another, and K tells a game maintainer how many compatibility events separate two development builds.

### Text form

```
engine-version := major "." minor "." patch [ "-dev." dev ]
major, minor, patch := "0" | [1-9][0-9]{0,4}        (value ≤ 65535)
dev := "0" | [1-9][0-9]{0,4}                          (value ≤ 65534)
```

`Zenith_ParseEngineVersion` steps, in order:

1. Length 0 or above 27 bytes: `ENGINE_VERSION_ERROR_MALFORMED`.
2. Scan three decimal components separated by `.`; a component that is empty, has a leading zero (other than `0` itself), has more than five digits or contains a byte outside `0`–`9`: `MALFORMED`.
3. A component above 65535: `ENGINE_VERSION_ERROR_COMPONENT_RANGE`.
4. End of text: success with `m_bPrerelease = false`, `m_uDevCounter = 0`.
5. Otherwise the next five bytes are exactly `-dev.` (else `MALFORMED`), followed by one decimal number under the rules of step 2 that ends the text (else `MALFORMED`); a value above 65534: `ENGINE_VERSION_ERROR_DEV_COUNTER_RANGE`; success with `m_bPrerelease = true`.

Complexity O(length); no allocation; the input is untrusted text and never reaches an assert ([Z2-N-CONTRACT-472]).

`Zenith_FormatEngineVersion` writes the text form and a NUL into `xDestination` and returns the text length without the NUL. When `xDestination.GetCount()` is below the length plus one it writes nothing and returns `ENGINE_VERSION_ERROR_BUFFER_TOO_SMALL`. A value with `m_bPrerelease == false` and a non-zero `m_uDevCounter`, or a dev counter above 65534, is a programmer error (`ZENITH_ASSERT`), because no parse can produce it.

### Ordering key

```
Zenith_GetEngineVersionKey(v) = (u64(v.m_uMajor) << 48) | (u64(v.m_uMinor) << 32) | (u64(v.m_uPatch) << 16)
                               | (v.m_bPrerelease ? u64(v.m_uDevCounter) : 0xFFFF)
```

The key orders exactly as semantic-version precedence over this grammar: a pre-release precedes its release (`2.0.0-dev.65534 < 2.0.0` because 65534 < 0xFFFF), dev counters compare numerically, and every release of a MAJOR precedes the next MAJOR's first development build. `Zenith_CompareEngineVersions` returns −1, 0 or 1 from the two keys. `Zenith_GetEngineVersion()` returns the value of the five constants.

### Engine version hash for bytecode

`Zenith_ScriptEngineVersionHash()` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptCompileOptions header") returns `Zenith_GetEngineVersionKey(Zenith_GetEngineVersion())` and is the one producer of `Zenith_ScriptCookDesc::m_ulEngineVersionHash` and of the runtime's build identity (cross-area note MT2 (e), adopted by SCRIPT, row SC30). The key changes at every changelog entry (each compatibility event, the release, each release fix and the roll), so bytecode re-cooks then, and two builds of one commit agree; SCRIPT keys only regenerated cook output by it and commits no bytecode or cache fixture whose header holds it.

### Where the version is shown

<!-- assumption: pending MOD, SHIP -->
The first and third rows are requests (cross-area notes MT5, MT4) that the owners have not yet adopted; the `zenith.d.luau` row is SCRIPT's, adopted (cross-area note MT2 (b), row SC30; `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Definition emitter procedure").

| Consumer | What it shows | Owner of the change |
|---|---|---|
| the startup composition dump and the first log record of every run | `Zenith2 engine <szZENITH_ENGINE_VERSION>` | MOD (cross-area note MT5) |
| the generated `zenith.d.luau` header comment | `-- generated; engine <szZENITH_ENGINE_VERSION>; binding-set hash 0x<16 hex digits>` | SCRIPT (cross-area note MT2) |
| release notes and upgrade guides | the released engine version and the two versions a guide spans | SHIP (cross-area note MT4) |
| `api_surface.json`, the surface growth, calibration and rehearsal records | the version they were produced at | MAINT |
<!-- /assumption -->

## Engine changelog

`Zenith2/Tools/Maintenance/engine_changelog.json` is the committed, append-only record of every version step. Its format follows PROC's format conventions (`Zenith2/Docs/Design/PROC.md` → "Format conventions"): UTF-8 without BOM, LF, four-space indentation, `schema` and `schemaVersion` first.

### Changelog schema

<!-- elaboration -->
```json
{
    "schema": "zenith2.engine-changelog",
    "schemaVersion": 1,
    "comment": "Append-only. One entry per engine version step; see Zenith2/Docs/Design/MAINT/Versioning.md \"Engine changelog\".",
    "entries": [
        {
            "version": "2.0.0-dev.0",
            "kind": "roll",
            "stage": 2,
            "summary": "Engine version introduced.",
            "ticket": "Z2T-MAINT-001",
            "released": null,
            "items": []
        },
        {
            "version": "2.0.0-dev.1",
            "kind": "compat",
            "stage": 2,
            "summary": "Zenith_ArenaDesc::m_uBlockBytes deprecated in favour of m_ulBlockBytes.",
            "ticket": "Z2T-CORE-071",
            "released": null,
            "items": [
                {
                    "surface": "cpp",
                    "change": "deprecated",
                    "library": "ZenithCore",
                    "header": "Zenith_Arena.h",
                    "name": "Zenith_ArenaDesc::m_uBlockBytes",
                    "replacement": "Zenith_ArenaDesc::m_ulBlockBytes",
                    "removedIn": 3
                }
            ]
        }
    ]
}
```

| Member | Type | Rule |
|---|---|---|
| `version` | string | the engine-version text form; equal to the header's `szZENITH_ENGINE_VERSION` after this entry |
| `kind` | string | `roll`, `compat`, `release` or `release-fix` |
| `stage` | integer 0–7 | the MAJOR of `version` |
| `summary` | string | one sentence, 1–400 UTF-8 bytes, no line break |
| `ticket` | string | `^(Z2T-[A-Z]+-[0-9]{3}\|Z2-[0-9]+)$`: the Draft-ID of the drafted ticket whose diff appends the entry, or the board key of a ticket filed without one (the removal, graduation and rehearsal TASKs filed from the roll's plan, and BUGs filed during an acceptance) |
| `released` | object or null | `roll` alone: `{ "version": "<N.0.P>", "commit": "<40 lower-case hex>" }`, the release this roll closes; `null` for the first roll and every other kind |
| `items` | array | `compat`: one or more items; every other kind: empty |

Item members:

| Member | Type | Rule |
|---|---|---|
| `surface` | string | one of `cpp`, `luau`, `game-json`, `schema-json`, `authoring`, `cooked`, `bytecode`, `save`, `replay`, `config`, `player-cli`, `tool-cli`, `package` (rows S1 to S13 of "Supported surfaces", in order) |
| `change` | string | `deprecated`, `removed`, `format-version`, `added-optional`, `retired` |
| `library` | string or null | `cpp`: the library target; `luau`: the binding's library; otherwise null |
| `header` | string or null | `cpp`: the root header file name; otherwise null |
| `name` | string | the declaration (`Zenith_X`, `Zenith_X::Member`), binding (`Zenith.Physics.Raycast`), key path, document type or format name |
| `replacement` | string or null | `deprecated`: the replacement; otherwise null |
| `removedIn` | integer or null | `deprecated`: D + 1; otherwise null |

Which changes are compatibility events (each needs one `compat` entry, with every item of the diff in it):

| Change | `surface`, `change` |
|---|---|
| deprecating a shipped stable C++ block, member or enumerator | `cpp`, `deprecated` |
| removing a shipped deprecated C++ declaration | `cpp`, `removed` |
| deprecating or removing a shipped stable Luau binding | `luau`, `deprecated` / `removed` |
| bumping `game.json`'s `schemaVersion` | `game-json`, `format-version` |
| adding an optional key to the current `game.json` schema | `game-json`, `added-optional` |
| a schema version step of a data-defined component schema or an authoring-document type that has shipped | `schema-json` / `authoring`, `format-version` |
| bumping `uZENITH_ENGINE_COOK_VERSION` or a cooked kind's format version | `cooked`, `format-version` |
| bumping the `.zlbc` container version | `bytecode`, `format-version` |
| bumping a save or replay format version | `save` / `replay`, `format-version` |
| retiring a shipped configuration key | `config`, `retired` |
| deprecating or removing a shipped Player or tool option | `player-cli` / `tool-cli`, `deprecated` / `removed` |
| bumping a packaging or update format version | `package`, `format-version` |

A diff that makes several of these changes appends one entry listing every item; the dev counter moves by one per entry, never per item.

### Changelog rules

| Rule ID | Rule |
|---|---|
| CHG-SCHEMA | the file parses as JSON with the members and types of the two tables, no other member, `schema` `zenith2.engine-changelog` and `schemaVersion` 1 |
| CHG-FIRST | the first entry is a `roll` with `released: null` whose version is `<S>.0.0-dev.0` |
| CHG-TRANSITION | each subsequent entry is a transition the table of "Version state machine" allows from the state the previous entries reached |
| CHG-VERSION-STEP | each entry's `version` is exactly the version the transition produces |
| CHG-STAGE | `stage` equals the MAJOR of `version` |
| CHG-HEADER | the last entry's `version` equals `szZENITH_ENGINE_VERSION`, and the five numeric constants equal its components |
| CHG-DRAFTID | `ticket` matches its pattern (a Draft-ID or a board key) |
| CHG-ITEM | every item obeys the item table and `compat` entries have at least one item |
| CHG-RELEASED | a `roll`'s `released.version` equals the version reached immediately before it |

## Version state machine

<!-- elaboration -->
States: `DEV(N, K)` (header `N.0.0-dev.K`) and `RELEASE(N, P)` (header `N.0.P`). The initial state comes from the first entry, `DEV(S, 0)`.

| State | Event (changelog entry kind) | Next state | Who | Precondition |
|---|---|---|---|---|
| `DEV(N, K)` | `compat` | `DEV(N, K + 1)` | the ticket whose diff makes the compatibility change | K + 1 ≤ 65534 |
| `DEV(N, K)` | `release` | `RELEASE(N, 0)` | the human, first step of `Z2 S<N> stage acceptance` ("Release and roll procedures") | `Set-Zenith2EngineRelease.ps1` preconditions |
| `DEV(N, K)` | `release-fix`, `roll` | refused (CHG-TRANSITION) | — | — |
| `RELEASE(N, P)` | `release-fix` | `RELEASE(N, P + 1)` | a diff of the release-fix scope (below) that lands after the release step: an acceptance fix, or, after the Stage 7 release, any maintenance change | P + 1 ≤ 65535 |
| `RELEASE(N, P)` | `roll` | `DEV(N + 1, 0)` | the human, last step of the acceptance | N ≤ 6; `Invoke-Zenith2VersionRoll.ps1` preconditions |
| `RELEASE(N, P)`, N ≤ 6 | `compat` | refused (CHG-TRANSITION): a compatibility change found during an acceptance waits for the next stage; the acceptance records it as a known issue and files its ticket in fix version N + 1, and when an acceptance requirement cannot be met without it, the human appends a `Z2-D-` ruling that the requirement is met in stage N + 1 | — | — |
| `RELEASE(7, P)` | `compat` | `RELEASE(7, P + 1)` | a ticket that makes a compatibility change after the Stage 7 release (ruling MAINT-R1-RULINGS R7) | the change follows [Z2-N-CONTRACT-523] ("Deprecations at Stage 7") |
| `RELEASE(7, P)` | `roll` | refused: the programme defines no fix version after `Z2 Stage 7`; the human ruling that creates one also rolls | — | — |

- **Release-fix scope.** A diff that lands while the header is in a `RELEASE` state and changes a file under `Zenith2/Engine/`, `Zenith2/Player/`, `Zenith2/Shaders/`, `Zenith2/CMake/`, `Zenith2/ThirdParty/` or `Zenith2/Games/`, or a non-test source or script under `Zenith2/Tools/`, appends a `release-fix` entry (the stage acceptance lands only fixes, with every lane idle, Z2-D-083); the reviewer of such a diff confirms the entry. The acceptance's own records are exempt and bump nothing: files under `Zenith2/Docs/` (the surface growth report, the calibration record, `Zenith2/Docs/DecisionLog.md` entries), the MAINT data files `Zenith2/Tools/Maintenance/*.json`, and the gate data files `Zenith2/Tools/Gates/libgraph_baseline.json` and `complexity_profile.json`; and the diffs that carry the `release` entry and the `roll` entry themselves, although they change `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h` (they are the transitions, not fixes).
- A diff that makes a compatibility change of the table "Which changes are compatibility events" appends a `compat` entry and rewrites the header in the same diff; the `CHG-HEADER` rule makes a header edit without an entry, or an entry without a header edit, a test failure, and the `DEP-LOGGED` rule of "Deprecation checks" makes an unlogged C++ deprecation of a shipped block a test failure; the remaining kinds are confirmed by review, because a format bump is not textually detectable across all owners.
- MAJOR therefore equals the stage of the sprint in progress: the roll is the last step of the acceptance, and `zagent sprint complete` refuses to close a sprint before its gate ticket is Done (the Vault change filed with the ZEN tooling tickets, `Zenith2/Docs/Roadmap.md` → "Stage 0 Tooling bring-up", entry criterion 4), so the next sprint starts with the rolled header.

## Compatibility windows

The full window per surface; the row numbers are those of "Supported surfaces".

<!-- elaboration -->
| # | Change | Allowed when | Required in the same diff | Then |
|---|---|---|---|---|
| S1 | change or remove an experimental or internal declaration | always | every in-repo caller migrated; tests updated | — |
| S1 | change incompatibly or remove a provisional declaration | pending Q97: by default as a shipped one (the deprecation rows below); under Q97 (a), always | under Q97 (a): every in-repo caller migrated and the citing drafted tickets listed ("Provisional changes pending Q97") | — |
| S1 | add a declaration | always (it is provisional) | its contract block with `since Z2 Stage <fix version of the ticket>`, tests | ships at the roll of that stage |
| S1 | change a shipped stable declaration's signature or semantics incompatibly | never directly | — | add the replacement and deprecate the old one |
| S1 | deprecate a shipped stable declaration | in `DEV(N, K)` | the markers with D = N; every caller migrated to R ([Z2-N-CONTRACT-523]); the deprecated body forwards to R; its own tests moved to R; a `compat` entry | removed in fix version N + 1 |
| S1 | remove a deprecated declaration | in `DEV(N, K)` with N = D + 1 | every reference removed; a `compat` entry | the roll records `removedIn` |
| S1 | change a shipped stable declaration compatibly (a new member, a new overload, a new enumerator appended) | in `DEV` | the new parts carry their own `since` through a `@deviation Version: since Z2 Stage <N>` line | ships at the roll |
| S2 | the same rows for Luau bindings | as S1 | SCRIPT's binding descriptor fields ("Luau API classification") | as S1 |
| S3 | a `game.json` schema change | in `DEV` | `schemaVersion` + 1, the `ZenithNewGame --migrate` step, every in-repo `game.json` migrated, a `compat` entry | the configure accepts the new version alone |
| S4, S5 | a schema change of a shipped schema or document type | in `DEV` | one migration step, the new version's fixture and expected output ([Z2-N-CONTRACT-549]), a `compat` entry | migrated forever |
| S6, S7 | a cooked or bytecode format change | in `DEV` | the format or cook version bump, a `compat` entry | every game re-cooks |
| S8, S9 | a save or replay format change | in `DEV` | SAVE's migration step and fixture, a `compat` entry | readable for the save major's lifetime |
| S10 | rename a shipped configuration key | never | — | add the new key; retire the old one |
| S10 | retire a shipped configuration key | its schema entry's description begins `deprecated Z2 Stage <D>, use <key>` for one fix version first | a `compat` entry at retirement | the unknown-key warning and file preservation of PLAT apply |
| S11, S12 | change or remove a shipped option | in `DEV` | the old option keeps working and prints `warning: <option> is deprecated since Z2 Stage <D>; use <replacement>; removed in Z2 Stage <D+1>` once per run for one fix version, then is removed; a `compat` entry at each step | — |
| S13 | a packaging or update format change | per PACK, PLAT and SHIP designs | a `compat` entry | patches only within a release line |

<!-- informative -->
For cooked data, [Z2-N-CONTRACT-529]'s "engine version" is the engine cook version `uZENITH_ENGINE_COOK_VERSION` that ASSET defines and every cooked header records, not the engine version of this file: a compatibility event that leaves cooked bytes unchanged does not re-cook. Cross-document row X385 asks the core-doc batch to say so.
<!-- /informative -->

## Deprecation lifecycle

### Declaration state machine

<!-- elaboration -->
States of one public declaration (a block, a member or an enumerator):

| State | Defined by |
|---|---|
| `PROVISIONAL` | stable class, effective `since` ≥ MAJOR |
| `SHIPPED` | stable class, effective `since` < MAJOR, no deprecation marker |
| `DEPRECATED` | markers present with D; MAJOR = D |
| `REMOVAL-DUE` | markers present with D; MAJOR = D + 1 |
| `OVERDUE` | markers present with D; MAJOR > D + 1 (a check failure) |
| `REMOVED` | absent from the tree; `api_surface.json` records `removedIn` |
| `EXPERIMENTAL` | experimental clause with G ≥ MAJOR |
| `EXPIRED` | experimental clause with G < MAJOR (a check failure) |
| `INTERNAL` | internal clause or name rule |

| State | Event | Next state | Check that enforces it |
|---|---|---|---|
| `PROVISIONAL` | an incompatible edit or a removal | pending Q97: by default the `SHIPPED` rows below apply (review confirms); under Q97 (a), `PROVISIONAL` or absent, with callers migrated and citing drafted tickets listed | Build, Test, review |
| `PROVISIONAL` | a compatible edit | `PROVISIONAL` | Build, Test |
| `PROVISIONAL` | the roll of its `since` stage | `SHIPPED`; added to `api_surface.json` | `SUR-UNLISTED` |
| `PROVISIONAL` | an experimental or internal clause is added | pending Q97: by default refused (an incompatible change of a stable declaration, [Z2-N-CONTRACT-523]); under Q97 (a), `EXPERIMENTAL` / `INTERNAL` with the conditions of "Provisional changes pending Q97" | `CLS-*`, review |
| `SHIPPED` | a deprecating diff in stage N | `DEPRECATED` (D = N) | `DEP-*`, `SUR-DEPRECATION` |
| `SHIPPED` | removal without deprecation | refused | `SUR-SHIPPED-MISSING` |
| `SHIPPED` | an experimental or internal clause is added | refused | `SUR-CLASS` |
| `DEPRECATED` | the roll of stage D | `REMOVAL-DUE`; the roll records `deprecatedIn` = D and generates its removal ticket | `SUR-DEPRECATION` |
| `DEPRECATED` | the markers are removed (un-deprecation) | `SHIPPED` | report `SUR-UNDEPRECATED`; the reviewer confirms that every migrated caller stays migrated |
| `REMOVAL-DUE` | the removal ticket lands | `REMOVED` (pending listing update) | `SUR-SHIPPED-MISSING` passes because `deprecatedIn` + 1 ≤ MAJOR |
| `REMOVAL-DUE` | the roll of stage D + 1 with the declaration present | refused: the roll's precondition fails | `DEP-OVERDUE` |
| `REMOVED` | the roll | `api_surface.json` entry gains `removedIn` = MAJOR at removal | `SUR-*` |
| `EXPERIMENTAL` | the clause is removed (graduation) | `PROVISIONAL` with its `since` raised to the current stage in the same diff, so it ships at this stage's roll | `CLS-*` |
| `EXPERIMENTAL` | the roll of stage G with the clause present | refused: the roll's precondition fails | `CLS-EXPIRED` |
| `INTERNAL` | any edit with callers migrated | `INTERNAL` | Build, Test |

### Rules for the deprecating diff

1. Add the replacement first, or in the same diff, as a provisional declaration.
2. Add both markers ("Deprecation markers") with D = the current MAJOR and the removal stage D + 1.
3. Migrate every caller in the repository, including tests, examples, tools, benchmarks and engine Luau modules ([Z2-N-CONTRACT-523]); because warnings are errors, a missed caller fails the `Build` step (`CompileFail_Ship_DeprecatedUseIsError` proves the mechanism, `Zenith2/Docs/Design/SHIP/Release.md` → "Compatibility labels"). The Build step compiles `win-debug`, `win-release` and, from Stage 2, `android-arm64`; a caller compiled only in `linux-tsan` (an `Internal/Linux/` source) is proved by `Sanitizers -Preset linux-tsan`, which the diff's category union runs when it changes a header such a source includes, and a caller that relies on behaviour rather than on a signature is confirmed by review.
4. Rewrite the deprecated declaration's body as a call of the replacement, so the replacement's tests cover it; where the semantics differ, the body adapts its arguments and its contract block (or deviation line) states the difference in one sentence.
5. Move the deprecated declaration's own test cases to the replacement (no test may call the deprecated declaration, since the call is a compile error and ForbiddenCheck F15 forbids silencing it).
6. Append the `compat` changelog entry and rewrite the header's dev counter.
7. Edit the owning design document's section in the same diff (Z2-D-085).

### Rules for the removal diff

1. Land in `DEV(D + 1, K)`, from the removal ticket the roll generated (owned by the declaration's owning area, fix version `Z2 Stage <D+1>`, kind `maintenance`).
2. Delete the declaration, its markers, its forwarding body and every remaining reference.
3. Append a `compat` entry with `change: "removed"`.
4. Leave `api_surface.json` unchanged; the next roll writes `removedIn`.

### Deprecations at Stage 7

The programme defines no fix version after `Z2 Stage 7`, so no roll follows the Stage 7 acceptance (ruling MAINT-R1-RULINGS R7). The Stage 7 release step lists the Stage 7 declarations ("Release step"), and after `7.0.0` they follow [Z2-N-CONTRACT-523] like every shipped declaration: a deprecation at MAJOR 7 states `deprecated Z2 Stage 7, use <R>, removed Z2 Stage 8`, the version advances by PATCH alone (`release-fix` and `compat` entries from `RELEASE(7, P)`), and the removal lands in the fix version that the human ruling creating it opens, together with that ruling's roll.

## Deprecation checks

The deprecation family of MAINT checks, run in the `Test` step ("PowerShell check tests" of `Zenith2/Docs/Design/MAINT/Tests.md`). The **scan set** is every tracked `*.h`, `*.hpp`, `*.inl` and `*.cpp` under `Zenith2/Engine/**`, `Zenith2/Player/**` and `Zenith2/Tools/**`, except `Zenith2/Tools/Maintenance/Tests/**` (whose fixtures carry a `.fixture` suffix in any case) and `Zenith2/Tools/Gates/Tests/**`; the compile-fail sources (`*_CompileFail.cpp.in`, among them SHIP's `Zenith2/CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in`, which declares a deprecated function by design) match no scanned extension and lie outside the set. `DEP-MESSAGE`, `DEP-PAIRED`, `DEP-STAGES`, `DEP-OVERDUE`, `DEP-REPLACEMENT` and `DEP-LOGGED` read the root headers of the scan set; `DEP-ONLY-ZENITH2` reads the rest. Further inputs: the engine version header, `engine_changelog.json`, `api_surface.json`.

<!-- elaboration -->
| Rule ID | Rule | Finding text |
|---|---|---|
| DEP-MESSAGE | every `[[deprecated(` occurrence in a root header reads exactly `[[deprecated("use <R>; removed in Z2 Stage <removal-stage>")]]` with R matching the replacement production | `<file>:<line>: DEP-MESSAGE: deprecated attribute does not read "use <Replacement>; removed in Z2 Stage <N>"` |
| DEP-PAIRED | every attribute has its contract text (a block clause or a deviation line within the three lines above), with equal R and removal stage, and every deprecated block clause or deviation line has its attributes | `DEP-PAIRED: <file>:<line>: attribute and contract text disagree (<detail>)` |
| DEP-STAGES | D ≤ MAJOR, removal = D + 1, and D ≥ `min(E, MAJOR)` where E is the block's effective `since` (`max(since, 2)`): a block past its introduction stage is never deprecated before it, and a pulled-forward block (E > MAJOR) is deprecated with D = MAJOR, so deprecation is always reachable | `DEP-STAGES: <name> deprecated in Z2 Stage <D> with removal <R>; the engine is at stage <MAJOR>` |
| DEP-OVERDUE | removal stage ≥ MAJOR | `DEP-OVERDUE: <name> was to be removed in Z2 Stage <R>; the engine is at stage <MAJOR>` |
| DEP-REPLACEMENT | the last identifier of R is declared somewhere other than by the deprecated declaration itself: it is the name of a contract block, or it occurs, outside comments and string literals, on a declaration line of a root header (the declaration line of a function declaration as "Deprecation markers" defines it, or a line beginning `class`, `struct`, `enum` or `using` after its indentation) | `DEP-REPLACEMENT: <name> names replacement <R>, which no source declares` |
| DEP-LOGGED | every deprecation with D = MAJOR of a block listed in `api_surface.json` (a shipped block) appears as a `cpp` `deprecated` item of a `compat` entry of the current MAJOR | `DEP-LOGGED: shipped <name> deprecated in this stage has no changelog item` |
| DEP-ONLY-ZENITH2 | no attribute appears in an `Internal/` or `Tests/` source of the scan set | `DEP-ONLY-ZENITH2: <file>:<line>: deprecation outside a public header` |

The scanner strips `//` and `/* */` comments and string literals before identifier matching, except that it reads `///` contract lines and the attribute's own string literal by position; it never compiles anything, so the check runs in seconds over the whole tree.

## API surface listing

`Zenith2/Tools/Maintenance/api_surface.json` lists every shipped stable C++ block and Luau binding. T4 creates it empty (`shippedThrough` null); after that only the roll writes it ("Release and roll procedures", and the Stage 7 release step); an ordinary ticket never edits it, and the file is a `reviewPaths` file (cross-area note MT3; X386) so an edit outside the roll is reviewed.

### Surface listing schema

<!-- elaboration -->
```json
{
    "schema": "zenith2.api-surface",
    "schemaVersion": 1,
    "comment": "Written by Invoke-Zenith2VersionRoll.ps1 alone; see Zenith2/Docs/Design/MAINT/Versioning.md \"API surface listing\".",
    "engineVersion": "3.0.0-dev.0",
    "shippedThrough": 2,
    "cpp": [
        {
            "library": "ZenithCore",
            "header": "Zenith_Arena.h",
            "kind": "type",
            "name": "Zenith_Arena",
            "since": 2,
            "deprecatedIn": null,
            "replacement": null,
            "removedIn": null
        }
    ],
    "luau": [
        {
            "library": "ZenithPhysics",
            "namespace": "Physics",
            "name": "Raycast",
            "since": 3,
            "deprecatedIn": null,
            "replacement": null,
            "removedIn": null
        }
    ]
}
```

| Member | Rule |
|---|---|
| `engineVersion` | the version the roll wrote (the new `DEV(N + 1, 0)`); after the Stage 7 release step, `7.0.0` |
| `shippedThrough` | the stage the last roll released; `null` before the first roll |
| `cpp[]` | one record per shipped stable contract block, identified by (`library`, `name`) (the header is recorded, not part of the identity), byte-ordered by `library`, `header`, `name`; `kind` is the block's kind keyword (`type`, `value`, `interface`, `container`, `enum`, `functions`) |
| `luau[]` | one record per shipped stable binding function, byte-ordered by `namespace`, `name` |
| `deprecatedIn` | the D of the deprecation the declaration shipped with; written by the roll of stage D |
| `replacement` | its R |
| `removedIn` | the stage in which the removal diff landed; written by the roll that ends that stage; the record stays, so a name is never silently reused |

### Surface listing rules

| Rule ID | Rule |
|---|---|
| SUR-SCHEMA | the file obeys the schema and its ordering |
| SUR-SHIPPED-MISSING | a record with `removedIn: null` whose block is absent from every root header of its library (or whose binding is absent) fails, unless `deprecatedIn` is not null and `deprecatedIn + 1 ≤ MAJOR` (a permitted removal awaiting the roll, reported) |
| SUR-UNLISTED | a stable block with effective `since ≤ shippedThrough` that has no record fails (the block's `since` was lowered, or the roll was skipped) |
| SUR-SINCE | a record's `since` differs from its block's `since` |
| SUR-CLASS | a recorded block now carries an experimental or internal clause, or matches an internal name rule |
| SUR-DEPRECATION | a record with `deprecatedIn: null` whose block is deprecated with D ≠ MAJOR fails (a back-dated or stale deprecation); a record with `deprecatedIn: d` whose block is deprecated with D ≠ d fails |
| SUR-UNDEPRECATED | a record with `deprecatedIn: d` whose block carries no deprecation: report only |
| SUR-REUSED | a record with `removedIn` set whose name is declared again: report only; the reviewer confirms the new declaration is unrelated |
| SUR-MOVED | a record whose block is found in another root header of the same library: report only; the next roll rewrites `header` |
| SUR-LUAU-NOT-AVAILABLE | the roll found no `bindings.json` for some repository game, so `luau[]` was left unchanged: report only (the state of every roll before T10) |

Member-level declarations (a member function, a data member, an enumerator) are not listed: their deprecation is checked by the `DEP` rules and their removal by review, the upgrade guide generator's removed-API scan (`Zenith2/Docs/Design/SHIP/Release.md` → "Upgrade guides") and the build of every in-repo caller.

## Luau API classification

SCRIPT's binding descriptor carries the classification (cross-area note MT2, adopted by SCRIPT, row SC30; `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Binding stability and deprecation"): `Zenith_ScriptFunctionDesc` holds `m_uSinceStage`, `m_eStability` (`Zenith_ScriptApiStability`: `SCRIPT_API_STABILITY_STABLE`, `SCRIPT_API_STABILITY_EXPERIMENTAL`, `SCRIPT_API_STABILITY_INTERNAL`), `m_uExperimentalUntilStage` and `m_xDeprecation` (`Zenith_ScriptDeprecation { m_szReplacement; m_uDeprecatedStage; m_uRemovedStage }`), copied from `AddFunction`'s trailing `Zenith_ScriptFunctionVersion` (default: stable since Z2 Stage 0), and `m_szLibrary`, the registrant library; `Seal` refuses an inconsistent version by rules B1 to B4. The four classification members are inputs of `Zenith_ScriptBindingSetHash`; `m_szLibrary` is not. The behaviours MAINT relies on:

1. The definition emitter writes, directly above a deprecated function's signature in `Zenith.luau`, the line `--- deprecated Z2 Stage <D>, use <R>, removed Z2 Stage <D+1>`, and above an experimental one `--- experimental until Z2 Stage <G>`.
2. The compiler's name-reference walk records the first call on each line whose callee is a deprecated binding; the module manifest carries the records; `LoadModules` records, once per (module, line) per world load, the warning `SCRIPT_BINDING_DEPRECATED_CALL` (ZS1808) with the text `<Module>:<Line>: ZS1808: Zenith.<Namespace>.<Function> is deprecated since Z2 Stage <D>; use <Replacement>; it is removed in Z2 Stage <Removed>` in the world's diagnostic store, as a warning and never a Luau error (`Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Deprecated call sites"). `Test_Script_Binding_DeprecatedCallDiagnosed` asserts it (cross-area row SH14).
3. The `luau-types` step writes `${ZENITH_OUT}/Generated/Luau/<Game>/bindings.json`: `{ "schema": "zenith2.script-binding-listing", "schemaVersion": 1, "bindingSetHash": "0x<16 hex>", "functions": [ { "library", "namespace", "name", "since", "stability", "experimentalUntil", "deprecatedIn", "replacement", "removedIn" } ] }`, one record per function of the sealed table, every stability included, byte-ordered by namespace and name; `library` is `null` for a binding registered with no registrant library (`Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Binding listing").

The Luau classification rules mirror the C++ ones: a binding is shipped when its effective since stage `max(since, 2)` is below MAJOR and its stability is stable; a shipped binding is removed only after one fix version of deprecation; an experimental binding graduates by G; internal bindings are never listed. The `luau[]` section of `api_surface.json` is the union, over every repository game, of the shipped stable functions of its `bindings.json` (every module is selected by at least one repository game, `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Sample game checks", so the union covers every binding). The Luau rules `SUR-*` and `DEP-*` apply with `bindings.json` in place of header text from MAINT T10 on; a roll that finds no listing for some repository game leaves `luau[]` unchanged and reports `SUR-LUAU-NOT-AVAILABLE`.

## Release and roll procedures

Both procedures are steps of the stage acceptance ticket `Z2 S<N> stage acceptance` (PROC, `needs-human`; cross-area note MT3, adopted by PROC's revision under ruling MAINT-R1-RULINGS R5), run by the human in the main checkout with every lane idle (Z2-D-083). Stage 0 and Stage 1 run neither: the engine version header lands in Stage 2.

The release and roll commits change `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h`, and the acceptance commits `Zenith2/Tools/Gates/libgraph_baseline.json`; both are `reviewPaths` files ([Z2-N-CONTRACT-491], [Z2-N-CONTRACT-492]). Before each such commit the human records a review of the staged diff, and no category is exempt (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure", step 10); MAINT adds no step of its own.

### Release step

`pwsh -NoProfile -File Zenith2/Tools/Maintenance/Set-Zenith2EngineRelease.ps1 -Stage <N> -Ticket <acceptance Draft-ID>`:

1. Preconditions (exit 4 with the failing item named): the tree is clean (`git status --porcelain --untracked-files=all` empty); the header state is `DEV(N, K)` for the given N; `Test-Zenith2Maintenance.ps1 -Check All` exits 0.
2. Rewrite the header constants to `N.0.0` (`bZENITH_ENGINE_VERSION_PRERELEASE = false`, `uZENITH_ENGINE_VERSION_DEV = 0`, `szZENITH_ENGINE_VERSION = "N.0.0"`), preserving every other byte.
3. Append the `release` entry (`summary`: `Release of fix version Z2 Stage <N>.`).
3a. At N = 7 alone, run `Update-Zenith2ApiSurface -ShippedThrough 7` (the listing rebuild of roll step 3 without the version change, writing the listing's `engineVersion` as the header's `7.0.0`, not a next development version), because no roll follows ("Deprecations at Stage 7").
4. Print the two changed paths; the human commits them as `MAINT: release engine N.0.0`, then runs the acceptance suites.

### Release fixes during the acceptance

A fix that lands while the header is `N.0.P` rewrites the header to `N.0.(P+1)` and appends a `release-fix` entry naming its ticket (Draft-ID or board key), in its own diff, when the diff falls in the release-fix scope of "Version state machine".

### Roll step

`pwsh -NoProfile -File Zenith2/Tools/Maintenance/Invoke-Zenith2VersionRoll.ps1 -Stage <N> -Ticket <acceptance Draft-ID> [-PlanOut <path>]`, the last step of the acceptance:

1. Preconditions (exit 4 naming each failure): the tree is clean; the state is `RELEASE(N, P)`; N ≤ 6; `Test-Zenith2Maintenance.ps1 -Acceptance -Stage N` exits 0 (every acceptance check of `Zenith2/Docs/Design/MAINT/Tests.md` → "Acceptance checks", including no deprecation with removal stage ≤ N and no experimental clause with G ≤ N).
2. Record `releasedCommit` = `git rev-parse HEAD`.
3. Rebuild `api_surface.json` (`Update-Zenith2ApiSurface.ps1 -ShippedThrough <N>`): for each stable block of the tree with effective `since ≤ N`, keep or add its record (copying the effective `since`; a moved block keeps its record and gets its new `header`); set `deprecatedIn`/`replacement` from its markers where `deprecatedIn` is null and D = N; for each record whose block is absent from its library and `removedIn` is null, set `removedIn = N`; when every repository game has `${ZENITH_OUT}/Generated/Luau/<Game>/bindings.json`, apply the same to `luau[]` from their union, else leave `luau[]` unchanged and print the report finding `SUR-LUAU-NOT-AVAILABLE`; set `shippedThrough = N` and `engineVersion = "<N+1>.0.0-dev.0"`.
4. Rewrite the header to `<N+1>.0.0-dev.0`.
5. Append the `roll` entry with `released: { "version": "N.0.P", "commit": "<releasedCommit>" }`.
6. Write the plan file (default `Zenith2/Docs/Research/Maintenance/stage-<N+1>-plan.json`): `{ "schema": "zenith2.maintenance-plan", "schemaVersion": 1, "stage": <N+1>, "removals": [ { "surface", "library", "header", "name", "replacement", "owner", "epic" } ], "graduations": [ { "header", "name", "until", "owner", "epic" } ], "rehearsals": [ … ] }`: one removal per declaration with D = N (removal due in N + 1), one graduation per experimental block with G = N + 1, and the rehearsal plan of `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal plan"; `owner` and `epic` come from `Get-Zenith2Owner` on the header path.
7. Run `Test-Zenith2Maintenance.ps1 -Check All` on the result (exit 4 and every changed file restored from `git` when it fails).
8. Print the changed paths; the human commits them as `MAINT: roll engine to <N+1>.0.0-dev.0`, then files one `maintenance` TASK per plan removal and graduation under its `epic`, and the rehearsal TASKs of the plan under each entry's owner epic (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Rehearsal plan"), fix version `Z2 Stage <N+1>`, before closing the acceptance ticket.

Each rewrite is written to `<path>.z2tmp` beside the target, flushed, and renamed over the target; on any failure after the first rename the script restores every target with `git checkout -- <path>` and exits 4, so an interrupted roll leaves either the old files or the new ones.
