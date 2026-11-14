# TWOD design: simulation

Section file of `Zenith2/Docs/Design/TWOD.md`. It specifies the simulation half of the `Zenith2D` module: where each TWOD system runs in the twelve frame phases, sprite animation (flipbooks, the animator state machine, animation events), cutout skeletons, the tilemap runtime (edits, collision generation), 2D physics behind its backend seam (ingest, stepping, write-back, events, one-way platforms, continuous collision, sleeping, queries, origin shift), the determinism rules every TWOD system obeys, save participation and the module's per-world state. Every algorithm here produces results that are independent of worker count, chunk layout and scheduling (Z2-D-019).

## Scope of this file

| Topic | Heading |
|---|---|
| nodes, phases, declared access | "Phase placement" |
| flipbook frames | "Flipbook evaluation" |
| state transitions | "Animator state machine" |
| frame events | "Animation events" |
| reloading animation sets | "Animation hot reload" |
| cutout skeletons | "Cutout skeleton evaluation" |
| tilemaps at run time | "Tilemap runtime", "Tile edit playback", "Tilemap collision generation", "Tilemap hot reload" |
| the 2D physics model | "Physics 2D model" |
| the backend seam and its implementations | "Physics 2D backend seam" and its four subsections |
| one tick of 2D physics | "Physics 2D ingest", "Physics 2D external writes", "Physics 2D pending operations", "Physics 2D step", "Physics 2D write-back and sync" |
| events | "Contact and trigger events" |
| special contacts | "One-way platforms", "Continuous collision", "Sleeping" |
| queries | "Physics 2D queries" |
| rebasing | "Physics 2D origin shift" |
| the rules that make it deterministic | "Determinism rules for TWOD" |
| saves | "Save participation" |
| per-world state and memory | "Module world state", "Memory consumer" |

## Phase placement

Every TWOD node is a `Zenith_SystemDesc` registered by `AttachWorld` with `JOB_AFFINITY_ANY` (`Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"`); property lists name the IDs of `Zenith2/Docs/Design/TWOD/PublicAPI.md`; "by name" means resolved from the frozen type registry by stable name at `AttachWorld` and omitted when the type is absent.

| Node | Phase | Writer source | Declared access | Sub-jobs | Cost hint (µs) |
|---|---|---|---|---|---|
| `TwoD.Animate` | 3 Animation | — | `Write` SpriteAnimator 2–4, 6, 7, 9–11; `Read` SpriteAnimator 1, 5, 8; `EmitsEvent` `Zenith.TwoD.AnimationEvent`, `Zenith.TwoD.AnimationFinished` | `ParallelFor` over animator chunks, grain 1 chunk | 150 |
| `TwoD.AnimateSkeletons` | 3 Animation | — | `After("TwoD.Animate")`; `Read` SpriteAnimator 1–7, Skeleton 1–3; `WriteResource` `Zenith.TwoD.Poses` | `ParallelFor` over skeleton chunks, grain 1 chunk | 150 |
| `TwoD.Physics2DStep` | 5 Physics | — | `Write` RigidBody 20–22, Joint 21; `Read` RigidBody 1–11, Collider 1–16, Joint 1–20, Tilemap 1, 6, 8–10, `Zenith.LocalTransform` 1–2 (own row); `ReadAnyRow` `Zenith.WorldTransform` 1–7; `WriteResource` `Zenith.TwoD.Physics`, `Zenith.TwoD.TilemapEdits`; under the Jolt outcome of D-SEL-2DPHYS only, once PHYS's post-signoff patch of row TD10 exists, also `WriteResource` `Zenith.Physics.PlanarWorld` by name ("Candidate J production realisation"); `EmitsEvent` the five physics events | ingest and write-back `ParallelFor` over chunks, grain 1 chunk; the backend step on the node's thread | 1500 |
| `TwoD.Physics2DSyncTransforms` | 6 PostPhysics | `WRITER_SOURCE_PHYSICS_2D` (row TD9) | `Write` `Zenith.LocalTransform` 1–2; `Read` RigidBody 1; `ReadAnyRow` `Zenith.WorldTransform` 1–7; `ReadResource` `Zenith.TwoD.Physics` | `ParallelFor` over chunks | 200 |
| `TwoD.Extract` | 12 (extractor) | — | `Zenith2/Docs/Design/TWOD/Rendering.md → "Extraction"` | `ParallelFor` over sprite chunks | 300 |
| `TwoD.PhysicsDebugDraw` | epilogue stage (stage 12), registered when `Zenith_IsShippingBuild()` is false and the debug-draw recorder exists | — | `ReadResource` `Zenith.TwoD.Physics` | none | 100 |

