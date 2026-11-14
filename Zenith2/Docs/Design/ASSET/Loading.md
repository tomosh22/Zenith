# ASSET design: runtime loading

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It specifies what `Zenith_AssetLoader` does over time: the asset state machine, the service that advances loads, the pipeline stages from catalogue lookup to activation, decompression back-pressure, cancellation at every stage, publication to simulation, residency budgets and eviction, unload and reload, missing and corrupt assets, device-loss reload, headless loading and shutdown. The declarations are `Zenith2/Docs/Design/ASSET/RuntimeAPI.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| states, reload and recovery pipelines, GPU residency, request handling | "Load request lifecycle" |
| where the work runs | "The service" |
| the pipeline | "Pipeline stages" |
| the decode queue and I/O throttling | "Decompression back-pressure" |
| cancelling | "Cancellation" |
| determinism boundary | "Publication to simulation" |
| memory | "Residency budgets and eviction" |
| unloading and development reload | "Unload and reload" |
| failures | "Missing and corrupt assets" |
| GPU recovery, headless, shutdown | "Device-loss reload", "Headless loading", "Shutdown sequence" |

## Load request lifecycle

### Asset state machine

The slot state (`Zenith_AssetState`) changes only on these events. The tables below give, for every state, every event: the next state and the action, or why the event cannot occur in that state (the service's consistency check `ZENITH_CHECK`s such a combination, which `Test_AssetStateMachine_EveryTransition` and the stress tests prove unreachable). A load pipeline is QUEUED while it looks up its entry, waits for its hard dependencies, waits for its kind's loader and waits for admission (the sub-stages of "Load record stages"); it holds no load record until admission. A reload that has swapped keeps a RETIRING record for its old object ("Reload and recovery pipelines"); a slot is never freed while one remains, so a free that E8 or E10 below would perform waits for that record's end (RA).

| Event | Meaning |
|---|---|
| E1 acquire | `Acquire`, `AcquireAny`, `AcquireByLogicalPath`, `AcquireForDeserialization` or a handle copy adds a reference (a revive when the count was 0) |
| E2 load request | `RequestLoad`, an internal dependency request or a pinned-fallback request for the asset |
| E3 admission | the in-flight budget admits a pipeline in sub-stage ADMISSION ("In-flight load budget") and gives it a load record |
| E4 activate | the kind loader's `Activate` for the slot's load pipeline |
| E5 load failure | any failure of the load pipeline (catalogue, dependency, missing loader, I/O, corruption, version, platform, loader, allocation) |
| E6 last request cancelled | the pipeline's last live request is cancelled or its operation handle destroyed |
| E7 cancellation complete | every read of the pipeline reported, every decode slice ended and, when the loader was involved, `AcknowledgeCancel` arrived |
| E8 last reference dropped | the service processes a retire entry and re-reads a zero count |
| E9 eviction | the keep-alive cache evicts the asset (budget, WARNING flush, `BeginShutdown` flush) |
| E10 unload acknowledged | the kind loader's `AcknowledgeUnload` of the slot's current object (a retiring old object's acknowledgement is the pipeline event RA of "Reload and recovery pipelines", never E10) |
| E11 reload | `RequestReload` (development builds) |
| E12 device recovery | `ReloadGpuResidencySet`; ignored for a slot of a kind that is not GPU-resident |
| E13 entry removed | a published catalogue swap no longer lists the slot's GUID |
| E14 entry changed | a published swap lists it with a different output hash |
| E15 entry kept | a published swap lists it with the same output hash |
| E16 shutdown | `BeginShutdown` |
| E17 loader unregistered | `UnregisterKindLoader` of the slot's kind |
| E18 loader registered | `RegisterKindLoader` of the slot's kind succeeds (step 6 of "Zenith_AssetRegistry" in RuntimeAPI.md) |
| E19 registration closed | `CloseRegistration` |

UNLOADED:

| Event | Next | Action |
|---|---|---|
| E1 | UNLOADED | reference + 1; a revive clears the retire-queued flag |
| E2 | QUEUED | a queued load pipeline in sub-stage CATALOGUE (the slot's side record, RuntimeAPI.md "Slot side records"); the request joins it at its priority |
| E3, E4, E5, E7 | cannot occur | no pipeline exists |
| E6 | cannot occur | no live request exists |
| E8 | freed | index entry removed, generation + 1, slot to the free list's tail; while a RETIRING record remains, the free waits for its RA |
| E9 | cannot occur | not cached |
| E10 | cannot occur | no unload in progress |
| E11 | UNLOADED | the reload operation completes `SUCCEEDED` at the next slice: there is no object to replace, and the next load reads the current snapshot |
| E12 | UNLOADED | nothing |
| E13 | UNLOADED | the entry index is marked absent; a following E2 fails `NOT_FOUND` at CATALOGUE |
| E14, E15 | UNLOADED | the entry index is re-mapped to the new snapshot |
| E16 | UNLOADED | nothing; subsequent requests are refused `SHUTTING_DOWN` |
| E17, E18, E19 | UNLOADED | nothing |

QUEUED (sub-stages CATALOGUE, DEPENDENCIES, WAIT_LOADER, ADMISSION):

| Event | Next | Action |
|---|---|---|
| E1 | QUEUED | reference + 1 |
| E2 | QUEUED | the request joins; the pipeline's priority is recomputed and passed to its dependency requests |
| E3 | LOADING | only from ADMISSION: a load record from the pool, stage HEADER_READ; the record takes a reference on the current snapshot and builds its expectation from the slot's entry there |
| E4, E7 | cannot occur | no loader call and no I/O has been made |
| E5 | FAILED | a failure found in CATALOGUE (`NOT_FOUND`, `NO_LOADER` after `CloseRegistration`), DEPENDENCIES (`DEPENDENCY_FAILED`, `OUT_OF_MEMORY` for the dependency block) or at E19: record the error; the dependency requests are cancelled and the dependency block released; every live request completes `FAILED`; a reload queued by E11 completes `FAILED` with the same error |
| E6 | UNLOADED | at the next slice: the dependency requests are cancelled, the dependency block released, the side record cleared (nothing was read) and every request completes `CANCELLED` |
| E8 | cannot occur | each request holds a reference |
| E9, E10 | cannot occur | not cached, not unloading |
| E11 | QUEUED | the reload operation joins the load as one more request: the load has read nothing, and it reads the snapshot current at admission |
| E12 | QUEUED | nothing: no GPU state exists yet |
| E13 | QUEUED, then FAILED | the pipeline returns to CATALOGUE, which fails `NOT_FOUND` (E5) at the next slice |
| E14 | QUEUED | the pipeline returns to CATALOGUE against the new entry: its new dependency requests are issued first, then the old ones cancelled and the old block released, so a dependency both entries share never unloads |
| E15 | QUEUED | re-mapped; nothing else |
| E16 | QUEUED, then UNLOADED | every live request is cancelled (E6) |
| E17 | cannot occur | the registration outlives every request: shutdown step 4 cancels them before any loader unregisters at step 6 |
| E18 | QUEUED | in WAIT_LOADER for this kind: → ADMISSION; in CATALOGUE or DEPENDENCIES: the "no loader" flag clears, so the pipeline passes WAIT_LOADER without waiting; other kinds: nothing |
| E19 | QUEUED, then FAILED | at the slice that applies E19 ("Zenith_AssetRegistry" in RuntimeAPI.md): in WAIT_LOADER, or with the "no loader" flag set, and its kind still absent from the served set that slice read: fails `NO_LOADER` (E5); a kind served by then passes as at E18; otherwise nothing |

LOADING (the load record's stages, "Load record stages", CANCELLING included):

| Event | Next | Action |
|---|---|---|
| E1 | LOADING | reference + 1 |
| E2 | LOADING | the request joins; priority recomputed; in stage CANCELLING the request waits and starts a new pipeline (QUEUED) at E7 |
| E3 | cannot occur | already admitted |
| E4 | ACTIVE | publish the object (release store); writer W4 installs the attempt in the residency word: PENDING for a GPU-resident kind that is not headless (RESIDENT or FAILED when the attempt's report came first), else NONE ("GPU residency sub-state"); the pipeline's dependency block becomes the object's; every live request completes `SUCCEEDED`; the record returns to the pool; a reload queued by E11 starts now |
| E5 | FAILED | record the error; the attempt is closed (W3); every live request completes `FAILED`; pipeline resources and the dependency block released; a reload queued by E11 completes `FAILED` with the same error |
| E6 | LOADING (CANCELLING) | the stage's cancellation of "Cancellation points by stage" |
| E7 | UNLOADED, or QUEUED when a request arrived during CANCELLING | every cancelled request completes `CANCELLED`; the attempt is closed (W3) and the record returns to the pool; a request that arrived during CANCELLING starts a new pipeline in sub-stage CATALOGUE |
| E8 | cannot occur | each request holds a reference |
| E9, E10 | cannot occur | not cached, not unloading |
| E11 | LOADING | slot flag bit 3: the pipeline may have read the bytes the reload replaces, so after its E4 a reload pipeline runs and the reload operation completes with that reload |
| E12 | LOADING | GPU-resident kind: a record whose loader has been called (stage BEGIN or any stage after it) restarts: it is cancelled as a load is — its in-flight reads report, its decode slices end, the loader is sent `Cancel` and acknowledges — with its live requests (their operations continue) and its record kept, then writer W2 gives it a new attempt and it re-enters HEADER_READ, so `BeginLoad` receives a new ticket and no ticket of an upload made before the loss is reused; a record in HEADER_READ or HEADER_VALIDATE continues unchanged (no loader call and no upload exist yet); a record already restarting or in CANCELLING continues; a last request cancelled during the restart turns it into CANCELLING (E6). Other kinds: nothing |
| E13, E14 | LOADING | the pipeline completes against the snapshot it holds, and its result stands; nothing else (a development reload is the editor's explicit E11) |
| E15 | LOADING | nothing |
| E16 | LOADING (CANCELLING) | every live request is cancelled (E6) |
| E17 | cannot occur | as for QUEUED |
| E18, E19 | LOADING | nothing: an admitted pipeline's kind is served |

ACTIVE:

| Event | Next | Action |
|---|---|---|
| E1 | ACTIVE | reference + 1 |
| E2 | ACTIVE | the request completes `SUCCEEDED` at the next slice; no pipeline runs |
| E3, E5, E6, E7 | cannot occur for the load pipeline | it has completed; the reload and recovery pipelines have their own table ("Reload and recovery pipelines") |
| E4 | cannot occur for the load pipeline | as above |
| E8 | CACHED or UNLOADING | a second pipeline is cancelled first and a RETIRING old object unloaded at once ("Reload and recovery pipelines"); then CACHED when the kind is cacheable, the GPU residency is neither LOST nor FAILED, cache admission is open, the entry is still in the snapshot and the asset's bytes fit after evicting from the LRU head; otherwise UNLOADING and `Unload` is called with no lock held. An object whose residency is LOST (its recovery queued or failed) or FAILED is never cached: a revive would skip its recovery and keep resolving to an object that is not resident, so it unloads and a subsequent acquire loads it again |
| E9, E10 | cannot occur | not cached, not unloading |
| E11 | ACTIVE | a reload pipeline is requested (reload RESERVING, "Reload and recovery pipelines") |
| E12 | ACTIVE | GPU-resident kind: residency LOST (W5; unchanged when already LOST) and, when no second pipeline is reserving, queued or running, a device-recovery pipeline is queued (recovery QUEUED); with one, that pipeline's E12 row of "Reload and recovery pipelines" applies; other kinds: nothing |
| E13 | ACTIVE | the object stays for its holders; the entry index is marked absent; a following E2 still completes `SUCCEEDED`; at E8 the asset unloads (it is not cached) |
| E14 | ACTIVE | the object stays; re-mapped to the new entry (the editor issues E11 to replace the object) |
| E15 | ACTIVE | re-mapped |
| E16 | ACTIVE | the object stays until its references drop (the pinned references are released by `BeginShutdown`, which may lead to E8); a second pipeline is cancelled and a RETIRING old object unloaded at once |
| E17 | UNLOADING | a second pipeline is cancelled first and a RETIRING old object unloaded at once; `Unload` is called even with references left (each logged as a warning with its count); `UnregisterKindLoader` services until E10 and any RA; at E10 the slot becomes UNLOADED with its references, so `Resolve` returns `NOT_ACTIVE` |
| E18, E19 | ACTIVE | nothing |

FAILED:

| Event | Next | Action |
|---|---|---|
| E1 | FAILED | reference + 1; `GetFailure` keeps the error |
| E2 | FAILED | the request completes `FAILED` with the recorded error at the next slice |
| E3 … E7 | cannot occur | no pipeline exists |
| E8 | freed | as for UNLOADED |
| E9, E10 | cannot occur | not cached, not unloading |
| E11 | QUEUED | the error is cleared and the reload operation becomes the new pipeline's request at `ASSET_PRIORITY_NORMAL`, in sub-stage CATALOGUE |
| E12 | FAILED | nothing |
| E13 | UNLOADED or FAILED | a `NOT_FOUND` or `NOT_INSTALLED` failure is cleared (UNLOADED; a following E2 fails `NOT_FOUND`); other failures stay |
| E14 | UNLOADED | the error is cleared; the next E2 loads the new entry ("Retry after a mount change") |
| E15 | UNLOADED or FAILED | a `NOT_FOUND` or `NOT_INSTALLED` failure is cleared (the file may now be installed); other failures stay until E11 or restart |
| E16, E17, E18, E19 | FAILED | nothing (a `NO_LOADER` failure stays: registration never reopens) |

UNLOADING:

| Event | Next | Action |
|---|---|---|
| E1 | UNLOADING | reference + 1; the unload continues (a loader's release is never aborted); the slot is not freed at E10 while the count is non-zero |
| E2 | UNLOADING | the request waits (slot flag bit 5); at E10 its pipeline starts |
| E3 … E7 | cannot occur | no pipeline exists |
| E8 | UNLOADING | nothing: the retire entry of a slot revived and released during its unload is ignored, and E10 frees the slot |
| E9 | cannot occur | not cached |
| E10 | UNLOADED, freed or QUEUED | resident bytes to 0, the object's dependency block released, residency NONE (W6: the attempt values kept); then QUEUED (sub-stage CATALOGUE) when a request waits, freed when the count is 0 (at the RA of a RETIRING record when one remains), else UNLOADED |
| E11 | UNLOADING | the reload operation completes `SUCCEEDED` at E10 (nothing to replace; a waiting request loads the current entry) |
| E12 | UNLOADING | nothing: the loader's `Unload` releases the lost device's resources through deferred deletion as it always does |
| E13, E14, E15 | UNLOADING | the entry index is marked absent or re-mapped; a waiting request loads against the new snapshot |
| E16 | UNLOADING | a waiting request completes `CANCELLED` at the next slice |
| E17 | UNLOADING | `UnregisterKindLoader` services until this slot's E10 before it returns |
| E18, E19 | UNLOADING | nothing |

CACHED:

| Event | Next | Action |
|---|---|---|
| E1 | ACTIVE | the 0 → 1 revive: the asset leaves the cache; `asset.cache.hits` + 1 |
| E2, E11 | cannot occur | a request carries a counted handle, whose acquisition revived the slot (E1) first |
| E3 … E7 | cannot occur | no pipeline exists |
| E8 | cannot occur | the count is already 0 |
| E9 | UNLOADING | `Unload` |
| E10 | cannot occur | no unload in progress |
| E12 | UNLOADING or CACHED | GPU-resident kind: evicted instead of recovered (an unreferenced object is not worth a reload); other kinds: nothing |
| E13, E14 | UNLOADING | evicted: a cached object of an entry that changed or left the catalogue is never revived |
| E15 | CACHED | re-mapped |
| E16 | UNLOADING | the cache is flushed |
| E17 | UNLOADING | evicted |
| E18, E19 | CACHED | nothing |

The state is the live state, advanced by ZenithAssets' threads; it is for presentation, tools and tests. Simulation never reads it ("Publication to simulation").

### Reload and recovery pipelines

An ACTIVE slot runs at most one second pipeline at a time in its side record: a development reload (slot flag bit 2) or a device recovery (bit 4). Each waits for admission like a load and takes its own load record at admission, marked `m_bReload` or `m_bDeviceRecovery` in `Zenith_AssetLoadInfo`, with a new attempt ticket (writer W1 of RuntimeAPI.md "Slot header layout"); the slot stays ACTIVE throughout. A reload that has swapped keeps its load record in stage RETIRING, holding the old object, the old object's dependency block and the peak reservation, until the old object's `AcknowledgeUnload`: that acknowledgement is the pipeline event RA, routed by the record, never the slot's E10, so the ACTIVE table's "E10 cannot occur" holds. The side record is free again from the swap, so a recovery of the new object can be queued while the old one retires. A RETIRING record counts as one load in flight with no bytes ("In-flight load budget"), and the slot is freed only when no RETIRING record remains. A cell this table does not list changes nothing in the pipeline: E1 and E2 apply to the slot's own table only, and E18 and E19 change nothing in any pipeline state, because an ACTIVE slot's kind is served, so a second pipeline never waits for a loader.

| Pipeline | Event | Next | Action |
|---|---|---|---|
| none | E11 on ACTIVE | reload RESERVING | issue the reload operation; the side record takes the reload in RESERVING |
| reload RESERVING | `ReserveTransitionPeak(TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY, resident estimate)` succeeds | reload QUEUED | sub-stage CATALOGUE at the priority of the reload operation (`ASSET_PRIORITY_NORMAL`); the entry's hard dependencies are requested into a new pipeline dependency block (the old object's block stays) |
| reload RESERVING | the reservation is refused | reload RESERVING | retried at the next `AdvanceFrame`; `asset.reloads.deferred` + 1 ([Z2-N-ARCH-815]) |
| reload RESERVING | the reload operation is cancelled, E8, E16 or E17 | none | the operation completes `CANCELLED` (nothing was reserved); an owed recovery's count ends; then the event applies to the slot |
| reload RESERVING or QUEUED | E12 | unchanged | the reload, once admitted, builds its object on the new device; side-record bits 2 (a recovery is owed) and 3 (counted by the recovery operation) are set |
| reload RESERVING | E13, E14, E15 | reload RESERVING | nothing: CATALOGUE reads the entry current when the reservation succeeds |
| reload QUEUED | E3 (admission) | reload RUNNING | a load record with `m_bReload` and a new attempt (W1) |
| reload QUEUED | the reload operation is cancelled | none | dependency requests cancelled, the new block released, the reservation released; the old object stays; an owed recovery is queued (recovery QUEUED) |
| reload QUEUED | a failure in CATALOGUE or DEPENDENCIES | none | the old object stays ACTIVE; the reload operation completes `FAILED`; the reservation and the new block are released; an owed recovery is queued |
| reload QUEUED | E13 | reload QUEUED, then none | it returns to CATALOGUE, which fails `NOT_FOUND`: the failure row above |
| reload QUEUED | E14 | reload QUEUED | it returns to CATALOGUE against the new entry, as a QUEUED load does: its new dependency requests are issued first, then the old ones cancelled and their block released |
| reload RUNNING | the loader activates `pNew` | reload RETIRING | swap the object pointer (release); W4 installs the reload's attempt (PENDING for a GPU-resident kind, or the early outcome its report recorded); the new dependency block becomes the object's and the old object with its block moves into the record; the reload operation completes `SUCCEEDED`; the side record is released; an owed recovery is dropped (the new object was built on the current device) and the slot's count in the recovery operation ends |
| reload RUNNING | failure | none | W3 closes the attempt; the old object stays ACTIVE; the reload operation completes `FAILED`; the reservation and the new block are released; an owed recovery is queued |
| reload RUNNING | the reload operation is cancelled | reload cancelling, then none | the record is cancelled as a load is ("Cancellation"), W3 at its end; the old object stays; the reservation and the new block are released; an owed recovery is queued |
| reload RUNNING | E12 | reload RUNNING, restarted | the record restarts as a LOADING record does under E12 (a new attempt, W2); bits 2 and 3 are set |
| reload RUNNING | E13, E14 | reload RUNNING | the pipeline completes against the snapshot its record holds, and its result stands, as a LOADING record does |
| reload RESERVING, QUEUED, RUNNING or RETIRING | a further E11 | unchanged | slot flag bit 3: when this reload ends (for RETIRING, at RA) one more reload runs, and the further operations complete with it |
| reload RETIRING | `asset.reload.retireFrames` frames have passed since the swap | reload RETIRING, unloading | `Unload` of the old object, with an unload context over the RETIRING record |
| reload RETIRING | RA (the old object's `AcknowledgeUnload`) | none | the old object's dependency block and the reservation are released and the record returns to the pool; a reload queued by bit 3 starts; a free of the slot that waited for RA happens now |
| reload RETIRING | E8 or E16 | reload RETIRING, unloading | `Unload` of the old object at once when not yet called (at zero references or at shutdown no `Resolve` pointer of the current frame can still be used); then the event applies to the slot, which is freed only after RA |
| reload RETIRING | E17 | reload RETIRING, unloading | `Unload` of the old object at once when not yet called; `UnregisterKindLoader` services until RA as well as until the current object's E10 |
| reload RETIRING | E12 | reload RETIRING | the old object: nothing (its `Unload` releases the lost device's resources through deferred deletion); the current object takes ACTIVE × E12, so its recovery is queued in the free side record and runs while the old object retires |
| reload RETIRING | E13, E14, E15 | reload RETIRING | nothing for the old object; the current object takes the ACTIVE rows |
| none | E12 on a GPU-resident ACTIVE slot | recovery QUEUED | the side record takes the recovery in sub-stage ADMISSION (the object's dependencies are ACTIVE and held), at `ASSET_PRIORITY_HIGH`, pinned slots `ASSET_PRIORITY_CRITICAL`, with request sequences issued in (pinned first, kind code, GUID) order; bit 3 is set |
| recovery QUEUED | E3 (admission) | recovery RUNNING | a load record with `m_bDeviceRecovery`, `m_pExistingObject` and a new attempt (W1) |
| recovery QUEUED | E12 | recovery QUEUED | it stays queued in its admission position and is counted by the new recovery operation ("Device-loss reload") |
| recovery RUNNING | E12 | recovery RUNNING, restarted | it restarts as a LOADING record does under E12 (a new attempt, W2), so it never activates an object whose GPU side was created on the lost device; counted by the new operation |
| recovery RUNNING | the loader activates the existing object | none | W4 installs the recovery's attempt: PENDING, or the early outcome its report recorded; the slot's count in the recovery operation ends |
| recovery RUNNING | failure | none | W3; residency stays LOST (the object reports itself not resident and its consumers bind fallbacks); logged; the recovery operation records the first error and the slot's count ends |
| recovery QUEUED or RUNNING | E11 | unchanged | the reload is requested when the recovery ends |
| recovery QUEUED or RUNNING | E13, E14 | cancelling, then none | the recovery is cancelled (a queued one at once, a running one as a load is, W3 at its end), because the bytes the existing object was built from are no longer the entry's; residency stays LOST; logged at info; the slot's count ends with no error; the object is replaced by the editor's E11, or unloads at E8 |
| recovery QUEUED or RUNNING | E15 | unchanged | re-mapped |
| any queued or running pipeline | E8 (the count reaches 0) | cancelling, then none | the second pipeline is cancelled first (a queued one at once, a running one as a load is); a recovery's count ends; then E8 applies to the slot, whose residency LOST or FAILED sends it to UNLOADING |
| any queued or running pipeline | E16 | cancelling, then none | cancelled with every other operation; a recovery's count ends and the recovery operation completes `CANCELLED` |
| any queued or running pipeline | E17 | cancelling, then none | cancelled first as at E8, `UnregisterKindLoader` servicing until it ends; then E17 applies to the slot |

### GPU residency sub-state

For a GPU-resident kind (`GetGpuResidentKinds()`, test kinds included) in a windowed registry, an ACTIVE or CACHED slot also carries a `Zenith_AssetGpuResidency`; a CPU kind and every headless slot stay NONE. The residency is one field of the slot's atomic residency word (`m_ulResidency`, RuntimeAPI.md "Slot header layout"), which also holds the installed attempt value, the current attempt value, the open flag and the open attempt's early outcome, and every transition below is one compare-exchange of that word by one of the writers W1 … W7 that the layout's table "Residency word writers" enumerates. Every admitted pipeline is an attempt with a ticket of its own (W1 at admission, W2 at an E12 restart), so no ticket is reused by an attempt cancelled after its infallible push or by a restarted one, and a report that arrives before its attempt's activation is kept as the early outcome that the activation installs ("Zenith_AssetRegistry" in RuntimeAPI.md).

| Residency | Event | Next | Action |
|---|---|---|---|
| any | E3, or an E12 restart | unchanged | W1 or W2: a new attempt value, marked open; the ticket `{slot, generation, attempt}` in the attempt's `Zenith_AssetLoadInfo::m_xResidency` |
| any | a report with the open attempt's ticket, before its activation | unchanged | W7 records the early outcome, RESIDENT or FAILED; a FAILED one is counted in `asset.gpu.failed` and logged once, at once |
| any | the attempt ends without activating | unchanged | W3: the attempt is closed and its early outcome discarded; its value is never issued again |
| NONE (a load), any (a reload swap), LOST (a recovery) | the attempt activates | PENDING, RESIDENT or FAILED | W4 installs the attempt: its early outcome when one was recorded, else PENDING (NONE for a CPU kind or a headless registry) |
| PENDING | `ReportGpuResident` with the installed ticket | RESIDENT | W7 |
| PENDING | `ReportGpuFailed` with the installed ticket | FAILED | W7; `asset.gpu.failed` + 1; one `asset.load` error record with the loader's code; the kind's fallback applies ("Fallback policy per kind") |
| any | a stale ticket, or a second report of one attempt | unchanged | ignored |
| PENDING, RESIDENT, FAILED | E12 | LOST | W5; a recovery is queued (ACTIVE × E12) |
| LOST | E12 | LOST | nothing more: ACTIVE × E12 and "Reload and recovery pipelines" decide what is queued |
| any | E10 | NONE | W6: the attempt values are kept |

The gauges follow the residency field, whichever writer changes it: every change from r0 to r1 moves `asset.gpu.pending`, `asset.gpu.lost` and `Zenith_AssetRegistryStats::m_auGpuResidencyCounts` by one down for r0 and one up for r1 (RuntimeAPI.md "Slot header layout", last bullet); recording or discarding an early outcome moves none.

- A GPU-resident kind's loader calls `Activate` once its object is complete on the CPU side and every upload it needs has been requested: handed to the kind owner's pending-upload list by a step that cannot fail. Every fallible step of that hand-off (a staging allocation, a list node) comes before `Activate`, and its failure is `Fail`, a load failure. The owner performs the fallible GPU work afterwards at its own render-graph node (GEOM's pool allocation and `RequestUpload` at `Flux.BuildGraph`, retrying `FLUX_UPLOAD_ERROR_QUEUE_FULL` at its next run, `Zenith2/Docs/Rendering/GpuResources.md → "Upload paths"`). So the loader never waits for the owner's allocation, a rendered frame, the copy timeline or the render subgraph, and a GPU-kind asset reaches ACTIVE during startup step 17 or earlier, before the first frame renders ("Tool and boot servicing"). The owner's node runs on its own thread, so an upload may complete, and the loader report it, before `Activate` has returned or before the service has applied it (a starved background service slice, a pre-empted loader thread); that report is the attempt's early outcome.
- After activation the owner's work either completes — the loader calls `ReportGpuResident` — or fails: a pool allocation that finds its pool full (`POOL_FULL`) or an upload that reports `FAILED` is `ReportGpuFailed(ticket, code)`, which is not a load failure (the asset stays ACTIVE and its requests have completed `SUCCEEDED`). A loader makes exactly one report per attempt of a GPU-resident kind, with that attempt's ticket, whether it comes before or after the activation. A FAILED object reports itself not resident; `ResolveOrFallback` substitutes the kind's fallback for it exactly as for a FAILED load, and the owner's consumers bind their usage fallbacks as they do for PENDING (`Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"`: a texture not yet tail-resident binds the fallback of its usage). The failure is counted and logged once per attempt; it is cleared only by a new activation — a development reload (E11) or a device recovery (E12) — or by the unload at zero references (a FAILED object is never cached), never retried by the owner on its own.
- A loader keeps the CPU data its requested uploads read until it reports RESIDENT or FAILED; `DropCpuCopies` releases CPU copies of RESIDENT and FAILED objects only ("Memory pressure responses").
- The residency is presentation state like the load state (`GetGpuResidency`); no simulation code reads it.

