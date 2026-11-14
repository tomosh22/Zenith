# MOD design — Game build

This file is part of the MOD design family. The entry document is `Zenith2/Docs/Design/MOD.md`; this file specifies how `zenith_add_game` turns `Games/<Name>/game.json` into an executable: the CMake implementation plan, the `game.json` schema v1 with every validation message, the module and backend mapping tables, the generated composition translation unit, the Android Gradle wiring and the CMake-level fixture tests. The contract it elaborates is `Zenith2/Docs/Architecture/BuildSystem.md → "Games in CMake"`, `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` and `Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table"`; nothing here relaxes them. Every obligation traces to a `Z2-R-MOD-NNN` in `Zenith2/Docs/Requirements/MOD.md`.

A line `<!-- elaboration -->` marks a detail designed here beyond the core documents. A block `<!-- assumption: pending <AREA> -->` states an interface this file assumes from an area whose design document is not yet signed off.

## zenith_add_game implementation

### Inputs and preconditions

`CMake/ZenithAddGame.cmake` defines one public function, `zenith_add_game(<Name>)`, and the private helpers below. It is included by `CMake/ZenithBuild.cmake` and has no top-level command other than `function()` definitions and `include_guard(GLOBAL)`, so the validator it contains also runs in CMake script mode (`cmake -P`) for the fixture tests of § "CMake fixture tests".

The function runs inside `Games/<Name>/CMakeLists.txt`, after every `Engine/`, `Player/`, `Tools/`, `Benchmarks/` and `Tests/` subdirectory has been added (the root order of `Zenith2/Docs/Architecture/BuildSystem.md → "Top-level tree and helper modules"`), so every library declaration it queries already exists.

<!-- elaboration -->
The declaration store it queries is written by `zenith_add_library` step 2 (Zenith2/Docs/Architecture/BuildSystem.md → "The zenith_add_library helper", "Target creation steps") as global properties, one set per declared library, recorded before the `SUPPORTED` check so a library absent from the current preset is still known:

