#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"

#include "Editor/Animation/Zenith_AnimationPoseIK.h"
#include "Editor/Animation/Zenith_BoneSpace.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

#include <cmath>

//=============================================================================
// The IK half of the dope sheet's pose authoring: the BAKE verb (WU-4.4) and
// the TARGET WIDGET that aims it (E1), in one TU behind WU-4.1's declarations.
//
// ★ NOT ONE LINE OF ImGui IN THIS FILE, for the same reason
// Zenith_EditorPanel_Animation_Ops.cpp has none: every verb below takes a
// PIXEL, a target or nothing at all, and reads no mouse, no modifier and no
// focus — so a unit (and an authoring step) performs the whole gesture without
// synthesising input. The two functions that DO read the mouse — the input
// translation and the handle's draw — live in the _Render TU beside the pane
// they belong to, and each of them ends in one of these.
//
// ★ THE SOLVE ITSELF IS NOT HERE. It lives in Zenith_AnimationPoseIK, which is
// pure, owns no panel state and is unit-tested against skeletons built in code.
// This file is the wiring: which bones, from what pose, to what target, and
// where the result is written.
//
// ★ AND THE WIDGET IS A POINT, NOT A GIZMO. What an IK drag needs on screen is
// one draggable position; the three rotation rings beside it turn ONE bone,
// which is a different gesture with a different pivot. Both are ImGui draw-list
// decorations over the preview image (design note §8.1) and both latch at
// press, so the one rule they need between them is that each refuses to start
// while the other is live — see Action_BeginIKDragAtPixel.
//=============================================================================

//=============================================================================
// The target: where it sits, and where it lands on screen.
//=============================================================================

void Zenith_EditorPanel_Animation::SeedIKTargetFromSelection()
{
	if (m_bIKDragActive)
	{
		// The drag OWNS the target for the length of the gesture. Re-seeding
		// under it would snap the handle back to the joint on the next frame and
		// the solve would chase a target the cursor is not on.
		return;
	}

	const u_int uBone = m_xSession.IsOpen()
		? m_xSession.GetSelectedBoneIndex()
		: kuINVALID_BONE_SELECTION;
	const Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (uBone == kuINVALID_BONE_SELECTION || pxInstance == nullptr)
	{
		// No effector, so no target. GetIKTargetModelSpace and GetIKHandlePixel
		// both answer false off this, which is what makes "nothing selected draws
		// NOTHING" one test rather than three.
		m_uIKSeededForBone = kuINVALID_BONE_SELECTION;
		return;
	}
	m_uIKSeededForBone = uBone;

	// ★ RE-TAKEN WHENEVER NO DRAG OWNS IT, not only when the SELECTION moved, and
	// the difference is a wart rather than a nicety. The effector's joint moves
	// for reasons other than a selection change — a seek, a ring drag on an
	// ancestor, an undo — and a target seeded once would sit where the bone USED
	// to be, showing a leader line to nothing and snapping the chain back to a
	// stale point on the first pixel of the next grab. Re-taking every frame
	// makes the resting contract simply "the handle IS the effector; drag it to
	// reach", and costs one matrix read. A released drag leaves the two
	// coincident anyway — the bake put the effector ON the target — so nothing
	// the user placed is thrown away by this.
	//
	// ★ THE EFFECTOR'S OWN MODEL-SPACE JOINT. MODEL space rather than world
	// because that is the space Zenith_AnimationPoseIK solves in and the space
	// Action_BakeIKForSelectedChain takes — converting once, here, is what keeps
	// the session model matrix out of every expression below it.
	//
	// This reads the model-transform CACHE (design note §3.2), which is current
	// because every writer of the live pose ends in RefreshDerivedPose and both
	// Tick and Seek route through ComputeSkinningMatrices.
	const Zenith_Maths::Matrix4 xModel = Zenith_BoneSpace::BoneModelMatrix(*pxInstance, uBone);
	m_xIKTarget = Zenith_Maths::Vector3(xModel[3].x, xModel[3].y, xModel[3].z);
}

