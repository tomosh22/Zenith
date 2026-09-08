# Mesh Animation System

## Overview

The mesh animation system provides skeletal animation for 3D models. It consists of animation clips that store keyframe data, skeleton instances that manage runtime bone state, and integration with the GPU skinning pipeline.

## Core Components

### Flux_AnimationClip
Stores animation keyframe data loaded from `.zanim` files.

**Bone Channels:** Each animated bone has a `Flux_BoneChannel` containing:
- Position keyframes (Vector3 + time)
- Rotation keyframes (Quaternion + time)
- Scale keyframes (Vector3 + time)

> **★ KEY TIMES ARE SECONDS, ON THE SAME CLOCK AS `m_fDuration` (D3).** Every
> `std::pair<V,float>::second` in a channel, every `Add*Keyframe` / `InsertKeyframeAt`
> argument and every `Sample*()` argument is a time in seconds
> (`Flux_AnimationClip.h`). They used to be TICKS — the channel stored
> `aiVectorKey::mTime` unconverted and `Flux_SkeletonPose::SampleFromClip` multiplied
> the incoming wall-clock seconds by the clip's ticks-per-second on the way in, so a
> clip carried two clocks and every generator, test and consumer had to remember which
> one it was holding. **`Flux_BonePose.cpp`'s two `SampleFromClip` overloads no longer
> convert at all**: `fTime` goes straight to the channel.
>
> `m_uTicksPerSecond` survives as **import provenance only** — the tick rate of the
> FILE the clip came from, so a re-export to a tick-based format can put the times back
> on the source's grid. Nothing samples through it. The tools-only
> `Flux_BoneChannel(const aiNodeAnim*, double dSourceTicksPerSecond)` constructor is
> where the division happens, and the divisor is a required argument so an import path
> cannot forget it.
>
> **There is no `GetDurationInTicks()`.** It was a second authority on the clip's
> length, expressed in the one unit nothing is in any more, and every caller of it was
> sampling with a tick number. Sample with `GetDuration()`. The one legitimate consumer
> is a re-export to a tick-based file format, which writes
> `GetDuration() * GetTicksPerSecond()` at the call site where the grid is visible
> (`Tools/Zenith_Tools_AssimpConvert.cpp`).
>
> `Flux_AnimationEvent::m_fNormalizedTime` is NOT part of this (D4) — an event time is
> a `[0,1]` fraction of the clip and stays one.

**Sampling:** `SamplePosition/Rotation/Scale(float fTimeSeconds)` interpolates between keyframes — **curve-interpolated through the per-key tangents since WU-8.1**; see *Tangent sampling* below for the two formulas and for the one decision the whole change rests on. Two free helpers make "the last key lands at or before the end of the clip" checkable now that both are in the same unit, and are pure/allocation-free so a generator and a headless unit can assert with the same call: `Flux_ClipLastKeyTimeSeconds(clip)` and `Flux_ClipKeyTimesFitDuration(clip, epsilon)`.

#### Tangent sampling (WU-8.1) — a strict superset, and why it is one

★ **AN UNSET (EXACTLY ZERO) TANGENT IS THE *LINEAR* TANGENT, NOT A FLAT ONE.**
That single reading is what let curve interpolation land without moving a byte or a
pose. Every generated clip in the tree and every clip Assimp imported carries zero
in every tangent slot, and the classical Hermite reading of a zero derivative is a
FLAT handle — which turns a straight segment into a smoothstep. Taking it would have
silently re-timed every animation in the repository into an ease-in/ease-out, with
no data moving and therefore nothing for a unit, a byte comparison or a bake hash to
catch.

So an unset tangent means "this end of this segment has no authored derivative" and
the sampler substitutes the segment's own linear slope there (for rotation, slerp's
own constant angular velocity). A segment bounded by **two** unset tangents runs the
pre-WU-8.1 expression verbatim — `glm::mix` / `glm::slerp`, no Hermite arithmetic
executed at all — so it is bit-identical rather than close, and that is the branch
every clip in the tree takes today. `Flux_TangentIsUnset` is the named predicate and
is an EXACT compare on purpose: it tests a default-constructed sentinel that
round-trips as exact zero bits, not a measurement.

★ **THE TRADE:** a genuinely flat tangent (zero derivative — the "ease" handle) is
NOT authorable, because the zero vector is spoken for. `ComputeFlatTangents` writes
zeroes and therefore produces LINEAR segments, which is what WU-8.1 specifies "flat"
mode to mean; telling flat from linear needs a per-key tangent MODE, which is a field
that is not on the wire and therefore a schema move. **Do not label a UI control
"flat" without reading this paragraph.**

| Track | Formula |
|---|---|
| position, scale | cubic Hermite, tangents in **units/second**: `P(u) = h00*P0 + h10*dt*m0 + h01*P1 + h11*dt*m1`. The `dt` factors are what make a tangent a VELOCITY rather than a handle length — retime a key and the curve keeps its physical slope. `m0 = m1 = (P1-P0)/dt` collapses the four terms to `(1-u)P0 + u P1`, and that identity is what "unset means linear" is built on |
| rotation | a **cumulative-Bezier quaternion Hermite** (Kim/Kim/Shin 1995) over the three control deltas `v1 = dt*w0/3`, `v3 = dt*w1/3`, `v2 = RotVec(E(v1)^-1 * q0^-1*q1 * E(v3)^-1)`, blended by the cumulative Bernstein basis `B1 = 1-(1-u)^3`, `B2 = 3u^2-2u^3`, `B3 = u^3`. Tangents are **body-frame angular velocities**, each in its OWN key's frame |

★ **THE ROTATION FORM IS THE CUMULATIVE ONE BECAUSE IT IS EXACT AT *BOTH* ENDS.**
`B1'(0) = 3` with `B2'(0) = B3'(0) = 0` makes the angular velocity at `u=0` exactly
`w0`, and symmetrically `B3'(1) = 3` makes it exactly `w1` — so C1 across a key is a
property of the construction, not of the segment being short. A single-chart log-space
Hermite (`q0 * E(hermite(u))`) is exact at `u=0` only: the differential of the
exponential map is the identity at the origin and not at `v`, so its end-of-segment
velocity is wrong by O(|v|) — invisible in any one pose, a hitch at every keyframe in
motion. And `w0 = w1 = v/dt` makes `v1 = v2 = v3 = v/3` about one axis, which commute
and sum against `B1+B2+B3 = 3u`, giving `q0 * E(u*v)` — slerp, exactly.

**Tangent MODES are an editor concept and are not stored.** The clip holds numbers.
Two pure whole-track presets realise a mode as numbers, both writing through the
`Set*Tangent` setters so there is one write path into the parallel arrays:

- `Flux_BoneChannel::ComputeAutoTangents(Flux_AnimTrack)` — Catmull-Rom: in = out =
  the centred slope `(v_{k+1}-v_{k-1})/(t_{k+1}-t_{k-1})`, one-sided at the two
  endpoint keys, zero where there is no span to divide by. For rotation, the same in
  angular-velocity terms, **rotated into key k's own body frame** by
  `q_k^-1 * q_{k-1}` — the raw relative rotation comes out in the EARLIER key's frame,
  and the sampler reads a tangent in its own key's.
- `Flux_BoneChannel::ComputeFlatTangents(Flux_AnimTrack)` — zeroes. See the trade above.

**`Flux_RootMotion` is still sampled LINEARLY**, deliberately. It carries no tangent
array (D17 declined to give it one, because that moves the `.zanim` layout), so
`SamplePositionDelta` / `SampleRotationDelta` are unchanged lerp/slerp.
**`Flux_SkeletonPose::SampleFromClip` needed no change either** — both overloads call
the channel samplers behind their `Has*Keyframes()` guards and do nothing else with
the values, so a tangent reaches a bone's local pose for free. That is a claim, so it
is pinned by two units in `Flux_BonePose.Tests.inl` rather than left as a comment.

**No schema bump.** The block D17 reserved is exactly what curve sampling needed —
in/out per key, angular velocity for rotation — so the wire format did not move and
`uZENITH_ANIMATION_SCHEMA_CURRENT` is still **2**. A re-bake of the generated clips is
byte-identical, because none of them authors a tangent.

**Loading:** `LoadFromAssimp()` (tools-only) imports from Assimp's `aiAnimation` structure. Binary `.zanim` files are loaded through the asset system via `Zenith_AnimationAsset::LoadFromFile()` (AssetHandling/Zenith_AnimationAsset.cpp), which now returns a `Zenith_Status` taken straight from `Flux_AnimationClip::ParseStream()` — a refused file no longer reports a successful load holding an empty clip.

#### Clip metadata (`Flux_AnimationClipMetadata`)

Beyond `m_strName` / `m_fDuration` / `m_bLooping` / `m_fBlendInTime` / `m_fBlendOutTime`:

| field | means |
|---|---|
| `m_uTicksPerSecond` | **import provenance** — the SOURCE FILE's tick rate. Never applied to a key time. Default 24 |
| `m_uAuthoredFrameRate` (D6) | **editorial intent** — the fps the clip was authored at: what a key grid snaps to, what a re-bake should resample to. Deliberately NOT `m_uTicksPerSecond`. Also never applied to a key time |
| `m_strSkeletonPath` (D7) | the RIG this clip animates, as an asset path |
| `m_strPreviewModelPath` (D7) | a model to preview it on, as an asset path |
| `m_bGenerated` (D8) | true when a generator produced the clip. A generated clip is rewritten in full on every tools boot, so this is what tells a consumer that editing it in place is pointless — it is what the Animation Editor's open path refuses on, offering promotion to an authored override instead |

★ **THE RIG IS NEVER INFERRED FROM BONE NAMES, and `m_strSourcePath` IS NOT THE RIG.**
The source path is the `.glb`/FBX the clip was imported from — a provenance breadcrumb
that is empty for every procedurally generated clip — so overloading it as the skeleton
reference would leave a generated clip unable to name its own rig and would silently
retarget an imported one onto its source file. Both new paths are normalized through
`Zenith_AssetRegistry::NormalizeAssetPath` on the way in and out of the stream, exactly
like `m_strSourcePath`, so an absolute authoring-machine path never reaches the file.

