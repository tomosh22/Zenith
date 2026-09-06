# Animation Pose Authoring — WU-4.0 design spike

**Status:** design only. No code was written for this note.
**Scope:** the six questions Phase 4 (pose authoring) splits across WU-4.1 … WU-4.4.
**Audience:** the four implementer briefs, and whoever reviews their diffs.

Every path below is relative to the repo root `C:\dev\Zenith`. **Engine source lives
under `Zenith/`** — `Zenith/Flux/Gizmos/`, `Zenith/Editor/`, `Zenith/Maths/`. (The
WU-4.0 brief said there is no `Zenith/` prefix; there is. `C:\dev\Zenith\Flux` does
not exist.)

---

## 0. Premises checked against the tree

| Premise | Verdict | Evidence |
|---|---|---|
| Nothing existing can address a bone | **TRUE** | `Flux_GizmosImpl::SetTargetEntity(Zenith_Entity*)` — `Zenith/Flux/Gizmos/Flux_GizmosImpl.h:128`, `Zenith/Flux/Gizmos/Flux_Gizmos.cpp:374`. The target member is `Zenith_Entity* m_pxTargetEntity` (`Flux_GizmosImpl.h:206`) and all eight interaction sites resolve it through `GetGizmoTargetWithTransform()` (`Flux_Gizmos.cpp:130`, called at 237, 413, 749, 800, 837, 881). `Zenith_SelectionSystem::RaycastSelect` returns a `Zenith_EntityID` (`Zenith/Editor/Zenith_SelectionSystem.h:56`), caches by `Zenith_EntityID` (`:73`) and tests through `Zenith_ModelComponent*` (`:65`). |
| Authoring writes bone-local rotations | **TRUE** | `Flux_SkeletonPose::SampleFromClip` writes `m_axLocalPoses[i].m_xRotation` (`Zenith/Flux/MeshAnimation/Flux_BonePose.cpp:257`); the controller copies those into the instance with `SetBoneLocalTransform` (`Flux_AnimationController.cpp:346`). |
| `kuFluxViewSlotPreview == 5` | **TRUE** | `1u + kuFluxViewNumShadowSlots`, `kuFluxViewNumShadowSlots = 4u` — `Zenith/Flux/RenderViews/Flux_RenderViews.h:45,48`. |
| The controller invokes `Solve` inside `ApplyOutputPoseToSkeleton`, and the header warns game code off | **TRUE** | `Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:334-340`; warning at `Flux_InverseKinematics.h:170-176`. |
| Phase-3 gives key times in **SECONDS** | **CONTRADICTED by the tree.** See §5.2. | `Flux_BoneChannel::AddPositionKeyframe(float fTimeTicks, …)` (`Flux_AnimationClip.h:89-91`) and `SampleFromClip` multiplies seconds by ticks-per-second before sampling (`Flux_BonePose.cpp:168`, `:201`). Channel storage is **ticks**. |
| `Zenith_AnimationDocument`, `Zenith_AnimationPreviewSession`, `Zenith_EditorPanel_Animation`, `Flux_BoneChannel::InsertKeyframeAt` / `RemoveKeyframe` / `SetKeyframeTime` / `SetKeyframeValue`, `m_uAuthoredFrameRate`, `fANIM_TIME_EPSILON` | **NOT PRESENT anywhere in the checkout** (searched `Zenith/**` and all `.claude/worktrees/**`). Every signature below that touches them is written against the WU-4.0 brief's description, not against code I could read. | — |

Two further findings that change what Phase 4 can build, detailed in §7:

- **The Gizmos pass cannot draw into the preview view.** `Flux_GizmosImpl::SetupRenderGraph`
  declares one pass writing `GetFinalRenderTarget()` with no `.View(slot)`
  (`Flux_Gizmos.cpp:369-371`). `Flux_PrimitivesImpl` has the same limitation — it writes the
  main G-buffer MRTs (`Zenith/Flux/Primitives/Flux_Primitives.cpp:571-575`).
- **The preview slot already has an owner.** `Flux_MaterialPreviewController` activates and
  stages slot `kuFluxViewSlotPreview` from its own liveness window
  (`Zenith/Flux/RenderViews/Flux_MaterialPreviewController.h:144,156-161`). Two panels open
  at once contend for one slot.

---

## 1. The selectable-bone target

### Decision

**A parallel, single-bone selection lives on `Zenith_AnimationPreviewSession`.**
`Zenith_SelectionSystem` is not touched. `Zenith_EditorSelectionState` is not touched.

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPreviewSession.h   (ADDITIONS to the Phase-3 class)

inline constexpr u_int kuINVALID_BONE_SELECTION = 0xFFFFFFFFu;

class Zenith_AnimationPreviewSession
{
public:
	// ---- bone selection (WU-4.1) --------------------------------------------
	void   SelectBone(u_int uBoneIndex);          // out-of-range clears the selection
	void   ClearBoneSelection() { m_uSelectedBoneIndex = kuINVALID_BONE_SELECTION; }
	u_int  GetSelectedBoneIndex() const { return m_uSelectedBoneIndex; }
	bool   HasBoneSelection()     const { return m_uSelectedBoneIndex != kuINVALID_BONE_SELECTION; }

	void   SetHoveredBoneIndex(u_int uBoneIndex) { m_uHoveredBoneIndex = uBoneIndex; }
	u_int  GetHoveredBoneIndex() const { return m_uHoveredBoneIndex; }

	// The session's model matrix — model space -> world space for everything in §3.
	// Phase 4 keeps this IDENTITY (see §3.1); the accessor exists so the helpers can
	// be tested with a non-identity value.
	const Zenith_Maths::Matrix4& GetSessionModelMatrix() const { return m_xSessionModelMatrix; }
	void  SetSessionModelMatrix(const Zenith_Maths::Matrix4& xModel);

	// Recomputes m_axModelSpaceTransforms on the owned instance. MUST be called after
	// any pose write and before any §2 / §3 helper reads a model matrix (see §3.2).
	void   RefreshDerivedPose();

