# Architecture — Build system and presets

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Build system and presets". Normative statements carry `[Z2-N-ARCH-NNN]` tags numbered in the range 151–259, shared with the entry document's numbering.

<!-- informative -->
Zenith generated twelve Visual Studio configurations per game through Sharpmake, with explicit per-configuration source excludes and a hand-maintained library table. Zenith2 replaces all of it with one CMake tree (Z2-D-001): directory ownership decides which target a file belongs to, a platform directory decides which platform compiles it, and a preset decides the toolchain. Nothing is listed by hand except a library's declared dependencies.
<!-- /informative -->

## Settled rules governing the build

The build is governed by these settled decisions: Z2-D-001 (CMake presets, Ninja Multi-Config, globbed sources, one library per backend), Z2-D-002 (games in-repo, statically linked, no C++), Z2-D-008 (reproducibility manifest), Z2-D-009 (C++20), Z2-D-010 (platforms), Z2-D-011 (sanitizer presets), Z2-D-017 (the Luau exception firewall), Z2-D-024 (static modules), Z2-D-025 (offline shader compilation), Z2-D-028 (gate budget), Z2-D-081 (one executable per game), Z2-D-082 (third-party pinning and confinement).

- [Z2-N-ARCH-151] The build system is CMake driven exclusively through `Zenith2/CMakePresets.json`, and every configure preset uses the `Ninja Multi-Config` generator.
- [Z2-N-ARCH-152] Source files are discovered by `file(GLOB_RECURSE … CONFIGURE_DEPENDS)` inside the shared helpers; no `CMakeLists.txt` in the tree lists a source file by name.
- [Z2-N-ARCH-153] Ninja's automatic CMake re-run is the one regeneration mechanism; no script, gate step or developer workflow regenerates the build by any other means.
- [Z2-N-ARCH-154] The language standard is C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`) for every Zenith2 target; third-party targets keep the standard their own build files select.
- [Z2-N-ARCH-155] Every backend (ECS, RHI, physics, compression, online services) is its own library target, and the consumer selects a backend by linking it; no preprocessor symbol selects a backend.

## Top-level tree and helper modules

The root `Zenith2/CMakeLists.txt` has exactly this shape:

```cmake
cmake_minimum_required(VERSION 3.28)
project(Zenith2 LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
include(ZenithBuild)                    # zenith_begin_build / zenith_end_build and the other helpers

zenith_begin_build()                    # checks ZENITH_CHECKOUT_HASH, the preset, the toolchain pins
add_subdirectory(ThirdParty)            # manifest-driven third-party targets
zenith_add_subdirectories(Engine)       # every Engine/<Library>/CMakeLists.txt, globbed
zenith_add_subdirectories(Player)       # Player/CMakeLists.txt (the ZenithPlayer library)
zenith_add_subdirectories(Tools)        # every Tools/<Tool>/CMakeLists.txt that exists
zenith_add_subdirectories(Benchmarks GROUPS Budgets)   # every Benchmarks/<Harness>/ and Benchmarks/Budgets/<AREA>/CMakeLists.txt
zenith_add_subdirectories(Tests)        # every Tests/<Suite>/CMakeLists.txt (integration suites)
zenith_add_subdirectories(Games)        # every Games/<Name>/CMakeLists.txt
zenith_end_build()                      # writes declared_deps.json and the coverage manifest
```

<!-- elaboration -->
The helper modules live in `Zenith2/CMake/`:

| File | Contents |
|---|---|
| `CMake/ZenithBuild.cmake` | `zenith_begin_build`, `zenith_end_build`, `zenith_add_subdirectories`; includes every file below that exists (the Stage 0 scaffold creates all but `ZenithShaders.cmake`) |
| `CMake/ZenithAddLibrary.cmake` | `zenith_add_library` |
| `CMake/ZenithAddTool.cmake` | `zenith_add_tool`, `zenith_add_benchmark`, `zenith_add_integration_tests` |
| `CMake/ZenithAddGame.cmake` | `zenith_add_game`, the `game.json` validator, and the Android wiring: Gradle project generation and the `<Game>_Apk` target (step 9 of § "The zenith_add_game helper"; Zenith2/Docs/Design/MOD/GameBuild.md → "Gradle project generation") |
| `CMake/ZenithWarnings.cmake` | the warning flag sets of Zenith2/Docs/Contracts.md → "Tests and code hygiene gates", applied to every Zenith2 target |
| `CMake/ZenithFlags.cmake` | per-toolchain, per-configuration compile and link flags other than warnings (tables below) |
| `CMake/ZenithSupported.cmake` | the `ZENITH_SUPPORTED` matrix macros and evaluation |
| `CMake/ZenithThirdParty.cmake` | reads `ThirdParty/manifest.json`, resolves the dependency cache, creates targets |
| `CMake/ZenithShaders.cmake` | the shader custom commands behind the `SHADERS` option of `ZenithShadersGenerated`, including the `--stream-sets` aggregate command; created by the ZenithShaderCompiler ticket, its first consumer |
| `CMake/ZenithBudgetTable.cmake` | `zenith_generate_budget_table(<BudgetsMd> <ArchitectureMd> <OutputHeader>)`, the generator of the memory governor's budget header from Zenith2/Docs/Budgets.md → "Row index" (content CORE's, `Zenith2/Docs/Design/CORE/MemoryGovernor.md`; placement SHIP's) |
| `CMake/Tests/ZenithCMakeTests.cmake` | the test registry: the CMake script-mode fixture tests, the compile-fail tests, the death and sanitizer-defect CTest entries and the fuzz targets (§ "Test, conformance and fuzz targets"); included by `zenith_end_build` in every configure preset that has a test preset (`win`, `win-asan`, `linux-tsan`) and in no Android preset |
| `CMake/toolchains/msvc-x64.cmake` | MSVC toolchain file: compiler pin check, sccache launcher |
| `CMake/toolchains/clang-linux-x64.cmake` | WSL/Linux clang toolchain file: compiler pin check, sccache launcher |

- [Z2-N-ARCH-156] `zenith_add_subdirectories(<Dir> [GROUPS <Group>...])` globs `<Dir>/*/CMakeLists.txt` (and `<Dir>/CMakeLists.txt` when present) and, for each named group, `<Dir>/<Group>/*/CMakeLists.txt`, with `CONFIGURE_DEPENDS`, merges and sorts the result by byte order, and calls `add_subdirectory` in that order, so adding a library, harness or game directory never edits a parent `CMakeLists.txt`; a `CMakeLists.txt` directly inside a group directory is a configure error, `zenith_add_subdirectories(<Dir>): <Dir>/<Group> is a group directory and holds no CMakeLists.txt`.
- [Z2-N-ARCH-157] Every `CMakeLists.txt` below `Engine/`, `Player/`, `Tools/`, `Benchmarks/`, `Tests/` and `Games/` consists of exactly one helper call (`zenith_add_library`, `zenith_add_tool`, `zenith_add_benchmark`, `zenith_add_integration_tests` or `zenith_add_game`); the `SourceLayout` gate step fails on any other command in those files.
- [Z2-N-ARCH-158] `zenith_begin_build` stops the configure with `FATAL_ERROR` when `$ENV{ZENITH_CHECKOUT_HASH}` does not match `^[0-9a-f]{16}$`, printing `Zenith2: ZENITH_CHECKOUT_HASH is unset or malformed; configure through Tools/Gates/Configure.ps1 or run Tools/Setup/Enter-Zenith2Env.ps1 first`.

### Checkout hash and output root

- [Z2-N-ARCH-159] The checkout hash is the first 16 lower-case hexadecimal digits of the SHA-256 of the checkout root's absolute path, lower-cased, with `\` replaced by `/` and no trailing slash, encoded as UTF-8.
- [Z2-N-ARCH-226] `Tools/Setup/Enter-Zenith2Env.ps1` and every gate step compute the checkout hash with the one function `Get-Zenith2CheckoutHash`, defined once in `Zenith2/Tools/Setup/Zenith2Env.psm1`.
- [Z2-N-ARCH-160] Every build tree, report, generated file and cooked output lives under `Zenith2/out/<checkout-hash>/` (Windows presets) or `$HOME/.zenith2/out/<checkout-hash>/` (the `linux-tsan` preset), and nothing a build or gate step produces is written anywhere else inside the checkout.

The resulting output layout (`ZENITH_OUT` is `Zenith2/out/<checkout-hash>` on Windows):

| Path | Contents |
|---|---|
| `${ZENITH_OUT}/<configure-preset>/` | the CMake binary directory of that configure preset |
| `${ZENITH_OUT}/<configure-preset>/<Config>/bin/` | executables and shared libraries of that configuration |
| `${ZENITH_OUT}/<configure-preset>/<Config>/lib/` | static libraries |
| `${ZENITH_OUT}/declared_deps.json` | the declared-dependency record (§ "The declared_deps.json record") |
| `${ZENITH_OUT}/generated/<Library>/` | files generated for a library (for `ZenithShadersGenerated`: shader blobs, reflection, parameter headers, vertex layout tables) |
| `${ZENITH_OUT}/Games/<Name>/` | a game's platform-independent `composition.json` |
| `${ZENITH_OUT}/Games/<Name>/<configure-preset>/` | that preset's generated composition TU, its `android/` Gradle project and its `jni/` staging |
| `${ZENITH_OUT}/Generated/Luau/<Name>/` | a game's generated `.d.luau` files and `.luaurc` |
| `${ZENITH_OUT}/Cooked/<Name>/<platform>/` | cooked content of a game |
| `${ZENITH_OUT}/gates/` | gate step reports (libgraph, complexity, coverage, sanitizers, tests) |
| `${ZENITH_OUT}/test-fixtures/<AREA>/` | fixture sets generated at test time by `z2-fixture-setup` entries ([Z2-N-ARCH-257]) |

The nested `Zenith2/.gitignore` ignores `out/`, so no build output is ever an untracked file that could change a tree hash.

## The zenith_add_library helper

### Signature

```cmake
zenith_add_library(<Name>
	[KIND <ENGINE|MODULE|BACKEND|PLAYER|GENERATED>]     # default ENGINE
	[DEPENDS <Library>...]                              # declared DIRECT Zenith2 library dependencies
	[THIRD_PARTY <ManifestName>...]                     # ThirdParty/manifest.json entries this library owns
	[SUPPORTED <MatrixEntry>...]                        # the ZENITH_SUPPORTED matrix; default ZENITH_ALL
	[EXCEPTIONS <OFF|INTERNAL>]                         # default OFF; INTERNAL: ZenithScript and ZenithScriptCompiler
	[SHADERS <Directory>]                               # KIND GENERATED alone: the Slang root compiled offline
	[GENERATES <File>...]                               # extra generated files attributed to this library
	[MODULE_NAME <Name>]                                # KIND MODULE: the name game.json uses
	[BACKEND_OF <ApiLibrary>]                           # KIND BACKEND: the API library this backend implements
	[BACKEND_NAME <Name>]                               # KIND BACKEND: the value game.json uses
	[FACTORY <Symbol>]                                  # KIND BACKEND: the exported factory function
	[TEST_DEPENDS <Target>...]                          # extra link targets of <Name>_Tests and <Name>'s conformance adapters
	[TEST_LABELS <Label>...]                            # CTest labels applied to every test discovered from <Name>_Tests
	[TEST_THIRD_PARTY <ManifestName>...]                # usage-tests manifest entries linked into <Name>'s test targets only
	[CONFORMANCE_LABELS <Label>...]                     # CTest labels applied to every test discovered from <Name>'s conformance executable
)
```

| Argument | Type | Rule |
|---|---|---|
| `<Name>` | identifier | equals the directory name under `Engine/` and matches `^Zenith[A-Za-z0-9]+(_[A-Za-z0-9]+)?$`; the one exemption is `KIND PLAYER`, whose target `ZenithPlayer` lives in `Zenith2/Player/` with include root `Zenith2/` |
| `KIND` | enum | `ENGINE` (engine library, including a backend family's core library under [Z2-N-ARCH-075]), `MODULE` (optional engine module, see Architecture.md → "Engine modules and game projects"), `BACKEND` (one backend of an API library), `PLAYER` (the shared Player library, one instance), `GENERATED` (an `INTERFACE` target that owns generated headers: `ZenithShadersGenerated`, one instance) |
| `DEPENDS` | list | Zenith2 library names; each becomes a `PUBLIC` link and a declared edge |
| `THIRD_PARTY` | list | manifest `name` values; each becomes a `PRIVATE` link and `SYSTEM` include directory |
| `SUPPORTED` | list | matrix entries (§ "The ZENITH_SUPPORTED matrix") |
| `EXCEPTIONS` | enum | `OFF` or `INTERNAL` |
| `SHADERS` | path | accepted for `KIND GENERATED` alone: `Zenith2/Shaders/`, whose programs the host shader compiler compiles |
| `GENERATES` | list | generated file paths, relative to `${ZENITH_OUT}/generated/<Name>/` |
| `MODULE_NAME` | identifier | required for, and exclusive to, `KIND MODULE`; matches `^[A-Z][A-Za-z0-9]*$` |
| `TEST_DEPENDS` | list | Zenith2 library or tool-library targets linked into `<Name>_Tests`, into `<Name>`'s `Tests/ConformanceAdapter/**` or `Tests/GraphConformance/**` sources, and into `<Name>_Conformance` when `<Name>` is not a backend family's API library ([Z2-N-ARCH-256]), and nowhere else; recorded as `testDepends` in `declared_deps.json`; for example `zenith_add_library(ZenithFlux … TEST_DEPENDS ZenithPathTracer)`. Only the entries supported in the current preset are linked: an entry the preset lacks is skipped, never removes `<Name>_Tests` (or `<Name>_Conformance`) from the preset and is never a configure error ([Z2-N-ARCH-172]); each entry gives the targets it links into the compile definition `ZENITH_TEST_LINKS_<TARGET>` (the target name in upper case, for example `ZENITH_TEST_LINKS_ZENITHRHI_D3D12`), `1` where it is linked and `0` where the preset lacks it, so test sources select the linked backends with `#if ZENITH_TEST_LINKS_<TARGET>` and never with a platform macro |
| `TEST_LABELS` | list | CTest labels, each `^[a-z][a-z0-9-]*$`, applied to every CTest test discovered from `<Name>_Tests` through the discovery call's `LABELS` property ([Z2-N-ARCH-251]; a configure-time `set_tests_properties` cannot name a discovered test); for example `zenith_add_library(ZenithCore TEST_LABELS memory)`, which the `Zenith2Memory` gate line `Test.ps1 -Label memory` selects |
| `TEST_THIRD_PARTY` | list | manifest `name` values with `usage: tests`; linked into `<Name>_Tests`, `<Name>`'s conformance executable (the family's, for an API library) and `<Name>`'s fuzz targets, never into `<Name>`; recorded as `testThirdParty` in `declared_deps.json` (RPROD's `flip`) |
| `CONFORMANCE_LABELS` | list | CTest labels, each `^[a-z][a-z0-9-]*$`, applied to every test discovered from the conformance executable `<Name>` creates or contributes to (`zenith_add_library(ZenithRHI … CONFORMANCE_LABELS z2-parity-backend)`); recorded as `conformanceLabels` in `declared_deps.json` |
| `BACKEND_OF` | library | required for, and exclusive to, `KIND BACKEND` |
| `BACKEND_NAME` | identifier | required for `KIND BACKEND`; matches `^[A-Z][A-Za-z0-9]*$` |
| `FACTORY` | symbol | required for `KIND BACKEND`; matches `^Zenith_Create[A-Za-z]+_[A-Za-z0-9]+$` |

A library's whole `CMakeLists.txt` is, for example:

```cmake
zenith_add_library(ZenithJobs DEPENDS ZenithCore ZenithGraph ZenithProfiling)
```

```cmake
zenith_add_library(ZenithECS_Flecs KIND BACKEND BACKEND_OF ZenithECS BACKEND_NAME Flecs
	FACTORY Zenith_CreateWorld_Flecs
	DEPENDS ZenithECS ZenithReflect ZenithCore
	THIRD_PARTY flecs)
```

### Directory rules and globbing

The helper globs `*.cpp *.c *.h *.hpp *.inl` recursively under the library directory with `CONFIGURE_DEPENDS` and `LIST_DIRECTORIES false`, sorts the result by byte order, and partitions it by path relative to the library directory:

| Relative path (glob partition) | Destination |
|---|---|
| `<file>` at the library root | the library; headers here are its public API |
| `Internal/Windows/**` | the library, when `ZENITH_TARGET_OS` is `Windows` |
| `Internal/Android/**` | the library, when `ZENITH_TARGET_OS` is `Android` |
| `Internal/Linux/**` | the library, when `ZENITH_TARGET_OS` is `Linux` |
| `Internal/**` (other) | the library, private |
| `Tests/Conformance/**` | the family conformance executable of a backend family's API library, or `<Name>_Conformance` for any other library (§ "Test, conformance and fuzz targets") |
| `Tests/ConformanceAdapter/**` | this backend's adapter inside the family conformance executable |
| `Tests/GraphConformance/**` | this library's adapter inside `ZenithGraph_Conformance` |
| `Tests/Fuzz/**` | one fuzz executable per `.cpp`, created by the test registry `CMake/Tests/ZenithCMakeTests.cmake` in the `win-asan` preset alone |
| `Tests/Support/**` (ZenithCore alone) | the `ZenithTestSupport` static library (test main, counting types, property generators, fault-injecting heap) |
| `Tests/Support/**` (any other library) | no target: header-only shared test helpers (`inline` functions and templates), included by that library's test, conformance and fuzz sources, by dependent libraries' tests and by benchmark sources (`Benchmarks/<Harness>/**`, `Benchmarks/Budgets/<AREA>/**`); a compiled-language source there is a configure error ([Z2-N-ARCH-255]) |
| `Tests/**` (other) | the `<Name>_Tests` executable |

- [Z2-N-ARCH-161] Public headers of a library sit directly in the library's root directory; a subdirectory of a library other than `Internal/` and `Tests/` that contains a compiled-language source is a configure error (non-source directories such as the Player's `GradleTemplate/` are ignored by the glob), `zenith_add_library(<Name>): directory '<dir>' is not allowed; public headers live at the library root, private code under Internal/`. One signposted carve-out exists: ZenithCore's three per-platform threading headers `Platform/Windows/Zenith_PlatformThreading.h`, `Platform/Android/Zenith_PlatformThreading.h` and `Platform/Linux/Zenith_PlatformThreading.h`, and no other file under `Platform/` (`Zenith2/Docs/Contracts.md` → "Library directory layout" [Z2-N-CONTRACT-583], [Z2-N-CONTRACT-584]; Z2-D-141).
- [Z2-N-ARCH-162] A directory named `Windows`, `Android` or `Linux` containing a compiled-language source anywhere other than directly below `Internal/` is a configure error, and platform-specific code lives only under `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/`, except ZenithCore's three per-platform threading headers under `Platform/<Platform>/` (the carve-out of [Z2-N-ARCH-161], [Z2-N-CONTRACT-583]), for which `zenith_add_library()` adds `${CMAKE_CURRENT_SOURCE_DIR}/Platform/${CMAKE_SYSTEM_NAME}` to ZenithCore's public include directories; a `Tests/` tree therefore has no platform partition, and test code that needs a platform API calls ZenithTestSupport's child-process and own-executable facility (`Zenith_RunChildTestCase`, `Zenith_TestProcess`, `Zenith_GetTestExecutablePath` in `ZenithCore/Tests/Support/Zenith_TestProcess.h`), whose platform bodies are ordinary ZenithCore `Internal/<Platform>/` files (ruling X349 (a); Zenith2/Docs/Design/CORE/Memory.md → "Test child processes").
- [Z2-N-ARCH-163] Test code lives only under a library's `Tests/` directory (or an integration suite under `Zenith2/Tests/`), and nothing under `Tests/` is ever compiled into the library target itself.
- [Z2-N-ARCH-164] The include root of every engine library is `Zenith2/Engine`, so every include of an engine header reads `#include "<Library>/<Header>.h"`, except the bare `#include "Zenith_PlatformThreading.h"` of ZenithCore's `Zenith_Mutex.h`, `Zenith_ConditionVariable.h` and `Zenith_Semaphore.h`, resolved through the per-platform include directory of [Z2-N-ARCH-162] (the signposted carve-out of `Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-583], [Z2-N-CONTRACT-584]); tool libraries use `Zenith2/Tools` the same way, and the Player library uses `Zenith2/` so its headers read `#include "Player/<Header>.h"`.
- [Z2-N-ARCH-165] A third-party header is included only from files under the owning library's `Internal/` directory; libgraph's leakage rule and `ForbiddenCheck` both fail an include of a manifest dependency's header from any other file.
- [Z2-N-ARCH-227] The helper partitions globbed files exactly as the glob-partition table above states, including fuzz executables created in the `win-asan` preset alone, `Tests/Support/**` built into `ZenithTestSupport` from ZenithCore alone, and every other library's `Tests/Support/**` compiled into no target ([Z2-N-ARCH-255]); compile-fail sources are named `*_CompileFail.cpp.in`, which the `*.cpp` glob never matches, so no library, test or fuzz target compiles them.
- [Z2-N-ARCH-255] Only ZenithCore's `Tests/Support/` is compiled (into `ZenithTestSupport`: the shared test main, counting types, property generators and fault-injecting heap of the table above). The `Tests/Support/` directory of any other library holds header-only helpers (`inline` functions and templates) and is never compiled on its own: it is excluded from `<Name>_Tests`, from the conformance executables and from fuzz targets, whose sources include its headers through the include root ([Z2-N-ARCH-164]), as do dependent libraries' tests and the sources of every `zenith_add_benchmark` target (`Benchmarks/<Harness>/**` and `Benchmarks/Budgets/<AREA>/**`, the two benchmark directory forms of [Z2-N-ARCH-156]); a `.cpp` or `.c` file under it is the configure error `zenith_add_library(<Name>): Tests/Support/ holds headers only outside ZenithCore; move '<file>' under Tests/ or make it header-only`. Test code compiled into one executable alone lives elsewhere under `Tests/` (for example `Tests/Zenith_<Name>TestTypes.cpp` in `<Name>_Tests`, or `Tests/Conformance/` for a conformance fixture).
<!-- elaboration -->
- [Z2-N-ARCH-238] `ZenithShadersGenerated` (`Zenith2/Engine/ZenithShadersGenerated/`, `KIND GENERATED`) is the one owner of the shader-compile commands and the generated shader headers; every library that includes a generated shader header (ZenithFlux and each rendering module) names it in `DEPENDS`, and libgraph attributes every generated header to it (Zenith2/Docs/Contracts.md → "Layering DAG").

### Target creation steps

`zenith_add_library` performs these steps in order:

1. Validate the arguments (table above); any violation is a `FATAL_ERROR` naming the argument.
2. Append the declaration to the global property `ZENITH_DECLARATIONS` (this happens before step 3, so a library unsupported in the current preset is still recorded).
3. Evaluate `SUPPORTED` against `ZENITH_PRESET`; when the current configure preset is outside the matrix, return without creating any target. When a `THIRD_PARTY` entry is a manual-download prebuilt absent from the dependency cache (a partner-gated entry such as `steamworks-sdk`), the declaration is recorded with `unavailable: "<entry>"` and the call returns without creating any target: the library is declared-but-unavailable rather than skipped by a conditional call, which would be the per-configuration exclusion of [Z2-N-ARCH-171].
3a. For `KIND GENERATED` alone: `add_library(<Name> INTERFACE)`; `target_include_directories(<Name> INTERFACE ${ZENITH_OUT}/generated/<Name>)`; `target_link_libraries(<Name> INTERFACE <DEPENDS…>)`; `zenith_add_shaders(<Name> <SHADERS dir>)`, which adds one offline compile command per shader program with its outputs (blob container, reflection, `<Program>.generated.h`) under `${ZENITH_OUT}/generated/<Name>/`, one custom target `<Name>_Build` depending on every output, and `add_dependencies(<Name> <Name>_Build)`; record the declaration and every generated file for `declared_deps.json`; return — steps 4 to 12 do not run for this kind (no sources, no flags, no test targets).
4. Glob and partition sources (table above).
5. `add_library(<Name> STATIC <sources>)`; set `FOLDER`, `OUTPUT_NAME`, and the output directories of § "Checkout hash and output root".
6. `target_include_directories(<Name> PUBLIC <include-root>)` and, per `THIRD_PARTY` entry, `target_include_directories(<Name> SYSTEM PRIVATE <dependency include directories>)`.
7. `target_link_libraries(<Name> PUBLIC <DEPENDS…> PRIVATE <third-party targets…>)`.
8. Apply `zenith_apply_flags(<Name>)` (§ "Compile and link flags").
9. When `EXCEPTIONS INTERNAL`: create the object library `<Name>_EH` from the `Internal/**` partition, compile it with exceptions enabled, and add `$<TARGET_OBJECTS:<Name>_EH>` to `<Name>`.
10. When `GENERATES` is set: add the listed files' generating custom commands (from the library's own inputs) and attribute them to `<Name>`.
11. Create the test targets (§ "Test, conformance and fuzz targets"; the conformance executables are created by `zenith_end_build` once every declaration is known, [Z2-N-ARCH-256]), linking the `TEST_DEPENDS` targets supported in this preset into `<Name>_Tests` with their `ZENITH_TEST_LINKS_<TARGET>` definitions, giving every test target the compile definitions `ZENITH_TEST_FIXTURE_ROOT`, `ZENITH_REPO_ROOT`, `ZENITH_TEST_GENERATED_ROOT` and `ZENITH_SHADERS_GENERATED_DIR` ([Z2-N-ARCH-250], [Z2-N-ARCH-254]), linking `TEST_THIRD_PARTY`, applying `TEST_LABELS`, recording `CONFORMANCE_LABELS` for the conformance executable, and handing the library's `Tests/Fuzz/**` and `*_CompileFail.cpp.in` lists to the test registry.
12. Record `KIND`, `MODULE_NAME`, `BACKEND_OF`, `BACKEND_NAME`, `FACTORY` and the generated-file list and `TEST_DEPENDS` for `declared_deps.json`.

- [Z2-N-ARCH-166] A library target is always `STATIC`; no Zenith2 engine, module, backend or Player library is ever built as a shared library, and Android's single shared object is the game executable produced by `zenith_add_game`.
- [Z2-N-ARCH-167] `DEPENDS` lists exactly the library's direct dependencies: a library whose files include a header of, or reference a symbol of, library X names X in `DEPENDS` even when X is reachable transitively, and libgraph fails a used-but-undeclared edge.
- [Z2-N-ARCH-168] A `KIND MODULE` library names `ZenithRuntime` in `DEPENDS` and exports exactly two entry points, `Zenith_Module_<MODULE_NAME>_RegisterTypes` and `Zenith_Module_<MODULE_NAME>_Start`, from its root header `Zenith_Module_<MODULE_NAME>.h`, with the signatures given in Architecture.md → "Engine modules and game projects".
- [Z2-N-ARCH-169] A `KIND BACKEND` library names its `BACKEND_OF` library in `DEPENDS`, declares the `FACTORY` function in a public root header named `Zenith_<Family>_<BACKEND_NAME>.h`, and exports no other public function; the API library itself exports no factory.
- [Z2-N-ARCH-170] `ZenithTestSupport`, built by the helper from `Engine/ZenithCore/Tests/Support/**`, is linked by test, conformance, fuzz, benchmark and integration-suite targets and by nothing else; `TestSeparationCheck` fails when any other target links it.

### Tools, benchmarks and integration suites

<!-- elaboration -->
Three sibling helpers apply the same glob, partition, flag and record rules as `zenith_add_library`, differing just in the target they create:

```cmake
zenith_add_tool(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [TEST_THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...])
zenith_add_benchmark(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...]
	[SLANG_SHADERS <Directory>] [SLANG_ALTERNATIVES <N>...])
zenith_add_integration_tests(<Name> [DEPENDS <Library>...] [THIRD_PARTY <ManifestName>...] [SUPPORTED <MatrixEntry>...]
	[TEST_LABELS <Label>...])
```

`TEST_THIRD_PARTY` of `zenith_add_tool` links usage-tests entries into `<Name>_Tests` alone; `THIRD_PARTY` of `zenith_add_integration_tests` links entries `PRIVATE` into `Zenith_Integration_<Name>`, and its `TEST_LABELS` (each `^[a-z][a-z0-9-]*$`) label every test discovered from that executable ([Z2-N-ARCH-251]); `SLANG_ALTERNATIVES` is a list of D-SLANG alternative IDs (1, 2, 3; default all three), and the probe receives `ZENITH_PROBE_SLANG_VERSION` and `ZENITH_BENCH_SHADER_ALTERNATIVES`; a `THIRD_PARTY` name on a benchmark, tool or integration suite whose manifest entry lacks the preset's platform is skipped, not refused; `SLANG_SHADERS` and `SLANG_ALTERNATIVES` are accepted for the benchmark `BindlessProbe` alone (the build-time shader compile of Zenith2/Docs/Rendering.md [Z2-N-REND-101]'s one exemption), any other benchmark passing them being the configure error `zenith_add_benchmark(<Name>): SLANG_SHADERS is reserved for BindlessProbe ([Z2-N-REND-101])` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests").

| Helper | Directory | Target | Notes |
|---|---|---|---|
| `zenith_add_tool` | `Tools/<Dir>/` (the target name `<Name>` need not equal the directory name, e.g. `Tools/NewGame/` builds `ZenithNewGame`) | executable `<Name>`, or a static library when the directory has no `Internal/Zenith_<Name>Main.cpp`; for an executable, every `Internal/**` source except `Internal/Zenith_<Name>Main.cpp` is compiled once into the object library `<Name>_Objects`, linked by both `<Name>` and `<Name>_Tests`, so tool tests exercise the internals without a second compile | host tools; default `SUPPORTED ZENITH_TOOLS_HOST` |
| `zenith_add_benchmark` | `Benchmarks/<Name>/` or `Benchmarks/Budgets/<AREA>/` | executable `Zenith_Bench_<Name>` linking nanobench, `ZenithTestSupport`, its `DEPENDS` and the `THIRD_PARTY` entries (a selection harness measuring a candidate no engine library owns yet); `DEPENDS` may name a tool executable's object library `<Tool>_Objects` (row `zenith_add_tool` above), which the benchmark then links instead of the tool executable, inheriting that tool's `SUPPORTED` matrix, so a benchmark measures tool internals with no second helper call ([Z2-N-ARCH-157]) | also registers a CTest smoke run with `--smoke` (Z2-D-084 harness tickets) |
| `zenith_add_integration_tests` | `Tests/<Name>/` | executable `Zenith_Integration_<Name>` linking doctest and `ZenithTestSupport` | default `SUPPORTED ZENITH_HOST`, narrowed by bullet 172 to the presets its `DEPENDS` allow; may link `ZenithRuntime` and every module; runs in the `Test` step |

### The ZENITH_SUPPORTED matrix

A matrix entry is either a configure-preset name or a macro. The macros, defined in `CMake/ZenithSupported.cmake`:

| Macro | Configure presets |
|---|---|
| `ZENITH_ALL` | `win`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64` |
| `ZENITH_GPU` | `win`, `win-asan`, `android-arm64`, `android-x86_64` (every preset with a GPU stack) |
| `ZENITH_WINDOWS` | `win`, `win-asan` |
| `ZENITH_ANDROID` | `android-arm64`, `android-x86_64` |
| `ZENITH_HOST` | `win`, `win-asan`, `linux-tsan` (presets that run tests on the build machine) |
| `ZENITH_TOOLS_HOST` | `win` (the preset that builds the host tools) |

Matrix values by library:

| Library | `SUPPORTED` |
|---|---|
| CPU-only engine libraries (Core, Graph, Jobs, Profiling, Reflect, VertexFormat, Compression and its backends, Document, ECS and its backends, Scene, Assets, Save, Script, Package) | `ZENITH_ALL` |
| `ZenithScriptCompiler` (the Luau compiler and the D-LUAU-STATELESS (open, Stage 1) check; tool side) | `ZENITH_HOST` |
| `ZenithPlatform` | `ZENITH_ALL` (its `Internal/Linux/` serves the headless test host) |
| `ZenithRHI`, `ZenithRHI_Vulkan`, `ZenithShadersGenerated`, `ZenithFlux`, `ZenithRuntime`, `ZenithPlayer`, and every module | `ZENITH_GPU` |
| `ZenithRHI_D3D12` | `ZENITH_WINDOWS` |
| `ZenithPhysics_PhysX` | `ZENITH_GPU` until D-SEL-PHYSX-ANDROID (open, Stage 1) rules; `ZENITH_WINDOWS` if it rules against Android |
| Host tools (`ZenithShaderCompiler`, `ZenithCook`, `ZenithEditor`, `ZenithNewGame`, `ZenithDevTool`, `ZenithContent`) | `ZENITH_TOOLS_HOST` |
| `ZenithContentCore` (static tool library: the source parsers and decoders, CPU-only, whose fuzz targets need `win-asan`) | `ZENITH_WINDOWS` |
| `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` (static tool libraries whose hosted parsers' fuzz targets need `win-asan`, as `ZenithContentCore`) | `ZENITH_WINDOWS` |
| `ZenithPathTracer` (test-only tool library) | `ZENITH_WINDOWS` |
| `ZenithDocumentHost` (tool library: the host-filesystem document storage) | `ZENITH_HOST`, so its tests run under ASan and TSan |

<!-- elaboration -->
Modules depend on `ZenithRuntime`, which is a GPU library, so every module is `ZENITH_GPU`. The CPU-side conformance suites of module backends (physics) therefore run in `win-asan` rather than `linux-tsan`; the `linux-tsan` host covers every library below `ZenithRuntime`, which is where the job graph, ECS, containers and the Luau VM glue live (the `Zenith2Memory` category's paths).

- [Z2-N-ARCH-171] Every `zenith_add_library` and `zenith_add_tool` call states its matrix through `SUPPORTED` or accepts its kind's default, and the `CompileCoverage` gate step checks every source against the matrix its target declares, narrowed by its `Internal/<Platform>` directory; a library whose record carries `unavailable` is reported as declared-but-unavailable, naming the absent entry, instead of failing.
- [Z2-N-ARCH-172] A target that depends on a library absent from the current preset is itself absent from that preset; a mismatch is a configure error, `zenith: <Target> is supported in <preset> but depends on <Library>, which is not`. A `TEST_DEPENDS` entry is not such a dependency: `<Name>_Tests` and `<Name>_Conformance` link the entries the preset supports, skip the others and define `ZENITH_TEST_LINKS_<TARGET>` to `1` or `0` for each (argument row `TEST_DEPENDS`), so a library's tests exist in every preset its library does.
- [Z2-N-ARCH-173] GPU libraries (RHI, its backends, Flux, Runtime, Player and every module) exclude `linux-tsan`, so the `linux-tsan` tree contains no GPU library, window system or audio device code.

### Exceptions and the firewall toggle

- [Z2-N-ARCH-174] Every Zenith2 target compiles with exceptions disabled: MSVC `/EHs-c- /D_HAS_EXCEPTIONS=0`, clang `-fno-exceptions`.
- [Z2-N-ARCH-175] `EXCEPTIONS INTERNAL` is accepted for `ZenithScript` and `ZenithScriptCompiler` alone (the two Luau firewall libraries, Zenith2/Docs/Scripting.md → "Luau integration and the exception firewall"); any other library passing it fails the configure with `zenith_add_library(<Name>): EXCEPTIONS INTERNAL is reserved for ZenithScript and ZenithScriptCompiler (Z2-D-017)`.
- [Z2-N-ARCH-176] The `ZenithScript_EH` and `ZenithScriptCompiler_EH` object libraries compile with `/EHs` on MSVC — never `/EHsc` — and with `-fexceptions` on clang, and the Luau third-party targets compile with the same flag: exactly the `targets` elements of the manifest entry `luau` that carry `"exceptions": true` (`Luau.VM`, `Luau.Ast`, `Luau.Compiler`, `Luau.CodeGen` and every further Luau target the entry lists, § "The manifest dependencies section"), which is the set Zenith2/Docs/Scripting.md → "Exception-enabled target list" enumerates.
- [Z2-N-ARCH-177] `ForbiddenCheck` reads the CMake File API compile groups and fails when `/EHs`, `/EHsc`, `/EHa` or `-fexceptions` appears on any compile group other than `ZenithScript_EH`, `ZenithScriptCompiler_EH` and the Luau targets whose manifest `targets` element carries `"exceptions": true` ([Z2-N-ARCH-176]).

<!-- informative -->
`/EHsc` lets the compiler assume `extern "C"` functions never throw, so it may elide the cleanups around a call into a C-linkage Luau API that raises through `lua_error`. `/EHs` keeps them. The Scripting document specifies the protected-entry side of the firewall: `Zenith2/Docs/Scripting.md → "Luau integration and the exception firewall"`.
<!-- /informative -->

### Test, conformance and fuzz targets

| Target | Created when | Sources | Links | Registered with CTest |
|---|---|---|---|---|
| `<Name>_Tests` | `Tests/` has at least one `.cpp` outside the conformance, fuzz and support directories | `Tests/**` minus `Tests/Conformance/**`, `Tests/ConformanceAdapter/**`, `Tests/GraphConformance/**`, `Tests/Fuzz/**`, `Tests/Support/**` (in every library: ZenithCore's is `ZenithTestSupport`, any other library's is header-only and compiled by no target, [Z2-N-ARCH-255]) | `<Name>`, its `TEST_DEPENDS` targets supported in this preset (each with its `ZENITH_TEST_LINKS_<TARGET>` definition, argument row `TEST_DEPENDS`), `ZenithTestSupport`, doctest | in `ZENITH_HOST` presets |
| `<Api>_Conformance` | the API library has `Tests/Conformance/` | the API's `Tests/Conformance/**` plus every backend's `Tests/ConformanceAdapter/**` | the API, every `KIND BACKEND` library with `BACKEND_OF <Api>` supported in this preset, `ZenithTestSupport`, doctest | in `ZENITH_HOST` presets |
| `<Name>_Conformance` | a library that is not a backend family's API library (no `KIND BACKEND` library names it in `BACKEND_OF`) has `Tests/Conformance/` | `<Name>`'s `Tests/Conformance/**` | `<Name>`, its `TEST_DEPENDS` targets supported in this preset, `ZenithTestSupport`, doctest | in `ZENITH_HOST` presets, each case once per `KIND BACKEND` library among the `TEST_DEPENDS` targets (discovery once per backend with `--zenith-backend=<BACKEND_NAME>` and the suffix `[<BACKEND_NAME>]`), named `<doctest name>[<Backend>]` ([Z2-N-ARCH-256], [Z2-N-ARCH-179]) |
| `ZenithGraph_Conformance` | always | `ZenithGraph/Tests/Conformance/**` plus `Tests/GraphConformance/**` of `ZenithJobs` and `ZenithFlux` | `ZenithGraph`, `ZenithJobs`, `ZenithFlux` (where supported) | in `ZENITH_HOST` presets |
| `Zenith_Fuzz_<Stem>` | `win-asan`, one per `Tests/Fuzz/<Stem>.cpp`, created by the test registry `CMake/Tests/ZenithCMakeTests.cmake` | that file | `<Name>`, `ZenithTestSupport`, libFuzzer | by the registry, as a 60-second smoke run, only when `ZENITH_SANITIZER` is set |
| `<Name>_CompileFail_<Stem>` | one per `Tests/**/<Stem>_CompileFail.cpp.in`, created by the test registry as an `EXCLUDE_FROM_ALL` object target | that file, configured to `<Stem>_CompileFail.cpp` in the build tree | `<Name>`'s include directories and flags | by the registry, as one CTest build of the target with `WILL_FAIL` (or with the registration's expected diagnostic as `PASS_REGULAR_EXPRESSION`, [Z2-N-ARCH-249]), in `ZENITH_HOST` presets |

- [Z2-N-ARCH-178] A backend family's conformance executable links every backend of the family that the preset supports, and runs every conformance scenario once per backend; a backend that fails a scenario fails the test.
- [Z2-N-ARCH-256] A library that is not a backend family's API library and has `Tests/Conformance/` gets the executable `<Name>_Conformance` of the target table, which links `<Name>` and every `TEST_DEPENDS` target the preset supports and runs each conformance case once per `KIND BACKEND` library among those targets (for example `ZenithScript_Conformance`, whose `TEST_DEPENDS` names the three ECS backends, `Zenith2/Docs/Scripting.md` [Z2-N-SCRIPT-523]); `zenith_end_build` creates it, and every other conformance executable of the target table, once every declaration is known (the byte-ordered `add_subdirectory` adds an API library before its backends), from what each `zenith_add_library` call recorded, so no area helper creates or registers a conformance executable of its own; discovery of such an executable runs once per `KIND BACKEND` library `B` among its `TEST_DEPENDS` with the extra argument `--zenith-backend=<BACKEND_NAME of B>` and the test suffix `[<BACKEND_NAME>]`, the test code reading the selected backend through ZenithTestSupport (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Conformance executables"), and a library with no `KIND BACKEND` target among its `TEST_DEPENDS` runs each case once, under its plain doctest name.
- [Z2-N-ARCH-179] Each doctest `TEST_CASE` is registered as its own CTest test through doctest's CTest discovery, and conformance cases once per backend, named `<doctest name>[<Backend>]` (Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"); the test main excludes the `z2-death-*`, `z2-sanitizer-defect`, `z2-selftest`, `z2-fixture-setup`, `z2-*-cooked` and `z2-acceptance-*` suites from the default run and from discovery unless a suite or case is named (`z2-fixture-setup` and `z2-*-cooked` are registered as fixture entries, [Z2-N-ARCH-257]; each `z2-acceptance-<area>` suite is registered as one CTest entry for stage acceptance only, which `Run-Committed.ps1 -Acceptance -Stage N` (`Zenith2Acceptance`) runs and the `Test` step never does, for example CONTENT's `z2-acceptance-content`, `Zenith2/Docs/Design/CONTENT/Tests.md`), and the test registry `CMake/Tests/ZenithCMakeTests.cmake` registers each case of those suites as its own CTest entry with the pass condition of Zenith2/Docs/Contracts.md → "Tests and code hygiene gates" (table "Death and sanitizer-defect tests") as `PASS_REGULAR_EXPRESSION`: `z2-death-assert`, and any `z2-death-check` case whose check is compiled only with `ZENITH_ASSERTS_ENABLED=1`, only in the asserts-enabled configurations (`Debug`, `Sanitize`, `Hwasan`), and `z2-sanitizer-defect` only when `ZENITH_SANITIZER` is set.
- [Z2-N-ARCH-249] A compile-fail test source is a `*_CompileFail.cpp.in` file under its library's `Tests/`, never matched by the source glob and never linked; the test registry configures it into the build tree as one `EXCLUDE_FROM_ALL` target and registers one CTest entry that builds that target with `WILL_FAIL`, so the test passes exactly when the source fails to compile; or, when the registration names the expected diagnostic (`EXPECT <regex>`), with that diagnostic as `PASS_REGULAR_EXPRESSION` in place of `WILL_FAIL` (CTest cannot combine the two), so the entry passes only when the source fails to compile for the stated reason.
- [Z2-N-ARCH-250] `zenith_add_library` gives every `<Name>_Tests` target the compile definition `ZENITH_TEST_FIXTURE_ROOT="<absolute path of the library's Tests/Fixtures directory>"`, and every test, conformance, benchmark and integration-suite target the compile definition `ZENITH_REPO_ROOT="<absolute repository root>"` (the directory that holds `Zenith2/`), both with forward slashes and no trailing slash and defined whether or not the directory exists, so a test opens committed fixtures relative to them (`Zenith_ReadRepoFile`) and never through a path relative to the working directory. On Android presets both name the device directory of the target instead: `ZENITH_REPO_ROOT="/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/repo"` (the external files directory of the target's test APK, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs", which a native run under `adb shell` reads too) and `ZENITH_TEST_FIXTURE_ROOT` that directory followed by the repository-relative path of the library's `Tests/Fixtures`; a device run first pushes every `Zenith2/**/Tests/Fixtures/` tree there with `adb push`, keeping its repository-relative path, so every case that reads a committed fixture runs unchanged on the device. Every `<Name>_Tests` and conformance executable also receives `ZENITH_TEST_GENERATED_ROOT="${ZENITH_OUT}/generated/<Name>/Tests"` (same form), the one directory a test or a registry block writes generated test inputs into before a test reads them, which on Android presets names the device directory instead, `ZENITH_TEST_GENERATED_ROOT="/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/generated/Tests"`, into which a device run pushes the host directory (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files"), and every test the helpers and the registry register carries the CTest environment variable `ZENITH_EXPECTED_BUILD=<platform>:<configuration>:<sanitizer>` (`windows`/`android`/`linux`; the lower-cased configuration, `shipping` in `ShippingTest` ([Z2-N-ARCH-231]); `none`/`asan`/`tsan-ubsan`/`hwasan`), matching `Zenith_BuildPlatform`, `Zenith_BuildConfiguration` and `Zenith_SanitizerKind` one to one (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Test environment").
- [Z2-N-ARCH-254] `zenith_add_library`, `zenith_add_tool`, `zenith_add_benchmark` and `zenith_add_integration_tests` give every test, conformance, benchmark and integration-suite target the compile definition `ZENITH_SHADERS_GENERATED_DIR="<ZENITH_OUT>/generated/ZenithShadersGenerated"` (absolute, forward slashes, no trailing slash, defined whether or not the directory exists; on Android presets the device directory of the target instead, `ZENITH_SHADERS_GENERATED_DIR="/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/shaders"`, into which a device run pushes the host `Blobs/` and `DevelopmentBlobs/` directories, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files", as [Z2-N-ARCH-250] does for fixtures), below which the built shader containers sit at `Blobs/<ProgramPath>.zshader` and the development containers at `DevelopmentBlobs/<ProgramPath>.zshader` (`Zenith2/Docs/Rendering/ShaderSystem.md` → "Inputs and outputs", [Z2-N-REND-108], [Z2-N-REND-993]); a target whose test reads a built container is built after those containers because its library names `ZenithShadersGenerated` in `DEPENDS` or `TEST_DEPENDS` (or depends on a library that does), and no library, tool or Player target receives the definition.
- [Z2-N-ARCH-251] `zenith_add_library(… TEST_LABELS <label>...)` applies its labels to every CTest test discovered from `<Name>_Tests`, `zenith_add_library(… CONFORMANCE_LABELS <label>...)` to every test discovered from the conformance executable the library creates or contributes to, and `zenith_add_integration_tests(… TEST_LABELS <label>...)` to every test discovered from `Zenith_Integration_<Name>`; each is passed to the executable's `doctest_discover_tests` call as the `LABELS` property, because a configure-time `set_tests_properties` cannot name a test discovered at build time. Every such discovery call also gives each test the name of every doctest `TEST_SUITE` its case sits in as a further label (doctest discovery's `ADD_LABELS`, merged into the same `LABELS` property and never replacing the call's labels), so a suite such as `script-fault-matrix` is selectable as a label, and a gate line selecting a label (`Test.ps1 -Label memory`) selects exactly the tests carrying it.
- [Z2-N-ARCH-252] The test registry `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` is included by `zenith_end_build` in every configure preset that a test preset of `CMakePresets.json` names (`win`, `win-asan`, `linux-tsan`) and in no Android preset, and it is the one place that creates fuzz targets and compile-fail targets and registers death, sanitizer-defect, compile-fail, fuzz-smoke, fixture-setup, fixture-requiring and stage-acceptance CTest entries (the last two only in the presets where the setup case's target is built, [Z2-N-ARCH-257]); no library `CMakeLists.txt` registers a test of its own.
- [Z2-N-ARCH-257] A fixture set that an area's design declares as generated at test time is registered by the test registry as CTest fixtures: the doctest case that writes it, in suite `z2-fixture-setup`, is one entry (`<AREA>_<Set>Fixtures`, run as `<Target> --test-suite=z2-fixture-setup --test-case=<Case>`) with `FIXTURES_SETUP <Fixture>`; each consuming doctest suite, named `z2-<area>-cooked`, is one entry `<Target>[<suite>]` (run as `<Target> --test-suite=<suite>`) with `FIXTURES_REQUIRED <Fixture>`, and a fuzz smoke that reads the set gains `FIXTURES_REQUIRED <Fixture>` too; every one of these entries carries `ENVIRONMENT <VARIABLE>=${ZENITH_OUT}/test-fixtures/<AREA>`, the variable and fixture name the design declares (`Zenith2/Docs/Design/GEOM.md` → "Fixtures": `GEOM_CookedFixtures`, `GeomCookedFixtures`, `ZENITH2_GEOM_COOKED_FIXTURES`, suite `z2-geom-cooked`). The setup case empties and rewrites that directory and fails, naming the variable, when it is unset; a consuming case that mutates a fixture copies it into a subdirectory of its own first; nothing a fixture entry writes lands outside `${ZENITH_OUT}/test-fixtures/`. Because the test main excludes both suite kinds from discovery ([Z2-N-ARCH-179]), no case of them is also registered on its own. A fixture setup entry is registered only in the presets where its setup case's target is built, and an entry requiring that fixture only where the setup entry is registered: the registry wraps both in `if(TARGET <Target>)` of the setup target (for GEOM `if(TARGET ZenithCook_Tests)`, so `win` alone, because ZenithCook is `ZENITH_TOOLS_HOST`). A consumer that runs in a preset without the setup entry, every fuzz smoke among them (fuzz targets exist in `win-asan` alone), requires no fixture and builds its inputs in code, from header-only seed files under its library's `Tests/Support/` ([Z2-N-ARCH-255]; GEOM's `Zenith2/Engine/ZenithFlux/Tests/Support/Flux_GeomSeedFiles.h`, `Zenith2/Docs/Design/GEOM.md` → "Fixtures"), and the area's design names that route for each such consumer.
- [Z2-N-ARCH-228] Test results are judged pass or fail alone, and no gate compares or pins test counts.
- [Z2-N-ARCH-229] Each test executable writes its JUnit report to `${ZENITH_OUT}/gates/tests/<Name>.xml`.
- [Z2-N-ARCH-180] On Android presets the test and conformance executables are built (so `CompileCoverage` covers them) and are never registered with CTest; on-device test runs belong to acceptance and `needs-human` device tickets, and every such run pushes the committed fixture trees, the built shader containers and the target's generated test inputs to the target's device directories before it starts ([Z2-N-ARCH-250], [Z2-N-ARCH-254]; `CMake/ZenithPushTestApkFiles.cmake`, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files"), which is the one device-file rule a device ticket cites.
- [Z2-N-ARCH-181] Test, conformance, fuzz, benchmark and integration-suite targets never enter the `Shipping` configuration's build presets (`win-shipping`, `android-arm64-shipping`); the `ShippingTest` configuration, which builds test, conformance and integration-suite targets with Shipping's code ([Z2-N-ARCH-231]), is never a Player's: no build preset that names `zenith_players` uses it, and neither the Shipping link closure ([Z2-N-ARCH-240]) nor `TestSeparationCheck` reads it; and `TestSeparationCheck` fails when a shipping Player's link closure contains any of them or `ZenithTestSupport`.
- [Z2-N-ARCH-240] The Shipping link closure of a game is the transitive set of targets that the CMake File API reports as link dependencies of the game executable in the `Shipping` configuration of the `win` and `android-arm64` trees (the build presets `win-shipping` and `android-arm64-shipping`); libgraph writes it to `${ZENITH_OUT}/gates/libgraph/shipping_closure_<Game>_<preset>.json`.
- [Z2-N-ARCH-241] The `Libgraph` step fails when a Shipping link closure contains `ZenithScriptCompiler`, `Luau.Compiler`, `Luau.Ast`, a test, conformance, fuzz or benchmark target, `ZenithTestSupport`, or any tool target.
- [Z2-N-ARCH-242] No Player links `ZenithScriptCompiler` in any configuration: its consumers are `ZenithCook` and `ZenithEditor`, and its matrix is `ZENITH_HOST` (never an Android preset).

### The declared_deps.json record

The CMake File API reports transitive static-library dependencies, which hides used-but-undeclared edges, so the helpers record each target's declared direct dependencies. `zenith_end_build` writes `${ZENITH_OUT}/declared_deps.json`:

```json
{
	"schema": "zenith2.declared-deps",
	"schemaVersion": 1,
	"libraries": [
		{
			"name": "ZenithJobs",
			"kind": "ENGINE",
			"dir": "Engine/ZenithJobs",
			"depends": ["ZenithCore", "ZenithGraph", "ZenithProfiling"],
			"thirdParty": [],
			"supported": ["android-arm64", "android-x86_64", "linux-tsan", "win", "win-asan"],
			"exceptions": "OFF",
			"moduleName": null,
			"backendOf": null,
			"backendName": null,
			"factory": null,
			"testDepends": [],
			"generated": []
		}
	],
	"tools": [
		{
			"name": "ZenithCook",
			"dir": "Tools/ZenithCook",
			"depends": ["Zenith2D", "ZenithAI", "ZenithAnimation", "ZenithAssets", "ZenithAudio", "ZenithCinematics", "ZenithCompression", "ZenithCompression_Zlib", "ZenithCompression_Zstd", "ZenithCore", "ZenithDocument", "ZenithDocumentHost", "ZenithEnvironment", "ZenithGameplay", "ZenithJobs", "ZenithNet", "ZenithOnline", "ZenithPhysics", "ZenithPlatform", "ZenithReflect", "ZenithRuntime", "ZenithScene", "ZenithScriptCompiler", "ZenithShadersGenerated", "ZenithStreaming", "ZenithUI", "ZenithVFX", "ZenithVertexFormat"],
			"thirdParty": ["KTX-Software", "fastgltf", "meshoptimizer", "mikktspace"],
			"supported": ["win"]
		}
	],
	"games": [
		{
			"name": "OpenWorldSlice",
			"dir": "Games/OpenWorldSlice",
			"platforms": ["android", "windows"],
			"modules": ["AI", "Animation", "Audio", "Gameplay", "Physics", "Streaming", "UI"],
			"backends": { "ecs": "Flecs", "physics": { "android": "Jolt", "windows": "PhysX" } }
		}
	],
	"generated": [
		{
			"file": "generated/ZenithShadersGenerated/Flux/Shaders/Flux_SSAO.generated.h",
			"attributedTo": "ZenithShadersGenerated",
			"producer": "ZenithShaderCompiler"
		}
	]
}
```

The record also carries, without a `schemaVersion` change (no existing field changes meaning): `testThirdParty` (the `TEST_THIRD_PARTY` names) and `conformanceLabels` (the `CONFORMANCE_LABELS`) on each `libraries[]` record, `testThirdParty` on each `tools[]` record, and the arrays `benchmarks[]` and `integrationSuites[]`, one record `{ name, dir, depends, thirdParty, supported }` per `zenith_add_benchmark` and `zenith_add_integration_tests` call, every list byte-ordered; a declared-but-unavailable library's record carries `unavailable` (the absent entry's name; absent from every other record); libgraph's third-party ownership rule reads `thirdParty` and `testThirdParty` of every record (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "declared_deps additions"). `zenith_end_build` also writes `${ZENITH_OUT}/third_party_targets.json`, one record `{ entry, target, owner, usage, shipping, exceptions, runtimeFiles }` per created third-party target, sorted by `target` and left untouched when its bytes are unchanged, which the licence inventory reads (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "End-of-build outputs").

- [Z2-N-ARCH-182] `declared_deps.json` is deterministic: arrays sorted by byte order of `name` (and of `file` for `generated`), keys in the order shown, four-space indentation, LF line endings, and the file is rewritten only when its bytes change, so an unchanged configure never touches its timestamp.
- [Z2-N-ARCH-183] The record contains every declaration of the tree regardless of which preset is being configured, so every preset writes identical bytes; `supported` records where each target exists.
- [Z2-N-ARCH-184] Every generated file consumed by a compile is listed under `generated` with the library it is attributed to (the library whose helper call produced it: `ZenithShadersGenerated` for shader headers, the declaring library for `GENERATES` files), and libgraph attributes the file's includes to that library.
- [Z2-N-ARCH-243] For a `KIND GENERATED` (INTERFACE) target, libgraph attributes the generated headers' includes to that target and their compiled symbols to the library whose object includes them; an INTERFACE target has no symbol stage, and libgraph's CMake File API front end reads `INTERFACE_LIBRARY` targets for their declared edges and include directories.

## Compile and link flags

`zenith_apply_flags(<Target>)` applies the flags below to every Zenith2 target; third-party targets receive the rows of the table "Third-party flag rows" alone (§ "Third-party targets and configuration variables"), and every compile group of either kind also carries the per-configuration variables of that section.

### MSVC flags for the win and win-asan presets

| Scope | Flags |
|---|---|
| All configurations | the MSVC warning set of `CMake/ZenithWarnings.cmake`, plus `/std:c++20 /Zc:throwingNew /external:anglebrackets /external:W0 /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /fp:precise /DNOMINMAX /DWIN32_LEAN_AND_MEAN /DZENITH_PLATFORM_WINDOWS=1` |
| Debug information | `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded` (`/Z7`) in every configuration; link `/DEBUG:FULL` |
| Reproducibility | every configuration: compile `/Brepro`; link `/Brepro`, executables also `/PDBALTPATH:%_PDB%` (the PE records the PDB file name only), so the PE timestamp and the CodeView RSDS GUID are a hash of the output ([Z2-N-ARCH-258]) |
| Hardening (`Release`, `Shipping`, `ShippingTest`) | compile `/guard:cf` on every Zenith2 target; executables link `/guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT` ([Z2-N-ARCH-259]) |
| Runtime library | `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL` |
| `Debug` | `/Od /Gy- /RTC1 /DZENITH_CONFIG_DEBUG=1`; link `/INCREMENTAL:NO /OPT:NOREF /OPT:NOICF` |
| `Release` | `/O2 /Ob2 /Gy /Gw /DZENITH_CONFIG_RELEASE=1`; link `/INCREMENTAL:NO /OPT:REF /OPT:ICF` |
| `Shipping` | `/O2 /Ob2 /Gy /Gw /DZENITH_CONFIG_SHIPPING=1`; link `/INCREMENTAL:NO /OPT:REF /OPT:ICF`; the game executable links `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup`, so the Player's `main` stays the entry point |
| `ShippingTest` (`win` alone) | the `Shipping` row's compile flags, `/DZENITH_CONFIG_SHIPPING=1` included, and its link flags (it builds no game executable, [Z2-N-ARCH-231]) |
| `Sanitize` (`win-asan`) | `/O1 /Oy- /Gy- /fsanitize=address /DZENITH_CONFIG_SANITIZE=1 /DZENITH_SANITIZER_ADDRESS=1`; no `/RTC`; link `/INCREMENTAL:NO`; fuzz targets add `/fsanitize=fuzzer` |

- [Z2-N-ARCH-185] `zenith_apply_flags` applies the warning sets of `CMake/ZenithWarnings.cmake` (warnings as errors, specified in Zenith2/Docs/Contracts.md → "Tests and code hygiene gates") to every Zenith2 target, and third-party headers are included as system headers so their warnings never apply.
- [Z2-N-ARCH-186] The `Debug` configuration of the `win` preset (the `win-debug` build preset) always compiles with `/Gy-` and never with `/ZI`, so libgraph's symbol stage reuses the `win-debug` build and no fourth build flavour exists.
- [Z2-N-ARCH-187] MSVC debug information is `/Z7` in every configuration, because sccache does not cache compiles that write a shared PDB (`/Zi`); libgraph detects function-level linking from the File API's compile fragments, never from the debug-information flag.
- [Z2-N-ARCH-188] No Zenith2 target enables RTTI (`/GR-`, `-fno-rtti`) or floating-point contraction (`/fp:precise` without `/fp:contract`; clang `-ffp-contract=off`), because the determinism guarantee depends on identical per-entity instruction sequences (Architecture.md → "Determinism guarantee and mechanisms").
- [Z2-N-ARCH-230] `zenith_apply_flags` applies exactly the flags of the tables "MSVC flags for the win and win-asan presets" and "Clang flags for the android and linux-tsan presets" to Zenith2 targets and exactly the third-party flag rows of § "Third-party targets and configuration variables" to third-party targets, `zenith_begin_build` sets exactly that section's per-configuration variables, and a flag change edits those tables in the same diff.
- [Z2-N-ARCH-258] Every executable carries a build identity that is a function of its bytes, and two clean builds of one tree are byte-identical (the two "Reproducibility" rows, applied to every Zenith2 and third-party target in every configuration): every MSVC executable links `/Brepro` with `/DEBUG:FULL` and `/INCREMENTAL:NO` (both already in every configuration), so its CodeView RSDS GUID is a hash of the image, and every Linux executable and Android shared object links `-Wl,--build-id=sha1`; so `Zenith_GetRunningBuildID` (ZenithSave, `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header") is non-zero in every preset.

<!-- informative -->
The plan's note that CMake's MSVC default is `/Zi` (not `/ZI`, which implies `/Gy`) concerns function-level linking only. `/Z7` keeps that property and additionally lets sccache cache MSVC compiles, which the pinned-sccache rule of Z2-D-028 needs.
<!-- /informative -->

### Clang flags for the android and linux-tsan presets

| Scope | Flags |
|---|---|
| All configurations | the clang warning set of `CMake/ZenithWarnings.cmake` (which includes the curated conversion subset), plus `-std=c++20 -fno-rtti -fno-exceptions -ffp-contract=off -fvisibility=hidden -g` |
| Android | `-DZENITH_PLATFORM_ANDROID=1`; shared-object link `-Wl,-z,max-page-size=16384 -Wl,--build-id=sha1 -Wl,--no-undefined` |
| Linux | `-DZENITH_PLATFORM_LINUX=1`; executable link `-Wl,--build-id=sha1` ([Z2-N-ARCH-258]) |
| Reproducibility | every configuration: compile `-ffile-prefix-map=${ZENITH_ROOT}=Zenith2 -ffile-prefix-map=${ZENITH_OUT}=out -fno-record-gcc-switches`; every executable and shared object links `-Wl,--build-id=sha1` (Android and Linux, [Z2-N-ARCH-258]) |
| Hardening (Android, `Release` and `Shipping`) | compile `-fstack-protector-strong -D_FORTIFY_SOURCE=2` on every Zenith2 target; the shared object links `-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack` ([Z2-N-ARCH-259]) |
| `Debug` | `-O0 -DZENITH_CONFIG_DEBUG=1` |
| `Release` | `-O2 -DZENITH_CONFIG_RELEASE=1` |
| `Shipping` | `-O2 -DZENITH_CONFIG_SHIPPING=1` |
| `Hwasan` (`android-arm64`) | `-O1 -fno-omit-frame-pointer -fsanitize=hwaddress -DZENITH_CONFIG_HWASAN=1 -DZENITH_SANITIZER_HWADDRESS=1`; link `-fsanitize=hwaddress` |
| `Sanitize` (`linux-tsan`) | `-O1 -fno-omit-frame-pointer -fsanitize=thread -fsanitize=undefined -fno-sanitize-recover=undefined -DZENITH_CONFIG_SANITIZE=1 -DZENITH_SANITIZER_THREAD=1`; link the same sanitizer flags |

- [Z2-N-ARCH-247] `zenith_apply_flags` defines every configuration define — `ZENITH_CONFIG_DEBUG`, `ZENITH_CONFIG_RELEASE`, `ZENITH_CONFIG_SHIPPING`, `ZENITH_CONFIG_SANITIZE`, `ZENITH_CONFIG_HWASAN`, `ZENITH_PLATFORM_WINDOWS`, `ZENITH_PLATFORM_ANDROID`, `ZENITH_PLATFORM_LINUX`, `ZENITH_SANITIZER_ADDRESS`, `ZENITH_SANITIZER_HWADDRESS`, `ZENITH_SANITIZER_THREAD`, `ZENITH_DEBUG` and `ZENITH_ASSERTS_ENABLED` — in every Zenith2 target and configuration, to `1` where the tables above list it (and, for `ZENITH_DEBUG`, in `Debug`; for `ZENITH_ASSERTS_ENABLED`, where Zenith2/Docs/Contracts.md → "Errors and results" enables `ZENITH_ASSERT`) and to `0` everywhere else, so exactly one platform define and one configuration define are 1 and at most one sanitizer define is 1.
- [Z2-N-ARCH-248] Code tests a configuration define by its value (`#if ZENITH_ASSERTS_ENABLED`, or the functions of `ZenithCore/Zenith_BuildInfo.h`), never by whether it is defined (`#ifdef`, `defined(…)`), and `ZenithCore/Internal/Zenith_BuildInfoChecks.cpp` `static_assert`s the counts of bullet 247.
- [Z2-N-ARCH-189] The configuration defines (`ZENITH_CONFIG_*`, `ZENITH_PLATFORM_*`, `ZENITH_SANITIZER_*`) are read only through the `constexpr` functions of `ZenithCore/Zenith_BuildInfo.h` (`Zenith_IsDebugBuild()`, `Zenith_IsShippingBuild()`, `Zenith_IsSanitizerBuild()`, `Zenith_GetBuildPlatform()`), and never change a type's layout (Zenith2/Docs/Contracts.md → "No layout-changing conditionals").

### Third-party targets and configuration variables

Third-party flag rows (applied by `zenith_apply_flags(<Target> THIRD_PARTY)`; an `INTERFACE` or `IMPORTED` target receives no compile row; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Third-party flag rows"):

| Row | MSVC | clang |
|---|---|---|
| sanitizer | the `Sanitize` row's `/fsanitize=address` (`win-asan`) | `-fsanitize=thread -fsanitize=undefined -fno-sanitize-recover=undefined` (`linux-tsan`), `-fsanitize=hwaddress` (`Hwasan`) |
| runtime library | `CMAKE_MSVC_RUNTIME_LIBRARY` as the MSVC table | — |
| debug information | `/Z7` (`Embedded`) | `-g` |
| exceptions off (every third-party target except the Luau targets) | `/EHs-c- /D_HAS_EXCEPTIONS=0` | `-fno-exceptions` |
| exceptions on (the Luau targets of Zenith2/Docs/Scripting.md → "Exception-enabled target list") | `/EHs /D_HAS_EXCEPTIONS=1` | `-fexceptions` |
| RTTI off (every third-party target, the Luau targets included) | `/GR-` | `-fno-rtti` |
| reproducibility | the compile and link halves of the MSVC "Reproducibility" row | the compile and link halves of the clang "Reproducibility" row |

Per-configuration variables, set by `zenith_begin_build` for every configuration of `CMAKE_CONFIGURATION_TYPES` after it removes the inherited tokens `/EHsc`, `/EHs`, `/GR`, `/W3`, `-fexceptions` and `-frtti` from `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS` ([Z2-N-SCRIPT-027]); it also sets `CMAKE_MAP_IMPORTED_CONFIG_SHIPPING`, `CMAKE_MAP_IMPORTED_CONFIG_SHIPPINGTEST`, `CMAKE_MAP_IMPORTED_CONFIG_SANITIZE` and `CMAKE_MAP_IMPORTED_CONFIG_HWASAN` to `Release`, so an imported target with Debug and Release locations alone resolves (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Global flag hygiene"):

| Variable | MSVC (`win`, `win-asan`) | clang (Android, Linux) |
|---|---|---|
| `CMAKE_<LANG>_FLAGS_DEBUG` | `/Ob0 /Od` | `-O0` |
| `CMAKE_<LANG>_FLAGS_RELEASE` | `/O2 /Ob2 /DNDEBUG` | `-O2 -DNDEBUG` |
| `CMAKE_<LANG>_FLAGS_SHIPPING` | `/O2 /Ob2 /DNDEBUG` | `-O2 -DNDEBUG` |
| `CMAKE_<LANG>_FLAGS_SHIPPINGTEST` | `/O2 /Ob2 /DNDEBUG` | — |
| `CMAKE_<LANG>_FLAGS_SANITIZE` | `/O1 /Oy-` | `-O1 -fno-omit-frame-pointer` |
| `CMAKE_<LANG>_FLAGS_HWASAN` | — | `-O1 -fno-omit-frame-pointer` |
| `CMAKE_{EXE,SHARED,MODULE}_LINKER_FLAGS_<CONFIG>` | empty (the link rows come from `zenith_apply_flags`) | empty |

- [Z2-N-ARCH-259] The hardening rows of the two flag tables apply in `Release`, `Shipping` and `ShippingTest` (which takes every `Shipping` row, [Z2-N-ARCH-231]) alone, before the first budget run, so a budget run measures the code that ships: the compile half to every Zenith2 target (libraries, executables and the Android shared object, each object an image links), never to a third-party target, and the link half to every Zenith2 executable and shared object; the Shipping qualification's binary checks read them back (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Windows package checklist", "Android package checklist").

### Configurations

| Configuration | Purpose | Programmer-error asserts | Development services (profiler overlay, console, DAP, remote tools) |
|---|---|---|---|
| `Debug` | development, libgraph symbol stage | on | available |
| `Release` | development at speed, performance runs, CI smoke | off in every target (`ZENITH_ASSERTS_ENABLED=0`; `ZENITH_CHECK`, `ZENITH_FATAL` and `ZENITH_UNREACHABLE` stay on), the host tools the `win-tools` build preset builds included: their Release invariants use `ZENITH_CHECK` (Zenith2/Docs/Contracts.md [Z2-N-CONTRACT-479]) | available; profiler overlay off by default |
| `Shipping` | distributed builds | off (`ZENITH_ASSERTS_ENABLED=0`; the always-on checks of Zenith2/Docs/Contracts.md → "Errors and results" stay on) | absent (Architecture.md → "Security architecture") |
| `ShippingTest` | host tests of Shipping behaviour: Shipping's code built into test, conformance and integration-suite targets alone (`win` tree, build and test preset `win-shipping-test`) | off, as `Shipping` | absent, as `Shipping` |
| `Sanitize` | `win-asan`, `linux-tsan` | on | available |
| `Hwasan` | the S24 HWASan APK | on | available |

- [Z2-N-ARCH-231] The build configurations are exactly the rows of the table "Configurations", with programmer-error asserts and development services present or absent as each row states. `ShippingTest` receives every flag row, define, hardening row, per-configuration variable and imported-configuration mapping of `Shipping`, so `Zenith_IsShippingBuild()` is true and `Zenith_BuildConfiguration` reports `BUILD_CONFIGURATION_SHIPPING` in it (its tests carry `ZENITH_EXPECTED_BUILD=windows:shipping:none`, [Z2-N-ARCH-250]); it exists in the `win` configure tree alone and is built and run only through the `win-shipping-test` build and test presets, whose one target `zenith_tests` ([Z2-N-ARCH-191]) builds every test, conformance and integration-suite target the preset supports, so a design's "Shipping test build" case is a case of that preset and no Player is ever built in it.

## Presets

`Zenith2/CMakePresets.json` uses preset schema version 6. Configure presets name the toolchain and tree; build presets name the configuration and target set. The gate steps, CI and developers use the build-preset names.

```json
{
	"version": 6,
	"cmakeMinimumRequired": { "major": 3, "minor": 28, "patch": 0 },
	"configurePresets": [
		{
			"name": "zenith-base",
			"hidden": true,
			"generator": "Ninja Multi-Config",
			"binaryDir": "${sourceDir}/out/$env{ZENITH_CHECKOUT_HASH}/${presetName}",
			"cacheVariables": {
				"ZENITH_PRESET": "${presetName}",
				"ZENITH_ROOT": "${sourceDir}",
				"ZENITH_OUT": "${sourceDir}/out/$env{ZENITH_CHECKOUT_HASH}"
			}
		},
		{
			"name": "win",
			"inherits": "zenith-base",
			"displayName": "Windows x64, MSVC",
			"toolchainFile": "${sourceDir}/CMake/toolchains/msvc-x64.cmake",
			"cacheVariables": {
				"CMAKE_CONFIGURATION_TYPES": "Debug;Release;Shipping;ShippingTest",
				"ZENITH_TARGET_OS": "Windows",
				"ZENITH_HOST_TOOLS_DIR": "${sourceDir}/out/$env{ZENITH_CHECKOUT_HASH}/win/Release/bin"
			},
			"condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
		},
		{
			"name": "win-asan",
			"inherits": "zenith-base",
			"displayName": "Windows x64, MSVC AddressSanitizer + libFuzzer",
			"toolchainFile": "${sourceDir}/CMake/toolchains/msvc-x64.cmake",
			"cacheVariables": {
				"CMAKE_CONFIGURATION_TYPES": "Sanitize",
				"ZENITH_TARGET_OS": "Windows",
				"ZENITH_SANITIZER": "address",
				"ZENITH_HOST_TOOLS_DIR": "${sourceDir}/out/$env{ZENITH_CHECKOUT_HASH}/win/Release/bin"
			},
			"condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
		},
		{
			"name": "linux-tsan",
			"inherits": "zenith-base",
			"displayName": "Linux x64 (WSL2), clang ThreadSanitizer + UBSan, headless",
			"binaryDir": "$env{HOME}/.zenith2/out/$env{ZENITH_CHECKOUT_HASH}/linux-tsan",
			"toolchainFile": "${sourceDir}/CMake/toolchains/clang-linux-x64.cmake",
			"cacheVariables": {
				"CMAKE_CONFIGURATION_TYPES": "Sanitize",
				"ZENITH_TARGET_OS": "Linux",
				"ZENITH_SANITIZER": "thread;undefined",
				"ZENITH_OUT": "$env{HOME}/.zenith2/out/$env{ZENITH_CHECKOUT_HASH}"
			},
			"condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" }
		},
		{
			"name": "android-arm64",
			"inherits": "zenith-base",
			"displayName": "Android arm64-v8a, NDK r27c",
			"toolchainFile": "$env{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake",
			"cacheVariables": {
				"CMAKE_CONFIGURATION_TYPES": "Debug;Release;Shipping;Hwasan",
				"ZENITH_TARGET_OS": "Android",
				"ANDROID_ABI": "arm64-v8a",
				"ANDROID_PLATFORM": "android-34",
				"ANDROID_STL": "c++_static",
				"ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES": "ON",
				"CMAKE_C_COMPILER_LAUNCHER": "sccache",
				"CMAKE_CXX_COMPILER_LAUNCHER": "sccache",
				"ZENITH_HOST_TOOLS_DIR": "${sourceDir}/out/$env{ZENITH_CHECKOUT_HASH}/win/Release/bin"
			},
			"condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
		},
		{
			"name": "android-x86_64",
			"inherits": "android-arm64",
			"displayName": "Android x86_64 (emulator smoke only), NDK r27c",
			"cacheVariables": {
				"CMAKE_CONFIGURATION_TYPES": "Debug;Release",
				"ANDROID_ABI": "x86_64"
			}
		}
	],
	"buildPresets": [
		{ "name": "win-debug", "configurePreset": "win", "configuration": "Debug" },
		{ "name": "win-release", "configurePreset": "win", "configuration": "Release" },
		{ "name": "win-shipping", "configurePreset": "win", "configuration": "Shipping", "targets": ["zenith_players"] },
		{ "name": "win-shipping-test", "configurePreset": "win", "configuration": "ShippingTest", "targets": ["zenith_tests"] },
		{ "name": "win-tools", "configurePreset": "win", "configuration": "Release", "targets": ["zenith_host_tools"] },
		{ "name": "win-asan", "configurePreset": "win-asan", "configuration": "Sanitize" },
		{ "name": "linux-tsan", "configurePreset": "linux-tsan", "configuration": "Sanitize" },
		{ "name": "android-arm64", "configurePreset": "android-arm64", "configuration": "Release" },
		{ "name": "android-arm64-debug", "configurePreset": "android-arm64", "configuration": "Debug" },
		{ "name": "android-arm64-shipping", "configurePreset": "android-arm64", "configuration": "Shipping", "targets": ["zenith_players"] },
		{ "name": "android-arm64-hwasan", "configurePreset": "android-arm64", "configuration": "Hwasan" },
		{ "name": "android-x86_64", "configurePreset": "android-x86_64", "configuration": "Debug" }
	],
	"testPresets": [
		{ "name": "win-debug", "configurePreset": "win", "configuration": "Debug", "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error", "timeout": 60 } },
		{ "name": "win-release", "configurePreset": "win", "configuration": "Release", "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error", "timeout": 60 } },
		{ "name": "win-shipping-test", "configurePreset": "win", "configuration": "ShippingTest", "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error", "timeout": 60 } },
		{ "name": "win-asan", "configurePreset": "win-asan", "configuration": "Sanitize", "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error", "timeout": 60 } },
		{ "name": "linux-tsan", "configurePreset": "linux-tsan", "configuration": "Sanitize", "output": { "outputOnFailure": true }, "execution": { "noTestsAction": "error", "timeout": 60 } }
	]
}
```

<!-- elaboration -->
The plan's build-preset list (`win-debug`, `win-release`, `win-tools`, `win-asan`, `linux-tsan`, `android-arm64`, `android-x86_64`) is kept verbatim. `win-shipping`, `win-shipping-test`, `android-arm64-debug`, `android-arm64-shipping` and `android-arm64-hwasan` are added as build presets over the same configure trees, because the Shipping configuration, the Shipping test build and the HWASan build type need names the gate steps can pass. They add no configure tree and no build flavour to the base gate list.

### Preset reference

| Build preset | Configure tree | Generator | Toolchain | Configuration | Targets | Condition | Used by |
|---|---|---|---|---|---|---|---|
| `win-debug` | `win` | Ninja Multi-Config | MSVC (pinned), sccache | `Debug` | all | host Windows | Build, Test, Libgraph (symbol stage), CompileCoverage, HeaderSelfContained |
| `win-release` | `win` | Ninja Multi-Config | MSVC, sccache | `Release` | all | host Windows | Build, Test, CI integration smoke, performance runs |
| `win-shipping` | `win` | Ninja Multi-Config | MSVC, sccache | `Shipping` | `zenith_players` | host Windows | packaging, TestSeparationCheck, stage acceptance |
| `win-shipping-test` | `win` | Ninja Multi-Config | MSVC, sccache | `ShippingTest` | `zenith_tests` | host Windows | Test (the Shipping-behaviour cases, test preset of the same name) |
| `win-tools` | `win` | Ninja Multi-Config | MSVC, sccache | `Release` | `zenith_host_tools` | host Windows | first step of every Build, `ZENITH_HOST_TOOLS_DIR` |
| `win-asan` | `win-asan` | Ninja Multi-Config | MSVC, sccache | `Sanitize` | all | host Windows | Sanitizers (change-triggered), fuzz smoke, acceptance |
| `linux-tsan` | `linux-tsan` | Ninja Multi-Config | clang (pinned; WSL2 locally, native in CI), sccache | `Sanitize` | CPU libraries, their tests and conformance suites | host Linux | Sanitizers (change-triggered), CI Linux job |
| `android-arm64` | `android-arm64` | Ninja Multi-Config | NDK r27c clang, sccache | `Release` | Players, libraries, tests (compile only) | host Windows | Build (base category from Stage 2), ApkCheck |
| `android-arm64-debug` | `android-arm64` | Ninja Multi-Config | NDK r27c clang, sccache | `Debug` | all | host Windows | device debugging |
| `android-arm64-shipping` | `android-arm64` | Ninja Multi-Config | NDK r27c clang, sccache | `Shipping` | `zenith_players` | host Windows | Stage 6 packaging |
| `android-arm64-hwasan` | `android-arm64` | Ninja Multi-Config | NDK r27c clang, sccache | `Hwasan` | Players, tests | host Windows | HWASan APK on the S24 (needs-human device step) |
| `android-x86_64` | `android-x86_64` | Ninja Multi-Config | NDK r27c clang, sccache | `Debug` | Players | host Windows | emulator smoke only |

- [Z2-N-ARCH-253] The `--stream-sets` aggregate command of `ZenithShadersGenerated`, which writes `Flux/Shaders/Flux_StreamSets.generated.h`, is ordered before every compile of `ZenithCook` (its cook packer reads `axFLUX_STREAMSET_LAYOUTS`; ZenithCook declares `ZenithShadersGenerated` in `DEPENDS`), so the cook never packs vertex streams against stale layout tables.
- [Z2-N-ARCH-190] Every Build gate step builds `win-tools` before any other build preset of the phase, and every custom command that runs a host tool names the tool's file under `ZENITH_HOST_TOOLS_DIR` in its `DEPENDS`, so a tool change re-runs the commands that used it.
- [Z2-N-ARCH-191] The `zenith_host_tools`, `zenith_players` and `zenith_tests` utility targets are created by `zenith_end_build`: `zenith_host_tools` depends on every `zenith_add_tool` target supported in the preset, `zenith_players` on every game executable, and `zenith_tests` on every test, conformance and integration-suite target supported in the preset (the target set of `win-shipping-test`, [Z2-N-ARCH-231]).
- [Z2-N-ARCH-192] `ZENITH_HOST_TOOLS_DIR` is the one path by which a build reaches the host tools (ZenithShaderCompiler, ZenithCook); Android presets never build a host tool, and every Android custom command runs the Windows tool from that directory.
- [Z2-N-ARCH-193] The Configure gate step configures every configure preset whose `condition` holds on the host; a preset whose condition is false is reported as `skipped (condition: hostSystemName)` and is never an error.

## Toolchain pins and the setup script

<!-- elaboration -->
The reproducibility manifest (Z2-D-008) and the dependency manifest are one file, `Zenith2/ThirdParty/manifest.json`, with a `toolchain` section and a `dependencies` section; the evidence fingerprint hashes each section separately as `toolchainHash` and `dependenciesHash`.

### The toolchain section

| Key | Pins |
|---|---|
| `msvc` | Visual Studio instance version, `cl.exe` version (`19.xx.yyyyy`), installer channel URI, the component list, and the SHA-256 of the installer bootstrapper |
| `llvm` | one LLVM release (clang, clang-tidy, clang-format, llvm-readelf) as one `version` with a `windows` and a `linux` archive object of the LLVM project's release archives (URL and SHA-256 each); the `linux` archive is extracted to `$HOME/.zenith2/toolchain/llvm/<version>/` inside the WSL distro and on the native Linux CI host by one setup function, never installed from a distro package |
| `cmake`, `ninja`, `sccache` | version, archive URL, SHA-256 per host OS |
| `slang` | release version, archive URL, SHA-256 (offline tool only); consumed by the `slang` dependency entry through `fromToolchain` |
| `dxc` | release version, archive URL, SHA-256 (offline tool only); consumed by the `dxc` dependency entry through `fromToolchain` |
| `warp` | NuGet package `Microsoft.Direct3D.WARP`, version, SHA-256 of the `.nupkg`; consumed by the `warp` dependency entry through `fromToolchain` |
| `lavapipe` | Mesa release, archive URL, SHA-256 of the prebuilt Windows and Linux lavapipe ICD archives; consumed by the `lavapipe` dependency entry through `fromToolchain` |
| `ndk` | `r27c`, archive URL, SHA-256, and `pkgRevision` (the NDK's `source.properties` `Pkg.Revision`, the value the Android configure compares with `ANDROID_NDK_REVISION`) |
| `jdk` | the JDK 17 build, archive URL, SHA-256 |
| `gradle` | wrapper distribution version, URL, SHA-256 (the wrapper's `distributionSha256Sum`) |
| `agp` | Android Gradle Plugin version, at least 8.5.1 |
| `androidSdk` | `platforms;android-<compileSdk>`, `build-tools;<version>`, `platform-tools;<version>`, each with its SHA-256 |
| `wsl` | the pinned distro name (for example `Ubuntu-24.04`), the root-filesystem image URL and SHA-256, `snapshot` (the `snapshot.ubuntu.com` date every package is installed from), `packages` (the system packages both Linux hosts install, each `{ "package", "version" }`, sorted by `package`) and `licence` |
| `python`, `pwsh` | the interpreter versions the gate steps and analyzers run under; `python` also runs the developer-only Vulkan-Profiles generator, which no build or gate step runs |
| `vulkanProfilesGenerator` | the Vulkan-Profiles release whose `scripts/gen_profiles_solution.py` generates `ZenithRHI_Vulkan`'s committed profile files, which record the JSON's SHA-256 and this version in their first line (developer-only) |

Every key except `vulkanProfilesGenerator` carries `licence`, an SPDX expression of the tool's own terms (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain licence terms"); a toolchain pin ships in no package and has no licence file. Each key holds a pin object whose JSON shape is `Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain pin object" and "Toolchain keys" (a `version` and one archive object `{ "url", "sha256", "stripPrefix", "format" }` per host, or the key-specific fields of its row). A key may also carry `runtimeComponents`: the redistributable runtime files the toolchain provides and a packaged build ships (the MSVC C++ runtime DLLs, kind `app-local-dll`) or the statically linked toolchain code whose licence a notice states (the NDK's `libc++`, kind `static-runtime`), each with `id`, `platform`, `kind`, `files` and the licence fields (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Toolchain runtime components"). Slang, DXC, WARP and lavapipe are pinned here once and reach the build through the `fromToolchain` field of their dependency entries.

<!-- informative -->
Two host tools are **checked prerequisites** of the setup, not pins of the `toolchain` section, because neither contributes a byte to a build output: `git` at least 2.40 and, on Windows hosts, `git-lfs` at least 3.4.0. `Setup-Zenith2.ps1` verifies each one's presence and version (for git-lfs it also runs `git lfs install --local` and `git lfs pull`), exits 3 when one is missing or older, and `-Check` reports both; git's own LFS state of the clone is the one named exception to the setup writing nothing inside the checkout outside `out/`. The specification is `Zenith2/Docs/Design/SHIP/Manifest.md` → "Git prerequisites" (ruling GIT-LFS-PREREQ; requirement Z2-R-SHIP-154).
<!-- /informative -->

- [Z2-N-ARCH-194] `CMake/toolchains/msvc-x64.cmake` and `CMake/toolchains/clang-linux-x64.cmake` read the compiler pin (`msvc.clVersion`, the `llvm` pin's `version`) and select the pinned compiler, and `zenith_begin_build` compares `CMAKE_CXX_COMPILER_VERSION` against that pin (a toolchain file runs before the compiler is identified) and stops the configure on a mismatch with `Zenith2: compiler <found> does not match the pinned <pinned> in ThirdParty/manifest.json`; on Android presets `zenith_begin_build` checks `ANDROID_NDK_REVISION` against the `ndk` pin's `pkgRevision` the same way.
- [Z2-N-ARCH-195] `Zenith2/Tools/Setup/Setup-Zenith2.ps1` is the clean-machine setup script: it installs or verifies every `toolchain` pin, downloads and verifies every `dependencies` source, installs the WSL distro with its pinned packages, and applies the `linux-tsan` kernel setting; `-Check` verifies without changing anything and exits 0 when everything matches or 1 on any mismatch, listing each mismatch.
- [Z2-N-ARCH-196] Every download is verified against its manifest SHA-256 before use, and a mismatch deletes the download and fails the setup; no configure step downloads anything (no `FetchContent` or `ExternalProject` download at configure time).

### Compiler cache

- [Z2-N-ARCH-197] sccache is the compiler launcher for MSVC and for NDK and Linux clang (`CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` set by the toolchain files and the Android preset), pinned by the manifest.
- [Z2-N-ARCH-198] The cache directory is machine-wide and shared by every checkout and lane: `%LOCALAPPDATA%\Zenith2\sccache` on Windows and `$HOME/.zenith2/sccache` in WSL, for every compiler.
- [Z2-N-ARCH-239] Every configure preset sets `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` to `sccache` (the Windows and Linux toolchain files, and the Android presets' cache variables), resolved from the `PATH` that `Enter-Zenith2Env.ps1` prepends with the pinned sccache directory.
- [Z2-N-ARCH-232] The shared cache is capped with `SCCACHE_CACHE_SIZE=60G`.
- [Z2-N-ARCH-233] The Build step prints `sccache --show-stats` hit rates in its report, so cold and warm builds are visible.

## Third-party integration

The dependency table (what each dependency is for and which library owns it) is in Architecture.md → "Third-party dependencies". This section specifies the manifest format and how dependencies become targets.

### The manifest dependencies section

Each entry of `dependencies` is an object with these fields:

| Field | Type | Meaning |
|---|---|---|
| `name` | string | the CMake-visible name used in `THIRD_PARTY`, which matches it exactly; lower-case kebab case `^[a-z0-9]+(-[a-z0-9]+)*$`, at most 64 characters; unique |
| `version` | string | the upstream release or tag |
| `source` | object | `{ "url": string, "sha256": 64 hex digits, "stripPrefix": string, "format": "zip" \| "tar.gz" \| "tar.xz" }`, the pinned source archive; absent from a `prebuilt` or `host-tool` entry that uses `fromToolchain` |
| `fromToolchain` | string | optional, `prebuilt` and `host-tool` entries alone: the key of the `toolchain` section whose archive supplies the artefact (Slang, DXC, WARP, lavapipe), so each is pinned once; the entry then has no `source` and its `prebuilt` items carry only the layout fields |
| `cmakeSourceDir` | string | optional, `cmake-subproject` entries: the directory inside the extracted archive that `add_subdirectory` receives (zstd: `build/cmake`); the archive root when absent |
| `prebuilt` | array | `[{ "abi": "win-x64" \| "android-arm64-v8a" \| "android-x86_64" \| "linux-x64", "url": string, "sha256": 64 hex digits, "stripPrefix": string, "format": "zip" \| "tar.gz" \| "tar.xz" \| "nupkg" \| "exe", "include": [dir…], "link": [path…], "runtime": [path…], "runtimeSubdir": string, "executables": [path…] }]`; present only for `prebuilt` and `host-tool` entries (the archive fields absent with `fromToolchain`); `include` becomes `SYSTEM` include directories, `link` the import or static libraries (tools alone), `runtime` the files staged next to every executable whose link closure reaches the entry, under `runtimeSubdir`, and `executables` the `host-tool` executables, each an `IMPORTED` target `<name>::<stem>` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "The prebuilt array") |
| `licence` | string | SPDX licence expression |
| `licenceElected` | string | required exactly when `licence` contains `OR`: the one allowed alternative under which Zenith2 uses the dependency (an expression without `OR`) |
| `integration` | enum | `cmake-subproject`, `zenith-wrapper`, `header-only`, `prebuilt`, `host-tool` |
| `owner` | string, or array | the Zenith2 library (or tool) whose `Internal/` may include it; an array only for a `header-only` entry shared by sibling backends of one API library (`renderdoc-app`: `["ZenithRHI_D3D12", "ZenithRHI_Vulkan"]`); or a benchmark target name `Zenith_Bench_<Name>` (the one harness that measures a selection candidate before its ruling; usage `tools` or `tests`); or one of the reserved tokens `tests` (test infrastructure such as doctest, linked into every test, conformance, fuzz and integration target when `autoLink` is true), `benchmark` (linked only into benchmark targets: every one when `autoLink` is true, as nanobench, otherwise only a benchmark naming it, as a selection candidate before its ruling), `test-runs` (a runtime redistributable no target links, staged next to test executables: WARP, lavapipe, the Vulkan validation layer) and `host` (a host executable or library built from a `cmake-subproject` and linked by no Zenith2 target, inside `targets` alone); no target is named after a token (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Owner values") |
| `autoLink` | boolean | optional, default `false`; accepted only with the owner tokens `tests` and `benchmark` |
| `definitions` | object | optional, `cmake-subproject` and `zenith-wrapper` entries: the per-platform shape of `options`; each pair becomes `<KEY>=<value>` in `target_compile_definitions(<target> PRIVATE …)` of every target of the entry (`LUA_USE_LONGJMP=0` on Luau) |
| `dependsOn` | array | optional: the names of other entries this entry's build consumes (KTX-Software on `zstd`); every `dependsOn` entry is created first, and the graph is acyclic |
| `pins` | object | optional: facts about the pinned version that build code reads or a test re-measures (`includeDir`, the one key every mode reads; Luau's `apiLinkage`); a re-measured pin that differs fails its test and never updates itself (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Recorded pins") |
| `shipping` | enum | optional, `usage: runtime` entries alone: `included` (the default) or `excluded`; an excluded dependency (Tracy, [Z2-N-ARCH-124]; the RenderDoc in-application API) is linked as `$<$<NOT:$<CONFIG:Shipping,ShippingTest>>:<target>>`, reached only from code inside `if constexpr (!Zenith_IsShippingBuild())`, and omitted from a Shipping package's licence inventory |
| `platforms` | array | subset of `windows`, `android`, `linux` |
| `language` | enum | `c`, `cpp`, `binary` |
| `options` | object | CMake cache variables set before `add_subdirectory` (`cmake-subproject` entries), as `{ "common": {…}, "windows": {…}, "android": {…}, "linux": {…} }`; a platform object's keys override `common` |
| `licenceFile` | string | the licence file's path inside the archive |
| `licenceSha256` | 64 hex digits | the SHA-256 of that licence file, checked by `Setup-Zenith2.ps1 -Check` |
| `sanitizerExempt` | boolean | `true` only for a `prebuilt` redistributable that the OS or driver loader loads and nothing links (D3D12 Agility SDK runtime, WARP, lavapipe); requires `decision` |
| `decision` | string | the `Z2-D-NNN` that admits `sanitizerExempt` |
| `patches` | array | `[{ "file": "ThirdParty/patches/<name>/<n>.patch", "sha256": 64 hex digits, "decision": "Z2-D-NNN" }]` |
| `usage` | enum | `runtime`, `tools`, `tests` |
| `targets` | array | optional: `[{ "target": <CMake target>, "owner": <library or host>, "usage": <enum>, "exceptions": bool, "hostExecutable": string, "platforms": [..] }, …]` (`platforms`, optional, a subset of the entry's: an element for another platform need not exist), overriding `owner` and `usage` per CMake target of a multi-target dependency (Luau: `Luau.VM` and `Luau.CodeGen` owned by ZenithScript for runtime; `Luau.Ast` and `Luau.Compiler` owned by ZenithScriptCompiler for tools); `exceptions` (default `false`) compiles that target with the exceptions-on row and is accepted on the targets of the entry `luau` alone (Z2-D-017), and `hostExecutable` (with `owner` `host` and `usage` `tools` alone) names a host executable's output (`luau-analyze`), built into `ZENITH_HOST_TOOLS_DIR` by `zenith_host_tools` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "The targets array") |

An example entry:

```json
{
	"name": "sdl3",
	"version": "3.2.x",
	"source": { "url": "https://github.com/libsdl-org/SDL/releases/download/release-3.2.x/SDL3-3.2.x.tar.gz", "sha256": "<64 hex digits>", "stripPrefix": "SDL3-3.2.x", "format": "tar.gz" },
	"licence": "Zlib",
	"integration": "cmake-subproject",
	"owner": "ZenithPlatform",
	"platforms": ["android", "linux", "windows"],
	"language": "c",
	"options": {
		"common": { "SDL_SHARED": "OFF", "SDL_STATIC": "ON", "SDL_TEST_LIBRARY": "OFF" },
		"linux": { "SDL_VIDEO": "OFF", "SDL_AUDIO": "OFF", "SDL_RENDER": "OFF", "SDL_GPU": "OFF", "SDL_CAMERA": "OFF", "SDL_HAPTIC": "OFF", "SDL_JOYSTICK": "OFF", "SDL_X11": "OFF", "SDL_WAYLAND": "OFF", "SDL_PIPEWIRE": "OFF", "SDL_PULSEAUDIO": "OFF", "SDL_ALSA": "OFF" }
	},
	"licenceFile": "LICENSE.txt",
	"licenceSha256": "<64 hex digits>",
	"patches": [],
	"usage": "runtime"
}
```

The `3.2.x` placeholder in this example stands for whichever exact release the Stage 0 reproducibility-manifest ticket pins; the manifest itself carries exact versions and hashes.

Integration modes:

| Mode | Meaning | Examples |
|---|---|---|
| `cmake-subproject` | the upstream CMake project is added with `add_subdirectory(<cache>/<name>/<sha256>[/<cmakeSourceDir>] EXCLUDE_FROM_ALL)` after setting the `options` cache variables | `sdl3`, `jolt`, `zstd`, `luau`, `meshoptimizer`, `ktx-software`, `fastgltf`, `tracy`, `flecs`, `miniaudio` |
| `zenith-wrapper` | `ThirdParty/<name>/CMakeLists.txt` globs the upstream sources from the cache and builds a static library | `imgui`, `volk`, `vma`, `d3d12ma`, `physx`, `zlib` (upstream 1.3.1's CMake renames `zconf.h` inside its source directory, which would modify the hash-verified dependency cache) |
| `header-only` | an `INTERFACE` target exposing the include directory | `entt`, `doctest`, `nanobench`, `vulkan-headers`, `vulkan-profiles` |
| `prebuilt` | a binary artefact imported per ABI | `d3d12-agility-sdk`, `warp` |
| `host-tool` | an executable used by offline tools, never linked | `slang`, `dxc` |

- [Z2-N-ARCH-199] Upstream sources are integrated as released, never forked, rewritten or reorganised; a local change is a patch file listed in the entry's `patches` array with the decision ID that approved it, applied by the setup script after verification.
- [Z2-N-ARCH-200] A dependency's headers are visible only to its `owner` library, which is the one library naming it in `THIRD_PARTY` (for a dependency with several targets, such as Luau, each target has one owner); a second library naming the same target is a configure error unless it is a test, benchmark or tool target, or the entry is `header-only` and its `owner` array lists both libraries, every one a `KIND BACKEND` of the same `BACKEND_OF` library; the manifest's ownership kinds are checked at the end of the configure against the declaration store (`zenith_add_tool` records `ZENITH_DECL_<Name>_TOOL_KIND`), the setup script's script mode checking structure only, and an owner a pin names before the ticket that declares it is reported at the end of the configure (one `STATUS` line per entry and owner), not refused, until it is declared, while a case-insensitive near miss of a declared name is refused naming the declared spelling (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Validation rules", rule K4).
- [Z2-N-ARCH-201] In the `win-asan`, `linux-tsan` and `Hwasan` builds every C and C++ dependency compiled into a runtime target (Jolt, PhysX, Luau, flecs, EnTT, zlib, zstd, SDL3 and the rest) is compiled from its pinned source with the sanitizer's flags; a `prebuilt` C or C++ library linked into a runtime target of a sanitizer build is a configure error.
- [Z2-N-ARCH-234] A `prebuilt` entry marked `sanitizerExempt` with a `decision` ID (the D3D12 Agility SDK runtime, WARP and lavapipe, which the OS or driver loader loads and nothing links) is admitted in sanitizer builds; the exemption exists only as that manifest field, and an exempt entry that a target links is a configure error.
- [Z2-N-ARCH-237] SDL3 on the `linux-tsan` host is configured with the `linux` options of its manifest entry, which turn off video, audio, render, GPU, camera, haptic, joystick and every window-system and audio-server backend, so the headless tree links no window-system or audio library.
- [Z2-N-ARCH-202] A sanitizer suppression-file entry is admitted only with a decision ID in a comment on the line above it, and suppression files live in `Zenith2/Tools/Gates/Suppressions/` (`asan.supp`, `tsan.supp`, `ubsan.supp`, `hwasan.supp`).
- [Z2-N-ARCH-203] No runtime target links Slang or DXC; they are `host-tool` entries used by `ZenithShaderCompiler`, and libgraph's link-surface stage fails when a Slang or DXC symbol appears in any runtime target.

### The dependency cache

- [Z2-N-ARCH-204] Verified dependency sources live in the machine-wide cache `%LOCALAPPDATA%\Zenith2\thirdparty\<name>\<dir>\` (Windows) or `$HOME/.zenith2/thirdparty/<name>/<dir>/` (WSL, and the native Linux CI host that `Setup-Zenith2.ps1`'s Linux host mode provisions, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Linux host mode"), shared by every checkout, where `<dir>` is the source `<sha256>` of an unpatched entry, `<sha256>-p<first 16 hex digits of the entry's patch-set SHA-256>` of a patched one (so two lanes whose manifests differ only in patches never share a tree) and `<abi>-<sha256 of that ABI's archive>` of a `prebuilt` or `host-tool` item with its own archive; a `fromToolchain` entry has no `thirdparty` directory (its tree is the toolchain install). Each tree's stamp `<dir>.zenith2-stamp.json` sits beside it, never inside (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Cache layout"). `CMake/ZenithThirdParty.cmake` derives the path from the manifest and fails the configure with `Zenith2: <name> <version> missing from the dependency cache; run Tools/Setup/Setup-Zenith2.ps1` when it is absent, and with `Zenith2: <name> <version>: dependency cache stamp does not match ThirdParty/manifest.json; run Tools/Setup/Setup-Zenith2.ps1` when the stamp's source or patch-set SHA-256 differs from the manifest's.

## Games in CMake

### The zenith_add_game helper

```cmake
zenith_add_game(<Name>)
```

A game's `Games/<Name>/CMakeLists.txt` is that single line. The helper performs these steps:

1. Check that `<Name>` equals the directory name and matches `^[A-Z][A-Za-z0-9]{0,31}$`, and that every top-level entry of `Games/<Name>/` is `game.json`, `CMakeLists.txt`, a directory named in `content.roots`, `Tests`, `Workload`, `Acceptance`, `.migration` (the gitignored backup directory of authoring-document migration, Zenith2/Docs/Contracts.md → "Compatibility policy per kind") or, from Stage 4, `.editor` (the optional, gitignored directory of the editor's per-user state, `Zenith2/Docs/Design/EDIT.md` → "Untracked editor files") and `package.recipe.json` (an optional regular file, PACK's package recipe, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Game directory entries"; a directory of that name is refused).
2. Read `Games/<Name>/game.json` with `file(READ)` and parse it with `string(JSON … ERROR_VARIABLE …)`; validate every key (§ "The game.json schema"). The three checks CMake's JSON reader does not make reliably — a duplicated key, a file above 65,536 bytes (`Games/<Name>/game.json: file size <n> bytes exceeds the 65536-byte limit`) and a leading UTF-8 byte-order mark (`Games/<Name>/game.json: a UTF-8 byte-order mark is not allowed`) — are the C++ validator's, run by `<Name>_Validate` (step 8).
3. When the current preset's target OS is not in `platforms`, print `zenith_add_game(<Name>): not built for <os> (platforms in game.json)` and return.
4. Resolve modules: each name maps to the `KIND MODULE` library with that `MODULE_NAME`; check the module-dependency closure (Architecture.md → "Engine modules and game projects").
5. Resolve backends: `backends.ecs` to the `BACKEND_OF ZenithECS` library with that `BACKEND_NAME`; `backends.physics` (per platform) likewise when `Physics` is selected; both compression backends in every game and `ZenithOnline_Offline` when `Online` is selected; on Windows both RHI backends, on Android `ZenithRHI_Vulkan`.
6. Generate `${ZENITH_OUT}/Games/<Name>/<configure-preset>/Zenith_GameComposition.generated.cpp` (content in Architecture.md → "Composition root and Player").
7. Create the executable: on Windows `add_executable(<Name> …)`, on Android `add_library(<Name> SHARED …)` producing `lib<Name>.so`; sources are the generated composition TU; links are `ZenithPlayer`, `ZenithRuntime`, the resolved modules and backends; on Android `ZenithPlayer` is linked as `$<LINK_LIBRARY:WHOLE_ARCHIVE,ZenithPlayer>` so that its entry point reaches the shared object (Architecture/CompositionRoot.md → "Player layout").
8. Create `<Name>_Validate` (`ZenithNewGame validate <Name>`, the C++ validator, from `ZENITH_HOST_TOOLS_DIR`), `<Name>_Content` (the cook of the game's content roots) and `<Name>_Types` (the `.d.luau` generation); `<Name>_Content` and `<Name>_Types` depend on `<Name>_Validate`, and the executable depends on all three, so a `game.json` the C++ validator refuses stops the build before the cook and the link.
9. On Android presets, generate the Gradle project and the `<Name>_Apk` target (§ "Android packaging").
10. Record the game in `declared_deps.json`.

- [Z2-N-ARCH-246] A top-level entry of `Games/<Name>/` outside the list of step 1 fails the configure with `Games/<Name>: '<entry>': unknown top-level entry; allowed are game.json, CMakeLists.txt, the content.roots directories, Tests, Workload, Acceptance, .migration, .editor and package.recipe.json`, and none of `Tests`, `Workload`, `Acceptance`, `.migration` and `.editor` is ever a content root, so test inputs, workload definitions, acceptance records, migration backups and editor state are never cooked; the one test input inside a content root, the behaviour-state migration fixtures under `Scripts/Tests/` (Zenith2/Docs/Scripting.md [Z2-N-SCRIPT-400]), is skipped by `<Name>_Content`, which cooks nothing below the `Tests/` directory of the `Scripts` content root.
- [Z2-N-ARCH-205] A game directory contains no compiled-language source: `SourceLayout` fails on any `.cpp`, `.c`, `.h`, `.hpp` or `.inl` under `Games/`.
- [Z2-N-ARCH-206] The game executable's only source is the generated composition TU; every other object comes from linked engine, module, backend and Player libraries.
- [Z2-N-ARCH-207] Cooking and type generation run through the host tools (`ZenithCook --game <Name> --platform <windows|android> --out ${ZENITH_OUT}/Cooked/<Name>/<platform>` and `ZenithCook --game <Name> --step luau-types`, which writes `${ZENITH_OUT}/Generated/Luau/<Name>/` (Zenith2/Docs/Scripting.md → "Modules, bindings and generated types")) with `DEPFILE` outputs, so an unchanged content root re-cooks nothing.
- [Z2-N-ARCH-208] Generated `.d.luau` files are written under `${ZENITH_OUT}/Generated/Luau/<Name>/` and never into `Games/<Name>/`, so a build never leaves an untracked file in the source tree.

### The game.json schema

```json
{
	"schemaVersion": 1,
	"name": "OpenWorldSlice",
	"displayName": "Open World Slice",
	"version": "0.1.0",
	"platforms": ["windows", "android"],
	"modules": ["AI", "Animation", "Audio", "Gameplay", "Physics", "Streaming", "UI"],
	"backends": {
		"ecs": "Flecs",
		"physics": { "windows": "PhysX", "android": "Jolt" }
	},
	"content": {
		"roots": ["Assets", "Schemas", "Scenes", "Scripts"],
		"startupScene": "Scenes/Main.scene.json"
	},
	"android": {
		"applicationId": "com.zenith2.openworldslice",
		"versionCode": 1,
		"orientation": "landscape"
	}
}
```

| Key | Type | Presence | Allowed values and rules |
|---|---|---|---|
| `schemaVersion` | integer | always | `1`; any other value fails with `Games/<Name>/game.json: schemaVersion: game.json schemaVersion <n> is not supported; migrate it with ZenithNewGame --migrate` |
| `name` | string | always | equals the directory name |
| `displayName` | string | always | 1–64 UTF-8 bytes, no control characters |
| `version` | string | always | `MAJOR.MINOR.PATCH`, each 0–65535 in decimal without a leading zero (`^(0\|[1-9][0-9]{0,4})\.(0\|[1-9][0-9]{0,4})\.(0\|[1-9][0-9]{0,4})$`) |
| `platforms` | array of string | always | non-empty, unique, each `windows` or `android` |
| `modules` | array of string | always (may be empty) | unique; each a registered `MODULE_NAME` (Architecture.md → "Engine modules and game projects") |
| `backends.ecs` | string | always | `Custom`, `Flecs` or `EnTT` |
| `backends.physics` | string or object | iff `Physics` is in `modules` | `Jolt` or `PhysX`; or an object whose keys are exactly the entries of `platforms`, each `Jolt` or `PhysX` |
| `content.roots` | array of string | always | non-empty, unique; each `^[A-Z][A-Za-z0-9]{0,31}$`, an existing directory directly under `Games/<Name>/`, and not `Tests`, `Workload`, `Acceptance`, `.migration` or `.editor` |
| `content.startupScene` | string | always | `^[A-Za-z0-9_./-]{1,255}$`; no empty, `.` or `..` component; its first component is one of `content.roots`; ends `.scene.json`; the file exists |
| `scriptRecovery` | object | optional | keys `policy`, `delayTicks` and `maxRecoveriesPerModule`, each present when the object is (the recovery policy of Zenith2/Docs/Scripting.md → "Quarantine and hot reload", [Z2-N-SCRIPT-423]) |
| `scriptRecovery.policy` | string | with `scriptRecovery` | `manual` or `automatic` |
| `scriptRecovery.delayTicks` | integer | with `scriptRecovery` | 1–3600 |
| `scriptRecovery.maxRecoveriesPerModule` | integer | with `scriptRecovery` | 0–100 |
| `android.applicationId` | string | iff `android` in `platforms` | `^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*){2,}$` |
| `android.versionCode` | integer | iff `android` in `platforms` | 1–2100000000 |
| `android.orientation` | string | iff `android` in `platforms` | `landscape`, `sensorLandscape`, `portrait`, `sensorPortrait` |
| `x-<anything>` | any | optional | reserved for tools; ignored by CMake and preserved by every tool that rewrites the file |

- [Z2-N-ARCH-209] `zenith_add_game` fails the configure on a missing key, a wrong type, a value outside its allowed set, or any top-level or nested key that is neither in the table nor prefixed `x-`, naming the key path in the one format `Games/<Name>/game.json: <keyPath>: <expected>, found <actual>` (a missing key: `<keyPath>: missing`; an unknown key: `<keyPath>: unknown key`), for example `Games/OpenWorldSlice/game.json: backends.physics.android: expected "Jolt" or "PhysX", found "Havok"`; the `schemaVersion` and PhysX-on-Android messages are instances of this format with their own detail text in place of `expected <expected>, found <actual>`, and the module-dependency message (bullet 267 of Architecture.md), which concerns the list as a whole, is `Games/<Name>/game.json: ` followed by its own text with no key path.
- [Z2-N-ARCH-210] `game.json` names no RHI and no compression backend: a Windows game links both RHI backends and selects one at run time through configuration, an Android game links `ZenithRHI_Vulkan`, and every game links both compression backends.
- [Z2-N-ARCH-211] When D-SEL-PHYSX-ANDROID (open, Stage 1) rules that PhysX is not an Android backend, `backends.physics` naming `PhysX` for `android` fails the configure with `Games/<Name>/game.json: backends.physics.android: PhysX is not an Android physics backend (capability matrix); use Jolt`.

### Android packaging

- [Z2-N-ARCH-212] The Android toolchain is NDK r27c with `ANDROID_PLATFORM android-34` (minSdk 34), `ANDROID_STL c++_static`, `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON`, and every shared object linked with `-Wl,-z,max-page-size=16384`.
- [Z2-N-ARCH-213] The APK is packaged by the Gradle project that `zenith_add_game` generates from `Zenith2/Player/GradleTemplate/` into `${ZENITH_OUT}/Games/<Name>/<configure-preset>/android/`.
- [Z2-N-ARCH-244] The generated Gradle project uses AGP at least 8.5.1, JDK 17 and the Gradle version from the manifest pins.
- [Z2-N-ARCH-245] Every Gradle build type except `hwasan` sets `packaging.jniLibs.useLegacyPackaging = false`.
- [Z2-N-ARCH-214] The `ApkCheck` gate step runs `llvm-readelf -l` on every `.so` inside the APK (including any prebuilt middleware shared object) and fails when any `PT_LOAD` segment's alignment is below `0x4000`.
- [Z2-N-ARCH-235] The `ApkCheck` gate step runs `zipalign -c -P 16 -v 4` on the release APK and fails on a non-zero exit.
- [Z2-N-ARCH-236] On Android the Player's entry point `SDL_main` is `extern "C"` with `__attribute__((visibility("default")))` in `Player/Internal/Android/Zenith_PlayerEntry_Android.cpp`, and `ApkCheck` fails when `llvm-readelf --dyn-syms` does not list `SDL_main` as a defined default-visibility function in the game's shared object.
- [Z2-N-ARCH-215] The HWASan APK is the Gradle build type `hwasan`, fed by the `Hwasan` configuration: `debuggable true`, `packaging.jniLibs.useLegacyPackaging = true` (so `extractNativeLibs=true`), and `lib/arm64-v8a/wrap.sh` containing `#!/system/bin/sh` and `LD_HWASAN=1 exec "$@"`; it is exempt from the zip-alignment check and still passes the ELF alignment check.
- [Z2-N-ARCH-216] The `android-x86_64` preset exists for emulator smoke runs alone; no acceptance, budget or parity result is ever taken from it, and no gate line of the base category builds it.

The Gradle build types:

| Build type | CMake configuration | Debuggable | Legacy JNI packaging | Signing | Checks |
|---|---|---|---|---|---|
| `debug` | `Debug` | yes | off | debug key | ELF alignment |
| `release` | `Release` | no | off | the per-machine development keystore (`ZENITH_DEV_KEYSTORE`, alias `zenith2-development`) | ELF alignment, zipalign 16 KB |
| `shipping` | `Shipping` | no | off | none in Gradle: no `signingConfig`, so Gradle writes `app-shipping-unsigned.apk`, which the release pipeline aligns and signs with the release key (`Zenith2/Docs/Design/SHIP/Release.md` → "Android APK signing") | ELF alignment, zipalign 16 KB |
| `hwasan` | `Hwasan` | yes | on (`extractNativeLibs=true`) | debug key | ELF alignment |

## The linux-tsan host

The `linux-tsan` preset is a headless host for tests alone with no GPU libraries, running ThreadSanitizer and UndefinedBehaviorSanitizer together.

- [Z2-N-ARCH-217] `Zenith2/Tools/Gates/Sanitizers.ps1` invokes the preset through `wsl.exe -d <pinned distro> -e bash -lc "<command>"`, passing `ZENITH_CHECKOUT_HASH` and the translated `/mnt/<drive>/…` source path explicitly in the command string; the distro name comes from the manifest's `wsl` pin.
- [Z2-N-ARCH-218] The `linux-tsan` binary directory is on the Linux filesystem (`$HOME/.zenith2/out/<checkout-hash>/linux-tsan`), never on DrvFS (`/mnt/…`).
- [Z2-N-ARCH-219] The setup script sets `vm.mmap_rnd_bits` to at most 28 in the WSL distro and on the native Linux CI host its Linux host mode provisions (`sysctl -w vm.mmap_rnd_bits=28`, persisted in `/etc/sysctl.d/99-zenith2-tsan.conf`), and `Sanitizers.ps1` reads the live value before running and fails with a message naming the setting when it is higher.
- [Z2-N-ARCH-220] The sanitizer runtime options are fixed: `TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:history_size=7:suppressions=<Suppressions/tsan.supp>` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:suppressions=<Suppressions/ubsan.supp>`.
- [Z2-N-ARCH-221] CI runs the `linux-tsan` preset natively on a Linux runner, and the Linux job runs only while a `linux-tsan` gate line is active in the committed Z2 category gate lists.

## Gate-budget rules that shape the build

- [Z2-N-ARCH-222] A warm gate phase (measure or verify) completes within 30 minutes of wall-clock time; the cap is the category's `phaseTimeoutMinutes`, and the build layout (one configure tree per toolchain, sccache, incremental Ninja builds) exists to keep a warm phase under it.
- [Z2-N-ARCH-223] The Build step detects a cold start (an empty or invalidated binary directory: a missing `CMakeCache.txt`, or a changed hash of `CMakePresets.json`, `ThirdParty/manifest.json` or `CMake/**`) and reports `cold: true` with its reason in the gate output, so the cold cap from the gate-budget rows applies (Zenith2/Docs/Budgets.md → "Gate budget rows").
- [Z2-N-ARCH-224] Every step has its own hard timeout sized for a cold build, recorded in the gate-budget rows; the step kills its process tree when the timeout passes.

## Hygiene gates fed by the build

The build produces the inputs of these gate steps; the steps themselves are specified in Zenith2/Docs/Contracts.md → "Tests and code hygiene gates".

| Step | Build input |
|---|---|
| `SourceLayout` | the glob partition rules above, applied to tracked and untracked files (`git status --porcelain --untracked-files=all`) |
| `CompileCoverage` | `declared_deps.json` `supported` matrices, File API compile groups, built objects |
| `Libgraph` | `declared_deps.json`, the File API, the `win-debug` objects and libraries built with `/Gy-` |
| `HeaderSelfContained` | one generated TU per public root header, compiled with the owning library's flags |
| `ForbiddenCheck` | sources plus File API compile groups (exception flags, forbidden tokens) |
| `TestSeparationCheck` | the link closure of every `Shipping` game executable |
| `ApkCheck` | the release and hwasan APKs |
| `Format` | `clang-format` from the pinned LLVM, over every source |

- [Z2-N-ARCH-225] `HeaderSelfContained` compiles its generated per-header TUs (specified in Zenith2/Docs/Contracts.md → "Tests and code hygiene gates") with each owning library's flags from the `win-debug` configuration of the `win` tree, so it adds no configure tree and no build flavour.
