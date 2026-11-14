# Zenith2 rendering: shader system and pipeline state objects

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file sections "Shader system and offline compilation" and "Pipeline state objects". Normative statements share the `REND` tag family of the entry file.

## Shader source layout

```
Zenith2/Shaders/
	Common/          modules shared by every program
		Bindless.slang         heap accessors: Flux_TextureSRV<T>, Flux_TextureUAV<T>, Flux_BufferSRV, Flux_BufferUAV; D3D12 space bases
		Params.slang           Flux_PushConstants, Flux_LoadParams<T>(Flux_PushConstants): the push constant every entry point takes as its uniform parameter; the D3D12 draw-ID root constant at b1, space0, its one register, declared for the D3D target only
		DrawID.slang           Flux_DrawIndex, Flux_DrawRecordIndex, Flux_InstanceIndex; declares no binding (Flux_DrawIndex reads Params.slang's draw-ID constant on D3D12)
		DepthMode.slang        depth-mode abstraction: clear values, comparisons, linearisation
		VertexFormats.slang    the GPU half of the vertex codec
		Half.slang             integer-only half-float codec
		Material.slang         Flux_MaterialInput, Flux_SurfaceData, Flux_MaterialType, EvaluateMaterial
		GPUScene.slang         GPU scene record layouts and accessors
		Sampling.slang         Flux_SampleMaterial and explicit-gradient helpers
		VirtualTexture.slang   module VirtualTexture: Flux_VTBeginInvocation, Flux_VTPoolsBound, Flux_SampleVirtual, Flux_VTRecordRequest, Flux_VTWriteFeedback (internal invocation state; Stage 4)
		Octahedral.slang       octahedral encode/decode
		Random.slang           counter-based hashes for noise and jitter
		Culling.slang          Flux_TransparentEntry, Flux_FrustumPlanes, Flux_SlotCommandRecord, Flux_SlotCommandScatter, Flux_CullSurvivor, Flux_CullCounters
		HiZTest.slang          the occlusion-test projection and mip selection (instance, meshlet and transparent culling, the HiZ overlay)
		MeshVertex.slang       the family vertex structs, Flux_TransformObjectToClip, the material vertex-modifier call
		StreamSetFields.slang, StreamSets.slang   the [StreamSet(<code>)] declarations of every reachable stream-set (GEOM)
		VertexKernel.slang     the exact arithmetic kernel of the vertex codec (GEOM)
		Meshlets.slang, LOD.slang, Terrain.slang, Procedural.slang   GEOM's shared records
		Probes.slang, Media.slang, GI.slang   LIGHT's shared records
		Environment.slang      ENV's shared records, Env_FrameEnvironment
		Skinning.slang         Flux_SkinDispatchRecord
		Particles.slang        Flux_ParticleState, Flux_EmitterCommand, Flux_ParticleDrawRecord, Flux_ParticleSortPair, …
	Lighting/        EvaluateLighting.slang, BRDF.slang, Clusters.slang, Shadows.slang, IBL.slang, Fog.slang, Decals.slang
	Particles/       modules imported by particle programs: interfaces, random, curves, noise, modules, collision, frame mapping (VFX)
		Types/       the engine particle update and shading types (VFX)
	Materials/       engine materials: Standard.slang + Standard.material.json, Unlit.slang + Unlit.material.json, Fallback.slang (magenta)
	Passes/<Feature>/<Program>.slang   every file under Passes/ is one program
	Tests/<Feature>/<Program>.slang    test-only programs (render tests, probes, the path tracer); never packaged
	ShaderChecks_allowlist.json         decision-backed lint exceptions
```

Game materials live beside the game's content: `Zenith2/Games/<Name>/Assets/Materials/<Material>.slang` with its `<Material>.material.json`.

- [Z2-N-REND-105] Every `.slang` file under `Zenith2/Shaders/Passes/` and `Zenith2/Shaders/Tests/` is exactly one program and every file under `Zenith2/Shaders/Common/`, `Zenith2/Shaders/Lighting/`, `Zenith2/Shaders/Particles/` and `Zenith2/Shaders/Particles/Types/` is a module imported by programs; a module under those directories declares no entry point, and a program imports no other program.
- [Z2-N-REND-106] Games contribute shader code exclusively as material modules under `Zenith2/Games/<Name>/Assets/Materials/` and as code-authored particle update and shading types (implementing `Flux_ParticleUpdateType` or `Flux_ParticleShadingType`) under `Zenith2/Games/<Name>/Assets/VFX/`; a game declares no pass program, because passes are added by C++ feature functions and games contain no C++ (Z2-D-002).
- [Z2-N-REND-107] A program's stable ID is the 64-bit hash (selected by D-SEL-HASH (open, Stage 1)) of its shader-root-relative path without extension (`Passes/GTAO/GTAO`, or `Games/<Name>/Materials/<Material>` for a game material module), so renaming or moving a program changes its ID and invalidates the persistent pipeline caches' entries for it.

## The ZenithShaderCompiler tool

`ZenithShaderCompiler` is a Windows host tool built from `Zenith2/Tools/ZenithShaderCompiler/`. It is the one process that links Slang and DXC (see Z2-N-REND-004 in the entry file).

### Command line

```
ZenithShaderCompiler
	--shader-root <dir>          repeatable; Zenith2/Shaders and each selected game's Assets/Materials
	--program <root-relative>    repeatable; default: every program under Passes/ and Tests/
	--material-set <file>        cook mode: the game's material-variant set to compile material programs against
	--particle-type-set <file>   cook mode: the game's particle type set (written by the vfx.particleTypes cook step, before rprod.shaders) to compile particle programs against
	--stream-set-list <file>     cook mode: the game's stream_sets.json; raster programs compile for its codes and those of Zenith2/Shaders/StreamSets.list
	--stream-sets                the aggregate stream-set command: writes Flux/Shaders/Flux_StreamSets.generated.h (see "Stream-set declarations")
	--target spirv|dxil|all      default all on Windows builds, spirv for android presets
	--tier desktop|mobile|all    heap base offsets and tier-dependent permutations
	--config debug|release       debug: no optimisation, embedded debug info; release: optimised, debug info in a sidecar
	--out-dir <dir>              blob containers, reflection JSON, lint JSON
	--header-dir <dir>           generated <Name>.generated.h headers
	--depfile <file>             Ninja-format depfile listing every source the program read
	--manifest <file>            compile manifest: every program, permutation key, blob hash
	--container-magic <u32>      required: the container magic written at offset 0 ([Z2-N-REND-994])
	--container-version <u32>    required: the container version written at offset 4 ([Z2-N-REND-994])
	--no-lint                    skip shader lints (never used by gate steps)
	--check                      verify outputs are current; compiles nothing
	--watch                      file-watch mode (see "Watch mode and editor reload")
	--reload-journal <file>      watch mode: the editor reload journal to append to
	--jobs <N>                   parallel compile jobs; default: hardware threads
	--version                    print the tool version and the pinned Slang and DXC versions, then exit
```

| Exit code | Meaning |
|---|---|
| 0 | success; every requested output is current |
| 1 | one or more compile errors (each printed as `file(line,col): error ZSC####: message`) |
| 2 | command-line usage error |
| 3 | `--check`: at least one output is stale; the stale outputs are listed |
| 4 | one or more lint failures |
| 5 | a program's permutation count exceeds `Z2-B-RPROD-024` Shader permutation limit per game |
| 6 | internal error (a tool defect, never a content error) |

Each compile error carries a stable `ZSC_ERROR_*` code of the compiler's error enumeration (`Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes" holds the full list); the stream-set codes are in "Stream-set declarations" below. The push-constant code:

| Compiler error | Raised when |
|---|---|
| `ZSC_ERROR_PUSH_CONSTANTS` (24) | an entry point whose reflected `uniform` parameters (`EntryPointReflection::getVarLayout`) are not exactly one 8-byte `Flux_PushConstants` at offset 0, laid out as the one SPIR-V push-constant block or the one D3D constant buffer at `b0, space0`; names the program, the entry point, the target and the reflected size and location (`Zenith2/Docs/Rendering.md` → "Binding model" [Z2-N-REND-061], [Z2-N-REND-998]) |

### Inputs and outputs

| Input | Output (under `--out-dir` unless stated) | Consumer |
|---|---|---|
| a program `.slang` and every module it imports | `Blobs/<ProgramPath>.zshader` (all permutations, both APIs), under the container root `${ZENITH_OUT}/generated/ZenithShadersGenerated/Blobs/` for the build commands ([Z2-N-REND-108]) | Flux runtime loader, the cook, tests through `ZENITH_SHADERS_GENERATED_DIR` |
| the same, when the program has a `[PermutationBuild("development")]` constant | `DevelopmentBlobs/<ProgramPath>.zshader`: the development permutations alone, in the container format with the program's ID, layout hash and tool hash ([Z2-N-REND-993]) | development tests and benchmarks through `ZENITH_SHADERS_GENERATED_DIR`; never the cook or the PSO precache predictor |
| the same | `Reflection/<ProgramPath>.reflection.json` | the cook, the editor, gate steps, tests |
| the same | `<header-dir>/Flux/Shaders/<ProgramName>.generated.h`, with `<header-dir>` = `${ZENITH_OUT}/generated/ZenithShadersGenerated/` | C++ feature code through `ZenithShadersGenerated` |
| the same | `Lint/<ProgramPath>.lint.json` | the ShaderChecks gate step |
| the same | `--depfile` | Ninja incremental rebuilds |
| all programs of one invocation | `--manifest` (`shaders.manifest.json`) | the cook, the PSO precache predictor |
| the CMake values `ZENITH_SHADER_CONTAINER_MAGIC` and `ZENITH_SHADER_CONTAINER_VERSION` (written by `zenith_add_shaders` at configure time, not by the compiler) | `<header-dir>/Flux/Shaders/Flux_ShaderContainerFormat.generated.h` ([Z2-N-REND-994]) | the compiler's callers, `Flux_ShaderLibrary`, `Flux_PipelineCacheStore`, RHI's test reader |
| every `[StreamSet]` struct under `Zenith2/Shaders/Common/` (`--stream-sets`) | `<header-dir>/Flux/Shaders/Flux_StreamSets.generated.h` | runtime PSO creation, the cook's packer and stream-set choice, the mesh reader |
| every material type of the shader roots (cook mode) | `Reflection/Materials/<ModulePath>/<Type>.material.json` (schema version 1, `Zenith2/Docs/Design/GEOM/Materials.md → "Material reflection input"`) | the material cook's document validation |

