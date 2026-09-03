# Shader Coding Style Guide

Conventions for Slang shader source files in `Zenith/Flux/Shaders/`. Slang is
the only supported source language post-migration; the legacy GLSL paths
(`.vert`/`.frag`/`.comp`/`.fxh`) and the `enableGLSL` Slang flag have been
removed.

> The **Flux Shader System Overhaul** restructured these files around Slang's
> real capabilities (compile-error access control on the descriptor spine,
> `interface`/`extension` seams, generics, `[SpecializationConstant]` folding,
> Slang debug info). The sections below are the resulting living conventions;
> they are **enforced in code** (FluxCompiler gates + boot asserts), not just
> style advice.

## File Naming

- `Flux_<Subsystem>_<Purpose>.slang` (e.g., `Flux_UnifiedMesh_ToGBuffer.slang`).
- One `.slang` mega-file per subsystem with multiple named entry points where
  the variants are closely related.
- Shared modules live under `Common/`. Most are pulled with `import`
  (`Common.PBR`, `Common.GBuffer`, `Common.Material`, `Common.MaterialSurface`,
  `Common.Lighting`, …); a few are textual `#include`s (`Common/Bindings.slang`
  and the other spine-dependent includes — see **Include vs Import** below).

## Entry Points

- Vertex entry: `vsMain`. Fragment entry: `fsMain`. Compute entry: `csMain`.
  (Slang uses `fsMain`, not the HLSL `psMain`. The whole tree is 46 `vsMain` +
  46 `fsMain` + 16 `csMain`; there are no `vsMainStatic`-style suffixed variants.)
- Programs are **owned by their feature**, not a central registry. A feature
  declares each program as an `inline constexpr Flux_ShaderDecl` in
  `Flux/<Feature>/Flux_<Feature>_Shaders.h` (module path + entry-point names +
  metadata), listed in that feature's `apxALL[]`. `Flux_ShaderCatalog`
  (`Flux/Slang/Flux_ShaderCatalog.{h,cpp}`) is the flat index over every
  feature's `apxALL`; FluxCompiler walks it to compile + codegen. The
  pipeline-construction handle is `const Flux_ShaderDecl&` — there is **no**
  central program enum and **no** `Flux_ShaderRegistry` (both retired). See
  `Flux/CLAUDE.md` → *Feature Registry*.
- Slang's per-entry SPIR-V emitter renames each entry point to `main` in the
  emitted blob; the engine binds the Vulkan-visible name `"main"` regardless of
  the source-level name.

## Variable Naming (Hungarian Prefix)

| Prefix | Scope / Type | Example |
|--------|-------------|---------|
| `g_` | Global parameter (uniform / buffer / texture / sampler) | `g_xView`, `g_axTextures` |
| `a_` | Vertex stage input | `a_xPosition`, `a_xUV` |
| `o_` | Inter-stage varying output | `o_xNormalWS`, `o_fBitangentSign` |
| `x` | vec / mat / struct | `xWorldPos`, `xNormalMatrix` |
| `f` | float | `fDepth`, `fRoughness` |
| `u` | uint | `uMipLevel`, `uInstanceID` |
| `i` | int | `iCascade` |
| `b` | bool | `bUseVAT` |
| `ax` | array of vec/struct | `axOffsets`, `axShadowMats` |

## The frequency-set spine + accessor facade

The binding model is a frequency-based spine, declared **once** in
`Common/Bindings.slang` (text-`#include`d first by every shader) and **enforced
in code** (`Flux_ShaderCatalog::ValidateFrequencyTaxonomy`,
`Flux_PersistentSetLayouts::ValidateCanonicalGroup`, `Flux_ViewSetBinding`):

- **Set 0 — GLOBAL**: view-invariant per-frame data (`g_xGlobal` + the GPU
  material table `g_axMaterials`).
- **Set 1 — VIEW**: per-camera data + promoted view-frequency resources
  (`g_xView`, the CSM array, shadow matrices, clustered-lighting buffers, IBL —
  see `Flux/Flux_PersistentSetLayouts.h`).
- **Set 2 — BINDLESS**: the unbounded `g_axTextures[]` table.
- **Set 3 — PASS / Set 4 — DRAW**: each shader's own local `ParameterBlock`s,
  declared **after** the `#include` so they land at the next free spaces.

