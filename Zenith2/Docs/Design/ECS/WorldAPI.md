# ECS design — the ZenithECS public API

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It declares the public headers of `Zenith2/Engine/ZenithECS/` with their contract blocks, in Zenith Hungarian, except the four system and scheduling headers (`Zenith_System.h`, `Zenith_SystemContext.h`, `Zenith_MergeContext.h`, `Zenith_ScheduleErrors.h`), which `Zenith2/Docs/Design/ECS/SystemAPI.md` declares under the same conventions; § "Header list" names every header of the library and the file that declares it. The specification it elaborates is `Zenith2/Docs/Architecture/ECSModel.md` (every type that file declares keeps its declared members and meaning here), `Zenith2/Docs/Architecture/FramePhases.md` → "Schedule-compile errors" and "Arbitration rules", `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder" and `Zenith2/Docs/Contracts.md` → "Handle kinds". A line `<!-- elaboration -->` marks a member or type this design adds to the specification; each addition is listed in `Zenith2/Docs/Design/ECS.md` → "Reconciliation items" so the core document can adopt it.

A member or type whose comment says `consumer seam` is declared by the slice that implements it, never by the API-only slices (`Zenith2/Docs/Design/ECS.md` → "API-only slices").

Code blocks use tabs. Every fallible function returns a `Zenith_Result`. The contract blocks follow `Zenith2/Docs/Contracts.md` → "Contract blocks per type" exactly; a `functions` block covers the free functions after it. Every header begins with `#pragma once` and includes only its own library's public headers, the public headers of ZenithCore, ZenithReflect, ZenithGraph, ZenithJobs and ZenithProfiling, and the allowed standard headers.

## Header list

| Header (`Zenith2/Engine/ZenithECS/`) | Declares | Declared in |
|---|---|---|
| `Zenith_ECSTypes.h` | `Zenith_ECSErrorCode`, `Zenith_ECSError`, `Zenith_ToString(Zenith_ECSErrorCode)`, `Zenith_PropertyValue`, `Zenith_ComponentInit`, `Zenith_SpawnTicket`, `Zenith_QuerySlot`, `Zenith_EntityLocation`, `Zenith_WorldStats`, `Zenith_ContainerRef`, the ECS limits | this file |
| `Zenith_EntityHandle.h` | `Zenith_EntityHandle`, the `Zenith_ReflectValueTraits<Zenith_EntityHandle>` specialisation | this file |
| `Zenith_ComponentHandle.h` | `Zenith_ComponentHandle` | this file |
| `Zenith_EntityTable.h` | `Zenith_EntityTableDesc`, `Zenith_EntitySlotFlags`, `Zenith_EntityTable` (backend support) | this file |
| `Zenith_ContainerHeap.h` | `Zenith_ContainerHeap` (backend support) | this file |
| `Zenith_ChunkView.h` | `Zenith_InvocationScope`, `Zenith_IsVersionNewer`, `Zenith_RawColumn`, `Zenith_ChunkColumnBinding`, `Zenith_ChunkBinding`, `Zenith_ChunkView` | this file |
| `Zenith_Query.h` | `Zenith_QueryTermOp`, `Zenith_QueryTerm`, `Zenith_QueryDesc`, `Zenith_QueryID` | this file |
| `Zenith_World.h` | `Zenith_WorldDesc`, `Zenith_SyncPointToken`, `Zenith_PlaybackWrite`, `Zenith_WorldPlaybackAccess`, `Zenith_World`, `Zenith_PfnCreateWorld`, `Zenith_WorldTestAccess` | this file |
| `Zenith_CommandBuffer.h` | `Zenith_MergeKey`, `Zenith_SpawnDesc`, `Zenith_CommandBuffer` | this file |
| `Zenith_CommandKinds.h` | `Zenith_PfnCommandPlayback`, `Zenith_CommandKindFlags`, `Zenith_CommandKindDesc`, `Zenith_CommandKindTable` | this file |
| `Zenith_System.h` | `Zenith_SystemKind`, `Zenith_PfnSystemRun`, `Zenith_PfnSystemMerge`, `Zenith_SystemDesc`, `Zenith_EpilogueStageDesc`, `Zenith_ReductionKind`, `Zenith_ReductionDesc`, `Zenith_ReductionSlot`, `Zenith_SystemBuilder`, `Zenith_SystemRegistryDesc`, `Zenith_SystemReloadScope`, `Zenith_SystemRegistry` | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_System header" |
| `Zenith_SystemContext.h` | `Zenith_EventWriter`, `Zenith_EventRecordView`, `Zenith_EventReader`, `Zenith_ContributionWriter`, `Zenith_SystemContext` | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header" |
| `Zenith_MergeContext.h` | `Zenith_ECSCapacityClass`, `Zenith_ECSCapacityDemand`, `Zenith_MergeCapacity`, `Zenith_MergeContext` | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_MergeContext header" |
| `Zenith_ScheduleErrors.h` | `Zenith_ScheduleErrorCode`, `Zenith_ScheduleDiagnostic`, `Zenith_ScheduleError`, `Zenith_ToString(Zenith_ScheduleErrorCode)` | `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_ScheduleErrors header" |
| `Zenith_WorldSimulation.h` | `Zenith_SpawnResult`, `Zenith_WorldSimulation` | this file |
| `Zenith_WorldRegistry.h` | `Zenith_WorldTagIssue`, `Zenith_WorldRegistry` | this file |
| `Zenith_WorldInspector.h` | `Zenith_WorldInspector` | this file |
| `Zenith_ECSObservers.h` | `Zenith_StructuralEventKind`, `Zenith_StructuralEvent`, `Zenith_StructuralObserver`, `Zenith_TickBoundaryStep`, `Zenith_TickBoundaryAccess`, `Zenith_TickBoundaryHandler` | this file |
| `Zenith_ECSScriptTraits.h` | the `Zenith_ScriptValueTraits<Zenith_EntityHandle>` specialisation of the binding ABI | this file |
| `Zenith_ECSTyped.h` (with `Zenith_ECSTyped.inl`) | `Zenith_ColumnView<T>`, `Zenith_Read<T>`, `Zenith_Write<T>`, `Zenith_QueryTermFor<T>`, `Zenith_SetProperty<T>`, `Zenith_AddComponent<T>` | this file |

Every public header is in the Z2 categories' `reviewPaths` ([Z2-N-CONTRACT-492]). The backend libraries' factory headers are listed in § "Backend factory headers". Types marked "backend support" are public only because backends are separate libraries that cannot include `ZenithECS/Internal/` ([Z2-N-CONTRACT-024]); no consumer outside a backend library names them, and the `Libgraph` swap-contract report for the ECS family lists them as used by backends alone.

## Zenith_ECSTypes header

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ECSTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"               // Zenith_Span, Zenith_ByteSpan
#include "ZenithReflect/Zenith_ReflectIDs.h"      // Zenith_TypeID, Zenith_PropertyID

inline constexpr uint32 uECS_MAX_WORLD_TAG = 4095;
inline constexpr uint32 uECS_MAX_ENTITIES = 1u << 24;              // slot bits of Zenith_EntityHandle
inline constexpr uint32 uECS_MAX_GENERATION = (1u << 28) - 1;      // a slot at this generation retires
inline constexpr uint32 uECS_MAX_QUERY_TERMS = 16;
inline constexpr uint32 uECS_MAX_QUERIES_PER_WORLD = 4096;
inline constexpr uint32 uECS_MAX_COMPONENTS_PER_ENTITY = 64;
inline constexpr uint32 uECS_MAX_ARCHETYPES = 65535;
inline constexpr uint32 uECS_MAX_COMPONENT_BYTES = 1024;           // the inline limit of every component
inline constexpr uint32 uECS_MAX_COMPONENT_ALIGNMENT = 16;
inline constexpr uint32 uECS_ALTERNATE_TEST_CHUNK_ROWS = 13;       // the determinism matrix's alternate capacity

/// @contract enum Zenith_ECSErrorCode
/// Ownership: Plain enumeration, the code of Zenith_ECSError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: ECS_ERROR_STALE_HANDLE: generation mismatch or a destroyed entity. ECS_ERROR_WRONG_WORLD: the handle's
///   world tag is another world's. ECS_ERROR_NO_COMPONENT: the entity lacks the component. ECS_ERROR_UNKNOWN_TYPE: the
///   type ID is not registered, or is not TYPE_KIND_COMPONENT. ECS_ERROR_UNKNOWN_PROPERTY: the property is not in the
///   type's current revision, or not declared writable where a write is attempted. ECS_ERROR_CAPACITY: entity,
///   container-heap, archetype, query or components-per-entity capacity exhausted. ECS_ERROR_OUT_OF_MEMORY: the world
///   heap refused an allocation. ECS_ERROR_WORLD_TAGS_EXHAUSTED: every one of the 4,095 tags is live.
///   ECS_ERROR_BACKEND: a backend-internal failure, whose text the backend logs at the point of failure.
///   ECS_ERROR_NULL_HANDLE: the null handle was resolved. ECS_ERROR_INVALID_DESC: a desc field is outside its range.
///   ECS_ERROR_VALUE_SIZE: a property value's byte count differs from the property's reflected size.
///   ECS_ERROR_PHASE_NOT_SCHEDULABLE: Zenith_SystemRegistry::AddSystem (or a reload scope's AddSystem or ReplaceSystem)
///   received a phase that no tick stage runs — PHASE_RENDER_EXTRACTION, PHASE_COMMANDS_ONLY or any value above
///   PHASE_SAVE_SNAPSHOT; phase-12 work registers only through AddEpilogueStage or the extractor path.
/// Version: since Z2 Stage 2. An enumerator never changes meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_ECSErrorCode : uint8
{
	ECS_ERROR_STALE_HANDLE = 1,
	ECS_ERROR_WRONG_WORLD,
	ECS_ERROR_NO_COMPONENT,
	ECS_ERROR_UNKNOWN_TYPE,
	ECS_ERROR_UNKNOWN_PROPERTY,
	ECS_ERROR_CAPACITY,
	ECS_ERROR_OUT_OF_MEMORY,
	ECS_ERROR_WORLD_TAGS_EXHAUSTED,
	ECS_ERROR_BACKEND,
	// elaboration: appended after the specification's enumerators, values 10 to 13
	ECS_ERROR_NULL_HANDLE,
	ECS_ERROR_INVALID_DESC,
	ECS_ERROR_VALUE_SIZE,
	ECS_ERROR_PHASE_NOT_SCHEDULABLE,   // value 13; assumption: pending core-doc batch 4 (X84)
};

/// @contract value Zenith_ECSError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480] (code first, 16 bytes: the uint8 code, 3 padding
///   bytes, the 4-byte property ID, the 8-byte type ID).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: None.
/// Errors: m_xType and m_xProperty name the type and property involved, zero when not applicable; names and entity
///   simulation IDs are logged at the point of failure ([Z2-N-CONTRACT-464]), never carried.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ECSError
{
	Zenith_ECSErrorCode m_eCode;
	Zenith_PropertyID m_xProperty;
	Zenith_TypeID m_xType;
};
static_assert(sizeof(Zenith_ECSError) == 16 && alignof(Zenith_ECSError) == 8);

/// @contract functions Zenith_ECSErrorText
/// Ownership: Returns string literals.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: An out-of-range value returns "ECS_ERROR_<invalid>".
/// Version: since Z2 Stage 2.
/// @endcontract
constexpr const char* Zenith_ToString(Zenith_ECSErrorCode eCode);   // "ECS_ERROR_STALE_HANDLE", …

/// @contract value Zenith_PropertyValue
/// Ownership: Plain value; borrows m_xValue for the call it is passed to.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_xValue is invalid once its storage is released by the caller.
/// Units: m_xValue holds the property's value in its value encoding (Zenith2/Docs/Design/ECS/Reflect.md →
///   "Value encodings"), never in a storage layout.
/// Errors: None; a size mismatch is reported by the consumer as ECS_ERROR_VALUE_SIZE.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_PropertyValue
{
	const Zenith_PropertyID m_xProperty = {};
	const Zenith_ByteSpan m_xValue = {};
};

/// @contract value Zenith_ComponentInit
/// Ownership: Plain value; borrows m_axValues for the call.
/// Thread affinity: IMMUTABLE. Members non-const: held in the Zenith_Array that Zenith_SceneComponentAmender edits in
///   place.
/// Invalidation: As Zenith_PropertyValue.
/// Units: None.
/// Errors: None; absent properties take their registered defaults; a property listed twice is ECS_ERROR_UNKNOWN_PROPERTY
///   at playback.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ComponentInit
{
	Zenith_TypeID m_xType;
	Zenith_Span<const Zenith_PropertyValue> m_axValues;
};

/// @contract value Zenith_SpawnTicket
/// Ownership: Plain value; a placeholder local to the command buffer that issued it.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: Meaningless outside its buffer; resolved to a handle (or to the null handle when the spawn is rejected)
///   at the playback of that buffer.
/// Units: Record index within the buffer.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SpawnTicket { uint32 m_uBufferLocalIndex = 0xFFFFFFFFu; };

/// @contract value Zenith_QuerySlot
/// Ownership: Plain value filled by Zenith_SystemBuilder::Query and bound to a world query at Zenith_SystemRegistry::Freeze.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of Zenith_SystemBuilder::Query.
/// Invalidation: Valid for the system registry that issued it and its recompiles.
/// Units: Index into the registry's query table.
/// Errors: An unbound slot passed to Zenith_SystemContext is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_QuerySlot { uint32 m_uIndex = 0xFFFFFFFFu; };

/// @contract value Zenith_EntityLocation
/// Ownership: Plain value returned by Zenith_World::Resolve.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_uChunkOrdinal and m_uRow are valid until the next structural playback; m_ulSimulationID never changes.
/// Units: Backend chunk ordinal and row.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EntityLocation
{
	const uint64 m_ulSimulationID = 0;
	const uint32 m_uChunkOrdinal = 0;
	const uint32 m_uRow = 0;
	const bool m_bDestroyPending = false;
};

/// @contract value Zenith_WorldStats
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None; a snapshot at the call.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_WorldStats
{
	uint32 m_uEntities, m_uChunks, m_uArchetypes;
	uint64 m_ulComponentBytes, m_ulContainerHeapBytes;
	// elaboration
	uint64 m_ulChangeTrackingBytes;     // row and column versions
	uint32 m_uPendingDestroy;
	uint32 m_uRetiredSlots;
};

/// @contract value Zenith_ContainerRef
/// Ownership: Plain value stored inline in a component row; the block it names is owned by the world's
///   Zenith_ContainerHeap and released by the backend when the row is destroyed or the field replaced.
/// Thread affinity: IMMUTABLE as a value; the block follows the component's access rules. Members non-const: the value
///   lives in component rows that systems, playback and property sets write in place.
/// Invalidation: Invalid after the field is replaced (the old block is freed at the end of the tick) or the row destroyed.
/// Units: m_uBlock is a heap block index (0 = empty container); m_uCount elements.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized: serialization writes the elements.
/// @endcontract
struct Zenith_ContainerRef
{
	uint32 m_uBlock = 0;
	uint32 m_uCount = 0;
};
static_assert(sizeof(Zenith_ContainerRef) == 8 && alignof(Zenith_ContainerRef) == 4);
```

