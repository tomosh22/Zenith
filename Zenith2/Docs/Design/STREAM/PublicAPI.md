# STREAM design — the ZenithStreaming public API

This file is part of the STREAM design family; the entry document is `Zenith2/Docs/Design/STREAM.md`. It declares every public header of `Zenith2/Engine/ZenithStreaming/`, with one contract block per type in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type", the internal declarations the other files of this family rely on, the script bindings, and the declarations STREAM assumes from areas that have not declared them yet. Code blocks use tabs; every fallible function returns `Zenith_Result`.

## Scope of this file

| Need | Heading |
|---|---|
| the header inventory | "Header list" |
| limits, enumerations, coordinates, group IDs | "Zenith_StreamingTypes header" |
| the error record | "Zenith_StreamingErrors header" |
| components and resources | "Zenith_StreamingComponents header" |
| the runtime object of the world kind | "Zenith_StreamingWorldAsset header" |
| encoding a world | "Zenith_StreamingWorldWriter header" |
| tools' read-only view | "Zenith_StreamingState header" |
| development overrides | "Zenith_StreamingCommands header" |
| the module's entry points and fault points | "Zenith_Module_Streaming header" |
| configuration keys, settings, the world state, the snapshot pool, the hook, the listener, the consumer, the kind loader, command kinds | "Internal declarations" |
| Luau bindings | "Script bindings" |
| what STREAM assumes from SCENE, SCRIPT, ECS, MOD, ASSET, RPROD | "Assumed declarations of other areas" |

## Header list

| Header (`Zenith2/Engine/ZenithStreaming/`) | Includes | Declares |
|---|---|---|
| `Zenith_StreamingTypes.h` | `ZenithCore/Zenith_Types.h`, `ZenithCore/Zenith_PersistentID.h` | limits; `Zenith_StreamingGridRole`, `Zenith_StreamingDistanceMetric`, `Zenith_StreamingCellState`, `Zenith_StreamingBand`, `Zenith_StreamingPersistenceMode`, `Zenith_StreamingRegionPolicy`, `Zenith_StreamingCellOverride`; `Zenith_StreamingCellCoord`, `Zenith_StreamingAbsolutePoint`, `Zenith_StreamingGroupID`; `Zenith_ToString` overloads; `Zenith_MakeStreamingGroupID` |
| `Zenith_StreamingErrors.h` | `ZenithCore/Zenith_Types.h` | `Zenith_StreamingErrorCode`, `Zenith_StreamingError`, `Zenith_ToString(Zenith_StreamingErrorCode)` |
| `Zenith_StreamingComponents.h` | `ZenithReflect/Zenith_ReflectIDs.h`, `ZenithECS/Zenith_ECSTypes.h`, `ZenithAssets/Zenith_AssetHandle.h`, the two headers above | the seven component structs, `Zenith_StreamingCellOverrideSlot`, their property-ID constants, `Zenith_RegisterStreamingTypes` |
| `Zenith_StreamingWorldAsset.h` | `ZenithCore/Zenith_Span.h`, `ZenithCore/Zenith_StringView.h`, `ZenithCore/Zenith_PersistentID.h`, `ZenithReflect/Zenith_ReflectIDs.h`, `ZenithAssets/Zenith_AssetTypes.h`, the types header | `Zenith_StreamingGridInfo`, `Zenith_StreamingCellInfo`, `Zenith_StreamingHLODNodeInfo`, `Zenith_StreamingReferenceRecord`, `Zenith_StreamingPersistentEntry`, `Zenith_StreamingWorldAsset` |
| `Zenith_StreamingWorldWriter.h` | `ZenithCore/Zenith_Span.h`, `ZenithCore/Zenith_StringView.h`, `ZenithCore/Zenith_Result.h`, `ZenithAssets/Zenith_CookedFile.h`, the world-asset and errors headers | the descriptor structs, `Zenith_StreamingValidationMessage`, `Zenith_StreamingValidationReport`, `Zenith_StreamingWorldSections`, `Zenith_ValidateStreamingWorld`, `Zenith_EncodeStreamingWorld`, `Zenith_WriteStreamingWorldFile` |
| `Zenith_StreamingState.h` | `ZenithCore/Zenith_JsonWriter.h`, the types header | `Zenith_StreamingCellView`, `Zenith_StreamingSourceStatus`, `Zenith_StreamingStats`, `Zenith_StreamingStateView` |
| `Zenith_StreamingCommands.h` | `ZenithECS/Zenith_CommandBuffer.h`, the types header | `Zenith_RecordStreamingCellOverride` |
| `Zenith_Module_Streaming.h` | `ZenithRuntime/Zenith_Module.h` | `Zenith_Module_Streaming_RegisterTypes`, `Zenith_Module_Streaming_Start`, the fault points |

## Zenith_StreamingTypes header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_PersistentID.h"

/// @contract functions Zenith_StreamingLimits
/// Ownership: Compile-time constants.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts; metres for sizes; bytes for byte limits.
/// Errors: None; the reader refuses data above a limit with STREAMING_ERROR_WORLD_LIMIT and the cook with
///   COOK_ERROR_STREAM_LIMIT.
/// Version: since Z2 Stage 4. A limit rises only with a format version bump (Formats.md "Limits").
/// @endcontract
inline constexpr uint32 uSTREAMING_MAX_GRIDS = 8;
inline constexpr uint32 uSTREAMING_MAX_CELLS = 65536;                     // per world, every grid together
inline constexpr uint32 uSTREAMING_MAX_DEPENDENCIES_PER_CELL = 32;
inline constexpr uint32 uSTREAMING_MAX_HLOD_LEVELS = 3;
inline constexpr uint32 uSTREAMING_MAX_HLOD_NODES = 65536;
inline constexpr uint32 uSTREAMING_MAX_HLOD_MESHES_PER_NODE = 4;
inline constexpr uint32 uSTREAMING_MAX_REFERENCES = 1048576;
inline constexpr uint32 uSTREAMING_MAX_REFERENCES_PER_CELL = 4096;      // as source and as target, each
inline constexpr uint32 uSTREAMING_MAX_PERSISTENT_ENTITIES = 1048576;
inline constexpr uint32 uSTREAMING_MAX_PERSISTENT_PER_CELL = 1024;
inline constexpr uint32 uSTREAMING_MAX_CELL_ENTITIES = 16384;
inline constexpr uint32 uSTREAMING_MAX_SOURCES = 16;                    // per world; further sources are ignored
inline constexpr uint32 uSTREAMING_MAX_DELTA_BYTES_PER_CELL = 65535;
inline constexpr uint32 uSTREAMING_MAX_LIVE_INSTANCES = 192;            // ceiling of stream.maxLiveInstances
inline constexpr uint32 uSTREAMING_MAX_CONCURRENT_LOADS = 48;           // ceiling of stream.maxConcurrentCellLoads
inline constexpr uint32 uSTREAMING_MAX_QUEUED_CELLS = 64;               // ceiling of stream.maxQueuedCells
// Snapshot slots per attached world: the most loads any build can hold in flight at a phase 11 (LOADING + CANCELLING +
// QUEUED ≤ the pending bound, rejected unloads included: Runtime.md "Caps"), so a restore re-requesting every in-flight load
// of any save cannot exhaust the pool (Runtime.md "Restored loads beyond the pool").
inline constexpr uint32 uSTREAMING_SNAPSHOT_SLOTS = uSTREAMING_MAX_CONCURRENT_LOADS + uSTREAMING_MAX_QUEUED_CELLS;   // 112
inline constexpr uint32 uSTREAMING_MAX_PERSISTENT_TYPES = 32;           // names in stream.persistence.componentTypes
inline constexpr uint32 uSTREAMING_MAX_RETIRING_POOLS = 16;             // retiring-pool list capacity, reserved at Start
inline constexpr uint32 uSTREAMING_PRECISION_RADIUS_METRES = 4096;      // binary32 spacing ≤ 2^−12 m below it (Runtime.md "Precision target")
inline constexpr uint32 uSTREAMING_MAX_CELL_OVERRIDES = 64;             // override slots on a world reference (Runtime.md "Overrides on the world reference")
inline constexpr uint32 uSTREAMING_TOMBSTONE_WORDS = uSTREAMING_MAX_PERSISTENT_PER_CELL / 64;   // 16: uint64 words of a cell's tombstone bits
inline constexpr sint32 iSTREAMING_MAX_CELL_COORDINATE = 1048576;       // |x| and |z| of a cell coordinate
inline constexpr uint32 uSTREAMING_MIN_CELL_SIZE_METRES = 32;
inline constexpr uint32 uSTREAMING_MAX_CELL_SIZE_METRES = 8192;
inline constexpr uint32 uSTREAMING_CELL_SIZE_STEP_METRES = 32;           // cell sizes are multiples of it
inline constexpr uint32 uSTREAMING_GRID_NAME_BYTES = 32;                // including the terminating zero
inline constexpr uint32 uSTREAMING_INVALID_CELL = 0xFFFFFFFFu;
inline constexpr uint32 uSTREAMING_INVALID_NODE = 0xFFFFFFFFu;
inline constexpr uint16 uSTREAMING_WORLD_FORMAT_VERSION = 1;
inline constexpr uint32 uSTREAMING_DELTA_FORMAT_VERSION = 1;
inline constexpr uint32 uSTREAMING_HLOD_LEVEL_FACTOR = 4;               // a level-k node covers 4^(k−1) × 4^(k−1) cells

/// @contract enum Zenith_StreamingGridRole
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value outside the enumerators read from a file is STREAMING_ERROR_WORLD_CORRUPT.
/// Version: since Z2 Stage 4. Serialized as uint8 in .zstream; a value keeps its meaning forever. The value order is the
///   role order of load ordering (Runtime.md "Load ordering and admission"): a lower value loads first among equals.
/// @endcontract
enum Zenith_StreamingGridRole : uint8
{
	STREAMING_GRID_ROLE_COLLISION = 1,    // static collision; defines the simulation region (Runtime.md "Region states and coordinated subsystems")
	STREAMING_GRID_ROLE_NAVIGATION = 2,   // navigation tiles
	STREAMING_GRID_ROLE_GAMEPLAY = 3,     // general content
	STREAMING_GRID_ROLE_AUDIO = 4,        // ambience, emitters, acoustic zones, their banks
	STREAMING_GRID_ROLE_LARGE = 5,        // large distant content with a long activation radius
};

/// @contract enum Zenith_StreamingDistanceMetric
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_StreamingGridRole.
/// Version: since Z2 Stage 4. Serialized as uint8.
/// @endcontract
enum Zenith_StreamingDistanceMetric : uint8
{
	STREAMING_DISTANCE_METRIC_XZ = 1,     // horizontal distance to the cell's XZ rectangle (a cylinder)
	STREAMING_DISTANCE_METRIC_XYZ = 2,    // distance to the cell's box including its vertical extent
};

/// @contract enum Zenith_StreamingCellState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; transitions are Runtime.md "Cell state machine". A persisted value outside the enumerators (a corrupt save) is
///   read as UNLOADED by the rebuild and counted (stream.commands.malformed).
/// Version: since Z2 Stage 4. Serialized as uint8 in Zenith.Streaming.CellStatus property 1 (Runtime.md "Persisted cell
///   status"); values never move.
/// @endcontract
enum Zenith_StreamingCellState : uint8
{
	STREAMING_CELL_STATE_UNLOADED = 1,
	STREAMING_CELL_STATE_REQUESTED,        // a load action recorded this tick; its commit is pending at the sync point
	STREAMING_CELL_STATE_LOADING,          // SCENE instance LOADING (preparation running)
	STREAMING_CELL_STATE_QUEUED,           // published; waiting for admission
	STREAMING_CELL_STATE_ACTIVATING,       // admitted this tick; resolved at the next update
	STREAMING_CELL_STATE_ACTIVE,
	STREAMING_CELL_STATE_UNLOADING,        // unload action recorded; the instance is being forgotten
	STREAMING_CELL_STATE_CANCELLING,       // unload of a LOADING instance recorded (or observed); waiting for its discarded publication
	STREAMING_CELL_STATE_FAILED,           // a published failure or a refused commit; a retry is scheduled
	STREAMING_CELL_STATE_FAILED_PERMANENT, // retries exhausted, a catalogue miss or a permanent code; reset by a rebinding (T22)
};
inline constexpr uint32 uSTREAMING_CELL_STATE_COUNT = 10;

/// @contract enum Zenith_StreamingBand
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Never serialized; ordered: a lower value is more urgent.
/// @endcontract
enum Zenith_StreamingBand : uint8
{
	STREAMING_BAND_REQUIRED = 0,     // within a source's required radius: a source is not ready while such a cell is not active
	STREAMING_BAND_ACTIVATION = 1,   // within a source's activation radius: wanted active
	STREAMING_BAND_KEEP = 2,         // within the activation radius plus the unload hysteresis: kept when already wanted
	STREAMING_BAND_PREFETCH = 3,     // within the prefetch radius: data only, presentation side
	STREAMING_BAND_NONE = 4,
};

/// @contract enum Zenith_StreamingPersistenceMode
/// Ownership: Plain value; property 1 of Zenith.Streaming.Persistent.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value outside the enumerators is treated as STREAMING_PERSISTENCE_NONE and counted once per entity.
/// Version: since Z2 Stage 4. Serialized as uint8 in scene documents and saves; values never move.
/// @endcontract
enum Zenith_StreamingPersistenceMode : uint8
{
	STREAMING_PERSISTENCE_NONE = 0,          // reloads in its authored state
	STREAMING_PERSISTENCE_DESTRUCTION = 1,   // a destruction persists across unload and reload
	STREAMING_PERSISTENCE_STATE = 2,         // destruction and the configured component state persist
};

/// @contract enum Zenith_StreamingRegionPolicy
/// Ownership: Plain value; property 1 of Zenith.Streaming.RegionBound.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An unknown value is treated as STREAMING_REGION_POLICY_FLAG_ONLY.
/// Version: since Z2 Stage 4. Serialized as uint8.
/// @endcontract
enum Zenith_StreamingRegionPolicy : uint8
{
	STREAMING_REGION_POLICY_FLAG_ONLY = 0,   // the region flag is written; the entity is kept
	STREAMING_REGION_POLICY_DESTROY = 1,     // the entity is destroyed in the tick its region becomes inactive
};

/// @contract enum Zenith_StreamingCellOverride
/// Ownership: Plain value carried by the Streaming.SetCellOverride command and stored in the world reference's override slots.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An unknown value is ignored by the playback and counted.
/// Version: since Z2 Stage 4. Serialized as uint8 in Zenith_StreamingCellOverrideSlot (Zenith.Streaming.WorldRef property 3),
///   so it is saved with the reference; values never move.
/// @endcontract
enum Zenith_StreamingCellOverride : uint8
{
	STREAMING_CELL_OVERRIDE_NONE = 0,
	STREAMING_CELL_OVERRIDE_FORCE_ACTIVE = 1,     // band REQUIRED whatever the sources
	STREAMING_CELL_OVERRIDE_FORCE_UNLOADED = 2,   // band NONE whatever the sources; every transitive dependent is forced NONE too
};

/// @contract value Zenith_StreamingCellCoord
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: Cells of the named grid.
/// Errors: None; FindCell returns uSTREAMING_INVALID_CELL for a coordinate with no cell.
/// Version: since Z2 Stage 4. Serialized field by field, little-endian (Formats.md "Cell records").
/// @endcontract
struct Zenith_StreamingCellCoord
{
	uint8 m_uGrid = 0;
	uint8 m_auReserved[3] = {};
	sint32 m_iX = 0;
	sint32 m_iZ = 0;
	friend constexpr bool operator==(const Zenith_StreamingCellCoord&, const Zenith_StreamingCellCoord&) = default;
};
static_assert(sizeof(Zenith_StreamingCellCoord) == 12);

/// @contract value Zenith_StreamingAbsolutePoint
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: Metres in the absolute world frame (CORE Maths.md "Coordinate systems"), double precision.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingAbsolutePoint
{
	double m_dX = 0.0;
	double m_dY = 0.0;
	double m_dZ = 0.0;
};