Shaders carry NO `[[vk::binding]]` — every resource is a member of its frequency
block, and the descriptor set is assigned by declaration order (which is why the
spine must be `#include`d first; see below).

**Accessor rule (do not poke the spine blocks directly).** Feature shaders reach
the GLOBAL / VIEW / BINDLESS sets **only** through the free-function accessor
facade at the bottom of `Common/Bindings.slang`. The raw `g_xGlobalSet` /
`g_xViewSet` / `g_xBindlessSet` `ParameterBlock` instances **never** appear
outside `Bindings.slang`:

```slang
float3   xCam    = GetCamPos();                 // not g_xViewSet.g_xView.g_xCamPos_Pad.xyz
float4x4 xVP     = GetViewProjMat();
float3   xSun    = GetSunDir();                 // sun is per-view (VIEW set)
if (ViewShadowsEnabled()) { ... }               // predicate over g_uViewFlags
float4x4 xShadow = GetShadowMatrix(iCascade);   // structured-buffer element load
MaterialGPU m    = GetMaterial(dc.g_uMaterialIndex);
Sampler2D  xTex  = GetBindlessTexture(m.g_xTexIdx0.x);
```

Each accessor is a trivial pure forwarder, so substituting it for a former
direct poke is referentially transparent (behaviour-preserving). **Sampler
accessors return the sampler**, not a pre-baked sample, so the call site keeps
its exact `.Sample` / `.SampleLevel` / `.Gather` form:
`GetCSM().Gather(float3(uv, cascade))`.

This rule is **enforced by the FluxCompiler spine lint** (`Flux/Slang/Flux_SpineLint.h`,
run in `FluxCompiler main()` — a hard `return 1` on any violation). Its three
rules: (1) no spine-block poke outside `Bindings.slang`, (2) no `extension
GlobalParams|ViewParams|BindlessParams` outside `Bindings.slang`, (3) no
`ParameterBlock<spine-struct>` redeclaration. To expose a new spine field, add
the field to the layout struct **and** an accessor in `Bindings.slang`; grow the
GLOBAL/VIEW blocks there in lockstep with `Flux/Flux_PersistentSetLayouts.h`.

**Matrix layout:** `defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR`
is set at session creation so HLSL-syntax matrices match the std140 GLM-uploaded
layout. Don't transpose at the call site.

## Include vs Import

Slang gives you two ways to pull shared code. They are **not** interchangeable
here — the choice is dictated by whether the file participates in descriptor-set
assignment or in the textual accessor scope:

| Use `import Common.Foo;` | Use `#include "Common/Foo.slang"` |
|---|---|
| The module declares **no** `ParameterBlock` and needs no textually-included symbols (`Common.PBR`, `Common.GBuffer`, `Common.Material`, `Common.MaterialSurface`, `Common.Lighting`, `Common.DrawConstants`, `Common.Fullscreen`, …). | The file **declares** spine/PASS/DRAW `ParameterBlock`s whose set assignment depends on declaration order (`Common/Bindings.slang`, `Common/UnifiedMeshDraw.slang`); **or** it calls the spine free-function accessors and so must sit in the same textual scope after `Bindings.slang` (`Common/ShadowSampling.slang`); **or** it declares textually-scoped globals that must land in the consuming shader — e.g. `[SpecializationConstant]` (`Common/ViewModes.slang`, whose IDs are assigned relative to the shader that inlines it). |
| Order-independent — a first-class module. | **Order-load-bearing / scope-bound** — `import` would reorder local-vs-imported declarations and shift the spine's set 0/1/2 assignment, put the accessor free functions out of scope, or namespace-wrap the spec-constant globals. |
| No include guard (modules are first-class units). | Wrap in a `#ifndef FLUX_COMMON_<NAME>_SLANG` header guard (textual includes can be pulled more than once transitively). |

Rule of thumb: **`#include "Common/Bindings.slang"` first**, then the other
textual includes, then `import` the binding-free helper modules, then declare the
shader's own PASS/DRAW blocks. A module that adds a PASS/DRAW block or reads the
spine accessors must be an `#include`, never an `import`.

## Specialization Constants

