# Zenith2 VFX design: particles and effects

The design of the VFX area: CPU and GPU particles, ribbons and trails, collision, bounds, culling, sorting, LOD, event integration, budgets, and the previews and inspectors of code-authored effects. This entry file holds the purpose, the dependencies, the library layout, the top-level contracts (configuration, frame-graph placement, threading, errors, diagnostics, failures, budgets, stages, decisions) and the requirement index; the section files below hold the full API, formats, algorithms, rendering, authoring and tests. The normative sources are `Zenith2/Docs/Rendering.md` (the render world's emitter commands, the particle pool, the particle passes and the capability row "Particles and ribbons (soft)"), `Zenith2/Docs/Architecture.md` (the module, its layer and its headless behaviour) and Z2-D-079 (effects are authored in code, with previews).

## Purpose and scope

### What VFX owns

| Owned | Where |
|---|---|
| the `ZenithVFX` module: the effect components, the attached-effect lifecycle, detached effects and their trigger queue, the presentation-side spawn scheduling, LOD, admission and CPU particle simulation, effect lights and decals, the render extractor, the Luau `VFX` namespace and the `VFX.Spawn` command kind | `Zenith2/Engine/ZenithVFX/` |
| the particle effect asset kind (`ASSET_KIND_PARTICLE_EFFECT`, `.zvfx`), its loader and reader | `Zenith2/Engine/ZenithFlux/Flux_ParticleEffect.h` and `Internal/Particles/` |
| the render side: the particle system, the particle pool's policy, the five particle passes, sorting, the draw programs and their PSO keys | `Zenith2/Engine/ZenithFlux/Flux_Particles.h`, `Internal/Particles/`, `Internal/Passes/Particles/`, `Zenith2/Shaders/Passes/Particles/`, `Zenith2/Shaders/Particles/` |
| the effect document `*.vfx.json`, its schema and migration chain, the `vfx.effect` and `vfx.particleTypes` cook steps | `Zenith2/Tools/ZenithCook/Internal/VFX/` |
| the engine effect content | `Zenith2/Engine/ZenithVFX/Content/` |

### Explicit exclusions

| Not VFX | Owner | Reason |
|---|---|---|
| the pool allocation mechanism (`CreatePool`, ranges, regions) | RHI (`Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"`) | VFX owns the particle pool's policy alone: region sizes, growth, range lifetime |
| the render graph, the per-pass table conventions, buckets, render extraction ordering and the PSO cache | RPROD | VFX's passes are ordinary feature functions; its extractor is ordered by RPROD's rule (`Before("Render.Seal@W0")`, set by `AddExtractor`) |
| the lighting function, light records, clusters and froxel fog | LIGHT | lit particles call LIGHT's functions and write LIGHT's light deltas |
| vertex formats, mesh resources, decal records and decal order | GEOM | mesh particles draw GEOM's meshes; effect decals are GEOM decal records |
| weather state, rain and snow presence, wind and precipitation occlusion data | ENV | ENV's prefabs name VFX effects; VFX reads wind and occlusion on the GPU, and the render side reads each world's wind block from the render environment for the wind term of particle bounds |
| sound attached to effects | AUDIO | AUDIO subscribes to VFX's events by stable name or authors its own components on the same prefab |
| destruction debris that affects the world | PHYS, D-CLASS-SIM (open, Stage 2) | particles are presentation only; VFX's mesh particles are presentation debris |
| the editor panels hosting the preview and the inspector | EDIT (Z2-D-079) | VFX supplies the preview world behaviour, the data and the statistics |
| tile-local merging of particle passes on the S24 | RPROD, D-TBDR (open, Stage 5), Z2-D-036 | no VFX requirement waits on it |

### Inventory items covered

| Item | Title | Where specified |
|---|---|---|
| Z2-I-VFX-01 | CPU particles | `Zenith2/Docs/Design/VFX/Simulation.md → "CPU simulation"` |
| Z2-I-VFX-02 | GPU particles | `Zenith2/Docs/Design/VFX/Simulation.md → "GPU simulation"`, `Zenith2/Docs/Design/VFX/Rendering.md → "Pass tables"` |
| Z2-I-VFX-03 | ribbons | `Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"` |
| Z2-I-VFX-04 | trails | `Zenith2/Docs/Design/VFX/Simulation.md → "Particle trails"`, `Zenith2/Docs/Design/VFX/Rendering.md → "Particle trail ribbons"` |
| Z2-I-VFX-05 | particle collision | `Zenith2/Docs/Design/VFX/Simulation.md → "Collision"` |
| Z2-I-VFX-06 | particle bounds | `Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"`, `Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"` |
| Z2-I-VFX-07 | particle culling | `Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling"`, `Zenith2/Docs/Design/VFX/Simulation.md → "Culling behaviour"` |
| Z2-I-VFX-08 | particle sorting | `Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm"`, `Zenith2/Docs/Design/VFX/Rendering.md → "Draw ordering"` |
| Z2-I-VFX-09 | VFX LOD | `Zenith2/Docs/Design/VFX/Simulation.md → "LOD and significance"` |
| Z2-I-VFX-10 | VFX event integration | `Zenith2/Docs/Design/VFX/Simulation.md → "Attached effect lifecycle"`, `Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"`, `Zenith2/Docs/Design/VFX/Rendering.md → "Effect lights"` |
| Z2-I-VFX-11 | VFX budgets | "Budget rows", `Zenith2/Docs/Design/VFX/Simulation.md → "Budgets and admission"` |
| Z2-I-VFX-12 | previews for code-authored effects | `Zenith2/Docs/Design/VFX/Authoring.md → "Effect preview"` |
| Z2-I-VFX-13 | inspectors for code-authored effects | `Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector"` |

All thirteen are required at Stage 5; none is excluded or deferred.

### Section files

| File | Headings |
|---|---|
| `Zenith2/Docs/Design/VFX/PublicAPI.md` | Scope of this file · Header map · Zenith_Module_VFX header (Module shutdown order) · Zenith_VFXErrors header · Zenith_VFXComponents header (Zenith_VFXEffectComponent, Zenith_VFXColliderComponent, Component registration, Authority census rows) · Zenith_VFXEvents header · Zenith_VFXCommands header · Zenith_VFXReference header · Zenith_VFXScriptBindings (Luau surface, VFX trigger record) · Flux_ParticleEffect header (Effect enumerations, Flux_ParticleEmitterDesc, Zenith_ParticleEffectAsset, Effect kind loader) · Flux_Particles header (Instance keys, Flux_RenderEmitterCommand, Flux_RenderParticleCollider, Flux_ParticleSystem, Flux_ParticleStats, Particle random functions, Particle wind bound) · Internal Flux particle declarations (Flux_ParticleFrame, Flux_ParticleSystem internals) · Shared GPU structs (Flux_ParticleState, Flux_EmitterCommand, Flux_ParticleDrawRecord, Flux_ParticleSortPair, Flux_ParticleEventRecord, Flux_ParticleTrailSample, Flux_ParticleValue, Flux_ParticleValidationRecord, Flux_ParticleCollider) · Slang interfaces for code-authored modules (Flux_ParticleUpdateType, Flux_ParticleShadingType) |
| `Zenith2/Docs/Design/VFX/EffectFormat.md` | Scope of this file · Effect document (Top-level keys, Emitter keys, Value forms, Spawn modules, Initialisation modules, Update modules, Render modules, Light and decal modules, LOD keys, Event edges, Document limits, Document validation, Example effect document) · Module catalogue (Module identifiers, Module support by target, Module parameter blocks) · Curve baking · Cooked effect format (Effect file header, Section kinds, Effect header section, Emitter record section, Module record section, Constants section, String table section, Event edge section, Reader validation, Format limits) · Effect constants image · Effect document migration |
| `Zenith2/Docs/Design/VFX/Simulation.md` | Scope of this file · Simulation model (Presentation-only particles, Presentation clock, Where each part runs) · Attached effect lifecycle (Lifecycle state machine, Lifecycle system) · Detached effects and the trigger queue · Emitter instance state machine · Spawn scheduling (Rate spawning, Burst spawning, Distance spawning, Event spawning, Spawn ordinals and capacity, Prewarm, Catch-up steps) · Particle random numbers · Particle initialisation (Initialisation algorithms) · Particle update order · Collision (Analytic colliders, Depth-buffer collision, Precipitation occlusion) · Particle trails · LOD and significance (Viewers, Screen size and LOD selection, Culling behaviour, Bounds, GPU bounds validation) · Budgets and admission (CPU admission per world, GPU admission per frame, Sorted-particle admission) · CPU simulation (CPU emitter storage, CPU update algorithm, CPU particle output) · GPU simulation (Particle pool layout, Emitter pool ranges, Frame building, ParticleEmit dispatch, ParticleSimulate dispatch, ParticleCompactAndSortKeys dispatch, ParticleSort algorithm, Reduce-then-scan fallback, Group mapping) · Determinism rules · Render-world command stream (Commands per frame, Full-state frames, Deferred and failed frames, Device loss) · Memory pressure response |
| `Zenith2/Docs/Design/VFX/Rendering.md` | Scope of this file · Draw pipeline overview · Per-view emitter culling · Draw ordering (Partitions, Emitter ranks and the sort view, Order within an emitter) · Draw records and indirect commands · Sprite rendering (Facing modes, Sub-UV flipbooks) · Ribbon rendering (Emitter ribbons, Particle trail ribbons) · Mesh particle rendering · Particle shading (Shade input and output, Lighting modes, Soft particles, Fog and pre-exposure, Blend modes) · Effect lights · Effect decals · Pass tables (ParticleEmit table, ParticleSimulate table, ParticleCompactAndSortKeys table, ParticleSort table, Particles table, Differences from the core pass tables) · PSO keys and permutations · Capability and quality tiers · Render-side failure behaviour |
| `Zenith2/Docs/Design/VFX/Authoring.md` | Scope of this file · Code-authored effects · Effect preview (Preview session, Timeline scrubbing, Preview statistics) · Effect inspector · Cook pipeline (Importer and intermediate, Effect cook step, Capacity sizing, Bounds baking, Particle type set step, Cook report members) · Hot reload · Engine effect content · Weather effects for ENV |
| `Zenith2/Docs/Design/VFX/Tests.md` | Scope of this file · Test targets · Unit tests · CPU reference parity tests · GPU simulation tests · Render tests · Determinism tests · Stress tests · Failure and fault-injection tests · Cook tests · Fixtures · Gate steps |

### Conventions

- Zenith Hungarian throughout; renderer types `Flux_`, module types `Zenith_VFX`; the asset object keeps `Zenith_ParticleEffectAsset`, the name `Zenith2/Docs/Design/ASSET.md → "Asset kinds"` gives the kind.
- Every fault-injection point carries the high 16 bits `0x5646` ('VF'): `0x56460001 …` in the module's start, `0x56461001 …` in `EnableParticles`.
- Effect time is seconds of presentation time; nothing in VFX reads wall-clock time.
- Text in this area that would otherwise read as an obligation is traced to a `Z2-R-VFX-NNN` in `Zenith2/Docs/Requirements/VFX.md`.

## Dependencies

### Library dependencies

`zenith_add_library(ZenithVFX KIND MODULE MODULE_NAME VFX DEPENDS ZenithRuntime ZenithFlux ZenithShadersGenerated ZenithAssets ZenithScene ZenithECS ZenithReflect ZenithJobs ZenithProfiling ZenithVertexFormat ZenithCore SUPPORTED ZENITH_GPU)`.

| Edge | Use | Status |
|---|---|---|
| ZenithRuntime, ZenithFlux, ZenithShadersGenerated, ZenithAssets, ZenithScene, ZenithECS, ZenithReflect, ZenithCore | the module interfaces, the particle system and render-world types, generated shared structs, the effect asset, components and systems, reflection, containers | the row of `Zenith2/Docs/Architecture.md → "Library dependency graph"` |
| ZenithJobs | `ParallelFor` for CPU emitters and the lifecycle system | in Architecture's row (cross-doc item X242 (a)) |
| ZenithProfiling | the `VFX.*` zones and counters; the panel provider interface | in Architecture's row (X242 (a)) |
| ZenithVertexFormat | the stream-set codes of mesh particles' meshes | in Architecture's row (X242 (a)) |

ZenithVFX declares no edge to ZenithPhysics, ZenithEnvironment, ZenithAudio, ZenithAnimation or ZenithScript: modules never depend on each other ([Z2-N-ARCH-850]); script bindings go through the runtime's registration context. ZenithFlux includes no ZenithVFX header. ZenithCook and ZenithEditor already link every module library (the ZenithCook row of `Zenith2/Docs/Architecture.md → "Library dependency graph"`), which is what lets the cook's bounds bake call `Zenith_VFXRunReference`.

### Cited contracts

| Heading | What VFX relies on |
|---|---|
| `Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"` | `Zenith_ModuleStartContext`, `Zenith_WorldAttachContext` (`AddWorldResource`, `AddLifecycleListener`, `AddEpilogueStage`, `AddExtractor`), `Zenith_EpilogueStageDesc`, `Zenith_ModuleError` |
| `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"` | RegisterTypes at step 6, Start at step 15, the configuration schema |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Simulation clock"` | tick index, tick duration and interpolation alpha of the presentation clock |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "ParallelFor"` | the worker-independent fan-out of CPU emitters |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"` | extraction in phase 12 and the one-frame lag (Z2-D-050) |
| `Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"`, "Per-field authority", "Commands" | `PHASE_LATE_SCRIPTS` authority of the effect components, sync-point playback of `VFX.Spawn` |
| `Zenith2/Docs/Architecture.md → "Memory governor"` | the Rendering subsystem's pressure levels |
| `Zenith2/Docs/Rendering.md → "Purpose and scope"` | `Flux_RenderWorldSection::m_axEmitterCommands` and `AddEmitterCommand` |
| `Zenith2/Docs/Rendering.md → "Shading paths and capability matrix"` | the row "Particles and ribbons (soft)" |
| `Zenith2/Docs/Rendering/FrameAnatomy.md → "Common prologue"`, `Zenith2/Docs/Rendering/FrameAnatomy.md → "Forward+ frame"` | prologue row 4 (simulation, queue A) and the `Particles` row after `TransparentForward` |
| `Zenith2/Docs/Rendering/PerPass/Common.md → "Particles"` | the core pass rows the VFX tables refine |
| `Zenith2/Docs/Rendering/GpuResources.md → "Pools"`, "Build-node upload order" | the particle pool row; `DeclareFrameData` before `GetFrameData` |
| `Zenith2/Docs/Rendering/ShaderSystem.md → "Reflection JSON"` | the reflection the particle type set reads |
| `Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"`, "Frame deferral" | pool ranges; deferred frames carry emitter commands forward |
| `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"` | `EnableParticles`, `DisableParticles`, `GetParticleSystem` and the owned particle-system member join it, declared by RPROD's renderer API ticket Z2T-RPROD-014 under the ENABLE-FEATURES ruling extended to particles, with VFX writing the three bodies in RPROD's `Internal/Renderer/Flux_Renderer.cpp` (VX2) |
| `Zenith2/Docs/Rendering.md → "Render environment"` | `Flux_RenderWorldSection::m_xEnvironment.m_xBlocks` (ENV's `Flux_EnvironmentBlocks`: `m_xWind`, `m_axLocalWindSources`), which the render side reads for the wind term of particle bounds |
| `Zenith2/Docs/Design/ENV/Foliage.md → "Gusts and turbulence"` | the wind formula whose upper bound `Flux_ParticleWindBound` computes |
| `Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor registration"` | the extractor's ordering and the clock dependency |
| `Zenith2/Docs/Design/RPROD/Views.md → "Flux_View header"` | `Flux_ViewRequest::m_uFeatureMaskSet` for preview views |
| `Zenith2/Docs/Design/RPROD/PSO.md → "Fixed state tables"` | the premultiplied and additive blend states |
| `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookStep"`, `Zenith2/Docs/Design/ASSET/Cook.md → "Zenith_CookContext"` | the two cook steps |
| `Zenith2/Docs/Design/ASSET/Loading.md → "Development reload"` | effect hot reload |
| `Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"`, "Lighting channels" | effect lights and lit particles |
| `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Fog application"` | `Flux_ApplyFroxelFog` |
| `Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"`, "Decal order and application" | effect decals |
| `Zenith2/Docs/Design/GEOM/Meshes.md → "Geometry pools"` | mesh particles' streams and indices |
| `Zenith2/Docs/Design/SCRIPT/Bindings.md → "Side-effect consumer registration"` | `SIDE_EFFECT_KIND_VFX_TRIGGER`, consumed by `Zenith_Module_VFX_Start` |
| `Zenith2/Docs/Design/ENV/WaterWeather.md → "Effect prefabs"` | ENV's weather prefabs carry VFX effects |
| `Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"` | the `VFX` row: `ZenithVFX`, plain start, `ZENITH_GPU`, first stage S5 |

`Zenith2/Shaders/Common/GPUScene.slang` is RPROD's file of shared GPU-scene records, to which areas add their records; VFX adds none, because particles are not GPU-scene instances and effect decals are GEOM's decal records. VFX's shared structs live in `Zenith2/Shaders/Common/Particles.slang`, the file the shared-struct table of `Zenith2/Docs/Rendering/ShaderSystem.md` already assigns to `Flux_EmitterCommand`.

### Simulation publication and asynchronous sources

VFX publishes nothing into simulation: particles are presentation only ("Presentation-only particles" of `Zenith2/Docs/Design/VFX/Simulation.md`), the lifecycle is an ordinary phase-10 system, and the trigger queue only receives. So VFX declares no asynchronous source kind of `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"` and needs no module publication sink; scripts reach VFX only through the buffered side effect `SIDE_EFFECT_KIND_VFX_TRIGGER`.

### Interfaces VFX declares for lower layers

None is needed: VFX is a layer-7 module and every call it makes points downward. The render side is reached through the render world (data, not callbacks), the asset system through VFX's kind loader (an interface ZenithAssets declares), and scripts through the side-effect consumer (an interface SCRIPT declares).

### Assumed types from pending designs

| Assumption | Owner | Row | Where |
|---|---|---|---|
| the `BuildGraph` calls and the `ApplyConfigChanges` forwarding, whose call sites RPROD's design specifies and which VFX slice 11 adds in code, since no RPROD ticket adds them (either order links against slice 2a's no-op definitions); RPROD's producer of `Internal/Passes/Flux_PrologueFeatures.h` (A5 row GB11, shared with GEOM), which slice 2a needs; the declarations of `EnableParticles`, `DisableParticles`, `GetParticleSystem` and the owned member are RPROD's, applied (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`, Z2T-RPROD-014) | RPROD | VX2 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"` |
| `Zenith_ModuleStartContext::GetProfilerOverlay` | MOD | DEVT row DV12, VX14 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"` |
| the precipitation occlusion capture as a frame-shared texture and mapping | ENV | VX6 | `Zenith2/Docs/Design/VFX/Simulation.md → "Precipitation occlusion"` |
| `Flux_MeshResource::GetDrawRange`, `GetRasterPoolStreams`, the published `geom.meshBounds` | GEOM | VX8 | `Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands"`, `Zenith2/Docs/Design/VFX/Authoring.md → "Effect cook step"` |
| `rprod.shaders` after `vfx.particleTypes`; the particle PSO precache entries | RPROD | VX15 | `Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"` |
| the preview host and inspector customisation | EDIT | VX11 | `Zenith2/Docs/Design/VFX/Authoring.md → "Preview session"` |
| the accessor through which a module's binding reads a handle's published load result, with its effect class and contexts | SCRIPT | VX17 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Luau surface"` |

Settled by core-doc patch e, so no longer rows of this table: the VFX pass tables, `m_axParticleColliders`, `AddParticleCollider`, the two `Flux_FrameShared` members, the added shared structs and the per-step repetition (X240: `Zenith2/Docs/Rendering/PerPass/Common.md → "ParticleEmit"` and its four sibling tables, `Zenith2/Docs/Rendering.md → "The render world hand-off"`, `Zenith2/Docs/Rendering.md → "Shared producers"`, `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`); the particle directories, game particle types, cook-time particle permutations and the particle reflection file (X241: [Z2-N-REND-105], [Z2-N-REND-106], [Z2-N-REND-123] to [Z2-N-REND-125], `Zenith2/Docs/Rendering/ShaderSystem.md → "Reflection JSON"`); the three library edges and the Rendering shedding row (X242: `Zenith2/Docs/Architecture.md → "Declared direct dependencies"`, `Zenith2/Docs/Architecture.md → "Shedding per subsystem"`); `GetSimulationClock`, `GetWorldPurpose` and `Zenith_WorldPurpose` (X243: `Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"`, [Z2-N-ARCH-879]).

## Library and file layout

### Target summary

| Target | Kind | `CMakeLists.txt` | `ZENITH_SUPPORTED` |
|---|---|---|---|
| `ZenithVFX` | `KIND MODULE`, `MODULE_NAME VFX` | the call of "Library dependencies" | `ZENITH_GPU` |
| VFX's files in `ZenithFlux` | part of ZenithFlux | ZenithFlux's own call | ZenithFlux's (`ZENITH_GPU`) |
| VFX's cook steps in `ZenithCook` | part of the tool | ZenithCook's own call | `ZENITH_TOOLS_HOST` |
| `ZenithVFX_Tests` | test executable | created by ZenithVFX's own `zenith_add_library` call from its `Tests/**` sources | `ZenithVFX`'s |
| `Zenith_Integration_VFX` | integration suite | `zenith_add_integration_tests(VFX DEPENDS ZenithRuntime ZenithVFX)` | narrowed to the GPU presets by its dependencies ([Z2-N-ARCH-172]) |
| `Zenith_Bench_VFXBudget` | benchmark | `zenith_add_benchmark(VFXBudget DEPENDS ZenithRuntime ZenithVFX SUPPORTED ZENITH_GPU)` in `Zenith2/Benchmarks/Budgets/VFX/` | `ZENITH_GPU` |

`ZENITH_GPU` is `win`, `win-asan`, `android-arm64` and `android-x86_64` (`Zenith2/Docs/Architecture/BuildSystem.md → "The ZENITH_SUPPORTED matrix"`); ZenithVFX has no `Internal/Windows|Android|Linux` files, because nothing it does is platform-specific.

### ZenithVFX files

```
Zenith2/Engine/ZenithVFX/
	CMakeLists.txt
	Zenith_Module_VFX.h                 PublicAPI.md "Zenith_Module_VFX header"
	Zenith_VFXErrors.h                  PublicAPI.md "Zenith_VFXErrors header"
	Zenith_VFXComponents.h              PublicAPI.md "Zenith_VFXComponents header"
	Zenith_VFXEvents.h                  PublicAPI.md "Zenith_VFXEvents header"
	Zenith_VFXCommands.h                PublicAPI.md "Zenith_VFXCommands header"
	Zenith_VFXReference.h               PublicAPI.md "Zenith_VFXReference header"
	Internal/
		Zenith_Module_VFX.cpp               RegisterTypes, Start, the start steps and their fault points
		Zenith_VFXModuleInstance.h/.cpp     the module instance: AttachWorld, DetachWorld, the memory consumer, shutdown
		Zenith_VFXModuleTestAccess.h        the shutdown-trace test seam (Zenith_VFXShutdownTrace, PublicAPI.md "Module shutdown order")
		Zenith_VFXConfig.h/.cpp             Zenith_RegisterVFXConfigKeys and the settings value of "Configuration keys"
		Zenith_VFXErrors.cpp                Zenith_ToString
		Zenith_VFXComponents.cpp            Zenith_RegisterVFXComponentTypes
		Zenith_VFXEvents.cpp                the two event types' registration
		Zenith_VFXCommands.cpp              payload validation, Zenith_RecordVFXSpawn, Zenith_VFXSpawnPlayback
		Zenith_VFXScriptBindings.h/.cpp     the VFX binding table and the trigger consumer
		Zenith_VFXTriggerQueue.h/.cpp       the world resource Zenith.VFX.TriggerQueue
		Zenith_VFXPanelProvider.h/.cpp      the profiler panel provider of "On-screen profiler rows"
		Lifecycle/
			Zenith_VFXLifecycle.h/.cpp      VFX.Lifecycle and the lifecycle listener
		Presentation/
			Zenith_VFXWorldState.h/.cpp     per-world presentation state: instances, emitters, viewers, colliders
			Zenith_VFXUpdate.cpp            the VFX.Update stage body
			Zenith_VFXSchedule.h/.cpp       rate, burst, distance, event, prewarm and catch-up scheduling
			Zenith_VFXLOD.h/.cpp            viewers, screen size, LOD levels, significance
			Zenith_VFXAdmission.h/.cpp      CPU admission per world
			Zenith_VFXBounds.h/.cpp         runtime bounds
			Zenith_VFXLightsDecals.h/.cpp   effect lights and decals
		Simulation/
			Zenith_VFXCpuSimulator.h/.cpp   the CPU simulator (CPU emitters, the reference simulation)
			Zenith_VFXModules.h/.cpp        the C++ implementation of every module
			Zenith_VFXNoise.h/.cpp          value noise
			Zenith_VFXCurves.h              table evaluation, shared formula with the Slang twin
			Zenith_VFXValues.cpp            Flux_EvaluateValue's C++ twin (EffectFormat.md "Effect constants image")
		Zenith_VFXReference.cpp             Zenith_VFXRunReference
		Extraction/
			Zenith_VFXExtract.h/.cpp        VFX.Extract
	Content/
		Effects/*.vfx.json                  Authoring.md "Engine effect content"
		Textures/*.png
	Tests/
		Zenith_VFXModule_Tests.cpp, Zenith_VFXLifecycle_Tests.cpp, Zenith_VFXTriggerQueue_Tests.cpp,
		Zenith_VFXSchedule_Tests.cpp, Zenith_VFXLOD_Tests.cpp, Zenith_VFXAdmission_Tests.cpp, Zenith_VFXBounds_Tests.cpp,
		Zenith_VFXExtract_Tests.cpp, Zenith_VFXScript_Tests.cpp, Zenith_VFXSave_Tests.cpp, Zenith_VFXConfig_Tests.cpp,
		Zenith_VFXCpuSim_Tests.cpp, Zenith_VFXReference_Tests.cpp, Zenith_VFXReload_Tests.cpp, Zenith_VFXStress_Tests.cpp
```

### ZenithFlux particle files

```
Zenith2/Engine/ZenithFlux/
	Flux_ParticleEffect.h               PublicAPI.md "Flux_ParticleEffect header"
	Flux_Particles.h                    PublicAPI.md "Flux_Particles header"
	Internal/Particles/
		Flux_ParticleInternal.h             PublicAPI.md "Internal Flux particle declarations"
		Flux_ParticleEffectReader.h/.cpp    EffectFormat.md "Reader validation"
		Flux_ParticleEffectLoader.cpp       the kind loader and Flux_ParseParticleEffect
		Flux_ParticleSystem.cpp             EnableParticles, the instance table, stats, debug flags
		Flux_ParticleApply.cpp              Flux_ParticleApplyRenderWorld, the pending list
		Flux_ParticleFrameBuild.cpp         Flux_ParticleBuildFrame: admission, batches, sort setup, per-view draws
		Flux_ParticleFrameData.cpp          Flux_ParticleWriteFrameData, Flux_ParticleEndFrame
		Flux_ParticleRecovery.cpp           Flux_ParticleOnDeviceRecovered, the pressure response
	Internal/Passes/Particles/
		Flux_ParticleSimulationPasses.cpp   Flux_AddParticleSimulation
		Flux_ParticleDrawPass.cpp           Flux_AddParticleDraw
	Tests/Particles/
		Flux_ParticleEffect_Tests.cpp, Flux_ParticleSystem_Tests.cpp, Flux_ParticleSim_Tests.cpp,
		Flux_ParticleSort_Tests.cpp, Flux_ParticleDraw_Tests.cpp, Flux_ParticleParity_Tests.cpp
		Types/TestUpdate_Spiral.slang, Types/TestShading_Flat.slang
	Tests/Support/Flux_TestEffectBuilder.h   the header-only test effect builder ([Z2-N-ARCH-255])
	Tests/Parity/Scenes/ParticlesBasic.scene.json, ParticlesLit.scene.json, ParticlesDebris.scene.json
	Tests/Fuzz/ParticleEffect.cpp       the fuzz target Zenith_Fuzz_ParticleEffect
```

### Shader files

```
Zenith2/Shaders/Common/Particles.slang              the [FluxShared] structs ("Shared GPU structs")
Zenith2/Shaders/Particles/
	ParticleInterfaces.slang        Flux_ParticleUpdateType, Flux_ParticleShadingType, Flux_SampleParticle
	ParticleRandom.slang            the hash, bit-identical to Flux_ParticleRandom
	ParticleCurves.slang            table evaluation
	ParticleNoise.slang             value noise
	ParticleModules.slang           every built-in module
	ParticleCollision.slang         analytic and depth collision
	ParticleFrame.slang             Flux_ParticleFindCommand, the group mapping
	Types/Standard.slang            ParticleShading_Standard
Zenith2/Shaders/Passes/Particles/
	Emit.slang  Simulate.slang  SortKeys.slang  RadixSort.slang
	ParticleDraw.slang  RibbonDraw.slang  MeshParticleDraw.slang
```

### ZenithCook VFX files

```
Zenith2/Tools/ZenithCook/Internal/VFX/
	Zenith_VFXCookErrors.h                  Zenith_VFXCookErrorCode and the warnings
	Zenith_VFXDocumentTypes.h/.cpp          Zenith_RegisterVFXDocumentTypes
	Zenith_VFXEffectStep.h/.cpp             the vfx.effect step
	Zenith_VFXDocumentValidator.h/.cpp      "Document validation"
	Zenith_VFXCurveBake.h/.cpp              "Curve baking"
	Zenith_VFXCapacity.h/.cpp               "Capacity sizing"
	Zenith_VFXBoundsBake.h/.cpp             "Bounds baking"
	Zenith_VFXEffectWriter.h/.cpp           the .zvfx writer
	Zenith_VFXParticleTypesStep.h/.cpp      the vfx.particleTypes step
Zenith2/Tools/ZenithCook/Tests/VFX/         the cook tests
Zenith2/Tools/ZenithCook/Tests/Fixtures/VFX/            effect document fixtures
Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/VFX/  v<N>.json, v<N>.expected.json
Zenith2/Tools/ZenithCook/Tests/Fuzz/VFXDocument.cpp    the fuzz target Zenith_Fuzz_VFXDocument
```

### Files VFX adds to other libraries

```
Zenith2/Engine/ZenithProfiling/Internal/Panels/Zenith_PanelsVFX.cpp   axPANEL_ROWS_VFX, the vfx tab's rows (DEVT owns the file's mechanism)
Zenith2/Docs/Research/Phases/AuthorityCensus.md                        VFX's census rows (PublicAPI.md "Authority census rows")
```

### Test and benchmark files

```
Zenith2/Tests/VFX/                  CMakeLists.txt, Zenith_VFXIntegration_Tests.cpp, Scenes/*.scene.json, Scripts/*.luau
Zenith2/Benchmarks/Budgets/VFX/     CMakeLists.txt, Zenith_Bench_VFXBudget.cpp, the W-Cinematic and W-OpenWorld VFX loads
```

## Public API

### Header summary

| Header | Library | Declares | Specified in |
|---|---|---|---|
| `Zenith_Module_VFX.h` | ZenithVFX | the two module entry points | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"` |
| `Zenith_VFXErrors.h` | ZenithVFX | `Zenith_VFXError` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXErrors header"` |
| `Zenith_VFXComponents.h` | ZenithVFX | the five components, their property IDs and registration | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXComponents header"` |
| `Zenith_VFXEvents.h` | ZenithVFX | `Zenith.VFX.EffectStarted`, `Zenith.VFX.EffectStopped` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXEvents header"` |
| `Zenith_VFXCommands.h` | ZenithVFX | `Zenith_VFXSpawnPayload`, `Zenith_RecordVFXSpawn` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header"` |
| `Zenith_VFXReference.h` | ZenithVFX | `Zenith_VFXRunReference` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXReference header"` |
| `Flux_ParticleEffect.h` | ZenithFlux | effect enumerations, limits, `Zenith_ParticleEffectAsset`, the loader, `Flux_ParseParticleEffect` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleEffect header"` |
| `Flux_Particles.h` | ZenithFlux | instance keys, `Flux_RenderEmitterCommand`, `Flux_RenderParticleCollider`, `Flux_ParticleSystem`, the random functions | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_Particles header"` |
| `Common/Particles.slang` | ZenithShadersGenerated | the shared GPU structs | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Shared GPU structs"` |
| `Particles/ParticleInterfaces.slang` | shader module | the two Slang interfaces | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Slang interfaces for code-authored modules"` |

The Luau surface is the `VFX` namespace (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Luau surface"`) plus SCRIPT's `ctx:SpawnVfx`, which records the same trigger.

### Handle kinds and ownership

| Object | Kind | Owner |
|---|---|---|
| the module instance | `Zenith_Owned<Zenith_ModuleInstance>` | the runtime |
| the effect kind loader | `Zenith_Owned<Zenith_AssetKindLoader>` | the module instance; unregistered before destruction |
| an effect | `Zenith_AssetHandle<Zenith_ParticleEffectAsset>` (generational) | shared; a component stores the persistent ID, the presentation state holds handles |
| the particle system | `Flux_ParticleSystem*` borrowed from `Flux_Renderer` | the renderer |
| an effect instance | `Flux_ParticleInstanceKey` (a value) | the world's presentation state, mirrored by the render side's instance table |
| an attach target | `Zenith_EntityHandle` (generational) | the payload; a stale handle releases the instance |
| a pool range | RHI's pool range handle | the particle system |

## Data structures and formats

### Formats summary

| Structure | Bytes | Specified in |
|---|---|---|
| `Zenith_VFXSpawnPayload` | 96 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header"` |
| a trigger queue record | 112 | `Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"` |
| `Flux_RenderEmitterCommand` | 256 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderEmitterCommand"` |
| `Flux_RenderParticleCollider`, `Flux_ParticleCollider` | 64 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderParticleCollider"` |
| `Flux_ParticleState` | 64 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleState"` |
| `Flux_EmitterCommand` | 256 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand"` |
| `Flux_ParticleDrawRecord`, `Flux_ParticleSortPair`, `Flux_ParticleEventRecord`, `Flux_ParticleTrailSample` | 32, 8, 48, 16 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Shared GPU structs"` |
| `Flux_ParticleValue`, `Flux_ParticleValidationRecord` | 48, 32 | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleValue"`, `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleValidationRecord"` |
| the effect document `*.vfx.json` (schema version 1) | ≤ 1 MiB | `Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect document"` |
| the cooked effect `.zvfx` (magic `ZVFX`, format version 1, ten sections) | — | `Zenith2/Docs/Design/VFX/EffectFormat.md → "Cooked effect format"` |
| the effect constants image | ≤ 98,560 | `Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect constants image"` |
| `particle_types.json` | — | `Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"` |

Every serialized structure is little-endian and written field by field (Z2-D-038); none depends on C++ layout. Runtime structs shared with Slang are generated from `Common/Particles.slang` (Z2-D-015) with `static_assert`ed sizes.

### Configuration keys

<!-- elaboration -->
Registered by `Zenith_RegisterVFXConfigKeys` through the registry's configuration schema in `Zenith_Module_VFX_RegisterTypes`; read at Start and, for the keys without "yes" in the Restart column, again at every quality-tier change and user commit ("Runtime configuration changes"). Every key is presentation-only, so none is simulation-affecting; a value outside its domain takes the default with one `vfx` warning.

| Key | Type | Domain | Desktop default | S24 default | Restart | Consumer |
|---|---|---|---|---|---|---|
| `vfx.cpuParticlesPerWorld` | U32 | 0 … 262,144 | 16,384 | 4,096 | no | CPU admission |
| `vfx.cpuEmittersPerWorld` | U32 | 0 … 4,096 | 1,024 | 256 | no | CPU admission |
| `vfx.gpuActiveCapacity` | U32 | 0 … 4,194,304 | 1,048,576 | 262,144 | no | GPU admission |
| `vfx.gpuEmittersPerFrame` | U32 | 0 … 16,384 | 4,096 | 1,024 | no | GPU admission |
| `vfx.sortedParticlesPerFrame` | U32 | 0 … 1,048,576 | 131,072 | 32,768 | no | sorted admission |
| `vfx.sortedEmittersPerFrame` | U32 | 0 … 1,024 | 1,024 | 256 | no | sorted admission |
| `vfx.lodBias` | F32 | 0 … 3 | 0 | 0.5 | no | LOD selection (levels added) |
| `vfx.spawnScale` | F32 | 0 … 1 | 1 | 0.75 | no | spawn scheduling |
| `vfx.maxLightingMode` | ENUM {unlit, vertex, pixel} | — | pixel | vertex | no | lighting-mode clamp |
| `vfx.maxEffectLights` | U32 | 0 … 256 | 64 | 16 | no | effect lights per world |
| `vfx.maxEffectDecals` | U32 | 0 … 1,024 | 256 | 64 | no | effect decals per world |
| `vfx.triggerQueueCapacity` | U32 | 64 … 16,384 | 1,024 | 512 | yes | trigger queue size |
| `vfx.triggerQueueScriptReserve` | U32 | 0 … 16,320 | 512 | 256 | yes | the script share of the trigger queue (at least `Z2-B-SCRIPT-024` × `sim.maxCatchUpTicks`) |
| `vfx.sort.algorithm` | ENUM {onesweep, reduceThenScan} | — | onesweep | onesweep | no | the radix sort's form (`Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"`) |
| `vfx.detachedEffectsPerWorld` | U32 | 0 … 4,096 | 512 | 128 | no | detached cap |
| `vfx.pool.initialParticles` | U32 | 0 … `vfx.pool.maxParticles` | 262,144 | 65,536 | yes | pool state region initial size |
| `vfx.pool.maxParticles` | U32 | 64 … 4,194,304 | 1,048,576 | 262,144 | yes | pool state region maximum (from `Z2-B-VFX-001`) |
| `vfx.pool.maxTrailSamples` | U32 | 0 … 4,194,304 | 1,048,576 | 262,144 | yes | trail region maximum |
| `vfx.pool.maxEventRecords` | U32 | 0 … 1,048,576 | 262,144 | 65,536 | yes | event region maximum |
| `vfx.maxRenderInstances` | U32 | 64 … 65,536 | 4,096 | 1,024 | yes | render-side instance table |
| `vfx.debug.validateBounds` | BOOL | — | false | false | yes | GPU bounds validation (development builds; refused true in Shipping) |
| `vfx.preview.catchUpStepsPerFrame` | U32 | 1 … 64 | 8 | 8 | no | preview catch-up |

The budgets' defaults are the proposals of "Proposed values" until the Stage 1 ruling; the generated defaults then follow the adopted rows. Restart-required keys size startup allocations and follow `Zenith2/Docs/Design/PLAT/Configuration.md → "Restart-required keys"`.

### Runtime configuration changes

PLAT commits user writes and quality-tier changes in prologue step 3 and dispatches its change registry on the main thread (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`). The non-restart keys reach their consumers by two routes:

| Keys | Route | Takes effect |
|---|---|---|
| simulation side: `vfx.cpuParticlesPerWorld`, `vfx.cpuEmittersPerWorld`, `vfx.lodBias`, `vfx.spawnScale`, `vfx.maxEffectLights`, `vfx.maxEffectDecals`, `vfx.detachedEffectsPerWorld`, `vfx.preview.catchUpStepsPerFrame` | the module's change callback (start step 8) re-reads them into the module's settings value; only `VFX.Update` and `VFX.Extract` read it, and they never run during prologue step 3 | that frame's `VFX.Update` |
| render side: `vfx.gpuActiveCapacity`, `vfx.gpuEmittersPerFrame`, `vfx.sortedParticlesPerFrame`, `vfx.sortedEmittersPerFrame`, `vfx.maxLightingMode`, `vfx.sort.algorithm` | `Flux_Renderer::ApplyConfigChanges` calls `Flux_ParticleApplyConfigChanges` with `Flux_ReadParticleSystemDesc` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"`, row VX2) | the next built frame's admission, draw runs (the lighting clamp selects the lower `uPERM_LIGHTING`) and sort |

`Test_VFXConfig_TierDefaults` checks the resolved values and `Test_ParticleSystem_TierChangeAppliesBudgets` that they reach the particle system.

## Algorithms and state machines

### Frame-graph placement

| Node | Kind and registration | Phase or stage | Declared access | Per |
|---|---|---|---|---|
| `VFX.Spawn` playback | the command kind's playback at sync points (`Zenith2/Docs/Architecture/FramePhases.md → "Commands"`) | sync points after phases 1–10 | appends to `Zenith.VFX.TriggerQueue` | tick |
| the VFX trigger consumer | SCRIPT's side-effect consumer at script merges | script merges | appends to `Zenith.VFX.TriggerQueue` | tick |
| `VFX.Lifecycle` | `Zenith_SystemDesc{ "VFX.Lifecycle", PHASE_LATE_SCRIPTS, … }` | phase 10 | `Read(Zenith.VFX.Effect)`, `Write(Zenith.VFX.EffectState)`, commands and events | tick |
| `VFX.Update@W<tag>` | `AddEpilogueStage({ "VFX.Update", &Zenith_VFXUpdateRun, pxWorldState, 4 })` | epilogue (stage 12) | reads of the effect, state, collider, viewer, preview-control and transform components and the camera; `WriteResource(Zenith.VFX.TriggerQueue)` as its sole consumer | frame |
| `VFX.Extract@W<tag>` | `AddExtractor({ "VFX.Extract", &Zenith_VFXExtractRun, pxWorldState, 2 })`, `After("VFX.Update")` | phase 12, before `Render.Seal@W0` (set by `AddExtractor`) | reads the world state `VFX.Update` produced; writes the render-world partition: emitter commands, light deltas, decals, colliders | frame |
| render side | `Flux_ParticleApplyRenderWorld` (step 4), `Flux_ParticleBuildFrame` (7c), `Flux_AddParticleSimulation` (8), `Flux_AddParticleDraw` per view (9), `Flux_ParticleWriteFrameData` (11), `Flux_ParticleEndFrame` (13) | `Flux.BuildGraph` (frame k − 1, Z2-D-050) | the sealed render world; the particle system | frame |
| GPU | the four simulation passes on queue A (prologue row 4); `Particles` on queue G after `TransparentForward` | — | the pass tables | frame |

`VFX.Update` and `VFX.Extract` of different worlds run concurrently; within a world, `VFX.Extract` follows `VFX.Update`, and both run concurrently with other modules' epilogue reads. The CPU emitters' `ParallelFor` inside `VFX.Update` uses the job system's continuation rules (no blocking wait).

### Algorithm index

| Algorithm or state machine | Complexity | Heading |
|---|---|---|
| attached-effect lifecycle | O(entities with an effect) per tick | `Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"` |
| emitter presentation states | O(emitters) per frame | `Zenith2/Docs/Design/VFX/Simulation.md → "Emitter instance state machine"` |
| spawn scheduling | O(emitters + bursts) per frame | `Zenith2/Docs/Design/VFX/Simulation.md → "Spawn scheduling"` |
| LOD and significance | O(instances × viewers) | `Zenith2/Docs/Design/VFX/Simulation.md → "LOD and significance"` |
| CPU and GPU admission | O(n log n) over emitters | `Zenith2/Docs/Design/VFX/Simulation.md → "Budgets and admission"` |
| CPU simulation | O(C) per CPU emitter | `Zenith2/Docs/Design/VFX/Simulation.md → "CPU update algorithm"` |
| GPU emission and simulation | O(spawns), O(Σ C) | `Zenith2/Docs/Design/VFX/Simulation.md → "GPU simulation"` |
| sort keys and radix sort | O(N) per pass, four passes | `Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm"` |
| draw ordering and draw lists | O(emitters log emitters) per view | `Zenith2/Docs/Design/VFX/Rendering.md → "Draw ordering"` |
| capacity sizing and bounds baking | bounded by 10⁸ particle-steps per effect | `Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"`, `Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"` |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_Module_VFX_RegisterTypes`, `Zenith_Module_VFX_Start`, `Flux_Renderer::EnableParticles` | `MAIN_THREAD` (startup steps 6 and 15) |
| `Flux_Renderer::DisableParticles` | `MAIN_THREAD` at a render-idle point (shutdown step 6, or a failed module start's unwind) |
| the effect components | written in `PHASE_LATE_SCRIPTS` (authority) or by commands; read by `VFX.Lifecycle` and `VFX.Update` |
| `Zenith_RecordVFXSpawn` | `OWNER_THREAD` of the recording invocation |
| the trigger queue | written by sync-point playback and script merges one at a time per world; drained by `VFX.Update` |
| a world's presentation state | `VFX.Update@W<tag>` then `VFX.Extract@W<tag>`; never shared between worlds |
| `Zenith_ParticleEffectAsset` | `IMMUTABLE` after activation |
| `Flux_RenderEmitterCommand`, `Flux_RenderParticleCollider` | `IMMUTABLE` after `Render.Seal` |
| `Flux_ParticleSystem` | `MAIN_THREAD` for `GetStats`, `GetInstanceStats` and `SetDebugFlags` (the frame prologue, the editor, the profiler panel provider; copies from the three-slot stats ring); every mutation on `Flux.BuildGraph` alone |
| `Zenith_VFXRunReference`, `Flux_ParseParticleEffect`, the random functions, `Flux_ParticleWindBound`, `Flux_ParticleWindExpansion` | `ANY_THREAD` |
| the cook steps | the cook's item threads (`vfx.effect`, out of process) and the driver (`vfx.particleTypes`) |

### Lifetime

| Event | Behaviour |
|---|---|
| startup | RegisterTypes (types, keys) at step 6; Start's nine steps at step 15; `EnableParticles` before the first frame when a renderer exists |
| world attach | the trigger queue, the lifecycle listener, `VFX.Lifecycle`, `VFX.Update`, `VFX.Extract`, in that order |
| world detach | the world's instances destroyed (`DESTROY` commands in its last extraction), its effect handles released, its queue and state freed |
| entity destroyed | its attached instance released; particles live out on the render side |
| shutdown | at step 6 of `Zenith2/Docs/Contracts.md → "Shutdown ownership order"`, after the worlds (whose handles are gone) and after GPU idle: the module's destructor destroys its two callback tokens, calls `Flux_Renderer::DisableParticles()` (releasing every render-side effect handle at that render-idle point), then `UnregisterKindLoader` (which unloads every effect and waits for the acknowledgements), then destroys the loader (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Module shutdown order"`); the runtime undoes the context registrations ([Z2-N-ARCH-851]); the renderer, destroyed afterwards in the same step, finds no particle system |
| cancellation | VFX holds no asynchronous operation of its own; effect loads are cancelled by the asset system when the last handle drops |

### Locks and queues

VFX takes no lock: per-world state is touched only by that world's nodes, the trigger queue's writers are serialised by the frame graph, and the render side runs on `Flux.BuildGraph`. Its bounded queues are the trigger queue (`QUEUE_FULL_POLICY_DROP_NEWEST`, capacity `vfx.triggerQueueCapacity`) and the render-side pending list (merged entries: the oldest `UPDATE` dropped beyond `m_uMaxInstances × 16`; kept catch-up steps: at most `uFLUX_PARTICLE_MAX_PENDING_CATCH_UP_STEPS` per emitter, beyond which the instance's seek is interrupted, never a step dropped silently, `Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"`). A source emitter's event ring is not a queue: it is a slot array of exactly `C_src × E` records in which every record has its own index, so it is never full and drops nothing (`Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"`). VFX adds no row to the lock-ordering table.

### Error enumerations

| Enumeration | Values | Where |
|---|---|---|
| `Zenith_VFXError` | `OUT_OF_MEMORY` 1, `REGISTRATION`, `INVALID_ARGUMENT`, `UNKNOWN_EFFECT`, `UNKNOWN_PARAMETER`, `LIMIT`, `STALE_ENTITY`, `SHUTTING_DOWN` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXErrors header"` |
| `Flux_ParticleError` | `CORRUPT` 1, `VERSION`, `LIMIT`, `OUT_OF_MEMORY`, `POOL_FULL`, `DEVICE`, `INVALID_DESC`, `ALREADY_ENABLED` | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Effect enumerations"` |
| `Zenith_VFXCookErrorCode` | `SCHEMA` 1, `UNKNOWN_MODULE`, `TARGET_MODULE`, `VALUE_FORM`, `UNKNOWN_PARAMETER`, `EVENT_GRAPH`, `REFERENCE`, `CUSTOM_TYPE`, `LIMIT`, `BOUNDS_BAKE`; warnings `CURVE_RESOLUTION`, `CAPACITY_OVERFLOW`, `UNUSED_PARAMETER`, `SORT_COST`, `BURST_SPACING`, `BOUNDS_ESTIMATE` | `Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"` |
| module start errors | `Zenith_ModuleError` codes per start step | `Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"` |

Per-frame failures never fail a frame or a tick: they are counted ("Counters") and handled as "Failure table" states.

### Luau errors and asserts

| Situation | Outcome |
|---|---|
| a script passes a wrong argument count or type to any VFX binding | SCRIPT's dispatcher error `ZS1510` or `ZS1511` (`Zenith2/Docs/Scripting/MisuseCatalogue.md → "Catalogue"`) |
| a script passes a null effect, a non-finite or out-of-range value, more than four parameters, a null or stale entity to `VFX.Spawn` or `VFX.SpawnAttached` | a Luau error with VFX's MISUSE code `ZS5601` or `ZS5602`, the rows of VFX's block ZS5600–ZS5699 (`Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"`, table in `Zenith2/Docs/Design/VFX/PublicAPI.md → "Luau surface"`); the transaction follows SCRIPT's rules and the world is unchanged |
| `VFX.ParameterIndex` with an unknown name, or for an effect whose load the calling world has not published as loaded | no error: it returns `nil`; it reads the world's published load result, never the live registry, so its result is the same at the same tick on every run whatever the load's completion time (Z2-D-019) |
| a full C++ share of the trigger queue | no error: the C++ record is dropped and counted (presentation only); script triggers the merge accepted fit the script reserve |
| a C++ caller passes an invalid payload to `Zenith_RecordVFXSpawn` | `VFX_ERROR_INVALID_ARGUMENT`; no assert |
| an emitter index outside its effect in an engine-internal accessor | `ZENITH_ASSERT` (programmer error) |
| untrusted input (cooked files, documents, render-world commands) | an error or a counted drop; never an assert |

## Diagnostics and profiling

### Counters

Simulation-side counters are per world (`vfx.*`, published through the module's profiling counters); render-side counters are per frame (`Flux.Particles.*`, published with `Flux_ParticleStats` at submit).

| Counter | Counts |
|---|---|
| `vfx.triggers.dropped`, `vfx.triggers.invalid` | trigger records dropped by a full C++ share (or a full queue); records refused at playback |
| `vfx.detached.dropped`, `vfx.detached.truncated` | detached spawns beyond the cap; looping or endless effects played one cycle |
| `vfx.effects.failed` | instances whose effect failed to load |
| `vfx.lifecycle.invalid_components` | component values outside their domain |
| `vfx.cpu.budget_culled`, `vfx.cpu.alloc_failed` | CPU emitters refused by admission; CPU storage allocation failures |
| `vfx.spawns.clamped` | spawn counts clamped to capacity in one frame |
| `vfx.viewers.dropped`, `vfx.viewers.clamped` | viewers beyond eight; field-of-view values clamped |
| `vfx.colliders.dropped`, `vfx.colliders.invalid` | colliders beyond 64; colliders disabled for invalid dimensions |
| `vfx.lights.dropped`, `vfx.decals.dropped` | effect lights and decals beyond the per-world caps |
| `vfx.extract.dropped` | commands the render-world writer refused |
| `vfx.catchup.refused` | preview controls in a world whose purpose is not `WORLD_PURPOSE_PREVIEW`, once per world |
| `vfx.pressure.scale` | the active pressure factor |
| `vfx.cpu.storage_bytes` | a gauge: the CPU particle storage and trigger-queue bytes the module's memory consumer reports |
| `Flux.Particles.CommandsRejected`, `Flux.Particles.CollidersRejected` | render-world records failing validation |
| `Flux.Particles.WaitingAsset`, `Flux.Particles.EffectFailed`, `Flux.Particles.ConstantsFailed` | instances waiting for, or failing, their effect or constants buffer |
| `Flux.Particles.PoolFull`, `Flux.Particles.InstancesFull`, `Flux.Particles.InstancesTimedOut` | pool and instance-table exhaustion; instances with no command for two frames |
| `Flux.Particles.BudgetCulled`, `Flux.Particles.SortRefused`, `Flux.Particles.ViewDrawsDropped` | GPU admission refusals; sorted-budget refusals; per-view draw limit |
| `Flux.Particles.Overwrites` | live particles (event children included) overwritten by a newer spawn (validation builds) |
| `Flux.Particles.BoundsExceeded` | emitters whose GPU-measured bounds left their CPU bounds (validation builds) |
| `Flux.Particles.TextureFallback`, `Flux.Particles.MeshNotResident` | fallback textures drawn; mesh draws skipped |
| `Flux.Particles.DepthCollisionUnavailable`, `Flux.Particles.OcclusionUnavailable` | depth-collision or occlusion emitters without their input this frame |
| `Flux.Particles.PendingDropped`, `Flux.Particles.DeviceResets`, `Flux.Particles.PressureScale` | pending-list overflow; device-loss recoveries; the active pressure factor |
| `Flux.Particles.SeekInterrupted` | preview seeks interrupted because their kept catch-up steps would exceed 64 per emitter |

The values of `Flux_ParticleStats` (instances, GPU emitters simulated and refused, active work and its admission budget, CPU particles uploaded, sorted particles and emitters, draw commands, pool bytes, constants bytes) are published as `Flux.Particles.Stats.*` gauges (`COUNTER_KIND_GAUGE`), and the counters above as `COUNTER_KIND_DELTA` with the `_delta` suffix DEVT's registry keeps verbatim (`Zenith2/Docs/Design/DEVT/Profiler.md → "Counter registry"`); "On-screen profiler rows" shows them.

### On-screen profiler rows

VFX owns the `vfx` tab (title "VFX", owner area VFX) of DEVT's overlay: one `inline constexpr Zenith_ProfilerRowDesc axPANEL_ROWS_VFX[]` in `Zenith2/Engine/ZenithProfiling/Internal/Panels/Zenith_PanelsVFX.cpp`, added to the catalogue's tab order before `counters` by the ticket that lands VFX's first counters (`Zenith2/Docs/Design/DEVT/Overlay.md → "Built-in panel catalogue"`). Each row in `Zenith_ProfilerRowDesc` field order (`m_szLimit` is the BAR full-scale expression of DEVT's revised row struct, `Zenith2/Docs/Design/DEVT/Overlay.md → "Zenith_ProfilerPanels header"`, empty meaning 1.0; `m_fBudgetValue` stays 0 while the row is pending, and the Stage 1 VFX budget ticket sets it and writes the adopted desktop value as the millisecond bars' limit constant in the same diff, so a bar's full scale is its budget):

| Label | Expression | Kind | Unit | Scale | Limit | Budget row | Exactness |
|---|---|---|---|---|---|---|---|
| `VFX update` | `zone_ms("VFX.Update@W*")` | BAR | MILLISECONDS | 1 | empty (1.0 ms) until the budget ticket | `Z2-B-VFX-002` | exact |
| `VFX lifecycle` | `zone_ms("VFX.Lifecycle")` | VALUE | MILLISECONDS | 1 | — | — (the pending row "VFX lifecycle CPU") | exact |
| `Particle simulation GPU` | `zone_ms("ParticleEmit*") + zone_ms("ParticleSimulate*") + zone_ms("ParticleCompactAndSortKeys*") + zone_ms("ParticleSort*")` | BAR | MILLISECONDS | 1 | empty (1.0 ms) until the budget ticket | `Z2-B-VFX-003` | approximate (GPU frame lag) |
| `Particles draw GPU` | `zone_ms("Particles*")` | BAR | MILLISECONDS | 1 | empty (1.0 ms) until the budget ticket | `Z2-B-VFX-004` | approximate |
| `Particle memory` | `Flux.Particles.Stats.PoolBytesUsed + Flux.Particles.Stats.ConstantsBytes + vfx.cpu.storage_bytes` | BAR | BYTES | 1 | `Flux.Particles.Stats.PoolBytesCapacity + Flux.Particles.Stats.ConstantsBytes + vfx.cpu.storage_bytes` (the pool's current capacity; the constants and CPU storage are held whole) | `Z2-B-VFX-001` | exact |
| `GPU work admitted` | `Flux.Particles.Stats.GpuActiveWork` | BAR | COUNT | 1 | `Flux.Particles.Stats.GpuActiveCapacity` (the frame's admission budget after the pressure factor) | — | exact |
| `GPU emitters simulated` | `Flux.Particles.Stats.GpuEmittersSimulated` | VALUE | COUNT | 1 | — | — | exact |
| `Budget-culled emitters` | `Flux.Particles.BudgetCulled_delta + vfx.cpu.budget_culled_delta` | VALUE | COUNT | 1 | — | — | exact |
| `Sorted particles` | `Flux.Particles.Stats.SortedParticles` | VALUE | COUNT | 1 | — | — | exact |
| `CPU particles` | `Flux.Particles.Stats.CpuParticles` | VALUE | COUNT | 1 | — | — | exact |
| `Draw commands` | `Flux.Particles.Stats.DrawCommands` | VALUE | COUNT | 1 | — | — | exact |
| `Triggers dropped` | `vfx.triggers.dropped_delta` | VALUE | COUNT | 1 | — | — | exact |
| `Drops {}` | `each("Flux.Particles.*Dropped_delta")` | EACH | COUNT | 1 | — | — | exact |

GPU pass times are DEVT's GPU zones of the passes' names (RPROD's `Flux_PassTimingPublisher`, `Zenith2/Docs/Design/RPROD/Diagnostics.md → "On-screen profiler rows"`), which the GPU view also shows per pass. `vfx.cpu.storage_bytes` is the module memory consumer's reported bytes, published as a gauge.

The particle system's live table is a provider panel: `Zenith_VFXPanelProvider` (`Zenith2/Engine/ZenithVFX/Internal/Zenith_VFXPanelProvider.h`, implementing DEVT's `Zenith_ProfilerPanelProvider`, panel ID `vfxsystem`, title "VFX system"), registered at start step 9 through `Zenith_ModuleStartContext::GetProfilerOverlay().AddProvider` (`Zenith2/Docs/Design/DEVT/Overlay.md → "Panel providers"`; the member is DEVT's row DV12 to MOD). Its `Build`, called on the main thread only while its tab is visible, reads `Flux_ParticleSystem::GetStats()` (a copy) and writes, with `Header`, `Value` and `Bar`: instances; GPU work admitted against `vfx.gpuActiveCapacity` (a `Bar`); GPU emitters simulated and refused; sorted particles against `vfx.sortedParticlesPerFrame`; CPU particles; draw commands; pool bytes used against capacity; constants bytes; and, headless or without a particle system, one `Text` row saying so. Its token is destroyed first in the module's shutdown. `Test_VFXPanelProvider_BuildsRows` holds both.

### Profiling zones

| Zone | Covers | Row |
|---|---|---|
| `VFX.Lifecycle` | the lifecycle system | the pending row "VFX lifecycle CPU" |
| `VFX.Update@W<tag>` | the whole stage; child zones `VFX.Schedule`, `VFX.CpuSim` (one per leaf), `VFX.LightsDecals` | `Z2-B-VFX-002` (MP-CPUZONE) |
| `VFX.Extract@W<tag>` | the extractor | inside RPROD's extraction rows |
| `Flux.Particles.Apply`, `Flux.Particles.BuildFrame`, `Flux.Particles.WriteFrameData` | the render-side CPU steps | inside RPROD's build-node rows |
| GPU passes `ParticleEmit`, `ParticleSimulate`, `ParticleCompactAndSortKeys`, `ParticleSort` | prologue | `Z2-B-VFX-003` (MP-GPUPASS) |
| GPU pass `Particles`, per view | per view | `Z2-B-VFX-004` (MP-GPUPASS) |

### Log category

`vfx`: warnings for out-of-domain configuration values (once per key), invalid components (once per entity per run), failed effects (once per effect), and cook-side messages through the cook's diagnostics. Nothing per particle or per frame is logged.

### Debug views

<!-- elaboration -->
`Flux_ParticleSystem::SetDebugFlags` with `Flux_ParticleDebugFlags` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"`), in development builds, exposed by DEVT's debug UI as the "VFX" debug views:

| Flag | View |
|---|---|
| `FLUX_PARTICLE_DEBUG_BOUNDS` | every drawn emitter's CPU bounds as white lines; with `vfx.debug.validateBounds`, the GPU-measured bounds in yellow |
| `FLUX_PARTICLE_DEBUG_OVERDRAW` | every particle drawn additive grey 1/32, so overdraw reads as brightness |
| `FLUX_PARTICLE_DEBUG_LOD` | emitters tinted by LOD level: 0 white, 1 green, 2 yellow, 3 red |
| `FLUX_PARTICLE_DEBUG_SORT_RANK` | blended emitters tinted by rank, blue far to red near |
| `FLUX_PARTICLE_DEBUG_BUDGET` | budget-refused emitters (drawn frozen) tinted magenta |

The tints are a per-frame value in the draw passes' data (a draw-uniform branch), never a permutation; the overdraw view draws every emitter with the additive partition's PSO.

### Frame dump and telemetry

The frame dump's render-world section lists each world's emitter commands (instance key, emitter index, operation, flags, spawn counts, bounds) and colliders; the particle system adds its stats snapshot and its admission decisions per emitter. Telemetry carries the per-frame `Flux.Particles.Stats.*` gauges and the drop counters; no particle data is captured.

## Authoring and cooking

Effects are authored in code with previews (Z2-D-079): a reviewable `*.vfx.json` document naming built-in modules, with optional Slang update and shading types; there is no node graph. VFX owns:

| Surface | Heading |
|---|---|
| the document and its schema, limits, validation and migration | `Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect document"`, "Effect document migration" |
| the code-authored types | `Zenith2/Docs/Design/VFX/Authoring.md → "Code-authored effects"` |
| the preview world, scrubbing and statistics (hosted by EDIT's panel) | `Zenith2/Docs/Design/VFX/Authoring.md → "Effect preview"` |
| the inspector customisations (hosted by EDIT's inspector) | `Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector"` |
| the import (ASSET's `zenith-doc`), the `vfx.effect` and `vfx.particleTypes` steps | `Zenith2/Docs/Design/VFX/Authoring.md → "Cook pipeline"` |
| hot reload | `Zenith2/Docs/Design/VFX/Authoring.md → "Hot reload"` |
| the engine effects | `Zenith2/Docs/Design/VFX/Authoring.md → "Engine effect content"` |

The document type `Zenith.VFX.EffectDocument` is registered tool-side (`Zenith_RegisterVFXDocumentTypes`) and joins ASSET's registered document types (cross-area row VX5); the module registers no document type.

## Failure handling

### Failure table

| Failure | Behaviour | Signal |
|---|---|---|
| an effect asset missing from the catalogue | the component resolves to the kind's null object and plays nothing; a subsequent mount providing it starts it | `vfx.effects.failed`, one `vfx` warning |
| a corrupt or version-mismatched `.zvfx` | the loader fails the asset with the reader's code; instances naming it end in DONE | `vfx.effects.failed`, `Flux.Particles.EffectFailed` |
| a texture or mesh dependency failed | the texture's fallback; mesh draws skipped | `Flux.Particles.TextureFallback`, `Flux.Particles.MeshNotResident` |
| an invalid component value | nothing starts or stops for the entity that tick | `vfx.lifecycle.invalid_components` |
| an invalid spawn payload | `VFX_ERROR_INVALID_ARGUMENT` at record time; a Luau error for scripts | — |
| a full C++ share of the trigger queue, detached cap, light or decal cap, collider or viewer cap | the newest record or the least significant item dropped | the matching `*.dropped` counter |
| shutdown with effects still referenced by the render side | `DisableParticles` releases them before the kind loader is unregistered ("Module shutdown order" of `Zenith2/Docs/Design/VFX/PublicAPI.md`) | — |
| CPU storage allocation failure | the emitter is budget-culled for that frame and retried next frame | `vfx.cpu.alloc_failed` |
| the module's start fails | the start step's error; every earlier step undone; the game does not start | the runtime's start failure exit |
| `EnableParticles` fails | `MODULE_ERROR_OUT_OF_MEMORY` or `MODULE_ERROR_DEVICE`; the start unwinds | — |
| the pool region at its maximum | the emitter stays unallocated and retries | `Flux.Particles.PoolFull` |
| the render instance table full | the new instance refused | `Flux.Particles.InstancesFull` |
| a render-world record failing validation | dropped; the emitter keeps its previous state | `Flux.Particles.CommandsRejected`, `Flux.Particles.CollidersRejected` |
| the writer refuses a command (slot full) | the emitter misses the frame; a full-state frame repairs it | `vfx.extract.dropped`, `Flux.Particles.InstancesTimedOut` |
| a deferred or failed frame | commands merged into the pending list and applied next submitted frame; a seeking emitter's steps kept unmerged and executed first, in order | `Flux.Particles.PendingDropped` |
| a preview seek whose kept steps would exceed 64 per emitter | the seek is interrupted: kept steps dropped, the instance reset and drawn as nothing until the next seek; the preview session issues the seek again | `Flux.Particles.SeekInterrupted`, `Flux_ParticleInstanceStats::m_bSeekInterrupted` |
| device loss | ranges and buffers recreated; GPU particles restart empty | `Flux.Particles.DeviceResets` |
| memory pressure | budgets scale by 0.75 (WARNING) and 0.5 (CRITICAL); revert one frame after the level falls | `vfx.pressure.scale`, `Flux.Particles.PressureScale` |
| a budget breach (frame admission) | the least significant emitters refused, drawn frozen or culled by their cull behaviour | `vfx.cpu.budget_culled`, `Flux.Particles.BudgetCulled`, `Flux.Particles.SortRefused` |
| a missing PSO | RPROD's missing-PSO fallback: the run skipped in Shipping, magenta in development | RPROD's PSO counters |
| a cook error | the item fails with `COOK_ERROR_STEP_FAILED` and the VFX detail code; the previous cooked file is kept | the cook report |
| a reload fails | the old effect keeps playing; the panel shows the cook errors | the cook server's result |

## Test strategy

Every test named here is specified with its assertion in `Zenith2/Docs/Design/VFX/Tests.md`:

| Kind | Heading | Gate step |
|---|---|---|
| CPU unit tests of the module, components, scheduling, LOD, admission, extraction, bindings, save, configuration, diagnostics | `Zenith2/Docs/Design/VFX/Tests.md → "Unit tests"` | `Test` |
| CPU reference parity against the GPU simulator, with tolerances | `Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"` | `Test` (WARP and lavapipe) |
| GPU simulation tests | `Zenith2/Docs/Design/VFX/Tests.md → "GPU simulation tests"` | `Test` |
| render tests and parity scenes, all in-run comparisons, no committed goldens | `Zenith2/Docs/Design/VFX/Tests.md → "Render tests"` | `Test`, `BackendParity`, `PathParity` |
| determinism tests | `Zenith2/Docs/Design/VFX/Tests.md → "Determinism tests"` | `Test` |
| stress tests | `Zenith2/Docs/Design/VFX/Tests.md → "Stress tests"` | `Test` (`z2-stress`) |
| failure, fault-injection and fuzz tests | `Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests"` | `Test`, `Sanitizers -Preset win-asan` |
| cook tests | `Zenith2/Docs/Design/VFX/Tests.md → "Cook tests"` | `Test` |
| device budget runs | `Zenith2/Docs/Design/VFX/Tests.md → "Gate steps"` | `needs-human`, `gpu-reason:visual-measure` |

WARP and lavapipe are software devices on every gate machine, so no VFX test is `needs-gpu`; tests use real objects (no double of a concrete class), build effects through the cook or `Flux_TestEffectBuilder`, and compare images within one run.

## Budget rows

### Owned rows

| Row | Name | Unit | Probe | Platforms | Workloads | Rule | Status | Producer |
|---|---|---|---|---|---|---|---|---|
| `Z2-B-VFX-001` | Particle pools and CPU particle data | MB | MP-SUBMEM | Win, S24 | W-Cinematic, W-OpenWorld | BR-GOVERNOR | pending | Stage 1 per-system budget ticket for VFX |
| `Z2-B-VFX-002` | CPU particle update | ms | MP-CPUZONE | Win, S24 | W-Cinematic, W-OpenWorld | BR-ACCEPT | pending | Stage 1 per-system budget ticket for VFX |
| `Z2-B-VFX-003` | Particle simulation GPU | ms | MP-GPUPASS | Win, S24 | W-Cinematic, W-OpenWorld | BR-ACCEPT | pending | Stage 1 per-system budget ticket for VFX |
| `Z2-B-VFX-004` | Particles and ribbons draw GPU | ms | MP-GPUPASS | Win, S24 | W-Cinematic, W-OpenWorld | BR-ACCEPT | pending | Stage 1 per-system budget ticket for VFX |

`Z2-B-VFX-001` is dual-resident in `MEM-WIN-RAM` and `MEM-WIN-VRAM`; `Z2-B-VFX-002` is a member of `CPU-SIM`; `Z2-B-VFX-003` sits in the prologue of every GPU group and `Z2-B-VFX-004` in each path's per-view list (`Zenith2/Docs/Budgets.md`). VFX has no Stage 1 harness half: the ruling cites reference-engine and published data, and the measurement harness `Zenith_Bench_VFXBudget` lands with the Stage 5 work it measures.

### Rows VFX is measured against

| Row | Owner | Use |
|---|---|---|
| `Z2-B-SCRIPT-024` VFX trigger capacity per tick | SCRIPT | the trigger queue's minimum size ("Detached effects and the trigger queue" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| `Z2-B-RPROD-024` Shader permutation limit per game | RPROD | the particle programs' permutations join the per-game count |
| `Z2-B-RPROD-031` | RPROD | the parity tolerances of the `Particles*` scenes |
| `Z2-B-CORE-005`, `Z2-B-RHI-006` | CORE, RHI | the memory totals the particle pool and CPU storage are allocated from |

### Pending row needs

Two rows VFX needs that `Zenith2/Docs/Budgets.md` lacks, in its row columns (`Zenith2/Docs/Budgets.md → "Row fields"`); cross-doc item X263 added them as Z2-B-VFX-005 (`Zenith2/Docs/Budgets.md → "Engine service CPU rows"`) and Z2-B-VFX-006 ([Z2-N-BUDGET-470]), with the SCRIPT predecessor of the VFX producer row and the demand input of the CORE row ([Z2-N-BUDGET-472]), and their values are written by the producer named:

| Name | Status | Value (proposal) | Unit | Procedure | Platform | Tolerance | Workloads | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|
| Z2-B-VFX-005 VFX lifecycle CPU | pending | 0.05 ms desktop, 0.1 ms S24 (`VFX.Lifecycle` over 2,000 effect entities per world, O(entities) at ≈ 20 ns per entity, fanned out) | ms | MP-CPUZONE (zone `VFX.Lifecycle`), member of `CPU-SIM` | Win, S24 | default | W-Cinematic, W-OpenWorld | BR-ACCEPT | Stage 1 per-system budget ticket for VFX |
| Z2-B-VFX-006 VFX trigger queue capacity per world | pending | 1,024 desktop, 512 S24: a script reserve of `Z2-B-SCRIPT-024` × `sim.maxCatchUpTicks` (proposed 512 and 256) plus a C++ share of the same size | triggers | MP-COUNTER (`vfx.triggers.dropped` over the run, which must stay 0 for script-accepted triggers) | Win, S24 | 0 (hard) | W-Cinematic, W-OpenWorld | BR-DROP | Stage 1 per-system budget ticket for VFX |

### Proposed values

The proposals the Stage 1 ruling starts from (published reference-engine scales for one hero effect set plus ambient effects, divided across the two workloads):

| Row | Desktop (Win) | S24 | Basis |
|---|---|---|---|
| `Z2-B-VFX-001` | 96 MB | 24 MB | 1,048,576 / 262,144 particle states × 64 B, trail and event regions at their maxima, CPU storage double-buffered |
| `Z2-B-VFX-002` | 0.5 ms | 1.0 ms | 16,384 / 4,096 CPU particles at ≈ 30 ns per particle-step on one core, fanned out |
| `Z2-B-VFX-003` | 0.5 ms | 0.8 ms | 1,048,576 / 262,144 simulated slots plus one 131,072 / 32,768-key sort |
| `Z2-B-VFX-004` | 1.0 ms | 1.5 ms | the W-Cinematic hero view's overdraw at the tier's resolution |

The configuration keys' budget defaults are these proposals; the Stage 1 ruling replaces them with the adopted values in the same diff that writes the rows.

## Stage plan

### Stage 1

| Ticket | Kind | Content |
|---|---|---|
| Stage 1 per-system budget ticket for VFX | `needs-human` ruling TASK, after the Stage 1 per-system budget tickets for CORE (Z2T-CORE-061), JOBS (Z2T-JOBS-002), RPROD (Z2T-RPROD-002) and SCRIPT (Z2T-SCRIPT-017, which allocates `Z2-B-SCRIPT-024`, the input of the trigger-queue reserve) | writes, in one diff after `Check-Allocation.ps1`: `Z2-B-VFX-001` … `004`; the two rows Z2-B-VFX-005 "VFX lifecycle CPU" and Z2-B-VFX-006 "VFX trigger queue capacity per world" (cross-doc item X263, [Z2-N-BUDGET-470]); the trigger-queue reserve default `vfx.triggerQueueScriptReserve` = the allocated `Z2-B-SCRIPT-024` × `Z2-B-JOBS-002` (`sim.maxCatchUpTicks`) and the queue capacity = the reserve plus the C++ share it rules; the configuration defaults and `m_fBudgetValue`s that follow the adopted rows; VFX's `allocation.json`; the DecisionLog entry. It confirms, and does not write, VFX's per-world demand line of "CPU particle output" (`Zenith2/Docs/Design/VFX/Simulation.md`), which is an input of the earlier CORE ticket's `Z2-B-CORE-005` allocation ([Z2-N-BUDGET-472], X263): it records in its diff that the allocated value covers the line, or files the shortfall against the CORE ruling |

Besides it, only the ZenithFlux API slice (slice 2a below) is scheduled before the Stage 4 sprint; no VFX behaviour lands before Stage 5.

### Stage 5 slices

Every VFX ticket carries fix version `Z2 Stage 5`. Slice 2a is scheduled in the `Stage 3 — Playable authoring slice` sprint, because RPROD's Stage 3 `Flux_BuildForwardPlusView` calls `Flux_AddParticleDraw` and needs its definition at link time (declare first, implement after the callee); every other slice, waiting on no open decision, is scheduled in the `Stage 4 — Production content and worlds` sprint. Declaration precedes implementation: each API-only slice lands before the slices that call it, and a callee is implemented before its callers. The renderer members follow the same rule across areas: RPROD's Z2T-RPROD-014 (Stage 2 sprint) declares them, slice 2a (Stage 3 sprint) defines their bodies, and slice 7, the first caller, needs 2a.

| # | Slice | Kind | Needs |
|---|---|---|---|
| 2a | API-only, ZenithFlux (Stage 3 sprint): `Flux_ParticleEffect.h`, `Flux_Particles.h`, `Flux_ParticleInternal.h` (the `Flux_ParticleFrame` and `Flux_ParticleStep` declarations), `Common/Particles.slang`, with contract blocks; definitions of `Flux_AddParticleSimulation` and `Flux_AddParticleDraw` that declare no pass and of the six internal `Flux_Particle*` frame functions that do nothing ("Flux_ParticleSystem internals" of `Zenith2/Docs/Design/VFX/PublicAPI.md`), so RPROD's path functions in `Flux_Features.h` and a `BuildGraph` that already calls them link before the particle passes exist; `Flux_CreateParticleSystem` with the desc validation and construction step 1 (`Allocate`), `Flux_ReleaseParticleSystem` (no resources yet), the `Flux_ParticleSystem` public members, `Flux_ParticleWindBound`, and the bodies of `Flux_Renderer::EnableParticles`, `DisableParticles` and `GetParticleSystem` in RPROD's `Internal/Renderer/Flux_Renderer.cpp` (the ENABLE-FEATURES ruling extended to particles: RPROD declares, VFX defines), so the module start and shutdown of slice 7 link and run; `Test_ParticleDraw_NoParticleFrameDeclaresNoPass`, `Test_ParticleSystem_EnableDisableLifecycle`, `Test_ParticleSystem_Create_FailsAtAllocate_Unwinds` | API-only | RPROD's renderer API ticket Z2T-RPROD-014 (declares the three `Flux_Renderer` members and the owned particle-system member); RPROD's view-surface ticket Z2T-RPROD-011 (`Flux_ViewContext.h`, to which this slice adds `Flux_FrameShared::m_pxParticles` and `m_xParticleSorted` per REND X240 (5), unless that ticket already copied them); RPROD's producer of `Internal/Passes/Flux_PrologueFeatures.h`, the header that declares `Flux_AddParticleSimulation` (the gap of A5 row GB11 of `C:/dev/Zenith2-drafts/findings/A5_cross_area_blockers.md`, shared with GEOM), so the slice ticket stays `draft` until a ticket creates it, as GEOM's do; nothing of VFX |
| 1 | the ZenithVFX scaffold (target, `DEPENDS`, empty module entry points) | implementation | ARCH X242 (applied) |
| 2b | API-only, ZenithVFX: the six ZenithVFX headers and `ParticleInterfaces.slang` with contract blocks | API-only | 1, 2a |
| 3 | the effect document type, validator, curve bake and writer; the migration fixtures | implementation | 2b |
| 4 | the reader, `Flux_ParseParticleEffect`, the kind loader; the fuzz target | implementation | 3 |
| 5 | the CPU simulator, modules, noise, random functions and `Zenith_VFXRunReference` | implementation | 4 |
| 6 | capacity sizing and bounds baking; the `vfx.effect` step complete | implementation | 5, GEOM VX8 |
| 7 | components, configuration keys, errors, events, module start (with the configuration listener and the profiler panel provider; start step 6 and the unwind call slice 2a's `EnableParticles` and `DisableParticles` bodies), the authority census rows, the module shutdown order and `Test_VFXModule_ShutdownOrder` | implementation | 2a, 2b, MOD DV12 |
| 8 | the lifecycle system, trigger queue, `VFX.Spawn`, Luau bindings, trigger consumer | implementation | 7 |
| 9 | the presentation state: scheduling, LOD, CPU admission, bounds, CPU emitters, `VFX.Update` | implementation | 5, 8, ARCH X243 (applied) |
| 10 | `VFX.Extract`, colliders, effect lights and decals | implementation | 9, REND X240 (applied) |
| 11 | the particle system: construction steps 2 … 6 of `Flux_CreateParticleSystem` with their fault tests (the bodies of slice 2a unchanged), `Flux_ReleaseParticleSystem`'s releases, the pool, instance table, apply, pending list (merged entries and kept catch-up steps), recovery, configuration changes, the pressure response with its row in the memory-shedding case table, the render-side wind buckets; adds the `Flux_Particle*` calls to `Flux.BuildGraph` (setting `Flux_FrameShared::m_pxParticles`) and `ApplyConfigChanges` in RPROD's `Flux_Renderer.cpp` at the call sites RPROD's design specifies, since no RPROD ticket adds them (row VX2; either order links against slice 2a's no-op definitions); `Test_VFXModule_ShutdownReleasesRenderSideFirst` | implementation | 2a, 4, 7, RHI's pool-allocation ticket and VX3 |
| 12 | the simulation passes: emit, simulate, sort keys, radix sort; GPU admission; frame building | implementation | 11 |
| 13 | the draw pass: sprites, ribbons, trails, mesh particles, shading, lighting, soft, fog | implementation | 12, LIGHT VX7, GEOM VX8 |
| 14 | the particle type set step and the PSO precache entries | implementation | 13, REND X241 (applied), RPROD VX15 |
| 15 | the parity scenes and the engine effect content | implementation | 13 |
| 16 | preview behaviour (catch-up, freeze, statistics), hot reload, debug views | implementation | 12, EDIT VX11 |
| 17 | stress and integration suites; the budget benchmark | implementation | 15 |
| 18 | the Stage 5 VFX acceptance measurements on the reference hardware and the S24, including the sort stress that decides the S24's `vfx.sort.algorithm` | `needs-human` device run | 17 |

The depth-collision and lit-particle slices need LIGHT's and RPROD's Stage 5 surfaces, which exist by then; precipitation occlusion activates when ENV's Stage 5 capture lands (VX6) and is inert before it.

## Decisions

### Settled decisions

| Decision | Consequence for VFX |
|---|---|
| Z2-D-079 | effects are documents plus Slang, with previews and inspectors; no node graph |
| Z2-D-058 | dynamic particle data lives in a pool (RHI's mechanism), never in the read-only GPU scene tables |
| Z2-D-019 | particles are presentation only, so worlds with VFX stay deterministic; VFX's own reproducibility rule is "Determinism rules" |
| Z2-D-024 | ZenithVFX is a statically linked module selected in `game.json` |
| Z2-D-038, Z2-D-048 | JSON documents with migration; versioned binary `.zvfx` re-cooked on change |
| Z2-D-050 | VFX extracts in phase 12; the render side runs one frame behind |
| Z2-D-054 | particle passes bind through the bindless push constant |
| Z2-D-055, Z2-D-056, Z2-D-057 | passes are feature functions over handles; barriers automatic; aliasing on with the parity invariant |
| Z2-D-059 | particles draw per view by view kind and feature mask; previews request the bit |
| Z2-D-080 | effect components have `PHASE_LATE_SCRIPTS` authority; spawns are commands at sync points |
| Z2-D-016, Z2-D-051 | script spawns are transaction side effects consumed at the merge |
| Z2-D-015, Z2-D-025 | Slang owns the shared structs; every permutation is compiled offline and precached |
| Z2-D-014 | the particle programs follow the GPU rules and lints |
| Z2-D-035 | particles render on every path, parity by final image |
| Z2-D-013 | half precision for colour and shading, `float` for positions |
| Z2-D-062 | mesh particles use GEOM's vertex streams |
| Z2-D-045, Z2-D-046, Z2-D-040 | `Zenith_Result` errors, no mutable globals, owned and generational handles |
| Z2-D-012 | the VFX rows follow the budget change rule |
| Z2-D-021 | the flagship slices (OpenWorldSlice, CinematicSlice) whose content VFX's effects serve; the module's selection itself is `game.json`'s (MOD's module table, `Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"`) and its stage is the Roadmap's Stage 5 |
| Z2-D-036 | no tile-local merging of particle passes until D-TBDR is measured |
| Z2-D-085, Z2-D-097 | design-doc edits and rulings land in the ticket's own diff |

### Open decisions

| Decision | Effect on VFX |
|---|---|
| D-SEL-HASH (open, Stage 1) | instance keys, view IDs and type IDs use the selected 64-bit hash; none is serialized, so a change moves them all at once |
| D-SLANG (open, Stage 1) | the Slang version the particle programs compile with |
| D-PHASES (open, Stage 1) refinement of Z2-D-080 | the lifecycle system's phase and the command kind's phase mask follow the refinement |
| D-SCRIPT-TXN (open, Stage 1) refinement of Z2-D-051 | the trigger consumer's merge point |
| D-CLASS-SIM (open, Stage 2) | whether destruction debris exists as physics; VFX's mesh particles stay presentation either way |
| D-TBDR (open, Stage 5) | does not gate VFX |

### VFX rulings

Design choices VFX owns, each with its alternatives and the reason; none overrides a settled decision. Every row is a choice the plan leaves to the area.

<!-- elaboration -->
| Ruling | Chosen | Alternatives | Reason |
|---|---|---|---|
| V1 presentation-only particles | particles never feed simulation; gameplay impacts come from physics and scripts | particle collision events visible to gameplay | keeps Z2-D-019 unconditional and makes shedding and device loss harmless |
| V2 ring slots by spawn ordinal | `slot = ordinal mod C`, every dispatch and draw sized on the CPU, no alive or dead list and no GPU-written count | (a) atomic alive and dead lists with GPU-written counts, `DispatchIndirect` and `DrawIndexedIndirectCount` (the core tables' text); (b) ring slots plus a deterministic order-preserving prefix-sum compaction of live slots per emitter for sort and draw, with GPU-written instance counts in CPU-written commands | (a) atomic appends order particles by completion, so blended and even additive results (float addition order) differ between runs, which [Z2-N-REND-433] and [Z2-N-REND-791] forbid. (b) is deterministic and makes sort and draw cost proportional to live count, but a GPU-written count means admission and draw cost are unknown before submission, and the CPU would need a readback to budget them. V2 is applied to the core tables through cross-doc item X240 under the orchestrator's 2026-10-03 ruling; (b) is the named fallback if the S24 device run shows dead-slot work above `Z2-B-VFX-003` or `-004` |
| V3 spawn counts on the CPU | `VFX.Update` computes every rate, distance and burst spawn; an event target's dispatch size is CPU-known, its children GPU-determined | GPU-side rate accumulators | exact admission, deterministic ordinals, CPU emitters and GPU emitters share one scheduler |
| V4 CPU-resident effect kind | constants image on the CPU, GPU buffer created on first use | a GPU-resident kind | headless runs load effects through the real loader; device loss rebuilds from the CPU copy; ASSET's kind row reads "GPU resident: no" |
| V5 two draw partitions | premultiplied blended (sorted by rank) then additive | per-pixel OIT; per-particle global sort across emitters | bounded cost with deterministic order; additive is order-independent up to float rounding, which the fixed order makes reproducible |
| V6 one sort per frame in the sort view | per-emitter segments of a stable radix sort, `onesweep` or the reduce-then-scan fallback | a sort per view | GPU cost; other views use the sort view's order, a defined approximation |
| V7 analytic colliders plus depth collision | `Zenith.VFX.Collider` and the sort view's previous HiZ (world-space GPU emitters of the sort view's world) | physics-engine queries | no module edge to physics; GPU-friendly |
| V8 no particle shadows | particles cast no shadow | shadow-casting particles | shadow views cost; lit particles receive shadows in pixel mode |
| V9 previews by fixed-step catch-up | seek re-simulates from 0 in steps of the preview world's tick duration (16,667 µs in a 60 Hz world), ≤ 8 steps per frame executed in arrival order and drawn after the last, only in worlds of purpose `WORLD_PURPOSE_PREVIEW`, event freshness keyed by simulate step, steps of an unexecuted frame kept unmerged (at most 64), an overflowing seek interrupted and re-issued; its live particle state equals one-tick-per-frame playback, the generation byte aside | snapshots; GPU readback | a scrubbed frame is a frame a game could show; no extra GPU state |
| V10 composed bounds | the emitter-frame box baked by the CPU reference simulation at cook time, plus an analytic world displacement box and a runtime inherited-velocity term; an event target composed from its chain root's box, its spawn-relative box and the chain's composed world and inherited terms; wind added on the render side from each frame's wind bound | runtime GPU bounds readback; one baked emitter-space box (rotation, emitter motion and distance spawning unaccounted); a cook-time wind reference (60 m/s with gusts, 210 m/s, makes windy boxes useless; any smaller one is not a bound) | no GPU-to-CPU dependency in shipping; for the linear module class a proven bound of the world, inherited and wind parts and the bucket unions under emitter rotation, motion, distance spawning, event chains, any wind and steps up to 1/15 s (`D_own = ½ a L (L + Δ_max)` for the semi-implicit integrator), around the emitter-frame box the cook measures at 1/60 s, which rests on `margin` and the development-build validation at larger run-time steps ("Bounds" of `Zenith2/Docs/Design/VFX/Simulation.md`, "Bounds baking" of `Zenith2/Docs/Design/VFX/Authoring.md`); an estimate, flagged and warned, for the nonlinear class; the development-build validation catches escapes |

### Ring-slot costs

V2's costs, stated so that the budget rows and capacities are sized for them:

| Cost | Consequence | S24 numbers at the default tier |
|---|---|---|
| work scales with capacity, not live count | `ParticleSimulate` runs one thread per slot of every admitted emitter, `ParticleCompactAndSortKeys` one per slot of every depth-sorted emitter, and every draw issues `C` instances (a dead slot is a degenerate primitive); `Z2-B-VFX-003` and `-004` are calibrated on admitted capacity | at `vfx.gpuActiveCapacity` 262,144: 262,144 simulate threads per frame whatever the live count, and up to 1,048,576 sprite vertex invocations (`4 × C`) for the drawn emitters |
| deaths free no slot | a slot is reused only when its ordinal comes round, so capacity covers `r_max × L_max`, a factor `L_max / E[L]` above a list design's need | rain spawned at 2,000 per second with a 2 s lifetime but killed by occlusion or collision after 0.3 s on average holds 4,000 slots for ≈ 600 live particles: 85% of its simulate and vertex work is dead slots; capacity is `rate × L_max` whatever kills particles early, so a weather effect authors its lifetime as its spawn volume's fall time and its kill volume only removes particles the camera left behind ("Weather effects for ENV" of `Zenith2/Docs/Design/VFX/Authoring.md`) |
| the CPU never learns a GPU live count | no admission, scheduling or draw decision reads one; ribbon UVs use a CPU-known span; the preview's live column is an upper bound unless the development-build validation readback is on, and then one to F frames late | — |
| event children need ordinal-derived slots | a target's capacity covers its children's whole ordinal span, and event consumers cost `C_src × E_edge × k` threads per step and edge (`C_src × E × k` over a source's two edges of equal `k`), admitted with the target ("Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`) | a 4,096-slot source with a 3-collision edge and a death edge of 16 children each takes 196,608 + 65,536 = 262,144 consumer threads, the S24's whole budget, so such an effect is refused rather than overrunning it |

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-VFX-001 | Module library target | S5 | Zenith2/Docs/Design/VFX.md → "Library dependencies"; Zenith2/Docs/Design/VFX.md → "Target summary"; Zenith2/Docs/Design/VFX.md → "ZenithVFX files" |
| Z2-R-VFX-002 | API-only header slice first | S5 | Zenith2/Docs/Design/VFX.md → "Stage 5 slices"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Header map" |
| Z2-R-VFX-003 | Module entry points | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header" |
| Z2-R-VFX-004 | Module start unwinds | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"; Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests" |
| Z2-R-VFX-005 | Headless module behaviour | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header" |
| Z2-R-VFX-006 | Component types registered | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXComponents header"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Component registration" |
| Z2-R-VFX-007 | Error enumerations | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXErrors header"; Zenith2/Docs/Design/VFX.md → "Error enumerations" |
| Z2-R-VFX-008 | Contract blocks on every public type | S5 | Zenith2/Docs/Design/VFX.md → "Header summary" |
| Z2-R-VFX-009 | Particles are presentation only | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Presentation-only particles" |
| Z2-R-VFX-010 | Presentation clock | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Presentation clock" |
| Z2-R-VFX-011 | Lifecycle state machine | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine"; Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle system" |
| Z2-R-VFX-012 | Destroy after seconds in ticks | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine" |
| Z2-R-VFX-013 | Invalid component values ignored | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine" |
| Z2-R-VFX-014 | Started and stopped events | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXEvents header"; Zenith2/Docs/Design/VFX/Simulation.md → "Lifecycle state machine" |
| Z2-R-VFX-015 | Lifecycle state saved and restored | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Attached effect lifecycle" |
| Z2-R-VFX-016 | Spawn command kind | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header" |
| Z2-R-VFX-017 | Luau spawn bindings | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Luau surface"; Zenith2/Docs/Design/VFX.md → "Luau errors and asserts" |
| Z2-R-VFX-018 | Script trigger consumer | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "VFX trigger record"; Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-019 | Trigger queue full policy | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-020 | Trigger order deterministic | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-021 | Detached effects attach to targets | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-022 | Detached effect cap and truncation | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-023 | Emitter instance state machine | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Emitter instance state machine"; Zenith2/Docs/Design/VFX/Simulation.md → "Commands per frame" |
| Z2-R-VFX-024 | Rate spawning exact | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Rate spawning" |
| Z2-R-VFX-025 | Burst spawning | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Burst spawning" |
| Z2-R-VFX-026 | Distance spawning | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Distance spawning" |
| Z2-R-VFX-027 | Prewarm | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Prewarm" |
| Z2-R-VFX-028 | Particle random numbers | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Particle random functions"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle random numbers" |
| Z2-R-VFX-029 | Instance keys and seeds | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Instance keys" |
| Z2-R-VFX-030 | Ring slots by spawn ordinal | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Spawn ordinals and capacity"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout" |
| Z2-R-VFX-031 | Particle initialisation and update order | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Initialisation algorithms"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle update order" |
| Z2-R-VFX-032 | CPU emitter simulation | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "CPU update algorithm"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU emitter storage" |
| Z2-R-VFX-033 | CPU particle output | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output"; Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands" |
| Z2-R-VFX-034 | CPU admission per world | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "CPU admission per world" |
| Z2-R-VFX-035 | CPU particle update cost | S5 | Zenith2/Docs/Design/VFX.md → "Owned rows" |
| Z2-R-VFX-036 | CPU storage accounted | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU emitter storage" |
| Z2-R-VFX-037 | Particle pool through RHI pools | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout"; Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges" |
| Z2-R-VFX-038 | Particle system construction unwinds | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem" |
| Z2-R-VFX-039 | Simulation pass declarations | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "ParticleEmit table"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSimulate table"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleCompactAndSortKeys table" |
| Z2-R-VFX-040 | Dead slots untouched | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSimulate dispatch" |
| Z2-R-VFX-041 | GPU admission per frame | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "GPU admission per frame" |
| Z2-R-VFX-042 | Frame building | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Frame building"; Zenith2/Docs/Design/VFX/Rendering.md → "Draw pipeline overview" |
| Z2-R-VFX-043 | GPU simulation reproducible | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules" |
| Z2-R-VFX-044 | Async compute identical | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers" |
| Z2-R-VFX-045 | Aliasing invariant | S5 | Zenith2/Docs/Design/VFX/Tests.md → "GPU simulation tests"; Zenith2/Docs/Design/VFX/Tests.md → "Render tests" |
| Z2-R-VFX-046 | CPU and GPU parity of modules | S5 | Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests" |
| Z2-R-VFX-047 | Local-space parity | S5 | Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests" |
| Z2-R-VFX-048 | Capacity overflow parity | S5 | Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests" |
| Z2-R-VFX-049 | CPU and GPU targets agree | S5 | Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests" |
| Z2-R-VFX-050 | Command validation | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderEmitterCommand"; Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour" |
| Z2-R-VFX-051 | Deferred frames keep commands | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames" |
| Z2-R-VFX-052 | Device loss recovery | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Device loss" |
| Z2-R-VFX-053 | Full-state resync | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Full-state frames" |
| Z2-R-VFX-054 | Effect constants image | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect constants image" |
| Z2-R-VFX-055 | Code-authored update types | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleUpdateType"; Zenith2/Docs/Design/VFX/Authoring.md → "Code-authored effects" |
| Z2-R-VFX-056 | Particle simulation GPU time | S5 | Zenith2/Docs/Design/VFX.md → "Owned rows" |
| Z2-R-VFX-057 | Particle memory | S5 | Zenith2/Docs/Design/VFX.md → "Owned rows"; Zenith2/Docs/Design/VFX.md → "Proposed values" |
| Z2-R-VFX-058 | Emitter ribbons connect consecutive ordinals | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons" |
| Z2-R-VFX-059 | Ribbon UV modes | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons" |
| Z2-R-VFX-060 | CPU ribbons | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ribbons"; Zenith2/Docs/Design/VFX/Authoring.md → "Engine effect content" |
| Z2-R-VFX-061 | Trail sampling | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Particle trails" |
| Z2-R-VFX-062 | Trail ribbons | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Particle trail ribbons" |
| Z2-R-VFX-063 | Trail and event region sizing | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing"; Zenith2/Docs/Design/VFX/Simulation.md → "Particle pool layout" |
| Z2-R-VFX-064 | Analytic colliders | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Analytic colliders" |
| Z2-R-VFX-065 | Collider gathering | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Analytic colliders"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_RenderParticleCollider" |
| Z2-R-VFX-066 | Depth-buffer collision | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Depth-buffer collision" |
| Z2-R-VFX-067 | Collision events | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning" |
| Z2-R-VFX-068 | Precipitation occlusion | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Precipitation occlusion"; Zenith2/Docs/Design/VFX/Authoring.md → "Weather effects for ENV" |
| Z2-R-VFX-069 | No physics dependency | S5 | Zenith2/Docs/Design/VFX.md → "Library dependencies"; Zenith2/Docs/Design/VFX.md → "VFX rulings" |
| Z2-R-VFX-070 | Baked bounds | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXReference header" |
| Z2-R-VFX-071 | Fixed bounds and code-authored types | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking" |
| Z2-R-VFX-072 | Runtime bounds | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Bounds" |
| Z2-R-VFX-073 | GPU bounds validation | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "GPU bounds validation" |
| Z2-R-VFX-074 | Bake reproducible | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/Tests.md → "Determinism tests" |
| Z2-R-VFX-075 | Per-view frustum culling | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling" |
| Z2-R-VFX-076 | Cull behaviour | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Culling behaviour" |
| Z2-R-VFX-077 | View feature gating | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling" |
| Z2-R-VFX-078 | Per-view draw limit | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling" |
| Z2-R-VFX-079 | Dead slots draw nothing | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Sprite rendering" |
| Z2-R-VFX-080 | Stable radix sort | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSort table" |
| Z2-R-VFX-081 | Sort partition edge cases | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "ParticleSort algorithm" |
| Z2-R-VFX-082 | Per-emitter sort segments | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "ParticleCompactAndSortKeys dispatch" |
| Z2-R-VFX-083 | Sort view and emitter ranks | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Emitter ranks and the sort view" |
| Z2-R-VFX-084 | Two draw partitions | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Partitions"; Zenith2/Docs/Design/VFX/Rendering.md → "Order within an emitter" |
| Z2-R-VFX-085 | Sorted-particle budget | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Sorted-particle admission" |
| Z2-R-VFX-086 | LOD viewers | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Viewers" |
| Z2-R-VFX-087 | Screen size and hysteresis | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Screen size and LOD selection" |
| Z2-R-VFX-088 | LOD levels disable modules | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "LOD keys"; Zenith2/Docs/Design/VFX/Simulation.md → "Screen size and LOD selection" |
| Z2-R-VFX-089 | Tier defaults | S5 | Zenith2/Docs/Design/VFX.md → "Configuration keys"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers" |
| Z2-R-VFX-090 | Event ring capacity | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning" |
| Z2-R-VFX-091 | Effect lights | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Effect lights" |
| Z2-R-VFX-092 | Effect decals | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Effect decals" |
| Z2-R-VFX-093 | Weather effects for ENV | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Weather effects for ENV" |
| Z2-R-VFX-094 | Stage 1 budget values | S1 | Zenith2/Docs/Design/VFX.md → "Owned rows"; Zenith2/Docs/Design/VFX.md → "Proposed values"; Zenith2/Docs/Design/VFX.md → "Pending row needs"; Zenith2/Docs/Design/VFX.md → "Stage 1"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU particle output" |
| Z2-R-VFX-095 | Configuration keys | S5 | Zenith2/Docs/Design/VFX.md → "Configuration keys" |
| Z2-R-VFX-096 | Particles draw GPU time | S5 | Zenith2/Docs/Design/VFX.md → "Owned rows" |
| Z2-R-VFX-097 | Counters published | S5 | Zenith2/Docs/Design/VFX.md → "Counters"; Zenith2/Docs/Design/VFX.md → "On-screen profiler rows" |
| Z2-R-VFX-098 | Memory pressure shedding | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Memory pressure response" |
| Z2-R-VFX-099 | Pool full behaviour | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour"; Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges" |
| Z2-R-VFX-100 | Maximum-capacity stress | S5 | Zenith2/Docs/Design/VFX/Tests.md → "Stress tests" |
| Z2-R-VFX-101 | Churn stress | S5 | Zenith2/Docs/Design/VFX/Tests.md → "Stress tests" |
| Z2-R-VFX-102 | Debug views | S5 | Zenith2/Docs/Design/VFX.md → "Debug views"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem" |
| Z2-R-VFX-103 | Sprite facing modes | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Facing modes"; Zenith2/Docs/Design/VFX/Rendering.md → "Sprite rendering" |
| Z2-R-VFX-104 | Sub-UV flipbooks | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Sub-UV flipbooks" |
| Z2-R-VFX-105 | Soft particles | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Soft particles" |
| Z2-R-VFX-106 | Fog and pre-exposure | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Fog and pre-exposure" |
| Z2-R-VFX-107 | Lit particles | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Lighting modes" |
| Z2-R-VFX-108 | Mesh particles | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Mesh particle rendering" |
| Z2-R-VFX-109 | Indirect draw runs | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Draw records and indirect commands" |
| Z2-R-VFX-110 | Draw pass declarations | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Particles table" |
| Z2-R-VFX-111 | Backend and path parity | S5 | Zenith2/Docs/Design/VFX/Tests.md → "Render tests"; Zenith2/Docs/Design/VFX/Rendering.md → "Capability and quality tiers" |
| Z2-R-VFX-112 | PSO precache | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "PSO keys and permutations" |
| Z2-R-VFX-113 | Shader rules | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Blend modes"; Zenith2/Docs/Design/VFX/Tests.md → "Gate steps" |
| Z2-R-VFX-114 | Texture fallback | S5 | Zenith2/Docs/Design/VFX/Rendering.md → "Render-side failure behaviour" |
| Z2-R-VFX-115 | Effect document schema | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Top-level keys"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation" |
| Z2-R-VFX-116 | Effect identity | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Importer and intermediate" |
| Z2-R-VFX-117 | Curve baking | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Curve baking" |
| Z2-R-VFX-118 | Capacity sizing | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing" |
| Z2-R-VFX-119 | Cooked effect format | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Cooked effect format"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Section kinds" |
| Z2-R-VFX-120 | Reader rejects corrupt files | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Reader validation" |
| Z2-R-VFX-121 | Format limits | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Format limits"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document limits" |
| Z2-R-VFX-122 | Document migration | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect document migration" |
| Z2-R-VFX-123 | Cook key inputs | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Effect cook step" |
| Z2-R-VFX-124 | Particle type set | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"; Zenith2/Docs/Design/VFX/Rendering.md → "PSO keys and permutations" |
| Z2-R-VFX-125 | Cook report members | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Cook report members" |
| Z2-R-VFX-126 | Effect kind loader | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Effect kind loader" |
| Z2-R-VFX-127 | Missing effect plays nothing | S5 | Zenith2/Docs/Design/VFX.md → "Failure table" |
| Z2-R-VFX-128 | Engine effect content | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Engine effect content" |
| Z2-R-VFX-129 | Preview session | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Preview session" |
| Z2-R-VFX-130 | Timeline scrubbing | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"; Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps" |
| Z2-R-VFX-131 | Catch-up only in previews | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_Module_VFX header" |
| Z2-R-VFX-132 | Preview statistics | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Preview statistics"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleStats" |
| Z2-R-VFX-133 | Effect hot reload | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Hot reload" |
| Z2-R-VFX-134 | Inspector parameter data | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_ParticleEffectAsset" |
| Z2-R-VFX-135 | Edit-world play controls | S5 | Zenith2/Docs/Design/VFX/Authoring.md → "Effect inspector" |
| Z2-R-VFX-136 | Runtime-spawned attached effects distinct | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Instance keys"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXEffectComponent" |
| Z2-R-VFX-137 | Event children slots never collide | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/Authoring.md → "Capacity sizing" |
| Z2-R-VFX-138 | Event freshness by simulate step | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand" |
| Z2-R-VFX-139 | Event graphs admitted as one unit | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "GPU admission per frame"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU admission per world"; Zenith2/Docs/Design/VFX/Simulation.md → "CPU update algorithm" |
| Z2-R-VFX-140 | Shutdown releases render side first | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Module shutdown order"; Zenith2/Docs/Design/VFX.md → "Lifetime" |
| Z2-R-VFX-141 | Render-side budgets follow tier changes | S5 | Zenith2/Docs/Design/VFX.md → "Runtime configuration changes"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem" |
| Z2-R-VFX-142 | On-screen profiler rows and panel | S5 | Zenith2/Docs/Design/VFX.md → "On-screen profiler rows" |
| Z2-R-VFX-143 | Detached start time | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Zenith_VFXCommands header" |
| Z2-R-VFX-144 | Ordinal reset before wrap | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Spawn ordinals and capacity" |
| Z2-R-VFX-145 | Generation wrap clears ranges | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges" |
| Z2-R-VFX-146 | Instance timeout | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Emitter pool ranges" |
| Z2-R-VFX-147 | Command stream backend independent | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Determinism rules"; Zenith2/Docs/Design/VFX/Simulation.md → "Commands per frame" |
| Z2-R-VFX-148 | Document validator fuzzed | S5 | Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation"; Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests" |
| Z2-R-VFX-149 | Pending triggers never saved | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue" |
| Z2-R-VFX-150 | Authority census rows | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Authority census rows" |
| Z2-R-VFX-151 | Sort fallback identical | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Reduce-then-scan fallback"; Zenith2/Docs/Design/VFX/Rendering.md → "ParticleSort table" |
| Z2-R-VFX-152 | Depth collision within its world | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Depth-buffer collision" |
| Z2-R-VFX-153 | ZenithFlux API slice links RPROD paths | S5 | Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem internals"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"; Zenith2/Docs/Design/VFX.md → "Stage 5 slices" |
| Z2-R-VFX-154 | Catch-up steps executed in arrival order | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Catch-up steps"; Zenith2/Docs/Design/VFX/Simulation.md → "Frame building"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleFrame" |
| Z2-R-VFX-155 | Seek steps survive deferred frames | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Deferred and failed frames"; Zenith2/Docs/Design/VFX/Authoring.md → "Timeline scrubbing"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem" |
| Z2-R-VFX-156 | Event consumers read their own edge | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleEventRecord"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_EmitterCommand" |
| Z2-R-VFX-157 | Event-target bounds from the chain root | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Emitter record section" |
| Z2-R-VFX-158 | Wind term from the frame wind | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/PublicAPI.md → "Particle wind bound"; Zenith2/Docs/Design/VFX/Rendering.md → "Per-view emitter culling" |
| Z2-R-VFX-159 | Bounds proven or flagged as estimates | S5 | Zenith2/Docs/Design/VFX/Simulation.md → "Bounds"; Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"; Zenith2/Docs/Design/VFX/EffectFormat.md → "Document validation" |

## Reconciliation items

Core-doc changes VFX depends on are cross-doc items in `C:/dev/Zenith2-drafts/reviews/CROSS_DOC_ITEMS.md`; needs from other areas' designs are rows VX1 … VX17 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.

| Item | Owner | Need |
|---|---|---|
| X240 | REND (core docs), applied | the VFX pass tables replace the core particle rows of `Zenith2/Docs/Rendering/PerPass/Common.md` (ring slots, the radix sort with its reduce-then-scan fallback), the pool row and prologue row 4; `Flux_RenderWorldSection::m_axParticleColliders` and `AddParticleCollider` with the CPU-span copy rule; `Flux_FrameShared::m_pxParticles` and `m_xParticleSorted`; VFX's structs in the shared-struct table; (7) `ParticleEmit` and `ParticleSimulate` repeat once per step, at most 128, in a frame carrying catch-up or kept steps |
| X241 | REND (core docs), applied | the particle shader directories, game particle types, cook-time particle permutations in `m_ulMaterialVariantID`, the particle reflection file and `--particle-type-set` |
| X242 | ARCH (core docs), applied | the ZenithJobs, ZenithProfiling and ZenithVertexFormat edges of ZenithVFX; the Rendering shedding row text |
| X243 | ARCH (core docs), applied | `Zenith_WorldAttachContext::GetSimulationClock` and `GetWorldPurpose` |
| X263 | BUDGET (core docs), applied | the two pending rows as the next two VFX rows, the SCRIPT predecessor of the VFX producer row, and the render-world writers' demand lines (VFX's "CPU particle output") as an input of the CORE ticket's `Z2-B-CORE-005` allocation |
| VX1 | REND | superseded by X240 |
| VX2 | RPROD | answered by the ENABLE-FEATURES ruling extended to particles, applied by RPROD's post-round pass: Z2T-RPROD-014 and `Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"` declare `Flux_Renderer::EnableParticles`, `DisableParticles`, `GetParticleSystem` and the owned particle-system member, and VFX's slice 2a writes the bodies (the two `Flux_FrameShared` members are Rendering's, X240 (5)); answered too: RPROD's design specifies the call sites (`Flux.BuildGraph` steps 4, 7c, 11 and 13, the recovery step, the `ApplyConfigChanges` forwarding); in code they are added by slice 11, since no RPROD ticket adds them (either order links against slice 2a's no-op definitions); RPROD's Stage 3 path function links against VFX's slice-2a definitions; still open: RPROD's producer of `Internal/Passes/Flux_PrologueFeatures.h` (the gap of A5 row GB11, shared with GEOM), on which slice 2a stays `draft` |
| VX3 | RHI | the particle pool's three regions in the pool row and their growth policy as VFX's |
| VX4 | ARCH | superseded by X242 |
| VX5 | ASSET | (a) resolved: ASSET applied kind 19's "GPU resident: no"; (b) `Zenith.VFX.EffectDocument` in the registered document types; (c) `*.vfx.json` mapped by `zenith-doc` with the `vfx.effectDocument` intermediate |
| VX6 | ENV | the precipitation occlusion capture as `Flux_FrameShared::m_xPrecipitationOcclusion` and its mapping in `Env_FrameEnvironment`; weather prefabs name world-space VFX effects following the anchor, carrying no VFX component before Stage 5, with lifetimes authored as their spawn volumes' fall times; (c) informational: the particle system reads each world's `m_xBlocks.m_xWind` and local wind sources from the render environment on the CPU for the bounds' wind term |
| VX7 | LIGHT | lit particles' surface and lighting-channel use; `Flux_MakeNamedLightKey` keys of effect lights |
| VX8 | GEOM | `GetDrawRange`, `GetRasterPoolStreams`, the published `geom.meshBounds`; procedural "VFX ribbons" wording refers to VFX's ribbon programs |
| VX9 | SCRIPT | resolved: block ZS5600–ZS5699 is VFX's |
| VX10 | MOD, ARCH | `GetSimulationClock`, now X243 (a) |
| VX11 | EDIT | the effect preview host creating preview worlds of purpose `WORLD_PURPOSE_PREVIEW`, the inspector customisations, viewer components on editor cameras; the session re-issuing a seek once when `m_bSeekInterrupted` turns true |
| VX12 | AUDIO | effect sounds by the stable event names `Zenith.VFX.EffectStarted` / `EffectStopped` |
| VX13 | BUDGET | superseded by X263 (applied by core-doc patch e) |
| VX14 | DEVT | VFX conforms to DEVT: its `vfx` tab rows (with `m_szLimit` per BAR row) and its panel provider registered through `GetProfilerOverlay` (DEVT's row DV12); DEVT's debug UI exposes `Flux_ParticleDebugFlags` |
| VX15 | RPROD | `rprod.shaders` runs after `vfx.particleTypes`; the particle PSO precache entries |
| VX16 | ANIM | notifies reach effects through scripts calling `VFX.Spawn` with `leadSeconds`; `ANIM_EVENT_FLAG_VFX` is advisory for scripts |
| VX17 | SCRIPT | the accessor through which a module's binding reads a handle's published load result in the calling world (the state `Asset:IsLoaded()` reads), and the effect class and contexts it registers under, for `VFX.ParameterIndex` |
