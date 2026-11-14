# SHIP design — the CMake helpers

Companion to `Zenith2/Docs/Design/SHIP.md`. `Zenith2/CMake/` is SHIP's directory (`Zenith2/Docs/Architecture.md` → "Repository layout", table "Ownership per directory"). The Stage 0 scaffold ticket (Z2T-MOD-056, MOD epic) lands the first version of every helper (`Zenith2/Docs/Design/MOD.md` → "Stage 0 scaffold"); from then on every helper except `ZenithAddGame.cmake` and the CMake fixture tests is SHIP's (`Zenith2/Docs/Design/MOD.md` → "CMake helper files owned by MOD"). This file specifies what SHIP adds after the scaffold: the `THIRD_PARTY` behaviour in full, the two integration modes the scaffold defers, target mapping, pin header templates, runtime file staging, the parts of `zenith_add_library` the scaffold does not carry, the flag rows (including the assert define), the toolchain files and the generic test-registry functions. `Zenith2/Docs/Architecture/BuildSystem.md` is authoritative for every rule it already states (core-document patch c of 2026-10-03 applied cross-document items X220 in part, X221, X222, X203 and X231 to it, and core-document patch d, applied 2026-10-09, X215, X216 and X262, which this revision conforms to); additions are marked `<!-- elaboration -->` and listed, with the cross-document item that carries or carried their core wording, in `Zenith2/Docs/Design/SHIP.md` → "Reconciliations with the core documents".

## Scope of this file

| Section | Specifies |
|---|---|
| "Ownership of the CMake directory" | which file is whose |
| "zenith_begin_build and zenith_end_build" | the global flag hygiene, the manifest validation call, the end-of-build outputs SHIP adds |
| "THIRD_PARTY behaviour" | name resolution, link and include rules, shipping exclusion, runtime staging, pin header templates |
| "ZenithThirdParty.cmake" | every function, every mode, target mapping, exposed variables |
| "zenith_add_library completion" | `TEST_THIRD_PARTY`, the per-preset `TEST_DEPENDS` links, the discovery labels, the conformance executables, the fuzz and compile-fail hand-off, the test environment, the `ShippingTest` configuration |
| "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" | the sibling helpers' `THIRD_PARTY`, test support and runtime staging |
| "zenith_apply_flags" | the assert define, the third-party rows, the reproducibility and hardening rows |
| "Toolchain files" | `msvc-x64.cmake`, `clang-linux-x64.cmake`, the Android checks |
| "Registry functions" | the registration forms SHIP adopts (CORE's block functions, MOD's fuzz rule), SHIP's own functions and the registry tail |
| "declared_deps additions" | the record fields SHIP adds |

## Ownership of the CMake directory

| File | Owner | Content specified by |
|---|---|---|
| `Zenith2/CMake/ZenithBuild.cmake` | SHIP | `Zenith2/Docs/Architecture/BuildSystem.md` → "Top-level tree and helper modules"; § "zenith_begin_build and zenith_end_build" |
| `Zenith2/CMake/ZenithAddLibrary.cmake` | SHIP | BuildSystem → "The zenith_add_library helper"; § "zenith_add_library completion"; § "Registry functions" |
| `Zenith2/CMake/ZenithAddTool.cmake` | SHIP | BuildSystem → "Tools, benchmarks and integration suites"; § "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests" |
| `Zenith2/CMake/ZenithWarnings.cmake` | SHIP | the warning sets of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (subsection "Compiler warning flags"); SHIP changes it only when that subsection changes |
| `Zenith2/CMake/ZenithFlags.cmake` | SHIP | BuildSystem → "Compile and link flags"; § "zenith_apply_flags" |
| `Zenith2/CMake/ZenithSupported.cmake` | SHIP | BuildSystem → "The ZENITH_SUPPORTED matrix" |
| `Zenith2/CMake/ZenithThirdParty.cmake` | SHIP | BuildSystem → "Third-party integration"; `Zenith2/Docs/Design/SHIP/Manifest.md`; § "ZenithThirdParty.cmake" |
| `Zenith2/CMake/toolchains/msvc-x64.cmake`, `clang-linux-x64.cmake` | SHIP | § "Toolchain files" |
| `Zenith2/CMake/ZenithAddGame.cmake` | MOD | `Zenith2/Docs/Design/MOD/GameBuild.md`; it calls SHIP's `zenith_stage_runtime_files` (§ "Runtime file staging") |
| `Zenith2/CMake/ZenithSecurity.cmake` | SEC | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Key resolution at configure"; it defines only `zenith_security_resolve_update_key` (SEC row SEC6 (d)) |
| `Zenith2/CMake/ZenithShaders.cmake` | RPROD (stream-set wiring: GEOM) | `Zenith2/Docs/Design/RPROD.md`, `Zenith2/Docs/Design/GEOM.md` |
| `Zenith2/CMake/ZenithBudgetTable.cmake` | CORE (content) | `Zenith2/Docs/Design/CORE.md` (the generator of the memory-budget header); SHIP reviews only its placement |
| `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` and MOD's runners and fixtures | MOD (file); every area appends its block | `Zenith2/Docs/Design/MOD/GameBuild.md` → "CMake fixture tests" |
| the SHIP block of `ZenithCMakeTests.cmake`, its one tail call `_zenith_ship_registry_tail()`, `Zenith2/CMake/Tests/RunShipFixture.cmake`, `Zenith2/CMake/Tests/DiscoverDeathCases.cmake`, `Zenith2/CMake/Tests/Ship/**` | SHIP | `Zenith2/Docs/Design/SHIP/Tests.md`; § "Registry tail" |

No new helper module is added by SHIP: the registry functions and the registry tail live in `ZenithAddLibrary.cmake`, the manifest validator in `ZenithThirdParty.cmake`, and the one new script, `DiscoverDeathCases.cmake`, is a test-registry script under `CMake/Tests/` like MOD's runners, so BuildSystem's helper-module table (which lists `CMake/ZenithBudgetTable.cmake` since cross-document item X220 item 10) stays complete.

## zenith_begin_build and zenith_end_build

### Global flag hygiene

<!-- elaboration -->
`zenith_begin_build()` runs after `project()` and before `add_subdirectory(ThirdParty)`, and performs, in order:

1. The checkout-hash check of [Z2-N-ARCH-158].
2. The compiler pin checks of [Z2-N-ARCH-194]: on an Android preset, `ANDROID_NDK_REVISION` must equal the manifest's `toolchain.ndk.pkgRevision`, else `FATAL_ERROR "Zenith2: NDK <found> does not match the pinned <pinned> in ThirdParty/manifest.json"`; on `win` and `win-asan`, the first three dot-separated components of `CMAKE_CXX_COMPILER_VERSION` (MSVC reports four, `19.xx.yyyyy.z`) must equal `toolchain.msvc.clVersion`; on `linux-tsan`, `CMAKE_CXX_COMPILER_VERSION` (three components) must equal the `llvm` pin's `version`; a mismatch stops with `FATAL_ERROR "Zenith2: compiler <found> does not match the pinned <pinned> in ThirdParty/manifest.json"`. The comparison runs here because a toolchain file runs before the compiler is identified.
3. The manifest validation: `zenith_manifest_validate` over `ThirdParty/manifest.json`, rules V1 to V40 (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules"; the ownership rules K1 to K5 run in `zenith_end_build`); the file is added to `CMAKE_CONFIGURE_DEPENDS`.
4. Flag hygiene, so that every flag in a compile group comes from `zenith_apply_flags` or the configuration variables below and nothing else:
   - from `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS`, remove the tokens `/EHsc`, `/EHs`, `/GR`, `/W3`, `-fexceptions`, `-frtti` wherever the platform modules or the NDK toolchain put them ([Z2-N-SCRIPT-027]);
   - set the per-configuration variables of every configuration in `CMAKE_CONFIGURATION_TYPES` to exactly the values of the table below (CMake has no defaults for `Shipping`, `ShippingTest`, `Sanitize` and `Hwasan`, so without this the third-party targets would build those configurations unoptimised);
   - `set(CMAKE_MAP_IMPORTED_CONFIG_SHIPPING Release)`, `…_SHIPPINGTEST Release`, `…_SANITIZE Release`, `…_HWASAN Release`, so an imported target with only Debug and Release locations (a prebuilt import library) resolves (X216, applied).

| Variable | MSVC (`win`, `win-asan`) | clang (Android, Linux) |
|---|---|---|
| `CMAKE_<LANG>_FLAGS_DEBUG` | `/Ob0 /Od` | `-O0` |
| `CMAKE_<LANG>_FLAGS_RELEASE` | `/O2 /Ob2 /DNDEBUG` | `-O2 -DNDEBUG` |
| `CMAKE_<LANG>_FLAGS_SHIPPING` | `/O2 /Ob2 /DNDEBUG` | `-O2 -DNDEBUG` |
| `CMAKE_<LANG>_FLAGS_SHIPPINGTEST` | `/O2 /Ob2 /DNDEBUG` | — (the configuration exists in the `win` tree alone) |
| `CMAKE_<LANG>_FLAGS_SANITIZE` | `/O1 /Oy-` | `-O1 -fno-omit-frame-pointer` |
| `CMAKE_<LANG>_FLAGS_HWASAN` | — | `-O1 -fno-omit-frame-pointer` |
| `CMAKE_{EXE,SHARED,MODULE}_LINKER_FLAGS_<CONFIG>` | empty (the link rows come from `zenith_apply_flags`) | empty |

The table equals the per-configuration variables of `Zenith2/Docs/Architecture/BuildSystem.md` → "Third-party targets and configuration variables" (cross-document item X221, applied), which is authoritative; the values enter every Zenith2 and third-party compile group ([Z2-N-ARCH-230]). `/RTC1` and the `/Zi` CMake adds to `_DEBUG` are absent from the table on purpose: `/RTC1` belongs to the Zenith2 `Debug` row of BuildSystem's MSVC table alone, and debug information is `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` ([Z2-N-ARCH-187]).

### End-of-build outputs

`zenith_end_build()` writes `declared_deps.json` and creates `zenith_host_tools` and `zenith_players` ([Z2-N-ARCH-191]); it writes `declared_deps.json` after the scaffold's `_zenith_include_cmake_tests()` has returned, so the registry's tool-side fuzz declarations are in its `toolFuzzTargets[]` (§ "Tool-side fuzz targets"; SEC row SEC6 (c)). SHIP adds, in this order:

- the ownership rules K1 to K5 over the declaration store (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules"), every failure printed, then one `FATAL_ERROR` with the count; an owner not yet declared is reported at `STATUS` and written to `${ZENITH_OUT}/third_party_pending_owners.json` (written only when its bytes change, `[]` when none), never refused;
- the Luau closure check of § "Target mapping and host executables", and the check that no target outside an entry links one of that entry's unlisted targets;
- the `TEST_DEPENDS` links and `ZENITH_TEST_LINKS_<TARGET>` definitions of every `<Name>_Tests` (§ "Test third-party links"), made here because a `TEST_DEPENDS` target may live in a directory that the byte-ordered walk adds after the library's;
- the conformance executables (§ "Conformance executables"), each appended, as it is created, to the global property `ZENITH_CONFORMANCE_TARGETS` that the registry tail enumerates, created **before** the scaffold's `_zenith_include_cmake_tests()` includes the registry, so a registry block can register a death case on `<Api>_Conformance` and the registry tail (§ "Registry tail"), which runs inside the registry, finds every test and conformance executable already created;
- the `zenith_tests` utility target ([Z2-N-ARCH-191], X216): it depends on every test, conformance and integration-suite target supported in the preset, and is the one target of the `win-shipping-test` build preset (§ "ShippingTest configuration");
- `zenith_host_tools` also depends on every third-party target whose manifest `targets[]` element carries `hostExecutable` (`luau-analyze`), so `win-tools` builds it into `ZENITH_HOST_TOOLS_DIR`;
- `${ZENITH_OUT}/third_party_targets.json`: one record per created third-party target `{ "entry", "target", "owner", "usage", "shipping", "exceptions", "runtimeFiles" }`, sorted by `target`, written only when its bytes change, read by the licence inventory (`Zenith2/Docs/Design/SHIP/Release.md` → "Inventory selection algorithm") and offered to libgraph's third-party ownership rule (cross-area row SH6);
- the configure report line `Zenith2: third party: <n> entries, <m> targets, <k> skipped for <os>` at `STATUS`.

## THIRD_PARTY behaviour

### Name resolution

`THIRD_PARTY <ManifestName>...` on `zenith_add_library`, `zenith_add_tool`, `zenith_add_benchmark` or `zenith_add_integration_tests`, and `TEST_THIRD_PARTY <ManifestName>...` on `zenith_add_library` and `zenith_add_tool`, resolve each name by these rules, in order; the first failing rule stops the configure:

| # | Rule | Failure |
|---|---|---|
| 1 | the name is a manifest entry, spelt exactly (lower-case kebab case, V9; no case folding) | `zenith_add_library(<Lib>): THIRD_PARTY <Name> is not a ThirdParty/manifest.json dependency of this preset` (the scaffold's message, kept) |
| 2 | the entry's `platforms` contains the preset's platform. For `zenith_add_library` a failure is the rule-1 message (an entry skipped for this OS created no target, and a library is created only on platforms its dependencies support). For `zenith_add_benchmark`, `zenith_add_tool` and `zenith_add_integration_tests`, whose one `THIRD_PARTY` list serves every platform of their `SUPPORTED` matrix, the name is **skipped** on this preset instead: nothing is linked, the target is created, and the skip is printed at `STATUS` (`Zenith2: <Target>: THIRD_PARTY <Name> skipped on <preset> (entry platforms <list>)`) | rule-1 message (libraries only) |
| 3 | the naming target is permitted: it is the entry's `owner` (or an element of the owner array, or the benchmark target the owner names, `Zenith_Bench_<Name>` being the target `zenith_add_benchmark(<Name>)` creates); or it is a test, benchmark or tool target ([Z2-N-ARCH-200]); or the owner is `benchmark` and the target is a benchmark. A declared owner and an owner not declared yet are treated alike: rule 3 compares names only | `zenith_add_library(<Lib>): THIRD_PARTY <Name> is owned by <owner>; only the owner, test, benchmark and tool targets may name it` |
| 4 | the usage agrees: `THIRD_PARTY` on a library resolves to `runtime` targets and to the `tools` targets that library owns (K2 admits a library as a `tools` owner: `ZenithScriptCompiler` owns `Luau.Ast` and `Luau.Compiler`; `Libgraph`'s Shipping closure check, [Z2-N-ARCH-240], keeps such a library out of every Player); `TEST_THIRD_PARTY` names a `tests` entry; a tool names `tools` or `runtime` entries; a benchmark names an entry of any usage (`runtime` and `tools` entries it measures, `tests` entries for its own infrastructure and the selection candidates); an integration suite names `runtime` and `tests` entries | `…: THIRD_PARTY <Name> has usage <usage>, which <kind> target <Lib> may not link` |
| 5 | the entry owns at least one target for the naming library (`targets[].owner`), or has no `targets` | `…: <Name> has no target owned by <Lib>` |
| 6 | the entry is not `owner: "test-runs"` or `owner: "host"` (nothing links those) | `…: <Name> is a <owner> entry and is linked by nothing` |

The resolved target list is the entry's targets owned by the naming library; a test, benchmark or tool target naming an entry it does not own gets every `runtime` and `tools` target of the entry except `host` targets. In every case only the `targets[]` elements whose `platforms` contain the preset's platform are considered.

Rule 2's skip is what lets one multi-platform benchmark name a Windows-only entry: `zenith_add_benchmark(BindlessProbe SUPPORTED win android-arm64 THIRD_PARTY vulkan-headers volk d3d12-agility-sdk …)` links `d3d12-agility-sdk` on `win` and skips it on `android-arm64`, where the probe's D3D12 code lives under `Internal/Windows/` and is not compiled. A name absent from the manifest (rule 1) still fails on every preset, so a skip never hides a misspelling.

**Unavailable entries.** A name that passes rules 1 to 6 but whose entry is an unavailable manual download (`ZENITH_TP_<ID>_AVAILABLE` is `0`, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Manual-download items") makes `zenith_add_library` create nothing in this configure, as a preset outside its `SUPPORTED` does: the declaration is already recorded (`ZENITH_DECL_<Lib>_*`, before the `SUPPORTED` check), the configure prints `Zenith2: <Lib>: not created: THIRD_PARTY <Name> is a manual download not in the dependency cache`, and the library's `declared_deps.json` record carries `unavailableThirdParty: ["<Name>"]`. A target that depends on it is then absent by [Z2-N-ARCH-172]'s rule for an absent library, and a game composition leaves an unavailable backend out of its backend list (ONLINE's design, MOD's `zenith_add_game`). `zenith_add_tool`, `zenith_add_benchmark` and `zenith_add_integration_tests` refuse such a name (`…: THIRD_PARTY <Name> is a manual download; only a library may name it`), so nothing but the backend library that owns a partner-gated SDK depends on it.

### Link and include rules

For each resolved target `T` of library `L` (`_zenith_library_link_third_party(<L> <Name>...)`, the scaffold's function, extended):

| Integration | Link | Include |
|---|---|---|
| `cmake-subproject`, `zenith-wrapper` | `target_link_libraries(<L> PRIVATE <T>)` | `target_include_directories(<L> SYSTEM PRIVATE $<TARGET_PROPERTY:<T>,INTERFACE_INCLUDE_DIRECTORIES>)` |
| `header-only`, `prebuilt` | `target_link_libraries(<L> PRIVATE <name>)` (the `INTERFACE` target) | through the `INTERFACE` target, marked `SYSTEM` |
| `host-tool` | the `INTERFACE` target when the entry has `link` libraries (ZenithShaderCompiler links `slang`); the executable through `$<TARGET_FILE:<name>::<stem>>` in custom commands only | through the `INTERFACE` target |

- `PRIVATE` is the rule for every library: no third-party target propagates to a dependant ([Z2-N-ARCH-122]), so every library that needs a dependency names it itself.
- A static library's `PRIVATE` link still appears in its link interface as `$<LINK_ONLY:<T>>` (CMake's rule for static libraries), so the final executable links `T` without seeing its headers.
- `TEST_THIRD_PARTY` links into `<L>_Tests`, `<L>`'s conformance executable and its fuzz targets only, never into `<L>` itself, and is recorded as `testThirdParty` (§ "declared_deps additions").
- `autoLink` entries (`doctest` for every test, conformance, fuzz and integration target; `nanobench` for every benchmark) are linked by the target-creation steps, not by `THIRD_PARTY`.

### Shipping-excluded dependencies

<!-- elaboration -->
For an entry with `shipping: "excluded"` the link of rule "Link and include rules" is wrapped: `target_link_libraries(<L> PRIVATE $<$<NOT:$<CONFIG:Shipping,ShippingTest>>:<T>>)` (BuildSystem's `shipping` row, X216 applied: `ShippingTest` links what `Shipping` links). The include directory stays unconditional, because the owner's sources compile in every configuration and reach the dependency only inside `if constexpr (!Zenith_IsShippingBuild())`. For a compiled entry (`tracy`) a Shipping link that still references a symbol of `T` fails with an unresolved external, which is the intended failure (`Zenith2/Docs/Design/SHIP/Release.md` → "Development services in Shipping"). For a `header-only` entry (`renderdoc-app`) the wrap removes nothing, because `T` is an `INTERFACE` target without objects; there the guarantee is the owner's `if constexpr` entry plus the forbidden-symbol checks (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Shipping exclusion field"). `CMake_Ship_ShippingExcludedLinkIsConditional` asserts the recorded command.

### Runtime file staging

<!-- elaboration -->
`zenith_stage_runtime_files(<ExecutableTarget> [TEST])` copies the runtime files of the prebuilt entries an executable needs next to it:

1. Collect the executable's declared closure: its own `THIRD_PARTY` entries plus, for every library in the transitive closure of its declared `DEPENDS` (read from the declaration store `ZENITH_DECL_<Lib>_DEPENDS` and `ZENITH_DECL_<Lib>_THIRD_PARTY`), that library's entries. `ZENITH_DECL_<Lib>_THIRD_PARTY` is SHIP's addition to the store: SHIP's `zenith_add_library` completion (T3) records the library's `THIRD_PARTY` names, byte-ordered, as that global property before its `SUPPORTED` check, beside the scaffold's `ZENITH_DECL_<Lib>_TEST_THIRD_PARTY` (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Inputs and preconditions" lists the store; cross-area row SH20 asks MOD to add the row there).
2. Keep the entries whose `prebuilt` item for the preset's ABI (or whose `fromToolchain` layout) has a non-empty `runtime` list; with `TEST`, add every `owner: "test-runs"` entry of the platform.
3. For each kept entry, add `add_custom_command(TARGET <ExecutableTarget> POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:<Exe>>/<runtimeSubdir> COMMAND ${CMAKE_COMMAND} -E copy_if_different <files> $<TARGET_FILE_DIR:<Exe>>/<runtimeSubdir>/ COMMAND_EXPAND_LISTS VERBATIM)`, where `<files>` is `$<IF:$<CONFIG:Shipping,ShippingTest>,<pins.shippingRuntime>,<runtime>>` when the entry has `pins.shippingRuntime`, otherwise `<runtime>` (a `ShippingTest` test executable stages the shipped files, so a Shipping-behaviour test runs against `D3D12Core.dll` alone); every path is absolute under the entry's cache directory.
4. Record the staged files on the target as the property `ZENITH_RUNTIME_FILES` (list of `<subdir>/<file name>`), which the packaging step reads (`Zenith2/Docs/Design/SHIP/Release.md` → "Windows package").

`zenith_add_library`, `zenith_add_tool`, `zenith_add_benchmark` and `zenith_add_integration_tests` call it with `TEST` for every test, conformance, fuzz, benchmark and integration executable they create; `zenith_add_game` (MOD) calls it without `TEST` for the game executable (cross-area row SH2). On Android no entry has runtime files, so the function adds nothing there.

### Pin header templates

<!-- elaboration -->
Answers SCRIPT's "manifest reader for the generated pin header" (cross-area row SB15) and RHI's Agility version constant without a second command in a library's `CMakeLists.txt` ([Z2-N-ARCH-157]):

1. `zenith_add_library` and `zenith_add_tool` glob `*.generated.h.in` under the target's directory (outside `Tests/`) with `CONFIGURE_DEPENDS`; the `*.h` glob never matches these files.
2. For each template `<dir>/<rel>/<Stem>.generated.h.in`, the helper defines one CMake variable per scalar field of every manifest entry the target names in `THIRD_PARTY` (and only those), then runs `configure_file(<template> ${ZENITH_OUT}/generated/<Target>/<Target>/<rel>/<Stem>.generated.h @ONLY)`; `configure_file` rewrites the output only when its bytes change.
3. The variables, for an entry with ID `<ID>` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Integration modes in detail"):

| Variable | Value |
|---|---|
| `ZENITH_TP_<ID>_VERSION` | `version` |
| `ZENITH_TP_<ID>_SOURCE_SHA256` | `source.sha256` (or the toolchain archive's) |
| `ZENITH_TP_<ID>_<PIN>` | every `pins` scalar, with `<PIN>` the key converted from camelCase to upper snake case (`bytecodeMin` → `BYTECODE_MIN`, `sdkVersion` → `SDK_VERSION`); a one-level object contributes `<PIN>_<SUBKEY>` (`nativePolicy.windows` → `NATIVE_POLICY_WINDOWS`); booleans become `1` or `0`; arrays become a comma-separated list of double-quoted C string literals |
| `ZENITH_TP_<ID>_<PIN>_FIRST8_LE` | for every `pins` string of at least 16 lower-case hex digits: `0x<b7><b6>…<b0>ull`, the first eight bytes read as a little-endian `uint64` (the Luau pin hash of [Z2-N-SCRIPT-501]) |

4. The template uses `@VAR@` only; an `@ZENITH_TP_…@` reference to a variable the helper did not define is a configure error (`zenith_add_library(<Lib>): <template>: @<VAR>@ is not defined by the THIRD_PARTY entries of <Lib>`), detected by scanning the template before `configure_file`.
5. The output directory `${ZENITH_OUT}/generated/<Target>` is a `PRIVATE` include directory of the target and of its test targets; the generated file is recorded in `declared_deps.json` → `generated` attributed to the target ([Z2-N-ARCH-184]), so libgraph sees it as the target's own private header. An include reads `#include "ZenithScript/Internal/Zenith_ScriptLuauPin.generated.h"`.

`CMake_Ship_PinTemplateLuau` and `CMake_Ship_PinTemplateUndefinedVariable` cover it. Because the helper configures every template automatically, a library never calls `configure_file` itself (its `CMakeLists.txt` is the one helper call of [Z2-N-ARCH-157]); SCRIPT's pin-record ticket (Z2T-SCRIPT-106) owes nothing: its "`configure_file` in the ZenithScript CMake through the manifest reader the MOD helper exposes" is satisfied by this automatic configure, so its diff adds `Internal/Zenith_ScriptLuauPin.generated.h.in` and writes no `configure_file` line (cross-area row SH13). The native policies reach the template as `@ZENITH_TP_LUAU_NATIVE_POLICY_WINDOWS@` and `@ZENITH_TP_LUAU_NATIVE_POLICY_ANDROID@`, the manifest's string values (`disabled`, `toolsOnly`, `all`); the enumerator index of `eSCRIPT_LUAU_NATIVE_POLICY_WINDOWS` and `eSCRIPT_LUAU_NATIVE_POLICY_ANDROID` is the template's own `constexpr` mapping of that string. The `luau` entry's ID is `LUAU`, so the template variables keep their names (`@ZENITH_TP_LUAU_BYTECODE_MIN@`).

## ZenithThirdParty.cmake

### zenith_third_party_add_all

`zenith_third_party_add_all()` (the one call of `Zenith2/ThirdParty/CMakeLists.txt`) performs:

1. Read and validate the manifest (§ "Global flag hygiene" step 3 already validated it; the walk re-reads the file for its own parse). SHIP's walk replaces the scaffold's Stage 0 reader: its field check is the validator's (V11 and V14), so a `fromToolchain` entry without `source` is valid, where the Stage 0 reader stopped on a missing `source.url`.
2. Order the entries: the manifest order (byte order of `name`), with every entry moved after its `dependsOn` entries (a stable topological sort, ties kept in manifest order).
3. For each entry whose `platforms` contains the preset's platform: resolve its cache directory, check its stamp (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Stamp check at configure"), create its targets by its mode (below), enumerate every target the entry created (for a `cmake-subproject` or `zenith-wrapper` entry, the `BUILDSYSTEM_TARGETS` of its binary directory and, recursively, of every directory in that directory's `SUBDIRECTORIES` property, so the targets of nested upstream directories such as KTX-Software's, Jolt's and SDL3's are included), apply the third-party flag rows to every enumerated static, object, shared, module and executable target (`zenith_apply_flags(<t> THIRD_PARTY [EXCEPTIONS])`; an `INTERFACE` or `IMPORTED` target receives no compile row, because rows on it could only be `INTERFACE` options that would reach every Zenith2 consumer's compile group), check the `targets` list against the enumerated targets (§ "Target mapping and host executables"), and record each target in `third_party_targets.json`.
4. Entries for other platforms are skipped silently and counted in the configure report line.

Every target-creating command goes through the scaffold's recorder `_zenith_third_party_command`, so every mode is covered by script-mode fixtures.

An entry of the preset's platform creates no target, and the walk continues, in exactly one case: a `zenith-wrapper` entry whose `ThirdParty/<name>/CMakeLists.txt` does not exist yet (the state between its pinning ticket and the owner ticket that adds the wrapper file, `zlib` before Z2T-COMPRESS-007; `Zenith2/Docs/Design/SHIP.md` → "Pin landing order"). The walk prints `Zenith2: <name> <version>: ThirdParty/<name>/CMakeLists.txt is not present yet; no target created` at `STATUS`, writes no `third_party_targets.json` record for it (it created no target), and a `THIRD_PARTY <name>` that reaches it fails rule 1 of § "Name resolution" with the scaffold's message, so a library can never link a missing wrapper. The scaffold's `FATAL_ERROR` for the same state is the Stage 0 behaviour this replaces.

### Mode cmake-subproject

The scaffold's behaviour (every key of `options.common`, then of `options.<platform>`, set as a forced `STRING` cache variable; `add_subdirectory(<cache>[/<cmakeSourceDir>] ${CMAKE_BINARY_DIR}/ThirdParty/<name> EXCLUDE_FROM_ALL)`), plus:

- before `add_subdirectory`, every option value's `@ZENITH_TP_<ID>_DIR@` token is replaced by the named `dependsOn` entry's cache directory, and a token naming an entry absent from `dependsOn` is a configure error;
- after it, every `definitions` pair is added with `target_compile_definitions(<t> PRIVATE <KEY>=<value>)` to every target of the entry;
- `CMAKE_POLICY_DEFAULT_CMP0077` is `NEW` for the duration of the call, so `option()` in the subproject honours the cache variables set from `options`;
- the `targets[].exceptions` targets receive the exceptions-on row (§ "Third-party flag rows").

### Mode zenith-wrapper

The scaffold's behaviour (`set(ZENITH_THIRD_PARTY_SOURCE_DIR <cache>)` then `add_subdirectory(${ZENITH_ROOT}/ThirdParty/<name> ${CMAKE_BINARY_DIR}/ThirdParty/<name>)`), plus `definitions` as above, and the absent-wrapper rule of § "zenith_third_party_add_all" in place of the scaffold's refusal. A wrapper's `CMakeLists.txt` creates `STATIC` libraries only, globs its sources from `ZENITH_THIRD_PARTY_SOURCE_DIR` by an explicit list of file names (the wrapper is the one place in the tree where a source list is written, because it describes an upstream tree, not a Zenith2 directory), and calls no Zenith2 helper.

### Mode header-only

The scaffold's behaviour (`add_library(<name> INTERFACE)`, `target_include_directories(<name> SYSTEM INTERFACE <cache>)`), with the include directory `<cache>/<pins.includeDir>` when `pins.includeDir` is present.

### Mode prebuilt

<!-- elaboration -->
Lands in SHIP's Stage 1 ticket T4 (`Zenith2/Docs/Design/SHIP.md` → "Stage 1"), before the RHI pinning ticket that adds its first entries (the Agility SDK, WARP, lavapipe, the validation layer), which is `blocked_by` T4 (Z2T-MOD-056 defers the mode to this point):

1. Select the `prebuilt` item whose `abi` matches the preset (`win-x64` for `win` and `win-asan`; `android-arm64-v8a`; `android-x86_64`; `linux-x64`); none is `FATAL_ERROR "Zenith2: <name>: no prebuilt item for ABI <abi>"`.
2. Resolve its directory: `<cache>/<name>/<abi>-<sha256>` for an own archive, or the toolchain install directory for `fromToolchain`.
3. `add_library(<name> INTERFACE)`; `target_include_directories(<name> SYSTEM INTERFACE <dir>/<include>…)`; `target_link_libraries(<name> INTERFACE <dir>/<link>… <dir>/<importLibrary>…)`. An item with `manualDownload` whose directory is absent stops at this step: no target, `ZENITH_TP_<ID>_AVAILABLE` `0`, and the unavailable line of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Manual-download items".
4. Set the target properties `ZENITH_RUNTIME_FILES` (absolute paths of `runtime`), `ZENITH_RUNTIME_SUBDIR` and, when present, `ZENITH_SHIPPING_RUNTIME_FILES`; define `ZENITH_TP_<ID>_RUNTIME_FILES` as the absolute list.
5. In a sanitizer preset (`ZENITH_SANITIZER` set), an entry with a non-empty `link` named by a runtime target is the configure error of [Z2-N-ARCH-201]; `sanitizerExempt` entries (empty `link`) are admitted ([Z2-N-ARCH-234]), and so is an `importLibrary`, which carries no compiled code of the dependency: the DLL it names runs uninstrumented, as a system DLL does (cross-document item X457).

### Mode host-tool

<!-- elaboration -->
Lands in the same Stage 1 ticket T4 as "Mode prebuilt", before the Slang and DXC pinning ticket (`blocked_by` T4), so `slang::slangc` exists for the Stage 1 bindless probe (Z2T-RHI-001) long before its owner `ZenithShaderCompiler` (Z2T-RPROD-032, Stage 2) is declared:

1. On a non-Windows preset the entry is skipped (host tools exist on the `win` host only, [Z2-N-ARCH-192]).
2. Resolve the directory as in "Mode prebuilt".
3. For each `executables` element `<path>`: `add_executable(<name>::<stem> IMPORTED GLOBAL)` with `IMPORTED_LOCATION <dir>/<path>`; define `ZENITH_TP_<ID>_EXE_<STEM>` (stem upper-cased, `slangc` → `SLANGC`).
4. When `link` is non-empty, also `add_library(<name> INTERFACE)` with the include and link lists, as "Mode prebuilt"; runtime files are staged next to the tool executable that links it (`ZenithShaderCompiler.exe` receives `slang.dll`, `slang-glslang.dll`, `dxcompiler.dll` and `dxil.dll` through `zenith_stage_runtime_files`), never next to a runtime executable.
5. A custom command that runs the tool names `<name>::<stem>` in `COMMAND` and the tool file in `DEPENDS`, so a pin change re-runs it ([Z2-N-ARCH-190]).

### Target mapping and host executables

- After a `cmake-subproject` or `zenith-wrapper` entry is added, each `targets[].target` whose element `platforms` contain the preset's platform must exist (`if(NOT TARGET …)` → `FATAL_ERROR "Zenith2: <name>: target <target> listed in targets does not exist"`); an element for other platforms is not checked (`Luau.Analyze.CLI` on `android-arm64` and `linux-tsan`).
- For the `luau` entry, the **closure rule** keeps the exception-enabled set equal to `Zenith2/Docs/Scripting.md` → "Exception-enabled target list" ([Z2-N-SCRIPT-021]):
  1. `E` is the set of the entry's `exceptions: true` elements present on the preset; `N` is the fixed name set `Luau.VM`, `Luau.CodeGen`, `Luau.Ast`, `Luau.Compiler`, `Luau.Config`, `Luau.Analysis`, `Luau.Analyze.CLI`; `A` is the set of non-`INTERFACE` targets of the entry in the transitive `LINK_LIBRARIES` closure of `Luau.Analysis` (the table's "further `Luau.*` library target that the pinned revision's `Luau.Analysis` depends on").
  2. Every element of `E` is in `N ∪ A`; otherwise `FATAL_ERROR "Zenith2: luau: <t> is marked exceptions but is not in Scripting.md's exception-enabled target list; adding it needs a decision recorded against Z2-D-017"`.
  3. Every non-`INTERFACE` target of the entry in the transitive `LINK_LIBRARIES` closure of an element of `E` is itself in `E`; otherwise `FATAL_ERROR "Zenith2: luau: <t> is linked by the exception-enabled <e> but is not listed with exceptions; adding it needs a decision recorded against Z2-D-017"`. A CLI helper library that the pinned `Luau.Analyze.CLI` links outside `A` therefore stops the configure until a ruling admits it, and the set never grows silently.
  4. Every other target the entry created (the unlisted CLI executables of `LUAU_BUILD_CLI`) keeps the ordinary third-party rows, is never built (the subproject is `EXCLUDE_FROM_ALL`) and is linked by nothing: `zenith_end_build` fails with `Zenith2: <target> links <t>, an unlisted target of luau` when any target outside the entry links one.
  The closure is read from the targets' `LINK_LIBRARIES` properties after `add_subdirectory` returns, following only names that are targets of the entry; generator expressions in those properties are evaluated for no configuration, so an element appears in the closure when it is named in any branch.
- A `hostExecutable` target gets `set_target_properties(<t> PROPERTIES OUTPUT_NAME <hostExecutable> RUNTIME_OUTPUT_DIRECTORY ${ZENITH_OUT}/${ZENITH_PRESET}/$<CONFIG>/bin EXCLUDE_FROM_ALL FALSE)` on the `win` preset, and is added to `zenith_host_tools`; on other presets the CLI targets are not created (`LUAU_BUILD_CLI` is `OFF` there) and the element's `platforms` exempt them from the existence check.
- `targets[].owner: "host"` targets are linked by no Zenith2 target: rule 6 of "Name resolution" refuses any `THIRD_PARTY` that would reach them.

### Exposed variables

`ZenithThirdParty.cmake` defines these cache-internal variables for the other helpers; they are the only way any helper reaches a cache or toolchain path (answers MOD's MB2):

| Variable | Value | Defined on |
|---|---|---|
| `ZENITH_TP_<ID>_DIR` | the verified tree (or prebuilt directory) of every entry created for the preset | every preset |
| `ZENITH_TP_<ID>_RUNTIME_FILES`, `ZENITH_TP_<ID>_EXE_<STEM>` | as the modes above | as created |
| `ZENITH_TOOLCHAIN_<KEY>_DIR` | `<cache root>/toolchain/<key>/<version>` for every toolchain key | every preset |
| `ZENITH_JDK_DIR` | `ZENITH_TOOLCHAIN_JDK_DIR` | Android presets |
| `ZENITH_ANDROID_SDK_DIR` | `<cache root>/android-sdk/<compileSdk>-<packages hash 16>` | Android presets |
| `ZENITH_GRADLE_HOME` | `<cache root>/gradle-home/<agp>-<metadata hash 16>` | Android presets |
| `ZENITH_DEV_KEYSTORE` | `<cache root>/keys/development.keystore` | Android presets |
| `ZENITH_GRADLE_VERIFICATION_METADATA` | `${ZENITH_ROOT}/ThirdParty/gradle/verification-metadata.xml` | Android presets |

Each Android-only directory is checked to exist at configure, with the message `Zenith2: <what> missing; run Tools/Setup/Setup-Zenith2.ps1 -AcceptAndroidSdkLicenses` when it is absent. `<cache root>` is `ZENITH_THIRD_PARTY_CACHE_ROOT`'s parent when the fixtures set it, otherwise `$ENV{LOCALAPPDATA}/Zenith2` or `$ENV{HOME}/.zenith2` (the scaffold's rule).

## zenith_add_library completion

### Keywords added by SHIP

<!-- elaboration -->
The scaffold implements every keyword of `Zenith2/Docs/Architecture/BuildSystem.md` → "Signature". SHIP adds two, whose core wording is cross-document item X220 item 5 (applied by core-document patch c; BuildSystem's signature now lists both), and completes `TEST_LABELS` to the discovery form of [Z2-N-ARCH-251] (X215, applied by core-document patch d):

```cmake
zenith_add_library(<Name>
	…                                                   # every keyword of BuildSystem's signature
	[TEST_THIRD_PARTY <ManifestName>...]                # tests-usage entries linked into <Name>'s test targets only
	[CONFORMANCE_LABELS <Label>...]                     # CTest labels of every test discovered from <Name>'s conformance executable
)
```

| Argument | Rule |
|---|---|
| `TEST_THIRD_PARTY` | manifest names with `usage: tests`, resolved as "Name resolution"; linked into `<Name>_Tests`, `<Name>`'s conformance executable (or the family's, for an API library) and `<Name>`'s fuzz targets (the scaffold's fuzz rule links `ZENITH_DECL_<Name>_TEST_THIRD_PARTY`); never into `<Name>` (RPROD's `flip`) |
| `CONFORMANCE_LABELS` | labels `^[a-z][a-z0-9-]*$`, applied to every test discovered from the conformance executable `<Name>` creates or contributes to (§ "Discovery labels"); for an API family the labels of the API library and of every backend library are unioned. `zenith_add_library(ZenithRHI … CONFORMANCE_LABELS z2-parity-backend)` is RHI's use (the orchestrator's 2026-10-02 parity-label ruling; cross-area row SH18) and `CONFORMANCE_LABELS script-conformance` SCRIPT's |
| `TEST_LABELS` (BuildSystem's keyword, completed here) | labels `^[a-z][a-z0-9-]*$`, applied to every test discovered from `<Name>_Tests` (§ "Discovery labels"); the scaffold's configure-time `set_tests_properties` form is replaced in T5, because it cannot name a test discovered at build time |

### Discovery labels

<!-- elaboration -->
Implements [Z2-N-ARCH-251] (X215, applied). Every `doctest_discover_tests` call the helpers make — for `<Name>_Tests`, for every conformance executable (§ "Conformance executables" step 4, once per backend where it runs per backend), for `<Name>_Tests` of a tool and for `Zenith_Integration_<Name>` — passes one `PROPERTIES LABELS "<list>"` and `ADD_LABELS 1`:

| Executable | `<list>` |
|---|---|
| `<Name>_Tests` (library or tool) | the library's `TEST_LABELS` |
| `<Api>_Conformance`, `<Name>_Conformance` | the union of the `CONFORMANCE_LABELS` of the library and, for an API family, of every backend library |
| `Zenith_Integration_<Name>` | the suite's `TEST_LABELS` (§ "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests") |

- `ADD_LABELS 1` makes doctest's discovery add the name of every doctest `TEST_SUITE` a case sits in as a further label, merged into the same `LABELS` property and never replacing `<list>` (doctest's `doctest_discover_tests` appends the suite labels to the `PROPERTIES` labels of each test it writes), so a suite such as `script-fault-matrix` is selectable with `ctest -L script-fault-matrix` and a gate line `Test.ps1 -Label memory` selects exactly the tests carrying `memory`.
- A label list is byte-ordered and duplicate-free before it is passed; an empty list passes no `PROPERTIES LABELS` and still `ADD_LABELS 1`.
- Death and fixture suites are excluded from discovery by the test main ([Z2-N-ARCH-179]), so their names never become discovered labels; their entries carry the labels their registration gives (`"<suite>;memory"` for CORE's `zenith_core_death_case`, § "Registry functions").
- `CMake_Ship_DiscoveryLabels` asserts the recorded discovery call of each executable kind.

### Conformance executables

<!-- elaboration -->
The conformance targets of BuildSystem's target table ([Z2-N-ARCH-178], [Z2-N-ARCH-256]), which the scaffold does not create (cross-area rows EB1, SC13), are created by `zenith_end_build()` once every declaration is known:

1. For each API library `A` that has `Tests/Conformance/` and is named by at least one `KIND BACKEND` library's `BACKEND_OF`: `add_executable(<A>_Conformance <A>/Tests/Conformance/** <B>/Tests/ConformanceAdapter/** for every backend B of A supported in the preset>)`, linking `A`, every such `B`, `A`'s and each `B`'s `TEST_DEPENDS` and `TEST_THIRD_PARTY` targets, `ZenithTestSupport` and doctest; it receives the test compile definitions ([Z2-N-ARCH-250], [Z2-N-ARCH-254]), `ZENITH_TEST_LINKS_<TARGET>` for every `TEST_DEPENDS` entry of `A` and of each `B` (`1` where the preset links it, `0` where it skips it, [Z2-N-ARCH-172]; cross-document item X197; links and definitions both come from `_zenith_ship_link_test_depends` of § "Test third-party links") and `zenith_stage_runtime_files(… TEST)`.
2. For each other library `L` with `Tests/Conformance/`: `add_executable(<L>_Conformance <L>/Tests/Conformance/**)` linking `L`, its `TEST_DEPENDS` targets supported in the preset, `ZenithTestSupport` and doctest, with the same compile definitions, `ZENITH_TEST_LINKS_<TARGET>` for every `TEST_DEPENDS` entry included (through `_zenith_ship_link_test_depends(<L>_Conformance <L>)`). `<L>_Tests` receives its links and definitions from the same function (§ "Test third-party links"), so the two executables of one library never disagree.
3. `ZenithGraph_Conformance` from `ZenithGraph/Tests/Conformance/**` plus `Tests/GraphConformance/**` of `ZenithJobs` and `ZenithFlux` where supported.
4. Registration, in `ZENITH_HOST` presets: `doctest_discover_tests(<exe> PROPERTIES LABELS "<labels>" ENVIRONMENT "ZENITH_EXPECTED_BUILD=<value>" TIMEOUT 60 ADD_LABELS 1)`, where `<labels>` are the library's `CONFORMANCE_LABELS` (§ "Discovery labels"). For `<A>_Conformance` the family's own case macro names each case `<doctest name>[<Backend>]` (`Zenith2/Docs/Design/COMPRESS.md`'s `ZENITH_COMPRESSION_CONFORMANCE_CASE`, ECS's backend loop), so discovery registers each name once. For `<L>_Conformance` with `KIND BACKEND` libraries among its `TEST_DEPENDS` ([Z2-N-ARCH-256]), discovery runs once per such backend `B`: `doctest_discover_tests(<exe> EXTRA_ARGS --zenith-backend=<BACKEND_NAME of B> TEST_SUFFIX "[<BACKEND_NAME>]" …)`, and the test code reads the selected backend from ZenithTestSupport's `Zenith_TestSelectedBackend()` (cross-area row SH4 to CORE); with no backend among `TEST_DEPENDS`, discovery runs once with no suffix.
5. On Android presets the conformance executables are built and never registered ([Z2-N-ARCH-180]).

### Fuzz and compile-fail partitions

<!-- elaboration -->
`zenith_add_library` partitions `Tests/Fuzz/*.cpp` and `Tests/**/*_CompileFail.cpp.in` as [Z2-N-ARCH-227] states and hands both lists to the test registry:

- **Fuzz sources** go to the global property `ZENITH_FUZZ_SOURCES` (`<Lib>|<absolute path>`), exactly as the scaffold does, and the scaffold's generic fuzz rule `_zenith_register_fuzz_targets()` (Z2T-MOD-056, the registry's last line) creates `Zenith_Fuzz_<Stem>` and its smoke entry `Zenith_Fuzz_<Stem>.Smoke` for every source no area helper already made a target for, in `win-asan`. That rule consumes every source by construction and refuses a duplicate stem, so SHIP adds no fuzz registration function for libraries and no "unconsumed fuzz source" check; SHIP's fuzz additions are the tool-side helper (§ "Tool-side fuzz targets"), the smoke options (§ "Fuzz smoke options") and applying a fixture set's properties to area-made smokes (§ "Registry tail").
- **Compile-fail sources** go to the global property `ZENITH_COMPILE_FAIL_SOURCES` (`<Lib>|<absolute path>`), SHIP's addition. They are registered by CORE's `zenith_core_compile_fail` (§ "Registry functions"); the registry tail fails the configure with `Zenith2: <path>: compile-fail source not registered by any block of CMake/Tests/ZenithCMakeTests.cmake` when a handed-over source was registered by no block, in every test preset (cross-area rows GB27, EB1), so a compile-fail source can never be silently dropped.

### Test environment

<!-- elaboration -->
Answers CORE's CB4. Every test the helpers register (doctest discovery, conformance discovery, registry entries, the discovered death cases) carries the CTest environment variable

```text
ZENITH_EXPECTED_BUILD=<platform>:<configuration>:<sanitizer>
```

| Field | Values | From |
|---|---|---|
| `<platform>` | `windows`, `android`, `linux` | `ZENITH_TARGET_OS` lower-cased |
| `<configuration>` | `debug`, `release`, `shipping`, `sanitize`, `hwasan` | `$<IF:$<CONFIG:ShippingTest>,shipping,$<LOWER_CASE:$<CONFIG>>>`, evaluated per configuration (doctest's discovery runs as a per-configuration post-build command, so the generator expression resolves for each configuration of a multi-configuration tree); `ShippingTest` reports `shipping` ([Z2-N-ARCH-231], [Z2-N-ARCH-250]) |
| `<sanitizer>` | `none`, `asan`, `tsan-ubsan`, `hwasan` | `ZENITH_SANITIZER` (`address` → `asan`, `thread;undefined` → `tsan-ubsan`) and the `Hwasan` configuration (`hwasan`) |

Every `<Name>_Tests` and conformance executable also receives the compile definition `ZENITH_TEST_GENERATED_ROOT` ([Z2-N-ARCH-250] as X262 applied it): on host presets `"${ZENITH_OUT}/generated/<Name>/Tests"`; on Android presets the target's device directory `"/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/generated/Tests"`, into which a device run pushes the host directory (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files"); forward slashes, no trailing slash, defined whether or not the directory exists. Both forms come from the scaffold's one function `_zenith_test_root_definitions` (Z2T-MOD-056), which T5 extends by the generated-root output, so the host and device forms cannot drift. It is the one directory a test or a registry block writes generated test inputs into before a test reads them (SCRIPT's test bytecode and recorded replays, cross-area row SB15). The device copy is MOD's test-APK route (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs"); the definition itself is now the helper's on every preset, so cross-area row SH15 (b) asks MOD only for the push.

Values match `Zenith_BuildPlatform`, `Zenith_BuildConfiguration` and `Zenith_SanitizerKind` of `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Build information" one to one (`windows` ↔ `BUILD_PLATFORM_WINDOWS`, `sanitize` ↔ `BUILD_CONFIGURATION_SANITIZE`, `shipping` ↔ `BUILD_CONFIGURATION_SHIPPING` in both `Shipping` and `ShippingTest`, `tsan-ubsan` ↔ `SANITIZER_KIND_TSAN_UBSAN`). `Pwsh_Ship_ExpectedBuildEnvironment` reads `ctest --show-only=json-v1 -C <config>` in the `win` tree and asserts every registered test carries the variable with the configuration's value.

Every test, conformance, benchmark and integration-suite target also receives `ZENITH_SHADERS_GENERATED_DIR` ([Z2-N-ARCH-254], Z2-R-SHIP-153): on host presets `"${ZENITH_OUT}/generated/ZenithShadersGenerated"`; on Android presets the target's device directory `"/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/shaders"`, into which a device run pushes the host `Blobs/` and `DevelopmentBlobs/` directories (row C21); forward slashes, no trailing slash, defined whether or not the directory exists. It is the fourth output of the same `_zenith_test_root_definitions`, applied by `zenith_add_library` (every `<Name>_Tests`), by `zenith_end_build` (every conformance executable), by `zenith_add_tool` (its `<Name>_Tests`), and by `zenith_add_benchmark` and `zenith_add_integration_tests`; no library, tool, game, Player or `KIND GENERATED` target receives it.

### Test third-party links

<!-- elaboration -->
`<Name>_Tests` links `<Name>`, the `TEST_DEPENDS` entries the preset supports, its `TEST_THIRD_PARTY` targets, `ZenithTestSupport`, every `autoLink` `tests` entry (doctest). The `TEST_DEPENDS` half implements [Z2-N-ARCH-172], BuildSystem's `TEST_DEPENDS` argument row and its step 11 (cross-document item X197, applied; cross-area row RB21) in SHIP's ticket T5, replacing the scaffold's unconditional link (Z2T-MOD-056 links every `TEST_DEPENDS` target into `<Lib>_Tests` and defines no `ZENITH_TEST_LINKS_<TARGET>`). `<Name>_Tests` of every library (a tool has no `TEST_DEPENDS` argument) links only the `TEST_DEPENDS` entries the preset supports, skips the rest with no configure error and without removing `<Name>_Tests`, and defines `ZENITH_TEST_LINKS_<TARGET>` (the target name upper-cased, BuildSystem's `TEST_DEPENDS` row: `ZENITH_TEST_LINKS_ZENITHRHI_D3D12`) to `1` or `0` for every entry:

1. `zenith_add_library` records the library's `TEST_DEPENDS` names, byte-ordered, as the global property `ZENITH_DECL_<Name>_TEST_DEPENDS` before its `SUPPORTED` check (SHIP's addition to the declaration store, beside `ZENITH_DECL_<Name>_THIRD_PARTY`; cross-area row SH24 asks MOD to list it in the store table) and links none of them itself. The decision waits for `zenith_end_build` because the byte-ordered `add_subdirectory` can reach a `TEST_DEPENDS` target's directory after the library's: `Engine/ZenithFlux` is added before `Engine/ZenithRHI_D3D12` and `Engine/ZenithRHI_Vulkan`, so whether the preset has those targets is known only once every directory has been added.
2. `zenith_end_build`, before it creates the conformance executables, calls `_zenith_ship_link_test_depends(<Name>_Tests <Name>)` for every name of `ZENITH_DECLARATIONS`, in byte order, whose `<Name>_Tests` exists (`if(TARGET <Name>_Tests)`). For each entry `<T>` of `ZENITH_DECL_<Name>_TEST_DEPENDS`, in order:
   - `<T>` is in neither `ZENITH_DECLARATIONS` nor `ZENITH_TOOLS`: `FATAL_ERROR "zenith_add_library(<Name>): TEST_DEPENDS <T> is not a Zenith2 library or tool library"`. Both lists hold every library and tool directory the top-level walk adds, in every preset, recorded before each helper's `SUPPORTED` check, so an entry the preset lacks is still declared and only a misspelling reaches this message;
   - otherwise, when `if(TARGET <T>)` is true (the preset supports `<T>`: a library or tool is created only on the presets it supports): `target_link_libraries(<Name>_Tests PRIVATE <T>)`, a link from the top-level directory to a target of another directory, which policy CMP0079 admits (`NEW` under BuildSystem's `cmake_minimum_required(VERSION 3.28)`), and the value `1`;
   - otherwise: no link, no message, the value `0`; `<Name>_Tests` stays in the preset and the configure continues.
3. For every entry, linked or skipped, `target_compile_definitions(<Name>_Tests PRIVATE ZENITH_TEST_LINKS_<TARGET>=<0|1>)`, `<TARGET>` being `<T>` through `string(TOUPPER)` (target names hold only `[A-Za-z0-9_]`, so nothing else changes). Test sources select the linked backends with `#if ZENITH_TEST_LINKS_<TARGET>`, never with a platform macro.
4. `zenith_end_build` passes each conformance executable's `TEST_DEPENDS` through the same function (§ "Conformance executables"), and `declared_deps.json` → `testDepends` keeps every declared entry, linked or not.

On `android-arm64`, `zenith_add_library(ZenithFlux … TEST_DEPENDS ZenithRHI_Vulkan ZenithRHI_D3D12)` (the call Z2T-RHI-055 extends) gives `ZenithFlux_Tests` the link `ZenithRHI_Vulkan` alone, `ZENITH_TEST_LINKS_ZENITHRHI_VULKAN=1` and `ZENITH_TEST_LINKS_ZENITHRHI_D3D12=0`, and no `FATAL_ERROR`; on `win` both are `1`. Each of these executables also gets `zenith_stage_runtime_files(<exe> TEST)`, so a GPU test finds WARP's DLL beside it. `CMake_Ship_TestLinksDefinitions` covers the rule (Z2-R-SHIP-152).

### ShippingTest configuration

<!-- elaboration -->
Implements the `ShippingTest` configuration of `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags" (table "Configurations", [Z2-N-ARCH-231], X216 applied by core-document patch d) in SHIP's helpers:

| Helper behaviour | In `ShippingTest` |
|---|---|
| configuration types | `win`'s `CMAKE_CONFIGURATION_TYPES` is `Debug;Release;Shipping;ShippingTest` (the presets file); no other tree has it |
| per-configuration variables and imported mapping | `CMAKE_<LANG>_FLAGS_SHIPPINGTEST` and `CMAKE_MAP_IMPORTED_CONFIG_SHIPPINGTEST` (§ "Global flag hygiene") |
| `zenith_apply_flags` | every row of `Shipping` (`/DZENITH_CONFIG_SHIPPING=1` included, so `Zenith_IsShippingBuild()` is true) and its link rows; `ZENITH_ASSERTS_ENABLED=0`, `ZENITH_DEBUG=0`; the hardening rows (§ "Hardening flags") |
| excluded dependencies | not linked (`$<CONFIG:Shipping,ShippingTest>`, § "Shipping-excluded dependencies") |
| runtime staging | `pins.shippingRuntime` (§ "Runtime file staging") |
| test environment | `ZENITH_EXPECTED_BUILD=windows:shipping:none` (§ "Test environment") |
| what builds | the `win-shipping-test` build preset builds `zenith_tests` alone (every test, conformance and integration-suite target the preset supports, [Z2-N-ARCH-191]); no game executable, Player, tool or benchmark is built in it, and no build preset naming `zenith_players` uses it ([Z2-N-ARCH-181]) |
| registry | the registry's entries exist in `ShippingTest` like in `Release`: `z2-death-assert` and asserts-only entries are absent (their `CONFIGURATIONS` are `Debug Sanitize Hwasan`) |

`CMake_Ship_ShippingTestConfiguration` asserts each row in script mode. Running the `win-shipping-test` test preset in the `Test` gate step is a PROC `needs-human` edit of the protected step and is not SHIP's.

### Generated shader library

<!-- elaboration -->
Realises step 3a of `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper" (cross-area row RPB22; SHIP owns it because `ZenithAddLibrary.cmake` is SHIP's after the scaffold, orchestrator ruling RPB22-SHIP of 2026-10-10; Z2-R-SHIP-153). `zenith_add_library(<Name> KIND GENERATED SHADERS <Directory> DEPENDS <Library>...)`, used once, by `ZenithShadersGenerated` ([Z2-N-ARCH-238]):

1. `SHADERS` is accepted for `KIND GENERATED` alone (`zenith_add_library(<Name>): SHADERS is accepted for KIND GENERATED alone`), and `KIND GENERATED` requires it (`zenith_add_library(<Name>): KIND GENERATED needs SHADERS <Directory>`). A relative directory is resolved against the repository root (the parent of `ZENITH_ROOT`, as `SHADERS Zenith2/Shaders/` is written), an absolute one is used unchanged, and a missing directory stops the configure (`zenith_add_library(<Name>): SHADERS <dir> does not exist`).
2. After the declaration is recorded: `add_library(<Name> INTERFACE)`, `target_include_directories(<Name> INTERFACE ${ZENITH_OUT}/generated/<Name>)`, `target_link_libraries(<Name> INTERFACE <DEPENDS…>)`, then `zenith_add_shaders(<Name> <resolved directory>)`, RPROD's function in `CMake/ZenithShaders.cmake`, which adds the offline compile commands, their outputs and `<Name>_Build`. `ZenithBuild.cmake` includes `CMake/ZenithShaders.cmake` with `include(… OPTIONAL)` in its helper-include list (BuildSystem's helper table: it includes every helper file that exists), so the function is defined as soon as RPROD adds that file, without an RPROD edit of a SHIP file. While that function is not defined (the helper lands before RPROD's ticket, declare first), the call stops the configure with `zenith_add_library(<Name>): KIND GENERATED needs zenith_add_shaders (CMake/ZenithShaders.cmake)`; no tree declares a `KIND GENERATED` library before the function exists.
3. The declaration (`kind: generated`, its `DEPENDS`) and every file `zenith_add_shaders` appends to the global property `ZENITH_GENERATED_FILES_<Name>` (an absent property is an empty list; cross-area row ShB15 asks RPROD's `zenith_add_shaders` to append every output it declares) are recorded for `declared_deps.json` (§ "declared_deps additions"), and the function returns: steps 4 to 12 (sources, flags, test targets, fuzz and compile-fail hand-over) do not run for this kind.

Tests: `CMake_Ship_KindGeneratedBranch` (`Zenith2/Docs/Design/SHIP/Tests.md` → "CMake script-mode tests"); the definition of § "Test environment" is `CMake_Ship_ShadersGeneratedDirDefined`.

## zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests

<!-- elaboration -->
The signatures of `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites" stand (cross-document item X220 item 5, applied; the integration suites' `TEST_LABELS`, X215, applied by core-document patch d); SHIP implements their remaining behaviour (cross-area rows CB2, CB5, EB3, GB27, RB7) in its Stage 1 ticket T6 (`Zenith2/Docs/Design/SHIP.md` → "Stage 1"):

```cmake
zenith_add_tool(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [TEST_THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...])
zenith_add_benchmark(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...])
zenith_add_integration_tests(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...] [TEST_LABELS <Label>...])
```

| Helper | `THIRD_PARTY` | Links | Asserts | Registration |
|---|---|---|---|---|
| `zenith_add_tool` | linked `PRIVATE` into the tool, `<Name>_Objects` and `<Name>_Tests` (the tool's own tests exercise the same third-party code) | `DEPENDS`, `THIRD_PARTY`; `<Name>_Tests` adds `ZenithTestSupport`, doctest, `TEST_THIRD_PARTY` | by configuration, like every target (§ "Assert define"); a tool's Release invariants use `ZENITH_CHECK` ([Z2-N-CONTRACT-479]) | the tool's tests by doctest discovery (§ "Discovery labels") |
| `zenith_add_benchmark` | linked `PRIVATE` into `Zenith_Bench_<Name>`; any entry may be named, including `owner: "benchmark"` candidates ([Z2-N-ARCH-200]) | `DEPENDS`, `THIRD_PARTY`, nanobench (`autoLink`), `ZenithTestSupport` (CB2) | by configuration | the `--smoke` CTest run, labelled `z2-bench-smoke`, timeout 60 s |
| `zenith_add_integration_tests` | linked `PRIVATE` into `Zenith_Integration_<Name>` | `DEPENDS`, `THIRD_PARTY`, `ZenithTestSupport`, doctest | by configuration | doctest discovery with `ZENITH_EXPECTED_BUILD`, its `TEST_LABELS` (each `^[a-z][a-z0-9-]*$`) as the discovery call's `LABELS` and the suite names through `ADD_LABELS` (§ "Discovery labels", [Z2-N-ARCH-251]) |

`zenith_add_tool` also records `ZENITH_DECL_<Name>_TOOL_KIND` (`EXECUTABLE`, or `LIBRARY` when the directory has no `Internal/Zenith_<Name>Main.cpp`), `zenith_add_benchmark` records `ZENITH_DECL_Zenith_Bench_<Name>_BENCHMARK` (`1`), and `zenith_add_integration_tests` appends `<Name>` to the global list `ZENITH_INTEGRATION_SUITES` (which the registry tail enumerates, § "Unlisted death-case discovery"), each as a global property before its `SUPPORTED` check, so the ownership rules K1 to K5 know every tool and every benchmark in every preset (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules"; a benchmark target is the owner of a selection candidate pinned by Z2T-CORE-053). For all three helpers rule 2 of § "Name resolution" skips, rather than refuses, an entry absent from the preset's platform, and rule 4 states which usages each may name.

The bindless probe's build-time shader compile (`Zenith2/Docs/Rendering.md` [Z2-N-REND-101]'s one exemption, cross-document item X167; cross-area row RB7) is two further `zenith_add_benchmark` arguments:

```cmake
zenith_add_benchmark(<Name> … [SLANG_SHADERS <Directory>] [SLANG_ALTERNATIVES <N>...])
```

- `SLANG_SHADERS` is accepted for `<Name>` `BindlessProbe` alone; any other benchmark passing it fails the configure with `zenith_add_benchmark(<Name>): SLANG_SHADERS is reserved for BindlessProbe ([Z2-N-REND-101])`, and the ticket that lands `ZenithShaderCompiler` removes the argument together with the exemption.
- `SLANG_ALTERNATIVES` is the list of D-SLANG (open, Stage 1) lowering alternatives to compile, each `1`, `2` or `3` (the alternatives of `Zenith2/Docs/Rendering.md` → "Decision D-SLANG"), unique; it defaults to `1 2 3`, so every alternative the probe's `--alternative <1|2|3>` can select exists. Which alternative the probe runs by default is the probe's own option (RHI's design: 2), not a build argument. Z2T-RHI-001's `SLANG_ALTERNATIVES 2` is a valid list (alternative 2 alone is compiled, the probe's default), so that ticket owes nothing; the D-SLANG case ticket that runs `--alternative 1` and `--alternative 3` passes `SLANG_ALTERNATIVES 1 2 3` or omits the argument (cross-area row SH18 (b), advisory). Any other value is `zenith_add_benchmark(<Name>): SLANG_ALTERNATIVES <v> is not 1, 2 or 3`.
- For every `<Directory>/*.slang` file and every listed alternative `N`, on the `win` preset, one custom command runs `slang::slangc <file> -D ZENITH_DSLANG_ALTERNATIVE=<N> -target spirv -O2 -o ${ZENITH_OUT}/generated/Bench_<Name>/Shaders/alt<N>/<stem>.spv` and one the same with `-target dxil -profile sm_6_6 -o …/<stem>.dxil`; both name the `slangc` file and the source in `DEPENDS`, and the benchmark depends on their outputs. Android presets compile nothing (no host tool runs there, [Z2-N-ARCH-192]) and read the `.spv` files the `win` tree produced.
- The benchmark receives the compile definitions `ZENITH_BENCH_SHADER_DIR="${ZENITH_OUT}/generated/Bench_<Name>/Shaders"` (forward slashes), `ZENITH_BENCH_SHADER_ALTERNATIVES="<N>,<N>…"` (the compiled list) and `ZENITH_PROBE_SLANG_VERSION="<toolchain.slang.version>"` (the pinned Slang release, the value the probe report's `slangVersion` field records; read from the manifest at configure, on every preset). Android packaging of the probe is MOD's hosted benchmark APK (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs", Z2T-MOD-063).

All three record their `THIRD_PARTY` list in `declared_deps.json` (§ "declared_deps additions") and call `zenith_stage_runtime_files(… TEST)` on every executable they create (the tool executable itself without `TEST`).

**Static tool libraries in `win-asan`** (SEC row SEC20, accepted; core-document item X444). A `zenith_add_tool` target without `Internal/Zenith_<Name>Main.cpp` (`ZENITH_DECL_<Name>_TOOL_KIND` `LIBRARY`) may declare `SUPPORTED ZENITH_WINDOWS`, as `ZenithContentCore` does: `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` take it so that a parser they host is fuzzed in `win-asan` (§ "Tool-side fuzz targets"), while the tool executables (`ZenithCook`, `ZenithEditor`, `ZenithNewGame`, `ZenithContent`, `ZenithDevTool`, `ZenithShaderCompiler`) stay `ZENITH_TOOLS_HOST`. SHIP's preset model admits it without a new rule: every library those three depend on is `ZENITH_ALL`, `ZENITH_GPU`, `ZENITH_HOST` or `ZENITH_WINDOWS` and therefore built in `win-asan`, and [Z2-N-ARCH-172] already makes the configure fail with its `… depends on <Library>, which is not` line if a later owner move appends a `ZENITH_TOOLS_HOST` dependency (a tool executable, `ZenithShaderCompiler`) to one of them. In `win-asan` each of the three is created with its `<Name>_Tests`, which the `Sanitizers -Preset win-asan` step builds and runs like every other test target of that tree; nothing of them reaches `linux-tsan` or an Android preset.

**Fuzz sources of a tool.** `zenith_add_tool` keeps `Tests/Fuzz/**` out of `<Name>_Tests` and `<Name>_Objects` in every preset, as [Z2-N-ARCH-227] does for libraries (SEC row SEC6 (g)), and hands nothing to `ZENITH_FUZZ_SOURCES`: a tool's fuzz source is built only by an explicit `zenith_tool_fuzz_target` call.

## zenith_apply_flags

```cmake
zenith_apply_flags(<Target> [THIRD_PARTY] [EXCEPTIONS])
```

| Option | Meaning |
|---|---|
| none | a Zenith2 engine, module, backend, Player, game, tool, test, conformance, fuzz, benchmark or integration target: every row of BuildSystem's two flag tables ([Z2-N-ARCH-230]) |
| `THIRD_PARTY` | a third-party target: the rows of "Third-party flag rows" only; never applied to an `INTERFACE` or `IMPORTED` target |
| `EXCEPTIONS` | with `THIRD_PARTY` only (a `luau` `exceptions: true` target), or on `<Name>_EH` object libraries ([Z2-N-ARCH-176]): the exceptions-on row replaces the exceptions-off row |

The `TOOL` option of earlier drafts is withdrawn: it existed only to fix the assert define to 1 in tools, which the per-configuration rule below replaces (Q71, cross-document item X231).

### Assert define

Implements [Z2-N-CONTRACT-479] and the table "Configurations" of `Zenith2/Docs/Architecture/BuildSystem.md` → "Compile and link flags", as core-document patch c applied them (the orchestrator's Q71 ruling (a), 2026-10-03): `ZENITH_ASSERTS_ENABLED` is a value of the configuration, identical in every Zenith2 target of that configuration, tools included.

| Target class | `ZENITH_ASSERTS_ENABLED` | `ZENITH_DEBUG` |
|---|---|---|
| every Zenith2 target (engine, module, backend and Player libraries; game executables; `zenith_add_tool` targets; test, conformance, fuzz, benchmark and integration targets) | `$<IF:$<CONFIG:Debug,Sanitize,Hwasan>,1,0>` | `$<IF:$<CONFIG:Debug>,1,0>` |

Because the value never differs between two targets of one link, an inline function or template in an engine public header that uses `ZENITH_ASSERT` has one definition in every translation unit of an executable, and no tool sees a different assert behaviour from the engine code it links. A host tool that needs an invariant checked in the `win-tools` Release build writes `ZENITH_CHECK`, which is on in every configuration. The flagged proposal SHIP-1 that raised the earlier per-target define is withdrawn as resolved by this ruling.

### Third-party flag rows

The rows are `Zenith2/Docs/Architecture/BuildSystem.md` → "Third-party targets and configuration variables" (cross-document item X221, applied by core-document patch c), which is authoritative; they exist because `ForbiddenCheck` rule F17 and [Z2-N-SCRIPT-028] fail any compile group outside the Luau firewall that enables exceptions or RTTI, and clang enables both by default:

| Row | MSVC | clang |
|---|---|---|
| sanitizer | the `Sanitize` row's `/fsanitize=address` (win-asan) | `-fsanitize=thread -fsanitize=undefined -fno-sanitize-recover=undefined` (linux-tsan), `-fsanitize=hwaddress` (Hwasan) |
| runtime library | `CMAKE_MSVC_RUNTIME_LIBRARY` as BuildSystem | — |
| debug information | `/Z7` (`Embedded`) | `-g` |
| exceptions off (every target except `EXCEPTIONS`) | `/EHs-c- /D_HAS_EXCEPTIONS=0` | `-fno-exceptions` |
| exceptions on (`EXCEPTIONS`, the `luau` `exceptions: true` targets) | `/EHs /D_HAS_EXCEPTIONS=1` ([Z2-N-SCRIPT-027]: the Luau targets define `_HAS_EXCEPTIONS` explicitly, the value the `<Name>_EH` groups use) | `-fexceptions` |
| RTTI off (every third-party target) | `/GR-` | `-fno-rtti` |
| reproducibility | the "Reproducibility flags" row | the same |

- The rows are applied to every non-`INTERFACE`, non-`IMPORTED` target the entry creates, found by the recursive enumeration of § "zenith_third_party_add_all" step 3, so nested upstream directories receive them too. An `INTERFACE` or `IMPORTED` target receives no compile row.
- `_HAS_EXCEPTIONS` is fixed per compile group: `0` in every Zenith2 group outside `<Name>_EH` and in every third-party group except the Luau rows, `1` in the Luau rows and the `_EH` groups. The standard-library inline definitions that depend on it therefore differ only between the firewall's groups and the rest, a boundary [Z2-N-SCRIPT-025] keeps every Luau header behind, so no inline entity used across that boundary is compiled with both values.
- Optimisation comes from the per-configuration variables of "Global flag hygiene". Third-party warnings are never errors and their headers are `SYSTEM` includes ([Z2-N-CONTRACT-044]). A dependency that does not build with exceptions and RTTI off is not admitted to the manifest unless a recorded decision amends F17 for it; no such decision exists.

### Reproducibility flags

Applied to every Zenith2 and third-party target in every configuration, from Stage 1 (the build IDs SAVE reads from Stage 2 need them, and [Z2-N-ARCH-258] states them for every preset; cross-document items X203 and X221, applied):

| Toolchain | Compile | Link |
|---|---|---|
| MSVC | `/Brepro` | `/Brepro`; executables also `/PDBALTPATH:%_PDB%` (the PE records the PDB file name only, never the build directory) |
| clang | `-ffile-prefix-map=${ZENITH_ROOT}=Zenith2 -ffile-prefix-map=${ZENITH_OUT}=out -fno-record-gcc-switches` | `-Wl,--build-id=sha1` on every executable and shared object, Android and Linux (`linux-tsan` included, so `Zenith_GetRunningBuildID` is non-zero in every preset, SAVE row SV15) |

MSVC links `/DEBUG:FULL` in every configuration (BuildSystem's debug-information row), so every executable carries a CodeView RSDS record (SAVE row SV15). `/Brepro` makes the PE timestamp and the CodeView GUID a hash of the image, so the RSDS GUID — the Windows build ID of `Zenith_GetRunningBuildID()` (`Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header") and the symbol-store key — is a function of the bytes.

### Hardening flags

Applied in the `Release`, `Shipping` and `ShippingTest` configurations (MSVC; `ShippingTest` takes every `Shipping` row, [Z2-N-ARCH-231], X216 applied) and in `Release` and `Shipping` on Android, from Stage 1, before the first budget run, so every budget run (Release, [Z2-N-BUDGET-108]) measures the hardened code that ships ([Z2-N-ARCH-259]; cross-document item X221, applied). Every Stage 1 to Stage 6 budget allocation and evidence record is therefore measured with these rows on.

| Toolchain | Compile | Link |
|---|---|---|
| MSVC | `/guard:cf` | `/guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT` |
| clang (Android) | `-fstack-protector-strong -D_FORTIFY_SOURCE=2` | `-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack` |

| Target class | Compile half | Link half |
|---|---|---|
| every Zenith2 target: engine, module, backend, Player and tool libraries, `<Name>_Objects`, game, tool, test, conformance, fuzz, benchmark and integration executables, the Android shared object (every object an image links) | yes | — |
| every Zenith2 executable and shared object | (as above) | yes |
| third-party targets | never (their rows are § "Third-party flag rows" alone; an unhardened third-party object inside a hardened image is the documented cost of integrating upstream builds unchanged) | — |

The linux-tsan preset builds only `Sanitize`, so no Linux hardening row exists. The Shipping qualification's binary checks read the rows back (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist", "Android package checklist"); SEC's hardening evidence cites those checks (cross-area row SH8).

## Toolchain files

| File | Steps |
|---|---|
| `Zenith2/CMake/toolchains/msvc-x64.cmake` | read `toolchain.msvc.clVersion` from `${CMAKE_CURRENT_LIST_DIR}/../../ThirdParty/manifest.json` with `string(JSON)`; `set(CMAKE_C_COMPILER cl)`, `set(CMAKE_CXX_COMPILER cl)` (resolved from the environment `Enter-Zenith2Env.ps1` imported); `set(CMAKE_C_COMPILER_LAUNCHER sccache)` and the C++ one ([Z2-N-ARCH-197], [Z2-N-ARCH-239]); `set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)`; `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")`; records the pin in `ZENITH_PINNED_COMPILER_VERSION`, which `zenith_begin_build` compares (step 2 of § "Global flag hygiene": the first three components of `CMAKE_CXX_COMPILER_VERSION`), because a toolchain file runs before the compiler is identified ([Z2-N-ARCH-194]) |
| `Zenith2/CMake/toolchains/clang-linux-x64.cmake` | read the `llvm` pin's `version`; `set(CMAKE_C_COMPILER $ENV{HOME}/.zenith2/toolchain/llvm/<version>/bin/clang)` and `CMAKE_CXX_COMPILER` the same directory's `clang++`, the tree the `llvm` pin's `linux` archive is extracted to by `Install-Zenith2LinuxHost` inside WSL and on the native Linux CI host alike (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Linux host mode"); sccache launchers; `ZENITH_PINNED_COMPILER_VERSION` for the comparison in `zenith_begin_build` |
| the Android presets | use the NDK's own `android.toolchain.cmake` ([Z2-N-ARCH-212]); `zenith_begin_build` checks `ANDROID_NDK_REVISION` (§ "Global flag hygiene" step 2) |

## Registry functions

<!-- elaboration -->
The test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` ([Z2-N-ARCH-252]) already has registration forms that every area's tickets call: CORE's block (Z2T-CORE-002) defines `zenith_core_death_case`, `zenith_core_selftest_case`, `zenith_core_compile_fail` and `zenith_core_fuzz_target`, which the CORE, ECS, SCENE, SCRIPT and ASSET tickets call about 130 times for any target, and the scaffold (Z2T-MOD-056) defines the generic fuzz rule. SHIP adopts those forms unchanged as the programme's registration, so no existing call, CTest name or DoD citation changes, and adds only what none of them provides: two functions for its own script tests, the fixture-set function, the tool-side fuzz helper with its ZenithCook wrapper, the smoke-options function, and the registry tail. Each SHIP function registers nothing outside the presets [Z2-N-ARCH-252] names and is a no-op on Android presets.

### Death-case registration

A death, asserts-only, sanitizer-defect or self-test case is registered with CORE's function, exactly as Z2T-CORE-002 defines it (the authoritative form; restated here only so SHIP's tail can read its output):

```cmake
zenith_core_death_case(TARGET SUITE CASE [ASSERTS_ONLY] [PASS_REGEX <regex>] [FAIL_REGEX <regex>] [CONFIGURATIONS <Config>...])
zenith_core_selftest_case(TARGET CASE PASS_REGEX [CONFIGURATIONS <Config>...])
```

- The CTest name is `<Target>.<Case>`, the command `$<TARGET_FILE:<Target>> --test-suite=<Suite> --test-case=<Case>`, the labels `"<Suite>;memory"`, the timeout 60 seconds; suites `z2-death-check`, `z2-death-assert`, `z2-sanitizer-defect` and `z2-selftest`, with the pass conditions of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (table "Death and sanitizer-defect tests"); `z2-death-assert` and `ASSERTS_ONLY` entries carry `CONFIGURATIONS Debug Sanitize Hwasan` (`ZENITH_CORE_ASSERTS_CONFIGURATIONS`, the asserts-enabled configurations of [Z2-N-CONTRACT-479] — every target class, tools included, since asserts follow the configuration, § "Assert define"); `z2-sanitizer-defect` only with `ZENITH_SANITIZER`.
- `<Target>` may be any test or conformance executable, so death cases needing a backend run inside `<Api>_Conformance` (EB2; ECS's tickets register `ZenithECS_Conformance` cases this way).
- SHIP's earlier `zenith_register_death_case` is withdrawn: it would have registered a second name form (`<Target>[<Case>]`) beside CORE's committed one.

### Unlisted death-case discovery

Death, asserts-only and sanitizer-defect cases are excluded from doctest discovery ([Z2-N-ARCH-179]) and registered one by one, so a case added to a source and never registered would never run. Rather than failing such a tree (which would red signed-off tickets whose cases are registered elsewhere or not at all, for example Z2T-GRAPH-009's `Test_Graph_ForeignHandle_Asserts`), the registry tail registers every such case itself, with the suite's default pass condition, so every death case of every executable runs (EB2's "registration of discovered cases").

**Naming convention.** A case of a `z2-death-check`, `z2-death-assert` or `z2-sanitizer-defect` suite is named by its test ID itself (`TEST_CASE("Test_Graph_ForeignHandle_Asserts")`), not by the `<Library>/<Type>/<Behaviour>` form [Z2-N-CONTRACT-411] gives every other case, because every registration passes the `Test_*` ID as `CASE` and runs `--test-case=<CASE>` (Z2T-CORE-002; Z2T-ECS-106: "the death case is named by its `Test_*` name"); the backend-loop forms keep their suffix (`Test_…[Custom]`, `<case>[Flecs]`). A death case written in the long form makes CORE's entry fail (its `--test-case=<ID>` selects no case, so the pass pattern never appears), and step 4 (c) keeps it from also being registered a second time by discovery, so the one red entry names the mistake.

The discovery runs in two halves: the registry tail writes, at configure and generate time, what CTest needs; CTest lists each executable's death suites when it reads the test list, the way doctest's own `TEST_INCLUDE_FILES` discovery works. Nothing is attached to the executables themselves:

1. **Enumeration** (configure, inside `_zenith_ship_registry_tail()`, `ZENITH_HOST` presets only). The executables `X`, byte-ordered and without duplicates, are: `<Lib>_Tests` for every name `<Lib>` of `ZENITH_DECLARATIONS` for which `if(TARGET <Lib>_Tests)` holds; `<Tool>_Tests` for every name of `ZENITH_TOOLS` for which `if(TARGET <Tool>_Tests)` holds; every name of the global property `ZENITH_CONFORMANCE_TARGETS`, which `zenith_end_build` appends each conformance executable to as it creates it (§ "End-of-build outputs"); and `Zenith_Integration_<Name>` for every `<Name>` of the global property `ZENITH_INTEGRATION_SUITES` (appended by SHIP's `zenith_add_integration_tests`, T6; both lists are SHIP's additions to the declaration store, cross-area row SH24) for which `if(TARGET Zenith_Integration_<Name>)` holds, since integration suites carry death cases too (Z2T-CORE-086 registers four on `Zenith_Integration_Core`). Before T6 lands that property is empty and the suites' unregistered cases wait for it. No other executable kind has death suites (benchmarks, fuzz targets and `<NAME>_CompileFail` executables have none).
2. **Registered names** (configure). For each `X`, every test of the registry's directory (`get_property(… DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY TESTS)`) whose name begins `X.`, with that prefix removed (CORE's `<Target>.<Case>` form), byte-ordered, one per line, written to `${ZENITH_OUT}/generated/registry/<X>.registered-cases.txt` (rewritten only when its bytes change; empty when none).
3. **Per-configuration parameters** (generate). `file(GENERATE OUTPUT ${ZENITH_OUT}/generated/registry/<X>.death-$<CONFIG>.cmake CONTENT "<text>")`, one file per configuration of the tree, whose text sets `ZENITH_EXE` to `$<TARGET_FILE:X>`, `ZENITH_TARGET` to `X`, `ZENITH_CONFIG` to `$<CONFIG>`, `ZENITH_SANITIZED` to `1` exactly when `ZENITH_SANITIZER` is set (the condition under which CORE's `zenith_core_death_case` registers `z2-sanitizer-defect`, so `1` in `win-asan` and `linux-tsan`) and `0` otherwise, `ZENITH_REGISTERED` to the file of step 2, `ZENITH_EXPECTED_BUILD` to the value of § "Test environment" (its generator expression evaluated for that configuration), and `ZENITH_PASS_<suite>` for each of the three suites to the pattern of step 5, and then includes `${ZENITH_ROOT}/CMake/Tests/DiscoverDeathCases.cmake`. `file(GENERATE)` evaluates `$<TARGET_FILE:X>` for a target of any directory, so the tail adds no `add_custom_command(TARGET … POST_BUILD …)`: CMake refuses that form for a target created in another directory (policy CMP0040, `NEW` under BuildSystem's `cmake_minimum_required(VERSION 3.28)`), and every `<Lib>_Tests` is created in its library's directory while the registry runs at the top level.
4. **CTest-time listing.** The tail writes `${ZENITH_OUT}/generated/registry/<X>.death-include.cmake` once, with the text `if(EXISTS "<dir>/<X>.death-${CTEST_CONFIGURATION_TYPE}.cmake")`, `include("<dir>/<X>.death-${CTEST_CONFIGURATION_TYPE}.cmake")`, `endif()`, and appends it to the `TEST_INCLUDE_FILES` property of the registry's directory, so CTest includes it whenever it reads the test list (`ctest`, `ctest -N`, `ctest --show-only=json-v1`). `DiscoverDeathCases.cmake` then:
   - (a) when `ZENITH_EXE` does not exist (that configuration was not built) registers nothing, exactly as doctest's discovery has no tests for an unbuilt configuration;
   - (b) for `z2-death-check`, `z2-death-assert` and, when `ZENITH_SANITIZED` is `1`, `z2-sanitizer-defect`, runs `execute_process(COMMAND <ZENITH_EXE> --list-test-cases --test-suite=<s> --no-version --no-colors=true …)` (doctest lists a suite's cases in every build; naming a suite lifts the test main's exclusion) and takes as case names the non-empty lines that do not begin with `[doctest]` and do not consist of `=` characters; a non-zero exit is `message(FATAL_ERROR "Zenith2: <X>: --list-test-cases --test-suite=<s> exited <code>")`, which CTest reports while reading the test list, so a broken executable never silently registers nothing;
   - (c) drops every name listed in `ZENITH_REGISTERED`, and every name of the form `<Library>/<Type>/<Behaviour>`, optionally followed by `[<Backend>]`, whose derived ID `Test_<Type>_<Behaviour>` (with the same suffix; `<Type>` the second `/`-separated component, `<Behaviour>` the third) is listed there, so a case named in the long form whose ID is registered is never registered twice (for an `ASSERTS_ONLY` case, never in `Release`);
   - (d) for each remaining case `<c>` of suite `<s>`: `add_test("X.<c>" "<ZENITH_EXE>" "--test-suite=<s>" "--test-case=<c>")` and `set_tests_properties("X.<c>" PROPERTIES PASS_REGULAR_EXPRESSION "${ZENITH_PASS_<s>}" LABELS "<s>;z2-discovered" TIMEOUT 60 ENVIRONMENT "ZENITH_EXPECTED_BUILD=${ZENITH_EXPECTED_BUILD}")` (the CTest-file forms), omitting every `z2-death-assert` case when `ZENITH_CONFIG` is not `Debug`, `Sanitize` or `Hwasan`; with `ZENITH_SCRIPT_MODE` set (the fixtures) both commands are recorded through `_zenith_registry_command` instead;
   - (e) writes the names (d) registered to `${ZENITH_OUT}/generated/registry/<X>.discovered-cases-<config>.txt` (empty when every case is registered; rewritten only when its bytes change), the review record of the cases that rely on the default.
5. **Pass patterns.** The SHIP block of the registry sets each suite's pattern once, from `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (table "Death and sanitizer-defect tests"): `ZENITH_SHIP_PASS_z2-death-check` `ZENITH (CHECK|FATAL|UNREACHABLE) FAILED`, `ZENITH_SHIP_PASS_z2-death-assert` `ZENITH ASSERT FAILED`, `ZENITH_SHIP_PASS_z2-sanitizer-defect` `ERROR: (AddressSanitizer|HWAddressSanitizer): `; step 3 copies them into each parameter file as `ZENITH_PASS_<suite>`. Cross-document item X247 (b) (pending) adds the `linux-tsan` patterns `WARNING: ThreadSanitizer: ` and `runtime error: ` to the `z2-sanitizer-defect` row; when it lands, `ZENITH_SHIP_PASS_z2-sanitizer-defect` is the one SHIP line that changes (CORE's `zenith_core_death_case` carries its own copy, CORE's to change). Until then a discovered `z2-sanitizer-defect` case in `linux-tsan` is held to the ASan/HWASan pattern, exactly as a registered one is.
6. A discovered `z2-death-check` case runs in every configuration, so a check compiled only with asserts enabled ([Z2-N-ARCH-179]'s asserts-only class) needs its explicit `zenith_core_death_case(… ASSERTS_ONLY)`; the discovered-cases file is where a reviewer sees that it is missing.

`CMake_Ship_DeathCaseDiscovery` covers the enumeration, the generated parameter files and the script (`-DZENITH_LIST_OUTPUT_<suite>=<file>` replaces running the executable, as MOD's runners do).

### Compile-fail registration

A compile-fail source is registered with CORE's function, exactly as Z2T-CORE-002 defines it: `zenith_core_compile_fail(NAME SOURCE [DEFINES <D>...] [EXPECT <regex>])`, `SOURCE` relative to `ZENITH_ROOT` and ending `_CompileFail.cpp.in`, configured `COPYONLY` to `${CMAKE_CURRENT_BINARY_DIR}/CompileFail/<NAME>.cpp`, built as the `EXCLUDE_FROM_ALL` executable `<NAME>_CompileFail` linking `ZenithCore`, registered as `<NAME>` with label `z2-compile-fail`, timeout 300, and `PASS_REGULAR_EXPRESSION <EXPECT>` or `WILL_FAIL TRUE` ([Z2-N-ARCH-249]). SHIP's own compile-fail test uses it: `zenith_core_compile_fail(CompileFail_Ship_DeprecatedUseIsError CMake/Tests/Ship/DeprecatedUse_CompileFail.cpp.in EXPECT "C4996|deprecated")` in the SHIP block. SHIP's earlier `zenith_register_compile_fail` is withdrawn. The tail adds the build-tree lock to every entry and checks that no handed-over source is unregistered (§ "Registry tail").

### Fuzz target registration

Fuzz targets are created by the scaffold's generic rule (Z2T-MOD-056: `_zenith_register_fuzz_targets()`, the registry's last line, `win-asan` with `ZENITH_SANITIZER` alone): for each `ZENITH_FUZZ_SOURCES` entry whose stem has no target yet, `Zenith_Fuzz_<Stem>` linking the library, `ZenithTestSupport` and its `TEST_THIRD_PARTY` with `/fsanitize=fuzzer`, and the smoke entry `Zenith_Fuzz_<Stem>.Smoke` (`-max_total_time=60`, the absolute corpus `<directory>/Corpus/<Stem>` when it exists, label `z2-fuzz`, timeout 120, `PASS_REGULAR_EXPRESSION "Done [0-9]+ runs in [0-9]+ second"`, `FAIL_REGULAR_EXPRESSION "ERROR: (AddressSanitizer|libFuzzer)"`), with the global property `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>` appended to its properties; an area helper's own target (`zenith_core_fuzz_target`, `zenith_assets_fuzz_target`) is left alone. SHIP's earlier `zenith_register_fuzz_targets` and its `_Smoke` name are withdrawn. The full-duration acceptance run invokes the same executables with `-max_total_time` of SEC's tier table, 3,600, 1,800 and 600 seconds for tiers 1, 2 and 3 at Stage 6 (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns"; SEC row SEC6 (a), which answers SH8). The rule's smoke command carries the options of § "Fuzz smoke options" after `-max_total_time=60`, a SHIP edit of the rule's body in `ZenithAddLibrary.cmake`.

### Tool-side fuzz targets

<!-- elaboration -->
The one registration form for a parser hosted in a tool target (SEC's specification: `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets"; ruling SEC-R1-RULINGS R2; row SEC6 (g)), defined in `Zenith2/CMake/ZenithAddTool.cmake` and called from the area blocks of the test registry:

```cmake
zenith_tool_fuzz_target(<Stem>
	TOOL <ToolTarget>                     # a row of the table below
	TOOL_DIR <directory>                  # that row's directory, relative to Zenith2/
	SOURCES <relative path>...            # each under <TOOL_DIR>/Internal/, never the tool's main TU
	DEPENDS <Library>...                  # each a library target present in win-asan
	[THIRD_PARTY <ManifestName>...])

# The ZenithCook wrapper, defined beside it; Z2T-ASSET-035, -039, -075, Z2T-ANIM-029 and Z2T-VFX-012 call it as written:
function(zenith_cook_fuzz_target STEM)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;DEPENDS;THIRD_PARTY")
	zenith_tool_fuzz_target(${STEM} TOOL ZenithCook TOOL_DIR Tools/ZenithCook
		SOURCES ${ARG_SOURCES} DEPENDS ${ARG_DEPENDS} THIRD_PARTY ${ARG_THIRD_PARTY})
endfunction()
```

| `TOOL` | `TOOL_DIR` | Kind |
|---|---|---|
| `ZenithCook` | `Tools/ZenithCook` | executable |
| `ZenithEditor` | `Tools/ZenithEditor` | executable |
| `ZenithAuthoring` | `Tools/Authoring` | static tool library (`ZENITH_WINDOWS`) |
| `ZenithEditorCore` | `Tools/EditorCore` | static tool library (`ZENITH_WINDOWS`) |
| `ZenithNewGameScaffold` | `Tools/NewGameScaffold` | static tool library (`ZENITH_WINDOWS`) |

Algorithm, in every preset whose configure includes the registry (`win`, `win-asan`, `linux-tsan`, [Z2-N-ARCH-252]):

1. An unknown keyword is `FATAL_ERROR "zenith_tool_fuzz_target(<Stem>): unknown argument <ARG>"`; `<Stem>` is checked against SEC's stem rule (`^[A-Z][A-Za-z0-9]*(_[A-Z][A-Za-z0-9]*)*$`, at most 48 characters, no `Fuzz` or `Zenith` prefix) with `Zenith2: zenith_tool_fuzz_target(<Stem>): <Stem> breaks the stem rule`.
2. `TOOL` is a row of the table and `TOOL_DIR` equals that row's directory, else `Zenith2: zenith_tool_fuzz_target(<Stem>): TOOL_DIR <value> is not the directory of <ToolTarget> (<expected>)`.
3. Append `<Stem>|<ToolTarget>|<SOURCES joined by ;>` to the global property `ZENITH_TOOL_FUZZ_TARGETS`; a stem already in it is `Zenith2: zenith_tool_fuzz_target(<Stem>): stem declared twice`. `zenith_end_build` writes the property as `toolFuzzTargets[]` (§ "declared_deps additions").
4. Unless the preset is `win-asan` with `ZENITH_SANITIZER` set, return.
5. `${ZENITH_ROOT}/<TOOL_DIR>/Tests/Fuzz/<Stem>.cpp` exists, else `Zenith2: zenith_tool_fuzz_target(<Stem>): <path> does not exist`.
6. Every `SOURCES` entry begins `Internal/`, exists under `${ZENITH_ROOT}/<TOOL_DIR>/`, and is not `Internal/Zenith_<ToolTarget>Main.cpp`, else `Zenith2: zenith_tool_fuzz_target(<Stem>): source <entry> <reason>` (`is not under Internal/`, `does not exist`, `is the tool's main`).
7. Every `DEPENDS` entry is a target that `zenith_add_library` or a `LIBRARY`-kind `zenith_add_tool` created in this preset, else `Zenith2: zenith_tool_fuzz_target(<Stem>): <Library> is not built in win-asan`; a tool executable is never valid (its sources are named in `SOURCES`).
8. `add_executable(Zenith_Fuzz_<Stem> <fuzz source> <SOURCES>)` (a stem the generic rule or an area helper already made is refused by the target name); `target_include_directories(… PRIVATE ${ZENITH_ROOT}/Tools ${ZENITH_ROOT}/<TOOL_DIR> ${ZENITH_ROOT}/<TOOL_DIR>/Internal)`; link the `DEPENDS`, `ZenithTestSupport` and the `autoLink` test entries; resolve `THIRD_PARTY` as a test target does (§ "Name resolution"); `/fsanitize=fuzzer` at compile and link; `zenith_apply_flags`; `zenith_stage_runtime_files(… TEST)`.
9. `add_test(NAME Zenith_Fuzz_<Stem>.Smoke COMMAND Zenith_Fuzz_<Stem> -max_total_time=60 <options> [<corpus>])`, `<options>` from `zenith_fuzz_smoke_args(<Stem> ${ZENITH_ROOT}/<TOOL_DIR>/Tests/Fuzz …)` and `<corpus>` the absolute `${ZENITH_ROOT}/<TOOL_DIR>/Tests/Fuzz/Corpus/<Stem>` when it exists; the generic rule's properties (label `z2-fuzz`, timeout 120, its pass and fail expressions), the environment of § "Fuzz smoke options" and `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>`.

SecurityConformance check SC10 reads `toolFuzzTargets[]` of the `win` tree's `declared_deps.json`; Android presets include no registry, so their `declared_deps.json` carries `toolFuzzTargets: []`. `CMake_Sec_ToolFuzzTargetDeclarations` is SEC's test name for this helper (Z2-R-SEC-044), registered in the SHIP block.

### Fuzz smoke options

<!-- elaboration -->
Every `Zenith_Fuzz_<Stem>.Smoke` runs with SEC's smoke options (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "win-asan with libFuzzer"; Z2-R-SEC-045, Z2-R-SEC-116; row SEC6 (b)):

- `zenith_fuzz_smoke_args(<Stem> <FuzzDir> <OutVar>)` (`ZenithAddLibrary.cmake`) returns `-rss_limit_mb=2048;-timeout=10;-max_len=<maxLen>`, plus `-dict=<FuzzDir>/<Stem>.dict` when that file exists. `<maxLen>` is the largest `fuzz.maxLen` of the `parsers[]` entries of `${ZENITH_ROOT}/Tools/Security/parser_inventory.json` whose `fuzz.stem` is `<Stem>`, or 65,536 when the file or such an entry is absent (the default of SEC's registry). The generic rule, `zenith_tool_fuzz_target` and every area helper that creates a smoke (CORE's `zenith_core_fuzz_target`, ASSET's `zenith_assets_fuzz_target`; edges those areas add) put the list after `-max_total_time=60`.
- The cache variable `ZENITH_ASAN_SUPPRESSIONS` (`FILEPATH`, default `${ZENITH_ROOT}/Tools/Gates/Suppressions/asan.supp`, the file PROC's `Sanitizers` step passes) gives the one suppression path; the smoke environment value is `ASAN_OPTIONS=halt_on_error=1:detect_leaks=0:suppressions=<ZENITH_ASAN_SUPPRESSIONS>:abort_on_error=1:allocator_may_return_null=1:symbolize=1`.
- The registry tail applies that value (step 5 of § "Registry tail"): an area-made or tool-made smoke receives it through `set_tests_properties`, a smoke the generic rule creates afterwards through `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>`, and in both cases the one `ENVIRONMENT` key holds `ASAN_OPTIONS=…` first and then every `ENVIRONMENT` value the fixture-set hook already carried, in order, because a second `ENVIRONMENT` key in one `set_tests_properties` call would replace the first.


### zenith_register_script_test

```cmake
zenith_register_script_test(<TestName> SCRIPT <path.cmake> [HOST_PWSH] [ARGS <-Dvar=value>...] [LABELS <Label>...] [TIMEOUT <s>] [CONFIGURATIONS <Config>...])
zenith_register_pwsh_test(<TestName> SCRIPT <path.ps1> [ARGS <arg>...] [LABELS <Label>...] [TIMEOUT <s>] [CONFIGURATIONS <Config>...])
```

- `zenith_register_script_test` registers `add_test(NAME <TestName> COMMAND ${CMAKE_COMMAND} -DZENITH_ROOT=${ZENITH_ROOT} -DZENITH_OUT=${ZENITH_OUT} <ARGS> -P <path>)` in every test preset, label `z2-cmake` plus `LABELS`, timeout 60 unless given; the Stage 1 JSON evidence checks of the orchestrator's 2026-09-26 ruling and SCRIPT's `Zenith_Script_LuauPinRecord_Check` use it.
- `zenith_register_pwsh_test` registers `add_test(NAME <TestName> COMMAND <pinned pwsh> -NoProfile -File <path> <ARGS>)` in the `win` preset only, label `z2-pwsh` plus `LABELS`, timeout 120 unless given, with `ZENITH_REPO_ROOT` and `ZENITH_OUT` in the test environment.
- `HOST_PWSH` marks a script-mode test whose script runs PowerShell (SEC row SEC6 (i)): it follows the pwsh rule above and is registered only when `ZENITH_PRESET` is `win`, and its command gains `-DZENITH_PWSH=<pinned pwsh>` before `-P`, so the script runs the pinned interpreter and never searches `PATH`. SEC's block registers `CMake_Sec_UpdateKeyResolution` with it; `CMake_Sec_PublishedKeysAgree`, pure CMake, is registered without it and runs in every test preset.
- `CONFIGURATIONS` is passed as `add_test(… CONFIGURATIONS <Config>...)`, so the entry exists only in those configurations (`Pwsh_Ship_ReleaseChecks_WindowsBinary` uses `CONFIGURATIONS Release`); without it the entry exists in every configuration. A test that needs a built executable takes it as an argument (`ARGS -Exe $<TARGET_FILE:Minimal>`); a CTest entry cannot depend on a build, so the file exists because the `Build` step builds `Minimal` before the `Test` step runs.
- Both carry `ZENITH_EXPECTED_BUILD` (§ "Test environment").

### zenith_register_fixture_set

```cmake
zenith_register_fixture_set(<AREA> <Set> <Target> <SetupCase> <Variable> [CONSUMER_SUITES <suite>...] [FUZZ_CONSUMERS <Stem>...])
```

Implements [Z2-N-ARCH-257] once for every area: registers `<AREA>_<Set>Fixtures` (`<Target> --test-suite=z2-fixture-setup --test-case=<SetupCase>`, `FIXTURES_SETUP <AREA><Set>Fixtures`), one `<Target>[<suite>]` entry per consumer suite with `FIXTURES_REQUIRED`, and gives every one of them `ENVIRONMENT <Variable>=${ZENITH_OUT}/test-fixtures/<AREA>`. For each `FUZZ_CONSUMERS` stem it appends `FIXTURES_REQUIRED <AREA><Set>Fixtures ENVIRONMENT <Variable>=…` to the global property `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>`, the scaffold rule's hook, so a rule-made smoke receives them; the registry tail applies the same property to an area-made smoke.

### Registry tail

<!-- elaboration -->
`_zenith_ship_registry_tail()` (defined in `ZenithAddLibrary.cmake`) is one call that SHIP's ticket T5 places in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` after every area block and immediately before the scaffold's `_zenith_register_fuzz_targets()` line (after MOD's `_zenith_register_suite_content()` where that exists, Z2T-MOD-066), so every area's registrations exist when it runs and it stays inside the registry ([Z2-N-ARCH-252]). In `ZENITH_HOST` presets it performs, in order:

1. **Unlisted death-case discovery** (§ "Unlisted death-case discovery").
2. **Build-tree lock**: every test of the registry's directory that runs `cmake --build` in the build tree gains `RESOURCE_LOCK zenith2-build-tree`, appended to any lock it has, so a parallel `ctest -j` never runs two `cmake --build` processes in one build tree and a failure caused by such concurrency can never be mistaken for the expected one. CMake exposes no test's command as a property, so two keys find them: (a) the command key, a scan of the registry file's text (`file(READ)` of `${ZENITH_ROOT}/CMake/Tests/ZenithCMakeTests.cmake`, the fixture's own copy in script mode) with `string(REGEX MATCHALL)` for every `add_test(NAME <n> COMMAND ${CMAKE_COMMAND} --build`, any spaces, tabs and line breaks between the tokens, whose `<n>` is a literal name (no `${`) for which `if(TEST <n>)` holds: RPROD's `Test_RenderGraph_TrivialCaptureRequired`, its twin `Test_RenderGraph_TrivialCaptureTwinCompiles` and `Test_PassContext_ExposesNoBarrierAPI` (Z2T-RPROD-016, -030), which carry no `z2-compile-fail` label; (b) the label key, every test whose `LABELS` contain `z2-compile-fail` (CORE's `zenith_core_compile_fail` entries, whose `add_test(NAME ${NAME} …)` line the command key skips because the name is computed). A test both keys find is locked once. RPROD's form is routed to RPROD (cross-area row SH23); the lock covers it either way.
3. **Compile-fail source check**: for each `<Lib>|<path>` of `ZENITH_COMPILE_FAIL_SOURCES`, some file under `${CMAKE_BINARY_DIR}/CompileFail/` must have the same SHA-256 as `<path>` (CORE's function copies the source byte for byte, `COPYONLY`); otherwise `FATAL_ERROR "Zenith2: <path relative to ZENITH_ROOT>: compile-fail source not registered by any block of CMake/Tests/ZenithCMakeTests.cmake"`. A source registered twice with different `DEFINES` (Z2T-CORE-029) matches either copy.
4. **Fixture properties of area-made fuzz smokes**: for every stem with `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>` set whose `Zenith_Fuzz_<Stem>.Smoke` already exists (`if(TEST …)`, an area helper's smoke), `set_tests_properties` with that value; the scaffold rule that follows applies it to the smokes it creates and skips the area-made ones, so no smoke receives it twice.
5. **Smoke sanitizer environment** (`win-asan` only): for every test of the registry's directory named `Zenith_Fuzz_<Stem>.Smoke`, the `ENVIRONMENT` of § "Fuzz smoke options", merged with the `ENVIRONMENT` it already has; for every stem of `ZENITH_FUZZ_SOURCES` with no such test yet, `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>` rewritten so its one `ENVIRONMENT` key carries the same merged value, which the generic rule then applies.

Each step goes through the scaffold's recorder `_zenith_registry_command`, so `CMake_Ship_RegistryTail` asserts it in script mode (step 5 by `CMake_Ship_FuzzSmokeOptions`), including that the tail records no `add_custom_command(TARGET …)`.

## declared_deps additions

<!-- elaboration -->
`declared_deps.json` (schema `zenith2.declared-deps`, [Z2-N-ARCH-182]) gains, without a `schemaVersion` change because no existing field changes meaning (cross-document item X220 item 7, applied):

| Record | Field | Content |
|---|---|---|
| `libraries[]` | `testThirdParty` | the `TEST_THIRD_PARTY` names, byte-ordered |
| `libraries[]` | `conformanceLabels` | the `CONFORMANCE_LABELS`, byte-ordered |
| `tools[]` | `testThirdParty` | as above |
| `benchmarks[]` (new array) | `name`, `dir`, `depends`, `thirdParty`, `supported` | one record per `zenith_add_benchmark` |
| `integrationSuites[]` (new array) | `name`, `dir`, `depends`, `thirdParty`, `supported` | one record per `zenith_add_integration_tests` |
| `toolFuzzTargets[]` (new array) | `stem`, `tool`, `sources` | one record per `zenith_tool_fuzz_target` call, sorted by `stem`, `sources` in call order (SEC row SEC6 (c)); `[]` in Android presets |
| `libraries[]` | `unavailableThirdParty` | the byte-ordered `THIRD_PARTY` names that kept the library from being created in this configure (§ "Unavailable entries" under "Name resolution"); `[]` otherwise |

Libgraph's third-party ownership rule reads `thirdParty` and `testThirdParty` of every record: an include of a dependency's header is permitted from `Internal/` of the entry's owner and from any test, benchmark or tool target that names the entry (cross-area row SH6).
