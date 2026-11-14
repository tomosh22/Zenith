# ECS design — the ZenithECS system and scheduling API

This file is part of the ECS design family; the entry document is `Zenith2/Docs/Design/ECS.md`. It declares the four public headers of `Zenith2/Engine/ZenithECS/` through which systems are registered, run and merged — `Zenith_System.h`, `Zenith_SystemContext.h`, `Zenith_MergeContext.h` and `Zenith_ScheduleErrors.h` — with their contract blocks, in Zenith Hungarian. It is split from `Zenith2/Docs/Design/ECS/WorldAPI.md`, which declares every other ZenithECS header and whose "Header list" names all of them; the conventions of that file's preamble (tabs, contract blocks, include rules, `<!-- elaboration -->` markers listed in `Zenith2/Docs/Design/ECS.md` → "Reconciliation items") apply here unchanged. The specifications it elaborates are `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder", `Zenith2/Docs/Architecture/ECSModel.md` → "Scheduling contract" and `Zenith2/Docs/Architecture/FramePhases.md` → "Schedule-compile errors"; the algorithms behind these declarations are `Zenith2/Docs/Design/ECS/Scheduling.md`.

A member whose comment says `consumer seam` is declared by the slice that implements it, never by the API-only slices (`Zenith2/Docs/Design/ECS.md` → "API-only slices").

## Scope of this file

| Header (`Zenith2/Engine/ZenithECS/`) | Heading here | Declares |
|---|---|---|
| `Zenith_System.h` | § "Zenith_System header" | `Zenith_SystemKind`, `Zenith_PfnSystemRun`, `Zenith_PfnSystemMerge`, `Zenith_SystemDesc`, `Zenith_EpilogueStageDesc`, `Zenith_ReductionKind`, `Zenith_ReductionDesc`, `Zenith_ReductionSlot`, `Zenith_SystemBuilder`, `Zenith_SystemRegistryDesc`, `Zenith_SystemReloadScope`, `Zenith_SystemRegistry` |
| `Zenith_SystemContext.h` | § "Zenith_SystemContext header" | `Zenith_EventWriter`, `Zenith_EventRecordView`, `Zenith_EventReader`, `Zenith_ContributionWriter`, `Zenith_SystemContext` |
| `Zenith_MergeContext.h` | § "Zenith_MergeContext header" | `Zenith_ECSCapacityClass`, `Zenith_ECSCapacityDemand`, `Zenith_MergeCapacity`, `Zenith_MergeContext` |
| `Zenith_ScheduleErrors.h` | § "Zenith_ScheduleErrors header" | `Zenith_ScheduleErrorCode`, `Zenith_ScheduleDiagnostic`, `Zenith_ScheduleError`, `Zenith_ToString(Zenith_ScheduleErrorCode)` |

## Zenith_System header

`Zenith_SystemDesc`, `Zenith_SystemBuilder` and `Zenith_PfnSystemRun` are `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame graph builder" (declarations at the ECS level); `Zenith_SystemRegistry` is `Zenith2/Docs/Architecture/ECSModel.md` → "Scheduling contract"; `Zenith_EpilogueStageDesc` is `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", declared here because `Zenith_SystemRegistry::AddEpilogueStage` takes it.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_System.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_Phase.h"
#include "ZenithReflect/Zenith_Arbitration.h"      // Zenith_WriterSource
#include "ZenithJobs/Zenith_JobTypes.h"            // Zenith_JobAffinity
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_ScheduleErrors.h"

class Zenith_SystemContext;
class Zenith_MergeContext;
class Zenith_TypeRegistry;
class Zenith_CommandKindTable;
class Zenith_Heap;
class Zenith_FaultInjector;
struct Zenith_FrameGraphDiagnostic;        // ZenithJobs/Zenith_JobsErrors.h

/// @contract enum Zenith_SystemKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_SystemKind : uint8
{
	SYSTEM_KIND_NATIVE,              // a C++ system: FRAME_NODE_KIND_SYSTEM
	SYSTEM_KIND_SCRIPT_CHUNK,        // a Luau chunk system: FRAME_NODE_KIND_SYSTEM, phases 2 and 10 in v1
	SYSTEM_KIND_SCRIPT_BEHAVIOURS,   // the behaviours of one module in one script phase: FRAME_NODE_KIND_BEHAVIOURS, main thread
	SYSTEM_KIND_EPILOGUE_STAGE,      // added by AddEpilogueStage: FRAME_NODE_KIND_STAGE in EPILOGUE stage 12
};

/// @contract functions Zenith_SystemFunctionTypes
/// Ownership: Function pointers registered with a system.
/// Thread affinity: A run function runs on its node's thread (and its sub-jobs through ForLeaf); a merge function runs on
///   the system's merge node, one merge node of a world's phase at a time, in the stage's merge-chain order
///   (Scheduling.md, "Merge nodes and the merge chain").
/// Invalidation: None.
/// Units: None.
/// Errors: Neither may assert on script or content data; failures are recorded through the context.
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnSystemRun = void (*)(Zenith_SystemContext& xContext);
// elaboration: a custom merge (SCRIPT's merge point); null selects the ECS default merge
using Zenith_PfnSystemMerge = void (*)(Zenith_MergeContext& xContext);

