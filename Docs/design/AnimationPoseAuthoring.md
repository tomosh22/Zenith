# Animation Pose Authoring — WU-4.0 design spike

**Status:** design spike, since IMPLEMENTED. Phases 1–4 shipped as WU-4.1 (`36e1c4c4`),
WU-4.2 (`3acce596`), WU-4.3 (`f69e4c1d`) and WU-4.4 (`da697fce`). This note was written
before any of that code existed; it is left in place with its original structure and
voice, and every place the shipped code disagrees with it is marked with a
**"Corrected after implementation (2026-09-06):"** lead-in rather than silently
rewritten — including the one place (§2, bone-capsule ownership) where the original
reasoning was backwards rather than merely superseded by a later decision.
**Scope:** the six questions Phase 4 (pose authoring) splits across WU-4.1 … WU-4.4.
**Audience:** the four implementer briefs, and whoever reviews their diffs — now also
whoever is reading this after the fact to understand why the shipped code looks the way
it does.

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
| Phase-3 gives key times in **SECONDS** | **CONTRADICTED by the tree at design time.** See §5.2. **Corrected after implementation (2026-09-06): the tree was fixed before Phase 4 needed it — key times ARE seconds everywhere now, and the Phase-3 premise this row disputed turned out to be correct once the fix landed.** | `Flux_BoneChannel::AddPositionKeyframe(float fTimeTicks, …)` (`Flux_AnimationClip.h:89-91`) and `SampleFromClip` multiplies seconds by ticks-per-second before sampling (`Flux_BonePose.cpp:168`, `:201`) — this was the state of the tree when the row was written. Channel storage is now **seconds**; see `Zenith/Flux/MeshAnimation/CLAUDE.md` ("KEY TIMES ARE SECONDS … D3") and §5.2. |
| `Zenith_AnimationDocument`, `Zenith_AnimationPreviewSession`, `Zenith_EditorPanel_Animation`, `Flux_BoneChannel::InsertKeyframeAt` / `RemoveKeyframe` / `SetKeyframeTime` / `SetKeyframeValue`, `m_uAuthoredFrameRate`, `fANIM_TIME_EPSILON` | **Corrected after implementation (2026-09-06): ALL PRESENT NOW.** At the time this note was written none of these existed; Phases 1–4 (WU-4.1 `36e1c4c4`, WU-4.2 `3acce596`, WU-4.3 `f69e4c1d`, WU-4.4 `da697fce`) landed every one of them. | `Zenith_AnimationDocument.h`, `Zenith_AnimationPreviewSession.h` (`Zenith/Editor/`, **not** `Editor/Animation/` — see §7), `Zenith/Editor/Panels/Zenith_EditorPanel_Animation.h`, `Zenith/Flux/MeshAnimation/Flux_AnimationClip.h` (the four `Flux_BoneChannel` mutators), `Flux_AnimationClipMetadata::m_uAuthoredFrameRate` (`Flux_AnimationClip.h:120`, D6), `fANIM_TIME_EPSILON` (`Flux_AnimationClip.h`, D9) |

Two further findings that change what Phase 4 can build, detailed in §8 (both ADOPTED
as re-plan corrections, confirmed shipped):

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

**Corrected after implementation (2026-09-06): the ownership rule below is the
OPPOSITE of what shipped, and the shipped rule is the correct one.** The mistake was
made at design time and caught by WU-4.1's own header comment
(`Zenith/Editor/Animation/Zenith_BonePickGeometry.h:14-46`), which spells out the
derivation and calls this note wrong by name. What follows is corrected to match
`Zenith_BonePickGeometry.{h,cpp}`; the original wording is kept struck through in
spirit (see the block quote) because the reasoning error is worth keeping visible
rather than silently rewritten.

### Decision (as shipped)

Pick against **one capsule per (bone, child) pair, owned by the PARENT**, plus a joint
sphere for a leaf (no children — it owns no capsule) and for any bone whose every
capsule was degenerate. A bone with three children owns three capsules, all three
swinging together under that bone's own rotation. The root owns real capsules to its
children like any other bone — it is not a special case that "gets no shape".

**★ The capsule from joint `i` to joint `child(i)` belongs to bone `i` — the PARENT
whose rotation moves it — not to the child.** The original text in this note said the
opposite ("the capsule from `parent(i)` to `i` belongs to bone `i`"), and the error is
invisible to every geometric check because it never moves a vertex — it only mislabels
which bone a click resolves to. The proof is in the composition, not an opinion:

`Flux_SkeletonInstance::ComposeTransformMatrix` (`Flux_SkeletonInstance.cpp:231-241`)
builds `L_i = T(p_i) · R(q_i) · S(s_i)`, and `ComputeSkinningMatrices` (`:296-318`) sets
`M_i = M_parent · L_i`. The translation column of a product `A · (T(p) · R · S)` is
`A · (p, 1)`, because `R · S` contributes no translation — so
`translation(M_i) = M_parent · p_i`, with **no `q_i` in it**. Bone `i`'s own joint
therefore does **not** move when `q_i` changes; the joints **below** it do. The segment
a user sees swing when they rotate bone `i` is the one from joint `i` to joint
`child(i)`, so that is the segment that has to select bone `i`. Under the rule this note
originally stated, clicking the segment that visibly swings would select the *child* —
every drag would rotate the wrong joint, and it would still look entirely plausible in a
screenshot. Dragging the forearm edits the forearm bone (the elbow angle) precisely
because the forearm *owns* the capsule from the elbow to the wrist, not the other way
round.

### Types and entry points (as shipped)

