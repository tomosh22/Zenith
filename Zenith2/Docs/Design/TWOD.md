# TWOD — 2D

The design of the TWOD area: the `Zenith2D` module (sprites, atlases, tilemaps, 2D animation, cutout skeletons, 2D physics, pixel-perfect cameras), the TWOD-owned 2D renderer inside ZenithFlux (sprite batching through the render graph, the pixel-perfect canvas), 2D authoring in the editor and the cook, the UI2D sample's 2D content and the `Game2D` flagship slice. This entry document holds the scope, dependencies, layout, the top-level contracts, the tables every other file cites, and the index of the section files.

## Purpose and scope

### What TWOD owns

Z2-D-078 makes 2D first class: "sprites and batching through the render graph, atlases, tilemaps, 2D animation, 2D physics (D-SEL-2DPHYS, open, Stage 2), pixel-perfect cameras and rendering, 2D authoring in the editor, and the UI2D sample, proven by `Game2D`". TWOD owns, concretely:

| Capability | Where it lives | Specified in |
|---|---|---|
| the `Zenith2D` module: components, world resources, events, commands, systems, asset kind loaders, script bindings, save participant | `Zenith2/Engine/Zenith2D/` | `Zenith2/Docs/Design/TWOD/PublicAPI.md`, `Zenith2/Docs/Design/TWOD/Simulation.md` |
| sprites, sort groups, sorting layers, y-sorting | Zenith2D (components, extraction); ZenithFlux (sort, batch, draw) | "Component table"; `Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"` |
| sprite batching through the render graph | `Zenith2/Engine/ZenithFlux/Internal/Sprite2D/` with the public header `Flux_Sprite2D.h` | `Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm"` |
| atlases: the authoring document, the deterministic packer, the cooked format, the runtime asset | ZenithAuthoring, ZenithCook, Zenith2D | `Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"`, `Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite atlas format"` |
| tilemaps and tilesets: chunked storage, runtime edits, collision generation, GPU chunk pool | Zenith2D, ZenithFlux | `Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"`, `Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"` |
| 2D animation: flipbooks, the animator state machine, animation events, cutout skeletons | Zenith2D | `Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"`, "Cutout skeleton evaluation" |
| 2D physics: the public API, the internal backend seam, the D-SEL-2DPHYS harness and ruling | Zenith2D (`Internal/Physics2D/`), `Zenith2/Benchmarks/Physics2DSelection/` | `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"`; "Selection decision D-SEL-2DPHYS" |
| pixel-perfect cameras and rendering: the canvas geometry, snapping, exactness rules, the composite | Zenith2D (camera component), ZenithFlux | `Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry"`, "Snapping", "Pixel exactness rules" |
| 2D authoring in the editor: atlas, tilemap, animation and rig editors, gizmos, inspectors | `Zenith2/Tools/ZenithEditor/Internal/2D/` | `Zenith2/Docs/Design/TWOD/Authoring.md → "Editor extension"` |
| the UI2D sample's 2D content; the `Game2D` flagship slice and its acceptance | `Zenith2/Games/WorkloadUI2D` (UI's game, TWOD's 2D layer), `Zenith2/Games/Game2D` | `Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content"`, "Game2D flagship slice" |

### Explicit exclusions

| Not in TWOD | Why | Owner or decision |
|---|---|---|
| 2D dynamic lighting (normal-mapped sprites, 2D shadows) | not in the inventory; the canvas is unlit, world-space sprites are lit by nothing | outside Z2-D-078's list |
| deformable skinned 2D meshes (vertex-weighted sprite meshes) | not in the inventory; cutout skeletons cover the 2D animation item | outside Z2-D-078's list |
| world-space text and UI widgets | UI's | `Zenith2/Docs/Design/UI.md` |
| 3D physics | PHYS's, Z2-D-003 | `Zenith2/Docs/Design/PHYS.md` |
| networked 2D games, split screen | Z2-D-073, Z2-D-076 | deferred, excluded |
| modding of 2D content | Z2-D-075 | excluded |
| stereo and XR views of 2D content | Z2-D-072 | Stage 7, outside TWOD |

### Inventory items covered

| Item | Title | Stage | Covered by |
|---|---|---|---|
| Z2-I-TWOD-01 | sprites | S3 | "Component table"; `Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"` |
| Z2-I-TWOD-02 | sprite batching through the render graph | S3 | `Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm"`, "Sprite2DCanvas pass", "Sprite2DWorld pass" |
| Z2-I-TWOD-03 | atlases | S3 | `Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"`, `Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite atlas format"` |
| Z2-I-TWOD-04 | tilemaps | S4 | `Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"`, `Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"` |
| Z2-I-TWOD-05 | 2D animation | S4 | `Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"`, "Animator state machine", "Cutout skeleton evaluation" |
| Z2-I-TWOD-06 | 2D physics (selection ticket D-SEL-2DPHYS) | S3 | `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"`; "Selection decision D-SEL-2DPHYS" |
| Z2-I-TWOD-07 | pixel-perfect cameras | S3 | `Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"`, `Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry"` |
| Z2-I-TWOD-08 | pixel-perfect rendering | S3 | `Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"`, "Pixel exactness rules", "Sprite2DComposite pass" |
| Z2-I-TWOD-09 | 2D authoring in the editor | S4 | `Zenith2/Docs/Design/TWOD/Authoring.md → "Editor extension"` and the editor sections after it |
| Z2-I-TWOD-10 | the UI2D sample | S4 | `Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content"`, "Game2D flagship slice" |

<!-- elaboration: the UI2D sample -->
The inventory's "UI2D sample" is read as the 2D content of the W-UI2D workload game `Zenith2/Games/WorkloadUI2D`, whose UI side and game are UI's (`Zenith2/Docs/Budgets.md → "W-UI2D"` names the game; the Stage 1 workload scale ticket for W-UI2D is UI's), plus the 2D proof in `Game2D` that Z2-D-078 names. TWOD owns the 2D layer of the first and the whole of the second; row TD16 asks UI to accept the split.
<!-- /elaboration -->

### Document family

| File | Holds | Headings |
|---|---|---|
| `Zenith2/Docs/Design/TWOD.md` | this entry | Purpose and scope · Dependencies · Library and file layout · Public API · Data structures and formats · Algorithms and state machines · Threading lifetime and error contract · Diagnostics and profiling · Authoring and cooking · Failure handling · Test strategy · Budget rows · Stage plan · Decisions · Requirement index · Cross-area reconciliation |
| `Zenith2/Docs/Design/TWOD/PublicAPI.md` | every public header, full declarations and contract blocks | Scope of this file · Header list · Zenith_Module_2D header · Zenith_TwoDTypes header · Zenith_Sprite2DComponents header · Zenith_Camera2D header · Zenith_SpriteAtlas header · Zenith_Sprite2DAnimation header · Zenith_Skeleton2D header · Zenith_Tilemap header · Zenith_Physics2D header · Zenith_TwoDEvents header · Zenith_TwoDScriptApi header · Flux_Sprite2D header |
| `Zenith2/Docs/Design/TWOD/Simulation.md` | phases, animation, skeletons, tilemaps, 2D physics, determinism, saves, world state | Scope of this file · Phase placement · Flipbook evaluation · Animator state machine · Animation events · Animation hot reload · Cutout skeleton evaluation · Tilemap runtime · Tile edit playback · Tilemap collision generation · Tilemap hot reload · Physics 2D model · Physics 2D backend seam · Seam header placement · Candidate B realisation · Candidate J harness realisation · Candidate J production realisation · Physics 2D ingest · Physics 2D external writes · Physics 2D pending operations · Physics 2D step · Physics 2D write-back and sync · Contact and trigger events · One-way platforms · Continuous collision · Sleeping · Physics 2D queries · Physics 2D origin shift · Determinism rules for TWOD · Save participation · Module world state · Memory consumer |
| `Zenith2/Docs/Design/TWOD/Rendering.md` | extraction, sorting, batching, the canvas, passes, shaders, HDR, view extensions, failures | Scope of this file · Extraction · Sort keys · Batching algorithm · Frame data path · Canvas geometry · Snapping · Pixel exactness rules · Sprite stream-sets · Tile tables and the chunk pool · Sprite2DScatter pass · Sprite2DCanvas pass · Sprite2DComposite pass · Sprite2DWorld pass · Shader programs · Colour and HDR output · View extensions and view kinds · Headless and device loss · Render failure behaviour |
| `Zenith2/Docs/Design/TWOD/Formats.md` | authoring schemas, validation, cooked formats, the save section, migration, limits | Scope of this file · Common rules · Tile cell encoding · Tile collision kinds · Sprite atlas document schema · Sprite animation document schema · Tileset document schema · Tilemap document schema · Rig document schema · Image generator document schema · Validation rules · Cooked sprite atlas format · Cooked sprite animation format · Cooked tileset format · Cooked tilemap format · Cooked rig format · Cooked reader checks · TWOD save section format · Document types and migration · Parser limits |
| `Zenith2/Docs/Design/TWOD/Authoring.md` | import, packing, cook steps, scene checks, the editor, templates, the 2D games | Scope of this file · Tool-side layout · ZenithAuthoring functions · Sprite atlas importer · Seeded image generator · Atlas packing · Cook steps · Cook step keys · Cook diagnostics · Scene content checks · Editor extension · 2D scene view mode · Sprite and atlas editor · Tilemap painter · 2D animation editor · Rig editor · 2D physics authoring · 2D project template content · UI2D sample content · Game2D flagship slice · 2D authoring workflow |
| `Zenith2/Docs/Design/TWOD/Tests.md` | every named test and its gate step | Scope of this file · Test targets and files · Test support · Unit tests · Types components and registration · Formats and loaders · Sprites sorting and extraction · Canvas and snapping · Animation and skeletons · Tilemaps · Physics · Save and memory · Script bindings · Render tests · Cook and authoring tests · Editor tests · D-SEL-2DPHYS scenario suite · Integration suite · Fuzz targets · Benchmarks · Device runs · Gate steps and labels |

### Conventions

- Stable names `Zenith.TwoD.*`; the Luau table `TwoD`; configuration keys `twod.*`; counters `twod.*` (module) and `flux.twod.*` (renderer), every per-frame delta ending `_delta`; log category `twod`; graph nodes `TwoD.*@W<tag>`; profiler zones `TwoD.*` and `Flux.Sprite2D.*`.
- 2D content lives in the world's XY plane: +X right, +Y up, rotation about +Z counter-clockwise in radians, metres; the entity's z is its depth for world-space sprites and is ignored by canvas sprites, whose order is the sort key alone.
- Every TWOD obligation is traced to a requirement `Z2-R-TWOD-NNN` ("Requirement index").

## Dependencies

### Areas TWOD depends on

| Area | What TWOD relies on | Headings |
|---|---|---|
| CORE | containers, results, string IDs, hashing, the JSON document tree, maths, memory governor | `Zenith2/Docs/Design/CORE/Text.md → "JSON document tree"`; `Zenith2/Docs/Architecture.md → "Memory governor"` |
| ECS | components, queries, systems, world resources, events, commands | `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"` |
| SCENE | transforms, the lifecycle listener, document framework, canonical serialization | `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"` |
| ASSET | kind table, kind loaders, cook steps, importers, cooked container | `Zenith2/Docs/Design/ASSET/Formats.md → "Cooked asset file"`, `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStepDesc"`, `Zenith2/Docs/Design/ASSET/Import.md → "Zenith_ImportContext"` |
| GEOM | texture usage classes and the texture source intermediate; stream-set validation | `Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"` |
| RPROD | the renderer, views, view extensions, extractors, the main-camera selection, frame data path | `Zenith2/Docs/Design/RPROD/Views.md → "View extensions"`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"` |
| PHYS (both outcomes of D-SEL-2DPHYS) | the PHYS ingest, external-write, sync and origin-shift rules TWOD mirrors in 2D (text only, no PHYS code); for the harness's candidate J only, the `jolt` entry pinned by PHYS's Stage 1 pinning ticket with PHYS's build options and world settings | `Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"`, `Zenith2/Docs/Design/PHYS/Simulation.md → "Physics-to-transform sync"`, `Zenith2/Docs/Design/PHYS/Backends.md → "Jolt world construction"` |
| PHYS (Jolt outcome of D-SEL-2DPHYS only) | the planar world resource `Zenith.Physics.PlanarWorld` of candidate J's production realisation (row TD10). Under the orchestrator's ruling TD10-CONDITIONAL, PHYS (signed off without TD10) takes it only if the ruling selects Jolt-2D, through a PHYS post-signoff patch landed before any TWOD J-path implementation ticket; under the Box2D outcome TWOD depends on no PHYS deliverable | `Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"` (the PHYS heading does not exist until the patch) |
| SAVE | the participant interface and the phase-11 snapshot | `Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`, `Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"` |
| SCRIPT | the binding ABI, the misuse catalogue | `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"` |
| EDIT | the extension interface, reusable widgets, the 2D scene view mode, ZenithAuthoring | `Zenith2/Docs/Design/EDIT/Panels.md → "Extension interface"`, `Zenith2/Docs/Design/EDIT/AuthoringTools.md → "Tools hosted for other areas"`, `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"` |
| CONTENT | image decoders | `Zenith2/Docs/Design/CONTENT/Importers.md → "Zenith_MediaDecoders header"` |
| MOD | module entry points, templates | `Zenith2/Docs/Design/MOD/ProjectCreation.md → "Templates"` |
| UI | composition after the canvas; WorkloadUI2D | `Zenith2/Docs/Design/UI.md → "Luau errors and asserts"` (the pattern TWOD follows) |

### Libraries Zenith2D depends on

The Architecture row (`Zenith2/Docs/Architecture.md → "Library dependency graph"`) lists ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithCore plus the 2D-physics dependency D-SEL-2DPHYS rules. This design's code also includes headers of ZenithSave (the participant), ZenithJobs (`ParallelFor` in nodes), ZenithProfiling (zones) and ZenithVertexFormat (the stream-set codec of the SPRITE family), so the row widens to:

<!-- assumption: pending ARCH (row TD20, cross-doc item X297) -->
```cmake
# Zenith2/Engine/Zenith2D/CMakeLists.txt
zenith_add_library(Zenith2D
	KIND MODULE MODULE_NAME 2D
	DEPENDS ZenithRuntime ZenithFlux ZenithShadersGenerated ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithCore
	        ZenithSave ZenithJobs ZenithProfiling ZenithVertexFormat
	THIRD_PARTY box2d                 # the Box2D outcome of D-SEL-2DPHYS; the Jolt outcome instead adds ZenithPhysics to DEPENDS after PHYS's TD10 patch
	SUPPORTED ZENITH_GPU)
```
<!-- /assumption -->

The `THIRD_PARTY box2d` line and the manifest entry's owner change from the harness to `Zenith2D` with usage `runtime` land in the implementation ticket that follows the D-SEL-2DPHYS ruling if Box2D wins (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"`); the Box2D outcome needs nothing from PHYS, and the `DEPENDS` list above is then complete. Under the Jolt outcome only, and only after the PHYS post-signoff patch that lands row TD10 (ruling TD10-CONDITIONAL), Zenith2D links no Jolt: it gains `ZenithPhysics` in `DEPENDS` for the header of the Physics module's planar world resource, the module list's `2D` row gains the module dependency `Physics` with the Jolt backend (so a game selecting `2D` also selects `Physics` on Jolt), and `Zenith_Module_2D_Start` keeps its single parameter (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"`; cross-doc item X297 (d), row TD21). Before the ruling the module ships without 2D physics bodies (the API slice only, "Stage plan").

### Interfaces TWOD declares for lower layers

None. TWOD's GPU side is a part of ZenithFlux (one library below Zenith2D), reached downward only: the module writes render-world records and registers view extensions; ZenithFlux never calls Zenith2D. The physics backend seam is internal to Zenith2D (in the harness's own tree until the ruling).

