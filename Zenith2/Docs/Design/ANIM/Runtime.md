# ANIM design: runtime, systems and extraction

Section file of `Zenith2/Docs/Design/ANIM.md`. It declares the public headers of the asset objects, poses, the world resources, commands, events, IK, retargeting and the preview, and specifies the kind loaders, animator instances and their asset bindings, the frame-graph systems of phases 3, 6 and 12, the lifecycle listener, ragdoll transitions, inverse kinematics, motion warping, retargeting, facial animation, significance-based update-rate budgeting, clip streaming, render extraction, save state, the Luau bindings and the memory consumer. The graph semantics it runs are `Zenith2/Docs/Design/ANIM/Graph.md`; the bytes it reads are `Zenith2/Docs/Design/ANIM/Formats.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| asset object APIs | "Zenith_SkeletonAsset header", "Zenith_AnimationClip header", "Zenith_AnimationGraphAsset header" |
| poses | "Zenith_AnimationPose header" |
| world resources | "Zenith_AnimationWorld header" |
| commands and events | "Zenith_AnimationCommands header", "Zenith_AnimationEvents header" |
| solvers and retargeting | "Zenith_AnimationIK header", "Zenith_AnimationRetarget header" |
| the motion matcher | "Zenith_AnimationMotionMatch header" |
| editor and test evaluation | "Zenith_AnimationPreview header" |
| loading | "Asset kind loaders", "Asset bindings" |
| instances | "Animator instances" |
| frame work | "Systems", "Lifecycle listener", "Render extraction" |
| features | "Ragdoll transitions", "Inverse kinematics", "Motion warping", "Retargeting", "Facial animation", "Update-rate budgeting", "Clip streaming" |
| persistence, scripts, memory | "Save state", "Luau bindings", "Memory consumer" |

## Zenith_SkeletonAsset header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_SkeletonAsset.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAnimation/Zenith_AnimationTypes.h"

/// @contract value Zenith_SkeletonChain
/// Ownership: Plain value inside a Zenith_SkeletonAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Joint indices.
/// Errors: None; validated at load.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SkeletonChain
{
	uint64 m_ulNameHash = 0;
	uint16 m_uStartJoint = 0;
	uint16 m_uEndJoint = 0;
	Zenith_ChainRole m_eRole = CHAIN_ROLE_CUSTOM;
	bool m_bRetargetByIK = false;
};

/// @contract value Zenith_SkeletonIKDefinition
/// Ownership: Plain value inside a Zenith_SkeletonAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Joint indices; m_xHint a model-space direction (TWO_BONE, LOOK_AT) or (0, ankle height in metres, 0) (FOOT).
/// Errors: None; validated at load (Formats.md "Skeleton validation", SV6).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_SkeletonIKDefinition
{
	uint64 m_ulNameHash = 0;
	Zenith_IKKind m_eKind = IK_KIND_TWO_BONE;
	uint8 m_uJointCount = 0;
	uint16 m_auJoints[4] = {};
	Zenith_Vector3 m_xHint = xVECTOR3_ZERO;
};

/// @contract value Zenith_SkeletonSocket
/// Ownership: Plain value inside a Zenith_SkeletonAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: m_xOffset in the joint's space (metres, unit quaternion, factors).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SkeletonSocket
{
	uint64 m_ulNameHash = 0;
	uint16 m_uJoint = 0;
	Zenith_Transform m_xOffset;
};

/// @contract value Zenith_SkeletonCurve
/// Ownership: Plain value inside a Zenith_SkeletonAsset.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Curve units (morph weights in [-1, 1]; others the rig's).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SkeletonCurve
{
	uint64 m_ulNameHash = 0;
	Zenith_CurveKind m_eKind = CURVE_KIND_CONTROL;
	bool m_bAdditive = false;
	float m_fDefault = 0.0f;
};

/// @contract type Zenith_SkeletonAsset
/// Ownership: Created by the Animation.Skeleton kind loader (ZenithAnimation/Internal/Assets/) from a .zskel and shared
///   through Zenith_AssetHandle<Zenith_SkeletonAsset>; destroyed by the loader's Unload when the last reference drops.
///   Its arrays are one general-heap block.
/// Thread affinity: ANY_THREAD. Immutable once activated.
/// Invalidation: Spans and pointers it returns are valid while a handle to it is held.
/// Units: Metres, unit quaternions, scale factors; joint indices 0 … GetJointCount() − 1.
/// Errors: Out-of-range joint, curve or socket indices are ZENITH_ASSERT (engine code only computes them from the asset);
///   lookups by name hash return uANIM_INVALID_INDEX or null when absent.
/// Version: since Z2 Stage 3. Cooked format ZSKL version 1; re-cooked on any engine version change ([Z2-N-CONTRACT-529]).
/// @endcontract
class Zenith_SkeletonAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SKELETON;

	uint32 GetJointCount() const;
	uint32 GetCurveCount() const;
	Zenith_Span<const uint16> GetParents() const;               // 0xFFFF for joint 0
	Zenith_Span<const Zenith_Transform> GetRestLocal() const;
	Zenith_Span<const Zenith_Matrix4> GetRestModel() const;      // recomputed at load in f32
	Zenith_Span<const Zenith_Matrix4> GetRestModelInverse() const;
	uint32 FindJoint(uint64 ulNameHash) const;                // O(log J)
	uint64 GetJointNameHash(uint32 uJoint) const;
	Zenith_StringView GetJointName(uint32 uJoint) const;
	uint32 GetJointForRole(Zenith_JointRole eRole) const;      // uANIM_INVALID_INDEX when unassigned
	Zenith_Span<const Zenith_SkeletonChain> GetChains() const;
	const Zenith_SkeletonIKDefinition* FindIKDefinition(uint64 ulNameHash) const;
	const Zenith_SkeletonSocket* FindSocket(uint64 ulNameHash) const;
	bool HasMirrorTable() const;
	uint32 GetMirrorJoint(uint32 uJoint) const;               // uJoint itself without a table
	Zenith_MirrorAxis GetMirrorAxis() const;
	uint32 FindCurve(uint64 ulNameHash) const;
	Zenith_Span<const Zenith_SkeletonCurve> GetCurves() const;
	Zenith_Span<const float> GetShellDistances() const;
	float GetMaxReach() const;
	float GetHipsHeight() const;
	uint64 GetRestPoseHash() const;
	uint64 GetHierarchyHash() const;
	bool IsHumanoidComplete() const;
	uint64 GetResidentBytes() const;
};
```

## Zenith_AnimationClip header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationClip.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAnimation/Zenith_AnimationTypes.h"
#include "ZenithAnimation/Zenith_AnimationErrors.h"
#include "ZenithAnimation/Zenith_AnimationMath.h"
#include "ZenithAnimation/Zenith_AnimationPose.h"

/// @contract value Zenith_AnimClipEvent
/// Ownership: Plain value inside a Zenith_AnimationClip (decoded from ANIM_SECTION_CLIP_EVENTS). It holds no interned string:
///   the clip is shared by every world, so each world interns the event names into its own binding-cache entry
///   ("Asset bindings" step 3) and the clip is never written after activation.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Seconds for times; m_fMinWeight a fraction.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimClipEvent
{
	uint64 m_ulNameHash = 0;
	float m_fTime = 0.0f;
	float m_fDuration = 0.0f;                 // 0: instant
	Zenith_EventFireMode m_eFireMode = EVENT_FIRE_MODE_WEIGHTED;
	uint8 m_uFlags = 0;                      // ANIM_EVENT_FLAG_GAMEPLAY | _AUDIO | _VFX | _WARP_WINDOW
	float m_fMinWeight = 0.25f;
	sint32 m_iPayload = 0;
};

/// @contract value Zenith_AnimClipMarker
/// Ownership: Plain value inside a Zenith_AnimationClip.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Seconds.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimClipMarker
{
	uint64 m_ulNameHash = 0;
	float m_fTime = 0.0f;
};

/// @contract value Zenith_ClipSegmentTable
/// Ownership: Borrowed view of decoded segments: the clip's own table for a resident clip, the world's stream cache table
///   for a streamed clip ("Clip streaming"); a null entry is a segment that is not resident.
/// Thread affinity: IMMUTABLE for the duration of a phase-3 node (the cache changes only at tick boundaries and in
///   Animation.Significance, which precedes every evaluation of the tick). Members non-const: an element of the stream
///   cache's table array, which the cache rewrites at tick boundaries.
/// Invalidation: Invalid after the current phase.
/// Units: Segment indices.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ClipSegmentTable
{
	Zenith_Span<const Zenith_DecodedClipSegment* const> m_apxSegments;   // Zenith_DecodedClipSegment is Internal (sampling)
};

/// @contract type Zenith_AnimationClip
/// Ownership: Created by the Animation.Clip kind loader; shared through Zenith_AssetHandle<Zenith_AnimationClip>. The header
///   set and, for a resident clip, every decoded segment are one general-heap block; a streamed clip's segments live in
///   each world's stream cache.
/// Thread affinity: ANY_THREAD. Immutable once activated.
/// Invalidation: Spans are valid while a handle is held.
/// Units: Seconds; metres; unit quaternions; the sample rate in Hz.
/// Errors: Sampling returns ANIMATION_ERROR_CLIP_NOT_RESIDENT when a needed segment is absent; nothing asserts on file data.
/// Version: since Z2 Stage 3. Cooked format ZANM version 1.
/// @endcontract
class Zenith_AnimationClip
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_ANIMATION_CLIP;

	const Zenith_PersistentID& GetSkeletonID() const;
	uint64 GetSkeletonRestPoseHash() const;
	float GetDuration() const;
	float GetSampleRate() const;
	uint32 GetFrameCount() const;
	uint32 GetFlags() const;                 // ANIM_CLIP_FLAG_LOOP … ANIM_CLIP_FLAG_MIRRORABLE
	Zenith_RootMotionMode GetRootMotionMode() const;
	Zenith_AdditiveSpace GetAdditiveSpace() const;
	Zenith_Span<const Zenith_AnimClipEvent> GetEvents() const;
	Zenith_Span<const Zenith_AnimClipMarker> GetMarkers() const;
	Zenith_StringView GetEventName(uint32 uEvent) const;
	uint32 GetSegmentCount() const;
	uint32 GetChunkCount() const;             // 0 for a resident clip
	uint32 GetChunkOfSegment(uint32 uSegment) const;
	Zenith_ClipSegmentTable GetResidentSegments() const;   // empty spans for a streamed clip
	float GetMaxMeasuredError() const;
	uint64 GetResidentBytes() const;
};

/// @contract functions Zenith_ClipSampling
/// Ownership: Pure functions over an immutable clip and caller-owned output.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Seconds; the output pose in the clip skeleton's local space (deltas for an additive clip).
/// Errors: ANIMATION_ERROR_CLIP_NOT_RESIDENT when a needed segment of xSegments is null (xOut is then unchanged);
///   ZENITH_ASSERT on an output span shorter than the clip's joint or curve count.
/// Version: since Z2 Stage 3. Results are bit-identical per platform and build ([Z2-N-ARCH-883]).
/// @endcontract
Zenith_Result<void, Zenith_AnimationError> Zenith_SampleClip(const Zenith_AnimationClip& xClip,
	const Zenith_ClipSegmentTable& xSegments, float fTime, bool bLoop, Zenith_PoseView xOut);
Zenith_DualQuaternion Zenith_SampleRootMotion(const Zenith_AnimationClip& xClip, float fTime);   // Q(t)
Zenith_DualQuaternion Zenith_ComputeRootMotionDelta(const Zenith_AnimationClip& xClip, float fFrom, float fTo,
	uint32 uWraps, bool bForward);                                                                         // Graph.md "Root motion sampling"
```

**Clip sampling.** `Zenith_SampleClip(clip, segments, t, loop, out)`:

1. `t := loop ? t − D × floor(t / D) : clamp(t, 0, D)` (a single-pose clip, F = 1, returns frame 0).
2. `f0 = min(floor(t × rate), F − 2)`; the interval length `L = (f0 = F − 2) ? D − f0 / rate : 1 / rate`; `α = clamp((t − f0 / rate) / L, 0, 1)`.
3. Segments `s0 = f0 / 16`, `s1 = (f0 + 1) / 16`; either null: `ANIMATION_ERROR_CLIP_NOT_RESIDENT`.
4. Per joint: a `DEFAULT` channel writes the skeleton's rest value (the identity for an additive clip); a `CONSTANT` channel writes its constant; an `ANIMATED` channel decodes frames f0 and f0 + 1 ("Range reduction" of `Zenith2/Docs/Design/ANIM/Formats.md`) and interpolates: translation and scale linearly, rotation by `Zenith_Nlerp`.
5. Per curve: the same, linear.

Complexity O(J + C) with two bit-unpacks per animated component; at J = 60 a sample takes about 2 µs on the reference desktop (`Zenith_Bench_ClipSampling`, a report).

## Zenith_AnimationGraphAsset header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationGraphAsset.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAnimation/Zenith_AnimationTypes.h"

/// @contract value Zenith_AnimParameterInfo
/// Ownership: Plain value returned by value.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: The parameter's own units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimParameterInfo
{
	uint64 m_ulNameHash = 0;
	Zenith_AnimParameterKind m_eKind = ANIM_PARAMETER_KIND_FLOAT;
	bool m_bReserved = false;
	uint16 m_uTriggerHoldTicks = 0;
	float m_fDefault = 0.0f;
	float m_fMinimum = 0.0f;
	float m_fMaximum = 0.0f;
};

/// @contract type Zenith_AnimationGraphAsset
/// Ownership: Created by the Animation.Graph kind loader; shared through Zenith_AssetHandle<Zenith_AnimationGraphAsset>; one
///   general-heap block holding every decoded table.
/// Thread affinity: ANY_THREAD. Immutable once activated.
/// Invalidation: Views are valid while a handle is held.
/// Units: Indices into the graph's tables.
/// Errors: Lookups return uANIM_INVALID_INDEX when absent; index arguments out of range are ZENITH_ASSERT.
/// Version: since Z2 Stage 3. Cooked format ZAGR version 1.
/// @endcontract
class Zenith_AnimationGraphAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_ANIMATION_GRAPH;

	const Zenith_PersistentID& GetSkeletonID() const;
	uint64 GetSkeletonRestPoseHash() const;
	uint32 GetParameterCount() const;
	uint32 FindParameter(uint64 ulNameHash) const;          // O(log P)
	Zenith_AnimParameterInfo GetParameter(uint32 uParameter) const;
	uint32 GetLayerCount() const;
	uint32 FindLayer(uint64 ulNameHash) const;
	uint32 FindSlot(uint64 ulNameHash) const;
	uint32 FindStateMachine(uint64 ulNameHash) const;
	uint32 FindState(uint32 uMachine, uint64 ulNameHash) const;
	uint64 GetStateNameHash(uint32 uMachine, uint32 uState) const;
	uint32 GetClipCount() const;
	const Zenith_PersistentID& GetClipID(uint32 uClip) const;
	uint32 GetInstanceMemoryBytes() const;
	bool Inertialises() const;        // a SWITCH, STATE_MACHINE, SLOT or MOTION_MATCH node exists: the instance gets an inertial buffer
	uint32 GetPoseStackDepth() const;
	uint64 GetGraphHash() const;
	Zenith_StringView GetNodeName(uint32 uNode) const;
};
```

## Zenith_AnimationPose header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationPose.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithAnimation/Zenith_AnimationTypes.h"

class Zenith_SkeletonAsset;

/// @contract value Zenith_PoseView
/// Ownership: Non-owning view of caller-owned joint and curve storage (a pose-stack entry, an instance buffer).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when the storage is reused or freed.
/// Units: Local transforms in metres, unit quaternions, factors; curves in curve units.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PoseView
{
	Zenith_Span<Zenith_Transform> m_axLocal;
	Zenith_Span<float> m_afCurves;
};

/// @contract value Zenith_ConstPoseView
/// Ownership: As Zenith_PoseView, read-only.
/// Thread affinity: IMMUTABLE for the view's lifetime. Members non-const: callers fill arrays of it element by element
///   before passing them.
/// Invalidation: As Zenith_PoseView.
/// Units: As Zenith_PoseView.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ConstPoseView
{
	Zenith_Span<const Zenith_Transform> m_axLocal;
	Zenith_Span<const float> m_afCurves;
};

/// @contract functions Zenith_PoseOperations
/// Ownership: Pure functions; outputs are caller-owned and may alias an input only where noted.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As the views; weights are fractions, clamped to [0, 1].
/// Errors: ZENITH_ASSERT when spans differ in length from the skeleton's joint or curve counts.
/// Version: since Z2 Stage 3 (blend, N-way, additive, local-to-model, rest); Stage 4 (mirror, masked model-space).
/// @endcontract
void Zenith_SetRestPose(const Zenith_SkeletonAsset& xSkeleton, Zenith_PoseView xOut);
void Zenith_SetIdentityPose(Zenith_PoseView xOut);                                 // the additive zero
void Zenith_BlendPoses(Zenith_ConstPoseView xA, Zenith_ConstPoseView xB, float fWeight, Zenith_PoseView xOut);   // xOut may alias xA
void Zenith_BlendPosesN(Zenith_Span<const Zenith_ConstPoseView> axInputs, Zenith_Span<const float> afWeights,
	Zenith_PoseView xOut);
void Zenith_BlendPosesMasked(Zenith_ConstPoseView xBase, Zenith_ConstPoseView xLayer, float fWeight,
	Zenith_Span<const float> afJointWeights, Zenith_Span<const float> afCurveWeights, Zenith_PoseView xOut);
void Zenith_BlendPosesMaskedModelRotation(const Zenith_SkeletonAsset& xSkeleton, Zenith_ConstPoseView xBase,
	Zenith_ConstPoseView xLayer, float fWeight, Zenith_Span<const float> afJointWeights, Zenith_PoseView xOut);
void Zenith_ApplyAdditivePose(const Zenith_SkeletonAsset& xSkeleton, Zenith_ConstPoseView xBase, Zenith_ConstPoseView xAdditive,
	float fWeight, Zenith_AdditiveSpace eSpace, Zenith_Span<const float> afJointWeights, Zenith_PoseView xOut);
void Zenith_LocalToModel(const Zenith_SkeletonAsset& xSkeleton, Zenith_Span<const Zenith_Transform> axLocal,
	Zenith_Span<Zenith_Matrix4> axModel);
void Zenith_ModelToLocal(const Zenith_SkeletonAsset& xSkeleton, Zenith_Span<const Zenith_Matrix4> axModel,
	Zenith_Span<Zenith_Transform> axLocal);                                        // shear dropped and counted
