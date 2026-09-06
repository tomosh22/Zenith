#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

class Flux_SkeletonInstance;
class Zenith_SkeletonAsset;
class Zenith_AnimationDocument;

//=============================================================================
// Zenith_AnimationPoseIK (WU-4.4) — IK-ASSISTED POSING, BAKED DOWN TO KEYS.
//
// Two functions and a request struct. Nothing here holds state, nothing here
// reads engine state, and nothing here draws: a solve is a pure function of a
// skeleton instance's CURRENT local pose plus a model-space target, and a bake
// is a sequence of Zenith_AnimationDocument verbs. All of it is exercisable
// with no device, no view and no window.
//
// ★ THE SOLVE RUNS ON A SCRATCH POSE AND A TRANSIENT CHAIN, WHICH IS WHAT MAKES
// IT LEGAL. Flux_InverseKinematics.h warns callers off Solve() because a
// Flux_AnimationController invokes its OWN solver inside
// ApplyOutputPoseToSkeleton — so calling Solve() on the controller's pose would
// run IK twice per frame on the same data. The hazard is the DOUBLE SOLVE, not
// the function. Here:
//
//   - the pose is a stack-local Flux_SkeletonPose seeded from the skeleton
//     INSTANCE's local TRS, never Flux_AnimationController::GetOutputPose();
//   - the chain is a stack-local Flux_IKChain that is never registered with any
//     solver the controller owns, so the controller's chain map stays empty;
//   - the solver is a stack-local Flux_IKSolver, and the entry point is the
//     public SINGLE-CHAIN SolveChain, which takes no world matrix at all and so
//     forces the model-space contract rather than negotiating it through
//     Flux_IKTarget::m_bIsModelSpace against a world matrix that may lag.
//
// The controller's pose is not read and not written, its solver is not invoked,
// and nothing about the runtime IK path changes.
//
// ★ THE OUTPUT IS KEYS, NOT A LIVE SOLVE. After the fact an IK-posed frame is
// indistinguishable from a hand-posed one: the clip holds ordinary rotation
// keys, the .zanim carries nothing IK-specific, and playback needs no solver.
// That is the whole meaning of "baked down to keys".
//=============================================================================

namespace Zenith_AnimationPoseIK
{
	//-------------------------------------------------------------------------
	// How far UP the hierarchy Action_BakeIKForSelectedChain walks from the
	// SELECTED bone. The selected bone is the EFFECTOR; its parent and
	// grandparent complete the chain, which is the shoulder/elbow/wrist and
	// hip/knee/ankle shape every factory in Flux_InverseKinematics.h builds.
	// Fewer bones are used when the walk reaches a root first.
	//
	// ★ THREE IS A SCOPE CHOICE, NOT A SOLVER LIMIT (design note §6.4). FABRIK
	// handles any length; what a longer chain needs is a UI to author it with,
	// which is a later phase. Nothing below hard-codes it — it is a parameter of
	// BuildChainFromEffector.
	//-------------------------------------------------------------------------
	inline constexpr u_int kuIK_DEFAULT_CHAIN_LENGTH = 3u;

	// Two joints is the shortest thing FABRIK can express (one bone), and it is
	// the same bound Flux_IKSolver::SolveChain enforces on itself — except that it
	// enforces it by returning having done nothing, which is indistinguishable
	// from a solve that converged instantly. Refused up here instead.
	inline constexpr u_int kuIK_MIN_CHAIN_LENGTH = 2u;

	inline constexpr u_int kuIK_DEFAULT_MAX_ITERATIONS = 10u;
	inline constexpr float kfIK_DEFAULT_TOLERANCE = 0.001f;

	// A chain whose total length is below this has no direction to reach in: the
	// solver's normalize of (target - root) over a zero-length chain is the one
	// input that produces NaN for every bone at once. Refused rather than solved.
	inline constexpr float kfIK_MIN_CHAIN_LENGTH_METRES = 1.0e-5f;

	// ★ THE CHANNEL'S OWN THRESHOLD, ALIASED RATHER THAN COPIED. A quaternion
	// shorter than this cannot be normalised into a rotation, and D15 has
	// Flux_BoneChannel REFUSE one rather than normalise it — because one NaN
	// rotation key poisons every pose the clip can produce, at every time,
	// through the slerp. A second number here would let this module hand out a
	// rotation the writer then refuses, which reads as "the bake silently did
	// nothing".
	inline constexpr float kfIK_MIN_QUAT_LENGTH = fANIM_MIN_QUAT_LENGTH;

	//-------------------------------------------------------------------------
	// One solve.
	//
	// ★ THE CHAIN IS BONE INDICES, ROOT FIRST, EFFECTOR LAST. Indices rather
	// than names because every caller in the editor already has an index (bone
	// selection, the pick set, the overlay) and a name round trip is exactly
	// where a rig with two identically-named bones would silently retarget the
	// solve. The helper converts to names internally — Flux_IKChain is
	// name-keyed — and REFUSES if resolving those names does not hand back the
	// indices it was given, which is what catches that case.
	//
	// The bones do not have to be adjacent in the hierarchy: rotating a bone
	// rotates its whole subtree rigidly, so the distance between two chain
	// joints is preserved whether one is the other's child or its grandchild.
	// They DO have to be distinct, and each has to exist in both the asset and
	// the instance.
	//-------------------------------------------------------------------------
	struct SolveRequest
	{
		// Root -> effector. At least kuIK_MIN_CHAIN_LENGTH entries, all distinct.
		Zenith_Vector<u_int> m_auChainBoneIndices;