bool Zenith_EditorPanel_Animation::GetIKTargetModelSpace(Zenith_Maths::Vector3& xOut) const
{
	if (m_uIKSeededForBone == kuINVALID_BONE_SELECTION)
	{
		return false;
	}
	xOut = m_xIKTarget;
	return true;
}

bool Zenith_EditorPanel_Animation::GetIKHandlePixel(float& fOutPixelX, float& fOutPixelY) const
{
	// ★ NOTHING IS OFFERED WITHOUT A BONE SELECTION — the same rule the ring set
	// keeps (GetPoseRingSet) and the pose toolbar keeps: with nothing selected
	// there is no effector, so a handle would be a control that cannot act.
	if (!m_xSession.IsOpen() || !m_xSession.HasBoneSelection() ||
		m_uIKSeededForBone == kuINVALID_BONE_SELECTION)
	{
		return false;
	}

	const Zenith_Maths::Vector4 xWorld =
		m_xSession.GetSessionModelMatrix() * Zenith_Maths::Vector4(m_xIKTarget, 1.0f);
	// ProjectPreviewWorldPoint refuses without a rendered frame and refuses a
	// point behind the camera, which are the two ways this can legitimately have
	// no answer.
	return ProjectPreviewWorldPoint(
		Zenith_Maths::Vector3(xWorld.x, xWorld.y, xWorld.z), fOutPixelX, fOutPixelY);
}