/// @contract value Zenith_SystemDesc
/// Ownership: Plain value; m_szName copied at AddSystem; m_pUserData owned by the registrant, outliving the world.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct the desc and set its fields before
///   AddSystem.
/// Invalidation: None.
/// Units: m_uCostHint in microseconds (at least 1).
/// Errors: Checked at Freeze with the schedule errors of Zenith_ScheduleErrors.h.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SystemDesc
{
	const char* m_szName = nullptr;               // "<Module>.<System>", unique per world, no '@'
	Zenith_Phase m_ePhase = PHASE_NONE;
	Zenith_PfnSystemRun m_pfnRun = nullptr;
	void* m_pUserData = nullptr;
	Zenith_JobAffinity m_eAffinity = JOB_AFFINITY_ANY;
	Zenith_WriterSource m_eWriterSource = WRITER_SOURCE_NONE;
	uint32 m_uCostHint = 1;
	// elaboration
	Zenith_SystemKind m_eSystemKind = SYSTEM_KIND_NATIVE;
	Zenith_PfnSystemMerge m_pfnMerge = nullptr;
};

/// @contract value Zenith_EpilogueStageDesc
/// Ownership: Plain value; m_szName copied; m_pUserData owned by the registrant, outliving the world.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct the desc and set its fields before
///   AddEpilogueStage.
/// Invalidation: None.
/// Units: m_uCostHint in microseconds.
/// Errors: Checked at Freeze.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EpilogueStageDesc
{
	const char* m_szName = nullptr;        // "<Module>.<Stage>", unique per world
	Zenith_PfnSystemRun m_pfnRun = nullptr;
	void* m_pUserData = nullptr;
	uint32 m_uCostHint = 1;
	// elaboration: set by Zenith_WorldAttachContext::AddExtractor; EmitInto orders the node before Render.Seal
	bool m_bRenderExtractor = false;
};

/// @contract enum Zenith_ReductionKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The kinds and combine operations of Zenith2/Docs/Scripting.md → "Fixed-order reduction tree".
/// @endcontract
enum Zenith_ReductionKind : uint8
{
	REDUCTION_KIND_SUM_F64,        // + in f64
	REDUCTION_KIND_SUM_I64,        // wrapping + in i64
	REDUCTION_KIND_MIN,            // min in f64, NaN propagating
	REDUCTION_KIND_MAX,            // max in f64, NaN propagating
	REDUCTION_KIND_COUNT,          // wrapping + in i64 of 1 per contribution
};

/// @contract value Zenith_ReductionDesc
/// Ownership: Plain value; m_szName copied.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A target that is not an f64, i64 or u64 property is SCHEDULE_ERROR_UNKNOWN_PROPERTY at Freeze; the target
///   property is treated as a direct write of the system, so authority applies ([Z2-N-SCRIPT-184]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReductionDesc
{
	const char* const m_szName = nullptr;
	const Zenith_TypeID m_xTargetType = {};    // a component carried by exactly one entity (a singleton)
	const Zenith_PropertyID m_xTargetProperty = {};
	const Zenith_ReductionKind m_eKind = REDUCTION_KIND_SUM_F64;
};

/// @contract value Zenith_ReductionSlot
/// Ownership: Plain value filled by Zenith_SystemBuilder::Reduction.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of Zenith_SystemBuilder::Reduction
///   and reassigned as a member.
/// Invalidation: Valid for its registry.
/// Units: Index into the registry's reduction table.
/// Errors: An unbound slot is ZENITH_ASSERT.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReductionSlot { uint32 m_uIndex = 0xFFFFFFFFu; };

