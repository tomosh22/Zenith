# AI design: tool-side and internal declarations

Section file of `Zenith2/Docs/Design/AI.md`. It declares the two surfaces the public headers of `Zenith2/Docs/Design/AI/PublicAPI.md` do not: the tool-side root header `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h` that ENV's importer, WORLD's partition cook, AI's cook step and the editor call, and every `ZenithAI/Internal/` type a ticket writes and another ticket or test calls (the module services, the script path consumer and result decoder, the navigation load records, the agent runtime record, the perception ledger, the search tile view, the save participant). Algorithms stay in `Zenith2/Docs/Design/AI/Navigation.md` and `Zenith2/Docs/Design/AI/Agents.md`; byte layouts in `Zenith2/Docs/Design/AI/Formats.md`.

## Scope of this file

- Every declaration carries its contract block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"); internal types carry one too, because the `ApiContractCheck` step reads the headers that tests include.
- Tool-side code lives in the static tool library `ZenithAuthoring` by the orchestrator's ruling Q84 (a) (`Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs"): AI's sources in `Zenith2/Tools/Authoring/Internal/AI/`, its one public root header `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`, included as `#include "Authoring/Zenith_AuthoringAI.h"`. The library never names a ZenithCook or ZenithEditor type (core-doc item X301), so the cook-step registration `Zenith_RegisterAICookSteps` stays in ZenithCook (`Zenith2/Tools/ZenithCook/Internal/AI/Zenith_AICookRegistration.h`), and document types register only through `Zenith_RegisterAIDocumentTypes`, never from `Zenith_Module_AI_RegisterTypes` (ZenithAI has no ZenithDocument edge, the ANIM precedent).
- WORLD's two types `Zenith_WorldCellBounds` and `Zenith_WorldCellContentWriter`, and its product record `Zenith_WorldProduct`, are adopted unchanged (row WD11); `Zenith_ImportProduct` is ZenithCook's (`Tools/ZenithCook/Internal/Import/Zenith_Importer.h`), so the manifest view takes WORLD's `Zenith_WorldProduct`, whose three fields are the same.

## Zenith_AuthoringAI header

<!-- assumption: pending ARCH, WORLD, ENV (core-doc items X257, X301; rows AI3, AI11, AI12, WD11) -->
```
Zenith2/Tools/Authoring/
	Zenith_AuthoringAI.h                      AI's root header (below); ZenithAuthoring's DEPENDS gains ZenithAI in AI's first move (X301's rule)
	Internal/AI/
		Zenith_AINavSourceIntermediate.cpp    the ZNSG writer and reader
		Zenith_AIBakeInputs.cpp               tile and volume input gathering
		Zenith_AICellContent.cpp              the manifest view and the per-cell contribution
		Zenith_AINavigationDocuments.cpp      the two document types, their canonical schemas and migration chains
	Tests/AI/                                 Zenith_AIAuthoring_Tests.cpp (ZenithAuthoring_Tests; `Zenith2/Docs/Design/AI/Tests.md` → "Test targets")
```
<!-- /assumption -->

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAI.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_JsonDocument.h"               // Zenith_JsonDocument, Zenith_JsonNode
#include "ZenithDocument/Zenith_MigrationRegistry.h"      // Zenith_MigrationRegistry, Zenith_DocumentError
#include "ZenithScene/Zenith_SceneData.h"                 // Zenith_SceneData (a decoded cooked scene, SCENE/SceneAPI.md "Zenith_SceneData header")
#include "ZenithDocument/Zenith_DocumentMigrator.h"       // Zenith_OpenedDocument
#include "ZenithAssets/Zenith_AssetTypes.h"               // Zenith_AssetGUID
#include "ZenithEnvironment/Zenith_EnvironmentGeneration.h"   // Zenith_NavigationGeometry, Zenith_WaterBodyShape (ENV)
#include "ZenithAI/Zenith_NavBuild.h"
#include "ZenithAI/Zenith_NavVolume.h"
#include "Authoring/Zenith_AuthoringWorld.h"              // Zenith_WorldCellBounds, Zenith_WorldCellContentWriter, Zenith_WorldProduct

/// @contract value Zenith_AINavSourceView
/// Ownership: Plain value whose spans borrow the ZNSG bytes passed to Zenith_AIReadNavSourceIntermediate.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Invalid when those bytes are released.
/// Units: Tile-local metres (relative to m_adTileOrigin).
/// Errors: None.
/// Version: since Z2 Stage 4. The layout is Formats.md "Navigation source intermediate".
/// @endcontract
struct Zenith_AINavSourceView
{
	sint32 m_iEnvTileX = 0;
	sint32 m_iEnvTileZ = 0;
	double m_adTileOrigin[3] = {};
	Zenith_Span<const Zenith_Vector3> m_axVertices;
	Zenith_Span<const uint32> m_auIndices;      // three per triangle
	Zenith_Span<const uint8> m_auAreas;         // one Zenith_NavigationArea (0 … 4) per triangle
};

/// @contract interface Zenith_AIBakeInputSource
/// Ownership: Implemented by the caller of the gather functions — AI's cook step over its Zenith_CookContext (each call records
///   a cook input: OpenSourceDocument, ReadDependencyOutput), the editor over its open documents and the properties it has
///   read; borrowed for one call.
/// Thread affinity: The calling thread (a cook worker, or the editor's background operation).
/// Invalidation: A pointer or span a member returns is valid until the next call on the same source.
/// Units: Bytes; absolute metres for water bodies.
/// Errors: Any error is AI_ERROR_SOURCE_GEOMETRY_CORRUPT (unreadable property) or AI_ERROR_DOCUMENT_SCHEMA (unopenable scene)
///   with m_uContext the failing input's ordinal in the tile job; the gather function returns it unchanged.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_AIBakeInputSource
{
public:
	virtual ~Zenith_AIBakeInputSource() = default;
	// A scene document of the tile job (scene entry form 1), opened and migrated in memory.
	virtual Zenith_Result<const Zenith_OpenedDocument*, Zenith_AIError> OpenScene(const Zenith_AssetGUID& xScene) = 0;
	// A cooked scene product of the tile job (scene entry form 2, Formats.md "Tile bake intermediate"): the cook step reads the
	// product's cooked output with ReadDependencyOutput(xScene, "") and decodes it with SCENE's Zenith_DecodeCookedSceneFile into
	// xArena (SCENE/SceneAPI.md "Zenith_SceneCooker header"); the gather adds the entry's offset in binary64 (row WD23).
	virtual Zenith_Result<const Zenith_SceneData*, Zenith_AIError> ReadCookedScene(const Zenith_AssetGUID& xScene,
		Zenith_Arena& xArena) = 0;
	// Another asset's published property ("ai.navSource" of a navigation:<x>_<z> product, "ai.navMappings" of the manifest) or,
	// with an empty name, its cooked output (a collision shape's .zcol, decoded through PHYS's neutral-geometry decoder, row AI6).
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_AIError> ReadDependencyOutput(const Zenith_AssetGUID& xAsset,
		Zenith_StringView strProperty) = 0;
	// The water bodies of the terrain that owns a navigation:<x>_<z> product (ENV's Zenith_ListWaterBodiesForNavigation, row AI3).
	virtual Zenith_Result<Zenith_Span<const Zenith_WaterBodyShape>, Zenith_AIError> ListWaterBodies(
		const Zenith_AssetGUID& xNavigationProduct, Zenith_Arena& xArena) = 0;
};

/// @contract interface Zenith_AINavigationProductSink
/// Ownership: Implemented by the caller of Zenith_AIDeclareNavigationProducts — AI's importer ai.navigation over its
///   Zenith_ImportContext (DeclareProduct, WriteIntermediate), WORLD's partition over its plan's recording sink (row WD23);
///   borrowed for one call.
/// Thread affinity: The calling thread.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: A refusal is returned as AI_ERROR_DOCUMENT_SCHEMA with m_uContext the product's ordinal; the declaration stops.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_AINavigationProductSink
{
public:
	virtual ~Zenith_AINavigationProductSink() = default;
	// One product (Formats.md "Navigation document schema": the manifest key, navTile:…, navVolume:…), in key order;
	// bPrimary is true only for the manifest declared under the key "navigation".
	virtual Zenith_Result<void, Zenith_AIError> DeclareProduct(Zenith_StringView strKey, uint16 uKind,
		bool bPrimary) = 0;
	// An intermediate blob of the product last declared: "ai.navTileJob" (tile and volume products) or "ai.navMappings" (the manifest).
	virtual Zenith_Result<void, Zenith_AIError> AttachIntermediate(Zenith_StringView strName,
		Zenith_ByteSpan xBytes) = 0;
};

