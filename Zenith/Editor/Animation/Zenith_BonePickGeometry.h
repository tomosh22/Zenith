#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

class Flux_SkeletonInstance;
class Zenith_SkeletonAsset;

//=============================================================================
// Zenith_BonePickGeometry (WU-4.1) — the CPU hit volumes a bone is picked with.
//
// ★★ THE CAPSULE FROM JOINT i TO JOINT child(i) BELONGS TO BONE i — THE PARENT
// WHOSE ROTATION MOVES IT. This is the single thing about bone picking that is
// most easily got one level out, it is invisible to every geometric check, and
// the design note (Docs/design/AnimationPoseAuthoring.md §2) states the OPPOSITE
// rule. The note is wrong, and here is the evidence, which is a property of the
// composition rather than an opinion:
//
//   Flux_SkeletonInstance::ComposeTransformMatrix (Flux_SkeletonInstance.cpp
//   :231-241) builds L_i = T(p_i) * R(q_i) * S(s_i), and ComputeSkinningMatrices
//   (:296-318) sets M_i = M_parent * L_i. The translation column of a product
//   A * (T(p) * R * S) is A * (p, 1), because R * S contributes no translation.
//   So   translation(M_i) = M_parent * p_i,   with NO q_i in it.
//
// Bone i's own joint therefore does NOT move when q_i changes; the joints BELOW
// it do. The segment a user sees swing when they rotate bone i is the one from
// joint i to joint child(i), so that is the segment that has to select bone i.
// Under the note's rule, clicking the segment that visibly swings would select
// the child — every drag would rotate the wrong joint, and it would still look
// entirely plausible in a screenshot.
//
// The shape set that follows:
//   * ONE CAPSULE PER (bone, child) PAIR, owned by the PARENT. A bone with three
//     children owns three capsules; all three swing together under q_i, which is
//     exactly what they represent. The ROOT owns the capsules to its children
//     like any other bone.
//   * A JOINT SPHERE for a LEAF (no children, so it owns no capsule — its joint
//     dot is the only thing drawn for it) and for a bone whose every capsule was
//     degenerate.
//
// ★ CPU-ONLY AND DEVICE-FREE. Flux_SkeletonInstance holds no GPU resources at
// all, so a unit builds a skeleton in code, calls ComputeSkinningMatrices() and
// fires rays — no device, no view, no window, and nothing here is
// requiresGraphics.
//=============================================================================

// Radii, all derived from the skeleton's own extent so the same numbers work on
// a 1.8 m humanoid and a 0.2 m prop rig.
inline constexpr float kfBONE_PICK_RADIUS_FRACTION   = 0.12f;   // of the segment length
inline constexpr float kfBONE_PICK_MIN_RADIUS_EXTENT = 0.004f;  // of the skeleton extent
inline constexpr float kfBONE_PICK_JOINT_EXTENT      = 0.020f;  // of the skeleton extent
// Shorter than this and the capsule is degenerate: the axis cannot be normalised
// meaningfully and the joint sphere takes over.
inline constexpr float kfBONE_PICK_MIN_SEGMENT       = 1.0e-4f;

//-----------------------------------------------------------------------------
// One hit volume, in WORLD space (the session model matrix is already folded in
// by Zenith_BuildBonePickSet, so a picking ray needs no transform of its own).
//-----------------------------------------------------------------------------
struct Zenith_BonePickShape
{
	// The bone this shape SELECTS — the one whose rotation moves it.
	u_int                 m_uBoneIndex   = 0u;
	// The owning bone's OWN joint. Equal to m_xB for a joint-only shape.
	Zenith_Maths::Vector3 m_xA           = Zenith_Maths::Vector3(0.0f);
	// The CHILD joint the capsule runs to. Equal to m_xA for a joint-only shape.
	Zenith_Maths::Vector3 m_xB           = Zenith_Maths::Vector3(0.0f);
	float                 m_fRadius      = 0.0f;
	bool                  m_bIsJointOnly = false;
	// Which child this capsule runs to. Only meaningful for a capsule; a bone
	// with several children owns several shapes and this is what tells them
	// apart (an overlay drawing "the selected bone" draws all of them).
	u_int                 m_uChildBoneIndex = 0u;
};