/// @contract type Zenith_SystemBuilder
/// Ownership: Owned by its Zenith_SystemRegistry; returned by reference from AddSystem and AddEpilogueStage.
/// Thread affinity: OWNER_THREAD (main thread), valid until Zenith_SystemRegistry::Freeze.
/// Invalidation: Invalid after Freeze and after the next AddSystem call on the same registry.
/// Units: None.
/// Errors: Every declaration is recorded; problems are reported by Freeze as schedule diagnostics, never by the call.
///   The inert builder AddSystem returns for a rejected registration records nothing.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SystemBuilder
{
public:
	Zenith_SystemBuilder& Read(Zenith_TypeID xType);
	Zenith_SystemBuilder& Read(Zenith_TypeID xType, Zenith_Span<const Zenith_PropertyID> axProperties);
	Zenith_SystemBuilder& Write(Zenith_TypeID xType);
	Zenith_SystemBuilder& Write(Zenith_TypeID xType, Zenith_Span<const Zenith_PropertyID> axProperties);
	Zenith_SystemBuilder& ReadResource(Zenith_TypeID xResourceType);
	Zenith_SystemBuilder& WriteResource(Zenith_TypeID xResourceType);
	Zenith_SystemBuilder& EmitsCommands();
	Zenith_SystemBuilder& EmitsEvent(Zenith_TypeID xEventType);
	Zenith_SystemBuilder& Before(const char* szSystemName);
	Zenith_SystemBuilder& After(const char* szSystemName);
	Zenith_SystemBuilder& BeforeIfPresent(const char* szSystemName);
	Zenith_SystemBuilder& AfterIfPresent(const char* szSystemName);
	Zenith_SystemBuilder& Query(const Zenith_QueryDesc& xQuery, Zenith_QuerySlot& xOutSlot);
	// elaboration
	Zenith_SystemBuilder& ReadAnyRow(Zenith_TypeID xType, Zenith_Span<const Zenith_PropertyID> axProperties);   // cross-entity reads
	Zenith_SystemBuilder& ReadsEvent(Zenith_TypeID xEventType);
	Zenith_SystemBuilder& Reduction(const Zenith_ReductionDesc& xDesc, Zenith_ReductionSlot& xOutSlot);
	Zenith_SystemBuilder& ConsumesMergeCapacity();    // forces a merge node although no command or event is declared
};

/// @contract value Zenith_SystemRegistryDesc
/// Ownership: Plain value; the pointers are borrowed and outlive the registry.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A null registry, heap or kind table, an unsealed kind table or an unfrozen type registry is ECS_ERROR_INVALID_DESC,
///   and so is m_uMaxSystems outside 1 … 65,536: with Freeze's own merge-point check it keeps a tick's merge points
///   below 2^20 (Queries.md, "Change versions and merge versions" item 1).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_SystemRegistryDesc
{
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;
	const Zenith_CommandKindTable* const m_pxCommandKinds = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
	const uint16 m_uWorldTag = 0;
	const uint32 m_uMaxSystems = 4096;        // 1 … 65,536
	Zenith_FaultInjector* const m_pxFaults = nullptr;
};

/// @contract type Zenith_SystemReloadScope
/// Ownership: Returned by value from Zenith_SystemRegistry::BeginReload; borrows the registry.
/// Thread affinity: MAIN_THREAD, frame-prologue steps 6 to 8 only.
/// Invalidation: Invalid after Commit or Abandon.
/// Units: None.
/// Errors: Commit returns the Freeze result of the changed registry; a failed Commit leaves the previous declarations in
///   effect ([Z2-N-ARCH-409]). ReplaceSystem or AddSystem with a phase that is not schedulable (the rule of
///   Zenith_SystemRegistry::AddSystem) is ZENITH_CHECK: only SCRIPT opens reload scopes and it passes phases 2 and 10 alone.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SystemReloadScope
{
public:
	Zenith_SystemBuilder& ReplaceSystem(const Zenith_SystemDesc& xDesc);   // keeps the index of the same name
	void RemoveSystem(const char* szSystemName);                                          // leaves a tombstone: index kept
	Zenith_SystemBuilder& AddSystem(const Zenith_SystemDesc& xDesc);       // appended
	Zenith_Result<void, Zenith_ScheduleError> Commit();
	void Abandon();
};