- [Z2-N-REND-108] The shader-compile custom commands belong to `ZenithShadersGenerated`: `Zenith2/Engine/ZenithShadersGenerated/CMakeLists.txt` is the one helper call `zenith_add_library(ZenithShadersGenerated KIND GENERATED DEPENDS ZenithCore ZenithVertexFormat ZenithRHI SHADERS Zenith2/Shaders/)`, which adds one command per program running `ZenithShaderCompiler` with `--program`, `--depfile`, `--out-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/`, `--header-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/` and the preset's `--config`, `--target` and `--tier`, plus the one shared-types command of "Shared GPU structs"; each program command's outputs are the program's blob container, reflection, lint file and generated header (and its development container when it has a `[PermutationBuild("development")]` constant, [Z2-N-REND-993]), and no other `CMakeLists.txt` (in particular not `ZenithFlux`'s) declares a shader command. The build tree's container root is therefore `${ZENITH_OUT}/generated/ZenithShadersGenerated/Blobs/`, beside the generated headers: a program's container is `Blobs/<ProgramPath>.zshader` below the directory every test, conformance, benchmark and integration-suite target receives as the compile definition `ZENITH_SHADERS_GENERATED_DIR` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets", [Z2-N-ARCH-254]). A test reads a built container only through that definition (`Zenith_ReadRepoFileAt(ZENITH_SHADERS_GENERATED_DIR, …)` with the relative path `Blobs/<ProgramPath>.zshader`), never through a path of its own, and a Player reads containers only from the cooked package ([Z2-N-REND-123]).
- [Z2-N-REND-109] Android presets run the host `ZenithShaderCompiler` found through `ZENITH_HOST_TOOLS_DIR` and emit SPIR-V and the mobile tier; no Android target builds the compiler.
- [Z2-N-REND-110] The compiler is deterministic: identical inputs, flags and tool versions produce byte-identical outputs, and generated headers are written only when their content changes, so an unchanged shader never triggers a C++ rebuild.
- [Z2-N-REND-111] The compiler invokes Slang with SPIR-V 1.6 (Vulkan 1.3) and DXIL Shader Model 6.6 through DXC, row-major matrix layout, scalar layout on Vulkan, `-enable-16bit-types` for DXIL, and warnings as errors.
- [Z2-N-REND-112] `--check` exits 3 and lists every stale output whenever any output is older than an input it depends on or was produced by a different tool version; the ShaderChecks gate step runs it after the Build step to prove the committed build graph produced every blob.

## Slang authoring vocabulary

| Attribute | Applies to | Meaning |
|---|---|---|
| `[FluxParams]` | one struct per program | the program's parameter struct; the generator emits `Flux_<Program>Params` from it |
| `[FluxShared]` | a struct in a `Common/` module | a GPU record CPU code also reads or writes; the generator emits it into `Flux_SharedTypes.generated.h` (see "Shared GPU structs") |
| `[FullCoverage]` | a UAV field | the pass writes every texel or byte of the view it names, so the write may be a transient's first access |
| `[WriteOnly]` | a UAV field | the pass never reads the view; with `[FullCoverage]` the previous contents are discardable |
| `[Stages(Vertex, Pixel, Compute, Mesh, Task)]` | a resource field | narrows the field's stage mask (the default is every stage of the program) |
| `[Optional]` | a resource field | the field may be left unset; the graph writes descriptor slot 0 (the null substitute) and derives no access |
| `[Permutation(v0, v1, …)]` | an `extern static const` | a permutation constant and its value set; the first value is the default |
| `[PermutationRequires("bPERM_X", value)]` | a permutation constant | the constant takes its non-default values only when `bPERM_X` equals `value` |
| `[PermutationValueRequires(v, "C", w)]` | a permutation constant, repeatable | the constant takes the value `v` only when constant `C` equals `w`; several on one value all apply |
| `[PermutationBuild("development")]` | a program-level permutation constant | the constant's non-default values are compiled only into the program's development container `DevelopmentBlobs/<ProgramPath>.zshader`, never into `Blobs/<ProgramPath>.zshader` and never in cook mode, so no cooked package holds them ([Z2-N-REND-993]); `development` is the one accepted argument |
| `[AlphaTest]` | an entry point | the entry point is compiled into permutations where `bPERM_ALPHA_TEST` is true and is the one place `discard`/`clip` may appear |
| `[VtxFmt("…")]`, `[Stream(n)]`, `[PerInstance]` | vertex-input fields | storage format, stream index, instance rate (see `Zenith2/Docs/Rendering/VertexData.md` "Vertex declaration in Slang") |
| `[TileLocal]` | a resource field | reserved for D-TBDR (open, Stage 5); rejected with `ZSC_ERROR_TILE_LOCAL_UNSUPPORTED` |
| `[StreamSet(<integer code expression>)]` | a vertex-input struct in a `Common/` module | the struct is the declaration of that stream-set code; Slang folds the expression and the compiler reads it from the reflection (see "Stream-set declarations") |
| `[TextureUsage("colour" | "linear" | "normal" | "mask")]` | a texture field of a material type's `Params` | the usage class the cook checks the bound texture against; a `normal` field marks the meshes that need tangents |
| `[UVChannel(n)]` | a texture field of a material type's `Params` | the UV set the field samples (default 0), recorded for the mesh cook and the texture streamer |

- [Z2-N-REND-113] Every program declares exactly one `[FluxParams]` struct, and every resource field of it is a `DescriptorHandle<T>`; a program with zero or two parameter structs fails with `ZSC_ERROR_PARAMS_COUNT`.
- [Z2-N-REND-114] A resource field's access kind is derived from its type and attributes: SRV types are `READ`, UAV types are `READ_WRITE`, and `[WriteOnly]` UAV fields are `WRITE`; the compiler emits `FULL_COVERAGE` only for fields attributed `[FullCoverage]`, and the pass author is responsible for the shader actually covering the view, which poison-on-first-use detects when it does not.
- [Z2-N-REND-115] An attribute outside the table above, or one applied to the wrong kind of declaration, is a compile error (`ZSC_ERROR_UNKNOWN_ATTRIBUTE`, `ZSC_ERROR_ATTRIBUTE_TARGET`).

## Generated parameter headers

Each program yields one `<ProgramName>.generated.h`, placed at `${ZENITH_OUT}/generated/ZenithShadersGenerated/Flux/Shaders/`, included as `#include "Flux/Shaders/GTAO.generated.h"`.

