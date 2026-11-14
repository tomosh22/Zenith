# ANIM design: the animation graph

Section file of `Zenith2/Docs/Design/ANIM.md`. It specifies what an animation graph is and how one evaluates: parameters, the node catalogue, the update and pose passes, pose blending, blend spaces, state machines with crossfade and inertial transitions, layers and masks, slots, synchronisation, animation events, root motion, graph compilation, hot reload and the motion-matching node that D-SEL-MOTIONMATCH (open, Stage 3) gates. The bytes of a compiled graph are `Zenith2/Docs/Design/ANIM/Formats.md → "Graph file"`; the systems that run it are `Zenith2/Docs/Design/ANIM/Runtime.md → "Systems"`.

## Scope of this file

| Topic | Heading |
|---|---|
| the model and its limits | "Graph model" |
| inputs | "Parameters" |
| every node kind | "Node catalogue" |
| one evaluation | "Evaluation algorithm" |
| how poses combine | "Pose blending" |
| 1D and 2D blending by parameters | "Blend spaces" |
| states and transitions | "State machines" |
| partial-body animation | "Layers and masks" |
| one-shot clips injected by gameplay and cinematics | "Slots" |
| keeping clips in step | "Synchronization" |
| notifies | "Animation events" |
| the motion the animation carries | "Root motion" |
| document to asset | "Graph compilation" |
| editor iteration | "Graph hot reload" |
| the gated motion-matching node | "Motion matching node" |

## Graph model

An animation graph is a forest of nodes, one tree per **layer**; each tree's root produces the layer's pose; layer 0 is the base layer and layers 1 … 7 blend onto it in order ("Layers and masks"); an optional **IK stage** runs after the layers (`Zenith2/Docs/Design/ANIM/Runtime.md → "IK stage order"`). Every node produces a **pose**: J local joint transforms (`Zenith_Transform`) and C curve values of the graph's skeleton, plus, as side outputs of the update pass, weighted root-motion contributions and fired events. A graph is bound to exactly one skeleton; every clip it names is cooked for that skeleton or retargeted to it.

<!-- elaboration -->
Limits (`Zenith_AnimationTypes.h`, enforced by the compiler and re-checked by the loader): 4,096 nodes; 64 children per node; 256 parameters; 256 state machines with 256 states and 2,048 transitions each; 8 conditions per transition; 8 layers; 8 slots; 64 masks; 32 sync groups; 64 samples per blend space; 1,024 clips; a pose stack 16 deep; 65,536 bytes of instance state.

<!-- informative -->
The reference engine's `Flux/MeshAnimation` separated a definition from an instance (`Flux_AnimationStateMachineDef` against `Flux_AnimationStateMachine`) and built its blend tree from heap-allocated virtual nodes (`Flux_BlendTreeNode` subclasses). ANIM keeps the definition/instance split but compiles the graph into flat tables and a per-instance state block, so an instance is one allocation, evaluation follows array order, and two machines evaluating one graph on two workers share nothing mutable.
<!-- /informative -->

## Parameters

### Parameter kinds

| Kind | Stored as | Set by | Read by |
|---|---|---|---|
| `ANIM_PARAMETER_KIND_FLOAT` | f32, clamped to [minimum, maximum] at application | `Zenith_RecordSetAnimParameter`, the `Animation.SetFloat` binding | blend weights, blend-space coordinates, play rates, conditions |
| `ANIM_PARAMETER_KIND_INT` | f32 holding an integer in [−2^24, 2^24] | `Animation.SetInt` | switch nodes, conditions |
| `ANIM_PARAMETER_KIND_BOOL` | 0.0 or 1.0 | `Animation.SetBool` | conditions, the mirror node |
| `ANIM_PARAMETER_KIND_TRIGGER` | 0.0 or 1.0 plus a hold counter | `Animation.SetTrigger`, `Animation.ResetTrigger` | `TRIGGERED` conditions |

An instance's parameter block holds one f32 per parameter (and one `u8` hold counter per trigger) in the graph's sorted order; it is part of the instance's state in `Zenith_AnimationWorld` and of its save record (`Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"`).

### Reserved parameters

Names beginning `Zenith.` are reserved; the compiler adds these to every graph whose document references them and refuses a document parameter that uses the prefix:

| Name | Kind | Written by |
|---|---|---|
| `Zenith.RagdollRecover` | TRIGGER (hold 4) | ANIM's lifecycle listener at `LIFECYCLE_EVENT_RAGDOLL_RECOVER` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Ragdoll recover"`) |
| `Zenith.RagdollFacing` | INT (0 face up, 1 face down) | the same listener |
| `Zenith.Speed` | FLOAT | GAME's movement system through commands (convention; any writer may set it) |
| `Zenith.Teleported` | TRIGGER (hold 1) | ANIM's listener at `LIFECYCLE_EVENT_TELEPORTED` |

### Parameter commands

Parameters change only through commands applied at a sync point ([Z2-N-ARCH-483]); the command kinds are ANIM's (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"`), recordable in phases 1 to 10 and played back on the sync-point node in merge order, so a phase-2 write is visible to the phase-3 evaluation of the same tick and two writes of one parameter in one phase resolve by merge order (the last wins). A command naming an unknown parameter or the wrong kind is dropped at playback and counted (`anim.parameters.rejected_delta`), never asserted; the binding that recorded it raises a Luau error at record time when it can tell (the graph is bound and the name is absent).

## Node catalogue

`Zenith_AnimNodeKind` (`Zenith_AnimationTypes.h`). Payload sizes are the `ANIM_SECTION_GRAPH_NODE_DATA` bytes per node; instance sizes are the node's bytes in the per-instance state block.