Compile-time-specializable branches use Slang `[SpecializationConstant]`. The
canonical case is the view-mode **permission mask** in `Common/ViewModes.slang`
(included only by the two lit programs):

```slang
[SpecializationConstant] const bool FLUX_SC_VIEW_SHADOWS_PERMITTED        = true;
[SpecializationConstant] const bool FLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED = true;
```

Fold them as a permission mask **over** the retained runtime flag —
`if (FLUX_SC_VIEW_SHADOWS_PERMITTED && ViewShadowsEnabled()) { … }`. Default
`true` ⇒ codegen identical to the pre-fold shader (safe to land shader-side
alone); a variant pipeline that bakes `false` constant-folds the whole clause
away (e.g. the BASIC-mode pipeline strips CSM/cluster sampling for cascade +
preview views).

Rules:
- **IDs are auto-assigned in declaration order** and **resolved by name** through
  reflection (`Flux_ResolveSpecConstants`) — C++ **never** hardcodes an ID, so a
  hot-reload that renumbers them stays correct. The generated header emits an
  `hsc<Name>` handle per program; features call `xSpec.m_xSpecConstants.AddBool(handle, value)`.
- Reflection **drops** spec constants from the binding table (they occupy no
  descriptor), so they are neutral to all four binding gates. The reflection
  sidecar is **v5** (`kFluxReflectionVersion`), which appends the spec-constant
  table after the bindings.
- Vulkan attaches one `VkSpecializationInfo` per pipeline via a **local** copy of
  the shader's stage array (never mutate the shader's shared `m_xInfos`).

## Interface / Extension Seams

Where a subsystem has a genuinely swappable strategy, express it as a Slang
`interface` in `Common/` and let each subsystem conform its own type via
`extension` **without editing Common**. Two live seams:

- **Surface model** — `ISurfaceModel { MaterialSurface Evaluate(); }` +
  `UberMaterialSurface` in `Common/MaterialSurface.slang`. Terrain conforms
  `TerrainSplatSurface` via `extension` inside `Flux_Terrain_ToGBuffer.slang`
  (its 4-material splat blend), with the G-buffer packing staying outside.
- **Shadow filter** — `IShadowFilter { bool EvaluateCascade(int iCascade,
  float3 xWorldPos, float3 xNormal, float fNdotL, out float fShadow); }` +
  `ShadowFilterPCSS` (the 16-tap Vogel PCF + PCSS tier) in
  `Common/ShadowSampling.slang`. The translucent pass declares a local
  `ShadowFilterSimple4Tap` and conforms it via `extension` inside its own shader
  — the subsystem-local proof that a filter plugs in with zero Common edits. Each
  consumer keeps its **own** cascade walk (selection + cross-fade); they differ
  deliberately.

Critical rule: use an interface as a **generic / extension constraint only,
never as an existential value** (`ISomething xThing = …`). An existential
changes reflection; a constraint does not. A conformance that is declared but not
called is dead-stripped (zero artifact churn) — that is fine, and is exactly how
a "declare-and-conform" showcase proves the seam compiles.

## Generics

Kill byte-identical-by-comment duplication with generics + a policy interface,
again used as a **constraint only**. The unified-mesh shell family
(`Common/UnifiedMeshDraw.slang`) shares one `TransformUnifiedVertex` (the VAT +
world transform — the foliage-self-shadow contract, now structural not a comment)
and one `EmitUnifiedFragment<TPolicy : IUnifiedMeshOutput>` fragment core; each
shell supplies a compile-time `GBufferPolicy` / `GBufferVelocityPolicy` for its
own MRT packing. Helpers take the per-draw pass data (resolved scene object,
draw constants, VAT sampler) as **parameters** and never poke `g_xPassSet` /
`g_xDrawSet`, so each shell keeps its own PASS/DRAW blocks at spaces 3/4 →
reflection-layout identity is preserved.

## Slang Debug Info (RenderDoc)

Runtime **Debug** builds compile shaders with Slang debug info
(`m_bEmitDebugInfo`, set under `#ifdef ZENITH_DEBUG` in
`Zenith_Vulkan_Shader::InitialiseFromProgramSource`), so a RenderDoc capture
shows Slang source. Debug info is additive `NonSemantic` SPIR-V — execution- and
pixel-invariant. `--shader-debug-o0` additionally disables optimization (opt-in;
it changes float math, so it is **not** paired by default). FluxCompiler never
sets either flag, so the **checked-in `.spv` artifacts are byte-identical by
construction** regardless of the local build's debug-info state.

