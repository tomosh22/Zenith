#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Animation/Zenith_AnimationPoseIK.h"

#include "Editor/Zenith_AnimationDocument.h"
#include "Editor/Zenith_AnimTimelineMath.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

#include <cmath>

namespace
{
	// ★ THE SIBLING'S IDIOM, NOT std::isfinite, and deliberately so: this is the
	// same expression Zenith_BonePickGeometry.cpp's BonePickIsFinite uses, one
	// directory over and in the same phase. The project compiles /fp:fast, under
	// which a compiler is entitled to assume no NaNs and fold an isfinite() call
	// to a constant; the self-comparison plus a magnitude bound survives that
	// because the bound is a real comparison against a real number.
	bool PoseIKIsFinite(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	bool PoseIKIsFinite(const Zenith_Maths::Vector3& xValue)
	{
		return PoseIKIsFinite(xValue.x) && PoseIKIsFinite(xValue.y) && PoseIKIsFinite(xValue.z);
	}

	bool PoseIKIsFinite(const Zenith_Maths::Quat& xValue)
	{
		return PoseIKIsFinite(xValue.w) && PoseIKIsFinite(xValue.x)
			&& PoseIKIsFinite(xValue.y) && PoseIKIsFinite(xValue.z);
	}

	// A rotation this module is willing to hand out or write: finite AND long
	// enough that glm::normalize is meaningful. The two are separate checks
	// because a quaternion of exactly zero is finite and still unusable.
	bool PoseIKIsUsableRotation(const Zenith_Maths::Quat& xValue)
	{
		if (!PoseIKIsFinite(xValue))
		{
			return false;
		}
		return glm::length(xValue) >= Zenith_AnimationPoseIK::kfIK_MIN_QUAT_LENGTH;
	}

	//--------------------------------------------------------------------------
	// How many bones both the asset and the live instance agree exist. A pose is
	// only composable over the intersection: the asset owns the parent chain and
	// the instance owns the TRS, and an index past either of them reads a slot
	// nothing wrote.
	//--------------------------------------------------------------------------
	u_int PoseIKUsableBoneCount(const Flux_SkeletonInstance& xInstance, const Zenith_SkeletonAsset& xSkeleton)
	{
		u_int uCount = xInstance.GetNumBones();
		if (xSkeleton.GetNumBones() < uCount)
		{
			uCount = xSkeleton.GetNumBones();
		}
		if (uCount > FLUX_MAX_BONES)
		{
			uCount = FLUX_MAX_BONES;
		}
		return uCount;
	}

	bool PoseIKChainIndicesAreDistinct(const Zenith_Vector<u_int>& auIndices)
	{
		for (u_int uA = 0; uA < auIndices.GetSize(); ++uA)
		{
			for (u_int uB = uA + 1u; uB < auIndices.GetSize(); ++uB)
			{
				if (auIndices.Get(uA) == auIndices.Get(uB))
				{
					return false;
				}
			}
		}
		return true;
	}
}

//=============================================================================
// BuildChainFromEffector
//=============================================================================
bool Zenith_AnimationPoseIK::BuildChainFromEffector(const Zenith_SkeletonAsset& xSkeleton,
	u_int uEffectorBoneIndex,
	u_int uMaxChainLength,
	Zenith_Vector<u_int>& auOutChainBoneIndices)
{
	if (uMaxChainLength < kuIK_MIN_CHAIN_LENGTH)
	{
		return false;
	}
	if (uEffectorBoneIndex >= xSkeleton.GetNumBones())
	{
		return false;
	}

	// Collected EFFECTOR-FIRST because that is the direction the parent chain
	// runs, then reversed once at the end. Building it backwards in place would
	// need an insert-at-front per level, which is the same reversal spelled worse.
	Zenith_Vector<u_int> auWalk;
	auWalk.Reserve(uMaxChainLength);

	u_int uCurrent = uEffectorBoneIndex;
	while (auWalk.GetSize() < uMaxChainLength)
	{
		auWalk.PushBack(uCurrent);

		const int32_t iParent = xSkeleton.GetBone(uCurrent).m_iParentIndex;
		if (iParent == Zenith_SkeletonAsset::INVALID_BONE_INDEX || iParent < 0)
		{
			break;
		}
		if (static_cast<u_int>(iParent) >= xSkeleton.GetNumBones())
		{
			// A malformed asset. Stop where the chain is still describable rather
			// than indexing past the bone array.
			break;
		}
		// The asset's own ordering invariant (parents precede children) makes a
		// cycle impossible, but the loop is bounded by uMaxChainLength anyway so a
		// corrupted asset cannot hang the editor.
		uCurrent = static_cast<u_int>(iParent);
	}

	if (auWalk.GetSize() < kuIK_MIN_CHAIN_LENGTH)
	{
		// The effector is a root: there is no bone above it to bend.
		return false;
	}

	auOutChainBoneIndices.Clear();
	auOutChainBoneIndices.Reserve(auWalk.GetSize());
	for (u_int u = auWalk.GetSize(); u > 0u; --u)
	{
		auOutChainBoneIndices.PushBack(auWalk.Get(u - 1u));
	}
	return true;
}

//=============================================================================
// SolveChainToLocalRotations — the nine-step body of design note §6.2.
//=============================================================================
bool Zenith_AnimationPoseIK::SolveChainToLocalRotations(const Flux_SkeletonInstance& xInstance,
	const Zenith_SkeletonAsset& xSkeleton,
	const SolveRequest& xRequest,
	Zenith_Vector<Zenith_Maths::Quat>& axOutLocalRotations)
{
	//-------------------------------------------------------------------------
	// (0) Validate everything the solver would otherwise consume silently.
	//     Flux_IKSolver::SolveChain has no error channel — it simply returns
	//     having done nothing, or divides by a zero it was handed — so every
	//     refusal has to happen here, before any work.
	//-------------------------------------------------------------------------
	const u_int uChainLength = xRequest.m_auChainBoneIndices.GetSize();
	if (uChainLength < kuIK_MIN_CHAIN_LENGTH)
	{
		return false;
	}

	const u_int uUsableBones = PoseIKUsableBoneCount(xInstance, xSkeleton);
	if (uUsableBones == 0u)
	{
		return false;
	}

	for (u_int u = 0; u < uChainLength; ++u)
	{
		if (xRequest.m_auChainBoneIndices.Get(u) >= uUsableBones)
		{
			return false;
		}
	}
	if (!PoseIKChainIndicesAreDistinct(xRequest.m_auChainBoneIndices))
	{
		return false;
	}

	if (!PoseIKIsFinite(xRequest.m_xTargetModelSpace))
	{
		return false;
	}
	if (!PoseIKIsFinite(xRequest.m_fWeight) || xRequest.m_fWeight < 0.0f || xRequest.m_fWeight > 1.0f)
	{
		return false;
	}
	if (!PoseIKIsFinite(xRequest.m_fTolerance) || xRequest.m_fTolerance <= 0.0f)
	{
		return false;
	}
	if (xRequest.m_uMaxIterations == 0u)
	{
		return false;
	}
	// A short constraint list would constrain the WRONG joints:
	// Flux_IKSolver::ApplyConstraints indexes it positionally against the chain.
	if (xRequest.m_axJointConstraints.GetSize() != 0u
		&& xRequest.m_axJointConstraints.GetSize() != uChainLength)
	{
		return false;
	}

	//-------------------------------------------------------------------------
	// (1) The SCRATCH pose. Stack-local, sized to the rig, and never the
	//     controller's — see the header note on why that is the whole trick.
	//-------------------------------------------------------------------------
	Flux_SkeletonPose xScratch;
	xScratch.Initialize(uUsableBones);

	//-------------------------------------------------------------------------
	// (2) Seed it from the INSTANCE's current local TRS, so the solve starts
	//     from the pose the author is looking at rather than from the bind pose
	//     or from whatever the clip last evaluated to.
	//-------------------------------------------------------------------------
	for (u_int u = 0; u < uUsableBones; ++u)
	{
		Flux_BoneLocalPose& xLocal = xScratch.GetLocalPose(u);
		xLocal.m_xPosition = xInstance.GetBoneLocalPosition(u);
		xLocal.m_xRotation = xInstance.GetBoneLocalRotation(u);
		xLocal.m_xScale = xInstance.GetBoneLocalScale(u);

		if (!PoseIKIsFinite(xLocal.m_xPosition) || !PoseIKIsUsableRotation(xLocal.m_xRotation)
			|| !PoseIKIsFinite(xLocal.m_xScale))
		{
			// The pose being solved FROM is already broken. Solving it would
			// launder the NaN into keys, which is the one outcome worse than
			// refusing.
			return false;
		}
	}

	//-------------------------------------------------------------------------
	// (3) The TRANSIENT chain. Flux_IKChain is name-keyed, so the indices are
	//     converted to names and then resolved back — and the round trip is
	//     CHECKED, which is what catches a rig carrying two bones with the same
	//     name (GetBoneIndex answers with the first, and the solve would quietly
	//     bend a different limb).
	//-------------------------------------------------------------------------
	Flux_IKChain xChain;
	xChain.m_strName = "EditorPoseIK";
	xChain.m_uMaxIterations = xRequest.m_uMaxIterations;
	xChain.m_fTolerance = xRequest.m_fTolerance;
	xChain.m_bUsePoleVector = xRequest.m_bUsePoleDirection;
	xChain.m_xBoneNames.Reserve(uChainLength);

	for (u_int u = 0; u < uChainLength; ++u)
	{
		const std::string& strName = xSkeleton.GetBone(xRequest.m_auChainBoneIndices.Get(u)).m_strName;
		if (strName.empty())
		{
			return false;
		}
		xChain.m_xBoneNames.PushBack(strName);
	}

	if (xRequest.m_bUsePoleDirection)
	{
		if (!PoseIKIsFinite(xRequest.m_xPoleDirectionModelSpace))
		{
			return false;
		}
		// A direction, never a position — matching Flux_IKChain::m_xPoleVector's
		// own documented convention. SafeNormalize keeps a zero-length hint from
		// becoming a NaN axis; it degrades to "no usable hint" instead.
		xChain.m_xPoleVector = Flux_IKSolver::SafeNormalize(xRequest.m_xPoleDirectionModelSpace,
			Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	}

	for (u_int u = 0; u < xRequest.m_axJointConstraints.GetSize(); ++u)
	{
		xChain.m_xJointConstraints.PushBack(xRequest.m_axJointConstraints.Get(u));
	}

	xChain.ResolveBoneIndices(xSkeleton);
	if (xChain.m_xBoneIndices.GetSize() != uChainLength)
	{
		return false;
	}
	for (u_int u = 0; u < uChainLength; ++u)
	{
		if (xChain.m_xBoneIndices.Get(u) != xRequest.m_auChainBoneIndices.Get(u))
		{
			return false;
		}
	}

	//-------------------------------------------------------------------------
	// (4) Compose model space, (5) measure the chain against THAT pose.
	//     Order matters: ComputeBoneLengths reads model-space translations, so it
	//     cannot run before the compose, and the compose cannot run before the
	//     indices resolve.
	//-------------------------------------------------------------------------
	xScratch.ComputeModelSpaceMatricesFromSkeleton(xSkeleton);
	xChain.ComputeBoneLengths(xScratch);

	if (xChain.m_xBoneLengths.GetSize() != uChainLength - 1u)
	{
		return false;
	}
	if (!PoseIKIsFinite(xChain.m_fTotalLength) || xChain.m_fTotalLength < kfIK_MIN_CHAIN_LENGTH_METRES)
	{
		// Every joint sits on top of the next one. There is no direction to reach
		// in, and SolveChain's normalize of (target - root) over a zero-length
		// chain is exactly the input that NaNs every bone at once.
		return false;
	}

	//-------------------------------------------------------------------------
	// (6) The target. m_bIsModelSpace is set because SolveChain takes no world
	//     matrix at all; the flag is what documents that at the call site.
	//-------------------------------------------------------------------------
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = xRequest.m_xTargetModelSpace;
	xTarget.m_fWeight = xRequest.m_fWeight;
	xTarget.m_bEnabled = true;
	xTarget.m_bIsModelSpace = true;
	// End-effector ORIENTATION is deliberately not driven: Phase 4 authors a
	// reach, and an unrequested wrist twist is a pose change the author did not
	// ask for and cannot see the cause of.
	xTarget.m_bUseRotation = false;

	//-------------------------------------------------------------------------
	// (7) Solve, on OUR solver and OUR pose.
	//-------------------------------------------------------------------------
	Flux_IKSolver xSolver;
	xSolver.SolveChain(xScratch, xChain, xTarget, xSkeleton);

	//-------------------------------------------------------------------------
	// (8) Recompose. Flux_AnimationController does the same thing on both sides
	//     of its own Solve, and its comment says why: the post-solve recompute is
	//     what keeps the model matrices consistent for "downstream CPU readers
	//     (debug draw, gizmos, animation tools)". That is us — a caller that
	//     writes these rotations back and then reads a model matrix must not see
	//     the pre-solve geometry.
	//-------------------------------------------------------------------------
	xScratch.ComputeModelSpaceMatricesFromSkeleton(xSkeleton);

	//-------------------------------------------------------------------------
	// (9) Extract, validate, and only THEN publish. Nothing reaches the caller's
	//     vector until every rotation in the chain has passed.
	//-------------------------------------------------------------------------
	Zenith_Vector<Zenith_Maths::Quat> axSolved;
	axSolved.Reserve(uChainLength);
	for (u_int u = 0; u < uChainLength; ++u)
	{
		const Zenith_Maths::Quat& xRotation =
			xScratch.GetLocalPose(xRequest.m_auChainBoneIndices.Get(u)).m_xRotation;
		if (!PoseIKIsUsableRotation(xRotation))
		{
			return false;
		}
		axSolved.PushBack(glm::normalize(xRotation));
	}

	axOutLocalRotations.Clear();
	axOutLocalRotations.Reserve(uChainLength);
	for (u_int u = 0; u < uChainLength; ++u)
	{
		axOutLocalRotations.PushBack(axSolved.Get(u));
	}
	return true;
}

//=============================================================================
// BakeChain
//=============================================================================
bool Zenith_AnimationPoseIK::BakeChain(Zenith_AnimationDocument& xDocument,
	const Zenith_SkeletonAsset& xSkeleton,
	const Zenith_Vector<u_int>& auChainBoneIndices,
	const Zenith_Vector<Zenith_Maths::Quat>& axLocalRotations,
	float fTimeSeconds,
	u_int uFrameRate)
{
	if (!xDocument.IsOpen())
	{
		return false;
	}

	const u_int uChainLength = auChainBoneIndices.GetSize();
	if (uChainLength == 0u || axLocalRotations.GetSize() != uChainLength)
	{
		return false;
	}
	if (!PoseIKIsFinite(fTimeSeconds) || fTimeSeconds < 0.0f)
	{
		return false;
	}

	// ★ SNAPPED THROUGH THE ONE DEFINITION OF THE GRID. A second rounding rule
	// here would put a baked key a hair off the frame a hand-authored one lands
	// on, and the mutator's fANIM_TIME_EPSILON would then read the two as
	// different times — so a re-bake would stack keys instead of replacing them.
	const float fSnapped = Zenith_AnimTimelineSnapToFrame(fTimeSeconds, uFrameRate);
	if (!PoseIKIsFinite(fSnapped) || fSnapped < 0.0f)
	{
		return false;
	}

	// ★ VALIDATE EVERY BONE BEFORE OPENING THE COMPOUND. A refusal discovered
	// halfway through is recoverable (EndCompound(false) rolls it back), but the
	// document is left dirty by the verbs that did run — so the cheap checks
	// happen where they cost nothing.
	for (u_int u = 0; u < uChainLength; ++u)
	{
		if (auChainBoneIndices.Get(u) >= xSkeleton.GetNumBones())
		{
			return false;
		}
		if (xSkeleton.GetBone(auChainBoneIndices.Get(u)).m_strName.empty())
		{
			return false;
		}
		if (!PoseIKIsUsableRotation(axLocalRotations.Get(u)))
		{
			return false;
		}
	}

	if (!xDocument.BeginCompound())
	{
		return false;
	}

	bool bAllWritten = true;
	for (u_int u = 0; u < uChainLength; ++u)
	{
		const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone(
			xSkeleton.GetBone(auChainBoneIndices.Get(u)).m_strName, FLUX_ANIM_TRACK_ROTATION);

		// D25: on an occupied time this is a VALUE EDIT that keeps the key's id,
		// which is exactly what re-baking the same frame should do.
		if (xDocument.InsertKey(xTrack, fSnapped, axLocalRotations.Get(u)) == uINVALID_ANIM_KEY_ID)
		{
			bAllWritten = false;
			break;
		}
	}

	// bKeep == bAllWritten: a partial bake is undone in reverse and never reaches
	// the stack, so the author never has to press Ctrl+Z through a pose nobody
	// asked for.
	const bool bPushed = xDocument.EndCompound("Bake IK Pose", bAllWritten);
	return bAllWritten && bPushed;
}

#ifdef ZENITH_TESTING
#include "Editor/Animation/Zenith_AnimationPoseIK.Tests.inl"
#endif

#endif // ZENITH_TOOLS
