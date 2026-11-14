# MOD requirement catalogue — Engine modules & projects

The requirements of the MOD area (Engine modules & projects): the in-repo game model, `zenith_add_game` and `game.json`, static optional modules with composition-root registration, and project creation without C++. The design that specifies them is `Zenith2/Docs/Design/MOD.md` with its split files `Zenith2/Docs/Design/MOD/GameBuild.md` and `Zenith2/Docs/Design/MOD/ProjectCreation.md`.

## Purpose

This catalogue breaks every MOD inventory item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" ("MOD Engine modules & projects") into testable requirements. Each requirement has exactly one owner, this area; a requirement that depends on another area names it in `Zenith2/Docs/Design/MOD.md` → "Dependencies". Stage is the stage whose acceptance the requirement serves; a ticket implementing it may be pulled forward into an earlier sprint (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"). The catalogue is authoritative for coverage (Z2-D-067); the design headings named under **Design** specify each requirement.

## Requirements

### Z2-R-MOD-001 Games live only under Zenith2/Games

- **Statement:** Every game is a directory `Zenith2/Games/<Name>/` in this repository, added to the one CMake tree by the `zenith_add_subdirectories(Games)` glob, and no game is configured from any other location.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the fixture `CMake_GameJson_Valid_Minimal` and step 1 of `zenith_add_game` refusing a directory name mismatch (`CMake_GameJson_NameMismatch`), run in the Test step (`win-debug`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Game directory layout"; Zenith2/Docs/Design/MOD/GameBuild.md → "Step by step"

### Z2-R-MOD-002 No compiled-language source in a game

- **Statement:** No `.cpp`, `.c`, `.h`, `.hpp` or `.inl` file exists anywhere under `Zenith2/Games/`, and a game executable's only compiled source is its generated composition TU under `${ZENITH_OUT}`.
- **Covers:** Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** the `SourceLayout` gate step's fixture (a `.cpp` dropped under a game fails) and `Test_NewGameScaffold_Plan_ContainsNoCompiledSource` (Test step)
- **Design:** Zenith2/Docs/Design/MOD.md → "Game directory layout"; Zenith2/Docs/Design/MOD.md → "Gate step interplay"; Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"

### Z2-R-MOD-003 Game directory entries are restricted

- **Statement:** `zenith_add_game` fails the configure on any top-level entry of `Games/<Name>/` other than `game.json`, `CMakeLists.txt`, the `content.roots` directories, `Tests`, `Workload`, `Acceptance` and `.migration`, with the exact unknown-entry message, and refuses a top-level `*.z2journal` or `game.json.z2tmp` with the interrupted-migration message.
- **Covers:** Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_UnknownTopLevelEntry` and `Test_NewGameScaffold_Migrate_RecoversPendingJournal` (the refusal before recovery), Test step
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Game directory entries"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-004 A game CMakeLists.txt is one zenith_add_game call

- **Statement:** Every `Games/<Name>/CMakeLists.txt` consists of exactly the single call `zenith_add_game(<Name>)`.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the `SourceLayout` gate step's one-helper-call check ([Z2-N-ARCH-157]) over every game, and `Test_NewGameScaffold_Plan_BlankTemplate_Files` asserting the written file
- **Design:** Zenith2/Docs/Design/MOD.md → "Game directory layout"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "Files every template writes"

### Z2-R-MOD-005 Every game builds on every change

- **Statement:** Every game under `Games/` is configured and built in every preset that its `platforms` allow within `ZENITH_GPU`, so a change that breaks any game fails the Build step.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-001
- **Budgets:** Z2-B-PROC-008, Z2-B-PROC-009, Z2-B-PROC-010
- **Acceptance evidence:** the Build step for `win-debug`, `win-release` and `android-arm64` building `zenith_players`, whose dependencies are every game executable ([Z2-N-ARCH-191]), recorded in the gate report
- **Design:** Zenith2/Docs/Design/MOD.md → "Supported matrices"; Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"

### Z2-R-MOD-006 Games/Minimal builds on Windows and Android

- **Statement:** `Zenith2/Games/Minimal` exists with the `game.json` of "Repository games" and builds as `Minimal.exe` in `win-debug` and `win-release` and as `libMinimal.so` in `android-arm64`.
- **Covers:** Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-081
- **Budgets:** Z2-B-PROC-008, Z2-B-PROC-009, Z2-B-PROC-010
- **Acceptance evidence:** the Build step outputs for the three presets, and `CMake_GameJson_Valid_Minimal`; collected by the Stage 2 acceptance run
- **Design:** Zenith2/Docs/Design/MOD.md → "Minimal"; Zenith2/Docs/Design/MOD.md → "Stage 2"

### Z2-R-MOD-007 Games/Minimal boots headless

- **Statement:** `Games/Minimal` starts headless with its data-only startup scene (no behaviour), registers its data-defined `MinimalCounter` type from `schemas.zreg`, loads the scene, runs one frame through the frame loop and exits with code 0.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-081, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** the CTest test `Game_Minimal_Boot` from `Games/Minimal/Tests/Boot.gametest.json` (Test step, `win-debug` and `win-release`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Minimal"; Zenith2/Docs/Design/MOD.md → "Game boot tests"; Zenith2/Docs/Design/MOD.md → "The gametest format"

### Z2-R-MOD-008 Games/Minimal runs on the S24

- **Statement:** The release APK of `Games/Minimal` installs and runs on the Galaxy S24 Ultra for 300 ticks and exits normally.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** a device run record (install, launch with `--exit-after-ticks 300`, exit code and log) committed by the Stage 2 acceptance run, a `needs-human` device step
- **Design:** Zenith2/Docs/Design/MOD.md → "Minimal"; Zenith2/Docs/Design/MOD.md → "Which gate step runs each"; Zenith2/Docs/Design/MOD/GameBuild.md → "Android Gradle wiring"

### Z2-R-MOD-009 Workload games select the tabled modules and backends

- **Statement:** By the Stage 3 acceptance, the `game.json` of each focused workload game (`WorkloadIndoor`, `WorkloadOutdoor`, `WorkloadCharacters`, `WorkloadDense`, `WorkloadUI2D`) selects exactly the platforms, the backends and the Stage 3 modules of the table "Workload games" (only modules first available in Stage 2 or 3, `Net` included for `WorkloadCharacters`), and each builds and passes its `Game_<Name>_Boot` test.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-024, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** the Build step and `Game_<Name>_Boot` for each workload game (Test step), plus the configure report lines matching the table
- **Design:** Zenith2/Docs/Design/MOD.md → "Workload games"; Zenith2/Docs/Design/MOD.md → "Canonical module order"

### Z2-R-MOD-010 Flagship slices select the tabled modules and backends

- **Statement:** The `game.json` of `OpenWorldSlice`, `CinematicSlice` and `Game2D` selects exactly the platforms, modules and backends of the table "Flagship slices" once the listed modules exist, and each builds and passes its `Game_<Name>_Boot` test.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-021, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** the Build step and `Game_<Name>_Boot` for each slice (Test step), and the configure report lines matching the table
- **Design:** Zenith2/Docs/Design/MOD.md → "Flagship slices"

### Z2-R-MOD-011 The game model is specified in Stage 1

- **Statement:** A Stage 1 specification ticket confirms this design family against the Stage 1 rulings, rules on the reconciliations R1 to R6 with the Architecture owner, and appends its rulings to `Zenith2/Docs/DecisionLog.md` in its own diff.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-024, Z2-D-081, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the specification ticket's DecisionLog entries and the edited design sections, checked by the Stage 1 acceptance run (coverage gate step in acceptance mode `-Stage 1`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Reconciliations with the core documents"; Zenith2/Docs/Design/MOD.md → "Stage 1"

### Z2-R-MOD-012 zenith_add_game checks the game name

- **Statement:** `zenith_add_game(<Name>)` fails the configure with the name-rule message when `<Name>` differs from the directory name, fails `^[A-Z][A-Za-z0-9]{0,31}$`, is a Windows device name, starts with `Zenith`, or names an existing declared library, tool or game.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_NameMismatch` and the name-rule cases of the GameJson fixtures (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Step by step"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-013 game.json is read through the CMake JSON reader

- **Statement:** `zenith_add_game` reads `game.json` with `file(READ)` and `string(JSON … ERROR_VARIABLE …)`, rejects a missing file, invalid JSON and a non-object root with their exact messages, and adds the file to `CMAKE_CONFIGURE_DEPENDS`; it relies on no duplicate-key, size or byte-order-mark behaviour of CMake's reader.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_MissingFile`, `_NotJson`, `_RootArray` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Reading game.json through the CMake JSON reader"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-014 Validation reports every error in one configure

- **Statement:** `zenith_add_game` collects every validation error of a game, emits each with `SEND_ERROR` in key order, then fails with the summary line, and creates no target of that game.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_CollectsAllErrors` and `CMake_GameJson_MissingEveryKey` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Step by step"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-015 Validation messages use the fixed formats

- **Statement:** Every configure-time message of `zenith_add_game` is exactly one line of the table "Validation messages", and no other text is emitted for a validation failure.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** every `CMake_GameJson_*` fixture, whose `expect.txt` holds the exact lines (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-016 A game not built for the OS is skipped

- **Statement:** When the preset's target OS is not in `platforms`, `zenith_add_game` prints `zenith_add_game(<Name>): not built for <os> (platforms in game.json)`, creates no target and still records the game.
- **Covers:** Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_PlatformSkipped` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Step by step"

### Z2-R-MOD-017 Modules resolve to KIND MODULE libraries

- **Statement:** Each `modules` entry resolves to the `KIND MODULE` library whose `MODULE_NAME` equals it; an unknown name fails with the unknown-module message listing the thirteen module names.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_UnknownModule` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Module resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"

### Z2-R-MOD-018 Module dependencies are never added implicitly

- **Statement:** A selected module whose module dependency is not also selected fails the configure with `Games/<Name>/game.json: module '<A>' requires module '<B>'; add "<B>" to modules`, and `zenith_add_game` never adds a module on its own.
- **Covers:** Z2-I-MOD-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_ModuleDependencyMissing` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Module resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-019 Unsupported modules and backends fail per preset

- **Statement:** A selected module or backend that is not a target in the current preset fails the configure naming the preset and the library's `ZENITH_SUPPORTED` matrix.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_ModuleUnsupportedInPreset` and `CMake_GameJson_BackendUnsupportedInPreset` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Module resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"

### Z2-R-MOD-020 Modules register in canonical order

- **Statement:** Modules appear in the composition in canonical order: a topological order of their module dependencies with ties broken by byte order of `MODULE_NAME`, independent of the order `game.json` lists them.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_ModuleOrderCanonical`, `CMake_Composition_ModuleOrderShuffledA`/`_ShuffledB` and `CMake_GameJson_ModuleOrderIndependent` (Test step), and the doctest `Test_Composition_ModuleOrderCanonical` over the generated test tables (`ZenithRuntime_Tests`), with the canonical-order check of startup composition validation re-verifying every game's table in each `Game_<Name>_Boot`
- **Design:** Zenith2/Docs/Design/MOD.md → "Canonical module order"; Zenith2/Docs/Design/MOD/GameBuild.md → "Module resolution"; Zenith2/Docs/Design/MOD.md → "Generated test compositions"; Zenith2/Docs/Design/MOD.md → "Composition validation at startup"

### Z2-R-MOD-021 The ECS backend is selected at link time

- **Statement:** `backends.ecs` selects one of `Custom`, `Flecs` and `EnTT`; the game links exactly that ECS backend library and the composition carries its factory and name.
- **Covers:** Z2-I-MOD-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_EcsInvalid`, `CMake_Composition_NoModules` (Test step) and the Libgraph Shipping link closure of `Games/Minimal` containing exactly one ECS backend
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Backend to target mapping"

### Z2-R-MOD-022 The physics backend is selected per platform

- **Statement:** `backends.physics` is present if and only if `Physics` is selected, is `Jolt` or `PhysX` or an object keyed by exactly the `platforms`, and the game links exactly the backend named for the preset's platform.
- **Covers:** Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-003, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_PhysicsWithoutModule`, `_PhysicsMissing`, `_PhysicsObjectKeys`, `_PhysicsInvalid` and `CMake_Composition_PhysicsPerPlatform` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"

### Z2-R-MOD-023 PhysX for Android follows the capability matrix

- **Statement:** When the `ZenithPhysics_PhysX` matrix excludes the Android presets, a `game.json` naming `PhysX` for `android` fails with `PhysX is not an Android physics backend (capability matrix); use Jolt`, and the validator reads the matrix rather than hard-coding a platform.
- **Covers:** Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-003, D-SEL-PHYSX-ANDROID (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_PhysXAndroidRefused` with fixture declarations restricting PhysX to Windows (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-024 Both compression backends are always linked

- **Statement:** Every game links both `ZenithCompression_Zlib` and `ZenithCompression_Zstd` and carries both codecs in its composition, and `game.json` has no compression key.
- **Covers:** Z2-I-MOD-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_NoRhiOrCompressionKeys`, `CMake_Composition_NoModules` (Test step) and `Test_Composition_Validate_RejectsMissingCodec`
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Backend to target mapping"

### Z2-R-MOD-025 RHI backends follow the platform

- **Statement:** A Windows game links `ZenithRHI_Vulkan` and `ZenithRHI_D3D12` and chooses one at run time through `rhi.backend`, RHI's per-platform enum key whose resolved value maps onto an `m_axRHIBackends` entry name (a value outside the enum is a configuration warning and the platform default runs, [Z2-N-ARCH-929]); an Android game links `ZenithRHI_Vulkan` alone; `game.json` has no RHI key.
- **Covers:** Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_AndroidRHI`, `CMake_GameJson_NoRhiOrCompressionKeys` (Test step), `Test_Composition_Validate_RejectsEmptyRHIList` and `Test_Runtime_RhiBackend_InvalidValueFallsBackToDefault`
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Backend to target mapping"

### Z2-R-MOD-026 Online backends are all linked and chosen by configuration

- **Statement:** A game selecting `Online` links every Online backend library supported in the preset, and its `StartOnline` thunk passes all of them to `Zenith_Module_Online_Start` as the backend list.
- **Covers:** Z2-I-MOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_OnlineThunk` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"; Zenith2/Docs/Design/MOD/GameBuild.md → "Thunk rules"

### Z2-R-MOD-027 Generated files are per preset and rewritten only on change

- **Statement:** The composition TU is written to `${ZENITH_OUT}/Games/<Name>/<preset>/`, `composition.json` to `${ZENITH_OUT}/Games/<Name>/`, and every generated text file is rewritten only when its bytes change.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_UnchangedNotRewritten` and `CMake_Composition_PhysicsPerPlatform` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Generated file writing"

### Z2-R-MOD-028 The composition TU holds only constant data and thunks

- **Statement:** The generated composition TU contains only includes of the selected libraries' root headers, `constexpr` or `const` data, one-line `return` thunks binding modules to backends and the `Zenith_GetGameComposition` definition; no mutable variable, no `main` and no other logic.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** the golden files of every `CMake_Composition_*` fixture (Test step) and `Test_Composition_NoMutableGlobals` (the `ForbiddenCheck` writable-data scan over each game's composition object)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Generated composition TU template"; Zenith2/Docs/Design/MOD/GameBuild.md → "Thunk rules"; Zenith2/Docs/Design/MOD/GameBuild.md → "Include rules"

### Z2-R-MOD-029 Factory symbols come from backend declarations

- **Statement:** The generator writes every factory symbol from the resolved backend's `FACTORY` declaration and every backend header as `<Lib>/Zenith_<Family>_<BACKEND_NAME>.h`, never spelling a factory name itself.
- **Covers:** Z2-I-MOD-04, Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_PhysicsThunk` and `CMake_Composition_NoModules` with fixture declarations carrying distinct factory names (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backend resolution"; Zenith2/Docs/Design/MOD/GameBuild.md → "Backend to target mapping"

### Z2-R-MOD-030 The composition hash follows ARCH-864

- **Statement:** The composition hash is exactly [Z2-N-ARCH-864]: the first 16 hexadecimal digits of the SHA-256 of the LF-joined lines (game name, version, modules in canonical order, ECS backend, physics backend for the platform or `none`, RHI backends, codecs), read as a little-endian `uint64` for `m_ulCompositionHash`, and printed everywhere (TU header comment, configure report, `composition.json`, startup dump) in one spelling: the digest's first 16 hexadecimal digits as `string(SHA256)` prints them.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_HashStable` and `CMake_Composition_NoModules` against the values of "Hash golden" (Test step)
- **Design:** Zenith2/Docs/Design/MOD.md → "Composition hash"; Zenith2/Docs/Design/MOD.md → "Hash golden"; Zenith2/Docs/Design/MOD/GameBuild.md → "Template text"

### Z2-R-MOD-031 composition.json records the resolved composition

- **Statement:** `zenith_add_game` writes `composition.json` with the keys, order and formatting of "The composition.json record", byte-identical from every preset, deriving each other platform's composition hash and Online backend list from the declaration store with that platform's representative preset (`win`, `android-arm64`), and recording the per-platform Online backend names under `online`.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-03, Z2-I-MOD-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the `expected.composition.json` golden of every `CMake_Composition_*` fixture and `CMake_Composition_PhysicsPerPlatform` (Test step)
- **Design:** Zenith2/Docs/Design/MOD.md → "The composition.json record"; Zenith2/Docs/Design/MOD/GameBuild.md → "Build records"

### Z2-R-MOD-032 The Windows game is an executable whose one source is generated

- **Statement:** On Windows presets a game is `add_executable(<Name>)` whose only source is the generated TU, linking `ZenithPlayer`, `ZenithRuntime`, its modules and its backends, with the Shipping subsystem flags.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the File API codemodel of `Games/Minimal` checked by `CompileCoverage` (one compile group, the generated TU) and the Build step
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"

### Z2-R-MOD-033 The Android game is one shared object exporting SDL_main

- **Statement:** On Android presets a game is `lib<Name>.so`, links `ZenithPlayer` whole-archive, and exports `SDL_main` as a defined default-visibility function; no other shared object is produced.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-010
- **Budgets:** Z2-B-PROC-023
- **Acceptance evidence:** `Test_Composition_AndroidExportsSdlMain`, realised as the `ApkCheck` check of the same name over every game shared object
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Android shared object specifics"; Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"

### Z2-R-MOD-034 Windows games carry the D3D12 Agility exports

- **Statement:** On Windows presets the generated TU defines the two `const` D3D12 Agility SDK exports and a post-build step stages the Agility runtime next to the executable.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_NoModules` golden (the exports) and `Test_Runtime_HeadlessMatchesWindowedSimulation` running a D3D12 WARP device from the staged runtime (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Windows executable specifics"; Zenith2/Docs/Design/MOD/GameBuild.md → "Include rules"

### Z2-R-MOD-035 Content and type targets use the fixed cook commands

- **Statement:** `<Name>_Content` and `<Name>_Types` run exactly the cook command lines of [Z2-N-ARCH-207] with `DEPFILE` outputs, depend on the cook executable and `composition.json`, and the game executable depends on both.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** Z2-B-PROC-008
- **Acceptance evidence:** a Build-step incremental check: a second `win-debug` build with no change runs no cook command (the Build report's Ninja log), and `Game_Minimal_Boot` loading cooked content
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Content and type targets"

### Z2-R-MOD-036 Game tests are registered from gametest files

- **Statement:** Each `Games/<Name>/Tests/<Stem>.gametest.json` is validated and registered as the CTest test `Game_<Name>_<Stem>` in the `win` and `win-asan` presets with the exact command line of "Game test registration", passing on exit code 0.
- **Covers:** Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameTest_Valid`, `CMake_GameTest_Invalid` and every `Game_<Name>_Boot` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Game test registration"; Zenith2/Docs/Design/MOD.md → "The gametest format"

### Z2-R-MOD-037 Aggregate targets build every game

- **Statement:** `zenith_end_build` creates `zenith_players`, depending on every game executable, and on Android presets `zenith_apks`, depending on every `<Name>_Apk`.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the Build step for `win-shipping`/`android-arm64-shipping` (target `zenith_players`) and `ApkCheck` building `zenith_apks`
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "The Apk target"; Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"

### Z2-R-MOD-038 Games are recorded in declared_deps.json

- **Statement:** Every game, including one skipped for the preset's OS, is recorded in the `games` array of `declared_deps.json` in the shape of Zenith2/Docs/Architecture/BuildSystem.md, so `CompileCoverage` and libgraph see each game's matrix.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the `games` array of `declared_deps.json` read by the `CompileCoverage` step for each game's matrix, and the `expected.composition.json` goldens of the `CMake_Composition_*` fixtures (Test step, `CompileCoverage` step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Build records"; Zenith2/Docs/Design/MOD.md → "Gate step interplay"

### Z2-R-MOD-039 The configure report names every game resolution

- **Statement:** Every configure prints one `zenith_add_game(<Name>): …` line per game and preset in the format of "Configure report".
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the `expected.report.txt` golden of every `CMake_Composition_*` fixture, compared by `RunCompositionFixture.cmake` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Configure report"; Zenith2/Docs/Design/MOD.md → "Configure report"

### Z2-R-MOD-040 CMake fixture suite covers the validator

- **Statement:** The CMake fixture tests of the table "Fixture list" exist under `Zenith2/CMake/Tests/`, are registered by `zenith_end_build` in the `win` preset, and each asserts its listed case.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-03, Z2-I-MOD-04, Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** the `CMake_*` CTest tests (Test step, `win-debug`)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "CMake fixture tests"; Zenith2/Docs/Design/MOD/GameBuild.md → "Fixture list"

### Z2-R-MOD-041 Gradle project is generated per game and preset

- **Statement:** On Android presets `zenith_add_game` generates the Gradle project from `Player/GradleTemplate/` into `${ZENITH_OUT}/Games/<Name>/<preset>/android/` with every placeholder of "Placeholders" substituted and SDL3's activity sources copied from the verified cache.
- **Covers:** Z2-I-MOD-05, Z2-I-MOD-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-010, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_AndroidRHI` comparing the substituted `AndroidManifest.xml` and `app/build.gradle.kts` with their goldens (Test step), and the D-SLANG arm64 probe's APK build of `Games/Minimal` (`android-arm64`)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Gradle project generation"; Zenith2/Docs/Design/MOD/GameBuild.md → "Placeholders"

### Z2-R-MOD-042 JNI staging and build types match configurations

- **Statement:** The game shared object is staged to `jni/<Config>/<abi>/lib<Name>.so`, and the Gradle build types `debug`, `release`, `shipping` and `hwasan` take their native library from the matching configuration.
- **Covers:** Z2-I-MOD-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `ApkCheck` inspecting the release and hwasan APKs of `Games/Minimal` for `lib/<abi>/libMinimal.so` from the matching configuration
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Build types and JNI staging"

### Z2-R-MOD-043 The Apk target runs pinned Gradle offline

- **Statement:** `<Name>_Apk` runs the generated Gradle wrapper with the pinned JDK, Android SDK and Gradle home, `--offline` and `--no-daemon`, after the wrapper distribution is verified against its pinned SHA-256.
- **Covers:** Z2-I-MOD-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-008
- **Budgets:** Z2-B-PROC-023
- **Acceptance evidence:** an `ApkCheck` run on a machine whose Gradle cache holds only the pinned artefacts, recording no network access, and a wrapper-checksum mismatch fixture failing the target
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "The Apk target"

### Z2-R-MOD-044 Cooked content ships in the APK assets

- **Statement:** Every Gradle build type packages `${ZENITH_OUT}/Cooked/<Name>/android/` as the APK's `assets/`.
- **Covers:** Z2-I-MOD-05, Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-027, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `ApkCheck` listing `assets/` of the release APK of `Games/Minimal` against its cooked tree, and the S24 run record of `Games/Minimal`
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Build types and JNI staging"

### Z2-R-MOD-045 schemaVersion 1 is the only accepted version

- **Statement:** `game.json` must carry `schemaVersion` 1; any other value fails with the message naming `ZenithNewGame --migrate`.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_SchemaVersion2` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-046 Identity keys are validated

- **Statement:** `name`, `displayName` and `version` are validated by the rules of the table "Keys": `name` equals the directory, `displayName` is 1–64 UTF-8 bytes without control characters, `version` is `MAJOR.MINOR.PATCH` with each part at most 65535 and no leading zero.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_NameMismatch`, `_DisplayNameControlChar`, `_DisplayNameTooLong`, `_VersionForms` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"

### Z2-R-MOD-047 platforms selects windows and android

- **Statement:** `platforms` is a non-empty array of unique values, each `windows` or `android`.
- **Covers:** Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_PlatformsInvalid` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"

### Z2-R-MOD-048 modules is a unique list of module names

- **Statement:** `modules` is an array, possibly empty, of unique registered `MODULE_NAME`s.
- **Covers:** Z2-I-MOD-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_UnknownModule` and a duplicate-module case in `CMake_GameJson_CollectsAllErrors` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"

### Z2-R-MOD-049 content roots and startup scene are validated

- **Statement:** `content.roots` names existing top-level directories other than `Tests`, `Workload`, `Acceptance` and `.migration`, and `content.startupScene` is a safe relative path under one of them ending `.scene.json` that exists.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_RootsInvalid`, `CMake_GameJson_StartupSceneInvalid` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"; Zenith2/Docs/Design/MOD/GameBuild.md → "Validation messages"

### Z2-R-MOD-050 The android block exists exactly for Android games

- **Statement:** The `android` object is present if and only if `android` is in `platforms`, with a valid `applicationId`, a `versionCode` in 1–2100000000 and an `orientation` of the four allowed values.
- **Covers:** Z2-I-MOD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_AndroidBlock` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"

### Z2-R-MOD-051 scriptRecovery is validated and carried to the runtime

- **Statement:** An optional `scriptRecovery` object with `policy` (`manual` or `automatic`), `delayTicks` (1–3600) and `maxRecoveriesPerModule` (0–100) is validated, written into the composition's settings, and passed to the script runtime at startup step 14; when absent the configuration-dependent default applies.
- **Covers:** Z2-I-MOD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_ScriptRecoveryInvalid`, `CMake_GameJson_Valid_Full` (Test step) and a runtime test asserting the script runtime receives the declared policy (`Test_Runtime_ScriptRecoverySettings_Passed`, `ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"; Zenith2/Docs/Design/MOD.md → "Zenith_GameComposition.h"

### Z2-R-MOD-052 Unknown keys fail and x- keys are ignored

- **Statement:** Any key at any object level that is neither in the table "Keys" nor prefixed `x-` fails the configure with the unknown-key message; `x-` keys are ignored by CMake and preserved by every tool that rewrites the file.
- **Covers:** Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `CMake_GameJson_UnknownKeys` (Test step) and `Test_NewGameScaffold_Migrate_PreservesXKeys`
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Keys"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "game.json migration"

### Z2-R-MOD-053 A module is a static library with two entry points

- **Statement:** Every optional module is a `KIND MODULE` static library with a `MODULE_NAME`, depending on `ZenithRuntime`, whose root header `Zenith_Module_<MODULE_NAME>.h` declares exactly `Zenith_Module_<MODULE_NAME>_RegisterTypes` and `Zenith_Module_<MODULE_NAME>_Start` in the `Start` form of its row; a module registers its configuration keys from `RegisterTypes`, and no third entry point exists.
- **Covers:** Z2-I-MOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `ApiContractCheck` and `HeaderSelfContained` over each module root header, and the first game selecting each module passing `Game_<Name>_Boot`, which links both entry points through its composition
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"; Zenith2/Docs/Design/MOD.md → "Module root headers"

### Z2-R-MOD-054 The module table is the contract for module areas

- **Statement:** The thirteen modules of "Module to target mapping" exist with the listed library, header, module dependencies, backend family, `Start` form and matrix from their first stage, and a change to a row edits the table in the same diff.
- **Covers:** Z2-I-MOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** `CMake_Declarations_MirrorMatches`, a subset check: every live module and backend declaration equals its fixture row, fixture rows not yet landed are reported without failing (Test step, green from Stage 2), and under `-DZENITH_ACCEPTANCE_STAGE=<N>` (the Stage N acceptance run) a row not landed by its first stage fails
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"

### Z2-R-MOD-055 Module dependencies are declared and checked

- **Statement:** A module that uses another module declares it in `DEPENDS`, and the `Libgraph` step fails an undeclared module-to-module edge.
- **Covers:** Z2-I-MOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step's used-but-undeclared rule over the module libraries
- **Design:** Zenith2/Docs/Design/MOD.md → "Gate step interplay"; Zenith2/Docs/Design/MOD/GameBuild.md → "Module resolution"

### Z2-R-MOD-056 An unselected module is absent from the game

- **Statement:** A module that a game does not select is not linked into its executable, registers no type, system or binding, and is absent from the game's `.d.luau`.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` Shipping link closure of `Games/Minimal` containing no module library, and the cook's `luau-types` output for Minimal containing no module type (Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Target creation"; Zenith2/Docs/Design/MOD/GameBuild.md → "Content and type targets"

### Z2-R-MOD-057 No code is loaded at run time

- **Statement:** No runtime target calls `LoadLibrary*` or `dlopen` outside `ZenithPlatform/Internal/` and `ZenithRHI_*/Internal/`, and no plugin directory or script-native extension exists.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** the `ForbiddenCheck` gate step's dynamic-loading rule ([Z2-N-ARCH-274]) with its failing fixture
- **Design:** Zenith2/Docs/Design/MOD.md → "Exclusions"; Zenith2/Docs/Design/MOD.md → "Gate step interplay"

### Z2-R-MOD-058 Backends reach modules only through Start

- **Statement:** A module with a backend family receives its backend factory or factory list only as the `Start` argument the generated composition passes, and never selects or names a backend itself.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_StartThunk_PassesSelectedBackend` (`ZenithRuntime_Tests`) and the `Libgraph` swap-contract report showing no module includes a backend header
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Thunk rules"; Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"

### Z2-R-MOD-059 Composition types live in ZenithRuntime

- **Statement:** `Zenith_GameComposition` and its entry types are declared in `ZenithRuntime/Zenith_GameComposition.h`; `Player/Zenith_GameComposition.h` declares only `Zenith_GetGameComposition()`, so no library below the Player includes a Player header.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` step (no `ZenithRuntime` edge to `ZenithPlayer`) and `HeaderSelfContained` over both headers
- **Design:** Zenith2/Docs/Design/MOD.md → "Zenith_GameComposition.h"; Zenith2/Docs/Design/MOD.md → "Player headers"; Zenith2/Docs/Design/MOD.md → "Reconciliations with the core documents"

### Z2-R-MOD-060 The composition is validated at startup

- **Statement:** At the start of step 6, before any registration, the runtime validates the composition by every check of "Composition validation at startup" and stops with exit code 3 and the check's message on the first defect.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Composition_Validate_RejectsEmptyRHIList`, `_RejectsMissingCodec`, `_RejectsDuplicateModuleName`, `_RejectsNullEntryPoint` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Composition validation at startup"

### Z2-R-MOD-061 An interface-version mismatch stops startup

- **Statement:** A module descriptor whose `m_uInterfaceVersion` differs from `uZENITH_MODULE_INTERFACE_VERSION` stops startup with exit code 3 and `module '<name>' interface version <v> does not match runtime <r>`.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Composition_InterfaceVersionMismatch_Stops` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Composition validation at startup"

### Z2-R-MOD-062 Registration follows the fixed order

- **Statement:** At the start of step 6, before `Zenith_Config::Create`, the runtime registers engine configuration keys in layer order, its own keys and the engine's types, then calls each module's `RegisterTypes` in canonical order (a module registers its configuration keys there, having no third entry point), then freezes the configuration schema; step 10 constructs the process `Zenith_StringTable` (CORE's interner, sized from the required keys `strings.maxStrings` and `strings.maxTextBytes`), binds the type registry to it, registers the game's `schemas.zreg` and freezes the registry, before any world, asset load or script exists; MOD implements no interner of its own.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_RegisterTypesInCanonicalOrder`, `Test_Runtime_ModuleConfigKey_ResolvesFromGameLayer`, `Test_Runtime_EngineKeyRegistrationFailure_Exit2` and `Test_Runtime_RegistryFrozenBeforeWorld` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Configuration-key registration"; Zenith2/Docs/Design/MOD.md → "Module lifecycle state machine"; Zenith2/Docs/Design/MOD.md → "Startup sequence with unwinding"

### Z2-R-MOD-063 A RegisterTypes failure unwinds the registry

- **Statement:** A module's `RegisterTypes` failure fails step 6 with exit code 3, starts no module, and destroys the configuration schema and the type registry with every key and type registered so far.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_RegisterTypesFailure_UnwindsRegistry` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Module lifecycle state machine"

### Z2-R-MOD-064 A Start failure destroys started modules in reverse

- **Statement:** When module i fails `Start`, the runtime undoes module i's start journal, destroys modules i−1 … 0 in reverse with their journals undone, starts no module after it, and exits with the mapped code.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_StartFailure_DestroysStartedInReverse` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Module lifecycle state machine"; Zenith2/Docs/Design/MOD.md → "Step failure to exit code mapping"

### Z2-R-MOD-065 A rejected registration fails the call that made it

- **Statement:** A registration rejected by a start or attach context under "Registration rejection rules" makes the enclosing `Start` or `AttachWorld` fail with `MODULE_ERROR_REGISTRATION` whatever it returned, and every rejection of that call is logged.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_RejectedRegistration_FailsStart`, `Test_ModuleHost_CommandKindName_MustCarryModulePrefix`, `Test_ModuleHost_JournalFull_RejectsRegistration` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Registration rejection rules"

### Z2-R-MOD-066 Modules attach to every world in canonical order

- **Statement:** Every started module is attached to each new world in canonical order; when module i fails `AttachWorld`, its attach journal is undone, modules i−1 … 0 are detached in reverse, the world is destroyed, and no module receives `DetachWorld` for a world it never joined.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_AttachFailure_DetachesInReverse`, `Test_ModuleHost_DetachOnlyAttachedWorlds` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Module lifecycle state machine"

### Z2-R-MOD-067 Registrations are undone in reverse

- **Statement:** Every registration a module makes through a context is journaled and undone in reverse when the module is destroyed or detached, and a module never unregisters another module's registration.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleRegistrationsUndone` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Registration journal and undo"

### Z2-R-MOD-068 Shutdown detaches and destroys modules in reverse

- **Statement:** Shutdown destroys worlds in descending tag order, each detaching modules in reverse canonical order, then destroys module instances in reverse canonical order, then the remaining subsystems in the order of "Shutdown sequence": the frame executor before the job system, the owners of the platform's I/O threads (`Zenith_AsyncIO`) before the RHI device, and the SDL platform object after the RHI device and the asset registry.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ModuleHost_Shutdown_DetachesAndDestroysInReverse` and `Test_Runtime_StartupOrder`'s destruction half (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Shutdown sequence"

### Z2-R-MOD-069 No registration through static initialisation

- **Statement:** No module, backend or game registers anything through static initialisation, and no MOD runtime file or generated composition object holds a writable data symbol.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_Composition_NoMutableGlobals` and the `ForbiddenCheck` writable-data scan (F16) over `ZenithRuntime` and `ZenithPlayer`
- **Design:** Zenith2/Docs/Design/MOD.md → "Gate step interplay"; Zenith2/Docs/Design/MOD/GameBuild.md → "Include rules"

### Z2-R-MOD-070 Startup runs the steps in order and unwinds in reverse

- **Statement:** The runtime runs startup steps 4–18 in the order of Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence", allocates during startup only through `Zenith_TryNew`, and on the failure of step s destroys exactly the constructed steps s−1 … 4 (and s when it failed after construction) in reverse, returning a `Zenith_RuntimeError` naming s and the mapped exit code; a failure injected by a test hook takes the exit code of the step's first mapping row.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartupOrder`, `Test_Runtime_StartupFailureUnwinds`, `Test_Runtime_AfterStepFailure_UnwindsTheStepToo` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup sequence with unwinding"; Zenith2/Docs/Design/MOD.md → "Zenith_RuntimeTestAccess.h"

### Z2-R-MOD-071 Every startup step has a failure-injection test

- **Statement:** Each of the fifteen construction steps of `Zenith_Runtime::Create` has a test `Test_Runtime_Create_FailsAt<Step>_Unwinds` that fails that step alone through the fault-injecting heap and asserts the error, zero leaks, no running thread and no leftover registration.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** the fifteen `Test_Runtime_Create_FailsAt<Step>_Unwinds` tests (`ZenithRuntime_Tests`, Test step and `Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Runtime and composition tests"; Zenith2/Docs/Design/MOD.md → "Startup sequence with unwinding"

### Z2-R-MOD-072 Startup failures map to the exit-code table

- **Statement:** Every startup failure exits with the code of "Step failure to exit code mapping", and every exit of the Player is one of the codes of `Zenith_ExitCode`.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ExitCodeMapping_MatchesTable` (`ZenithRuntime_Tests`) and `Test_PlayerMain_ReturnsRuntimeExitCode` (`ZenithPlayer_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Step failure to exit code mapping"; Zenith2/Docs/Design/MOD.md → "Zenith_ExitCode.h"

### Z2-R-MOD-073 Headless is the same executable without a device

- **Statement:** The headless configuration skips steps 12 and 13, starts device-needing modules in their headless mode, creates no window, RHI device, renderer or audio device, and runs every simulation system unchanged.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_HeadlessSkipsDevice` (`ZenithRuntime_Tests`) and every `Game_<Name>_Boot` running headless
- **Design:** Zenith2/Docs/Design/MOD.md → "Device loss and headless"; Zenith2/Docs/Design/MOD.md → "Frame-graph nodes registered by ZenithRuntime"

### Z2-R-MOD-074 Headless simulation matches windowed simulation

- **Statement:** The same replay run headless and windowed on WARP yields identical tick hashes.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_HeadlessMatchesWindowedSimulation` (`ZenithRuntime_Tests`, WARP)
- **Design:** Zenith2/Docs/Design/MOD.md → "Runtime and composition tests"

### Z2-R-MOD-075 The command line accepts exactly the option table

- **Statement:** `Zenith_CommandLine::Parse(iArgc, ppszArgv)` accepts exactly the options of Architecture's table under the parsing rules of "Zenith_CommandLine.h", constructs one `Zenith_CommandLineFileGrant` per host-path option given (handed out by `GetFileGrant`), and returns for every other input a `Zenith_CommandLineError` whose `m_uArgIndex` names the offending argument, which the Player prints before exiting with code 2 without creating any runtime object.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Player_ArgumentParsing`, `Test_PlayerMain_UnknownOption_CreatesNoRuntime`, `Test_CommandLine_RepeatedSet_KeepsOrder`, `Test_CommandLine_AssetsRootRefusedUnderShippingRules` (Player and runtime tests)
- **Design:** Zenith2/Docs/Design/MOD.md → "Zenith_CommandLine.h"; Zenith2/Docs/Design/MOD.md → "Player tests"

### Z2-R-MOD-076 The Player installs the root heap and hooks first

- **Statement:** `Zenith_PlayerMain` installs the root heap, creates the log router (CORE's step 1b, the one object between startup steps 1 and 2; an allocation failure returns 4 without installing hooks), fills and installs the profiler hooks, and only then parses the command line; the runtime starts the router's drain thread at step 7 after adding its sinks; after `Zenith_RuntimeMain` returns the Player destroys the router, then uninstalls the hooks and the heap; it is the only installer.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_PlayerMain_ReturnsRuntimeExitCode` asserting the test heap sees the install as its first event, and the `ForbiddenCheck` writable-data scan
- **Design:** Zenith2/Docs/Design/MOD.md → "Player headers"

### Z2-R-MOD-077 The startup report records every step

- **Statement:** The Player times steps 1–3 into its `Zenith_StartupReport` and passes it through `Zenith_RuntimeMain` to `Zenith_Runtime::Create`, which fills each executed step's duration, the last completed step and, on failure, the failing step, module index and error, the steps unwound and the exit code.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartupReport_RecordsEveryStep` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup report"; Zenith2/Docs/Design/MOD.md → "Zenith_Runtime.h"

### Z2-R-MOD-078 The composition is dumped at startup

- **Statement:** At the end of step 7 the runtime logs the composition under `runtime.composition` in the line format of "Startup composition dump", including the build identity and the configuration provenance for the RHI backend and the startup scene; the runtime logs only in the five categories of "Log categories".
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_Composition_Dump_ListsEveryEntry` and `Test_Runtime_LogCategories_Declared` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup composition dump"; Zenith2/Docs/Design/MOD.md → "Log categories"

### Z2-R-MOD-079 Startup emits profiler zones and the timeline

- **Statement:** Each startup step, module entry-point call and shutdown phase emits its named profiler zone, the runtime publishes the `runtime.*` counters, and `Zenith_RuntimeMain` logs the startup timeline after step 18 or after a failure's unwind.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** a `ZenithRuntime_Tests` case capturing the profiler hooks during a headless start (`Test_Runtime_StartupZones_Emitted`) and asserting one zone per step and module call
- **Design:** Zenith2/Docs/Design/MOD.md → "Profiler zones and counters"; Zenith2/Docs/Design/MOD.md → "Startup timeline"

### Z2-R-MOD-080 The loose content root defaults beside the executable

- **Statement:** In non-Shipping Windows builds without `--assets-root`, the runtime mounts `<executable directory>/../../../Cooked/<game>/windows/` as the loose mount.
- **Covers:** Z2-I-MOD-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_LooseRootDefault_RelativeToExecutable` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup scene and loose content root"

### Z2-R-MOD-081 The startup scene is a configuration default

- **Statement:** The runtime registers `content.startupScene` with the composition's startup scene as its engine default, so `--set content.startupScene=…` selects another scene without rebuilding.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_Composition_StartupScene_IsConfigDefault` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup scene and loose content root"

### Z2-R-MOD-082 Exit after ticks runs exactly the requested ticks

- **Statement:** `--exit-after-ticks <n>` requests shutdown at the tick boundary after the n-th tick of the primary world, summed across world swaps, so exactly n ticks run and the exit code is 0 unless an automated assertion was recorded.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ExitAfterTicks_RunsExactlyN` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Frame loop driver"

### Z2-R-MOD-083 Startup fits the cold-boot rows

- **Statement:** The startup sequence of every workload and flagship game, from process entry to the first frame, completes within its share of the cold-boot rows on the reference desktop and the S24, as reported by the startup timeline.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-013, Z2-B-SHIP-014
- **Acceptance evidence:** the W-Load measurement record under `Zenith2/Docs/Research/` with the startup timelines of each game, from the Stage 3 mid-sprint integration check (`needs-human`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Startup timeline"; Zenith2/Docs/Design/MOD.md → "Budget rows"

### Z2-R-MOD-084 ZenithNewGame create takes the fixed options and defaults

- **Statement:** `ZenithNewGame create <Name>` accepts exactly the options of the synopsis with the defaults and rules of the option table.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Options_Defaults`, `Test_NewGameScaffold_Options_Conflicts`, `Test_NewGame_Cli_ExitCodes` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Synopsis"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "ZenithNewGame command line"

### Z2-R-MOD-085 Invalid names are refused with exit code 2

- **Statement:** `ZenithNewGame create` refuses a name failing `^[A-Z][A-Za-z0-9]{0,31}$`, a Windows device name, a `Zenith`-prefixed name, an engine library, backend, module, tool or existing game name, and an existing directory, with exit code 2 and the exact message, writing nothing.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Name_RejectsInvalid`, `_RejectsReserved`, `_RejectsEngineName`, `Test_NewGameScaffold_Apply_ExistingDirectory_Refused`, `Test_NewGame_Cli_ExitCodes` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Output"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "Scaffold algorithm"

### Z2-R-MOD-086 Creation writes the template files and no C++

- **Statement:** A created game holds exactly the files of "Files every template writes" for its template, selects no module whichever template it uses, holds no compiled-language source, and nothing is written outside `Games/<Name>/`.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Plan_BlankTemplate_Files`, `_3DTemplate_Files`, `_2DTemplate_Files`, `Test_NewGameScaffold_Plan_ContainsNoCompiledSource`, `Test_NewGameScaffold_Apply_WritesOnlyInsideGameDirectory` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Templates"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "Files every template writes"

### Z2-R-MOD-087 Creation is atomic

- **Statement:** Creation writes the `.newgame-incomplete` marker first and `CMakeLists.txt` last, and any failure removes the game directory, so a failed or interrupted creation is never seen by a build and is refused by the next `create`.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Apply_FailureMidWrite_LeavesNothing`, `_WritesCMakeListsLast`, `_IncompleteDirectory_Refused` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Scaffold algorithm"

### Z2-R-MOD-088 A created game configures, builds and boots

- **Statement:** A game created from each of the three templates configures, builds and boots its data-only startup scene headless on the first run from Stage 2, running one frame and exiting 0, because no template selects a module or attaches a behaviour at Stage 2.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-081
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** `Test_NewGame_CreatedGameBoots` with subcases `[Blank]`, `[3D]`, `[2D]` (`Zenith_Integration_NewGame`, Test step, `win`)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Project creation tests"

### Z2-R-MOD-089 Creation output is byte-stable

- **Statement:** The same creation options always produce byte-identical files, written in the canonical JSON form for `game.json`.
- **Covers:** Z2-I-MOD-08, Z2-I-MOD-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Plan_IsByteStable` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Canonical JSON"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "The written game.json"

### Z2-R-MOD-090 validate checks a game without configuring

- **Statement:** `ZenithNewGame validate <Name>` runs the C++ `game.json` validator, the game-directory entry check and the schema validator over `Schemas/`, and prints every error in its fixed format with exit code 3.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGame_Cli_ExitCodes` (the invalid-game case) and `Test_GameJsonValidator_MatchesCMakeFixtures` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Validation in C++"

### Z2-R-MOD-091 The C++ and CMake validators agree

- **Statement:** For every GameJson fixture whose expected messages name no preset, the C++ validator produces exactly the fixture's expected lines, which the CMake validator also produces.
- **Covers:** Z2-I-MOD-08, Z2-I-MOD-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_GameJsonValidator_MatchesCMakeFixtures` and the `CMake_GameJson_*` fixtures over the same `expect.txt` files (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Validation in C++"; Zenith2/Docs/Design/MOD/GameBuild.md → "Fixture list"

### Z2-R-MOD-092 game.json migrates as an authoring document

- **Statement:** `ZenithNewGame --migrate` migrates `game.json` through the authoring-document framework of Zenith2/Docs/Contracts.md → "Compatibility policy per kind": one registered step per version, the dry-run preview, the backup under `Games/<Name>/.migration/backups/<UTC timestamp>/`, the journaled atomic write and its recovery, unknown-field preservation with warnings, fixtures under `Tests/Fixtures/Migration/GameJson/`, and read-only refusal of a newer version.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGameScaffold_Migrate_CurrentIsNoOp`, `_PreviewWritesNothing`, `_BacksUpBeforeWrite`, `_RecoversPendingJournal`, `_IdentityFixture`, `_PreservesXKeys`, `_NewerVersion_Refused` (Test step), with the framework's `Test_JournaledWrite_InterruptedAtEachStep_Recovers`
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "game.json migration"

### Z2-R-MOD-093 ZenithNewGame exit codes are fixed

- **Statement:** `ZenithNewGame` exits 0 on success, 1 on an I/O failure with nothing left behind, 2 on a usage or name error or an existing directory, and 3 on validation errors.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewGame_Cli_ExitCodes` (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Exit codes"

### Z2-R-MOD-094 The Zenith2 root is found or given

- **Statement:** `ZenithNewGame` uses `--root` when given and otherwise the nearest ancestor of the current directory holding `CMakePresets.json`, `Engine/` and `Games/`, or such a directory's `Zenith2/` child, and exits 2 when none exists.
- **Covers:** Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** a `Test_NewGame_Cli_ExitCodes` case run from a directory outside any tree, and the scaffold tests passing `--root` to a scratch tree (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Synopsis"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "Scaffold algorithm"

### Z2-R-MOD-095 The wizard creates games through the scaffold library

- **Statement:** The editor's new-project wizard creates a game only through `Zenith_PlanNewGame` and `Zenith_ApplyNewGamePlan`, adding no default, rule or file of its own, so its result is byte-identical to `ZenithNewGame create` with the same options.
- **Covers:** Z2-I-MOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** `Test_NewProjectWizard_MatchesCli` (`Tools/ZenithEditor/Tests/`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Editor wizard boundary"

### Z2-R-MOD-096 The wizard shows the plan and the validator messages

- **Statement:** The wizard shows the planned file list and every validation message from `Zenith_PlanNewGame` as the author edits the options, and shows the apply report's messages verbatim.
- **Covers:** Z2-I-MOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** the EDIT usability walkthrough for project creation recorded by the Stage 4 acceptance run, plus `Test_NewProjectWizard_MatchesCli` asserting the displayed messages for an invalid module choice
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Editor wizard boundary"

### Z2-R-MOD-097 The wizard tells the author how the new game builds

- **Statement:** After a successful creation the wizard states that the next build configures the new game and offers to open it once built, without running a configure of its own.
- **Covers:** Z2-I-MOD-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** the Stage 4 EDIT walkthrough record for project creation
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "Editor wizard boundary"

### Z2-R-MOD-098 The Stage 0 scaffold lands the presets and a Player stub

- **Statement:** The Stage 0 scaffold ticket lands `Zenith2/CMakeLists.txt`, `Zenith2/CMakePresets.json` with every preset including `android-arm64` and `android-x86_64`, the shared CMake helpers, a first engine library with one test, and the Player stub with both platform entry points.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-081, Z2-D-128
- **Budgets:** Z2-B-PROC-007, Z2-B-PROC-008
- **Acceptance evidence:** the scaffold ticket's `tick_gates.json` with every base step green and `treeShaStart == treeShaEnd`, collected by the Stage 0 acceptance ticket
- **Design:** Zenith2/Docs/Design/MOD.md → "Stage 0 scaffold"

### Z2-R-MOD-099 The Stage 0 stub game builds on Windows and Android

- **Statement:** The Stage 0 `zenith_add_game` subset builds `Games/Minimal` as `Minimal.exe` in `win-debug` and `win-release` and as `libMinimal.so` exporting `SDL_main` in `android-arm64`.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-05
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-010
- **Budgets:** Z2-B-PROC-010
- **Acceptance evidence:** the scaffold's Build outputs and an `llvm-readelf --dyn-syms` record in its gate report, collected by the Stage 0 acceptance ticket
- **Design:** Zenith2/Docs/Design/MOD.md → "Stage 0 scaffold"; Zenith2/Docs/Design/MOD/GameBuild.md → "Stage 0 subset"

### Z2-R-MOD-100 Dynamic plugins are excluded

- **Statement:** Zenith2 has no dynamic plugin: no module, backend or game is loaded from a shared library at run time (rationale: a second link model, a binary-compatibility surface and an unload problem, for games that all live in one tree).
- **Covers:** Z2-I-MOD-10
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** none (excluded); the rule "No code is loaded at run time" keeps it excluded
- **Design:** Zenith2/Docs/Design/MOD.md → "Exclusions"

### Z2-R-MOD-101 An installed SDK is excluded

- **Statement:** Zenith2 ships no installed SDK: no install target or exported CMake package exists for engine libraries (rationale: games outside the tree escape the gates and create a binary-compatibility surface).
- **Covers:** Z2-I-MOD-11
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** none (excluded)
- **Design:** Zenith2/Docs/Design/MOD.md → "Exclusions"

### Z2-R-MOD-102 Out-of-repo projects are excluded

- **Statement:** No game outside `Zenith2/Games/` is supported (rationale: every game must be covered by the same gates, libgraph and complexity checks).
- **Covers:** Z2-I-MOD-12
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-002
- **Budgets:** none
- **Acceptance evidence:** none (excluded); "Games live only under Zenith2/Games" keeps it excluded
- **Design:** Zenith2/Docs/Design/MOD.md → "Exclusions"

### Z2-R-MOD-103 Third-party modding is excluded

- **Statement:** Zenith2 has no third-party mod loading (rationale: it needs run-time code loading, a stable script API across releases and an untrusted-content boundary, each contradicting settled choices).
- **Covers:** Z2-I-MOD-13
- **Stage:** S1
- **Classification:** excluded
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** none (excluded)
- **Design:** Zenith2/Docs/Design/MOD.md → "Exclusions"

### Z2-R-MOD-104 The build runs the C++ validator

- **Statement:** Every game has a `<Name>_Validate` step that runs `ZenithNewGame validate` before its content and types are built and before its executable links, so duplicate keys, a file over 65,536 bytes and a byte-order mark, which only the C++ validator checks, fail the build with the validator's exact messages.
- **Covers:** Z2-I-MOD-02, Z2-I-MOD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-002
- **Budgets:** Z2-B-PROC-008
- **Acceptance evidence:** `CMake_GameJson_DuplicateKey`, `CMake_GameJson_TooLarge`, `CMake_GameJson_ByteOrderMark` (the `Validator/` cases, which run the executable and compare the exit code and the unprefixed standard-error lines) and `CMake_GameJson_ValidateStepRefuses` (Test step), landing with the `ZenithNewGame` ticket
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Checks only the C++ validator makes"; Zenith2/Docs/Design/MOD/GameBuild.md → "Content and type targets"

### Z2-R-MOD-105 Workload games gain their Stage 4 modules

- **Statement:** By the Stage 4 acceptance, `WorkloadOutdoor` selects `Environment`, `Gameplay` and `Streaming` and `WorkloadCharacters` selects `Animation`, `Gameplay`, `Net`, `Physics` and `AI`, the full selections of the table "Workload games", and both still build and pass `Game_<Name>_Boot`.
- **Covers:** Z2-I-MOD-03, Z2-I-MOD-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** the Build step and `Game_WorkloadOutdoor_Boot`, `Game_WorkloadCharacters_Boot` (Test step), and their configure report lines matching the table
- **Design:** Zenith2/Docs/Design/MOD.md → "Workload games"

### Z2-R-MOD-106 The build identity carries the per-preset inputs

- **Statement:** Each composition carries `m_ulBuildIdentity`, computed from the composition hash, the configure preset, the Online backend set, `uZENITH_MODULE_INTERFACE_VERSION`, the startup scene and the script-recovery settings as "Build identity" states, from LF-joined input lines with no trailing LF or space, the composition hash in its printed spelling, and the literal lines `none` and `default` for an absent Online set and `scriptRecovery`, shown in the TU header, the configure report and the startup dump, while the composition hash stays exactly [Z2-N-ARCH-864].
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_NoModules` against "Hash golden", `CMake_Composition_HashStable` (the build identity changes where the composition hash does not) and `Test_Composition_Dump_ListsEveryEntry`
- **Design:** Zenith2/Docs/Design/MOD.md → "Build identity"; Zenith2/Docs/Design/MOD/GameBuild.md → "Template text"

### Z2-R-MOD-107 Runtime error records are string-free

- **Statement:** `Zenith_ModuleError`, `Zenith_RuntimeError` and `Zenith_CommandLineError` are trivially copyable records of at most 4 bytes whose first member `m_eCode` is their `…ErrorCode` enumeration, carrying only fixed-size context (step, module index, argument index), never a string or pointer; textual context travels in `Zenith_StartupReport` and the log.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the `static_assert(Zenith_ErrorType<E>)` inside `Zenith_Result` compiling for each record (Build step), `HeaderSelfContained` and `ApiContractCheck` over the three headers, and `Test_Runtime_StartupFailureUnwinds` asserting `m_eStep`
- **Design:** Zenith2/Docs/Design/MOD.md → "Error enumerations"; Zenith2/Docs/Design/MOD.md → "Reconciliations with the core documents"; Zenith2/Docs/Design/MOD.md → "Zenith_Runtime.h"

### Z2-R-MOD-108 Startup steps drive CORE, PLAT, JOBS and GRAPH as their designs specify

- **Statement:** Before step 4 the runtime fills the platform description's early keys through `ApplyValueText`; after step 4 it starts the platform watchdog for `--timeout-seconds`; at step 6 it loads the input bindings after `Zenith_Config::Create`; at step 7 it adds the log sinks, resizes the ring and starts the log drain thread; step 9 applies the floating-point environment on the main thread before creating the job system, fills the job-system description from the `jobs.*` keys and `runtime.headless`, and fails with exit code 2 on an invalid job or clock description; step 16 marks exactly one input world; step 18 hands the frame executor a schedule from GRAPH's schedule cache; step 19 carries input transitions after a tickless, unpaused frame, clears them after a ticking frame, and waits for events while the lifecycle throttles.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StepNine_AppliesJobConfiguration`, `Test_Runtime_StartupPlatformDuties` and `Test_Runtime_FrameLoop_ThrottleAndCarry` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Step construction notes"; Zenith2/Docs/Design/MOD.md → "Step failure to exit code mapping"

### Z2-R-MOD-109 The frame-loop driver follows the executor protocol

- **Statement:** In the prologue's recompile step the frame loop rebuilds and acquires a schedule from the schedule cache inside the zone `Frame.ScheduleRecompile` on a structural change, acquires with `Zenith_FrameCompileOptions::m_bForceMiss` set when `Zenith_FrameExecutor::ConsumeRecompileRequest()` reports a `jobs.recompile` request, and reprioritises when the executor's `IsReprioritiseDue()` holds; before `RunFrame(k)` it tells Flux the frame index k − 1 and hands `IsRenderWorldResyncPending()` to the render-world writer; it releases unused schedules after every `RunFrame`; and at shutdown it lets the current iteration complete, calls no further `RunFrame` and discards the in-flight render world ([Z2-N-ARCH-426]), then destroys the executor before the job system and the cache after the executor; and it supplies the executor's frame-dump write callback, which writes only to the user mount and never waits.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_FrameLoop_ExecutorProtocol` (`ZenithRuntime_Tests`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Frame loop driver"; Zenith2/Docs/Design/MOD.md → "Shutdown sequence"

### Z2-R-MOD-110 Games/Minimal gains its behaviour at Stage 3

- **Statement:** From Stage 3, `Games/Minimal` carries `Scripts/Main.luau` attached to its `Main` entity, writing `MinimalCounter.Ticks` each tick, and its `Tests/Scripted.gametest.json` boots headless, runs 300 ticks and exits 0.
- **Covers:** Z2-I-MOD-01, Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-016, Z2-D-020
- **Budgets:** none
- **Acceptance evidence:** the CTest test `Game_Minimal_Scripted` (Test step, `win-debug` and `win-release`)
- **Design:** Zenith2/Docs/Design/MOD.md → "Minimal"; Zenith2/Docs/Design/MOD.md → "Stage 3"

### Z2-R-MOD-111 Templates write and attach the Main behaviour from Stage 3

- **Statement:** From Stage 3 every template writes `Scripts/Main.luau` (the behaviour of "The written behaviour") in place of `Scripts/.keep` and attaches it to its `Main` or `Camera` entity, and a game created from each template still configures, builds and boots on its first run.
- **Covers:** Z2-I-MOD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-002, Z2-D-016
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** `Test_NewGameScaffold_Plan_BlankTemplate_Files`, `_3DTemplate_Files`, `_2DTemplate_Files` and `Test_NewGame_CreatedGameBoots` in the Stage 3 form (Test step)
- **Design:** Zenith2/Docs/Design/MOD/ProjectCreation.md → "The written behaviour"; Zenith2/Docs/Design/MOD/ProjectCreation.md → "Files every template writes"

### Z2-R-MOD-112 One module publication sink per world, bound at creation and retired with the table

- **Statement:** Every world has exactly one `Zenith_ModulePublicationSink`, created at world creation step 4 before any module attaches, which binds the world's `Zenith_AsyncResultTable*` once and never re-resolves `GetAsyncResults()`; the world host record owns it as a `Zenith_Owned` until GAME's world host takes it into the world's retired entry, and it is destroyed only after the table at the entry's release, so a park after the world's destruction reaches the retired table.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-040, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_BoundAtWorldCreation` (`ZenithRuntime_Tests`, Test step; also under `Sanitizers -Preset win-asan`)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Sink binding at world creation"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Sink lifetime and retirement"

### Z2-R-MOD-113 Sink sources and handlers are registrations with rejection rules

- **Statement:** `DeclareSource` and `SetSourceHandler` on the module publication sink are registrations of the attaching module, valid only during its `AttachWorld` and before the world's first tick, rejected with `MODULE_ERROR_REGISTRATION` and a logged reason for an out-of-range or already declared kind, an invalid descriptor, a world arena above `ulMODULE_PUBLICATION_MAX_ARENA_BYTES`, a handler for a kind the module does not own, a second handler, or a sink-declared kind left without a handler when `AttachWorld` returns; a handler registration is journaled and undone with the module.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_DeclarationRules` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Source declaration and handler registration"

### Z2-R-MOD-114 Sink allocation is first-fit at declared sites and refused under replay in the prologue

- **Statement:** `Zenith_ModulePublicationSink::AllocateCompletionID` allocates only for a sink-declared kind at a site the source declared (a merge function on the world's merge chain, or the main thread in frame-loop step 1 for a recorded-only source), returns `PUBLICATION_SINK_ERROR_FULL` at capacity and `PUBLICATION_SINK_ERROR_REPLAYING` for a prologue allocation under `--replay` with nothing changed, and no prologue allocation happens after the frame's `RunFrame` starts.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_AllocationSites` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Allocation sites and acceptance"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Replay behaviour of the sink"

### Z2-R-MOD-115 Parked results reach the table live or retired and are forwarded to module handlers

- **Statement:** `Zenith_ModulePublicationSink::Park` completes the ID in the world's table from any thread (a zero-length recorded payload included), drops a late local completion under `--replay` as a counted no-op, treats every other table error as an asserted and counted engine defect, and the sink's forwarder calls the kind's `Zenith_ModulePublicationHandler` at tick-boundary step 1 for each publication in completion-ID order, live and in replay, with `m_bScriptOwned` set for a script request's result; `PeekRecorded` returns a kind's recorded publications of a tick under `--replay`.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_ParkAndForward`, `Test_Runtime_ModuleSink_ReplayPeekAndLateLocal` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Parking a result"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "The module publication forwarder"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Replay behaviour of the sink"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Sink diagnostics"

### Z2-R-MOD-116 The scene sink forwards SCENE's resolver and drops late local completions

- **Statement:** `Zenith_RuntimeScenePublicationSink::SetPublicationResolver` installs SCENE's resolver on the table the sink bound at world creation, and the sink's `Park` maps `ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION` under a publication source to a no-op counted in `runtime.async.late_local_completions`, never an assert.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ScenePublicationSink_ForwardsResolver` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD.md → "SCENE and SCRIPT seams of the runtime"

### Z2-R-MOD-117 The world attach context answers the sink, command-kind, control, clock and purpose lookups

- **Statement:** `Zenith_WorldAttachContext` returns the world's module publication sink, the sealed command-kind table's ID for a name (0 when absent), the world's `Zenith_WorldControl`, the step-9 `Zenith_SimulationClock` and the world's `Zenith_WorldPurpose` set by its creator, and `Zenith_RuntimeTestAccess::CreatePurposedWorld` creates edit and preview worlds whose modules observe that purpose.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_AttachContext_Lookups` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "World attach context lookups"

### Z2-R-MOD-118 Test assertions are recorded from any thread and worlds can request shutdown

- **Statement:** `Zenith_Runtime::RecordTestAssertionFailure` may be called from any thread, including frame threads during `RunFrame`, and makes the run's normal exit code 5; a non-zero world shutdown request read after a `RunFrame` stops the run after the current iteration.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_TestAssertion_FromWorkerThread` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Test assertion failures and world shutdown requests"; Zenith2/Docs/Design/MOD.md → "Frame loop driver"

### Z2-R-MOD-119 World creation is factored for the world host and destruction walks GAME's teardown order

- **Statement:** `Zenith_WorldHost_CreateWorld` performs the per-world creation steps 1–8 for any world from a `Zenith_WorldCreateDesc` and unwinds on failure, the runtime state holds GAME's game state and per-world records, and `Zenith_WorldHost_DestroyWorld` walks `axWORLD_TEARDOWN_ORDER` calling `m_pfnOnTeardownStep` before each step.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_WorldHost_CreateAndTeardownWalk` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime state the world host shares"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "World creation and destruction"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Call sites GAME's tickets add"

### Z2-R-MOD-120 A swap's schedule is precompiled and the swap is observable through test hooks

- **Statement:** `Zenith_FrameLoop_PrecompileSwap` acquires the post-swap schedule from the schedule cache without installing it, changing nothing cached or running on failure, so prologue step 8's rebuild after a successful precompile is a cache hit; `m_pfnOnSwapStep` and `GetGameFlowState` expose the swap to tests.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_PrecompileSwap_TrialAcquire` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Swap precompile and test hooks"

### Z2-R-MOD-121 Modules reach the profiler overlay from their start context

- **Statement:** `Zenith_ModuleStartContext::GetProfilerOverlay` returns DEVT's on-screen overlay in development builds and null in Shipping and headless builds, and a provider a module registers through it is undone with the module.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartContext_ProfilerOverlay` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context overlay access"

### Z2-R-MOD-122 The start context implements frame listeners, engine epilogue stages, script consumers and script sources

- **Statement:** `Zenith_ModuleStartContext::RegisterFrameListener`, `AddEngineEpilogueStage`, `SetScriptSideEffectConsumer` and `DeclareScriptAsyncSource` are implemented as journaled registrations valid during `Start`: listeners receive `OnStopAudio` and `OnResumeAudio` inside `Platform.Pump` and `OnPlatformEvents` after prologue step 1 in canonical module order, each module's one engine stage compiles into every schedule after the named per-world stages, consumers and sources reach the process binding table, and every duplicate or invalid registration is rejected and undone.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartContext_FrameListenersForwarded`, `Test_Runtime_StartContext_EngineEpilogueStage`, `Test_Runtime_StartContext_ScriptConsumersAndSources` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context registrations"

### Z2-R-MOD-123 Modules' fault points and test mount sources are reachable in a booted runtime

- **Statement:** `Zenith_ModuleStartContext::GetFaultInjector` and `Zenith_WorldAttachContext::GetFaultInjector` return the runtime's fault injector (null outside a test-access runtime), and `Zenith_RuntimeTestAccess::AddTestMountSource` and `RemoveTestMountSource` mount and unmount a test-owned mount source in a booted runtime's virtual file system between frames, and `Zenith_RuntimeTestHooks::m_pfnOnShutdownStep` is called before each shutdown step.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_TestSeams_FaultInjectorAndMountSource` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime test seams for modules"

### Z2-R-MOD-124 A source kind has one declaration point and FULL is reported at allocation

- **Statement:** Each asynchronous source kind is declared exactly once, either on the world's module publication sink with `DeclareSource` in the owning module's `AttachWorld` or with `Zenith_ModuleStartContext::DeclareScriptAsyncSource` in its `Start`, and a second declaration in either form, with identical values or not, is a rejected registration; `Park` reports nothing, so a full source is reported only by `AllocateCompletionID` returning `PUBLICATION_SINK_ERROR_FULL` with nothing changed, and a producer's reserved slot is kept through its own outstanding count.
- **Covers:** Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_DeclarationRules`, `Test_Runtime_ModuleSink_AllocationSites` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Source declaration and handler registration"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Parking a result"

### Z2-R-MOD-125 A mixed kind's owner allocates its notifications in the prologue through the sink

- **Statement:** For a script source declared with `DeclareScriptAsyncSource` whose owning module registered a handler for it on the world's sink, `AllocateCompletionID` on the main thread in frame-loop step 1 is accepted (refused at any other site, refused under `--replay`, `FULL` at the kind's shared capacity), its `Park` accepts `PUBLICATION_KIND_GENERIC_RECORDED` alone, and every allocated ID is completed exactly once by the module's park.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ModuleSink_MixedKindPrologue` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Allocation sites and acceptance"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Parking a result"

### Z2-R-MOD-126 Modules learn a recording run and can request an exit after start

- **Statement:** `Zenith_ModuleStartContext::IsRecordingReplay` is true during `Start` exactly under `--record`, and `RequestExitAfterStart` makes a successful startup run no frame and exit with code 0 through the ordinary shutdown order, while a startup failure keeps its own exit code.
- **Covers:** Z2-I-MOD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartContext_RecordingAndExitAfterStart` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK"

### Z2-R-MOD-127 One module provides the package entitlement query from its start context

- **Statement:** `Zenith_ModuleStartContext::ProvideEntitlementQuery` accepts at most one query per process during `Start`, rejects a second with the first owner's name, journals the provision and undoes it when the module's start fails or the module is destroyed, and hands the query to the package bridge's forwarding entitlement query.
- **Covers:** Z2-I-MOD-06, Z2-I-MOD-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_StartContext_EntitlementProvider` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Start context members for ONLINE and PACK"; Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Call sites PACK's tickets add"

### Z2-R-MOD-128 Test hooks override the platform base path

- **Statement:** `Zenith_RuntimeTestHooks::m_strBasePathOverride`, when non-empty, becomes the booted runtime's `Zenith_Platform::GetBasePath()` (with a trailing `/`), so mounts resolve against a test's install tree, and no Player or tool runtime ever overrides its base path.
- **Covers:** Z2-I-MOD-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_TestHooks_BasePathOverride` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime test seams for modules"

### Z2-R-MOD-129 A tool host drives the game's runtime through the tool access header

- **Statement:** `Zenith_RuntimeToolAccess::Create` runs the shared startup with no initial world and no startup scene and hands the process services over once; `RunOneFrame` runs one frame-loop iteration with the tool hooks at fixed positions, returns false on a shutdown request while staying runnable, and reports unrecoverable frames as `RUNTIME_ERROR_FRAME_FAILED`; `CreateWorld`, `DestroyWorld`, `SetInputWorld`, `AddToolExtractor`, `FindModuleInstance` and `QueueSchemaJson` behave as specified, and a call at a position the table refuses fails its `ZENITH_CHECK`.
- **Covers:** Z2-I-MOD-07, Z2-I-MOD-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-081, Z2-D-046, Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_ToolAccess_CreateWithoutWorld`, `Test_Runtime_ToolAccess_FrameHookOrder`, `Test_Runtime_ToolAccess_WorldsAndExtractors`, `Test_Runtime_ToolAccess_SchemaQueueAndPositions`, `Test_Runtime_ToolAccess_IllegalPosition_Checks` (`ZenithRuntime_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Runtime tool access"

### Z2-R-MOD-130 The Player's entries apply the CRT maths mode first and the runtime checks it

- **Statement:** The first statement of the Player's `main` and `SDL_main` is `Zenith_ApplyCrtMathMode()`, before the root heap and the crash-helper dispatch, and every runtime create path begins with `ZENITH_CHECK(Zenith_IsCrtMathModeApplied())`; the Player passes its log router, OOM pressure flag and profiler to `Zenith_RuntimeMain` in the core order.
- **Covers:** Z2-I-MOD-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `ZenithPlayer.EntryCallsCrtMathModeFirst` (`cmake -P` CTest), `Test_PlayerMain_ProfilerCreatedAndPassed` (`ZenithPlayer_Tests`, Test step)
- **Design:** Zenith2/Docs/Design/MOD.md → "Player headers"; Zenith2/Docs/Design/MOD.md → "Zenith_Runtime.h"

### Z2-R-MOD-131 Android backup and transfer rules keep the device record and transient save files out

- **Statement:** Every Android game's generated Gradle project carries data-extraction and full-backup rules that include only `saves/` of the user mount, exclude `saves/resume.zsave`, `saves/last_profile.zslp` and ONLINE's device record `saves/online/device.zodv`, and are named by the manifest's `<application>` element.
- **Covers:** Z2-I-MOD-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-081
- **Budgets:** none
- **Acceptance evidence:** `CMake_Composition_BackupRules` (CTest fixture, Test step)
- **Design:** Zenith2/Docs/Design/MOD/GameBuild.md → "Backup and transfer rules"

## Coverage of inventory items

| Item | Classification | Requirements |
|---|---|---|
| Z2-I-MOD-01 | required | Z2-R-MOD-001, Z2-R-MOD-002, Z2-R-MOD-003, Z2-R-MOD-005, Z2-R-MOD-006, Z2-R-MOD-007, Z2-R-MOD-008, Z2-R-MOD-011, Z2-R-MOD-032, Z2-R-MOD-036, Z2-R-MOD-044, Z2-R-MOD-080, Z2-R-MOD-098, Z2-R-MOD-110 |
| Z2-I-MOD-02 | required | Z2-R-MOD-004, Z2-R-MOD-012, Z2-R-MOD-013, Z2-R-MOD-014, Z2-R-MOD-015, Z2-R-MOD-027, Z2-R-MOD-031, Z2-R-MOD-032, Z2-R-MOD-033, Z2-R-MOD-034, Z2-R-MOD-035, Z2-R-MOD-037, Z2-R-MOD-038, Z2-R-MOD-039, Z2-R-MOD-040, Z2-R-MOD-041, Z2-R-MOD-045, Z2-R-MOD-046, Z2-R-MOD-049, Z2-R-MOD-051, Z2-R-MOD-052, Z2-R-MOD-081, Z2-R-MOD-091, Z2-R-MOD-098, Z2-R-MOD-099, Z2-R-MOD-104 |
| Z2-I-MOD-03 | required | Z2-R-MOD-009, Z2-R-MOD-010, Z2-R-MOD-017, Z2-R-MOD-018, Z2-R-MOD-019, Z2-R-MOD-031, Z2-R-MOD-040, Z2-R-MOD-048, Z2-R-MOD-056, Z2-R-MOD-105 |
| Z2-I-MOD-04 | required | Z2-R-MOD-009, Z2-R-MOD-010, Z2-R-MOD-019, Z2-R-MOD-021, Z2-R-MOD-022, Z2-R-MOD-023, Z2-R-MOD-024, Z2-R-MOD-025, Z2-R-MOD-026, Z2-R-MOD-029, Z2-R-MOD-031, Z2-R-MOD-040, Z2-R-MOD-058, Z2-R-MOD-105 |
| Z2-I-MOD-05 | required | Z2-R-MOD-005, Z2-R-MOD-008, Z2-R-MOD-009, Z2-R-MOD-010, Z2-R-MOD-016, Z2-R-MOD-019, Z2-R-MOD-022, Z2-R-MOD-023, Z2-R-MOD-025, Z2-R-MOD-033, Z2-R-MOD-038, Z2-R-MOD-040, Z2-R-MOD-041, Z2-R-MOD-042, Z2-R-MOD-043, Z2-R-MOD-044, Z2-R-MOD-047, Z2-R-MOD-050, Z2-R-MOD-099, Z2-R-MOD-131 |
| Z2-I-MOD-06 | required | Z2-R-MOD-017, Z2-R-MOD-053, Z2-R-MOD-054, Z2-R-MOD-055, Z2-R-MOD-056, Z2-R-MOD-057, Z2-R-MOD-058, Z2-R-MOD-112, Z2-R-MOD-115, Z2-R-MOD-122, Z2-R-MOD-125, Z2-R-MOD-127 |
| Z2-I-MOD-07 | required | Z2-R-MOD-007, Z2-R-MOD-011, Z2-R-MOD-020, Z2-R-MOD-027, Z2-R-MOD-028, Z2-R-MOD-029, Z2-R-MOD-030, Z2-R-MOD-059, Z2-R-MOD-060, Z2-R-MOD-061, Z2-R-MOD-062, Z2-R-MOD-063, Z2-R-MOD-064, Z2-R-MOD-065, Z2-R-MOD-066, Z2-R-MOD-067, Z2-R-MOD-068, Z2-R-MOD-069, Z2-R-MOD-070, Z2-R-MOD-071, Z2-R-MOD-072, Z2-R-MOD-073, Z2-R-MOD-074, Z2-R-MOD-075, Z2-R-MOD-076, Z2-R-MOD-077, Z2-R-MOD-078, Z2-R-MOD-079, Z2-R-MOD-081, Z2-R-MOD-082, Z2-R-MOD-083, Z2-R-MOD-106, Z2-R-MOD-107, Z2-R-MOD-108, Z2-R-MOD-109, Z2-R-MOD-110, Z2-R-MOD-112, Z2-R-MOD-113, Z2-R-MOD-114, Z2-R-MOD-115, Z2-R-MOD-116, Z2-R-MOD-117, Z2-R-MOD-118, Z2-R-MOD-119, Z2-R-MOD-120, Z2-R-MOD-121, Z2-R-MOD-122, Z2-R-MOD-123, Z2-R-MOD-124, Z2-R-MOD-125, Z2-R-MOD-126, Z2-R-MOD-127, Z2-R-MOD-128, Z2-R-MOD-129, Z2-R-MOD-130 |
| Z2-I-MOD-08 | required | Z2-R-MOD-084, Z2-R-MOD-085, Z2-R-MOD-086, Z2-R-MOD-087, Z2-R-MOD-088, Z2-R-MOD-089, Z2-R-MOD-090, Z2-R-MOD-091, Z2-R-MOD-092, Z2-R-MOD-093, Z2-R-MOD-094, Z2-R-MOD-104, Z2-R-MOD-111 |
| Z2-I-MOD-09 | required | Z2-R-MOD-089, Z2-R-MOD-095, Z2-R-MOD-096, Z2-R-MOD-097, Z2-R-MOD-129 |
| Z2-I-MOD-10 | excluded | Z2-R-MOD-057, Z2-R-MOD-100 |
| Z2-I-MOD-11 | excluded | Z2-R-MOD-101 |
| Z2-I-MOD-12 | excluded | Z2-R-MOD-001, Z2-R-MOD-102 |
| Z2-I-MOD-13 | excluded | Z2-R-MOD-103 |

## Excluded and deferred

MOD has no deferred item. Its four excluded items each carry a rationale and a decision ID and have no tickets:

| Item | Requirement | Decision | Rationale |
|---|---|---|---|
| Z2-I-MOD-10 | Z2-R-MOD-100 | Z2-D-024 | a second link model, a binary-compatibility surface and an unload problem, for games that all live in one tree |
| Z2-I-MOD-11 | Z2-R-MOD-101 | Z2-D-002 | games outside the tree escape the gates and create a binary-compatibility surface |
| Z2-I-MOD-12 | Z2-R-MOD-102 | Z2-D-002 | every game must be covered by the same gates, libgraph and complexity checks |
| Z2-I-MOD-13 | Z2-R-MOD-103 | Z2-D-075 | it needs run-time code loading, a stable script API across releases and an untrusted-content boundary, each contradicting settled choices |

The four excluded requirements carry stage S1 because the Stage 1 SCOPE decision tickets (`Zenith2/Docs/Roadmap.md` → "Stage 1 Architecture validation", deliverable 20) record each exclusion with its decision ID; the inventory gives the items no stage, and no ticket implements them.

Two further items stay excluded by requirements that are themselves required: "No code is loaded at run time" (dynamic plugins) and "Games live only under Zenith2/Games" (out-of-repo projects). A change to any exclusion is a flagged proposal first (Z2-D-022) and an inventory change under `Zenith2/Docs/Requirements/Inventory.md` → "Change rule".