/// @contract value Zenith_StreamingGroupID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Never serialized by STREAM; RPROD's PSO cook writes the same value (row ST7), so a
///   D-SEL-HASH (open, Stage 1) change moves both at once in one cook.
/// @endcontract
struct Zenith_StreamingGroupID
{
	uint64 m_ulValue = 0;
};

/// @contract functions Zenith_StreamingTypeFunctions
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Zenith_ToString of an unknown value returns "unknown".
/// Version: since Z2 Stage 4.
/// @endcontract
// Zenith_Hash64 (seed 0) of the 16-byte cooked binary form of the cell scene's GUID ([Z2-N-CONTRACT-067]).
Zenith_StreamingGroupID Zenith_MakeStreamingGroupID(const Zenith_PersistentID& xCellScene);
constexpr const char* Zenith_ToString(Zenith_StreamingCellState eState);   // "unloaded", "requested", …
constexpr const char* Zenith_ToString(Zenith_StreamingGridRole eRole);     // "collision", "navigation", …
constexpr const char* Zenith_ToString(Zenith_StreamingBand eBand);
```

## Zenith_StreamingErrors header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_StreamingErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. When each enumerator is returned: Zenith2/Docs/Design/STREAM.md "Error enumerations".
/// Version: since Z2 Stage 4. An enumerator shipped in a fix version keeps its meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_StreamingErrorCode : uint8
{
	STREAMING_ERROR_INVALID_DESC = 1,
	STREAMING_ERROR_OUT_OF_MEMORY,
	STREAMING_ERROR_WORLD_CORRUPT,
	STREAMING_ERROR_WORLD_VERSION,
	STREAMING_ERROR_WORLD_LIMIT,
	STREAMING_ERROR_WORLD_NOT_RESIDENT,
	STREAMING_ERROR_WORLD_DUPLICATE,
	STREAMING_ERROR_WORLDREF_RUNTIME,
	STREAMING_ERROR_CELL_LOAD_FAILED,
	STREAMING_ERROR_CELL_COMMIT_REFUSED,
	STREAMING_ERROR_CELL_DUPLICATE_INSTANCE,
	STREAMING_ERROR_SNAPSHOT_POOL_FULL,
	STREAMING_ERROR_OVERRIDE_NOT_ALLOWED,
	STREAMING_ERROR_DELTA_CAPACITY,
	STREAMING_ERROR_DELTA_CORRUPT,
	STREAMING_ERROR_CONFIG_INVALID,
	STREAMING_ERROR_SCENE_STATE_UNAVAILABLE,
	STREAMING_ERROR_REGISTRATION,
};

/// @contract value Zenith_StreamingError
/// Ownership: Plain value; the struct form of Zenith2/Docs/Contracts.md "Errors and results" ([Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uCell a cell index (uSTREAMING_INVALID_CELL when none); m_uDetail the code of the layer below (an
///   Zenith_AssetErrorCode, a Zenith_SceneErrorCode, a reader check number of Formats.md "Reader checks").
/// Errors: None; it describes one. m_eCode is never 0 in a failure.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingError
{
	Zenith_StreamingErrorCode m_eCode = STREAMING_ERROR_INVALID_DESC;
	uint8 m_uGrid = 0xFF;
	uint16 m_uDetail = 0;
	uint32 m_uCell = 0xFFFFFFFFu;
	uint64 m_ulAux = 0;         // a byte offset (reader), a section kind (reader), an entity simulation ID (persistence)
};
static_assert(sizeof(Zenith_StreamingError) == 16);

constexpr const char* Zenith_ToString(Zenith_StreamingErrorCode eCode);   // "STREAMING_ERROR_…" text
```

## Zenith_StreamingComponents header

