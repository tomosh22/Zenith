# EDIT — editor and collaboration

The entry document of the EDIT design family. EDIT is `ZenithEditor`, the Windows host tool in which games are authored, with its tool library `ZenithEditorCore` (ruling Q86 (a)) and the test game `Zenith2/Games/EditorFixture/`: the editor foundation (stable authoring identities through SCENE, the command system, undo transactions, document ownership, selection), the edit, play and preview worlds over `ZenithRuntime`, play in editor, the shell (ImGui on PLAT and Flux), viewports, cameras, picking, gizmos and snapping, the panels and the static extension interface, the asset browser and reimport over ASSET's cook server, previews and thumbnails, hot reload, the authoring-document migration UI, autosave and crash recovery, source control, merge-friendly files, automation and the live link, the authoring tools EDIT builds over other areas' documents, and the usability walkthroughs. The requirement catalogue is `Zenith2/Docs/Requirements/EDIT.md` (Stages 1 to 3, the coverage and the exclusions) with `Zenith2/Docs/Requirements/EDIT/Stage4Plus.md` (Stages 4 to 7).

## Purpose and scope

### What EDIT owns

| Subject | Where it is specified |
|---|---|
| the editor process, its composition, the runtime tool access it uses, the session, the world model, play in editor, documents, commands and undo (asynchronous changes included), selection, the clipboard | `Zenith2/Docs/Design/EDIT/Foundation.md` |
| the declarations of the `ZenithEditorCore` headers and of the `ZenithEditor` internal headers | `Zenith2/Docs/Design/EDIT/Declarations.md`, `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` |
| the shell, viewports and views, cameras, picking, gizmos, snapping, overlays | `Zenith2/Docs/Design/EDIT/Viewports.md` |
| the panel framework, the extension interface, every generic panel, previews and thumbnails | `Zenith2/Docs/Design/EDIT/Panels.md` |
| the cook client, reimport, asset operations, hot reload, the schema change, the migration flow, autosave, recovery, source control, merge-friendly files | `Zenith2/Docs/Design/EDIT/Pipeline.md` |
| the reusable authoring widgets, prefab editing, terrain, environment, lighting, material, VFX, animation, UI and audio tools, and tools hosted for other areas | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` |
| every editor-defined file format | `Zenith2/Docs/Design/EDIT/Formats.md` |
| the three targets (`ZenithAuthoring`'s contract, each owner's functions being its own; `ZenithEditorCore`; `ZenithEditor`), their dependency growth, and the test game `Zenith2/Games/EditorFixture/` | § "Library and file layout", § "Dependencies" |
| the command registry, automation scripts, the live link, operation timing, the walkthroughs | `Zenith2/Docs/Design/EDIT/Automation.md` |
| every EDIT test | `Zenith2/Docs/Design/EDIT/Tests.md` |

### Explicit exclusions

| Excluded | Decision | Where it lives instead |
|---|---|---|
| dynamic plugins and third-party editor extensions | Z2-D-024, Z2-D-075 | static extensions compiled into the editor (`Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface") |
| node-graph material and VFX editors | Z2-D-079 | materials and VFX are code with previews and inspectors (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Material tools", "VFX tools") |
| an editor for Android, or any shipping editor | Z2-D-010 | the editor is a `ZENITH_TOOLS_HOST` tool; content reaches devices through the cook |
| documents, their schemas, migration chains and operations | Z2-D-048 | their owners (SCENE's framework and documents; ANIM, ENV, UI, AUDIO, GEOM, LIGHT, PLAT, MOD documents) |
| the importers, cook steps and asset database | Z2-D-038 | ASSET (`Zenith2/Tools/ZenithCook`), reached only through the cook server |
| the on-screen profiler, the console command registry | Z2-D-005 | DEVT; EDIT hosts the editor overlay toggle and provider |
| the semantic three-way merge driver, recoverable batch operations | — | CONTENT (Z2-I-CONTENT-16 to -21, -27 to -29) |
| world partitions, bake orchestration | — | WORLD |
| local multiplayer and split-screen | Z2-D-076 | not provided; several play worlds are separate worlds, not split screens |

### Inventory items covered

| Item | Subject | Stage | Specified in |
|---|---|---|---|
| Z2-I-EDIT-01 | undo/redo transactions | S3 | Foundation.md "Commands and history" |
| Z2-I-EDIT-02 | selection | S3 | Foundation.md "Selection"; Viewports.md "Picking" |
| Z2-I-EDIT-03 | gizmos | S3 | Viewports.md "Gizmos" |
| Z2-I-EDIT-04 | snapping | S3 | Viewports.md "Snapping" |
| Z2-I-EDIT-05 | asset browser | S3 | Panels.md "Asset browser"; Pipeline.md "Cook client", "Reimport", "Asset operations" |
| Z2-I-EDIT-06 | previews | S3 | Panels.md "Previews and thumbnails" |
| Z2-I-EDIT-07 | prefab editing | S4 | AuthoringTools.md "Prefab editing" |
| Z2-I-EDIT-08 | autosave | S4 | Pipeline.md "Autosave" |
| Z2-I-EDIT-09 | recovery | S4 | Pipeline.md "Session lock and crash detection", "Recovery on startup" |
| Z2-I-EDIT-10 | multi-world play | S3 | Foundation.md "Worlds in the editor", "Play in editor" |
| Z2-I-EDIT-11 | source-control integration | S4 | Pipeline.md "Source control" |
| Z2-I-EDIT-12 | merge-friendly files | S4 | Pipeline.md "Merge-friendly files"; Formats.md |
| Z2-I-EDIT-13 | automation APIs | S4 | Automation.md |
| Z2-I-EDIT-14 | extension APIs | S4 | Panels.md "Panel framework" |
| Z2-I-EDIT-15 | migration UI: upgrade preview | S4 | Pipeline.md "Migration flow"; Panels.md "Migration panel" |
| Z2-I-EDIT-16 | migration UI: backup | S4 | Pipeline.md "Applying a migration", "Restoring a backup" |
| Z2-I-EDIT-17 | migration UI: resume | S4 | Pipeline.md "Resuming after an interruption" |
| Z2-I-EDIT-18 … 21 | walkthroughs: build a scene, author a prefab variant, iterate a script, reimport an asset | S4 | Automation.md "Usability walkthroughs" |

EDIT also owns the tools of other areas' visual-authoring items (Z2-D-079): Z2-I-ENV-01, -02 and -03 (terrain sculpting, terrain painting and foliage placement, S3 and S4), Z2-I-ANIM-16 (S4), Z2-I-UI-14 (S4), the hosting of AUDIO's authoring panels for Z2-I-AUDIO-22 (S4), and the dialog of Z2-I-MOD-09 (S4); its requirements name those items in their "Covers" lines.

### Document family

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/EDIT/Foundation.md` | Scope of this file · Editor process · Editor session · Worlds in the editor · Play in editor · Documents · Commands and history · Selection · Clipboard |
| `Zenith2/Docs/Design/EDIT/Viewports.md` | Scope of this file · Editor shell · Viewports · Editor cameras · Picking · Gizmos · Snapping · Overlays |
| `Zenith2/Docs/Design/EDIT/Panels.md` | Scope of this file · Panel framework · Toolbar · Hierarchy panel · Inspector panel · Schema panel · Asset browser · Previews and thumbnails · Console and log panels · Migration panel · Project settings panel · New project wizard · Input bindings editor · Recovery dialog · Source control panel · Preferences panel · Profiler integration |
| `Zenith2/Docs/Design/EDIT/Pipeline.md` | Scope of this file · Cook client · Reimport · Asset operations · Hot reload · Schema change · Migration flow · Autosave · Session lock and crash detection · Recovery on startup · Source control · Merge-friendly files |
| `Zenith2/Docs/Design/EDIT/AuthoringTools.md` | Scope of this file · Reusable authoring widgets · Prefab editing · Terrain tools · Environment documents · Lighting tools · Material tools · VFX tools · Animation tools · UI authoring tool · Audio tools · Tools hosted for other areas |
| `Zenith2/Docs/Design/EDIT/Formats.md` | Scope of this file · Common rules · Session file · Layout file · Preferences file · Autosave file · Recovery manifest · Session lock line · Thumbnail file · Clipboard document · Endpoint file · Editor trash layout |
| `Zenith2/Docs/Design/EDIT/Automation.md` | Scope of this file · Command registry · Automation scripts · Live link endpoint · Operation timing · Usability walkthroughs |
| `Zenith2/Docs/Design/EDIT/Tests.md` | Scope of this file · Test targets and fixtures · Foundation tests · History and undo tests · Selection and clipboard tests · Document tests · Projection and world tests · Play tests · Shell and viewport tests · Camera gizmo and snapping tests · Picking tests · Panel tests · Pipeline tests · Hot reload tests · Migration and recovery tests · Source control tests · Automation tests · Authoring tool tests · Headless editing tests · Render tests · Integration tests · Benchmarks and budget runs |
| `Zenith2/Docs/Design/EDIT/Declarations.md` | Scope of this file · ZenithEditorCore headers · Declaration index |
| `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` | Scope of this file · ZenithEditor internal headers |

## Dependencies

### Areas EDIT depends on

| Area | What EDIT relies on | Headings |
|---|---|---|
| MOD | the runtime, its composition types, the frame-loop driver, the scaffold library, the composition record | `Zenith2/Docs/Design/MOD.md` → "Zenith_Runtime.h", "The composition.json record", "Composition hash", "Frame loop driver"; `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Editor wizard boundary" |
| GAME | the play world's origin `WORLD_ORIGIN_EDITOR_PLAY`, pause reasons, world creation and teardown | `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation", "Pause arbitration", "World state resource", "Shutdown and teardown" |
| ARCH (core, queued) | the world purpose `Zenith_WorldPurpose` (`WORLD_PURPOSE_PLAY`, `WORLD_PURPOSE_EDIT`, `WORLD_PURPOSE_PREVIEW`) read through `Zenith_WorldAttachContext::GetWorldPurpose()`, set by the world's creator | cross-doc item X243 (b) (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces", queued); used as VFX uses it (`Zenith2/Docs/Design/VFX/Simulation.md` → "Catch-up steps") |
| SCENE | documents, edits, sort keys, resolution, activation, capture, reference repair, the document framework | `Zenith2/Docs/Design/SCENE.md` → "What SCENE exposes to EDIT"; `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneEdit header", "Zenith_SceneDocument header", "Zenith_PrefabResolver header", "Zenith_SceneCapture header", "Zenith_ReferenceRepair header"; `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Document edits for the editor", "Sort keys for edits"; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_DocumentMigrator header", "Journaled write", "Storage-root writer lock" |
| ECS | the world inspector, out-of-tick playback, field sets, the reflection walk, schema re-registration | `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldInspector header", "Zenith_CommandBuffer header", "Zenith_WorldSimulation header"; `Zenith2/Docs/Design/ECS/Schemas.md` → "Editor re-registration"; `Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world"; `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectionWalk header" |
| ASSET | the cook server protocol and its command line, development reload, the lock files, the registered document types | `Zenith2/Docs/Design/ASSET/Cook.md` → "The cook server", "Options"; `Zenith2/Docs/Design/ASSET/Formats.md` → "Cook server messages", "Cook lock file"; `Zenith2/Docs/Design/ASSET/Loading.md` → "Development reload"; `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" |
| SCRIPT | in-process compilation, the stateless check, hot reload, console evaluation, diagnostics | `Zenith2/Docs/Design/SCRIPT.md` → "What SCRIPT authors and what EDIT owns"; `Zenith2/Docs/Scripting.md` → "Quarantine and hot reload" |
| RPROD, RHI | view requests, view extensions, the shader reload protocol, the PSO table | `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header", "View extensions"; `Zenith2/Docs/Rendering/ShaderSystem.md` → "Watch mode and editor reload"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states" |
| PLAT | windows, presentation input and claims, text input, clipboard, bindings authoring (the editor watches host directories with its own `Zenith_EditorHostWatcher`, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers") | `Zenith2/Docs/Design/PLAT.md` → "Editor surfaces PLAT exposes"; `Zenith2/Docs/Design/PLAT/Input.md` → "Presentation claims", "Keyboard and mouse"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Clipboard" |
| ENV | the terrain document, strokes, edits, the live preview, regeneration | `Zenith2/Docs/Design/ENV.md` → "What ENV authors"; `Zenith2/Docs/Design/ENV/Terrain.md` → "Terrain edit deltas and undo", "Live preview"; `Zenith2/Docs/Design/ENV/Bake.md` → "Regeneration after edits" |
| ANIM | the preview, the debug state, the graph compiler | `Zenith2/Docs/Design/ANIM.md` → "What ANIM authors"; `Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationPreview header" |
| UI | the editor model, the preview context, the catalogues | `Zenith2/Docs/Design/UI.md` → "Editor surfaces" |
| AUDIO | the panels it specifies, the tools-only system accessor | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces" |
| GEOM, LIGHT, VFX | material parameters and inspectors, light authoring surfaces, effect authoring | `Zenith2/Docs/Design/GEOM.md` → "Editor surfaces"; `Zenith2/Docs/Design/LIGHT.md` → "Editor surfaces"; `Zenith2/Docs/Design/VFX/Authoring.md` → "Preview session", "Effect inspector" |
| PHYS (drafting) | "Drop to rest" and the ragdoll preview over an isolated simulation | `Zenith2/Docs/Design/PHYS.md` → "Spawn clearance rule", "Editor surfaces" |
| DEVT | the overlay in editor views, the console | `Zenith2/Docs/Design/DEVT/Overlay.md` → "Editor overlay" |
| CORE, COMPRESS | containers, JSON, maths, SHA-256, logging, the memory governor, codec IDs | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader"; `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems"; `Zenith2/Docs/Design/COMPRESS.md` → "Codec ID registry" |
| STREAM, SAVE | their editor surfaces | `Zenith2/Docs/Design/STREAM.md` → "Editor surfaces"; `Zenith2/Docs/Design/SAVE.md` → "Editor surfaces" |

### Libraries and third-party dependencies

EDIT builds three host-tool targets, each one `zenith_add_tool` call (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"), all `SUPPORTED ZENITH_TOOLS_HOST`, because each depends, directly or through the one below it, on `ZenithRuntime` or a module (`ZENITH_GPU`), and [Z2-N-ARCH-172] makes any wider matrix a configure error in `win-asan` and `linux-tsan`. The lists below are the target state; every list grows by § "Dependency growth rule", and `ZenithAuthoring`'s list grows owner by owner (§ "ZenithAuthoring owners"):

```cmake
# Zenith2/Tools/Authoring/CMakeLists.txt: the tool library of ruling Q84 (a); no Internal/Zenith_ZenithAuthoringMain.cpp, so static.
# Created by EDIT's library-creation ticket with DEPENDS ZenithCore alone (§ "Library creation ticket"); each owner's
# move appends the libraries its own files include (§ "ZenithAuthoring contract rules", rule 4). Target state today:
zenith_add_tool(ZenithAuthoring
	DEPENDS ZenithCore ZenithReflect ZenithDocument ZenithAssets ZenithScene ZenithCompression ZenithJobs
	        ZenithAnimation ZenithAudio ZenithEnvironment ZenithUI ZenithVFX ZenithAI Zenith2D ZenithCinematics
	        ZenithStreaming ZenithContentCore
	SUPPORTED ZENITH_TOOLS_HOST)

# Zenith2/Tools/EditorCore/CMakeLists.txt: the tool library of ruling Q86 (a); static
zenith_add_tool(ZenithEditorCore
	DEPENDS ZenithCore ZenithReflect ZenithDocument ZenithDocumentHost ZenithScene ZenithECS ZenithAssets ZenithJobs
	        ZenithProfiling ZenithPlatform ZenithScript ZenithRuntime ZenithAuthoring
	SUPPORTED ZENITH_TOOLS_HOST)

# Zenith2/Tools/ZenithEditor/CMakeLists.txt: the executable
zenith_add_tool(ZenithEditor
	DEPENDS ZenithEditorCore ZenithAuthoring
	        ZenithCore ZenithGraph ZenithJobs ZenithProfiling ZenithPlatform ZenithReflect ZenithDocument ZenithDocumentHost
	        ZenithVertexFormat ZenithCompression ZenithCompression_Zlib ZenithCompression_Zstd
	        ZenithECS ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT ZenithScene ZenithAssets ZenithSave
	        ZenithRHI ZenithRHI_Vulkan ZenithRHI_D3D12 ZenithShadersGenerated ZenithFlux ZenithScript ZenithScriptCompiler
	        ZenithRuntime Zenith2D ZenithPhysics ZenithPhysics_Jolt ZenithPhysics_PhysX ZenithAnimation ZenithAudio ZenithAI
	        ZenithUI ZenithOnline ZenithOnline_Offline ZenithNet ZenithStreaming ZenithGameplay ZenithCinematics ZenithVFX
	        ZenithEnvironment ZenithNewGameScaffold
	THIRD_PARTY imgui
	SUPPORTED ZENITH_TOOLS_HOST)
```

`Zenith2/Docs/Architecture.md` → "Library dependency graph" lists ZenithEditor's direct dependencies as "ZenithRuntime, every module, every backend, ZenithNewGameScaffold, ZenithScriptCompiler, ZenithDocument, ZenithDocumentHost"; libgraph fails a used-but-undeclared edge ([Z2-N-ARCH-167]), and the editor includes engine-library headers directly (ECS, Scene, Flux, the generated shader headers, the vertex-format codec, Platform), so the row widens to the list above, and the tool table and the matrix table ("Matrix values by library") gain rows for `ZenithAuthoring` and `ZenithEditorCore`. Cross-doc item X257 carries the core rows (its two conditional parts are ruled: Q84 (a) and Q86 (a)), and X301 replaces X257 (2)'s fixed `ZenithAuthoring` list by the per-owner rule of § "ZenithAuthoring contract rules" and adds ZenithContent to both libraries' consumers; the two new directories `Tools/Authoring/` and `Tools/EditorCore/` join the protected `Zenith2/CLAUDE.md` layout line through the PROC hand-off EDIT-P1 of `findings/A5_proc_handoffs.md` (row ED13).

### Dependency growth rule

The lists above are target states. A dependency on a target that does not exist yet is a configure error ([Z2-N-ARCH-172]) and `DEPENDS` lists exactly the direct dependencies ([Z2-N-ARCH-167]), so every list grows ticket by ticket under one rule, with no stage claim:

1. The ticket whose diff first includes a header of library X from a target's sources adds X to that target's `DEPENDS` in the same diff; no ticket names a library its files do not include.
2. A forward declaration (`class Zenith_Clipboard;`) is not an include and references no symbol, so it adds no edge: a header slice that names a type of a subsequent library by pointer only (the Stage 2 `Zenith_EditorClipboard::Create(…, Zenith_Clipboard* …)`, `Declarations.md` → "Clipboard header") configures before that library is added; the ticket that first calls a member of the type includes its header and adds the edge.
3. `ZenithAuthoring`'s list is created with `ZenithCore` alone (§ "Library creation ticket") and grows only by owners' moves (§ "ZenithAuthoring contract rules", rule 4).
4. `ZenithEditor`'s backend entries (`ZenithECS_Flecs`, `ZenithECS_EnTT`, `ZenithPhysics_PhysX`, `ZenithOnline_Offline`, the RHI and compression backends) are added by the ticket that adds the backend's row to the module catalogue or the ECS backend table (`Foundation.md` → "Module catalogue"), which includes the backend's factory header.

The entry each subsystem's first ticket adds, for the reviewer's check of rule 1:

| Target | Entry | First includer |
|---|---|---|
| `ZenithEditorCore` | ZenithCore, ZenithReflect, ZenithDocument, ZenithDocumentHost, ZenithScene, ZenithAssets | the document table, JSON edits, the history, selection and the clipboard (the clipboard's `SetAssets` takes `Zenith_AssetGUID`; the selection's ASSET kind names it) |
| `ZenithEditorCore` | ZenithPlatform | the clipboard's system-clipboard path (`ReadSystemClipboard` calls `Zenith_Clipboard`), then the host paths and the file moves |
| `ZenithEditorCore` | ZenithECS, ZenithJobs, ZenithProfiling, ZenithScript, ZenithRuntime | the world set, the projection, the play controller, the operation context's timers |
| `ZenithEditorCore` | ZenithAuthoring | `Zenith_RegisterEditorDocumentTypes` (its first owner registration) and `Zenith_EditorContentCookChannel` (CONTENT's channel interface, row CT18) |
| `ZenithAuthoring` | each owner's entries | that owner's move (§ "ZenithAuthoring owners"); ZenithVFX by VFX's Stage 5 move (`Zenith2/Docs/Design/VFX.md` → "Stage 5 slices") |
| `ZenithEditor` | each module and backend | the catalogue row or extension that first includes it (rule 4) |

`Test_EditorCatalogue_CoversDeclaredModules` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Module catalogue") checks the catalogue against the modules the build declares, so it holds at every point of this growth.

### Tool-side code the editor needs

Ruling Q84 (a) (orchestrator, 2026-10-09) and the orchestrator's AUTHORING-CONTRACT ruling (2026-10-09): `ZenithAuthoring` is the one static tool library holding the authoring-side code both ZenithCook and ZenithEditor call, on the precedent of `ZenithDocumentHost` and `ZenithNewGameScaffold`; it is outside every Shipping link closure (a tool target, [Z2-N-ARCH-241]). EDIT owns its contract (the rules and the layout below), its `CMakeLists.txt`, its library root header and its first test; each owner owns its subdirectory, its root header, its tests and its moves, which land as bounded patches of the owner's design; EDIT edits none of those areas' documents. The ruling's `Tools/Authoring/<Area>/` is realised as `Zenith2/Tools/Authoring/Internal/<Dir>/` plus one public root header per owner, because [Z2-N-ARCH-161] admits compiled sources only at a library's root and under `Internal/`.

### ZenithAuthoring contract rules

<!-- elaboration -->
1. **Layout.** An owner's sources and private headers sit in `Zenith2/Tools/Authoring/Internal/<Dir>/` (subdirectories below it are the owner's, for example CONTENT's `Batch/`, `Repair/`); its public declarations sit in exactly one root header `Zenith2/Tools/Authoring/Zenith_Authoring<Area>.h`, included as `#include "Authoring/Zenith_Authoring<Area>.h"` ([Z2-N-ARCH-164]), carrying a contract block per type (`Zenith2/Docs/Contracts.md` → "Contract blocks per type", checked by `ApiContractCheck` because the header is a public root header of a library). `<Dir>` and `<Area>` are the owner's names in § "ZenithAuthoring owners"; no two owners share a directory or a header, and no source sits in `Tools/Authoring/<Dir>/` outside `Internal/` (a configure error, [Z2-N-ARCH-161]).
2. **Tests.** The library's one test target is `ZenithAuthoring_Tests` (created by `zenith_add_tool` from `Zenith2/Tools/Authoring/Tests/**`, linking `ZenithAuthoring`, `ZenithTestSupport` and doctest; matrix `ZENITH_TOOLS_HOST`, so it runs in the `Test` step of the `win` preset). An owner's tests sit in `Tests/<Dir>/`, each file named `Zenith_<Subject>_Tests.cpp`, each test name unique programme-wide (TEST-NAME-UNIQUE); its header-only helpers in `Tests/<Dir>/Support/` (X184: compiled by no target, [Z2-N-ARCH-255]); its committed fixtures under the target's fixture root `Tests/Fixtures/` ([Z2-N-ARCH-250]): migration fixtures in `Tests/Fixtures/Migration/<Dir>/<Type>/v<N>.json` and `v<N>.expected.json`, which is [Z2-N-CONTRACT-548]'s `Tests/Fixtures/Migration/<Type>/` with `<Type>` qualified by the owner's directory, because one library holds every owner's types, and other fixtures in `Tests/Fixtures/<Dir>/`. No `Tests/<Dir>/Fuzz/` directory exists: fuzz targets are created in `win-asan` alone (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets"), where `ZenithAuthoring` (`ZENITH_TOOLS_HOST`) is absent, and a `.cpp` there would be compiled into `ZenithAuthoring_Tests` beside the test main; a parser of external bytes that needs a fuzz target lives in a library built in `win-asan` (the module itself, or a `ZENITH_WINDOWS` tool library such as CONTENT's `ZenithContentCore`).
3. **What may be compiled in.** Only the owner's own sources under `Internal/<Dir>/` and EDIT's library file. Never another library's `Internal/` source (a public function of the other library is called instead, `Zenith2/Docs/Contracts.md` → "API boundary"), never a ZenithCook or ZenithEditor header, and no declaration, forward declaration or parameter of a ZenithCook-internal or ZenithEditor-internal type (`Zenith_CookStepRegistry`, `Zenith_CookServices`, `Zenith_CookContext`, `Zenith_CookDiagnosticLevel`, `Zenith_EditorPanelContext`, …): ZenithCook depends on ZenithAuthoring and the layering is a DAG ([Z2-N-ARCH-167]). Cook-step registration therefore stays in ZenithCook (`Tools/ZenithCook/Internal/<Area>/`, the signature of `Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookStepRegistry"), and the step's body calls the owner's `ZenithAuthoring` functions with plain inputs and converts their results (TWOD's `Zenith_TwoDDiagnosticLevel` precedent, `Zenith2/Docs/Design/TWOD/Authoring.md` → "ZenithAuthoring functions"); ImGui and every panel stay in ZenithEditor (`Internal/<Area>/`). Engine-library public headers and ZenithContentCore's public headers may be included, each adding its edge (rule 4).
4. **Dependencies.** The library is created with `DEPENDS ZenithCore`; an owner's move appends, in its own diff, every library its files include that the list does not yet hold (§ "Dependency growth rule", rule 1), and records the edge in its own design; no owner's design may claim an edge is "already declared" unless the list on the `master` it branches from holds it. ZenithAuthoring holds no other edge: no `ZenithRuntime` (no owner needs it today), no `ZenithEditorCore`, no `ZenithCook`.
5. **Consumers.** ZenithCook (which adds `ZenithAuthoring` to its own `DEPENDS` in the first owner move that lands, an ASSET-owned line), `ZenithEditorCore`, `ZenithEditor`, CONTENT's tool `ZenithContent`, and the integration suites `Zenith_Integration_ContentPipeline` (CONTENT's) and `Zenith_Integration_WorldAuthoring` (WORLD's), each `SUPPORTED ZENITH_TOOLS_HOST`.
6. **Blocking.** Every owner's move ticket is blocked by EDIT's library-creation ticket (§ "Library creation ticket"); an EDIT ticket that calls a moved function is `draft`, blocked by that owner's move ticket.

### ZenithAuthoring owners

<!-- assumption: pending ANIM, ENV, AUDIO, ASSET, UI, VFX, GEOM, AI, CINE, CONTENT, TWOD, WORLD, STREAM, PACK, MOD, ECS, PLAT (rows ED39 to ED50) -->
| Owner | `<Dir>` | Root header | Edges its move adds | What the editor calls | Tests and fixtures | State (row) |
|---|---|---|---|---|---|---|
| EDIT (the library itself) | — (`Internal/Zenith_AuthoringLibrary.cpp`) | `Zenith_AuthoringLibrary.h` | ZenithCore | nothing (the library's version constant) | `Tests/Zenith_AuthoringLibrary_Tests.cpp` | § "Library creation ticket" |
| ANIM | `Animation` | `Zenith_AuthoringAnimation.h` | ZenithReflect, ZenithDocument, ZenithAnimation | `Zenith_ListAnimationGraphInputs`, `Zenith_CompileAnimationGraph`, `Zenith_RegisterAnimationDocumentTypes` | `Tests/Animation/`; `Tests/Fixtures/Migration/Animation/<Type>/` | moved in ANIM's design, conforms (ED39) |
| ENV | `Environment` | `Zenith_AuthoringEnvironment.h` | ZenithDocument, ZenithEnvironment | `Zenith_RegisterEnvironmentDocumentTypes` (AI's and WORLD's cell functions are ENV's too, rows AI3, WD11) | `Tests/Environment/`; `Tests/Fixtures/Migration/Environment/<Type>/` | move pending (ED40) |
| AUDIO | `Audio` | `Zenith_AuthoringAudio.h` | ZenithDocument, ZenithAudio, ZenithCompression | `Zenith_RegisterAudioDocumentTypes`, `Zenith_AudioCookBankPreview`, `Zenith_AudioCookMixerSection` | `Tests/Audio/`; `Tests/Fixtures/Migration/Audio/<Type>/` | moved in AUDIO's design; its test file and "already declared" edge deviate (ED41, ED47) |
| ASSET | `Asset` | `Zenith_AuthoringAsset.h` | ZenithDocument, ZenithAssets | `Zenith_RegisterAssetMetaMigrations` (taking `const Zenith_AssetMetaSettingsSource*`, null in the editor) and the `.meta` member-migration table (`Zenith_AssetMetaMemberMigration`, `Zenith_GetAssetMetaMemberMigrations`, CONTENT's `provenance` member among its rows); CONTENT's DDC types (row CT8) | `Tests/Asset/`; `Tests/Fixtures/Migration/Asset/AssetMeta/` (the `provenance` member's fixtures in `AssetMeta/provenance/`) | moved in ASSET's design (`Zenith2/Docs/Design/ASSET/Migration.md` → "The meta document type"), conforms (ED42) |
| UI | `UI` | `Zenith_AuthoringUI.h` | ZenithDocument, ZenithUI | `Zenith_RegisterUIDocumentTypes`, `Zenith_GetUICanonicalSchema` | `Tests/UI/`; `Tests/Fixtures/Migration/UI/<Type>/` | moved (`Zenith2/Docs/Design/UI.md` → "ZenithAuthoring UI files"), conforms (ED43) |
| VFX | `VFX` | `Zenith_AuthoringVFX.h` | ZenithDocument, ZenithVFX (Stage 5) | `Zenith_RegisterVFXDocumentTypes` | `Tests/VFX/`; `Tests/Fixtures/Migration/VFX/EffectDocument/` | move pending, Stage 5 (ED44) |
| GEOM | `Geom` | `Zenith_AuthoringGeom.h` | ZenithDocument | `Zenith_RegisterMaterialMigrations` | `Tests/Geom/`; `Tests/Fixtures/Migration/Geom/Material/` | move accepted; fixtures and test home to confirm (ED46) |
| AI | `AI` | `Zenith_AuthoringAI.h` | ZenithAI (ZenithEnvironment from ENV's move) | `Zenith_RegisterAIDocumentTypes`; the Navigation extension's bake inputs (row AI12) | `Tests/AI/`; `Tests/Fixtures/Migration/AI/<Type>/` | designed, conforms (AI12) |
| CINE | `Cinematics` | `Zenith_AuthoringCinematics.h` | ZenithDocument, ZenithCinematics, ZenithCompression | `Zenith_RegisterCinematicDocumentTypes`, `Zenith_ExportCinematicCapture` | `Tests/Cinematics/`; `Tests/Fixtures/Migration/Cinematics/SequenceDocument/` | does not conform: layout, cook steps, another library's `Internal/` sources, fuzz targets (ED45) |
| CONTENT | `CONTENT` | `Zenith_AuthoringContent.h` | ZenithDocument, ZenithScene, ZenithAssets, ZenithContentCore | `Zenith_RegisterContentDocumentTypes`, `Zenith_ContentCookChannel` (implemented by `Zenith_EditorContentCookChannel`), `Zenith_ContentBatchOperation`, the preset and override validators (row CT18) | `Tests/CONTENT/`; `Tests/Fixtures/Migration/CONTENT/ImportPresetDocument/` | designed; fixture directory to align (ED48); its design's "ZenithAuthoring's declared set" edge wording to replace by these edges (ED50) |
| TWOD | `TwoD` | `Zenith_AuthoringTwoD.h` | ZenithDocument, Zenith2D | `Zenith_RegisterTwoDDocumentTypes`, the atlas packer and auto-tile functions for the 2D extension | `Tests/TwoD/`; `Tests/Fixtures/Migration/TwoD/<Type>/` | designed; fixture and fuzz directories to align (ED48, TD8) |
| WORLD | `WORLD` | `Zenith_AuthoringWorld.h` | ZenithReflect, ZenithDocument, ZenithScene, ZenithAssets, ZenithEnvironment, ZenithStreaming, ZenithJobs, ZenithCompression (the libraries WORLD's files include, `Zenith2/Docs/Design/WORLD.md` → "Libraries WORLD code links"; each appended by WORLD's move unless an earlier owner's move already appended it) | `Zenith_RegisterWorldDocumentTypes`, the partition, generator and fingerprint functions for the World extension | `Tests/WORLD/`; `Tests/Fixtures/Migration/WORLD/<Type>/` and `Tests/Fixtures/WORLD/` (its goldens) | designed; fixture directories (`Tests/WORLD/Golden/`, `Tests/WORLD/Migration/`) and "already in its `DEPENDS`" edge wording to align (ED48, ED50, WD15, X303) |
| STREAM, PACK, MOD, ECS, PLAT | `Streaming`, `Package`, `Game`, `Schema`, `Platform` | `Zenith_AuthoringStreaming.h`, `…Package.h`, `…Game.h`, `…Schema.h`, `…Platform.h` | ZenithDocument and the owner's library | the document-type registration of `.streamworld.json`, `package.recipe.json`, `game.json`, `*.schema.json`, and PLAT's bindings and configuration documents | `Tests/<Dir>/`; `Tests/Fixtures/Migration/<Dir>/<Type>/` | registrations today in ZenithCook's `Internal/` or unnamed, which the editor cannot call (ED49) |
<!-- /assumption -->

The registration calls `Zenith_RegisterEditorDocumentTypes` makes, their order and headers are `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor document type registrations". GEOM's move (`Zenith2/Docs/Design/GEOM.md` → "Editor surfaces": `Zenith_RegisterMaterialMigrations` from ZenithCook's `Internal/Materials/` to `Internal/Geom/`) is accepted by the AUTHORING-CONTRACT ruling: the material document's registration is in `ZenithAuthoring`, while its reader, writer and cook step stay in ZenithCook, and the editor validates a material document through the cook server's `validate` (A5 row GB24).

### Library creation ticket

<!-- elaboration -->
EDIT's first implementation ticket is the Stage 2 ticket "Stage 2 ZenithAuthoring and ZenithEditorCore library targets" (Draft-ID `Z2T-EDIT-007`, claimed in `tickets/_shared/draft_ids.json` and drafted ahead of EDIT's batch on the Q85 (b) precedent; sprint `Stage 2 — Foundation`, fix version `Z2 Stage 2`; `draft` until the core rows X257 (2) and X301 and the PROC hand-off EDIT-P1 land). The owner move tickets already drafted (Z2T-GEOM-136, Z2T-ANIM-021, Z2T-STREAM-015, Z2T-VFX-009) take `blocked_by: Z2T-EDIT-007` through `findings/A5_cross_area_blockers.md` (row EdB1), as rule 6 requires. It creates both libraries empty of features, so every owner's move and EDIT's own pull-forward slice block on one small ticket:

| File | Content |
|---|---|
| `Zenith2/Tools/Authoring/CMakeLists.txt` | `zenith_add_tool(ZenithAuthoring DEPENDS ZenithCore SUPPORTED ZENITH_TOOLS_HOST)` |
| `Zenith2/Tools/Authoring/Zenith_AuthoringLibrary.h` | `inline constexpr uint32 uZENITH_AUTHORING_LIBRARY_VERSION = 1;` and `uint32 Zenith_GetAuthoringLibraryVersion();` with a contract block (`functions Zenith_AuthoringLibrary`: no state, `ANY_THREAD`) |
| `Zenith2/Tools/Authoring/Internal/Zenith_AuthoringLibrary.cpp` | returns the constant |
| `Zenith2/Tools/Authoring/Tests/Zenith_AuthoringLibrary_Tests.cpp` | `Test_AuthoringLibrary_VersionLinked`: the function returns `uZENITH_AUTHORING_LIBRARY_VERSION` (so `ZenithAuthoring_Tests` exists from the first diff) |
| `Zenith2/Tools/Authoring/README.md` | the rules of § "ZenithAuthoring contract rules" and the owner table of § "ZenithAuthoring owners", quoted with this heading as their source |
| `Zenith2/Tools/EditorCore/CMakeLists.txt` | `zenith_add_tool(ZenithEditorCore DEPENDS ZenithCore SUPPORTED ZENITH_TOOLS_HOST)` |
| `Zenith2/Tools/EditorCore/Zenith_EditorError.h`, `Zenith_EditorIDs.h` | the declarations of `Zenith2/Docs/Design/EDIT.md` → "Error enumeration" and `Zenith2/Docs/Design/EDIT/Declarations.md` → "Identifier header" |
| `Zenith2/Tools/EditorCore/Internal/Zenith_EditorIDs.cpp` | `Zenith_MakeEditorCommandID` (`Zenith_Hash64` of the identifier text) |
| `Zenith2/Tools/EditorCore/Tests/Zenith_EditorIDs_Tests.cpp` | `Test_EditorIDs_CommandIDHashesText`: `Zenith_MakeEditorCommandID("entity.create").m_ulHash` equals `Zenith_Hash64` of the 13 bytes `entity.create`, and two different identifiers give different IDs |

Its gates are the `Zenith2` category's (`Configure`, `Build`, `Test`, `SourceLayout`, `Libgraph`, `ApiContractCheck`, `HeaderSelfContained`); its Definition of Done includes the failure scenario "a source placed in `Tools/Authoring/Foo/` outside `Internal/` fails the configure with [Z2-N-ARCH-161]'s message".

### Interfaces EDIT declares for lower layers

None: `ZenithEditor` is a tool, a leaf of the dependency graph, and nothing calls it; `ZenithEditorCore` and `ZenithAuthoring` are tool libraries whose only dependants are tools and tool-matrix suites: `ZenithEditorCore`'s are ZenithEditor, CONTENT's tool `ZenithContent` (row CT18) and the integration suites `Zenith2/Tests/EditorMigration/` (SCENE's) and `Zenith2/Tests/EditorSchema/` (ECS's); `ZenithAuthoring`'s are those of § "ZenithAuthoring contract rules" rule 5 (ZenithCook, `ZenithEditorCore`, ZenithEditor, `ZenithContent`, `Zenith_Integration_ContentPipeline`, `Zenith_Integration_WorldAuthoring`). The editor's own interfaces (`Zenith_EditorExtension`, `Zenith_EditorPanel`, `Zenith_EditorCommand`, `Zenith_EditorDocument`, `Zenith_EditorOperation`, `Zenith_EditorRuntimeHost`) are implemented inside the editor or a suite alone. Upward notifications the editor needs from lower layers use the interfaces those layers declare (`Flux_ViewExtension`, `Zenith_InputClaimant`, `Zenith_TerrainDocumentSink`, `Zenith_TerrainTileSource`, `Zenith_PrefabProvider`, `Zenith_MemoryConsumer`) or the tool frame hooks MOD declares (`Zenith_ToolFrameHooks`, row ED1), through which the runtime hands the editor its script world events on the main thread.

### Consumers of EDIT and their needs

Every need other areas have filed against EDIT, answered by the names they use:

| Need (row) | Consumer | EDIT heading | EDIT API or row |
|---|---|---|---|
| ScB13: migration UI over `Zenith_MigrationPlan`, `ApplyPlan`, `RecoverPendingJournals`; the editor composition registering every owner's document type | SCENE (Z2T-SCENE-087, -089, -090) | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Storage roots and the migration registry" | `Zenith_EditorMigrationModel` and `Zenith_RegisterEditorDocumentTypes` in `ZenithEditorCore` (ruling Q86 (a)); Z2T-SCENE-089's test moves to the suite `Zenith2/Tests/EditorMigration/` linking `ZenithEditorCore` (row ED26) |
| ScB13: the producer of the EDIT budget row "Scene open" | SCENE (Z2T-SCENE-087, -090, -092) | § "Producers EDIT schedules" | row **Z2-B-EDIT-005**, producer "Stage 1 W-Editor row ticket for scene open", Draft-ID `Z2T-EDIT-005` (registered) |
| AB4: producers of EDIT's two reimport rows | ASSET (Z2T-ASSET-093 and its split draft, Z2T-ASSET-099) | § "Producers EDIT schedules" | **Z2-B-EDIT-004** "Asset reimport" (`Z2T-EDIT-004`); **Z2-B-EDIT-010** "Walkthrough reimport an asset" (`Z2T-EDIT-001`) |
| EB13, E11: the schema panel and re-registration between play sessions; the edit-world flag ECS defines and `CreateWorld` sets (row EdB26); ECS-100's "editor session" and `ZenithEditorCore` | ECS (Z2T-ECS-099, -100) | `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world rules" | `Zenith_EditorSchemaModel` in `ZenithEditorCore` (ruling Q86 (a)); edit worlds created with the edit-world flag; `ApplySchemaRevision` after the window ends; the lock through `ZenithCook schema-lock` (row ED27) |
| SB14, SC19: hot-reload producer, `RequestHotReload`, the console compile thunk `m_pfnCompileConsole`, the producer of Z2-B-EDIT-002 | SCRIPT (Z2T-SCRIPT-073, -078, -083, -101) | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload", "Script reload timing"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Console" | **Z2-B-EDIT-002**, producer "Stage 1 W-Editor row ticket for script hot reload", `Z2T-EDIT-002` |
| GA14 (b), GA5: play worlds with origin `WORLD_ORIGIN_EDITOR_PLAY`; the editor's pause uses `WORLD_PAUSE_REASON_EDITOR` | GAME | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play", "Pause and single step", "World kinds" | `play.start`, `play.pause`; play worlds keep origin 4; edit and preview worlds are told apart by the X243 (b) world purpose, never by an origin (row ED37) |
| EN12, EnB13, X127: the terrain command and tool, the preview call sites, regeneration, the sink and tile source, the W-Editor sculpt-latency row and procedure, the inspectors, **the anchor on the editor camera** | ENV (Z2T-ENV-012 … -116) | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain tools", "Environment documents"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities" | **Z2-B-EDIT-011**, **Z2-B-EDIT-012**, producer "Stage 1 per-system budget ticket for EDIT" (`Z2T-EDIT-001`); the anchor is `Zenith.Environment.Anchor` on the proxy entity of the active scene view, the documents' anchors being withheld from edit worlds (row ED31) |
| bake UI; inspectors, light gizmos, the grading editor, "capture now" | LIGHT | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Lighting tools" | bake through the cook server; orchestration WORLD's (row ED18); realtime "capture now" as an edit-world-only write stripped from the play copy (row ED32) |
| AN11: the eight animation panels; AN12: "Animation graph preview latency" | ANIM | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Animation tools" | pending row, producer "Stage 1 per-system budget ticket for EDIT" (row ED23, X260; X212's other wording is reconciled there) |
| GB24: material preview and inspector; documents validated through the cook server | GEOM (Z2T-GEOM-044, -051, -090) | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Material tools"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Material preview" | `validate` requests; `ReplaceParameters` re-applied by the material projection handler on every forward and inverse change; preview keys from the document |
| UI15: the UI authoring tool on `Zenith_UIEditorModel`; UI's game view surface | UI | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "UI authoring tool"; `Zenith2/Docs/Design/EDIT/Viewports.md` → "Game viewport input" | the editor's own panels are ImGui (EDIT); UI provides the model, the preview context and the catalogues (row ED14 for the input rectangle) |
| M19, Z2-R-MOD-095 … -097: the wizard dialog and `Test_NewProjectWizard_MatchesCli` | MOD | `Zenith2/Docs/Design/EDIT/Panels.md` → "New project wizard" | the test in `Tools/ZenithEditor/Tests/Zenith_NewProjectWizard_Tests.cpp` |
| P15, PB21: the editor claimant; the binding editor's canonical writer | PLAT | `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor input claimant"; `Zenith2/Docs/Design/EDIT/Panels.md` → "Input bindings editor" | `Zenith_EditorInputClaimant`; `Zenith_WriteBindingsJson` |
| AS14, AS30, AS31: the cook server client, the lock-held restart, presenting `DOCUMENT_ERROR_STORAGE_LOCKED` | ASSET, SCENE | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Cook client"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Saving" | `Zenith_EditorCookClient` (in `ZenithEditorCore`) |
| S20: sort keys for every placing edit and the two-document apply | SCENE | `Zenith2/Docs/Design/EDIT/Panels.md` → "Hierarchy operations"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Prefab apply and revert" | `entity.reparent`, `prefab.applyOverrides` |
| AU3, AU18: hosting AUDIO's panels; the running audio system in play-in-editor; the in-process preview cook | AUDIO | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Audio tools" | `FindModuleInstance` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access"); the preview cook in `ZenithAuthoring` (ruling Q84 (a), row ED41) |
| VX11: the VFX preview host with `WORLD_PURPOSE_PREVIEW`, the inspector customisations, viewers on the editor's camera entities | VFX | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "VFX tools"; `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities" | the preview host creates preview-purpose worlds (X243 (b)); `Zenith.VFX.Viewer` on each scene view's proxy entity (row ED30) |
| RPB20: the shader-watch journal consumer | RPROD | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload" | **Z2-B-EDIT-003**, producer "Stage 1 W-Editor row ticket for shader blob reload", `Z2T-EDIT-003` |
| CB8: the editor's shedding case | CORE (Z2T-CORE-090) | `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail cache" | ruling Q86 (b): `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` in `ZenithEditor_Tests`; CORE's case table records `MEMORY_SUBSYSTEM_EDITOR_CACHES` as tool-only, tested there (row ED28) |
| CB11: the scene-open row for D-SEL-JSON's comparison | CORE (Z2T-CORE-059) | § "Producers EDIT schedules" | **Z2-B-EDIT-005** (non-blocking), `Z2T-EDIT-005` |
| GR14: a W-Editor workload with scripted schedule recompiles | GRAPH (Z2T-GRAPH-049) | `Zenith2/Docs/Design/EDIT/Automation.md` → "Measurement procedure" | the W-Editor script's recompile steps: five play worlds opened and closed from Stage 3, five schema applies and undos from Stage 4 |
| the per-system budget rows Z2-B-EDIT-001, -007 … -012 | Budgets | § "Rows EDIT owns" | producer "Stage 1 per-system budget ticket for EDIT" (`Z2T-EDIT-001`) |
| "Drop to rest" (a 2-second isolated simulation written as an undoable edit) and the ragdoll preview stepping an isolated world | PHYS (drafting) | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Physics tools" | command `physics.dropToRest` over PHYS's isolated simulation (row ED34), one record of `SET_PROPERTY` edits; the ragdoll preview a PHYS-ticketed area panel (row ED15) |
| the streaming panel; play-in-editor saves | STREAM, SAVE | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Tools hosted for other areas" | the panel host |
| DV27: the editor's `main` dispatches the crash helper directly after installing the root heap | DEVT | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Startup sequence" (step 1) | `Zenith_RunCrashHelperIfRequested(iArgc, aszArgv, Zenith_CrashHelperIdentity{ "ZenithEditor", 0 }, iExit)` as the second statement of `Zenith_EditorMain`; a true result uninstalls the heap and returns `iExit` |
| AI12: the Navigation extension; AI's authoring code in `ZenithAuthoring` | AI | `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table"; § "ZenithAuthoring owners" | the extension `Internal/Navigation/`, tests `Tests/Navigation/`, AI-ticketed; `Internal/AI/` with `Zenith_AuthoringAI.h`; edit and preview worlds register `AI.NavIngest` alone (ED6) |
| PK1: format-registry rows for the editor's binary formats | PACK | `Zenith2/Docs/Design/EDIT/Formats.md` → "Format registry rows" | `ZAUT` (`.zautosave`) and `ZTHB` (`.zthumb`), each with its refusal test `Test_EditorAutosave_DamagedFileRejected` and `Test_EditorFormats_ThumbnailFileRoundTrip` |
| PK9: the packaging panel over `package.recipe.json`, `size-report.json` and `partition.json` | PACK | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Tools hosted for other areas" | the PACK-ticketed area panel `Internal/Package/`; the recipe edited as a JSON document through EDIT's migration and save path; the two reports read-only |
| CT18: the cook channel, the CONTENT registration, the panels, the batch path's undo | CONTENT | `Zenith2/Docs/Design/EDIT/Declarations.md` → "Content cook channel header"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reference repair", "Merge conflicts in documents"; `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Tools hosted for other areas" | `Zenith_EditorContentCookChannel` in `ZenithEditorCore`; `Zenith_RegisterContentDocumentTypes` in the registration; the CONTENT panels; a batch's record holds `Zenith_EditorContentBatchOperation`, whose inverse is `Zenith_ContentBatchOperation::Rollback` |
| CT30 (supersedes CT27 (a) and CT18 (a)'s blocking channel members): the batch thread model and the asynchronous channel | CONTENT (Z2T-CONTENT-051 to -054, -060) | `Zenith2/Docs/Design/EDIT/Declarations.md` → "Content cook channel header"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reference repair"; § "Threads EDIT owns" | `EditorContentBatch` owns the batch (`Plan`, `Run` twice, `Rollback` after `Open`); `Zenith_EditorContentCookChannel` implements `BeginRecord`, `BeginList`, `BeginImport`, `BeginSetSettings`, `BeginDependents`, `Poll` and `Cancel` in PUMP and FORWARD modes, none of them waiting; progress through a depth-1 queue to the main thread (Z2T-EDIT-099) |
| CH22: `Audio` in EditorFixture's `modules` from Stage 3 | CHAR (walkthrough fixtures) | `Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures" | `"modules": ["Animation", "Audio", "Environment", "Gameplay", "Physics"]` (Z2T-EDIT-026), the Audio row of the Stage 3 module catalogue (Z2T-EDIT-028) |
| CN13: the sequencer panel | CINE | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Tools hosted for other areas" | `Internal/Cinematics/`, CINE-ticketed, over `Zenith_EditorTimeline` and `Zenith_EditorCurveEditor` (`Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel") |

### Assumed interfaces from other areas

Each is fenced `<!-- assumption: pending … -->` where it is used and recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` with the prefix `ED`; the core-document needs are cross-doc items:

| Row | Owner | Assumption | Used in |
|---|---|---|---|
| ED1 | MOD (core wording: X258) | `Zenith_RuntimeToolAccess` (create without initial world and startup scene, filling `Zenith_ToolServices` once; `RunOneFrame` with tool hooks, the script world events among them; `CreateWorld` by purpose; `DestroyWorld`; `SetInputWorld(0)`; `FindModuleInstance`; `QueueSchemaJson`; `AddToolExtractor`), the legality of each call per hook (Foundation.md "Tool access calls per hook"), the runtime staying runnable after `RunOneFrame` returned false until the tool destroys it, `GetRequestedExitCode` for the device-loss exit, `Zenith_ToolRuntimeDesc::m_pxRHIFaultInjector` for tests, and `Zenith_ToolRuntimeDesc::m_xCrashIdentity`, the identity startup step 7's crash handler carries (the editor passes `{ "ZenithEditor", 0 }`, the values its main gives `Zenith_RunCrashHelperIfRequested`, as DEVT's `Zenith_CrashHandlerDesc::m_xIdentity` requires). Answered: MOD specifies `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeToolAccess.h` in `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access", which X258's core wording now cites; EDIT's text conforms to it | Foundation.md "Runtime tool access", "Tool access calls per hook", "Startup sequence"; Viewports.md "Editor extractor" |
| ED2, ED3 | GAME, MOD | withdrawn: replaced by the X243 (b) world purpose | — |
| ED4 … ED9 | SCRIPT, PHYS, AI, GAME, RPROD, ENV/VFX/ANIM/AUDIO/UI/STREAM | edit and preview world behaviour per module, keyed on the world purpose | Foundation.md "Module behaviour in edit and preview worlds" |
| ED10 | GAME, ECS | `Zenith_WorldControl::RequestStep`, `Zenith_WorldSimulation::RunTicksThenPause`, with the step semantics of Foundation.md "Pause and single step" | the same |
| ED11 | MOD, PLAT | no input world while editing | Foundation.md "Input routing during play" |
| ED12 | RPROD, GEOM | tool textures, the composite-only shell view (core part X259), depth at the overlay point, mesh draw ranges for the selection mask, `ReloadShaderContainer`, `Flux.PSO.ReloadSwaps` | Viewports.md; Pipeline.md "Shader reload" |
| ED13 | ARCH | the editor's libraries and dependency lists (core part X257) | § "Libraries and third-party dependencies" |
| ED14 | UI, PLAT | the play world's presentation rectangle for UI pointer input | Viewports.md "Game viewport input" |
| ED15 | every area with editor panels | area-owned panels against the extension interface | Panels.md "Area-owned panels" |
| ED16 | SCRIPT | `Zenith_ScriptWorldListener::OnModuleReloaded`, forwarded to the editor through the tool hook `OnScriptModuleLive` | Pipeline.md "Script reload timing" |
| ED17 | ANIM | the `editor` member of graph nodes and states | AuthoringTools.md "Animation tools" |
| ED18 | WORLD | bake orchestration | AuthoringTools.md "Bake panel" |
| ED19 | ARCH, ASSET, ANIM, ENV, AUDIO | the `ZenithAuthoring` tool library (ruled Q84 (a) and AUTHORING-CONTRACT; the moves and deviations are rows ED39 to ED50) | § "Tool-side code the editor needs", "ZenithAuthoring contract rules" |
| ED20 | ENV | `Flux.Environment.PreviewPublishedFrame` | AuthoringTools.md "Terrain live preview" |
| ED21 … ED24 | Budgets (core part X260) | pending rows | § "Pending row needs" |
| ED26 | SCENE | Z2T-SCENE-089's test in the suite `Zenith2/Tests/EditorMigration/` over `Zenith_EditorMigrationModel` | Pipeline.md "Migration flow" |
| ED27 | ECS | Z2T-ECS-100 over `ZenithEditorCore`'s `Zenith_EditorSchemaModel`; the lock through `ZenithCook schema-lock` | Panels.md "Schema panel workflow" |
| ED28 | CORE | CB8: `MEMORY_SUBSYSTEM_EDITOR_CACHES` tested in `ZenithEditor_Tests` | Panels.md "Thumbnail cache" |
| ED29 | MOD | the game-directory allowlist admits `.editor` (gitignored) and `Workload/<W>.editorscript.json`; the nested `.gitignore` row | § "Untracked editor files"; Automation.md "Measurement procedure" |
| ED30 | VFX | `Zenith.VFX.Viewer` on the editor proxy entities; in-place play counters stripped from the play copy | Foundation.md "Editor proxy entities" |
| ED31 | ENV | the edit world's anchor on the active scene view's proxy; documents' anchors withheld from edit worlds | the same |
| ED32 | LIGHT | realtime "capture now" as an edit-world-only write | AuthoringTools.md "Probe capture" |
| ED33 | RPROD | the editor's view-extension registration at its startup step 12; the shell and overlay filters by view ID; `Zenith.Editor.GameViewTarget` declared by RPROD; game-view gating by world purpose; the "Present pass realisation" row (core part X259) | Viewports.md "Shell view request", "Viewport views" |
| ED34 | PHYS | an isolated simulation for "Drop to rest" and the ragdoll preview | AuthoringTools.md "Physics tools" |
| ED35 | ASSET | `ZenithCook --game-root <dir>`; the `schema-lock` verb's locks; `--serve` with `--out` | Pipeline.md "Server process"; Tests.md "Fixtures" |
| ED36 | MOD, SCRIPT | the runtime's script world listener forwards halts and live module generations through the tool hooks on the main thread | Foundation.md "Runtime tool access" |
| ED37 | GAME, MOD | `Zenith_WorldCreateDesc` carries the X243 (b) purpose; edit and preview worlds take origin `WORLD_ORIGIN_STARTUP` | Foundation.md "World kinds" |
| ED38 | MOD | the name validator `Zenith_IsValidGameName` exported by `ZenithNewGameScaffold` | Foundation.md "Executable and command line" |
| ED39 … ED44 | ANIM, ENV, AUDIO, ASSET, UI, VFX | the moves into `ZenithAuthoring` of § "ZenithAuthoring owners" (ED39 and ED43 conform; ED41 moved with the deviations of ED47) | § "ZenithAuthoring owners" |
| ED45 | CINE | CINE's ZenithAuthoring part conforms to the contract: `Internal/Cinematics/` and one root header `Zenith_AuthoringCinematics.h`; its cook steps registered in ZenithCook's `Internal/Cinematics/`; no ZenithCinematics `Internal/Loading/` source compiled in (public validators instead); no `Tests/<Dir>/Fuzz/` | § "ZenithAuthoring contract rules" |
| ED46 | GEOM | the material registration move accepted; its fixtures and test homed in ZenithAuthoring's layout | § "ZenithAuthoring owners" |
| ED47 | AUDIO | AUDIO's move adds ZenithDocument, ZenithAudio and ZenithCompression itself (no edge is "already declared"); its test file moves to `Tests/Audio/` | § "ZenithAuthoring contract rules" |
| ED48 | CONTENT, TWOD, WORLD | migration fixtures under `Tests/Fixtures/Migration/<Dir>/<Type>/`; no `Tests/TwoD/Fuzz/` | § "ZenithAuthoring contract rules" |
| ED49 | STREAM, PACK, MOD, ECS, PLAT | the document-type registrations the editor calls live in `ZenithAuthoring` under the owner's directory and root header | Foundation.md "Editor document type registrations" |
| ED50 | WORLD, CONTENT, ANIM | rule 4 and rule 2 of § "ZenithAuthoring contract rules": WORLD's move appends every edge its files include (ZenithReflect, ZenithDocument, ZenithScene, ZenithAssets, ZenithEnvironment, ZenithStreaming, ZenithJobs, ZenithCompression; none is "already in its `DEPENDS`") and its fixtures move from `Tests/WORLD/Golden/` and `Tests/WORLD/Migration/` to `Tests/Fixtures/WORLD/` and `Tests/Fixtures/Migration/WORLD/<Type>/`; CONTENT names the edges its move appends instead of "ZenithAuthoring's declared set"; ANIM's ticket Z2T-ANIM-021 calls its ZenithAnimation edge ANIM's own rule-4 append, not a growth-table entry brought forward (no stage columns exist) | § "ZenithAuthoring owners" |

## Library and file layout

### Targets

| Target | Directory | Kind | Links | Matrix |
|---|---|---|---|---|
| `ZenithAuthoring` | `Zenith2/Tools/Authoring/` | static tool library (no main); `ZenithAuthoring_Tests` from its `Tests/` (§ "ZenithAuthoring contract rules" rule 2) | ZenithCook, ZenithEditorCore, ZenithEditor, ZenithContent, `Zenith_Integration_ContentPipeline`, `Zenith_Integration_WorldAuthoring` | `ZENITH_TOOLS_HOST` |
| `ZenithEditorCore` | `Zenith2/Tools/EditorCore/` | static tool library (no main); `ZenithEditorCore_Tests` from its `Tests/` | ZenithEditor, ZenithContent, `Zenith_Integration_EditorMigration`, `Zenith_Integration_EditorSchema` | `ZENITH_TOOLS_HOST` |
| `ZenithEditor` | `Zenith2/Tools/ZenithEditor/` | executable, `ZenithEditor_Objects`, `ZenithEditor_Tests` | nothing links it | `ZENITH_TOOLS_HOST` |
| `EditorFixture` | `Zenith2/Games/EditorFixture/` | the test game of § "Test game" (`zenith_add_game`) | — | its `game.json` platforms: `windows` |

### ZenithEditorCore target

`Zenith2/Tools/EditorCore/` holds the session, the documents and their table, JSON edits, commands, the history and its operations, selection, the clipboard, the event bus, the world set and proxies, the projection, the play controller, the cook client and child process, the migration and schema models, the host paths, the host watcher, the storage roots and the document-type registration. Its public headers are the root headers of `Zenith2/Docs/Design/EDIT/Declarations.md` → "ZenithEditorCore headers"; they include no ImGui, STL or third-party header, so the integration suites include them. Its `Internal/` holds the implementations (the platform-independent change queue of the host watcher among them, `Internal/Zenith_EditorHostChangeQueue.h` and `.cpp`), `Internal/Windows/Zenith_EditorChildProcess_Windows.cpp`, `Internal/Windows/Zenith_EditorHostPaths_Windows.cpp`, `Internal/Windows/Zenith_EditorHostWatcher_Windows.cpp` (`ReadDirectoryChangesW`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Host watcher header"), `Internal/Windows/Zenith_EditorFileTime_Windows.cpp` and `Internal/Windows/Zenith_EditorFileMove_Windows.cpp` (`MoveFileExW` for asset operations and autosave renames). `ZenithEditorCore_Tests` (`Tools/EditorCore/Tests/`) links `ZenithEditorCore` and its `DEPENDS` alone, so it holds exactly the tests that need neither a runtime nor anything of `ZenithEditor`: the document, JSON-edit, history (over test-local commands and scripted operations), selection, clipboard, event, cook-client and migration-model tests of `Zenith2/Docs/Design/EDIT/Tests.md`, each over document-only sessions and in-memory or scratch storages. No `ZenithEditorCore_Tests` case creates a runtime: a runtime needs a composition, and the module catalogue, the ECS backends and the modules are `ZenithEditor`'s (`Zenith2/Docs/Design/EDIT/Tests.md` → "Test files" gives each test its executable).

### ZenithEditor target

One `zenith_add_tool` target, `ZenithEditor`, in `Zenith2/Tools/ZenithEditor/`: an executable (it has `Internal/Zenith_ZenithEditorMain.cpp`), its object library `ZenithEditor_Objects` (every other `Internal/**` source) and the test executable `ZenithEditor_Tests` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"). The executable has no root header: nothing links or includes it. Its headers live under `Internal/` and carry contract blocks by EDIT convention.

### Internal directories

| Directory | Contents |
|---|---|
| `Internal/Zenith_ZenithEditorMain.cpp` | `Zenith_EditorMain`: the composition root, startup and shutdown |
| `Internal/App/` | `Zenith_EditorApp` (the tool hooks and the runtime restart), `Zenith_EditorCommandLine`, `Zenith_EditorComposition` (the C++ composition hash of MOD's algorithm), `Zenith_EditorModuleCatalogue` (the one TU including every module and backend root header) |
| `Internal/Documents/` | the TERRAIN and UI document kinds and their factories, `Zenith_EditorPrefabProvider`, `Zenith_EditorTerrainSink`, `Zenith_EditorTerrainTileSource`, `Zenith_EditorTerrainOperation`, `Zenith_EditorUIOperation` (the SCENE and JSON kinds, the table and every other document type are `ZenithEditorCore`'s) |
| `Internal/Commands/` | the built-in commands (the history and the registry are `ZenithEditorCore`'s) |
| `Internal/Shell/` | the ImGui adapter, renderer, texture table, dock layout, panel host, notifications, shortcuts, preferences, icons, key table |
| `Internal/Viewport/` | viewports, cameras, picking, the pick-mesh cache, gizmos and their mathematics, snapping, the overlay extension and snapshot, the editor extractor |
| `Internal/Panels/` | the generic panels of `Zenith2/Docs/Design/EDIT/Panels.md` |
| `Internal/Pipeline/` | reimport, asset operations and their operations (`Zenith_EditorAssetSettingsOperation`, `Zenith_EditorFileOperation`), the preview host, thumbnails, the cache consumer (the cook client is `ZenithEditorCore`'s) |
| `Internal/HotReload/` | script, shader and schema reload |
| `Internal/Recovery/` | autosave, recovery, the session lock |
| `Internal/SourceControl/` | the git backend and status parser |
| `Internal/Automation/` | the script runner, the report, the live link, operation timing |
| `Internal/Extensions/` | `Zenith_EditorExtension`, the extension table |
| `Internal/Widgets/` | the graph canvas, timeline, curve editor, JSON document editor |
| `Internal/Formats/` | the editor file readers and writers and their version chains |
| `Internal/<Area>/` | `Terrain`, `Environment`, `Lighting`, `Materials`, `Animation`, `UI`, `VFX`, `Audio`, `Streaming`, `Save`, `Cinematics`, `2D`, `Character`, `World`, `Navigation`, `Package`, `Content` (the extension table of `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table") |
| `Internal/Windows/` | `Zenith_EditorSessionLock_Windows.cpp`, `Zenith_EditorLiveLinkSocket_Windows.cpp` (the child processes of the cook server, the shader watcher, git and the schema-lock run all use `ZenithEditorCore`'s `Internal/Windows/Zenith_EditorChildProcess_Windows.cpp`) |
| `Internal/Physics/` | the "Drop to rest" command over PHYS's isolated simulation (AuthoringTools.md "Physics tools"); PHYS's ragdoll preview panel |

### Shader sources

`Zenith2/Shaders/Editor/`: `ImGui.slang`, `Grid.slang`, `SelectionMask.slang`, `SelectionOutline.slang`, `Icons.slang`, `Gizmo.slang`, compiled offline by ZenithShaderCompiler into `ZenithShadersGenerated` (Z2-D-025), their generated parameter headers included by `Internal/Shell/` and `Internal/Viewport/`.

### Tests directory

`Zenith2/Tools/ZenithEditor/Tests/` with the files of `Zenith2/Docs/Design/EDIT/Tests.md` → "Test files", `Tests/<Area>/` for area-owned panels, and `Tests/Fixtures/` (`Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures"); `Zenith2/Tools/EditorCore/Tests/` with the core files of the same table; `Zenith2/Tools/Authoring/Tests/Zenith_AuthoringLibrary_Tests.cpp` (EDIT's one `ZenithAuthoring_Tests` file; every other file there is an owner's, § "ZenithAuthoring contract rules" rule 2). No `Tests/Support/` source is compiled in either ([Z2-N-ARCH-255], ruling X184): shared helpers are header-only, under `Tools/EditorCore/Tests/Support/`.

### Test game

`Zenith2/Games/EditorFixture/` is a registered game (`zenith_add_game(EditorFixture)`, platforms `windows`), owned by EDIT, used by every EDIT test that needs a real game: the real cook server, child editor processes, play worlds with modules, the world-gating tests, the authoring tools and the scene-publish check. Because it is a game, the configure writes its `composition.json` ([Z2-N-ARCH-257] needs no generator for it) and builds its Player. Its content and module selection are `Zenith2/Docs/Design/EDIT/Tests.md` → "Fixtures"; tests that mutate sources never touch the tracked directory: they copy it into `${ZENITH_OUT}/test-fixtures/EDIT/Games/EditorFixture/` first (the fixture-setup case of the same heading).

### Supported matrix

`ZENITH_TOOLS_HOST` (the `win` configure preset, built by the `win-tools` build preset) for all three targets: the editor, its libraries and their tests build and run on Windows only; no Android, `win-asan` or `linux-tsan` build exists for them. The libraries the editor calls carry their own sanitizer coverage.

### Untracked editor files

`Games/<Name>/.editor/` and `${ZENITH_OUT}/EditorCache/` are never tracked. Every per-user and recovery file of every storage root lives under the game's `.editor/`, never inside an engine library:

| Path | Holds | Root |
|---|---|---|
| `Games/<Name>/.editor/user/` | `session.json`, `layout.json`, `endpoint.json` | the session |
| `Games/<Name>/.editor/session.lock` | the session lock | the session |
| `Games/<Name>/.editor/autosave/`, `.editor/recovery.json`, `.editor/trash/` | the game root's autosaves, recovery manifest and trash | the game root |
| `Games/<Name>/.editor/roots/<Library>/autosave/`, `.editor/roots/<Library>/recovery.json` | an engine content root's autosaves and manifest (`<Library>` the root's library name, `ZenithPlatform`) | the engine root `Zenith2/Engine/<Library>/Internal/Content/` |

An engine root therefore never gains a directory: `Zenith2/Engine/<Lib>/Internal/Content/` holds only the library's content, which SourceLayout and the cook scan as before, and no `.gitignore` row for engine roots is needed. Engine-root documents are never deleted through the editor (they are read-only unless `editor.engineContent.writable`, and the trash serves the game root alone).

MOD's "Game directory entries" (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game directory entries") admits `.editor` (directory, optional, gitignored, never a content root) and `<W>.editorscript.json` in `Workload` through Z2T-MOD-070 (MOD follow-up 4), and the nested `Zenith2/.gitignore` gains `Games/*/.editor/`. Z2T-MOD-070 is in the Stage 3 sprint, and every EDIT ticket that writes under `.editor/` is blocked by it: the trash of the import and terrain-wizard undo lands at Stage 3 (Z2T-EDIT-057, -068), and the session file, the layout file, the endpoint file, autosave, the recovery manifest and the session lock land at Stage 4 (`Zenith2/Docs/Design/EDIT.md` → "Stage 4"). The Stage 3 editor writes nothing else under `.editor/`: it opens with the default layout and `game.json`'s startup scene, and shutdown step 5 is skipped (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Shutdown sequence").

## Public API

### API rules for a tool

- Every fallible function returns `Zenith_Result<T, Zenith_EditorError>`; untrusted input (scripts, the clipboard, files, live-link lines, child-process output) never asserts (Z2-D-045).
- Every type carries a contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type": a requirement for the public headers of `ZenithEditorCore` (`ApiContractCheck`), and EDIT convention for the `Internal/` headers of `ZenithEditor`.
- Dependencies are passed in constructors; the editor holds no mutable global and no service lookup beyond the hooks its main installs (Z2-D-046). The `Zenith_ToolServices` references are filled once by `Zenith_RuntimeToolAccess::Create` and handed by the composition root to the constructors that need them (Foundation.md "Runtime tool access").
- The STL is allowed inside the tool's `Internal/` sources (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy"), but never in a `ZenithEditorCore` header and never `std::unique_ptr`/`shared_ptr` (Z2-D-040); ImGui headers stay in `Internal/` of `ZenithEditor`.

### Header index

The declarations are `Zenith2/Docs/Design/EDIT/Declarations.md` → "ZenithEditorCore headers" (the public headers of `Zenith2/Tools/EditorCore/`) and `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "ZenithEditor internal headers"; every named EDIT type, with its header and declaring heading, is in `Zenith2/Docs/Design/EDIT/Declarations.md` → "Declaration index". API-only slices 1 and 2 of § "API-only slices" are exactly the headers that index lists for them.

### Editor configuration keys

<!-- elaboration -->
Preferences (per user, `Zenith2/Docs/Design/EDIT/Formats.md` → "Preferences file"), with their defaults and ranges:

| Key | Default | Range |
|---|---|---|
| `editor.frame.syncBudgetUs` | 4,000 | 500 … 50,000 |
| `editor.documents.maxOpen` | 512 | 16 … 4,096 |
| `editor.projection.arenaBytes`, `editor.projection.bufferBytes` | 64 MiB, 16 MiB | 4 … 1,024 MiB, 1 … 256 MiB |
| `editor.projection.verifyEveryN` | 0 | 0 … 10,000 |
| `editor.undo.maxRecords`, `editor.undo.maxBytes`, `editor.undo.maxRecordBytes` | 1,000; 512 MiB (row ED24 sets the value); 256 MiB | 10 … 100,000; 16 MiB … 16 GiB; 1 MiB … 4 GiB |
| `editor.undo.maxTerrainRecordBytes` | Z2-B-EDIT-011, until allocated 256 MiB | 1 MiB … 4 GiB |
| `editor.undo.mergeWindowMs` | 500 | 0 … 5,000 |
| `editor.history.asyncTimeoutSeconds` | 300: an inactivity timeout, measured since the cook server last sent any line while the record's request is queued or running (Foundation.md "Asynchronous changes") | 10 … 3,600 |
| `editor.selection.maxObjects` | 100,000 | 1,000 … 1,000,000 |
| `editor.play.maxWorlds` | 4 | 1 … 8 |
| `editor.play.saveBeforePlay`, `editor.play.escapeReleasesFocus` | false, false | |
| `editor.preview.maxWorlds` | 8 | 1 … 32 |
| `editor.thumbnails.perFrame`, `editor.thumbnails.warmupFrames`, `editor.thumbnails.diskMaxBytes` | 2, 4, 512 MiB | 1 … 16, 1 … 60, 0 … 64 GiB |
| `editor.autosave.intervalSeconds`, `editor.autosave.frameBudgetUs` | 120, 2,000 | 0 … 3,600, 200 … 20,000 |
| `editor.cook.debounceMs`, `editor.cook.maxQueued` | 250, 256 | 50 … 5,000, 16 … 4,096 |
| `editor.assetOps.batchThreshold` | 64 | 1 … 100,000 |
| `editor.migration.planDocumentsPerFrame` | 64 | 1 … 4,096 |
| `editor.camera.scheme`, `editor.camera.lookDegreesPerCount`, `editor.camera.flySpeed`, `editor.camera.boostFactor`, `editor.camera.frameDurationMs` | unity, 0.15, 8, 4, 200 | unity or unreal; 0.01 … 2; 0.05 … 2,000; 1 … 20; 0 … 2,000 |
| `editor.gizmo.sizePixels` | 110 | 40 … 400 |
| `editor.grid.fadeDistance` | 500 | 10 … 100,000 |
| `editor.shell.maxDrawBytes`, `editor.shell.maxTextures` | 32 MiB, 4,096 | 4 … 256 MiB, 256 … 65,536 |
| `editor.overlay.maxVertices`, `editor.overlay.maxSelectedInstances` | 262,144; 65,536 | |
| `editor.viewports.maxScene` | 4 | 1 … 8 (also the proxy entities per edit world) |
| `editor.panel.slowDrawMs` | 8 | 1 … 100 |
| `editor.log.maxLines`, `editor.assetBrowser.cachedPages` | 20,000; 512 | |
| `editor.animation.compileBudgetMs` | 50 | 5 … 1,000 |
| `editor.shaders.watch`, `editor.engineContent.writable`, `editor.inspector.showHidden`, `editor.icons.showEmpty` | true, false, false, false | |
| `editor.externalEditor`, `editor.git.path` | `code -g %file%:%line%`, `git` | |

### Handle kinds used

| Handle (`Zenith2/Docs/Contracts.md` → "Handle kinds") | Use in EDIT |
|---|---|
| persistent IDs | documents, authoring objects, selection, clipboard, session files |
| world handles (entities) | within one editor frame only, resolved from persistent IDs |
| shared asset handles | textures shown in ImGui (pinned), preview assets |
| device handles | tool textures through the renderer's API |
| operation handles | cook requests, readbacks, background operations |

## Data structures and formats

### In-memory structures

| Structure | Size or bound | Where |
|---|---|---|
| history record | its arena plus terrain edits; bounded by `editor.undo.maxRecordBytes` | Foundation.md "Command records" |
| history | `editor.undo.maxRecords` records and `editor.undo.maxBytes` bytes | Foundation.md "History memory bound" |
| selection | up to `editor.selection.maxObjects` refs of 64 bytes (kind 1 padded to 8 by the 8-byte alignment of the members after it, document 16, object 16, sub-ID 16, sub 8) with a hash set beside it | Foundation.md "Selection model" |
| pending record | at most one asynchronous record in flight per history | Foundation.md "Asynchronous changes" |
| editor proxy entities | `editor.viewports.maxScene` per edit world | Foundation.md "Editor proxy entities" |
| draw snapshot | two slots, each at most `editor.shell.maxDrawBytes` | Viewports.md "ImGui renderer" |
| overlay snapshot | two slots, 16-byte overlay vertices | Viewports.md "Overlay records" |
| pick-mesh cache | decoded positions, indices and a BVH per mesh, LRU within Z2-B-EDIT-001 | Viewports.md "Precise mesh picking" |
| texture table | `editor.shell.maxTextures` slots | Viewports.md "Editor texture references" |

### Formats summary

| Format | Kind | Specified in |
|---|---|---|
| session, layout, preferences, recovery, endpoint, trash, clipboard | versioned JSON (`zenith2.editor-*`) | `Zenith2/Docs/Design/EDIT/Formats.md` |
| autosave (`ZAUT`), thumbnail (`ZTHB`) | versioned binary with codec ID and SHA-256 | `Zenith2/Docs/Design/EDIT/Formats.md` → "Autosave file", "Thumbnail file" |
| automation script and report | versioned JSON | `Zenith2/Docs/Design/EDIT/Automation.md` → "Automation script format", "Automation report" |
| the documents the editor edits | their owners' formats, written by their owners' canonical writers | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rules for files the editor writes" |

## Algorithms and state machines

### Algorithm index

| Algorithm | Heading |
|---|---|
| startup, composition, frame, shutdown | Foundation.md "Startup sequence", "Editor composition", "Editor frame", "Shutdown sequence" |
| projection and reconcile | Foundation.md "Projection per edit kind", "Reconcile algorithm" |
| the play copy | Foundation.md "Play copy normalisation" |
| execute, undo, redo, transactions, merging, eviction, pruning, asynchronous changes | Foundation.md "Executing a command", "Undo and redo", "Transactions", "Interactions and merging", "History memory bound", "Closing a document and its history", "Asynchronous changes" |
| the runtime restart | Pipeline.md "Runtime restart" |
| JSON edits and inverses | Foundation.md "JSON edit inverses" |
| camera navigation and framing | Viewports.md "Fly navigation", "Orbit pan and dolly", "Framing" |
| picking | Viewports.md "CPU bounds picking", "Precise mesh picking", "Marquee selection", "Gizmo handle picking" |
| gizmo mathematics and snapping | Viewports.md "Translate drag mathematics", "Rotate drag mathematics", "Scale drag mathematics", "Applying a gizmo delta", "Grid snapping", "Rotation snapping", "Scale snapping", "Surface snapping", "Vertex snapping" |
| overlays | Viewports.md "Grid pass", "Selection outline pass" |
| reimport, asset operations, reloads, schema change, migration, autosave, recovery | Pipeline.md |
| terrain strokes | AuthoringTools.md "Sculpt and paint strokes" |

### State machine index

| State machine | Heading |
|---|---|
| play per world | Foundation.md "Play state machine" |
| document states | Foundation.md "Document states" |
| history records | Foundation.md "Asynchronous changes" |
| cook client | Pipeline.md "Client state machine" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| session, documents, history, operations, selection, panels, commands, projection, proxies, play controller, preview host, migration and schema models | `MAIN_THREAD` (editor frame steps 2, 4, 6) |
| `Zenith_EditorTextureTable` | `ANY_THREAD_EXCEPT(Resolve)`: `Resolve` on `Flux.BuildGraph` |
| overlay extension, ImGui renderer (`AddPasses`) | the `Flux.BuildGraph` node, reading only immutable snapshots |
| editor extractor body | its EPILOGUE stage-12 node, reading only the immutable frame snapshot |
| cook client and child process queues | lock-free bounded queues between the pipe threads and the main thread |
| script world events | recorded by the runtime's listener on the observing thread, delivered to the editor on the main thread through the tool hooks (Foundation.md "Runtime tool access") |

### Threads EDIT owns

| Thread | Work | Joined |
|---|---|---|
| `EditorChildOut`, `EditorChildErr`, `EditorChildIn` (one set per child process: the cook server, the shader watcher, a git command, a schema-lock run) | the child's pipes | when the child's owner destroys it: the cook server's and the shader watcher's at shutdown step 6, git's at step 3, a schema-lock run's when it exits |
| `EditorAutosave` | autosave writes | shutdown step 4 |
| `EditorHostWatch` (one per host watcher: each storage root, the shader reload journal) | `ReadDirectoryChangesW` completions, coalesced into the watcher's bounded hand-off queue (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Host watcher header") | by the watcher's destructor, shutdown step 7; a runtime restart does not touch it |
| `EditorSourceControl` | git commands, one at a time | shutdown step 3, after the running command ends or its 30-second timeout terminates it |
| `EditorLiveLink` | the endpoint's accept and socket reads (lines are handed to the main thread) | shutdown step 3 |
| `EditorContentBatch` (one per running CONTENT batch, Stage 4) | the `Zenith_ContentBatchOperation` above `editor.assetOps.batchThreshold` documents that it creates and owns (it calls `Plan` or `Open`, both `Run` calls and `Rollback`: CONTENT's owner-thread rule, `Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Batch API"); its cook requests forwarded to the main thread through a channel that never waits (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Content cook channel header") | when the batch finishes or rolls back; shutdown step 1 waits for it as for a pending record |

None of them is a job worker; none takes a `Zenith_Mutex` (`EditorContentBatch` waits on no primitive: CONTENT's `Run` sleeps 1 ms between polls of the channel, and the thread reads the main thread's verdict on the planned documents the same way).

### Frame-graph nodes EDIT registers

| Node | Subgraph | Declared access |
|---|---|---|
| `Editor.Extract` per edit, play and preview world | EPILOGUE stage 12 of the world's group, added through `Zenith_RuntimeToolAccess::AddToolExtractor` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access") | write: the world's render-world section; reads: none in ECS |
| `EditorImGui` pass (shell view), the overlay passes | inside `Flux.BuildGraph` through view extensions | render-graph resources only |

### Lock-order entries

None: EDIT's cross-thread hand-offs are `Zenith_BoundedQueue`s (no lock-order row, `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue") and child processes.

### Cancellation and shutdown

Cook requests are cancelled with the protocol's `cancel`; background operations (regeneration, migration planning, thumbnails) hold operation handles cancelled at shutdown and on supersession; a pending asynchronous record is cancelled by its inactivity timeout (`editor.history.asyncTimeoutSeconds`) or by the status bar's cancel, its outcome is resolved from the server's state before any compensation, and shutdown keeps running editor frames until it settles or its bounded deadline passes (Foundation.md "Asynchronous changes", "Shutdown sequence"); shutdown is `Zenith2/Docs/Design/EDIT/Foundation.md` → "Shutdown sequence"; every thread is joined by its owner before the session is destroyed.

### Error enumeration

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorError.h
enum Zenith_EditorErrorCode : uint8
{
	EDITOR_ERROR_USAGE = 1,                  // command line
	EDITOR_ERROR_OUT_OF_MEMORY,
	EDITOR_ERROR_COMPOSITION_STALE,          // composition.json missing, stale or naming an unknown module
	EDITOR_ERROR_RUNTIME,                    // the runtime tool access failed (the Zenith_RuntimeError is logged)
	EDITOR_ERROR_DOCUMENT,                   // a Zenith_DocumentError (logged with path and position)
	EDITOR_ERROR_SCENE,                      // a Zenith_SceneError
	EDITOR_ERROR_DOCUMENT_LIMIT,
	EDITOR_ERROR_DOCUMENT_NOT_OPEN,
	EDITOR_ERROR_DOCUMENT_READ_ONLY,
	EDITOR_ERROR_DOCUMENT_CONFLICT,
	EDITOR_ERROR_STORAGE_LOCKED,
	EDITOR_ERROR_JSON_EDIT,
	EDITOR_ERROR_PROJECTION,
	EDITOR_ERROR_COMMAND_UNKNOWN,
	EDITOR_ERROR_COMMAND_ARGS,
	EDITOR_ERROR_COMMAND_DISABLED,
	EDITOR_ERROR_COMMAND_REENTRANT,
	EDITOR_ERROR_NOTHING_TO_UNDO,
	EDITOR_ERROR_NOTHING_TO_REDO,
	EDITOR_ERROR_HISTORY_FENCE,
	EDITOR_ERROR_UNDO_FAILED,
	EDITOR_ERROR_SELECTION_LIMIT,
	EDITOR_ERROR_NOT_FOUND,
	EDITOR_ERROR_CLIPBOARD_FORMAT,
	EDITOR_ERROR_PLAY_STATE,
	EDITOR_ERROR_PLAY_WORLD_LIMIT,
	EDITOR_ERROR_PLAY_START_FAILED,
	EDITOR_ERROR_SCHEMA_INVALID,
	EDITOR_ERROR_SCHEMA_PLAY_ACTIVE,
	EDITOR_ERROR_MIGRATION_BLOCKED,
	EDITOR_ERROR_COOK_SERVER_UNAVAILABLE,
	EDITOR_ERROR_COOK_SERVER_RESTARTED,
	EDITOR_ERROR_COOK_PROTOCOL,
	EDITOR_ERROR_COOK_BUSY,
	EDITOR_ERROR_COOK_REQUEST_FAILED,
	EDITOR_ERROR_COOK_CANCELLED,
	EDITOR_ERROR_SCRIPT_COMPILE,
	EDITOR_ERROR_AUTOSAVE_IO,
	EDITOR_ERROR_RECOVERY_DAMAGED,
	EDITOR_ERROR_SOURCE_CONTROL,
	EDITOR_ERROR_AUTOMATION_SCRIPT,
	EDITOR_ERROR_AUTOMATION_ASSERT,
	EDITOR_ERROR_AUTOMATION_TIMEOUT,
	EDITOR_ERROR_AUTOMATION_BUSY,
	EDITOR_ERROR_ENDPOINT_AUTH,
	EDITOR_ERROR_EXTENSION_DUPLICATE,
	EDITOR_ERROR_EXTENSION_LIMIT,
	EDITOR_ERROR_TEXTURE_LIMIT,
	EDITOR_ERROR_PREVIEW_UNAVAILABLE,        // headless or no renderer
	EDITOR_ERROR_ENVIRONMENT,                // a Zenith_EnvironmentError (terrain)
	EDITOR_ERROR_HISTORY_PENDING,            // an asynchronous record is pending (Foundation.md "Asynchronous changes")
	EDITOR_ERROR_HISTORY_ASYNC_TIMEOUT,      // a pending record's request outlived editor.history.asyncTimeoutSeconds
	EDITOR_ERROR_RUNTIME_RESTART,            // the runtime could not be created again (Pipeline.md "Runtime restart")
	EDITOR_ERROR_HOST_PATH,                  // the build tree, the games root or a host file was not found
	EDITOR_ERROR_PHYSICS_SANDBOX,            // PHYS's isolated simulation refused or failed ("Drop to rest")
	EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN,     // an editor-originated write would overwrite a dirty open document (Foundation.md "Editor-originated writes")
	EDITOR_ERROR_DEVICE_LOST,                // the runtime requested the device-loss exit (Foundation.md "Device loss")
	EDITOR_ERROR_SCHEMA_QUEUE_FULL,          // a schema apply over QueueSchemaJson's 64 files or 8 MiB (RUNTIME_ERROR_QUEUE_FULL)
};

struct Zenith_EditorError                      // the error form of [Z2-N-CONTRACT-480]: code first, 8 bytes, no string
{
	Zenith_EditorErrorCode m_eCode;
	uint8 m_uWrappedCode = 0;                 // the wrapped area's code (document, scene, cook, environment), or 0
	uint16 m_uReserved = 0;
	uint32 m_uDetail = 0;                     // an index (document, step, entry) or 0xFFFFFFFF
};
```

Process exit codes are `Zenith_ExitCode`'s values: 0 normal; 1 a runtime error, the session lock held, a failed root, or a runtime restart that failed after its autosave; 2 usage; 3 composition, registration or a missing build tree (`EDITOR_ERROR_HOST_PATH`); 4 device or world creation; 5 a failed automation assertion; 6 device lost; 124 the watchdog.

### Luau errors versus asserts

The editor raises no Luau error of its own; console evaluation and hot reload return SCRIPT's `Zenith_ScriptError` values, shown as diagnostics. Every input the editor reads from outside its own code (documents, scripts, the clipboard, child processes, the live link, per-user files) is validated and never asserted on.

### Determinism obligations

The editor never changes a play world's simulation except through the documented paths (input in the focused game viewport, inspector edits as commands, pause and step), and a play world starts from the play copy of Foundation.md "Play copy normalisation", whose content is a function of the documents alone: the capture of the edit world ([Z2-N-ARCH-766]) with the editor proxy entities excluded and every edit-world-only state replaced by the documents' values. So a play world given the same documents, the same inputs and the same edits is deterministic per Z2-D-019, whatever the edit world ticked or the author scrubbed before Play; `Test_EditorPlay_CaptureExcludesEditWorldOnlyState`, `Test_EditorPlay_TwoWorldsIndependent` and `Test_EditorHeadless_PlayRunsTicks` check it. Every play world's seed is a function of the edit world's seed alone (no play ordinal, Foundation.md "Starting play" step 4), so a world started second runs exactly as a fresh session's first. Authoring IDs the editor generates (new entities, pasted and duplicated subtrees, new documents) come from the session's random source, seeded by `--random-seed` or the automation script's `seed` when given and from the operating system's entropy otherwise, so a seeded session (every automation and test session) generates the same IDs for the same command sequence; the editor proxy entities draw no ID from it (their IDs are derived, Foundation.md "Editor proxy entities"), so a windowed and a headless run of one script generate the same IDs.

## Diagnostics and profiling

### Counters

| Counter | Meaning |
|---|---|
| `editor.undo.records`, `editor.undo.bytes`, `editor.undo.evicted`, `editor.undo.fences` | history state |
| `editor.documents.open`, `editor.documents.dirty` | document table |
| `editor.projection.applies`, `editor.projection.reconciles`, `editor.projection.errors` | projection |
| `editor.play.worlds`, `editor.play.ignored_events` | play |
| `editor.play.copy_dropped_entities`, `editor.play.copy_normalised_sets`, `editor.play.copy_drift` | the play copy (Foundation.md "Play copy normalisation") |
| `editor.undo.pending`, `editor.undo.async_failures`, `editor.undo.compensation_fences`, `editor.undo.resolutions` (by outcome: completed, no effect, partial, changed elsewhere, unknown) | asynchronous records |
| `editor.documents.self_writes_ignored`, `editor.documents.plan_refusals` | editor-originated writes (Foundation.md "Editor-originated writes") |
| `editor.watcher.dropped`, `editor.watcher.high_water`, `editor.watcher.rescans` | the host watchers' hand-off queues (Foundation.md "Host watchers") |
| `editor.caches.deferred_releases` | cache entries the governor marked and step 6 released (Panels.md "Thumbnail cache") |
| `editor.events.dropped`, `editor.script.events_dropped` | the event bus; script reload events the editor could not time |
| `editor.cook.state`, `editor.cook.queued`, `editor.cook.restarts` | cook client |
| `editor.autosave.writes`, `editor.autosave.skipped` | autosave |
| `editor.shell.draw_lists_dropped`, `editor.shell.texture_misses`, `editor.overlay.selection_truncated` | shell |
| `editor.gizmo.clamped` | gizmo |
| `editor.liveLink.rejected` | live link |
| `editor.caches.bytes` | editor caches (`MEMORY_SUBSYSTEM_EDITOR_CACHES`) |

### Profiling zones

`Editor.Frame` (children `Editor.Frame.Input`, `.Panels`, `.Commands`, `.Projection`, `.PrologueEnd`, `.FrameEnd`), `Editor.Panel.<type>`, `Editor.Cook.Poll`, `Editor.Reconcile`, `Editor.Autosave.Serialize`, `Editor.Picking`, `Editor.Extract`; they feed DEVT's overlay and the `editor` provider (`Zenith2/Docs/Design/EDIT/Panels.md` → "Profiler integration").

### Log categories

`editor`, `editor.startup`, `editor.document`, `editor.undo`, `editor.play`, `editor.cook`, `editor.cook.server` (the server's stderr), `editor.reload`, `editor.migration`, `editor.recovery`, `editor.runtime` (the runtime restart), `editor.sourceControl`, `editor.automation`, `editor.liveLink`.

### On-screen profiler rows

The `editor` provider's rows: the editor frame's step times, the slowest three panels' draw times, the history bytes, the cook queue depth.

### Debug views and dumps

Scene views offer every debug view the renderer, LIGHT, VFX and DEVT register (the view menu); `editor.commands.dump`, the automation report, the migration preview report and the recovery manifest are the editor's dumps.

## Authoring and cooking

### What EDIT authors

EDIT authors no document type of its own: every document it writes is an owner's, written by the owner's canonical writer; its own files are per-user, untracked editor state (`Zenith2/Docs/Design/EDIT/Formats.md`). It is the authoring surface of every visual-authoring item (Z2-D-079) through the tools of `Zenith2/Docs/Design/EDIT/AuthoringTools.md`.

### Cook integration

The editor cooks nothing in process: imports, reimports, play-set cooks, bakes, validations and `.meta` migrations are requests to `ZenithCook --serve` (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Cook client"); the build cook and the server exclude each other through the output-tree lock.

### Migration hooks

EDIT registers every owner's document types (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Storage roots and the migration registry") and presents migration (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow"); its own per-user files migrate through the editor's file chains (`Zenith2/Docs/Design/EDIT/Formats.md` → "Common rules").

## Failure handling

### Failure table

| Failure | Behaviour |
|---|---|
| missing or stale `composition.json` | exit code 3 with the reconfigure message |
| another session holds the game | exit code 1 naming the holder |
| a corrupt or conflicting migration journal | that root read-only, the journal named |
| a document that fails to parse, has a chain gap or a failed step | not opened; the error with path and position; other documents unaffected |
| a newer-version document | opened read-only |
| an external change of a dirty document | CONFLICTED with keep-mine and take-disk |
| a save refused (lock, stale, low disk) | documents stay dirty; the reason shown |
| a command failing mid-way | every applied change reverted; nothing recorded |
| an asynchronous change failing (a server error, a restart mid-request, an inactivity timeout, a cancel) | the outcome resolved from the server's state, then compensated in reverse; a transient failure leaves the history as before, a permanent one or a failed compensation leaves a fence, and no failure discards an unsaved edit (Foundation.md "Asynchronous changes") |
| an editor-originated write meeting a dirty open document | refused before any change with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` naming the document (Foundation.md "Editor-originated writes") |
| an undo failing | compensated; fenced for a permanent cause, kept for a transient one; reported |
| the runtime failing to restart on a schema undo | every dirty document autosaved, exit code 1 (Pipeline.md "Runtime restart") |
| the history over its bound | oldest records evicted; oversize records fenced |
| a projection error | reconcile of the document |
| a play-start failure, a halted script world | back to editing; the edit world untouched |
| the cook server crashing, locked out, incompatible | restart with back-off; 2-second retry with a status; stop with a rebuild message |
| a shader layout change or compile error | old blobs kept; the rebuild or the errors shown |
| an autosave write failure | a warning; editing continues |
| a crash | autosaves and journals recovered at the next start |
| git missing, timing out or failing | the operation fails with the git output; badges hidden without a repository |
| an allocation failure | `EDITOR_ERROR_OUT_OF_MEMORY` for the operation; the general heap's OOM handler for infallible paths |
| device loss | the renderer requests shutdown with `EXIT_CODE_DEVICE_LOST`; `RunOneFrame` returns false and `GetRequestedExitCode` reports it; `Zenith_EditorApp::Run` writes an autosave of every dirty document within 7,000 ms, skips the dirty-document prompt, terminates git, the cook server (after 500 ms) and the shader watcher instead of waiting for them, and returns exit code 6 within 9 s of the loss, inside the renderer's 10-second fatal window (Foundation.md "Device loss") |
| a host watcher that cannot be created, or a watcher's queue overflowing | a warning and the status-bar line "File watching unavailable for <directory>", editing continues; an overflow is a rescan of the watched directory, never a lost change (Foundation.md "Host watchers") |
| memory pressure | the editor caches shed by the governor's levels |
| an untrusted script, clipboard text, live-link line or per-user file | validated; refused with its error; never asserted on |

## Test strategy

### Test kinds

Unit and component tests of every subsystem; property tests of undo sequences; process-level tests launching child editors and the real cook server (`z2-stress`); headless editing tests; render tests on WARP with structural assertions (no golden images); integration tests and the walkthrough rehearsals. The full list with the case each asserts is `Zenith2/Docs/Design/EDIT/Tests.md`.

### Gate steps per test group

All EDIT tests run in the `Test` gate step in the `win` configure preset (the tools' matrix). Render tests carry the doctest suites `z2-stress` (the 600-second timeout, [Z2-N-CONTRACT-409]) and `editor-render` (the label that selects them, [Z2-N-ARCH-251]) and create their device on WARP (`--set rhi.backend=d3d12 --set rhi.adapter=software`) and lavapipe (`--set rhi.backend=vulkan --set rhi.adapter=software`, with the shared gate environment's `VK_DRIVER_FILES`; `Zenith2/Docs/Design/RHI.md` → "Required test groups"), so they need no GPU and no `needs-gpu` label (ruling X170). The Stage 3 and Stage 4 acceptance suites re-run them and add the `needs-human` W-Editor measurements and walkthrough runs. `Libgraph`, `SourceLayout`, `CompileCoverage`, `Complexity`, `ForbiddenCheck`, `Format`, `ApiContractCheck` and `TestSeparationCheck` cover the three targets' sources; `ShaderChecks` covers `Zenith2/Shaders/Editor/`.

## Budget rows

### Rows EDIT owns

| Row | Name | Unit | Producer | Draft-ID (registered) |
|---|---|---|---|---|
| Z2-B-EDIT-001 | Editor caches | MB | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-002 | Script hot reload | s | Stage 1 W-Editor row ticket for script hot reload | `Z2T-EDIT-002` |
| Z2-B-EDIT-003 | Shader blob reload | s | Stage 1 W-Editor row ticket for shader blob reload | `Z2T-EDIT-003` |
| Z2-B-EDIT-004 | Asset reimport | s | Stage 1 W-Editor row ticket for asset reimport | `Z2T-EDIT-004` |
| Z2-B-EDIT-005 | Scene open | s | Stage 1 W-Editor row ticket for scene open | `Z2T-EDIT-005` |
| Z2-B-EDIT-006 | Play-in-editor start | s | Stage 1 W-Editor row ticket for play-in-editor start | `Z2T-EDIT-006` |
| Z2-B-EDIT-007 | Walkthrough build a scene | min | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-008 | Walkthrough author a prefab variant | min | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-009 | Walkthrough iterate a script | min | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-010 | Walkthrough reimport an asset | min | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-011 | Terrain edit delta memory per undo step | MB | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |
| Z2-B-EDIT-012 | Terrain sculpt preview latency | ms | Stage 1 per-system budget ticket for EDIT | `Z2T-EDIT-001` |

### Producers EDIT schedules

<!-- elaboration -->
The six producers are Stage 1 `needs-human` TASKs of kind `budget` (`Zenith2/Docs/Budgets.md` → "Per-system producer shape": EDIT has no harness half; each ruling cites reference-engine or published data), sprint `Stage 1 — Architecture validation`, fix version `Z2 Stage 1`, titled exactly with their Budgets producer names ([Z2-N-BUDGET-462]), each writing its rows' values and appending its allocating decision to `Zenith2/Docs/DecisionLog.md` in its own diff. Ruling Q85 (b) drafted them as a mini-batch ahead of EDIT's full ticket batch: their Draft-IDs are claimed in `tickets/_shared/draft_ids.json` and each `Z2-B-EDIT-` row has its `budgets` entry in `tickets/_shared/producers.json`, so consumers block on them by ID. SCENE's Z2T-SCENE-087, -090 and -092 and CORE's Z2T-CORE-059 name **Z2-B-EDIT-005** and block on `Z2T-EDIT-005`; ASSET's revalidation (Z2T-ASSET-099) names **Z2-B-EDIT-004** and **Z2-B-EDIT-010** and blocks on `Z2T-EDIT-004` and `Z2T-EDIT-001`; SCRIPT names **Z2-B-EDIT-002** (`Z2T-EDIT-002`); RPROD's RPB20 consumer names **Z2-B-EDIT-003** (`Z2T-EDIT-003`); ENV names **Z2-B-EDIT-011** and **Z2-B-EDIT-012** (`Z2T-EDIT-001`). The per-system ticket is blocked by the CORE, JOBS and RPROD rulings ([Z2-N-BUDGET-023]); the five W-Editor row tickets are blocked by nothing (`Zenith2/Docs/Budgets.md` → "Producer tickets").

### Rows EDIT is measured against

Z2-B-ASSET-011 "Incremental no-op cook time per slice" (part of Z2-B-EDIT-004, [Z2-N-BUDGET-327]); Z2-B-GRAPH-001 "CPU frame-graph schedule recompile" (W-Editor, the editor's recompile triggers); the measurement procedures MP-EDIT and MP-WALKTHROUGH (`Zenith2/Docs/Budgets.md` → "Editor iteration rows"). The editor is not measured against the Player memory totals ([Z2-N-BUDGET-143]).

### Pending row needs

Cross-doc item X260 carries these four rows into `Zenith2/Docs/Budgets.md`; until it lands they have no `Z2-B-` ID and no EDIT requirement cites them as a budget:

| Row | Name | Unit | Measured | Producer |
|---|---|---|---|---|
| ED21 | Editor frame CPU | ms | MP-CPUZONE of `Editor.Frame` with W-Editor's scene open, median frame | Stage 1 per-system budget ticket for EDIT |
| ED22 | Editor projection apply CPU per 1,000 changes | ms | MP-EDIT | Stage 1 per-system budget ticket for EDIT |
| ED23 | Animation graph preview latency (row AN12) | ms | MP-EDIT from a graph edit to the first frame drawing the recompiled pose | Stage 1 per-system budget ticket for EDIT (the one producer X260 settles for X212 too: the five W-Editor row tickets are fixed by `Zenith2/Docs/Roadmap.md` → "Stage 1 deliverables", deliverable 12) |
| ED24 | Editor undo history memory | MB | MP-SUBMEM of the history (the value of `editor.undo.maxBytes`) | Stage 1 per-system budget ticket for EDIT |

When X260 lands, the core-doc patch that adds the rows extends `Z2T-EDIT-001`'s Goal and Definition of Done with the four new IDs in the same patch, so no row is left without its allocating ticket.

## Stage plan

### Stage 1

The six budget producers of § "Producers EDIT schedules" (`Z2T-EDIT-001` … `-006`); nothing else (the editor exists from Stage 3).

### Stage 2 pull-forward

Per `Zenith2/Docs/Roadmap.md` → "Pull-forward rule", the library-creation ticket of § "Library creation ticket" (`Z2T-EDIT-007`, fix version `Z2 Stage 2`) comes first and creates the empty `ZenithAuthoring` and `ZenithEditorCore` targets every owner's authoring ticket blocks on; then the editor foundation that depends on SCENE and Reflect alone is scheduled in the Stage 2 sprint with fix version `Z2 Stage 3`: `ZenithEditorCore` growing by § "Dependency growth rule", holding the document table and JSON edits, the command system and registry, the history (records, transactions, merging, the memory bound, fences, dirty tracking, the operation interface with its synchronous kinds), the selection and the clipboard over in-memory documents, with their unit and property tests in `ZenithEditorCore_Tests`. The asynchronous record path and the cook client follow at Stage 3. The slice's sessions are document-only and carry a type registry their caller owns (`Zenith_EditorSessionDesc::m_pxTypes` and `m_pxStrings`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Session description"), so the history and the clipboard work over scene documents without a runtime.

### Stage 3

The editor process and composition (over MOD's runtime tool access, `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access"), the edit, play and preview worlds with their purposes (X243 (b)), the editor proxy entities and the play copy normalisation, play in editor with several worlds, the shell, viewports, cameras, picking, gizmos, snapping and overlays, the hierarchy, inspector, asset browser and import with asynchronous records, previews and thumbnails (on Forward+ until Z2T-RPROD-112 lands, ruling Q83), console, log, diagnostics, profiler toggle, the cook client, the host watchers (external changes, the script sources, the shader reload journal), reimport, script, shader and asset hot reload, the terrain sculpt tools with the live preview and foliage scatter (Z2-D-131 moved terrain to Stage 3), lighting and material tools, the command registry, headless editing, render tests, the test game `EditorFixture`; the Stage 3 editor writes nothing under `Games/<Name>/.editor/` (§ "Untracked editor files"). Acceptance evidence: undo and redo, independent play worlds (`Zenith2/Docs/Roadmap.md` → "Acceptance evidence").

### Stage 4

Prefab editing (instances, overrides, variants, the prefab stage), the per-user session, layout and endpoint files, autosave, crash recovery and the session lock (every `.editor/` writer, blocked by Z2T-MOD-070), source control, the merge-friendly rules' tests, automation scripts and the live link, the extension table complete with area panels (animation, UI, audio, streaming, save, cinematics, 2D, character hosting, PHYS's ragdoll preview), "Drop to rest", the migration and schema models in `ZenithEditorCore` with the schema panel and its undo by runtime restart, the migration UI with complete coverage of every document type, asset rename, move, delete and the trash, terrain painting, the project settings panel and the new-project wizard, the bindings editor, the walkthrough rehearsals and the human walkthrough runs. Acceptance evidence: crash recovery; the walkthrough rows measured.

### Stage 5

Thermal erosion brush (ENV's Stage 5 kind); VFX's `ZenithAuthoring` move (row ED44, adding ZenithVFX); the VFX tools (the effect inspector customisations, in-place play, the viewers on the proxies, the effect preview), because every VFX ticket carries fix version `Z2 Stage 5` (`Zenith2/Docs/Design/VFX.md` → "Stage 5 slices"); the multi-view previews RPROD's Stage 5 enables; previews switch to the Forward path with Z2T-RPROD-112 (ruling Q83).

### Stage 6

The editor ships in no package; Stage 6 re-runs the editor suites against the final tools build and records the walkthrough rows once more for the release.

### Stage 7

Console and XR content authored through the same editor and cooked for the new platforms (Z2-D-071, Z2-D-072); per-partition concurrent editing with WORLD.

### API-only slices

1. The `ZenithEditorCore` headers of the document, change, history, command, command registry, selection, clipboard and event declarations (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Identifier header", "Change header", "History header", "Command header", "Command registry header", "Selection snapshot", "Clipboard header", "Event header", and Foundation.md "Document interface", "Command records", "Executing a command", "Selection model") with stub definitions (Stage 2 pull-forward), so the authoring-tool tickets compile against them. A Stage 2 header names a type of a library the slice does not yet depend on only by forward declaration and pointer, with the member present and null until its stage (the clipboard's `Zenith_Clipboard*`, the session description's runtime, services, cook client and runtime host; § "Dependency growth rule" rule 2), and each such member's Version line says when it becomes non-null. The slice follows the library-creation ticket `Z2T-EDIT-007` (§ "Library creation ticket").
2. `Zenith_EditorExtension`, `Zenith_EditorPanel`, the panel context and the registration types (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Panel context header", "Extension registration types") and the widget model interfaces before any area-owned panel ticket.
3. The automation step and event tables before the walkthrough rehearsals.

## Decisions

### Settled decisions governing EDIT

| Decision | Effect on EDIT |
|---|---|
| Z2-D-079 | animation, terrain, UI and cinematics get visual tools; materials and VFX get previews and inspectors |
| Z2-D-048 | migrate, never drop: preview, backup, journaled write, unknown-field preservation; the migration UI |
| Z2-D-024, Z2-D-075 | static extensions only; no dynamic plugins; modding excluded |
| Z2-D-059 | editor and preview views with independent histories |
| Z2-D-025 | shaders recompiled out of process; the editor loads blobs |
| Z2-D-016, Z2-D-023 | scripts hot-reload through SCRIPT's transactional reload and recovery |
| Z2-D-020 | data-defined schemas re-registered between play sessions |
| Z2-D-038 | JSON authoring, canonical writers, no layout-dependent formats |
| Z2-D-005 | the profiler in the editor |
| Z2-D-012 | the editor rows and their producers |
| Z2-D-019 | play worlds are deterministic from their play copy |
| Z2-D-045, Z2-D-046, Z2-D-040, Z2-D-034 | errors, no globals, ownership, naming |
| Z2-D-002 | games stay C++-free; editor extension by games is data alone |

Orchestrator rulings EDIT applies (recorded in `C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md`; a person may override): Q83 (previews on Forward+ until Z2T-RPROD-112), Q84 (a) (`ZenithAuthoring`), Q85 (b) (the producer mini-batch), Q86 (a) for SCENE-089 and ECS-100 (`ZenithEditorCore`) and (b) for CB8, AUTHORING-CONTRACT (EDIT owns `ZenithAuthoring`'s contract: the per-owner growing `DEPENDS` of X301, the layout and `ZenithAuthoring_Tests`, no other library's `Internal/` source and no ZenithCook-internal type, the early Stage 2 library-creation ticket, GEOM's material-migration move accepted), FINAL-DOUBLES (declared seams with production implementations, or observable-state assertions; never a double of a `final` class).

### Open decisions EDIT depends on

| Decision | Effect |
|---|---|
| D-LUAUDBG (open, Stage 1) | which debugger client the editor launches against SCRIPT's DAP server |
| D-SEL-JSON (open, Stage 1) | the JSON backend behind every document the editor opens; compared against Z2-B-EDIT-005 |
| D-SEL-HASH (open, Stage 1) | merge keys, command IDs and cache keys use `Zenith_Hash64`; none is persisted across builds except the thumbnail cache key, which a hash change invalidates |
| D-SEL-UIRT (open, Stage 1) | the UI authoring tool's preview context follows UI's ruling |
| D-SEL-FBX (open, Stage 4) | which source files the import dialog accepts |

### Selection decisions EDIT owns

None: ImGui (docking) is the settled editor UI library of `Zenith2/Docs/Architecture.md` → "Third-party dependencies"; git is the system tool the user installs.

## Requirement index

| Requirement | Title | Stage | Design |
|---|---|---|---|
| Z2-R-EDIT-001 | Editor executable target | S3 | EDIT.md → "ZenithEditor target"; EDIT.md → "Libraries and third-party dependencies"; EDIT.md → "Targets" |
| Z2-R-EDIT-002 | Editor command line | S3 | EDIT/Foundation.md → "Executable and command line" |
| Z2-R-EDIT-003 | Composition from the game record | S3 | EDIT/Foundation.md → "Editor composition"; EDIT/Foundation.md → "Module catalogue" |
| Z2-R-EDIT-004 | Module catalogue matches the declared libraries | S3 | EDIT/Foundation.md → "Module catalogue"; EDIT.md → "Dependency growth rule" |
| Z2-R-EDIT-005 | Startup unwinds on failure | S3 | EDIT/Foundation.md → "Startup sequence"; EDIT/Foundation.md → "Runtime tool access" |
| Z2-R-EDIT-006 | One frame graph for the editor | S3 | EDIT/Foundation.md → "Editor frame"; EDIT/Foundation.md → "Runtime tool access"; EDIT/Foundation.md → "Tool access calls per hook"; EDIT/Foundation.md → "Editor run loop" |
| Z2-R-EDIT-007 | Orderly shutdown | S3 | EDIT/Foundation.md → "Shutdown sequence"; EDIT/Foundation.md → "Editor run loop" |
| Z2-R-EDIT-008 | Document type registration | S3 | EDIT/Foundation.md → "Storage roots and the migration registry"; EDIT/Foundation.md → "Editor document type registrations" |
| Z2-R-EDIT-009 | Edit world is a projection of documents | S3 | EDIT/Foundation.md → "Edit world rules"; EDIT/Foundation.md → "Edit world projection"; EDIT/Foundation.md → "Projection per edit kind" |
| Z2-R-EDIT-010 | Edit world creation | S3 | EDIT/Foundation.md → "World kinds"; EDIT/Foundation.md → "Edit world rules"; EDIT/Foundation.md → "Runtime tool access" |
| Z2-R-EDIT-011 | No gameplay simulation in edit worlds | S3 | EDIT/Foundation.md → "Module behaviour in edit and preview worlds" |
| Z2-R-EDIT-012 | Projection buffer splitting and errors | S3 | EDIT/Foundation.md → "Edit world projection" |
| Z2-R-EDIT-013 | Reconcile restores resolved state | S3 | EDIT/Foundation.md → "Reconcile algorithm" |
| Z2-R-EDIT-014 | Unknown components not created in the edit world | S3 | EDIT/Foundation.md → "Projection per edit kind"; EDIT/Panels.md → "Unknown and newer data" |
| Z2-R-EDIT-015 | Preview worlds | S3 | EDIT/Foundation.md → "Preview worlds"; EDIT/Panels.md → "Preview host" |
| Z2-R-EDIT-016 | Play state machine | S3 | EDIT/Foundation.md → "Play state machine" |
| Z2-R-EDIT-017 | Play copies the edit world by serialization | S3 | EDIT/Foundation.md → "Starting play"; EDIT/Foundation.md → "Play copy normalisation" |
| Z2-R-EDIT-018 | Play start failure leaves nothing | S3 | EDIT/Foundation.md → "Play state machine"; EDIT/Foundation.md → "Starting play" |
| Z2-R-EDIT-019 | Stop leaves the edit world unchanged | S3 | EDIT/Foundation.md → "Stopping play" |
| Z2-R-EDIT-020 | Editor pause reason | S3 | EDIT/Foundation.md → "Pause and single step" |
| Z2-R-EDIT-021 | Single step runs exactly n ticks | S3 | EDIT/Foundation.md → "Pause and single step" |
| Z2-R-EDIT-022 | Several independent play worlds | S3 | EDIT/Foundation.md → "Several play worlds"; EDIT/Foundation.md → "Starting play" |
| Z2-R-EDIT-023 | Play-world edits are discarded at Stop | S3 | EDIT/Foundation.md → "Play-world edits" |
| Z2-R-EDIT-024 | Apply play values to documents | S3 | EDIT/Foundation.md → "Applying play values to documents" |
| Z2-R-EDIT-025 | Halted script world stops play | S3 | EDIT/Foundation.md → "Play state machine" |
| Z2-R-EDIT-026 | Input routing during play | S3 | EDIT/Foundation.md → "Input routing during play"; EDIT/Viewports.md → "Game viewport input"; EDIT/Viewports.md → "Editor input claimant" |
| Z2-R-EDIT-027 | One open document per path | S3 | EDIT/Foundation.md → "Document table"; EDIT/Foundation.md → "Document identity and ownership" |
| Z2-R-EDIT-028 | Document ownership through commands | S3 | EDIT/Foundation.md → "Document identity and ownership"; EDIT/Panels.md → "Panel interface" |
| Z2-R-EDIT-029 | Migrated documents save the plan bytes | S3 | EDIT/Foundation.md → "Document states"; EDIT/Foundation.md → "Saving" |
| Z2-R-EDIT-030 | Newer documents are read-only | S3 | EDIT/Foundation.md → "Read-only and newer documents" |
| Z2-R-EDIT-031 | Journaled saves with expected digests | S3 | EDIT/Foundation.md → "Saving" |
| Z2-R-EDIT-032 | Save failures keep documents dirty | S3 | EDIT/Foundation.md → "Saving" |
| Z2-R-EDIT-033 | Unchanged documents write nothing | S4 | EDIT/Pipeline.md → "Rules for files the editor writes" |
| Z2-R-EDIT-034 | External changes | S3 | EDIT/Foundation.md → "External changes"; EDIT/Foundation.md → "Host watchers"; EDIT/Foundation.md → "Document table"; EDIT/Foundation.md → "Closing a document and its history" |
| Z2-R-EDIT-035 | JSON edits with exact inverses | S3 | EDIT/Foundation.md → "JSON edits"; EDIT/Foundation.md → "JSON edit inverses" |
| Z2-R-EDIT-036 | One record per command | S3 | EDIT/Foundation.md → "Command records"; EDIT/Foundation.md → "Executing a command" |
| Z2-R-EDIT-037 | Atomic transactions | S3 | EDIT/Foundation.md → "Transactions" |
| Z2-R-EDIT-038 | Undo and redo | S3 | EDIT/Foundation.md → "Undo and redo"; EDIT/Foundation.md → "Asynchronous changes" |
| Z2-R-EDIT-039 | Failed undo compensates | S3 | EDIT/Foundation.md → "Undo and redo"; EDIT/Foundation.md → "History fences"; EDIT/Foundation.md → "Asynchronous changes" |
| Z2-R-EDIT-040 | No reentrant commands | S3 | EDIT/Foundation.md → "Executing a command"; EDIT/Foundation.md → "Undo and redo" |
| Z2-R-EDIT-041 | Interactions and merging | S3 | EDIT/Foundation.md → "Interactions and merging" |
| Z2-R-EDIT-042 | History memory bound | S3 | EDIT/Foundation.md → "History memory bound"; EDIT/Foundation.md → "History fences" |
| Z2-R-EDIT-043 | Closing a document prunes its history | S3 | EDIT/Foundation.md → "Closing a document and its history" |
| Z2-R-EDIT-044 | Dirty state follows state IDs | S3 | EDIT/Foundation.md → "Dirty tracking" |
| Z2-R-EDIT-045 | Undo round trips for every command | S3 | EDIT/Foundation.md → "Round-trip invariants" |
| Z2-R-EDIT-046 | Selection by persistent references | S3 | EDIT/Foundation.md → "Selection model"; EDIT/Foundation.md → "Selection persistence and mapping" |
| Z2-R-EDIT-047 | Selection operations | S3 | EDIT/Foundation.md → "Selection operations" |
| Z2-R-EDIT-048 | Selection restored by undo | S3 | EDIT/Foundation.md → "Selection and undo" |
| Z2-R-EDIT-049 | Transform edits apply to selection roots | S3 | EDIT/Foundation.md → "Selection model"; EDIT/Viewports.md → "Multi-object edits" |
| Z2-R-EDIT-050 | Paste with fresh identities | S3 | EDIT/Foundation.md → "Copy cut paste and duplicate" |
| Z2-R-EDIT-051 | Clipboard document is untrusted | S3 | EDIT/Formats.md → "Clipboard document"; EDIT/Foundation.md → "Copy cut paste and duplicate" |
| Z2-R-EDIT-052 | ImGui fed from PLAT input | S3 | EDIT/Viewports.md → "ImGui platform adapter" |
| Z2-R-EDIT-053 | Editor input claimant | S3 | EDIT/Viewports.md → "Editor input claimant" |
| Z2-R-EDIT-054 | ImGui rendered through the render graph | S3 | EDIT/Viewports.md → "ImGui renderer"; EDIT/Viewports.md → "Shell view request" |
| Z2-R-EDIT-055 | Texture references are generational and owned | S3 | EDIT/Viewports.md → "Editor texture references"; EDIT/ToolDeclarations.md → "Texture backend header" |
| Z2-R-EDIT-056 | Display scale and fonts | S3 | EDIT/Viewports.md → "Fonts and DPI" |
| Z2-R-EDIT-057 | Shortcuts dispatch commands | S3 | EDIT/Viewports.md → "Shortcuts"; EDIT/Formats.md → "Preferences file" |
| Z2-R-EDIT-058 | Editor views through the render world | S3 | EDIT/Viewports.md → "Viewport kinds"; EDIT/Viewports.md → "Viewport views" |
| Z2-R-EDIT-059 | Viewport resize resets history | S3 | EDIT/Viewports.md → "Viewport views"; EDIT/Viewports.md → "Editor texture references" |
| Z2-R-EDIT-060 | Editor extractor | S3 | EDIT/Viewports.md → "Editor extractor" |
| Z2-R-EDIT-061 | Headless editing | S3 | EDIT/Viewports.md → "Headless viewports" |
| Z2-R-EDIT-062 | Scene publish from a headless boot | S3 | EDIT/Viewports.md → "Headless viewports"; EDIT/Tests.md → "Headless editing tests"; EDIT/Tests.md → "Fixtures" |
| Z2-R-EDIT-063 | Camera schemes | S3 | EDIT/Viewports.md → "Camera schemes" |
| Z2-R-EDIT-064 | Look in raw device counts | S3 | EDIT/Viewports.md → "Fly navigation" |
| Z2-R-EDIT-065 | Fly, orbit, pan and dolly | S3 | EDIT/Viewports.md → "Fly navigation"; EDIT/Viewports.md → "Orbit pan and dolly" |
| Z2-R-EDIT-066 | Framing the selection | S3 | EDIT/Viewports.md → "Framing" |
| Z2-R-EDIT-067 | Orthographic views and bookmarks | S3 | EDIT/Viewports.md → "Orthographic views"; EDIT/Viewports.md → "Camera bookmarks" |
| Z2-R-EDIT-068 | CPU bounds picking | S3 | EDIT/Viewports.md → "Click picking"; EDIT/Viewports.md → "CPU bounds picking" |
| Z2-R-EDIT-069 | Precise mesh picking | S3 | EDIT/Viewports.md → "Precise mesh picking" |
| Z2-R-EDIT-070 | Prefab root picking | S4 | EDIT/Viewports.md → "Click picking" |
| Z2-R-EDIT-071 | Marquee selection | S3 | EDIT/Viewports.md → "Marquee selection" |
| Z2-R-EDIT-072 | Handles are picked first | S3 | EDIT/Viewports.md → "Gizmo handle picking" |
| Z2-R-EDIT-073 | Gizmo modes spaces and pivots | S3 | EDIT/Viewports.md → "Gizmo modes and spaces"; EDIT/Viewports.md → "Pivot" |
| Z2-R-EDIT-074 | Constant screen-size handles | S3 | EDIT/Viewports.md → "Handle sizing" |
| Z2-R-EDIT-075 | Translate drag mathematics | S3 | EDIT/Viewports.md → "Translate drag mathematics" |
| Z2-R-EDIT-076 | Rotate drag mathematics | S3 | EDIT/Viewports.md → "Rotate drag mathematics" |
| Z2-R-EDIT-077 | Scale drag mathematics | S3 | EDIT/Viewports.md → "Scale drag mathematics" |
| Z2-R-EDIT-078 | Gizmo edits write local transforms | S3 | EDIT/Viewports.md → "Applying a gizmo delta" |
| Z2-R-EDIT-079 | Component gizmos | S3 | EDIT/Viewports.md → "Component gizmos" |
| Z2-R-EDIT-080 | Grid snapping | S3 | EDIT/Viewports.md → "Grid snapping"; EDIT/Viewports.md → "Snap settings" |
| Z2-R-EDIT-081 | Rotation and scale snapping | S3 | EDIT/Viewports.md → "Rotation snapping"; EDIT/Viewports.md → "Scale snapping" |
| Z2-R-EDIT-082 | Surface and vertex snapping | S3 | EDIT/Viewports.md → "Surface snapping"; EDIT/Viewports.md → "Vertex snapping" |
| Z2-R-EDIT-083 | Snap toggle inversion | S3 | EDIT/Viewports.md → "Snap settings" |
| Z2-R-EDIT-084 | Overlay passes within the GPU rules | S3 | EDIT/Viewports.md → "Editor overlay passes"; EDIT/Viewports.md → "Grid pass"; EDIT/Viewports.md → "Selection outline pass"; EDIT/Viewports.md → "Gizmo pass" |
| Z2-R-EDIT-085 | Overlay snapshot caps | S3 | EDIT/Viewports.md → "Overlay records" |
| Z2-R-EDIT-086 | Panel and extension interfaces | S4 | EDIT/Panels.md → "Panel interface"; EDIT/Panels.md → "Extension interface"; EDIT/Panels.md → "Extension table" |
| Z2-R-EDIT-087 | Games extend the editor through data | S4 | EDIT/Panels.md → "Extension interface"; EDIT/Panels.md → "Inspector data source" |
| Z2-R-EDIT-088 | Area-owned panels follow the extension contract | S4 | EDIT/Panels.md → "Area-owned panels" |
| Z2-R-EDIT-089 | Hierarchy from documents | S3 | EDIT/Panels.md → "Hierarchy data source" |
| Z2-R-EDIT-090 | Hierarchy edits | S3 | EDIT/Panels.md → "Hierarchy operations" |
| Z2-R-EDIT-091 | Inspector through reflection | S3 | EDIT/Panels.md → "Inspector data source"; EDIT/Panels.md → "Property editors" |
| Z2-R-EDIT-092 | Multi-object inspector edits | S3 | EDIT/Panels.md → "Multi-object editing" |
| Z2-R-EDIT-093 | Schema panel validation | S4 | EDIT/Panels.md → "Schema panel workflow"; EDIT/Declarations.md → "Schema model header" |
| Z2-R-EDIT-094 | Schema re-registration between play sessions | S4 | EDIT/Panels.md → "Schema panel workflow"; EDIT/Pipeline.md → "Schema change" |
| Z2-R-EDIT-095 | Schema change undo | S4 | EDIT/Pipeline.md → "Schema undo"; EDIT/Pipeline.md → "Runtime restart"; EDIT/Panels.md → "Schema panel workflow" |
| Z2-R-EDIT-096 | Asset browser through the cook server | S3 | EDIT/Panels.md → "Asset browser data source"; EDIT/Panels.md → "Asset browser views" |
| Z2-R-EDIT-097 | Import by copy | S3 | EDIT/Panels.md → "Import and drag and drop" |
| Z2-R-EDIT-098 | Importer settings with digest check | S3 | EDIT/Panels.md → "Asset inspector" |
| Z2-R-EDIT-099 | Dependencies and provenance views | S3 | EDIT/Panels.md → "Dependencies and provenance" |
| Z2-R-EDIT-100 | Preview host | S3 | EDIT/Panels.md → "Preview host" |
| Z2-R-EDIT-101 | Material preview | S3 | EDIT/Panels.md → "Material preview"; EDIT/AuthoringTools.md → "Material tools" |
| Z2-R-EDIT-102 | Thumbnails | S3 | EDIT/Panels.md → "Thumbnail generation" |
| Z2-R-EDIT-103 | Editor caches under the governor | S3 | EDIT/Panels.md → "Thumbnail cache" |
| Z2-R-EDIT-104 | Console and script console | S3 | EDIT/Panels.md → "Console" |
| Z2-R-EDIT-105 | Log and diagnostics panels | S3 | EDIT/Panels.md → "Log panel"; EDIT/Panels.md → "Diagnostics panel" |
| Z2-R-EDIT-106 | Profiler toggle and editor provider | S3 | EDIT/Panels.md → "Profiler integration" |
| Z2-R-EDIT-107 | Cook server process | S3 | EDIT/Pipeline.md → "Server process" |
| Z2-R-EDIT-108 | Cook client state machine | S3 | EDIT/Pipeline.md → "Client state machine" |
| Z2-R-EDIT-109 | Cook request protocol | S3 | EDIT/Pipeline.md → "Request queue"; EDIT/Pipeline.md → "Cook client failures" |
| Z2-R-EDIT-110 | Reimport reloads every world | S3 | EDIT/Pipeline.md → "Reimport flow"; EDIT/Pipeline.md → "Notifications"; EDIT/ToolDeclarations.md → "Asset reload sink header" |
| Z2-R-EDIT-111 | Reimport timing | S3 | EDIT/Pipeline.md → "Reimport timing"; EDIT/Automation.md → "Timed operations" |
| Z2-R-EDIT-112 | Rename and move keep GUIDs | S4 | EDIT/Pipeline.md → "Rename and move" |
| Z2-R-EDIT-113 | Delete into the editor trash | S4 | EDIT/Pipeline.md → "Delete and the editor trash"; EDIT/Formats.md → "Editor trash layout" |
| Z2-R-EDIT-114 | Create and duplicate | S4 | EDIT/Pipeline.md → "Create and duplicate" |
| Z2-R-EDIT-115 | Large repairs through the batch operation | S4 | EDIT/Pipeline.md → "Reference repair" |
| Z2-R-EDIT-116 | In-process script compilation | S3 | EDIT/Pipeline.md → "Script reload"; EDIT/Foundation.md → "Host watchers" |
| Z2-R-EDIT-117 | Script reload queued on play worlds | S3 | EDIT/Pipeline.md → "Script reload" |
| Z2-R-EDIT-118 | Script reload timing | S3 | EDIT/Pipeline.md → "Script reload timing"; EDIT/Automation.md → "Timed operations" |
| Z2-R-EDIT-119 | Shader journal applied in order | S3 | EDIT/Pipeline.md → "Shader reload"; EDIT/Foundation.md → "Host watchers" |
| Z2-R-EDIT-120 | Shader reload responses | S3 | EDIT/Pipeline.md → "Shader reload" |
| Z2-R-EDIT-121 | Shader reload timing | S3 | EDIT/Pipeline.md → "Shader reload timing" |
| Z2-R-EDIT-122 | Scene open timing | S3 | EDIT/Foundation.md → "Reconcile algorithm"; EDIT/Automation.md → "Timed operations" |
| Z2-R-EDIT-123 | Play-in-editor start timing | S3 | EDIT/Foundation.md → "Starting play"; EDIT/Automation.md → "Timed operations" |
| Z2-R-EDIT-124 | W-Editor measurement procedure | S3 | EDIT/Automation.md → "Measurement procedure"; EDIT.md → "Untracked editor files" |
| Z2-R-EDIT-125 | Create prefab from selection | S4 | EDIT/AuthoringTools.md → "Creating a prefab" |
| Z2-R-EDIT-126 | Prefab stage | S4 | EDIT/AuthoringTools.md → "Prefab stage" |
| Z2-R-EDIT-127 | Overrides and variants | S4 | EDIT/AuthoringTools.md → "Instances overrides and variants"; EDIT/AuthoringTools.md → "Prefab apply and revert"; EDIT/Panels.md → "Prefab override display" |
| Z2-R-EDIT-128 | Terrain strokes | S3 | EDIT/AuthoringTools.md → "Sculpt and paint strokes" |
| Z2-R-EDIT-129 | Terrain undo through the history | S3 | EDIT/AuthoringTools.md → "Terrain undo" |
| Z2-R-EDIT-130 | Terrain live preview call sites | S3 | EDIT/AuthoringTools.md → "Terrain live preview" |
| Z2-R-EDIT-131 | Terrain sculpt preview latency | S3 | EDIT/AuthoringTools.md → "Terrain live preview"; EDIT/Automation.md → "Timed operations" |
| Z2-R-EDIT-132 | Regeneration after a stroke | S4 | EDIT/AuthoringTools.md → "Regeneration after a stroke" |
| Z2-R-EDIT-133 | Terrain panel and creation wizard | S3 | EDIT/AuthoringTools.md → "Terrain panel"; EDIT/AuthoringTools.md → "Terrain creation wizard" |
| Z2-R-EDIT-134 | Foliage scatter and placement preview | S3 | EDIT/AuthoringTools.md → "Foliage tools" |
| Z2-R-EDIT-135 | Environment inspectors validate | S4 | EDIT/AuthoringTools.md → "Environment inspectors"; EDIT/AuthoringTools.md → "Time of day and weather scrubbing"; EDIT/Foundation.md → "Edit-world-only writes" |
| Z2-R-EDIT-136 | Lighting tools | S3 | EDIT/AuthoringTools.md → "Light and probe inspectors"; EDIT/AuthoringTools.md → "Probe capture"; EDIT/AuthoringTools.md → "Grading editor"; EDIT/Foundation.md → "Edit-world-only writes" |
| Z2-R-EDIT-137 | Bake panel through the cook server | S4 | EDIT/AuthoringTools.md → "Bake panel" |
| Z2-R-EDIT-138 | VFX inspector and preview | S5 | EDIT/AuthoringTools.md → "VFX tools"; EDIT/Foundation.md → "Editor proxy entities" |
| Z2-R-EDIT-139 | Reusable authoring widgets | S4 | EDIT/AuthoringTools.md → "Graph canvas"; EDIT/AuthoringTools.md → "Timeline"; EDIT/AuthoringTools.md → "Curve editor"; EDIT/AuthoringTools.md → "JSON document editor" |
| Z2-R-EDIT-140 | Animation panels | S4 | EDIT/AuthoringTools.md → "Animation graph editor"; EDIT/AuthoringTools.md → "Blend space editor"; EDIT/AuthoringTools.md → "Mask editor"; EDIT/AuthoringTools.md → "Rig setup"; EDIT/AuthoringTools.md → "Clip timeline"; EDIT/AuthoringTools.md → "Retarget setup"; EDIT.md → "Tool-side code the editor needs" |
| Z2-R-EDIT-141 | Animation preview and debugger | S4 | EDIT/AuthoringTools.md → "Animation preview"; EDIT/AuthoringTools.md → "Live animation debugger" |
| Z2-R-EDIT-142 | UI authoring tool | S4 | EDIT/AuthoringTools.md → "UI authoring tool" |
| Z2-R-EDIT-143 | Audio panel hosting | S4 | EDIT/AuthoringTools.md → "Audio tools" |
| Z2-R-EDIT-144 | Project settings through the scaffold | S4 | EDIT/Panels.md → "Project settings panel" |
| Z2-R-EDIT-145 | New project wizard | S4 | EDIT/Panels.md → "New project wizard" |
| Z2-R-EDIT-146 | Bindings editor | S4 | EDIT/Panels.md → "Input bindings editor" |
| Z2-R-EDIT-147 | Upgrade preview | S4 | EDIT/Pipeline.md → "Planning on open"; EDIT/Panels.md → "Migration panel" |
| Z2-R-EDIT-148 | Backup before migration | S4 | EDIT/Pipeline.md → "Applying a migration" |
| Z2-R-EDIT-149 | Resume after interruption | S4 | EDIT/Pipeline.md → "Resuming after an interruption" |
| Z2-R-EDIT-150 | Restore a backup batch | S4 | EDIT/Pipeline.md → "Restoring a backup" |
| Z2-R-EDIT-151 | Complete migration coverage | S4 | EDIT/Pipeline.md → "Migration flow" |
| Z2-R-EDIT-152 | Autosave schedule | S4 | EDIT/Pipeline.md → "Autosave schedule" |
| Z2-R-EDIT-153 | Autosave write | S4 | EDIT/Pipeline.md → "Autosave write"; EDIT/Formats.md → "Autosave file"; EDIT/Formats.md → "Recovery manifest" |
| Z2-R-EDIT-154 | Session lock | S4 | EDIT/Pipeline.md → "Session lock and crash detection"; EDIT/Formats.md → "Session lock line" |
| Z2-R-EDIT-155 | Crash recovery | S4 | EDIT/Pipeline.md → "Recovery on startup"; EDIT/Panels.md → "Recovery dialog" |
| Z2-R-EDIT-156 | Git backend | S4 | EDIT/Pipeline.md → "Backend" |
| Z2-R-EDIT-157 | Status and badges | S4 | EDIT/Pipeline.md → "Status"; EDIT/Panels.md → "Source control panel" |
| Z2-R-EDIT-158 | Diff, revert and conflicts | S4 | EDIT/Pipeline.md → "Diff revert and resolve"; EDIT/Foundation.md → "Editor-originated writes"; EDIT/Pipeline.md → "Merge conflicts in documents" |
| Z2-R-EDIT-159 | Commit and locks | S4 | EDIT/Pipeline.md → "Commit and locks" |
| Z2-R-EDIT-160 | Owners' canonical writers only | S4 | EDIT/Pipeline.md → "Rules for files the editor writes" |
| Z2-R-EDIT-161 | Per-user state outside documents | S4 | EDIT/Pipeline.md → "Per-user state stays out of documents"; EDIT/Formats.md → "Session file"; EDIT/Formats.md → "Layout file"; EDIT/Formats.md → "Preferences file"; EDIT/Formats.md → "Common rules" |
| Z2-R-EDIT-162 | Thumbnail cache file | S3 | EDIT/Formats.md → "Thumbnail file"; EDIT/Panels.md → "Thumbnail cache" |
| Z2-R-EDIT-163 | Command registry | S3 | EDIT/Automation.md → "Command identifiers"; EDIT/Automation.md → "Command descriptors"; EDIT/Automation.md → "Built-in commands" |
| Z2-R-EDIT-164 | Automation scripts | S4 | EDIT/Automation.md → "Automation script format"; EDIT/Automation.md → "Step kinds"; EDIT/Automation.md → "Waiting and events"; EDIT/Automation.md → "Running a script"; EDIT/Automation.md → "Automation report"; EDIT/Foundation.md → "Asynchronous changes" |
| Z2-R-EDIT-165 | Automation assertions | S4 | EDIT/Automation.md → "Assertions" |
| Z2-R-EDIT-166 | Scripts are untrusted | S4 | EDIT/Automation.md → "Running a script" |
| Z2-R-EDIT-167 | Live link endpoint security | S4 | EDIT/Automation.md → "Endpoint security"; EDIT/Formats.md → "Endpoint file" |
| Z2-R-EDIT-168 | Live link protocol | S4 | EDIT/Automation.md → "Endpoint protocol" |
| Z2-R-EDIT-169 | Walkthrough build a scene | S4 | EDIT/Automation.md → "Walkthrough build a scene"; EDIT/Automation.md → "Walkthrough rules" |
| Z2-R-EDIT-170 | Walkthrough author a prefab variant | S4 | EDIT/Automation.md → "Walkthrough author a prefab variant"; EDIT/Automation.md → "Walkthrough rules" |
| Z2-R-EDIT-171 | Walkthrough iterate a script | S4 | EDIT/Automation.md → "Walkthrough iterate a script"; EDIT/Automation.md → "Walkthrough rules" |
| Z2-R-EDIT-172 | Walkthrough reimport an asset | S4 | EDIT/Automation.md → "Walkthrough reimport an asset"; EDIT/Automation.md → "Walkthrough rules" |
| Z2-R-EDIT-173 | Undo and redo acceptance evidence | S3 | EDIT/Foundation.md → "Round-trip invariants"; EDIT.md → "Stage 3" |
| Z2-R-EDIT-174 | Independent play worlds acceptance evidence | S3 | EDIT/Foundation.md → "Several play worlds"; EDIT.md → "Stage 3" |
| Z2-R-EDIT-175 | Crash recovery acceptance evidence | S4 | EDIT/Pipeline.md → "Recovery on startup"; EDIT.md → "Stage 4" |
| Z2-R-EDIT-176 | Editor render validation | S3 | EDIT/Tests.md → "Render tests"; EDIT/Tests.md → "Suites and gate steps" |
| Z2-R-EDIT-177 | Editor on consoles and XR | S7 | EDIT.md → "Stage 7" |
| Z2-R-EDIT-178 | Collaborative editing of world partitions | S7 | EDIT/AuthoringTools.md → "Tools hosted for other areas"; EDIT.md → "Stage 7" |
| Z2-R-EDIT-179 | ZenithEditorCore tool library | S3 | EDIT.md → "ZenithEditorCore target"; EDIT.md → "Libraries and third-party dependencies"; EDIT/Declarations.md → "ZenithEditorCore headers" |
| Z2-R-EDIT-180 | ZenithAuthoring tool library contract | S3 | EDIT.md → "Tool-side code the editor needs"; EDIT.md → "Libraries and third-party dependencies"; EDIT.md → "ZenithAuthoring contract rules"; EDIT.md → "ZenithAuthoring owners" |
| Z2-R-EDIT-181 | Dependency lists grow with their includes | S3 | EDIT.md → "Dependency growth rule" |
| Z2-R-EDIT-182 | Host paths from the executable location | S3 | EDIT/Foundation.md → "Host paths"; EDIT/Declarations.md → "Host paths header"; EDIT/Foundation.md → "Startup sequence" |
| Z2-R-EDIT-183 | Tool services injected once | S3 | EDIT/Foundation.md → "Runtime tool access"; EDIT.md → "API rules for a tool" |
| Z2-R-EDIT-184 | Editor proxy entities | S3 | EDIT/Foundation.md → "Editor proxy entities"; EDIT/Declarations.md → "World set header" |
| Z2-R-EDIT-185 | Withheld components in edit worlds | S3 | EDIT/Foundation.md → "Editor proxy entities"; EDIT/Foundation.md → "Projection per edit kind" |
| Z2-R-EDIT-186 | Edit-world-only writes stay out of documents and play worlds | S3 | EDIT/Foundation.md → "Edit-world-only writes"; EDIT/AuthoringTools.md → "Time of day and weather scrubbing"; EDIT/AuthoringTools.md → "Probe capture" |
| Z2-R-EDIT-187 | Play copy normalisation | S3 | EDIT/Foundation.md → "Play copy normalisation"; EDIT/Foundation.md → "Starting play" |
| Z2-R-EDIT-188 | Play copy drift is reported and reconciled | S3 | EDIT/Foundation.md → "Play copy normalisation" |
| Z2-R-EDIT-189 | Asynchronous records lock the history | S3 | EDIT/Foundation.md → "Asynchronous changes"; EDIT/Declarations.md → "History header" |
| Z2-R-EDIT-190 | Asynchronous failures are compensated | S3 | EDIT/Foundation.md → "Asynchronous changes"; EDIT/Declarations.md → "Change header" |
| Z2-R-EDIT-191 | Asynchronous timeout and cancel | S3 | EDIT/Foundation.md → "Asynchronous changes" |
| Z2-R-EDIT-192 | Automation waits on record events | S4 | EDIT/Automation.md → "Running a script"; EDIT/Automation.md → "Waiting and events" |
| Z2-R-EDIT-193 | Settings undo refuses a changed meta | S3 | EDIT/Panels.md → "Asset inspector"; EDIT/Foundation.md → "Asynchronous changes" |
| Z2-R-EDIT-194 | Asset file moves under the writer lock | S4 | EDIT/Pipeline.md → "Rename and move"; EDIT/Declarations.md → "Storage roots header" |
| Z2-R-EDIT-195 | Runtime restart keeps the session | S4 | EDIT/Pipeline.md → "Runtime restart"; EDIT/ToolDeclarations.md → "Application header" |
| Z2-R-EDIT-196 | A failed runtime restart exits after an autosave | S4 | EDIT/Pipeline.md → "Runtime restart" |
| Z2-R-EDIT-197 | Schema undo keeps older records undoable | S4 | EDIT/Pipeline.md → "Schema undo" |
| Z2-R-EDIT-198 | Texture table reclaims after two frames | S3 | EDIT/Viewports.md → "Editor texture references" |
| Z2-R-EDIT-199 | View extensions filter by view ID | S3 | EDIT/Viewports.md → "ImGui renderer"; EDIT/Viewports.md → "Editor overlay passes" |
| Z2-R-EDIT-200 | Game views follow the world purpose | S3 | EDIT/Viewports.md → "Viewport views" |
| Z2-R-EDIT-201 | Projection handlers run on every change | S3 | EDIT/Foundation.md → "Projection per edit kind"; EDIT/Declarations.md → "Projection change header" |
| Z2-R-EDIT-202 | Document state replacement keeps the history | S3 | EDIT/Foundation.md → "Document interface" |
| Z2-R-EDIT-203 | Schema lock through the cook command line | S4 | EDIT/Panels.md → "Schema panel workflow"; EDIT/Declarations.md → "Schema model header" |
| Z2-R-EDIT-204 | Migration model excludes meta documents from in-process plans | S4 | EDIT/Pipeline.md → "Planning on open"; EDIT/Declarations.md → "Migration model header" |
| Z2-R-EDIT-205 | Models drivable without the shell | S4 | EDIT/Declarations.md → "Migration model header"; EDIT/Declarations.md → "Schema model header"; EDIT/Panels.md → "Migration panel" |
| Z2-R-EDIT-206 | The EditorFixture test game and scratch copies | S3 | EDIT/Tests.md → "Fixtures"; EDIT.md → "Test game" |
| Z2-R-EDIT-207 | Script world events on the main thread | S3 | EDIT/Foundation.md → "Editor frame"; EDIT/Foundation.md → "Runtime tool access"; EDIT/Pipeline.md → "Script reload timing" |
| Z2-R-EDIT-208 | Editor state directories are admitted untracked | S4 | EDIT.md → "Untracked editor files"; EDIT/Automation.md → "Measurement procedure" |
| Z2-R-EDIT-209 | Drop to rest | S4 | EDIT/AuthoringTools.md → "Drop to rest"; EDIT/AuthoringTools.md → "Physics tools" |
| Z2-R-EDIT-210 | Ragdoll preview hosting | S4 | EDIT/AuthoringTools.md → "Ragdoll preview"; EDIT/Panels.md → "Area-owned panels" |
| Z2-R-EDIT-211 | Tool access calls only in legal hooks | S3 | EDIT/Foundation.md → "Tool access calls per hook"; EDIT/Foundation.md → "Editor run loop" |
| Z2-R-EDIT-212 | Device loss autosaves and exits six | S4 | EDIT/Foundation.md → "Device loss"; EDIT.md → "Failure table" |
| Z2-R-EDIT-213 | Unknown request outcomes are resolved from server state | S3 | EDIT/Foundation.md → "Asynchronous changes"; EDIT/Declarations.md → "Change header" |
| Z2-R-EDIT-214 | Partial meta migrations are compensated exactly | S4 | EDIT/Foundation.md → "Asynchronous changes"; EDIT/Pipeline.md → "Applying a migration" |
| Z2-R-EDIT-215 | Editor-originated writes go through the document model | S3 | EDIT/Foundation.md → "Editor-originated writes"; EDIT/Foundation.md → "External changes" |
| Z2-R-EDIT-216 | Repairs and migrations keep open documents and their history | S4 | EDIT/Foundation.md → "Editor-originated writes"; EDIT/Pipeline.md → "Rename and move"; EDIT/Pipeline.md → "Reference repair"; EDIT/Panels.md → "Schema panel workflow" |
| Z2-R-EDIT-217 | Reproducible authoring IDs | S3 | EDIT/Foundation.md → "Session random source"; EDIT/Foundation.md → "Executable and command line" |
| Z2-R-EDIT-218 | Library creation ticket | S2 | EDIT.md → "Library creation ticket"; EDIT.md → "ZenithAuthoring contract rules" |
| Z2-R-EDIT-219 | Deferred changes build from earlier results | S3 | EDIT/Foundation.md → "Executing a command"; EDIT/AuthoringTools.md → "Terrain creation wizard"; EDIT/Declarations.md → "Change header" |
| Z2-R-EDIT-220 | Editor binary formats registered with PACK | S4 | EDIT/Formats.md → "Format registry rows" |
| Z2-R-EDIT-221 | Host watchers report source and journal changes | S3 | EDIT/Foundation.md → "Host watchers"; EDIT/Declarations.md → "Host watcher header"; EDIT/Foundation.md → "External changes" |
| Z2-R-EDIT-222 | Runtime restart rebinds platform objects and types | S4 | EDIT/Pipeline.md → "Runtime restart"; EDIT/Declarations.md → "Clipboard header"; EDIT/Viewports.md → "ImGui platform adapter"; EDIT/Foundation.md → "Document table" |
| Z2-R-EDIT-223 | Redo of create, duplicate and import keeps the GUID | S4 | EDIT/Foundation.md → "Asynchronous changes"; EDIT/Pipeline.md → "Create and duplicate"; EDIT/AuthoringTools.md → "Terrain creation wizard"; EDIT/Formats.md → "Editor trash layout" |
| Z2-R-EDIT-224 | Every session carries a type registry | S3 | EDIT/Declarations.md → "Session description"; EDIT/Foundation.md → "Session object"; EDIT/Tests.md → "Fixtures" |