	Flux_SkeletonInstance*       GetSkeletonInstance()       { return m_pxSkeletonInstance; }
	const Flux_SkeletonInstance* GetSkeletonInstance() const { return m_pxSkeletonInstance; }

private:
	u_int m_uSelectedBoneIndex = kuINVALID_BONE_SELECTION;
	u_int m_uHoveredBoneIndex  = kuINVALID_BONE_SELECTION;
	Zenith_Maths::Matrix4 m_xSessionModelMatrix = Zenith_Maths::Matrix4(1.0f);
};
```

### Rejected: a variant in `Zenith_SelectionSystem`

Widening it costs four unrelated edits and buys nothing. `RaycastSelect` returns
`Zenith_EntityID` (`Zenith_SelectionSystem.h:56`); the AABB cache is
`Zenith_HashMap<Zenith_EntityID, BoundingBox>` (`:73`); the precise phase takes a
`Zenith_ModelComponent*` (`:65`); and the editor's selection state is an
`unordered_set<Zenith_EntityID>` plus a primary id (`Zenith/Editor/Zenith_EditorState.h:27-28`).
A bone index is not an entity, has no generation counter, is meaningless without the
skeleton it indexes, and must survive a scene load that clears entity selection
outright (`Editor/CLAUDE.md`, *Selection cleared when entity deleted or scene loaded*).

### Rejected: selection on the document rather than the session

The bone index indexes the **skeleton**, and the skeleton instance is owned by the
session, not the document. If a document is ever re-bound to a different skeleton the
selection must reset — that is session lifetime. Keeping it off the document also
keeps it out of the undo commands (§4): bone selection is deliberately **not
undoable**, matching entity selection, which is also not.

### Multi-bone selection is out of scope

Phase 4 selects exactly one bone. Multi-bone posing needs a defined multi-bone
rotation pivot and a per-bone delta decomposition, and would fork both §3 and §4. A
`Zenith_Vector<u_int>` can replace the scalar later without moving any other decision.

---

## 2. Bone hit geometry

### Decision

Pick against **one capsule per non-root bone (parent joint → this bone's joint), plus a
joint sphere only where a capsule cannot serve** — i.e. for leaf bones and for bones
whose capsule is degenerate. That combination has no overlap ambiguity: every point in
space is inside at most one shape belonging to any given bone, and a leaf tip is still
pickable.

**★ The capsule from `parent(i)` to `i` belongs to bone `i`, not to the parent.** This is
the single thing four subagents are most likely to get opposite ways round, so it is
stated as a rule: bone `i`'s local rotation `q_i` is applied at the parent joint and
moves the joint at `M_i` (`Flux_SkeletonInstance.cpp:317`), so the drawn segment between
those two joints is exactly the geometry `q_i` moves. Dragging the forearm therefore
edits the forearm bone, whose local rotation is the elbow angle.

### Types and entry points

```cpp
// Zenith/Editor/Animation/Zenith_BonePickGeometry.h            (NEW — WU-4.1)

struct Zenith_BonePickShape
{
	u_int                 m_uBoneIndex   = 0u;
	Zenith_Maths::Vector3 m_xA           = Zenith_Maths::Vector3(0.0f);  // parent joint (== m_xB when joint-only)
	Zenith_Maths::Vector3 m_xB           = Zenith_Maths::Vector3(0.0f);  // this bone's joint
	float                 m_fRadius      = 0.0f;
	bool                  m_bIsJointOnly = false;
};

struct Zenith_BonePickSet
{
	Zenith_Vector<Zenith_BonePickShape> m_xShapes;
	float m_fSkeletonExtent = 0.0f;   // model-space AABB diagonal over all joints
};

// Rebuilds xOut from the instance's CURRENT model-space transforms, pre-multiplied by
// xSessionModel so every shape is in WORLD space and the picking ray needs no transform.
// PRECONDITION: xSkeleton.ComputeSkinningMatrices() has run since the last pose write.
void Zenith_BuildBonePickSet(const Flux_SkeletonInstance& xSkeleton,
	const Zenith_Maths::Matrix4& xSessionModel,
	Zenith_BonePickSet& xOut);

// Nearest positive-t hit. Returns false and leaves both outputs untouched on a miss.
bool Zenith_RaycastBonePickSet(const Zenith_BonePickSet& xSet,
	const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir,
	u_int& uOutBoneIndex,
	float& fOutDistance);
