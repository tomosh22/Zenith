# WORLD design: declarations

Section file of `Zenith2/Docs/Design/WORLD.md`. It holds every declaration WORLD's code exposes: the one public root header of WORLD inside ZenithAuthoring, `Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h` (included as `#include "Authoring/Zenith_AuthoringWorld.h"`, `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 1), the ZenithCook-internal importer header, and the ZenithEditor-internal headers of WORLD's extension. Every type carries a contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type"; every fallible function returns `Zenith_Result<T, Zenith_WorldError>`. These are tool-side declarations (`ZENITH_TOOLS_HOST`), never in a Player.

## Header list

| Header | Library | Declares | Section |
|---|---|---|---|
| `Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h` | ZenithAuthoring (public root) | everything below except the two last rows | this file, from "Limits and constants" to "Navigation document functions" |
| `Zenith2/Tools/ZenithCook/Internal/World/Zenith_WorldImporter.h` | ZenithCook (internal) | `Zenith_WorldImporter`, `Zenith_WorldImportSourceAccess`, the two production contributors, `Zenith_RegisterWorldImporters` | "Importer declarations" |
| `Zenith2/Tools/ZenithEditor/Internal/World/*.h` | ZenithEditor (internal) | the extension, the region loader, the three background operations (plan, bake, generate) and the editor plan, the commands | "Editor declarations" |