| Global property | Value |
|---|---|
| `ZENITH_DECLARATIONS` | the list of every declared library name, in declaration order |
| `ZENITH_DECL_<Lib>_KIND` | `ENGINE`, `MODULE`, `BACKEND`, `PLAYER` or `GENERATED` |
| `ZENITH_DECL_<Lib>_DEPENDS` | the `DEPENDS` list |
| `ZENITH_DECL_<Lib>_SUPPORTED` | the matrix, expanded to configure-preset names and sorted by byte order |
| `ZENITH_DECL_<Lib>_MODULE_NAME` | `MODULE_NAME`, or empty |
| `ZENITH_DECL_<Lib>_BACKEND_OF` | `BACKEND_OF`, or empty |
| `ZENITH_DECL_<Lib>_BACKEND_NAME` | `BACKEND_NAME`, or empty |
| `ZENITH_DECL_<Lib>_FACTORY` | `FACTORY`, or empty |
| `ZENITH_DECL_<Lib>_THIRD_PARTY` | the library's `THIRD_PARTY` names in byte order, recorded by SHIP's `zenith_add_library` completion before the `SUPPORTED` check and read by runtime staging (A3 cross-area row SH20 (a); `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Runtime file staging") |
| `ZENITH_DECL_<Lib>_TEST_THIRD_PARTY` | the library's `TEST_THIRD_PARTY` names, as above |
| `ZENITH_DECL_Zenith_Bench_<Name>_BENCHMARK` | recorded by SHIP's `zenith_add_benchmark` for each benchmark (row SH20 (a)) |
| `ZENITH_TOOLS`, `ZENITH_DECL_<Tool>_…` | the same for `zenith_add_tool` targets (used by the name-collision check) |
| `ZENITH_GAMES` | appended by `zenith_add_game` step 14: every game name |

A library is present in the current preset exactly when `if(TARGET <Lib>)` holds, because `zenith_add_library` returns before creating a target outside its matrix.

### Step by step

`zenith_add_game(<Name>)` performs these steps in order. Steps 1 to 7 only read and validate; no target exists until step 8, so a failed validation leaves no half-made game in the build graph.

| # | Step | Failure behaviour |
|---|---|---|
| 1 | Check the argument count is 1, `<Name>` equals `${CMAKE_CURRENT_SOURCE_DIR}`'s last path component, matches `^[A-Z][A-Za-z0-9]{0,31}$`, is not a reserved Windows device name (case-insensitive `CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`), does not start with `Zenith`, and is not in `ZENITH_DECLARATIONS`, `ZENITH_TOOLS` or `ZENITH_GAMES` | `FATAL_ERROR` with the name-rule message of § "Validation messages" |
| 2 | List the game directory's top-level entries with `file(GLOB … CONFIGURE_DEPENDS LIST_DIRECTORIES true RELATIVE …)` and check each against the allowed set of § "Game directory entries" | collected error |
| 3 | Read `game.json` (§ "Reading game.json through the CMake JSON reader") | collected error; steps 4–7 are skipped when the text is not JSON |
| 4 | Validate every key (`zenith_validate_game_json`, § "The validator functions") | collected errors |
| 5 | If the current `ZENITH_TARGET_OS` (lower-cased: `windows`, `android`, `linux`) is not in `platforms`: print `STATUS` `zenith_add_game(<Name>): not built for <os> (platforms in game.json)`, record the game for `declared_deps.json` (step 14) and return | — |
| 6 | Resolve modules (§ "Module resolution") | collected errors |
| 7 | Resolve backends (§ "Backend resolution") | collected errors |
| 7a | When any error was collected: emit each with `message(SEND_ERROR …)` in collection order, then `message(FATAL_ERROR "zenith_add_game(<Name>): <n> error(s) in Games/<Name>/game.json")` | configure fails |
| 8 | Write the generated files (§ "Generated file writing") | — |
| 9 | Create the executable target (§ "Target creation") | — |
| 10 | Create `<Name>_Validate`, `<Name>_Content` and `<Name>_Types` (§ "Content and type targets") | — |
| 11 | Register the game's tests (§ "Game test registration") | collected errors, reported as in step 7a |
| 12 | On Windows presets, stage the D3D12 Agility runtime (§ "Windows executable specifics") | — |
| 13 | On Android presets, generate the Gradle project and `<Name>_Apk` (§ "Android Gradle wiring") | — |
| 14 | Append `<Name>` to `ZENITH_GAMES` and record the game for `declared_deps.json` and `composition.json` (§ "Build records") | — |
| 15 | Print the configure report line (§ "Configure report") | — |

<!-- elaboration -->
Collecting errors instead of stopping at the first one lets a single configure report every problem in a `game.json`, which is what a person fixing a hand-edited file needs; the fixture tests assert every expected message appears.

### Game directory entries

<!-- elaboration -->
The top-level entries allowed in `Games/<Name>/`:

| Entry | Kind | Present in | Meaning |
|---|---|---|---|
| `game.json` | file | every game | the descriptor |
| `CMakeLists.txt` | file | every game | exactly `zenith_add_game(<Name>)` (the `SourceLayout` gate step enforces the one-call rule) |
| each directory named in `content.roots` | directory | every game | cooked content roots |
| `Tests` | directory | optional | `*.gametest.json` files and the input scripts they name (§ "Game test registration") |
| `Workload` | directory | optional (workload games and flagship slices) | `<W>.workload.json` workload definitions (Zenith2/Docs/Budgets.md → "Workload definitions"), and beside them `<W>.editorscript.json`, the W-Editor automation script (`Zenith2/Docs/Design/EDIT/Automation.md` → "Measurement procedure") |
| `Acceptance` | directory | optional (flagship slices) | the acceptance package (Zenith2/Docs/Budgets.md → "Production-quality acceptance") |
| `.migration` | directory | optional; gitignored | authoring-document migration backups under `.migration/backups/<UTC timestamp>/` (Zenith2/Docs/Contracts.md → "Compatibility policy per kind", [Z2-N-CONTRACT-541]) |
| `.editor` | directory | optional; gitignored (`Games/*/.editor/` in `Zenith2/.gitignore`); from Stage 4 | the editor's per-user state: sessions, layouts, autosaves, the recovery manifest, the session lock, the trash (`Zenith2/Docs/Design/EDIT.md` → "Untracked editor files"); never a content root, like `.migration` (A3 cross-area row ED29) |
| `package.recipe.json` | file | optional; from Stage 4 | PACK's packaging recipe, the authoring document `Zenith.Package.RecipeDocument` (`Zenith2/Docs/Design/PACK/Formats.md` → "Packaging recipe"); a game without it packages everything into the base unit. It is not under a content root, so it is never cooked as content; PACK's shipping cook and package build read it (`Zenith2/Docs/Design/PACK/Tooling.md`), and the editor's packaging panel creates and edits it (A5 cross-area row PkB11) |

A top-level `<batch>.z2journal` or `game.json.z2tmp` (the journal and temporary file of an interrupted journaled write, [Z2-N-CONTRACT-540]) fails with `Games/<Name>: '<entry>': an interrupted migration is pending; run ZenithNewGame --migrate <Name> to recover it`. Any other top-level entry fails with `Games/<Name>: '<entry>': unknown top-level entry; allowed are game.json, CMakeLists.txt, the content.roots directories, Tests, Workload, Acceptance and .migration`. A directory named `Tests`, `Workload`, `Acceptance` or `.migration` is never a content root (`content.roots` naming one fails, § "Validation messages"), so test inputs, workload definitions and acceptance records are never cooked into a package.

From Stage 4 the allowed set also holds `.editor` (Z2T-MOD-070), as `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_game helper" states in step 1 and [Z2-N-ARCH-246], which fixes the line's text (cross-doc item X312, A5 cross-area row MB28). That core line ends `… Tests, Workload, Acceptance, .migration and .editor`; the next paragraph adds `package.recipe.json` to it.

<!-- assumption: pending ARCH (X453) -->
From Stage 4 the allowed set also holds the file `package.recipe.json` (Z2T-MOD-097; A5 cross-area rows PkB11, PkB13, ruling PACK-R2-RULINGS R2-M8), and the unknown-entry line ends `… Tests, Workload, Acceptance, .migration, .editor and package.recipe.json`. Only a regular file of exactly that name is admitted: a directory named `package.recipe.json`, or a file of any other spelling or case, is an unknown entry. Stage 4 is when PACK's packaging panel first writes the file into a game directory and `<Name>_ShippingContent` first reads it; before Stage 4 the one recipe in the repository belongs to PACK's fixture game under `Zenith2/Tests/Package/Fixtures/PackGame/`, which ZenithCook cooks with `--game-root` and which never reaches `zenith_add_game`. The CMake check (`_zenith_game_check_entries`) and the C++ check (`Zenith_ValidateGame`, `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Validation in C++") admit the same set, since both are held to the same fixtures. `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_game helper" does not list the file yet: the step-1 entry list and the line text of [Z2-N-ARCH-246] are cross-doc item X453.
<!-- /assumption -->

### Reading game.json through the CMake JSON reader

1. `if(NOT EXISTS "${dir}/game.json")` fails `Games/<Name>/game.json: missing`.
2. `file(READ "${dir}/game.json" strJson)`.
3. `string(JSON strType ERROR_VARIABLE strErr TYPE "${strJson}")`; a non-empty `strErr` fails `Games/<Name>/game.json: not valid JSON: <strErr>`; a type other than `OBJECT` fails `Games/<Name>/game.json: (root): expected an object, found <type lower-cased>`.
4. `set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${dir}/game.json")`, so an edit re-runs the configure through Ninja's automatic re-run, the one regeneration mechanism (Z2-D-001).

Every value is then read through one helper, so that type checks are uniform:

```cmake
# _zenith_json_get(<outValue> <outType> <json> <member>...)
#   Sets <outType> to NULL|NUMBER|STRING|BOOLEAN|ARRAY|OBJECT, or MISSING when the member path does not exist.
#   Uses string(JSON … ERROR_VARIABLE …) so a missing member never stops the configure.
function(_zenith_json_get outValue outType json)
	string(JSON strType ERROR_VARIABLE strErr TYPE "${json}" ${ARGN})
	if(strErr)
		set(${outType} MISSING PARENT_SCOPE)
		set(${outValue} "" PARENT_SCOPE)
		return()
	endif()
	string(JSON strValue ERROR_VARIABLE strErr GET "${json}" ${ARGN})
	set(${outType} "${strType}" PARENT_SCOPE)
	set(${outValue} "${strValue}" PARENT_SCOPE)
endfunction()
```

| JSON type | What `GET` returns | How the validator checks it |
|---|---|---|
| integer | its decimal text | `TYPE` is `NUMBER` and the text matches `^-?[0-9]+$`; then a range check with `if(… GREATER_EQUAL …)` on at most 10-digit values (longer text fails the range check before numeric comparison) |
| string | the unescaped string | `TYPE` is `STRING` |
| boolean | `ON` or `OFF` | `TYPE` is `BOOLEAN` |
| array | its JSON text | `TYPE` is `ARRAY`; elements are read by index `0 … LENGTH−1` |
| object | its JSON text | `TYPE` is `OBJECT`; keys are enumerated with `string(JSON … MEMBER <json> <path…> <index>)` for `0 … LENGTH−1` |

### Checks only the C++ validator makes

<!-- elaboration -->
Three checks cannot be made reliably through CMake's JSON reader, so they live in the C++ validator of `Zenith2/Docs/Design/MOD/ProjectCreation.md → "Validation in C++"` alone and run at build time through `<Name>_Validate` (§ "Content and type targets"):

| Check | Why not in CMake | C++ validator message |
|---|---|---|
| duplicate keys | the reader parses into a keyed object and keeps the last value of a duplicated key, so the second occurrence is gone before any member is enumerated | `Games/<Name>/game.json: <keyPath>: duplicate key` |
| a file over 65,536 bytes | a size limit is a parser limit of the untrusted-input reader (Zenith2/Docs/Architecture.md → "Security architecture"), which the C++ reader enforces before parsing | `Games/<Name>/game.json: file size <n> bytes exceeds the 65536-byte limit` |
| a UTF-8 byte-order mark | the reader's treatment of a mark is not a documented behaviour of CMake's JSON reader | `Games/<Name>/game.json: a UTF-8 byte-order mark is not allowed` |

In the configure, a duplicated key yields the last value and a byte-order mark yields whatever CMake's reader reports (typically the not-valid-JSON message); neither is relied on. The build refuses all three through `<Name>_Validate`, and `ZenithNewGame validate` refuses them without a build.

<!-- assumption: pending ARCH -->
Zenith2/Docs/Architecture/BuildSystem.md → "Games in CMake" ("The zenith_add_game helper") currently says in step 2: "Read `Games/<Name>/game.json` with `file(READ)`, refusing a file above 65,536 bytes (`Games/<Name>/game.json: file size <n> bytes exceeds the 65536-byte limit`) and one that begins with a UTF-8 byte-order mark (`Games/<Name>/game.json: a UTF-8 byte-order mark is not allowed`), and parse it with `string(JSON … ERROR_VARIABLE …)`", and in step 8: "Create `<Name>_Content` (the cook of the game's content roots) and `<Name>_Types` (the `.d.luau` generation), and make the executable depend on both." The batched Architecture patch moves the size and byte-order-mark refusals to the C++ validator run by `<Name>_Validate` and adds that target to step 8 (cross-area note M27); the message texts are unchanged.
<!-- /assumption -->

Control characters in `displayName` are detected by building a CMake character class from `string(ASCII 1 2 … 31 127 …)` once per configure and testing with `string(REGEX MATCH …)`; `string(LENGTH)` counts bytes, which is the unit of the 64-byte limit.

### The validator functions

```cmake
# zenith_validate_game_json(<prefix> JSON <text> GAME_DIR <dir> GAME_NAME <name> TARGET_OS <os>)
#   Validates game.json schema v1 without creating any target.
#   Outputs, all in the caller's scope:
#     <prefix>_ERRORS               list of complete message lines, in key order of the table "game.json schema v1"
#     <prefix>_DISPLAY_NAME, <prefix>_VERSION
#     <prefix>_PLATFORMS            sorted by byte order
#     <prefix>_MODULES              as listed (canonical order is computed in module resolution)
#     <prefix>_ECS                  Custom|Flecs|EnTT
#     <prefix>_PHYSICS_<os>         Jolt|PhysX for each os in PLATFORMS (empty when Physics is not selected)
#     <prefix>_ROOTS                sorted by byte order
#     <prefix>_STARTUP_SCENE        the game-relative path, e.g. Scenes/Main.scene.json
#     <prefix>_ANDROID_APPLICATION_ID, <prefix>_ANDROID_VERSION_CODE, <prefix>_ANDROID_ORIENTATION
#     <prefix>_SCRIPT_RECOVERY_DECLARED (ON|OFF), _POLICY (manual|automatic), _DELAY_TICKS, _MAX_RECOVERIES
function(zenith_validate_game_json prefix)
```

The function checks the keys in the order of the table "game.json schema v1" and appends one line per violation. It checks the module names only for membership in the known module-name set (`ZENITH_DECL_*_MODULE_NAME` values); closure, platform support and backend presence are module and backend resolution's work, because they need the current preset.

<!-- elaboration -->
The known module-name set is read from the declaration store in project mode. In script mode (fixture tests) the runner defines the store from `Zenith2/CMake/Tests/GameJson/declarations.cmake`, a fixed table of the thirteen modules of `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` and every backend of the table "Backend to target mapping"; `CMake_Declarations_MirrorMatches` checks that the live store is a subset of the fixed table: every live `MODULE` or `BACKEND` declaration must equal its fixture row, and a fixture row with no live counterpart is reported as `not yet landed` without failing, so the check is green from Stage 2, when no module exists, to Stage 5, when `VFX` lands. When the runner is given `-DZENITH_ACCEPTANCE_STAGE=<N>` (passed by the Stage N acceptance run, absent otherwise), a `not yet landed` row whose "First stage" is at most N is an error, so a module that misses its stage fails that stage's acceptance. The same fixed table is what `zenith_end_build`'s table-only generation resolves against (`Zenith2/Docs/Design/MOD.md` → "Generated test compositions").

### Module resolution

1. For each name in `<prefix>_MODULES`: find the library `L` with `ZENITH_DECL_<L>_KIND` = `MODULE` and `ZENITH_DECL_<L>_MODULE_NAME` equal to the name. None: `Games/<Name>/game.json: modules[<i>]: expected one of <the thirteen names, byte-ordered, comma-separated>, found "<name>"`.
2. Module dependencies of `L` are the entries of `ZENITH_DECL_<L>_DEPENDS` whose kind is `MODULE`, mapped to their `MODULE_NAME`. For each dependency `B` not in the selection: `Games/<Name>/game.json: module '<A>' requires module '<B>'; add "<B>" to modules` ([Z2-N-ARCH-267]). Nothing is ever added implicitly.
3. When `L` is not a target in the current preset (`if(NOT TARGET <L>)`): `Games/<Name>/game.json: modules[<i>]: module '<A>' is not supported in preset <ZENITH_PRESET> (ZENITH_SUPPORTED of <L>)`.
4. Compute the canonical order (`Zenith2/Docs/Design/MOD.md → "Canonical module order"`) into `<prefix>_MODULES_CANONICAL` and the libraries into `<prefix>_MODULE_LIBS`, index-aligned.

### Backend resolution

1. **ECS.** Find the library with `BACKEND_OF` = `ZenithECS` and `BACKEND_NAME` = `<prefix>_ECS`. It must be a target in the preset, else `Games/<Name>/game.json: backends.ecs: backend '<B>' is not supported in preset <ZENITH_PRESET> (ZENITH_SUPPORTED of <Lib>)`.
2. **Physics** (only when `Physics` is selected). For the current OS, the backend is `<prefix>_PHYSICS_<os>`; find `BACKEND_OF ZenithPhysics` with that `BACKEND_NAME`. When the library exists but its matrix excludes the current preset: on Android with `PhysX`, the message is `Games/<Name>/game.json: backends.physics.android: PhysX is not an Android physics backend (capability matrix); use Jolt` ([Z2-N-ARCH-211]); in every other case the generic unsupported-backend message of step 1 with key path `backends.physics` or `backends.physics.<os>`. Whether PhysX's matrix includes the Android presets is the ruling of D-SEL-PHYSX-ANDROID (open, Stage 1); this step reads the matrix and hard-codes no platform.
3. **Compression.** Both libraries with `BACKEND_OF ZenithCompression` (`ZenithCompression_Zlib`, `ZenithCompression_Zstd`), always, in byte order of `BACKEND_NAME`. Either absent is an engine defect: `FATAL_ERROR` `zenith_add_game(<Name>): compression backend <Lib> is missing from preset <ZENITH_PRESET>`.
4. **RHI.** Every library with `BACKEND_OF ZenithRHI` that is a target in the preset, ordered `Vulkan` first and then byte order (so Windows yields `Vulkan`, `D3D12` and Android yields `Vulkan`). An empty set is the same engine-defect `FATAL_ERROR` naming `ZenithRHI`.
5. **Online** (only when `Online` is selected). Every library with `BACKEND_OF ZenithOnline` that is a target in the preset, in platform preference order: every store backend (every such library whose `BACKEND_NAME` is not `Offline`) first, in byte order of `BACKEND_NAME`, then `Offline` last (A3 cross-area row ON18 (a); core-doc row X338 for [Z2-N-ARCH-268]). Stage 4 yields `Offline` alone; Stage 6 yields `Steam`, `Offline` on Windows and `GooglePlay`, `Offline` on Android, each store backend by its own `SUPPORTED` matrix. ZenithOnline tries `online.backend` first and then this order, and names no backend itself. `ZenithOnline_SteamCore` and `ZenithOnline_GooglePlayCore` are engine libraries with no `BACKEND_OF` (core-doc rows X358, X363): resolution never sees them, and they reach a game only through their store backend's `DEPENDS`, so this step needs nothing for them (row ON18 (e)).
6. For each resolved backend record `FACTORY` and the public header `<Lib>/Zenith_<Family>_<BACKEND_NAME>.h`, where `<Family>` is `BACKEND_OF` without its `Zenith` prefix (`ECS`, `RHI`, `Compression`, `Physics`, `Online`), the header rule of [Z2-N-ARCH-169].

### Target creation

| Target | Windows presets (`win`, `win-asan`) | Android presets |
|---|---|---|
| the game | `add_executable(<Name> "${gen}/Zenith_GameComposition.generated.cpp")` | `add_library(<Name> SHARED "${gen}/Zenith_GameComposition.generated.cpp")`, output `lib<Name>.so` |
| links (`PRIVATE`) | `ZenithPlayer`, `ZenithRuntime`, the module libraries in canonical order, the backend libraries in the order ECS, RHI, Compression, Physics, Online | as Windows, with `ZenithPlayer` as `$<LINK_LIBRARY:WHOLE_ARCHIVE,ZenithPlayer>`, plus `SDL3-static` made whole-archive by `LINK_LIBRARY_OVERRIDE_SDL3-static` (§ "Android shared object specifics") |
| flags | `zenith_apply_flags(<Name>)`; Shipping adds `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup` (Zenith2/Docs/Architecture/BuildSystem.md → "Compile and link flags") | `zenith_apply_flags(<Name>)`, which applies `-Wl,-z,max-page-size=16384 -Wl,--build-id=sha1 -Wl,--no-undefined` to the shared object |
| output directory | `${ZENITH_OUT}/<preset>/<Config>/bin/` | the same, then staged for Gradle (§ "Build types and JNI staging") |
| folder | `Games` | `Games` |
| dependencies | `<Name>_Validate`, `<Name>_Content`, `<Name>_Types` | `<Name>_Validate`, `<Name>_Content`, `<Name>_Types` |

`${gen}` is `${ZENITH_OUT}/Games/<Name>/<ZENITH_PRESET>/` (§ "Generated file writing"). The executable has no include directory of its own: the generated TU reaches every header through the include roots its linked libraries publish.

<!-- elaboration -->
Windows links no whole-archive because the CRT's reference to `main` pulls the entry object from `ZenithPlayer` ([Z2-N-ARCH-859]); Android needs it because the SDL activity looks `SDL_main` up by name ([Z2-N-ARCH-858]). Modules are never whole-archive: the composition TU's function-pointer references pull their entry objects.

### Generated file writing

<!-- elaboration -->
`zenith_add_game` writes into `${ZENITH_OUT}/Games/<Name>/<ZENITH_PRESET>/`, one directory per configure preset, because the Windows and Android trees resolve different RHI and physics backends into the same game and would otherwise overwrite each other's composition TU. The platform-independent record `composition.json` sits one level up. Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table" names `${ZENITH_OUT}/Games/<Name>/`; this file refines it to that directory's per-preset subdirectory.

| File | Content | Written by |
|---|---|---|
| `${ZENITH_OUT}/Games/<Name>/<preset>/Zenith_GameComposition.generated.cpp` | § "Generated composition TU template" | step 8 |
| `${ZENITH_OUT}/Games/<Name>/composition.json` | `Zenith2/Docs/Design/MOD.md → "The composition.json record"` | step 14 (every preset writes identical bytes) |
| `${ZENITH_OUT}/Games/<Name>/<preset>/android/` | the Gradle project (Android presets) | step 13 |
| `${ZENITH_OUT}/Games/<Name>/<preset>/jni/<Config>/<abi>/lib<Name>.so` | the staged shared object (Android presets) | a post-build step of the game target |

Every text file is produced by composing the full content in a CMake variable, reading the existing file when present, and calling `file(WRITE)` only when the bytes differ. An unchanged `game.json` therefore never touches a timestamp and never triggers a rebuild of the game (the same rule as [Z2-N-ARCH-182]). Line endings are LF and the files end with one LF.

### Content and type targets

| Target | Command | Outputs | Rebuild trigger |
|---|---|---|---|
| `<Name>_Validate` | `${ZENITH_HOST_TOOLS_DIR}/ZenithNewGame validate <Name> --root ${ZENITH_ROOT}`, then `cmake -E touch` of the stamp | `${ZENITH_OUT}/Games/<Name>/<preset>/validate.stamp` | `DEPENDS` on `game.json`, every `Schemas/*.schema.json` (globbed with `CONFIGURE_DEPENDS`) and the `ZenithNewGame` executable under `ZENITH_HOST_TOOLS_DIR`; a non-zero exit fails the build with the validator's messages |
| `<Name>_Content` | `${ZENITH_HOST_TOOLS_DIR}/ZenithCook --game <Name> --platform <windows\|android> --out ${ZENITH_OUT}/Cooked/<Name>/<platform>` | a stamp file `${ZENITH_OUT}/Cooked/<Name>/<platform>/.stamp` plus the cooked tree | `DEPFILE ${ZENITH_OUT}/Cooked/<Name>/<platform>/cook.d`, written by the cook; `DEPENDS` on the cook executable under `ZENITH_HOST_TOOLS_DIR` ([Z2-N-ARCH-190]) and on `composition.json` |
| `<Name>_Types` | `${ZENITH_HOST_TOOLS_DIR}/ZenithCook --game <Name> --step luau-types` | `${ZENITH_OUT}/Generated/Luau/<Name>/.stamp` plus the files of Zenith2/Docs/Scripting.md → "Modules, bindings and generated types" | `DEPFILE ${ZENITH_OUT}/Generated/Luau/<Name>/types.d`; `DEPENDS` on the cook executable and `composition.json` |

All three are `add_custom_command` outputs wrapped in `add_custom_target`, created once per game per configure preset; `<Name>_Content` and `<Name>_Types` depend on `<Name>_Validate`, so a game whose `game.json` the C++ validator refuses never cooks. `<platform>` is `windows` on Windows presets and `android` on Android presets. The command lines are exactly those of [Z2-N-ARCH-207].

<!-- assumption: pending ASSET -->
The cook learns which modules the game selected by reading `${ZENITH_OUT}/Games/<Name>/composition.json` (the resolved, canonical record), not by re-validating `game.json`, so the module set the cook registers for schema validation and `.d.luau` generation is exactly the set the game links. The ASSET design (`Zenith2/Docs/Design/ASSET.md`) owns the cook's reading of it; `Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"` states that the cook registers exactly the selected set.
<!-- /assumption -->

### Game test registration

<!-- elaboration -->
A game carries no C++, so its automated tests are data: each `Games/<Name>/Tests/<Stem>.gametest.json` (format in `Zenith2/Docs/Design/MOD.md → "The gametest format"`) becomes one CTest test `Game_<Name>_<Stem>`:

1. Glob `Tests/*.gametest.json` with `CONFIGURE_DEPENDS`, sorted by byte order; `<Stem>` must match `^[A-Z][A-Za-z0-9]{0,31}$`, else `Games/<Name>/Tests/<file>: file name must be <Stem>.gametest.json with <Stem> matching ^[A-Z][A-Za-z0-9]{0,31}$`.
2. Validate the file with the same `_zenith_json_get` helper (messages prefixed `Games/<Name>/Tests/<Stem>.gametest.json:`).
3. In the `win` and `win-asan` presets only (the presets that run games on the build machine), `add_test(NAME Game_<Name>_<Stem> COMMAND $<TARGET_FILE:<Name>> --headless --assets-root ${ZENITH_OUT}/Cooked/<Name>/windows --exit-after-ticks <ticks> --timeout-seconds <timeoutSeconds> [--input-script <absolute path>] [--set <key>=<value>]…)` with `--set` pairs in byte order of key, and properties `TIMEOUT` = `timeoutSeconds + 30`, `LABELS "z2-game;z2-game-<Name>"`. The test passes on exit code 0 alone.
4. **A gametest that needs the game's package** (`requiresPackage` true, A5 cross-area row PkB13). A CTest test cannot depend on a build target, so the dependency is a CTest fixture on a test that builds the target:
   - Once per game with at least one such gametest, step 11 registers the setup test `add_test(NAME Game_<Name>_PackageBuild COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Debug --target <Name>_Package CONFIGURATIONS Debug)`, with properties `FIXTURES_SETUP Game_<Name>_Package`, `RESOURCE_LOCK zenith2-build-tree`, `TIMEOUT 1800` and `LABELS "z2-game;z2-game-<Name>"`. The lock is SHIP's build-tree lock (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Registry tail"). The tail locks only tests registered in the registry's own directory, so this step sets the lock itself.
   - Each such gametest is registered as in step 3 plus `CONFIGURATIONS Debug` and the property `FIXTURES_REQUIRED Game_<Name>_Package`.
   - In this way the gametest always runs against a Debug executable and the Debug package built from the same tree, which PACK's identity check needs (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Simulated deliverable units"). Under `ctest -C Release` neither test runs.
   - When the package build fails, or `<Name>_Package` does not exist, the setup test fails. CTest then reports every dependent gametest as not run, so the Test step fails, and the gametest never runs against a stale package.
   - `<Name>_Package` is PACK's target (`Zenith2/Docs/Design/PACK/Tooling.md`, from Stage 4); MOD only names it.
5. On Android presets the tests are not registered ([Z2-N-ARCH-180]); on-device runs belong to `needs-human` device tickets.

<!-- elaboration -->
**Test inputs, content roots and `content:/` paths.** These three rules hold together:
- The top-level `Tests/` directory of a game holds `*.gametest.json` files and the input scripts they name. The runtime reads input scripts from the source tree by absolute path (`--input-script`); it never reads them through `content:/`. `Tests/` is never a content root (§ "Game directory entries"), so nothing under it is cooked.
- A gametest's scene, assert behaviours and assets are content. They live under a content root, and the `set` value `content.startupScene` names them by logical path. The logical path of `Games/<Name>/Assets/<p>` is `content:/<p>`, and that of `Games/<Name>/<R>/<p>` for any other root `R` is `content:/<R>/<p>` (`Zenith2/Docs/Design/ASSET.md` → "Content root mapping").
- So `content:/Scenes/PackCheck/PackDelivery.scene.json` is `Games/<Name>/Scenes/PackCheck/PackDelivery.scene.json`, in the `Scenes` root: cooked, and present in every package a device runs. A `content:/Tests/<p>` path is `Games/<Name>/Assets/Tests/<p>`, a subdirectory of the `Assets` root that is cooked like any other. It is never the top-level `Tests/` directory.
- A scene placed in the top-level `Tests/` has no logical path. A gametest naming it fails startup at step 17 (exit code 3).
- The one cook exclusion inside a content root is `Scripts/Tests/` ([Z2-N-ARCH-246]).

### Windows executable specifics

<!-- assumption: pending RHI -->
The D3D12 Agility SDK runtime is loaded by the OS from a directory next to the executable, and the executable must export its SDK version and path. On Windows presets the generated TU therefore also defines the two exports (see § "Generated composition TU template") from the constants `uZENITH_D3D12_AGILITY_SDK_VERSION` and `szZENITH_D3D12_AGILITY_SDK_PATH` that `ZenithRHI_D3D12/Zenith_RHI_D3D12.h` declares, and `zenith_add_game` adds a post-build `copy_if_different` of the manifest's Agility SDK runtime DLLs into `$<TARGET_FILE_DIR:<Name>>/D3D12/`. The RHI design (`Zenith2/Docs/Design/RHI.md`) owns the constant names and values; Zenith2/Docs/Architecture.md → "Third-party dependencies" records the runtime DLL as a `sanitizerExempt` prebuilt.
<!-- /assumption -->

### Android shared object specifics

- The target is `SHARED` with `OUTPUT_NAME <Name>`, so the library is `lib<Name>.so`, and `@ZENITH_GAME_LIB@` is `<Name>`.
- `ZenithPlayer` is whole-archive, so `Player/Internal/Android/Zenith_PlayerEntry_Android.cpp`'s `SDL_main` (C linkage, default visibility) is in the shared object ([Z2-N-ARCH-236]).
- A `POST_BUILD` step copies `$<TARGET_FILE:<Name>>` to `${ZENITH_OUT}/Games/<Name>/<preset>/jni/$<CONFIG>/<abi>/lib<Name>.so` with `copy_if_different`, where `<abi>` is `ANDROID_ABI`.
- SDL3 is linked statically into the shared object and no other `.so` is produced or packaged ([Z2-N-ARCH-854]). Because `Zenith2Activity` is an SDL activity, the library must also carry SDL3's JNI entry points (`JNI_OnLoad` and the `nativeSetupJNI` family the activity calls as it loads the library), which live in SDL3's `SDL_android.c` object and which a plain static link pulls in only when another object references them; at Stages 0 and 1 nothing links SDL3 at all. So `_zenith_link_activity_runtime(<Name>)` (`CMake/ZenithAddGame.cmake`, every Android preset, from the Stage 0 subset onwards) links `SDL3-static` and sets the target property `LINK_LIBRARY_OVERRIDE_SDL3-static` to `WHOLE_ARCHIVE`. The override form, rather than a `$<LINK_LIBRARY:WHOLE_ARCHIVE,SDL3-static>` item, is required from Stage 2, where `ZenithPlatform` links `SDL3-static` without a feature and CMake refuses one library named with two features in one link unless an override resolves them.
- The same function adds a `POST_BUILD` export check before the JNI staging copy: `${CMAKE_COMMAND} -DZENITH_READELF=<the NDK's llvm-readelf> -DZENITH_LIBRARY=$<TARGET_FILE:<Name>> -P CMake/ZenithCheckApkExports.cmake`, which runs `llvm-readelf --dyn-syms --wide` and fails the build with `zenith apk: <file> does not export <symbol>` unless `SDL_main` and then `JNI_OnLoad` each appear as a defined `FUNC` of `GLOBAL` binding and `DEFAULT` visibility (the script also accepts `-DZENITH_READELF_OUTPUT=<file>` in place of running the tool, which its fixtures use). A library without either symbol would install and then fail at activity start on the device.

### Build records

Step 14 records the game for `zenith_end_build`, which writes the `games` array of `declared_deps.json` in the exact shape of Zenith2/Docs/Architecture/BuildSystem.md → "The declared_deps.json record" (`name`, `dir`, `platforms`, `modules`, `backends`), with `modules` in byte order, and writes `composition.json` for every game whose `game.json` validated. A game whose validation failed has already stopped the configure.

### Configure report

Step 15 prints one `STATUS` line per game and preset, so a CI log shows every game's resolution:

```text
zenith_add_game(OpenWorldSlice): windows modules=[Animation,Audio,Cinematics,Environment,Gameplay,Online,Physics,AI,Streaming,UI,VFX] ecs=Flecs physics=PhysX rhi=[Vulkan,D3D12] online=[Offline] composition=5d1f0a3c9e7b2248 build=91c4e07a2b6d38f5
```

The module list is the canonical order; `physics=` and `online=` are omitted when the module is not selected; `composition=` is the 16-digit value of `Zenith2/Docs/Design/MOD.md → "Composition hash"` and `build=` that of `Zenith2/Docs/Design/MOD.md → "Build identity"`.

## Module to target mapping

The module list is Zenith2/Docs/Architecture.md → "Engine modules and game projects" ("The module list"); this table adds what `zenith_add_game` and the generated TU need from each row.

| `MODULE_NAME` | Library | Root header | Module dependencies (from `DEPENDS`) | Backend family | `Start` form | `SUPPORTED` | First stage |
|---|---|---|---|---|---|---|---|
| `2D` | `Zenith2D` | `Zenith2D/Zenith_Module_2D.h` | — (2D physics per D-SEL-2DPHYS, open, Stage 2) | — | plain | `ZENITH_GPU` | S3 |
| `AI` | `ZenithAI` | `ZenithAI/Zenith_Module_AI.h` | `Physics` | — | plain | `ZENITH_GPU` | S4 |
| `Animation` | `ZenithAnimation` | `ZenithAnimation/Zenith_Module_Animation.h` | — | — | plain | `ZENITH_GPU` | S3 |
| `Audio` | `ZenithAudio` | `ZenithAudio/Zenith_Module_Audio.h` | — | — | plain | `ZENITH_GPU` | S3 |
| `Cinematics` | `ZenithCinematics` | `ZenithCinematics/Zenith_Module_Cinematics.h` | `Animation` | — | plain | `ZENITH_GPU` | S4 |
| `Environment` | `ZenithEnvironment` | `ZenithEnvironment/Zenith_Module_Environment.h` | — | — | plain | `ZENITH_GPU` | S4 |
| `Gameplay` | `ZenithGameplay` | `ZenithGameplay/Zenith_Module_Gameplay.h` | — | — | plain | `ZENITH_GPU` | S3 |
| `Net` | `ZenithNet` | `ZenithNet/Zenith_Module_Net.h` | — | — | plain | `ZENITH_GPU` | S3 |
| `Online` | `ZenithOnline` | `ZenithOnline/Zenith_Module_Online.h` | — | Online (link all supported, choose by configuration) | backend list | `ZENITH_GPU` | S4 |
| `Physics` | `ZenithPhysics` | `ZenithPhysics/Zenith_Module_Physics.h` | — | Physics (link one) | one factory | `ZENITH_GPU` | S3 |
| `Streaming` | `ZenithStreaming` | `ZenithStreaming/Zenith_Module_Streaming.h` | — | — | plain | `ZENITH_GPU` | S4 |
| `UI` | `ZenithUI` | `ZenithUI/Zenith_Module_UI.h` | — | — | plain | `ZENITH_GPU` | S3 |
| `VFX` | `ZenithVFX` | `ZenithVFX/Zenith_Module_VFX.h` | — | — | plain | `ZENITH_GPU` | S5 |

The first stage is the stage in which the owning area's roadmap delivers the module; before it, the library does not exist and `zenith_add_game` reports an unknown module. `Start` forms:

| Form | Declaration in the module's root header | Thunk in the generated TU |
|---|---|---|
| plain | `Zenith_Module_<M>_Start(Zenith_ModuleStartContext& xContext)` | none: the descriptor points at the entry point directly |
| one factory | `Zenith_Module_Physics_Start(Zenith_ModuleStartContext& xContext, Zenith_PfnCreatePhysics pfnCreateBackend)` | `Start<M>` calls it with `&<FACTORY>` of the resolved backend |
| backend list | `Zenith_Module_Online_Start(Zenith_ModuleStartContext& xContext, Zenith_Span<const Zenith_OnlineBackendEntry> axBackends)` | `Start<M>` calls it with the constexpr array of every resolved Online backend |

The backend-list form is Architecture's (Zenith2/Docs/Architecture.md → "Engine modules and game projects", "Module entry points"), which declares `Zenith_Module_Online_Start(…, Zenith_Span<const Zenith_OnlineBackendEntry> axBackends)`, and its "Assumed types" table places `Zenith_OnlineBackendEntry` and `Zenith_PfnCreateOnlineServices` in `ZenithOnline/Zenith_OnlineServices.h`.

<!-- assumption: pending ONLINE -->
The configuration key that chooses the Online entry at run time is the ONLINE design's (`Zenith2/Docs/Design/ONLINE.md`).
<!-- /assumption -->

## Backend to target mapping

| Family | API library | Backend library | `BACKEND_NAME` | Selected by | Linked into a game | Composition entry | `SUPPORTED` | First stage |
|---|---|---|---|---|---|---|---|---|
| ECS | `ZenithECS` | `ZenithECS_Custom` | `Custom` | `backends.ecs` | the selected one | `m_pfnCreateWorld`, `m_szECSBackendName` | `ZENITH_ALL` | S2 |
| ECS | `ZenithECS` | `ZenithECS_Flecs` | `Flecs` | `backends.ecs` | the selected one | as above | `ZENITH_ALL` | S2 |
| ECS | `ZenithECS` | `ZenithECS_EnTT` | `EnTT` | `backends.ecs` | the selected one | as above | `ZENITH_ALL` | S2 |
| RHI | `ZenithRHI` | `ZenithRHI_Vulkan` | `Vulkan` | none | every game | `m_axRHIBackends` entry `"Vulkan"` | `ZENITH_GPU` | S2 |
| RHI | `ZenithRHI` | `ZenithRHI_D3D12` | `D3D12` | none | every Windows game | `m_axRHIBackends` entry `"D3D12"` | `ZENITH_WINDOWS` | S2 |
| Compression | `ZenithCompression` | `ZenithCompression_Zlib` | `Zlib` | none | every game | `m_axCodecs` entry `"zlib"` | `ZENITH_ALL` | S2 |
| Compression | `ZenithCompression` | `ZenithCompression_Zstd` | `Zstd` | none | every game | `m_axCodecs` entry `"zstd"` | `ZENITH_ALL` | S2 |
| Physics | `ZenithPhysics` | `ZenithPhysics_Jolt` | `Jolt` | `backends.physics` | the selected one, when `Physics` is selected | the `StartPhysics` thunk | `ZENITH_GPU` | S3 |
| Physics | `ZenithPhysics` | `ZenithPhysics_PhysX` | `PhysX` | `backends.physics` | the selected one, when `Physics` is selected | the `StartPhysics` thunk | `ZENITH_GPU` until D-SEL-PHYSX-ANDROID (open, Stage 1) rules; `ZENITH_WINDOWS` if it rules against Android | S3 |
| Online | `ZenithOnline` | `ZenithOnline_Offline` | `Offline` | none | every game selecting `Online` | the `StartOnline` thunk's list | `ZENITH_GPU` | S4 |
| Online | `ZenithOnline` | `ZenithOnline_Steam` (pending ONLINE) | `Steam` | none | every Windows game selecting `Online`, before `Offline` | the `StartOnline` thunk's list | `ZENITH_WINDOWS` | S6 |
| Online | `ZenithOnline` | `ZenithOnline_GooglePlay` (pending ONLINE) | `GooglePlay` | none | every Android game selecting `Online`, before `Offline` | the `StartOnline` thunk's list | `ZENITH_ANDROID` | S6 |

- A composition entry's name is `BACKEND_NAME`, except codec entries, which are `BACKEND_NAME` lower-cased (`zlib`, `zstd`), the codec names the cooked headers record (Zenith2/Docs/Design/COMPRESS.md owns the codec IDs).
- The RHI configuration value `rhi.backend` is RHI's per-platform enum key (`vulkan`, `d3d12` on Windows, `vulkan` alone on Android; Zenith2/Docs/Rendering.md → "GPU baseline and profiles"); a value outside it is a configuration warning and the next-lower layer, finally the platform default `vulkan`, stays in effect ([Z2-N-ARCH-929], orchestrator ruling Q67 (a)), and step 12 maps the resolved value onto the `m_axRHIBackends` entry whose generated `BACKEND_NAME` equals it ignoring ASCII case. Backend resolution lists every RHI backend of the preset, so the mapping always finds an entry.
- A factory symbol is always the backend declaration's `FACTORY` value; the generator never spells a factory name itself. The factory pointer types (`Zenith_PfnCreateWorld`, `Zenith_PfnCreateDevice`, `Zenith_PfnCreateCompression`, `Zenith_PfnCreatePhysics`) are the ones Zenith2/Docs/Architecture/CompositionRoot.md → "The generated composition table" declares beside their factories (`Zenith_PfnCreateCompression` also being COMPRESS's, `Zenith2/Docs/Design/COMPRESS/PublicAPI.md` → "Zenith_Compression.h"), and only the name `Zenith_PfnCreateDevice` is pending the RHI design (`Zenith2/Docs/Design/MOD.md` → "Zenith_GameComposition.h"); whatever those factories are named by their owning designs (Zenith2/Docs/Rendering.md names the RHI factories `Zenith_CreateRHI_Vulkan` and `Zenith_CreateRHI_D3D12`), the generated TU takes the name from the declaration.

## game.json schema v1

### Keys

The schema is Zenith2/Docs/Architecture/BuildSystem.md → "Games in CMake" ("The game.json schema"); this table restates it with the elaborated keys added and marked.

| Key path | Type | Presence | Allowed values and rules |
|---|---|---|---|
| `schemaVersion` | integer | always | `1` |
| `name` | string | always | equals the directory name |
| `displayName` | string | always | 1–64 UTF-8 bytes, valid UTF-8, no control character (U+0000–U+001F, U+007F) |
| `version` | string | always | `^(0\|[1-9][0-9]{0,4})\.(0\|[1-9][0-9]{0,4})\.(0\|[1-9][0-9]{0,4})$`, each component ≤ 65535 |
| `platforms` | array of string | always | non-empty, unique, each `windows` or `android` |
| `modules` | array of string | always (may be empty) | unique; each a registered `MODULE_NAME` |
| `backends` | object | always | keys `ecs` and, iff `Physics` is in `modules`, `physics`; nothing else |
| `backends.ecs` | string | always | `Custom`, `Flecs` or `EnTT` |
| `backends.physics` | string or object | iff `Physics` is in `modules` | `Jolt` or `PhysX`; or an object whose keys are exactly the entries of `platforms`, each `Jolt` or `PhysX` |
| `content` | object | always | keys `roots` and `startupScene`; nothing else |
| `content.roots` | array of string | always | non-empty, unique; each `^[A-Z][A-Za-z0-9]{0,31}$`, an existing directory directly under `Games/<Name>/`, and not `Tests`, `Workload`, `Acceptance` or `.migration` |
| `content.startupScene` | string | always | `^[A-Za-z0-9_./-]{1,255}$`; no empty, `.` or `..` component; its first component is one of `content.roots`; ends `.scene.json`; the file exists |
| `android` | object | iff `android` is in `platforms` | keys `applicationId`, `versionCode`, `orientation`; nothing else |
| `android.applicationId` | string | iff `android` | `^[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*){2,}$`, at most 150 bytes |
| `android.versionCode` | integer | iff `android` | 1–2100000000 |
| `android.orientation` | string | iff `android` | `landscape`, `sensorLandscape`, `portrait`, `sensorPortrait` |
| `scriptRecovery` (elaboration) | object | optional | keys `policy`, `delayTicks`, `maxRecoveriesPerModule`, each required when the object is present |
| `scriptRecovery.policy` | string | with `scriptRecovery` | `manual` or `automatic` |
| `scriptRecovery.delayTicks` | integer | with `scriptRecovery` | 1–3600 |
| `scriptRecovery.maxRecoveriesPerModule` | integer | with `scriptRecovery` | 0–100 |
| `x-<anything>` | any | optional, at any object level | reserved for tools; ignored by CMake and preserved by every tool that rewrites the file |

<!-- elaboration -->
`scriptRecovery` is the key Zenith2/Docs/Scripting.md → "Quarantine and hot reload" ([Z2-N-SCRIPT-423]) places in `game.json`; the value ranges are this design's. When the key is absent, the policy is the configuration-dependent default of the `Zenith_ScriptRecoveryPolicy` enumeration that Scripting declares: `manual` in non-Shipping builds and `automatic` with `delayTicks` 60 and `maxRecoveriesPerModule` 3 in Shipping builds. When present, it applies in every configuration. The composition carries the declared value (`Zenith2/Docs/Design/MOD.md → "Zenith_GameComposition.h"`), and ZenithRuntime passes it to the script runtime at startup step 14.

<!-- elaboration -->
The tighter `content.*` character rules keep every path both a legal C++ string literal without escaping and a legal virtual path under Zenith2/Docs/Architecture/PlatformLayer.md → "Virtual filesystem" ("Path validation"); the existence checks make a typo a configure error instead of a startup failure.

### Validation messages

A missing or wrong-typed parent object (`backends`, `content`, `android`, `scriptRecovery`) emits exactly one line for itself, and its children are not visited, so `{}` yields one `missing` line per always-present top-level key and nothing for `backends.ecs` or `content.roots`.

Every message is one line. The generic form is `Games/<Name>/game.json: <keyPath>: <detail>`, where `<keyPath>` uses `.` between object keys and `[<i>]` for array indices, and `<detail>` is `expected <expected>, found <actual>` (`<actual>` is the JSON text of the value, or its lower-cased JSON type when it is a container). The table is complete: a validator emits no other text.

| Condition | Exact message |
|---|---|
| name rule | `zenith_add_game(<Name>): <Name> must equal the directory name, match ^[A-Z][A-Za-z0-9]{0,31}$, not be a Windows device name, not start with Zenith and not name an existing target` |
| unknown top-level entry | `Games/<Name>: '<entry>': unknown top-level entry; allowed are game.json, CMakeLists.txt, the content.roots directories, Tests, Workload, Acceptance and .migration` ([Z2-N-ARCH-246]); from Stage 4 the list ends `… .migration, .editor and package.recipe.json` (§ "Game directory entries") |
| interrupted migration pending (a top-level `<batch>.z2journal` or `game.json.z2tmp`) | `Games/<Name>: '<entry>': an interrupted migration is pending; run ZenithNewGame --migrate <Name> to recover it` |
| missing file | `Games/<Name>/game.json: missing` |
| not JSON | `Games/<Name>/game.json: not valid JSON: <CMake parser message>` |
| root not an object | `Games/<Name>/game.json: (root): expected an object, found <type>` |
| missing key | `Games/<Name>/game.json: <keyPath>: missing` |
| unknown key | `Games/<Name>/game.json: <keyPath>: unknown key` |
| wrong type or value | `Games/<Name>/game.json: <keyPath>: expected <expected>, found <actual>` |
| unsupported schema version | `Games/<Name>/game.json: schemaVersion: game.json schemaVersion <n> is not supported; migrate it with ZenithNewGame --migrate` |
| name mismatch | `Games/<Name>/game.json: name: expected "<Name>" (the directory name), found <actual>` |
| duplicate array entry | `Games/<Name>/game.json: <keyPath>[<i>]: duplicate of <keyPath>[<j>]` |
| `backends.physics` present without Physics | `Games/<Name>/game.json: backends.physics: unknown key`, followed by the `STATUS` line `zenith_add_game(<Name>): backends.physics is allowed only when Physics is in modules`, which the fixtures do not compare |
| `backends.physics` object keys differ from platforms | `Games/<Name>/game.json: backends.physics: expected exactly the keys of platforms (<list>), found <keys>` |
| content root missing on disk | `Games/<Name>/game.json: content.roots[<i>]: expected an existing directory Games/<Name>/<root>, found none` |
| content root reserved | `Games/<Name>/game.json: content.roots[<i>]: expected a content directory, found reserved name "<root>"` |
| startup scene outside the roots | `Games/<Name>/game.json: content.startupScene: expected a path whose first component is one of content.roots, found "<path>"` |
| startup scene file missing | `Games/<Name>/game.json: content.startupScene: expected an existing file, found none at Games/<Name>/<path>` |
| `android` block without the platform | `Games/<Name>/game.json: android: unknown key`, followed by the `STATUS` line `zenith_add_game(<Name>): android is allowed only when android is in platforms`, not compared |
| unknown module | `Games/<Name>/game.json: modules[<i>]: expected one of <names>, found "<name>"` |
| missing module dependency | `Games/<Name>/game.json: module '<A>' requires module '<B>'; add "<B>" to modules` |
| module unsupported in the preset | `Games/<Name>/game.json: modules[<i>]: module '<A>' is not supported in preset <preset> (ZENITH_SUPPORTED of <Lib>)` |
| backend unsupported in the preset | `Games/<Name>/game.json: <keyPath>: backend '<B>' is not supported in preset <preset> (ZENITH_SUPPORTED of <Lib>)` |
| PhysX on Android after a ruling against it | `Games/<Name>/game.json: backends.physics.android: PhysX is not an Android physics backend (capability matrix); use Jolt` |
| too large, byte-order mark, duplicate key | C++ validator only (§ "Checks only the C++ validator makes") |
| summary after collected errors | `zenith_add_game(<Name>): <n> error(s) in Games/<Name>/game.json` |

<!-- informative -->
The module-dependency message keeps the exact literal of [Z2-N-ARCH-267], which has no key path; the PhysX message places [Z2-N-ARCH-211]'s text in the detail position of the generic form, and the schema-version message places the text [Z2-N-ARCH-209] quotes there too. Architecture's [Z2-N-ARCH-209] calls both special messages "instances of this format"; the table above is the reading this design implements and the fixtures assert, recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` for Architecture to confirm.
<!-- /informative -->

### Stage 0 subset

<!-- elaboration -->
The Stage 0 scaffold ticket lands `zenith_add_game` before any runtime library exists (Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"). Its Stage 0 form:

| Aspect | Stage 0 | From Stage 2 |
|---|---|---|
| keys accepted | `schemaVersion`, `name`, `displayName`, `version`, `platforms`, `content`, `android`, `x-*`; any other key fails as an unknown key | the full table "Keys" |
| `content.startupScene` | checked for shape only (the file need not exist) | shape and existence |
| module and backend resolution | skipped (no module or backend library exists) | full |
| generated TU | the Stage 0 stub composition (`Zenith2/Docs/Design/MOD.md → "Stage 0 scaffold"`) | the full template |
| links | `ZenithPlayer` only, plus on Android `SDL3-static` whole-archive and the export check from the Stage 1 packaging ticket (§ "Android shared object specifics") | the full link set, with the same Android additions |
| content, types and test targets | not created | created |
| Android | the shared object and, from the Stage 1 packaging ticket, the Gradle project and `<Name>_Apk` | unchanged |

The Stage 2 game-model ticket replaces the subset with the full validator in one diff, together with `Games/Minimal/game.json` gaining `modules` and `backends`.

## Generated composition TU template

### Template text

The generated file for a Windows preset, with every substitution named:

```cpp
// Generated by zenith_add_game(@NAME@) from Games/@NAME@/game.json for preset @PRESET@. Never edited.
// composition @HASH16@ build @BUILD16@
#include "Player/Zenith_GameComposition.h"
@BACKEND_INCLUDES@
@MODULE_INCLUDES@

@AGILITY_EXPORTS@
namespace
{
@START_THUNKS@
	constexpr Zenith_ModuleDescriptor axMODULES[] =
	{
@MODULE_ROWS@
	};
	constexpr Zenith_RHIBackendEntry axRHI[] =
	{
@RHI_ROWS@
	};
	constexpr Zenith_CodecEntry axCODECS[] =
	{
		{ "zlib", &@ZLIB_FACTORY@ },
		{ "zstd", &@ZSTD_FACTORY@ },
	};
	constexpr Zenith_GameComposition xCOMPOSITION =
	{
		"@NAME@",
		"@VERSION@",
		@MODULES_SPAN@,
		&@ECS_FACTORY@,
		"@ECS_NAME@",
		axRHI,
		axCODECS,
		{ "@STARTUP_SCENE_VPATH@", @SCRIPT_RECOVERY_DECLARED@, @SCRIPT_RECOVERY_POLICY@, @SCRIPT_RECOVERY_DELAY@, @SCRIPT_RECOVERY_MAX@ },
		0x@HASHVALUE@ull,
		0x@BUILDVALUE@ull,
	};
}

const Zenith_GameComposition& Zenith_GetGameComposition()
{
	return xCOMPOSITION;
}
```

| Placeholder | Value |
|---|---|
| `@NAME@`, `@VERSION@`, `@PRESET@` | the game name, `version`, `ZENITH_PRESET` |
| `@HASH16@` | the printed spelling of `Zenith2/Docs/Design/MOD.md → "Composition hash"` ([Z2-N-ARCH-864]): the digest's first 16 lower-case hexadecimal digits as `string(SHA256)` prints them |
| `@BUILD16@` | the printed spelling of `Zenith2/Docs/Design/MOD.md → "Build identity"`, the same way |
| `@HASHVALUE@`, `@BUILDVALUE@` | the little-endian numeric values as 16 lower-case hexadecimal digits, most significant first: the printed spelling's byte pairs in reverse order |
| `@BACKEND_INCLUDES@` | one `#include "<Lib>/Zenith_<Family>_<BACKEND_NAME>.h"` per resolved backend, in the order ECS, RHI, Compression, Physics, Online |
| `@MODULE_INCLUDES@` | one `#include "<Lib>/Zenith_Module_<M>.h"` per module, in canonical order |
| `@AGILITY_EXPORTS@` | Windows presets: the two lines of § "Include rules"; Android presets: empty |
| `@START_THUNKS@` | one thunk per module whose `Start` form is not plain (§ "Thunk rules"), in canonical order; empty otherwise |
| `@MODULE_ROWS@` | one `{ "<M>", uZENITH_MODULE_INTERFACE_VERSION, &Zenith_Module_<M>_RegisterTypes, <start>, <backendName>, "<deps>" },` per module in canonical order; `<deps>` is the module's selected module dependencies comma-joined in byte order (`"Physics"` for `AI`), or empty; `<start>` is `&Zenith_Module_<M>_Start` or `&Start<M>`; `<backendName>` is `"<BACKEND_NAME>"` for a one-factory module, `"<N1>,<N2>"` (comma-joined, no spaces) for a backend-list module, `nullptr` otherwise |
| `@MODULES_SPAN@` | `axMODULES` when at least one module is selected; `{}` when none (an empty `constexpr` array is ill-formed, so neither `axMODULES` nor its rows are emitted then) |
| `@RHI_ROWS@` | `{ "<BACKEND_NAME>", &<FACTORY> },` per resolved RHI backend, `Vulkan` first |
| `@ZLIB_FACTORY@`, `@ZSTD_FACTORY@`, `@ECS_FACTORY@` | the `FACTORY` values of the resolved backends |
| `@ECS_NAME@` | the ECS `BACKEND_NAME` |
| `@STARTUP_SCENE_VPATH@` | the startup scene's virtual path (`Zenith2/Docs/Design/MOD.md → "Startup scene and loose content root"`) |
| `@SCRIPT_RECOVERY_DECLARED@` | `true` when `scriptRecovery` is present, else `false` |
| `@SCRIPT_RECOVERY_POLICY@` | `SCRIPT_RECOVERY_POLICY_MANUAL` or `SCRIPT_RECOVERY_POLICY_AUTOMATIC` (Scripting's enumerators); `SCRIPT_RECOVERY_POLICY_MANUAL` when undeclared |
| `@SCRIPT_RECOVERY_DELAY@`, `@SCRIPT_RECOVERY_MAX@` | the declared integers followed by `u`; `0u` when undeclared |

Indentation is one tab per level, matching `Zenith2/.clang-format`, so the generated file is clang-format clean without running the formatter.

### Include rules

- The generated TU includes `Player/Zenith_GameComposition.h`, the backend root headers and the module root headers, and nothing else: no `Internal/` header, no third-party header, no standard header (the composition types need none).
- On Windows presets `@AGILITY_EXPORTS@` is:

```cpp
extern "C" __declspec(dllexport) extern const uint32 D3D12SDKVersion = uZENITH_D3D12_AGILITY_SDK_VERSION;
extern "C" __declspec(dllexport) extern const char* const D3D12SDKPath = szZENITH_D3D12_AGILITY_SDK_PATH;
```

  Both are constants in a read-only section, so the writable-data scan ([Z2-N-CONTRACT-151]) finds nothing; the constant names are the RHI assumption of § "Windows executable specifics".

### Thunk rules

A thunk exists only to bind a module's `Start` to its resolved backends, and has exactly this shape:

```cpp
	Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> StartPhysics(Zenith_ModuleStartContext& xContext)
	{
		return Zenith_Module_Physics_Start(xContext, &Zenith_CreatePhysics_Jolt);
	}
```

```cpp
	constexpr Zenith_OnlineBackendEntry axONLINE[] = { { "Offline", &Zenith_CreateOnlineServices_Offline } };
	Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> StartOnline(Zenith_ModuleStartContext& xContext)
	{
		return Zenith_Module_Online_Start(xContext, axONLINE);
	}
```

A thunk body is a single `return` of the entry-point call; it holds no condition, loop, local variable or other logic ([Z2-N-ARCH-843]).

<!-- elaboration -->
The generator also has a table-only mode, which resolves a fixture's declared module set against the fixed table `Zenith2/CMake/Tests/GameJson/declarations.cmake` rather than the live store, used for the runtime's generated test compositions (`Zenith2/Docs/Design/MOD.md` → "Generated test compositions"): it emits only the `constexpr` module-descriptor array, in a namespace named for the fixture, forward-declares each plain module's two entry points instead of including module headers, emits no thunk, and emits no `Zenith_GetGameComposition`. It accepts plain modules only and fails with `zenith_add_game: table-only mode supports plain modules only` otherwise. The factory names shown are examples; the generator writes each backend declaration's `FACTORY`.

### Worked examples

`Games/Minimal` (no modules, Custom ECS) on the `win` preset:

```cpp
// Generated by zenith_add_game(Minimal) from Games/Minimal/game.json for preset win. Never edited.
// composition 704cef4b6c1da71a build fde0475d91f2eebb
#include "Player/Zenith_GameComposition.h"
#include "ZenithECS_Custom/Zenith_ECS_Custom.h"
#include "ZenithRHI_Vulkan/Zenith_RHI_Vulkan.h"
#include "ZenithRHI_D3D12/Zenith_RHI_D3D12.h"
#include "ZenithCompression_Zlib/Zenith_Compression_Zlib.h"
#include "ZenithCompression_Zstd/Zenith_Compression_Zstd.h"

extern "C" __declspec(dllexport) extern const uint32 D3D12SDKVersion = uZENITH_D3D12_AGILITY_SDK_VERSION;
extern "C" __declspec(dllexport) extern const char* const D3D12SDKPath = szZENITH_D3D12_AGILITY_SDK_PATH;

namespace
{
	constexpr Zenith_RHIBackendEntry axRHI[] =
	{
		{ "Vulkan", &Zenith_CreateRHI_Vulkan },
		{ "D3D12", &Zenith_CreateRHI_D3D12 },
	};
	constexpr Zenith_CodecEntry axCODECS[] =
	{
		{ "zlib", &Zenith_CreateCompression_Zlib },
		{ "zstd", &Zenith_CreateCompression_Zstd },
	};
	constexpr Zenith_GameComposition xCOMPOSITION =
	{
		"Minimal",
		"0.1.0",
		{},
		&Zenith_CreateWorld_Custom,
		"Custom",
		axRHI,
		axCODECS,
		{ "content:/Scenes/Main.scene.json", false, SCRIPT_RECOVERY_POLICY_MANUAL, 0u, 0u },
		0x1aa71d6c4bef4c70ull,
		0xbbeef2915d47e0fdull,
	};
}

const Zenith_GameComposition& Zenith_GetGameComposition()
{
	return xCOMPOSITION;
}
```

The two values are the golden of `Zenith2/Docs/Design/MOD.md` → "Hash golden", which `CMake_Composition_NoModules` pins. The factory names follow the owning documents (`Zenith_CreateRHI_*` from Zenith2/Docs/Rendering.md, `Zenith_CreateCompression_*` from Zenith2/Docs/Contracts.md and Zenith2/Docs/WorkerGuide.md); the generator copies whatever the declarations record.

The current Architecture example (OpenWorldSlice selecting Audio, Online with the Offline backend and Physics with Jolt, on Flecs) is reproduced by the fixture `CMake_Composition_PhysicsThunk`, whose golden file differs from Zenith2/Docs/Architecture/CompositionRoot.md's illustration only in the additions this design makes (`m_szModuleDependencies`, `m_ulBuildIdentity`, the per-preset header comment) and in the factory names the declarations carry.

## Android Gradle wiring

### Gradle project generation

On Android presets, step 13 of `zenith_add_game`:

1. Copies `Zenith2/Player/GradleTemplate/` into `${ZENITH_OUT}/Games/<Name>/<preset>/android/` with `configure_file(… @ONLY)` for every `*.in` file (dropping `.in`) and `configure_file(… COPYONLY)` for the rest, file by file in byte order of path, so unchanged outputs keep their timestamps.
2. Copies SDL3's Java activity sources `org/libsdl/app/*.java` from the verified SDL3 source in the dependency cache (`%LOCALAPPDATA%\Zenith2\thirdparty\SDL3\<sha256>\android-project\app\src\main\java\org\libsdl\app\`) into `app/src/main/java/org/libsdl/app/`, `COPYONLY`.
3. Creates the `<Name>_Apk` target (§ "The Apk target").

The template's file list and the activity and manifest content are Zenith2/Docs/Architecture/CompositionRoot.md → "Android packaging". From Stage 4 the template also carries the accessibility bridge class `app/src/main/java/org/zenith2/player/Zenith2Accessibility.java` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract"), `Zenith2Activity.getAccessibilityHostView()` returning SDL's surface view, `-keep class org.zenith2.player.Zenith2Accessibility { *; }` in `app/proguard-rules.pro` and `fontScale` in `android:configChanges` (A3 cross-area row P20 (a); the core text's half is cross-document item X269 (3)).

<!-- assumption: pending ARCH (X428) -->
From Stage 4 the template also carries PLAT's connectivity bridge class `app/src/main/java/org/zenith2/player/Zenith2Connectivity.java` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity"; A3 cross-area row P24 (a)): its constructor `(long handle, android.content.Context context)` calls `ConnectivityManager.registerDefaultNetworkCallback`; `onCapabilitiesChanged` with `NET_CAPABILITY_INTERNET` and `NET_CAPABILITY_VALIDATED` calls `nativeConnectivityChanged(handle, 2)`; `onLost` and `onUnavailable` call `nativeConnectivityChanged(handle, 1)`; `synchronized void detach()` unregisters the callback and zeroes the handle, and every callback reads the handle under the same monitor, the accessibility bridge's handshake. The class declares `private static native void nativeConnectivityChanged(long handle, int state)`. `app/proguard-rules.pro` gains `-keep class org.zenith2.player.Zenith2Connectivity { *; }` and the manifest gains `android.permission.ACCESS_NETWORK_STATE`. The core text's half is cross-document item X428.
<!-- /assumption -->

### Placeholders

The placeholders of Zenith2/Docs/Architecture/CompositionRoot.md → "Android packaging", with the values this design fixes, plus two it adds:

| Placeholder | Value |
|---|---|
| `@ZENITH_APPLICATION_ID@` | `android.applicationId` |
| `@ZENITH_VERSION_CODE@` | `android.versionCode` |
| `@ZENITH_VERSION_NAME@` | `version` |
| `@ZENITH_DISPLAY_NAME@` | `displayName`, XML-escaped (`&`, `<`, `>`, `"`, `'`) because it lands in `strings.xml` |
| `@ZENITH_GAME_LIB@` | `<Name>` |
| `@ZENITH_ORIENTATION@` | `android.orientation` |
| `@ZENITH_ABI@` | `ANDROID_ABI` (`arm64-v8a` or `x86_64`) |
| `@ZENITH_MIN_SDK@` | `34` |
| `@ZENITH_COMPILE_SDK@`, `@ZENITH_TARGET_SDK@` | the manifest's `androidSdk` pins |
| `@ZENITH_AGP_VERSION@`, `@ZENITH_GRADLE_SHA256@` | the manifest's `agp` and `gradle` pins |
| `@ZENITH_JNI_DIR@` | `${ZENITH_OUT}/Games/<Name>/<preset>/jni`; each build type appends `/<Config>` (§ "Build types and JNI staging") |
| `@ZENITH_ASSETS_DIR@` | `${ZENITH_OUT}/Cooked/<Name>/android/` |
| `@ZENITH_NDK_VERSION@` (elaboration) | the manifest's `ndk` pin as AGP's `ndkVersion` string (for r27c, the NDK's `Pkg.Revision`) |
| `@ZENITH_BUILD_TYPES@` (elaboration) | `debug,release,shipping,hwasan` for `android-arm64`; `debug,release` for `android-x86_64` |

### Build types and JNI staging

`app/build.gradle.kts.in` declares the build types of Zenith2/Docs/Architecture/BuildSystem.md → "Games in CMake" ("Android packaging"); this design fixes their JNI source directories:

| Build type | CMake configuration | `jniLibs.srcDirs` | Native debug symbols |
|---|---|---|---|
| `debug` | `Debug` | `@ZENITH_JNI_DIR@/Debug` | kept (`keepDebugSymbols += "**/*.so"`) |
| `release` | `Release` | `@ZENITH_JNI_DIR@/Release` | stripped by AGP with the pinned NDK |
| `shipping` | `Shipping` | `@ZENITH_JNI_DIR@/Shipping` | stripped; the unstripped object is SHIP's symbol-retention input |
| `hwasan` | `Hwasan` | `@ZENITH_JNI_DIR@/Hwasan` | kept |

The game target's post-build step stages `lib<Name>.so` into `@ZENITH_JNI_DIR@/<Config>/<abi>/`, the layout `jniLibs` requires. Every build type draws assets from `@ZENITH_ASSETS_DIR@` (`assets.srcDirs`), so cooked content ships in `assets/` ([Z2-N-ARCH-856]).

### Backup and transfer rules

<!-- assumption: pending ONLINE -->
From Stage 4 the Gradle template carries two backup-rule resources and the manifest names them, so Android's cloud backup and device-to-device transfer copy only the user's saves and never ONLINE's device record (A3 cross-area rows SV15 (b), ON18 (d), and ON22, which moved ONLINE's files under `user:/saves/online/` because PLAT's rule R10 admits only `config`, `saves`, `cache` and `logs` as a `user:/` first component). The user mount is the app's internal storage (`SDL_GetAndroidInternalStoragePath()`, `Zenith2/Docs/Design/PLAT/Filesystem.md`), which is Android's `file` domain, so every rule path is the `user:/` path without its root. The template file list of `Zenith2/Docs/Architecture/CompositionRoot.md` → "The Gradle template" carries both files and its manifest snippet the three attributes (cross-doc item X371, applied by core-doc patch g).

`app/src/main/res/xml/zenith_data_extraction_rules.xml` (no placeholder; copied `COPYONLY`):

```xml
<?xml version="1.0" encoding="utf-8"?>
<data-extraction-rules>
	<cloud-backup>
		<include domain="file" path="saves/"/>
		<exclude domain="file" path="saves/resume.zsave"/>
		<exclude domain="file" path="saves/last_profile.zslp"/>
		<exclude domain="file" path="saves/online/device.zodv"/>
	</cloud-backup>
	<device-transfer>
		<include domain="file" path="saves/"/>
		<exclude domain="file" path="saves/resume.zsave"/>
		<exclude domain="file" path="saves/last_profile.zslp"/>
		<exclude domain="file" path="saves/online/device.zodv"/>
	</device-transfer>
</data-extraction-rules>
```

`app/src/main/res/xml/zenith_full_backup_content.xml` (the API 30 and earlier form; the template's `minSdk` is 34, so it is inert on every supported device and kept because SAVE's rule names it):

```xml
<?xml version="1.0" encoding="utf-8"?>
<full-backup-content>
	<include domain="file" path="saves/"/>
	<exclude domain="file" path="saves/resume.zsave"/>
	<exclude domain="file" path="saves/last_profile.zslp"/>
	<exclude domain="file" path="saves/online/device.zodv"/>
</full-backup-content>
```

`AndroidManifest.xml.in`'s `<application>` element gains `android:allowBackup="true" android:dataExtractionRules="@xml/zenith_data_extraction_rules" android:fullBackupContent="@xml/zenith_full_backup_content"`.

- Once a rule file names an `include`, Android backs up only what an `include` names, so `saves/` is the whole backed-up set and `config/`, `cache/` and `logs/` are outside every include. ONLINE's device record `saves/online/device.zodv` (the install secret and the device ID, `Zenith2/Docs/Design/ONLINE/Formats.md` → "Device record") is inside `saves/`, so it is excluded by name; an `exclude` wins over an `include` for the same file. ONLINE's sync manifests and offline store under `saves/online/` stay included, because they describe the saves they travel with (row ON22 (b)).
- Android's rule paths are literal files or directories with no wildcard, so the suffix exclusions SAVE and ONLINE ask for (`*.z2journal`, `*.z2tmp`, `*.corrupt` inside `saves/<profile>/`; `*.z2atomic` beneath `saves/online/`, row ON22 (b)) cannot be written as rules; those files are inside the included `saves/` and are backed up when present. A3 cross-area row M53 asks SAVE to rule on them (startup recovery after a restore, or a literal transient directory MOD then excludes); the files above change in that ruling's diff.
- `CMake_Composition_BackupRules` (§ "Fixture list") compares both generated files and the generated manifest's `<application>` attributes with goldens on `android-arm64`; the restore behaviour on a device is a `needs-human` record (SAVE's `Test_SaveDevice_S24_BackupRulesExcludeTransient` for `saves/`, ONLINE's device run for the device record).
<!-- /assumption -->

### The Apk target

```cmake
add_custom_target(<Name>_Apk
	COMMAND ${CMAKE_COMMAND} -E env
		"JAVA_HOME=${ZENITH_JDK_DIR}" "ANDROID_HOME=${ZENITH_ANDROID_SDK_DIR}" "GRADLE_USER_HOME=${ZENITH_GRADLE_HOME}"
		"${gen}/android/gradlew.bat" --offline --no-daemon -p "${gen}/android" assemble$<CONFIG>
	DEPENDS <Name> <Name>_Content
	WORKING_DIRECTORY "${gen}/android"
	VERBATIM)
set_target_properties(<Name>_Apk PROPERTIES EXCLUDE_FROM_ALL TRUE FOLDER Games)
```

- `ZENITH_JDK_DIR`, `ZENITH_ANDROID_SDK_DIR` and `ZENITH_GRADLE_HOME` are the dependency-cache paths `CMake/ZenithThirdParty.cmake` derives from the manifest's `jdk`, `androidSdk` and `gradle` pins ([Z2-N-ARCH-855]).
- `assemble$<CONFIG>` selects the build type of the configuration being built (`assembleRelease` for the `android-arm64` build preset).
- `--offline` makes Gradle use only the pre-populated `GRADLE_USER_HOME`; the wrapper distribution is verified against `@ZENITH_GRADLE_SHA256@` by `distributionSha256Sum` before use ([Z2-N-ARCH-862]).
- `--no-daemon` keeps a Gradle daemon from outliving the step and holding files the next lane's build or the step's executable sweep needs.
- `zenith_end_build` creates `zenith_apks` on Android presets, depending on every `<Name>_Apk`; the APK outputs are `${gen}/android/app/build/outputs/apk/<buildType>/app-<buildType>.apk`.

<!-- assumption: pending SHIP -->
`Setup-Zenith2.ps1` pre-populates `GRADLE_USER_HOME` with AGP and its transitive artefacts at the pinned versions, verified by a committed Gradle `verification-metadata.xml`, so `--offline` never needs the network and no unpinned artefact is ever downloaded. The SHIP design (`Zenith2/Docs/Design/SHIP.md`) owns the setup script and the verification metadata; `ApkCheck` (PROC) builds `zenith_apks` for the release and hwasan types.
<!-- /assumption -->

### Packaging hooks

PACK's packaging (`Zenith2/Docs/Design/PACK/Tooling.md` → "CMake targets", → "App Bundle build"; A3 cross-area row PK4) needs three things of MOD's game build. MOD provides the first two; the third is a call site PACK adds.

1. **Linker map.** `zenith_add_game` step 9 adds `target_link_options(<Name> PRIVATE $<$<CONFIG:Release,Shipping>:<map option>>)`. The option is `-Wl,-Map=${ZENITH_OUT}/Games/<Name>/<preset>/lib<Name>.map` on Android presets and `/MAP:${ZENITH_OUT}/Games/<Name>/<preset>/<Name>.map` on Windows presets, where `<preset>` is the same path segment the JNI staging path uses (§ "Android shared object specifics"). Debug and Hwasan links write no map. PACK's size report attributes the binary's bytes from it (`Zenith2/Docs/Design/PACK/Formats.md`, `native.byLibrary`).
2. **Gradle project arguments.** `_zenith_gradle_project(<gen> <lib> <applicationId> <displayName> <buildTypes> [keywords…])` parses keyword arguments after its five positional ones with `cmake_parse_arguments(PARSE_ARGV 5 ARG "" "" "ASSETS_DIR_DEBUG;ASSETS_DIR_RELEASE;ASSETS_DIR_SHIPPING;ASSETS_DIR_HWASAN;ASSET_PACKS;RUNTIME_DEPENDENCIES;JAVA_SOURCE_DIRS")`. An unknown keyword stops the configure with `zenith gradle: unknown argument '<word>'`. Each keyword fills a placeholder of `app/build.gradle.kts.in` or `settings.gradle.kts.in`:

| Keyword | Placeholder | Value when given | Value when absent |
|---|---|---|---|
| `ASSETS_DIR_<TYPE>` | `@ZENITH_ASSETS_DIR_<TYPE>@`, the build type's `assets.srcDirs` | the directory, `/`-separated | `@ZENITH_ASSETS_DIR@` |
| `ASSET_PACKS` | `@ZENITH_ASSET_PACK_INCLUDES@` (settings), `@ZENITH_ASSET_PACK_LIST@` (app) | one `include(":<pack>")` line per pack, in list order; `assetPacks += listOf(":<pack>", …)` | empty lines |
| `RUNTIME_DEPENDENCIES` | `@ZENITH_RUNTIME_DEPENDENCIES@` | one `implementation("<coordinate>")` line per entry, in list order | empty |
| `JAVA_SOURCE_DIRS` | `@ZENITH_JAVA_SOURCE_DIRS@` | `java.srcDirs("<dir>", …)`, in list order | empty |

   For each `ASSET_PACKS` entry the function also configures `Zenith2/Player/GradleTemplate/assetpack/build.gradle.kts.in` into `<gen>/<pack>/build.gradle.kts`, with `@ZENITH_ASSET_PACK_NAME@` set to the pack. That template file and its delivery-type placeholder are PACK's (row PK4); MOD's generator only copies it. `app/build.gradle.kts.in` always carries `androidResources { noCompress += "zpak" }`. With no keyword argument the generated project is byte-identical to the five-argument form's, and every `CMake_Gradle_*` fixture keeps its golden output.
3. **The package call.** The PACK ticket that creates `CMake/ZenithPackage.cmake` adds, in the same diff, `include(ZenithPackage)` in `CMake/ZenithAddGame.cmake` and the call `zenith_add_package(<Name>)` as the last action of step 13, after the Apk target exists. That call passes `ASSETS_DIR_RELEASE` and `ASSETS_DIR_SHIPPING` as the package's `sideload/assets/` to the game's Gradle project, and adds `<Name>_Package` to the Release and Shipping `<Name>_Apk` dependencies. This is the rule of `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Call sites PACK's tickets add", applied to CMake: no MOD ticket waits on a PACK implementation.

`CMake_Gradle_KeywordArguments` (script mode) generates a project with every keyword set and compares it with its golden tree. `CMake_Gradle_UnknownKeyword` expects the unknown-argument message. `CMake_Game_LinkerMapOption` checks the link options of a Release and a Debug configuration in a fixture's `targets.cmake`.

### Hosted executable APKs

Some executables that are not games must run inside the shared Player's activity on the S24, because a native binary started by `adb shell` never receives the activity's lifecycle callbacks or the app's storage: the Stage 1 benchmarks that the RHI rulings run on the device (cross-area row RB7) and, from Stage 3, test executables that need surface callbacks (row RB20) or a device run of their fixture-reading cases (row ScB18). Both are **hosted APKs**: on the `android-arm64` preset alone, a listed executable target's own sources are rebuilt as one `SHARED` library `<Target>_ApkLib`, packaged by the same Gradle template as a game (`Zenith2Activity`, an SDL activity; § "Gradle project generation"), with only the library name, the application ID and the build types changing.

| | Benchmark APKs (from Stage 1) | Test APKs (from Stage 3) |
|---|---|---|
| list in `CMake/ZenithAddGame.cmake` | `_ZENITH_BENCH_APK_TARGETS`: `Zenith_Bench_BindlessProbe`, `Zenith_Bench_Budgets_RHI`, and from Stage 4 `Zenith_Bench_AccessibilityBridge` (A3 cross-area row P20 (c); `org.zenith2.bench.accessibilitybridge`) | `_ZENITH_TEST_APK_TARGETS`: `ZenithFlux_Tests` (RB20), `ZenithDocument_Tests`, `ZenithScene_Tests` (SCENE's device run, ScB18) |
| function, aggregate | `zenith_add_bench_apks()`, `zenith_bench_apks` | `zenith_add_test_apks()`, `zenith_test_apks` |
| the activity's entry `SDL_main` | the benchmark's own `main`, aliased by the link options `-Wl,--defsym=SDL_main=main` and `-Wl,--export-dynamic-symbol=SDL_main` (every listed benchmark defines `main` in its own `Internal/` sources) | `ZenithTestSupport`'s `Tests/Support/Zenith_TestEntry_SDLMain.cpp`, extracted from the archive by `-Wl,--undefined=SDL_main`; it calls `Zenith_RunTestMain(iArgc, ppszArgv, TEST_OUTPUT_ROUTE_TAGGED_DIAGNOSTIC)` (`Zenith2/Docs/Design/CORE/Memory.md` → "Test main declarations") |
| SDL3 and the export check | `_zenith_link_activity_runtime(<Target>_ApkLib)`, as for a game (§ "Android shared object specifics"): `SDL3-static` whole-archive through `LINK_LIBRARY_OVERRIDE_SDL3-static`, so the activity's JNI entry points (`JNI_OnLoad` and the `nativeSetupJNI` family) are in the library, and the post-link check of `SDL_main` and `JNI_OnLoad` | the same call; the override also covers the `SDL3-static` the test target reaches through its own `ZenithPlatform` dependency |
| device files | none: the input files the arguments name are pushed beside the report (below) | the committed fixture trees, the built shader containers and the test's generated inputs, pushed to the APK's external files directory by `CMake/ZenithPushTestApkFiles.cmake` (§ "Test APK device files") |
| `@ZENITH_APPLICATION_ID@` | `org.zenith2.bench.` + the target name lowered to ASCII lower case with the `zenith_bench_` prefix removed (`org.zenith2.bench.bindlessprobe`) | `org.zenith2.test.` + the target name lowered (`org.zenith2.test.zenithflux_tests`) |
| `@ZENITH_BUILD_TYPES@` | `release` | `debug,release,hwasan` (the `hwasan` type exactly as a game's: the `Hwasan` configuration's library, `wrap.sh`, extracted native libraries; Z2-D-011 HWASan on the S24) |
| output of record | the report file the arguments name, written under the app's external files directory `/sdcard/Android/data/<applicationId>/files/` and read back with `adb pull` | doctest's lines on logcat tag `Zenith2Test` |
| Gradle project | `${ZENITH_OUT}/BenchApks/<Target>/android-arm64/android/` | `${ZENITH_OUT}/TestApks/<Target>/android-arm64/android/` |

The shared machinery is three internal functions of `CMake/ZenithAddGame.cmake`, created by the benchmark-APK ticket and reused by the test-APK ticket: `_zenith_gradle_project(<gen> <lib> <applicationId> <displayName> <buildTypes>)` (the template copy, the SDL3 activity copy and the `configure_file` steps factored out of `_zenith_game_gradle`, which calls it, so a game's Gradle output is byte-identical), `_zenith_target_property(<out> <target> <property>)` (`get_target_property` in project mode; the script-mode runner reads a fixture's `targets.cmake`), and `_zenith_hosted_apk(<Target> <gen> <applicationId> <buildTypes> <linkOptions…>)`, which creates `<Target>_ApkLib` from the target's `SOURCES` (relative paths made absolute against its `SOURCE_DIR`), `INCLUDE_DIRECTORIES`, `COMPILE_DEFINITIONS`, `COMPILE_OPTIONS` and `LINK_LIBRARIES`, adds `-Wl,-z,max-page-size=16384` and the entry's link options, stages `lib<Target>_ApkLib.so` into `${gen}/jni/$<CONFIG>/${ANDROID_ABI}/` post-build, and creates `<Target>_Apk` with the command block of § "The Apk target" (`DEPENDS <Target>_ApkLib` only, `EXCLUDE_FROM_ALL TRUE`). A listed target the preset lacks stops the configure with `zenith hosted apk: '<Target>' is not a target of this preset`. Every `<Target>_ApkLib` is `EXCLUDE_FROM_ALL`, no Player or game target depends on it, and it is a benchmark or test target in the sense of [Z2-N-ARCH-181] and [Z2-N-ARCH-200] (it may link a third-party target its owner names, as the benchmark or test it packages does). `zenith_apks` depends on neither aggregate, so `ApkCheck`'s game APKs are unchanged.

Every hosted library is checked after it links by the same `POST_BUILD` export check as a game's shared object (`CMake/ZenithCheckApkExports.cmake`, § "Android shared object specifics"), which `_zenith_hosted_apk` adds through `_zenith_link_activity_runtime` before the staging copy.

Launch and read-back (the device steps that use them are the RHI rulings'): `adb shell am start -W -n org.zenith2.bench.bindlessprobe/org.zenith2.player.Zenith2Activity --esa org.zenith2.args --full,--alternative,2,--report,/sdcard/Android/data/org.zenith2.bench.bindlessprobe/files/probe.json`, then `adb pull /sdcard/Android/data/org.zenith2.bench.bindlessprobe/files/probe.json`; input files are pushed to the same directory with `adb push` first (which also creates it). The benchmark's standard output is not captured on Android, so its report file is the only output of record. `release` APKs accept the arguments extra (only the `shipping` build type refuses it).

### Test APK device files

[Z2-N-ARCH-250] gives every test target on an Android preset `ZENITH_REPO_ROOT="/sdcard/Android/data/org.zenith2.test.<target name in ASCII lower case>/files/repo"` and `ZENITH_TEST_FIXTURE_ROOT` that directory followed by the library's repository-relative `Tests/Fixtures` path, and [Z2-N-ARCH-180] makes a device run push the committed fixture trees there first. A test APK's application ID is `org.zenith2.test.` + the same lowered target name, so that directory is exactly `<the APK's external files directory>/repo`: the hosted process reads it with no storage permission, through the same `Zenith_ReadRepoFile` a host run uses, and `adb push` and `adb shell` can write and read it. `_zenith_hosted_apk` copies the test target's definitions unchanged, so the test sources run unmodified; `zenith_add_test_apks()` checks the copy at configure time and stops with `zenith test apk: '<Test>' ZENITH_REPO_ROOT '<value>' is not the APK's device directory '/sdcard/Android/data/<applicationId>/files/repo'` when the copied value differs or is absent.

The two other directory definitions name the device too on Android presets (cross-document item X262, applied): `ZENITH_SHADERS_GENERATED_DIR="/sdcard/Android/data/<applicationId>/files/shaders"` ([Z2-N-ARCH-254], the built shader containers, which `ZenithFlux_Tests` reads because ZenithFlux depends on `ZenithShadersGenerated`) and `ZENITH_TEST_GENERATED_ROOT="/sdcard/Android/data/<applicationId>/files/generated/Tests"` ([Z2-N-ARCH-250], the generated test inputs), given by the helpers (SHIP's test-target change extends `_zenith_test_root_definitions`, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md`). A test APK library therefore copies every definition unchanged; `zenith_add_test_apks()` checks `ZENITH_SHADERS_GENERATED_DIR` against `<files>/shaders` and `ZENITH_TEST_GENERATED_ROOT` against `<files>/generated/Tests` when the entry is present, with the same stop message naming the definition, so a shader-reading or generated-input-reading case runs on the device the same way as a fixture-reading one.

`CMake/ZenithPushTestApkFiles.cmake` is the one push every device step that runs a test APK uses, after `adb install` (the external files directory belongs to the installed package and is removed by an uninstall): `cmake -DZENITH_ADB=<adb> -DZENITH_TEST_TARGET=<Test> -DZENITH_ROOT=<repository>/Zenith2 -DZENITH_OUT=<the preset's ZENITH_OUT> -P Zenith2/CMake/ZenithPushTestApkFiles.cmake`. With `D` = `/sdcard/Android/data/org.zenith2.test.<lowered target>/files`, it runs, in order and stopping at the first non-zero exit with `zenith test apk: adb <arguments> failed: <code>`:
1. `adb shell rm -rf D/repo D/shaders D/generated`, then `adb shell mkdir -p D/repo D/shaders D/generated`;
2. for every directory `Zenith2/**/Tests/Fixtures` of the repository outside `ZENITH_OUT`, in byte order of its repository-relative path `P`: `adb shell mkdir -p D/repo/<parent of P>`, then `adb push <repository>/P D/repo/<parent of P>/`, so the tree lands at `D/repo/P` with its repository-relative path kept;
3. `adb push <ZENITH_OUT>/generated/ZenithShadersGenerated/Blobs D/shaders/` and the same for `DevelopmentBlobs`, each only when the directory exists;
4. `adb push <ZENITH_OUT>/generated/<Library>/Tests D/generated/` for the test target's library when that directory exists, so it lands at `D/generated/Tests`.
`-DZENITH_DRY_RUN=ON` prints the command lines instead of running them, which its fixture compares. The script never pushes outside `D`.

## CMake fixture tests

### Fixture layout

<!-- elaboration -->
```text
Zenith2/CMake/Tests/
	ZenithCMakeTests.cmake              registers every fixture test (included by zenith_end_build in every preset with a test preset, after the conformance executables exist; [Z2-N-ARCH-252])
	RunGameJsonFixture.cmake            script-mode runner for GameJson fixtures
	RunCompositionFixture.cmake         script-mode runner for Composition fixtures
	RunGameTestFixture.cmake            script-mode runner for GameTest fixtures
	GameJson/
		declarations.cmake              the fixed module and backend declaration table for script mode
		<Case>/game.json                the input (absent for the missing-file case)
		<Case>/expect.txt               "OK", or one expected message line per line, in emission order
		<Case>/<Root>/…                 content-root directories and files the case needs
	Composition/
		<Case>/game.json
		<Case>/preset.txt               the preset to simulate: win, win-asan or android-arm64
		<Case>/expected.generated.cpp.txt
		<Case>/expected.composition.json
		<Case>/expected.report.txt              the configure report line the case prints
		<Case>/expected.AndroidManifest.xml.txt  Android-preset cases only: the substituted manifest
		<Case>/expected.build.gradle.kts.txt     Android-preset cases only: the substituted app build file
		<Case>/expected.zenith_data_extraction_rules.xml.txt, <Case>/expected.zenith_full_backup_content.xml.txt   the BackupRules case only: the copied rule files
	Validator/
		<Case>/Games/<Case>/game.json   an input only the C++ validator can judge
		<Case>/expect.txt               the expected exit code on the first line, then the expected messages
	GameTest/
		<Case>/<Stem>.gametest.json
		<Case>/expect.txt
```

Golden files carry a `.txt` suffix so no source scanner treats them as compiled code. `Zenith2/CMake/` holds no compiled-language source, so the `SourceLayout` and `CompileCoverage` steps have nothing to check there.

### Runner script

Each fixture test is `add_test(NAME CMake_<Suite>_<Case> COMMAND ${CMAKE_COMMAND} -DZENITH_FIXTURE_DIR=<dir> -DZENITH_ROOT=${ZENITH_ROOT} -P ${ZENITH_ROOT}/CMake/Tests/Run<Suite>Fixture.cmake)`, labelled `z2-cmake`, with a 60-second timeout. `RunGameJsonFixture.cmake`:

1. `include(${ZENITH_ROOT}/CMake/ZenithAddGame.cmake)` and `include(${ZENITH_ROOT}/CMake/Tests/GameJson/declarations.cmake)`.
2. Reads `game.json` from the fixture directory with the step-3 reader and runs `zenith_validate_game_json`, then module and backend resolution against the fixture declarations, with the preset given by the fixture (default `win`); in script mode `if(TARGET …)` is replaced by a membership test against the fixture declarations' `SUPPORTED` lists, through the one helper `_zenith_decl_present(<Lib> <preset> <out>)` that project mode implements with `if(TARGET)`.
3. Compares the collected message list with `expect.txt` line by line, with `<Name>` already substituted by the fixture's directory name; any difference prints both lists and ends with `message(FATAL_ERROR …)`, so CTest sees a non-zero exit.

`RunCompositionFixture.cmake` runs the same steps followed by the generator functions in script mode, writing into `${CMAKE_CURRENT_BINARY_DIR}`, and compares byte for byte with every golden file the case holds: the generated TU, `composition.json`, the configure report line, and for Android-preset cases the substituted `AndroidManifest.xml` and `app/build.gradle.kts` of the Gradle project it generates.

<!-- elaboration -->
`zenith_add_game` creates every target and custom command through two internal wrappers, `_zenith_game_target(<kind> <name> …)` and `_zenith_game_command(<output> …)`, which call `add_executable`, `add_library`, `add_custom_target` and `add_custom_command` in project mode. The script-mode runners replace both wrappers with recorders that append the call's arguments to a list, so a fixture can assert the targets, command lines and dependencies a game would get without a configure.

The `Validator/` cases are not script-mode tests: each is `add_test(NAME CMake_GameJson_<Case> COMMAND $<TARGET_FILE:ZenithNewGame> validate <Case> --root <fixture dir>)` in the `win` preset, passing when the exit code equals `expect.txt`'s first line and the standard-error lines equal its remaining lines exactly, in order (the output form of `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Output"; the `schemas:` trailer goes to standard output and is not compared). They prove that the build-time validator runs and rejects what the CMake reader cannot see.

### Fixture list

| Test | Asserts |
|---|---|
| `CMake_GameJson_Valid_Minimal` | the Minimal game's `game.json` validates with no message |
| `CMake_GameJson_Valid_Full` | a file using every key, including `scriptRecovery`, an object-form `backends.physics` and `x-` keys at three levels, validates |
| `CMake_GameJson_MissingFile` | the missing-file message |
| `CMake_GameJson_TooLarge` | `Validator/`: `ZenithNewGame validate` exits 3 with the size message for a 65,537-byte file |
| `CMake_GameJson_ByteOrderMark` | `Validator/`: `ZenithNewGame validate` exits 3 with the byte-order-mark message |
| `CMake_GameJson_NotJson` | a truncated object gives the not-JSON message |
| `CMake_GameJson_RootArray` | a top-level array gives the root message |
| `CMake_GameJson_SchemaVersion2` | the unsupported-schema-version message |
| `CMake_GameJson_MissingEveryKey` | `{}` gives one missing-key message per always-present top-level key, in table order, and none for their children (the missing-parent rule) |
| `CMake_GameJson_UnknownKeys` | an unknown top-level key and an unknown `backends` key each give the unknown-key message; an `x-` key gives none |
| `CMake_GameJson_DuplicateKey` | `Validator/`: `ZenithNewGame validate` exits 3 with the duplicate-key message for a repeated `displayName` |
| `CMake_GameJson_ValidateStepRefuses` | a script-mode case over `Valid_Minimal` with the target recorders of § "Runner script": `<Name>_Validate` carries exactly the documented command line and dependencies, and `<Name>_Content`, `<Name>_Types` and the executable depend on it |
| `CMake_GameJson_NameMismatch` | the name-mismatch message |
| `CMake_GameJson_DisplayNameControlChar` | a tab in `displayName` gives the wrong-value message |
| `CMake_GameJson_DisplayNameTooLong` | 65 bytes give the wrong-value message; 64 bytes pass |
| `CMake_GameJson_VersionForms` | `1.2`, `01.0.0` and `1.0.65536` each fail; `65535.0.0` passes |
| `CMake_GameJson_PlatformsInvalid` | an empty array, `"linux"` and a duplicate each give their message |
| `CMake_GameJson_UnknownModule` | `"Havok"` in `modules` gives the unknown-module message listing all thirteen names |
| `CMake_GameJson_ModuleDependencyMissing` | `["AI"]` gives the module-dependency message for `Physics`; `["Cinematics"]` for `Animation` |
| `CMake_GameJson_ModuleOrderIndependent` | `["UI","Physics","AI"]` and `["AI","UI","Physics"]` resolve to the same canonical list |
| `CMake_GameJson_OnlineBackendOrder` | with `Online` selected, the fixture table's `Steam` (Windows presets), `GooglePlay` (Android presets) and `Offline` rows resolve to `Steam;Offline` on `win` and `GooglePlay;Offline` on `android-arm64`; with a fixture override restricting `ZenithOnline_Steam` away from `win`, `win` resolves to `Offline` alone; a reversed order fails the comparison |
| `CMake_GameJson_EcsInvalid` | `"Entt"` (wrong case) gives the wrong-value message |
| `CMake_GameJson_PhysicsWithoutModule` | `backends.physics` without `Physics` gives its message |
| `CMake_GameJson_PhysicsMissing` | `Physics` without `backends.physics` gives the missing-key message |
| `CMake_GameJson_PhysicsObjectKeys` | an object whose keys differ from `platforms` gives its message |
| `CMake_GameJson_PhysicsInvalid` | `"Havok"` gives the wrong-value message for `backends.physics.android` |
| `CMake_GameJson_PhysXAndroidRefused` | with fixture declarations restricting `ZenithPhysics_PhysX` to `ZENITH_WINDOWS`, `{"android":"PhysX"}` on the `android-arm64` preset gives the PhysX message |
| `CMake_GameJson_BackendUnsupportedInPreset` | with fixture declarations restricting `ZenithECS_EnTT` to `ZENITH_WINDOWS`, `"ecs": "EnTT"` on `android-arm64` gives the generic unsupported-backend message for `backends.ecs` |
| `CMake_GameJson_ModuleUnsupportedInPreset` | a fixture module restricted to Windows, selected on `android-arm64`, gives the unsupported-module message |
| `CMake_GameJson_RootsInvalid` | a missing directory, `"Tests"`, `"../x"` and a duplicate each give their message |
| `CMake_GameJson_StartupSceneInvalid` | a path outside the roots, a missing file, `a/../b.scene.json` and a `.json` suffix each give their message |
| `CMake_GameJson_AndroidBlock` | `android` present without the platform, absent with it, and each of its three keys invalid, give their messages |
| `CMake_GameJson_ScriptRecoveryInvalid` | `"policy":"auto"`, `"delayTicks":0` and a missing `maxRecoveriesPerModule` give their messages |
| `CMake_GameJson_PlatformSkipped` | a Windows-only game run with preset `android-arm64` prints the skip line, resolves no module and creates no target, and is still recorded for `declared_deps.json` |
| `CMake_GameJson_UnknownTopLevelEntry` | a `Docs/` directory in the game gives the unknown-entry message |
| `CMake_GameDir_EditorDirectoryAdmitted` | from Stage 4: a game holding `.editor/` and `Workload/WTest.editorscript.json` validates with no message |
| `CMake_GameDir_PackageRecipeAdmitted` | from Stage 4: sub-case `1/`, a game holding a top-level file `package.recipe.json`, validates with no message; sub-case `2/`, a directory named `package.recipe.json`, gives the unknown-entry message |
| `CMake_GameJson_CollectsAllErrors` | a file with five independent errors gives all five lines in table order, then the summary line |
| `CMake_GameJson_NoRhiOrCompressionKeys` | `backends.rhi` and `backends.compression` each give the unknown-key message |
| `CMake_Composition_NoModules` | the Minimal golden TU and record on `win` |
| `CMake_Composition_AndroidRHI` | on `android-arm64` the RHI list is `Vulkan` alone and no Agility export is emitted; the substituted manifest and app build file equal their goldens |
| `CMake_Composition_BackupRules` | on `android-arm64` the generated `zenith_data_extraction_rules.xml` and `zenith_full_backup_content.xml` equal their hand-written goldens, and the generated manifest's `<application>` element carries `allowBackup`, `dataExtractionRules` and `fullBackupContent` with the values of § "Backup and transfer rules"; fails when `saves/online/device.zodv` is missing from either file's excludes |
| `CMake_Composition_ModuleOrderCanonical` | shuffled `modules` produce byte-identical output; beside it, the doctest `Test_Composition_ModuleOrderCanonical` runs over the table-only output of the fixtures `ModuleOrderShuffledA` and `ModuleOrderShuffledB` |
| `CMake_Composition_ModuleOrderShuffledA`, `CMake_Composition_ModuleOrderShuffledB` | the modules `UI, Cinematics, 2D, Gameplay, Animation` and `Animation, Gameplay, 2D, Cinematics, UI` generate identical tables in canonical order `2D, Animation, Cinematics, Gameplay, UI`, with `Cinematics` carrying the dependency `Animation` |
| `CMake_Composition_PhysicsThunk` | the current Architecture example's selection (Audio, Online with the Offline backend, Physics with Jolt, Flecs): one `StartOnline` and one `StartPhysics` thunk, in the order `Audio`, `Online`, `Physics`, because all three are ready at once and ties break by byte order |
| `CMake_Composition_PhysicsPerPlatform` | `{"windows":"PhysX","android":"Jolt"}` yields the PhysX factory on `win` and the Jolt factory on `android-arm64`, with the same `composition.json` from both |
| `CMake_Composition_OnlineThunk` | `Online` selected yields the `StartOnline` thunk with the Offline entry |
| `CMake_Composition_HashStable` | the composition hash equals the value pinned from the [Z2-N-ARCH-864] lines read little-endian, changes when any one of its inputs changes, is equal for `win` and `win-asan`, and does not change when only the preset, the Online set, the startup scene or `scriptRecovery` changes, which change the build identity instead |
| `CMake_Composition_UnchangedNotRewritten` | a second generation into the same directory leaves the file's modification time unchanged |
| `CMake_Declarations_MirrorMatches` | every `MODULE` and `BACKEND` declaration of the live `declared_deps.json` equals its row of the fixture table (name, module dependencies, `BACKEND_OF`, `BACKEND_NAME`, `FACTORY`, matrix); a live declaration absent from the fixture table fails; a fixture row with no live declaration is printed as `not yet landed` and does not fail, except under `-DZENITH_ACCEPTANCE_STAGE=<N>`, where a row whose first stage is at most N fails |
| `CMake_GameTest_Valid` | a valid `.gametest.json` produces the expected `add_test` command line |
| `CMake_GameTest_Invalid` | a bad stem, `ticks` 0, `timeoutSeconds` 601, an input script outside `Tests/`, an unknown key and a `requiresPackage` of `"yes"` each give their message |
| `CMake_GameTest_RequiresPackage` | a game with two gametests that set `requiresPackage` and one that does not records exactly one `Game_<Name>_PackageBuild` setup test, with the command line and properties of § "Game test registration" step 4. The two dependent gametests carry `CONFIGURATIONS Debug` and `FIXTURES_REQUIRED Game_<Name>_Package`; the third carries neither. A game with no such gametest records no setup test |

<!-- elaboration -->
`CMake_Declarations_MirrorMatches` is registered as a normal CTest test that reads `${ZENITH_OUT}/declared_deps.json` after the configure, so it runs in the Test step like every other fixture test.
