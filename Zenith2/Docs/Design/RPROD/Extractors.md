# RPROD design: the engine extractors and the render-instance delta

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It specifies the seam between simulation and rendering that RPROD owns: the render-instance key and delta records of `Flux_RenderWorld.h`, the two engine render component types (the renderable mesh and the camera), and the four engine extractors of `Zenith2/Engine/ZenithRuntime/Internal/Zenith_EngineExtractors.cpp` (the file `Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"` assigns to RPROD) with their registration, declared reads, algorithms, determinism, failure handling, counters and tests.

## Scope of this file

The specification is `Zenith2/Docs/Rendering.md → "Purpose and scope"` (headings "The render world hand-off" and "Render world resync", [Z2-N-REND-007] to [Z2-N-REND-009], [Z2-N-REND-015]) and `Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"` ([Z2-N-ARCH-415] to [Z2-N-ARCH-420]). The frame-phase rules the extractors obey are `Zenith2/Docs/Architecture/FramePhases.md → "The twelve phases"` ([Z2-N-ARCH-472], [Z2-N-ARCH-476], [Z2-N-ARCH-503]). The ordering of every extractor before `Render.Seal` is ECS's (`Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"`, the orchestrator's ruling (a) on Q40). The render-world consumers of what these extractors write are `Zenith2/Docs/Design/RPROD/Renderer.md → "Render world consumption"`, RHI's `Flux_GPUScene` (`Zenith2/Docs/Design/RHI/Memory.md → "Flux_GPUScene"`) and LIGHT's light mirror (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light extraction and the CPU light mirror"`).

| Extractor node | Writes | Stage | Heading |
|---|---|---|---|
| `Render.ExtractInstances` | `Flux_RenderInstanceDelta` records and history resets of every entity with a renderable-mesh component | S2 | "Instance extractor" |
| `Render.ExtractCameras` | one `Flux_ViewRequest` per active engine camera | S2 | "Camera extractor" |
| `Render.ExtractLights` | `Flux_RenderLightDelta` records of LIGHT's five light component types | S3 | "Light extractor" |
| `Render.ExtractEnvironment` | the world's one `Flux_RenderEnvironment` | S3 | "Environment extractor" |

The engine extracts nothing else: skinned poses are ANIM's extractor's (`Flux_RenderWorldWriter::AddSkinnedPose`, fed by ANIM's pose buffers, reconciliation row RP29), decals GEOM's and the decal-owning module's, emitter commands VFX's, UI lists UI's, debug draws DEVT's, ENV's terrain, water, foliage and per-view environment records ENV's (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), and procedural records their generators' (`Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`). Each of those areas registers its own extractor through the same `Zenith_WorldAttachContext::AddExtractor`.

## Extractor files

| Path | Contents | Library |
|---|---|---|
| `Zenith2/Engine/ZenithFlux/Flux_RenderWorld.h` | Rendering's render-world declarations with the additions of "Flux_RenderWorld header additions" | ZenithFlux (RPROD's diff) |
| `Zenith2/Engine/ZenithFlux/Flux_RenderComponents.h` | the two engine render component structs and their property-ID constants ("Flux_RenderComponents header") | ZenithFlux |
| `Zenith2/Engine/ZenithFlux/Internal/Renderer/Flux_RenderWorldWriter.cpp` | the writer's `AddInstanceDelta` flag mask, the environment span re-pointing of "Environment extractor" | ZenithFlux |
| `Zenith2/Engine/ZenithRuntime/Internal/Zenith_RenderTypes.h`, `.cpp` | `Zenith_RegisterRenderComponentTypes` ("Render component registration") | ZenithRuntime |
| `Zenith2/Engine/ZenithRuntime/Internal/Zenith_EngineExtractors.h`, `.cpp` | `Zenith_AttachEngineExtractors`: the registration of the four extractors and of the extraction state's lifecycle listener ("Extractor registration") | ZenithRuntime (MOD's file table names the `.cpp`) |
| `Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_EngineExtraction.h`, `.cpp` | the per-world extraction state and its lifecycle listener ("Engine extraction state") | ZenithRuntime |
| `Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_DeltaTracker.h`, `.cpp` | the delta tracker shared by the instance and light extractors ("Delta tracking") | ZenithRuntime |
| `Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_InstanceExtractor.cpp`, `Zenith_LightExtractor.cpp`, `Zenith_CameraExtractor.cpp`, `Zenith_EnvironmentExtractor.cpp` | the four extractor bodies | ZenithRuntime |
| `Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_ExtractionResolver.h`, `Zenith_AssetExtractionResolver.h`, `.cpp` | the resolution seam and its production implementation ("Extraction resolver") | ZenithRuntime |
| `Zenith2/Engine/ZenithRuntime/Tests/EngineExtractors/` | "Extractor tests", with the fixture resolver `Zenith_FixtureExtractionResolver.h` | `ZenithRuntime_Tests` |

The dependency route is Architecture's and MOD's: ZenithRuntime declares `ZenithECS`, `ZenithScene`, `ZenithReflect`, `ZenithJobs`, `ZenithAssets` and `ZenithFlux` in its `DEPENDS` (`Zenith2/Docs/Design/MOD.md → "ZenithRuntime files"`), so the extractors include ECS, SCENE and ZenithFlux headers, while ZenithFlux includes none of ZenithScene, ZenithECS or ZenithScript ([Z2-N-REND-005], [Z2-N-ARCH-076]). No extractor includes a module header: every type an extractor reads that a module produces is a ZenithFlux type the module declares (ENV's `Flux_EnvironmentBlocks` in `Flux_Environment.h`) or a ZenithFlux component struct (LIGHT's `Flux_LightComponents.h`, this file's `Flux_RenderComponents.h`).

<!-- assumption: pending MOD (RP26) -->
MOD's "ZenithRuntime files" table lists `Internal/Zenith_EngineExtractors.cpp` alone and its test table five files; the other rows above, the `Tests/EngineExtractors/` directory in `ZenithRuntime_Tests`, the call of `Zenith_RegisterRenderComponentTypes` in startup step 6 (the engine-types part, beside LIGHT's `Zenith_RegisterLightingComponentTypes`) and the call of `Zenith_AttachEngineExtractors` after the modules' `AttachWorld` calls are reconciliation row RP26. The extractor ticket adds them to MOD's tables in its own diff. Two MOD rules conflict with this file, recorded as reconciliation row RP32 and followed here until MOD conforms: (a) `Zenith2/Docs/Design/MOD.md → "Registration rejection rules"` rejects an `AddExtractor` whose `m_szName` does not start with `<MODULE_NAME>.`, and the runtime's own attach context has no module name, so MOD exempts the runtime's attach context and its four `Render.*` names from the prefix rule (they are still unique in the world); (b) `Zenith2/Docs/Design/MOD.md → "Frame-graph nodes registered by ZenithRuntime"` registers none of its nodes in the headless configuration, `Render.Seal` included, while [Z2-N-ARCH-439] orders every extractor before `Render.Seal` in every configuration and `Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"` assumes the seal is declared headless, so MOD registers `Render.Seal` (EPILOGUE, not part of the render subgraph) in every configuration and leaves only the render subgraph empty headless.
<!-- /assumption -->

## Flux_RenderWorld header additions

`Flux_RenderWorld.h` declares Rendering's `Flux_RenderWorldSection`, `Flux_RenderWorld`, `Flux_RenderWorldError`, `Flux_RenderWorldWriter` and `Flux_RenderWorldRing` exactly as `Zenith2/Docs/Rendering.md → "Purpose and scope"` gives them, RPROD's `Flux_RenderEnvironment` (`Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`) and the types below, beside `Flux_ViewRequest` (`Zenith2/Docs/Design/RPROD/Views.md → "Flux_View header"`, included). GEOM's, LIGHT's and ENV's element types are included from their headers and never re-declared.

### Instance keys

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderWorld.h (additions)
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithFlux/Flux_MeshResource.h"    // GEOM's Flux_MeshID
#include "ZenithFlux/Flux_MaterialResource.h"   // GEOM's Flux_MaterialID
#include "ZenithFlux/Flux_Lights.h"          // LIGHT's Flux_RenderLightDelta, Flux_RenderLightKey
#include "ZenithFlux/Flux_View.h"            // Flux_ViewRequest

/// @contract value Flux_RenderInstanceKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: None; a key names one render instance for the lifetime of its world.
/// Units: None; bit 63 clear: an entity key (Flux_MakeEntityInstanceKey); bit 63 set: a module key.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized, so a D-SEL-HASH (open, Stage 1) change moves every key at once and breaks
///   nothing.
/// @endcontract
struct Flux_RenderInstanceKey
{
	uint64 m_ulValue = 0;
	friend constexpr bool operator==(Flux_RenderInstanceKey, Flux_RenderInstanceKey) = default;
};

inline constexpr uint64 ulFLUX_RENDER_KEY_MODULE_BIT = 1ull << 63;

