# SCENE design — transactional loading, additive scenes and scene transitions

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies how cooked scenes reach worlds: the preparation operation that runs on background jobs and touches no world, activation into a world at a tick boundary or into an isolated world before a swap, the startup scene, additive scenes, scene transitions built on the world-transition lifecycle command (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition"), carry-over, cancellation and missing-type handling at load.

## Principles

| Principle | Mechanism |
|---|---|
| a scene loads in isolation | preparation (decode, dependencies, spawn-record construction) never touches a world; its result is a `Zenith_PreparedScene` owned by the operation |
| all or nothing | activation into a running world is one command buffer accepted or rejected whole at one merge point; activation into an isolated world completes before the world is visible |
| tick-boundary visibility | a completed preparation is an asynchronous result published at a tick boundary in completion-ID order and logged (Z2-D-053); the swap of a transition happens in step 7 of a frame prologue ([Z2-N-ARCH-557]) |
| determinism | the tick at which content appears is the logged publication tick; replay holds the tick until the recomputed preparation is ready ([Z2-N-SCRIPT-337]) |
| cancellable | every stage has cancellation points; a cancelled preparation releases everything on its own executor thread before reporting `OPERATION_STATUS_CANCELLED` ([Z2-N-CONTRACT-335]) |

## Preparation operation

`Zenith_SceneLoader::BeginPrepare(const Zenith_SceneLoadDesc&)` returns a `Zenith_OperationHandle` (`Zenith2/Docs/Contracts.md` → "Handle kinds") for a background operation submitted with `Zenith_JobSystem::SubmitBackground` at `JOB_PRIORITY_BACKGROUND_HIGH` (`Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem"), each slice at most 2 ms ([Z2-N-ARCH-393]), progressing through continuations.

### Preparation state machine

| State | Work (in slices) | Cancellation point | Next |
|---|---|---|---|
| `REQUESTED` | use the desc's `Zenith_AssetHandle<Zenith_Scene>`, or acquire it for the virtual path with `AcquireByLogicalPath`; request its load through ASSET's loader (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetRegistry", "Zenith_AssetLoader") | before the request | `READING` |
| `READING` | wait (by re-submitting the continuation at the next slice, never blocking) until the registry resolves the handle to the resident `Zenith_Scene`, whose kind loader holds the decoded sections, or reports the load failed | each slice | `DECODING`; `FAILED` (`SCENE_ERROR_ASSET_UNAVAILABLE`, including ASSET's `ASSET_ERROR_DEPENDENCY_FAILED` for a hard dependency that failed without a substitute, naming it; `SCENE_ERROR_COOKED_VERSION` for ASSET's version mismatch) |
| `DECODING` | validate and decode the sections (`Zenith_Scene::GetSections`) into `Zenith_SceneData` in the operation's arena (`Zenith_DecodeCookedScene`), in chunks of 4,096 entities per slice | between chunks | `DEPENDENCIES`; `FAILED` (`SCENE_ERROR_COOKED_CORRUPT`, `SCENE_ERROR_UNKNOWN_TYPE` under the FAIL policy) |
| `DEPENDENCIES` | acquire with `AcquireAny(id, kind)` a handle for every ASSET_REFS entry, a counted reference each, which the operation holds; acquire only, never wait: the entries are the scene's hard dependencies (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Scene and prefab cook steps"), which ASSET made ACTIVE, or FAILED with an ACTIVE substitute, before the scene's kind loader ran, and keeps for the scene's ACTIVE life (`Zenith2/Docs/Design/ASSET/Loading.md` → "Dependency loading") | each slice | `BUILDING`; `FAILED` (`SCENE_ERROR_ASSET_UNAVAILABLE` when `AcquireAny` fails, naming the asset) |
| `BUILDING` | convert field sets to `Zenith_ComponentInit` storage (intern strings, write asset handle bytes) in chunks; when the desc's `m_xIDScope` is non-null, replace every entity's persistent ID and every ENTITY-typed value naming an entity of the scene with `Zenith_DeriveAuthoringID(m_xIDScope, id)` in the same chunks, so no derivation runs on a simulation node (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation") | between chunks | `READY` |
| `READY` | report `OPERATION_STATUS_SUCCEEDED`; the prepared scene is taken by `Zenith_SceneLoader::TakePrepared(handle)` | — | (activation) |
| `FAILED` | release everything; report `OPERATION_STATUS_FAILED` with the `Zenith_SceneErrorCode` as the failure code (`Zenith_JobContext::ReportFailed`) | — | — |
| `CANCELLED` | release the arena, every asset handle and every interned-string reference; report `OPERATION_STATUS_CANCELLED` | — | — |

A load committed with a non-zero hook tag (`Zenith_AdditiveLoadDesc::m_ulHookTag`) adds the state `GATED` between `DEPENDENCIES` and `BUILDING` and runs `BUILDING` in two passes (skip decisions, then conversion with component amendment), as `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Gated preparation state" specifies. A failure with `SCENE_ERROR_ASSET_UNAVAILABLE` (in `READING` or `DEPENDENCIES`) carries the causing `Zenith_AssetErrorCode` as its detail (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Asset error detail"), and a test probe installed with `Zenith_SceneLoader::SetPreparationProbe` is called at the first slice of `DECODING`, `DEPENDENCIES` and `BUILDING` (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Preparation probe and hold").

- Memory: the operation's arena is reserved with the memory governor before `DECODING` when the scene's decoded size (the scene header section's entity count × the record sizes plus the decoded section sizes, bounded) exceeds `scene.prepareReserveThresholdBytes` (1 MiB default), through the subsystem-scoped reservation of `Zenith2/Docs/Architecture.md` → "Subsystem reservations" (`ReserveSubsystemBytes(MEMORY_SUBSYSTEM_SIMULATION, bytes)`, [Z2-N-ARCH-825]); the returned `Zenith_SubsystemReservation` is held beside the arena and released with it ([Z2-N-ARCH-826]); a refusal (`MEMORY_ERROR_SUBSYSTEM_REFUSED`) fails the operation with `SCENE_ERROR_MEMORY_REFUSED`. For a transition the peak is the world-transition reservation of [Z2-N-ARCH-559], taken by the runtime before `BeginPrepare`.
- Progress: 0.1 after `READING`, 0.1–0.6 across `DECODING` by entity, 0.6–0.9 across `DEPENDENCIES` by count, 0.9–1.0 across `BUILDING`; monotonic ([Z2-N-CONTRACT-094]).
- The prepared scene owns: the arena holding `Zenith_SceneData` and the init storage, the asset handles of the scene and its dependencies, and the unknown-component table. Destroying it releases them in that order.

## Activation paths

| Path | Where activation runs | Visible | Used by |
|---|---|---|---|
| startup | startup step 17 on the main thread, before the first frame (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"): `BeginPrepare`, then `Zenith_JobSystem::RunRoot` helping until the operation completes, then out-of-tick activation | from the first tick | the startup scene |
| isolated world | step 7 of successive frame prologues, in slices bounded by `scene.activationBudgetUs` (2,000 µs default; the pending row Z2-B-SCENE-003), into the new world that is in no schedule ([Z2-N-ARCH-424]) | from the new world's first tick after the swap | world transitions |
| additive | the phase-1 system `Scene.ActivateAdditive` of the target world records one buffer; applied at the phase-1 sync point | from phase 2 of the publication tick | additive scenes |
| editor | `Zenith_ActivateSceneData` out of tick into the edit world, directly from a resolved document (no preparation operation) | immediately | EDIT |

### Out-of-tick activation

`Zenith_ActivatePreparedOutOfTick` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneLoader header") activates the next slice of a prepared scene:

1. Build the next chunk of at most 4,096 entities into a command buffer over the prepared scene's arena: one `Spawn` per entity with its persistent ID (the authored or derived ID), its parent's handle (a ticket of this chunk, or the real handle of a parent activated by an earlier chunk, looked up through `FindByPersistentID`), its components and the transform components; ENTITY-typed values naming entities of the scene are tickets within the chunk, or a `Scene.BindReferences` record (below) when the target is in a subsequent chunk or outside the scene.
2. `ApplyOutOfTick` the buffer ([Z2-N-ARCH-567]'s "deserialized with new handles"), which plays spawns, then SCENE's module records, and fires the structural observer, so the hierarchy cache follows.
3. Repeat while the elapsed time is below `ulBudgetUs` and chunks remain; report progress.
4. After the last chunk: `Zenith_PropagateTransformsOutOfTick`; mark the activation complete.

`Scene.BindReferences` (module command kind, step 7): payload `{ uint32 count; count × { uint64 entity bits or persistent ID source, uint64 type ID, uint32 property ID, uint32 flags, 16-byte target persistent ID } }`; playback resolves the target with `FindByPersistentID` and writes the property with `SetProperty`; an unresolved target writes the null handle and counts `scene.references.unresolved` with a log line naming entity, component, property and target ([Z2-N-SCRIPT-397]'s policy for stale references, applied to scenes).

## Startup scene

At startup step 17 the runtime calls `Zenith_SceneLoader::LoadStartupScene` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneLoader header"), which runs preparation to completion under `RunRoot` of the job system the loader was created with, activates out of tick with no budget, propagates, and returns `Zenith_Result<void, Zenith_SceneError>`. Unknown types use the FAIL policy; any error maps to the runtime's exit code 3 for step 17 (`Zenith2/Docs/Design/MOD.md` → "Startup sequence with unwinding"), whose rule covers a startup scene that is missing, corrupt, of a mismatched version, or references an unregistered type. The load writes the replay log's `WORLD_BEGIN` scene persistent ID and content hash through the runtime (MOD).

## Scene transitions

A scene transition is the world-transition lifecycle command with a scene as its target: `Zenith_Runtime::RequestWorldTransition(uFromWorldTag, Zenith_WorldTransitionDesc{ m_xScene, m_axCarryOver })` (MOD's API) drives these SCENE steps, and `Zenith_RecordRequestSceneTransition` records the request as a command so a script can issue it inside its transaction (the command's playback calls the runtime's request through the upward interface `Zenith_SceneTransitionSink` that ZenithRuntime implements):

| Step | Owner | Action |
|---|---|---|
| 1 | Runtime | reserve the world-transition peak (Z2-B-CORE-007, [Z2-N-ARCH-559]); a refusal fails the request with `RUNTIME_ERROR_TRANSITION_PEAK_REFUSED` |
| 2 | SCENE | `BeginPrepare(scene)` — the old world keeps ticking |
| 3 | Runtime (prologue step 7) | once the operation succeeded: create the isolated world (world tag, backend world, system registry, `Zenith_SceneWorldState::Create` and `Attach`) — it contributes no node ([Z2-N-ARCH-424]) |
| 4 | SCENE (prologue step 7, sliced) | `Zenith_ActivatePreparedOutOfTick` into the isolated world within `scene.activationBudgetUs` per prologue |
| 5 | Runtime | when activation completes, park the load completion as `PUBLICATION_KIND_WORLD_LOADED` / `ASYNC_SOURCE_WORLD_LOAD` for the old world; it publishes at the old world's next tick boundary and is logged ([Z2-N-ARCH-788]) |
| 6 | Runtime (next prologue step 7) | swap: cancel the old world's operations, carry over (§ "Carry-over"), destroy the old world, recompile in step 8, write `WORLD_SWAP` ([Z2-N-ARCH-566]) |

<!-- assumption: pending X109(b) -->
[Z2-N-ARCH-437] fixes the prologue at eight steps, and `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "The frame prologue" words step 7 as "Pending world-transition swaps whose load completion has been published and logged are performed". World creation in step 7 already has a precedent: the table "Schedule recompile triggers" of `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Schedule compilation and caching" places "world created or destroyed (including a world transition's swap)" with `ZenithRuntime`, "in step 7 of the frame prologue". Sliced activation has none, and it needs the prologue, the only main-thread window in which no frame-graph job runs, because out-of-tick playback may not overlap any node of any world. SCENE therefore assumes step 7 covers the whole transition work of the table, in this order within the step: create each isolated world whose preparation succeeded (row 3), call `Zenith_SceneLoader::StepActivations(scene.activationBudgetUs)` (row 4; it also starts the pending additive preparations of § "Additive load sequence" step 3 first), then perform each swap whose completion was published and logged (row 6). Cross-doc item X109(b) widens step 7's text to that sentence (the eight-step count of [Z2-N-ARCH-437] is unchanged); cross-area note S9 is its SCENE-side record.
<!-- /assumption -->

The replay of a transition performs the same steps; at step 5 a replay holds the old world's publication tick until activation is complete, as for any recomputed result ([Z2-N-SCRIPT-337]), so the swap tick is the recorded one.

### Carry-over

`Zenith_CaptureEntities` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneCapture header") captures each root and its subtree out of tick (after the old world's final end-of-tick playback, [Z2-N-ARCH-567]): entities in depth-first ascending simulation ID, components as serialized-only field sets (`Zenith_CaptureFieldSet`, `bSerializedOnly = true`), ENTITY references as persistent IDs (entities without one inside the captured set receive derived IDs `Zenith_DeriveAuthoringID(root, simulation-ID-as-ID)` for the transfer), asset references as asset IDs. `Zenith_ActivateSceneData` then spawns them into the new world with their persistent IDs; a carry-over persistent ID already present in the new scene is `SCENE_ERROR_DUPLICATE_ID` and that root is skipped with a log line, never merged silently. A carry-over ID that names no live entity is skipped and counted (`scene.carryover.missing`).

The same capture serves editor play (copy the edit world into a play world, [Z2-N-ARCH-766]) and EDIT's copy and paste between worlds ([Z2-N-ARCH-762]).

## Additive scenes

An additive scene is a cooked scene instantiated into a running world beside its current content, as one scene instance with an ID, loaded and unloaded by commands.

The recording functions `Zenith_RecordLoadAdditiveScene` and `Zenith_RecordUnloadAdditiveScene`, the set and every value type of this section are declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header".

### Additive load sequence

1. **Request** (any phase 1–10), in one of two forms that reach the same commit:
   - C++: `Zenith_RecordLoadAdditiveScene` records the command kind `Scene.LoadAdditive` and returns a buffer-local `Zenith_SceneLoadTicket`;
   - script: `Scene.LoadAdditive(scene, allowDuplicate?)` appends an asynchronous request and returns a `Zenith.AsyncTicket` (§ "Script requests").
2. **Commit**, in merge order:
   - C++: at the sync point's step 7, the playback of `Scene.LoadAdditive` takes a completion ID from the publication sink (`AllocateCompletionID`; 0 fails the load with `SCENE_ERROR_LIMIT`, counted in `scene.additive.failed`, and starts nothing) and commits the load at once;
   - script: SCRIPT's commit has already assigned the completion ID and delivered the request to `Zenith_SceneWorldState::AcceptScriptLoadRequest`, which only appends it to the world's script-load inbox; SCENE's tick-boundary handler commits the inbox at the next tick boundary, step 3, in inbox order, which is the merge order of the SCRIPT commits (§ "Tick-boundary handler").
   A commit: another live (`LOADING`, `QUEUED`, `ACTIVE`) instance of the same scene without `m_bAllowDuplicate` fails the load with `SCENE_ERROR_DUPLICATE_INSTANCE`; a set already holding `scene.maxAdditiveInstances` live records fails it with `SCENE_ERROR_LIMIT`; otherwise the instance ID is assigned from the world's counter (`Zenith_SceneInstanceID`, from 1, never reused within a world), the record is created in `LOADING` holding its completion ID, and a pending preparation `{ instance ID, scene persistent ID, ID scope, unknown-type policy }` is appended to the world state's pending list. A failed commit creates no record and parks its failure publication at once (`Park`, payload `{ 0, code, detail }`). A tagged commit, accepted or refused, is reported to the activation hook through `OnLoadCommitted`, and its pending preparation captures the world's current hook (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Hook call points", "Hook captured per load"). The C++ ticket resolves in `Zenith_AdditiveSceneSet::GetCommittedLoads()` of that sync point, with the instance ID or the error; a script ticket already names the completion ID.
3. **Start** (the next frame prologue, step 7): `Zenith_SceneLoader::StepActivations` first calls `BeginPrepare` for every pending preparation of every world attached to the loader, in world-tag order and then list order, on the main thread with no frame running (the loader's `MAIN_THREAD` rule), with a desc holding the scene handle acquired by persistent ID (`Acquire<Zenith_Scene>`), the ID scope (§ "Instance identity") and the policy. A refusal (`SCENE_ERROR_LIMIT` when the operation table is full, `SCENE_ERROR_OUT_OF_MEMORY`) marks the instance `FAILED` and parks its failure.
4. **Prepare** in the background (§ "Preparation operation").
5. **Publish**: the completed operation is parked with the 8-byte `Zenith_SceneLoadResult` payload; at the next tick boundary of the world it is published in completion-ID order (`ASYNC_SOURCE_SCENE_LOAD`, `PUBLICATION_KIND_GENERIC_RECOMPUTED` on success, `PUBLICATION_KIND_GENERIC_RECORDED` on failure), and SCENE's publication handler moves the prepared scene into the world's activation queue (`Zenith.SceneActivationQueue`, state `QUEUED`) or marks the instance `FAILED` (§ "Publication service").
6. **Activate** (phase 1 of the publication tick): `Scene.ActivateAdditive` (C++ system, phase 1, `EmitsCommands`, writes resource `Zenith.SceneActivationQueue`) takes queued instances in instance-ID order; for each tagged one it first asks the hook `AdmitActivation` (false leaves it `QUEUED` without advancing its retry count) and adds the hook's `GetRootTranslation` to its recorded positions (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Hook call points"); for each admitted one it calls `Reserve(bytes)` on `Commands(0, uInstanceID)` for the whole instance and records every spawn with the persistent IDs the preparation produced (authored, or already derived with the instance's scope, § "Instance identity") plus the `Scene.BindReferences` record; the default merge accepts or rejects the buffer whole.
7. **Result** (the phase-1 sync point): accepted → the entities exist from phase 2; the instance is `ACTIVE` and records its root handles. Rejected by merge-time capacity (entity slots, commands, container bytes) → nothing exists; the instance stays queued and retries at the next tick's phase 1, up to `scene.activationRetryTicks` (8) ticks, then becomes `FAILED` with `SCENE_ERROR_ACTIVATION_CAPACITY`. An instance whose encoded size exceeds one buffer's cap (`Reserve` fails) or whose spawn count exceeds `ecs.maxSpawnsPerTick` fails at once with `SCENE_ERROR_ACTIVATION_TOO_LARGE` (the scene must be split or loaded by a transition).

Everything in steps 2, 5, 6 and 7 happens in merge order, inbox order or publication order, and step 3 changes only when a preparation starts, never the logged publication tick, so a replay reproduces the loads tick for tick.

The source kind `ASYNC_SOURCE_SCENE_LOAD` (8, from the registry of `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry") and the payload rule are § "Publication service".

### Script requests

<!-- elaboration -->
A script load uses SCRIPT's asynchronous-request protocol unchanged (`Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"), with exactly one completion ID:

| Step | What happens |
|---|---|
| call | `Scene.LoadAdditive(scene, allowDuplicate?)` is a behaviour-only buffered binding (a chunk system cannot issue an asynchronous request). It appends one `SIDE_EFFECT_KIND_ASYNC_REQUEST` record with the 24-byte payload `{ uint8 source kind = 8, uint8 flags (bit 0 allow duplicate), uint16 reserved 0, uint32 reserved 0, 16-byte scene persistent ID }`, records no ECS command, and returns a `Zenith.AsyncTicket` holding a provisional index |
| merge | the request is charged to SCRIPT's asynchronous-request capacity of source kind 8, whose in-flight limit is `scene.maxAdditiveInstances`; a rejected or rolled-back transaction submits nothing and consumes no completion ID |
| commit | SCRIPT assigns the completion ID from the world's counter in merge order ([Z2-N-SCRIPT-334]) and remaps the ticket to it in the same commit ([Z2-N-SCRIPT-339]), so the ticket is never provisional after its commit and ZS1105 cannot arise for it; ZenithRuntime's `SIDE_EFFECT_KIND_ASYNC_REQUEST` multiplexer routes source kind 8 to `Zenith_SceneWorldState::AcceptScriptLoadRequest(ulCompletionID, xRequest)`. SCENE adopts that ID: its sink's `AllocateCompletionID` serves C++ requests only |
| publication | the tick boundary that publishes the load calls SCENE's handler through the forwarder, and the result stays in SCRIPT's published table for the ticket |
| read | `ticket:IsReady()` is true from the publication tick; `ticket:Result()` returns the Luau table `{ instance: number, ok: boolean, error: string? }` decoded from the payload by `Zenith_DecodeSceneLoadResult` (`instance` is 0 when the load failed before an instance existed; `error` is `Zenith_ToString` of the code, nil on success). A success means the instance was `QUEUED` at publication and ran its activation in phase 1 of that tick; the script reads the outcome with `Scene.GetInstanceState(instance)` |

The result is released like every script result: when an invocation that read it commits, or when its owning instance is destroyed or quarantined. An unread result holds one in-flight unit of source kind 8, which also bounds C++ loads, and SCRIPT's unconsumed-result trace names its holder.

<!-- assumption: pending SCRIPT, MOD -->
Three things are asked of SCRIPT and MOD by cross-area row S22: the source record of kind 8 is declared with `uMaxResultBytes` 8 (the `Zenith_SceneLoadResult` size); a publication of kind 8 whose `m_xOwnerModule` is valid (a script request) stays in the published table after the forwarding handler has run, instead of being released as a C++-consumed result; and `ticket:Result()` of kind 8 returns the table above through the decoder SCENE supplies. ZenithRuntime's multiplexer routes source kind 8 to `AcceptScriptLoadRequest` of the world's SCENE state.
<!-- /assumption -->

### Tick-boundary handler

`Zenith_SceneWorldState::Attach` registers SCENE's `Zenith_TickBoundaryHandler` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header") with the world. It changes only SCENE's per-world state and uses no member of the `Zenith_TickBoundaryAccess` it receives:

| Step | Actions, in order |
|---|---|
| `TICK_BOUNDARY_STEP_PUBLISH_ASYNC` | record `ulTickStarting` as the world state's current tick and clear the set's "tagged commit refused this tick" flag (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Live instances and capture coherence"); additive publications reach SCENE through the sink's forwarder, which SCRIPT's handler calls in this step (§ "Publication service") |
| `TICK_BOUNDARY_STEP_RESTORE_CINEMATICS` | nothing |
| `TICK_BOUNDARY_STEP_APPLY_DISABLES` | 1. Commit every entry of the script-load inbox in inbox order (§ "Additive load sequence" step 2), then clear the inbox. 2. Release the records of instances that became `UNLOADED` since the previous boundary, returning their slots to the set, except a tagged record whose discarded publication is unpublished, released by the publication handler after `OnDiscardedPublication`. |

Both actions run after step 1's publications, so a load committed here publishes no earlier than the boundary after its preparation completes, and a record released here is never named by a publication of the same boundary.

### Instance identity

- The first live instance of a scene in a world uses the scene's authored and derived IDs unchanged, so saves, carry-over and references by persistent ID from other content address its entities directly.
- A second concurrent instance of the same scene requires `Zenith_AdditiveLoadDesc::m_bAllowDuplicate` (or the script's `allowDuplicate`); its entity IDs are `Zenith_DeriveAuthoringID(xInstanceScope, id)` with `xInstanceScope = Zenith_DeriveAuthoringID(sceneID, xInstanceAsID)`, where `xInstanceAsID` is the `Zenith_PersistentID` with `m_ulHigh = 0` and `m_ulLow = uInstanceID` (the `Zenith_SceneInstanceID` value zero-extended), so its cooked binary form is eight zero bytes followed by the instance ID as a little-endian `uint64`; without the flag the load fails at commit with `SCENE_ERROR_DUPLICATE_INSTANCE`. `Test_AdditiveScenes_DuplicateInstanceNeedsFlag` pins the vector for scene ID `6f1c0e0a-2b1d-4c7e-9a41-7d3b5e2a9c10`, instance 2 and one entity ID.
- The commit knows the instance ID, so it stores `xInstanceScope` in the pending preparation and the preparation derives every ID in its `BUILDING` slices (`Zenith_SceneLoadDesc::m_xIDScope`, § "Preparation state machine"); phase 1 records the derived IDs and derives nothing, so a 50,000-entity duplicate costs the tick no SHA-256 work.

### Unload

`Zenith_RecordUnloadAdditiveScene` records `Destroy` for each root entity of the instance (the destroy cascade removes the subtrees, [Z2-N-ARCH-549]) and the command kind `Scene.ForgetInstance`, whose step-7 playback sets the instance `UNLOADED` and releases its prepared data. An unload of a `LOADING` instance requests cancellation of its preparation and discards it at publication; an unload of a queued, not yet active instance removes it from the queue. For a tagged `LOADING` instance the playback also sets the record's "discard requested" flag (and removes a pending preparation that has not started, parking its discard at once), so exactly one RECORDED cancelled publication follows whatever the operation's outcome (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Discarded publications of tagged loads"). Entities reparented under the instance by gameplay are destroyed with it (they are in the subtree); entities the instance's scripts spawned as roots are not part of the instance.

### Additive scene set

| State | Meaning | Transitions |
|---|---|---|
| `LOADING` | preparation running | → `QUEUED` (published success), `FAILED` (published failure), `UNLOADED` (unloaded) |
| `QUEUED` | prepared, waiting for phase 1 | → `ACTIVE` (accepted), `QUEUED` (rejected, retry), `FAILED` (retries exhausted or too large), `UNLOADED` |
| `ACTIVE` | entities exist | → `UNLOADED` |
| `FAILED` | error recorded; no entity exists | terminal; the record is released at `Scene.ForgetInstance` or world destruction |
| `UNLOADED` | terminal | released at the next tick boundary, step 3, by SCENE's tick-boundary handler, except a tagged record whose discarded publication is unpublished, released by the publication handler after `OnDiscardedPublication` |

`Zenith_AdditiveSceneSet` (per world) holds the records, exposes the state and error of an instance to scripts through SCENE's bindings (`Scene.GetInstanceState(id)`) and the `scene.dump` command, and is part of the world's save state: SAVE's `Zenith.Scene.AdditiveSet` participant enumerates the records with `ForEachInstance` and restores them with `RestoreInstances`, which re-requests a `LOADING` or `QUEUED` instance under its recorded completion ID (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Additive set save and restore").

## Publication service

SCENE declares the upward interface it publishes additive loads through, `Zenith_ScenePublicationSink` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header"), because ZenithScene (layer 4) cannot name the runtime or SCRIPT that own the world's publication state ([Z2-N-ARCH-078]). The per-world state it maps onto is SCRIPT's asynchronous result table (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table": the completion-ID counter of [Z2-N-SCRIPT-334], one source record per `Zenith_AsyncSourceKind`, the completion ring, the tick-boundary step-1 publication and its per-source handlers).

| Sink member | Called by SCENE | What ZenithRuntime's implementation does on SCRIPT's table |
|---|---|---|
| `SetPublishHandler(pfnPublish, pUserData)` | once, in `Zenith_SceneWorldState::Attach` | declares the `ASYNC_SOURCE_SCENE_LOAD` source record (`uMaxInFlight` = `scene.maxAdditiveInstances`, `uMaxResultBytes` = 8) and installs a runtime-side `Zenith_AsyncPublicationHandler` on it that forwards each publication to `pfnPublish` with the `Zenith_TickBoundaryAccess&` it received |
| `AllocateCompletionID()` | `Scene.LoadAdditive` playback, sync point step 7, merge order — C++ requests only | `AllocateCompletionID(ASYNC_SOURCE_SCENE_LOAD)`; a full source returns 0, and SCENE then fails that load at commit with `SCENE_ERROR_LIMIT` (counted in `scene.additive.failed`) |
| `Park(id, publicationKind, payload)` | a preparation's completion continuation on its background thread, a failed commit, a refused start | `Complete(id, ASYNC_SOURCE_SCENE_LOAD, payload)` with the publication kind recorded beside it; the ID is one the table allocated for kind 8, by the sink or by SCRIPT's commit of a script request |
| `SetPublicationResolver(pfnResolve, pUserData)` (Stage 4) | once, in `Zenith_SceneWorldState::Attach`, after `SetPublishHandler` | forwards to SCRIPT's `SetPublicationResolver(ASYNC_SOURCE_SCENE_LOAD, …)`, which calls the resolver in live mode once per drained completion of kind 8 after the drain-and-sort and before logging (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Discarded publications of tagged loads"; row ST21) |
| the handler | — | invoked at the world's tick boundary, step 1, in completion-ID order, with the tick boundary's `Zenith_TickBoundaryAccess` |

The handler (`Zenith_PfnScenePublish`) receives the completion ID, the publication kind, the payload and the tick boundary's `Zenith_TickBoundaryAccess&`, the only world access a tick-boundary handler has (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptHost header"; `Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_ECSObservers header"). Its permitted actions: look up the instance record by completion ID in SCENE's own table; on success move the prepared scene into the activation queue and set `QUEUED`; on failure set `FAILED` with the payload's code and detail and release the prepared scene; for a tagged record carrying "discard requested", release any prepared scene the operation produced, call `OnDiscardedPublication` through the hook the load captured, and release the record (`Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Discarded publications of tagged loads", step 4); count the outcome. It writes no world state, records no command and changes no structure, so it needs nothing beyond that access, and it uses none of it; activation happens in phase 1 through `Scene.ActivateAdditive`. A publication naming an unknown completion ID (an untagged instance unloaded while loading) is counted and ignored.

Payloads: both publication kinds carry the 8-byte `Zenith_SceneLoadResult` `{ uint32 m_uInstance, uint8 m_uCode, uint8 m_uDetail, uint16 m_uReserved }` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AdditiveScenes header"). Success is `PUBLICATION_KIND_GENERIC_RECOMPUTED` with code 0 (the handler finds the instance by completion ID in SCENE's own table, and a replay recomputes the preparation, which yields the same instance ID); failure is `PUBLICATION_KIND_GENERIC_RECORDED` with the `Zenith_SceneError`'s code and detail, so a replay publishes the recorded failure even when its own preparation succeeded, and SCENE's handler then releases that prepared scene unactivated. The source and publication kinds cross the interface as `uint8` values of `Zenith2/Docs/Architecture.md` → "Multiplayer foundations", because ZenithScene cannot include ZenithSave's header.

Additive scene loads use the source kind `ASYNC_SOURCE_SCENE_LOAD`, value 8, assigned to SCENE in `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry" ([Z2-N-SCRIPT-924], [Z2-N-SCRIPT-925]), with the payload rule above; SCENE cites the value and never declares it. SCRIPT's asynchronous result table indexes sixteen source records (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"). Cross-area notes S10 and S11 are the SCENE-side records, S21 records the source-record parameters for SCRIPT and MOD, and S22 the 8-byte result and the script path.

## Cancellation

| Cancelled at | Released | Reported |
|---|---|---|
| `REQUESTED`, `READING` | the scene's asset handle | `OPERATION_STATUS_CANCELLED` |
| `DECODING` | the arena (the partial `Zenith_SceneData`) and the scene handle | `OPERATION_STATUS_CANCELLED` |
| `DEPENDENCIES` | every dependency handle acquired so far, the arena, the scene handle | `OPERATION_STATUS_CANCELLED` |
| `GATED` (tagged loads) | every dependency handle, the arena, the scene handle | `OPERATION_STATUS_CANCELLED` |
| `BUILDING` | interned strings (their references), the init storage, handles, arena | `OPERATION_STATUS_CANCELLED` |
| after `READY`, before activation | the prepared scene (by the requester, [Z2-N-CONTRACT-336]) | — |
| during isolated-world activation | the runtime destroys the isolated world (bulk destruction releases every entity's handles); SCENE releases the prepared scene | the transition's operation reports `OPERATION_STATUS_CANCELLED` |
| additive, after publication | the queue entry and prepared scene; no entity was created | instance `UNLOADED` |

Each cancellation is counted in `scene.prepare.cancelled` and in `scene.prepare.cancelled.<state>` for the state it reached (`PENDING` for a pending preparation removed before it started; `Zenith2/Docs/Design/SCENE/ModuleSeams.md` → "Preparation probe and hold"). A preparation that completes before it observes the cancel reports `OPERATION_STATUS_SUCCEEDED` ([Z2-N-CONTRACT-337]) and the requester releases the result. `Test_SceneLoader_CancelStress` performs 1,000 issue-then-cancel cycles at randomised stages and asserts no entity, asset reference, interned-string reference or allocation remains ([Z2-N-CONTRACT-339]).

## In-flight asynchronous results across a transition

The scenario `Test_Scenario_WorldTransitionWithInFlightAsync` ([Z2-N-ARCH-562], owned by GAME in `Zenith2/Tests/Lifecycle/`) runs through this path; SCENE's own integration test `Test_SceneTransition_InFlightAdditiveDiscarded` issues an additive load and 8 streaming loads in world A, requests a transition to scene B and asserts: the swap happens after the tick that published B's completion; A's additive load, completing after the swap, is discarded unpublished and counted in `world.async_discarded` ([Z2-N-ARCH-558]); nothing of A's scene appears in B; carry-over entities keep their persistent IDs; a `--replay` of the run swaps after the same tick.

## Missing-type handling at load

| Where | Unknown component type | Unknown property of a known type |
|---|---|---|
| document open (editor, cook, tools) | kept opaque, warned, round-trips byte for byte | kept in the field set, warned (`DOCUMENT_WARNING_UNKNOWN_FIELD`), round-trips |
| editor edit world | the component is not created; EDIT shows it as preserved unknown data; saving keeps it | the property is skipped when applying (`Zenith_ApplyFieldSet` skips and counts) and kept in the document |
| cook | cook error `SCENE_ERROR_UNKNOWN_TYPE` | cook warning; the property is not cooked |
| runtime preparation of cooked data | FAIL policy (default, and always for the startup scene): the preparation fails, nothing is activated; SKIP policy (`Zenith_SceneLoadDesc::m_eUnknownTypePolicy`, for tools and tests): the component is not created, its field set is kept in the prepared scene's unknown table, `scene.load.unknown_components` counts it | skipped with a counted warning |

ECS's failure table ("unknown type on load", `Zenith2/Docs/Design/ECS.md` → "Failure handling") is the SKIP row; the FAIL row is MOD's exit-code rule for the startup scene and the default for cooked content, which can only carry an unknown type when the cook and the Player disagree.