/// @contract type Zenith_SystemRegistry
/// Ownership: Created by Create as a Zenith_Owned<Zenith_SystemRegistry>; moved into its Zenith_WorldSimulation, which
///   owns it for the world's lifetime; storage from the desc's heap.
/// Thread affinity: MAIN_THREAD. Registrations are accepted until Freeze, and again only through a Zenith_SystemReloadScope.
/// Invalidation: Builders are invalid after Freeze; slots and registration indices stay valid for the world.
/// Units: Registration indices (the merge-key system order).
/// Errors: Freeze returns Zenith_ScheduleError carrying the first diagnostic's code and the count; the diagnostics are
///   GetDiagnostics. AddSystem after Freeze outside a reload scope is ZENITH_CHECK. AddSystem with m_ePhase
///   PHASE_RENDER_EXTRACTION, PHASE_COMMANDS_ONLY or any value above PHASE_SAVE_SNAPSHOT is rejected at the call: no
///   system record and no registration index are created, the rejection is logged under ecs.schedule
///   ("schedule: system '<S>' declares phase <P>, which no tick stage runs; phase-12 work registers through
///   AddEpilogueStage or AddExtractor") and kept as Zenith_ECSError { ECS_ERROR_PHASE_NOT_SCHEDULABLE } in
///   GetRejectedRegistrations, and the returned builder is the registry's inert builder, which discards every declaration
///   chained on it. PHASE_NONE is not rejected at the call; Freeze reports it as SCHEDULE_ERROR_PHASE_NONE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SystemRegistry
{
public:
	// elaboration
	static Zenith_Result<Zenith_Owned<Zenith_SystemRegistry>, Zenith_ECSError> Create(const Zenith_SystemRegistryDesc& xDesc);

	Zenith_SystemBuilder& AddSystem(const Zenith_SystemDesc& xDesc);
	Zenith_SystemBuilder& AddEpilogueStage(const Zenith_EpilogueStageDesc& xDesc);
	Zenith_Result<void, Zenith_ScheduleError> Freeze();
	Zenith_Span<const Zenith_ScheduleDiagnostic> GetDiagnostics() const;
	uint32 GetSystemCount() const;
	uint32 GetRegistrationIndex(const char* szSystemName) const;
	void SetEnabled(uint32 uRegistrationIndex, bool bEnabled);

	// elaboration
	void AddWorldResource(Zenith_TypeID xResourceType, Zenith_Phase eOwningPhase, void* pResource);
	bool IsFrozen() const;
	bool IsEnabled(uint32 uRegistrationIndex) const;
	Zenith_SystemReloadScope BeginReload();
	uint64 GetDeclarationHash() const;   // over every declaration; a schedule-cache key input
	void RecordGraphFailure(Zenith_Span<const Zenith_FrameGraphDiagnostic> axGraphDiagnostics);   // SCHEDULE_ERROR_GRAPH entries
	// Registrations AddSystem rejected at the call (ECS_ERROR_PHASE_NOT_SCHEDULABLE), in call order; the composition root
	// compares the count before and after each module's AttachWorld and fails that module's attach when it grew.
	Zenith_Span<const Zenith_ECSError> GetRejectedRegistrations() const;
};
```

<!-- elaboration -->
`AddWorldResource(xType, eOwningPhase, pResource)` names the resource instance a world owns; the owning phase is declared once, by the resource type's registration (`Zenith_TypeBuilder::ResourceOwningPhase`, `Zenith2/Docs/Design/ECS/Reflect.md` → "Freeze validation" rule 4), and `eOwningPhase` restates it: `Freeze` reports an `eOwningPhase` different from the type's `m_eResourceOwningPhase` as `SCHEDULE_ERROR_RESOURCE_AUTHORITY` with the message `schedule: world resource '<Resource>' is added with owning phase <Q>, but its type declares <P>`, and the authority checks read the type's phase alone (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Check order" step 5).

## Zenith_SystemContext header

`Zenith_SystemContext` is `Zenith2/Docs/Architecture/ECSModel.md` → "Scheduling contract" with the additions marked; the leaf form answers cross-area note J11: a sub-job allocates from its own job context's arena, never through the node's context.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_SystemContext.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_ChunkView.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithECS/Zenith_System.h"

class Zenith_JobContext;
class Zenith_Arena;

/// @contract type Zenith_EventWriter
/// Ownership: Owned by its system context; one per logical invocation; records ECS_RECORD_EVENT into that invocation's
///   command buffer.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With its context.
/// Units: None.
/// Errors: Emitting an undeclared event type is ZENITH_ASSERT and records nothing; value errors are found at merge.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EventWriter
{
public:
	void Emit(Zenith_TypeID xEventType, Zenith_Span<const Zenith_PropertyValue> axValues);
};

/// @contract value Zenith_EventRecordView
/// Ownership: A borrowed view of one delivered event in the world's event queue.
/// Thread affinity: IMMUTABLE for the phase it was obtained in.
/// Invalidation: Invalid after the end of the tick.
/// Units: m_pPayload in the event type's inline layout (Reflect.md, "Zenith_TypeLayout header").
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EventRecordView
{
	const Zenith_TypeID m_xEventType = {};
	const Zenith_MergeKey m_xKey = {};  // the emitting record's merge key; delivery order is merge order
	const void* const m_pPayload = nullptr;
};

/// @contract type Zenith_EventReader
/// Ownership: A borrowed range over the delivery queue of one event type.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With its context.
/// Units: Events.
/// Errors: Reading an event type the system did not declare with ReadsEvent is ZENITH_ASSERT and yields an empty range.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EventReader
{
public:
	uint32 GetCount() const;
	Zenith_EventRecordView Get(uint32 uIndex) const;
};

/// @contract type Zenith_ContributionWriter
/// Ownership: Owned by its system context; one per logical invocation.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: With its context.
/// Units: A contribution is an f64 or i64 value (8 bytes) in its reduction's kind.
/// Errors: An undeclared reduction target, an ambiguous target type, or a byte count other than 8 is ZENITH_ASSERT and
///   records nothing.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ContributionWriter
{
public:
	void Contribute(Zenith_TypeID xReductionTarget, Zenith_ByteSpan xContribution);   // the one reduction onto that type
	// elaboration
	void Contribute(const Zenith_ReductionSlot& xReduction, double dValue);
	void Contribute(const Zenith_ReductionSlot& xReduction, sint64 iValue);
};

/// @contract type Zenith_SystemContext
/// Ownership: Created by the ECS node trampoline on the node's stack for one system invocation, or by ForLeaf for one
///   sub-job; neither copied nor stored ([Z2-N-ARCH-718]). Scratch, pending change bits and command buffers come from the
///   node arena of the job context it was created for.
/// Thread affinity: OWNER_THREAD — valid only for the invocation (or leaf) it was created for; every accessor grants exactly
///   what the system's registration declared.
/// Invalidation: Every view, buffer and reader it returned is invalid after the invocation's merge completes.
/// Units: Ticks; microseconds (GetTickDurationUs).
/// Errors: An undeclared access is ZENITH_ASSERT where asserts are enabled and is rejected in every build: GetResource
///   returns null, GetColumn of an undeclared property returns a null column ([Z2-N-ARCH-399], [Z2-N-ARCH-719]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_SystemContext
{
public:
	Zenith_SystemContext(const Zenith_SystemContext&) = delete;
	Zenith_SystemContext& operator=(const Zenith_SystemContext&) = delete;
	Zenith_SystemContext(Zenith_SystemContext&&) = default;          // returned by ForLeaf, never stored

	Zenith_JobContext& Jobs();
	uint16 GetWorldTag() const;
	uint64 GetTickIndex() const;
	uint64 GetTickDurationUs() const;
	Zenith_InvocationScope GetScope() const;
	uint32 GetChunkCount(const Zenith_QuerySlot& xQuery) const;
	Zenith_ChunkView GetChunk(const Zenith_QuerySlot& xQuery, uint32 uChunkOrdinal);
	void* GetResource(Zenith_TypeID xResourceType);
	template<typename T> T* Resource() { return static_cast<T*>(GetResource(T::xTYPE_ID)); }
	Zenith_CommandBuffer& Commands(uint64 ulSimulationID);
	Zenith_EventWriter& Events(uint64 ulSimulationID);
	Zenith_ContributionWriter& Contributions(uint64 ulSimulationID);
	Zenith_Arena& GetScratch();

	// elaboration
	Zenith_SystemContext ForLeaf(Zenith_JobContext& xLeafJobs) const;      // a sub-job's own context
	Zenith_CommandBuffer& Commands(uint64 ulSimulationID, uint32 uInvocationOrdinal);
	// consumer seam: the event writer of the logical invocation (ulSimulationID, uInvocationOrdinal), recording into the buffer
	// Commands(ulSimulationID, uInvocationOrdinal) returns, so its events are accepted or rejected with that buffer alone;
	// Events(ulSimulationID) is Events(ulSimulationID, 0). ulSimulationID 0 names no entity (§ "The current logical invocation").
	Zenith_EventWriter& Events(uint64 ulSimulationID, uint32 uInvocationOrdinal);
	Zenith_ChunkView GetChunkInScope(const Zenith_QuerySlot& xQuery, uint32 uChunkOrdinal,
		const Zenith_InvocationScope& xScope);                                              // a script invocation's own scope
	Zenith_Result<Zenith_ChunkView, Zenith_ECSError> GetChunkOfEntity(const Zenith_QuerySlot& xQuery,
		Zenith_EntityHandle xEntity, uint32& uRowOut);
	Zenith_EventReader ReadEvents(Zenith_TypeID xEventType) const;
	Zenith_Phase GetPhase() const;
	uint64 GetWorldSeed() const;
	uint32 GetSystemOrder() const;
	const Zenith_TypeRegistry& GetTypes() const;
	Zenith_Result<Zenith_EntityLocation, Zenith_ECSError> Resolve(Zenith_EntityHandle xEntity) const;
	// The read version of this system's previous completed invocation (the last-seen version its query slots hold): 0 in
	// its first invocation, after RestoreSimulationState and after the version wrap sweep reset it (Queries.md,
	// "Change-filtered queries"). A row-level change filter compares Zenith_ChunkView::GetRowChangeVersions against it.
	uint32 GetPreviousReadVersion() const;

private:
	Zenith_SystemContext() = default;
	friend class Zenith_WorldSimulation;
	void* m_pInvocation = nullptr;        // the invocation record (ZenithECS/Internal); shared by the node and its leaves
	Zenith_JobContext* m_pxJobs = nullptr; // the job context whose arenas this context allocates from
};
```

