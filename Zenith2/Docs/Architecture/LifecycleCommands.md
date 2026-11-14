# Architecture — Lifecycle commands

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Lifecycle commands". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 541–589. The phases, sync points and playback steps these commands publish at are specified in `Zenith2/Docs/Architecture/FramePhases.md`.

## Settled rules and ownership

<!-- elaboration -->
The lifecycle commands are part of the frame-phase contract (Z2-D-080), owned by the GAME area. Each command has a defined publication point and its own tests, and five cross-system scenario tests exercise them together. Spawn and destroy are `Zenith_CommandBuffer` members (`ZenithECS`); every other lifecycle command is a free recording function in the library that owns its payload types — `ZenithScene` (teleport), the Physics module (ragdoll), the Cinematics module (cinematics) — which records a registered command kind through `Zenith_CommandBuffer::Record`, except prefab instantiation (`Zenith_RecordInstantiatePrefab`, ZenithScene), which expands at record time into ordinary spawn records and plays at sync-point step 6 like any spawn; world transition and shutdown are `ZenithRuntime` requests; the systems that react to them — physics, animation, cinematics, rendering, streaming — implement `Zenith_LifecycleListener`, an interface declared low in the stack by `ZenithScene`, so every notification points downward (Zenith2/Docs/Contracts.md → "Layering DAG").

- [Z2-N-ARCH-541] Every lifecycle command is recorded into a command buffer (or, for world transition and shutdown, requested from the runtime), is applied at the publication point of the table "Lifecycle command summary", and never takes effect immediately at the call.
- [Z2-N-ARCH-542] Lifecycle commands are dispatched to listeners at step 7 of sync-point playback, in merge order (except that the records of a kind registered with `COMMAND_KIND_FLAG_PLAY_LAST` play after every other step-7 record of the playback, in merge order among themselves), on the sync-point node of the world, so every listener sees the same sequence on every run.

## Lifecycle command summary