| Value | Kind | Children | Payload | Instance state | Output |
|---|---|---|---|---|---|
| 1 | `CLIP` | 0 | 16: `u16 clip index; u16 flags (bit 0 loop, bit 1 loop overrides the clip's flag, bit 2 mirror); f32 play rate; f32 start time (normalised); u16 rate parameter (0xFFFF none); u16 reserved` | 16: `f32 time; f32 previous time; f32 last path weight; u32 loop count` | the sampled clip ("Pose pass") |
| 2 | `BLEND1D` | 2 … 64 (one per sample) | 4: `u16 blend space index; u16 reserved` | 4 per child: the last weights | the weighted blend of "Blend1D weights" |
| 3 | `BLEND2D` | 3 … 64 | 4: as `BLEND1D` | 4 per child | "Blend2D weights" |
| 4 | `BLEND_DIRECT` | 1 … 64 | 2 per child: the weight parameter of each child, then padding to 4 | 4 per child | the normalised weighted blend; all weights 0 gives child 0 |
| 5 | `BLEND` | 2 | 8: `u16 weight parameter (0xFFFF constant); u16 reserved; f32 weight` | 0 | `Blend(child0, child1, w)` |
| 6 | `ADDITIVE` | 2 (base, additive) | 8: as `BLEND` | 0 | `ApplyAdditive(base, additive, w)`; the additive child's clips must be additive |
| 7 | `LAYERED` | 2 … 9 (base, overlays) | 12 per overlay: `u16 mask index; u8 mode; u8 rotation space; u16 weight parameter; u16 reserved; f32 weight` | 0 | the overlays applied in order, as "Layers and masks" defines for layers |
| 8 | `SWITCH` | 2 … 64 | 8 + 4 per child: `u16 parameter; u16 flags (bit 0 inertial); f32 blend seconds;` then `i32` value per child | 12: `u16 active child; u16 previous child; f32 blend progress; u32 reserved (0)` | the child whose value equals the parameter (child 0 when none); on change crossfaded over the blend seconds, or, with the inertial flag, switched at once under an instance take of the blend seconds ("Inertial blending") |
| 9 | `STATE_MACHINE` | 1 per state (the states' roots) | 4: `u16 machine index; u16 reserved` | 80: "State machine tables" | "State machines" |
| 10 | `TIME_SCALE` | 1 | 8: `u16 rate parameter; u16 reserved; f32 rate` | 0 | the child with its time advance scaled by the rate (clamped to [0, 16]) |
| 11 | `SLOT` | 1 (the input) | 4: `u16 slot index; u16 reserved` | 32: "Slots" | the input overridden by the slot's clip |
| 12 | `POSE_BLEND` | 1 (the base) | 8 + 8 per pose: `u16 clip index (a pose-library clip); u16 pose count; u32 reserved;` per pose `u16 frame; u16 source index; u8 source kind (0 parameter, 1 curve of the base); u8 reserved[3]` | 0 | "Pose libraries" of `Zenith2/Docs/Design/ANIM/Runtime.md` |
| 13 | `MIRROR` | 1 | 4: `u16 bool parameter (0xFFFF always); u16 reserved` | 0 | the child mirrored through the skeleton's mirror table |
| 14 | `REST_POSE` | 0 | 0 | 0 | the rest pose (the identity in an additive subtree) |
| 15 | `MOTION_MATCH` | 0 | "Motion matching node" | "Motion matching node" | the database frame `Zenith_MotionMatcher` selects, played and switched inertially (D-SEL-MOTIONMATCH (open, Stage 3)) |

<!-- elaboration -->
Stage placement: kinds 1 to 11 and 14 land in Stage 3; 12, 13 and 15 in Stage 4, kind 15's tickets blocked by the D-SEL-MOTIONMATCH (open, Stage 3) ruling ticket. A graph naming a kind its loader does not implement is refused (`ANIMATION_ERROR_CORRUPT`), so a Stage 3 runtime never meets a Stage 4 node.

## Evaluation algorithm

One evaluation of one instance advances the instance by its evaluation delta `Δt` (the simulated seconds since its previous evaluation, `Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"`) times the animator's play rate, and produces the instance's **animated pose**, its root-motion delta and its events. It reads only the instance's own state, its parameters, its IK goals and warp targets, its bound assets (immutable) and world state committed before the phase; so the result never depends on the worker or on other instances ([Z2-N-ARCH-883]).

### Update pass

Top-down, from each layer root with the layer's weight (`w_L`, from its parameter clamped to [0, 1], or its constant), in layer order:

1. A node receives its **path weight** w (the product of the weights on the path from its layer root, times `w_L`).
2. `STATE_MACHINE`: run "Transition evaluation" with this evaluation's parameters and the state times from the previous evaluation; then give each active state its weight ("Transition blending") and recurse into the states' roots.
3. `BLEND1D`, `BLEND2D`, `BLEND_DIRECT`, `BLEND`, `ADDITIVE`, `LAYERED`, `SWITCH`: compute the children's local weights from the parameters; recurse with `w × local weight`. Children whose local weight is 0 are still visited with weight 0, so their times stay continuous for synchronisation, but are skipped by the pose pass ("Weight culling").
4. `CLIP`: register (node, w) with its sync group, or, outside a group, advance its time now: `t' = t + Δt × rate × timeScale`, where `timeScale` is the product of the `TIME_SCALE` factors on its path; a loop clip wraps (`t'` mod D, counting wraps), a non-loop clip clamps to [0, D].
5. `SLOT`: advance the slot's clip and blend weight ("Slots").
6. After every layer has been visited, resolve each sync group in group index order ("Synchronization"); then collect events ("Animation events") and root-motion contributions ("Root motion blending") from every clip node in node order.

Complexity: O(active nodes) per evaluation, plus O(transitions of each active machine).

### Pose pass

Bottom-up over the node array in evaluation order (post-order), skipping any subtree whose root's path weight is below `fANIM_WEIGHT_EPSILON` (1e-4; the record's subtree size gives the range to skip):