The root header includes only `ZenithCore`, `ZenithReflect`, `ZenithDocument`, `ZenithScene`, `ZenithAssets`, `ZenithStreaming` (for `Zenith_StreamingGridRole` and the STREAM limits), `ZenithEnvironment` (for `Zenith_PlacementSurface`, `Zenith_TerrainDocument` and `Zenith_TerrainTileSource`) and `ZenithCompression` (payloads, the codec set ENV's terrain loader takes) public headers, and WORLD's test files additionally include `ZenithJobs` (the worker-count invariance tests drive work items through `Zenith_JobSystem::RunRoot`). Under `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 4 the library starts at `DEPENDS ZenithCore`, so WORLD's move ticket appends, in its own diff, each of ZenithReflect, ZenithDocument, ZenithScene, ZenithAssets, ZenithStreaming, ZenithEnvironment, ZenithCompression and ZenithJobs that the list on the `master` it branches from does not yet hold; WORLD's design claims none of them as already declared (row WD24 asks EDIT to list the eight in WORLD's owner row). No WORLD declaration names a job system: every WORLD function is single-threaded on its caller's thread, and callers schedule WORLD's work items themselves ("Partition functions", "Generator types", "Lighting bake functions").

## Limits and constants

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringWorld.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Sha256.h"
#include "ZenithCore/Zenith_Hash.h"
#include "ZenithCore/Zenith_Random.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithDocument/Zenith_DocumentMigrator.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithScene/Zenith_SceneDocument.h"
#include "ZenithScene/Zenith_PrefabResolver.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithStreaming/Zenith_StreamingTypes.h"
#include "ZenithEnvironment/Zenith_ProceduralPlacement.h"
#include "ZenithEnvironment/Zenith_TerrainDocument.h"
#include "ZenithCompression/Zenith_CompressionCodecSet.h"

class Zenith_TypeRegistry;

inline constexpr uint32 uWORLD_MAX_LAYERS = 32;
inline constexpr uint32 uWORLD_MAX_LAYER_NAME_BYTES = 24;
inline constexpr uint32 uWORLD_KEY_CHARS = 8;                          // a layer key or generator key: the first 8 characters of the id
inline constexpr uint32 uWORLD_MAX_REGIONS = 65536;
inline constexpr uint32 uWORLD_MAX_GENERATORS = 64;
inline constexpr uint32 uWORLD_MAX_TERRAINS = 16;
inline constexpr uint32 uWORLD_MAX_WORLD_ENTITIES = 16777216;
inline constexpr uint32 uWORLD_MAX_GENERATED_PER_REGION = 65536;
inline constexpr uint32 uWORLD_MAX_LIGHT_PAGES = 256;
inline constexpr uint32 uWORLD_MAX_BAKE_TRIANGLES = 16777216;
inline constexpr uint32 uWORLD_MAX_DIAGNOSTICS = 4096;                 // records a Zenith_WorldDiagnostics keeps
inline constexpr uint32 uWORLD_PROBE_FACE_SIZE = 128;                  // baked reflection-probe face edge (LIGHT's desktop size)
inline constexpr uint64 ulWORLD_MAX_PAYLOAD_BYTES = 268435456ull;
inline constexpr sint32 iWORLD_MAX_REGION_COORDINATE = 1 << 20;
inline constexpr uint32 uWORLD_PERSISTENT_LAYER = 0xFFFFFFFFu;        // the ID index's value for persistent-scene entities
inline constexpr uint32 uWORLD_IMPORTER_VERSION = 1;                   // bumped by any change to an intermediate for unchanged inputs
inline constexpr uint32 uWORLD_LIGHT_TRACER_VERSION = 1;               // bumped by any change to the tracer's output for unchanged
                                                                        // inputs, a change of Zenith_Hash64's algorithm included
inline constexpr uint32 uWORLD_GENERATOR_FRAMEWORK_VERSION = 1;
inline constexpr uint64 ulWORLD_GENERATOR_SALT = 0x576F726C6447656Eull;   // "WorldGen"
inline constexpr uint64 ulWORLD_PLACEMENT_SALT = 0x576F726C64506C63ull;   // "WorldPlc"
inline constexpr uint64 ulWORLD_LIGHT_TRACER_SEED = 0x576F726C644C6974ull; // "WorldLit": the tracer's seed (Bakes.md "Tracing")
inline constexpr uint32 uWORLD_EDITOR_MAX_LIVE_PLANS = 3;               // Editing.md "World editor extension"
inline constexpr uint32 uWORLD_EDITOR_PREVIEW_ROOTS_PER_FRAME = 16384;  // Editing.md "Partition preview"
```

| Scope constant (a `Zenith_PersistentID` constant built from two `uint64` literals) | Derives |
|---|---|
| `xWORLD_SCOPE_ANCHOR` = `{0x574F524C44414E43, 0x484F520000000001}` | a cell's or navigation input scene's anchor root ID from its scene GUID |
| `xWORLD_SCOPE_COLLIDER_PROXY` = `{0x574F524C44434F4C, 0x4C49444500000002}` | a collider proxy's ID from its entity |
| `xWORLD_SCOPE_IRRADIANCE_VOLUME` = `{0x574F524C44474956, 0x4F4C554D00000003}` | a cell's irradiance-volume entity from the cell GUID |
| `xWORLD_SCOPE_NAV_MESH_REF` = `{0x574F524C444E4156, 0x4D45534800000004}` | an unstreamed content scene's `Zenith.AI.NavMeshRef` entity from its manifest's GUID (Partition.md "Unstreamed worlds") |

The scope constants are permanent format properties: derived IDs are stored in cooked scenes and saves.

## Error and diagnostics types

```cpp
/// @contract enum Zenith_WorldErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are appended, never renumbered (reports and diagnostics carry them by name).
/// @endcontract
enum Zenith_WorldErrorCode : uint16
{
	WORLD_ERROR_INVALID_DESC = 1,         // an argument or document member outside its documented range; W4, W18, W29, W37
	WORLD_ERROR_OUT_OF_MEMORY,
	WORLD_ERROR_DOCUMENT_SCHEMA,          // W1 to W3
	WORLD_ERROR_REFERENCE,                // W5, W46: a referenced asset is missing or of another kind
	WORLD_ERROR_LAYER_INVALID,            // W6 to W8, W19, W25, W26
	WORLD_ERROR_GRID_UNKNOWN,             // W9
	WORLD_ERROR_GRID_LIMIT,               // W10
	WORLD_ERROR_GRID_INVALID,             // W11, W12, W27, W28, W38
	WORLD_ERROR_GRID_ALIGNMENT,           // W13, W14
	WORLD_ERROR_REGION_INVALID,           // W15, W16, W22, W39
	WORLD_ERROR_REGION_MISSING,           // W20
	WORLD_ERROR_DUPLICATE_ENTITY,         // W23
	WORLD_ERROR_PREFAB_RESOLUTION,        // SCENE resolution failed; detail: the Zenith_SceneErrorCode
	WORLD_ERROR_ENTITY_BOUNDS,            // an entity outside its cell's overlap bound, or beyond 4,096 m in a layer or content scene
	WORLD_ERROR_LIMIT,                    // a limit of Documents.md "Document limits" or Partition.md "Partition limits"; detail: the row
	WORLD_ERROR_GENERATOR_INVALID,        // W17, W30, W32, W33, W36
	WORLD_ERROR_GENERATOR_VERSION,        // W31
	WORLD_ERROR_GENERATOR_FAILED,         // a generator returned an error, emitted an output it does not own, or duplicate output keys
	WORLD_ERROR_GENERATOR_LIMIT,          // more than uWORLD_MAX_GENERATED_PER_REGION outputs
	WORLD_ERROR_BAKE_CORRUPT,             // W42, W43
	WORLD_ERROR_BAKE_IDENTITY,            // W44
	WORLD_ERROR_BAKE_GEOMETRY,            // a mesh could not be read, or a unit above uWORLD_MAX_BAKE_TRIANGLES
	WORLD_ERROR_ATLAS_FULL,               // more than uWORLD_MAX_LIGHT_PAGES pages
	WORLD_ERROR_BAKE_CANCELLED,
	WORLD_ERROR_SOURCE_ACCESS,            // the source access refused a read; detail: its own code (an import or document error)
	WORLD_ERROR_STORAGE,                  // a document write failed; detail: the Zenith_DocumentErrorCode
	WORLD_ERROR_FINDINGS_DROPPED,         // the summary ERROR after the collector dropped records ("World validation messages")
};
constexpr const char* Zenith_ToString(Zenith_WorldErrorCode eCode);

/// @contract enum Zenith_WorldWarningCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are appended, never renumbered.
/// @endcontract
enum Zenith_WorldWarningCode : uint16
{
	WORLD_WARNING_ENTITY_OUTSIDE_REGION = 1001,   // W21
	WORLD_WARNING_GENERATED_EDITED,               // W24
	WORLD_WARNING_BAKE_STALE,                     // W34, W41
	WORLD_WARNING_BAKE_MISSING,                   // W35, W40
	WORLD_WARNING_BAKE_ORPHANED,                  // W45
	WORLD_WARNING_FINDINGS_DROPPED,               // the summary WARNING after the collector dropped records
	WORLD_WARNING_REFERENCE_UNRESOLVED,
	WORLD_WARNING_REFERENCE_UNBINDABLE,
	WORLD_WARNING_PERSISTENT_IN_LAYER_SCENE,
	WORLD_WARNING_HLOD_MATERIALS_DROPPED,
	WORLD_WARNING_NAVIGATION_REACH,               // W47
};
constexpr const char* Zenith_ToString(Zenith_WorldWarningCode eCode);

/// @contract value Zenith_WorldError
/// Ownership: Plain value, 24 bytes.
/// Thread affinity: IMMUTABLE. Members non-const: held as the error of a Zenith_Result, whose move assignment assigns
///   it.
/// Invalidation: None.
/// Units: m_uIndex a record, region or unit index as the code states.
/// Errors: None; it is the error.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldError
{
	Zenith_WorldErrorCode m_eCode = WORLD_ERROR_INVALID_DESC;
	uint16 m_uDetail = 0;                // the underlying code: SCENE, document, import, environment, limit row
	uint32 m_uIndex = 0;
	Zenith_PersistentID m_xSubject;       // the world, region, entity or unit the error names; null when none
};
static_assert(sizeof(Zenith_WorldError) == 24);

/// @contract enum Zenith_WorldDiagnosticLevel
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_WorldDiagnosticLevel : uint8 { WORLD_DIAGNOSTIC_INFO = 1, WORLD_DIAGNOSTIC_WARNING, WORLD_DIAGNOSTIC_ERROR };

/// @contract value Zenith_WorldDiagnostic
/// Ownership: A record of a Zenith_WorldDiagnostics; its views point into the collector's arena.
/// Thread affinity: IMMUTABLE once recorded. Members non-const: an element of an output array that its producer fills
///   in place.
/// Invalidation: With the collector.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldDiagnostic
{
	Zenith_WorldDiagnosticLevel m_eLevel;
	uint16 m_uCode;                      // a Zenith_WorldErrorCode (ERROR) or Zenith_WorldWarningCode (WARNING); 0 for INFO
	uint16 m_uRule;                      // the W-number of Documents.md "World validation messages", 0 when none
	Zenith_StringView m_strPath;          // the document's storage path
	Zenith_StringView m_strPointer;       // the JSON pointer, empty when none
	Zenith_StringView m_strMessage;       // begins with Zenith_ToString of the code
};

/// @contract type Zenith_WorldDiagnostics
/// Ownership: A stack or member object owning its records in its own arena, created from the Zenith_Heap given at
///   construction (Zenith2/Engine/ZenithCore/Zenith_Heap.h).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Records are invalid after Reset or destruction.
/// Units: None.
/// Errors: Add never fails visibly: beyond uWORLD_MAX_DIAGNOSTICS records it counts the overflow per level
///   (GetDroppedCount, GetDroppedErrorCount) and keeps the first records; the caller reports the summary of
///   Documents.md "World validation messages".
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldDiagnostics
{
public:
	explicit Zenith_WorldDiagnostics(Zenith_Heap& xHeap);
	void Add(Zenith_WorldDiagnosticLevel eLevel, uint16 uCode, uint16 uRule, Zenith_StringView strPath,
		Zenith_StringView strPointer, Zenith_StringView strMessage);
	Zenith_Span<const Zenith_WorldDiagnostic> GetRecords() const;   // insertion order
	uint32 GetErrorCount() const;                                // kept and dropped
	uint32 GetDroppedCount() const;
	uint32 GetDroppedErrorCount() const;
	void Reset();
};
```

## Identity and keys

```cpp
/// @contract functions Zenith_WorldIdentity
/// Ownership: Pure functions; text is appended to the caller's array.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ParseWorldRegionKey returns WORLD_ERROR_REGION_INVALID for text outside the grammar of Documents.md
///   "Region objects"; the others cannot fail.
/// Version: since Z2 Stage 4. Every derivation is a permanent format property (stored in cooked scenes and saves); SHA-256,
///   outside D-SEL-HASH (open, Stage 1).
/// @endcontract
Zenith_PersistentID Zenith_WorldKeyID(Zenith_StringView strKey);          // SHA-256("ZWKEY:" ‖ key), v4 bits
Zenith_PersistentID Zenith_WorldDeriveProductGUID(const Zenith_PersistentID& xWorld, Zenith_StringView strKey);
Zenith_PersistentID Zenith_WorldAnchorEntityID(const Zenith_PersistentID& xScene);
Zenith_PersistentID Zenith_WorldColliderProxyID(const Zenith_PersistentID& xEntity);
Zenith_PersistentID Zenith_WorldIrradianceVolumeEntityID(const Zenith_PersistentID& xCellScene);
Zenith_PersistentID Zenith_WorldGeneratedID(const Zenith_PersistentID& xGenerator, Zenith_StringView strOutputKey);

/// @contract value Zenith_WorldRegionCoord
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a key is copy-assigned wherever it is stored (records, containers,
///   sort keys).
/// Invalidation: None.
/// Units: Region indices; |x|, |z| ≤ iWORLD_MAX_REGION_COORDINATE.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldRegionCoord { sint32 m_iX = 0; sint32 m_iZ = 0; };

void Zenith_WorldIDKey(const Zenith_PersistentID& xID, Zenith_Array<char>& xOut);   // the first 8 characters of the 36-character form
void Zenith_WorldRegionKey(Zenith_StringView strLayerKey, Zenith_WorldRegionCoord xCoord, Zenith_StringView strGeneratorKey,
	Zenith_Array<char>& xOut);                     // "<layerKey>/<x>_<z>", or "<layerKey>/<x>_<z>/<generatorKey>" when non-empty
void Zenith_WorldCoordKey(Zenith_WorldRegionCoord xCoord, Zenith_Array<char>& xOut);                              // "<x>_<z>"
void Zenith_WorldCellKey(Zenith_StringView strGrid, sint32 iX, sint32 iZ, Zenith_Array<char>& xOut);          // "cell:<grid>/<x>_<z>"
void Zenith_WorldLayerGridName(Zenith_StringView strLayerKey, Zenith_Array<char>& xOut);                        // "layer.<layerKey>"
struct Zenith_WorldParsedRegionKey { Zenith_StringView m_strLayerKey; Zenith_WorldRegionCoord m_xCoord; Zenith_StringView m_strGeneratorKey; };
Zenith_Result<Zenith_WorldParsedRegionKey, Zenith_WorldError> Zenith_ParseWorldRegionKey(Zenith_StringView strKey);
```

## World description types

```cpp
/// @contract enum Zenith_WorldLayerKind, Zenith_WorldLayerState, Zenith_WorldLayerRealisation, Zenith_WorldStreamingMode
/// Ownership: Plain enumerations.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a document value outside them is W3 or W8.
/// Version: since Z2 Stage 4. Values are stable.
/// @endcontract
enum Zenith_WorldLayerKind : uint8 { WORLD_LAYER_KIND_DATA = 1, WORLD_LAYER_KIND_GAMEPLAY };
enum Zenith_WorldLayerState : uint8 { WORLD_LAYER_STATE_EDITOR_ONLY = 1, WORLD_LAYER_STATE_BAKED, WORLD_LAYER_STATE_RUNTIME };
enum Zenith_WorldLayerRealisation : uint8 { WORLD_LAYER_REALISATION_NONE = 0, WORLD_LAYER_REALISATION_GRID, WORLD_LAYER_REALISATION_ADDITIVE };
enum Zenith_WorldStreamingMode : uint8 { WORLD_STREAMING_MODE_STREAMED = 1, WORLD_STREAMING_MODE_UNSTREAMED };

/// @contract value Zenith_WorldLayerDesc
/// Ownership: Plain value inside a Zenith_WorldDesc; views into the reader's arena.
/// Thread affinity: IMMUTABLE after Zenith_ReadWorldDocument returns. Members non-const: the document reader
///   default-constructs it and fills it field by field.
/// Invalidation: With the reader's arena.
/// Units: None.
/// Errors: None; validated by the reader.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldLayerDesc
{
	Zenith_StringView m_strName;          // display only
	Zenith_PersistentID m_xID;
	Zenith_StringView m_strLayerKey;      // the first 8 characters of m_xID's text
	Zenith_WorldLayerKind m_eKind;
	Zenith_WorldLayerState m_eState;
	Zenith_WorldLayerRealisation m_eRealisation;
	bool m_bInitiallyActive;
	bool m_bBakeLighting, m_bBakeNavigation, m_bBakeHlod;
	uint8 m_uGridIndex;                  // BAKED with "grid": the base grid's index; 0xFF when unset
	uint8 m_uLayerGridIndex;             // RUNTIME grid: layerGridBase + its position in layerGridOrder; 0xFF otherwise
};

/// @contract value Zenith_WorldGridDesc
/// Ownership: Plain value inside a Zenith_WorldDesc; m_xObject borrows the reader's document tree.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena and document.
/// Units: Metres (double for origins and extents).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGridDesc
{
	Zenith_StringView m_strName;
	Zenith_StreamingGridRole m_eRole;
	uint32 m_uCellSizeMetres;
	double m_dOriginX, m_dOriginZ, m_dBaseY, m_dHeight;
	float m_fOverlapMargin;
	float m_fActivationRadius, m_fUnloadHysteresis;
	uint8 m_uHlodLevels;
	float m_afHlodMaxDistance[3], m_afHlodRefineDistance[3];
	uint32 m_uDefaultForMask;            // bit per Zenith_StreamingGridRole value listed in defaultFor
	bool m_bLayerGrid;                    // a layer.<layerKey> grid built by the partition
	bool m_bRetired;                      // a layer grid whose layerGridOrder entry names no current grid-realised layer
	bool m_bDefaultActive;                // written as the grid object's defaultActive (row WD19)
	Zenith_JsonNode m_xObject;            // the grid object (base grids) or the template (layer grids), copied minus defaultFor
};

/// @contract value Zenith_WorldRegionDesc
/// Ownership: Plain value inside a Zenith_WorldDesc.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldRegionDesc
{
	uint32 m_uLayer;                     // index into Zenith_WorldDesc::m_axLayers
	Zenith_WorldRegionCoord m_xCoord;
	Zenith_PersistentID m_xScene;         // the region document's id (no asset)
	uint32 m_uGenerator;                 // index into m_axGenerators, or 0xFFFFFFFF for an authored region
	Zenith_StringView m_strKey;           // the region key
	Zenith_StringView m_strPath;          // the region document's path relative to the world directory
};

/// @contract value Zenith_WorldGeneratorRef
/// Ownership: Plain value inside a Zenith_WorldDesc.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGeneratorRef
{
	Zenith_StringView m_strGeneratorKey;  // the generators map key: the first 8 characters of m_xDocument's text (W16)
	Zenith_PersistentID m_xDocument;
	Zenith_StringView m_strPath;          // "Generators/<GeneratorKey>.generator.json" (W30); the display name lives in the document
};

/// @contract value Zenith_WorldLightingSettings, Zenith_WorldHlodSettings, Zenith_WorldNavigationSettings
/// Ownership: Plain values inside a Zenith_WorldDesc; spans into the reader's arena.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: As Documents.md "Lighting settings", "HLOD settings", "Navigation settings".
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldMaterialAlbedo { Zenith_PersistentID m_xMaterial; Zenith_Vector3 m_xAlbedo; };
struct Zenith_WorldLightingSettings
{
	bool m_bEnabled = true;               // the reader applies the mode-dependent default
	uint8 m_uGridIndex = 0;
	float m_fTexelsPerMetre = 4.0f;
	uint32 m_uPageSize = 1024, m_uPaddingTexels = 2, m_uMinRectTexels = 4;
	uint32 m_uSamplesPerTexel = 256, m_uBounces = 2;
	float m_fInfluenceRadius = 64.0f, m_fVolumeSpacing = 4.0f;
	uint32 m_uVolumeSamples = 256, m_uProbeSamplesPerTexel = 16;
	Zenith_Vector3 m_xSkyUpper, m_xSkyLower;
	float m_fSkyRadianceNits = 5000.0f;
	bool m_bSun = false;
	Zenith_Vector3 m_xSunDirection, m_xSunColour;
	float m_fSunIlluminanceLux = 0.0f;
	Zenith_Vector3 m_xDefaultAlbedo, m_xTerrainAlbedo;
	Zenith_Span<const Zenith_WorldMaterialAlbedo> m_axMaterialAlbedo;   // ascending material GUID
};
struct Zenith_WorldHlodSettings
{
	bool m_bEnabled = true;
	uint32 m_auTrianglesPerNode[3] = { 20000, 10000, 5000 };
	float m_afMaxError[3] = { 0.02f, 0.05f, 0.1f };
	bool m_abFlatten[3] = { false, true, true };
	float m_afMinInstanceRadius[3] = { 0.5f, 2.0f, 8.0f };
};
struct Zenith_WorldNavigationSettings
{
	bool m_bEnabled = false;
	Zenith_PersistentID m_xSettings;
	Zenith_JsonNode m_xObject;            // the bakes.navigation object, for the derived documents' copied members
};

/// @contract value Zenith_WorldDesc
/// Ownership: Plain value returned by Zenith_ReadWorldDocument; every span and view lives in the reader's arena and the
///   opened document it was read from, which must outlive it.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With that arena and document.
/// Units: Metres; region indices.
/// Errors: None after the reader.
/// Version: since Z2 Stage 4. The in-memory form is tool source API; the document's version is its schemaVersion.
/// @endcontract
struct Zenith_WorldDesc
{
	Zenith_PersistentID m_xID;
	Zenith_StringView m_strName;
	uint64 m_ulSeed;
	Zenith_PersistentID m_xPersistentScene;
	uint32 m_uRegionSize;
	double m_dOriginX, m_dOriginZ, m_dBaseY, m_dHeight;
	Zenith_WorldStreamingMode m_eMode;
	bool m_bSplitStaticColliders, m_bLargeExtentPromotion;
	Zenith_Span<const Zenith_WorldGridDesc> m_axGrids;        // base grids, then layer grids (Partition.md step 2)
	uint32 m_uBaseGridCount;                                 // = layerGridBase
	Zenith_Span<const Zenith_WorldLayerDesc> m_axLayers;      // name order
	Zenith_Span<const Zenith_WorldRegionDesc> m_axRegions;    // key order
	Zenith_Span<const Zenith_WorldGeneratorRef> m_axGenerators;   // generator-key order
	Zenith_Span<const Zenith_PersistentID> m_axTerrains;      // ascending
	Zenith_WorldLightingSettings m_xLighting;
	Zenith_WorldHlodSettings m_xHlod;
	Zenith_WorldNavigationSettings m_xNavigation;
};
```

## World document functions

```cpp
/// @contract functions Zenith_WorldDocumentFunctions
/// Ownership: Results live in the caller's arena and borrow xDocument.
/// Thread affinity: ANY_THREAD; the document is read-only for the call.
/// Invalidation: None beyond the result's arena.
/// Units: None.
/// Errors: Zenith_ReadWorldDocument returns the first ERROR's code after reporting every finding of the failing pass into
///   xDiagnostics; it checks the reader rules of Documents.md "World validation messages" (W1 to W4, W6 to W12, W15, W16,
///   W18, W19, W25, W26, W28, W29), which need no other document, and none of the importer rules, which Zenith_PlanWorldPartition
///   checks; WORLD_ERROR_OUT_OF_MEMORY when the arena is exhausted. Zenith_ValidateWorldDocument is the editor's validator: the
///   same reader rules, no result.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<Zenith_WorldDesc, Zenith_WorldError> Zenith_ReadWorldDocument(const Zenith_OpenedDocument& xDocument,
	const Zenith_PersistentID& xMetaGUID, Zenith_Arena& xArena, Zenith_WorldDiagnostics& xDiagnostics);
Zenith_Result<void, Zenith_WorldError> Zenith_ValidateWorldDocument(const Zenith_OpenedDocument& xDocument,
	Zenith_Arena& xScratch, Zenith_WorldDiagnostics& xDiagnostics);
// Region rectangle helpers (double precision, half-open).
void Zenith_WorldRegionAnchor(const Zenith_WorldDesc& xWorld, Zenith_WorldRegionCoord xCoord, double adOut[3]);
Zenith_WorldRegionCoord Zenith_WorldRegionOf(const Zenith_WorldDesc& xWorld, double dX, double dZ);
```

## Document type registration functions

```cpp
/// @contract functions Zenith_WorldDocumentTypeRegistration
/// Ownership: Registers four static Zenith_DocumentTypeDesc records and their canonical schemas with the caller's registry.
/// Thread affinity: MAIN_THREAD, before the registry's Freeze.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's Zenith_DocumentError, returned as WORLD_ERROR_STORAGE with it as detail, after the first refusal.
/// Version: since Z2 Stage 4. The type versions are Documents.md "Document type registration".
/// @endcontract
Zenith_Result<void, Zenith_WorldError> Zenith_RegisterWorldDocumentTypes(Zenith_MigrationRegistry& xRegistry);
const Zenith_CanonicalSchema& Zenith_GetWorldCanonicalSchema();
const Zenith_CanonicalSchema& Zenith_GetWorldGeneratorCanonicalSchema();
const Zenith_CanonicalSchema& Zenith_GetWorldLightBakeCanonicalSchema();
const Zenith_CanonicalSchema& Zenith_GetWorldLightPageCanonicalSchema();
```

## Source access interface

```cpp
/// @contract interface Zenith_WorldSourceAccess
/// Ownership: Implemented by the importer over its Zenith_ImportContext (Zenith_WorldImportSourceAccess), by the editor over
///   the saved documents of its storage roots and the plan's summary table (Zenith_WorldEditorSourceAccess, in run or pinned
///   mode, Editing.md "World editor extension") and by
///   tests over memory (Zenith_WorldTestSources, Tests/WORLD/Support header-only); borrowed by the partition, generator, terrain
///   and bake functions for one call.
/// Thread affinity: OWNER_THREAD: the one thread running the current call (the import thread in the cook; one job at a time
///   of the operation that created it in the editor; the test's thread), never two threads at once and never a work item
///   (Editing.md "World editor extension": "Source access affinity").
/// Invalidation: Documents and spans it returns are valid until the call that requested them returns.
/// Units: Bytes.
/// Errors: Each member returns WORLD_ERROR_SOURCE_ACCESS with the implementation's code as detail (an import error, a
///   document error); a missing optional input (a light bake header, a terrain tile) is reported as found = false, never an
///   error. GetMeshLod0Summary of the editor's access answers only from the plan's summary table; a mesh missing from it is
///   WORLD_ERROR_SOURCE_ACCESS. The editor's access in pinned mode answers an open whose file digest differs from the plan's
///   read table with WORLD_ERROR_SOURCE_ACCESS, detail DOCUMENT_ERROR_STALE_PLAN.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldSourceAccess
{
public:
	virtual ~Zenith_WorldSourceAccess() = default;
	virtual Zenith_Result<void, Zenith_WorldError> OpenDocumentByGUID(const Zenith_PersistentID& xAsset,
		Zenith_OpenedDocument& xOut) = 0;
	// Path relative to the world directory (region, generated, generator and bake documents); bFound false and no error when
	// the file does not exist.
	virtual Zenith_Result<bool, Zenith_WorldError> OpenDocumentByPath(Zenith_StringView strWorldRelativePath,
		Zenith_OpenedDocument& xOut) = 0;
	// Path relative to the directory of xAnchor's source document (a terrain's tileFiles; importer: OpenInputDocumentRelativeTo,
	// row WD31); bFound false and no error when the file does not exist.
	virtual Zenith_Result<bool, Zenith_WorldError> OpenDocumentRelativeTo(const Zenith_PersistentID& xAnchor,
		Zenith_StringView strRelativePath, Zenith_OpenedDocument& xOut) = 0;
	virtual Zenith_Result<Zenith_Span<const Zenith_WorldProduct>, Zenith_WorldError> ListProducts(
		const Zenith_PersistentID& xSource) = 0;
	// The mesh's LOD 0 summary, GEOM's published property "geom.lod0Summary" on ASSET_PLATFORM_WINDOWS (row WD30; Bakes.md
	// "Mesh summaries"; importer: ReadPublishedProperty, row WD31; editor: the plan's summary table).
	virtual Zenith_Result<Zenith_WorldMeshSummary, Zenith_WorldError> GetMeshLod0Summary(const Zenith_PersistentID& xMesh) = 0;
	virtual const Zenith_TypeRegistry& GetTypes() const = 0;
	virtual Zenith_PrefabProvider& GetPrefabProvider() = 0;
	virtual const Zenith_CompressionCodecSet& GetCodecs() const = 0;   // for ENV's terrain loader
};

/// @contract value Zenith_WorldMeshSummary
/// Ownership: Plain value, the decoded 48-byte property geom.lod0Summary.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Object-space metres for the sphere.
/// Errors: None; a property of another size is WORLD_ERROR_SOURCE_ACCESS from the access.
/// Version: since Z2 Stage 4. The layout is row WD30's.
/// @endcontract
struct Zenith_WorldMeshSummary { Zenith_Sha256Value m_xDigest; float m_afCentre[3]; float m_fRadius; };

/// @contract value Zenith_WorldProduct
/// Ownership: Plain value; the key view borrows the access implementation's or the plan's storage.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: As the span that returned it.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldProduct { Zenith_StringView m_strKey; Zenith_AssetGUID m_xGUID; Zenith_AssetKind m_eKind; };

/// @contract type Zenith_WorldTerrainTileSource
/// Ownership: A stack object over a borrowed Zenith_WorldSourceAccess and one terrain GUID; borrowed by
///   Zenith_TerrainDocument::Load for one call.
/// Thread affinity: OWNER_THREAD (the access's thread).
/// Invalidation: A span ReadTile returns is valid until the next ReadTile call.
/// Units: Bytes of one *.terraintile.json document.
/// Errors: ReadTile returns ENV_ERROR_TILE_MISSING when the access answers not found and ENV_ERROR_TILE_READ for an access
///   error (ENV's Zenith_TerrainTileSource contract).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Partition.md "Terrain loading".
/// @endcontract
class Zenith_WorldTerrainTileSource final : public Zenith_TerrainTileSource
{
public:
	Zenith_WorldTerrainTileSource(Zenith_WorldSourceAccess& xSources, const Zenith_PersistentID& xTerrain, Zenith_Arena& xScratch);
	// OpenDocumentRelativeTo(terrain, path), then the canonical bytes of the migrated tree (Zenith_WriteCanonicalJson with
	// the canonical schema the access's registry holds for ENV's tile type).
	Zenith_Result<Zenith_ByteSpan, Zenith_EnvironmentError> ReadTile(Zenith_StringView strRelativePath) override;
};
// Opens the terrain document by GUID, writes its canonical bytes and calls Zenith_TerrainDocument::Load(bytes, tile source,
// xSources.GetCodecs(), false, xHeap); ENV's error is WORLD_ERROR_SOURCE_ACCESS with ENV's code as detail.
Zenith_Result<Zenith_Owned<Zenith_TerrainDocument>, Zenith_WorldError> Zenith_WorldLoadTerrain(
	Zenith_WorldSourceAccess& xSources, const Zenith_PersistentID& xTerrain, Zenith_Arena& xScratch, Zenith_Heap& xHeap);
```

## Cell content writer

```cpp
/// @contract value Zenith_WorldCellBounds
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: None.
/// Units: Absolute world metres in double; the anchor is the minimum XZ corner at the grid's base Y.
/// Errors: None.
/// Version: since Z2 Stage 4. Adopted by AI's design (row WD11).
/// @endcontract
struct Zenith_WorldCellBounds
{
	uint8 m_uGridIndex;
	Zenith_StreamingGridRole m_eRole;
	sint32 m_iX, m_iZ;
	double m_dMinX, m_dMinZ, m_dMaxX, m_dMaxZ;   // half-open [min, max)
	double m_dBaseY, m_dHeight;
	Zenith_PersistentID m_xCellScene;             // the cell's GUID (for contributed IDs)
};

/// @contract interface Zenith_WorldCellContentWriter
/// Ownership: Implemented by the partition (Internal/WORLD/Partition/Zenith_WorldCellWriter.cpp); borrowed by a contributor for
///   one call.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Positions relative to the cell anchor (the contributor subtracts the anchor in double and narrows).
/// Errors: BeginEntity refuses a null or already-used ID and AddComponent an unknown stable name or a field set that does not
///   decode (WORLD_ERROR_INVALID_DESC naming the contributor); after the first refusal the cell fails with that error.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4. Adopted by AI's design (row WD11).
/// @endcontract
class Zenith_WorldCellContentWriter
{
public:
	virtual ~Zenith_WorldCellContentWriter() = default;
	virtual Zenith_Result<void, Zenith_WorldError> BeginEntity(const Zenith_PersistentID& xID,
		Zenith_StringView strName, const Zenith_Vector3& xLocalPosition, const Zenith_Quaternion& xLocalRotation) = 0;
	// A component in the JSON field-set form (ECS Reflect "JSON field-set form") at the given type version.
	virtual Zenith_Result<void, Zenith_WorldError> AddComponent(Zenith_StringView strStableName, uint32 uVersion,
		const Zenith_JsonDocument& xFields, Zenith_JsonNode xFieldSet) = 0;
	virtual void EndEntity() = 0;
};

/// @contract interface Zenith_WorldCellContributor
/// Ownership: Implemented by the importer's two production contributors (ENV's and AI's functions behind them, "Importer
///   declarations") and by the tests' recording contributor (Tests/WORLD/Support/Zenith_WorldRecordingContributor.h,
///   header-only); borrowed by Zenith_PlanWorldPartition through Zenith_WorldPartitionInput::m_apxContributors.
/// Thread affinity: OWNER_THREAD: the partition's thread.
/// Invalidation: None.
/// Units: As Zenith_WorldCellContentWriter.
/// Errors: Contribute returns the writer's refusal, or the contributor's own failure as WORLD_ERROR_INVALID_DESC with the
///   contributor's code as detail; the partition fails the cell with it.
/// Reentrancy: NONE: Contribute calls only the writer it is given.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldCellContributor
{
public:
	virtual ~Zenith_WorldCellContributor() = default;
	// Bit (1 << Zenith_StreamingGridRole) per role whose base-grid cells this contributor writes into; the partition calls
	// Contribute for every cell of a base grid whose role bit is set and, for ENV, that holds terrain content.
	virtual uint32 GetRoleMask() const = 0;
	virtual bool NeedsTerrainContent() const = 0;
	// axDerivedProducts: the products the partition itself derived so far (AI reads its navigation products from them).
	virtual Zenith_Result<void, Zenith_WorldError> Contribute(const Zenith_WorldCellBounds& xCell,
		Zenith_Span<const Zenith_WorldProduct> axDerivedProducts, Zenith_WorldCellContentWriter& xWriter) = 0;
};
```

## Role rules

```cpp
/// @contract value Zenith_WorldRoleRule
/// Ownership: constexpr table data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The rows are Partition.md "Grid assignment"; a change is a WORLD design diff.
/// @endcontract
enum Zenith_WorldRoleRuleKind : uint8
{
	WORLD_ROLE_RULE_KIND_KINEMATIC = 1,   // row 0: some kinematic RigidBody, no dynamic one
	WORLD_ROLE_RULE_KIND_ONLY_COMPONENTS, // rows 1 to 3: every component of every entity is in the list
	WORLD_ROLE_RULE_KIND_ANY,             // row 4
};
struct Zenith_WorldRoleRule
{
	const Zenith_StreamingGridRole m_eRole{};
	const Zenith_WorldRoleRuleKind m_eKind{};
	const Zenith_Span<const char* const> m_aszAllowedComponents{};   // ONLY_COMPONENTS rows
	const bool m_bRequireStatic = false;                             // row 1: every entity static (Partition.md "Grid
	                                                                 // assignment"); Zenith.Physics.RigidBody listed only
	                                                                 // with MotionType STATIC
};
Zenith_Span<const Zenith_WorldRoleRule> Zenith_GetWorldRoleRules();   // in match order
```

## Partition functions

```cpp
/// @contract value Zenith_WorldPartitionInput
/// Ownership: Plain value of borrowed references that outlive the call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldPartitionInput
{
	const Zenith_OpenedDocument* m_pxWorldDocument = nullptr;
	Zenith_PersistentID m_xMetaGUID;
	Zenith_WorldSourceAccess* m_pxSources = nullptr;
	Zenith_Span<Zenith_WorldCellContributor* const> m_apxContributors;   // ENV's then AI's (importer); recording ones (tests)
	bool m_bIncludeBakeFingerprints = true;                  // false: skip W34, W40, W41 and their digest and tile reads (the
	                                                         // editor's partition job; its fingerprint job computes them)
	bool (*m_pfnIsCancelled)(void* pUserData) = nullptr;   // checked between steps; null: never cancelled
	void* m_pCancelUserData = nullptr;
};

/// @contract value Zenith_WorldCellPlan
/// Ownership: A record of a Zenith_WorldPartitionPlan, in its arena.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: With the plan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldCellPlan
{
	Zenith_WorldCellBounds m_xBounds;
	uint32 m_uFirstHierarchy, m_uHierarchyCount;           // into the plan's hierarchy index array, step 13 order
	uint32 m_uEntityCount;                                  // contributed entities included
	uint32 m_uPersistentCount, m_uReferencesOut, m_uReferencesIn;
	uint32 m_uDependencyCount;                              // grid-derived dependencies (Partition.md "Partition limits" row 10)
	uint32 m_uLightingUnit;                                 // index into the plan's lighting units, or 0xFFFFFFFF
};

/// @contract value Zenith_WorldPartitionReport
/// Ownership: Plain value inside the plan.
/// Thread affinity: IMMUTABLE. Members non-const: its producer resets it and accumulates its counters in place.
/// Invalidation: With the plan.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 4. Its JSON form is the import's INFO diagnostics and the editor's partition-check view.
/// @endcontract
struct Zenith_WorldPartitionReport
{
	uint32 m_uRegions, m_uHierarchies, m_uEntities, m_uDroppedEditorOnly;
	uint32 m_auCellsPerGrid[8], m_auEntitiesPerGrid[8];
	uint8 m_auLayerGridIndex[32];                           // per layer in name order; 0xFF for a layer without a grid
	uint32 m_uPromoted, m_uSplits, m_uUnsplit, m_uReferences, m_uUnresolved, m_uUnbindable, m_uPersistent;
	uint32 m_uHlodNodes, m_uLightingFresh, m_uLightingStale, m_uLightingMissing;
	uint32 m_uNavigationRegions, m_uNavigationProducts;
	uint32 m_uGenerated;                                    // entities from cook-mode generators
	uint32 m_uWorstCaseLiveInstances;                       // Partition.md "Partition limits" row 11
};

/// @contract type Zenith_WorldPartitionPlan
/// Ownership: Created by Zenith_PlanWorldPartition in the caller's arena (trivially destructible records); borrows the opened
///   documents of its sources, which must outlive it.
/// Thread affinity: IMMUTABLE after creation; the writer functions read it from any one thread.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldPartitionPlan
{
public:
	const Zenith_WorldDesc& GetWorld() const;
	Zenith_Span<const Zenith_WorldCellPlan> GetCells() const;           // (grid, z, x) order
	uint32 GetLayerSceneCount() const;
	const Zenith_PersistentID& GetLayerSceneLayer(uint32 uIndex) const; // layer id, layer name order
	bool HasContentScene() const;                                       // unstreamed mode
	Zenith_Span<const Zenith_WorldRegionCoord> GetNavigationRegions() const;   // (z, x) order
	// AI's products of Partition.md "Navigation input scenes" step 3, with their intermediate blobs (name, bytes) in the plan.
	uint32 GetNavigationProductCount() const;
	const Zenith_WorldProduct& GetNavigationProduct(uint32 uIndex) const;
	Zenith_Span<const Zenith_WorldIntermediateBlob> GetNavigationProductBlobs(uint32 uIndex) const;
	uint32 GetHlodNodeCount() const;
	const Zenith_WorldPartitionReport& GetReport() const;
	Zenith_Span<const Zenith_WorldProduct> GetProducts() const;         // key byte order, derived GUIDs
	// Every mesh a Zenith.Render.MeshInstance of a BAKED layer names, ascending GUID: a superset of the meshes fingerprint item 4
	// names, since membership needs the summaries' spheres (the editor's summary request).
	Zenith_Span<const Zenith_PersistentID> GetLightingMeshes() const;
	// The read table: every storage path the plan read, with the file digest of its first open (path order); the editor's
	// pinned access compares every later open with it (Editing.md "World editor extension").
	Zenith_Span<const Zenith_WorldReadDocument> GetReadDocuments() const;
};
struct Zenith_WorldReadDocument { Zenith_StringView m_strStoragePath; Zenith_Sha256Value m_xDigest; };   // declared before the plan
struct Zenith_WorldIntermediateBlob { Zenith_StringView m_strName; Zenith_ByteSpan m_xBytes; };   // declared before the plan

/// @contract functions Zenith_WorldPartitionFunctions
/// Ownership: The plan and every written byte live in the caller's arena or array.
/// Thread affinity: OWNER_THREAD: each call runs entirely on its caller's thread and starts no job; the writers may be called
///   for different cells from different threads concurrently, since they only read the immutable plan.
/// Invalidation: None beyond the arena.
/// Units: None.
/// Errors: Every finding goes to xDiagnostics; the first ERROR's code is returned after every finding of its step; nothing is
///   returned on error; WORLD_ERROR_BAKE_CANCELLED when m_pfnIsCancelled returned true. The writers fail only with
///   WORLD_ERROR_OUT_OF_MEMORY or the SCENE edit's error as WORLD_ERROR_PREFAB_RESOLUTION.
/// Version: since Z2 Stage 4. The algorithm is Partition.md "Partition algorithm"; its output bytes are versioned by
///   uWORLD_IMPORTER_VERSION.
/// @endcontract
Zenith_Result<Zenith_WorldPartitionPlan*, Zenith_WorldError> Zenith_PlanWorldPartition(
	const Zenith_WorldPartitionInput& xInput, Zenith_Arena& xArena, Zenith_WorldDiagnostics& xDiagnostics);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldCellDocument(const Zenith_WorldPartitionPlan& xPlan,
	uint32 uCell, Zenith_Arena& xScratch, Zenith_Array<char>& xOut);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldLayerDocument(const Zenith_WorldPartitionPlan& xPlan,
	uint32 uLayerScene, Zenith_Arena& xScratch, Zenith_Array<char>& xOut);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldContentDocument(const Zenith_WorldPartitionPlan& xPlan,
	Zenith_Arena& xScratch, Zenith_Array<char>& xOut);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldNavigationInputDocument(const Zenith_WorldPartitionPlan& xPlan,
	uint32 uNavigationRegion, Zenith_Arena& xScratch, Zenith_Array<char>& xOut);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldStreamDocument(const Zenith_WorldPartitionPlan& xPlan,
	Zenith_Array<char>& xOut);
// The editor preview: grid assignment and cell coordinates for already-resolved hierarchies, no references, no bakes.
struct Zenith_WorldAssignment { Zenith_PersistentID m_xRoot; uint8 m_uGridIndex; sint32 m_iX, m_iZ; uint16 m_uFlags; };   // flag 1 promoted, 2 bound violation
Zenith_Result<Zenith_Span<const Zenith_WorldAssignment>, Zenith_WorldError> Zenith_AssignWorldContent(
	const Zenith_WorldDesc& xWorld, Zenith_Span<const Zenith_SceneData* const> axRegions, Zenith_Span<const uint32> auRegionIndices,
	const Zenith_TypeRegistry& xTypes, Zenith_Arena& xArena);
```

## Generator types

```cpp
/// @contract value Zenith_WorldGeneratorDesc
/// Ownership: constexpr data of a generator.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGeneratorDesc
{
	const char* const m_szID = nullptr;            // "world.lattice"
	const uint32 m_uOldestVersion = 0;
	const uint32 m_uLatestVersion = 0;
	const char* const m_szDescription = nullptr;
};

/// @contract value Zenith_WorldObstacle
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: World-absolute metres (double XZ), everywhere this type appears.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldObstacle { double m_dX, m_dZ; float m_fRadius; Zenith_PersistentID m_xID; };

/// @contract value Zenith_WorldGeneratedOutput
/// Ownership: A record written into a Zenith_WorldGeneratorContext's output; views in the context's arena.
/// Thread affinity: IMMUTABLE once written. Members non-const: an element of an output array that its producer fills in
///   place.
/// Invalidation: With the context's arena.
/// Units: m_xPosition region-anchor-relative metres; m_dOwnerX, m_dOwnerZ world-absolute metres; a unit quaternion.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGeneratedOutput
{
	Zenith_StringView m_strOutputKey;
	double m_dOwnerX = 0.0, m_dOwnerZ = 0.0;   // the owner point (Generation.md "Generator terms")
	Zenith_PersistentID m_xPrefab;         // null: a plain entity with m_xEntityComponents
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	float m_fScale = 1.0f;
	Zenith_StringView m_strName;
	const Zenith_JsonDocument* m_pxEntityComponents = nullptr;   // a components map (Scene document "components" form)
	Zenith_JsonNode m_xEntityComponents;
};

/// @contract type Zenith_WorldGeneratorContext
/// Ownership: Created by Zenith_RunWorldGeneratorRegion per region job; borrowed by Generate.
/// Thread affinity: OWNER_THREAD: the region job's thread.
/// Invalidation: Every view is invalid after the job.
/// Units: Region-anchor-relative metres for positions; world-absolute metres for owner points and obstacles.
/// Errors: Emit returns WORLD_ERROR_GENERATOR_LIMIT beyond uWORLD_MAX_GENERATED_PER_REGION.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldGeneratorContext
{
public:
	Zenith_WorldRegionCoord GetRegion() const;
	void GetRegionAnchor(double adOut[3]) const;
	uint32 GetRegionSize() const;
	bool OwnsPoint(double dX, double dZ) const;            // the region's half-open rectangle
	// The document's stream of Generation.md "Seed derivation" with the counter set to uOutputOrdinal × 16 + uDraw.
	Zenith_RandomSource MakeRandom(uint64 ulOutputOrdinal, uint32 uDraw) const;
	uint64 GetPlacementSeed() const;
	const Zenith_JsonDocument& GetParametersDocument() const;
	Zenith_JsonNode GetParameters() const;
	Zenith_Span<const Zenith_WorldObstacle> GetObstacles() const;   // the job's obstacle set, sorted (x, z, ID)
	// The job's exclusion set (world.scatter; Generation.md "Obstacles and dependencies"), sorted (shape kind, entity ID).
	Zenith_Span<const Zenith_PlacementExclusion> GetExclusions() const;
	const Zenith_OpenedDocument* FindInput(Zenith_StringView strPath) const;   // inputs opened at Prepare
	const Zenith_PlacementSurface* GetSurface() const;     // null when the parameters name none
	Zenith_Result<void, Zenith_WorldError> Emit(const Zenith_WorldGeneratedOutput& xOutput);
	Zenith_Arena& GetScratch();
};

class Zenith_WorldGenerator;   // Generation.md "Generator interface and registry" (declared in this header)
Zenith_Span<const Zenith_WorldGenerator* const> Zenith_GetWorldGenerators();   // ascending ID
const Zenith_WorldGenerator* Zenith_FindWorldGenerator(Zenith_StringView strID);

/// @contract value Zenith_WorldGeneratorRun
/// Ownership: Plain value of borrowed references.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGeneratorRun
{
	const Zenith_WorldDesc* m_pxWorld = nullptr;
	const Zenith_OpenedDocument* m_pxGeneratorDocument = nullptr;
	Zenith_Span<const Zenith_WorldRegionCoord> m_axRegions;  // empty: the document's whole region set
	Zenith_WorldSourceAccess* m_pxSources = nullptr;
	Zenith_Span<const Zenith_SceneData* const> m_axAuthoredRegions;    // every resolved authored region of the world, by region index
	                                                                   // (authored obstacles are world-wide, Generation.md "Obstacles and dependencies")
	Zenith_Span<const Zenith_WorldObstacle> m_axDependencyObstacles;   // outputs of dependsOn over their whole region sets, world-absolute
	Zenith_Span<const Zenith_TerrainDocument* const> m_axTerrains;     // loaded terrains a surface may name (Partition.md "Terrain loading")
	Zenith_Span<const Zenith_WorldExclusion> m_axExclusions;           // every Zenith.Environment.PlacementExclusion entity of the world's
	                                                                   // authored regions, world-absolute; Prepare keeps D's exclusionsFromLayers
};
/// @contract value Zenith_WorldExclusion
/// Ownership: Plain value; m_xShape.m_axPoints borrows the run's storage.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: With the run's storage.
/// Units: World metres (ENV's float fields, narrowed once from double); radians.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldExclusion { Zenith_PlacementExclusion m_xShape; Zenith_PersistentID m_xEntity; Zenith_PersistentID m_xLayer; };

/// @contract type Zenith_WorldGeneratorPlan
/// Ownership: Created by Zenith_PrepareWorldGenerator in the caller's arena; borrows the run's documents.
/// Thread affinity: IMMUTABLE after creation: Zenith_RunWorldGeneratorRegion may be called for distinct regions from any
///   threads concurrently.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldGeneratorPlan
{
public:
	uint32 GetRegionCount() const;
	Zenith_WorldRegionCoord GetRegion(uint32 uIndex) const;                     // (z, x) order
	const Zenith_Sha256Value& GetFingerprint(uint32 uIndex) const;             // Generation.md "Generator outputs and staleness"
};

/// @contract value Zenith_WorldGeneratorRegionResult
/// Ownership: Written by Zenith_RunWorldGeneratorRegion; spans in the arena passed to that call.
/// Thread affinity: IMMUTABLE after the call. Members non-const: its producer writes it field by field into the
///   caller's output.
/// Invalidation: With that arena.
/// Units: As Zenith_WorldGeneratedOutput.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldGeneratorRegionResult
{
	Zenith_WorldRegionCoord m_xRegion;
	Zenith_Span<const Zenith_WorldGeneratedOutput> m_axOutputs;    // output key order
	Zenith_Sha256Value m_xFingerprint;
};

/// @contract functions Zenith_WorldGeneratorFunctions
/// Ownership: Results in the caller's arenas.
/// Thread affinity: OWNER_THREAD per call; no call starts a job. Zenith_RunWorldGeneratorRegion is a work item: concurrent
///   calls for distinct regions of one plan, each with its own arena and diagnostics, are safe.
/// Invalidation: None.
/// Units: None.
/// Errors: W30 to W33 and W36 findings (Prepare); WORLD_ERROR_GENERATOR_FAILED, WORLD_ERROR_GENERATOR_LIMIT (a region job);
///   nothing partial is returned.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<Zenith_WorldGeneratorPlan*, Zenith_WorldError> Zenith_PrepareWorldGenerator(const Zenith_WorldGeneratorRun& xRun,
	Zenith_Arena& xArena, Zenith_WorldDiagnostics& xDiagnostics);
Zenith_Result<void, Zenith_WorldError> Zenith_RunWorldGeneratorRegion(const Zenith_WorldGeneratorPlan& xPlan,
	uint32 uRegion, Zenith_Arena& xArena, Zenith_WorldGeneratorRegionResult& xOut);
// Prepare, then every region job inline in plan order; the result spans live in xArena.
Zenith_Result<Zenith_Span<const Zenith_WorldGeneratorRegionResult>, Zenith_WorldError> Zenith_RunWorldGenerator(
	const Zenith_WorldGeneratorRun& xRun, Zenith_Arena& xArena, Zenith_WorldDiagnostics& xDiagnostics);
// Writes one baked region's generated scene document (canonical bytes, at uSceneSchemaVersion) from a region result.
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldGeneratedDocument(const Zenith_WorldDesc& xWorld,
	const Zenith_PersistentID& xGenerator, const Zenith_PersistentID& xSceneID, const Zenith_WorldGeneratorRegionResult& xRegion,
	uint32 uSceneSchemaVersion, Zenith_Arena& xScratch, Zenith_Array<char>& xOut);

/// @contract type Zenith_WorldPlanePlacementSurface
/// Ownership: A plain object; borrowed by Zenith_Place for one call.
/// Thread affinity: ANY_THREAD (pure reads).
/// Invalidation: None.
/// Units: Metres.
/// Errors: None; Sample always succeeds with normal +Y and no hole.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldPlanePlacementSurface final : public Zenith_PlacementSurface
{
public:
	explicit Zenith_WorldPlanePlacementSurface(float fHeight);
	bool Sample(float fX, float fZ, uint32 uRuleSetIndex, Zenith_PlacementSurfaceSample& xOut) const override;
	float SampleLayerWeight(float fX, float fZ, uint32 uLayerIndex) const override;   // 0
	float SampleDensity(float fX, float fZ, uint32 uFoliageType) const override;      // 1
	bool IsUnderWater(float fX, float fZ) const override;                               // false
	uint32 GetMissingInputs() const override;   // LAYERS | DENSITY | EXCLUSION_MASKS
};
```

## Fingerprint builder

```cpp
/// @contract type Zenith_WorldFingerprintBuilder
/// Ownership: A plain object wrapping a Zenith_Sha256Stream; allocates nothing.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Every value little-endian with a fixed width; text and bytes as a uint32 byte length then the bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. The byte sequences each bake kind feeds it are Bakes.md "Input fingerprints" and Generation.md
///   "Generator outputs and staleness"; they are persisted through the documents that record them.
/// @endcontract
class Zenith_WorldFingerprintBuilder
{
public:
	void AddU32(uint32 uValue);
	void AddU64(uint64 ulValue);
	void AddF32Bits(float fValue);
	void AddF64Bits(double dValue);
	void AddID(const Zenith_PersistentID& xID);
	void AddDigest(const Zenith_Sha256Value& xDigest);
	void AddHash128(const Zenith_Hash128& xHash);
	void AddText(Zenith_StringView strText);
	void AddBytes(Zenith_ByteSpan xBytes);   // as AddText: a uint32 length, then the bytes
	Zenith_Sha256Value Finish() const;
private:
	Zenith_Sha256Stream m_xStream;
};
void Zenith_FormatWorldFingerprint(const Zenith_Sha256Value& xValue, Zenith_Array<char>& xOut);   // 64 lowercase hex
```

## Bake planning functions

```cpp
/// @contract enum Zenith_WorldBakeKind, Zenith_WorldBakeUnitState
/// Ownership: Plain enumerations.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are stable (the bake report carries their names).
/// @endcontract
enum Zenith_WorldBakeKind : uint8
{
	WORLD_BAKE_KIND_GENERATOR = 1, WORLD_BAKE_KIND_LIGHTING, WORLD_BAKE_KIND_NAVIGATION, WORLD_BAKE_KIND_HLOD,
};
enum Zenith_WorldBakeUnitState : uint8
{
	WORLD_BAKE_UNIT_STATE_FRESH = 1, WORLD_BAKE_UNIT_STATE_STALE, WORLD_BAKE_UNIT_STATE_MISSING,
	WORLD_BAKE_UNIT_STATE_EDITED, WORLD_BAKE_UNIT_STATE_ORPHANED, WORLD_BAKE_UNIT_STATE_IN_COOK,
};

/// @contract value Zenith_WorldBakeUnit
/// Ownership: A record of a Zenith_WorldBakePlan.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: With the plan.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldBakeUnit
{
	Zenith_WorldBakeKind m_eKind;
	Zenith_WorldBakeUnitState m_eState;
	bool m_bSelected;
	Zenith_StringView m_strKey;           // Bakes.md "Bake units"
	Zenith_Sha256Value m_xCurrent;        // explicit kinds; zero for in-cook kinds
	Zenith_Sha256Value m_xRecorded;       // zero when MISSING
	Zenith_Span<const uint32> m_auDependsOn;   // unit indices
};

/// @contract value Zenith_WorldBakePlanInput, Zenith_WorldBakePlan, Zenith_WorldBakeUnitResult
/// Ownership: Plain values; the plan's spans live in the caller's arena.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill the input field by field, and the planner writes the
///   plan and its unit results in place.
/// Invalidation: With the arena.
/// Units: Texels, rays, milliseconds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldBakePlanInput
{
	const Zenith_WorldPartitionPlan* m_pxPartition = nullptr;   // a plan computed with fingerprints
	Zenith_WorldSourceAccess* m_pxSources = nullptr;
	uint32 m_uKindMask = 0x3;            // bit (kind − 1): which kinds to select; navigation and HLOD are reported only
	Zenith_Span<const Zenith_WorldRegionCoord> m_axScope;     // empty: the whole world
	bool m_bForce = false;
	bool m_bOverwriteEdited = false;
	Zenith_Span<const Zenith_WorldProductCookResult> m_axLastCookResults;   // navigation and HLOD products, key byte order
};
struct Zenith_WorldProductCookResult { Zenith_AssetGUID m_xProduct; bool m_bCooked; uint16 m_uCookError; };   // declared before the input
struct Zenith_WorldBakePlan
{
	Zenith_Span<const Zenith_WorldBakeUnit> m_axUnits;         // execution order (Bakes.md "Bake planner" step 4)
	uint32 m_uSelectedCount = 0;
};
struct Zenith_WorldBakeUnitResult
{
	uint32 m_uUnit;
	Zenith_WorldBakeUnitState m_eAfter;
	bool m_bFailed;
	Zenith_WorldError m_xError;
	uint64 m_ulTexels, m_ulRays;
	uint32 m_uDurationMs;
	Zenith_PersistentID m_xBackupBatch;   // the SaveDocuments batch's backup ID; null when nothing was written
};

/// @contract functions Zenith_WorldBakePlanning
/// Ownership: Results in the caller's arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: WORLD_ERROR_SOURCE_ACCESS, WORLD_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_WorldError> Zenith_PlanWorldBakes(const Zenith_WorldBakePlanInput& xInput,
	Zenith_Arena& xArena, Zenith_WorldBakePlan& xOut);
// Bakes.md "Lighting fingerprint": P (items 1 to 7), the mesh list M with digests, and F. A pure function of the plan's
// documents and the access's digests, independent of access order.
struct Zenith_WorldMeshDigest { Zenith_PersistentID m_xMesh; Zenith_Sha256Value m_xDigest; };
struct Zenith_WorldLightingFingerprint
{
	Zenith_Sha256Value m_xInputDigest;                       // P
	Zenith_Span<const Zenith_WorldMeshDigest> m_axMeshes;    // M, ascending mesh GUID
	Zenith_Sha256Value m_xValue;                             // F = Zenith_FinishWorldLightingFingerprint(P, M)
};
Zenith_Result<Zenith_WorldLightingFingerprint, Zenith_WorldError> Zenith_ComputeWorldLightingFingerprint(
	const Zenith_WorldPartitionPlan& xPlan, uint32 uCell, Zenith_WorldSourceAccess& xSources, Zenith_Arena& xArena);
Zenith_Sha256Value Zenith_FinishWorldLightingFingerprint(const Zenith_Sha256Value& xInputDigest,
	Zenith_Span<const Zenith_WorldMeshDigest> axMeshes);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldBakeReport(const Zenith_WorldBakePlan& xPlan,
	Zenith_Span<const Zenith_WorldBakeUnitResult> axResults, Zenith_Array<char>& xOut);
```

## Lighting bake functions

```cpp
/// @contract value Zenith_WorldMeshGeometry
/// Ownership: Plain value; spans borrow the geometry source's storage for the bake.
/// Thread affinity: IMMUTABLE. Members non-const: the geometry source builds it field by field and keeps it in its own
///   storage.
/// Invalidation: When the bake's finish phase returns.
/// Units: Object-space metres; unit normals; UV1 in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldMeshGeometry
{
	Zenith_Span<const Zenith_Vector3> m_axPositions;
	Zenith_Span<const Zenith_Vector3> m_axNormals;
	Zenith_Span<const Zenith_Vector2> m_axUV1;              // empty: the mesh has no UV1
	Zenith_Span<const uint32> m_auIndices;                 // LOD 0, three per triangle
	Zenith_Span<const uint16> m_auTriangleMaterialSlot;    // one per triangle
	Zenith_Span<const Zenith_PersistentID> m_axSlotMaterials;   // the mesh's material per slot
	Zenith_Sha256Value m_xLod0Digest;                           // GEOM's Zenith_ComputeMeshGeometryDigest of exactly this geometry (row WD30)
};

/// @contract interface Zenith_WorldMeshGeometrySource
/// Ownership: Implemented by the editor over GEOM's CPU reader of the cook server's cooked files (row WD9), each file read
///   once whole and its digest computed from the decoded geometry (row WD30), and by tests over in-memory meshes with a
///   settable digest; borrowed for one unit's prepare phase.
/// Thread affinity: OWNER_THREAD: called only by Zenith_PrepareWorldLightingUnit, which copies every triangle it needs.
/// Invalidation: Spans stay valid until the prepare call returns.
/// Units: None.
/// Errors: Prepare returns WORLD_ERROR_BAKE_GEOMETRY naming the first mesh that cannot be read.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldMeshGeometrySource
{
public:
	virtual ~Zenith_WorldMeshGeometrySource() = default;
	virtual Zenith_Result<void, Zenith_WorldError> Prepare(Zenith_Span<const Zenith_PersistentID> axMeshes) = 0;
	virtual const Zenith_WorldMeshGeometry* Get(const Zenith_PersistentID& xMesh) const = 0;
};

/// @contract value Zenith_WorldLightBakeJob
/// Ownership: Plain value built by Zenith_MakeWorldLightBakeJob in the caller's arena from a partition plan's cell: the snapshot
///   of Bakes.md "Bake execution".
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: With the arena.
/// Units: Unit-local metres (relative to the cell anchor).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldBakeInstance
{
	Zenith_PersistentID m_xEntity, m_xMesh;
	Zenith_PersistentID m_axMaterialOverrides[8];
	float m_afLocalToUnit[12];            // 3 × 4 row-major
	bool m_bReceiver;
};
struct Zenith_WorldBakeLight
{
	uint8 m_uType;                       // 0 directional, 1 point, 2 spot (LIGHT's record types)
	Zenith_Vector3 m_xPosition, m_xDirection, m_xColour;
	float m_fIntensity;                   // candela (point, spot) or lux (directional), after LIGHT's unit rules
	float m_fRange, m_fInnerCos, m_fOuterCos;
	Zenith_PersistentID m_xEntity;
};
struct Zenith_WorldBakeProbe { Zenith_PersistentID m_xEntity; Zenith_Vector3 m_xCentre; float m_fCaptureEV100; bool m_bInterior; };
struct Zenith_WorldLightBakeJob
{
	Zenith_WorldCellBounds m_xCell;
	Zenith_StringView m_strUnitKey;
	Zenith_WorldLightingSettings m_xSettings;                    // copied
	Zenith_Span<const Zenith_WorldBakeInstance> m_axInstances;   // ascending entity ID
	Zenith_Span<const Zenith_WorldBakeLight> m_axLights;         // ascending entity ID; the sun last
	Zenith_Span<const Zenith_WorldBakeProbe> m_axProbes;         // ascending entity ID
	Zenith_Span<const Zenith_Vector3> m_axTerrainTriangles;      // unit-local, three per triangle
	double m_dContentMinY, m_dContentMaxY;
	Zenith_WorldLightingFingerprint m_xFingerprint;              // the pinned plan's P, M and F for this unit
};

/// @contract type Zenith_WorldLightBakeState
/// Ownership: Created by Zenith_PrepareWorldLightingUnit in the caller's arena: the packed atlas, the BVH, the triangle
///   records and the unit's output arrays; released with the arena.
/// Thread affinity: ANY_THREAD for Zenith_TraceWorldLightingItem with distinct item indices (each item writes only its own
///   elements of the output arrays); the caller establishes happens-before between every item's completion and
///   Zenith_FinishWorldLightingUnit (the editor through each item's operation handle completing, tests through RunRoot
///   returning). IMMUTABLE otherwise.
/// Invalidation: With the arena.
/// Units: Unit-local metres; lux; nits.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldLightBakeState
{
public:
	uint32 GetWorkItemCount() const;   // Bakes.md "Tracing", work items, in their order
	uint64 GetTriangleCount() const;
	// F recomputed from the job's P and the digests of the geometry Prepare decoded (Bakes.md "Bake execution").
	const Zenith_Sha256Value& GetTracedFingerprint() const;
	Zenith_Span<const Zenith_PersistentID> GetChangedMeshes() const;   // decoded digest differs from M
};

/// @contract type Zenith_WorldLightBakeResult
/// Ownership: In the caller's arena: pages, bindings, the volume and the probes' radiance.
/// Thread affinity: IMMUTABLE after the finish phase. Members non-const: the finish phase writes the bindings, the volume
///   and the probes into the caller's arena in place.
/// Invalidation: With the arena.
/// Units: Lux (irradiance), unit vectors, nits (probe radiance), unit-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldLightmapBinding { Zenith_PersistentID m_xInstance; uint16 m_uPage; float m_afScale[2], m_afBias[2]; };
struct Zenith_WorldBakedVolume { uint32 m_auDims[3]; Zenith_Vector3 m_xMinCorner, m_xSpacing; Zenith_Span<const float> m_afSH; Zenith_Span<const uint8> m_auValidity; };
struct Zenith_WorldBakedProbe { Zenith_PersistentID m_xEntity; Zenith_Vector3 m_xCaptureOffset; float m_fCaptureEV100; bool m_bInterior; Zenith_Span<const float> m_afRadiance; };
class Zenith_WorldLightBakeResult
{
public:
	uint32 GetPageCount() const;
	Zenith_Span<const float> GetPageIrradiance(uint32 uPage) const;     // 3 per texel
	Zenith_Span<const float> GetPageDirection(uint32 uPage) const;      // 3 per texel
	Zenith_Span<const float> GetPageDirectionality(uint32 uPage) const;
	Zenith_Span<const uint8> GetPageCoverage(uint32 uPage) const;
	Zenith_Span<const Zenith_WorldLightmapBinding> GetBindings() const;   // ascending instance ID
	bool HasVolume() const;
	const Zenith_WorldBakedVolume& GetVolume() const;
	Zenith_Span<const Zenith_WorldBakedProbe> GetProbes() const;
	uint64 GetTracedTexels() const;
	uint64 GetTracedRays() const;
	const Zenith_Sha256Value& GetTracedFingerprint() const;   // written as the header's fingerprint
};

/// @contract functions Zenith_WorldLightingFunctions
/// Ownership: Results in the caller's arena or array.
/// Thread affinity: OWNER_THREAD for Make, Prepare, Finish, Write and Read; Zenith_TraceWorldLightingItem as
///   Zenith_WorldLightBakeState states. No function starts a job, waits or blocks.
/// Invalidation: None.
/// Units: As the types.
/// Errors: WORLD_ERROR_ATLAS_FULL, WORLD_ERROR_BAKE_GEOMETRY (Prepare); WORLD_ERROR_BAKE_CANCELLED (pfnIsCancelled returned true
///   between rows: the item stops and Finish returns the error, nothing returned); WORLD_ERROR_OUT_OF_MEMORY; the reader:
///   W42 to W44 findings.
/// Version: since Z2 Stage 4. The output is versioned by uWORLD_LIGHT_TRACER_VERSION.
/// @endcontract
Zenith_Result<Zenith_WorldLightBakeJob, Zenith_WorldError> Zenith_MakeWorldLightBakeJob(
	const Zenith_WorldPartitionPlan& xPlan, uint32 uCell, Zenith_WorldSourceAccess& xSources, Zenith_Arena& xArena);
struct Zenith_WorldAtlasPlacement { Zenith_PersistentID m_xInstance; uint16 m_uPage; uint16 m_uSide; uint16 m_uX, m_uY; };
Zenith_Result<Zenith_Span<const Zenith_WorldAtlasPlacement>, Zenith_WorldError> Zenith_PackWorldLightmapAtlas(
	Zenith_Span<const Zenith_PersistentID> axInstances, Zenith_Span<const double> adAreas, const Zenith_WorldLightingSettings& xSettings,
	Zenith_Arena& xArena, uint32& uPageCountOut);
Zenith_Result<Zenith_WorldLightBakeState*, Zenith_WorldError> Zenith_PrepareWorldLightingUnit(const Zenith_WorldLightBakeJob& xJob,
	Zenith_WorldMeshGeometrySource& xGeometry, Zenith_Arena& xArena);
void Zenith_TraceWorldLightingItem(Zenith_WorldLightBakeState& xState, uint32 uItem, bool (*pfnIsCancelled)(void*),
	void* pCancelUserData);
Zenith_Result<Zenith_WorldLightBakeResult*, Zenith_WorldError> Zenith_FinishWorldLightingUnit(
	const Zenith_WorldLightBakeState& xState, Zenith_Arena& xArena);
// Prepare, every item inline in item order, Finish: the single-threaded convenience form.
Zenith_Result<Zenith_WorldLightBakeResult*, Zenith_WorldError> Zenith_BakeWorldLightingUnit(const Zenith_WorldLightBakeJob& xJob,
	Zenith_WorldMeshGeometrySource& xGeometry, bool (*pfnIsCancelled)(void*), void* pCancelUserData,
	Zenith_Arena& xArena);
// The header and one document per page, canonical bytes, with the derived IDs of Documents.md "Baked lighting documents".
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldLightBakeDocuments(const Zenith_WorldLightBakeJob& xJob,
	const Zenith_WorldLightBakeResult& xResult, const Zenith_PersistentID& xWorld, Zenith_Arena& xScratch,
	Zenith_Array<char>& xHeaderOut, Zenith_Span<Zenith_Array<char>> axPagesOut);
// The importer's reader: header and pages back into a result view (decoded payloads in the arena).
Zenith_Result<Zenith_WorldLightBakeResult*, Zenith_WorldError> Zenith_ReadWorldLightBakeDocuments(
	const Zenith_OpenedDocument& xHeader, Zenith_Span<const Zenith_OpenedDocument* const> apxPages, Zenith_Arena& xArena,
	Zenith_Sha256Value& xFingerprintOut, uint32& uTracerVersionOut, Zenith_WorldDiagnostics& xDiagnostics);
```

## HLOD functions

```cpp
/// @contract value Zenith_WorldHlodSlot, Zenith_WorldHlodNode
/// Ownership: Records of a Zenith_WorldHlodResult, in its arena.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: With the arena.
/// Units: Absolute metres for the anchor; node-anchor-relative transforms.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldHlodSourceInstance
{
	Zenith_PersistentID m_xEntity, m_xMesh;
	uint8 m_uSourceLod;
	float m_afNodeFromInstance[12];
	Zenith_PersistentID m_axMaterialOverrides[8];
	uint32 m_uTriangles;
};
struct Zenith_WorldHlodSlot
{
	bool m_bFlatten;
	uint32 m_uTriangleTarget;
	float m_fMaxError;
	Zenith_Span<const Zenith_WorldHlodSourceInstance> m_axInstances;   // ascending entity ID
	Zenith_Span<const Zenith_PersistentID> m_axMaterials;              // keep mode: the slot's materials, section order
	Zenith_PersistentID m_xMeshGUID;                                   // the hlod: product's derived GUID
};
struct Zenith_WorldHlodNode
{
	uint8 m_uGridIndex, m_uLevel;
	sint32 m_iX, m_iZ;
	double m_adAnchor[3];
	bool m_bCastsShadows;
	Zenith_Span<const Zenith_WorldHlodSlot> m_axSlots;                 // 1 … 4
};
struct Zenith_WorldHlodResult { Zenith_Span<const Zenith_WorldHlodNode> m_axNodes; uint32 m_uDroppedMaterials = 0; };

/// @contract functions Zenith_WorldHlodFunctions
/// Ownership: Results in the caller's arena or array.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: WORLD_ERROR_LIMIT (more than 65,536 nodes), WORLD_ERROR_OUT_OF_MEMORY; the recipe writer's error from GEOM's writer
///   (row WD8) as WORLD_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 4. The algorithm is Bakes.md "HLOD workflow".
/// @endcontract
Zenith_Result<void, Zenith_WorldError> Zenith_BuildWorldHlodNodes(const Zenith_WorldPartitionPlan& xPlan,
	const Zenith_WorldHlodSettings& xSettings, Zenith_Arena& xArena, Zenith_WorldHlodResult& xOut);
Zenith_Result<void, Zenith_WorldError> Zenith_WriteWorldHlodRecipe(const Zenith_WorldHlodNode& xNode, uint32 uSlot,
	Zenith_Array<uint8>& xOut);
```

## Navigation document functions

```cpp
/// @contract functions Zenith_WorldNavigationFunctions
/// Ownership: The derived document is built into the caller's arena as a Zenith_JsonDocument.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None beyond the arena.
/// Units: Metres (double for bounds and offsets).
/// Errors: WORLD_ERROR_INVALID_DESC when navigation is disabled or the region is not a navigation region.
/// Version: since Z2 Stage 4. The derivation is Documents.md "Derived navigation documents"; AI's schema version 1 with
///   row WD23's scene entries.
/// @endcontract
// axNeighbourInputs: the navInput: product GUID of each coordinate of the 3 × 3 neighbourhood that has a navigation input scene,
// (z, x) order, with its coordinate (possibly empty: a terrain-only region); axTerrains: the navigation-enabled terrains whose
// extent intersects the region, ascending GUID, written as { "terrain": <GUID> } entries (row WD34).
struct Zenith_WorldNavigationInputRef { Zenith_WorldRegionCoord m_xRegion; Zenith_AssetGUID m_xScene; };
Zenith_Result<Zenith_JsonDocument*, Zenith_WorldError> Zenith_DeriveWorldNavigationDocument(const Zenith_WorldDesc& xWorld,
	Zenith_WorldRegionCoord xRegion, Zenith_Span<const Zenith_WorldNavigationInputRef> axNeighbourInputs,
	Zenith_Span<const Zenith_AssetGUID> axTerrains, const Zenith_AssetGUID& xSettings, Zenith_Arena& xArena);
```

## Importer declarations

```cpp
// Zenith2/Tools/ZenithCook/Internal/World/Zenith_WorldImporter.h — ZenithCook-internal
/// @contract type Zenith_WorldImporter
/// Ownership: One static instance, registered by Zenith_RegisterWorldImporters.
/// Thread affinity: ANY_THREAD (ASSET's importer contract); an import touches only its context and runs every WORLD function
///   inline on its thread (Partition.md "The world importer").
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Import returns IMPORT_ERROR_SOURCE_INVALID after reporting every WORLD finding through the context
///   (IMPORT_ERROR_OUT_OF_MEMORY for WORLD_ERROR_OUT_OF_MEMORY); the cook reports COOK_ERROR_IMPORT_FAILED (ENV-F7's shape).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Its desc is Partition.md "The world importer".
/// @endcontract
class Zenith_WorldImporter final : public Zenith_Importer
{
public:
	const Zenith_ImporterDesc& GetDesc() const override;
	Zenith_Result<void, Zenith_ImportError> Import(Zenith_ImportContext& xContext) override;
};

/// @contract type Zenith_WorldImportSourceAccess
/// Ownership: A stack object of one Import call over its context.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: When the import returns.
/// Units: None.
/// Errors: Maps the context's Zenith_ImportError into WORLD_ERROR_SOURCE_ACCESS (detail: the import error).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldImportSourceAccess final : public Zenith_WorldSourceAccess { /* the eight members of Zenith_WorldSourceAccess */ };

/// @contract type Zenith_WorldEnvironmentContributor, Zenith_WorldNavigationContributor
/// Ownership: Stack objects of one Import call; the environment contributor reads ENV's published cell content through the
///   context's ReadPublishedProperty and keeps nothing between calls.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: When the import returns.
/// Units: As Zenith_WorldCellContentWriter.
/// Errors: A property read failure as WORLD_ERROR_SOURCE_ACCESS; a malformed GUID list, or AI's error, as
///   WORLD_ERROR_INVALID_DESC with that code as detail.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_WorldEnvironmentContributor final : public Zenith_WorldCellContributor   // places ENV-102's published cell content (ruling WORLD-WDB9-WDB6)
{
public:
	// axTerrains: the world's terrain GUIDs whose environment.cellContent.<x>_<z> properties are read, ascending GUID.
	Zenith_WorldEnvironmentContributor(Zenith_ImportContext& xContext, Zenith_Span<const Zenith_AssetGUID> axTerrains);
	uint32 GetRoleMask() const override;       // gameplay | collision
	bool NeedsTerrainContent() const override;  // true
	Zenith_Result<void, Zenith_WorldError> Contribute(const Zenith_WorldCellBounds& xCell,
		Zenith_Span<const Zenith_WorldProduct> axDerivedProducts, Zenith_WorldCellContentWriter& xWriter) override;
};
class Zenith_WorldNavigationContributor final : public Zenith_WorldCellContributor    // calls Zenith_AIWriteCellNavigationContent (row WD11)
{
public:
	explicit Zenith_WorldNavigationContributor(Zenith_Arena& xArena);   // the manifest view's storage
	uint32 GetRoleMask() const override;       // navigation
	bool NeedsTerrainContent() const override;  // false
	Zenith_Result<void, Zenith_WorldError> Contribute(const Zenith_WorldCellBounds& xCell,
		Zenith_Span<const Zenith_WorldProduct> axDerivedProducts, Zenith_WorldCellContentWriter& xWriter) override;
};

Zenith_Result<void, Zenith_ImportError> Zenith_RegisterWorldImporters(Zenith_ImporterRegistry& xRegistry);
```

`Import`: (1) `OpenSourceDocument`; (2) build the access and the two contributors; (3) `Zenith_PlanWorldPartition` on the import thread; (4) report diagnostics through the code-taking `Report(eLevel, strCode, strMessage)` of row WD35, `strCode` the `Zenith_ToString` of the WORLD code (empty for INFO): first one summary WARNING per bake-staleness code present (`WORLD_WARNING_BAKE_STALE`, then `WORLD_WARNING_BAKE_MISSING`, text `<code>: <n> units (<first unit key>, …)`), so the codes PACK's release check reads always fall inside ASSET's 256-diagnostic cap per import, then every diagnostic (ERROR, WARNING, INFO) in report order, then the summary of a dropped overflow; on error return; (5) for each product in key order `DeclareDerivedProduct`, then write its intermediate blobs (`Zenith_WriteWorldCellDocument`, `Zenith_WriteWorldLayerDocument`, `Zenith_WriteWorldContentDocument`, `Zenith_WriteWorldNavigationInputDocument`, `Zenith_WriteWorldStreamDocument`, `Zenith_WriteWorldHlodRecipe`, the navigation products' blobs from the plan, the lighting intermediates of `Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting import"); (6) return.

## Editor declarations

| Header (`Zenith2/Tools/ZenithEditor/Internal/World/`) | Declares |
|---|---|
| `Zenith_WorldEditorExtension.h` | `Zenith_WorldEditorExtension final : public Zenith_EditorExtension` (`GetName` "World", `Register`) |
| `Zenith_WorldPanel.h` | `Zenith_WorldPanel`, `Zenith_WorldBakesPanel` (`Zenith_EditorPanel` implementations) |
| `Zenith_WorldRegionLoader.h` | `Zenith_WorldRegionLoader { void Update(Zenith_EditorSession&, const Zenith_EditorCameraState&); void SetActiveWorld(Zenith_EditorDocumentID); uint32 GetOpenCount() const; }` |
| `Zenith_WorldCommands.h` | one `Zenith_EditorCommand` per row of `Zenith2/Docs/Design/WORLD/Editing.md` → "World commands" |
| `Zenith_WorldEditorPlanOperation.h` | `Zenith_WorldEditorPlanOperation` (`Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension"): `RequestRefresh()`, `NoteInputChange(Zenith_Span<const Zenith_StringView> astrPaths)` (increments the input-change generation and records each path's last change), `Poll() → Zenith_EditorOperationStatus` (advances partition job → summary request → fingerprint job → generation check → publication), `GetLatest() → Zenith_WorldEditorPlanRef` (null before the first completion), `GetGeneration() → uint64`, `Cancel()`; `Zenith_WorldEditorPlan` (partition plan with its read table, `G0`, diagnostics, summary table, bake plan, in its own arena) and `Zenith_WorldEditorPlanRef` (a counted reference keeping one plan alive, at most `uWORLD_EDITOR_MAX_LIVE_PLANS` live); `MAIN_THREAD`; its jobs run at `JOB_PRIORITY_BACKGROUND_TOOL` |
| `Zenith_WorldBakeOperation.h` | `Zenith_WorldBakeOperation` (the state machine of `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake execution"): `Start(Zenith_WorldEditorPlanRef, selection, Zenith_JobSystem&) → Zenith_Result<void, Zenith_EditorError>` (refused while a read document is DIRTY or a path of the plan's read table changed after its `G0`, or while a bake or generation of the world runs), `Poll() → Zenith_EditorOperationStatus`, `Cancel()`, `GetReport()`; `MAIN_THREAD`; its jobs run at `JOB_PRIORITY_BACKGROUND_TOOL` |
| `Zenith_WorldGenerateOperation.h` | `Zenith_WorldGenerateOperation`: `Start(Zenith_WorldEditorPlanRef, generator, regions, overwriteEdited, force, Zenith_JobSystem&)`, `Poll()`, `Cancel()`, `TakeResults()`; one prepare job, then one job per region job, at `JOB_PRIORITY_BACKGROUND_TOOL` ("Bake panel and commands") |
| `Zenith_WorldEditorSourceAccess.h` | `Zenith_WorldEditorSourceAccess final : public Zenith_WorldSourceAccess` over the saved documents of the storage roots (opened on its job's thread through the roots' migrators), the asset database's source paths for GUIDs, and the plan's summary table; run mode records each path's file digest at its first open into the plan's read table and compares later opens; pinned mode (`Zenith_WorldEditorSourceAccess(roots, Zenith_WorldEditorPlanRef pinned)`) compares every open of a recorded path with the read table and records unrecorded paths in its own table |
| `Zenith_WorldEditorJobObserver.h` | `class Zenith_WorldEditorJobObserver { public: virtual ~Zenith_WorldEditorJobObserver() = default; virtual void OnJobEntry(uint32 uFunctionID, bool bOnWorker, Zenith_JobPriority ePriority) = 0; };` (`Reentrancy: NONE`, called from job threads concurrently, so an implementation synchronises itself), passed optionally (null in production) to the three operations' constructors; the function IDs `uWORLD_JOB_FN_PLAN_PARTITION` 1, `_PLAN_BAKES` 2, `_PREPARE_GENERATOR` 3, `_RUN_GENERATOR_REGION` 4, `_MAKE_LIGHT_BAKE_JOB` 5, `_PREPARE_LIGHTING_UNIT` 6, `_TRACE_LIGHTING_ITEM` 7, `_FINISH_LIGHTING_UNIT` 8, `_WRITE_LIGHT_BAKE_DOCUMENTS` 9 |
| `Zenith_WorldCookedMeshSource.h` | `Zenith_WorldCookedMeshSource final : public Zenith_WorldMeshGeometrySource` over the cook server's output tree and GEOM's reader (row WD9) |

Each carries a contract block by EDIT's convention for internal headers (`Zenith2/Docs/Design/EDIT.md` → "API rules for a tool").