/// @contract value Zenith_AINavManifestTile, Zenith_AINavManifestVolume, Zenith_AINavigationManifestView
/// Ownership: Plain values whose spans and string views borrow the arena passed to Zenith_AIMakeNavigationManifestView.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: Invalid after that arena is reset.
/// Units: Absolute metres in double.
/// Errors: None.
/// Version: since Z2 Stage 4. Sorted: tiles by (class name bytes, layer, tile x, tile z), volumes by (class name bytes, index).
/// @endcontract
struct Zenith_AINavManifestTile
{
	Zenith_StringView m_strClass;
	Zenith_AssetGUID m_xAsset;
	sint32 m_iTileX = 0;
	sint32 m_iTileZ = 0;
	uint8 m_uLayer = 0;
	uint8 m_auReserved[1] = {};
	uint16 m_uTileSize = 0;                     // 16, 32, 64 or 128
	double m_dMinX = 0.0;                        // tile x × tile size
	double m_dMinZ = 0.0;
};
struct Zenith_AINavManifestVolume
{
	Zenith_StringView m_strClass;
	Zenith_AssetGUID m_xAsset;
	uint32 m_uIndex = 0;                        // the region's index in the document's volumes array
	uint32 m_uReserved = 0;
	double m_adMin[3] = {};                      // the region's minimum corner, from the key's millimetres / 1000
};
struct Zenith_AINavigationManifestView
{
	Zenith_Span<const Zenith_AINavManifestTile> m_axTiles;
	Zenith_Span<const Zenith_AINavManifestVolume> m_axVolumes;
};

/// @contract functions Zenith_AuthoringAI
/// Ownership: Results borrow the caller's arena; the registration adds two document types to the caller's registry.
/// Thread affinity: The caller's thread (a cook worker, the editor's main thread or background operation); every function
///   touches only its arguments.
/// Invalidation: As each result's contract.
/// Units: As each function's comment.
/// Errors: Zenith_AIError for every content error (never an assert: every input is untrusted); Zenith_DocumentError from
///   the registration unchanged.
/// Version: since Z2 Stage 4. Tool-side; ZenithAuthoring is outside every Shipping link closure.
/// @endcontract
// ENV's importer stage Environment.Navigation: geometry in world metres, converted to tile-local (v − origin in binary64,
// narrowed last); refuses more than 263,169 vertices or 524,288 triangles, an index ≥ the vertex count or an area > 4.
Zenith_Result<Zenith_ByteSpan, Zenith_AIError> Zenith_AIWriteNavSourceIntermediate(
	const Zenith_NavigationGeometry& xGeometry, const double adTileOrigin[3], Zenith_Arena& xArena);
// The reader of the same bytes; every check of Formats.md "Navigation source intermediate" (AI_ERROR_SOURCE_GEOMETRY_CORRUPT).
Zenith_Result<void, Zenith_AIError> Zenith_AIReadNavSourceIntermediate(Zenith_ByteSpan xBytes,
	Zenith_AINavSourceView& xOut);
// A TILE job (the ZNTJ blob, Formats.md "Tile bake intermediate") to the input of Zenith_NavBuildTile, by
// Navigation.md "Bake inputs": terrain triangles from each source product, static colliders and blockers from each scene
// (entity world transforms composed down the hierarchy, SCENE's walk, row AI5), area volumes, mappings from the manifest;
// sorted by (source kind, source persistent ID, triangle index).
Zenith_Result<Zenith_NavBuildTileInput, Zenith_AIError> Zenith_AIGatherBakeInputs(Zenith_ByteSpan xTileJob,
	const Zenith_NavClassParams& xParams, Zenith_AIBakeInputSource& xSource, Zenith_Arena& xArena);
// A VOLUME job to the input of Zenith_NavBuildVolume (Volumes.md "Volume bake" step 1): every triangle regardless of slope,
// the job's water boxes and the water bodies of each source product's terrain.
Zenith_Result<Zenith_NavVolumeBuildInput, Zenith_AIError> Zenith_AIGatherVolumeBakeInputs(
	Zenith_ByteSpan xTileJob, Zenith_AIBakeInputSource& xSource, Zenith_Arena& xArena);
// The product declaration of a validated navigation document (Formats.md "Navigation document schema", the paragraph that
// begins "The product declaration"): the manifest under strManifestKey ("navigation" for ai.navigation, primary;
// "navigation/<x>_<z>" for WORLD's derived documents, non-primary), its ai.navMappings blob, then every navTile: and navVolume:
// product in key order with its ai.navTileJob blob (Formats.md "Tile bake intermediate"; a scene entry's GUID and offset
// copied into the job as form 2, a path resolved to its GUID as form 1). xRoot is the document's root object in xDocument;
// a schema error is AI_ERROR_DOCUMENT_SCHEMA naming the member path. Byte-identical blobs for identical inputs.
Zenith_Result<void, Zenith_AIError> Zenith_AIDeclareNavigationProducts(const Zenith_JsonDocument& xDocument,
	Zenith_JsonNode xRoot, Zenith_StringView strManifestKey, Zenith_AINavigationProductSink& xSink, Zenith_Arena& xArena);
// WORLD's derived navigation documents' products to the view; manifest keys ("navigation", "navigation/…") and every key
// other than navTile:… and navVolume:… are ignored; a navTile or navVolume key that does not parse, or two products with one key, is AI_ERROR_DOCUMENT_SCHEMA
// with m_uContext the product's index.
Zenith_Result<Zenith_AINavigationManifestView, Zenith_AIError> Zenith_AIMakeNavigationManifestView(
	Zenith_Span<const Zenith_WorldProduct> axProducts, Zenith_Arena& xArena);
// One NAVIGATION cell's contribution (algorithm below); a writer refusal is AI_ERROR_INVALID_DESC with m_uDetail 1 and
// m_uContext the entity's ordinal, after which WORLD fails the cell.
Zenith_Result<void, Zenith_AIError> Zenith_AIWriteCellNavigationContent(const Zenith_AINavigationManifestView& xView,
	const Zenith_WorldCellBounds& xCell, Zenith_WorldCellContentWriter& xWriter);