//=============================================================================
// The drag transaction.
//
// ★ THE SAME TWO-LAYER RULE AS THE RING DRAG (design note §4.1): the live pose
// is rewritten every mouse move and is neither undoable nor serialized; the
// KEYS are written ONCE, on release, through Action_BakeIKForSelectedChain and
// therefore through Action_SetKeyForBones. Nothing is pushed on an undo stack
// between Begin and End.
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_BeginIKDragAtPixel(float fPixelX, float fPixelY)
{
	// ★ THE TWO MANIPULATORS EXCLUDE EACH OTHER, and this is one half of that
	// rule (Action_BeginBoneDragAtPixel carries the other). They latch different
	// things and both open the session's single drag bracket, so a second Begin
	// would leave one latch belonging to nobody and one mouse-up ending the
	// wrong transaction.
	if (m_bIKDragActive || m_bPoseDragActive)
	{
		return false;
	}
	if (!m_xDocument.IsOpen() || !m_xSession.IsOpen() || m_xSession.NeedsRigSelection() ||
		!m_xSession.HasBoneSelection())
	{
		return false;
	}

	SeedIKTargetFromSelection();

	float fHandleX = 0.0f;
	float fHandleY = 0.0f;
	if (!GetIKHandlePixel(fHandleX, fHandleY))
	{
		return false;
	}
	const float fDx = fPixelX - fHandleX;
	const float fDy = fPixelY - fHandleY;
	// The RINGS' tolerance, deliberately, rather than a second number: both
	// handles are grabbed out of the same image-pixel space with the same
	// cursor, and two tolerances is how "I can hit the ring but not the target"
	// becomes a bug report nobody can reproduce.
	if (std::sqrt(fDx * fDx + fDy * fDy) > fANIM_POSE_RING_GRAB_PIXELS)
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

	// ★ THE CHAIN IS BUILT ONCE, HERE, and it is the SAME walk
	// Action_BakeIKForSelectedChain makes — the selected bone is the effector
	// and the walk goes UP. Building it per mouse move would re-read a pose the
	// previous move wrote.
	Zenith_Vector<u_int> auChainBones;
	if (!Zenith_AnimationPoseIK::BuildChainFromEffector(*pxSkeleton,
		m_xSession.GetSelectedBoneIndex(),
		Zenith_AnimationPoseIK::kuIK_DEFAULT_CHAIN_LENGTH,
		auChainBones))
	{
		// A ROOT effector has nothing above it to bend, which is a refusal rather
		// than a solve that does nothing.
		return false;
	}

	// ★ THE LATCH. Every Update restores these before it solves, because
	// Zenith_AnimationPoseIK seeds from the live TRS: solving from the previous
	// solve's output COMPOSES, and a pose that depends on how many mouse moves
	// the drag happened to span is not a pose anybody can author twice.
	m_axIKLatchedLocalRotations.Clear();
	for (u_int u = 0; u < auChainBones.GetSize(); ++u)
	{
		m_axIKLatchedLocalRotations.PushBack(pxInstance->GetBoneLocalRotation(auChainBones.Get(u)));
	}
	m_auIKDragChainBones = auChainBones;

	// The session's bracket goes on the EFFECTOR — the bone the selection names
	// and the last entry of the chain. That is what suspends Tick for the length
	// of the gesture (§4.2) and what UpdateBoneDrag writes through below.
	if (!m_xSession.BeginBoneDrag(m_xSession.GetSelectedBoneIndex()))
	{
		m_auIKDragChainBones.Clear();
		m_axIKLatchedLocalRotations.Clear();
		return false;
	}

	// The drag plane: through the target, facing the camera, frozen. The ray's
	// ORIGIN is the orbit camera position (BuildPreviewRay's own contract), so
	// this needs no second copy of the camera maths.
	Zenith_Maths::Vector3 xRayOrigin(0.0f);
	Zenith_Maths::Vector3 xRayDir(0.0f);
	if (!BuildPreviewRay(fPixelX, fPixelY, xRayOrigin, xRayDir))
	{
		m_xSession.EndBoneDrag();
		m_auIKDragChainBones.Clear();
		m_axIKLatchedLocalRotations.Clear();
		return false;
	}

	const Zenith_Maths::Vector4 xTargetWorld =
		m_xSession.GetSessionModelMatrix() * Zenith_Maths::Vector4(m_xIKTarget, 1.0f);
	m_xIKDragPlanePoint = Zenith_Maths::Vector3(xTargetWorld.x, xTargetWorld.y, xTargetWorld.z);

	const Zenith_Maths::Vector3 xToCamera = xRayOrigin - m_xIKDragPlanePoint;
	if (!(Zenith_Maths::LengthSq(xToCamera) > 1.0e-12f))
	{
		// The camera is inside the target. There is no plane to drag on, and
		// normalising this would put a NaN into a bone rotation and then into a
		// saved .zanim.
		m_xSession.EndBoneDrag();
		m_auIKDragChainBones.Clear();
		m_axIKLatchedLocalRotations.Clear();
		return false;
	}
	m_xIKDragPlaneNormal = Zenith_Maths::Normalize(xToCamera);

	m_xIKDragStartTarget = m_xIKTarget;
	m_fIKDragStartPixelX = fPixelX;
	m_fIKDragStartPixelY = fPixelY;
	m_bIKWasUnkeyedAtDragStart = m_xSession.HasUnkeyedPose();
	m_bIKDragMoved = false;
	m_bIKDragActive = true;
	return true;
}