### Areas that consume TWOD

| Consumer | Uses |
|---|---|
| UI | the canvas runs before `UIComposite`; UI's WorkloadUI2D carries TWOD's 2D layer |
| GAME | Game2D's gameplay scripts through `TwoD.*` and components |
| EDIT | hosts the 2D extension and the 2D view mode |
| SAVE | runs the TWOD participant |
| SHIP | Game2D acceptance and its defect cap Z2-B-SHIP-022 |

## Library and file layout

### Target summary

| Target | Kind | Directory | Supported | Stage |
|---|---|---|---|---|
| `Zenith2D` | module, `MODULE_NAME 2D` | `Zenith2/Engine/Zenith2D/` | `ZENITH_GPU` | S3 (API slice S2) |
| TWOD's part of `ZenithFlux` | files of RPROD's library | `Zenith2/Engine/ZenithFlux/Flux_Sprite2D.h`, `Zenith2/Engine/ZenithFlux/Internal/Sprite2D/` | as ZenithFlux | S3 |
| TWOD's part of `ZenithAuthoring`, `ZenithCook`, `ZenithEditor` | files of EDIT's and ASSET's tools | `Zenith2/Docs/Design/TWOD/Authoring.md → "Tool-side layout"` | `ZENITH_TOOLS_HOST` | S3, S4 |
| `Zenith2D_Tests` | doctest target | `Zenith2/Engine/Zenith2D/Tests/` | `ZENITH_GPU` | S3 |
| `Zenith_Integration_TwoD` | integration suite | `Zenith2/Tests/TwoD/` | `ZENITH_GPU` | S3 |
| `Zenith_Bench_Physics2DSelection` | benchmark harness: the seam (`Seam/`), both candidates (`Internal/Box2D/`, `Internal/Jolt2D/`), the scenarios (`Scenarios/`, the two cost scenarios included); public headers only plus the `box2d` and `jolt` entries in its own `THIRD_PARTY`; it links ZenithProfiling for the zone `Physics2DSelection.Tick` and no PHYS library; declared by `zenith_add_benchmark(Physics2DSelection DEPENDS Zenith2D ZenithSave ZenithProfiling ZenithCore THIRD_PARTY box2d jolt SUPPORTED ZENITH_WINDOWS ZENITH_ANDROID)` | `Zenith2/Benchmarks/Physics2DSelection/` | `ZENITH_WINDOWS`, `ZENITH_ANDROID` (win-debug, win-release, android-arm64) | S2 |
| `Zenith_Bench_TwoD` | benchmark | `Zenith2/Benchmarks/TwoD/` | `ZENITH_GPU` | S3 |
| `Game2D` | game | `Zenith2/Games/Game2D/` | the game matrix | S1 (definition), S4 (first playable) |

### Zenith2D files

```
Zenith2/Engine/Zenith2D/
	Zenith_Module_2D.h  Zenith_TwoDTypes.h  Zenith_Sprite2DComponents.h  Zenith_Camera2D.h  Zenith_SpriteAtlas.h
	Zenith_Sprite2DAnimation.h  Zenith_Skeleton2D.h  Zenith_Tilemap.h  Zenith_Physics2D.h  Zenith_TwoDEvents.h
	Zenith_TwoDScriptApi.h
	CMakeLists.txt
	Internal/
		Zenith_Module2D.cpp                 RegisterTypes, Start, the module instance (Zenith_TwoDModule)
		Zenith_TwoDConfig.h/.cpp            Zenith_TwoDConfig and Zenith_ReadTwoDConfig
		Zenith_TwoDWorldState.h/.cpp        "Module world state" of Simulation.md
		Zenith_TwoDLoaders.cpp              the five asset kind loaders and their reader checks
		Zenith_TwoDCommands.cpp             the five command kinds' record and playback functions
		Zenith_TwoDLifecycle.cpp            the lifecycle listener
		Zenith_TwoDSaveParticipant.h/.cpp
		Zenith_TwoDMemoryConsumer.cpp
		Zenith_Sprite2DExtract.cpp          TwoD.Extract
		Zenith_Sprite2DExtensions.cpp       the two view extensions
		Animation/  Zenith_Flipbook.cpp  Zenith_Animator.cpp  Zenith_Skeleton2DEval.cpp
		Tilemap/    Zenith_TilemapRuntime.cpp  Zenith_TileEditPlayback.cpp  Zenith_TileCollision.cpp
		Physics2D/  Zenith_Physics2DBackend.h (moved in from the harness's Seam/ by the ruling's implementation ticket)
		            Zenith_Physics2DWorld.cpp (ingest, step, write-back, sync, events, queries, origin shift)
		            Box2D/ (Box2D outcome: moved in by the ruling's implementation ticket) or Planar/ (Jolt outcome: a J-path ticket, after PHYS's TD10 patch)
		Script/     Zenith_TwoDScriptApi.cpp  Zenith_TwoDMisuse.h
		Debug/      Zenith_TwoDDebugDraw.cpp  Zenith_TwoDConsole.cpp
	Tests/
		Zenith_TwoDTypes_Tests.cpp … (Tests.md "Test targets and files")
		Support/    header-only test support (Tests.md "Test support")
		Fuzz/       TwoDAsset.cpp  TwoDSaveSection.cpp
```

No file is platform-specific: Zenith2D has no `Internal/Windows|Android|Linux/` directory.

### ZenithFlux files TWOD owns

```
Zenith2/Engine/ZenithFlux/
	Flux_Sprite2D.h
	Internal/Sprite2D/
		Flux_Sprite2DRenderer.cpp           Create, PrepareFrame, WriteFrame, EndFrame, OnDeviceRecovered
		Flux_Sprite2DFrame.h                the frame record (opaque outside this directory)
		Flux_Sprite2DSort.cpp               keys and the radix sort
		Flux_Sprite2DBatch.cpp
		Flux_Sprite2DCanvas.cpp             Flux_ComputeCanvasGeometry, snapping
		Flux_Sprite2DTiles.cpp              chunk pool and tile table
		Flux_Sprite2DPasses.cpp             Flux_AddSprite2DCanvas, Flux_AddSprite2DWorld and the four passes
	Tests/Sprite2D/                         TWOD's ZenithFlux_Tests files
```