| Command | Recorded through | Publication point | First observer | Test |
|---|---|---|---|---|
| Spawn | `Zenith_CommandBuffer::Spawn`, `Zenith_RecordInstantiatePrefab` | sync point after the issuing phase (playback step 6) | every node of the next phase | `Test_Lifecycle_Spawn_VisibleToNextPhase` |
| Destroy | `Zenith_CommandBuffer::Destroy` | marked at the sync point after the issuing phase; removed at the end-of-tick playback | handles are stale from the next tick | `Test_Lifecycle_Destroy_DeferredToTickEnd` |
| Teleport | `Zenith_RecordTeleport` (ZenithScene) | sync point after the issuing phase (step 7) | the next phase of the same tick (issued in phases 1–10; the render histories reset at the frame's extraction) | `Test_Lifecycle_Teleport_ResetsAllHistories` |
| Ragdoll enter and recover | `Zenith_RecordEnterRagdoll`, `Zenith_RecordRecoverFromRagdoll` (Physics module) | sync point after the issuing phase (step 7); blending runs in phase 6 of every following tick until done | phase 5 of the same tick when issued in phases 1–4; phase 5 of the next tick when issued in phases 5–10 | `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend` (issues from phase 2 and from phase 10, and asserts both) |
| Cinematic start, interrupt and skip | `Zenith_RecordStartCinematic`, `Zenith_RecordInterruptCinematic`, `Zenith_RecordSkipCinematic` (Cinematics module) | sync point after the issuing phase (step 7); restore at the next tick boundary after end or skip | the next phase (start); the next tick (restore) | `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` |
| World transition | `Zenith_Runtime::RequestWorldTransition` | the new world loads in isolation; its load completion is published and logged at an old-world tick boundary; the swap happens in step 7 of the next frame prologue | the first tick of the new world | `Test_Lifecycle_WorldTransition_SwapsAtTickBoundary` |
| Shutdown | `Zenith_Runtime::RequestShutdown` | when the current frame-loop iteration returns: every tick that iteration started drains (every phase runs) and no further tick starts | nothing: teardown | `Test_Lifecycle_Shutdown_ReversePhaseTeardown` |

## Listener interface

Declared in `Engine/ZenithScene/Zenith_LifecycleListener.h`:

```cpp
enum Zenith_LifecycleEventKind : uint8
{
	LIFECYCLE_EVENT_SPAWNED,
	LIFECYCLE_EVENT_DESTROY_PENDING,
	LIFECYCLE_EVENT_DESTROYED,
	LIFECYCLE_EVENT_TELEPORTED,
	LIFECYCLE_EVENT_RAGDOLL_ENTER,
	LIFECYCLE_EVENT_RAGDOLL_RECOVER,
	LIFECYCLE_EVENT_CINEMATIC_START,
	LIFECYCLE_EVENT_CINEMATIC_INTERRUPT,
	LIFECYCLE_EVENT_CINEMATIC_SKIP,
	LIFECYCLE_EVENT_CINEMATIC_END,
	LIFECYCLE_EVENT_ORIGIN_SHIFTED,       // entity null; payload const Zenith_WorldOriginShift* (ZenithScene)
};

struct Zenith_LifecycleEvent
{
	const Zenith_LifecycleEventKind m_eKind = {};
	const Zenith_EntityHandle m_xEntity = {};           // null for cinematic events that bind several entities
	const uint64 m_ulTick = 0;
	const void* const m_pPayload = nullptr;                  // the command's payload struct, by kind; valid for the call
};

// Contract: implemented by modules, registered by the composition root per world.
// Thread affinity: PHASE-independent; called on the world's sync-point node, one world at a time.
// Reentrancy: NONE — a listener records commands into the provided buffer, never calls back into playback.
class Zenith_LifecycleListener
{
public:
	virtual ~Zenith_LifecycleListener() = default;
	virtual uint32 GetInterestMask() const = 0;        // bit per Zenith_LifecycleEventKind
	virtual void OnLifecycleEvent(const Zenith_LifecycleEvent& xEvent, Zenith_WorldPlaybackAccess& xWorld,
		Zenith_CommandBuffer& xFollowUp) = 0;
};
```

- [Z2-N-ARCH-543] Listeners are registered per world by the composition root in module registration order and are invoked in that order for each event; the order is part of the determinism contract.
- [Z2-N-ARCH-572] `LIFECYCLE_EVENT_ORIGIN_SHIFTED` (value 10) is dispatched by the `Scene.ShiftWorldOrigin` playback at the end of step 7 of its sync point, after every transform and flagged property is shifted, with a null entity and `m_pPayload` pointing at the played `Zenith_WorldOriginShift` for the duration of the dispatch, so a module holding position-valued state (the Physics module's backend world) shifts it at the same sync point as the transforms (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Origin shifted lifecycle event").
- [Z2-N-ARCH-544] A listener changes world state only through the `Zenith_WorldPlaybackAccess` it receives (property writes allowed at playback) and through follow-up commands, which are applied at the next sync point; it never records into the buffer being played back.

## Spawn

```cpp
struct Zenith_SpawnDesc
{
	const Zenith_Span<const Zenith_ComponentInit> m_axComponents = {};   // type ID + initial property bytes
	const Zenith_PersistentID m_xPersistentID = {};                       // null for runtime-only entities
	const Zenith_EntityHandle m_xParent = {};                             // null for a root
};

Zenith_SpawnTicket Zenith_CommandBuffer::Spawn(const Zenith_SpawnDesc& xDesc);
// Engine/ZenithScene/Zenith_LifecycleCommands.h
Zenith_SpawnTicket Zenith_RecordInstantiatePrefab(Zenith_CommandBuffer& xBuffer,
	Zenith_AssetHandle<Zenith_Prefab> xPrefab, const Zenith_Transform& xAt);
```

A `Zenith_SpawnTicket` is a buffer-local placeholder. Subsequent commands in the same buffer may name it (for example, `SetProperty` on the ticket), and playback resolves it to the real handle.

- [Z2-N-ARCH-545] A spawned entity is created at the sync point after the issuing phase and is visible to every node of the next phase; its entity handle and stable simulation ID are assigned at playback in merge order, so they are identical on every run (Zenith2/Docs/Scripting.md → "Determinism rules and randomness").
- [Z2-N-ARCH-546] A spawn rejected at merge-time capacity (entity capacity, the per-entity snapshot cap of its issuer) creates nothing, consumes no ID, and resolves its ticket to the null handle; commands naming a null-resolved ticket are dropped with the counter `ecs.commands_stale_delta`.

## Destroy

- [Z2-N-ARCH-547] `Destroy` marks the entity `pending destroy` at the sync point after the issuing phase and fires `LIFECYCLE_EVENT_DESTROY_PENDING` there.
- [Z2-N-ARCH-569] An entity marked `pending destroy` stays fully readable and queryable for the rest of its tick.
- [Z2-N-ARCH-570] The end-of-tick playback removes every pending-destroy entity, fires `LIFECYCLE_EVENT_DESTROYED` and increments its slot generation, so its handles are stale from the next tick.
- [Z2-N-ARCH-548] Render extraction skips entities marked `pending destroy`, so a destroyed entity is never drawn in the frame whose tick destroyed it.
- [Z2-N-ARCH-549] Destroying an entity destroys its hierarchy children in the same playback, in depth-first order of stable simulation ID, and each child is marked and removed like its parent.
- [Z2-N-ARCH-571] The parent relation of entities is ECS's (held by `Zenith_EntityTable` and set by spawn playback and changed only by the `SET_PARENT` record ECS playback applies), so the destroy cascade of bullet 549 is ECS playback; ZenithScene's hierarchy reads the relation through `Zenith_EntityTable` and never stores a second copy.

## Teleport

```cpp
enum Zenith_TeleportFlags : uint32
{
	TELEPORT_FLAG_NONE                  = 0,
	TELEPORT_FLAG_KEEP_VELOCITY         = 1u << 0,   // keep linear and angular velocity (rotated into the new frame)
	TELEPORT_FLAG_KEEP_ANIMATION_PHASE  = 1u << 1,   // keep the animation graph's time, reset only its root
};

// Engine/ZenithScene/Zenith_LifecycleCommands.h
void Zenith_RecordTeleport(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity,
	const Zenith_Transform& xTarget, uint32 uTeleportFlags);
```

At playback the teleport sets the entity's local transform (and recomputes its world transform and its children's immediately, so the next phase sees consistent transforms), then fires `LIFECYCLE_EVENT_TELEPORTED` to listeners:

| Listener | Reset |
|---|---|
| physics | body pose set to the target; linear and angular velocity zeroed unless `TELEPORT_FLAG_KEEP_VELOCITY`; contacts and CCD state cleared |
| animation | root-motion accumulator and root delta zeroed; graph time kept only with `TELEPORT_FLAG_KEEP_ANIMATION_PHASE`; IK targets re-seeded |
| transform interpolation (Scene) | the previous-tick transform set equal to the target, so interpolation does not sweep |
| render extraction | the instance's motion-vector history reset flag set for its next extraction, so Flux writes previous = current and draws no velocity smear |
| streaming | the entity's streaming anchor moved; cell priorities recomputed at the next tick boundary |

- [Z2-N-ARCH-550] A teleport resets physics velocity, interpolation, the animation root and the render motion-vector history of the entity and of every descendant, so no system observes a velocity or a smear from the jump.
- [Z2-N-ARCH-551] A teleport of an entity in ragdoll moves every ragdoll body rigidly with the root and zeroes their velocities unless `TELEPORT_FLAG_KEEP_VELOCITY`, and the ragdoll blend state is kept.

## Ragdoll enter and recover

```cpp
// Engine/ZenithPhysics/Zenith_Ragdoll.h
inline constexpr uint32 uPHYSICS_NO_IMPULSE_BONE = 0xFFFFFFFFu;   // Physics-local sentinel: apply no impulse

struct Zenith_RagdollEnterDesc
{
	float m_fBlendSeconds = 0.1f;                       // animation → physics ownership blend
	Zenith_Vector3 m_xImpulse;                           // applied to m_uImpulseBone at the first physics step
	uint32 m_uImpulseBone = uPHYSICS_NO_IMPULSE_BONE;             // a skeleton joint index
};

struct Zenith_RagdollRecoverDesc
{
	float m_fBlendSeconds = 0.4f;                       // physics → animation ownership blend
	Zenith_PersistentID m_xGetUpClip;                    // an Animation.Clip asset's persistent ID, or null for none; ZenithPhysics
	                                                     // sits below ZenithAnimation and cannot name Zenith_AnimationClip
};

// Engine/ZenithPhysics/Zenith_Ragdoll.h
// xKinds: the Physics module's command-kind IDs, resolved at its first AttachWorld (Zenith2/Docs/Design/PHYS/ComponentsAPI.md)
void Zenith_RecordEnterRagdoll(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_RagdollEnterDesc& xDesc);
void Zenith_RecordRecoverFromRagdoll(Zenith_CommandBuffer& xBuffer, const Zenith_PhysicsCommandKinds& xKinds,
	Zenith_EntityHandle xEntity, const Zenith_RagdollRecoverDesc& xDesc);
```

Ownership of the character's bone transforms moves between the Animation writer source and the Ragdoll writer source through the arbitration rule of the pose component (Architecture/FramePhases.md → "Arbitration rules"), with the blend weight advanced once per tick in phase 6 by `m_fBlendSeconds` of simulated time.

- [Z2-N-ARCH-552] Ragdoll enter creates the ragdoll bodies from the current pose at the playback of the sync point after the issuing phase; a command issued in phases 1–4 is therefore simulated by phase 5 of the same tick, and one issued in phases 5–10 by phase 5 of the next tick.
- [Z2-N-ARCH-564] Ragdoll enter blends ownership from animation to physics over `m_fBlendSeconds` of simulated time, advanced in phase 6 of each tick.
- [Z2-N-ARCH-565] Ragdoll recover blends ownership from physics back to animation over its own `m_fBlendSeconds` and removes the ragdoll bodies at the playback after the blend reaches 1.
- [Z2-N-ARCH-553] An enter received while a recover blend runs (or a recover while an enter blend runs) reverses the blend from its current weight, never jumping to 0 or 1.

## Cinematic start, interrupt and skip

```cpp
// Zenith_Cinematic, Zenith_CinematicBinding, Zenith_CinematicTicket, Zenith_CinematicInstanceID and Zenith_CinematicCommandKinds
// are declared by the CINE design (Zenith2/Docs/Design/CINE/PublicAPI.md → "Zenith_CinematicTypes header",
// "Zenith_CinematicAsset header", "Zenith_CinematicCommands header"); the ticket resolves to the instance ID at playback
// like a spawn ticket.
struct Zenith_CinematicStartDesc
{
	Zenith_AssetHandle<Zenith_Cinematic> m_xSequence;
	Zenith_Span<const Zenith_CinematicBinding> m_axBindings;   // track name → entity
	float m_fBlendInSeconds = 0.25f;
	float m_fBlendOutSeconds = 0.25f;
	float m_fRate = 1.0f;                                      // a multiplier of world time
	float m_fStartSeconds = 0.0f;
	Zenith_StringID m_xTag;                                    // a caller tag; the empty ID for none
	uint32 m_uFlags = 0;
	Zenith_StringID m_xVoiceLocale;                            // the simulation voice-locale tag; the empty ID by default
};

// Engine/ZenithCinematics/Zenith_CinematicCommands.h; xKinds is the world's Zenith_CinematicsWorld::GetCommandKinds()
Zenith_CinematicTicket Zenith_RecordStartCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	const Zenith_CinematicStartDesc& xDesc);
void Zenith_RecordInterruptCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);   // blends out from now
void Zenith_RecordSkipCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);   // jumps to the end, then restores
```

At start, the cinematic module takes a **pre-cinematic snapshot** of the owned fields of every bound entity: every property the cinematic's tracks write, plus every property the component schema marks `restoreAfterCinematic`. While the cinematic plays, its track writers use the Cinematic writer source, so the default precedence gives them priority over every gameplay source.

| Event | Effect |
|---|---|
| start | snapshot taken at playback; tracks start writing in the next tick's phases; blend in over `m_fBlendInSeconds` |
| natural end | blend out over `m_fBlendOutSeconds`; at the next tick boundary after the blend completes, restore properties marked `restoreAfterCinematic` from the snapshot and release it |
| interrupt | blend out from the current weight; restore as for a natural end |
| skip | evaluate the sequence's end state once (events on skipped tracks marked `fireOnSkip` fire in timeline order), then restore at the next tick boundary without blending; no END follows a skip |

- [Z2-N-ARCH-554] A cinematic takes arbitration precedence over every other writer source while it plays, and on end, interrupt or skip it restores gameplay state from the pre-cinematic snapshot of owned fields at the next tick boundary.
- [Z2-N-ARCH-555] Properties written by a cinematic track and not marked `restoreAfterCinematic` keep the cinematic's final value at restore, so a sequence can leave a character where it ended; the choice is declared per property in the component schema or registration, never at run time.
- [Z2-N-ARCH-556] A bound entity destroyed during a cinematic is dropped from its bindings at the destroy playback, its snapshot entry is released, and the sequence continues for the remaining bindings.

## World transition

```cpp
struct Zenith_WorldTransitionDesc
{
	Zenith_AssetHandle<Zenith_Scene> m_xScene;
	Zenith_Span<const Zenith_PersistentID> m_axCarryOver;   // persistent entities moved to the new world
};

Zenith_Result<Zenith_OperationHandle, Zenith_RuntimeError> Zenith_Runtime::RequestWorldTransition(
	uint16 uFromWorldTag, const Zenith_WorldTransitionDesc& xDesc);
```

The old world keeps ticking normally until the swap. The new world is created and loaded in isolation: it has its own ECS instance, physics world, script VMs and streaming state, contributes no node to the running schedule, and ticks nothing. When loading completes, the completion is an asynchronous result like any other: it is published at the old world's next tick boundary in completion-ID order and logged in that tick's replay record as a `PUBLICATION_KIND_WORLD_LOADED` publication (Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"; Architecture.md → "Multiplayer foundations"). The swap is then performed in step 7 of the first frame prologue after the frame that ran that tick (Architecture/CPUFrameGraph.md → "The frame prologue": isolated worlds whose preparation succeeded are created, `Zenith_SceneLoader::StepActivations` starts every pending additive-scene preparation and then advances isolated-world activation within `scene.activationBudgetUs`, then the published and logged swaps are performed), which also writes a `WORLD_SWAP` record carrying the old world's last executed tick and the new world's tag, seed, scene persistent ID and content hash. A `--replay` run performs the swap from the logged record: the replay caps each frame's tick count so that a prologue falls directly after the recorded last tick, and swaps there. The steps:

A completion addressed to an old world that is paused publishes at the first tick boundary after the world resumes, not before (Architecture/FramePhases.md → "Sync points and the tick boundary", bullet 507), so a paused old world defers its own swap.

1. Cancel every outstanding asynchronous operation of the old world; results that complete afterwards are discarded unpublished.
2. Serialize the carry-over entities from the old world (after its last tick's end-of-tick playback) and deserialize them into the new world with new handles and their persistent IDs.
3. Destroy the old world (listeners receive `LIFECYCLE_EVENT_DESTROYED` for every entity in debug diagnostics mode alone; normal destruction is bulk).
4. Recompile the schedule with the new world's nodes, in step 8 of the same prologue.
5. Run the new world's first tick boundary, with its tick index starting at 0.

After the swap, the new world is the primary world: `--exit-after-ticks` counts ticks of the primary world summed across swaps, and `--record` continues the same log with the new world's `WORLD_BEGIN` record.

- [Z2-N-ARCH-557] A new world is loaded in isolation and swapped in only in step 7 of a frame prologue, before every world's first tick boundary of that frame; no tick ever observes a half-loaded world or both worlds' state mixed.
- [Z2-N-ARCH-566] The tick after which a world swap happens is determined by the logged publication of the load completion and recorded in the `WORLD_SWAP` record, and a replay swaps after the same tick, so a run with a world transition replays bit for bit (Z2-D-019, Z2-D-053).
- [Z2-N-ARCH-567] Carry-over entities are serialized from the old world after its final end-of-tick playback and deserialized into the new world with new handles and unchanged persistent IDs before the new world's first tick boundary.
- [Z2-N-ARCH-558] Asynchronous results addressed to a world that has been swapped out are discarded when they complete and never published into any world, and the discard is counted in `world.async_discarded`, published by the world host alone: the swapped-out world's `Zenith_AsyncResultTable` is retired, not destroyed, and outlives the world, together with the runtime sinks producers complete through, until no producer can still complete into it (`GetInFlightCount()` is 0, or shutdown after every job has run), so a completion after the world's destruction reaches the retired table, which discards and counts it (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Retired world asynchronous results"); a world destroyed with results in flight other than at shutdown retires its table the same way.
- [Z2-N-ARCH-559] The memory governor reserves the world-transition peak (both worlds resident) before loading starts, and a refused reservation fails the transition request with `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED` instead of loading (Architecture.md → "Memory governor").

## Shutdown

- [Z2-N-ARCH-560] `RequestShutdown` takes effect when the current frame-loop iteration returns: every tick that iteration started drains (every phase runs), the commands of its last sync points are flushed by their own playback, no further tick starts, and each world is torn down in reverse phase order (extraction state, snapshot state, scripts, events, the modules' per-world state in reverse canonical order (AI, post-physics, physics, movement, animation), transforms and the scene world state, input, then the world's asynchronous result table), followed by the process shutdown order of Zenith2/Docs/Contracts.md → "Threading and concurrency contract"; the scripts entry keeps the world's asynchronous result table, which is destroyed, or retired while results are in flight ([Z2-N-ARCH-558]), only after the modules' per-world state, the scene world state and the input resources, because producers complete into it from their `DetachWorld` and from background jobs, and the scene world state, which owns the lifecycle dispatcher the modules' listeners are registered with, is destroyed after the modules' `DetachWorld`.
- [Z2-N-ARCH-561] No lifecycle command recorded in the final tick is dropped silently: commands recorded after the final sync point of a world are counted in `world.commands_after_shutdown` and logged.

## Scenario tests

The five cross-system scenarios run in three integration suites, one per stage, each linking just the module libraries that exist at its stage (`Zenith2/Docs/Design/GAME/PhaseContract.md` → "Suite target and fixtures"), on every ECS backend and with 1, 2, 8 and all workers:

| Suite directory | Target | Stage | Links | Tests |
|---|---|---|---|---|
| `Zenith2/Tests/Lifecycle/` | `Zenith_Integration_Lifecycle` | 2 | `ZenithRuntime` and the three ECS backends | `Test_Scenario_SpawnAndDestroySameTick`, `Test_Scenario_ReplayMatrix` |
| `Zenith2/Tests/LifecycleCharacters/` | `Zenith_Integration_LifecycleCharacters` | 3 | the above plus `ZenithGameplay`, `ZenithPhysics`, `ZenithPhysics_Jolt`, `ZenithAnimation` | `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend`, `Test_Scenario_TeleportDuringRagdoll`, `Test_Scenario_ReplayMatrixCharacters` |
| `Zenith2/Tests/LifecycleOpenWorld/` | `Zenith_Integration_LifecycleOpenWorld` | 4 | the above plus `ZenithCinematics`, `ZenithStreaming`, `ZenithAI` | `Test_Lifecycle_Cinematic_SkipRestoresGameplayState`, `Test_Lifecycle_Cinematic_InterruptBlendsOut`, `Test_Scenario_SkipCinematicMidBlend`, `Test_Scenario_WorldTransitionWithInFlightAsync`, `Test_Scenario_SaveAtSnapshotDuringStreaming`, `Test_Scenario_ReplayMatrixOpenWorld` |

- [Z2-N-ARCH-562] Every scenario in the table "Lifecycle scenario tests" exists under the name given and asserts every listed condition.
- [Z2-N-ARCH-568] Each scenario is recorded once per backend and its log replayed with 1, 2, 8 and all workers, and the replays produce bit-identical per-tick world-state hashes on that backend; each suite carries its own replay-matrix case (`Test_Scenario_ReplayMatrix`, `Test_Scenario_ReplayMatrixCharacters`, `Test_Scenario_ReplayMatrixOpenWorld`), and the COHERENT case of `Test_Scenario_SaveAtSnapshotDuringStreaming` is not part of it.

Lifecycle scenario tests:

| Test | Setup | Asserts |
|---|---|---|
| `Test_Scenario_TeleportDuringRagdoll` | a character enters ragdoll at tick 10; a teleport is recorded in phase 2 of tick 20, mid-blend | every ragdoll body moved rigidly by the teleport offset; body velocities zero; blend weight unchanged; interpolation and motion-vector history reset for tick 20's extraction; no body intersects the target position's static geometry |
| `Test_Scenario_SkipCinematicMidBlend` | a cinematic binding two characters starts at tick 10; `SkipCinematic` at tick 12, during blend-in | the end state evaluated once; `fireOnSkip` events delivered once, in timeline order; `restoreAfterCinematic` properties equal the pre-cinematic snapshot from the next tick; non-restored properties hold the end state; no cinematic writer source active afterwards |
| `Test_Scenario_SpawnAndDestroySameTick` | phase 2 spawns entity E and records `SetProperty` on its ticket; phase 7 destroys E; phase 10 reads E | E visible from phase 3; the property applied; E readable in phase 10 with `pending destroy`; E not extracted; E's handle stale in the next tick; spawn/destroy listener order identical on every run |
| `Test_Scenario_WorldTransitionWithInFlightAsync` | world A issues 32 path requests and 8 streaming loads, then requests a transition to world B | the swap happens in a frame prologue after the tick that published A's load completion, and the `WORLD_SWAP` record names that tick; a `--replay` of the recorded run swaps after the same tick; no A result is published into B; `world.async_discarded` equals the results completing after the swap; carry-over entities keep their persistent IDs; the schedule recompiles once |
| `Test_Scenario_SaveAtSnapshotDuringStreaming` | a world streams cells while 20 animated characters and 50 physics bodies run; a save is requested in phase 10 | the snapshot contains no half-streamed cell, no duplicated or lost entity, and physics and animation state from the same tick; a COHERENT save (a behaviour suspended mid-`Run`, a quarantined or disabled instance, a loading additive instance, PhysX) is checked by SAVE's coherence assertions, and a replay resumed from it runs without hash comparison and reports that; for an EXACT save (no behaviour suspended mid-`Run`, no quarantined instance or instance of a disabled or disabling module, no in-flight additive load, every participant EXACT) the run records with `--record`, the phase-10 save carries its replay position, and a replay resumed from the save (`Zenith_RuntimeReplay_ResumeFromSave`) reproduces the original run's recorded tick hashes from that tick, taking every publication from the log; an interrupted write (fault-injected) leaves the previous save intact |

The per-command tests of the table "Lifecycle command summary" live in `Engine/ZenithRuntime/Tests/`, except the ragdoll test `Test_Lifecycle_Ragdoll_EnterAndRecoverBlend`, which runs in `Zenith2/Tests/LifecycleCharacters/` (Stage 3), and the cinematic tests `Test_Lifecycle_Cinematic_SkipRestoresGameplayState` and `Test_Lifecycle_Cinematic_InterruptBlendsOut`, which run in `Zenith2/Tests/LifecycleOpenWorld/` (Stage 4), under unchanged names, because they need modules above ZenithRuntime; `ZenithECS_Conformance` proves the spawn and destroy playback in its own, distinctly named cases `Test_ECS_SpawnVisibleNextPhase` and `Test_ECS_DestroyAtTickEnd`, since a test-case name is unique programme-wide.

- [Z2-N-ARCH-563] Every per-command test named in the table "Lifecycle command summary" exists and asserts that command's publication point and first observer as the table states.