`GetChunk`, `GetChunkInScope` and `GetChunkOfEntity` obtain the backend's commit-disabled view and stamp it before returning it: the invocation's merge version, the scope (the node's, or for `GetChunkInScope` the script invocation's), commit enabled, `m_bWritable` per declared write, and pending bits and staging weights from this context's arena (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ChunkView header", table "Merge version stamping"). `ForLeaf` is the only way a sub-job obtains a context: the sub-job body receives the node's context pointer through its job data ([Z2-N-ARCH-718]) and calls `pxNode->ForLeaf(xLeafJobs)`; the returned context shares the invocation's immutable declaration state and allocates scratch, change bits, buffers and event records from `xLeafJobs.GetNodeArena()` (`Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobContext"). The invocation keeps a lock-free list of every buffer and writable view each leaf created, so its merge finds them (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Commit and collection").

<!-- elaboration -->
`GetPreviousReadVersion` returns the previous read version P the invocation record captured at its start (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Disabled systems" step 2), the same value in the node and in every leaf. It is the supported way to filter rows by change: a row whose `GetRowChangeVersions` entry is newer than P under `Zenith_IsVersionNewer`, or every row when P is 0, is a row committed since the previous invocation read. The system contract has two clauses that go with it (`Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"): a system's committed result does not depend on whether an unchanged row or chunk was handed to it, because `QUERY_TERM_CHANGED` and this filter both over-match after `RestoreSimulationState` and after a sweep reset; and a system keeps no last-seen version of its own across invocations, because ECS resets P and the query slots at a restore and a sweep and cannot reach a copy. A system that nonetheless keeps one must treat `GetPreviousReadVersion() == 0` as "reset" and discard its copy in that invocation.