// Zenith.NavigationSettingsDocument and Zenith.NavigationDocument, schema version 1 each, with their canonical schemas and
// migration chains (AI.md "Document types and migration"); called by ZenithCook's registration and by the editor.
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterAIDocumentTypes(Zenith_MigrationRegistry& xRegistry);
```

**Manifest view.** For each product in input order: `navTile:<class>/<tileSize>/<tx>_<tz>_<layer>` gives a tile entry (`<class>` matches `^[A-Za-z][A-Za-z0-9_]{0,30}$`, `<tileSize>` ∈ {16, 32, 64, 128}, `<tx>`, `<tz>` signed decimal in −32,768 … 32,767, `<layer>` in 0 … 31, minimum corner `(tx × tileSize, tz × tileSize)`); `navVolume:<class>/<index>/<xmm>_<ymm>_<zmm>` a volume entry (index 0 … 255, millimetres signed decimal within ±2^53, minimum `mm / 1000` in binary64); then both arrays are sorted by their contract orders with `Zenith_Sort` (keys unique, so the order is unique). O(n log n).

**Cell content.** For one cell `xCell` (half-open `[m_dMinX, m_dMaxX) × [m_dMinZ, m_dMaxZ)`):

1. Tiles: the view's tiles whose `(m_dMinX, m_dMinZ)` lies in the cell, in view order; grouped by class; each class's tiles split into groups of 16 in order. Each group is one entity: ID `Zenith_DeriveAuthoringID(xCell.m_xCellScene, L)` (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation") with L the first 16 bytes of `Zenith_Sha256Digest("ZAIN" ‖ "tiles/" ‖ class ‖ "/" ‖ group index in decimal)` read as SCENE's derivation reads a local ID; name `nav.<class>.<group>`; local position zero and identity rotation (the cell anchor; the tiles carry absolute origins); one component `Zenith.AI.NavMeshTiles` version 1 in the JSON field-set form `{ "Tiles": [ "<GUID>", …, null to 16 entries ], "Count": n }`.
2. Volumes: the view's volumes whose minimum (x, z) lies in the cell, in view order: one entity each, L from `"ZAIN" ‖ "volume/" ‖ class ‖ "/" ‖ index`, name `nav.<class>.volume<index>`, component `Zenith.AI.NavVolumeRef` version 1 `{ "Volume": "<GUID>" }`.
3. `BeginEntity`, `AddComponent`, `EndEntity` per entity in that order; WORLD appends them after the cell's authored content. The output is a pure function of the view and the cell (`Test_NavCook_CellContentRecords`).

`Zenith_NavVolumeBuildInput` and the volume builder `Zenith_NavBuildVolume` are public ZenithAI declarations beside `Zenith_NavVolumeSearch` (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavVolume header").

ZenithCook's part, outside ZenithAuthoring: `void Zenith_RegisterAICookSteps(Zenith_CookStepRegistry& xSteps, Zenith_ImporterRegistry& xImporters)` (`Zenith2/Tools/ZenithCook/Internal/AI/Zenith_AICookRegistration.h`) registers the importers `ai.navSettings` and `ai.navigation` and the kind-15 step `ai.navMesh`; the step implements `Zenith_AIBakeInputSource` over its `Zenith_CookContext` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext") and calls the functions above and `Zenith_NavBuildTile`.

## Internal declarations

### Zenith_AIWorldState header

`Zenith2/Engine/ZenithAI/Internal/Zenith_AIWorldState.h` declares one state object per attached world, created by `AttachWorld` step 2 and destroyed by `DetachWorld` after every resource is removed. Each member below that is a world resource is registered with `AddWorldResource` under the name given (owning phase `PHASE_AI_NAVIGATION`).

```cpp
/// @contract value Zenith_AINavGateEntry, Zenith_AINavGateHold
/// Ownership: Elements of Zenith_AIWorldState's m_axGate and m_axGateHolds.
/// Thread affinity: Written by AI.NavIngest's own job (step 2) and, for holds, by the test on the main thread with no frame running.
/// Invalidation: A gate entry is removed at its release or when its load is abandoned.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 4. The algorithm is Navigation.md "Completion gate".
/// @endcontract
struct Zenith_AINavGateEntry
{
	uint64 m_ulCompletionID = 0;             // the drained record's m_ulUserTag
	Zenith_OperationStatus m_eStatus;         // SUCCEEDED or FAILED (a CANCELLED record belongs to an abandoned load and never enters)
	Zenith_AssetError m_xError;               // FAILED only
};
struct Zenith_AINavGateHold { uint64 m_ulCompletionID = 0; uint64 m_ulReleaseTick = 0; };

/// @contract type Zenith_AIWorldState
/// Ownership: Owned by the module instance's world table (Zenith_AIWorldTable), one per attached world; its resources are
///   borrowed by the world's schedule between AddWorldResource and the removal in DetachWorld.
/// Thread affinity: Each member as its resource's declared access; creation and destruction MAIN_THREAD with no frame running.
/// Invalidation: Destroyed by DetachWorld.
/// Units: None.
/// Errors: Create returns AI_ERROR_OUT_OF_MEMORY (attach step 2 fault point).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIWorldState
{
	uint16 m_uWorldTag = 0;
	Zenith_WorldPurpose m_ePurpose = WORLD_PURPOSE_PLAY;        // from GetWorldPurpose (X243 (b))
	Zenith_AIWorldResource m_xWorld;          // "Zenith.AI.World": nav mesh, volume set, link table (slots with salts), load
	                                          // records, join list, supplier records, obstacle applied records, carve queue,
	                                          // carved records, m_uAppliedRebaseCount, m_adAppliedOrigin[3]
	Zenith_AIBlackboardStore m_xBlackboards;  // "Zenith.AI.Blackboards"
	Zenith_AICrowdGrid m_xCrowdGrid;          // "Zenith.AI.CrowdGrid": the sorted 48-byte entries and the cell index
	Zenith_AIStimulusGrid m_xStimulusGrid;    // "Zenith.AI.StimulusGrid": the sorted 40-byte entries and the cell index
	Zenith_AIPathService m_xPathService;      // "Zenith.AI.PathService": queue, descriptor table, active set, searches, result slots
	Zenith_AIAgentRuntimeTable m_xAgents;     // "Zenith.AI.AgentRuntime": the records of "Agent runtime record"
	Zenith_AIPerceptionLedger m_xLedger;      // "Zenith.AI.PerceptionLedger": "Perception ledger"
	Zenith_AINoiseBuffer m_xNoises;           // "Zenith.AI.NoiseBuffer": stamped entries, at most 2 × ai.perception.maxNoisesPerTick
	Zenith_AIPathInbox m_xInbox;              // module state, written only on the world's merge chain ("Path request consumer")
	Zenith_AITraceStaging m_xTrace;           // development builds: one 1,024-record buffer per VM slot (Scripting.md "Debug trace helpers")
	Zenith_Owned<Zenith_AssetLoadCompletionQueue> m_xLoads;          // "ai.loads.w<tag>", capacity ai.nav.maxLoadsInFlight
	Zenith_Owned<Zenith_AssetLoadCompletionQueue> m_xRestoreLoads;   // "ai.restore.w<tag>", only during a restore; capacity
	                                          // min(65,536, ai.nav.maxTiles + 256 + ai.nav.maxLoadsInFlight + 1) (Agents.md "Restore" step 4)
	Zenith_Array<Zenith_AINavGateEntry> m_axGate;   // the completion gate (Navigation.md "Completion gate"): capacity
	                                          // ai.nav.maxLoadsInFlight, reserved at attach, sorted by completion ID; outside the save and the hash
	Zenith_Owned<Zenith_AISaveParticipant> m_xParticipant;           // play worlds only
	uint32 m_uLinkCapacityOverride = 0;      // tests only (never set by the module): a per-tile link capacity, 0 = 4 × polygons
	uint32 m_uListenerSuppressMask = 0;      // tests only: bit LIFECYCLE_EVENT_ORIGIN_SHIFTED skips the listener's shift
	Zenith_Array<Zenith_AINavGateHold> m_axGateHolds;   // tests only (never set by the module): completion ID → release tick,
	                                          // sorted by completion ID; a hold whose record is gone is ignored
	uint32 m_uGateFixedDelay = 0;            // tests only: 0 off; else release at the load's issue tick + this many ticks
	uint32 m_uGateLateReleases = 0;          // test observation only (no module code reads it): entries admitted after their release tick
};
```

### Module services

```cpp
/// @contract value Zenith_AIConfig
/// Ownership: Plain value filled by Zenith_AIReadConfig at start step 1 from the ai.* keys (PublicAPI.md "Configuration keys").
/// Thread affinity: IMMUTABLE after start; members non-const: its producer default-constructs it and fills it member by
///   member.
/// Invalidation: None.
/// Units: As each key.
/// Errors: Zenith_AIReadConfig returns MODULE_ERROR_REGISTRATION for a malformed value or a violated cross-key rule, logging the key.
/// Version: since Z2 Stage 4. One member per key, in the key table's order.
/// @endcontract
struct Zenith_AIConfig
{
	uint32 m_uMaxSearchNodes, m_uExpansionsPerRequestPerTick, m_uMaxActiveRequests, m_uMaxQueuedRequests, m_uMaxTicksPerRequest;
	uint32 m_uMaxTileRebuildsPerTick, m_uMaxTiles, m_uMaxLoadsInFlight, m_uMaxAgents;
	float m_fRetargetDistance, m_fStuckSeconds;
	uint32 m_uOptimizeIntervalTicks, m_uMaxNeighbours;
	float m_fNeighbourRadius;
	float m_afTierDistances[3];
	uint32 m_auTierIntervals[4];
	uint32 m_uTeamCount;
	Zenith_StringID m_axTeamNames[uAI_MAX_TEAMS];
	uint32 m_uMaxRaysPerTick, m_uMaxNoisesPerTick;
	uint32 m_uDebugViews;                    // development builds; 0 in Shipping
	Zenith_PersistentID m_xSelectedAgent;     // development builds; null in Shipping
};

