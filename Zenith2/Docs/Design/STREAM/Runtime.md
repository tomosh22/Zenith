# STREAM design — runtime algorithms and state machines

This file is part of the STREAM design family; the entry document is `Zenith2/Docs/Design/STREAM.md`. It specifies every algorithm STREAM runs: binding a world, the per-tick update, bands and the desired set, load ordering and admission, the cell state machine, action buffers, cancellation at every stage, references, persistence, regions, large coordinates, teleports, the presentation-side service, HLOD, PSO groups, memory pressure, save and restore, world transitions, replay and headless behaviour. Every number below is a configuration key of `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Internal declarations" or a constant of "Zenith_StreamingTypes header", named where it is used.

## Scope of this file

| Need | Heading |
|---|---|
| which world a frame streams | "World binding" |
| the 15 steps of `Streaming.Update` and their node declaration | "Per-tick update" |
| distances, bands, the forward bias | "Distance and band computation" |
| which cells are wanted, dependency closure | "Desired-set algorithm" |
| which wanted cells load and activate first, and how many | "Load ordering and admission" |
| the cell states and every transition | "Cell state machine" |
| how a refused command buffer is detected and reverted | "Action buffers and rejection" |
| cancelling at I/O, decompression, upload, preparation, queue and activation | "Cancellation at every stage", "Ownership of partial results", "Completion reporting" |
| references across cells | "Cross-cell references" |
| state that survives unload | "Persistent entities" |
| physics, navigation and audio | "Region states and coordinated subsystems" |
| absolute positions, the origin, rebases | "Large coordinates and origin rebasing" |
| teleports, readiness, loading screens | "Teleports and readiness" |
| the epilogue stage | "The streaming service" |
| HLOD | "HLOD selection and extraction" |
| PSO groups | "PSO groups and the preparation gate" |
| shedding and peaks | "Memory pressure and transition peaks" |
| saves | "Save and restore" |
| transitions and shutdown | "World transitions and shutdown" |
| determinism | "Replay and determinism" |
| runs without a device | "Headless runs" |
| bounds | "Complexity" |

## World binding

A world is streamed when it holds exactly one enabled `Zenith.Streaming.WorldRef` on scene content. Step 2 of "Per-tick update" evaluates the binding every tick:

1. Collect the entities with `Zenith.Streaming.WorldRef` whose `Enabled` is true, in ascending simulation ID.
2. Refuse each that has no authored persistent ID (`STREAMING_ERROR_WORLDREF_RUNTIME`): the world asset must be a hard dependency of a loaded scene, which only scene content guarantees. Refuse all but the first remaining one (`STREAMING_ERROR_WORLD_DUPLICATE`). Each refusal is logged once per (entity, code) under `stream.world`.
3. Resolve the first one's handle with `Zenith_AssetRegistry::Resolve`. Because the reference is a scene property of asset kind `Streaming.World`, it is a hard dependency of its scene (`Zenith2/Docs/Design/ASSET/Loading.md` → "Dependency loading"): the asset is ACTIVE before the scene activated, with fallback policy FAIL, so `Resolve` cannot fail for scene content. A failure (`NOT_ACTIVE`) is therefore a defect: binding FAILED with `STREAMING_ERROR_WORLD_NOT_RESIDENT`, logged, and the world is not streamed. The binding never reads asset state for any other purpose, so it reads no asynchronous state.
4. Binding state transitions:

| From | Condition | To | Actions |
|---|---|---|---|
| NONE | a chosen reference resolves | BOUND | size the per-cell arrays and the per-persistent-entry arrays to the world's counts (`Zenith_Array::TryResize`; a failure is FAILED with `STREAMING_ERROR_OUT_OF_MEMORY`), set every cell UNLOADED, size the presentation state's HLOD array, run the two binding checks below and rebuild step R5's marking pass ("Cell identity across re-cooks"), log the binding |
| BOUND | the chosen reference is the same entity with the same asset | BOUND | nothing |
| BOUND | no reference chosen, or a different entity or asset | UNBINDING | every cell not UNLOADED, FAILED or FAILED_PERMANENT gets the unload of "Cell state machine" (paced as any unload); captures run as for any unload |
| UNBINDING | every cell UNLOADED, FAILED or FAILED_PERMANENT | NONE, then the new binding at once if one is chosen | release the world handle, clear the arrays |
| any | a chosen reference is refused | REFUSED (that reference) | the previous binding, if BOUND, unbinds as above |

`Zenith_StreamingWorldState::m_eBinding` holds the state as `0 NONE`, `1 BOUND`, `2 REFUSED`, `3 FAILED`, `4 UNBINDING`. The world reference being bound or unbound carries the same value in its saved `BindingState` (`Zenith.Streaming.WorldRef` property 4), written directly by step 2 whenever it changes (a reference that stops being the binding gets 0), so a restore resumes an UNBINDING world unbinding ("Rebuild" R1). A rebinding to the same world (T22) writes the holder of every FAILED_PERMANENT cell it resets in the same step.

The two binding checks, each a log line under `stream.world` and a GAUGE counter, never a refusal (the world streams either way):

1. **Live-instance bound.** When the world record's worst-case live instances ("Worst-case active cells" in Formats.md, the cook's bound for one source at radius scale 1) exceeds `stream.maxLiveInstances`, `stream.world.bound_exceeds_cap` is 1 and the line names both numbers: the configured cap is below what one source can keep active, so the live-cap preemption of "Loads (step 9)" will run in steady state.
2. **No-rebase precision.** When `stream.rebase.enabled` is false and any cell's farthest XZ corner lies more than `uSTREAMING_PRECISION_RADIUS_METRES` (4,096 m) from the current origin O on an axis, `stream.rebase.precision_warning` is 1 and the line names the worst cell and its distance ("Precision target").

A rebinding to the same asset (a reference disabled and re-enabled) resets every FAILED_PERMANENT cell: it is the deterministic way to retry content after a fix, and the editor's reload re-enables the reference of a play world after a cook. Delta holder entities of the old binding stay (they name their world ID and their cell scene, "Cell identity across re-cooks") and are used again by a subsequent binding of the same world.

## Per-tick update

### Update node declaration

```cpp
// Internal/Zenith_StreamingModule.cpp, AttachWorld step (5)
Zenith_SystemDesc xUpdate;
xUpdate.m_szName = "Streaming.Update";
xUpdate.m_ePhase = PHASE_INPUT;
xUpdate.m_eAffinity = JOB_AFFINITY_ANY;
xUpdate.m_pfnRun = &Zenith_StreamingUpdate_Run;
xUpdate.m_pUserData = pxWorldState;
xContext.GetSystems().AddSystem(xUpdate)
	.Before("Scene.ActivateAdditive")
	.EmitsCommands()
	.Read(Zenith_StreamingWorldRefComponent::xTYPE_ID, axWORLDREF_INPUT_PROPERTIES)   // 1–3 (3: the override slots)
	.Write(Zenith_StreamingWorldRefComponent::xTYPE_ID, axWORLDREF_OUTPUT_PROPERTIES) // 4 BindingState
	.Read(Zenith_StreamingSourceComponent::xTYPE_ID, axSOURCE_INPUT_PROPERTIES)        // 1–6
	.Write(Zenith_StreamingSourceComponent::xTYPE_ID, axSOURCE_OUTPUT_PROPERTIES)      // 7–14
	.Read(Zenith_StreamingRegionBoundComponent::xTYPE_ID, axREGION_INPUT_PROPERTIES)   // 1
	.Write(Zenith_StreamingRegionBoundComponent::xTYPE_ID, axREGION_OUTPUT_PROPERTIES) // 2–4
	.Read(Zenith_StreamingCellDeltaComponent::xTYPE_ID, axALL)                         // holders: the key and the delta
	.Write(Zenith_StreamingCellStatusComponent::xTYPE_ID, axALL)                       // holders: the persisted cell status
	.ReadAnyRow(xWORLD_TRANSFORM_TYPE_ID, axPOSITION_AND_ROTATION)                     // sources, region-bound and persistent entities
	.ReadAnyRow(Zenith_StreamingPersistentComponent::xTYPE_ID, axALL)
	.ReadAnyRow(Zenith_StreamingCellAnchorComponent::xTYPE_ID, axALL)
	.ReadAnyRow(<each type of stream.persistence.componentTypes>, <its eligible properties>)
	.ReadResource(xSCENE_ACTIVATION_QUEUE_TYPE_ID)                                     // Zenith.SceneActivationQueue
	.ReadResource(xWORLD_ORIGIN_TYPE_ID)                                               // Zenith.WorldOrigin (row ST3)
	.WriteResource(Zenith_StreamingWorldState::xTYPE_ID)                               // Zenith.Streaming.State
	.Query(xSourceQuery, xSourceSlot)
	.Query(xRegionQuery, xRegionSlot)
	.Query(xWorldRefQuery, xWorldRefSlot)
	.Query(xHolderQuery, xHolderSlot);   // WITH CellDelta, CellStatus; QUERY_TERM_CHANGED on CellDelta property 1 for discovery
```

`Before("Scene.ActivateAdditive")` orders the update before SCENE's activation system in phase 1, so the hook's `AdmitActivation` and `GetRootTranslation` read the admission table this tick's update wrote, through the resource-version edge the declarations also produce (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Sync-point playback"). The node is a single job (no leaves): its work is bounded per "Complexity" and its inputs are mostly STREAM's own arrays. Every direct write to a particular holder, source, region-bound entity or world reference goes through `Zenith_SystemContext::GetChunkOfEntity(slot, handle, row)` of the declaring query (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"), so it is a phase-1 system write of a property whose authority is `PHASE_INPUT` ([Z2-N-ARCH-483]): it is never a command, so no merge can reject it.

### Steps

| # | Step | Writes |
|---|---|---|
| 1 | **Reset check.** When `GetPreviousReadVersion()` is 0 or `m_bNeedsRebuild` is set, run "Save and restore" step R1 to R6 and continue | world state |
| 2 | **Binding** ("World binding"). Unbound: write `Ready` false and `MissingRequiredCells` 0 on every source and `InActiveRegion` true with `EvaluatedTick` 0 on every region-bound entity (an unstreamed world restricts nothing, and gives no basis for a DESTROY transition), then stop | world state, source and region outputs |
| 3 | **Holders and inboxes**, in this order (each part in "Persisted cell status"): holder discovery steps 1 and 2 (map every new holder to its cell, writing the cell's record into it; resolve every cell whose holder spawn or holder prune was recorded last tick); then the commit-refused inbox (T3) and the discard inbox (T18), whose rows write the holders the first part has just mapped, so T3 of a cell's first load writes the holder its own load buffer spawned; then holder discovery step 3 (re-spawn a holder gameplay destroyed) | cells, holders |
| 4 | **Observation.** For every cell in REQUESTED, LOADING, QUEUED, ACTIVATING, ACTIVE, UNLOADING, CANCELLING or FAILED with a release pending, read its instance's state from `Zenith_AdditiveSceneSet::GetState` (and, for ACTIVE, its anchor's handle) and apply exactly one row of the observation table of "Cell state machine", in cell index order; then merge the pending tombstones ("Tombstones": every cell still ACTIVE with pending bits moves them into its committed bits; T26 and T27 have already discarded theirs) | cells, holders |
| 5 | **Sources.** Collect enabled sources (at most 16, ascending simulation ID; more are ignored and logged once per world); absolute position `P = O + (double)p` from `Zenith.WorldTransform` and `Zenith.WorldOrigin`; forward from the rotation's +Z axis projected to XZ and normalised (zero when its length is below 10^−3); teleport detection from the source's own persisted properties 10 to 14 ("Teleports and readiness"), then this tick's P written into properties 10 to 13 and property 14 cleared | source records, source properties 10–14 |
| 6 | **Bands** ("Distance and band computation") for every (source, grid in its mask), then overrides | cell bands |
| 7 | **Closure** ("Desired-set algorithm") | cell bands, pins |
| 8 | **Unloads and cancellations** ("Load ordering and admission", unload half): each recorded in its own action buffer with its captures ("Persistent entities") | actions |
| 9 | **Loads**: candidates sorted and issued up to the caps; each in its own action buffer | actions, slots |
| 10 | **Admission**: QUEUED cells admitted up to the caps; the admission table built; ApplyCellState and BindReferences recorded per admitted cell | admission table, actions |
| 11 | **Regions** ("Region states and coordinated subsystems") | region outputs, actions for DESTROY |
| 12 | **Readiness** ("Teleports and readiness") | source outputs |
| 13 | **Rebase** ("Large coordinates and origin rebasing") | action |
| 14 | **Retries, re-homes and prunes**, in this order: FAILED cells whose retry tick passed return to UNLOADED (they become candidates at the next tick's step 9); the deferred re-homes of "Cell identity across re-cooks"; the holder prunes of "Persisted cell status" | cells, holders, actions |
| 15 | **Counters**: this tick's events added to the cumulative totals of `m_xCounters` (monotonic; the service differences them per frame, "The streaming service") and the gauges written; `m_ulLastUpdateTick` | world state |

Every step that changes a persisted field of a cell writes it to the cell's holder in the same step, directly ("Persisted cell status"); no step defers a persisted write to another step or tick.

Profiler sub-zones: `Streaming.Update.Observe` (steps 3–4), `.Bands` (5–7), `.Unload` (8), `.Load` (9), `.Admit` (10), `.Regions` (11), `.Persist` (the captures inside step 8, and the re-homes and prunes of step 14).

## Distance and band computation

<!-- elaboration -->
For a source at absolute point P with radius scale s (clamped to [0.25, 4]) and a cell of grid g with absolute bounds `[x0, x1) × [y0, y1) × [z0, z1)` (`x0 = originX + x · c`, `x1 = x0 + c`, likewise z; `y0 = baseY`, `y1 = baseY + height`), in binary64:

```text
dx = max(x0 − Px, 0, Px − x1)
dz = max(z0 − Pz, 0, Pz − z1)
dy = metric == XYZ ? max(y0 − Py, 0, Py − y1) : 0
d  = sqrt(dx² + dy² + dz²)
band = d ≤ Rreq·s           → REQUIRED
       d ≤ Ract·s           → ACTIVATION
       d ≤ (Ract + H)·s     → KEEP
       d ≤ Rpre·s           → PREFETCH
       otherwise            → NONE