```

### How it is built from the skeleton

Exact calls, in order:

1. `const u_int uNumBones = xSkeleton.GetNumBones();` — `Flux_SkeletonInstance.h:108`.
2. `const Zenith_SkeletonAsset* pxAsset = xSkeleton.GetSourceSkeleton();` — `Flux_SkeletonInstance.h:103`. Bail (empty set) if null.
3. Joint position for bone `i`:
   `Zenith_Maths::Vector3(xSessionModel * xSkeleton.GetBoneModelTransform(i)[3])`
   — `GetBoneModelTransform` is `Flux_SkeletonInstance.h:119`, returning the cache filled
   by `ComputeSkinningMatrices` at `Flux_SkeletonInstance.cpp:317`.
4. Parent index: `pxAsset->GetBone(i).m_iParentIndex`, sentinel
   `Zenith_SkeletonAsset::INVALID_BONE_INDEX` (`Zenith/AssetHandling/Zenith_SkeletonAsset.h:31,40,99`).
5. Child counts: **one pre-pass** incrementing a `Zenith_Vector<u_int>` sized `uNumBones`
   from each bone's parent index. **Do not call `Zenith_SkeletonAsset::GetChildBones`
   per bone** (`Zenith_SkeletonAsset.h:123`) — it returns a fresh `Zenith_Vector` by value,
   i.e. one allocation per bone per rebuild, on a per-frame path.
6. Emit a capsule for every `i` with a valid parent and `length(B - A) >= kfBONE_PICK_MIN_SEGMENT`.
7. Emit a joint sphere at `B` for every `i` whose child count is 0, and for every `i`
   whose capsule was suppressed at step 6.

Radii, all derived from `m_fSkeletonExtent` so the same code works on a 1.8 m humanoid
and a 0.2 m prop rig:

```cpp
inline constexpr float kfBONE_PICK_RADIUS_FRACTION   = 0.12f;   // of the segment length
inline constexpr float kfBONE_PICK_MIN_RADIUS_EXTENT = 0.004f;  // of the skeleton extent
inline constexpr float kfBONE_PICK_JOINT_EXTENT      = 0.020f;  // of the skeleton extent
inline constexpr float kfBONE_PICK_MIN_SEGMENT       = 1.0e-4f;
```

`fRadius = max(kfBONE_PICK_RADIUS_FRACTION * fSegmentLength, kfBONE_PICK_MIN_RADIUS_EXTENT * m_fSkeletonExtent)`;
joint radius = `kfBONE_PICK_JOINT_EXTENT * m_fSkeletonExtent`.

### The ray test

Every helper in `Zenith/Maths/Zenith_Maths_Intersections.h` is **origin-anchored** — the
shape sits at the origin and the caller pre-translates the ray. `RayIntersectsCylinder`
(`:63`) takes `(rayOrigin, rayDir, axis, radius, length, outDistance)` with the cylinder
running from the origin along `axis`. So, per capsule:

```
n   = normalize(B - A);  len = length(B - A)
hit = RayIntersectsCylinder(xRayOrigin - A, xRayDir, n, r, len, t)
```

and, when that misses, two sphere tests at `A` and at `B` for the hemispherical caps.
Take the smallest positive `t` across all shapes. On a tie within `1e-4`, prefer the
**capsule** (a joint sphere only exists where no capsule covers the point, so a tie is a
float artefact, not a genuine ambiguity).

**`Zenith_Maths::Intersections` has no ray-sphere test.** It has Circle (`:7`), AABB
(`:32`) and Cylinder (`:63`) only. The one at
`Zenith/Flux/Skybox/Flux_AtmosphereTransmittance.h:45` is origin-centred and
atmosphere-specific. **WU-4.1 adds `RayIntersectsSphere` to
`Zenith/Maths/Zenith_Maths_Intersections.h`**, origin-anchored to match its neighbours:

```cpp
static bool RayIntersectsSphere(const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir, float fRadius, float& fOutDistance);
```

That is a shared-file edit and it is **WU-4.1's alone**; no other unit may touch that header.

### CPU-only and headless

Nothing here reaches the GPU. `Flux_SkeletonInstance` holds no device resources at all —
its `Destroy()` clears an asset handle and a bone count and nothing else
(`Flux_SkeletonInstance.cpp:127-131`), and the comment at `Flux_SkeletonInstance.h:136-138`
records that the legacy bone buffer was retired. A test therefore builds a skeleton in
code with `Zenith_SkeletonAsset::AddBone` + `ComputeBindPoseMatrices`
(`Zenith_SkeletonAsset.h:133,150`), calls `Flux_SkeletonInstance::CreateFromAsset`
(`:38`), `ComputeSkinningMatrices()`, then fires rays. No device, no view, no window.

---

## 3. Bone-local ↔ model ↔ world

### 3.1 The four matrices, named once

| Symbol | What it is | Where it comes from |
|---|---|---|
| `L_i` | bone `i`'s local matrix, `T(p) · R(q) · S(s)` | `Flux_SkeletonInstance::ComposeTransformMatrix`, `Flux_SkeletonInstance.cpp:231-241` |
| `M_i` | bone `i`'s model matrix, `M_parent · L_i` (roots: `M_i = L_i`) | `ComputeSkinningMatrices`, `Flux_SkeletonInstance.cpp:308-318`; read back via `GetBoneModelTransform(i)`, `Flux_SkeletonInstance.h:119` |
| `W` | the session model matrix, model → world | `Zenith_AnimationPreviewSession::GetSessionModelMatrix()` (§1) |
| `B⁻¹_i` | inverse bind pose | `Zenith_SkeletonAsset::Bone::m_xInverseBindPose`, `Zenith_SkeletonAsset.h:50` |

**★ `B⁻¹_i` takes no part in pose authoring.** It appears in exactly one place — the
skinning matrix, `m_axSkinningMatrices[i] = m_axModelSpaceTransforms[i] * xBone.m_xInverseBindPose`
(`Flux_SkeletonInstance.cpp:321`). It converts *mesh* vertices, not *bones*. A subagent
reaching for it while converting a gizmo delta is about to write a bug that looks
plausible and is wrong at every non-identity bind pose. The bind pose (`m_xBindPosition`
/ `m_xBindRotation` / `m_xBindScale`, `Zenith_SkeletonAsset.h:43-45`) is likewise only a
*seed* — `SetToBindPose` (`Flux_SkeletonInstance.cpp:136`) and the controller's per-frame
reseed (`Flux_AnimationController.cpp:272-277`) — never a factor in a delta.

**Decision: `W` is identity for the whole of Phase 4.** The preview camera orbits the
origin (`Flux_PreviewOrbitCameraPos` / `Flux_PreviewBuildViewConstants`,
`Flux_MaterialPreviewController.h:50,78-97`), so a non-identity session model matrix buys
nothing and adds a term to every conversion. Every helper still takes `W` explicitly so
the tests can pin the general case, and so the day a session places the character
somewhere else, nothing has to be re-derived.

### 3.2 The precondition nobody can see

`GetBoneModelTransform` returns a **cache**. It is written only by
`ComputeSkinningMatrices` (`Flux_SkeletonInstance.cpp:296-318`), and
`SetBoneLocalTransform` (`:157-172`) does **not** invalidate or update it. So a live pose
write followed immediately by a model-space read returns the *previous frame's* geometry
— pick shapes one frame stale, a gizmo ring one frame behind the bone. There is no
assert and no symptom other than lag.

Hence `Zenith_AnimationPreviewSession::RefreshDerivedPose()` (§1), which is a single call
to `m_pxSkeletonInstance->ComputeSkinningMatrices()`, and the rule: **every pose write is
followed by `RefreshDerivedPose()` before anything reads a model matrix.**

### 3.3 The conversions

```cpp
// Zenith/Editor/Animation/Zenith_BoneSpace.h                  (NEW — WU-4.2)
// Pure free functions. No ImGui, no Flux, no ECS. Headlessly unit-tested.

