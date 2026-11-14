# SAVE design — algorithms and state machines

This file is part of the SAVE design family. The entry document is `Zenith2/Docs/Design/SAVE.md`. It specifies, step by step and with complexity and failure behaviour, every algorithm SAVE runs, and every SAVE state machine as a table of states × events. The declarations are `Zenith2/Docs/Design/SAVE/PublicAPI.md`; the byte layouts `Zenith2/Docs/Design/SAVE/Formats.md`.

## Scope of this file

- Request intake, the phase-11 snapshot, the out-of-tick snapshot, image assembly and the journaled write.
- Journal recovery, the slot policy, autosaves, listings, loads, verification, slot export and import, the slot listener, migration and restore.
- The resume save, checkpoints, persistent world and entity state, profile switching and its intake freeze.
- Replay recording, playback, replay mode, seeking, resuming a replay from a save, and verification.
- The save system's shutdown sequence.
- The five SAVE state machines.

Notation: W is a bound world; t is the tick index of the state being saved (the state after tick t's end-of-tick playback); E the entity count of W; B the decoded bytes of a capture.

## Request intake

<!-- elaboration -->
Requests reach a world's binding from these producers, each already in a deterministic order:

| Producer | Thread and point | Order | Completion ID |
|---|---|---|---|
| script `Save.*` calls (`SIDE_EFFECT_KIND_SAVE_REQUEST`) | the committing world's merge node, through `Zenith_RuntimeSaveRequestConsumer::Consume` | merge order ([Z2-N-SCRIPT-226]) | allocated by the bridge's consumer on the committing merge node, in merge order, and passed in `m_ulCompletionID`: SCRIPT's commit hands the consumer 0 for a `SAVE_REQUEST` record (`Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Commit implementation" step 2), so `Zenith_RuntimeSaveRequestConsumer::Consume` calls `GetAsyncResults().AllocateCompletionID(ASYNC_SOURCE_SAVE)` for operations 1–4 (for a LOAD before the linked ID) and never for 0x80 or 0x81 ([Z2-N-SCRIPT-341] admits the allocation on the merge chain); SCRIPT's capacity class Z2-B-SCRIPT-025 has already accepted the request |
| C++ systems (`Zenith_CommandBuffer::Record` of the command kind `Save.Request`) | the sync point's playback step 7, through the command kind's playback function | merge order ([Z2-N-ARCH-481]) | allocated by the sink at intake step 5, after the per-tick limit of step 4 |
| the engine autosave | the phase-11 node, § "Autosave scheduling" | tick order | allocated by the world hooks on the phase-11 node |
| the runtime (thermal checkpoint, menu save of a paused world, tests) | the main thread between frames, `SubmitOutOfTick` | call order | none: the operation handle is the only result |
| PLAT's resume write, RHI's exit autosave | the main thread, `SubmitResumeWrite`, `SubmitExitAutosave` | call order | none |

`QueueRequest(W, r)` (the first two producers):

1. Look up W's binding; absent → `SAVE_ERROR_WORLD_NOT_BOUND` (the consumer logs `save: request for unbound world <tag>` and drops it; a script request's completion ID, which the consumer allocated in the world's own table, is completed by the consumer at once through that table with the 64-byte header carrying `SAVE_ERROR_WORLD_NOT_BOUND`, and a LOAD's linked ID with GAME's `WORLD_TRANSITION_FAILURE_PREPARE` payload, so no allocated ID is left unpublished).
2. Operation 0x80 (metadata) and 0x81 (autosave flag), which only scripts produce, each appended with a zero `Zenith_ScriptCapacityDemand` (no capacity class, so SCRIPT's save-request class Z2-B-SCRIPT-025 never counts them and the consumer allocates no completion ID for them, `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Script-facing API"): 0x81 sets W's autosave flag; 0x80 sets the key in W's per-tick metadata list (a key already present takes the new value; a key that would make the list hold more than `uSAVE_MAX_GAME_METADATA_PAIRS` (29) distinct keys is ignored, logged `save: metadata key '<key>' ignored: 29 keys already set this tick` and counted in `save.metadata.dropped`; the binding already refused a `zenith.` key with ZS5106 and oversized text with ZS5103). Return: nothing is queued or published, no serial and no completion ID is used. The list is cleared at phase-11 step 2.
3. Assign the request serial `W.m_uNextSerial++` (`uint32`, restored from the WORLD section at a load), for every request that reaches this step, script or C++, in merge order.
4. A C++ request: `W.m_uCppRequestsThisTick++`; when it is now above `save.maxRequestsPerTick`, the request is refused **before any completion ID of its own is allocated**: it joins the tick's limit batch — the first over-limit C++ request of the tick allocates the batch's one completion ID (`sink.AllocateCompletionID(W)`, `W.m_uOutstanding++`), and every over-limit request appends its serial to the batch (the first 4,000 serials are listed; the count is exact) and counts in `save.requests.refused` — and intake returns `SAVE_ERROR_REQUEST_LIMIT`. Phase-11 step 2 completes the batch with its `SAVE_OPERATION_REFUSED_BATCH` payload (`Zenith2/Docs/Design/SAVE/Formats.md` → "Publication payloads"), so the refusal is published at the next boundary and no request is ever dropped without a publication, while the tick admits at most `save.maxRequestsPerTick` + 1 C++ completion IDs whatever the number of commands. Script requests are not counted here: SCRIPT's merge already applied the same number as Z2-B-SCRIPT-025, and a script request over it was rejected there (BR-MERGE) and never reaches the consumer.
5. The completion ID: a C++ request: if `W.m_uOutstanding == uSaveMaxOutstanding` → refuse with `SAVE_ERROR_QUEUE_FULL` before allocating, count `save.requests.unpublishable` and log `save: request refused: <n> results outstanding (limit <m>)` (unreachable by the bound of `Zenith2/Docs/Design/SAVE.md` → "Asynchronous source capacity", now that step 4 bounds C++ IDs before allocation; `Test_SaveSystem_OutstandingCapacityBound` asserts it never happens); otherwise `ulCompletionID = sink.AllocateCompletionID(W)` (0 is the same defect, counted the same way). A script request uses `r.m_ulCompletionID`, which the consumer allocated on the same merge node just before calling `QueueRequest` (an allocation of 0 there is the same counted defect: the consumer logs `save: script request dropped: no ASYNC_SOURCE_SAVE completion ID` and does not call `QueueRequest`, unreachable by the capacity bound). Either way `W.m_uOutstanding++` and `save.outstanding.high_water` updates. Every check below that refuses completes this ID at once through `sink.Complete(W, ulCompletionID, payload)` with the 64-byte header carrying the serial of step 3; the result publishes at the next tick boundary like any other.
6. Validate a C++ request: the slot for MANUAL writes, deletes and loads with `Zenith_ParseSaveSlotName` (`SAVE_ERROR_INVALID_SLOT_NAME` or `SAVE_ERROR_RESERVED_SLOT_NAME`); its own metadata: more than 29 pairs, a key beginning `zenith.`, or the merge with the per-tick list (below) above 29 distinct keys → `SAVE_ERROR_INVALID_DESC`. The bindings already raised a Luau error for every script misuse, so scripts never fail here. The request's metadata is then fixed by one rule: **the per-tick list as it stands at this request's intake, with the request's own pairs (C++ only) replacing equal keys**, copied with the request; a copy of display name and metadata above 12 KiB (the most the limits allow is 29 pairs of 64 + 256 bytes plus a 128-byte display name, 9,408 bytes) is a defect of the caller and completes `SAVE_ERROR_INVALID_DESC`. A subsequent `Save.SetMetadata` therefore never changes a request already queued, and a deferred capture writes the pairs its request copied.
7. `SAVE_ERROR_SHUTTING_DOWN` after `BeginShutdown`, and (Stage 4) `SAVE_ERROR_PROFILE_MISMATCH` under the profile rule of § "Profile switch intake freeze": complete at once.
8. A WRITE: if W's pending list holds `save.maxQueuedRequests` entries → complete `SAVE_ERROR_QUEUE_FULL`. If a pending (not yet captured) WRITE of the same (kind, slot) exists, mark it superseded: it completes `SAVE_ERROR_SUPERSEDED` when the new one is captured; a request already captured is never superseded (both are written in serial order and the subsequent one wins on disk). Then append `{serial, completion ID, request copy}` to the pending list, copying display name and metadata into W's intake arena.
9. A LIST, DELETE or LOAD. A script LOAD (Stage 4) carries `r.m_ulLinkedCompletionID`, the requesting world's `ASYNC_SOURCE_WORLD_LOAD` ID the consumer allocated on the same merge node just before, which SAVE stores opaque for the hand-off (§ "Load pipeline"); a linked ID of 0 means the world already holds a transition or another load in flight (that source's capacity is 1), and the SAVE ID completes at once with `SAVE_ERROR_TRANSITION_BUSY`, queueing nothing. Otherwise `TryPush` into the operation queue; full → complete `SAVE_ERROR_QUEUE_FULL`. **Every refusal of a script LOAD at steps 7–9 also completes its linked ID** (`sink.CompleteLinkedLoadRefused(W, linked)`: GAME's failure payload `WORLD_TRANSITION_FAILURE_PREPARE`), and so does a cancellation of a queued script LOAD by `BeginShutdown`; so the world's single world-load slot is never leaked (§ "Script loads and the world host").

Every value these steps assign (completion ID, serial, outstanding count, the C++ count, the limit batch) is a function of the merge order and of recorded publication ticks alone; whether a refusal happens may depend on writer progress (a full pending list), but the refusal is a recorded publication, so a replay reproduces it without re-deciding it. Complexity O(1) per request plus O(pending) for step 8's supersede scan and O(29) for the metadata merge. The pending list, the counters, the metadata list, the limit batch and the intake arena are W's, touched only by W's merge nodes, sync points, phase-11 node and tick-boundary handler, which the frame graph serialises; no lock is needed.

`SubmitOutOfTick(W, r)` runs on the main thread with no frame running, for any bound world (paused or between two ticks of a running one, whose state is then the state after its last completed tick): it validates the request (steps 6 and 7, the profile rule included, which `SubmitResumeWrite` and `SubmitExitAutosave` apply too, returning `SAVE_ERROR_PROFILE_MISMATCH` synchronously), acquires an ordinary capture arena (none free → `SAVE_ERROR_QUEUE_FULL` returned synchronously; the caller retries next frame), runs § "Out-of-tick snapshot" at once, pushes the job and returns the operation handle. It allocates no completion ID and no serial, publishes nothing and changes no world state, so a thermal step or a menu save never shifts a subsequent completion ID.

## Phase-11 snapshot

The system `Save.Snapshot` of W runs in phase 11 on any worker (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases"), declaring `ReadAllComponents()`, `ReadAllResources()`, a read of every participant's declared resources, and a write of W's snapshot buffer resource `Zenith.SaveSnapshotBuffer`, the one write phase 11 admits ([Z2-N-ARCH-503]). Its body calls `Zenith_SaveSystem::CaptureForTick(W, inspector, t)`:

1. **Autosave decision** (§ "Autosave scheduling"): when an autosave is due, allocate its completion ID (`hooks.AllocateSnapshotCompletionID()`) and serial now, `W.m_uOutstanding++`, record it as W's outstanding autosave, and append it to the pending list as an AUTOSAVE write (or complete it `SAVE_ERROR_QUEUE_FULL` when the pending list is full, and, Stage 4, `SAVE_ERROR_PROFILE_MISMATCH` when the profile rule of § "Profile switch intake freeze" refuses W, counted in `save.autosave.profile_refused`: the autosave is skipped, never its allocation). The allocation happens on the due tick whether or not a capture arena is free and whether or not the run records.
2. End the tick's intake: when W has a limit batch (intake step 4), complete its ID with the `SAVE_OPERATION_REFUSED_BATCH` payload through `sink.Complete` and clear it; reset `W.m_uCppRequestsThisTick` to 0 and clear the per-tick metadata list (phase 11 runs after the tick's last merge node and sync point, so every intake point of the tick has run).
3. Collect the due work: the pending list; a replay checkpoint if W has a recorder and `t mod replay.checkpointIntervalTicks == 0`. Nothing due → return (the common case costs two branches).
4. Acquire an ordinary capture arena (indices 0 or 1) from the free-arena mask (`Zenith2/Docs/Design/SAVE.md` → "Cross-thread hand-offs"). None free → the pending list stays for the next tick's phase 11 (`save.snapshot.deferred` counts), a due replay checkpoint is skipped (`save.replay.checkpoints_skipped`), return. The deferral is invisible to simulation: every request's completion ID and serial were assigned at intake, and the result each publishes is recorded.
5. `xAsync = hooks.CaptureAsyncState(arena)`, `ulNext = hooks.GetNextCompletionID()`.
6. **Exclusions**: for each participant in registration order, `CollectExcludedEntities(ctx, list)`; sort the list ascending. Expand it to descendants: one pass over the entity list in simulation-ID order builds a child index (for each entity, append it to its parent's child list in an arena array, O(E)), then a depth-first walk from each excluded root marks its whole subtree (each entity visited at most once, O(E)). This is correct whatever the order of simulation IDs along a parent chain, including an entity reparented under a subsequently spawned one.
7. Run `Zenith_WorldSnapshotter::Capture` (§ "Snapshot capture"); its fidelity is the weakest any participant reported (`GetCaptureFidelity`), recorded with the job.
8. On failure: release the arena to the mask, complete every collected request with the error (payload result code; `m_ulDetail` logged), clear them from the pending list, return the error (logged by the node, not fatal).
9. A due replay checkpoint: assemble a one-world `SAVE_KIND_REPLAY_CHECKPOINT` image from the captured sections at once on this node (codec NONE, so the cost is the capture plus one copy) and call `AddCheckpoint(tag, t, image)`; it allocates no completion ID and changes no world state. With no pending request, release the arena and return.
10. Copy W's thumbnail (when set, at most 512 × 288 × 4 = 589,824 bytes) into the capture arena, so the writer never reads the binding's copy, which `SetThumbnail` may overwrite between frames. Hand off: `writerQueue.TryPush(job{ W, t, arena index, sections, fidelity, request list (each request with its own metadata copy, intake step 6), autosave flag, thumbnail copy })`; the queue (capacity 2, one job per ordinary arena) never refuses, because a job exists only for an acquired arena. The pending list is cleared of the captured requests.

Cost: O(E × C) property encodes, where C is the mean serialized component count, plus O(R) resource encodes; memory B ≤ `save.imageArenaBytes`. Its CPU time is the zone `Save.Snapshot`, measured against Z2-B-SAVE-002 on the frames that save ([Z2-N-BUDGET-224]).

### Snapshot capture

`Zenith_WorldSnapshotter::Capture(desc, async, arena, out)`:

1. **WORLD**: write the 96-byte record from the desc (`Zenith2/Docs/Design/SAVE/Formats.md` → "World section"): tag, flags, seed, t, next simulation ID, playtime ticks, base scene, counts, the request serial counter, the outstanding autosave completion ID.
2. **ENTITIES**: `n = inspector.GetEntityCount()`; for `uFirst = 0; uFirst < n; uFirst += 1024`, `ListEntities(uFirst, span of 1024)` (simulation-ID order). For each entity h not pending destroy and not excluded:
   1. simulation ID, persistent ID and parent's simulation ID from the inspector (`GetSimulationID`, `GetPersistentID`, `GetParent`); a parent that is excluded or pending destroy makes h a root in the save and is counted in `save.snapshot.orphaned`.
   2. `ListComponents(h)` (stable-name byte order) → sort by type ID.
   3. For each component type T on h, unless T has at least one property and every property is transient: `resolver.SetContext(simulation ID, T)`; `ReadComponent(h, T, scratch)` into a 64-byte-aligned scratch block of `T.m_uSize` bytes (a zero-property tag type has size 1 and encodes as a record with no fields); `Zenith_CaptureFieldSet(T, scratch, strings, resolver, true, arena)`; `Zenith_EncodeFieldSet` into the section; a capture error → `SAVE_ERROR_SERIALIZE` with `m_ulDetail` = T's ID.
   4. Append the unknown-carry components of h's simulation ID (binary search in the carry, sorted by simulation ID) and set flag bit 0.
   5. Back-patch the record's component count and byte length.
3. **EVENTS**: for each carried event in delivery order (`ListCarriedEvents`, assumption fenced in the entry document's "Assumed interfaces from other areas"): type ID, merge key, and the event payload's field set captured as in 2.3.
4. **ASYNC**: next completion ID and the two lists from `async`, already sorted.
5. **RESOURCES**: `ListResources` (ascending type ID); for each resource type with at least one serialized property: `resolver.SetContext(0, type)`, `ReadResource` into scratch, `Zenith_CaptureFieldSet(…, true, …)`, encode; then interleave the carry's unknown resources at their type IDs (`Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section").
6. **UNKNOWN_CARRY**: the carry's sections, when any.
7. **PARTICIPANT**: for each participant: a `Zenith_SaveSectionWriter` over the remaining budget, `CaptureSection(ctx, writer)`; an error → `SAVE_ERROR_PARTICIPANT` with the key; the section's flags get REQUIRED when the participant's flags say so; then `GetCaptureFidelity(ctx)`, and the first COHERENT answer makes the capture COHERENT and names that participant's key for the log.
8. Every write is bounds-checked against `m_ulBudgetBytes`; the first overflow fails the capture with `SAVE_ERROR_IMAGE_TOO_LARGE`.

The resolver maps an entity reference to its target's persistent ID, to the save-local form (simulation ID) for a runtime-only target, or to null for a stale handle or a target pending destroy (`IsPendingDestroy`, uncounted) (`Zenith2/Docs/Design/SAVE/Formats.md` → "Saved entity references"); asset references through the asset value kind's `m_pfnToPersistentID`. Containers are read through `ReadContainer` and encoded element by element. Nothing in the output depends on chunk layout, worker count, slot numbers or addresses, so the decoded section bytes are a pure function of W's state after tick t ([Z2-N-ARCH-884] reasoning applied to saves): `Test_Snapshot_BytesIndependentOfWorkersAndLayout`.

## Out-of-tick snapshot

Used by `SubmitOutOfTick` (a paused world, or a running world between frames), the resume write, the exit autosave, the restored-world replay checkpoint and tests. The request's metadata is the request's own pairs (no per-tick list exists out of tick) and the thumbnail is copied into the capture arena as in § "Phase-11 snapshot" step 10. It is § "Snapshot capture" with `xInspector = W.GetInspector()` on the main thread, `ctx.m_bOutOfTick = true`, no pending-destroy entities to skip (none exist out of tick), and the same output. The capture must run with no frame of W running (`ZENITH_ASSERT(!W.IsInFrame())`). Its arena: an ordinary one for `SubmitOutOfTick`; arena 2 (the priority slot's) for the resume write and the exit autosave; the restored-world checkpoint (under `--record` only) captures into a transient arena from the save system's heap, capped at `save.imageArenaBytes` and freed once `AddCheckpoint` copied the image, so it never waits for or competes with the capture arenas and is never skipped; an allocation failure or an image above the checkpoint bound stops recording instead (the log keeps its committed prefix, which ends before the restored session), because a restored session without its checkpoint could not be replayed.

## Image assembly and compression

On `Zenith.SaveWriter`, after each wake (the wake semaphore with a 100 ms timeout, so shutdown is observed), the thread pops one job from the priority slot, else the writer queue, else the operation queue (§ "Load pipeline", § "Listing slots" and § "Slot policy" run operation jobs), and for each request of a write job in serial order:

1. Skip a superseded request (complete `SAVE_ERROR_SUPERSEDED`) and a cancelled one (`SAVE_ERROR_CANCELLED`).
2. Resolve the target: § "Slot policy" gives the directory and slot.
3. Header: format version current; kind; sequence = `++m_ulProfileSequence`; UTC seconds from `Zenith_GetUtcTime()` now (read on the writer thread, never by simulation code, [Z2-N-ARCH-885]); build ID, composition and content hashes and release line from the desc; tick t; own completion ID and own request serial = the request's (both 0 for an out-of-tick, resume or exit request); the replay position when W has a recorder (`writer.GetWorldOrdinal(W)`, t); flags (`HAS_THUMBNAIL`, `HAS_REPLAY_POSITION`, and `COHERENT_ONLY` when the job's capture fidelity is COHERENT, logged `save: '<slot>' written COHERENT_ONLY (<participant> reported coherent)` and counted in `save.fidelity.coherent`).
4. Metadata: slot, display name, playtime = W's playtime ticks at t ÷ `sim.tickHz`, world count 1 (every SAVE image holds one world: the resume and exit-autosave images capture the primary world only), the request's copied pairs (at most 29, none beginning `zenith.`) plus the engine pairs `zenith.location`, `zenith.release`, `zenith.profile` (the active profile key), at most 32, sorted by key.
5. `builder.Begin(header, metadata, {codec, level}, assemblyArena)`; `SetThumbnail` from the job's thumbnail copy when it has one; `AddSection` for each captured section in (kind, key) order; `Finish`. Every section is compressed with `Zenith_CompressSection` (one-shot; scratch from `GetCompressScratchBound`), stored when it would not shrink. Errors → complete the request with `SAVE_ERROR_COMPRESSION`, `SAVE_ERROR_IMAGE_TOO_LARGE` or `SAVE_ERROR_SECTION_LIMIT`. Steps 3–5 check the request's `Zenith_SaveCancelToken` before step 3 and before each `AddSection`; a requested cancellation abandons the builder (the assembly arena is reset) and completes the request `SAVE_ERROR_CANCELLED` with nothing written, so an assembly is cancellable between any two sections.
6. Replay mode (`m_bReplayMode`): § "Replay mode" — no storage access and no completion; go to step 8.
7. Otherwise § "Journaled write sequence"; then complete the request with the outcome: on success the 64-byte header {WRITE, kind, 0, flags 0, serial, bytes = file size, snapshot tick t, slot}, exactly the payload an ordinary restore of this file rebuilds for its own entry (`Zenith2/Docs/Design/SAVE/Formats.md` → "Publication payloads"); `save.write.latency_us` and `save.write.last_bytes` updated. A successful CHECKPOINT request also builds a `Zenith_PreparedSave` over a copy of the job's decoded sections in its own arena, carrying the header of the file just written (its `GetHeader()` has the written `m_ulFileBytes`, `m_ulOwnCompletionID` and `m_uOwnRequestSerial`, so an ordinary restore of the retained checkpoint rebuilds the own entry's payload exactly as a restore of the file does), and hands it to the main thread through W's pending-checkpoint slot (§ "Checkpoints").
8. After the last request of the job, reset the capture arena and release its bit to the free-arena mask (`memory_order_release`).

Compression is level `save.compressionLevel` (default 3, clamped to `iSAVE_ANDROID_MAX_COMPRESSION_LEVEL` on Android) with zstd: about 3.5 MiB of context per compression, inside the 3.5 MiB S24 guidance of COMPRESS (`Zenith2/Docs/Design/COMPRESS.md` → "Memory per context"). The time of steps 3–7 is `m_ulLastWriteNs`.

## Journaled write sequence

<!-- elaboration -->
`Zenith_RunJournaledSaveWrite(storage, {D, S, keepBackup, overwriteNewer}, N, seq, scratch, cancel, faults)`; paths `D/S.zsave` (target), `D/S.zsave.bak`, `D/S.zsave.z2tmp`, `D/S.z2journal` (`Zenith2/Docs/Design/SAVE/Formats.md` → "File names and directories"). This is Contracts' table ([Z2-N-CONTRACT-540]) for one document with the save backup policy of [Z2-N-CONTRACT-553]:

| Step | Action | Journal after | Cancellable |
|---|---|---|---|
| 0a | `CreateDirectory(D)`; when `D/S.z2journal` exists, recover it first (§ "Journal recovery") | — | yes |
| 0b | `O = ReadWhole(target)` when it exists; parse its header; a format version newer than current and `!overwriteNewer` → `SAVE_ERROR_SLOT_NEWER_VERSION`, nothing written; otherwise validate O fully (`Zenith_SaveImageView::Open` + `VerifyAll`, every section decoded once with both CRCs checked, the cancel token checked before each section's decode); invalid → flag `OLD_INVALID` | — | yes, between sections |
| 0c | free space: `QueryFreeBytes() ≥ 2·|N| + |O| + storage.GetLowDiskMarginBytes()` (PLAT's `io.lowDiskMarginBytes` on real storage, so `WriteAtomic`'s own low-disk refusal cannot arrive after step 1), else `SAVE_ERROR_LOW_DISK`, nothing written | — | yes |
| 1 | `WriteAtomic(D/S.z2journal, ZSJN{PREPARED, seq, S, sha(O), sha(N), |O|, |N|})` | PREPARED | yes |
| 2 | when O exists, is valid and `keepBackup`: `WriteAtomic(D/S.zsave.bak, O)`, flag `BACKUP_WRITTEN`; rewrite the journal | BACKED_UP | yes |
| 3 | `WriteAtomic(D/S.zsave.z2tmp, N)`; rewrite the journal | WRITTEN | yes, until the temporary's rename inside `WriteAtomic` |
| 4 | rewrite the journal COMMITTING; `Rename(D/S.zsave.z2tmp, target)` (PLAT's atomic replace, `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH` or `rename` + directory `fsync`); rewrite the journal with `RENAMED` | COMMITTING | no |
| 5 | `Delete(D/S.z2journal)` | none | no |

- A failure in steps 0–3 deletes the temporary (best effort), deletes the journal and returns the error: the target is O and the backup is either the previous backup or O, both valid saves of the slot.
- A failure in step 4 or 5 leaves the journal for § "Journal recovery" (the next write of the slot or the next start runs it) and returns `SAVE_ERROR_IO`.
- An invalid O is never copied over a valid backup, so bit rot in the current save cannot destroy the last good one.
- Each `WriteAtomic` is itself crash-atomic (PLAT W1–W7, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"), so the journal file is always a complete ZSJN of one state.
- **Longest uncancellable unit.** With the checks of § "Image assembly and compression" and step 0b, a write observes a cancellation within one of: one storage call (one `WriteAtomic` chunk, PLAT checking the token between chunks, or one chunk of `ReadWhole`, which takes the token too), one section's compression (a captured section is at most `save.imageArenaBytes` decoded), or one section's decode and verification in step 0b (at most 256 MiB decoded for a section of an older file). After step 4 begins nothing is cancellable, and only a journal rewrite of 176 bytes, one rename and one delete remain. That unit bounds both the exit autosave's rollback wait and the shutdown step 7b join (§ "Shutdown sequence").
- Fault points: `uFAULT_POINT_SAVE_JOURNAL_STEP + n` before step n's first operation; `0x53560020` between the rename and the journal rewrite that follows it. A test that injects a fault abandons the write as a crash would (no cleanup runs) by destroying the storage's view of in-flight state (`Zenith_MemorySaveStorage::SimulatePowerLoss`).

Cost: two full-file writes (backup and temporary) plus three journal writes of 176 bytes, one rename and one delete: O(|N| + |O|) bytes of I/O.

## Journal recovery

`Zenith_RecoverSaveJournals(storage, D, arena)` runs at `Zenith_SaveSystem::Create` for `saves/` (the resume journal) and the initial profile directory, at every `SetActiveProfile` for the new directory, and at step 0a of a write of a slot with a pending journal. For each `*.z2journal` in byte order of name, with S the slot named by the file stem:

| Found | Action | Outcome |
|---|---|---|
| unreadable, or `Zenith_DecodeSaveJournal` fails, or its slot differs from the stem | `Rename(journal, journal + ".corrupt")` (replacing); `Delete(D/S.zsave.z2tmp)` | `SAVE_RECOVERY_JOURNAL_CORRUPT` |
| PREPARED, BACKED_UP | `Delete(tmp)`; `Delete(journal)` | ROLLED_BACK |
| WRITTEN | tmp exists and `sha(tmp) == new` → continue as COMMITTING; otherwise `Delete(tmp)`, `Delete(journal)` | ROLLED_FORWARD or ROLLED_BACK |
| COMMITTING | `sha(target) == new` → `Delete(tmp)`; else tmp valid (`sha == new`) → `Rename(tmp, target)`; else target absent or `sha(target) == old` → `Delete(tmp)` (the rename never happened; roll back); else (target torn) `sha(bak) == old` → `WriteAtomic(target, bak)`; else nothing matches → `Rename(journal, journal + ".conflict")`, leave every file | ROLLED_FORWARD, ROLLED_BACK, RESTORED_BACKUP or CONFLICT |

After the journals: every `*.zsave.z2tmp` without a journal is deleted and counted (`m_uStaleTemporariesRemoved`). Every action is idempotent and the journal is deleted last, so recovery interrupted at any point and re-run converges to the same files ([Z2-N-CONTRACT-540]); `Test_SaveJournal_RecoveryInterruptedAndRerunConverges`. Each outcome is logged at info level (`save: recovered '<slot>': <outcome> (found <state>)`) and counted; CONFLICT and JOURNAL_CORRUPT are warnings, and the slot then lists from whatever files exist. Cost O(journals × file size) for the digests.

## Slot policy

<!-- elaboration -->
| Kind | Directory | Slot name | Backup | Retained in memory | Selection |
|---|---|---|---|---|---|
| MANUAL | `saves/<profile>` | the request's | yes | no | the named slot |
| QUICK | `saves/<profile>` | `quick` | yes | no | one slot |
| AUTOSAVE | `saves/<profile>` | `autosave-<i>`, i in [0, `save.autosaveSlotCount`) | yes | no | the first i with no valid save, else the i whose valid header has the lowest sequence; ties to the lower i |
| CHECKPOINT | `saves/<profile>` | `checkpoint` | yes | yes (the decoded sections, per world) | one slot |
| RESUME | `saves` | `resume` | no | no | PLAT's path |
| REPLAY_CHECKPOINT | — | — | — | — | written into the replay log only |

The autosave selection reads only the in-memory slot table built at profile activation (headers of every `.zsave` in the directory, via `ReadPrefix(128)`) and updated after every write, so it does no I/O at selection. A manual slot holds at most one save plus its backup; deleting a slot (`BeginDelete`) deletes `S.zsave`, then `S.zsave.bak`, then `S.zsave.corrupt`; an interruption between the deletes leaves only a backup, which the next listing reports as `SAVE_SLOT_STATE_RECOVERABLE_FROM_BACKUP`, never a corrupt slot.

## Autosave scheduling

<!-- elaboration -->
At step 1 of § "Phase-11 snapshot", an engine autosave is due when all hold, every term being simulation-visible state:

1. `save.autosaveIntervalTicks > 0` (simulation-affecting, in the replay header);
2. W is the primary world;
3. W's autosave flag is enabled (`Save.SetAutosaveEnabled`; WORLD flags bit 2, saved and restored);
4. `t mod save.autosaveIntervalTicks == 0`;
5. W has no outstanding autosave: no AUTOSAVE completion ID of W has been allocated and not yet published. The binding's outstanding-autosave ID is set at allocation, cleared by `OnResultPublished` at the tick boundary that publishes it, and saved in the WORLD section, so a restore carries it; publication ticks are recorded, so a recorded run and its replay agree on this term although the replay writes nothing.

Writer progress (whether the previous autosave is still being written), arena availability and `--record` appear in none of these terms. When due, the completion ID comes from `hooks.AllocateSnapshotCompletionID()` and the serial from W's counter, both on the phase-11 node (cross-area row SV3: SCRIPT's table admits that allocation for engine autosaves on the phase-11 node, which the frame structure serialises after every merge node of the tick and before the next boundary), so the autosave's identity is a function of the tick alone. A due autosave whose capture finds no free arena is not dropped and not re-decided: it stays in the pending list with its ID and serial and is captured at the next phase 11 with a free arena; its result names the subsequent snapshot tick. A cinematic or any game state suppresses autosaves through the flag. The exit autosave of RHI's device-loss clean exit is not an engine autosave in this sense: it is `SubmitExitAutosave`, out of tick, with no completion ID (§ "Shutdown sequence").

`Test_SaveRuntime_AutosaveReplaysIdentically` records 600 ticks with interval 100 over a storage that blocks every write for 3 s, replays the log, and asserts identical tick hashes, identical publications (completion IDs, ticks, payloads) and an identical next completion ID at tick 600.

Term 5 reads a publication tick, which is writer timing: two live runs with different storage speeds may legitimately allocate autosaves at different ticks (interval 10 at 60 Hz is about 167 ms; a 500 ms write leaves tick 10's autosave unpublished at tick 20, so tick 20 allocates none, while a 0 ms run allocates one). What SAVE guarantees is record ↔ replay: a replay reads the recorded publication ticks and allocates exactly the recorded run's autosaves (`Test_SaveSystem_AutosaveIdentityReplaysAcrossWriterSpeeds`). Two live runs agree only when their SAVE results publish at the same ticks, which tests arrange with `Zenith_MemorySaveStorage`'s write gate (`Zenith2/Docs/Design/SAVE/Tests.md` → "Save system tests").

## Listing slots

<!-- elaboration -->
`BeginListSlots()` issues an operation run on the writer thread:

1. `ListFiles(saves/<profile>)`; group names by stem into (`S.zsave`, `S.zsave.bak`) pairs; ignore `.z2tmp`, `.z2journal`, `.corrupt`, `.conflict`.
2. For each stem in byte order (at most `uSAVE_MAX_SECTIONS` stems; more are counted and skipped): `ReadPrefix(file, uSAVE_LISTING_PREFIX_BYTES)` (131,072 bytes, which holds the header, a full 1,024-entry table and a maximal METADATA section); `Zenith_ParseSaveHeader`; `Zenith_ParseSaveSectionTable`; the METADATA entry must lie inside the prefix (else the stem is CORRUPT: a valid writer always places it first); check its stored CRC; `Zenith_ParseSaveMetadata`.
3. State: header newer → NEWER_VERSION; other major → INCOMPATIBLE; the save's prefix valid → VALID; else the backup's prefix valid → RECOVERABLE_FROM_BACKUP (its metadata is shown); else CORRUPT.
4. Complete with the slot array (`TakeSlotList` copies it into the caller's arena) and, for a script request, publish the LIST payload of the first 64 slots.

A listing reads at most 128 KiB per file and never decodes a compressed section, so it costs O(slots) small reads. VALID here means "the header, table and metadata verify"; a load verifies everything (§ "Verification and corruption recovery").

## Load pipeline

<!-- elaboration -->
`BeginLoad(kind, slot)` and `BeginLoadPath(path)` (C++), and a script `Save.Load` (Stage 4, through intake step 9), issue an operation run on the writer thread (one load at a time; a second waits in the queue):

1. `ReadWhole(target, 256 MiB, loadArena, cancel)`.
2. § "Verification and corruption recovery" (may switch to the backup).
3. Decode every section into a `Zenith_SaveImageTree` (its own arena, charged to Z2-B-SAVE-001).
4. `Zenith_MigrateSaveImage(registry, tree)` (§ "Migration pipeline").
5. Build the `Zenith_PreparedSave` (header, per-world index of sections, fidelity = COHERENT when the header flag says so); complete SUCCEEDED (`save.load.last_us` updated).

The world side, owned by GAME's world host (`Zenith2/Docs/Architecture/LifecycleCommands.md` → "World transition"): create one isolated world with the saved seed (`GetWorldSeed(0)`; a save's world count is always 1, and GAME refuses any other count with `WORLD_TRANSITION_FAILURE_SAVE_WORLD_COUNT`), attach modules exactly as for a scene-loaded world — `Zenith_RuntimeSave_AttachWorld` binds the world to SAVE (`BindWorld`) and registers its participants and snapshot system — then `MakeRestoreDesc(newTag, false)` (`MakeRestoreDesc(newTag, true)` when the transition is a replayed one, under `--replay`; `Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine" row `RESERVING` names the call without the flag, cross-area row GA11) and `BeginWorldRestore(0, desc)` (cross-area row GA11 (b): bind before restore), `Step` in prologue step 7 within `save.restoreBudgetUs` per frame, and swap at a tick boundary when DONE. At the swap GAME calls `SwapWorld`, then `BeginWorld`, then `Zenith_RuntimeReplay_LogRestoredWorld` under `--record` (GA11 (c)).

Cancellation is checked between steps and inside the read; a cancelled load releases its arenas on the writer thread and completes CANCELLED (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", cancellation completion).

### Script loads and the world host

A script `Save.Load(slot)` (Stage 4, with GAME's save-load transition) carries two completion IDs of the requesting world, both allocated at intake in merge order on the committing merge node by the bridge's consumer (SCRIPT's commit hands it 0 for the side effect): first the `ASYNC_SOURCE_SAVE` ID (`m_ulCompletionID`), then the `ASYNC_SOURCE_WORLD_LOAD` ID right after it (`m_ulLinkedCompletionID`). Then:

| Step | Who, where | `ASYNC_SOURCE_SAVE` ID | `ASYNC_SOURCE_WORLD_LOAD` ID |
|---|---|---|---|
| the consumer cannot allocate the linked ID (a transition or another `Save.Load` of the world is in flight; GAME declares the source with capacity 1) | the merge node, intake step 9 | completed at once: `SAVE_ERROR_TRANSITION_BUSY`, `Operation` 4 | none was allocated |
| intake refuses the load (`SAVE_ERROR_SHUTTING_DOWN`, `SAVE_ERROR_QUEUE_FULL`), or `BeginShutdown` cancels it while queued | the merge node, intake steps 7–9; the main thread at shutdown | completed with that error (or `SAVE_ERROR_CANCELLED`) | completed at the same point through `sink.CompleteLinkedLoadRefused` with GAME's 8-byte failure payload carrying `WORLD_TRANSITION_FAILURE_PREPARE` |
| the load operation finishes (steps 1–5) | the writer thread | completed: Result 0 (read, verified, migrated) or the load's error, `Operation` 4 | untouched |
| `Zenith_RuntimeSave_PumpMainThread` → `TakeFinishedScriptLoad(W)` | the main thread, prologue step 7 | — | success: `Zenith_WorldHost_BeginTransitionFromSave(state, W, move(save), worldLoadID)`, after which GAME owns the ID and publishes the transition's success or failure (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Publication payloads"); failure: the bridge completes it with GAME's 8-byte failure payload carrying `WORLD_TRANSITION_FAILURE_PREPARE` |

So every allocated ID is completed exactly once on every path (as X255's rule for every producer of a world's table asks), the world's single world-load slot is released whatever happens to the load, the requesting world always receives `Zenith.SaveCompleted` (`Operation` 4), and on failure it stays. An `UnbindWorld` of the requesting world (it was destroyed) drops both completions with its table. In replay mode the refusal paths still call the sink, and SCRIPT discards those live completions because the recorded publications of the same IDs are the ones it publishes (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table"). In replay mode the writer reads nothing and completes nothing (§ "Replay mode"): the recorded `ASYNC_SOURCE_SAVE` publication arrives at its recorded tick; when it carries Result 0, the bridge's replay form takes the restored world from the log instead of a file — `GetSessionAfterSwap` of the requesting world's session, its `GetRestoredCheckpoint`, `PrepareFromImage` — and calls `Zenith_WorldHost_BeginTransitionFromSave` with that prepared save and the linked ID, while GAME's own replay rule times the swap from the log through `FindNextPublication` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Replay of a transition"). A recorded failure needs nothing more: when the load failed before the hand-off, GAME's handler ignores the bridge-completed ID (the host never held it); when it failed after the hand-off, GAME's handler ends its record from the recorded failure payload. Under `--replay` the transition GAME begins with the replayed prepared save restores its world with `MakeRestoreDesc(newTag, true)` (a live run passes `false`, § "Load pipeline"), because every recorded result of the restored world, the save's own included, then comes from the log.

## Verification and corruption recovery

1. `Zenith_SaveImageView::Open(bytes)`: header CRC, magic, version, sizes, table CRC, table rules.
2. `VerifyAll`: decode every section once, checking the stored CRC before decompression and the decoded CRC after.
3. Any error for which `Zenith_IsSaveErrorRecoverableByBackup(code)` holds (BAD_MAGIC, TRUNCATED, CHECKSUM, CORRUPT, COMPRESSION), or a read failure of the save while the backup exists: log `save: '<slot>' failed validation (<error>, section <kind> world <i>); trying the backup`, read `S.zsave.bak`, run steps 1–2 on it.
4. The backup passes → use it, set `m_bFromBackup`, `Rename(S.zsave, S.zsave.corrupt)` (replacing an older `.corrupt`), and count `save.load.from_backup`; the next write of the slot then has no current file and writes the new save without touching the backup (step 2 of the journal finds no O).
5. Both fail → `SAVE_ERROR_CORRUPT` (or the save's own error code), nothing renamed.
6. VERSION_NEWER and VERSION_MAJOR are not recovered from the backup (the backup was written by the same build line and is checked the same way only if the save is corrupt).

## Slot export and import

<!-- elaboration -->
Stage 4, for ONLINE's cloud model (A3 rows ON1 (a), ON1 (b), ON16 (d); Z2-D-074: SAVE gives profiles and slot metadata, ONLINE owns the cloud). Both are operations of the operation queue, run on the writer thread, in the active profile's directory `D = saves/<active profile>`. They share one **transfer arena** (1 MiB blocks from the save system's heap, at most 256 MiB, charged to Z2-B-SAVE-001): at most one export or import is running or holds untaken bytes, and a second `Begin` returns `SAVE_ERROR_QUEUE_FULL`. Both return `SAVE_ERROR_REFUSED_IN_REPLAY` in replay mode, `SAVE_ERROR_SHUTTING_DOWN` after `BeginShutdown`, and `SAVE_ERROR_PROFILE_MISMATCH` while a profile switch to another profile is pending (§ "Profile switch intake freeze"); each is a cancellable `Zenith_OperationHandle`.

### Slot export

`BeginExportSlot(S)`:

1. `ReadWhole(D/S.zsave, 256 MiB, transfer arena, cancel)`, then `Zenith_SaveImageView::Open` and `VerifyAll` with the decode scratch in the load arena.
2. A failure for which `Zenith_IsSaveErrorRecoverableByBackup` holds, or a read failure while `D/S.zsave.bak` exists: the same two steps on the backup; it passes → the export carries the backup's bytes with `m_bFromBackup` (a slot listed RECOVERABLE_FROM_BACKUP exports its backup, row ON16 (d)). VERSION_NEWER and VERSION_MAJOR are not retried on the backup (§ "Verification and corruption recovery" item 6); no file of the slot → `SAVE_ERROR_NOT_FOUND`.
3. An export renames, quarantines and writes nothing. It completes SUCCEEDED with the bytes held in the transfer arena; `GetExportedSlotBytes` returns their length. `TakeExportedSlot(h, xOut)` copies them into `xOut`, fills `Zenith_SaveExportedSlot` from the header (sequence, UTC seconds, kind, version, the COHERENT_ONLY flag) and METADATA (slot name, playtime seconds, a display-name view into `xOut`), and releases the arena; destroying the handle releases it too.

### Slot import

`BeginImportSlot(T, xFile)`: a file above `ulSAVE_MAX_FILE_BYTES` is `SAVE_ERROR_IMAGE_TOO_LARGE`; otherwise the main thread copies it into the transfer arena (`SAVE_ERROR_OUT_OF_MEMORY` when it cannot) and queues the operation:

1. `Open` and `VerifyAll`, as at a load (no backup to fall back to); a newer format is `SAVE_ERROR_VERSION_NEWER`, another major `SAVE_ERROR_VERSION_MAJOR`. An older minor is installed as it is and migrates at its load like any file on disk (Z2-D-048).
2. The header kind is MANUAL, QUICK, AUTOSAVE or CHECKPOINT, else `SAVE_ERROR_INVALID_DESC`. The METADATA pair `zenith.profile` equals the active profile's key, else `SAVE_ERROR_IDENTITY_MISMATCH` (an absent pair too), so another account's save is never installed.
3. The target: an engine kind is imported only into the slot its METADATA names (`SAVE_ERROR_INVALID_DESC` otherwise); a MANUAL file into any `T` that passes `Zenith_ParseSaveSlotName(T, MANUAL)`. When `T` differs from the METADATA slot name, the image is rebuilt in the assembly arena: METADATA re-encoded with `T` (codec NONE, both CRCs recomputed), every other section's stored bytes copied unchanged at offsets shifted by the length difference, then the section table CRC, the file bytes and the header CRC recomputed; the rebuilt image passes `Open` again.
4. `Zenith_RunJournaledSaveWrite(storage, {D, T, keepBackup = true, overwriteNewer = false}, N, the header's own sequence, …)`: the local file becomes the backup, and a newer-version local file is kept (`SAVE_ERROR_SLOT_NEWER_VERSION`). The bytes keep their sequence, because they must stay the bytes the cloud holds; the profile sequence becomes the larger of its value and that sequence, so every save this system writes afterwards still takes a sequence above every file of the profile (§ "Image assembly and compression").
5. The slot table is updated, a slot notice is queued (§ "Slot listener"), the transfer arena is released and the operation completes SUCCEEDED; cancellation follows the write's rule (before journal step 4 only).

A profile switch discards untaken export bytes (their `TakeExportedSlot` returns `SAVE_ERROR_CANCELLED`), so nothing of the previous profile stays readable (§ "Profile switching" step 2).

## Slot listener

<!-- elaboration -->
Stage 4 (A3 rows ON1 (c), ON16 (b)). `SetSlotListener(L)` stores the one `Zenith_SaveSlotListener` and an atomic "notices on" flag (release). While the flag is on, the writer pushes one **slot notice** into the bounded notice queue (`Zenith_BoundedQueue<Zenith_SaveSlotNotice>`, capacity `2 × save.maxQueuedRequests + 2`, full policy: the writer waits, never drops) for:

- each write that SUCCEEDED into `saves/<active profile>/`: a script, C++, out-of-tick, exit-autosave or imported save, carrying the slot's `Zenith_SaveSlotInfo` with its display name copied (at most 128 bytes); never the resume write or a replay checkpoint;
- each delete that SUCCEEDED and found at least one of the slot's `.zsave`, `.zsave.bak` and `.zsave.corrupt`, carrying the slot and the kind the slot table held. Nothing else is a deletion: no `.corrupt` quarantine at a load, no journal recovery, no file that vanished without SAVE, no cancelled delete.

Backpressure: the writer pops an ordinary job (writer queue or operation queue) only while at least `save.maxQueuedRequests + 1` queue entries are free, so a job's notices (at most one per request it holds) always fit and the priority slot's single notice always finds the reserved entry; otherwise it waits on the wake semaphore. `PumpMainThread` pops every notice in completion order, calls `OnSlotWritten` or `OnSlotDeleted` under the system guard, and releases the semaphore when it popped any. With no listener set a popped notice is discarded. `SetSlotListener(nullptr)` and `BeginShutdown` turn the flag off and discard the queue, so the drain at shutdown never waits on notices. `SetActiveProfile` is `SAVE_ERROR_PROFILE_BUSY` while a notice awaits delivery, so every notice is delivered under the profile it belongs to. In replay mode the writer writes and deletes nothing, so no notice exists.

## Migration pipeline

Three independent chains, each one step per version ([Z2-N-CONTRACT-524]):

| What | Versioned by | Migrated by | When |
|---|---|---|---|
| the container and SAVE's own section layouts | `(major, minor)` in the header | `Zenith_MigrateSaveImage`: for m from the file's minor to the current minor − 1, `steps[m](tree)`, each a pure function ([Z2-N-CONTRACT-538]) setting the tree's version to (major, m + 1) | load step 4 |
| each component and event type | the field set's `m_uVersion` | `Zenith_MigrateFieldSet` (ECS; schema ops and C++ steps; script ops through the save-loading VM factory in `Zenith_MigrationDiagnostics`) | restore, per record |
| each participant section | the entry's section version | the participant inside `RestoreSection` (behaviour states: SCRIPT's `Migrate` chains inside `RestoreSavedStates`) | restore |

`Zenith_SaveMigrationRegistry::Freeze` fails at `Create` when a step is missing for any minor below the current minor ([Z2-N-CONTRACT-539]). A failed step leaves the file untouched and fails the load with `SAVE_ERROR_MIGRATION`. A migrated save is written back only by the next ordinary save of the slot (always at the current version); a load never rewrites a file. Format version 1.0 has no steps; the first minor bump adds step 0 and the fixture `v1_1`.

## Restore pipeline

<!-- elaboration -->
`Zenith_WorldRestoreJob::Step()` on the main thread, W freshly created, empty, isolated and already bound (its desc from `MakeRestoreDesc(W, bUnderPublicationSource)`):

| State | Work per Step | Next |
|---|---|---|
| DECODING | parse the WORLD section (counts checked against the other sections); build the entity index (offset of each record, O(E)); check simulation IDs strictly ascending and below the next simulation ID, persistent IDs unique; `W.RestoreSimulationState(t, nextSimulationID)` before any playback (it resets change tracking, `Zenith2/Docs/Design/ECS/Scheduling.md` → "Out-of-tick playback") | SPAWNING |
| SPAWNING | for the next `m_uEntitiesPerBatch` records: per component record, `resolver.SetContext(simulation ID, type)`; decode; type unknown → keep in the carry; version older → `Zenith_MigrateFieldSet`; newer → `SAVE_ERROR_MIGRATION` (newer component data than the build: `MIGRATION_ERROR_NEWER_VERSION`); construct the type's default value in scratch, `Zenith_ApplyFieldSet` with the resolver deferring entity references (left null; the (entity, type) recorded when any stored reference of the record is not null; a container property whose element type holds an `ENTITY` value anywhere is deferred whole); record `SpawnWithSimulationID({components, persistent ID, parent null}, simulation ID)` and the batch's `SetContainer` records for the other containers on the ticket; `W.ApplyOutOfTick(batch)`; read `GetSpawnResults()` into the simulation-ID → handle map | SPAWNING until every batch, then LINKING |
| LINKING | parent cycle check (each chain walked with a visited stamp, O(E)); one buffer of `SetParent` for every entity with a parent; for every recorded (entity, type): re-decode its record, apply it with full resolution into scratch, and record one `SetProperty` per `ENTITY`-typed top-level property of the type (from the type table) and one `SetContainer` per deferred container; `ApplyOutOfTick` | EVENTS |
| EVENTS | one buffer of event records with their saved merge keys; `ApplyOutOfTick` appends them to the carried queue (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Out-of-tick playback" step 2) | RESOURCES |
| RESOURCES | for each record of the RESOURCES section: `Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section" (read the current value, apply the saved serialized properties with full resolution, `SetResource` out of tick); one `ApplyOutOfTick` | PARTICIPANTS |
| PARTICIPANTS | decide each ASYNC entry's `Zenith_SaveAsyncDisposition` from the entry, the participants' declared re-issue kinds and `m_bUnderPublicationSource`, building the own-request success payload and the CANCELLED and abandoned payloads in the job's arena (`Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"); `hooks.RestoreAsyncState(next, entries)`; then for each registered participant in registration order: its section (by key) or "absent", `RestoreSection(ctx, reader)` with `ctx.m_pxReissue` the job's `Zenith_SaveReissueSet`; a REQUIRED section whose participant is not registered → `SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING`; other unmatched sections → the carry; finally every left-in-flight entry of a declared kind that no participant marked is completed with the zero-length abandoned payload (`hooks.CompleteAtRestore`) and counted in `save.restore.async_failed` | PARTICIPANTS_READY |
| PARTICIPANTS_READY | for each participant in registration order from the first not yet ready: `IsRestoreReady(ctx)`; a `false` ends this Step with the state unchanged (the participant is asked again at the next Step; one that returned `true` is not asked again); every participant ready → DONE in the same Step. No tick of W runs before DONE, so the wait moves no simulation tick: a frame-driven restore waits across prologues (the frame's `Assets.Service` advances the participants' loads), a synchronous one services `ListRestoreWaits` between Steps (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Synchronous restore driver") | DONE |
| DONE | write the binding's restored state: request serial counter, autosave flag, outstanding autosave completion ID, playtime base (the saved playtime ticks) and first tick (t + 1), unknown carry; `m_uOutstanding` = the number of restored `ASYNC_SOURCE_SAVE` entries (unpublished, completed at restore, or left in flight), each of which publishes exactly once afterwards | — |
| FAILED | — | — |

- Every Step returns once `m_ulBudgetUs` has elapsed after a batch (checked with `Zenith_GetMonotonicTimeUs`); the batch size bounds one batch to about 1 ms on the reference desktop for the W-Dense component mix.
- The restored state does not depend on the batch size: batches apply in simulation-ID order and every handle mapping is by simulation ID (`Test_Restore_BatchSizeDoesNotChangeState`).
- Any error moves to FAILED with the error; the caller destroys W (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneLoader header", the same rule for isolated activation).
- Cost O(E × C) decodes and three `ApplyOutOfTick` passes plus one for resources; transient memory the decoded image plus one batch of scratch.
- After DONE, the first tick of W is t + 1 and its first query invocations see every chunk as changed (a load is "everything changed"). A restore from an EXACT save continues bit-identically to the uninterrupted run under the delivery rule of `Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries" (an ordinary restore publishes every `ASYNC_SOURCE_SAVE` entry that was in flight at the snapshot at the boundary of t + 1, so the uninterrupted run it equals is the one in which those results published the same payloads at that boundary; `Test_Restore_ContinuationMatchesUninterrupted`); a live run of the uninterrupted world matches it only when no request of another I/O- or scheduling-timed source is in flight at the snapshot or issued in the compared window, and otherwise the reference is the original run's recording, through a replay resumed from the save (`Zenith2/Docs/Design/SAVE.md` → "Determinism obligations"); a restore from a COHERENT save continues consistently.

## Resume save and restore

The process-death path of PLAT (`Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence", "Relaunch decision"):

1. `SubmitResumeWrite(primaryWorldTag, "saves/resume.zsave")` on `DID_ENTER_BACKGROUND`, the world paused: one out-of-tick snapshot of the primary world, into arena 2 (so a resume write never waits for the two ordinary capture arenas), as one job of kind RESUME pushed into the priority slot, which the writer pops before any other queued job; refused (`SAVE_ERROR_QUEUE_FULL`) only while an exit autosave holds arena 2; returns the operation handle PLAT polls. Participants with `uSAVE_PARTICIPANT_FLAG_EXCLUDE_FROM_RESUME` are left out. A game runs one live world outside a world transition, so the primary world is the whole game state; a background entry during a transition saves the old (primary) world, and the relaunch restores that world, the transition never having happened from the player's point of view.
2. The writer assembles a one-world image (header kind RESUME, no thumbnail) and runs the journaled write with `keepBackup = false` in directory `saves`.
3. PLAT's deadline cancels the handle; the journal steps check the token, PLAT's `WriteAtomic` deletes its temporary on cancellation, and a rolled-back journal leaves the previous resume file (or none) intact: never a torn file.
4. At startup step 17 `ReadResumeIdentity(path)` reads the 128-byte prefix synchronously on the main thread (the writer holds no job: nothing is queued before the first frame): header CRC, magic, version, then `{build ID, composition hash, content hash, tick}`; any failure → PLAT's `RESUME_DISCARD_REASON_UNREADABLE`.
5. On PLAT's decision to restore: `BeginLoadPath(path)`; the load additionally refuses an image whose `zenith.profile` pair differs from the save system's active profile (`SAVE_ERROR_IDENTITY_MISMATCH`, so no cross-account restore) and any version other than the current one (the resume kind is never migrated); the active profile at startup is the last-profile record's (`Zenith2/Docs/Design/SAVE/Formats.md` → "Last profile record"), so a resume written under an account profile is restored when the process relaunches before the platform reports the sign-in; then the one world is created, bound and restored to DONE synchronously by `Zenith_RuntimeSave_RestoreSynchronously(state, job, runtime.startupLoadTimeoutMs)` (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Synchronous restore driver"), which services the participants' restore waits between Steps, so a participant whose `IsRestoreReady` waits for assets (ANIM) completes before the first tick; any failure, a readiness wait past the timeout included → `RESUME_DISCARD_REASON_RESTORE_FAILED` and the startup scene loads. Under `--record` a successful resume is logged as § "Replay recording" (the startup step 17 row) states: the step-16 session closed, then the restored world's `WORLD_BEGIN` and restored checkpoint. If the platform afterwards reports a different account, § "Profile switching" applies as for any account change.

## Checkpoints

<!-- elaboration -->
A CHECKPOINT request (script `Save.Checkpoint()`, or the runtime's thermal `RequestCheckpointSave` through `SubmitOutOfTick`) writes the `checkpoint` slot; after the write succeeds, the writer builds a `Zenith_PreparedSave` over a copy of the job's decoded sections and the header of the file just written (no I/O to restore it, already the current version; the header's file bytes, own completion ID and own request serial are the written file's, § "Image assembly and compression" step 7) and hands it to the main thread through W's pending-checkpoint slot (`Zenith2/Docs/Design/SAVE.md` → "Cross-thread hand-offs"). `PumpMainThread` promotes it to W's retained checkpoint in prologue step 7 unless a lease is held, in which case it waits (a newer one displaces it). `AcquireRetainedCheckpoint(W)` returns a lease that pins W's checkpoint holder and exposes the retained checkpoint non-const, so `BeginWorldRestore` runs on it; GAME's "retry from checkpoint" restores it into a new world exactly as a load, holding the lease until its restore job is destroyed. The holder (retained and pending copies plus the lease count) is owned by the save system, not by W's binding, so two orderings are safe by construction: (i) `SetActiveProfile` while any lease of any world is alive is refused with `SAVE_ERROR_PROFILE_BUSY` (the runtime retries it each prologue, so an account sign-out arriving during a retry switches after the retry's restore job is destroyed), and it never destroys a leased checkpoint; (ii) `UnbindWorld(W)` at the swap that retires the old world frees W's binding but, when the holder's lease count is above 0, detaches the holder instead of destroying it: the last lease's destructor destroys a detached holder (main thread), and the lease never touches the freed binding. A holder with no lease is destroyed with its binding or when a newer checkpoint replaces its content. `~Zenith_SaveSystem` ZENITH_CHECKs that no lease is alive. The retained and pending copies' bytes are reported by the memory consumer. Which checkpoint is retained is not simulation state: a replay of a retry takes its world from the log's restored-world checkpoint, never from the retained copy.

## Persistent world and entity state

<!-- elaboration -->
Persistent state has no SAVE-specific store: whatever must survive a save lives in serialized components (saved by ENTITIES), in reflected world resources (saved by RESOURCES), in state outside reflection that a module's participant saves, or in behaviour state (saved by `Zenith.Script.States`).

| State | Where it lives | Saved and restored by |
|---|---|---|
| entities of resident streamed cells and of additive instances | the world | ENTITIES; the instance records through `Zenith.Scene.AdditiveSet` |
| the persistent state of entities whose cell is not resident (changed properties, destroyed authored entities as tombstones) | STREAM's `Zenith.Streaming.CellDelta` components (a container of the cell's delta encoding) on runtime holder entities, and `Zenith.Streaming.Persistent` on the entities it tracks | ENTITIES, like every component; STREAM applies the deltas at the cell's next activation |
| which cells are resident or requested | STREAM's components; STREAM rebuilds `Zenith.Streaming.State` from anchors, delta holders, SCENE's additive set and the restored-load inbox and registers no participant (STREAM's row ST14) | ENTITIES and SCENE's additive-set participant (`Zenith.Scene.AdditiveSet`) |
| world-level state that is reflected: the environment clock and weather (`Zenith.Environment.State`), the simulation input contexts, the world host's continuity base | world resources with serialized properties | RESOURCES |
| world-level state outside reflection: physics bodies and constraints, animation graph state, navigation and in-flight path requests, cinematic playback | module-owned state | each module's participant (row SV17) |
| gameplay state: quests, dialogue, inventory (game-side Luau, Z2-D-077) | components and behaviour state | ENTITIES and `Zenith.Script.States` |

What SAVE guarantees for it:

1. Every serialized property of every saved entity, containers included, round-trips bit-exactly (`Test_Snapshot_RoundTripAllPropertyTypes`), so a cell delta's bytes are restored unchanged.
2. A LOADING or QUEUED additive instance is re-requested at restore with its saved hook tag (STREAM's `Zenith_MakeStreamingHookTag` values are save-stable, row ST14), so STREAM's activation hook applies its delta and tombstones to the reload exactly as to a first load (`Zenith2/Docs/Design/SAVE/Formats.md` → "Participant section framing"); such a capture is COHERENT.
3. An instance that was ACTIVE is not reloaded: its entities come back from ENTITIES, already carrying the applied deltas, so no entity is created twice.
4. `Test_SaveCrossSystem_UnloadSaveReloadEquivalent` and `Test_SaveCrossSystem_DestroyedPersistentStaysDestroyed` prove the combination with STREAM.

## Profile switching

<!-- elaboration -->
`SetActiveProfile(P)` on the main thread:

1. Refuse with `SAVE_ERROR_REFUSED_IN_REPLAY` in replay mode, and with `SAVE_ERROR_INVALID_DESC` for a key that is neither `local` nor 16 lowercase hexadecimal digits (Stage 4). When P is the active profile (Stage 4, row ON1 (e)), return success at once: nothing is recovered, cleared, written or logged, and a pending switch ends (§ "Profile switch intake freeze"). Otherwise refuse with `SAVE_ERROR_PROFILE_BUSY` while the writer holds a job, any queue or operation is non-empty, any retained-checkpoint lease is alive (§ "Checkpoints"), or a slot notice awaits delivery (§ "Slot listener"); the runtime retries at each prologue until it succeeds; PLAT's account events are not lost because the runtime keeps the latest requested profile.
2. Clear every world's retained and pending checkpoint (no lease exists, by step 1), the slot table and any untaken export bytes (§ "Slot export and import"); nothing of the previous profile stays readable.
3. `CreateDirectory(saves/P)`; `Zenith_RecoverSaveJournals(saves/P)`; rebuild the slot table and `m_ulProfileSequence` (the maximum header sequence among the profile's `.zsave` and `.bak` files) with `ReadPrefix(128)` per file.
4. `Zenith_WriteLastSaveProfile(storage, P)` (a failure is logged and does not stop the switch: the next start then begins in the previous profile and switches again on sign-in).
5. Switch, end a pending switch; log `save: profile switched to <kind>`, `<kind>` being `local` for the local profile and `account` for any other (row ON16 (c)).

The runtime's account consumer calls it on sign-in and user switch (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Account changes"); sign-out switches to `local`. From Stage 4 that consumer is ONLINE's session, which brackets the switch with § "Profile switch intake freeze".

**Profile keys in diagnostics** (Stage 4, row ON16 (c)). A profile key is an unkeyed hash prefix of an account ID, which a small account space inverts by enumeration, so no SAVE log line, counter, telemetry record, crash annotation or `save.dump` output carries one: each names the profile `local` or `account`. The key stays only where it must: the directory name, the METADATA pair `zenith.profile` and the last-profile record.

### Profile switch intake freeze

<!-- elaboration -->
Stage 4 (A3 rows ON1 (d), ON16 (a)). `BindWorld` records the active profile in the binding (`GetBindingProfile`). The **profile rule** refuses a request of world W with `SAVE_ERROR_PROFILE_MISMATCH` when W's binding profile differs from the active profile, or when a switch is pending and W's binding profile differs from its target:

- at intake step 7 (§ "Request intake") for a WRITE, LIST, DELETE or LOAD, completed at once like any intake refusal (a script LOAD's linked ID through `CompleteLinkedLoadRefused`); operations 0x80 and 0x81 still apply, because they change only W's own state;
- at phase-11 step 1 for a due autosave, which is allocated as always and completed at once, so completion IDs and serials stay a function of simulation state (§ "Autosave scheduling"): a replay supplies the recorded refusal;
- synchronously for `SubmitOutOfTick`, `SubmitResumeWrite` and `SubmitExitAutosave` of W.

`BeginProfileSwitch(T)` (MAIN_THREAD, no frame running) sets the pending target T, replacing an earlier one, and logs `save: profile switch to <kind> pending`; work already queued or running drains, and retained-checkpoint leases end by their own rule. A successful `SetActiveProfile` ends the freeze, and so does `AbandonProfileSwitch()` (a no-op with none pending). The active profile and the pending target change only on the main thread with no frame running, so every intake point of a frame reads the same values. Because the freeze refuses every new request of the previous profile's worlds and every new export or import, SAVE's queues only shrink (unless C++ code issues loads, listings or deletes meanwhile, which ONLINE does not) and a busy `SetActiveProfile` retried each prologue succeeds after a bounded number of frames, however often a world autosaves.

## Replay recording

With `--record <file.zrpl>` (non-Shipping builds), the runtime creates a host-file sink, a `Zenith_ReplayLogWriter` and the Stage 2 core recorder `Zenith_RuntimeReplayCoreRecorder`, registered as each world's first tick-boundary handler; from Stage 3, `Zenith_RuntimeReplayPublicationObserver` is registered as SCRIPT's publication observer and adds SCRIPT's checkpoint contribution to the tick hash.

| Moment | Action | Stage |
|---|---|---|
| startup world created (step 16) | `BeginWorld(tag, seed, scene, sceneHash)` → ordinal | 2 |
| startup step 17, a resume restored under `--record` (after `Zenith_RuntimeSave_RestoreSynchronously` reached DONE) | the step-16 session is closed with `SwapWorld(step-16 tag, 0, restored tag, the save's base-scene content hash)` (no tick of the step-16 world ran, so its session holds no `TICK` record), then `BeginWorld` of the restored world (the save's seed and base scene) and `Zenith_RuntimeReplay_LogRestoredWorld(restored tag, t)`, so the log's next session is a restored session (`m_bRestored`) that replay playback step 3 restores; a resume refused or failed leaves the step-16 session open and records nothing | 3 |
| a transition's new world, at the swap (prologue step 7) | `SwapWorld(old, lastTick, new, hash)` (the old world's pending hash is computed out of tick first), then `BeginWorld`; for a world restored from a save, then `Zenith_RuntimeReplay_LogRestoredWorld`: a fresh out-of-tick capture into a one-world `SAVE_KIND_REPLAY_CHECKPOINT` image and `AddCheckpoint(tag, t, image)` (`Zenith2/Docs/Design/SAVE/Formats.md` → "Restored-world convention") | 2 (swap), 4 (restored worlds) |
| tick boundary of tick t, before step 1 (handler step `PUBLISH_ASYNC`, registered first) | for t > first tick: `HashTickState(t − 1)`, plus SCRIPT's checkpoint contribution from Stage 3 → `SetTickHash(tag, t − 1, hash)` | 2 |
| tick boundary step 1 (publication observer) | `SetTickPublications(tag, t, headers, payloads)` | 3 |
| phase 1 (PLAT's input system) | `SetTickInputCommands(tag, t, commands)` | 2 |
| a fatal non-replayable event (`OnWorldHalted`, the watchdog) | `SetTickFlags(tag, t, REPLAY_TICK_FLAG_NON_REPLAYABLE)` | 3 |
| phase 11, t a checkpoint multiple | § "Phase-11 snapshot" step 9: the capture assembled at once on the node into a one-world image with codec NONE and handed over with `AddCheckpoint(tag, t, image)`; it allocates no completion ID and changes no world state | 4 |
| frame-loop step 3 | `CommitFrame()`: completed records in the order of Formats.md "Records SAVE writes" to the sink; the host-file sink's thread appends and flushes every `replay.flushIntervalMs` | 2 |
| shutdown step 4 | every world's last tick hashed out of tick, `Close()`: commit, flush, join `Zenith.ReplayWrite` | 2 |

A sink error, a full commit queue or reaching `replay.maxLogBytes` stops recording: the log keeps its committed, flushed prefix, which the reader opens as a truncated log; `save.replay.write_errors` counts and the run continues (recording is diagnostic). With `--replay` and `--record` together, the recorder records the replayed run. Recording changes nothing a tick computes: checkpoints allocate nothing, and SAVE's completion IDs never depend on whether a recorder exists. Because two live runs agree only when their SAVE results publish at the same ticks (§ "Autosave scheduling"), `Test_SaveRuntime_RecordingDoesNotChangeHashes` holds publication ticks equal with the gated in-memory storage and also replays the recorded run.

## Replay playback

With `--replay <file.zrpl>`:

1. Startup: `OpenHostFile(grant, replay.maxLogBytes)`; `CheckIdentity(running)`; a mismatch exits with code 5 naming the field ([Z2-N-ARCH-784]); the header's simulation-affecting list is applied with `ApplySimulationAffecting` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Simulation-affecting list").
2. The save system runs in replay mode (§ "Replay mode").
3. Each world session in log order: a restored session (`m_bRestored`) that no replayed transition begins — the first session of a log whose recorded run started from a resume (§ "Replay recording", the startup step 17 row) — creates its world from the session's restored checkpoint (`GetRestoredCheckpoint`, `PrepareFromImage`, `MakeRestoreDesc(tag, true)`, restored to DONE by `Zenith_RuntimeSave_RestoreSynchronously` with `runtime.startupLoadTimeoutMs`) instead of loading the scene (a recording always writes that checkpoint or stops before the session, § "Out-of-tick snapshot"). A restored session begun by a replayed save-load transition is not restored here: GAME's world host restores it frame by frame through the bridge's replay form, with `MakeRestoreDesc(tag, true)` (§ "Script loads and the world host").
4. The Stage 2 core player `Zenith_RuntimeReplayCorePlayer` supplies `GetTickInputCommands` (phase 1 reads them instead of devices) and `GetRemainingTicks` (the clock caps T so no tick beyond the log and none at or after a `NON_REPLAYABLE` tick runs, [Z2-N-ARCH-790]); from Stage 3, `Zenith_RuntimeReplayPublicationSource` supplies `GetRecorded` (publications).
5. At each tick boundary it hashes the previous tick and calls `verifier.Check`; the first mismatch calls the desc's callback (the runtime records a test assertion failure, exit code 5) and is logged `replay: tick <t> of world session <o> diverged: recorded <h1>, replayed <h2>`.
6. When `IsFinished()`, the runtime requests shutdown; exit code 0 when no mismatch was recorded.

## Replay mode

<!-- elaboration -->
With `Zenith_SaveSystemDesc::m_bReplayMode` (`--replay`), every SAVE operation that would touch the replaying machine's user files is suppressed, and every result comes from the log:

| Operation | Live | Replay mode |
|---|---|---|
| WRITE (any kind, script or C++, engine autosave) | captured, assembled, journaled, completed | captured and assembled (the frame cost stays representative), no storage call, no completion: the recorded publication is the result |
| DELETE | the slot's files deleted on the writer thread | no storage call, no completion |
| LIST | prefix reads of the profile directory | no storage call, no completion: the recorded LIST payload is the result |
| LOAD (script) | read, verified, migrated, handed to the world host | no storage call, no completion; the world comes from the log's restored session (§ "Script loads and the world host") |
| `BeginLoad`, `BeginLoadPath`, `BeginListSlots`, `BeginDelete` (C++) | operations | refused, `SAVE_ERROR_REFUSED_IN_REPLAY` (a C++ caller that needs a recorded result is a script request) |
| `SubmitOutOfTick` | out-of-tick write | captured, no storage call; the operation completes `SAVE_ERROR_SUPPRESSED_IN_REPLAY` (it publishes nothing in either mode) |
| `SubmitResumeWrite`, `SubmitExitAutosave`, `SetActiveProfile` | storage writes | refused, `SAVE_ERROR_REFUSED_IN_REPLAY` |
| journal recovery at `Create` | runs | skipped (no storage call) |
| `Zenith_ReadLastSaveProfile` at startup step 11 | reads `saves/last_profile.zslp` | not called: the initial profile is `local` (`Zenith_SaveSystemDesc::m_xInitialProfile`) |
| the slot table of the initial profile (`ReadPrefix(128)` per file) | built at `Create` | not built (the table stays empty; nothing reads it in replay mode) |
| `ReadResumeIdentity` | reads the resume prefix | refused, `SAVE_ERROR_REFUSED_IN_REPLAY`; the runtime skips the resume decision of startup step 17 entirely under `--replay` (cross-area row SV24), so PLAT never reaches its discard path and nothing deletes or reads the user's `saves/resume.zsave` |

So a replay reads and writes no user file at all, from startup to shutdown (`Test_SaveRuntime_ReplayTouchesNoStorage` starts the runtime under `--replay` and runs a recorded delete, listing, load and write over a storage whose every member fails the test, including those startup would call). The writer never calls `sink.Complete` in replay mode; intake refusals and phase-11 completions still do, and SCRIPT discards each such live completion of an ID whose recorded publication it supplies (`Zenith2/Docs/Design/SCRIPT/Runtime.md` → "Asynchronous result table", publication row), so the result table publishes exactly the recorded results.

## Replay seeking

`Zenith_RuntimeReplay_SeekTo(state, ordinal, target)` (development builds; the editor's replay viewer and tests):

1. Find `c = reader.FindLatestExactCheckpointAtOrBefore(ordinal, target)`: a checkpoint whose image lacks `COHERENT_ONLY`, at or before the target.
2. Strategy CHECKPOINT when c exists: restore c into a new isolated world (`PrepareFromImage`, `MakeRestoreDesc(tag, true)`, so recorded results are never published twice, `Zenith2/Docs/Design/SAVE/Formats.md` → "Async section"), then run ticks c.tick + 1 … target headless with the recorded inputs and publications as fast as possible.
3. Strategy FULL otherwise (no checkpoint, or only COHERENT ones): recreate the session's world from its `WORLD_BEGIN` (or, for a restored session, its restored checkpoint, which is a fresh capture of the restored world and is restored the same way) and run ticks from its first tick to target. FULL is always correct; it is slower.
4. Verify every replayed tick's hash as in § "Replay playback"; report the strategy, the ticks run, the time, `m_bHashesCompared = true` and `m_bVerified`.

Cost: O(target − c.tick) ticks for CHECKPOINT; with `replay.checkpointIntervalTicks = 600` a seek runs at most 599 ticks after the restore. Asynchronous work in flight at the checkpoint is re-issued by its participants with the recorded completion IDs, and recomputed publications match because the re-issued request computes the same result (`Test_ReplaySeek_InFlightAsyncReissued`). A checkpoint taken while some behaviour has a `Run` coroutine suspended mid-body, while some instance is quarantined or of a module disabled or disabling, or while an additive instance is loading, is COHERENT (`Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries"), and a seek whose latest checkpoints are all COHERENT falls back to FULL; behaviour state alone, a pending recovery included, does not make a checkpoint COHERENT (orchestrator rulings Q82 (b) and SC25, fenced in `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations"). `Test_ReplaySeek_CoherentCheckpointsFallBackToFull` proves the fallback. A seek's restore runs through `Zenith_RuntimeSave_RestoreSynchronously`, so a participant that waits for assets is ready before the first replayed tick.

## Resuming a replay from a save

`Zenith_RuntimeReplay_ResumeFromSave(state, save, target)` ([Z2-N-ARCH-789]; development builds and tests) continues a recorded run from a save written during it:

1. The save's header must carry `HAS_REPLAY_POSITION`; its (ordinal, tick) must name a session of the open log whose ticks include tick; otherwise `REPLAY_ERROR_NOT_FOUND`.
2. Restore the save into a new isolated world (`MakeRestoreDesc(tag, true)`, driven to DONE by `Zenith_RuntimeSave_RestoreSynchronously`), and continue the session from tick + 1 to target with the recorded inputs and publications; every SAVE result of the save, its own included, then publishes from the log at its recorded tick, so a resumed replay needs no delivery rule.
3. An EXACT save: every tick's hash is compared as in § "Replay playback" (`m_bHashesCompared = true`).
4. A COHERENT save: no hash is compared (`SetHashComparison(tag, false)`), the run is reported with `m_bHashesCompared = false`, `save.replay.unverified_ticks` counts the ticks, and the log line `save.replay: resumed from '<slot>' at (<ordinal>, <tick>) without hash comparison: the save is COHERENT_ONLY` says why. The resumed run is still the recorded run's inputs over a consistent world; it is simply not claimed bit-identical.

## Replay verification

The tick hash of tick t is `HashTickState` of the world after tick t's end-of-tick playback (`Zenith2/Docs/Design/ECS/Scheduling.md` → "World state hash") with SCRIPT's committed-checkpoint contribution appended, digested by `Zenith_Hash64Stream`. Recording and verification compute it at the same point (the next tick boundary, before publication), so both sides hash identical states. A hash never covers presentation state (rendering, audio, UI, thermal level, memory pressure) or SAVE's own I/O timing, which is why those can never cause a mismatch (entry document → "Reproducibility boundaries").

## Shutdown sequence

| Shutdown step (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Shutdown ownership order") | SAVE action |
|---|---|
| before 4 | on RHI's device-loss clean exit only, after the last `RunFrame` returned (every world out of tick): `Zenith_RuntimeSave_RequestAutosaveForExit` → `SubmitExitAutosave(primary)`: it first cancels every queued job (CANCELLED; a queued script LOAD's linked ID through `CompleteLinkedLoadRefused`) and requests cancellation of the running ordinary write or operation, which stops at its next cancellation point and rolls back unless it reached journal step 4 (after which only a journal rewrite, one rename and one delete remain); then it captures the primary world out of tick into arena 2 and pushes the job into the priority slot, which the writer pops as soon as the preempted job returns; no completion ID. The time `T0` of this call starts the exit budget |
| 4 | `Zenith_RuntimeSave_BeginShutdown`: `BeginShutdown()` refuses new requests (`SAVE_ERROR_SHUTTING_DOWN`), cancels queued jobs not yet started (CANCELLED) except the exit autosave, and (Stage 4) clears the slot listener and discards its undelivered notices (§ "Slot listener"). **Device-loss exit:** the ordinary drain is skipped (everything ordinary was cancelled at `T0`); the main thread polls the exit autosave's operation every 1 ms until `T0` + `uSAVE_EXIT_AUTOSAVE_BUDGET_MS` (6,000 ms), then requests its cancellation (rollback before journal step 4: the previous autosave slot stays intact) and polls `IsDrained()` for at most `uSAVE_EXIT_ROLLBACK_WAIT_MS` (1,000 ms), then continues whatever remains (the writer thread is joined at step 7b; a write that has not yet reached its next cancellation point finishes at most its current uncancellable unit there: the longest uncancellable unit of § "Journaled write sequence"). **Ordinary exit:** the running write finishes; the main thread polls `IsDrained()` for at most `save.shutdownDrainMs` (5,000 ms), then requests cancellation of whatever still runs (rollback unless it reached journal step 4). In both, the replay writer then hashes the last ticks and `Close()`s |
| 6 | worlds are destroyed: `UnbindWorld` for each, after which completions for them are dropped by the sink |
| 7b (after the job system, before step 8) | `Zenith_RuntimeSave_Destroy`: destroy `Zenith_SaveSystem` (joins `Zenith.SaveWriter`) and the replay writer (joined `Zenith.ReplayWrite` at Close) |
| 8 | `Zenith_AsyncIO` is destroyed after SAVE, so every SAVE I/O completed before |

<!-- elaboration -->
The device-loss budget split: [Z2-N-CONTRACT-484] and `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine" (state `EXITING`, step 6) give the whole clean exit — the autosave **and** shutdown — 10 s before the `FATAL_CODE_DEVICE_LOST` fallback. SAVE takes at most 6 s for the exit autosave (preemption of the running write included: the autosave never queues behind an ordinary write, which stops within its current uncancellable unit — one storage call, one section's compression, or one section's decode in step 0b, § "Journaled write sequence" — because assembly, compression and step 0b's verification are cancellable between sections) plus 1 s for the rollback of a cancelled autosave, whose own uncancellable unit is the same, 7 s in all from `T0`, leaving 3 s for the rest of the shutdown order (job system, worlds, replay close, SAVE's thread join at 7b, `Zenith_AsyncIO`), none of whose steps waits on storage. A 6 s autosave covers a W-OpenWorld save on the S24 with margin (the pending row "Save write duration per save" measures it; a breach of that row, BR-ACCEPT, is what revisits the split). At 6 s the autosave is abandoned with rollback, so the exit still ends with exit code 6 and the previous autosave intact, never with the fatal. The ordinary path's `save.shutdownDrainMs` applies only when no device was lost.

## State machines

### Write request state machine

| State \ Event | phase-11 capture | no capture arena free | capture error | assembled | journal success | journal error | cancel requested | superseded | shutdown begins |
|---|---|---|---|---|---|---|---|---|---|
| QUEUED (pending) | → CAPTURED | stays QUEUED (deferred) | → FAILED, publish | — | — | — | → CANCELLED, publish | → SUPERSEDED, published at the next capture | → CANCELLED (SHUTTING_DOWN), publish |
| CAPTURED | — | — | — | → WRITING | — | — | → CANCELLED, publish | — (supersede marks pending requests only) | → CANCELLED if not yet popped, except the exit autosave |
| WRITING | — | — | — | — | → SUCCEEDED, publish | → FAILED, publish | → CANCELLED (before journal step 4) or ignored (from step 4) | — | finishes within the drain window (`save.shutdownDrainMs`; for the exit autosave `uSAVE_EXIT_AUTOSAVE_BUDGET_MS` from its submission, an ordinary write being cancelled at that submission), else cancelled as above |
| SUCCEEDED, FAILED, CANCELLED, SUPERSEDED | terminal | | | | | | | | |

"Publish" is the sink's `Complete` with the 64-byte payload for a request with a completion ID (the world sees the result at its next tick boundary, in completion-ID order, [Z2-N-CONTRACT-093]), and the operation handle's completion for an out-of-tick, resume or exit request. In replay mode no transition publishes (§ "Replay mode").

### Journal state machine

| State \ Event | step completes | crash or power loss | error before step 4 | recovery at start |
|---|---|---|---|---|
| none | write PREPARED → PREPARED | nothing on disk changed | — | — |
| PREPARED | backup written → BACKED_UP (or straight to WRITTEN without a backup) | journal stays PREPARED | delete tmp and journal → none | roll back → none |
| BACKED_UP | temporary written → WRITTEN | stays BACKED_UP | as above | roll back → none |
| WRITTEN | journal COMMITTING → COMMITTING | stays WRITTEN | as above | tmp valid → roll forward; else roll back → none |
| COMMITTING | renamed, journal deleted → none | stays COMMITTING (RENAMED flag may be set) | — (step 4 errors leave it for recovery) | roll forward, roll back, restore the backup, or CONFLICT → none (or `.conflict` kept) |

### Load operation state machine

| State \ Event | read done | read failed | verify ok | verify failed, backup present | verify failed, no backup | migrate ok | migrate failed | cancel | taken |
|---|---|---|---|---|---|---|---|---|---|
| PENDING | — | — | — | — | — | — | — | → CANCELLED | — |
| READING | → VERIFYING | → READING_BACKUP when a backup exists, else FAILED | — | — | — | — | — | → CANCELLED | — |
| VERIFYING | — | — | → MIGRATING | → READING_BACKUP | → FAILED | — | — | → CANCELLED | — |
| READING_BACKUP | → VERIFYING_BACKUP | → FAILED | — | — | — | — | — | → CANCELLED | — |
| VERIFYING_BACKUP | — | — | → MIGRATING (from backup; rename the save `.corrupt`) | — | → FAILED | — | — | → CANCELLED | — |
| MIGRATING | — | — | — | — | — | → READY | → FAILED | → CANCELLED | — |
| READY (SUCCEEDED) | — | — | — | — | — | — | — | ignored | → TAKEN |

### Restore job state machine

The states of `Zenith_WorldRestoreState` (DECODING, SPAWNING, LINKING, EVENTS, RESOURCES, PARTICIPANTS, PARTICIPANTS_READY, DONE, FAILED), in the order of § "Restore pipeline"; every state moves to FAILED on its step's error, and the caller destroys the world. PARTICIPANTS_READY stays while any participant's `IsRestoreReady` returns false and has no error of its own; a bound on the wait is the caller's: the synchronous driver returns `SAVE_ERROR_RESTORE` after its timeout, while GAME's world host has no transition timeout (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Transition record state machine": `ACTIVATING` leaves only on done, failure or a cancel request on the transition's ticket), so a frame-driven restore waits until the transition is cancelled (`WORLD_TRANSITION_FAILURE_CANCELLED`, which destroys the isolated world) or, once GAME adopts cross-area row SV25 (assumption fenced in `Zenith2/Docs/Design/SAVE.md` → "Assumed interfaces from other areas"), until GAME fails it `RESTORE` after `runtime.startupLoadTimeoutMs`. DONE and FAILED are terminal; `Step` on them returns the state unchanged.

A participant whose capture was EXACT may find at restore that its state restores only COHERENT; it calls `Zenith_SaveRestoreContext::ReportCoherentRestore(key)` from `RestoreSection` or `IsRestoreReady` (PHYS: an EXACT blob refused by `RestoreState`, or a FAILED shape asset; AI: navigation content changed since the capture). The job keeps the first reporting key and from then on `GetRestoredFidelity()` is `SAVE_FIDELITY_COHERENT`; the resume path ("Resuming a replay from a save") then disables hash comparison exactly as for a `COHERENT_ONLY` image and names the reporting participant in its log line, and the seek path ("Replay seeking") discards that checkpoint restore and continues with strategy FULL. A report never fails the restore.

### Replay writer state machine

| State \ Event | Create | record calls | CommitFrame | sink error | Close |
|---|---|---|---|---|---|
| OPEN (header written) | — | → RECORDING | stays | → FAILED | → CLOSED |
| RECORDING | — | stays | commits completed records | → FAILED | commits, flushes, joins → CLOSED |
| FAILED | — | ignored (counted) | no-op | — | joins → CLOSED |
| CLOSED | — | ZENITH_ASSERT | ZENITH_ASSERT | — | no-op |