```

The radii are the grid's binary32 values widened to binary64 before the product, so the comparison is exact and identical on every platform ([Z2-N-ARCH-884]; `Zenith2/Docs/Design/CORE/Maths.md` → "Numerical conventions"). A cell's band is the most urgent band any source gives it. The cell's ordering distance is the effective distance of its most urgent source:

```text
θ      = angle between the source's forward (XZ) and the direction from P to the cell centre (XZ)
d_eff  = d × (1 − β · max(0, cos θ))        β = stream.priority.forwardBias (0.25; 0 … 0.9)
qdist  = min(floor(d_eff × 16), 0xFFFFFFFF)  (1/16 m quantisation)
```

`cos θ` is computed from the normalised vectors as a dot product in binary64; a zero forward gives `cos θ = 0`. The bias only reorders loads and admissions (a cell ahead of the source loads first); it never changes a band, so turning on the spot never loads or unloads a cell.

**Enumeration.** For each (source, grid in its mask), the cells examined are those whose coordinate lies in the square `[floor((Px − R − originX) / c), floor((Px + R − originX) / c)]` × the same for z, with `R = Rpre · s`. When that square holds more than 4,096 coordinates, R is reduced to the largest prefetch half-width whose square holds at most 4,096 (never below `(Ract + H) · s`, whose square the cook bounds), and the clip is logged once per (world, grid). Each coordinate is looked up with `FindCell` (O(1)); absent coordinates cost one hash probe. Coordinates are visited z-major, x-minor, so the per-cell "most urgent source" ties resolve in ascending source order; the source with the highest `Priority` wins a tie of equal band and equal qdist, then the lowest simulation ID.

**Overrides.** A cell's override is read every tick in this step from the bound world reference's override slots (`Zenith.Streaming.WorldRef` property 3, "Overrides on the world reference" below), never from a STREAM-private copy, so it is saved with the reference and a restored run applies exactly the overrides the uninterrupted run applies. After the sources, a cell whose override is FORCE_ACTIVE gets band REQUIRED and qdist 0. A cell whose override is FORCE_UNLOADED gets NONE, and so does every transitive dependent of it: the grids are visited in reverse dependency order (`GetGridInDependencyOrder` from the last position to the first, so a dependency is visited before its dependents) and each cell with a forced-unloaded dependency is marked forced-unloaded too, whatever its own override. A forced-unloaded cell is never pinned by the closure ("Desired-set algorithm"), so its dependents unload first, at the paced rate, and the cell itself unloads once none of them is live. FORCE_ACTIVE on a cell that a FORCE_UNLOADED dependency forces out loses (the dependency would otherwise have to be active). Overrides are applied only when `stream.debug.allowOverrides` is true and the build is not Shipping; an override naming a coordinate with no cell is counted in `stream.overrides.unknown_cell` and ignored, a refused one in `stream.overrides.refused`.

### Overrides on the world reference

<!-- elaboration -->
`Zenith.Streaming.WorldRef` property 3 `Overrides` is a `FIXED_ARRAY` of `uSTREAMING_MAX_CELL_OVERRIDES` (64) `Zenith.Streaming.CellOverrideSlot` structs `{ uint8 m_uGrid; uint8 m_uOverride; uint16 m_uReserved; sint32 m_iX; sint32 m_iZ; }` (12 bytes each; `m_uOverride` 0 marks a free slot), authority `PHASE_COMMANDS_ONLY`, serialized and `PROPERTY_FLAG_EDITOR_HIDDEN`. Its only writer is the `Streaming.SetCellOverride` playback (step 7 of the sync point the command plays at), through its `Zenith_WorldPlaybackAccess` (`ReadProperty` of the array, the slot changed, `SetProperty` of the array, the fixed-array pattern of "Binding" in "Cross-cell references"), on the world reference the world state names as bound:

| The command names | The playback does |
|---|---|
| a coordinate a slot already holds, override NONE | frees that slot (`m_uOverride` 0, every other field 0) |
| a coordinate a slot already holds, another override | replaces that slot's override |
| a new coordinate, override NONE | nothing |
| a new coordinate, another override, a free slot exists | fills the lowest free slot |
| a new coordinate, no free slot | nothing; `stream.overrides.refused` + 1, logged once per world under `stream.world` |
| anything, while the world is not BOUND, or in a Shipping build, or with `stream.debug.allowOverrides` false | nothing; `stream.overrides.refused` + 1 (`STREAMING_ERROR_OVERRIDE_NOT_ALLOWED` logged) |

Step 6 reads the 64 slots of the bound reference (O(64)) and resolves each with `FindCell` (absent: `stream.overrides.unknown_cell` + 1, once per slot content). A playback write is applied at the sync point, is never a merge-rejectable record of its own (the command's buffer is the recorder's, accepted or rejected whole by the merge like any command), and lands in a saved property, so a capture taken after the playback holds the override and a capture taken before it does not, in both runs. Overrides stay on the reference across an unbinding and a rebinding to the same reference; a different reference carries its own slots.

## Desired-set algorithm

1. **Own bands** come from "Distance and band computation".
2. **Wanted.** A cell is wanted when its band is REQUIRED or ACTIVATION, or its band is KEEP and its state is one of REQUESTED, LOADING, QUEUED, ACTIVATING, ACTIVE (hysteresis: KEEP retains, never starts).
3. **Closure.** Grids are visited in the world's dependency order (dependents before their dependencies, `GetGridInDependencyOrder`); for each wanted cell of the grid, each dependency d is raised to `min(band(d), band(cell))` over the urgent bands REQUIRED and ACTIVATION (a KEEP cell pins its dependencies at KEEP), marked pinned, and counted in its dependents-wanting count. Because the visit order is topological, a dependency raised by a dependent is raised before its own dependencies are visited, so one pass closes the graph. Its ordering distance becomes the minimum of its own and the dependent's.
4. **Unwanted.** Every other cell is unwanted. An unwanted ACTIVE cell stays while any of its dependents is not UNLOADED (a dependency outlives its dependents: unloading collision under live gameplay content would drop bodies through the floor), and while its unload delay runs.
5. **Unload delay.** When an ACTIVE cell becomes unwanted, its unload due tick is set to the current tick plus its grid's `unloadDelayTicks`; it is cleared (0) when the cell is wanted again. The delay is running while the due tick is non-zero and greater than the current tick, and the unload is issued at the first tick at which it is not (a delay of 0 ticks issues at once). The due tick is an absolute tick, so it changes only when the delay starts, is cut ("Loads (step 9)" preemption (a) sets it to the current tick) or is cleared, and each change is written to the cell's holder in the step that makes it (`CellStatus` property 8, "Persisted cell status").

The closure is per tick, from scratch, and depends only on the sources, the overrides, the cell states, the persisted cell status and the cooked graph.

## Load ordering and admission

### Caps

| Quantity | Key or constant | Default | Counted as |
|---|---|---|---|
| cells in REQUESTED, LOADING or CANCELLING | `stream.maxConcurrentCellLoads` (Z2-B-STREAM-003) | 16 | in flight |
| cells in flight, plus cells QUEUED, plus UNLOADING cells whose recorded unload was of a QUEUED instance (flag "unload of a QUEUED instance") | `stream.maxConcurrentCellLoads` + `stream.maxQueuedCells` | 16 + 32 | pending |
| cells not UNLOADED, FAILED or FAILED_PERMANENT | `stream.maxLiveInstances` | 160 | live |
| load actions per tick | `stream.maxNewRequestsPerTick` | 4 | issued |
| unload and cancel actions per tick | `stream.unload.maxCellsPerTick` | 8 | unloads |
| admissions per tick | `stream.activation.maxCellsPerTick` | 4 | admitted |
| entities admitted per tick | `stream.activation.maxEntitiesPerTick` | 4,096 | admitted entities |

`stream.maxLiveInstances` is at most 192, which leaves 64 of SCENE's 256 additive instances (`scene.maxAdditiveInstances`) to the game's own additive loads; SCENE's refusal at the limit is still handled ("Cell state machine", commit refused). The pending bound exists for two reasons: it bounds the prepared-but-not-active scene data a world holds (a QUEUED cell waiting for its dependencies or for pacing keeps its whole prepared scene, Z2-B-STREAM-001), and it bounds the loads a save can hold in flight, which the snapshot pool must be able to re-adopt at restore ("Zenith_StreamingSnapshotPool" in PublicAPI.md).

**The pending bound, proved.** Write C = `stream.maxConcurrentCellLoads`, Q = `stream.maxQueuedCells` and `pending = inFlight + queued + unloadingFromQueued`, where inFlight counts REQUESTED, LOADING and CANCELLING cells, queued counts QUEUED cells and unloadingFromQueued counts UNLOADING cells whose flag "unload of a QUEUED instance" is set (T9 sets it; T16, T17, T29 and T34 clear it). The claim is that LOADING + CANCELLING + QUEUED ≤ pending ≤ C + Q at every phase 11 and at the start of every step 9, in every build. Proof by the transitions that change a counted state:

| Transition | Effect on pending |
|---|---|
| T1 (issue, step 9) | + 1, and step 9 issues only while `pending < C + Q` (its third condition), so pending ≤ C + Q after it |
| T2, T5, T7, T19, T24, T30 (REQUESTED → LOADING, LOADING → QUEUED, LOADING → CANCELLING, CANCELLING → LOADING, LOADING → CANCELLING, CANCELLING → QUEUED) | 0: both states are counted |
| T9 (QUEUED → UNLOADING, flag set) | 0: the cell moves from queued to unloadingFromQueued |
| T17 from a flagged UNLOADING cell (back to QUEUED, flag cleared) | 0 |
| T13 (ACTIVATING → QUEUED) | + 1, but it runs in step 4 and is preceded by T8 (QUEUED → ACTIVATING, − 1) in step 10 of the previous tick, and no step issues between them; so pending at the next step 9 equals pending at the step 9 that preceded that T8 |
| every other row out of a counted state (T3, T4, T6, T8, T10, T16 and T29 of a flagged cell, T18, T25, T31, T34) | − 1 |

A flagged UNLOADING cell cannot meet T17's ACTIVE branch: its instance was QUEUED when the unload was recorded, and a QUEUED instance becomes ACTIVE only through admission, which an UNLOADING cell never receives (T34 releases it if SCENE reports it ACTIVE anyway).

A rejected unload of a QUEUED cell (T17) therefore returns the cell to QUEUED without raising pending above the value it had when step 9 last issued, which is the hole an uncounted UNLOADING cell would open: step 8's T9 would free a unit, step 9 would issue into it in the same tick, and the rejection would bring the QUEUED cell back above the cap. Since every LOADING, CANCELLING or QUEUED cell is counted in pending, LOADING + CANCELLING + QUEUED ≤ C + Q ≤ `uSTREAMING_MAX_CONCURRENT_LOADS` + `uSTREAMING_MAX_QUEUED_CELLS` (48 + 64 = 112) whenever no restore intervened. The SCENE instances a phase-11 capture can hold LOADING or QUEUED come from cells in those three states (a REQUESTED cell's instance included), from a flagged UNLOADING cell whose unload was rejected (its instance still QUEUED) or a CANCELLING cell whose unload was rejected (its instance still LOADING), all counted, and from one uncounted kind: an ACTIVATING cell whose SCENE activation buffer was rejected (its instance still QUEUED, T13 at the next update). Such cells number at most the T8s of this tick's step 10, which ran after the step 9 that bounded pending and lowered pending by exactly that many, so SCENE's LOADING + QUEUED instances at phase 11 never exceed the pending value step 9 left, ≤ C + Q, and a save written by any build holds at most C + Q ≤ 112 tagged loads in flight.

**After a restore** the rebuild may adopt up to `uSTREAMING_SNAPSHOT_SLOTS` (112) in-flight loads ("Rebuild" R4), which can exceed the restoring build's own C or C + Q (a save written with C = 48 restored by a build with C = 16). The caps are then over-full: step 9 issues nothing while `inFlight ≥ C` or `pending ≥ C + Q` (its conditions are inequalities, never equalities), every wanted candidate is counted in `stream.cells.loads_deferred`, and issuing resumes once the adopted loads have published, activated, failed or been cancelled below both caps. Nothing else changes: an over-full count is never an error and is never corrected by cancelling an adopted load.

### Ordering key

Every list STREAM processes in priority order is sorted ascending by the 64-bit key (59 bits used)

```text
(band: 3 bits, 3 − sourcePriority: 2 bits, roleOrder: 3 bits, qdist: 32 bits, grid: 3 bits, cell index: 16 bits)
```

where sourcePriority is the most urgent source's `Priority` (0 … 3, so a higher priority sorts first) and roleOrder is the grid role's value (COLLISION 1 first, then NAVIGATION, GAMEPLAY, AUDIO, LARGE). The key is a total order (the cell index is last), so `Zenith_Sort` over the scratch arena gives the same order on every machine; stability is irrelevant under a total key.

### Unloads (step 8)

1. Candidates: unwanted cells in ACTIVE (delay 0, no non-UNLOADED dependent), QUEUED, LOADING, and REQUESTED cells are not candidates (their commit is pending this sync point; they are re-examined next tick as LOADING).
2. Sort descending by the key (the least urgent, farthest first); dependents sort before their dependencies automatically, because a dependency with a live dependent is not a candidate.
3. Issue up to `stream.unload.maxCellsPerTick`: ACTIVE and QUEUED → UNLOADING (an ACTIVE cell's flag "unload in progress" is set, "Tombstones"; a QUEUED cell's flag "unload of a QUEUED instance" is set, "Caps"), LOADING → CANCELLING, each with its action buffer ("Action buffers and rejection") and each new state written to the cell's holder in this step. Candidates beyond the cap wait; `stream.cells.unload_deferred` counts them.

### Loads (step 9)

1. Candidates: wanted cells in UNLOADED whose band is REQUIRED or ACTIVATION, whose unload or release was not recorded this tick (flag bit 1: the old instance's `Scene.ForgetInstance` plays at this same sync point, and a new load of the cell waits one tick so it can never meet the old instance at commit), and whose every dependency is wanted (closure guarantees it unless the dependency is FAILED_PERMANENT, in which case the dependent is skipped and counted in `stream.cells.blocked_by_dependency`). A dependency need not be ACTIVE to start loading its dependent: loads overlap; only activation waits.
2. Sort ascending by the key.
3. Issue while `issued < maxNewRequestsPerTick`, `inFlight < maxConcurrentCellLoads`, `inFlight + queued + unloadingFromQueued < maxConcurrentCellLoads + maxQueuedCells` (the pending bound of "Caps") and `live < maxLiveInstances`. When the live cap blocks a REQUIRED candidate, it preempts kept content, in this order, one cell per blocked REQUIRED candidate and tick: (a) the least urgent unwanted ACTIVE cell whose unload delay is still running has its delay cut to 0; (b) when there is none, the least urgent ACTIVE cell that is wanted only through the KEEP band (its own band KEEP, pinned by no dependent in REQUIRED or ACTIVATION, no dependent live) is marked preempted: it is treated as unwanted with delay 0 at the next tick's step 8 and its KEEP retention is suspended until it leaves the KEEP band. Each preemption counts `stream.cells.preempted`. Both are functions of simulation state only. Every candidate left is counted in `stream.cells.loads_deferred` (BR-DEFER of Z2-B-STREAM-003).
4. Per issued load: `Acquire` the cell scene handle (`Zenith_AssetRegistry::Acquire<Zenith_Scene>`, ANY_THREAD, never starts a load; `NOT_FOUND` or `KIND_MISMATCH` makes the cell FAILED_PERMANENT at once with `STREAMING_ERROR_CELL_LOAD_FAILED` and the ASSET code as detail, a catalogue fact that is the same in a replay; the status is written to the cell's holder, or, when the cell has none, carried by a status-holder action, "Persisted cell status"); compute the tag `Zenith_MakeStreamingHookTag(cell, tick)`; acquire a snapshot slot with the cell's entity table ("Component presence": tombstone bits and presence masks of its persistent entries, built from the cell's delta and its committed tombstone bits), table filled, and the gate open when no renderer exists ("PSO groups and the preparation gate"); record the load action, which begins with the `Spawn` of the cell's holder when the cell has none ("Action buffers and rejection"). The cell becomes REQUESTED, written to its holder in this step when it has one (a new holder is spawned with State REQUESTED).

### Admission (step 10)

1. Candidates: QUEUED cells that are wanted and whose every dependency is ACTIVE (a dependency that is ACTIVATING this tick does not count: its entities exist only after the sync point).
2. Sort ascending by the key.
3. Admit while `admitted < stream.activation.maxCellsPerTick` and `admittedEntities + entityCount(cell) ≤ stream.activation.maxEntitiesPerTick`, except that the first candidate of a tick is admitted whatever its entity count (a cell above the cap would otherwise never activate). The cook bounds a cell's entity count by the smaller of 16,384 and the game's configured `ecs.maxSpawnsPerTick` (the value of Z2-B-ECS-003 once allocated; Formats.md V16), so the first candidate always passes SCENE's own spawn-count check and a cooked cell can never fail `SCENE_ERROR_ACTIVATION_TOO_LARGE` for its entity count. Each refusal counts `stream.cells.activation_deferred`.
4. For each admitted cell: an admission entry (instance ID, cell, root translation, entity count), the state ACTIVATING (written to its holder with the instance, flags "state applied", "references bound" and "unload in progress" cleared, and the committed and pending tombstone bits of every persistent entry of the cell cleared, in the mirror and in `CellStatus` properties 9 and 10, before any entity of the activation exists, "Tombstones"), and one action buffer holding `Streaming.ApplyCellState` then `Streaming.BindReferences`. The table is sorted by instance ID for the hook.
5. Not admitted QUEUED cells stay QUEUED; SCENE does not count a refused admission against `scene.activationRetryTicks` (row ST2).

## Cell state machine

<!-- elaboration -->
The ten-state machine, its observation-and-revert discipline and CANCELLING held until the discarded publication are design choices of this family ("Design choices at a glance" in the entry document). Each transition is a pure function of the cell's record, the instance state SCENE reports and this tick's decisions. Every row that changes a persisted field (the state, the instance, a failure counter, a tick, a flag of "Persisted cell status") writes it to the cell's holder in the step that applies the row, so the holder equals the cell record at the end of every step, except for the three hook-driven changes that land in an inbox (T2's instance, T3's refusal, T18's discard; "What is saved").

### States

| State | Meaning | Holds |
|---|---|---|
| UNLOADED | no SCENE instance | nothing (tombstones and delta persist in components) |
| REQUESTED | a load action was recorded this tick; its commit plays at this sync point | the cell scene handle (in the action), a slot |
| LOADING | SCENE instance LOADING: committed; preparation pending or running | slot (both bits until the preparation ends) |
| QUEUED | SCENE instance QUEUED: prepared, published, waiting for admission | nothing of STREAM's once observed (the simulation bit is cleared at T5) |
| ACTIVATING | admitted this tick | the admission entry |
| ACTIVE | SCENE instance ACTIVE: the entities exist | the anchor handle, the persistent handle table entries, bound references |
| UNLOADING | the unload of an ACTIVE or QUEUED instance recorded | — |
| CANCELLING | the unload of a LOADING instance recorded, or an external unload of a LOADING instance observed; the discarded publication is awaited | slot until its preparation ends |
| FAILED | a published failure or a commit refusal; a retry is scheduled | the retry tick; "release pending" while SCENE's FAILED record is still live |
| FAILED_PERMANENT | `stream.cell.maxRetries` exhausted, a catalogue error or a permanent code | nothing; cleared by a rebinding |

### Transitions

Event rows are taken in the step that produces their event (steps 3, 8, 9, 10, 14, the binding and the sync point); observation rows in step 4, one per cell, chosen by the matrix of "Totality". "obs" is the instance state `GetState` reports in step 4; "gone" is UNLOADED or not found (`GetError` is `SCENE_ERROR_INSTANCE_NOT_FOUND` once SCENE's tick-boundary handler has released the record).

| # | From | Event | To | Actions |
|---|---|---|---|---|
| T1 | UNLOADED | issued in step 9 | REQUESTED | handle, tag, slot, load action |
| T2 | REQUESTED | `OnLoadCommitted` with an instance (this sync point) | LOADING | instance ID recorded |
| T3 | REQUESTED | `OnLoadCommitted` refused (commit-refused inbox, step 3) | FAILED | both slot bits cleared; `stream.cells.commit_refused`; `STREAMING_ERROR_CELL_COMMIT_REFUSED` (LIMIT) or `_CELL_DUPLICATE_INSTANCE`; retry scheduled by "Failure classification" |
| T4 | REQUESTED | still REQUESTED in step 4 and absent from the commit-refused inbox drained in step 3: the load buffer was rejected at merge, so neither `OnLoadCommitted` call happened | UNLOADED | both slot bits cleared (no preparation exists); handle dropped; `stream.actions.rejected` |
| T5 | LOADING | obs QUEUED | QUEUED | simulation slot bit cleared (the preparation has ended: SCENE parks after `OnPreparationEnded`) |
| T6 | LOADING | obs FAILED | FAILED | simulation slot bit cleared; the release recorded (`Zenith_RecordUnloadAdditiveScene` of the FAILED instance, whose `Scene.ForgetInstance` releases SCENE's record) and "release pending" set; `stream.cells.failed`; retry by "Failure classification" |
| T7 | LOADING | unwanted, issued in step 8 | CANCELLING | unload action |
| T8 | QUEUED | admitted in step 10 | ACTIVATING | admission entry, ApplyCellState and BindReferences action; flag "unload in progress" and the cell's committed and pending tombstone bits cleared ("Tombstones") |
| T9 | QUEUED | unwanted, issued in step 8 | UNLOADING | unload action; flag "unload of a QUEUED instance" set ("Caps") |
| T10 | QUEUED | obs FAILED (activation retries exhausted or too large) | FAILED | as T6 |
| T11 | ACTIVATING | obs ACTIVE and the cell's ApplyCellState played ("state applied" set by the playback) | ACTIVE | anchor handle recorded; retries used, consecutive capacity failures and consecutive transient failures reset to 0; `stream.cells.activated` |
| T12 | ACTIVATING | obs ACTIVE, ApplyCellState not played (its buffer was rejected) | ACTIVE | ApplyCellState and BindReferences recorded again at once (phase 1 of this tick: entities exist); the late ApplyCellState overwrites the restored properties, including any value gameplay wrote to them in phases 2 to 10 of the activation tick, and leaves the tombstone bits alone ("Restore"); the three failure counters reset as in T11; `stream.cells.late_state_apply` |
| T13 | ACTIVATING | obs QUEUED (SCENE's activation buffer rejected by capacity) | QUEUED | `stream.cells.activation_rejected`; re-admitted in step 10 of this tick when it qualifies |
| T14 | ACTIVATING | obs FAILED | FAILED | as T6 |
| T15 | ACTIVE | unwanted with delay 0, or preempted, issued in step 8 | UNLOADING | captures, unbind, unload action; flag "unload in progress" set (the only row that sets it) |
| T16 | UNLOADING | obs gone | UNLOADED | persistent handles and the anchor handle cleared; flags "unload of a QUEUED instance" and "unload in progress" cleared (the latter a backstop: the unload's `UnbindReferences` playback normally cleared it); `stream.cells.released` |
| T17 | UNLOADING | obs ACTIVE or QUEUED (the unload buffer was rejected) | ACTIVE or QUEUED | the capture of the rejected buffer is discarded (its delta write never played); flags "unload in progress" and "unload of a QUEUED instance" cleared; `stream.actions.rejected` |
| T18 | CANCELLING | the discard inbox names the cell (SCENE published the discarded load and called `OnDiscardedPublication`) | UNLOADED | simulation slot bit cleared; handle dropped; `stream.cells.cancelled`, `stream.cells.released` |
| T19 | CANCELLING | obs LOADING (the unload buffer was rejected) | LOADING | `stream.actions.rejected` |
| T20 | CANCELLING | obs UNLOADED and no discard yet | CANCELLING | waits; after 600 ticks logs once under `stream.cell` |
| T21 | FAILED | retry tick reached (step 14) and no release pending | UNLOADED | — |
| T22 | FAILED_PERMANENT | rebinding to the same world | UNLOADED | retries used, capacity failures and transient failures 0 |
| T23 | any but UNLOADED | world unbinding | as the unload rows | — |
| T24 | LOADING | obs UNLOADED: other code unloaded the instance (any C++ caller may record `Zenith_RecordUnloadAdditiveScene` on any instance); SCENE parks the discarded publication exactly as for STREAM's own unload (row ST2) | CANCELLING | `stream.cells.external_unload`; completes by T18 |
| T25 | QUEUED | obs gone (external unload) | UNLOADED | handle dropped; `stream.cells.external_unload` |
| T26 | ACTIVATING | obs gone (an external unload played in the activation's sync point after the spawns) | UNLOADED | as T27 (the merge has no committed bit to add: admission cleared them, and the pending ones are discarded) |
| T27 | ACTIVE | obs gone (external unload) | UNLOADED | no capture (the entities are gone); a delta-merge action records the cell's delta with a DESTROYED record added for every persistent entry whose committed tombstone bit is set, keeping every previous record; the cell's pending tombstone bits are discarded and counted in `stream.persistence.tombstones_dropped` (they hold the unload's own destroys, which are indistinguishable from deaths of the same ticks, "Tombstones"); state changes since the activation are lost; persistent handles cleared; `stream.cells.external_unload` |
| T28 | ACTIVE | obs ACTIVE and the anchor's handle is no longer valid (gameplay destroyed the anchor root and with it every entity of the cell) | UNLOADING | the release of the instance and a delta-merge action recorded (no capture), with a DESTROYED record for every entry whose committed or pending bit is set: the tombstones the destroy recorded stand, because nothing distinguishes that destroy from a gameplay death; persistent handles cleared; `stream.cells.anchor_destroyed` |
| T29 | UNLOADING | obs FAILED (the unload buffer was rejected and the QUEUED instance failed) | FAILED | as T6; flag "unload of a QUEUED instance" cleared; `stream.actions.rejected` |
| T30 | CANCELLING | obs QUEUED (the unload buffer was rejected and the preparation published success) | QUEUED | simulation slot bit cleared; `stream.actions.rejected` |
| T31 | CANCELLING | obs FAILED (the unload buffer was rejected and the preparation published failure) | FAILED | as T6; `stream.actions.rejected` |
| T32 | FAILED with release pending | obs FAILED (the release buffer was rejected) | FAILED | the release recorded again; `stream.actions.rejected` |
| T33 | FAILED with release pending | obs gone | FAILED | "release pending" cleared, so T21 may run |
| T34 | REQUESTED, LOADING, QUEUED, ACTIVATING, ACTIVE, UNLOADING, CANCELLING, FAILED | an observation the matrix of "Totality" names T34 (an instance state no other row allows for that state, such as a QUEUED cell observed ACTIVE without admission) | UNLOADING when the instance is live, otherwise UNLOADED | the release recorded when live; every slot bit STREAM holds cleared; flag "unload of a QUEUED instance" cleared; `stream.cells.unexpected_state`; logged once per cell per session under `stream.cell` |

A cell that stops being wanted while ACTIVATING completes its activation (it cannot be interrupted, [Z2-N-CONTRACT-337]) and is unloaded from ACTIVE at a subsequent tick; a cell that becomes wanted again while UNLOADING or CANCELLING finishes that transition and loads afresh, so no state is ever shared between two instances of one cell. A delta-merge action (T27, T28) is one buffer holding the delta holder writes of "Capture" step 4 with the merged delta and the clearing of `CellStatus` property 11; a rejected one is detected at the next update (property 11 still true) and recorded again from the persisted committed bits and the persisted delta, so no tombstone of the activation is lost and a restored run re-records exactly the merge the uninterrupted run re-records.

### Totality

Every (STREAM state, observed instance state) pair has exactly one row. States without an instance (UNLOADED, FAILED with no release pending, FAILED_PERMANENT) observe nothing; REQUESTED has no instance to observe and is resolved by T2, T3 or T4; every other state is resolved by this matrix ("stay" means no transition this tick; the event rows of steps 8 to 10 may still apply to the cell afterwards):

| STREAM state | obs LOADING | obs QUEUED | obs ACTIVE | obs FAILED | obs UNLOADED | not found |
|---|---|---|---|---|---|---|
| LOADING | stay | T5 | T34 | T6 | T24 | T34 |
| QUEUED | T34 | stay | T34 | T10 | T25 | T25 |
| ACTIVATING | T34 | T13 | T11 or T12 | T14 | T26 | T26 |
| ACTIVE | T34 | T34 | stay, or T28 when the anchor handle is invalid | T34 | T27 | T27 |
| UNLOADING | T34 | T17 | T17 | T29 | T16 | T16 |
| CANCELLING | T19 | T30 | T34 | T31 | T20 | T34 |
| FAILED with release pending | T34 | T34 | T34 | T32 | T33 | T33 |

T18 is an event row of step 3: a cell whose discard arrived is UNLOADED before step 4 runs, so step 4 never observes a CANCELLING cell whose discard arrived, and the CANCELLING row's "obs UNLOADED" cell is always T20 (the instance is UNLOADED and its discarded publication has not been published yet). A discarded publication for a cell that is not CANCELLING (possible only after T34) is counted in `stream.cells.unexpected_state` and ignored. `Test_StreamingCellMachine_TableIsTotal` enumerates every cell of this matrix and the three REQUESTED resolutions and asserts that exactly one row fires for each.

### Failure classification

Every failure STREAM observes carries a `Zenith_SceneErrorCode` (the instance's `GetError`, or the commit refusal's code) and, for `SCENE_ERROR_ASSET_UNAVAILABLE`, the root `Zenith_AssetErrorCode` SCENE carries as its detail (the instance's `GetErrorDetail`, row ST2 (i)). The class decides the retry, so that no failure caused by capacity, memory pressure or a passing I/O condition makes content permanently absent, and a failure of the content itself is retried a bounded number of times:

| Class | Retry | Counters |
|---|---|---|
| capacity | no retry consumed; the cell's consecutive capacity failures n (saturating at 255) rise by 1, and the retry tick is now + `Backoff(n)` | `stream.cells.capacity_failures` |
| transient | while the cell's consecutive transient failures m (saturating at 255) after this failure are at most `stream.cell.maxTransientRetries`, no retry is consumed and the retry tick is now + `Backoff(m)`; beyond it, the failure is treated as content | `stream.cells.transient_failures` |
| content | one retry consumed (retries used + 1, saturating at 255); the retry tick is now + `stream.cell.retryTicks`; FAILED_PERMANENT when the retries used exceed `stream.cell.maxRetries` | `stream.cells.failed` |
| permanent | FAILED_PERMANENT at once | `stream.cells.failed` |

`Backoff(k)` = `min((uint64)stream.cell.capacityRetryTicks << min(k − 1, 31), (uint64)stream.cell.retryTicks)` for k ≥ 1, computed in `uint64`: the key's maximum 3,600 is below 2^12, so the shifted value stays below 2^43, no shift exceeds 31 bits and no step overflows (the doubling saturates at `stream.cell.retryTicks` long before k reaches 255). The three counters (retries used, n, m) reset to 0 at T11, T12 and T22, and only there; each is a persisted field ("Persisted cell status").

| `Zenith_SceneErrorCode` | Class |
|---|---|
| `SCENE_ERROR_LIMIT` (commit: the set or the completion IDs are full; start: the loader's operation table is full), `SCENE_ERROR_OUT_OF_MEMORY`, `SCENE_ERROR_MEMORY_REFUSED`, `SCENE_ERROR_ACTIVATION_CAPACITY`, `SCENE_ERROR_CANCELLED` (a published cancellation STREAM did not request: SCENE or ASSET shutting a preparation down) | capacity |
| `SCENE_ERROR_ASSET_UNAVAILABLE` | by its ASSET detail, table below |
| `SCENE_ERROR_ACTIVATION_TOO_LARGE` | permanent (the cook's V16 makes it unreachable for a cooked cell; reaching it is a content-cook mismatch) |
| every other code (`DOCUMENT`, `SCHEMA`, `DUPLICATE_ID`, `PREFAB_NOT_FOUND`, `PREFAB_CYCLE`, `PREFAB_TOO_DEEP`, `HIERARCHY_TOO_DEEP`, `UNKNOWN_TYPE`, `COOKED_VERSION`, `COOKED_CORRUPT`, `DUPLICATE_INSTANCE`, `INSTANCE_NOT_FOUND`, `INVALID_DESC`, `WORLD_NOT_ATTACHED`, `ECS`, `READ_ONLY`, `EDIT_INVALID`, `NOT_FOUND`, and any value outside the enumerators) | content |

| ASSET detail of `SCENE_ERROR_ASSET_UNAVAILABLE` (`Zenith_AssetErrorCode`, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Errors") | Class | Why |
|---|---|---|
| `ASSET_ERROR_OUT_OF_MEMORY`, `ASSET_ERROR_QUEUE_FULL`, `ASSET_ERROR_REGISTRY_FULL`, `ASSET_ERROR_SHUTTING_DOWN`, `ASSET_ERROR_TIMEOUT` | capacity | an allocation, an intake or completion queue, the registry's slots or the service's lifetime: pressure on the loader, never a property of the cell's content |
| `ASSET_ERROR_IO`, `ASSET_ERROR_NOT_INSTALLED` | transient | a read that failed this time (the `Zenith_IOErrorCode` is not carried) or an asset pack not delivered yet: either may succeed at a retry, and either may also be permanent, so the transient budget bounds it |
| `ASSET_ERROR_DEPENDENCY_FAILED` | transient | ASSET reports a failed hard dependency without its cause today; row ST22 asks ASSET to carry the dependency's own root code, which SCENE then forwards in its place (so this row is reached only until ST22 lands, and for a dependency whose root code is itself unknown) |
| 0 (SCENE carried no detail) or a value outside the enumerators | transient | a defect of the producer; bounded like I/O, counted in `stream.cells.unknown_failure_detail` |
| every other code (`NULL_HANDLE`, `STALE_HANDLE`, `FOREIGN_HANDLE`, `KIND_MISMATCH`, `NOT_FOUND`, `CORRUPT`, `VERSION_MISMATCH`, `PLATFORM_MISMATCH`, `NO_LOADER`, `LOADER_FAILED`, `NOT_ACTIVE`, `NO_RUNTIME_OBJECT`, `INVALID_REQUEST`, `CATALOGUE_INVALID`, `LIMIT_EXCEEDED`, `KIND_ALREADY_REGISTERED`, `REGISTRATION_CLOSED`, `NOT_ALLOWED`, `COOK_IDENTITY_MISMATCH`, `NO_FALLBACK`) | content | the asset or its catalogue entry is wrong for this build, which only a re-cook or a rebinding fixes |

At the defaults (`stream.cell.capacityRetryTicks` 30, `stream.cell.retryTicks` 600, `stream.cell.maxTransientRetries` 4, `stream.cell.maxRetries` 3), a capacity failure backs off 30, 60, 120, 240, 480, then 600 ticks forever and never reaches FAILED_PERMANENT; a cell failing with `ASSET_ERROR_IO` every time retries after 30, 60, 120 and 240 ticks without consuming a retry (failures 1 to 4), then three times after 600 ticks (failures 5 to 7, retries used 1 to 3), and is FAILED_PERMANENT at its eighth consecutive failure (retries used 4 > 3); a content failure is FAILED_PERMANENT at its fourth (the first load and its three retries).

`SCENE_ERROR_DUPLICATE_INSTANCE` is a content conflict, never STREAM's own instance: STREAM never issues a load of a cell whose previous instance's release plays at the same sync point ("Loads (step 9)" step 1), and an UNLOADED instance is not live for SCENE's duplicate check, so a duplicate is a live instance of the cell scene the game loaded itself; it retries a bounded number of times and is then FAILED_PERMANENT. Every code and detail is SCENE's committed or published value, a logged simulation input (a published failure is `PUBLICATION_KIND_GENERIC_RECORDED` with the 8-byte `Zenith_SceneLoadResult` whose `m_uDetail` holds the ASSET code, `Zenith2/Docs/Design/SCENE/Loading.md` → "Publication service"), so the classification replays exactly. The failed cell's `Zenith_StreamingError` carries `STREAMING_ERROR_CELL_LOAD_FAILED` (or the commit codes of T3) with the SCENE code in the low byte of `m_uDetail` and the ASSET detail in its high byte.

### Persisted cell status

<!-- elaboration -->
Every field of a cell's record that a subsequent tick's decision reads, and that SCENE's additive set does not already hold, is mirrored in a saved component on the cell's **holder**: the runtime entity that carries `Zenith.Streaming.CellDelta` (its key: property 1 the cell scene GUID, property 2 the world ID) and `Zenith.Streaming.CellStatus` (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingComponents header"). SAVE saves the holder with every other entity (ENTITIES), so STREAM still registers no `Zenith_SaveParticipant` (row SV10) and a restore puts back exactly the status the uninterrupted run continues from ("Save and restore").

**Every cell whose status is not the default, or whose delta holds a record, has a holder.** A holder is spawned when the cell's status first leaves the default and is pruned when it returns to it with an empty delta ("Holder pruning" below). The first load action of a cell without a holder begins with the `Spawn` of one (CellDelta properties 1 to 3, an empty property 4; CellStatus with State REQUESTED and every other field 0), so the merge accepts or rejects the holder and the load together (the cell's runtime flag "holder spawn pending" is set, as for every holder spawn): a rejected buffer leaves neither (T4), an accepted one leaves the holder in place from the commit's sync point, before any failure, delay or tombstone of the cell can exist. A catalogue miss at issue ("Loads (step 9)" step 4) changes the status of a cell that may have no holder and records no load; it records a **status-holder action** instead, one buffer holding the `Spawn` of a holder with State FAILED_PERMANENT (or, when the cell has a holder, writes the status directly and records nothing). The cell is FAILED_PERMANENT at once in its record, with its runtime flag "holder spawn pending" set; holder discovery of the next update keeps it FAILED_PERMANENT when the holder exists, and returns it to UNLOADED (to be re-decided in step 9) when the spawn was rejected, so the record and the saved state agree from the start of that update on.

| `CellStatus` property | Field of the cell record | Written by |
|---|---|---|
| 1 State | the cell state | `Streaming.Update`, in the step of every row that changes it |
| 2 Instance | the SCENE instance the state refers to (0 none) | `Streaming.Update` (T2's instance at the next update, from the hook's record) |
| 3 RetriesUsed, 4 CapacityFailures, 5 TransientFailures | the three failure counters of "Failure classification" | `Streaming.Update` (the failure rows, T11, T12, T22) |
| 6 Flags (`uint16`) | bit 0 release pending; bit 1 preempted; bit 2 references bound; bit 3 state applied; bit 4 unload in progress (set at T15 only, "Tombstones"); bit 5 unload of a QUEUED instance; bit 6 re-home pending (the holder carries a delta record to re-home, "Cell identity across re-cooks"; on an orphan holder too) | `Streaming.Update` (bits 0, 1, 4, 5, 6; bits 2, 3 and 4 cleared at admission); the `Streaming.BindReferences` playback (bit 2), the `Streaming.ApplyCellState` playback (bit 3), the `Streaming.UnbindReferences` playback (clears bit 4); each playback reads the current value and writes it back with its bit changed |
| 7 RetryTick | the tick at which a FAILED cell may retry | `Streaming.Update` |
| 8 UnloadDueTick | the unload due tick ("Desired-set algorithm" step 5; 0 none) | `Streaming.Update` |
| 9 TombstoneBits (`FIXED_ARRAY` of 16 `U64`) | committed tombstones of this activation, bit i for the cell's persistent entry i | `Streaming.Update` (the merge at the end of step 4; the T27, T28 merges read them); cleared by `Streaming.Update` at admission (T8) |
| 10 PendingTombstoneBits (`FIXED_ARRAY` of 16 `U64`) | tombstones recorded since the last update | STREAM's lifecycle listener at the destroy's playback ("Tombstones"); cleared by `Streaming.Update` at the merge, at the discard and at admission (T8) |
| 11 DeltaMergePending (`BOOL`) | a delta-merge action recorded and not yet applied (T27, T28; a re-home sets none, "Cell identity across re-cooks") | `Streaming.Update` sets it when it records the merge; the merge buffer itself clears it (its last record is `SetProperty` of property 11 to false), so it is false exactly when the merge played |

Every write is direct: `Streaming.Update` writes through `GetChunkOfEntity` of its holder query (a phase-1 write of a `PHASE_INPUT` property), and the playbacks and the listener write through the `Zenith_WorldPlaybackAccess` they receive ([Z2-N-ARCH-544]); no status write is a command a merge could reject, except the holder's own `Spawn` (tied to the load, status-holder, repair or re-home action that records it), property 11's clearing record (tied to its merge, whose rejection it must share) and a prune's `Destroy` of the whole holder (resolved by holder discovery, below). The per-tick fields of the record (bands, own band, pins, dependents wanting, qdist, the per-tick flag bits, the admission entry) are recomputed every tick and are not persisted; the hook tag and the slot belong only to loads in flight, whose captures are COHERENT ("What is saved"); the tick of the last state change is diagnostic.

**The three inputs that reach the record outside `Streaming.Update`.** SCENE calls the activation hook on its sync-point node and its tick-boundary node, where STREAM has no write access to world state, so three changes land first in the world state's memory and reach the holder in step 3 of the next update, after holder discovery steps 1 and 2 have mapped every holder spawned at the previous sync points (a row applied in step 3 writes the holder discovered in the same step, so T3 of a cell's first load, whose holder its own load buffer spawned, writes that holder): T2's instance (the commit of a load: the cell is LOADING, a COHERENT capture in any case), T3's refusal (the commit-refused inbox) and T18's discard (the discard inbox: the cell is CANCELLING, COHERENT in any case). Only T3 can make an otherwise EXACT capture lose state, which is why SCENE's participant reports a capture whose tick refused a tagged commit as COHERENT (row ST2 (g)).

**Holder discovery** (step 3: steps 1 and 2 before the inboxes, step 3 after them):

1. For every row of the holder query whose `CellDelta` property 1 changed since the previous read (every row when `GetPreviousReadVersion()` is 0, `Zenith2/Docs/Design/ECS/Queries.md` → "Change-filtered queries"), resolve its cell with `FindCellByScene`; a holder of another world ID or of no cell of the bound world is left alone ("Cell identity across re-cooks"); a holder whose cell already has a live holder of a lower simulation ID is a duplicate, counted in `stream.persistence.orphan_deltas` and never read or written; otherwise its handle is recorded in the cell's mirror and the cell's record is written into it, directly (a newly spawned holder carries the status its spawn recorded, which steps after that recording may have changed). Matching an already-mapped holder again is a no-op, so the over-matching the filter allows changes nothing.
2. For every cell with "holder spawn pending": a mapped holder clears the flag. No holder means the spawning buffer was rejected: a cell whose spawn came with its first load stays REQUESTED for T4 to resolve in step 4; a cell whose spawn was a status-holder action (a catalogue miss) becomes UNLOADED and is re-decided in step 9; a cell whose spawn was a holder repair has the repair recorded again in this tick. The flag is cleared in every case and set again by the next spawn recorded. For every cell with "holder prune pending": an invalid holder handle means the prune played, so the mirror's holder handle is cleared and the record set to the default status (UNLOADED, instance 0, every counter, tick and flag 0, no tombstone bit); a valid one means the prune buffer was rejected, and nothing else happens (the holder is pruned when its cell next enters UNLOADED). The flag is cleared either way.
3. For every cell whose mirrored holder handle is no longer valid (gameplay destroyed STREAM's runtime entity, a misuse): a holder-repair action spawns a new holder carrying the cell's current status and an empty delta (the destroyed holder's delta bytes are lost, counted in `stream.persistence.holders_respawned`), and sets "holder spawn pending"; until step 1 of the next update discovers the new holder and writes the cell's record into it, a capture holds the status at the repair's recording, without the changes the following steps of that tick made. STREAM destroys a holder only when re-homing empties an orphan ("Cell identity across re-cooks") or a prune removes one that holds nothing (below).

**Holder pruning** (step 14, after the retries and the re-homes). Every cell that entered UNLOADED in this tick, in any step after the rebuild (the rebuild's own steps do not count), and is still UNLOADED, gets a holder-prune action, `Destroy(holder)` in its own buffer, when its holder exists, no other action of this tick writes it, its status is the default apart from its two ticks (instance 0, the three counters 0, every flag bit 0, no committed or pending tombstone bit, `DeltaMergePending` false) and its delta holds no record (`CellDelta` property 4 empty, or an encoding of zero records); its runtime flag "holder prune pending" is set, and holder discovery step 2 of the next update resolves it. The retry tick and the unload due tick are read only in FAILED and ACTIVE, so the default status the pruned cell takes loses nothing a subsequent tick reads, and the next load of the cell spawns a new holder with State REQUESTED as a first load does. The decision reads only this tick's transitions and the persisted status, and a rejected prune is not retried (its holder stays until its cell next enters UNLOADED), so an uninterrupted run and a run restored from a capture of any tick prune the same holders. Holders that exist are therefore at most the cells whose status is not the default or whose delta holds a record, plus the cells whose last prune was rejected, plus orphans ("Complexity"); `stream.persistence.holders_pruned` counts the prunes that played.

## Action buffers and rejection

Every action is one command buffer `Commands(0, uOrdinal)` with `uOrdinal` = 1 + the action's index in the tick's action list (the rebuild's releases, then unloads in step-8 order, then loads in step-9 order, then holder repairs and status-holder actions in cell index order, then admissions in step-10 order, then region destroys, then the rebase, then step 14's re-homes in their visiting order, then step 14's holder prunes in cell index order), so merge keys are a pure function of the tick's decisions. Each buffer `Reserve`s its whole size first; a `Reserve` failure (an action above one buffer's cap) is a defect of the caps, logged, and the action is not recorded (the cell keeps its state and retries next tick).

| Action | Records, in order |
|---|---|
| load | when the cell has no holder, `Spawn` of its holder (`CellDelta` 1 to 3, `CellStatus` State REQUESTED, "Persisted cell status"); then `Zenith_RecordLoadAdditiveScene(handle, { m_bAllowDuplicate false, m_eUnknownTypePolicy UNKNOWN_TYPE_POLICY_FAIL, m_ulHookTag tag })` |
| status holder (a catalogue miss of a cell with no holder) | `Spawn` of its holder with `CellStatus` State FAILED_PERMANENT |
| holder repair | `Spawn` of a holder carrying the cell's current status and an empty delta |
| unload of an ACTIVE cell | the delta holder writes of "Persistent entities" (`SetContainer` of `CellDelta` property 4 and `SetProperty` of property 3 on the cell's holder, which exists since its first load); `Record(Streaming.UnbindReferences, {cell, instance})`; `Zenith_RecordUnloadAdditiveScene(set, instance)` |
| unload of a QUEUED or LOADING cell | `Zenith_RecordUnloadAdditiveScene(set, instance)` |
| release of a FAILED, unexpected or anchor-less instance (T6, T10, T14, T28, T29, T31, T32, T34), or of a restored load the rebuild did not adopt (R4) | `Zenith_RecordUnloadAdditiveScene(set, instance)` |
| delta merge (T27, T28) | the delta holder writes of "Persistent entities" with the merged delta, then `SetProperty` of `CellStatus` property 11 to false on the same holder |
| re-home (step 14) | for each target cell with no holder, the `Spawn` of one carrying its current status; the delta holder writes of "Persistent entities" on every target holder with its merged delta; the same writes on the source holder with the moved records removed; `Destroy` of an orphan source holder left with no record; no `CellStatus` record ("Cell identity across re-cooks") |
| holder prune (step 14) | `Destroy(holder)` ("Persisted cell status", "Holder pruning") |
| admission | `Record(Streaming.ApplyCellState, {cell, instance})`; `Record(Streaming.BindReferences, {cell, instance})` |
| region destroy | `Destroy(entity)` per entity of one cell's region transition |
| rebase | `Zenith_RecordShiftWorldOrigin(shift)` (row ST3) |

The default merge accepts or rejects each buffer whole (`Zenith2/Docs/Design/ECS/Scheduling.md` → "Capacity acceptance"). STREAM never assumes acceptance: each action's effect is observed at the next update (T4, T12, T13, T17, T19, T29 to T32, holder discovery and its prune check, the delta-merge check of `CellStatus` property 11, and step 14's re-home rule, which reads the deltas themselves), and a rejected action is simply re-decided from the new state. Because each action has its own buffer, one rejection never drags another action with it, and a rejected unload never loses a capture: the entities still exist and are captured again at the re-issued unload.

## Cancellation at every stage

A cell load is a SCENE preparation over ASSET loads; STREAM cancels by unloading the instance and by destroying its own handles, and each lower stage releases what it holds ([Z2-N-CONTRACT-336], [Z2-N-CONTRACT-338]). The plan's four stages are each named by the layer that runs them: I/O and decompression are ASSET's pipeline stages under SCENE's `READING` (`Zenith2/Docs/Design/ASSET/Loading.md` → "Cancellation points by stage"), GPU upload is ASSET's `DELIVER`/`ACTIVATING` of a GPU-kind dependency, and activation is SCENE's sync point. SCENE's own `DECODING` is the decode of already-decompressed sections into `Zenith_SceneData`, a separate stage:

| Stage of the cell | What runs | STREAM's cancel | Released below STREAM | STREAM releases |
|---|---|---|---|---|
| REQUESTED (commit pending) | nothing yet | none possible this tick; the cell is re-examined as LOADING next tick | — | — |
| LOADING, preparation not started (start at the next prologue) | nothing | unload action (T7) | SCENE removes the pending preparation, calls `OnPreparationEnded` and parks the discarded publication (row ST2 (a), (c)) | the slot at T18 |
| LOADING, ASSET `HEADER_READ` or `SECTIONS` in `READING` of the scene or a hard dependency (I/O) | PLAT reads | unload action | SCENE cancels its operation and drops the scene handle; when that was the pipeline's last live request, ASSET cancels every in-flight read and releases staging only after the read reports | the slot at T18 |
| LOADING, ASSET `SECTIONS` in `DECODING` of the scene or a hard dependency (decompression) | ASSET's decode slices (COMPRESS contexts) | unload action | as above; the decode slice checks the cancel flag at its next boundary, destroys its `Zenith_CompressionContext` and releases staging and its unit | the slot at T18 |
| LOADING, ASSET `DELIVER` or `ACTIVATING` of a hard dependency (a CPU kind's object build, or a GPU kind's upload: a mesh, texture, probe of the cell) | the kind loader; GEOM's or RHI's upload | unload action | the dependency's last reference drops; ASSET calls its kind loader's `Cancel` (or `Unload` once active), which withdraws pending uploads and retires staging and pool ranges through deferred deletion | the slot at T18 |
| LOADING, SCENE `DECODING`, `DEPENDENCIES`, `GATED` or `BUILDING` | SCENE slices | unload action | SCENE's cancellation table (`Zenith2/Docs/Design/SCENE/Loading.md` → "Cancellation"): arena, handles, interned strings, init storage | the slot at T18; a closed gate opens for a slot without world state so the operation reaches its cancellation point |
| QUEUED | nothing | unload action (T9) | SCENE drops the queue entry and the prepared scene | — |
| ACTIVATING | the activation buffer | not cancellable; unloaded once ACTIVE | — | — |
| ACTIVE | — | unload (T15) | destroys release every entity's handles at the sync point | captures, references, persistent handles |
| prefetch or booster of the cell (presentation) | ASSET load at BACKGROUND or HIGH | the service destroys the operation handle and drops the asset handle | ASSET's cancellation of that request | the presentation handle-pool entry |
| the cell's PSO group | precacher creation jobs | the service destroys the group's operation handle | the precacher cancels creations no other request shares (`Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API") | — |

