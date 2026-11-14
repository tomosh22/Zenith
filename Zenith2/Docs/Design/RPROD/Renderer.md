# RPROD design: the renderer, quality tiers and frame pacing

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies `Flux_Renderer` (the object the composition root creates at startup step 13 and whose node bodies the render subgraph runs), its configuration keys, the consumption of `Flux_RenderWorld`, the render frame sequence node by node, the per-platform quality tiers with their selection, and the frame pacer.

## Scope of this file

The specification is `Zenith2/Docs/Rendering.md → "Purpose and scope"` (the render world hand-off), "Render views" (quality tier members), `Zenith2/Docs/Rendering/PerPass/Common.md → "Present"` (the present contract and [Z2-N-REND-805]) and `Zenith2/Docs/Rendering/FrameAnatomy.md` [Z2-N-REND-790], which delegates frame pacing to this document. The RHI hooks the pacer calls are `Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"`. Code lives in `Zenith2/Engine/ZenithFlux/Internal/Renderer/`, `Internal/Quality/` and `Internal/Pacing/`.

## Flux_Renderer header

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_ConfigStore.h"
#include "ZenithCore/Zenith_OperationHandle.h"
#include "ZenithRHI/Zenith_RHIDevice.h"
#include "ZenithFlux/Flux_RenderWorld.h"
#include "ZenithFlux/Flux_GraphSettings.h"
#include "ZenithFlux/Flux_ShadingPath.h"
#include "ZenithFlux/Flux_Quality.h"
#include "ZenithFlux/Flux_ViewExtension.h"
#include "ZenithFlux/Flux_PSOPrecache.h"
#include "ZenithFlux/Flux_QueueValues.h"       // RHI's Flux_DeviceLossState

class Zenith_Heap;
class Flux_ProceduralGeometrySystem;           // GEOM's, Flux_ProceduralGeometry.h
class Zenith_JobContext;
class Zenith_JobSystem;
class Zenith_AssetRegistry;
class Zenith_Window;
class Flux_EnvironmentRenderer;              // ENV's, Internal/Environment/Flux_EnvironmentRenderer.h
class Flux_UIRenderer;                       // UI's, ZenithFlux/Flux_UI.h
struct Flux_UIRendererDesc;                  // UI's, ZenithFlux/Flux_UI.h
class Flux_ParticleSystem;                   // VFX's, ZenithFlux/Flux_Particles.h (row VX2)
struct Flux_ParticleSystemDesc;              // VFX's, ZenithFlux/Flux_Particles.h
enum Flux_ParticleError : uint8;            // VFX's, ZenithFlux/Flux_ParticleEffect.h (opaque declaration; same underlying type)

enum Flux_RendererError : uint8
{
	FLUX_RENDERER_ERROR_OUT_OF_MEMORY = 1,
	FLUX_RENDERER_ERROR_DEVICE = 2,              // a device call of a creation step failed; the step is in Zenith_Log
	FLUX_RENDERER_ERROR_SHADER_LIBRARY = 3,      // the boot shader containers are missing or refused (FLUX_SHADER_ERROR_*)
	FLUX_RENDERER_ERROR_PRECACHE_FILE = 4,       // pso_precache.zbin missing or refused
	FLUX_RENDERER_ERROR_CONFIGURATION = 5,       // a key outside its domain after validation
	FLUX_RENDERER_ERROR_EXTENSION_LIMIT = 6,     // more than 32 view extensions
	FLUX_RENDERER_ERROR_ALREADY_ENABLED = 7,     // EnableEnvironment or EnableUI called a second time
};

struct Flux_RendererSettings
{
	Flux_ShadingPath m_eShadingPath = FLUX_SHADING_PATH_FORWARD_PLUS;   // r.shadingPath; desktop main, editor, texture views
	Flux_GraphSettings m_xGraph;
	Flux_PacingSettings m_xPacing;
	float m_fDepthPrecisionToleranceMetres = 0.25f;                      // r.depth.precisionToleranceMetres (Z2-B-RPROD-029, pending; proposed)
	bool m_bRecordPSOs = false;                                          // --record-psos (development builds)
};

struct Flux_RendererDesc
{
	Zenith_RHIDevice* m_pxDevice = nullptr;
	Zenith_Window* m_pxMainWindow = nullptr;
	Zenith_Platform* m_pxPlatform = nullptr;    // the exit sink's fatal message box (Zenith_Platform::ShowFatalMessage, Zenith2/Docs/Design/PLAT/Platform.md)
	Zenith_JobSystem* m_pxJobs = nullptr;
	Zenith_AssetRegistry* m_pxAssets = nullptr;
	const Zenith_ConfigStore* m_pxConfig = nullptr;
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_StringView m_strGameName;
	Zenith_StringView m_strActiveQualityTier;    // the tier ID PLAT resolved (Zenith2/Docs/Design/PLAT/Configuration.md "Quality tier resolution")
	Zenith_Span<const Zenith_StringView> m_axPlatformTiers;   // the platform's tier list, highest first
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;       // tests alone
};

/// @contract type Flux_Renderer
/// Ownership: Created by ZenithRuntime at startup step 13 through Create as a Zenith_Owned
///   (Zenith2/Docs/Architecture/CompositionRoot.md "Startup sequence"), after the RHI device of step 12 and before the
///   script runtime; destroyed in shutdown step 6 after every GPU queue went idle and retired ([Z2-N-CONTRACT] shutdown
///   table as cross-doc item X76 amends it). It owns the graph caches, the view registry, the culling system, the PSO
///   table and shader library, the frame pacer, the quality state and the RHI- and GEOM-owned Flux subsystems
///   (Zenith2/Docs/Design/RHI.md "Startup and shutdown"; Zenith2/Docs/Design/GEOM.md "Construction and shutdown").
/// Thread affinity: ANY_THREAD_EXCEPT(Create, ApplyConfigChanges, RegisterViewExtension, UnregisterViewExtension,
///   SetRendererSettings, RunPrologue, GetProceduralGeometry, EnableEnvironment, EnableUI, EnableParticles, DisableParticles, GetParticleSystem, BuildGraph, RecordPartition, Submit, Present). Create, the
///   configuration and extension calls and RunPrologue run on the main thread in the frame prologue or at startup;
///   GetProceduralGeometry on the main thread at startup (module start, step 15), before the first frame, when no render
///   subgraph runs; BuildGraph runs on the Flux.BuildGraph node, RecordPartition on the Flux.Record sub-jobs, Submit and
///   Present on the render-submit thread (Zenith2/Docs/Architecture/CPUFrameGraph.md "Frame structure"). GetPrecacher,
///   GetStats and GetDeviceLossState are ANY_THREAD.
/// Invalidation: None across frames; per-frame spans returned by GetStats are snapshots.
/// Units: Frames; nanoseconds; bytes.
/// Errors: Create returns Flux_RendererError after unwinding every constructed step in reverse
///   ([Z2-N-CONTRACT-340]); a compile error of a frame's graph is a programmer error (ZENITH_CHECK in development
///   builds after the diagnostics are logged; in Shipping the frame is skipped and counted, [Z2-N-REND-291]).
/// Reentrancy: NONE. It invokes Flux_ViewExtension::AddPasses; no renderer member may be called from inside it.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Flux_Renderer
{
public:
	static Zenith_Result<Zenith_Owned<Flux_Renderer>, Flux_RendererError> Create(const Flux_RendererDesc& xDesc);
	~Flux_Renderer();

	// Frame prologue, main thread
	void ApplyConfigChanges(Zenith_Span<const Zenith_ConfigKey> axChanged, Zenith_StringView strActiveTier);   // prologue step 3
	void SetRendererSettings(const Flux_RendererSettings& xSettings);                                         // console and debug UI
	Zenith_Result<void, Flux_RendererError> RegisterViewExtension(Flux_ViewExtension& xExtension);
	void UnregisterViewExtension(Flux_ViewExtension& xExtension);
	void RunPrologue();                                                                // prologue step 4 (RHI row R19)
	Flux_ProceduralGeometrySystem& GetProceduralGeometry();               // GEOM's system, for module source registration (row GM22)
	Zenith_Result<Flux_EnvironmentRenderer*, Flux_RendererError> EnableEnvironment();          // ENV's module start (row EN17); at most once
	Zenith_Result<Flux_UIRenderer*, Flux_RendererError> EnableUI(const Flux_UIRendererDesc& xDesc);   // UI's module start step 6 (row UI7); at most once
	Zenith_Result<Flux_ParticleSystem*, Flux_ParticleError> EnableParticles(const Flux_ParticleSystemDesc& xDesc);   // VFX's module start, startup step 15 (row VX2); at most once
	void DisableParticles();                                                          // render-idle point: VFX's module shutdown (shutdown step 6) or a failed start's unwind; no-op when null
	Flux_ParticleSystem* GetParticleSystem();                           // null until EnableParticles succeeds, and after DisableParticles

	// Render subgraph node bodies (registered by ZenithRuntime's engine stage nodes)
	void BuildGraph(const Flux_RenderWorld& xWorld, Zenith_JobContext& xContext);     // Flux.BuildGraph
	void RecordPartition(uint32 uPartition, Zenith_JobContext& xContext);           // one Flux.Record sub-job
	uint32 GetPartitionCount() const;                                  // after BuildGraph
	void Submit(Zenith_JobContext& xContext);                                         // Flux.Submit
	void Present(Zenith_JobContext& xContext);                                        // Flux.Present

	// Queries
	Flux_PSOPrecacher& GetPrecacher();                                  // Zenith2/Docs/Design/RPROD/PSO.md "Precache API"
	const Flux_RendererStats& GetStats() const;
	const Flux_QualitySettings& GetActiveQuality() const;
	Flux_QualityTier GetActiveTier() const;
	Flux_DeviceLossState GetDeviceLossState() const;                     // RHI's polled state (row R19)
private:
	Flux_Renderer(const Flux_RendererDesc& xDesc);
};
```

`RunPrologue` and `GetDeviceLossState` are RHI's two forwarding members (RHI row R19, `Zenith2/Docs/Design/RHI.md → "Render-idle points"`): `RunPrologue` calls `Flux_DeviceLossHandler::RunPrologue()` and then, while the handler's state is `FLUX_DEVICE_LOSS_STATE_NORMAL`, `Flux_SwapchainManager::ProcessPendingRecreations()`; `GetDeviceLossState` returns the handler's polled state (`Flux_DeviceLossState`, `ZenithFlux/Flux_QueueValues.h`, RHI's). The composition root's prologue step 4 and `RunFrame`'s render condition call them (`Zenith2/Docs/Architecture/CompositionRoot.md`, X128 applied).

`EnableEnvironment` and `EnableUI` create ENV's `Flux_EnvironmentRenderer` (`Zenith2/Docs/Design/ENV.md` → "ZenithFlux ENV files") and UI's `Flux_UIRenderer` (`Zenith2/Docs/Design/UI/ServicesAPI.md`) on the main thread during module start, before the first frame, keep each as a `Zenith_Owned` member destroyed before the GEOM managers, and return the borrowed pointer; a second call returns `FLUX_RENDERER_ERROR_ALREADY_ENABLED`, a creation failure the callee's mapped error. RPROD declares both members and the two owned members; ENV's and UI's implementation tickets define the bodies in `Internal/Renderer/Flux_Renderer.cpp`, each blocked by the renderer API ticket that declares them. `Flux.BuildGraph` step 7 sets `Flux_FrameShared::m_pxEnvironment` to the environment renderer and `Flux_FrameShared::m_pxUIFrame` to the UI renderer's `GetFrame()` before any feature function runs, each null when its module did not enable it ([Z2-N-REND-995]; cross-doc items X209 and X230, applied); the UI half of that assignment lands with UI's `EnableUI` body, because `GetFrame` is UI's.

`EnableParticles`, `DisableParticles` and `GetParticleSystem` are the particle counterpart of `EnableEnvironment` (cross-area row VX2; `Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem"`, "Flux_ParticleSystem internals", "Module shutdown order"). `EnableParticles` runs on the main thread at startup step 15 from VFX's module start, before the first frame, creates VFX's `Flux_ParticleSystem` as a `Zenith_Owned` member destroyed in the renderer's shutdown before the GPU scene whose pool it created, and returns the borrowed pointer; it returns `FLUX_PARTICLE_ERROR_INVALID_DESC` for a desc outside its domain, `FLUX_PARTICLE_ERROR_ALREADY_ENABLED` on a second call, and a construction failure after unwinding its own steps with `GetParticleSystem()` null. `DisableParticles` runs on the main thread at a render-idle point (VFX's module shutdown, step 6 of the shutdown ownership order, or the unwind of a failed module start): it releases every effect handle of the instance table, frees every pool range, effect constants buffer and the quad index buffer through deferred deletion and destroys the system; it is a no-op when no system exists. RPROD declares the three members; VFX's enabling body ticket adds the owned member `Zenith_Owned<Flux_ParticleSystem> m_xParticles` and defines the bodies in `Internal/Renderer/Flux_Renderer.cpp` (ruling Q94 (a), as ENV's and UI's enabling tickets add their owned members), blocked by the renderer API ticket that declares the three members. While the system exists the renderer calls VFX's six internal functions at fixed points: `Flux_ParticleApplyRenderWorld` at `Flux.BuildGraph` step 4, `Flux_ParticleBuildFrame` at step 7c, `Flux_ParticleWriteFrameData` at step 11 after `FITS` or `EXTENDED`, `Flux_ParticleEndFrame(bSubmitted)` at step 13 on every path, `Flux_ParticleOnDeviceRecovered` in the renderer's device-loss recovery (after it rebuilds its own tables, `Zenith2/Docs/Design/RPROD.md → "Failure handling"`), and, from `ApplyConfigChanges` (prologue step 3), `Flux_ParticleApplyConfigChanges(*GetParticleSystem(), Flux_ReadParticleSystemDesc(xConfig))` when a changed key starts with `vfx.` or the active tier changed, adopted at the next `BuildGraph` like `Flux_QualitySettings` ("Quality tier selection" step 3). VFX's ZenithFlux API slice defines the six functions as no-ops (`Flux_ParticleBuildFrame` returns an empty frame), so whichever of VFX's `EnableParticles` body and RPROD's Stage 3 renderer work adds the calls first links; no call is made when `GetParticleSystem()` is null.