bool Zenith_EditorPanel_Animation::Action_UpdateIKDragToPixel(float fPixelX, float fPixelY)
{
	if (!m_bIKDragActive)
	{
		return false;
	}

	// ★ PIXEL HALF OF THE DEAD ZONE, ASKED FIRST. A cursor that has not left the
	// press pixel is a press, whatever reprojecting that pixel says (see
	// fANIM_IK_DRAG_DEAD_ZONE_PIXELS).
	if (!m_bIKDragMoved)
	{
		const float fDx = fPixelX - m_fIKDragStartPixelX;
		const float fDy = fPixelY - m_fIKDragStartPixelY;
		if (fDx * fDx + fDy * fDy <= fANIM_IK_DRAG_DEAD_ZONE_PIXELS * fANIM_IK_DRAG_DEAD_ZONE_PIXELS)
		{
			return true;
		}
	}

	Zenith_Maths::Vector3 xRayOrigin(0.0f);
	Zenith_Maths::Vector3 xRayDir(0.0f);
	if (!BuildPreviewRay(fPixelX, fPixelY, xRayOrigin, xRayDir))
	{
		// The pane stopped being able to answer mid-gesture (no rendered frame).
		// The drag stays open — the mouse button is still down — and the pose is
		// left exactly where the last good move put it.
		return true;
	}

	const float fDenominator = Zenith_Maths::Dot(xRayDir, m_xIKDragPlaneNormal);
	if (std::fabs(fDenominator) < 1.0e-6f)
	{
		// The cursor is on the plane's horizon: the ray is parallel to it and the
		// intersection is at infinity. Ignored rather than clamped — a clamp
		// would teleport the target to the edge of the world for one frame.
		return true;
	}
	const float fT = Zenith_Maths::Dot(m_xIKDragPlanePoint - xRayOrigin, m_xIKDragPlaneNormal)
		/ fDenominator;
	if (!(fT > 0.0f))
	{
		// Behind the camera.
		return true;
	}

	const Zenith_Maths::Vector3 xHitWorld = xRayOrigin + xRayDir * fT;
	const Zenith_Maths::Vector4 xHitModel =
		glm::inverse(m_xSession.GetSessionModelMatrix()) * Zenith_Maths::Vector4(xHitWorld, 1.0f);
	const Zenith_Maths::Vector3 xTarget(xHitModel.x, xHitModel.y, xHitModel.z);

	// ★ MEASURED FROM THE PRESS, NOT FROM THE PREVIOUS FRAME — the position twin
	// of the ring drag's dead zone. Until the cursor has actually left the
	// handle NOTHING is written: a solve at the press position would move the
	// chain by float noise and would still raise the unkeyed-pose badge, so a
	// press-and-release would leave the pane warning about a pose nobody changed.
	const bool bNonTrivial =
		Zenith_Maths::LengthSq(xTarget - m_xIKDragStartTarget) >
		(fANIM_IK_DRAG_DEAD_ZONE * fANIM_IK_DRAG_DEAD_ZONE);
	if (!bNonTrivial && !m_bIKDragMoved)
	{
		return true;
	}
	m_bIKDragMoved = true;
	m_xIKTarget = xTarget;

	Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	const Zenith_SkeletonAsset* pxSkeleton =
		(pxInstance != nullptr) ? pxInstance->GetSourceSkeleton() : nullptr;
	if (pxInstance == nullptr || pxSkeleton == nullptr)
	{
		return true;
	}

	// ---- restore the latch, then solve from it ------------------------------
	ApplyIKChainRotations(m_axIKLatchedLocalRotations);

	Zenith_AnimationPoseIK::SolveRequest xRequest;
	xRequest.m_auChainBoneIndices = m_auIKDragChainBones;
	xRequest.m_xTargetModelSpace = m_xIKTarget;

	Zenith_Vector<Zenith_Maths::Quat> axLocalRotations;
	if (!Zenith_AnimationPoseIK::SolveChainToLocalRotations(*pxInstance, *pxSkeleton,
		xRequest, axLocalRotations))
	{
		// ★ A REFUSED SOLVE LEAVES THE LATCHED POSE, which the restore above has
		// already put back — not a half-applied chain, and not the previous
		// frame's solve either. SolveChainToLocalRotations writes nothing on any
		// refusal, so there is no partial output to undo.
		return true;
	}

	ApplyIKChainRotations(axLocalRotations);
	return true;
}

void Zenith_EditorPanel_Animation::ApplyIKChainRotations(
	const Zenith_Vector<Zenith_Maths::Quat>& axLocalRotations)
{
	Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr || axLocalRotations.GetSize() != m_auIKDragChainBones.GetSize() ||
		m_auIKDragChainBones.GetSize() == 0u)
	{
		return;
	}

	const u_int uCount = m_auIKDragChainBones.GetSize();
	for (u_int u = 0; u + 1u < uCount; ++u)
	{
		const u_int uBone = m_auIKDragChainBones.Get(u);
		// Copied out first: the getters return references INTO the arrays the
		// setter writes.
		const Zenith_Maths::Vector3 xPosition = pxInstance->GetBoneLocalPosition(uBone);
		const Zenith_Maths::Vector3 xScale = pxInstance->GetBoneLocalScale(uBone);
		pxInstance->SetBoneLocalTransform(uBone, xPosition, axLocalRotations.Get(u), xScale);
	}

	// ★ THE EFFECTOR GOES THROUGH THE SESSION, and that is what raises the
	// unkeyed-pose badge (§4.4 — the one thing a user can silently lose). It is
	// also the bone the drag bracket is open on, which is the only bone
	// UpdateBoneDrag will write. Its own RefreshDerivedPose covers the ancestors
	// written above, so the chain reaches model space in one recompute.
	m_xSession.UpdateBoneDrag(axLocalRotations.Get(uCount - 1u));
}