		// ★ MODEL SPACE, ALWAYS. There is no world matrix in this API and that is
		// deliberate — see the header note. A panel that has a world/drag point
		// converts it first (Zenith_BoneSpace).
		Zenith_Maths::Vector3 m_xTargetModelSpace = Zenith_Maths::Vector3(0.0f);

		// Optional bend hint, a DIRECTION in model space (matching
		// Flux_IKChain::m_xPoleVector, which is a direction and not a position).
		// Normalised on the way in; ignored unless m_bUsePoleDirection.
		Zenith_Maths::Vector3 m_xPoleDirectionModelSpace = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		bool m_bUsePoleDirection = false;

		// Empty means UNCONSTRAINED. When non-empty it must hold exactly one
		// entry per chain bone — a short list would silently constrain the wrong
		// joints, since Flux_IKSolver::ApplyConstraints indexes it positionally.
		Zenith_Vector<Flux_JointConstraint> m_axJointConstraints;

		u_int m_uMaxIterations = kuIK_DEFAULT_MAX_ITERATIONS;
		float m_fTolerance = kfIK_DEFAULT_TOLERANCE;
		// [0, 1]. Zero is legal and means "solve, then apply none of it", which
		// returns the chain's current rotations.
		float m_fWeight = 1.0f;
	};

	//-------------------------------------------------------------------------
	// The chain Action_BakeIKForSelectedChain uses: uEffectorBoneIndex and up to
	// (uMaxChainLength - 1) of its ancestors, written out ROOT FIRST.
	//
	// Refused — and axOutChainBoneIndices left untouched — when the effector does
	// not resolve, when uMaxChainLength is below kuIK_MIN_CHAIN_LENGTH, or when
	// the walk cannot reach two bones because the effector IS a root.
	//-------------------------------------------------------------------------
	bool BuildChainFromEffector(const Zenith_SkeletonAsset& xSkeleton,
		u_int uEffectorBoneIndex,
		u_int uMaxChainLength,
		Zenith_Vector<u_int>& auOutChainBoneIndices);

	//-------------------------------------------------------------------------
	// Solve one chain to a model-space target and hand back the resulting
	// BONE-LOCAL rotations, one per chain bone, in chain order (so entry i
	// belongs to xRequest.m_auChainBoneIndices.Get(i)).
	//
	// PURE with respect to engine state: it reads xInstance's local TRS and
	// writes nothing to it, touches no controller, no scene, no GPU and no file.
	//
	// ★ RETURNS FALSE AND WRITES NOTHING on any refusal. The rotations are
	// accumulated into a local buffer and only copied out once every one of them
	// has been checked finite and normalisable, so a caller can never be handed a
	// half-valid chain — one NaN rotation reaching a key would poison every pose
	// the clip can produce, at every time, through the slerp.
	//-------------------------------------------------------------------------
	bool SolveChainToLocalRotations(const Flux_SkeletonInstance& xInstance,
		const Zenith_SkeletonAsset& xSkeleton,
		const SolveRequest& xRequest,
		Zenith_Vector<Zenith_Maths::Quat>& axOutLocalRotations);

	//-------------------------------------------------------------------------
	// Write one rotation key per chain bone at fTimeSeconds, as ONE undo step.
	//
	// ★ THIS IS THE FALLBACK PATH, NOT THE PREFERRED ONE. The panel's
	// Action_BakeIKForSelectedChain goes through Action_SetKeyForBones — the same
	// verb a hand drag uses (design note §5.4) — so a key written by an IK bake
	// and a key written by a drag are byte-identical. This exists because
	// WU-4.3's body lands separately, and because the solve has to be testable
	// end-to-end without depending on it. It writes through exactly the same
	// document verbs, so what reaches the clip is the same either way.
	//
	// The time is snapped to the frame grid (uFrameRate == 0 means unsnapped, per
	// Zenith_AnimTimelineSnapToFrame). Every bone and every rotation is validated
	// BEFORE the compound is opened, and any refusal inside it rolls the whole
	// bake back rather than leaving a partial pose on the undo stack.
	//-------------------------------------------------------------------------
	bool BakeChain(Zenith_AnimationDocument& xDocument,
		const Zenith_SkeletonAsset& xSkeleton,
		const Zenith_Vector<u_int>& auChainBoneIndices,
		const Zenith_Vector<Zenith_Maths::Quat>& axLocalRotations,
		float fTimeSeconds,
		u_int uFrameRate);
}

// ★ NO FORCE-LINK ANCHOR HERE, unlike Zenith_BoneSpace and
// Zenith_AnimationDocument, and the difference is worth saying out loud rather
// than looking like an omission. Those two are anchored because NOTHING
// references them: an .obj the linker never pulls in takes its ZENITH_TEST
// registrars with it and the unit count moves by zero with nothing going red.
// This TU is referenced for real — Zenith_EditorPanel_Animation_IK.cpp calls
// all three functions above from Action_BakeIKForSelectedChain — so an anchor
// would be a symbol nobody calls standing in for a dependency that exists. If
// that call ever goes away the units go with it, and the gate's exact
// `ran == baseline` check is what turns that into a red rather than a silence.

#endif // ZENITH_TOOLS