All seven components, the override slot struct and the resource are C++-registered at startup step 6 by `Zenith_RegisterStreamingTypes`, which `Zenith_Module_Streaming_RegisterTypes` calls (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder").

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingComponents.h
#pragma once
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_ECSTypes.h"                 // Zenith_ContainerRef
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithStreaming/Zenith_StreamingTypes.h"
#include "ZenithStreaming/Zenith_StreamingErrors.h"

class Zenith_TypeRegistry;
class Zenith_StreamingWorldAsset;

/// @contract value Zenith_StreamingCellOverrideSlot
/// Ownership: Plain value; the element type of Zenith.Streaming.WorldRef property 3, registered as the TYPE_KIND_STRUCT
///   "Zenith.Streaming.CellOverrideSlot".
/// Thread affinity: IMMUTABLE. Members non-const: the value lives in component rows that systems, playback and property
///   sets write in place.
/// Invalidation: None.
/// Units: m_uGrid a grid index; m_iX, m_iZ a cell coordinate of that grid; m_uOverride a Zenith_StreamingCellOverride (0 = free).
/// Errors: None; a slot naming no cell is counted by Streaming.Update (stream.overrides.unknown_cell).
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_StreamingCellOverrideSlot
{
	uint8 m_uGrid = 0;                   // 1 Grid
	uint8 m_uOverride = 0;               // 2 Override (Zenith_StreamingCellOverride; 0 = free slot)
	uint16 m_uReserved = 0;              // 3 Reserved, 0
	sint32 m_iX = 0;                     // 4 X
	sint32 m_iZ = 0;                     // 5 Z
};
static_assert(sizeof(Zenith_StreamingCellOverrideSlot) == 12);

/// @contract value Zenith_StreamingWorldRefComponent
/// Ownership: Component "Zenith.Streaming.WorldRef" stored by the ECS backend; m_xWorld holds one counted reference,
///   copied and destroyed through the lifecycle hooks ([Z2-N-CONTRACT-087]).
/// Thread affinity: As every component; properties 1 and 3 are written by commands only (3 by the Streaming.SetCellOverride
///   playback), property 2 in PHASE_LATE_SCRIPTS, property 4 by Streaming.Update in PHASE_INPUT.
/// Invalidation: None.
/// Units: m_uBindingState the world state's binding value (0 NONE, 1 BOUND, 4 UNBINDING; Runtime.md "World binding").
/// Errors: None; a reference that is not scene content is refused by Streaming.Update (STREAMING_ERROR_WORLDREF_RUNTIME).
/// Version: since Z2 Stage 4; type version 1. Stable name and property IDs are permanent.
/// @endcontract
struct Zenith_StreamingWorldRefComponent
{
	Zenith_AssetHandle<Zenith_StreamingWorldAsset> m_xWorld;   // 1 World (asset Streaming.World), PHASE_COMMANDS_ONLY
	bool m_bEnabled = true;                                     // 2 Enabled, PHASE_LATE_SCRIPTS
	uint8 m_uBindingState = 0;                                 // 4 BindingState, PHASE_INPUT, editor-hidden
	Zenith_StreamingCellOverrideSlot m_axOverrides[uSTREAMING_MAX_CELL_OVERRIDES];   // 3 Overrides, PHASE_COMMANDS_ONLY, editor-hidden
	static constexpr Zenith_PropertyID uPROP_WORLD{ 1 };
	static constexpr Zenith_PropertyID uPROP_ENABLED{ 2 };
	static constexpr Zenith_PropertyID uPROP_OVERRIDES{ 3 };
	static constexpr Zenith_PropertyID uPROP_BINDING_STATE{ 4 };
	static const Zenith_TypeID xTYPE_ID;                        // Zenith_MakeTypeID("Zenith.Streaming.WorldRef")
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_StreamingSourceComponent
/// Ownership: Component "Zenith.Streaming.Source" on any entity with Zenith.WorldTransform (the pawn, the camera, a
///   preload marker).
/// Thread affinity: As every component; properties 1–6 written in PHASE_LATE_SCRIPTS (or by commands), properties 7–13
///   written by Streaming.Update in PHASE_INPUT alone, property 14 by Streaming.Update (cleared) and by STREAM's lifecycle
///   listener at the playback of a teleport (set).
/// Invalidation: None.
/// Units: m_fRadiusScale a factor applied to every radius of every grid, clamped to [0.25, 4]; m_uGridMask bit g selects
///   grid g; m_uPriority 0 … 3; m_uMissingRequiredCells cells; m_uTeleportSequence a count; m_dPreviousX/Y/Z absolute metres
///   (double) of the source at m_ulPreviousTick (0 = none); m_bTeleportPending a teleport the next update has not seen.
/// Errors: None; out-of-range values are clamped by the property ranges.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_StreamingSourceComponent
{
	bool m_bEnabled = true;                   // 1 Enabled
	float m_fRadiusScale = 1.0f;              // 2 RadiusScale, range [0.25, 4]
	uint32 m_uGridMask = 0xFFu;              // 3 GridMask: bit g = grid g
	bool m_bPrimary = false;                  // 4 Primary: the rebase and readiness-metric source
	bool m_bDrivesHLOD = false;               // 5 DrivesHLOD: the HLOD cut is computed from it (Runtime.md "Driving source and scaling")
	uint8 m_uPriority = 0;                   // 6 Priority, range [0, 3]: the source wins a band tie
	bool m_bReady = false;                    // 7 Ready (STREAM): every required cell of its grids is ACTIVE
	uint32 m_uMissingRequiredCells = 0;      // 8 MissingRequiredCells (STREAM)
	uint32 m_uTeleportSequence = 0;          // 9 TeleportSequence (STREAM): +1 per observed teleport
	double m_dPreviousX = 0.0;                // 10 PreviousX (STREAM): absolute position read by the update of m_ulPreviousTick
	double m_dPreviousY = 0.0;                // 11 PreviousY (STREAM)
	double m_dPreviousZ = 0.0;                // 12 PreviousZ (STREAM)
	uint64 m_ulPreviousTick = 0;             // 13 PreviousTick (STREAM): 0 = no previous position
	bool m_bTeleportPending = false;          // 14 TeleportPending (STREAM's listener sets it; the update clears it)
	static constexpr Zenith_PropertyID uPROP_ENABLED{ 1 }, uPROP_RADIUS_SCALE{ 2 }, uPROP_GRID_MASK{ 3 }, uPROP_PRIMARY{ 4 },
		uPROP_DRIVES_HLOD{ 5 }, uPROP_PRIORITY{ 6 }, uPROP_READY{ 7 }, uPROP_MISSING_REQUIRED_CELLS{ 8 },
		uPROP_TELEPORT_SEQUENCE{ 9 }, uPROP_PREVIOUS_X{ 10 }, uPROP_PREVIOUS_Y{ 11 }, uPROP_PREVIOUS_Z{ 12 },
		uPROP_PREVIOUS_TICK{ 13 }, uPROP_TELEPORT_PENDING{ 14 };
	static const Zenith_TypeID xTYPE_ID;      // Zenith_MakeTypeID("Zenith.Streaming.Source")
};

/// @contract value Zenith_StreamingRegionBoundComponent
/// Ownership: Component "Zenith.Streaming.RegionBound" on a dynamic entity that must not simulate outside active cells
///   (a body, an agent, an emitter, a source's pawn). PHYS, AI and AUDIO read property 2 by stable name (rows ST8–ST10).
/// Thread affinity: As every component; property 1 by commands only, properties 2–4 by Streaming.Update in PHASE_INPUT.
/// Invalidation: None.
/// Units: m_uCellIndex the region-defining cell containing the entity (uSTREAMING_INVALID_CELL outside every cell);
///   m_ulEvaluatedTick the tick of the bound evaluation that wrote properties 2 and 3 (0 = none).
/// Errors: None.
/// Version: since Z2 Stage 4; type version 1. Properties 2–4 are serialized: the DESTROY transition compares with them
///   (Runtime.md "The region").
/// @endcontract
struct Zenith_StreamingRegionBoundComponent
{
	uint8 m_uPolicy = STREAMING_REGION_POLICY_FLAG_ONLY;   // 1 Policy, PHASE_COMMANDS_ONLY
	bool m_bInActiveRegion = false;                         // 2 InActiveRegion, PHASE_INPUT
	uint32 m_uCellIndex = uSTREAMING_INVALID_CELL;         // 3 CellIndex, PHASE_INPUT
	uint64 m_ulEvaluatedTick = 0;                          // 4 EvaluatedTick, PHASE_INPUT, editor-hidden
	static constexpr Zenith_PropertyID uPROP_POLICY{ 1 }, uPROP_IN_ACTIVE_REGION{ 2 }, uPROP_CELL_INDEX{ 3 },
		uPROP_EVALUATED_TICK{ 4 };
	static const Zenith_TypeID xTYPE_ID;                    // Zenith_MakeTypeID("Zenith.Streaming.RegionBound")
};

/// @contract value Zenith_StreamingPersistentComponent
/// Ownership: Component "Zenith.Streaming.Persistent", authored in cell scenes by WORLD's partition cook (Formats.md
///   "Cell scene rules"); every property is written by commands only.
/// Thread affinity: As every component.
/// Invalidation: None.
/// Units: m_xCellScene the persistent ID of the cell scene the entity is authored in (content-stable across re-cooks,
///   Runtime.md "Cell identity across re-cooks"); m_xEntityID the entity's own persistent ID (a copy the cook verifies).
/// Errors: None; an inconsistent value is a cook error (COOK_ERROR_STREAM_PERSISTENT_ID); a mode outside the enumerators is
///   treated as STREAMING_PERSISTENCE_NONE and counted (stream.persistence.unknown_modes).
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_StreamingPersistentComponent
{
	uint8 m_uMode = STREAMING_PERSISTENCE_STATE;     // 1 Mode
	Zenith_PersistentID m_xCellScene;                 // 2 CellScene: the cell scene's persistent ID
	Zenith_PersistentID m_xEntityID;                  // 3 EntityID
	static constexpr Zenith_PropertyID uPROP_MODE{ 1 }, uPROP_CELL_SCENE{ 2 }, uPROP_ENTITY_ID{ 3 };
	static const Zenith_TypeID xTYPE_ID;              // Zenith_MakeTypeID("Zenith.Streaming.Persistent")
};

/// @contract value Zenith_StreamingCellAnchorComponent
/// Ownership: Component "Zenith.Streaming.CellAnchor" on the one root entity of every cell scene (Formats.md "Cell scene
///   rules"); property 2 is written by the Streaming.ApplyCellState playback; every property by commands only.
/// Thread affinity: As every component.
/// Invalidation: None.
/// Units: m_xCellScene the cell scene's persistent ID (the cell's content-stable identity); m_uInstance a
///   Zenith_SceneInstanceID value (0 until the anchor's cell was applied).
/// Errors: None.
/// Version: since Z2 Stage 4; type version 1. Saved: a restore rebuilds the cell table from it (Runtime.md "Save and restore").
/// @endcontract
struct Zenith_StreamingCellAnchorComponent
{
	Zenith_PersistentID m_xCellScene;                 // 1 CellScene: the cell scene's persistent ID
	uint32 m_uInstance = 0;                          // 2 Instance
	Zenith_PersistentID m_xWorldID;                   // 3 WorldID: the streaming world asset's GUID
	static constexpr Zenith_PropertyID uPROP_CELL_SCENE{ 1 }, uPROP_INSTANCE{ 2 }, uPROP_WORLD_ID{ 3 };
	static const Zenith_TypeID xTYPE_ID;              // Zenith_MakeTypeID("Zenith.Streaming.CellAnchor")
};

/// @contract value Zenith_StreamingCellDeltaComponent
/// Ownership: Component "Zenith.Streaming.CellDelta" on a cell's holder: the runtime entity Streaming.Update spawns in the
///   first load action of a cell that has none (Runtime.md "Persisted cell status"), which also carries
///   Zenith.Streaming.CellStatus; property 4's block belongs to the world's container heap
///   (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ContainerHeap header"); every property is written by commands only.
/// Thread affinity: As every component.
/// Invalidation: None.
/// Units: m_xCellScene the cell scene's persistent ID (the delta's content-stable key); m_xBytes a container of uint8
///   holding the encoding of Formats.md "Cell delta encoding".
/// Errors: None; a decode failure is STREAMING_ERROR_DELTA_CORRUPT at apply time.
/// Version: since Z2 Stage 4; type version 1. Saved and restored like every component; m_uFormatVersion selects the decoder.
/// @endcontract
struct Zenith_StreamingCellDeltaComponent
{
	Zenith_PersistentID m_xCellScene;                 // 1 CellScene: the cell scene's persistent ID
	Zenith_PersistentID m_xWorldID;                   // 2 WorldID
	uint32 m_uFormatVersion = uSTREAMING_DELTA_FORMAT_VERSION;   // 3 FormatVersion
	Zenith_ContainerRef m_xBytes;                     // 4 Bytes: container<u8>, at most uSTREAMING_MAX_DELTA_BYTES_PER_CELL
	static constexpr Zenith_PropertyID uPROP_CELL_SCENE{ 1 }, uPROP_WORLD_ID{ 2 }, uPROP_FORMAT_VERSION{ 3 }, uPROP_BYTES{ 4 };
	static const Zenith_TypeID xTYPE_ID;              // Zenith_MakeTypeID("Zenith.Streaming.CellDelta")
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};

/// @contract value Zenith_StreamingCellStatusComponent
/// Ownership: Component "Zenith.Streaming.CellStatus" on a cell's holder, beside Zenith.Streaming.CellDelta: the cell's
///   persisted status (Runtime.md "Persisted cell status"), saved with the holder so that a restore resumes it exactly.
/// Thread affinity: As every component; every property has authority PHASE_INPUT: Streaming.Update writes it directly; the
///   Streaming.ApplyCellState, Streaming.BindReferences and Streaming.UnbindReferences playbacks, STREAM's lifecycle listener
///   and the delta-merge buffer's last record write the properties Runtime.md names for them at a sync point.
/// Invalidation: None.
/// Units: m_uState a Zenith_StreamingCellState; m_uInstance a Zenith_SceneInstanceID value; ticks for m_ulRetryTick and
///   m_ulUnloadDueTick (0 = none); bit i of the tombstone arrays is the cell's persistent entry i.
/// Errors: None; a value outside its range read back from a save is clamped by the rebuild and counted
///   (stream.commands.malformed).
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_StreamingCellStatusComponent
{
	uint8 m_uState = STREAMING_CELL_STATE_UNLOADED;              // 1 State
	uint8 m_uRetriesUsed = 0;                                    // 3 RetriesUsed
	uint8 m_uCapacityFailures = 0;                               // 4 CapacityFailures
	uint8 m_uTransientFailures = 0;                              // 5 TransientFailures
	uint16 m_uFlags = 0;                                         // 6 Flags (bits of Runtime.md "Persisted cell status")
	bool m_bDeltaMergePending = false;                            // 11 DeltaMergePending
	uint8 m_uReserved = 0;
	uint32 m_uInstance = 0;                                      // 2 Instance
	uint32 m_uReserved2 = 0;
	uint64 m_ulRetryTick = 0;                                    // 7 RetryTick
	uint64 m_ulUnloadDueTick = 0;                                // 8 UnloadDueTick
	uint64 m_aulTombstoneBits[uSTREAMING_TOMBSTONE_WORDS] = {};  // 9 TombstoneBits (committed)
	uint64 m_aulPendingTombstoneBits[uSTREAMING_TOMBSTONE_WORDS] = {};   // 10 PendingTombstoneBits
	static constexpr Zenith_PropertyID uPROP_STATE{ 1 }, uPROP_INSTANCE{ 2 }, uPROP_RETRIES_USED{ 3 }, uPROP_CAPACITY_FAILURES{ 4 },
		uPROP_TRANSIENT_FAILURES{ 5 }, uPROP_FLAGS{ 6 }, uPROP_RETRY_TICK{ 7 }, uPROP_UNLOAD_DUE_TICK{ 8 },
		uPROP_TOMBSTONE_BITS{ 9 }, uPROP_PENDING_TOMBSTONE_BITS{ 10 }, uPROP_DELTA_MERGE_PENDING{ 11 };
	static const Zenith_TypeID xTYPE_ID;                          // Zenith_MakeTypeID("Zenith.Streaming.CellStatus")
};
static_assert(sizeof(Zenith_StreamingCellStatusComponent) == 288);

/// @contract functions Zenith_StreamingTypeRegistration
/// Ownership: Records declarations into the caller's registry.
/// Thread affinity: MAIN_THREAD, startup step 6 (the registry's declaration phase).
/// Invalidation: None.
/// Units: None.
/// Errors: Declarations are validated by Zenith_TypeRegistry::Freeze; the function itself returns
///   STREAMING_ERROR_REGISTRATION only when the registry is not in its declaration phase.
/// Version: since Z2 Stage 4.
/// @endcontract
// Registers, in this order: the struct Zenith.Streaming.CellOverrideSlot, the seven components above (authority per
// property as commented), the world resource type
// "Zenith.Streaming.State" (TYPE_KIND_RESOURCE, Zenith_TypeBuilder::ResourceOwningPhase(PHASE_INPUT)), and nothing else.
Zenith_Result<void, Zenith_StreamingError> Zenith_RegisterStreamingTypes(Zenith_TypeRegistry& xTypes);
```

| Stable name | Prop | Name | Type | Default | Authority | Flags |
|---|---|---|---|---|---|---|
| `Zenith.Streaming.WorldRef` | 1 | World | ASSET `Streaming.World` | null | `PHASE_COMMANDS_ONLY` | serialized |
| | 2 | Enabled | BOOL | true | `PHASE_LATE_SCRIPTS` | serialized |
| | 3 | Overrides | FIXED_ARRAY of 64 STRUCT `Zenith.Streaming.CellOverrideSlot` (Grid U8, Override U8, Reserved U16, X I32, Z I32) | all zero | `PHASE_COMMANDS_ONLY` | serialized, editor-hidden |
| | 4 | BindingState | U8 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| `Zenith.Streaming.Source` | 1 | Enabled | BOOL | true | `PHASE_LATE_SCRIPTS` | serialized |
| | 2 | RadiusScale | F32 | 1.0, range [0.25, 4] | `PHASE_LATE_SCRIPTS` | serialized, has range |
| | 3 | GridMask | U32 | 0xFF | `PHASE_LATE_SCRIPTS` | serialized |
| | 4 | Primary | BOOL | false | `PHASE_LATE_SCRIPTS` | serialized |
| | 5 | DrivesHLOD | BOOL | false | `PHASE_LATE_SCRIPTS` | serialized |
| | 6 | Priority | U8 | 0, range [0, 3] | `PHASE_LATE_SCRIPTS` | serialized, has range |
| | 7 | Ready | BOOL | false | `PHASE_INPUT` | serialized |
| | 8 | MissingRequiredCells | U32 | 0 | `PHASE_INPUT` | serialized |
| | 9 | TeleportSequence | U32 | 0 | `PHASE_INPUT` | serialized |
| | 10, 11, 12 | PreviousX, PreviousY, PreviousZ | F64 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 13 | PreviousTick | U64 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 14 | TeleportPending | BOOL | false | `PHASE_INPUT` | serialized, editor-hidden |
| `Zenith.Streaming.RegionBound` | 1 | Policy | U8 | 0 | `PHASE_COMMANDS_ONLY` | serialized |
| | 2 | InActiveRegion | BOOL | false | `PHASE_INPUT` | serialized |
| | 3 | CellIndex | U32 | 0xFFFFFFFF | `PHASE_INPUT` | serialized |
| | 4 | EvaluatedTick | U64 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| `Zenith.Streaming.Persistent` | 1 | Mode | U8 | 2 | `PHASE_COMMANDS_ONLY` | serialized |
| | 2 | CellScene | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| | 3 | EntityID | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| `Zenith.Streaming.CellAnchor` | 1 | CellScene | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| | 2 | Instance | U32 | 0 | `PHASE_COMMANDS_ONLY` | serialized |
| | 3 | WorldID | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| `Zenith.Streaming.CellDelta` | 1 | CellScene | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| | 2 | WorldID | PERSISTENT_ID | null | `PHASE_COMMANDS_ONLY` | serialized |
| | 3 | FormatVersion | U32 | 1 | `PHASE_COMMANDS_ONLY` | serialized |
| | 4 | Bytes | CONTAINER of U8, max 65,535 | empty | `PHASE_COMMANDS_ONLY` | serialized |
| `Zenith.Streaming.CellStatus` | 1 | State | U8 | 1 (UNLOADED) | `PHASE_INPUT` | serialized, editor-hidden |
| | 2 | Instance | U32 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 3 | RetriesUsed | U8 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 4 | CapacityFailures | U8 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 5 | TransientFailures | U8 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 6 | Flags | U16 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 7 | RetryTick | U64 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 8 | UnloadDueTick | U64 | 0 | `PHASE_INPUT` | serialized, editor-hidden |
| | 9 | TombstoneBits | FIXED_ARRAY of 16 U64 | all zero | `PHASE_INPUT` | serialized, editor-hidden |
| | 10 | PendingTombstoneBits | FIXED_ARRAY of 16 U64 | all zero | `PHASE_INPUT` | serialized, editor-hidden |
| | 11 | DeltaMergePending | BOOL | false | `PHASE_INPUT` | serialized, editor-hidden |

Every property STREAM's runtime writes is serialized, because a subsequent tick reads it and a restore must resume it (`Zenith2/Docs/Design/STREAM/Runtime.md` → "What is saved"); the ones only STREAM writes are `PROPERTY_FLAG_EDITOR_HIDDEN` as well, so no editor surface offers them for authoring. A cooked cell scene carries their defaults: `RegionBound` property 4 at 0 makes a freshly activated entity's first evaluation a non-transition, and `Source` property 13 at 0 gives a spawned source no previous position. The script names follow the Hungarian mapping (`Zenith.Streaming.Source.Ready` is `ready`).

Property 4 of `Zenith.Streaming.CellDelta` is declared through ZenithReflect's C++ container element form (row ST4; the ECS design half is X364): `Property(Zenith_StreamingCellDeltaComponent::uPROP_BYTES, "Bytes", &Zenith_StreamingCellDeltaComponent::m_xBytes).ElementType(PROPERTY_TYPE_U8).MaxCount(65535)`. A `CONTAINER` property needs no type hooks (`Zenith2/Docs/Design/ECS/Reflect.md` → "Freeze validation", rule 7), so `Zenith.Streaming.CellDelta` registers with null hooks; a C++ `CONTAINER` property declared without `ElementType` fails `Freeze` with `REFLECT_ERROR_UNKNOWN_ELEMENT_TYPE`.

## Zenith_StreamingWorldAsset header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingWorldAsset.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"            // Zenith_TypeID, Zenith_PropertyID
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithStreaming/Zenith_StreamingTypes.h"

/// @contract value Zenith_StreamingGridInfo
/// Ownership: Plain value inside a Zenith_StreamingWorldAsset; returned by const reference.
/// Thread affinity: IMMUTABLE. Members non-const: the world reader writes the arena arrays element by element.
/// Invalidation: With its world asset.
/// Units: Metres (double for positions, float for radii); ticks for m_uUnloadDelayTicks.
/// Errors: None; validated at load (Formats.md "Reader checks").
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingGridInfo
{
	char m_acName[uSTREAMING_GRID_NAME_BYTES];   // UTF-8, zero-terminated, unique per world
	Zenith_StreamingGridRole m_eRole;
	Zenith_StreamingDistanceMetric m_eMetric;
	uint8 m_uHLODLevelCount;                     // 0 … 3
	uint8 m_uDependencyGridMask;                 // bit h: cells of this grid depend on overlapping cells of grid h
	uint32 m_uCellSizeMetres;
	uint32 m_uFirstCell;                         // global index of the grid's first cell
	uint32 m_uCellCount;
	double m_dOriginX, m_dOriginZ;                // absolute metres; cell (0, 0) starts here
	double m_dBaseY, m_dHeight;                   // vertical extent of every cell (STREAMING_DISTANCE_METRIC_XYZ; bounds)
	float m_fActivationRadius;
	float m_fRequiredRadius;
	float m_fUnloadHysteresis;
	float m_fPrefetchRadius;
	uint32 m_uUnloadDelayTicks;
	float m_fOverlapMarginFraction;               // entity positions may lie this fraction of a cell outside it
	float m_afHLODMaxDistance[uSTREAMING_MAX_HLOD_LEVELS];      // per level 1 … 3
	float m_afHLODRefineDistance[uSTREAMING_MAX_HLOD_LEVELS];   // per level 1 … 3; below it the node refines to children
	uint32 m_uWorstCaseActiveCells;              // the cook's bound for one source at RadiusScale 1
};

/// @contract value Zenith_StreamingCellInfo
/// Ownership: Plain value inside a Zenith_StreamingWorldAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the world reader writes the arena arrays element by element.
/// Invalidation: With its world asset.
/// Units: Bytes; entity counts; index ranges into the asset's tables.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingCellInfo
{
	Zenith_PersistentID m_xScene;                 // the cell scene's asset GUID (kind ASSET_KIND_SCENE)
	Zenith_PersistentID m_xAnchorEntity;          // the anchor root's persistent ID
	Zenith_StreamingCellCoord m_xCoord;
	uint32 m_uEntityCount;                       // entities of the cell scene, ≤ uSTREAMING_MAX_CELL_ENTITIES
	uint64 m_ulEstimatedBytes;                   // the cook's estimate of the cell's resident bytes (all kinds)
	uint32 m_uFirstDependency, m_uDependencyCount;
	uint32 m_uFirstDependent, m_uDependentCount;
	uint32 m_uFirstPersistent, m_uPersistentCount;
	uint32 m_uFirstIncomingReference, m_uIncomingReferenceCount;
	uint32 m_uFirstOutgoingReference, m_uOutgoingReferenceCount;   // into the outgoing index table
	uint32 m_uHLODNode;                          // the level-1 node of this cell, or uSTREAMING_INVALID_NODE
};

/// @contract value Zenith_StreamingHLODNodeInfo
/// Ownership: Plain value inside a Zenith_StreamingWorldAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the world reader writes the arena arrays element by element.
/// Invalidation: With its world asset.
/// Units: Node coordinates in units of the node's level (a level-k node at (x, z) covers base cells
///   [x·4^(k−1), (x+1)·4^(k−1)) × [z·4^(k−1), (z+1)·4^(k−1))).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingHLODNodeInfo
{
	uint8 m_uLevel;                              // 1 … 3
	uint8 m_uGrid;
	uint8 m_uMeshCount;                          // 1 … 4
	uint8 m_uFlags;                              // bit 0: casts shadows
	sint32 m_iX, m_iZ;
	uint32 m_uParent;                            // uSTREAMING_INVALID_NODE for a top node
	uint32 m_uFirstChild, m_uChildCount;         // into the node table (children are contiguous)
	Zenith_PersistentID m_axMeshes[uSTREAMING_MAX_HLOD_MESHES_PER_NODE];   // GEOM mesh GUIDs, baked in node-anchor space
};

/// @contract value Zenith_StreamingReferenceRecord
/// Ownership: Plain value inside a Zenith_StreamingWorldAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the world reader writes the arena arrays element by element.
/// Invalidation: With its world asset.
/// Units: m_uElement the fixed-array element index, 0xFFFFFFFF for a scalar property.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingReferenceRecord
{
	uint32 m_uSourceCell;
	uint32 m_uTargetCell;
	Zenith_PersistentID m_xSourceEntity;
	Zenith_PersistentID m_xTargetEntity;
	Zenith_TypeID m_xType;                        // the component holding the reference
	Zenith_PropertyID m_xProperty;                // an ENTITY property or a FIXED_ARRAY of ENTITY
	uint32 m_uElement;
};

/// @contract value Zenith_StreamingPersistentEntry
/// Ownership: Plain value inside a Zenith_StreamingWorldAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the world reader writes the arena arrays element by element.
/// Invalidation: With its world asset.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingPersistentEntry
{
	Zenith_PersistentID m_xEntity;
	uint32 m_uCell;
	uint8 m_uMode;                               // Zenith_StreamingPersistenceMode
	uint8 m_auReserved[3];
};

/// @contract type Zenith_StreamingWorldAsset
/// Ownership: Created by Zenith_StreamingWorldKindLoader when ZenithAssets loads a .zstream; shared through
///   Zenith_AssetHandle<Zenith_StreamingWorldAsset>; its tables live in one allocation from the registry's heap, released by
///   the loader's Unload.
/// Thread affinity: IMMUTABLE once activated: every member is a const read callable from any thread.
/// Invalidation: References and spans returned are valid while a handle is held.
/// Units: Metres (absolute, double); indices into the asset's tables.
/// Errors: Lookups return uSTREAMING_INVALID_CELL or empty spans; an index outside its table is ZENITH_ASSERT (engine code
///   computes indices from the asset itself).
/// Version: since Z2 Stage 4. Cooked data re-cooks on any format change (Z2-D-048).
/// @endcontract
class Zenith_StreamingWorldAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_STREAMING_WORLD;   // row ST6

	const Zenith_PersistentID& GetWorldID() const;
	uint32 GetGridCount() const;
	const Zenith_StreamingGridInfo& GetGrid(uint32 uGrid) const;
	uint8 GetGridInDependencyOrder(uint32 uPosition) const;   // dependents before their dependencies
	uint32 FindGrid(Zenith_StringView strName) const;           // 0xFFFFFFFF when absent
	uint32 GetCellCount() const;
	const Zenith_StreamingCellInfo& GetCell(uint32 uCell) const;
	uint32 FindCell(const Zenith_StreamingCellCoord& xCoord) const;   // O(1) expected
	// The cell whose scene is xScene (the content-stable cell identity of saves), uSTREAMING_INVALID_CELL when none; O(1) expected.
	uint32 FindCellByScene(const Zenith_PersistentID& xScene) const;
	// The cell of grid uGrid whose XZ rectangle (and, for XYZ grids, vertical extent) contains the absolute point.
	uint32 FindCellContaining(uint32 uGrid, const Zenith_StreamingAbsolutePoint& xPoint) const;
	Zenith_StreamingAbsolutePoint GetCellAnchor(uint32 uCell) const;  // minimum corner, base height
	Zenith_Span<const uint32> GetDependencies(uint32 uCell) const;
	Zenith_Span<const uint32> GetDependents(uint32 uCell) const;
	Zenith_Span<const Zenith_StreamingPersistentEntry> GetPersistentEntries(uint32 uCell) const;
	uint32 FindPersistentCell(const Zenith_PersistentID& xEntity) const;   // O(log n)
	Zenith_Span<const Zenith_StreamingReferenceRecord> GetIncomingReferences(uint32 uCell) const;
	Zenith_Span<const uint32> GetOutgoingReferences(uint32 uCell) const;  // indices of GetReference
	const Zenith_StreamingReferenceRecord& GetReference(uint32 uIndex) const;
	uint32 GetHLODNodeCount() const;
	const Zenith_StreamingHLODNodeInfo& GetHLODNode(uint32 uNode) const;
	Zenith_StreamingAbsolutePoint GetHLODNodeAnchor(uint32 uNode) const;
	Zenith_Span<const uint32> GetTopHLODNodes(uint32 uGrid) const;
	uint64 GetResidentBytes() const;
};
```

## Zenith_StreamingWorldWriter header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingWorldWriter.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithAssets/Zenith_CookedFile.h"          // Zenith_CookedFileWriter
#include "ZenithStreaming/Zenith_StreamingWorldAsset.h"
#include "ZenithStreaming/Zenith_StreamingErrors.h"

class Zenith_Arena;

/// @contract value Zenith_StreamingGridDesc
/// Ownership: Plain value; m_strName borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As Zenith_StreamingGridInfo; m_axDependsOn names other grids of the same desc.
/// Errors: Validated by Zenith_ValidateStreamingWorld.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingGridDesc
{
	const Zenith_StringView m_strName = {};
	const Zenith_StreamingGridRole m_eRole = STREAMING_GRID_ROLE_GAMEPLAY;
	const Zenith_StreamingDistanceMetric m_eMetric = STREAMING_DISTANCE_METRIC_XZ;
	const uint32 m_uCellSizeMetres = 128;
	const double m_dOriginX = 0.0, m_dOriginZ = 0.0, m_dBaseY = -1024.0, m_dHeight = 4096.0;
	const float m_fActivationRadius = 384.0f, m_fRequiredRadius = 128.0f, m_fUnloadHysteresis = 64.0f, m_fPrefetchRadius = 640.0f;
	const uint32 m_uUnloadDelayTicks = 0;
	const float m_fOverlapMarginFraction = 0.25f;
	const uint8 m_uHLODLevelCount = 0;
	const float m_afHLODMaxDistance[uSTREAMING_MAX_HLOD_LEVELS] = {};
	const float m_afHLODRefineDistance[uSTREAMING_MAX_HLOD_LEVELS] = {};
	const Zenith_Span<const Zenith_StringView> m_axDependsOn = {};
};

/// @contract value Zenith_StreamingCellDesc
/// Ownership: Plain value; spans borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As Zenith_StreamingCellInfo.
/// Errors: Validated by Zenith_ValidateStreamingWorld.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingCellDesc
{
	const uint8 m_uGrid = 0;                      // index into Zenith_StreamingWorldDesc::m_axGrids
	const sint32 m_iX = 0, m_iZ = 0;
	const Zenith_PersistentID m_xScene = {};
	const Zenith_PersistentID m_xAnchorEntity = {};
	const uint32 m_uEntityCount = 0;
	const uint64 m_ulEstimatedBytes = 0;
};

/// @contract value Zenith_StreamingHLODNodeDesc
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As Zenith_StreamingHLODNodeInfo.
/// Errors: Validated by Zenith_ValidateStreamingWorld.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingHLODNodeDesc
{
	const uint8 m_uGrid = 0, m_uLevel = 1, m_uFlags = 1;
	const sint32 m_iX = 0, m_iZ = 0;
	const Zenith_Span<const Zenith_PersistentID> m_axMeshes = {}; // 1 … 4
};

/// @contract value Zenith_StreamingWorldDesc
/// Ownership: Plain value; every span borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Zenith_ValidateStreamingWorld.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingWorldDesc
{
	const Zenith_PersistentID m_xWorldID = {};
	const Zenith_Span<const Zenith_StreamingGridDesc> m_axGrids = {};
	const Zenith_Span<const Zenith_StreamingCellDesc> m_axCells = {};          // any order; the encoder sorts
	const Zenith_Span<const Zenith_StreamingHLODNodeDesc> m_axHLODNodes = {};
	const Zenith_Span<const Zenith_StreamingReferenceRecord> m_axReferences = {}; // cells as indices into m_axCells
	const Zenith_Span<const Zenith_StreamingPersistentEntry> m_axPersistent = {}; // cells as indices into m_axCells
	const Zenith_Span<const uint32> m_auExplicitDependencies = {}; // pairs (dependent, dependency) beyond the grid-derived ones
};

/// @contract value Zenith_StreamingValidationMessage
/// Ownership: Plain value inside a Zenith_StreamingValidationReport; m_strText lives in the validator's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the arena passed to Zenith_ValidateStreamingWorld.
/// Units: m_uCookCode a Zenith_CookErrorCode value (row ST6); m_uCell a cell index of the desc, uSTREAMING_INVALID_CELL when
///   the finding concerns no cell.
/// Errors: None; it describes one finding.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingValidationMessage
{
	const Zenith_StreamingErrorCode m_eCode = STREAMING_ERROR_INVALID_DESC;
	const uint16 m_uCookCode = 0;
	const uint32 m_uCell = uSTREAMING_INVALID_CELL;
	const Zenith_StringView m_strText = {};
};

/// @contract type Zenith_StreamingValidationReport
/// Ownership: Owned by the caller; its messages live in the arena passed to the validator.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With the arena.
/// Units: Counts.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingValidationReport
{
	Zenith_Span<const Zenith_StreamingValidationMessage> m_axMessages;   // every finding, in Formats.md "Validation messages" order
	uint32 m_auWorstCaseActiveCells[uSTREAMING_MAX_GRIDS] = {};
	uint32 m_uWorstCaseLiveInstances = 0;                 // Σ over grids; compared with stream.maxLiveInstances
};

/// @contract functions Zenith_StreamingWorldWriting
/// Ownership: Outputs in the caller's arena or writer.
/// Thread affinity: ANY_THREAD (pure over the desc).
/// Invalidation: Section spans live in xArena.
/// Units: Bytes.
/// Errors: STREAMING_ERROR_INVALID_DESC for any validation finding (the report names them all), STREAMING_ERROR_WORLD_LIMIT
///   above a limit, STREAMING_ERROR_OUT_OF_MEMORY when xArena is exhausted (a checked failure, not the arena's fatal cap:
///   the encoder sizes its arena request first through Zenith_GetEncodedStreamingWorldBytes).
/// Version: since Z2 Stage 4. Byte-identical output for equal descs, whatever the order of m_axCells, m_axHLODNodes,
///   m_axReferences and m_axPersistent (`Zenith2/Docs/Design/STREAM/Formats.md` → "Common rules").
/// @endcontract
Zenith_Result<void, Zenith_StreamingError> Zenith_ValidateStreamingWorld(const Zenith_StreamingWorldDesc& xDesc,
	Zenith_Arena& xArena, Zenith_StreamingValidationReport& xOut);
uint64 Zenith_GetEncodedStreamingWorldBytes(const Zenith_StreamingWorldDesc& xDesc);
/// @contract value Zenith_StreamingWorldSections
/// Ownership: Plain value; each span borrows bytes in the arena passed to Zenith_EncodeStreamingWorld.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With that arena.
/// Units: Bytes, uncompressed; index i is section kind i + 1 (Formats.md "Sections").
/// Errors: None.
/// Version: since Z2 Stage 4. Eight entries while uSTREAMING_WORLD_FORMAT_VERSION is 1.
/// @endcontract
struct Zenith_StreamingWorldSections
{
	Zenith_ByteSpan m_axSections[8];                       // indexed by section kind − 1 (Formats.md "Sections")
};
Zenith_Result<void, Zenith_StreamingError> Zenith_EncodeStreamingWorld(const Zenith_StreamingWorldDesc& xDesc,
	Zenith_Arena& xArena, Zenith_StreamingWorldSections& xOut);
// Encodes, then adds the eight sections to xWriter (zstd level 3 for sections above 4 KiB, NONE otherwise) for the caller
// to Finish; the writer's desc carries magic ZSTW, uSTREAMING_WORLD_FORMAT_VERSION and ASSET_KIND_STREAMING_WORLD.
Zenith_Result<void, Zenith_StreamingError> Zenith_WriteStreamingWorldFile(const Zenith_StreamingWorldDesc& xDesc,
	Zenith_CookedFileWriter& xWriter, Zenith_Arena& xArena);
```

## Zenith_StreamingState header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingState.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_JsonWriter.h"
#include "ZenithStreaming/Zenith_StreamingTypes.h"

class Zenith_WorldSimulation;

/// @contract value Zenith_StreamingCellView
/// Ownership: Plain value copied out of a world's streaming state (a diagnostic view; the saved component is
///   Zenith.Streaming.CellStatus, Zenith_StreamingCellStatusComponent, a different thing).
/// Thread affinity: IMMUTABLE.
/// Invalidation: A snapshot at the call.
/// Units: Ticks; bytes; metres.
/// Errors: None.
/// Version: since Z2 Stage 4. Diagnostics only.
/// @endcontract
struct Zenith_StreamingCellView
{
	const uint32 m_uCell = 0;
	const Zenith_StreamingCellState m_eState = {};
	const Zenith_StreamingBand m_eBand = {};
	const Zenith_StreamingCellOverride m_eOverride = {};
	const uint8 m_uRetries = 0;
	const uint32 m_uInstance = 0;       // the SCENE instance, 0 when none
	const uint64 m_ulStateSinceTick = 0;
	const float m_fDistance = 0.0f;      // the effective distance of the last update
	const uint32 m_uDeltaBytes = 0;
};

/// @contract value Zenith_StreamingSourceStatus
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: A snapshot at the call.
/// Units: Metres (absolute); counts.
/// Errors: None.
/// Version: since Z2 Stage 4. Diagnostics only.
/// @endcontract
struct Zenith_StreamingSourceStatus
{
	const uint64 m_ulSimulationID = 0;
	const Zenith_StreamingAbsolutePoint m_xPosition = {};
	const bool m_bReady = false;
	const bool m_bPrimary = false;
	const uint32 m_uMissingRequiredCells = 0;
	const uint32 m_uTeleportSequence = 0;
};

/// @contract value Zenith_StreamingStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: A snapshot at the call.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_StreamingStats
{
	uint32 m_auCellsPerState[uSTREAMING_CELL_STATE_COUNT];
	uint32 m_uInFlight;
	uint32 m_uLiveInstances;
	uint32 m_uSources;
	uint32 m_uHLODDisplayed;
	uint64 m_ulResidentBytes;
	uint32 m_uRebaseCount;
	bool m_bBound;                       // a world reference is bound
};

/// @contract type Zenith_StreamingStateView
/// Ownership: A borrowed view constructed by Zenith_GetStreamingStateView over one world; never stored past the call that
///   obtained it.
/// Thread affinity: MAIN_THREAD with no frame of the world running (Zenith_WorldSimulation::IsInFrame() false).
/// Invalidation: Invalid when the world advances or is destroyed.
/// Units: As the view and status structs.
/// Errors: Zenith_GetStreamingStateView returns false for a world STREAM is not attached to.
/// Version: since Z2 Stage 4. Tools and diagnostics; no simulation code uses it.
/// @endcontract
class Zenith_StreamingStateView
{
public:
	Zenith_StreamingStats GetStats() const;
	uint32 GetCellCount() const;
	Zenith_StreamingCellView GetCell(uint32 uCell) const;
	uint32 GetSourceCount() const;
	Zenith_StreamingSourceStatus GetSource(uint32 uIndex) const;
	void WriteDump(Zenith_JsonWriter& xWriter) const;   // zenith2.stream-dump v1 (Formats.md "Stream dump format")
private:
	friend bool Zenith_GetStreamingStateView(Zenith_WorldSimulation&, Zenith_StreamingStateView&);
	const void* m_pState = nullptr;
};

bool Zenith_GetStreamingStateView(Zenith_WorldSimulation& xWorld, Zenith_StreamingStateView& xOut);
```

## Zenith_StreamingCommands header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_StreamingCommands.h
#pragma once
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithStreaming/Zenith_StreamingTypes.h"

/// @contract functions Zenith_StreamingCommandRecording
/// Ownership: Records into the caller's buffer.
/// Thread affinity: OWNER_THREAD — the buffer's.
/// Invalidation: None.
/// Units: None.
/// Errors: None at record time; the playback refuses every override with STREAMING_ERROR_OVERRIDE_NOT_ALLOWED (logged,
///   stream.overrides.refused) in a Shipping build, when stream.debug.allowOverrides is false, while the world is not bound
///   or when the bound reference's 64 slots are full; Streaming.Update counts a slot naming no cell (stream.overrides.unknown_cell).
/// Version: since Z2 Stage 4. Development and test use; the command kind exists in every build.
/// @endcontract
// Records the module command "Streaming.SetCellOverride" (phases 1–10). Its playback writes the override into the bound
// world reference's override slots (Zenith.Streaming.WorldRef property 3, Runtime.md "Overrides on the world reference"),
// so it is saved with the reference; the override applies from the next Streaming.Update.
void Zenith_RecordStreamingCellOverride(Zenith_CommandBuffer& xBuffer, const Zenith_StreamingCellCoord& xCell,
	Zenith_StreamingCellOverride eOverride);
```

## Zenith_Module_Streaming header

```cpp
// Zenith2/Engine/ZenithStreaming/Zenith_Module_Streaming.h
#pragma once
#include "ZenithRuntime/Zenith_Module.h"

/// @contract functions Zenith_Module_Streaming
/// Ownership: The module instance is owned by ZenithRuntime as a Zenith_Owned<Zenith_ModuleInstance>.
/// Thread affinity: MAIN_THREAD: RegisterTypes at startup step 6, Start at step 15.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ModuleError: MODULE_ERROR_REGISTRATION (a registration refused; the detail is logged under stream.world),
///   MODULE_ERROR_OUT_OF_MEMORY. Every construction step unwinds what it built ([Z2-N-CONTRACT-340]).
/// Version: since Z2 Stage 4. Module interface version uZENITH_MODULE_INTERFACE_VERSION (1).
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Streaming_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Streaming_Start(
	Zenith_ModuleStartContext& xContext);

// Fault points (prefix 0x5357, "SW").
// In the one step order of the prose below and of Zenith2/Docs/Design/STREAM.md "Construction and shutdown": Start steps 2 to 7
// are 0x53570001 to 0x53570006; AttachWorld steps 2, 3, 5, 6, 7, 8 are 0x53570010 to 0x53570015 (steps 1 and 4 have no fault point).
inline constexpr uint32 uFAULT_POINT_STREAMING_MODULE_INSTANCE = 0x53570001u;     // Start: the instance allocation
inline constexpr uint32 uFAULT_POINT_STREAMING_RETIRING_LIST = 0x53570002u;      // Start: the retiring-pool list
inline constexpr uint32 uFAULT_POINT_STREAMING_MEMORY_CONSUMER = 0x53570003u;     // Start: RegisterMemoryConsumer
inline constexpr uint32 uFAULT_POINT_STREAMING_KIND_LOADER = 0x53570004u;         // Start: RegisterKindLoader
inline constexpr uint32 uFAULT_POINT_STREAMING_COMMAND_KINDS = 0x53570005u;       // Start: the four command kinds
inline constexpr uint32 uFAULT_POINT_STREAMING_BINDINGS = 0x53570006u;            // Start: the binding table
inline constexpr uint32 uFAULT_POINT_STREAMING_WORLD_STATE = 0x53570010u;         // AttachWorld: the world state (step 2)
inline constexpr uint32 uFAULT_POINT_STREAMING_PRESENTATION = 0x53570011u;        // AttachWorld: the presentation state
inline constexpr uint32 uFAULT_POINT_STREAMING_SYSTEMS = 0x53570012u;             // AttachWorld: the three nodes
inline constexpr uint32 uFAULT_POINT_STREAMING_LISTENER = 0x53570013u;            // AttachWorld: the lifecycle listener
inline constexpr uint32 uFAULT_POINT_STREAMING_SNAPSHOT_POOL = 0x53570014u;       // AttachWorld: the world's snapshot pool
inline constexpr uint32 uFAULT_POINT_STREAMING_HOOK = 0x53570015u;                // AttachWorld: SetActivationHook (refused)
```

`Zenith_Module_Streaming_RegisterTypes` calls `Zenith_RegisterStreamingTypes(xTypes)` and registers the configuration keys of "Internal declarations" through `xTypes.GetConfigSchema()`; it registers nothing else ([Z2-N-ARCH-848]).

`Zenith_Module_Streaming_Start` performs, in order, each step consulting the start context's fault injector at its point and unwinding every previous step on failure: (1) read `Zenith_StreamingSettings` from `xContext.GetConfig()` (a configuration error is `MODULE_ERROR_REGISTRATION` with `STREAMING_ERROR_CONFIG_INVALID` logged, naming the key); (2) allocate the instance (`Zenith_TryNew`, fault point 0x53570001); (3) reserve the retiring-pool list, a `Zenith_Array<Zenith_Owned<Zenith_StreamingSnapshotPool>>` with capacity `uSTREAMING_MAX_RETIRING_POOLS` (16) that never grows afterwards (0x53570002); (4) register the memory consumer through `RegisterMemoryConsumer` (0x53570003); (5) `xContext.GetAssets().RegisterKindLoader(ASSET_KIND_STREAMING_WORLD, …)` (0x53570004); (6) register the four command kinds through `RegisterCommandKind` (0x53570005); (7) register the four bindings through `RegisterScriptBindings` (0x53570006). The renderer pointer (`GetRenderer()`, null in a headless run) is kept for the service's PSO requests.

`AttachWorld(Zenith_WorldAttachContext& xContext)` performs, in order: (1) obtain `xContext.GetSceneState()` (row ST5, X227 (b); a null result is `MODULE_ERROR_REGISTRATION` with `STREAMING_ERROR_SCENE_STATE_UNAVAILABLE`); (2) create the world state (`Zenith_StreamingWorldState::Create`, 0x53570010); (3) create the presentation state (0x53570011); (4) `AddWorldResource(Zenith.Streaming.State, PHASE_INPUT, pState)`; (5) add `Streaming.Update`, `Streaming.Service`, `Streaming.ExtractHLOD` with the declarations of `Zenith2/Docs/Design/STREAM/Runtime.md` → "Per-tick update" and "The streaming service" (0x53570012); (6) `AddLifecycleListener` (0x53570013); (7) create the world's snapshot pool (`Zenith_StreamingSnapshotPool::Create`, `uSTREAMING_SNAPSHOT_SLOTS` slots, 0x53570014); (8) `SetActivationHook` on the world's SCENE state with the pool's hook (row ST2, 0x53570015). `DetachWorld(uWorldTag)` reverses (8) to (2): it clears the hook (operations that started before keep the pointer they captured, row ST2 (d)), drops the world's simulation reference on every slot, moves the pool to the retiring list when any slot is still held by a preparation (it is destroyed by the first sweep that finds every slot FREE), destroys the presentation state (destroying every operation handle, which requests cancellation, [Z2-N-CONTRACT-091]) and destroys the world state; the runtime undoes the registrations ([Z2-N-ARCH-851]). Detach cannot fail and allocates nothing: the retiring list was reserved at Start, the list is swept before the move, and a move into a full list after the sweep is `ZENITH_CHECK` (16 pools retiring at once would mean 16 world detaches inside the lifetime of one cancelled preparation, whose slices end it within frames). The retiring list is also swept at every `AttachWorld` and at the instance's destruction, which follows shutdown step 4 (every preparation has ended by then) and `ZENITH_CHECK`s that every retiring pool is FREE.

## Internal declarations

### Configuration keys

Every key is registered with `m_uAllowedLayerMask` of engine default, platform, game and command line (not quality tier, not user), and `m_bRequiresRestart` true unless stated. "Sim" marks `m_bSimulationAffecting`.

| Key | Type | Default | Range | Sim | Meaning |
|---|---|---|---|---|---|
| `stream.maxConcurrentCellLoads` | INT | 16 | 1 … 48 | yes | cells in REQUESTED, LOADING or CANCELLING at once (Z2-B-STREAM-003) |
| `stream.maxQueuedCells` | INT | 32 | 1 … 64 | yes | QUEUED cells beyond the in-flight cap: loads are issued only while in-flight + QUEUED + UNLOADING cells whose unload was of a QUEUED instance < this + `stream.maxConcurrentCellLoads` (Runtime.md "Caps") |
| `stream.maxLiveInstances` | INT | 160 | 1 … 192 | yes | STREAM's share of SCENE's 256 additive instances per world |
| `stream.maxNewRequestsPerTick` | INT | 4 | 1 … 32 | yes | load actions per tick |
| `stream.activation.maxCellsPerTick` | INT | 4 | 1 … 32 | yes | admissions per tick |
| `stream.activation.maxEntitiesPerTick` | INT | 4096 | 64 … 65,536 | yes | entities admitted per tick (pending row need) |
| `stream.unload.maxCellsPerTick` | INT | 8 | 1 … 64 | yes | unload actions per tick |
| `stream.cell.retryTicks` | INT | 600 | 1 … 36,000 | yes | ticks before a FAILED cell is retried |
| `stream.cell.maxRetries` | INT | 3 | 0 … 16 | yes | content-failure retries before FAILED_PERMANENT |
| `stream.cell.capacityRetryTicks` | INT | 30 | 1 … 3,600 | yes | first backoff after a capacity or transient failure, doubling per consecutive one up to `stream.cell.retryTicks` (Runtime.md "Failure classification") |
| `stream.cell.maxTransientRetries` | INT | 4 | 0 … 16 | yes | consecutive transient failures (`ASSET_ERROR_IO`, `NOT_INSTALLED`, an unknown dependency cause) retried without consuming a retry before they count as content failures |
| `stream.priority.forwardBias` | FLOAT | 0.25 | 0 … 0.9 | yes | the direction bias β of Runtime.md "Distance and band computation" |
| `stream.teleport.distanceMetres` | FLOAT | 256 | 16 … 65,536 | yes | a one-tick move above it counts as a teleport |
| `stream.teleport.criticalTicks` | INT | 60 | 0 … 3,600 | no | booster priority boost after a teleport |
| `stream.rebase.enabled` | BOOL | false | — | yes | origin rebasing |
| `stream.rebase.thresholdMetres` | FLOAT | 2048 | 512 … 65,536 | yes | rebase trigger T (Runtime.md "Rebase rule": derived from the 4,096 m precision radius) |
| `stream.rebase.quantumMetres` | INT | 2048 | 128 … 8,192 (multiple of 128, at most 2 × T) | yes | shift granularity q |
| `stream.rebase.minIntervalTicks` | INT | 30 | 1 … 36,000 | yes | minimum ticks N between shifts |
| `stream.rebase.vertical` | BOOL | false | — | yes | whether Y is rebased too |
| `stream.persistence.componentTypes` | STRING | empty | comma-separated stable names, ≤ 1,024 bytes, ≤ 32 names; never `Zenith.WorldTransform` or `Zenith.PreviousWorldTransform` | yes | the component types whose presence and eligible properties STATE persistence captures |
| `stream.prefetch.lookaheadSeconds` | FLOAT | 2.0 | 0 … 10 | no | velocity lookahead of the prefetch disc (restart not required) |
| `stream.service.frameBudgetUs` | INT | 1000 | 100 … 8,000 | no | the service's per-frame time budget (from Z2-B-STREAM-002) |
| `stream.hlod.enabled` | BOOL | true | — | no | HLOD display (restart not required) |
| `stream.hlod.hysteresisMetres` | FLOAT | 64 | 0 … 1,024 | no | residency hysteresis of HLOD bands |
| `stream.pso.gateTimeoutFrames` | INT | 300 | 1 … 3,600 | no | frames after which a cell's PSO gate opens regardless |
| `stream.debug.allowOverrides` | BOOL | true (non-Shipping), false (Shipping) | — | yes | cell overrides honoured |
| `stream.debug.view` | INT | −1 | −1 … 7 | no | the grid the debug view draws; −1 none (restart not required) |

### Zenith_StreamingSettings

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingConfig.h
struct Zenith_StreamingSettings        // read once at Start; immutable afterwards
{
	uint32 m_uMaxConcurrentCellLoads, m_uMaxQueuedCells, m_uMaxLiveInstances, m_uMaxNewRequestsPerTick;
	uint32 m_uActivationMaxCellsPerTick, m_uActivationMaxEntitiesPerTick, m_uUnloadMaxCellsPerTick;
	uint32 m_uCellRetryTicks, m_uCellMaxRetries, m_uCellCapacityRetryTicks, m_uCellMaxTransientRetries;
	float m_fForwardBias, m_fTeleportDistanceMetres;
	uint32 m_uTeleportCriticalTicks;
	bool m_bRebaseEnabled, m_bRebaseVertical;
	float m_fRebaseThresholdMetres;
	uint32 m_uRebaseQuantumMetres, m_uRebaseMinIntervalTicks;
	Zenith_FixedArray<Zenith_TypeID, uSTREAMING_MAX_PERSISTENT_TYPES> m_axPersistentTypes;   // resolved at Start, ascending type ID
	uint32 m_uPersistentTypeCount;
	float m_fPrefetchLookaheadSeconds;
	uint32 m_uServiceFrameBudgetUs;
	bool m_bHLODEnabled;
	float m_fHLODHysteresisMetres;
	uint32 m_uPSOGateTimeoutFrames;
	bool m_bAllowOverrides;
	sint32 m_iDebugViewGrid;
};
Zenith_Result<Zenith_StreamingSettings, Zenith_StreamingError> Zenith_ReadStreamingSettings(const Zenith_Config& xConfig,
	const Zenith_TypeRegistry& xTypes);   // STREAMING_ERROR_CONFIG_INVALID names the first bad key or type
```

A persistent type that is not a registered `TYPE_KIND_COMPONENT`, or is one of the derived transform types `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform`, fails `Zenith_ReadStreamingSettings`, as does a quantum above twice the threshold; a type with properties of ineligible kinds (Runtime.md "Persistent entities", table "Eligible property kinds") is accepted, and those properties are skipped at capture.

### Zenith_StreamingWorldState

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingWorldState.h
/// @contract type Zenith_StreamingWorldState
/// Ownership: Created by Create as a Zenith_Owned<Zenith_StreamingWorldState> per attached world; owned by the module
///   instance; registered as the world resource "Zenith.Streaming.State" (owning phase PHASE_INPUT).
/// Thread affinity: EXTERNALLY_SYNCHRONISED, per Zenith2/Docs/Design/STREAM.md "Thread affinity per type".
/// Invalidation: Cell arrays are re-sized only at a binding change (World binding), on Streaming.Update.
/// Units: Ticks; cell indices.
/// Errors: Create returns STREAMING_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingWorldState
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_StreamingWorldState>, Zenith_StreamingError> Create(
		uint16 uWorldTag, const Zenith_StreamingSettings& xSettings, Zenith_Heap& xHeap);
	static const Zenith_TypeID xTYPE_ID;                                   // "Zenith.Streaming.State"

	// Binding (Runtime.md "World binding")
	Zenith_AssetHandle<Zenith_StreamingWorldAsset> m_xWorld;               // null while unbound
	const Zenith_StreamingWorldAsset* m_pxWorld = nullptr;
	Zenith_EntityHandle m_xWorldRefEntity;
	uint8 m_eBinding = 0;                                                 // 0 NONE, 1 BOUND, 2 REFUSED, 3 FAILED, 4 UNBINDING

	// Per cell (Formats.md "In-memory runtime records")
	Zenith_Array<Zenith_StreamingCellRuntime> m_axCells;
	Zenith_Array<Zenith_EntityHandle> m_axPersistentHandles;               // per persistent entry; null while its cell is not active
	Zenith_Array<uint8> m_auPersistentFlags;                              // per persistent entry; bit 0 committed, bit 1 pending tombstone
	                                                                       // (caches of CellStatus properties 9 and 10)
	Zenith_Array<Zenith_StreamingCellDeltaMirror> m_axDeltas;              // per cell: holder, anchor handle, delta facts

	// Sources (at most uSTREAMING_MAX_SOURCES, ascending simulation ID)
	Zenith_FixedArray<Zenith_StreamingSourceRuntime, uSTREAMING_MAX_SOURCES> m_axSources;
	uint32 m_uSourceCount = 0;

	// This tick's admission table, read by the hook during Scene.ActivateAdditive
	Zenith_FixedArray<Zenith_StreamingAdmissionEntry, 32> m_axAdmitted;
	uint32 m_uAdmittedCount = 0;
	Zenith_StreamingAbsolutePoint m_xOriginAtUpdate;                       // Zenith.WorldOrigin as read by this tick's update (root translations;
	                                                                       // the bindings read the resource itself, "Script bindings")

	// Inboxes written by the hook on the sync-point and tick-boundary nodes, drained by the next update (tombstones and
	// teleports need none: the listener writes them into saved properties at the playback, Runtime.md "Tombstones", "Teleports")
	// Both are reserved at Create and never grow; an append into a full one is ZENITH_CHECK, unreachable by these bounds.
	Zenith_Array<uint32> m_auDiscardInbox;                                // cells (hook OnDiscardedPublication); capacity uSTREAMING_SNAPSHOT_SLOTS:
	                                                                       // each entry names a distinct cell holding a slot (adopted loads included)
	Zenith_Array<uint32> m_auCommitRefusedInbox;                          // cells (hook OnLoadCommitted); capacity stream.maxNewRequestsPerTick:
	                                                                       // one commit per load issued in the tick, drained by the next update
	Zenith_Array<Zenith_StreamingRestoredLoad> m_axRestoredLoadInbox;      // (instance, tag, slot) SAVE's restore re-requested; capacity uSTREAMING_SNAPSHOT_SLOTS
	Zenith_StreamingSnapshotPool* m_pxSnapshots = nullptr;                 // the world's pool, owned by the module instance

	// Cumulative counter totals (monotonic) and gauges written by Streaming.Update; the service differences them per frame
	Zenith_StreamingTickCounters m_xCounters;
	uint64 m_ulLastUpdateTick = 0;
	// No copy of the last rebase tick: the rebase interval reads Zenith.WorldOrigin::m_ulLastRebaseTick, which is saved.

	// Re-home caches of CellStatus flag bit 6 (Runtime.md "Cell identity across re-cooks"), rebuilt by rebuild steps R3 and R5
	uint32 m_uRehomePendingCells = 0;                                     // cells whose holder carries bit 6 (flags 3 bit 1 of the cell record)
	uint32 m_uRehomePendingOrphans = 0;                                   // orphan holders carrying bit 6
	bool m_bNeedsRebuild = true;                                           // Runtime.md "Save and restore"
};
```

### Zenith_StreamingSnapshotPool

<!-- elaboration -->
The per-world pool of preparation snapshots with atomic holder masks, retired rather than destroyed while preparations hold slots, is a design choice of this family ("Design choices at a glance" in the entry document).

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingSnapshotPool.h

// A hook tag: bits 0-31 the cell index + 1, bits 32-63 the low 32 bits of the tick that recorded the load. Unique among a
// world's live loads (one cell records at most one load per tick, and a restore continues the saved tick counter, so a
// restored tag never equals a new one within 2^32 ticks), a pure function of simulation state, and therefore stable
// across a save and its restore (SAVE re-requests a LOADING or QUEUED instance with its saved tag, row ST14). The cell
// bits are informational: after a re-cook they may name another cell, so nothing reads a cell from a tag (Runtime.md
// "Rebuild" R4 identifies a restored load's cell from its instance's scene GUID).
constexpr uint64 Zenith_MakeStreamingHookTag(uint32 uCell, uint64 ulTick)
{
	return (static_cast<uint64>(static_cast<uint32>(ulTick)) << 32) | static_cast<uint64>(uCell + 1u);
}

/// @contract type Zenith_StreamingSnapshotPool
/// Ownership: One per attached world, created at AttachWorld with uSTREAMING_SNAPSHOT_SLOTS (112) slots from the root heap,
///   each slot's entity table reserved at Create; it owns the world's Zenith_StreamingActivationHook. The module instance
///   owns the pool, moves it to its retiring list at DetachWorld while any slot is still held by a preparation, and
///   destroys it once every slot is FREE, so the hook a preparation calls is alive until its last call.
/// Thread affinity: ANY_THREAD_EXCEPT(Acquire, ReleaseSimulation, SetCell, FillEntityTable, DetachWorldState). Acquire,
///   ReleaseSimulation, SetCell and FillEntityTable run on the world's Streaming.Update node, its sync-point node or the
///   main thread during a restore; DetachWorldState on the main thread at DetachWorld; Find, IsTombstoned, GetAmendment,
///   IsGateOpen and ReleasePreparation on preparation threads; OpenGate on the world's Streaming.Update or Streaming.Service
///   node. Each slot carries one std::atomic<uint8> holder mask (bit 0 simulation, bit 1 preparation); the slot is FREE
///   when the mask is 0. Every mask change is a fetch_and/fetch_or with memory_order_acq_rel, the gate and table-filled
///   flags are a release store read with an acquire load, and the tag table is open-addressed with linear probing over
///   2 × slot count entries (256, a power of two), each entry one std::atomic<uint64> tag and a uint32 slot index:
///   inserted by Acquire with a release store before SCENE can observe the tag (the load record plays afterwards, at step
///   7), erased by compare-exchange to the deleted marker by whichever side clears the last holder bit.
/// Invalidation: A slot (and its tag) is valid while its holder mask is non-zero.
/// Units: Hook tags as Zenith_MakeStreamingHookTag; never ordered by STREAM.
/// Errors: Acquire returns STREAMING_ERROR_SNAPSHOT_POOL_FULL when no slot is FREE. A live run that started without a
///   restore cannot reach it: issuing never exceeds stream.maxConcurrentCellLoads slots carrying the simulation bit (the
///   REQUESTED, LOADING and CANCELLING cells), and a slot holding the preparation bit alone exists only after
///   DetachWorldState, in a retired pool, or after rebuild step R4 neutralised an unadopted restored load (Runtime.md "Rebuild"). After a restore, up to uSTREAMING_SNAPSHOT_SLOTS adopted loads may carry the
///   simulation bit, more than the restoring build's cap; no load is issued until the in-flight count falls below that cap
///   (Runtime.md "Caps", "After a restore"). A restore cannot reach the refusal for any save a build within the
///   configuration ranges wrote, because such a save holds at most 112 tagged loads in flight (the pending bound, rejected
///   unloads included). When reached (a corrupt save), the refusal is counted (stream.snapshots.pool_full), the load is
///   left unadopted and the rebuild releases its instance.
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingSnapshotPool
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_StreamingSnapshotPool>, Zenith_StreamingError> Create(
		uint32 uSlotCount, Zenith_StreamingWorldState& xWorldState, Zenith_Heap& xHeap);
	Zenith_StreamingActivationHook& GetHook();
	// FREE → mask 3: records the tag, the cell (uSTREAMING_INVALID_CELL for a restored load until SetCell), the gate flag
	// and, when xTable is non-null, a copy of the cell's entity table (the table-filled flag is then set); inserts the tag.
	// The lowest-index FREE slot is taken; which slot never reaches simulation state.
	Zenith_Result<uint32, Zenith_StreamingError> Acquire(uint64 ulTag, uint32 uCell,
		const Zenith_StreamingEntityTable* pxTable, bool bGateOpen);
	uint32 Find(uint64 ulTag) const;                // slot index, or 0xFFFFFFFF
	void SetCell(uint32 uSlot, uint32 uCell);                     // a restored load, once R4 identified its cell
	void FillEntityTable(uint32 uSlot, const Zenith_StreamingEntityTable& xTable);   // a restored load
	// Preparation threads; both answer from the table copy and the bound world asset's sorted persistent entries of the
	// slot's cell (binary search by persistent ID), and answer "nothing" for an entity that is not a persistent entry.
	bool IsTombstoned(uint32 uSlot, const Zenith_PersistentID& xEntity) const;
	Zenith_StreamingAmendment GetAmendment(uint32 uSlot, const Zenith_PersistentID& xEntity) const;
	bool IsGateOpen(uint32 uSlot) const;             // acquire load
	void OpenGate(uint32 uSlot);                                   // release store; only once the table is filled
	bool IsTableFilled(uint32 uSlot) const;
	uint32 GetCell(uint32 uSlot) const;
	Zenith_StreamingWorldState* GetWorldState() const; // null once the world detached
	void ReleasePreparation(uint32 uSlot);                         // clears bit 1 (OnPreparationEnded), acq_rel
	void ReleaseSimulation(uint32 uSlot);                          // clears bit 0, acq_rel
	void DetachWorldState();                                        // clears the world-state pointer and every bit 0
	bool IsEverySlotFree() const;
	uint32 GetHeldCount() const;                     // slots with a non-zero mask (diagnostics)
	uint64 GetResidentBytes() const;
};

// Internal value types of the pool (Formats.md "Snapshot slot").
struct Zenith_StreamingEntityTable            // one cell's persistent entries, by index in the cell's persistent range
{
	uint64 m_aulTombstoneBits[uSTREAMING_MAX_PERSISTENT_PER_CELL / 64];     // bit i: entry i is skipped at preparation
	uint32 m_auAddMask[uSTREAMING_MAX_PERSISTENT_PER_CELL];                 // bit t: configured type t must be present
	uint32 m_auRemoveMask[uSTREAMING_MAX_PERSISTENT_PER_CELL];              // bit t: configured type t must be absent
};
static_assert(sizeof(Zenith_StreamingEntityTable) == 8320);
struct Zenith_StreamingAmendment { uint32 m_uAddMask = 0; uint32 m_uRemoveMask = 0; };   // over the configured types
```

Each slot is a `Zenith_StreamingSnapshotSlot` (`Zenith2/Docs/Design/STREAM/Formats.md` → "In-memory runtime records"): the holder mask, the gate flag, the table-filled flag, the tag, the cell index, the frame its PSO group was requested, a pointer to its entity table (8,320 bytes, reserved at `Create`) and the world asset pointer captured at `Acquire` (the bound asset stays alive while any of its cells is not UNLOADED, and a cell is UNLOADED only after its preparation ended, "Cell state machine"). A slot whose mask reaches 0 needs no further action: `Acquire` reuses it. A load SCENE never prepares still releases both bits: a refused commit clears them at `OnLoadCommitted`, a rejected load buffer at T4, and a refused start or a pending preparation removed before it started calls `OnPreparationEnded` (row ST2 (c)). The gate opens only after the table is filled, and SCENE calls `ShouldSkipEntity` and `AmendComponents` only once the gate is open (row ST2), so a restored load whose table R4 fills can never be prepared from an empty table.

The pool's world-state pointer is cleared before the world state is destroyed, so a preparation that outlives its world reads null and the hook answers it without touching freed memory; the slot memory and the hook stay valid until every bit is clear, because the pool is retired, not destroyed, while any bit is set.

**Concurrency coverage.** The pool is STREAM's only concurrent code. ZenithStreaming is a module, hence a `ZENITH_GPU` library that the `linux-tsan` preset does not build (`Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix": module CPU-side suites run in `win-asan` instead), so TSan cannot see it. Its coverage is therefore three tests that do not depend on a race detector: `Test_StreamingSnapshotPool_EveryInterleavingOfOneSlot` drives one slot through every interleaving of its six concurrent operations (simulation release, preparation release, gate open, gate poll, tombstone and amendment queries, tag lookup) across two threads with explicit step barriers, asserting the mask, the tag table and the FREE transition after each; `Test_StreamingSnapshotPool_ConcurrentGateAndEnd` runs 100,000 cycles on 8 threads under `win-asan` for use-after-free and double-release; and the contract above names the memory order of every atomic, so review checks each against the interleaving table.

### Zenith_StreamingActivationHook

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingHook.h
/// @contract type Zenith_StreamingActivationHook
/// Ownership: One per attached world, a member of that world's Zenith_StreamingSnapshotPool; passed to the world's SCENE
///   state with SetActivationHook at attach and cleared at detach. It lives as long as its pool, which is retired rather
///   than destroyed while any preparation still holds a slot; each preparation calls the pointer it captured at its
///   commit (row ST2 (d)).
/// Thread affinity: As Zenith_SceneActivationHook's members (row ST2): OnLoadCommitted on the world's sync-point node, or
///   on the main thread when SAVE's restore re-requests a load; OnDiscardedPublication on the world's tick-boundary handler;
///   ShouldSkipEntity, AmendComponents, IsPreparationGateOpen and OnPreparationEnded on preparation threads (they touch only
///   the pool's slots, tag table and the immutable world asset), OnPreparationEnded also on the sync-point node for a
///   pending preparation removed before it started; AdmitActivation and GetRootTranslation on the world's
///   Scene.ActivateAdditive node (they read the admission table Streaming.Update wrote before it).
/// Invalidation: None.
/// Units: Hook tags as Zenith_MakeStreamingHookTag.
/// Errors: None; a tag with no slot, or a pool with no world state, answers "skip nothing", "amend nothing", "gate open"
///   and "not admitted", and is counted in stream.hook.unknown_tags; so does a discarded publication of a slot whose cell is
///   uSTREAMING_INVALID_CELL.
/// Reentrancy: NONE. It calls only the amender SCENE passes to AmendComponents, which records into the prepared scene and
///   calls nothing back.
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingActivationHook final : public Zenith_SceneActivationHook
{
public:
	void OnLoadCommitted(uint64 ulTag, Zenith_SceneInstanceID xInstance, Zenith_SceneErrorCode eError) override;
	void OnDiscardedPublication(uint64 ulTag) override;
	bool ShouldSkipEntity(uint64 ulTag, const Zenith_PersistentID& xEntity) override;
	void AmendComponents(uint64 ulTag, const Zenith_PersistentID& xEntity, Zenith_SceneComponentAmender& xAmender) override;
	bool IsPreparationGateOpen(uint64 ulTag) override;
	void OnPreparationEnded(uint64 ulTag) override;
	bool AdmitActivation(uint64 ulTag, Zenith_SceneInstanceID xInstance) override;
	Zenith_Vector3 GetRootTranslation(uint64 ulTag) override;
};
```

| Member | STREAM's body |
|---|---|
| `OnLoadCommitted` | a tag in the table, instance non-zero: the cell's instance ID is recorded and the cell moves REQUESTED → LOADING (T2; its holder is written by the next update); a tag in the table, refused: both slot bits are cleared and the cell is appended to the commit-refused inbox with the code (T3 in step 3 of the next update, after holder discovery has mapped any holder the load's own buffer spawned, which T3 then writes); a tag not in the table and instance non-zero (SAVE's restore re-requesting a load the snapshot held in flight): a slot is acquired with cell `uSTREAMING_INVALID_CELL`, the gate closed and no table, and the triple (instance, tag, slot) is appended to the restored-load inbox the rebuild reads (Runtime.md "Rebuild" R4); when the pool refuses, the triple is appended with no slot (`stream.snapshots.pool_full`); when the inbox is full, nothing is appended and `stream.restore.unadopted` counts it (R4 releases the instance either way); a tag not in the table and refused (instance 0: a restore re-request SCENE refused, row ST2 (j)): nothing is appended and `stream.restore.unadopted` counts it |
| `OnDiscardedPublication` | appends the slot's cell to the world state's discard inbox (CANCELLING → UNLOADED at the next update, T18); a tag with no slot, and a slot whose cell is `uSTREAMING_INVALID_CELL` (a restored load rebuild step R4 did not adopt), append nothing and count `stream.hook.unknown_tags` |
| `ShouldSkipEntity` | `IsTombstoned(Find(tag), id)`; false when the tag has no slot or the pool has no world state |
| `AmendComponents` | `GetAmendment(Find(tag), id)`; for each set add bit t `xAmender.AddComponent(settings.m_axPersistentTypes[t])`, for each set remove bit `xAmender.RemoveComponent(…)` (Runtime.md "Component presence"); nothing when the tag has no slot, the pool has no world state or the entity is not persistent |
| `IsPreparationGateOpen` | `IsGateOpen(Find(tag))`; true when the tag has no slot or the pool has no world state (a detached world's preparation is cancelled and must reach its end) |
| `OnPreparationEnded` | `ReleasePreparation(Find(tag))` |
| `AdmitActivation` | true when this tick's admission table holds the cell with this instance ID (binary search by instance ID) |
| `GetRootTranslation` | the admission entry's root translation (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Large coordinates and origin rebasing") |

### Zenith_StreamingLifecycleListener

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingListener.cpp (declared in Internal/Zenith_StreamingModule.h)
/// @contract type Zenith_StreamingLifecycleListener
/// Ownership: One per attached world, a member of that world's Zenith_StreamingWorldState; registered with the world's
///   lifecycle listener registry at AttachWorld step 6 and unregistered before the world state is destroyed.
/// Thread affinity: The world's sync-point node, inside the playback that dispatches the event (SCENE's Scene.Teleport,
///   ECS's destroy playback), never concurrently with Streaming.Update.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an event for an entity STREAM does not track is ignored.
/// Reentrancy: NONE. It changes world state only through the Zenith_WorldPlaybackAccess it receives (property writes,
///   [Z2-N-ARCH-544]) and records no follow-up command.
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingLifecycleListener final : public Zenith_LifecycleListener
{
public:
	uint32 GetInterestMask() const override;   // TELEPORTED | DESTROY_PENDING
	// Writes only through xWorld; never records into xFollowUp.
	void OnLifecycleEvent(const Zenith_LifecycleEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld,
		Zenith_CommandBuffer& xFollowUp) override;
};
```

| Event | STREAM's body |
|---|---|
| `LIFECYCLE_EVENT_TELEPORTED` for an entity carrying `Zenith.Streaming.Source` | `SetProperty(entity, Source, 14 TeleportPending, true)`; nothing else (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Teleports") |
| `LIFECYCLE_EVENT_DESTROY_PENDING` for an entity carrying `Zenith.Streaming.Persistent` of mode DESTRUCTION or STATE whose cell is ACTIVE or ACTIVATING with "unload in progress" clear | sets the entry's pending bit in the mirror and in `CellStatus` property 10 of the cell's holder (`ReadProperty` of the array, the bit set, `SetProperty`); `stream.persistence.tombstones` + 1 (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Tombstones") |
| any other event or entity | nothing |

The listener's method signature is SCENE's `Zenith_LifecycleListener` interface (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_LifecycleListener header"); the table above is what STREAM's implementation does with it.

### Zenith_StreamingMemoryConsumer

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingMemory.cpp (declared in the module header of Internal/)
/// @contract type Zenith_StreamingMemoryConsumer
/// Ownership: A member of the module instance; registered at Start through RegisterMemoryConsumer.
/// Thread affinity: ANY_THREAD for GetResidentBytes and GetDeviceLocalBytes (relaxed atomic sums); OnPressureLevelChanged
///   on the main thread in the frame prologue.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Reentrancy: NONE. It sets an atomic level read by each world's service; it calls nothing.
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingMemoryConsumer final : public Zenith_MemoryConsumer
{
public:
	Zenith_MemorySubsystem GetSubsystem() const override;   // MEMORY_SUBSYSTEM_STREAMING
	uint64 GetResidentBytes() const override;               // Runtime.md "Memory pressure and transition peaks"
	void OnPressureLevelChanged(Zenith_MemoryPressureLevel eOld, Zenith_MemoryPressureLevel eNew) override;
};
```

### Zenith_StreamingWorldKindLoader

```cpp
// Zenith2/Engine/ZenithStreaming/Internal/Zenith_StreamingWorldKindLoader.h
/// @contract type Zenith_StreamingWorldKindLoader
/// Ownership: A member of the module instance; registered at Start for ASSET_KIND_STREAMING_WORLD
///   (m_szName "Streaming.WorldLoader", m_uFormatVersion uSTREAMING_WORLD_FORMAT_VERSION, m_uMaxConcurrentLoads 4);
///   unregistered at shutdown step 6 before the instance is destroyed.
/// Thread affinity: ANY_THREAD (ZenithAssets' loader contract).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Fail(uLoaderCode) with a Zenith_StreamingErrorCode value: WORLD_CORRUPT, WORLD_LIMIT, OUT_OF_MEMORY.
/// Reentrancy: NONE for ZenithAssets' calls (the loader contract of Zenith2/Docs/Design/ASSET/RuntimeAPI.md
///   "Zenith_AssetKindLoader").
/// Version: since Z2 Stage 4. Internal.
/// @endcontract
class Zenith_StreamingWorldKindLoader final : public Zenith_AssetKindLoader
{
public:
	void BeginLoad(Zenith_AssetLoadContext& xContext) override;        // requests all eight sections in one round
	void OnSectionsLoaded(Zenith_AssetLoadContext& xContext, Zenith_Span<const Zenith_AssetLoadedSection> axSections) override;
	void Cancel(Zenith_AssetLoadContext& xContext) override;            // frees a partially built asset; AcknowledgeCancel
	void Unload(Zenith_AssetUnloadContext& xContext) override;          // frees the asset; AcknowledgeUnload
	uint64 DropCpuCopies() override;                     // 0: not a GPU-resident kind
};
```

`OnSectionsLoaded` parses the sections with the reader of `Zenith2/Docs/Design/STREAM/Formats.md` → "Reader checks" into one allocation (the tables, the cell hash map, the dependents table built by inverting dependencies), calls `SetResidentBytes(MEMORY_SUBSYSTEM_STREAMING, bytes, 0)` and `Activate`; any check failure is `Fail` with the code, and the allocation is released first.

### Command kinds

| Name | Payload type | Max bytes | Phase mask | Playback |
|---|---|---|---|---|
| `Streaming.ApplyCellState` | `Zenith.Streaming.ApplyCellStatePayload` | 8 | phase 1 | Runtime.md "Persistent entities" (restore), anchor instance, persistent handle table |
| `Streaming.BindReferences` | `Zenith.Streaming.CellPayload` | 8 | phase 1 | Runtime.md "Cross-cell references" (bind) |
| `Streaming.UnbindReferences` | `Zenith.Streaming.CellPayload` | 8 | phase 1 | Runtime.md "Cross-cell references" (unbind) |
| `Streaming.SetCellOverride` | `Zenith.Streaming.OverridePayload` | 16 | phases 1–10 | writes the bound world reference's override slot (or refuses, "Zenith_StreamingCommands header"; Runtime.md "Overrides on the world reference") |

Payload layouts are `Zenith2/Docs/Design/STREAM/Formats.md` → "Module command payloads"; each payload type is registered as a `TYPE_KIND_STRUCT` by `Zenith_RegisterStreamingTypes` (after the components; "Zenith_StreamingComponents header" counts them among "nothing else" because they exist only to describe payloads). Every playback validates its payload's size and ranges and counts and ignores a malformed one. Each playback finds its world's state from the world tag of the `Zenith_WorldPlaybackAccess` it receives (`GetWorldTag`, core-doc item X238; the kind table and its user data are process-wide), and writes the holder, anchor or world reference properties Runtime.md names through that access. A destroyed persistent entity needs no command kind: STREAM's lifecycle listener writes its tombstone into the cell's holder at the destroy's own playback ("Zenith_StreamingLifecycleListener").

## Script bindings

Registered by `Zenith_Module_Streaming_Start` into the binding table it passes to `RegisterScriptBindings` (`Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI"), from `Internal/Zenith_StreamingScriptBindings.cpp`:

| Luau | C++ function | Contexts | Effect | Resources | Returns |
|---|---|---|---|---|---|
| `Streaming.IsPositionActive(position: vector): boolean` | `Zenith_StreamingScriptApi::IsPositionActive(Zenith_ScriptInvocationContext&, Zenith_Vector3 xLocal)` | SYSTEM, BEHAVIOUR | READ_WORLD | `Zenith.Streaming.State`, `Zenith.WorldOrigin` | true when the region-defining cell containing the position is ACTIVE (Runtime.md "Region states and coordinated subsystems") |
| `Streaming.GetCellState(position: vector, grid: string?): string` | `Zenith_StreamingScriptApi::GetCellState(Zenith_ScriptInvocationContext&, Zenith_Vector3 xLocal, Zenith_StringID xGrid)` | SYSTEM, BEHAVIOUR | READ_WORLD | `Zenith.Streaming.State`, `Zenith.WorldOrigin` | `Zenith_ToString` of the state of the grid's cell at the position (the first GAMEPLAY grid when `grid` is nil); `"none"` when no cell |
| `Streaming.GetWorldOrigin(): (number, number, number)` | `Zenith_StreamingScriptApi::GetWorldOrigin(Zenith_ScriptInvocationContext&)` | SYSTEM, BEHAVIOUR | READ_WORLD | `Zenith.Streaming.State`, `Zenith.WorldOrigin` | the absolute origin `O` as three doubles |
| `Streaming.SetCellOverride(grid: string, x: number, z: number, override: string)` | `Zenith_StreamingScriptApi::SetCellOverride(…)` | TOOLS | BUFFERED, `uSIDE_EFFECT_MASK_FIELD_COMMAND` | — | records `Streaming.SetCellOverride` |

The three `READ_WORLD` functions read the cell table of this tick's update and the origin from the `Zenith.WorldOrigin` resource itself, never `m_xOriginAtUpdate`: in a rebase tick the shift plays at sync point 1, so phases 2 to 10 hold shifted local positions and the resource holds the shifted O, and `IsPositionActive`, `GetCellState` (absolute = O + local) and `GetWorldOrigin` answer for the same point the script means. A script in phases 2 to 10 therefore sees the cell states the phase-1 update left and the current origin; an unknown grid name raises a controlled Luau error naming the grid; a non-finite position returns false or `"none"`. The generated `.d.luau` declares them in `Zenith.Streaming` and the components as `Zenith.Streaming.Source` etc.

## Assumed declarations of other areas

### SCENE activation hook

<!-- assumption: pending SCENE (row ST2) -->
```cpp
// Zenith2/Engine/ZenithScene/Zenith_AdditiveScenes.h (additions)
/// @contract type Zenith_SceneComponentAmender
/// Ownership: Constructed by SCENE's preparation on its stack for one AmendComponents call; borrowed by the hook for that
///   call only.
/// Thread affinity: OWNER_THREAD (the preparation thread).
/// Invalidation: Invalid when AmendComponents returns.
/// Units: Component type IDs.
/// Errors: None returned; an unregistered type ID is ignored and counted (scene.amend.unknown_types); an allocation
///   failure in the operation's arena fails the preparation with SCENE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SceneComponentAmender
{
public:
	// Appends the type's default-constructed init block to the entity's init record; a no-op when the entity has the type.
	void AddComponent(Zenith_TypeID xType);
	// Drops the type's init block from the entity's init record; a no-op when the entity lacks the type.
	void RemoveComponent(Zenith_TypeID xType);
};

/// @contract interface Zenith_SceneActivationHook
/// Ownership: Implemented above ZenithScene (STREAM); borrowed by Zenith_SceneWorldState for the world's lifetime and by
///   each committed load that captured it (obligation (d) below) until that load's last call.
/// Thread affinity: Each member on the thread its comment names.
/// Invalidation: None.
/// Units: ulTag is the implementer's opaque tag from Zenith_AdditiveLoadDesc::m_ulHookTag (0 = no hook call for that load).
/// Errors: None; the implementer answers every call.
/// Reentrancy: NONE. No member calls back into SCENE, except the amender's two members during AmendComponents.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SceneActivationHook
{
public:
	virtual ~Zenith_SceneActivationHook() = default;
	// Sync-point node, Scene.LoadAdditive playback (step 7), in merge order: the commit of a load with this tag
	// (xInstance non-zero) or its refusal (eError SCENE_ERROR_LIMIT or SCENE_ERROR_DUPLICATE_INSTANCE). Also called on the
	// main thread when SAVE's restore re-requests a pending load with its saved tag, after ENTITIES were restored (xInstance 0
	// and the refusal's code when SCENE refuses the re-request, obligation (j)).
	virtual void OnLoadCommitted(uint64 ulTag, Zenith_SceneInstanceID xInstance, Zenith_SceneErrorCode eError) = 0;
	// The world's tick-boundary handler, step 1, in completion-ID order: the publication of a tagged load whose instance
	// became UNLOADED while LOADING (obligations (a) and (b)).
	virtual void OnDiscardedPublication(uint64 ulTag) = 0;
	// Preparation thread, after DEPENDENCIES and before BUILDING (a GATED state between them): false keeps the operation
	// waiting and re-polls at its next slice, which is a cancellation point; BUILDING starts only once true.
	virtual bool IsPreparationGateOpen(uint64 ulTag) = 0;
	// Preparation thread, BUILDING (so always after the gate opened), once per entity in cooked order: true removes the
	// entity and its descendants from the prepared scene; ENTITY values naming a removed entity become null and count
	// scene.references.skipped (no log line).
	virtual bool ShouldSkipEntity(uint64 ulTag, const Zenith_PersistentID& xEntity) = 0;
	// Preparation thread, BUILDING, once per entity ShouldSkipEntity kept, before the entity's init record is final: the
	// implementer adds or removes components of the entity's init record through xAmender (obligation (e)).
	virtual void AmendComponents(uint64 ulTag, const Zenith_PersistentID& xEntity, Zenith_SceneComponentAmender& xAmender) = 0;
	// Exactly once per committed load with this tag, after its last call above and before its operation parks its
	// publication, whatever the status (SUCCEEDED, FAILED, CANCELLED): on the preparation thread; on the main thread in
	// StepActivations for a start the loader refused (operation table full, out of memory); on the sync-point node in the
	// Scene.ForgetInstance playback for a pending preparation removed before it started (obligation (c)).
	virtual void OnPreparationEnded(uint64 ulTag) = 0;
	// Scene.ActivateAdditive node (phase 1), for each QUEUED instance with this tag, in instance-ID order, before its spawns
	// are recorded: false leaves it QUEUED for the next tick and does not count against scene.activationRetryTicks.
	virtual bool AdmitActivation(uint64 ulTag, Zenith_SceneInstanceID xInstance) = 0;
	// Same node, for an admitted instance: a translation SCENE adds to every root's Zenith.LocalTransform position, and to
	// every recorded Zenith.WorldTransform and Zenith.PreviousWorldTransform position, of the instance's spawns.
	virtual Zenith_Vector3 GetRootTranslation(uint64 ulTag) = 0;
};

// Zenith_AdditiveLoadDesc gains:     uint64 m_ulHookTag = 0;
// Zenith_SceneWorldState gains:      void SetActivationHook(Zenith_SceneActivationHook* pxHook);   // MAIN_THREAD, attach/detach
// Zenith_AdditiveSceneSet gains:     Zenith_Span<const Zenith_SceneInstanceID> GetLiveInstances() const;
//                                    // obligation (f)
//                                    uint8 GetErrorDetail(Zenith_SceneInstanceID xInstance) const;
//                                    // obligation (i): the FAILED record's detail, 0 when none
// Zenith_ScenePublicationSink gains: virtual void SetPublicationResolver(Zenith_PfnScenePublicationResolve pfnResolve,
//                                        void* pUserData) = 0;   // obligation (b), row ST21
// with, declared before the sink:
//   using Zenith_PfnScenePublicationResolve = void (*)(uint64 ulCompletionID, uint8& uPublicationKind,
//       uint8* pPayload, uint32 uPayloadCapacity, uint32& uPayloadBytes, void* pUserData);
// The hook tag is part of an instance's pending-load record, so SAVE saves it for a LOADING or QUEUED instance and its
// restore re-requests the load with it (rows ST14, SV10); an ACTIVE instance is restored from ENTITIES and calls no hook.
```

The obligations SCENE's post-signoff patch takes, each precise enough to apply verbatim (row ST2):

- **(a) A discarded publication for every outcome.** For every committed load with a non-zero tag whose instance becomes UNLOADED while LOADING, whoever recorded the unload, exactly one publication of its completion ID is published at a tick boundary, whatever the operation's outcome: succeeded (the prepared scene is then released unactivated), failed, cancelled, or never started (a pending preparation that the `Scene.ForgetInstance` playback removes before `StepActivations` started it, which parks the publication at once with `Park(id, PUBLICATION_KIND_GENERIC_RECORDED, {instance, SCENE_ERROR_CANCELLED, 0})`). SCENE's publication handler calls `OnDiscardedPublication(tag)` for it, with the hook captured at commit, and releases the instance record only after that call: the tick-boundary handler's step 3 does not release an UNLOADED tagged record whose discarded publication is still unpublished. Untagged loads keep today's behaviour (counted and ignored).
- **(b) Its publication kind is decided at publication, and is RECORDED.** The discarded publication is logged as `PUBLICATION_KIND_GENERIC_RECORDED` with the 8-byte `Zenith_SceneLoadResult { m_uInstance = the instance, m_uCode = SCENE_ERROR_CANCELLED, m_uDetail = 0 }` in every case, so a replay publishes it at its recorded tick and never holds the tick for a preparation the replay itself cancels. Mechanism, with no compare-exchange and no reclassification of a parked entry: (1) each tagged instance record carries a plain flag "discard requested", whose only writer is the `Scene.ForgetInstance` playback of a tagged LOADING instance on the world's sync-point node (the playback sets it, requests the operation's cancellation or removes the pending preparation, and never touches a parked result); (2) the operation's completion continuation always calls `Park` with its own result, exactly as for a load nobody unloaded, after `OnPreparationEnded` (obligation (c)); (3) SCENE installs, in `Zenith_SceneWorldState::Attach`, a publication resolver through `Zenith_ScenePublicationSink::SetPublicationResolver`, which ZenithRuntime forwards to SCRIPT's table for source kind 8 (row ST21); at every tick boundary in live mode, after the completions are drained and sorted and before the publications are logged, SCRIPT calls the resolver once per drained completion of kind 8 in completion-ID order on the tick-boundary node; SCENE's resolver looks the completion ID up in its own table and, when the record carries "discard requested", sets `uPublicationKind` to `PUBLICATION_KIND_GENERIC_RECORDED`, writes the 8 bytes `{instance, SCENE_ERROR_CANCELLED, 0}` and sets `uPayloadBytes` to 8, whatever the parked kind and payload were; any other completion it leaves unchanged. The flag is written on the sync-point node and read on the tick-boundary node of the same world, which the frame graph never runs concurrently, so no ordering between a preparation thread's `Park` and the playback can change the logged result: a result parked before the playback, after it, or never (the never-started preparation) is logged identically. (4) SCENE's handler, receiving the resolved publication, finds the record by completion ID, releases any prepared scene the operation produced, calls `OnDiscardedPublication(tag)` and only then releases the record. (5) Under a publication source (`--replay`, a seek) the resolver is not called: the logged discard is published from the log at its recorded tick, and the replay's own local completion of that ID is dropped by SCRIPT (row ST21 (c)); when that local completion arrives after the logged discard was published, the sink's `Park` does not assert on the ID SCRIPT reports already published, and SCENE releases the operation's prepared scene at its next `StepActivations` (an operation whose record no longer exists), counted in `scene.prepare.orphaned`.
- **(c) `OnPreparationEnded` exactly once per committed tagged load**, including the pending preparation removed before it started: the `Scene.ForgetInstance` playback calls it on the sync-point node before parking that load's discarded publication. A refused start calls it on the main thread in `StepActivations`, and a started preparation on its thread after its last other call and before it parks.
- **(d) The hook pointer is captured per load.** The pending-preparation record created at commit stores the world's hook pointer current at that commit; `BeginPrepare` passes it into the operation; every subsequent call for that load (the preparation-thread members, `OnPreparationEnded` and the discarded publication's `OnDiscardedPublication`) uses the captured pointer, never the world's current one. `SetActivationHook(nullptr)` at detach therefore affects only loads committed afterwards, and a world's destruction never races a running preparation's call; STREAM keeps every captured hook alive until all its slots are FREE. Calls for new simulation events (`OnLoadCommitted`, `AdmitActivation`, `GetRootTranslation`) use the world's current hook, and a null hook makes no call (the load behaves as untagged from then on).
- **(e) `AmendComponents`** is called once per entity `ShouldSkipEntity` kept, during `BUILDING`, before that entity's init record is final; the amender appends a default-constructed init block or drops one, in the operation's arena, so the entity spawns with the amended component set at the activation's sync point.
- **(f) `GetLiveInstances()`** returns the IDs of every LOADING, QUEUED, ACTIVE and FAILED record in ascending order (a span valid until the set's next change), the enumeration STREAM's rebuild after a restore needs.
- **(g) Save fidelity.** SCENE's `Zenith.Scene.AdditiveSet` participant reports COHERENT for a capture holding (1) a LOADING or QUEUED instance (as it does today), (2) an UNLOADED tagged record whose discarded publication is unpublished (such a record is not saved, so the restored world has none and STREAM rebuilds the cell UNLOADED), or (3) a tagged load whose commit the `Scene.LoadAdditive` playback refused (`SCENE_ERROR_LIMIT` or `SCENE_ERROR_DUPLICATE_INSTANCE`) at a sync point of the capture tick: the hook learns of that refusal only through `OnLoadCommitted`, which carries no world write access, so the implementer cannot persist it before the phase-11 snapshot (`Zenith2/Docs/Design/STREAM/Runtime.md` → "What is saved"). SCENE keeps a per-world flag "tagged commit refused this tick", set by the playback and cleared at the tick boundary, and the participant reads it in `GetCaptureFidelity`.
- **(h) Test support.** SCENE counts a cancelled preparation in `scene.prepare.cancelled.<state>` for the state it was cancelled in (`REQUESTED`, `READING`, `DECODING`, `DEPENDENCIES`, `GATED`, `BUILDING`, `PENDING` for a removed pending preparation), and its `Tests/Support/` gains a header-only preparation hold usable from other areas' integration suites (Tests.md "Assumed test support of other areas"). The patch registers these SCENE tests, each in `Zenith2/Engine/ZenithScene/Tests/`: `Test_AdditiveScenes_HookCallPoints` (every member called at its stated point and thread, `ShouldSkipEntity` and `AmendComponents` only after the gate opened, `OnPreparationEnded` once per committed tagged load including a removed pending one, the captured hook used after `SetActivationHook(nullptr)`), `Test_AdditiveScenes_DiscardResolverRecordsCancelled` (a tagged load unloaded while LOADING, with its result parked before the unload's playback, after it, and never, logs one `PUBLICATION_KIND_GENERIC_RECORDED` `{instance, SCENE_ERROR_CANCELLED, 0}` each time and calls `OnDiscardedPublication` once before the record is released; a replay of the run holds no tick for it) and `Test_AdditiveScenes_AssetErrorDetailPublished` (obligation (i) for a READING and a DEPENDENCIES failure, and the participant's COHERENT report of (g) (3)).
- **(i) The ASSET cause of `SCENE_ERROR_ASSET_UNAVAILABLE`.** Whenever SCENE fails a preparation with `SCENE_ERROR_ASSET_UNAVAILABLE` (state `READING`: the scene's or a hard dependency's load failed; state `DEPENDENCIES`: `AcquireAny` failed), `Zenith_SceneError::m_uDetail` holds the `Zenith_AssetErrorCode` of the ASSET error that caused it, never 0: the registry's reported load error in `READING`, `AcquireAny`'s error in `DEPENDENCIES`, and for `ASSET_ERROR_DEPENDENCY_FAILED` the failed dependency's own root code when ASSET supplies it (row ST22), otherwise `ASSET_ERROR_DEPENDENCY_FAILED` itself. The published `Zenith_SceneLoadResult::m_uDetail` carries the same value (the payload already has the byte), the publication handler stores it in the FAILED record with the code, and `Zenith_AdditiveSceneSet::GetErrorDetail(instance)` returns it (0 for a record with no detail or an unknown instance). `Zenith_SceneError`'s `m_uDetail` comment gains "the `Zenith_AssetErrorCode` for ASSET_UNAVAILABLE". Every `Zenith_AssetErrorCode` fits the `uint8` (the enumeration is `uint8`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Errors").
- **(j) A refused restore re-request.** When SCENE refuses a load SAVE's restore re-requests with a saved tag (its set or its completion IDs full, a duplicate), it reports the refusal as it reports a refused commit, `OnLoadCommitted(tag, 0, code)` on the main thread. STREAM appends nothing, counts `stream.restore.unadopted` and needs nothing else from SCENE: the refused instance does not exist, so nothing is left to release, and the cell, UNLOADED after the rebuild, loads afresh when wanted (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Restored loads beyond the pool").
<!-- /assumption -->

### SCRIPT publication resolver

The resolver is core text since core-doc item X271 (row ST21): `Zenith_PfnAsyncPublicationResolve`, `SetPublicationResolver` and the `Zenith_AsyncResultTable` contract's resolver order and replay `ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION` sentences in `Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript asynchronous results and host interfaces", and [Z2-N-SCRIPT-341]:

```cpp
// Zenith2/Engine/ZenithScript/Zenith_ScriptAsync.h (Zenith_AsyncResultTable, as Scripting/PublicTypes.md declares it)
// Live mode only, at tick-boundary step 1, after the drained completions are sorted by completion ID and before the
// publication observer logs them: once per drained completion of uSourceKind, in completion-ID order, on the world's
// tick-boundary node. The resolver may replace the publication kind (any Zenith_PublicationKind value) and the payload
// (at most the source's uMaxResultBytes, the capacity passed); it reads only its owner's state and calls nothing back.
// Never called under a publication source. One resolver per source kind, set before the first tick (SetHandler's rule).
using Zenith_PfnAsyncPublicationResolve = void (*)(uint64 ulCompletionID, uint8& uPublicationKind, uint8* pPayload,
	uint32 uPayloadCapacity, uint32& uPayloadBytes, void* pUserData);
void SetPublicationResolver(uint8 uSourceKind, Zenith_PfnAsyncPublicationResolve pfnResolve, void* pUserData);
```

<!-- assumption: pending MOD (row ST21) -->
ZenithRuntime's implementation of `Zenith_ScenePublicationSink::SetPublicationResolver` forwards to `SetPublicationResolver(ASYNC_SOURCE_SCENE_LOAD, …)` of the table the sink bound at the world's creation (core-doc item X274: a sink never re-resolves `GetAsyncResults()`, so a retired table keeps the resolver with its sink). The replay-side rule SCRIPT takes with it (row ST21 (c)): under a publication source, a live completion of an ID whose recorded entry is of a recorded kind is dropped when it is drained at or before the recorded tick (the existing rule of `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table", "a live completion of the same ID is discarded"), and `Complete` of an ID whose recorded entry was already published returns `UNKNOWN_COMPLETION` and releases nothing twice; the payload slot and the in-flight unit were returned when the recorded entry was published, and the sink's `Park` maps that return to a no-op counted in `runtime.async.late_local_completions`, never `ZENITH_ASSERT`. The target headings of the SCRIPT and MOD patches are listed in row ST21; the SCRIPT half above is core text, the forwarding and the `Park` mapping are MOD's.
<!-- /assumption -->

### ASSET dependency root cause

<!-- assumption: pending ASSET (row ST22) -->
`Zenith_AssetError::m_uDetail` of `ASSET_ERROR_DEPENDENCY_FAILED` holds the root `Zenith_AssetErrorCode` of the failed hard dependency (the first code in the dependency chain that is not `ASSET_ERROR_DEPENDENCY_FAILED`), where `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Errors" now says "otherwise 0". SCENE forwards that root code as its detail (obligation (i)), so a dependency that failed for capacity or I/O is classified as such ("Failure classification" in Runtime.md); until this lands, STREAM classifies `ASSET_ERROR_DEPENDENCY_FAILED` as transient.
<!-- /assumption -->

### SCENE world origin

<!-- assumption: pending SCENE (row ST3) -->
```cpp
// Zenith2/Engine/ZenithScene/Zenith_WorldOrigin.h
/// @contract value Zenith_WorldOrigin
/// Ownership: The world resource "Zenith.WorldOrigin" (owning phase PHASE_INPUT, no system writer), one per world, owned by
///   Zenith_SceneWorldState; written only by the Scene.ShiftWorldOrigin playback; saved with SCENE's world state.
/// Thread affinity: Readable by any node declaring a read of the resource.
/// Invalidation: None.
/// Units: Metres, absolute frame, double; m_uRebaseCount a count of shifts since the world was created.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldOrigin
{
	double m_dX = 0.0, m_dY = 0.0, m_dZ = 0.0;
	uint32 m_uRebaseCount = 0;
	uint64 m_ulLastRebaseTick = 0;
	static const Zenith_TypeID xTYPE_ID;
};

/// @contract value Zenith_WorldOriginShift
/// Ownership: Plain value copied into the command.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres; each component an integer multiple of 1 metre (exact in double).
/// Errors: A non-finite or non-integral component is counted and the command ignored.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_WorldOriginShift { const double m_dX = 0.0, m_dY = 0.0, m_dZ = 0.0; };

// Records "Scene.ShiftWorldOrigin" (phases 1–10). Playback, single-threaded, LAST in step 7 of its sync point: after
// every structural record of step 6 (spawns, including an activation's) and after every other step-7 record of that sync
// point, whatever its merge key (SCENE defers the shift's execution to the end of the step; at most one shift per sync
// point, a second counted and ignored). Then: O += S; m_uRebaseCount + 1; m_ulLastRebaseTick = tick; in ascending
// simulation ID: every Zenith.WorldTransform position (and the translation column of Row0–Row2) and every
// Zenith.PreviousWorldTransform position becomes (float)((double)p − S); every root's Zenith.LocalTransform position
// likewise; every property flagged PROPERTY_FLAG_WORLD_POSITION (row ST4) likewise. Cost O(entities with a transform +
// flagged properties) in one sync point, measured in the rebase-tick hitch row Z2-B-SHIP-016.
void Zenith_RecordShiftWorldOrigin(Zenith_CommandBuffer& xBuffer, const Zenith_WorldOriginShift& xShift);
```
<!-- /assumption -->

### ECS world-position flag

`Zenith_PropertyFlags` has `PROPERTY_FLAG_WORLD_POSITION = 1u << 7` (VEC3 properties only; a non-VEC3 property carrying it fails `Freeze` with `REFLECT_ERROR_BAD_FLAGS`), set by `Zenith_PropertyBuilder::Flags` and, in schemas, by the field attribute `"worldPosition": true`, as the `worldPosition` row of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "The schema file" states (core-doc item X227 (a), row ST4).

### MOD scene-state accessor

`Zenith_WorldAttachContext` has `Zenith_SceneWorldState* GetSceneState();` (the world's SCENE state, which ZenithRuntime creates at startup step 16 before attaching modules; never null in a runtime world), declared in `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" by core-doc item X227 (b) (row ST5; [Z2-N-ARCH-879]).

### ASSET kind row

<!-- assumption: pending ASSET (row ST6) -->
| Code | Enumerator | Stable name | Script name | Runtime object type | Owner | Cooked extension | Magic | Memory subsystem | Default fallback policy | Cacheable | GPU resident |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 35 | `ASSET_KIND_STREAMING_WORLD` | `Streaming.World` | `StreamingWorld` | `Zenith_StreamingWorldAsset` | STREAM | `.zstream` | `ZSTW` | STREAMING | FAIL | no | no |

Code 35 assumes ENV's `ASSET_KIND_CLOUD_NOISE` (row EN22) takes 34 first; if STREAM's row lands first it takes 34, and the code is whichever `uASSET_KIND_DEFINED_COUNT` names when the STREAM kind ticket lands (ASSET's growth rule). `Zenith_CookErrorCode` gains `COOK_ERROR_STREAM_DOCUMENT`, `_GRID`, `_CELL_RANGE`, `_CELL_DUPLICATE`, `_CELL_SCENE_KIND`, `_CELL_ANCHOR`, `_CELL_BOUNDS`, `_DEPENDENCY_CYCLE`, `_RADII`, `_REFERENCE`, `_PERSISTENT_ID`, `_HLOD`, `_LIMIT`, in that order.
<!-- /assumption -->

### RPROD origin-change rule

<!-- assumption: pending RPROD (row ST7) -->
When `Zenith.WorldOrigin::m_uRebaseCount` of a world differs from the value its previous sealed render world was extracted with, that world's extraction is a full-state extraction (`Flux_RenderWorldWriter::IsFullExtraction()` true for every extractor of the world), and RPROD translates by −S (S = the new origin minus the one the previous sealed render world was extracted with, in binary64, narrowed per value) every instance's previous-frame transform it keeps for motion vectors and every view's previous view transform and temporal-history reprojection matrix, instead of resetting them: the shift moves every position and the camera by the same S, so motion vectors and temporal histories stay valid. A view or an effect whose history cannot be translated exactly (a history stored in a frame RPROD cannot shift) gets `m_bCameraCut` or a history reset for that effect only. RPROD's PSO precache cook assigns each streaming cell's group the ID `Zenith_MakeStreamingGroupID(cell scene GUID)`, reading the cell table of every `.zstream` the game cooks.
<!-- /assumption -->
