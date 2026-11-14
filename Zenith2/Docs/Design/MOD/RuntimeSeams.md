# Zenith2 design — MOD runtime seams for modules and the world host

Companion to `Zenith2/Docs/Design/MOD.md`. It holds the runtime seams that modules and GAME's world host call in `ZenithRuntime`: the single module publication sink of orchestrator ruling Q76 (a), the lookups of the world attach context, the start context's registrations, the world-host seams of A3 cross-area row GA5, the test-failure entry, the start-context members ONLINE and PACK need, the call sites PACK's tickets add, and the tool host's access header (A3 cross-area row ED1). Every obligation traces to a `Z2-R-MOD-NNN` of `Zenith2/Docs/Requirements/MOD.md` (Z2-R-MOD-112 to Z2-R-MOD-129). A block `<!-- assumption: pending <AREA> (<X row>) -->` marks a member that a core document does not declare yet; the cross-document item it names carries the core sentence.

## Scope of this file

| Section | Holds | Requirements |
|---|---|---|
| "Module publication sink" | `Zenith_ModulePublicationSink`, its handler interface, the forwarder into SCRIPT's `Zenith_AsyncResultTable`, its binding, lifetime, replay rules and counters; the one declaration point per kind, FULL at allocation, and the mixed kind's prologue allocations | Z2-R-MOD-112 to Z2-R-MOD-116, Z2-R-MOD-124, Z2-R-MOD-125 |
| "World attach context lookups" | `GetPublicationSink`, `FindCommandKind`, `GetWorldControl`, `GetSimulationClock`, `GetWorldPurpose` and the purposed test world | Z2-R-MOD-117 |
| "World host seams" | the runtime state GAME's world host shares, world creation and destruction, the call sites GAME's tickets add, the swap precompile and the test hooks | Z2-R-MOD-119, Z2-R-MOD-120 |
| "Test assertion failures and world shutdown requests" | `Zenith_Runtime::RecordTestAssertionFailure` from any thread, the world-requested shutdown | Z2-R-MOD-118 |
| "Start context registrations" | the runtime's implementation of `RegisterFrameListener`, `AddEngineEpilogueStage`, `SetScriptSideEffectConsumer` and `DeclareScriptAsyncSource`, and the query `GetScriptAsyncCapacity` | Z2-R-MOD-122 |
| "Start context overlay access" | `Zenith_ModuleStartContext::GetProfilerOverlay` | Z2-R-MOD-121 |
| "Start context members for ONLINE and PACK" | `IsRecordingReplay`, `RequestExitAfterStart`, `ProvideEntitlementQuery` | Z2-R-MOD-126, Z2-R-MOD-127 |
| "Start context and attach context members for CINE" | `GetStringTable`, `AddTickBoundaryHandler`, `GetCaptureTickGate` and `Zenith_CaptureTickGate::HoldNextTick` | Z2-R-MOD-117, Z2-R-MOD-122 |
| "Call sites PACK's tickets add" | where PACK's runtime bridge is called from MOD's files, the step-4 unwind, the per-world package record, the step-15 engine registration window and the after-`RunFrame` position | PACK's requirements (`Zenith2/Docs/Requirements/PACK.md`) |
| "Runtime tool access" | `Zenith_RuntimeToolAccess`, its hooks, the legal call positions, its errors | Z2-R-MOD-129 |
| "Runtime test seams for modules" | the contexts' fault injector, the test mount sources of a booted runtime, the base-path override and the running-build-ID override | Z2-R-MOD-123, Z2-R-MOD-128 |
| "Seam tests" | every test of the sections above | all of the above |

SCRIPT's half of the sink — how a result a module parks enters the world's table, the replay adoption of the recorded next completion ID and the publication resolver — is `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table". The sink owns no result storage: every slot, ring entry and counter is the table's.

## Module publication sink

ZenithScript is above no module's dependency edge, so a module cannot name `Zenith_AsyncResultTable` ([Z2-N-ARCH-078]). Every module whose results reach simulation as asynchronous publications (ANIM's bindings and chunk reads, AI's path and navigation loads, PHYS's deferred queries, UI's events, AUDIO's script notifications, CINE's sequence bindings, ONLINE's notifications) uses one per-world object, `Zenith_ModulePublicationSink`, which ZenithRuntime implements over the world's table. One sink serves every module of a world; Z2-D-053 (an `asynchronous result` is visible only at a `tick boundary`, in `completion ID` order) holds because the sink only maps calls onto the table.

### Zenith_ModulePublication header

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_ModulePublication.h (public; includes no ZenithScript header)
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"

class Zenith_TickBoundaryAccess;           // ZenithECS

inline constexpr uint32 uMODULE_PUBLICATION_MAX_RESULT_BYTES = 1024;          // per result of a sink-declared source
inline constexpr uint32 uMODULE_PUBLICATION_MAX_IN_FLIGHT = 65536;            // per sink-declared source
inline constexpr uint64 ulMODULE_PUBLICATION_MAX_ARENA_BYTES = 8ull << 20;    // Σ in-flight × bytes over the world's sink-declared sources

/// @contract value Zenith_ModulePublication
/// Ownership: Plain value; m_xPayload borrows the table's result slot or the replay log's buffer.
/// Thread affinity: IMMUTABLE. Members non-const: held in the result table's reused slots.
/// Invalidation: m_xPayload is valid for the handler call (or until the next PeekRecorded).
/// Units: m_uSourceKind and m_uPublicationKind hold Zenith_AsyncSourceKind and Zenith_PublicationKind values
///   (Zenith2/Docs/Architecture.md → "Multiplayer foundations"; the kind values are Zenith2/Docs/Scripting.md's registry).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ModulePublication
{
	uint64 m_ulCompletionID = 0;
	uint8 m_uSourceKind = 0;
	uint8 m_uPublicationKind = 0;
	uint8 m_bScriptOwned = 0;         // 1: a script request's result; the table keeps it for the script's ticket after the handler
	uint8 m_uReserved = 0;
	uint32 m_uReserved2 = 0;
	Zenith_ByteSpan m_xPayload;
};

// Where a sink-declared source allocates its completion IDs (bit flags of Zenith_ModuleSourceDesc::m_uSites).
enum Zenith_ModuleSourceSite : uint8
{
	MODULE_SOURCE_SITE_MERGE_CHAIN = 1 << 0,   // in a merge function on the world's merge chain (recorded or recomputed kinds)
	MODULE_SOURCE_SITE_PROLOGUE    = 1 << 1,   // on the main thread in frame-loop step 1; recorded publication kinds only
};

/// @contract value Zenith_ModuleSourceDesc
/// Ownership: Plain value, copied by DeclareSource.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None.
/// Units: m_uMaxInFlight in results, m_uMaxResultBytes in bytes.
/// Errors: DeclareSource rejects an out-of-range value as a registration (§ "Source declaration and handler registration").
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ModuleSourceDesc
{
	uint32 m_uMaxInFlight = 0;        // 1 … uMODULE_PUBLICATION_MAX_IN_FLIGHT
	uint32 m_uMaxResultBytes = 0;     // 0 … uMODULE_PUBLICATION_MAX_RESULT_BYTES
	uint8 m_uSites = 0;               // MERGE_CHAIN, or PROLOGUE alone
	uint8 m_auReserved[3] = {};
};

/// @contract enum Zenith_PublicationSinkErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: FULL: the source's in-flight capacity is exhausted and nothing changed; REPLAYING: a prologue allocation under
///   --replay, refused and nothing changed; SITE: the kind is not a sink-declared source of this world or the calling site is
///   not one it declared (an engine defect, also ZENITH_ASSERT); DETACHED: the world's table was retired.
/// Version: since Z2 Stage 3; enumerators keep their meaning once shipped.
/// @endcontract
enum Zenith_PublicationSinkErrorCode : uint8
{
	PUBLICATION_SINK_ERROR_FULL = 1,
	PUBLICATION_SINK_ERROR_REPLAYING,
	PUBLICATION_SINK_ERROR_SITE,
	PUBLICATION_SINK_ERROR_DETACHED,
};
constexpr const char* Zenith_ToString(Zenith_PublicationSinkErrorCode eCode);

// The error record of [Z2-N-CONTRACT-480]: 4 bytes, no string.
struct Zenith_PublicationSinkError
{
	Zenith_PublicationSinkErrorCode m_eCode;
	uint8 m_uSourceKind = 0;
	uint16 m_uReserved = 0;
};

/// @contract interface Zenith_ModulePublicationHandler
/// Ownership: Implemented by a module; registered per source kind with SetSourceHandler; outlives the world's table.
/// Thread affinity: ANY_THREAD. Called on the world's tick-boundary node, step 1, once per publication of the kind, in
///   completion-ID order, live and in replay.
/// Invalidation: xPublication's payload is valid for the call.
/// Units: None.
/// Errors: None.
/// Reentrancy: NONE. It writes world state only through xWorld (property reads and writes, never structure, [Z2-N-SCRIPT-343])
///   and calls no sink member.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ModulePublicationHandler
{
public:
	virtual ~Zenith_ModulePublicationHandler() = default;
	virtual void OnPublished(const Zenith_ModulePublication& xPublication, Zenith_TickBoundaryAccess& xWorld) = 0;
};

/// @contract type Zenith_ModulePublicationSink
/// Ownership: One per world, created by ZenithRuntime at the world's creation and owned by its world host record as a
///   Zenith_Owned; GAME's world host takes it into the world's retired entry and destroys it after the table
///   (§ "Sink lifetime and retirement"). Modules borrow it through Zenith_WorldAttachContext::GetPublicationSink.
/// Thread affinity: DeclareSource and SetSourceHandler MAIN_THREAD during the declaring module's AttachWorld; AllocateCompletionID
///   on the world's merge chain or on the main thread in frame-loop step 1; Park and GetOutstandingCount ANY_THREAD; IsReplaying,
///   GetNextTick and PeekRecorded MAIN_THREAD while the module is attached.
/// Invalidation: A module may call Park through its reference until every completion ID it allocated or took is completed,
///   even after its DetachWorld returned; every other member only until its DetachWorld returns.
/// Units: Completion IDs; ticks; bytes.
/// Errors: AllocateCompletionID returns Zenith_PublicationSinkError; Park never fails visibly (§ "Parking a result");
///   a rejected declaration fails the attaching module's AttachWorld with MODULE_ERROR_REGISTRATION.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ModulePublicationSink
{
public:
	void DeclareSource(uint8 uSourceKind, const Zenith_ModuleSourceDesc& xDesc);
	void SetSourceHandler(uint8 uSourceKind, Zenith_ModulePublicationHandler& xHandler);
	Zenith_Result<uint64, Zenith_PublicationSinkError> AllocateCompletionID(uint8 uSourceKind);
	void Park(uint64 ulCompletionID, uint8 uSourceKind, uint8 uPublicationKind, Zenith_ByteSpan xPayload);
	uint32 GetOutstandingCount(uint8 uSourceKind) const;   // allocated and not yet released
	bool IsReplaying() const;
	uint64 GetNextTick() const;      // the tick whose boundary publishes a result parked now
	Zenith_Span<const Zenith_ModulePublication> PeekRecorded(uint8 uSourceKind, uint64 ulTick);
};
```