/// @contract functions Flux_RenderInstanceKeyFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
// Zenith_Hash64 of the 16 bytes of xEntity (little-endian, Zenith2/Docs/Design/CORE/Maths.md "Zenith_Hash64"), seed uWorldTag,
// with bit 63 cleared: the shape of LIGHT's Flux_MakeEntityLightKey.
Flux_RenderInstanceKey Flux_MakeEntityInstanceKey(const Zenith_PersistentID& xEntity, uint32 uWorldTag);
// A module's own key space (ENV's foliage keys, a module's generated instances): ulHash with bit 63 set.
constexpr Flux_RenderInstanceKey Flux_MakeModuleInstanceKey(uint64 ulHash)
{
	return Flux_RenderInstanceKey{ ulHash | ulFLUX_RENDER_KEY_MODULE_BIT };
}
```

| Key space | Producer | Rule |
|---|---|---|
| bit 63 clear | the engine instance extractor, for an entity with a renderable-mesh component | `Flux_MakeEntityInstanceKey(persistent ID, world tag)` |
| bit 63 set | a module's own instances (ENV's placed foliage `Zenith_FoliageRenderKey`, reconciliation row RP25) | `Flux_MakeModuleInstanceKey(the module's 64-bit hash)` |

The two spaces are disjoint by construction, so an entity key never collides with a module key. Two entity keys collide only on a `Zenith_Hash64` collision of two persistent IDs in one world, which the GPU scene's key map would treat as one instance; `Test_EngineExtractor_KeysDisjointAndStable` asserts the bit rule and the stability of a key across two sessions of one scene.

### Module render-world records added after Stage 2

<!-- elaboration -->
Two module families append sections and typed writer members to `Flux_RenderWorld.h` after Stage 2, each in the RPROD ticket that implements them; the element types stay their owners' and are declared in the owners' ZenithFlux headers, which `Flux_RenderWorld.h` includes ([Z2-N-REND-018]):

| Spans (after `m_axEnvironmentViews`, in this order) | Writer members | Element types | Rule |
|---|---|---|---|
| `m_axSprite2DLists`, `m_axTilemapChunks`, `m_axTilesetRecords` (Stage 3; Rendering since X295) | `AddSprite2DList`, `AddTilemapChunk`, `AddTilesetRecord` | TWOD's, `ZenithFlux/Flux_Sprite2D.h` | each member copies the record and every span it holds into the partition's slot arena and re-points the copy's spans there; a record or span that does not fit is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` and counted in `Flux.RenderWorld.Dropped` (TwB2) |
| `m_axVideoDraws`, `m_axScreenOverlays` (Stage 4; pending X292 (c)) | `AddVideoDraw(const Flux_VideoDrawRecord&)`, `AddScreenOverlay(const Flux_ScreenOverlayRecord&)` | CINE's, `ZenithFlux/Flux_Video.h` (`Zenith2/Docs/Design/CINE/Formats.md → "Render-world records"`) | the same copying and refusal rule; the two records hold no pointer into simulation memory (CnB4) |

The merge keeps them per world section in extractor registration order, as every other span.

### Instance flags

<!-- elaboration -->
```cpp
/// @contract enum Flux_InstanceFlags
/// Ownership: Plain enumeration; the bits of the GPU scene's instance record m_uFlags
///   (Zenith2/Docs/Rendering/GpuResources.md "Instance record (128 bytes)"), declared once here.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Flux_RenderWorldWriter::AddInstanceDelta clears every bit outside uFLUX_INSTANCE_DELTA_FLAG_MASK.
/// Version: since Z2 Stage 2. The values are the record's bit positions and never move.
/// @endcontract
enum Flux_InstanceFlags : uint32
{
	FLUX_INSTANCE_FLAG_CASTS_SHADOW = 1u << 0,
	FLUX_INSTANCE_FLAG_SKINNED = 1u << 1,
	FLUX_INSTANCE_FLAG_PROCEDURAL = 1u << 2,
	FLUX_INSTANCE_FLAG_HISTORY_RESET = 1u << 3,
	FLUX_INSTANCE_FLAG_HIDDEN_IN_MAIN = 1u << 4,
	FLUX_INSTANCE_FLAG_TRANSPARENT = 1u << 5,
};

inline constexpr uint32 uFLUX_INSTANCE_DELTA_FLAG_MASK = FLUX_INSTANCE_FLAG_CASTS_SHADOW | FLUX_INSTANCE_FLAG_PROCEDURAL |
	FLUX_INSTANCE_FLAG_HIDDEN_IN_MAIN;
```

| Bit | Record flag | Set by |
|---|---|---|
| 0 | `CASTS_SHADOW` | the producer of the delta (the engine instance extractor from `Flux_MeshInstanceComponent::m_bCastsShadows`) |
| 1 | `SKINNED` | the GPU scene when it applies the delta: set exactly when the resolved `Flux_MeshResource::IsSkinned()` (reconciliation row RP27) |
| 2 | `PROCEDURAL` | the producer of a procedural instance (GEOM's rule, row GM22) |
| 3 | `HISTORY_RESET` | the GPU scene, from the section's `m_axHistoryResets` ([Z2-N-REND-009]) |
| 4 | `HIDDEN_IN_MAIN` | the producer (the engine instance extractor from `m_bHiddenInMain`) |
| 5 | `TRANSPARENT` | the GPU scene when it applies the delta: set exactly when any effective material of the instance's material assignment (override or the mesh's default of the slot) has `FLUX_MATERIAL_DOMAIN_TRANSPARENT` (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`; row RP27) |
| 6 | `VIRTUAL_GEOMETRY` (Stage 7, `FLUX_INSTANCE_FLAG_VIRTUAL_GEOMETRY = 1u << 6`) | the GPU scene when it applies the delta: set exactly when the resolved mesh is a virtualized-geometry mesh (`Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md → "VG path integration"`, row AV1 (h)); the visibility-buffer buckets skip it (`Zenith2/Docs/Design/RPROD/Passes.md → "Virtual texture bindings in material passes"`) |
| 7–31 | reserved | nobody; cleared |

The derived bits come from the build node, where GEOM's managers have finalised the mesh and its materials; a simulation-side producer cannot know a mesh's default materials or whether a loading mesh is skinned, so it never sets them.

### Flux_RenderInstanceDelta layout

<!-- elaboration -->
```cpp
/// @contract enum Flux_InstanceDeltaOp
/// Ownership: Plain enumeration; the op byte of Flux_RenderInstanceDelta.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Flux_GPUScene::ApplyDeltas applies an ADD of a key it holds as an UPDATE and an UPDATE of an unknown key
///   as an ADD, and ignores a REMOVE of an unknown key (row RP27).
/// Version: since Z2 Stage 2. A render-world value, never serialized; the values never move.
/// @endcontract
enum Flux_InstanceDeltaOp : uint8 { FLUX_INSTANCE_DELTA_OP_ADD = 1, FLUX_INSTANCE_DELTA_OP_UPDATE, FLUX_INSTANCE_DELTA_OP_REMOVE };

inline constexpr uint32 uFLUX_INSTANCE_MAX_MATERIAL_OVERRIDES = 8;

/// @contract value Flux_RenderInstanceDelta
/// Ownership: Plain value in the render world's slot arena, written through Flux_RenderWorldWriter::AddInstanceDelta.
/// Thread affinity: IMMUTABLE after Render.Seal.
///   Members non-const: the extractor fills each record field by field in the render world before Render.Seal.
/// Invalidation: Valid for the render world that carries it.
/// Units: Metres (m_xLocalToWorld, m_xWorldBounds); m_uCustomData bits 16–31 in metres when the mesh is cooked with
///   FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE.
/// Errors: Applied by RHI's Flux_GPUScene::ApplyDeltas: an ADD of a key it already holds is applied as an UPDATE, an
///   UPDATE of an unknown key as an ADD, a REMOVE of an unknown key is ignored (row RP27); a null or stale mesh ID of a
///   non-procedural delta draws nothing and is counted by the GPU scene.
/// Version: since Z2 Stage 2. A render-world value, never serialized; Test_InstanceDelta_Layout pins the layout.
/// @endcontract
struct Flux_RenderInstanceDelta   // 184 bytes
{
	Flux_RenderInstanceKey m_xKey;
	Flux_InstanceDeltaOp m_eOp = FLUX_INSTANCE_DELTA_OP_ADD;
	uint8 m_uReserved0 = 0;
	uint16 m_uReserved1 = 0;
	uint32 m_uFlags = 0;                                  // Flux_InstanceFlags bits of uFLUX_INSTANCE_DELTA_FLAG_MASK
	Flux_MeshID m_xMesh;                                   // null with FLUX_INSTANCE_FLAG_PROCEDURAL (GEOM, row GM22)
	uint32 m_uCustomData = 0;                             // the instance record's m_uCustomData ("Instance extraction algorithm" step 6)
	uint32 m_uLayerMask = 0x01000001;                     // bits 0–23 render layers, 24–31 lighting channels
	Zenith_Matrix4 m_xLocalToWorld;                        // the interpolated render transform; bottom row (0, 0, 0, 1)
	Zenith_AABB m_xWorldBounds;                            // the mesh's local bounds through m_xLocalToWorld
	Flux_MaterialID m_axMaterialOverrides[uFLUX_INSTANCE_MAX_MATERIAL_OVERRIDES];   // per material slot; null = the mesh's default
};
static_assert(sizeof(Flux_RenderInstanceDelta) == 184);
static_assert(alignof(Flux_RenderInstanceDelta) == 8);
```

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 8 | `m_xKey` | the instance |
| 8 | 1 | `m_eOp` | ADD, UPDATE or REMOVE; a REMOVE reads nothing but the key |
| 9 | 3 | `m_uReserved0`, `m_uReserved1` | zero |
| 12 | 4 | `m_uFlags` | the producer bits of "Instance flags" |
| 16 | 8 | `m_xMesh` | GEOM's `Flux_MeshID` of the mesh (`Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"`) |
| 24 | 4 | `m_uCustomData` | per-instance data a material reads; for a mesh with a flagged primitive, bits 16–31 always carry the cull distance of `Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"` (65,535 never culls) |
| 28 | 4 | `m_uLayerMask` | the instance record's `m_uLayerMask`: render layers in bits 0–23, LIGHT's lighting channels in bits 24–31 (cross-doc item X103 (d)) |
| 32 | 64 | `m_xLocalToWorld` | `Zenith_Matrix4` (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`); the GPU scene writes its top three rows as the record's `m_xLocalToWorld` float3x4 and derives `m_fMaxScale` |
| 96 | 24 | `m_xWorldBounds` | `Zenith_AABB`; read by the CPU transparent collector (`Zenith2/Docs/Design/RPROD/Culling.md → "Transparent collection and emission"`) |
| 120 | 64 | `m_axMaterialOverrides` | `Flux_MaterialID` per material slot 0–7; slot 0 is a procedural instance's material (row GM22) |
| 184 | — | end | 184 is a multiple of the 8-byte alignment of `m_xKey`; no tail padding |

- Every UPDATE carries the instance's complete state (not a field diff), so the GPU scene re-derives the record from one delta and an UPDATE can replace a failed ADD.
- The previous transform is not a delta field: the GPU scene keeps it, copying the record's previous current transform into `m_xPrevLocalToWorld` for every updated instance and the current one for an added instance or a history reset (`Zenith2/Docs/Design/RHI/Memory.md → "Scatter staging and coalescing"` step 4). The extractor's settle update ("Delta tracking") is what makes a stopped instance's previous transform catch up.
- The transform is a `Zenith_Matrix4`, not SCENE's `Zenith_Transform`: ZenithFlux includes no ZenithScene header ([Z2-N-REND-005]), and a world transform with shear has no position-rotation-scale form (`Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"`).
- Material slots at and above 8 always take the mesh's default material. GEOM's "render-world input" row reads "up to S overrides"; an inline array of `uGEOM_MAX_MATERIAL_SLOTS` (255) entries would make every delta 2,160 bytes and a W-Outdoor full-state world of 100,000 instances 216 MB, so the delta carries eight and the component authors eight (reconciliation row RP28 asks GEOM's row to name the cap).

`Test_InstanceDelta_Layout` asserts the size, the alignment and every offset with `offsetof`.

### Procedural instance records

GEOM's procedural geometry (row GM22) adds one span and one writer member to Rendering's declarations, which `Zenith2/Docs/Rendering.md → "Purpose and scope"` records since X141 and this header carries:

```cpp
// Flux_RenderWorldSection gains, after m_axHistoryResets:
	Zenith_Span<const Flux_RenderProceduralInstance> m_axProceduralInstances;   // GEOM's element type (Flux_ProceduralGeometry.h)
// Flux_RenderWorldWriter gains:
	Zenith_Result<void, Flux_RenderWorldError> AddProceduralInstance(const Flux_RenderProceduralInstance& xRecord);
```

A procedural instance is two records: one `Flux_RenderProceduralInstance` (its generator, topology and frame parameters) and one `Flux_RenderInstanceDelta` with `FLUX_INSTANCE_FLAG_PROCEDURAL`, a null `m_xMesh`, `m_axMaterialOverrides[0]` its material and the same key. The consumer is GEOM's `Flux_ProceduralApplyRecords`, which `Flux_Renderer::BuildGraph` calls before the GPU scene applies the deltas (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux.BuildGraph"`). No engine extractor writes a procedural record; a module's procedural source does.

## Engine render components

Q48 was ruled (a) by the orchestrator: RPROD declares the engine's renderable-mesh and camera components, with the extractors that read them, through the mechanism Q42 ruled for engine component types (C++-registered structs, `Zenith2/Docs/Design/LIGHT.md → "Flux_LightComponents header"` is the other instance of it).

### Flux_RenderComponents header

Engine component types are C++-registered structs (Q42, ruled by the orchestrator; the core-doc sentence is X108, applied in batch 4). The structs are declared in ZenithFlux with no reflection dependency and registered by ZenithRuntime ("Render component registration"). If the human overrides Q42 in favour of engine-content schemas, the same stable names, fields and property IDs become `*.schema.json` files and this header becomes their generated view.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderComponents.h
#pragma once
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_MeshResource.h"       // GEOM's loaded types: complete, because Zenith_AssetHandle checks
#include "ZenithFlux/Flux_MaterialResource.h"   // Zenith_AssetType<T> in its class body
#include "ZenithFlux/Flux_TextureResource.h"

/// @contract value Flux_MeshInstanceComponent
/// Ownership: Plain component value stored by the ECS backend through the reflected type Zenith.Render.MeshInstance; its
///   asset handles each hold one reference, copied and destroyed through the type's lifecycle hooks ([Z2-N-ARCH-645]).
/// Thread affinity: As every component: written by the phases its authority allows (PHASE_LATE_SCRIPTS), read by
///   Render.ExtractInstances in phase 12.
/// Invalidation: None.
/// Units: m_uCullDistanceMetres in metres.
/// Errors: None; the extractor skips a null or unresolvable mesh ("Instance extractor").
/// Version: since Z2 Stage 2; type version 1. A layout change never changes a serialized form ([Z2-N-ARCH-644]).
/// @endcontract
struct Flux_MeshInstanceComponent
{
	Zenith_AssetHandle<Flux_MeshResource> m_xMesh;                   // 1  Mesh
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride0;  // 2  MaterialOverride0 (null: the mesh's default)
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride1;  // 3  MaterialOverride1
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride2;  // 4  MaterialOverride2
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride3;  // 5  MaterialOverride3
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride4;  // 6  MaterialOverride4
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride5;  // 7  MaterialOverride5
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride6;  // 8  MaterialOverride6
	Zenith_AssetHandle<Flux_MaterialResource> m_xMaterialOverride7;  // 9  MaterialOverride7
	uint32 m_uCustomData = 0;                                       // 10 CustomData
	uint16 m_uCullDistanceMetres = 65535;                           // 11 CullDistanceMetres, 65,535 = no limit
	uint32 m_uLayerMask = 0x01000001;                               // 12 LayerMask: layer 0, lighting channel 0
	bool m_bCastsShadows = true;                                     // 13 CastsShadows
	bool m_bHiddenInMain = false;                                    // 14 HiddenInMain: drawn in every view but the main view
	bool m_bVisible = true;                                          // 15 Visible: false removes the instance from the render world
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract enum Flux_CameraKind
/// Ownership: Plain enumeration; the values of Flux_CameraComponent::m_uKind (property 11).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a camera whose m_uKind holds any other value is neither the main camera nor a texture camera, and the camera
///   extractor counts it in Render.Extract.Cameras.Rejected.
/// Version: since Z2 Stage 2 (MAIN); TEXTURE since Z2 Stage 5. Serialized as property 11's uint8, so the values never move.
/// @endcontract
enum Flux_CameraKind : uint8 { FLUX_CAMERA_KIND_MAIN = 0, FLUX_CAMERA_KIND_TEXTURE = 1 };

/// @contract value Flux_CameraComponent
/// Ownership: Plain component value stored through the reflected type Zenith.Render.Camera; m_xOutputTexture holds one
///   reference.
/// Thread affinity: As every component; read by Render.ExtractCameras in phase 12.
/// Invalidation: None.
/// Units: Degrees for m_fFovYDegrees (radians after extraction); metres for the planes and m_fOrthoHeightMetres; frames for
///   m_uUpdateIntervalFrames; pixels for the output size.
/// Errors: None; the extractor refuses an invalid lens ("Camera extractor").
/// Version: since Z2 Stage 2 (properties 1–10); properties 11–15 since Z2 Stage 5; type version 1.
/// @endcontract
struct Flux_CameraComponent
{
	bool m_bActive = true;                                    // 1  Active
	sint32 m_iPriority = 0;                                    // 2  Priority: the main camera is the active one with the highest
	uint8 m_uProjection = 1;                                 // 3  Projection: Flux_ProjectionKind (1 perspective, 2 orthographic)
	float m_fFovYDegrees = 60.0f;                             // 4  FovYDegrees, (0, 179]
	float m_fOrthoHeightMetres = 10.0f;                       // 5  OrthoHeightMetres, > 0
	float m_fNearMetres = 0.1f;                               // 6  NearMetres, > 0
	float m_fFarMetres = 2000.0f;                             // 7  FarMetres, > near; 0 = infinite (perspective only)
	uint32 m_uFeatureMaskSet = 0;                            // 8  FeatureMaskSet: Flux_ViewFeature bits enabled beyond the kind
	uint32 m_uFeatureMaskClear = 0;                          // 9  FeatureMaskClear
	uint32 m_uCutRequest = 0;                                // 10 CutRequest: a change of value is a camera cut
	uint8 m_uKind = FLUX_CAMERA_KIND_MAIN;                   // 11 Kind (Stage 5 for TEXTURE)
	Zenith_AssetHandle<Flux_TextureResource> m_xOutputTexture;   // 12 OutputTexture (TEXTURE kind; pending GEOM, RP28)
	uint32 m_uOutputWidth = 0;                               // 13 OutputWidth (TEXTURE kind)
	uint32 m_uOutputHeight = 0;                              // 14 OutputHeight (TEXTURE kind)
	uint32 m_uUpdateIntervalFrames = 1;                      // 15 UpdateIntervalFrames (TEXTURE kind)
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

// Property-ID constants: uPROP_MESH_INSTANCE_MESH = 1 … uPROP_MESH_INSTANCE_VISIBLE = 15,
// uPROP_CAMERA_ACTIVE = 1 … uPROP_CAMERA_UPDATE_INTERVAL_FRAMES = 15, one per row of "Render component registration".

inline constexpr uint64 ulFLUX_NO_MAIN_CAMERA = ~0ull;

/// @contract functions Flux_MainCameraSelection
/// Ownership: Pure function; borrows both spans for the call.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None; ZENITH_ASSERT(axCameras.GetCount() == aulSimulationIDs.GetCount()); ulFLUX_NO_MAIN_CAMERA when no element
///   qualifies.
/// Version: since Z2 Stage 2.
/// @endcontract
// The index of the world's main camera: among the elements with m_bActive true and m_uKind FLUX_CAMERA_KIND_MAIN, the one
// with the highest m_iPriority, ties to the lowest simulation ID. The result names the same camera for the elements in any
// order. Render.ExtractCameras and ENV's submersion test both call it (row RP34), so no second copy of the rule exists.
uint64 Flux_SelectMainCamera(Zenith_Span<const Flux_CameraComponent> axCameras,
	Zenith_Span<const uint64> aulSimulationIDs);
```

| Stable name | Struct | Property IDs and names | Extracted into |
|---|---|---|---|
| `Zenith.Render.MeshInstance` | `Flux_MeshInstanceComponent` | 1 Mesh (asset `Flux.Mesh`), 2–9 MaterialOverride0–7 (asset `Flux.Material`), 10 CustomData, 11 CullDistanceMetres, 12 LayerMask, 13 CastsShadows, 14 HiddenInMain, 15 Visible | `Flux_RenderInstanceDelta` ("Instance extractor") |
| `Zenith.Render.Camera` | `Flux_CameraComponent` | 1 Active, 2 Priority, 3 Projection, 4 FovYDegrees, 5 OrthoHeightMetres, 6 NearMetres, 7 FarMetres, 8 FeatureMaskSet, 9 FeatureMaskClear, 10 CutRequest, 11 Kind, 12 OutputTexture (asset `Flux.Texture`), 13 OutputWidth, 14 OutputHeight, 15 UpdateIntervalFrames | `Flux_ViewRequest` ("Camera extractor"), with LIGHT's `Zenith.Lighting.CameraLighting` on the same entity |

<!-- assumption: pending GEOM (RP28) -->
A `FLUX_CAMERA_KIND_TEXTURE` camera renders into `m_xOutputTexture`, which must be a render-target texture: GEOM's `Flux_TextureResource` owns a persistent read-only texture (`Zenith2/Docs/Design/GEOM/Textures.md`), so a render-target texture asset kind with a writable `Flux_TextureHandle` is reconciliation row RP28. Until it lands, the extractor writes no request for a `TEXTURE` camera and counts it in `Render.Extract.Cameras.Rejected`; the Stage 5 advanced multi-view ticket is blocked by RP28 for gameplay texture cameras alone (the other texture views are requested by their areas' extractors).
<!-- /assumption -->

### Render component registration

`Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterRenderComponentTypes(Zenith_TypeRegistry& xTypes)` (`ZenithRuntime/Internal/Zenith_RenderTypes.h`) registers each struct with `Zenith_TypeRegistry::BeginComponent<T>(szStableName, 1)`, `DefaultAuthority(PHASE_LATE_SCRIPTS)`, one `Property(uPROP_…, szName, &T::m_…)` per row of the table above with `Flags(PROPERTY_FLAG_SERIALIZED)` and the `Range` and `Default` of the struct's comments, the lifecycle hooks of the asset-handle members (`Zenith_MakeLifecycleHooks<T>()`), then `End()` (`Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`, `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`). The runtime calls it in startup step 6's engine-types part, after `Zenith_RegisterSceneTypes` and beside LIGHT's `Zenith_RegisterLightingComponentTypes` (row RP26). The type IDs are `Zenith_MakeTypeID("Zenith.Render.MeshInstance")` and `Zenith_MakeTypeID("Zenith.Render.Camera")`; property IDs are never reused ([Z2-N-ARCH-641]). The scaffold's template scenes name the camera by `Zenith.Render.Camera` and a visible mesh by `Zenith.Render.MeshInstance` (row RP26, answering MOD row M18 for cameras).

## Extractor registration

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_EngineExtractors.h
struct Zenith_EngineExtractorDeps
{
	const Zenith_World* m_pxWorld = nullptr;                    // entity table: persistent IDs (ECS WorldAPI.md "Zenith_World header")
	const Zenith_WorldSimulation* m_pxSimulation = nullptr;     // GetChangeVersion
	const Zenith_SimulationClock* m_pxClock = nullptr;          // GetInterpolationAlpha (JOBS PublicAPI.md "Zenith_SimulationClock header")
	const Zenith_ExtractionResolver* m_pxResolver = nullptr;    // mesh and material resolution ("Extraction resolver")
	const Zenith_TypeRegistry* m_pxTypes = nullptr;             // the ENV resource type lookup
	Zenith_Heap* m_pxHeap = nullptr;
};

// MAIN_THREAD, inside the runtime's attach step of one world, after every module's AttachWorld.
Zenith_Result<Zenith_Owned<Zenith_EngineExtraction>, Zenith_ModuleError> Zenith_AttachEngineExtractors(
	Zenith_WorldAttachContext& xContext, const Zenith_EngineExtractorDeps& xDeps);
```

`Zenith_AttachEngineExtractors` creates the world's `Zenith_EngineExtraction` ("Engine extraction state", `MODULE_ERROR_OUT_OF_MEMORY` when its `Zenith_TryNew` fails), registers it as a lifecycle listener with `xContext.AddLifecycleListener`, and registers the four extractors in this order through `Zenith_WorldAttachContext::AddExtractor` (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`, [Z2-N-ARCH-419]), each with a `Zenith_ExtractorDesc` (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"`) whose `m_pUserData` is the extraction state:

| Order | `m_szName` | `m_pfnExtract` | `m_uCostHint` (µs) |
|---|---|---|---|
| 1 | `Render.ExtractInstances` | `Zenith_ExtractInstances` | 400 |
| 2 | `Render.ExtractLights` | `Zenith_ExtractLights` | 50 |
| 3 | `Render.ExtractCameras` | `Zenith_ExtractCameras` | 5 |
| 4 | `Render.ExtractEnvironment` | `Zenith_ExtractEnvironment` | 5 |

Each call returns the extractor's `Zenith_SystemBuilder&`, on which the attach step declares the reads and queries of the extractor's "declared reads" table and nothing else: no write, no command, no `Before` or `After`. An extractor declares no ordering of its own because `Zenith_WorldSimulation::EmitInto` supplies `Before("Render.Seal@W0", true)` on every extractor registered through `AddExtractor` (`m_bRenderExtractor = true`, `Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"`, [Z2-N-ARCH-439]; Q40 ruled (a)), and `Render.Seal` declares none either (`Zenith2/Docs/Design/MOD.md → "Frame loop driver"`). The four extractors have no ordering among themselves: each writes its own writer partition ([Z2-N-ARCH-419]), the partitions merge in registration order, and the phase boundary orders every phase-1 to phase-11 write before them ([Z2-N-ARCH-472]).

The engine extractors are registered for every world and in every configuration, headless included: every render path runs headless, and in a configuration with no render subgraph the sealed world is simply never consumed. What differs headless is resolution, not the algorithm: ASSET serves every GPU-resident kind headless with its validate-only loader, so the production resolver answers every mesh `RESOLVE_OUTCOME_NO_RUNTIME_OBJECT`, every renderable entity is held once and not retried while the availability epoch stays ("Extraction resolver"), and a headless run's extraction cost after the first frame is the chunk test over changed chunks plus the rebuild of changed rows. The extractor tests run headless with the fixture resolver instead, which resolves meshes and materials to fixture IDs, so they observe every ADD, UPDATE and REMOVE a windowed run writes ("Extractor tests"). The extraction state outlives the world's schedule: the runtime keeps the `Zenith_Owned` until the world is detached and destroys it after the executor that runs its nodes ([Z2-N-CONTRACT-340]'s reverse order).

### Extraction resolver

The instance extractor resolves assets through one internal interface, never through the asset registry directly, so a test can supply resolved IDs and a headless run can wait instead of polling:

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_ExtractionResolver.h
struct Zenith_ResolvedMesh
{
	Flux_MeshID m_xMesh;                       // GEOM's ID of the resident mesh
	Zenith_AABB m_xLocalBounds;                // Flux_MeshResource::GetBounds()
	bool m_bCustomCullDistance = false;        // a primitive of the mesh carries FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE
};

enum Zenith_ResolveOutcome : uint8
{
	RESOLVE_OUTCOME_RESOLVED = 0,
	RESOLVE_OUTCOME_UNRESOLVED,                // any Resolve error but NO_RUNTIME_OBJECT: loading, failed, stale, null
	RESOLVE_OUTCOME_NO_RUNTIME_OBJECT,         // ACTIVE with a null object: ASSET's validate-only loader (headless)
};

// Every member is const and ANY_THREAD: the chunk-pass leaves call it concurrently.
class Zenith_ExtractionResolver
{
public:
	virtual ~Zenith_ExtractionResolver() = default;
	virtual Zenith_ResolveOutcome ResolveMesh(const Zenith_AssetHandle<Flux_MeshResource>& xMesh,
		Zenith_ResolvedMesh& xOut) const = 0;
	virtual Zenith_ResolveOutcome ResolveMaterial(const Zenith_AssetHandle<Flux_MaterialResource>& xMaterial,
		Flux_MaterialID& xOut) const = 0;
	// Never moves backwards; it moves (after the object is published) whenever an asset may have become resolvable, so an
	// unchanged value means no asset hold can have become resolvable since the value was last read.
	virtual uint64 GetAvailabilityEpoch() const = 0;
};
```

| Implementation | Where | Behaviour |
|---|---|---|
| `Zenith_AssetExtractionResolver` | `Internal/Extraction/Zenith_AssetExtractionResolver.h/.cpp`, constructed by the runtime over its `const Zenith_AssetRegistry&` and its headless flag, owned by the runtime and destroyed after every world is detached | `ResolveMesh` calls `Zenith_AssetRegistry::Resolve` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"`): success gives `RESOLVED` with `pxMesh->GetID()`, `pxMesh->GetBounds()` and `m_bCustomCullDistance` from GEOM's mesh (pending GEOM, row RP36; false until then, so the cull-distance packing is inert as `Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"` says); `ASSET_ERROR_NO_RUNTIME_OBJECT` gives `NO_RUNTIME_OBJECT` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Errors"`); every other error gives `UNRESOLVED`. `ResolveMaterial` is the same with `pxMaterial->GetID()`. `GetAvailabilityEpoch` is the registry's availability epoch (below) |
| `Zenith_FixtureExtractionResolver` | `Tests/EngineExtractors/Zenith_FixtureExtractionResolver.h` | a table from asset GUID to an outcome and the fixture IDs and bounds the test sets, a counter of calls per GUID, and an epoch the test bumps; it never touches a registry |

<!-- assumption: pending ASSET (RP33) -->
ASSET declares no availability notification. A GPU-resident asset becomes resolvable only when an activation publishes a non-null object: event E4 of `Zenith2/Docs/Design/ASSET/Loading.md → "Asset state machine"`, step 4 of `Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"`, and a reload swap (`Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"`). Reconciliation row RP33 asks ASSET for `uint64 Zenith_AssetRegistry::GetAvailabilityEpoch() const` (ANY_THREAD, an acquire load of a registry-wide counter incremented with a release store after each such publication), which a headless registry never moves because its validate-only loader activates null objects. Until RP33 lands, `Zenith_AssetExtractionResolver::GetAvailabilityEpoch` returns 0 in a headless runtime, which is exact because no GPU-resident asset ever resolves there, and the frame index in a windowed one, which retries asset holds every frame as a correct but slower interim.
<!-- /assumption -->

## Engine extraction state

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_EngineExtraction.h
class Zenith_EngineExtraction final : public Zenith_LifecycleListener
{
public:
	uint32 GetInterestMask() const override;   // TELEPORTED and DESTROYED
	void OnLifecycleEvent(const Zenith_LifecycleEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld,
		Zenith_CommandBuffer& xFollowUp) override;

	Zenith_EngineExtractorDeps m_xDeps;
	uint16 m_uWorldTag = 0;
	Zenith_DeltaTracker m_xInstances;                  // Render.ExtractInstances' tracker
	Zenith_DeltaTracker m_axLights[5];                 // one per light type, in FLUX_LIGHT_TYPE order
	Zenith_HashSet<uint64> m_xCameraTeleports;        // simulation IDs teleported since the camera extractor last ran
	uint64 m_ulMainCameraSimulationID = 0;            // the main camera of the previous frame; 0 = none
	uint32 m_uMainCameraCutRequest = 0;               // its CutRequest value extracted in the previous frame
	Zenith_QuerySlot m_axSlots[8];                     // the declared queries (instances, five light types, cameras, world lighting)
};
```

| Member | Written by | Read by |
|---|---|---|
| each tracker's teleport and destroy sets | `OnLifecycleEvent`, on the world's sync-point node during ticks | that tracker's extractor, in phase 12 of the same frame |
| each tracker's key map, held set, moving flags, change reference | its extractor alone, in phase 12 | its extractor alone |
| `m_xCameraTeleports`, the main-camera members | `OnLifecycleEvent` (teleports) and `Render.ExtractCameras` | `Render.ExtractCameras` |

The listener and the extractors never run at once: the listener runs on sync-point nodes inside TICK subgraphs, the extractors in EPILOGUE stage 12 after every tick of the frame, and the iteration barrier separates frames ([Z2-N-ARCH-385]). Within phase 12 each extractor touches only its own members, so the four extractors running concurrently share no mutable state. The listener's thread affinity is `Zenith_LifecycleListener`'s (`EXTERNALLY_SYNCHRONISED`, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleListener header"`).

`OnLifecycleEvent` resolves the event's entity through `xWorld.GetEntityTable()` (`Resolve`, `GetSimulationID`; a `DESTROYED` event arrives before the slot is released, `Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"`) and:

| Event | Action |
|---|---|
| `LIFECYCLE_EVENT_TELEPORTED` | insert the simulation ID into each tracker's teleport set and into `m_xCameraTeleports`; SCENE dispatches one event per teleported descendant (`Zenith2/Docs/Design/SCENE/Transforms.md → "Teleport"` step 5), so an ancestor's teleport reaches every descendant camera and instance |
| `LIFECYCLE_EVENT_DESTROYED` | for each tracker holding the simulation ID in its key map: move its key into the tracker's pending-remove list and erase the entry; erase it from every held set and teleport set |

Every other event kind is outside the interest mask. The listener allocates only from the trackers' own heap-backed containers and `m_xCameraTeleports`, each reserved at attach to the world's entity capacity, so it never allocates on the sync-point node in steady state; a growth failure is logged (`render.extract`) and counted, and the affected entity is reconciled by the next sweep.

## Delta tracking

The instance extractor and the light extractor share one algorithm, `Zenith_DeltaTracker`, over one declared query each. It produces exact ADD, UPDATE and REMOVE deltas from the ECS's change versions, for commits made in a tick and out of tick alike, keeps interpolated transforms moving, and never scans unchanged chunks in a frame that ran a tick.

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Extraction/Zenith_DeltaTracker.h
struct Zenith_TrackedEntry
{
	uint64 m_ulKey = 0;                     // the Flux render key of the entity (instance or light)
	Zenith_EntityHandle m_xEntity;
	uint64 m_ulEmittedHash = 0;             // Zenith_Hash64 of the last emitted delta's bytes after the op byte
	uint64 m_ulEmittedTransformHash = 0;    // Zenith_Hash64 of the last emitted delta's transform bytes (below)
	uint8 m_uFlags = 0;                     // bit 0 MOVING, bit 1 RETRY_UPDATE
};   // 40 bytes

enum Zenith_HeldReason : uint8
{
	HELD_REASON_UNRESOLVED = 1,
	HELD_REASON_NO_RUNTIME_OBJECT,
	HELD_REASON_WRITE_FAILED,
	HELD_REASON_NOT_DRAWN,
};

struct Zenith_HeldEntry
{
	Zenith_EntityHandle m_xEntity;           // the entity a retry resolves (GetChunkOfEntity)
	Zenith_HeldReason m_eReason = HELD_REASON_UNRESOLVED;
};   // 16 bytes

class Zenith_DeltaTracker
{
public:
	Zenith_HashMap<uint64, Zenith_TrackedEntry> m_xTracked;   // simulation ID → entry: emitted and live in the GPU scene
	Zenith_HashMap<uint64, Zenith_HeldEntry> m_xHeld;         // simulation ID → entry: in the query, not emitted
	Zenith_Array<uint64> m_aulPendingRemoves;                 // keys whose REMOVE is still to be written
	Zenith_Array<uint64> m_aulPendingHistoryResets;           // keys whose history reset is still to be written
	Zenith_HashSet<uint64> m_xTeleported;                     // simulation IDs teleported since the last run
	uint64 m_aulWriteStart[3] = {};                           // per write list, the key or ID a run starts from ("Slot-full handling")
	uint64 m_ulAvailabilityEpoch = 0;                         // the resolver's epoch the last run read
	uint32 m_uChangeReference = 0;                            // GetChangeVersion() at the end of the last run; 0 = never ran
};
```

The transform bytes of a delta are the range the build function names: bytes 32 to 95 of a `Flux_RenderInstanceDelta` (`m_xLocalToWorld`), and `m_xPositionWS`, `m_xDirectionWS` and `m_xTangentWS` of LIGHT's `Flux_RenderLightDelta`.

| Held reason | Meaning | Retried |
|---|---|---|
| `HELD_REASON_UNRESOLVED` | the resolver answered `RESOLVE_OUTCOME_UNRESOLVED` for an asset the delta needs (loading, failed or stale) | only in a run whose availability epoch differs from `m_ulAvailabilityEpoch` ("Extraction resolver") |
| `HELD_REASON_NO_RUNTIME_OBJECT` | the resolver answered `RESOLVE_OUTCOME_NO_RUNTIME_OBJECT` (ASSET's validate-only loader, headless) | as `UNRESOLVED`: never in a headless run, whose epoch does not move |
| `HELD_REASON_WRITE_FAILED` | its ADD was refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` | every frame |
| `HELD_REASON_NOT_DRAWN` | authored not to draw (a null mesh, `m_bVisible` false; a light type's own rule) | no: a property change brings it back through the change test |

A property change of a held entity's own components is seen by the change test whatever its reason, so a new mesh handle is resolved in the frame it is committed.

Per run, with `R` the change reference, `V = m_pxSimulation->GetChangeVersion()` and `E = m_pxResolver->GetAvailabilityEpoch()` read at the start of the run:

1. **Reference.** The run is a full walk (step 5 with every row changed, and "Full-state extraction" when the writer asks for it) when R == 0; when `xContext.GetPreviousReadVersion() == 0`, the ECS reset signal (the extractor's first invocation, and its first invocation after `RestoreSimulationState` or after the version wrap sweep reset it: `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"`, whose system contract requires a system that keeps a last-seen version of its own to discard it then); when `Zenith_IsVersionNewer(R, V)` (the versions went backwards, a second guard for a restore); or when the writer's `IsFullExtraction()` is true. Otherwise a row is changed when, for any declared property p of the tracker's columns, its version v = `GetRowChangeVersions(type, p)[row]` is at or newer than R: `v == R || Zenith_IsVersionNewer(v, R)`.
2. **Chunk pass.** `n = xContext.GetChunkCount(slot)` over the tracker's unfiltered query; `xContext.Jobs().ParallelFor(n, 1, leaf)` (`Zenith2/Docs/Design/ECS/Queries.md → "Chunk iteration contract"`). Each leaf, with its own `ForLeaf` context, takes `GetChunk(slot, i)`, adds its row count to a per-leaf counter, skips the chunk when no declared column's `GetChangeVersion(type, p)` is at or newer than R (the column test of a `QUERY_TERM_CHANGED` term with the equality added, applied to the union of the tracker's components), and otherwise, for each changed row: looks the simulation ID up in `m_xTracked` and `m_xHeld` (read only; the leaves never write the maps), builds the row's delta (the extractor's build function), and appends a candidate `{simulation ID, handle, op, delta}` to the leaf's list in its node arena; a build that yields "not drawn" appends a candidate `REMOVE` when tracked plus a hold with `HELD_REASON_NOT_DRAWN`, and an unresolvable asset appends `REMOVE` when tracked plus a hold with the outcome's reason.
3. **Merge.** The continuation concatenates the leaves' lists, sorts the candidates by simulation ID with `Zenith_Sort` (stable), and applies them in that order. A candidate whose simulation ID is tracked or held under another entity handle (an ID reissued after a restore) first retires that entry: a tracked key joins the pending-remove list, a held entry is dropped. Then a hold candidate inserts or updates the row's held entry (its handle and reason); an untracked row whose build yielded a delta is an ADD: `m_xTracked` gains the entry with its key, a held entry of the row is erased, and MOVING is set only when the row's previous and current world transforms differ. A row neither tracked nor held at the start of the run is a new row of the query, which the light extractor's sweep rule reads. A tracked row whose delta hash equals `m_ulEmittedHash` and whose MOVING flag is clear emits nothing; any other tracked row is an UPDATE: MOVING is set when the delta's transform hash differs from `m_ulEmittedTransformHash`, and when it equals it while MOVING is set the UPDATE is the settle update and MOVING is cleared. Every emitted ADD or UPDATE stores both hashes of the emitted delta.
4. **Retries.** In ascending simulation ID, every entry step 3 did not handle that is a held entry with `HELD_REASON_WRITE_FAILED`, a held entry with `HELD_REASON_UNRESOLVED` or `HELD_REASON_NO_RUNTIME_OBJECT` when E differs from `m_ulAvailabilityEpoch`, or a tracked entry with RETRY_UPDATE: `xContext.GetChunkOfEntity(slot, entry.m_xEntity, row)` (`Zenith2/Docs/Design/ECS/Queries.md → "Resolve and random access"`). A held entry whose call fails with `ECS_ERROR_STALE_HANDLE` or `ECS_ERROR_NO_COMPONENT` (destroyed, or no longer matching the query) is dropped. A tracked entry whose call fails with `ECS_ERROR_NO_COMPONENT` keeps its entry for step 5, whose counts then differ and whose sweep writes its REMOVE; a tracked entry cannot see `ECS_ERROR_STALE_HANDLE`, because the listener erased every destroyed entity's entry before the run (`ZENITH_CHECK` in development builds, handled as `NO_COMPONENT` in Shipping). A found row is built and applied as in step 3.
5. **Removal reconciliation.** `rows` = the sum of the leaves' row counts; `expected` = `|m_xTracked| + |m_xHeld|` after steps 3 and 4. The listener has already moved every destroyed entity's key to the pending-remove list, and an entity can enter a query whose terms are all `WITH` only by gaining a column, which stamps it, so for such a query `rows == expected` exactly when no entity left the query any other way. When they differ (a live entity lost the component), the run walks every row of every chunk, marks each simulation ID seen, moves the key of every tracked entry not seen to the pending-remove list, drops every held entry not seen, and treats every seen untracked, unheld row as an ADD candidate of step 3; `Render.Extract.Sweeps` counts the walk. A full walk (step 1) always performs this reconciliation. The light extractor's queries carry `WITHOUT` terms, through which an entity can enter a query unstamped, so it applies the stronger condition of "Light extractor declared reads".
6. **Moving entries.** Every tracked entry with MOVING set that steps 3 and 4 did not emit, in ascending simulation ID: resolve it through `GetChunkOfEntity` (step 5 has already removed every entry that left the query), rebuild its delta with this frame's interpolation factor and emit an UPDATE; when the rebuilt delta's transform hash equals the entry's `m_ulEmittedTransformHash` (the transform has stopped changing), the UPDATE is the settle update that makes the GPU scene's previous transform equal the current one, and MOVING is cleared after it.
7. **Write.** Into the writer, in this order, each list from its start in `m_aulWriteStart` ("Slot-full handling"): every pending-remove key as a REMOVE delta in ascending key order; the ADD and UPDATE deltas of steps 3, 4 and 6 in ascending simulation ID; every key of `m_aulPendingHistoryResets` (appended by "Instance extractor" step 4 this run, or kept from a refused run) in ascending key order, each accepted key erased from the list and a key whose REMOVE this run writes dropped from it unwritten. The first write that returns `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` ends the writing for the run: every remaining record is kept by its row of "Slot-full handling" without a further call.
8. **Close.** `m_uChangeReference = V'`, where `V'` is `GetChangeVersion()` read at the end of the run (equal to V: no sync point runs during phase 12); `m_ulAvailabilityEpoch = E`; clear the teleport set.

| Property | Value |
|---|---|
| exactness | every change of a declared property committed after the previous run is seen, in a tick or out of tick. An in-tick commit of a subsequent tick carries a version newer than R (versions are precomputed per merge point and grow across ticks, `Zenith2/Docs/Design/ECS/Queries.md → "Change versions and merge versions"`). An out-of-tick write (`Zenith_WorldSimulation::ApplyOutOfTick`: editor edits, world and save loads, carry-over deserialisation, and SCENE's `Zenith_PropagateTransformsOutOfTick` after every activation and capture restore, `Zenith2/Docs/Design/SCENE/Transforms.md → "Out-of-tick propagation"`) is stamped `GetChangeVersion()` (`Zenith2/Docs/Design/ECS/Scheduling.md → "Out-of-tick playback"`), which out of tick is V(t, M_t) of the current tick index: equal to R when the previous run followed a completed tick t, newer otherwise, and at or newer than R either way. A spawned or added row is stamped by the playback that creates it ([Z2-N-ARCH-728]) and a structural move stamps the columns new to the row (`Zenith2/Docs/Design/ECS/Backends.md → "Change tables"`), so both are seen. `RestoreSimulationState` re-stamps every version to V(t, M) and resets every system's previous read version to 0, so even a restore to the current tick index, where R equals V(t, M) and the backwards test sees nothing, forces a full walk. An unchanged tracked entity emits nothing unless MOVING |
| what the equality over-reports | no in-tick commit stamps V(t, M), the version R holds: stage 11's end-of-tick playback plays no command (phase 11 records none, `Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"`), and its compaction relocates rows with their versions, raising a column version only to a moved row's own (the relocation rule of `Zenith2/Docs/Design/ECS/Backends.md → "Change tables"`). The rows at exactly R are therefore rows written out of tick after the previous run's tick. In consecutive frames with no tick (a paused world, the editor) R stays, so those rows are rebuilt every such frame and suppressed by the emitted-state hash; the first frame with a tick moves R past them |
| why not the query slot's own filter or the previous read version alone | a `QUERY_TERM_CHANGED` term tests one component type and the tracker needs the union of three; and a stage-12 reader's read point is the end-of-tick sync point, whose version V(t, M) is the version out-of-tick writes are stamped with, so a strictly-newer filter keyed on the read version misses them (reconciliation row RP31 asks ECS to close that gap for its own `CHANGED` terms). The tracker therefore keeps R itself, which the system contract of `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"` permits only with its reset rule: `GetPreviousReadVersion() == 0` discards R (step 1) |
| cost | O(matched chunks × declared columns) per frame for the chunk test, O(changed rows) for building, O(k log k) for the k candidates, O(moving entries) for step 6, O(held entries) for step 4 in a frame whose epoch moved; a sweep is O(rows) and runs only in a frame where a live entity left the query; a frame with no tick also rebuilds the rows written out of tick since the last tick |
| memory | 40 bytes per tracked entry, 16 per held entry, 8 per pending remove and pending history reset, all in heap-backed containers of the extraction state under `MEMORY_SUBSYSTEM_RENDERING` |

The emitted-state hash is `Zenith_Hash64` of the delta's bytes from offset 9 to the end (everything but the key and the op); comparing hashes rather than storing the 184-byte delta per entry costs one missed settle update on a 64-bit collision, which shows as one frame of motion-vector smear on that instance.

## Instance extractor

`Render.ExtractInstances` writes one `Flux_RenderInstanceDelta` per changed renderable entity, one REMOVE per entity that stopped being renderable and one history reset per teleported renderable entity.

### Instance extractor declared reads

| Declaration | Stable name, property IDs |
|---|---|
| `Read` and query term `WITH` | `Zenith.Render.MeshInstance`, properties 1–15 ("Render component registration") |
| `Read` and query term `WITH` | `Zenith.WorldTransform`, properties 1–7 (`Zenith2/Docs/Design/SCENE/Transforms.md → "Transform components"`) |
| `Read` and query term `WITH` | `Zenith.PreviousWorldTransform`, properties 1–3 |

One query slot, `Render.ExtractInstances/All`, holds the three `WITH` terms; the tracker's change test runs over the 25 declared columns.

### Instance extraction algorithm

The tracker's build function for one row, with `α = m_pxClock->GetInterpolationAlpha()` (the value the frame's `Flux_RenderWorld::m_fInterpolationAlpha` carries, [Z2-N-ARCH-415]):

1. `m_bVisible` false or a null `m_xMesh`: not drawn.
2. Resolve `m_xMesh` through `m_pxResolver->ResolveMesh` and each non-null override through `ResolveMaterial` ("Extraction resolver"): any `RESOLVE_OUTCOME_UNRESOLVED` holds the row with `HELD_REASON_UNRESOLVED`; otherwise any `RESOLVE_OUTCOME_NO_RUNTIME_OBJECT` holds it with `HELD_REASON_NO_RUNTIME_OBJECT`. The delta's `m_xMesh` becomes the resolved mesh ID, each override the resolved material ID, each null override the null `Flux_MaterialID`.
3. Transform: `Zenith_InterpolateWorldTransform(previous, world, α, xMatrix)` (`Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_TransformHierarchy header"`), which renders the current transform uninterpolated for a sheared or teleported row (`Zenith2/Docs/Design/SCENE/Transforms.md → "Interpolation data"`).
4. Teleport: when the row's simulation ID is in the tracker's teleport set (a `Zenith_RecordTeleport` played back in any tick of the frame, `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"`, [Z2-N-ARCH-550]), the delta is emitted as an UPDATE even if its hash is unchanged and its key is appended to `m_aulPendingHistoryResets`, which "Delta tracking" step 7 writes through `AddHistoryReset` ([Z2-N-REND-009]); a teleported entity is never MOVING after the frame of its teleport.
5. Bounds: the resolved mesh's `m_xLocalBounds` through the matrix by the centre-extent form (centre' = M · c; extent'ᵢ = Σⱼ |Mᵢⱼ| · eⱼ).
6. Fields: `m_uFlags = (m_bCastsShadows ? CASTS_SHADOW : 0) | (m_bHiddenInMain ? HIDDEN_IN_MAIN : 0)`; `m_uCustomData` = the component's `CustomData` unchanged when the resolved mesh has no primitive carrying `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (`m_bCustomCullDistance` false), and otherwise always `(CustomData & 0xFFFF) | (m_uCullDistanceMetres << 16)`, so the default 65,535 reaches the culling pass as "never culls" rather than leaving the component's high bits for it to read as a distance (`Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"`); `m_uLayerMask` = the component's value; the key `Flux_MakeEntityInstanceKey(xTable.GetPersistentID(slot), uWorldTag)` for an untracked row (the entity table of `m_pxWorld`, `Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_World header"`, resolved read-only, which any node of a phase may do), the stored key otherwise.

A skinned mesh's instance is extracted like any other; its pose is ANIM's record (row RP29), matched to the instance by the same key, which ANIM computes with `Flux_MakeEntityInstanceKey` from the same entity.

## Light extractor

`Render.ExtractLights` writes LIGHT's `Flux_RenderLightDelta` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"`) for the light components of `Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`. The sun and the moon are not light deltas: they are the render environment's slots ("Environment extractor").

### Light extractor declared reads

| Query slot | Terms |
|---|---|
| `Render.ExtractLights/Point` | `WITH Zenith.Lighting.PointLight` (1–15), `WITH Zenith.WorldTransform` (1–7), `WITH Zenith.PreviousWorldTransform` (1–3) |
| `Render.ExtractLights/Spot` | `WITH Zenith.Lighting.SpotLight` (1–17), the two transform terms, `WITHOUT Zenith.Lighting.PointLight` |
| `Render.ExtractLights/Directional` | `WITH Zenith.Lighting.DirectionalLight` (1, 2, 7–18), the two transform terms, `WITHOUT` point and spot |
| `Render.ExtractLights/Rect` (Stage 5) | `WITH Zenith.Lighting.RectLight` (1–19), the two transform terms, `WITHOUT` point, spot, directional |
| `Render.ExtractLights/Disc` (Stage 5) | `WITH Zenith.Lighting.DiscLight` (1–19), the two transform terms, `WITHOUT` the four others |

The `WITHOUT` terms give every entity at most one light: an entity carrying two light component types contributes the first of them in the order point, spot, directional, rect, disc, so no two light deltas share the entity's key. Each slot has its own tracker (`m_axLights[type]`).

A `WITHOUT` term lets an entity enter a query without any stamp: a structural move carries the row's versions and stamps only the columns new to the row (`Zenith2/Docs/Design/ECS/Backends.md → "Change tables"`), so an entity that loses its point light enters the spot query unchanged, and one tracker's row count can stay equal while one entity leaves it and another enters it. The light extractor therefore runs the step-5 sweep of all five trackers in any run in which any of them had `rows != expected` or met a new row of its query in step 3 (a stamped entry). This is exact: an unstamped entry into type k's query requires the entity to have lost a component of the higher-precedence type i it was extracted as, which is a departure from type i's query; the highest-precedence type with an unstamped entry or a departure can have no unstamped entry (that would need a departure from a still higher type), and departures alone always make its count differ; and a gained light component or transform column stamps a new row. Lights are few, so the extra sweeps cost O(light rows) in the frames that change precedence. REMOVEs of every light type are written before any ADD ("Delta tracking" step 7, applied across the five trackers), so the key's REMOVE precedes its new ADD in the partition. The instance tracker needs no such rule: its three terms are all `WITH`, so its count check alone is exact.

### Light extraction algorithm

The build function for one row of light type t:

1. Transform: `Zenith_InterpolateWorldTransform(previous, world, α, M)`; position `M · (0, 0, 0, 1)`; direction `normalize(−(M column 2))` (the light's local −Z); tangent (rect) `normalize(M column 0)` re-orthogonalised against the direction (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light record encodings"`). A zero-length direction column makes the light not drawn and counted in `Render.Extract.Lights.Refused`.
2. Units ("Light conversions" below) into `m_fIntensity`, `m_xTint`, the angles and the sizes.
3. Flags: `FLUX_LIGHT_FLAG_CASTS_SHADOW` from `CastsShadows`, `FLUX_LIGHT_FLAG_VOLUMETRIC` from `Volumetric`, `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` from the directional light's `ContactShadows`, `FLUX_LIGHT_FLAG_TWO_SIDED` from the rect and disc lights' `TwoSided`, `FLUX_LIGHT_FLAG_NO_SPECULAR` when `SpecularEnabled` is false; `m_uLightingChannels` from `LightingChannels`; `m_uVolumetricScale = clamp(round(VolumetricScale × 128), 0, 255)`; the shadow fields copied (`ShadowPriorityBias`, `ShadowMaxTileTexels`, `ShadowNormalOffsetTexels`, `ShadowDepthBiasMetres`).
4. Key `Flux_MakeEntityLightKey(persistent ID, uWorldTag)` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light keys"`), stored on first emission.
5. Validation: `Flux_ValidateLightDelta` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Validation"`); an accepted delta is written as returned (ignored flags cleared); a refused delta is not written, the entity is held as not drawn (a tracked light gets its REMOVE), `render.extract` logs `light <key>: refused` once per key and `Render.Extract.Lights.Refused` counts it. The render side validates again with the same pure function, so a delta can never be accepted on one side and refused on the other.

### Light conversions

| Light type | Intensity into `m_fIntensity` | Other members |
|---|---|---|
| point | property 4 `IntensityUnit` 0: candela as authored; 1: `Flux_LumensToCandelaPoint(Intensity)` | `m_fRange = RangeMetres`; `m_fSourceRadius = SourceRadiusMetres` |
| spot | as point, lumens through `Flux_LumensToCandelaSpot(Intensity, θ_outer)` | `m_fSpotInnerHalfAngle = InnerAngleDegrees × π / 180`, `m_fSpotOuterHalfAngle = OuterAngleDegrees × π / 180` (half-angles, as authored), range and radius as point |
| directional | `IlluminanceLux` (identity, lux) | `m_fSourceRadius = AngularRadiusDegrees × π / 180`; `m_fRange = FLT_MAX` |
| rect (Stage 5) | property 19 `Unit` (LIGHT's rect member, "replaces 4 for rect lights"; property 4 is not read) 0: nits as authored; 1: `Flux_LumensToNitsRect(Intensity, WidthMetres, HeightMetres, TwoSided)` | `m_fWidth`, `m_fHeight` (16, 17), `m_xTangentWS` |
| disc (Stage 5) | property 4 `IntensityUnit` (the disc schema has no `Unit`) 0: nits, the disc's stored unit; 1: `Flux_LumensToNitsDisc(Intensity, RadiusMetres, TwoSided)` | `m_fSourceRadius = RadiusMetres` (16); the barn cone angles from properties 18 and 19 as the spot's |

The rect schema carries two unit properties (4 and 19) and the disc schema one whose value 0 LIGHT names only for the point light (candela); reconciliation row RP35 asks LIGHT to state 4's meaning for area lights and to retire one of the rect's two, and until then the rows above apply.

Colour: when `TemperatureKelvin` is non-zero the tint is `Flux_KelvinToLinearRec709(TemperatureKelvin)` (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light units"`); otherwise the tint is `Colour` divided by its largest component c, and `m_fIntensity` is multiplied by c, so the tint's largest component is 1.0 as LIGHT's delta requires and an authored dim colour stays dim; a black colour gives intensity 0. The degenerate inputs of LIGHT's table (angles and sizes below 1e-4, negative or non-finite lumens) are LIGHT's functions' own clamps and logs.

## Camera extractor

`Render.ExtractCameras` writes one `Flux_ViewRequest` (`Zenith2/Docs/Design/RPROD/Views.md → "Flux_View header"`) per world for its main camera, and, from Stage 5, one per active texture camera. View requests are not deltas: every request is written every frame.

### Camera extractor declared reads

| Declaration | Stable name, property IDs |
|---|---|
| query `Render.ExtractCameras/All`: `WITH` | `Zenith.Render.Camera` (1–15), `Zenith.WorldTransform` (1–7), `Zenith.PreviousWorldTransform` (1–3) |
| same query, `OPTIONAL` | `Zenith.Lighting.CameraLighting` (1–20, `Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`) |

### View request fill

The extractor walks every chunk of the query (cameras are few, so it keeps no tracker), gathers every row's `Flux_CameraComponent` and simulation ID into two frame-arena arrays in chunk order, selects the main camera with `Flux_SelectMainCamera` over them ("Flux_RenderComponents header": among the rows with `Active` true and `Kind` `MAIN`, the highest `Priority`, ties to the lowest simulation ID, whatever the gather order) and fills its request:

| `Flux_ViewRequest` member | Value |
|---|---|
| `m_eKind` | `VIEW_KIND_MAIN` |
| `m_ulOwnerID` | the world tag: the main view's owner is "the world's stable ID" (`Zenith2/Docs/Rendering.md → "Render views"`, table "View identity and persistent state") |
| `m_uSubIndex` | 0 |
| `m_xCamera.m_xView` | the inverse of the interpolated world matrix (`Zenith_InterpolateWorldTransform` at α) with its scale removed: the matrix's columns 0–2 normalised before `Zenith_TryInvertAffine`; a singular matrix rejects the request |
| `m_xCamera.m_xProjection`, `m_xJitteredProjection`, `m_xPrevView`, `m_xPrevProjection`, `m_xJitterNDC`, `m_xPrevJitterNDC` | identity and zero: `Flux_BuildViewList` builds the projections from `m_xLens` with the view's depth mode, LIGHT's temporal code adds the jitter, and the registry supplies the previous-frame members (`Zenith2/Docs/Design/RPROD/Views.md → "View list construction"` step 4) |
| `m_xCamera.m_fNear`, `m_fFar` | `NearMetres`; `FarMetres`, with 0 written as `+∞` |
| `m_xCamera.m_eProjection` | `Projection` |
| `m_xCamera.m_bCameraCut` | false; the view list copies the request's `m_bCameraCut` into it |
| `m_xLens` | `m_fFovYRadians = FovYDegrees × π / 180`, `m_fOrthoHeightMetres = OrthoHeightMetres`, `m_fAspectOverride = 0` |
| `m_xOutputs` | `m_eKind = FLUX_VIEW_OUTPUT_SWAPCHAIN`, the other members zero |
| `m_uOutputWidth`, `m_uOutputHeight`, `m_xViewport` | 0: a swapchain request takes the main swapchain's extent and the whole output in the view list |
| `m_uFeatureMaskSet`, `m_uFeatureMaskClear` | `FeatureMaskSet`, `FeatureMaskClear` |
| `m_uUpdateIntervalFrames` | 1 |
| `m_xQualityOverrides` | the defaults (no override) |
| `m_uEditorViewportID`, `m_uReadsViewCount`, `m_axReadsViews` | 0, 0, zero |
| `m_xCameraLighting` | the 20 properties of `Zenith.Lighting.CameraLighting` in LIGHT's member order when the entity carries it, else a default-constructed `Flux_CameraLightingSettings` (row L2) |
| `m_bCameraCut` | "Camera cuts" below |

A lens outside the component's ranges (a field of view outside (0°, 179°], a non-positive near or orthographic height, a far at or below near other than 0, an infinite far with an orthographic projection, a non-finite value) rejects the request: nothing is written for that camera, `render.extract` logs `camera <entity>: invalid lens` once per entity and `Render.Extract.Cameras.Rejected` counts it; the next-priority active camera is not substituted, so a broken main camera renders nothing visibly rather than silently switching.

A Stage 5 `TEXTURE` camera fills the same members with `m_eKind = VIEW_KIND_TEXTURE`, `m_ulOwnerID = Flux_MakeEntityInstanceKey(persistent ID, uWorldTag).m_ulValue`, `m_xOutputs` of kind `FLUX_VIEW_OUTPUT_TEXTURE` with the output texture's handle, the output size and `m_uUpdateIntervalFrames` from the component (pending RP28, "Flux_RenderComponents header").

The underwater medium of a submerged view is a per-view extraction record, not part of ENV's blocks: ENV's `Environment.ExtractEnvironment` writes one `Flux_RenderEnvironmentView` per view request keyed by `Flux_MakeViewID(kind, owner, sub-index)` of the request's identity (`Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"`, `Zenith2/Docs/Design/ENV.md → "Render-world records"`), reading the engine camera component this file declares and computing the identity from the table above (kind `VIEW_KIND_MAIN`, owner the world tag, sub-index 0), so the record and this extractor's request name the same view; LIGHT turns the record into the view's `Flux_ViewMediumOverride` on the render side. That holds only while both extractors pick the same camera, so ENV selects it by calling `Flux_SelectMainCamera`, never by a copy of its rule (reconciliation rows RP25 (d) and RP34; the section member is Rendering's since X156); a divergent copy would key the override to no view. The camera extractor itself reads no water state, because ZenithRuntime has no edge to ZenithEnvironment.

### Game view target

<!-- assumption: pending core-doc (X259 (c)) -->
RPROD declares the world resource the editor adds to a play world it shows in a game viewport (A3 cross-area rows ED8, ED33 (b)):

```cpp
// Zenith2/Engine/ZenithFlux/Flux_RenderComponents.h addition
struct Flux_GameViewTarget                       // world resource "Zenith.Editor.GameViewTarget", owning phase 1; the editor writes it between frames through the runtime tool access, so no phase node writes it
{
	uint32 m_uEditorViewportID = 0;             // the game viewport's ID (1 … 65,535); 0: no target
	Flux_TextureHandle m_xOutput;                // the viewport's render-target tool texture
	uint32 m_uWidth = 0;
	uint32 m_uHeight = 0;
};
```

`Render.ExtractCameras` keys on the world's purpose, read through the extraction resolver's attach-time copy of `GetWorldPurpose()`:

| World purpose | Request written |
|---|---|
| `WORLD_PURPOSE_PLAY` without the resource, or with `m_uEditorViewportID` 0 | the main camera's `VIEW_KIND_MAIN` request, unchanged |
| `WORLD_PURPOSE_PLAY` with a target | the main camera's request with `m_eKind = VIEW_KIND_EDITOR`, `m_uEditorViewportID` the target's, `m_xOutputs` `FLUX_VIEW_OUTPUT_TEXTURE` with the target's texture, the target's extent, and `FLUX_VIEW_FEATURE_EDITOR_OVERLAY` cleared; the owner ID, cut and lighting are as for the main request |
| `WORLD_PURPOSE_EDIT`, `WORLD_PURPOSE_PREVIEW` | no camera request |

So no editor world writes `VIEW_KIND_MAIN`, and the game's camera logic, priority and cuts are RPROD's in the editor as in a Player. The extractor declares a read of the resource; a world without it reads nothing. `Test_CameraExtractor_GameViewTarget` asserts the three rows over one world of each purpose.
<!-- /assumption -->

### Camera cuts

`m_bCameraCut` is true in a frame when any of these holds, and false otherwise:

| Condition | Reason |
|---|---|
| the main camera's simulation ID is in `m_xCameraTeleports` | a teleport of the camera's entity or of any ancestor (SCENE dispatches the event to every descendant), LIGHT row L17 and `Zenith2/Docs/Design/LIGHT/Temporal.md` ("a teleport of the entity that owns the active camera, or of any of its ancestors, is a camera cut") |
| the main camera's `CutRequest` differs from `m_uMainCameraCutRequest` | a scripted cut (a script increments the property) |
| the main camera's simulation ID differs from `m_ulMainCameraSimulationID` and the latter is not 0 | switching cameras is a cut |

The view registry then treats every temporal history of the view as absent for the frame ([Z2-N-REND-009], [Z2-N-REND-599]). The extractor stores the main camera's simulation ID and `CutRequest` (a rejected lens included: the camera is still the main camera, and the frame merely has no view of it); a world with no active main camera writes no main request and stores 0. At the end of every run, whichever of these holds, it clears `m_xCameraTeleports`, so a teleport of a non-main camera, or one in a world with no main camera, is never carried into a subsequent frame's cut.

## Environment extractor

`Render.ExtractEnvironment` is the one writer of the world's `Flux_RenderEnvironment` (`Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`) and calls `SetEnvironment` exactly once per world per frame ([Z2-N-REND-008]), including in a full-state world.

### Environment extractor declared reads

| Declaration | Source |
|---|---|
| query `Render.ExtractEnvironment/WorldLighting`: `WITH Zenith.Lighting.WorldLighting` (1–7) | LIGHT's world-lighting component (`Zenith2/Docs/Design/LIGHT.md → "Light component schemas"`) |
| `ReadResource` of `Zenith.Environment.RenderBlocks` | ENV's `PHASE_INPUT` world resource of type `Flux_EnvironmentBlocks` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), written by `Environment.Advance` in phase 1 alone (`Zenith2/Docs/Design/ENV.md → "Frame-graph placement"`) |

The resource read is declared only when `m_pxTypes->FindTypeByStableName("Zenith.Environment.RenderBlocks")` finds the type, which ENV registers in its `RegisterTypes` step: a game without the Environment module declares no read of an unregistered type. The phase boundary orders the read after phase 1 of every tick of the frame ([Z2-N-ARCH-472]), so no edge inside phase 12 is declared or needed, and no phase-12 node writes the resource ([Z2-N-ARCH-476]; [Z2-N-ARCH-488]; extractors write only their partitions, [Z2-N-ARCH-419]). The extractor includes `ZenithFlux/Flux_Environment.h` and no ZenithEnvironment header.

### Environment extraction algorithm

1. World lighting: among the query's rows, the one with the lowest simulation ID fills `Flux_WorldLightingBlock` (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`) member by member from properties 1–7 (`m_bValid = true`; the two asset properties as the handles' persistent IDs); any further row is ignored, counted in `Render.Extract.WorldLightingDuplicates` and logged once per world; no row leaves the block's defaults (`m_bValid = false`).
2. Blocks: `pxBlocks = xContext.GetResource(type ID)`. Present: `m_xBlocks = *pxBlocks` (the struct holds no pointer, ENV's read contract); `m_xLighting.m_xAtmosphere` takes `m_bValid`, `m_uSkyRevision`, `m_xAtmosphere`, `m_xFog`, `m_xStars` and `m_xCloudShadow` from it with both spans empty; `m_xSun`, `m_xMoon`, `m_bHasSun`, `m_bHasMoon` from it (ENV has already keyed the slots with `Flux_MakeNamedLightKey`, `Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`). Absent: every member keeps its default (`Flux_AtmosphereBlock::m_bValid` false, `m_bHasSun` and `m_bHasMoon` false, `Flux_EnvironmentBlocks`' member defaults).
3. A frame in which no tick ran `Environment.Advance` (a frame with zero ticks) reads the resource's values of the last tick, which is what the extractor copies; nothing is interpolated.
4. `xWriter.SetEnvironment(xEnvironment)`. The writer's implementation (RPROD's) copies the value into the section and then re-points `m_xLighting.m_xAtmosphere.m_axLocalFogVolumes` at the section's own copy `m_xBlocks.m_axLocalFogVolumes[0 … m_uLocalFogVolumeCount)`, so the span points into render-world storage and never into the resource ([Z2-N-ARCH-416]). `m_axViewMediumOverrides` stays empty: LIGHT fills it on the render side from ENV's per-view records ("View request fill").
5. A second call returns `FLUX_RENDER_WORLD_ERROR_ENVIRONMENT_SET_TWICE`, which cannot happen with one writer and is therefore `ZENITH_CHECK`ed in development builds and logged in Shipping.

## Full-state extraction

When `xWriter.IsFullExtraction()` is true ([Z2-N-REND-015], `Zenith2/Docs/Rendering.md → "Purpose and scope"` heading "Render world resync" step 3):

| Extractor | Full-state behaviour |
|---|---|
| instances, lights | the pending-remove list, the pending history resets, the held set, the key map and the write starts are cleared; every row of every chunk is built and emitted as an ADD (unresolvable and not-drawn rows enter the held set); no REMOVE and no history reset is written (a re-created slot's previous transform equals its current one); MOVING is set for rows whose previous and current world transforms differ |
| cameras | unchanged: requests are full state every frame; a full-state frame is not a camera cut |
| environment | unchanged: `SetEnvironment` once |

`Test_EngineExtractor_FullStateWritesEveryLiveInstance` covers the instance and light rows.

## Determinism of extraction

- Within one extractor's partition the order is fixed by content, never by scheduling: REMOVEs in ascending key, then ADDs and UPDATEs in ascending simulation ID, then history resets in ascending key ("Delta tracking" step 7), each list rotated to start at its fairness start after a refused run ("Slot-full handling"), a value that is itself a function of the previous frames' content. The leaves of the chunk pass write only their own node-arena lists and read the maps, and the continuation sorts before writing, so the partition's bytes are independent of the worker count, of the leaf-to-worker assignment and of chunk order ([Z2-N-ARCH-702]: chunk order is storage order, which may differ between runs and backends).
- Across extractors, the partitions merge in registration order (instances, lights, cameras, environment, after every module extractor registered earlier), then Rendering's chunk-order rule applies to partitions of the same extractor, of which each engine extractor has one ([Z2-N-REND-008]).
- Every input is committed simulation state, the clock's interpolation factor, the resolver's answers (the outcome, and the `Flux_MeshID` and `Flux_MaterialID` slot values of a resolved asset) and the lifecycle events of the frame's ticks. The resolver's answers depend on load completion: the frame at which a held entity resolves, and the slot values GEOM's managers assign in the order assets load, both follow asynchronous loading, whose completion order depends on the worker count. That is presentation, never simulation (Z2-D-019), so a replay reproduces the partitions exactly when the same assets are resident with the same IDs at the same frames.

`Test_EngineExtractor_DeterministicAcrossWorkerCounts` asserts byte-identical sections at 1, 4 and 8 workers under that precondition: it extracts through `Zenith_FixtureExtractionResolver`, which answers every mesh and material resolved from frame 0 with fixed IDs, so no answer depends on load order.

## Slot-full handling

A writer call refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` has already been counted in `Flux.RenderWorld.Dropped` by the writer ([Z2-N-REND-008]). The extractor never asserts on it:

| Refused record | Kept so that the next frame writes it |
|---|---|
| an ADD | the entity enters the held set with `HELD_REASON_WRITE_FAILED` and no key-map entry, so the next run retries the ADD |
| an UPDATE | the entry keeps its previous `m_ulEmittedHash` and `m_ulEmittedTransformHash` and gets RETRY_UPDATE |
| a REMOVE | the key stays in the pending-remove list |
| a history reset | the key stays in `m_aulPendingHistoryResets` and is written by the next run ("Delta tracking" step 7); the instance shows one more frame of motion-vector smear per refused frame |
| a view request | nothing: requests are rewritten every frame; the frame has no view of that camera |
| `SetEnvironment` | nothing: rewritten next frame; the render side keeps the previous frame's environment for one frame (LIGHT's rule for a missing environment) |

Each refusal also increments `Render.Extract.Retried`. After the first refused record of a run the extractor calls the writer no more in that run: every remaining record of the run, of whatever kind (ADD, UPDATE, REMOVE, history reset), is kept by its row of the table above without a call, so a full arena costs one refused call per extractor per frame, not one per record. `Render.Extract.PendingRetries` gauges the records kept.

Under persistent overflow (an arena smaller than the records a frame needs, frame after frame), a fixed ascending order would refuse the same highest keys and simulation IDs every frame and starve them. The fairness rule: each of the three write lists of "Delta tracking" step 7 (the REMOVE keys, the ADD and UPDATE simulation IDs, the history-reset keys) is written in ascending order starting at the first element at or above its entry in `m_aulWriteStart` and wrapping to the lowest; the first refusal of a run sets that list's start to the refused element's key or ID, and a run that writes a whole list sets its start back to 0. So every record of a list is written within ⌈N / c⌉ frames of its first refusal, N being the list's length and c the records the arena accepts per frame; the lists keep their priority (REMOVEs, then ADDs and UPDATEs, then history resets), so a lower list advances only in frames the higher ones fit. The start values are content, not scheduling, so the partition's bytes stay independent of the worker count ("Determinism of extraction").

## Extraction counters and zones

| Counter | Kind | Meaning |
|---|---|---|
| `Render.Extract.Instances.Added`, `.Updated`, `.Removed`, `.HistoryResets` | delta | records written by `Render.ExtractInstances` this frame |
| `Render.Extract.Instances.Tracked`, `.Held`, `.Moving` | gauge | the tracker's sizes |
| `Render.Extract.Lights.Added`, `.Updated`, `.Removed`, `.Refused` | delta | the light tracker records and the refused deltas |
| `Render.Extract.Cameras.Requests`, `.Rejected`, `.Cuts` | delta | view requests written, rejected, cut |
| `Render.Extract.WorldLightingDuplicates` | cumulative | ignored extra world-lighting components |
| `Render.Extract.Sweeps`, `.Retried`, `.AssetRetries` | cumulative | removal-reconciliation walks; refused writes; held entries retried because the availability epoch moved |
| `Render.Extract.PendingRetries` | gauge | records a refused run kept for the next frame ("Slot-full handling") |

Each extractor node emits a profiler zone named after its node, `Render.ExtractInstances`, `Render.ExtractLights`, `Render.ExtractCameras` and `Render.ExtractEnvironment` ([Z2-N-BUDGET-223]), with the world suffix `@W<tag>` of the node name; `Z2-B-RPROD-003` Render extraction CPU is the sum of the four zones of every world per frame (MP-CPUZONE). The log category is `render.extract` (`ZENITH_LOG_CATEGORY`, `Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`).

## Extractor tests

`Zenith2/Engine/ZenithRuntime/Tests/EngineExtractors/` in `ZenithRuntime_Tests` (row RP26), run in the `Test` gate step (win-debug, win-release) and `Sanitizers -Preset win-asan` at stage acceptance. Each test builds a headless world through MOD's runtime test access with the four extractors attached, drives ticks, and reads the sealed render world's sections; no GPU is needed. Every test but `Test_EngineExtractor_ProductionResolverHoldsHeadless` passes `Zenith_FixtureExtractionResolver` as `m_pxResolver` ("Extraction resolver"), whose meshes and materials resolve to fixture IDs and bounds, because the headless asset registry's validate-only loader resolves none. The compositions compile the headless win-debug and win-release configuration of `ZenithRuntime_Tests`, in which `Render.Seal` is an EPILOGUE node and only the render subgraph is empty (pending MOD, row RP32).

| Test | Asserts |
|---|---|
| `Test_EngineExtractor_InstanceDeltasAddUpdateRemove` | a spawn of an entity with `Zenith.Render.MeshInstance` gives exactly one ADD with the entity's key; a transform change of one entity in a chunk of 100 gives exactly one UPDATE (that entity) and none for the 99 unchanged; a frame with no change gives no instance delta; a destroy gives exactly one REMOVE; removing the component from a live entity gives one REMOVE through the sweep; `m_bVisible = false` gives one REMOVE and `true` one ADD; a settle UPDATE follows the last transform change and clears MOVING whether it comes from step 3 or step 6; out of tick: an `ApplyOutOfTick` `SetProperty` of property 13 (`CastsShadows`) on a paused world gives one UPDATE in the next frame and none in the frame after it, an out-of-tick spawn (a world load) gives its ADDs in the next frame, and a `RestoreSimulationState` to the current tick index followed by an out-of-tick load gives a section whose GPU-scene replay equals exactly the loaded state; a held entity whose handle `GetChunkOfEntity` answers `ECS_ERROR_NO_COMPONENT` is dropped; a mesh the fixture flags with `m_bCustomCullDistance` gives bits 16–31 of `m_uCustomData` equal to 65,535 for the default and to the authored distance otherwise, and an unflagged mesh passes `CustomData` unchanged |
| `Test_EngineExtractor_FullStateWritesEveryLiveInstance` | with full extraction requested, the section holds one ADD per live renderable entity and per live light, no REMOVE and no history reset, and the next frame returns to deltas |
| `Test_EngineExtractor_TeleportEmitsHistoryReset` | a `Zenith_RecordTeleport` of a parent in tick t writes one history reset for the parent's and each renderable descendant's key in that frame and none in the next, and the teleported instances' deltas carry the current transform |
| `Test_EngineExtractor_InterpolatedTransform` | at α = 0.25 an entity moving from x = 0 to x = 4 between ticks is extracted at x = 1; a frame with zero ticks re-emits it at the new α; a sheared entity is extracted uninterpolated |
| `Test_EngineExtractor_LightDeltasMatchSchemas` | a point light of 800 lumens gives 63.66 cd (±1e-3); a spot of 30° outer angle converts degrees to radians and lumens by `Flux_LumensToCandelaSpot`; 6,500 K gives `Flux_KelvinToLinearRec709(6500)`; an authored colour (0.5, 0.25, 0) gives tint (1, 0.5, 0) and half the intensity; the direction is the entity's world −Z; an entity with a point and a spot light yields the point light alone; with entity A carrying a spot light and entity B a point and a spot light, A gaining a point light while B loses its point light in the same tick gives, in one partition, A's spot REMOVE before its point ADD and B's point REMOVE before its spot ADD, with both trackers' row counts unchanged; a rect light reads its unit from property 19 and a disc light from property 4; a delta `Flux_ValidateLightDelta` refuses is not written and is counted |
| `Test_EngineExtractor_CameraCutOnTeleport` | teleporting the camera's parent sets `m_bCameraCut` for exactly one frame; incrementing `CutRequest` sets it; switching the highest-priority camera sets it; a frame with none of these leaves it false; an invalid lens writes no request and counts it; a teleport in a world with no main camera leaves `m_xCameraTeleports` empty after the run and sets no cut when a main camera appears in a subsequent frame; `Flux_SelectMainCamera` returns the same camera for the rows in any order |
| `Test_EngineExtractor_OrderedBeforeSeal` | in the compiled frame schedule of the headless composition of "Extractor tests" with two worlds, each of the eight engine extractor nodes has an edge to `Render.Seal` and declares no ordering of its own |
| `Test_EngineExtractor_DeterministicAcrossWorkerCounts` | a 10,000-entity scene run for 60 frames with moving, spawned and destroyed entities, every asset resolved from frame 0 by the fixture resolver, gives byte-identical sections at 1, 4 and 8 workers |
| `Test_EngineExtractor_SlotFullDropsAndCounts` | with a render-world slot arena sized to refuse, the refused ADDs, UPDATEs, REMOVEs and history resets are written by the following frames and reach the section, each run makes one refused call, `Flux.RenderWorld.Dropped` and `Render.Extract.Retried` count them, nothing asserts, and the GPU-scene replay of the delta stream equals a run without refusals; under persistent overflow every refused ADD of N reaches the section within ⌈N / c⌉ frames |
| `Test_EngineExtractor_HeldEntitiesWaitForAvailability` | an entity whose mesh the fixture answers `NO_RUNTIME_OBJECT` (and one it answers `UNRESOLVED`) is held, the fixture's `ResolveMesh` call count for that mesh stays 1 across 60 frames with an unchanged epoch, and the entity is ADDed in the first frame after the test resolves the mesh and bumps the epoch; a property change of the held entity is rebuilt in its frame whatever the epoch |
| `Test_EngineExtractor_ProductionResolverHoldsHeadless` | `Zenith_AssetExtractionResolver` over a headless asset registry, with every mesh and material load completed before the entities spawn: 100 renderable entities are held with `HELD_REASON_NO_RUNTIME_OBJECT` in their first frame, `Render.Extract.AssetRetries` stays 0 across the next 60 frames and no instance delta is written |
| `Test_EngineExtractor_EnvironmentSetOnce` | "Environment extractor": `SetEnvironment` once per world per frame; a fixture `PHASE_INPUT` resource `Zenith.Environment.RenderBlocks` written in phase 1 of tick t is copied in the frame of tick t (atmosphere block, slots, blocks) with the local-fog-volume span pointing into the section; a world without the Environment module declares no resource read and writes the defaults; a frame with zero ticks copies the previous tick's values |
| `Test_EngineExtractor_KeysDisjointAndStable` | "Instance keys": entity keys have bit 63 clear, module keys set; one scene loaded in two sessions gives the same keys |
| `Test_RenderComponents_Registered` | both component types register with the stable names, property IDs, flags and defaults of "Render component registration" and round-trip a scene document by property ID |
| `Test_InstanceDelta_Layout` | "Flux_RenderInstanceDelta layout" (in `ZenithFlux_Tests`, `Tests/Renderer/`) |

## Stage placement of the extractors

| Stage | Lands |
|---|---|
| S2 | `Flux_RenderWorld.h` additions (keys, flags, the delta); `Flux_RenderComponents.h` and `Zenith_RegisterRenderComponentTypes`; the extraction state, the delta tracker, `Render.ExtractInstances` and `Render.ExtractCameras` (main kind), so the Stage 2 renderer draws a scene through the whole hand-off |
| S3 | `Render.ExtractLights` (point, spot, directional), `Render.ExtractEnvironment`, the procedural records |
| S5 | the rect and disc light queries; `TEXTURE` cameras once RP28 lands |

The S2 ticket is blocked by nothing beyond the Stage 2 core-doc state; its assumption blocks (RP26 and RP32 for MOD's tables and rules, X108 for the registration mechanism) are recorded in its own diff as the extractor ticket's design edits, and the two whose owner has not answered carry an exact interim (RP33: the availability epoch 0 headless and the frame index windowed; RP36: no custom cull distance), so neither blocks it.