`Flux_Renderer::Enable2D`'s body lives in RPROD's `Internal/Renderer/Flux_Renderer.cpp` (TWOD's lines; row TD1).

### Shader sources

`Zenith2/Shaders/Passes/Sprite2D/`: `Sprite2DScatter.slang`, `Sprite2DCanvas.slang` (sprite and tile programs), `Sprite2DComposite.slang`, `Sprite2DWorld.slang`; the generated parameter structs reach C++ through ZenithShadersGenerated (Z2-D-015). Their entry points and permutations are `Zenith2/Docs/Design/TWOD/Rendering.md → "Shader programs"`.

### Supported matrix

Zenith2D and its tests are `ZENITH_GPU`: built in win-debug, win-release, win-asan, android-arm64 and android-x86_64, never in linux-tsan. Box2D (candidate B) is the `box2d` manifest entry, `integration` `cmake-subproject` with every option of its pinned `CMakeLists.txt` set explicitly (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"`), compiled from source in the dependency cache with the engine's flags; its headers are included only under the harness's `Internal/Box2D/` and, after a Box2D ruling, `Internal/Physics2D/Box2D/`. Jolt is PHYS's `jolt` entry; only the harness names it besides `ZenithPhysics_Jolt`.

## Public API

### Header map

`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Header list"` lists the twelve public headers and their first stages; every declaration is in that file.

### Component table

| Stable name | Script name | C++ type | Properties | Authority | First stage |
|---|---|---|---|---|---|
| `Zenith.TwoD.Sprite` | `Sprite` | `Zenith_SpriteComponent` | 13 (Atlas, Sprite, Tint, FlipX, FlipY, Layer, Order, DrawMode, Size, Blend, Space, Visible, PixelSnap) | `PHASE_LATE_SCRIPTS` | S3 |
| `Zenith.TwoD.SortGroup` | `SortGroup` | `Zenith_SortGroupComponent` | 3 | `PHASE_LATE_SCRIPTS` | S3 |
| `Zenith.TwoD.Camera` | `Camera2D` | `Zenith_Camera2DComponent` | 11 (ReferenceWidth, ReferenceHeight, PixelsPerUnit, ScaleMode, SnapMode, LetterboxColour, ClearColour, Enabled, BoundsEnabled, BoundsMin, BoundsMax) | `PHASE_LATE_SCRIPTS` | S3 |
| `Zenith.TwoD.SpriteAnimator` | `SpriteAnimator` | `Zenith_SpriteAnimatorComponent` | 11 | state fields `PHASE_ANIMATION`; set, speed, paused `PHASE_LATE_SCRIPTS`; state and parameters by commands | S4 |
| `Zenith.TwoD.Skeleton` | `Skeleton2D` | `Zenith_Skeleton2DComponent` | 3 | `PHASE_LATE_SCRIPTS` | S4 |
| `Zenith.TwoD.Tilemap` | `Tilemap` | `Zenith_TilemapComponent` | 10 | `PHASE_LATE_SCRIPTS` | S4 |
| `Zenith.TwoD.RigidBody` | `RigidBody2D` | `Zenith_RigidBody2DComponent` | 1–11 configuration, 20–22 runtime | 1–11 `PHASE_LATE_SCRIPTS`; 20–22 `PHASE_PHYSICS` | S3 |
| `Zenith.TwoD.Collider` | `Collider2D` | `Zenith_Collider2DComponent` | 16 | `PHASE_LATE_SCRIPTS` | S3 |
| `Zenith.TwoD.Joint` | `Joint2D` | `Zenith_Joint2DComponent` | 1–20 configuration (20 WorldAnchor), 21 Broken | 1–20 `PHASE_LATE_SCRIPTS`; 21 `PHASE_PHYSICS` | S3 |

The 2D camera component sits on the main camera entity RPROD selects (beside `Zenith.Render.Camera`, which it never writes); a main camera with an enabled `Zenith.TwoD.Camera` gets a canvas.

Every type TWOD registers sets its script name explicitly through `ScriptName` (orchestrator ruling X353-SCRIPTNAMES, cross-doc item X353): the name is rule 1 of `Zenith2/Docs/Scripting/Bindings.md → "Name mapping from Zenith Hungarian"` applied to the C++ type name, never ECS's default (the stable name's last segment, `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`), so each name is unique programme-wide because C++ type names are. Under the default, rule 1 of `Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"` (`REFLECT_ERROR_BAD_NAME`) would fail startup in every game selecting the 2D module (`Zenith.TwoD.Camera` and RPROD's `Zenith.Render.Camera` both default to `Camera`) and in every game selecting 2D and Physics (ten last segments shared with PHYS's types). The script names of the events, structs, payloads and world resources are the table of "World resources events and commands". The five asset kinds' script names (`SpriteAtlas`, `SpriteAnimation`, `Tileset`, `Tilemap` and `Rig2D`, "Asset kinds") are ASSET's kind-table column, the `K` of `asset<K>`, and are not types the module registers; `Tilemap` as a kind and `Tilemap` as a component never meet in one name scope.

### World resources events and commands

| Kind | Name | Owner phase or emitter |
|---|---|---|
| world resource | `Zenith.TwoD.Physics` (`Zenith_Physics2DWorldResource`, queries) | `PHASE_PHYSICS` |
| world resource | `Zenith.TwoD.Poses` (`Zenith_Skeleton2DPoseResource`) | `PHASE_ANIMATION` |
| world resource | `Zenith.TwoD.TilemapEdits` (`Zenith_TilemapEditResource`) | `PHASE_PHYSICS` |
| world resource written, not owned | `Zenith.Physics.PlanarWorld` (PHYS's, under the Jolt outcome of D-SEL-2DPHYS only, once PHYS's post-signoff patch lands row TD10; ruling TD10-CONDITIONAL) | written by `TwoD.Physics2DStep` alone |
| event | `Zenith.TwoD.AnimationEvent`, `Zenith.TwoD.AnimationFinished` | `TwoD.Animate` |
| event | `Zenith.TwoD.ContactBegan`, `Zenith.TwoD.ContactEnded`, `Zenith.TwoD.TriggerEntered`, `Zenith.TwoD.TriggerExited`, `Zenith.TwoD.JointBroken` | `TwoD.Physics2DStep` |
| command kind | `TwoD.SetTile`, `TwoD.FillTiles` | played back at sync step 7 ("Tile edit playback") |
| command kind | `TwoD.AnimatorPlay`, `TwoD.AnimatorSetParameter` | played back at sync step 7 |
| command kind | `TwoD.Body2D` (impulse, angular impulse, force, torque, set velocity, wake, set kinematic target, set gravity) | played back at sync step 7 |

Script names of the non-component types `Zenith_Module_2D_RegisterTypes` registers (X353-SCRIPTNAMES; the rule of "Component table"):

| Stable name | Kind | C++ type | Script name |
|---|---|---|---|
| `Zenith.TwoD.AnimationEvent` | event | `Zenith_Animation2DEvent` | `Animation2DEvent` |
| `Zenith.TwoD.AnimationFinished` | event | `Zenith_Animation2DFinishedEvent` | `Animation2DFinishedEvent` |
| `Zenith.TwoD.ContactBegan` | event | `Zenith_Contact2DBeganEvent` | `Contact2DBeganEvent` |
| `Zenith.TwoD.ContactEnded` | event | `Zenith_Contact2DEndedEvent` | `Contact2DEndedEvent` |
| `Zenith.TwoD.TriggerEntered` | event | `Zenith_Trigger2DEnteredEvent` | `Trigger2DEnteredEvent` |
| `Zenith.TwoD.TriggerExited` | event | `Zenith_Trigger2DExitedEvent` | `Trigger2DExitedEvent` |
| `Zenith.TwoD.JointBroken` | event | `Zenith_Joint2DBrokenEvent` | `Joint2DBrokenEvent` |
| `Zenith.TwoD.SetTilePayload` | struct (payload) | `Zenith_SetTilePayload` | `SetTilePayload` |
| `Zenith.TwoD.FillTilesPayload` | struct (payload) | `Zenith_FillTilesPayload` | `FillTilesPayload` |
| `Zenith.TwoD.AnimatorPlayPayload` | struct (payload) | `Zenith_AnimatorPlayPayload` | `AnimatorPlayPayload` |
| `Zenith.TwoD.AnimatorParameterPayload` | struct (payload) | `Zenith_AnimatorParameterPayload` | `AnimatorParameterPayload` |
| `Zenith.TwoD.Body2DCommandPayload` | struct (payload) | `Zenith_Body2DCommandPayload` | `Body2DCommandPayload` |
| `Zenith.TwoD.TileCellRef` | struct (script value) | `Zenith_TileCellRef` | `TileCellRef` |
| `Zenith.TwoD.RayHit` | struct (script value) | `Zenith_TwoDRayHit` | `TwoDRayHit` |
| `Zenith.TwoD.OverlapResult` | struct (script value) | `Zenith_TwoDOverlapResult` | `TwoDOverlapResult` |
| `Zenith.TwoD.AnimatorParameter2D` | struct (the element type of `Zenith.TwoD.SpriteAnimator` property 9) | `Zenith_AnimatorParameter2D` | `AnimatorParameter2D` |
| `Zenith.TwoD.Physics` | world resource | `Zenith_Physics2DWorldResource` | `Physics2DWorldResource` |
| `Zenith.TwoD.Poses` | world resource | `Zenith_Skeleton2DPoseResource` | `Skeleton2DPoseResource` |
| `Zenith.TwoD.TilemapEdits` | world resource | `Zenith_TilemapEditResource` | `TilemapEditResource` |

Every recorder takes the `Zenith_TwoDCommandKinds` the module resolved at its first `AttachWorld` (`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"`), which C++ callers read from either world resource's `GetCommandKinds()`.

### Configuration keys

Registered by `Zenith_Module_2D_RegisterTypes` through `Zenith_TypeRegistry::GetConfigSchema()`; "sim" marks a simulation-affecting key (part of the determinism inputs), "pres" a presentation-only key:

| Key | Type | Default | Range | Class |
|---|---|---|---|---|
| `twod.sortingLayers` | string array | `["Default"]` | 1 … 32 names, each 1 … 64 bytes | pres |
| `twod.ySortLayers` | u8 array | `[]` | indices 0 … 31 | pres |
| `twod.anim.maxPosedEntities` | u32 | 4,096 | 1 … 65,536 | sim |
| `twod.tilemap.maxEditedChunks` | u32 | 4,096 | 1 … 65,536 | sim |
| `twod.physics.gravity` | vec2 | (0, −9.81) | each finite, magnitude ≤ 1,000 | sim |
| `twod.physics.layerCollision` | 32 × u32 | all bits set | — | sim |
| `twod.physics.layerNames` | 32 strings | `"Layer<i>"` | each ≤ 64 bytes | pres (editor) |
| `twod.physics.maxBodies` | u32 | 16,384 | 1 … 65,536 | sim |
| `twod.physics.maxShapes` | u32 | 65,536 | 1 … 262,144 | sim |
| `twod.physics.maxJoints` | u32 | 4,096 | 1 … 65,536 | sim |
| `twod.physics.maxContactPairs` | u32 | 65,536 | 1 … 262,144 | sim |
| `twod.physics.maxPendingOpsPerTick` | u32 | 16,384 | 1 … 65,536 | sim |
| `twod.physics.substepsPerTick` | u32 | 4 | 1 … 8 | sim |
| `twod.physics.sleepSeconds` | f32 | 0.5 | 0.05 … 10 | sim |
| `twod.physics.linearSleepTolerance` | f32 | 0.01 m/s | 0.0001 … 1 | sim |
| `twod.physics.angularSleepTolerance` | f32 | 0.0349 rad/s | 0.0001 … 1 | sim |
| `twod.physics.oneWaySpeedThreshold` | f32 | 0.1 m/s | 0 … 10 | sim |
| `twod.physics.oneWaySlop` | f32 | 0.02 m | 0 … 0.5 | sim |
| `twod.render.maxSpritesPerFrame` | u32 | 262,144 | 1,024 … 1,048,576 | pres |
| `twod.render.maxCanvasesPerFrame` | u32 | 4 | 1 … 8 | pres |
| `twod.render.tileChunkPoolChunks` | u32 | 4,096 | 64 … 65,536 | pres |
| `twod.render.tileChunkMirrorChunks` | u32 | 8,192 | 64 … 65,536 | pres |
| `twod.render.tileTableEntries` | u32 | 262,144 | 1,024 … 1,048,576 | pres |
| `twod.render.maxChunkUploadsPerFrame` | u32 | 64 | 1 … 4,096 | pres (GPU uploads per frame) |
| `twod.extract.maxChunkRecordsPerFrame` | u32 | 256 | 1 … 4,096 | pres (chunk records an extractor writes per frame) |
| `twod.debug.physics` | bool | false | — | pres (development builds) |

Simulation keys are read at world attach and never change during a world's life; presentation keys are read at module start (render keys) or every frame (sorting names for the editor).

### Luau surface

The `TwoD` table (`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDScriptApi header"`):

| Function | Access | Arguments (units) | Returns |
|---|---|---|---|
| `TwoD.RayCast(origin, delta, layerMask)` | READ_WORLD `Zenith.TwoD.Physics` | vec2 m, vec2 m, integer mask | `Zenith.TwoD.RayHit` `{ Hit, Collider, Body, Point, Normal, Fraction, Cell }` (a value struct; `Hit` false when nothing was hit) |
| `TwoD.CircleCast(origin, radius, delta, layerMask)` | READ_WORLD `Zenith.TwoD.Physics` | vec2 m, number m > 0, vec2 m, mask | `Zenith.TwoD.RayHit` |
| `TwoD.OverlapBox(min, max, layerMask)` | READ_WORLD `Zenith.TwoD.Physics` | vec2 m, vec2 m, mask | `Zenith.TwoD.OverlapResult` `{ Count, Truncated, Colliders, Bodies }`, at most 64 |
| `TwoD.ApplyImpulse(body, impulse, point)` | BUFFERED | entity, vec2 N·s, vec2 m | nothing |
| `TwoD.ApplyForce(body, force, point)` | BUFFERED | entity, vec2 N, vec2 m | nothing |
| `TwoD.SetVelocity(body, linear, angular)` | BUFFERED | entity, vec2 m/s, rad/s | nothing |
| `TwoD.SetKinematicTarget(body, position, rotation)` | BUFFERED | entity, vec2 m, rad | nothing |
| `TwoD.SetGravity(gravity)` | BUFFERED | vec2 m/s² | nothing |
| `TwoD.Play(entity, state, restart)` | BUFFERED | entity, state name (resolved at playback), bool | nothing |
| `TwoD.SetParameter(entity, parameter, value)` | BUFFERED | entity, name (resolved at playback), number (the binding's parameter is `double`, a `NUMBER`; a BOOL parameter takes 0 or 1, and a Luau boolean is a marshalling error of SCRIPT's, never a BOOL write) | nothing |
| `TwoD.SetTrigger(entity, parameter)` | BUFFERED | entity, trigger name (resolved at playback) | nothing |
| `TwoD.GetTile(map, layer, x, y)` | READ_WORLD `Zenith.TwoD.TilemapEdits` | entity, integer, cells | the 32-bit cell |
| `TwoD.SetTile(map, layer, x, y, cell)` | BUFFERED | entity, integer, cells, cell bits | nothing |
| `TwoD.FillTiles(map, layer, x0, y0, x1, y1, cell)` | BUFFERED | entity, integer, cells (inclusive), cell bits | nothing |
| `TwoD.WorldToCell(map, world)` | READ_WORLD `Zenith.TwoD.TilemapEdits` | entity, vec2 m | vec2 of integer cells |
| `TwoD.GetBonePosition(entity, bone)` | READ_WORLD `Zenith.TwoD.Poses` | entity, bone name (resolved through the pose's rig) | vec2 m (world) |
| `TwoD.ReferenceToWorld(cameraPos, ppu, w, h, ref)`, `TwoD.WorldToReference(…)` | PURE | camera fields, vec2 | vec2 |

Component fields (sprite, camera, body configuration, collider, joint) are read and written through component views like every component, each view named by its type's script name of "Component table" (`Sprite`, `Camera2D`, `RigidBody2D`, `Collider2D`, `Joint2D` and the others), never by a stable name's last segment; enums are exposed as `TwoD.Sprite2DSpace` and the other reflected enums.

### Handle kinds used

| Handle | Kind | Use |
|---|---|---|
| `Zenith_AssetHandle<T>` for the five kinds | generational asset handle | component properties, loaders |
| `Zenith_EntityHandle` | generational entity handle | joints' other body, query results, script arguments |
| `Flux_TextureID` | generational texture ID | atlas pages in render-world records |
| `Zenith_Body2DID`, `Zenith_Shape2DID`, `Zenith_Joint2DID` | internal generational slot IDs | the backend seam only |

## Data structures and formats

### Format index

| Format | Specified in |
|---|---|
| five authoring documents (`*.spriteatlas.json`, `*.spriteanim.json`, `*.tileset.json`, `*.tilemap.json`, `*.rig2d.json`) | `Zenith2/Docs/Design/TWOD/Formats.md → "Sprite atlas document schema"` and the four headings after it |
| five cooked files (`.zatlas`, `.zsanim`, `.ztileset`, `.ztilemap`, `.zrig2d`) | `Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite atlas format"` and the four headings after it |
| the 32-bit tile cell | `Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding"` |
| the TWOD save section | `Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format"` |
| the `twod.atlasSections` intermediate | `Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"` step 6 |
| render-world records (72-byte sprite record, 72-byte tilemap draw, chunk and tileset records) | `Zenith2/Docs/Design/TWOD/PublicAPI.md → "Flux_Sprite2D header"` |
| the 64-bit tile cell key | `Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding"` |
| GPU instance layouts (40-byte canvas, 48-byte world, 36-byte chunk instance) | `Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets"` |

### Asset kinds

Requested from ASSET (row TD3); the codes are the next free codes ASSET reserves in `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`:

| Enumerator | Stable name | Script name | Runtime object type | Owner | Cooked extension | Magic | Memory subsystem | Default fallback policy | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|
| `ASSET_KIND_SPRITE_ATLAS` | `TwoD.SpriteAtlas` | `SpriteAtlas` | `Zenith_SpriteAtlasAsset` | TWOD | `.zatlas` | `ZATL` | ASSETS | FAIL | yes | no (pages are texture assets) |
| `ASSET_KIND_SPRITE_ANIMATION` | `TwoD.SpriteAnimation` | `SpriteAnimation` | `Zenith_SpriteAnimationAsset` | TWOD | `.zsanim` | `ZSAN` | ASSETS | FAIL | yes | no |
| `ASSET_KIND_TILESET` | `TwoD.Tileset` | `Tileset` | `Zenith_TilesetAsset` | TWOD | `.ztileset` | `ZTSE` | ASSETS | FAIL | yes | no |
| `ASSET_KIND_TILEMAP` | `TwoD.Tilemap` | `Tilemap` | `Zenith_TilemapAsset` | TWOD | `.ztilemap` | `ZTMP` | SIMULATION | FAIL | yes | no |
| `ASSET_KIND_RIG_2D` | `TwoD.Rig` | `Rig2D` | `Zenith_Rig2DAsset` | TWOD | `.zrig2d` | `ZR2D` | ASSETS | FAIL | yes | no |

A tilemap is SIMULATION memory because its cells feed collision; the governor sheds nothing of it.

### In-memory sizes that matter

| Structure | Size |
|---|---|
| `Flux_Sprite2DRecord` | 72 bytes |
| `Flux_Sprite2DTilemapDraw` | 72 bytes |
| `Flux_TilesetTileEntry` (GPU tile table entry) | 24 bytes |
| tile chunk (GPU pool and CPU) | 4,096 bytes (1,024 cells) |
| overlay chunk (CPU, edits) | 4,228 bytes (4,248 in the save section with its key) |
| chunk mirror entry (CPU, renderer) | 4,096 bytes of cells plus an 8-byte content hash |
| canvas instance / world instance / chunk instance | 40 / 48 / 36 bytes |
| sort key | 8 bytes, plus the 8-byte simulation ID tie-break |
| posed entity | 128 bones × 24 bytes + 256 slots × 12 bytes at most |
| `Zenith_TwoDCommandKinds`, `Zenith_TileCellRef` | 20, 12 bytes |
| `Zenith_TwoDError` | 8 bytes |

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| extraction of sprites, chunks and tilesets | O(sprites + changed chunks) per frame | `Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"` |
| sort (LSD radix, 64-bit key + ID) | O(n) | `Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"` |
| batching | O(n) | `Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm"` |
| canvas geometry and snapping | O(1) per canvas, O(n) per sprite list | `Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry"`, "Snapping" |
| flipbook frame selection | O(log f) per animator | `Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation"` |
| animator transitions | O(t) per animator | `Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"` |
| cutout skeleton evaluation | O(bones + slots) per skeleton | `Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"` |
| tile edit playback | O(cells) per command | `Zenith2/Docs/Design/TWOD/Simulation.md → "Tile edit playback"` |
| tile collision (marching squares, chain merge, ghost vertices) | O(cells) per dirty chunk | `Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"` |
| physics ingest, step, write-back, sync | O(changes log changes) + step | `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"` and after |
| atlas packing (MaxRects BSSF) | O(n · f) | `Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"` |

### State machine index

| State machine | States | Specified in |
|---|---|---|
| animator | entering default, in state, transitioning (one per tick), finished (once clips) | `Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"` |
| body sleep | awake, sleeping (woken by contact, command or external write) | `Zenith2/Docs/Design/TWOD/Simulation.md → "Sleeping"` |
| chunk residency in the renderer | absent, mirrored (CPU only), resident (CPU and GPU) | `Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"` |
| contact pair | none, touching (began emitted), separated (ended emitted) | `Zenith2/Docs/Design/TWOD/Simulation.md → "Contact and trigger events"` |

The chunk residency machine as a table, per (map key, layer, chunk) in the renderer (the others are tables in Simulation.md); "wanted" is the residency pass's per-frame set:

| State \ event | UPSERT record | wanted, upload slot available this frame (a free GPU slot, or an unwanted resident chunk to evict) | wanted, every GPU slot wanted this frame | not wanted, an eviction picks it | REMOVE record | device recovered |
|---|---|---|---|---|---|---|
| absent | → mirrored (`flux.twod.mirror_full_delta` and stays absent when the mirror is full) | — | — | — | stays absent | stays absent |
| mirrored | cells replaced | → resident: the scatter writes the cells this frame, at most `maxChunkUploadsPerFrame` a frame | stays mirrored, draws empty (`flux.twod.chunks_refused_delta`, `flux.twod.chunks_pending_delta`) | — | → absent | stays mirrored |
| resident | cells replaced; re-uploaded at the front of the queue when the content hash changed | stays resident, `lastWantedFrame` updated | stays resident | → mirrored, slot freed after retirement (`flux.twod.chunks_evicted_delta`) | → absent, slot freed after retirement | → mirrored |

### Module start sequence

`Zenith_Module_2D_Start(xContext)` performs, each step with a failure-injection value and reverse unwinding on failure ([Z2-N-CONTRACT-340]):

1. Read `Zenith_TwoDConfig` from `xContext.GetConfig()` (`Zenith_ReadTwoDConfig`; the schema already clamped every value).
2. Register the five asset kind loaders with `xContext.GetAssets()`.
   2a. Box2D outcome only: install Box2D's allocator hooks (`b2SetAllocator`, the same root-heap function pointers every time; `Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"`). Nothing is installed under the Jolt outcome.
3. Register the five command kinds (`TwoD.SetTile`, `TwoD.FillTiles`, `TwoD.AnimatorPlay`, `TwoD.AnimatorSetParameter`, `TwoD.Body2D`) with `RegisterCommandKind`: payload types and sizes of "Zenith_Module_2D header" in PublicAPI.md, phase mask 0x7FE (phases 1 … 10), the playback functions and the module instance as user data. `RegisterCommandKind` returns no ID, so the IDs are resolved at the first attach.
4. Register the lifecycle listener (destroyed and teleported entities).
5. Validate the 2D physics configuration against the simulation tick: a tick above 1/15 s or a substep count outside 1 … 8 fails with `MODULE_ERROR_REGISTRATION` and the log names the key.
6. Register the `TwoD` Luau table (`Zenith_RegisterTwoDScriptBindings` into a `Zenith_ScriptBindingTable`, then `RegisterScriptBindings`).
7. When `GetRenderer()` is non-null: `Flux_Renderer::Enable2D(xDesc)` with the `twod.render.*` values, then `RegisterViewExtension` for `Zenith_Sprite2DCanvasExtension` and `Zenith_Sprite2DWorldExtension`; headless, nothing (`Zenith2/Docs/Design/TWOD/Rendering.md → "Headless and device loss"`).
8. Register the memory consumer (`MEMORY_SUBSYSTEM_SIMULATION`).
9. Register the console commands and the debug-draw toggle in development builds (`Zenith_IsShippingBuild()` false).
10. Construct `Zenith_TwoDModule` owning everything above and return it.

TWOD declares no asynchronous source: every TWOD result is synchronous within a tick or a buffered command.

### World attach and detach

`Zenith_TwoDModule::AttachWorld(xContext)`, each step with a failure-injection value:

0. At the first attach only: resolve the five kind IDs into the module's `Zenith_TwoDCommandKinds` with `xContext.FindCommandKind("TwoD.SetTile")` and the four other names; a zero fails the step with `MODULE_ERROR_REGISTRATION`.
1. Create the world's `Zenith_TwoDWorldState` (`Zenith2/Docs/Design/TWOD/Simulation.md → "Module world state"`): the backend world with the configured gravity, layer matrix and capacities (`PHYSICS2D_ERROR_OUT_OF_MEMORY` fails the step; under Box2D `b2CreateWorld` on the main thread, `PHYSICS2D_ERROR_WORLD_CAPACITY` beyond the world table; under the Jolt outcome, after PHYS's conditional TD10 patch, the step checks by name that the world carries `Zenith.Physics.PlanarWorld` and fails with `MODULE_ERROR_REGISTRATION` naming it when the Physics module was started with `physics.planarWorld` false), the ground body, the record tables, the pose buffers, the overlay and the pending list.
2. Add the three world resources (`Zenith.TwoD.Physics`, `Zenith.TwoD.Poses`, `Zenith.TwoD.TilemapEdits`).
3. Register `TwoD.Animate@W<tag>` and `TwoD.AnimateSkeletons@W<tag>` (phase 3).
4. Register `TwoD.Physics2DStep@W<tag>` (phase 5) and `TwoD.Physics2DSyncTransforms@W<tag>` (phase 6, `WRITER_SOURCE_PHYSICS_2D`).
5. Add the extractor `TwoD.Extract@W<tag>` (phase 12, Before `Render.Seal@W0`).
6. Register the world's save participant `Zenith_TwoDSaveParticipant` (`Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"`).
7. In development builds, register `TwoD.PhysicsDebugDraw@W<tag>` when the debug-draw recorder exists.
8. Register the world's lifecycle listener interest (`LIFECYCLE_EVENT_DESTROYED`, teleport, `LIFECYCLE_EVENT_ORIGIN_SHIFTED`).

<!-- assumption: pending ARCH (row TD23) -->
Step 0 uses `uint32 Zenith_WorldAttachContext::FindCommandKind(const char* szName) const` (0 when absent), answered from the sealed `Zenith_CommandKindTable::FindByName`, the member PHYS (row PH7) and GAME (row GA5 (c)) also assume.
<!-- /assumption -->

`DetachWorld(uWorldTag)` reverses them: the participant is unregistered, the nodes and resources are removed by the runtime's journal, the backend world and every table are destroyed; pending commands of a detached world are discarded with the world.

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| components, events, payloads, value types | as every component; `IMMUTABLE` values |
| `Zenith_SpriteAtlasAsset`, `Zenith_SpriteAnimationAsset`, `Zenith_TilesetAsset`, `Zenith_TilemapAsset`, `Zenith_Rig2DAsset` | `IMMUTABLE` once ACTIVE; read from any node |
| `Zenith_Physics2DWorldResource` | queries `ANY_THREAD` during the read phases after phase 5 of a tick; mutation only by `TwoD.Physics2DStep` |
| `Zenith_Skeleton2DPoseResource` | written by `TwoD.AnimateSkeletons`, read after phase 3 |
| `Zenith_TilemapEditResource` | written at sync playback and by `TwoD.Physics2DStep` (dirty set), read anywhere after |
| the backend (`Zenith_Physics2DBackend`) | `OWNER_THREAD`: the step node in phase 5, the snapshot node in phase 11 (capture), the sync-point node at an origin shift, or the main thread out of tick (restore, attach, detach) |
| `Flux_Sprite2DRenderer` | `PrepareFrame`, `WriteFrame`, `EndFrame` on `Flux.BuildGraph`; record jobs read the frame |
| `Zenith_TwoDModule` | `MAIN_THREAD` for start, attach, detach, shutdown |

### Frame-graph placement

| Node | Subgraph, phase | Declared access |
|---|---|---|
| sync point step 7 (TWOD's playback functions and lifecycle listener) | SIMULATION, every sync point | tile overlay, animator state and parameters, the pending physics list, in merge order; at a sync point that plays `Scene.ShiftWorldOrigin`, the backend world (`ShiftOrigin`) and the module's position-valued state (the written local positions, the map records' world positions, the pending list's world points), written by the lifecycle listener after the shift's playback ("Physics 2D origin shift" of Simulation.md) |
| `TwoD.Animate@W<tag>`, `TwoD.AnimateSkeletons@W<tag>` | SIMULATION, phase 3 | `Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"` |
| `TwoD.Physics2DStep@W<tag>` | SIMULATION, phase 5 | the same table |
| `TwoD.Physics2DSyncTransforms@W<tag>` | SIMULATION, phase 6 | the same table |
| `TwoD.Extract@W<tag>` | EPILOGUE, phase 12 | the same table; the world's render-world writer partition |
| `TwoD.PhysicsDebugDraw@W<tag>` | EPILOGUE, development builds | read `Zenith.TwoD.Physics`; the debug-draw recorder |
| `Flux.BuildGraph` (RPROD's) | render | `PrepareFrame` (step 4), the 2D passes (extension points), `WriteFrame` (step 11), `EndFrame` (step 13) |

### Lock-order entries

None. TWOD takes no lock: per-world state is owned by its nodes under the graph's declared access, the backend runs on one thread, the renderer's state is owned by `Flux.BuildGraph`, and memory-consumer totals are relaxed atomics. Box2D is configured with `workerCount = 1` and no task callbacks, so it creates no thread and takes no lock of its own on the engine's threads; the harness's Jolt candidate steps with `JPH::JobSystemSingleThreaded`, and the planar world of the Jolt outcome (conditional, row TD10) is stepped single-threaded from TWOD's node with its own temp allocator and job system.

### Construction and shutdown

Construction is the module start and attach sequences above. Shutdown: every world detaches (reverse attach), then the module instance's destructor unregisters the view extensions, the memory consumer, the lifecycle listener and the console commands, and the runtime's journal undoes the type and binding registrations. The renderer's 2D state is destroyed with the renderer (RPROD's shutdown), after the module.

### Cancellation

TWOD work is bounded per tick and per frame and is not cancellable mid-step: a physics step runs to completion; extraction is bounded by the slot; chunk uploads are bounded by `twod.render.maxChunkUploadsPerFrame`. Asset loads are cancelled by releasing handles; a command recorded for a world that detaches before its sync point is discarded with the world.

### Error enumerations

| Enumeration | Header | Codes |
|---|---|---|
| `Zenith_TwoDErrorCode` | `Zenith_TwoDTypes.h` | `TWOD_ERROR_OUT_OF_MEMORY` (1), `INVALID_DESC`, `CORRUPT_ASSET`, `VERSION_MISMATCH`, `UNKNOWN_SPRITE`, `UNKNOWN_STATE`, `UNKNOWN_PARAMETER`, `UNKNOWN_BONE`, `CELL_OUT_OF_RANGE`, `TILE_OUT_OF_RANGE`, `LAYER_OUT_OF_RANGE`, `NOT_RESIDENT`, `CAPACITY`, `STALE_ENTITY`, `MISSING_COMPONENT`, `PHYSICS` (16) |
| `Zenith_Physics2DErrorCode` | `Zenith_Physics2D.h` | `PHYSICS2D_ERROR_OUT_OF_MEMORY` (1), `INVALID_DESC`, `BODY_CAPACITY`, `SHAPE_CAPACITY`, `JOINT_CAPACITY`, `NO_BODY`, `BAD_QUERY`, `STATE_CORRUPT`, `STEP_FAILED`, `WORLD_CAPACITY` (10) |
| `Flux_Sprite2DError` | `Flux_Sprite2D.h` | `FLUX_SPRITE2D_ERROR_OUT_OF_MEMORY` (1), `INVALID_DESC`, `DEVICE` (3) |
| `Zenith_TwoDAuthoringErrorCode` | `Zenith2/Tools/Authoring/Zenith_AuthoringTwoD.h` | `Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"` |
| module errors | `Zenith_ModuleError` (MOD) | `MODULE_ERROR_REGISTRATION`, `MODULE_ERROR_OUT_OF_MEMORY` |

### Luau errors and asserts

<!-- elaboration: TWOD's misuse block and its catalogue rows -->
Script misuse raises a Luau error, never an assert. TWOD's catalogued codes are the block ZS2400–ZS2499: the orchestrator's block assignment (ruling MISUSE-BLOCKS-2) gives TWOD this block (ZS2300–ZS2399 is PHYS's, cross-doc item X264 (k)), and cross-doc item X298 adds its row to the table under [Z2-N-SCRIPT-926]. One meaning per code; the assigned codes are ZS2401 and ZS2405 to ZS2409:

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS2401 | MISUSE | a query argument that is not finite, a zero-length `delta`, a radius ≤ 0, or `min` above `max` in either axis | `TwoD.<Function>: <argument> must be <rule>, got <Value>` |
| ZS2405 | MISUSE | a tile layer outside 0 … 15, or cell bits with any of bits 19–31 set | `TwoD.<Function>: <argument> out of range, got <Value>` |
| ZS2406 | MISUSE | a `FillTiles` rectangle of more than 65,536 cells, or a coordinate outside −2^30 … 2^30 − 1 | `TwoD.FillTiles: rectangle too large or out of range` |
| ZS2407 | MISUSE | `TwoD.GetBonePosition` with a bone name the entity's rig does not contain | `TwoD.GetBonePosition: unknown bone '<Value>'` |
| ZS2408 | MISUSE | a non-finite vector or number in a physics command | `TwoD.<Function>: <argument> must be finite, got <Value>` |
| ZS2409 | MISUSE | `TwoD.SetGravity` with a magnitude above 1,000 m/s² | `TwoD.SetGravity: gravity magnitude must be at most 1000, got <Value>` |

`<Function>` is the Luau function name and `<Value>` the argument as Luau's `tostring` prints it. ZS2402 is not assigned: the `layerMask` parameter is a `uint32`, which SCRIPT marshals before the thunk runs ("integral and in range required; non-integral raises ZS1502, out of range ZS1503", `Zenith2/Docs/Scripting/Bindings.md → "Value marshalling"`), so no TWOD code can see a bad mask. ZS2403 and ZS2404 are not assigned: a BUFFERED binding reads no world state at the call (PHYS's rule for its own BUFFERED bindings), so `TwoD.Play`, `TwoD.SetParameter` and `TwoD.SetTrigger` record the name's `Zenith_StringID` and playback resolves it, dropping an unknown name or a kind mismatch with `twod.anim.command_dropped_delta`, never an error and never depending on residency at the call. ZS2407 stays because `TwoD.GetBonePosition` is READ_WORLD on its declared resource `Zenith.TwoD.Poses` and resolves the bone through the pose's own rig. `nil` where an entity is required is SCRIPT's ZS1104; a stale or foreign entity passed to a READ_WORLD function is SCRIPT's ZS1101 or ZS1102; an entity lacking the component a command needs is counted at playback (`twod.physics.command_no_body_delta`, `twod.anim.command_dropped_delta`, `twod.tilemap.edit_dropped_delta`). `Test_TwoDScript_MisuseCodes` raises each code with its exact text. Asserts guard only programmer errors inside Zenith2D and ZenithFlux's 2D files.

## Diagnostics and profiling

### Counters

| Counter | Meaning |
|---|---|
| `twod.anim.command_dropped_delta`, `twod.anim.events_clipped_delta`, `twod.anim.unresolved_set_delta`, `twod.anim.unresolved_rig_delta` | animation commands dropped (stale entity, missing component, unresolved set, unknown name, kind mismatch); events over the per-tick cap; unresolved animation sets and rigs |
| `twod.command.unresolved_kind_delta` | a recorder called with a zero kind ID (the module not started) |
| `twod.tilemap.bad_tile_delta`, `twod.tilemap.edit_dropped_delta`, `twod.tilemap.edit_too_large_delta`, `twod.tilemap.overlay_full_delta`, `twod.tilemap.unresolved_delta`, `twod.tilemap.transformed_delta` | tile-cell and edit problems; a rotated or scaled map entity drawn and collided axis-aligned |
| `twod.physics.bodies_refused_delta`, `command_dropped_delta`, `command_no_body_delta`, `dual_body_delta`, `external_pose_delta`, `external_velocity_delta`, `static_moved_delta`, `step_failed_delta`, `bad_enum_delta` (all `twod.physics.*`) | physics ingest, command and step outcomes |
| `twod.physics.non_finite_delta` | a body configuration (RigidBody 2–5, 10–11) rejected at ingest because a value is not finite (the body is not created), or a `TwoD.Body2D` payload dropped at playback because a vector or scalar is not finite |
| `twod.physics.bad_shape_delta` | a degenerate collider (zero extent, a non-convex or clockwise polygon, fewer than 3 polygon vertices) that created no shape; one development-build warning per entity |
| `twod.physics.joint_unresolved_delta` | a joint whose `BodyA`, or non-null `BodyB`, resolves to no 2D body at ingest step 5, so no constraint exists this tick |
| `twod.physics.command_bad_kind_delta` | a `TwoD.Body2D` payload whose kind byte is not a `Zenith_Body2DCommandKind` value, dropped at playback |
| `twod.canvas.no_render_camera_delta` | an enabled `Zenith.TwoD.Camera` on an entity without `Zenith.Render.Camera`, which therefore produces no canvas (counted once per frame per such entity) |
| `twod.physics.bodies`, `twod.physics.awake_bodies`, `twod.physics.contacts`, `twod.physics.joints` | gauges per world |
| `twod.physics.origin_shifts` (cumulative), `twod.physics.origin_invalid_delta` | origin shifts applied; shifts refused as defects |
| `twod.extract.slot_full_delta`, `twod.extract.tiled_clipped_delta` | extraction |
| `twod.render.bad_layer_delta`, `missing_sprite_delta`, `mixel_sprites_delta`, `rotated_canvas_sprites_delta`, `sort_depth_delta`, `bad_enum_delta` (all `twod.render.*`) | content problems seen at extraction |
| `twod.canvas.none_delta` | a frame whose main camera has no enabled 2D camera while 2D content exists |
| `flux.twod.batches`, `flux.twod.sprites`, `flux.twod.chunks_resident` | renderer gauges per frame |
| `flux.twod.canvas_downscaled_delta`, `canvas_limit_delta`, `chunks_refused_delta`, `chunks_pending_delta`, `chunks_evicted_delta`, `mirror_full_delta`, `missing_page_delta`, `page_table_full_delta`, `sprites_dropped_delta`, `tile_table_full_delta`, `tilemap_unknown_delta` (all `flux.twod.*`) | `Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour"` |

### Log categories

`twod` for every module message (warnings once per entity or asset per world, rate-limited by the log system), `twod.physics` for backend messages, `flux.twod` for the renderer.

### Profiler zones

`TwoD.Animate`, `TwoD.AnimateSkeletons`, `TwoD.Physics2DStep` (with sub-zones `Ingest`, `Step`, `WriteBack`, `Events`), `TwoD.Physics2DSyncTransforms`, `TwoD.Extract`, `TwoD.Playback`; renderer `Flux.Sprite2D.Prepare`, `Flux.Sprite2D.Write`; GPU timestamps per 2D pass (`Sprite2DScatter`, `Sprite2DCanvas`, `Sprite2DComposite`, `Sprite2DWorld`). The D-SEL-2DPHYS harness (not the module) opens one zone per simulated tick, `Physics2DSelection.Tick`, with `ZENITH_PROFILE_SCOPE` (`Zenith2/Docs/Design/DEVT/Profiler.md → "Zenith_ProfileZone header"`); its scope is `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"`. The on-screen profiler's 2D row shows sprites, batches, resident chunks, bodies (awake / total) and the 2D CPU and GPU times.

### Debug views

| View | Toggle | Shows |
|---|---|---|
| physics shapes | `twod.debug.physics` or the console command `twod.physics.draw 1` | shapes, contact points and normals, sleep state (grey), joints |
| canvas | `twod.canvas.show` | the reference rectangle outlined by the composite pass in the output; the integer scale and the snap offset in the profiler's `twod` row (from `Flux_Sprite2DStats`) |
| batches | `twod.batches.show` | each batch's canvas pixels tinted by a palette colour chosen by batch index (the canvas pass's per-batch push constant); the batch count in the profiler's `twod` row |
| tile chunks | `twod.chunks.show` | a one-texel border on each drawn chunk by the tile program: resident (green), mirrored and wanted but not yet resident (amber), refused (red); the non-resident ones are drawn as border-only chunk instances |

The console commands exist in development builds only; they change presentation alone and never simulation. The extractor copies the three presentation flags into each `Flux_RenderSprite2DList::m_uDebugFlags` (bit 0 canvas, bit 1 batches, bit 2 chunks), and the renderer acts on the list's bits.

### Frame-dump entries

The frame dump gains a `twod` section (when DEVT's frame dump exists): per world, sprite count, batch list (program, blend, first instance, count), canvas geometry (reference size, k, letterbox), resident chunk count, physics gauges.

## Authoring and cooking

### What TWOD authors

TWOD has five authoring document types, one importer (`twod.spriteAtlas`), five cook steps, scene content checks and an editor extension; every one is specified in `Zenith2/Docs/Design/TWOD/Authoring.md`:

| Surface | Heading |
|---|---|
| shared tool functions | `Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"` |
| atlas import and packing | `Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"`, `Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"` |
| cook steps, keys, diagnostics | `Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"`, `Zenith2/Docs/Design/TWOD/Authoring.md → "Cook step keys"`, `Zenith2/Docs/Design/TWOD/Authoring.md → "Cook diagnostics"` |
| scene checks | `Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"` |
| editor | `Zenith2/Docs/Design/TWOD/Authoring.md → "Editor extension"` and the six headings after it |

### Migration and re-cook

Each document type migrates through SCENE's framework with preview, backup and a journaled write, preserving unknown members (`Zenith2/Docs/Design/TWOD/Formats.md → "Document types and migration"`); cooked TWOD files are never migrated: a format version change re-cooks them (`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`).

## Failure handling

### Failure table

| Failure | Behaviour | Evidence |
|---|---|---|
| a cooked TWOD file fails a check | the load fails with `TWOD_ERROR_CORRUPT_ASSET` and the check number; the referencing components draw nothing and collide nothing; one warning per asset | `Test_TwoDFormats_ReaderChecks` |
| a TWOD asset not yet resident | sprites draw nothing, tilemaps draw and collide nothing, animators hold their state; every one resolves on the frame or tick the asset activates | `Test_TwoDLoaders_FailureIsResult` |
| an atlas page not resident | the white fallback for that frame | `Test_Sprite2DRender_PixelExactCanvas` (fallback case) |
| a sprite name missing from its atlas | nothing drawn, `twod.render.missing_sprite_delta` | `Test_Sprite2DExtract_RecordsMatchComponents` |
| allocation failure at start or attach | the step fails and unwinds; the game's start fails with the module error | `Test_TwoDModule_StartUnwindsOnFailure` |
| allocation failure during a tick | none can occur: every per-tick structure is reserved at attach to its configured capacity | capacity tests |
| capacity exhausted (bodies, overlay, pending ops, events, sprites, chunks) | deterministic refusal in merge or key order with its counter | the capacity tests of Tests.md |
| device loss | the 2D GPU state is rebuilt from the renderer's CPU mirrors at the paced upload rate; simulation is untouched | `Test_Sprite2DRender_DeviceLossRecovers` |
| headless | no 2D passes; simulation unchanged | `Test_Sprite2DRender_HeadlessAddsNoPass` |
| a corrupt save section, or an EXACT backend blob that fails its check | `SAVE_ERROR_CORRUPT`, no partial state; SAVE destroys the world (no restore-time downgrade) | `Test_TwoDSave_CorruptSectionRefused` |
| a backend that cannot save exact state | the participant's `GetCaptureFidelity` returns COHERENT, so the capture is COHERENT; the restore rebuilds bodies from components | `Test_TwoDSave_CoherentDowngrade` |
| a tile chunk not GPU-resident | it draws empty until the residency pass uploads it | `Test_Sprite2DRender_PoolPressureRecovers` |
| a physics step failure | logged and counted; a defect report, never a branch | `twod.physics.step_failed_delta`, the scenario suite |
| script misuse | a controlled Luau error of TWOD's block | `Test_TwoDScript_MisuseCodes` |
| invalid authoring document | a cook error naming the rule and pointer; the editor shows it inline | `Test_TwoDValidation_EveryRule` |
| budget breach | the row's breach class of `Zenith2/Docs/Budgets.md → "Breach consequences"`; TWOD has no runtime degradation path (no shedding) | the budget runs |

## Test strategy

### Test targets and gate steps

`Zenith2/Docs/Design/TWOD/Tests.md → "Test targets and files"` maps every target to its directory, presets and gate step; `Zenith2/Docs/Design/TWOD/Tests.md → "Gate steps and labels"` lists what each gate step checks for TWOD. No TWOD test is `needs-gpu`: GPU tests run on WARP and lavapipe (X170).

### Test catalogue index

| Group | Heading |
|---|---|
| unit tests by subsystem | `Zenith2/Docs/Design/TWOD/Tests.md → "Unit tests"` |
| render tests | `Zenith2/Docs/Design/TWOD/Tests.md → "Render tests"` |
| cook and authoring tests | `Zenith2/Docs/Design/TWOD/Tests.md → "Cook and authoring tests"` |
| editor tests | `Zenith2/Docs/Design/TWOD/Tests.md → "Editor tests"` |
| the D-SEL-2DPHYS scenario suite | `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"` |
| integration, fuzz, benchmarks, device runs | `Zenith2/Docs/Design/TWOD/Tests.md → "Integration suite"`, `Zenith2/Docs/Design/TWOD/Tests.md → "Fuzz targets"`, `Zenith2/Docs/Design/TWOD/Tests.md → "Benchmarks"`, `Zenith2/Docs/Design/TWOD/Tests.md → "Device runs"` |

## Budget rows

### Rows owned

| Row | Title | Workloads | Producer |
|---|---|---|---|
| Z2-B-TWOD-001 | Sprite batching and 2D preparation CPU (MP-CPUZONE; the spans `TwoD.Extract`, `Flux.Sprite2D.Prepare`, `Flux.Sprite2D.Write` per frame) | W-UI2D, W-Game2D | Stage 1 per-system budget ticket for TWOD |
| Z2-B-SHIP-022 | Severity-weighted open-defect cap Game2D (SHIP's row; TWOD owns its producer) | W-Game2D | Stage 1 flagship acceptance ticket for Game2D |

### Rows measured against

| Row | Why |
|---|---|
| Z2-B-PACK-004 | Windows install size Game2D |
| Z2-B-CONTENT-004, Z2-B-CONTENT-009 | source-asset quotas of Game2D and WorkloadUI2D |
| Z2-B-CORE-007 | world-transition peak of W-Game2D |
| Z2-B-SCENE-003 | scene activation slice in W-Game2D |
| Z2-B-UI-001, Z2-B-UI-002 | UI rows measured in the same W-UI2D runs |
| Z2-B-PHYS-002 | physics step CPU: its W-Game2D column measures PHYS's nodes only; under the Jolt outcome Game2D selects `Physics` but its 2D bodies are stepped by `TwoD.Physics2DStep`, whose cost is TWOD-004, so PHYS's column measures PHYS's own nodes in either outcome (cross-doc X264 (e)) |
| Z2-B-JOBS-002 | the catch-up bound on ticks per frame that bounds physics steps per frame |

### Pending row needs

New pending rows, owned by TWOD, producer "Stage 1 per-system budget ticket for TWOD", requested through cross-doc item X296; until the row index holds them they are cited by their proposed IDs without the `Z2-B-` prefix:

| Proposed ID | Title | Unit | Procedure | Platforms | Workloads | Breach |
|---|---|---|---|---|---|---|
| TWOD-002 | 2D sprite passes GPU (Sprite2DScatter, Sprite2DCanvas, Sprite2DWorld) | ms | GPU pass timestamps | Win, S24 | W-UI2D, W-Game2D | BR-ACCEPT |
| TWOD-003 | 2D canvas composite GPU (Sprite2DComposite) | ms | GPU pass timestamps | Win, S24 | W-UI2D, W-Game2D | BR-ACCEPT |
| TWOD-004 | 2D physics step CPU (MP-CPUZONE: `TwoD.Physics2DStep` + `TwoD.Physics2DSyncTransforms` per tick) | ms | MP-CPUZONE | Win, S24 | W-UI2D, W-Game2D | BR-ACCEPT |
| TWOD-005 | 2D physics world memory | MB | MP-SUBMEM (the backend's resident bytes) | Win, S24 | W-UI2D, W-Game2D | BR-GOVERNOR |
| TWOD-006 | 2D animation CPU (`TwoD.Animate` + `TwoD.AnimateSkeletons` per tick) | ms | MP-CPUZONE | Win, S24 | W-UI2D, W-Game2D | BR-ACCEPT |
| TWOD-007 | TWOD module resident memory (record tables, poses, overlays, the module's memory consumer) | MB | MP-SUBMEM | Win, S24 | W-UI2D, W-Game2D | BR-GOVERNOR |
| TWOD-008 | 2D renderer memory (the CPU chunk mirror, the GPU chunk pool and tile table, `Flux_Sprite2DStats::m_ulResidentBytes`) | MB | MP-SUBMEM of the RENDERING subsystem | Win, S24 | W-UI2D, W-Game2D | BR-GOVERNOR |

D-SEL-2DPHYS's acceptance rule compares against TWOD-004 and TWOD-005, because 2D physics is stepped by Zenith2D's node under either outcome ("Selection decision D-SEL-2DPHYS"). The harness cannot run TWOD-004's procedure (it has no Zenith2D module, ingest or sync node), so the ruling compares the harness's own zone `Physics2DSelection.Tick` (every seam call one `TwoD.Physics2DStep` would make for that tick) against the row's committed value as an upper bound, and the harness's peak `GetResidentBytes` + `GetProcessResidentBytes` against TWOD-005's (`Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"`): the production row adds the backend-independent ingest, write-back and sync, so a harness value within the row is necessary for the row and the report records the comparison as an upper-bound check. Each W-Game2D row is measured at the content scale the Stage 1 flagship acceptance ticket for Game2D commits, and each W-UI2D row at the scale of UI's Stage 1 workload scale ticket for W-UI2D ([Z2-N-BUDGET-194], [Z2-N-BUDGET-195]; rows TD16, TD17).

## Stage plan

### Stage table

| Stage | TWOD work | Gated by | Acceptance evidence |
|---|---|---|---|
| S0 | none | — | — |
| S1 | the Stage 1 per-system budget ticket for TWOD (rows Z2-B-TWOD-001 and the pending rows); the Stage 1 flagship acceptance ticket for Game2D (content scale, playthrough, Z2-B-SHIP-022), into which TWOD's proposed 2D physics scale goes as an input ("Game2D flagship slice" of Authoring.md) | the CORE, JOBS, RPROD budget tickets | the committed numbers in Budgets; the committed Game2D content scale |
| S2 | the API-only slices below; the `box2d` manifest pin (`needs-human`); the D-SEL-2DPHYS harness ticket (an ordinary loop ticket: seam, both candidates, scenarios, its registered CTest), blocked by PHYS's Stage 1 pinning ticket (`jolt`) and the `box2d` pin; the measurement run ticket (`needs-human`: the scenario suite and the cost scenarios `CostGame2D` and `CostUI2D` on the reference desktop and the S24 at the committed Stage 1 scales, writing the report), blocked by the harness, the Stage 1 flagship acceptance ticket for Game2D and UI's Stage 1 workload scale ticket for W-UI2D; the ruling ticket (`needs-human`), blocked by the run and by the Stage 1 per-system budget ticket for TWOD (which commits TWOD-004 and TWOD-005, the values the rule compares against) | D-SEL-2DPHYS (open, Stage 2) | the harness CTest in `Test`; the report under `Zenith2/Docs/Research/Physics2DSelection/`; the ruling in DecisionLog; when neither candidate passes the gates the ruling ticket stays Blocked and Stage 2 acceptance waits on it |
| S3 | the module, sprites, sort groups, atlases (importer, packer, cook, loader), batching, the canvas, snapping, the composite, the world path, 2D physics (the winner), bindings for physics, sprites and cameras, the save participant, determinism | the D-SEL-2DPHYS ruling; under the Jolt outcome only: a PHYS post-signoff patch lands row TD10 and the PHYS implementation ticket it implies, both before TWOD's J-path tickets (`Internal/Physics2D/Planar/`, the `ZenithPhysics` `DEPENDS`, the `Physics` module dependency, Game2D's `physics.planarWorld`, and the switch of `Zenith_TwoDTestWorld` to the Physics module on Jolt that deletes the Box2D-path test stand-in `Zenith_TwoDTestForeignBody.h`), which stay draft until the ruling (TD10-CONDITIONAL); the Box2D path depends on no PHYS deliverable; RPROD's renderer and extension points (rows TD1, TD2); ASSET's kind rows (TD3); GEOM's usage class (TD4) | unit, render and integration tests; the scenario suite's `Test` cases on the winner |
| S4 | tilemaps (runtime, edits, collision, chunk pool), 2D animation (flipbooks, animator, events), cutout skeletons, the four document cook steps, the editor extension and its panels, the 2D template content, the UI2D sample content, Game2D first playable | EDIT's extension interface and ZenithAuthoring; UI's WorkloadUI2D | editor and cook tests; the walkthrough; Game2D first playable on Windows and the S24 |
| S5 | HDR output of the canvas through `Flux_EncodeOutput` at `r.hdr.uiNits` | RPROD's HDR path | `Test_Sprite2DRender_HDR10Encoding` |
| S6 | sustained runs, fuzzing at scale, Game2D release acceptance (the WS-SLICE rows, the defect cap) | SHIP's acceptance | the flagship acceptance records |
| S7 | none (stereo and consoles outside TWOD) | — | — |

### API-only slices

1. S2: `Zenith_Module_2D.h`, `Zenith_TwoDTypes.h`, `Zenith_Physics2D.h` declarations with stub definitions (no bodies created), and `Flux_Sprite2D.h` with feature functions that add no pass, so dependent areas compile against them before the implementation (the declare-first rule).
2. S3: `Zenith_Sprite2DAnimation.h` and `Zenith_Tilemap.h` declarations, so S3 scene authoring can name the S4 components.

### Ticket slices and ordering

S3: (1) module skeleton and registration; (2) atlas document, packer, importer, cook step, loader; (3) sprite components and extraction; (4) ZenithFlux sort, batch, canvas geometry, snapping; (5) passes and shaders; (6) the 2D camera and composite; (7) physics on the winner: ingest, step, write-back, sync, events, queries (under the Jolt outcome this slice and (8) are J-path tickets, drafted only after the ruling and blocked by the PHYS post-signoff patch of row TD10); (8) one-way, CCD, sleeping, joints; (9) script bindings; (10) save participant; (11) determinism replay. S4: (12) tilesets and tilemaps, runtime and edits; (13) tile collision; (14) chunk pool and tile rendering; (15) flipbooks and the animator; (16) skeletons; (17) the four document cook steps and validators; (18) the editor extension and panels; (19) templates, UI2D content, Game2D first playable.

## Decisions

### Settled decisions

| Decision | What it fixes for TWOD |
|---|---|
| Z2-D-078 | 2D is first class; the scope list |
| Z2-D-021 | Game2D is an equally binding flagship slice |
| Z2-D-019 | determinism per backend and build |
| Z2-D-024 | Zenith2D is a static module selected in `game.json` |
| Z2-D-002 | games contain no C++; Game2D is data and Luau |
| Z2-D-003 | 3D physics is `Zenith_Physics`; candidate J's production realisation, if D-SEL-2DPHYS selects it, reaches Jolt through it (the Physics module's planar world resource, conditional on PHYS's post-signoff patch, TD10-CONDITIONAL); the harness measures the same Jolt directly, because `Zenith_Physics` exists only from PHYS's Stage 3 |
| Z2-D-084 | D-SEL-2DPHYS is a harness ticket plus a `needs-human` ruling |
| Z2-D-038 | JSON authoring documents, versioned binary cooked data |
| Z2-D-054 | bindless: one push constant per draw |
| Z2-D-015 | Slang shader sources |
| Z2-D-080 | twelve phases, per-field authority, commands at sync points |
| Z2-D-050 | render extraction with one frame of CPU render lag |
| Z2-D-045, Z2-D-040, Z2-D-046 | errors, ownership, no mutable globals |
| Z2-D-082 | third-party pinning and `Internal/` confinement apply to `box2d` if selected (and to the harness's use of it before the ruling); Z2-D-082 itself pins and selects nothing |
| Z2-D-048 | migrate documents, re-cook cooked data |

### Open decisions

D-SEL-2DPHYS (open, Stage 2) is TWOD's; TWOD also depends on D-SEL-HASH (open, Stage 1) through `Zenith_Hash64` in name indices (a hash change re-cooks TWOD files and changes nothing else).

### Selection decision D-SEL-2DPHYS

| Field | Value |
|---|---|
| Question | Box2D v3, owned by Zenith2D, or Jolt in a 2D-constrained mode through `Zenith_Physics` (no new dependency) |
| Candidates | B: Box2D v3 (manifest `box2d`, single-threaded per world, soft-step substeps; `Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation"`). J: Jolt 5.4 with `EAllowedDOFs::Plane2D` bodies, extruded shapes and per-chunk mesh chains, measured directly on PHYS's `jolt` entry in the harness (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J harness realisation"`) and, only if selected, realised in production through the Physics module's planar world resource (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"`), which PHYS adds by a post-signoff patch of row TD10 under the orchestrator's ruling TD10-CONDITIONAL |
| Gates | (G1) every scenario of `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"` except `ExactSave` and the two cost scenarios passes; (G2) the `Determinism` scenario passes per build on Windows and on the S24 |
| Rows | the 2D physics step CPU row (TWOD-004) and the 2D physics world memory row (TWOD-005), each at the Stage 1 committed Game2D scale (the Stage 1 flagship acceptance ticket for Game2D) and the committed W-UI2D scale (UI's Stage 1 workload scale ticket for W-UI2D), on the S24 and the reference desktop. The measurements are those of `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"`: `CostGame2D` against the rows' W-Game2D values and `CostUI2D` against their W-UI2D values. A candidate "meets the rows" when, on both platforms and in both scenarios, the p95 of the harness zone `Physics2DSelection.Tick` is at most TWOD-004's committed value and the peak harness resident bytes are at most TWOD-005's committed value. TWOD-004's own procedure also spans the backend-independent ingest, write-back and sync, so the committed value is an upper bound for the harness value: meeting it is necessary for the production row, and the report records each comparison as an upper-bound check |
| Recorded, not gating | `ExactSave` (EXACT capability), API fit, Player code size, licence; each is in the report |
| Acceptance rule | the table below, one total function of the measurements |
| Harness | `Zenith_Bench_Physics2DSelection` (`Zenith2/Benchmarks/Physics2DSelection/`), an ordinary Stage 2 loop ticket whose evidence is its registered CTest `Test_Physics2DSelection_HarnessRuns` (`Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"`); blocked by PHYS's Stage 1 pinning ticket (`jolt`) and the `needs-human` `box2d` pin ticket (owner the harness, usage tools) |
| Measurement run | a `needs-human` Stage 2 ticket that runs the harness on the reference desktop and the S24 at the committed scales and commits `Zenith2/Docs/Research/Physics2DSelection/<date>.md` (every scenario's pass or fail per candidate, the determinism hashes per build, the p50 and p95 of `Physics2DSelection.Tick` and the peak resident bytes per candidate, cost scenario and platform, the recorded criteria); blocked by the harness ticket, the Stage 1 flagship acceptance ticket for Game2D and UI's Stage 1 workload scale ticket for W-UI2D |
| Ruling | a `needs-human` Stage 2 ticket, blocked by the measurement run and by the Stage 1 per-system budget ticket for TWOD (which commits TWOD-004 and TWOD-005), that applies the acceptance rule to the report and appends the ruling to DecisionLog with an explicit `Z2-D-NNN` and states the outcome in this design, its diff touching only `Zenith2/Docs/`; the implementation ticket that follows it moves the seam header into `Internal/Physics2D/`, deletes the harness implementation of the loser, and under the Box2D outcome moves `Internal/Box2D/` into `Internal/Physics2D/Box2D/` and applies the Box2D consequences (the `box2d` owner and usage change, `THIRD_PARTY box2d`, X309 (b) as amended by X332 (b)). Under the Jolt outcome only, neither the ruling nor that implementation ticket carries J production code: a PHYS post-signoff patch lands row TD10 and the PHYS implementation ticket it implies, and only then are TWOD's J-path tickets (`Internal/Physics2D/Planar/`, `ZenithPhysics` in `DEPENDS`, the `Physics` module dependency, `physics.planarWorld` in Game2D's configuration, X297 (d)) taken out of draft (TD10-CONDITIONAL) |
| Consumers blocked | TWOD's physics tickets (S3), and under the Jolt outcome also the PHYS post-signoff patch; Stage 2 acceptance waits on the ruling ticket only in the "neither passes the gates" row |

| Gates (G1 and G2) | Rows | Selected | Recorded with the ruling |
|---|---|---|---|
| both pass | both meet | J (no new dependency) | — |
| both pass | exactly one meets | the one that meets | the other's breach |
| both pass | neither meets | the candidate with the lower Game2D-scale p95 step time on the S24 | both breaches; the winner's breach against its rows |
| exactly one passes | any | the one that passes | its row breach, if any; the other's failing scenarios |
| neither passes | — | none: the ruling ticket stays Blocked naming every failing scenario of each candidate, and Stage 2 acceptance waits on it | — |

Ties after the rows (two candidates in the same row of the table and the row's selection still undecided: equal p95 to the reported resolution of 0.01 ms in the "neither meets" row) are broken by EXACT capability (the candidate whose `ExactSave` scenario passes wins), then by J. A winner whose `SupportsExactState` is false ships COHERENT 2D captures ("Save participation" of Simulation.md), which the per-capture fidelity rule admits. This is the orchestrator's ruling on the D-SEL-2DPHYS acceptance rule (2026-10-09; the human may override), applied to DecisionLog and Architecture's "Named selection decisions" row through cross-doc item X299.

### Design rulings owned by TWOD

| # | Ruling | Where |
|---|---|---|
| TW1 | The 2D GPU side lives in ZenithFlux (`Internal/Sprite2D/`, `Flux_Sprite2D.h`), on UI's precedent, because passes and render-world records are Flux's; the module reaches it only downward | "Library and file layout" |
| TW2 | Every canvas path is `R8G8B8A8_SRGB` with point sampling and an integer composite; pixel exactness is byte equality, tested without tolerance | `Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"` |
| TW3 | Batches break only on program or blend; textures are bindless page indices in the instance | `Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm"` |
| TW4 | Tile edits are residency-independent overlay writes in merge order; collision follows edits at the next ingest | `Zenith2/Docs/Design/TWOD/Simulation.md → "Tile edit playback"` |
| TW5 | 2D physics has its own writer source in phase 6, and an entity never carries both physics bodies | `Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D write-back and sync"` |
| TW6 | Atlas packing is deterministic MaxRects BSSF without rotation, shared by the cook and the editor through ZenithAuthoring | `Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing"` |
| TW7 | 2D animation never moves an entity's transform (no root motion) | `Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"` |
| TW8 | Tile chunks are sent once per change into a renderer-owned CPU mirror; GPU residency is the renderer's least-recently-wanted pool, so no feedback reaches simulation | `Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"` |
| TW9 | If D-SEL-2DPHYS selects Jolt-2D, candidate J's production world is the Physics module's planar world resource (added by PHYS's post-signoff patch, TD10-CONDITIONAL), written only by TWOD's node; Zenith2D never constructs Jolt | `Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"` |
| TW10 | The canvas holds premultiplied linear colour; every blend mode's alpha factors are fixed | `Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DCanvas pass"` |

## Requirement index

Every requirement of `Zenith2/Docs/Requirements/TWOD.md` and the headings that specify it:

| Requirement | Title | Design headings |
|---|---|---|
| Z2-R-TWOD-001 | Module library target | Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD.md → "Target summary"; Zenith2/Docs/Design/TWOD.md → "Zenith2D files" |
| Z2-R-TWOD-002 | Module entry points | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Module_2D header"; Zenith2/Docs/Design/TWOD.md → "Component table"; Zenith2/Docs/Design/TWOD.md → "World resources events and commands" |
| Z2-R-TWOD-003 | Start sequence unwinds | Zenith2/Docs/Design/TWOD.md → "Module start sequence"; Zenith2/Docs/Design/TWOD.md → "World attach and detach" |
| Z2-R-TWOD-004 | Public headers carry contracts | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Scope of this file"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Header list" |
| Z2-R-TWOD-005 | API-only slices land first | Zenith2/Docs/Design/TWOD.md → "API-only slices"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Header list" |
| Z2-R-TWOD-006 | Error codes are stable | Zenith2/Docs/Design/TWOD.md → "Error enumerations"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header" |
| Z2-R-TWOD-007 | Configuration keys registered | Zenith2/Docs/Design/TWOD.md → "Configuration keys" |
| Z2-R-TWOD-008 | Sprite component | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DComponents header"; Zenith2/Docs/Design/TWOD.md → "Component table" |
| Z2-R-TWOD-009 | Sort groups | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DComponents header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys" |
| Z2-R-TWOD-010 | Sorting layers and y-sort | Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"; Zenith2/Docs/Design/TWOD.md → "Configuration keys" |
| Z2-R-TWOD-011 | Draw modes | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-012 | Extraction writes render-world records | Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Flux_Sprite2D header" |
| Z2-R-TWOD-013 | Extraction is all-or-nothing per slot | Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour" |
| Z2-R-TWOD-014 | Sort keys | Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys" |
| Z2-R-TWOD-015 | Sort independent of layout | Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD" |
| Z2-R-TWOD-016 | Batching breaks only on program and blend | Zenith2/Docs/Design/TWOD/Rendering.md → "Batching algorithm" |
| Z2-R-TWOD-017 | Frame data through the upload ring | Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path" |
| Z2-R-TWOD-018 | Sprite passes in the render graph | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DCanvas pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DWorld pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds" |
| Z2-R-TWOD-019 | Stream-sets | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite stream-sets" |
| Z2-R-TWOD-020 | Sprite cap | Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour" |
| Z2-R-TWOD-021 | Blend modes | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Colour and HDR output" |
| Z2-R-TWOD-022 | World-space sprites | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DWorld pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds" |
| Z2-R-TWOD-023 | Headless adds no pass | Zenith2/Docs/Design/TWOD/Rendering.md → "Headless and device loss"; Zenith2/Docs/Design/TWOD.md → "Module start sequence" |
| Z2-R-TWOD-024 | Device loss recovery | Zenith2/Docs/Design/TWOD/Rendering.md → "Headless and device loss" |
| Z2-R-TWOD-025 | Missing page fallback | Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour" |
| Z2-R-TWOD-026 | Atlas document schema | Zenith2/Docs/Design/TWOD/Formats.md → "Sprite atlas document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules" |
| Z2-R-TWOD-027 | Atlas importer | Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer" |
| Z2-R-TWOD-028 | Deterministic packing | Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing" |
| Z2-R-TWOD-029 | Trim and extrude | Zenith2/Docs/Design/TWOD/Authoring.md → "Atlas packing" |
| Z2-R-TWOD-030 | Point atlases are exact | Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite atlas importer"; Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules" |
| Z2-R-TWOD-031 | Cooked atlas format | Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite atlas format"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules" |
| Z2-R-TWOD-032 | Atlas frame lookup | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_SpriteAtlas header"; Zenith2/Docs/Design/TWOD/Formats.md → "Common rules" |
| Z2-R-TWOD-033 | Atlas cook step and keys | Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook step keys" |
| Z2-R-TWOD-034 | Tileset document and format | Zenith2/Docs/Design/TWOD/Formats.md → "Tileset document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked tileset format" |
| Z2-R-TWOD-035 | Tilemap document and format | Zenith2/Docs/Design/TWOD/Formats.md → "Tilemap document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked tilemap format" |
| Z2-R-TWOD-036 | Tile cell encoding | Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding" |
| Z2-R-TWOD-037 | Tilemap component | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Tilemap header"; Zenith2/Docs/Design/TWOD.md → "Component table" |
| Z2-R-TWOD-038 | Effective cell | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime" |
| Z2-R-TWOD-039 | Tile edits in merge order | Zenith2/Docs/Design/TWOD/Simulation.md → "Tile edit playback" |
| Z2-R-TWOD-040 | Overlay capacity | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime" |
| Z2-R-TWOD-041 | Tile collision outlines | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"; Zenith2/Docs/Design/TWOD/Formats.md → "Tile collision kinds" |
| Z2-R-TWOD-042 | No bumps across chunk seams | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation" |
| Z2-R-TWOD-043 | Tile chunk extraction | Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-044 | GPU chunk pool and tile table | Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DScatter pass"; Zenith2/Docs/Design/TWOD.md → "State machine index" |
| Z2-R-TWOD-045 | Tile edits visible next frame | Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool" |
| Z2-R-TWOD-046 | Tilemap hot reload keeps edits | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap hot reload" |
| Z2-R-TWOD-047 | Animation document and format | Zenith2/Docs/Design/TWOD/Formats.md → "Sprite animation document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked sprite animation format" |
| Z2-R-TWOD-048 | Flipbook frame selection | Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation" |
| Z2-R-TWOD-049 | Paused and zero speed | Zenith2/Docs/Design/TWOD/Simulation.md → "Flipbook evaluation" |
| Z2-R-TWOD-050 | Animator transitions | Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine" |
| Z2-R-TWOD-051 | Triggers | Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine" |
| Z2-R-TWOD-052 | Animator commands | Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine"; Zenith2/Docs/Design/TWOD.md → "Luau errors and asserts" |
| Z2-R-TWOD-053 | Animation events | Zenith2/Docs/Design/TWOD/Simulation.md → "Animation events"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDEvents header" |
| Z2-R-TWOD-054 | Events independent of workers | Zenith2/Docs/Design/TWOD/Simulation.md → "Animation events"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD" |
| Z2-R-TWOD-055 | Animation hot reload | Zenith2/Docs/Design/TWOD/Simulation.md → "Animation hot reload" |
| Z2-R-TWOD-056 | Rig document and format | Zenith2/Docs/Design/TWOD/Formats.md → "Rig document schema"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked rig format" |
| Z2-R-TWOD-057 | Cutout skeleton evaluation | Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Skeleton2D header" |
| Z2-R-TWOD-058 | Slots and draw order | Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-059 | No root motion | Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement" |
| Z2-R-TWOD-060 | D-SEL-2DPHYS harness | Zenith2/Docs/Design/TWOD/Simulation.md → "Seam header placement"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"; Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS" |
| Z2-R-TWOD-061 | D-SEL-2DPHYS ruling | Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS" |
| Z2-R-TWOD-062 | Box2D pin | Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation" |
| Z2-R-TWOD-063 | Backend seam is internal | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam"; Zenith2/Docs/Design/TWOD/Simulation.md → "Seam header placement" |
| Z2-R-TWOD-064 | Physics components | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Physics2D header"; Zenith2/Docs/Design/TWOD.md → "Component table" |
| Z2-R-TWOD-065 | Ingest in simulation-ID order | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"; Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD" |
| Z2-R-TWOD-066 | One physics body per entity | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D ingest"; Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"; Zenith2/Docs/Design/TWOD/Tests.md → "Test support" |
| Z2-R-TWOD-067 | Single-threaded deterministic step | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam" |
| Z2-R-TWOD-068 | External writes adopted | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D external writes" |
| Z2-R-TWOD-069 | Physics commands | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D pending operations" |
| Z2-R-TWOD-070 | Write-back and sync | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D write-back and sync" |
| Z2-R-TWOD-071 | Contact and trigger events | Zenith2/Docs/Design/TWOD/Simulation.md → "Contact and trigger events"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDEvents header" |
| Z2-R-TWOD-072 | Joints | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Physics2D header"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite" |
| Z2-R-TWOD-073 | One-way platforms | Zenith2/Docs/Design/TWOD/Simulation.md → "One-way platforms" |
| Z2-R-TWOD-074 | Continuous collision | Zenith2/Docs/Design/TWOD/Simulation.md → "Continuous collision" |
| Z2-R-TWOD-075 | Sleeping | Zenith2/Docs/Design/TWOD/Simulation.md → "Sleeping" |
| Z2-R-TWOD-076 | Queries | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D queries" |
| Z2-R-TWOD-077 | Physics capacities | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"; Zenith2/Docs/Design/TWOD.md → "Failure table" |
| Z2-R-TWOD-078 | Scenario suite as a gate | Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "Integration suite" |
| Z2-R-TWOD-079 | Camera component | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD.md → "Component table" |
| Z2-R-TWOD-080 | Canvas geometry rules | Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry" |
| Z2-R-TWOD-081 | Camera mapping functions | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header" |
| Z2-R-TWOD-082 | Snap modes | Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping" |
| Z2-R-TWOD-083 | Camera-relative snapping | Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-084 | Camera bounds | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-085 | Pure-2D views | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Camera2D header"; Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds" |
| Z2-R-TWOD-086 | Several canvases | Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"; Zenith2/Docs/Design/TWOD/Rendering.md → "Render failure behaviour" |
| Z2-R-TWOD-087 | Pixel-exact canvas | Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"; Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping" |
| Z2-R-TWOD-088 | Flips and symmetries exact | Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules"; Zenith2/Docs/Design/TWOD/Formats.md → "Tile cell encoding" |
| Z2-R-TWOD-089 | Integer composite | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Canvas geometry" |
| Z2-R-TWOD-090 | Sprite corner snapping | Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping"; Zenith2/Docs/Design/TWOD/Rendering.md → "Pixel exactness rules" |
| Z2-R-TWOD-091 | UI composites over the canvas | Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds" |
| Z2-R-TWOD-092 | HDR output | Zenith2/Docs/Design/TWOD/Rendering.md → "Colour and HDR output" |
| Z2-R-TWOD-093 | Editor game view matches the Player | Zenith2/Docs/Design/TWOD/Authoring.md → "2D scene view mode" |
| Z2-R-TWOD-094 | Document types registered | Zenith2/Docs/Design/TWOD/Formats.md → "Document types and migration"; Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions" |
| Z2-R-TWOD-095 | Unknown members preserved | Zenith2/Docs/Design/TWOD/Formats.md → "Common rules" |
| Z2-R-TWOD-096 | Validation rules | Zenith2/Docs/Design/TWOD/Formats.md → "Validation rules"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook diagnostics" |
| Z2-R-TWOD-097 | Document cook steps | Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Authoring.md → "Cook step keys" |
| Z2-R-TWOD-098 | Cook validators | Zenith2/Docs/Design/TWOD/Authoring.md → "Cook steps"; Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks" |
| Z2-R-TWOD-099 | Shared authoring functions | Zenith2/Docs/Design/TWOD/Authoring.md → "ZenithAuthoring functions"; Zenith2/Docs/Design/TWOD/Authoring.md → "Tool-side layout" |
| Z2-R-TWOD-100 | Scene content checks | Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks" |
| Z2-R-TWOD-101 | Editor extension | Zenith2/Docs/Design/TWOD/Authoring.md → "Editor extension" |
| Z2-R-TWOD-102 | Atlas editor | Zenith2/Docs/Design/TWOD/Authoring.md → "Sprite and atlas editor" |
| Z2-R-TWOD-103 | Tilemap painter | Zenith2/Docs/Design/TWOD/Authoring.md → "Tilemap painter" |
| Z2-R-TWOD-104 | Play-mode painting uses commands | Zenith2/Docs/Design/TWOD/Authoring.md → "Tilemap painter" |
| Z2-R-TWOD-105 | Animation and rig editors | Zenith2/Docs/Design/TWOD/Authoring.md → "2D animation editor"; Zenith2/Docs/Design/TWOD/Authoring.md → "Rig editor" |
| Z2-R-TWOD-106 | 2D authoring workflow | Zenith2/Docs/Design/TWOD/Authoring.md → "2D authoring workflow" |
| Z2-R-TWOD-107 | 2D template content | Zenith2/Docs/Design/TWOD/Authoring.md → "2D project template content" |
| Z2-R-TWOD-108 | UI2D sample 2D layer | Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content" |
| Z2-R-TWOD-109 | Generated sample content | Zenith2/Docs/Design/TWOD/Authoring.md → "UI2D sample content"; Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice" |
| Z2-R-TWOD-110 | Game2D definition | Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice" |
| Z2-R-TWOD-111 | Game2D flagship acceptance ticket | Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"; Zenith2/Docs/Design/TWOD.md → "Rows owned" |
| Z2-R-TWOD-112 | Game2D first playable | Zenith2/Docs/Design/TWOD/Authoring.md → "Game2D flagship slice"; Zenith2/Docs/Design/TWOD/Tests.md → "Device runs" |
| Z2-R-TWOD-113 | Game2D release acceptance | Zenith2/Docs/Design/TWOD.md → "Stage table"; Zenith2/Docs/Design/TWOD.md → "Rows owned" |
| Z2-R-TWOD-114 | Per-system budget ticket | Zenith2/Docs/Design/TWOD.md → "Pending row needs"; Zenith2/Docs/Design/TWOD.md → "Rows owned" |
| Z2-R-TWOD-115 | 2D CPU within budget | Zenith2/Docs/Design/TWOD/Tests.md → "Benchmarks"; Zenith2/Docs/Design/TWOD.md → "Rows owned" |
| Z2-R-TWOD-116 | Determinism replay | Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD" |
| Z2-R-TWOD-117 | Presentation never reaches simulation | Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"; Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction" |
| Z2-R-TWOD-118 | Save participant | Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation"; Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format" |
| Z2-R-TWOD-119 | Coherent downgrade | Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation" |
| Z2-R-TWOD-120 | Corrupt save refused | Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format"; Zenith2/Docs/Design/TWOD/Simulation.md → "Save participation" |
| Z2-R-TWOD-121 | Script bindings | Zenith2/Docs/Design/TWOD.md → "Luau surface"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDScriptApi header" |
| Z2-R-TWOD-122 | Misuse codes | Zenith2/Docs/Design/TWOD.md → "Luau errors and asserts" |
| Z2-R-TWOD-123 | Memory consumer | Zenith2/Docs/Design/TWOD/Simulation.md → "Memory consumer" |
| Z2-R-TWOD-124 | Cooked readers never assert | Zenith2/Docs/Design/TWOD/Formats.md → "Cooked reader checks" |
| Z2-R-TWOD-125 | Parser limits | Zenith2/Docs/Design/TWOD/Formats.md → "Parser limits" |
| Z2-R-TWOD-126 | Schedule access | Zenith2/Docs/Design/TWOD/Simulation.md → "Phase placement"; Zenith2/Docs/Design/TWOD.md → "Frame-graph placement" |
| Z2-R-TWOD-127 | No locks | Zenith2/Docs/Design/TWOD.md → "Lock-order entries"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam" |
| Z2-R-TWOD-128 | Diagnostics | Zenith2/Docs/Design/TWOD.md → "Counters"; Zenith2/Docs/Design/TWOD.md → "Profiler zones"; Zenith2/Docs/Design/TWOD.md → "Debug views" |
| Z2-R-TWOD-129 | Command kind IDs resolved at attach | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"; Zenith2/Docs/Design/TWOD.md → "World attach and detach" |
| Z2-R-TWOD-130 | Animator command payloads | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DAnimation header"; Zenith2/Docs/Design/TWOD/Simulation.md → "Animator state machine" |
| Z2-R-TWOD-131 | Script query value structs | Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDScriptApi header"; Zenith2/Docs/Design/TWOD.md → "Luau surface" |
| Z2-R-TWOD-132 | Tilemap parallax and opacity | Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"; Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime" |
| Z2-R-TWOD-133 | Premultiplied canvas blending | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DCanvas pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass" |
| Z2-R-TWOD-134 | Signed composite indexing | Zenith2/Docs/Design/TWOD/Rendering.md → "Sprite2DComposite pass"; Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping" |
| Z2-R-TWOD-135 | Integer-domain snapping | Zenith2/Docs/Design/TWOD/Rendering.md → "Snapping" |
| Z2-R-TWOD-136 | Camera-invariant y sort | Zenith2/Docs/Design/TWOD/Rendering.md → "Sort keys" |
| Z2-R-TWOD-137 | Physics origin shift | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D origin shift" |
| Z2-R-TWOD-138 | D-SEL-2DPHYS measurement run | Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"; Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure" |
| Z2-R-TWOD-139 | Trigger reach | Zenith2/Docs/Design/TWOD/Simulation.md → "Contact and trigger events"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam" |
| Z2-R-TWOD-140 | Joint break force | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D step"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D backend seam" |
| Z2-R-TWOD-141 | Axis-aligned tilemaps follow their entity | Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap collision generation"; Zenith2/Docs/Design/TWOD/Simulation.md → "Tilemap runtime"; Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks" |
| Z2-R-TWOD-142 | Box2D process state | Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate B realisation" |
| Z2-R-TWOD-143 | Canvas backend parity | Zenith2/Docs/Design/TWOD/Tests.md → "Gate steps and labels"; Zenith2/Docs/Design/TWOD/Tests.md → "Render tests" |
| Z2-R-TWOD-144 | Physics 2D writer source | Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D write-back and sync" |
| Z2-R-TWOD-145 | Jolt-2D through the planar world | Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation" |
| Z2-R-TWOD-146 | D-SEL-2DPHYS cost procedure | Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"; Zenith2/Docs/Design/TWOD.md → "Selection decision D-SEL-2DPHYS"; Zenith2/Docs/Design/TWOD.md → "Pending row needs" |
| Z2-R-TWOD-147 | Box2D path needs no PHYS deliverable | Zenith2/Docs/Design/TWOD.md → "Areas TWOD depends on"; Zenith2/Docs/Design/TWOD.md → "Libraries Zenith2D depends on"; Zenith2/Docs/Design/TWOD.md → "Stage table" |
| Z2-R-TWOD-148 | Deterministic trigonometry | Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"; Zenith2/Docs/Design/TWOD/Simulation.md → "Physics 2D model"; Zenith2/Docs/Design/TWOD/Simulation.md → "Cutout skeleton evaluation" |
| Z2-R-TWOD-149 | Explicit script names | Zenith2/Docs/Design/TWOD.md → "Component table"; Zenith2/Docs/Design/TWOD.md → "World resources events and commands"; Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Module_2D header" |

## Cross-area reconciliation

### Cross-area rows

The rows TWOD filed in `findings/A3_cross_area_notes.md` (TD1 … TD24) and the cross-doc items it raised (X295 … X300, X332 in revision 1, and X348 in revision 2: the 2D canvas row of Rendering's capability matrix with its parity tolerance):

| Row | To | Need |
|---|---|---|
| TD1 | RPROD, REND | `Flux_Renderer::Enable2D` (declaration RPROD's, body TWOD's), the `PrepareFrame`/`WriteFrame`/`EndFrame` calls at `Flux.BuildGraph` steps 4, 11 and 13, `Flux_FrameShared::m_px2DFrame`, the three render-world section spans and writer members (X295) |
| TD2 | RPROD, REND | `FLUX_EXTENSION_POINT_BEFORE_UI = 5`, called after TonemapAndGrade and before UIComposite (X295) |
| TD3 | ASSET | the five kind rows; the `zenith-doc` passthrough claims the four document suffixes |
| TD4 | GEOM | the usage class `FLUX_TEXTURE_USAGE_COLOUR_EXACT`; the TWOD importer as a `geom.textureSource` writer |
| TD5 | REND | the required-format row `R8G8B8A8_SRGB` gains the canvas and the sampled pages as TWOD uses |
| TD6 | GEOM | the SPRITE stream-set family (`0x3001` … `0x3003`, stream 4, locations 8–13) in the stream-set table and validation |
| TD7 | RPROD | the view's source world tag on `Flux_ViewContext` (shared with UI's UI8 (d)); a pure-2D view (`(uFeatureMask & uTWOD_PURE_2D_FEATURE_CLEAR_MASK) == 0`) culls every 3D pass |
| TD8 | EDIT | the 2D row of the extension table and the hosted-tools table filled from this design; ZenithAuthoring `DEPENDS` gains Zenith2D; the game view honours the pure-2D mask |
| TD9 | ARCH | `WRITER_SOURCE_PHYSICS_2D = 6`, schema name `Physics2D`, precedence after physics and before animation (X300 as amended by X309 (a)) |
| TD10 | PHYS | CONDITIONAL (orchestrator ruling TD10-CONDITIONAL, 2026-10-10): only if D-SEL-2DPHYS selects Jolt-2D, a PHYS post-signoff patch, landed before any TWOD J-path implementation ticket, adds `physics.planarWorld` and the planar world resource `Zenith.Physics.PlanarWorld` with the members and the five production clauses of "Candidate J production realisation" (Simulation.md: Jolt backend only, no double shift, no double count, phase-5 concurrency, hook lifetime); PHYS's A3 post-round did not take it. The harness needs nothing from PHYS but the Stage 1 `jolt` pin and PHYS's build options and world settings |
| TD11 | SAVE | withdrawn in revision 1: TWOD's participant overrides `GetCaptureFidelity`, so fidelity is decided per capture and no restore-time downgrade member is needed |
| TD12 | SCENE | a component content-check registration in the `scene.scene` step, so `Zenith_CheckTwoDSceneContent` runs in the cook |
| TD13 | core docs | [Z2-N-CONTRACT-552] lists the five TWOD document types (X297) |
| TD14 | core docs | the "Parser limits" table gains TWOD's rows (X297) |
| TD15 | MOD | the 2D template's content when the 2D module is selected |
| TD16 | UI | WorkloadUI2D carries TWOD's 2D layer; the UI2D sample split; TWOD's proposed W-UI2D 2D physics scale (500 dynamic bodies) as an input to UI's Stage 1 workload scale ticket |
| TD17 | SHIP, Budgets | the Game2D content scale committed by the Stage 1 flagship acceptance ticket for Game2D in `Zenith2/Docs/Research/Workloads/W-Game2D.md` (not a ContentSpecs.md section, whose section list is fixed); TWOD's proposed 2D physics scale (2,000 dynamic bodies, 20,000 tile unit edges, 200 joints) as an input (X332 (a)) |
| TD18 | SCRIPT | the misuse block ZS2400–ZS2499 (X298) |
| TD19 | Budgets | the seven pending TWOD rows (TWOD-008 added in revision 1) and the producer table (X296) |
| TD20 | ARCH | the Zenith2D `DEPENDS` widening; "the 2D GPU side is TWOD's files in ZenithFlux" in the ownership table (X297) |
| TD21 | MOD, ARCH | Game2D's `game.json` modules; under the Jolt outcome only (after the TD10 patch), the `2D` row's module dependency "Physics (Jolt)", Game2D's `Physics` (Jolt) selection and `physics.planarWorld`; no factory parameter under either outcome |
| TD22 | SCRIPT | how a BUFFERED thunk obtains a module's command-kind ID for a `SIDE_EFFECT_KIND_FIELD_COMMAND` record (the same open route GAME's and PHYS's bindings use); that a fixed-array field of a returned value struct marshals as a Luau array of N elements with null handles as nil |
| TD23 | ARCH, MOD | `Zenith_WorldAttachContext::FindCommandKind` (shared with PH7 and GA5 (c)) |
| TD24 | STREAM, SCENE, ECS | TWOD's lifecycle listener consumes `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (X264 (j)); `PROPERTY_FLAG_WORLD_POSITION` admits VEC2 properties, shifted by (S.x, S.y), for TWOD's world-space VEC2 properties `Zenith.TwoD.Camera` 10–11 and `Zenith.TwoD.Joint` 20 WorldAnchor (X332 (c)); `Zenith.TwoD.Joint` 5 AnchorB is B-local and is never flagged |

### Assumed interfaces pending other areas

Each is written in an `assumption` block where it is used: the Rendering additions (`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Flux_Sprite2D header"`, `Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"`), the Zenith2D `DEPENDS` row ("Libraries Zenith2D depends on"), the ASSET kind codes ("Asset kinds"), CORE's deterministic trigonometric functions `Zenith_Sin`, `Zenith_Cos` and `Zenith_Atan2` and the CRT maths mode call `Zenith_ApplyCrtMathMode` (the assumption block under the table of `Zenith2/Docs/Design/TWOD/Simulation.md → "Determinism rules for TWOD"`, X314; the names are those `Zenith2/Docs/Design/CORE/Maths.md → "Transcendental declarations"` declares, pending CORE's X314 follow-up) and, under the Jolt outcome only, PHYS's planar world (`Zenith2/Docs/Design/TWOD/Simulation.md → "Candidate J production realisation"`, row TD10, conditional under TD10-CONDITIONAL).
