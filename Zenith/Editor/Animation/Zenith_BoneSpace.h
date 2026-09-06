#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"

class Flux_SkeletonInstance;

//=============================================================================
// Zenith_BoneSpace (WU-4.2) — the bone-local <-> model <-> world conversions the
// pose manipulator is built on.
//
// ★ NOT ONE LINE OF UI, AND NOTHING MUTABLE. No ImGui, no ECS, no panel state,
// no statics: every function takes a skeleton instance and a matrix and returns
// a number. The manipulator (WU-4.3) is an ImGui draw-list overlay that projects
// what these return; keeping the arithmetic out here is what makes the part that
// is easy to get silently wrong catchable by a headless unit.
//
//-----------------------------------------------------------------------------
// THE MATRICES, NAMED ONCE (design note §3.1)
//
//   L_i  bone i's local matrix, T(p) * R(q) * S(s)
//        — Flux_SkeletonInstance::ComposeTransformMatrix
//   M_i  bone i's model matrix, M_parent(i) * L_i   (roots: M_i = L_i)
//        — written by ComputeSkinningMatrices, read via GetBoneModelTransform
//   W    the session model matrix, model -> world
//        — Zenith_AnimationPreviewSession::GetSessionModelMatrix(); IDENTITY for
//          the whole of Phase 4, but passed explicitly everywhere so the general
//          case is pinned by tests and nothing has to be re-derived the day a
//          session places the character somewhere other than the origin.
//
// ★ THE FOURTH MATRIX — THE INVERSE BIND POSE — TAKES NO PART IN AUTHORING, AND
// IS DELIBERATELY NOT REFERENCED ANYWHERE IN THIS FILE OR ITS .cpp. It exists in
// exactly one expression in the engine, the skinning matrix
// (model-space * inverse bind pose), where it converts MESH VERTICES. It does not
// convert BONES. Reaching for it while converting a gizmo delta produces code
// that looks plausible, is right at an identity bind pose, and is wrong at every
// other one. The bind TRS is likewise only a SEED for a pose, never a factor in
// a delta.
//
//-----------------------------------------------------------------------------
// ★ THE PRECONDITION NOBODY CAN SEE (design note §3.2)
//
// GetBoneModelTransform returns a CACHE. It is written only by
// Flux_SkeletonInstance::ComputeSkinningMatrices, and SetBoneLocalTransform does
// NOT invalidate or update it — so a pose write followed immediately by a
// model-space read returns the PREVIOUS geometry, with no assert and no symptom
// other than one frame of lag.
//
// Every function here reads that cache and TAKES WHAT IT IS GIVEN: none of them
// recomputes it, because a helper that quietly recomputed would hide the staleness
// from the one caller (the panel) that is able to sequence the refresh properly,
// and would recompute the whole skeleton several times per frame besides. The
// contract is the caller's: every pose write is followed by
// Zenith_AnimationPreviewSession::RefreshDerivedPose() before anything here is
// called.
//
//-----------------------------------------------------------------------------
// SCALE (design note §3.4 / §9.5)
//
// L_i's 3x3 is R*S, so a quaternion recovered from an accumulated model matrix is
// the true rotation only while the accumulated scale is uniform and positive.
// Zenith_Maths::DecomposeTRS is documented for exactly that class of matrix
// (affine, shear-free, positive scale) and normalises what it returns. Every rig
// in the tree has unit bind scale, so ParentWorldRotation ASSERTS uniformity and
// handedness rather than assuming them — a sheared parent frame would otherwise
// produce a delta that is subtly wrong in a way no gate could see.
//=============================================================================