## Compute Shader Documentation

Every compute entry point should include a banner comment:
```slang
// ============================================================================
// <Purpose — one line>
// Workgroup: <NxMxK> (<thread count> threads — <rationale>)
// Shared memory: <size or "none">
//
// <2-3 sentence description of what the shader does>
// ============================================================================
[numthreads(N, M, K)]
void csMain(...) { ... }
```

Use `[numthreads(...)]` (HLSL syntax). For texture sampling inside compute,
prefer `SampleLevel(uv, mip)` over `Sample(uv)` — implicit derivatives in a
compute thread require `ComputeDerivativeGroupQuadsKHR` and Slang will fail to
lower without it.

## Vertex Input Formats (`[VtxFmt]` / `[PerInstance]`)

**A `VsIn` struct IS the vertex layout.** Nothing hand-writes a CPU-side attribute
table: Slang reflection reports what the `VsIn` declares, FluxCompiler bakes that
into `Generated/<Subsystem>.h` as `kaxVertexAttribs[]` / `kVertexLayout`, the CPU
side packs against those constants, and pipeline construction builds its vertex
input from the LIVE reflection. See `Flux/CLAUDE.md` → *Vertex Layouts*.

Both attributes are declared in **`Common/VertexFormats.slang`** — `#include` it
(textually, never `import`: an attribute must resolve by name in the scope of the
shader that spells it, and an imported module namespace-wraps it out of that scope).

```slang
#include "Common/VertexFormats.slang"  // [VtxFmt] / [PerInstance] authoring attributes

struct VsIn
{
	float3 a_xPosition : POSITION;                                   // untagged -> inferred float3, 12 B
	[VtxFmt("half2")] float2 a_xUV : TEXCOORD0;                      // 4 B stored, float2 declared
	[VtxFmt("snorm10_10_10_2")] float4 a_xNormal : NORMAL;           // 4 B stored, float4 declared
	[PerInstance] float2 a_xOffsetPx : TEXCOORD1;                    // binding 1, 8 B
	[PerInstance] [VtxFmt("unorm8x4")] float4 a_xColour : TEXCOORD2; // binding 1, 4 B
};
```

- **Un-annotated fields are inferred from the declared type** (`float2` → 8-byte
  `FLOAT2`, and so on). Tag a field **only** when its storage format differs from
  what it is declared as — which it must be whenever fetch hardware widens or
  converts. A four-byte SNORM10:10:10:2 attribute is still declared `float4`;
  without the tag the generator infers a sixteen-byte `float4` and bakes a wrong
  stride.
- **Legal format strings are exactly the ones `Flux_ParseVertexFormatString`
  accepts** (`Flux/Slang/Flux_SlangCompiler.cpp`) — currently `half2`, `half4`,
  `snorm16x2`, `snorm16x4`, `unorm16x2`, `unorm16x4`, `unorm8x4`, `uint8x4`,
  `uint16x4`, `snorm10_10_10_2`. A typo is a **hard compile error**, never a silent
  fall-back to the declared type.
- **`[PerInstance]`** moves the field onto binding 1 (the per-instance stream);
  everything untagged stays on binding 0. There are exactly two bindings
  (`uFLUX_MAX_VERTEX_BINDINGS`).
- **The semantic vocabulary is CLOSED** — `POSITION`, `TEXCOORD`, `NORMAL`,
  `TANGENT`, `BINORMAL`, `COLOR` (`FluxVertexSemantic` in
  `Flux/Flux_VertexLayoutDesc.h`). A semantic outside that list is a **hard codegen
  error**, not an invented tag; adding one means an enumerator + a table row in that
  header, in the same change as the shader that declares it.
- **Declaration order in the struct is the byte order in the buffer.** An offset is
  the running sum of the storage sizes *of the fields ahead of it on the same
  binding* — tight-packed, no padding inserted, and the two bindings count
  independently. So reordering fields, or moving one across the `[PerInstance]`
  split, is a layout change even when no format changed.

