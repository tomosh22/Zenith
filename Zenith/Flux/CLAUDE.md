# Flux Rendering System

## Render Pipeline at a Glance

A typical frame compiles into roughly this topologically-sorted order. The render graph derives the order from each pass's declared Reads/Writes — this is **not** a hardcoded sequence and individual subsystems can be enabled/disabled without changing the rest. Click `Render/RenderGraph/Print Pass Order` in the debug variables panel to dump the live order at runtime.

```
+-----------------------------+
| Frame begin                 |
+-----------------------------+
              |
              v
+-----------------------------------+  GPU-driven opaque setup (compute)
| Unified Skinning                  |--> skins animated meshes into the shared arena
| Unified Cull Reset -> Culling     |--> fills the per-(view,bucket) cull-output +
+-----------------------------------+    indirect-arg buffers for EVERY view, camera
              |                           and shadow cascade alike
              v
+-----------------------------+    Cascaded Shadow Maps (4 cascades)
| Shadow cascades             |--> writes shadow depth targets; draws INDIRECT from
+-----------------------------+    the cull output above, which is why the cull must
              |                     precede them (see the ordering note below)
              v
+-----------------------------------+  G-buffer build
| Terrain                           |\
| UnifiedMesh (statics + trees +    | >--> writes MRT diffuse / normals+ambient / material + scene depth
|   compute-skinned animated meshes)|/
| Vegetation (grass)                |
| Primitives (debug + gameplay)     |
+-----------------------------------+
              |
              v
+-----------------------------+    Screen-space effects
| SSAO                        |
| HiZ generation              |
| SSR (raymarch / upsample)   |
| SSGI (raymarch / denoise)   |
+-----------------------------+
              |
              v
+-----------------------------+    Lighting
| DeferredShading             |--> reads G-buffer + shadows + IBL + SSR + SSGI, writes HDR scene
| Skybox                      |
| Volumetric Fog              |
| Particles                   |
+-----------------------------+
              |
              v
+-----------------------------+    Temporal AA (default ON) + optional upscaling (see TAA/CLAUDE.md)
| TAA Resolve / History / Sharpen |--> resolves the lit HDR scene (main view) via motion vectors +
+-----------------------------+     sub-pixel jitter -> the post-FX scene colour. With upscaling on,
              |                      the scene chain above renders below output res and TAA
              v                      reconstructs at full res here. (Off/`--taa=0` -> raw HDR passes through.)
+-----------------------------+    HDR -> LDR (bloom is AFTER TAA)
| HDR Bloom + Tonemap         |--> reads the TAA output (GetSceneColourForPostFX; raw HDR when TAA off),
+-----------------------------+     writes swapchain LDR. (The FXAA stopgap here was removed once TAA shipped.)
              |
              v
+-----------------------------+    Overlays (LDR)
| UI quads / text             |
| Editor gizmos (tools build) |
| ImGui (tools build)         |
+-----------------------------+
              |
              v
+-----------------------------+
| Present                     |
+-----------------------------+
```

For the full graph lifecycle (Setup -> Compile -> Execute), barrier synthesis, the fluent builder API, and the Print Pass Order debug button, see [RenderGraph/CLAUDE.md](RenderGraph/CLAUDE.md).

> **The shadow cascades run AFTER the unified cull, not first.** The diagram used to open with the
> cascades because "shadows come first" is the folklore order. They can't: each cascade issues an
> indirect draw from the cull-output buffers that `UnifiedMesh`'s compute passes fill for all views at
> once, so the cull is its producer. That ordering is carried by the cascade passes' declared
> `ReadBuffer`s — which only become graph edges if `UnifiedMesh` is registered BEFORE `Shadows` in
> `RegisterDefaultFeatures` (a reader links only to an EARLIER-declared writer). It wasn't, the edges
> were silently absent, and the sort interleaved the cascades with their own producers — one drew from
> indirect args the reset had just zeroed. `Flux_RenderGraph::ValidateProducerBeforeConsumer` now
> guards it and must stay at zero violations. **`Grass` is registered before `Shadows` for exactly
> the same reason** — cascades 0-1 draw its LO blade partition indirect, so they read the buffers
> its reset/placement/fixup compute passes fill.