- Sync-point work: the five command kinds' playback functions ("Tile edit playback", "Animator state machine", "Physics 2D pending operations") run at step 7 of every sync point in merge order, each finding its world's state by `xWorld.GetWorldTag()` (`Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"`).
- The module's lifecycle listener (`Zenith_LifecycleListener`, ZenithScene) has the interest bits `LIFECYCLE_EVENT_DESTROYED` (release the entity's body, pose and overlay ownership at the end-of-tick playback), the teleport event (adopt the teleported local position and rotation, all seven floats, into the body record's written bits, "Physics 2D external writes") and `LIFECYCLE_EVENT_ORIGIN_SHIFTED` ("Physics 2D origin shift").
- Root motion does not exist in TWOD: 2D animation never moves an entity's transform; gameplay moves entities by scripts or by 2D physics.
- `Test_TwoDSchedule_AccessCompiles` builds a world with every TWOD component type and, on the Box2D path, the stand-in `Zenith.Physics.RigidBody` type of `Zenith_TwoDTestForeignBody.h` registered through the test world's registration callback (`Zenith2/Docs/Design/TWOD/Tests.md → "Test support"`), and no PHYS module, and asserts the schedule compiles with the node table above, including a second phase-6 `Zenith.LocalTransform` writer that the test registers with `WRITER_SOURCE_PHYSICS` beside `TwoD.Physics2DSyncTransforms`, so the two writer sources are distinct. Its planar-world case exists under the Jolt outcome only and is added by the J-path ticket (Tests.md "Types components and registration").

## Flipbook evaluation

A flipbook clip is `n` frames (1 … 1,024), each an atlas frame name with a duration `d_i` in milliseconds (1 … 60,000) and an optional event list; the clip has a loop mode LOOP, ONCE or PING_PONG and the state a speed factor `v_s` (0 … 16). With the world's tick duration `Δ` in microseconds (`Zenith_SimulationClock`), the component's `Speed` `v_c` and its `StateTicks` `k`:

1. Elapsed milliseconds: `T(k) = ⌊(double)k × (double)Δ / 1000 × (double)v_s × (double)v_c⌋`, an unsigned 64-bit integer.
2. Period: `D = Σ d_i`; PING_PONG `P = D + Σ_{i = n−2 … 1} d_i` (for n ≤ 2, P = D).
3. Clip time: LOOP `t = T mod D`; ONCE `t = min(T, D − 1)`; PING_PONG `t = T mod P`, mapped to the forward sequence 0 … n−1 then backward n−2 … 1.
4. Frame: the `i` with `prefix(i) ≤ t < prefix(i + 1)` over the cooked prefix sums, by binary search: O(log n).
5. Loop count: LOOP `⌊T / D⌋`, PING_PONG `⌊T / P⌋`, ONCE 0; saturating at 2^32 − 1.

`TwoD.Animate`, per animator row with a resolved set and `Paused` false, in this order: evaluate the state machine for this tick ("Animator state machine"), then `StateTicks += 1`, then compute the frame with `k = StateTicks`, write `CurrentFrame`, `CurrentSprite` (the frame's name) and `LoopCount`, then emit the events of "Animation events". A paused animator writes nothing. A skeletal state writes `CurrentFrame` = 0 and `CurrentSprite` empty; its time is read by "Cutout skeleton evaluation" from the same `T(StateTicks)`.

`Test_Sprite2DFlipbook_FrameFormula` asserts frames, loop counts and the three loop modes at `Δ` = 16,667 µs and 33,333 µs over 10,000 ticks against the formulas computed independently in the test; `Test_Sprite2DFlipbook_SpeedZeroHolds` and `Test_Sprite2DFlipbook_PausedWritesNothing` assert the edge cases.

## Animator state machine

The cooked animation set holds states (each: name, clip, speed `v_s`, loop mode), parameters (name, kind of `Zenith_Animation2DParameterKind`, default) and transitions (each: source state or ANY, target state, up to 4 conditions `(parameter, Zenith_Animation2DConditionOp, threshold)` that must all hold, an optional exit time as a fraction 0 … 16 of the source clip's period, and `bCanTransitionToSelf` for ANY transitions), in authored order.

| State of the animator | Event | Next state | Action |
|---|---|---|---|
| UNSTARTED (`State` = 0xFFFF) | tick with the set resolved | `GetDefaultState()` | `PreviousState` = 0xFFFF, `StateTicks` = 0, parameters set to their defaults, `TriggerBits` = 0 |
| UNSTARTED | tick with the set unresolved | UNSTARTED | nothing; `twod.anim.unresolved_set_delta` |
| in state s | tick, first transition that holds (ANY transitions first in authored order, then s's own) | its target t (t ≠ s unless the transition allows self) | `PreviousState` = s, `State` = t, `StateTicks` = 0, `LoopCount` = 0, clear the trigger bits the firing transition tested; no further transition this tick |
| in state s | tick, none holds | s | nothing |
| in state s | `TwoD.AnimatorPlay` naming a state t of the resolved set, at a sync point | t | when t ≠ s or the payload's restart flag: `PreviousState` = s, `State` = t, `StateTicks` = 0, `LoopCount` = 0 |
| any | `TwoD.AnimatorSetParameter` naming a parameter of the resolved set, at a sync point | unchanged | write the parameter's value; for a trigger, set its bit |
| any | either command naming a name the resolved set lacks, or arriving while the set is unresolved | unchanged | the command is dropped, `twod.anim.command_dropped_delta` |
| any | the set's asset changes (`AnimationSet` written, or a hot reload) | "Animation hot reload" | |
| any | `Paused` true | unchanged | nothing is evaluated |

A condition holds per its operator against the parameter's value: GREATER `v > th`, LESS `v < th`, EQUAL `v == th`, NOT_EQUAL `v != th`, TRUE `v != 0`, FALSE `v == 0`, TRIGGERED the parameter's trigger bit. An exit time `e` holds when `T(StateTicks) ≥ e × period`. Transitions fire instantly: 2D animation has no cross-fade.

- Name resolution is uniform and happens at playback only. Both commands record the name's `Zenith_StringID` (`Zenith_AnimatorPlayPayload`, `Zenith_AnimatorParameterPayload` of `Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_Sprite2DAnimation header"`); the playback function resolves it with `Zenith_SpriteAnimationAsset::FindState` or `FindParameter` against the set the entity's component names at that sync point. A recorder never reads the entity, its component or its asset, so recording is the same whether or not the set is resident, and a script call never raises an error for an unknown name.
- Playback drops a command whose entity is stale or lacks the component, whose name the resolved set lacks, whose parameter kind disagrees with the payload's kind byte (a trigger set through `SetParameter`, a value written to a trigger), or that arrives while the set is unresolved; each drop counts `twod.anim.command_dropped_delta`.
- Several commands for one entity in one sync point apply in merge order; the last `AnimatorPlay` wins.
- `Test_SpriteAnimator_Transitions` covers every row of the table; `Test_SpriteAnimator_AnyBeforeOwn`, `Test_SpriteAnimator_OneTransitionPerTick` and `Test_SpriteAnimator_TriggerConsumedOnlyByFiringTransition` cover the ordering rules; `Test_SpriteAnimator_CommandsResolveAtPlayback` records commands for an entity whose set is not resident, then resident, and with an unknown name, and asserts the three outcomes and the counter.

## Animation events

For each animator row, after the frame is computed for `k`, the events of every frame boundary crossed in `(T(k − 1), T(k)]` in playback order are emitted (a LOOP clip that wraps emits the frames from the old position to the end, then from the start), at most 64 per row per tick (`twod.anim.events_clipped_delta` counts the rest); each is a `Zenith_Animation2DEvent` emitted through `Zenith_SystemContext::Events(simulation ID)`, so the merge key is (system order, simulation ID, ordinal, sequence) and the delivery order in phase 9 is ascending simulation ID, then emission order ([Z2-N-ARCH-478]). A ONCE clip whose `T(k − 1) < D ≤ T(k)` also emits `Zenith_Animation2DFinishedEvent` after its frame events. A transition at tick start emits nothing for the old state; the new state's frame 0 events fire when `k` = 1 crosses its boundary at 0 (the first tick of a state includes time 0).

`Test_Animation2DEvents_OrderAndWrap` asserts the event sequence for a three-frame LOOP clip over a wrap and with a large `Δ` that skips frames; `Test_Animation2DEvents_IndependentOfWorkers` replays 2,000 animated entities with 1, 2, 8 and all workers and asserts identical event sequences.

## Animation hot reload

When an animator's `AnimationSet` publishes a new object (development reload) or the property is written: a state index whose name exists in the new set keeps the state by name, `StateTicks` unchanged; otherwise the animator returns to UNSTARTED. Parameters are remapped by name (an absent name is dropped; a new parameter takes its default). The remap happens at the first `TwoD.Animate` that sees the new object, in ascending simulation ID; `Test_SpriteAnimationHotReload_RemapsByName`.

## Cutout skeleton evaluation

A rig is up to 128 bones (each: name, parent index lower than its own, bind pose `Zenith_Bone2DPose`) and up to 256 slots (each: bone, default attachment, draw order, colour), with skins that map (slot, attachment name) to an atlas frame plus an attachment transform relative to the bone. A skeletal clip holds per-bone tracks (translation VEC2, rotation F32 radians, scale VEC2) and per-slot tracks (attachment STEP, colour LINEAR, draw-order STEP), each a sorted keyframe list with interpolation LINEAR or STEP.

`TwoD.AnimateSkeletons`, per skeleton row whose animator plays a skeletal state, at clip time `t` (the flipbook formula's `t`, with the clip's period):

1. For each bone in index order: each channel with a track takes the track's value at `t` (binary search for the keyframe pair; LINEAR interpolates translation and scale componentwise and rotation along the shorter arc in radians; STEP holds the earlier key); a channel without a track keeps the bind pose's value.
2. Compose the rig-space matrix `R_b = R_parent × T(translation) × Rz(rotation) × S(scale)` as a 2D affine `Zenith_Affine2D` (`a, b, c, d, tx, ty` in binary32, column-vector convention of `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`), with `Rz(r)`'s entries `Zenith_Cos(r)` and `Zenith_Sin(r)` from CORE's deterministic functions (the assumption block of "Determinism rules for TWOD"), never `<cmath>` ( the shorter-arc rotation interpolation of step 1 is plain arithmetic on radians: `Δ = wrap(r1 − r0)` into (−π, π], `r = r0 + Δ·u`); the root's parent is the identity.
3. Slots: the attachment and colour from their tracks at `t` (or the skin's defaults), the draw order from the draw-order track, ties by slot index. Each slot of the pose is a 12-byte `Zenith_Slot2DPose { uint16 m_uSlot; uint16 m_uAttachment; uint16 m_uDrawOrder; uint16 m_uReserved; uint32 m_uColourRGBA8; }` (attachment 0xFFFF = none; colour linear, each channel `⌊clamp(c, 0, 1) × 255 + 0.5⌋`), in draw order.
4. Write the bone matrices and the slot list into the entity's entry of `Zenith.TwoD.Poses`: a frame-stable buffer in the module's per-world state, indexed by simulation ID through a sorted index rebuilt when the set of posed entities changes; the entry also records the rig the pose was evaluated with, so `Zenith_Skeleton2DPoseView::FindBone` resolves a bone name against exactly that rig.

Complexity O(B log K + S) per skeleton (B bones, K keys per track, S slots). The extractor reads the poses in phase 12 and emits one record per visible slot ("Extraction" step 4 of Rendering.md); the script function `TwoD.GetBonePosition` resolves the bone through the pose's rig and multiplies its rig-space origin by the entity's `Zenith.WorldTransform`. Phases 1 and 2 of a tick read the poses written in phase 3 of the previous tick, so the pose buffer is simulation-visible state and is saved ("Save participation"). `Test_Skeleton2D_PoseMatchesReference` compares 200 sampled times of a three-level rig against an independent CPU composition in the test; `Test_Skeleton2D_SlotDrawOrderTrack` asserts the draw-order changes.

## Tilemap runtime

A tilemap asset holds up to 16 layers (each: name, sorting-layer offset, order offset, z offset in metres, parallax factor VEC2, opacity) over one tileset, and the authored cells of every non-empty chunk (32 × 32 cells, `uTWOD_TILE_CHUNK_EXTENT`). A tileset holds up to 65,535 tiles (index 1 … n): each tile's atlas frame, collision kind ("Tilemap collision generation"), optional custom polygon (3 … 8 vertices in tile-unit space [0, 1]²) and optional animation frames (frame list with one duration). The parallax factor and the opacity are presentation: parallax is applied by the renderer to the layer's drawn origin (`Zenith2/Docs/Design/TWOD/Rendering.md → "Tile tables and the chunk pool"`, rule "Parallax"), and a layer whose parallax differs from (1, 1) generates no collision.

Tilemaps are axis-aligned: drawing and collision use the map entity's world position (x, y, z) only. A map entity whose world rotation is not the identity or whose world scale is not 1 is drawn and collides as if it were, and counts `twod.tilemap.transformed_delta` once per entity per world (a development-build warning names it; scene check rule 6 of `Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"` warns at authoring time).

The effective cell of `(map, layer, x, y)` is the edit overlay's value when the cell was edited, otherwise the authored cell; outside the map's bounds it is empty. The overlay lives in the world resource `Zenith.TwoD.TilemapEdits`: an open-addressed hash map from `(map simulation ID, layer, chunk x, chunk y)` to an overlay chunk `{ uint32 cells[1024]; uint32 editedBits[32]; uint32 revision; }` (4,228 bytes), at most `twod.tilemap.maxEditedChunks` (default 4,096, 1 … 65,536) per world.

`Test_TilemapRuntime_EffectiveCell` asserts authored, edited and out-of-bounds reads; `Test_TilemapRuntime_OverlayCapacity` fills the overlay and asserts the deterministic rejection of the next new chunk.

## Tile edit playback

`TwoD.SetTile` and `TwoD.FillTiles` play back at step 7 of the sync point after the phase that recorded them, in merge order:

1. Resolve the map entity in the world; a stale handle or a missing `Zenith.TwoD.Tilemap` drops the command (`twod.tilemap.edit_dropped_delta`).
2. Clip: `FillTiles` clips its half-open rectangle `[x0, x1) × [y0, y1)` to `[−2^30, 2^30)` per axis and drops a rectangle of 0 or more than 65,536 cells (`twod.tilemap.edit_too_large_delta`); the map's authored bounds are not consulted, so playback never depends on whether the map's asset is resident (a cell outside the bounds is stored and reads empty at use).
3. For each cell in row-major order from `(x0, y0)`: find or create the overlay chunk (creation refused at capacity: the rest of the command is dropped, `twod.tilemap.overlay_full_delta`); write the cell and set its edited bit; increment the chunk's revision once per command.
4. Add each touched chunk to the world's collision dirty set (sorted by key) and to the extractor's pending upload set ("Extraction" step 8 of Rendering.md).

`Test_TileEditPlayback_MergeOrder` records conflicting edits from two parallel chunk systems and asserts the merge-order winner with 1, 2, 8 and all workers.

## Tilemap collision generation

Every map with `Collision` true has one static body per layer whose parallax is (1, 1), created when the map's collision is first generated and placed at the map entity's world position (previous tick's world transform; z ignored by physics, rotation forced to 0 by the axis-aligned rule), with the map's physics layer, friction and restitution. Chains are built in map-local coordinates (cell corner `(x, y)` at `(x × cellSize.x, y × cellSize.y)` metres) and attached to that body, so moving the map entity moves its chains without regenerating them. At ingest ("Physics 2D ingest" step 4), for every map whose chunk is in the collision dirty set or newly resident (an authored map's chunks are all "newly resident" when its asset first resolves, which happens at scene activation, before the first tick of the scene), in ascending (map simulation ID, layer, chunk y, chunk x):

| Collision kind | Value | Generated |
|---|---|---|
| NONE | 0 | nothing |
| FULL | 1 | merged into outline chains (below) |
| ONE_WAY_UP | 2 | the cell's top edge, merged with collinear neighbours into one-way chains of normal +Y |
| CUSTOM | 3 | the tile's polygon, transformed by the cell's flip and transpose bits, as one static polygon shape per cell on the layer's static body |

1. Build the 34 × 34 binary occupancy grid of the chunk's FULL cells plus a one-cell border read from the neighbouring chunks' effective cells.
2. Marching squares over the grid yields the boundary edges of the solid region, each edge a unit segment between cell corners, oriented with the solid on the left.
3. Link edges into polylines, merging collinear runs (a polyline vertex exists only where the direction changes); clip every polyline to the chunk's own 32 × 32 square: a loop entirely inside is closed; a polyline crossing the square's border becomes an open chain whose ghost vertices are the neighbouring vertices beyond the border (from the border ring), so chains of adjacent chunks meet without a bump.
4. For each chain, the per-segment cell table: segment s of the chain (between vertex s and s + 1, merged runs included) is split back into its unit edges, and each unit edge's owning solid cell (the cell on its left) gives a cell key `Zenith_MakeTileCellKey(layer, x, y)` (`Zenith2/Docs/Design/TWOD/PublicAPI.md → "Zenith_TwoDTypes header"`); the chain's `Zenith_Chain2DDesc::m_aulSegmentCellKeys` lists one key per unit edge in chain order, so a contact or query hit on a merged segment resolves to its exact cell from the hit point's parameter along the segment (`⌊u × unitEdges⌋`, clamped).
5. Remove the chunk's previous chains and polygons first, then create one chain per polyline through the seam ("Physics 2D backend seam"), and record the chunk's chain shape IDs and cell tables in the chain index of the module world state.

When the map entity's world position changes (its world transform's change version is newer than at the previous ingest), the ingest calls `SetBodyTransform` on each of the map's static bodies with the new position, in ascending layer, counts `twod.physics.static_moved_delta` and regenerates nothing.

Complexity O(34²) per dirty chunk. `Test_TileCollision_OutlineMatchesReference` compares the chains of 64 random 32 × 32 occupancy patterns with an independent edge-tracing reference in the test, including every cell table; `Test_TileCollision_NoBumpAcrossChunks` slides a box across a 256-cell floor spanning eight chunks on the selected backend and asserts its vertical velocity never exceeds 10⁻³ m/s; `Test_TileCollision_MovedMapFollows` moves a map entity by (3.5, −2) m and asserts a body resting on it is carried, its chains are not regenerated (the chain index's shape IDs are unchanged) and a ray hit on a merged segment reports the cell under the hit point.

## Tilemap hot reload

A tileset or map that publishes a new object marks every chunk of every map using it collision-dirty and pending for upload; overlay edits stay keyed by cell and are re-applied over the new authored cells; `Test_TilemapHotReload_KeepsEdits`.

## Physics 2D model

| Aspect | Rule |
|---|---|
| plane and units | the XY plane of `Zenith2/Docs/Design/CORE/Maths.md → "Coordinate systems"`, metres, kilograms, seconds; rotation is the angle about +Z, `θ = 2·Zenith_Atan2(q.z, q.w)` from the world rotation (CORE's deterministic function (the assumption block of "Determinism rules for TWOD")); z and the other rotation components are ignored by physics and kept by the sync |
| bodies | one per entity with an enabled `Zenith.TwoD.RigidBody`; colliders on the entity and on descendants without their own body form its compound shape, each placed by its world transform relative to the body's (previous tick's world transforms); a collider with no body above it is a shape of a static body created at that collider's entity |
| ground body | one static body per world, created at attach at the origin and moved only by "Physics 2D origin shift", so after shifts it sits at `g = −ΣS` (binary32, the sum of the applied shifts); a joint whose `BodyB` is null attaches to it, with its anchor in the ground body's frame `WorldAnchor − g` (Joint property 20, world space; property 5 AnchorB is B-local and is ignored for such a joint) computed at every creation and re-creation (ingest step 5), so a joint created before a shift and one re-created after it agree while `WorldAnchor` is a current-frame point; `g` and the ground body's seam ID are saved in the section header ("Save participation") |
| body key of a shape | the simulation ID of the entity whose 2D body owns the shape: the body entity for a collider of a body, the collider's own entity for a collider without a body (its static body sits there), the map entity for tile chains and CUSTOM tile polygons, and 0 for the ground body; the hit and contact order key "body simulation ID" is this value, the same in the seam and in `Zenith_Physics2DWorldResource` |
| layers | 32; the project key `twod.physics.layerCollision` (32 × u32, bit j of entry i) gives which pairs collide, made symmetric at load by AND-ing (i, j) with (j, i); `twod.physics.layerNames` names them for the editor |
| materials | per collider friction and restitution; a pair's friction is `√(f_a · f_b)`, its restitution `max(r_a, r_b)` |
| gravity | `twod.physics.gravity` (default (0, −9.81)), changed at run time by `TwoD.SetGravity`, saved with the world |
| capacities | `twod.physics.maxBodies` 16,384 (1 … 65,536), `maxShapes` 65,536 (1 … 262,144), `maxJoints` 4,096 (1 … 65,536), `maxContactPairs` 65,536 (1 … 262,144), `maxPendingOpsPerTick` 16,384 |
| substeps | `twod.physics.substepsPerTick` 4 (1 … 8) |
| sleeping | `twod.physics.sleepSeconds` 0.5, `linearSleepTolerance` 0.01 m/s, `angularSleepTolerance` 0.0349 rad/s |
| sensors | a sensor collider reports dynamic and kinematic non-sensor colliders only ("Contact and trigger events") |
| continuous collision | no dynamic body tunnels static geometry; a `Bullet` body does not tunnel dynamic bodies either ("Continuous collision") |

## Physics 2D backend seam

```cpp
// Zenith_Physics2DBackend.h: in the harness tree until the D-SEL-2DPHYS ruling, then Zenith2/Engine/Zenith2D/Internal/Physics2D/
// ("Seam header placement"). Every type it uses beyond its own is from a public header: Zenith_Physics2D.h, Zenith_TwoDTypes.h
// (Zenith_TileCellRef, Zenith_MakeTileCellKey), ZenithSave's section reader and writer, ZenithCore.
struct Zenith_Body2DID { uint32 m_uSlot = 0; uint32 m_uGeneration = 0; };
struct Zenith_Shape2DID { uint32 m_uSlot = 0; uint32 m_uGeneration = 0; };
struct Zenith_Joint2DID { uint32 m_uSlot = 0; uint32 m_uGeneration = 0; };

struct Zenith_Body2DDesc { uint8 m_eType; Zenith_Vector2 m_xPosition; float m_fRotation; Zenith_Vector2 m_xLinearVelocity;
	float m_fAngularVelocity; float m_fGravityScale; float m_fLinearDamping; float m_fAngularDamping; float m_fMassOverride;
	bool m_bFixedRotation; bool m_bBullet; bool m_bSleepAllowed; uint64 m_ulSimulationID; };
struct Zenith_Shape2DDesc { uint8 m_eKind; Zenith_Vector2 m_axPoints[8]; uint8 m_uPointCount; float m_fRadius; float m_fDensity;
	float m_fFriction; float m_fRestitution; bool m_bSensor; uint8 m_uLayer; bool m_bOneWay; Zenith_Vector2 m_xOneWayNormal;
	uint64 m_ulColliderSimulationID; uint64 m_ulCellKey; };          // m_ulCellKey: a CUSTOM tile polygon's cell, else 0
struct Zenith_Chain2DDesc { Zenith_Span<const Zenith_Vector2> m_axPoints; Zenith_Vector2 m_xGhostBefore; Zenith_Vector2 m_xGhostAfter;
	bool m_bLoop; bool m_bOneWay; uint8 m_uLayer; float m_fFriction; float m_fRestitution; uint64 m_ulOwnerSimulationID;
	Zenith_Span<const uint64> m_aulSegmentCellKeys; };                // one cell key per unit edge, chain order
struct Zenith_Contact2DRecord { uint64 m_ulLow; uint64 m_ulHigh; uint64 m_ulSubKeyLow; uint64 m_ulSubKeyHigh; uint8 m_eKind;
	bool m_bSensor; Zenith_Vector2 m_xPoint; Zenith_Vector2 m_xNormal; float m_fApproachSpeed; };   // sub-keys: cell keys, or 0

// Implemented by exactly one class in a shipping build, the one D-SEL-2DPHYS (open, Stage 2) selects; the harness
// Zenith_Bench_Physics2DSelection compiles both candidates. Thread affinity: OWNER_THREAD (the world's TwoD.Physics2DStep node in
// phase 5, the world's phase-11 snapshot node for SaveState and GetResidentBytes, the world's sync-point node at an
// LIFECYCLE_EVENT_ORIGIN_SHIFTED (ShiftOrigin), and the main thread out of tick for restore, attach and detach).
// Reentrancy: NOT_APPLICABLE.
class Zenith_Physics2DBackend
{
public:
	virtual ~Zenith_Physics2DBackend() = default;
	virtual Zenith_Result<Zenith_Body2DID, Zenith_Physics2DError> CreateBody(const Zenith_Body2DDesc& xDesc) = 0;
	virtual void DestroyBody(Zenith_Body2DID xBody) = 0;
	virtual Zenith_Result<Zenith_Shape2DID, Zenith_Physics2DError> AddShape(Zenith_Body2DID xBody, const Zenith_Shape2DDesc& xDesc,
		Zenith_Vector2 xLocalOffset, float fLocalRotation) = 0;
	virtual Zenith_Result<Zenith_Shape2DID, Zenith_Physics2DError> AddChain(Zenith_Body2DID xStaticBody, const Zenith_Chain2DDesc& xDesc) = 0;
	virtual void RemoveShape(Zenith_Shape2DID xShape) = 0;
	virtual Zenith_Result<Zenith_Joint2DID, Zenith_Physics2DError> CreateJoint(const Zenith_Joint2DComponent& xJoint,
		Zenith_Body2DID xBodyA, Zenith_Body2DID xBodyB, uint64 ulJointSimulationID) = 0;
	virtual void DestroyJoint(Zenith_Joint2DID xJoint) = 0;
	// Newtons: |linear constraint impulse of the tick's last substep| / (fTickSeconds / uSubsteps); the angular part is not included.
	virtual float GetJointReactionForce(Zenith_Joint2DID xJoint) const = 0;
	virtual void SetBodyTransform(Zenith_Body2DID xBody, Zenith_Vector2 xPosition, float fRotation) = 0;
	virtual void SetBodyVelocity(Zenith_Body2DID xBody, Zenith_Vector2 xLinear, float fAngular) = 0;
	virtual void ApplyImpulse(Zenith_Body2DID xBody, Zenith_Vector2 xImpulse, Zenith_Vector2 xWorldPoint) = 0;
	virtual void ApplyAngularImpulse(Zenith_Body2DID xBody, float fImpulse) = 0;
	virtual void ApplyForce(Zenith_Body2DID xBody, Zenith_Vector2 xForce, Zenith_Vector2 xWorldPoint) = 0;
	virtual void ApplyTorque(Zenith_Body2DID xBody, float fTorque) = 0;
	virtual void SetAwake(Zenith_Body2DID xBody, bool bAwake) = 0;
	virtual void SetGravity(Zenith_Vector2 xGravity) = 0;
	virtual void SetLayerMatrix(const uint32 (&auLayerCollision)[32]) = 0;
	// Every body origin p becomes the binary32 difference p − xShift, in ascending body slot; velocities, sleep states, joint frames
	// (body-local) and contacts are kept ("Physics 2D origin shift").
	virtual void ShiftOrigin(Zenith_Vector2 xShift) = 0;
	// One tick: uSubsteps substeps of fTickSeconds / uSubsteps on the calling thread, never a worker fan-out.
	virtual Zenith_Result<void, Zenith_Physics2DError> Step(float fTickSeconds, uint32 uSubsteps) = 0;
	// The tick's contact begin/end records (per shape pair, after the step) and the sensor enter/exit records (end-of-tick overlap
	// changes), appended to axOut in the backend's order (sorted by the caller).
	virtual void DrainContacts(Zenith_Array<Zenith_Contact2DRecord>& axOut) = 0;
	virtual void GetBodyState(Zenith_Body2DID xBody, Zenith_Vector2& xPosition, float& fRotation, Zenith_Vector2& xLinear,
		float& fAngular, bool& bSleeping) const = 0;
	// Bounded collectors: each keeps the axOut.Size() smallest hits by (fraction, body key, collider simulation ID, cell key) over
	// every hit the query finds, whatever its traversal order, and returns the total hit count (which may exceed the capacity).
	// Body key: "body key of a shape" of "Physics 2D model" (0 for the ground body); cell key: Zenith_MakeTileCellKey, 0 for a
	// non-tile hit.
	virtual Zenith_Result<uint32, Zenith_Physics2DError> CastRay(Zenith_Vector2 xOrigin, Zenith_Vector2 xDelta, float fRadius,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_Span<Zenith_RayCast2DHit> axOut) const = 0;   // fRadius 0: a ray
	virtual Zenith_Result<uint32, Zenith_Physics2DError> OverlapBox(Zenith_Vector2 xMin, Zenith_Vector2 xMax, bool bPointOnly,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_Span<Zenith_Overlap2DHit> axOut) const = 0;
	// EXACT state: every body, shape (chains with their cell tables), joint, contact, sensor overlap and warm-start value the next
	// Step reads, with every seam ID preserved bit for bit; false = this backend cannot, and SaveState writes nothing.
	virtual bool SupportsExactState() const = 0;
	virtual Zenith_Result<void, Zenith_Physics2DError> SaveState(Zenith_SaveSectionWriter& xWriter) const = 0;
	virtual Zenith_Result<void, Zenith_Physics2DError> RestoreState(Zenith_SaveSectionReader& xReader) = 0;
	// This world's share; GetProcessResidentBytes the backend's process-wide share counted once per module (zero when none).
	virtual uint64 GetResidentBytes() const = 0;
	virtual uint64 GetProcessResidentBytes() const = 0;
};
```

Neutral semantics every implementation produces (the scenario suite measures each one against both candidates, `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS scenario suite"`):

| Semantic | Rule |
|---|---|
| plane | bodies move in XY and rotate about Z only |
| shapes | box, circle, capsule (segment along local X), convex polygon (3 … 8), two-sided segment (static and kinematic only), one-sided chains with ghost vertices |
| sensor reach | an overlap between a sensor shape and a non-sensor shape of a dynamic or kinematic body; static shapes, chains and other sensors are never reported; evaluated once per tick against the end-of-tick poses |
| contact records | one begin record when a non-sensor shape pair starts touching during the tick, one end record when it stops; the sub-key of a chain shape is the cell key of the touched unit edge |
| continuous collision | "Continuous collision" |
| one-way | "One-way platforms" |
| joint break force | `GetJointReactionForce`'s definition above |
| queries | the bounded collector above |
| origin shift | `ShiftOrigin`'s definition above |

### Seam header placement

Before the ruling the seam header is `Zenith2/Benchmarks/Physics2DSelection/Seam/Zenith_Physics2DBackend.h`, owned by the harness: Zenith2D holds only its API slice in Stage 2, and non-negotiable 1 keeps a Zenith2D `Internal/` header out of any other target. The harness includes public headers only (`Zenith2D/Zenith_Physics2D.h` and `Zenith2D/Zenith_TwoDTypes.h` of the Stage 2 API slice, ZenithSave, ZenithCore, and ZenithProfiling's `Zenith_ProfileZone.h` and `Zenith_Profiler.h` for the zone of `Zenith2/Docs/Design/TWOD/Tests.md → "D-SEL-2DPHYS cost procedure"`) plus its own `Seam/` and `Internal/` trees, is declared by `zenith_add_benchmark(Physics2DSelection DEPENDS Zenith2D ZenithSave ZenithProfiling ZenithCore THIRD_PARTY box2d jolt SUPPORTED ZENITH_WINDOWS ZENITH_ANDROID)` in `Zenith2/Benchmarks/Physics2DSelection/CMakeLists.txt` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md → "zenith_add_tool zenith_add_benchmark and zenith_add_integration_tests"`; the helper links ZenithTestSupport and nanobench and registers the `--smoke` CTest run, which is `Test_Physics2DSelection_HarnessRuns`), and so names the `box2d` and `jolt` manifest entries in its own `THIRD_PARTY` (a benchmark target may name an entry it measures natively, `Zenith2/Docs/Design/SHIP/Manifest.md → "Owner values"`). After the ruling, the implementation ticket that lands its outcome moves `Seam/Zenith_Physics2DBackend.h` to `Zenith2/Engine/Zenith2D/Internal/Physics2D/Zenith_Physics2DBackend.h` unchanged and moves the winner's implementation as below.

### Candidate B realisation

The harness implementation is `Zenith2/Benchmarks/Physics2DSelection/Internal/Box2D/`; under the Box2D outcome the ruling moves it to `Zenith2/Engine/Zenith2D/Internal/Physics2D/Box2D/`.

| Seam aspect | Box2D v3 (the `box2d` entry, a v3.1 release pinned by the `needs-human` pin ticket) |
|---|---|
| process state | `b2SetAllocator(&Zenith_Box2DAlloc, &Zenith_Box2DFree)`, free functions routing to `Zenith_GetRootHeap()` with `max(alignment, 16)`, installed at module start step 2a with the same function pointers every time and never uninstalled (idempotent, so a second module instance in one process changes nothing); Box2D's world table is written only by `b2CreateWorld` in `AttachWorld` and `b2DestroyWorld` in `DetachWorld`, both on the main thread; a live world beyond the pinned release's `B2_MAX_WORLDS` fails attach with `PHYSICS2D_ERROR_WORLD_CAPACITY`. This is the process-state exception of cross-doc item X309 (b) |
| world | `b2DefaultWorldDef()` plus `gravity`, `workerCount = 1`, `enqueueTask = nullptr`, `finishTask = nullptr`, `enableSleep = true`, `enableContinuous = true`, every other member at the pinned release's `b2DefaultWorldDef()` value, so the solver runs on the calling thread and never blocks a job ([Z2-N-CONTRACT-315]) |
| step | `b2World_Step(worldId, fTickSeconds, uSubsteps)` once (Box2D's soft-step substepping) |
| shapes | `b2MakeBox`/`b2MakeOffsetBox`, `b2Circle`, `b2Capsule`, `b2MakePolygon` of the hull, `b2Segment`; chains `b2ChainDef` with the points `[ghostBefore, p0 … pn, ghostAfter]` (open) or `p0 … pn` with `isLoop` (closed), one material per chain, the cell table kept beside the chain's `b2ChainId` and its segment shapes |
| sensors | `isSensor` on sensor shapes; `enableSensorEvents = true` on every non-sensor shape of dynamic and kinematic bodies and false on static shapes and chains, which yields the neutral sensor reach (v3.1 sensors never detect sensors); records from `b2World_GetSensorEvents` (end-of-step overlaps) |
| contacts | `enableContactEvents = true` on every non-sensor shape; records from `b2World_GetContactEvents` begin and end arrays; a chain segment's sub-key from the segment's index in the chain's cell table |
| one-way | `b2World_SetPreSolveCallback` with `enablePreSolveEvents` on one-way shapes ("One-way platforms") |
| continuous | `enableContinuous` on the world covers dynamic against static; `isBullet = m_bBullet` on the body def covers dynamic against dynamic |
| joints | distance, revolute, prismatic, weld, wheel and motor joint defs one for one; break force from `b2Joint_GetConstraintForce` (the last substep's force, newtons) magnitude |
| queries | `b2World_CastRay`, `b2World_CastShape` (circle proxy), `b2World_OverlapAABB` with a callback that feeds the bounded collector and never terminates early |
| exact state | `SupportsExactState` returns false: Box2D v3 has no world-state save or restore API, so `SaveState` writes nothing and TWOD captures under this backend are COHERENT ("Save participation") |
| resident bytes | per world 0 plus nothing; `GetProcessResidentBytes` = `b2GetByteCount()`, Box2D's process-wide live-byte count, which equals the sum over Zenith2D's worlds because Zenith2D is Box2D's only user |
| origin shift | `b2Body_SetTransform(bodyId, p − s, rotation)` per body in ascending slot, then `b2Body_SetAwake(bodyId, false)` for a body that was sleeping before the call |

The `box2d` manifest entry has the shape of PHYS's `jolt` entry (`Zenith2/Docs/Design/PHYS/Backends.md → "Third-party pins"`): `integration` `cmake-subproject`, licence MIT, owner `Zenith_Bench_Physics2DSelection` with usage `tools` until the ruling and `Zenith2D` with usage `runtime` under the Box2D outcome ([Z2-N-ARCH-121]), platforms android and windows, one target `box2d`, and `options.common` setting every option of the pinned `CMakeLists.txt` explicitly: `BOX2D_SAMPLES`, `BOX2D_BENCHMARKS`, `BOX2D_DOCS`, `BOX2D_UNIT_TESTS`, `BOX2D_VALIDATE`, `BOX2D_PROFILE`, `BOX2D_AVX2` and `BUILD_SHARED_LIBS` all OFF (one instruction set on every x64 machine, as PHYS's Jolt row), compiled from source with the engine's flags. Its headers are included only under the implementation directory above.

### Candidate J harness realisation

`Zenith2/Benchmarks/Physics2DSelection/Internal/Jolt2D/`, implemented directly on the `jolt` entry that PHYS's Stage 1 pinning ticket pins, with PHYS's build options (`Zenith2/Docs/Design/PHYS/Backends.md → "Build options"`) and PHYS's world settings (`Zenith2/Docs/Design/PHYS/Backends.md → "Jolt world construction"` steps 4 and 5), so the measured candidate is the Jolt that production would run. It never goes through `Zenith_Physics`, which does not exist before PHYS's Stage 3 module.

| Seam aspect | Jolt 5.4 |
|---|---|
| process state | the candidate's construction follows `Zenith2/Docs/Design/PHYS/Backends.md → "Jolt construction steps"` steps 2, 4 and 5: it refuses when `JPH::Factory::sInstance` is set (the one-instance refusal), installs Jolt's allocation and trace hooks with the same function pointers every time, sets `sInstance` and calls `JPH::RegisterTypes()`. Its destructor calls `JPH::UnregisterTypes()` and resets `sInstance` only; the hooks stay installed and are never uninstalled, as PHYS's destructor leaves them ("they point at code, not at the object"). This is the third-party process-state exception of cross-doc item X264 (i), of which this candidate is one more instance; the harness never constructs it beside a `Zenith_Physics` instance |
| world | one `JPH::PhysicsSystem` per world, `Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, …)` with three object layers (static, moving, sensor-query) and the layer matrix in the object-layer pair filter |
| step | `PhysicsSystem::Update(fTickSeconds, uSubsteps, &tempAllocator, &jobSystem)` once, with a `JPH::JobSystemSingleThreaded`, so the step runs on the calling thread |
| bodies | `BodyCreationSettings::mAllowedDOFs = JPH::EAllowedDOFs::Plane2D`; `mMotionQuality = JPH::EMotionQuality::LinearCast` for every dynamic body; `mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided` with mass = 2D density × area (or `MassOverride`) and inertia about Z the 2D polar moment (the X and Y moments set equal to it, unused under Plane2D); `mEnhancedInternalEdgeRemoval = true` on dynamic bodies; bodies created in ascending simulation ID with `BodyInterface::CreateBodyWithID`, the seam slot giving the `BodyID`, so IDs survive a restore |
| shapes | boxes and convex polygons as prisms extruded over z ∈ [−0.5, 0.5] m (`BoxShape`, `ConvexHullShape`), circles as `SphereShape` (its z = 0 section is the circle), capsules as `CapsuleShape` rotated so its axis is local X, segments as a thin `BoxShape` of half thickness 0.005 m; a compound body as `StaticCompoundShape` (rebuilt when its shapes change), each sub-shape's user data the collider's simulation ID |
| chains | per map layer one static body whose shape is a `MutableCompoundShape` of one `MeshShape` per chunk; every unit edge of a chain becomes a quad (two triangles over z ∈ [−0.5, 0.5] m) facing the chain's outward normal, with `MeshShapeSettings::mActiveEdgeCosThresholdAngle = cos(5°)` and per-triangle user data the cell key; dynamic bodies' `mEnhancedInternalEdgeRemoval` removes the internal edges between meshes of adjacent chunks, which is the J equivalent of ghost vertices; open-chain ghost vertices are not geometry |
| sensors | sensor shapes are not Jolt bodies: after `Update`, for each sensor collider in ascending simulation ID, `NarrowPhaseQuery::CollideShape` of the sensor's shape at its owner's end-of-tick pose against the moving layer with a body filter that admits dynamic and kinematic bodies and a shape filter that rejects sensor sub-shapes; the set of overlapping colliders is diffed against the previous tick's (kept in the backend and saved) into enter and exit records |
| contacts | `ContactListener::OnContactAdded` and `OnContactRemoved` per sub-shape pair, mapped to begin and end records through the sub-shape user data (collider simulation ID, or the triangle's cell key) |
| one-way | `ContactListener::OnContactValidate` with the rule of "One-way platforms"; it runs on the calling thread under `JobSystemSingleThreaded`, so it is deterministic |
| continuous | `LinearCast` on every dynamic body, which exceeds the neutral rule (non-bullet bodies do not tunnel dynamic bodies either); the rule is a floor, so the scenario suite accepts it |
| joints | distance → `DistanceConstraint` (limits, `mLimitsSpringSettings` from stiffness and damping); revolute → `HingeConstraint` about Z (limits, motor); prismatic → `SliderConstraint` along the in-plane axis (limits, motor); weld → `FixedConstraint`, or a `SixDOFConstraint` with every axis locked and spring settings when `Stiffness` > 0; wheel → `SixDOFConstraint` with the axis translation free under a spring and rotation about Z free with a motor; motor → `SixDOFConstraint` with position motors toward the target offset, bounded force and torque; break force from `GetTotalLambdaPosition()` (the last collision step's linear impulse) divided by `fTickSeconds / uSubsteps` |
| queries | `NarrowPhaseQuery::CastRay`, `CastShape` (a sphere for the circle cast), `CollideShape` (an extruded box for the box overlap) with collectors that feed the bounded collector |
| exact state | `SupportsExactState` returns true: `SaveState` writes the seam's ID tables, every body's creation settings in ascending `BodyID`, the constraints in creation order, the sensor overlap sets and `PhysicsSystem::SaveState(recorder, EStateRecorderState::All)`; `RestoreState` recreates bodies with `CreateBodyWithID` and constraints in the recorded order, then calls `PhysicsSystem::RestoreState` |
| resident bytes | PHYS's Jolt accounting formula ("Resident byte accounting" of `Zenith2/Docs/Design/PHYS/Backends.md`) over this world |
| origin shift | `BodyInterface::SetPosition(id, p − s, JPH::EActivation::DontActivate)` per body in ascending `BodyID` (Jolt has no world shift; PHYS's "Origin shift on Jolt" does the same) |

### Candidate J production realisation

This whole subsection is conditional. Under the orchestrator's ruling TD10-CONDITIONAL (2026-10-10), PHYS's signed-off design does not contain the planar world: PHYS takes row TD10 only if D-SEL-2DPHYS selects Jolt-2D, through a PHYS post-signoff patch that lands TD10 (and the PHYS implementation ticket it implies) before any TWOD implementation ticket that needs it, and every TWOD ticket of the J path (`Internal/Physics2D/Planar/`, the `ZenithPhysics` `DEPENDS`, the `Physics` module dependency, Game2D's `physics.planarWorld`, the planar-world case of `Test_TwoDSchedule_AccessCompiles`, and the test-world switch below) stays draft until the ruling. The test-world switch: attach step 1 under the Jolt outcome requires `Zenith.Physics.PlanarWorld`, so every physics test world must start the Physics module, whose registration of the real `Zenith.Physics.RigidBody` would make the Box2D-path stand-in a `REFLECT_ERROR_DUPLICATE_STABLE_NAME` at `Freeze`; the J-path ticket therefore switches `Zenith_TwoDTestWorld` to start the Physics module on Jolt with `physics.planarWorld` true, makes `Test_Physics2DIngest_DualBodySkipped`, `Test_Physics2DIngest_LaterPhysicsBodyRemoves2DBody` and `Test_TwoDSchedule_AccessCompiles` add PHYS's real component instead of the stand-in, and deletes `Zenith_TwoDTestForeignBody.h` in the same diff (`Zenith2/Docs/Design/TWOD/Tests.md → "Test support"`). The Box2D path needs none of it. The text below is what that patch is asked to provide.

Under the Jolt outcome, Zenith2D never constructs Jolt or a `Zenith_Physics` instance: one instance per backend kind is alive in a process (`PHYSICS_ERROR_BACKEND_ALREADY_ACTIVE`), and the `jolt` entry has one owner, `ZenithPhysics_Jolt`. The 2D world is instead a planar world of the Physics module's own backend, created by the Physics module at its `AttachWorld` and published as the world resource `Zenith.Physics.PlanarWorld`, which `TwoD.Physics2DStep` declares `WriteResource` on and which no PHYS node reads or writes. This is collaboration through a world resource and declared access, the form [Z2-N-ARCH-850] admits; no module looks another up.

<!-- assumption: pending a conditional PHYS post-signoff patch (row TD10, ruling TD10-CONDITIONAL; Jolt outcome only) -->
PHYS adds, under the Jolt outcome of D-SEL-2DPHYS only and in its post-signoff patch (never in the ruling's diff): the configuration key `physics.planarWorld` (bool, default false, simulation-affecting, set by a game whose `game.json` selects the `2D` module); when true, `AttachWorld` creates a second `Zenith_PhysicsWorld` with the planar descriptor (every body restricted to translation X, Y and rotation Z) and adds `Zenith.Physics.PlanarWorld` (`Zenith_PhysicsPlanarWorldResource`, owning phase `PHASE_PHYSICS`) whose members map the seam one for one: bodies with the allowed-degrees-of-freedom property, the five primitive shapes, runtime triangle-mesh chains with per-triangle user data and active-edge handling, per-body continuous collision, a contact-validate callback slot for the one-way rule, the end-of-tick sensor overlap query, the six joint kinds with the last-substep reaction force, the bounded-collector queries, a single-threaded `Step(fTickSeconds, uSubsteps)` called from the writer node only, `SaveState`/`RestoreState` with handle preservation, `ShiftOrigin` and the world's resident bytes. Every member is reached through the declared access of the TWOD node or the save participant call that uses it and is never cached across calls. The patch also states five production clauses:

1. **Jolt backend only.** `physics.planarWorld` true requires the Jolt backend: the Physics module's start fails with `PHYSICS_ERROR_INVALID_DESC`, its log naming the key `physics.planarWorld`, when the selected backend is `PHYSICS_BACKEND_PHYSX`, because the harness measured Jolt alone. The module list's `2D` row's dependency is "Physics (Jolt)" under the Jolt outcome (row TD21).
2. **No double shift.** PHYS's origin-shift listener shifts its own backend worlds and skips the planar world; TWOD's listener shifts the planar world through the seam's `ShiftOrigin` ("Physics 2D origin shift"), so every planar body moves by −S exactly once.
3. **No double count.** The planar world's resident bytes are counted by TWOD's memory consumer only ("Memory consumer"); PHYS's consumer (`Zenith2/Docs/Design/PHYS/Simulation.md → "Memory consumer"`) excludes them from its Σ worlds.
4. **Phase-5 concurrency.** PHYS's step node and `TwoD.Physics2DStep` run concurrently in phase 5 on one PHYS backend instance, so the planar world owns its own `JPH::TempAllocatorImpl` and `JPH::JobSystemSingleThreaded`, and its `Step` reads and writes no state shared with PHYS's own worlds' steps (no shared temp allocator, job system, contact listener, body interface lock or broadphase); the shared factory and registered types are read-only after construction.
5. **Hook lifetime.** The allocation and trace hooks are PHYS's, installed once by PHYS's construction steps and never uninstalled; the planar world installs nothing.
<!-- /assumption -->

The production implementation `Zenith2/Engine/Zenith2D/Internal/Physics2D/Planar/` maps the seam onto that resource with the harness's J mapping (a J-path ticket carries it over after the patch), Zenith2D's `DEPENDS` gains `ZenithPhysics` for the resource's header, the module list's `2D` row gains the module dependency "Physics (Jolt)", and `Zenith_Module_2D_Start` keeps its one parameter: no factory reaches Zenith2D under either outcome (cross-doc item X297 (d), row TD21). The 2D bodies stay Zenith2D's (their own components, ingest, writer source and events), so `WRITER_SOURCE_PHYSICS_2D` exists under both outcomes. Under the Box2D outcome nothing of this subsection applies.

The seam is internal: no type of it appears in a public header, and the module's public API is the same under either ruling ("Selection decision D-SEL-2DPHYS" of the entry document).

## Physics 2D ingest

The first stage of `TwoD.Physics2DStep`, before the backend step, mirroring the PHYS ingest of `Zenith2/Docs/Design/PHYS/Simulation.md → "Ingest algorithm"` in 2D:

1. **Scan.** `ParallelFor` over the chunks of the queries `WITH Zenith.TwoD.RigidBody`, `WITH Zenith.TwoD.Collider`, `WITH Zenith.TwoD.Joint` and `WITH Zenith.TwoD.Tilemap`: each leaf writes the rows whose structural epoch, change version or parent world transform changed since the previous tick into a per-leaf slot; the slots are concatenated and sorted by simulation ID, so the change list is independent of chunk layout. The first ingest after an EXACT restore ("Save participation") treats every row as unchanged and runs step 6 over every body row, comparing against the restored written bits; the first ingest after a COHERENT restore, and the first ingest of a world, treat every row as changed.
2. **Removals.** In ascending simulation ID: joints whose entity or bodies no longer qualify, then bodies (their shapes, their joints, their pair records ended without events). A 2D body whose entity now also carries PHYS's `Zenith.Physics.RigidBody` is removed in this step (`twod.physics.dual_body_delta`, one development-build warning naming its simulation ID).
3. **Creations and rebuilds.** In ascending simulation ID: compose the compound shape; create the body (`CreateBody` with the position and angle of `W_parent · L`, initial velocities on creation; the record's written bits set to `L`'s exact bits: position x, y, z and rotation x, y, z, w, seven binary32 values) or replace its shapes; a refusal for capacity leaves the entity bodiless (`twod.physics.bodies_refused_delta`), retried at its next change. An entity that also carries PHYS's `Zenith.Physics.RigidBody` gets no 2D body (`twod.physics.dual_body_delta`), so the two physics writers never meet on one entity even when the cook-time rule was bypassed.
4. **Tile collision.** "Tilemap collision generation" for the dirty and newly resident chunks and for moved maps, then clear the collision dirty set.
5. **Joints.** For every changed joint, or one whose bodies were created or removed: destroy and re-create it in ascending simulation ID. A joint whose `BodyB` is null is created against the ground body with its ground-frame anchor `WorldAnchor − g` (Joint property 20; ingest passes `CreateJoint` a copy of the component whose `m_xAnchorB` holds that value, so property 5 is never read for such a joint), `g` the ground body's current position (−ΣS after shifts, "Physics 2D model"), so a creation before any shift and a re-creation after shifts place the anchor at the same world point of the current frame while `WorldAnchor` is a current-frame point. A joint with a non-null `BodyB` uses AnchorA and AnchorB as authored, body-local, and ignores property 20. A joint whose `BodyA` or non-null `BodyB` resolves to no 2D body is not created and counts `twod.physics.joint_unresolved_delta`.
6. **External writes.** "Physics 2D external writes".
7. **Pending operations.** "Physics 2D pending operations".
8. **Kinematic targets.** For every kinematic body in ascending simulation ID: its target is its entity's composed pose of this tick (or the latest `SET_KINEMATIC_TARGET`); the velocity `v = (target − current) / tick` and `ω = wrap(θ_target − θ) / tick` is set (`θ` by `Zenith_Atan2` as "Physics 2D model" states (the assumption block of "Determinism rules for TWOD"); `wrap` maps to (−π, π] by adding or subtracting 2π, plain arithmetic), so the body reaches the target at the end of the tick under both candidates.

Steps 2 and 3 resolve `Zenith.Physics.RigidBody` by name at `AttachWorld` from the frozen type registry and skip both checks when the type is absent, so Zenith2D needs no PHYS code under either outcome. Complexity O(c log c) for c changed rows plus O(k) kinematic bodies; an unchanged world costs the chunk scan. `Test_Physics2DIngest_DualBodySkipped` covers step 3 and `Test_Physics2DIngest_LaterPhysicsBodyRemoves2DBody` adds `Zenith.Physics.RigidBody` to an entity whose 2D body exists and asserts the 2D body is gone at the next tick with the counter; on the Box2D path both register the TWOD test-support stand-in type of that stable name through the test world's registration callback (`Zenith2/Docs/Design/TWOD/Tests.md → "Test support"`), never PHYS's module, so they build and run without PHYS; under the Jolt outcome the J-path ticket replaces the stand-in with PHYS's real component ("Candidate J production realisation").

## Physics 2D external writes

As PHYS's rule (`Zenith2/Docs/Design/PHYS/Simulation.md → "External writes"`), compared in the space TWOD wrote, bit-exactly over all seven floats: RigidBody 20–21 differing from the written velocity bits → `SetBodyVelocity` (`twod.physics.external_velocity_delta`); `Zenith.LocalTransform` 1–2 (position three floats, rotation four) differing from the written local bits of a dynamic body → `SetBodyTransform` to `W_parent · L_observed`, velocities kept, the body woken (`twod.physics.external_pose_delta`); a static body moved → `SetBodyTransform` with `twod.physics.static_moved_delta` and one development-build warning; kinematic bodies follow their composed pose by design. Teleports are adopted by the lifecycle listener at their sync point and an origin shift by "Physics 2D origin shift", so neither reaches this comparison. `Test_Physics2DExternal_SevenFloatCompare` writes a rotation differing only in its x component and asserts one adoption, and a bit-equal rewrite of all seven floats and asserts none.

## Physics 2D pending operations

`TwoD.Body2D` playback appends `{ kind, simulation ID, payload }` to the world's pending list (capacity `twod.physics.maxPendingOpsPerTick`, `QUEUE_FULL_POLICY_REJECT`: a full list drops the command with `twod.physics.command_dropped_delta`, deterministic because playback runs in merge order). Ingest step 7 applies them in list order: impulses and angular impulses once before the step, forces and torques summed per body in list order and applied before the step (constant over the tick), velocities set, `WAKE`, `SET_GRAVITY` (the world's, saved with the world); a command naming an entity without a body is dropped (`twod.physics.command_no_body_delta`). The list is empty after step 7 and filled again by the sync points of the following phases, so a capture at phase 11 holds the commands phase 10 recorded; it is saved ("Save participation").

## Physics 2D step

1. `Step(Δ_seconds, n)` with `n = twod.physics.substepsPerTick`: Box2D runs its own `n` soft-step substeps; Jolt runs one `Update` of `n` collision steps. The tick duration is validated at attach: above 1/15 s is `PHYSICS2D_ERROR_INVALID_DESC`.
2. The number of ticks per frame is the simulation clock's, bounded by the catch-up row `Z2-B-JOBS-002`, so steps per frame never exceed that row × 1 call (× n substeps).
3. A `PHYSICS2D_ERROR_STEP_FAILED` is logged with its code and counted `twod.physics.step_failed_delta`; it is a defect report, never a simulation branch, and the scenario suite asserts it never occurs.
4. Joint breakage: after the step, for each joint with `BreakForce > 0` in ascending simulation ID, `GetJointReactionForce` (newtons, the last substep's linear constraint force, the seam's definition) above `BreakForce` destroys the constraint, writes Broken = true and emits `Zenith.TwoD.JointBroken` with that force. `Test_Physics2DJoint_BreakEmitsEvent` hangs a 10 kg box from a revolute joint with `BreakForce` 150 N under gravity 9.81 (static load 98.1 N) and asserts no break, then applies a 1,000 N·s impulse and asserts exactly one break event in that tick.

## Physics 2D write-back and sync

- **Write-back** (last stage of `TwoD.Physics2DStep`): `ParallelFor` over the RigidBody chunks; for each dynamic or kinematic body that was awake this tick or changed sleep state, `GetBodyState` writes RigidBody 20–22 and stores the velocities' exact bits as written bits. Poses are not written in phase 5.
- **Sync** (`TwoD.Physics2DSyncTransforms`, phase 6, `WRITER_SOURCE_PHYSICS_2D`): writes `Zenith.LocalTransform` 1–2 into the staging column for every awake dynamic body and every dynamic body whose parent's world transform changed: position `(x, y, z_kept)` and rotation `Rz(θ) · R_kept_xy`, with `Rz(θ)` the quaternion `(0, 0, Zenith_Sin(θ/2), Zenith_Cos(θ/2))` from CORE's deterministic functions (the assumption block of "Determinism rules for TWOD"), where `R_kept_xy` is the component of the entity's local rotation about axes other than +Z (identity for a 2D entity); for a parented entity `Local = inverse(W_parent) · W_body` with the parent's previous-tick world transform, as PHYS specifies (`Zenith2/Docs/Design/PHYS/Simulation.md → "Physics-to-transform sync"`); the seven floats written become the record's written bits. Scale is never written.
- **Writer source.** <!-- assumption: pending ARCH (row TD9; cross-doc items X300 and X309 (a)) --> `WRITER_SOURCE_PHYSICS_2D = 6` is appended after `WRITER_SOURCE_CINEMATIC` (existing values never move), its schema name is `"Physics2D"`, and the default arbitration precedence becomes cinematic > ragdoll > physics > physics2D > animation > script (`Zenith2/Docs/Architecture/FramePhases.md → "Writer sources and default precedence"`; the order names each source once, [Z2-N-ARCH-493]). <!-- /assumption --> The precedence arbitration of `Zenith2/Docs/Architecture/FramePhases.md → "Arbitration rules"` merges this writer with the others; an entity carries at most one of the two physics bodies by the authoring rule `TWOD_COOK_DIAG_TWO_PHYSICS_BODIES` (`Zenith2/Docs/Design/TWOD/Authoring.md → "Scene content checks"`) and the ingest rules of steps 2 and 3 of "Physics 2D ingest", so the PHYSICS and PHYSICS_2D positions never compete on one entity.

`Test_Physics2DSync_WritesOnlyAwakeBodies`, `Test_Physics2DSync_KeepsZAndScale` and `Test_Physics2DSync_ParentedBody` assert the three rules.

## Contact and trigger events

After the step, `DrainContacts` fills the tick's records; the module sorts them by the record key (low simulation ID, high simulation ID, sub-key low, sub-key high, kind), where a sub-key is the cell key of a tile chain's unit edge or of a CUSTOM tile polygon and 0 otherwise, then walks them as PHYS's "Event emission" does (`Zenith2/Docs/Design/PHYS/Simulation.md → "Event emission"`): touching counts per collider pair (the low and high simulation IDs; a tilemap's pairs use the map entity's ID), `ContactBegan` when a non-sensor pair's count goes from 0 and either collider has `ReportContacts`, `ContactEnded` when it returns to 0, `TriggerEntered`/`TriggerExited` when a sensor pair's overlap begins or ends (`Trigger` names the sensor's entity), `JointBroken` from step 4 of "Physics 2D step". Each event of key ID K (the pair's low simulation ID, or the joint's) is emitted through `Events(K)` in the fixed group order began, ended, entered, exited, broken, and within a group by ascending high simulation ID. Delivery in phase 9 is therefore a function of simulation IDs and cell keys alone. The per-world event queue capacity is `Z2-B-GAME-003` (checked at the merge; a rejection is deterministic and counted).

Trigger reach is the neutral rule of "Physics 2D backend seam": a sensor reports dynamic and kinematic non-sensor colliders; a static collider, a tile chain and another sensor are never reported, on either candidate. The overlap is evaluated against the end-of-tick poses, so a body that passes through a sensor within one tick without overlapping it at a tick end produces no event (the documented sampling rule).

`Test_Physics2DEvents_SortedByKey`, `Test_Physics2DEvents_IndependentOfWorkers` (1, 2, 8 and all workers, forced compaction, two chunk capacities) and `Test_Physics2DEvents_TriggerReach` (a sensor overlapping a dynamic, a kinematic, a static body, a tile chain and another sensor: exactly two enter events) assert the order and the reach.

## One-way platforms

A collider or chain with `OneWay` keeps a contact with another collider only while the other body's velocity relative to the platform along the platform's world normal `n` is at most `twod.physics.oneWaySpeedThreshold` (default 0.1 m/s, i.e. not moving away from below) and the other body's lowest point along `n` was above the platform's surface minus `twod.physics.oneWaySlop` (0.02 m) at the start of the tick; a contact disabled once in a tick stays disabled for that shape pair until the pair separates. The disabled pairs are module state keyed by (platform collider simulation ID, platform sub-key, other collider simulation ID), saved with the world. Box2D realises it in the pre-solve callback and Jolt in `OnContactValidate`, both on the step thread with the same rule. `Test_Physics2DOneWay_PassFromBelowLandFromAbove` and `Test_Physics2DOneWay_DropThroughCommand` (a script disables the pair by setting the collider's `Enabled` false for one tick) assert it.

## Continuous collision

The neutral rule: no dynamic body tunnels static geometry (tile chains included), and a `Bullet` body does not tunnel dynamic bodies either. Box2D meets it with world continuous collision plus `isBullet`; Jolt with `LinearCast` on every dynamic body, which also keeps non-bullet bodies from tunnelling dynamic ones. The rule is a floor: behaviour beyond it (a non-bullet body against a thin dynamic body) is backend-specific and no test asserts it. `Test_Physics2DCCD_BulletThroughThinWall` fires a circle of radius 0.05 m at 300 m/s at 60 Hz at a static wall 0.02 m thick and, as a bullet, at a dynamic box 0.02 m thick of 1,000 kg, and asserts it passes neither.

## Sleeping

A body whose linear speed stays below `linearSleepTolerance` and angular speed below `angularSleepTolerance` for `sleepSeconds` sleeps (when `SleepAllowed`); a contact with an awake body, a joint change, a command or an external write wakes it. Sleeping bodies are not written back or synced. `Test_Physics2DSleep_RestAndWake`.

## Physics 2D queries

`Zenith_Physics2DWorldResource` forwards to the backend's queries with a span of `uTWOD_MAX_QUERY_HITS` (64) entries; each backend query is a bounded collector that keeps the 64 smallest hits by the total order (fraction, body simulation ID, collider simulation ID, cell key) over every hit it finds, whatever its traversal order, and returns the total count. The resource sorts the kept hits by the same order and sets the truncation bit when the total exceeds the span, so a query's result never depends on the backend's internal traversal. Queries run in any phase except 5 and read the state after the last completed step; a query from a parallel chunk system is a read of the declared resource and concurrent queries are allowed (both candidates' query paths are read-only on a stepped world). Script queries count as READ_WORLD ([Z2-N-SCRIPT-562]).

Zenith2D is `ZENITH_GPU` and is never built in `linux-tsan`, so the concurrent-query path has no thread-sanitizer run; `Sanitizers -Preset win-asan` covers memory errors only. `Test_Physics2DQuery_TieBreakDeterministic` builds equal-distance hits on four bodies and asserts the order, and builds 100 hits along one ray and asserts the 64 kept are the 64 smallest by the order and the truncation bit is set; `Test_Physics2DQuery_ConcurrentReads` runs 8 sub-jobs of 1,000 casts each and asserts identical results; `Test_Physics2DQuery_ConcurrentReadsStress` (win-asan, 32 sub-jobs × 10,000 casts while no step runs) asserts no sanitizer report and equal results.

## Physics 2D origin shift

<!-- assumption: pending SCENE, STREAM (core-doc item X264 (j); row TD24) -->
The `Scene.ShiftWorldOrigin` playback dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` with a null entity and the played `Zenith_WorldOriginShift` as payload at the end of step 7 of its sync point, after it has shifted every transform (`Zenith2/Docs/Design/STREAM/PublicAPI.md → "SCENE world origin"`), exactly as PHYS consumes it (`Zenith2/Docs/Design/PHYS/Simulation.md → "Origin shift"`).
<!-- /assumption -->

TWOD's lifecycle listener, on the sync-point node, for the world being played back:

1. Converts the shift to `s = ((float)S.x, (float)S.y)`; every component is an integer number of metres of magnitude at most 2^24, exact in binary32. A non-finite, non-integral or larger component is a defect: logged, counted `twod.physics.origin_invalid_delta`, the backend is not shifted, and the next ingest adopts the moved bodies as external writes.
2. Calls `ShiftOrigin(s)` on the backend world: every body, the map static bodies and the ground body included, moves by `−s`; joint frames are body-local, so every joint, world-anchored ones included, follows. Under the Jolt outcome this call is the only shift the planar world receives (clause 2 of "Candidate J production realisation").
3. Applies SCENE's formula `(float)((double)p − S)` per axis to the module's position-valued state in ascending simulation ID: the written local position (x, y and z) of every root body, every map's recorded world position, the ground body's recorded position `g` (so `g = −ΣS` after the shifts), and the world points of the pending list's `APPLY_IMPULSE`, `APPLY_FORCE` and `SET_KINEMATIC_TARGET` entries (x and y). Child entities' written values are unchanged, because SCENE does not shift a child's local transform. Overlay chunks, chain indices and poses are map-local or rig-local and are not touched.
4. Counts `twod.physics.origin_shifts` (cumulative).

World-space VEC2 properties of TWOD components (`Zenith.TwoD.Camera` 10–11 BoundsMin and BoundsMax, `Zenith.TwoD.Joint` 20 WorldAnchor) are not shifted by SCENE until `PROPERTY_FLAG_WORLD_POSITION` admits VEC2 properties (row TD24, cross-doc item X332 (c)); until then a 2D game in a rebasing world authors camera bounds and world joint anchors relative to an entity rather than in world space. The flag is per property and unconditional (`Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Origin shift playback"`), which is why the world anchor is its own property: `Zenith.TwoD.Joint` 5 AnchorB is B-local for every body-to-body joint and is never flagged, so a rebase never moves it. `Test_Physics2DOriginShift_ContinuesSeamlessly` shifts a running world with stacked bodies, a tilemap, two joints (one world-anchored, one body-to-body) and pending impulses by (2,048, −2,048) m and asserts that queries in the rebase tick hit the same entities at positions equal to the previous ones minus S within one binary32 rounding, that no external-write or `static_moved` counter changes, and that a body resting on the map neither falls nor bounces. The world-anchored joint has `BodyB` null and `WorldAnchor` = (2,050, 4) m authored at the origin-frame position: after the shift the test writes `WorldAnchor` = (2,050 − 2,048, 4 + 2,048) m, the current-frame point SCENE writes once `PROPERTY_FLAG_WORLD_POSITION` admits VEC2 (row TD24), which destroys and re-creates the joint at ingest step 5, and asserts the re-created joint's world anchor (`g` plus its ground-frame anchor) equals (2, 2,052) m within one binary32 rounding and that the hanging body does not move by more than 10⁻⁴ m in the re-creation tick. The body-to-body joint's AnchorA and AnchorB bits are unchanged by the shift, its joint record keeps its seam ID (it is not re-created), and its two bodies keep their separation within one binary32 rounding.

## Determinism rules for TWOD

| Rule | Mechanism |
|---|---|
| every creation, destruction, ingest and emission walks entities in ascending stable simulation ID | the sorted change lists of "Physics 2D ingest"; never chunk or slot order ([Z2-N-ARCH-884]) |
| the physics step runs on one thread | the backend configurations of "Physics 2D backend seam"; worker count never reaches the solver |
| animation time is a function of ticks | "Flipbook evaluation" step 1 in binary64; no wall clock ([Z2-N-ARCH-885]) |
| events and commands merge by key | `Events(simulation ID)` and merge-order playback ([Z2-N-CONTRACT-354]) |
| capacities are checked at merge or playback in merge order | the overlay, pending-op and event capacities |
| tile edits and animator commands never depend on asset residency at the call | "Tile edit playback" step 2; "Animator state machine" (resolution at playback) |
| queries never depend on traversal order | the bounded collector of "Physics 2D queries" |
| an origin shift changes positions by one formula | "Physics 2D origin shift" |
| presentation never reaches simulation | extraction, snapping, canvases and every render setting read simulation and write nothing back ([Z2-N-ARCH-812]) |
| floating-point environment fixed | every TWOD node runs on threads with the environment of [Z2-N-CONTRACT-360] |
| no CRT transcendentals in simulation | every trigonometric evaluation in a simulation node or sync-point playback (θ from a quaternion in "Physics 2D model", `Rz(θ)` in "Physics 2D write-back and sync", the skeleton's `Rz(rotation)` compose in "Cutout skeleton evaluation" step 2, and the kinematic target's `θ_target` in "Physics 2D ingest" step 8) uses CORE's deterministic `Zenith_Sin`, `Zenith_Cos` and `Zenith_Atan2` (orchestrator ruling X314-FMA3, cross-doc item X314; the assumption block below), never `std::sin`, `std::cos`, `std::atan2` or any other `<cmath>` transcendental; `std::sqrt` (correctly rounded by IEEE 754) is allowed, and the pair restitution `max`, the pair friction `√(f_a · f_b)` and `floor` are exact or correctly rounded. Shaders, the renderer and extraction are presentation and are outside this rule |

<!-- assumption: pending CORE (X314, ruling X314-FMA3; CORE's follow-up ticket fixes the final names and header) -->
TWOD assumes the binary32 overloads `float Zenith_Sin(float fRadians)`, `float Zenith_Cos(float fRadians)` and `float Zenith_Atan2(float fY, float fX)` of `ZenithCore/Zenith_Transcendental.h`, bit-identical for identical argument bits on every CPU and platform and independent of the CRT's FMA3 flag, and `void Zenith_ApplyCrtMathMode()` (on Windows x64 `_set_FMA3_enable(0)`) as the first statement of every `main`, the D-SEL-2DPHYS harness's included. These are the declarations `Zenith2/Docs/Design/CORE/Maths.md → "Transcendental declarations"` and → "The CRT maths mode" hold on 2026-10-10; until CORE's X314 follow-up is signed off they are placeholders, and a rename there is applied to the four sites named in the row above and to the harness's `main` in the diff that lands it. Every site cites this block.
<!-- /assumption -->

`Test_TwoDDeterminism_Replay` (target `Zenith_Integration_TwoDDeterminism` in `Zenith2/Tests/Determinism/TwoD/`, linking the 2D module) replays a 600-tick log of the 2D determinism scene (2,000 dynamic bodies, 200 joints, a 256 × 128-cell tilemap edited every 10 ticks, 500 animated sprites, 50 skeletons) with 1, 2, 8 and all workers, forced compaction and two chunk capacities, on each ECS backend, and asserts bit-identical per-tick world-state hashes per backend. The scene's counts are a test fixture, not the D-SEL-2DPHYS measurement scale.

## Save participation

`Zenith_TwoDSaveParticipant` (`Internal/Zenith_TwoDSaveParticipant.h`), one per world, registered with `Zenith_SaveSystem::RegisterParticipant` at world attach (`Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`):

| Member | Value |
|---|---|
| `GetName` | `"Zenith.TwoD.World"` (key `Zenith_SaveParticipantKeyOf` of it) |
| `GetSectionVersion` | 1 |
| `GetFlags` | `uSAVE_PARTICIPANT_FLAG_REQUIRED` |
| `DeclareAccess` | `ReadResource` of `Zenith.TwoD.Physics`, `Zenith.TwoD.Poses` and `Zenith.TwoD.TilemapEdits` on the snapshot system (the module tables, pending list and backend sit behind the first; under the Jolt outcome only, after the conditional TD10 patch, also `ReadResource` `Zenith.Physics.PlanarWorld`) |
| `GetCaptureFidelity` | overridden: `SupportsExactState() ? SAVE_FIDELITY_EXACT : SAVE_FIDELITY_COHERENT`, so a COHERENT-only backend makes every TWOD capture COHERENT through SAVE's per-capture fidelity, and an EXACT backend's captures are EXACT and seekable (`Zenith2/Docs/Design/SAVE/Algorithms.md → "Phase-11 snapshot"`) |
| `IsRestoreReady` | true once every tilemap, tileset, animation set and rig named by a TWOD component of the restored world is ACTIVE or FAILED, so the first tick evaluates animators, skeletons and tile collision against the same assets the uninterrupted run used |

`CaptureSection` writes the section of `Zenith2/Docs/Design/TWOD/Formats.md → "TWOD save section format"` from the state as it stands after phase 10's sync point ([Z2-N-ARCH-477]):

| Captured at phase 11 | EXACT restore | COHERENT restore |
|---|---|---|
| the tilemap overlay: every overlay chunk in ascending key order, cells, edited bits and revision | recreated exactly | recreated exactly |
| the collision dirty set (sorted keys) | restored; its chunks are regenerated at the first ingest | ignored: every chunk of every map becomes collision-dirty |
| the map records (simulation ID, tilemap asset GUID, per-layer static body IDs, recorded world position) and the chain index (per chunk: chain shape IDs and cell tables) | restored; tile chains are part of the backend state and are not regenerated | ignored: rebuilt at the first ingest |
| the body, shape and joint records (simulation ID ↔ seam ID, the seven written local floats and three written velocity floats per body, the joint broken flags) | restored, seam IDs included | written bits restored; seam IDs ignored, bodies, shapes and joints re-created from components at the first ingest (velocities from RigidBody 20–21, sleep from 22) |
| the contact pair set (low, high, count, sensor flag) and the one-way disabled pairs | restored | restored; at the first step a begin record for a pair already in the set raises no event and sets its count to 1, and a pair of the set with no begin record in that step emits its ended or exited event |
| the pending operation list (kind, simulation ID, payload, in list order) and the world gravity | restored | restored |
| the ground body's seam ID and its recorded position `g` (header offsets 60 and 68 of the section) | restored, so a null-`BodyB` joint created after the restore anchors at `WorldAnchor − g` exactly as in the uninterrupted run (the backend blob restores the body itself at −ΣS) | ignored: the ground body that attach created sits at the origin with `g` = 0, which matches a world whose bodies are re-created from components in the current frame |
| the pose buffer (per posed entity: bone matrices and slot list) | restored, so phases 1 and 2 of the next tick read the poses the uninterrupted run reads | restored |
| the backend state (`SaveState`), when `SupportsExactState()` | `RestoreState` | not present (a COHERENT capture writes none) |

- Restore runs in two passes: the whole section is parsed and checked against "Parser limits" into scratch first, so a violation is `SAVE_ERROR_CORRUPT` with no state changed; then the state is replaced. A section whose EXACT flag is set but whose backend blob fails `RestoreState`'s own check fails the restore with `SAVE_ERROR_CORRUPT` and the key, and SAVE destroys the world ("a restore failure fails that restore and the world is destroyed"); there is no restore-time downgrade, because fidelity is decided per capture.
- A section with the EXACT flag restored by a build whose backend kind differs (bit 1 of the section flags) is restored by the COHERENT path and logged once; a cross-backend restore can never be EXACT.
- Animator and skeleton component state is in the ENTITIES section; the pose buffer above is what makes the next tick's early phases identical.
- `Test_TwoDSave_ExactRoundTrip` saves at tick 300 of the determinism scene, restores into a new world, runs 300 ticks and asserts the hashes equal an uninterrupted run (on an EXACT backend); its ground-body case shifts the origin by (2,048, 0) m before the save, creates a null-`BodyB` joint two ticks after the restore, and asserts the per-tick world-state hashes equal the uninterrupted run's, which fails if `g` is not restored; `Test_TwoDSave_ExactRoundTripWithPendingCommands` saves in a tick whose phase 10 recorded impulses, a kinematic target, an `AnimatorPlay` on a skeletal entity and tile edits in two chunks, and asserts the same equality plus no external-write counter change in the first restored tick; `Test_TwoDSave_CoherentDowngrade` runs the participant over a backend whose `SupportsExactState` is false and asserts the capture reports `SAVE_FIDELITY_COHERENT`, the save carries `uSAVE_HEADER_FLAG_COHERENT_ONLY` and the restore recreates bodies at the recorded poses and velocities with no duplicate `ContactBegan`; `Test_TwoDSave_CorruptSectionRefused` flips bytes and asserts the error with no partial state.

## Module world state

`Zenith_TwoDWorldState` (`Internal/Zenith_TwoDWorldState.h`), one per attached world, owned by the module instance in a sorted table by world tag:

| Member | Content | Bound |
|---|---|---|
| `m_xPhysics` | `Zenith_Owned<Zenith_Physics2DBackend>`; the body, shape and joint record tables (slot arrays sized to the capacities, indexed by the backend IDs, each with a sorted simulation-ID index); the ground body; the map records and the chain index; the contact pair set; the one-way disabled set; the restore flags of "Physics 2D ingest" step 1 | the physics capacities |
| `m_xPhysicsResource` | `Zenith_Physics2DWorldResource` borrowing `m_xPhysics` | — |
| `m_xPoses` | `Zenith_Skeleton2DPoseResource`: posed entities' bone matrices, slot lists and rigs | `twod.anim.maxPosedEntities` 4,096 × (128 bones × 24 B + 256 slots × 12 B) |
| `m_xTilemaps` | `Zenith_TilemapEditResource`: the overlay and the collision dirty set | `twod.tilemap.maxEditedChunks` |
| `m_xPendingOps` | the physics pending list | `twod.physics.maxPendingOpsPerTick` |
| `m_xExtraction` | the pending chunk set, the per-map sent-chunk revisions, the sent-tileset set (presentation, never saved: a restore makes the next extraction full) | chunks of resident maps |

Every member is created in `AttachWorld` (each step with a failure-injection value, `Zenith2/Docs/Design/TWOD.md → "World attach and detach"`) and destroyed in `DetachWorld` in reverse.

## Memory consumer

The module registers one `Zenith_MemoryConsumer` of `MEMORY_SUBSYSTEM_SIMULATION` whose `GetResidentBytes` is the backend's `GetProcessResidentBytes` once plus the sum over worlds of the backend's `GetResidentBytes`, the record tables, the pose buffers and the overlays (relaxed atomic totals updated by the owning nodes). Under the Jolt outcome the backend's `GetResidentBytes` is the planar world's, which PHYS's consumer excludes (clause 3 of "Candidate J production realisation"), so the governor counts it once, here; `GetProcessResidentBytes` is then 0 (the Jolt factory and shapes are PHYS's process share). TWOD sheds nothing at WARNING or CRITICAL: the Simulation row of `Zenith2/Docs/Architecture.md → "Memory governor"` ("Shedding per subsystem"); `Test_TwoDMemoryConsumer_ReportsAndShedsNothing` asserts resident bytes unchanged under forced levels. Atlas, animation, rig, tileset and tilemap assets are counted by ASSET under their kinds' memory-subsystem column; the renderer's chunk mirror, GPU chunk pool and tile table are ZenithFlux's and are reported through `Flux_Sprite2DStats` (the pending row proposed as TWOD-008).