## Zenith_EntityHandle header

The bit layout is `Zenith2/Docs/Contracts.md` → "Handle kinds" (slot bits 0–23, generation 24–51, world tag 52–63); only `Zenith_EntityTable`, `Zenith_CommandBuffer` and the script value traits (which carry the 64 bits opaquely and never interpret them) read or build the bits.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_EntityHandle.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithReflect/Zenith_ReflectValueTraits.h"   // the primary template this header specialises

template<typename T> struct Zenith_ScriptValueTraits;   // the primary template of ZenithReflect/Zenith_ScriptBinding.h

/// @contract value Zenith_EntityHandle
/// Ownership: Plain value. Copying never extends the entity's lifetime; the issuing world owns the entity.
/// Thread affinity: IMMUTABLE. Resolving it follows the issuing world's access rules for the current phase. Members
///   non-const: handles are copy-assignable, and are held in components, containers and out-parameters that are
///   overwritten.
/// Invalidation: Stale once the entity's destroy is played back at the end of its tick; resolution then fails in every
///   configuration (generation compare). Never valid in another world (world-tag compare). A ticket handle
///   (Zenith_CommandBuffer::TicketHandle) is meaningful only inside the buffer that issued it.
/// Units: None.
/// Errors: Resolution returns Zenith_Result<…, Zenith_ECSError>: ECS_ERROR_NULL_HANDLE, ECS_ERROR_STALE_HANDLE,
///   ECS_ERROR_WRONG_WORLD. No resolution path asserts. Residual aliasing after a tag reissue is mitigated by the
///   registry's initial generations (Zenith2/Docs/Design/ECS/Scheduling.md → "World registry algorithm").
/// Version: since Z2 Stage 2. Never serialized; the bit layout may change in any build.
/// @endcontract
class Zenith_EntityHandle
{
public:
	constexpr Zenith_EntityHandle() = default;
	constexpr bool IsNull() const { return m_ulBits == 0; }
	// elaboration: the world tag is public, so scripts and tools compare worlds without resolving
	constexpr uint16 GetWorldTag() const { return static_cast<uint16>(m_ulBits >> 52); }
	friend constexpr bool operator==(Zenith_EntityHandle, Zenith_EntityHandle) = default;

private:
	friend class Zenith_EntityTable;
	friend class Zenith_CommandBuffer;
	friend struct Zenith_ScriptValueTraits<Zenith_EntityHandle>;   // reads and builds the bits (§ "Zenith_ECSScriptTraits header")
	explicit constexpr Zenith_EntityHandle(uint64 ulBits) : m_ulBits(ulBits) {}
	uint64 m_ulBits = 0;
};
static_assert(sizeof(Zenith_EntityHandle) == 8);

// elaboration (consumer seam): ZenithECS specialises the reflection traits next to its type (Reflect.md, "Zenith_ReflectValueTraits header")
/// @contract value Zenith_ReflectValueTraits<Zenith_EntityHandle>
/// Ownership: A compile-time traits specialisation; owns nothing.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None. A C++ member of this type is a PROPERTY_TYPE_ENTITY property of 8 bytes holding the handle's bits in
///   memory values; field sets carry the referenced entity's persistent ID (Reflect.md, "Value encodings").
/// Errors: None; a Default of an entity property is checked by Freeze rule 12 (8 bytes, the null handle's zero bits only).
/// Version: since Z2 Stage 2.
/// @endcontract
template<>
struct Zenith_ReflectValueTraits<Zenith_EntityHandle>
{
	static constexpr bool bSUPPORTED = true;
	static constexpr Zenith_PropertyValueType eTYPE = PROPERTY_TYPE_ENTITY;
	static constexpr bool bCARRIES_LIFETIME = false;     // copying a handle never extends the entity's lifetime
	static Zenith_TypeID ElementType() { return {}; }
};
```

The specialisation lives in this header, not in `Zenith_ECSTypes.h`, so that every component header declaring a `Zenith_EntityHandle` member sees it before `Zenith_TypeBuilder::Property` instantiates the traits: a component struct includes this header to declare the member at all. A component holding only handles and plain values stays trivially copyable, so its hooks are null and its default object is the byte copy of `T{}`, whose entity cells are the null handle's zero bits.

A **ticket handle** has world tag 0, generation 0 and slot `ticket index + 1`; it is non-null, never live, and is accepted only by the commands of the buffer that issued the ticket (§ "Zenith_CommandBuffer header").

## Zenith_ComponentHandle header

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ComponentHandle.h
#pragma once
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract value Zenith_ComponentHandle
/// Ownership: Plain value (16 bytes). The issuing world owns the component.
/// Thread affinity: IMMUTABLE. Members non-const: handles are copy-assignable, and are held in components, containers
///   and out-parameters that are overwritten.
/// Invalidation: Stale when its entity handle is stale or the entity no longer carries the type; a removal followed by
///   a re-add on the same live entity is not detected ([Z2-N-CONTRACT-075]); a consumer that needs that reads the
///   column's change version.
/// Units: None.
/// Errors: Resolution through Zenith_World::Resolve plus HasComponent returns ECS_ERROR_NULL_HANDLE,
///   ECS_ERROR_STALE_HANDLE, ECS_ERROR_WRONG_WORLD or ECS_ERROR_NO_COMPONENT; never an assert.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
class Zenith_ComponentHandle
{
public:
	constexpr Zenith_ComponentHandle() = default;
	constexpr Zenith_ComponentHandle(Zenith_EntityHandle xEntity, Zenith_TypeID xTypeID) : m_xEntity(xEntity), m_xTypeID(xTypeID) {}
	Zenith_EntityHandle GetEntity() const { return m_xEntity; }
	Zenith_TypeID GetTypeID() const { return m_xTypeID; }
	friend constexpr bool operator==(const Zenith_ComponentHandle&, const Zenith_ComponentHandle&) = default;
private:
	Zenith_EntityHandle m_xEntity;
	Zenith_TypeID m_xTypeID;
};
static_assert(sizeof(Zenith_ComponentHandle) == 16);
```

## Zenith_EntityTable header

<!-- elaboration -->
`Zenith_EntityTable` is the one implementation of entity identity every backend uses: slots, generations, world tags, stable simulation IDs, persistent IDs, the parent relation and destroy marks. Its algorithms are `Zenith2/Docs/Design/ECS/Backends.md` → "Entity table".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_EntityTable.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"

class Zenith_Heap;
class Zenith_FaultInjector;

/// @contract value Zenith_EntityTableDesc
/// Ownership: Plain value, carried in Zenith_WorldDesc.
/// Thread affinity: IMMUTABLE. Members non-const: held by value in Zenith_WorldDesc, which
///   Zenith_ApplyECSConfigToWorldDesc patches in place.
/// Invalidation: None.
/// Units: A generation value.
/// Errors: m_uInitialGeneration 0 or above uECS_MAX_GENERATION is ECS_ERROR_INVALID_DESC at world creation.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EntityTableDesc
{
	uint32 m_uInitialGeneration = 0;   // computed by Zenith_WorldRegistry, never 0
};

/// @contract enum Zenith_EntitySlotFlags
/// Ownership: Plain bit flags of one slot.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bits.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EntitySlotFlags : uint32
{
	ENTITY_SLOT_FLAG_NONE = 0,
	ENTITY_SLOT_FLAG_LIVE = 1u << 0,
	ENTITY_SLOT_FLAG_DESTROY_PENDING = 1u << 1,
	ENTITY_SLOT_FLAG_RETIRED = 1u << 2,       // the generation reached uECS_MAX_GENERATION; never reissued
};

/// @contract container Zenith_EntityTable
/// Ownership: Created by Create as a Zenith_Owned<Zenith_EntityTable> by a backend's world factory, which owns it for the
///   world's lifetime; storage (56 bytes per slot, the free-slot bit set, the persistent-ID map) from the world heap,
///   reserved for uMaxEntities at Create.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Mutators run on the sync-point node or out-of-tick playback of the owning
///   world; readers (Resolve, GetSimulationID, GetParent, IsDestroyPending) run from any node of a phase, where no
///   mutator runs.
/// Invalidation: None of its own; handles it issued follow Zenith_EntityHandle.
/// Units: Slots; generations; simulation IDs.
/// Errors: Resolve returns ECS_ERROR_NULL_HANDLE, ECS_ERROR_WRONG_WORLD, ECS_ERROR_STALE_HANDLE; Allocate returns
///   ECS_ERROR_CAPACITY when no slot is free. A mutation of a free slot is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Backend support.
/// Alignment: Slot records are 8-byte aligned in one reservation from the world heap; no element storage is exposed:
///   callers receive slot indices and handles by value.
/// Overflow: Allocate returns ECS_ERROR_CAPACITY once all uMaxEntities slots are live or retired; a slot whose generation
///   reaches uECS_MAX_GENERATION is retired and never reissued; Create rejects uMaxEntities of 0 or above
///   uECS_MAX_ENTITIES with ECS_ERROR_INVALID_DESC before any size arithmetic.
/// Allocation failure: Only Create allocates (the slot records, the free-slot bit set and the persistent-ID index, each
///   behind its fault point); a refusal returns ECS_ERROR_OUT_OF_MEMORY and unwinds the earlier reservations. No other
///   member allocates.
/// Complexity: Resolve, GetSimulationID, GetParent, GetHandle, IsDestroyPending, SetBackendLocation, SetParent and
///   MarkDestroyPending O(1); FindByPersistentID O(1) expected; Allocate O(1) amortised (O(uMaxEntities / 64) worst case,
///   the free-slot scan); Release and GetChildren O(children of the slot).
/// @endcontract
class Zenith_EntityTable
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EntityTable>, Zenith_ECSError> Create(uint16 uWorldTag,
		const Zenith_EntityTableDesc& xDesc, uint32 uMaxEntities, Zenith_Heap& xHeap, Zenith_FaultInjector* pxFaults);

	// Resolution (read-only; any node of a phase).
	Zenith_Result<uint32, Zenith_ECSError> Resolve(Zenith_EntityHandle xEntity) const;   // the slot
	uint64 GetSimulationID(uint32 uSlot) const;
	Zenith_PersistentID GetPersistentID(uint32 uSlot) const;
	Zenith_EntityHandle GetHandle(uint32 uSlot) const;
	Zenith_EntityHandle GetParent(uint32 uSlot) const;                                  // null for a root
	bool IsDestroyPending(uint32 uSlot) const;
	uint64 GetBackendLocation(uint32 uSlot) const;                                     // the backend's word
	uint32 GetLiveCount() const;
	uint32 GetHighestGenerationIssued() const;
	Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> FindByPersistentID(Zenith_PersistentID xID) const;

	// Mutation (sync-point node or out-of-tick playback).
	Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> Allocate(uint64 ulSimulationID,
		Zenith_PersistentID xPersistentID);                                                             // lowest free slot
	void SetBackendLocation(uint32 uSlot, uint64 ulLocation);
	void SetParent(uint32 uSlot, uint32 uParentSlot);                                                // 0xFFFFFFFF = root
	void MarkDestroyPending(uint32 uSlot);
	void Release(uint32 uSlot);             // end-of-tick removal: generation + 1, or retirement at the maximum
	// Children of uSlot in ascending stable simulation ID; fills axOut, returns the count written.
	uint32 GetChildren(uint32 uSlot, Zenith_Span<uint32> axOut) const;
	uint32 GetChildCount(uint32 uSlot) const;

	uint32 GetSlotCapacity() const;
	bool IsLive(uint32 uSlot) const;

	// Test seam: the generation a subsequent Allocate of uSlot will issue.
	uint32 PeekNextGeneration(uint32 uSlot) const;
};

