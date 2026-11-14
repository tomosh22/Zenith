# RPROD design: the offline shader compiler, shader loading and pipeline state objects

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies the internals of the `ZenithShaderCompiler` host tool, the runtime shader library, PSO keys and their fixed state tables, the PSO table and its states, the precache sets (the cook step, the file, the runtime API), the creation schedule, the missing-PSO fallback per build type, PSO recording in development runs, the shader and PSO lints, and the permutation limit.

## Scope of this file

The specification is `Zenith2/Docs/Rendering/ShaderSystem.md` (every heading) with `Zenith2/Docs/Rendering.md → "Shader system and offline compilation"`, "Pipeline state objects" and "GPU performance rules and lints". The persistent pipeline-cache store, its file names, invalidation and write cadence are RHI's (`Zenith2/Docs/Design/RHI/Recovery.md → "Pipeline cache store"`); this file uses the store.

### Ownership of the shader toolchain

| Piece | Owner | Why |
|---|---|---|
| `Zenith2/Tools/ZenithShaderCompiler/` (the tool, its lints, watch mode) | RPROD | `Zenith2/Docs/Architecture.md → "Repository layout"` (table "Ownership per directory") and Z2-I-RPROD-13, -14 |
| `Zenith2/Engine/ZenithShadersGenerated/` (the `KIND GENERATED` target and its commands) | RPROD | the same table |
| `Zenith2/Shaders/` (the tree, `Common/` modules' structure, `Passes/` programs of RPROD's passes) | RPROD; each area writes its own modules and programs inside it | the same table |
| the Slang and DXC manifest pins, the setup script's host-tool install | SHIP (`Zenith2/Docs/Design/SHIP.md`, pending) | `Zenith2/Docs/Architecture.md → "Third-party dependencies"` (Z2-D-008, Z2-D-082) |
| the editor's reload watcher | EDIT consumes the journal RPROD's tool writes ([Z2-N-REND-131]) | `Zenith2/Docs/Design/EDIT.md` (pending) |
| the PSO precache cook step in `ZenithCook` | RPROD (files under `Zenith2/Tools/ZenithCook/Internal/Steps/PSO/`) | as GEOM owns its cook steps inside ZenithCook |
| DEVT | no share: the profiler displays the PSO counters | — |

## ZenithShaderCompiler files

```cmake
# Zenith2/Tools/ZenithShaderCompiler/CMakeLists.txt
zenith_add_tool(ZenithShaderCompiler DEPENDS ZenithCore ZenithVertexFormat THIRD_PARTY slang dxc SUPPORTED ZENITH_TOOLS_HOST)
```

