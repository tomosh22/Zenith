# EDIT design — tests

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It lists every EDIT test with its file and the case it asserts, the fixtures, and the gate step that runs it. Tests follow `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates": doctest cases named `"ZenithEditor/<Type>/<Behaviour>"` (or `"ZenithEditorCore/<Type>/<Behaviour>"` in the library's tests) whose ID is `Test_<Type>_<Behaviour>`, pass or fail only, no pinned counts, every failure scenario its own case, tests in the same ticket as the code, and every name unique programme-wide (ruling TEST-NAME-UNIQUE: no EDIT name repeats another area's). No test asks a worker to record a count, digest or run; every proof below is a registered case the `Test` step runs (ruling WORKER-EVIDENCE), and no test double replaces a `final` engine class (ruling FINAL-DOUBLES: the cook server is faked at the child-process seam `Zenith_EditorChildProcess`; the texture table and the reload paths are tested through the editor's declared seams `Zenith_EditorTextureBackend` and `Zenith_EditorAssetReloadSink`, whose production implementations forward to the renderer and the registry; everything else observes real engine objects' state, the renderer being the real one on WARP or lavapipe, or absent in a headless session).

## Scope of this file

| Topic | Heading |
|---|---|
| targets, fixtures, suites | "Test targets and fixtures" |
| the tests, by subsystem | "Foundation tests" … "Integration tests" |
| measurement | "Benchmarks and budget runs" |

## Test targets and fixtures

### Test files

Four test executables hold EDIT's tests; each test runs in the executable that links everything it needs, and every row below names it in its "Executable" column:

| Executable | Built from | Links | Holds |
|---|---|---|---|
| `ZenithEditorCore_Tests` | `Zenith2/Tools/EditorCore/Tests/**` | `ZenithEditorCore` and its `DEPENDS`, `ZenithTestSupport`, doctest | the tests that need neither a runtime nor any `ZenithEditor` code: documents, JSON edits, the history over test-local commands and scripted operations, selection, the clipboard, events, the cook client against the fake server, the migration model, the host watcher. Every case works on a document-only session (`m_pxRuntime` null, `m_pxTypes` and `m_pxStrings` a test-owned registry and string table, § "Fixtures") over `Zenith_MemoryDocumentStorage` or a scratch host storage; **no case creates a runtime**, because a runtime needs a composition and the module catalogue, the ECS backends and the modules are `ZenithEditor`'s |
| `ZenithEditor_Tests` | `Zenith2/Tools/ZenithEditor/Tests/**` | `ZenithEditor_Objects` (every `Internal/**` source except the main, so the internals run without a second compile, `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites") and therefore every module, backend and `ZenithEditorCore`, `ZenithTestSupport`, doctest | every test that needs a runtime, a world, a built-in command, a `ZenithEditor` document kind (TERRAIN, UI), the shell, a renderer or a child process; the area-owned panels' tests under `Tests/<Area>/`; the render tests |
| `ZenithAuthoring_Tests` | `Zenith2/Tools/Authoring/Tests/**` | `ZenithAuthoring`, `ZenithTestSupport`, doctest | EDIT's one library test (`Test_AuthoringLibrary_VersionLinked`); every other file is an owner's (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 2) |
| `Zenith_Integration_EditorMigration`, `Zenith_Integration_EditorSchema` | `Zenith2/Tests/EditorMigration/`, `Zenith2/Tests/EditorSchema/` | `ZenithEditorCore` and the libraries each suite names | SCENE's and ECS's tests over EDIT's models (Z2T-SCENE-089, Z2T-ECS-100; rows ED26, ED27); owned and ticketed by those areas |

Their matrix is the tools', `ZENITH_TOOLS_HOST` (the `win` configure preset): EDIT's tests run in the `win` Test step and not under `win-asan` or `linux-tsan`, which build no tool of `ZENITH_TOOLS_HOST`; the libraries EDIT calls carry their own sanitizer coverage.

| File | Executable | Tests |
|---|---|---|
| `Tools/ZenithEditor/Tests/Zenith_EditorCommandLine_Tests.cpp` | `ZenithEditor_Tests` | § "Foundation tests" except the two library tests |
| `Tools/EditorCore/Tests/Zenith_EditorIDs_Tests.cpp` | `ZenithEditorCore_Tests` | `Test_EditorIDs_CommandIDHashesText` |
| `Tools/Authoring/Tests/Zenith_AuthoringLibrary_Tests.cpp` | `ZenithAuthoring_Tests` | `Test_AuthoringLibrary_VersionLinked` |
| `Tools/EditorCore/Tests/Zenith_EditorHistory_Tests.cpp`, `Tools/EditorCore/Tests/Zenith_EditorUndoProperty_Tests.cpp` | `ZenithEditorCore_Tests` | § "History and undo tests" rows marked `ZenithEditorCore_Tests` |
| `Tools/ZenithEditor/Tests/Zenith_EditorUndoCommands_Tests.cpp` | `ZenithEditor_Tests` | `Test_EditorUndo_EveryCommandRoundTrips`, `Test_EditorUndo_TerrainPropertySequences` |
| `Tools/EditorCore/Tests/Zenith_EditorSelection_Tests.cpp`, `Tools/EditorCore/Tests/Zenith_EditorClipboard_Tests.cpp` | `ZenithEditorCore_Tests` | § "Selection and clipboard tests" rows marked `ZenithEditorCore_Tests` |
| `Tools/EditorCore/Tests/Zenith_EditorDocuments_Tests.cpp`, `Tools/EditorCore/Tests/Zenith_EditorJsonEdit_Tests.cpp` | `ZenithEditorCore_Tests` | § "Document tests" rows marked `ZenithEditorCore_Tests` except the host watcher's (the terrain sink's case is in `Tools/ZenithEditor/Tests/Zenith_EditorTerrainTools_Tests.cpp`) |
| `Tools/EditorCore/Tests/Zenith_EditorHostWatcher_Tests.cpp` | `ZenithEditorCore_Tests` | `Test_EditorHostWatcher_ReportsSourceAndJournalChanges`, `Test_EditorHostWatcher_QueueCoalescesAndBounds` |
| `Tools/ZenithEditor/Tests/Zenith_EditorProjection_Tests.cpp` | `ZenithEditor_Tests` | § "Projection and world tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorPlay_Tests.cpp` | `ZenithEditor_Tests` | § "Play tests", `Test_EditorHistory_PlayHistorySeparate`, `Test_EditorSelection_MapsToPlayWorld` |
| `Tools/ZenithEditor/Tests/Zenith_EditorShell_Tests.cpp` | `ZenithEditor_Tests` | § "Shell and viewport tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorGizmoMath_Tests.cpp`, `Tools/ZenithEditor/Tests/Zenith_EditorCamera_Tests.cpp` | `ZenithEditor_Tests` | § "Camera gizmo and snapping tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorPicking_Tests.cpp` | `ZenithEditor_Tests` | § "Picking tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorPanels_Tests.cpp`, `Tools/ZenithEditor/Tests/Zenith_NewProjectWizard_Tests.cpp` | `ZenithEditor_Tests` | § "Panel tests" |
| `Tools/EditorCore/Tests/Zenith_EditorCookClient_Tests.cpp` | `ZenithEditorCore_Tests` | § "Pipeline tests" rows marked `ZenithEditorCore_Tests` |
| `Tools/ZenithEditor/Tests/Zenith_EditorAssetOps_Tests.cpp` | `ZenithEditor_Tests` | § "Pipeline tests" rows marked `ZenithEditor_Tests`, `Test_EditorDocuments_RenameKeepsIdentity` |
| `Tools/ZenithEditor/Tests/Zenith_EditorHotReload_Tests.cpp` | `ZenithEditor_Tests` | § "Hot reload tests" |
| `Tools/EditorCore/Tests/Zenith_EditorMigration_Tests.cpp` | `ZenithEditorCore_Tests` | § "Migration and recovery tests" rows marked `ZenithEditorCore_Tests` |
| `Tools/ZenithEditor/Tests/Zenith_EditorRecovery_Tests.cpp`, `Tools/ZenithEditor/Tests/Zenith_EditorFormats_Tests.cpp` | `ZenithEditor_Tests` | § "Migration and recovery tests" rows marked `ZenithEditor_Tests` |
| `Tools/ZenithEditor/Tests/Zenith_EditorSourceControl_Tests.cpp` | `ZenithEditor_Tests` | § "Source control tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorAutomation_Tests.cpp` | `ZenithEditor_Tests` | § "Automation tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorTerrainTools_Tests.cpp`, `Tools/ZenithEditor/Tests/Zenith_EditorPrefabTools_Tests.cpp`, `Tools/ZenithEditor/Tests/Zenith_EditorAuthoringWidgets_Tests.cpp`, `Tools/ZenithEditor/Tests/<Area>/…` | `ZenithEditor_Tests` | § "Authoring tool tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorHeadless_Tests.cpp` | `ZenithEditor_Tests` | § "Headless editing tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorRender_Tests.cpp` | `ZenithEditor_Tests` | § "Render tests", and the `editor-render` rows of other sections |
| `Tools/ZenithEditor/Tests/Zenith_EditorIntegration_Tests.cpp` | `ZenithEditor_Tests` | § "Integration tests" |
| `Tools/ZenithEditor/Tests/Zenith_EditorFixtureSetup_Tests.cpp` | `ZenithEditor_Tests` | the fixture-setup case of § "Fixtures" |

Header-only helpers live under `Tools/EditorCore/Tests/Support/` (the fake cook server `Zenith_EditorFakeCookServer.h`, the in-memory document builders, the scripted operation `Zenith_EditorScriptedOperation.h`, the reflection fixtures `Zenith_EditorTestTypes.h` (§ "Fixtures"), the minimal glTF writer `Zenith_EditorTestGltf.h`) and `Tools/ZenithEditor/Tests/Support/` (the recording seams `Zenith_EditorRecordingTextureBackend.h` and `Zenith_EditorRecordingReloadSink.h`, `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Texture backend header", "Asset reload sink header"); nothing there is compiled on its own ([Z2-N-ARCH-255], ruling X184), and both test executables include them through the include root. No helper is a double of a `final` engine class (ruling FINAL-DOUBLES): the fake cook server replaces the child process behind the interface `Zenith_EditorChildProcess`, the recording backend and sink implement the editor's own declared seams, and every other test observes the real engine object's state.

### Fixtures

<!-- elaboration -->
The test game is `Zenith2/Games/EditorFixture/` (`Zenith2/Docs/Design/EDIT.md` → "Test game"), a registered game, so the configure writes its `composition.json` and builds its Player, and no test generates a composition:

| Item | Content |
|---|---|
| `game.json` | the full file, every key of `Zenith2/Docs/Architecture/BuildSystem.md` → "The game.json schema" present: `{ "schemaVersion": 1, "name": "EditorFixture", "displayName": "Editor Fixture", "version": "0.1.0", "platforms": ["windows"], "modules": ["Animation", "Audio", "Environment", "Gameplay", "Physics"], "backends": { "ecs": "Custom", "physics": "Jolt" }, "content": { "roots": ["Assets", "Scenes", "Schemas", "Scripts"], "startupScene": "Scenes/Fixture.scene.json" } }` at Stage 3; `Audio` is selected from Stage 3, so a play world of a scratch copy has AUDIO's Luau bindings (a copy cannot add a module: a changed `game.json` makes its composition record stale, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor composition"), and the Stage 3 module catalogue carries its row; `UI` is added to `modules` by the Stage 4 ticket that first tests it and `VFX` by the Stage 5 ticket that first tests it (a module is added in the diff that first tests it, so the game configures at every stage); like every repository game it carries `Tests/Boot.gametest.json` (one tick), whose cook of the tracked tree needs no generated source |
| `Scenes/Fixture.scene.json` | 11 entities in a two-level hierarchy: a floor with a static box collider, a 1 m box with a dynamic body 0.5 m above it, a camera, a directional light, a realtime reflection probe (`CaptureRequest`), an environment settings entity with an anchor, a mover entity with the behaviour of `Scripts/FixtureMover.luau` (a `speed` property), and four empty entities; the fixture-setup case adds a twelfth, the mesh entity referencing `Assets/Crate.glb` by the fixed GUID `xEDIT_FIXTURE_CRATE_GUID`, to the scratch copy only, so the tracked tree never references a source it does not hold; from Stage 4 one prefab instance of `Scenes/Flat.prefab.json`; from Stage 5 an effect entity of `Assets/Spark.vfx.json` |
| `Scenes/Flat.prefab.json`, `Scenes/Variant.prefab.json` (Stage 4) | a flat prefab of three entities; a variant of it |
| `Schemas/Fixture.Stats.schema.json` | the data-defined component `Fixture.Stats` at version 1 (two fields); the version 2 file (one renamed, one retyped field, with its migration chain) is `Tools/ZenithEditor/Tests/Fixtures/Schemas/Fixture.Stats.v2.schema.json`, written over the scratch copy by the schema tests |
| `Assets/` | `Fixture.material.json` (on `Material_Standard`), `Fixture.terrain.json` with its 2 × 2 tile documents and one layer document, `Fixture.animgraph.json`, a placement rule set; `Crate.glb` and its `.meta` are not committed (a binary content source, ruling Q55): the fixture-setup case writes `Crate.glb` from declared constants with `Zenith_EditorTestGltf.h` and `Crate.glb.meta` with the fixed GUID (a `.meta` the cook server recognises, so `assignGuids` creates nothing for it) into the scratch copy |
| `Scripts/FixtureMover.luau` | a behaviour moving its entity along +X at `speed` m/s |

Every test that writes sources, starts the real cook server or a child editor works on a scratch copy, never on the tracked tree ([Z2-N-ARCH-025]): the setup case `EDIT_EditorFixtureGames` (`Test_EditorFixture_SetupCopiesGames`, doctest suite `z2-fixture-setup`, in `ZenithEditor_Tests`) empties `${ZENITH_OUT}/test-fixtures/EDIT/` and copies into `Games/` there `Games/EditorFixture/` (plus `Crate.glb`, its `.meta` and the mesh entity) and, when it exists, `Games/OpenWorldSlice/` (the walkthrough rehearsals' content, Stage 4; each rehearsal case fails naming `OpenWorldSlice` when the copy is absent, and the rehearsal tickets are blocked by OpenWorldSlice's content ticket, so the setup case itself never fails for it). It is registered by the test registry as CTest fixture `EditEditorFixtureGames` with variable `ZENITH2_EDIT_FIXTURE_GAMES=${ZENITH_OUT}/test-fixtures/EDIT` ([Z2-N-ARCH-257]); every consuming test sits in doctest suite `z2-edit-cooked` (registered with `FIXTURES_REQUIRED EditEditorFixtureGames`), copies the game it mutates into a subdirectory of its own first, and passes `--game-root <its copy>/Games` to the editor or the cook. The registry wraps both in `if(TARGET ZenithEditor_Tests)`, so they exist in `win` alone. The tests in `z2-edit-cooked` are every test that launches a child editor (`Test_EditorMigration_ResumeAfterKillAtEachJournalState`, `Test_EditorRecovery_RestoreAfterCrash`, `Test_EditorRecovery_ChangedFileRestoresAsCopy`, `Test_EditorSessionLock_SecondEditorRefused`, `Test_EditorHeadless_OpenEditSaveRoundTrip`, `Test_EditorHeadless_ScenePublishFromHeadlessBoot`), the real cook server (`Test_EditorCookClient_RealServerHandshake`, every `Test_EditorIntegration_*` case, the four walkthrough rehearsals), writes the fixture game's sources (`Test_EditorSchemaPanel_ApplyMigratesDocumentsAndWorld`, `Test_EditorSchemaPanel_ApplyKeepsOpenDocumentHistory`, `Test_EditorSchemaPanel_WindowRefusalRestores`, `Test_EditorSchema_UndoRestoresDocumentsAndRows`, the schema-apply arm of `Test_EditorFrame_CallsOnlyInLegalHooks`, `Test_EditorPhysics_DropToRestOneRecord`), restarts a child editor's runtime (`Test_EditorApp_RestartFailureExitsOneAfterAutosave`) or exits on device loss with the real cook server child (`Test_EditorApp_DeviceLostAutosavesAndExitsSix`); every member's row below carries the label; the setup case itself is `Test_EditorFixture_SetupCopiesGames`.

What a test receives, all from § "Host paths" of Foundation.md applied to the test executable's own path: `m_strHostToolsDir` (`ZenithEditor.exe`, `ZenithCook.exe` and `EditorFixture.exe` of the same configuration are there), `m_strOutRoot` (`declared_deps.json`, the `composition.json` of `EditorFixture`, the scratch root); plus `ZENITH_TEST_FIXTURE_ROOT` (`Tools/ZenithEditor/Tests/Fixtures`, [Z2-N-ARCH-250]) and the variable above.

Other fixtures:

- `Tests/Fixtures/Scripts/`: automation scripts, including the four walkthrough rehearsals under `Walkthroughs/`.
- `Tests/Fixtures/Formats/`: hand-written session, layout, preferences, recovery and clipboard files of version 1 and of a version 2 (newer) for the reader tests; autosave and thumbnail files are generated by the tests from their writers, never committed as binaries.
- `Tests/Fixtures/Git/`: none; the source-control tests create a scratch repository under `${ZENITH_OUT}/test-fixtures/EDIT/git/` with the system `git`.
- In-memory documents (`Zenith_MemoryDocumentStorage`) serve every `ZenithEditorCore_Tests` case; those cases create document-only sessions and never a runtime (§ "Test files"). Each such session gets its type registry from the test: the case creates a `Zenith_TypeRegistry` and a `Zenith_StringTable` of its own, registers the reflection fixtures into the registry (the component types of the scene fixtures, through ZenithReflect's registration, in the header-only `Tools/EditorCore/Tests/Support/Zenith_EditorTestTypes.h`), and passes both as `Zenith_EditorSessionDesc::m_pxTypes` and `m_pxStrings` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Session description"), destroying them after the session; so a document-only session opens scene documents through `Zenith_SceneDocument::Load` exactly as a runtime session does. A `ZenithEditor_Tests` case that needs a runtime creates it with `Zenith_RuntimeToolAccess::Create` and the `EditorFixture` composition built by the editor's own composition code (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor composition") from the record read under `m_strOutRoot`.
- Every test session is seeded: in-process sessions pass a non-zero `m_ulRandomSeed`, child editors `--random-seed`, and every committed automation script carries `seed` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Session random source").
- Test sessions run with `--user-dir` pointed at a scratch directory, so no test writes the user's preferences.

### Suites and gate steps

| Suite label | Contents | Timeout | Gate step |
|---|---|---|---|
| (default) | unit and component tests | the default CTest timeout | `Test` (`win`) |
| `z2-stress` | the property-based undo sequences, the tests that launch child editor or cook processes, the walkthrough rehearsals, the render tests | 600 s | `Test` (`win`) |
| `z2-edit-cooked` | the tests on scratch copies of a game (§ "Fixtures"), with `FIXTURES_REQUIRED EditEditorFixtureGames` | 600 s when also `z2-stress`, else the default | `Test` (`win`) |
| `editor-render` | § "Render tests": a hidden-window editor (`Zenith_ToolRuntimeDesc::m_bHiddenWindow`) on WARP (`--set rhi.backend=d3d12 --set rhi.adapter=software`) and on lavapipe (`--set rhi.backend=vulkan --set rhi.adapter=software`, the shared gate environment's `VK_DRIVER_FILES`), each case run once per backend (`Zenith2/Docs/Design/RHI.md` → "Required test groups"); the doctest suite name is the CTest label ([Z2-N-ARCH-251]); no `needs-gpu` (ruling X170) | 600 s (also `z2-stress`) | `Test` (`win`) |

## Foundation tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorCommandLine_ParsesEditorOptions` | `ZenithEditor_Tests` | every editor option of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executable and command line" is parsed into `Zenith_EditorOptions` (`--random-seed 0x10` gives 16; `--random-seed 0` is `EDITOR_ERROR_USAGE`), and the remaining arguments are forwarded in order |
| `Test_EditorCommandLine_MissingGame_Usage` | `ZenithEditor_Tests` | no `--game`, and `--game` naming a directory without `game.json`, each return `EDITOR_ERROR_USAGE` with the documented message |
| `Test_EditorCommandLine_HeadlessScriptResolvesRecoveryDiscard` | `ZenithEditor_Tests` | `--headless --automation-script x --recovery ask` resolves to `EDITOR_RECOVERY_CHOICE_DISCARD` with one log line |
| `Test_EditorComposition_MatchesRecordHash` | `ZenithEditor_Tests` | the composition built from the fixture game's record has the record's `compositionHash.windows` |
| `Test_EditorComposition_StaleRecord_Refused` | `ZenithEditor_Tests` | a record whose hash does not match, and one naming an unknown module, each fail startup step 4 with exit code 3 |
| `Test_EditorComposition_HashMatchesMinimalGolden` | `ZenithEditor_Tests` | `Zenith_ComputeEditorCompositionHash` over `Games/Minimal`'s values gives MOD's golden printed spelling `704cef4b6c1da71a` (`Zenith2/Docs/Design/MOD.md` → "Hash golden") |
| `Test_EditorCatalogue_CoversDeclaredModules` | `ZenithEditor_Tests` | against `declared_deps.json`: every MODULE library in ZenithEditor's `depends` has a row with its `moduleName`, every module BACKEND library there a row with its `backendName`, and every row's library is in `depends`; a fixture record with one extra module library and one with a missing one each fail naming the library |
| `Test_EditorHostPaths_ResolveFromExecutable` | `ZenithEditor_Tests` | an executable path `<root>/Zenith2/out/0123456789abcdef/win/Release/bin/ZenithEditor.exe` gives the out root, the Zenith2 root, the games root and the host tools directory; `--game-root` overrides the games root; a path with no `declared_deps.json` three levels up returns `EDITOR_ERROR_HOST_PATH` |
| `Test_EditorCommandLine_BadGameNameRefused` | `ZenithEditor_Tests` | `harbour`, `1Harbour` and a 33-character name each return `EDITOR_ERROR_USAGE` through `Zenith_IsValidGameName` |
| `Test_EditorApp_RestartRuntimeKeepsSession` | `ZenithEditor_Tests` | an in-process headless session with two open documents (one dirty), four records, a selection and clipboard content restarts its runtime through `editor.restartRuntime`: the documents' bytes and state IDs, the undo count, the selection and the clipboard are unchanged, the edit world equals a fresh projection, `runtimeRestarted` is published with the requesting frame's index, and the restart ran between frames (`RestartRuntime`'s `ZENITH_CHECK(!m_bInsideHook)` did not fire); a scene document open across the restart is re-read under the new registry (`RebindTypes`) with its bytes and state ID unchanged; its windowed arm is `Test_EditorApp_RestartRebindsPlatformObjects` |
| `Test_EditorApp_RestartRebindsPlatformObjects` | `ZenithEditor_Tests` | `editor-render`, Stage 4: an in-process hidden-window editor restarts its runtime with a copied entity in the clipboard; a copy after the restart reaches the new platform's clipboard (read back through `Zenith_Clipboard::GetText` as the entity's `zenith2.editor-clipboard` document), a paste from the system clipboard after it pastes the entity, and the ImGui platform adapter's `DisplaySize` equals the new main window's pixel size in the first frame after the restart (Pipeline.md "Runtime restart" steps 4 and 7) |
| `Test_EditorApp_RestartFailureExitsOneAfterAutosave` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`, Stage 4: a child editor (`--headless --automation-script --random-seed 1` on a scratch copy of `EditorFixture`) dirties a document and runs `editor.restartRuntime {faultAtStep: 6}`; the child exits with code 1 and the dirty document's `.zautosave` exists with its state ID |
| `Test_EditorApp_DeviceLostAutosavesAndExitsSix` | `ZenithEditor_Tests` | `editor-render`, `z2-edit-cooked`, Stage 4: an in-process hidden-window editor on WARP with RHI's `Zenith_RHITestHooks` as `m_pxRHIFaultInjector`, the real cook server child on its own scratch copy of `EditorFixture` and one dirty document; a second device loss within 60 s of the first makes `Run` return 6 within 9 s of the loss, with the document's `.zautosave` written, no prompt opened and the cook server process exited (Foundation.md "Device loss") |
| `Test_EditorSession_SeededIDsReproducible` | `ZenithEditor_Tests` | one script of creates, pastes and duplicates run twice with `--random-seed 7`, once windowed with two scene views and once headless, generates equal IDs; two unseeded runs generate different IDs (Foundation.md "Session random source") |
| `Test_EditorFrame_CallsOnlyInLegalHooks` | `ZenithEditor_Tests` | `editor-render`, `z2-edit-cooked` (its schema apply writes the scratch copy's sources): Play, Stop, a preview opened and evicted, a governor transition to `CRITICAL` and a schema apply run under the runtime's hook checks (Foundation.md "Tool access calls per hook") with no check firing, and the preview world evicted by the governor is destroyed in that frame's `OnFrameEnd` |
| `Test_EditorStartup_UnwindsEachStep` | `ZenithEditor_Tests` | a fault injected at each startup step 5–15 unwinds every earlier step (the root heap's leak table is empty) and returns the step's exit code |
| `Test_EditorStartup_CrashHelperArgumentReachesHelper` | `ZenithEditor_Tests` | `z2-stress`: `ZenithEditor.exe --zenith-crash-helper` launched with no inherited handles exits with the helper's own refusal code 3 (DEVT's validation), never with the command line's usage code 2, and writes no `editor.startup` line (the helper ran before parsing) |
| `Test_EditorDocumentTypes_RegistrationOrderMatchesAsset` | `ZenithEditor_Tests` | the registry's types appear in the order of Foundation.md "Editor document type registrations" for the rows whose owners have moved, and `Freeze` succeeds |
| `Test_EditorShutdown_OrderAndCleanAutosaves` | `ZenithEditor_Tests` | a clean shutdown runs the ten steps in order (recorded by test hooks), steps 1 and 2 inside `Run`'s frames and steps 3–10 in the destructor, and leaves no `.zautosave` file; with a pending record whose server never answers, `Run` keeps running frames until the inactivity timeout (a test clock) cancels and resolves it, then completes the shutdown |
| `Test_EditorFixture_SetupCopiesGames` | `ZenithEditor_Tests` | `z2-fixture-setup` (CTest fixture `EditEditorFixtureGames`): empties `${ZENITH2_EDIT_FIXTURE_GAMES}`, copies `Games/EditorFixture/` (and `Games/OpenWorldSlice/` when present) into its `Games/`, writes `Assets/Crate.glb` from declared constants with `Zenith_EditorTestGltf.h`, writes its `Crate.glb.meta` with the fixed GUID constant `xEDIT_FIXTURE_CRATE_GUID`, and adds the mesh entity referencing it to the copy's `Scenes/Fixture.scene.json` through SCENE's edit API and canonical writer; fails naming the variable when it is unset |
| `Test_EditorIDs_CommandIDHashesText` | `ZenithEditorCore_Tests` | `ZenithEditorCore_Tests` (the library-creation ticket, EDIT.md "Library creation ticket"): `Zenith_MakeEditorCommandID("entity.create").m_ulHash` equals `Zenith_Hash64` of its 13 bytes, and two different identifiers give different IDs |
| `Test_AuthoringLibrary_VersionLinked` | `ZenithAuthoring_Tests` | `ZenithAuthoring_Tests` (the same ticket): `Zenith_GetAuthoringLibraryVersion()` returns `uZENITH_AUTHORING_LIBRARY_VERSION` |

## History and undo tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorHistory_ExecuteRecordsOneRecord` | `ZenithEditorCore_Tests` | a command of three changes produces one record with three changes, the label, the documents and the selection before and after |
| `Test_EditorHistory_NoChangeCommandRecordsNothing` | `ZenithEditorCore_Tests` | a command applying no change leaves both stacks unchanged |
| `Test_EditorHistory_FailedCommandLeavesNoTrace` | `ZenithEditorCore_Tests` | a failure injected at the k-th change of a five-change command, for every k, leaves every document's canonical bytes and both stacks unchanged |
| `Test_EditorHistory_UndoRedoRestoreBytes` | `ZenithEditorCore_Tests` | Execute, Undo and Redo of one command of each change kind `ZenithEditorCore` implements (`SCENE_EDIT`, `JSON_EDIT`, `DOCUMENT_BYTES` of a document) and of a scripted operation restore the documents' canonical bytes exactly (the kinds `ZenithEditor` implements are `Test_EditorUndo_EveryCommandRoundTrips`) |
| `Test_EditorHistory_NewCommandClearsRedo` | `ZenithEditorCore_Tests` | after an undo, a new command empties the redo stack |
| `Test_EditorHistory_ReentrantExecuteRefused` | `ZenithEditorCore_Tests` | a command that executes another command fails with `EDITOR_ERROR_COMMAND_REENTRANT` and records nothing |
| `Test_EditorHistory_InteractionMergesAll` | `ZenithEditorCore_Tests` | 120 commands inside one interaction with one merge key give one record, keeping the first inverse and the last forward value |
| `Test_EditorHistory_MergeWindowRespected` | `ZenithEditorCore_Tests` | two commands with the same key 400 ms apart merge, 600 ms apart do not, and an intervening command or an undo prevents merging |
| `Test_EditorHistory_MemoryBoundEvictsOldest` | `ZenithEditorCore_Tests` | exceeding `editor.undo.maxBytes` evicts whole oldest undo records first, then redo records, and counts `editor.undo.evicted` |
| `Test_EditorHistory_RecordCountBound` | `ZenithEditorCore_Tests` | the 1,001st record evicts the oldest |
| `Test_EditorHistory_OversizeRecordBecomesFence` | `ZenithEditorCore_Tests` | a JSON record above `editor.undo.maxRecordBytes`, and a record of a scripted operation of kind `EDITOR_CHANGE_TERRAIN_EDIT` above `editor.undo.maxTerrainRecordBytes`, are applied, replaced by fences, and undo stops at each with `EDITOR_ERROR_HISTORY_FENCE` |
| `Test_EditorHistory_MigrationRecordExemptFromRecordCap` | `ZenithEditorCore_Tests` | a document-bytes record larger than `editor.undo.maxRecordBytes` but within `editor.undo.maxBytes` is kept (older records evicted to fit) and undoes; one larger than `editor.undo.maxBytes` becomes a fence naming its backup batch |
| `Test_EditorHistory_AsyncChangeLocksHistory` | `ZenithEditorCore_Tests` | with a scripted asynchronous operation pending: every history, document-table and migration-model refusal of Foundation.md "Asynchronous changes" (lock table) returns `EDITOR_ERROR_HISTORY_PENDING` and every allowed command runs; the record is on neither stack; commit publishes `historyCommitted` and puts it on the undo stack; a transient failure compensates and leaves both stacks and documents as before; a permanent failure of an undo fences the record; a failed compensation leaves an "incomplete" fence, publishes `historyFailed` naming the dirty touched document and leaves it dirty with its unsaved state (no reload); the autosave write is `Test_EditorAutosave_IncompleteFenceWritesTouchedDocuments` (Stage 4) |
| `Test_EditorHistory_AsyncTimeoutIsInactivity` | `ZenithEditorCore_Tests` | with a test clock and the fake server: a request whose server sends a `progress` line every 200 s runs 900 s and commits; one whose server sends no line for `editor.history.asyncTimeoutSeconds` is cancelled at that point, resolved, compensated, and reported with `EDITOR_ERROR_HISTORY_ASYNC_TIMEOUT` |
| `Test_EditorHistory_ReloadPrunesOnlyThatDocument` | `ZenithEditorCore_Tests` | after an external change reloads clean document D, the records touching D (and the older records of a document sharing a record with D) are gone, a record of an unrelated document F still undoes, and no fence exists |
| `Test_EditorHistory_FailedUndoCompensates` | `ZenithEditorCore_Tests` | an inverse forced to fail mid-record re-applies the inverses already applied, leaves the document as before the attempt, and fences the record |
| `Test_EditorHistory_CloseDocumentPrunesDependentRecords` | `ZenithEditorCore_Tests` | closing D removes every record touching D and the older records of E that a removed record touching D and E precedes |
| `Test_EditorHistory_UndoToEntry` | `ZenithEditorCore_Tests` | `edit.undoTo` undoes every record up to and including the chosen one |
| `Test_EditorHistory_DirtyFollowsStateID` | `ZenithEditorCore_Tests` | edit, save, undo makes the document dirty; redo makes it clean again; undo of every record after an open makes it clean |
| `Test_EditorHistory_PlayHistorySeparate` | `ZenithEditor_Tests` | commands during play go to the play history and leave the main history untouched; Stop discards the play history |
| `Test_EditorUndo_PropertySequences` | `ZenithEditorCore_Tests` | `z2-stress`: 10,000 random sequences of up to 200 operations (test-local commands of the five scene-edit and six JSON-edit kinds, undos, redos) over a scene and a JSON graph document in a document-only session end in the state of a model replaying the surviving commands; creating commands carry their IDs (`id`, `idSeed`) from the test's seeded generator, so the run and the model create identical IDs; the seed is fixed per case and printed on failure |
| `Test_EditorUndo_TerrainPropertySequences` | `ZenithEditor_Tests` | `z2-stress`: 2,000 random sequences of up to 100 operations (strokes, structural terrain edits, undos, redos) over the fixture terrain document end in the state of the model replaying the surviving operations |
| `Test_EditorUndo_EveryCommandRoundTrips` | `ZenithEditor_Tests` | every registered command with `EDITOR_COMMAND_FLAG_CHANGES_DOCUMENTS` executed on its fixture satisfies invariants 1 and 2 of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Round-trip invariants"; a command without a fixture entry fails the test |

## Selection and clipboard tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorSelection_SetAddToggleRemove` | `ZenithEditorCore_Tests` | each operation's result, order and active object |
| `Test_EditorSelection_LimitRefused` | `ZenithEditorCore_Tests` | `Add` beyond `editor.selection.maxObjects` returns `EDITOR_ERROR_SELECTION_LIMIT` and changes nothing |
| `Test_EditorSelection_RootsExcludeSelectedDescendants` | `ZenithEditorCore_Tests` | `ListRootEntities` omits a selected child of a selected parent |
| `Test_EditorSelection_SurvivesUndoAndReload` | `ZenithEditorCore_Tests` | references to entities stay valid across undo, redo and a document reload (the reconcile half is `Test_EditorProjection_ReconcileRestoresDriftedWorld`) |
| `Test_EditorSelection_UndoRestoresSelection` | `ZenithEditorCore_Tests` | undoing a deletion re-selects the deleted entities; redo restores the selection after |
| `Test_EditorSelection_MapsToPlayWorld` | `ZenithEditor_Tests` | the main selection resolves to play-world entities with the same persistent IDs |
| `Test_EditorClipboard_PasteRemapsIDs` | `ZenithEditorCore_Tests` | copy and paste of a subtree with an internal entity reference gives fresh IDs and the reference pointing at the pasted copy |
| `Test_EditorClipboard_PasteRebalancesFullGroup` | `ZenithEditorCore_Tests` | a paste into a sibling group with equal sort keys rebalances the group in one record |
| `Test_EditorClipboard_SystemClipboardRoundTrip` | `ZenithEditorCore_Tests` | the clipboard document `WriteDocument` produces for a copied subtree parses back through `ReadDocument` into an equal set (no platform clipboard: the system paths call these two) |
| `Test_EditorClipboard_ForeignTextRefused` | `ZenithEditorCore_Tests` | non-JSON text, a JSON object of another schema, and a newer version given to `ReadDocument` each fail with `EDITOR_ERROR_CLIPBOARD_FORMAT` |
| `Test_EditorClipboard_UnknownComponentPreserved` | `ZenithEditorCore_Tests` | pasting a component unknown to the game keeps it as preserved unknown data |

## Document tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorDocuments_OpenOncePerSession` | `ZenithEditorCore_Tests` | opening the same path twice returns the same document |
| `Test_EditorDocuments_OpenMigratedThenSave` | `ZenithEditorCore_Tests` | an older-version document opens `MIGRATED`; saving it writes the plan's bytes (`Test_EditorSave_MigratedEqualsPlanBytes` covers the bytes) |
| `Test_EditorSave_MigratedEqualsPlanBytes` | `ZenithEditorCore_Tests` | the saved bytes equal the migration plan's new bytes for that document |
| `Test_EditorDocuments_NewerIsReadOnly` | `ZenithEditorCore_Tests` | a newer-version document opens read-only and every edit command fails with `EDITOR_ERROR_DOCUMENT_READ_ONLY` before any change |
| `Test_EditorDocuments_SaveUnchangedWritesNothing` | `ZenithEditorCore_Tests` | saving a clean document performs no storage mutation (the memory storage's mutation count) |
| `Test_EditorDocuments_SaveStaleDigestConflicts` | `ZenithEditorCore_Tests` | a file changed on disk after open makes save fail with `STALE_PLAN` and the document CONFLICTED |
| `Test_EditorDocuments_SaveStorageLockedStaysDirty` | `ZenithEditorCore_Tests` | a held writer lock makes save fail with `EDITOR_ERROR_STORAGE_LOCKED`, leaves the document dirty and names the holder |
| `Test_EditorDocuments_SaveLowDiskStaysDirty` | `ZenithEditorCore_Tests` | a storage reporting too few free bytes leaves the document dirty with the free and required bytes in the notification |
| `Test_EditorDocuments_ExternalChangeReloadsClean` | `ZenithEditorCore_Tests` | in a document-only session, an external change of a clean document, delivered through `Zenith_EditorDocumentTable::OnSourcesChanged` as a host path, reloads it and prunes its records (no fence); the same change reported a second time (the cook server's `changed` after the watcher's record) changes nothing; a change whose digest was noted by `NoteEditorWrite` is ignored once and counted; a removed file makes the document MISSING; a path naming no open document is ignored (the projection half is `Test_EditorProjection_ExternalReloadReconciles`) |
| `Test_EditorHostWatcher_ReportsSourceAndJournalChanges` | `ZenithEditorCore_Tests` | a recursive watcher over a scratch root and a non-recursive one over a scratch journal directory (`${ZENITH_OUT}/test-fixtures/EDIT/watch/`): adding, twice modifying within the debounce, renaming and removing a nested `Scripts/Mover.luau` drain as ADDED, one MODIFIED, REMOVED then RENAMED, and REMOVED; a write under `.editor/` drains nothing; appends to `reload.journal` drain as MODIFIED; a file in a subdirectory of the journal directory drains nothing from the non-recursive watcher; a watcher over a missing directory returns `EDITOR_ERROR_HOST_PATH` (Foundation.md "Host watchers") |
| `Test_EditorHostWatcher_QueueCoalescesAndBounds` | `ZenithEditorCore_Tests` | `Zenith_EditorHostChangeQueue` on a test clock: three reports of one path within the debounce flush as one record of the last kind, a report after the debounce as a second record; 5,000 distinct paths flushed at once drain as 4,096 records, the oldest dropped and the last the watched directory's RESCAN record, with `editor.watcher.dropped` counting 905; `NoteOverflow` flushes one RESCAN record (Declarations.md "Host watcher header") |
| `Test_EditorDocuments_PlannedWriteRule` | `ZenithEditorCore_Tests` | `CheckPlannedWrite` returns success for a closed and a clean document and `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` for a dirty, a conflicted, a missing and a migrated one; `DeferExternalChanges` holds notifications and `ResumeExternalChanges` drops those of the adopted digest and runs the rest in order |
| `Test_EditorSave_InsertChangesOnlyNewLines` | `ZenithEditorCore_Tests` | inserting an entity between two siblings and saving changes exactly the new object's lines in the line diff of the canonical bytes (Pipeline.md "Rules for files the editor writes" rule 2) |
| `Test_EditorDocuments_ReplaceStateKeepsHistory` | `ZenithEditorCore_Tests` | `ReplaceState` with given bytes and a state ID sets both, marks the document clean or dirty by that ID, and leaves the history's records undoable |
| `Test_EditorDocuments_ExternalChangeConflictsDirty` | `ZenithEditorCore_Tests` | an external change of a dirty document makes it CONFLICTED; "Keep mine" saves over with the new expected digest; "Take disk" reloads |
| `Test_EditorJsonEdit_InverseRestoresBytes` | `ZenithEditorCore_Tests` | every edit kind at the first, a middle and the last position: edit then inverse restores the canonical bytes; an inverse's inverse equals the edit |
| `Test_EditorJsonEdit_InvalidPointerRefused` | `ZenithEditorCore_Tests` | a malformed pointer, a missing member, a wrong kind, a duplicate key and an index out of range each fail with `EDITOR_ERROR_JSON_EDIT` and leave the tree unchanged |
| `Test_EditorJsonEdit_KeepsUnknownMemberAnchors` | `ZenithEditorCore_Tests` | inserting a known member before an unknown member keeps the unknown member's anchor and the canonical bytes |
| `Test_EditorTerrainSink_OneBatchPerSave` | `ZenithEditor_Tests` | `Zenith_TerrainDocument::Save` through the editor's sink writes the terrain document and its dirty tiles in one journaled batch with expected digests |

## Projection and world tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorProjection_EachEditKind` | `ZenithEditor_Tests` | each scene edit kind of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind" changes the edit world exactly as its row states (inspector reads after `ApplyOutOfTick`) |
| `Test_EditorProjection_UnknownComponentNotCreated` | `ZenithEditor_Tests` | an unknown component of the document is not created in the edit world and is kept in the document |
| `Test_EditorProjection_BufferSplitAtCap` | `ZenithEditor_Tests` | a projection larger than `editor.projection.bufferBytes` is applied as several buffers with the same result |
| `Test_EditorProjection_ErrorMarksReconcile` | `ZenithEditor_Tests` | an injected `ApplyOutOfTick` error marks the document for reconcile and the next reconcile restores the state |
| `Test_EditorProjection_ReconcileRestoresDriftedWorld` | `ZenithEditor_Tests` | a world with a destroyed entity, a changed property and an extra component reconciles to the resolved document exactly |
| `Test_EditorProjection_ExternalReloadReconciles` | `ZenithEditor_Tests` | an external change of a clean projected scene document reloads it and reconciles the edit world to the new bytes |
| `Test_EditorProjection_OpenSceneEqualsActivation` | `ZenithEditor_Tests` | opening a scene gives the same edit world (`HashTickState`) as `Zenith_ActivateSceneData` of the resolved document into a fresh world |
| `Test_EditorWorlds_EditWorldFlagAndPurpose` | `ZenithEditor_Tests` | the main edit world and a prefab stage report `WORLD_PURPOSE_EDIT` through the attach context, carry the edit-world flag and origin `WORLD_ORIGIN_STARTUP`, and are never the input world; a preview world reports `WORLD_PURPOSE_PREVIEW` without the flag; a play world reports `WORLD_PURPOSE_PLAY` with origin `WORLD_ORIGIN_EDITOR_PLAY` |
| `Test_EditorProxies_ViewerAndAnchorFollowSceneView` | `ZenithEditor_Tests` | moving a scene view's camera moves its proxy at the next frame; the anchor moves to the newly focused view's proxy; a document's own anchor is not projected into the edit world; a reconcile leaves the proxies untouched; no document changes; a proxy is never picked or listed; each proxy's ID is the derived ID of Foundation.md "Editor proxy entities" and creating proxies draws nothing from the session's random source |
| `Test_EditorWorlds_AnimateToggleUsesEditorReason` | `ZenithEditor_Tests` | the toggle adds and removes `WORLD_PAUSE_REASON_EDITOR` and extraction still runs while paused |
| `Test_EditorWorlds_NoScriptOrPhysicsStepInEditWorld` | `ZenithEditor_Tests` | with Physics and a behaviour in the fixture, the edit world runs 60 ticks with no behaviour invocation and no physics step (counters) |
| `Test_EditorWorlds_PreviewPoolEvictsLRU` | `ZenithEditor_Tests` | opening `editor.preview.maxWorlds + 1` previews destroys the least recently shown world |

## Play tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorPlay_StateMachineTable` | `ZenithEditor_Tests` | every (state, event) pair of the play state machine table yields its next state and action; an unlisted pair is ignored and counted |
| `Test_EditorPlay_StartCapturesAndActivates` | `ZenithEditor_Tests` | Play captures the edit world, creates a play world with purpose `WORLD_PURPOSE_PLAY` and origin `WORLD_ORIGIN_EDITOR_PLAY`, activates the play copy and publishes `playStarted` at its first tick |
| `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` | `ZenithEditor_Tests` | Stage 3: after a time-of-day scrub, a realtime "capture now", 120 edit-world ticks and two proxies, the play world's `HashTickState` after its first tick equals a Play from the same documents in a fresh session, no proxy ID is in the play world, the copy statistics count the normalised sets, and the documents' bytes are unchanged; an injected projection drift is restored from the document and counted as drift |
| `Test_EditorPlay_CaptureExcludesWeatherScrub` | `ZenithEditor_Tests` | Stage 4: the same assertions after a weather change through ENV's weather commands |
| `Test_EditorPlay_CaptureExcludesInPlaceEffect` | `ZenithEditor_Tests` | Stage 5 (the `VFX` module added to EditorFixture): the same assertions after an effect is played in place in the edit world |
| `Test_EditorPlay_StepCancelledByDebugger` | `ZenithEditor_Tests` | a `DEBUGGER` reason applied during a 7-tick step clears the remaining count, leaves the world paused under both reasons and the play state PAUSED |
| `Test_EditorPlay_RefusedWhilePendingOrUnresolved` | `ZenithEditor_Tests` | Play while an asynchronous record is pending returns `EDITOR_ERROR_HISTORY_PENDING`; in a read-only session it starts; with an active scene that failed to resolve it returns `EDITOR_ERROR_PLAY_STATE` naming the document |
| `Test_EditorPlay_StartCooksPlaySet` | `ZenithEditor_Tests` | Play sends one `cook` request with the capture's asset GUIDs before creating the world |
| `Test_EditorPlay_StartFailureLeavesEditing` | `ZenithEditor_Tests` | a world-creation failure and an activation failure each return to EDITING with nothing created |
| `Test_EditorPlay_StopLeavesEditWorldUnchanged` | `ZenithEditor_Tests` | the edit world's `HashTickState` and every open document's canonical bytes are equal before Play and after Stop |
| `Test_EditorPlay_PauseUsesEditorReason` | `ZenithEditor_Tests` | Pause and Resume add and remove only `WORLD_PAUSE_REASON_EDITOR`; a gameplay pause stays applied after Resume |
| `Test_EditorPlay_StepRunsExactlyN` | `ZenithEditor_Tests` | Step 1 and Step 7 advance the tick index by exactly 1 and 7 with a frame time worth 3 ticks per frame, and Step 2 advances 2 ticks over two frames whose time is worth less than one tick each; after Resume no catch-up burst runs |
| `Test_EditorPlay_TwoWorldsIndependent` | `ZenithEditor_Tests` | the inputs of Foundation.md "Several play worlds": timeline T into world A only, edit E into world B only; after 120 ticks A's hash equals its solo run with T and no edit, B's equals its solo run with no input and E at the boundary after tick 30 (B is the session's second Play and each solo run a fresh session's first: the seed has no ordinal, Foundation.md "Starting play" step 4) |
| `Test_EditorPlay_MaxWorldsRefused` | `ZenithEditor_Tests` | starting a world beyond `editor.play.maxWorlds` fails with `EDITOR_ERROR_PLAY_WORLD_LIMIT` |
| `Test_EditorPlay_HaltedScriptStopsPlay` | `ZenithEditor_Tests` | a play world whose script world halts goes to STOPPING and the edit world is untouched |
| `Test_EditorPlay_ApplyToDocumentIsUndoable` | `ZenithEditor_Tests` | "Apply to document" writes the differing properties as one record, overrides on instance entities, and undo removes them |
| `Test_EditorPlay_HotReloadCatchUp` | `ZenithEditor_Tests` | a module compiled in process before Play is queued on the new play world's script world |
| `Test_EditorPlay_InputRoutingFollowsFocus` | `ZenithEditor_Tests` | focusing the game viewport sets the play world as the input world; another panel's focus sets none; the reserved chords alone reach the editor while focused |

## Shell and viewport tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorShell_ImGuiAdapterMapsInput` | `ZenithEditor_Tests` | a scripted `Zenith_InputFrame` (keys, text, mouse, wheel, focus) reaches `ImGuiIO` as the table of `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui platform adapter" states |
| `Test_EditorShell_ClaimantClaimsOutsideGameViewport` | `ZenithEditor_Tests` | presses outside a focused game viewport are claimed; inside are not; actions are claimed while ImGui wants the keyboard |
| `Test_EditorShell_DrawSnapshotTwoSlots` | `ZenithEditor_Tests` | the snapshot of frame k is intact while the editor writes frame k + 1, and slot reuse happens only after the iteration of frame k + 1 returned |
| `Test_EditorShell_DrawSnapshotCapDropsWholeLists` | `ZenithEditor_Tests` | a capture beyond `editor.shell.maxDrawBytes` drops whole draw lists from the end and counts them |
| `Test_EditorTextureTable_StaleReferenceDrawsChecker` | `ZenithEditor_Tests` | over the recording backend: a released reference whose slot was reclaimed and reused resolves to the checker, the new reference to the new texture |
| `Test_EditorTextureTable_ResizeWhileDrawnNeverAliases` | `ZenithEditor_Tests` | 200 frames of resizing a drawn viewport with the table over the recording backend `Zenith_EditorRecordingTextureBackend`: every resolve returns the reference's own texture or the checker, never a registration made after it or a recycled bindless index, and every backend `Destroy` comes at least two iterations after its release; a release in step 6 with `ulFirstFrameWithout = i + 1` is reclaimed no earlier than iteration i + 3 |
| `Test_EditorShell_ExtensionsFilterByViewID` | `ZenithEditor_Tests` | the ImGui renderer adds its pass to the shell view alone and the overlay extension to the snapshot's scene views alone, among a shell view, two scene views and a game view of `VIEW_KIND_EDITOR` |
| `Test_EditorTextureTable_AssetPinnedWhileReferenced` | `ZenithEditor_Tests` | `editor-render`: an asset texture registered for ImGui stays ACTIVE in the registry (`GetState`) until two frames after `Release`, and its handle is dropped at the reclaim |
| `Test_EditorShell_DisplayScaleRebuildsAtlas` | `ZenithEditor_Tests` | a display-scale change rebuilds the font atlas once and scales the style from the unscaled copy (no compounding over three changes) |
| `Test_EditorShell_ShortcutsDispatchCommands` | `ZenithEditor_Tests` | each default chord executes its command; a chord is not dispatched while a text field has focus |
| `Test_EditorViewports_ViewRequestMembers` | `ZenithEditor_Tests` | the staged request of a scene view, a game view and a preview carries the members of `Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views" |
| `Test_EditorViewports_ResizeSetsCameraCutOnce` | `ZenithEditor_Tests` | a panel resize recreates the output at the new even size and sets the cut bit for one frame |
| `Test_EditorViewports_HiddenViewportRequestsNothing` | `ZenithEditor_Tests` | a collapsed panel stages no request |
| `Test_EditorViewports_ExtractorWritesSnapshotRequests` | `ZenithEditor_Tests` | the `Editor.Extract` node writes exactly the staged requests of its world |

## Camera gizmo and snapping tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorCamera_LookUsesRawCounts` | `ZenithEditor_Tests` | in relative mode, a delta of 100 counts yaws by 15° at the default factor, independent of the points-based position delta |
| `Test_EditorCamera_PitchClamped` | `ZenithEditor_Tests` | pitch never exceeds ±89.5° |
| `Test_EditorCamera_FlySpeedScaling` | `ZenithEditor_Tests` | wheel notches scale the speed by 1.25 each within [0.05, 2,000]; `Shift` multiplies by the boost |
| `Test_EditorCamera_PanKeepsPointUnderCursor` | `ZenithEditor_Tests` | a pan of n pixels moves the camera so the pivot-depth point under the cursor stays under it, within 1e-4 m |
| `Test_EditorCamera_FrameSelectionDistance` | `ZenithEditor_Tests` | framing a sphere of radius 2 at 60° vertical and 16:9 gives `d = 2 / sin(30°) × 1.1`; a zero-radius sphere uses 1 m |
| `Test_EditorCamera_UnrealSchemeGestures` | `ZenithEditor_Tests` | the `unreal` scheme's left-drag moves and turns on the ground plane |
| `Test_EditorGizmoMath_AxisClosestPoint` | `ZenithEditor_Tests` | `s` and `t` of the axis drag against analytic cases, a ray parallel to the axis (no motion) and a closest point behind the camera (no motion) |
| `Test_EditorGizmoMath_PlaneDrag` | `ZenithEditor_Tests` | the plane hit against analytic cases; an edge-on plane gives no motion |
| `Test_EditorGizmoMath_RotateAccumulatesBeyond180` | `ZenithEditor_Tests` | a 270° drag accumulates to 270°, not −90° |
| `Test_EditorGizmoMath_EdgeOnRingStaysDraggable` | `ZenithEditor_Tests` | a ring seen edge-on uses the projected closest point and rotates |
| `Test_EditorGizmoMath_ScaleClampedAboveZero` | `ZenithEditor_Tests` | a drag through the pivot clamps the factor at 0.001 |
| `Test_EditorGizmoMath_HandleSizeConstantOnScreen` | `ZenithEditor_Tests` | the handle length in pixels is constant across depths in perspective and orthographic |
| `Test_EditorGizmoMath_HiddenAxisWhenParallel` | `ZenithEditor_Tests` | an axis with |a · v| > 0.995 and a plane with |n · v| < 0.06 are hidden |
| `Test_EditorGizmo_DragIsOneRecord` | `ZenithEditor_Tests` | a 90-frame drag produces one record whose undo restores the start transform exactly |
| `Test_EditorGizmo_LocalTransformUnderScaledParent` | `ZenithEditor_Tests` | dragging a child of a rotated, non-uniformly scaled parent keeps the world result within 1e-5 and writes the decomposed local transform |
| `Test_EditorGizmo_SingularParentRefused` | `ZenithEditor_Tests` | a parent with zero scale refuses the drag with the notification |
| `Test_EditorGizmo_RootsOnly` | `ZenithEditor_Tests` | a selected child of a selected parent moves once |
| `Test_EditorSnapping_GridAbsolute` | `ZenithEditor_Tests` | positions snap to `o + g × round((x − o)/g)` on the dragged axes only |
| `Test_EditorSnapping_GridRelative` | `ZenithEditor_Tests` | the delta snaps and an off-grid object keeps its offset |
| `Test_EditorSnapping_RotationExactMultiples` | `ZenithEditor_Tests` | the applied rotation is an exact multiple of the increment from the start orientation |
| `Test_EditorSnapping_ScaleFactor` | `ZenithEditor_Tests` | the factor snaps to multiples of the increment and never below it |
| `Test_EditorSnapping_SurfaceAndNormalAlign` | `ZenithEditor_Tests` | surface snapping places the pivot on the hit and aligns +Y to the normal; no hit falls back to the plane drag |
| `Test_EditorSnapping_VertexSnap` | `ZenithEditor_Tests` | the nearest source vertex lands on the nearest target vertex within 12 px; beyond 12 px the drag is unsnapped |
| `Test_EditorSnapping_CtrlInvertsToggle` | `ZenithEditor_Tests` | holding `Ctrl` snaps with snapping off and does not snap with it on |

## Picking tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorPicking_RayFromPixel` | `ZenithEditor_Tests` | the ray through pixel centres matches analytic rays for perspective and orthographic views |
| `Test_EditorPicking_SlabTest` | `ZenithEditor_Tests` | ray–AABB hits and misses including axis-parallel rays and a ray starting inside |
| `Test_EditorPicking_NearestPreciseHitWins` | `ZenithEditor_Tests` | an object whose bounds are nearer but whose triangles are missed loses to a farther precise hit |
| `Test_EditorPicking_MeshBvhMatchesBruteForce` | `ZenithEditor_Tests` | for 10,000 random rays the BVH closest hit equals a brute-force triangle loop over the decoded mesh |
| `Test_EditorPicking_PickMeshCacheLRU` | `ZenithEditor_Tests` | the cache evicts the least recently used mesh under its byte cap and decodes again on the next pick |
| `Test_EditorPicking_TerrainMarch` | `ZenithEditor_Tests` | the terrain ray march hits within 1 cm of the analytic surface of a fixture height field |
| `Test_EditorPicking_PrefabRootThenChild` | `ZenithEditor_Tests` | the first click selects the instance root, a second click within 500 ms the entity, `Alt` selects the entity at once |
| `Test_EditorPicking_MarqueeFrustum` | `ZenithEditor_Tests` | a marquee selects exactly the entities whose AABBs intersect its frustum; locked and hidden entities are skipped; `Shift` adds and `Alt` removes |
| `Test_EditorPicking_HandleBeforeObject` | `ZenithEditor_Tests` | a handle under the cursor wins over an object in front of it; ties prefer axes over planes over the centre |

## Panel tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorHierarchy_SiblingOrderFromDocument` | `ZenithEditor_Tests` | rows follow `ListChildrenInSiblingOrder`, not the world |
| `Test_EditorHierarchy_ReparentKeepsWorldTransform` | `ZenithEditor_Tests` | a drag reparent writes `SET_PARENT` and local-transform edits in one record so the world transform is unchanged; `Alt` keeps the local transform |
| `Test_EditorHierarchy_ReorderUsesSortKeyBetween` | `ZenithEditor_Tests` | a drop between two siblings rewrites only the dropped object's sort key |
| `Test_EditorHierarchy_InvalidDropRefused` | `ZenithEditor_Tests` | a drop onto a descendant, across documents or inside an instance is refused before any edit |
| `Test_EditorHierarchy_LargeSceneLazy` | `ZenithEditor_Tests` | a 100,000-entity document lists only expanded rows and draws the visible range alone (row count of calls) |
| `Test_EditorInspector_EditRecordsSetPropertyDiff` | `ZenithEditor_Tests` | editing a field records a `SET_PROPERTY` of the changed property alone, through a gathered copy |
| `Test_EditorInspector_DataDefinedAndCppAlike` | `ZenithEditor_Tests` | a data-defined component and a C++ component with the same fields produce the same rows |
| `Test_EditorInspector_RangeClamp` | `ZenithEditor_Tests` | a value typed beyond the declared range is clamped; a non-finite float is refused |
| `Test_EditorInspector_MultiEditOneTransaction` | `ZenithEditor_Tests` | editing a shared property of three entities is one record; a differing value shows the mixed marker |
| `Test_EditorInspector_OverrideRevertApply` | `ZenithEditor_Tests` | revert and apply-to-prefab on an instance property write the documented edits in one record each |
| `Test_EditorInspector_UnknownComponentReadOnly` | `ZenithEditor_Tests` | an unknown component shows read-only and can only be removed |
| `Test_EditorInspector_PlayModeEditsGoToPlayWorld` | `ZenithEditor_Tests` | an edit during play changes the play world and no document |
| `Test_EditorSchemaPanel_ValidationShowsMessages` | `ZenithEditor_Tests` | an invalid schema file shows the validator's messages with positions and applies nothing |
| `Test_EditorSchemaPanel_ApplyRefusedDuringPlay` | `ZenithEditor_Tests` | applying while a play world exists fails with `EDITOR_ERROR_SCHEMA_PLAY_ACTIVE`; applying while an asynchronous record is pending fails with `EDITOR_ERROR_HISTORY_PENDING`; applying while an open document of the plan is dirty fails with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` and writes nothing |
| `Test_EditorSchemaPanel_ApplyMigratesDocumentsAndWorld` | `ZenithEditor_Tests` | `z2-edit-cooked`: applying a v1 → v2 change migrates the documents (backup, journal), re-registers the schema in the window, migrates the edit world's rows and reconciles |
| `Test_EditorSchemaPanel_ApplyKeepsOpenDocumentHistory` | `ZenithEditor_Tests` | `z2-edit-cooked`: a schema apply while a migrated scene is open and clean with three earlier records, followed by the cook server's `changed` notification for the scene: the scene is not reloaded (no prune), its three records still undo, and `editor.documents.self_writes_ignored` counts the notification; `ApplySchemaRevision` runs in the frame after the window's result |
| `Test_EditorSchemaPanel_WindowRefusalRestores` | `ZenithEditor_Tests` | `z2-edit-cooked`: a schema the window refuses leaves the previous revision and restores the documents from the batch backup |
| `Test_EditorAssetBrowser_ListPagesConcatenated` | `ZenithEditor_Tests` | a three-page `list` result is shown in page order with the final count checked |
| `Test_EditorAssetBrowser_NotificationInvalidatesFolder` | `ZenithEditor_Tests` | a `changed` notification for a source invalidates exactly its folder's cached pages |
| `Test_EditorAssetBrowser_ImportCopiesNeverMoves` | `ZenithEditor_Tests` | a dropped file is copied into the folder with a unique name, its original untouched, and `assignGuids` then `import` are sent |
| `Test_EditorAssetBrowser_ImportUndoTrashes` | `ZenithEditor_Tests` | undo of an import moves the copied file and its `.meta` into the editor trash |
| `Test_EditorAssetInspector_SettingsApplyAndConflict` | `ZenithEditor_Tests` | `setSettings` with the record's digest applies as one record; `COOK_ERROR_META_CHANGED` refreshes the record and applies nothing |
| `Test_EditorPreviewHost_PreviewRequestsPerPanel` | `ZenithEditor_Tests` | each visible preview panel stages one `VIEW_KIND_PREVIEW` request; a hidden one none and its world pauses |
| `Test_EditorThumbnails_KeyInvalidatesOnReimport` | `ZenithEditor_Tests` | a reimport changes the cache key and the thumbnail is regenerated |
| `Test_EditorThumbnails_PerFrameCap` | `ZenithEditor_Tests` | at most `editor.thumbnails.perFrame` thumbnails render per frame |
| `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` | `ZenithEditor_Tests` | the governor's warning level frees pick meshes at once and marks decoded thumbnails and idle previews to 50% LRU, released in the same frame's step 6; critical frees and marks all (`Zenith2/Docs/Architecture.md` → "Memory governor"); `GetResidentBytes` counts marked entries until step 6; no world is destroyed inside the callback; the subsystem is `MEMORY_SUBSYSTEM_EDITOR_CACHES` (the CB8 case, ruling Q86 (b)) |
| `Test_EditorAssetBrowser_DependencyAndProvenanceViews` | `ZenithEditor_Tests` | the dependency view lists `dependencies` and `dependents` with edge kinds and the provenance view lists the records breadth-first with the depth marker, from canned results |
| `Test_EditorSchemaPanel_CommitLockRunsCli` | `ZenithEditor_Tests` | "Commit schema lock" launches `ZenithCook.exe schema-lock EditorFixture` (scripted child process), shows the new version on exit 0, reports "retry" on exit 6 with nothing changed, and records no history record |
| `Test_EditorBakePanel_CookRequestForOutputs` | `ZenithEditor_Tests` | "Bake" sends one `cook` request with the active scene set's bake-output GUIDs, shows its progress and diagnostics, and reloads the published outputs |
| `Test_EditorProfiler_ToggleAndProvider` | `ZenithEditor_Tests` | the toolbar toggle and `F3` both call `CycleState`; the `editor` provider publishes the frame's step times and the three slowest panels' draw times |
| `Test_EditorAudioHosting_PanelsReachSystem` | `ZenithEditor_Tests` | AUDIO's four panel types register through the extension interface, edit bank and mixer documents only through commands, and in play reach the running `Zenith_AudioSystem` through `FindModuleInstance` |
| `Test_EditorConsole_LuauLineUsesConsoleEvaluate` | `ZenithEditor_Tests` | a `>` line evaluates on the focused play world's script world through the editor's compile thunk |
| `Test_EditorMigrationPanel_ShowsPlanEntries` | `ZenithEditor_Tests` | the plan view shows every entry's versions, hunks, changed paths and warnings from a fixture plan |
| `Test_EditorProjectSettings_WritesThroughScaffold` | `ZenithEditor_Tests` | an apply writes the scaffold canonical writer's bytes and shows the rebuild message |
| `Test_NewProjectWizard_MatchesCli` | `ZenithEditor_Tests` | a scripted wizard session with the options of `ZenithNewGame create Harbour --template 3D --platforms windows` produces a directory byte-identical to the command line's (MOD's named test, EDIT's file, Stage 4) |
| `Test_NewProjectWizard_ShowsValidatorMessagesVerbatim` | `ZenithEditor_Tests` | an invalid name and an invalid module selection show the scaffold's messages unchanged (Z2-R-MOD-096) |
| `Test_EditorBindings_SavesThroughCanonicalWriter` | `ZenithEditor_Tests` | a bindings edit is saved with `Zenith_WriteBindingsJson`'s bytes |

## Pipeline tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorCookClient_ProtocolAgainstFakeServer` | `ZenithEditorCore_Tests` | paging, cancel of a queued and a running request, the duplicate-`id` refusal, an unknown request, EOF mid-request with restart, exit code 6 with the 2-second retry, a version mismatch |
| `Test_EditorCookClient_StateMachineTable` | `ZenithEditorCore_Tests` | every (state, event) pair of the client state machine yields its next state and action |
| `Test_EditorCookClient_BackoffAndFailed` | `ZenithEditorCore_Tests` | consecutive crashes back off 250, 500, 1,000 … 8,000 ms and five within 60 s reach FAILED |
| `Test_EditorCookClient_RequestQueueFullRejects` | `ZenithEditorCore_Tests` | a full request queue fails `Submit` with `EDITOR_ERROR_COOK_BUSY` |
| `Test_EditorCookClient_RealServerHandshake` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: the real `ZenithCook --serve` on the scratch fixture game answers `hello` and `list` and shuts down cleanly |
| `Test_EditorReimport_ReloadsActiveAssets` | `ZenithEditor_Tests` | a `reimport` result reaches the recording reload sink `Zenith_EditorRecordingReloadSink` (ToolDeclarations.md "Asset reload sink header") as `OnMountsChanged` when the catalogue changed and `RequestReload` for every changed GUID the sink reports active, in that order and for no inactive GUID, and invalidates the thumbnails and pick meshes of the changed GUIDs |
| `Test_EditorReimport_PublishedNotificationReloads` | `ZenithEditor_Tests` | a `published` notification reaches the recording reload sink the same way, unasked |
| `Test_EditorAssetOps_UndoSettingsServerConflict` | `ZenithEditor_Tests` | settings applied through the fake cook server commit asynchronously; after the `.meta`'s settings change behind it, the undo's fresh `record` fails the settings comparison: no `setSettings` is sent, the record becomes a fence "settings changed on disk; undo not possible" and the `.meta` is unchanged; a `setSettings` answered `COOK_ERROR_META_CHANGED` (a change between the `record` and the write) fences the same way |
| `Test_EditorAssetOps_SettingsUndoAfterSubAssetRewrite` | `ZenithEditor_Tests` | the fake server's scripted reimport after a settings apply rewrites the `.meta`'s `subAssets` (one sub-asset added, the digest changed); the undo's fresh `record` passes the settings comparison, its `setSettings` carries that `record`'s digest, and the undo commits with the old settings restored; the redo does the same with the new settings (Foundation.md "Asynchronous changes") |
| `Test_EditorAssetOps_SettingsCancelledAfterWriteCommits` | `ZenithEditor_Tests` | the fake server applies a `setSettings` then answers `COOK_ERROR_CANCELLED`; a `record` showing the new settings resolves `COMPLETED` and the record commits with the recorded digest; one showing the old settings resolves `NO_EFFECT` and nothing is recorded; one showing a third writer's settings resolves `CHANGED_ELSEWHERE`: nothing is recorded (execute), or the record becomes a fence (undo); with the resolution's own `record` failing (`UNKNOWN`), the re-resolution's fresh `record` showing the new settings sends `setSettings` back to the old settings with that digest, one showing the old settings sends nothing, and one showing a third writer's settings sends nothing and leaves the "incomplete" fence naming the `.meta` |
| `Test_EditorAssetOps_RenameWithDirtyReferencerRefused` | `ZenithEditor_Tests` | a prefab rename with a referencing scene open and dirty fails with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` naming it, moves no file and records nothing; with the scene open and clean it succeeds, the scene's in-memory `hint` equals its file's, the scene stays CLEAN and its earlier records still undo |
| `Test_EditorAssetOps_RenameServerRestartCompensates` | `ZenithEditor_Tests` | the fake server dies during the rename's `assignGuids`: the change resolves `PARTIAL` at once (its local part applied; `editor.undo.resolutions` counts one partial and no no-effect), both files are back at their old names, the hints are restored, `assignGuids` of the old paths is sent once the server is back, `historyFailed` is published and nothing is recorded |
| `Test_EditorAssetOps_RenameTakesWriterLock` | `ZenithEditor_Tests` | with the root's writer lock held by another process, a rename fails with `EDITOR_ERROR_STORAGE_LOCKED` naming the holder and moves nothing; with it free, both moves happen inside one acquire |
| `Test_EditorAssetOps_RenameRepairsHints` | `ZenithEditor_Tests` | a prefab rename rewrites every instance `hint` in one batch and keeps every GUID; undo restores the names and the hints |
| `Test_EditorAssetOps_RenameFailureRollsBack` | `ZenithEditor_Tests` | a failure at each of the operation's steps leaves both files at their original names and records nothing |
| `Test_EditorAssetOps_DeleteMovesToTrash` | `ZenithEditor_Tests` | delete moves the source and `.meta` into the trash with `trash.json`; undo moves them back; references are repaired per the chosen option |
| `Test_EditorAssetOps_DuplicateGetsFreshIDs` | `ZenithEditor_Tests` | a duplicated scene has a fresh document ID and the server assigns a new GUID |
| `Test_EditorAssetOps_RedoCreateKeepsGuid` | `ZenithEditor_Tests` | Stage 4: a created document, undone and redone through the fake cook server: the undo moves the file and its `.meta` into the trash directory the record names; the redo moves both back (their `trash.json` entries removed), its `assignGuids` creates nothing, and the GUID equals the first one (`m_axCreatedAssets`); the same holds for an import and a duplicate; with the trashed `.meta` replaced by one carrying another GUID, the redo fails permanently and the record becomes a fence (Foundation.md "Asynchronous changes") |
| `Test_EditorAssetOps_LargeBatchUsesBatchOperation` | `ZenithEditor_Tests` | a folder move whose referencers exceed `editor.assetOps.batchThreshold`, with one open clean referencing scene, is one record holding `Zenith_EditorContentBatchOperation` over CONTENT's recording channel; the open scene ends CLEAN with the repaired hints and its history intact; undo runs `Rollback` and restores every file's bytes |
| `Test_EditorDocuments_RenameKeepsIdentity` | `ZenithEditor_Tests` | a rename keeps the document ID, its selection references and its history |

## Hot reload tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorScriptReload_CompileErrorToDiagnostics` | `ZenithEditor_Tests` | a syntax error's message, file and line reach the diagnostics panel and no reload is queued |
| `Test_EditorScriptReload_StatelessRejectionToDiagnostics` | `ZenithEditor_Tests` | a chunk-system module failing the stateless check is not queued and its diagnostic is shown |
| `Test_EditorScriptReload_QueuesOnEveryPlayWorld` | `ZenithEditor_Tests` | accepted bytecode is queued with `RequestHotReload` on each play world and not on the edit world |
| `Test_EditorScriptReload_BytecodeEqualsCooked` | `ZenithEditor_Tests` | the in-process bytecode equals the cooked `.zlbc` payload for the same source |
| `Test_EditorScriptReload_DefinitionsRegenerated` | `ZenithEditor_Tests` | a registry change rewrites the `.d.luau` definitions under `${ZENITH_OUT}/Generated/Luau/<Name>/` and writes nothing under the game directory |
| `Test_EditorShaderReload_AppliesInSeqOrder` | `ZenithEditor_Tests` | journal records appended out of file-chunk order are applied in `seq` order; a gap re-reads from the start without re-applying |
| `Test_EditorShaderReload_LayoutChangedEngineProgram` | `ZenithEditor_Tests` | an engine program's `layout-changed` record keeps the old container and shows the rebuild message |
| `Test_EditorShaderReload_LayoutChangedMaterialRecooks` | `ZenithEditor_Tests` | a material program's `layout-changed` record sends a `cook` of every loaded material of the type, then reloads |
| `Test_EditorShaderReload_ErrorRecordKeepsOld` | `ZenithEditor_Tests` | an `error` record keeps the old container and lists each error |
| `Test_EditorShaderReload_PartialLineWaits` | `ZenithEditor_Tests` | a record without its LF is applied only once the line is complete |

## Migration and recovery tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorMigration_PlanOnOpenIsReadOnly` | `ZenithEditorCore_Tests` | planning every root writes nothing (no storage mutation) |
| `Test_EditorMigration_BlockedPlanRefused` | `ZenithEditorCore_Tests` | a plan holding a blocked entry cannot be applied and lists the blocking errors first |
| `Test_EditorMigration_ApplyBacksUpThenWrites` | `ZenithEditorCore_Tests` | applying writes the backup batch before any document, then the documents, in one journaled batch per root; the record's undo restores the old bytes in the session |
| `Test_EditorMigration_MetaThroughServerToken` | `ZenithEditorCore_Tests` | `.meta` entries apply through `migrateApply` with the preview token; a changed token re-plans |
| `Test_EditorMigration_MetaPartialApplyCompensates` | `ZenithEditorCore_Tests` | a `migrateApply` of three entries cancelled after the second item: `migratePreview` shows two applied, the resolution is `PARTIAL`, the compensation writes those two entries' backup bytes with their post-migration digests expected, and the three `.meta` files end at their original bytes with nothing recorded |
| `Test_EditorMigration_StalePlanReplans` | `ZenithEditorCore_Tests` | a document changed after planning makes the apply fail stale and re-plan |
| `Test_EditorMigration_ResumeAfterKillAtEachJournalState` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: a child editor applying a migration is killed at each journal state; the next session's recovery report and the documents' bytes match SCENE's recovery table |
| `Test_EditorMigration_CorruptJournalMakesRootReadOnly` | `ZenithEditorCore_Tests` | a corrupt journal makes its root read-only with the journal named, and other roots stay writable |
| `Test_EditorMigration_RestoreBackupBatch` | `ZenithEditorCore_Tests` | "Restore this batch" writes the backup bytes back as one record with the current digests expected |
| `Test_EditorSchema_UndoRestoresDocumentsAndRows` | `ZenithEditor_Tests` | `z2-edit-cooked`: undoing a schema change restores the schema file and the documents' pre-apply bytes, restarts the runtime, and the edit world equals a fresh projection under the old schema; with the schema file open in the JSON editor panel and two earlier JSON edits of it recorded, its open document takes the record's "before" state ID and equals its file, and both earlier edits still undo |
| `Test_EditorAutosave_WritesDirtyOnly` | `ZenithEditor_Tests` | an autosave round writes one file per dirty document whose state changed since its last autosave and none for clean ones |
| `Test_EditorAutosave_FileRoundTrip` | `ZenithEditor_Tests` | a written `.zautosave` reads back the same entries, digests and state ID |
| `Test_EditorAutosave_DamagedFileRejected` | `ZenithEditor_Tests` | a truncated file, a wrong magic, a newer version and a wrong digest are each treated as damaged |
| `Test_EditorAutosave_QueueFullSkips` | `ZenithEditor_Tests` | a full hand-off queue skips the document for the round and counts it |
| `Test_EditorAutosave_SaveDeletesAutosave` | `ZenithEditor_Tests` | a successful save deletes the document's autosave and manifest entry |
| `Test_EditorSessionLock_SecondEditorRefused` | `ZenithEditor_Tests` | `z2-edit-cooked`: a second session on the same game exits with code 1 naming the holder from the lock line |
| `Test_EditorRecovery_RestoreAfterCrash` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: after a child editor is terminated with autosaves on disk, `--recovery restore` restores each document's bytes as a dirty, undoable state and leaves the files unchanged |
| `Test_EditorRecovery_ChangedFileRestoresAsCopy` | `ZenithEditor_Tests` | `z2-edit-cooked`: a file changed between the sessions is never overwritten; the copy `<name>.recovered.<ext>` is written |
| `Test_EditorRecovery_DiscardKeepsUntilCleanExit` | `ZenithEditor_Tests` | discard moves the autosaves to `discarded/` and the next clean shutdown deletes them |
| `Test_EditorFormats_SessionFileRoundTrip` | `ZenithEditor_Tests` | a written session file reads back to equal state |
| `Test_EditorFormats_NewerVersionUsesDefaults` | `ZenithEditor_Tests` | a version 2 session, layout and preferences file is not read, defaults are used, and the file is not overwritten until the state changes |
| `Test_EditorFormats_UnknownMembersKept` | `ZenithEditor_Tests` | unknown members of a version 1 file survive a rewrite in position |
| `Test_EditorFormats_ThumbnailFileRoundTrip` | `ZenithEditor_Tests` | a written thumbnail reads back the same pixels; a damaged one is deleted |

## Source control tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorSourceControl_ParsesPorcelainV2` | `ZenithEditor_Tests` | status lines for modified, added, deleted, renamed, untracked and conflicted entries parse to their states |
| `Test_EditorSourceControl_NoRepositoryHidesBadges` | `ZenithEditor_Tests` | a game outside a repository shows "Not under source control" and runs no further command |
| `Test_EditorSourceControl_DiffCanonicalisesHead` | `ZenithEditor_Tests` | the diff of a document whose HEAD bytes are non-canonical shows only semantic changes |
| `Test_EditorSourceControl_RevertIsUndoable` | `ZenithEditor_Tests` | reverting a document writes the HEAD bytes through the migrator as one record whose undo restores the working bytes; reverting an open dirty document (after the confirmation) leaves it CLEAN with the HEAD bytes, is not refused by the per-document rule, and its undo restores the file's pre-revert bytes and the in-memory unsaved state with its old state ID, dirty again (Foundation.md "Editor-originated writes", "The revert exception") |
| `Test_EditorSourceControl_CommitRefusedWhileDirty` | `ZenithEditor_Tests` | a commit with a staged dirty document is refused |
| `Test_EditorSourceControl_CommitUsesIndex` | `ZenithEditor_Tests` | with two modified files of which one is staged, the commit holds the staged file alone, with its staged bytes (no pathspec) |
| `Test_EditorSourceControl_ConflictedDocumentOpensConflicted` | `ZenithEditor_Tests` | a document with conflict markers opens CONFLICTED with the three resolution options |
| `Test_EditorSourceControl_TimeoutTerminatesGit` | `ZenithEditor_Tests` | a `git` command exceeding 30 s is terminated and fails with `EDITOR_ERROR_SOURCE_CONTROL` |
| `Test_EditorSourceControl_ArgumentsNeverShellInterpreted` | `ZenithEditor_Tests` | a path containing spaces, quotes and `&` reaches `git` as one argument |

## Automation tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorAutomation_ValidatesWholeScriptFirst` | `ZenithEditor_Tests` | a script with an unknown step kind, a missing member or a wrong value kind fails before its first step runs |
| `Test_EditorAutomation_OneStepPerFrame` | `ZenithEditor_Tests` | steps run one per frame at most, each after the previous completed |
| `Test_EditorAutomation_WaitEventTimeout` | `ZenithEditor_Tests` | a wait whose event never arrives fails at its timeout with `EDITOR_ERROR_AUTOMATION_TIMEOUT` |
| `Test_EditorAutomation_AssertionFailureExitsFive` | `ZenithEditor_Tests` | a failed assertion with `--exit-after-script` ends with exit code 5 and writes expected and actual values to the report |
| `Test_EditorAutomation_WriteFileOutsideGameRefused` | `ZenithEditor_Tests` | a `writeFile` path outside the game directory and the fixture root is refused at validation |
| `Test_EditorAutomation_ReportFormat` | `ZenithEditor_Tests` | the report holds every member of `Zenith2/Docs/Design/EDIT/Automation.md` → "Automation report" |
| `Test_EditorAutomation_MeasurementsUseEvents` | `ZenithEditor_Tests` | `measureEnd` of each operation records the start and the documented end event |
| `Test_EditorCommands_ArgsValidated` | `ZenithEditor_Tests` | an unknown, missing or wrongly typed argument fails with `EDITOR_ERROR_COMMAND_ARGS` naming it |
| `Test_EditorCommands_DumpListsEveryCommand` | `ZenithEditor_Tests` | `editor.commands.dump` lists every registered command with its flags and arguments |
| `Test_EditorLiveLink_LoopbackOnly` | `ZenithEditor_Tests` | the endpoint binds `127.0.0.1` alone |
| `Test_EditorLiveLink_WrongTokenRejected` | `ZenithEditor_Tests` | a wrong or missing token closes the connection; five rejections in 10 s close the endpoint for 60 s |
| `Test_EditorLiveLink_StepsAndEvents` | `ZenithEditor_Tests` | an authenticated client runs steps with results and receives subscribed events |
| `Test_EditorLiveLink_BusyWithScript` | `ZenithEditor_Tests` | a client connecting while a script runs is refused with `EDITOR_ERROR_AUTOMATION_BUSY` |

## Authoring tool tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorGraphCanvas_ConnectRefusesIncompatible` | `ZenithEditor_Tests` | dragging between incompatible pins is refused with the model's reason and records nothing |
| `Test_EditorGraphCanvas_PasteRemapsNodeIDs` | `ZenithEditor_Tests` | pasted nodes get fresh IDs with their internal links remapped |
| `Test_EditorTimeline_SnapsToFramesAndKeys` | `ZenithEditor_Tests` | key drags snap to frames and to keys within 6 px |
| `Test_EditorCurveEditor_AutoTangents` | `ZenithEditor_Tests` | auto tangents equal the Catmull-Rom formula |
| `Test_EditorJsonDocumentEditor_ValidatorRejectsChange` | `ZenithEditor_Tests` | a change the type's validator rejects fails the command and leaves the document unchanged |
| `Test_EditorPrefab_CreateFromSelectionKeepsReferences` | `ZenithEditor_Tests` | creating a prefab replaces the selection by an instance and every outside reference to the selected entities still resolves |
| `Test_EditorPrefab_VariantOverridesOnly` | `ZenithEditor_Tests` | editing in a variant's stage writes overrides into the variant alone |
| `Test_EditorPrefab_SaveReconcilesInstances` | `ZenithEditor_Tests` | saving a prefab reconciles every instance in the main edit world |
| `Test_EditorPrefab_ApplyToReadOnlyRefused` | `ZenithEditor_Tests` | apply-to-prefab on a read-only prefab is refused before any edit |
| `Test_EditorTerrain_DabSpacing` | `ZenithEditor_Tests` | a drag along a straight 10 m path with radius 4 emits dabs every 1 m |
| `Test_EditorTerrain_StrokeIsOneRecordOfSegments` | `ZenithEditor_Tests` | a 30-frame stroke is one record of 30 terrain edits; undo reverts newest first and the document bytes equal the pre-stroke bytes |
| `Test_EditorTerrain_UndoChainNeverStale` | `ZenithEditor_Tests` | 200 random strokes, undos and redos never produce `ENV_ERROR_EDIT_STALE` |
| `Test_EditorTerrain_OversizeStrokeFences` | `ZenithEditor_Tests` | a stroke whose edits exceed `editor.undo.maxTerrainRecordBytes` becomes a fence |
| `Test_EditorTerrain_PreviewCallSites` | `ZenithEditor_Tests` | `QueueRegions` runs after each stroke frame, `Prepare` in `OnPrologueEnd`, `WriteRecords` in the extractor |
| `Test_EditorTerrain_RegenerationAtStrokeEnd` | `ZenithEditor_Tests` | regeneration runs once at stroke end over the stroke's regions, and a new stroke cancels and restarts it for the union |
| `Test_EditorTerrain_StrokeDeterministic` | `ZenithEditor_Tests` | the same automation stroke applied to two copies of a document gives identical bytes |
| `Test_EditorMaterial_LiveParametersAndValidate` | `ZenithEditor_Tests` | a parameter edit is a JSON edit applied live with `ReplaceParameters` and validated through the server's `validate` |
| `Test_EditorMaterial_UndoRestoresLiveParameters` | `ZenithEditor_Tests` | `editor-render`: the material preview of a fixture material whose `baseColour` is edited from (1, 0, 0) to (0, 0, 1), read back after the edit, the undo and the redo: the edited image differs from the original (the centre pixel's blue exceeds its red), the image after the undo equals the original byte for byte, the image after the redo equals the edited one (in-run comparisons) |
| `Test_EditorMaterial_PackingMatchesReflection` | `ZenithEditor_Tests` | `Zenith_EditorPackMaterialParameters` of a fixture document against a fixture reflection equals bytes written by hand from the reflection's offsets and kinds; texture fields are zero |
| `Test_EditorTerrain_WizardOneRecordPlacesImportedTerrain` | `ZenithEditor_Tests` | the terrain creation wizard against the fake cook server is one record of a create change and a deferred scene operation; the placed entity's terrain component references the GUID the fake `assignGuids` returned; undo leaves the scene's bytes and the folder as before; redo then moves the terrain files back from the trash and places the entity again, its terrain component referencing a GUID equal to the first, which the fake server's `record` resolves |
| `Test_EditorPhysics_DropToRestOneRecord` | `ZenithEditor_Tests` | `z2-edit-cooked`: the fixture box dropped onto the floor is one record whose undo restores its pose, resting within 1 cm of the floor plus its half extent; a selection without dynamic bodies leaves the command disabled; a refused sandbox records nothing (Stage 4, row ED34) |
| `Test_EditorPhysics_RagdollPreviewSteps` | `ZenithEditor_Tests` | `Tests/Physics/` (PHYS-ticketed, Stage 4): the ragdoll preview panel steps the ragdoll in PHYS's isolated simulation each visible frame, poses the preview world's skinned mesh, and edits the ragdoll document only through commands |
| `Test_EditorAnimation_GraphEditCompilesAndPreviews` | `ZenithEditor_Tests` | a graph document edit compiles in memory, shows the compiler's diagnostics on nodes, and drives the preview |
| `Test_EditorAnimation_NodePositionsInEditorMember` | `ZenithEditor_Tests` | moving a node writes the node's `editor` member and no unknown member |
| `Test_EditorUI_ModelOperationsThroughHistory` | `ZenithEditor_Tests` | each UI model operation is one record whose undo applies the model's inverse |
| `Test_EditorLighting_CaptureNowBakedIsRecorded` | `ZenithEditor_Tests` | "Capture now" on a baked probe increments `CaptureRequest` as an undoable edit; on a realtime probe it is edit-world-only |
| `Test_EditorEnvironment_TimeOfDayNotRecorded` | `ZenithEditor_Tests` | time-of-day scrubbing changes the edit world and records nothing |

## Headless editing tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorHeadless_OpenEditSaveRoundTrip` | `ZenithEditor_Tests` | `z2-edit-cooked`: a headless child session (`--headless --automation-script --random-seed 1`) opens the fixture scene, creates entities with explicit `id` arguments, moves, reparents and deletes entities, saves and exits 0; the saved bytes equal those of the same edits applied in process through SCENE's edit API and canonical writer to the same fixture scene (an in-run comparison, no committed golden) |
| `Test_EditorHeadless_SavedBytesEqualWindowed` | `ZenithEditor_Tests` | `editor-render`: the same script, seeded with `--random-seed 1`, run headless and windowed (two scene views, so two proxies) on WARP saves byte-identical documents, including the IDs of entities the script created without an `id` argument |
| `Test_EditorHeadless_ScenePublishFromHeadlessBoot` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: a headless child editor on a scratch copy of `EditorFixture` authors `Scenes/Authored.scene.json` from an empty document through commands (entities, components, a prefab instance, terrain strokes) and saves it; `ZenithCook.exe --game EditorFixture --game-root <copy> --platform windows --out <copy>/Cooked` cooks it; the registered game's Player `EditorFixture.exe --headless --assets-root <copy>/Cooked --set content.startupScene=Scenes/Authored.scene.json --exit-after-ticks 2` exits 0; and an in-process runtime (`Zenith_RuntimeToolAccess::Create` with the fixture's composition and the same `--assets-root`) loads the cooked scene into a play-purpose world through its `Zenith_SceneLoader` with the expected entity count |
| `Test_EditorHeadless_PickingUsesCpuPath` | `ZenithEditor_Tests` | headless picking returns the CPU picker's hits |
| `Test_EditorHeadless_PlayRunsTicks` | `ZenithEditor_Tests` | headless Play creates a play world that runs 300 ticks and Stop leaves the edit world unchanged |
| `Test_EditorHeadless_PreviewsReportNoRenderer` | `ZenithEditor_Tests` | thumbnails and previews report "no renderer" and stage no request |
| `Test_EditorHeadless_UndoRoundTripScript` | `ZenithEditor_Tests` | a script that executes 50 commands, undoes all, redoes all and asserts the document digests after each phase passes |

## Render tests

Render tests run a hidden-window editor (`Zenith_ToolRuntimeDesc::m_bHiddenWindow`) on WARP and on lavapipe and assert structural properties of read-back images, never golden images (in-run comparisons and analytic expectations, ruling Q55; a worker cannot record goldens), each in `Tests/Zenith_EditorRender_Tests.cpp` in the doctest suites `z2-stress` and `editor-render` (§ "Suites and gate steps"):

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorRender_ShellCompositeOnly` | `ZenithEditor_Tests` | the shell view's output equals the background colour outside the ImGui windows and runs no scene pass (the frame dump lists no scene pass for it) |
| `Test_EditorRender_ViewportTextureSampledByShell` | `ZenithEditor_Tests` | a scene view's output texture is drawn into the shell at its panel rectangle (a fixture's red quad appears at the expected pixels) |
| `Test_EditorRender_GridLinesAtIncrement` | `ZenithEditor_Tests` | with snap 1 m and a top orthographic view of known extent, grid line pixels appear at the expected columns and rows within 1 px |
| `Test_EditorRender_SelectionOutlineAroundSelected` | `ZenithEditor_Tests` | the outline pixels form a closed band around the selected fixture cube's projected silhouette and none around the unselected one |
| `Test_EditorRender_GizmoAxisColours` | `ZenithEditor_Tests` | the translate gizmo's axis pixels carry the three axis colours at their projected directions |
| `Test_EditorRender_TwoViewportsIndependentHistories` | `ZenithEditor_Tests` | two scene views of one world keep separate TAA histories: moving one camera changes only that view's output |
| `Test_EditorRender_StaleTextureShowsChecker` | `ZenithEditor_Tests` | an image drawn through a released texture reference shows the checker pattern |
| `Test_EditorRender_SyncValidationClean` | `ZenithEditor_Tests` | the editor frames above run with Vulkan synchronization validation (lavapipe) and D3D12 GPU-based validation (WARP) clean |

## Integration tests

| Test ID | Executable | Asserts |
|---|---|---|
| `Test_EditorIntegration_SchemaChangeEndToEnd` | `ZenithEditor_Tests` | `z2-edit-cooked`: the schema panel flow on the fixture game from validation to the edit world's migrated rows and the lock commit |
| `Test_EditorIntegration_ReimportEndToEnd` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: with the real cook server, changing a fixture `.glb` updates the edit world's mesh and publishes `reimportVisible`; a settings change of `Crate.glb` that adds a sub-asset (its reimport rewrites the `.meta`'s `subAssets`), then undo, commits the undo and restores the old settings and sub-asset set (Foundation.md "Asynchronous changes", the settings comparison) |
| `Test_EditorIntegration_ScriptReloadEndToEnd` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: during play, a `writeFile` of a changed module publishes `scriptReloadLive` and the behaviour's new constant is observed |
| `Test_EditorIntegration_MigrationUIEndToEnd` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`: the older fixture game (the scratch copy with `Schemas/Fixture.Stats.schema.json` replaced by `Tests/Fixtures/Schemas/Fixture.Stats.v2.schema.json` and its chain, so every document holding `Fixture.Stats` v1 rows needs migration) opens, plans, applies with backup, and an apply killed at a journal state resumes at the next start |
| `Test_Walkthrough_BuildScene_Rehearsal` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`, Stage 4: the "build a scene" rehearsal script passes its check; fails naming `OpenWorldSlice` when the scratch root holds no copy of it |
| `Test_Walkthrough_PrefabVariant_Rehearsal` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`, Stage 4: the "author a prefab variant" rehearsal passes its check; fails naming `OpenWorldSlice` when the scratch root holds no copy of it |
| `Test_Walkthrough_IterateScript_Rehearsal` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`, Stage 4: the "iterate a script" rehearsal passes its check; fails naming `OpenWorldSlice` when the scratch root holds no copy of it |
| `Test_Walkthrough_ReimportAsset_Rehearsal` | `ZenithEditor_Tests` | `z2-stress`, `z2-edit-cooked`, Stage 4: the "reimport an asset" rehearsal passes its check; fails naming `OpenWorldSlice` when the scratch root holds no copy of it |

## Benchmarks and budget runs

- EDIT has no Stage 1 measurement harness: its Stage 1 producers ("Stage 1 W-Editor row ticket for <operation>", Z2T-EDIT-002 to -006, and "Stage 1 per-system budget ticket for EDIT", Z2T-EDIT-001) cite reference-engine or published data (`Zenith2/Docs/Budgets.md` → "Per-system producer shape"), and the editor that the rows measure exists from Stage 3.
- From Stage 3 the W-Editor runs are `needs-human` measurement tickets on the reference desktop that execute the committed W-Editor automation script (`Zenith2/Docs/Design/EDIT/Automation.md` → "Measurement procedure") five times per operation and commit the reports under `Zenith2/Docs/Research/Budgets/EDIT/`.
- The walkthroughs are `needs-human` Stage 4 acceptance runs (`Zenith2/Docs/Design/EDIT/Automation.md` → "Usability walkthroughs").