#### `.zanim` on the wire

- **The shared envelope (D1).** `WriteToDataStream` leads with
  `Zenith_WriteStreamHeader(stream, uZENITH_ANIMATION_ASSET_TYPE_ID,
  uZENITH_ANIMATION_SCHEMA_CURRENT)` — **type id 6, schema 2**, both declared in
  `AssetHandling/Zenith_AssetTypeIds.h`. `Export()` is `WriteToDataStream` +
  `WriteToFile`, so it inherits the header for free. Schema 1 was the first
  self-describing layout; schema **2 reinterpreted the key-time floats as SECONDS with
  no field moving**.
- **`ParseStream(stream)` is the load contract** and returns a status: no envelope (or
  a stream too short for one) → `BAD_MAGIC`; another asset's type id →
  `INVALID_ARGUMENT`; a newer envelope, or any schema that is not
  `uZENITH_ANIMATION_SCHEMA_CURRENT` → `VERSION_MISMATCH`. Every refusal asserts
  EXACTLY once and leaves the clip EMPTY (`ResetToEmpty`), never half-parsed. The void
  `ReadFromDataStream` remains only for `Zenith_DataStream`'s `<<`/`>>` dispatch.
- **`ParsePayload(stream, uSchemaVersion)` is the migrator's split point.** It reads the
  body from a cursor already past the envelope and validates nothing about the header.
  Its acceptance test is `1 <= schema <= current` under `ZENITH_TOOLS` and
  `== current` outside it, so the "no legacy branch in the runtime reader" ruling is
  enforced by the compiler rather than by convention. The one caller allowed to pass a
  non-current schema is the authored-clip migrator (`Tools/Zenith_Tools_AnimMigrate.cpp`
  — see `Tools/CLAUDE.md`).
- **★ CHANNELS ARE SERIALIZED IN BONE-NAME ORDER (D5), not hash order.** Walking
  `m_xBoneChannels` directly put them on disk in `Zenith_HashMap` bucket order, so the
  same clip built by two different insertion sequences serialized to different bytes for
  identical animation data. `WriteToDataStream` sorts a pointer array by bone name
  first. That was free while every `.zanim` was gitignored bake output; it stops being
  free the day one is committed, which `Assets/Authored/` now does.
- **The per-key tangent block (D17), SAMPLED since WU-8.1.** Each channel carries a
  `Zenith_Vector<Flux_KeyTangents>` parallel to each of its three key arrays — same
  size, zero by default, kept in lockstep by every add/insert/remove/retime path. It
  was **serialized and round-tripped but NOT sampled** from D17 until WU-8.1, purely
  so the on-disk layout would not have to move when curve-interpolated sampling
  landed — **and it did not: the schema is still 2 and no field moved** (see *Tangent
  sampling* above). ★ **A ROTATION TANGENT IS AN ANGULAR VELOCITY**, a
  `Vector3` in axis × radians-per-second form — the same shape as a position or scale
  tangent's units-per-second — because the natural derivative of a slerped rotation
  curve is a body-frame angular velocity. Quaternion Bezier control points would be four
  components meaningful only relative to their own segment's endpoints, and could
  neither be blended nor retimed. `Flux_RootMotion` carries **no** tangents and
  deliberately gains none.

#### Keyframe mutation (D9–D16)

The clip used to be **append-only** — `Add*Keyframe` plus `SortKeyframes`, three private
vectors and const-only getters — so nothing could remove, retime or revalue a key. The
mutators live on `Flux_BoneChannel`, are reached from outside through
`Flux_AnimationClip::GetBoneChannelMutable()` / `GetOrAddBoneChannel()`, and address a
key as **(track, key index)** through one selector enum:

```cpp
enum Flux_AnimTrack { FLUX_ANIM_TRACK_POSITION, FLUX_ANIM_TRACK_ROTATION, FLUX_ANIM_TRACK_SCALE };
```

★ **ONE SELECTOR ENUM RATHER THAN THREE OVERLOAD FAMILIES**, because the layer above
addresses a key as (bone, TRACK, keyIndex) and has to store that triple in an undo
record; three families would push the same switch into every undo command at every call
site. The VALUE, in contrast, IS an overload — a `Vector3` for position/scale, a `Quat`
for rotation — and a mismatched pair is refused at runtime with an assert rather than
reinterpreted.

`constexpr float fANIM_TIME_EPSILON = 1.0e-5f` is **the one time-comparison tolerance**
(D9): two key times within it ARE the same time, and nothing compares two key times with
`==`. It sits ~420× below the finest authorable frame grid (1/240 s) so it cannot merge
two keys on adjacent frames, and ~10× above float spacing at a realistic clip time
(2⁻²⁰ ≈ 9.5e-7 s at t = 10 s) so it cannot fail to recognise a key that has been through
a file. A dope sheet that picked its own hit-test tolerance would disagree with the
mutator about whether a slot is occupied, which is the one disagreement that can destroy
a key.

The policy, as implemented, and it holds for every entry point:

- **A refusal changes NOTHING** — no clamp, no merge, no partial edit — and each verb
  returns `true` only when the clip actually changed.
- **Times must be FINITE and NON-NEGATIVE (D10)**, or the call is refused with exactly
  one assert. A clamp would turn a caller's arithmetic slip into a real key at a real
  time that nothing downstream could tell from an authored one.
- **`InsertKeyframeAt` on an OCCUPIED time REPLACES that key's value in place (D11)**,
  keeping its index slot, its stored time and its tangent entry; on a free time it
  inserts at the sorted position with a zero tangent. Either way the resulting index
  comes back through `puOutKeyIndex`.
- **`SetKeyframeTime` onto an occupied time is REFUSED (D11/D25) — no silent merge.** A
  merge destroys a key during a drag, which is exactly when a user is least able to
  notice. On a free time the key and its tangent move together to their new sorted
  position.
- **Rotation values are NORMALIZED on write, and a quaternion shorter than
  `fANIM_MIN_QUAT_LENGTH` (1e-6) is REFUSED, not normalized (D15)** — `glm::normalize`
  of a zero-length quaternion is NaN, and one NaN rotation key poisons every pose the
  clip can produce at every time, through the slerp.
- **The clip DURATION is never touched (D12)** — it is authored, not recomputed — and
  **a key past the duration is legal (D13)**: the panel warns, the mutator does not veto.
- **The track is left TIME-SORTED**; there is no `SortKeyframes()` to remember
  afterwards, and the input is asserted sorted going in.
- **D14: an empty channel must not survive inside a clip.** The clip-level
  `RemoveKeyframe(bone, track, index)` removes the key and, when that leaves zero
  position AND zero rotation AND zero scale keys, removes the CHANNEL, via
  `PruneEmptyChannel(bone)`. ★ **AN EMPTY CHANNEL IS NOT NEUTRAL AND THE TWO SAMPLERS
  DISAGREE ABOUT IT**: `Flux_SkeletonPose::SampleFromClip` guards each track with
  `Has*Keyframes()` and leaves the bind pose alone, while the direct channel API
  (`Flux_BoneChannel::Sample*`) has no such guard and returns origin / identity / unit
  scale. Removing it gives "this bone is not animated" exactly one representation. A
  caller that mutates through `GetBoneChannelMutable()` bypasses the pruning (a channel
  cannot reach the map that owns it) and must call `PruneEmptyChannel` itself.
- **`Flux_RootMotion` takes the SAME enum and the SAME verbs on its two delta tracks
  (D16)**, minus scale: `FLUX_ANIM_TRACK_SCALE` is refused (false + assert). It shares
  the bone channel's implementation and simply passes no tangent array, so the lockstep
  rule is vacuous there rather than re-implemented.

#### `ReplaceContentsFrom` — a reload must not move the clip (D26/D27/D28)

A controller borrows the clip **pointer** (`Flux_AnimationClipCollection::AddClipReference`)
and a state machine resolves its clip references through that collection by name
(`Flux_AnimationStateMachine::ResolveClipReferences`). `ForceUnload` plus a fresh acquire
hands back a DIFFERENT address, so every borrowed pointer and every resolved blend-tree
node is left pointing at freed memory. `ReplaceContentsFrom(source)` copies the source's
contents INTO this object instead, as a **whole-object copy assignment** rather than a
field list — a member added to the class later is carried automatically instead of being
silently left behind. (That is also why the copy operations are `= default`ed
explicitly: an implicit copy assignment on a class with a user-declared destructor is
deprecated in C++20.)

★ **THE NAME IS IMMUTABLE ACROSS A REPLACE (D28).** The collection is name-keyed,
`AddClip` on a name collision DELETES the clip already there, and `ResolveClipReferences`
resolves through the same map — so a rename underneath a live clip corrupts two lookups
at once and neither reports it. A source whose name differs is refused (assert + false,
destination untouched); the one exception is a destination with no name yet, which is a
freshly constructed clip being populated, not a rename. Renaming is Save As.

★ **IT IS NOT A SYNCHRONISATION POINT AND DOES NOT CREATE ONE (D27).** It is a plain
non-atomic write over live data; the caller must guarantee no animation update is in
flight (the editor calls it from the main thread, between frames). Load and validate into
a TEMPORARY clip first — `Zenith_AnimationAsset::ReloadFromDisk` is the worked example,
staging into a stack-local clip so a file that fails to parse never reaches a live one.

Relatedly, `Flux_AnimationClipCollection::AddClip` / `AddClipReference` **assert on an
empty clip name**: an unnamed clip keys on `""`, two of them evict each other from
`m_xClipsByName` while both stay in `m_xClips`, and the ordered list and the map end up
disagreeing with a freed pointer in one of them.

### Flux_SkeletonInstance
Runtime skeleton state for a single animated entity.

**Per-Bone State:**
- Local position, rotation, scale (current pose)
- Model-space transform (computed from hierarchy)
- Skinning matrix (read CPU-side by the unified compute-skinning pass)

**Key Methods:**
- `SetToBindPose()` - Reset to skeleton asset's bind pose
- `SetBoneLocalTransform()` - Set individual bone's local TRS
- `ComputeSkinningMatrices()` - Walk hierarchy, compute skinning matrices

