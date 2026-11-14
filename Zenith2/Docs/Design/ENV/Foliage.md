# ENV design: foliage and wind

Section file of `Zenith2/Docs/Design/ENV.md`. It specifies the two foliage classes, the foliage type document, `Zenith_FoliageSystem`, how placed instances stream into the GPU scene as ordinary instances through RPROD's instancing (`Zenith2/Docs/Design/GEOM/Meshes.md → "Instancing"`: "100k foliage instances of W-Outdoor are GPU-scene instances like any other"), density per quality tier and density shedding, the GPU detail-foliage generator (a source of GEOM's procedural geometry), the wind model, the shared Slang wind function and the foliage vertex modifier. Where instances come from (procedural placement and hand placement) is `Zenith2/Docs/Design/ENV/Procedural.md`; their cooked form is `Zenith2/Docs/Design/ENV/Bake.md → "Foliage cell file"`.

## Scope of this file

| Concern | Heading |
|---|---|
| placed versus detail foliage | "Foliage classes" |
| the foliage type document | "Foliage type document" |
| runtime API | "Zenith_FoliageSystem header" |
| instance records, keys, activation, full-state extraction | "Placed instance streams" |
| quality-tier density and simulation visibility | "Density per quality tier" |
| the instance cap and shedding | "Instance cap and shedding" |
| grass and small plants generated on the GPU through GEOM's procedural geometry | "Detail foliage generator" |
| global and local wind | "Wind model" |
| the one Slang wind function and its CPU mirror | "Wind shader function" |
| vertex deformation of foliage meshes | "Foliage vertex modifier" |

## Foliage classes

| Class | What | Source | Where it lives at runtime | Simulation-visible | Tier-scaled |
|---|---|---|---|---|---|
| placed | trees, bushes, rocks, large plants: any mesh asset with LODs | procedural rules and explicit instances, cooked per environment cell | GPU-scene instances (instance deltas through the render world); collidable ones also as PHYS static shapes in the cell content | only instances whose type has collision or `m_bGameplayVisible` | only render-only types |
| detail | grass blades, ground cover, small flowers: never collides | the tile density maps and splat layers, evaluated on the GPU | procedural instances in GEOM's procedural-geometry pool (`Zenith2/Docs/Design/GEOM/Procedural.md → "Pool layout"`), one per (detail cell, detail type), generated once and placed on the terrain at draw time ("Detail foliage generator") | never | always |

Placed instances are not ECS entities: an entity per instance would cost 100k entities in W-Outdoor for data no system writes. A placed instance is a 32-byte record in a cooked foliage cell; gameplay that needs an instance as an entity (a harvestable tree) uses a prefab in the scene instead (design ruling E3 in the entry).

## Foliage type document