### Load record stages

A pipeline passes these stages in order; the first four are sub-stages of the slot state QUEUED and use only the slot's side record, the rest are the stages of an admitted load record in the slot state LOADING:

| Stage | Entered when | Leaves when |
|---|---|---|
| CATALOGUE (QUEUED) | E2 on an UNLOADED, FAILED (E11) or UNLOADING (at E10) slot, or E14 | at the next slice: the entry is found in the current snapshot (→ DEPENDENCIES) or not (E5 `NOT_FOUND`); a kind with no loader sets the "no loader" flag while registration is open and is E5 `NO_LOADER` once it is closed |
| DEPENDENCIES (QUEUED) | the entry is found | every hard dependency is ACTIVE, or FAILED with policy SUBSTITUTE and its kind's role-0 pinned fallback ACTIVE (→ WAIT_LOADER); a SUBSTITUTE dependency whose pinned fallback is not yet terminal keeps it waiting; a dependency FAILED otherwise, or whose fallback is absent or FAILED → E5 `DEPENDENCY_FAILED`; an entry with no hard dependency passes at once ("Dependency loading") |
| WAIT_LOADER (QUEUED) | dependencies satisfied | the "no loader" flag is clear (→ ADMISSION at once), or E18 for the kind (→ ADMISSION), or E19 while the kind is still unserved (E5 `NO_LOADER`); counted in the gauge `asset.loads.waiting_loader` |
| ADMISSION (QUEUED) | a served kind with satisfied dependencies, or a device recovery at E12 | E3: the in-flight budget admits it in (priority, request sequence) order and gives it a load record (→ HEADER_READ) |
| HEADER_READ | admission | the read of `m_uHeaderBytes` completes (→ HEADER_VALIDATE) |
| HEADER_VALIDATE | read complete | `Zenith_ParseCookedHeader` succeeds (→ BEGIN) or fails (FAILED) |
| BEGIN | validated | the loader's `BeginLoad` returns having requested sections (→ SECTIONS) or made a terminal call |
| SECTIONS | a round is requested | every section record of the round is DONE (→ DELIVER) |
| DELIVER | the round is complete | `OnSectionsLoaded` returns: another round requested (→ SECTIONS), a terminal call made, or neither (→ ACTIVATING) |
| ACTIVATING | `OnSectionsLoaded` returned with no terminal call | the loader, in its own job, completes the object on the CPU side and, for a GPU kind, requests its uploads from its owner, then calls `Activate` (→ ACTIVE) or `Fail` (→ FAILED) |
| CANCELLING | the last live request is cancelled in any admitted stage above | every in-flight I/O and decode has completed or been cancelled and, when the loader was involved, `AcknowledgeCancel` arrives (→ UNLOADED, or QUEUED for a request that arrived meanwhile) |
| RETIRING | a reload's record after its swap ("Reload and recovery pipelines") | the old object's `AcknowledgeUnload` (RA): the record returns to the pool |