inline constexpr uint32 uFAULT_POINT_ENTITY_TABLE_SLOTS = 0x45430001u;           // slot array reservation
inline constexpr uint32 uFAULT_POINT_ENTITY_TABLE_FREE_BITS = 0x45430002u;
inline constexpr uint32 uFAULT_POINT_ENTITY_TABLE_PERSISTENT_MAP = 0x45430003u;
```

## Zenith_ContainerHeap header

<!-- elaboration -->
The out-of-line storage of `container<T>` fields, one per world, sized by `Zenith_WorldDesc::m_uContainerHeapBytes`; its algorithm is `Zenith2/Docs/Design/ECS/Backends.md` → "Container heap".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ContainerHeap.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithECS/Zenith_ECSTypes.h"

class Zenith_Heap;
class Zenith_FaultInjector;

/// @contract type Zenith_ContainerHeap
/// Ownership: Created by Create as a Zenith_Owned<Zenith_ContainerHeap> by a backend's world factory; one contiguous
///   reservation of uBytes from the world heap, carved into 16-byte-aligned blocks of power-of-two size classes from
///   16 bytes to 64 MiB (a container is at most 65,535 elements of at most 1,024 bytes).
/// Thread affinity: EXTERNALLY_SYNCHRONISED. Reserve and CancelReservation run on a world's merge chain, its sync-point
///   node, its tick-boundary node (Zenith_TickBoundaryAccess::SetContainer) or out-of-tick playback; AllocateReserved,
///   FreeDeferred, FreeNow and FlushDeferred on the sync-point node, the tick-boundary node or out-of-tick playback;
///   GetBlock reads from any node that declared the field.
/// Invalidation: A block's address is stable until the block is freed; FreeDeferred releases at FlushDeferred (end of tick).
/// Units: Bytes; element counts.
/// Errors: Reserve reserves one block of the size class uBytes names (GetBlockBytes of one container) and returns false,
///   changing nothing, when the free blocks could not serve every outstanding reservation plus this one (merge-time
///   capacity or an out-of-tick refusal, never an assert); AllocateReserved within a reservation cannot fail; Create
///   returns ECS_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2. Backend support.
/// @endcontract
class Zenith_ContainerHeap
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ContainerHeap>, Zenith_ECSError> Create(uint32 uBytes,
		Zenith_Heap& xHeap, Zenith_FaultInjector* pxFaults);
	uint64 GetFreeBytes() const;
	// elaboration: the bytes of the largest free block (0 when none is free); while no reservation is outstanding, any set of
	// reservations whose block bytes sum to at most this value is accepted by Reserve whatever the fragmentation
	uint32 GetLargestFreeBlockBytes() const;
	uint64 GetUsedBytes() const;
	static uint32 GetBlockBytes(uint32 uElementBytes, uint32 uCount);   // the size class a request uses
	bool Reserve(uint32 uBytes);
	void CancelReservation(uint32 uBytes);
	uint32 AllocateReserved(uint32 uElementBytes, uint32 uCount);       // block index ≥ 1
	uint8* GetBlock(uint32 uBlock);
	const uint8* GetBlock(uint32 uBlock) const;
	void FreeDeferred(uint32 uBlock);
	void FreeNow(uint32 uBlock);
	void FlushDeferred();
};

inline constexpr uint32 uFAULT_POINT_CONTAINER_HEAP_RESERVE = 0x45430010u;
```

## Zenith_ChunkView header

`Zenith_InvocationScope`, `Zenith_RawColumn` (with `MarkRowChanged` and `MarkAllRowsChanged`) and `Zenith_ChunkView` (with `CommitRowChanges`) are exactly `Zenith2/Docs/Architecture/ECSModel.md` → "Views"; this design adds the binding records a backend fills, two accessors and a backend constructor. The algorithms are `Zenith2/Docs/Design/ECS/Queries.md` → "Views and validation" and "Row change bits and CommitRowChanges".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ChunkView.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_TypeInfo.h"        // Zenith_PropertyValueType
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract value Zenith_InvocationScope
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: the system context reassigns it for each invocation.
/// Invalidation: None; a scope ID is never reused within its namespace for a world's lifetime (table "Scope namespaces").
/// Units: An invocation ID: a frame-graph node invocation (Zenith_JobContext::GetInvocationID), a script invocation
///   (Zenith_ScriptInvocationID), or a playback scope of the world simulation.
/// Errors: 0 is invalid in every namespace.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_InvocationScope
{
	uint64 m_ulInvocationID = 0;
};

/// @contract functions Zenith_ChangeVersionOrder
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: uint32 change versions compared in serial-number arithmetic.
/// Errors: None; meaningful while the two versions are fewer than 2^31 merge points apart, which the version wrap sweep
///   guarantees for every stored version (Queries.md, "Version wrap sweep"); versions are never 0, and a last-seen 0
///   ("never seen") is handled by the caller before this compare. The skipped 0 on the ring 1 … 2^32 − 1 shifts a
///   wrapped difference by one, which never changes its sign below 2^31 − 1.
/// Version: since Z2 Stage 2.
/// @endcontract
constexpr bool Zenith_IsVersionNewer(uint32 uVersion, uint32 uThan)
{
	return static_cast<sint32>(uVersion - uThan) > 0;
}

/// @contract value Zenith_RawColumn
/// Ownership: A borrowed view of one property column of one chunk; owns nothing.
/// Thread affinity: OWNER_THREAD — the invocation that obtained it.
/// Invalidation: With its chunk view (invocation end, or a structural epoch change).
/// Units: Rows; m_uStride in bytes.
/// Errors: MarkRowChanged or MarkAllRowsChanged on a non-writable column, or a row >= m_uRowCount, is a programmer error
///   (ZENITH_ASSERT); script misuse is caught before this call by the view layer and raised as a Luau error.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RawColumn
{
	uint8* m_pData = nullptr;              // row 0; null when the property is not declared by this invocation
	uint32 m_uStride = 0;                  // bytes between consecutive rows
	uint32 m_uRowCount = 0;
	const uint32* m_puRowIndex = nullptr;  // null = identity; otherwise row r lives at m_puRowIndex[r]
	uint64* m_pulRowChangeBits = nullptr;  // the invocation's pending per-row change bits for this column
	bool m_bWritable = false;               // the invocation declared a write of this property

	void MarkRowChanged(uint32 uRow);
	void MarkAllRowsChanged();
};

/// @contract value Zenith_ChunkColumnBinding
/// Ownership: Plain record filled by a backend in its query cache; owned by that cache.
/// Thread affinity: IMMUTABLE during a phase: written only by the backend at a sync point or out-of-tick playback.
///   Members non-const: the backend rewrites bindings in place at each sync point and out-of-tick playback.
/// Invalidation: Invalid after the next structural playback of the world.
/// Units: Bytes and rows.
/// Errors: None.
/// Version: since Z2 Stage 2. Backend support.
/// @endcontract
struct Zenith_ChunkColumnBinding
{
	Zenith_TypeID m_xType;
	Zenith_PropertyID m_xProperty;
	Zenith_PropertyValueType m_eValueType;
	bool m_bWriteDeclared = false;          // the query term names the property with m_bWrite
	bool m_bOwnRowOnly = false;             // set from the schedule (Queries.md, "Own-row and any-row reads")
	uint16 m_uArbitrationIndex = 0xFFFFu;  // the world's arbitration record when the property is arbitrated
	uint32 m_uFieldBytes = 0;
	uint8* m_pData = nullptr;
	uint32 m_uStride = 0;
	const uint32* m_puRowIndex = nullptr;
	uint32* m_puColumnVersion = nullptr;   // the (chunk, column) change version
	uint32* m_puRowVersions = nullptr;     // one uint32 per row: the merge version of the row's last committed write
};

/// @contract value Zenith_ChunkBinding
/// Ownership: Plain record filled by a backend in its query cache, one per (query, chunk ordinal); owned by that cache.
/// Thread affinity: IMMUTABLE during a phase (as Zenith_ChunkColumnBinding). Members non-const: the backend rewrites
///   bindings in place at each sync point and out-of-tick playback.
/// Invalidation: Invalid after the next structural playback of the world.
/// Units: Rows.
/// Errors: None.
/// Version: since Z2 Stage 2. Backend support.
/// @endcontract
struct Zenith_ChunkBinding
{
	uint32 m_uRowCount = 0;
	uint32 m_uChunkOrdinal = 0;
	const Zenith_EntityHandle* m_pxEntities = nullptr;
	const uint64* m_pulSimulationIDs = nullptr;
	const uint32* m_puStructuralEpoch = nullptr;       // the live epoch of the backend chunk
	Zenith_Span<const Zenith_ChunkColumnBinding> m_axColumns;   // the query's columns, in (type ID, property ID) order
};