void Zenith_MirrorPose(const Zenith_SkeletonAsset& xSkeleton, Zenith_ConstPoseView xIn, Zenith_PoseView xOut);
```

The algorithms are `Zenith2/Docs/Design/ANIM/Graph.md → "Pose blending"`; `Zenith_MirrorPose` maps joint j's transform onto `mirror(j)` reflected through the mirror plane: for axis X, `t = (−t.x, t.y, t.z)` and `r = (r.x, −r.y, −r.z, r.w)`, analogous for Y and Z, scales copied.

## Zenith_AnimationWorld header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationWorld.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Matrix.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithAnimation/Zenith_AnimationTypes.h"
#include "ZenithAnimation/Zenith_AnimationErrors.h"

/// @contract enum Zenith_AnimatorStatus
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Serialized in save records as uint8; values never move.
/// @endcontract
enum Zenith_AnimatorStatus : uint8
{
	ANIMATOR_STATUS_NONE = 0,         // no instance for that simulation ID
	ANIMATOR_STATUS_BINDING,          // assets requested; the instance outputs the rest pose
	ANIMATOR_STATUS_BOUND,            // evaluating
	ANIMATOR_STATUS_BINDING_FAILED,   // an asset failed or mismatched; rest pose until the component changes
	ANIMATOR_STATUS_DISABLED,         // Enabled false, or the instance could not be allocated
};

/// @contract value Zenith_AnimStateInfo
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: returned in a Zenith_Result, whose move assignment assigns it.
/// Invalidation: None.
/// Units: Seconds; normalised time a fraction.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimStateInfo
{
	uint64 m_ulMachineNameHash = 0;
	uint64 m_ulStateNameHash = 0;
	float m_fTimeInState = 0.0f;
	float m_fNormalisedTime = 0.0f;
	bool m_bInTransition = false;
	uint64 m_ulTransitionTargetHash = 0;
};

/// @contract value Zenith_ConstModelPoseView
/// Ownership: Non-owning view into a world resource's pose buffers.
/// Thread affinity: IMMUTABLE for the reading node's phase. Members non-const: returned in a Zenith_Result, whose move
///   assignment assigns it.
/// Invalidation: Invalid after the phase in which it was obtained.
/// Units: Model-space matrices (metres), the entity's object space.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_ConstModelPoseView
{
	Zenith_Span<const Zenith_Matrix4> m_axModel;
	Zenith_Span<const float> m_afCurves;
	uint64 m_ulEvaluationTick = 0;
};

/// @contract value Zenith_AnimationWorldStats
/// Ownership: Plain value returned by Zenith_AnimationWorld::GetStats.
/// Thread affinity: IMMUTABLE. Members non-const: its producer resets it and accumulates its counters in place.
/// Invalidation: None.
/// Units: Counts are of the latest Animation.Significance and Animation.Evaluate runs; bytes are resident bytes.
/// Errors: None.
/// Version: since Z2 Stage 3 (streaming members zero until Stage 4).
/// @endcontract
struct Zenith_AnimationWorldStats
{
	uint64 m_ulTick = 0;                          // the world tick of the latest Animation.Evaluate
	uint32 m_auInstancesByStatus[5] = {};         // indexed by Zenith_AnimatorStatus (NONE unused, always 0)
	uint32 m_auInstancesByRate[4] = {};           // rate divisor 1, 2, 4, 8
	uint32 m_uInstancesRefused = 0;               // this tick, at the instance cap
	uint32 m_uEvaluations = 0;                    // instances evaluated this tick
	uint32 m_uEvaluationsSkipped = 0;             // bound instances not due this tick
	uint64 m_ulJointsEvaluated = 0;
	uint32 m_uClipSamples = 0;
	uint32 m_uTransitions = 0;
	uint32 m_uInertialTransitions = 0;
	uint32 m_uEventsEmitted = 0;
	uint32 m_uEventsDropped = 0;
	uint32 m_uBindingsInFlight = 0;               // binding requests issued and not yet published
	uint32 m_uStreamRequestsInFlight = 0;
	uint32 m_uStreamStalls = 0;                   // this tick
	uint32 m_uStreamDeferred = 0;                 // this tick
	uint32 m_uStreamFailedChunks = 0;             // chunks in FAILED
	uint64 m_ulStreamResidentBytes = 0;
	uint64 m_ulInstanceBytes = 0;                 // instance blocks and records
	uint64 m_ulCacheBytes = 0;                    // binding caches, retarget maps, remap tables
};

inline constexpr uint32 uANIM_DEBUG_MAX_NODES = 256;
inline constexpr uint32 uANIM_DEBUG_MAX_PARAMETERS = 256;
inline constexpr uint32 uANIM_DEBUG_MAX_EVENTS = 16;

/// @contract value Zenith_AnimDebugLayer
/// Ownership: Plain value; an element of Zenith_AnimDebugState::m_axLayers.
/// Thread affinity: OWNER_THREAD (as the enclosing state).
/// Invalidation: None; a snapshot of the latest evaluation.
/// Units: m_fWeight and m_afTransitionProgress fractions in [0, 1]; hashes Zenith_Hash64 of names.
/// Errors: None; entries past m_uTransitionCount are zero.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimDebugLayer
{
	uint64 m_ulLayerHash = 0;
	float m_fWeight = 0.0f;
	Zenith_AnimStateInfo m_xState;                 // the layer's first state machine (zero when it has none)
	uint8 m_uTransitionCount = 0;                 // active transition records, 0 … uANIM_MAX_ACTIVE_TRANSITIONS
	uint8 m_auReserved[3] = {};
	uint64 m_aulTransitionTargetHash[4] = {};
	float m_afTransitionProgress[4] = {};          // crossfade records only: an inertial take pushes no record
};

/// @contract value Zenith_AnimDebugSlot
/// Ownership: Plain value; an element of Zenith_AnimDebugState::m_axSlots.
/// Thread affinity: OWNER_THREAD (as the enclosing state).
/// Invalidation: None; a snapshot of the latest evaluation.
/// Units: m_fTime seconds into the slot's clip; m_fWeight a fraction in [0, 1]; m_ulSlotHash Zenith_Hash64 of the slot name.
/// Errors: None; an empty slot has a null m_xClip and weight 0.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimDebugSlot
{
	uint64 m_ulSlotHash = 0;
	Zenith_PersistentID m_xClip;
	float m_fTime = 0.0f;
	float m_fWeight = 0.0f;
	uint16 m_uTag = 0;
	uint16 m_uReserved = 0;
};

/// @contract value Zenith_AnimDebugEvent
/// Ownership: Plain value; an element of Zenith_AnimDebugState::m_axLastEvents.
/// Thread affinity: OWNER_THREAD (as the enclosing state).
/// Invalidation: None; a snapshot of the latest evaluation.
/// Units: m_fClipTime seconds into the firing clip; m_ulNameHash Zenith_Hash64 of the event name.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimDebugEvent
{
	uint64 m_ulNameHash = 0;
	float m_fClipTime = 0.0f;
	uint8 m_uPhase = 0;                           // 0 instant, 1 begin, 2 end
	uint8 m_uLayer = 0;
	uint16 m_uReserved = 0;
};

/// @contract value Zenith_AnimDebugState
/// Ownership: Caller-owned output of GetDebugState (about 8 KiB; callers keep one, never per entity per frame).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None; a snapshot of the latest evaluation.
/// Units: Seconds; weights fractions; hashes Zenith_Hash64 of names.
/// Errors: None; m_bTruncated is set when the graph has more nodes or parameters than the arrays hold.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimDebugState
{
	Zenith_AnimatorStatus m_eStatus = ANIMATOR_STATUS_NONE;
	uint8 m_uRateDivisor = 1;
	uint8 m_uPhaseOffset = 0;
	bool m_bTruncated = false;
	float m_fSignificance = 0.0f;
	float m_fDistanceToCamera = 0.0f;
	uint64 m_ulGraphHash = 0;
	uint64 m_ulLastEvaluationTick = 0;
	uint32 m_uLayerCount = 0;
	Zenith_AnimDebugLayer m_axLayers[uANIM_MAX_LAYERS];
	uint32 m_uSlotCount = 0;
	Zenith_AnimDebugSlot m_axSlots[uANIM_MAX_SLOTS];
	uint32 m_uSyncGroupCount = 0;
	uint64 m_aulSyncGroupHash[32] = {};
	uint16 m_auSyncLeaderNode[32] = {};           // node index of each group's leader this evaluation
	uint32 m_uNodeCount = 0;
	float m_afNodeWeights[uANIM_DEBUG_MAX_NODES] = {};   // path weight per node, node order
	uint32 m_uParameterCount = 0;
	uint64 m_aulParameterHash[uANIM_DEBUG_MAX_PARAMETERS] = {};
	float m_afParameterValue[uANIM_DEBUG_MAX_PARAMETERS] = {};
	uint32 m_uEventCount = 0;
	Zenith_AnimDebugEvent m_axLastEvents[uANIM_DEBUG_MAX_EVENTS];   // the latest evaluation's first sixteen
	uint32 m_uResidentChunks = 0;                 // of the streamed clips this instance plays
	uint32 m_uStallsThisEvaluation = 0;
	bool m_bInertialActive = false;                // the instance's inertialiser ("Inertial blending" of Graph.md)
	bool m_bInertialTakePending = false;
	uint16 m_uReserved = 0;
	float m_fInertialElapsed = 0.0f;
	float m_fInertialSeconds = 0.0f;               // T of the active take
};

/// @contract type Zenith_AnimationWorld
/// Ownership: Created by the Animation module's AttachWorld, one per world, owned by the module instance; registered as the
///   world resource "Zenith.Animation.World" with owning phase PHASE_ANIMATION; destroyed at DetachWorld after every
///   completion ID ANIM allocated in the world has been completed (Zenith2/Docs/Design/ANIM.md "Zenith_Module_Animation header").
///   It holds the evaluation state of every instance; the instance table it is looked up through is the world resource
///   "Zenith.Animation.Residency" ("Resource partition" below).
/// Thread affinity: PHASE(Animation) for every write (Animation.Evaluate alone in its stage, plus ANIM's tick-boundary
///   publication handler and lifecycle listener on the sync-point node); the const members are callable from any node that
///   declared ReadResource on both "Zenith.Animation.World" and "Zenith.Animation.Residency", and read state committed by the
///   latest Animation.Evaluate.
/// Invalidation: Pose views are invalid after the reading node's phase.
/// Units: Seconds, metres; simulation IDs.
/// Errors: Lookups return ANIMATION_ERROR_NO_INSTANCE for a simulation ID without an instance, ANIMATION_ERROR_NOT_BOUND
///   before binding, ANIMATION_ERROR_UNKNOWN_PARAMETER and ANIMATION_ERROR_UNKNOWN_NAME for names the bound assets lack.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AnimationWorld
{
public:
	static const Zenith_TypeID xTYPE_ID;                                      // Zenith_MakeTypeID("Zenith.Animation.World")
	uint32 GetInstanceCount() const;
	Zenith_AnimatorStatus GetStatus(uint64 ulSimulationID) const;
	Zenith_Result<float, Zenith_AnimationError> GetParameter(uint64 ulSimulationID, uint64 ulNameHash) const;
	Zenith_Result<Zenith_AnimStateInfo, Zenith_AnimationError> GetStateInfo(uint64 ulSimulationID, uint32 uLayer) const;
	Zenith_Result<float, Zenith_AnimationError> GetCurve(uint64 ulSimulationID, uint64 ulCurveHash) const;
	Zenith_Result<Zenith_ConstModelPoseView, Zenith_AnimationError> GetAnimatedModelPose(uint64 ulSimulationID) const;
	Zenith_Result<bool, Zenith_AnimationError> IsClipReady(uint64 ulSimulationID, const Zenith_PersistentID& xClip) const;
	Zenith_AnimationWorldStats GetStats() const;
	// development builds return ANIMATION_ERROR_NOT_AVAILABLE in Shipping
	Zenith_Result<void, Zenith_AnimationError> GetDebugState(uint64 ulSimulationID, Zenith_AnimDebugState& xOut) const;
};

/// @contract type Zenith_AnimationFinalPoses
/// Ownership: Created with the Zenith_AnimationWorld of the same world; registered as the world resource
///   "Zenith.Animation.FinalPoses" with owning phase PHASE_POST_PHYSICS.
/// Thread affinity: PHASE(PostPhysics) for writes (Animation.PostPhysicsBlend alone, plus the publication handler and the
///   lifecycle listener); const members from any node that declared ReadResource on it and on "Zenith.Animation.Residency"
///   (the instance lookup), reading the poses committed by the latest Animation.PostPhysicsBlend.
/// Invalidation: Views are invalid after the reading node's phase.
/// Units: Model space of the entity (metres); sockets returned in model space.
/// Errors: As Zenith_AnimationWorld; ANIMATION_ERROR_UNKNOWN_NAME for an absent socket.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AnimationFinalPoses
{
public:
	static const Zenith_TypeID xTYPE_ID;                                      // Zenith_MakeTypeID("Zenith.Animation.FinalPoses")
	Zenith_Result<Zenith_ConstModelPoseView, Zenith_AnimationError> GetFinalModelPose(uint64 ulSimulationID) const;
	Zenith_Result<Zenith_Matrix4, Zenith_AnimationError> GetJointModelTransform(uint64 ulSimulationID, uint32 uJoint) const;
	Zenith_Result<Zenith_Transform, Zenith_AnimationError> GetSocketModelTransform(uint64 ulSimulationID,
		uint64 ulSocketHash) const;
};

/// @contract value Zenith_AnimBudgetDistances
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres; the values of animation.budget.fullRateDistance, .halfRateDistance and .quarterRateDistance.
/// Errors: None; Zenith_AnimComputeRateDivisor treats a non-increasing triple by the rule below, never asserts.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AnimBudgetDistances
{
	float m_fFullRateDistance = 15.0f;
	float m_fHalfRateDistance = 40.0f;
	float m_fQuarterRateDistance = 80.0f;
};

/// @contract functions Zenith_AnimRateRule
/// Ownership: None; a pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: fDistanceMetres metres (≥ 0; the camera distance of "Significance"); fBias the animator's SignificanceBias in [0.01, 100].
/// Errors: None: a non-finite or negative distance, or a bias outside its range (clamped), still returns a divisor.
/// Version: since Z2 Stage 4. The function Animation.Significance calls for every instance, public so tools (CHAR's budget
///   panel, row CH15) predict a divisor with the same code; deterministic (no transcendental, a fixed comparison order).
/// @endcontract
uint8 Zenith_AnimComputeRateDivisor(float fDistanceMetres, float fBias, bool bForceFullRate,
	const Zenith_AnimBudgetDistances& xDistances);   // 1, 2, 4 or 8
```

`Zenith_AnimationWorldStats` is what the on-screen Animation panel draws and `GetStats` fills it from the latest run of each node in O(1); `Zenith_AnimDebugState` is the data of `anim.dump`, CHAR's pose debugging (Z2-I-CHAR-03) and EDIT's live graph view; `GetDebugState` fills it in O(nodes + parameters) from the instance's block and returns `ANIMATION_ERROR_NOT_AVAILABLE` in Shipping builds.

<!-- elaboration -->
**Resource partition.** A world resource has one writer per stage (`Zenith2/Docs/Design/ECS/Scheduling.md → "Stages and barriers"`, [Z2-N-ARCH-403]), and a system with a merge node writes through that merge node. Phase 3 has two ANIM writers, so ANIM's per-world state is split over three world resources, each with one declared writer in its stage:

| Resource (stable name) | Type | Owning phase | Declared writer in its stage | Holds |
|---|---|---|---|---|
| `Zenith.Animation.Residency` | `Zenith_AnimationResidency` (Internal, `Internal/Instances/Zenith_AnimResidency.h`) | `PHASE_ANIMATION` | the merge node of `Animation.Significance` (the system node's own body writes it before its merge node runs) | the instance table and each instance's residency record (simulation ID, entity, status, bindings, significance and rate decision, phase offset, block pointer and block generation, the reset serial, queued commands), block allocation and release, the binding cache (with each clip's per-world interned event names and the retarget maps), the stream cache and its byte accounting, the request table (every allocated completion ID until it publishes), the two in-flight counters, the collected-needs array the merge function consumes, the abandoned-ID list, and the Significance half of the statistics |
| `Zenith.Animation.World` | `Zenith_AnimationWorld` | `PHASE_ANIMATION` | `Animation.Evaluate` (the declared writer is its merge node, which `EmitsEvent` gives it) | each instance's evaluation state: the evaluation record (applied block generation, applied reset serial), parameters and trigger counters (trigger consumption), node state, the inertial buffer and pending take, the pose history and `m_fLastDeltaSeconds`, the animated local and model poses and curves, slot and IK state, the curve overrides, the root-motion spread state, this evaluation's events and stall counts, and the Evaluate half of the statistics |
| `Zenith.Animation.FinalPoses` | `Zenith_AnimationFinalPoses` | `PHASE_POST_PHYSICS` | `Animation.PostPhysicsBlend` | each instance's final local poses (current, previous), final model pose and final curves, and its ragdoll recovery state (O, `w_1`, the last p, active; "Ragdoll recover") |

An instance block is one allocation whose regions belong to these resources ("Pose store layout"): `Animation.Significance` allocates and releases blocks (a Residency write) and never writes a region of `Zenith.Animation.World` or `Zenith.Animation.FinalPoses`; it requests the initialisation or reset of a block's contents by bumping the block generation or the reset serial in the residency record, and `Animation.Evaluate` applies the request ("Animation.Evaluate" step 0). The tick-boundary publication handler and the lifecycle listener run on single nodes outside every stage and may write all three. Every node that reads `Zenith.Animation.World` or `Zenith.Animation.FinalPoses` looks the instance up through the residency, so it declares `ReadResource Zenith.Animation.Residency` as well (the systems of "Systems", the extractor, the read-world bindings, the save participant, and PHYS's `Physics.Step` and ragdoll enter playback through the pose source, row AN6). `Zenith_AnimationWorld` and `Zenith_AnimationFinalPoses` hold a borrowed pointer to their world's `Zenith_AnimationResidency`, set at AttachWorld, through which their const members look instances up.

```cpp
// Zenith2/Engine/ZenithAnimation/Internal/Instances/Zenith_AnimResidency.h (Internal: ZenithAnimation and its tests only)
#pragma once
#include "ZenithCore/Zenith_Array.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithAnimation/Internal/Instances/Zenith_AnimInstance.h"      // Zenith_AnimInstanceEntry, Zenith_AnimResidencyRecord,
                                                                        // Zenith_AnimNeed, Zenith_AnimRequest, Zenith_AnimSignificanceStats
#include "ZenithAnimation/Internal/Assets/Zenith_AnimBindings.h"         // Zenith_AnimBindingCache
#include "ZenithAnimation/Internal/Streaming/Zenith_AnimStreamCache.h"   // Zenith_AnimStreamCache

class Zenith_AnimationResidency
{
public:
	static const Zenith_TypeID xTYPE_ID;                                  // Zenith_MakeTypeID("Zenith.Animation.Residency")
	Zenith_Array<Zenith_AnimInstanceEntry> m_axInstances;                 // { uint64 simulation ID; Zenith_AnimResidencyRecord* }, sorted
	Zenith_AnimBindingCache m_xBindingCache;
	Zenith_AnimStreamCache m_xStreamCache;
	Zenith_Array<Zenith_AnimRequest> m_axRequests;                        // ascending completion ID; every ID not yet published
	Zenith_AnimNeed* m_axCollectedNeeds = nullptr;                        // fixed array, capacity bindingQueueCapacity + maxReadsInFlight
	uint32 m_uCollectedNeedCount = 0;                                    // written by Animation.Significance, consumed by its merge function
	Zenith_Array<uint64> m_aulAbandonedIDs;                              // completion IDs to complete with the abandoned payload
	uint32 m_uBindingLoadsInFlight = 0;                                  // ≤ animation.bindingQueueCapacity
	uint32 m_uChunkReadsInFlight = 0;                                    // ≤ animation.streaming.maxReadsInFlight
	Zenith_AnimSignificanceStats m_xStats;                                // the Significance half of Zenith_AnimationWorldStats
};
```

The merge function reaches the residency through `Zenith_MergeContext::GetUserData` (the system's `m_pUserData`, set at AttachWorld to the world's `Zenith_AnimationResidency`, `Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_MergeContext header"`), so the collected needs live in the resource the merge node writes, never in the system node's arena. The collected-needs array is allocated at AttachWorld with capacity `animation.bindingQueueCapacity + animation.streaming.maxReadsInFlight` (each issued need consumes one unit of one counter, so no run collects more needs than can be issued); a run that finds more outstanding needs stops collecting at the capacity, and the rest stay outstanding for a subsequent run in the same order.

## Zenith_AnimationCommands header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationCommands.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"

/// @contract value Zenith_AnimSlotPlayDesc
/// Ownership: Plain value copied into the command payload at record time.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Seconds; m_fRate a factor in [0, 16].
/// Errors: Out-of-range values are clamped at playback and counted; an unknown slot is dropped at playback and counted.
/// Version: since Z2 Stage 3. 56-byte payload of Animation.PlaySlot.
/// @endcontract
struct Zenith_AnimSlotPlayDesc
{
	uint64 m_ulSlotHash = 0;
	Zenith_PersistentID m_xClip;
	float m_fBlendInSeconds = 0.2f;
	float m_fBlendOutSeconds = 0.25f;
	float m_fRate = 1.0f;
	float m_fStartTime = 0.0f;                // seconds into the clip
	uint16 m_uTag = 0;                       // the issuer's tag; a cinematic sets bit 15 (ANIM_SLOT_TAG_CINEMATIC); bit 14
	                                          // (ANIM_SLOT_TAG_FULL_RATE) forces full rate while the slot is active
	uint8 m_uFlags = 0;                      // bit 0 loop, bit 1 root motion
	uint8 m_uReserved = 0;
};

/// @contract functions Zenith_AnimationCommandRecording
/// Ownership: Record into the caller's command buffer; own nothing.
/// Thread affinity: OWNER_THREAD — the buffer's.
/// Invalidation: None.
/// Units: As each payload; name hashes are Zenith_Hash64 of the UTF-8 name.
/// Errors: Recording cannot fail beyond the buffer's own capacity (merge-time rejection, [Z2-N-CONTRACT-346]); semantic
///   errors (an unknown parameter, a stale entity) are detected at playback and counted, never asserted.
/// Version: since Z2 Stage 3 (parameters, triggers, slots, force state, reset, prefetch); Stage 4 (curve overrides).
/// @endcontract
void Zenith_RecordSetAnimParameter(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulNameHash, float fValue);
void Zenith_RecordSetAnimTrigger(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulNameHash, bool bSet);
void Zenith_RecordPlaySlot(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, const Zenith_AnimSlotPlayDesc& xDesc);
void Zenith_RecordStopSlot(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulSlotHash, float fBlendOutSeconds);
void Zenith_RecordSetSlotTime(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulSlotHash, float fTime);
void Zenith_RecordForceAnimState(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulMachineHash,
	uint64 ulStateHash, float fBlendSeconds);
void Zenith_RecordResetAnimGraph(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity);
void Zenith_RecordSetAnimCurveOverride(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, uint64 ulCurveHash,
	float fValue, float fWeight);                                     // fWeight 0 removes the override
void Zenith_RecordPrefetchClip(Zenith_CommandBuffer& xBuffer, Zenith_EntityHandle xEntity, const Zenith_PersistentID& xClip,
	float fStartTime);
```

| Command kind | Payload (bytes) | Phases | Playback (sync-point node, merge order) |
|---|---|---|---|
| `Animation.SetParameter` | entity, name hash, value, kind tag (24) | 1–10 | write the parameter (clamped) into the instance's block; an unbound instance queues it (at most 64 queued per instance, the oldest dropped and counted) and applies the queue at binding |
| `Animation.SetTrigger` | entity, name hash, set flag (24) | 1–10 | set (value 1, hold counter) or reset |
| `Animation.PlaySlot` | `Zenith_AnimSlotPlayDesc` (48 with alignment) and the entity (8) (56) | 1–10 | "Slots" of `Zenith2/Docs/Design/ANIM/Graph.md`; the clip binds through "Asset bindings" |
| `Animation.StopSlot` | entity, slot hash, blend out (24) | 1–10 | start the blend-out |
| `Animation.SetSlotTime` | entity, slot hash, time (24) | 1–10 | set the slot clip time exactly; the cinematic scrub path (CINE) — events between the old and new time do not fire |
| `Animation.ForceState` | entity, machine hash, state hash, seconds (32) | 1–10 | "State machine state table" |
| `Animation.ResetGraph` | entity (8) | 1–10 | reset every machine, slot and time |
| `Animation.SetCurveOverride` | entity, curve hash, value, weight (24) | 1–10 | up to 8 overrides per instance, applied after the layers as `Blend(curve, value, weight)`; a ninth replaces the lowest-weight one |
| `Animation.PrefetchClip` | entity, clip ID, start time (32) | 1–10 | add the clip's chunks at the start time to the instance's prefetch window until the clip plays or 300 ticks pass |

Commands recorded in phases 1 and 2 reach phase 3 of the same tick; commands of phases 3 to 10 reach the next tick's evaluation ([Z2-N-ARCH-489]).

## Zenith_AnimationEvents header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationEvents.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract value Zenith_AnimNotifyEvent
/// Ownership: The reflected payload of the event type "Zenith.Animation.Notify" (TYPE_KIND_EVENT), emitted by
///   Animation.Evaluate and delivered in phase 9.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: A delivered event view is invalid after the end of the tick (ECS's rule).
/// Units: m_fClipTime seconds; m_fTimeIntoTick a fraction of one tick in [0, k) for a k-tick evaluation span; m_fWeight a
///   fraction.
/// Errors: None.
/// Version: since Z2 Stage 3. Every property is serialized, so replays and the world-state hash record it ([Z2-N-ARCH-610]).
/// @endcontract
struct Zenith_AnimNotifyEvent
{
	Zenith_EntityHandle m_xEntity;            // 1  Entity
	uint64 m_ulNameHash = 0;                 // 2  NameHash
	Zenith_StringID m_xName;                  // 3  Name: from the world's binding-cache entry of the clip ("Asset bindings")
	Zenith_PersistentID m_xClip;              // 4  Clip
	float m_fClipTime = 0.0f;                 // 5  ClipTime
	float m_fTimeIntoTick = 0.0f;             // 6  TimeIntoTick
	float m_fWeight = 0.0f;                   // 7  Weight
	sint32 m_iPayload = 0;                   // 8  Payload
	uint8 m_uPhase = 0;                      // 9  Phase: 0 instant, 1 begin, 2 end
	uint8 m_uFlags = 0;                      // 10 Flags: gameplay, audio, VFX, warp window
	uint8 m_uLayer = 0;                      // 11 Layer
	uint8 m_uReserved = 0;
};

/// @contract value Zenith_AnimStateChangedEvent
/// Ownership: The reflected payload of "Zenith.Animation.StateChanged", emitted when a state machine takes a transition.
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As Zenith_AnimNotifyEvent.
/// Units: Seconds.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimStateChangedEvent
{
	Zenith_EntityHandle m_xEntity;            // 1 Entity
	uint64 m_ulMachineHash = 0;              // 2 Machine
	uint64 m_ulFromStateHash = 0;            // 3 FromState
	uint64 m_ulToStateHash = 0;              // 4 ToState
	float m_fDuration = 0.0f;                 // 5 Duration
	uint8 m_uLayer = 0;                      // 6 Layer
	uint8 m_uReserved[3] = {};
};
```

## Zenith_AnimationIK header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationIK.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"

/// @contract value Zenith_TwoBoneIKInput
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Model-space metres and unit quaternions; m_fSoftness metres; m_fWeight a fraction.
/// Errors: None; degenerate inputs are handled by the solver (below).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_TwoBoneIKInput
{
	Zenith_Vector3 m_xRootPosition, m_xMidPosition, m_xEndPosition;
	Zenith_Quaternion m_xRootRotation, m_xMidRotation, m_xEndRotation;
	Zenith_Vector3 m_xTargetPosition;
	Zenith_Quaternion m_xTargetRotation;
	Zenith_Vector3 m_xPoleDirection;          // the desired bend direction of the middle joint
	float m_fSoftness = 0.0f;                 // 0: hard reach
	float m_fWeight = 1.0f;
	bool m_bApplyEndRotation = false;
};

/// @contract value Zenith_TwoBoneIKOutput
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Model-space unit quaternions; m_fReachErrorMetres metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_TwoBoneIKOutput
{
	const Zenith_Quaternion m_xRootRotation{}, m_xMidRotation{}, m_xEndRotation{};
	const bool m_bReached = false;
	const float m_fReachErrorMetres = 0.0f;
};

/// @contract value Zenith_ChainIKSettings
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ChainIKSettings
{
	const uint32 m_uMaxIterations = 10;
	const float m_fToleranceMetres = 0.001f;
};

/// @contract functions Zenith_IKSolvers
/// Ownership: Pure functions over caller-owned spans.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Model space; metres; radians.
/// Errors: ZENITH_ASSERT on span-length mismatches; numerically degenerate configurations are handled and reported in
///   the outputs, never asserted.
/// Version: since Z2 Stage 4. Bit-identical per platform and build.
/// @endcontract
Zenith_TwoBoneIKOutput Zenith_SolveTwoBoneIK(const Zenith_TwoBoneIKInput& xInput);
uint32 Zenith_SolveChainIK(Zenith_Span<Zenith_Vector3> axPositions, Zenith_Span<const float> afBoneLengths,
	const Zenith_Vector3& xTarget, const Zenith_ChainIKSettings& xSettings);      // returns iterations used
void Zenith_ChainPositionsToRotations(Zenith_Span<const Zenith_Vector3> axOldPositions, Zenith_Span<const Zenith_Vector3> axNewPositions,
	Zenith_Span<Zenith_Quaternion> axModelRotations);
void Zenith_SolveLookAt(Zenith_Span<Zenith_Quaternion> axModelRotations, Zenith_Span<const Zenith_Vector3> axPositions,
	Zenith_Span<const Zenith_Vector3> axForwardAxes, Zenith_Span<const float> afShares, const Zenith_Vector3& xTarget,
	float fMaxAngleRadians, float fWeight);
```

## Zenith_AnimationRetarget header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationRetarget.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithAnimation/Zenith_AnimationErrors.h"
#include "ZenithAnimation/Zenith_AnimationPose.h"

class Zenith_SkeletonAsset;

/// @contract enum Zenith_RetargetRootTranslation
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_RetargetRootTranslation : uint8 { RETARGET_ROOT_SCALE_BY_HIPS_HEIGHT = 0, RETARGET_ROOT_COPY, RETARGET_ROOT_NONE };

/// @contract value Zenith_RetargetSettings
/// Ownership: Plain value; the spans are borrowed for Create.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Joint indices.
/// Errors: Validated by Create.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_RetargetSettings
{
	Zenith_RetargetRootTranslation m_eRootTranslation = RETARGET_ROOT_SCALE_BY_HIPS_HEIGHT;
	Zenith_Span<const uint32> m_auTargetToSourceOverride;   // empty, or one entry per target joint (uANIM_INVALID_INDEX: role mapping;
	                                                          // uANIM_UNMAPPED_JOINT: keep the target's rest rotation)
};

/// @contract type Zenith_RetargetMap
/// Ownership: Created by Create as a Zenith_Owned<Zenith_RetargetMap>; cached per world by the module ("Retarget map
///   construction") and by the cook step; general-heap storage.
/// Thread affinity: ANY_THREAD after Create (immutable).
/// Invalidation: None while owned.
/// Units: Unit quaternions; the root scale a factor.
/// Errors: Create returns ANIMATION_ERROR_RETARGET_UNSUPPORTED when a target joint has no mapping by role or override and
///   the settings require one, ANIMATION_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_RetargetMap
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_RetargetMap>, Zenith_AnimationError> Create(const Zenith_SkeletonAsset& xSource,
		const Zenith_SkeletonAsset& xTarget, const Zenith_RetargetSettings& xSettings, Zenith_Heap& xHeap);
	uint32 GetSourceJoint(uint32 uTargetJoint) const;
	float GetRootScale() const;
	uint64 GetKey() const;           // hash of both skeletons' hierarchy and rest hashes and the settings
	void Apply(Zenith_ConstPoseView xSource, Zenith_PoseView xTarget);   // "Runtime retarget application"
};
```

## Zenith_AnimationPreview header

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationPreview.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithAnimation/Zenith_AnimationErrors.h"
#include "ZenithAnimation/Zenith_AnimationMath.h"
#include "ZenithAnimation/Zenith_AnimationWorld.h"    // Zenith_ConstModelPoseView, Zenith_AnimDebugState
#include "ZenithAnimation/Zenith_AnimationEvents.h"

class Zenith_SkeletonAsset;
class Zenith_AnimationGraphAsset;
class Zenith_AnimationClip;

/// @contract value Zenith_AnimationPreviewDesc
/// Ownership: Plain value; the pointed-to assets must outlive the preview (the caller holds handles).
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Create.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AnimationPreviewDesc
{
	const Zenith_SkeletonAsset* m_pxSkeleton = nullptr;
	const Zenith_AnimationGraphAsset* m_pxGraph = nullptr;          // null: a single-clip preview of m_apxClips[0]
	Zenith_Span<const Zenith_AnimationClip* const> m_apxClips;      // the graph's clip table, in order
	Zenith_Heap* m_pxHeap = nullptr;
};

/// @contract type Zenith_AnimationPreview
/// Ownership: Created by Create as a Zenith_Owned; owned by the editor's preview panel or a test.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Views it returns are invalid after the next Advance or Set* call.
/// Units: Seconds; model space.
/// Errors: Create returns ANIMATION_ERROR_SKELETON_MISMATCH, ANIMATION_ERROR_INVALID_ASSET, ANIMATION_ERROR_OUT_OF_MEMORY;
///   Set* return ANIMATION_ERROR_UNKNOWN_PARAMETER and ANIMATION_ERROR_UNKNOWN_NAME.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AnimationPreview
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AnimationPreview>, Zenith_AnimationError> Create(
		const Zenith_AnimationPreviewDesc& xDesc);
	Zenith_Result<void, Zenith_AnimationError> SetParameter(uint64 ulNameHash, float fValue);
	Zenith_Result<void, Zenith_AnimationError> SetTrigger(uint64 ulNameHash);
	Zenith_Result<void, Zenith_AnimationError> PlaySlot(uint64 ulSlotHash, uint32 uClip, float fBlendIn,
		float fBlendOut);
	Zenith_Result<void, Zenith_AnimationError> SetClipTime(float fSeconds);   // single-clip previews
	void Advance(float fSeconds);                                     // one evaluation; seconds clamped to [0, 1]
	void Reset();
	Zenith_ConstModelPoseView GetModelPose() const;
	Zenith_Span<const Zenith_AnimNotifyEvent> GetEvents() const;     // fired by the last Advance
	Zenith_DualQuaternion GetRootMotion() const;                   // of the last Advance
	Zenith_Result<void, Zenith_AnimationError> GetDebugState(Zenith_AnimDebugState& xOut) const;
};
```

The preview runs the same evaluator as `Animation.Evaluate` ("Animation.Evaluate"), with every clip resident, no IK goals, no world and no streaming, so `Test_AnimationPreview_MatchesWorldEvaluation` can compare a preview with a world instance bit for bit.

## Zenith_AnimationMotionMatch header

`Zenith_MotionMatcher` is the type D-SEL-MOTIONMATCH (open, Stage 3) names as its Noun (`Zenith2/Docs/DecisionLog.md → "D-SEL-MOTIONMATCH"`). It is public because three users outside one ticket need it: the Stage 3 harness (`Zenith2/Benchmarks/MotionMatching/`, `Z2-R-ANIM-178`), the `anim.graph` cook step that writes `ANIM_SECTION_GRAPH_MOTION_DATABASE`, and node kind 15 (`Z2-R-ANIM-179`, "Motion matching node" of `Zenith2/Docs/Design/ANIM/Graph.md`). The header and its search land with the harness ticket; the node's tickets are blocked by the ruling ticket.

```cpp
// Zenith2/Engine/ZenithAnimation/Zenith_AnimationMotionMatch.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithScene/Zenith_Transform.h"
#include "ZenithAnimation/Zenith_AnimationErrors.h"

class Zenith_SkeletonAsset;
class Zenith_AnimationClip;

inline constexpr uint32 uANIM_MOTION_MATCH_FEATURES = 27;     // the feature vector of "Motion matching node"
inline constexpr uint32 uANIM_MOTION_MATCH_BOX_FRAMES = 16;   // frames per pruning box
inline constexpr uint32 uANIM_MOTION_MATCH_NO_FRAME = 0xFFFFFFFF;

/// @contract value Zenith_MotionMatchFrameRef
/// Ownership: Plain value; one per database frame.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the database's frame array, which the build fills in
///   place.
/// Invalidation: None.
/// Units: m_fTime seconds into clip m_uClip (an index into the database's clip list).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MotionMatchFrameRef
{
	uint32 m_uClip = 0;
	float m_fTime = 0.0f;
};

/// @contract value Zenith_MotionMatchDatabaseView
/// Ownership: Non-owning view of a database's arrays: a Zenith_MotionMatchDatabase's, or the decoded graph section's.
/// Thread affinity: IMMUTABLE while the owner lives.
/// Invalidation: Invalid when the owner is destroyed.
/// Units: Features are normalised ((raw − mean) / stdDev per dimension); boxes bound the normalised features.
/// Errors: None; Zenith_MotionMatcher asserts the spans' lengths agree (F frames × 27, B = ceil(F / 16) boxes × 27).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MotionMatchDatabaseView
{
	const Zenith_Span<const float> m_afFeatures{};                 // F × 27, frame-major
	const Zenith_Span<const float> m_afMean{};                     // 27
	const Zenith_Span<const float> m_afStdDev{};                   // 27, each ≥ 1e-6 (a constant dimension stores 1)
	const Zenith_Span<const Zenith_MotionMatchFrameRef> m_axFrames{};   // F
	const Zenith_Span<const float> m_afBoxMin{};                   // B × 27
	const Zenith_Span<const float> m_afBoxMax{};                   // B × 27
};