> **Primitives sits in the G-buffer block, not with the overlays** — this diagram used to list it beside DeferredShading/Skybox/Fog/Particles. `Flux_PrimitivesImpl::SetupRenderGraph` declares its one pass ("Primitives GBuffer") as a `Writes` of all four core MRTs + the depth attachment, and "Apply Lighting" `Read`s exactly those, so the topological sort can only put Primitives **before** lighting. Primitive draws are therefore lit (or, on the gameplay channel, tagged unlit and passed through) by the deferred pass like any other opaque geometry — they are not composited on top of it.

## Files

### Core
- `Flux.h/cpp` - Main rendering infrastructure, pipeline specification (`Flux_SurfaceInfo` and the view types live in `Flux_Types.h`)
- `Flux_Buffers.h/cpp` - Buffer management
- `Flux_RecordValidation.h` - Shared inline asserts (pipeline/view/draw-constant validity) called by both backends' recorder methods
- `Flux_Enums.h` - Rendering enums (ResourceAccess, TextureFormat, BlendFactor, DepthCompareFunc, MeshTopology, ShaderDataType, BindingType, LoadAction, StoreAction, MRTIndex, etc.)
- `Flux_GraphicsImpl.h` - Global graphics state, frame constants (`Flux_GraphicsImpl` class with `FrameConstants`); `Flux_Graphics.cpp` holds thin static forwards onto it (no `Flux_Graphics.h`)
- `Flux_RenderTargets.h/cpp` - Render target management
- `Flux_Types.h` - Type definitions, `IsCompressedFormat()` helper
- `Flux_VertexLayoutDesc.h` - The constexpr description of a program's vertex input (`Flux_VertexLayoutElement` / `Flux_VertexLayoutDesc`) + the CLOSED semantic vocabulary. A dependency-light POD leaf: the auto-generated `Shaders/Generated/<Subsystem>.h` headers include it, and so does a Core-visible TU that static_asserts one of them against `Zenith_TerrainChunkLayout`.
- `Flux_VertexCodec.h` - **THE** place a vertex attribute is quantised (half / snorm / unorm / SNORM10:10:10:2). Pure, header-only, allocation-free — the same code runs in the offline exporters, the editor's live-edit hooks and the headless unit suite.
- `Flux_VertexLayoutValidation.h/.cpp` - The stale-codegen tripwire every backend's `FromSpecification` calls. See *Vertex Layouts* below.



Note: Materials and textures are now in `AssetHandling/` (see AssetHandling/CLAUDE.md):
- `Zenith_MaterialAsset.h/cpp` - Material properties + texture references
- `Zenith_TextureAsset.h/cpp` - GPU texture wrapper with SRV

### Subdirectories
- `UnifiedMesh/` - **THE opaque mesh pipeline — static, instanced-foliage, AND skeletal** (GPU-driven: compute cull → indirect draw to the camera G-buffer + every shadow cascade, fed from the render snapshot). Skeletal meshes are GPU compute-skinned into a shared arena (Stage 5) then drawn like static geometry; `Flux_SkeletonInstance::GetSkinningMatrices` is the CPU input. Stage 4 retired the per-object StaticMeshes/InstancedMeshes draw loops and Stage 5 retired the per-object skeletal draw + its bone constant-buffer.
- `MeshAnimation/` - Skeletal animation system (see MeshAnimation/CLAUDE.md). ECS entry point is `Zenith_AnimatorComponent`, not `Zenith_ModelComponent`.
- `Terrain/` - Terrain rendering (see Terrain/CLAUDE.md)
- `Shadows/` - Cascaded shadow maps
- `DeferredShading/` - Deferred lighting
- `SSAO/` - Screen-space ambient occlusion
- `Fog/` - Volumetric fog system (see Fog/CLAUDE.md)
  - Four techniques: Simple, Froxel, Raymarch, God Rays
  - Runtime technique selection via debug variables
  - Requires 3D texture support (see Vulkan/CLAUDE.md)