/// @contract type Zenith_ChunkView
/// Ownership: A borrowed view of one chunk for one invocation; owns nothing; its pending change bits and staging weights
///   live in the invocation's arena (Zenith_SystemContext::GetChunk allocates them) or, for a playback write, in the
///   arena the playback passed to Zenith_WorldPlaybackAccess::BeginPlaybackWrites (the tick arena, or out of tick the
///   simulation's out-of-tick arena).
/// Thread affinity: OWNER_THREAD, valid for the invocation named by its scope; never stored beyond it. CommitRowChanges
///   runs in the merge continuation or merge node of that invocation after every sub-job has joined, or on the playback
///   node that took the playback write.
/// Invalidation: Invalid when its invocation ends (at its merge) or its chunk's structural epoch changes; IsValid reports both.
/// Units: Rows; m_uMergeVersion is a change version (table "Merge version stamping").
/// Errors: CommitRowChanges returns ECS_ERROR_STALE_HANDLE when the view is not valid for xScope and
///   ECS_ERROR_UNKNOWN_PROPERTY when the column was not declared writable or the view is commit-disabled (a world-level
///   view, merge version 0); no partial effect on failure. Use of an invalid view in C++ is ZENITH_ASSERT where asserts
///   are enabled ([Z2-N-ARCH-697]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ChunkView
{
public:
	constexpr Zenith_ChunkView() = default;                          // the empty view: 0 rows, never valid
	// Backends: uMergeVersion 0 builds a commit-disabled, read-only view (Zenith_World::GetChunk, GetChunkOfEntity);
	// a non-zero version builds a commit-enabled playback-write view (Zenith_WorldPlaybackAccess::GetPlaybackWrite).
	Zenith_ChunkView(const Zenith_ChunkBinding& xBinding, const Zenith_InvocationScope& xScope, uint32 uMergeVersion);

	uint32 GetRowCount() const;
	Zenith_RawColumn GetColumn(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	const Zenith_EntityHandle* GetEntities() const;
	const uint64* GetSimulationIDs() const;
	uint32 GetStructuralEpoch() const;
	uint32 GetChangeVersion(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	bool IsValid(const Zenith_InvocationScope& xCurrent) const;

	// elaboration: per-row change versions (replication, extraction, tools); null for an undeclared property
	const uint32* GetRowChangeVersions(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;
	// elaboration: the weight column of an arbitrated property this writer stages (read by a WeightedBlend merge, ignored by
	// Precedence and Custom merges, because a binding does not record the rule); null for every other column
	float* GetArbitrationWeights(Zenith_TypeID xType, Zenith_PropertyID xProperty) const;

	Zenith_Result<void, Zenith_ECSError> CommitRowChanges(const Zenith_InvocationScope& xScope, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, const uint64* pulAcceptedRowBits);

private:
	friend class Zenith_SystemContext;
	friend class Zenith_MergeContext;
	friend class Zenith_WorldSimulation;
	const Zenith_ChunkBinding* m_pxBinding = nullptr;
	uint64* m_pulPendingBits = nullptr;     // one bit per row per declared-writable column, invocation arena
	float* m_pfArbitrationWeights = nullptr; // per writable WeightedBlend column, invocation arena
	uint64 m_ulScope = 0;
	uint32 m_uEpochAtCreation = 0;
	uint32 m_uMergeVersion = 0;             // 0 = commit-disabled (world-level view)
	bool m_bCommitEnabled = false;           // set only by the stamping paths of table "Merge version stamping"
};
```

<!-- elaboration -->
Merge version stamping — the one place a view's merge version is set ([Z2-N-ARCH-694]; the values are `Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"):

| View obtained through | Constructed by | Merge version | Commit |
|---|---|---|---|
| `Zenith_World::GetChunk`, `Zenith_World::GetChunkOfEntity` (tools, tests, the inspector, SCRIPT's random-access read path before stamping) | the backend, with `uMergeVersion` 0 | 0 | disabled: every column is returned with `m_bWritable = false` and no pending bits, `CommitRowChanges` returns `ECS_ERROR_UNKNOWN_PROPERTY` |
| `Zenith_SystemContext::GetChunk`, `GetChunkInScope`, `GetChunkOfEntity` | the backend (merge version 0), then the context re-stamps it through its friendship before returning it | the invocation's merge version, from the invocation record the trampoline filled (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Disabled systems" step 2) | enabled; pending bits allocated in the calling context's arena for every declared-writable column |
| `Zenith_MergeContext::GetWrittenViews` | the invocation's own views, unchanged | the invocation's merge version | enabled |
| `Zenith_WorldPlaybackAccess::GetPlaybackWrite` | the backend, with the playback's version | the sync point's merge version (sync-point playback), the tick boundary's (tick-boundary property writes) or `Zenith_WorldSimulation::GetChangeVersion()` (out-of-tick playback) | enabled; pending bits are the playback's own, in the arena passed to `BeginPlaybackWrites` |

A view never carries a version it did not receive from one of these four paths, so a backend worker never needs the world simulation's counters and a tool's view has a defined (disabled) commit behaviour.

Scope namespaces:

| Namespace | Scope value | Issued by |
|---|---|---|
| frame-graph node invocation | `Zenith_JobContext::GetInvocationID()`: bits 62 and 63 clear, never 0 (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md` → "Invocation IDs") | the executor, one per node invocation; a node's sub-jobs share it |
| script invocation | `Zenith_ScriptInvocationID`: VM index in bits 63..48, per-VM counter in bits 47..0 | SCRIPT (`Zenith2/Docs/Scripting.md` → "Component writes shadows and side-effect buffers") |
| world playback | `0xFFFF'0000'0000'0000 | ((tick index × 16 + stage) & 0x0000'FFFF'FFFF'FFFF)`: stage 0 is the tick boundary's property writes, stages 1 … 10 the sync points, 11 the end-of-tick playback; out-of-tick playback uses stage 15 and a separate monotonically increasing counter in place of the tick index | `Zenith_WorldSimulation` |

A view is compared only against scopes of the namespace it was created in: a system's views against its node scope, a script's against its script invocation ID, playback's against its playback scope. VM index 0xFFFF is reserved for playback and never issued as a VM index (recorded for SCRIPT in the cross-area notes).

## Zenith_Query header

```cpp
// Zenith2/Engine/ZenithECS/Zenith_Query.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract enum Zenith_QueryTermOp
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_QueryTermOp : uint8
{
	QUERY_TERM_WITH,           // the component is present
	QUERY_TERM_WITHOUT,        // the component is absent
	QUERY_TERM_OPTIONAL,       // columns present when the component is
	QUERY_TERM_CHANGED,        // present and its declared properties changed after the query's last-seen version
};

/// @contract value Zenith_QueryTerm
/// Ownership: Plain value; borrows m_axProperties for the CompileQuery call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after compilation (the compiled query copies it).
/// Units: None.
/// Errors: Validation errors are CompileQuery's (Queries.md, "Query compilation").
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_QueryTerm
{
	const Zenith_TypeID m_xType = {};
	const Zenith_QueryTermOp m_eOp = QUERY_TERM_WITH;
	const Zenith_Span<const Zenith_PropertyID> m_axProperties = {}; // empty = every property
	const bool m_bWrite = false;
};

/// @contract value Zenith_QueryDesc
/// Ownership: Plain value; m_szName and m_axTerms are copied at compilation.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after compilation.
/// Units: None.
/// Errors: 0 or more than uECS_MAX_QUERY_TERMS terms is ECS_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_QueryDesc
{
	const char* const m_szName = nullptr;
	const Zenith_Span<const Zenith_QueryTerm> m_axTerms = {}; // 1 to 16 terms
};

/// @contract value Zenith_QueryID
/// Ownership: Plain value naming a compiled query of one world.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: Valid for the world's lifetime.
/// Units: Index into the world's query table.
/// Errors: An ID from another world, or the default value, is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_QueryID { uint32 m_uIndex = 0xFFFFFFFFu; };
```

## Zenith_World header

The backend interface, `Zenith_WorldDesc`, `Zenith_SyncPointToken`, `Zenith_WorldTestAccess`, `Zenith_WorldPlaybackAccess` and `Zenith_PfnCreateWorld` are `Zenith2/Docs/Architecture/ECSModel.md` → "Worlds and backends" and `Zenith2/Docs/Architecture/CompositionRoot.md` → "Runtime and entry types", with the additions marked.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_World.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_EntityTable.h"          // Zenith_EntityTableDesc
#include "ZenithECS/Zenith_ChunkView.h"
#include "ZenithECS/Zenith_Query.h"

class Zenith_TypeRegistry;
class Zenith_Heap;
class Zenith_Arena;
class Zenith_FaultInjector;
class Zenith_ContainerHeap;
class Zenith_WorldSimulation;

/// @contract value Zenith_WorldDesc
/// Ownership: Plain value passed to a backend factory; m_pxTypes, m_pxHeap and m_pxFaults are borrowed and outlive the world.
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_ApplyECSConfigToWorldDesc patches it in place.
/// Invalidation: None.
/// Units: Rows, entities, bytes.
/// Errors: m_uWorldTag outside 1 … 4095, m_uMaxEntities 0 or above uECS_MAX_ENTITIES, m_uChunkCapacityRows above 1,024,
///   m_uContainerHeapBytes below 64 KiB, a null registry or heap, an unfrozen registry, or a registry whose reference
///   defaults are unresolved (Zenith_TypeRegistry::AreReferenceDefaultsResolved false) is ECS_ERROR_INVALID_DESC.
///   m_bEditWorld is never validated: either value is accepted.
/// Version: since Z2 Stage 2; m_bEditWorld since Z2 Stage 3.
/// @endcontract
struct Zenith_WorldDesc
{
	uint16 m_uWorldTag = 0;
	Zenith_EntityTableDesc m_xEntityTable;
	const Zenith_TypeRegistry* m_pxTypes = nullptr;
	Zenith_Heap* m_pxHeap = nullptr;
	uint32 m_uChunkCapacityRows = 0;               // 0 = backend default; tests set other values
	uint32 m_uMaxEntities = 1u << 20;
	uint32 m_uContainerHeapBytes = 64u << 20;
	uint64 m_ulWorldSeed = 0;
	// elaboration: [Z2-N-CONTRACT-225], the factory's fault injector, null in production
	Zenith_FaultInjector* m_pxFaults = nullptr;
	// elaboration (consumer seam): the edit-world flag (§ "The edit-world flag"); set only by the world's creator from the world's purpose,
	// true exactly for an edit world; read back through Zenith_World::IsEditWorld
	bool m_bEditWorld = false;
};

/// @contract value Zenith_SyncPointToken
/// Ownership: A capability value; carries nothing but proof of the caller.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; construction outside Zenith_WorldSimulation and Zenith_WorldTestAccess does not compile.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SyncPointToken
{
private:
	Zenith_SyncPointToken() = default;
	friend class Zenith_WorldSimulation;
	friend class Zenith_WorldTestAccess;
};

// elaboration
/// @contract value Zenith_PlaybackWrite
/// Ownership: Plain value returned by Zenith_WorldPlaybackAccess::GetPlaybackWrite; m_xView and m_pulRowBits borrow the
///   backend's playback write set and the arena passed to BeginPlaybackWrites.
/// Thread affinity: OWNER_THREAD — the playback node that took it.
/// Invalidation: Invalid after EndPlaybackWrites or any structural call.
/// Units: Rows; m_pulRowBits holds one bit per row of m_xView, rounded up to 64-bit words.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_PlaybackWrite
{
	Zenith_ChunkView m_xView;              // commit-enabled, the playback's scope and merge version, one written column
	Zenith_TypeID m_xType;
	Zenith_PropertyID m_xProperty;
	const uint64* m_pulRowBits = nullptr; // the rows SetProperty or SetContainer wrote in this (chunk, column)
};

/// @contract interface Zenith_WorldPlaybackAccess
/// Ownership: Implemented by each backend; owned by its world; reached only through Zenith_World::GetPlaybackAccess.
/// Thread affinity: OWNER_THREAD — the world's sync-point node, out-of-tick playback, or world loading before the first tick.
///   The tick-boundary node reaches the property members alone, through Zenith_TickBoundaryAccess
///   (§ "Zenith_ECSObservers header"), never this interface.
/// Invalidation: Every Zenith_EntityLocation, chunk binding and view of the world is invalid after any structural call.
/// Units: Simulation IDs; bytes of value encodings. GetWorldTag returns the tag of the world being played back, equal to
///   Zenith_World::GetWorldTag; a module command's playback keys its per-world state by it ([Z2-N-ARCH-716]).
/// Errors: Every failure is a recoverable Zenith_ECSError logged with the command that caused it ([Z2-N-ARCH-716]):
///   ECS_ERROR_NULL_HANDLE, ECS_ERROR_STALE_HANDLE, ECS_ERROR_WRONG_WORLD, ECS_ERROR_NO_COMPONENT,
///   ECS_ERROR_UNKNOWN_TYPE, ECS_ERROR_UNKNOWN_PROPERTY, ECS_ERROR_VALUE_SIZE, ECS_ERROR_CAPACITY, ECS_ERROR_OUT_OF_MEMORY.
///   RewriteContainer returns ECS_ERROR_UNKNOWN_PROPERTY for a property that is not a CONTAINER or whose element type
///   carries lifetime, and ECS_ERROR_VALUE_SIZE when xElements is not exactly the current count × the element size; it
///   never returns ECS_ERROR_CAPACITY. No call asserts on data a script or content produced.
/// Reentrancy: NONE. Every mutation is applied immediately to storage; no view is live while it runs.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_WorldPlaybackAccess
{
public:
	virtual ~Zenith_WorldPlaybackAccess() = default;
	virtual uint16 GetWorldTag() const = 0;   // the world being played back (Zenith_World::GetWorldTag)
	virtual Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> CreateEntity(uint64 ulSimulationID,
		Zenith_Span<const Zenith_ComponentInit> axComponents) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> AddComponent(Zenith_EntityHandle xEntity,
		const Zenith_ComponentInit& xComponent) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> RemoveComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> SetProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xValue) = 0;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty) const = 0;
	virtual Zenith_Result<void, Zenith_ECSError> MarkDestroyPending(Zenith_EntityHandle xEntity) = 0;
	virtual bool IsDestroyPending(Zenith_EntityHandle xEntity) const = 0;

	// elaboration: the rest of the playback surface
	virtual Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> CreateEntityWithIdentity(uint64 ulSimulationID,
		Zenith_PersistentID xPersistentID, Zenith_EntityHandle xParent, Zenith_Span<const Zenith_ComponentInit> axComponents) = 0;
	virtual Zenith_Result<void, Zenith_ECSError> SetParent(Zenith_EntityHandle xEntity, Zenith_EntityHandle xParent) = 0;
	// A new block of GetBlockBytes(element bytes, uCount) taken with AllocateReserved, so the caller holds one reservation of
	// that size class (the merge at a sync point; the caller itself everywhere else, § "Container writes without a merge
	// reservation"); the replaced block is queued with FreeDeferred, whatever the two counts.
	virtual Zenith_Result<void, Zenith_ECSError> SetContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xElements, uint32 uCount) = 0;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, uint32& uCountOut) const = 0;
	virtual bool HasComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType) const = 0;
	virtual Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> FindByPersistentID(Zenith_PersistentID xID) const = 0;
	virtual Zenith_EntityTable& GetEntityTable() = 0;
	virtual Zenith_ContainerHeap& GetContainerHeap() = 0;
	virtual void DestroyNow(Zenith_EntityHandle xEntity) = 0;     // end-of-tick removal and world teardown only

	// elaboration: the playback's own pending bits ([Z2-N-ARCH-728]; Zenith2/Docs/Design/ECS/Backends.md → "Playback writes").
	// Between BeginPlaybackWrites and EndPlaybackWrites every successful SetProperty and SetContainer sets its row's bit
	// in the (backend chunk, column) block it touched, allocating the block from xArena on first touch. Outside that pair a
	// successful SetProperty or SetContainer stamps the written row's version and its (chunk, column)'s column version at
	// once with the playback version (step 6 of a sync point or of out-of-tick playback, Scheduling.md, "Property writes
	// during structural playback").
	virtual void BeginPlaybackWrites(Zenith_Arena& xArena) = 0;
	virtual uint32 GetPlaybackWriteCount() const = 0;    // written (chunk, column) pairs
	// The uIndex-th pair in ascending (type ID, property ID, backend chunk key), as a view with xScope and uMergeVersion.
	virtual Zenith_PlaybackWrite GetPlaybackWrite(uint32 uIndex, const Zenith_InvocationScope& xScope,
		uint32 uMergeVersion) = 0;
	virtual void EndPlaybackWrites() = 0;                                // forgets the set; the arena owns the blocks
	// The version of the playback in progress, set by the world simulation at the start of every playback and reset to 0 at
	// its end: rows CreateEntity, CreateEntityWithIdentity and AddComponent create, columns a move adds to a row, and property
	// writes outside the write set are stamped with it; such a call while it is 0 is ZENITH_ASSERT and stamps nothing.
	virtual void SetPlaybackVersion(uint32 uVersion) = 0;

	// elaboration (consumer seam): the in-place rewrite of an existing container (§ "In-place container rewrite"): xElements must hold exactly
	// the cell's current element count; the elements are copied into the cell's existing block, so nothing is reserved,
	// allocated or freed and the call cannot fail for capacity. Records and stamps like SetContainer.
	virtual Zenith_Result<void, Zenith_ECSError> RewriteContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xElements) = 0;
	// elaboration (consumer seam): the value the last SetPlaybackVersion stored, 0 outside every playback (§ "Playback version")
	virtual uint32 GetPlaybackVersion() const = 0;
};

/// @contract interface Zenith_World
/// Ownership: Created by one backend factory (Zenith_CreateWorld_Custom, _Flecs, _EnTT) as a Zenith_Owned<Zenith_World>;
///   owned by its Zenith_WorldSimulation; storage from m_pxHeap.
/// Thread affinity: PHASE-dependent — chunk reads and declared-column writes from frame-graph nodes; structural calls only
///   through Zenith_WorldPlaybackAccess on the world's sync-point node; CompileQuery, RefreshQuery, ForceCompaction and
///   RunEndOfTick on the sync-point node or out-of-tick; EXTERNALLY_SYNCHRONISED otherwise.
/// Invalidation: Destroying it invalidates every handle, view, query ID and binding it issued.
/// Units: Rows, chunks, bytes.
/// Errors: Zenith_Result<…, Zenith_ECSError> with the codes of Zenith_ECSErrorCode; GetChunk with an out-of-range ordinal
///   is ZENITH_ASSERT and returns the empty view. GetChunk and GetChunkOfEntity return commit-disabled views (merge
///   version 0, every column read-only); only the stamping paths of § "Zenith_ChunkView header" make a view writable.
/// Reentrancy: NOT_APPLICABLE. The world invokes the registry's lifecycle hooks, which call no ECS API.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_World
{
public:
	virtual ~Zenith_World() = default;

	virtual uint16 GetWorldTag() const = 0;
	virtual Zenith_Result<Zenith_EntityLocation, Zenith_ECSError> Resolve(Zenith_EntityHandle xEntity) const = 0;
	virtual Zenith_Result<Zenith_QueryID, Zenith_ECSError> CompileQuery(const Zenith_QueryDesc& xDesc) = 0;
	virtual uint32 GetChunkCount(Zenith_QueryID xQuery) const = 0;
	virtual Zenith_ChunkView GetChunk(Zenith_QueryID xQuery, uint32 uChunkOrdinal,
		const Zenith_InvocationScope& xScope) = 0;
	virtual Zenith_WorldPlaybackAccess& GetPlaybackAccess(const Zenith_SyncPointToken& xToken) = 0;
	virtual void RunEndOfTick(const Zenith_SyncPointToken& xToken) = 0;
	virtual Zenith_WorldStats GetStats() const = 0;

	virtual void ForceCompaction(const Zenith_SyncPointToken& xToken) = 0;

	// elaboration: the rest of the backend surface (Backends.md, "Common backend contract")
	virtual Zenith_Result<Zenith_ChunkView, Zenith_ECSError> GetChunkOfEntity(Zenith_QueryID xQuery,
		Zenith_EntityHandle xEntity, const Zenith_InvocationScope& xScope, uint32& uRowOut) = 0;
	virtual uint32 GetQueryStructureVersion() const = 0;
	virtual bool IsQueryStale(Zenith_QueryID xQuery) const = 0;
	virtual void RefreshQuery(const Zenith_SyncPointToken& xToken, Zenith_QueryID xQuery) = 0;
	virtual const Zenith_EntityTable& GetEntityTable() const = 0;
	virtual const Zenith_ContainerHeap& GetContainerHeap() const = 0;
	virtual const Zenith_TypeRegistry& GetTypes() const = 0;
	virtual const char* GetBackendName() const = 0;               // "Custom", "Flecs", "EnTT"
	virtual void SetArbitrationStaging(const Zenith_SyncPointToken& xToken, Zenith_Span<const Zenith_TypeID> axTypes,
		Zenith_Span<const Zenith_PropertyID> axProperties, Zenith_Span<const uint8> auSourceCounts) = 0;
	virtual void ResetArbitrationStaging(const Zenith_SyncPointToken& xToken) = 0;   // each sync point
	// The version wrap sweep (Queries.md, "Version wrap sweep"): every column and row version more than uThreshold merge
	// points behind uCurrentVersion on the ring 1 … 2^32 − 1 is set to the version uThreshold merge points before it.
	virtual void SweepChangeVersions(const Zenith_SyncPointToken& xToken, uint32 uCurrentVersion, uint32 uThreshold) = 0;
	// elaboration (consumer seam, since Z2 Stage 3): Zenith_WorldDesc::m_bEditWorld as the factory received it; constant for the world's
	// life; ANY_THREAD (§ "The edit-world flag")
	virtual bool IsEditWorld() const = 0;
};

/// @contract functions Zenith_PfnCreateWorldType
/// Ownership: A factory pointer carried by the generated composition table.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: As the factory it points to.
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnCreateWorld = Zenith_Result<Zenith_Owned<Zenith_World>, Zenith_ECSError> (*)(const Zenith_WorldDesc& xDesc);

/// @contract type Zenith_WorldTestAccess
/// Ownership: A static-only access type; owns nothing.
/// Thread affinity: MAIN_THREAD between ticks.
/// Invalidation: None.
/// Units: None.
/// Errors: A call from a non-test target fails TestSeparationCheck; a call while a frame runs is ZENITH_ASSERT.
/// Version: since Z2 Stage 2. Compiled in every configuration, reachable only through this type ([Z2-N-CONTRACT-415]).
/// @endcontract
class Zenith_WorldTestAccess
{
public:
	static void ForceCompaction(Zenith_World& xWorld);
	static Zenith_WorldPlaybackAccess& Playback(Zenith_World& xWorld);
	// elaboration
	static void RunEndOfTick(Zenith_World& xWorld);
	// Refreshes, in ascending query-ID order, every stale query the simulation compiled into its world (each live slot's
	// query and each arbitration record's internal query) and returns how many were stale; a call while a frame runs is
	// ZENITH_ASSERT and returns 0 with nothing refreshed.
	static uint32 RefreshAllQueries(Zenith_WorldSimulation& xSimulation);
	// Advances the simulation's unwrapped merge counter by ulMerges without running a tick: adds ulMerges to the tick base B
	// of the latest tick, so GetChangeVersion() and the next tick continue from the advanced counter; runs the version wrap
	// sweep once for every multiple of 2^29 the advance crosses, each with c the version of that multiple (a real tick
	// crosses at most one, because M < 2^20 < 2^29), and leaves the sweep floor at the advanced counter's (Queries.md,
	// "Version wrap sweep").
	static void AdvanceMergeCounter(Zenith_WorldSimulation& xSimulation, uint64 ulMerges);
	// Lowers the merge-point limit that the next Freeze or recompile of the simulation's registry checks (default and
	// maximum 2^20), so a test reaches the limit without 2^20 declarations (Queries.md, "Change versions and merge
	// versions" item 1). A value of 0 or above 2^20 is ZENITH_ASSERT and changes nothing.
	static void SetMergePointLimit(Zenith_WorldSimulation& xSimulation, uint32 uLimit);
};
```

`GetChunkOfEntity` returns the (commit-disabled) view of the chunk holding `xEntity` with `xQuery`'s columns and writes the entity's row; it fails with `ECS_ERROR_NO_COMPONENT` when the entity does not structurally match the query; `Zenith_SystemContext::GetChunkOfEntity` calls it and stamps the result. It is the random-access path of `Zenith2/Docs/Scripting/Bindings.md` → "Field access algorithm" step 4. `SetArbitrationStaging` tells the backend which properties are arbitrated and how many writer sources stage them (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Arbitration merge").

### The edit-world flag

<!-- elaboration -->
`Zenith_WorldDesc::m_bEditWorld` marks an **edit world**: a world that is the editor's projection of its documents, and the only kind of world whose stored rows a schema revision migrates (`Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world"). The flag has one owner and one producer:

- **Owner.** ECS declares the field, its reader `Zenith_World::IsEditWorld` and their meaning, and is the only area that reads it to decide anything: `Zenith_WorldSimulation::ApplySchemaRevision` (Stage 4) refuses a world whose `IsEditWorld()` is false with `ECS_ERROR_INVALID_DESC`. Every other consumer only reports it.
- **Producer.** The flag is set exactly once, by the creator of the world, from the world's purpose: the runtime's world creation sets `m_bEditWorld = true` for a world of purpose `WORLD_PURPOSE_EDIT` and false for every other purpose, so `Zenith_RuntimeToolAccess::CreateWorld` of `WORLD_PURPOSE_EDIT` is the one path that produces an edit world (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access"). The editor never sets the flag; it chooses the purpose. A world created by a backend factory with a default desc (every test world, the startup world, a transition target) is not an edit world.
- **Backends.** Each factory copies the flag from the validated desc at construction step 1 (§ "Factory construction steps" of `Zenith2/Docs/Design/ECS/Backends.md`) into the world object, and `IsEditWorld` returns that copy for the world's whole life; the call is ANY_THREAD, O(1) and never fails. The flag changes no storage, query, playback, change-tracking or world-state-hash behaviour: two worlds that differ only in the flag hold, play and hash identically.
- **Since Z2 Stage 3.** The field and the reader land together, before any consumer reads them.

`Test_ECS_EditWorldFlagReported` (conformance, every backend) asserts it.

### Container writes without a merge reservation

<!-- elaboration -->
`SetContainer` has one rule on every backend: it takes a new block of `Zenith_ContainerHeap::GetBlockBytes(element bytes, uCount)` with `AllocateReserved` (block 0 for a zero count), copies the elements in, and queues the replaced block with `FreeDeferred`, whether or not the two counts are equal. `AllocateReserved` runs only within a reservation of that size class, so the caller of a `SetContainer` of non-zero block bytes holds one:

| Caller | Who reserves |
|---|---|
| sync-point step 5 and the ticket-addressed records of step 6 (`SET_CONTAINER` of an accepted buffer) | the merge that accepted the buffer (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance") |
| a structural observer's follow-up buffer, out-of-tick playback | the playback itself, one `Reserve` per record before the call; a refusal drops the record with `ECS_ERROR_CAPACITY` (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Steps" step 6 for follow-ups, → "Out-of-tick playback" step 1) |
| a tick-boundary handler | `Zenith_TickBoundaryAccess::SetContainer`, which reserves for its caller (§ "Zenith_ECSObservers header") |
| any other direct caller of the playback access (a module command's playback function or a lifecycle listener at step 7, a structural observer's own call) | the caller: `GetContainerHeap().Reserve(Zenith_ContainerHeap::GetBlockBytes(element bytes, uCount))` before the call, false meaning the write is refused with nothing changed, and `CancelReservation` of the same bytes when the call then fails |

A `SetContainer` of non-zero block bytes with no reservation of its class outstanding is a programmer error (`AllocateReserved`'s `ZENITH_ASSERT`), never a data error. A caller that only needs to change the values of a container whose count stays the same uses `RewriteContainer` (§ "In-place container rewrite"), which needs no reservation at all.

### In-place container rewrite

<!-- elaboration -->
`Zenith_WorldPlaybackAccess::RewriteContainer(xEntity, xType, xProperty, xElements)` writes new values into the block a container cell already owns. Every backend implements it with these steps; P is the property's `Zenith_PropertyInfo`, C the entity's cell `{ m_uBlock, m_uCount }`:

1. Resolve `xEntity` and check `xType` exactly as `SetProperty` does: `ECS_ERROR_NULL_HANDLE`, `ECS_ERROR_STALE_HANDLE`, `ECS_ERROR_WRONG_WORLD`, `ECS_ERROR_UNKNOWN_TYPE`, `ECS_ERROR_NO_COMPONENT`, each changing nothing.
2. P is not `PROPERTY_TYPE_CONTAINER`, or its element **carries lifetime** — `m_eElementValueType` is `PROPERTY_TYPE_ASSET`, or `PROPERTY_TYPE_STRUCT` whose element type's `Zenith_TypeLayout::m_bTriviallyCopyable` is false — → `ECS_ERROR_UNKNOWN_PROPERTY` with `m_xType` and `m_xProperty` set, logged under `ecs.playback` as `ecs: RewriteContainer refused <stable name>.<property name> entity <simID>: element type carries lifetime` (or `: not a container`), changing nothing. A lifetime-carrying element is replaced with `SetContainer`, whose copy and destroy hooks keep every reference balanced.
3. `xElements.size() != C.m_uCount × P.m_uElementSize` → `ECS_ERROR_VALUE_SIZE`, changing nothing. The count cannot change through this call.
4. `C.m_uCount == 0` → success with nothing written, recorded or stamped.
5. Otherwise copy `C.m_uCount × P.m_uElementSize` bytes from `xElements` into `GetBlock(C.m_uBlock)` with move semantics (`memmove`: `xElements` may be, or overlap, the block's own bytes, such as a span `ReadContainer` returned). C is unchanged; nothing is reserved, allocated, freed or deferred, and no hook runs (step 2 admits only elements that have none).
6. Record or stamp exactly as a successful `SetContainer`: inside a playback write set the row's bit in its (chunk, column) block; outside it the row's and column's versions take the playback version (`Zenith2/Docs/Design/ECS/Backends.md` → "Playback writes"). The chunk's epoch, the query structure version and every cache are unchanged.

It is O(count × element bytes), allocates nothing on any backend and cannot fail for capacity, so it is the write a lifecycle listener or a module command's playback uses to change values held in container elements at a sync point without a reservation — for example shifting positions held in container elements at the origin-shift sync point. It is legal wherever `Zenith_WorldPlaybackAccess` is (the sync-point node, out-of-tick playback, world loading), never on a merge node: the merge-time container swap keeps `SetContainer`, because readers of the same phase may still hold the replaced block, which stays readable until the end of the tick; at a sync point no view is live (Reentrancy NONE), so rewriting in place hides nothing from anyone. `Test_ECS_RewriteContainerInPlace` (conformance, every backend) asserts every step.

### Playback version

<!-- elaboration -->
`Zenith_WorldPlaybackAccess::GetPlaybackVersion()` returns the value the last `SetPlaybackVersion` stored (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Playback write commit" item 0), so a module command's playback function, a structural observer or a lifecycle listener can tell one playback from the next:

| While | It returns |
|---|---|
| sync point p of tick t (steps 1 to 10, observers and listeners included) | the sync point's merge version |
| the end-of-tick playback of tick t | V(t, M) |
| tick-boundary steps 0 to 3 of tick t | V(t, 0) |
| an out-of-tick playback | `Zenith_WorldSimulation::GetChangeVersion()`, which is V(t, M) of the current tick t |
| anything else (between playbacks, before the first) | 0 |

Two in-tick playbacks of a world return different values unless the world's unwrapped merge counter advanced by an exact multiple of 2^32 − 1 between them (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"). Every out-of-tick playback between tick t's end-of-tick playback and tick t + 1's boundary returns V(t, M): the same value as each other and as that end-of-tick playback, which plays no `MODULE` record. A consumer that acts at most once per sync point compares the value with the one it stored at its last action. The call is O(1), never fails and changes nothing; `Test_ECS_PlaybackVersionPerPlayback` (conformance, every backend) asserts the table.

## Zenith_CommandBuffer header

`Zenith_MergeKey`, the record header and the members `Spawn`, `Destroy`, `AddComponent`, `RemoveComponent`, `SetProperty`, `Record` and `Reserve` are `Zenith2/Docs/Architecture/ECSModel.md` → "Command buffers"; `Zenith_SpawnDesc` is `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Spawn".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_CommandBuffer.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"

class Zenith_Arena;

/// @contract value Zenith_MergeKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Ordered lexicographically (system order, simulation ID, invocation ordinal, sequence).
/// @endcontract
struct Zenith_MergeKey
{
	uint32 m_uSystemOrder = 0;
	uint64 m_ulSimulationID = 0;
	uint32 m_uInvocationOrdinal = 0;
	uint32 m_uSequence = 0;
	friend constexpr bool operator==(const Zenith_MergeKey&, const Zenith_MergeKey&) = default;
	friend constexpr bool operator<(const Zenith_MergeKey& xA, const Zenith_MergeKey& xB)
	{
		if (xA.m_uSystemOrder != xB.m_uSystemOrder) { return xA.m_uSystemOrder < xB.m_uSystemOrder; }
		if (xA.m_ulSimulationID != xB.m_ulSimulationID) { return xA.m_ulSimulationID < xB.m_ulSimulationID; }
		if (xA.m_uInvocationOrdinal != xB.m_uInvocationOrdinal) { return xA.m_uInvocationOrdinal < xB.m_uInvocationOrdinal; }
		return xA.m_uSequence < xB.m_uSequence;
	}
};

/// @contract value Zenith_SpawnDesc
/// Ownership: Plain value; borrows m_axComponents for the Spawn call (the buffer copies the values).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after the call.
/// Units: None.
/// Errors: Validated at playback: an unknown type drops the spawn with ECS_ERROR_UNKNOWN_TYPE logged; a stale parent drops
///   it with ecs.commands_stale_delta.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SpawnDesc
{
	const Zenith_Span<const Zenith_ComponentInit> m_axComponents = {};
	const Zenith_PersistentID m_xPersistentID = {}; // null for runtime-only entities
	const Zenith_EntityHandle m_xParent = {};    // null for a root; a ticket handle of the same buffer is accepted
};

/// @contract type Zenith_CommandBuffer
/// Ownership: One per logical invocation (system order, simulation ID, invocation ordinal) of a system invocation, or one
///   per out-of-tick batch; its records live in the arena passed at construction; committed to the world only by the
///   recording invocation's merge (Zenith_MergeContext::AcceptBuffer) or by Zenith_WorldSimulation::ApplyOutOfTick.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Tickets and ticket handles are meaningful only for this buffer; the buffer is invalid after its arena resets.
/// Units: Bytes of records; the record header is 32 bytes, payloads 8-byte aligned.
/// Errors: Recording functions return void: arena growth beyond the arena's cap is FATAL_CODE_ARENA_EXHAUSTED, and a
///   record that would exceed uCapBytes is dropped and counted in ecs.commands_over_cap_delta with a log entry (never an
///   assert); Reserve returns ECS_ERROR_CAPACITY when uBytes would exceed the cap. Semantic errors (stale handles,
///   unknown types, wrong value sizes) are detected at playback.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_CommandBuffer
{
public:
	// elaboration: construction; systems obtain buffers from Zenith_SystemContext::Commands
	Zenith_CommandBuffer(Zenith_Arena& xArena, const Zenith_MergeKey& xBaseKey, uint32 uCapBytes);

	Zenith_SpawnTicket Spawn(const Zenith_SpawnDesc& xDesc);
	void Destroy(Zenith_EntityHandle xEntity);
	void AddComponent(Zenith_EntityHandle xEntity, const Zenith_ComponentInit& xComponent);
	void RemoveComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType);
	void SetProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType, Zenith_PropertyID xProperty, Zenith_ByteSpan xValue);
	void Record(uint32 uModuleCommandKind, Zenith_ByteSpan xPayload);
	Zenith_Result<void, Zenith_ECSError> Reserve(uint32 uBytes);

	// elaboration
	void SetContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType, Zenith_PropertyID xProperty,
		Zenith_ByteSpan xElements, uint32 uCount);                        // elements in the element type's value encoding
	void SetParent(Zenith_EntityHandle xEntity, Zenith_EntityHandle xParent);   // null parent = make root
	static Zenith_EntityHandle TicketHandle(Zenith_SpawnTicket xTicket);
	const Zenith_MergeKey& GetBaseKey() const;
	uint32 GetRecordCount() const;
	uint32 GetRecordBytes() const;
	uint32 GetSpawnCount() const;
	uint32 GetContainerBytes() const;    // container storage its SetContainer records need at playback
	void Discard();                                     // drops every record (rejected or rolled back)
};
```

Record kinds (`m_uKind` of the 32-byte header; payload layouts in `Zenith2/Docs/Design/ECS/Scheduling.md` → "Record format"):

| Kind | Value | Recorded by | Playback step |
|---|---|---|---|
| `ECS_RECORD_SPAWN` | 1 | `Spawn` | 6 |
| `ECS_RECORD_DESTROY` | 2 | `Destroy` | 6 (mark) and end of tick (removal) |
| `ECS_RECORD_ADD_COMPONENT` | 3 | `AddComponent` | 6 |
| `ECS_RECORD_REMOVE_COMPONENT` | 4 | `RemoveComponent` | 6 |
| `ECS_RECORD_SET_PROPERTY` | 5 | `SetProperty` | 5 |
| `ECS_RECORD_SET_CONTAINER` | 6 | `SetContainer` | 5 |
| `ECS_RECORD_SET_PARENT` | 7 | `SetParent` | 6 |
| `ECS_RECORD_MODULE` | 8 | `Record` (module and lifecycle command kinds) | 7 |
| `ECS_RECORD_EVENT` | 9 | `Zenith_EventWriter::Emit` (events travel in the same buffer) | 8 |
| `ECS_RECORD_CONTRIBUTION` | 10 | `Zenith_ContributionWriter::Contribute` | folded at merge, never played back |

## Zenith_CommandKinds header

<!-- elaboration -->
`Zenith_CommandKindDesc` and `Zenith_PfnCommandPlayback` are the declarations of `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" (module interfaces), declared here because the sync-point playback that calls them is ZenithECS code and ZenithECS (layer 3) cannot include a ZenithRuntime header; `ZenithRuntime/Zenith_Module.h` includes this header instead of declaring them.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_CommandKinds.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_CommandBuffer.h"

class Zenith_WorldPlaybackAccess;
class Zenith_TypeRegistry;
class Zenith_Heap;

/// @contract functions Zenith_PfnCommandPlaybackType
/// Ownership: A function pointer registered with a command kind.
/// Thread affinity: The world's sync-point node, single-threaded per world, in merge order.
/// Invalidation: None.
/// Units: xPayload holds exactly the recorded payload bytes.
/// Errors: The function logs and counts its own failures; it never asserts on payload contents.
/// Version: since Z2 Stage 2.
/// @endcontract
// The kind table and its m_pUserData are one per process, so a module whose playback changes per-world module state keys
// that state by xWorld.GetWorldTag() (Architecture/ECSModel.md, Zenith_WorldPlaybackAccess; [Z2-N-ARCH-716]).
using Zenith_PfnCommandPlayback = void (*)(Zenith_WorldPlaybackAccess& xWorld, Zenith_ByteSpan xPayload,
	const Zenith_MergeKey& xKey, void* pUserData);

// elaboration (consumer seam)
/// @contract enum Zenith_CommandKindFlags
/// Ownership: Plain enumeration of bits, combined into Zenith_CommandKindDesc::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bits.
/// Errors: A bit outside uCOMMAND_KIND_FLAGS_DEFINED_MASK is ECS_ERROR_INVALID_DESC at Zenith_CommandKindTable::Register.
/// Version: since Z2 Stage 2. A bit never changes meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_CommandKindFlags : uint32
{
	COMMAND_KIND_FLAG_PLAY_LAST = 1u << 0,     // records play after every other step-7 record of the playback (§ "Play-last command kinds")
};
inline constexpr uint32 uCOMMAND_KIND_FLAGS_DEFINED_MASK = 0x1u;

/// @contract value Zenith_CommandKindDesc
/// Ownership: Plain value; m_szName copied at registration; m_pUserData owned by the registrant, outliving every world.
/// Thread affinity: IMMUTABLE. Members non-const: Register copies it over a slot of the kind table's fixed array.
/// Invalidation: None.
/// Units: Bytes; phase bit mask (bit p set = recordable in phase p, 1 to 10); m_uFlags a set of COMMAND_KIND_FLAG_* bits.
/// Errors: Validated by Zenith_CommandKindTable::Register.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CommandKindDesc
{
	const char* m_szName = nullptr;            // "<Module>.<Command>"
	Zenith_TypeID m_xPayloadType;              // a registered TYPE_KIND_STRUCT describing the payload
	uint32 m_uMaxPayloadBytes = 0;
	uint32 m_uAllowedPhaseMask = 0;
	Zenith_PfnCommandPlayback m_pfnPlayback = nullptr;
	void* m_pUserData = nullptr;
	// elaboration (consumer seam): 0 for an ordinary kind; COMMAND_KIND_FLAG_PLAY_LAST (§ "Play-last command kinds")
	uint32 m_uFlags = 0;
};

/// @contract type Zenith_CommandKindTable
/// Ownership: Created by Create as a Zenith_Owned<Zenith_CommandKindTable>; the composition root owns one per process,
///   fills it in startup step 15, seals it before step 16 and passes it to every Zenith_SystemRegistryDesc.
/// Thread affinity: MAIN_THREAD until Seal; ANY_THREAD and IMMUTABLE afterwards, except Remove (MAIN_THREAD), which after
///   Seal is legal only while no world and no system registry refers to the table (the composition root's undo of a
///   module registration at module unload or failed startup, [Z2-N-ARCH-851]); the table cannot observe that, so it is
///   the caller's precondition.
/// Invalidation: Kind IDs are valid for the process. Remove tombstones a kind: its ID is never reused, Find of it returns
///   null and FindByName no longer finds its name (which a later Register may take again under a new ID).
/// Units: A kind ID is the registration index + 1 (0 is never a kind); the ID Register returns is the token Remove takes.
///   GetCount counts tombstoned kinds too (it is the highest kind ID issued).
/// Errors: Register returns ECS_ERROR_INVALID_DESC for an empty or duplicate name, a zero phase mask or a bit outside
///   1–10, a zero payload bound, a null playback function, or an m_uFlags bit outside uCOMMAND_KIND_FLAGS_DEFINED_MASK;
///   ECS_ERROR_UNKNOWN_TYPE for a payload type that is not a
///   registered TYPE_KIND_STRUCT; ECS_ERROR_CAPACITY after 4,096 kinds (tombstones included). Register after Seal is
///   ZENITH_CHECK. Remove of 0, of an ID never issued or of a kind already removed is ZENITH_ASSERT and changes nothing.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_CommandKindTable
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_CommandKindTable>, Zenith_ECSError> Create(const Zenith_TypeRegistry& xTypes,
		Zenith_Heap& xHeap);
	Zenith_Result<uint32, Zenith_ECSError> Register(const Zenith_CommandKindDesc& xDesc);
	void Remove(uint32 uKind);      // elaboration: tombstones the kind Register returned ([Z2-N-ARCH-851] undo by token)
	void Seal();
	bool IsSealed() const;
	const Zenith_CommandKindDesc* Find(uint32 uKind) const;
	uint32 FindByName(const char* szName) const;     // 0 when absent
	uint32 GetCount() const;
};
```

### Play-last command kinds

<!-- elaboration -->
A kind registered with `COMMAND_KIND_FLAG_PLAY_LAST` in `m_uFlags` is a **play-last kind**. Step 7 of every playback that plays `MODULE` records (a sync point, out-of-tick playback; `Zenith2/Docs/Design/ECS/Scheduling.md` → "Steps" step 7) makes two passes over the same records: the first plays every record whose kind is not play-last, in merge order; the second plays every record of a play-last kind, in merge order among themselves. A play-last record therefore plays after every other `MODULE` record of the playback, whatever its merge key, and after every structural record of step 6, which precedes step 7 anyway. Nothing else changes: recording, the phase mask, the payload bound, merge acceptance, counters and the played-back command log (which lists records in the order they played) treat the kind like any other. The flag is a property of the kind, fixed at `Register`; the second pass costs one more walk over the playback's records, O(records). A module uses it for a command whose effect every other module command of the same sync point must precede, such as a world-origin shift that must see every position the sync point wrote. `Test_CommandKindTable_PlayLastFlag` (unit) and `Test_ECS_PlayLastKindsPlayAfterOtherRecords` (conformance, every backend) assert it.

## Zenith_WorldSimulation header

`Create`, `GetWorld`, `EmitInto`, `GetTickIndex`, `GetChangeVersion` and `GetNextSimulationID` are `Zenith2/Docs/Architecture/ECSModel.md` → "Worlds and backends".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_WorldSimulation.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithECS/Zenith_World.h"
#include "ZenithECS/Zenith_System.h"
#include "ZenithECS/Zenith_ECSObservers.h"
#include "ZenithECS/Zenith_WorldInspector.h"

class Zenith_FrameGraphBuilder;
class Zenith_Heap;
class Zenith_StringTable;
class Zenith_Hash64Stream;

/// @contract value Zenith_SpawnResult
/// Ownership: Plain value in the simulation's spawn-result log.
/// Thread affinity: IMMUTABLE.
/// Invalidation: The log is replaced at the next sync point.
/// Units: None.
/// Errors: m_xEntity is null for a rejected or dropped spawn.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SpawnResult
{
	const Zenith_MergeKey m_xKey = {};   // the spawn record's key
	const Zenith_SpawnTicket m_xTicket = {};
	const Zenith_EntityHandle m_xEntity = {};
	const uint64 m_ulSimulationID = 0;
};

/// @contract type Zenith_WorldSimulation
/// Ownership: Created by Create as a Zenith_Owned<Zenith_WorldSimulation>; owns its world, its system registry, command
///   and event queues, merge capacity, per-tick arenas and the node records it registers; the composition root owns it.
/// Thread affinity: MAIN_THREAD for creation, destruction, EmitInto, ApplyOutOfTick and the out-of-tick accessors; the
///   tick-boundary, system, merge, arbitration and sync-point bodies it registers run on the job system.
/// Invalidation: Destroying it destroys the world (every handle and view invalid) and reports the highest generation to
///   the world registry through its creator.
/// Units: Ticks; merge versions.
/// Errors: Create returns Zenith_ECSError (a null world or registry is ECS_ERROR_INVALID_DESC); EmitInto returns
///   Zenith_ScheduleError with every diagnostic in the registry; ApplyOutOfTick returns the first playback error and applies
///   every other record.
/// Reentrancy: NONE. Observers and tick-boundary handlers it calls record follow-up commands and never call back into it.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_WorldSimulation
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_WorldSimulation>, Zenith_ECSError> Create(
		Zenith_Owned<Zenith_World> xWorld, Zenith_Owned<Zenith_SystemRegistry> xSystems, Zenith_Heap& xHeap);

	Zenith_World& GetWorld();
	Zenith_Result<void, Zenith_ScheduleError> EmitInto(Zenith_FrameGraphBuilder& xBuilder);
	uint64 GetTickIndex() const;
	uint32 GetChangeVersion() const;
	uint64 GetNextSimulationID() const;

	// elaboration
	Zenith_SystemRegistry& GetSystems();
	void AddStructuralObserver(Zenith_StructuralObserver& xObserver);            // before the first tick
	void AddTickBoundaryHandler(Zenith_TickBoundaryHandler& xHandler);           // before the first tick
	Zenith_Result<void, Zenith_ECSError> ApplyOutOfTick(Zenith_CommandBuffer& xBuffer);   // loading, editor edits
	void RestoreSimulationState(uint64 ulTickIndex, uint64 ulNextSimulationID);                       // save load, out of tick
	Zenith_Span<const Zenith_SpawnResult> GetSpawnResults() const;                         // of the latest sync point
	Zenith_WorldInspector GetInspector() const;                                           // out of tick
	void HashTickState(const Zenith_StringTable& xStrings, Zenith_Hash64Stream& xHash) const;           // after the end of tick
	void SetPaused(bool bPaused);                                                                       // applied at the next tick boundary
	Zenith_Result<void, Zenith_ECSError> RunTicksThenPause(uint32 uTicks);            // MAIN_THREAD, between frames (§ "Run ticks then pause")
	bool IsInFrame() const;
};
```

`GetChangeVersion` returns, while a frame instance of the world runs, the merge version of the latest completed sync point (the end-of-tick playback is the last sync point of a tick): during a phase it is the version the preceding sync point committed at, and each merge point of the phase commits at its own precomputed merge version above it. Out of tick it returns V(t, M) of the current tick index t, whether or not tick t's end-of-tick playback completed: after a cancelled frame instance the two differ, and only V(t, M) is above every read version the instance's completed systems took (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 5). Before the first tick it returns 1, the reserved version of tick 0's boundary (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions"). Out-of-tick playback stamps it. `RestoreSimulationState(t, id)` also resets the unwrapped merge counter's tick base to t × (M + 1) for the current merge-point count M, so versions after a save load or a replay checkpoint are a function of the restored tick alone, and resets the change-tracking state to the restored point: every stored column and row version is re-stamped to V(t, M), every query slot's last-seen and own versions and every system's previous read version to 0, and the sweep floor to the restored counter's (`Zenith2/Docs/Design/ECS/Queries.md` → "Change versions and merge versions" item 2), so every `QUERY_TERM_CHANGED` system sees every chunk in its first invocation after the restore, and a replay resumed from the restored state reproduces the recorded tick hashes' ECS part from the next tick, because no system's committed result depends on whether an unchanged row was handed to it (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"). <!-- elaboration --> `SetPaused` is the world's own pause ([Z2-N-ARCH-764]): the tick-boundary node applies a pending `SetPaused` before its step 1; a paused world's tick-boundary node then runs none of steps 1 to 4 (no asynchronous result is published into a paused world — they stay queued and publish in completion-ID order at the first tick boundary after the world resumes, so a publication's recorded tick is always a tick that ran), the world's tick index and tick base do not advance, and every other node of that world's tick instance completes without running its body; the flag never changes the schedule. A world-transition load completion addressed to a paused old world is such a result: it stays queued, and the swap waits, until the old world resumes (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition").

<!-- assumption: pending core-doc batch (X123) -->
The paused-world rule is X123's first item: `Zenith2/Docs/Architecture/FramePhases.md` [Z2-N-ARCH-505], `Zenith2/Docs/Scripting.md` [Z2-N-SCRIPT-335] and the load-completion sentence of `Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition" take the paused-world exception in the core-doc batch that applies it; until then a worker follows this paragraph.
<!-- /assumption -->

### Run ticks then pause

<!-- elaboration -->
`RunTicksThenPause(uTicks)` runs a paused world for exactly `uTicks` ticks and then pauses it again (A3 cross-area row ED10; GAME's single step, `Zenith2/Docs/Design/GAME/WorldHost.md` → "Single step", is its caller). It is `MAIN_THREAD` and called between frames: a call while `IsInFrame()` is true is `ZENITH_ASSERT` and changes nothing. `uTicks` above `uECS_MAX_RUN_TICKS` (600) returns `ECS_ERROR_INVALID_DESC` and changes nothing.

The world simulation holds a run as `bool m_bTickRunActive` and `uint32 m_uTickRunRemaining`. An accepted call sets both (`m_uTickRunRemaining = uTicks`) and makes a pending `SetPaused(false)`. Step 0 of the tick-boundary node (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick") applies the run after any pending `SetPaused`:

1. While a run is active and `m_uTickRunRemaining` is greater than 0, the boundary decrements it and the tick runs.
2. When a run is active and `m_uTickRunRemaining` is 0, the boundary sets the world paused, ends the run and stops, exactly as a paused world's boundary does.

So `RunTicksThenPause(n)` with n ≥ 1 runs ticks at the next n boundaries and pauses at the boundary after the n-th. `RunTicksThenPause(0)` cancels a run and pauses at the next boundary, with no tick run. A `SetPaused` call while a run is active ends the run, and its own value applies at the next boundary. A new `RunTicksThenPause` call replaces the active run. The run is not simulation state: it is neither saved, hashed nor replayed. A step's ticks are ordinary ticks with the world's recorded inputs (Z2-D-019).

| Test | Asserts, on every backend |
|---|---|
| `Test_ECS_RunTicksThenPause_RunsExactlyN` | a paused conformance world given `RunTicksThenPause(3)` advances its tick index by exactly 3 over five frames of one tick each, its phase-1 system runs 3 times, and the world is paused afterwards with no handler step called at the pausing boundary |
| `Test_ECS_RunTicksThenPause_ZeroCancels` | a run of 5 cancelled with `RunTicksThenPause(0)` after 2 ticks runs no further tick and leaves the world paused; `SetPaused(true)` during a run also ends it |
| `Test_ECS_RunTicksThenPause_Refusals` | 601 returns `ECS_ERROR_INVALID_DESC` and leaves the world paused and the run inactive; a call while a frame runs fails its `ZENITH_ASSERT` where asserts are enabled and changes nothing where they are not |

## Zenith_WorldRegistry header

Exactly `Zenith2/Docs/Architecture/ECSModel.md` → "World registry", with a factory.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_WorldRegistry.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithECS/Zenith_ECSTypes.h"

class Zenith_Heap;

/// @contract value Zenith_WorldTagIssue
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: A world tag (1 … 4095) and a generation.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_WorldTagIssue
{
	const uint16 m_uTag = 0;
	const uint32 m_uInitialGeneration = 0; // copied into Zenith_WorldDesc::m_xEntityTable; never 0
};

/// @contract type Zenith_WorldRegistry
/// Ownership: Created by Create as a Zenith_Owned<Zenith_WorldRegistry>; owned by the composition root (startup step 10)
///   and passed to whatever creates worlds; never global state ([Z2-N-ARCH-706]). Fixed storage: 4,096 tag records of
///   8 bytes.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Tags; generations.
/// Errors: AcquireTag returns ECS_ERROR_WORLD_TAGS_EXHAUSTED when every tag is live, before any backend factory runs;
///   ReleaseTag of a tag that is not live is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_WorldRegistry
{
public:
	// elaboration
	static Zenith_Result<Zenith_Owned<Zenith_WorldRegistry>, Zenith_ECSError> Create(Zenith_Heap& xHeap);

	Zenith_Result<Zenith_WorldTagIssue, Zenith_ECSError> AcquireTag();
	void ReleaseTag(uint16 uTag, uint32 uHighestGenerationIssued);
	uint32 GetLiveWorldCount() const;
	// elaboration
	bool IsLive(uint16 uTag) const;
	uint32 GetCreationCounter() const;
};
```

## Zenith_WorldInspector header

<!-- elaboration -->
The read-only walk that EDIT's inspector, SCENE's serializer and SAVE's snapshot use outside a tick. It shows C++-registered and data-defined components identically, because it reads through reflection alone ([Z2-N-ARCH-643]); the property walk itself is `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectionWalk header".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_WorldInspector.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"

class Zenith_World;

/// @contract type Zenith_WorldInspector
/// Ownership: A value returned by Zenith_WorldSimulation::GetInspector; borrows the world.
/// Thread affinity: MAIN_THREAD, only while no frame of its world runs; phase-11 snapshot nodes read through the same
///   calls under the phase's read-only rule.
/// Invalidation: Invalid when the next frame starts or the world is destroyed.
/// Units: Entities in ascending stable simulation ID; components in byte order of stable name.
/// Errors: ECS_ERROR_NULL_HANDLE, ECS_ERROR_STALE_HANDLE, ECS_ERROR_WRONG_WORLD, ECS_ERROR_NO_COMPONENT; a destination
///   smaller than the component's size is ECS_ERROR_VALUE_SIZE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_WorldInspector
{
public:
	uint32 GetEntityCount() const;
	// Writes up to axOut.GetCount() handles starting at the uFirst-th entity in simulation-ID order; returns the count.
	uint32 ListEntities(uint32 uFirst, Zenith_Span<Zenith_EntityHandle> axOut) const;
	Zenith_Result<uint32, Zenith_ECSError> ListComponents(Zenith_EntityHandle xEntity,
		Zenith_Span<Zenith_TypeID> axOut) const;
	// Gathers the component into its reflected layout; container fields as Zenith_ContainerRef, read with ReadContainer.
	Zenith_Result<void, Zenith_ECSError> ReadComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_MutableByteSpan xOut) const;
	Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, uint32& uCountOut) const;
	Zenith_Result<uint64, Zenith_ECSError> GetSimulationID(Zenith_EntityHandle xEntity) const;
	Zenith_Result<Zenith_PersistentID, Zenith_ECSError> GetPersistentID(Zenith_EntityHandle xEntity) const;
	Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> GetParent(Zenith_EntityHandle xEntity) const;
private:
	friend class Zenith_WorldSimulation;
	explicit Zenith_WorldInspector(const Zenith_World& xWorld);
	const Zenith_World* m_pxWorld = nullptr;
};
```

Edits made in the editor are recorded as commands and applied with `Zenith_WorldSimulation::ApplyOutOfTick`; the inspector itself never writes.

## Zenith_ECSObservers header

<!-- elaboration -->
The two upward-notification interfaces ZenithECS declares ([Z2-N-ARCH-078], [Z2-N-CONTRACT-175]): the structural observer `ZenithScene` implements to forward spawn and destroy events to its `Zenith_LifecycleListener`s, and the tick-boundary handler through which SCRIPT, the runtime and CINE perform tick-boundary steps 1 to 3 of `Zenith2/Docs/Architecture/FramePhases.md` → "Sync points and the tick boundary".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ECSObservers.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"

class Zenith_WorldPlaybackAccess;
class Zenith_EntityTable;

/// @contract enum Zenith_StructuralEventKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_StructuralEventKind : uint8
{
	STRUCTURAL_EVENT_SPAWNED,             // playback step 6, after the entity exists
	STRUCTURAL_EVENT_DESTROY_PENDING,     // playback step 6, at the mark
	STRUCTURAL_EVENT_DESTROYED,           // end-of-tick removal, before the slot is released
};

/// @contract value Zenith_StructuralEvent
/// Ownership: Plain value; valid for the observer call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_StructuralEvent
{
	const Zenith_StructuralEventKind m_eKind = {};
	const Zenith_EntityHandle m_xEntity = {};
	const uint64 m_ulSimulationID = 0;
	const uint64 m_ulTick = 0;
	const Zenith_MergeKey m_xKey = {};    // the causing record's key; zero for end-of-tick removals
};

/// @contract interface Zenith_StructuralObserver
/// Ownership: Implemented above ZenithECS (ZenithScene); registered per world by the composition root in module order.
/// Thread affinity: PHASE-independent: called on the world's sync-point node, one world at a time.
/// Invalidation: None.
/// Units: None.
/// Errors: An observer records failures in its own diagnostics; it never asserts.
/// Reentrancy: NONE. An observer changes state only through the playback access it receives and through xFollowUp,
///   applied at the next sync point ([Z2-N-ARCH-544]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_StructuralObserver
{
public:
	virtual ~Zenith_StructuralObserver() = default;
	virtual void OnStructuralEvent(const Zenith_StructuralEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld,
		Zenith_CommandBuffer& xFollowUp) = 0;
};

/// @contract enum Zenith_TickBoundaryStep
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_TickBoundaryStep : uint8
{
	TICK_BOUNDARY_STEP_PUBLISH_ASYNC = 1,      // step 1
	TICK_BOUNDARY_STEP_RESTORE_CINEMATICS = 2, // step 2
	TICK_BOUNDARY_STEP_APPLY_DISABLES = 3,     // step 3
};

/// @contract type Zenith_TickBoundaryAccess
/// Ownership: A borrowed wrapper over the world's Zenith_WorldPlaybackAccess, constructed by the world simulation on the
///   tick-boundary node's stack for one tick boundary; never copied or stored by a handler.
/// Thread affinity: OWNER_THREAD — the world's tick-boundary node, for the duration of the OnTickBoundary calls.
/// Invalidation: Invalid when the tick-boundary node's step 3 returns.
/// Units: Bytes of value encodings.
/// Errors: The wrapped playback calls' Zenith_ECSError codes (ECS_ERROR_NULL_HANDLE, ECS_ERROR_STALE_HANDLE,
///   ECS_ERROR_WRONG_WORLD, ECS_ERROR_NO_COMPONENT, ECS_ERROR_UNKNOWN_TYPE, ECS_ERROR_UNKNOWN_PROPERTY,
///   ECS_ERROR_VALUE_SIZE); SetContainer also returns ECS_ERROR_CAPACITY when the container heap cannot reserve the new
///   block, changing nothing. Never an assert on data. It exposes no structural member, so a structural change at the
///   tick boundary does not compile.
/// Version: since Z2 Stage 2. elaboration
/// @endcontract
class Zenith_TickBoundaryAccess
{
public:
	// Writes and marks the row in the tick boundary's playback write set; committed at the tick boundary's merge version.
	Zenith_Result<void, Zenith_ECSError> SetProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xValue);
	// elaboration (consumer seam): reserves the new block itself (an out-of-tick reservation; ECS_ERROR_CAPACITY when refused, nothing
	// changed), then Zenith_WorldPlaybackAccess::SetContainer; property-only, marked and committed like SetProperty
	// (§ "Tick-boundary container writes")
	Zenith_Result<void, Zenith_ECSError> SetContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, Zenith_ByteSpan xElements, uint32 uCount);
	Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadProperty(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty) const;
	Zenith_Result<Zenith_ByteSpan, Zenith_ECSError> ReadContainer(Zenith_EntityHandle xEntity, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, uint32& uCountOut) const;
	bool HasComponent(Zenith_EntityHandle xEntity, Zenith_TypeID xType) const;
	bool IsDestroyPending(Zenith_EntityHandle xEntity) const;
	Zenith_Result<Zenith_EntityHandle, Zenith_ECSError> FindByPersistentID(Zenith_PersistentID xID) const;
	const Zenith_EntityTable& GetEntityTable() const;
private:
	friend class Zenith_WorldSimulation;
	explicit Zenith_TickBoundaryAccess(Zenith_WorldPlaybackAccess& xAccess);
	Zenith_WorldPlaybackAccess* m_pxAccess = nullptr;
};

/// @contract interface Zenith_TickBoundaryHandler
/// Ownership: Implemented above ZenithECS; registered per world in module order before the first tick.
/// Thread affinity: The world's tick-boundary node.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: A handler logs its own failures.
/// Reentrancy: NONE. A handler writes world state only through xWorld's property members and never records commands or
///   changes structure; a handler that needs structure (a spawn carried by an asynchronous result) publishes the result
///   to script or module state and the consumer records commands in a phase of the tick.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_TickBoundaryHandler
{
public:
	virtual ~Zenith_TickBoundaryHandler() = default;
	virtual void OnTickBoundary(Zenith_TickBoundaryStep eStep, uint64 ulTickStarting, Zenith_TickBoundaryAccess& xWorld) = 0;
};
```

<!-- elaboration -->
The tick boundary is not a structural playback point: nothing it runs spawns, destroys, adds or removes components or reparents, so no epoch, query structure version or query cache changes there and every phase-1 query sees the state the previous end-of-tick (or out-of-tick) playback left ([Z2-N-ARCH-683]). Property writes of steps 1 to 3 (a cinematic's restore, a published result written into a component) are committed after step 3 exactly like a sync point's step-5 writes, at the tick boundary's merge version (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Tick boundary and end-of-tick").

### Tick-boundary container writes

<!-- elaboration -->
The tick boundary has no merge, so no reservation exists for a container write a handler makes there; `Zenith_TickBoundaryAccess::SetContainer(xEntity, xType, xProperty, xElements, uCount)` makes its own (§ "Container writes without a merge reservation"). It reads the property's element size from the world's type registry, whose pointer the world simulation binds into the access's private state when it builds the access, and performs, in order:

1. `FindProperty(xType, xProperty)`; no such property, or one that is not `PROPERTY_TYPE_CONTAINER`, or `uCount` above its `m_uMaxCount`, or `xElements.size() != uCount × m_uElementSize` → forward the call unchanged to `Zenith_WorldPlaybackAccess::SetContainer`, which returns the matching error (`ECS_ERROR_UNKNOWN_TYPE`, `ECS_ERROR_UNKNOWN_PROPERTY` or `ECS_ERROR_VALUE_SIZE`) before it allocates anything, and return that.
2. `ReadContainer(xEntity, xType, xProperty, uCountOut)` resolves the entity; a failure (`ECS_ERROR_NULL_HANDLE`, `ECS_ERROR_STALE_HANDLE`, `ECS_ERROR_WRONG_WORLD`, `ECS_ERROR_NO_COMPONENT`) is returned with nothing reserved.
3. b = `Zenith_ContainerHeap::GetBlockBytes(m_uElementSize, uCount)`. When b > 0 and `GetContainerHeap().Reserve(b)` returns false: return `ECS_ERROR_CAPACITY` with `m_xType` and `m_xProperty` set, log WARNING under `ecs.playback` exactly `ecs: tick boundary SetContainer refused <stable name>.<property name> entity <simID>: container heap`, and change nothing (no reservation stays outstanding).
4. Forward to `Zenith_WorldPlaybackAccess::SetContainer`. A failure here (only an engine defect can cause one after steps 1 and 2) calls `CancelReservation(b)` when b > 0 and returns the error; success marks the row in the tick boundary's write set exactly as `SetProperty` does, so the write is committed after step 3 at V(t, 0) and the replaced block is freed by the end-of-tick flush.

An equal count is not special: the call still takes a new block, as `SetContainer` always does. The reservation reduces the container heap's free bytes before phase 1's starting capacity is computed at tick-boundary step 4, so the phases of the tick see it as committed state (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance"). `Test_ECS_TickBoundarySetContainer` (conformance, every backend) asserts every step.

## Zenith_ECSScriptTraits header

<!-- elaboration -->
`Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI" ([Z2-N-SCRIPT-566]) makes the owner of each handle type specialise `Zenith_ScriptValueTraits<T>` next to the type. ZenithECS specialises it for `Zenith_EntityHandle`, carrying the handle's 64 bits opaquely as a `Zenith_ScriptOpaqueHandle` of value kind `SCRIPT_VALUE_KIND_ENTITY`. The header includes `ZenithReflect/Zenith_ScriptBinding.h` and `ZenithECS/Zenith_EntityHandle.h` and nothing of ZenithScript.

<!-- assumption: pending SCRIPT -->
The binding ABI names the two trait members (`ToValue`, `FromValue`) without fixing their parameter lists; this specialisation assumes the shape below, which every handle owner (ZenithAssets, ZenithCore) would share, and the assumption is recorded for SCRIPT in the cross-area notes (row E12).
<!-- /assumption -->

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ECSScriptTraits.h
#pragma once
#include "ZenithReflect/Zenith_ScriptBinding.h"   // Zenith_ScriptValueTraits, Zenith_ScriptValue, Zenith_ScriptThunkStatus
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract functions Zenith_ECSScriptTraits
/// Ownership: Pure conversion functions; own nothing.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None; the value carries the handle bits, so a converted handle follows Zenith_EntityHandle's own
///   staleness and world-tag rules when it is resolved.
/// Units: m_xHandle.m_aulBits[0] = the handle's 64 bits; m_aulBits[1] = 0; m_xType = the zero type ID.
/// Errors: FromValue of a value whose m_eKind is not SCRIPT_VALUE_KIND_ENTITY (SCRIPT_VALUE_KIND_NIL included) returns
///   SCRIPT_THUNK_STATUS_ERROR with xError.m_uCode = 1511 and an empty message, leaves xOut unchanged, and the dispatcher,
///   which knows the function and argument index, formats the ZS1511 catalogue text; a non-zero m_aulBits[1] is the same
///   error. ToValue cannot fail; the null handle converts to an ENTITY value with zero bits.
/// Version: since Z2 Stage 2.
/// @endcontract
template<>
struct Zenith_ScriptValueTraits<Zenith_EntityHandle>
{
	static constexpr Zenith_ScriptValueKind eKIND = SCRIPT_VALUE_KIND_ENTITY;
	static Zenith_ScriptValue ToValue(Zenith_EntityHandle xEntity);
	static Zenith_ScriptThunkStatus FromValue(const Zenith_ScriptValue& xValue, Zenith_EntityHandle& xOut,
		Zenith_ScriptThunkError& xError);
};
```

`Test_ECSScriptTraits_RoundTrip` (`ZenithECS_Tests`) converts the null handle, a live handle and a ticket handle through `ToValue` and `FromValue` bit for bit, and asserts that `FromValue` of a NUMBER, a NIL and an ASSET value returns the error status with code 1511 and leaves the output handle unchanged.

## Backend factory headers

Each backend's one public header declares its factory and its fault points and nothing else ([Z2-N-ARCH-169], [Z2-N-CONTRACT-039]):

```cpp
// Zenith2/Engine/ZenithECS_Custom/Zenith_ECS_Custom.h
#pragma once
#include "ZenithECS/Zenith_World.h"

/// @contract functions Zenith_ECS_Custom
/// Ownership: Returns a Zenith_Owned<Zenith_World> the caller owns; storage from xDesc.m_pxHeap.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: ECS_ERROR_INVALID_DESC, ECS_ERROR_OUT_OF_MEMORY; every step constructed so far is unwound on failure
///   ([Z2-N-ARCH-684]).
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_World>, Zenith_ECSError> Zenith_CreateWorld_Custom(const Zenith_WorldDesc& xDesc);
```

`Zenith2/Engine/ZenithECS_Flecs/Zenith_ECS_Flecs.h` declares `Zenith_CreateWorld_Flecs` and `Zenith2/Engine/ZenithECS_EnTT/Zenith_ECS_EnTT.h` declares `Zenith_CreateWorld_EnTT`, with the same signature and contract block (named `Zenith_ECS_Flecs` and `Zenith_ECS_EnTT`), their Errors field adding `ECS_ERROR_BACKEND`. Each backend's fault points are `constexpr uint32 uFAULT_POINT_ECS_<BACKEND>_<STEP>` values in the same header, one per construction step of `Zenith2/Docs/Design/ECS/Backends.md` → "Factory construction steps".

## Typed convenience wrappers

Inline wrappers over the type-erased functions for C++-registered types ([Z2-N-ARCH-705]); they add no storage behaviour and resolve `T::xTYPE_ID` and the `uPROP_*` constants the type's registration declares (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectValueTraits header").

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ECSTyped.h (definitions in Zenith_ECSTyped.inl)
#pragma once
#include "ZenithECS/Zenith_ChunkView.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithECS/Zenith_System.h"
#include "ZenithReflect/Zenith_ReflectValueTraits.h"

/// @contract value Zenith_ColumnView
/// Ownership: A borrowed typed view over one Zenith_RawColumn; owns nothing.
/// Thread affinity: OWNER_THREAD — as the raw column.
/// Invalidation: As the raw column.
/// Units: Rows.
/// Errors: Constructing it for a property whose value type is not T's is ZENITH_ASSERT; indexing beyond the row count
///   is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename T>
class Zenith_ColumnView
{
public:
	Zenith_ColumnView(Zenith_RawColumn xColumn, Zenith_PropertyValueType eExpected);
	uint32 GetRowCount() const;
	const T& Get(uint32 uRow) const;
	void Set(uint32 uRow, const T& xValue);          // writes and calls MarkRowChanged(uRow)
	bool IsNull() const;
private:
	Zenith_RawColumn m_xColumn;
};

/// @contract functions Zenith_ECSTypedWrappers
/// Ownership: Inline wrappers; own nothing.
/// Thread affinity: As the wrapped call.
/// Invalidation: As the wrapped call.
/// Units: None.
/// Errors: As the wrapped call.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename TComponent> Zenith_SystemBuilder& Zenith_Read(Zenith_SystemBuilder& xBuilder);
template<typename TComponent> Zenith_SystemBuilder& Zenith_Write(Zenith_SystemBuilder& xBuilder);
template<typename TComponent> constexpr Zenith_QueryTerm Zenith_QueryTermFor(Zenith_QueryTermOp eOp = QUERY_TERM_WITH,
	bool bWrite = false);
template<typename TComponent, typename TField>
void Zenith_SetProperty(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, TField TComponent::* pMember,
	const TField& xValue);
template<typename TComponent>
void Zenith_AddComponent(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, const TComponent& xValue);
```

`Zenith_SetProperty<T>` finds the property ID of `pMember` through `Zenith_ReflectValueTraits<TComponent>::PropertyOf(pMember)` (a `constexpr` table the type's registration header provides) and encodes `xValue` in the property's value encoding. `Zenith_AddComponent<T>` walks `T`'s properties through its registered `Zenith_TypeInfo` and records one `Zenith_PropertyValue` per property, so even a C++ component reaches storage as property values ([Z2-N-ARCH-717]). `Zenith_ColumnView<T>::Get` reads through `m_puRowIndex` when present, so it works on every backend's storage.
