# AI design: public API

Section file of `Zenith2/Docs/Design/AI.md`. It declares every public header of the `ZenithAI` module library: the value types and limits, the error record, the navigation mesh and its queries, the build pipeline entry points shared by the cook, the editor and runtime carving, the volume navigation types, the runtime asset object of kind 15, the components, events, commands and world resources the module registers, the debug header, the module entry points, the configuration keys and the authority census rows. Algorithms are in `Zenith2/Docs/Design/AI/Navigation.md`, `Zenith2/Docs/Design/AI/Volumes.md` and `Zenith2/Docs/Design/AI/Agents.md`; byte layouts in `Zenith2/Docs/Design/AI/Formats.md`; the script surface in `Zenith2/Docs/Design/AI/Scripting.md`.

## Scope of this file

- Every declaration here is in `Zenith2/Engine/ZenithAI/` (the library root); the file names are the headers' names.
- Every fallible public function returns `Zenith_Result<T, Zenith_AIError>`; every public type carries one contract block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type").
- No third-party type, no STL type and no `ZenithAI/Internal/` header appears in any header of this file; when D-SEL-NAV (open, Stage 1/2) selects Recast/Detour, its types stay inside `ZenithAI/Internal/RecastDetour/` (`Zenith2/Docs/Design/AI/Navigation.md` → "Candidate mapping").
- Units are SI: metres, seconds, radians; durations that reach simulation state are ticks (`uint64`), converted from seconds by `max(1, ceil(seconds × tickHz))` where `tickHz` is the world's simulation tick rate (`Zenith_SystemContext::GetTickDurationUs`).
- Positions passed to or returned by any query are world-local positions (the rebased frame of `Zenith.WorldOrigin`); cooked navigation data is stored tile-local and converted at the boundary (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift").

## Header list

| Header | Content |
|---|---|
| `Zenith_AITypes.h` | limits, enumerations, `Zenith_NavPolyRef`, `Zenith_NavTileKey`, `Zenith_NavClassIndex`, `Zenith_NavPoint`, `Zenith_NavQueryFilter` |
| `Zenith_AIErrors.h` | `Zenith_AIErrorCode`, `Zenith_AIError`, `Zenith_ToString` |
| `Zenith_NavMesh.h` | `Zenith_NavMesh`, the read-only view of one world's navigation tiles, overlay and link table |
| `Zenith_NavQuery.h` | path search, straight path, surface movement, nearest point, raycast and random-point queries |
| `Zenith_NavBuild.h` | the bake pipeline (rasterization to tile assembly) and the layer rebuild used by carving |
| `Zenith_NavVolume.h` | the sparse-voxel volume data, volume path search and line-of-sight |
| `Zenith_NavMeshAsset.h` | `Zenith_NavMeshAsset`, the runtime object of `ASSET_KIND_NAV_MESH` |
| `Zenith_AIComponents.h` | the eleven components and the container element type |
| `Zenith_AIEvents.h` | the four events |
| `Zenith_AICommands.h` | `Zenith_AICommandKinds` and the recording functions of the six command kinds |
| `Zenith_AIWorldResource.h` | `Zenith_AIWorldResource` and `Zenith_AIBlackboardStore`, the two world resources other code reads |
| `Zenith_AIDebug.h` | debug view bits, the canonical AI state hash |
| `Zenith_Module_AI.h` | the two module entry points |

The internal world resources `Zenith.AI.CrowdGrid`, `Zenith.AI.StimulusGrid`, `Zenith.AI.NoiseBuffer`, `Zenith.AI.PathService`, `Zenith.AI.AgentRuntime` and `Zenith.AI.PerceptionLedger` are declared in `ZenithAI/Internal/Zenith_AIWorldState.h`: only the module's own nodes and its save participant read them, and they are registered types (`TYPE_KIND_RESOURCE`) so the schedule can order their accesses. Their declarations, and every other internal type a ticket writes (the module services, the path consumer and decoder, the load records, the agent runtime record, the perception ledger, the save participant, the search tile view), are `Zenith2/Docs/Design/AI/Internals.md` → "Internal declarations"; the tool-side header `Zenith_AuthoringAI.h` is `Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header".

## Zenith_AITypes header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AITypes.h
#pragma once
#include <cstdint>
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithAI/Zenith_AIErrors.h"

inline constexpr uint32 uAI_MAX_GROUND_CLASSES = 4;              // navmesh agent classes per world
inline constexpr uint32 uAI_MAX_VOLUME_CLASSES = 2;              // volume agent classes per world
inline constexpr uint32 uAI_NAV_MAX_VERTS_PER_POLY = 6;
inline constexpr uint32 uAI_NAV_MAX_POLYS_PER_TILE = 4096;       // 12-bit polygon index
inline constexpr uint32 uAI_NAV_MAX_VERTS_PER_TILE = 16384;
inline constexpr uint32 uAI_NAV_MAX_DETAIL_VERTS_PER_POLY = 127;     // Recast's cap: a uint8 local index addresses 6 + 127 points
inline constexpr uint32 uAI_NAV_MAX_DETAIL_TRIS_PER_POLY = 255;      // Recast's cap: the uint8 per-polygon triangle count
inline constexpr uint32 uAI_NAV_MAX_MARK_POLYS = 256;                // polygons one MARK obstacle flags (Navigation.md "Mark procedure")
inline constexpr uint32 uAI_NAV_MAX_LAYERS_PER_COLUMN = 32;      // stacked tiles at one (x, z) of one class
inline constexpr uint32 uAI_NAV_MAX_TILES_PER_WORLD = 65536;
inline constexpr uint32 uAI_NAV_MAX_LINKS_PER_WORLD = 16384;     // off-mesh links registered at once
inline constexpr uint32 uAI_NAV_MAX_LINKS_PER_POLY = 8;
inline constexpr uint32 uAI_NAV_MAX_OBSTACLES_PER_WORLD = 4096;
inline constexpr uint32 uAI_NAV_AREA_COUNT = 64;                 // area codes 0 … 63
inline constexpr uint8 uAI_NAV_AREA_NULL = 63;                   // never stored on a polygon; the "not walkable" mark of the bake
inline constexpr uint32 uAI_NAV_COST_PROFILES = 8;
inline constexpr uint32 uAI_PATH_MAX_CORRIDOR_POLYS = 256;
inline constexpr uint32 uAI_PATH_MAX_CORNERS = 64;               // straight-path corners kept by an agent
inline constexpr uint32 uAI_SCRIPT_PATH_MAX_CORNERS = 32;        // corners in a script path result
inline constexpr uint32 uAI_PATH_RESULT_BYTES = 416;             // 32-byte header + 32 × 12-byte corners (Formats.md "Script result payloads")
inline constexpr uint32 uAI_PATH_MIN_SEARCH_NODES = 256;
inline constexpr uint32 uAI_PATH_MAX_SEARCH_NODES = 8192;
inline constexpr uint32 uAI_AVOID_MAX_NEIGHBOURS = 8;
inline constexpr uint32 uAI_AVOID_MAX_SEGMENTS = 16;
inline constexpr uint32 uAI_MAX_AGENTS_PER_WORLD = 4096;
inline constexpr uint32 uAI_PERCEPTION_MAX_MEMORY = 16;          // PerceivedTarget entries per perceiver
inline constexpr uint32 uAI_PERCEPTION_MAX_RAYS_PER_UPDATE = 8;
inline constexpr uint32 uAI_MAX_TEAMS = 32;
inline constexpr uint32 uAI_MAX_NOISES_PER_TICK = 256;
inline constexpr uint32 uAI_MAX_LOD_FOCI = 4;
inline constexpr uint32 uAI_LOD_TIER_COUNT = 4;
inline constexpr uint32 uAI_BLACKBOARD_MAX_KEYS_PER_OWNER = 64;
inline constexpr uint32 uAI_BLACKBOARD_MAX_ENTRIES_PER_WORLD = 65536;
inline constexpr uint32 uAI_VOLUME_MAX_NODES = 4194304;          // octree nodes per volume file
inline constexpr uint32 uAI_VOLUME_MAX_LEVELS = 10;              // level 0 = 4×4×4 leaf bricks
inline constexpr float fAI_NAV_MIN_CELL_SIZE = 0.05f;
inline constexpr float fAI_NAV_MAX_CELL_SIZE = 1.0f;

/// @contract enum Zenith_NavMedium
/// Ownership: Plain enumeration; property 2 Medium of Zenith.AI.Agent and the class record's medium.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value makes the agent IDLE with AI_ERROR_INVALID_DESC counted in ai.agent.invalid_delta.
/// Version: since Z2 Stage 4; values are stable (saved).
/// @endcontract
enum Zenith_NavMedium : uint8
{
	NAV_MEDIUM_GROUND = 1,        // walks the navmesh of a ground class; swims its WATER polygons when the area profile allows
	NAV_MEDIUM_FLYING = 2,        // moves through the AIR cells of a volume class
	NAV_MEDIUM_UNDERWATER = 3,    // moves through the WATER cells of a volume class
};

/// @contract enum Zenith_NavAreaCode
/// Ownership: Plain enumeration of the fixed area codes; codes 5 … 62 are game areas named by the navigation settings
///   document (Formats.md "Area table"); 63 is uAI_NAV_AREA_NULL.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Codes 0 … 4 equal Zenith_NavigationArea of ZenithEnvironment value for value.
/// @endcontract
enum Zenith_NavAreaCode : uint8
{
	NAV_AREA_GROUND = 0,
	NAV_AREA_ROAD = 1,
	NAV_AREA_OFF_PATH = 2,
	NAV_AREA_UNWALKABLE = 3,      // only an input code: the bake turns it into uAI_NAV_AREA_NULL
	NAV_AREA_WATER = 4,
	NAV_AREA_FIRST_GAME = 5,
	NAV_AREA_LAST_GAME = 62,
};

/// @contract enum Zenith_NavPolyFlags
/// Ownership: Plain bit flags of a polygon: the baked flags of its area plus the runtime overlay bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; bit values are stored in tile sections and never renumbered.
/// @endcontract
enum Zenith_NavPolyFlags : uint16
{
	NAV_POLY_FLAG_NONE = 0,
	NAV_POLY_FLAG_WALK = 1u << 0,
	NAV_POLY_FLAG_SWIM = 1u << 1,             // set for NAV_AREA_WATER and areas declared swimmable
	NAV_POLY_FLAG_DOOR = 1u << 2,             // a game area declared as a door
	NAV_POLY_FLAG_JUMP = 1u << 3,             // a game area declared as needing a jump
	NAV_POLY_FLAG_OFFMESH = 1u << 4,          // the polygon is an off-mesh link's link node (overlay only)
	NAV_POLY_FLAG_BLOCKED = 1u << 14,         // overlay: a MARK obstacle covers it
	NAV_POLY_FLAG_DISABLED = 1u << 15,        // overlay: reserved for a disabled tile
	NAV_POLY_FLAG_ALL = 0xFFFFu,
};

/// @contract value Zenith_NavTileKey
/// Ownership: Plain 8-byte value; the stable identity of a navmesh tile across sessions and saves.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Tile coordinates in tile units of the class's tile size, measured from the absolute world origin.
/// Errors: None.
/// Version: since Z2 Stage 4. Packed order (Pack) is the total order every deterministic tie-break uses.
/// @endcontract
struct Zenith_NavTileKey
{
	sint16 m_iTileX = 0;
	sint16 m_iTileZ = 0;
	uint8 m_uLayer = 0;          // 0 … uAI_NAV_MAX_LAYERS_PER_COLUMN − 1, stacked storeys of one column
	uint8 m_uClass = 0;          // ground class index of the settings document, 0 … 3
	uint16 m_uReserved = 0;      // 0
	constexpr uint64 Pack() const;   // class << 56 | layer << 48 | (tx + 32768) << 16 | (tz + 32768)
	friend constexpr bool operator==(const Zenith_NavTileKey&, const Zenith_NavTileKey&) = default;
};
static_assert(sizeof(Zenith_NavTileKey) == 8);

/// @contract value Zenith_NavPolyRef
/// Ownership: Plain 8-byte generational reference to one polygon of one loaded tile; null is 0.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Stale once its tile slot (polygon form) or link table slot (link-node form) is freed or re-used (the 16-bit
///   salt differs); every query resolving a stale reference fails with AI_ERROR_STALE_POLY_REF. Never serialized: saves store
///   (tile index, polygon index) for a polygon and (link entity simulation ID, class) for a link node (Formats.md "Save
///   section layout", node reference entry).
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Two forms, told apart by bit 63. Polygon form (bit 63 clear): bits 0–11 polygon index,
///   12–31 tile slot, 32–47 salt (1 … 65,535), 48–63 zero. Link-node form (bit 63 set): bits 0–19 link table index
///   (< uAI_NAV_MAX_LINKS_PER_WORLD), 20–31 zero, 32–47 the link table slot's salt (1 … 65,535), 48–62 zero. A corridor, a
///   corner and a search node hold either form (Navigation.md "Search graph"). MakeLinkNode(0, 0) (value 1 << 63) is the
///   **dead link node**: salt 0 matches no link table slot, so it never resolves; a restore writes it for a link-node
///   reference that no longer resolved at capture (Formats.md "Save section layout", node reference entry kind 3).
/// @endcontract
struct Zenith_NavPolyRef
{
	uint64 m_ulValue = 0;
	constexpr bool IsNull() const { return m_ulValue == 0; }
	constexpr bool IsLinkNode() const { return (m_ulValue >> 63) != 0; }
	constexpr uint32 GetPolyIndex() const { return static_cast<uint32>(m_ulValue & 0xFFFu); }       // polygon form
	constexpr uint32 GetTileSlot() const { return static_cast<uint32>((m_ulValue >> 12) & 0xFFFFFu); }  // polygon form
	constexpr uint32 GetLinkIndex() const { return static_cast<uint32>(m_ulValue & 0xFFFFFu); }    // link-node form
	constexpr uint16 GetSalt() const { return static_cast<uint16>((m_ulValue >> 32) & 0xFFFFu); }  // both forms
	static constexpr Zenith_NavPolyRef Make(uint16 uSalt, uint32 uSlot, uint32 uPoly);
	// (1 << 63) | (salt << 32) | index; uIndex < uAI_NAV_MAX_LINKS_PER_WORLD, and uSalt ≥ 1 or (uSalt, uIndex) = (0, 0), the dead link node.
	static constexpr Zenith_NavPolyRef MakeLinkNode(uint16 uSalt, uint32 uIndex);
	friend constexpr bool operator==(Zenith_NavPolyRef, Zenith_NavPolyRef) = default;
};

/// @contract value Zenith_NavClassIndex
/// Ownership: Plain value: an index into the world's class table (ground 0 … 3, volume 4 … 5); 0xFF is none.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Valid for the world session; classes are fixed by the first settings asset the world accepts.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavClassIndex
{
	uint8 m_uValue = 0xFFu;
	constexpr bool IsValid() const { return m_uValue != 0xFFu; }
	constexpr bool IsVolume() const { return m_uValue >= uAI_MAX_GROUND_CLASSES && IsValid(); }
};

/// @contract value Zenith_NavPoint
/// Ownership: Plain value: a position on the navmesh and the polygon that contains its projection.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: m_xPoly follows Zenith_NavPolyRef.
/// Units: World-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavPoint
{
	Zenith_Vector3 m_xPosition;
	Zenith_NavPolyRef m_xPoly;
};

/// @contract value Zenith_NavQueryFilter
/// Ownership: Plain 272-byte value; built from a cost profile (Zenith_AIWorldResource::GetCostProfile) or by hand.
/// Thread affinity: IMMUTABLE; members non-const: Validate normalises the cost table in place, and callers fill it.
/// Invalidation: None.
/// Units: Cost multipliers (dimensionless, ≥ 1.0 for an admissible heuristic; Navigation.md "Cost and heuristic").
/// Errors: A cost below 1.0, NaN or negative is AI_ERROR_INVALID_DESC at the query; +∞ excludes the area.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavQueryFilter
{
	float m_afAreaCost[uAI_NAV_AREA_COUNT] = {};   // per area code; 0 entries are replaced by 1.0 at Validate
	uint16 m_uIncludeFlags = NAV_POLY_FLAG_WALK;  // a polygon passes when (flags & include) != 0 …
	uint16 m_uExcludeFlags = NAV_POLY_FLAG_BLOCKED | NAV_POLY_FLAG_DISABLED;   // … and (flags & exclude) == 0
	uint32 m_uReserved = 0;
	uint64 m_ulReserved = 0;
	Zenith_Result<void, Zenith_AIError> Validate();   // normalises 0 to 1.0; checks ranges
	float GetMinCost() const;                                 // over included areas; the heuristic scale
};
static_assert(sizeof(Zenith_NavQueryFilter) == 272);

/// @contract enum Zenith_NavPathStatus
/// Ownership: Plain enumeration of path outcomes.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values are stable (script results and saves carry them).
/// @endcontract
enum Zenith_NavPathStatus : uint8
{
	NAV_PATH_STATUS_NONE = 0,
	NAV_PATH_STATUS_COMPLETE = 1,         // the corridor ends in the end polygon
	NAV_PATH_STATUS_PARTIAL = 2,          // the corridor ends in the explored polygon closest to the end
	NAV_PATH_STATUS_NO_START = 3,         // no polygon within the start extents
	NAV_PATH_STATUS_NO_END = 4,           // no polygon within the end extents; the search still ran toward the raw end point
	NAV_PATH_STATUS_NO_NAVMESH = 5,       // the class has no loaded tile
	NAV_PATH_STATUS_INVALID_REQUEST = 6,  // a non-finite position, an unknown class, a bad filter
	NAV_PATH_STATUS_CANCELLED = 7,        // the owner was destroyed or the world transitioned
	NAV_PATH_STATUS_TRUNCATED = 8,        // longer than the corridor capacity: the first 256 polygons toward the end
};

/// @contract enum Zenith_NavPathLimit
/// Ownership: Plain enumeration; the limit that ended a search (Navigation.md "Partial results").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values are stable (script results carry them).
/// @endcontract
enum Zenith_NavPathLimit : uint8
{
	NAV_PATH_LIMIT_NONE = 0,              // COMPLETE, or PARTIAL because the open list was exhausted (the end is unreachable)
	NAV_PATH_LIMIT_NODE_POOL = 1,         // PARTIAL at the first node the pool could not allocate
	NAV_PATH_LIMIT_TICK_CAP = 2,          // PARTIAL: finished early at ai.path.maxTicksPerRequest
	NAV_PATH_LIMIT_CORRIDOR = 3,          // TRUNCATED: more polygons than the corridor span
};

/// @contract enum Zenith_NavCornerFlags
/// Ownership: Plain bit flags of a straight-path corner.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_NavCornerFlags : uint8
{
	NAV_CORNER_FLAG_NONE = 0,
	NAV_CORNER_FLAG_START = 1u << 0,
	NAV_CORNER_FLAG_END = 1u << 1,
	NAV_CORNER_FLAG_OFFMESH_START = 1u << 2,   // the corner is the start of an off-mesh link
	NAV_CORNER_FLAG_AREA_CHANGE = 1u << 3,     // only with NAV_STRAIGHT_PATH_AREA_CROSSINGS
};

/// @contract value Zenith_NavCorner
/// Ownership: Plain 24-byte value.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: m_xPoly follows Zenith_NavPolyRef.
/// Units: World-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavCorner
{
	Zenith_Vector3 m_xPosition;
	uint8 m_uFlags = 0;          // Zenith_NavCornerFlags
	uint8 m_auReserved[3] = {};
	Zenith_NavPolyRef m_xPoly;    // the polygon entered at the corner (the link node for OFFMESH_START)
};
static_assert(sizeof(Zenith_NavCorner) == 24);
```

## Zenith_AIErrors header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AIErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_AIErrorCode
/// Ownership: Plain enumeration; the code of every Zenith_AIError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; an enumerator keeps its value and meaning once shipped; new codes append.
/// @endcontract
enum Zenith_AIErrorCode : uint8
{
	AI_ERROR_INVALID_DESC = 1,          // a descriptor or argument out of range, non-finite or inconsistent
	AI_ERROR_OUT_OF_MEMORY,             // an allocation failed; nothing changed
	AI_ERROR_NO_NAVMESH,                // the class has no loaded tile
	AI_ERROR_NO_POLY,                   // no polygon within the search extents
	AI_ERROR_CLASS_UNKNOWN,             // a class name the world's settings do not declare
	AI_ERROR_SETTINGS_MISMATCH,         // a tile or volume names another settings asset or hash than the world's
	AI_ERROR_TILE_CAPACITY,             // uAI_NAV_MAX_TILES_PER_WORLD reached; the tile is not added
	AI_ERROR_TILE_DUPLICATE,            // a tile with the same key is already loaded
	AI_ERROR_TILE_CORRUPT,              // a tile section fails a check of Formats.md "Tile section"
	AI_ERROR_FORMAT_VERSION,            // a navigation file or section version this build does not read
	AI_ERROR_STALE_POLY_REF,            // the reference's tile slot was freed or re-used
	AI_ERROR_INVALID_POLY_REF,          // null, or a polygon index beyond its tile
	AI_ERROR_NODE_POOL_EXHAUSTED,       // the search ran out of nodes; the result is PARTIAL
	AI_ERROR_BUILD_FAILED,              // a bake stage failed (the detail names the stage, Navigation.md "Bake stages")
	AI_ERROR_BUILD_LIMIT,               // a bake output exceeded a limit of "Zenith_AITypes header"
	AI_ERROR_VOLUME_CORRUPT,            // a volume section fails a check of Formats.md "Volume section"
	AI_ERROR_VOLUME_CAPACITY,           // a volume exceeds uAI_VOLUME_MAX_NODES
	AI_ERROR_LINK_INVALID,              // an off-mesh link with an endpoint off the navmesh or a bad kind
	AI_ERROR_LINK_CAPACITY,             // uAI_NAV_MAX_LINKS_PER_WORLD reached
	AI_ERROR_OBSTACLE_INVALID,          // an obstacle shape with non-positive or non-finite extents
	AI_ERROR_OBSTACLE_CAPACITY,         // uAI_NAV_MAX_OBSTACLES_PER_WORLD reached
	AI_ERROR_AGENT_CAPACITY,            // uAI_MAX_AGENTS_PER_WORLD reached; the entity is not an agent
	AI_ERROR_NOT_ROOT_ENTITY,           // an agent that is not a hierarchy root
	AI_ERROR_SAVE_CORRUPT,              // the save section fails a check of Formats.md "Save section layout"
	AI_ERROR_DOCUMENT_SCHEMA,           // an authoring document fails its schema
	AI_ERROR_SOURCE_GEOMETRY_CORRUPT,   // a ZNSG intermediate or a SOURCE_GEOMETRY section fails its checks
	AI_ERROR_MODULE_STATE,              // a member called in a module state that does not allow it (a programmer error elsewhere)
};

constexpr const char* Zenith_ToString(Zenith_AIErrorCode eCode);   // "AI_ERROR_INVALID_DESC", …

/// @contract value Zenith_AIError
/// Ownership: Plain 8-byte error record returned inside Zenith_Result ([Z2-N-CONTRACT-480] form: code first, no string).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uDetail is code-specific (a bake stage, a section index, a property ID); m_uContext a polygon count, a tile slot
///   or a byte offset as the returning function documents.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIError
{
	const Zenith_AIErrorCode m_eCode = AI_ERROR_INVALID_DESC;
	const uint8 m_uReserved = 0;
	const uint16 m_uDetail = 0;
	const uint32 m_uContext = 0;
};
static_assert(sizeof(Zenith_AIError) == 8);
```

## Zenith_NavMesh header

`Zenith_NavMesh` is the read-only view of a world's navigation state: the loaded tiles (immutable asset data), the runtime overlay (per-polygon flag and area overrides written by MARK obstacles, and the carved tile replacements), the link table, the class table and the cost profiles. One exists per world inside the AI module's world state; readers get it from `Zenith_AIWorldResource::GetNavMesh()`. Its only writer is the `AI.NavIngest` node.

```cpp
// Zenith2/Engine/ZenithAI/Zenith_NavMesh.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAI/Zenith_AITypes.h"
#include "ZenithAI/Zenith_AIErrors.h"

/// @contract value Zenith_NavClassInfo
/// Ownership: Plain value copied from the world's settings asset (Formats.md "Agent class object").
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Metres, radians.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavClassInfo
{
	uint64 m_ulNameHash = 0;                 // Zenith_Hash64 of the class name's UTF-8 bytes
	char m_acName[32] = {};                   // NUL-terminated, at most 31 bytes, ^[A-Za-z][A-Za-z0-9_]{0,30}$
	Zenith_NavMedium m_eMedium = NAV_MEDIUM_GROUND;
	uint8 m_auReserved[7] = {};
	float m_fAgentRadius = 0.35f;
	float m_fAgentHeight = 1.8f;
	float m_fMaxClimb = 0.35f;
	float m_fMaxSlopeRadians = 0.7853982f;
	float m_fCellSize = 0.25f;                // volume classes: the leaf voxel edge
	float m_fCellHeight = 0.1f;               // volume classes: 0
	float m_fTileSize = 32.0f;                // ground classes; volume classes: the brick region edge (Volumes.md)
};

/// @contract value Zenith_NavTileInfo
/// Ownership: Plain value describing one loaded tile.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Valid while the tile is loaded; m_uVersion changes whenever the tile's polygons are replaced by a carve.
/// Units: World-local metres for bounds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavTileInfo
{
	Zenith_NavTileKey m_xKey;
	uint32 m_uSlot = 0;
	uint16 m_uSalt = 0;
	uint16 m_uPolyCount = 0;
	uint32 m_uVersion = 0;                   // 1 when added; incremented by every carve swap
	uint32 m_uFlags = 0;                     // bit 0: carve-capable (has a layers section); bit 1: currently carved
	Zenith_Vector3 m_xBoundsMin;
	Zenith_Vector3 m_xBoundsMax;
	uint64 m_ulContentHash = 0;              // the tile section's 32-bit check, zero-extended (Formats.md "Common rules"); saves use it
	                                          // to detect a re-cooked tile
};

/// @contract type Zenith_NavMesh
/// Ownership: Owned by the AI module's world state, one per world; borrowed by readers through
///   Zenith_AIWorldResource::GetNavMesh for the duration of a node invocation.
/// Thread affinity: The const members are ANY_THREAD from every node that declares ReadResource of "Zenith.AI.World" (they
///   read the state AI.NavIngest left in the current tick); AI.NavIngest is the one writer, through Internal members (the
///   origin-shift listener also recomputes tile origins at sync point 1 with no node running, Navigation.md "Origin shift").
/// Invalidation: Spans returned are valid until the end of the reading node's invocation.
/// Units: World-local metres; areas and costs as Zenith_NavQueryFilter.
/// Errors: Members return Zenith_AIError; untrusted inputs (script arguments) never assert.
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 4. Results are a deterministic function of the loaded tiles, the overlay, the link table and the
///   arguments; no result depends on tile slot numbers (Navigation.md "Tie-breaking").
/// @endcontract
class Zenith_NavMesh
{
public:
	Zenith_NavClassIndex FindClass(Zenith_StringView strName) const;        // invalid when unknown
	Zenith_NavClassIndex FindClassByHash(uint64 ulNameHash) const;
	const Zenith_NavClassInfo* GetClassInfo(Zenith_NavClassIndex xClass) const;   // null when invalid
	uint32 GetClassCount() const;
	bool HasTiles(Zenith_NavClassIndex xClass) const;
	uint32 GetTileCount() const;
	Zenith_Result<Zenith_NavTileInfo, Zenith_AIError> FindTile(const Zenith_NavTileKey& xKey) const;
	Zenith_Result<Zenith_NavTileInfo, Zenith_AIError> GetTileOfPoly(Zenith_NavPolyRef xPoly) const;
	Zenith_Result<Zenith_NavPolyRef, Zenith_AIError> ResolvePoly(const Zenith_NavTileKey& xKey, uint32 uPoly) const;
	Zenith_Result<uint16, Zenith_AIError> GetPolyFlags(Zenith_NavPolyRef xPoly) const;   // baked | overlay
	Zenith_Result<uint8, Zenith_AIError> GetPolyArea(Zenith_NavPolyRef xPoly) const;     // overlay override if set
	// The polygon's vertices in world-local metres, counter-clockwise seen from +Y; returns the count written (3 … 6).
	uint32 GetPolyVertices(Zenith_NavPolyRef xPoly, Zenith_Span<Zenith_Vector3> axOut) const;
	// The surface height under xPosition inside the polygon, from the detail mesh; false when outside its footprint.
	bool GetPolyHeight(Zenith_NavPolyRef xPoly, const Zenith_Vector3& xPosition, float& fHeightOut) const;
	Zenith_Result<Zenith_NavQueryFilter, Zenith_AIError> GetCostProfile(uint32 uProfile) const;  // 0 … 7
	uint32 GetCostProfileIndex(Zenith_StringView strName) const;     // 0xFFFFFFFF when unknown
	uint64 GetSettingsHash() const;                                 // 0 before the first tile
	uint32 GetLinkCount() const;
	uint64 GetChangeSerial() const;   // incremented by every tile add, remove, carve swap, overlay or link change
};
```

## Zenith_NavQuery header

Free functions over a `const Zenith_NavMesh&`; each takes a scratch arena for its temporary allocations and allocates nothing from the heap. `Zenith_NavPathSearch` is the sliced search the path service steps (`Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing").

```cpp
// Zenith2/Engine/ZenithAI/Zenith_NavQuery.h
#pragma once
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Random.h"
#include "ZenithAI/Zenith_NavMesh.h"

/// @contract value Zenith_NavRaycastHit
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: m_xLastPoly follows Zenith_NavPolyRef.
/// Units: m_fFraction in [0, 1] along the segment; FLT_MAX-free: 1.0 means no wall was hit.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavRaycastHit
{
	float m_fFraction = 1.0f;
	Zenith_Vector3 m_xHitNormal;               // the wall's outward 2D normal (y = 0); zero when nothing was hit
	Zenith_NavPolyRef m_xLastPoly;             // the last polygon the ray reached
	uint32 m_uVisitedCount = 0;               // polygons written to the caller's span
};

/// @contract value Zenith_NavPathSearchDesc
/// Ownership: Plain value copied by Zenith_NavPathSearch::Begin.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: World-local metres; m_xExtents are half extents of the nearest-polygon search box.
/// Errors: Begin validates every member (AI_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavPathSearchDesc
{
	Zenith_NavClassIndex m_xClass;
	Zenith_Vector3 m_xStart;
	Zenith_Vector3 m_xEnd;
	Zenith_Vector3 m_xExtents{ 1.0f, 2.0f, 1.0f };
	Zenith_NavQueryFilter m_xFilter;
	uint32 m_uMaxNodes = 2048;                // uAI_PATH_MIN_SEARCH_NODES … uAI_PATH_MAX_SEARCH_NODES
	bool m_bUseLinks = true;                   // traverse registered off-mesh links
	uint8 m_auReserved[3] = {};
};

/// @contract value Zenith_NavPathResult
/// Ownership: Plain value whose m_axCorridor borrows the caller's span.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: Corridor references follow Zenith_NavPolyRef.
/// Units: m_fCost in metres × area cost.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
inline constexpr uint8 uNAV_PATH_RESULT_FLAG_DEAD_LINK = 1u << 0;   // PARTIAL cut before a dead link node (Navigation.md "Search state and slicing")

struct Zenith_NavPathResult
{
	Zenith_NavPathStatus m_eStatus = NAV_PATH_STATUS_NONE;
	Zenith_NavPathLimit m_eLimit = NAV_PATH_LIMIT_NONE;   // a limit other than NONE: the end may still be reachable
	uint8 m_uResultFlags = 0;                 // uNAV_PATH_RESULT_FLAG_*; not carried in the script payload
	uint8 m_uReserved = 0;
	uint32 m_uCorridorCount = 0;
	Zenith_NavPoint m_xStart;                  // the start projected onto the start polygon
	Zenith_NavPoint m_xEnd;                    // the end, or for PARTIAL and TRUNCATED the point of the last corridor polygon closest to it
	float m_fCost = 0.0f;
	uint32 m_uExpansions = 0;                 // nodes expanded over every Step
};

/// @contract value Zenith_NavSearchNode
/// Ownership: Plain 32-byte record of a search's node pool (Navigation.md "Search state and slicing").
/// Thread affinity: IMMUTABLE as a value; members non-const: an element its producer writes in place in an arena or
///   Zenith_Array; the pool is the owning search's.
/// Invalidation: m_ulRef follows Zenith_NavPolyRef (a link-node reference follows the link table slot's salt).
/// Units: World-local metres; g in metres × area cost.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavSearchNode
{
	uint64 m_ulRef = 0;                       // a Zenith_NavPolyRef value: polygon form, or link-node form for a link node
	Zenith_Vector3 m_xPosition;                // the point the search entered the node at
	float m_fG = 0.0f;
	uint32 m_uParent = 0xFFFFFFFFu;           // node index; 0xFFFFFFFF for the start node
	uint32 m_uFlags = 0;                      // bit 0 open, bit 1 closed
};
static_assert(sizeof(Zenith_NavSearchNode) == 32);

/// @contract value Zenith_NavSearchTileStamp
/// Ownership: Plain 16-byte value: the identity of a tile a search expanded into (Navigation.md "Search state and slicing").
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavSearchTileStamp
{
	Zenith_NavTileKey m_xKey;
	uint32 m_uVersion = 0;
	uint16 m_uSalt = 0;
	uint16 m_uReserved = 0;
};
static_assert(sizeof(Zenith_NavSearchTileStamp) == 16);

/// @contract value Zenith_NavSearchSnapshot
/// Ownership: Plain value whose spans borrow the search (GetSnapshot) or the caller (RestoreSnapshot) for the call.
/// Thread affinity: IMMUTABLE for the call; members non-const: its producer default-constructs it and fills it member by
///   member.
/// Invalidation: GetSnapshot's spans are invalid after the next Step, Reset or the node arena's reset.
/// Units: As Zenith_NavSearchNode.
/// Errors: RestoreSnapshot validates every index and count (AI_ERROR_INVALID_DESC with m_uDetail the member ordinal).
/// Version: since Z2 Stage 4. The save form is Formats.md "Save section layout" (search state); the participant converts node
///   references to and from (tile key, polygon index) and (link entity simulation ID, class).
/// @endcontract
struct Zenith_NavSearchSnapshot
{
	uint8 m_eState = 0;                                   // Navigation.md "Search state and slicing"
	Zenith_NavPathStatus m_eStatus = NAV_PATH_STATUS_NONE;
	uint8 m_uFlags = 0;                                   // bit 0 end polygon found, bit 1 pool exhaustion noted
	uint8 m_uReserved = 0;
	Zenith_NavPoint m_xStart;
	Zenith_NavPoint m_xEnd;
	uint32 m_uBestNode = 0xFFFFFFFFu;
	uint32 m_uExpansions = 0;
	Zenith_Span<const Zenith_NavSearchNode> m_axNodes;    // node-array order
	Zenith_Span<const uint32> m_auOpenHeap;               // heap-array order
	Zenith_Span<const Zenith_NavSearchTileStamp> m_axStamps;                  // Zenith_NavPathSearch; empty for a volume search
	Zenith_Span<const struct Zenith_NavVolumeSearchStamp> m_axVolumeStamps;   // Zenith_NavVolumeSearch; empty for a navmesh search
};

/// @contract type Zenith_NavPathSearch
/// Ownership: A value object owned by its caller (the path service keeps one per active request in its arena); its node pool
///   is carved from the arena given to Begin (or RestoreSnapshot) and is valid until that arena is reset.
/// Thread affinity: OWNER_THREAD. One search is stepped by one thread at a time; distinct searches run concurrently over one
///   Zenith_NavMesh.
/// Invalidation: Step returns AI_ERROR_STALE_POLY_REF when a tile the search expanded into was removed, its slot re-used or
///   its version changed by a carve swap since the search stamped it, and the caller restarts it (Navigation.md "Per-tick
///   processing"). A MARK overlay change, a neighbour tile added or a link change does not stale a search.
/// Units: Expansions are node pops from the open list.
/// Errors: Begin returns AI_ERROR_INVALID_DESC, AI_ERROR_CLASS_UNKNOWN, AI_ERROR_NO_NAVMESH, AI_ERROR_OUT_OF_MEMORY (the node
///   pool: the arena block is smaller than B(N), Navigation.md "Search state and slicing"); Step and Finish return the
///   Zenith_NavMesh errors above; RestoreSnapshot returns AI_ERROR_INVALID_DESC, AI_ERROR_INVALID_POLY_REF (a node reference that
///   does not resolve, except the dead link node MakeLinkNode(0, 0), which it accepts) and AI_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Step runs the expansion step of Navigation.md "Search state and slicing" (relaxation with
///   position replacement and node reopening, the best-node rule, tile stamps sized min(nodes, ai.nav.maxTiles), so no stamp
///   limit ends a search). The state after k expansions is a pure function of the navmesh history during the search (the
///   overlay, tiles and links each expansion saw), the descriptor and k; so a save stores the whole state (GetSnapshot) and a
///   restore rebuilds it (RestoreSnapshot), after which the next Step is bit-identical to the uninterrupted run's
///   (Agents.md "Save and restore algorithm").
/// @endcontract
class Zenith_NavPathSearch
{
public:
	Zenith_Result<void, Zenith_AIError> Begin(const Zenith_NavMesh& xMesh, const Zenith_NavPathSearchDesc& xDesc,
		Zenith_Arena& xNodeArena);
	// The whole state, for the save participant; valid until the next Step, Reset or arena reset.
	Zenith_NavSearchSnapshot GetSnapshot() const;
	// Rebuilds a begun search from a snapshot whose node references were re-resolved against xMesh: carves the block and reserves
	// both lookups as Begin does, copies the nodes (a dead link node kept as MakeLinkNode(0, 0)), the heap and the stamps (salts
	// from xMesh, versions from the snapshot), rebuilds the visited-node lookup, and takes the mesh's current change serial; the
	// descriptor is the saved one.
	Zenith_Result<void, Zenith_AIError> RestoreSnapshot(const Zenith_NavMesh& xMesh,
		const Zenith_NavPathSearchDesc& xDesc, const Zenith_NavSearchSnapshot& xSnapshot, Zenith_Arena& xNodeArena);
	// Expands at most uMaxExpansions nodes; returns true when the search finished (found, exhausted or failed).
	Zenith_Result<bool, Zenith_AIError> Step(const Zenith_NavMesh& xMesh, uint32 uMaxExpansions);
	// Writes the corridor (start polygon first) into axCorridorOut; callable once Step returned true, or early to take a
	// PARTIAL result toward the best node so far.
	Zenith_Result<Zenith_NavPathResult, Zenith_AIError> Finish(const Zenith_NavMesh& xMesh,
		Zenith_Span<Zenith_NavPolyRef> axCorridorOut);
	uint32 GetExpansions() const;
	bool IsBegun() const;
	void Reset();
private:
	Zenith_NavSearchNode* m_pxNodes = nullptr;             // the arena block of m_uMaxNodes nodes (32 bytes each)
	uint32* m_puOpenHeap = nullptr;                       // m_uMaxNodes node indices
	uint32* m_puHeapIndex = nullptr;                      // per node its heap slot, 0xFFFFFFFF when not in the heap
	struct Zenith_NavSearchLookup* m_pxLookup = nullptr;   // node reference → index, membership only (Internals.md "Search tile view");
	                                                       // TryReserve(m_uMaxNodes) at Begin and RestoreSnapshot
	Zenith_NavSearchTileStamp* m_pxStamps = nullptr;       // m_uStampCapacity entries
	struct Zenith_NavSearchLookup* m_pxStampLookup = nullptr;   // tile key Pack() → stamp index, membership only; TryReserve(m_uStampCapacity)
	uint32 m_uStampCapacity = 0;                          // min(m_uMaxNodes, ai.nav.maxTiles): never full before the pool
	uint32 m_uMaxNodes = 0;
	uint32 m_uNodeCount = 0;
	uint32 m_uOpenCount = 0;
	uint32 m_uStampCount = 0;
	uint32 m_uBestNode = 0xFFFFFFFFu;
	uint32 m_uExpansions = 0;
	uint64 m_ulMeshSerial = 0;                            // the serial the stamps were last checked against
	Zenith_NavPathSearchDesc m_xDesc;
	Zenith_NavPoint m_xStart;
	Zenith_NavPoint m_xEnd;
	uint8 m_eState = 0;                                   // Navigation.md "Search state and slicing"
	uint8 m_uFlags = 0;
	Zenith_NavPathStatus m_eStatus = NAV_PATH_STATUS_NONE;
};

// Synchronous convenience for tools, tests and the editor: Begin + Step(unbounded) + Finish.
Zenith_Result<Zenith_NavPathResult, Zenith_AIError> Zenith_NavFindPath(const Zenith_NavMesh& xMesh,
	const Zenith_NavPathSearchDesc& xDesc, Zenith_Span<Zenith_NavPolyRef> axCorridorOut, Zenith_Arena& xScratch);

inline constexpr uint32 uNAV_STRAIGHT_PATH_AREA_CROSSINGS = 1u << 0;   // add a corner at every area change
inline constexpr uint32 uNAV_STRAIGHT_PATH_ALL_CROSSINGS = 1u << 1;    // add a corner at every portal

// The funnel (string-pulling) algorithm over a corridor; returns the corner count written (≤ axCornersOut.GetCount()).
Zenith_Result<uint32, Zenith_AIError> Zenith_NavFindStraightPath(const Zenith_NavMesh& xMesh,
	const Zenith_Vector3& xStart, const Zenith_Vector3& xEnd, Zenith_Span<const Zenith_NavPolyRef> axCorridor,
	uint32 uOptions, Zenith_Span<Zenith_NavCorner> axCornersOut);

/// @contract value Zenith_NavMoveResult
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: World-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavMoveResult
{
	Zenith_Vector3 m_xPosition;               // the reached position, constrained to the surface
	uint32 m_uVisitedCount = 0;              // polygons written to the caller's span, start first
	bool m_bReachedTarget = false;
	uint8 m_auReserved[3] = {};
};

// Moves from xStart (on xStartPoly) toward xTarget along the surface through at most axVisitedOut.GetCount() (≤ 16)
// polygons, sliding along walls (Navigation.md "Surface movement").
Zenith_Result<Zenith_NavMoveResult, Zenith_AIError> Zenith_NavMoveAlongSurface(const Zenith_NavMesh& xMesh,
	Zenith_NavPolyRef xStartPoly, const Zenith_Vector3& xStart, const Zenith_Vector3& xTarget,
	const Zenith_NavQueryFilter& xFilter, Zenith_Span<Zenith_NavPolyRef> axVisitedOut);

Zenith_Result<Zenith_NavPoint, Zenith_AIError> Zenith_NavFindNearestPoint(const Zenith_NavMesh& xMesh,
	Zenith_NavClassIndex xClass, const Zenith_Vector3& xPosition, const Zenith_Vector3& xHalfExtents,
	const Zenith_NavQueryFilter& xFilter);

Zenith_Result<Zenith_NavRaycastHit, Zenith_AIError> Zenith_NavRaycast(const Zenith_NavMesh& xMesh,
	const Zenith_NavPoint& xStart, const Zenith_Vector3& xEnd, const Zenith_NavQueryFilter& xFilter,
	Zenith_Span<Zenith_NavPolyRef> axVisitedOut);

// A uniformly area-weighted random point reachable from xCentre within fRadius metres of path-connected surface; draws come
// from the caller's counter-based stream, so the point is a function of the stream state (Navigation.md "Random reachable point").
Zenith_Result<Zenith_NavPoint, Zenith_AIError> Zenith_NavRandomReachablePoint(const Zenith_NavMesh& xMesh,
	const Zenith_NavPoint& xCentre, float fRadius, const Zenith_NavQueryFilter& xFilter, Zenith_RandomSource& xRandom,
	Zenith_Arena& xScratch);
```

## Zenith_NavBuild header

The bake pipeline, a public part of `ZenithAI` because three callers share it: the cook step `ai.navMesh` (`Zenith2/Tools/ZenithCook/Internal/AI/`, through `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`), the editor's in-process regeneration after terrain and scene edits, and runtime carving (`AI.NavIngest`). The stage contracts are `Zenith2/Docs/Design/AI/Navigation.md` → "Bake stages"; each stage's implementation is the D-SEL-NAV (open, Stage 1/2) winner's (`Zenith2/Docs/Design/AI/Navigation.md` → "Candidate mapping").

```cpp
// Zenith2/Engine/ZenithAI/Zenith_NavBuild.h
#pragma once
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithAI/Zenith_NavMesh.h"

/// @contract enum Zenith_NavPartition
/// Ownership: Plain enumeration of the region partition of stage 6.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is AI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_NavPartition : uint8
{
	NAV_PARTITION_WATERSHED = 1,   // the cook default: distance field then flood (best polygons, slowest)
	NAV_PARTITION_MONOTONE = 2,    // sweep partition (fast, long thin polygons acceptable)
	NAV_PARTITION_LAYERS = 3,      // overlapping walkable layers separated (multi-storey tiles)
};

/// @contract value Zenith_NavClassParams
/// Ownership: Plain value; one ground class's bake parameters from the settings document (Formats.md "Agent class object").
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Metres, radians, cells (the m_u* members are counts of xz cells).
/// Errors: Zenith_NavValidateClassParams returns AI_ERROR_INVALID_DESC with m_uDetail the first bad member's ordinal.
/// Version: since Z2 Stage 4. Every member is a key input of the cook step (Navigation.md "Bake determinism").
/// @endcontract
struct Zenith_NavClassParams
{
	float m_fCellSize = 0.25f;               // fAI_NAV_MIN_CELL_SIZE … fAI_NAV_MAX_CELL_SIZE
	float m_fCellHeight = 0.1f;              // 0.02 … 0.5
	float m_fAgentRadius = 0.35f;            // 0 … 8
	float m_fAgentHeight = 1.8f;             // > 0, ≤ 16
	float m_fMaxClimb = 0.35f;               // 0 … agent height
	float m_fMaxSlopeRadians = 0.7853982f;   // 0 … 1.5533 (89°)
	uint32 m_uRegionMinArea = 8;            // cells²; smaller isolated regions are removed
	uint32 m_uRegionMergeArea = 20;         // cells²; smaller regions merge into neighbours
	float m_fEdgeMaxLength = 12.0f;          // metres; 0 disables splitting
	float m_fEdgeMaxError = 1.3f;            // cells; contour simplification tolerance, 0.1 … 3
	float m_fDetailSampleDistance = 6.0f;    // cells; 0 disables the detail mesh (flat polygons)
	float m_fDetailSampleMaxError = 1.0f;    // cell heights
	float m_fTileSize = 32.0f;               // metres; one of 16, 32, 64, 128 (divides ENV's 128 m cell)
	Zenith_NavPartition m_ePartition = NAV_PARTITION_WATERSHED;
	uint8 m_auReserved[3] = {};
};
Zenith_Result<void, Zenith_AIError> Zenith_NavValidateClassParams(const Zenith_NavClassParams& xParams);
uint32 Zenith_NavGetTileBorderCells(const Zenith_NavClassParams& xParams);   // ceil(r / cs) + 3

/// @contract value Zenith_NavAreaVolume
/// Ownership: Plain value; an oriented box that overrides the area of every walkable cell it contains (stage 5).
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: Tile-local metres.
/// Errors: Extents ≤ 0 or non-finite are AI_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavAreaVolume
{
	Zenith_Vector3 m_xCentre;
	Zenith_Vector3 m_xHalfExtents;
	Zenith_Quaternion m_xRotation;           // yaw-only rotations are exact; a tilted box is applied by its xz footprint and y range
	uint8 m_uArea = NAV_AREA_GROUND;        // 0 … 62, or uAI_NAV_AREA_NULL to cut a hole
	uint8 m_uPriority = 0;                  // volumes apply in ascending (priority, ordinal) order; the last applied wins
	uint16 m_uOrdinal = 0;                  // the authoring order the cook assigns (simulation-ID order of the source entities)
};

/// @contract value Zenith_NavBuildTileInput
/// Ownership: Plain value of borrowed spans valid for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None beyond the borrowed spans.
/// Units: Tile-local metres: positions relative to m_adTileOrigin (the tile's minimum corner without the border).
/// Errors: Validated by Zenith_NavBuildTile (AI_ERROR_INVALID_DESC with the member ordinal).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavBuildTileInput
{
	Zenith_NavTileKey m_xKey;
	double m_adTileOrigin[3] = {};                     // absolute world metres of the tile's minimum corner
	float m_fMinY = 0.0f;                              // tile-local vertical bounds of the input
	float m_fMaxY = 0.0f;
	Zenith_Span<const Zenith_Vector3> m_axVertices;    // triangle soup vertices, tile-local, border included
	Zenith_Span<const uint32> m_auIndices;            // three per triangle, counter-clockwise seen from +Y for upward faces
	Zenith_Span<const uint8> m_auAreas;               // one Zenith_NavAreaCode per triangle (NAV_AREA_UNWALKABLE marks a blocker)
	Zenith_Span<const Zenith_NavAreaVolume> m_axAreaVolumes;
	bool m_bKeepLayers = false;                        // keep the stage-4 compact heightfield for carving (Formats.md "Layers section")
	uint8 m_auReserved[7] = {};
};

/// @contract value Zenith_NavBuildStats
/// Ownership: Plain value filled by the build functions.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Counts; microseconds of the calling thread (diagnostics only, never a key input or an output byte).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavBuildStats
{
	uint32 m_uInputTriangles = 0;
	uint32 m_uSpans = 0;
	uint32 m_uWalkableSpans = 0;
	uint32 m_uRegions = 0;
	uint32 m_uContours = 0;
	uint32 m_uPolys = 0;
	uint32 m_uVertices = 0;
	uint32 m_uDetailTriangles = 0;
	uint64 m_aulStageMicroseconds[10] = {};           // per stage of Navigation.md "Bake stages"
};

/// @contract type Zenith_NavBuiltTile
/// Ownership: A view into the arena given to the build call; valid until that arena is reset.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the arena reset.
/// Units: The neutral tile representation of Formats.md "Tile section" (quantised vertices, polygons, detail, BV tree).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavBuiltTile
{
	Zenith_NavTileKey m_xKey;
	Zenith_ByteSpan m_xTileSection;                    // the encoded tile section body (Formats.md "Tile section")
	Zenith_ByteSpan m_xLayersSection;                  // empty unless m_bKeepLayers
	uint32 m_uPolyCount = 0;
	uint32 m_uVertexCount = 0;
	Zenith_Vector3 m_xBoundsMin;                       // tile-local
	Zenith_Vector3 m_xBoundsMax;
};

// Stages 1–10 for one tile (Navigation.md "Bake stages"). An input with no walkable span produces a tile with zero
// polygons and AI_ERROR_NONE-equivalent success; the caller decides whether to write it (the cook omits empty tiles).
Zenith_Result<Zenith_NavBuiltTile, Zenith_AIError> Zenith_NavBuildTile(const Zenith_NavClassParams& xParams,
	const Zenith_NavBuildTileInput& xInput, Zenith_Arena& xArena, Zenith_NavBuildStats& xStatsOut);

/// @contract value Zenith_NavObstacleShape
/// Ownership: Plain value; one carving obstacle in tile-local metres.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Validated by the rebuild (AI_ERROR_OBSTACLE_INVALID).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavObstacleShape
{
	Zenith_Vector3 m_xCentre;
	Zenith_Vector3 m_xHalfExtents;                     // cylinder: x = radius, y = half height, z ignored
	float m_fYaw = 0.0f;                               // box only
	uint8 m_eShape = 0;                               // Zenith_AINavObstacleShape
	uint8 m_uArea = uAI_NAV_AREA_NULL;                // NULL cuts; another code re-areas the footprint
	uint16 m_uReserved = 0;
	uint64 m_ulSimulationID = 0;                      // the obstacle entity: the order key of Navigation.md "Carve procedure"
};

// Stages 6–10 from a stored layers section with obstacles and the tile's area volumes applied (the carve rebuild). The
// partition is xParams.m_ePartition, the class's own (the cook and every runtime rebuild of one class use one partition),
// so a rebuild with no obstacle reproduces the baked tile section (Test_NavBuild_KeepLayersRoundTrip).
Zenith_Result<Zenith_NavBuiltTile, Zenith_AIError> Zenith_NavRebuildTileFromLayers(
	const Zenith_NavClassParams& xParams, const Zenith_NavTileKey& xKey, Zenith_ByteSpan xLayersSection,
	Zenith_Span<const Zenith_NavObstacleShape> axObstacles, Zenith_Arena& xArena, Zenith_NavBuildStats& xStatsOut);
```

## Zenith_NavVolume header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_NavVolume.h
#pragma once
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAI/Zenith_AITypes.h"
#include "ZenithAI/Zenith_AIErrors.h"
#include "ZenithAI/Zenith_NavBuild.h"                 // Zenith_NavBuildStats
#include "ZenithAI/Zenith_NavQuery.h"                 // Zenith_NavSearchSnapshot

/// @contract enum Zenith_NavVolumeMedium
/// Ownership: Plain enumeration of a leaf voxel's two medium bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; stored in volume sections.
/// @endcontract
enum Zenith_NavVolumeMedium : uint8
{
	NAV_VOLUME_MEDIUM_AIR = 0,
	NAV_VOLUME_MEDIUM_WATER = 1,
	NAV_VOLUME_MEDIUM_SOLID = 2,      // inflated by the class radius at bake
};

/// @contract value Zenith_NavVolumeRef
/// Ownership: Plain 8-byte reference to one octree node of one loaded volume (Volumes.md "Volume model").
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Stale when the volume slot is freed or re-used (salt); never serialized (saves store volume GUID + node
///   index + voxel index).
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Bits 0–5 leaf voxel index (ignored when bit 6, whole node, is set), 6 whole-node flag,
///   7–28 node index within the level, 29–32 level, 33–40 volume slot, 41–56 salt, 57–63 zero.
/// @endcontract
struct Zenith_NavVolumeRef
{
	uint64 m_ulValue = 0;
	constexpr bool IsNull() const { return m_ulValue == 0; }
};

/// @contract value Zenith_NavVolumeSearchStamp
/// Ownership: Plain 8-byte value: a volume one volume search created a unit in (Volumes.md "Volume search" step 6).
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The save form is the volume's index in the section's volume list (Formats.md "Save section layout").
/// @endcontract
struct Zenith_NavVolumeSearchStamp
{
	uint32 m_uVolumeSlot = 0;                 // the volume's slot in Zenith_NavVolumeSet
	uint16 m_uSalt = 0;                       // that slot's salt when the stamp was taken (1 … 65,535)
	uint16 m_uReserved = 0;
};
static_assert(sizeof(Zenith_NavVolumeSearchStamp) == 8);

/// @contract value Zenith_NavVolumeSearchDesc
/// Ownership: Plain value copied by Zenith_NavVolumeSearch::Begin.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: World-local metres.
/// Errors: Validated by Begin (AI_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_NavVolumeSearchDesc
{
	Zenith_NavClassIndex m_xClass;
	Zenith_NavVolumeMedium m_eMedium = NAV_VOLUME_MEDIUM_AIR;   // the medium the agent may occupy (AIR or WATER)
	uint8 m_auReserved[6] = {};
	Zenith_Vector3 m_xStart;
	Zenith_Vector3 m_xEnd;
	float m_fSearchRadius = 2.0f;             // nearest free voxel search around start and end
	uint32 m_uMaxNodes = 4096;               // uAI_PATH_MIN_SEARCH_NODES … uAI_PATH_MAX_SEARCH_NODES
	float m_fMediumChangeCost = 4.0f;         // added at an AIR/WATER boundary for an amphibious class (Volumes.md "Medium transitions")
};

/// @contract type Zenith_NavVolumeSearch
/// Ownership: A value object like Zenith_NavPathSearch, owned by the path service per active volume request.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Stale only when a stamped volume is removed or its slot re-used (AI_ERROR_STALE_POLY_REF); volumes never change
///   content while loaded, so a volume stamp has no version (Volumes.md "Volume search" step 6).
/// Units: Expansions.
/// Errors: As Zenith_NavPathSearch; AI_ERROR_NO_POLY when no free voxel is within m_fSearchRadius of the start or end.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Deterministic in the same sense as Zenith_NavPathSearch, and saved whole the same way: its
///   snapshot reuses Zenith_NavSearchSnapshot with m_ulRef holding a Zenith_NavVolumeRef value, m_axStamps empty and one
///   Zenith_NavVolumeSearchStamp per stamped volume in m_axVolumeStamps.
/// @endcontract
class Zenith_NavVolumeSearch
{
public:
	Zenith_Result<void, Zenith_AIError> Begin(const class Zenith_NavVolumeSet& xVolumes,
		const Zenith_NavVolumeSearchDesc& xDesc, Zenith_Arena& xNodeArena);
	Zenith_NavSearchSnapshot GetSnapshot() const;
	Zenith_Result<void, Zenith_AIError> RestoreSnapshot(const class Zenith_NavVolumeSet& xVolumes,
		const Zenith_NavVolumeSearchDesc& xDesc, const Zenith_NavSearchSnapshot& xSnapshot, Zenith_Arena& xNodeArena);
	Zenith_Result<bool, Zenith_AIError> Step(const class Zenith_NavVolumeSet& xVolumes, uint32 uMaxExpansions);
	// Smoothed corners (Volumes.md "Volume path smoothing"), start first; returns the count written and the status.
	Zenith_Result<uint32, Zenith_AIError> Finish(const class Zenith_NavVolumeSet& xVolumes,
		Zenith_Span<Zenith_Vector3> axCornersOut, Zenith_NavPathStatus& eStatusOut);
	uint32 GetExpansions() const;
	void Reset();
private:
	Zenith_NavSearchNode* m_pxNodes = nullptr;             // m_uMaxNodes units; m_ulRef holds a Zenith_NavVolumeRef value, the
	                                                       // position is the unit's centre (Volumes.md "Volume search")
	uint32* m_puOpenHeap = nullptr;
	uint32* m_puHeapIndex = nullptr;                      // per unit its heap slot, 0xFFFFFFFF when not in the heap
	struct Zenith_NavSearchLookup* m_pxLookup = nullptr;   // volume reference value → unit index, membership only; TryReserve(m_uMaxNodes)
	uint16* m_puStampBySlot = nullptr;                    // 256 entries: volume slot → stamp index + 1, 0 when unstamped
	uint32 m_uMaxNodes = 0;
	uint32 m_uNodeCount = 0;
	uint32 m_uOpenCount = 0;
	uint32 m_uBestNode = 0xFFFFFFFFu;
	uint32 m_uExpansions = 0;
	Zenith_NavVolumeSearchStamp* m_pxStamps = nullptr;     // m_uStampCapacity = min(m_uMaxNodes, 256) entries in the node arena
	uint32 m_uStampCount = 0;
	uint32 m_uStampCapacity = 0;
	uint64 m_ulVolumeSerial = 0;
	Zenith_NavVolumeSearchDesc m_xDesc;
	uint8 m_eState = 0;
};

/// @contract value Zenith_NavWaterPrism
/// Ownership: Plain value whose m_axPolygon borrows the caller's span for the Zenith_NavBuildVolume call.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None beyond the borrowed span.
/// Units: Region-local metres, as Zenith_NavVolumeBuildInput.
/// Errors: Validated by Zenith_NavBuildVolume (AI_ERROR_INVALID_DESC: a polygon of 1, 2 or more than 256 vertices, a box with
///   min > max on an axis, or a surface below the base).
/// Version: since Z2 Stage 4. One water body of the volume bake (Volumes.md "Volume bake").
/// @endcontract
struct Zenith_NavWaterPrism
{
	Zenith_Span<const Zenith_Vector2> m_axPolygon;     // 3 … 256 XZ vertices counter-clockwise; empty: the box below
	Zenith_Vector3 m_xBoxMin;                          // used when the polygon is empty
	Zenith_Vector3 m_xBoxMax;
	float m_fSurfaceHeight = 0.0f;
	float m_fBaseHeight = 0.0f;
};

/// @contract value Zenith_NavVolumeBuildInput
/// Ownership: Plain value of borrowed spans valid for the call (filled by Zenith_AIGatherVolumeBakeInputs, Internals.md
///   "Zenith_AuthoringAI header", or by tests).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None beyond the borrowed spans.
/// Units: Region-local metres (relative to m_adRegionMin); heights absolute minus m_adRegionMin[1].
/// Errors: Validated by Zenith_NavBuildVolume (AI_ERROR_INVALID_DESC with the member ordinal).
/// Version: since Z2 Stage 4. The algorithm is Volumes.md "Volume bake".
/// @endcontract
struct Zenith_NavVolumeBuildInput
{
	uint8 m_uClass = 0;                               // the volume class index (4 … 5)
	uint8 m_uLevels = 0;                              // 1 … uAI_VOLUME_MAX_LEVELS
	uint8 m_auReserved[2] = {};
	float m_fLeafEdge = 0.5f;                          // the class cell size v
	float m_fAgentRadius = 0.3f;
	double m_adRegionMin[3] = {};
	float m_fRegionEdge = 0.0f;                        // 4 v × 2^(levels − 1)
	Zenith_Span<const Zenith_Vector3> m_axVertices;
	Zenith_Span<const uint32> m_auIndices;
	Zenith_Span<const Zenith_NavWaterPrism> m_axWater;
};
// Volumes.md "Volume bake" steps 2–6; the encoded volume section body in xArena; AI_ERROR_VOLUME_CAPACITY beyond
// uAI_VOLUME_MAX_NODES. Byte-identical output for identical input.
Zenith_Result<Zenith_ByteSpan, Zenith_AIError> Zenith_NavBuildVolume(const Zenith_NavVolumeBuildInput& xInput,
	Zenith_Arena& xArena, Zenith_NavBuildStats& xStatsOut);

/// @contract type Zenith_NavVolumeSet
/// Ownership: Owned by the AI module's world state beside Zenith_NavMesh; the loaded volumes of every volume class.
/// Thread affinity: Const members ANY_THREAD for nodes declaring ReadResource of "Zenith.AI.World"; AI.NavIngest writes.
/// Invalidation: Spans valid for the reading invocation.
/// Units: World-local metres.
/// Errors: Zenith_AIError.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_NavVolumeSet
{
public:
	uint32 GetVolumeCount() const;
	Zenith_Result<Zenith_NavVolumeMedium, Zenith_AIError> SampleMedium(Zenith_NavClassIndex xClass,
		const Zenith_Vector3& xPosition) const;   // SOLID outside every loaded volume
	// Three-dimensional line of sight through leaf voxels of the given free medium (Volumes.md "Volume line of sight").
	bool HasLineOfSight(Zenith_NavClassIndex xClass, const Zenith_Vector3& xFrom, const Zenith_Vector3& xTo,
		Zenith_NavVolumeMedium eMedium) const;
	Zenith_Result<Zenith_Vector3, Zenith_AIError> FindNearestFree(Zenith_NavClassIndex xClass,
		const Zenith_Vector3& xPosition, float fRadius, Zenith_NavVolumeMedium eMedium) const;
	uint64 GetChangeSerial() const;
};
```

## Zenith_NavMeshAsset header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_NavMeshAsset.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithAI/Zenith_NavMesh.h"

/// @contract enum Zenith_NavFileRole
/// Ownership: Plain enumeration; the info section's role field (Formats.md "Info section").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value outside the enumerators is AI_ERROR_TILE_CORRUPT at load.
/// Version: since Z2 Stage 4; stable.
/// @endcontract
enum Zenith_NavFileRole : uint8
{
	NAV_FILE_ROLE_SETTINGS = 1,          // the cooked navigation settings document: classes, area table, cost profiles
	NAV_FILE_ROLE_TILE = 2,              // one navmesh tile of one ground class (optionally with its layers section)
	NAV_FILE_ROLE_MANIFEST = 3,          // a navigation document's product list (hard references to its tiles and volumes)
	NAV_FILE_ROLE_VOLUME = 4,            // one volume region of one volume class
	NAV_FILE_ROLE_SOURCE_GEOMETRY = 5,   // a terrain tile's navigation source (ENV's navigation:<x>_<z> product); never loaded by a world
};

/// @contract type Zenith_NavMeshAsset
/// Ownership: The runtime object of ASSET_KIND_NAV_MESH (code 15, "AI.NavMesh", ".znav", "ZNAV"); created and destroyed by
///   the AI kind loader; shared across worlds through Zenith_AssetHandle<Zenith_NavMeshAsset> (reference-counted by
///   ZenithAssets; kind 15 is not cacheable, so it unloads at zero references).
/// Thread affinity: IMMUTABLE after Activate; ANY_THREAD reads.
/// Invalidation: Pointers and spans it returns live as long as a handle to it does.
/// Units: The file's units (Formats.md "Navigation file"): quantised tile-local positions with the class cell sizes.
/// Errors: Accessors of a section the role does not carry return an empty span or null.
/// Version: since Z2 Stage 4. Format version uAI_NAV_FORMAT_VERSION (1).
/// @endcontract
class Zenith_NavMeshAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_NAV_MESH;
	Zenith_NavFileRole GetRole() const;
	const Zenith_PersistentID& GetSettingsID() const;      // the settings asset every non-settings file names
	uint64 GetSettingsHash() const;
	Zenith_Span<const Zenith_NavClassInfo> GetClasses() const;           // SETTINGS only
	Zenith_ByteSpan GetAreaTable() const;                                // SETTINGS only, Formats.md "Area table"
	Zenith_ByteSpan GetCostProfiles() const;                             // SETTINGS only
	const Zenith_NavTileKey* GetTileKey() const;                         // TILE only
	Zenith_ByteSpan GetTileSection() const;                              // TILE only, decoded
	Zenith_ByteSpan GetLayersSection() const;                            // TILE with layers; stored (compressed) bytes
	Zenith_Span<const Zenith_AssetHandle<Zenith_NavMeshAsset>> GetManifestEntries() const;   // MANIFEST only
	Zenith_ByteSpan GetVolumeSection() const;                            // VOLUME only, decoded
	uint64 GetResidentBytes() const;
};

inline constexpr uint16 uAI_NAV_FORMAT_VERSION = 1;
```

## Zenith_AIComponents header

All components are C++-registered reflected types (`Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration"), registered by `Zenith_Module_AI_RegisterTypes`. Each table lists property ID, name, type, default, authority and whether the property is serialized. Authority values are `Zenith_GetPhaseName` strings (`Zenith2/Docs/Architecture/FramePhases.md` → "The phase enum"); `AINavigation` is phase 7.

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AIComponents.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_ECSTypes.h"                 // Zenith_ContainerRef
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithAI/Zenith_AITypes.h"
#include "ZenithAI/Zenith_NavMeshAsset.h"

/// @contract enum Zenith_AIMoveStatus
/// Ownership: Plain enumeration; property 40 MoveStatus of Zenith.AI.Agent (Agents.md "Agent state machine").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values are saved and read by scripts by name.
/// @endcontract
enum Zenith_AIMoveStatus : uint8
{
	AI_MOVE_STATUS_IDLE = 0,
	AI_MOVE_STATUS_PLANNING = 1,
	AI_MOVE_STATUS_MOVING = 2,
	AI_MOVE_STATUS_TRAVERSING_LINK = 3,
	AI_MOVE_STATUS_WAITING_LINK = 4,     // a scripted link: waits for AI.CompleteOffMeshLink
	AI_MOVE_STATUS_ARRIVED = 5,
	AI_MOVE_STATUS_FAILED = 6,
	AI_MOVE_STATUS_SUSPENDED = 7,        // InActiveRegion false, or the agent's class has no tile
	AI_MOVE_STATUS_PAUSED = 8,           // AI.Agent PAUSE
};

/// @contract enum Zenith_AIAgentFlags
/// Ownership: Plain bit flags; property 11 Flags of Zenith.AI.Agent.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AIAgentFlags : uint32
{
	AI_AGENT_FLAG_NONE = 0,
	AI_AGENT_FLAG_AVOIDANCE = 1u << 0,             // take part in local avoidance (default on)
	AI_AGENT_FLAG_ALLOW_PARTIAL = 1u << 1,         // follow a PARTIAL path instead of failing
	AI_AGENT_FLAG_AUTO_LINKS = 1u << 2,            // traverse WALK and JUMP links automatically (default on)
	AI_AGENT_FLAG_KINEMATIC = 1u << 3,             // AI integrates the transform even with a character controller absent (Agents.md)
	AI_AGENT_FLAG_IGNORE_LOD = 1u << 4,            // always tier 0
	AI_AGENT_FLAG_FACE_MOVEMENT = 1u << 5,         // write a desired yaw from the velocity (default on)
};

/// @contract value Zenith_AIAgentComponent
/// Ownership: Component value stored through "Zenith.AI.Agent" (type version 1).
/// Thread affinity: Configuration CommandsOnly; request properties CommandsOnly (written by the AI.Agent command's playback);
///   state AINavigation, written by AI.Locomotion alone; AppliedVelocity CommandsOnly (AI's own follow-up command).
/// Invalidation: None.
/// Units: Metres, seconds, radians, m/s, m/s², ticks.
/// Errors: Invalid configuration (radius ≤ 0, height ≤ 0, speed < 0, non-finite) leaves the agent IDLE and counts
///   ai.agent.invalid_delta; an agent that is not a hierarchy root counts ai.agent.not_root and is never moved.
/// Version: since Z2 Stage 4; type version 1. The algorithms are Agents.md.
/// @endcontract
struct Zenith_AIAgentComponent
{
	Zenith_StringID m_xClass;                          // 1 ClassName: a class of the settings document
	uint8 m_eMedium = NAV_MEDIUM_GROUND;              // 2 Medium
	uint8 m_uCostProfile = 0;                         // 10 CostProfile, 0 … 7
	uint8 m_uAvoidancePriority = 128;                 // 8 AvoidancePriority: higher yields less
	uint8 m_uReserved0 = 0;
	float m_fRadius = 0.35f;                           // 3 Radius
	float m_fHeight = 1.8f;                            // 4 Height
	float m_fMaxSpeed = 3.5f;                          // 5 MaxSpeed
	float m_fMaxAcceleration = 8.0f;                   // 6 MaxAcceleration
	float m_fMaxTurnRateRadians = 6.2831853f;          // 7 MaxTurnRate (rad/s)
	float m_fAvoidanceHorizon = 2.0f;                  // 9 AvoidanceHorizon (s)
	uint32 m_uFlags = AI_AGENT_FLAG_AVOIDANCE | AI_AGENT_FLAG_AUTO_LINKS | AI_AGENT_FLAG_FACE_MOVEMENT;   // 11 Flags
	Zenith_Vector3 m_xMoveTarget;                      // 20 MoveTarget (world-local)
	Zenith_EntityHandle m_xMoveTargetEntity;           // 21 MoveTargetEntity: follow an entity (null: the fixed target)
	float m_fAcceptRadius = 0.5f;                      // 22 AcceptRadius
	float m_fSpeedScale = 1.0f;                        // 23 SpeedScale, 0 … 4
	uint32 m_uMoveSerial = 0;                         // 24 MoveSerial: incremented by every MOVE_TO and STOP playback
	uint8 m_eRequest = 0;                             // 25 Request: 0 none, 1 move, 2 stop, 3 pause, 4 resume
	uint8 m_auReserved1[3] = {};
	Zenith_Vector3 m_xAppliedVelocity;                 // 30 AppliedVelocity: the velocity AI applied last tick (any-row readable in phase 7)
	uint8 m_eMoveStatus = AI_MOVE_STATUS_IDLE;        // 40 MoveStatus
	uint8 m_ePathStatus = NAV_PATH_STATUS_NONE;       // 41 PathStatus of the latest plan
	uint8 m_uLinkKind = 0;                            // 42 LinkKind of the link being traversed
	uint8 m_uReserved2 = 0;
	Zenith_Vector3 m_xDesiredVelocity;                 // 43 DesiredVelocity: the velocity chosen this tick
	Zenith_Vector3 m_xNextCorner;                      // 44 NextCorner
	float m_fRemainingDistance = 0.0f;                 // 45 RemainingDistance along the straight path
	uint64 m_ulStatusTick = 0;                        // 46 StatusTick: the tick MoveStatus last changed
	uint32 m_uCompletedSerial = 0;                    // 47 CompletedSerial: the MoveSerial the last MoveCompleted answered
	float m_fDesiredYaw = 0.0f;                        // 48 DesiredYaw (radians): the turn-limited yaw chosen this tick; a root-motion
	                                                   //    agent's animation input (Agents.md "Locomotion output")
	static const Zenith_TypeID xTYPE_ID;               // Zenith_MakeTypeID("Zenith.AI.Agent")
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
};
```

### Agent properties

| IDs | Group | Authority | Serialized |
|---|---|---|---|
| 1 … 11 | configuration | `CommandsOnly` | yes |
| 20 … 25 | request | `CommandsOnly` (written by `AI.Agent` playback) | yes |
| 30 | applied velocity | `CommandsOnly` (written by `AI.Locomotion`'s follow-up `SetProperty`) | yes |
| 40 … 48 | state | `AINavigation` (`AI.Locomotion`) | yes |

Properties 20 MoveTarget and 44 NextCorner are registered with `PROPERTY_FLAG_WORLD_POSITION` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "ECS world-position flag", pending ECS through STREAM's row ST4 and core-doc item X227 (a)), so `Scene.ShiftWorldOrigin` shifts them by `(float)((double)p − S)` (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift"); 30 AppliedVelocity and 43 DesiredVelocity are velocities and 48 a yaw, invariant under a translation, and carry no flag.

```cpp
/// @contract enum Zenith_AIAwarenessLevel
/// Ownership: Plain enumeration; the level of a perceived target (Agents.md "Awareness model").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; stable (events and saves).
/// @endcontract
enum Zenith_AIAwarenessLevel : uint8
{
	AI_AWARENESS_UNAWARE = 0,       // awareness < SuspiciousThreshold
	AI_AWARENESS_SUSPICIOUS = 1,    // ≥ SuspiciousThreshold and < 1.0
	AI_AWARENESS_ALERT = 2,         // = 1.0 (clamped)
};

/// @contract enum Zenith_AISense
/// Ownership: Plain bit flags of the senses that contributed to a perceived target.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AISense : uint8
{
	AI_SENSE_NONE = 0,
	AI_SENSE_SIGHT = 1u << 0,
	AI_SENSE_HEARING = 1u << 1,
	AI_SENSE_DAMAGE = 1u << 2,
	AI_SENSE_TOUCH = 1u << 3,
	AI_SENSE_FORCED = 1u << 4,      // AI.Perception FORCE_AWARE
};

/// @contract value Zenith_AIPerceivedTarget
/// Ownership: Plain 48-byte struct "Zenith.AI.PerceivedTarget", the element of PerceptionState property 30 Memory.
/// Thread affinity: IMMUTABLE as a value; members non-const: a registered struct its containing component, payload or
///   result writes field by field.
/// Invalidation: m_xTarget follows Zenith_EntityHandle (a destroyed target's entry is removed at the next perception update).
/// Units: World-local metres; awareness in [0, 1]; ticks.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIPerceivedTarget
{
	Zenith_EntityHandle m_xTarget;                     // 1 Target
	uint64 m_ulTargetSimulationID = 0;                // 2 TargetSimulationID (sort key; persistent through saves by the entity reference)
	Zenith_Vector3 m_xLastKnownPosition;               // 3 LastKnownPosition
	float m_fAwareness = 0.0f;                         // 4 Awareness
	uint64 m_ulLastSensedTick = 0;                    // 5 LastSensedTick
	uint8 m_uSenses = 0;                              // 6 Senses (Zenith_AISense of the last update)
	uint8 m_eLevel = AI_AWARENESS_UNAWARE;            // 7 Level
	uint8 m_auReserved[6] = {};
};
static_assert(sizeof(Zenith_AIPerceivedTarget) == 48);

/// @contract value Zenith_AIPerceptionState
/// Ownership: Component value stored through "Zenith.AI.PerceptionState" (type version 1), on every perceiving entity.
/// Thread affinity: Configuration CommandsOnly; state and Memory AINavigation, written by AI.Perception alone.
/// Invalidation: None.
/// Units: Metres, radians, awareness per second, ticks.
/// Errors: Invalid configuration (range ≤ 0, angles outside [0, π], rates < 0) makes the perceiver sense nothing and counts
///   ai.perception.invalid_delta.
/// Version: since Z2 Stage 4; type version 1. The algorithms are Agents.md "Perception".
/// @endcontract
struct Zenith_AIPerceptionState
{
	float m_fSightRange = 30.0f;                       // 1 SightRange
	float m_fSightHalfAngle = 1.0471976f;              // 2 SightHalfAngle (60°)
	float m_fPeripheralRange = 10.0f;                  // 3 PeripheralRange
	float m_fPeripheralHalfAngle = 1.5707964f;         // 4 PeripheralHalfAngle (90°)
	float m_fEyeHeight = 1.6f;                         // 5 EyeHeight above the entity origin
	float m_fGainNear = 4.0f;                          // 6 GainNear: awareness per second at distance 0 in the primary cone
	float m_fGainFar = 0.5f;                           // 7 GainFar: at SightRange
	float m_fPeripheralGainScale = 0.25f;              // 8 PeripheralGainScale
	float m_fDecayPerSecond = 0.2f;                    // 9 Decay
	float m_fSuspiciousThreshold = 0.33f;              // 10 SuspiciousThreshold
	float m_fHearingScale = 1.0f;                      // 11 HearingScale (multiplies a noise's loudness)
	float m_fMemorySeconds = 20.0f;                    // 12 MemorySeconds: forget an UNAWARE target after this long unsensed
	uint32 m_uPerceiveTeamMask = 0xFFFFFFFFu;         // 13 PerceiveTeams (bit per team of ai.perception.teamNames)
	uint32 m_uSightLayerMask = 0xFFFFFFFFu;           // 14 SightBlockers (physics layers that block sight)
	uint8 m_uTeam = 0;                                // 15 Team of the perceiver (it never perceives its own entity)
	uint8 m_auReserved0[3] = {};
	Zenith_EntityHandle m_xPrimaryTarget;              // 20 PrimaryTarget: highest awareness, ties by lower simulation ID
	float m_fPrimaryAwareness = 0.0f;                  // 21 PrimaryAwareness
	uint8 m_ePrimaryLevel = AI_AWARENESS_UNAWARE;     // 22 PrimaryLevel
	uint8 m_auReserved1[3] = {};
	Zenith_Vector3 m_xPrimaryLastKnown;                // 23 PrimaryLastKnownPosition
	uint64 m_ulLastUpdateTick = 0;                    // 24 LastUpdateTick
	Zenith_ContainerRef m_xMemory;                     // 30 Memory: container<Zenith.AI.PerceivedTarget>, maxCount 16
	static const Zenith_TypeID xTYPE_ID;               // Zenith_MakeTypeID("Zenith.AI.PerceptionState")
};
```

### Perception properties

| IDs | Group | Authority | Serialized |
|---|---|---|---|
| 1 … 15 | configuration | `CommandsOnly` | yes |
| 20 … 24 | summary state | `AINavigation` | yes |
| 30 | Memory (`container<Zenith.AI.PerceivedTarget>`, maxCount `uAI_PERCEPTION_MAX_MEMORY`) | `AINavigation` | yes |

PerceptionState 23 PrimaryLastKnownPosition is registered with `PROPERTY_FLAG_WORLD_POSITION`, so SCENE shifts it. `Zenith.AI.PerceivedTarget` 3 LastKnownPosition (the container element struct) carries no flag: SCENE's shift visits component properties only, so AI's origin-shift listener shifts every Memory element itself (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift"; row AI19 (b)'s fallback).

```cpp
/// @contract value Zenith_AIStimulusSourceComponent
/// Ownership: Component value stored through "Zenith.AI.StimulusSource" (type version 1): an entity perceivers can see.
/// Thread affinity: Every property CommandsOnly (written by commands or scene data); read any-row by AI.StimulusGrid.
/// Invalidation: None.
/// Units: Metres; visibility in [0, 1].
/// Errors: A visibility outside [0, 1] is clamped and counted in ai.perception.invalid_delta.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIStimulusSourceComponent
{
	uint8 m_uTeam = 0;                                // 1 Team
	uint8 m_uFlags = 1;                               // 2 Flags: bit 0 visible, bit 1 hidden from every sense (stealth override)
	uint16 m_uReserved = 0;
	float m_fVisibility = 1.0f;                        // 3 Visibility (multiplies the sight gain; 0 = invisible)
	float m_fTargetHeight = 1.2f;                      // 4 TargetHeight: the sight ray's end above the entity origin
	float m_fRadius = 0.4f;                            // 5 Radius: the target's lateral half width (second and third rays)
	static const Zenith_TypeID xTYPE_ID;               // "Zenith.AI.StimulusSource"
};

/// @contract enum Zenith_AINavObstacleShape
/// Ownership: Plain enumeration; property 1 Shape of Zenith.AI.NavObstacle.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is AI_ERROR_OBSTACLE_INVALID (counted, ignored).
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AINavObstacleShape : uint8
{
	AI_NAV_OBSTACLE_CYLINDER = 1,
	AI_NAV_OBSTACLE_BOX = 2,
};

/// @contract enum Zenith_AINavObstacleMode
/// Ownership: Plain enumeration; property 2 Mode of Zenith.AI.NavObstacle.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: CARVE on a tile without a layers section falls back to MARK and counts ai.nav.carve_fallback_delta.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AINavObstacleMode : uint8
{
	AI_NAV_OBSTACLE_MARK = 1,       // flag every overlapped polygon BLOCKED (or re-area it); no rebuild
	AI_NAV_OBSTACLE_CARVE = 2,      // rebuild the overlapped carve-capable tiles from their layers with the footprint cut
};

/// @contract value Zenith_AINavObstacleComponent
/// Ownership: Component value stored through "Zenith.AI.NavObstacle" (type version 1).
/// Thread affinity: Every property CommandsOnly; the obstacle's pose is the entity's Zenith.LocalTransform read by AI.NavIngest.
/// Invalidation: None.
/// Units: Metres, radians.
/// Errors: Invalid extents are AI_ERROR_OBSTACLE_INVALID (counted; the obstacle is ignored).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavObstacleComponent
{
	uint8 m_eShape = AI_NAV_OBSTACLE_CYLINDER;        // 1 Shape
	uint8 m_eMode = AI_NAV_OBSTACLE_CARVE;            // 2 Mode
	uint8 m_uArea = uAI_NAV_AREA_NULL;                // 3 Area: NULL blocks; another code re-areas
	uint8 m_bEnabled = 1;                             // 4 Enabled
	Zenith_Vector3 m_xHalfExtents{ 0.5f, 1.0f, 0.5f }; // 5 HalfExtents (cylinder: x radius, y half height)
	float m_fMoveThreshold = 0.25f;                    // 6 MoveThreshold: metres of movement before the obstacle re-applies
	static const Zenith_TypeID xTYPE_ID;               // "Zenith.AI.NavObstacle"
};

/// @contract enum Zenith_AIOffMeshLinkKind
/// Ownership: Plain enumeration; property 1 Kind of Zenith.AI.OffMeshLink.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Another value is AI_ERROR_LINK_INVALID.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AIOffMeshLinkKind : uint8
{
	AI_OFFMESH_LINK_WALK = 1,       // AI moves the agent in a straight line over Duration
	AI_OFFMESH_LINK_JUMP = 2,       // AI moves the agent along a parabola of apex JumpHeight over Duration
	AI_OFFMESH_LINK_SCRIPTED = 3,   // the agent waits (WAITING_LINK) until AI.CompleteOffMeshLink; scripts animate it
	AI_OFFMESH_LINK_TELEPORT = 4,   // AI records Zenith_RecordTeleport to the end point
};

/// @contract value Zenith_AIOffMeshLinkComponent
/// Ownership: Component value stored through "Zenith.AI.OffMeshLink" (type version 1); the link's endpoints are relative to the
///   entity's Zenith.LocalTransform, so a link moves with its entity (a re-register is triggered by MoveThreshold).
/// Thread affinity: Configuration CommandsOnly; property 10 Enabled CommandsOnly (AI.OffMeshLink ENABLE/DISABLE).
/// Invalidation: None.
/// Units: Metres, seconds.
/// Errors: An endpoint more than SnapDistance from every polygon of the class makes the link unregistered
///   (ai.nav.links_unresolved, counted, retried when a tile is added).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIOffMeshLinkComponent
{
	uint8 m_eKind = AI_OFFMESH_LINK_WALK;             // 1 Kind
	uint8 m_bBidirectional = 1;                       // 2 Bidirectional
	uint8 m_uArea = NAV_AREA_GROUND;                  // 3 Area (its cost and flags apply to the link)
	uint8 m_uClassMask = 0x0F;                        // 4 ClassMask: the ground classes the link joins
	Zenith_Vector3 m_xStart;                           // 5 Start (entity-local)
	Zenith_Vector3 m_xEnd;                             // 6 End (entity-local)
	float m_fSnapDistance = 0.5f;                      // 7 SnapDistance
	float m_fDurationSeconds = 1.0f;                   // 8 Duration of an automatic traversal
	float m_fJumpHeight = 1.0f;                        // 9 JumpHeight
	uint8 m_bEnabled = 1;                             // 10 Enabled
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;               // "Zenith.AI.OffMeshLink"
};

/// @contract value Zenith_AINavMeshRefComponent
/// Ownership: Component value "Zenith.AI.NavMeshRef" (type version 1): a world's whole navigation manifest (non-streamed worlds).
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: None.
/// Errors: A handle whose asset is not a MANIFEST is counted (ai.nav.wrong_role) and ignored.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavMeshRefComponent
{
	Zenith_AssetHandle<Zenith_NavMeshAsset> m_xManifest;    // 1 Manifest
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.NavMeshRef"
};

/// @contract value Zenith_AINavMeshTilesComponent
/// Ownership: Component value "Zenith.AI.NavMeshTiles" (type version 1): the tiles of one STREAM navigation cell, written into
///   cell scenes by WORLD's partition cook from AI's cell contribution (Navigation.md "Streamed tiles").
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: None.
/// Errors: A TILE handle that is null or of another role is skipped and counted (ai.nav.wrong_role).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavMeshTilesComponent
{
	Zenith_AssetHandle<Zenith_NavMeshAsset> m_axTiles[16];  // 1 Tiles (a 128 m cell holds 16 tiles of 32 m; unused entries null)
	uint8 m_uCount = 0;                                    // 2 Count
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.NavMeshTiles"
};

/// @contract value Zenith_AINavVolumeRefComponent
/// Ownership: Component value "Zenith.AI.NavVolumeRef" (type version 1): one volume region.
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_AINavMeshTilesComponent.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavVolumeRefComponent
{
	Zenith_AssetHandle<Zenith_NavMeshAsset> m_xVolume;      // 1 Volume
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.NavVolumeRef"
};

/// @contract value Zenith_AINavAreaVolumeComponent
/// Ownership: Component value "Zenith.AI.NavAreaVolume" (type version 1): authoring data the bake reads from scene documents;
///   inert at run time (no AI system declares access to it).
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: Metres (the box is the entity's transform scaled by HalfExtents).
/// Errors: Reported by the bake (AI_ERROR_INVALID_DESC naming the entity's persistent ID).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AINavAreaVolumeComponent
{
	Zenith_StringID m_xArea;                                // 1 Area: an area name of the settings document, or "null"
	Zenith_Vector3 m_xHalfExtents{ 1.0f, 1.0f, 1.0f };      // 2 HalfExtents
	uint8 m_uPriority = 0;                                 // 3 Priority
	uint8 m_uClassMask = 0x0F;                             // 4 ClassMask
	uint8 m_auReserved[2] = {};
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.NavAreaVolume"
};

/// @contract value Zenith_AISignificanceComponent
/// Ownership: Component value "Zenith.AI.Significance" (type version 1) on any agent or perceiver; absent means tier 0.
/// Thread affinity: Tier and Distance AINavigation (AI.Significance); IgnoreFocus CommandsOnly.
/// Invalidation: None.
/// Units: Metres; ticks.
/// Errors: None.
/// Version: since Z2 Stage 4. The algorithm is Agents.md "Simulation LOD".
/// @endcontract
struct Zenith_AISignificanceComponent
{
	uint8 m_uTier = 0;                                     // 1 Tier, 0 … 3
	uint8 m_bIgnoreFocus = 0;                              // 2 Pinned: stays tier 0
	uint8 m_auReserved[2] = {};
	float m_fFocusDistance = 0.0f;                          // 3 FocusDistance: to the nearest LOD focus
	uint32 m_uInterval = 1;                                // 4 Interval: ticks between full updates (1, 2, 4, 8)
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.Significance"
};

/// @contract value Zenith_AILODFocusComponent
/// Ownership: Component value "Zenith.AI.LODFocus" (type version 1) on the entities AI significance is measured from (player pawns).
/// Thread affinity: CommandsOnly.
/// Invalidation: None.
/// Units: None.
/// Errors: More than uAI_MAX_LOD_FOCI foci: the lowest simulation IDs are used, the rest counted (ai.lod.foci_ignored).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AILODFocusComponent
{
	float m_fWeight = 1.0f;                                 // 1 Weight: distances to this focus are divided by it
	static const Zenith_TypeID xTYPE_ID;                    // "Zenith.AI.LODFocus"
};
```

### Navigation reference properties

| Type | IDs | Authority | Serialized | Read by |
|---|---|---|---|---|
| `Zenith.AI.NavMeshRef` | 1 | `CommandsOnly` | yes | `AI.NavIngest` |
| `Zenith.AI.NavMeshTiles` | 1, 2 | `CommandsOnly` | yes | `AI.NavIngest` |
| `Zenith.AI.NavVolumeRef` | 1 | `CommandsOnly` | yes | `AI.NavIngest` |
| `Zenith.AI.NavObstacle` | 1 … 6 | `CommandsOnly` | yes | `AI.NavIngest` |
| `Zenith.AI.OffMeshLink` | 1 … 10 | `CommandsOnly` | yes | `AI.NavIngest` |
| `Zenith.AI.NavAreaVolume` | 1 … 4 | `CommandsOnly` | yes | the cook step `ai.navMesh` only |
| `Zenith.AI.StimulusSource` | 1 … 5 | `CommandsOnly` | yes | `AI.StimulusGrid` |
| `Zenith.AI.Significance` | 1, 3, 4 `AINavigation`; 2 `CommandsOnly` | as stated | yes | every AI node |
| `Zenith.AI.LODFocus` | 1 | `CommandsOnly` | yes | `AI.Significance` |

## Zenith_AIEvents header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AIEvents.h
#pragma once
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithAI/Zenith_AIComponents.h"

/// @contract enum Zenith_AIMoveResult
/// Ownership: Plain enumeration; the Result of Zenith.AI.MoveCompleted.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; stable.
/// @endcontract
enum Zenith_AIMoveResult : uint8
{
	AI_MOVE_RESULT_ARRIVED = 1,
	AI_MOVE_RESULT_UNREACHABLE = 2,     // NO_START, NO_END, or PARTIAL without ALLOW_PARTIAL
	AI_MOVE_RESULT_STOPPED = 3,         // an AI.Stop, or a new MoveTo that superseded this one
	AI_MOVE_RESULT_FAILED = 4,          // stuck (Agents.md "Stuck detection"), invalid configuration, or no navmesh for 600 ticks
	AI_MOVE_RESULT_PARTIAL_ARRIVED = 5, // arrived at the end of a PARTIAL path (ALLOW_PARTIAL)
};

/// @contract value Zenith_AIMoveCompletedEvent
/// Ownership: Event value "Zenith.AI.MoveCompleted" emitted by AI.Locomotion on the agent's simulation ID; delivered in phase 9.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: m_xAgent follows Zenith_EntityHandle.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. One event per MoveSerial at most (Agents.md "Agent state machine").
/// @endcontract
struct Zenith_AIMoveCompletedEvent
{
	Zenith_EntityHandle m_xAgent;          // 1 Agent
	uint32 m_uMoveSerial = 0;             // 2 MoveSerial
	uint8 m_eResult = 0;                  // 3 Result
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;   // "Zenith.AI.MoveCompleted"
};

/// @contract value Zenith_AIOffMeshLinkReachedEvent
/// Ownership: Event value "Zenith.AI.OffMeshLinkReached", emitted when an agent starts a link.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As above.
/// Units: World-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIOffMeshLinkReachedEvent
{
	Zenith_EntityHandle m_xAgent;          // 1 Agent
	Zenith_EntityHandle m_xLink;           // 2 Link (the link's entity)
	Zenith_Vector3 m_xStart;               // 3 Start
	Zenith_Vector3 m_xEnd;                 // 4 End
	uint8 m_eKind = 0;                    // 5 Kind
	uint8 m_auReserved[3] = {};
	static const Zenith_TypeID xTYPE_ID;   // "Zenith.AI.OffMeshLinkReached"
};

/// @contract value Zenith_AIStimulusEvent
/// Ownership: Event value "Zenith.AI.Stimulus", emitted by AI.Perception on the perceiver's simulation ID when a target's
///   awareness level changes.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As above.
/// Units: Awareness in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIStimulusEvent
{
	Zenith_EntityHandle m_xPerceiver;      // 1 Perceiver
	Zenith_EntityHandle m_xTarget;         // 2 Target
	uint8 m_eFromLevel = 0;               // 3 From
	uint8 m_eToLevel = 0;                 // 4 To
	uint8 m_uSenses = 0;                  // 5 Senses
	uint8 m_uReserved = 0;
	float m_fAwareness = 0.0f;             // 6 Awareness
	static const Zenith_TypeID xTYPE_ID;   // "Zenith.AI.Stimulus"
};

/// @contract value Zenith_AITargetForgottenEvent
/// Ownership: Event value "Zenith.AI.TargetForgotten", emitted when a memory entry is removed.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: As above.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AITargetForgottenEvent
{
	Zenith_EntityHandle m_xPerceiver;      // 1 Perceiver
	Zenith_EntityHandle m_xTarget;         // 2 Target (may be stale: the target was destroyed)
	uint8 m_uReason = 0;                  // 3 Reason: 1 decayed, 2 destroyed, 3 forced (AI.ForgetTarget), 4 capacity
	uint8 m_auReserved[7] = {};
	static const Zenith_TypeID xTYPE_ID;   // "Zenith.AI.TargetForgotten"
};
```

## Zenith_AICommands header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AICommands.h
#pragma once
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithAI/Zenith_AIComponents.h"

/// @contract value Zenith_AICommandKinds
/// Ownership: Plain value: the command-kind IDs AI resolved at its first AttachWorld (FindCommandKind, AI.md "Assumed interfaces").
/// Thread affinity: IMMUTABLE after the first AttachWorld; members non-const: the module fills it at AttachWorld.
/// Invalidation: None while the module is started.
/// Units: None.
/// Errors: A zero ID means the module is not started: the recording functions record nothing and count
///   ai.command.unresolved_kind_delta.
/// Version: since Z2 Stage 4. One member per row of AI.md "Command kinds", in that order.
/// @endcontract
struct Zenith_AICommandKinds
{
	uint32 m_uAgent = 0;
	uint32 m_uNoise = 0;
	uint32 m_uStimulus = 0;
	uint32 m_uBlackboard = 0;
	uint32 m_uOffMeshLink = 0;
	uint32 m_uPerception = 0;
};

/// @contract value Zenith_AIMoveRequest
/// Ownership: Plain value recorded into an AI.Agent MOVE_TO payload.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: m_xTargetEntity follows Zenith_EntityHandle.
/// Units: World-local metres.
/// Errors: Non-finite values or an accept radius outside (0, 100] make the playback count ai.command.invalid_delta and ignore it.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AIMoveRequest
{
	Zenith_Vector3 m_xTarget;
	Zenith_EntityHandle m_xTargetEntity;   // null: move to m_xTarget
	float m_fAcceptRadius = 0.5f;
	float m_fSpeedScale = 1.0f;            // 0 … 4; 0 keeps the current scale
};

void Zenith_RecordAIMoveTo(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xAgent,
	const Zenith_AIMoveRequest& xRequest);
void Zenith_RecordAIStop(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xAgent);
void Zenith_RecordAIPause(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xAgent,
	bool bPaused);
void Zenith_RecordAISetSpeedScale(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xAgent,
	float fSpeedScale);
void Zenith_RecordAISetMedium(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xAgent,
	Zenith_NavMedium eMedium);
// A noise heard by perceivers in the phase-7 node that consumes it (Agents.md "Hearing algorithm").
void Zenith_RecordAINoise(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, const Zenith_Vector3& xPosition,
	float fLoudness, Zenith_EntityHandle xInstigator, uint8 uTeam);
// A damage or touch stimulus on a perceiver (Agents.md "Damage and touch").
void Zenith_RecordAIStimulus(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xPerceiver,
	Zenith_EntityHandle xInstigator, Zenith_AISense eSense);
void Zenith_RecordAIForgetTarget(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xPerceiver,
	Zenith_EntityHandle xTarget);
void Zenith_RecordAIForceAware(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xPerceiver,
	Zenith_EntityHandle xTarget);
void Zenith_RecordAICompleteOffMeshLink(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds,
	Zenith_EntityHandle xAgent);
void Zenith_RecordAISetLinkEnabled(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xLink,
	bool bEnabled);

/// @contract value Zenith_AIBlackboardValue
/// Ownership: Plain 48-byte tagged value of the blackboard store (Agents.md "Blackboard store").
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: m_xEntity follows Zenith_EntityHandle.
/// Units: As the value; kind 6 in world-local metres.
/// Errors: None.
/// Version: since Z2 Stage 4. Kinds: 0 none, 1 bool, 2 number (f64), 3 vector (3 × f32, never shifted), 4 entity, 5 name
///   (interned string), 6 position (3 × f32, world-local, shifted at an origin rebase, Navigation.md "Origin shift").
/// @endcontract
struct Zenith_AIBlackboardValue
{
	uint8 m_eKind = 0;
	uint8 m_auReserved[7] = {};
	double m_dNumber = 0.0;                // kinds 1 (0 or 1) and 2
	Zenith_Vector3 m_xVector;              // kinds 3 and 6
	uint32 m_uReserved2 = 0;
	Zenith_EntityHandle m_xEntity;         // kind 4
	Zenith_StringID m_xName;               // kind 5
	uint32 m_uReserved3 = 0;
};
static_assert(sizeof(Zenith_AIBlackboardValue) == 48);

void Zenith_RecordAIBlackboardSet(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xOwner,
	Zenith_StringID xKey, const Zenith_AIBlackboardValue& xValue, uint64 ulExpiresTick);   // 0 = never expires
void Zenith_RecordAIBlackboardClear(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds, Zenith_EntityHandle xOwner,
	Zenith_StringID xKey);
void Zenith_RecordAIBlackboardClearAll(Zenith_CommandBuffer& xBuffer, const Zenith_AICommandKinds& xKinds,
	Zenith_EntityHandle xOwner);
```

Every recording function writes one record of its kind with the payload of `Zenith2/Docs/Design/AI/Formats.md` → "Command payloads", reserving its bytes through `Zenith_CommandBuffer::Reserve` first; a reservation failure records nothing and counts `ai.command.dropped_delta` (C++ callers) — a script caller's capacity was already accepted at its merge.

## Zenith_AIWorldResource header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AIWorldResource.h
#pragma once
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithAI/Zenith_NavMesh.h"
#include "ZenithAI/Zenith_NavVolume.h"
#include "ZenithAI/Zenith_AICommands.h"

/// @contract type Zenith_AIWorldResource
/// Ownership: One per world, owned by the AI module's world state; registered by AttachWorld as world resource "Zenith.AI.World"
///   with owning phase PHASE_AI_NAVIGATION; borrowed by readers through Zenith_SystemContext::GetResource.
/// Thread affinity: Const members ANY_THREAD for every node declaring ReadResource of "Zenith.AI.World" (phases 1 to 12 and the
///   epilogue), except ComputeStateHash; AI.NavIngest is the one writer (phase 7). Script READ_WORLD bindings list the
///   resource in Uses. ComputeStateHash is MAIN_THREAD with no frame of the world running (the ai.dump command, the
///   determinism tests, the replay hash hook between ticks), because it also reads Zenith.AI.PathService,
///   Zenith.AI.AgentRuntime, Zenith.AI.PerceptionLedger, Zenith.AI.Blackboards and Zenith.AI.NoiseBuffer, which a
///   Zenith.AI.World reader has not declared.
/// Invalidation: References returned are valid for the reading invocation.
/// Units: As Zenith_NavMesh.
/// Errors: As Zenith_NavMesh.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_AIWorldResource
{
public:
	const Zenith_NavMesh& GetNavMesh() const;
	const Zenith_NavVolumeSet& GetVolumes() const;
	const Zenith_AICommandKinds& GetCommandKinds() const;
	uint32 GetTeamIndex(Zenith_StringView strTeam) const;    // 0xFFFFFFFF when not in ai.perception.teamNames
	uint64 ComputeStateHash() const;                         // AI.md "Canonical AI state hash"; MAIN_THREAD, no frame
	static const Zenith_TypeID xTYPE_ID;                                             // Zenith_MakeTypeID("Zenith.AI.World")
};

/// @contract type Zenith_AIBlackboardStore
/// Ownership: One per world, owned by the AI module's world state; world resource "Zenith.AI.Blackboards", owning phase
///   PHASE_AI_NAVIGATION, with no system writer: only the playback of AI.Blackboard commands writes it (the Zenith.WorldOrigin
///   pattern; the first playback of a tick also prunes expired records), the world-destroy and entity-destroy steps of
///   Agents.md "Blackboard store" prune it, and the origin-shift listener shifts its POSITION values at sync point 1.
/// Thread affinity: Const members ANY_THREAD for nodes declaring ReadResource of "Zenith.AI.Blackboards"; they read the store as
///   the latest sync point left it.
/// Invalidation: None.
/// Units: Ticks for expiry.
/// Errors: Find returns false for an absent or expired key.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_AIBlackboardStore
{
public:
	bool Find(uint64 ulOwnerSimulationID, Zenith_StringID xKey, uint64 ulTick,
		Zenith_AIBlackboardValue& xValueOut) const;
	uint32 GetKeyCount(uint64 ulOwnerSimulationID, uint64 ulTick) const;
	uint32 GetEntryCount() const;
	static const Zenith_TypeID xTYPE_ID;                                             // "Zenith.AI.Blackboards"
};
```

## Zenith_AIDebug header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_AIDebug.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_AIDebugView
/// Ownership: Plain bit flags of the configuration key ai.debug.views (AI.md "Debug views").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Unknown bits are ignored.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_AIDebugView : uint32
{
	AI_DEBUG_VIEW_NONE = 0,
	AI_DEBUG_VIEW_NAVMESH = 1u << 0,          // polygons coloured by area, outlined
	AI_DEBUG_VIEW_TILES = 1u << 1,            // tile bounds, key text, carved tiles in orange
	AI_DEBUG_VIEW_LINKS = 1u << 2,            // off-mesh links (arcs), unresolved links in red
	AI_DEBUG_VIEW_OBSTACLES = 1u << 3,        // obstacle shapes and blocked polygons
	AI_DEBUG_VIEW_PATHS = 1u << 4,            // corridors (polygons) and straight paths (corners)
	AI_DEBUG_VIEW_STEERING = 1u << 5,         // preferred (green) and chosen (white) velocities, next corner
	AI_DEBUG_VIEW_AVOIDANCE = 1u << 6,        // neighbours and ORCA half-planes of the selected agent
	AI_DEBUG_VIEW_PERCEPTION = 1u << 7,       // sight cones, peripheral cones, LOS rays (green visible, red blocked)
	AI_DEBUG_VIEW_MEMORY = 1u << 8,           // last known positions and awareness bars
	AI_DEBUG_VIEW_NOISE = 1u << 9,            // noise spheres for 2 s
	AI_DEBUG_VIEW_VOLUMES = 1u << 10,         // occupied volume nodes in a 20 m box around the selected agent
	AI_DEBUG_VIEW_LOD = 1u << 11,             // tier colour per agent
	AI_DEBUG_VIEW_TRACE = 1u << 12,           // the selected agent's last decision trace records as text
};

inline constexpr uint32 uAI_DEBUG_TRACE_RECORDS_PER_AGENT = 64;
inline constexpr uint32 uAI_DEBUG_TRACE_AGENTS = 256;
```

## Zenith_Module_AI header

```cpp
// Zenith2/Engine/ZenithAI/Zenith_Module_AI.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

Zenith_Result<void, Zenith_ModuleError> Zenith_Module_AI_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_AI_Start(
	Zenith_ModuleStartContext& xContext);
```

The header declares the two entry points and nothing else ([Z2-N-ARCH-168]). `AI` declares the module dependency `Physics` (`Zenith2/Docs/Architecture.md` → "Engine modules and game projects"), so a game selecting AI selects Physics; AI's start reads nothing of the Physics module but its world resource, through system declarations.

## Configuration keys

Registered in `Zenith_Module_AI_RegisterTypes` through the configuration schema (`Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"). Every key is `restart` scope (read at module start), except the two `ai.debug.*` keys, which are `live` in development builds and absent in Shipping.

| Key | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `ai.path.maxSearchNodes` | int | 2048 | 256 … 8192 | node pool per path search (`Zenith_NavPathSearchDesc::m_uMaxNodes`) |
| `ai.path.expansionsPerRequestPerTick` | int | 512 | 32 … 8192 | the slice of one active request per tick |
| `ai.path.maxActiveRequests` | int | 64 | 1 … 1024 | active requests progressed per tick: Z2-B-AI-003's value once allocated (pending, allocated by the Stage 1 per-system budget ticket for AI) |
| `ai.path.maxQueuedRequests` | int | 4096 | 64 … 16384 | the path service queue (agents' internal requests and accepted script requests); start refuses a value below `ai.agent.maxAgents` + the pathfinding value of Z2-B-SCRIPT-026 (Navigation.md "Request queue") |
| `ai.path.maxTicksPerRequest` | int | 16 | 1 … 600 | after this many ticks an active request finishes as PARTIAL |
| `ai.nav.maxTileRebuildsPerTick` | int | 4 | 1 … 64 | carve rebuilds per world per tick |
| `ai.nav.maxTiles` | int | 16384 | 64 … 65536 | the tile slot table per world |
| `ai.nav.maxLoadsInFlight` | int | 1024 | 16 … 16384 | navigation load records per world from completion-ID allocation to publication: the `uMaxInFlight` of `ASYNC_SOURCE_NAVIGATION` and the capacity of the completion queue `ai.loads.w<tag>` (Navigation.md "Navigation asset requests"); simulation-affecting (it decides which needs issue in a tick) |
| `ai.agent.maxAgents` | int | 1024 | 16 … 4096 | the agent runtime table per world |
| `ai.agent.retargetDistance` | float | 1.0 | 0.1 … 50 | metres an entity target moves before a replan |
| `ai.agent.stuckSeconds` | float | 3.0 | 0.5 … 60 | progress window of stuck detection |
| `ai.agent.optimizeIntervalTicks` | int | 16 | 1 … 600 | corridor visibility optimisation period |
| `ai.avoid.maxNeighbours` | int | 8 | 1 … 8 | ORCA neighbours per agent at tier 0 |
| `ai.avoid.neighbourRadius` | float | 4.0 | 0.5 … 32 | metres; the crowd grid's query radius |
| `ai.lod.tierDistances` | string | `"30,60,120"` | three ascending positive numbers | tier boundaries in metres |
| `ai.lod.tierIntervals` | string | `"1,2,4,8"` | four powers of two, ascending, ≤ 64 | full-update interval per tier |
| `ai.perception.teamNames` | string | `"Neutral,Player,Enemy"` | 1 … 32 names `^[A-Za-z][A-Za-z0-9_]{0,30}$` | team index i is the i-th name |
| `ai.perception.maxRaysPerTick` | int | 512 | 16 … 8192 | sight rays per world per tick (Agents.md "Sight algorithm") |
| `ai.perception.maxNoisesPerTick` | int | 256 | 16 … 256 | the noise buffer per tick |
| `ai.debug.views` | int | 0 | bits of `Zenith_AIDebugView` | development builds |
| `ai.debug.selectedAgent` | string | `""` | a persistent ID in text form | the agent the AVOIDANCE, VOLUMES and TRACE views show |

A malformed value is `MODULE_ERROR_REGISTRATION` at start with the key and the reason logged in category `ai`, so the game does not start (`Test_AIModule_Start_RejectsBadConfiguration`).

## Authority census rows

Rows AI adds to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the diff that registers each type (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Census rows added by subsequent designs"); `Library` is `ZenithAI`, `Stage` is `S4`, and the census test that compares them is `Test_AuthorityCensus_OpenWorldModules`.

| Stable name | Kind | Properties | Owning phase | Writer sources | Arbitration | Writers (system names) | Readers outside the owning phase |
|---|---|---|---|---|---|---|---|
| `Zenith.AI.Agent` | component | 1 … 11 configuration, 20 … 25 request, 30 AppliedVelocity | CommandsOnly | — | none | `AI.Agent` playback; `AI.Locomotion`'s follow-up `SetProperty` (30) | scripts (2, 10), `AI.CrowdGrid` (30, any-row) |
| `Zenith.AI.Agent` | component | 40 … 48 state | AINavigation | — | none | `AI.Locomotion` | scripts (2, 10; a root-motion agent's Late behaviour reads 43, 44, 48), extractors (12) |
| `Zenith.AI.PerceptionState` | component | 1 … 15 | CommandsOnly | — | none | scene data, commands | `AI.Perception` |
| `Zenith.AI.PerceptionState` | component | 20 … 24, 30 | AINavigation | — | none | `AI.Perception` | scripts (2, 10) |
| `Zenith.AI.StimulusSource` | component | 1 … 5 | CommandsOnly | — | none | scene data, commands | `AI.StimulusGrid` |
| `Zenith.AI.NavObstacle`, `.OffMeshLink`, `.NavMeshRef`, `.NavMeshTiles`, `.NavVolumeRef`, `.NavAreaVolume`, `.LODFocus` | component | all | CommandsOnly | — | none | scene data, commands (`AI.OffMeshLink` writes 10) | `AI.NavIngest`, `AI.Significance` |
| `Zenith.AI.Significance` | component | 1, 3, 4 | AINavigation | — | none | `AI.Significance` | every AI node after it |
| `Zenith.AI.Significance` | component | 2 | CommandsOnly | — | none | scene data, commands | `AI.Significance` |
| `Zenith.AI.World` | resource | — | AINavigation | — | none | `AI.NavIngest` | scripts (READ_WORLD), the save snapshot (11), debug epilogue |
| `Zenith.AI.Blackboards` | resource | — | AINavigation | — | none | `AI.Blackboard` playback only | scripts (READ_WORLD), the save snapshot (11) |
| `Zenith.AI.CrowdGrid`, `.StimulusGrid`, `.PathService`, `.AgentRuntime`, `.PerceptionLedger`, `.NoiseBuffer` | resource | — | AINavigation | — | none | `AI.CrowdGrid`, `AI.StimulusGrid`, `AI.NavRequests`, `AI.Locomotion` (and its merge node), `AI.Perception` (and its merge node); `AI.Noise` playback | the save snapshot (11) |
| `Zenith.AI.MoveCompleted`, `.OffMeshLinkReached`, `.Stimulus`, `.TargetForgotten` | event | — | — | — | none | `AI.Locomotion`, `AI.Perception` | scripts and handlers (9, 10) |

AI also writes two properties of other areas' components, always through commands at the sync point after phase 7: `Zenith.Physics.CharacterController` properties 20 DesiredVelocity, 22 DesiredYawRadians and 23 InputFlags (owned by `EarlyScripts`, `Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_CharacterController header"), and `Zenith.LocalTransform` properties 1 Position and 2 Rotation (owned by `PostPhysics`, "every other writer through `SetProperty`"); both readings join those rows' "Writers" cells as `AI.Locomotion (command)`.