**Bone Ordering:** Bones are stored in hierarchical order (parents before children), allowing single-pass model-space computation without recursion.

### Flux_AnimationController
Manages animation playback for an entity.

**Playback State:** Tracks current animation clip, playback time, speed, and looping mode.

**Update:** Each frame advances time and samples the animation clip to update skeleton instance bone transforms.

**Blending:** Supports crossfading between animations via weighted bone transform blending.

**Direct-play SCRUBBING:** there was previously no way to ask for *the pose at a time* — every entry point ADVANCED a clock (`Update(dt)` steps, `PlayClip` restarts at zero), and the only time SETTER in the whole system was `Flux_BlendTreeNode_Clip::SetCurrentTimestamp` on the private, tools-only direct-play node. So:

- `HasDirectPlayClip()` — a direct-play clip is armed (`PlayClip` has run, `Stop` has not).
- `GetDirectPlayTime()` — that node's clip time in SECONDS, 0 when nothing is armed.
- `SeekDirectPlay(fTimeSeconds)` — evaluate the direct-play clip AT that time and apply it to the skeleton instance **without advancing any clock**. The pose is identical to one a tick reaching the same time would leave (both seed the bind pose and sample through the same helper, `SampleDirectPlayPoseAtCurrentTime`); a seek ignores playback speed, the paused flag and any in-flight crossfade, because a scrub is not a transition. It deliberately does NOT go through `UpdateWithSkeletonInstance`, which hands the frame to the LAYER path the moment any layer exists and would disable the preview it is trying to scrub.
- `WrapClipTime(clip, t)` — **PURE**: wrapped when the clip loops, clamped when it does not, returned unchanged for a clip with no duration. `SeekDirectPlay` folds through it, so a caller may pass a raw slider value.
- `SetEmitEventsOnSeek(bool)` / `GetEmitEventsOnSeek()` and `GetLastEventCheckTime()` — see *Event delivery* below.

★ **THE DECLARATIONS ARE NOT TOOLS-GATED even though direct play still is.** A caller in a non-tools build compiles and gets a documented `false`/`0.0f` rather than needing its own `#ifdef` around every call — there is no direct-play node to seek without `ZENITH_TOOLS`, and that is the whole of the behaviour.

**Per-frame DRIVE GUARD — one driver per controller per frame:** `TryBeginFrameDrive(pDriver, ulFrameToken)` / `ClearFrameDrive(pDriver)`, with `GetFrameDriveOwner()` / `GetFrameDriveToken()` and the sentinel `ulNO_DRIVE_FRAME`. The animator inspector ticks the entity's controller itself while the editor is Stopped (nothing else does — `Scene::Update` is not running), so a second panel that also ticked it would **double-tick**: the clip runs at 2× with both panels open and 1× with one, which reads as "the preview speed is wrong" rather than as two drivers, and no assert anywhere fires. The token is the caller's frame identity (`g_xEngine.Frame().GetFrameIndex()` for editor code); the FIRST claim in a given frame wins and every later one in that same frame is refused, **including a repeat by the same driver** — a second tick is a second tick whoever asks for it. Only the current owner may release. The pointer is identity only and is never dereferenced.

**Asset references — `Initialize(nullptr)` detaches, `ReleaseAssetReferences()` drops the lot.** A controller holds owning handles: `m_xSkeletonAsset` (taken by `Initialize`) and one `AnimationHandle` per `AddClipFromFile`, the latter pinning the assets behind the BORROWED clip pointers in `m_xClipCollection`. `Initialize(nullptr)` is a full DETACH — the skeleton handle is cleared FIRST and unconditionally, so "stop animating this instance" also gives the reference back; it used to `Set()` inside the `if (pxSkeleton)` branch and leave an AddRef'd cached pointer that nothing anywhere cleared. `ReleaseAssetReferences()` is the way to drop **everything** a controller holds while the registry is still alive: the skeleton handle, every animation handle and the clip collection, together, because they are one invariant — a borrowed pointer must never outlive the handle pinning it. It is a teardown verb, not a reset (the skeleton INSTANCE pointer, the state machine and the layers stay; `Update` no-ops without the asset, and anything that resolved a clip reference through the collection is left pointing at a clip the registry may now free). ★ **A controller owned by anything that can outlive `Zenith_AssetRegistry::Shutdown` MUST call it** — a handle released at atexit writes into freed memory, which asserts `"Release called on asset with 0 ref count"` only when the freed word happens to read zero and corrupts silently otherwise. `Zenith_AnimationPreviewSession::Close` is the worked example.

**Ownership (Wave-19):** A `Flux_AnimationController` is no longer a by-value member of `Zenith_AnimatorComponent`. It is owned by `Flux_AnimationControllerStore` (below) and the ECS component is a thin forwarding handle into it.

### Flux_AnimationControllerStore
Heap-stable owning store of one `Flux_AnimationController` per entity (`Flux_AnimationControllerStore.{h,cpp}`). Held by `Zenith_Engine` as `m_pxAnimationControllers`, reached via `g_xEngine.AnimationControllers()`. This is the Flux-side twin of WS18's `Flux_TerrainStreamingState` relocation: the heavy controller that used to live inside `Zenith_AnimatorComponent` moved here so the ECS component header carries no Flux include.

**Storage:** `Zenith_Vector<Flux_AnimationController*>` — each controller individually `new`/`delete`'d, so the pointer is **heap-stable** (it never moves when the vector grows or compacts). An index-by-entity-**slot** array (`Zenith_Vector<u_int>`, sentinel `0xFFFFFFFF`) maps an `EntityID` slot to its controller index for O(1), hash-free lookup. A parallel dense slot array repairs the slot→index mapping after a swap-and-pop removal.

**API:**
- `GetOrCreate(Zenith_EntityID) &` — controller for the entity, created on first call.
- `TryGet(Zenith_EntityID) *` — pointer-or-null, no allocation.
- `Get(Zenith_EntityID) &` — asserts present.
- `Destroy(Zenith_EntityID)` — **idempotent** (no-op when absent); returns whether it removed one.
- `GetCount()` — live controller count (test/diagnostic).

**Why heap-stable matters:** the component caches a `Flux_AnimationController*` for the hot path. Because the controller is keyed by the **stable** `EntityID` slot and never relocates, that pointer survives a component-pool relocation (swap-and-pop / `Grow`) and a cross-scene `MoveEntityToScene`. Pinned by the `Animator` suite in `Core/Zenith_UnitTests.Tests.inl`.

★ **AND IT SAYS NOTHING ABOUT WHAT IS INSIDE THE CONTROLLER (WU-6.3 / D44).** This paragraph used to read "…and games cache `Flux_AnimationLayer*` / `Flux_AnimationStateMachine*` into the controller's sub-objects… those pointers survive", which does not follow and was never true. Heap stability is a property of the store's ALLOCATION, and two ordinary controller verbs delete every layer it owns: `BuildFromControllerDef` rebuilds the layer list wholesale from a def, and `ReadFromDataStream` deletes and re-reads it. A game holding the pointer its `AddLayer` returned is reading freed memory from the first `.zanimctrl` load or scene deserialize onward — silently, because a freed layer usually still looks like one. **Hold the layer ID and call `GetLayerById` per use** (below). A state machine reached THROUGH a layer has the same lifetime as that layer and the same rule; the controller's own top-level `m_pxStateMachine` is a separate object with the controller's lifetime, replaced only by `CreateStateMachine` / `BuildStateMachineFromDef` / a stream read.

**Lifetime:** allocated in `Zenith_Engine::Initialise` (alongside the mesh subsystems) and deleted in `Zenith_Engine::Shutdown`. Controllers hold no GPU resources (the unified compute-skinning path reads their CPU skinning matrices), so there is no device-lifetime ordering constraint.