```cpp
// Zenith/Editor/Animation/Zenith_BonePickGeometry.h            (WU-4.1)

struct Zenith_BonePickShape
{
	u_int                 m_uBoneIndex      = 0u;  // the bone this shape SELECTS
	Zenith_Maths::Vector3 m_xA              = Zenith_Maths::Vector3(0.0f);  // the owning bone's OWN joint
	Zenith_Maths::Vector3 m_xB              = Zenith_Maths::Vector3(0.0f);  // the CHILD joint the capsule runs to
	float                 m_fRadius         = 0.0f;
	bool                  m_bIsJointOnly    = false;
	u_int                 m_uChildBoneIndex = 0u;   // which child, for a bone owning several shapes
};

struct Zenith_BonePickSet
{
	Zenith_Vector<Zenith_BonePickShape> m_xShapes;
	float m_fSkeletonExtent = 0.0f;   // model-space AABB diagonal over all joints
};

// Rebuilds xOut from the instance's CURRENT model-space transforms, pre-multiplied by
// xSessionModel so every shape is in WORLD space and the picking ray needs no transform.
// xSkeletonAsset is taken EXPLICITLY (parent indices live on the asset, not the
// instance), and PRECONDITION: xSkeleton.ComputeSkinningMatrices() has run since the
// last pose write.
void Zenith_BuildBonePickSet(const Flux_SkeletonInstance& xSkeleton,
	const Zenith_SkeletonAsset& xSkeletonAsset,
	const Zenith_Maths::Matrix4& xSessionModel,
	Zenith_BonePickSet& xOut);

// Nearest non-negative hit. Returns false and leaves both outputs untouched on a miss.
bool Zenith_RaycastBonePickSet(const Zenith_BonePickSet& xSet,
	const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir,
	u_int& uOutBoneIndex,
	float& fOutDistance);
```

Radii, all derived from `m_fSkeletonExtent` so the same code works on a 1.8 m humanoid
and a 0.2 m prop rig — these matched the design intent exactly:

```cpp
inline constexpr float kfBONE_PICK_RADIUS_FRACTION   = 0.12f;   // of the segment length
inline constexpr float kfBONE_PICK_MIN_RADIUS_EXTENT = 0.004f;  // of the skeleton extent
inline constexpr float kfBONE_PICK_JOINT_EXTENT      = 0.020f;  // of the skeleton extent
inline constexpr float kfBONE_PICK_MIN_SEGMENT       = 1.0e-4f;
```

### The tie-break (corrected — a bounded joint-priority rule, not a flat 1e-4 tie)

The design brief's "on a tie within `1e-4`, prefer the capsule" undersold what a leaf
needs. A leaf owns no capsule — its only shape is a small joint sphere sitting at the far
END of its parent's capsule — so whenever the parent bone is long relative to its own
radius, the leaf's sphere is entirely NESTED inside the parent's rounded cap, and a plain
nearest-wins rule would report the parent for every ray that could ever reach the leaf.
The tip would be permanently unselectable on some rigs and not others, purely as a
function of how the radius happened to work out — not a 1e-4-wide float artefact at all.