/// @contract type Zenith_AIModuleServices
/// Ownership: Owned by the module instance (Internal/Module/Zenith_AIModule.h); every pointer borrows an object that outlives it.
/// Thread affinity: Built on the main thread at start; IMMUTABLE afterwards except m_pxWorlds' and m_pxNavHandlers' tables, written on the main
///   thread at attach and detach with no frame running and read by the consumer on merge nodes.
/// Invalidation: Destroyed at module shutdown, after every world detached.
/// Units: None.
/// Errors: None.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIModuleServices
{
	Zenith_AIConfig m_xConfig;
	Zenith_AssetRegistry* m_pxAssets = nullptr;
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_DebugDraw* m_pxDebugDraw = nullptr;                 // &GetDebugDraw() at start step 1; never null (present in every build, X245 (c))
	Zenith_AIPathRequestConsumer* m_pxPathConsumer = nullptr;  // registered with SetScriptSideEffectConsumer
	Zenith_AIWorldTable* m_pxWorlds = nullptr;                 // world tag → Zenith_AIWorldState*, sorted array, binary search
	Zenith_AINavHandlerTable* m_pxNavHandlers = nullptr;       // world tag → its navigation publication handler ("Navigation load records")
	uint32 m_uScriptPathCapacity = 0;                         // P: the pathfinding value of Z2-B-SCRIPT-026 (AI.md "Assumed interfaces")
};
```

`Zenith_AIWorldTable` is a `Zenith_Array<Zenith_AIWorldEntry>` of `{ uint16 tag; Zenith_AIWorldState* state; }` sorted by tag, at most 16 worlds; a lookup of an absent tag returns null.

### Path request consumer

```cpp
/// @contract type Zenith_AIPathRequestConsumer
/// Ownership: Owned by the module instance; registered for SIDE_EFFECT_KIND_ASYNC_REQUEST through the start context and called
///   by ZenithRuntime's multiplexer for byte-0 ASYNC_SOURCE_PATHFINDING records.
/// Thread affinity: Consume runs on the committing merge node of the world named by uWorldTag; one merge node of a world runs at a
///   time, so the world's inbox needs no lock.
/// Invalidation: None.
/// Units: Completion IDs; the 64-byte payload of Formats.md "Script request payload".
/// Errors: A payload of another size or source byte, or an unknown world tag, is an engine defect (the record came from AI's
///   own binding): ZENITH_ASSERT in development builds; in release the request is completed CANCELLED through the sink so
///   its ID completes exactly once (X255), counted ai.path.consumer_defects_delta.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_AIPathRequestConsumer final : public Zenith_ScriptSideEffectConsumer
{
public:
	explicit Zenith_AIPathRequestConsumer(Zenith_AIWorldTable& xWorlds);
	void Consume(uint16 uWorldTag, Zenith_SideEffectKind eKind, uint64 ulCompletionID, Zenith_ByteSpan xPayload) override;
};