**The last live request.** ASSET cancels a pipeline only when the cancelled request was its last live one. While the service's booster (or a prefetch) of the cell scene is alive, SCENE's cancel does not stop the scene's own pipeline: it completes and the asset becomes ACTIVE with the booster's reference. The booster is destroyed by the service in the frame the cell leaves LOADING or CANCELLING ("Sub-steps, in order", sub-step 2), and the asset then unloads at zero references (or enters ASSET's keep-alive cache), so the residue at the end of a cancellation is the same with or without a booster; the stress runs every stage both ways ("Stress and determinism tests" in Tests.md).

A preparation that completes before it observes the cancel reports SUCCEEDED; SCENE then discards the prepared scene at publication ([Z2-N-CONTRACT-337]); for STREAM the outcome is the same discarded publication (logged as recorded, row ST2 (b)) and T18.

## Ownership of partial results

| Partial result | Owner while in flight | Released by | When |
|---|---|---|---|
| the cell scene handle | the load action, then SCENE's pending preparation | SCENE | at its cancellation or completion |
| hard-dependency assets of the cell | SCENE's preparation (counted references) | SCENE, then ASSET | cancellation: when the preparation releases; success: when the instance's entities are destroyed |
| decoded scene data, init storage | SCENE's preparation arena | SCENE | cancellation or discard |
| the snapshot slot | STREAM (simulation bit) and the preparation (preparation bit) | whichever clears the last bit | T3, T4, T5, T6, T18, T30, T31, T34 and R4's neutralisation of an unadopted restored load (simulation); `OnPreparationEnded` (preparation) |
| the entity table copy (tombstone bits, presence masks) | the slot | the slot's reuse | — |
| booster, prefetch, HLOD and PSO operation handles | the service's presentation state | the service | when no longer wanted, at shedding, at detach |
| entities of an activated cell | the world | the unload's destroys | the unload's sync point |
| the cell's delta | its holder entity | never (it persists; it is rewritten at each unload) | — |

No partial result is owned by two parties without a defined last releaser, which is what lets the 1,000-cycle stress assert zero residue.

## Completion reporting

A cell's release is complete when the cell is UNLOADED (T16 or T18) and: its slot's simulation bit is clear; its handles held by STREAM's simulation state are dropped; every presentation handle naming the cell's scene has been destroyed by the service (within one frame). `stream.cells.released` counts T16 and T18. At T18 both slot bits are clear: SCENE calls `OnPreparationEnded` before the operation parks any publication (row ST2 (c)), so the preparation bit is clear before the discarded publication can be published, and T18 clears the simulation bit. A slot can still be held after its cell left CANCELLING only in two cases, both outside T18: under `--replay`, where the recorded discard publishes at its logged tick while the replay's own cancelled preparation may still be running (row ST21 (c)); and after `DetachWorldState`. `Zenith_StreamingSnapshotPool::GetHeldCount` reports such slots, and the stress test waits for `IsEverySlotFree` (bounded by the preparation's 2 ms slices) before its residue assertion. STREAM reports no completion to gameplay other than the cell state (`Streaming.GetCellState`) and the sources' `Ready`.