struct Zenith_BonePickSet
{
	Zenith_Vector<Zenith_BonePickShape> m_xShapes;
	// The model-space AABB diagonal over every joint. Zero for an empty or
	// single-point skeleton, in which case every derived radius is zero too and
	// nothing is pickable — which is honest: there is no geometry to aim at.
	float m_fSkeletonExtent = 0.0f;
};

//-----------------------------------------------------------------------------
// Rebuild xOut from the instance's CURRENT model-space transforms, pre-multiplied
// by xSessionModel so every shape is in WORLD space.
//
// ★ PRECONDITION: xSkeleton.ComputeSkinningMatrices() has run since the last
// pose write. GetBoneModelTransform returns a CACHE that only that call fills;
// SetBoneLocalTransform neither updates nor invalidates it, so a pose write
// followed straight by a build produces shapes one write behind the bones, with
// no assert and no symptom other than lag.
// Zenith_AnimationPreviewSession::RefreshDerivedPose() is the one call that
// satisfies this.
//
// xSkeletonAsset supplies the parent indices. It is taken EXPLICITLY rather than
// read back off the instance so the function has no way to be handed a null
// asset, and so a unit can build both halves by hand. Bones past the smaller of
// the two counts are ignored.
//-----------------------------------------------------------------------------
void Zenith_BuildBonePickSet(const Flux_SkeletonInstance& xSkeleton,
	const Zenith_SkeletonAsset& xSkeletonAsset,
	const Zenith_Maths::Matrix4& xSessionModel,
	Zenith_BonePickSet& xOut);

// How far BEHIND the nearest capsule hit a joint sphere may still win, as a
// multiple of that capsule's own radius. See the ★ note on the raycast below.
inline constexpr float kfBONE_PICK_JOINT_PRIORITY_RADII = 2.0f;

//-----------------------------------------------------------------------------
// Nearest non-negative hit.
//
// ★ RETURNS FALSE AND LEAVES BOTH OUTPUTS UNTOUCHED ON A MISS. A caller that
// passed in a live selection index must not have it overwritten with a sentinel
// by a click that hit nothing — the caller decides what a miss means.
//
// ★ A JOINT SPHERE OUTRANKS A CAPSULE IT IS SITTING INSIDE, and that is not a
// tie-break — it is the rule that makes a leaf pickable at all. A leaf owns no
// capsule, so its only shape is a small sphere at its own joint, and that joint
// is by construction the far END of its parent's capsule. Whenever the parent
// bone is long relative to its radius the sphere is entirely nested inside the
// parent's rounded cap, and plain nearest-wins would report the parent for every
// ray that could ever reach the leaf — the tip would be permanently
// unselectable, on some rigs and not others, depending only on how the radius
// happened to work out.
//
// The margin is what keeps that from swallowing the scene: a sphere wins only
// while it is within kfBONE_PICK_JOINT_PRIORITY_RADII * (that capsule's radius)
// of the capsule hit — i.e. while it really is inside the geometry in front of
// it. A fingertip tucked behind a torso is far further back than that, and the
// torso in front of it wins as it should.
//
// Among shapes of the SAME class, nearest wins, with a 1e-4 tie going to the
// incumbent.
//-----------------------------------------------------------------------------
bool Zenith_RaycastBonePickSet(const Zenith_BonePickSet& xSet,
	const Zenith_Maths::Vector3& xRayOrigin,
	const Zenith_Maths::Vector3& xRayDir,
	u_int& uOutBoneIndex,
	float& fOutDistance);

#endif // ZENITH_TOOLS