> **★ CHANGING A `[VtxFmt]` REQUIRES A FluxCompiler RUN + AN ENGINE RESTART. IT IS
> NOT COVERED BY SHADER HOT-RELOAD.** Hot-reload rebuilds pipelines from the new
> reflection, but the **packed CPU-side buffers are not re-packed** (meshes, terrain
> chunks and instance streams were interleaved when they were built or baked), and
> the committed `Generated/` header does not move either. Rerun FluxCompiler
> (`Vulkan_vs2022_Release_Win64_True`), commit the regenerated tree, restart. If you
> skip it, the boot tripwire
> (`Flux_AssertVertexLayoutMatchesReflection`, called from every backend's
> `FromSpecification`) fires and **names the program** whose generated table went
> stale.

Manual codecs — `Flux_DequantPosition` and the half/snorm/unorm packers — also live
in `Common/VertexFormats.slang`, for the buffers no fetch unit touches (the compute
skinner reads and writes raw `u32` SSBO words). They must stay bit-for-bit the CPU
`Flux/Flux_VertexCodec.h`. **Never use `f32tof16` / `f16tof32`**: Slang lowers them
onto a 16-bit integer type, the module then declares the SPIR-V `Int16` capability,
and `vkCreateShaderModule` REJECTS it unless `shaderInt16` is enabled — a device
feature this engine does not require. Use the 32-bit-integer-only conversions in
that module instead.

## Texture Decodes Have One Site Each

**Never open-code the unpacking of a texture whose on-disk encoding is a policy
decision made elsewhere.** The encoding is chosen by the exporter — for normal
maps, per texture, in an asset root's `TextureUsage.ztexdecl` (see
`Zenith/AssetHandling/CLAUDE.md`) — so a shader that spells the decode itself is
a second copy of a contract it cannot see.

| Decode | The one site |
|---|---|
| Tangent-space normal map | `Common/Material.slang`'s **`UnpackNormalMapTS`** (`SampleNormalMap` is the TBN-applying convenience over it) |
| Vertex attribute dequant | `Common/VertexFormats.slang` (`Flux_DequantPosition`, the half/snorm/unorm helpers) — bit-for-bit `Flux/Flux_VertexCodec.h` |

`NORMAL_MAP` textures are exported **BC5**: two channels, R+G, with
`Z = sqrt(1 - x² - y²)` rebuilt at sample time. `UnpackNormalMapTS` does that, and
is also correct for the RGBA8 default-normal fallback (its stored B is ignored).

> **★ THIS RULE WAS BOUGHT.** The two terrain G-buffer shaders open-coded
> `Sample(uv).xyz * 2.0 - 1.0`, which is right for a three-channel map and wrong
> for BC5 — blue reads 0, so Z decodes to **−1** and the shading normal points
> INTO the surface. When the exporter started writing BC5, `NdotL` went to zero on
> every terrain pixel, the deferred pass skipped its whole CSM branch (it only
> resolves a shadow when `NdotL > 0`), and the terrain stopped receiving shadows
> game-wide. Nothing failed to compile, no test went red, and the symptom surfaced
> days later as "the houses stopped casting shadows onto the ground". The terrain
> shaders now share `UnpackNormalMapTS` — they cannot use `SampleNormalMap`
> itself, because they blend four layer normals before applying the TBN, but the
> part that has to agree with the exporter is shared. See
> `Docs/design/Photorealism.md` §1.10.
>
> There is **no lint for this yet**: nothing stops the next shader open-coding a
> normal sample. A FluxCompiler check rejecting a raw `Sample(...).xyz * 2.0 - 1.0`
> outside `Common/Material.slang`, in the shape of the existing spine lint
> (`Flux/Slang/Flux_SpineLint.h`), would close it.

## Vertex-Stage Buffer Reads

Storage-buffer reads from a vertex shader require Vulkan
`vertexPipelineStoresAndAtomics` if Slang emits them as RW. Use
`StructuredBuffer<T>` (read-only — Slang emits `NonWritable`) instead of
`RWStructuredBuffer<T>` whenever the vertex stage only reads the buffer.

## Code Style

- Tabs for indentation (matching C++ convention)
- Opening braces on the same line for functions and control flow
- Constants as `static const` with descriptive names
- Helper functions live in shared modules under `Common/`; prefer importing (or
  `#include`ing, per the decision table) over inline duplication