namespace Zenith_BoneSpace
{
	//-------------------------------------------------------------------------
	// M_i — bone uBone's model-space matrix, straight off the instance's cache.
	// Identity when the index is out of range.
	//-------------------------------------------------------------------------
	Zenith_Maths::Matrix4 BoneModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	//-------------------------------------------------------------------------
	// M_parent(i) — identity for a root bone, for an out-of-range index, and for
	// an instance with no source skeleton (the parent chain lives on the asset,
	// not on the instance). Identity is the right answer for a root rather than a
	// failure: a root's local rotation IS its model rotation, which is exactly
	// what composing against identity says.
	//-------------------------------------------------------------------------
	Zenith_Maths::Matrix4 ParentModelMatrix(const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	//-------------------------------------------------------------------------
	// W * M_i, and its translation column.
	//
	// BoneWorldPosition is the pivot the rotation ring is drawn around. Note what
	// that point IS: with L_i = T(p) * R(q) * S, the translation of M_i is
	// M_parent(i) * p, which does not depend on q_i at all — it is the FIXED
	// POINT of the bone's own rotation, the joint the bone turns ABOUT. (The
	// joints q_i moves are its children's.) That is the handle the user expects
	// under their cursor, and it is pinned by a unit.
	//-------------------------------------------------------------------------
	Zenith_Maths::Matrix4 BoneWorldMatrix(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);
	Zenith_Maths::Vector3 BoneWorldPosition(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	//-------------------------------------------------------------------------
	// The NORMALISED rotation of W * M_parent(i) — the frame a world-space delta
	// has to be conjugated into, because q_i is expressed in its parent's frame
	// and nothing else in the chain is. Extracted with Zenith_Maths::DecomposeTRS,
	// which asserts nothing itself, so this function asserts the uniform-positive-
	// scale precondition on its behalf.
	//-------------------------------------------------------------------------
	Zenith_Maths::Quat ParentWorldRotation(const Zenith_Maths::Matrix4& xSessionModel,
		const Flux_SkeletonInstance& xSkeleton, u_int uBone);

	//-------------------------------------------------------------------------
	// conj(qParentWorld) * dQworld * qParentWorld  (design note §3.4)
	//
	// Derivation, so the conjugation is not folklore. The manipulator produces a
	// world delta dQ_w about a world axis frozen at drag start — the same
	// construction the entity gizmo uses. Let P = W * M_parent(i) and
	// qP = rotation(P); the bone's world orientation is qP * q_i. Applying the
	// delta on the left:
	//
	//     qP * q_i'  =  dQ_w * qP * q_i
	//          q_i'  =  conj(qP) * dQ_w * qP * q_i
	//
	// For a ROOT bone qP is rotation(W), which Phase 4 keeps at identity, and the
	// whole thing collapses to the entity gizmo's newRotation = delta * initial.
	//
	// qParentWorld is normalised on the way in. It arrives unit from
	// ParentWorldRotation, but a non-unit quaternion here would not fail — it
	// would scale the delta and quietly shrink or grow the pose, so the one line
	// that costs nothing is worth more than the assert it replaces.
	//-------------------------------------------------------------------------
	Zenith_Maths::Quat WorldDeltaToBoneLocalDelta(const Zenith_Maths::Quat& xWorldDelta,
		const Zenith_Maths::Quat& xParentWorldRotation);

	//-------------------------------------------------------------------------
	// The whole gesture in one call: q' = WorldDeltaToBoneLocalDelta(...) * q.
	//
	// The result is deliberately NOT re-normalised. A drag latches the bone's
	// rotation at BeginBoneDrag and applies ONE delta to that latched value every
	// frame, so there is no accumulation to drift — and a normalise here would
	// mask the day someone starts feeding the previous frame's output back in.
	//-------------------------------------------------------------------------
	Zenith_Maths::Quat ApplyWorldDeltaToBoneLocal(const Zenith_Maths::Quat& xWorldDelta,
		const Zenith_Maths::Quat& xParentWorldRotation,
		const Zenith_Maths::Quat& xBoneLocalRotation);
}

// Force-link anchor: Zenith_Editor::Initialise calls this so this TU survives
// /OPT:REF. Nothing names this .obj until WU-4.3's manipulator calls the
// conversions for real — and an .obj the linker never pulls in takes the
// ZENITH_TEST registrars at the bottom of the .cpp with it, moving the unit count
// by zero and reddening nothing. Same idiom, same reason, as
// Zenith_AnimTimelineMath_ForceLink.
bool Zenith_BoneSpace_ForceLink();

#endif // ZENITH_TOOLS