namespace Zenith_BoneSpace
{
	// M_i. Identity when the index is out of range.
	Zenith_Maths::Matrix4 BoneModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	// M_parent(i), or identity for a root bone / an out-of-range index.
	Zenith_Maths::Matrix4 ParentModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	// W * M_i, and its translation column.
	Zenith_Maths::Matrix4 BoneWorldMatrix(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);
	Zenith_Maths::Vector3 BoneWorldPosition(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	// The NORMALISED rotation of W * M_parent(i) — the frame a world-space delta must be
	// conjugated into. Extracted with Zenith_Maths::DecomposeTRS (Zenith/Maths/Zenith_Maths.h:94).
	Zenith_Maths::Quat ParentWorldRotation(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	// conj(qParentWorld) * dQworld * qParentWorld
	Zenith_Maths::Quat WorldDeltaToBoneLocalDelta(const Zenith_Maths::Quat& xWorldDelta,
		const Zenith_Maths::Quat& xParentWorldRotation);

	// The whole gesture, in one call: q' = WorldDeltaToBoneLocalDelta(...) * q
	Zenith_Maths::Quat ApplyWorldDeltaToBoneLocal(const Zenith_Maths::Quat& xWorldDelta,
		const Zenith_Maths::Quat& xParentWorldRotation,
		const Zenith_Maths::Quat& xBoneLocalRotation);
}
```

### 3.4 Why the conjugation, derived

The manipulator produces a world-space delta `dQ_w` about a world axis through the
bone's world pivot — the same construction the entity gizmo uses,
`deltaRotation = glm::angleAxis(angle, axis)` with the axis frozen at drag start
(`Flux_Gizmos.cpp:874`). For an entity that is the end of it:
`newRotation = deltaRotation * initialRotation` (`Flux_Gizmos.cpp:875`).

A bone needs one more step, because `q_i` is expressed in its **parent's** frame. Let
`P = W · M_parent(i)` and `qP = rotation(P)`. The bone's world orientation is `qP · q_i`.
Applying the world delta on the left:

```
qP · q_i'  =  dQ_w · qP · q_i
     q_i'  =  conj(qP) · dQ_w · qP · q_i
```

so `dQ_local = conj(qP) · dQ_w · qP` and `q_i' = dQ_local · q_i`. For a root bone
`qP = rotation(W)` = identity under §3.1, and the bone case collapses to the entity case
— which is a useful test: **a root-bone drag must produce exactly what the entity gizmo
would.**

The world pivot for the ring is `BoneWorldPosition(W, skeleton, i)`, i.e. the
translation column of `W · M_i` — the joint the bone's rotation *moves*, not the joint it
*rotates about*. Both are defensible; use `M_i` because it is where the user sees the
handle, and pin it with a test.

**Non-uniform bone scale is a known limit.** `L_i`'s 3×3 is `R·S`
(`Flux_SkeletonInstance.cpp:236-240`), so a quaternion recovered from `P` is only the
true rotation when the accumulated scale is uniform and positive. `Zenith_Maths::DecomposeTRS`
is documented for exactly that class of matrix (`Zenith/Maths/Zenith_Maths.h:86-95`) and
normalises the result. Bind scales are 1 in every rig in the tree. WU-4.2 should assert
uniformity rather than silently produce a sheared frame.

---

## 4. Drag transactions and the undo stack

### 4.1 Two layers, and only one of them is undoable

| Layer | Written | Undoable | Serialized |
|---|---|---|---|
| the LIVE POSE — the session instance's local rotations | every drag frame | no | no |
| the KEYS — the document's channels | once, on release, and only if a key is actually written | **yes** | yes |

Nothing is pushed on the undo stack during a drag, and nothing is written to the
document during a drag. This is the "apply on release, not per drag frame" precedent
(`Zenith/EntityComponent/Components/CLAUDE.md:248-253`) and
`Zenith_Editor::RecordGizmoDragUndo` (`Zenith/Editor/Zenith_Editor.cpp:1287-1315`), which
compares the transform captured at `BeginInteraction` with the live one and records **one**
command, skipping a click that never moved (`:1309-1312`). Do the same: a drag whose
accumulated delta is within `1e-6` of identity records nothing.

### 4.2 The live pose during the drag

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPreviewSession.h    (further ADDITIONS)

	// While a bone drag is in flight the session must NOT re-evaluate from the clip —
	// a per-frame evaluate would stomp the drag. BeginBoneDrag latches the bone's
	// local rotation, suspends evaluation, and EndBoneDrag resumes it.
	void BeginBoneDrag(u_int uBoneIndex);
	void UpdateBoneDrag(const Zenith_Maths::Quat& xNewBoneLocalRotation);   // + RefreshDerivedPose()
	void EndBoneDrag();
	bool IsBoneDragActive() const { return m_bBoneDragActive; }

	u_int              GetDragBoneIndex()        const { return m_uDragBoneIndex; }
	Zenith_Maths::Quat GetDragInitialRotation()  const { return m_xDragInitialRotation; }
	Zenith_Maths::Quat GetBoneLocalRotation(u_int uBoneIndex) const;

	// TRUE when the live pose has been dragged away from what the clip evaluates to and
	// no key has been written. The panel must SHOW this — seeking discards it (§4.4).
	bool HasUnkeyedPose() const { return m_bUnkeyedPose; }

private:
	bool               m_bBoneDragActive     = false;
	bool               m_bUnkeyedPose        = false;
	u_int              m_uDragBoneIndex      = kuINVALID_BONE_SELECTION;
	Zenith_Maths::Quat m_xDragInitialRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
```

`UpdateBoneDrag` writes through `Flux_SkeletonInstance::SetBoneLocalTransform(uBone, pos,
rot, scale)` (`Flux_SkeletonInstance.h:62`) — position and scale are read back unchanged
from `GetBoneLocalPosition` / `GetBoneLocalScale` (`:70,80`), because that setter takes
all three and there is no rotation-only overload — then calls `RefreshDerivedPose()`.

### 4.3 The command

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPoseCommands.h      (NEW — WU-4.3)

enum Zenith_AnimTrackKind : u_int
{
	ZENITH_ANIM_TRACK_ROTATION = 0,
	ZENITH_ANIM_TRACK_TRANSLATION,
	ZENITH_ANIM_TRACK_SCALE,
};

struct Zenith_AnimKeyValue
{
	Zenith_AnimTrackKind  m_eKind     = ZENITH_ANIM_TRACK_ROTATION;
	Zenith_Maths::Quat    m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 m_xVector   = Zenith_Maths::Vector3(0.0f);
};

// ONE drag, or ONE Set Key press, or ONE IK bake = ONE command, however many tracks
// it moved. Addresses (document, trackId, keyId) per D24/D29 — NOT an EntityID.
class Zenith_UndoCommand_AnimPoseKeys : public Zenith_UndoCommand
{
public:
	struct TrackKey
	{
		u_int               m_uTrackId       = 0u;
		float               m_fTimeSeconds   = 0.0f;
		Zenith_AnimKeyValue m_xOldValue;
		Zenith_AnimKeyValue m_xNewValue;
		bool                m_bExistedBefore = false;   // false => Undo REMOVES the key
		u_int               m_uKeyId         = 0u;      // stable, non-serialized (D24/D29)
	};

	Zenith_UndoCommand_AnimPoseKeys(Zenith_AnimationDocument* pxDocument, const char* szDescription);

	void AddTrackKey(const TrackKey& xKey);
	u_int GetTrackKeyCount() const { return m_xKeys.GetSize(); }

	void Execute() override;                       // redo: re-apply every m_xNewValue
	void Undo() override;                          // restore m_xOldValue, or RemoveKeyframe when !m_bExistedBefore
	const char* GetDescription() const override { return m_strDescription.c_str(); }

private:
	Zenith_AnimationDocument* m_pxDocument = nullptr;
	Zenith_Vector<TrackKey>   m_xKeys;
	std::string               m_strDescription;
};
```

It is `Record`ed, never `Execute`d, on creation — `Zenith_UndoSystem::Record` exists for
exactly this ("add an ALREADY-APPLIED command to the undo stack without running
Execute", `Zenith/Editor/Zenith_UndoSystem.h:100-104`).

**★ Lifetime obligation this creates.** The command holds a raw
`Zenith_AnimationDocument*`. Every existing command resolves its target dynamically from
an `Zenith_EntityID` precisely so a stale handle cannot be dereferenced
(`Zenith_UndoSystem.h:33-42`). A document pointer has no such protection, so **closing a
document must clear both undo stacks** (`Zenith_UndoSystem::Clear()`, `:124`), exactly as
a scene load does. That contract belongs to whoever owns document open/close — a Phase-3
deliverable — and it is not visible from inside Phase 4. **Flag it into the Phase-3
brief; if it is not there, Phase 4 must add it and say so.**

### 4.4 What a discarded drag looks like

With auto-key **off**, a released drag writes no key and creates no undo entry. The pose
is live-but-unkeyed and is destroyed by the next seek, because the session re-evaluates
from the clip. That is the one thing a user can silently lose, so it is not allowed to be
silent: `HasUnkeyedPose()` must be surfaced in the panel (a badge next to the time
field), and seeking while it is true must be an explicit, visible discard. Making the
drag itself undoable was rejected — an undo stack entry that restores a pose the document
never contained is a lie about what was saved.

---

## 5. Set Key and auto-key

### 5.1 Which tracks

**Rotation only, for the bones the gesture touched.** Translation is authorable **only**
by an explicit action on a **root** bone (`m_iParentIndex == INVALID_BONE_INDEX`). Scale
is not authorable in Phase 4.

The reason is not tidiness. `Flux_SkeletonPose::SampleFromClip` writes a component **only
if that channel has keyframes**:

```
if (xChannel.HasPositionKeyframes())  … = xChannel.SamplePosition(fTimeInTicks);
if (xChannel.HasRotationKeyframes())  … = xChannel.SampleRotation(fTimeInTicks);
if (xChannel.HasScaleKeyframes())     … = xChannel.SampleScale(fTimeInTicks);
```
— `Flux_BonePose.cpp:254-259`, and the comment above it says so: *"This preserves bind
pose values for components not animated"*. Bones with no channel keep bind pose, seeded
by the controller each frame (`Flux_AnimationController.cpp:267-277`).

So **adding the first key to a channel changes that bone's behaviour across the entire
clip**, from "follows bind pose" to "follows a single constant". Writing translation and
scale keys "for consistency" alongside a rotation edit would therefore alter frames the
user never touched, in a way that only shows up on playback. Write the channel the user
authored, and nothing else.

### 5.2 At what time — and the unit hazard

```
fSnappedSeconds = roundf(fCurrentSeconds * fFrameRate) / fFrameRate
```

`fFrameRate` is the document's authored frame rate; when it is 0, do not snap and log
once. Snapping is **unconditional**, not a toggle: unsnapped keys land at arbitrary float
times, which makes the dope sheet's frame columns lie and makes D11's collision
behaviour unpredictable.

**★ THE CHANNEL STORES TICKS, NOT SECONDS.** `Flux_BoneChannel::AddPositionKeyframe(float
fTimeTicks, …)` / `AddRotationKeyframe` / `AddScaleKeyframe` (`Flux_AnimationClip.h:89-91`),
and both `SampleFromClip` overloads convert on the way in —
`float fTimeInTicks = fTime * xClip.GetTicksPerSecond();` (`Flux_BonePose.cpp:168`, `:201`).
`Zenith/AssetHandling/CLAUDE.md` records the same for the bush export: *"per-bone rotation
clip with keyframe times in **TICKS** (0..120, not 0..4 seconds)"*.

The WU-4.0 brief says the Phase-3 mutators take **seconds**. Both cannot describe the same
storage. Whichever way Phase 3 lands it, Phase 4 must convert in **exactly one place**:

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPoseCommands.h
// The ONE seconds <-> ticks conversion on the authoring path. If Phase 3's mutators
// really take seconds these are the identity and this comment is the proof it was
// checked; if they take ticks, this is where the factor lives. Do not scatter it.
inline float Zenith_AnimSecondsToChannelTime(float fSeconds, u_int uTicksPerSecond);
inline float Zenith_AnimChannelTimeToSeconds(float fChannelTime, u_int uTicksPerSecond);
```

**And `m_uAuthoredFrameRate` should not exist.** `Flux_AnimationClipMetadata` already
carries `uint32_t m_uTicksPerSecond = 24` (`Flux_AnimationClip.h:120`) with
`GetTicksPerSecond()` / `SetTicksPerSecond()` (`:174,189`), and it is the number
`SampleFromClip` divides by. A second frame-rate field on the document is the same
duplicated-pin failure the build system spent a refactor removing: two numbers, one
authority, and a silent factor-of-N when they disagree. **Recommendation: the authored
frame rate IS `m_uTicksPerSecond`.**

### 5.3 Duplicate times

D11 applies: `InsertKeyframeAt` on an occupied time **replaces** the key, preserving its
identity. So Set Key at an already-keyed frame is a value edit —
`m_bExistedBefore = true`, `m_xOldValue` = the previous value, and Undo restores the value
rather than removing the key.

**`SetKeyframeTime` must never appear on this path.** By D11 it *fails*, returning false,
when the destination is occupied. It is the dope sheet's key-drag verb. Pose authoring
only ever inserts-or-replaces at the playhead, where failing is not an acceptable outcome
and silently doing nothing is worse.

Two keys are "the same time" when `fabsf(a - b) <= fANIM_TIME_EPSILON` (`1e-5f`). Note
that after §5.2's snapping the comparison is against a grid, so the epsilon only absorbs
float error in `roundf(x * r) / r`, not genuine near-misses.

### 5.4 One writing path

```cpp
// Zenith/Editor/Panels/Zenith_EditorPanel_Animation.h         (Phase-3 class, ADDITIONS)

	// THE key-writing verb. Every caller — the Set Key button, auto-key on drag release,
	// and the IK bake (§6) — goes through this one function, so a key written three
	// different ways is byte-identical. Returns false when there is no document, no
	// session, no selection, or nothing to write.
	static bool Action_SetKeyForBones(const Zenith_Vector<u_int>& xBoneIndices,
		Zenith_AnimTrackKind eKind,
		const char* szUndoDescription);

	static bool Action_SetKeyForSelectedBone();          // the toolbar button / S
	static bool Action_SetAutoKey(bool bEnabled);
	static bool Action_GetAutoKey();
```

Auto-key state lives on the **session** (`bool m_bAutoKey = false;`), not on the document
— it is a per-editing-session preference, not clip content. On `EndBoneDrag`, if
`m_bAutoKey` and the accumulated delta is non-trivial, the panel calls
`Action_SetKeyForBones({dragBone}, ZENITH_ANIM_TRACK_ROTATION, "Pose Bone")`.

Auto-key writes **one key, at the playhead**. Rejected: Maya-style bracketing, which
inserts an extra key at the previous keyed time to "hold" the earlier pose. It silently
doubles the authored data and produces keys the user did not ask for — and with §5.1's
first-key rule, a bracketing key on an empty channel changes the whole clip twice over.

### 5.5 Automation

Mirroring the graph verbs (`Editor/CLAUDE.md`, *Graph Authoring via Editor Automation*),
each atomic action gets a step so a pose can be authored at boot and diffed:

```cpp
// Zenith/Editor/Zenith_EditorAutomation.h
	static void AddStep_AnimSelectBone(u_int uBoneIndex);
	static void AddStep_AnimRotateBoneLocal(u_int uBoneIndex, float fX, float fY, float fZ, float fW);
	static void AddStep_AnimSetKey();
	static void AddStep_AnimSetAutoKey(bool bEnabled);
	static void AddStep_AnimBakeIK(const char* szChainName);
```

**★ These land in a CONTIGUOUS enum block** and every block in
`Zenith_EditorAutomation` carries a "must stay CONTIGUOUS" comment naming its first and
last member, because `ExecuteAction` routes by `>=` / `<=` range comparison and an action
inserted mid-block silently routes to the wrong sub-executor (`Editor/CLAUDE.md`, *The
split dispatcher: twelve contiguous ranges*). The youngest block is additionally pinned
by a `static_assert` on its width plus a unit test on each member's position. Follow that
pattern exactly; it is not optional.

**Authored rotations that reach a committed asset must not use glm.** If a pose authored
by these steps is ever saved into a tracked `.zanim`, the quaternion must come from the
`Zenith_Maths::Authoring*` helpers or be passed verbatim
(`Zenith/Maths/Zenith_Maths.h:53-84`; `Editor/CLAUDE.md`, *AUTHORED ROTATIONS THAT LAND IN
A COMMITTED SCENE*). `AddStep_AnimRotateBoneLocal` takes `(x, y, z, w)` in **serialized
order** — deliberately not `glm::quat`'s `(w, x, y, z)` — and performs no arithmetic, for
the same reason `AddStep_SetTransformRotationQuat` does.

---

## 6. IK-assisted posing, baked to keys

### 6.1 How the editor gets a solved pose without violating the warning

Read the warning precisely. `Flux_InverseKinematics.h:170-176` says:

> *When this solver is owned by a `Flux_AnimationController`, the controller invokes
> `Solve()` automatically inside `ApplyOutputPoseToSkeleton`. Game code should set targets
> via the controller … and not call `Solve()` directly — doing so would run IK twice per
> frame on the same pose.*

The hazard is **solving the controller's pose twice**, not "`Solve` is off limits". The
editor avoids it by solving a pose the controller does not own:

- a **scratch** `Flux_SkeletonPose`, local to the call, seeded from the session skeleton
  instance's current bone-local TRS — **not** from `Flux_AnimationController::GetOutputPose()`
  (`Flux_AnimationController.h:71`);
- a **transient** `Flux_IKChain`, not registered with the controller's solver, so
  `m_pxIKSolver->GetChains()` stays empty and the guard at
  `Flux_AnimationController.cpp:334` never fires;
- `Flux_IKSolver::SolveChain` — the **public single-chain** entry
  (`Flux_InverseKinematics.h:182-185`) — rather than `Solve`. It takes no world matrix at
  all, which forces the model-space contract and removes the `m_bIsModelSpace` /
  `m_xWorldMatrix` interaction described at `Flux_InverseKinematics.h:21-28`.

The controller's `m_xOutputPose` is never touched and its own solver is never invoked. No
pose is solved twice, and nothing about the runtime IK path changes.

### 6.2 Signature

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPoseIK.h            (NEW — WU-4.4)

namespace Zenith_AnimationPoseIK
{
	struct SolveRequest
	{
		const Zenith_SkeletonAsset* m_pxSkeleton = nullptr;
		Flux_IKChain                m_xChain;                 // BY VALUE — the helper resolves + measures it
		Zenith_Maths::Vector3       m_xTargetModelSpace = Zenith_Maths::Vector3(0.0f);
		float                       m_fWeight = 1.0f;
	};

	// Seeds a scratch pose from the instance's CURRENT local TRS, resolves + measures the
	// chain, composes model space, solves ONE chain, recomposes, and writes back the
	// solved BONE-LOCAL rotations, one pair per chain bone, in chain order.
	// Returns false and writes nothing when the skeleton is null, the chain has fewer
	// than two bones, or any bone name fails to resolve.
	// PURE with respect to engine state: touches no controller, no live scene, no GPU.
	bool SolveChainToLocalRotations(const Flux_SkeletonInstance& xSkeleton,
		const SolveRequest& xRequest,
		Zenith_Vector<std::pair<u_int, Zenith_Maths::Quat>>& xOutBoneLocalRotations);
}
```

Body, in order — every call named so no subagent has to guess:

1. `Flux_SkeletonPose xScratch; xScratch.Initialize(xSkeleton.GetNumBones());`
   (`Flux_BonePose.h:84`)
2. For each bone `i`: `xScratch.GetLocalPose(i)` (`:90`) ← `GetBoneLocalPosition/Rotation/Scale(i)`
   (`Flux_SkeletonInstance.h:70,75,80`).
3. `xRequest.m_xChain.ResolveBoneIndices(*xRequest.m_pxSkeleton);` (`Flux_InverseKinematics.h:93`)
4. `xScratch.ComputeModelSpaceMatricesFromSkeleton(*xRequest.m_pxSkeleton);` (`Flux_BonePose.h:125`)
5. `xRequest.m_xChain.ComputeBoneLengths(xScratch);` (`Flux_InverseKinematics.h:96`)
6. Build a `Flux_IKTarget` with `m_xPosition = m_xTargetModelSpace`, `m_fWeight`,
   `m_bEnabled = true`, `m_bIsModelSpace = true`, `m_bUseRotation = false`
   (`Flux_InverseKinematics.h:14-29`).
7. `Flux_IKSolver xSolver; xSolver.SolveChain(xScratch, xRequest.m_xChain, xTarget, *xRequest.m_pxSkeleton);`
8. `xScratch.ComputeModelSpaceMatricesFromSkeleton(...)` again — mirroring the
   controller's pre/post recompute (`Flux_AnimationController.cpp:337-339`), whose comment
   explains that the post-solve recompute is what keeps model matrices consistent for
   *"downstream CPU readers (debug draw, gizmos, animation tools)"*. That is us.
9. Emit `(boneIndex, xScratch.GetLocalPose(boneIndex).m_xRotation)` for each chain bone.

### 6.3 The bake

The returned pairs go straight through `Action_SetKeyForBones(boneIndices,
ZENITH_ANIM_TRACK_ROTATION, "IK Pose")` from §5.4 — **the same function a hand drag
uses**. So one `Zenith_UndoCommand_AnimPoseKeys` covers the whole chain, one Ctrl+Z undoes
the whole IK gesture, and the clip contains nothing IK-specific. That is what "baked down
to keys" has to mean: after the fact, an IK-posed frame is indistinguishable from a
hand-posed one, and nothing in the `.zanim` needs a solver to play back.

The live pose is applied the same way as a drag — `UpdateBoneDrag`-style writes through
`SetBoneLocalTransform` plus `RefreshDerivedPose()` — so the IK path also honours §4.4:
solve without auto-key, and the pose is visible but unkeyed until Set Key.

### 6.4 Chain scope for Phase 4

A **transient three-bone chain built from the selected bone and its two ancestors**, via
`Flux_IKSolver::CreateArmChain` / `CreateLegChain` (`Flux_InverseKinematics.h:192-200`)
when the names match those helpers' expectations, otherwise assembled directly into
`Flux_IKChain::m_xBoneNames`. **No chain-authoring UI, no chain serialization, no
constraints, no pole vector.** `Flux_JointConstraint` (`:35`) and `m_xPoleVector` (`:81`)
are left at their defaults; wiring them needs a UI to author them, which is a later
phase. The three-bone limit is a scope choice, not a solver limit — FABRIK handles any
length.

---

## 7. Fill-ins for WU-4.1 … WU-4.4

Read §8 first — the split below is the **post-re-plan** one.

### WU-4.1 — selectable bone target, hit geometry, and the shared surface

**Creates**
- `Zenith/Editor/Animation/Zenith_BonePickGeometry.h`
- `Zenith/Editor/Animation/Zenith_BonePickGeometry.cpp`
- `Zenith/Editor/Animation/Zenith_BonePickGeometry.Tests.inl`

**Owns (sole writer for all of Phase 4)**
- `Zenith/Editor/Animation/Zenith_AnimationPreviewSession.{h,cpp}` — lands the **complete**
  member and method set from §1 and §4.2 in one go, including the drag methods WU-4.3 will
  use.
- `Zenith/Editor/Panels/Zenith_EditorPanel_Animation.{h,cpp}` — declares **all** `Action_*`
  from §5.4 and §8, with bodies that `return false`, so 4.3 and 4.4 fill bodies rather than
  add declarations.
- `Zenith/Maths/Zenith_Maths_Intersections.h` — adds `RayIntersectsSphere` only.

**Implements**
- `Zenith_BuildBonePickSet`, `Zenith_RaycastBonePickSet`, `Zenith_BonePickShape`,
  `Zenith_BonePickSet`, the four `kf…` constants, `kuINVALID_BONE_SELECTION`.
- `SelectBone` / `ClearBoneSelection` / `GetSelectedBoneIndex` / `HasBoneSelection` /
  `SetHoveredBoneIndex` / `GetHoveredBoneIndex` / `GetSessionModelMatrix` /
  `SetSessionModelMatrix` / `RefreshDerivedPose` / `GetSkeletonInstance`.
- `RayIntersectsSphere`.

**Must pin with tests** — the capsule belongs to the child bone (§2); a leaf bone is
pickable; a degenerate bone falls back to its joint sphere; the nearest hit wins; a miss
leaves both outputs untouched; the pick set is stale until `ComputeSkinningMatrices` runs
(§3.2).

### WU-4.2 — space conversions (pure maths, no UI)

**Creates**
- `Zenith/Editor/Animation/Zenith_BoneSpace.h`
- `Zenith/Editor/Animation/Zenith_BoneSpace.cpp`
- `Zenith/Editor/Animation/Zenith_BoneSpace.Tests.inl`

**Owns** — those three files only. Touches nothing else.

**Implements** — the seven functions in §3.3, verbatim.

**Must pin with tests** — a root-bone world delta reproduces the entity-gizmo result
(`newRotation = delta * initial`, `Flux_Gizmos.cpp:875`); a delta applied to a rotated
parent produces the conjugated local delta; `ParentModelMatrix` is identity for a root;
`W` non-identity composes correctly; the inverse bind pose is **not** referenced anywhere
in the file.

### WU-4.3 — manipulator, drag transactions, Set Key, auto-key

**Creates**
- `Zenith/Editor/Animation/Zenith_AnimationPoseCommands.h`
- `Zenith/Editor/Animation/Zenith_AnimationPoseCommands.cpp`
- `Zenith/Editor/Animation/Zenith_AnimationPoseCommands.Tests.inl`
- `Zenith/Editor/Panels/Zenith_EditorPanel_Animation_Pose.cpp` — the manipulator + drag +
  key bodies, in their own TU behind WU-4.1's declarations.

**Fills** — `Action_SetKeyForBones`, `Action_SetKeyForSelectedBone`, `Action_SetAutoKey`,
`Action_GetAutoKey`, plus the hover/pick/drag frame handler.

**Also edits** — `Zenith/Editor/Zenith_EditorAutomation.{h,cpp}` for the five
`AddStep_Anim*` verbs and their contiguous enum block (§5.5). This is the one place
WU-4.3 touches a file outside its own set; it is a contiguous append, and the
`static_assert` + block-position unit test are part of the deliverable.

**Implements** — `Zenith_AnimTrackKind`, `Zenith_AnimKeyValue`,
`Zenith_UndoCommand_AnimPoseKeys` (+ `TrackKey`), `Zenith_AnimSecondsToChannelTime` /
`Zenith_AnimChannelTimeToSeconds`, the ImGui-draw-list rotation ring (§8.1).

**Must pin with tests** — one drag = one command however many frames it spanned; a drag
with no movement records nothing; Set Key on an empty channel inserts and Undo removes;
Set Key on an occupied time replaces and Undo restores the value, keeping the key id
(D11); the key time is snapped to the frame grid; the seconds↔ticks conversion is
exercised in both directions.

### WU-4.4 — IK-assisted posing, baked to keys

**Creates**
- `Zenith/Editor/Animation/Zenith_AnimationPoseIK.h`
- `Zenith/Editor/Animation/Zenith_AnimationPoseIK.cpp`
- `Zenith/Editor/Animation/Zenith_AnimationPoseIK.Tests.inl`
- `Zenith/Editor/Panels/Zenith_EditorPanel_Animation_IK.cpp`

**Fills** — `Action_BakeIKForSelectedChain` (declared by WU-4.1).

**Implements** — `Zenith_AnimationPoseIK::SolveRequest`,
`Zenith_AnimationPoseIK::SolveChainToLocalRotations`, the nine-step body in §6.2.

**Depends on** WU-4.3's `Action_SetKeyForBones` being *declared* (it is, by WU-4.1) and
ideally *implemented*. If 4.3 has not landed, 4.4 still compiles and its own tests still
pass; only the end-to-end bake needs 4.3.

**Must pin with tests** — an unreachable target clamps to the chain's total length rather
than diverging; a solved chain's local rotations reproduce the solved model-space joint
positions when recomposed; the controller's `GetOutputPose()` is byte-unchanged across a
solve; a chain with an unresolvable bone name returns false and writes nothing.

---

## 8. RE-PLAN REQUIRED

Two things in the four-way split do not survive contact with the tree. Neither is a
scoping failure of the work; both are ownership/feasibility corrections.

### 8.1 WU-4.2's "gizmo drive" is not deliverable as written

`Flux_GizmosImpl` cannot manipulate a bone in the preview view, for two independent
reasons:

1. **It is entity-typed all the way down.** `m_pxTargetEntity` (`Flux_GizmosImpl.h:206`),
   `SetTargetEntity(Zenith_Entity*)` (`:128`), and every one of `GatherGizmoPacket`,
   `BeginInteraction`, `RaycastGizmo`, `ApplyTranslation`, `ApplyRotation`, `ApplyScale`
   opening with `GetGizmoTargetWithTransform()` (`Flux_Gizmos.cpp:237,413,749,800,837,881`).
   Generalising it means a second target-source indirection beside `g_xGizmoTransformAccess`
   — a `Flux/Gizmos/` change, in a file no Phase-4 unit owns.
2. **It draws in the wrong view, and that is the harder half.** Its one pass writes
   `GetFinalRenderTarget()` with no `.View(slot)` (`Flux_Gizmos.cpp:369-371`), so it
   renders with the MAIN camera's constants, over the main viewport. The preview session
   renders into `kuFluxViewSlotPreview`. Making the gizmo per-view is a render-graph change
   (a second pass, preview-view transients, a `.View()` selection) that is not in any of
   the four briefs. `Flux_PrimitivesImpl` is no help — it writes the main G-buffer MRTs
   (`Flux_Primitives.cpp:571-575`).

**Correction.** The bone manipulator is an **ImGui draw-list overlay** on the preview
image inside `Zenith_EditorPanel_Animation`: project the bone's world pivot and the three
ring axes through the preview view/proj, draw the rings with `ImDrawList`, hit-test in
2D, and turn the drag into a world-space `dQ_w` fed to `Zenith_BoneSpace`. This is the
editor's own documented convention for anything drawn over the viewport ("★ Draw-list
decorations, not items", `Editor/CLAUDE.md`), it needs no renderer change, and it keeps
every piece of maths CPU-pure and headlessly testable — which §2 and §3 require anyway.

The ray for picking comes from `Zenith_Gizmo::ScreenToWorldRay(mousePos, {0,0},
imageSize, view, proj)` (`Zenith/Editor/Zenith_Gizmo.h:14-20`, implementation
`Zenith_Gizmo.cpp:9-51` — pure, no engine state), with `view`/`proj` built by
`Flux_PreviewBuildViewConstants(yaw, pitch, distance, out)`
(`Flux_MaterialPreviewController.h:78-97`, also pure) and the ray origin from
`Flux_PreviewOrbitCameraPos` (`:50`). `Flux_GizmosImpl::SnapValue(float, float)` is
`static` and pure (`Flux_GizmosImpl.h:161`) and **is** reusable for angle snapping.

**Consequence for the split:** with the manipulator moved into the panel, WU-4.2 and
WU-4.3 would both own `Zenith_EditorPanel_Animation_Pose.cpp`. So **the manipulator moves
into WU-4.3**, and **WU-4.2 becomes pure space maths with no UI at all** — three files,
seven functions, fully unit-testable, zero shared-file contention. That is the natural cut
and it makes 4.2 the easiest of the four rather than the most entangled.

### 8.2 Three units would otherwise write the same two files

As briefed, `Zenith_AnimationPreviewSession.{h,cpp}` is written by 4.1 (selection), 4.2
(refresh) and 4.3 (drag state); `Zenith_EditorPanel_Animation.{h,cpp}` is written by all
four. Four parallel subagents editing two shared files is exactly the failure this spike
exists to prevent.

**Correction.** WU-4.1 becomes the **sole owner** of both, and lands the complete surface
up front — every member in §1 and §4.2, every `Action_*` declaration in §5.4 and §7, with
stub bodies returning `false`. WU-4.3 and WU-4.4 then add their own TUs
(`…_Animation_Pose.cpp`, `…_Animation_IK.cpp`) and fill the declared stubs. WU-4.2 touches
neither file. That turns a four-way write conflict into a one-way dependency and gives
4.2/4.3/4.4 a stable compile target from the moment 4.1 lands.

**Sequencing this implies:** 4.1 first and alone; then 4.2, 4.3, 4.4 in parallel; 4.4's
end-to-end bake needs 4.3's `Action_SetKeyForBones` body but not its landing.

---

## 9. Open questions the briefs must carry

1. **Seconds or ticks?** (§5.2) The brief and the tree disagree. Phase 3 must state which,
   and WU-4.3's conversion helpers must be the only place it is expressed.
2. **`m_uAuthoredFrameRate` vs `m_uTicksPerSecond`.** (§5.2) Two frame-rate numbers on one
   clip is a duplicated pin. Recommend one.
3. **Who clears the undo stack when a document closes?** (§4.3) The pose commands hold a
   raw document pointer — the first commands in the editor that hold a raw target. Unowned
   as far as Phase 4 can see.
4. **The preview view slot has an incumbent.** (§0) `Flux_MaterialPreviewController` drives
   `kuFluxViewSlotPreview` from its own liveness window
   (`Flux_MaterialPreviewController.h:144,156-161`). With the Material Editor and the
   Animation Editor both open, both stage the same slot's constants and both submit
   external items to it. Nothing in Phase 4 arbitrates this. Whether it presents as a
   flicker, a wrong camera, or a wrong mesh depends on `Update()` ordering — and it will
   look like an animation bug.
5. **Non-uniform bone scale.** (§3.4) `DecomposeTRS` is documented for shear-free,
   positive-scale matrices. Every rig in the tree has unit bind scale. Assert rather than
   assume.