```cpp
// Generated by ZenithShaderCompiler from Zenith2/Shaders/Passes/GTAO/GTAO.slang. Do not edit.
#pragma once
#include "Zenith_RHIShaderInterface.h"

inline constexpr Zenith_RHIShaderProgramID xFLUX_PROGRAM_GTAO = { 0x3c9d52a1e07f4b16ull };
inline constexpr Zenith_RHIEntryPointID xFLUX_ENTRY_GTAO_MAIN = { 0x8e11f04c2d7a9b35ull };
inline constexpr uint64 ulFLUX_GTAO_LAYOUT_HASH = 0x51d0c7a9e3b28f64ull;

struct Flux_GTAOPermutation
{
	uint32 m_uQuality = 0;            // uPERM_QUALITY in {0, 1, 2}
	bool m_bHalfResolution = false;    // bPERM_HALF_RESOLUTION in {false, true}

	constexpr uint32 GetKey() const { return m_uQuality * 2u + (m_bHalfResolution ? 1u : 0u); }
	static constexpr uint32 uKEY_COUNT = 6;
};

struct Flux_GTAOParams
{
	Zenith_RHISRV<Zenith_RHITex2D> m_xDepth;
	Zenith_RHISRV<Zenith_RHITex2D> m_xNormals;
	Zenith_RHIUAV<Zenith_RHITex2D> m_xAO;
	Zenith_RHIFloat4x4 m_xInvProjection;
	Zenith_RHIFloat2 m_xInvExtent;
	float m_fRadius;
	uint32 m_uSliceCount;

	static constexpr Zenith_RHIShaderProgramID xPROGRAM = xFLUX_PROGRAM_GTAO;
	static constexpr uint64 ulLAYOUT_HASH = ulFLUX_GTAO_LAYOUT_HASH;
	using Permutation = Flux_GTAOPermutation;
};
static_assert(sizeof(Flux_GTAOParams) == 104);
static_assert(offsetof(Flux_GTAOParams, m_xDepth) == 0);
static_assert(offsetof(Flux_GTAOParams, m_xNormals) == 8);
static_assert(offsetof(Flux_GTAOParams, m_xAO) == 16);
static_assert(offsetof(Flux_GTAOParams, m_xInvProjection) == 24);
static_assert(offsetof(Flux_GTAOParams, m_xInvExtent) == 88);
static_assert(offsetof(Flux_GTAOParams, m_fRadius) == 96);
static_assert(offsetof(Flux_GTAOParams, m_uSliceCount) == 100);

inline constexpr Zenith_RHIParamFieldDesc axFLUX_GTAO_FIELDS[] =
{
	{ 0,  ZENITH_RHI_PARAM_FIELD_KIND_TEXTURE_SRV, ZENITH_RHI_PARAM_FIELD_ACCESS_READ,  ZENITH_RHI_SHADER_STAGE_COMPUTE, ZENITH_RHI_PARAM_FIELD_FLAG_NONE,          ZENITH_RHI_RESOURCE_DIM_TEX2D, 0, "m_xDepth" },
	{ 8,  ZENITH_RHI_PARAM_FIELD_KIND_TEXTURE_SRV, ZENITH_RHI_PARAM_FIELD_ACCESS_READ,  ZENITH_RHI_SHADER_STAGE_COMPUTE, ZENITH_RHI_PARAM_FIELD_FLAG_NONE,          ZENITH_RHI_RESOURCE_DIM_TEX2D, 0, "m_xNormals" },
	{ 16, ZENITH_RHI_PARAM_FIELD_KIND_TEXTURE_UAV, ZENITH_RHI_PARAM_FIELD_ACCESS_WRITE, ZENITH_RHI_SHADER_STAGE_COMPUTE, ZENITH_RHI_PARAM_FIELD_FLAG_FULL_COVERAGE, ZENITH_RHI_RESOURCE_DIM_TEX2D, 0, "m_xAO" },
};
template<> struct Zenith_RHIParamFieldTable<Flux_GTAOParams> { static constexpr Zenith_Span<const Zenith_RHIParamFieldDesc> xFIELDS = axFLUX_GTAO_FIELDS; };
```

The field-table entry type and its enums live in the ZenithRHI public root header `Zenith_RHIShaderInterface.h` (with `Zenith_RHISRV<T>`, `Zenith_RHIUAV<T>`, `Zenith_RHIDescriptorHandle`, the resource tag types, the plain-data types of "Shared GPU structs", `Zenith_RHIShaderProgramID`, `Zenith_RHIEntryPointID` and `Zenith_RHIPermutationKey`), because a generated header uses ZenithCore, ZenithRHI and ZenithVertexFormat types and no others (Z2-N-ARCH-083):

```cpp
struct Zenith_RHIParamFieldDesc
{
	uint16 m_uOffset;                  // byte offset in the parameter struct
	Zenith_RHIParamFieldKind m_eKind;        // uint8
	Zenith_RHIParamFieldAccess m_eAccess;    // uint8
	uint16 m_uStageMask;               // Zenith_RHIShaderStage bits
	uint16 m_uFlags;                   // Zenith_RHIParamFieldFlag bits
	Zenith_RHIResourceDim m_eDim;            // uint8
	uint32 m_uElementStride;           // structured buffers; 0 otherwise
	const char* m_szName;
};

enum Zenith_RHIParamFieldKind : uint8 { ZENITH_RHI_PARAM_FIELD_KIND_TEXTURE_SRV, ZENITH_RHI_PARAM_FIELD_KIND_TEXTURE_UAV, ZENITH_RHI_PARAM_FIELD_KIND_BUFFER_SRV, ZENITH_RHI_PARAM_FIELD_KIND_BUFFER_UAV, ZENITH_RHI_PARAM_FIELD_KIND_PERSISTENT_SRV };
enum Zenith_RHIParamFieldAccess : uint8 { ZENITH_RHI_PARAM_FIELD_ACCESS_READ, ZENITH_RHI_PARAM_FIELD_ACCESS_WRITE, ZENITH_RHI_PARAM_FIELD_ACCESS_READ_WRITE, ZENITH_RHI_PARAM_FIELD_ACCESS_TILE_LOCAL_READ };
enum Zenith_RHIShaderStage : uint16 { ZENITH_RHI_SHADER_STAGE_VERTEX = 1u << 0, ZENITH_RHI_SHADER_STAGE_PIXEL = 1u << 1, ZENITH_RHI_SHADER_STAGE_COMPUTE = 1u << 2, ZENITH_RHI_SHADER_STAGE_MESH = 1u << 3, ZENITH_RHI_SHADER_STAGE_TASK = 1u << 4 };
enum Zenith_RHIParamFieldFlag : uint16 { ZENITH_RHI_PARAM_FIELD_FLAG_NONE = 0, ZENITH_RHI_PARAM_FIELD_FLAG_FULL_COVERAGE = 1u << 0, ZENITH_RHI_PARAM_FIELD_FLAG_OPTIONAL = 1u << 1 };
struct Zenith_RHIShaderProgramID { uint64 m_ulValue; };   // the program-ID hash
struct Zenith_RHIEntryPointID { uint64 m_ulValue; };      // the entry-point-ID hash
enum Zenith_RHIResourceDim : uint8 { ZENITH_RHI_RESOURCE_DIM_TEX2D, ZENITH_RHI_RESOURCE_DIM_TEX2D_ARRAY, ZENITH_RHI_RESOURCE_DIM_TEX3D, ZENITH_RHI_RESOURCE_DIM_TEXCUBE, ZENITH_RHI_RESOURCE_DIM_TEXCUBE_ARRAY, ZENITH_RHI_RESOURCE_DIM_BYTE_BUFFER, ZENITH_RHI_RESOURCE_DIM_STRUCT_BUFFER };
```

| Generated element | Naming | Content |
|---|---|---|
| program ID | `xFLUX_PROGRAM_<PROGRAM>` | `Zenith_RHIShaderProgramID{uint64}` from the program-ID hash |
| entry point IDs | `xFLUX_ENTRY_<PROGRAM>_<ENTRY>` | one per entry point |
| layout hash | `ulFLUX_<PROGRAM>_LAYOUT_HASH` | 64-bit hash of the parameter struct's field names, types and offsets |
| permutation struct | `Flux_<Program>Permutation` | one member per permutation constant (Hungarian from the Slang name: `uPERM_QUALITY` → `m_uQuality`), `GetKey()`, `uKEY_COUNT` |
| parameter struct | `Flux_<Program>Params` | one member per Slang field, in declaration order; resource fields as `Zenith_RHISRV<Tag>`/`Zenith_RHIUAV<Tag>`, persistent handles as `Zenith_RHIDescriptorHandle`, plain data as `float`, `uint32`, `int32`, `Zenith_RHIFloat2..4`, `Zenith_RHIUInt2..4`, `Zenith_RHIFloat4x4`, `Zenith_RHIFloat3x4` |
| size and offset asserts | `static_assert(sizeof(...))`, one `static_assert(offsetof(...))` per field | the exact values Slang reflection reports for the scalar layout |
| field table | `axFLUX_<PROGRAM>_FIELDS` plus the `Zenith_RHIParamFieldTable<T>` specialisation | one entry per resource field (plain data is not tracked) |
| shared GPU structs | `Flux_SharedTypes.generated.h` | every Slang struct attributed `[FluxShared]` (GPU scene records, `Flux_DrawRecord`, `Flux_IndirectDrawIndexed`, `Flux_TransparentEntry`, `Flux_ScatterRecord`, `Flux_DecalRecord`, `Flux_ShadowViewRecord` and the other records GPU and CPU both touch), emitted once with a size assert and an offset assert per field |
| vertex layout tables | `axFLUX_<PROGRAM>_VERTEX_ATTRIBUTES`, `axFLUX_<PROGRAM>_VERTEX_STREAMS` | the family default; see `Zenith2/Docs/Rendering/VertexData.md` "Generated layout tables" |
| specialised vertex tables | `axFLUX_<PROGRAM>_VERTEX_ATTRIBUTES_<CODE>`, `axFLUX_<PROGRAM>_VERTEX_STREAMS_<CODE>` (`<CODE>` four upper-case hex digits) and `axFLUX_<PROGRAM>_VERTEX_LAYOUTS[]` (`Zenith_StreamSetLayoutEntry`, ascending code order) | one pair per stream-set the program can draw: the attributes the program declares that the code holds, with the code's formats, offsets and strides and the program's locations, then the null-stream attributes (stream 7, stride 0, input rate vertex) for every declared attribute the code lacks, and the declaration's layout hash (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"`) |

- [Z2-N-REND-116] Every generated header contains exactly the elements of the table above and includes nothing but `Zenith_RHIShaderInterface.h`, `Zenith_VertexFormat.h`, `Flux_SharedTypes.generated.h` and `Flux_StreamSets.generated.h`, so it uses no ZenithFlux type (Z2-N-ARCH-083) and compiles on its own in the HeaderSelfContained gate step; the C++ declarations of the shared GPU structs exist only as generated code, Slang being their source of truth (Z2-D-015).
- [Z2-N-REND-117] Every field of every generated parameter struct has a `static_assert(offsetof(...))` with the offset Slang reflection reports, and the struct has a `static_assert(sizeof(...))`, so a disagreement between the C++ and Slang layouts is a C++ compile error.
- [Z2-N-REND-118] The field table lists every resource field of the parameter struct, with offset, kind, access, stage mask, flags, dimension and element stride, and is the render graph's sole source of access information for parameter-struct fields.
- [Z2-N-REND-119] The permutation key is the mixed-radix number over the program's permutation constants in declaration order, the last-declared constant least significant, each digit being the index of the value in its `[Permutation(...)]` set; `GetKey()` in C++ and the compiler's enumeration use this one formula.