bool Zenith_EditorPanel_Animation::Action_EndIKDrag()
{
	if (!m_bIKDragActive)
	{
		return false;
	}

	const bool bMoved = m_bIKDragMoved;
	const Zenith_Maths::Vector3 xTarget = m_xIKTarget;

	m_bIKDragActive = false;
	m_bIKDragMoved = false;
	// The bracket closes BEFORE the bake: Action_SetKeyForBones reads the live
	// pose, which the drag has already written, and leaving evaluation suspended
	// past the gesture would freeze the play head's effect on the rig.
	m_xSession.EndBoneDrag();
	m_auIKDragChainBones.Clear();
	m_axIKLatchedLocalRotations.Clear();

	if (!bMoved)
	{
		// ★ A DRAG THAT NEVER MOVED RECORDS NOTHING — no solve ran, no key, no
		// undo entry, and no badge either unless one was already up before it
		// started. Zenith_Editor::RecordGizmoDragUndo's rule, and the ring drag's.
		if (!m_bIKWasUnkeyedAtDragStart)
		{
			m_xSession.ClearUnkeyedPose();
		}
		return true;
	}

	// ★ ONE BAKE, THROUGH THE VERB — which solves once more from the pose the
	// drag left (already at the target, so the solver converges immediately) and
	// keys through Action_SetKeyForBones, as ONE compound. Writing the keys here
	// instead would be a second key-writing path that a later change to §5.4
	// would silently leave behind.
	//
	// ★ AND IT BAKES WITH AUTO-KEY OFF TOO, unlike the ring drag's release. That
	// is not an oversight: Action_BakeIKForSelectedChain IS "solve and bake down
	// to keys" (design note §6), it keys unconditionally, and there is no
	// key-less IK verb for an auto-key-off release to call instead. Escape is
	// how an IK drag is abandoned without writing anything.
	Action_BakeIKForSelectedChain(xTarget);
	return true;
}

bool Zenith_EditorPanel_Animation::Action_CancelIKDrag()
{
	if (!m_bIKDragActive)
	{
		return false;
	}

	// Back to the rotations the press latched. The session still has the drag
	// open at this point, which is the only state in which UpdateBoneDrag — and
	// therefore ApplyIKChainRotations' last write — does anything.
	ApplyIKChainRotations(m_axIKLatchedLocalRotations);
	m_xSession.EndBoneDrag();

	m_bIKDragActive = false;
	m_bIKDragMoved = false;
	m_auIKDragChainBones.Clear();
	m_axIKLatchedLocalRotations.Clear();
	// The handle goes back under the effector, where the press found it.
	m_xIKTarget = m_xIKDragStartTarget;
	if (!m_bIKWasUnkeyedAtDragStart)
	{
		m_xSession.ClearUnkeyedPose();
	}
	return true;
}

//=============================================================================
// The bake (WU-4.4) — one action body, and the verb every IK path ends in.
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
	// ★ THE FALLBACK BELOW IS FOR A GENUINE REFUSAL, NOT FOR AN UNLANDED UNIT.
	// WU-4.3 has landed and Action_SetKeyForBones is the path every bake takes;
	// what is left for BakeChain to answer is the handful of states that verb
	// legitimately refuses — most usefully an undo COMPOUND already open, which
	// Action_SetKeyForBones will not write into because the key would join
	// somebody else's step. BakeChain writes through the same document verbs in
	// its own compound, so what reaches the clip is identical either way, and
	// keying the choice on the ACTION's return value rather than on a build flag
	// is what keeps a success from ever reaching it.
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