`GetProceduralGeometry` returns GEOM's `Flux_ProceduralGeometrySystem` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`), which the renderer owns (created after `Flux_MeshManager` in step 7 of "Create sequence"); a module registers its `Zenith_ProceduralGeometrySource` through it during module start, before the first frame (row GM22). The renderer registers GEOM's `Flux_RibbonSource` itself at construction.

Lock order: `Flux_Renderer` takes no lock of its own; the RHI-owned members it calls take `uLOCK_LEVEL_FLUX_PERSISTENT` (480) of `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"` (the lock ordering table), and no renderer call is made while a caller holds a lock of a higher level.

`Flux_RendererStats` carries the frame's view count, deferred views, pass count, culled and merged pass counts, transient memory statistics, PSO miss counters, pacing statistics and the compile time (every counter of `Zenith2/Docs/Design/RPROD/Diagnostics.md → "Counters"` in value form for tests).

### Create sequence

Each row lands with the stage named in its Stage column: the Stage 2 renderer creates rows 1 to 5, 7 and 8a, and each Stage 3 subsystem adds its own row to `Flux_Renderer::Create` (and its call sites in the frame nodes) in the diff that implements the subsystem, so no row is ever a stub or a shell.

| # | Step | Failure injection value | Stage |
|---|---|---|---|
| 1 | read and validate the RPROD keys of "Configuration keys" from the store | `0x52500001` | 2 |
| 2 | the quality state for the resolved tier ("Quality tier selection"): at Stage 2 the RPROD members' key defaults; from Stage 3 `Flux_ReadQualitySettings` over the resolved tier file | `0x52500002` | 2; 3 (tier files) |
| 3 | the graph caches (`Flux_GraphCaches::Create`) | `0x52500003` | 2 |
| 4 | the RHI-owned subsystems in RHI's order: frame lifetime (frame 0 prepared on the main thread), upload ring, readback pool, descriptor allocator, GPU scene, persistent resources, swapchain manager with the main window's swapchain (`Zenith2/Docs/Design/RHI.md → "Startup and shutdown"`, step 13); then the pipeline-cache store load | the RHI values | 2; 3 (pipeline-cache store, with the PSO table) |
| 5 | the shader library: every boot-group container ("Shader blob loading" of PSO.md) | `0x52500005` | 2 |
| 6a | the PSO table (`Flux_PSOTable::Create`) | `0x52500006` | 3 |
| 6b | the precache file (`Flux_ReadPrecacheFile`) and the precacher; the boot group is created before the first frame ([Z2-N-REND-202]); the global group starts after the first frame | `0x5250000E` | 3 |
| 7 | the GEOM-owned managers in GEOM's order (`Zenith2/Docs/Design/GEOM.md → "Construction and shutdown"`) | the GEOM values | 2 |
| 7b | LIGHT's lighting system (`Flux_LightingSystem::Create`, `Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`) with the resolved tier's `Flux_LightQualitySettings` | `0x5250000B` | 3 |
| 8a | the view registry (borrowing the lighting system of step 7b; null before Stage 3) | `0x52500008` | 2 |
| 8b | the culling system (bucket registry, draw-key table) | `0x5250000C` | 3 |
| 8c | the transparent collector | `0x5250000D` | 3 |
| 8d | DEVT's debug-overlay persistent resources: the 128 × 48 `R8_UNORM` font atlas from `aulDEBUG_FONT_GLYPHS` and its descriptor (`Zenith2/Docs/Design/DEVT/DebugDraw.md → "Debug font"`, row DV6); DEVT's debug-overlay ticket adds the row in its own diff | `0x5250000F` | 3 |
| 9 | the frame pacer | `0x52500009` | 3 |