<!-- elaboration -->
| File (`Zenith2/Tools/ZenithShaderCompiler/`) | Contents |
|---|---|
| `Internal/Zenith_ShaderCompilerMain.cpp` | `main`: argument parse, mode dispatch, exit codes of `Zenith2/Docs/Rendering/ShaderSystem.md → "The ZenithShaderCompiler tool"` |
| `Internal/Zenith_ShaderCompilerOptions.h/.cpp` | `Zenith_ShaderCompilerOptions` and the command-line parser (every flag of the specification plus `--shared-types` and GEOM's `--stream-set-list`) |
| `Internal/Zenith_ShaderSession.h/.cpp` | the Slang global session (one per process), module loading, SPIR-V 1.6 and DXIL SM 6.6 targets ([Z2-N-REND-111]) |
| `Internal/Zenith_ShaderDxc.h/.cpp` | DXC invocation for DXIL, `-enable-16bit-types`, warnings as errors; in process through `DxcCreateInstance` and `IDxcCompiler3`, linking SHIP's `dxc` entry (`inc`, `lib/x64/dxcompiler.lib`) with `dxcompiler.dll` and `dxil.dll` staged beside the tool (row SH19) |
| `Internal/Zenith_ShaderReflection.h/.cpp` | parameter struct, field table, entry points, vertex input, outputs per permutation, depth export |
| `Internal/Zenith_ShaderPermutations.h/.cpp` | the enumeration rule (seven steps), key formula ([Z2-N-REND-119]), `[PermutationRequires]` and `[PermutationValueRequires]` collapse |
| `Internal/Zenith_ShaderHeaderWriter.h/.cpp` | `<Name>.generated.h` with contract blocks ([Z2-N-CONTRACT-114]), size and offset asserts, the field table, the per-stream-set vertex tables |
| `Internal/Zenith_ShaderSharedTypes.h/.cpp` | the `--shared-types` aggregate `Flux_SharedTypes.generated.h` ([Z2-N-REND-135]) |
| `Internal/Zenith_ShaderContainerWriter.h/.cpp` | the `.zshader` container, byte for byte as the specification's table, little-endian, written field by field |
| `Internal/Zenith_ShaderReflectionJson.h/.cpp` | `Reflection/<ProgramPath>.reflection.json` and the material reflection files |
| `Internal/Zenith_ShaderLints.h/.cpp` | the twelve lints of "Shader lint implementation" |
| `Internal/Zenith_ShaderDepfile.cpp`, `Internal/Zenith_ShaderManifest.cpp` | the Ninja depfile and `shaders.manifest.json` |
| `Internal/Zenith_ShaderCheckMode.cpp` | `--check` ([Z2-N-REND-112]) |
| `Internal/Windows/Zenith_ShaderWatch_Windows.cpp` | `--watch` over `ReadDirectoryChangesW`, the 200 ms debounce, the reload journal ([Z2-N-REND-128]) |
| `Tests/…` | `Zenith2/Docs/Design/RPROD/Tests.md → "PSO and shader tests"` |

The tool is a host executable in `ZENITH_TOOLS_HOST`; Android presets run the host binary through `ZENITH_HOST_TOOLS_DIR` ([Z2-N-REND-109]); no runtime target links it or Slang or DXC ([Z2-N-REND-004]).

## Compiler pipeline

Once per build, before any per-program command, the aggregate `--stream-sets` command of `ZenithShadersGenerated` (row 0 below, GEOM's rules of `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`); then per program, in this order:

| # | Step | Failure |
|---|---|---|
| 0 | (aggregate, once) reflect every `[StreamSet(<code>)]` struct under `Zenith2/Shaders/Common/`, validate it against its code, check the declared set equals the reachable set, parse `Zenith2/Shaders/StreamSets.list`, write `Flux/Shaders/Flux_StreamSets.generated.h` | the seven `ZSC_ERROR_STREAM_SET_*` codes and `ZSC_ERROR_VERTEX_FORMAT` ("Compiler error codes") |
| 1 | load the program and its imports from the shader roots; record every source for the depfile | `ZSC_ERROR_SOURCE_MISSING` |
| 2 | validate the vocabulary: one `[FluxParams]` struct ([Z2-N-REND-113]), known attributes on legal targets ([Z2-N-REND-115]), `[FluxShared]` placement and member types ([Z2-N-REND-134], [Z2-N-REND-136]) | `ZSC_ERROR_PARAMS_COUNT`, `…_UNKNOWN_ATTRIBUTE`, `…_ATTRIBUTE_TARGET`, `…_SHARED_TYPE`, `…_SHARED_DUPLICATE`, `…_SHARED_LOCATION`, `…_TILE_LOCAL_UNSUPPORTED` |
| 3 | reflect the parameter struct with the scalar layout; fail above `uFLUX_MAX_PARAMS_BYTES` (2,048) | `ZSC_ERROR_PARAMS_TOO_LARGE` |
| 4 | enumerate the permutations (steps 1–7 of the specification), with stream-sets from the program's family and `StreamSets.list` or the cook's `--stream-set-list`, and material variants from `--material-set`; derive each drawable code's specialised vertex table with its surface substitution and null-stream attributes (GEOM's "Program stream-set specialisation") | `ZSC_ERROR_PERMUTATION_LIMIT` (exit 5) when the per-game total exceeds `cook.permutationLimitPerGame`; `ZSC_ERROR_STREAM_SET_NAME` for a `--stream-set-list` name that is not declared |
| 5 | compile each (key, stream-set, material variant) for each requested API and tier; SPIR-V through Slang, DXIL through Slang to HLSL then DXC | `ZSC_ERROR_COMPILE` (with the Slang or DXC message) |
| 6 | reflect outputs per permutation (always-written, never-written, conditionally written colour outputs; depth export); reflect each entry point's `uniform` parameters as the target lays them out (`EntryPointReflection::getVarLayout()`): exactly one, of type `Flux_PushConstants`, 8 bytes at offset 0, which Slang places in the entry point's one push-constant block on SPIR-V and in one constant buffer at `b0, space0` on the D3D target, the register root parameter 0 of the global root signature binds as two 32-bit root constants | `ZSC_ERROR_PUSH_CONSTANTS` |
| 7 | run the lints on the program and each permutation | exit 4 on an unallowlisted `fail` |
| 8 | write the header (only when its bytes change, [Z2-N-REND-110]), reflection JSON, lint file, container (temporary file and rename) | `ZSC_ERROR_OUTPUT_WRITE` |

The compiler is deterministic: permutations compile in key order on `--jobs` worker threads whose results are gathered in key order, and no output carries a timestamp or path of the build machine ([Z2-N-REND-110]).

## Compiler error codes

<!-- elaboration -->
```cpp
enum Zenith_ZscError : uint16   // Internal/Zenith_ShaderCompilerOptions.h; the printed code is ZSC<value, 4 digits>
{
	ZSC_ERROR_USAGE = 1, ZSC_ERROR_SOURCE_MISSING = 2, ZSC_ERROR_PARAMS_COUNT = 3, ZSC_ERROR_PARAMS_TOO_LARGE = 4,
	ZSC_ERROR_UNKNOWN_ATTRIBUTE = 5, ZSC_ERROR_ATTRIBUTE_TARGET = 6, ZSC_ERROR_SHARED_TYPE = 7, ZSC_ERROR_SHARED_DUPLICATE = 8,
	ZSC_ERROR_SHARED_LOCATION = 9, ZSC_ERROR_TILE_LOCAL_UNSUPPORTED = 10, ZSC_ERROR_COMPILE = 11, ZSC_ERROR_PERMUTATION_LIMIT = 12,
	ZSC_ERROR_OUTPUT_WRITE = 13, ZSC_ERROR_STREAM_SET_CODE = 14, ZSC_ERROR_MATERIAL_SET = 15, ZSC_ERROR_INTERNAL = 16,
	ZSC_ERROR_STREAM_SET_MISMATCH = 17, ZSC_ERROR_STREAM_SET_DUPLICATE = 18, ZSC_ERROR_STREAM_SET_LOCATION = 19,
	ZSC_ERROR_STREAM_SET_MISSING = 20, ZSC_ERROR_STREAM_SET_UNREACHABLE = 21, ZSC_ERROR_STREAM_SET_NAME = 22,
	ZSC_ERROR_VERTEX_FORMAT = 23, ZSC_ERROR_PUSH_CONSTANTS = 24,
};
```

Every error prints `file(line,col): error ZSC####: message` and maps to exit code 1 except usage (2), the limit (5), lint failures (4) and internal errors (6). The printed numbers are stable: a new code takes the next value, which is why the stream-set codes after the first sit above `ZSC_ERROR_INTERNAL`.

The stream-set codes and the pipeline step that raises each are GEOM's rules (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`, GEOM row GM24); their rows in `Zenith2/Docs/Rendering/ShaderSystem.md → "The ZenithShaderCompiler tool"` are Rendering's since X138 (applied in core-doc batch 4).

| Code | Raised at | Condition |
|---|---|---|
| `ZSC_ERROR_VERTEX_FORMAT` | step 0 | a `[StreamSet]` struct violates the vertex-format vocabulary ([Z2-N-REND-868] to [Z2-N-REND-870]) |
| `ZSC_ERROR_STREAM_SET_CODE` | step 0 | the attribute's argument does not fold to a code `Zenith_ValidateStreamSetCode` accepts; names the struct and the value |
| `ZSC_ERROR_STREAM_SET_MISMATCH` | step 0 | the reflected layout does not hold exactly the code's attributes in the code's formats (`Zenith_CheckStreamSetDeclaration`), or a family struct of `MeshVertex.slang` lacks its family default's declaration; names the struct, the code and the first differing attribute |
| `ZSC_ERROR_STREAM_SET_DUPLICATE` | step 0 | two structs declare one code; names both |
| `ZSC_ERROR_STREAM_SET_LOCATION` | step 0 | `[StreamSet]` outside `Common/`, or on a struct with a field that is not a vertex input |
| `ZSC_ERROR_STREAM_SET_MISSING` | step 0 | a reachable code (`Zenith_IsReachableStreamSet`) has no struct; names its canonical name |
| `ZSC_ERROR_STREAM_SET_UNREACHABLE` | step 0 | a declared MESH, TERRAIN or PROCEDURAL code the reachable rule rejects; names the struct |
| `ZSC_ERROR_STREAM_SET_NAME` | step 0 (`StreamSets.list`), step 4 (`--stream-set-list`) | a name that does not parse (`Zenith_ParseStreamSetName`) to a declared code |
| `ZSC_ERROR_PUSH_CONSTANTS` | step 6 | an entry point whose reflected `uniform` parameters are not exactly one 8-byte `Flux_PushConstants` at offset 0, laid out as the one SPIR-V push-constant block or the one D3D constant buffer at `b0, space0`; names the program, the entry point, the target and the reflected size and location |

`ZSC_ERROR_PUSH_CONSTANTS` is RPROD's code, and its row in `Zenith2/Docs/Rendering/ShaderSystem.md → "The ZenithShaderCompiler tool"` is owed by Rendering. No shader declares the push constant with a `vk::` attribute or a `register`: the entry point's `uniform Flux_PushConstants` parameter is the declaration, Slang derives its placement on each target, and the C++ side takes the size, the SPIR-V push-constant placement and the D3D register and space from the reflection this step checks, so ([Z2-N-REND-061], [Z2-N-REND-998]) the global layouts of `Zenith2/Docs/Design/RHI/Vulkan.md → "Pipeline layout and pipelines"` and `Zenith2/Docs/Design/RHI/D3D12.md → "Root signature and command signatures"` hold exactly what every program reflects.

The tool uses the STL (a host tool, [Z2-N-CONTRACT-271] allows it) and never throws (exceptions stay off in every target but the Luau firewall).

## Compiler additions adopted from GEOM

RPROD adopts the compiler additions GEOM specifies (reconciliation rows GM3 and GM4, amended by GM24), each landing with its ShaderSystem table row in the same diff:

| Addition | Specified in |
|---|---|
| the struct attribute `[StreamSet(<integer code expression>)]` in the closed vocabulary (folded by Slang, read from the reflection's attribute argument); the modules `Zenith2/Shaders/Common/StreamSetFields.slang` (the `<AXIS>_<VALUE>_FIELDS` and `_BITS` segment macros) and `StreamSets.slang` (the `FLUX_SS_DECLARE` generator over the 255 reachable codes) | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"` |
| the aggregate command `ZenithShaderCompiler --stream-sets --shader-root Zenith2/Shaders --header-dir ${ZENITH_OUT}/generated/ZenithShadersGenerated/ --depfile <file>`, writing `Flux/Shaders/Flux_StreamSets.generated.h` (`axFLUX_STREAMSET_ATTRIBUTES_<CODE>`, `axFLUX_STREAMSET_STREAMS_<CODE>`, `axFLUX_STREAMSET_LAYOUTS[]`, ascending code order, byte-deterministic), on whose output every per-program command depends; its seven stream-set error codes | same heading; "Compiler error codes" above |
| `--stream-set-list <file>` (cook mode), per-program specialised tables `axFLUX_<PROGRAM>_VERTEX_ATTRIBUTES_<CODE>`/`_STREAMS_<CODE>`/`_LAYOUTS[]` for every drawable code with the surface substitution and the null stream (stream `uVERTEX_NULL_STREAM` = 7, input rate `VERTEX`, stride 0, the null attributes packed at offsets 0, 4, 8 in the family default's formats), the link-time constants `uFLUX_STREAMSET_FAMILY`, `uFLUX_STREAMSET_POSITION`, `uFLUX_STREAMSET_SURFACE`, `bFLUX_STREAMSET_UV0`, `bFLUX_STREAMSET_UV1`, `bFLUX_STREAMSET_COLOUR`, `Zenith2/Shaders/StreamSets.list` | `Zenith2/Docs/Design/GEOM/VertexFormat.md → "Program stream-set specialisation"` |
| `Flux_MeshVertex` gains `[Stream(2)] [VtxFmt("unorm16x2")] float2 m_xUV1 : TEXCOORD1;` and `[Stream(2)] [VtxFmt("unorm8x4")] float4 m_xColour : COLOR0;`; the family-struct rule reads "contains the family default's declaration" | same heading (GEOM row GM24) |
| `[FluxShared]` `Flux_ProceduralRecord` (64 bytes) and `Flux_ProceduralDispatchData` (16 bytes) in `Zenith2/Shaders/Common/Procedural.slang` | `Zenith2/Docs/Design/GEOM/Procedural.md → "Per-frame regeneration"` |
| `Reflection/Materials/<ModulePath>/<Type>.material.json` and the `[TextureUsage]`, `[UVChannel]` attributes | `Zenith2/Docs/Design/GEOM/Materials.md → "Material reflection input"` |
| `[FluxShared]` `Flux_MeshletRecord`, `Flux_LODView`, `Flux_TerrainRecord`; GEOM's layouts of `Flux_DecalRecord` and `Flux_SkinDispatchRecord`; the Common modules `VertexKernel.slang` and `MeshVertex.slang` | `Zenith2/Docs/Design/GEOM/Meshes.md → "Meshlets"`, "LOD selection", "Terrain geometry"; `Zenith2/Docs/Design/GEOM/Materials.md → "Decal records"` |

The vocabulary half of these additions (the `[StreamSet]` attribute in [Z2-N-REND-115]'s closed list, the `--stream-sets` command and generated header in "The ZenithShaderCompiler tool" and "Generated parameter headers", the null-stream rows of [Z2-N-REND-116]'s element table, the `Flux_ProceduralRecord` and `Flux_ProceduralDispatchData` rows of "Shared GPU structs") is in `Zenith2/Docs/Rendering/ShaderSystem.md` since X138.

## Shader blob loading

<!-- elaboration -->
`Flux_ShaderLibrary` (`Internal/PSO/Flux_ShaderLibrary.h/.cpp`) loads `.zshader` containers from the cooked package through ZenithAssets' reader (`Zenith2/Docs/Design/ASSET.md`, pending; the same assumption GEOM records), on asset-loader jobs:

| Step | Rule |
|---|---|
| header | refuse a mismatched magic, container version or tool hash with `FLUX_SHADER_ERROR_CONTAINER_VERSION` naming the file ([Z2-N-REND-121]) |
| tables | read the entry, permutation and blob-reference tables into the heap; blobs stay in the mapped cooked bytes until their PSOs exist |
| permutation flags | the `flags` field of each permutation entry, whose bits "Permutation flags bits" below defines; the graph's attachment checks and the PSO builder's write masks read them (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Compile-time rule checks"`) |
| blob hash | verified at the blob's first use for PSO creation; a mismatch is `FLUX_SHADER_ERROR_CORRUPT_BLOB` and every PSO entry created from that blob becomes `FLUX_PSO_STATE_FAILED` with reason `FLUX_PSO_FAIL_REASON_BLOB`, which stands for the run (a shader reload of the program alone returns it to `ABSENT`), logged once, with no further creation job ([Z2-N-REND-122]; "PSO table and states") |
| lookup | `Find(program, permutation key) → blob reference` by binary search over the sorted permutation table |

```cpp
enum Flux_ShaderError : uint8 { FLUX_SHADER_ERROR_CONTAINER_VERSION = 1, FLUX_SHADER_ERROR_CORRUPT_BLOB, FLUX_SHADER_ERROR_MISSING_PROGRAM, FLUX_SHADER_ERROR_MISSING_PERMUTATION, FLUX_SHADER_ERROR_IO };
```

### Permutation flags bits

`Zenith2/Docs/Rendering/ShaderSystem.md → "Shader blob container"` gives the permutation entry a `flags u32` and leaves its bits undefined. The bits are Rendering's since X101 (the container table carries them), so the graph's `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT` check rests on a Rendering-defined encoding. The compiler writes the bits (pipeline step 6) and the runtime reads them.

| Bits | Meaning |
|---|---|
| 0–7 | bit n set: colour output n is written on every path of the permutation's pixel entry point |
| 8–15 | bit 8 + n set: colour output n is never written (the PSO builder forces its write mask to 0, [Z2-N-REND-921]) |
| 16 | the permutation exports depth (`SV_Depth*`), allowed only for allowlisted programs (lint `NoDepthExport`) |
| 17 | the permutation's pixel entry point is the `[AlphaTest]` entry |
| 18–31 | zero; a reader refuses a container with a set bit here (`FLUX_SHADER_ERROR_CONTAINER_VERSION`) |

A colour output whose bits n and 8 + n are both clear is conditionally written, which the `ConditionalOutputWrite` lint fails ([Z2-N-REND-921]); both set is a compiler defect the container writer asserts against.

## Flux_PSO header

`Zenith2/Engine/ZenithFlux/Flux_PSO.h` declares `Flux_RasterState`, `Flux_AttachmentFormats` and `Flux_PSOKey` exactly as `Zenith2/Docs/Rendering/ShaderSystem.md → "PSO keys"` gives them ([Z2-N-REND-195]), plus:

<!-- elaboration -->
```cpp
enum Flux_CullMode : uint8 { FLUX_CULL_MODE_BACK = 0, FLUX_CULL_MODE_FRONT = 1, FLUX_CULL_MODE_NONE = 2 };
enum Flux_FillMode : uint8 { FLUX_FILL_MODE_SOLID = 0, FLUX_FILL_MODE_WIREFRAME = 1 };
enum Flux_Topology : uint8 { FLUX_TOPOLOGY_TRIANGLE_LIST = 0, FLUX_TOPOLOGY_LINE_LIST = 1, FLUX_TOPOLOGY_POINT_LIST = 2 };
enum Flux_BlendStateIndex : uint8 { FLUX_BLEND_OPAQUE = 0, FLUX_BLEND_ALPHA = 1, FLUX_BLEND_PREMULTIPLIED = 2, FLUX_BLEND_ADDITIVE = 3, FLUX_BLEND_DECAL_CHANNELS = 4 };
enum Flux_StencilStateIndex : uint8 { FLUX_STENCIL_DISABLED = 0, FLUX_STENCIL_WRITE_REFERENCE = 1, FLUX_STENCIL_TEST_EQUAL = 2, FLUX_STENCIL_TEST_NOT_EQUAL = 3 };
enum Flux_DepthBiasIndex : uint8 { FLUX_DEPTH_BIAS_NONE = 0, FLUX_DEPTH_BIAS_SHADOW = 1, FLUX_DEPTH_BIAS_DECAL = 2 };
enum Flux_DepthFlag : uint8 { FLUX_DEPTH_FLAG_TEST = 1u << 0, FLUX_DEPTH_FLAG_WRITE = 1u << 1, FLUX_DEPTH_FLAG_CLAMP = 1u << 2 };

Flux_RasterState Flux_MakeRasterState(const Flux_DepthMode& xMode, bool bColourPass, Flux_CullMode eCull, Flux_BlendStateIndex eBlend);   // compare from Flux_DepthModeOps
uint64 Flux_HashPSOKey(const Flux_PSOKey& xKey);                // Zenith_Hash64 of the 56 bytes ([Z2-N-REND-195])
bool Flux_PSOKeyEqual(const Flux_PSOKey& xA, const Flux_PSOKey& xB);   // memcmp of the 56 bytes
```

Contract blocks: `Flux_RasterState`, `Flux_AttachmentFormats` and `Flux_PSOKey` are `value` blocks (`IMMUTABLE`, with "Members non-const: the PSO builder fills each field in turn and the PSO table copy-assigns keys into its entries.", bytes serialized field by field in the precache file, padding always zero); the enumerations are `enum` blocks whose values are part of the cooked precache file and never renumber.

## Fixed state tables

`Zenith2/Engine/ZenithFlux/Internal/PSO/Flux_FixedStates.h` ([Z2-N-REND-197]):

| Table | Entries |
|---|---|
| blend | 0 opaque (off); 1 alpha (colour `src·a + dst·(1−a)`, alpha `a + dst_a·(1−a)`); 2 premultiplied (`src + dst·(1−a)`); 3 additive (`src·a + dst`, alpha unchanged); 4 decal channels (alpha blend on colour, write mask per the decal permutation) |
| stencil | 0 disabled; 1 write the reference (always, replace); 2 test equal to the reference; 3 test not equal |
| depth bias | 0 none; 1 shadow `FLUX_DEPTH_BIAS_SHADOW` (constant 0, slope-scaled 2.0, clamp 0: LIGHT's values, `Zenith2/Docs/Design/LIGHT/Shadows.md → "Shadow depth rendering"`, which name the entry `FLUX_RASTER_BIAS_SHADOW`; the enumerator name is this table's and LIGHT conforms, reconciliation row RP19); 2 decal (constant −1, slope −1) |

Changing a table entry changes PSO contents under unchanged keys, so it bumps `uFLUX_SHADER_CONTAINER_VERSION` and invalidates persistent caches, like the sampler table ([Z2-N-REND-058]).

## PSO table and states

<!-- elaboration -->
`Flux_PSOTable` (`Internal/PSO/Flux_PSOTable.h/.cpp`) maps `Flux_PSOKey` to one entry `{ state word, Zenith_RHIPipelineHandle, group, first-use frame, m_bNullStream }`, whose state word is one `std::atomic<uint16>` holding the `Flux_PSOState` in its low byte and the `Flux_PSOFailReason` in its high byte. The state is one enumeration, used by this table, by "Missing-PSO fallback realisation" and by the entry document:

```cpp
// Internal/PSO/Flux_PSOTable.h
enum Flux_PSOState : uint8
{
	FLUX_PSO_STATE_READY = 1,     // the pipeline exists; BindPipeline binds it
	FLUX_PSO_STATE_PENDING,       // a creation job is queued or running
	FLUX_PSO_STATE_ABSENT,        // no entry for the key, or an entry reset by device-loss recovery or a shader reload
	FLUX_PSO_STATE_FAILED,        // creation failed or the blob is corrupt, with a Flux_PSOFailReason ([Z2-N-REND-208])
};

enum Flux_PSOFailReason : uint8
{
	FLUX_PSO_FAIL_REASON_NONE = 0,     // the entry is not FAILED
	FLUX_PSO_FAIL_REASON_BLOB = 1,     // a corrupt blob or a missing permutation: a property of the cooked bytes
	FLUX_PSO_FAIL_REASON_DEVICE,       // PIPELINE_CREATE or SHADER_BLOB_INVALID from the RHI: a property of the device and driver
};
```

The state and the reason change together, by one release store of the 16-bit state word, and are read together by one acquire load, so a reader never sees `FAILED` without its reason; the fallback and the log read the reason from that load.

[Z2-N-REND-208] admits `FLUX_PSO_STATE_FAILED` as the table's fourth state (X101 and X144 applied): without it a permanently uncreatable key (a creation failure, or a corrupt blob, [Z2-N-REND-122]) would sit in `ABSENT`, whose fallback queues a creation job, and re-queue one every frame. The state carries its reason: a blob failure is never re-queued for the run and returns to `ABSENT` only on a shader reload; a device-side failure is never re-queued within the same device and returns to `ABSENT` when device-loss recovery recreates the device; each is logged once per key.

| State | Event | Next | Action |
|---|---|---|---|
| `ABSENT` | a group request or a record-time miss | `PENDING` | one creation job is queued at the group's priority; a second miss of a `PENDING` key queues nothing |
| `PENDING` | the job creates the pipeline through the RHI with the pipeline cache | `READY` | the handle is published with a release store; `Flux.PSO.Created<Group>` |
| `PENDING` | the blob hash mismatches (`FLUX_SHADER_ERROR_CORRUPT_BLOB`) or the permutation is missing from its container (`FLUX_SHADER_ERROR_MISSING_PERMUTATION`) | `FAILED`, reason `BLOB` | logged once, `pso: creation failed <key names> (blob): <shader error>`; draws take the `FAILED` fallback; no job is queued for the key again until a reload |
| `PENDING` | the RHI returns `PIPELINE_CREATE` or `SHADER_BLOB_INVALID` | `FAILED`, reason `DEVICE` | logged once, `pso: creation failed <key names> (device): <RHI error>`; draws take the `FAILED` fallback; no job is queued for the key again on this device |
| `READY` | a shader reload of the program (editor, [Z2-N-REND-129]) | `READY` (replacement `PENDING` beside it) | the old handle stays bound until the replacement is `READY`, then swaps and the old one retires |
| `FAILED` (either reason) | a shader reload of the program (a new blob) | `ABSENT` | the reason is cleared; the next miss queues a creation job from the new blob |
| `READY`, `PENDING` | device-loss recovery | `ABSENT` | the table is rebuilt from the boot and resident groups (their keys are requested again) |
| `FAILED`, reason `DEVICE` | device-loss recovery | `ABSENT` | the reason is cleared: the recreated device (and, after a driver update, an invalidated pipeline cache, `Zenith2/Docs/Design/RHI/Recovery.md → "Pipeline cache store"`) may create what the old device refused; a boot or resident key is requested again with its group, any other key at its next miss; a second failure on the new device is logged once more |
| `FAILED`, reason `BLOB` | device-loss recovery | `FAILED`, reason `BLOB` | the cooked bytes are unchanged, so the failure stands; nothing is logged again |

No creation is retried within one device. A device-side refusal on a live device is either a deterministic driver verdict on these inputs, which a retry repeats, or driver memory exhaustion during device-memory pressure, whose defined response is RHI's device-memory path (`Zenith2/Docs/Design/RHI/Memory.md → "Device memory exhaustion"`), not a PSO timer; and a retry mid-run would be a PSO creation outside the precache schedule ([Z2-N-REND-202]) that a qualification run, which requires zero misses (`Z2-B-RPROD-025`), would record anyway. Recovery onto a new device is therefore the one event that clears a `DEVICE` failure.

Storage is an open-addressed `Zenith_HashMap<uint64, uint32>` from the key hash to an entry index, with full-key comparison on hit; a key with no entry is `ABSENT`. The table is read lock-free by record jobs (entries are appended, never moved; the state and the reason are the one atomic 16-bit state word above) and written by creation jobs and the build node. Its memory is `Z2-B-RPROD-002` PSO cache and pipeline objects.

## PSO builder

`Flux_PSOBuilder` turns a key into an RHI desc: the program's blobs for each stage from the shader library; the vertex input from the program's specialised layout table for the key's stream-set ([Z2-N-REND-865]; `axFLUX_<PROGRAM>_VERTEX_LAYOUTS[]`, including, when the table has a null stream, the binding 7 at stride 0 and input rate `VERTEX` with one attribute per null attribute: Vulkan `VkVertexInputBindingDescription { 7, 0, VK_VERTEX_INPUT_RATE_VERTEX }`, D3D12 `D3D12_INPUT_ELEMENT_DESC { …, InputSlot = 7, AlignedByteOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }`, GEOM's "Program stream-set specialisation"); the raster state expanded from `Flux_RasterState` and the fixed tables; the colour write masks forced to 0 for every output the permutation never writes ([Z2-N-REND-921]); the attachment formats from the key; front faces counter-clockwise ([Z2-N-REND-198]); the debug name the key hash in hex. It runs on creation jobs alone ([Z2-N-REND-203]). The builder records, in the entry, whether the table has a null stream (`m_bNullStream`), which the raster context reads at `BindPipeline` to bind GEOM's `Flux_NullVertexStream` at binding 7 (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Pass context implementation"`, GEOM row GM25).

## Precache API

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_PSOPrecache.h
enum Flux_PSOGroupKind : uint8 { FLUX_PSO_GROUP_BOOT = 0, FLUX_PSO_GROUP_GLOBAL = 1, FLUX_PSO_GROUP_SCENE = 2, FLUX_PSO_GROUP_STREAMING_CELL = 3 };
enum Flux_PSOError : uint8 { FLUX_PSO_ERROR_UNKNOWN_GROUP = 1, FLUX_PSO_ERROR_QUEUE_FULL, FLUX_PSO_ERROR_DEVICE_LOST };

/// @contract type Flux_PSOPrecacher
/// Ownership: Owned by Flux_Renderer; reached through Flux_Renderer::GetPrecacher.
/// Thread affinity: ANY_THREAD. Scene and streaming code request groups from their load jobs.
/// Invalidation: A returned operation handle is owned by the requester; destroying it cancels the group's pending
///   creations that no other request shares.
/// Units: None.
/// Errors: FLUX_PSO_ERROR_UNKNOWN_GROUP for a scene or cell with no group in pso_precache.zbin (the handle completes at
///   once, logged at debug level); FLUX_PSO_ERROR_QUEUE_FULL when 64 group requests are pending (retry next frame).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_PSOPrecacher
{
public:
	Zenith_Result<Zenith_OperationHandle, Flux_PSOError> RequestGroup(Flux_PSOGroupKind eKind, uint64 ulGroupID);
	bool IsBootGroupReady() const;
	Flux_PSOStats GetStats() const;
};
```

A scene's activation and a streaming cell's activation wait on their group's operation handle as on their other GPU resources ([Z2-N-REND-202]); SCENE and STREAM call `RequestGroup` with the scene or cell GUID hash (`Zenith2/Docs/Design/SCENE.md`, `Zenith2/Docs/Design/STREAM.md`, pending).

## Precache cook step

<!-- elaboration -->
`Zenith2/Tools/ZenithCook/Internal/Steps/PSO/` (RPROD) builds `pso_precache.zbin` per game and platform after the material, mesh and permutation steps ([Z2-N-REND-199]):

| Step | Action |
|---|---|
| 1 | load `Games/<Name>/PSO/recorded.psoset.json`; resolve every entry by names; an unresolvable entry is a cook warning and dropped ([Z2-N-REND-200]) |
| 2 | predict: engine programs × enabled permutations × format sets and depth formats; material programs × used variants × passes of the domain × stream-sets × both depth formats × the platform's paths × tiers; the magenta fallback for every (format set, stream-set) (the table "Predicted set") |
| 3 | union, deduplicate by key bytes, assign groups: boot (loading screen, UI, fallbacks, the swapchain-format passes, [Z2-N-REND-985]), global (engine passes of every path the platform supports), scene (the material PSOs of a scene's resident content), streaming cell |
| 4 | run the PSO lints of "PSO lints" on every key; a failure is a cook error |
| 5 | write the file (the specification's layout, keys sorted by (group, key hash)) |
| 6 | report per group the key count in the cook report's `psos` member |

`Test_Cook_PSOPredictedSetCoversWorkloads` plays every workload's scripted path in development mode and asserts zero misses against the cooked set ([Z2-N-REND-213]).

## Precache file reader

`Flux_PSOPrecacheFile` reads `pso_precache.zbin` at renderer creation: magic `'ZPSO'`, version equal to the build's, group table bounds, key count bounds, every key's enumerators within range; any failure is `FLUX_RENDERER_ERROR_PRECACHE_FILE` in Shipping (the game cannot run without its precache set) and a warning with an empty set in development builds, so every PSO becomes a miss and is recorded.

## PSO creation schedule realisation

| Moment | Realisation ([Z2-N-REND-202]) |
|---|---|
| boot | `r.pso.creationJobs` background jobs at high priority create the boot group; the first frame's `BuildGraph` waits for `IsBootGroupReady` (a main-thread wait at startup, before the frame loop) |
| after the first frame | the global group at low background priority; the first scene's activation waits on its handle |
| scene load | the scene's group, started by SCENE when the scene's load starts |
| streaming cell | the cell's group at low priority, started when the cell's I/O starts |

Creation jobs call the RHI's `ANY_THREAD` pipeline creation with the pipeline cache of RHI's store ([Z2-N-REND-204]); no creation happens on the render-submit thread or in a pass lambda, which `Flux.PSO.SyncCreates` counts and asserts zero ([Z2-N-REND-203]).

## Missing-PSO fallback realisation

`Flux_RasterContext::BindPipeline` and the graph's compute bodies apply the table of `Zenith2/Docs/Rendering/ShaderSystem.md → "Missing-PSO fallback"` ([Z2-N-REND-208] to [Z2-N-REND-211]):

| Entry state (`Flux_PSOState`) | Shipping | Development (`r.pso.missFallback = auto`) |
|---|---|---|
| `FLUX_PSO_STATE_READY` | bind | bind |
| `FLUX_PSO_STATE_PENDING` | return false; the bucket's or draw's calls are dropped | bind the magenta fallback PSO of the pass's (format set, stream-set) and return true |
| `FLUX_PSO_STATE_ABSENT` | as `PENDING`, and queue one asynchronous creation job, which moves the entry to `PENDING`, so a miss never queues a second job | as `PENDING`, and queue the job |
| `FLUX_PSO_STATE_FAILED` (either reason) | as `PENDING`; no job is queued | magenta; no job is queued |
| compute or fullscreen pass | skip the pass; its full-coverage outputs are cleared to zero by a graph-inserted clear | skip; outputs filled with the poison pattern |

Every fallback increments `Flux.PSO.MissBuckets` (or one per direct draw), `Flux.PSO.MissInstances` (the bucket's CPU candidate count, or 1) or `Flux.PSO.MissPasses`, and logs the key once per run; nothing compiles a shader or creates a PSO synchronously. `r.pso.missFallback = skip|magenta` forces one column in a development build. A qualification run requires zero misses (`Z2-B-RPROD-025`, [Z2-N-BUDGET-267]).

## PSO recording in development

With `--record-psos`, every key the run created that the loaded precache set lacked is appended once to `Zenith2/out/<checkout-hash>/PSO/<Game>.misses.psoset.json` in the recorded-set format, names resolved through the reflection files of the build ([Z2-N-REND-201]); `r.pso.dumpMisses` writes the current miss list on demand. A developer merges the file into the committed recorded set in an ordinary diff.

## Shader lint implementation

The twelve lints of `Zenith2/Docs/Rendering/ShaderSystem.md → "Shader lints"` run on the Slang IR after linking each permutation (so a helper reached from an entry point is checked in that context), except `NoVulkanAttributes`, which scans source text:

| Lint | Detection |
|---|---|
| `DiscardOnlyInAlphaTest` | a `discard` or `clip` in the call graph of an entry point not attributed `[AlphaTest]` |
| `NoDepthExport` | an output with `SV_Depth*` semantics in a program absent from `ShaderChecks_allowlist.json` |
| `NoMin16Float` | any `min16*`/`min10float` type in the IR |
| `HalfRatioReport` | counts of 32-bit and 16-bit float arithmetic instructions per entry point (report) |
| `ConditionalOutputWrite` | an output store not post-dominating the entry block and not absent on every path ([Z2-N-REND-921]) |
| `MaterialExplicitGradients` | an implicit-derivative sample instruction in a function of a material module |
| `VertexFormatVocabulary` | a `[VtxFmt]` string outside the closed vocabulary or with the wrong lane count |
| `ParamsSize` | the reflected params size above 2,048 |
| `BindlessThroughAccessors` | `ResourceDescriptorHeap`, `SamplerDescriptorHeap` or a raw descriptor array outside `Common/Bindless.slang` |
| `NoGlobalBindings` | an explicit `register` outside `Common/Params.slang` and `Common/Bindless.slang`; the push constant is the entry point's `uniform Flux_PushConstants` parameter, which carries none, and the D3D12 draw-ID root constant at `b1, space0` is the one `register` of `Common/Params.slang`, declared there beside `Flux_PushConstants` and read by `Common/DrawID.slang`'s `Flux_DrawIndex()`, which declares nothing itself |
| `NoVulkanAttributes` | any name in the `vk` namespace (an attribute such as `[[vk::binding]]`, `[[vk::push_constant]]` or `[[vk::location]]`, an intrinsic such as `vk::RawBufferLoad`, or a type) in any `.slang` file of `Zenith2/` outside `out/`, `ThirdParty/` and the gate fixtures, `Common/` included, found by a token scan of the source text because the linked IR keeps no attribute spelling; no allowlist entry admits one |
| `TileLocalRejected` | a `[TileLocal]` field until D-TBDR (open, Stage 5) is ruled |

The ShaderChecks gate step (a PROC script) aggregates the lint files and runs `--check` ([Z2-N-REND-112], [Z2-N-REND-127]); every lint has a failing fixture program under `Zenith2/Tools/ZenithShaderCompiler/Tests/Fixtures/Lints/`.

## PSO lints

Evaluated at cook time for every precached key and in development builds at PSO creation:

| Lint | Rule |
|---|---|
| `PSO.OpaqueCullBack` | an opaque or alpha-tested PSO with culling off whose material document does not set `twoSided` fails ([Z2-N-REND-923]); two-sided PSOs are counted in `Flux.PSO.TwoSided` and the cook report |
| `PSO.ColourPassEqualDepth` | a PSO whose program is a colour-after-prepass program (the classification below) has depth test on, depth write off and `m_eDepthCompare == EQUAL` ([Z2-N-REND-917]) |
| `PSO.UnwrittenOutputMaskZero` | every output the permutation never writes has write mask 0 ([Z2-N-REND-921]) |
| `PSO.BlendFormatFeature` | a blend-enabled attachment format has the B feature ([Z2-N-REND-038]) |

<!-- elaboration -->
The cook has no graph, so `PSO.ColourPassEqualDepth` classifies a PSO by its program, never by position in a frame. `Internal/PSO/Flux_ColourPassPrograms.h` holds a constexpr table of program paths with one predicate each, taken from the per-pass tables (`Zenith2/Docs/Rendering.md → "Per-pass specifications"`):

| Programs (the pass whose per-pass table names them) | Predicate on the PSO's `Flux_RasterState` |
|---|---|
| the opaque colour programs that redraw the prepass's geometry: `ForwardPlusOpaque`, `ForwardOpaquePerObject`, `GBuffer` | `FLUX_DEPTH_FLAG_TEST` set, `FLUX_DEPTH_FLAG_WRITE` clear, `m_eDepthCompare == EQUAL` (`Flux_DepthModeOps::GetColourCompare`), no `[AlphaTest]` entry (bit 17 of the permutation flags clear) |
| geometry the prepass never wrote: `TransparentForward`, `Particles` | `FLUX_DEPTH_FLAG_TEST` set, `FLUX_DEPTH_FLAG_WRITE` clear, `m_eDepthCompare == Flux_DepthModeOps::GetOpaqueCompare(mode)` for the PSO key's depth format |
| every other program (prepass, shadow and depth-only, visibility raster, fullscreen, sky, decal volume, compute) | no predicate: its depth state is its per-pass table's |

`Test_PSOLint_ColourPassListMatchesTables` reads the per-pass section files from the source tree, as `Test_Paths_MatrixMatchesData` reads the capability matrix, and asserts that every program in the table is the program row of the pass named beside it and that no other pass whose per-pass table binds depth read-only with an `EQUAL` compare is missing from the first row.

## Permutation limit

The compiler counts each program's compiled permutations, the cook sums them per game over every program and tier, and a total above `Z2-B-RPROD-024` fails the compile (exit 5) and the cook (`COOK_ERROR_PERMUTATION_LIMIT`, listing the ten largest programs) ([Z2-N-REND-126], [Z2-N-REND-253]); GEOM's cook step implements the per-game sum and report (`Zenith2/Docs/Design/GEOM/Materials.md → "Permutation limit check and report"`) and RPROD's compiler the per-invocation check. While the row is pending, `cook.permutationLimitPerGame` defaults to 0, meaning "report without failing".