## Cross-cell references

### Binding

`Streaming.BindReferences(cell, instance)` plays at step 7 after the activated cell's spawns:

1. Verify the cell's instance is `instance`; a different instance is a defect counted under `stream.commands.malformed`. When the instance is QUEUED (SCENE's activation buffer was rejected while STREAM's was accepted, the legitimate T13 case), count `stream.cells.state_apply_skipped`, leave the "references bound" flag clear and stop: the cell is re-admitted and binds at its activation. Any other non-ACTIVE state stops the same way.
2. For each incoming record (target = cell) whose source cell is ACTIVE, and each outgoing record (source = cell) whose target cell is ACTIVE, in reference index order: `FindByPersistentID` of the source and target entities through the playback's `Zenith_WorldPlaybackAccess`; when both exist, write the target's handle into the source's property (scalar: `SetProperty`; fixed-array element: read the array with `ReadProperty`, replace the element, write it back); count `stream.references.bound`. A missing entity (destroyed by gameplay, skipped as tombstoned) leaves the slot null and counts `stream.references.unresolved`, logged once per (cell, reference) per session under `stream.ref`.
3. Set the cell's "references bound" flag.

Both directions are written by the activation of whichever cell activates second, so the order in which two neighbours activate never matters.

### Unbinding

`Streaming.UnbindReferences(cell, instance)` plays at step 7 of the unload's sync point, after the destroys: for each incoming record whose source cell is ACTIVE (its source entity lives on), write null into the source's slot if it still holds the handle of the (now destroyed) target; outgoing records need nothing (their source entities are gone). Count `stream.references.unbound`. A gameplay write to a reference slot after binding is respected: unbinding nulls the slot only when it still names the destroyed target.