`*.foliage.json`, an authoring document (Contracts' migration rules), one per foliage type:

```json
{
	"schemaVersion": 1,
	"id": "5b0f3a91-2c4e-4d7a-9e11-7a2c6b8d0f13",
	"name": "PineTall",
	"class": "placed",
	"mesh": "0d4c…-guid",
	"materialOverrides": [ null, "91aa…-guid" ],
	"footprintRadius": 2.5,
	"scaleRange": [ 0.85, 1.25 ],
	"alignToNormal": 0.15,
	"randomYaw": true,
	"castShadows": true,
	"cullDistance": 1800.0,
	"collision": { "kind": "capsule", "radius": 0.35, "height": 9.0, "physicalMaterial": "Wood" },
	"gameplayVisible": false,
	"renderOnly": false,
	"wind": { "stiffness": 0.8, "trunkBend": 0.02, "branchFlutter": 0.15, "leafFlutter": 0.6, "phaseVariation": 1.0 },
	"detail": null
}
```

| Key | Type | Rule |
|---|---|---|
| `class` | `placed` · `detail` | fixes the runtime path of "Foliage classes" |
| `mesh` | mesh asset GUID | placed: the mesh with its LOD chain (GEOM's LOD selection picks the LOD per instance on the GPU) |
| `materialOverrides` | array of material GUID or `null`, one per mesh section | `null` keeps the mesh's material; foliage materials are `Material_Foliage` variants ("Foliage vertex modifier") |
| `footprintRadius` | metres, 0.05 … 64 | the instance's radius, its one meaning: an excluding placement stage after the instance's own keeps `exclusionRadius + footprintRadius` from the instance, an explicit instance is an obstacle of this radius, and the scatter brush spaces the type's instances two radii apart (`Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"`, `Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"`); a rule's own spacing is its `minDistance` |
| `scaleRange` | two floats, 0.05 … 20, min ≤ max | uniform scale drawn per instance |
| `alignToNormal` | 0 … 1 | the fraction by which the instance's up axis leans from +Y toward the surface normal |
| `castShadows` | bool | sets `FLUX_INSTANCE_FLAG_CASTS_SHADOW` |
| `cullDistance` | metres, 1 … 10,000 | written into the instance's custom data as the distance-cull range; the GPU culling pass drops the instance beyond it (`Zenith2/Docs/Design/RPROD/Culling.md → "Per-instance cull distance"`, row RP21: the flag `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` is set on the type's mesh by GEOM's mesh cook when the mesh's import preset requests it, which CONTENT's foliage mesh preset does, pending REND and GEOM) |
| `collision` | `null` or `{kind: capsule·box·convex, …, physicalMaterial}` | a non-null value makes every instance simulation-visible and contributes a static shape to the cell's collision (`Zenith2/Docs/Design/ENV/Bake.md → "Collidable foliage"`) |
| `gameplayVisible` | bool | the instance list of this type is kept in the CPU cell data and queryable by gameplay (`Zenith_FoliageWorldResource::QueryInstances`), and the type is never tier-scaled |
| `renderOnly` | bool | true only when `collision` is null and `gameplayVisible` is false; the cook refuses any other combination (`COOK_ERROR_IMPORT_FAILED`, diagnostic `ENV_ERROR_DOCUMENT_SCHEMA`; Bake.md "Cook determinism") |
| `wind` | object | "Global wind state" parameters of the vertex modifier |
| `detail` | `null` (placed) or object (detail) | "Detail foliage generator": `bladeHeightRange` (two floats, 0.02 … 4 m), `bladeWidth` (0.002 … 0.5 m), `bendMax` (0 … 1, a fraction of the height), `cardSize` (0.1 … 4 m), `colourBase`, `colourTip` (linear RGB), `colourVariation` (0 … 1), `densityPerSquareMetre` (0 < ρ ≤ 64), `layers` (the layer IDs of the terrain palette it grows on, each `{layer, minWeight}` with a threshold in [0, 1]; the cook resolves them to palette indices per terrain, and a layer the terrain lacks is skipped with a cook warning), `coverageMask` (an optional card coverage texture GUID, class MASK); a terrain uses at most 4 detail types (`uENV_TERRAIN_MAX_DETAIL_TYPES`), the first 4 detail types of its foliage-type list, and the cook warns about the rest |

## Zenith_FoliageSystem header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_FoliageSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

/// @contract value Zenith_FoliageInstance
/// Ownership: Plain value; 32 bytes; the cooked record of one placed instance, read in place from a loaded cell.
/// Thread affinity: IMMUTABLE. Members non-const: the foliage cook builds each record and sorts the cell's record array
///   in place before writing it.
/// Invalidation: A pointer into a cell's record array is valid while the cell's asset handle is held.
/// Units: Metres (world position); the rotation is a unit quaternion quantised to SNORM16 x4 by the GEOM kernel.
/// Errors: None.
/// Version: since Z2 Stage 3. The layout is the cooked format of "Foliage cell file" (Bake.md); a change bumps that
///   format's version and the engine cook version.
/// @endcontract
struct Zenith_FoliageInstance
{
	float m_fPositionX = 0.0f;               // world metres
	float m_fPositionY = 0.0f;
	float m_fPositionZ = 0.0f;
	sint16 m_aiRotation[4] = {};            // SNORM16 quaternion (x, y, z, w), w ≥ 0, via the GEOM exact kernel
	float m_fScale = 1.0f;
	uint16 m_uTypeIndex = 0;                // into the cell's type table
	uint16 m_uThinningRank = 0;             // "Tier thinning rule"; 0 … 65535, drawn at cook
	uint32 m_uInstanceKey = 0;              // unique within the cell: Zenith_PlacementInstanceKey or Zenith_ExplicitInstanceKey
};
static_assert(sizeof(Zenith_FoliageInstance) == 32);

/// @contract value Zenith_FoliageSettings
/// Ownership: Plain value filled by the module from its configuration keys at start and at every quality-tier change.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: m_fDensityScale and m_fDetailDensityScale are fractions in [0, 1]; distances in metres.
/// Errors: Out-of-range keys are rejected by the configuration layer before they reach this value.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FoliageSettings
{
	float m_fDensityScale = 1.0f;            // environment.foliage.densityScale: render-only placed types
	float m_fDetailDensityScale = 1.0f;      // environment.foliage.detailDensityScale
	float m_fDetailRadius = 96.0f;           // environment.foliage.detailRadius
	float m_fCullDistanceScale = 1.0f;       // environment.foliage.cullDistanceScale
	uint32 m_uMaxActivationsPerFrame = 8192;   // environment.foliage.maxActivationsPerFrame
	uint32 m_uInstanceCap = 0;              // environment.foliage.instanceCap; from the pending row "Placed foliage instances resident cap"
	float m_fDetailNearRadius = 32.0f;       // environment.foliage.detailNearRadius: blades inside, cards outside
	uint32 m_uDetailMaxVertices = 1048576;  // environment.foliage.detailMaxVertices
	uint32 m_uDetailCellsPerFrame = 8;      // environment.foliage.detailCellsPerFrame
};

/// @contract value Zenith_FoliageQueryResult
/// Ownership: Plain value returned by QueryInstances; the record pointer borrows the cell's data.
/// Thread affinity: IMMUTABLE for the phase in which it was obtained. Members non-const: an element of an output array
///   that its producer fills in place.
/// Invalidation: m_pxInstance is invalid after the tick in which it was returned.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_FoliageQueryResult
{
	const Zenith_FoliageInstance* m_pxInstance = nullptr;
	Zenith_PersistentID m_xFoliageType;
	Zenith_EnvCellCoord m_xCell;
	float m_fDistance = 0.0f;
};

/// @contract type Zenith_FoliageWorldResource
/// Ownership: One per world, owned by the Environment module instance and registered as the world resource
///   "Zenith.Environment.FoliageWorld" with owning phase PHASE_INPUT; it holds the foliage cell asset handles of the
///   active cells.
/// Thread affinity: PHASE(Input) for writes (Environment.UpdateFoliageCells alone); reads from any system declaring
///   ReadResource, and from the foliage extractor.
/// Invalidation: The active cell set changes only at phase 1 of a subsequent tick.
/// Units: Metres.
/// Errors: QueryInstances returns ENV_ERROR_CAPACITY when more results exist than axOut holds (the first axOut.GetCount()
///   in canonical order are written); a render-only type is never answered (ENV_ERROR_FOLIAGE_NOT_QUERYABLE).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_FoliageWorldResource
{
public:
	// Instances of gameplay-visible or collidable types within fRadius of xCentre, sorted by (distance, cell, key).
	Zenith_Result<uint32, Zenith_EnvironmentError> QueryInstances(const Zenith_Vector3& xCentre, float fRadius,
		const Zenith_PersistentID& xFoliageTypeOrNull, Zenith_Span<Zenith_FoliageQueryResult> axOut) const;
	uint32 GetActiveCellCount() const;
	uint64 GetResidentInstanceCount() const;
};

/// @contract functions Zenith_FoliageFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Fractions in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
// "Tier thinning rule": whether a render-only instance of the given rank is drawn at the given density scale.
constexpr bool Zenith_FoliageKeptAtDensity(uint16 uThinningRank, float fDensityScale);
// The render-instance key of a placed instance ("Render instance keys").
uint64 Zenith_FoliageRenderKey(uint16 uWorldTag, Zenith_EnvCellCoord xCell, uint32 uInstanceKey);
```

`Zenith_FoliageSystem` is internal (`Internal/Foliage/Zenith_FoliageSystem.h`): the per-world object of the module instance that owns the resource above, runs `Environment.UpdateFoliageCells` and the foliage extractor, and holds the extraction bookkeeping of "Activation and deactivation".

## Placed instance streams

### Instance record layout

The record is `Zenith_FoliageInstance` above: 12 bytes of cooked world position (a cell spans at most 128 m, but world positions avoid a per-cell origin; a rebase is applied at extraction, below), 8 bytes of SNORM16 quaternion encoded and decoded through GEOM's exact kernel (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"`) so the rotation the cook wrote is the one every platform reads, 4 bytes of scale, the type index, the thinning rank and the stable instance key. Records of a cell are sorted by (type index, instance key).

Cooked positions are in the world frame the terrain was authored in. When STREAM has rebased the world origin by an offset O (pending STREAM, "Large coordinates" in Bake.md), the extractor builds each instance's local-to-world translation as `(record position − O)`, subtracted in `double` and narrowed to `float` last, and every CPU query of the foliage resource takes its inputs in the rebased frame and adds O before comparing with records, so a rebase never rewrites cooked data.

### Render instance keys

Each placed instance is one `Flux_RenderInstanceDelta` (RPROD, pending) whose `Flux_RenderInstanceKey` is `Zenith_FoliageRenderKey(worldTag, cell, instanceKey)`: `Zenith_Hash64U64(instanceKey, Zenith_Hash64U64(pack(cell), Zenith_Hash64U64(worldTag, ulENV_FOLIAGE_KEY_SALT)))`, with `ulENV_FOLIAGE_KEY_SALT = 0x456E76466F6C6961` ("EnvFolia"). The key is stable across sessions and cell reloads, so a cell that unloads and reloads re-adds the same keys.

The delta carries the type's `Flux_MeshID` (resolved from its mesh asset handle, `Zenith2/Docs/Design/GEOM/Meshes.md → "Flux_MeshResource header"`), the material overrides, the local-to-world matrix built from the record (translation, rotation, uniform scale, leaned by `alignToNormal` at cook), the shadow flag, and 32 bits of custom data: bits 0–15 the instance's wind phase seed (the low 16 bits of the instance key's hash), bits 16–31 the type's cull distance in metres × `m_fCullDistanceScale`, saturated to 65,535.

### Activation and deactivation

| Step | Where | Action |
|---|---|---|
| 1 | a streamed cell spawns its `Zenith_FoliageCellComponent` entity (STREAM, pending) | the component holds the cell's `Zenith_AssetHandle` of kind `Environment.FoliageCell` |
| 2 | `Environment.UpdateFoliageCells` (phase 1) | a newly present component's asset is requested at `ASSET_PRIORITY_NORMAL`; a component whose asset became active at this tick boundary is added to the active set; a removed component's cell leaves the active set |
| 3 | the foliage extractor `Environment.ExtractFoliage` (phase 12) | compares the active set with its bookkeeping (the cells it has emitted and how far): for a new cell it emits ADD deltas for the cell's drawn instances, continuing next frame when the frame's `m_uMaxActivationsPerFrame` is used up; for a removed cell it emits REMOVE for every key it had emitted |
| 4 | Flux (`Flux.BuildGraph`) | applies the deltas to the GPU scene; culling, LOD and draws are GPU-driven like every instance |

- The extractor's bookkeeping is presentation state owned by the module instance, excluded from the world state hash and from saves; it is written only by the one extractor node, so no lock is needed.
- Cells are served in ascending order of (distance from the environment anchor's position rounded to 16 m, cell z, cell x), so the nearest foliage appears first after a teleport; the order is presentation-only. The anchor is the world position of the entity carrying `Zenith_EnvironmentAnchorComponent` (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"`), which a game puts on its main camera rig and EDIT on the active editor camera; with no anchor the world origin is used and `ENV_WARNING_ANCHOR_MISSING` is logged once per world. The extractor reads the anchor's world transform through SCENE's transform read like any component it declares a read of.
- A cell whose asset fails to load (`ASSET_STATE_FAILED`) stays empty, is counted in `environment.foliage.cells_failed`, and never blocks other cells.

### Full-state extraction

When `Flux_RenderWorldWriter::IsFullExtraction()` is true ([Z2-N-REND-015]), the extractor clears its bookkeeping and re-emits every drawn instance of every active cell as ADD in cell order, without the per-frame limit, so a resync rebuilds the GPU scene's foliage exactly.

## Density per quality tier

### Render-only and simulation-visible instances

A type is render-only when it has no collision and is not gameplay-visible. Only render-only types are thinned by the quality tier and shed under pressure; every other instance is drawn whenever its cell is active, whatever the tier, so what the camera sees of a collidable tree never disagrees with what blocks the player, and no simulation result depends on the tier (Z2-D-019; [Z2-N-ARCH-812]).

### Tier thinning rule

The cook draws each instance's `m_uThinningRank` uniformly in [0, 65535] from its placement stream ("Seeded streams" in Procedural.md). An instance of a render-only type is drawn when `Zenith_FoliageKeptAtDensity(rank, s)`, i.e. `rank < floor(s × 65536)`, with `s = m_fDensityScale`. Lowering `s` removes instances in a fixed order, so raising it again brings back exactly the instances that were removed, with no popping of others. A tier change (`Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier changes"`) reaches the extractor through `Zenith_FoliageSettings` and produces REMOVE or ADD deltas for the affected instances over the following frames, within the activation limit.

| Key | Allowed layers | Desktop default | S24 default | Meaning |
|---|---|---|---|---|
| `environment.foliage.densityScale` | engine default, platform, game, quality tier, user | 1.0 | 0.6 | placed render-only density |
| `environment.foliage.detailDensityScale` | same | 1.0 | 0.5 | detail foliage density |
| `environment.foliage.detailRadius` | same | 96 m | 48 m | detail foliage radius |
| `environment.foliage.cullDistanceScale` | same | 1.0 | 0.6 | scales every type's cull distance |

The defaults above are the engine-default and platform layers; the quality-tier files (PLAT) may override them per tier. Thermal adaptation lowers the tier (`Zenith2/Docs/Design/PLAT/Android.md → "Applying thermal actions"`), which lowers these values without any ENV code knowing about thermal state.

## Instance cap and shedding

- `m_uInstanceCap` (the pending row "Placed foliage instances resident cap", W-Outdoor's 100k being its sizing case) bounds the number of placed instances the extractor keeps in the GPU scene per world.
- When adding a cell's instances would exceed the cap, render-only instances are shed first: the extractor emits the new cell's simulation-visible instances in full and keeps render-only instances of all active cells in ascending thinning rank (the tier order above) while the count stays within the cap, so the highest ranks are shed first; the shed count is `environment.foliage.instances_shed`.
- When the GPU scene refuses an instance (the instance table full, RPROD's `FLUX_RESOURCE_ERROR_TABLE_FULL`), the same order applies on the next frame with the cap lowered to the accepted count, and `ENV_WARNING_FOLIAGE_TABLE_FULL` is logged once per session.
- Under memory pressure the foliage cell data belongs to `MEMORY_SUBSYSTEM_STREAMING` and is released when STREAM evicts cells (the Streaming row of `Zenith2/Docs/Architecture.md → "Memory governor"`, table "Shedding per subsystem"); ENV sheds nothing else of its own.

## Detail foliage generator

### Detail source

Detail foliage is procedural geometry, which GEOM owns (orchestrator ruling on Q38): ENV implements GEOM's generator interface and never allocates, writes or sizes the pool itself.

<!-- assumption: pending RPROD, REND (X141) -->
`Flux_DetailFoliageSource` (`Zenith2/Engine/ZenithFlux/Internal/Environment/Flux_DetailFoliageSource.cpp`, `.h`) implements `Zenith_ProceduralGeometrySource` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`). `Flux_EnvironmentRenderer` registers it at its own creation through `Flux_Renderer::GetProceduralGeometry().RegisterSource` (GEOM's row GM22) and calls `UnregisterSource` before its destruction; admission, slots, the pass and the draw path are GEOM's ("Registration and sizing", "Per-frame regeneration", "Flux_AddProceduralGeometry contract", "Drawing procedural instances" of that file). The instances reach GEOM as `Flux_RenderProceduralInstance` records appended with `Flux_RenderWorldWriter::AddProceduralInstance`, core-doc item X141 (GEOM's row GM22), pending core-doc batch 4. Recorded as cross-area row EN16.
<!-- /assumption -->

| Descriptor field (`Flux_ProceduralGeneratorDesc`) | Value |
|---|---|
| `m_szName` | `"env.detailFoliage"` (the generator key is its `Zenith_Hash64`) |
| `m_xProgram` | `Zenith2/Shaders/Passes/Procedural/DetailFoliage.slang` → `Flux_DetailFoliageParams`, which begins with `Flux_ProceduralDispatchData m_xDispatch` and GEOM's two resource fields |
| `m_uFrameParameterBytes` | 48 ("Detail parameters") |
| `m_eUV0Format` | `FLUX_PROCEDURAL_UV_FORMAT_UNORM16X2`: stream-set code 0x2010, `procedural:pos-snorm16x4|surface-qt16-uv16` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets"`) |
| `m_uUVReferenceResolution` | 1: UV0.x is the along-blade fraction and UV0.y an integer lattice index, neither a texel coordinate, so GEOM's UV precision warning never applies |

### Detail cells and lattice

- A detail cell is 16 m × 16 m aligned to the terrain origin. The foliage extractor (`Environment.ExtractFoliage`, phase 12) selects the frame's cells: every cell of an active terrain tile whose centre lies within the tier's outer radius R of the environment anchor ("Activation and deactivation"), keeping an already emitted cell until its centre leaves R + 8 m.
- Each selected cell has one procedural instance per detail type of the terrain (at most 4) whose density map exists on the cell's tile. The instance's lattice is `n × n` points with `n = min(cap(ring), ceil(16 × sqrt(ρ)))`, `ρ` the type's `densityPerSquareMetre` and `cap(ring)` the tier's lattice cap of "Detail foliage tiers": the **near ring** (cells whose centre lies within `m_fDetailNearRadius`, with the same 8 m hysteresis) draws blades, the **far ring** cards. The density scale never changes n; it scales acceptance at draw time, so lowering it removes blades in a fixed order.
- The extractor admits cells nearest first (distance rounded to 1 m, then cell z, then cell x) while the vertex total of the admitted instances, `Σ n² × 8`, stays within `m_uDetailMaxVertices`; a cell that does not fit is not emitted and counts `environment.foliage.detail_cells_capped`. At most `m_uDetailCellsPerFrame` new instances are emitted per frame.
- A ring change, a tier change of n and an edit of the tile's height range are topology changes: the extractor raises the instance's `m_uTopologyVersion`, and GEOM keeps drawing the old version until the new one is drawable. A density or splat edit needs no regeneration at all, because placement happens at draw time ("Draw-time placement").

| Per instance | Value |
|---|---|
| instance key | `Zenith_Hash64U64(pack(cx, cz), Zenith_Hash64U64(typeIndex | blockIndex << 8, Zenith_Hash64U64(terrainKey, ulENV_DETAIL_SALT)))`, `ulENV_DETAIL_SALT = 0x456E764465746169` ("EnvDetai") |
| `Flux_RenderInstanceDelta` | ADD on admission, REMOVE when the cell leaves; `FLUX_INSTANCE_FLAG_PROCEDURAL`, no shadow flag at the far ring (cards cast no shadow), a null mesh, material override 0 = the terrain's detail material (the sub-asset `material:detail`, `Material_DetailFoliage`, cooked from Stage 4 for a terrain with at least one detail type, `Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`) |
| local-to-world | the translation `(cellMinX − Ox, hMin, cellMinZ − Oz)` with O the rebase offset, no rotation, unit scale |
| `m_uCustomData` | bits 0–7 the terrain's extractor slot S (`Flux_RenderTerrainDelta::m_uTerrainSlot`, which the same extractor bookkeeping assigned, `Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"`), resolved by the detail material through `Env_FrameEnvironment::m_xTerrainSlots[S]`; bits 8–9 the detail type index; bit 10 the ring (0 blades, 1 cards); bits 11–17 n (1 … 64); bits 18–21 the block index; bits 22–31 zero |
| topology parameters (32 bytes) | `uint32 n`, `uint32 ring`, `f32 hMin`, `f32 hMax` (the cell's closed-rectangle height range from the tile CPU data), `f32 padXZ`, `f32 extentY` ("Blade and card geometry", from the type's `detail` block), `uint16 firstRow`, `uint16 rowCount` (the block; 0 and n when unsplit), 4 reserved bytes |
| the calls | the source's `GetDesc`, `ComputeSizing` and `WriteIndices` (`Zenith2/Docs/Design/GEOM/Procedural.md → "Flux_ProceduralGeometry header"`), called by GEOM's admission on `Flux.BuildGraph` ("Registration and sizing" of that file); the record is GEOM's `Flux_RenderProceduralInstance` with `m_ulGeneratorKey = Zenith_Hash64("env.detailFoliage")` |
| `REGENERATE` | set on the admission record and on the record of a topology change, with the frame parameters below; no record is written in other frames |

### Blade and card geometry

`ComputeSizing(topology)`: `m_uVertexCount = n² × 8`; `m_uIndexCount = n² × 18` for blades, `n² × 12` for cards; `m_xBounds` = `[−padXZ, −fENV_DETAIL_WIND_PAD, −padXZ] … [16 + padXZ, (hMax − hMin) + extentY, 16 + padXZ]` in the instance's object space, with `padXZ = max(cardSize / 2, bendMax × bladeHeightMax + bladeWidth / 2) + fENV_DETAIL_WIND_PAD` and `extentY = max(bladeHeightMax, cardSize) + fENV_DETAIL_WIND_PAD`, `fENV_DETAIL_WIND_PAD` = 0.5 m (the extractor computes both from the type and carries them in the topology parameters); UV0 range `[0, 0] … [1, n² − 1]`. GEOM's per-instance limits (`uGEOM_MAX_PROCEDURAL_VERTICES` = 65,535 vertices, 16-bit slot-local indices; `uGEOM_MAX_PROCEDURAL_INDICES` = 393,216) bound one instance to 8,191 lattice points at 8 vertices each. A (cell, type) pair whose lattice exceeds that is therefore split into **blocks**: the lattice rows are cut into `B = ceil(n² / 4,096)` blocks of `ceil(n / B)` consecutive rows (the last block shorter), each block its own procedural instance with its own key (the block index is hashed into the instance key and carried in custom data bits 18–21), topology parameters (`n`, the block's first row and row count) and slot; `ComputeSizing` sizes a block as `rows × n × 8` vertices. With the lattice capped at `uENV_DETAIL_MAX_LATTICE` = 64, one block of at most 4,096 points (32,768 vertices, 73,728 indices) holds every (cell, type) and B = 1; the tiers of "Detail foliage tiers" cap n at 48, so desktop-high density never needs a second block, and a density raise beyond the cap keeps the cap rather than exceeding GEOM's limit.

`WriteIndices`: point p owns vertices `8p … 8p + 7`. A blade is a strip of three quads over four vertex pairs (vertex `8p + 2i + s` is side s of level i): triangles `(2i, 2i + 2, 2i + 1)` and `(2i + 1, 2i + 2, 2i + 3)` for i = 0, 1, 2, offset by 8p. A card is two crossed quads (vertices 0–3 and 4–7): `(0, 2, 1)`, `(1, 2, 3)`, `(4, 6, 5)`, `(5, 6, 7)`, offset by 8p. Front faces are counter-clockwise ([Z2-N-REND-885]); the detail material is two-sided.

The generator program writes, per vertex, with GEOM's `Flux_ProceduralWriteVertex`: point `p = localVertex / 8` at lattice `(a, b) = (p mod n, p / n)`, spacing `s = 16 / n`; the pivot `((a + d1) × s, 0, (b + d2) × s)` with the draws `d_k` of "Generator hash"; for a blade of height `H = lerp(bladeHeightMin, bladeHeightMax, d3)`, facing `θ = 2π d4` and bend `β = bendMax × d5`, vertex (i, side) at fraction `t = i / 3`: `pivot + side offset (side − 0.5) × bladeWidth × (1 − t) along the facing's perpendicular + (0, t × H, 0) + β × t² × H along the facing`, the tangent along the blade, the normal the facing, sign +1, UV0 `(t, p)`; a card's vertices are its two crossed vertical quads of edge `cardSize` standing on the pivot (x or z within ±`cardSize / 2` of it, y from 0 to `cardSize`), UV0 `(0 or 1 by row, p)`. Why every written position lies in the box without the writer's clamp: pivots lie in `[0, 16)` per axis; a blade vertex is within `bendMax × H + bladeWidth / 2` of its pivot horizontally (the bend `β × t² × H` with β ≤ bendMax and t ≤ 1, the side offset at most half the width) and between 0 and H vertically; a card vertex within `cardSize / 2` horizontally and between 0 and `cardSize` vertically; the draw-time lift adds at most `hMax − hMin` and the wind at most `fENV_DETAIL_WIND_PAD` in any direction, because the detail material clamps the length of its wind offset to it ("Draw-time placement"). The box also bounds culling, so the lifted, wind-displaced blades are never culled. `Test_DetailFoliage_SourceSizingAndIndices` evaluates the generator on the CPU with the extreme parameters (`cardSize` 4, `bladeHeightMax` 4, `bendMax` 1, `bladeWidth` 0.5, every draw 0 and 1) and asserts every position inside the returned box before the clamp.

### Draw-time placement

`Material_DetailFoliage` (`Zenith2/Shaders/Materials/DetailFoliage.slang`, domain `alphaTest`, `twoSided`) positions the generated geometry on the terrain in its vertex modifier (the hook of "Material_Foliage"), for every pass that draws it:

1. `p = round(UV0.y)`, `n` from the instance's custom data; the pivot's world XZ is the instance translation plus `((a + d1) × s, (b + d2) × s)`, recomputed with the same hash as the generator.
2. Acceptance: `P = density(pivot) × layerFactor(pivot) × m_fDetailDensityScale`, where density is channel `t` (the detail type) of the tile's packed detail-density texel (u8 / 255, `Load` from `Env_TerrainTileTextures::m_xDetailDensity`) and layerFactor 1 when the type lists no layers, else the largest splat weight among its listed layers at or above their thresholds (0 when none), both read with `Load` through the terrain's tile table (the record `m_xTerrainSlots[S]` of the custom data's slot, Terrain.md "Terrain material module"); the blade is kept when `d0 < P`. A rejected blade returns the pivot for every vertex, which collapses its triangles to zero area.
3. A kept blade's vertex is lifted by `h(pivot) − hMin` (hMin = the instance translation's y), with `h` the terrain's triangle height of "Triangle interpolation" read from the height tile at its finest resident mip (mip 0 wherever GEOM's terrain requires level-0 nodes, which covers every detail radius of the tier table), then displaced by `Env_FoliageWindOffset(m_xWind, xType, pivot, position, half3(t, 0, 0), uint(d7 × 65536), time)`: `m_xWind` of the frame's `Env_FrameEnvironment`, `xType` the detail type's `wind` block (`stiffness`, `trunkBend`, `branchFlutter`, `leafFlutter` of its foliage type document) as an `Env_FoliageWindParams` in the detail material's per-type constants, the vertex weights `(t, 0, 0)` with `t = UV0.x` (a blade bends as a trunk and has no branch or leaf flutter), and the phase seed from draw 7; the offset's length is clamped to `fENV_DETAIL_WIND_PAD` (0.5 m), for the current and the previous frame's time.

`Evaluate` shades from UV0 alone: base-to-tip colour `lerp(colourBase, colourTip, t)` of the type (the material's per-type constants), varied by `colourVariation × (d6 − 0.5)`; `EvaluateOpacity` is 1 (cards use a per-type coverage mask texture when the type names one). Placement at draw time means the density scale, a tier change of the scale, and editor density and splat edits change what is drawn in the same frame without regenerating a slot.

### Generator hash

Draw `k` of lattice point p of detail type t in detail cell `(cx, cz)` of terrain T is `d_k = Zenith_RandomToUnitFloat(Zenith_Hash64U64(k, Zenith_Hash64U64(p, cellSeed)))` with `cellSeed = Zenith_Hash64U64(pack(cx, cz), Zenith_Hash64U64(t, T.m_ulPlacementSeed ^ ulENV_DETAIL_SALT))`, computed on the GPU in 32-bit integer arithmetic by the GPU port of the pinned hash in `Zenith2/Shaders/Environment/EnvHash.slang` (pending D-SEL-HASH (open, Stage 1): the port implements the selected function; until then the generator exists as the API-only slice). Draws: 0 acceptance, 1 and 2 jitter, 3 height, 4 facing, 5 bend, 6 colour variation, 7 wind phase. The generator and the vertex modifier evaluate the same function, so a blade's identity depends on the lattice point and never on the frame, the camera or the dispatch order (no flicker under TAA), and a lower density keeps a subset of the higher density's blades.

### Detail parameters

The 48 frame-parameter bytes of a regeneration record: `uint32 cellSeed[2]` (low word first), `uint32 n`, `uint32 ring`, `f32 spacing` (16 / n), `f32 bladeHeightMin`, `f32 bladeHeightMax`, `f32 bladeWidth`, `f32 bendMax`, `f32 cardSize`, 8 reserved bytes, all from the type's `detail` block and the instance.

### Detail foliage tiers

| Tier class | Outer radius R | Near radius (blades) | Near lattice cap | Far lattice cap (cards) | Density scale | `detailMaxVertices` |
|---|---|---|---|---|---|---|
| desktop high | 96 m | 32 m | 48 | 16 | 1.0 | 1,048,576 |
| desktop low | 64 m | 24 m | 40 | 12 | 0.7 | 524,288 |
| S24 high | 48 m | 24 m | 32 | 12 | 0.5 | 262,144 |
| S24 low | 32 m | 0 m (cards only) | — | 12 | 0.35 | 131,072 |

### Detail pool sizing

The worst case per world, for GEOM's pool row `Z2-B-GEOM-001` and regeneration row `Z2-B-GEOM-005` (inputs to the Stage 1 per-system budget ticket for GEOM, row EN16):

| Quantity | Formula | desktop high | desktop low | S24 high | S24 low |
|---|---|---|---|---|---|
| cells in range (square bound) | `(2(R + 8) / 16 + 1)²`, of which near `(2(R_near + 8) / 16 + 1)²` | 196, near 36 | 100, near 25 | 64, near 25 | 36, near 0 |
| uncapped vertices, 4 detail types | `4 × 8 × (near × capNear² + far × capFar²)` | 3,964,928 | 1,625,600 | 998,912 | 165,888 |
| resident vertices | `min(uncapped, detailMaxVertices)` | 1,048,576 | 524,288 | 262,144 | 131,072 |
| pool bytes (20 per vertex: 8 in region 0, 12 in region 1) | | 20.0 MiB | 10.0 MiB | 5.0 MiB | 2.5 MiB |
| index bytes (blades 18 and cards 12 indices per 8 vertices, 2 bytes each; upper bound at 18) | `vertices × 4.5` | 4.5 MiB | 2.25 MiB | 1.13 MiB | 0.56 MiB |
| vertices regenerated per frame, at most | `detailCellsPerFrame × cap² × 8` with the tier's largest lattice cap | 147,456 (8 cells) | 102,400 (8 cells) | 32,768 (4 cells) | 4,608 (4 cells) |

The S24 tier files set `environment.foliage.detailCellsPerFrame` to 4.

<!-- informative -->
Zenith regenerated every grass blade every frame from a lattice hash (`Zenith/Flux/Vegetation/CLAUDE.md`), which made identity stable but paid the placement cost every frame. Zenith2 keeps the hash-stable identity, generates each cell's geometry once into GEOM's pool, and moves the terrain-dependent decisions to the vertex modifier, because GEOM's generator reads only its frame parameters and the terrain textures it would need change their descriptors on every streaming swap.
<!-- /informative -->


## Wind model

### Global wind state

The world's wind is part of `Zenith_EnvironmentState` (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentState header"`), computed each tick in phase 1 from the weather:

| Field | Units | Rule |
|---|---|---|
| `m_fDirectionX`, `m_fDirectionZ` | unit vector in XZ | the weather's blended direction, rotated by the slow wander below |
| `m_fSpeed` | m/s | the weather's blended mean speed, 0 … 60 |
| `m_fGustStrength` | fraction of speed, 0 … 2 | the weather's blended gust strength |
| `m_fGustFrequency` | Hz, 0.01 … 2 | the weather's |
| `m_fTurbulence` | 0 … 1 | the weather's |
| `m_fWanderAngle` | radians | a random walk: each tick adds `(u − 0.5) × 2 × m_fWanderRate × dt`, `u` the tick's draw 0 of the wind stream, clamped to ±0.5 rad around the weather's direction |

The wind stream is `Zenith_RandomSource(worldSeed, Zenith_Hash64("Zenith.Environment.Wind"))` with its counter set to `tick × 4 + k` before draw `k`, so the state at tick N is a pure function of (world seed, weather history, N): identical on replay and independent of workers (Z2-D-019).

### Gusts and turbulence

Gusts are not stored; they are evaluated where wind is sampled, as `g(p, t) = m_fGustStrength × G(dot(p.xz, dir) / λ − t × f)`, where `G` is a smooth periodic function built from two sines of incommensurate frequencies (`0.5 + 0.35 sin(2πx) + 0.15 sin(2π × 2.37 x + 1.1)`), `λ` = 40 m the gust wavelength, `f` = `m_fGustFrequency`; turbulence adds `m_fTurbulence × (N(p/8, t) − 0.5)` with `N` the value noise of `EnvHash.slang` at a 0.5 s time lattice. The wind vector at `p` is `dir × speed × (1 + g + turbulence)`.

### Local wind sources

Stage 5: `Zenith_WindSourceComponent` entities (radial: explosions, helicopters; directional volumes: vents) add bounded contributions (`radius`, `strength`, `falloff`, `duration`): `Environment.Advance` writes at most 16 per world, nearest the environment anchor, into the environment blocks' `m_axLocalWindSources` (`Env_LocalWindSource`, `Zenith2/Docs/Design/ENV.md → "Render-world records"`) in phase 1, and ENV's GPU side exposes them through `Env_FrameEnvironment::m_xLocalWindSources`. In Stage 4 the global wind is the only source.

## Wind shader function

### Env_WindSample

```slang
// Zenith2/Shaders/Common/Environment.slang (every [FluxShared] struct of ENV; [Z2-N-REND-136])
[FluxShared] struct Env_WindParams          // 64 bytes, part of the render environment's wind block
{
	float2 m_xDirectionXZ;
	float m_fSpeed;
	float m_fGustStrength;
	float m_fGustFrequency;
	float m_fTurbulence;
	float m_fTimeSeconds;                   // this frame's wind time
	float m_fPrevTimeSeconds;               // the previous frame's, for velocity
	uint m_uLocalSourceCount;               // Stage 5
	uint m_auReserved[7];
};

// Zenith2/Shaders/Environment/Wind.slang (imports Common/Environment.slang)
float3 Env_WindSample(Env_WindParams xWind, float3 xPositionWS, float fTimeSeconds);   // m/s
```

The wind time is `simulationTick × tickSeconds + interpolationAlpha × tickSeconds` of the frame's render world (`Flux_RenderWorld::m_ulSimulationTick`, `m_fInterpolationAlpha`, `Zenith2/Docs/Rendering.md → "Purpose and scope"`), reduced modulo 3,600 s in `double` on the CPU before it is written as `float`, so it never loses precision in a long session; the modulo is a multiple of every period the functions above use (the incommensurate sine is evaluated on the reduced time, so its phase jumps once an hour, which is accepted).

### Env_FoliageWindOffset

```slang
// Zenith2/Shaders/Common/Environment.slang
[FluxShared] struct Env_FoliageWindParams   // 16 bytes, from the foliage type, in the material constants
{
	float m_fStiffness;
	float m_fTrunkBend;
	float m_fBranchFlutter;
	float m_fLeafFlutter;
};

// Offset of a vertex in world space, from the instance pivot, the vertex's height fraction (vertex colour R),
// branch weight (G) and leaf weight (B), for the given time.
float3 Env_FoliageWindOffset(Env_WindParams xWind, Env_FoliageWindParams xType, float3 xPivotWS, float3 xPositionWS,
	half3 xVertexWeights, uint uPhaseSeed, float fTimeSeconds);
```

| Component | Rule |
|---|---|
| trunk bend | the wind at the pivot, `v = Env_WindSample(pivot)`; `bend = m_fTrunkBend × |v| / (1 + m_fStiffness)`; the vertex is rotated about the pivot's horizontal axis perpendicular to `v` by `bend × heightFraction²`, preserving its distance from the pivot (a length-preserving bend, so trees do not stretch) |
| branch flutter | `m_fBranchFlutter × branchWeight × sin(2π (t × 0.9 + φ))` along the vertex's offset from the trunk axis, `φ` = `uPhaseSeed / 65536` |
| leaf flutter | `m_fLeafFlutter × leafWeight × 0.05 m × sin(2π (t × 3.1 + φ + dot(p, (0.37, 0.11, 0.53))))` along the vertex normal |

The function is the single source: every pass that positions a foliage vertex (depth prepass phases 1 and 2, the opaque pass, shadow depth, and from Stage 5 the G-buffer and the visibility-buffer raster and resolve) calls it through the vertex modifier, so a shadow and its caster never disagree.

### CPU mirror and tolerance

`Zenith_EvaluateWindCPU(const Zenith_WindState&, const Zenith_Vector3& xPositionWS, double dTimeSeconds)` in `Zenith_Wind.h` (`Zenith2/Docs/Design/ENV.md → "Zenith_Wind header"`) computes `Env_WindSample` on the CPU for gameplay (particles through VFX, cloth through PHYS, sails, audio wind intensity). Because `sin` differs between CPU libraries and GPU hardware, agreement is a tolerance, not bit equality: `|v_GPU − v_CPU| ≤ 1e-3 × max(1, |v_CPU|)` m/s on the test grid (`Test_Wind_GpuMatchesCpuMirror`). No simulation state depends on the GPU value.

## Foliage vertex modifier

### Material_Foliage

Foliage materials are the engine material `Material_Foliage` (`Zenith2/Shaders/Materials/Foliage.slang`): the standard surface model with a two-sided translucency term for leaves, domain `alphaTest` for leaf cards (drawn after opaque in depth-writing passes, the only pipelines that discard, [Z2-N-REND-249]) and `opaque` for trunks and rocks, `twoSided` only for leaf sections ([Z2-N-REND-923]), with a vertex modifier that applies `Env_FoliageWindOffset` in object space before the instance transform's translation.

<!-- assumption: pending RPROD, GEOM -->
Rendering's material model is a surface function alone (`Zenith2/Docs/Rendering.md → "Materials"`). ENV needs an optional vertex-stage hook in `Flux_MaterialType`: a material type declares `static const bool bHAS_VERTEX_MODIFIER = true` and `static float3 ModifyPositionWS(Flux_VertexModifierInput xIn, Params xParams, float fTimeSeconds)`, where `Flux_VertexModifierInput` carries the instance's pivot (the local-to-world translation), the world position before modification, the vertex colour, UV0, the instance custom data and the frame's `Env_FrameEnvironment` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`); every mesh vertex program calls it for the current frame and, where velocity is written, again with the previous frame's time and the previous transform. The hook is a material-level permutation, so a material without it costs nothing. RPROD confirms that its mesh programs position vertices through GEOM's mesh vertex function, which applies the hook (row RP20); the hook itself, UV0 and the frame environment in its input are the core-doc insertion X127 (1), extended by rows EN1 and EN18. `Material_Water`, `Material_Foliage` and `Material_DetailFoliage` are its first users.
<!-- /assumption -->

### Previous-frame evaluation for velocity

The prepass writes velocity from the current and previous positions of each vertex ([Z2-N-REND-009] resets only teleported instances); for a foliage vertex both are wind-displaced, evaluated with `m_fTimeSeconds` and `m_fPrevTimeSeconds` of `Env_WindParams`, so moving leaves get correct motion vectors and TAA does not smear them.

### Shadow and depth pass agreement

The shadow views of a frame evaluate the modifier with the same `Env_WindParams` and the same time as the main view, so a leaf's shadow is cast by the same displaced geometry the camera sees; `Test_FoliageWind_ShadowMatchesMainDepth` renders a single swaying tree and compares the shadow-view depth of its silhouette with a reference built from the main view's positions.
