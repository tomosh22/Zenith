#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"

#include "Editor/Animation/Zenith_AnimationPoseIK.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

//=============================================================================
// The IK half of the dope sheet's pose authoring (WU-4.4): ONE action body,
// in its own TU behind WU-4.1's declaration.
//
// ★ NOT ONE LINE OF ImGui IN THIS FILE, for the same reason
// Zenith_EditorPanel_Animation_Ops.cpp has none: the action reads no mouse, no
// modifier and no focus, so a unit (and an authoring step) can perform the whole
// gesture without synthesising input. There is deliberately NO IK section drawn
// in the preview pane here — the pane is composed by RenderPreviewPane in the
// _Render TU, which WU-4.4 does not own and which offers no hook a free function
// could attach to. The verb is complete and callable; the widget that aims it is
// a follow-up.
//
// ★ THE SOLVE ITSELF IS NOT HERE. It lives in Zenith_AnimationPoseIK, which is
// pure, owns no panel state and is unit-tested against skeletons built in code.
// This file is the wiring: which bones, from what pose, to what target, and
// where the result is written.
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_BakeIKForSelectedChain(const Zenith_Maths::Vector3& xTargetModelSpace)
{
	//-------------------------------------------------------------------------
	// Preconditions. Each is its own early return rather than one compound
	// condition, because a bare `false` from an action already has too many
	// causes to be readable and collapsing five of them into one line makes it
	// worse for anyone stepping through.
	//-------------------------------------------------------------------------
	if (!m_xDocument.IsOpen())
	{
		return false;
	}
	if (!m_xSession.IsOpen() || m_xSession.NeedsRigSelection())
	{
		return false;
	}
	if (!m_xSession.HasBoneSelection())
	{
		return false;
	}

	Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr)
	{
		return false;
	}
	const Zenith_SkeletonAsset* pxSkeleton = pxInstance->GetSourceSkeleton();
	if (pxSkeleton == nullptr)
	{
		return false;
	}

	//-------------------------------------------------------------------------
	// ★ THE SELECTED BONE IS THE EFFECTOR, and the chain walks UP from it —
	// kuIK_DEFAULT_CHAIN_LENGTH bones, or fewer when the walk reaches a root
	// (design note §6.4). Clicking a wrist and dragging therefore bends the
	// elbow and the shoulder, which is what a poser expects; clicking a
	// shoulder and dragging bends only what is above it, and clicking a ROOT is
	// refused because there is nothing above it to bend.
	//
	// No chain-authoring UI, no serialization, no constraints and no pole
	// vector in Phase 4 — the three-bone limit is a scope choice, not a solver
	// limit.
	//-------------------------------------------------------------------------
	Zenith_Vector<u_int> auChainBones;
	if (!Zenith_AnimationPoseIK::BuildChainFromEffector(*pxSkeleton,
		m_xSession.GetSelectedBoneIndex(),
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH,
		auChainBones))
	{
		return false;
	}

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = auChainBones;
	xRequest.m_xTargetModelSpace = xTargetModelSpace;

	Zenith_Vector<Zenith_Maths::Quat> axLocalRotations;
	if (!Zenith_AnimationPoseIK::SolveChainToLocalRotations(*pxInstance, *pxSkeleton, xRequest, axLocalRotations))
	{
		return false;
	}

	//-------------------------------------------------------------------------
	// Apply to the LIVE pose exactly the way a hand drag does — a direct
	// SetBoneLocalTransform followed by RefreshDerivedPose.
	//
	// ★ RefreshDerivedPose IS NOT OPTIONAL (design note §3.2).
	// Flux_SkeletonInstance::GetBoneModelTransform is a CACHE that only
	// ComputeSkinningMatrices writes; SetBoneLocalTransform neither updates nor
	// invalidates it. Without this call the pick shapes and the bone overlay
	// would be one write stale, with no assert and no symptom except lag.
	//-------------------------------------------------------------------------
	for (u_int u = 0; u < auChainBones.GetSize(); ++u)
	{
		const u_int uBone = auChainBones.Get(u);
		// Copied out of the instance first: the getters return references INTO
		// the arrays the setter writes.
		const Zenith_Maths::Vector3 xPosition = pxInstance->GetBoneLocalPosition(uBone);
		const Zenith_Maths::Vector3 xScale = pxInstance->GetBoneLocalScale(uBone);
		pxInstance->SetBoneLocalTransform(uBone, xPosition, axLocalRotations.Get(u), xScale);
	}
	m_xSession.RefreshDerivedPose();

	//-------------------------------------------------------------------------
	// ★ THE KEYS GO THROUGH Action_SetKeyForBones — THE SAME VERB A HAND DRAG
	// USES (design note §5.4/§6.3). That is what makes an IK-posed frame
	// indistinguishable from a hand-posed one after the fact: one undo entry
	// covers the whole gesture and the clip contains nothing IK-specific.
	// bRotation, and NOT translation: the chain bones were rotated, and writing
	// a track the author did not touch would change frames they never edited
	// (SampleFromClip writes a component only if that channel HAS keyframes, so
	// the FIRST key on a channel changes that bone's behaviour across the whole
	// clip).
	//
	// ★ THE FALLBACK BELOW EXISTS BECAUSE WU-4.3 LANDS SEPARATELY, and the
	// panel's own unit currently asserts that Action_SetKeyForBones still
	// refuses. It writes through the same Zenith_AnimationDocument verbs, in the
	// same compound, so what reaches the clip is identical either way — the
	// difference is only which function put it there. It is deliberately keyed
	// on the ACTION's return value rather than on a build flag: a genuine
	// refusal from a landed 4.3 (nothing written) is exactly the situation the
	// fallback is safe in, and a success short-circuits before it.
	//-------------------------------------------------------------------------
	if (Action_SetKeyForBones(auChainBones, true, false))
	{
		// Nothing is done here afterwards ON PURPOSE. Marking the document
		// edited and clearing the unkeyed-pose flag are obligations of THE
		// key-writing verb, not of each of its callers — the Set Key button and
		// auto-key-on-release need them just as much, and a copy here is how
		// three call sites end up disagreeing about which of them ran.
		return true;
	}

	const bool bBaked = Zenith_AnimationPoseIK::BakeChain(m_xDocument, *pxSkeleton,
		auChainBones, axLocalRotations, m_xSession.GetTime(), GetFrameRate());
	if (bBaked)
	{
		// The session re-copies the clip on the next Render (an undo/redo depth
		// is a heuristic; this is the exact signal), and the pose is no longer
		// unkeyed — it is in the document now, which is the whole point.
		NotifyDocumentEdited();
		m_xSession.ClearUnkeyedPose();
	}
	return bBaked;
}

#endif // ZENITH_TOOLS