**Layering:** the store lives under `Flux/`, so it includes `Flux_AnimationController.h` freely. Its `.cpp` includes `EntityComponent/Zenith_Entity.h` for the `Zenith_EntityID` slot index (the store's single, allow-listed Flux→EntityComponent edge); the header only forward-declares the id.

## Skinning Equation

The fundamental skinning equation transforms vertices from mesh-local space to world space:

```
skinnedPos = sum(weight[i] * skinningMatrix[boneIndex[i]] * meshLocalPos)
```

Where each skinning matrix is:
```
skinningMatrix = modelSpaceTransform * inverseBindPose
```

**At Bind Pose:** When all bones are at their bind pose transforms, `modelSpaceTransform * inverseBindPose` should equal the expected bind pose world position for each bone.

## GPU Integration

Skeletal meshes are skinned through the **unified GPU-driven mesh path** (`Flux/UnifiedMesh`):

**Bone palette:** All live skeletons' `GetSkinningMatrices()` are concatenated CPU-side into a per-frame bone-palette SSBO (de-duplicated per `Flux_SkeletonInstance`). There is no per-instance bone constant buffer.

**Compute-skinning pre-pass:** A compute shader skins each animated submesh-instance's bind-pose vertices to OBJECT space into a shared, grow-only skinned-vertex arena (a buffer with both UAV and VERTEX usage).

**Draw:** The existing GPU-driven cull/draw/shadow kernels consume the arena as ordinary static geometry (they apply the object's model matrix), so skeletal meshes need no dedicated draw or shadow shaders.

## Animation Data Flow

1. **Export:** Assimp `aiAnimation` converted to `Flux_AnimationClip`, saved as `.zanim`
2. **Load:** `.zanim` loaded into `Flux_AnimationClip` at runtime
3. **Update:** `Flux_AnimationController::Update()` advances time, samples clip
4. **Apply:** Sampled TRS values written to `Flux_SkeletonInstance` bones
5. **Compute:** `ComputeSkinningMatrices()` walks bone hierarchy
6. **Palette:** live skeletons' skinning matrices concatenated into the per-frame bone-palette SSBO
7. **Skin:** the unified compute-skinning pre-pass writes object-space vertices into the shared arena
8. **Render:** the GPU-driven mesh path draws the arena like static geometry

## Animation State Machine

### The def / instance split (WU-6.1)

★ **THE STATE MACHINE USED TO BE BOTH HALVES ON ONE OBJECT.** `m_pxCurrentState`,
`m_pxActiveTransition`, `m_xParameters` and **two `FLUX_MAX_BONES` poses** sat beside
`m_xStates` behind a `// Runtime state` comment that marked the split without acting on
it. Nothing could be shared from a registry, and the controller serialized the whole
thing inline.

| Type | Holds | File |
|---|---|---|
| `Flux_AnimationStateMachineDef` | states, transitions, conditions, any-state transitions, the default state, parameter **declarations + defaults**, each container state's nested def | `Flux_AnimationStateMachineDef.{h,cpp}` |
| `Flux_AnimationStateMachine` | current state, active transition + its target, interruptible/priority, the shared-parameter pointer, the two poses | `Flux_AnimationStateMachine.{h,cpp}` |

`Flux_AnimationParameters`, `Flux_TransitionCondition`, `Flux_StateTransition` and
`Flux_AnimationState` moved into the def header with the def; every consumer reaches them
through `Flux_AnimationStateMachine.h` exactly as before.

- **The instance OWNS its def, by value**, and `BuildFromDef(const Def&, Flux_AnimationClipCollection* = nullptr)`
  takes a **COPY**. ★ A non-owning pointer would let two instances of one def share the
  def's blend trees — and a leaf carries its own PLAYHEAD
  (`Flux_BlendTreeNode_Clip::m_fCurrentTimestamp`). Two characters "on the same
  controller" would step each other's clips: not a def/instance split, but two instances
  wearing one instance's state.
- **The copy runs through the def's own serializer.** A blend tree is a polymorphic
  hierarchy with no clone verb, and `Write`/`ReadFromDataStream` is the one faithful walk
  of it that already exists and is already pinned by a test. A node field added later is
  carried by the copy the day it is carried by the file. What does NOT survive: state
  CALLBACKS (a function pointer is not authored data) and resolved clip POINTERS — which
  is why `BuildFromDef` takes the collection.
- **Imperative authoring is unchanged and is still the normal way to build one.**
  `AddState` / `RemoveState` / `GetState` / `SetDefaultState` / `AddAnyStateTransition` /
  `GetStates` / `GetName` / `ResolveClipReferences` on the machine all forward into the
  owned def. Every game and every existing unit builds a machine this way.
- `RemoveState` also drops the runtime pointers into the state it is about to free —
  including as a **transition target**, which the pre-split version left dangling.
- `ResolveClipReferences` now **recurses into sub-state machines**. It stopped at the top
  level before, so every clip leaf inside a nested machine stayed unresolved after a load
  and posed the bind pose forever — silently, because an unresolved leaf resets rather
  than asserting.
- `Flux_AnimationStateMachineDef::Clear()` resets **all five** members — it deletes every
  state (and with it every nested def), then clears the state map, the default-state name,
  the any-state transitions, the def's **own name** and the **parameter declarations** — so
  a cleared def is indistinguishable from a default-constructed one. The name and the
  declarations used to survive it, which was invisible because the only caller is
  `ReadFromDataStream` (it overwrites both immediately); `Clear()` is public, and the first
  external caller would have inherited the previous def's identity.

**Serialization is the def, and the byte layout did not move.** `Flux_AnimationStateMachine::WriteToDataStream`
is `m_xDef.WriteToDataStream`, field-for-field what it always wrote (name, default state,
parameters, states, any-state transitions). It is a **payload with no envelope**: WU-6.2
embeds it in a `.zanimctrl` and owns the magic/schema word, and adding one here would mean
two. The one on-disk change WU-6.1 makes is in the blend-space nodes — see D48 below.

★ **A sub-state machine is still a nested INSTANCE inside the def**, not a nested def
beside one. `Flux_AnimationState::CreateSubStateMachine` returns a drivable
`Flux_AnimationStateMachine*` and every game and test authors through it, so the state
holds the machine and the machine holds its own def; the split recurses correctly at each
level, but the instance half of a child lives inside the parent's def rather than beside
the parent's instance. That is the residual, and it is what a shared-def registry would
have to move.

### Flux_AnimationStateMachine
High-level animation control using a state-based model (Flux_AnimationStateMachine.h/cpp).

**Components:**
- **States** (`Flux_AnimationState`): Named states with blend trees, outgoing transitions, and optional sub-state machines
- **Transitions** (`Flux_StateTransition`): Rules for changing states with conditions, duration, and priority
- **Parameters** (`Flux_AnimationParameters`): Float, Int, Bool, and Trigger values used in transition conditions
- **Conditions** (`Flux_TransitionCondition`): Comparisons (Greater, Less, Equal, etc.) against parameters
- **Any-State Transitions**: Transitions that fire from any current state (checked before per-state transitions, skip self-loops)

**Key Design Decisions:**

1. **Transitions Only Checked When Not Transitioning**: The `Update()` method only checks for new transitions when `m_pxActiveTransition == nullptr`. This prevents:
   - Same transition being restarted every frame (causing transitions to never complete)
   - Lower priority transitions from interrupting higher priority ones

2. **Any-State transitions checked first**: `CheckAnyStateTransitions()` runs before per-state `CheckTransitions()`. Self-loops are skipped.

3. **Trigger Consumption**: Trigger parameters are consumed (reset to false) when evaluated. This ensures one-shot transitions.

4. **Exit Time Transitions**: Transitions with `m_bHasExitTime = true` only fire after the source animation reaches `m_fExitTime` (normalized 0-1).

5. **Priority Ordering**: Transitions are sorted by priority (highest first). When checking transitions, the first valid one wins.

6. **CrossFade API**: `CrossFade(stateName, duration)` creates a synthetic transition bypassing conditions. No-ops if already in the target state.

**Common Pitfall - Transition Restart Bug:**
If transitions are checked during an active transition AND the transition condition is still true (e.g., Speed > 0.1 remains true), calling `StartTransition()` will restart the transition, resetting elapsed time. The fix is to only check transitions when not already transitioning.

### AnimatorStateInfo (Flux_AnimatorStateInfo)
Runtime state introspection struct (Unity's `GetCurrentAnimatorStateInfo()`):
- `m_strStateName` - Current state name
- `m_fNormalizedTime` - Progress [0-1], integer part = loop count
- `m_fLength` - Clip duration in seconds
- `m_fSpeed` - Playback rate
- `m_bHasLooped` - True once normalized time has exceeded 1.0 (past first cycle)
- `m_bIsTransitioning` / `m_fTransitionProgress` - Transition state
- `IsName(const char*)` - Name comparison

### Parameters are the CONTROLLER's (D42)

★ **`Flux_AnimationController::SetFloat` WAS A SILENT NO-OP FOR EVERY LAYERED GAME.** The
shortcuts were `if (m_pxStateMachine) m_pxStateMachine->GetParameters().Set…`, and a
controller built out of LAYERS has a null `m_pxStateMachine`. Zenithmon's humans are
exactly that shape: `ZM_PlayerController::DriveAnimatorSpeed` sets `"Speed"` every frame
through `Zenith_AnimatorComponent::SetFloat`, and the layer's Idle↔Walk transition never
saw a thing. Nothing asserted; the character just never walked.

The controller now owns **one** `Flux_AnimationParameters` and publishes it:

- `Flux_AnimationController::GetParameters()` is the live set. `SetFloat` / `SetInt` /
  `SetBool` / `SetTrigger` / `GetFloat` / `GetInt` / `GetBool` read and write it, and are
  no longer inline in the header (they publish on demand).
- `PublishSharedParameters()` seeds every attached machine's **declarations** into the
  live set and then binds the set to the top-level machine and to every layer's machine
  via `SetSharedParameters`. Sub-machines are bound as they always were, by
  `Flux_AnimationStateMachine::SetState` / `StartTransition` passing `&GetParameters()` —
  which now resolves to the controller's set.
- **Seeding never overwrites a name already present.** Two layers commonly declare the
  same `"Speed"`; re-seeding on the second would snap the live value back to a default
  mid-play, which reads as a one-frame glitch and points at nothing.
- `SeedParametersInto` **recurses through container states**, so a parameter declared only
  inside a sub-machine still reaches the controller — otherwise a value set on the
  animator could never satisfy a condition two levels down.
- **Publication is LAZY**: the first `Update`, a `ReadFromDataStream`, or the first `Set*`
  naming something the live set does not carry yet. Authoring happens after the controller
  exists (add a layer → create its machine → declare its parameters → first frame), so a
  publish at construction would seed nothing. The latch is cleared by `CreateStateMachine`,
  the auto-creating `GetStateMachine`, `AddLayer` and `BuildStateMachineFromDef`.
- The **move operations re-bind** (`RebindSharedParameters`, allocation-free so it is
  `noexcept`-safe): every machine a move takes still points at the *source's* set, which
  is about to be destroyed.

`Flux_AnimationStateMachine::GetParameters()` returns the shared set when one is bound and
the **def's declaration table** otherwise — so a standalone machine (a unit, an editor
scratch graph) reads and writes its own declarations exactly as before, and only the
declarations are serialized.

### Sub-State Machines
States can contain nested state machines via `Flux_AnimationState::CreateSubStateMachine()`. Child state machines share the parent's parameters via `SetSharedParameters()` — which, under a controller, is that controller's one live set (D42 above). Entry starts at the child's default state.

### ★ Named blend-parameter bindings (D48) — a REPAIR, not a feature

**A blend space in a running game was frozen at its deserialized literal.**
`Flux_BlendTreeNode::Evaluate` takes no parameter set, and
`Flux_AnimationStateMachine::EvaluateState` called it with `(fDt, xOutPose, xSkeleton)` and
nothing else — so no parameter value could ever reach a 1D or 2D blend space. The only
thing that could move one was `SetParameter`, and **`SetParameter` had no caller outside
the unit tests** (grep: `Core/Zenith_UnitTests.Tests.inl`, `Flux_BlendTree.Tests.inl`,
`Flux_AnimationController.Tests.inl` — zero in `Games/**`). A blend space was
unusable in a shipping game and the units all passed.

The repair follows the file's own late-binding pattern — `m_strClipName` + `ResolveClip`,
a NAME in the def resolved against the live world:

- `Flux_BlendTreeNode_BlendSpace1D::m_strParameterName`, and
  `Flux_BlendTreeNode_BlendSpace2D::m_strParameterNameX` / `…Y` (the two axes bind
  **independently** — an X on `"Speed"` beside a Y left on its literal is the common
  case). All **serialized**: the binding is authored data, the value it reads is not.
- `Flux_BlendTreeNode::ResolveParameters(const Flux_AnimationParameters&)` is a virtual
  walk. Composites forward it to **every** child, not only the ones about to be evaluated,
  so a branch that becomes selected next frame already holds this frame's value.
- `EvaluateState` calls it on the root of a state's tree immediately before `Evaluate`, so
  the position tracks the named parameter **every frame**.
- `SetParameter` survives as the manual override and is what an UNBOUND space uses. On a
  bound space it is overwritten before the next pose — correct precedence (the authored
  binding beats a poke from outside) but a trap if you expect the poke to stick.
- A bound name that is **not declared** leaves the literal alone rather than reading
  `GetFloat`'s 0.0. Snapping a walk/run blend to zero over a misspelling reads as "the run
  animation stopped working"; a stuck literal at least plays what was authored, and the
  binding is visible in the def.

★ **THE ON-DISK NOTE.** These two/three strings are the only byte-layout change WU-6.1
makes, and a state machine IS serialized into a scene —
`Zenith_AnimatorComponent::WriteToDataStream` writes the whole controller, which writes the
machine inline. It is safe **today** only because no committed `.zscen` can contain a blend
space: nothing under `Games/**` constructs a `Flux_BlendTreeNode_BlendSpace1D/2D` at all
(the only in-tree constructors are the three unit `.inl` files). Any future change to a
blend-tree node's payload has the same reach and no version word to hide behind — WU-6.2's
`.zanimctrl` envelope is where that gets fixed.

### Deleted: the hand-rolled state-machine loader (D49)

`Flux_AnimationController::LoadStateMachineFromFile` and
`Flux_AnimationStateMachine::LoadFromFile` are **gone**, not migrated. They were a raw
`std::ifstream` slurp into a `Zenith_DataStream` with no envelope, no magic, no schema, no
writer counterpart and no defined extension — nothing could produce a file for them to
read. Neither had a caller anywhere in the tree. `Flux_AnimationStateMachine.cpp` no longer
includes `<fstream>`. The real path is `BuildFromDef` plus WU-6.2's asset.

### The animator controller asset (WU-6.2) — `.zanimctrl` and `.zanimmask`

★ **THERE WAS NO TOP-LEVEL CONTROLLER ASSET, AND WU-6.1's STATE-MACHINE DEF COULD
NOT BECOME ONE.** A `Flux_AnimationController` has an OPTIONAL top-level state
machine *and* N layers, each owning its own — and every layered game reaches its
graph THROUGH a layer, with `m_pxStateMachine` null (D42's Zenithmon no-op is the
same shape). Persisting one `Flux_AnimationStateMachineDef` therefore persists, at
best, the half nobody runs.

| Type | Holds | File |
|---|---|---|
| `Flux_AnimatorControllerDef` | name, clip PATHS, an optional **embedded** top-level `Flux_AnimationStateMachineDef`, the layer list, the monotonic layer-id counter | `Flux_AnimatorControllerDef.{h,cpp}` |
| `Flux_AnimatorControllerLayerDef` | stable id, name, weight, blend mode, `m_bEmitEvents`, a bone-mask ASSET PATH, an **embedded** SM def | same file |
| `Zenith_BoneMaskAsset` | `{bone name, weight}` pairs + an explicit `m_bHasAvatarMask` | `AssetHandling/Zenith_BoneMaskAsset.{h,cpp}` |

★ **THE STATE MACHINES EMBED AND THE MASKS DO NOT (D46).** A state machine's
states name *this* controller's clips (by name, through its clip collection) and
its parameters, so a shared SM def would be meaningless outside the controller
that owns it. A mask names BONES, so it is **skeleton**-scoped and shared verbatim
by every controller on that rig; embedding it would give one rig as many
independent copies of "upper body" as it has controllers, and no way to fix them
all at once. Hence one path string per layer, and `.zanimmask` as its own type.

**The clip list is part of the def.** Clip references inside a state machine are
by NAME and resolve through `Flux_AnimationClipCollection`, so a def naming no
files would rebuild into a controller whose every leaf posed the bind pose —
silently, because an unresolved leaf resets rather than asserting.
`GetClipPaths()` is the `AddClipFromFile` list.

**The envelope lives on the controller def** (`Zenith_WriteStreamHeader`, type id
**7**, schema **1**), which is exactly why `Flux_AnimationStateMachineDef`'s
serializer is payload-only: a `.zanimctrl` has one magic word and one schema word,
not three. `ParseStream` mirrors `Flux_AnimationClip::ParseStream` line for line —
no envelope → `BAD_MAGIC`, another type id → `INVALID_ARGUMENT`, any non-current
schema → `VERSION_MISMATCH`, each refusal asserting exactly once and leaving the
def CLEARED.

**Runtime API on `Flux_AnimationController`:**

- `bool BuildFromControllerDef(const Flux_AnimatorControllerDef&, const Zenith_SkeletonAsset* pxSkeletonForMasks)`
  — clips FIRST (the machines resolve through the collection, so the order is
  load-bearing), then the top-level machine, then every layer rebuilt wholesale.
  ★ **A dangling reference FAILS LOUDLY**: a mask path that does not resolve, a
  masked def built with no skeleton, or a clip that does not load returns FALSE
  with a `Zenith_Error` naming the path. The build still completes as far as it
  can, so the return value is the ONLY thing that says the result is incomplete.
- `bool ExportControllerDef(Flux_AnimatorControllerDef&) const` — the inverse, and
  what an editor Save will call.
- `Zenith_AnimatorComponent::LoadControllerAsset(path)` acquires the asset and
  calls the former, resolving masks against the rig its `ModelComponent` animates.

★ **THE PATH IS NOT SERIALIZED INTO A SCENE, DELIBERATELY.**
`Zenith_AnimatorComponent::WriteToDataStream` writes the whole controller INLINE
and committed `.zscen` files carry those bytes today; persisting a controller-asset
REFERENCE instead moves that layout and is a separate decision. `LoadControllerAsset`
is a runtime verb.

★ **THREE THINGS THE `.zanimctrl` CARRIES THAT A `.zscen` DOES NOT**, because
`Flux_AnimationLayer::WriteToDataStream` reaches committed scene bytes and may not
move: the layer **id**, `m_bEmitEvents` (D41 changed no schema, so a controller
restored from a scene comes back with every silenced overlay emitting again) and
the **bone-mask asset path**. The runtime layer gained `GetLayerId` /
`SetLayerId` and `GetBoneMaskAssetPath` / `SetBoneMaskAssetPath` as
NON-serialized fields for exactly this.

★ **A LAYER'S INDEX IS NOT ITS IDENTITY**, which is what the id is for: inserting
or removing a layer renumbers every one above it, so anything remembering "layer 2"
silently starts naming a different layer. `Flux_AnimatorControllerDef::AddLayer`
mints from a monotonic counter that is **itself serialized**, and a removed layer's
id is never handed out again. `AssignLayerId` (not the layer's own `SetLayerId`) is
what an export uses, because re-stating ids chosen elsewhere must also move the
counter past them. WU-6.3 gives the RUNTIME controller the same counter — see
*Stable layer IDs* below.

★ **`Flux_BoneMask` IS RESOLVED AND INDEX-BASED, SO IT CANNOT BE EXPORTED.** It
holds a flat weight array and no provenance — not the skeleton, not the file. That
is why the layer stores the mask PATH beside it; without it, `ExportControllerDef`
would write an empty reference for every masked layer and turn a save into a mask
deletion. A controller masked by hand exports UNMASKED and returns false saying so.

### Resolving a mask (WU-7.1) — one name→index walk, three things to know

★ **THERE USED TO BE TWO NAME→INDEX RESOLUTIONS AND THEY KEYED ON DIFFERENT
MAPS.** `Flux_BoneMask::SetFromBoneNames` took a **`Flux_MeshGeometry`** and
looked names up in `m_xBoneNameToIdAndOffset`, while the entire runtime pose path
— `Flux_SkeletonPose::SampleFromClip`'s live overload, `Flux_SkeletonInstance`,
the controller, every layer — resolves against
`Zenith_SkeletonAsset::m_xBoneNameToIndex`. A mask numbered by one and applied to
a pose numbered by the other is a mask on the wrong bones, with nothing to
observe but an animation that looks wrong. There is now a **skeleton-asset
overload**:

```cpp
bool Flux_BoneMask::SetFromBoneNames(const Zenith_SkeletonAsset& xSkeleton,
    const Zenith_Vector<std::string>& xBoneNames,
    const Zenith_Vector<float>& xWeights,
    Zenith_Vector<std::string>* pxOutUnresolvedNames = nullptr);
```

`Zenith_BoneMaskAsset::ResolveTo` is a **caller** of it plus the error reporting
(Flux cannot know which asset the names came from, and the PATH is most of what
makes the message useful). An empty `xWeights` means 1.0 per named bone, matching
the mesh-geometry overload; any other length mismatch is refused WHOLE, because a
weight silently attached to the wrong bone is worse than no mask. The resolve
always re-fills the array to `FLUX_MAX_BONES` first, so resolving into a mask that
came out of a stream produces a COMPLETE one.

★ **A NAME IS THE ONLY THING THAT TRAVELS BETWEEN TWO RIGS**, which is the whole
reason `.zanimmask` stores names. Two skeletons carrying the same bone names in a
different ORDER resolve one authored mask to different INDICES and to the same
per-bone meaning. Pinned by
`BoneMask.OneMaskResolvesByNameOnTwoRigsWithDifferentBoneOrder`, which builds two
in-memory skeletons with permuted bone order and compares per NAME.

★ **THE CONSTRUCTOR AND THE STREAM READER DISAGREE ABOUT THE LENGTH.**
`Flux_BoneMask()` fills `FLUX_MAX_BONES` zeroes; `ReadFromDataStream` sizes to
whatever count the stream carried, which may be FEWER. Every accessor is
bounds-checked against the **stored** count — `GetBoneWeight` past it answers 0,
which is also what `Flux_SkeletonPose::MaskedBlend` already substitutes for a
short mask, so the two agree. `GetWeightCount()` is the stored count and
`HasAnyNonZeroWeight()` is the derivation below.

★ **AN ALL-ZERO SCENE-INLINE MASK STILL READS AS "NO MASK", AND THAT IS FROZEN.**
`Flux_AnimationLayer::ReadFromDataStream` decides "this layer has a mask" by
scanning for any non-zero weight — so an explicitly all-zero mask, which is a
meaningful one ("this layer overrides nothing yet"), comes back UNMASKED, and an
OVERRIDE layer with no mask replaces the WHOLE skeleton. **`m_bHasAvatarMask` is
NOT on the wire and may not be added**: that payload is written by
`Flux_AnimationLayer::WriteToDataStream`, which `Flux_AnimationController` writes
inline and `Zenith_AnimatorComponent` writes into a `.zscen` — committed scene
files carry those bytes with no version word to branch on.

**The `.zanimmask` route is the one that carries the flag, and the runtime already
honours it**: `BuildFromControllerDef` calls `SetAvatarMask` only when
`Zenith_BoneMaskAsset::HasAvatarMask()` is true, so an all-zero mask from an asset
produces a MASKED layer and an asset with the flag off produces an unmasked one
whatever its weights are. Pinned end-to-end by
`BoneMaskAsset.AnAllZeroMaskFromAnAssetStillMasksTheLayerBuiltFromIt`, in both
directions.

★ **AN ADDITIVE LAYER IGNORES ITS MASK ENTIRELY.** The layer composition tests
`LAYER_BLEND_ADDITIVE` FIRST and goes to `Flux_SkeletonPose::AdditiveBlend`, whose
signature has no mask in it; only the OVERRIDE branch reaches `MaskedBlend`. Every
layer has an `m_xAvatarMask` field regardless, so the field list does not say so —
which is exactly how a UI ends up offering a control that does nothing. The
editor's one statement of the rule is
`Zenith_BoneMaskDocument::LayerAcceptsMask(blendMode)` (see `Editor/CLAUDE.md` →
*The Bone Mask sub-panel*).

### Animation Layers (Flux_AnimationLayer)
Multiple independent state machines composing poses:
- Each layer has its own `Flux_AnimationStateMachine`, weight, and blend mode
- **Override** (`LAYER_BLEND_OVERRIDE`): Replaces lower layers, optionally masked by `Flux_BoneMask`
- **Additive** (`LAYER_BLEND_ADDITIVE`): Adds on top of lower layers
- Layer 0 is the base; additional layers compose on top
- Managed by `Flux_AnimationController::AddLayer()`, `GetLayer()`, `SetLayerWeight()`
- `SetEmitEvents(bool)` silences **one layer's** animation events (see *Animation Event Delivery*). Not serialized

### Stable layer IDs — the handle gameplay holds (WU-6.3, D43/D44)

★ **DO NOT CACHE A `Flux_AnimationLayer*` ACROSS FRAMES. HOLD ITS ID.** This
REPLACES a guarantee this document used to give (see *Why heap-stable matters*):
`BuildFromControllerDef` and `ReadFromDataStream` each delete every layer the
controller owns and build a new list, so a pointer taken from `AddLayer` or
`GetLayer` is valid only until the next controller-asset load or scene
deserialize — and nothing on either path can tell a holder. The failure is a
use-after-free with no assert in front of it.

★ **THE TOP-LEVEL `Flux_AnimationStateMachine*` IS THE SAME HAZARD AND HAS NO ID
TO HOLD INSTEAD** — `CreateStateMachine`, `BuildStateMachineFromDef`,
`BuildFromControllerDef` and `ReadFromDataStream` each `delete` and replace it, so
the pointer `CreateStateMachine` returns is for configuring the machine you just
made and nothing else. Re-resolve per use with `HasStateMachine()` and then
`GetStateMachine()`, in that order: `GetStateMachine()` AUTO-CREATES an empty
`"Default"` machine, so an unguarded reach turns "there is no graph" into a silent
no-op on an empty one. (`RenderTest_TennisPlayerComponent::BeginStroke` is the
worked example; it cached the pointer and an editor undo freed it.)

| Verb | Answers |
|---|---|
| `AddLayer(name)` | a new layer, carrying a FRESHLY MINTED id. Use the returned pointer to configure it and drop it |
| `GetLayerById(u_int)` | the layer with that id, or **nullptr** — including for `uFLUX_INVALID_LAYER_ID`, which nothing is ever minted |
| `GetLayerByName(const std::string&)` | the FIRST layer with that name in blend order, or nullptr. Names are not unique; this is the one-time lookup that gets you an id |
| `GetLayer(uIndex)` | a POSITION IN THE BLEND ORDER — what composition needs, and what identity is not |
| `GetNextLayerId()` | the id the next `AddLayer` will mint |

The id is **unique within one controller and monotonic for its whole lifetime**:

- **Every creation path mints**, not just `BuildFromControllerDef` — `AddLayer`
  (which is how every game in the tree authors) and each layer
  `ReadFromDataStream` rebuilds. Before this, the runtime field existed but only
  the def-build path ever wrote it, so an imperatively-built controller's layers
  all carried the same value and nothing could tell them apart.
- **`BuildFromControllerDef` ADOPTS the def's id** and moves the counter past it
  (`AdoptLayerId`, the mirror of `Flux_AnimatorControllerDef::AssignLayerId`, and
  for the same reason: a bare `SetLayerId` would leave the counter behind the ids
  now in the list and the next `AddLayer` would mint a duplicate). So an id
  survives a `.zanimctrl` round trip, and a rebuild that REORDERS layers moves the
  index while the id stays put.
- **The counter is never rewound** — not by a rebuild that destroys every layer,
  not by a deserialize. That is what makes a stale id resolve to nullptr instead of
  to a different layer that inherited its number, which is the index bug with extra
  steps.
- **A scene-restored layer's id is FRESH, not the one the save was taken with.**
  The id is deliberately absent from `Flux_AnimationLayer::WriteToDataStream`
  (committed `.zscen` files carry that layout with no version word), so the ids are
  stable within a run and nothing may persist one. Resolve by NAME once after
  building or loading a graph, then hold the id.

Worked consumer: `Games/RenderTest/Components/RenderTest_PlayerComponent.h` stores
`m_uBaseLayerId` / `m_uAimLayerId` (sentinel `uFLUX_INVALID_LAYER_ID`) and resolves
through `ResolveBaseLayer()` / `ResolveAimLayer()` at each use — the null guards its
call sites already needed for the pre-`OnStart` case cover a vanished layer too.
Pinned by the four `WU6_3_*` units in `Flux_AnimationController.Tests.inl` and by
`Animator.LayerIdAddressesTheSameLayerAcrossACrossSceneMove` in
`Core/Zenith_UnitTests.Tests.inl`.

### Hot reload — a changed def onto a LIVE controller (WU-6.4, D45)

★ **`BuildFromControllerDef` IS A DEMOLITION, AND THAT IS CORRECT FOR A LOAD.**
It deletes every layer, replaces or drops the top-level machine, and hands each
machine to `BuildFromDef` — which calls `ResetRuntime` (no current state, no
transition in flight) and then `CopyFrom` (every blend-tree playhead back at
zero). Run it against an edited `.zanimctrl` while the game is playing and the
character snaps to every graph's default state at frame 0, every layer weight
reverts to whatever the file says, and every live parameter value is replaced by
its declared default. **Nothing asserts**, because that is precisely what a cold
load is supposed to do — so the failure looks like "the editor made the character
twitch" rather than like a missing feature.

| Verb | Does |
|---|---|
| `Flux_AnimationController::ReloadFromControllerDef(def, skeletonForMasks)` | the whole-controller reload — what an editor Save/apply calls |
| `Flux_AnimationStateMachine::ReloadFromDef(def, clips)` | one machine, for a caller that holds one |
| `Flux_AnimationStateMachine::CaptureRuntimeSnapshot()` / `RestoreRuntimeSnapshot(s)` | the two primitives both of the above are built from |
| `Flux_AnimationStateMachine::RestoreMatchedParameterValues(prev, live)` | D45's parameter rule, as a pure function over two sets |

**What survives, and on what KEY.** The key is the whole design; each one is the
only identity that spans the two defs:

| thing | survives on | otherwise |
|---|---|---|
| parameter VALUE | **name AND type** | the new declaration's DEFAULT |
| current state | **name** | the layer's/machine's new DEFAULT state |
| normalized time | **its STATE surviving** | 0 |
| an active transition | **CANCELLED onto its TARGET** | the default state, at time 0 |
| layer weight | **layer ID** (WU-6.3) | a vanished id is dropped; a new id keeps the def's weight |

- **The snapshot holds NAMES AND FRACTIONS, never pointers.** Every runtime
  pointer on a machine (`m_pxCurrentState`, `m_pxTransitionTargetState`) names a
  `Flux_AnimationState` that `CopyFrom` is about to delete, so a snapshot
  carrying them would be a snapshot of freed memory by the time it was used. It
  is also why *"a renamed state does not survive"* needs no rule of its own.
- ★ **NAME *AND* TYPE FOR A PARAMETER, because the value is in a UNION.**
  `Flux_AnimationParameters::Parameter` overlays `float`/`int32_t`/`bool`, so
  matching on name alone would hand a float `"Speed"` of 4.25 to an int
  `"Speed"` as `1082130432`. A retype is an edit; an edit gets the new default.
- ★ **THE LIVE PARAMETER SET IS EMPTIED FIRST, NOT RE-SEEDED OVER.** `SeedInto`
  **never overwrites** (D42) — right for publishing, wrong for reloading: seeding
  the new declarations onto the old set leaves a parameter the edit RENAMED in
  the live set forever, under a name no condition reads and nothing can remove.
  The reload therefore resets `m_xParameters`, lets the rebuild seed the new
  declarations at their new defaults, and only then restores what matched.
- ★ **A PENDING TRIGGER SURVIVES.** A trigger is an input the game has already
  delivered and not yet spent; dropping it across a reload swallows a jump. A
  trigger that was not pending is never un-set.
- ★ **AN ACTIVE TRANSITION IS CANCELLED ONTO ITS TARGET, NOT RESUMED.** Resuming
  would need the source POSE SNAPSHOT the `Flux_CrossFadeTransition` is holding,
  over a state set the edit may have changed; landing back on the SOURCE would
  re-run the transition's conditions from scratch and re-spend a one-shot the
  player has already paid for. The target's **own** normalized time comes across
  — `UpdateTransition` evaluates the target every frame of the fade (the source
  contributes a frozen snapshot and does not advance), so it is a real playhead.
- ★ **A NORMALIZED TIME DIES WITH ITS STATE.** It is a fraction of a *particular*
  clip; carrying 0.8 onto whatever the new default happens to be drops the
  character into the middle of an unrelated animation — a glitch nobody can trace
  back to the edit that caused it.
- ★ **THE INVERSE OF `GetNormalizedTime` IS A VIRTUAL, `Flux_BlendTreeNode::
  SetNormalizedTime`, WITH PER-NODE SEMANTICS.** It did not always exist — this
  used to read *"there is no `SetNormalizedTime` anywhere in the system"*, and the
  restore was a private static on the state machine
  (`ApplyNormalizedTimeToTree`) dispatching on `GetNodeTypeName()` with `strcmp`
  over the seven node types. `Flux_AnimationStateMachineDef` carried a second,
  structurally identical `strcmp` walk for clip resolution; both are gone, and
  clip resolution is the matching virtual `ResolveClips`. The bottom of the walk
  is unchanged: a normalized time becomes **SECONDS** at the leaf, through
  `Flux_BlendTreeNode_Clip::SetCurrentTimestamp` — still the only place a
  playhead is written, and still a **D40 scrub** (the previous-timestamp mark
  follows and any pending span is dropped, so a restore does not fire every event
  the playhead was dropped past).

  Every leaf is put at the **same** normalized time, which is the only reading
  that inverts `GetNormalizedTime` across the **six** composite shapes at once:

  | Node | `GetNormalizedTime` reads | `SetNormalizedTime` writes |
  |---|---|---|
  | `Clip` | its own seconds ÷ duration | `SetCurrentTimestamp(f × duration)`, 0 if unresolved or zero-length |
  | `Blend` | `mix(A, B, weight)` | **both** children |
  | `BlendSpace1D` | the NEAREST point's | **every** point's node |
  | `BlendSpace2D` | the NEAREST point's | **every** point's node |
  | `Additive` | the base's only | base **and** additive |
  | `Masked` | the base's only | base **and** override |
  | `Select` | the SELECTED child's | **every** child |

  ★ **The write side mirrors `Reset()`, not the read side, and the three
  asymmetries are deliberate.** An unselected `Select` branch and an `Additive`
  node's additive branch are exactly the ones nothing else moves, and either
  becomes the pose the moment an index or a weight changes; leaving them where a
  `Reset` left them puts the layers a playthrough apart. ★ **Nor is the inversion
  exact for a half-populated `Blend`** — `GetNormalizedTime` substitutes 0 for a
  null child, so `mix(0, T, w)` reads back below `T`.

  The base declarations are **non-pure, defaulting to a no-op**: a node type
  added without an override keeps the zero its `Reset` left rather than being
  handed a wrong time, which is the same behaviour the old case list had by
  omission — now stated once, in the class it belongs to, instead of being a
  property of two separate `strcmp` lists staying in step.
- ★ **CLIPS ARE RE-ACQUIRED BEFORE THE OLD REFERENCES ARE DROPPED.** The clip
  collection holds BORROWED pointers pinned by `m_xAnimationAssets`, and a def
  that no longer names a clip must give that reference back — but releasing first
  would take a still-wanted asset's refcount through zero between the two calls,
  which is the window `UnloadUnused` sweeps. The old handles are COPIED onto the
  stack (a handle copy AddRefs) and released after the rebuild. Emptying the
  collection is also the only thing that releases a dropped clip at all:
  `BuildFromControllerDef` only ever ADDS, so a reload without it would
  accumulate every clip every revision of the def ever mentioned.
- ★ **THE EDITOR'S DIRECT-PLAY PREVIEW IS DROPPED (`Stop()`).**
  `m_pxDirectPlayNode` holds a raw `Flux_AnimationClip*` out of the collection
  the reload empties, and the node is in no def, so nothing can re-resolve it.
  Re-arming is the panel's business — the panel is the only thing that can have
  armed it.

**Two things it does NOT preserve, stated rather than left to be discovered:**

- **A sub-state machine's own current state.** Entering a container state
  re-enters its child at the child's default (`SetState`), and the child INSTANCE
  lives inside the def `CopyFrom` just replaced, so there is no object to restore
  onto. See the WU-6.1 note about the nested-instance residual.
- **A transition's elapsed time, its source pose and its interruptibility.**
  Cancellation is the rule, not a limitation — see above.

★ **NOT A SYNCHRONISATION POINT, AND IT DOES NOT CREATE ONE** — the same ruling
`Flux_AnimationClip::ReplaceContentsFrom` carries (D27). Capture, rebuild and
restore are three plain non-atomic writes over live data; **the caller guarantees
no animation update is in flight.** The editor calls it from the main thread,
between frames.

★ **THE TWO RETURN VALUES MEAN DIFFERENT THINGS, DELIBERATELY.**
`ReloadFromControllerDef` returns the **BUILD** contract — false when a clip or a
bone mask the def NAMES did not resolve, a defect the caller must not ignore —
and says nothing about survival, because a state the edit deleted is the author's
intent rather than a failure. `ReloadFromDef` has no dangling-reference failure
mode to report (`BuildFromDef` returns `void`), so its bool reports the one thing
a caller there can act on: **true** when the machine landed on the state the
snapshot named, **false** when it fell back to the new default.

Pinned by five `WU64_*` units in `Flux_AnimationStateMachine.Tests.inl` (the
state / time / transition rows, each with its negative, plus a standalone
machine's own parameter table) and five in `Flux_AnimationController.Tests.inl`
(parameter kept by name+type, renamed, retyped; layer weights by id across a
reorder, and a deleted-plus-added layer pair). Every one of them **drives the
graph before reloading** — a test that reloaded a machine which had never ticked
passes against the demolition this work replaces.

### Update Modes (Flux_AnimationUpdateMode)
- `ANIMATION_UPDATE_NORMAL` - Uses scaled deltaTime
- `ANIMATION_UPDATE_FIXED` - For physics-synced animation
- `ANIMATION_UPDATE_UNSCALED` - For UI/pause menu animations

### Callbacks
State lifecycle hooks use function pointers + void* userdata (NOT std::function):
```cpp
using Flux_AnimStateCallback = void(*)(void* pUserData);
using Flux_AnimStateUpdateCallback = void(*)(void* pUserData, float fDt);
```

## Animation Event Delivery

★ **EVENTS USED TO FIRE ON EXACTLY ONE PATH, AND IT WAS THE EDITOR'S.**
`Flux_AnimationController::ProcessEvents` had a single call site, inside
`#ifdef ZENITH_TOOLS` and gated on the tools-only direct-play node — and inside the
function the clip was ALSO only ever sourced from that node. In a shipping build the
whole mechanism returned immediately; even in a tools build, nothing a state machine or
a layer played could fire an event. A game that authored footsteps into a `.zanim` and
hooked `SetEventCallback` got silence, with no diagnostic anywhere. **`ProcessEvents` is
gone.** Delivery now runs on the state-machine and layer paths in EVERY build, and
direct play is one more source into the same dispatcher rather than the only one.

**The unit of report is a leaf, not a controller.** ★ ONE CONTROLLER-LEVEL TIME CANNOT
SEE A BLEND TREE'S LEAVES: a 1D blend space between a 0.9 s walk and a 1.4 s run has two
playheads at two normalized times advancing at two rates, so "which events did the
playhead cross this frame" has no single answer above the leaf. Each evaluated leaf
reports its own crossing as a `Flux_ClipEventSpan` (`Flux_BlendTree.h`):

| field | meaning |
|---|---|
| `m_pxClip` | the clip whose events are being scanned |
| `m_fPrevNormalizedTime` / `m_fCurrNormalizedTime` | the step, as `[0,1]` fractions of the clip — matching `Flux_AnimationEvent::m_fNormalizedTime` |
| `m_fWeight` | the blend weight this leaf carried in its layer for this evaluate |
| `m_bForward` | the SIGN OF THE STEP, kept explicitly because the times alone cannot tell a reverse step from a wrap |
| `m_bWrapped` | the step crossed the loop point. Set from the RAW advanced time (`prev + dt*rate >= duration`), **not** from `curr < prev` — a step longer than the clip lands back ABOVE prev and would otherwise read as no wrap |
| `m_bLooping` / `m_bReachedEnd` | a non-looping clip hit its duration on this step, so the top end closes |

**The collection walk.** `CollectEventSpans(Zenith_Vector<Flux_ClipEventSpan>*)` runs
leaf → blend tree → state machine → layer → controller, once per `Update`, AFTER the
pose has been evaluated:

- `Flux_BlendTreeNode::CollectEventSpans` is virtual and ★ **WALKS EVERY CHILD, EVEN THE
  ONES THIS FRAME DID NOT EVALUATE** — because a leaf holds its pending span until
  something collects it, and **collecting is what clears it**. A blend space whose
  parameter moved off a point, or a `Select` whose index changed, would otherwise keep
  handing out the span from the last frame that DID evaluate it, once per frame, forever.
- `pxOutSpans` **may be NULL**, and that is a real mode rather than a defensive check:
  "walk and clear, discard the result".
- Each leaf's weight comes from `SetEvalWeight`, set by a composite on each child
  immediately before calling that child's `Evaluate`, so by the leaf it is the product of
  every fraction down the path. It is deliberately not recomputed by a second walk — that
  would be a guard comparing a value against a re-computation of itself.
- `Flux_AnimationController::DispatchClipEvents` mirrors `UpdateWithSkeletonInstance`
  exactly — layers, then the editor's direct-play preview, then the state machine —
  because anything else would dispatch events from a path that did not produce this
  frame's pose. It is **not** tools-gated, and it runs every frame whether or not a
  callback is installed (collecting is what clears).

**Who emits, when more than one clip is crossing an event at once:**

- **Within one layer (D35)** — the leaf with the HIGHEST blend weight; ties go to the
  LOWEST leaf index, which is collection order (depth-first, children in declaration
  order). The comparison is strictly greater so the first of an equal pair keeps the win,
  and the running best starts at zero so a **zero-weight leaf can never emit**.
- **Across layers (D36)** — every layer arbitrates and emits **independently**, and that
  is the correct default rather than a simplification: a masked upper/lower split is two
  animations on one skeleton, and the legs' footsteps and the arms' swing beats are both
  real. `Flux_AnimationLayer::SetEmitEvents(false)` silences one layer; a silenced layer
  is still WALKED with a null sink, so re-enabling it cannot fire a span the silenced
  frames left pending. It is deliberately NOT derived from the layer's WEIGHT — a weight
  is animated, so tying events to it would make a footstep fire or not depending on where
  in a fade the frame landed. **A controller with no layers is one layer** for this.
- **Across a crossfade (D37)** — the side at weight **>= 0.5** emits, a dead-even 0.5
  going to the **TARGET**; the other side is still walked with a null sink so its span is
  cleared rather than saved up to fire the moment the weights cross. A self-transition has
  one state on both sides and one set of leaves, so the silent side is only cleared when
  it is a *different* state. ★ **TODAY THE OUTGOING SIDE HAS NOTHING TO GIVE**:
  `UpdateTransition` evaluates only the TARGET state and the source contributes a pose
  SNAPSHOT frozen at `StartTransition`, so its blend tree does not advance and produces no
  span at all. The `>= 0.5` branch is written both ways round anyway, because the rule is
  about which side MAY emit and the day the source starts advancing is not the day to
  rediscover that.

**Which events, over one step (D38/D39/D40).** `SpanContainsEventTime(span, t)` is the
PUBLIC, pure predicate — the tests pin it directly:

- `!m_bForward` → **false, always**. Reverse emits nothing (D39) and is an early return,
  not an assert: `SetPlaybackSpeed` accepts negatives and shipping content uses them. The
  leaf's previous time still MOVES, so the next forward frame scans from where the
  playhead actually is instead of replaying the skipped span as a burst.
- `m_bWrapped` → `[prev, 1) U [0, curr)`, as one OR, so a step longer than the clip fires
  each event ONCE rather than twice.
- `m_bReachedEnd` → `[prev, curr]`, the one CLOSED top end. A non-looping clip stops AT
  1.0 and never steps past it, so a half-open span could never contain an event authored
  there — and it fires exactly once.
- otherwise → `[prev, curr)`.
- ★ **AN EVENT AT NORMALIZED 1.0 ON A LOOPING CLIP IS AN EVENT AT 0.0 OF THE NEXT LOOP**,
  folded with `fmod` before any of the above (so a time authored past 1.0 folds too).
  Without the fold, a clip with a beat on its last frame fires it either never (half-open
  at the top) or twice.
- **A seek emits nothing unless asked (D40)** but still MOVES the bookkeeping mark.
  Leaving the mark behind would make the next forward tick process the whole span from the
  old mark and fire a burst the playhead skipped. `SetEmitEventsOnSeek(true)` opts in; a
  BACKWARD scrub still emits nothing even then, same rule as reverse playback.
  `Flux_BlendTreeNode_Clip::SetCurrentTimestamp` follows the same rule at leaf level — the
  previous timestamp follows the new one and any pending span is dropped.

`m_fLastEventCheckTime` (read via `GetLastEventCheckTime()`) is the **direct-play mark
only**: the state-machine and layer paths have no single controller-level playhead to
mark — a blend tree's leaves each run their own clock — so their bookkeeping lives per
leaf in `Flux_BlendTreeNode_Clip::GetPreviousTimestamp()`. None of the per-frame event
bookkeeping is serialized (D41: this changed no schema).

## File Structure

```
MeshAnimation/
  Flux_AnimationClip.h/cpp           - Animation keyframe storage
  Flux_AnimationController.h/cpp     - Playback control (owns clips, state machine, IK, layers)
  Flux_AnimationControllerStore.h/cpp- Heap-stable owning store of per-entity controllers (g_xEngine.AnimationControllers()); ECS entry point Zenith_AnimatorComponent forwards into it
  Flux_AnimationStateMachineDef.h/cpp- The AUTHORED half (WU-6.1): Flux_AnimationParameters,
                                       Flux_TransitionCondition, Flux_StateTransition,
                                       Flux_AnimationState and Flux_AnimationStateMachineDef
                                       (states / transitions / any-state / default state /
                                       parameter DECLARATIONS + defaults / nested defs),
                                       its serializer and its by-name clip resolution
  Flux_AnimationStateMachine.h/cpp   - The INSTANCE half: current state, active transition,
                                       shared-parameter pointer, the two poses; owns a def
                                       by value and forwards the imperative builders into it
  Flux_AnimatorControllerDef.h/cpp   - WU-6.2: the .zanimctrl payload — clip paths, an
                                       optional embedded top-level SM def, the layer list
                                       (id / name / weight / blend mode / emit flag / bone-mask
                                       asset path / embedded SM def) and the shared envelope
                                       (type id 7, schema 1)
  Flux_AnimationLayer.h/cpp          - Animation layer (weight, blend mode, avatar mask;
                                       plus WU-6.2's NON-serialized layer id + mask asset path)
  Flux_SkeletonInstance.h/cpp        - Runtime skeleton state
  Flux_BonePose.h/cpp                - Bone transform utilities (Blend, MaskedBlend, AdditiveBlend)
                                       and Flux_BoneMask (the RESOLVED, index-based weight array)
  Flux_BonePose.Tests.inl            - Unit tests for Flux_BoneMask (WU-7.1): one mask resolved by
                                       NAME onto two rigs with permuted bone order, the implicit
                                       full-weight list, the whole-refusal on a name/weight length
                                       mismatch, an unresolvable name being LISTED, the stored-count
                                       bound on every accessor, and D47's "an all-zero mask cannot
                                       be told from no mask by its weights"; plus WU-8.1's two
                                       SkeletonPose units - a zero-tangent clip poses exactly where
                                       it always did, and an authored tangent reaches a bone's LOCAL
                                       POSE through an unchanged SampleFromClip
  Flux_BlendTree.h/cpp               - Animation blending (Clip, 1D, 2D, Masked nodes)
  Flux_InverseKinematics.h/cpp       - IK solving (FABRIK)
  Flux_AnimationClip.Tests.inl       - Unit tests for clip storage/sampling, in six categories:
                                       Animation (root motion + end-of-clip clamping),
                                       AnimationSerialization (the envelope, the new metadata
                                       fields, the tangent block, bone-name write order),
                                       AnimationTime (key times are SECONDS),
                                       AnimationMutation (D9..D16), AnimationReload (D26/D28),
                                       AnimationTangents (WU-8.1: the zero-tangent no-regression
                                       property against a longhand lerp/slerp oracle, the
                                       hand-computed Hermite term, "an unset tangent is the
                                       SEGMENT SLOPE not a flat one", measured C1 across a key
                                       with a mismatched control, and the two Compute*Tangents
                                       presets)
  Flux_AnimationController.Tests.inl - Unit tests for event DELIVERY (WU-5A / D34..D40):
                                       arbitration, layers, crossfade sides, loop and
                                       non-looping boundaries, reverse, seek, and the pure
                                       SpanContainsEventTime rules; plus WU-6.3's four
                                       WU6_3_* stable-layer-id units (minting + monotonicity,
                                       an id surviving a def rebuild that REORDERS layers,
                                       GetLayerByName hit/miss/first-wins, and ids minted
                                       for layers restored from a stream); plus the five
                                       WU64_* reload units (D26/D28) —
                                       ReloadKeepsAParameterMatchedByNameAndType,
                                       ReloadGivesARenamedParameterTheNewDefault,
                                       ReloadGivesARetypedParameterTheNewDefault,
                                       ReloadKeepsLayerWeightsByIdAcrossAReorder,
                                       ReloadDropsARemovedLayerAndGivesANewOneTheDefsWeight
  Flux_BlendTree.Tests.inl           - Unit tests for blend tree nodes (incl. span collection)
  Flux_AnimatorControllerDef.Tests.inl - Unit tests for WU-6.2's def: the envelope round trip
                                       (top-level machine AND layers), the three refusal modes,
                                       stable/never-reused layer ids, and CopyFrom's deep copy
  Flux_AnimationStateMachine.Tests.inl - Unit tests for WU-6.1: def-built vs imperatively-built
                                       machines posing identically over 60 ticks, the D48 bound
                                       and UNBOUND blend spaces, D42's controller parameter
                                       reaching a sub-machine condition AND a layer's machine
                                       (the Zenithmon no-op), and the def's stream round trip;
                                       plus the five WU64_* reload units (D26/D28) —
                                       ReloadKeepsTheCurrentStateAndItsNormalizedTime,
                                       ReloadDropsADeletedCurrentStateToTheNewDefaultAtTimeZero,
                                       ReloadCancelsAnInFlightTransitionOntoItsTarget,
                                       ReloadFallsToTheDefaultWhenTheTransitionTargetIsGone,
                                       ReloadKeepsAStandaloneMachinesParameterValuesByNameAndType
```

## Constants

- `MAX_BONES = 100` - Maximum bones per skeleton (`Zenith_SkeletonAsset::MAX_BONES`; the bone-palette SSBO reserves a MAX_BONES block per skeleton)
- `BONES_PER_VERTEX_LIMIT = 4` - Maximum bone influences per vertex (defined in AssetHandling/Zenith_MeshAsset.h)
