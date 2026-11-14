# SAVE design — runtime bridge

This file is part of the SAVE design family. The entry document is `Zenith2/Docs/Design/SAVE.md`; the public declarations of `ZenithSave` are `Zenith2/Docs/Design/SAVE/PublicAPI.md`. It holds the declarations SAVE owns inside `Zenith2/Engine/ZenithRuntime/Internal/Save/` (the file list and stages are `Zenith2/Docs/Design/SAVE.md` → "Runtime bridge files").

## Scope of this file

- The Stage 2 replay core (`Zenith_RuntimeReplayCore.h`), the Stage 3 save bridge (`Zenith_RuntimeSave.h`) and the Stage 3 script replay adapters (`Zenith_RuntimeReplayScript.h`), each in full.
- The bridge participants `Zenith.Script.States` and `Zenith.Scene.AdditiveSet` and their fidelity and re-issue rules.
- The synchronous restore driver used outside a frame (the resume at startup, seeks and a replay's restored sessions).

## Runtime bridge declarations

SAVE writes these files inside `ZenithRuntime` (layer 6), the lowest library that sees both `ZenithSave` and `ZenithScript` (layer 5 each, [Z2-N-ARCH-072]); `ZenithRuntime` owns the directory, SAVE owns the files (`Zenith2/Docs/Design/SAVE.md` → "Runtime bridge files"). They are `Internal/` declarations of `ZenithRuntime`, not public types, so `ApiContractCheck` does not read them; each states its affinity in a comment. Every bridge function that can fail returns a `Zenith_Result`, mapped by the composition root as `Zenith2/Docs/Design/SAVE.md` → "Startup and shutdown" tabulates.

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Save/Zenith_RuntimeReplayCore.h  (internal to ZenithRuntime; Stage 2)
#pragma once
#include "ZenithSave/Zenith_ReplayLog.h"
#include "ZenithECS/Zenith_ECSObservers.h"                  // Zenith_TickBoundaryHandler, Zenith_TickBoundaryAccess
#include "ZenithPlatform/Zenith_InputCommand.h"
#include "ZenithCore/Zenith_Hash.h"                  // Zenith_Hash64Stream

struct Zenith_RuntimeState;

// --record (Stage 2 core): WORLD_BEGIN / WORLD_SWAP, each tick's input commands and its hash at the next tick boundary.
// Main thread for BeginWorld/SwapWorld/Commit; OnTickBoundary on the world's boundary node; SetTickInputCommands on phase 1.
class Zenith_RuntimeReplayCoreRecorder final : public Zenith_TickBoundaryHandler
{
public:
	Zenith_RuntimeReplayCoreRecorder(Zenith_RuntimeState& xState, Zenith_ReplayLogWriter& xWriter);
	void OnTickBoundary(Zenith_TickBoundaryStep eStep, uint64 ulTickStarting, Zenith_TickBoundaryAccess& xWorld) override;
	void RecordTickInputCommands(uint16 uWorldTag, uint64 ulTick, Zenith_Span<const Zenith_InputCommand> axCommands);
	// Stage 3 adds the SCRIPT checkpoint contribution through this hook (null before Stage 3: the hash is HashTickState alone).
	using Zenith_PfnTickHashContribution = void (*)(uint16 uWorldTag, uint64 ulTick, Zenith_Hash64Stream& xHash, void* pUser);
	void SetHashContribution(Zenith_PfnTickHashContribution pfnContribute, void* pUser);
	Zenith_ReplayLogWriter& GetWriter();
};

// --replay (Stage 2 core): input commands from the log, the clock cap, hash verification at each tick boundary.
class Zenith_RuntimeReplayCorePlayer final : public Zenith_TickBoundaryHandler
{
public:
	Zenith_RuntimeReplayCorePlayer(Zenith_RuntimeState& xState, const Zenith_ReplayLogReader& xReader, Zenith_ReplayVerifier& xVerifier);
	void OnTickBoundary(Zenith_TickBoundaryStep eStep, uint64 ulTickStarting, Zenith_TickBoundaryAccess& xWorld) override;
	Zenith_Span<const Zenith_InputCommand> GetTickInputCommands(uint16 uWorldTag, uint64 ulTick) const;
	uint64 GetRemainingTicks(uint16 uWorldTag) const;   // the clock caps T with it
	bool IsFinished() const;                             // every session replayed
	void SetHashComparison(uint16 uWorldTag, bool bCompare);          // false after a resume from a COHERENT save
	void SetHashContribution(Zenith_RuntimeReplayCoreRecorder::Zenith_PfnTickHashContribution pfnContribute, void* pUser);
	const Zenith_ReplayLogReader& GetReader() const;
};

// MAIN_THREAD, startup step 11: creates the core recorder (--record) or player (--replay) and registers it as each new
// world's first tick-boundary handler at attach. A reader or writer failure is the composition root's exit code 4; an
// identity mismatch exit code 5.
Zenith_Result<void, Zenith_ReplayError> Zenith_RuntimeReplayCore_Create(Zenith_RuntimeState& xState);
```

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Save/Zenith_RuntimeSave.h  (internal to ZenithRuntime; Stage 3)
#pragma once
#include "ZenithSave/Zenith_SaveSystem.h"
#include "ZenithScript/Zenith_ScriptHost.h"
#include "ZenithScript/Zenith_ScriptAsync.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"

struct Zenith_RuntimeState;

// The ASYNC_SOURCE_SAVE completion sink: allocates C++ requests' IDs (script requests' IDs are the consumer's) and completes
// results into the world's
// Zenith_AsyncResultTable (writer thread, the world's merge chain and its phase-11 node; SaveSystem.h's sink contract).
// Replay mode: the writer completes nothing, but intake refusals and phase-11 completions still call Complete, and SCRIPT
// discards each such live completion because the recorded publication of the same ID is the one it publishes
// (Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"). An unbound world's completions are dropped.
class Zenith_RuntimeSaveCompletionSink final : public Zenith_SaveCompletionSink
{
public:
	explicit Zenith_RuntimeSaveCompletionSink(Zenith_RuntimeState& xState);
	uint64 AllocateCompletionID(uint16 uWorldTag) override;
	void Complete(uint16 uWorldTag, uint64 ulCompletionID, Zenith_ByteSpan xPayload) override;
	// Completes the ASYNC_SOURCE_WORLD_LOAD ID with GAME's 8-byte failure payload WORLD_TRANSITION_FAILURE_PREPARE.
	void CompleteLinkedLoadRefused(uint16 uWorldTag, uint64 ulLinkedCompletionID) override;
};

// SIDE_EFFECT_KIND_SAVE_REQUEST consumer (merge node): decodes the script payload and calls Zenith_SaveSystem::QueueRequest.
// SCRIPT's commit passes ulCompletionID = 0 for every SAVE_REQUEST record (Design/SCRIPT/Transactions.md "Commit
// implementation" step 2), so for operations 1-4 the consumer allocates the request's ID itself on this merge node, in merge
// order ([Z2-N-SCRIPT-341] admits the merge chain): m_ulCompletionID = GetAsyncResults().AllocateCompletionID(ASYNC_SOURCE_SAVE);
// for 0x80 and 0x81 it allocates nothing. An allocation of 0 is the counted defect save.requests.unpublishable (logged, the
// request dropped; unreachable by SAVE.md "Asynchronous source capacity"). For a LOAD (Stage 4) it then, right after the SAVE
// ID, allocates the requesting world's ASYNC_SOURCE_WORLD_LOAD completion ID on the same merge node
// (GetAsyncResults().AllocateCompletionID) into m_ulLinkedCompletionID, so both IDs are allocated at intake in merge order.
// When QueueRequest returns SAVE_ERROR_WORLD_NOT_BOUND, the consumer completes the IDs it allocated through the world's
// table itself (the 64-byte header with that code; a LOAD's linked ID with WORLD_TRANSITION_FAILURE_PREPARE). That source's capacity is 1 (GAME declares
// DeclareSource(ASYNC_SOURCE_WORLD_LOAD, 1, 8)), so the allocation returns 0 whenever the world already has a transition or
// another Save.Load in flight (two Save.Load calls in one tick are within Z2-B-SCRIPT-025); the consumer then passes
// m_ulLinkedCompletionID = 0 and intake completes the SAVE ID at once with SAVE_ERROR_TRANSITION_BUSY (Operation 4),
// queueing nothing (Algorithms.md "Request intake" step 9). Both allocations are functions of the world's async state, which a
// replay reproduces, so a replay allocates the same IDs and refuses the same requests.
class Zenith_RuntimeSaveRequestConsumer final : public Zenith_ScriptSideEffectConsumer
{
public:
	explicit Zenith_RuntimeSaveRequestConsumer(Zenith_RuntimeState& xState);
	void Consume(uint16 uWorldTag, Zenith_SideEffectKind eKind, uint64 ulCompletionID, Zenith_ByteSpan xPayload) override;
};

// ASYNC_SOURCE_SAVE publication handler (tick boundary node, step 1): appends each published result to the world's
// bridge-side result list (Zenith_RuntimeSaveResultList, capacity uSaveMaxOutstanding entries of 16 KiB) and calls
// Zenith_SaveSystem::OnResultPublished; it writes no world state, so it needs nothing from Zenith_TickBoundaryAccess.
// The phase-1 system "Save.EmitResults" emits the Zenith.Save* events from the list in completion-ID order and clears it.
class Zenith_RuntimeSavePublicationHandler final : public Zenith_AsyncPublicationHandler
{
public:
	explicit Zenith_RuntimeSavePublicationHandler(Zenith_RuntimeState& xState);
	void OnPublished(const Zenith_ScriptPublication& xPublication, Zenith_TickBoundaryAccess& xWorld) override;
};

// The participants the bridge owns, one instance of each per world.
// "Zenith.Script.States", REQUIRED. CaptureSection writes every entry of GetStateSnapshot() with its three flags
// (m_bRunSuspended, m_bRecoveryPending, m_bQuarantinedOrDisabled; Formats.md "Participant section framing"), an entry with no
// committed checkpoint as a zero-length blob; RestoreSection rebuilds each Zenith_ScriptSavedState with its flags and blob and
// passes the span to RestoreSavedStates, whose step 6 decides each instance's lifecycle from the flags (PENDING_RECOVER only for
// a behaviour declaring OnRecover whose entry carries a flag; ACTIVE or DORMANT with no recovery invocation otherwise).
// GetCaptureFidelity: COHERENT when at least one captured entry has m_bRunSuspended (a restore never restores an execution
// position, so Run restarts from its recorded phase, [Z2-N-SCRIPT-393], [Z2-N-SCRIPT-395]) or m_bQuarantinedOrDisabled (the
// restore makes a recovery the uninterrupted run did not make: module disables and recovery counters are not saved); EXACT
// otherwise, m_bRecoveryPending included (the pending recovery runs at the same tick under the reserved recovery ordinal, which
// consumes no invocation ordinal), per the SCRIPT rules of the paragraph below. GetReissuedSourceKinds: 0.
class Zenith_ScriptStateSaveParticipant final : public Zenith_SaveParticipant
{
public:
	Zenith_ScriptStateSaveParticipant(Zenith_ScriptWorld& xScripts, Zenith_StringTable& xStrings);
	const char* GetName() const override;
	uint16 GetSectionVersion() const override;           // 1
	Zenith_SaveParticipantFlags GetFlags() const override; // uSAVE_PARTICIPANT_FLAG_REQUIRED
	void DeclareAccess(Zenith_SystemBuilder& xSnapshotSystem) override;   // a read of the script world resource
	Zenith_Result<void, Zenith_SaveError> CaptureSection(const Zenith_SaveCaptureContext& xContext,
		Zenith_SaveSectionWriter& xWriter) override;
	Zenith_SaveFidelity GetCaptureFidelity(const Zenith_SaveCaptureContext& xContext) const override;
	Zenith_Result<void, Zenith_SaveError> RestoreSection(const Zenith_SaveRestoreContext& xContext,
		Zenith_SaveSectionReader& xReader) override;
private:
	bool m_bLastCaptureNotExact = false;          // set by CaptureSection (an entry with m_bRunSuspended or
	                                              // m_bQuarantinedOrDisabled), read by GetCaptureFidelity
};
// "Zenith.Scene.AdditiveSet", REQUIRED. GetCaptureFidelity: COHERENT when an instance was LOADING or QUEUED at the
// capture (its load completes at an I/O-dependent tick), EXACT otherwise. GetReissuedSourceKinds: bit ASYNC_SOURCE_SCENE_LOAD
// (8): the section records each LOADING or QUEUED instance's load completion ID, and RestoreSection re-requests the load
// through SCENE's RestoreInstances under that recorded ID (row SV4 (e)) and marks it re-issued, so under a publication source
// (a seek, a replay's restored session) the recorded SCENE_LOAD publication completes the very request the restored set
// waits for; an ordinary load gets the live completion of the same ID.
class Zenith_AdditiveScenesSaveParticipant final : public Zenith_SaveParticipant
{
public:
	Zenith_AdditiveScenesSaveParticipant(Zenith_AdditiveSceneSet& xSet, Zenith_SceneLoader& xLoader);
	// members as Zenith_ScriptStateSaveParticipant plus GetReissuedSourceKinds, section version 1
private:
	bool m_bLastCaptureHadPendingLoad = false;
};

// MAIN_THREAD, startup step 11, after the codec set (exit code 4 on failure).
Zenith_Result<void, Zenith_SaveError> Zenith_RuntimeSave_CreateSystem(Zenith_RuntimeState& xState);
// MAIN_THREAD, step 14 (exit code 3 on failure).
Zenith_Result<void, Zenith_SaveError> Zenith_RuntimeSave_RegisterBindings(Zenith_RuntimeState& xState,
	Zenith_ScriptBindingTable& xTable);
// MAIN_THREAD, step 16 / a transition's world creation, after every module attached; uPredecessorTag 0 for a startup world.
Zenith_Result<void, Zenith_SaveError> Zenith_RuntimeSave_AttachWorld(Zenith_RuntimeState& xState, uint16 uWorldTag,
	uint16 uPredecessorTag);
void Zenith_RuntimeSave_DetachWorld(Zenith_RuntimeState& xState, uint16 uWorldTag);
// MAIN_THREAD, prologue step 7: Zenith_SaveSystem::PumpMainThread, then (Stage 4) the finished script loads.
void Zenith_RuntimeSave_PumpMainThread(Zenith_RuntimeState& xState);
// MAIN_THREAD after the last RunFrame returned (RHI's device-loss clean exit): SubmitExitAutosave (which preempts the running
// ordinary write); shutdown step 4 waits for the returned operation at most uSAVE_EXIT_AUTOSAVE_BUDGET_MS from this call.
Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> Zenith_RuntimeSave_RequestAutosaveForExit(
	Zenith_RuntimeState& xState);
// MAIN_THREAD between frames: the thermal RequestCheckpointSave, a CHECKPOINT through SubmitOutOfTick (no completion ID).
Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> Zenith_RuntimeSave_RequestCheckpoint(
	Zenith_RuntimeState& xState);
// MAIN_THREAD, outside any frame: § "Synchronous restore driver".
Zenith_Result<void, Zenith_SaveError> Zenith_RuntimeSave_RestoreSynchronously(Zenith_RuntimeState& xState,
	Zenith_WorldRestoreJob& xJob, uint32 uTimeoutMs);
void Zenith_RuntimeSave_BeginShutdown(Zenith_RuntimeState& xState);            // shutdown step 4
void Zenith_RuntimeSave_Destroy(Zenith_RuntimeState& xState);                  // shutdown step 7b, before Zenith_AsyncIO
```

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Save/Zenith_RuntimeReplayScript.h  (internal to ZenithRuntime; Stage 3)
#pragma once
#include "Zenith_RuntimeReplayCore.h"
#include "ZenithScript/Zenith_ScriptHost.h"

// --record: implements SCRIPT's publication observer and forwards each tick's publications to the core recorder's writer.
class Zenith_RuntimeReplayPublicationObserver final : public Zenith_ScriptPublicationObserver
{
public:
	explicit Zenith_RuntimeReplayPublicationObserver(Zenith_RuntimeReplayCoreRecorder& xRecorder);
	void OnPublished(uint16 uWorldTag, uint64 ulTick, Zenith_Span<const Zenith_ScriptPublication> axPublished) override;
	void OnWorldHalted(uint16 uWorldTag, uint64 ulTick);   // forwarded by the runtime's Zenith_ScriptWorldListener
};

// --replay: implements SCRIPT's publication source over the core player's reader.
class Zenith_RuntimeReplayPublicationSource final : public Zenith_ScriptPublicationSource
{
public:
	explicit Zenith_RuntimeReplayPublicationSource(const Zenith_RuntimeReplayCorePlayer& xPlayer);
	Zenith_Span<const Zenith_ScriptPublication> GetRecorded(uint16 uWorldTag, uint64 ulTick) override;
};

// MAIN_THREAD, step 14 (Stage 3): attaches the observer or source and the SCRIPT hash contribution to the core objects.
Zenith_Result<void, Zenith_ReplayError> Zenith_RuntimeReplayScript_Attach(Zenith_RuntimeState& xState);

enum Zenith_ReplaySeekStrategy : uint8 { REPLAY_SEEK_STRATEGY_CHECKPOINT = 1, REPLAY_SEEK_STRATEGY_FULL = 2 };
struct Zenith_ReplaySeekResult
{
	Zenith_ReplaySeekStrategy m_eStrategy;
	uint64 m_ulTicksRun;
	uint64 m_ulElapsedUs;
	bool m_bHashesCompared;                   // false only after a resume from a COHERENT save
	bool m_bVerified;                         // every compared tick's hash matched
};
// Development builds: rebuild the session's world at tick ulTargetTick (Algorithms.md "Replay seeking"); MAIN_THREAD.
Zenith_Result<Zenith_ReplaySeekResult, Zenith_ReplayError> Zenith_RuntimeReplay_SeekTo(Zenith_RuntimeState& xState,
	uint32 uSessionOrdinal, uint64 ulTargetTick);
// Development builds: restore xSave into the session its header position names and continue the replay from there
// ([Z2-N-ARCH-789]); hashes are compared only for an EXACT save (Algorithms.md "Resuming a replay from a save"); MAIN_THREAD.
Zenith_Result<Zenith_ReplaySeekResult, Zenith_ReplayError> Zenith_RuntimeReplay_ResumeFromSave(Zenith_RuntimeState& xState,
	Zenith_PreparedSave& xSave, uint64 ulTargetTick);
// MAIN_THREAD, at the swap to a world restored from a save (after SwapWorld and BeginWorld): captures the restored world out
// of tick into a one-world SAVE_KIND_REPLAY_CHECKPOINT image and calls AddCheckpoint (Formats.md "Restored-world convention").
void Zenith_RuntimeReplay_LogRestoredWorld(Zenith_RuntimeState& xState, uint16 uWorldTag, uint64 ulRestoredTick);
```

The rules of `Zenith.Script.States` above rest on orchestrator ruling Q82 (b) as delivered by SCRIPT's row SC25 (cross-area rows SV23 and SC25), core text since cross-doc row X267 ([Z2-N-SCRIPT-274] and [Z2-N-SCRIPT-928] in `Zenith2/Docs/Scripting.md` → "Ordering keys"; [Z2-N-SCRIPT-394], [Z2-N-SCRIPT-929] and [Z2-N-SCRIPT-930] in `Zenith2/Docs/Scripting.md` → "OnRecover"), designed in `Zenith2/Docs/Design/SCRIPT/HotReload.md` → "Recovery invocations and exact restore" (steps 1–7) and implemented by ticket Z2T-SCRIPT-114: (a) the recovery invocation of a restored instance (`OnRecover`, [Z2-N-SCRIPT-394]) runs under the reserved ordinal `uSCRIPT_RECOVERY_ORDINAL`, consumes no invocation ordinal of [Z2-N-SCRIPT-274] and records, draws and merges under it, so an instance's other invocations keep the merge keys of the uninterrupted run; (b) `Zenith_ScriptSavedState` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptStateSnapshot header") carries three flags read in phase 11 — `bool m_bRunSuspended` (the instance's `Run` coroutine exists and has yielded without returning), `bool m_bRecoveryPending` (the instance is `PENDING_RECOVER`) and `bool m_bQuarantinedOrDisabled` (the instance is QUARANTINED, or its module is disabled or flagged to disable at the next tick boundary) — and `RestoreSavedStates` step 6 decides each restored instance's lifecycle from them; (c) `GetStateSnapshot` holds every instance past NEW, an instance with no committed checkpoint carrying an empty `m_xCheckpoint` that restores its declared defaults (so it never runs `OnCreate` again), which the section stores as a zero-length blob. SAVE's ticket that implements the section's flags byte and `Zenith_ScriptStateSaveParticipant::GetCaptureFidelity` is blocked by Z2T-SCRIPT-114; no interim rule ships, and until that ticket lands nothing in SAVE claims EXACT for a world with behaviour state. SCRIPT's `Test_Script_State_RestoreWithoutSuspendedRunIsExact` and `Test_Script_State_RestoreSuspendedRunRecovers` are part of the evidence of SAVE's fidelity and behaviour-state requirements.

## Runtime test access for saves

<!-- elaboration -->
Integration suites restore a slot into a booted runtime and carry a slot from one runtime boot to the next in the same process (A5 cross-area row StB23). SAVE adds two members to MOD's `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h`, as its other test entries do:

```cpp
// Zenith_RuntimeTestHooks gains (null: the production storage over PLAT):
using Zenith_PfnCreateSaveStorage = Zenith_Result<Zenith_Owned<Zenith_SaveStorage>, Zenith_SaveError> (*)(Zenith_Heap& xHeap,
	void* pUserData);
//	Zenith_PfnCreateSaveStorage m_pfnCreateSaveStorage = nullptr;
//	void* m_pSaveStorageUserData = nullptr;

// Zenith_RuntimeTestAccess gains (MAIN_THREAD, between frames):
static Zenith_Result<void, Zenith_SaveError> RestoreSaveIntoWorld(Zenith_Runtime& xRuntime,
	Zenith_StringView strSlot);
```

- `Zenith_RuntimeSave_CreateSystem` calls `m_pfnCreateSaveStorage(heap, m_pSaveStorageUserData)` when it is set, instead of `Zenith_CreateAsyncIOSaveStorage`, and maps a refusal to step 11's exit code 4. A test passes a factory that wraps one store it owns, such as the header-only `Zenith_MemorySaveStorage` over a test-owned byte store, so two runtimes created one after the other read and write the same slots. The storage is still owned by each runtime's save system; the store behind it is the test's. No global holds either.
- `RestoreSaveIntoWorld` issues, for the primary world, the load a C++ `Save.Request` command of operation LOAD issues. The entry stores the request in the primary world's binding as one pending host request; the first sync point of the next tick plays it back through the `Save.Request` playback function, after that sync point's recorded commands in merge order, so the completion ID and the linked world-load ID are allocated in-tick at intake exactly as for a command. The following frames then run the ordinary load path (read, verify, migrate, GAME's `Zenith_WorldHost_BeginTransitionFromSave`, the restore, the swap). It returns success once stored; the intake's refusals (`SAVE_ERROR_TRANSITION_BUSY`, `SAVE_ERROR_QUEUE_FULL`, `SAVE_ERROR_SHUTTING_DOWN`) reach the caller as the load's `Zenith.SaveCompleted` error, as for a command. A second call before the first is played back returns `SAVE_ERROR_QUEUE_FULL`. The caller runs frames until the primary world's tag changes or the load's `Zenith.SaveCompleted` reports an error, so the test exercises exactly the load a game performs. The call is refused with `SAVE_ERROR_INVALID_DESC` while a frame runs or with no primary world bound.
- `Test_SaveRuntime_TestStorageSharedAcrossBoots` saves a slot in one runtime, destroys it, boots a second over the same store and lists the slot. `Test_SaveRuntime_RestoreSaveIntoWorld` restores it into the second runtime's primary world, whose tick counter and a participant's state equal the saved values after the swap. `Test_SaveRuntime_RestoreSaveIntoWorldRefusals` covers a busy transition and a call with no primary world.

## Synchronous restore driver

<!-- elaboration -->
A restore that must finish outside a frame — the resume at startup step 17, `Zenith_RuntimeReplay_SeekTo`, `Zenith_RuntimeReplay_ResumeFromSave` and a replay's restored session (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay playback" step 3) — runs `Zenith_RuntimeSave_RestoreSynchronously(state, job, uTimeoutMs)` on the main thread, with no frame running and the job system running. A load-from-save transition does not use it: GAME steps its job in prologue step 7, and the frame's own `Assets.Service` node advances whatever the participants wait for.

1. Record the start time (`Zenith_GetMonotonicTimeUs`).
2. Loop: `xJob.Step()`; an error returns it (the caller destroys the world); DONE returns success.
3. While the state is `WORLD_RESTORE_STATE_PARTICIPANTS_READY`: `n = xJob.ListRestoreWaits(span of 64)`; for each listed handle call `ServiceUntilComplete(*handle, uSliceMs)` on the runtime's own `Zenith_AssetLoader` instance (the one `Zenith_RuntimeState` owns, the loader that issued the participants' loads) with `uSliceMs = min(16, remaining)` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetLoader header"; legal here because the main thread outside a frame is not a job worker, `Zenith2/Docs/Design/ASSET/Loading.md` → "Tool and boot servicing"). The result is handled: `ASSET_ERROR_TIMEOUT` is the normal end of a slice whose operation is still running and is ignored (the handle is serviced again in the next pass); any other result — success, the operation's own failure or `ASSET_ERROR_INVALID_REQUEST` for a handle that is not this loader's — means "that wait is done" for this pass, and the participant alone decides readiness at the next `Step`. When no listed handle consumed a slice (none listed, or every call returned at once), `Zenith_YieldThread()` once, so the loop never spins without servicing or yielding; then `Step` again.
4. When `uTimeoutMs` has elapsed with the job not DONE: log `save: restore of world <tag> not ready after <ms> ms (participant <key>)` naming `GetFirstUnreadyParticipantKey()`, and return `SAVE_ERROR_RESTORE` with `m_ulDetail` = that key.

Timeouts: the resume uses `runtime.startupLoadTimeoutMs` (MOD's key; a failure is PLAT's `RESUME_DISCARD_REASON_RESTORE_FAILED`, `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Resume save and restore"); seeks, resumed replays and restored replay sessions use the same key (a failure is `REPLAY_ERROR_CHECKPOINT`). Cost: the restore's own work plus the waited loads; the loop never spins without servicing or yielding.