The shipped rule (`kfBONE_PICK_JOINT_PRIORITY_RADII = 2.0f`,
`Zenith_BonePickGeometry.h:111-137`): **a joint sphere outranks a capsule it is sitting
inside**, but only while it is within `kfBONE_PICK_JOINT_PRIORITY_RADII` × (that
capsule's own radius) of the capsule's own hit — i.e. while it genuinely is inside the
geometry in front of it. A fingertip tucked behind a torso sits far further back than
that margin, and the torso in front of it wins, as it should. Among shapes of the SAME
class, nearest wins, with a `1e-4` tie going to the incumbent — that part of the original
text was correct, just incomplete without the cross-class rule above it.

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

**`Zenith_Maths::Intersections` had no ray-sphere test at design time and now does.**
WU-4.1 added `RayIntersectsSphere` to `Zenith/Maths/Zenith_Maths_Intersections.h`
(confirmed present, `:69-...`), origin-anchored to match its neighbours, and — one
detail the design brief did not specify — it reports the **nearest non-negative** hit,
so a ray whose origin starts inside the sphere reports the exit point rather than
missing: a picking ray that begins inside a joint's own ball must still be able to
select it. It is an ordinary `inline` free function in the header, not a `static`
member of a class:

```cpp
inline bool RayIntersectsSphere(const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir, float fRadius, float& fOutDistance);
```

### CPU-only and headless

Nothing here reaches the GPU. `Flux_SkeletonInstance` holds no device resources at all —
its `Destroy()` clears an asset handle and a bone count and nothing else
(`Flux_SkeletonInstance.cpp:127-131`) — and a test builds a skeleton in code, calls
`Flux_SkeletonInstance::CreateFromAsset`, `ComputeSkinningMatrices()`, then fires rays.
No device, no view, no window.

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

**Corrected after implementation (2026-09-06): a SEEK does not need this call — only a
DIRECT pose write does, and the shipped session says so explicitly.**
`Zenith_AnimationPreviewSession::Tick()` and `Seek()` route through
`Flux_AnimationController::SeekDirectPlay`, which ends in
`ApplyOutputPoseToSkeleton()` → `ComputeSkinningMatrices()` — so the cache is already
current by the time either returns, and calling `RefreshDerivedPose()` again after one
would be a redundant, harmless recompute of the whole skeleton, not a correctness fix.
The header is explicit about the distinction
(`Zenith_AnimationPreviewSession.h:248-250`): *"Tick() and Seek() do NOT need it … The
hazard is the DIRECT writes (the drag)."* The rule as shipped is narrower than the
paragraph above implies: `RefreshDerivedPose()` exists for, and is required after, the
one write path that bypasses the controller — `Flux_SkeletonInstance::SetBoneLocalTransform`
called directly, which is exactly what the drag path (`UpdateBoneDrag`, §4.2) and the IK
bake (§6) do. `UpdateBoneDrag` calls it every drag frame for precisely that reason.

### 3.3 The conversions

```cpp
// Zenith/Editor/Animation/Zenith_BoneSpace.h                  (WU-4.2)
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

**Confirmed against the shipped header, verbatim.** `Zenith/Editor/Animation/Zenith_BoneSpace.h`
(WU-4.2, `3acce596`) declares these seven functions with these exact signatures — the
design brief's guesses here were right. One implementation note the brief did not
anticipate: `ApplyWorldDeltaToBoneLocal`'s result is deliberately **not** re-normalised,
because a drag latches the bone's rotation once at `BeginBoneDrag` and applies one fresh
delta to that latched value every frame rather than accumulating output back into input,
so there is no drift to correct for.

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
translation column of `W · M_i`.

**Corrected after implementation (2026-09-06): that parenthetical had it backwards.**
This note originally called `M_i`'s translation "the joint the bone's rotation *moves*,
not the joint it *rotates about*" and picked it anyway on convenience grounds ("both are
defensible… it is where the user sees the handle"). §2's derivation makes the actual
relationship explicit: with `L_i = T(p) · R(q) · S`, `translation(M_i) = M_parent(i) · p`,
which does **not depend on `q_i`** — it is the FIXED POINT of bone `i`'s own rotation,
i.e. exactly the joint the bone turns *about*. (The joints `q_i` moves belong to its
*children* — see §2.) So the shipped comment in `Zenith_BoneSpace.h:90-95` states the
correct reason for the same choice: `M_i`'s translation is used for the ring pivot
**because** it is the fixed point the rotation happens around, which is also where the
user's cursor naturally lands, not as a convenience trade-off between two equally
defensible options. The decision (use `M_i`) stands; the justification in this note was
wrong and is corrected here rather than silently rewritten.

**Non-uniform bone scale.** `L_i`'s 3×3 is `R·S` (`Flux_SkeletonInstance.cpp:236-240`),
so a quaternion recovered from `P` is only the true rotation when the accumulated scale
is uniform and positive. `Zenith_Maths::DecomposeTRS` is documented for exactly that
class of matrix (`Zenith/Maths/Zenith_Maths.h:86-95`) and normalises the result. Bind
scales are 1 in every rig in the tree. **Confirmed as shipped:**
`Zenith_BoneSpace::ParentWorldRotation` asserts uniformity and handedness on the
extracted scale rather than assuming it, exactly as this note recommended (§9 item 5,
resolved).

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

**Corrected after implementation (2026-09-06): no `Zenith_UndoCommand_AnimPoseKeys` was
built, and none was needed.** This section designed a bespoke undo command carrying its
own document pointer, its own lifetime obligation and its own track-kind enum. WU-3.3
had already solved the identical problem for the dope sheet's own multi-key operations
by the time WU-4.3 landed (`Zenith_AnimationDocument::BeginCompound` /
`EndCompound`, §4 of `Zenith/Editor/CLAUDE.md`), and pose authoring reuses it verbatim
rather than inventing a parallel mechanism:

- **The document's own verbs are the writer.** `Zenith_AnimationDocument::InsertKey`
  (rotation or vector overload) and `SetKeyValue` are what `Action_SetKeyForBones`
  calls — the same stable-id, mark-dirty, push-undo verbs every other document mutation
  goes through (§0's premise table). There is no separate `Zenith_AnimKeyValue` /
  `Zenith_AnimTrackKind` pair in the pose-authoring code: the document already has
  `Zenith_AnimKeyValue` (`Zenith_AnimationDocument.h:97-105`, a `Vector3` + `Quat` +
  `m_bIsRotation` tag) and the existing `Flux_AnimTrack` enum
  (`FLUX_ANIM_TRACK_POSITION` / `_ROTATION` / `_SCALE`,
  `Flux/MeshAnimation/Flux_AnimationClip.h`) — there is no `Zenith_AnimTrackKind`
  anywhere in the tree.
- **The grouping primitive is `BeginCompound()` / `EndCompound()`.** One drag, one Set
  Key press, or one IK bake brackets however many `InsertKey`/`SetKeyValue` calls it
  makes inside one compound, and `EndCompound(szDescription)` pushes it as a single
  `Zenith_AnimCommand_Compound` (`Zenith/Editor/Zenith_EditorAnimCommands.h:233-246`) —
  or, with `bKeep = false`, unwinds everything it collected when a mutation refuses
  partway through, so a rejected multi-bone write never reaches the stack half-applied.
  `Zenith_AnimCommand_Compound` is built by the document itself, not by a caller: while
  a compound is open, every command the ordinary verbs would have `Record`ed is
  *adopted* into it instead, so a bone-drag call site calls the same public verbs it
  always would and does not know it is being grouped.
- **The lifetime obligation this note flagged is resolved, and resolved the way this
  note hoped it would be (§9 item 3).** `Zenith_AnimationDocument` owns its **own**
  undo stack (`Zenith_UndoSystem m_xUndoSystem`, a member, not the shared editor one),
  and every command it pushes holds a raw `Zenith_AnimationDocument*` back at that same
  object. `Close()` refuses while dirty and otherwise clears the stack
  (`Zenith_AnimationDocument.h:178-182`), and the destructor clears it unconditionally
  (`Zenith_AnimationDocument.cpp:250-...`) — so there is no window in which a live
  command outlives its target, and no separate Phase-3 contract needed writing: the
  document *is* the thing that owns open/close, and it already owned its own undo
  lifetime before pose authoring needed one.

None of this needed a `Record()` call the way the brief's bespoke command did — the
existing verbs already `Record` (or get adopted into a compound) as part of what they
do, which is the whole point of reusing them rather than parallelling them.

### 4.4 What a discarded drag looks like

With auto-key **off**, a released drag writes no key and creates no undo entry. The pose
is live-but-unkeyed and is destroyed by the next seek, because the session re-evaluates
from the clip. That is the one thing a user can silently lose, so it is not allowed to be
silent: `HasUnkeyedPose()` must be surfaced in the panel (a badge next to the time
field), and seeking while it is true must be an explicit, visible discard. Making the
drag itself undoable was rejected — an undo stack entry that restores a pose the document
never contained is a lie about what was saved.

**Confirmed as shipped, plus one gesture this note did not specify.**
`Zenith_EditorPanel_Animation::Action_EndBoneDrag` writes the key only when auto-key is
on AND the drag actually moved (`IsBonePoseDragActive`/`GetPoseDragAngleRadians`
distinguish "never grabbed" from "grabbed and produced nothing", matching the
diagnostics pattern the rest of the panel uses). The design brief did not call out a
cancel path; the shipped one is `Action_CancelBoneDrag()` — Escape puts the bone back
where the drag found it and ends the drag, and because nothing reached the document
during the drag there is nothing to undo.

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

**Confirmed as shipped, with the root-translation path as its own verb rather than a
flag.** `Zenith_EditorPanel_Animation::Action_SetKeyForBones(xBoneIndices, bRotation,
bTranslationForRoot)` carries `bTranslationForRoot` as a parameter, but the panel's
public surface for an author to actually reach it is a **separate** function,
`Action_SetKeyTranslationForRoot()` — refused unless the selected bone is a root — so
writing a translation key is a decision an author makes explicitly rather than a flag
that rides along with an ordinary rotation edit. Scale is not authorable anywhere in the
Phase-4 surface, exactly as designed.

### 5.2 At what time — and the unit hazard

```
fSnappedSeconds = roundf(fCurrentSeconds * fFrameRate) / fFrameRate
```

`fFrameRate` is the document's authored frame rate; when it is 0, do not snap and log
once. Snapping is **unconditional**, not a toggle: unsnapped keys land at arbitrary float
times, which makes the dope sheet's frame columns lie and makes D11's collision
behaviour unpredictable.

**Corrected after implementation (2026-09-06): the tree this note found had already been
fixed by the time Phase 4 needed it, and the "should not exist" recommendation below was
overtaken by a distinction this note did not consider.** `Zenith/Flux/MeshAnimation/CLAUDE.md`
now states plainly: *"KEY TIMES ARE SECONDS, ON THE SAME CLOCK AS `m_fDuration` (D3)"* —
every `Add*Keyframe` / `InsertKeyframeAt` argument and every `Sample*()` argument is
seconds, and neither of `Flux_BonePose.cpp`'s two `SampleFromClip` overloads converts at
all any more. The tick-based storage this note found (`fTimeInTicks = fTime *
xClip.GetTicksPerSecond()`) was the *old* behaviour and has been replaced; there is no
`Zenith_AnimSecondsToChannelTime` / `Zenith_AnimChannelTimeToSeconds` pair anywhere in
the tree because pose authoring needs no conversion — `Zenith_AnimationDocument::InsertKey`
/ `SetKeyTime` / the panel's Set Key path all take and store seconds directly, the same
unit the playhead and the dope-sheet ruler already used.

**`m_uAuthoredFrameRate` DOES exist, and it is not the duplicated pin this note warned
against — it is a second number with a genuinely different job from `m_uTicksPerSecond`.**
`Flux_AnimationClipMetadata` carries both, and they answer different questions:

| Field | Meaning |
|---|---|
| `m_uTicksPerSecond` | **import provenance only** — the tick rate of the file the clip was imported from. Never applied to a key time (the old conversion this note quoted is gone) |
| `m_uAuthoredFrameRate` (D6) | **editorial intent** — the fps the clip was authored at: what the key grid snaps to (§5.2's `fSnappedSeconds` formula uses exactly this), and what a re-bake should resample to |

So the recommendation this note made — "the authored frame rate IS `m_uTicksPerSecond`"
— would have been wrong to adopt: a clip imported at 30 ticks/second that an author
wants to key on a 24 fps grid needs both numbers, distinctly, at once. §9 items 1 and 2
are resolved by this split, not by collapsing one field into the other.

### 5.3 Duplicate times

D11 applies: `InsertKeyframeAt` on an occupied time **replaces** the key, preserving its
identity. So Set Key at an already-keyed frame is a value edit rather than a
remove-and-reinsert, and Undo restores the previous value rather than removing the key.

**Corrected after implementation (2026-09-06):** this is the document's existing D11
contract (`Zenith_AnimationDocument::InsertKey`, §0 of `Zenith/Editor/CLAUDE.md`'s
`Zenith_AnimationDocument` entry), not fields on a bespoke command — there is no
`m_bExistedBefore` / `m_xOldValue` pair in the pose-authoring code (see §4.3's
correction). The behaviour described is otherwise accurate: `InsertKey` on an occupied
time is a value edit under the hood.

**`SetKeyframeTime` must never appear on this path.** By D11 it *fails*, returning false,
when the destination is occupied. It is the dope sheet's key-drag verb. Pose authoring
only ever inserts-or-replaces at the playhead, where failing is not an acceptable outcome
and silently doing nothing is worse.

Two keys are "the same time" when `fabsf(a - b) <= fANIM_TIME_EPSILON` (`1e-5f`,
confirmed present — §0). Note that after §5.2's snapping the comparison is against a
grid, so the epsilon only absorbs float error in `roundf(x * r) / r`, not genuine
near-misses.

### 5.4 One writing path

**Corrected after implementation (2026-09-06):** the real signatures live on
`Zenith_EditorPanel_Animation` (not `static`, and not carrying a `Zenith_AnimTrackKind`
that does not exist — see §4.3):

```cpp
// Zenith/Editor/Panels/Zenith_EditorPanel_Animation.h

	// THE key-writing verb (design note §5.4). Every caller — the Set Key button,
	// auto-key on drag release, and the IK bake — goes through this one function.
	// bRotation writes the rotation track; bTranslationForRoot additionally writes
	// translation, and ONLY for a bone with no parent. Scale is not authorable.
	bool Action_SetKeyForBones(const Zenith_Vector<u_int>& xBoneIndices, bool bRotation, bool bTranslationForRoot);
	bool Action_SetKeyForSelectedBone();

	// Its own verb rather than a flag on the one above (design note §5.1's correction).
	bool Action_SetKeyTranslationForRoot();

	bool Action_SetAutoKey(bool bEnabled);
	bool Action_GetAutoKey() const;
```

Auto-key state lives on the **session** (`bool m_bAutoKey = false;`), not on the document
— it is a per-editing-session preference, not clip content, confirmed as designed
(`Zenith_AnimationPreviewSession::SetAutoKey` / `GetAutoKey`). On `Action_EndBoneDrag`, if
auto-key is on and the drag actually moved, the panel writes the key through
`Action_SetKeyForBones` inside the same compound the drag itself opened, so the key and
the drag's document-side bookkeeping land as **one** undo step created **on release** —
not two, and not one pushed per drag frame (§4.1's rule holds unchanged).

Auto-key writes **one key, at the playhead**. Rejected: Maya-style bracketing, which
inserts an extra key at the previous keyed time to "hold" the earlier pose. It silently
doubles the authored data and produces keys the user did not ask for — and with §5.1's
first-key rule, a bracketing key on an empty channel changes the whole clip twice over.

### 5.5 Automation

Mirroring the graph verbs (`Editor/CLAUDE.md`, *Graph Authoring via Editor Automation*),
each atomic action gets a step so a pose can be authored at boot and diffed.

**Corrected after implementation (2026-09-06): the real block is five steps, not five
different ones than guessed, and free functions rather than statics.** As shipped
(`Zenith/Editor/Zenith_EditorAutomation.h`, the `ANIM_POSE_*` enum block):

```cpp
// Zenith/Editor/Zenith_EditorAutomation.h
void AddStep_AnimSelectBone(int iBoneIndex);
// (fAxisX, fAxisY, fAxisZ) must be a CARDINAL axis — (1,0,0)/(0,1,0)/(0,0,1) — and the
// angle is in DEGREES, converted with Zenith_Maths::AuthoringRadians (see below).
void AddStep_AnimRotateSelectedBoneWorld(float fAxisX, float fAxisY, float fAxisZ, float fAngleDegrees);
void AddStep_AnimSetKeyForSelectedBone();
void AddStep_AnimSetAutoKey(bool bEnabled);
// An ASSERTION step, not a mutator: (fX, fY, fZ, fW) in SERIALIZED order.
void AddStep_AnimExpectBoneLocalRotation(int iBoneIndex, float fX, float fY, float fZ, float fW, float fTolerance);
```

Two differences from what this note guessed, both load-bearing:

- There is **no `AddStep_AnimRotateBoneLocal(quat)`.** The shipped rotate step takes an
  axis + degrees, not a raw quaternion, and — unlike the dope-sheet's own
  `AddStep_AnimRotateBoneLocal` this note imagined — **the axis is refused unless it is
  exactly cardinal**. That is the FP-determinism rule from `Editor/CLAUDE.md`'s
  *AUTHORED ROTATIONS THAT LAND IN A COMMITTED SCENE*, applied here for the first time to
  a bone rather than an entity transform: `glm::angleAxis` is a header inline whose
  floating-point model is fixed at its own definition point, so a Debug and a Release
  tools build can disagree in the last bit or two on a non-cardinal axis, and an authored
  pose that reached a tracked `.zanim` would ping-pong in `git status` forever under
  every tolerance-based guard. `Zenith_Maths::AuthoringRotationX/Y/Z` are the pinned,
  non-inline replacements, and they only cover the three cardinal axes — so the executor
  refuses anything else rather than silently computing it the other way.
- **There is no `AddStep_AnimBakeIK`.** WU-4.4 shipped `Action_BakeIKForSelectedChain`
  (§6) but no automation step wraps it and no target-selection widget aims it yet — see
  §6's correction and §7's "as built" table. A pose can be authored and asserted at boot
  through the five steps above; an IK bake cannot, today, be driven the same way.

**★ These land in a CONTIGUOUS enum block** — confirmed, `ANIM_POSE_SELECT_BONE` ..
`ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION`, five wide, pinned by
`static_assert(... == 4, "the ANIM_POSE block must stay CONTIGUOUS and five wide")` —
and every block in `Zenith_EditorAutomation` carries a "must stay CONTIGUOUS" comment
naming its first and last member, because `ExecuteAction` routes by `>=` / `<=` range
comparison and an action inserted mid-block silently routes to the wrong sub-executor
(`Editor/CLAUDE.md`, *The split dispatcher: twelve contiguous ranges*). Follow that
pattern exactly; it is not optional.

**Authored rotations that reach a committed asset must not use glm — confirmed, and
solved differently from how this note guessed.** This note originally assumed the
rotate step would take a raw quaternion and pass it through verbatim (the
`AddStep_SetTransformRotationQuat` pattern). The shipped rotate step instead takes an
axis + angle and computes the rotation itself via the pinned `Zenith_Maths::Authoring*`
helpers, so the "pass verbatim" escape hatch was not needed here — the cardinal-axis
restriction described above is what keeps it deterministic instead. The
`(fX, fY, fZ, fW)` serialized-order convention this note describes for a verbatim
pass-through survives only on the READ side, in the assertion step
`AddStep_AnimExpectBoneLocalRotation`.

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

**Corrected after implementation (2026-09-06): the shipped surface is index-based, not
`Flux_IKChain`-by-value, and it is two functions plus a chain builder, not one.** The
`std::pair<u_int, Quat>` output this note guessed does not exist either — the output is
a plain `Zenith_Vector<Quat>` addressed positionally against the request's own index
list. As built (`Zenith/Editor/Animation/Zenith_AnimationPoseIK.h`, WU-4.4):

```cpp
// Zenith/Editor/Animation/Zenith_AnimationPoseIK.h            (WU-4.4)

namespace Zenith_AnimationPoseIK
{
	inline constexpr u_int kuIK_DEFAULT_CHAIN_LENGTH = 3u;   // §6.4 — a scope choice, not a solver limit
	inline constexpr u_int kuIK_MIN_CHAIN_LENGTH = 2u;
	inline constexpr u_int kuIK_DEFAULT_MAX_ITERATIONS = 10u;
	inline constexpr float kfIK_DEFAULT_TOLERANCE = 0.001f;
	inline constexpr float kfIK_MIN_CHAIN_LENGTH_METRES = 1.0e-5f;
	// Aliased to the channel's OWN threshold (fANIM_MIN_QUAT_LENGTH), not copied —
	// so this module can never hand out a rotation the writer would then refuse.
	inline constexpr float kfIK_MIN_QUAT_LENGTH = fANIM_MIN_QUAT_LENGTH;

	struct SolveRequest
	{
		// Root -> effector, BONE INDICES (not a Flux_IKChain by value — every caller
		// already has an index, and a name round-trip is exactly where a rig with two
		// identically-named bones would silently retarget the solve).
		Zenith_Vector<u_int> m_auChainBoneIndices;
		Zenith_Maths::Vector3 m_xTargetModelSpace = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xPoleDirectionModelSpace = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		bool m_bUsePoleDirection = false;
		Zenith_Vector<Flux_JointConstraint> m_axJointConstraints;   // empty == unconstrained
		u_int m_uMaxIterations = kuIK_DEFAULT_MAX_ITERATIONS;
		float m_fTolerance = kfIK_DEFAULT_TOLERANCE;
		float m_fWeight = 1.0f;
	};

	// The chain Action_BakeIKForSelectedChain uses: the effector and up to
	// (uMaxChainLength - 1) of its ancestors, ROOT FIRST. Refused when the effector
	// does not resolve, uMaxChainLength < kuIK_MIN_CHAIN_LENGTH, or the effector IS a root.
	bool BuildChainFromEffector(const Zenith_SkeletonAsset& xSkeleton,
		u_int uEffectorBoneIndex, u_int uMaxChainLength,
		Zenith_Vector<u_int>& auOutChainBoneIndices);

	// Seeds a scratch pose from the instance's CURRENT local TRS, resolves + measures a
	// TRANSIENT Flux_IKChain built from the request's indices, composes model space,
	// solves ONE chain, recomposes, and writes back the solved BONE-LOCAL rotations, one
	// per chain bone, IN CHAIN ORDER (entry i belongs to m_auChainBoneIndices[i]).
	// PURE with respect to engine state: touches no controller, no live scene, no GPU.
	bool SolveChainToLocalRotations(const Flux_SkeletonInstance& xInstance,
		const Zenith_SkeletonAsset& xSkeleton,
		const SolveRequest& xRequest,
		Zenith_Vector<Zenith_Maths::Quat>& axOutLocalRotations);

	// The FALLBACK bake path (§6.3) — writes one rotation key per chain bone directly
	// through the document, as one compound. Exists because WU-4.3's
	// Action_SetKeyForBones lands separately; a panel with 4.3 landed never reaches it.
	bool BakeChain(Zenith_AnimationDocument& xDocument, const Zenith_SkeletonAsset& xSkeleton,
		const Zenith_Vector<u_int>& auChainBoneIndices,
		const Zenith_Vector<Zenith_Maths::Quat>& axLocalRotations,
		float fTimeSeconds, u_int uFrameRate);
}
```

Body, in order (`Zenith_AnimationPoseIK.cpp`) — the shipped function validates every
precondition explicitly before doing any work, which the design note did not spell out
as its own step:

0. **Validate everything the solver would otherwise consume silently** — `Flux_IKSolver::
   SolveChain` has no error channel, so a bad chain length, an out-of-range or
   duplicate index, a non-finite target/weight/tolerance, a zero iteration count, or a
   mismatched constraint-list length are all refused here, before any state is touched.
1. `Flux_SkeletonPose xScratch; xScratch.Initialize(uUsableBones);` — the STACK-LOCAL
   scratch pose, sized to `min(instance bone count, asset bone count, FLUX_MAX_BONES)`.
2. Seed it from the **instance's current local TRS** (`GetBoneLocalPosition/Rotation/Scale`),
   validating every seeded bone is finite/usable before the solve ever runs — a broken
   input pose is refused rather than laundered into a NaN key.
3. Build the **transient** `Flux_IKChain` from bone NAMES resolved off the request's
   indices, then call `ResolveBoneIndices` and **check the round trip**: the indices
   that come back must equal the ones that went in, which is what catches a rig
   carrying two identically-named bones.
4. `ComputeModelSpaceMatricesFromSkeleton`, then (5) `ComputeBoneLengths` — order
   forced, since the length measurement reads model-space translations the compose
   just produced.
6. Build the `Flux_IKTarget`: `m_bIsModelSpace = true` (there is no world matrix in this
   API at all), `m_bUseRotation = false` — an unrequested end-effector twist is a pose
   change the author did not ask for and could not see the cause of.
7. `Flux_IKSolver xSolver; xSolver.SolveChain(xScratch, xChain, xTarget, xSkeleton);` —
   our solver, our pose, never the controller's.
8. Recompose model space again, mirroring the controller's own pre/post recompute
   (`Flux_AnimationController.cpp`), whose comment explains the post-solve recompute is
   what keeps model matrices consistent for downstream CPU readers.
9. Extract each chain bone's solved local rotation, validate every one, and only THEN
   copy them all into the caller's output vector — nothing reaches the caller until
   every rotation in the chain has passed.

### 6.3 The bake

`Zenith_EditorPanel_Animation::Action_BakeIKForSelectedChain` calls
`Action_SetKeyForBones(auChainBones, /*bRotation*/true, /*bTranslationForRoot*/false)` —
**the same function a hand drag uses** (§5.4, itself built on the document's `InsertKey`
/ `BeginCompound` / `EndCompound`, not a bespoke command — see §4.3's correction). So one
compound covers the whole chain, one Ctrl+Z undoes the whole IK gesture, and the clip
contains nothing IK-specific. That is what "baked down to keys" has to mean: after the
fact, an IK-posed frame is indistinguishable from a hand-posed one, and nothing in the
`.zanim` needs a solver to play back. Only when that call refuses (because WU-4.3 has
not landed in the running binary) does the panel fall back to `Zenith_AnimationPoseIK::
BakeChain`, which writes through the identical document verbs directly.

The live pose is applied the same way as a drag — direct `SetBoneLocalTransform` writes
plus `RefreshDerivedPose()` — so the IK path also honours §4.4: solve without auto-key,
and the pose is visible but unkeyed until Set Key.

**★ Confirmed and worth flagging: there is no target-selection widget yet.** WU-4.4
shipped the whole verb chain — `BuildChainFromEffector` → `SolveChainToLocalRotations` →
`Action_SetKeyForBones`/`BakeChain` — and `Action_BakeIKForSelectedChain` is fully
callable and unit-tested end to end, but nothing in the preview pane lets a user AIM it:
the `.cpp`'s own header comment says so — *"There is deliberately NO IK section drawn in
the preview pane here … The verb is complete and callable; the widget that aims it is a
follow-up."* §5.5 confirms there is no automation step for it either.

### 6.4 Chain scope for Phase 4

**Corrected after implementation (2026-09-06):** the shipped chain builder
(`BuildChainFromEffector`) walks straight up the asset's own parent pointers from the
selected bone and stops at `kuIK_DEFAULT_CHAIN_LENGTH` (3) bones or a root, whichever
comes first, then reverses the walk to root-first order. It does **not** call
`Flux_IKSolver::CreateArmChain` / `CreateLegChain` — neither is referenced anywhere
under `Zenith/Editor/` — because the by-name matching those helpers do is exactly the
kind of round trip §6.2 point 3 exists to avoid. **No chain-authoring UI, no chain
serialization, no constraints, no pole vector** — confirmed: `SolveRequest`'s
`m_axJointConstraints` and `m_bUsePoleDirection`/`m_xPoleDirectionModelSpace` exist on
the struct (so the solver-level plumbing is there) but nothing in the editor populates
them yet. The three-bone limit is a scope choice, not a solver limit — FABRIK handles
any length, and `kuIK_DEFAULT_CHAIN_LENGTH` is a parameter of `BuildChainFromEffector`,
not a constant baked into the solve.

---

## 7. As built (WU-4.1 … WU-4.4)

**Corrected after implementation (2026-09-06): this section was a forward-looking
fill-in plan; it is replaced here with what actually shipped**, since the plan's file
list, "Implements" lines and split rationale are what §2–§6's corrections above found
wrong in several places (`Zenith_AnimationPoseCommands.*` and
`Zenith_UndoCommand_AnimPoseKeys` were never built; the automation verbs and the IK
request shape both differ from what was planned). One location correction that applies
across the whole table: **the session lives at `Zenith/Editor/Zenith_AnimationPreviewSession.{h,cpp}`**,
not under `Editor/Animation/` as this note assumed throughout — only the newer,
pose-authoring-specific files landed in `Editor/Animation/`.

| Unit | Commit | Files | What it landed |
|---|---|---|---|
| WU-4.1 | `36e1c4c4` | `Zenith/Editor/Animation/Zenith_BonePickGeometry.{h,cpp,Tests.inl}`; additions to `Zenith/Editor/Zenith_AnimationPreviewSession.{h,cpp}` (selection, hover, `RefreshDerivedPose`, the pick-set cache, the drag primitives §4.2 depends on); the full `Action_*` declaration surface on `Zenith/Editor/Panels/Zenith_EditorPanel_Animation.h` (stub bodies for what 4.3/4.4 fill); `RayIntersectsSphere` added to `Zenith/Maths/Zenith_Maths_Intersections.h` (inline free function, not a class member) | Selectable bone target + hit geometry (§2, with the ownership rule corrected from this note's original — see §2) |
| WU-4.2 | `3acce596` | `Zenith/Editor/Animation/Zenith_BoneSpace.{h,cpp,Tests.inl}` | The seven space-conversion functions (§3.3) — signatures matched the design brief exactly |
| WU-4.3 | `f69e4c1d` | `Zenith/Editor/Panels/Zenith_EditorPanel_Animation_Pose.cpp` (manipulator + drag + Set Key/auto-key bodies); the five-step `ANIM_POSE_*` block in `Zenith/Editor/Zenith_EditorAutomation.{h,cpp}` (§5.5) | The manipulator, drag transactions, Set Key and auto-key — reusing the document's OWN `BeginCompound`/`EndCompound` + `Zenith_AnimCommand_Compound` (`Zenith_EditorAnimCommands.h`) rather than the bespoke `Zenith_UndoCommand_AnimPoseKeys` this note designed (§4.3) |
| WU-4.4 | `da697fce` | `Zenith/Editor/Animation/Zenith_AnimationPoseIK.{h,cpp,Tests.inl}`; `Zenith/Editor/Panels/Zenith_EditorPanel_Animation_IK.cpp` | IK-assisted posing baked to keys (§6) — an index-based `SolveRequest` + a separate `BuildChainFromEffector`, not the `Flux_IKChain`-by-value shape this note designed; `Action_BakeIKForSelectedChain` is complete and unit-tested but has **no target-selection widget and no automation step** yet |

None of the file-list or "sole writer" plumbing this section used to carry (which file
each unit "Owns", the shared-write-conflict avoidance) is worth keeping once the work is
done — it was planning detail for coordinating four parallel subagents, not a fact about
the shipped system. What is worth keeping is the same "must pin with tests" intent this
section had; the actual coverage lives in each unit's own `.Tests.inl` (named above)
rather than being re-derived here.

---

## 8. RE-PLAN REQUIRED

Two things in the four-way split do not survive contact with the tree. Neither is a
scoping failure of the work; both are ownership/feasibility corrections.

**Both corrections below are ADOPTED, confirmed by the shipped code (2026-09-06).**

### 8.1 WU-4.2's "gizmo drive" is not deliverable as written — ADOPTED

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

**Confirmed as shipped.** `Zenith_EditorPanel_Animation.h`'s own header says it in the
same words this correction predicted: *"IT IS AN ImGui DRAW-LIST OVERLAY ON THE PREVIEW
IMAGE, NOT Flux_Gizmos … Flux_GizmosImpl is entity-typed all the way down AND declares
one pass writing the FINAL render target with no per-view selection."* The rings, the
hit-test and the drag all live in `Zenith_EditorPanel_Animation_Pose.cpp` (WU-4.3), and
`Zenith_BoneSpace` (WU-4.2) is exactly the three files / seven functions / no-UI shape
this correction called for (§3.3, §7).

### 8.2 Three units would otherwise write the same two files — ADOPTED

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

**Confirmed as shipped.** `Zenith_EditorPanel_Animation.h` carries the complete
`Action_*` surface — including the pose-authoring block — declared in one place with a
comment naming exactly this reasoning: *"THE WHOLE Action_* SURFACE IS DECLARED HERE BY
WU-4.1, INCLUDING THE PARTS IT DOES NOT IMPLEMENT … which is what turns what would have
been a three-way write conflict on one file into a one-way dependency."* WU-4.3's body
lives in `Zenith_EditorPanel_Animation_Pose.cpp` and WU-4.4's in
`Zenith_EditorPanel_Animation_IK.cpp`, each filling declared stubs rather than adding
declarations — and the IK unit's own commentary confirms the "compiles without 4.3"
half of the sequencing worked as planned: `Action_BakeIKForSelectedChain` calls
`Action_SetKeyForBones` and falls back to `Zenith_AnimationPoseIK::BakeChain` when that
call refuses, keyed on the RETURN VALUE rather than a build flag, precisely so the
fallback is safe whether or not 4.3 has landed in the binary being built.

---

## 9. Open questions the briefs must carry — resolutions (2026-09-06)

1. **Seconds or ticks?** — **RESOLVED: seconds, everywhere, unconditionally.**
   `Zenith/Flux/MeshAnimation/CLAUDE.md`: *"KEY TIMES ARE SECONDS, ON THE SAME CLOCK AS
   `m_fDuration` (D3)."* The tick-based storage this note found when it was written has
   been replaced; neither `SampleFromClip` overload converts any more, and pose
   authoring needed no seconds↔ticks conversion helper of its own because there is
   nothing left to convert (§5.2's correction).
2. **`m_uAuthoredFrameRate` vs `m_uTicksPerSecond`.** — **RESOLVED, but not by
   recommending one:** both exist, and they mean different things.
   `m_uTicksPerSecond` is import provenance (the source file's tick rate, applied to
   nothing); `m_uAuthoredFrameRate` (D6) is editorial intent — the fps the pose-authoring
   grid snaps to. This note's original recommendation ("the authored frame rate IS
   `m_uTicksPerSecond`") would have been the wrong call; see §5.2's correction for why
   the two numbers need to stay distinct.
3. **Who clears the undo stack when a document closes?** — **RESOLVED: the document
   itself, and it always did.** There is no bespoke pose-authoring command holding a raw
   document pointer (§4.3's correction) — `Zenith_AnimationDocument` owns its own
   `Zenith_UndoSystem` as a member, every command it pushes already points back at that
   same object, `Close()` clears it (refusing first while dirty) and the destructor
   clears it unconditionally. No separate Phase-3 contract needed to be written down;
   the ownership this question worried about not existing had already been decided by
   the time Phase 4 needed it.
4. **The preview view slot has an incumbent.** — **RESOLVED: `Flux_PreviewSlotArbiter`**
   (`Zenith/Flux/RenderViews/Flux_PreviewSlotArbiter.h`), a last-opened-wins arbiter
   living in `Flux/RenderViews` (not `Editor/`, because Flux may not include Editor and
   both claimants — `Flux_MaterialPreviewController` and
   `Zenith_AnimationPreviewSession` — sit on opposite sides of that boundary). It is
   owner-agnostic (a `void*` identity plus a display name, never dereferenced), claims on
   a transition rather than per frame (so a re-claim-every-visible-frame panel could
   never be dispossessed), and a dispossessed claimant is told WHO holds the slot so its
   panel can offer a reclaim. `Flux_MaterialPreviewController` participates with the same
   `Claim`/`Release`/`HasSlot` calls the animation session uses
   (`Flux_MaterialPreviewController.h:174,179,189-192`) — confirming both sides of the
   arbitration this note asked for actually exist, not just the animation side.
5. **Non-uniform bone scale.** — **RESOLVED: assert, as recommended.**
   `Zenith_BoneSpace::ParentWorldRotation` asserts uniform, positive-determinant scale on
   the extracted matrix rather than assuming it (confirmed in the shipped header's own
   comment, §3.4's correction) — a sheared parent frame is refused loudly rather than
   producing a delta that is subtly wrong in a way no gate could see.