### Stream-set declarations

Every reachable vertex stream-set is one vertex-input struct of `Zenith2/Shaders/Common/StreamSets.slang` attributed `[StreamSet(<code>)]`, expanded by the Slang preprocessor from GEOM's generator macros; the declarations are the one source of every layout (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`). The aggregate command `ZenithShaderCompiler --stream-sets --shader-root Zenith2/Shaders --header-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/ --depfile <file>` is added by `ZenithShadersGenerated`'s `SHADERS` option beside the shared-types command; its inputs are the `.slang` files of `Zenith2/Shaders/Common/`, every per-program command depends on its output, and it writes `Flux/Shaders/Flux_StreamSets.generated.h`: one `axFLUX_STREAMSET_ATTRIBUTES_<CODE>` / `axFLUX_STREAMSET_STREAMS_<CODE>` pair per declared stream-set, then the index `axFLUX_STREAMSET_LAYOUTS[]` (`Zenith_StreamSetLayoutEntry`: the code and its layout description with its layout hash), every array in ascending code order and every attribute array in ascending (stream, offset) order.

| Compiler error | Raised when |
|---|---|
| `ZSC_ERROR_VERTEX_FORMAT` | a `[StreamSet]` struct violates the vertex-format vocabulary ([Z2-N-REND-868] to [Z2-N-REND-870]) |
| `ZSC_ERROR_STREAM_SET_CODE` | the attribute's argument does not fold to a code `Zenith_ValidateStreamSetCode` accepts; names the struct and the value |
| `ZSC_ERROR_STREAM_SET_MISMATCH` | the reflected layout does not hold exactly the code's attributes in the code's formats, or a family struct of `MeshVertex.slang` lacks its family default's declaration; names the struct, the code and the first differing attribute |
| `ZSC_ERROR_STREAM_SET_DUPLICATE` | two structs declare one code; names both |
| `ZSC_ERROR_STREAM_SET_LOCATION` | `[StreamSet]` outside `Common/`, on a struct with a field that is not a vertex input, or a declared field in stream 7 (reserved to the null stream) |
| `ZSC_ERROR_STREAM_SET_MISSING` | a reachable code (`Zenith_IsReachableStreamSet`) has no struct; names its canonical name |
| `ZSC_ERROR_STREAM_SET_UNREACHABLE` | a declared MESH, TERRAIN or PROCEDURAL code the reachable rule rejects; names the struct |
| `ZSC_ERROR_STREAM_SET_NAME` | a name of `Zenith2/Shaders/StreamSets.list` or of the cook's `--stream-set-list` file does not parse (`Zenith_ParseStreamSetName`) to a declared code |

| Null stream | Rule |
|---|---|
| binding | a program attribute the drawn code lacks goes to stream 7 at input rate vertex and stride 0, packed in the program's declaration order in the family default's format (an absent UV reads (0, 0), an absent colour (0, 0, 0, 0)); a pool code (`POOL16`, `POOL32`) binds `POSITION` there, never omitting it, because every permutation of a mesh vertex program declares it |
| draw | every draw whose PSO table carries the null stream binds GEOM's zero-filled `Flux_NullVertexStream` (a `Flux_PersistentReadOnlyBuffer` with usage `VERTEX`) at binding 7; the pass declares nothing for it ([Z2-N-REND-388]) |
| engine and cook sets | engine raster programs compile for the codes of `Zenith2/Shaders/StreamSets.list`; in cook mode, for the union of that list and the game's `stream_sets.json` (`--stream-set-list`) |

- [Z2-N-REND-139] Every vertex stream-set is declared exactly once, as a `[StreamSet(<code>)]` struct under `Zenith2/Shaders/Common/`, the `--stream-sets` command raises exactly the errors of the table above, and its header is byte-deterministic for the same `Common/` sources, so runtime PSO creation, the cook's packer and the mesh reader all read one set of layouts ([Z2-N-REND-871], [Z2-N-REND-872]).
- [Z2-N-REND-140] A raster program's specialised vertex tables bind every declared attribute the drawn stream-set lacks to the null stream of the table above, and the raster context binds `Flux_NullVertexStream` at binding 7 for every draw with such a PSO.

## Reflection JSON