/// @contract value Zenith_MotionMatchWeights
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Non-negative factors per feature group (feet positions 6, foot velocities 6, hips velocity 3, trajectory
///   positions 6, trajectory facings 6), in that order.
/// Errors: None; negative values are clamped to 0.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MotionMatchWeights
{
	const float m_afGroup[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
};

/// @contract value Zenith_MotionMatchResult
/// Ownership: Plain value returned by Search.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_fCost the weighted squared distance in normalised units.
/// Errors: None; m_uFrame is uANIM_MOTION_MATCH_NO_FRAME only for an empty database.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MotionMatchResult
{
	const uint32 m_uFrame = uANIM_MOTION_MATCH_NO_FRAME;
	const float m_fCost = 0.0f;
	const uint32 m_uBoxesTested = 0;
	const uint32 m_uFramesTested = 0;
};

/// @contract value Zenith_MotionMatchBuildDesc
/// Ownership: Plain value; the span is borrowed for Build.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Joint indices of the skeleton; Hz.
/// Errors: Validated by Build.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_MotionMatchBuildDesc
{
	uint16 m_uLeftFoot = 0;
	uint16 m_uRightFoot = 0;
	uint16 m_uHips = 0;
	float m_fSampleRateHz = 30.0f;
};

/// @contract value Zenith_MotionMatchSourceClip
/// Ownership: Plain value; the spans are borrowed for BuildFromFrames.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: Hz and seconds; local transforms in metres; the motion track Q_k in the clip's root-motion space.
/// Errors: Validated by BuildFromFrames (m_axLocalFrames.Size() == frames × joints, m_axMotion.Size() == frames).
/// Version: since Z2 Stage 3: it lands with BuildFromFrames in the harness ticket, because ANIM's harness and CHAR's
///   authoring-cost harness (row CH15) build databases from source frames before the D-SEL-MOTIONMATCH ruling.
/// @endcontract
struct Zenith_MotionMatchSourceClip
{
	Zenith_Span<const Zenith_Transform> m_axLocalFrames;    // F × J, frame-major: the in-place local pose of each frame
	Zenith_Span<const Zenith_Transform> m_axMotion;         // F: Q_k (Q_0 the identity), scale 1
	float m_fFrameRateHz = 30.0f;
	float m_fDurationSeconds = 0.0f;
	bool m_bLoop = false;
};

/// @contract type Zenith_MotionMatchDatabase
/// Ownership: Created by Build as a Zenith_Owned; owned by the harness, the cook step or a test; one block from the heap passed in.
/// Thread affinity: ANY_THREAD after Build (immutable).
/// Invalidation: Views are valid while the database lives.
/// Units: As Zenith_MotionMatchDatabaseView.
/// Errors: Build returns ANIMATION_ERROR_SKELETON_MISMATCH (a clip of another skeleton), ANIMATION_ERROR_INVALID_ASSET (a
///   foot or hips index out of range, no clip, a clip shorter than its excluded tail), ANIMATION_ERROR_LIMIT_EXCEEDED (more
///   than 1,048,576 frames), ANIMATION_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 3. Deterministic: the same skeleton, clips (in order) and desc give bit-identical arrays.
/// @endcontract
class Zenith_MotionMatchDatabase
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_MotionMatchDatabase>, Zenith_AnimationError> Build(
		const Zenith_SkeletonAsset& xSkeleton, Zenith_Span<const Zenith_AnimationClip* const> axClips,
		const Zenith_MotionMatchBuildDesc& xDesc, Zenith_Heap& xHeap);
	// The cook-side build: the same core over source frames (the anim.graph step's path, "Motion matching node" of Graph.md),
	// since Z2 Stage 3 like the whole header (the Stage 3 harnesses call it; row CH15);
	// auParents is the skeleton's published parents property (J entries, joint 0's 0xFFFF).
	static Zenith_Result<Zenith_Owned<Zenith_MotionMatchDatabase>, Zenith_AnimationError> BuildFromFrames(
		Zenith_Span<const uint16> auParents, Zenith_Span<const Zenith_MotionMatchSourceClip> axClips,
		const Zenith_MotionMatchBuildDesc& xDesc, Zenith_Heap& xHeap);
	Zenith_MotionMatchDatabaseView GetView() const;
	uint64 GetResidentBytes() const;
};

/// @contract type Zenith_MotionMatcher
/// Ownership: Plain value holding a database view and weights; owns nothing.
/// Thread affinity: ANY_THREAD (Search is const and pure).
/// Invalidation: Invalid when the viewed database is destroyed.
/// Units: As Zenith_MotionMatchDatabaseView.
/// Errors: ZENITH_ASSERT on a query span that is not 27 long; non-finite queries are refused by the caller (the node counts
///   them and keeps its current frame).
/// Version: since Z2 Stage 3. Bit-identical per platform and build.
/// @endcontract
class Zenith_MotionMatcher
{
public:
	Zenith_MotionMatcher(const Zenith_MotionMatchDatabaseView& xDatabase, const Zenith_MotionMatchWeights& xWeights);
	// The raw query (27 values in the feature order) normalised by the database's mean and deviation.
	void NormaliseQuery(Zenith_Span<const float> afRaw, Zenith_Span<float> afOut) const;
	// The best frame for a normalised query; uCurrentFrame is the frame the instance plays (uANIM_MOTION_MATCH_NO_FRAME: none).
	Zenith_MotionMatchResult Search(Zenith_Span<const float> afQuery, uint32 uCurrentFrame) const;
	// The unpruned reference Search must equal: every frame, the same ties.
	Zenith_MotionMatchResult SearchBruteForce(Zenith_Span<const float> afQuery, uint32 uCurrentFrame) const;
};
```

<!-- elaboration -->
**BuildFromFrames and Build.** `BuildFromFrames` is the core: each source clip is sampled as the runtime sampler samples a clip (linear translation and scale, `Zenith_Nlerp` rotation, at the clip's own frame timing; the motion track the same way, "Root motion sampling" of `Zenith2/Docs/Design/ANIM/Graph.md`), so a database built at cook from the frames `anim.rawClip` yields after extraction and closure equals, frame for frame, the database the same frames give at run time. `Build` decodes each runtime clip at its own frames into a temporary frame array (`Zenith_SampleClip` at each frame time, every segment resident, and its root-motion section) and calls the same core, so `Build` over clips cooked at index 18 equals `BuildFromFrames` over their decoded frames bit for bit (`Test_MotionMatch_DatabaseCookInputs`). `BuildFromFrames` returns `ANIMATION_ERROR_INVALID_ASSET` for a span whose size disagrees with its clip's frame count, a parent index not below its joint, or the errors `Build` returns. The core, for each clip in order, takes every frame at `m_fSampleRateHz` from time 0 up to `D − 1 s` for a non-loop clip (the last second has no full future trajectory) and up to `D` exclusive for a loop clip (the trajectory wraps): sample the clip as above, compose model space, and write the 27 raw features of "Motion matching node" of `Zenith2/Docs/Design/ANIM/Graph.md`: velocities by the central difference over one frame interval (forward at a clip's first frame), the trajectory from `Zenith_ComputeRootMotionDelta` to each future time expressed in the frame's motion space. Then mean and standard deviation per dimension in `double` over every frame (a deviation below 1e-6 stores 1), normalise every feature, and compute each 16-frame box's per-dimension minimum and maximum. O(F × J).

**Search.** The cost of frame f is `c(f) = Σ_d w(d) × (q_d − x_{f,d})²`, summed in dimension order in `float`, where `w(d)` is the weight of d's group. Boxes are visited in ascending order; a box whose lower bound `Σ_d w(d) × dist(q_d, [min_{b,d}, max_{b,d}])²` (0 inside the interval) is greater than the best cost found so far is skipped, otherwise its frames are scored in ascending order and a frame replaces the best only when its cost is strictly lower (ties go to the lower frame index). Then the **continuation** `f_c = uCurrentFrame + 1`, when it exists and belongs to the same clip, wins when `c(f_c) ≤` the best cost. `SearchBruteForce` scores every frame in ascending order with the same comparisons, so the two return the same frame and the bit-identical cost (the box bound never exceeds the cost of any frame inside it). O(B + frames of unpruned boxes).

## Asset kind loaders

The module start registers three `Zenith_AssetKindLoader` implementations with `Zenith_AssetRegistry` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader"`): `Animation.SkeletonLoader` (format 1), `Animation.ClipLoader` (format 1) and `Animation.GraphLoader` (format 1), in `Internal/Assets/`. None is GPU resident.

| Loader | BeginLoad | OnSectionsLoaded | Activate |
|---|---|---|---|
| skeleton | request every section into ZenithAssets' buffers | validate SV1–SV8 (and the hashes), allocate one block, copy, recompute the rest model in f32 | the `Zenith_SkeletonAsset*` |
| clip | request the header set and, for a resident clip, every segment | validate CV1–CV8 (CV2 is checked at bind, when the skeleton is known), decode each segment's bit-rate table, segment ranges and track prefix offsets into `Zenith_DecodedClipSegment` records, copy the bit streams | the `Zenith_AnimationClip*` |
| graph | request every section | validate GV1–GV5, decode the tables; the clips and the skeleton are the graph's catalogue hard dependencies, so they are ACTIVE or FAILED with a fallback before `BeginLoad` ([Z2-N-ARCH-951], `Zenith2/Docs/Design/ASSET/Loading.md → "Dependency loading"`) | the `Zenith_AnimationGraphAsset*` |

`Cancel` releases the partial block and acknowledges; `Unload` frees the block; `DropCpuCopies` returns 0 (no GPU-resident data). Every validation failure is `Fail(code)` with a `Zenith_AnimationError` value as the code, logged under `anim` with the asset GUID and the rule; `SetResidentBytes(MEMORY_SUBSYSTEM_SIMULATION, bytes, 0)` reports the block (kinds 7–9 are SIMULATION, `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`). `BeginLoad` returns within 1 ms; decoding is O(file bytes).

## Asset bindings

An animator's assets reach simulation only at a tick boundary, in completion-ID order, so a load's completion time never changes a simulation result (Z2-D-053, `Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"`). Phase nodes never call `Zenith_AssetRegistry::Resolve`.