The rendering memory consumer (RHI's `Flux_MemoryConsumer`) is not a Create step: the composition root registers it with the governor at startup step 13, so the governor stays outside the renderer's constructor injection. `EnableEnvironment`, `EnableUI` and `EnableParticles` ("Flux_Renderer header") are not Create steps either: a module's start calls them after `Create` has returned. Fault-injection values carry the library-distinct high 16 bits `0x5250` ('RP') per cross-doc item X67. Each step has a `Test_FluxRenderer_Create_FailsAt<Step>_Unwinds` case ([Z2-N-CONTRACT-341]), added with its row.

## Tool textures

<!-- elaboration -->
A tool host (the editor's font atlas, icons, thumbnails and viewport outputs, A3 cross-area row ED12 (a)) needs 2D textures created and filled from memory outside the asset system, some of which a view renders into. They are not `Flux_PersistentReadOnly` resources, which hand out no render-target view ([Z2-N-REND-547]):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ToolTexture.h
enum Flux_ToolTextureFlags : uint8
{
	FLUX_TOOL_TEXTURE_FLAG_NONE = 0,
	FLUX_TOOL_TEXTURE_FLAG_RENDER_TARGET = 1u << 0,   // a view may name it as a FLUX_VIEW_OUTPUT_TEXTURE output
};

struct Flux_ToolTextureDesc
{
	uint32 m_uWidth = 0;                             // 1 … 8,192
	uint32 m_uHeight = 0;                            // 1 … 8,192
	Zenith_RHIFormat m_eFormat = ZENITH_RHI_FORMAT_R8G8B8A8_UNORM;   // R8_UNORM, R8G8B8A8_UNORM, R8G8B8A8_UNORM_SRGB or R16G16B16A16_FLOAT
	uint8 m_uFlags = FLUX_TOOL_TEXTURE_FLAG_NONE;
	Zenith_ByteSpan m_xInitialPixels;                 // empty, or exactly width × height × texel bytes, row-major, no padding
	const char* m_szDebugName = nullptr;
};

struct Flux_ToolTextureHandle                         // a generational device handle; null after DestroyToolTexture
{
	Flux_TextureHandle m_xTexture;                    // what a view's Flux_ViewOutputs::m_xTexture names
	bool IsValid() const;
};

// Flux_Renderer members (MAIN_THREAD, between frames; tool hosts alone, never a Player path)
	Zenith_Result<Flux_ToolTextureHandle, Flux_RendererError> CreateToolTexture(const Flux_ToolTextureDesc& xDesc);
	Zenith_Result<void, Flux_RendererError> UpdateToolTexture(Flux_ToolTextureHandle xTexture, Zenith_ByteSpan xPixels);
	void DestroyToolTexture(Flux_ToolTextureHandle xTexture);
	Zenith_RHIDescriptorHandle GetToolTextureSRV(Flux_ToolTextureHandle xTexture) const;
```

- `CreateToolTexture` creates the device texture with sampled usage, and render-target usage when the flag is set, and one bindless SRV descriptor allocated once. When pixels are given it queues their upload through the renderer's upload ring; the texture samples as transparent black until the upload completes. A desc outside its domain is `FLUX_RENDERER_ERROR_CONFIGURATION`, and a device refusal is `FLUX_RENDERER_ERROR_DEVICE`. At most 4,096 tool textures are live (`FLUX_RENDERER_ERROR_OUT_OF_MEMORY` beyond).
- `UpdateToolTexture` replaces the whole contents through the upload ring. It is refused for a render-target texture or a span of the wrong size (`FLUX_RENDERER_ERROR_CONFIGURATION`) and changes nothing then.
- `DestroyToolTexture` releases the texture and its descriptor through deferred deletion with the last-use values of the current frame on every queue, so a frame in flight that samples it finishes first ([Z2-N-REND-488]). A stale or null handle is counted in `Flux.ToolTextures.Stale` and ignored.
- `GetToolTextureSRV` returns the descriptor, or the null descriptor for a stale handle.
- A view whose output is a tool texture imports it into the graph with `ImportTexture` like any `FLUX_VIEW_OUTPUT_TEXTURE` output, and `Flux_AddPresent` marks it an output ("Present pass realisation").
- Device-loss recovery recreates every live tool texture at its size and format with its contents lost, and keeps every handle valid; the tool redraws them.

## Shader reload entry

<!-- elaboration -->
The editor's reload watcher applies the shader compiler's reload journal ([Z2-N-REND-128], [Z2-N-REND-129]) through one renderer entry (A3 cross-area row ED12 (e)):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h additions
struct Flux_ShaderReloadRequest
{
	Zenith_RHIShaderProgramID m_xProgram;             // the program the journal record names
	Zenith_StringView m_strContainerPath;             // the rebuilt .zshader, host path
	uint64 m_ulContainerHash = 0;                    // the record's container hash; a file with another hash is refused
};

struct Flux_ShaderReloadStatus
{
	uint32 m_uPendingReplacements = 0;               // the program's entries whose replacement PSO is not yet READY
	uint64 m_ulLastSwapFrame = 0;                    // the frame index of the program's latest replacement swap; 0: none
};

// Flux_Renderer members (MAIN_THREAD, between frames; development builds alone)
	Zenith_Result<void, Flux_ShaderError> ReloadShaderContainer(const Flux_ShaderReloadRequest& xRequest);
	Flux_ShaderReloadStatus GetShaderReloadStatus(Zenith_RHIShaderProgramID xProgram) const;
```

`ReloadShaderContainer` reads the container through `Flux_ShaderLibrary`'s loader, refusing it with the library's header errors (or `FLUX_SHADER_ERROR_IO` for a hash mismatch or an unreadable file) and changing nothing then. On success it replaces the program's container in the library: the old container stays mapped until every PSO made from it has retired. It then calls `Flux_PSOTable::OnShaderReload(program)`, which moves the program's entries by the reload rows of "PSO table and states" in `Zenith2/Docs/Design/RPROD/PSO.md` (`READY` keeps its handle with a replacement `PENDING` beside it, and `FAILED` becomes `ABSENT`) and queues the replacements on the asynchronous creation schedule. Each swap of a replacement into its entry increments the cumulative counter `Flux.PSO.ReloadSwaps` and sets the program's last swap frame. `GetShaderReloadStatus` lets the editor find the end of a reload: the frame after the program's last swap, once no replacement is pending. In Shipping the entry returns `FLUX_SHADER_ERROR_IO` and loads nothing.

## Feature systems enabled after Create

<!-- elaboration -->
Three more module-owned GPU systems follow the ENABLE-FEATURES pattern of `EnableEnvironment`, `EnableUI` and `EnableParticles` ("Flux_Renderer header"): RPROD declares the members, the `Flux_FrameShared` member and the call sites; the owning area writes the bodies and adds the owned `Zenith_Owned` member in `Internal/Renderer/Flux_Renderer.cpp` (ruling Q94 (a)), in a ticket blocked by the RPROD ticket that declares them (rows TD1, CN8 (a), AV1 (a)).

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h additions
class Flux_Sprite2DRenderer;                   // TWOD's, ZenithFlux/Flux_Sprite2D.h
struct Flux_Sprite2DRendererDesc;
class Flux_VideoFrameSource;                   // CINE's, ZenithFlux/Flux_Video.h
class Flux_VideoSystem;
class Flux_VirtualTextureSystem;               // ADVGFX's, ZenithFlux/Flux_VirtualTexture.h
struct Flux_VirtualTextureDesc;
enum Flux_VirtualTextureError : uint8;        // opaque declaration; same underlying type

// MAIN_THREAD before the first frame: module start (startup step 15), or startup step 13 right after Create for VT
	Zenith_Result<Flux_Sprite2DRenderer*, Flux_RendererError> Enable2D(const Flux_Sprite2DRendererDesc& xDesc);   // Stage 3; at most once
	Zenith_Result<void, Flux_RendererError> EnableVideo(Flux_VideoFrameSource& xSource);                          // Stage 4; at most once
	void DisableVideo();                                                // render-idle point (module shutdown, a failed start's unwind); no-op when not enabled
	Flux_VideoSystem* GetVideoSystem();                   // null until EnableVideo succeeds, and after DisableVideo
	Zenith_Result<Flux_VirtualTextureSystem*, Flux_VirtualTextureError> EnableVirtualTexturing(const Flux_VirtualTextureDesc& xDesc);   // Stage 4; at most once
	Flux_VirtualTextureSystem* GetVirtualTextureSystem(); // null until EnableVirtualTexturing succeeds
```

| System | Enabled by | `Flux_FrameShared` member (set at `Flux.BuildGraph` step 7 before any feature function; null when not enabled) | Calls RPROD's frame nodes make while the system exists | Device-loss recovery |
|---|---|---|---|---|
| 2D, `Flux_Sprite2DRenderer` (`Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"`) | Zenith2D's module start step 7 | `const Flux_Sprite2DFrame* m_px2DFrame` (the renderer's `GetFrame()`) | `PrepareFrame(xWorld, xRing)` at step 4 after the GPU scene's `ApplyDeltas`; `WriteFrame(xRing)` at step 11 after `FITS` or `EXTENDED`; `EndFrame(bSubmitted)` at step 13 on every path out of the node | `OnDeviceRecovered()` once, after the renderer has rebuilt its own tables and before the first recovered frame (row TwB15) |
| video, `Flux_VideoSystem` (`Zenith2/Docs/Design/CINE/Video.md → "GPU side"`) | the Cinematics module's start step 6 | `Flux_VideoSystem* m_pxVideo` | the prologue row 2b `VideoConvert`, which CINE's video-system ticket records at step 8 after `StreamingUpload`; the epilogue row 6b (`Zenith2/Docs/Design/RPROD/Passes.md → "Common epilogue rows 6b and 6c"`) | CINE's (the surfaces are recreated by the video system) |
| VT, `Flux_VirtualTextureSystem` (`Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md → "Frame-level flow"`) | ZenithRuntime at startup step 13 right after `Create`, VT enabled or not (row AV2) | none: the system imports its pools into `m_xVTPhysical` and `m_xVTPageTable` | `BeginFrame(xContext)` at a step 3c after GEOM's step 3 and before step 4; `ImportResources(xGraph, xShared)` at step 7; `WriteFrameData()` at step 11 after `FITS` or `EXTENDED`; `EndFrame(bSubmitted)` at step 13 on every path, the frame's last VT call; `OnCameraCut(xViewID)` from view-list step 4 for each listed view whose `m_bCameraCut` is set | `OnDeviceLost()` before the renderer's own rebuild, `OnDeviceRecovered()` after it |

- A second call of an enable member returns `FLUX_RENDERER_ERROR_ALREADY_ENABLED` (`EnableVirtualTexturing`: ADVGFX's `FLUX_VT_ERROR_ALREADY_ENABLED`); a creation failure returns the callee's mapped error after unwinding the callee's steps, with the getter null.
- The owned members are destroyed in the renderer's destructor before the GEOM managers, after `DisableVideo` has run for video.
- The 2D renderer's three frame calls are added by TWOD's renderer ticket with its body (`Zenith2/Docs/Design/TWOD/Rendering.md → "View extensions and view kinds"`); the recovery call is RPROD's. The VT calls and the camera-cut call are RPROD's, made through ADVGFX's API slice, whose frame members are no-ops until ADVGFX's system lands (the VFX pattern of "Flux_Renderer header").
- `m_px2DFrame` and `m_pxVideo` are members of Rendering's `Flux_FrameShared`; cross-doc item X454 (a) adds them and names them in [Z2-N-REND-995]'s list of routes.

## Tool graph entry

<!-- elaboration -->
A tool host runs a graph of its own outside the frame loop's render subgraph through one renderer entry (ADVGFX's path tracer, `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Execution substrate"`, row AV17). A tool frame is an ordinary frame of RHI's frame lifetime that has no view and presents nothing.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h additions (Stage 5)
enum Flux_ToolGraphError : uint8
{
	FLUX_TOOL_GRAPH_ERROR_FRAME_IN_PROGRESS = 1,      // called while a render frame is in progress, inside an executor iteration, or from a tool callback
	FLUX_TOOL_GRAPH_ERROR_DEVICE_LOST = 2,            // the device-loss state was not NORMAL at entry, or the device was lost before the frame retired
	FLUX_TOOL_GRAPH_ERROR_CALLBACK_FAILED = 3,        // the build callback returned an error; nothing was submitted
	FLUX_TOOL_GRAPH_ERROR_COMPILE = 4,                // the tool graph failed to compile; every diagnostic is logged; nothing was submitted
	FLUX_TOOL_GRAPH_ERROR_UPLOAD_DEFERRED = 5,        // ExtendReservation returned neither FITS nor EXTENDED; nothing was submitted
	FLUX_TOOL_GRAPH_ERROR_READBACK_FAILED = 6,        // a readback the callback declared finished FAILED
	FLUX_TOOL_GRAPH_ERROR_TIMEOUT = 7,                // the frame's timeline values were not reached within uFLUX_TOOL_GRAPH_WAIT_MS with the device healthy
	FLUX_TOOL_GRAPH_ERROR_SLOTS_EXHAUSTED = 8,        // AcquireToolMaterialSlot: uFLUX_MAX_TOOL_MATERIAL_SLOTS live, or the GPU scene's material-slot table full
	FLUX_TOOL_GRAPH_ERROR_MATERIAL_NOT_RESIDENT = 9,  // AcquireToolMaterialSlot: the material is not resident
	FLUX_TOOL_GRAPH_ERROR_VT_NOT_SETTLED = 10,        // SettleToolFrames ran uMaxFrames frames and the VT system did not report settled
	FLUX_TOOL_GRAPH_ERROR_INVALID_ARGUMENT = 11,      // a null callback, uMaxFrames outside 1 … uFLUX_MAX_TOOL_SETTLE_FRAMES
};

using Flux_ToolGraphBuildFn = Zenith_Result<void, Flux_ToolGraphError> (*)(Flux_RenderGraph& xGraph, Flux_FrameShared& xShared, void* pUserData);

inline constexpr uint32 uFLUX_TOOL_GRAPH_WAIT_MS = 30000;
inline constexpr uint32 uFLUX_MAX_TOOL_MATERIAL_SLOTS = 256;
inline constexpr uint32 uFLUX_MAX_TOOL_SETTLE_FRAMES = 600;

// Flux_Renderer members: MAIN_THREAD (the thread that created the job system), outside every iteration of GRAPH's executor
	Zenith_Result<void, Flux_ToolGraphError> RunToolGraph(Flux_ToolGraphBuildFn pfnBuild, void* pUserData);
	Zenith_Result<uint32, Flux_ToolGraphError> SettleToolFrames(uint32 uMaxFrames);   // frames run
	Zenith_Result<uint32, Flux_ToolGraphError> AcquireToolMaterialSlot(const Flux_MaterialResource& xMaterial);
	void ReleaseToolMaterialSlot(uint32 uSlot);
```

### Tool frames in flight

- A render frame is **in progress** from the start of its `Flux.BuildGraph` node to the end of its `Flux.Present` node. `RunToolGraph` and `SettleToolFrames` return `FLUX_TOOL_GRAPH_ERROR_FRAME_IN_PROGRESS` when a render frame is in progress, when they are called inside an iteration of GRAPH's executor, or when they are called from a tool callback. Between two iterations of the frame loop (after `RunFrame` returned, [Z2-N-ARCH-386]) and in a test host that runs no frame loop, no render frame is in progress.
- Frames already submitted may still execute on the GPU; the entry does not wait for them except through the slot rule.
- **Slot rule.** A tool frame takes the next slot of `Flux_FrameLifetime`'s ring of `r.framesInFlight` slots exactly as a render frame does: it waits on that slot's last timeline values, so the frame that used the slot `r.framesInFlight` frames earlier has retired, and it consumes the next frame index. It acquires no swapchain image, runs no pacer and presents nothing. Its outcome is `SUBMITTED`, `DEFERRED` or `FAILED` like any frame's, and the next render frame takes the following slot.

### Tool frame thread and job context

The caller's affinity is `MAIN_THREAD`; the frame runs where a render frame runs. `RunToolGraph` executes three phases:

| Phase | Where | Work |
|---|---|---|
| A | `Zenith_JobSystem::RunRoot` on the caller, whose root job continues with `JOB_AFFINITY_RENDER_SUBMIT`; the continuation holds the `Zenith_JobContext` the job system supplies | the slot rule; renderer steps 2 and 3 (graph caches, GEOM finalisation); VT `BeginFrame(xContext)` (step 3c); graph construction and the step-7 imports (tables, pools, VT `ImportResources`); prologue row 1 `Flux_AddGPUSceneUpdate`, so a tool material slot staged before the call is resident; the callback; step 10 compile; step 11 (descriptor patching, `ExtendReservation`, VT `WriteFrameData()`); step 13 (VT `EndFrame(bSubmitted)`); recording through `ParallelFor` over the partitions on `JOB_AFFINITY_ANY`, as `Flux.Record` records; RHI's submission |
| B | the caller | waits for the frame's timeline values in 1 ms polls of the timeline and of `GetDeviceLossState()`, at most `uFLUX_TOOL_GRAPH_WAIT_MS` |
| C | a second `RunRoot` whose root job continues with `JOB_AFFINITY_RENDER_SUBMIT` | the slot's retirement as `PrepareNextFrame` runs it for a render frame: deferred deletion, the retirement queue and readback resolution; then the caller checks every readback the callback declared |

So VT's `RENDER_SUBMIT_THREAD` members and `BeginFrame`'s `ParallelFor` run with a context whatever thread called the entry. The tool frame consumes no render world and runs no view list, no other prologue row, no LIGHT, VFX, 2D or video call and no epilogue.

### Tool graph errors

| Error | Raised when | What was done |
|---|---|---|
| `FRAME_IN_PROGRESS` | "Tool frames in flight" | nothing |
| `DEVICE_LOST` | the state was not `NORMAL` at entry, or became `LOST` before phase C | nothing at entry; otherwise the frame's slot retires through RHI's recovery and its readbacks fail ([Z2-N-REND-501]) |
| `CALLBACK_FAILED` | the callback returned an error | the graph is discarded; the VT calls of step 13 run with `bSubmitted` false; the frame is `FAILED` |
| `COMPILE` | `Compile()` returned an error | every diagnostic logged; as `CALLBACK_FAILED` (never a `ZENITH_CHECK`: a tool graph is the tool's input) |
| `UPLOAD_DEFERRED` | `ExtendReservation` deferred or failed | the frame is `DEFERRED` or `FAILED` and writes no frame data, as RHI's frame deferral |
| `READBACK_FAILED` | a declared readback finished `FAILED` (for example `FLUX_READBACK_ERROR_POOL_FULL`) | the frame retired; the other readbacks keep their results |
| `TIMEOUT` | phase B reached `uFLUX_TOOL_GRAPH_WAIT_MS` with the device reported healthy | the frame stays outstanding and retires at the next frame's slot wait; logged once in `flux.toolgraph` |
| `SLOTS_EXHAUSTED`, `MATERIAL_NOT_RESIDENT` | "Tool material slots" | no slot allocated |
| `VT_NOT_SETTLED` | "Virtual texture settlement in tool frames" | every frame run retired normally |
| `INVALID_ARGUMENT` | a null `pfnBuild`, or `uMaxFrames` outside 1 … `uFLUX_MAX_TOOL_SETTLE_FRAMES` | nothing |

`Flux.ToolGraph.Frames` counts the tool frames run.

### Virtual texture settlement in tool frames

RPROD pumps settlement; the tool host asks for it. `SettleToolFrames(uMaxFrames)` runs tool frames whose graph declares nothing but the tool frame's own work until `GetVirtualTextureSystem()` is null or the system's `IsSettled()` is true, and returns the number of frames run (0 when already settled). Each such frame runs VT's `BeginFrame` to `EndFrame`, so reads, decodes, uploads and page-table updates advance one frame per tool frame. A tool frame writes no feedback, so only VT's `full` residency mode settles this way: the path tracer sets `full`, calls `SettleToolFrames(uFLUX_MAX_TOOL_SETTLE_FRAMES)` before its first batch and maps `VT_NOT_SETTLED` to its own error. A device loss during the loop returns `DEVICE_LOST`.

### Tool material slots

- `AcquireToolMaterialSlot` (MAIN_THREAD, outside every tool and render frame) allocates one material-slot record in the GPU scene through RHI's material-slot allocator for a resident material outside any render-world instance, and stages the record exactly as an instance's material is staged, so it is resident from the next frame's `GPUSceneScatter` and `EvaluateMaterial(xIn, uMaterialSlot)` reads the constants a raster draw reads. It returns the slot index; at most `uFLUX_MAX_TOOL_MATERIAL_SLOTS` are live.
- `ReleaseToolMaterialSlot` frees the record through deferred deletion with the last-use values of the latest frame. An unknown or stale slot is ignored and counted in `Flux.ToolGraph.StaleSlotReleases`.
- Device loss: the renderer's recovery releases every tool slot (the full-state GPU-scene rebuild re-creates only the render world's material slots) and advances the tool-slot epoch, so a release of a slot acquired before the loss is stale. A tool host whose slots were released reacquires them; ADVGFX's tracer is unusable after a loss.

## Device recreation request

<!-- elaboration -->
<!-- assumption: pending core-doc (X443) -->
A Stage 7 caller asks the renderer for a device recreation when the device it has is not the one an OpenXR runtime requires (`Zenith2/Docs/Design/SCOPE/XrRuntime.md → "Loss recovery and shutdown"`, row SP16):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h additions (Stage 7)
enum Flux_DeviceRecreationReason : uint8 { FLUX_DEVICE_RECREATION_REASON_XR = 1 };
// Flux_Renderer member: MAIN_THREAD, a render-idle point of the frame prologue
	bool RequestDeviceRecreation(Flux_DeviceRecreationReason eReason);
```

`RequestDeviceRecreation` is RHI's third forwarding member beside `RunPrologue` and `GetDeviceLossState`: RPROD declares it and the enumeration, and RHI's recovery ticket defines it over `Flux_DeviceLossHandler`.

| Rule | Specification |
|---|---|
| refusal | returns false and does nothing when the device-loss state is not `FLUX_DEVICE_LOSS_STATE_NORMAL`; the caller retries at the next prologue |
| acceptance | returns true; at its next prologue call the handler waits for the device to go idle and leaves `NORMAL` for `RECOVERING`, skipping `LOST` and `REPORTING` |
| worlds | on entry the runtime pauses every world at its next tick boundary exactly as for a loss, and recovery step 5 unpauses them ([Z2-N-ARCH-880]) |
| policy | `rhi.deviceLossPolicy` does not apply; the request is not counted as a loss for the 60-second second-loss rule; a failure of the recreation takes `EXITING`; RHI counts `RHI.Device.RequestedRecreations` instead of `RHI.Device.Recoveries` |
| adapter | the desc's interop request decides the instance and the adapter; the same-adapter clause is lifted for a requested recreation (cross-doc item X443) |
| interop provider | `RECOVERING` calls the provider's `OnDeviceDestroying` before the device is destroyed, `OnDeviceDestroyed` after it, and `FillInteropRequest` and `OnDeviceCreated` around the new device, for loss and request alike |
| renderer | the renderer's own recovery runs as for a loss: the PSO table, the view histories (invalid), the bucket and draw-key tables, the tool textures and slots, and the feature systems of "Feature systems enabled after Create" |
| D3D12 | the renderer compares no adapter: ZenithXR skips the request when the existing device already has the runtime's adapter LUID and feature level |
<!-- /assumption -->

## Stereo frame source

<!-- elaboration -->
The OpenXR frame cycle drives the stereo views through one interface ZenithFlux declares and ZenithXR implements, so the callback points downward ([Z2-N-ARCH-078]; row SP1 (a), answering A5 row RPB21). `Zenith2/Engine/ZenithFlux/Flux_StereoFrameSource.h` (Stage 7) declares `Flux_XrViewPoses`, `Flux_StereoSubmitInfo` and `Flux_StereoFrameSource` exactly as `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md → "Interfaces of lower libraries"` lists them; `Flux_XrViewPoses` moves there from `Internal/Views/Flux_StereoViews.h`, widened with the depth images, the per-eye extents, the tangent form of `XrFovf` and the predicted display time. `Flux_Renderer` gains `void SetStereoFrameSource(Flux_StereoFrameSource* pxSource)` (MAIN_THREAD, a render-idle point; null clears).

| Node | Call | Rule |
|---|---|---|
| `Flux.BuildGraph`, view-list step 4 | `AcquireStereoFrame(ulFrameIndex, xRigWorldFromOrigin, xPoses)` once per frame, for the first `VIEW_KIND_STEREO` request in list order, with `xRigWorldFromOrigin` the inverse of the request's `m_xCamera.m_xView` | true: the request expands into its two eye views; false: the request renders as a mono view of `VIEW_KIND_MAIN`'s feature mask and the main swapchain output, with its own camera (the fallback of the Stage 7 stereo integration); no source set: the request is rejected with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE` as before Stage 7 |
| `Flux.Submit` | `BeforeSubmit(ulFrameIndex)` before the frame's eye-image copies are submitted | false: the copies are not submitted ("Stereo eye copies and the desktop mirror" of `Zenith2/Docs/Design/RPROD/Passes.md`) |
| `Flux.Present` | `AfterSubmit(xInfo)` after RHI's present, with `m_bRendered` true only when the frame expanded and the copies were submitted | — |
| `Flux.Present` | `PacesFrames()` read once per frame | true: the pacer runs in `FLUX_PACING_MODE_EXTERNAL` ("External pacing") |

A second `VIEW_KIND_STEREO` request in one frame is refused with `FLUX_VIEW_ERROR_INVALID_REQUEST`.

## Main-view capture

<!-- elaboration -->
A development tool renders the main view into an offscreen output of a requested size and reads it back after a named epilogue pass (CINE's capture, `Zenith2/Docs/Design/CINE/Video.md → "Capture algorithm"` step 4; rows CN8 (c), CnB20 (a)):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h additions (Stage 4; development builds)
enum Flux_CapturePoint : uint8
{
	FLUX_CAPTURE_POINT_VIDEO_COMPOSITE = 1,   // after epilogue row 6b, or where it would run
	FLUX_CAPTURE_POINT_UI_COMPOSITE = 2,      // after epilogue row 7, or where it would run
};
struct Flux_MainViewCaptureDesc
{
	uint32 m_uWidth = 0;                     // 16 … 3,840
	uint32 m_uHeight = 0;                    // 16 … 2,160
	Flux_CapturePoint m_ePoint = FLUX_CAPTURE_POINT_VIDEO_COMPOSITE;
};
class Flux_MainViewCapture              // reached through Flux_FrameShared::m_pxMainViewCapture on the build node
{
public:
	Zenith_Result<Flux_ReadbackHandle, Flux_ReadbackError> RequestReadback();   // at most once per frame
};
// Flux_Renderer members (MAIN_THREAD, between frames)
	Zenith_Result<void, Flux_RendererError> BeginMainViewCapture(const Flux_MainViewCaptureDesc& xDesc);
	void EndMainViewCapture();
```

| Rule | Specification |
|---|---|
| begin | a desc outside its domain, a second begin, or a Shipping build: `FLUX_RENDERER_ERROR_CONFIGURATION`, nothing changed |
| output | from the next frame until `EndMainViewCapture`, the main view's output is a renderer-owned `R8G8B8A8_SRGB` texture of the capture size (the SDR output transform), and its render extent follows the capture size; its histories are invalidated once at the begin and once at the end ([Z2-N-REND-599]) |
| window | `Flux_AddPresent` copies the capture output letterboxed into the main swapchain, so the window keeps showing the frame |
| request | `Flux_FrameShared::m_pxMainViewCapture` is non-null exactly while a capture is active; `RequestReadback` on the build node declares, after the named pass of the main view's epilogue, `AddReadback("Flux.MainViewCapture", output, whole texture)` and returns its handle; a second request in one frame returns `FLUX_READBACK_ERROR_POOL_FULL` |
| memory | the readback comes from the pool of `Z2-B-RHI-011` ([Z2-N-REND-499]); its failures are the readback pool's (`FRAME_DEFERRED`, `FRAME_FAILED`, device loss) |
| caller | CINE's frame source implementation calls it from `Flux_VideoSystem`'s frame work on the build node, so the capture's render channel stays on the render-submit thread |

`m_pxMainViewCapture` is a member of Rendering's `Flux_FrameShared` (cross-doc item X454 (a)).

## Configuration keys

`Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterFluxConfigKeys(Zenith_ConfigSchema& xSchema, Zenith_BuildPlatform ePlatform)` (`Zenith2/Engine/ZenithFlux/Flux_Config.h`) registers, with CORE's `Zenith_ConfigKeyDesc` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Configuration store"`), the RPROD keys below; the composition root calls it in engine-library order before the schema freeze (cross-doc item X63). RHI's keys (`rhi.*`, `r.framesInFlight`) and GEOM's are registered by their owners.

<!-- elaboration -->
| Key | Type | Default | Layers | Restart | Meaning |
|---|---|---|---|---|---|
| `r.shadingPath` | ENUM {forwardPlus, forward, deferred, visibilityBuffer} | forwardPlus | every | no | the desktop path of main, editor and texture views ([Z2-N-REND-692]); android accepts forwardPlus alone |
| `r.graph.aliasTransients`, `…poisonTransients`, `…asyncCompute`, `…splitBarriers`, `…mergePasses`, `…parallelRecord`, `…timestamps` | BOOL | "Flux_GraphSettings header" of RenderGraphFrontEnd.md | every | no | the graph toggles ([Z2-N-REND-290]) |
| `r.graph.arenaBytes` | INT | 33,554,432 | engine, platform, game | yes | the graph arena's block size |
| `r.depth.precisionToleranceMetres` | FLOAT (0, 0.4767) | 0.25 | engine (generated from `Z2-B-RPROD-029` once adopted) | yes | the D24 precision tolerance ([Z2-N-REND-592]) |
| `r.pacing.mode` | ENUM {off, interval} | interval | every | no | the pacer mode ("Frame pacer") |
| `r.pacing.targetFps` | INT [15, 240] | 60 | every | no | the frame-rate target |
| `r.pacing.refreshHzFallback` | INT [24, 240] | 60 | platform | no | the refresh rate when no present timing reports one |
| `r.pacing.presentMarginUs` | INT [0, 8000] | 1,500 | platform | no | the lead time of a timed present |
| `r.pso.creationJobs` | INT [1, 16] | 4 | platform | yes | parallel PSO creation jobs per group ([Z2-N-REND-202]) |
| `r.pso.missFallback` | ENUM {auto, skip, magenta} | auto | command line (development builds) | no | `auto` follows the build type; the other values let a development run preview the shipping behaviour |
| `render.quality.*` | the RPROD keys of "Quality settings" | the highest tier's values | engine, platform, game, quality tier, user, command line | per key | the tier content RPROD owns ([Z2-N-REND-601]) |
| LIGHT's keys | LIGHT's (`Zenith2/Docs/Design/LIGHT.md → "Configuration keys"`) | LIGHT's | LIGHT's | LIGHT's | registered by `Flux_RegisterLightConfigKeys`, which `Zenith_RegisterFluxConfigKeys` calls after RPROD's keys; read into `Flux_QualitySettings::m_xLight` |

Console commands without configuration (`r.graph.dump <N>`, `r.debug.hiz`, `r.debug.culling`, `r.pso.dumpMisses`) are listed in `Zenith2/Docs/Design/RPROD/Diagnostics.md → "Console commands and debug UI"`.

## Render world consumption

`Flux_RenderWorld` and its writer are Rendering's declarations (`Zenith2/Docs/Rendering.md → "Purpose and scope"`, heading "The render world hand-off"), and `Architecture/CPUFrameGraph.md` fixes when they are produced. `BuildGraph` consumes frame k−1's sealed world ([Z2-N-REND-007]) in this order, sections in ascending world-tag order and records in their merged order ([Z2-N-REND-008]):

| Span | Consumer | Owner of the consumer |
|---|---|---|
| `m_axProceduralInstances` | `Flux_ProceduralApplyRecords` (admission, sizing, the regeneration records), before the GPU scene applies the deltas, so a new procedural instance's pool slot and stream-set resolve when its delta is applied | GEOM (`Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"`) |
| `m_axInstanceDeltas`, `m_axLightDeltas`, `m_axHistoryResets` | `Flux_GPUScene::ApplyDeltas` (slots, scatter staging, history resets, [Z2-N-REND-009]); the instance delta is `Flux_RenderInstanceDelta` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderInstanceDelta layout"`) | RHI (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`) |
| the mesh and material references of instance deltas | GEOM's managers finalise loads before the deltas apply (`Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`) | GEOM |
| `m_axSkinnedPoses` | `Flux_SkinningSystem` palettes and dispatch records | GEOM |
| `m_axEmitterCommands`, `m_axParticleColliders` | `Flux_ParticleApplyRenderWorld` (commands into the instance table), when the particle system exists | VFX (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Flux_ParticleSystem internals"`; row VX2) |
| `m_axDecals` | the frame decal list | GEOM (`Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"`) |
| `m_axViewRequests` | `Flux_BuildViewList` | RPROD (`Zenith2/Docs/Design/RPROD/Views.md → "View list construction"`) |
| `m_xEnvironment` | `Flux_LightingSystem::ApplyRenderWorld` (the lighting environment, the sun and moon slots), ENV's GPU side (the environment blocks) | LIGHT, ENV ("Render environment" below) |
| ENV's records (terrain, water bodies, per-view environment records) | `Flux_EnvironmentRenderer::ApplyRecords` | ENV (`Zenith2/Docs/Design/ENV.md → "Render-world records"`) |
| `m_axUILists` | `Flux_AddUIComposite` | UI (pending) |
| `m_axSprite2DLists`, `m_axTilemapChunks`, `m_axTilesetRecords` | `Flux_Sprite2DRenderer::PrepareFrame`, when 2D is enabled | TWOD (`Zenith2/Docs/Design/TWOD/Rendering.md → "Frame data path"`) |
| `m_axVideoDraws`, `m_axScreenOverlays` (Stage 4, pending X292 (c)) | `Flux_VideoSystem`'s player table and `Flux_AddVideoComposite`, when video is enabled | CINE (`Zenith2/Docs/Design/CINE/Formats.md → "Render-world records"`) |
| `m_axDebugDraws` | `Flux_AddDebugOverlay` | DEVT (pending) |
| `m_ulInputSequence`, `m_ulInputSampleTimeUs` | the pacer's latency record ("Input-to-photon attribution") | RPROD |

A world with `m_bFullState` (`Zenith2/Docs/Rendering.md → "Render world resync"`, [Z2-N-REND-015]) is consumed before any other span of the frame: `Flux_GPUScene` discards its CPU-side slot state and re-allocates every slot from the full-state records (RHI's step 4 of that table), and RPROD then rebuilds the bucket registry's reference counts and the draw-key table from the re-created material slots, so the draw keys equal those of a delta replay from the first frame. Device-loss recovery reaches the same path through `Flux_RenderWorldRing::RequestFullExtraction` ([Z2-N-REND-987]); this replaces the resync flag of reconciliation row RP15, which Rendering now carries as `m_bFullState`.

## Render environment

`Flux_RenderEnvironment` is the per-world environment of `Flux_RenderWorldSection::m_xEnvironment`, declared in `Flux_RenderWorld.h` (RPROD's diff lands that header, "RPROD public root headers" of the entry document). Its model is the orchestrator's ruling on cross-doc item X125: one `SetEnvironment` per world per frame, as [Z2-N-REND-008] states, with exactly one writer, the engine environment extractor `Render.ExtractEnvironment` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Environment extractor"`); ENV supplies the blocks that extractor copies, and the sun and moon have explicit slots.

<!-- assumption: pending LIGHT (RP25) -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderWorld.h (the environment)
#include "ZenithFlux/Flux_LightSettings.h"     // LIGHT's Flux_RenderLightingEnvironment
#include "ZenithFlux/Flux_Lights.h"            // LIGHT's Flux_RenderLightDelta
#include "ZenithFlux/Flux_Environment.h"       // ENV's Flux_EnvironmentBlocks (Zenith2/Docs/Design/ENV.md "Render-world records")

/// @contract value Flux_RenderEnvironment
/// Ownership: Plain value inside a Flux_RenderWorldSection; copied from simulation by the engine environment extractor; the
///   atmosphere block's local-fog-volume span points into m_xBlocks of the same section (set by SetEnvironment).
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: With the render-world slot.
/// Units: As LIGHT's and ENV's member types.
/// Errors: A second SetEnvironment for the world in one frame returns FLUX_RENDER_WORLD_ERROR_ENVIRONMENT_SET_TWICE and
///   leaves the first value ([Z2-N-REND-008]).
/// Version: since Z2 Stage 3 (the sun and moon slots and ENV's blocks since Z2 Stage 4).
/// @endcontract
struct Flux_RenderEnvironment
{
	Flux_RenderLightingEnvironment m_xLighting;   // LIGHT's type (row L2): the atmosphere block and the world-lighting block
	Flux_RenderLightDelta m_xSun;                 // LIGHT's record type; the sun slot, directional light 0 when m_bHasSun
	Flux_RenderLightDelta m_xMoon;                // the moon slot, directional light 1 when m_bHasMoon
	Flux_EnvironmentBlocks m_xBlocks;             // ENV's resource, copied whole; its member defaults when no Environment module runs
	bool m_bHasSun = false;
	bool m_bHasMoon = false;
};
```

| Rule | Specification |
|---|---|
| the one writer | `Render.ExtractEnvironment` (phase 12, one per world), in ZenithRuntime's engine extractors (`Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"` assigns the file to RPROD); it calls `SetEnvironment` exactly once per world per frame |
| what it reads | LIGHT's world-lighting component (`Zenith.Lighting.WorldLighting`, a C++-registered struct under Q42, `Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`) for `m_xLighting.m_xWorldLighting`, and ENV's world resource `Zenith.Environment.RenderBlocks` of the ZenithFlux type `Flux_EnvironmentBlocks`, owned by `PHASE_INPUT` and written by ENV's phase-1 system `Environment.Advance` alone (`Zenith2/Docs/Design/ENV.md → "Render-world records"`, "Frame-graph placement"; `Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"`); the dependency route is ZenithRuntime → ZenithFlux, and no ZenithEnvironment header is included |
| ordering | none is declared: the phase boundary orders the phase-12 read after every phase-1 write of the frame's ticks ([Z2-N-ARCH-472]); no phase-12 node writes the resource ([Z2-N-ARCH-476], [Z2-N-ARCH-488]) |
| absent resource | a world without the Environment module: the extractor declares no read of the resource and writes LIGHT's world-lighting values with `Flux_AtmosphereBlock::m_bValid` false, no sun or moon, and `Flux_EnvironmentBlocks`' member defaults |
| a frame without `Environment.Advance` | a frame with zero ticks copies the resource's values of the last tick |
| atmosphere and fog | the atmosphere block of `m_xLighting` (`Flux_AtmosphereBlock`: `m_bValid`, `m_uSkyRevision`, `m_xAtmosphere`, `m_xFog`, the local fog volumes, `m_xStars`, `m_xCloudShadow`) is copied from the resource, which carries LIGHT's type names; the local-fog-volume span points at the section's copy in `m_xBlocks`, so one object has one writer and the render world holds no pointer into simulation memory ([Z2-N-ARCH-416]) |
| sun and moon | the resource's `m_xSun`, `m_xMoon`, `m_bHasSun`, `m_bHasMoon`, keyed by ENV with LIGHT's `Flux_MakeNamedLightKey` (`"Zenith.Environment.Sun"`, `"Zenith.Environment.Moon"`); LIGHT treats the slots as directional lights 0 and 1 whatever other directional lights the scene holds, and a scene directional light beyond the directional limit is dropped by LIGHT's overflow rule, never a slot (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`) |
| underwater medium | not an environment member: a per-view extraction record, ENV's `Flux_RenderEnvironmentView` per view request (`Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"`), from which LIGHT fills `Flux_AtmosphereBlock::m_axViewMediumOverrides` on the render side; the environment extractor leaves that span empty |
| ENV's other records | terrain, water-body and detail-foliage records are ordinary render-world records ENV's own extractors append (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), unchanged by the ruling |

X125 records the single-writer model in Rendering; LIGHT's text (`Zenith2/Docs/Design/LIGHT/Volumetrics.md`, "Atmosphere ownership" and "Underwater medium override") still says the environment extractor copies per-view medium overrides from ENV's resource, which the ruling on per-view records replaces; reconciliation row RP25 asks LIGHT to conform. The members' types are LIGHT's and ENV's and are cited, never re-declared here.
<!-- /assumption -->

## Render frame sequence

The render subgraph of iteration k renders frame k−1 ([Z2-N-ARCH-385]). Its four node bodies, in order:

### Flux.BuildGraph

| # | Action | Owner |
|---|---|---|
| 1 | frame-slot readiness is guaranteed by the preceding frame's `PrepareNextFrame` ([Z2-N-REND-492]) | RHI |
| 2 | `Flux_GraphCaches::BeginFrame`; poll the culling-counter and PSO-creation completions | RPROD |
| 3 | GEOM load finalisation, material replacements, texture swaps (steps 1 and 2 of `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`) | GEOM |
| 3b | `Flux_ProceduralApplyRecords(GetProceduralGeometry(), xWorld)` over every section's `m_axProceduralInstances` in world-tag order | GEOM |
| 3c | when the VT system exists, `Flux_VirtualTextureSystem::BeginFrame(xContext)` (Stage 4, "Feature systems enabled after Create") | ADVGFX |
| 4 | render-world consumption (the table above, the GPU scene's `ApplyDeltas` after step 3b; `Flux_ParticleApplyRenderWorld` when the particle system exists; the 2D renderer's `PrepareFrame` when 2D is enabled) | RHI, GEOM, VFX, TWOD, RPROD |
| 5 | `Flux_BuildViewList` (shadowed-light selection, round-robin, transparent collection) | RPROD |
| 6 | the draw-key table rebuild when buckets changed (`Zenith2/Docs/Design/RPROD/Culling.md → "Draw keys and command regions"`) | RPROD |
| 7 | construct `Flux_RenderGraph` in the frame's graph arena; import the tables, pools, histories and the swapchain placeholder | RPROD (tables and pools through RHI's `ImportTables`) |
| 7b | `Flux_ProceduralBuildFrame(GetProceduralGeometry(), xRing, xFrameArena)`; the returned `Flux_ProceduralFrame` is what step 8's `Flux_AddProceduralGeometry` declares its pass from | GEOM |
| 7c | when the particle system exists, `Flux_ParticleBuildFrame(*GetParticleSystem(), xViewList, xRing, xFrameArena)` after the view list; the returned frame is what step 8's `Flux_AddParticleSimulation` and the views' `Flux_AddParticleDraw` read through `Flux_FrameShared::m_pxParticles` (null when the system does not exist or the frame has no particle work; the member is Rendering's, cross-doc item X240) | VFX (row VX2) |
| 8 | the common prologue feature functions ([Z2-N-REND-720]) | RPROD calls; each area's function |
| 9 | per view in view-list order: `PushViewSuffix`, the per-view prologue, `Flux_BuildView` (the path function, which calls the view extensions at their points and ends in the common epilogue through `Flux_BuildCommonEpilogue`, so step 9 never calls the epilogue itself; `Zenith2/Docs/Design/RPROD/Passes.md → "Path frame functions"`), `PopViewSuffix` ([Z2-N-REND-606]) | RPROD |
| 10 | `Compile()`; on error log every diagnostic, then `ZENITH_CHECK` in development builds; in Shipping skip the frame, count `Flux.Graph.CompileFailures` and keep presenting the last image | RPROD |
| 11 | descriptor patching, upload reservation extension (`ExtendReservation`); when it returns `FITS` or `EXTENDED`, every build-node producer writes its declared frame data, LIGHT's through `Flux_LightingSystem::WriteFrameData(xRing)` (`Zenith2/Docs/Design/LIGHT.md → "Build-node upload data"`) and VFX's through `Flux_ParticleWriteFrameData(*GetParticleSystem(), xRing)` when the particle system exists (row VX2); a deferred or failed extension writes none | RHI; LIGHT's call made by RPROD |
| 12 | publish the partition count to `Flux.Record` | RPROD |
| 13 | on every path out of the node, the compile-failure skip of step 10 and a deferred or failed extension included, the last LIGHT call: `Flux_LightingSystem::EndFrame(Flux_FrameLifetime::GetFrameOutcome() == FLUX_FRAME_OUTCOME_SUBMITTED)`, which commits the frame's LIGHT state on a submitted frame and discards it otherwise (`Zenith2/Docs/Design/LIGHT.md → "Deferred and failed frames"`, "Frame-level flow of LIGHT work"); and, when the particle system exists, `Flux_ParticleEndFrame(*GetParticleSystem(), bSubmitted)` with the same outcome (row VX2) | RPROD calls; LIGHT; VFX |

`Flux_FrameShared` is Rendering's struct; LIGHT's row L2 added `const Flux_LightingFrame* m_pxLighting` and `Flux_RGBuffer m_xShadowViewRecords` to it (X103 (b) applied), and ENV and UI added `m_pxEnvironment` and `m_pxUIFrame` ([Z2-N-REND-995], X209 and X230 applied). The renderer fills `m_pxLighting` in step 7 with the frame LIGHT's `Flux_LightingSystem::BeginFrame` returned, before any feature function runs, and LIGHT's `AddSharedPasses` (step 8) sets `m_xShadowViewRecords`; every other member keeps the producer Rendering's "Shared producers" gives it.

### Flux.Record

One sub-job per partition through `Zenith_JobContext::ParallelFor` (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext"`), each calling `RecordPartition`, which is RHI's recording orchestration (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Recording"`) calling RPROD's pass bodies and contexts. The node's affinity is `JOB_AFFINITY_ANY`: recording never runs on the render-submit thread (cross-doc item X53).

### Flux.Submit and Flux.Present

`Submit` is RHI's (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Submission"`). `Present` runs the frame pacer ("Frame pacer") around RHI's present and captures, then RHI's `PrepareNextFrame`, both on the render-submit thread ([Z2-N-REND-805]).

## Quality settings

RPROD owns the shape of `Flux_QualitySettings`, its RPROD members' keys and the tier mechanism; LIGHT owns the nested aggregate `m_xLight`, whose type, members, keys and upscaler enumeration are LIGHT's and are cited here, never repeated (the seam ruling on LIGHT's round-1 review, reconciliation row RP19).

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_Quality.h
#pragma once
#include "ZenithFlux/Flux_LightSettings.h"   // LIGHT's Flux_LightQualitySettings

enum Flux_QualityTier : uint8
{
	FLUX_QUALITY_TIER_HIGHEST = 0,           // index 0 of the platform's tier list (highest first)
	FLUX_QUALITY_TIER_MAX_COUNT = 8,
	FLUX_QUALITY_TIER_INVALID = 0xFF,
};   // a value in [0, count) is the index of the active tier in the platform's tier list

struct Flux_QualitySettings                  // the active tier's values with the view's overrides applied
{
	uint32 m_uEnabledFeatures;              // Flux_ViewFeature bits the tier allows          render.quality.features
	uint32 m_uMaxViewsPerFrame;             // Z2-B-RPROD-022                                  render.quality.maxViewsPerFrame
	uint32 m_uMaxTextureAndCaptureViews;    // Z2-B-RPROD-023                                  render.quality.maxTextureAndCaptureViews
	uint32 m_uPerObjectLightCap;            // Z2-B-RPROD-013 (pending; proposed 8)            render.quality.perObjectLightCap
	float m_fLODScale;                       // GEOM's Flux_LODView::m_fLODScale                render.quality.lodScale
	sint32 m_iTextureMipBias;                 // GEOM's residency bias                           render.quality.textureMipBias
	Flux_LightQualitySettings m_xLight;      // LIGHT's (row L2): shadows (cascades, atlas, shadowed-light cap, shadow HiZ,
	                                         // filter quality), contact shadows, SSAO, fog and its froxel grid, SSR, probes,
	                                         // GI, temporal (the one upscaler enumeration), dynamic resolution, exposure,
	                                         // bloom, depth of field, motion blur
};

struct Flux_QualityOverrides                 // per view (texture views reduce quality; pressure actions 9 and 10)
{
	uint32 m_uFeatureClearMask = 0;
	sint32 m_iShadowQualityDelta = 0;          // added to m_xLight.m_xShadows.m_uFilterQuality, clamped into 0 to 2
	bool m_bFroxelGridDown = false;           // m_xLight.m_xFog.m_auGrid takes the next lower tier's grid (pressure action 10)
};
```

| Rule | Specification |
|---|---|
| ownership of meaning | RPROD: the struct, the tier mechanism, the members above `m_xLight` and their `render.quality.*` keys (views, per-object light cap, feature mask); GEOM: the meaning of the LOD scale and mip bias; LIGHT: every member of `m_xLight`, its configuration keys and their registration (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`, "Configuration keys") |
| one upscaler enumeration | the upscaler is `m_xLight.m_xTemporal.m_eUpscaler`, of LIGHT's type (`Zenith2/Docs/Design/LIGHT/Temporal.md → "Upscaler selection"`); RPROD declares no upscaler enumeration |
| dynamic resolution | the scale of a view is written by LIGHT's `Flux_DynamicResolutionController` alone (`Zenith2/Docs/Design/LIGHT/Temporal.md → "Dynamic-resolution controller"`); RPROD reads it in view-list construction and carries no resolution member here (one writer of the scale) |
| no anisotropy member | the sampler table is static and fixed ([Z2-N-REND-058]), so a per-tier anisotropy value could not reach a sampler without a container-version change; the tier carries none |
| reading LIGHT's members | `Flux_ReadLightQualitySettings(xStore, xSettings.m_xLight)` (LIGHT's) fills `m_xLight` in "Quality tier selection" step 2; `Zenith_RegisterFluxConfigKeys` calls `Flux_RegisterLightConfigKeys` after registering RPROD's keys |
| tier files | engine tier fragments `Zenith2/Engine/ZenithFlux/Internal/Config/Quality/<TierID>.config.json` (cooked into `content:/Engine/Config/Quality/<TierID>.config.json`, `Zenith2/Docs/Design/PLAT/Configuration.md → "Engine and game sub-sources"`), game overrides in `Games/<Name>/Assets/Config/Quality/<TierID>.config.json`; one file per tier carries both RPROD's and LIGHT's keys; the `.config.json` suffix is PLAT's layer-4 name, which LIGHT's `<TierID>.json` conforms to (row RP19) |
| tier lists | the engine fragments of `tiers.windows.json` and `tiers.android.json` (`zenith2.quality-tiers`, PLAT's format) under `Zenith2/Engine/ZenithFlux/Internal/Config/Quality/`, listing the tier IDs of `Zenith2/Docs/QualityMatrix.md` (pending, its "Quality tiers" section) highest first |
| budgeted values | `m_uMaxViewsPerFrame`, `m_uMaxTextureAndCaptureViews` and `m_uPerObjectLightCap` take the per-tier values of `Z2-B-RPROD-022`, `-023` and `-013` once the Stage 1 RPROD budget ticket adopts them; the tier files carry the proposed values until then and a test compares them with the generated budget table ([Z2-N-REND-608]) |
| Stages 2 to 4 | one tier per platform, `QUALITY_TIER_WIN_DEFAULT` and `QUALITY_TIER_S24_DEFAULT`, whose files carry the desktop and mobile values of the per-pass tables for RPROD's keys and of LIGHT's tier tables for LIGHT's keys (froxels 128 × 72 × 48 and 80 × 45 × 32 through the three INT keys `render.quality.froxelGridX`, `…Y` and `…Z`, bloom 6 and 5 levels; `Zenith2/Docs/Design/LIGHT/QualityTiers.md → "Volumetric tiers"`, "Post-processing and output tiers"); the light-cluster grid is no tier value, because Rendering's `LightClustering` table fixes it per capability tier (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Cluster grid"`) |
| Stage 5 | the full per-platform list of `QualityMatrix.md` (Z2-I-RPROD-20): the tier IDs LIGHT's tier tables use, `QUALITY_TIER_WIN_LOW`, `_DEFAULT`, `_HIGH`, `_EPIC` and `QUALITY_TIER_S24_LOW`, `_DEFAULT`, `_HIGH` (LIGHT row L23) |

LIGHT's landed revision conforms to this shape (rows L28 and L38 answering RP19): `Flux_LightQualitySettings` is reached through `Flux_ViewContext::m_pxQuality->m_xLight` and carries no output or cluster member, `Flux_UpscalerMode` is the only upscaler enumeration, the dynamic-resolution range lives in `Flux_DynamicResolutionSettings`, the froxel grid is `m_xLight.m_xFog.m_auGrid` from three INT keys, and LIGHT's tier files take PLAT's `.config.json` suffix (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`).

`m_xLight` is in Rendering's text since X103 (b).

<!-- assumption: pending BUDGET -->
The two default tier IDs must appear in `Zenith2/Docs/QualityMatrix.md`'s "Quality tiers" table when the Stage 1 QualityMatrix ticket writes it (`Zenith2/Docs/Budgets.md → "Production-quality acceptance"`, [Z2-N-BUDGET-404], whose example is `QUALITY_TIER_S24_DEFAULT`); reconciliation row RP9 records the need.
<!-- /assumption -->

## Quality tier selection

| Step | Action |
|---|---|
| 1 | PLAT resolves the tier ID (`quality.activeTier` when non-empty, else `quality.tier`, validated against the platform list, falling back to `QUALITY_TIER_<PLATFORM>_DEFAULT`) and loads its files as configuration layer 4 (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier resolution"`) |
| 2 | at `Create` and at every `ApplyConfigChanges` whose changed keys include a quality-tier-layer key or whose active tier changed, the renderer reads every `render.quality.*` key from the store into a new `Flux_QualitySettings`, fills `m_xLight` with `Flux_ReadLightQualitySettings`, sets `Flux_QualityTier` to the tier's index in `m_axPlatformTiers`, validates RPROD's ranges (an out-of-range value keeps the previous value and logs `flux.quality` once; the validation of `m_xLight` is LIGHT's), and hands `m_xLight` to `Flux_LightingSystem::ApplyQualitySettings` |
| 3 | the new settings apply from the next `BuildGraph`; views whose feature mask, extent or path changed invalidate their histories ([Z2-N-REND-599]) |
| 4 | per view, `Flux_ViewContext::m_pxQuality` points at the frame's copy with the view's `Flux_QualityOverrides` and the pressure overrides (actions 9 and 10 of `Zenith2/Docs/Rendering/GpuResources.md → "Responses to memory pressure"`) applied; action 11 (the dynamic-resolution floor) is applied by RPROD's pressure hook, on the `Flux.BuildGraph` node before `UpdateDynamicResolution`, calling `Flux_LightingSystem::GetDynamicResolution(xViewID).SetPressureFloor(true)` for every view for which `Flux_LightingSystem::HasDynamicResolution(xViewID)` is true while the level lasts, and `SetPressureFloor(false)` when it falls (`Zenith2/Docs/Design/LIGHT/Temporal.md → "Thermal and pressure inputs"`; `GetDynamicResolution` asserts `HasDynamicResolution`, so no other view is asked) |
| 5 | thermal adaptation changes the tier through PLAT's user-layer write only ([Z2-N-ARCH-919]); a tier change never touches simulation |

## Frame pacer

<!-- elaboration -->
The pacer chooses when each frame is presented and, through the latency slot, when the next frame may begin ([Z2-N-REND-805]). It runs on the render-submit thread inside `Flux.Present` (`Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"`); the iteration barrier ([Z2-N-ARCH-386]) makes the whole frame loop wait on it, which is the loop's one pacing point.

```cpp
// Internal/Pacing/Flux_FramePacer.h
enum Flux_PacingMode : uint8 { FLUX_PACING_MODE_OFF = 1, FLUX_PACING_MODE_INTERVAL = 2, FLUX_PACING_MODE_EXTERNAL = 3 };   // EXTERNAL: Stage 7, never a configuration value ("External pacing")
struct Flux_PacingSettings { Flux_PacingMode m_eMode = FLUX_PACING_MODE_INTERVAL; uint32 m_uTargetFps = 60; uint32 m_uRefreshHzFallback = 60; uint32 m_uPresentMarginUs = 1500; };

struct Flux_PacerInput                   // one frame's observations
{
	uint64 m_ulNowNs;
	uint64 m_ulLastPresentReturnNs;
	uint32 m_uTimingCount;
	const Zenith_RHIPresentTiming* m_pxTimings;   // newest first
	Zenith_RHIPresentMode m_ePresentMode;         // FIFO paces; MAILBOX and IMMEDIATE do not (step 5b)
	bool m_bPresentTiming;                    // VK_GOOGLE_display_timing
	bool m_bLatencyWaitable;                  // DXGI waitable object or present wait
	bool m_bSwapIntervalControl;              // D3D12 sync interval
};
struct Flux_PacerDecision { uint64 m_ulDesiredPresentNs; uint32 m_uSwapInterval; uint64 m_ulCpuWaitUntilNs; uint64 m_ulTargetIntervalNs; };

class Flux_FramePacerCore          // pure: no device, no clock; unit-tested with synthetic inputs
{
public:
	Flux_PacerDecision Decide(const Flux_PacerInput& xInput, const Flux_PacingSettings& xSettings);
	void Observe(uint64 ulPresentReturnNs);          // present-to-present intervals, ring of 600
	Flux_PacingState GetState() const;
};
```

| Step | Rule |
|---|---|
| 1 refresh | R = the newest valid `m_ulRefreshNs` of the present timings; otherwise `1e9 / r.pacing.refreshHzFallback` |
| 2 interval | target `T* = 1e9 / r.pacing.targetFps`; swap interval `n = max(1, floor(T* / R + 1/64))`, so the paced rate is never below the target and 16.667 ms on a 60 Hz display rounds to n = 1; `T = n × R` (Budgets' target interval of [Z2-N-BUDGET-257]: 16.67 ms at 60 fps on 60 Hz and 120 Hz displays) |
| 3 mode off | present at once; step 6 only |
| 4 desired time | `D(f) = max(A(f − 1) + T, now + margin)`, where `A(f − 1)` is the previous frame's actual present time from the timings when reported, else its desired time |
| 5 realisation | with a pacing present mode (`r.vsync = 1`, FIFO on Vulkan, sync interval ≥ 1 on D3D12, `Zenith2/Docs/Rendering/PerPass/Common.md → "Present"`): Android with display timing passes `D(f)` as the frame's desired present time; D3D12 presents with sync interval n; Vulkan without timing presents at once when n = 1 (FIFO paces) and, when n > 1, waits on the CPU until `D(f) − R / 2` (`Zenith_SleepUs`, a render-submit-thread wait that [Z2-N-ARCH-394] allows) and then presents |
| 5b no pacing present mode | with `r.vsync = 0` (MAILBOX or IMMEDIATE on Vulkan, sync interval 0 with tearing on D3D12), the present itself paces nothing, so for every n ≥ 1 the pacer waits on the CPU until `D(f) − r.pacing.presentMarginUs` and then presents; `m_uSwapInterval` is 0 and the D3D12 sync interval stays 0; in mode `off` it presents at once |
| 6 latency slot | after the present, `WaitForLatencySlot(2 × T + 50 ms)`, which bounds run-ahead to `r.framesInFlight` frames; a timeout logs `flux.pacing` once per second and continues |
| 7 record | `Observe` the present-return timestamp (the frame-time clock of [Z2-N-BUDGET-062]) and publish `Flux.Pacing.IntervalUs` and `Flux.Pacing.DeviationUs = |interval − T|`; the row `Z2-B-RPROD-020` is the p95 of the deviation over a run's measured interval (MP-FRAME) |

| State | Event | Next | Action |
|---|---|---|---|
| `WARMUP` | 3 presents observed with a valid refresh | `PACING` | compute n and T |
| `WARMUP`, `PACING` | swapchain recreated, present mode, `r.vsync` or `r.pacing.*` changed | `WARMUP` | reset the ring and `A` |
| `PACING` | a frame interval above 4 × T (a hitch) | `PACING` | `A(f − 1)` is replaced by `now`, so the pacer does not try to catch up |
| any | swapchain suspended (minimised, surface lost) | `SUSPENDED` | no present, no wait |
| `SUSPENDED` | swapchain live again | `WARMUP` | — |

The two swapchain hooks step 5 calls are RHI's, answering row RP8: `Zenith_RHISwapchain::SetDesiredPresentTime(uint64 ulPresentID, uint64 ulDesiredNs)` (`VkPresentTimesInfoGOOGLE` on the present carrying the ID with `VK_GOOGLE_display_timing`; `FEATURE_UNAVAILABLE` elsewhere) and `Zenith_RHISwapchain::SetSwapInterval(uint32 uInterval)` (1 to 4, the D3D12 sync interval), both called on the render-submit thread inside `Flux.Present` before the present (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHISwapchain header"`, `Zenith2/Docs/Design/RHI/Recovery.md → "Frame pacing hooks"`).

### External pacing

<!-- elaboration -->
While a stereo frame source is set and its `PacesFrames()` returns true ("Stereo frame source"), the pacer runs in `FLUX_PACING_MODE_EXTERNAL` whatever `r.pacing.mode` says (rows SP1 (b), XRR-V7): steps 1 to 5b are skipped (no desired present time, no swap-interval change, no CPU wait), so the runtime's `xrWaitFrame` is the frame loop's one pacing wait; step 6, the latency slot, stays, so run-ahead stays bounded by `r.framesInFlight`; step 7 observes the intervals between successive `AfterSubmit` calls and publishes them as `Flux.Pacing.IntervalUs`. Entering or leaving the mode moves the state machine to `WARMUP`. While the mode lasts the renderer asks RHI's swapchain manager for the window swapchain's `IMMEDIATE` present mode (`MAILBOX` where `IMMEDIATE` is unavailable), applied at the next render-idle point, so the desktop present never waits on the desktop's refresh, and restores the `r.vsync` mode when it ends.

### Pacing for measurement runs

A research cell or a benchmark harness run whose frame time must not be quantised by the display (RESEARCH's "Shading path cell matrix", answering row RS4) runs with `r.pacing.mode = off` and `r.vsync = 0`: the present mode is `MAILBOX` or `IMMEDIATE` on Vulkan and sync interval 0 on D3D12, and step 3 presents at once, so no CPU wait and no vsync interval enters a frame. The latency slot (step 6) still bounds run-ahead. The harnesses' configuration files set both keys.

### Input-to-photon attribution

Each `Present` records `{m_ulInputSequence, m_ulInputSampleTimeUs, present ID}` of the frame's render world into a ring of 16; when the present's display time becomes known (the timings, `VK_KHR_present_wait` completion or `GetFrameStatistics`), the latency `display − sample` is published as `Flux.Latency.InputToPhotonUs` for the MP-LATENCY procedure (`Zenith2/Docs/Budgets.md → "Input-to-photon latency"`, [Z2-N-BUDGET-378]) and row `Z2-B-RPROD-027`; the record carries the pipelining frame and the frames in flight separately, as the row's decomposition requires ([Z2-N-BUDGET-377]).