| Kind | Operation on the pose stack |
|---|---|
| `CLIP` | push; sample the clip at t into the top ("Clip sampling" of `Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationClip header"`); a missing, unbound or skeleton-mismatched clip pushes the rest pose |
| `REST_POSE` | push the rest pose (identity transforms in an additive subtree) |
| `BLEND`, `BLEND1D`, `BLEND2D`, `BLEND_DIRECT`, `SWITCH`, `STATE_MACHINE` | the evaluated children are on the stack in child order; replace them by their N-way blend ("N-way blend") with the normalised weights of the evaluated children (no node adds an inertial offset: the instance's inertialiser applies after the last layer) |
| `ADDITIVE` | pop the additive pose A, apply it to the base below it with the node's weight |
| `LAYERED` | pop the overlays, apply each to the base in order |
| `SLOT` | apply the slot's sampled clip to the top |
| `POSE_BLEND`, `MIRROR`, `TIME_SCALE` | transform the top in place (`TIME_SCALE` changes only the update pass) |
| a layer root | after the layer's root, the top is the layer pose; the base layer's stays, each further layer is applied to it ("Layers and masks") and popped |
| after the last layer | the top is the graph output G, the pre-inertial pose of this evaluation, which "Animation.Evaluate" step 3 (`Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"`) passes through the instance's inertialiser ("Inertial blending") before motion warping and IK |

### Pose stack

Each entry holds J `Zenith_Transform` (40 bytes each) and C f32; the stack depth is the compiler's (at most 16), and the stack lives in the evaluating sub-job's node arena (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobContext header"`), so an evaluation allocates nothing from the heap: at J = 60 and C = 0 a full stack is 38,400 bytes. A pose-stack overflow cannot happen at run time, because the loader recomputes the depth (GV5); the evaluator asserts it in assert-enabled builds.

### Weight culling

A child whose path weight is below 1e-4 is not sampled; the blend renormalises over the evaluated children. Culling changes no time, sync phase or event: it is purely the pose pass's, so the update pass, events and root motion are identical whether or not a child is culled; only the pose differs, by at most the culled weight's share.

## Pose blending

Every blend is per joint and per curve, in joint order, in f32, with the operation order below fixed, so the result is bit-identical on every worker ([Z2-N-ARCH-883]).

### Local-space blend

`Blend(A, B, w)` with w clamped to [0, 1]:

```
t = A.t + (B.t − A.t) × w
r = Zenith_Nlerp(A.r, B.r, w)          // toward B.r or −B.r, whichever is nearer A.r, then normalised (CORE)
s = A.s + (B.s − A.s) × w
curve c = A.c + (B.c − A.c) × w
```

Blending local transforms is rigid per joint: every joint's frame stays a rotation about its own origin and every bone keeps its parent-relative offset, so a point at a joint's origin that both inputs place identically stays there at every weight (`Test_PoseBlend_JointOnPivotFixed`).

### N-way blend

For evaluated children 0 … n − 1 with weights `w_i` normalised to sum 1 (in the children's order, `w_i / Σ w` computed once):

```
t = Σ w_i × t_i                                   // accumulated in child order
r = normalise(Σ w_i × align(r_i, r_k))            // align: negate r_i when dot(r_i, r_k) < 0; k the first child with w_k > 0
s = Σ w_i × s_i
c = Σ w_i × c_i
```

A zero accumulated rotation (opposing inputs) is replaced by `r_k` and counted (`anim.blend.degenerate_delta`).

### Additive application

`ApplyAdditive(base, A, w)` for a local-space additive pose (`t_d`, `r_d`, `s_d` as `Zenith2/Docs/Design/ANIM/Formats.md → "Additive clip cooking"` defines):

```
t = base.t + w × A.t_d
r = base.r · Zenith_Slerp(identity, A.r_d, w)
s = base.s ⊙ (1 + w × (A.s_d − 1))               // component-wise
curve c = base.c + w × A.c_d                      // curves with the additive flag; others take Blend(base.c, A.c, w)
```

A model-space additive applies its rotation about the joint's own model origin: with the base's model transforms `G`, `q' = Slerp(identity, A.r_d, w) · G.r`, the position kept, and the local rotation re-derived from the already-updated parent (`r = G'.parent.r⁻¹ · q'`), in joint order.

### Model-space conversion

`Zenith_LocalToModel(skeleton, local, model)`: `M_0 = TRS(L_0)`; for j = 1 … J − 1, `M_j = M_parent(j) · TRS(L_j)` with CORE's `Zenith_MakeTRS` and matrix product (`Zenith2/Docs/Design/CORE/Maths.md → "Matrix types"`), in `Zenith_Matrix4`, ascending j. O(J), 1 matrix product per joint.

### Rigid delta blending

A **delta** is a rigid transform that maps one frame onto another (a root-motion step, the ragdoll-recover offset, a motion-warp correction). Its translation depends on the pivot it rotates about (`t = p − R · p` for a rotation about p), so interpolating translations linearly while slerping rotations moves the pivot: blended at 0.72 and 0.28 across a 90° rotation, such a blend displaces the shared pivot by a quarter of the lever. Every weighted blend of deltas in ANIM is therefore a **dual-quaternion blend** (`Zenith_BlendDualQuaternions`, `Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationMath header"`): sign-align each unit dual quaternion to the first, accumulate the weighted sum, normalise. The normalised sum is a rigid transform at every weight and a rotation about a shared pivot stays a rotation about that pivot (`Test_DualQuaternion_BlendKeepsSharedPivot`, which also runs the translation-lerp form as a negative control that must move the pivot). Fractions of one delta (spreading a delta over k ticks, blending a delta towards the identity) use the screw power `Zenith_PowDualQuaternion`, which is exact along the screw axis. Poses (absolute joint frames) are never blended as deltas, and deltas are never blended as poses.

## Blend spaces

### Blend1D weights

Samples sorted by value `v_0 < v_1 < … < v_{n−1}`; parameter x clamped to [v_0, v_{n−1}]; find i with `v_i ≤ x ≤ v_{i+1}` (binary search; x equal to a sample value gives that sample weight 1); `w_{i+1} = (x − v_i) / (v_{i+1} − v_i)`, `w_i = 1 − w_{i+1}`, every other weight 0. Two samples with equal values are a compile error.

### Blend2D triangulation

The compiler triangulates the samples with Bowyer–Watson Delaunay insertion: samples inserted in (x, y) lexicographic order (ties impossible: duplicate points are a compile error), a super-triangle three times the bounding box, the in-circle test evaluated in `double` with the exact orientation of the four points (a degenerate co-circular case keeps the existing triangle); triangles touching the super-triangle are removed; the hull edges are the edges used by exactly one triangle, ordered counter-clockwise from the lowest (x, y) vertex. Every sample collinear is a compile error (`ANIM_GRAPH_DIAG_BLEND_SPACE_DEGENERATE`); fewer than 3 samples in a 2D space is an error. Complexity O(n²) for n ≤ 64.

### Blend2D weights

1. For each triangle in table order, compute the barycentric coordinates of the parameter point p (in `double`, rounded once to f32); the first triangle with every coordinate ≥ −1e-6 contains p; its three weights are the coordinates clamped to ≥ 0 and renormalised.
2. Outside the hull, p is projected onto the nearest hull edge (squared distance; ties to the lower edge index), and the two edge samples take the segment's linear weights.
3. Every other sample weight is 0.

Complexity O(triangles) ≤ O(2n). Weights are continuous across triangle edges and across the hull.

## State machines

### State machine tables

The 80-byte instance state of a `STATE_MACHINE` node:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | current state (machine-local) |
| 2 | 2 | active transition count n (0 … 4) |
| 4 | 4 | time in current state (s) |
| 8 | 4 | normalised time of the current state's leader clip at the previous evaluation |
| 12 | 4 | reserved (0; inertial state is per instance, "Inertial blending") |
| 16 | 64 | four transition records of 16 bytes: `u16 source state; u16 transition index; f32 progress (0 … 1); f32 source normalised time; u32 reserved (0)`, oldest first; only crossfade transitions push a record (an inertial take pushes none) |

The state's **leader clip** is the clip node of the state's subtree with the largest path weight in the previous evaluation (ties: the lowest node index); its normalised time drives exit-time conditions. A state whose subtree has no clip has normalised time `time in state / 1 s`, clamped to 1.

### Transition evaluation

Per evaluation, before any weight is computed:

1. **Zero-duration chain.** Up to `uANIM_MAX_ZERO_DURATION_TRANSITIONS_PER_EVALUATION` (4) times: find the first passing candidate (step 2); if it has duration 0, take it immediately (step 3) and repeat; otherwise stop.
2. **Candidates**, in this order, the first whose conditions all pass is chosen:
	- with no active transition: the machine's any-state transitions in priority order, then the current state's transitions in priority order; an any-state transition whose destination is the current state is skipped unless it is flagged "can target self";
	- with active transitions, only the interrupt candidates of the newest active transition's interrupt source: `CURRENT` — the transitions of that transition's source state; `NEXT` — those of its destination state; `CURRENT_THEN_NEXT` and `NEXT_THEN_CURRENT` — both lists in that order; `NONE` — no candidate; any-state transitions are always candidates while transitioning, after the source-based list.
3. **Take**: consume every trigger the transition's conditions name (set to 0, hold counter 0); for a crossfade transition push a transition record `{source = current, transition, progress 0, source normalised time}`; for a transition flagged inertial (flag bit 3) clear every active record instead and request an instance take of the transition's duration ("Inertial blending"), so only the destination is evaluated from this evaluation on; set current := destination; set the destination's time (0, or its normalised start time, or the source's normalised time when "sync destination time" is set; a state flagged "entry resets time" resets the clip times of its subtree); emit `Zenith.Animation.StateChanged` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationEvents header"`). A crossfade take while 4 records are active converts to an inertial take: the machine clears the records, pushes none and requests an instance take of the candidate's duration, which the instance's inertialiser measures against the instance's latest output ("Inertial blending").
4. After the candidates: advance every active record's progress by `Δt / duration`; records whose progress reaches 1 are removed oldest first, and when the newest completes the machine has no active transition; advance the time in state by Δt.

Conditions (`Zenith_ConditionOp`): `GREATER` value > operand; `LESS` value < operand; `EQUAL` value == operand and `NOT_EQUAL` value != operand (exact compares of the stored f32); `TRUE`, `FALSE` on a BOOL; `TRIGGERED` the trigger is set; `STATE_TIME_AT_LEAST` time in state ≥ operand seconds; `CLIP_READY` every clip in the destination state's subtree is bound and, when streamed, its next chunk resident; `EVENT_FIRED` an event with the condition's name hash fired from the current state's subtree in the previous evaluation. A transition with an exit time additionally requires the leader's normalised time to have crossed the exit time during the previous evaluation's advance (wrapping for a loop clip). A transition with zero conditions and no exit time passes always.

### Transition blending

While records are active, the pose is the fold, oldest first: `P = pose(source of record 0)`, then for each record i, `P = Blend(P, pose(destination of record i), curve(progress_i))`, with `curve` linear (`p`) or smoothstep (`p² (3 − 2p)`). The update pass gives the states the matching weights: with α_i the curve values, the destination of record i weighs `α_i × Π_{k>i} (1 − α_k)` and the source of record 0 `Π_k (1 − α_k)`; a state appearing twice sums its weights. Every active state's subtree is evaluated, so a crossfade costs both poses.

### Inertial blending

<!-- elaboration -->
Inertialisation is **per instance**: each animator instance has one inertialiser, which runs once per evaluation on the instance's whole graph output G (the pose after every layer and the curve overrides, "Pose pass"), after the pose pass and before motion warping and the IK stage (`Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"` step 3). Every take is therefore measured against, and added to, the same pose, whichever node requested it and whatever layers sit above that node: an override layer of weight α over a machine that takes inertially does not pop by `α × (x_prev − L)`, and an additive layer above it is not applied twice.

**Take requests.** These request a take of duration T: a state-machine transition flagged inertial and the fifth interrupt ("Transition evaluation" step 3); `Animation.ForceState` with seconds > 0 ("State machine state table"); a `SWITCH` change with its inertial flag ("Node catalogue"); a slot starting a clip while it already plays one, with T its blend-in, and a cinematic skip of a cinematic-tagged slot, with T = 0.15 s ("Slots", `Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"`); a motion-matching switch, with T the node's blend seconds ("Motion matching node"); a graph hot reload, with T = 0.15 s ("Graph hot reload"). A request records only `{pending, T}` in the instance record (`m_bInertialTakePending`, `m_fInertialTakeSeconds`, `Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"`), whether it is raised in the update pass of an evaluation, played back at a sync point or raised by the lifecycle listener before the instance's next evaluation; several requests before one inertialiser run keep the largest T (deterministic: a maximum is order-independent). A request with T = 0 takes nothing: the change shows at once. A reset ("State machine state table") clears both the pending request and the active offset.

**Take.** When a take is pending, the inertialiser reads two history poses of the instance, both still unmodified because the history rotates after it (`Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.Evaluate"` step 4): `x_prev`, the **current** history pose (the inertialised output of the latest evaluation, before motion warping and IK), and `x_prev2`, the **previous** one (the evaluation before that), and the instance record's `m_fLastDeltaSeconds` (Δt_last, the Δt of the evaluation that produced `x_prev`, which spans k ticks for a reduced-rate instance). For each joint it records the offset between `x_prev` and this evaluation's G (`x_dest`): a translation offset `o_t = t_prev − t_dest` with length `x0` and unit direction `u` (`u = 0` when `x0 < 1e-7`), and a rotation offset `o_r = r_prev · r_dest⁻¹` (sign-canonicalised to `w ≥ 0`) as a unit axis `u` and an angle `x0 ∈ [0, π]`; per curve the offset `o_c = c_prev − c_dest`. The velocity along each offset is the source's velocity over the last evaluation: translation `v0 = dot((t_prev − t_prev2) / Δt_last, u)`; rotation `v0 = dot(axisAngle(r_prev · r_prev2⁻¹) / Δt_last, u)` (axis times angle, canonicalised); `v0 = 0` when Δt_last is 0 (no previous evaluation). It stores `(u, x0, v0, t1)` per channel in the instance's inertial buffer, sets `elapsed = 0`, `T` and `active = 1`, and clears the pending request. A take while an earlier one is active restarts from the current output: `x_prev` already contains the earlier offset at its current decay, so the new offset replaces it with no jump.

**Application.** In every evaluation in which the buffer is active, including the take's own: `elapsed += Δt` (this evaluation's Δt, so the take's own evaluation continues the source's motion with velocity v0 instead of repeating `x_prev`), then the output is G with each channel's offset at `elapsed` applied: translation `+ x(elapsed) × u`, rotation pre-multiplied by the rotation of angle `x(elapsed)` about `u`, curves `+ o_c × max(0, 1 − elapsed / T)`; `active` clears once `elapsed ≥ t1` for every channel and `elapsed ≥ T` for the curves. When the buffer is not active the output is G. Per channel, with magnitude `x0`, velocity `v0` (clamped so `x0 × v0 ≤ 0`; a velocity pointing away from zero is set to 0), and the blend duration `T`:

```
t1 = (v0 < 0) ? min(T, −5 x0 / v0) : T
a0 = (−8 v0 t1 − 20 x0) / t1²
A  = −(a0 t1² + 6 v0 t1 + 12 x0) / (2 t1⁵)
B  =  (3 a0 t1² + 16 v0 t1 + 30 x0) / (2 t1⁴)
C  = −(3 a0 t1² + 12 v0 t1 + 20 x0) / (2 t1³)
x(t) = A t⁵ + B t⁴ + C t³ + (a0 / 2) t² + v0 t + x0,  x(t) = 0 for t ≥ t1
```

(the quintic of Bollo's inertialisation, evaluated in `double`). The curve is a function of `(x0, v0, t1)` alone, so the buffer stores those three per channel and every evaluation recomputes `a0`, `A`, `B`, `C` in `double` from them (bit-identical each time) and evaluates `x(t)` at the elapsed time.

**The buffer.** One per instance, allocated in the instance block when the bound graph `Inertialises()` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationGraphAsset header"`: the graph has a `SWITCH`, `STATE_MACHINE`, `SLOT` or `MOTION_MATCH` node, the node kinds through which every take request above arises, the hot reload of such a graph included); an instance of a graph without one of those nodes has no buffer and no request can arise for it (a development-build hot reload into such a graph shows the change at once). The layout:

| Part | Bytes | Content |
|---|---|---|
| header | 16 | `f32 elapsed` (seconds since the evaluation that produced `x_prev` at the take), `f32 T`, `u32 active` (0 none), `u32 reserved` |
| per joint, translation channel | 24 | `f32 u[3]; f32 x0; f32 v0; f32 t1` |
| per joint, rotation channel | 24 | `f32 u[3]; f32 x0; f32 v0; f32 t1` |
| per curve | 4 | `f32 offset o_c` (curves inertialise linearly) |

So a buffer is `48 × J + 4 × C + 16` bytes, padded to 16, once per instance (`Zenith2/Docs/Design/ANIM/Runtime.md → "Pose store layout"`); the inertialiser costs O(J + C) per evaluation while active and nothing otherwise. `Test_InertialBlend_VelocityFromHistoryAtReducedRate` asserts the take's velocities at a reduced rate and the continuation of the curve from the stored state; `Test_InertialBlend_NoPopUnderOverrideLayer` and `Test_InertialBlend_ForceStateWithoutInertialTransitions` assert the instance-level measurement and the request path (`Zenith2/Docs/Design/ANIM/Tests.md → "State machine tests"`).

### State machine state table

| State | Event | Next state | Action |
|---|---|---|---|
| `IDLE` (no record) | a crossfade candidate with duration > 0 passes | `TRANSITIONING` | push its record |
| `IDLE` | an inertial candidate with duration > 0 passes | `IDLE` in the destination | request an instance take of its duration ("Inertial blending") |
| `IDLE` | a candidate with duration 0 passes | `IDLE` | switch at once; at most 4 per evaluation |
| `TRANSITIONING` | the newest record's progress reaches 1 | `IDLE` | remove every record; inactive states' subtrees stop being evaluated (their node state is kept, and reset on re-entry when the state resets time) |
| `TRANSITIONING` | a crossfade interrupt candidate passes, fewer than 4 records | `TRANSITIONING` | push the record |
| `TRANSITIONING` | a crossfade interrupt candidate passes, 4 records (the fifth interrupt) | `IDLE` in the destination | clear the records; request an instance take of the candidate's duration |
| `TRANSITIONING` | an inertial interrupt candidate passes | `IDLE` in the destination | clear the records; request an instance take of its duration |
| any | `Animation.ForceState(machine, state, seconds)` command | `IDLE` in the named state | ignoring conditions: clear the records, enter the state as a take does, and request an instance take of `seconds` (none when 0), whether or not the machine has inertial transitions |
| any | reset (a teleport without `TELEPORT_FLAG_KEEP_ANIMATION_PHASE`, a rebind, `Animation.ResetGraph`) | `IDLE` in the entry state | clear the records and the subtree times, and the instance's inertial state (pending request and active offset); time in state 0 |
| any | the graph asset is reloaded | per "Graph hot reload" | — |

### Triggers

A trigger set by a command at a sync point takes the value 1 and the hold counter of its declaration (1 … 255). It is consumed (value 0) by the first transition that is taken with a `TRIGGERED` condition naming it; otherwise its counter decreases by one at the end of each evaluation of the instance and the trigger clears when the counter reaches 0. A trigger therefore survives a reduced-rate instance's skipped ticks until its next evaluation, and never leaks across more evaluations than its declaration allows.

## Layers and masks

### Layer blend modes

After the base layer, each layer i (in layer order) with weight `w_i` and mask m is applied to the accumulated pose P:

| Mode | Rotation space | Per joint j (α = `w_i × m_j`) | Per curve c (α = `w_i × m_c`) |
|---|---|---|---|
| override | local | `P_j = Blend(P_j, L_j, α)` | `Blend` |
| override | model | "Model-space rotation blending" | `Blend` |
| additive | local | `P_j = ApplyAdditive(P_j, L_j, α)` | additive add |
| additive | model | the model-space additive of "Additive application" | additive add |

A layer whose weight is 0 is skipped entirely (no update pass): its machines' times freeze until it regains weight, unless the layer is flagged "always update".

### Mask weights

A mask is J + C weights in [0, 1] (`Zenith2/Docs/Design/ANIM/Formats.md → "Mask document schema"`); no mask means 1 everywhere. Masks also apply to `LAYERED` node overlays and to slots.

### Model-space rotation blending

For an override layer in model space (an upper-body aim that must stay world-stable while the hips turn): compute the model rotations of P (`G_j.r`) and of the layer pose (`H_j.r`); for each joint in joint order, the blended model rotation is `Zenith_Nlerp(G_j.r, H_j.r, α)`; the local rotation is re-derived as `parentBlended⁻¹ · blended`, where `parentBlended` is the parent's blended model rotation (joint 0 uses the identity); translations and scales blend in local space. O(J).

## Slots

A slot is a named place in the graph where gameplay or a cinematic plays a clip the graph does not name (a hit reaction, an interaction, a cinematic performance). The `SLOT` node's 32-byte instance state: `u32 runtime clip binding (0xFFFFFFFF none); f32 time; f32 weight; f32 blend in (s); f32 blend out (s); f32 rate; u16 flags (bit 0 loop, bit 1 stopping, bit 2 root motion, bit 3 cinematic-owned); u16 tag; u32 reserved`.

| Event | Effect |
|---|---|
| `Animation.PlaySlot(slot, clip, blendIn, blendOut, rate, startTime, loop, tag)` played back at a sync point | the clip is requested for binding (`Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"`); when the binding publishes, the slot starts at startTime with weight ramping from its current weight to 1 over blendIn; a slot already playing a clip switches to the new one at once (its binding, time startTime and rate replaced; its weight kept and ramping to 1 over blendIn) and requests an instance take of blendIn seconds ("Inertial blending"), so the outgoing clip needs no second record in the 32-byte state |
| each evaluation | `t += Δt × rate` (loop wraps, non-loop clamps); weight ramps by smoothstep; a non-loop clip begins its blend-out at `D − blendOut` |
| `Animation.StopSlot(slot, blendOut)` | weight ramps to 0 over blendOut; at 0 the binding is released |
| blend-out completes | the slot is empty and the node outputs its input unchanged |

The slot's pose replaces the input as `Blend(input_j, slot_j, weight × mask_j)`; its events fire as a clip of path weight `pathWeight × weight` (`WEIGHTED` mode); its root motion, when the slot and the clip both allow it, joins the root-motion blend with that weight. `Zenith2/Docs/Design/ANIM/Runtime.md → "Lifecycle listener"` defines how cinematic interrupt and skip stop cinematic-owned slots.

## Synchronization

### Sync groups

A sync group keeps the clips that move together (the gait clips of a blend space, the two states of a transition between walks) at one cadence. Members are the clip nodes whose record names the group, or whose nearest blend-space ancestor declares it. Each evaluation, after the update pass has registered every member with its path weight (zero-weight members included):

1. The **leader** is the member with the largest path weight (ties: the lowest node index).
2. The group's mode advances the leader and positions the followers (below).
3. A member that joins (its weight was 0 at the previous evaluation) is positioned by the same rule, so it enters in step.

### Normalised-time sync

The group keeps a phase φ ∈ [0, 1). With member durations `D_i` and weights `w_i`, `D_eff = Σ w_i D_i / Σ w_i`; `φ' = φ + Δt × rate_leader / D_eff` (wrapping for loop members, clamping at 1 otherwise); every member's time is `t_i = φ' × D_i`. A walk blending into a run therefore changes cadence continuously and foot contacts stay aligned when the clips are authored with matching phase.

### Marker sync

With sync markers (`Zenith2/Docs/Design/ANIM/Formats.md → "Event and marker sections"`):

1. The leader advances by its own time `t_L' = t_L + Δt × rate_L`.
2. The leader's position is the marker pair (A, B) it lies between (A the last marker at or before `t_L'`, B the next, wrapping for a loop clip) and the fraction `f = (t_L' − t_A) / (t_B − t_A)`.
3. Each follower finds, starting from its previous pair, the first consecutive marker pair whose names are (A, B) (searching forward, wrapping once for a loop clip), and takes `t = t_A' + f × (t_B' − t_A')`.
4. A follower with no such pair, or a leader with fewer than two markers, falls back to normalised-time sync for that evaluation (`anim.sync.marker_fallback_delta`).

Marker sync aligns contacts even when the clips' phases differ (a limp against a walk). Complexity O(members × markers) per group.

## Animation events

### Event firing rules

For every clip node (and slot clip) whose time advanced in the evaluation, from `t` to `t'` with w its path weight:

1. The crossed interval: forward, `[t, t')` plus each full wrap (a loop clip that wrapped n times covers `[t, D)`, n − 1 whole loops and `[0, t')`); backward (negative rate), `(t', t]` with the same wrap rule; the first evaluation of a clip node includes time 0 (`[0, t')`).
2. An instant event fires once per crossing of its time.
3. A notify state fires `BEGIN` when its time is crossed and `END` when `time + duration` is crossed (mod D for a loop clip); when a clip with an open notify state stops contributing (its weight reaches 0, its state is left, its slot empties, the instance resets), the open state fires `END` in that evaluation, so begins and ends always pair.
4. Gate by fire mode: `ALWAYS` fires always; `LEADER_ONLY` fires only when the clip is its sync group's leader or belongs to no group; `WEIGHTED` fires when `w ≥` the event's minimum weight.
5. Order: by (layer index, node index, crossing order along the advance direction, event table index).
6. At most `uANIM_MAX_EVENTS_PER_INSTANCE_PER_EVALUATION` (64) events per instance per evaluation fire; the rest are dropped in that order and counted (`anim.events.dropped_delta`).

`Test_AnimEvents_LoopWrapFiresOncePerCrossing`, `Test_AnimEvents_BackwardPlayback`, `Test_AnimEvents_NotifyStateEndOnExit` and `Test_AnimEvents_WeightGate` assert the rules.

### Event delivery

Each fired event is emitted, in the order above, as one `Zenith.Animation.Notify` ECS event through `Zenith_SystemContext::Events(simulationID).Emit` (`Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"`), so it is merged at phase 3's sync point by merge key and delivered in phase 9 of the same tick ([Z2-N-ARCH-478]); C++ consumers (AUDIO, VFX, GAME) declare `ReadsEvent` on the type, and scripts receive it in their event handlers. The payload (`Zenith_AnimNotifyEvent`) carries the sub-tick time of the crossing, `m_fTimeIntoTick`, for audio scheduling. An event emitted by a reduced-rate evaluation covering k ticks carries the tick fraction computed over the whole span (0 … k), so consumers can place it.

## Root motion

### Root motion sampling

A clip's motion transform at time t, `Q(t)`, interpolates the root-motion frames like joint tracks (linear translation, `Zenith_Nlerp` rotation, the clip's frame timing). The motion delta of an advance from t to t' is, in the clip's motion space at t:

- forward without a wrap: `Δ = Q(t)⁻¹ · Q(t')`;
- forward with n ≥ 1 wraps of a loop clip: `Δ = Q(t)⁻¹ · Q(D) · Q(D)^(n−1) · Q(t')` (each whole loop contributes `Q(D)`, because `Q(0)` is the identity);
- backward: the inverse of the forward delta from t' to t;
- a mirrored clip: the delta mirrored through the skeleton's mirror plane (translation component and rotation axis reflected).

### Root motion blending

Root-motion contributions come from clips of the base layer (layers above it and additive subtrees never contribute) and from slots that allow root motion, each with its path weight. The instance's delta is `Zenith_BlendDualQuaternions` of the contributions' deltas with their weights normalised to sum 1 ("Rigid delta blending"); no contribution gives the identity. The animator's root-motion mode `IGNORE` discards it; `EXTRACT` publishes it ("Root motion publication").

### Root motion publication

The delta is published in the entity's local space at the start of the tick into the `Zenith.Animation.RootMotion` component (`Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"`), whose properties phase 3 owns; phase 3 never applies it to a transform ([Z2-N-ARCH-475]). In phase 4 it is applied, by the orchestrator's Q72 ruling: PHYS's character-controller node `Physics.CharacterMove` applies it, through collision, to every entity that has a `Zenith.Physics.CharacterController` (cross-area row AN6), and ANIM's `Animation.ApplyRootMotion` applies it to every other entity through `SetProperty` commands on `Zenith.LocalTransform` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Animation.ApplyRootMotion"`). GAME registers no phase-4 node. A reduced-rate instance spreads its span's delta over the following ticks (`Zenith2/Docs/Design/ANIM/Runtime.md → "Root motion under reduced rates"`).

## Graph compilation

### Compile steps

The `anim.graph` cook step (and the editor's preview compile, the same function `Zenith_CompileAnimationGraph`, declared in `Zenith2/Tools/Authoring/Zenith_AuthoringAnimation.h` and implemented in `Zenith2/Tools/Authoring/Internal/Animation/Zenith_GraphCompiler.cpp`, the tool library `ZenithAuthoring` of ruling Q84 (a), `Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"`). Steps 1 and 2 are the caller's gathering of the inputs the compiler receives in `Zenith_AnimGraphCompileInputs` (the cook step through its context, which records the cook inputs; the editor from its open documents); steps 3 to 9 are the compiler's:

1. Open the graph document through `OpenSourceDocument`, and every mask document it names; validate members, enumerations and references (a persistent ID that names nothing in the document is an error).
2. Read the skeleton's published properties and, for each clip, `ReadDependencyOutput(clip, "anim.clipInfo")` (duration, flags, skeleton GUID, rest pose hash, marker names), which records each as a cook input.
3. Refuse a clip of another skeleton that is not marked retargeted, and a clip whose rest pose hash differs from the skeleton's (`ANIM_GRAPH_DIAG_SKELETON_MISMATCH`).
4. Build the forest: for each layer in order, a post-order traversal from its root with children in document order; a node reached twice (shared) or never (orphan) is an error (`ANIM_GRAPH_DIAG_NODE_SHARED`, a warning `ANIM_GRAPH_DIAG_NODE_UNREACHABLE` for an orphan, which is not compiled).
5. Subtree sizes and the pose-stack depth, the exact integer the loader recomputes (GV5). Per node, with children in document order and `i` counted from 0:
	- `CLIP`, `REST_POSE`, `MOTION_MATCH`: `d = 1`;
	- `BLEND`, `BLEND1D`, `BLEND2D`, `BLEND_DIRECT`, `SWITCH`, `ADDITIVE`, `LAYERED` (base, then overlays): `d = max over i of (d(child_i) + i)`, because every evaluated child stays on the stack until the node combines them;
	- `STATE_MACHINE` with S states: the pose pass evaluates at most `m = min(S, 1 + uANIM_MAX_ACTIVE_TRANSITIONS)` distinct active states, in fold order, so `d = max over states s of d(s) + m − 1`;
	- `SLOT`: `d = max(d(input), 2)` (the slot's clip is sampled one entry above its input); `MIRROR`: `max(d(input), 2)` (the mirrored copy); `POSE_BLEND`: `max(d(input), 3)` (the library pose and its neutral frame); `TIME_SCALE`: `d(input)`;
	- the graph: `D = max(d(root of layer 0), max over layers L ≥ 1 of (1 + d(root of L)))`, since the accumulated base pose stays on the stack while each further layer evaluates;
	- the IK stage uses no pose-stack entry (it works on the instance's animated pose buffers, `Zenith2/Docs/Design/ANIM/Runtime.md → "IK stage order"`), and neither does the inertialiser (it works on the instance's inertial buffer after the pose pass, "Inertial blending").
	`D > 16` is `ANIM_GRAPH_DIAG_POSE_STACK_DEPTH`.
6. Lay out the per-instance state: each node's state at a 16-byte-aligned offset in node order. The instance's one inertial buffer is not part of this layout: the loader derives `Inertialises()` from the node kinds ("Inertial blending"), so the format carries no buffer count.
7. Sort parameters by name hash (collisions are an error), remap every reference; add the reserved parameters the document uses.
8. Compile blend spaces (sorting, triangulation), masks (quantisation), state machines (any-state transitions first, then each state's in authored order), conditions (type checks: `GREATER` on a TRIGGER is an error), slots, sync groups (a marker group whose clips have fewer than two markers is a warning), the IK stage (definition names checked against the skeleton's IK definitions).
9. Write the sections; compute the graph hash; publish `anim.graphInfo` for tools.

### Compile diagnostics

`Zenith_AnimGraphDiagnosticCode` (tool-side, in `Zenith_AuthoringAnimation.h`; returned in the compile result's sorted diagnostics, which the cook step reports through `Zenith_CookContext::Report` with the node's name and stable ID and the editor shows on its nodes):

| Code | Severity | Message form |
|---|---|---|
| `ANIM_GRAPH_DIAG_REFERENCE` | error | `animgraph: <document>: '<member>' names <id>, which is not a <kind> of this document` |
| `ANIM_GRAPH_DIAG_SKELETON_MISMATCH` | error | `animgraph: clip <guid> is cooked for skeleton <guid>, not <guid>` |
| `ANIM_GRAPH_DIAG_NODE_SHARED` | error | `animgraph: node '<name>' has two parents ('<a>', '<b>')` |
| `ANIM_GRAPH_DIAG_NODE_UNREACHABLE` | warning | `animgraph: node '<name>' is not reachable from any layer and is not compiled` |
| `ANIM_GRAPH_DIAG_POSE_STACK_DEPTH` | error | `animgraph: layer '<name>' needs a pose stack of <d>; the limit is 16` |
| `ANIM_GRAPH_DIAG_LIMIT` | error | `animgraph: <quantity> is <n>; the limit is <m>` |
| `ANIM_GRAPH_DIAG_PARAMETER_TYPE` | error | `animgraph: condition <i> of transition '<name>' applies <op> to <kind> parameter '<p>'` |
| `ANIM_GRAPH_DIAG_BLEND_SPACE_DEGENERATE` | error | `animgraph: blend space '<name>': <reason>` |
| `ANIM_GRAPH_DIAG_RESERVED_NAME` | error | `animgraph: parameter '<name>' uses the reserved prefix 'Zenith.'` |
| `ANIM_GRAPH_DIAG_MASK` | error | `animgraph: mask <guid> entry <i> names '<joint>', not a joint of skeleton <guid>` |
| `ANIM_GRAPH_DIAG_MARKERS_MISSING` | warning | `animgraph: sync group '<name>' uses markers but clip <guid> has <n>` |
| `ANIM_GRAPH_DIAG_IK_DEFINITION` | error | `animgraph: IK step <i> names '<name>', not an IK definition of skeleton <guid>` |

Every error of one document is reported in one compile, sorted by (code, node order).

## Graph hot reload

When a graph asset is reloaded in a development build (the editor recompiles a document while a play world runs), ANIM rebinds every instance of it at the next tick boundary through its asset-binding publication ("Asset bindings" of `Zenith2/Docs/Design/ANIM/Runtime.md`):

1. Parameters: copied by (name hash, kind); others take defaults.
2. Nodes: a node of the new graph whose stable ID and kind exist in the old copies its instance state (times, weights, slot state); others start fresh.
3. State machines: the current state is kept when its stable ID exists, otherwise the entry state; active transition records are dropped and the instance requests a take of 0.15 s ("Inertial blending"), measured at the first evaluation of the reloaded graph against the instance's current history pose, so the reload never pops; the instance block is reallocated for the new graph's layout in the publication handler, the history poses and the inertial state are copied into it, and a reloaded graph that does not `Inertialises()` drops the request (development builds only).
4. A reload whose skeleton differs from the bound skeleton resets the instance.

`Test_AnimGraph_HotReloadKeepsStateAndParameters` covers it. Hot reload is a development-build feature: shipping builds never reload assets (ASSET's rule), so it cannot affect a recorded replay.

## Motion matching node

<!-- elaboration -->
Node kind 15 is `Z2-R-ANIM-179`; its tickets are blocked by the D-SEL-MOTIONMATCH (open, Stage 3) ruling ticket, which reads the measurements of the Stage 3 harness (`Zenith2/Benchmarks/MotionMatching/`, `Z2-R-ANIM-178`) against `Z2-B-ANIM-001` and `Z2-B-ANIM-002`. The search is `Zenith_MotionMatcher` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"`, the decision's Noun), the same code the harness measures, so the ruling measures what the node runs. A ruling that excludes motion matching changes the inventory through its change rule, which reclassifies the requirement in that diff; this design states no alternative behaviour.

| Part | Specification |
|---|---|
| database | `ANIM_SECTION_GRAPH_MOTION_DATABASE`: every frame of the node's clips at 30 Hz except the last second of each non-loop clip; per frame a 27-float feature vector: left and right foot model position (6) and velocity (6), hips velocity (3), future root positions at +0.33, +0.67 and +1.0 s in the frame's motion space (xz, 6) and future facing directions (xz, 6); per-dimension mean and standard deviation; per-frame (clip index, time); an axis-aligned bounding box per 16 consecutive frames |
| payload | `u32 database offset; f32 search interval (s, default 0.1); f32 blend seconds (default 0.2); f32 feature weights[5] (feet, foot velocities, hips velocity, trajectory positions, trajectory facings); u16 desired velocity x, z parameters; u16 desired facing x, z parameters` |
| instance state | 32 bytes: `u32 current frame; f32 time since search; u32 reserved (0); f32 current clip time; u32 current clip; u32 reserved[3]` |
| query | the current pose's foot and hips features plus a trajectory predicted from the desired-velocity and desired-facing parameters with a critically damped spring (half-life 0.2 s), normalised by the database statistics and weighted |
| search | every search interval: `Zenith_MotionMatcher::Search` over the section's database view with the payload's weights and the instance's current frame: the best frame by weighted squared distance, scanning the 16-frame boxes in order and skipping a box whose lower-bound distance exceeds the best found (ties to the lower frame index); the current frame's continuation wins ties; a non-finite query keeps the current frame and is counted |
| database build | the `anim.graph` cook step, for each node of kind 15, reads each of the node's clips' published `anim.rawClip` through `ReadDependencyOutput` (a cook-input edge, so editing a clip's motion or settings re-cooks the graph; `Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"`), runs on it the clip cook's stages 2 and 3 (root-motion extraction with the mode and joint `anim.rawClip` carries, then loop closure for a loop clip; `Zenith2/Docs/Design/ANIM/Formats.md → "Clip cook order"`, the same ZenithCook functions the `anim.clip` step calls) to obtain the in-place frames and the motion track, builds the section with `Zenith_MotionMatchDatabase::BuildFromFrames` over those frames and the skeleton's published `parents` (`Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"`), serialises its view's arrays and passes those bytes to the graph compiler as `Zenith_AnimGraphCompileInputs::m_xMotionDatabase`, which writes them as `ANIM_SECTION_GRAPH_MOTION_DATABASE` and includes them in the graph hash (ruling ANIM-F19-VENUE: the clip stages are ZenithCook functions ZenithAuthoring may not call; the editor's preview compile passes no database, the graph loader refuses a kind-15 node without the section, and the editor previews such a graph from the cook server's output, `Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"`). The database is built from the resampled double-precision source values rounded to f32, not from the compressed decode, so a database feature differs from the runtime pose's feature at the same frame by at most the clip's compression error, which moves only near-ties; the cooked bytes are fixed, so the search stays deterministic |
| switch | when the best frame is in another clip or more than 0.2 s from the current time, play it from that frame and request an instance take of the blend seconds ("Inertial blending") |
| cost | at most `N / 16` box tests plus the frames of unpruned boxes per search; the harness measures the W-Characters cost on the S24 |