/// @contract type Zenith_AIPathInbox
/// Ownership: A member of Zenith_AIWorldState; capacity P (m_uScriptPathCapacity), reserved at attach.
/// Thread affinity: Append on the world's merge chain; Drain by AI.NavRequests step 1; capture and restore on the main thread.
/// Invalidation: Drain empties it.
/// Units: None.
/// Errors: Append on a full inbox cannot happen while SCRIPT holds the Z2-B-SCRIPT-026 bound (the consumer's defect path above).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIPathInboxEntry
{
	uint64 m_ulCompletionID = 0;
	uint64 m_ulCommitTick = 0;
	uint8 m_auPayload[64] = {};
};
```

`Consume` appends `{ ulCompletionID, the world's current tick, the 64 payload bytes }`; entries are appended in commit (merge) order, which for one source is completion-ID order.

### Path result decoder

```cpp
/// @contract function Zenith_AIDecodePathResult
/// Ownership: Fills xOut; the corners field's struct pointer borrows xPayload (Scripting.md "Path request payload and result").
/// Thread affinity: The thread running ticket:Result() (a behaviour invocation).
/// Invalidation: xOut is valid while the published result is held by the result table.
/// Units: As Formats.md "Script result payloads".
/// Errors: Returns false for a publication kind other than PUBLICATION_KIND_GENERIC_RECOMPUTED, a payload size other than
///   uAI_PATH_RESULT_BYTES, a status outside Zenith_NavPathStatus or a corner count above 32.
/// Version: since Z2 Stage 4. Signature: SCRIPT's Zenith_ScriptAsyncResultDecoder.
/// @endcontract
bool Zenith_AIDecodePathResult(uint8 uPublicationKind, Zenith_ByteSpan xPayload,
	Zenith_ScriptResultFields& xOut);
```

It writes eight fields (SCRIPT's `uSCRIPT_ASYNC_RESULT_MAX_FIELDS`): `status` (`m_szStaticString` one of "Complete", "Partial", "NoStart", "NoEnd", "NoNavMesh", "InvalidRequest", "Cancelled", "Truncated"), `limit` (`m_szStaticString` one of "None", "NodePool", "TickCap", "Corridor", from payload byte 2), `cornerCount`, `corners` (`SCRIPT_VALUE_KIND_STRUCT`, type `Zenith.AI.PathCorners`, pointer `xPayload.GetData() + 32`), `offMeshMask`, `length`, `cost`, `finalPoint`; a limit byte above 3 makes it return false.

### Navigation load records

```cpp
/// @contract value Zenith_AINavLoadRecord
/// Ownership: Owned by Zenith.AI.World's load table (sorted by completion ID, at most ai.nav.maxLoadsInFlight).
/// Thread affinity: Written by AI.NavIngest's node and merge node, by the publication handler at tick-boundary step 1, and by
///   the save participant out of tick; never two at once.
/// Invalidation: Removed at its publication.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The state machine is Navigation.md "Navigation asset requests".
/// @endcontract
enum Zenith_AINavLoadState : uint8
{
	AI_NAV_LOAD_STATE_ISSUED = 0,
	AI_NAV_LOAD_STATE_PARKED_OK = 1,
	AI_NAV_LOAD_STATE_PARKED_FAILED = 2,
	AI_NAV_LOAD_STATE_ABANDONED = 3,
};
struct Zenith_AINavLoadRecord
{
	uint64 m_ulCompletionID = 0;
	Zenith_AssetGUID m_xAsset;
	uint64 m_ulRequesterSimulationID = 0;
	uint8 m_uSupplierSlot = 0;                  // the requester's supplier slot, 0 … 15 (Navigation.md "Supplier records")
	uint8 m_auReserved0[7] = {};
	Zenith_AnyAssetHandle m_xHandle;            // the requested handle (one reference until publication)
	Zenith_OperationHandle m_xOperation;        // RequestLoad's operation; cancelled on abandonment
	uint8 m_eKind = 0;                          // 1 MANIFEST, 2 TILE, 3 VOLUME
	Zenith_AINavLoadState m_eState = AI_NAV_LOAD_STATE_ISSUED;
	uint8 m_auReserved[6] = {};
	uint64 m_ulIssueTick = 0;                   // the tick its merge node (or a restore) issued it; read only by the completion
	                                             // gate's test fixed delay, so outside the save and the canonical hash
};

/// @contract value Zenith_AINavJoinEntry
/// Ownership: An element of Zenith.AI.World's join list, pushed by the publication handler, consumed by AI.NavIngest step 1.
/// Thread affinity: As Zenith_AINavLoadRecord.
/// Invalidation: Consumed by the next phase 7.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavJoinEntry
{
	Zenith_AssetHandle<Zenith_NavMeshAsset> m_xAsset;   // resolved by the handler (a publication step may Resolve)
	uint64 m_ulRequesterSimulationID = 0;
	uint8 m_eKind = 0;
	uint8 m_uSupplierSlot = 0;                         // a manifest's entries carry the manifest's slot
	uint8 m_auReserved[6] = {};
};

/// @contract value Zenith_AINavSupplier
/// Ownership: An element of Zenith.AI.World's supplier table, sorted by simulation ID, one per qualifying entity
///   (Navigation.md "Supplier records"); capacity uAI_NAV_MAX_TILES_PER_WORLD.
/// Thread affinity: Written by AI.NavIngest step 3, the join step and the publication handler; read by the save participant and
///   ComputeStateHash with no frame running.
/// Invalidation: Removed with its entity's qualification.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The save form is Formats.md "Save section layout" list 4.
/// @endcontract
enum Zenith_AINavSupplierState : uint8
{
	AI_NAV_SUPPLIER_NEED = 0,
	AI_NAV_SUPPLIER_IN_FLIGHT = 1,
	AI_NAV_SUPPLIER_JOINED = 2,
	AI_NAV_SUPPLIER_FAILED = 3,
};
struct Zenith_AINavSupplierSlot { Zenith_AssetGUID m_xAsset; Zenith_AINavSupplierState m_eState = AI_NAV_SUPPLIER_NEED; uint8 m_auReserved[7] = {}; };
struct Zenith_AINavSupplier
{
	uint64 m_ulSimulationID = 0;
	uint8 m_eKind = 0;                                 // 1 manifest, 2 tiles, 3 volume
	uint8 m_uSlotCount = 0;                            // 1, or 0 … 16 for kind 2
	uint8 m_auReserved[6] = {};
	Zenith_AINavSupplierSlot m_axSlots[16];             // property order; the first m_uSlotCount meaningful
};

/// @contract type Zenith_AINavPublicationHandler
/// Ownership: Owned by the module instance's Zenith_AINavHandlerTable (one entry per world tag ever attached, created at that
///   world's first AttachWorld step 3 with Zenith_TryNew, freed with the module instance), never by Zenith_AIWorldState:
///   MOD's handler contract requires a handler to outlive the world's table, which a swap may retire after DetachWorld
///   (Zenith2/Docs/Design/MOD/RuntimeSeams.md "Sink lifetime and retirement"). Registered with
///   Zenith_ModulePublicationSink::SetSourceHandler(ASYNC_SOURCE_NAVIGATION, …) at attach step 3.
/// Thread affinity: OnPublished on the world's tick-boundary node, step 1 (MOD's forwarder), once per publication of
///   ASYNC_SOURCE_NAVIGATION in completion-ID order, live and in replay; Bind and Unbind MAIN_THREAD at AttachWorld step 3 and
///   DetachWorld, with no frame of the world running.
/// Invalidation: After Unbind, OnPublished returns at once (a retired table never publishes, so this is a guard, not a path).
/// Units: Completion IDs.
/// Errors: None. A publication whose completion ID has no load record, or of another source kind, is an engine defect:
///   ZENITH_ASSERT, and the publication is ignored.
/// Reentrancy: NONE. It calls no sink member and reads or writes nothing through xWorld: it writes only AI's own module state
///   of the bound world (the load records, the join list and the supplier records of Zenith.AI.World, which no node of the
///   world is running to read at tick-boundary step 1) and calls Zenith_AssetRegistry::Resolve.
/// Version: since Z2 Stage 4. The rows it runs are Navigation.md "Navigation asset requests" (PARKED_OK, PARKED_FAILED and
///   ABANDONED "published").
/// @endcontract
class Zenith_AINavPublicationHandler final : public Zenith_ModulePublicationHandler
{
public:
	explicit Zenith_AINavPublicationHandler(Zenith_AIWorldState& xWorld);   // bound to xWorld
	void Bind(Zenith_AIWorldState& xWorld);     // a re-attach of the same world tag
	void Unbind();                               // DetachWorld, before the world state is destroyed
	void OnPublished(const Zenith_ModulePublication& xPublication, Zenith_TickBoundaryAccess& xWorld) override;
private:
	Zenith_AIWorldState* m_pxWorld = nullptr;             // null once unbound
};

// The module's handler table: { uint16 tag; Zenith_Owned<Zenith_AINavPublicationHandler> handler; } sorted by tag, grown on
// the main thread at AttachWorld (at most one entry per world tag, so ≤ 65,536 entries of 16 bytes plus 16 bytes of handler).
struct Zenith_AINavHandlerEntry { uint16 m_uWorldTag = 0; uint8 m_auReserved[6] = {}; Zenith_Owned<Zenith_AINavPublicationHandler> m_xHandler; };
using Zenith_AINavHandlerTable = Zenith_Array<Zenith_AINavHandlerEntry>;
```

`OnPublished` reads `xPublication.m_ulCompletionID`, finds the load record by binary search, and runs the row of "Navigation asset requests" for the record's state and the publication: a PARKED_OK record with `PUBLICATION_KIND_GENERIC_RECOMPUTED` resolves the asset, validates role and settings and pushes the join entries; a PARKED_FAILED record (`PUBLICATION_KIND_GENERIC_RECORDED`, 24 bytes) fails the supplier slot; an ABANDONED record (any payload, the zero-length abandoned payload included) is dropped; each record is released. `Zenith_AIModuleServices::m_pxNavHandlers` points at the table.

### Agent runtime record

```cpp
/// @contract value Zenith_AIAgentRuntime
/// Ownership: An element of Zenith.AI.AgentRuntime's dense array, sorted by simulation ID (Agents.md "Agent runtime table").
/// Thread affinity: Written by AI.Locomotion's sub-job owning the agent's chunk, then by its merge function; read by
///   AI.NavRequests (the previous version), AI.DebugDraw, the save participant and the origin-shift listener (sync point 1).
/// Invalidation: Removed with its agent.
/// Units: World-local metres, m/s, radians, ticks.
/// Errors: None.
/// Version: since Z2 Stage 4. 4,096 bytes; offsets are part of the contract (the save form is Formats.md "Save section layout").
/// @endcontract
struct Zenith_AIPendingCompletion { uint32 m_uSerial = 0; uint8 m_eResult = 0; uint8 m_auReserved[3] = {}; };
struct alignas(16) Zenith_AIAgentRuntime
{
	uint64 m_ulSimulationID;              //    0
	uint8 m_uClass;                       //    8  Zenith_NavClassIndex value
	uint8 m_eMedium;                      //    9
	uint8 m_ePlanState;                   //   10  0 none, 1 queued, 2 active, 3 installed
	uint8 m_eLinkState;                   //   11  Navigation.md "Link traversal state machine"
	uint8 m_ePausedFrom;                  //   12  the MoveStatus a PAUSE interrupted; 0 when not paused
	uint8 m_uFlags;                       //   13  bit 0 ragdoll, bit 1 root-motion agent, bit 2 not-root counted, bit 3 invalid counted
	uint8 m_uStrikes;                     //   14
	uint8 m_uPendingCompletionCount;      //   15  0 … 8
	uint32 m_uHandledMoveSerial;          //   16
	uint32 m_uEmittedCompletedSerial;     //   20  the serial of the last MoveCompleted an accepted buffer carried
	uint32 m_uCorridorCount;              //   24  0 … 256
	uint32 m_uVolumeCornerCount;          //   28  0 … 64
	Zenith_NavPolyRef m_xCurrentPoly;      //   32
	uint64 m_ulRandomCounter;             //   40  the counter of Zenith_RandomSource(world seed, AI source ID, simulation ID) (A7)
	Zenith_Vector3 m_xTarget;              //   48
	uint32 m_uCornerCount;                //   60  0 … 4
	Zenith_NavPolyRef m_xTargetPoly;       //   64
	uint64 m_ulSuspendedSinceTick;        //   72  0 when not SUSPENDED (the 600-tick FAILED rule)
	Zenith_NavCorner m_axCorners[4];       //   80  … 175
	Zenith_EntityHandle m_xLinkEntity;     //  176
	Zenith_Vector3 m_xLinkStart;           //  184
	Zenith_Vector3 m_xLinkEnd;             //  196
	uint32 m_uLinkElapsedTicks;           //  208
	uint32 m_uLinkDurationTicks;          //  212
	uint8 m_eLinkKind;                    //  216
	uint8 m_auReserved0[7];               //  217
	Zenith_Vector3 m_xStuckStart;          //  224
	float m_fStuckIntended;                //  236  root-motion agents: Σ |v_chosen| Δt over the window (Agents.md "Stuck detection")
	uint64 m_ulStuckStartTick;            //  240
	uint64 m_ulNextOptimiseTick;          //  248
	uint64 m_ulNextReplanTick;            //  256
	Zenith_Vector3 m_xLastDesiredVelocity; //  264  last ACCEPTED values (Agents.md "Locomotion output")
	float m_fLastDesiredYaw;               //  276
	uint32 m_uLastInputFlags;             //  280
	uint32 m_uLastWrittenMask;            //  284  bit 0 desired velocity, 1 yaw, 2 input flags, 3 transform, 4 applied velocity
	Zenith_Vector3 m_xLastAppliedVelocity; //  288
	float m_fLastRootMotionYaw;            //  300
	Zenith_AIPendingCompletion m_axPendingCompletions[8];   // 304 … 367
	Zenith_Vector3 m_xProposedDesiredVelocity;              // 368  this tick's recorded values, resolved by the merge function
	float m_fProposedYaw;                  //  380
	uint32 m_uProposedInputFlags;         //  384
	uint32 m_uProposedMask;               //  388
	uint8 m_uProposedCompletionCount;     //  392  completions this tick's buffer carries (from the front of the pending list)
	uint8 m_bContinuing;                  //  393  the corridor is a continuation (Agents.md "Path corridor")
	uint8 m_ePlanReason;                  //  394  of the queued or active plan: 0 NEW_MOVE, 1 CONTINUATION, 2 RETARGET, 3 REPLAN
	                                       //       (Agents.md "Agent state machine", continuation state and plan reasons)
	uint8 m_uReserved1a;                  //  395
	float m_fContinuationDistance;         //  396  |target − corridor end point| of the last continuation plan
	uint8 m_auReserved1[624];             //  400  … 1023, zero
	Zenith_NavPolyRef m_axCorridor[uAI_PATH_MAX_CORRIDOR_POLYS];   // 1024 … 3071
	Zenith_Vector3 m_axVolumeCorners[uAI_PATH_MAX_CORNERS];         // 3072 … 3839
	uint8 m_auReserved2[256];             // 3840 … 4095, zero
};
static_assert(sizeof(Zenith_AIAgentRuntime) == 4096);
```

`Zenith_AILocomotionMerge(Zenith_MergeContext&)` (Internal/Agents/Zenith_AILocomotion.cpp): for each buffer in base-key order, accept or reject it ("Merge functions" of `Zenith2/Docs/Design/AI/Agents.md` → "Frame-graph placement"); for the buffer's agent, accepted → copy the proposed fields into the last-written fields under `m_uProposedMask`, remove `m_uProposedCompletionCount` entries from the front of the pending list and set `m_uEmittedCompletedSerial` to the last removed serial; rejected → leave both; then clear the proposed fields. One buffer per agent per tick (the agent's entity transaction), so the mapping is the buffer's base key.

### Perception ledger

```cpp
/// @contract type Zenith_AIPerceptionLedger
/// Ownership: The world resource "Zenith.AI.PerceptionLedger" (Agents.md "Perception events"), a member of Zenith_AIWorldState.
/// Thread affinity: Read by AI.Perception's sub-jobs (each its perceivers' entries); written by Zenith_AIPerceptionMerge; read
///   by the save participant and ComputeStateHash with no frame running.
/// Invalidation: Entries are removed with their perceiver.
/// Units: Ticks.
/// Errors: None; capacity is one entry per entity with Zenith.AI.PerceptionState, grown at the merge (an allocation failure
///   keeps the previous ledger and counts ai.perception.ledger_full_delta, events then re-emit next tick).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AILedgerTarget { uint64 m_ulTargetSimulationID = 0; uint8 m_eEmittedLevel = 0; uint8 m_auReserved[7] = {}; };
struct Zenith_AILedgerForget { Zenith_EntityHandle m_xTarget; uint64 m_ulTargetSimulationID = 0; uint8 m_uReason = 0; uint8 m_auReserved[7] = {}; };
struct Zenith_AILedgerEntry                // 32 + 16 × 16 + 16 × 24 = 672 bytes
{
	uint64 m_ulPerceiverSimulationID = 0;
	uint8 m_uTargetCount = 0;             // ≤ uAI_PERCEPTION_MAX_MEMORY
	uint8 m_uForgetCount = 0;             // ≤ 16
	uint8 m_uProposedForgetBase = 0;      // the pending forgets this tick's buffer emitted
	uint8 m_auReserved[21] = {};
	Zenith_AILedgerTarget m_axTargets[uAI_PERCEPTION_MAX_MEMORY];   // ascending target simulation ID
	Zenith_AILedgerForget m_axForgets[16];                          // ascending (target simulation ID, reason)
};
class Zenith_AIPerceptionLedger
{
public:
	const Zenith_AILedgerEntry* Find(uint64 ulPerceiverSimulationID) const;   // binary search
	uint32 GetEntryCount() const;
	static const Zenith_TypeID xTYPE_ID;                                                              // "Zenith.AI.PerceptionLedger"
};
```

Each `AI.Perception` sub-job writes its perceivers' **proposals** into a per-sub-job arena (the new emitted levels and the full pending-forget list it emitted), keyed by perceiver; `Zenith_AIPerceptionMerge` applies a proposal to the ledger entry for an accepted buffer and, for a rejected one, appends only that tick's new removals to the entry's forgets (Agents.md "Perception events" step 4).

### Obstacle and carve records

```cpp
/// @contract value Zenith_AIObstacleApplied, Zenith_AICarvedTile
/// Ownership: Elements of Zenith.AI.World's obstacle table (sorted by simulation ID, at most uAI_NAV_MAX_OBSTACLES_PER_WORLD)
///   and carved table (sorted by tile key Pack(), at most ai.nav.maxTiles); a carved record's shapes live in the carve
///   state's shape array, contiguous per record.
/// Thread affinity: Written by AI.NavIngest (Navigation.md "Mark procedure", "Carve procedure") and by the origin-shift
///   listener at sync point 1 (applied positions only); read by the save participant and ComputeStateHash with no frame running.
/// Invalidation: An applied record is removed when its obstacle disappears; a carved record when its tile returns to the baked
///   section or is removed.
/// Units: Applied positions world-local metres; carved shapes tile-local metres; radians.
/// Errors: None.
/// Version: since Z2 Stage 4. The save forms are Formats.md "Save section layout" lists 7 and 6, field for field.
/// @endcontract
struct Zenith_AIObstacleApplied                // 48 bytes
{
	uint64 m_ulSimulationID = 0;              //  0
	Zenith_Vector3 m_xPosition;                //  8  world-local, shifted by the listener at a rebase
	float m_fYaw = 0.0f;                       // 20
	Zenith_Vector3 m_xHalfExtents;             // 24
	float m_fMoveThreshold = 0.0f;             // 36
	uint8 m_eShape = 0;                       // 40
	uint8 m_eMode = 0;                        // 41
	uint8 m_uArea = 0;                        // 42
	uint8 m_bEnabled = 0;                     // 43
	uint8 m_auReserved[4] = {};               // 44
};
static_assert(sizeof(Zenith_AIObstacleApplied) == 48);
struct Zenith_AICarvedTile                     // 32 bytes
{
	Zenith_NavTileKey m_xKey;                  //  0
	uint32 m_uVersion = 0;                    //  8
	uint16 m_uShapeCount = 0;                 // 12  ≤ uAI_NAV_MAX_OBSTACLES_PER_WORLD
	uint16 m_uReserved = 0;                   // 14
	uint64 m_ulCheck = 0;                     // 16  the rebuilt tile section's 32-bit check, zero-extended
	uint32 m_uFirstShape = 0;                 // 24  index into the carve state's Zenith_Array<Zenith_NavObstacleShape>
	uint32 m_uReserved2 = 0;                  // 28
};
static_assert(sizeof(Zenith_AICarvedTile) == 32);
```

### Merge functions

```cpp
// AI.NavIngest's merge node (Navigation.md "Navigation asset requests"): the default merge's steps for the node's buffers,
// then, in collected order, allocate (Zenith_ModulePublicationSink::AllocateCompletionID(ASYNC_SOURCE_NAVIGATION)) and issue
// each need while the in-flight count is below ai.nav.maxLoadsInFlight; a need the sink refuses (PUBLICATION_SINK_ERROR_FULL,
// or any other Zenith_PublicationSinkError, treated alike) stays NEED in its supplier slot (ai.nav.loads_deferred_delta).
void Zenith_AINavIngestMerge(Zenith_MergeContext& xContext);      // Internal/NavMesh/Zenith_NavIngest.cpp
// AI.Perception's merge node (Agents.md "Perception events" step 4): accept or reject each perceiver's buffer in base-key
// order, then apply the proposal to the ledger entry of an accepted buffer, or append only the tick's new removals to the
// pending forgets of a rejected one.
void Zenith_AIPerceptionMerge(Zenith_MergeContext& xContext);     // Internal/Perception/Zenith_AIPerception.cpp
// AI.Locomotion's merge node: "Agent runtime record" below.
void Zenith_AILocomotionMerge(Zenith_MergeContext& xContext);     // Internal/Agents/Zenith_AILocomotion.cpp
```

Each is registered as its system's `Zenith_SystemDesc::m_pfnMerge` (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_MergeContext header"); the `Zenith_MergeContext` carries the node's buffers in base-key order and the world tag that finds the `Zenith_AIWorldState`.

### Internal resource types

Each type below is registered by `Zenith_Module_AI_RegisterTypes` as `TYPE_KIND_RESOURCE` under the stable name in its `xTYPE_ID` comment, added by `AttachWorld` step 3, and owned by `Zenith_AIWorldState`. They share one contract:

```cpp
/// @contract type Zenith_AIPathService, Zenith_AIAgentRuntimeTable, Zenith_AICrowdGrid, Zenith_AIStimulusGrid,
///   Zenith_AINoiseBuffer, Zenith_AITraceStaging
/// Ownership: Members of Zenith_AIWorldState, one per world; every array is reserved at AttachWorld from the configuration
///   (a failure is attach step 2's AI_ERROR_OUT_OF_MEMORY) and never grows in a tick.
/// Thread affinity: Written only by the one writer of Agents.md "Frame-graph placement" (path service: AI.NavRequests; agent
///   table: AI.Locomotion and its merge node; grids: AI.CrowdGrid, AI.StimulusGrid; noise buffer: AI.Noise and AI.Stimulus
///   playback; trace staging: TraceDecision per VM slot and the merge at AI.NavIngest), by the origin-shift listener at sync
///   point 1 (positions) and by the save participant out of tick; read by the nodes that declare ReadResource of the name.
/// Invalidation: Spans returned are valid for the reading invocation.
/// Units: World-local metres, ticks.
/// Errors: None; capacity outcomes are counted as their algorithms state.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Only Zenith_AITraceStaging is outside the save and the canonical hash (development diagnostics).
/// @endcontract
struct Zenith_AIPathQueueEntry { uint8 m_eKind = 0; uint8 m_eState = 0; uint8 m_uFlags = 0; uint8 m_uReserved = 0;
	uint32 m_uDescriptor = 0; uint64 m_ulOwner = 0; uint64 m_ulEnqueueTick = 0; uint32 m_uExpansions = 0;
	uint32 m_uTicksActive = 0; uint8 m_auReserved[32] = {}; };                      // 64 bytes (Navigation.md "Request queue")
class Zenith_AIPathService
{
public:
	uint32 GetActiveCount() const;                            // ai.path.active
	uint32 GetQueuedCount() const;                            // ai.path.queued
	static const Zenith_TypeID xTYPE_ID;                                              // "Zenith.AI.PathService"
private:
	Zenith_Array<Zenith_AIPathQueueEntry> m_axQueue;                 // ai.path.maxQueuedRequests, queue order
	Zenith_Array<Zenith_NavPathSearchDesc> m_axDescriptors;          // indexed by m_uDescriptor
	Zenith_Array<uint32> m_auActive;                                // queue indices of the active set, ai.path.maxActiveRequests
	Zenith_Array<Zenith_NavPathSearch> m_axSearches;                 // one per active slot
	Zenith_Array<Zenith_NavVolumeSearch> m_axVolumeSearches;         // one per active slot
	Zenith_Array<Zenith_Arena> m_axNodeArenas;                       // one block per active slot of B(N) = 40 N + 16 S + L(N) + L(S)
	                                                                 // bytes (240 KiB at N = 2,048; Navigation.md "Search state and slicing")
	Zenith_HashMap<uint64, uint32> m_xAgentResults;                // agent simulation ID → its result slot, never iterated
};
class Zenith_AIAgentRuntimeTable
{
public:
	const Zenith_AIAgentRuntime* Find(uint64 ulSimulationID) const;   // binary search
	uint32 GetCount() const;
	static const Zenith_TypeID xTYPE_ID;                                              // "Zenith.AI.AgentRuntime"
private:
	Zenith_Array<Zenith_AIAgentRuntime> m_axRecords;                 // ai.agent.maxAgents, ascending simulation ID
};
struct Zenith_AICrowdEntry { uint64 m_ulSimulationID = 0; Zenith_Vector3 m_xPosition; Zenith_Vector3 m_xVelocity;
	float m_fRadius = 0.0f; float m_fHeight = 0.0f; uint8 m_uPriority = 0; uint8 m_eMedium = 0; uint8 m_uFlags = 0;
	uint8 m_uReserved = 0; uint32 m_uReserved2 = 0; };                                                        // 48 bytes; flag bit 0 NON_AVOIDING
class Zenith_AICrowdGrid
{
public:
	static const Zenith_TypeID xTYPE_ID;                                              // "Zenith.AI.CrowdGrid"
private:
	Zenith_Array<Zenith_AICrowdEntry> m_axEntries;                   // sorted (cell key, simulation ID)
	Zenith_HashMap<uint64, uint64> m_xCells;                       // packed cell key → (first << 32 | count), never iterated
};
struct Zenith_AIStimulusEntry { uint64 m_ulSimulationID = 0; Zenith_Vector3 m_xTargetPoint; uint8 m_uTeam = 0;
	uint8 m_auReserved[3] = {}; float m_fVisibility = 0.0f; float m_fRadius = 0.0f; uint8 m_auReserved2[8] = {}; };   // 40 bytes
class Zenith_AIStimulusGrid
{
public:
	static const Zenith_TypeID xTYPE_ID;                                              // "Zenith.AI.StimulusGrid"
private:
	Zenith_Array<Zenith_AIStimulusEntry> m_axEntries;                // sorted (16 m cell key, simulation ID)
	Zenith_HashMap<uint64, uint64> m_xCells;
};
struct Zenith_AINoiseEntry { uint64 m_ulConsumeTick = 0; Zenith_Vector3 m_xPosition; float m_fLoudness = 0.0f;
	Zenith_EntityHandle m_xInstigator; Zenith_EntityHandle m_xPerceiver; uint8 m_eKind = 0; uint8 m_uTeam = 0;
	uint8 m_auReserved[6] = {}; };                                                   // 48 bytes; kind 1 noise, 2 damage, 3 touch
class Zenith_AINoiseBuffer
{
public:
	static const Zenith_TypeID xTYPE_ID;                                              // "Zenith.AI.NoiseBuffer"
private:
	Zenith_Array<Zenith_AINoiseEntry> m_axEntries;                   // 2 × ai.perception.maxNoisesPerTick, buffer order
};
struct Zenith_AITraceRecord { uint64 m_ulTick = 0; uint64 m_ulSimulationID = 0; uint16 m_uSequence = 0; uint16 m_uCode = 0;
	float m_fA = 0.0f; float m_fB = 0.0f; Zenith_StringID m_xLabel; };                  // 32 bytes (Scripting.md "Debug trace helpers")
class Zenith_AITraceStaging                                                     // not a world resource: module state
{
public:
	uint32 GetVMCount() const;
private:
	Zenith_Array<Zenith_FixedArray<Zenith_AITraceRecord, 1024>> m_axPerVM;   // one per VM slot
	Zenith_Array<uint32> m_auCounts;
};
```

### Search tile view

```cpp
/// @contract value Zenith_NavTileView
/// Ownership: Plain value: a pointer to one loaded tile's storage and its operation table; held in the tile slot.
/// Thread affinity: IMMUTABLE between tile changes; read by every query and search. Zenith_NavTileViewOps is a static table
///   per storage, so its members are const; Zenith_NavTileView's members are non-const: the tile slot reassigns the view at
///   a carve swap.
/// Invalidation: Replaced at a carve swap; invalid when the tile is removed.
/// Units: Tile-local quantised positions; world-local through the tile's L.
/// Errors: None (the storage passed T1 … T9 at load).
/// Version: since Z2 Stage 4. Two implementations exist until the D-SEL-NAV ruling (Navigation.md "Candidate mapping"):
///   the neutral section's (Internal/NavMesh/Zenith_NavTileCodec.cpp) and the Detour bridge's over dtMeshTile; the ruling
///   deletes one.
/// @endcontract
struct Zenith_NavTileViewOps
{
	uint32 (* const m_pfnGetPolyCount)(const void* pStorage) = nullptr;
	uint32 (* const m_pfnGetPolyVertices)(const void* pStorage, uint32 uPoly, Zenith_Vector3* axOut) = nullptr;   // ≤ 6, tile-local metres
	uint16 (* const m_pfnGetPolyNeighbour)(const void* pStorage, uint32 uPoly, uint32 uEdge) = nullptr;          // the tile section's encoding
	uint8 (* const m_pfnGetPolyArea)(const void* pStorage, uint32 uPoly) = nullptr;
	uint16 (* const m_pfnGetPolyFlags)(const void* pStorage, uint32 uPoly) = nullptr;                             // baked flags
	bool (* const m_pfnGetPolyHeight)(const void* pStorage, uint32 uPoly, const Zenith_Vector3& xLocal, float& fOut) = nullptr;
	uint32 (* const m_pfnQueryPolygons)(const void* pStorage, const Zenith_Vector3& xMin, const Zenith_Vector3& xMax,
		uint32* auOut, uint32 uCapacity) = nullptr;                                                         // BV tree, any order
};
struct Zenith_NavTileView
{
	const void* m_pStorage = nullptr;
	const Zenith_NavTileViewOps* m_pxOps = nullptr;
};
```

Every query first filters what `QueryPolygons` returns with an exact binary32 test of each polygon's own AABB (computed from `GetPolyVertices` and the detail heights, tile-local) against the query box, then sorts the survivors by polygon index, so neither storage's BV quantisation (Detour's `dtCreateNavMeshData` rebuilds its tree with its own rounding) nor its traversal order reaches a result: both storages yield the same candidate set for every query. External links and off-mesh links come from AI's own tables, never from the view.

The search's lookup type and the neighbour accessor, in `Internal/Search/Zenith_NavSearchInternal.h` (ZenithAI's own sources and tests include it; the public header names the lookup only by its forward declaration `struct Zenith_NavSearchLookup`):

```cpp
/// @contract type Zenith_NavSearchLookup
/// Ownership: Carved from a search's node arena by Begin or RestoreSnapshot (Navigation.md "Search state and slicing", arena block),
///   with TryReserve called before the first insert, so it never grows; it lives as long as the arena block.
/// Thread affinity: OWNER_THREAD, as its search.
/// Invalidation: Reset with the search.
/// Units: None.
/// Errors: None after the reservation.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Used for membership only and never iterated, so it is rebuilt from the node array or the stamps at
///   restore without changing a result. Storage L(n) = 16 n + 8 P(n) bytes for n entries.
/// @endcontract
struct Zenith_NavSearchLookup
{
	Zenith_HashMap<uint64, uint32> m_xMap;   // node reference value (or tile key Pack(), or volume reference value) → index
};

/// @contract value Zenith_NavNeighbour
/// Ownership: Plain value written by Zenith_NavMeshInternal::GetNeighbours into the caller's span.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: m_xPoly follows Zenith_NavPolyRef.
/// Units: World-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavNeighbour
{
	Zenith_NavPolyRef m_xPoly;                 // the neighbour polygon (polygon form)
	Zenith_Vector3 m_xEntry;                   // its entry position when entered from the source polygon (Navigation.md "Search graph"):
	                                           // the shared edge's midpoint, or for an external link the midpoint of the edge clamped to
	                                           // the link's overlap interval [tmin, tmax]
	uint8 m_uEdge = 0;                        // the source polygon's edge index
	uint8 m_bExternal = 0;                    // 1: through an external link
	uint8 m_auReserved[2] = {};
};

struct Zenith_NavMeshInternal
{
	// The canonical neighbour order of Navigation.md "Tile connection procedure" without off-mesh links: in-tile neighbours by
	// ascending edge index, then the external links in their sorted order. Unfiltered (the caller applies its filter). Writes at
	// most axOut.GetCount() entries and returns the total count; ANY_THREAD for readers of Zenith.AI.World; a stale xPoly returns 0.
	// Used by the search's neighbour step, surface movement, the raycast, the random-point flood and the test reachability and cost
	// oracle (Tests.md "Fixtures").
	static uint32 GetNeighbours(const Zenith_NavMesh& xMesh, Zenith_NavPolyRef xPoly,
		Zenith_Span<Zenith_NavNeighbour> axOut);
};
```

### Save participant class

```cpp
/// @contract type Zenith_AISaveParticipant
/// Ownership: Owned by Zenith_AIWorldState (play worlds), registered at AttachWorld step 6 and unregistered before DetachWorld
///   destroys the resources.
/// Thread affinity: SAVE's participant contract (PublicAPI of SAVE: "Zenith_SaveParticipant header").
/// Invalidation: None.
/// Units: Section version 1.
/// Errors: SAVE_ERROR_CORRUPT for any decode or re-issue mismatch; SAVE_ERROR_OUT_OF_MEMORY for the decoded copy.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4. The algorithm is Agents.md "Save and restore algorithm".
/// @endcontract
class Zenith_AISaveParticipant final : public Zenith_SaveParticipant
{
public:
	explicit Zenith_AISaveParticipant(Zenith_AIWorldState& xWorld);
	const char* GetName() const override;                         // "Zenith.AI.Navigation"
	uint16 GetSectionVersion() const override;                   // 1
	Zenith_SaveParticipantFlags GetFlags() const override;        // 0
	void DeclareAccess(Zenith_SystemBuilder& xSnapshotSystem) override;
	Zenith_Result<void, Zenith_SaveError> CaptureSection(const Zenith_SaveCaptureContext& xContext,
		Zenith_SaveSectionWriter& xWriter) override;
	uint32 GetReissuedSourceKinds() const override;               // PATHFINDING and NAVIGATION bits
	Zenith_Result<void, Zenith_SaveError> RestoreSection(const Zenith_SaveRestoreContext& xContext,
		Zenith_SaveSectionReader& xReader) override;
	bool IsRestoreReady(const Zenith_SaveRestoreContext& xContext) override;
	uint32 ListRestoreWaits(Zenith_Span<const Zenith_OperationHandle*> axOut) const override;
	// GetCaptureFidelity and CollectExcludedEntities are SAVE's defaults: every capture is EXACT, nothing is excluded.
};
```

| Member | Implementation |
|---|---|
| `DeclareAccess` | `ReadResource` of the six resources named in Agents.md "Fidelity" |
| `CaptureSection` | the lists of Formats.md "Save section layout" |
| `RestoreSection` | Restore steps 1–4 |
| `IsRestoreReady`, `ListRestoreWaits` | `IsRestoreReady` steps 1–8; the waits while step 1 is outstanding |
