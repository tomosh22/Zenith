# Zenith2 Scripting — transaction data structures

Companion to `Zenith2/Docs/Scripting.md` (its sections "Component writes shadows and side-effect buffers" and "Commit and failure semantics" specify the algorithms over these structures). Normative bullets share the `SCRIPT` numbering of the Scripting document family.

## Transaction data structures

```cpp
// Zenith2/Engine/ZenithScript/Internal/Zenith_ScriptTransaction.h
#pragma once
#include "ZenithECS/Zenith_ChunkView.h"             // Zenith_RawColumn
// Zenith_ContainerRef: the ZenithECS container reference of Architecture.md → "Data-defined components"
#include "ZenithCore/Zenith_Arena.h"               // Zenith_ArenaMarker
#include "ZenithScript/Zenith_ScriptTypes.h"

// Zenith_ScriptInvocationID and Zenith_ScriptLogicalID are public: Scripting/PublicTypes.md → "Public types".

enum Zenith_EntityTxnState : uint8
{
	ENTITY_TXN_STATE_OPEN,
	ENTITY_TXN_STATE_SUCCEEDED,       // returned or yielded without error
	ENTITY_TXN_STATE_FAILED,          // raised; rows restored, sub-buffer discarded at failure time
	ENTITY_TXN_STATE_ACCEPTED,        // passed merge-time capacity
	ENTITY_TXN_STATE_REJECTED,        // failed merge-time capacity; rows restored at merge
	ENTITY_TXN_STATE_COMMITTED,
};

// One declared-writable property column of one chunk range, captured on the invocation's first write to that chunk.
// Change bits are not captured: writes set pending bits in the invocation's private set (Zenith_RawColumn::m_pulRowChangeBits),
// which a restore clears for the failing row (Zenith2/Docs/Architecture.md → "ECS model").
struct Zenith_ColumnSnapshot
{
	uint32 m_uChunkOrdinal = 0;           // the chunk's ordinal in the invocation's query (Zenith_SystemContext::GetChunk)
	Zenith_TypeID m_xType;                 // the component type
	Zenith_PropertyID m_xProperty;         // property granularity: the column is one reflected property
	Zenith_RawColumn m_xColumn;            // recorded at capture; used for restores until the merge point
	uint32 m_uFirstRow = 0;               // first row of the invocation's range; the entity's row for behaviours
	uint32 m_uRowCount = 0;               // rows captured
	uint32 m_uFieldBytes = 0;             // bytes of the property per row
	uint8* m_puRowBytes = nullptr;        // m_uRowCount * m_uFieldBytes, gathered through m_xColumn
};

// Private side effects of one entity transaction (the "sub-buffer").
struct Zenith_SideEffectBuffer
{
	uint32 m_uFirstRecord = 0;            // range in the transaction's record array
	uint32 m_uRecordCount = 0;
	uint32 m_uPayloadBytes = 0;           // bytes in the transaction's payload arena
	uint32 m_uNextSequence = 0;           // sequence number of the next record: the third merge key
	Zenith_ScriptCapacityDemand m_xDemand; // per capacity class, accumulated as records are appended
};

// Zenith_SideEffectKind is declared with the binding ABI in ZenithReflect/Zenith_ScriptBinding.h
// (Scripting/Bindings.md → "Binding registration and the thunk ABI").

struct Zenith_SideEffectRecord
{
	Zenith_SideEffectKind m_eKind;
	uint32 m_uSequence = 0;
	uint32 m_uPayloadOffset = 0;          // into the transaction's payload arena
	uint32 m_uPayloadBytes = 0;
};

// A private copy of one out-of-line field (container or string) of one row.
struct Zenith_CowShadow
{
	uint32 m_uChunkOrdinal = 0;
	uint32 m_uRow = 0;
	Zenith_TypeID m_xType;
	Zenith_PropertyID m_xProperty;
	Zenith_ContainerRef m_xCommitted;      // the committed out-of-line storage the shadow was copied from
	void* m_pShadowData = nullptr;         // private copy in the transaction arena
	uint32 m_uShadowCount = 0;            // elements (bytes for strings)
	uint32 m_uShadowCapacityBytes = 0;    // reserved during the invocation
	bool m_bDirty = false;
};

struct Zenith_EntityTransaction
{
	uint32 m_uRow = 0;
	Zenith_ScriptLogicalID m_xLogicalID;
	Zenith_EntityTxnState m_eState = ENTITY_TXN_STATE_OPEN;
	Zenith_SideEffectBuffer m_xSideEffects;
	uint32 m_uFirstShadow = 0;
	uint32 m_uShadowCount = 0;
	uint32 m_uMemoryBytes = 0;            // per-entity cap accounting (see "Per-entity memory accounting")
	uint64 m_ulSafepoints = 0;
	uint32 m_uErrorIndex = 0;             // into the transaction's error table when FAILED or REJECTED
};

class Zenith_ScriptTransaction
{
public:
	Zenith_ScriptInvocationID m_xInvocationID;
	Zenith_ScriptSystemRef m_xSystem;              // system order + declaration
	Zenith_ArenaMarker m_xArenaMarker;             // the slot's scratch-arena marker at invocation start; the transaction itself and
	                                               // everything below live in the retention arena its system claimed on the slot
	Zenith_Span<Zenith_ColumnSnapshot> m_axSnapshots;
	Zenith_Span<Zenith_EntityTransaction> m_axEntities;
	Zenith_Span<Zenith_SideEffectRecord> m_axRecords;
	Zenith_Span<Zenith_CowShadow> m_axShadows;
	Zenith_Span<uint8> m_axPayload;
	bool m_bChunkSnapshotTaken = false;            // one chunk range per chunk-system invocation
};
```

- [Z2-N-SCRIPT-861] Every structure in this file is trivially destructible and lives in a transaction arena (a slot's scratch arena or a system's retention arena, `Zenith2/Docs/Scripting.md` → "Component writes shadows and side-effect buffers"), so resetting the arena (`ResetToMarker` at the invocation's end, `Reset()` at the system's merge) releases it with no destructor calls.
- [Z2-N-SCRIPT-862] `Zenith_ScriptInvocationID` values are never reused within a world session: the per-VM counter is 48 bits wide and a VM that would wrap it halts the world with a fatal diagnostic.