### Rules

- Containers of entities are refused at cook time (V22): a reference slot is a scalar or a fixed-array element, so binding never allocates.
- A reference from the persistent layer into a cell is not a cell reference: the persistent layer's entities name cell entities by persistent ID through gameplay code (`Zenith.Streaming.Persistent` entities keep their IDs across reloads), resolved when needed with a handle lookup that fails cleanly while the cell is unloaded.

## Persistent entities

<!-- elaboration -->
Persistence as per-cell delta records on runtime holder entities, keyed by the cell scene's GUID, is a design choice of this family ("Design choices at a glance" in the entry document; SAVE row SV10 adopts it).

### Modes

| Mode | At unload | At reload |
|---|---|---|
| NONE (no component) | nothing | authored state |
| DESTRUCTION | a tombstone if the entity was destroyed while its cell was active | skipped when tombstoned |
| STATE | a tombstone if destroyed; otherwise a STATE record of the configured types: which are present, and the eligible properties of each present one | skipped when tombstoned; otherwise spawned with its recorded component set (SCENE's amendment, "Component presence"), then the recorded values applied in the activation sync point |

A `Zenith.Streaming.Persistent` mode value outside the enumerators is treated as NONE and counted once per entity in `stream.persistence.unknown_modes`.

### Eligible property kinds

| Eligible | Not eligible (skipped at capture, counted once per type in `stream.persistence.ineligible_properties`) |
|---|---|
| `BOOL`, `U8` … `U64`, `I8` … `I64`, `F32`, `F64`, `VEC2`, `VEC3`, `VEC4`, `QUAT`, `COLOUR`, `ENUM`, `PERSISTENT_ID`, `FIXED_ARRAY` of any of these | `STRING_ID` (process-local interning), `ENTITY` (handles are per instance; cross-cell references are "Cross-cell references"), `ASSET` (the authored reference reloads with the cell), `CONTAINER`, `STRUCT`, every property without `PROPERTY_FLAG_SERIALIZED` |

A `VEC3` property flagged `PROPERTY_FLAG_WORLD_POSITION` (row ST4) is stored anchor-relative: at capture `(double)value + O − anchor`, narrowed to binary32; at restore `(double)stored + anchor − O`. An origin rebase while the cell is unloaded therefore cannot corrupt a restored position. `Zenith.LocalTransform` of a persistent entity is anchor-relative already (the anchor is its ancestor) and is captured as is when the game lists it. The derived transform types `Zenith.WorldTransform` and `Zenith.PreviousWorldTransform` (computed by propagation, origin-relative, matrices) are refused in `stream.persistence.componentTypes` by `Zenith_ReadStreamingSettings` with `STREAMING_ERROR_CONFIG_INVALID` naming the key: restored as captured they would be wrong after any rebase until propagation, and propagation recomputes them from `Zenith.LocalTransform` anyway.

### Tombstones

STREAM's lifecycle listener (interest mask TELEPORTED, DESTROY_PENDING) receives DESTROY_PENDING for every destroy, on the world's sync-point node during the playback of that destroy. For an entity that carries `Zenith.Streaming.Persistent` with mode DESTRUCTION or STATE, whose cell (looked up from property 2's cell scene GUID with `FindCellByScene`) is ACTIVE or ACTIVATING and whose "unload in progress" flag is clear, it finds the entity's index i in the cell's persistent entries (binary search by persistent ID), sets bit i of the cell's **pending** tombstone bits in the world state's mirror (`m_auPersistentFlags` bit 1) and writes the same bit into `CellStatus` property 10 of the cell's holder through its `Zenith_WorldPlaybackAccess` (`ReadProperty` of the array, the element's bit set, `SetProperty`; [Z2-N-ARCH-544] allows property writes at playback), and counts `stream.persistence.tombstones`. Nothing is queued: the write lands in a saved property in the same playback as the destroy, so no event can be dropped, delayed past the cell's next observation, or lost by a capture.

The "unload in progress" flag is set only at T15, when step 8 records the unload of an ACTIVE cell, the one unload that has destroys and a `Streaming.UnbindReferences` playback. It is cleared by that unload's `Streaming.UnbindReferences` playback (step 7 of the same sync point, after the unload's destroys of step 6), by the next update's observation when the unload buffer was rejected (T17), and, as backstops, by T16 and at admission (T8), both direct writes. A QUEUED unload (T9), a LOADING cancel (T7) and a release of a FAILED or unexpected instance never set it, so it cannot survive into a subsequent activation and suppress that activation's deaths. Destroys while it is set are the unload's own and are not deaths.

At the end of step 4, every cell still ACTIVE moves its pending bits into its **committed** bits (`m_auPersistentFlags` bit 0, `CellStatus` property 9: committed |= pending, pending = 0, both written directly). T26 and T27 (the cell's instance is gone) instead discard the pending bits, counted in `stream.persistence.tombstones_dropped`: when another caller unloads a cell's instance, the unload's destroys play while STREAM still sees the cell ACTIVE, so its pending bits mix that unload's destroys with the deaths of the same ticks and cannot be told apart; the committed bits, merged at earlier updates, hold every death observed before. T28 (the anchor destroyed) keeps both, because the anchor's destroy is a gameplay destroy. At capture (T15) the committed bits, together with the absence rule of "Capture" step 2, become DESTROYED records of the cell's delta, so a tombstone survives unload, save and reload. Admission of the cell's next activation (T8, step 10) clears both bit sets, directly, before any entity of that activation exists; no playback clears them, so a late `Streaming.ApplyCellState` (T12) cannot discard the tombstones of deaths in phases 2 to 10 of the activation tick that step 4 had already committed.

A death that records no tombstone is still written DESTROYED at the next capture by the absence rule ("Capture" step 2: an entry whose entity no longer exists), so only T26, T27 and T28 depend on the bits themselves. Two windows record no tombstone, and both are defined:

- **Phases 2 to 10 after a rejected unload** (T17). The flag stays set until the next update observes the rejection, so a death in a subsequent phase of that tick records no bit; the re-issued unload's capture writes DESTROYED for it by the absence rule.
- **Phase 1 after `Streaming.Update` in the tick that records the cell's unload.** A destroy recorded by a phase-1 node ordered after `Streaming.Update` plays at the same sync point as the unload's destroys, while the flag is set, and is indistinguishable from them; the capture already ran in the update, so the entity reloads with the state captured in that update. No script or game system can open this window: phase 1 hosts engine nodes only (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases": action mapping, the context stack, the replay-log read or write, module environment advance, `Scene.ActivateAdditive`), behaviours and script chunk systems run in phases 2 and 10 only, and none of the phase-1 nodes that exist records `Destroy` on cell content (`Scene.ActivateAdditive` records spawns; ENV's environment advance writes its own resource). Any future phase-1 node that may destroy entities of streamed cells must declare `Before("Streaming.Update")` (or `BeforeIfPresent`), so its destroys play at the sync point of the tick before the cell's unload is decided and are recorded as deaths. `Test_StreamingPersistence_PhaseOneDestroyInUnloadTick` pins the window's behaviour.

### Capture

For each ACTIVE cell being unloaded (T15), in `Streaming.Update.Persist`:

1. Start from the cell's previous delta (decoded from its holder component). Records the decoder's check D9 sets aside (entities that are not persistent entries of this cell: a re-home not yet performed) are carried into the new encoding unchanged; when it carries any, the capture sets the holder's `CellStatus` flag bit 6 "re-home pending" (a direct write; normally the bit is set already), so a deferred re-home survives every capture until step 14 performs it ("Cell identity across re-cooks").
2. For each persistent entry of the cell in ascending persistent ID: if its committed or pending tombstone bit is set or the previous delta holds a DESTROYED record for it, write DESTROYED. Otherwise resolve its handle from the persistent handle table; if the entity no longer exists (a death in a window that records no tombstone, "Tombstones") or is no longer a descendant of the cell's anchor (gameplay reparented it out of the cell, so it survives the unload as persistent-layer content), write DESTROYED so a reload never duplicates it. Otherwise, for mode STATE, write one STATE record: for each type of `stream.persistence.componentTypes` in ascending type ID, present (the entity carries it: `Zenith_CaptureFieldSet` with `bSerializedOnly` true, the eligible-kind filter and the world-position conversion applied) or absent (its type ID in the record's absent list, "Component presence").
3. Encode (Formats.md "Cell delta encoding") into the scratch arena. If the encoding exceeds 65,535 bytes or 1,024 records, drop STATE records (never DESTROYED records) in descending persistent ID until it fits; count `stream.persistence.delta_overflow` and log the cell once; dropped entities reload authored.
4. Record the holder writes into the unload's buffer: `SetContainer` of `CellDelta` property 4 and `SetProperty` of property 3 on the cell's holder (every ACTIVE cell has one: its load spawned it when the cell had none, and a holder is pruned only while its cell is UNLOADED, "Persisted cell status"). Counters `stream.persistence.captured_entities`, `.captured_bytes`.

### Component presence

A STATE record states the presence of every type configured at capture: a present type carries its field set, an absent one its type ID in the absent list. At reload the presence is applied before the entity exists, through SCENE's preparation, so no tick sees the authored component set of a persistent entity whose set changed at runtime:

1. When a load is issued (step 9) or adopted at a restore (R4), STREAM builds the slot's entity table from the cell's delta: per persistent entry of the cell (by its index in the cell's persistent range), the tombstone bit, an add mask and a remove mask over the indices of `stream.persistence.componentTypes` in this build (bit t: type t is present in the record, respectively in its absent list). A record type not configured in this build states nothing (its values are skipped at restore and counted in `stream.persistence.unconfigured_types`); a configured type the record does not mention keeps its authored presence.
2. SCENE calls the hook's `AmendComponents(tag, id, amender)` once per entity of the prepared scene after `ShouldSkipEntity` returned false, during `BUILDING` (row ST2). STREAM finds the entry by binary search over the cell's persistent entries (sorted by ID in the world asset) and calls `AddComponent(type)` for each set add bit and `RemoveComponent(type)` for each set remove bit; the amender adds the component with its type's defaults to the entity's init record, or removes it, and ignores an add of a type already present and a remove of an absent one.
3. `Streaming.ApplyCellState` then writes the recorded values of every present type in the activation sync point ("Restore"), so a runtime-added component reappears with its captured values and a runtime-removed one stays removed.

The masks are 32 bits because `stream.persistence.componentTypes` names at most 32 types. An entity of mode DESTRUCTION states no presence.

### Cell identity across re-cooks

Every saved identity of a cell is its cell scene's persistent ID, never its cooked index: `Zenith.Streaming.CellDelta` property 1, `Zenith.Streaming.CellAnchor` property 1 and `Zenith.Streaming.Persistent` property 2 are `CellScene` GUIDs, unique per world (reader check S11), and the cell index is derived from them at run time with `Zenith_StreamingWorldAsset::FindCellByScene` (the GUID map the kind loader builds in step L5). A re-cook that inserts or removes cells (a content update, Z2-D-048's "saves migrate forward") renumbers the cells but changes no GUID, so:

| Change between save and load | Behaviour |
|---|---|
| a cell inserted or removed elsewhere in the (grid, z, x) order | every delta, anchor and persistent marker still names its own cell; nothing is lost |
| a cell removed | its delta holder names a scene that is no longer a cell (an orphan, counted in the GAUGE `stream.persistence.orphan_deltas`): each of its records whose entity is now a persistent entry of another cell is re-homed to that cell by step 14 (below); the holder is kept with its remaining records, never applied (it is saved again, so restoring the cell in a subsequent content version finds its delta) |
| a cell added | it has no holder and reloads authored |
| a persistent entity moved to another cell (a re-partition that changed cell sizes moves many) | its record in the old cell's delta fails decoder check D9 for that cell and is re-homed by step 14 to the cell the by-ID persistent index (loader step L4, `FindPersistentCell`) names now; only a record whose entity is a persistent entry of no cell of the world is dropped (`stream.persistence.records_dropped`) |
| the world asset replaced (another `worldId`) | holders and anchors of the old world ID are ignored by the new binding and kept |

**Re-homing** is a per-tick rule of step 14 that the uninterrupted run and a restored run execute identically. A holder of the bound world ID has records to re-home when its delta holds a record whose entity's current cell c' (`FindPersistentCell`) differs from the holder's cell, or when the holder is an orphan and a record's entity is a persistent entry of some cell of the world. Such a holder carries `CellStatus` flag bit 6 "re-home pending": set, directly, by rebuild step R5 and by the binding's marking pass (both decode every holder of the bound world ID and set the bit where it is missing) and by a capture that carries a set-aside record ("Capture" step 1); cleared, directly, by step 14 when it decodes the holder and finds no record left to re-home. The world state caches the bit: for a cell's holder in flags 3 bit 1 of the cell record, with `m_uRehomePendingCells` counting the set bits, and for orphans in `m_uRehomePendingOrphans`, both rebuilt by R3 and R5 from the saved bits.

While either count is non-zero, step 14, after the retries, builds in its scratch arena the list L of the n flagged holders, cells' holders in ascending cell index and then flagged orphans in ascending simulation ID (the orphans found by a pass over the holder query's rows, run only while `m_uRehomePendingOrphans` is non-zero), and visits min(n, V) of them, V = `stream.unload.maxCellsPerTick`, starting at position (tick × V) mod n and wrapping: a tick decodes at most V deltas and records at most V re-home actions, every flagged holder is visited at least once every ⌈n / V⌉ ticks, and which holders a tick visits is a pure function of the tick index and the persisted bits. For each visited holder:

1. Decode the holder's delta. With no record left to re-home, clear bit 6 (and lower its count) and go to the next holder; no action.
2. Skip the holder this tick when it, or the holder of any target cell it would write, is written by another action of this tick (an unload capture, a T27 or T28 merge, a re-home already recorded, a holder spawn) or carries `DeltaMergePending`; it is visited again at the next tick.
3. For each record whose c' is UNLOADED, FAILED or FAILED_PERMANENT after this tick's steps 4 to 13 and the retries of this step, in ascending persistent ID: decode c''s delta (once per target), insert the record unless c''s delta already has a record for that entity (the newer content's own record wins and the moved record is discarded), and re-encode it (dropping STATE records in descending persistent ID if it would exceed the cap, as "Capture" step 3). A record whose c' is in any other state stays in the source holder: c''s content is live or in flight, and its next capture or activation would overwrite or ignore a record inserted now; `stream.persistence.rehome_deferred` counts it at this visit.
4. When step 3 moved or discarded a record, record one re-home action ("Action buffers and rejection"): a holder spawn for each target cell that has none (its status is then the default, so the spawn carries it, with "holder spawn pending" set), the delta writes of every changed target holder, the source holder rewritten without the moved and discarded records, and `Destroy` of an orphan source left with no record. Each moved record counts `stream.persistence.records_rehomed`.

A re-home sets no `DeltaMergePending`: whether it played is read back from the deltas themselves by the next tick's step 14, so a rejected re-home is simply recorded again, and a capture taken at any tick holds either the moved or the unmoved records, from which the next step 14 continues identically in either run. The rule reads only persisted state (the holders' deltas and status, the cell states this tick's earlier steps left, the bound world asset) and its actions come after every other action of the tick, so a run restored from an EXACT capture records exactly the re-homes of the uninterrupted run, at the same ticks and under the same merge keys. Neither R5 nor the binding moves a record: after a content update they only mark, and the moves follow in step 14 of that tick and the ticks after it.

The hook tag's low word (the cell index + 1 at the recording tick) is informational only; the rebuild identifies a restored load's cell from its instance's scene GUID ("Rebuild" R4).

### Restore

`Streaming.ApplyCellState(cell, instance)` plays at step 7 of the activation's sync point, before `Streaming.BindReferences` of the same buffer:

1. Verify the instance (a different instance is `stream.commands.malformed`; a QUEUED one, SCENE's activation having been rejected, counts `stream.cells.state_apply_skipped`, leaves the "state applied" flag clear and stops); set the anchor's `Instance` property (the anchor is the cell scene's root, found by its persistent ID). The playback identifies its world's state through the world tag of its playback access (core-doc item X238, `Zenith_WorldPlaybackAccess::GetWorldTag`), as every STREAM playback does.
2. Fill the persistent handle table for the cell's entries with `FindByPersistentID` (a tombstoned entity was skipped by the preparation and stays null). The entries' committed and pending tombstone bits are not touched here: admission (T8) cleared them before the activation's entities existed, so a late playback (T12) keeps the tombstones of deaths in phases 2 to 10 of the activation tick.
3. Decode the cell's delta (decoder checks D1 to D9); a failure discards the delta with `STREAMING_ERROR_DELTA_CORRUPT` logged and the cell activates authored.
4. For each STATE record whose entity exists: for each present type configured in this build, migrate the component record to its type's current version (`Zenith_MigrateFieldSet`), convert flagged world positions, and apply it with `Zenith_ApplyFieldSet` through `SetProperty` writes of the playback (properties the record lacks keep their authored or default values; the component exists, because the amendment added it); count `stream.persistence.restored_entities`. A type ID unknown to the registry is skipped and counted in `stream.persistence.unknown_types`.
5. Set the cell's "state applied" flag (read by T11 and T12), in the mirror and in `CellStatus` property 6 bit 3.

Restored values are visible from phase 2 of the activation tick, exactly when the entities first exist, so no system reads an entity's authored state and then its restored one, with one stated exception: when STREAM's admission buffer was rejected while SCENE's activation was accepted (T12), phases 2 to 10 of the activation tick see the authored values of each restored entity's components, or its type defaults for a component the amendment added, and the recorded values appear from phase 2 of the next tick (`stream.cells.late_state_apply` counts it): the late `ApplyCellState` overwrites the restored properties, including any value gameplay wrote to them in phases 2 to 10 of the activation tick, while the tombstones of that tick stand. The component set is never late: it is amended during preparation.

## Region states and coordinated subsystems

### The region

The **region grid** of a world is its first COLLISION grid, or, when it has none, its first GAMEPLAY grid. A world position is **in an active region** when the region-grid cell containing it (`FindCellContaining`) is ACTIVE. For every entity with `Zenith.Streaming.RegionBound`, step 11 writes `CellIndex` (property 3: the containing cell, or 0xFFFFFFFF), `InActiveRegion` (property 2: false outside every cell and in any non-ACTIVE cell) and `EvaluatedTick` (property 4: the current tick). The three properties are serialized (saved with the entity) and written directly by `Streaming.Update` through its region query. Cells activated this tick become ACTIVE at the sync point, so their entities' flags turn true at the next tick's step 11; an entity spawned by an activation carries its cooked values (`EvaluatedTick` 0, so it has no evaluation to compare with) and is flagged at the next update, one tick afterwards, which is the same tick its cell's dependents may first activate.

With `STREAMING_REGION_POLICY_DESTROY`, an entity is destroyed in the region action buffer (`stream.regions.destroyed`) when this step computes false for it while its stored `InActiveRegion` is true and its stored `EvaluatedTick` is the previous tick (its previous evaluation was the bound evaluation of tick − 1); FLAG_ONLY entities only see the flag. The transition reads only the entity's own saved properties, so a restored run destroys exactly the entities the uninterrupted run destroys, and a freshly spawned, freshly bound or long-unevaluated entity is never destroyed by its first evaluation. An unbound world writes `InActiveRegion` true and `EvaluatedTick` 0 everywhere (step 2), so the first bound evaluation after a binding destroys nothing.

### Physics coordination

<!-- elaboration -->
PHYS (row ST8, implemented in `Zenith2/Docs/Design/PHYS/Simulation.md` → "Region freeze" and "Cell bodies") treats `InActiveRegion == false` as "freeze": a dynamic body of such an entity is made kinematic-sleeping with its velocities saved and is excluded from integration and contact generation until the flag returns, so a body never falls through the collision of an unloaded cell. Static colliders, terrain colliders and triggers live in COLLISION cells and enter and leave with them. Dependent cells depend on the collision cells beneath them through the grid's `dependsOn`, so gameplay bodies never activate above absent collision. The guard band is the overlap margin: a body near a collision cell's edge lies inside the margin of the neighbour, and the KEEP hysteresis keeps that neighbour active while the source is near.

### Navigation coordination

<!-- elaboration -->
AI (row ST9, implemented in `Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles", "Tile remove procedure" and `Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine"; library D-SEL-NAV, open, Stage 1/2) streams navigation tiles as components of NAVIGATION cells. GAMEPLAY cells depend on the navigation cells beneath them, so an agent's tile is requested by the tick the agent's cell activates at the latest: AI requests a navigation cell's tiles in the phase 7 of the tick its tile entities spawn (the cell's activation sync point), and each tile joins in the phase 7 after its load's `ASYNC_SOURCE_NAVIGATION` publication, two ticks after the request at the earliest; an agent whose tile has not joined is SUSPENDED, then replans. An agent whose `InActiveRegion` is false is suspended (no path requests, no movement); a path request that crosses an unloaded tile returns AI's partial-path result, never STREAM's concern. Tile links across cells are AI's off-mesh links, registered when both tiles are active, the same both-directions rule as "Cross-cell references".

### Audio coordination

<!-- elaboration -->
AUDIO (row ST10, implemented in `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Region-bound emitters" and "Cells unloading under playing voices") streams ambience zones, emitters and acoustic volumes as components of AUDIO cells, with the banks they need as the cells' hard dependencies, so a bank loads with its cell and its last reference drops at unload. AUDIO grids typically use a larger activation radius than GAMEPLAY (sound is heard before it is seen). An emitter whose `InActiveRegion` is false is virtualised. Audio's own streaming of sample data is AUDIO's; STREAM only decides which cells' audio content exists.

### What other modules may and may not do

They read `Zenith.Streaming.RegionBound.InActiveRegion`, `CellIndex` and `Zenith.WorldOrigin` by stable name in their own systems (phases 2 to 10); they never call STREAM ([Z2-N-ARCH-850]) and never write STREAM's properties (authority `PHASE_INPUT`, STREAM's alone).

## Large coordinates and origin rebasing

<!-- elaboration -->
Absolute double anchors, anchor-relative cell scenes, the root translation at admission and the quantised rebase policy are design choices of this family ("Design choices at a glance" in the entry document); the shift itself is SCENE's (row ST3).

### The two frames

Absolute positions are binary64 metres in the world frame; simulation positions (every component) are binary32 relative to the world origin O of `Zenith.WorldOrigin` (row ST3), so the absolute position of a local p is `O + (double)p`. Cooked cell content is anchor-relative (Formats.md "Cell scene rules"), and the anchor of a cell is absolute, so no cooked float ever holds a large coordinate.

### Precision target

STREAM adopts the signed-off convention of `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems": a simulation position keeps about 0.25 mm resolution. A binary32 value in `[2^e, 2^(e+1))` has spacing `2^(e − 23)`, so every |coordinate| below `uSTREAMING_PRECISION_RADIUS_METRES` = 4,096 m (2^12) has spacing at most 2^−12 m = 0.244 mm, the spacing CORE quotes at 2 km (2,048 m to 4,096 m is one binade). The precision radius W = 4,096 m is the one target every STREAM number below is derived from; ENV's acceptance of 1 to 2 mm for its own float records up to ±16 km is reconciled with it by row ST19.

The consequence for motion: round-to-nearest loses a per-tick displacement below half a spacing, so the smallest velocity that moves a stored binary32 position at 60 Hz is `60 × 2^(e − 24)` m/s:

| Distance from O (per axis) | Spacing | Smallest velocity that moves the position at 60 Hz |
|---|---|---|
| below 1,024 m | 0.061 mm | 1.8 mm/s |
| 1,024 m to 2,048 m | 0.122 mm | 3.7 mm/s |
| 2,048 m to 4,096 m (W) | 0.244 mm | 7.3 mm/s |
| 8,192 m to 16,384 m (no rebase, ENV's large terrain) | 0.977 mm | 29.3 mm/s |

A subsystem that integrates slower motion (PHYS) keeps its sub-spacing remainder in its own state; STREAM's guarantee concerns the stored component position.

### Root translation

When a cell is admitted, its root translation is `t = (float)(anchor − O)` per axis, computed in binary64 and narrowed last, with O the origin `Streaming.Update` read this tick. SCENE adds t to the root's `Zenith.LocalTransform` position and to the recorded world-space positions of the spawns (row ST2), so the cell appears where it belongs relative to the current origin. A shift recorded in the same tick plays after those spawns and moves them with everything else ("Rebase rule").

### Rebase rule

Rebasing is off by default (`stream.rebase.enabled` false): a world whose content stays within W of the origin needs none, which includes W-Outdoor's 4 km² (Z2-D-131) when centred on the origin; the binding check of "World binding" warns (`stream.rebase.precision_warning`) when a world with rebasing off has a cell beyond W. When enabled, step 13 of the update, with the primary source (the lowest simulation ID among sources with `Primary`; none → no rebase):

1. Let `L = P − O` be the primary's local position (binary64). If `max(|Lx|, |Lz|)` (and `|Ly|` when `stream.rebase.vertical`) is at most T = `stream.rebase.thresholdMetres` (2,048), do nothing.
2. If fewer than N = `stream.rebase.minIntervalTicks` (30) ticks have passed since `Zenith.WorldOrigin::m_ulLastRebaseTick`, do nothing.
3. The shift is `S = round(L / q) × q` per rebased axis (round half away from zero), q = `stream.rebase.quantumMetres` (2,048), an exact integer in binary64; non-rebased axes 0.
4. Record `Zenith_RecordShiftWorldOrigin(S)` in the rebase action buffer; count `stream.rebase.count`; log under `stream.rebase`.

There is no "no rebase while a cell activates" condition and no postponement: SCENE plays the shift last in step 7 of sync point 1 (row ST3), after the sync point's spawns (step 6), after `Streaming.ApplyCellState` and `Streaming.BindReferences` (step 7) and after SCENE's own activation records, so a cell admitted this tick with a root translation computed from the pre-shift O is shifted with every other position. A rejected rebase buffer is re-decided at the next update from the unchanged O.

`Zenith_ReadStreamingSettings` refuses q > 2T (`STREAMING_ERROR_CONFIG_INVALID`), so after a shift `|L| ≤ q/2 ≤ T` and the rule never re-triggers at once.

**Bounds at the defaults.** The primary's local position satisfies `|L| ≤ T + v · N / 60` per rebased axis: after a shift it is at most q/2 ≤ T, and it can exceed T for at most N ticks before the next shift. At STREAM's design maximum traversal speed v = 300 m/s that is 2,048 + 150 = 2,198 m. The 300 m/s is STREAM's own design figure, not a Budgets number: `Zenith2/Docs/Budgets.md` → "Teleport and rapid traversal" ([Z2-N-BUDGET-374]) measures "the workload's maximum traversal speed" and names none, and W-Outdoor's camera path runs at 10 m/s ([Z2-N-BUDGET-199]); the defaults keep this bound for every v ≤ 300 m/s, and a workload that traverses faster sets a smaller `stream.rebase.minIntervalTicks` N or threshold T so that T + v·N/60 + R_g stays below W. Content the primary keeps active in grid g lies within `R_g = (Ract_g + H_g) · s + c_g · (1 + m_g)` of it per axis (the KEEP distance to the cell's rectangle, the cell's size and its overlap margin), so every such position satisfies `|p| ≤ 2,198 + R_g`, which is below W exactly when `R_g ≤ 1,898` m:

| Grid (Formats.md example) | c, Ract + H, m | R_g at s = 1 | worst `|p|` at s = 1 | largest s inside W |
|---|---|---|---|---|
| `collision` | 256, 576, 0.25 | 896 m | 3,094 m | 2.74 |
| `content` (gameplay) | 128, 448, 0.25 | 608 m | 2,806 m | 3.88 |

A grid or radius scale beyond its column still streams correctly; its far content only has the coarser spacing of the next binade, and the GAUGE `stream.precision.cells_beyond_radius` counts the ACTIVE cells whose farthest corner (overlap margin included) lies beyond W of O on an axis, computed in step 13 every tick. Content of a secondary source far from the primary (a preload marker across the world) is placed exactly but has the spacing of its distance from O; the same gauge reports it.

**Exactness of a shift.** For every binary32 position p, `(double)p − S` is exact in binary64; narrowing it to binary32 is exact whenever `|p − S| ≤ |p|` (the value moves toward the origin, so the result's spacing divides p's, and S is a multiple of 128 m, itself a multiple of every spacing below 2^30 m), and rounds once otherwise. A long journey therefore accumulates no drift: each value is rounded at most once per shift, and only values moving away from the origin, which the next shift brings back.

### Who reacts to a rebase

| Consumer | Reaction |
|---|---|
| SCENE | shifts transforms and flagged properties last in step 7 of the sync point (row ST3) |
| RPROD | the next extraction is full-state; every instance's previous-frame transform and every view's previous view transform are translated by −S instead of reset, so motion vectors and temporal histories stay valid; a camera cut only for a view whose history cannot be translated (row ST7) |
| ENV | subtracts O in binary64 in its render records; re-extracted by the full-state rule (row ST11) |
| PHYS, AI, AUDIO | read the new O: PHYS shifts its backend world at the shift's own sync point, on the lifecycle event SCENE's shift playback dispatches (`Zenith2/Docs/Design/PHYS/Simulation.md` → "Origin shift"; PHYS conforms to this rule's threshold, quantum and interval); AI its tile origins (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift"); AUDIO its listener frame (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Origin shifts") (rows ST8 to ST10) |
| scripts | the `READ_WORLD` bindings read `Zenith.WorldOrigin` itself, so a script in phases 2 to 10 of a rebase tick converts local positions with the shifted O ("Script bindings" in PublicAPI.md) |
| STREAM | its own state is absolute (anchors, source absolute positions), so it reacts only through the HLOD extractor's UPDATE records |

The rebase frame costs one full-state extraction and one O(entities) shift playback, at most once per N ticks (0.5 s at the defaults): the extraction is the full-state case RPROD already budgets (the frame after a cancelled frame, Z2-B-GEOM-003 for its uploads) and both land in the rebase-tick hitch row Z2-B-SHIP-016.

## Teleports and readiness

### Teleports

A source is teleported this tick when its `TeleportPending` (property 14) is true, or when its `PreviousTick` (property 13) is the previous tick and `|P − P_prev| > stream.teleport.distanceMetres` (256), with `P_prev` the absolute position its properties 10 to 12 hold. STREAM's lifecycle listener sets property 14 true on the source entity, through its `Zenith_WorldPlaybackAccess`, during the playback of `Scene.Teleport` that dispatches `LIFECYCLE_EVENT_TELEPORTED` for it (`Zenith2/Docs/Design/SCENE/Transforms.md` → "Teleport"); step 5 reads properties 10 to 14, then writes this tick's P into 10 to 12, the tick into 13 and false into 14. All five are serialized `PHASE_INPUT` properties of the source's own component, so a capture holds exactly the previous position and the pending teleport the next tick reads, and a restore infers neither a teleport the uninterrupted run did not see nor misses one it did. A source that was disabled, absent or ignored (beyond the sixteenth) in the previous tick has a stale `PreviousTick` and gets no distance test in its first tick back. A teleport increments the source's `TeleportSequence`, sets its readiness false until "Readiness" below says otherwise, and for `stream.teleport.criticalTicks` (60) ticks raises the presentation priority of the source's REQUIRED cells' boosters to CRITICAL ("The streaming service"). The simulation-side algorithms need nothing special: the bands move with the source in the same tick, unloads and loads follow at the caps, and the required cells around the destination sort first.

### Readiness

A source's `MissingRequiredCells` is the number of cells in its grids' REQUIRED band (from that source alone, after closure of its own required cells) that are not ACTIVE; `Ready` is `MissingRequiredCells == 0`. Both are written every tick in step 12 and are simulation state, readable by scripts. A game shows a loading screen or holds a pawn's input while its source is not ready (GAME, row ST18); STREAM itself never pauses simulation. `stream.residency_miss_ticks` counts ticks in which the primary source is not ready (the cells part of Z2-B-STREAM-005).

### Teleport-to-ready measurement

The service records the wall-clock time (`Zenith_Clock`, presentation) of the frame in which a source's `TeleportSequence` changed and of the first subsequent frame in which its `Ready` is true, and publishes the difference as `stream.teleport.last_to_ready_us` (MP-STREAM's cells part of Z2-B-STREAM-004). It reads only committed simulation values and writes no simulation state.

## The streaming service

### Service node declaration

```cpp
Zenith_EpilogueStageDesc xService;
xService.m_szName = "Streaming.Service";
xService.m_pfnRun = &Zenith_StreamingService_Run;
xService.m_pUserData = pxPresentationState;
xContext.AddEpilogueStage(xService)
	.ReadResource(Zenith_StreamingWorldState::xTYPE_ID)
	.ReadResource(xSCENE_ACTIVATION_QUEUE_TYPE_ID)
	.ReadResource(xWORLD_ORIGIN_TYPE_ID)
	.Read(Zenith_StreamingSourceComponent::xTYPE_ID)
	.ReadAnyRow(xWORLD_TRANSFORM_TYPE_ID, axPOSITION_AND_ROTATION);

Zenith_ExtractorDesc xExtract{ "Streaming.ExtractHLOD", &Zenith_StreamingExtractHLOD, pxPresentationState, 2 };
xContext.AddExtractor(xExtract)
	.After("Streaming.Service")
	.ReadResource(Zenith_StreamingWorldState::xTYPE_ID)
	.ReadResource(xWORLD_ORIGIN_TYPE_ID);
```

The service runs once per frame in the epilogue (stage 12), after the frame's ticks; it reads committed simulation state and owns the per-world presentation state. It never records commands and never writes a component or a resource: `Zenith.Streaming.State` is written only by `Streaming.Update` (the one system writer) and by STREAM's playbacks, listener and hook on the sync-point node and the tick-boundary handler ("Thread affinity per type" in the entry document), and the service reads its counters as cumulative totals and differences them against the previous frame's copy it keeps in its presentation state. Its time per frame is capped at `stream.service.frameBudgetUs` (1,000 µs, from Z2-B-STREAM-002): each sub-step checks the elapsed time between work items and stops, resuming next frame in the same order.

### Sub-steps, in order

| # | Sub-step | Work |
|---|---|---|
| 1 | Memory level | read the consumer's atomic level; apply "Memory pressure and transition peaks" |
| 2 | Boosters | for each cell in REQUESTED, LOADING or CANCELLING (the preparation is loading its scene), keep one `RequestLoad` of the cell scene handle at `ASSET_PRIORITY_HIGH` (CRITICAL for the REQUIRED cells of a source within its teleport critical window); ASSET deduplicates it with SCENE's own request and raises that request's priority (`Zenith2/Docs/Design/ASSET/Loading.md` → "Request deduplication and priorities"); destroy boosters of cells in any other state within this frame ("Cancellation at every stage", the last live request) |
| 3 | PSO groups and gates | "PSO groups and the preparation gate"; the double-residency reservation of a required load under a displayed proxy ("Double-residency reservation") |
| 4 | Prefetch | for cells in band PREFETCH that are UNLOADED (from the HLOD-driving source's lookahead disc, below), keep one `RequestLoad` of the cell scene handle at `ASSET_PRIORITY_BACKGROUND` with `ASSET_LOAD_FLAG_PREFETCH_SOFT_DEPENDENCIES` clear (hard dependencies follow the scene; ASSET's BACKGROUND reserve caps prefetch at half the decode queue, row AS18); cancel prefetches of cells that became wanted (SCENE's own load takes over the resident asset) or left the band; at most 8 new prefetches per frame, ordered by the band key |
| 5 | HLOD residency | "HLOD selection and extraction", residency half |
| 6 | Counters | publish the gauge counters and the per-frame differences of the cumulative totals of `m_xCounters` against the copy kept from the previous frame; the teleport-to-ready measurement |
| 7 | Debug view | when `stream.debug.view` names a grid and the build is not Shipping, write debug-draw records for the extractor |

### Prefetch disc

The prefetch band uses the simulation's bands; the service additionally extends it along motion: for the HLOD-driving source with velocity v (from its last two positions, presentation-side), the cells within `Rpre · s` of `P + v · stream.prefetch.lookaheadSeconds` (2 s) are prefetch candidates too. This reads only committed state and changes only presentation.

### Request state machine

| State | Meaning | Next |
|---|---|---|
| NONE | no request | REQUESTED (sub-step 2 or 4 wants it) |
| REQUESTED | an operation handle and an asset handle held | RESIDENT (the operation SUCCEEDED), NONE (cancelled, failed) |
| RESIDENT | the asset is ACTIVE and held | RELEASING (no longer wanted) |
| RELEASING | handles destroyed this frame | NONE |

A FAILED prefetch is not retried by the service (the simulation's own load will surface the failure deterministically).

## HLOD selection and extraction

<!-- elaboration -->
HLOD as a presentation-only quadtree of up to three levels with residency hysteresis and module instance keys is a design choice of this family ("Design choices at a glance" in the entry document).

### Driving source and scaling

The cut of every grid is computed from one source D: the lowest-simulation-ID enabled source with `DrivesHLOD` true; when none has it, the primary source; when there is no primary, the lowest-simulation-ID enabled source; with no source, nothing is displayed. `DrivesHLOD` defaults to false, so a preload marker or a capture camera never drives the cut by accident. Every HLOD distance (each level's maximum distance and refine distance, and the residency hysteresis) is multiplied by D's clamped radius scale s, exactly as the bands' radii are, so a source with a small radius scale sees the same nesting at a smaller scale and no ring between its scaled radii is left without either content or a proxy.

### The cut

Every frame, for each grid with HLOD levels, from D's absolute XZ position:

1. Start at the grid's top-level nodes (`GetTopHLODNodes`).
2. For node N of level k with region R (its covered base cells) at distance d (from D to R's XZ rectangle, binary64): if `d > maxDistance(k) · s`, N is not displayed and nothing below it is examined (beyond HLOD range: neither a proxy nor a gap).
3. A base cell holds content when it is ACTIVE or ACTIVATING (an activation's entities exist from its sync point, which precedes the epilogue). Every other state holds none: UNLOADING (its destroys played at the sync point), CANCELLING, LOADING, QUEUED, REQUESTED, FAILED, FAILED_PERMANENT and UNLOADED.
4. Level k ≥ 2: if any base cell of R holds content, N is not displayed (it would overlap real content); recurse into its children. Else if `d ≥ refine(k) · s`, display N when it is READY; when N is not READY, recurse into the children. Else recurse into the children.
5. Level 1 (R is the node's one cell): when the cell holds content, N is not displayed; otherwise N is displayed when READY, whatever d (a level-1 node is reached only inside its parent's refine distance, or as a top-level node inside its own maximum distance), and is one gap when not READY. Level 1's refine distance (the activation radius) is stored for the format's regularity and not read by the cut.
6. After a recursion, every base cell of R that holds a cell, holds no content and is covered by no displayed node (a cell without a level-1 node, or one whose node is not READY) counts one gap in `stream.hlod.gaps`.

So a cell the source approaches keeps its level-1 proxy through REQUESTED, LOADING, QUEUED and the tick of its activation, and loses it exactly when its content exists: within every top-level node's maximum distance, each base cell that holds a cell is covered by exactly one of its content, one displayed proxy, or a counted gap. A node suppressed by a refused double-residency reservation ("Double-residency reservation") is treated as not READY.

### Residency

A node is in residency range when `d ≤ (maxDistance + stream.hlod.hysteresisMetres) · s`; its meshes are requested (`RequestLoad`, `ASSET_PRIORITY_LOW`; `NORMAL` for nodes of the current cut) when it enters range and released when it leaves `(maxDistance + 2 × hysteresis) · s`. A node is READY when every mesh is `FLUX_MESH_RESIDENCY_RESIDENT`; a FAILED mesh makes the node permanently not READY for the session (`stream.hlod` log once), and the cut falls back to its children or a gap. HLOD state machine: ABSENT → REQUESTED (in range) → READY (all resident) → DISPLAYED (in the cut) → READY (out of the cut) → ABSENT (out of range).

### Extraction

`Streaming.ExtractHLOD(Zenith_SystemContext&, Flux_RenderWorldWriter&, void*)` writes the displayed set as instance deltas under module keys:

- Key: `Flux_MakeModuleInstanceKey(Zenith_Hash64(world ID bytes ‖ node index (uint32 LE) ‖ mesh slot (uint8)))`, never serialized (D-SEL-HASH, open, Stage 1).
- ADD for every (node, mesh) entering the cut, REMOVE for every one leaving, in ascending key; nothing for unchanged nodes, except UPDATE for every displayed node when the rebase count changed (their local transforms moved by −S; RPROD translates their previous transforms with the rest, row ST7).
- Transform: translation `(float)(nodeAnchor − O)`, identity rotation and scale; bounds the mesh's `GetBounds()` translated; flags `FLUX_INSTANCE_FLAG_CASTS_SHADOW` from the node's flag; mesh ID `Flux_MeshResource::GetID()`.
- `IsFullExtraction()`: every displayed (node, mesh) as an ADD, no REMOVE ([Z2-N-REND-015]).
- A refused write (`FLUX_RENDER_WORLD_ERROR_SLOT_FULL`) keeps the record pending for the next frame, as the engine extractors do.

HLOD is presentation only: no entity, no simulation cost, no save; under memory pressure it is shed first after prefetch.

## PSO groups and the preparation gate

<!-- elaboration -->
The trigger is a state the service observes. The service runs in the epilogue, after the frame's ticks, so it never sees a cell in REQUESTED (a load recorded in phase 1 is LOADING by that tick's sync point, T2); it sees slots. In sub-step 3, for every slot of the world's pool whose simulation bit is set, whose entity table is filled and whose "frame the PSO group was requested" is 0, whether the load was issued live (step 9) or adopted at a restore (R4), the service calls `Flux_PSOPrecacher::RequestGroup(FLUX_PSO_GROUP_STREAMING_CELL, Zenith_MakeStreamingGroupID(cell scene).m_ulValue)` (`Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API"), holds the operation handle in the presentation state and stores the current frame index in the slot; that frame starts the timeout clock. The slot's gate opens (`OpenGate`, release store, only once the entity table is filled) when, whichever is first:

| Condition | Why |
|---|---|
| the group's operation completed (SUCCEEDED, or FAILED with `FLUX_PSO_ERROR_UNKNOWN_GROUP` or `FLUX_PSO_ERROR_DEVICE_LOST`) | the cell's PSOs exist or never will on this device |
| `stream.pso.gateTimeoutFrames` (300) frames passed since the frame stored in the slot | a slow group never stalls a required cell indefinitely; the fallback rules of `Zenith2/Docs/Rendering/ShaderSystem.md` → "Missing-PSO fallback" apply to any miss |
| `RequestGroup` returned `FLUX_PSO_ERROR_QUEUE_FULL` for 30 consecutive frames (the stored frame stays 0 while it is refused, and the refusals are counted from the first) | the precacher's 64-request limit is saturated |
| no renderer exists (headless), live load | opened at `Acquire` in step 9 |
| no renderer exists (headless), restored load | opened by R4 right after `FillEntityTable` |

The preparation waits at the gate before BUILDING (row ST2), so a cell's scene publishes, and therefore activates, only after its PSOs are created or the timeout passed: the activation never hitches on a PSO compile ([Z2-N-REND-202]). Gate timing changes only when a preparation completes, which reaches simulation solely through SCENE's logged publication (`Zenith2/Docs/Design/STREAM.md` § "Determinism obligations"). A slot whose simulation bit clears (the load ended) has its group's operation handle destroyed by the service in the same sub-step.

## Memory pressure and transition peaks

### Shedding

STREAM implements the Streaming row of `Zenith2/Docs/Architecture.md` → "Shedding per subsystem", with this implementation detail ([Z2-N-ARCH-822]); every action is presentation, never simulation ([Z2-N-ARCH-812]). The CRITICAL cell releases presentation-held cell residency only, because unloading a simulation cell for memory would make which cells are ACTIVE depend on memory pressure, which [Z2-N-ARCH-812] forbids; core-doc item X226 (a) reworded the row to say so ("release presentation-held cell residency (prefetched cell data, HLOD meshes, boosters) outside the required band; no simulation cell is unloaded by memory pressure"), and the table below quotes the row's words as they read before that rewording, beside STREAM's mechanism for each:

| Level | Row's words | STREAM's mechanism | Counter |
|---|---|---|---|
| WARNING | halve the prefetch radius | the service's prefetch candidates use `Rpre/2 · s` (never below the KEEP radius) and the lookahead disc is dropped | `stream.shed.level` |
| WARNING | cancel `IO_PRIORITY_BACKGROUND` requests | every prefetch request is cancelled; HLOD meshes outside the current cut are released | `stream.shed.prefetch_cancelled`, `stream.shed.hlod_evicted` |
| CRITICAL | cancel every prefetch | as above, and no new prefetch is issued | same |
| CRITICAL | evict cells outside the required radius (persistent entities are kept) | every presentation-held residency of a cell outside the REQUIRED band is released: prefetched scene data, HLOD meshes of every node except the top-level nodes of the current cut (a grid with one or two HLOD levels keeps its top level, so a region keeps its coarsest proxy whatever the level count), and boosters; no simulation cell is unloaded (row ST17, X226) | `stream.shed.hlod_evicted`, `stream.shed.cells_evicted` (cells whose presentation residency was released) |
| CRITICAL | admit only `IO_PRIORITY_CRITICAL` requests | boosters are issued only for REQUIRED cells (at CRITICAL); prefetch and HLOD requests are not issued | `stream.boosters.active` |

Each response is applied within one frame of the level change and reverted one frame after the level falls ([Z2-N-ARCH-821]). STREAM's shedding case row joins `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (`axMEMORY_SHEDDING_CASES`) in STREAM's shedding ticket, as every subsystem's does (Z2-R-CORE-152).

### Resident bytes

`Zenith_StreamingMemoryConsumer::GetResidentBytes` sums (relaxed atomics updated by their owners): every bound world asset's `GetResidentBytes`, every world state's arrays, every snapshot pool (live and retiring), every presentation state's arrays and handle pool. Cell content is reported by its kinds' consumers (SCENE's preparation reservations, GEOM, ENV under `MEMORY_SUBSYSTEM_STREAMING`), never twice. The snapshot pool is the largest fixed share: `uSTREAMING_SNAPSHOT_SLOTS` (112) slots of 64 bytes plus an 8,320-byte entity table each, 0.9 MiB per attached world, counted against Z2-B-STREAM-001.

### Double-residency reservation

A cell whose region is covered by a displayed HLOD node holds both the proxy and the cell's data while its data loads. [Z2-N-ARCH-815]'s deferral applies to presentation prefetch only; a simulation-required load (every load STREAM issues is one, decided in step 9) is never deferred and never waits, because deferring it would make simulation depend on memory pressure (Z2-D-019). [Z2-N-ARCH-815] states this since core-doc item X226 (b), and this block is STREAM's mechanism:

- **Prefetch.** Before issuing a prefetch of a cell under a displayed node, the service calls `ReserveTransitionPeak(TRANSITION_PEAK_STREAMING_DOUBLE_RESIDENCY, estimatedBytes(cell))` (`Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Reservation algorithm"); a refusal defers that prefetch to a following frame and counts `stream.peak.deferred`; the reservation is held by the request and released (destroyed) when the node leaves the cut or the prefetch is cancelled.
- **Required load.** In sub-step 3, for every cell in LOADING or CANCELLING whose region is covered by a displayed node and that holds no reservation, the service reserves `estimatedBytes(cell)` at once, without deferring anything. A granted reservation is held for the whole load: until the cell holds content (its proxy then leaves the cut and the double residency ends) or its load ends otherwise (T18, a failure). A refused reservation releases the covering node's meshes at once and suppresses that node for the rest of the load (the cut treats it as not READY, so its region is a counted gap until the cell activates or a child proxy covers it); `stream.peak.proxy_released` counts it. Either way the peak row Z2-B-CORE-006 is never exceeded by double residency.

## Save and restore

### What is saved

Everything a subsequent tick of STREAM reads survives in components and in SCENE's additive set, which SAVE saves like any other world state (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Persistent world and entity state"; row SV10):

| State | Where | Saved by |
|---|---|---|
| deltas of unloaded cells | `Zenith.Streaming.CellDelta` property 4 on each cell's holder, keyed by cell scene GUID | ENTITIES |
| every cell's persisted status: state, instance, retries used, capacity and transient failures, retry tick, unload due tick, the flags (release pending, preempted, references bound, state applied, unload in progress, unload of a QUEUED instance, re-home pending), committed and pending tombstones, delta merge pending | `Zenith.Streaming.CellStatus` on the same holder ("Persisted cell status"; a cell with no holder has the default status) | ENTITIES |
| deaths in active cells | the committed and pending bits; the absence of the destroyed entities (the capture's absence rule) | ENTITIES |
| which cells are ACTIVE and their instances | `CellStatus` properties 1 and 2; `Zenith.Streaming.CellAnchor` on each active cell's root; SCENE's additive set | ENTITIES; SCENE's `Zenith.Scene.AdditiveSet` participant |
| loads in flight (LOADING, QUEUED) | SCENE's pending-load records with their hook tags | SCENE's participant re-requests them at restore (row SV10) |
| sources, with their previous absolute position, its tick and a pending teleport (`Zenith.Streaming.Source` properties 10 to 14) | components | ENTITIES |
| region policies, flags, containing cells and evaluation ticks (`Zenith.Streaming.RegionBound` 1 to 4) | components | ENTITIES |
| the world reference, its binding state and its override slots (`Zenith.Streaming.WorldRef` 1 to 4) | components | ENTITIES |
| the origin and the last rebase tick | `Zenith.WorldOrigin` | SCENE's world state |

STREAM therefore registers no `Zenith_SaveParticipant`: what `Zenith.Streaming.State` holds is per-tick (bands, pins, ordering distances, the admission table), a cache of the saved properties above (rebuilt from them), restore-only (the restored-load inbox), part of a load in flight (hook tags, slots, the discard inbox: COHERENT captures below), the commit-refused inbox (the one exception below), presentation, or diagnostic (cumulative counters, the tick of a cell's last state change, log rate limits; these restart at a restore and are not simulation state). STREAM excludes no entity from snapshots (`CollectExcludedEntities` stays empty): SCENE's activation is all-or-nothing within one sync point, and the phase-11 snapshot never sees a half-activated or half-unloaded cell.

**Fidelity.** Fidelity is a property of each capture (`Zenith2/Docs/Design/SAVE.md` → "Reproducibility boundaries"; rulings Q70 (a) and Q82 (b)). For STREAM a capture is COHERENT, not EXACT, in exactly three cases, each reported by SCENE's `Zenith.Scene.AdditiveSet` participant (row ST2 (g)), because STREAM has no participant:

| Capture | Why it is COHERENT |
|---|---|
| a cell's instance LOADING or QUEUED (an ACTIVATING cell whose SCENE activation was rejected and a flagged UNLOADING cell whose unload was rejected included) | the re-requested load publishes at a tick the uninterrupted run did not see (SAVE.md's own row for additive instances) |
| a cancelled cell's discarded publication still pending (a cell CANCELLING, its instance UNLOADED) | SCENE does not save the UNLOADED record, so the restored cell is UNLOADED at once, while the uninterrupted run waits for the discard |
| a tagged load whose commit SCENE refused at a sync point of the capture tick | the refusal reaches STREAM only through `OnLoadCommitted` on the sync-point node, which carries no write access to world state, so it waits in the commit-refused inbox until the next update, after the snapshot: the restored cell reads its holder's State REQUESTED with no instance and takes T4 (UNLOADED, loads at once) where the uninterrupted run takes T3 (FAILED, with a backoff or a retry consumed). Nothing STREAM saves can tell the two apart, which is why this state alone cannot be persisted |

Every other capture is EXACT for STREAM. With each cell UNLOADED, REQUESTED with its load buffer rejected, ACTIVATING, ACTIVE, UNLOADING, FAILED (with or without a release pending) or FAILED_PERMANENT, and with any unload delay running, any preemption, override, capacity or transient backoff, retry count, committed or pending tombstone, pending delta merge, region flag, source position or pending teleport, the rebuild puts back the record the uninterrupted run holds after its last update and the saved properties its next update reads; the restored run's step 4 therefore applies the same rows and its steps 5 to 14 record the same actions, deferred re-homes and holder prunes included. A replay resumed from an EXACT capture (`Zenith_RuntimeReplay_ResumeFromSave`, `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations") therefore reproduces the recording's per-tick hashes, taking every publication from the log (`Test_StreamingIntegration_ExactRestoreWithFailedAndDelayedCells`). A live continuation equals an uninterrupted live run only while no asynchronous request is in flight at the capture or issued in the compared window: every cell load publishes at a tick its preparation's timing decides, and two live runs whose asynchronous work completes at different moments may diverge ([Z2-N-SCRIPT-338]; `Zenith2/Docs/Design/SAVE.md` → "Determinism obligations"), so a streamed world is compared against its recording, never against a second live run. A capture is EXACT for the whole world only when nothing else makes it COHERENT (behaviour state with a suspended `Run`, a COHERENT module participant: SAVE.md's table). One condition outside SAVE's reporting also voids the claim: a holder that gameplay destroyed and STREAM has not yet re-spawned ("Persisted cell status", holder discovery step 3), whose cell restores with its default status; destroying an engine-owned holder is a misuse STREAM counts (`stream.persistence.holders_respawned`) but cannot report.

### Rebuild

`m_bNeedsRebuild` is set at attach and by the reset signal (`GetPreviousReadVersion() == 0`, which ECS raises after `RestoreSimulationState`, `Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header"). Step 1 of the update then runs, using SCENE's enumeration of live instances (`Zenith_AdditiveSceneSet::GetLiveInstances`, ascending instance ID, row ST2 (f)), each instance's scene GUID (`GetScene`) and every holder of the holder query (every row, since the previous read version is 0):

| # | Rebuild step |
|---|---|
| R1 | Re-evaluate the binding. The world reference whose `BindingState` (property 4) is BOUND or UNBINDING, the lowest simulation ID when several are, is the binding, in that state, with its world asset (a hard dependency of its scene, so ACTIVE); the arrays are sized for it and nothing is diffed or unloaded. With none, step 2's evaluation of "World binding" decides from NONE as at attach |
| R2 | Set every cell UNLOADED with every counter, tick and flag 0; clear every per-persistent-entry flag; clear the commit-refused and discard inboxes (empty after a restore in any case) but not the restored-load inbox; clear the admission table |
| R3 | Map every holder of the bound world ID to its cell (holder discovery step 1, with its duplicate rule) and copy its `CellStatus` into the cell's record: state, instance, the three counters, the two ticks, the flags (bit 6 into the re-home cache, counted in `m_uRehomePendingCells`), the committed and pending bits (into `m_auPersistentFlags` bits 0 and 1) and "delta merge pending". A persisted LOADING, QUEUED or CANCELLING state becomes UNLOADED here, and so does a persisted ACTIVATING or UNLOADING state whose persisted instance SCENE now reports LOADING or QUEUED: an ACTIVATING cell whose SCENE activation buffer was rejected in the capture tick (its instance was QUEUED, so SAVE re-requested it), or an UNLOADING cell flagged "unload of a QUEUED instance" whose unload was rejected. A capture holding any of these is COHERENT (row ST2 (g) (1)); the cell becomes UNLOADED with instance 0 and flag bits 2 to 5 clear, keeps its counters and ticks, and is claimed by no step, so R4 adopts its re-requested load. Every other state is kept, and the next step 4 resolves it against SCENE's restored set exactly as the uninterrupted run's next step 4 does (a persisted REQUESTED with no live instance takes T4, an UNLOADING cell whose instance is gone takes T16, a FAILED cell with a release pending takes T32 or T33, an ACTIVE cell whose anchor is gone takes T28). For an ACTIVE, ACTIVATING or UNLOADING cell whose persisted instance SCENE reports ACTIVE, record the anchor handle (the root carrying `Zenith.Streaming.CellAnchor` with this cell scene and world ID) and fill the persistent handle table with `FindByPersistentID` |
| R4 | For every live instance SCENE reports LOADING or QUEUED whose scene is a cell scene of the bound world, in ascending instance ID: when the restored-load inbox holds the instance with a slot and its cell is claimed by no earlier step, adopt it: the cell is LOADING with that instance and tag, `SetCell(slot, cell)`, `FillEntityTable` from the cell's restored delta and committed bits, and, when no renderer exists, `OpenGate` at once (with a renderer the service's rule of "PSO groups and the preparation gate" applies). Otherwise ("Restored loads beyond the pool") record the instance's release and count `stream.restore.unadopted`; when the instance's triple holds a slot (a second instance claimed for one cell, or any other triple R4 does not adopt), neutralise the slot first: `FillEntityTable(slot, table)` with a zero-filled `Zenith_StreamingEntityTable` in the scratch arena (no tombstone, no amendment), `OpenGate(slot)` and `ReleaseSimulation(slot)`, leaving the slot's cell `uSTREAMING_INVALID_CELL`, so the cancelled preparation reaches its end and releases the slot's last bit at `OnPreparationEnded`, and its discarded publication finds no slot or a slot with no cell, which the hook ignores. A live ACTIVE instance of a cell scene that no holder claims and whose roots carry that cell's anchor of this world ID is adopted as ACTIVE (its holder was destroyed; holder discovery step 3 re-spawns it); one without such an anchor is the game's own instance of the scene, left alone and counted in `stream.restore.unclaimed_instances`; a second ACTIVE instance claimed for one cell is released and counted in `stream.cells.unexpected_state`. R4 ends by clearing the restored-load inbox |
| R5 | The orphans and re-home marks of "Cell identity across re-cooks": decode every holder of the bound world ID; count the holders naming no cell of the bound world (`stream.persistence.orphan_deltas`, kept and never applied); set `CellStatus` flag bit 6, directly, on every holder that has records to re-home and lacks it (a save written against older content; never clearing a set bit, which only step 14 clears, so the restored run's visiting order equals the uninterrupted run's), and rebuild `m_uRehomePendingCells` and `m_uRehomePendingOrphans`. R5 moves no record: step 14 does, in this tick and the ticks after it, exactly as in the uninterrupted run |
| R6 | Write back to its holder every persisted field R2 to R5 changed (an adopted cell's State LOADING and instance; a persisted state R3 made UNLOADED; a re-home bit R5 changed), directly, so the holder equals the rebuilt record before step 2 runs |

R4's releases are recorded as ordinals before step 8's ("Action buffers and rejection"); the rest of the tick's update runs normally, so a cell R4 released is UNLOADED and loads afresh, at the earliest at the next tick, when it is wanted. A restored cell whose dependency is not ACTIVE is not possible (a saved ACTIVE cell's dependencies were ACTIVE at the snapshot); if content changed between save and load, the next tick's closure unloads what is no longer valid. A rebinding at run time (not a restore) runs no rebuild: "World binding" sizes the arrays, runs R5's marking pass (so records a re-cook moved are re-homed by step 14) and holder discovery maps the holders the world already has.

### Restored loads beyond the pool

Adoption is bounded and never silent. The live run keeps LOADING + CANCELLING + QUEUED cells within `stream.maxConcurrentCellLoads` + `stream.maxQueuedCells` at every phase 11, rejected unloads included (the proof of "Caps"), so a save holds at most that many tagged loads in flight, and the pool's `uSTREAMING_SNAPSHOT_SLOTS` = `uSTREAMING_MAX_CONCURRENT_LOADS` + `uSTREAMING_MAX_QUEUED_CELLS` = 112 slots cover every save written by any build, whatever its configuration within the ranges; the restoring build may then hold more in-flight loads than its own caps, and issues none until they fall below them ("Caps", "After a restore"). A save holding more (corrupt or hostile data) cannot overflow anything: `OnLoadCommitted` appends the pair to the restored-load inbox (capacity 112) without a slot when the pool refuses (`stream.snapshots.pool_full`), drops it when the inbox is full (`stream.restore.unadopted`), and R4 releases every LOADING or QUEUED cell-scene instance it did not adopt. An unadopted load either holds no slot, so its preparation answers "skip nothing, amend nothing, gate open" for an unknown tag, or holds a slot R4 neutralised (a zero-filled entity table, the gate open, the simulation bit released, the cell `uSTREAMING_INVALID_CELL`), which answers the same; either way it is "not admitted" (it is in no admission table), so it can neither activate nor resurrect a tombstoned entity. Its release cancels it, its preparation frees the slot's last bit at `OnPreparationEnded`, its discarded publication finds no slot or a slot with no cell and is counted under `stream.hook.unknown_tags` and ignored, and its cell, UNLOADED, is loaded afresh with its tombstones when wanted. A re-request SCENE refuses at restore reaches the hook as `OnLoadCommitted(tag, 0, code)` with a tag not in the table; it appends nothing and counts `stream.restore.unadopted` (row ST2 (j)): the refused instance does not exist, so nothing is left to release. No duplicate-instance loop is possible: the released instance is UNLOADED, hence not live, before the cell's next load commits.

## World transitions and shutdown

- **World transition.** A transition destroys the old world: STREAM's `DetachWorld` drops the world state, destroys the presentation state's handles (each destruction requests cancellation), clears the world-state pointer of the pool and retires the pool while preparations end. In-flight cell preparations of the old world are discarded by SCENE unpublished (`world.async_discarded`). The new world binds fresh at its first update.
- **Shutdown.** Step 4 (ASSET `BeginShutdown`) cancels every request; SCENE's preparations end and call `OnPreparationEnded`; step 6 detaches every world and destroys the module instance, which destroys every retiring pool after `ZENITH_CHECK`ing that every slot is FREE, and unregisters the kind loader.
- **Module disabled** (the game's `game.json` omits `Streaming`): no STREAM node exists; a `Zenith.Streaming.WorldRef` component in content is then an unknown type handled by SCENE's unknown-type policy.

## Replay and determinism

| Input of a simulation decision | Deterministic because |
|---|---|
| components, `Zenith.WorldOrigin`, the tick index | simulation state |
| SCENE's additive set states | they change only at sync points and at tick-boundary publications of source kind `ASYNC_SOURCE_SCENE_LOAD` (value 8 of SCRIPT's source registry), which are logged; a replay holds a tick until a recomputed preparation is ready ([Z2-N-SCRIPT-337]) |
| the discarded publications of cancelled loads | the publication kind and payload are decided at publication, not by the thread that parks the result: SCENE's per-source resolver turns every drained result of an instance whose record carries "discard requested" into `PUBLICATION_KIND_GENERIC_RECORDED` with `{instance, SCENE_ERROR_CANCELLED, 0}` before SCRIPT logs it (rows ST2 (b), ST21), whatever the preparation produced and whenever it parked, so the logged kind never depends on thread timing; a replay publishes each at its recorded tick without waiting for a preparation that the replay itself cancels, and drops the replay's own local completion of that ID (row ST21 (c)) |
| the hook's discard calls and commit calls | made at the same publications and playbacks |
| the persisted cell status, the source and region properties, the world reference's binding state and overrides | saved components ("Persisted cell status"), so a restore from an EXACT capture resumes them exactly |
| the cooked world | content, part of the replay's content hash |
| the catalogue answer of `Acquire` | content |
| simulation-affecting configuration keys | written into the replay header |

Nothing else reaches simulation: not the snapshot pool's slot choice, not the gate timing (only through publication timing), not asset residency, memory level, PSO state, wall-clock time or worker count. `Test_StreamingDeterminism_ReplayAcrossWorkers` replays a recorded traversal at 1, 2, 8 and all workers on each ECS backend and asserts equal per-tick world hashes; `Test_StreamingDeterminism_MemoryPressureDoesNotChangeTicks` forces WARNING and CRITICAL mid-run and asserts the hashes equal those of a run without pressure; `Test_StreamingDeterminism_CancelledLoadsReplay` records the 1,000-cycle cancellation stress and replays it at 1 and all workers with equal per-tick hashes, which only holds because every discarded publication is recorded; `Test_StreamingDeterminism_DiscardResolvedAtPublication` forces the one interleaving the stress may miss (a preparation that parks its own result after one boundary and before the unload's playback of the next tick) and `Test_StreamingDeterminism_ReplayReleasesLateLocalDiscard` the replay whose own cancelled preparation completes after the recorded discard was published.

## Headless runs

In a run without a device (the headless test host, `Zenith2/Docs/Architecture/PlatformLayer.md` → "The headless test host"; the Streaming row of `Zenith2/Docs/Architecture.md` → "The module list" says "unchanged"), the start context's renderer is null: the gate of a live load opens at `Acquire` and that of a restored load in R4, no PSO group is requested, the HLOD extractor writes nothing (the render subgraph does not run) and HLOD residency requests are not issued, so headless runs measure simulation-side streaming exactly. Every simulation algorithm runs unchanged; cell assets of GPU kinds load in their headless form (ASSET's validate-only path). The integration suites run headless (`Zenith2/Docs/Design/STREAM/Tests.md` → "Integration tests").

## Complexity

| Work per tick or frame | Bound | At the defaults |
|---|---|---|
| band enumeration | Σ over sources and their grids of min(4,096, (2⌈Rpre·s / c⌉ + 1)²) | one source, two grids (128 m, 256 m), Rpre 640/768: 121 + 49 cells |
| closure | wanted cells × dependencies (≤ 32) | < 2,000 edges |
| observation | cells in transition | ≤ 160 |
| sorts | n log n, n ≤ wanted cells | < 500 |
| captures | persistent entities of unloading cells × configured types | ≤ 8 cells × 1,024 entities |
| regions | region-bound entities (one hash probe each) | the game's count |
| service | boosters + prefetch candidates + HLOD nodes in range, capped by `stream.service.frameBudgetUs` | ≤ 1 ms |
| HLOD cut | nodes visited ≤ nodes in range | hundreds |
| holder discovery | holder rows changed since the previous read (every holder once per restore) | the holders spawned last tick, ≤ `stream.maxNewRequestsPerTick` + 1 |
| re-homes (step 14) | while any holder carries bit 6: a scan of the cell records' cached bits and, while an orphan carries it, of the holder query's rows; at most `stream.unload.maxCellsPerTick` delta decodes and re-home actions per tick | zero ticks in a world whose content never changed under its saves |
| holder prunes (step 14) | the cells that entered UNLOADED this tick | ≤ the unloads, cancellations and releases of the tick |
| persisted status writes | one direct write per changed field per cell per step; tombstone writes one per destroy | the cells that changed state this tick |
| memory | world state 56 B × cells + 32 B × cells (cell mirror) + 9 B × persistent entries (handle table and flags); pool 112 × (64 B + 8,320 B); one holder entity per cell whose status is not the default or whose delta holds a record (plus cells whose last prune was rejected, and orphans), its `CellStatus` 288 B and its `CellDelta` (ECS and container memory, counted by ECS, saved by SAVE and hashed per tick when recording) | 65,536 cells: 5.5 MiB plus 9 MiB at the 1,048,576-entry limit; pool 0.9 MiB; holders: one per cell holding persistent state or a failure record, not per cell visited (a cell with no persistent entity is pruned at its unload), so 288 B × the cells with persistent content, at most 18 MiB of `CellStatus` only in a 65,536-cell world whose every cell holds persistent state |

No step is quadratic in the cell count; the full-cell passes are the rebuild and the binding's marking pass (once per restore or binding) and, only while a holder carries the re-home bit, step 14's scan of the cached bits.