### The current logical invocation

<!-- elaboration -->
A logical invocation is one (simulation ID, invocation ordinal) pair of a system invocation: it owns one command buffer with base key `{ system order, simulation ID, ordinal, 0 }`, into which its commands, events and contributions record, and which its merge accepts or rejects as one unit (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Recording"). `Commands(sim)`, `Commands(sim, ordinal)`, `Events(sim)`, `Events(sim, ordinal)` and `Contributions(sim)` each **open** the logical invocation they name (creating its buffer on first use), and `Events(sim, ordinal)` lets one system emit events into several independently accepted buffers, for example one per group of events, so that a merge-capacity rejection of one group loses no other.

Each context — a node's, and each leaf's from `ForLeaf` — keeps one **current simulation ID** C, which the own-row rule reads (`Zenith2/Docs/Design/ECS/Queries.md` → "Own-row and any-row reads" step 3):

1. C is 0 (no current entity) when the context is created; a leaf context starts at 0 whatever its parent's C.
2. Every call that opens a logical invocation — the five calls above, any ordinal — sets C to its simulation ID. Simulation ID 0 names no entity, so `Events(0, ordinal)` or `Commands(0)` makes no entity current.
3. A `GetChunkOfEntity` or `GetChunkOfEntityInScope` call while C is 0 first sets C to the simulation ID of the entity it resolves (a behaviour reaching its own row first), then applies the rule.
4. Nothing else changes C: a view, a column read, an event read or a merge leaves it.

So the entity current for `GetChunkOfEntity` is the one most recently opened through `Commands` or `Events` (or `Contributions`), and a system writing another entity's own-row-only column through random access opens that entity's logical invocation first — `Commands(sim, 0)`, even when it records nothing, or `Events(sim)` in a system that declares no `EmitsCommands` (opening a writer records nothing and never asserts; only `Emit` of an undeclared event type does) — then gets that entity's own-row-only columns non-null. C changes no merge key, no buffer and no recorded byte.

## Zenith_MergeContext header

<!-- elaboration -->
The context of a system's merge: the ECS default merge of a C++ system, or the custom merge a script system registers (`Zenith2/Docs/Scripting.md` → "Merge point algorithm" runs inside it). It carries the phase's shared ECS capacity and the commit operations.

```cpp
// Zenith2/Engine/ZenithECS/Zenith_MergeContext.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithECS/Zenith_ECSTypes.h"
#include "ZenithECS/Zenith_ChunkView.h"
#include "ZenithECS/Zenith_CommandBuffer.h"

class Zenith_JobContext;
class Zenith_Arena;

/// @contract enum Zenith_ECSCapacityClass
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Entities; records; bytes (CONTAINER_BYTES).
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ECSCapacityClass : uint8
{
	ECS_CAPACITY_CLASS_ENTITY_SLOTS,       // Z2-B-ECS-003 and the world's free slots
	ECS_CAPACITY_CLASS_COMMAND_RECORDS,    // Z2-B-ECS-004
	ECS_CAPACITY_CLASS_EVENT_RECORDS,      // Z2-B-GAME-003
	ECS_CAPACITY_CLASS_CONTAINER_BYTES,    // Z2-B-SCRIPT-016 and the container heap's free bytes
	ECS_CAPACITY_CLASS_COUNT,
};

/// @contract value Zenith_ECSCapacityDemand
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: the demand is summed into m_auCount system by system.
/// Invalidation: None.
/// Units: Per class, as Zenith_ECSCapacityClass.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ECSCapacityDemand
{
	uint32 m_auCount[ECS_CAPACITY_CLASS_COUNT] = {};
};

/// @contract type Zenith_MergeCapacity
/// Ownership: Owned by the world simulation; one per world; reset from committed state at each phase start.
/// Thread affinity: OWNER_THREAD — the merge node holding the phase's merge chain, or the sync-point node.
/// Invalidation: None.
/// Units: As Zenith_ECSCapacityClass.
/// Errors: TryAccept returns false and changes nothing when any class does not fit (first-fit, [Z2-N-SCRIPT-333]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_MergeCapacity
{
public:
	uint32 GetRemaining(Zenith_ECSCapacityClass eClass) const;
	bool TryAccept(const Zenith_ECSCapacityDemand& xDemand);
};

/// @contract type Zenith_MergeContext
/// Ownership: Created by the ECS merge node trampoline for one system merge; neither copied nor stored.
/// Thread affinity: OWNER_THREAD — the system's merge node, after every sub-job and continuation of the system node has
///   finished; one merge of a world's phase runs at a time, in the stage's merge-chain order.
/// Invalidation: Every span it returns is invalid when the merge returns.
/// Units: None.
/// Errors: AcceptBuffer of a buffer another invocation recorded, or a second accept of one buffer, is ZENITH_ASSERT.
///   CommitRowChanges returns the view's errors. Committing cannot fail: capacity was accepted before it.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_MergeContext
{
public:
	Zenith_JobContext& Jobs();
	Zenith_InvocationScope GetSystemScope() const;           // the system node's invocation scope
	uint32 GetMergeVersion() const;
	// elaboration: V(t, R(S)), the version of the merge point immediately preceding the system's node in its stage's order
	// (Queries.md, "Stage order of merge points"): every commit of a property the system declares a read of that the
	// schedule ran before the node is at or below it, every such commit it ran after the node above it (a commit of a
	// property the system does not read may fall on either side). The system's query slots take it as their next
	// last-seen version and its next invocation reads it back through Zenith_SystemContext::GetPreviousReadVersion; a
	// row-level filter (SCENE's transform propagation) uses that, never a copy of its own.
	uint32 GetReadVersion() const;
	Zenith_MergeCapacity& GetCapacity();
	Zenith_Arena& GetScratch();
	void* GetUserData() const;                                // the system's m_pUserData

	// The invocation's recorded material, buffers in ascending base key.
	Zenith_Span<Zenith_CommandBuffer* const> GetCommandBuffers() const;
	Zenith_Span<Zenith_ChunkView> GetWrittenViews();
	Zenith_ECSCapacityDemand ComputeDemand(const Zenith_CommandBuffer& xBuffer) const;

	void AcceptBuffer(Zenith_CommandBuffer& xBuffer);   // appended to the phase's command and event queues in merge order
	void RejectBuffer(Zenith_CommandBuffer& xBuffer);   // discarded; counted in ecs.capacity_rejections_delta
	Zenith_Result<void, Zenith_ECSError> CommitRowChanges(Zenith_ChunkView& xView, Zenith_TypeID xType,
		Zenith_PropertyID xProperty, const uint64* pulAcceptedRowBits);
	void CommitAllWrittenRows();                        // the default: every pending bit of every written view
	void FoldReductions();                              // accepted contributions, merge order, fixed-order tree
};
```

### Merge reject filter

<!-- elaboration -->
A test that must see one system's buffer rejected at merge while another system's buffer in the same phase is accepted cannot use the capacity classes, which are world-wide (A5 cross-area row StB7). `Zenith_SystemTestAccess` (`Zenith2/Engine/ZenithECS/Zenith_SystemTestAccess.h`, test code alone by TestSeparationCheck condition 6) therefore gains a per-registry filter that the default merge consults:

```cpp
// Zenith2/Engine/ZenithECS/Zenith_SystemTestAccess.h additions
// Called by the default merge (Scheduling.md → "Commit and collection", step 2) for each buffer, before TryAccept, on the thread that
// runs the phase's merge chain; true rejects the buffer exactly as a capacity rejection of eClassOut.
using Zenith_PfnMergeRejectFilter = bool (*)(Zenith_StringView strSystemName, const Zenith_MergeKey& xBaseKey,
	Zenith_ECSCapacityClass& eClassOut, void* pUserData);
	static void SetMergeRejectFilter(Zenith_SystemRegistry& xSystems, Zenith_PfnMergeRejectFilter pfnFilter,
		void* pUserData);   // MAIN_THREAD, no frame running; null clears it
```

- The registry holds the filter and its user datum as members; a registry nobody gave a filter calls nothing, so a production world never runs a filter. Nothing about the filter is global.
- The default merge's step 2 asks the filter first. On true it takes the rejected branch with `eClassOut` (`RejectBuffer`, `ecs.capacity_rejections_delta` + 1, the log line naming that class) and does not call `TryAccept`, so the phase's remaining capacity is unchanged for the buffers after it. On false it proceeds unchanged.
- SCRIPT's merge function never calls the filter: script systems reject through their own acceptance.
- `Zenith2/Engine/ZenithECS/Tests/Support/Zenith_ECSMergeRejector.h` (header-only, X184) is the helper other areas' tests use. `struct Zenith_ECSMergeRejector { Zenith_StringView m_strSystemName; uint32 m_uRejectCount = 1; Zenith_ECSCapacityClass m_eClass = ECS_CAPACITY_CLASS_CONTAINER_BYTES; uint32 m_uRejected = 0; };` and its `static bool Filter(…)` reject the next `m_uRejectCount` buffers of the named system and count them in `m_uRejected`. `Install(Zenith_SystemRegistry&)` and `Remove(Zenith_SystemRegistry&)` call `SetMergeRejectFilter`.

| Test | Asserts, on every backend |
|---|---|
| `Test_ECS_MergeRejectFilter_TargetsOneSystem` | two phase-4 C++ systems A and B each record a spawn; with the rejector naming A, A's buffer is rejected (no spawn of A, `ecs.capacity_rejections_delta` 1, the log line names A and `CONTAINER_BYTES`) and B's spawn plays back; the next tick, with the count used up, both spawns play |
| `Test_ECS_MergeRejectFilter_LeavesCapacityUnchanged` | with A rejected by the filter, B's demand equal to the whole remaining entity-slot capacity is still accepted |
| `Test_ECS_MergeRejectFilter_ClearedAndAbsent` | after `Remove`, no buffer is rejected; a registry that never had a filter rejects nothing |

## Zenith_ScheduleErrors header

`Zenith_ScheduleErrorCode`, `Zenith_ScheduleDiagnostic` and `Zenith_ScheduleError` are `Zenith2/Docs/Architecture/FramePhases.md` → "Schedule-compile errors"; three codes are appended, with messages in `Zenith2/Docs/Design/ECS/Scheduling.md` → "Messages".

```cpp
// Zenith2/Engine/ZenithECS/Zenith_ScheduleErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithGraph/Zenith_Graph.h"               // uGRAPH_INVALID_INDEX
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract enum Zenith_ScheduleErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The meaning of each code is its message in Zenith2/Docs/Design/ECS/Scheduling.md → "Messages".
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ScheduleErrorCode : uint8
{
	SCHEDULE_ERROR_AUTHORITY_VIOLATION = 1,
	SCHEDULE_ERROR_DUPLICATE_WRITER,
	SCHEDULE_ERROR_DUPLICATE_WRITER_SOURCE,
	SCHEDULE_ERROR_WRITER_SOURCE_MISSING,
	SCHEDULE_ERROR_COMMANDS_ONLY_WRITE,
	SCHEDULE_ERROR_RESOURCE_AUTHORITY,
	SCHEDULE_ERROR_READ_ONLY_PHASE_WRITE,
	SCHEDULE_ERROR_BEHAVIOUR_PHASE,
	SCHEDULE_ERROR_PHASE_NONE,
	SCHEDULE_ERROR_UNKNOWN_TYPE,
	SCHEDULE_ERROR_UNKNOWN_PROPERTY,
	SCHEDULE_ERROR_DUPLICATE_SYSTEM_NAME,
	SCHEDULE_ERROR_ORDERING_TARGET_MISSING,
	SCHEDULE_ERROR_ORDERING_CONTRADICTS_PHASE,
	SCHEDULE_ERROR_QUERY_ACCESS_MISMATCH,
	SCHEDULE_ERROR_GRAPH,
	// elaboration: appended, values 17 to 19
	SCHEDULE_ERROR_CROSS_ENTITY_READ,          // SCRIPT's SCRIPT_SCHEDULE_CROSS_ENTITY_READ_OF_WRITTEN_COLUMN
	SCHEDULE_ERROR_SCRIPT_PHASE,               // a chunk system outside EarlyScripts and LateScripts in v1 ([Z2-N-ARCH-501])
	SCHEDULE_ERROR_INVALID_NAME,               // an empty name, a name containing '@', or a name above 127 bytes
};

/// @contract value Zenith_ScheduleDiagnostic
/// Ownership: Plain value held in its registry's diagnostic array; m_strText views registry-owned text.
/// Thread affinity: IMMUTABLE. Members non-const: stored in a Zenith_Array that Freeze sorts in place.
/// Invalidation: Invalid when the next Freeze or EmitInto of the same registry starts.
/// Units: Registration indices.
/// Errors: None; it describes one.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ScheduleDiagnostic
{
	Zenith_ScheduleErrorCode m_eCode;
	uint32 m_uSystemA = uGRAPH_INVALID_INDEX;
	uint32 m_uSystemB = uGRAPH_INVALID_INDEX;
	Zenith_TypeID m_xType;
	Zenith_PropertyID m_xProperty;
	uint16 m_uWorldTag = 0;
	Zenith_StringView m_strText;
};

/// @contract value Zenith_ScheduleError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: None.
/// Errors: m_eCode is the first diagnostic's code; the text is in GetDiagnostics.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ScheduleError
{
	Zenith_ScheduleErrorCode m_eCode = SCHEDULE_ERROR_GRAPH;
	uint16 m_uWorldTag = 0;
	uint32 m_uDiagnosticCount = 0;
};
static_assert(sizeof(Zenith_ScheduleError) <= 32);

constexpr const char* Zenith_ToString(Zenith_ScheduleErrorCode eCode);
```