1. **Request.** `Animation.Significance` collects each outstanding need (a binding or prefetch asset of an instance with no request record and no binding-cache entry; a chunk in a window with no request) into the collected-needs array of `Zenith.Animation.Residency` ("Resource partition" of "Zenith_AnimationWorld header"), in ascending simulation ID and, within an instance, in (graph, slot clips in slot order, prefetch clips) order. Completion IDs are allocated only on the world's merge chain or in the frame prologue ([Z2-N-SCRIPT-341]), so `Animation.Significance` declares `ConsumesMergeCapacity` and gets a merge node (`Zenith2/Docs/Design/ECS/Scheduling.md → "Merge nodes and the merge chain"`); its merge function (`m_pfnMerge`), single-threaded, reaches the array through `Zenith_MergeContext::GetUserData`, takes the collected needs in that order and, for each binding need while `m_uBindingLoadsInFlight < animation.bindingQueueCapacity`, gets a completion ID from the world's publication sink for source kind `ASYNC_SOURCE_ANIMATION` (registered as value 9 in `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`, [Z2-N-SCRIPT-925]; the sink is the assumption pending MOD, row AN1), records the request and issues it as `Zenith_AssetLoader::RequestLoad` with the world's completion queue `anim.loads.w<tag>` (capacity `animation.bindingQueueCapacity`, default 1,024) and the completion ID as the user tag; then it issues the chunk reads step 4 of "Animation.Significance" collected, each while `m_uChunkReadsInFlight < animation.streaming.maxReadsInFlight` ("Streaming state machine"). **In-flight bound.** AttachWorld declares the source through the sink with `uMaxInFlight = animation.bindingQueueCapacity + animation.streaming.maxReadsInFlight` and `uMaxResultBytes = 24` (row AN1); the two counters of the residency count each completion ID from its allocation until its publication at a tick boundary (a parked result counts), and a binding load counts until both its publication and the drain of its ASSET completion record (an abandoned load's late `CANCELLED` record included), so the completion queue, whose capacity equals the binding bound, can never overflow. A need beyond its counter, or one for which the sink's allocation fails with `ASYNC_TABLE_ERROR_FULL` (unreachable while the counters hold, handled all the same), is not issued and allocates no ID: a binding need stays outstanding and is collected again by the next run in its order (the instance stays `BINDING`, counted `anim.bindings.deferred_delta`), and a chunk need releases its byte reservation and defers (`anim.stream.deferred_delta`). The counters change only on the merge chain and at tick boundaries, so which needs issue, and so every completion ID, is a function of the collected list and committed state alone, which no chunk layout or worker count changes; `Animation.Evaluate` runs after the merge node because it declares `ReadResource Zenith.Animation.Residency`, whose declared writer is the merge node (ECS's rule 3, version threading). A need whose asset is already in the world's **binding cache** (assets published bound in this world and still referenced by an instance) binds immediately with no request and no completion ID; when the need is an instance's graph, `Animation.Significance` allocates the instance block at that point ("Pose store layout"), in ascending simulation ID, and bumps the block generation so that `Animation.Evaluate` initialises the block's contents before its first evaluation ("Animation.Evaluate" step 0); an allocation failure makes the instance `DISABLED` as at a publication.
2. **Drain.** Each `Animation.Significance` run drains the completion queue into its node arena and parks each completion of a live request with the sink: `SUCCEEDED` as `PUBLICATION_KIND_GENERIC_RECOMPUTED` with no payload, `FAILED` as `PUBLICATION_KIND_GENERIC_RECORDED` with ASSET's 24-byte load result record. A completion record whose request was abandoned (step 4), `CANCELLED` or otherwise, is dropped: its completion ID was already completed with the abandoned payload, and only the binding counter's drain half is released.
3. **Publish.** At the next tick boundary, step 1, ANIM's publication handler receives each publication in completion-ID order, removes its request record and releases the request's unit of its in-flight counter ("Request" above), and, on success, resolves the asset (a publication step may call `Resolve`), validates the binding (graph and skeleton GUIDs, rest pose hashes, CV2, GV6), allocates the instance's block on first binding and initialises its contents there (parameter defaults, queued commands, graph reset; it records the block generation as applied, "Pose store layout"), creates the asset's binding-cache entry, which for a clip holds the per-world interned event names (one `Zenith_StringID` per event, interned into the world's string table from `GetEventName`, in event order, the array the event emitter reads; the clip itself is never written, because it is shared by every world and every world ticks concurrently), and sets `ANIMATOR_STATUS_BOUND`; on failure or mismatch, `ANIMATOR_STATUS_BINDING_FAILED` with one log line (`anim`: the entity, the asset, the error). A replay supplies the recorded publication ticks and the requester holds the tick until its local load completes, as ASSET's rule states.
4. **Release and abandonment.** An instance's binding references drop when the instance is destroyed or rebinds; an asset leaves the binding cache when no instance references it, at the end of `Animation.Significance`. A request whose requester is gone (the instance destroyed, or rebound to another graph, while the load is in flight) or whose chunk ANIM no longer wants is **abandoned**: the request's completion ID is completed exactly once by ANIM, the producer that allocated it, as [Z2-N-SCRIPT-931] (core-doc item X255) and GAME's row GA19 require ("every completion ID allocated from a world's `Zenith_AsyncResultTable` is completed exactly once by the producer that allocated it"): in the same `Animation.Significance` run, in ascending completion ID, ANIM calls `RequestCancel` on the operation handle (ASSET's cancellation, `Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetLoader"`) and completes the ID through the sink as `PUBLICATION_KIND_GENERIC_RECORDED` with the zero-length "abandoned" payload (SAVE's convention, `Zenith2/Docs/Design/SAVE/Formats.md → "Async section"`, which every decoder maps to a failure); the request record stays in the request table, marked abandoned and with no requester, until its publication releases it and its counter unit; ANIM ignores ASSET's subsequent completion record of that operation (step 2), never re-issues an abandoned request under its ID (a need that arises again is a new request with a new ID), and counts no discard itself; a request already parked when its requester goes is complete, so it is only marked abandoned and its publication is dropped. A request of an instance destroyed at a sync point by `LIFECYCLE_EVENT_DESTROYED` is moved to the abandoned list by the listener and completed by the next `Animation.Significance` run. The publication handler drops a publication whose request record is absent or abandoned or whose requester no longer exists (releasing the record and its counter unit), and maps a zero-length `GENERIC_RECORDED` payload to a load failure of that request.

A change of `Zenith.Animation.Animator.Graph` (a command, a load) is seen by `Animation.Significance` as a new need: the instance keeps evaluating its old graph until the new one publishes, then rebinds with "Graph hot reload" semantics when the skeleton is unchanged and resets otherwise.

## Animator instances

### Instance table

`Zenith_AnimationResidency` (the world resource `Zenith.Animation.Residency`) holds a table of instances sorted by simulation ID (`Zenith_Array` of `{ uint64 simulation ID; Zenith_AnimInstance* instance }`, `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_Array"`), at most `animation.maxInstances` (default 4,096, range 1 … 16,384). `Animation.Significance` reconciles it each run: it walks the animator query's rows, collects the simulation IDs that have no instance (new) and marks the ones seen, sorts the new IDs, creates their instances in ascending order and destroys every unmarked instance; creations beyond the cap are refused in ascending simulation ID order and counted (`anim.instances.refused_delta`), so which animators run never depends on chunk layout. Lookups are binary searches, O(log N).

### Instance lifecycle state machine

| State | Event | Next | Action |
|---|---|---|---|
| — | an animator row with `Enabled` and a graph appears | `BINDING` | create the instance record (no pose memory yet); issue the graph need |
| `BINDING` | the graph publishes success and validates | `BOUND` | allocate the instance block; set the parameter defaults; apply queued commands; reset the graph |
| `BINDING` | the graph publishes failure or mismatch | `BINDING_FAILED` | log once; the extractor writes the skeleton's rest pose when the skeleton is bound, nothing otherwise |
| `BOUND` | `Graph` changes | `BOUND` (old graph) until the new one publishes | issue the new need ("Asset bindings") |
| `BOUND` | allocation of the instance block fails at binding | `DISABLED` | log `anim.instance_alloc_failed_delta`; no pose; retried only when the component changes |
| `BINDING` | `Graph` changes while the graph load is in flight | `BINDING` | abandon the in-flight request (its completion ID completed with the abandoned payload, "Asset bindings" step 4) and issue the new need |
| `BINDING_FAILED` | `Graph` changes | `BINDING` | issue the new need |
| any | `Enabled` becomes false | `DISABLED` | keep the state block; evaluation and events stop; the extractor writes the last final pose |
| `DISABLED` | `Enabled` becomes true | `BOUND` or `BINDING` | resume (a reset when `ANIMATOR_FLAG_RESET_ON_ENABLE`) |
| any | the row disappears (component removed or entity destroyed) | — | abandon every in-flight request of the instance ("Asset bindings" step 4), then release bindings and memory at the end of `Animation.Significance` (a `DESTROYED` lifecycle event in the same tick releases at once on the sync-point node and moves the requests to the abandoned list, which the next `Animation.Significance` run completes) |

`Test_AnimInstance_LifecycleTransitions` drives every row of the table.

### Pose store layout

An instance block is one general-heap allocation (`Zenith_Heap`, `Zenith2/Docs/Contracts/Memory.md → "Zenith_Heap"`), allocated and released by the owners of `Zenith.Animation.Residency` ("Resource partition" of "Zenith_AnimationWorld header"), 16-byte aligned regions in this order, each region belonging to the resource its writers write:

| Region | Size | Resource | Written by |
|---|---|---|---|
| parameters | 4 × P, then P trigger counters, padded to 16 | `Zenith.Animation.World` | command playback, `Animation.Evaluate` (consumption, decrement) |
| node state | the graph's instance memory bytes | `Zenith.Animation.World` | `Animation.Evaluate` |
| inertial buffer | 48 × J + 4 × C + 16, padded to 16, when the bound graph `Inertialises()`, else 0 (one per instance, "Inertial blending" of `Zenith2/Docs/Design/ANIM/Graph.md`) | `Zenith.Animation.World` | `Animation.Evaluate` |
| pose history (current: the latest evaluation's inertialised output, before motion warping and IK; previous: the one before) | 2 × 40 × J | `Zenith.Animation.World` | `Animation.Evaluate` |
| animated local pose (the latest evaluation's output after the IK stage, the input of `Animation.PostPhysicsBlend`) | 40 × J | `Zenith.Animation.World` | `Animation.Evaluate` |
| animated model pose | 64 × J | `Zenith.Animation.World` | `Animation.Evaluate` |
| animated curves | 4 × C | `Zenith.Animation.World` | `Animation.Evaluate` |
| final local pose (current, previous) | 2 × 40 × J | `Zenith.Animation.FinalPoses` | `Animation.PostPhysicsBlend` |
| final model pose | 64 × J | `Zenith.Animation.FinalPoses` | `Animation.PostPhysicsBlend` |
| final curves (current, previous) | 2 × 4 × C | `Zenith.Animation.FinalPoses` | `Animation.PostPhysicsBlend` |
| slot state | 32 × slot count (the clip binding handles beside it in the residency record) | `Zenith.Animation.World` | command playback, `Animation.Evaluate` |
| IK state | 64 × IK step count (spring states, previous goals) | `Zenith.Animation.World` | `Animation.Evaluate` |
| events | 64 × 48 (this evaluation's fired events, for the extractor's debug view and the preview) | `Zenith.Animation.World` | `Animation.Evaluate` |

At J = 60, C = 0, an inertialising graph, 8 parameters, 1 slot and 4 IK steps the block is 25,984 bytes (about 25 KiB) plus the graph's node state, so W-Characters' 100 instances hold about 2.5 MiB, within `Z2-B-ANIM-001` (pending). The instance record beside the block is 512 bytes in three parts, one per resource: the **residency record** (`Zenith.Animation.Residency`: simulation ID, entity, status, the block pointer and block generation, the reset serial, binding handles, significance and rate state, queued commands), the **evaluation record** (`Zenith.Animation.World`: the applied block generation and applied reset serial, `m_fLastDeltaSeconds` (the Δt of the evaluation that produced the current history pose, which inertial takes read), the pending inertial take (`m_bInertialTakePending`, `m_fInertialTakeSeconds`), root-motion spread state, curve overrides) and the **post-physics record** (`Zenith.Animation.FinalPoses`: the ragdoll recovery state). The block is allocated where an instance binds: in the publication handler (which initialises its contents at once), or in `Animation.Significance` for a binding-cache hit ("Asset bindings" step 1), which bumps the block generation and leaves the initialisation to `Animation.Evaluate`; both are single nodes that bind in ascending simulation ID. The ragdoll entry pose PHYS's enter playback reads through `GetEntryModelPose` is the final model pose itself, which no node writes between the latest phase 6 and the playback's sync point ("Ragdoll enter"), so the block has no entry-pose region.

## Systems

The module registers these frame-graph nodes per world in `AttachWorld` (`Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_System header"`); all are `SYSTEM_KIND_NATIVE` with `JOB_AFFINITY_ANY`:

| Node | Phase | Writer source | Declared access | Cost hint (µs) |
|---|---|---|---|---|
| `Animation.Significance` | 3 (Animation) | — | `ConsumesMergeCapacity` (its merge function allocates the completion IDs and issues the requests, "Asset bindings" step 1; the merge node is the declared writer of `Zenith.Animation.Residency`); `Read` `Zenith.Animation.Animator` (all), `Zenith.Physics.Ragdoll` (1 State, own row where present: the forced full rate, "Rate tiers and phase offsets"; assumption pending PHYS, row AN6); `ReadAnyRow` `Zenith.Render.Camera` (1 Active, 2 Priority, 11 Kind) and `Zenith.WorldTransform` (1–3); `ReadResource` `Zenith.Animation.World` (the previous evaluation's clip times for the prefetch windows and the applied block generations: a pre-reader, so it runs before `Animation.Evaluate` writes it); `WriteResource` `Zenith.Animation.Residency`; one query (`Animation.Significance/Animators`: `WITH Zenith.Animation.Animator`, `WITH Zenith.WorldTransform`, `OPTIONAL Zenith.Physics.Ragdoll`) | 50 |
| `Animation.Evaluate` | 3 | — | `After("Animation.Significance")`; `Read` `Zenith.Animation.Animator`, `Zenith.Animation.IKGoals`, `Zenith.Animation.MotionWarp`, `Zenith.WorldTransform` (own row), `Zenith.LocalTransform` (3 Scale, own row: the root-motion delta's scale, "Animation.Evaluate" step 7); `ReadAnyRow` `Zenith.WorldTransform` (IK and warp target entities); `ReadResource` `Zenith.Animation.Residency` (the post-read that orders it after the merge node of `Animation.Significance`: the instance table, rate decisions, the binding and stream caches), `Zenith.Physics.World` (foot placement queries, Stage 4; assumption pending PHYS, row AN6); `Write` `Zenith.Animation.RootMotion` (1–3); `WriteResource` `Zenith.Animation.World`; `EmitsEvent` `Zenith.Animation.Notify`, `Zenith.Animation.StateChanged` | 600 |
| `Animation.ApplyRootMotion` | 4 (Movement arbitration) | — | `EmitsCommands` (so it gets a merge node, `Zenith2/Docs/Design/ECS/Scheduling.md → "Merge nodes and the merge chain"`); `Read` `Zenith.Animation.RootMotion` (1–3), `Zenith.Animation.Animator` (3 RootMotion), `Zenith.LocalTransform` (1–2, own row); records `SetProperty` commands on `Zenith.LocalTransform` (1 Position, 2 Rotation); one query (`Animation.ApplyRootMotion/Movers`: `WITH Zenith.Animation.RootMotion`, `WITH Zenith.Animation.Animator`, `WITH Zenith.LocalTransform`, `WITHOUT Zenith.Physics.CharacterController`, assumption pending PHYS, row AN6); no ordering edge ("Animation.ApplyRootMotion") | 30 |
| `Animation.PostPhysicsOwnership` | 6 (Post-physics) | `WRITER_SOURCE_ANIMATION` | `Write` `Zenith.Animation.Pose` (1 PhysicsWeight) (a staging write, which the arbitration node reads) | 10 |
| `Animation.RagdollOwnership` | 6 | `WRITER_SOURCE_RAGDOLL` | `After("Physics.RagdollBlend")` (PHYS's phase-6 node that advances the blend weight, assumption pending PHYS, row AN6), so it reads this tick's weight with no one-tick lag; `Read` `Zenith.Physics.Ragdoll` (1 State, 2 BlendWeight); `Write` `Zenith.Animation.Pose` (1) (a staging write) | 10 |
| `Animation.PostPhysicsBlend` | 6 | — | `After("Physics.RagdollBlend")` (assumption pending PHYS, row AN6); `Read` `Zenith.Animation.Pose` (1), `Zenith.Physics.Ragdoll` (1 State, 2 BlendWeight: the recovery fraction, "Ragdoll recover" step 4), `Zenith.LocalTransform`, `Zenith.WorldTransform`; `ReadResource` `Zenith.Animation.World`, `Zenith.Animation.Residency`, `Zenith.Physics.World` (the ragdoll body poses); `WriteResource` `Zenith.Animation.FinalPoses` | 200 |
| `Animation.ExtractPoses` (extractor, `Zenith_WorldAttachContext::AddExtractor`) | 12 | — | "Extractor declared reads" | 150 |

The schedule inserts `Arbitrate.Zenith.Animation.Pose.PhysicsWeight` between the two ownership writers and `Animation.PostPhysicsBlend` (`Zenith2/Docs/Design/ECS/Scheduling.md → "Arbitration nodes"`); CINE's cinematic writer of the same property (`WRITER_SOURCE_CINEMATIC`) joins it when the Cinematics module is present (row AN9). No ANIM system names a generated `Arbitrate.*` node in a `Before` or `After`: ECS creates an arbitration node only for an arbitrated (property, phase) with at least one writer, so `Arbitrate.Zenith.LocalTransform.Scale` exists in phase 6 only with CINE's writer, and a required ordering on an absent node is `SCHEDULE_ERROR_ORDERING_TARGET_MISSING`. The orderings follow from the declarations instead (`Zenith2/Docs/Design/GRAPH/FrameGraphInternals.md → "Step 5 version threading"`): each ownership writer's staging write is a pre-write the arbitration node reads, so the writers run before it, and `Animation.PostPhysicsBlend`'s `Read` of `Zenith.Animation.Pose` (1) and of `Zenith.LocalTransform` makes it a post-reader of every arbitration node that exists for those properties in phase 6. The only explicit edges ANIM's phase-6 systems declare are `After("Physics.RagdollBlend")`, a system that exists in every world that attaches the module (ANIM requires Physics).

In phase 3 the order is `Animation.Significance` → its merge node → `Animation.Evaluate` → `Animation.Evaluate`'s merge node: `Animation.Evaluate` post-reads `Zenith.Animation.Residency`, whose one writer in the stage is the first merge node, and `Animation.Significance` pre-reads `Zenith.Animation.World`, whose one writer in the stage is the second; each ANIM world resource has exactly one writer node per stage. `Test_AnimationModule_ScheduleCompiles` (`Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios"`) freezes and compiles a world with Physics and Animation, without and with Cinematics, and asserts both.

<!-- elaboration -->
**Counter and statistics merge.** A node that fans out `ParallelFor` leaves (`Animation.Evaluate`, `Animation.ApplyRootMotion`, `Animation.PostPhysicsBlend`, the extractor) gives each leaf one fixed-size counter record in the node arena (the members of `Zenith_AnimationWorldStats` that node owns, and its `_delta` counters); after the last leaf completes, the node's own body sums the records in leaf-index order and publishes the counters once, so neither a counter nor `GetStats` depends on worker count or completion order. **Shared caches and once-only reports.** No leaf mutates per-world state another leaf can see: every shared cache (the extractor's morph remap tables) is built, stamped and evicted by the node's own body before `ParallelFor` and only read by the leaves ("Morph remap"); and every report a node makes once per key (a refusal counted once, a log line once per (entity, asset, cause)) is appended by the leaf to a report list in its own record, sized at the leaf's chunk row count in the node arena (each row makes at most one such report per node, so the list cannot overflow), and after the last leaf the node's body walks the records in leaf-index order and, for each report whose key is not yet in the per-world "already reported" set, inserts the key, counts and logs it, so the first report of a key is the same one whatever the worker count. The set is written only there, single-threaded, and is bounded by `uANIM_MAX_REPORTED_KEYS` (4,096 per world; when it is full, further keys count without logging).

### Animation.ApplyRootMotion

The phase-4 application of root motion to the entities that have no physics character controller (the orchestrator's Q72 ruling; PHYS's `Physics.CharacterMove` applies it, through collision, to the entities that have one, row AN6). One node fanning out `ParallelFor` over the chunks of `Animation.ApplyRootMotion/Movers`, grain 1 chunk, each leaf recording into its own command buffer through `ForLeaf` ([Z2-N-ARCH-718]). For each row whose `Zenith.Animation.RootMotion.Flags` has bit 0 (valid) set and whose animator mode is `ANIMATOR_ROOT_MOTION_EXTRACT`:

1. Read the delta (`DeltaPosition` Δp, `DeltaRotation` Δq, in the entity's local frame at the start of the tick, "Root motion publication" of `Zenith2/Docs/Design/ANIM/Graph.md`) and the row's `Zenith.LocalTransform` (position p, rotation r, scale s), all committed before phase 4.
2. The new local transform: `p' = p + r · Δp` (Δp is entity-local metres, already scaled by the entity's scale when `Animation.Evaluate` wrote it, "Zenith_AnimationComponents header" of `Zenith2/Docs/Design/ANIM.md`, so only the rotation maps it into the parent's space; PHYS's `Physics.CharacterMove` applies the same `Zenith_Rotate(q₀, DeltaPosition)`, so a scaled character travels the same distance with and without a controller) and `r' = Zenith_Normalize(r · Δq)`; scale unchanged.
3. Record `SetProperty(entity, Zenith.LocalTransform, 1, p')` and `SetProperty(entity, Zenith.LocalTransform, 2, r')`, applied at sync point 4 before phase 5 ([Z2-N-ARCH-483], [Z2-N-ARCH-489]); each recorded row counts one `anim.rootmotion.applied_delta`; a row whose result is not finite records nothing and appends a once-only report keyed by the entity, logged once per entity under `anim` by the node's body after the last leaf ("Counter and statistics merge").

A delta with bit 1 (reset, a teleport this tick) is the identity, so a teleported entity is not moved twice. Phase 6's arbitration of `Zenith.LocalTransform` may overwrite the commanded value only when a phase-6 writer writes that entity (for example a physics body's transform sync); that outcome is deterministic ([Z2-N-ARCH-489]) and is the content's choice (an animated entity that should be moved by root motion through collision carries a character controller). Cost O(rows), no allocation.

**Ordering relative to PHYS's controller node.** `Animation.ApplyRootMotion` declares no `Before` or `After` edge to `Physics.CharacterMove`: the two queries partition the rows by the `WITHOUT`/`WITH Zenith.Physics.CharacterController` filter, so no entity is moved by both; both only read `Zenith.Animation.RootMotion`; and structural changes (adding or removing a controller) apply only at sync points, so the partition is fixed for the whole phase. The schedule compiler's hazard check therefore orders nothing between them and the two run in parallel; an entity that gains a controller between ticks is moved by ANIM up to that tick and by PHYS from the next. PHYS's row AN6 records the matching rule on its side (its node reads root motion only for rows with a controller and records none of ANIM's commands).

### Animation.Significance

Single node, no sub-jobs (it is O(N) and must collect requests in a fixed order), with a merge node (`ConsumesMergeCapacity`) that issues them:

1. Drain the binding and streaming completions and park those of live requests ("Asset bindings" step 2; "Publication"); then complete every completion ID on the abandoned list, in ascending ID, with the abandoned payload ("Asset bindings" step 4).
2. Reconcile the instance table ("Instance table"), handle `Graph` and `Enabled` changes (a resume with `ANIMATOR_FLAG_RESET_ON_ENABLE` bumps the instance's reset serial), abandon the requests whose requester is gone, collect the binding needs.
3. Compute each instance's significance and rate decision for this tick ("Update-rate budgeting").
4. Update every streamed clip's prefetch windows from the instances' previous evaluation (read from `Zenith.Animation.World`), stamp the last-use tick of every chunk in a window, collect the missing chunks in ascending (clip GUID, chunk index) and reserve their bytes in the cache's accounting (so the decisions that follow in this run see them), abandon the in-flight reads of chunks no window wants, evict chunks outside every window ("Clip streaming").
5. Publish counters.
6. The merge node (`Animation.Significance.Issue` zone), which reaches the collected needs through `GetUserData`: in the collected order, allocate a completion ID for each need collected in steps 2 and 4 that its in-flight counter admits, and issue the binding loads and then the chunk reads, each chunk into a block allocated there ("Asset bindings" step 1, "Streaming state machine"); a need its counter does not admit, or that the sink refuses with `ASYNC_TABLE_ERROR_FULL`, is not issued and allocates no ID (a binding need counts `anim.bindings.deferred_delta` and is collected again by the next run; a chunk need releases its reservation and counts `anim.stream.deferred_delta`); a block that cannot be allocated releases its reservation, defers the chunk and counts `anim.stream.deferred_delta`, without allocating an ID. The array is empty when the merge function returns.

### Animation.Evaluate

One node fanning out `Zenith_JobContext::ParallelFor` over the chunks of its query (`Animation.Evaluate/Animators`: `WITH Zenith.Animation.Animator`, `WITH Zenith.Animation.RootMotion`, `WITH Zenith.WorldTransform`), grain 1 chunk; each leaf takes its context through `ForLeaf` ([Z2-N-ARCH-718]). For each row whose instance is `BOUND` and due this tick (rate decision):

0. Apply pending content requests ("Resource partition" of "Zenith_AnimationWorld header"): when the residency record's block generation differs from the evaluation record's applied generation, initialise the block (parameter defaults, then the instance's queued commands in their recorded order, then the graph reset) and record the generation as applied; when the reset serial differs from the applied one, reset the graph and record it. `Animation.Significance` clears a queue whose block generation has been applied, at its next run. An instance due for initialisation is evaluated this tick whatever its rate (its first evaluation forces k = 1).
1. Gather inputs: Δt (the accumulated simulated seconds since the instance's last evaluation, times the animator's play rate), parameters, IK goals and warp targets (own row), the entity's world transform from the previous tick (`Zenith.WorldTransform`, which phase 8 writes), and for IK target entities their world transforms (cross-entity reads of a property not written in phase 3).
2. Run the graph: the update pass, the pose pass ("Evaluation algorithm" of `Zenith2/Docs/Design/ANIM/Graph.md`), curve overrides, into the pose stack; its top is the graph output G. Takes requested in the update pass set the instance's pending take ("Inertial blending").
3. Inertialise ("Inertial blending" of `Zenith2/Docs/Design/ANIM/Graph.md`): a pending take reads the instance's **current** history pose (`x_prev`), its **previous** one and `m_fLastDeltaSeconds`, all still unmodified at this point, and G; an active offset is then applied to G, giving the inertialised pose.
4. Rotate the pose history: previous ← current, current ← the inertialised pose; then `m_fLastDeltaSeconds` ← this evaluation's Δt.
5. Motion warping ("Motion warping"), which corrects this evaluation's root-motion delta.
6. Copy the inertialised pose into the animated local pose, compute model space (`Zenith_LocalToModel`), then run the IK stage on them ("IK stage order"), which writes back local rotations and recomputes the affected model transforms; the history never holds IK, so a subsequent take measures against the pre-IK pose that IK is again applied to.
7. Write the root-motion row ("Root motion publication" of `Zenith2/Docs/Design/ANIM/Graph.md`, "Root motion under reduced rates"): `DeltaPosition` is the model-space delta's translation multiplied component-wise by the row's `Zenith.LocalTransform` scale (its units, "Zenith_AnimationComponents header" of `Zenith2/Docs/Design/ANIM.md`), `DeltaRotation` the delta's rotation; commit the row's change bits through the view.
8. Emit the events in order (`Events(simulationID)`).

An instance not due this tick writes its spread root-motion step (or zero) into its row, scaled as in step 7, and nothing else; a `BINDING`, `BINDING_FAILED` or `DISABLED` instance writes zero root motion. Per-instance work is independent, so the result is identical with any worker count and chunk layout (`Test_AnimDeterminism_WorkerAndLayoutIndependent`).

### Animation.PostPhysicsOwnership

For every row of `Zenith.Animation.Pose`, write `PhysicsWeight = 0` into the Animation staging column, so that when no ragdoll and no cinematic writes, the arbitration result is 0 every tick (a row no source writes would otherwise keep its previous value, [Z2-N-ARCH-495]).

### Animation.RagdollOwnership

For every row with PHYS's ragdoll component whose state is not `NONE`, write PHYS's blend weight `w ∈ [0, 1]` (advanced by PHYS once per tick in phase 6 over `m_fBlendSeconds`, [Z2-N-ARCH-564], [Z2-N-ARCH-565]) into the Ragdoll staging column. The arbitration of `PhysicsWeight` is `ARBITRATION_RULE_PRECEDENCE` with the declared order (Cinematic, Ragdoll, Animation): a cinematic that holds the character wins (its writer writes 0, so the cinematic's animation is the pose), else an active ragdoll, else animation's 0. Ownership of the character's bone transforms therefore moves between the Animation and Ragdoll writer sources through this arbitration rule, as `Zenith2/Docs/Architecture/LifecycleCommands.md → "Ragdoll enter and recover"` requires.

### Animation.PostPhysicsBlend

`ParallelFor` over instances in table order (grain 8), each leaf writing only its own instances' regions and post-physics records of `Zenith.Animation.FinalPoses`. Per instance with `w` = the arbitrated `PhysicsWeight`:

1. Copy current final to previous for the extractor's interpolation only when the instance was evaluated this tick, `w > 0` or a recovery offset is active (otherwise the pair stays the last two evaluated poses, "Pose interpolation").
2. The animated input A: the animated local pose; when a recovery offset is active, A is that pose with the recovery offset applied first ("Ragdoll recover": `Pow(O, 1 − p)` pre-multiplied onto joint 0's local transform, which moves the whole animated pose rigidly).
3. `w = 0`: the final local pose is A, the final model pose `Zenith_LocalToModel(A)` (the animated model pose itself when no offset is active).
4. `w > 0`: read the ragdoll's body transforms and blend A with the ragdoll pose by `w` ("Ragdoll blend"). The offset therefore moves only the animated contribution and the ragdoll's contribution is never offset, so at the first recover tick from ACTIVE (`w = 1`, `p = 0`) the final pose is exactly the ragdoll pose ("Ragdoll recover" step 4, by the recover weight premise of "Ragdoll transitions").

## Lifecycle listener

ANIM's `Zenith_LifecycleListener` (`Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_LifecycleListener header"`), registered per world with `AddLifecycleListener`, interest mask `TELEPORTED | DESTROYED | RAGDOLL_RECOVER | CINEMATIC_INTERRUPT | CINEMATIC_SKIP | CINEMATIC_END`, runs on the sync-point node and writes only ANIM's own instance state (in any of its three world resources, "Resource partition" of "Zenith_AnimationWorld header"):

| Event | Action |
|---|---|
| `LIFECYCLE_EVENT_TELEPORTED` | zero the root-motion spread state and the recovery offset; reset the IK springs to the current pose; set `Zenith.Teleported`; unless `TELEPORT_FLAG_KEEP_ANIMATION_PHASE`, reset the graph (every machine to its entry state, times 0); mark the instance history-reset for the next extraction (no interpolation, `FLUX_SKINNED_POSE_FLAG_HISTORY_RESET`); the ragdoll blend state is kept ([Z2-N-ARCH-551]). `Zenith2/Docs/Architecture/LifecycleCommands.md → "Teleport"` lists these as the animation row |
| `LIFECYCLE_EVENT_DESTROYED` | release the instance at once; move its in-flight requests to the abandoned list, which the next `Animation.Significance` run completes ("Asset bindings" step 4) |
| `LIFECYCLE_EVENT_RAGDOLL_RECOVER` | "Ragdoll recover" |
| `LIFECYCLE_EVENT_CINEMATIC_INTERRUPT` | every cinematic-tagged slot starts its blend-out with its own blend-out time |
| `LIFECYCLE_EVENT_CINEMATIC_SKIP` | every cinematic-tagged slot empties at once (weight 0, binding released) and the instance requests a take of 0.15 s ("Inertial blending" of `Zenith2/Docs/Design/ANIM/Graph.md`), so its next evaluation continues from its last output (no pop at skip, the CinematicSlice sign-off item) |
| `LIFECYCLE_EVENT_CINEMATIC_END` | cinematic-tagged slots blend out normally |

## Ragdoll transitions

<!-- assumption: pending PHYS -->
PHYS's design is in its revision 1 (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md`, `Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md`); until it signs off ANIM states what it relies on, in `ZenithPhysics/Zenith_Ragdoll.h` (row AN6): the component `Zenith.Physics.Ragdoll` with properties `State` (`u8`: 0 none, 1 entering, 2 active, 3 recovering; authority `PHASE_POST_PHYSICS`) and `BlendWeight` (f32, authority `PHASE_POST_PHYSICS`, advanced by PHYS's ragdoll system in phase 6); the interface `Zenith_RagdollPoseSource` that PHYS declares and ANIM implements (`GetJointCount`, `FindJoint(ulSimulationID, ulNameHash)`, `GetEntryModelPose(ulSimulationID, Zenith_Span<Zenith_Matrix4>)`, `GetAnimatedModelPose(ulSimulationID, Zenith_Span<Zenith_Matrix4>)`), installed per world through `Zenith_PhysicsWorldResource::SetRagdollPoseSource`; and the read `Zenith_PhysicsWorldResource::GetRagdollBodyPoses(ulSimulationID, Zenith_Span<Zenith_RagdollBodyPose>)` returning, per body, the bound joint's name hash and the body's world transform, valid in phase 6 after the physics step. PHYS's ragdoll asset names joints by name hash, so PHYS needs no ANIM type. PHYS's phase-6 node `Physics.RagdollBlend` advances `BlendWeight` once per tick, and `Animation.RagdollOwnership` and `Animation.PostPhysicsBlend` run after it; the latter reads `State` and `BlendWeight` for the recovery fraction ("Ragdoll recover" step 4). **Recover weight premise (the orchestrator's PHYS-RECOVER ruling, pending PHYS revision 1, row AN6):** in the first phase 6 after the `Physics.RecoverRagdoll` playback, `BlendWeight` still holds the weight the ragdoll had at the playback (1 from ACTIVE), so ANIM's first `Animation.PostPhysicsBlend` after `LIFECYCLE_EVENT_RAGDOLL_RECOVER` reads it unchanged; PHYS lowers it by `1 / (recover blend seconds × tickHz)`, clamped to 0, from the next tick on; a recover blend of 0 s therefore holds the weight at the first phase 6 and reaches 0 (state NONE) at the second. PHYS's revision 1 text states this schedule (`Zenith2/Docs/Design/PHYS/CharacterRagdollVehicle.md`, "RECOVERING keeps w₀ at k = 1"); the marker stays until that revision signs off. PHYS's playback of `LIFECYCLE_EVENT_RAGDOLL_RECOVER` places the entity before ANIM's listener runs ("Ragdoll recover" step 0). `Zenith_RagdollRecoverDesc::m_xGetUpClip` becomes a `Zenith_PersistentID` (row AN2), because ZenithPhysics cannot name `Zenith_AnimationClip`.
<!-- /assumption -->

### Ragdoll enter

PHYS's playback of `Physics.EnterRagdoll` (the sync point after the issuing phase, [Z2-N-ARCH-552]) creates the bodies from `GetEntryModelPose`, converting model space to world with the entity's world transform. ANIM's pose source returns the instance's final model pose from `Zenith.Animation.FinalPoses`: only `Animation.PostPhysicsBlend` (phase 6) and a restore write it, so between the latest phase 6 and any sync point of the next tick before phase 6 it is unchanged, and it is the pose the character shows. The listener therefore takes no snapshot and does not subscribe to `LIFECYCLE_EVENT_RAGDOLL_ENTER` (PHYS's dispatch of the event before its step 5 needs no ANIM action), and the block has no entry-pose region ("Pose store layout"). `GetEntryModelPose` returns false for an instance with no final pose (never evaluated), and PHYS then refuses the enter as its design states. During the blend (`w` rising from 0 to 1 over the enter blend seconds) the final pose is "Ragdoll blend"'s mix; the animation keeps evaluating underneath, so a recovery can blend back to a live pose.

### Ragdoll blend

1. Read the body poses; convert each body's world transform to the entity's model space for this tick, `Mdl = inverse(W_e) · W_body`, where `W_e` is the entity's world transform composed from its arbitrated `Zenith.LocalTransform` of this phase and its parent's `Zenith.WorldTransform` of the previous tick (a root entity: the local transform alone).
2. For each joint in joint order: a joint with a body takes the body's model transform; a joint without one takes its local transform from the animated input A ("Animation.PostPhysicsBlend" step 2) under its parent's ragdoll-mixed model transform.
3. Convert the ragdoll model pose to local (`Zenith_ModelToLocal`).
4. Final local = `Blend(A, ragdoll local, w)` per joint ("Local-space blend" of `Zenith2/Docs/Design/ANIM/Graph.md`), which is rigid per joint and keeps every shared pivot; then final model by `Zenith_LocalToModel`.

O(J + bodies) per ragdolling instance; ragdoll counts are budgeted inside `Z2-B-ANIM-003`.

### Ragdoll recover

<!-- elaboration -->
At `LIFECYCLE_EVENT_RAGDOLL_RECOVER`:

0. Placement (PHYS's, assumption pending PHYS, row AN6, as PHYS's concurrent draft specifies it): while the ragdoll is ENTERING or ACTIVE, PHYS's phase-6 `Physics.RagdollBlend` writes the entity root's `Zenith.LocalTransform` (writer source Ragdoll) so the root follows the ragdoll's root part (its joint position projected down by the ragdoll definition's root height offset, and the yaw of the root part's forward axis); PHYS's recover playback, which dispatches `LIFECYCLE_EVENT_RAGDOLL_RECOVER` before ANIM's listener runs, captures that root pose, makes the parts kinematic, and holds the root there through RECOVERING. The entity therefore stands under the ragdoll when recovery starts, and the pose-matching offset of step 3 is only the residual between the ragdoll's hips and the get-up's first hips pose, never the distance the ragdoll travelled. Get-up clips are authored with the root under the hips and facing the hips' forward yaw at their first frame.
1. Facing: with the hips joint's (role `HIPS`, else joint 0) final model rotation and the skeleton's model front +Z, the character faces down when the rotated front's world `y` is below −0.25, up otherwise; set `Zenith.RagdollFacing` and the trigger `Zenith.RagdollRecover`, which the graph's recovery transition consumes.
2. When the recover payload names a get-up clip (`m_xGetUpClip`, a persistent ID), play it in the slot named `Zenith.GetUp` when the graph has one (blend in 0, root motion allowed); otherwise the graph's own states provide the get-up.
3. Pose matching: at the first `Animation.PostPhysicsBlend` after the event, compute the rigid offset `O = H_ragdoll · H_anim⁻¹` between the ragdoll's hips model transform (in the entity's model space after the placement of step 0) and the get-up's animated hips model transform of this tick, as a unit dual quaternion. **Bound:** when O's translation exceeds 0.5 m or its rotation angle exceeds 90° (after sign canonicalisation), the recovery uses no offset (`O` = identity), logs once and counts `anim.ragdoll.recover_offset_rejected_delta`; the ragdoll blend alone then carries the transition.
4. The recovery fraction p comes from PHYS's own weight, never from a clock of ANIM's, so the offset and the weight reach their ends in the same tick whatever PHYS's decrement. `Animation.PostPhysicsBlend` reads the ragdoll's `BlendWeight` `w_r` (`Zenith.Physics.Ragdoll` 2, lowered by `Physics.RagdollBlend` earlier in the same phase 6, row AN6); at its first run after the event it stores `w_1 = w_r`, which by the recover weight premise of "Ragdoll transitions" (pending PHYS revision 1, row AN6) is the weight `w_0` the ragdoll had at the recover playback, exactly 1 for a recovery from ACTIVE; PHYS then lowers it by `1 / (T × tickHz)` per tick for the desc's `m_fBlendSeconds` T ([Z2-N-ARCH-565]); then, every tick while `State` is RECOVERING, `p = 1 − w_r / w_1` (p = 1 when `w_1 = 0`, which applies no offset); a recover blend of 0 s from ACTIVE gives, at the first tick, `w = w_1 = 1`, `p = 0` and a final pose equal to the ragdoll pose (the offset is applied to A, which `w = 1` hides), and at the second tick NONE, the offset state cleared and the final pose equal to the animated pose; when the ragdoll leaves RECOVERING for NONE (its weight reached 0) the offset state clears, and when it re-enters (ENTERING) the offset stays at its last p until the next recover event computes a new O, so no pose jumps either way. The offset is applied to the **animated** pose first, `A = Pow(O, 1 − p) · anim` (pre-multiplied onto joint 0's local transform, a rigid motion of the whole pose along O's screw), and only then blended with the ragdoll pose, `final = Blend(A, ragdoll, w)` ("Animation.PostPhysicsBlend" steps 2 to 4). So at the first recover tick p = 0 and A's hips model transform equals `H_ragdoll` exactly; for a recovery from ACTIVE, `w = w_1 = 1` there, so the final pose is exactly the ragdoll pose; at the tick PHYS's weight reaches 0 (`p = 1`, `w = 0`) the final pose is the animated pose with no residual offset; in between the animated input slides rigidly along one screw motion while the weight moves from the ragdoll to it. The ragdoll's contribution is never offset, so nothing is offset twice.
5. The offset state (O, `w_1`, the last p, active) lives in the instance's post-physics record (`Zenith.Animation.FinalPoses`, written by `Animation.PostPhysicsBlend` and the listener) and is saved; a teleport during recovery zeroes it ("Lifecycle listener").

`Test_RagdollRecover_OffsetBlendKeepsHipsPivot` checks step 4 against a closed-form screw, with the negative control `Test_RagdollRecover_LinearOffsetBlendMovesPivot`; `Test_RagdollRecover_NoDoubleOffsetAtFirstTick` (driven by PHYS's real `Physics.RagdollBlend` weight advance) and `Test_RagdollRecover_LargeOffsetRejectedCounted` check the composition, the fraction's schedule and the bound (`Zenith2/Docs/Design/ANIM/Tests.md → "Ragdoll and lifecycle tests"`).

## Inverse kinematics

IK lands in Stage 4. Goals come from the `Zenith.Animation.IKGoals` component (written by scripts and gameplay in phase 2, or through `SetProperty` commands) and from foot placement; the solvers are pure functions ("Zenith_AnimationIK header") on model-space transforms of the instance's animated pose.

### Two-bone solver

`Zenith_SolveTwoBoneIK` for upper A, middle B, end C, target T, pole direction P:

1. `a = |B − A|`, `b = |C − B|`; `d = |T − A|` clamped to `[|a − b| + 1e-4, a + b − 1e-4]`; with softness s > 0, `d_soft = a + b − s × exp(−(d − (a + b − s)) / s)` when `d > a + b − s`, else d (Perry's soft IK), and the target is moved to `A + normalise(T − A) × d_soft`.
2. The bend plane normal `n = normalise(cross(T − A, P))`; when `|cross| < 1e-6` the current plane `normalise(cross(B − A, C − B))`, and when that too is degenerate the skeleton definition's hint.
3. Law of cosines: `cos α = clamp((a² + d² − b²) / (2 a d), −1, 1)`; the new middle position `B' = A + rotate(normalise(T − A), n, α) × a`; `C' = A + normalise(T − A) × d`.
4. The root rotation is the rotation taking `(B − A)` to `(B' − A)` composed with the twist that keeps the bend in the plane; the middle rotation takes `(C − B)` to `(C' − B')`; with "apply end rotation" the end joint takes the target rotation.
5. Weight w: each output rotation is `Zenith_Slerp(original, solved, w)`.
6. `m_bReached` when `|T − C'| ≤ 1e-3`; the reach error otherwise.

O(1). `Test_TwoBoneIK_ReachesInsideRange`, `Test_TwoBoneIK_ClampsOutOfReach`, `Test_TwoBoneIK_PoleSelectsBendPlane`, `Test_TwoBoneIK_DegenerateInputsStable` cover it.

### FABRIK solver

`Zenith_SolveChainIK` for a chain of n + 1 joint positions with bone lengths `l_i`: when `|T − p_0| ≥ Σ l_i`, stretch the chain straight toward T; otherwise iterate at most `m_uMaxIterations` (10): backward pass (`p_n = T`, then `p_i = p_{i+1} + normalise(p_i − p_{i+1}) × l_i`), forward pass (`p_0` restored, then `p_{i+1} = p_i + normalise(p_{i+1} − p_i) × l_i`), stop when `|p_n − T| ≤ tolerance` (1 mm). `Zenith_ChainPositionsToRotations` then rotates each joint by the shortest arc from its old child direction to its new one, applied root first, so twist is preserved. O(iterations × n), n ≤ 32.

### Look-at solver

`Zenith_SolveLookAt` distributes an aim over up to four joints (spine to head) with shares summing to 1 (the definition's order, default 0.15, 0.2, 0.25, 0.4): for each joint root first, the rotation taking its forward axis toward the target is computed, clamped to the maximum angle (default 70° per joint), scaled by its share (slerp from identity) and applied to that joint and implicitly its descendants; weight blends the result with the input. O(joints).

### Foot placement

Per foot IK step (`FOOT` definition: ankle, toe, leg two-bone definition, ankle height h):

1. Probe: a physics ray from the ankle's world position raised by 0.5 m, straight down 1.0 m, against the foot-probe collision layers (`animation.ik.footProbeLayers`), through `Zenith_PhysicsWorldResource::RayCast` (assumption pending PHYS, row AN6), reading the physics state of the previous tick.
2. A hit gives the ground point g and normal n; the desired ankle height is `g.y + h`; a miss leaves the foot unchanged.
3. The pelvis offset is the most negative foot offset (so the lower foot reaches its ground), clamped to [−0.5, +0.2] m, smoothed by a critically damped spring with a half-life of 0.05 s using the tick's fixed Δt (deterministic).
4. Each leg's two-bone solve reaches its adjusted ankle target; the ankle rotation then turns by the shortest arc from +Y to n, limited to 30°.

### IK stage order

After the layers and motion warping, the graph's IK steps run in their record order: foot placement first when present (it moves the pelvis), then two-bone and chain goals, then look-at. Each step reads the model transforms the previous step left, writes local rotations back for the joints it changed, and recomputes the model transforms of the changed joints' subtrees. Goal weights below 1e-4 skip the step. A goal in world space is converted to model space with the entity's world transform of the previous tick.

## Motion warping

Motion warping (Stage 4) bends a clip's root motion so an action lands on a target (a vault onto a ledge, an attack onto an enemy). A clip marks the window with a notify state flagged warp window named `Warp.<Name>`; the `Zenith.Animation.MotionWarp` component holds up to four targets `{ name hash; world position; world rotation; flags (bit 0 active, bit 1 translation only) }`, written in phase 2.

Each evaluation in which the instance's leader clip (the base layer's highest-weight clip) is inside an active window whose target is active:

1. Remaining motion: `R = Q(t)⁻¹ · Q(t_end)` of the window from the clip's root-motion track (one sample pair, O(1)).
2. Desired motion: `R* = E⁻¹ · G`, the target G relative to the entity's current world transform E, in the motion space (mode 1 keeps the target's x, z and yaw).
3. This evaluation's clip delta Δ (from the root-motion blend) is replaced by `Δ' = Δ_t' · Δ_r'`: translation components scaled by `R*.t / R.t` per axis where `|R.t| ≥ 0.01 m`, and for smaller axes offset by `(R*.t − R.t) × (Δtime / remaining time)`; rotation `Δ.q · Zenith_Slerp(identity, R.q⁻¹ · R*.q, Δtime / remaining time)`.
4. The correction is recomputed every evaluation (closed loop), so a moving target is tracked; outside the window Δ is untouched.

`Test_MotionWarp_ReachesStaticTarget` (the entity's position after the window equals the target within 1 cm with the clip's delta applied through a test movement integrator), `Test_MotionWarp_TracksMovingTarget` and `Test_MotionWarp_OutsideWindowUntouched` cover it.

## Retargeting

Retargeting (Stage 4) plays a clip authored on one skeleton on another. It changes rotations (and the root translation scale) only; it never edits a mesh, and a difference between the two rigs' rest poses is carried by the per-joint rest corrections below, never by re-posing a skeleton or a mesh.

### Retarget map construction

`Zenith_RetargetMap::Create(source, target, settings)`:

1. For each target joint, the source joint is the settings' override, else the source joint of the same role, else (for a chain member) the joint at the same normalised position along the corresponding chain (chains are matched by chain role), else unmapped.
2. For each mapped pair (s, j), the rest rotation correction, from the rest model rotations `S_s` (source) and `T_j` (target): the target's model rotation is `q_j = q_s · S_s⁻¹ · T_j`, so the source's rotation away from its rest is carried to the target's rest frame.
3. The root scale: `hipsHeight(target) / hipsHeight(source)` for `RETARGET_ROOT_SCALE_BY_HIPS_HEIGHT` (1 when either is 0), 1 for `COPY`, 0 for `NONE`.
4. The key: `Zenith_Hash64` of both skeletons' hierarchy and rest pose hashes and the settings. The module caches maps per world by key (`Zenith_HashMap`, `Zenith2/Docs/Design/CORE/Containers.md → "Zenith_HashMap and Zenith_HashSet"`), built in the publication handler when a binding needs one.

O(J_s + J_t + chain lengths).

### Runtime retarget application

`Apply(source pose, target pose)`: compute the source model rotations (rotations only, O(J_s)); for each target joint in order, a mapped joint takes the model rotation `q_s · S_s⁻¹ · T_j` and is converted to local through its already-converted parent; an unmapped joint keeps its rest local rotation; every target joint keeps its rest local translation (bone lengths are the target's) except the root (the hips when assigned), whose translation is the source's scaled by the root scale; scales take the target's rest. Curves copy by name hash. Retargeting a source in its rest pose yields the target's rest pose exactly (`Test_Retarget_RestToRestIsIdentity`), including between a T-posed and an A-posed rig. A graph clip flagged retargeted is applied through the binding's map before blending.

## Facial animation

### Curves and morph targets

Facial animation (Stage 4) is joints (bone-driven faces animate like any joint) plus curves: every pose carries the skeleton's C curves; clips animate them; blends, layers, masks and overrides treat them as the graph defines. At extraction, curves of kind `MORPH` are mapped onto the mesh's morph targets by name hash ("Morph remap"); curves of kind `GAMEPLAY` are readable by simulation through `Zenith_AnimationWorld::GetCurve`; `CONTROL` curves drive pose libraries.

### Pose libraries

A pose-library clip (flag bit 4) holds one named pose per frame, frame 0 the neutral pose; its markers name the frames (a viseme set, an expression set). The `POSE_BLEND` node (`Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"`) applies, onto its base, each listed pose as an additive delta from frame 0 with the weight of its source (a parameter, or a `CONTROL` curve of the base pose, such as the viseme curves a lip-sync track writes): `pose = base ⊕ Σ_k w_k (P_k ⊖ P_0)` in local space, weights clamped to [0, 1]. CHAR's lip-sync workflow produces the viseme curves; ANIM plays and blends them.

### Pose followers

A `Zenith.Animation.PoseFollower` component on an entity with a renderable mesh names a leader entity with an animator of the same skeleton; the extractor writes, for the follower's mesh, the leader's final pose (with the follower's own morph remap). A head, an outfit or a prop bound to the body skeleton is therefore a separate mesh entity driven by one evaluation, the mechanism CHAR's modular characters build on. A leader of another skeleton, a missing leader, or a chain of followers deeper than one is refused at extraction and counted (`anim.extract.follower_refused_delta`).

## Update-rate budgeting

Significance-based update-rate reduction (Stage 4) lets distant or unimportant characters evaluate less often, the animation budgeting mechanism of [Z2-N-BUDGET-438]. Its inputs are simulation state alone, so the reduction is deterministic and replays identically; it never reads render state, the quality tier or the thermal state.

### Significance

`Animation.Significance` computes, per instance: `d` = the distance from the entity's world position (previous tick) to the world's main camera, the camera RPROD's `Flux_SelectMainCamera` selects (`Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`: among the `Zenith.Render.Camera` rows with `Active` true and `Kind` MAIN, the highest `Priority`, ties to the lowest simulation ID), so ANIM's camera is the one the frame renders (a cross-entity read of properties 1 Active, 2 Priority and 11 Kind in phase 3, gathered in chunk order and selected order-independently; with no main camera, `d = 0`); `s = bias / max(d, 1 m)` with `bias` the animator's `SignificanceBias` (default 1, range [0.01, 100]).

### Rate tiers and phase offsets

<!-- elaboration -->
The rate divisor k ∈ {1, 2, 4, 8} is `Zenith_AnimComputeRateDivisor(d, bias, bForce, xDistances)` ("Zenith_AnimationWorld header"), the one function `Animation.Significance` calls for every instance, with `xDistances` the configuration keys `animation.budget.fullRateDistance` (default 15 m), `animation.budget.halfRateDistance` (40 m) and `animation.budget.quarterRateDistance` (80 m) read at Start. Its steps, in this order: (1) `bForce` → 1; (2) `d` not finite or negative → 1 (a corrupt distance never lowers fidelity); (3) `b = clamp(bias, 0.01, 100)`, `r = d / b`; (4) `r ≤ full` → 1; (5) `r ≤ max(full, half)` → 2; (6) `r ≤ max(full, half, quarter)` → 4; (7) otherwise 8 (so a non-increasing triple collapses tiers instead of reordering them). `bForce` is true for `ANIMATOR_FLAG_FORCE_FULL_RATE`, a ragdoll in any state other than NONE (`Zenith.Physics.Ragdoll` 1 State of the previous tick, read by `Animation.Significance`; ENTERING, ACTIVE and RECOVERING all need every tick evaluated, the get-up included), an active slot whose tag has `ANIM_SLOT_TAG_CINEMATIC` (bit 15) or `ANIM_SLOT_TAG_FULL_RATE` (bit 14, CHAR's lip-sync contract, row CH16), an active motion-warp window and the instance's first evaluation. A slot is active from the publication of its clip's binding until its blend-out completes ("Slots" of `Zenith2/Docs/Design/ANIM/Graph.md`). The instance evaluates on ticks with `(tick + offset) mod k = 0`, `offset = (simulationID × 0x9E3779B97F4A7C15) >> 61` reduced mod k, which spreads equal-rate instances across ticks; a rate change takes effect at the instance's next due tick. Δt of an evaluation is the simulated time since the instance's previous evaluation. The keys are simulation-affecting (`m_bSimulationAffecting`, recorded in every replay header) and admit the engine-default, platform, game and command-line layers only, never the quality-tier or user layers ("Configuration keys" of `Zenith2/Docs/Design/ANIM.md`), so no runtime setting or thermal event can change a replayed result.

### Evaluation cap

`animation.budget.maxFullRateEvaluationsPerTick` (default 0: no cap): when more instances are due at k = 1 than the cap, the ones with the lowest significance (ties: the higher simulation ID) are demoted to k = 2 for this tick, in a deterministic order. The ANIM Stage 4 budget ruling ticket (`Zenith2/Docs/Design/ANIM.md → "Pending row needs"`) allocates Z2-B-ANIM-005 "Animation full-rate evaluations per tick cap" per platform from its measured per-evaluation cost (reported in Z2-B-ANIM-004 "Animation budgeting reduced-rate CPU"), and that row's value is the key's value (row AN12, X212, [Z2-N-BUDGET-468]).

### Root motion under reduced rates

An instance whose root-motion mode is `EXTRACT` evaluates at full rate unless `ANIMATOR_FLAG_ALLOW_REDUCED_ROOT_MOTION` is set. With it, an evaluation covering k ticks computes the span's delta D; the instance then publishes, on each of the following k ticks, `S = Zenith_PowDualQuaternion(D, 1 / k)` on the first k − 1 and the exact remainder `(S^{k−1})⁻¹ · D` on the last, so the k published steps compose to D exactly in model space and movement stays smooth (lagging the evaluation by up to k − 1 ticks); each step's translation is scaled by the entity's scale at the tick it is published ("Animation.Evaluate" step 7). Events of a span fire at its evaluation tick with their in-span time fraction.

## Clip streaming

Streaming (Stage 4) keeps only the needed part of a long clip resident. The clip's header set is resident whenever the clip is ACTIVE; its segments arrive per chunk ("Stream table section" of `Zenith2/Docs/Design/ANIM/Formats.md`).

### Residency model

Each world has a **stream cache** (in `Zenith_AnimationResidency`, the world resource `Zenith.Animation.Residency`): per (clip, chunk) a state and, when resident, the decoded segments in one general-heap block, with a total of at most `animation.streaming.cacheBytes` (default 16 MiB, a member of `Z2-B-ANIM-001`). Residency is simulation state: it changes only in `Animation.Significance` (requests, evictions) and in the publication handler (arrivals), so every evaluation of a tick sees one residency.

### Streaming state machine

<!-- elaboration -->
| State | Event | Next | Action |
|---|---|---|---|
| `ABSENT` | the chunk enters an instance's prefetch window and the cache can hold it | `REQUESTED` | `Animation.Significance` collects the need and reserves its bytes; its merge node, when `m_uChunkReadsInFlight < animation.streaming.maxReadsInFlight` (otherwise the reservation is released, the chunk stays `ABSENT` and counts `anim.stream.deferred_delta`, "Asset bindings" step 1), allocates a completion ID, counts it in `m_uChunkReadsInFlight` until its publication, and issues `Zenith_AssetReader::ReadSection` for each segment section of the chunk (`ASSET_PRIORITY_HIGH` for a chunk containing a current playback time, `NORMAL` otherwise) into a block allocated now (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetReader header"`) |
| `ABSENT` | the chunk is in a window and the cache is full | `ABSENT` | evict, least recently used first (by last-use tick, then clip GUID, then chunk index), chunks in no window; when none can be evicted, defer and count `anim.stream.deferred_delta` |
| `REQUESTED` | every read of the chunk completed | `REQUESTED` (parked) | park one publication: success RECOMPUTED, failure RECORDED |
| `REQUESTED` (not parked) | the chunk leaves every window, or its clip is reloaded, before its reads complete | `ABSENT` | abandon the request ("Asset bindings" step 4): `RequestCancel` on each read, complete the ID with the abandoned payload in this `Animation.Significance` run; the block is freed when every read reports complete; the reservation is released then |
| `REQUESTED` | the publication arrives, success | `RESIDENT` | decode the segment tables into the block |
| `REQUESTED` | the publication arrives, failure (an abandoned payload included) | `FAILED` | free the block; the clip's instances hold their times at the chunk boundary; log once |
| `RESIDENT` | in no window for `animation.streaming.keepTicks` (default 60) ticks | `ABSENT` | free the block at the end of `Animation.Significance` |
| `FAILED` | the clip asset is reloaded | `ABSENT` | — |
| any | the world detaches | — | cancel outstanding reads; complete every chunk-read completion ID ANIM has not completed with the abandoned payload (`DetachWorld`, `Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"`); wait (as an operation drain) for each read to report complete before freeing |

### Prefetch window

For each instance and each streamed clip it plays (clip nodes with weight above 0, slots, prefetch commands), the window is the chunks covering `[t, t + animation.streaming.prefetchSeconds]` (default 2.0 s) along the play direction, wrapping for a loop clip, computed from the previous evaluation's times. The window starts at the chunk of segment `(f0 + 1) / 16` as well as that of `f0 / 16` (`f0` the frame at t, "Clip sampling"), so a sample at the last frame of a chunk, which reads frame `f0 + 1` of the next segment, finds the next chunk resident (`Test_ClipSampling_SegmentBoundary`). When an evaluation needs a segment that is not resident, the clip node holds its previous time (no advance, no events) and counts `anim.stream.stalls_delta`; the pose is resampled at the held time, whose chunk is in the window and so resident. `CLIP_READY` conditions and `Animation.IsClipReady` let a graph or a cinematic wait for a clip before starting it.

### Publication

Streaming publications use the same sink, source kind (`ASYNC_SOURCE_ANIMATION`, `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`) and handler as bindings ("Asset bindings"), so a replay holds each tick until the recorded chunks are local and every run of a replay stalls identically. `Test_AnimStreaming_StallsAreReplayed` asserts that a recorded run and its replay, with I/O delayed differently, produce the same tick hashes.

## Render extraction

### Extractor declared reads

`Animation.ExtractPoses`, registered with `AddExtractor` (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"`), declares: `Read` `Zenith.Animation.Animator` (1 Graph, 4 Enabled), `Zenith.Animation.PoseFollower` (1 Leader), `Zenith.Render.MeshInstance` (1 Mesh, 15 Visible, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`); `ReadResource` `Zenith.Animation.FinalPoses` and `Zenith.Animation.Residency` (the instance lookup); two queries (animators with a mesh instance; followers with a mesh instance). `Before("Render.Seal@W0", true)` is supplied by `EmitInto` (`Zenith2/Docs/Design/ECS/Scheduling.md → "Epilogue and extractors"`).

### Extraction algorithm

Per row (chunk order; each row's record depends on that row alone):

1. Skip a row whose mesh is null, invisible, or whose instance (its own or its leader's) has no final pose.
2. Resolve the mesh through ANIM's extraction resolver (the presentation-side asset resolution RPROD's extractors use, `Zenith2/Docs/Design/RPROD/Extractors.md → "Extraction resolver"`; ANIM's own interface `Zenith_AnimMeshResolver` in `Internal/Extract/`, with the production implementation over `Zenith_AssetRegistry::Resolve` and a fixture implementation for tests). A mesh that is not resolvable is skipped this frame; one whose `Flux_MeshResource::GetSkeletonID()` (assumption pending GEOM, row AN4) differs from the animator's skeleton is refused, and its once-only report (keyed by entity and mesh) is counted and logged by the node's body after the last leaf ("Counter and statistics merge").
3. The pose: per joint the interpolated final local transform ("Pose interpolation"), composed to model space, written as `Flux_JointMatrix` rows (the top three rows of each `Zenith_Matrix4`), J entries indexed by skeleton joint index.
4. Morph weights: the morph remap of (mesh, skeleton) maps curve values onto the mesh's targets; a mesh with no targets writes an empty span.
5. `Flux_RenderSkinnedPose { key = Flux_MakeEntityInstanceKey(persistent ID of the row's entity, world tag), mesh ID, joints, morph weights, flags }` (`Zenith2/Docs/Design/GEOM/Skinning.md → "Flux_Skinning header"`, `Zenith2/Docs/Design/RPROD/Extractors.md → "Instance keys"`), with `FLUX_SKINNED_POSE_FLAG_HISTORY_RESET` for an instance teleported since the previous extraction or extracted for the first time, through `Flux_RenderWorldWriter::AddSkinnedPose` (`Zenith2/Docs/Rendering.md → "The render world hand-off"`), which copies the spans into the slot arena (assumption pending RPROD, row AN5); `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` drops the pose and counts `anim.extract.dropped_delta`.

The key is the one RPROD's engine instance extractor computes for the same entity, so the pose deforms that entity's instance (row RP29). Every pose is written every frame (poses are not deltas), so a full-state extraction needs nothing extra. O(J) per skinned mesh plus J matrix products.

### Pose interpolation

<!-- elaboration -->
The rendered pose interpolates between the last two final poses the instance produced, at tick indices `T_a < T_b`, with the frame's interpolation alpha α ([Z2-N-ARCH-415]) and the instance's rate k: `β = clamp((T − k + α − T_a) / (T_b − T_a), 0, 1)` for the world's last tick T, so a full-rate instance shows `lerp(pose(T − 1), pose(T), α)` like every interpolated transform, and a reduced-rate instance moves smoothly at a lag of up to k − 1 ticks. Interpolation blends local transforms ("Local-space blend"), then composes; a history reset uses β = 1.

### Morph remap

For each (mesh ID, skeleton) pair the extractor uses a table from the mesh's morph targets to skeleton curve indices by name hash (`Flux_MeshResource::GetMorphTargetNameHash`, assumption pending GEOM, row AN4); a target with no curve takes 0. The tables are cached per world in the extraction state (at most `animation.extract.maxRemapTables`, default 1,024), and their use is presentation only. The cache is immutable while the leaves run ("Counter and statistics merge"): the extractor node's own body, before `ParallelFor`, walks its two queries' rows in chunk order, resolves each row's mesh, builds the table of every (mesh, skeleton) pair the frame needs and the cache lacks, stamps each needed pair's last-use frame, and only then evicts, least recently used first (ties by mesh ID, then skeleton GUID), tables no row of this frame needs until the cache is within its bound; when the frame needs more pairs than the bound, the cache holds the first `maxRemapTables` pairs in that walk order and the rows of the other pairs write an empty morph span this frame and count `anim.extract.remap_overflow_delta`. The leaves read the cache and never insert, stamp or evict. `Test_AnimExtract_RemapCacheWorkerIndependent` covers it.

## Save state

ANIM's simulation state that a save or a replay checkpoint captures is its instance state, held in its world resources rather than in components, plus the world-level binding and residency state the instances depend on. ANIM's one `Zenith_SaveParticipant` per world (`Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`, SAVE's row SV17), named `Zenith.Animation.Instances`, section version 1 (every member: "Participant members" below), writes it in phase 11 and restores it before the restored world's first tick. ANIM is an EXACT participant: every capture it makes is `SAVE_FIDELITY_EXACT` (SAVE's per-capture fidelity, `Zenith2/Docs/Design/SAVE.md → "Reproducibility boundaries"`), and it never sets `uSAVE_PARTICIPANT_FLAG_COHERENT`. Four properties make that true: the section holds every value a subsequent tick reads, the restore allocates no completion ID, every restored instance is `BOUND` (or in its saved status) at the first tick, and every ANIM result that SAVE's async section restores as completed but unpublished finds, when it publishes at the boundary of tick t + 1, exactly the asset or chunk state the uninterrupted run had.

**Parked results.** `Animation.Significance` (phase 3) parks each drained completion with the sink, and it publishes only at the next tick boundary, so a phase-11 snapshot regularly holds ANIM results that completed and were not yet published. SAVE saves them in its async section and restores them as completed, publishing at the boundary of tick t + 1 (ordinary restore; under a publication source a recorded failure is put back in flight for the log to complete) (`Zenith2/Docs/Design/SAVE/Formats.md → "Async section"`, row AN8). SAVE never re-runs them and ANIM never re-issues them (re-issuing would complete one completion ID twice). A parked **success** carries no payload (`PUBLICATION_KIND_GENERIC_RECOMPUTED`), so ANIM's restore rebuilds what the publication relies on: a binding load's asset is acquired, and a chunk read's bytes are read into a newly allocated block, both outside the sink and behind the readiness step, so that at t + 1 the handler resolves an ACTIVE asset and finds a filled block, as in the uninterrupted run. A parked **failure** needs nothing beyond its block (its 24-byte record is in SAVE's section), except that a chunk failure's block is allocated again so the cache's accounting matches.

<!-- elaboration -->
**Section layout** (little-endian, field by field, through `Zenith_SaveSectionWriter`):

| Part | Content |
|---|---|
| world prologue | binding cache: count u32, then per asset `{ Zenith_PersistentID (16); u8 kind (7, 8, 9); u8 reserved[7] }` in ascending GUID; stream cache: count u32, then per (clip, chunk) in a state other than ABSENT `{ clip GUID (16); u32 chunk; u8 state (RESIDENT, REQUESTED, FAILED); u8 reserved[3]; u64 last-use tick }` in (GUID, chunk) order; requests: count u32, then `{ u64 completion ID; u8 kind (0 binding load, 1 chunk read); u8 state (0 issued, not completed; 1 parked success, completed and not published; 2 parked failure; 3 abandoned, completed by ANIM with the abandoned payload and not published); u8 reserved[6]; asset GUID (16); u32 chunk index (kind 1); u32 reserved; u64 requester simulation ID (kind 0) }` in ascending completion ID: every request with a completion ID that has not yet published |
| instance records | count u32, then one `Zenith_AnimatorSaveRecord` per instance (every status, not only `BOUND`) in ascending simulation ID |

`Zenith_AnimatorSaveRecord` (version 1):

| Part | Content |
|---|---|
| header | entity persistent ID (16), graph GUID (16), graph hash (8), status (1), flags (1), reserved (2), rate divisor (1), phase offset (1), ticks since evaluation (2), accumulated seconds (f32), `m_fLastDeltaSeconds` (f32), last evaluation tick (u64) |
| parameters | count u16, then `{ u64 name hash; f32 value; u8 trigger counter; u8 reserved[3] }` each |
| nodes | count u16, then per node with state `{ Zenith_PersistentID stable ID (16); u16 kind; u16 bytes; the node state bytes }` |
| inertial state | `u8 pending; u8 reserved[3]; f32 pending seconds; u32 buffer bytes (0 when the graph does not inertialise); the buffer bytes` |
| slots | count u8, then `{ u64 slot hash; Zenith_PersistentID clip (16); the 32-byte slot state }` |
| pose history | joint count u16, curve count u16, then the pose history current and previous (2 × 40 × J), the animated local pose after IK (40 × J), the animated curves current and previous (2 × 4 × C), the final local pose current (40 × J) and the final curves current (4 × C) |
| curve overrides, IK springs, root-motion spread, ragdoll recovery (O, `w_1`, the last p, active), queued commands | fixed-size blocks, each prefixed by a u16 length |

Model poses are not saved: they are `Zenith_LocalToModel` of the saved local poses, recomputed bit-identically at restore. The previous final pose is not saved: it is presentation (extraction interpolation), and the first extraction after a restore carries `FLUX_SKINNED_POSE_FLAG_HISTORY_RESET`. Everything else an evaluation, the ragdoll entry snapshot or the phase-6 blend reads is in the record, so a reduced-rate instance that is not due at the first restored tick still blends and extracts its saved pose. At J = 60 and C = 0 the pose history is 9,600 bytes per instance (960 KB for W-Characters' 100).

**Restore** (main thread, no frame running):

1. `RestoreSection` (SAVE's `WORLD_RESTORE_STATE_PARTICIPANTS`, after every entity and world resource exists). When `xContext.m_bSectionPresent` is false (a save written without the module, or before the participant was registered), no instance is restored and nothing is re-issued: the world's animators bind through the ordinary path ("Asset bindings") from the first tick, which is coherent and not identical, logged once under `anim` (`save: no animation section, animators rebind`), and `IsRestoreReady` returns true at once. Otherwise decode the section through `Zenith_SaveSectionReader` (untrusted: every count is checked against the parser limits and the remaining bytes, a violation is `SAVE_ERROR_CORRUPT`) into a **restore-owned decoded copy**, one general-heap block of the participant that outlives `RestoreSection` (the reader is SAVE's and invalid when the call returns) and that step 4 frees; migrate an older section version one step per version; then, for each binding-cache asset and for the asset of each parked-success binding load (state 1, kind 0), acquire it through `Zenith_AssetLoader::RequestLoad` on the world's **restore queue** `anim.restore.w<tag>`, whose completions are never parked with the publication sink. The sink is not called: the restore allocates no completion ID, so the world's next completion ID, which SAVE restores from its async section, equals the uninterrupted run's.
2. Re-issue the saved requests of state 0 (issued, not completed), and only those (states 1, 2 and 3 are completed results SAVE restores as unpublished), in ascending completion ID: for each, first call `xContext.m_pxReissue->MarkReissued(id)`; on `true` re-issue it under its recorded completion ID (the binding load on the world's ordinary completion queue, the chunk read into a newly allocated block, each counted in its in-flight counter), as SAVE's rule for in-flight asynchronous work requires (`Zenith2/Docs/Design/SAVE/Formats.md → "Async section"`, `Zenith2/Docs/Design/SAVE/Algorithms.md → "Replay seeking"`), so it publishes at a tick boundary exactly like the original request. A `false` from `MarkReissued`, or a state-0 entry for which `WasInFlight(id, ASYNC_SOURCE_ANIMATION)` is false, means the prologue and SAVE's async section disagree, and `RestoreSection` fails with `SAVE_ERROR_CORRUPT` (SAVE then destroys the world). Every in-flight entry of `ASYNC_SOURCE_ANIMATION` the section does not list stays unmarked, and SAVE completes it with the zero-length abandoned payload when PARTICIPANTS ends; the publication handler maps that payload to a load failure of its request (none exists, so it is dropped, "Asset bindings" step 4).
3. `IsRestoreReady` (SAVE's readiness step, `WORLD_RESTORE_STATE_PARTICIPANTS_READY`), polled once per restore `Step`: drain the restore queue; when every acquired asset is ACTIVE or FAILED, issue the reads of every RESIDENT chunk of the saved stream cache and of every parked-success chunk read (state 1, kind 1) into newly allocated blocks (also outside the sink), and allocate the block of every parked-failure chunk read (state 2); when those reads have completed, run step 4 once and return `true`; until then return `false`. A restore-time read that fails (the clip's content changed since the save) makes that chunk `FAILED` (logged once, counted with `anim.save.restore_bind_failed_delta`; a content failure, so that continuation is not hash-equal) and the restore continues. Nothing here blocks: the restore job keeps stepping and the loading screen covers the wait. `ListRestoreWaits` lists, while `IsRestoreReady` is false, the outstanding restore-queue load operations in ascending asset GUID, then the outstanding restore chunk reads in (clip GUID, chunk) order, the first `axOut.Size()` of them, so the synchronous restore driver (`Zenith2/Docs/Design/SAVE/RuntimeBridge.md → "Synchronous restore driver"`) services exactly the operations ANIM waits for outside a frame; it lists nothing once ready.
4. Apply, from the decoded copy, which is freed at the end of this step: build the binding-cache entries in ascending GUID (event-name interning, retarget maps); set the stream cache states (RESIDENT with the decoded blocks; REQUESTED for re-issued reads; REQUESTED (parked) under the recorded completion ID for parked reads, with the filled block of a success or the empty block of a failure, which the publication at t + 1 makes RESIDENT or FAILED exactly as in the uninterrupted run; FAILED; last-use ticks); record each abandoned request (state 3) under its completion ID with no requester, counted in its in-flight counter until its restored publication at t + 1 releases it, exactly as in the uninterrupted run; record each parked binding load under its completion ID with its requester and the restore queue's acquisition, which the publication handler takes over at t + 1 (the handler's `Resolve` finds the asset ACTIVE) and releases; create every instance in ascending simulation ID in its saved status, allocate its block, apply the record (nodes matched by stable ID, the inertial state when the restored graph `Inertialises()` (otherwise none), the pose history when the joint and curve counts match the bound skeleton), mark its block generation applied, and recompute its model poses. An instance whose recorded asset failed to load restores `BINDING_FAILED`, logs once and counts `anim.save.restore_bind_failed_delta` (a content failure, so that continuation is not hash-equal; the restore itself still completes). A parked publication that arrives at t + 1 for a request whose record is absent, or whose asset or chunk restored `FAILED`, is dropped by the handler ("Asset bindings" step 4), so a content failure at restore never applies a stale success.

The first tick after the restore therefore evaluates exactly the state the uninterrupted run had: no instance is `BINDING` that was `BOUND`, no completion ID was spent, and every resident chunk is resident. A record restored onto an edited graph (another graph hash) applies by stable ID with the "Graph hot reload" rules, so a record survives a graph edit (that continuation is coherent, not identical, because the content changed).

`Test_AnimSave_RecordRoundTrip`, `Test_AnimSave_RestoreBindsBeforeFirstTick`, `Test_AnimSave_RestoreAllocatesNoCompletionID`, `Test_AnimSave_InFlightReissuedWithRecordedIDs`, `Test_AnimSave_ParkedCompletionPublishesAfterRestore`, `Test_AnimSave_ReissueDeclaredAndMarked`, `Test_AnimSave_StreamResidencyRestored`, `Test_AnimSave_RestoreAfterGraphEdit` and `Test_AnimSave_RestoreBindFailureCounted` cover it (`Zenith2/Docs/Design/ANIM/Tests.md → "Module binding and failure tests"`).

<!-- elaboration -->
**Participant members.** ANIM implements every member of `Zenith_SaveParticipant` as SAVE declares it (`Zenith2/Docs/Design/SAVE/PublicAPI.md → "Zenith_SaveParticipant header"`); the class is `Zenith_AnimSaveParticipant` in `Internal/Instances/Zenith_AnimSaveParticipant.cpp`, one per world, registered in AttachWorld step 1 and unregistered before `DetachWorld` destroys the resources:

| Member | ANIM's implementation |
|---|---|
| `GetName` | `"Zenith.Animation.Instances"` |
| `GetSectionVersion` | `uANIM_SAVE_RECORD_VERSION` (1) |
| `GetFlags` | 0: not `uSAVE_PARTICIPANT_FLAG_REQUIRED` (a build without the module loads the save and carries the section as unknown), not `uSAVE_PARTICIPANT_FLAG_COHERENT` (every capture is EXACT), not `uSAVE_PARTICIPANT_FLAG_EXCLUDE_FROM_RESUME` (resume images include the section) |
| `DeclareAccess` | `ReadResource` of `Zenith.Animation.Residency`, `Zenith.Animation.World` and `Zenith.Animation.FinalPoses` on the phase-11 snapshot system (every resource `CaptureSection` reads) |
| `CollectExcludedEntities` | the default (none): an instance's state is complete at every tick boundary |
| `CaptureSection` | the section layout above, from the three resources |
| `GetCaptureFidelity` | the default, `SAVE_FIDELITY_EXACT` |
| `GetReissuedSourceKinds` | `1u << ASYNC_SOURCE_ANIMATION` (ANIM names the enumerator, value 9 in `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`) |
| `RestoreSection` | Restore steps 1 and 2 |
| `IsRestoreReady` | Restore step 3 |
| `ListRestoreWaits` | Restore step 3 |

ANIM's save-record version is the participant's section version, with one migration step per version ([Z2-N-CONTRACT-524]).

## Luau bindings

The module registers the binding table `Animation` (`Zenith2/Docs/Scripting/Bindings.md → "Binding registration and the thunk ABI"`) at start step 4; every function is callable from behaviours and chunk systems (`SCRIPT_FUNCTION_CONTEXT_SYSTEM | SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR`):

| Luau function | Effect | Errors raised (controlled Luau errors, [Z2-N-CONTRACT-473]) |
|---|---|---|
| `Animation.SetFloat(entity, name, value)`, `SetInt`, `SetBool` | buffered `Animation.SetParameter` command | non-finite value; a bound instance whose graph lacks the name or has another kind |
| `Animation.SetTrigger(entity, name)`, `ResetTrigger` | buffered `Animation.SetTrigger` | as above |
| `Animation.GetFloat(entity, name)` → number, `GetInt`, `GetBool` | read-world (`Zenith.Animation.World`): the value committed by the latest evaluation | the entity has no instance; unknown name |
| `Animation.GetState(entity, layer)` → `{ machine, state, time, normalisedTime, inTransition }` | read-world | layer out of range |
| `Animation.PlaySlot(entity, slot, clip, options)`, `StopSlot(entity, slot, blendOut)` | buffered `PlaySlot`, `StopSlot`; `options` is a table of the optional fields `blendIn`, `blendOut`, `rate`, `startTime`, `loop`, `rootMotion` and `tag` (an integer copied into `Zenith_AnimSlotPlayDesc::m_uTag`; a script may set bits 0 … 14, so `tag = 16384` is `ANIM_SLOT_TAG_FULL_RATE`, row CH16) | unknown slot; a clip of another skeleton without a retarget profile; blend times outside [0, 10] s; rate outside [0, 16]; a `tag` outside [0, 32767] (bit 15 is the cinematic writer's) |
| `Animation.ForceState(entity, machine, state, blendSeconds)`, `ResetGraph(entity)` | buffered | unknown names |
| `Animation.SetIKGoal(entity, goal, position, rotation, weight)` | buffered `SetProperty` on `Zenith.Animation.IKGoals` | goal index outside 0 … 7; non-finite values |
| `Animation.SetWarpTarget(entity, window, position, rotation)` | buffered `SetProperty` on `Zenith.Animation.MotionWarp` | more than four windows |
| `Animation.GetSocket(entity, socket)` → `(position, rotation)` in world space | read-world (`Zenith.Animation.FinalPoses` and the entity's world transform of the previous tick) | unknown socket |
| `Animation.GetCurve(entity, curve)` → number | read-world | unknown curve |
| `Animation.SetCurveOverride(entity, curve, value, weight)` | buffered | unknown curve; weight outside [0, 1] |
| `Animation.PrefetchClip(entity, clip, startTime)`, `Animation.IsClipReady(entity, clip)` → bool | buffered; read-world | — |
| `Animation.GetClipLength(clip)` → seconds | pure on the bound clip (the binding cache) | an unbound clip |
| `Animation.GetMarkerTime(clip, name)` → seconds, or nil | pure on the bound clip (the binding cache), like `GetClipLength`: the time of the marker whose name hash equals `Zenith_Hash64(name)` in `Zenith_AnimationClip::GetMarkers` (binary search over the time-sorted records is not possible by hash, so a linear scan, at most `uANIM_MAX_CLIP_MARKERS`), nil when the clip has no such marker (row CH16; CHAR's lip-sync window lookup) | an unbound clip; an empty name |

Buffered functions declare `SCRIPT_FUNCTION_EFFECT_BUFFERED` with the field-command side-effect mask, through which a binding's module commands travel as ENV's setters' do (assumption pending SCRIPT, row AN10); read-world functions declare `SCRIPT_FUNCTION_EFFECT_READ_WORLD` with the resource IDs of `Zenith.Animation.World`, `Zenith.Animation.FinalPoses` and `Zenith.Animation.Residency` (every getter looks the instance up through the residency, "Resource partition" of "Zenith_AnimationWorld header"). Names are interned strings hashed with `Zenith_Hash64`. The events `Zenith.Animation.Notify` and `Zenith.Animation.StateChanged` reach scripts as ECS events with generated `.d.luau` types.

### Misuse codes

Every error of the table above is a catalogued misuse error raised through the binding thunk's failure path (`Zenith2/Docs/Scripting/Bindings.md → "Binding registration and the thunk ABI"`) with a code of ANIM's block **ZS2000–ZS2099** (`Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"`, [Z2-N-SCRIPT-926]). The orchestrator's Q81 ruling assigns the block to ANIM (AUDIO takes the next free block); X244 added ANIM's row, listing ZS2001–ZS2013, to that table; the bindings ticket edits the row only when it adds a code. The text is `<Function>: <reason>` with these reasons:

| Code | Kind | Trigger | Reason text |
|---|---|---|---|
| ZS2001 | MISUSE | a non-finite number among the arguments (value, position, rotation, weight, time) | `argument <Index> is not finite` |
| ZS2002 | MISUSE | the entity has no animator instance | `entity <SimId> has no animator` |
| ZS2003 | MISUSE | the call needs a bound instance and the instance is `BINDING`, `BINDING_FAILED` or `DISABLED` (getters) | `the animator of entity <SimId> is not bound` |
| ZS2004 | MISUSE | the bound graph has no parameter of that name | `graph has no parameter '<Name>'` |
| ZS2005 | MISUSE | the parameter has another kind than the setter or getter | `parameter '<Name>' is <Kind>, not <Kind>` |
| ZS2006 | MISUSE | no slot, state machine, state, socket or curve of that name | `no <What> named '<Name>'` |
| ZS2007 | MISUSE | a layer index outside the graph's layers | `layer <Index> is out of range (the graph has <Count>)` |
| ZS2008 | MISUSE | a clip of another skeleton with no retarget profile | `clip <Guid> is for another skeleton and has no retarget profile` |
| ZS2009 | MISUSE | a blend time outside [0, 10] s, a rate outside [0, 16], or a slot `tag` outside [0, 32767] | `<What> <Value> is outside [<Min>, <Max>]` |
| ZS2010 | MISUSE | an IK goal index outside 0 … 7 | `IK goal <Index> is out of range (0 … 7)` |
| ZS2011 | MISUSE | a fifth warp window on one entity | `more than four warp windows` |
| ZS2012 | MISUSE | a curve-override weight outside [0, 1] | `weight <Value> is outside [0, 1]` |
| ZS2013 | MISUSE | `GetClipLength` or `GetMarkerTime` of a clip not in the world's binding cache, or `GetMarkerTime` with an empty name | `clip <Guid> is not bound in this world`; `marker name is empty` |

`Test_AnimationBindings_MisuseRaisesLuauError` raises each code once and asserts its code and text.

## Memory consumer

The module registers one `Zenith_MemoryConsumer` (`Zenith2/Docs/Architecture.md → "Memory governor"`) for `MEMORY_SUBSYSTEM_SIMULATION`; `GetResidentBytes` (relaxed reads) is the sum over worlds of instance blocks and records, stream caches, binding caches, retarget maps and remap tables, published as the gauge `anim.memory_bytes`. Animation is simulation memory, which is budgeted and not shed (the "Simulation" row of the governor's shedding table): `OnPressureLevelChanged` changes nothing, and stream-cache eviction is driven by playback windows alone, never by the pressure level, so memory pressure cannot change a simulation result ([Z2-N-ARCH-812]). `Test_AnimationMemory_ForcedCriticalShedsNothing` asserts identical tick hashes with and without a forced CRITICAL. The memory-consumer ticket appends ANIM's row to the memory suite's shedding cases, `Zenith2/Tests/Memory/Internal/Zenith_MemorySheddingCases.h` (`axMEMORY_SHEDDING_CASES`; subsystem `MEMORY_SUBSYSTEM_SIMULATION`, consumer `anim`, nothing shed at any level), in the same diff (A5 row CB8).