The class is concrete (not an interface) and has no seam; tests use the real sink of a runtime booted through `Zenith_RuntimeTestAccess::Create` (§ "Seam tests").

### Sink binding at world creation

`Zenith_WorldHost_CreateWorld` step 4 (`Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation") creates the world's script world, then the sink with `Zenith_TryNew<Zenith_ModulePublicationSink>(xRootHeap, xState, uTag, xScriptWorld.GetAsyncResults(), pxPublicationSource)`, where `pxPublicationSource` is the world's `Zenith_ScriptPublicationSource*` (non-null exactly under `--replay`). The constructor stores the `Zenith_AsyncResultTable*` and never reads `Zenith_ScriptWorld::GetAsyncResults()` again (cross-doc item X274 (3); `Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"); moving the table's `Zenith_Owned` into a retired entry does not move the table, so the bound pointer stays valid for the sink's whole life. The sink is created before any module attaches (step 6), and an allocation failure is world creation's `MODULE_ERROR_OUT_OF_MEMORY`. The world host record holds it in `Zenith_Owned<Zenith_ModulePublicationSink> m_pxModulePublication`.

The sink also holds, as plain state written only on the main thread before the world's first tick: 16 per-kind records `{ uint16 m_uOwnerModule; uint8 m_uSites; uint8 m_bSinkDeclared; uint32 m_uMaxResultBytes; Zenith_ModulePublicationHandler* m_pxHandler; }`, 16 forwarders (§ "The module publication forwarder"), the attaching module's index (set by the module host around each `AttachWorld`) and the replay scratch of § "Replay behaviour of the sink".

### Source declaration and handler registration

`DeclareSource(kind, desc)` is a registration of the attaching module (`Zenith2/Docs/Design/MOD.md` → "Registration rejection rules"); it is rejected, with the reason logged under `runtime.diagnostics` and the module's `AttachWorld` failing with `MODULE_ERROR_REGISTRATION`, when:

1. it is called outside an `AttachWorld`, or after the world's first tick;
2. `kind` ≥ 16, or the kind is already declared in this world (by another module, by SCRIPT for a script source, by SCENE's sink for `ASYNC_SOURCE_SCENE_LOAD`, by GAME for `ASYNC_SOURCE_WORLD_LOAD`, by the PLAT, SAVE or package bridge);
3. `m_uMaxInFlight` is 0 or above `uMODULE_PUBLICATION_MAX_IN_FLIGHT`, `m_uMaxResultBytes` above `uMODULE_PUBLICATION_MAX_RESULT_BYTES`, `m_uSites` is 0, has an unknown bit, or combines `PROLOGUE` with `MERGE_CHAIN`;
4. the world's sink-declared sources would then exceed `ulMODULE_PUBLICATION_MAX_ARENA_BYTES` of result arena (the overflow rule: the table's arena is Σ in-flight × result bytes, [Z2-N-SCRIPT-341]'s declared capacities);
5. the table's `DeclareSource` cannot reserve the source's slots (SCRIPT's out-of-memory case; the reason names it).

On acceptance the sink records the owner and sites and calls `m_pxTable->DeclareSource(kind, m_uMaxInFlight, m_uMaxResultBytes)`. A declaration is never undone: an attach failure fails the world's creation, which destroys the world and its table.

`SetSourceHandler(kind, handler)` is rejected the same way when it is called outside an `AttachWorld`, when the attaching module does not own the kind (it declared it neither on this sink in this attach nor with `Zenith_ModuleStartContext::DeclareScriptAsyncSource` at `Start`), or when the kind already has a handler. On acceptance the sink stores the handler and calls `m_pxTable->SetHandler(kind, &m_axForwarders[kind])`; the module's attach journal records the undo, which clears the handler pointer (`Zenith2/Docs/Design/MOD.md` → "Registration journal and undo"). After the module's `AttachWorld` returns, the module host checks every kind the module declared on the sink in that attach: one without a handler is a rejected registration (`async source <k> declared without a handler`), because a C++ result nobody handles would hold its slot for ever. A script source (`DeclareScriptAsyncSource`) needs no handler; ONLINE's mixed kind registers one (A3 cross-area row ON5 (b)).

**One declaration point per kind** (A5 cross-area row UiB7). A source kind is declared exactly once, in one of two forms, and the form decides who allocates its completion IDs:

| Form | Declared with | When | Who allocates its completion IDs | In-flight capacity |
|---|---|---|---|---|
| sink-declared (a C++ source no script requests) | `DeclareSource(kind, desc)` on the world's sink | in the owning module's `AttachWorld`, once per world | the module, through `AllocateCompletionID` at the sites `desc.m_uSites` names | `desc.m_uMaxInFlight` results of at most `desc.m_uMaxResultBytes` |
| script-declared (a script source, mixed when its owner also registers a handler) | `Zenith_ModuleStartContext::DeclareScriptAsyncSource(kind, uMaxResultBytes)` | in the owning module's `Start`, once per process | SCRIPT at commit for script requests; the owning module, through `AllocateCompletionID` in the prologue alone, for its own notifications once it registered a handler for the kind on the world's sink (§ "Allocation sites and acceptance" step 1) | SCRIPT's (Z2-B-SCRIPT-026; `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values") |

There is no idempotent re-declaration. A second declaration of a kind, in either form and whether or not its values equal the first's, is rejected by rule 2, because the two forms size different storage (a per-world slot reservation of the sink against SCRIPT's per-kind script arena) and the table keeps one capacity per kind, so "identical" has no single meaning across the forms. A C++ source whose results no script requests uses the sink-declared form alone and never calls `DeclareScriptAsyncSource`. UI's `ASYNC_SOURCE_UI` is such a source; its whole declaration, in each `AttachWorld`, is `DeclareSource(ASYNC_SOURCE_UI, { m_uMaxInFlight = ui.events.maxInFlight, m_uMaxResultBytes = uUI_PUBLICATION_MAX_BYTES (448), m_uSites = MODULE_SOURCE_SITE_PROLOGUE })` followed by `SetSourceHandler(ASYNC_SOURCE_UI, <UI's handler>)`, and UI's start step that called `DeclareScriptAsyncSource(ASYNC_SOURCE_UI, …)` is removed (UI conforms).

### Allocation sites and acceptance

`AllocateCompletionID(kind)` is the sink's acceptance test, first-fit like `Zenith_MergeCapacity::TryAccept`: a refusal changes nothing, so the producer defers the request in its own deterministic order and asks again in a subsequent run (A3 cross-area row AN1 (3)). In order:

1. The kind must be sink-declared in this world, or script-declared by a module that registered a handler for it on this sink (a mixed kind; A3 cross-area row ON18 (f)); else `PUBLICATION_SINK_ERROR_SITE` and `ZENITH_ASSERT` (the IDs of a script source without a handler are SCRIPT's alone, allocated at commit).
2. The site is `PROLOGUE` when the caller is the main thread and the runtime state's `m_bFramePrologueActive` is set (frame-loop step 1, set and cleared by the frame-loop driver), else `MERGE_CHAIN`. A site the source did not declare is `PUBLICATION_SINK_ERROR_SITE` and `ZENITH_ASSERT`; a mixed kind's one site is `PROLOGUE`, so a merge-chain allocation of it is `PUBLICATION_SINK_ERROR_SITE` and `ZENITH_ASSERT` too.
3. A `PROLOGUE` allocation while `IsReplaying()` is `PUBLICATION_SINK_ERROR_REPLAYING`, counted in `runtime.async.replay_prologue_refusals`: a prologue producer of a recorded kind allocates nothing under `--replay`, and the table adopts the recorded next completion ID at the tick boundary instead ([Z2-N-SCRIPT-341], [Z2-N-ARCH-794]; orchestrator ruling Q89 (a)). This is the backstop; producers (UI, ONLINE, PACK's bridge) drop live input before they allocate.
4. A retired table (`Zenith_AsyncResultTable::IsRetired()`) is `PUBLICATION_SINK_ERROR_DETACHED`.
5. `m_pxTable->AllocateCompletionID(kind)`; `ASYNC_TABLE_ERROR_FULL` is `PUBLICATION_SINK_ERROR_FULL`. Any other table error is an engine defect: `ZENITH_ASSERT` and `PUBLICATION_SINK_ERROR_SITE`.

A merge-chain allocation is ordered by the merge chain, interleaved deterministically with SCRIPT's commit allocations, so a recomputed kind reproduces its IDs under `--replay` with no help from the log (Z2-D-019, `Zenith_ReplayLog`). Every prologue allocation of a frame happens before `RunFrame` starts, so the `REPLAY_RECORD_TICK` record of the frame's first tick carries a next completion ID taken after all of them ([Z2-N-ARCH-794]); the driver clears `m_bFramePrologueActive` before calling `RunFrame` and no prologue site exists inside it.

A mixed kind's prologue allocations draw on the same per-kind capacity as its script requests (row 4's capacity already counts ONLINE's 8 notifications per world, `Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values"), so `FULL` is possible and is handled as for any source: nothing changed, the module defers. Steps 3 and 5 apply unchanged. Every completion ID the sink hands out is completed exactly once, by the allocating module's single `Park` of it ([Z2-N-SCRIPT-931], cross-doc item X255): the sink never completes, discards or counts an ID on a producer's behalf, and SCRIPT releases a ticketless publication's slot after the forwarder ran, as for every C++ request, so a module notification of a mixed kind never waits for a ticket.

### Parking a result

`Park(id, kind, publicationKind, payload)` completes one ID, any thread, never blocking:

1. The kind must be owned by a module in this world (sink-declared or a script source a module declared); else `ZENITH_ASSERT` and `runtime.async.park_errors` + 1, nothing completed.
2. A `PROLOGUE` source accepts only recorded publication kinds (`PUBLICATION_KIND_GENERIC_RECORDED`, `PUBLICATION_KIND_PLATFORM_RECORDED`), and a mixed kind's module notification accepts `PUBLICATION_KIND_GENERIC_RECORDED` alone (row ON18 (f)); another kind is `ZENITH_ASSERT` and `runtime.async.park_errors` + 1, and the result is completed as given.
3. `m_pxTable->Complete(id, publicationKind, payload)`. A zero-length `PUBLICATION_KIND_GENERIC_RECORDED` payload is accepted for every kind: it is the abandoned-request payload of [Z2-N-SCRIPT-931] and of SAVE's restore (A3 cross-area rows AN1 (6), SV3 (d)).
4. On `ASYNC_TABLE_ERROR_UNKNOWN_COMPLETION` while the world runs under a publication source: a late local completion of an ID whose recorded entry was already published; a no-op counted in `runtime.async.late_local_completions` (A3 cross-area row ST21 (c)). Any other error (`UNKNOWN_COMPLETION` live, `PAYLOAD_TOO_LARGE`) is `ZENITH_ASSERT` and `runtime.async.park_errors` + 1.

**How FULL is reported** (row UiB7). `Park` returns nothing and has no capacity outcome: the slot was reserved when `AllocateCompletionID` succeeded, so a producer learns `FULL` (and, under `--replay`, `REPLAYING`) from `AllocateCompletionID` alone, before it builds a payload, and every successful allocation is followed by exactly one `Park` of that ID. A producer that keeps a slot in reserve for one result kind of its own (UI's resume record, `Zenith2/Docs/Design/UI/Interaction.md` → "Pause menus") expresses the reserve through its own count: it allocates an ordinary result only while `GetOutstandingCount(kind)` minus its reserved results outstanding is below `m_uMaxInFlight − 1`, and allocates the reserved result only while none is outstanding, so the sink never refuses the reserved allocation for capacity. A reserved result is outstanding from its successful `AllocateCompletionID` until the module's handler sees it published; `Park` succeeding is not a condition the producer tests.

A park into a retired table succeeds: the table discards and counts the result, and neither the sink nor the producer counts it ([Z2-N-SCRIPT-931]; the world host alone publishes `world.async_discarded`, [Z2-N-ARCH-558]). This is how PHYS's `DetachWorld` completions and a late AI path job reach a swapped-out world (A3 cross-area rows PH23, GA19).

### The module publication forwarder

`Zenith_ModulePublishForwarder` (`Internal/Zenith_ModulePublicationSinkImpl.h`) implements SCRIPT's `Zenith_AsyncPublicationHandler`; the sink holds one per kind. `OnPublished(xPublication, xWorld)` builds a `Zenith_ModulePublication` from the `Zenith_ScriptPublication` (`m_bScriptOwned` = 1 when `m_xOwnerModule` is valid) and calls the kind's module handler with the same `Zenith_TickBoundaryAccess`. SCRIPT then releases a C++ request's slot after the handler and keeps a script request's publication for its ticket (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"), so a mixed kind such as `ASYNC_SOURCE_ONLINE` sees every publication, script results included. A cleared handler (journal undo) makes the forwarder return at once; the retired table never publishes, so it never calls a detached module.

### Replay behaviour of the sink

- `IsReplaying()` is true exactly when the world runs under a `Zenith_ScriptPublicationSource` (`--replay`).
- `GetNextTick()` is the index of the tick whose boundary publishes a result parked now: the world simulation's last completed tick plus one, read on the main thread between frames.
- `PeekRecorded(kind, tick)` (main thread, frame-loop step 1, replay only; an empty span otherwise) returns the recorded publications of the kind for `tick` from `GetRecorded(tag, tick)` of the publication source, copied into the sink's replay scratch, valid until the next `PeekRecorded` or the end of the prologue. The scratch is reserved at the first `PROLOGUE` declaration of the world, sized to the largest prologue source's `m_uMaxInFlight` × `m_uMaxResultBytes`; more entries than that capacity is `ZENITH_ASSERT` and truncation (a tick cannot publish more than a source's in-flight capacity). UI's MENU replay rule is its user (A3 cross-area row UI1 (d)).
- The late-local-completion rule of § "Parking a result" step 4 is the sink's half of [Z2-N-SCRIPT-341]'s replay table.

### Sink lifetime and retirement

The world host record owns the sink until GAME's world host takes it, with the table and the scene publication sink, into the world's `Zenith_RetiredResults` entry (swap step 6 for a swapped-out world, teardown step `WORLD_TEARDOWN_STEP_ASYNC_RESULTS` for any other; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results") and destroys it after the table and the scene sink at the entry's release (A3 cross-area row GA5 (k)). MOD's code never destroys a sink GAME took; `Zenith_WorldHost_DestroyWorld` releases the record's sink itself only when the record still holds it after the teardown walk (a creation unwind, or a build without GAME's step). After the table is retired every member but `Park` and `GetOutstandingCount` refuses or asserts, because the world's simulation and publication source are gone.

### Sink diagnostics

| Counter | Kind | Meaning |
|---|---|---|
| `runtime.async.late_local_completions` | CUMULATIVE | late local completions dropped under `--replay` (scene and module sinks together) |
| `runtime.async.park_errors` | CUMULATIVE | parks refused as engine defects (0 in every passing run) |
| `runtime.async.replay_prologue_refusals` | CUMULATIVE | prologue allocations refused under `--replay` |

Rejected declarations log one `runtime.diagnostics` line each: `module <name>: async source <k>: <reason>`.

## World attach context lookups

`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" declares these three members of `Zenith_WorldAttachContext` ([Z2-N-ARCH-879]; cross-doc item X344, applied by core-doc patch f); adding them changed no virtual of `Zenith_ModuleInstance`, so `uZENITH_MODULE_INTERFACE_VERSION` stays 1:

```cpp
// Zenith_WorldAttachContext additions (Zenith2/Engine/ZenithRuntime/Zenith_Module.h)
	Zenith_ModulePublicationSink& GetPublicationSink();          // the world's sink; see its contract
	uint32 FindCommandKind(const char* szName) const;          // 0 when absent
	Zenith_WorldControl& GetWorldControl();                     // GAME's Zenith_WorldControl.h
```

- `FindCommandKind` answers from the sealed process `Zenith_CommandKindTable::FindByName` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header"); the table is sealed before step 16, so every attach can call it. Kind IDs are one per process, so a module resolves its kinds at its first `AttachWorld` into its own instance state (never a global) and hands that state to its buffered bindings as their per-function user datum (SCRIPT's route of A3 cross-area rows TD22 (a) and AN10, cross-doc item X346).
- `GetWorldControl` returns the world host record's `Zenith_WorldHostGameRecord::m_xControl` (A3 cross-area row GA5 (a)).
- `GetSimulationClock` and `GetWorldPurpose` are core text ([Z2-N-ARCH-879]) and are implemented here: the clock is the step-9 `Zenith_SimulationClock`, and the purpose is the world's `Zenith_WorldPurpose`, set from `Zenith_WorldCreateDesc::m_ePurpose` (`WORLD_PURPOSE_PLAY` for the runtime's and the world host's worlds) and constant for the world's life.
- The test seam `static Zenith_Result<uint16, Zenith_ModuleError> Zenith_RuntimeTestAccess::CreatePurposedWorld(Zenith_Runtime& xRuntime, Zenith_WorldPurpose ePurpose)` (main thread, no frame running) creates an isolated world of that purpose through `Zenith_WorldHost_CreateWorld`, so every module's `AttachWorld` sees `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW` without the editor; `DestroyPurposedWorld(xRuntime, uTag)` destroys it (A5 cross-area row VxB12).

## World host seams

`Zenith2/Docs/Design/GAME/WorldHost.md` → "World host files and split with MOD" fixes the function split; this section fixes what MOD's half provides (A3 cross-area row GA5).

### Runtime state the world host shares

`Internal/Zenith_RuntimeState.h` (MOD's private state) holds `Zenith_Runtime* m_pxRuntime`, `Zenith_WorldHostGameState m_xGameHost`, `bool m_bFramePrologueActive`, the replay core objects SAVE's bridge creates at step 11 (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md`; the player's `GetReader()` is how GAME's intake calls `Zenith_ReplayLogReader::FindPublication`) and the world host records. Each record holds `Zenith_WorldHostGameRecord m_xGameRecord`, `Zenith_Owned<Zenith_ScenePublicationSink> m_pxScenePublication`, `Zenith_Owned<Zenith_ModulePublicationSink> m_pxModulePublication` and the world's `Zenith_WorldPurpose`. The GAME types are GAME's (`Internal/Zenith_WorldTransition.h`).

### World creation and destruction

- Step 16's per-world sequence is `Zenith_WorldHost_CreateWorld(xState, xDesc)` (GA5 (e)), the order of `Zenith2/Docs/Design/GAME/WorldHost.md` → "World creation" steps 1–8; `Zenith_WorldHost_CreateInitialWorld` calls it with the composition's world seed, `WORLD_ORIGIN_STARTUP`, not isolated, the input world, `WORLD_PURPOSE_PLAY`. `Zenith_WorldCreateDesc` gains `Zenith_WorldPurpose m_ePurpose = WORLD_PURPOSE_PLAY`. A failure unwinds every completed step and returns the step's `Zenith_ModuleError`.
- `Zenith_WorldHost_DestroyWorld` walks GAME's `axWORLD_TEARDOWN_ORDER` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Teardown order algorithm"; GA5 (f)), calling the optional `m_pfnOnTeardownStep(tag, step, pUserData)` before each step and MOD's own destruction code for each MOD-owned step; it cannot fail.

### Call sites GAME's tickets add

Each GAME function below is called from a MOD file, and the GAME ticket that defines the function adds its call in the same diff, at exactly the place given (the rule GAME's design already states for `Zenith_WorldHost_StepTransitions`), so no MOD ticket waits on a GAME implementation (declare first, implement after the callee):

| Call | Where in MOD's code |
|---|---|
| `Zenith_RegisterGameFlowTypes` | startup step 6, engine types, after `Zenith_RegisterRenderComponentTypes` |
| GAME's key registration (`world.transitionReserveFloorBytes`) | step 6, after SCENE's key registration |
| `Zenith_RegisterGameFlowCommandKinds` | step 15, after `Zenith_RegisterSceneCommandKinds` |
| `Zenith_RegisterGameScriptBindings` | step 15, before `Zenith_ScriptBindingTable::Seal` |
| `Zenith_WorldHost_ApplyPauseMasks`, then `Zenith_WorldHost_StepTransitions` | prologue step 7, replacing the driver's `StepActivations` call |
| `Zenith_WorldHost_ReleaseRetiredResults(xState, false)` | prologue step 7, after `StepTransitions` |
| `Zenith_WorldHost_ShutdownWorlds` | shutdown sequence step 1 |
| `Zenith_WorldHost_ReleaseRetiredResults(xState, true)` | right after shutdown sequence step 2 destroys the worlds (GA5 (j)) |

### Swap precompile and test hooks

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Zenith_FrameLoop.h
Zenith_Result<void, Zenith_FrameGraphError> Zenith_FrameLoop_PrecompileSwap(Zenith_RuntimeState& xState,
	uint16 uOutgoingTag, uint16 uIncomingTag);    // MAIN_THREAD, no frame running

// Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h additions
using Zenith_PfnSwapStepHook = bool (*)(uint16 uFromTag, uint16 uToTag, uint8 uStep, void* pUserData);
using Zenith_PfnTeardownStepHook = void (*)(uint16 uWorldTag, uint8 uStep, void* pUserData);
// Zenith_RuntimeTestHooks gains: Zenith_PfnSwapStepHook m_pfnOnSwapStep = nullptr; Zenith_PfnTeardownStepHook m_pfnOnTeardownStep = nullptr;
static uint8 GetGameFlowState(const Zenith_Runtime& xRuntime);   // the Zenith_GameFlowState value
```

`Zenith_FrameLoop_PrecompileSwap` (GA5 (i)) builds the frame graph exactly as prologue step 8 rebuilds it after the swap — the engine stages, every live world's `EmitInto` except the outgoing world's, and the incoming world's — and calls `Zenith_FrameScheduleCache::Acquire` with the default options, without `SetSchedule`; the acquired schedule stays referenced in the runtime state until step 8's next rebuild acquires the same key (a cache hit) or the swap fails, then is released. An error returns `Zenith_FrameGraphError` with each diagnostic logged and changes nothing cached or running. A step-8 failure after a successful precompile can only be an allocation failure, and it ends the run with exit code 3, cause `schedule recompile after a precompiled swap`. `m_pfnOnSwapStep` is called by GAME's swap before each of steps 2–12 (false fails steps 2–4 without running them and is ignored for 5–12); `m_pfnOnTeardownStep` by `DestroyWorld` (GA5 (h)).

## Test assertion failures and world shutdown requests

`void Zenith_Runtime::RecordTestAssertionFailure()` (core text: `Zenith2/Docs/Architecture/CompositionRoot.md` → "Runtime and entry types", X344 applied) is `ANY_THREAD`: one `fetch_add` (relaxed) on `std::atomic<uint32> m_uTestAssertionFailures` of the runtime state. GAME's `Runtime.ReportTestFailure` playback (a sync-point node), a replay divergence, SAVE's replay verifier and the migration-fixture run call it; the frame loop reads the count on the main thread after `RunFrame` returns, which orders every frame thread's increment before the read, and a non-zero count makes the normal exit `EXIT_CODE_TEST_FAILED` (5) (A3 cross-area row GA5 (d); A5 cross-area rows MB15, GmB2).

After every `RunFrame` the frame loop also reads `m_xGameHost.m_uShutdownRequestedByWorld` (acquire); a non-zero value is `RequestShutdown`, so the run stops after the current iteration with exit code 0, or 5 when an assertion was recorded (`Runtime.Quit`, GAME's `Zenith_WorldHost_RequestShutdownFromWorld`).

## Start context registrations

`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" declares these members ([Z2-N-ARCH-879]); `Internal/Zenith_ModuleStartContext.cpp` implements them. Each is `MAIN_THREAD`, valid during `Start` alone, a registration with the rejection rules of `Zenith2/Docs/Design/MOD.md` → "Registration rejection rules", and journaled and undone with its module (A5 cross-area row AuB3):

- `RegisterFrameListener(xListener)` appends to the runtime state's listener list in canonical module order. The runtime's `Zenith_AppLifecycleListener` (`Internal/Zenith_RuntimePlatformListeners.cpp`) calls every listener's `OnStopAudio` and `OnResumeAudio` inside `Platform.Pump`, in that order; the frame-loop driver calls `OnPlatformEvents(axEvents)` with the frame's platform events after prologue step 1. A second registration by one module is rejected.
- `AddEngineEpilogueStage(xDesc)`: at most one per module, `m_szName` of the form `<Module>.<Stage>` unique in the process and `m_pfnRun` non-null, else rejected. The stage is kept in the runtime state and emitted by the frame-graph build of startup step 18 and every prologue step-8 rebuild as one `FRAME_NODE_KIND_STAGE` node of EPILOGUE stage 13, group 0, headless included and with no world attached, ordered after every live world's epilogue stage named `m_szAfterWorldStage` (one generated `After` per matching world; none when no world has it). The returned builder accepts a write of one world-independent resource; any other access is refused when the schedule is compiled, which startup step 18 maps to exit code 3.
- `SetScriptSideEffectConsumer(eKind, xConsumer)` forwards to the process binding table's `SetSideEffectConsumer` for `SIDE_EFFECT_KIND_AUDIO_TRIGGER` and `SIDE_EFFECT_KIND_VFX_TRIGGER`; `SIDE_EFFECT_KIND_ASYNC_REQUEST` goes to the multiplexer's route table (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"); `SIDE_EFFECT_KIND_SAVE_REQUEST` is the runtime's own consumer and a module's registration of it is rejected, as is a second consumer for one kind.
- `DeclareScriptAsyncSource(uSourceKind, uMaxResultBytes)` forwards to the binding table's `DeclareAsyncSource` and records the module as the kind's owner, which the sink's ownership checks and the multiplexer's routes read; a kind of 16 or above, one already declared by any owner, or `uMaxResultBytes` above `uMODULE_PUBLICATION_MAX_RESULT_BYTES` is rejected. The kind's in-flight capacity is SCRIPT's (Z2-B-SCRIPT-026, applied at the merge).

<!-- assumption: pending ARCH (X419) -->
`uint32 GetScriptAsyncCapacity(uint8 uSourceKind) const` (`MAIN_THREAD`, during `Start`; A3 cross-area row AI1) is a query, not a registration, and journals nothing. It returns the in-flight capacity every script world of the run gives that kind at the merge. The first call in a `StartAll` runs `Zenith_ScriptReadWorldLimits(*xState.m_pxConfigSchema, xState.m_pxConfig->GetStore(), xDesc)` into a scratch `Zenith_ScriptWorldDesc` held by the host for that `StartAll` (the call startup step 16 makes for every world, over the same configuration). The member then returns `xDesc.m_xCapacities.m_auAsyncInFlight[uSourceKind - 1]` for `ASYNC_SOURCE_PATHFINDING` (1) to `ASYNC_SOURCE_ONLINE` (4), the four Z2-B-SCRIPT-026 values `Zenith_ScriptCapacityLimits` holds (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Source kind values"). Every other kind returns 0, because its capacity is its declarer's and not a SCRIPT-held class. A failed limits read returns 0 and logs `Runtime: script world limits unavailable during Start: <Zenith_ToString of the code>` once under `runtime.startup`; step 16 then fails with its own exit code. The AI module reads P, its path value, here for the path inbox's capacity and its start-time queue invariant (`Zenith2/Docs/Design/AI/Navigation.md` → "Request queue"). `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" does not list the member yet; cross-doc item X419 carries it.
<!-- /assumption -->

## Start context overlay access

`Zenith_ProfilerOverlay* Zenith_ModuleStartContext::GetProfilerOverlay()` (`MAIN_THREAD`, during `Start`; core text [Z2-N-ARCH-879], X245 (c) applied by core-doc patch f) returns DEVT's overlay created at startup step 7, or null in Shipping and headless builds, through which a module registers its panel provider (A3 cross-area row DV12, DEVT's `Zenith2/Docs/Design/DEVT/Overlay.md`; A5 cross-area row VxB16). A provider registration is journaled and undone with the module.

## Start context members for ONLINE and PACK

<!-- assumption: pending ARCH (X325 (1)), ONLINE, PACK (rows ON18 (b), (c), PK21) -->
`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" declares `IsRecordingReplay` and `RequestExitAfterStart` (`MAIN_THREAD` during `Start`, [Z2-N-ARCH-879]; cross-doc item X369, applied by core-doc patch g) and does not declare `ProvideEntitlementQuery` yet: cross-doc item X325 (1) carries it. None changes an entry point or a virtual of `Zenith_ModuleInstance`, so `uZENITH_MODULE_INTERFACE_VERSION` stays 1.

```cpp
// Zenith_ModuleStartContext additions (Zenith2/Engine/ZenithRuntime/Zenith_Module.h)
class Zenith_EntitlementQuery;   // ZenithPackage (Zenith2/Docs/Design/PACK/PublicAPI.md → "Zenith_EntitlementQuery header"); forward-declared only
	bool IsRecordingReplay() const;                     // MAIN_THREAD, during Start: true under --record
	void RequestExitAfterStart();                                     // MAIN_THREAD, during Start: finish startup, run no frame, exit 0
	void ProvideEntitlementQuery(Zenith_EntitlementQuery& xQuery);    // MAIN_THREAD, during Start: at most one per process; journaled
```

- `IsRecordingReplay()` is true exactly when the command line carries `--record` (`Zenith_CommandLine::GetRecordPath()` non-empty). It is not a registration and journals nothing; called outside `Start` it is `ZENITH_ASSERT` and returns the same value. ONLINE reads it to start its session with display-name exposure off, so no display name enters a replay log (A3 cross-area row ON18 (c); `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure").
- `RequestExitAfterStart()` sets the runtime state's `m_bExitAfterStart` and records the first requester's module index in `m_uExitAfterStartModule`; a second call by any module is a no-op; a call outside `Start` is `ZENITH_ASSERT` and changes nothing. It is not a registration and is not undone: a `Start` that fails after the call keeps the request, because its cause (Steam relaunching the game through the store client, `Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam initialisation and pumping") holds whether or not the module started. When startup succeeds through step 18 with the flag set, `Zenith_RuntimeMain` logs `Runtime: exit requested during module start by '<module>'; no frame runs` under `runtime.startup`, does not call `RunFrameLoop`, destroys the runtime through the ordinary shutdown order and returns `EXIT_CODE_NORMAL` (0); a startup failure keeps that step's exit code (A3 cross-area row ON18 (b)). Under the tool access (§ "Runtime tool access") `Create` succeeds and the first `RunOneFrame` returns false, running no frame, with `GetRequestedExitCode` = `EXIT_CODE_NORMAL`.
- `ProvideEntitlementQuery(xQuery)` is a registration with the rejection rules of `Zenith2/Docs/Design/MOD.md` → "Registration rejection rules": a second call in the process, by any module, is rejected with `entitlement query already provided by '<first module>'`, and a call outside `Start` with `entitlement query provided outside Start`. On acceptance the runtime state records `const Zenith_EntitlementQuery* m_pxModuleEntitlementQuery = &xQuery` and `uint16 m_uEntitlementQueryModule` (0xFFFF: none), the start journal gains the record `REGISTRATION_KIND_ENTITLEMENT_PROVIDER`, and the context calls `Zenith_RuntimePackage_SetEntitlementProvider(xRuntime, xQuery)` (the call PACK's ticket adds, § "Call sites PACK's tickets add"). The record's undo calls `Zenith_RuntimePackage_ClearEntitlementProvider(xRuntime)` and resets the pointer and the owner. It runs when the providing module's `Start` fails after the call, and when the module is destroyed at Contracts shutdown step 6, after `Zenith_RuntimePackage_BeginShutdown` already cleared the provider at Contracts step 4; the clear is idempotent by PACK's contract (no provider, every ID `UNKNOWN`). `xQuery` must stay valid until that undo; ONLINE's `session.GetPackageEntitlements()` does, because the session lives until its module is destroyed (A3 cross-area row PK21 (a), (b); `Zenith2/Docs/Design/PACK/Algorithms.md` → "Entitlement provider"). For tests, `static uint16 Zenith_RuntimeTestAccess::GetEntitlementProviderModule(const Zenith_Runtime& xRuntime)` returns `m_uEntitlementQueryModule`.
<!-- /assumption -->

## Start context and attach context members for CINE

`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces" declares these members ([Z2-N-ARCH-879]; cross-doc items X339 (a), X313 (c) and X313 (d), applied by core-doc patch h). `Internal/Zenith_ModuleStartContext.cpp` and `Internal/Zenith_WorldAttachContext.cpp` implement them. None changes a virtual of `Zenith_ModuleInstance`, so `uZENITH_MODULE_INTERFACE_VERSION` stays 1.

- `GetStringTable()` (`MAIN_THREAD`, during `Start`) returns the process `Zenith_StringTable` that startup step 10 created and bound to the type registry (the runtime state's `m_xStrings`). It is not a registration and journals nothing. The reference is valid for the process's lifetime, so a module's kind loaders keep it past `Start` and intern at every load (`Zenith2/Docs/Design/CINE/Formats.md`). The table is destroyed after every module instance, because MOD shutdown step 3 precedes the step-10 destroy entry.
- `AddTickBoundaryHandler(xHandler)` (`MAIN_THREAD`, during `AttachWorld`) forwards to the world's `Zenith_WorldSimulation::AddTickBoundaryHandler` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header") and appends the attach-journal record `REGISTRATION_KIND_TICK_BOUNDARY_HANDLER`. A call outside `AttachWorld` is rejected with `tick-boundary handler added outside AttachWorld` by the rules of `Zenith2/Docs/Design/MOD.md` → "Registration rejection rules", and does not reach ECS. Every `AttachWorld` runs before its world's first tick, so a world whose tick index is not 0 is a programmer error: `ZENITH_ASSERT`, and the call changes nothing.

  The record's undo asserts that the world's tick index is still 0 and removes nothing from ECS. Every path that undoes an attach journal destroys the world before it ticks again: a failed creation destroys it, and a destroyed world undoes its journals after `DetachWorld`. The handler list belongs to the world's simulation, so no handler outlives its module's attach.
- `GetCaptureTickGate()` (`MAIN_THREAD`, during `Start`) returns the runtime state's `Zenith_CaptureTickGate`. Startup step 15 creates the gate before `StartAll` in development builds. The member returns null in Shipping (`if constexpr (Zenith_IsShippingBuild())`) and when `sim.ticksPerFrame` is not 1. The gate lives in the runtime state for the process's lifetime.
  - `HoldNextTick()` (`MAIN_THREAD`, outside a frame) sets the gate's `m_bHoldNextTick`.
  - The clock advance of frame-loop step 1 reads and clears the flag. When it was set, every attached world's clock advances with `AdvanceExternal(0)` instead of the frame's ordinary advance, so the frame renders the same tick again without pausing any world.
  - A second `HoldNextTick` before that advance changes nothing. A call while a frame runs is `ZENITH_ASSERT` and changes nothing.

## Call sites PACK's tickets add

<!-- assumption: pending PACK (rows PK3, PK15, PK21, PK22, PK27 (d)) -->
PACK's runtime bridge is PACK's subdirectory `Internal/Package/` of ZenithRuntime (`Zenith2/Docs/Design/PACK/PublicAPI.md` → "Runtime bridge declarations"), and `ZenithRuntime` declares `ZenithPackage` in its `DEPENDS` in the PACK ticket that first adds a call below. Each PACK function is called from a MOD file, and the PACK ticket that defines the function adds its call in the same diff, at exactly the place given, as GAME's tickets do (§ "Call sites GAME's tickets add"), so no MOD ticket waits on a PACK implementation. "Contracts step" numbers are `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ("Shutdown ownership order"); "MOD shutdown step" numbers are `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence".

| Call | Where in MOD's code |
|---|---|
| `Zenith_RuntimePackage_RunPendingInstalls(xRuntime)` | the closing action of startup step 4, after `Zenith_Platform::Create` and the watchdog start; it never fails startup |
| `Zenith_RuntimePackage_ReleaseInstanceGuard(xRuntime)` | the first action of step 4's destroy entry, before `Zenith_Platform` is destroyed, so an injected or real failure at any later step unwinds with no install lock held (row PK22 (b)); a no-op when the closing action never ran; and at Contracts step 10b, after `Zenith_VirtualFileSystem` and before `Zenith_Platform` (MOD shutdown step 7; core-doc rows X318 (c), X343) |
| `Zenith_RuntimePackage_GetUpdateStampLookup(xRuntime)` | startup step 5, as `Zenith_VirtualFileSystemDesc::m_pxUpdateStamps` for the patch mount's verification (row PK27 (d); null under `package.reverifyOnStartup` or without a valid verified-stamp file; PLAT's member) |
| `Zenith_RuntimePackage_Create(xRuntime)` | the closing action of startup step 11, after `Zenith_RuntimeSave_CreateSystem`. A failed `Create` fails step 11, and its `Zenith_PackageError` picks the exit code: `PACKAGE_ERROR_REPLAY_CONTENT_MISSING` (value 37: a unit of the replay's recorded pack set, or a DLC revision, is not installed, [Z2-N-ARCH-784]) is exit code 5, and every other `Zenith_PackageError` is exit code 3, in every build type. A Shipping build's absent or invalid `package.zpkm` is one of those other errors; outside Shipping an absent manifest is no error. The two rows are in `Zenith2/Docs/Design/MOD.md` → "Step failure to exit code mapping". Z2T-PACK-048 writes the mapping in MOD's step-11 file `Internal/Zenith_StepAssets.cpp`, in the same diff as the call, and tests both codes; no MOD ticket implements it. Step 11's destroy entry calls `Zenith_RuntimePackage_Destroy` first |
| `Zenith_RuntimePackage_SetEntitlementProvider`, `Zenith_RuntimePackage_ClearEntitlementProvider` | inside `Zenith_ModuleStartContext::ProvideEntitlementQuery`'s acceptance branch and its journal record's undo (§ "Start context members for ONLINE and PACK") |
| `Zenith_RuntimePackage_RegisterCommandKinds(xRuntime, xKinds)` | startup step 15, the last call of `Zenith_RegisterEngineCommandKinds` (after `Zenith_RegisterGameFlowCommandKinds`), before `Zenith_ModuleHost::StartAll` and so before `Zenith_CommandKindTable::Seal`: the four `Package.*` module command kinds |
| `Zenith_RuntimePackage_RegisterScriptBindings(xRuntime, xBindings)` | startup step 15, the last call of `Zenith_RegisterEngineScriptBindings` (after `Zenith_RegisterGameScriptBindings`), before `StartAll` and so before `Zenith_ScriptBindingTable::Seal`: the `Package` namespace |
| `Zenith_RuntimePackage_AfterModulesStarted(xRuntime)` | startup step 15, after `Zenith_ModuleHost::StartAll` returned and before `Zenith_CommandKindTable::Seal` (row PK21 (a); core-doc row X357 (c)) |
| `Zenith_RuntimePackage_AttachWorld(xRuntime, *pxResults, uTag)` | `Zenith_WorldHost_CreateWorld` step 4, after the script world and the module publication sink and before GAME's step 5 and the module attaches of step 6, for every world the host creates (the startup world, a transition target, an isolated or restored world, a tool host's world); `pxResults` is the world's `Zenith_ScriptWorld::GetAsyncResults()`, read once there; the world host record holds the result as `Zenith_Owned<Zenith_RuntimePackageWorldRecord> m_pxPackageRecord`; `PACKAGE_ERROR_OUT_OF_MEMORY` is world creation's `MODULE_ERROR_OUT_OF_MEMORY` and every other error `MODULE_ERROR_REGISTRATION` (row PK22 (a)), unwound with the earlier steps |
| release of `m_pxPackageRecord` | `Zenith_WorldHost_DestroyWorld`, after the teardown walk: the world's table is detached by then and a detached table never publishes, so the record's handler is never called after its release |
| `Zenith_RuntimePackage_RunPrologue(xRuntime)` | prologue step 1b, after `Platform.Pump`, while `m_bFramePrologueActive` is set |
| `Zenith_RuntimePackage_ApplyCommands(xRuntime)` | the first call of `Zenith_FrameLoop_AfterRunFrame`, after `RunFrame` returns and before frame-loop step 3 (`Zenith_DevTools::EndFrame`). Every `Package.*` playback of the frame's ticks ran at a sync point inside `RunFrame` and buffered its command in the world's package record; the main thread applies them here in world-tag order, then merge order. The installer reads them at the next prologue step 1b, the first place a buffered command can have an effect |
| `Zenith_RuntimePackage_BeginShutdown(xRuntime, bDeviceLossExit)` | MOD shutdown step 1, at Contracts step 4, after `Zenith_RuntimeSave_BeginShutdown`; `bDeviceLossExit` is true exactly on the device-loss clean exit |
| `Zenith_RuntimePackage_Destroy(xRuntime)` | MOD shutdown step 3, after the module instances are destroyed (Contracts step 6) |

MOD gives these calls named positions:

- `Zenith_RegisterEngineCommandKinds(xState, xKinds)` and `Zenith_RegisterEngineScriptBindings(xState, xBindings)` (`Internal/Zenith_StepModules.cpp`) are step 15's two engine-library registration functions. Each calls its engine libraries' registrations in the order of the tables above, then calls `Zenith_RuntimeTestHooks::m_pfnOnEngineRegistrations` when it is set (kinds first, then bindings). Both run before `StartAll`; the seals follow `StartAll` and `Zenith_RuntimePackage_AfterModulesStarted`.
- `Zenith_FrameLoop_AfterRunFrame(xState, ulFrameIndex)` (`Internal/Zenith_FrameLoop.cpp`) is the main-thread position between `RunFrame` and frame-loop step 3. It ends with `Zenith_RuntimeTestHooks::m_pfnAfterRunFrame` when that is set.

Both hooks are `Zenith_RuntimeTestAccess` members and stay null in `Zenith_Runtime::Create`:

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h additions
using Zenith_PfnEngineRegistrationHook = bool (*)(Zenith_CommandKindTable* pxKinds, Zenith_ScriptBindingTable* pxBindings,
	void* pUserData);   // one table non-null per call; false fails step 15 with exit code 3
using Zenith_PfnAfterRunFrameHook = void (*)(uint64 ulFrameIndex, void* pUserData);
// Zenith_RuntimeTestHooks gains: Zenith_PfnEngineRegistrationHook m_pfnOnEngineRegistrations = nullptr;
//                                Zenith_PfnAfterRunFrameHook m_pfnAfterRunFrame = nullptr;
```

PACK's own tests prove the order (`Test_RuntimePackage_StartupStepOrder`, `Test_RuntimePackage_EntitlementProviderLifecycle`, `Zenith2/Docs/Design/PACK/Tests.md` → "Runtime bridge tests"); MOD's `Test_Runtime_StartupFailureUnwinds` already proves that step 4's destroy entry runs on every later failure.
<!-- /assumption -->

## Runtime tool access

MOD adopts EDIT's row ED1: a tool host (the editor, and any later tool that runs the game's runtime) drives the game's own runtime through one tool-only access header, compiled in every configuration like `Zenith_RuntimeTestAccess` and reachable only from tool sources (`Zenith2/Docs/Contracts.md` → "Test separation", `TestSeparationCheck` condition 7 and [Z2-N-CONTRACT-414]; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Runtime and entry types" declares the class with `Create` and `RunOneFrame`, the five `RUNTIME_ERROR_` values and [Z2-N-ARCH-876]'s three `Create` forms; cross-doc items X258 and X370, applied by core-doc patch g). This section declares the remaining members, the hook and service types and the legal call positions. There is one runtime: `Zenith_RuntimeToolAccess::Create` is the shared `Internal/Zenith_RuntimeCreate.cpp` implementation run in tool mode, and `RunOneFrame` is one iteration of the same frame-loop driver (`Zenith2/Docs/Design/MOD.md` → "Frame loop driver"), so the editor runs exactly the Player's code paths plus the hooks below.

```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_RuntimeToolAccess.h (public; tool sources only)
#pragma once
#include "ZenithRuntime/Zenith_Runtime.h"
#include "ZenithRuntime/Zenith_Extractor.h"              // Zenith_ExtractorDesc
#include "ZenithReflect/Zenith_Schema.h"                  // Zenith_SchemaError (Zenith2/Docs/Design/ECS/Schemas.md)
#include "ZenithECS/Zenith_ScheduleErrors.h"              // Zenith_ScheduleError (Zenith2/Docs/Design/ECS/SystemAPI.md)
#include "ZenithScript/Zenith_ScriptError.h"              // Zenith_ScriptError, Zenith_ScriptModuleID (Zenith_ScriptTypes.h)
#include "ZenithScript/Zenith_ScriptRuntime.h"            // Zenith_PfnScriptCompileConsole
#include "ZenithPlatform/Zenith_CrashHandler.h"           // Zenith_CrashHelperIdentity

struct Zenith_InputFrame;            // ZenithPlatform (Zenith2/Docs/Design/PLAT/Input.md)
class Zenith_RHIFaultInjector;       // ZenithRHI (Zenith2/Docs/Design/RHI/PublicAPIDevice.md)
class Zenith_WorldSimulation; class Zenith_SceneWorldState; class Zenith_ScriptWorld; class Zenith_WorldControl;
class Zenith_JobSystem; class Zenith_Platform; class Zenith_Config; class Zenith_TypeRegistry; class Zenith_StringTable;
class Zenith_AssetRegistry; class Zenith_MemoryGovernor; class Flux_Renderer; class Zenith_ScriptRuntime;
class Zenith_SceneServices; class Zenith_SceneLoader; class Zenith_CommandKindTable; class Zenith_InputSystem;

/// @contract value Zenith_ToolWorldDesc
/// Ownership: Plain value passed to CreateWorld. Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need. Invalidation: None. Units: None.
/// Errors: None. Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ToolWorldDesc
{
	Zenith_WorldPurpose m_ePurpose = WORLD_PURPOSE_EDIT;
	uint64 m_ulWorldSeed = 0;
};

/// @contract value Zenith_ToolWorld
/// Ownership: Borrowed pointers into the runtime's world host record; valid until DestroyWorld(m_uTag) or the runtime's
///   destruction. Thread affinity: MAIN_THREAD. Invalidation: DestroyWorld. Units: None. Errors: None. Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ToolWorld
{
	uint16 m_uTag = 0;
	Zenith_WorldPurpose m_ePurpose = WORLD_PURPOSE_EDIT;
	Zenith_WorldSimulation* m_pxSimulation = nullptr;
	Zenith_SceneWorldState* m_pxSceneState = nullptr;
	Zenith_ScriptWorld* m_pxScriptWorld = nullptr;      // null in a Stage 2 build (no script runtime)
	Zenith_WorldControl* m_pxControl = nullptr;
};

/// @contract value Zenith_ToolServices
/// Ownership: Borrowed references to the runtime's process objects, filled once by Create; valid until the runtime is
///   destroyed. The tool's composition root passes each to the constructors that need it (Z2-D-046); there is no getter.
/// Thread affinity: MAIN_THREAD (each object's own affinity applies to its use). Invalidation: the runtime's destruction.
/// Units: None. Errors: None. Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ToolServices
{
	Zenith_Heap* m_pxHeap = nullptr; Zenith_JobSystem* m_pxJobs = nullptr; Zenith_Platform* m_pxPlatform = nullptr;
	Zenith_Config* m_pxConfig = nullptr; Zenith_TypeRegistry* m_pxTypes = nullptr; Zenith_StringTable* m_pxStrings = nullptr;
	Zenith_AssetRegistry* m_pxAssets = nullptr; Zenith_MemoryGovernor* m_pxGovernor = nullptr;
	Flux_Renderer* m_pxRenderer = nullptr;                // null when headless
	Zenith_ScriptRuntime* m_pxScripts = nullptr;           // null in a Stage 2 build
	Zenith_SceneServices* m_pxSceneServices = nullptr; Zenith_SceneLoader* m_pxSceneLoader = nullptr;
	Zenith_CommandKindTable* m_pxCommandKinds = nullptr; Zenith_InputSystem* m_pxInput = nullptr;
};

/// @contract value Zenith_ToolRuntimeDesc
/// Ownership: Plain value copied by Create; m_pxRHIFaultInjector is borrowed and outlives the runtime.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need.
/// Invalidation: None. Units: None. Errors: None. Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ToolRuntimeDesc
{
	bool m_bHiddenWindow = false;                                  // startup step 12 creates the window hidden
	Zenith_RHIFaultInjector* m_pxRHIFaultInjector = nullptr;       // tests only; passed to step 12's device desc
	Zenith_CrashHelperIdentity m_xCrashIdentity;                   // step 7's Zenith_CrashHandlerDesc::m_xIdentity
	Zenith_PfnScriptCompileConsole m_pfnCompileConsole = nullptr;  // step 14's Zenith_ScriptRuntimeDesc::m_pfnCompileConsole
};

/// @contract value Zenith_SchemaWindowResult
/// Ownership: Plain value passed to OnSchemaWindowResult. Thread affinity: IMMUTABLE. Members non-const: filled field by field as the window runs. Invalidation: the call.
/// Units: m_ulRegistryRevision is Zenith_TypeRegistry::GetRevision(). Errors: m_xError is meaningful when !m_bApplied.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SchemaWindowResult
{
	bool m_bApplied = false;                 // every queued file registered and EndHotReload succeeded
	uint8 m_auReserved[3] = {};
	uint32 m_uFileCount = 0;                // files the window processed
	uint32 m_uFailedFileIndex = 0xFFFFFFFFu;   // queue index of the refused file, or none
	Zenith_SchemaError m_xError{};           // RegisterSchemaJson's error for that file
	uint64 m_ulRegistryRevision = 0;        // after EndHotReload, or unchanged when abandoned
};

/// @contract interface Zenith_ToolFrameHooks
/// Ownership: Implemented by the tool; borrowed by Create; outlives the runtime.
/// Thread affinity: MAIN_THREAD; every member is called inside RunOneFrame at its position (§ "Runtime tool access").
/// Invalidation: every reference argument is valid for the call. Units: ticks, frame indices. Errors: None.
/// Reentrancy: a hook calls only the Zenith_RuntimeToolAccess members its position admits (the table below), never RunOneFrame.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_ToolFrameHooks
{
public:
	virtual ~Zenith_ToolFrameHooks() = default;
	virtual void OnInputFrame(const Zenith_InputFrame& xFrame) = 0;           // after prologue steps 1–3
	virtual void OnSchemaWindowResult(const Zenith_SchemaWindowResult& xResult) = 0;   // inside prologue step 6
	virtual void OnPrologueEnd() = 0;                                          // after prologue step 8, before RunFrame
	virtual void OnWorldHalted(uint16 uWorldTag, uint64 ulTick, const Zenith_ScriptError& xError) = 0;
	virtual void OnScriptModuleLive(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, uint32 uGeneration) = 0;
	virtual void OnWorldScheduleFailed(uint16 uWorldTag, const Zenith_ScheduleError& xError) = 0;
	virtual void OnFrameEnd(uint64 ulFrameIndex) = 0;                         // after RunFrame returned
};

/// @contract type Zenith_RuntimeToolAccess
/// Ownership: A static-only access type; holds no state. Thread affinity: MAIN_THREAD, at the positions of the table below.
/// Invalidation: None. Units: None. Errors: Zenith_RuntimeError (the codes below); a call at a position the table refuses is
///   a ZENITH_CHECK. Reentrancy: NONE. Version: since Z2 Stage 3; compiled in every configuration, included only by tool
///   sources (X258).
/// @endcontract
class Zenith_RuntimeToolAccess
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Runtime>, Zenith_RuntimeError> Create(const Zenith_CommandLine& xCommandLine,
		const Zenith_GameComposition& xComposition, Zenith_Heap& xRootHeap, Zenith_LogRouter& xLogRouter,
		Zenith_OOMPressureFlag& xOOMPressureFlag, Zenith_Profiler& xProfiler, const Zenith_ToolRuntimeDesc& xDesc,
		Zenith_ToolFrameHooks& xHooks, Zenith_ToolServices& xServicesOut, Zenith_StartupReport& xReportOut);
	static Zenith_Result<bool, Zenith_RuntimeError> RunOneFrame(Zenith_Runtime& xRuntime);
	static Zenith_ExitCode GetRequestedExitCode(const Zenith_Runtime& xRuntime);
	static Zenith_Result<Zenith_ToolWorld, Zenith_RuntimeError> CreateWorld(Zenith_Runtime& xRuntime,
		const Zenith_ToolWorldDesc& xDesc);
	static void DestroyWorld(Zenith_Runtime& xRuntime, uint16 uTag);
	static void SetInputWorld(Zenith_Runtime& xRuntime, uint16 uTagOrZero);
	static Zenith_ModuleInstance* FindModuleInstance(Zenith_Runtime& xRuntime, const char* szModuleName);
	static Zenith_Result<void, Zenith_RuntimeError> AddToolExtractor(Zenith_Runtime& xRuntime, uint16 uWorldTag,
		const Zenith_ExtractorDesc& xDesc);
	static Zenith_Result<void, Zenith_RuntimeError> QueueSchemaJson(Zenith_Runtime& xRuntime,
		Zenith_ByteSpan xJson, Zenith_StringView strSourcePath);
};
```

`Zenith_RuntimeErrorCode` gains five enumerators after `RUNTIME_ERROR_UNKNOWN_WORLD` (`Zenith2/Docs/Design/MOD.md` → "Zenith_Runtime.h"; X370): `RUNTIME_ERROR_WORLD_CREATE` (CreateWorld failed; `m_uModuleIndex` names the failing module or is 0xFFFF; the module error is logged), `RUNTIME_ERROR_WORLD_SCHEDULED` (AddToolExtractor for a world already in a compiled schedule), `RUNTIME_ERROR_REGISTRATION` (the world's system registry refused the extractor), `RUNTIME_ERROR_QUEUE_FULL` (QueueSchemaJson over the queue's capacity) and `RUNTIME_ERROR_FRAME_FAILED` (a frame ended with an unrecoverable runtime error: a step-8 recompile failure of a running world, or a fatal runtime error the Player maps to exit code 1).

**Create in tool mode.** The implementation is the step table of `Zenith2/Docs/Design/MOD.md` → "Startup sequence with unwinding" with the runtime state's `m_eHostMode = RUNTIME_HOST_MODE_TOOL` (the Player and `Zenith_RuntimeTestAccess` use `RUNTIME_HOST_MODE_PLAYER`); the differences, all in that mode alone:

1. step 7 passes `xDesc.m_xCrashIdentity` as `Zenith_CrashHandlerDesc::m_xIdentity` (the Player passes `{ m_szGameName, m_ulBuildIdentity }` of its composition), through the runtime state's `m_xCrashIdentity`, which both modes fill before step 4;
2. step 12 creates the window with `Zenith_WindowDesc::m_bHidden = xDesc.m_bHiddenWindow` and passes `xDesc.m_pxRHIFaultInjector` to the device desc;
3. step 14 passes `xDesc.m_pfnCompileConsole` as `Zenith_ScriptRuntimeDesc::m_pfnCompileConsole` (null for the Player, [Z2-N-SCRIPT-923]);
4. step 16 creates no initial world and step 17 loads no startup scene and makes no resume decision; both still record their timings (0 µs) and count as completed in the report, so `m_eLastCompletedStep` reaches 18;
5. step 18 compiles the schedule of the engine stages alone (the headless shape, render subgraph present when a renderer exists);
6. after step 18, `xServicesOut` is filled from the runtime state, once.

A failing step unwinds exactly as for the Player and returns its `Zenith_RuntimeError` with the report filled, so the tool returns the report's exit code.

**Call positions.** The runtime state's `m_eToolPosition` names where the tool is: `BETWEEN_FRAMES` (outside `RunOneFrame`), `INPUT_FRAME`, `GOVERNOR` (the memory governor's callbacks inside prologue step 4), `SCHEMA_WINDOW`, `PROLOGUE_END`, `IN_FRAME` (inside `RunFrame`), `SCRIPT_EVENTS` and `FRAME_END`. Each member checks its position first with `ZENITH_CHECK` (a refused position is a tool defect, never a recoverable error), against this table, which is EDIT's `Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook" adopted with one added hook:

| Member | `INPUT_FRAME` | `GOVERNOR` | `SCHEMA_WINDOW` | `PROLOGUE_END` | `IN_FRAME` | `SCRIPT_EVENTS` | `FRAME_END` | `BETWEEN_FRAMES` |
|---|---|---|---|---|---|---|---|---|
| `CreateWorld`, `DestroyWorld`, `SetInputWorld`, `QueueSchemaJson` | yes | no | no | no | no | no | yes | yes |
| `AddToolExtractor` | yes | no | no | no | no | no | yes | yes |
| `FindModuleInstance` | yes | no | no | yes | no | no | yes | yes |
| `RunOneFrame`, destroying the runtime, `Create` | no | no | no | no | no | no | no | yes |

**RunOneFrame**, on the main thread, from `BETWEEN_FRAMES`:

1. When `m_bExitAfterStart` is set (§ "Start context members for ONLINE and PACK"): clear it, set the requested exit code to `EXIT_CODE_NORMAL`, return `false` with no frame run.
2. Prologue steps 1–3 exactly as the frame-loop driver runs them (`m_bFramePrologueActive` set); then `m_eToolPosition = INPUT_FRAME` and `OnInputFrame(<the frame's Zenith_InputFrame of PLAT's input system>)`.
3. Prologue step 4 with `m_eToolPosition = GOVERNOR` while the governor's callbacks run; step 5.
4. Prologue step 6 with `m_eToolPosition = SCHEMA_WINDOW`: when the schema queue is non-empty the runtime joins the queued files to the hot-reload window of [Z2-N-ARCH-863] (it opens one with `BeginHotReload` when no script change already opened it), calls `Zenith_TypeRegistry::RegisterSchemaJson(bytes, path)` for each file in queue order, then `EndHotReload`, or `AbandonHotReload` at the first refusal (the remaining files are not offered), empties the queue, and calls `OnSchemaWindowResult` once with the result; with an empty queue the step runs as for the Player and no hook is called.
5. Prologue steps 7 and 8. Step 8's rebuild freezes the system registry of every tool world created since the last rebuild (as GAME's step 9 freezes an isolated world); a freeze that returns a `Zenith_ScheduleError` destroys that world through `Zenith_WorldHost_DestroyWorld`, logs `runtime.world: tool world <tag>: schedule error <code>: <diagnostic>` per diagnostic, rebuilds without it and records `(tag, error)` for step 8 below; the frame runs.
6. `m_eToolPosition = PROLOGUE_END`, `OnPrologueEnd()`; clear `m_bFramePrologueActive`; `m_eToolPosition = IN_FRAME`; `RunFrame` exactly as the driver calls it.
7. A recompile failure of a running world or a recorded unrecoverable runtime error (the Player's exit-code-1 causes, except a script world halt): `m_eToolPosition = BETWEEN_FRAMES`, return `Zenith_Err(Zenith_RuntimeError{ RUNTIME_ERROR_FRAME_FAILED })`; the runtime stays destructible.
8. `m_eToolPosition = SCRIPT_EVENTS`: every script world halt and module-live event the runtime's `Zenith_RuntimeScriptListener` recorded this frame, in world-tag order and then record order, as `OnWorldHalted` and `OnScriptModuleLive` (in a tool host a halt ends no run: the tool decides, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor run loop"); then each schedule failure of step 5 as `OnWorldScheduleFailed`.
9. `m_eToolPosition = FRAME_END`, `OnFrameEnd(frameIndex)`; frame-loop step 3 (`Zenith_DevTools::EndFrame`, the replay writer's `CommitFrame`); `m_eToolPosition = BETWEEN_FRAMES`.
10. A shutdown request of this frame (a world's `Runtime.Quit`, a platform quit, `--exit-after-ticks`, the device-loss clean exit): record the exit code the Player would return (0, 5 when a test assertion was recorded, 6 for the device loss), clear the request and return `false`; otherwise return `true`. After `false` the runtime stays runnable: a further call runs an ordinary frame.

`GetRequestedExitCode` returns the code recorded at step 1 or 10, `EXIT_CODE_NORMAL` before any request.

**Worlds.** `CreateWorld` calls `Zenith_WorldHost_CreateWorld` with `{ m_ulWorldSeed, m_eOrigin, m_bIsolated = false, m_bInputWorld = false, m_ePurpose }`, where the origin is `WORLD_ORIGIN_EDITOR_PLAY` for `WORLD_PURPOSE_PLAY` and `WORLD_ORIGIN_STARTUP` for `WORLD_PURPOSE_EDIT` and `WORLD_PURPOSE_PREVIEW` (row ED37; the purpose, never the origin, tells modules what the world is for). For `WORLD_PURPOSE_EDIT` the ECS world desc also carries the edit-world flag ECS defines for "Schema revision in the edit world" (`Zenith2/Docs/Design/ECS/Backends.md`). At most 64 tool worlds exist at once (the runtime state's tool-world set); a 65th `CreateWorld` is `RUNTIME_ERROR_WORLD_CREATE` with `m_uModuleIndex` 0xFFFF. A failure returns `RUNTIME_ERROR_WORLD_CREATE` with the failing module's index, everything unwound. The world joins the schedule at the next prologue step 8 (a structural trigger); it is no input world until `SetInputWorld`. `DestroyWorld(tag)` (a tag `CreateWorld` returned; any other is `ZENITH_ASSERT`) is `Zenith_WorldHost_DestroyWorld`, GAME's teardown order, and the world leaves the schedule at the next step 8. `SetInputWorld(0)` routes input to no world; any other tag must be live. The world purpose is constant for the world's life ([Z2-N-ARCH-879]).

**Extractors, modules and schemas.** `AddToolExtractor(tag, xDesc)` is legal only for a world `CreateWorld` returned whose registry step 8 has not yet frozen (`RUNTIME_ERROR_WORLD_SCHEDULED` otherwise; an unknown tag is `RUNTIME_ERROR_UNKNOWN_WORLD`); it registers the extractor on the world's system registry as the attach context's `AddExtractor` does, in a per-world tool journal undone by `DestroyWorld`; a refusal (a name not unique in the world, a null function) is `RUNTIME_ERROR_REGISTRATION`. `FindModuleInstance(name)` returns the started instance whose `MODULE_NAME` equals `name` byte for byte, or null; the pointer is valid until the runtime is destroyed (AUDIO's tools-only accessor, row AU3). `QueueSchemaJson(xJson, strPath)` copies both into the runtime's schema queue: at most `uTOOL_SCHEMA_QUEUE_FILES` = 64 files and `ulTOOL_SCHEMA_QUEUE_BYTES` = 8 MiB of JSON and paths in total; a call that would exceed either is `RUNTIME_ERROR_QUEUE_FULL` and queues nothing; the queue is applied at the next prologue step 6.

The tool host's tests are in § "Seam tests"; `Test_Runtime_ToolAccess_*` boot the fixture composition through `Zenith_RuntimeToolAccess::Create` with a recording `Zenith_ToolFrameHooks`.

## Runtime test seams for modules

`Zenith_ModuleStartContext` and `Zenith_WorldAttachContext` each have `Zenith_FaultInjector* GetFaultInjector()` ([Z2-N-CONTRACT-255]'s interface; core text `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces", X344 (c) applied by core-doc patch f): the runtime's fault injector, which is `Zenith_RuntimeTestHooks::m_pxFaults` when the runtime was created through `Zenith_RuntimeTestAccess::Create` and null for `Zenith_Runtime::Create`, so a module's `Start` and `AttachWorld` fault points are swept by `Zenith_RunFaultSweep` through the real module host (A5 cross-area row StB5).

`static Zenith_Result<Zenith_MountSourceID, Zenith_FileError> Zenith_RuntimeTestAccess::AddTestMountSource(Zenith_Runtime& xRuntime, Zenith_MountKind eMount, Zenith_Owned<Zenith_MountSource> xSource, sint32 iOrder)` and `RemoveTestMountSource(Zenith_Runtime&, Zenith_MountSourceID)` (main thread, no frame running) forward to the runtime's `Zenith_VirtualFileSystem::AddSource` and `RemoveSource` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API"), so an integration suite mounts a test-owned source (ASSET's gated mount source) over a booted runtime's content and holds or delays its I/O; `Zenith_RuntimeTestFixture` (`Tests/Zenith_RuntimeTestFixture.h`) forwards both (A5 cross-area row StB6). A source still mounted at destruction is removed in shutdown step 6 before the file system is destroyed.

`Zenith_RuntimeTestHooks` also gains `using Zenith_PfnShutdownStepHook = void (*)(uint8 uStep, void* pUserData);` as `m_pfnOnShutdownStep`, called on the main thread before each step 1–8 of `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence", so a test attributes what a subsystem does (DEVT's file writer submitting `Zenith_AsyncIO` writes) to the shutdown step it happens in (DEVT's `Test_DevToolsRuntime_ShutdownWritesNothingAfterDetach`).

`Zenith_RuntimeTestHooks` also gains `Zenith_BuildID m_xBuildIDOverride` (all zero means none; A5 cross-area row PkB10). The runtime reads the running build ID once, before startup step 4, into the runtime state's `Zenith_BuildID m_xRunningBuildID`. It takes the override when `m_xBuildIDOverride.IsValid()`, and `Zenith_GetRunningBuildID()` otherwise (`Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header").

- Every runtime reader of the running build takes that member and never calls `Zenith_GetRunningBuildID` itself: step 5's `Zenith_VirtualFileSystemDesc::m_xBuild.m_auBuildID`, step 11's `Zenith_RuntimeSave_CreateSystem` and PACK's bridge.
- `Zenith_Runtime::Create` passes an all-null hook table, so a Player always reports its own image identity.
- The override lives in one runtime's state, never in a global, so one suite binary can boot runtimes with two build identities, one after the other.
- For tests, `static Zenith_BuildID Zenith_RuntimeTestAccess::GetRunningBuildID(const Zenith_Runtime& xRuntime)` returns `m_xRunningBuildID`.

<!-- assumption: pending PLAT (row PK23) -->
`Zenith_RuntimeTestHooks` also gains `Zenith_StringView m_strBasePathOverride` (empty: none; A3 cross-area row PK22 (c)). Startup step 4 copies it into `Zenith_PlatformDesc::m_strBasePathOverride`, PLAT's member of row PK23, so `Zenith_Platform::GetBasePath()` returns it (UTF-8, a trailing `/` added when absent) instead of `SDL_GetBasePath`, and the package mount, the patch mount and PACK's install file system all resolve against it: an integration suite runs a packaged game over a per-test copy of its install tree, generated at test time under `${ZENITH_OUT}/test-fixtures/<AREA>/` ([Z2-N-ARCH-257]). The view's bytes are the test's and outlive the runtime. `Zenith_Runtime::Create` and `Zenith_RuntimeToolAccess::Create` pass an empty view, so no Player or tool overrides its base path. `static Zenith_StringView Zenith_RuntimeTestAccess::GetPlatformBasePath(const Zenith_Runtime& xRuntime)` (main thread) returns the booted runtime's `Zenith_Platform::GetBasePath()`.
<!-- /assumption -->

## Seam tests

All in `Zenith2/Engine/ZenithRuntime/Tests/`, run by `ZenithRuntime_Tests` in the Test step over the fixture composition with the recording fake module (`Tests/FakeModules/`), headless.

| Test | Case it asserts |
|---|---|
| `Test_Runtime_ModuleSink_BoundAtWorldCreation` | the sink exists before the first module attaches and reaches the same table after the table's `Zenith_Owned` moved into a retired entry; a park after the swap is discarded and counted by the retired table, `runtime.async.park_errors` stays 0 (clean under `win-asan`) |
| `Test_Runtime_ModuleSink_DeclarationRules` | each rejection of § "Source declaration and handler registration" fails `AttachWorld` with `MODULE_ERROR_REGISTRATION` and its reason, including a declared kind with no handler; a handler set for a script source the module declared at `Start` is accepted; a kind declared with `DeclareScriptAsyncSource` at `Start` and again with `DeclareSource` at attach is rejected even with identical capacity and size, and so is a second `DeclareSource` of one kind in one world (one declaration point, row UiB7) |
| `Test_Runtime_ModuleSink_AllocationSites` | a merge-chain source allocates in a merge function and refuses from the prologue (`SITE`); a prologue source refuses at capacity (`FULL`, nothing changed) and under `--replay` (`REPLAYING`, counter + 1); a producer holding one reserved slot (`m_uMaxInFlight` 4, three ordinary results outstanding) still allocates its reserved result, and the fifth allocation is `FULL` |
| `Test_Runtime_ModuleSink_MixedKindPrologue` | a script source declared at `Start` whose module set a handler on the sink allocates in the prologue and parks `PUBLICATION_KIND_GENERIC_RECORDED`; the handler sees it at the next tick boundary and the slot is released after it; the same kind without a handler, or allocated from a merge function, is `SITE` (asserts in Debug); under `--replay` it is `REPLAYING`; a recomputed park of it asserts in Debug and counts `runtime.async.park_errors`; script requests of the kind still publish to their tickets |
| `Test_Runtime_ModuleSink_ParkAndForward` | parked results reach the module handler at the next tick boundary in completion-ID order with `m_bScriptOwned` correct; a zero-length recorded payload is accepted; a recomputed park of a prologue source asserts in Debug |
| `Test_Runtime_ModuleSink_ReplayPeekAndLateLocal` | under `--replay` `PeekRecorded` returns the recorded payloads of the kind and tick; a live park of an already published ID is a no-op counted in `runtime.async.late_local_completions` |
| `Test_Runtime_ScenePublicationSink_ForwardsResolver` | the scene sink installs SCENE's resolver on the table it bound, and a late local completion under `--replay` is counted, never asserted |
| `Test_Runtime_AttachContext_Lookups` | `FindCommandKind` returns the sealed table's ID (0 for an unknown name), `GetWorldControl` the record's control, `GetSimulationClock` the step-9 clock, and `GetWorldPurpose` PLAY for the startup world and EDIT and PREVIEW for `CreatePurposedWorld` worlds |
| `Test_Runtime_TestAssertion_FromWorkerThread` | `RecordTestAssertionFailure` called from a job worker during `RunFrame` makes the run exit 5; a world shutdown request stops the run after the current iteration |
| `Test_Runtime_WorldHost_CreateAndTeardownWalk` | `Zenith_WorldHost_CreateWorld` creates a second world through steps 1–8 and unwinds on an injected failure; `m_pfnOnTeardownStep` observes `axWORLD_TEARDOWN_ORDER` in order |
| `Test_Runtime_PrecompileSwap_TrialAcquire` | a successful precompile makes step 8's rebuild a cache hit; a failing one changes neither the cache nor the running schedule; `GetGameFlowState` reads the game state |
| `Test_Runtime_StartContext_FrameListenersForwarded` | two modules' listeners receive `OnStopAudio`, `OnResumeAudio` and `OnPlatformEvents` in canonical module order; a second registration by one module fails its `Start`; a failed start undoes the first module's listener |
| `Test_Runtime_StartContext_EngineEpilogueStage` | a module's engine stage compiles once per schedule, headless with no world and after `<Stage>@W<tag>` of every world that has it; a second stage of the module, or a stage writing a world resource, fails startup with exit code 3 |
| `Test_Runtime_StartContext_ScriptConsumersAndSources` | an audio-trigger consumer and a declared script source reach the binding table and the sink's ownership; a module's save-request consumer, a duplicate source and an oversize source are rejected and undone |
| `Test_Runtime_TestSeams_FaultInjectorAndMountSource` | a fake module's `Start` and `AttachWorld` fault points, swept through `GetFaultInjector`, each fail with the module's error and unwind; a test mount source added after startup serves `content:/` reads with its precedence and is removed cleanly; `m_pfnOnShutdownStep` observes steps 1–8 in order; `Zenith_Runtime::Create` gives a null injector |
| `Test_Runtime_StartContext_ProfilerOverlay` | a development build returns the overlay and a provider registered in `Start` is undone with the module; a Shipping-configuration build returns null |
| `Test_Runtime_StartContext_RecordingAndExitAfterStart` | `IsRecordingReplay` is true in `Start` under `--record` and false without it; a module calling `RequestExitAfterStart` makes `Zenith_RuntimeMain` return 0 with no `RunFrame` (the executor's frame count stays 0) and log the requester; a later module's `Start` failure keeps exit code 3 or 4; a second request changes nothing |
| `Test_Runtime_StartContext_EntitlementProvider` | a module's `ProvideEntitlementQuery` in `Start` records the provider and journals it; a second provider is rejected with the first owner's name and fails that `Start`; a provider whose module's `Start` fails later is undone; module destruction undoes it; `GetEntitlementProviderModule` names the providing module after its `Start` and is 0xFFFF after each undo, and the query object is never called by the runtime itself |
| `Test_Runtime_StartContext_StringTable` | a module's `Start` gets the step-10 string table (the type registry's bound table, the same address); a string it interns at `Start` resolves through that table after startup; the call journals nothing |
| `Test_Runtime_AttachContext_TickBoundaryHandler` | a handler added in `AttachWorld` is called at the world's tick-boundary steps in order; a handler added outside `AttachWorld` fails that call with `MODULE_ERROR_REGISTRATION` and never reaches ECS; a later module's failed attach destroys the world with its handler and leaks nothing |
| `Test_Runtime_CaptureTickGate_HoldsOneTick` | with `sim.ticksPerFrame` 1, `HoldNextTick` between frames makes exactly the next frame advance no tick on every world, and the frame after advances one; two calls hold one frame; in a Shipping-configuration build, or with `sim.ticksPerFrame` 2, `GetCaptureTickGate` is null |
| `Test_Runtime_StartContext_ScriptAsyncCapacity` | `GetScriptAsyncCapacity(ASYNC_SOURCE_PATHFINDING)` in `Start` equals the path value step 16 gives every script world's `m_auAsyncInFlight[0]`; kinds 1 to 4 return their four values, kind 0 and kinds 5 to 15 return 0; the call journals nothing |
| `Test_Runtime_Step15_EngineRegistrationWindow` | `m_pfnOnEngineRegistrations` is called twice in step 15 (kinds, then bindings) before any module's `Start`; a kind and a binding namespace it registers are present after startup (`FindCommandKind` non-zero, the namespace sealed with the rest); a hook returning false, or registering a kind name a module registers later, fails startup with exit code 3 and unwinds |
| `Test_Runtime_FrameLoop_AfterRunFramePosition` | `m_pfnAfterRunFrame` is called once per frame with the frame's index, after `RunFrame` returned (the frame executor's frame count already advanced) and before `Zenith_DevTools::EndFrame` (the profiler frame not yet closed); a run of three frames calls it three times in index order |
| `Test_Runtime_TestHooks_BuildIDOverride` | a runtime created with `m_xBuildIDOverride` = A reports A from `GetRunningBuildID` and from the virtual file system's `GetBuildIdentity().m_auBuildID`; a second runtime created after it in the same process with B reports B; without the override both report `Zenith_GetRunningBuildID()` |
| `Test_Runtime_TestHooks_BasePathOverride` | a runtime created with `m_strBasePathOverride` = a directory the test created under `${ZENITH_OUT}/test-fixtures/MOD/` reports it, with a trailing `/`, from `GetPlatformBasePath`; without the hook it reports `SDL_GetBasePath`'s value |
| `Test_RuntimeToolAccess_HeaderTypesAreValues` | (`Tests/Zenith_RuntimeToolAccess_Tests.cpp`) the five tool-access value types are trivially copyable with the declared defaults (purpose EDIT, failed file index 0xFFFFFFFF, null pointers), the five new error codes have their `Zenith_ToString` names, and the API slice's refusing `Create` returns `RUNTIME_ERROR_STARTUP_STEP` with nothing allocated |
| `Test_Runtime_ToolAccess_CreateWithoutWorld` | `Zenith_RuntimeToolAccess::Create` completes steps 4–18 with no world and no startup scene (`m_eLastCompletedStep` 18), fills every non-optional `Zenith_ToolServices` member, and passes the crash identity and the console thunk to steps 7 and 14; an injected step failure unwinds and returns the step's exit code |
| `Test_Runtime_ToolAccess_FrameHookOrder` | one `RunOneFrame` calls `OnInputFrame`, `OnPrologueEnd` and `OnFrameEnd` in that order around `RunFrame`, `OnSchemaWindowResult` only when a schema file was queued, and returns true; a quit request returns false with `GetRequestedExitCode` 0 and the next call runs a frame; a module's `RequestExitAfterStart` makes the first call return false with no frame |
| `Test_Runtime_ToolAccess_WorldsAndExtractors` | `CreateWorld` of each purpose returns a world whose modules see that purpose and which joins the schedule at the next step 8; `AddToolExtractor` before that step is scheduled and after it is `RUNTIME_ERROR_WORLD_SCHEDULED`; `DestroyWorld` walks GAME's teardown order; `SetInputWorld(0)` routes input to no world; a world whose step-8 freeze fails is destroyed and reported once through `OnWorldScheduleFailed` |
| `Test_Runtime_ToolAccess_SchemaQueueAndPositions` | two queued schema files apply in one step-6 window and report `m_bApplied`; a refused second file abandons the window with `m_uFailedFileIndex` 1 and the registry revision unchanged; a 65th file is `RUNTIME_ERROR_QUEUE_FULL`; `FindModuleInstance` finds a started fake module by name and returns null for another; a halted script world is reported through `OnWorldHalted` and ends no run |
| `Test_Runtime_ToolAccess_IllegalPosition_Checks` | (`Tests/Zenith_Runtime_DeathTests.cpp`, suite `z2-death-check`) `CreateWorld` called from `OnPrologueEnd` fails its position `ZENITH_CHECK` |