- `Particles/` - Particle systems. **TWO simulation paths, one draw pass and one billboard pipeline pair.** A `Flux_ParticleEmitterConfig` picks with `m_bUseGPUCompute`. The **CPU** path simulates in `Zenith_ParticleEmitterComponent`, is gathered EC-side into `Zenith_ParticleEmitterRenderData`, and is uploaded into two per-blend-mode dynamic vertex buffers each frame. The **GPU** path (`Flux_ParticleGPUImpl`) is fully GPU-driven: emitters reserve a slice of one 4096-particle pool, spawns are ring-buffered into a ping-pong SSBO, `Flux_ParticleUpdate.slang` integrates them **one dispatch per emitter** (gravity/drag/turbulence/blend mode all ride the push constants, which is why the 96 B particle record carries no emitter back-reference), and each survivor is compacted into its blend partition of a vertex-usable instance buffer while atomically bumping that partition's `VkDrawIndexedIndirectCommand`. The draw records two `DrawIndexedIndirect` — nothing is ever read back to size it, and the partition base is applied by the vertex-stream **bind offset**, not `firstInstance` (which would need `drawIndirectFirstInstance`). Both paths draw through the same `Flux_ParticlesImpl` pass, so an emitter's look does not depend on which one it uses. In-tree GPU consumer: Combat's arena candles.
- `Skybox/` - Sky rendering
- `Text/` - Text rendering
- `Primitives/` - Debug **and gameplay** primitives (see Primitives/CLAUDE.md). Two independent channels: the `Add*` debug channel, gated on the tools debug variable `Graphics/Primitives/Enabled`; and the `SubmitGameplay*` channel, which is drained unconditionally and drawn unlit/emissive. A production gameplay cue must use the second one.
- `Gizmos/` - Editor gizmos (see Gizmos/CLAUDE.md)
- `InstancedMeshes/` - Instance-group registration front-end (`Flux_InstanceGroup` CPU transform/anim SoA + VAT). Stage 4: the draw/cull/shadow passes were retired; `UnifiedMesh` reads the registered groups and draws them. (`Flux_InstanceCulling.h`'s frustum helper is shared with `UnifiedMesh`.)
- `DynamicLights/` - Clustered dynamic lighting (gather/upload front-end)
- `HDR/` - HDR bloom + tonemap pipeline (see HDR/CLAUDE.md)
- `HiZ/` - Hierarchical Z-buffer generation (see HiZ/CLAUDE.md)
- `IBL/` - Image-based lighting (see IBL/CLAUDE.md)
- `SSR/` - Screen-space reflections (see SSR/CLAUDE.md)
- `SSGI/` - Screen-space global illumination (see SSGI/CLAUDE.md)
- `Decals/` - Deferred decals (see Decals/CLAUDE.md)
- `Vegetation/` - GPU-driven grass (see Vegetation/CLAUDE.md). Three compute passes regenerate every blade from scratch each frame, then two indirect draws write them into the **G-buffer** — blades are opaque geometry lit by DeferredShading, not a forward overlay — and two more draw the LO blade into CSM cascades 0-1. Nothing per-blade is persisted or uploaded; the one thing that *is* persistent GPU state is the camera-anchored **displacement trail map** (a 256² RG16F ping-pong, decayed + re-anchored + mover-splatted by a fifth compute pass that runs last and is sampled a frame later).
- `Translucency/` - Forward translucent pass
- `SDFs/` - Signed distance field rendering
- `Quads/` - Textured/UI quad rendering
- `Present/` - Final-RT → backbuffer blit (backend-neutral present)
- `SceneGraph/` - Render scene snapshot + culling
- `RenderViews/` - Render-view registry (fixed slots: main camera / shadow cascades / material preview) + the editor material-preview controller. Each view owns a persistent VIEW descriptor-set instance (`.View(slot)` on a graph pass selects it) and, for full-pipeline views, its own G-buffer/depth/HDR transients. The material preview renders through the REAL pipeline as a second view (per-draw-item view masks scope content); its old bespoke render system was deleted.
- `MeshGeometry/` - Shared mesh geometry buffers
- `Shaders/` - `.slang` shader sources (`Common/` shared helper modules). Conventions + the frequency-set spine, accessor facade, `interface`/`extension` seams, spec-constant folding, and include-vs-import rules are in [Shaders/SHADER_STYLE.md](Shaders/SHADER_STYLE.md). Feature shaders reach the GLOBAL/VIEW/BINDLESS sets only through the free-function accessors in `Common/Bindings.slang` — never the raw `g_xViewSet`/`g_xGlobalSet`/`g_xBindlessSet` blocks (enforced by the spine lint below).
- `Slang/` - Shader catalog + Slang compilation glue (`Flux_ShaderCatalog`) + the spine lint (`Flux_SpineLint.h`, a FluxCompiler gate that fails the build on any direct spine poke / spine `extension` / block redeclaration outside `Common/Bindings.slang`).
- `Backend/` - Backend concept conformance asserts (`Flux_BackendConformance.cpp`) + the **neutral indirect-count capability/policy types** (`Flux_IndirectDraw.h`): `Flux_IndirectCountFallback` (`REQUIRE_NATIVE` / `ZERO_PADDED_TO_MAX`), `Flux_IndirectExecutionMode`, `Flux_IndirectDrawCapabilities`, `Flux_IndirectDrawOverride`, the pure `Flux_SelectIndirectExecutionMode` selector, the pure `Flux_IndirectDrawBatchPlan` fixed-draw batch planner, the 20-byte indexed-indirect-command ABI POD (`Flux_IndirectDrawIndexedCommand`) and `Flux_ZeroIndirectDrawIndexedCommand` helper. Dependency-light (`<cstdint>` / `<cstddef>`) so it is reachable by every backend concept, the CLI converter and the terrain allocation/seed. Vulkan stays OUT of this header: advertised-vs-enabled-vs-usable negotiation is the backend's own concern, surfaced here only as the USABLE semantic booleans. Pure tests live in `Flux_IndirectDraw.Tests.inl`, hosted by `Flux.cpp` (NOT `Flux_BackendConformance.cpp` — that TU has only compile-time static_asserts, so `/OPT:REF` dead-strips its .obj and takes the ZENITH_TEST registrars with it; `Flux.cpp` has many runtime functions and survives the link in every backend variant).
- `RenderGraph/` - Render graph lifecycle, barrier synthesis, builder API (see RenderGraph/CLAUDE.md)

## Architecture

### Direct command recording
Render systems record GPU work by calling methods **directly** on the backend command
buffer (aliased `Flux_CommandBuffer` — `Zenith_Vulkan_CommandBuffer` in the Vulkan build):
`pxCmdBuf->SetPipeline(...)`, `Draw(...)`, `Dispatch(...)`, plus the named-binding helper
`Flux_ShaderBinder`. There is no intermediate command-list DSL — a pass's `OnRecord`
callback receives a `Flux_CommandBuffer*` and emits native draws/dispatches/binds. The
backend command-recorder surface is the C++20 `FluxBackendCommandRecorder` concept (see
Backend Abstraction); shared `Flux_RecordValidation.h` helpers carry the validity asserts.
(Historically this went through a deferred `Flux_CommandList` byte-buffer DSL replayed by
`IterateCommands` — that abstraction was removed once the render graph owned ordering +
barriers, leaving the DSL pure indirection.)

### Pass Execution Order
There is no `RenderOrder` enum and no caller-supplied ordering token. Pass execution order is computed at frame boundary by `Flux_RenderGraph::Compile()`:

1. Each pass declares the resources it `Reads()`/`Writes()`/`DependsOn()` via the fluent `AddPass()` builder.
2. `Compile()` builds a dependency adjacency from those declarations and runs Kahn's topological sort.
3. Resource transitions (image layout / buffer access) are then synthesized automatically as barriers between consecutive passes.

At `Execute()`, the graph queues each enabled pass in topological order (`QueueRenderPass`) and then drives the single direct-recording stage: the backend records each pass's callback (`Flux_RenderGraph::RecordPassInto`) into per-worker command buffers, in topological order, with barriers emitted inline. Worker command buffers are submitted in order, so the topological order + inline barriers are what enforce cross-pass dependencies. See `Flux/RenderGraph/CLAUDE.md` for the full graph lifecycle and the **Print Pass Order** debug button which dumps the current frame's resolved order.

### Pipeline Specification
`Flux_PipelineSpecification` struct defines complete graphics pipeline state: shader, blend modes, depth test, vertex input, render targets, load/store actions.

### Vertex Layouts (reflection-driven)

**A vertex layout is authored in the shader and nowhere else.** A `VsIn` field's declared
type gives its storage format by inference; `[VtxFmt("snorm10_10_10_2")]` overrides that
wherever storage differs from the declared type (fetch hardware widens and converts, so a
four-byte SNORM10:10:10:2 attribute is still declared `float4`), and `[PerInstance]` moves a
field off the per-vertex stream (binding 0) onto the per-instance stream (binding 1). Both
attributes are declared in `Shaders/Common/VertexFormats.slang`; the authoring rules live in
[Shaders/SHADER_STYLE.md](Shaders/SHADER_STYLE.md).

The chain from there:

1. **Reflection.** Slang reports each `VsIn` field's semantic, storage format, binding and
   byte offset (`Flux_ShaderReflection::GetVertexAttributes` / `GetVertexStride`).
2. **Codegen.** FluxCompiler bakes that table into the program's `Shaders/Generated/<Subsystem>.h`
   as `inline constexpr kaxVertexAttribs[]` + `kVertexLayout` (a `Flux_VertexLayoutDesc`).
   These are committed, human-diffable constants; CPU packers and `static_assert`s read them.
3. **Pipeline build.** `Flux_PipelineBuilder::FromSpecification` builds the vertex input from
   the **LIVE reflection**, never from the generated table. The spec only *names* the layout
   it expects (`m_pxVertexLayout`).
4. **Tripwire.** Every backend's `FromSpecification` — Vulkan, Null and D3D12 alike — calls
   `Flux_ValidateVertexLayoutForSpec` (`Flux_VertexLayoutValidation.h`), which compares the
   generated table against the live reflection **exactly** (semantic, semantic index, storage
   format, binding, byte offset, and BOTH binding strides) and asserts naming the offending
   program plus *"RERUN FLUXCOMPILER"*. There is no tolerance to allow: a layout that is
   "nearly" right reads adjacent bytes as geometry, which renders garbage rather than failing.
   The tripwire never repairs anything — the GPU always fetches the live reflection, so a stale
   generated table is caught, reported, and otherwise inert.

**One codec, and one packer for the mesh family.** `Flux_VertexCodec.h` is universal: EVERY
packed lane the CPU writes, on every path, is quantised there and nowhere else. On top of it,
the interleaved MESH-family streams (static mesh, skin input, the unified-mesh geometry
buffers) go through the single generic interleaver `Flux_PackVertices`
(`MeshGeometry/Flux_VertexPacker.h`) — semantic-keyed SoA source views in, one layout-shaped
buffer out, with the *generated layout* as the authority for every offset and stride. That
replaced hand-written interleave loops which each hard-coded a byte layout agreeing with a
shader's `VsIn` by convention alone, so a mesh layout edit now moves the writer and the reader
together.

The two non-mesh producers write field-by-field instead of interleaving, and stay correct the
same way — through the codec, against a generated table:

- **Terrain** packs through `Terrain/Flux_TerrainVertexQuant.h` (the one place a baked terrain
  position/UV is quantised, shared by the exporter, the editor sculpt hook, CityBuilder's carve
  + stream-in hook and the chunk validator), pinned element-by-element against
  `Zenith_TerrainChunkLayout::axELEMENTS`.
- **Per-instance streams** pack in typed setters on the instance struct itself
  (`Flux_TextImpl.h`, `Flux_QuadsImpl.h`, `Flux_ParticleData.h`, `Flux_GizmosImpl.h`), each
  calling the codec, each pinned against its `Generated/<Subsystem>.h` table.

**Where each contract lives.** The per-stream formats are deliberately NOT duplicated here —
the numbers drift, the authorities do not:

| Stream | Authority |
|---|---|
| Static mesh + skin input | `MeshGeometry/Flux_MeshInstance.h` (`Flux_DeclareMeshVertexLayout`) + `UnifiedMesh/Flux_Skinning.h` |
| Terrain | `Core/Zenith_TerrainChunkLayout.h` (it is the ON-DISK contract), joined to the codec by `Terrain/Flux_TerrainVertexQuant.h` |
| Per-instance streams (Text / Quads / Particles / Gizmos) | each feature's `Flux_<Feature>Impl.h` instance struct, pinned against its `Shaders/Generated/<Subsystem>.h` table |
| Semantic vocabulary | `Flux_VertexLayoutDesc.h` — CLOSED; codegen hard-errors on a semantic outside it rather than inventing a tag |

> **★ CHANGING A `[VtxFmt]` IS NOT A SHADER HOT-RELOAD.** Hot-reload rebuilds pipelines from
> the new reflection, but the CPU-side packed buffers are **not** re-packed and the committed
> `Generated/` header does not move. Rerun FluxCompiler and restart the engine. If you don't,
> the tripwire fires on the next pipeline build and names the program.

The GPU half of the codec (`Flux_DequantPosition`, plus the manual half/snorm/unorm packers the
compute skinner needs because it reads and writes raw SSBO words with no fetch unit) lives in
`Shaders/Common/VertexFormats.slang` and must stay bit-for-bit the CPU codec — the skinning
arena is fetched by the SAME pipelines that fetch a CPU-packed static mesh. **Slang's
`f32tof16` / `f16tof32` are FORBIDDEN there**: they lower onto a 16-bit integer type, which
makes the module declare the SPIR-V `Int16` capability, and `vkCreateShaderModule` rejects that
without `VkPhysicalDeviceFeatures::shaderInt16` — a feature this engine does not require on any
backend or Android device. The 32-bit-integer-only half codec in that module is the replacement.

### Material System
Materials (`Zenith_MaterialAsset`) store textures and rendering properties. Located in `AssetHandling/`. Use `SetDiffuseTexture(TextureHandle(...))` when creating materials — the handle covers both path-based (serializable) and procedural-pointer textures. See `AssetHandling/CLAUDE.md` for details on material and texture asset management.

### Backend Abstraction
The renderer is backend-agnostic. `Flux_Backend.h` aggregates the seven C++20 concepts that any backend must satisfy: `FluxBackendDevice`, `FluxBackendMemoryAlloc`, `FluxBackendMemoryDelete`, `FluxBackendCommandRecorder`, `FluxBackendSync`, `FluxBackendPresentation`, and the shader/pipeline-builder family (`FluxBackendShader`, `FluxBackendPipelineBuilder`, `FluxBackendComputePipelineBuilder`, `FluxBackendRootSigBuilder`). `Backend/Flux_BackendConformance.cpp` static-asserts the active backend against each concept, so signature drift fails the build instead of the first frame. Adding a second backend (DX12 / Metal / WebGPU) means providing classes that satisfy each concept and adding the conformance asserts.

### Feature Registry & automatic shader hot-reload
`Flux_FeatureRegistry` (`Flux_FeatureRegistry.h/.cpp`) is the one table `Flux_RendererImpl` walks for init / render-graph setup / shutdown. Each `Flux_FeatureDesc` carries up to four captureless trampolines — `Initialise` / `SetupRenderGraph` / `Shutdown` / `BuildPipelines`. **A feature is added with exactly ONE call** — `RegisterFeature<&Zenith_Engine::Foo>(reg, "Foo")` — placed in render-graph declaration order inside `RegisterDefaultFeatures()`. That single call drives all three walks: **init** (registration order, forward), **setup** (the feature's `SetupRenderGraph` is auto-appended at the call site), and **shutdown** (reverse registration order, auto). The helper resolves each trampoline at compile time with a `requires` check, wiring only the methods `Foo` implements — so the former "irregulars" need no special casing: FluxGraphics omits `SetupRenderGraph`/`BuildPipelines`, DynamicLights omits both (gather/upload front-end), Fog has a no-op `Shutdown()` (RAII; its `BuildPipelines` rebuilds all fog techniques).

This works because init/shutdown are dependency-safe in ANY order beyond "FluxGraphics first" — every subsystem's `Initialise`/`Shutdown` touches only foundation (FluxGraphics/memory/backend) + its own state, never another feature. Only the **render-graph declaration order** is load-bearing (producer-before-consumer; see the ORDERING note in `Flux_FeatureRegistry.h`), and that's the order you write the calls in. Every entry in `RegisterDefaultFeatures()` is now a `RegisterFeature<>` call — no hand-written `AddSetupStep` irregulars remain. The former final-RT layout-transition step is now `Flux_Present::SetupRenderGraph` (the `Present` feature also owns the backend-neutral final-RT→backbuffer blit pipeline the swapchain records with), and the transient-creation step is `FluxGraphics::SetupRenderGraph`. `AddSetupStep` stays the primitive `RegisterFeature` uses to append a feature's setup, and remains available for out-of-tree (game) owners. (All shared cross-feature transients — G-buffer / depth / final-RT / HDR scene — are created up front by `FluxGraphics::SetupRenderGraph`, the first feature, which is why FluxGraphics owns the HDR scene target rather than HDR.) `RegisterFeature` is constrained on the `FluxRenderFeature` C++20 concept (a feature type exposes all four lifecycle methods — `Initialise` / `SetupRenderGraph` / `Shutdown` / `BuildPipelines`, a no-op stub where it has nothing to do), so passing a non-feature accessor — or a feature missing a method — is a clear compile error at the call site. **If a render system can't fit the one-liner, fix the render system** rather than adding bespoke registration.

**Each feature OWNS its shaders.** A feature declares its shader programs as `inline constexpr Flux_ShaderDecl` decls next to its code, in `Flux/<Feature>/Flux_<Feature>_Shaders.h`, plus one `apxALL[]` array listing them. That array is passed to `RegisterFeature`. There is **no central program enum and no registry row** — the pipeline-construction handle is `const Flux_ShaderDecl&` (resolved straight from the feature's `_Shaders.h`). `Flux_ShaderCatalog` (`Flux/Slang/Flux_ShaderCatalog.h/.cpp`) is the flat index = ∪ every feature's `apxALL` + a tiny `apxUnownedEnginePrograms[]` (engine programs no feature rebuilds). FluxCompiler walks the catalog to compile every program; codegen reads each decl's fields directly.

Shader hot-reload (ZENITH_TOOLS) is **derived from the feature table** — subsystems do *not* register themselves. `Flux_ShaderHotReload::AutoRegisterFeatures()` (called from `Flux::LateInitialise`) walks the registered features and wires every decl in each feature's `m_paxShaders` (its `apxALL`) to that feature's `BuildPipelines` callback, keyed by **decl identity** (`const Flux_ShaderDecl*`). Ownership is structural — no `m_szSubsystem`==feature convention, no override table. A decl's `m_szSubsystem` controls ONLY the generated-header grouping (so e.g. the out-of-tree `DevilsPlayground_DPFog` decl carries `m_szSubsystem` `"Fog"` and lands in `Generated/Fog.h` beside the engine's own fog programs, while no engine feature owns it — it sits in `apxUnownedEnginePrograms`).

> **Adding a renderer feature:** give the subsystem `Initialise`/`SetupRenderGraph`/`Shutdown`/`BuildPipelines` as needed; declare its programs in `Flux/Foo/Flux_Foo_Shaders.h` (named decls + `apxALL`); add ONE `RegisterFeature<&Zenith_Engine::Foo>(reg, "Foo", Flux_FooShaders::apxALL)` line at the right spot in `RegisterDefaultFeatures()` (render-graph order); and add ONE `#include "Flux/Foo/Flux_Foo_Shaders.h"` to `Flux_ShaderCatalog.cpp`'s block. **`Flux_ShaderCatalog::ValidateFeatureParity` (run at engine boot in all configs and by FluxCompiler) fails the build if you forget either line** — the catalog decl set must exactly equal (∪ registered-feature `apxALL`) ∪ unowned. Init, setup, shutdown, hot-reload, compile + codegen then all follow.

> **Modifying an existing feature's shader = 0 central edits:** edit the `.slang` and (if adding a program) add a decl + `apxALL` entry in the feature's own `_Shaders.h` + `Initialise(Flux_FooShaders::xX)`.

Out-of-tree owners (a game's own pass) call `Flux_ShaderHotReload::RegisterProgram(const Flux_ShaderDecl&, …)` / `RegisterSubsystem(…, const Flux_ShaderDecl* const*, …)` directly — after `AutoRegisterFeatures`. The `FluxCompiler` `shader-validation` CI workflow runs the canonical (Tools=True) FluxCompiler and fails if the catalog is invalid, parity breaks, or the checked-in generated tree is stale.

## Configuration

Key constants in `Core/ZenithConfig.h`:
- `FLUX_MAX_TARGETS` - Maximum render targets per pass
- `FLUX_MAX_BINDINGS_PER_GROUP` - Descriptors per shader
- `MAX_FRAMES_IN_FLIGHT` - Frame pipelining count

## Key Concepts

**Single-stage recording:** Each pass's `OnRecord` callback records native commands directly into a per-worker backend command buffer at `Execute()` time, in topological order, with graph-synthesized barriers emitted inline. There is no separate record-to-DSL + replay step.

**Multi-threaded Recording:** Passes are recorded in parallel — each worker records a contiguous topological slice of the pass list into its own command buffer (`FLUX_NUM_WORKER_THREADS`); the worker buffers are submitted in order, so order + inline barriers enforce dependencies.

**View Space Convention:** The engine uses **+Z forward** in view space (not -Z like OpenGL convention). When extracting linear depth from view-space positions, use `viewPos.z` directly without negation. See `Fog/CLAUDE.md` for depth reconstruction details.

**World Position Reconstruction:** Use `GetWorldPosFromDepthTex()` from `Shaders/Common/Frame.slang` for reconstructing world positions from the depth buffer - it handles the view/projection inverse transforms correctly.

## Design Rationale

### Why View Wrapper Structs Are Separate Types

The view types (`Flux_ShaderResourceView`, `Flux_UnorderedAccessView_Texture`/`_Buffer`, `Flux_RenderTargetView`, `Flux_DepthStencilView`, etc.) may appear duplicated since several share a near-identical structure (only `Flux_RenderTargetView` and `Flux_DepthStencilView` are truly identical — two members each; `Flux_ShaderResourceView` adds base-mip/mip-count fields, `Flux_UnorderedAccessView_Texture` adds a mip level, and the buffer variants use `BufferDescriptorHandle` instead of `ImageViewHandle`). Keeping them as separate types is intentional:

1. **Compile-Time Type Safety**: Separate types prevent accidentally passing an RTV where a DSV is expected. The compiler catches these errors, avoiding subtle runtime bugs.

2. **API Semantics Mirror Vulkan/D3D12**: The view types follow established graphics API conventions (D3D12's SRV/UAV/RTV/DSV pattern). Developers familiar with these APIs understand the semantic distinctions.

3. **View-Specific Extensions**: Separate types let each view carry the fields it actually needs (e.g., base-mip/mip-count on SRVs, a mip level on texture UAVs) and allow adding more later (e.g., array slices for RTVs) without breaking existing code.

4. **No Runtime Overhead**: The separate types add zero runtime cost - they're just naming conventions that the compiler optimizes away.

5. **Binding Function Overloads**: The command buffer's `BindSRV()`, `BindUAV()`, `BindRTV()` methods take specific view types, ensuring correct binding at compile time.

### Direct recording replaced the command-list DSL

Render systems used to record into a deferred `Flux_CommandList` byte-buffer DSL
(`AddCommand<Flux_CommandX>()`) that the backend later replayed via `IterateCommands`.
That DSL existed to "trivialise multithreading and synthesize barriers" — both of which the
render graph now owns (topological sort + `SynthesizeBarriers`), making the DSL pure
indirection. It was removed: passes now call the backend command-recorder methods directly
(the 1:1 targets the command classes used to forward to), recorded inside the dependency-
ordered worker stage. To add a new GPU operation, add the method to the
`FluxBackendCommandRecorder` concept + each backend, and call it from the pass callback.

### Buffer Wrapper Classes (Consolidated)

The buffer wrapper classes (`Flux_VertexBuffer`, `Flux_DynamicVertexBuffer`, etc.) are already consolidated onto two template bases in `Flux_Buffers.h`: `Flux_SingleBufferBase<TView>` (one buffer + optional view) and `Flux_FrameIndexedBufferBase<TView>` (per-frame-in-flight buffer/view arrays), each with a `Flux_NoView` specialization. The eight concrete types are thin leaves whose only remaining content is the domain-specific accessor names (`GetCBV`/`GetUAV`/`GetSRV`) — these forward to the base on purpose so the command-buffer binders stay type-safe at call sites and reject e.g. a vertex buffer passed as an index buffer. The two `Reset` overrides on `Flux_ReadWriteBuffer` / `Flux_DynamicReadWriteBuffer` exist to also clear their SRV-mirror views, which live outside the base's `TView` slot. This is intentional type-safety plumbing, not removable duplication — **not** a simplification candidate.