`Reflection/<ProgramPath>.reflection.json`, one per program, schema version 1 (in cook mode the compiler also writes one `Reflection/Particles/<ModulePath>/<Type>.particle.json` per particle type in the shader roots, schema version 1, holding `module`, `type`, `typeId` (`Zenith_Hash64` of `"<module path without extension>/<Type>"`), `interface` (`Flux_ParticleUpdateType` or `Flux_ParticleShadingType`) and `params` in the form of a program's `params`; `Zenith2/Docs/Design/VFX/Authoring.md` → "Cook pipeline"):

```json
{
	"schemaVersion": 1,
	"program": "Passes/GTAO/GTAO",
	"programId": "0x3c9d52a1e07f4b16",
	"compiler": { "tool": "1.0.0", "slang": "<pinned>", "dxc": "<pinned>" },
	"sources": [ { "path": "Zenith2/Shaders/Passes/GTAO/GTAO.slang", "hash": "…" } ],
	"entryPoints": [ { "name": "Main", "id": "0x8e11f04c2d7a9b35", "stage": "compute", "threadGroupSize": [8, 8, 1], "alphaTest": false } ],
	"params": {
		"name": "GTAOParams", "size": 104, "layoutHash": "0x51d0c7a9e3b28f64",
		"fields": [
			{ "name": "m_xDepth", "offset": 0, "size": 8, "kind": "textureSrv", "type": "Texture2D<float>", "dim": "tex2d", "access": "read", "stages": ["compute"], "flags": [] },
			{ "name": "m_xAO", "offset": 16, "size": 8, "kind": "textureUav", "type": "RWTexture2D<float>", "dim": "tex2d", "access": "write", "stages": ["compute"], "flags": ["fullCoverage"] },
			{ "name": "m_fRadius", "offset": 96, "size": 4, "kind": "data", "type": "float" }
		]
	},
	"permutations": {
		"constants": [
			{ "name": "uPERM_QUALITY", "type": "uint", "values": [0, 1, 2], "requires": null },
			{ "name": "bPERM_HALF_RESOLUTION", "type": "bool", "values": [false, true], "requires": null }
		],
		"keyCount": 6,
		"compiledKeys": [0, 1, 2, 3, 4, 5]
	},
	"vertexInput": null,
	"outputs": [],
	"lint": { "file": "Lint/Passes/GTAO/GTAO.lint.json", "failures": 0 },
	"blobs": [ { "key": 0, "api": "spirv", "entry": "Main", "size": 5312, "hash": "…" } ]
}
```

| Key | Meaning |
|---|---|
| `entryPoints[].alphaTest` | the entry point is attributed `[AlphaTest]` |
| `params.fields[].kind` | `textureSrv`, `textureUav`, `bufferSrv`, `bufferUav`, `persistentSrv`, or `data` |
| `permutations.compiledKeys` | every key the enumeration rule produced (keys collapsed by `[PermutationRequires]` are absent) |
| `vertexInput` | raster programs: streams (`index`, `stride`, `rate`) and attributes (`semantic`, `semanticIndex`, `stream`, `offset`, `format`) |
| `outputs` | raster programs, per permutation key: each colour output location, its format class and `written` (`always` or `never`), and `depthExport` |

- [Z2-N-REND-120] The reflection JSON of every program follows schema version 1 above, carries every key listed, and is the one machine-readable description of a program that the cook, the editor and the gate steps consume; a schema change bumps `schemaVersion` and updates every consumer in the same diff.

## Shader blob container

A `.zshader` file holds every compiled permutation of one program for every API it was compiled for. It is versioned binary cooked data (Z2-D-038), little-endian, serialized field by field, not by C++ struct layout.

| Offset | Field | Type | Meaning |
|---|---|---|---|
| 0 | magic | u32 | `uFLUX_SHADER_CONTAINER_MAGIC`, `'ZSHD'` (0x4448535A) ([Z2-N-REND-994]) |
| 4 | containerVersion | u32 | `uFLUX_SHADER_CONTAINER_VERSION` ([Z2-N-REND-994]); bumped by any change to this format, the global pipeline layout or root signature, or the static sampler table |
| 8 | programId | u64 | the program ID |
| 16 | layoutHash | u64 | the parameter-struct layout hash |
| 24 | sourceHash | u64 | hash over every source file the program read |
| 32 | toolHash | u64 | hash of the tool, Slang and DXC versions |
| 40 | apiMask | u32 | bit 0 SPIR-V, bit 1 DXIL |
| 44 | tierMask | u32 | bit 0 desktop, bit 1 mobile |
| 48 | entryPointCount | u32 | |
| 52 | permutationCount | u32 | compiled keys |
| 56 | entryTableOffset | u64 | array of `{entryId u64, stage u32, pad u32}` |
| 64 | permutationTableOffset | u64 | array of `{key u32, flags u32, materialVariantId u64, streamSetId u32, blobRefCount u32, blobRefOffset u64}` sorted by (key, materialVariantId, streamSetId); `flags` per the table "Permutation entry flags" below |
| 72 | blobRefs | array | `{entryIndex u16, api u8, tier u8, blobOffset u64, blobSize u32, blobHash u64}`; `blobHash` is `Zenith_Hash64` of the blob's bytes ([Z2-N-REND-992]) |
| … | blobs | bytes | each blob 16-byte aligned |

| Permutation entry flags (bits of `flags`) | Meaning |
|---|---|
| 0–7 | bit n set: colour output n is written on every path of the permutation's pixel entry point |
| 8–15 | bit 8 + n set: colour output n is never written (the PSO builder forces its write mask to 0, [Z2-N-REND-921]) |
| 16 | the permutation exports depth (`SV_Depth*`), allowed for allowlisted programs alone (lint `ShaderChecks.NoDepthExport`) |
| 17 | the permutation's pixel entry point is the `[AlphaTest]` entry |
| 18–31 | zero |

The flags are the inputs of the attachment checks of [Z2-N-REND-919], [Z2-N-REND-920] and [Z2-N-REND-921]: a colour output whose bits n and 8 + n are both clear is conditionally written, which `ShaderChecks.ConditionalOutputWrite` fails, and both set is a compiler defect.

- [Z2-N-REND-138] The compiler writes the permutation entry's `flags` exactly as the table "Permutation entry flags" defines them, and the runtime loader refuses a container whose entry has a bit of 18–31 set with `FLUX_SHADER_ERROR_CONTAINER_VERSION`.
- [Z2-N-REND-121] The runtime loader refuses a `.zshader` whose magic, container version or tool hash differs from what the runtime expects, with `FLUX_SHADER_ERROR_CONTAINER_VERSION` naming the file; it never attempts to interpret an older container.
- [Z2-N-REND-994] The container magic and version are declared once, in the generated header `Flux/Shaders/Flux_ShaderContainerFormat.generated.h` under `${ZENITH_OUT}/generated/ZenithShadersGenerated/` (so `#include "Flux/Shaders/Flux_ShaderContainerFormat.generated.h"` through `ZenithShadersGenerated`): `inline constexpr uint32 uFLUX_SHADER_CONTAINER_MAGIC = 0x4448535Au;` and `inline constexpr uint32 uFLUX_SHADER_CONTAINER_VERSION = <n>u;`. `zenith_add_shaders` (`CMake/ZenithShaders.cmake`) writes it at configure time, rewriting it only when its content changes, from the two CMake values `ZENITH_SHADER_CONTAINER_MAGIC` and `ZENITH_SHADER_CONTAINER_VERSION` set once in that file, and passes the same two values to every compiler invocation as `--container-magic` and `--container-version`, which the compiler writes into every container it produces and without which it exits 2; a version bump is an edit of that one CMake value. Its readers include it and name no literal of either value: the compiler's other callers (the cook's material compiles and watch mode) pass the header's constants, ZenithFlux's `Flux_ShaderLibrary` and `Flux_PipelineCacheStore` (the persistent cache key) compare with them, and RHI's header-only test reader in `ZenithRHI/Tests/Support/` reaches the header through its library's `TEST_DEPENDS ZenithShadersGenerated`, never through a ZenithFlux header.
- [Z2-N-REND-122] The runtime loader verifies each blob's hash when the blob is first used for PSO creation, and a mismatch is `FLUX_SHADER_ERROR_CORRUPT_BLOB`, treated as a missing PSO (see "Missing-PSO fallback").
- [Z2-N-REND-123] The cook packages the blob containers of every program the game can use (every engine program in the global PSO group, every material program compiled for the game's material-variant set, and every particle program's cooked variants compiled for the game's particle type set) into the game's cooked package, and nothing else.
- [Z2-N-REND-992] A blob reference's `blobHash` is `Zenith_Hash64` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Hash64", the algorithm selected by D-SEL-HASH (open, Stage 1)) of the blob's `blobSize` bytes exactly as stored at `blobOffset`, alignment padding excluded, with seed 0; the compiler writes it into the container and the same value as the blob hash of `--manifest`, and the loader's check of [Z2-N-REND-122] recomputes it the same way. It is a corruption check and never an identity: no cache key, file name or PSO key derives from it. The compiler and the loader depend on D-SEL-HASH through it as they already do through the program ID ([Z2-N-REND-107]); no SHA-256 digest or truncation of one is used for it. A change of the algorithm behind `Zenith_Hash64` changes every blob hash and is therefore a change to this format: it bumps `uFLUX_SHADER_CONTAINER_VERSION`, so an older container is refused by [Z2-N-REND-121] and recompiled, never checked against the new algorithm.

## Permutations

### Enumeration rule

| Step | Action |
|---|---|
| 1 | Collect the program's `[Permutation]` constants in declaration order. |
| 2 | Form the full cartesian product of their value sets. |
| 3 | For every constant with `[PermutationRequires("C", v)]`, drop every tuple where `C != v` and the constant is not at its default (first) value; for every `[PermutationValueRequires(v, "C", w)]`, drop every tuple where the constant equals `v` and `C != w`. Then assign each surviving tuple in which a `[PermutationBuild("development")]` constant is not at its default value to the development container and every other tuple to the program's container; in cook mode the development tuples are dropped. |
| 4 | Compute each surviving tuple's key by the mixed-radix rule. |
| 5 | For raster programs, multiply by the vertex stream-sets the program draws (from its vertex-input declaration and every precision-fallback layout it names). |
| 6 | For material-evaluating programs, multiply by the game's material variants for the domains the program draws (cook mode, from `--material-set`); for particle programs, multiply by the game's particle type set (cook mode, from `--particle-type-set`); variant 0 (no update type, `ParticleShading_Standard`) is compiled at build time with the engine programs. |
| 7 | Compile every resulting (key, stream-set, material variant) for every requested API and tier. |

```cpp
// Zenith_RHIShaderInterface.h (ZenithRHI public root header)
struct Zenith_RHIPermutationKey
{
	uint32 m_uProgramKey;          // mixed-radix key over program-level constants
	uint32 m_uStreamSetID;         // 0 for programs without vertex input
	uint64 m_ulMaterialVariantID;  // hash(material type, material-level permutation values), or the particle type ID; 0 for programs with neither
};
static_assert(sizeof(Zenith_RHIPermutationKey) == 16);
```

- [Z2-N-REND-124] Every permutation a program can be asked for is produced by the enumeration rule above and compiled offline: engine programs (and particle programs' variant 0) at build time, material-evaluating programs at cook time against the game's material-variant set, and particle programs at cook time against the game's particle type set.
- [Z2-N-REND-125] A material variant, particle type or stream-set that content references and the compiled set lacks (for a particle program, a missing (program, key, particle type) triple) is a cook error (`COOK_ERROR_PERMUTATION_MISSING`, naming the asset, the program and the key); at runtime, a permutation absent from the loaded container is a missing PSO (see "Missing-PSO fallback"), never a compile.
- [Z2-N-REND-126] The compiler reports each program's compiled permutation count, the cook reports the per-game total, and exceeding `Z2-B-RPROD-024` Shader permutation limit per game (pending) fails the compile with exit code 5 or the cook with `COOK_ERROR_PERMUTATION_LIMIT`.
- [Z2-N-REND-993] A `[PermutationBuild("development")]` constant keeps its non-default values out of every cooked package, in every configuration including `Shipping`: the compiler writes the permutations in which it is not at its default value into the development container `DevelopmentBlobs/<ProgramPath>.zshader` alone, the cook ([Z2-N-REND-123]) never packages a development container and compiles no development permutation in cook mode, and the PSO precache predictor never reads one, so a request for such a permutation from a cooked package is a missing permutation (`FLUX_SHADER_ERROR_MISSING_PERMUTATION`, "Missing-PSO fallback"). A development test or benchmark loads the development container through `ZENITH_SHADERS_GENERATED_DIR` and adds it to `Flux_ShaderLibrary` after the program's own container; the library then resolves a key of that program from either, and refuses a development container whose program ID, layout hash or tool hash differs from the program container's with `FLUX_SHADER_ERROR_CONTAINER_VERSION`. The attribute on a material-level constant, or with an argument other than `development`, is `ZSC_ERROR_ATTRIBUTE_TARGET`.

## Shader lints

The compiler runs the lints below on every program unless `--no-lint` is passed, and writes `Lint/<ProgramPath>.lint.json`: `{ "schemaVersion": 1, "program": "…", "results": [ { "lint": "ShaderChecks.DiscardOnlyInAlphaTest", "severity": "fail|report", "file": "…", "line": 0, "message": "…", "allowlistedBy": null } ] }`. The ShaderChecks gate step aggregates every lint file and fails on any `fail` result not allowlisted.

| Lint | Severity | Checks |
|---|---|---|
| `ShaderChecks.DiscardOnlyInAlphaTest` | fail | `discard`/`clip` appear only in code reachable from an `[AlphaTest]` entry point |
| `ShaderChecks.NoDepthExport` | fail | no `SV_Depth`, `SV_DepthGreaterEqual`, `SV_DepthLessEqual` unless allowlisted with a decision ID |
| `ShaderChecks.NoMin16Float` | fail | no `min16float`, `min16int`, `min16uint`, `min10float` |
| `ShaderChecks.HalfRatioReport` | report | float and half arithmetic instruction counts and their ratio per entry point |
| `ShaderChecks.ConditionalOutputWrite` | fail | every colour output of a permutation is written on every path or on none |
| `ShaderChecks.MaterialExplicitGradients` | fail | material modules sample only through explicit-gradient functions |
| `ShaderChecks.VertexFormatVocabulary` | fail | every `[VtxFmt]` string is in the closed vocabulary and matches its field's declared lane count |
| `ShaderChecks.ParamsSize` | fail | the parameter struct is at most `uFLUX_MAX_PARAMS_BYTES` |
| `ShaderChecks.BindlessThroughAccessors` | fail | `ResourceDescriptorHeap`, `SamplerDescriptorHeap` and raw descriptor arrays appear only in `Common/Bindless.slang` |
| `ShaderChecks.NoGlobalBindings` | fail | no resource declared with an explicit `register(...)` outside `Common/Params.slang` and `Common/Bindless.slang`; the push constant itself is an entry-point `uniform` parameter with no register (`Zenith2/Docs/Rendering.md` → "Binding model" [Z2-N-REND-998]), and the D3D12 draw-ID root constant at `b1, space0` is the one `register` of `Common/Params.slang`, declared there beside `Flux_PushConstants` for the D3D target only (Vulkan reads `SV_DrawIndex`); `Common/DrawID.slang` declares none |
| `ShaderChecks.NoVulkanAttributes` | fail | no identifier `vk` followed by `::` (an attribute such as `[[vk::binding(…)]]` or `[vk::push_constant]`, an intrinsic or a type), outside comments and string literals, in any `.slang` file of `Zenith2/` outside `out/`, `ThirdParty/` and the gate fixtures, `Common/` included; found by a token scan of the source text (the compiler's lint, because the linked IR keeps no attribute spelling, and the ShaderChecks gate step's own scan before any compile, so it fails even when no shader compiler exists), and it admits no allowlist entry (Z2-D-141, [Z2-N-REND-998]; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ShaderChecks" item 2) |
| `ShaderChecks.TileLocalRejected` | fail | no `[TileLocal]` field until D-TBDR (open, Stage 5) is ruled |

- [Z2-N-REND-127] The ShaderChecks gate step runs the lints of the table above on every program of the diff's build and fails on every `fail`-severity result that is not allowlisted in `Zenith2/Shaders/ShaderChecks_allowlist.json` by an entry `{ "lint", "program", "decision": "Z2-D-NNN", "reason" }`.

## Watch mode and editor reload

### Watch mode

| Step | Action |
|---|---|
| 1 | Watch every `--shader-root` recursively (`ReadDirectoryChangesW`). |
| 2 | Debounce changes for 200 ms after the last event. |
| 3 | Map changed files to affected programs through the dependency graph built from the programs' depfiles. |
| 4 | Recompile each affected program for the permutations recorded in the last compile manifest (and, for material programs, the material-variant set the manifest names). |
| 5 | Write each output as `<file>.tmp` and rename it over the old file, so a reader never sees a partial container. |
| 6 | Append one journal record per recompiled program to the reload journal. |

### The editor reload protocol

The journal is `Zenith2/out/<checkout-hash>/<preset>/Shaders/reload.journal`, one JSON object per line; lines are appended and existing lines are not rewritten:

```json
{ "seq": 42, "program": "Passes/GTAO/GTAO", "programId": "0x3c9d52a1e07f4b16", "status": "ok", "container": "Blobs/Passes/GTAO/GTAO.zshader", "containerHash": "…", "layoutHash": "0x51d0c7a9e3b28f64", "previousLayoutHash": "0x51d0c7a9e3b28f64", "errors": [] }
```

| `status` | Editor response |
|---|---|
| `ok`, `layoutHash == previousLayoutHash` | load the new container, mark every PSO of the program stale, recreate them through the asynchronous PSO creation schedule, keep drawing with the old PSOs until each replacement is ready, then swap |
| `layout-changed` (engine program) | keep the old container; show "The parameter struct of `<program>` changed layout; rebuild the C++ targets to load it" in the editor notification area and the console |
| `layout-changed` (material program) | re-cook the material constants of every loaded material of that type from their documents, then proceed as `ok` (material constants are data, so no C++ rebuild is needed) |
| `error` | keep the old container; show each error as `file(line,col): message` |

- [Z2-N-REND-128] Watch mode recompiles affected programs within the debounce window, writes outputs atomically by rename, and appends exactly one journal record per recompiled program; the editor applies records in `seq` order and never skips one.
- [Z2-N-REND-129] A shader reload never compiles a shader inside the editor process and never creates a PSO synchronously on the render-submit thread; replacement PSOs go through the asynchronous creation schedule, and the previous PSO stays bound until its replacement is ready.
- [Z2-N-REND-130] A layout change of an engine program's parameter struct is never hot-loaded: the editor keeps the old blobs and states that a C++ rebuild is needed.
- [Z2-N-REND-131] The reload watcher lives in the `ZenithEditor` library and is linked into editor executables exclusively; shipping and development Player executables contain no reload code and read no journal.
- [Z2-N-REND-132] The reload round trip (save a `.slang` file to the first frame drawn with the new PSO) is measured against `Z2-B-EDIT-003` Shader blob reload in `Zenith2/Docs/Budgets.md → "Editor iteration rows"`.

### Shader system tests

- [Z2-N-REND-133] The shader-system tests `Test_ShaderCompiler_DeterministicOutput`, `Test_ShaderCompiler_HeaderOffsetsMatchReflection`, `Test_ShaderCompiler_PermutationKeyFormula` (C++ `GetKey()` equals the compiler's key for every tuple of a fixture program), `Test_ShaderCompiler_PermutationRequiresCollapses`, `Test_ShaderCompiler_CheckDetectsStale`, `Test_ShaderCompiler_UnknownAttributeRejected`, `Test_ShaderCompiler_ParamsTooLargeRejected`, `Test_ShaderCompiler_WatchJournalAtomic`, `Test_Shader_ContainerVersionRejected`, `Test_Shader_CorruptBlobIsMissingPSO` and one failing fixture per lint of "Shader lints" exist and pass; ZSC tests live in `Zenith2/Tools/ZenithShaderCompiler/Tests/`.

## Shared GPU structs

A struct attributed `[FluxShared]` is a record that both GPU code and CPU code read or write. Its one C++ declaration is the generated code in `Flux_SharedTypes.generated.h`.

<!-- elaboration -->
| Slang member type | C++ member type | Size |
|---|---|---|
| `uint` | `uint32` | 4 |
| `int` | `int32` | 4 |
| `float` | `float` | 4 |
| `float2`, `float3`, `float4` | `Zenith_RHIFloat2`, `Zenith_RHIFloat3`, `Zenith_RHIFloat4` | 8, 12, 16 |
| `uint2`, `uint3`, `uint4` | `Zenith_RHIUInt2`, `Zenith_RHIUInt3`, `Zenith_RHIUInt4` | 8, 12, 16 |
| `int2`, `int3`, `int4` | `Zenith_RHIInt2`, `Zenith_RHIInt3`, `Zenith_RHIInt4` | 8, 12, 16 |
| `float3x4`, `float4x4` (row-major) | `Zenith_RHIFloat3x4`, `Zenith_RHIFloat4x4` | 48, 64 |
| `DescriptorHandle<T>` | `Zenith_RHIDescriptorHandle` | 8 |
| `T m_axName[N]` (fixed array of an admitted type) | `T m_axName[N]` | N × size of T |
| a nested `[FluxShared]` struct | that struct, by value | its size |
| `bool`, `half` and 16-bit types, `min16*`, resource and sampler types, unsized arrays, pointers | rejected with `ZSC_ERROR_SHARED_TYPE` | — |

```cpp
// Zenith_RHIShaderInterface.h: the plain-data types (4-byte aligned, no padding)
struct Zenith_RHIFloat2 { float m_afValue[2]; };    struct Zenith_RHIUInt2 { uint32 m_auValue[2]; };    struct Zenith_RHIInt2 { int32 m_aiValue[2]; };
struct Zenith_RHIFloat3 { float m_afValue[3]; };    struct Zenith_RHIUInt3 { uint32 m_auValue[3]; };    struct Zenith_RHIInt3 { int32 m_aiValue[3]; };
struct Zenith_RHIFloat4 { float m_afValue[4]; };    struct Zenith_RHIUInt4 { uint32 m_auValue[4]; };    struct Zenith_RHIInt4 { int32 m_aiValue[4]; };
struct Zenith_RHIFloat3x4 { float m_afValue[12]; }; struct Zenith_RHIFloat4x4 { float m_afValue[16]; };   // row-major
```

| Rule | Specification |
|---|---|
| Layout | scalar layout, identical to parameter structs ([Z2-N-REND-064]): every member is 4-byte aligned, vectors and matrices take no extra alignment, arrays have a stride equal to the element size, no padding is inserted, and the struct size is the sum of its members; a 16-byte stride is obtained by explicit `m_uPad…` members |
| GPU access | a record array is read and written through byte-address buffers at `index × sizeof(struct)`, or through structured buffers declared with the same scalar layout |
| Names | the C++ struct and member names are the Slang names unchanged |
| Declaring module | each `[FluxShared]` struct is declared in exactly one module under `Zenith2/Shaders/Common/`, as the table below assigns |
| Output | `${ZENITH_OUT}/generated/ZenithShadersGenerated/Flux/Shaders/Flux_SharedTypes.generated.h`: one struct per `[FluxShared]` declaration, in (module path, declaration) order, each followed by `static_assert(sizeof(…))` and one `static_assert(offsetof(…))` per member |
| Command | one aggregate custom command added by `ZenithShadersGenerated`'s `SHADERS` option beside the per-program commands: `ZenithShaderCompiler --shared-types --shader-root Zenith2/Shaders --header-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/ --depfile <file>`; its inputs are every `.slang` file under `Zenith2/Shaders/Common/`, and every per-program command depends on its output |
| Duplicate | a struct name declared `[FluxShared]` twice fails with `ZSC_ERROR_SHARED_DUPLICATE: struct '<Name>' is declared [FluxShared] in both '<module A>' and '<module B>'` |
| Outside `Common/` | `[FluxShared]` on a struct outside `Zenith2/Shaders/Common/` fails with `ZSC_ERROR_SHARED_LOCATION` |

| Declaring module (`Zenith2/Shaders/Common/`) | `[FluxShared]` structs |
|---|---|
| `Params.slang` | `Flux_PushConstants` |
| `DrawID.slang` | `Flux_DrawRecord`, `Flux_IndirectDrawIndexed` |
| `GPUScene.slang` | `Flux_InstanceRecord`, `Flux_PrimitiveRecord`, `Flux_MaterialSlotRecord`, `Flux_LightRecord`, `Flux_ScatterRecord`, `Flux_DecalRecord` (96 B, GEOM's layout, `Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"`), `Flux_ShadowViewRecord` (96 B, LIGHT's layout, `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow view record"`) |
| `Culling.slang` | `Flux_TransparentEntry`, `Flux_FrustumPlanes`, `Flux_SlotCommandRecord` (16 B), `Flux_SlotCommandScatter` (20 B), `Flux_CullSurvivor` (8 B), `Flux_CullCounters` (32 B) (RPROD's layouts, `Zenith2/Docs/Design/RPROD/Culling.md → "Culling records"`) |
| `Skinning.slang` | `Flux_SkinDispatchRecord` (32 B, GEOM's layout, `Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"`) |
| `Particles.slang` | `Flux_ParticleState`, `Flux_EmitterCommand`, `Flux_ParticleDrawRecord`, `Flux_ParticleSortPair`, `Flux_ParticleEventRecord`, `Flux_ParticleTrailSample`, `Flux_ParticleValue`, `Flux_ParticleValidationRecord`, `Flux_ParticleCollider` (VFX's layouts, `Zenith2/Docs/Design/VFX/PublicAPI.md`) |
| `Meshlets.slang` | `Flux_MeshletRecord` (32 B, GEOM) |
| `LOD.slang` | `Flux_LODView` (32 B, GEOM, no `bool` member) |
| `Terrain.slang` | `Flux_TerrainRecord` (80 B, GEOM) |
| `Procedural.slang` | `Flux_ProceduralRecord` (64 B), `Flux_ProceduralDispatchData` (16 B) (GEOM) |
| `Probes.slang` (Stage 4) | `Flux_ReflectionProbeRecord` (64 B, LIGHT's layout, `Zenith2/Docs/Design/LIGHT/Reflections.md → "Reflection probe record"`) |
| `Media.slang` | `Flux_AtmosphereGPU` (64 B), `Flux_FogMediumGPU` (48 B), `Flux_MediumOverrideGPU` (48 B), `Flux_LocalFogVolumeGPU` (112 B, Stage 5) (LIGHT's layouts, `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "GPU parameter records"`) |
| `GI.slang` (Stage 5) | `Flux_IrradianceVolumeRecord` (64 B), `Flux_LightmapPageRecord` (16 B) (LIGHT's layouts, `Zenith2/Docs/Design/LIGHT/GI.md → "GI GPU records"`) |
| `Environment.slang` | `Env_FrameEnvironment` (128 B), `Env_WaterWaveRecord` (32 B), `Env_WaterBodyRecord` (272 B), `Env_LocalWindSource` (32 B, Stage 5), and ENV's other `Env_` records (`Zenith2/Docs/Design/ENV.md → "Render-world records"`) |

- [Z2-N-REND-134] Every `[FluxShared]` struct uses only the admitted member types of the mapping table above and the scalar layout of the rules table, and the shader compiler rejects any other member type with `ZSC_ERROR_SHARED_TYPE`, naming the struct, the member and its type.
- [Z2-N-REND-135] `Flux_SharedTypes.generated.h` is produced by the one aggregate shared-types command at the output path of the rules table, contains every `[FluxShared]` struct of `Zenith2/Shaders/Common/` with its size and offset asserts, and is the one C++ declaration of each of those structs.
- [Z2-N-REND-136] Each `[FluxShared]` struct is declared in exactly the module the table above assigns it; a duplicate declaration fails with `ZSC_ERROR_SHARED_DUPLICATE` and a declaration outside `Common/` with `ZSC_ERROR_SHARED_LOCATION`, and adding a shared struct adds its row to that table in the same diff.
- [Z2-N-REND-137] The shared-types tests `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection` (every generated offset equals Slang's scalar-layout offset), `Test_ShaderCompiler_SharedTypeRejected` (a `bool` member fails), `Test_ShaderCompiler_SharedDuplicateRejected` and `Test_ShaderCompiler_SharedLocationRejected` exist in `Zenith2/Tools/ZenithShaderCompiler/Tests/` and pass.

## PSO keys

```cpp
struct Flux_RasterState      // 16 bytes, packed by Flux_PackRasterState
{
	uint8 m_eCullMode;       // FLUX_CULL_MODE_BACK (default), _FRONT, _NONE
	uint8 m_eFillMode;       // FLUX_FILL_MODE_SOLID, _WIREFRAME (debug views)
	uint8 m_eTopology;       // FLUX_TOPOLOGY_TRIANGLE_LIST (default), _LINE_LIST, _POINT_LIST
	uint8 m_uDepthFlags;     // bit 0 test enable, bit 1 write enable, bit 2 clamp enable
	uint8 m_eDepthCompare;   // from Flux_DepthModeOps, never hand-picked
	uint8 m_uStencilIndex;   // index into the fixed stencil-state table (0 = disabled)
	uint8 m_uDepthBiasIndex; // index into the fixed depth-bias table: 0 none, 1 FLUX_DEPTH_BIAS_SHADOW (constant 0, slope 2.0, clamp 0), 2 decal
	uint8 m_uPad;
	uint32 m_uBlendIndices;  // 4 bits per colour attachment: index into the fixed blend-state table
	uint32 m_uWriteMasks;    // 4 bits per colour attachment: RGBA write mask
};

struct Flux_AttachmentFormats  // 12 bytes
{
	uint8 m_aeColour[8];      // Zenith_RHIFormat per colour attachment, ZENITH_RHI_FORMAT_UNDEFINED past m_uColourCount
	uint8 m_eDepth;           // ZENITH_RHI_FORMAT_D24_UNORM_S8_UINT, ZENITH_RHI_FORMAT_D32_FLOAT_S8X24_UINT, or ZENITH_RHI_FORMAT_UNDEFINED
	uint8 m_uColourCount;
	uint16 m_uPad;
};

struct Flux_PSOKey             // 56 bytes; compute PSOs zero m_xRaster and m_xFormats
{
	Zenith_RHIShaderProgramID m_xProgram;
	Zenith_RHIPermutationKey m_xPermutation;
	Flux_RasterState m_xRaster;
	Flux_AttachmentFormats m_xFormats;
	uint32 m_uPad;
};
static_assert(sizeof(Flux_PSOKey) == 56);
```

- [Z2-N-REND-195] A PSO is identified by its `Flux_PSOKey` (program, permutation, raster state, attachment formats); two PSOs are the same exactly when their 56 key bytes are equal, and the key's 64-bit hash (selected by D-SEL-HASH (open, Stage 1)) is used for lookup and persistent-cache naming only, never for equality.
- [Z2-N-REND-196] Attachment formats in a raster PSO key come from the pass's `Flux_RasterTargets` as the render graph resolved them, and a raster context cannot bind a PSO whose formats differ from the pass's targets.
- [Z2-N-REND-197] Blend, stencil and depth-bias states are indices into fixed tables in `Zenith2/Engine/ZenithFlux/Internal/PSO/Flux_FixedStates.h`, so the PSO key space stays small and every state a PSO can take is enumerable by the precache predictor.
- [Z2-N-REND-198] Front faces are counter-clockwise in every PSO on both APIs, matching the vertex winding the cook emits (see `Zenith2/Docs/Rendering/VertexData.md`).

## PSO precache sets

### Recorded set

The recorded set is an authoring document per game, `Zenith2/Games/<Name>/PSO/recorded.psoset.json`, produced by development runs with `--record-psos` and committed. Entries name programs, permutation constants, material variants, stream-sets and formats by name, not by hash, so they survive a hash change.

```json
{
	"schemaVersion": 1,
	"entries": [
		{
			"program": "Passes/ForwardPlus/Opaque",
			"permutation": { "uPERM_SHADOW_QUALITY": 2, "bPERM_FOG": true },
			"material": { "type": "Material_Standard", "module": "Zenith2/Shaders/Materials/Standard.slang", "permutation": { "bPERM_DETAIL_NORMAL": false } },
			"streamSet": "pos-snorm16x4|surface-qt16-uv16",
			"raster": { "cull": "back", "depth": "equal-test-no-write", "blend": ["opaque"] },
			"formats": { "colour": ["R11G11B10_FLOAT"], "depth": "D24S8" },
			"tier": "desktop"
		}
	]
}
```

### Predicted set

| Source | Enumerated |
|---|---|
| engine programs | every program in the global group × every permutation the tier configuration enables × every attachment-format set and depth format the pass can take |
| material programs | every material variant the game's content uses × every pass its domain draws in (depth-writing buckets, colour pass, shadow depth, transparent, decal) × every stream-set of the meshes that use it × both depth formats (D24S8 and D32F_S8, because the device picks) × every path the platform supports × the platform's quality tiers |
| fallback programs | the magenta fallback for every (attachment-format set, stream-set) pair in the predicted set |

### Cooked precache file

`pso_precache.zbin`, one per platform per game, versioned binary:

| Offset | Field | Type |
|---|---|---|
| 0 | magic `'ZPSO'` | u32 |
| 4 | version | u32 |
| 8 | groupCount | u32 |
| 12 | keyCount | u32 |
| 16 | group table | `{groupKind u8 (0 boot, 1 global, 2 scene, 3 streaming cell), pad[3], groupId u64 (scene or cell GUID hash), firstKey u32, keyCount u32}` × groupCount |
| … | keys | `Flux_PSOKey` serialized field by field, sorted by (group, key hash) |

- [Z2-N-REND-199] The cook builds each game's precache set as the union of its recorded set and its predicted set, partitioned into boot, global, per-scene and per-streaming-cell groups, and writes it into `pso_precache.zbin`.
- [Z2-N-REND-200] A recorded-set entry that no longer resolves (unknown program, constant, value or material) is a cook warning listing the entry, and the entry is dropped from the cooked set rather than failing the cook.
- [Z2-N-REND-201] A development run with `--record-psos` appends every PSO key it created that the loaded precache set lacked to `Zenith2/out/<checkout-hash>/PSO/<Game>.misses.psoset.json` in the recorded-set format, so the miss can be merged into the committed recorded set.

## PSO creation schedule

| Moment | Group created | Mechanism | Gate |
|---|---|---|---|
| boot | boot group (loading screen, UI, magenta fallbacks) | parallel creation jobs | first frame waits on it |
| boot, after the first frame | global group | parallel creation jobs at background priority | the first scene's activation waits on it |
| scene load | the scene's group | parallel creation jobs | scene activation waits on the group's operation handle |
| streaming cell load | the cell's group | parallel creation jobs at background priority, started when the cell's I/O starts | cell activation waits on the group's operation handle, like its other GPU resources |

- [Z2-N-REND-202] PSOs are created ahead of use by jobs on the worker pool (see `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`), grouped as the table above lists, and a scene or streaming cell is activated after its PSO group's operation handle completes, and not before.
- [Z2-N-REND-203] No PSO is created on the render-submit thread or inside a pass lambda; `Flux.PSO.SyncCreates` counts PSO creations reached from the draw path and is 0 in every build, which `Test_PSO_NeverSyncCreateOnSubmitThread` asserts.
- [Z2-N-REND-204] Each PSO creation uses the persistent pipeline cache (Vulkan `VkPipelineCache` passed to `vkCreateGraphicsPipelines`/`vkCreateComputePipelines`; D3D12 `ID3D12PipelineLibrary1::LoadPipeline`, falling back to creation plus `StorePipeline` on a library miss).

## Persistent pipeline caches

| API | File (under the platform user-cache directory `<UserCache>/Zenith2/<Game>/PipelineCache/`) | Keyed by |
|---|---|---|
| Vulkan | `vk_<vendor>_<device>_<driverVersion>_<cacheUUIDhex>_<containerVersion>.bin` | vendor ID, device ID, driver version, `pipelineCacheUUID`, shader container version |
| D3D12 | `d3d12_<vendor>_<device>_<umdVersion>_<agilitySDKVersion>_<containerVersion>.bin` | vendor ID, device ID, UMD driver version, Agility SDK version, shader container version |

Each file starts with a Zenith2 header: `{ magic u32 ('ZPCV' Vulkan, 'ZPCD' D3D12), headerVersion u32, vendorId u32, deviceId u32, driverVersion u64, uuid u8[16] (Vulkan) or agilityVersion u32 + pad (D3D12), containerVersion u32, payloadSize u64, payloadHash u64 }`, then the payload (`vkGetPipelineCacheData` output, or `ID3D12PipelineLibrary::Serialize` output).

- [Z2-N-REND-205] At device creation Flux loads the cache file whose name matches the current key; a missing file, a header mismatch, a payload hash mismatch or a driver rejection (`D3D12_ERROR_DRIVER_VERSION_MISMATCH`, `D3D12_ERROR_ADAPTER_NOT_FOUND`, or a Vulkan cache the driver ignores) deletes the file and starts an empty cache, and never fails device creation.
- [Z2-N-REND-206] Cache files are written by the platform file service's journaled atomic write (temporary file, flush, rename; see `Zenith2/Docs/Architecture.md → "Platform layer"`), at clean shutdown and at most once per 60 s after new PSOs were created, on a background job; an interrupted write leaves the previous file intact.
- [Z2-N-REND-207] Cache files older than the current key for the same game are deleted at startup, and the total cache size per game is capped by `Z2-B-RPROD-030` On-disk pipeline-cache size (pending); an over-cap cache is rebuilt from empty rather than trimmed.

## Missing-PSO fallback

| Build | Raster pass, direct draw | Raster pass, indirect bucket | Compute or fullscreen pass |
|---|---|---|---|
| shipping | the draw is skipped for that frame | the whole material bucket's indirect call is skipped for that frame | the pass is skipped and its full-coverage outputs are cleared to zero |
| development | the magenta fallback PSO draws it | the magenta fallback PSO draws the bucket | the pass is skipped and its full-coverage outputs are filled with the poison pattern |

<!-- elaboration -->
- [Z2-N-REND-208] `Flux_RasterContext::BindPipeline` looks the key up in the PSO table, whose entries are `READY`, `PENDING`, `ABSENT` or `FAILED` (`Flux_PSOState`); for anything but `READY` it applies the fallback of the table above for the current build type, returns `false`, and (for `ABSENT` alone) starts an asynchronous creation job, never a synchronous one.
- [Z2-N-REND-214] `FAILED` carries its reason (`Flux_PSOFailReason`): a blob failure (a corrupt blob or a missing permutation, [Z2-N-REND-122]) is never re-queued for the run and returns to `ABSENT` only on a shader reload; a device-side creation failure (`PIPELINE_CREATE`, `SHADER_BLOB_INVALID`) is never re-queued within the same device and returns to `ABSENT` when device-loss recovery recreates the device; each is logged once per key, and its draws take the fallback of the table above.
- [Z2-N-REND-209] The magenta fallback program `Zenith2/Shaders/Materials/Fallback.slang` writes `(1, 0, 1, 1)` to every colour output and draws with the pass's own raster state, and its PSOs are in the boot group for every (attachment-format set, stream-set) pair of the predicted set, so the development fallback itself never misses.
- [Z2-N-REND-210] Every fallback increments `Flux.PSO.MissBuckets` (one per skipped or fallback-drawn bucket or direct draw), `Flux.PSO.MissInstances` (the bucket's CPU-side candidate instance count, or 1 for a direct draw) or `Flux.PSO.MissPasses` (compute and fullscreen), all shown by the on-screen profiler, and logs the key once per run.
- [Z2-N-REND-211] A PSO miss is a precaching defect, held to the adopted `Z2-B-RPROD-025` Missing-PSO events per qualification run (0); the one-frame artefact it causes (for example a skipped shadow draw leaking light for one frame) is accepted, and no fallback path ever compiles a shader or creates a PSO synchronously.

## PSO counters and tests

| Counter | Meaning |
|---|---|
| `Flux.PSO.CreatedBoot`, `Flux.PSO.CreatedGlobal`, `Flux.PSO.CreatedScene`, `Flux.PSO.CreatedStreaming` | PSOs created per group kind |
| `Flux.PSO.CacheHits`, `Flux.PSO.CacheMisses` | persistent-cache hits and misses (D3D12 library; Vulkan creation-feedback `VK_PIPELINE_CREATION_FEEDBACK_APPLICATION_PIPELINE_CACHE_HIT_BIT` where reported) |
| `Flux.PSO.CreateMsP95` | 95th-percentile creation time over the run |
| `Flux.PSO.MissBuckets`, `Flux.PSO.MissInstances`, `Flux.PSO.MissPasses` | fallbacks applied |
| `Flux.PSO.SyncCreates` | creations reached from the draw path (always 0) |
| `Flux.PSO.TwoSided` | PSOs with culling off |

- [Z2-N-REND-212] The PSO counters of the table above exist (the RHI emits `RHI.Pipelines.CacheHits`/`CacheMisses` and returns a hit flag per creation, from which Flux derives `Flux.PSO.CacheHits`/`CacheMisses`), feed the on-screen profiler and telemetry, and are reported in every workload run's cold-versus-warm cache measurement (see `Zenith2/Docs/Budgets.md → "Measured in every workload run"`).
- [Z2-N-REND-213] The PSO tests `Test_PSO_KeyIncludesFormatsFromGraph`, `Test_PSO_KeyEqualityIsBytewise`, `Test_PSO_MissingShippingSkipsBucket`, `Test_PSO_MissingDevelopmentDrawsMagenta`, `Test_PSO_MissingComputeClearsOutputs`, `Test_PSO_NeverSyncCreateOnSubmitThread`, `Test_PSO_CacheRejectsDriverMismatch`, `Test_PSO_CacheInterruptedWriteKeepsPrevious`, `Test_PSO_RecordedSetRoundTrip`, `Test_PSO_UnresolvedRecordedEntryWarns` and the cook test `Test_Cook_PSOPredictedSetCoversWorkloads` (every workload's scripted playthrough in development mode records zero misses against the cooked set) exist and pass.