Section record stages: WAIT_UNIT → READING → DECODE_READY → DECODING → DONE, or CANCELLED; the stored-section (`NONE` codec) path skips DECODE_READY's staging and runs its check in DECODING.

### Request deduplication and priorities

- One slot per GUID and one load pipeline per slot: every request for an asset joins its existing pipeline, counted in `asset.loads.deduplicated`; each request is its own `Zenith_OperationHandle` issued from the loader's operation table (capacity `asset.load.maxInFlight` × 8 + 256).
- A request holds a reference to the asset until its operation completes, so an asset under load always has references.
- The pipeline's priority is the minimum `Zenith_AssetPriority` value (the most urgent) over its live requests; it is recomputed on every new request, `SetPriority` and cancellation, and applied to the next read issued, the next decode picked and to its hard dependencies' pipelines (which take the minimum over their own requests and every dependent's).
- Admission order among pipelines in sub-stage ADMISSION (loads, reloads and recoveries alike): priority, then request sequence (a per-loader counter), so equal-priority pipelines start in request order.
- Priorities map one to one onto `Zenith_IOPriority` for reads and onto `JOB_PRIORITY_BACKGROUND_HIGH` (CRITICAL, HIGH) or `JOB_PRIORITY_BACKGROUND_LOW` (NORMAL, LOW, BACKGROUND) for decode slices.
- A pipeline stops only when its last live request is cancelled; cancelling one of several requests completes that request `CANCELLED` (its reference and completion slot released) and the pipeline continues.

## The service

### Service scheduling

- All pipeline state is advanced by one background operation at a time, `bg/Asset.Service` (an atomic "scheduled" flag: a kick sets it with `exchange(true)` and submits only when it was false).
- Kicks come from: `RequestLoad`, `SetPriority`, a cancellation (the operation's cancel flag is observed by the next slice: destroying a request's `Zenith_OperationHandle` kicks nothing, because CORE's operation handle has no hook, so the cancel is seen by the next slice that any other event or the `Assets.Service` node starts; releasing an asset handle's last reference kicks through the retire queue), a decode slice completing, a loader terminal call, `AdvanceFrame`, the retire queue, `OnMountsChanged`.
- One slice runs at most 2 ms of wall-clock time ([Z2-N-ARCH-393]): it drains, in order, the request intake queue, the loader acknowledgement queue, the decode completion queue, the I/O completion queue (`Zenith_IOCompletionQueue` owned by the loader) and the retire queue; then advances records in priority order, issuing reads and decode slices within "Decompression back-pressure"; and ends early when the budget is spent, re-submitting itself as a continuation (`Zenith_JobContext::Continue`) when work remains.
- A slice with nothing ready but I/O in flight clears the flag and ends; the `Assets.Service` node or the next event kicks it again.
- Loader calls (`BeginLoad`, `OnSectionsLoaded`, `Cancel`, `Unload`) are made from the service slice with no ZenithAssets lock held; a loader must return within 1 ms and put heavy work in its own jobs. A call that overruns counts `asset.decode.slice_overruns` once per slice.

### Internal queues

Every queue ZenithAssets owns is a CORE `Zenith_BoundedQueue` (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_BoundedQueue"`) with a declared policy ([Z2-N-CONTRACT-343]) and the three counters of [Z2-N-CONTRACT-345], published every frame by `Assets.Service`:

| Queue (counter prefix) | Producers → consumer | Capacity | Policy | Why it cannot misbehave when full |
|---|---|---|---|---|
| request intake (`asset.requests.queue`) | `RequestLoad`, `SetPriority`, cancellations from any thread → the service | `Zenith_AssetLoaderDesc::m_uRequestQueueCapacity` (power of two, default 4,096) | `QUEUE_FULL_POLICY_REJECT`: `RequestLoad` returns `ASSET_ERROR_QUEUE_FULL` and the caller handles it; a cancellation never needs a slot (it sets the operation's flag, which the next slice reads) | the producer is told |
| retire (`asset.retire.queue`) | the last handle release → the service | `m_uMaxAssets` | `QUEUE_FULL_POLICY_FATAL` | at most one entry per slot exists (the retire-queued flag), so it never fills; a full push is a defect |
| loader acknowledgement (`asset.ack.queue`) | `Activate`, `Fail`, `AcknowledgeCancel`, `AcknowledgeUnload` from any thread → the service | `asset.load.maxInFlight` + 2 × `m_uMaxAssets` | `QUEUE_FULL_POLICY_FATAL` | at most one call per load record is outstanding — its terminal call, or for a RETIRING record the old object's `AcknowledgeUnload` (RA) — and the pool holds exactly `asset.load.maxInFlight` records (a record returns to it only after the service drains that call), whatever the number of queued loads, reloads and recoveries; besides, per slot at most one `AcknowledgeUnload` of its current object (E10) is outstanding. So a slot has at most two unload acknowledgements outstanding — E10 and one RA, never two RAs, because RETIRING ends at RA and a further reload starts only after it — and the capacity exceeds the bound. `ReportGpuResident` and `ReportGpuFailed` do not use the queue |
| decode completion (`asset.decode_done.queue`) | decode slices → the service | `asset.decode.queueDepth` | `QUEUE_FULL_POLICY_FATAL` | one entry per reserved decode unit at most |
| I/O completion (`asset.io_done.queue`; PLAT's `Zenith_IOCompletionQueue`) | PLAT's I/O threads → the service | `asset.decode.queueDepth` + `asset.load.maxInFlight` | as PLAT declares for the queue type, with this capacity | a read is issued only with a reserved unit or a header reservation, so completions in flight never exceed the capacity |
| requester completion queues (`<name>`) | the service → the owning requester | the requester's, 1 … 65,536 | `QUEUE_FULL_POLICY_REJECT` at submission (RuntimeAPI.md "Zenith_AssetLoadCompletionQueue") | a slot is reserved when the request is accepted |

The decode queue is not a queue of elements but a count of reserved decode units ("Definitions"); its admission step behaves as `QUEUE_FULL_POLICY_REJECT` (the service's attempt to reserve fails, the record stays WAIT_UNIT and is retried at a following slice), because the service is a job and a job producer never blocks ([Z2-N-CONTRACT-344]); it publishes `asset.decode.queue.full`, `.dropped` (always 0) and `.high_water` like the others. The `.dropped` counter of every queue above is 0 by construction; a FATAL queue's `.full` is never observed.

### The Assets.Service node

The engine node `Assets.Service` (EPILOGUE, stage 12, group 0, "Assumed interfaces pending other areas" in the entry document; the node's catalogue row is core-doc item X110, pending) calls `Zenith_AssetLoader::ServiceFrame`, which within `asset.service.frameBudgetUs`:

1. calls `Zenith_AssetRegistry::AdvanceFrame(frameIndex)` (cache LRU ages, reload retirements, catalogue snapshot swaps);
2. drains the I/O completion queue into the records (so completions are noticed at least once per frame even when no event kicked the service);
3. publishes the counters of `Zenith2/Docs/Design/ASSET.md → "Counters"`, the queue counters of "Internal queues" included;
4. kicks `bg/Asset.Service` when any record is ready to advance.

The node reads and writes no world; it runs in headless runs too.

### Tool and boot servicing

- `ServiceUntilComplete(xLoad, uTimeoutMs)` runs service slices on the calling thread until the operation is complete or the timeout passes, sleeping with `Zenith_YieldThread` between slices when only I/O is in flight; decode slices still run as background jobs (the job system is running), and the calling thread is not a job worker, so waiting is allowed ([Z2-N-CONTRACT-315]). Tools and tests (the cook's validation, `ZenithAssets_Tests`) use `ServiceUntilComplete` with no frame loop.
- The boot model, in startup order (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`):

| Startup step | What happens to assets |
|---|---|
| 5 | the package and patch mounts create their `.zpak` sources through ZenithAssets' archive factory ("Zenith_AssetArchive header" in RuntimeAPI.md) |
| 6 | `Zenith_RegisterAssetsConfigKeys` and `Zenith_RegisterAssetTypes` |
| 10 | `InstallValueKindHooks(PROPERTY_TYPE_ASSET, Zenith_GetAssetHandleValueKindHooks())` before the schema blob |
| 11 | `Zenith_AssetRegistry::Create` (the built-in loaders registered; headless: the validate-only loader for every GPU-resident kind); `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, &Zenith_ResolveAssetReferenceDefault, registry)`; a failure of either is exit code 3; then, in the same step, ZenithScene's `Zenith_RegisterSceneAssetKinds` registers the scene and prefab loaders (`Zenith2/Docs/Design/SCENE.md → "Construction and shutdown"`) |
| 13, 15 | Flux (step 13) and the modules (step 15) call `RegisterKindLoader`; each registration pins its kind's fallback-table assets and requests them at CRITICAL, and, at the service's next slice, resumes every pipeline of its kind waiting in WAIT_LOADER (E18) |
| after 15 | `CloseRegistration`: the service's next slice fails with `NO_LOADER` (E19) every pipeline still waiting whose kind is unserved when that slice reads the served set, so a registration made just before is seen first; `ServiceUntilComplete(GetPinnedLoads(), runtime.startupLoadTimeoutMs)`; a pinned failure, `NO_LOADER` included, or a timeout is exit code 3 naming the kind, the role and the error |
| 17 | the startup scene loads through SCENE's loader, which requests the scene and its hard dependencies at CRITICAL and waits for them, bounded by `runtime.startupLoadTimeoutMs` (MOD's key) |
| 19, frames ≥ 1 | each GPU kind's owner allocates and queues, at its render-graph node, the uploads its pending list holds (GEOM at `Flux.BuildGraph`), and the copy work runs them; each loader reports `ReportGpuResident` as its uploads complete, or `ReportGpuFailed` when an allocation or upload fails (frame 0 runs no render subgraph, `Zenith2/Docs/Architecture/CompositionRoot.md → "The frame loop"`) |

- Registrations span steps 11 to 15 while pinned requests start at each registration, so a pinned asset or a hard dependency of a kind whose loader registers at a subsequent step — a default material's textures when the material loader registers first, an ENV or LIGHT fallback depending on another module's kind — waits in WAIT_LOADER (the gauge `asset.loads.waiting_loader`) and resumes at that kind's registration (E18); only `CloseRegistration` turns a still-missing loader into `NO_LOADER` (E19).
- No asset waits for rendering: a GPU-resident kind's loader activates as soon as its object is complete on the CPU side and its uploads are requested from its owner ("GPU residency sub-state"), so pinned GPU-kind fallbacks and a scene's GPU-kind hard dependencies are ACTIVE (residency PENDING) before steps 15 and 17 return, and the hard-dependency rule of "Dependency loading" holds for every kind, scenes included. A texture or mesh that is ACTIVE but PENDING draws as its usage's fallback, or not at all, until its upload completes; nothing is simulation-visible about it.
- In a headless run the GPU-resident kinds activate through the validate-only loader with a null object and residency NONE, so steps 15 and 17 complete the same way.

<!-- assumption: pending ARCH (X110, X147), MOD -->
The ASSET calls in the steps above are core-doc items for the startup table — X110 (pending) for steps 6, 10, 11 (`Create`), 15 and 17, X147 (4) (pending) for step 11's `ResolveReferenceDefaults` — and cross-area rows AS5 and AS12 for MOD's composition root.
<!-- /assumption -->

<!-- assumption: pending PLAT (X154) -->
The step-5 archive factory is a new ask of PLAT and of the startup table, core-doc and area item X154 (open) and cross-area row AS22; PLAT's filesystem design has no factory member today.
<!-- /assumption -->

## Pipeline stages

### Catalogue lookup

Sub-stage CATALOGUE of a QUEUED pipeline (no load record yet), at the first service slice after the pipeline is created or returned to it: find the entry of the slot's GUID in the current snapshot (`NOT_FOUND` when absent); check its kind against `GetServedKinds()` — served passes; unserved while registration is open sets the pipeline's "no loader" flag and continues (the pipeline will wait in WAIT_LOADER, "Load record stages"); unserved after `CloseRegistration` fails `NO_LOADER` (headless: the validate-only loader serves every GPU kind, "Headless loading"). At admission the load record takes a reference on the snapshot current then, finds the entry by the slot's `m_uCatalogueEntry` and builds the `Zenith_CookedFileExpectation` from it and from `GetKindInfo(kind)` (the magic).

### Dependency loading

Sub-stage DEPENDENCIES of the QUEUED pipeline, before admission, so a dependent never holds a load record or in-flight capacity its dependencies need:

1. Allocate the pipeline's dependency block (`16 × hard-dependency count` bytes from the registry's heap; failure is `OUT_OF_MEMORY`) and, for every dependency with `ASSET_DEPENDENCY_FLAG_HARD`, acquire a handle into it and issue an internal request at the pipeline's priority; at activation the block becomes the object's and is kept for its ACTIVE life, so a dependency outlives its dependent (RuntimeAPI.md "Slot side records").
2. Soft dependencies are acquired and requested only with `ASSET_LOAD_FLAG_PREFETCH_SOFT_DEPENDENCIES`, at BACKGROUND priority, and never waited for.
3. The pipeline waits in DEPENDENCIES until each hard dependency is ACTIVE, or FAILED with policy SUBSTITUTE and its kind's role-0 pinned fallback ACTIVE (the dependent then loads against the fallback, `Test_AssetLoader_DependencyFallbackAllowsParent`). A SUBSTITUTE dependency that fails while that fallback is still loading keeps the dependent waiting until the fallback is terminal, so the outcome never depends on which of the two finishes first; only a fallback that is absent (no loader registered, so none pinned) or itself FAILED fails the dependent, as any other failure does, with `DEPENDENCY_FAILED` and the dependency's GUID in the error.
4. The cook guarantees the hard-dependency graph is acyclic; the catalogue loader re-checks (`CATALOGUE_INVALID`), so a runtime wait cycle cannot occur.
5. The dependencies' handles are passed to `BeginLoad` in `m_axDependencies` in catalogue order.
6. Because waiting pipelines hold no load record and count against neither in-flight limit, and the hard-dependency graph is acyclic, the leaves of any dependency tree reach ADMISSION first and admission can always make progress: no set of dependents can occupy the budget their dependencies wait for (`Test_AssetResidency_InFlightBudget`). This holds only because a kind loader never waits for a load it requested: it never defers `Activate`, `Fail` or `AcknowledgeCancel` until a `RequestLoad` of its own completes (the loader contract, RuntimeAPI.md "Zenith_AssetKindLoader"), since such a loader would hold a load record while its target waited for one, and `asset.load.maxInFlight` of them would deadlock admission. What a loader needs ACTIVE before its activation is a catalogue hard dependency; the requests it makes are fire-and-forget.

### Header read and validation

1. The pipeline has just been admitted and holds its load record. Reserve `m_uHeaderBytes` of staging (a header is at most 163,904 bytes) and one decode unit (headers count against the queue like sections); issue one read of `[0, m_uHeaderBytes)` of the object path at the record's I/O priority, with the loader's completion queue.
2. On completion: `PATH_ERROR_NOT_FOUND` (through `IO_ERROR_NOT_FOUND`) → FAILED `NOT_INSTALLED`; another I/O error → FAILED `IO`.
3. `Zenith_ParseCookedHeader(bytes, expectation, recordArena)` applies Formats.md "Checks" in order; any failure → FAILED with its code.
4. The staging is released; the view lives in the record's arena until the record ends.

### Section planning

`BeginLoad` requests a round of sections. For each requested index ZenithAssets creates a section record: the destination is the loader's span or, when empty, a decode buffer of `m_ulDecodedBytes` from the staging pool's decode partition; the choice of path is "stored" (codec NONE, read straight into the destination), "one-shot" (stored bytes ≤ `asset.io.streamThresholdBytes`: read into staging, decode whole), or "streamed" (larger: "Streamed decode of large sections").

### Section read

A section record in WAIT_UNIT issues its read only through "Issue rule". A read covers `[m_ulOffset, m_ulOffset + m_ulStoredBytes)`, the destination being the staging span (compressed) or the final destination (stored). I/O completion with an error → the load fails `IO` with the section index; `IO_ERROR_QUEUE_FULL` at submission returns the record to WAIT_UNIT with its reservations released and counts `asset.io.queue_full`.

### Section decode

1. A DECODE_READY record is picked by the decode queue ("Decode concurrency and slices") and runs as `bg/Asset.Decode` slices.
2. Verify: `Zenith_VerifySection(entry, storedBytes)` (the size and the stored check); failure → FAILED `CORRUPT` / `BAD_SECTION_CHECK` with the section index.
3. Decode: codec NONE → nothing to do (the bytes are in place); otherwise `Zenith_DecompressSection(codecs, desc, dictionaries, stored, destination, workerScratch, limits)` with the codec set selecting the codec from the descriptor, never from configuration (Z2-D-004, `Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"`); dictionaries are the `ASSET_KIND_COMPRESSION_DICTIONARY` hard dependencies the file's descriptors name (their runtime objects are `Zenith_CompressionDictionary`); the worker scratch arena is sized once per decode thread by `GetDecompressScratchBound` of the largest section the catalogue lists and counted in Z2-B-ASSET-001. A failure → FAILED `CORRUPT` / `DECODE_FAILED` with the COMPRESS code in the detail's high byte.
4. One-shot decodes of more than `asset.decode.sliceBytes` decoded bytes are not allowed: such sections take the streamed path whatever their stored size, so every slice stays within 2 ms.
5. On success: staging released, the decode unit released, the record DONE, the service kicked.

### Delivery and activation

1. When a round's records are all DONE, the service calls `OnSectionsLoaded(context, sections)` in table order.
2. Buffers ZenithAssets allocated for the round are released when `OnSectionsLoaded` returns (the loader copied what it keeps); loader-owned destinations are the loader's.
3. The loader requests another round (at most `uASSET_MAX_SECTION_ROUNDS` = 8, else `LIMIT_EXCEEDED`), or calls `Activate` now, or calls it from its own job once the object is complete on the CPU side (stage ACTIVATING), or calls `Fail`. A GPU kind's `Activate` comes once its uploads are requested from its owner's pending list, never at their completion: completion is `ReportGpuResident`, a failure after activation `ReportGpuFailed` ("GPU residency sub-state").
4. `Activate(pObject)`: store the object (release), set ACTIVE, install the attempt in the residency word (writer W4 of RuntimeAPI.md "Slot header layout": PENDING, or the early outcome a report recorded first), advance the availability epoch for a non-null object (`GetAvailabilityEpoch`, "Zenith_AssetRegistry" in RuntimeAPI.md), complete every live request `SUCCEEDED` with a completion record, return the record to its pool, record `asset.critical_latency_us` for a CRITICAL load.

## Decompression back-pressure

### Definitions

| Term | Meaning |
|---|---|
| decode unit | one reservation in the decode queue; held by a header read or a section record from the moment its read is issued until its decode completes, fails or is cancelled |
| D | the decode queue depth, `asset.decode.queueDepth`, the value of Z2-B-ASSET-002 |
| S | the staging budget, `asset.io.stagingBytes` (part of Z2-B-ASSET-001) |
| C | decode concurrency, `asset.decode.concurrency` (0 means `max(1, workers / 2)`) |
| chunk | `asset.io.chunkBytes`, the streamed-decode read size |

The decode queue is the set of reserved units, bounded at D. Its admission is the one policy "Internal queues" declares: it behaves as `QUEUE_FULL_POLICY_REJECT` — the service's attempt to reserve a unit fails, the record stays in WAIT_UNIT and is retried at a following slice — because the service is a job and a job producer never blocks ([Z2-N-CONTRACT-344]). The consequence of a refused reservation is the breach code of Z2-B-ASSET-002, BR-STALL: the read waits, `asset.decode.queue.full` counts it, and the profiler shows it ("Back-pressure counters").

### Issue rule

A section read, or a header read, is issued only when all of these hold at the moment of issue; otherwise it stays WAIT_UNIT and the service moves on:

1. a decode unit can be reserved within its priority's limit ("Priority reserves");
2. `stagingReserved + need ≤ S`, where `need` is the stored bytes (one-shot compressed path), 2 × chunk (streamed path) or 0 (stored path, which reads into its destination);
3. the loads-in-flight and in-flight-bytes limits admitted the record ("In-flight load budget");
4. `Zenith_AsyncIO::Read` accepts the request.

Reservations are made before the read is submitted and released when the decode finishes, so at every instant reads in flight plus undecoded sections plus decoding sections is at most D, and reserved staging is at most S: a burst of any number of requests never puts more work in flight than the budget rows allow (`Test_AssetBackPressure_BurstNeverExceedsDepth`). This is how I/O is throttled by decode capacity (Z2-I-ASSET-17, [Z2-N-BUDGET-238]). A single compressed section whose stored bytes alone exceed S takes the streamed path.

Candidates are considered in (priority, request sequence) order each slice; a record that cannot issue does not block lower-priority records whose needs fit (a small stored section may pass a large waiting one), but a record waiting for staging for more than 8 consecutive slices blocks subsequent records of its own and lower priority from taking staging, so large sections are not starved.

### Priority reserves

| Pipeline priority | May hold units while reserved units are below |
|---|---|
| CRITICAL, HIGH | D |
| NORMAL, LOW | D − ⌈D / 4⌉ |
| BACKGROUND | ⌊D / 2⌋ |

The same fractions apply to staging bytes (CRITICAL and HIGH: S; NORMAL and LOW: S − ⌈S / 4⌉; BACKGROUND: ⌊S / 2⌋). So prefetch can never occupy the capacity a CRITICAL load needs, and a CRITICAL read waits at most for the decodes already running.

### Streamed decode of large sections

1. The section record reserves one unit and 2 × chunk of staging and creates a `Zenith_CompressionContext` in decompress mode with the descriptor's codec and dictionary (`Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressionContext.h"`), pledging `m_ulDecodedBytes`.
2. It reads chunk 0 into buffer A, then keeps one read ahead (chunk k+1 into the other buffer) while a decode slice feeds chunk k to the context, writing into the destination at the context's output offset, and updates a `Zenith_Hash64Stream` with the stored bytes for the stored check.
3. Each decode slice processes at most `asset.decode.sliceBytes` of output, then continues; a slice never waits for a read — when the next chunk has not arrived it ends and the read's completion re-queues the record.
4. At the end: the stream's hash must equal `m_uStoredCheck` and the produced bytes `m_ulDecodedBytes` (`BAD_SECTION_CHECK`, `DECODE_FAILED` otherwise).
5. Stored (`NONE`) sections larger than the threshold are read straight into the destination in chunk-sized reads, and the check is computed in decode slices of `asset.decode.sliceBytes` each.

### Decode concurrency and slices

- At most C decode slices run at once; the decode queue picks the next DECODE_READY record by (priority, request sequence).
- A slice runs `Zenith_DecompressSection` only when the section's decoded bytes are at most `asset.decode.sliceBytes`; everything larger is streamed in slices of that size, so a slice's work is bounded by design and the 2 ms rule holds at the COMPRESS throughput floors (Z2-B-COMPRESS-001/002: `asset.decode.sliceBytes` ≤ floor × 2 ms is checked by the Stage 1 ticket when it sets both).
- A slice that runs longer than 2 ms counts `asset.decode.slice_overruns`.

### Back-pressure counters

`asset.decode.queue.depth` (reserved units), `.high_water`, `.full` (issue attempts refused for want of a unit), `asset.io.staging_bytes`, `.staging_high_water`, `asset.io.throttled`, `asset.io.queue_full`, `asset.decode.slices`, `.slice_overruns`; the on-screen profiler's streaming panel shows the depth against Z2-B-ASSET-002. A refused issue is the BR-STALL breach consequence: it waits, counts, and is visible.

## Cancellation

### Cancellation points by stage

A request is cancelled by `Zenith_OperationHandle::RequestCancel()` or by destroying its handle while incomplete ([Z2-N-CONTRACT-091]); when it was the pipeline's last live request, the pipeline enters CANCELLING at the next service slice:

| Stage when cancelled | Behaviour |
|---|---|
| QUEUED: CATALOGUE, WAIT_LOADER, ADMISSION | the side record is cleared (no load record exists); nothing was read |
| QUEUED: DEPENDENCIES | the internal requests on dependencies are cancelled (their pipelines stop unless others request them); the dependency block and its handles are released |
| HEADER_READ, SECTIONS in READING (I/O) | `RequestCancel` on every in-flight read; PLAT removes queued reads without I/O and finishes an in-flight chunk before reporting (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Cancellation"`); the section's staging, unit and destination are released only after the read reports complete, so no buffer is freed under an I/O thread |
| SECTIONS in DECODING (decompression) | the decode slice checks the record's cancel flag at slice start and between streamed-decode steps; it destroys its `Zenith_CompressionContext`, releases staging and the unit, and ends; a one-shot decode already running finishes its (bounded) call first |
| DELIVER, ACTIVATING (the loader builds the object and requests its uploads) | `Zenith_AssetKindLoader::Cancel(context)`: the loader withdraws any upload request it has handed to its owner's pending list, releases partial resources through deferred deletion (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`) and calls `AcknowledgeCancel`; ZenithAssets waits for it. Uploads after activation belong to the object and are cancelled by its `Unload` |
| after `Activate` (activation) | too late: activation is one atomic publication; the requests complete `SUCCEEDED` ([Z2-N-CONTRACT-337]); the requester releases its handle to unload |
| a round's `OnSectionsLoaded` in progress | the loader sees `IsCancelRequested()`; ZenithAssets sends `Cancel` after the call returns |

### Completion reporting

- A cancelled request of a pipeline that continues completes `CANCELLED` at the next slice, after its completion slot and reference are released.
- When the pipeline itself is cancelled, every live request completes `CANCELLED` only after the last in-flight read has reported, every decode slice has ended and the loader has acknowledged: the operation reports complete after everything it held is released ([Z2-N-CONTRACT-335], [Z2-N-CONTRACT-336]).
- Completion order among one pipeline's requests is request order; each pushes its completion record (when it named a queue) before its operation status is set, so a requester that drains its queue after seeing the status finds the record.
- `SUCCEEDED` is reported only after the object is published (progress 1.0 no earlier than success, [Z2-N-CONTRACT-094]); progress is reported as (sections done / sections requested) × 0.9 before activation.

### Ownership of partial results

| Resource | Owner during the load | Released on cancel or failure by |
|---|---|---|
| catalogue snapshot reference, the file view | the load record | ZenithAssets, at record release |
| the pipeline's dependency block and its handles | the slot's side record | ZenithAssets, when the pipeline is cancelled or fails (at activation the block becomes the object's) |
| staging and decode units | the section record | ZenithAssets, after the I/O or decode completes |
| ZenithAssets decode buffers | the section record until `OnSectionsLoaded` returns | ZenithAssets |
| loader-owned destinations and every object, buffer, texture or upload the loader created | the kind loader | the loader, in `Cancel`, before `AcknowledgeCancel` |
| operation slots and completion slots | the request record | ZenithAssets, at completion |

After a cancelled load nothing of it remains: `Test_AssetCancel_Stress1000Cycles` asserts it for every stage ([Z2-N-CONTRACT-339]).

## Publication to simulation

- A load completes on ZenithAssets' threads at a time that depends on I/O and scheduling. Simulation must never observe that time (Z2-D-019, Z2-D-053): a requester whose result affects simulation (a script's `ctx:RequestLoad`, a streamed cell, a world-transition scene) names a `Zenith_AssetLoadCompletionQueue`, passes its completion ID as `m_ulUserTag`, drains the queue into its world's pending-result set, and publishes at the next tick boundary in completion-ID order (`Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"`).
- What a raw asset-load requester publishes, by completion status:

| Status | Publication |
|---|---|
| `SUCCEEDED` | `PUBLICATION_KIND_GENERIC_RECOMPUTED`, no payload: the replay re-runs the load and holds the tick until it is ready |
| `FAILED` (every failure class) | `PUBLICATION_KIND_GENERIC_RECORDED` with the 24-byte `Zenith_AssetLoadResultRecord` (Formats.md "Load result payload"): Scripting's I/O row, extended to every class so a replay reproduces a failure even when the content has since been repaired |
| `CANCELLED` | never published. A cancellation is the requester's own decision (a script's cancel, STREAM's shedding, a world teardown, shutdown); the requester discards the record at drain. A requester that cancelled a load whose result simulation still awaits re-issues the request under the same completion ID, so the published result is the re-issued load's |

- SCENE's scene loads are not raw asset loads: SCENE publishes its own preparation operation's result under its own source kind and payload (Formats.md "Load result payload", last paragraph).
- `Zenith_AssetRegistry::GetState` and `Resolve` are live and are called by presentation (Flux, audio, UI), tools and requesters' publication steps, never by a simulation phase node; the Luau `Asset:IsLoaded()` binding reads the world's published result, not `GetState` (a cross-area note to SCRIPT).
- On replay, a recorded publication is applied at its recorded tick: when the load has not completed by then, the requester's replay path waits for it at that tick boundary (`ServiceUntilComplete` is not used for this: the requester holds the tick until its completion record arrives); a recorded failure is replayed as a failure.
- Results for a world swapped out are discarded by the requester ([Z2-N-ARCH-558]); the load itself continues or stops by its handles like any other.

## Residency budgets and eviction

### Resident byte accounting

- Every kind loader reports each object's resident host and device bytes through `SetResidentBytes(subsystem, host, device)`; ZenithAssets keeps them per slot and sums them per memory subsystem (atomics), shown as `asset.resident.<subsystem>.*`.
- The registry's `Zenith_MemoryConsumer` reports `MEMORY_SUBSYSTEM_ASSETS` = ZenithAssets' own bytes (slot array, catalogue snapshots, pools, staging, decode scratch, the dump arena) + the host bytes of objects whose kind's subsystem is `ASSETS`. Bytes of other subsystems' kinds are reported by those subsystems' own consumers (Flux for rendering, audio for audio); ZenithAssets' per-subsystem sums are diagnostics and never reported twice ([Z2-N-ARCH-823]).
- Staging, decode buffers and scratch are allocated once at `Create` (their sizes are rows), so the load path allocates nothing beyond pooled records.

### The keep-alive cache

- An ACTIVE asset of a cacheable kind whose references reach zero enters the cache (state CACHED), ordered by the frame of its last release, unless its GPU residency is LOST or FAILED: such an object unloads (ACTIVE × E8 of "Asset state machine"), so a subsequent acquire loads it again rather than reviving an object no recovery is queued for; a cached asset's bytes count as cache bytes.
- The cache holds at most `asset.cache.maxBytes` of host + device bytes; inserting beyond it evicts from the LRU head until it fits (eviction → UNLOADING → `Unload`).
- `Acquire` of a CACHED asset revives it to ACTIVE with no I/O.
- Non-cacheable kinds (scenes, terrain, navmeshes, cinematics) unload at zero references immediately.
- The cache is never simulation-visible: a revived asset is published to simulation only through its requesters like any load.

### In-flight load budget

A pipeline in sub-stage ADMISSION — a load, a development reload or a device recovery, all alike — is admitted to its load record (E3) when `loadsInFlight < asset.load.maxInFlight` and `inFlightBytes + entry.m_ulFileBytes ≤ asset.load.maxInFlightBytes`, with the priority reserves of "Priority reserves" applied to both limits; a single file larger than the byte limit is admitted alone when nothing else is in flight. A kind whose loader desc sets `m_uMaxConcurrentLoads` ("Zenith_AssetKindLoaderDesc" in RuntimeAPI.md) is admitted only while fewer of its pipelines hold a load record, RETIRING records included: a pipeline of a kind at its limit is skipped for that slice and keeps its FIFO position, and admission continues with the next pipeline of the same FIFO, so one kind at its limit never holds back another kind, pipelines of one kind keep their relative order, and the walk is bounded by the slice's 2 ms budget. A RETIRING record counts as one load in flight and no bytes. Pipelines in CATALOGUE, DEPENDENCIES and WAIT_LOADER count in neither limit and hold no record. The load-record pool holds exactly `asset.load.maxInFlight` records, so admission never finds it empty, and the acknowledgement queue's capacity follows from it ("Internal queues"). The limit bounds how much residency is being built at once; it is not a memory-pressure response.

A device loss can queue more recovery pipelines than `asset.load.maxInFlight` (every ACTIVE asset of a GPU-resident kind, thousands against a limit of tens): the surplus waits in ADMISSION, holding only its side record, and is admitted as records return — pinned assets first at CRITICAL, then the rest at HIGH in (kind code, GUID) order, which the request sequences issued at E12 encode ("Device-loss reload"). No pool, queue or operation table grows, and `ReloadGpuResidencySet`'s operation completes when its count of "Device-loss reload" step 3 reaches zero (`Test_AssetLoader_DeviceRecoveryReloadsGpuKinds` with 64 GPU assets and `asset.load.maxInFlight` = 4).

### Memory pressure responses

`Zenith_AssetRegistry`'s consumer implements exactly the Assets row of `Zenith2/Docs/Architecture.md → "Memory governor"` (table "Shedding per subsystem"), adding only mechanism ([Z2-N-ARCH-822]):

| Level | Architecture's action | Mechanism in ZenithAssets | Counter |
|---|---|---|---|
| WARNING | release zero-reference assets pending deferred deletion immediately | the keep-alive cache is flushed (every CACHED asset unloaded) and admission to the cache is suspended; zero-reference assets unload at once | `asset.cache.flushes` |
| CRITICAL | also drop decoded CPU copies of GPU-resident assets | in addition, `DropCpuCopies()` is called once on every registered loader of a kind in `GetGpuResidentKinds()`, which releases the CPU copies of its RESIDENT and FAILED objects (PENDING objects keep the data their uploads read) | `asset.cpu_copies_dropped_bytes` |

- The response is applied within one frame of the level change (the consumer's notification arrives in the frame prologue on the main thread; it sets a flag and kicks the service, which flushes in its next slices) and reverted one frame after the level falls ([Z2-N-ARCH-821]): the cache accepts entries again; CPU copies are re-created only by a following load of that asset, never eagerly.
- Nothing else changes with the level: loads are admitted, prioritised and decoded exactly as at NORMAL, so no simulation-visible outcome depends on memory pressure ([Z2-N-ARCH-812]); STREAM's own row (cancelling background requests) reaches ZenithAssets as ordinary cancellations.

## Unload and reload

### Unload

1. The service moves the slot to UNLOADING and calls `Unload(context)` with no lock held.
2. The loader releases the object (Flux: `Flux_PersistentResources::Release` for every resource, whose destruction is deferred until every queue has completed the frames that used it, `Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`, [Z2-N-CONTRACT-083]), then `AcknowledgeUnload`.
3. ZenithAssets sets the resident bytes to zero, releases the object's dependency block (which may unload the dependencies in turn), and frees the slot when no reference remains ("Reference counting and retirement" in RuntimeAPI.md).

### Development reload

`RequestReload(handle)` (development builds only; Shipping returns `NOT_ALLOWED`) is what the editor calls after the cook server's `reimport` result or a `published` notification of its file watching (`Zenith2/Docs/Design/ASSET/Formats.md → "Notifications"`):

1. The editor first calls `OnMountsChanged` when the result or notification says `catalogueChanged`, so the refreshed snapshot is published at the next `AdvanceFrame` and every reload reads the new entry.
2. It then calls `RequestReload` for each changed GUID; per slot state the event is E11 of "Asset state machine", and on an ACTIVE slot the reload pipeline of "Reload and recovery pipelines" runs: the `TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY` reservation for the entry's resident estimate ([Z2-N-ARCH-815]), a second pipeline that waits for admission like a load and runs with `m_bReload`, the swap to `pNew` at activation, the old object kept `asset.reload.retireFrames` frames (so `Resolve` pointers of the current frame stay valid) and then unloaded, the reservation released after that unload.
3. A failed reload keeps the old object ACTIVE and reports the failure on the reload's operation.

## Missing and corrupt assets

### Failure classification

| Situation | Where detected | Result |
|---|---|---|
| GUID or logical path absent from the merged catalogue | `Acquire` | `NOT_FOUND`; `AcquireForDeserialization` gives a null handle, counts `asset.missing` every time and logs one `asset.missing` record per GUID while its 1,024-GUID log set has room, then at most one record per frame (RuntimeAPI.md "Reference counting and retirement") ([Z2-N-CONTRACT-086], [Z2-N-CONTRACT-342]) |
| entry present, file absent | header read | `NOT_INSTALLED` |
| file present, unreadable | any read | `IO` with the I/O code |
| magic, header check, size, table, bounds, section check, descriptor, GUID, kind, content hash | parse, verify, decode | `CORRUPT` with the `Zenith_AssetCorruption` detail |
| format or engine cook version | parse | `VERSION_MISMATCH` ([Z2-N-CONTRACT-528]) |
| platform | parse | `PLATFORM_MISMATCH` |
| a hard dependency failed | DEPENDENCIES | `DEPENDENCY_FAILED` (unless substituted) |
| the loader rejected the content | the loader | `LOADER_FAILED` with its code |

Every failure is a recoverable `Zenith_Result` or operation status; no byte of any file reaches an assert ([Z2-N-CONTRACT-472]). Each failure logs one `asset.load` error record (GUID, logical path, stage, code, detail, section) and counts `asset.failures.<code>`.

### Fallback policy per kind

- The policy of a kind is its row's column (`GetKindInfo(kind)->m_eDefaultFallbackPolicy`: the kind table, where an owner changes it by editing the table in its design, entry document "Asset kinds", or a test's declared row): `ResolveOrFallback<T>(h)` returns the object when ACTIVE or CACHED and its GPU residency is not FAILED; otherwise, for SUBSTITUTE, the kind's role-0 pinned asset's object (counted `asset.fallbacks.substituted`); for NULL_OBJECT and FAIL, null. A null handle resolves through the same rule (`GetFallback<T>`).
- Fallbacks are engine content the cook puts into every game's catalogue: each ASSET-phase cook step declares its kind's engine fallbacks by logical path and role (`Zenith2/Docs/Design/ASSET/Cook.md → "Engine fallbacks"`), and the base catalogue's fallback table lists them (`Zenith2/Docs/Design/ASSET/Formats.md → "Fallback table"`). The runtime never compiles a fallback GUID in: `RegisterKindLoader` reads the kind's rows, refuses a SUBSTITUTE kind with no role-0 row (`NO_FALLBACK`), pins every row (one reference owned by the registry until `BeginShutdown`) and requests each at CRITICAL at once; the kind's owner reaches rows other than role 0 through `GetPinnedAsset(kind, role)` (GEOM's per-usage fallback textures).
- Every pinned asset is ACTIVE before startup step 17 begins: after `CloseRegistration` the composition root services `GetPinnedLoads()` with `ServiceUntilComplete`, and a pinned asset that fails stops startup with exit code 3 (a broken install), naming the kind, the role and the error ("Tool and boot servicing"). A pinned GPU-kind asset is ACTIVE with residency PENDING at that point and becomes RESIDENT during the first frames; until then its object reports itself not resident, which nothing can observe because no frame has rendered. A pinned asset whose residency becomes FAILED is not a startup failure (the process is already running frames): it is logged as an error like any residency failure, and its consumers bind nothing for it.
- In headless runs the pinned assets of GPU kinds are validate-only loads (null objects); `ResolveOrFallback` then returns null for them, which headless consumers accept (no renderer exists).

### Retry after a mount change

After a published catalogue swap ("GUID index and catalogue snapshots" in RuntimeAPI.md), events E13 to E15 of "Asset state machine" apply to every slot: every FAILED slot whose failure is `NOT_INSTALLED` or `NOT_FOUND` is reset to UNLOADED, and every FAILED slot whose entry changed (a patch) likewise, so the next request loads it; failures of other classes stay until `RequestReload` or process restart. A swap is published only by `OnMountsChanged`, which the composition root calls after every `AddSource` or `RemoveSource` (an installed asset pack, a patch) and the editor after a cook that changed the catalogue.

## Device-loss reload

`Zenith2/Docs/Rendering/GpuResources.md → "Device loss"` step 4 calls `ReloadGpuResidencySet()`, event E12 of "Asset state machine" for every slot of a `m_bGpuResident` kind:

1. The service lists the ACTIVE slots of those kinds (`GetGpuResidentKinds()`), pinned assets included, ordered by (pinned first, kind code, GUID); each one's residency becomes LOST (writer W5) and, for a slot with no second pipeline, a device-recovery pipeline is queued in that order in sub-stage ADMISSION ("Reload and recovery pipelines"), HIGH priority (pinned assets CRITICAL). A slot whose reload or recovery is reserving, queued or running takes that pipeline's E12 row instead: a queued recovery stays queued, a running one restarts with a new attempt, and a reload builds its object on the new device and owes a recovery if it fails. Recoveries are admitted within `asset.load.maxInFlight` and the byte limit like any load ("In-flight load budget"); an admitted one has `m_bDeviceRecovery` and `m_pExistingObject` set, the header and the sections the loader requests are read again, and the loader recreates the GPU side of the existing object in place, activating it again with its existing pointer once its uploads are requested from its owner; the slot stays ACTIVE throughout.
2. CACHED slots of those kinds are evicted instead (E12 of CACHED); LOADING records whose loader has been called restart at HEADER_READ with a new attempt (E12 of LOADING); a running reload or recovery restarts likewise.
3. The returned operation counts every slot marked counted (side-record bit 3): each recovery queued or running, and each reload that owes one. A slot's count ends when its recovery activates, fails or is cancelled (E8, E13, E14, E16, E17), or when its owing reload activates, the new object needing no recovery. The operation completes when the count reaches zero: `SUCCEEDED` when no recovery failed, `FAILED` with the first error otherwise, so step 6 of the recovery policy can decide, and `CANCELLED` when E16 ended it; a cancelled recovery counts as ended and records no error. A failure leaves that object LOST (not resident; its consumers bind fallbacks) and is logged. Residency returns to RESIDENT as each loader reports its uploads complete during the following frames.
4. A second `ReloadGpuResidencySet` while an earlier operation is incomplete (a subsequent device loss: the recovery policy exits on a second loss within 60 s of the first, `Zenith2/Docs/Rendering/GpuResources.md → "Recover-or-clean-exit policy"`) completes the earlier operation `CANCELLED` at once and returns a new operation whose count takes over every slot still counted and adds the newly queued ones, so no slot is counted twice and no operation waits for ever.
5. CPU kinds are untouched.

## Headless loading

In the headless configuration (`m_bHeadless`), no GPU-kind loader exists (Flux is not created). `Zenith_AssetRegistry::Create` registers the built-in validate-only loader for every kind of `GetGpuResidentKinds()` (the table's GPU-resident kinds and any declared GPU-resident test kind):

```cpp
// Zenith2/Engine/ZenithAssets/Internal/Loading/Zenith_HeadlessKindLoader.h
/// @contract type Zenith_HeadlessKindLoader
/// Ownership: One instance owned by the registry's state, created by Create when m_bHeadless is set.
/// Thread affinity: ANY_THREAD, as Zenith_AssetKindLoader.
/// Invalidation: None.
/// Units: None.
/// Errors: None; it never fails a load (the header was validated before BeginLoad).
/// Reentrancy: NONE: its members call only their context.
/// Version: since Z2 Stage 2. Internal.
/// @endcontract
class Zenith_HeadlessKindLoader final : public Zenith_AssetKindLoader
{
public:
	void BeginLoad(Zenith_AssetLoadContext& xContext) override;          // SetResidentBytes(…, 0, 0); Activate(nullptr)
	void OnSectionsLoaded(Zenith_AssetLoadContext& xContext, Zenith_Span<const Zenith_AssetLoadedSection> axSections) override;   // unreachable: requests none
	void Cancel(Zenith_AssetLoadContext& xContext) override;             // AcknowledgeCancel()
	void Unload(Zenith_AssetUnloadContext& xContext) override;           // AcknowledgeUnload()
	uint64 DropCpuCopies() override;                      // returns 0
};
```

It requests no section, validates nothing beyond the header parse ZenithAssets already did, and activates with a null object and residency NONE. So a headless run requests, dedups, cancels, fails on missing or corrupt headers and publishes exactly as a windowed run does, and simulation is unchanged ([Z2-N-ARCH-868]); `Resolve` of such an asset returns `NO_RUNTIME_OBJECT`.

## Shutdown sequence

1. Shutdown step 4: `BeginShutdown` (event E16) refuses new requests and reads (`SHUTTING_DOWN`), requests cancellation of every live request, reload, recovery and read, unloads every RETIRING old object at once, releases the registry's pinned references, flushes the cache, and kicks the service; the job system's `CancelAndDrainBackground` runs service and decode slices until every operation has completed (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"`).
2. Shutdown step 5 retires every deferred deletion the loaders queued.
3. Shutdown step 6 destroys worlds (dropping their handles) and module subsystems. Before a kind loader's owner destroys the loader it calls `Zenith_AssetRegistry::UnregisterKindLoader(kind)`: every ACTIVE and CACHED asset of the kind is unloaded through it, the call services until each is acknowledged, and the kind becomes unserved (Flux does this before the RHI device is destroyed at step 9).
4. Shutdown step 10 destroys the registry: `ZENITH_CHECK` that no load or read is in flight; every slot still referenced is logged as a leak with its GUID, logical path and count ([Z2-N-CONTRACT-085]); the tests' leak check fails on any.

## Complexity

| Operation | Cost |
|---|---|
| handle copy, destroy | one atomic |
| `Acquire` | O(1) expected hash lookup under lock 200, plus O(log n) catalogue search on a miss |
| `Resolve` | O(1), no lock |
| `RequestLoad` | O(1) plus one queue push |
| a service slice | O(records advanced + completions drained), capped by 2 ms |
| catalogue `Find` | O(log n) binary search over the GUID-sorted table; `FindByLogicalPath` O(log n) over the path index |
| catalogue merge | O(total entries × log fragments) |
