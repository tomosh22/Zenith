#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Animation/Zenith_BoneSpace.h"
#include "Editor/Zenith_EditorUI.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"   // the pure orbit camera position
#include "Flux/Gizmos/Flux_GizmosImpl.h"                       // SnapValue: static, pure, shared

#include "imgui.h"

#include <cmath>
#include <string>

//=============================================================================
// POSE AUTHORING (WU-4.3) — the manipulator, the drag transaction, Set Key and
// auto-key. Its own TU behind WU-4.1's declarations, for the reason the panel
// is split at all: "what the panel knows", "what the panel paints" and "what
// the panel does to a clip" are the halves a reader wants separately, and a
// fourth unit filling bodies into somebody else's file is a write conflict.
//
// ★ TWO LAYERS, AND ONLY ONE OF THEM IS UNDOABLE (design note §4.1).
//
//   the LIVE POSE  — the session instance's bone-local rotations. Written every
//                    drag frame. Not undoable. Not serialized.
//   the KEYS       — the document's channels. Written ONCE, on release, and
//                    only when a key is actually asked for. Undoable. Serialized.
//
// Nothing is pushed on an undo stack during a drag and nothing is written to
// the document during one. That is Zenith_Editor::RecordGizmoDragUndo's shape
// exactly, down to "a click that never moved records nothing".
//
// ★ EVERY KEY THIS PANEL WRITES GOES THROUGH Action_SetKeyForBones. The toolbar
// button, auto-key on release and (WU-4.4) the IK bake all call that one
// function, so a key written three different ways is byte-identical — and one
// Ctrl+Z undoes whichever gesture produced it, because all three are one
// compound.
//
// ★ ROTATION ONLY, AND THAT IS NOT TIDINESS (§5.1).
// Flux_SkeletonPose::SampleFromClip writes a component ONLY if that channel has
// keyframes — bones with no channel keep the bind pose the controller reseeds
// every frame. So adding the FIRST key to a channel changes that bone's
// behaviour across the ENTIRE clip, from "follows bind pose" to "follows a
// single constant". Writing translation and scale "for consistency" beside a
// rotation edit would therefore alter frames the author never touched, in a way
// that only shows up on playback. Translation is authorable by its own explicit
// verb and only on a ROOT bone; scale is not authorable in Phase 4 at all.
//=============================================================================

namespace
{
	ImVec2 Vec(float fX, float fY) { return ImVec2(fX, fY); }

	constexpr float fPOSE_PI = 3.14159265358979f;
	constexpr float fPOSE_RAD_TO_DEG = 180.0f / fPOSE_PI;
	constexpr float fPOSE_DEG_TO_RAD = fPOSE_PI / 180.0f;

	// Below this the accumulated drag angle counts as "the user pressed and let
	// go", the same judgement RecordGizmoDragUndo makes on a transform delta.
	// 1e-4 rad is 0.006 degrees: far below anything a hand produces and far above
	// the float noise a few dozen accumulated atan2 steps carry.
	constexpr float fPOSE_DRAG_DEAD_ZONE_RADIANS = 1.0e-4f;

	// Bigger than any pixel distance a preview pane can produce, so
	// Zenith_AnimPoseDistanceToRing can answer for an invalid ring without the
	// caller special-casing it.
	constexpr float fPOSE_DISTANCE_INFINITE = 1.0e30f;

	//--------------------------------------------------------------------------
	// The bone's NAME. A track is addressed by (bone name, track) and never by an
	// index (D14/D24): a channel is DELETED when its last key goes and re-created
	// by the undo, so nothing may hold a channel pointer, and the skeleton's
	// index is not the clip's ordering either.
	//--------------------------------------------------------------------------
	bool PoseBoneName(const Zenith_AnimationPreviewSession& xSession, u_int uBone, std::string& strOut)
	{
		const Flux_SkeletonInstance* pxInstance = xSession.GetSkeletonInstance();
		if (pxInstance == nullptr)
		{
			return false;
		}
		const Zenith_SkeletonAsset* pxAsset = pxInstance->GetSourceSkeleton();
		if (pxAsset == nullptr || uBone >= pxAsset->GetNumBones())
		{
			return false;
		}
		strOut = pxAsset->GetBone(uBone).m_strName;
		// An unnamed bone cannot address a channel, and inserting under an empty
		// name would create a track nothing can ever find again.
		return !strOut.empty();
	}

	bool PoseBoneIsRoot(const Zenith_AnimationPreviewSession& xSession, u_int uBone)
	{
		const Flux_SkeletonInstance* pxInstance = xSession.GetSkeletonInstance();
		if (pxInstance == nullptr)
		{
			return false;
		}
		const Zenith_SkeletonAsset* pxAsset = pxInstance->GetSourceSkeleton();
		if (pxAsset == nullptr || uBone >= pxAsset->GetNumBones())
		{
			return false;
		}
		return pxAsset->GetBone(uBone).m_iParentIndex == Zenith_SkeletonAsset::INVALID_BONE_INDEX;
	}

	// Shortest distance from a point to one segment, in pixels.
	float PoseDistanceToSegment(const Zenith_Maths::Vector2& xA, const Zenith_Maths::Vector2& xB,
		float fX, float fY)
	{
		const float fDx = xB.x - xA.x;
		const float fDy = xB.y - xA.y;
		const float fLengthSq = fDx * fDx + fDy * fDy;
		float fT = 0.0f;
		if (fLengthSq > 1.0e-12f)
		{
			fT = ((fX - xA.x) * fDx + (fY - xA.y) * fDy) / fLengthSq;
			if (fT < 0.0f) { fT = 0.0f; }
			if (fT > 1.0f) { fT = 1.0f; }
		}
		const float fPx = xA.x + fDx * fT;
		const float fPy = xA.y + fDy * fT;
		return std::sqrt((fX - fPx) * (fX - fPx) + (fY - fPy) * (fY - fPy));
	}
}

//=============================================================================
// The pure geometry. No ImGui, no panel state, no engine singleton — a unit
// builds a ring set by hand and asserts against it.
//=============================================================================

Zenith_Maths::Vector3 Zenith_AnimPoseRingAxis(u_int uAxis)
{
	switch (uAxis)
	{
		case 1u: return Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		case 2u: return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		default: return Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	}
}

void Zenith_AnimPoseRingBasis(u_int uAxis, Zenith_Maths::Vector3& xOutU, Zenith_Maths::Vector3& xOutV)
{
	// ★ cross(u, v) == axis FOR ALL THREE, which is the whole contract: it makes
	// the ring parameter the right-handed rotation angle about the axis, so
	// walking the projected polyline forward is turning positively. The cyclic
	// choice below (X: Y,Z — Y: Z,X — Z: X,Y) is the one assignment that gets
	// that right for every axis without a per-axis sign flip.
	switch (uAxis)
	{
		case 1u:
			xOutU = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			xOutV = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			break;
		case 2u:
			xOutU = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			xOutV = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			break;
		default:
			xOutU = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			xOutV = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			break;
	}
}

float Zenith_AnimPoseSignedScreenAngle(const Zenith_Maths::Vector2& xPivot,
	const Zenith_Maths::Vector2& xFrom, const Zenith_Maths::Vector2& xTo)
{
	const float fAx = xFrom.x - xPivot.x;
	const float fAy = xFrom.y - xPivot.y;
	const float fBx = xTo.x - xPivot.x;
	const float fBy = xTo.y - xPivot.y;

	// A cursor on the pivot has no direction to measure, and normalising it would
	// produce a NaN that then reaches a bone rotation and a saved .zanim.
	if ((fAx * fAx + fAy * fAy) < 1.0e-8f || (fBx * fBx + fBy * fBy) < 1.0e-8f)
	{
		return 0.0f;
	}

	// atan2(cross, dot) rather than the difference of two atan2s: it is one call,
	// it is already wrapped into (-pi, pi], and it never has to be un-wrapped
	// across the branch cut the two-call form straddles at +/-pi.
	return std::atan2(fAx * fBy - fAy * fBx, fAx * fBx + fAy * fBy);
}

float Zenith_AnimPoseDistanceToRing(const Zenith_AnimPoseRing& xRing, float fX, float fY)
{
	if (!xRing.m_bValid)
	{
		return fPOSE_DISTANCE_INFINITE;
	}
	float fBest = fPOSE_DISTANCE_INFINITE;
	for (u_int u = 0; u < uANIM_POSE_RING_SEGMENTS; ++u)
	{
		const float fDistance = PoseDistanceToSegment(xRing.m_axPoints[u], xRing.m_axPoints[u + 1u], fX, fY);
		if (fDistance < fBest)
		{
			fBest = fDistance;
		}
	}
	return fBest;
}

bool Zenith_AnimPosePickRing(const Zenith_AnimPoseRingSet& xRings, float fX, float fY,
	float fTolerancePixels, u_int& uOutAxis)
{
	float fBest = fTolerancePixels;
	u_int uBest = uINVALID_ANIM_POSE_RING;
	for (u_int uAxis = 0; uAxis < 3u; ++uAxis)
	{
		const float fDistance = Zenith_AnimPoseDistanceToRing(xRings.m_axRings[uAxis], fX, fY);
		// Strictly less, so the FIRST of two exactly-equidistant rings wins and
		// the answer does not depend on iteration order changing.
		if (fDistance < fBest)
		{
			fBest = fDistance;
			uBest = uAxis;
		}
	}
	if (uBest == uINVALID_ANIM_POSE_RING)
	{
		// ★ uOutAxis IS LEFT UNTOUCHED ON A MISS, matching
		// Zenith_RaycastBonePickSet — a caller that ignored the bool would
		// otherwise read a plausible 0 meaning "the X ring".
		return false;
	}
	uOutAxis = uBest;
	return true;
}

//=============================================================================
// The ring set — the one projection the overlay, the hit test and the units all
// read.
//=============================================================================

bool Zenith_EditorPanel_Animation::GetPoseRingSet(Zenith_AnimPoseRingSet& xOut) const
{
	if (!m_xSession.IsOpen() || !m_xSession.HasBoneSelection() || !m_bPreviewImageRectValid)
	{
		return false;
	}
	const Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr)
	{
		return false;
	}

	const u_int uBone = m_xSession.GetSelectedBoneIndex();
	// ★ THE PIVOT IS THE BONE'S OWN JOINT, and that is the joint its rotation
	// turns ABOUT rather than one it moves: with L_i = T(p) * R(q) * S, the
	// translation of M_i is M_parent(i) * p, which does not contain q_i at all.
	// The joints q_i moves are its CHILDREN's. So the ring is centred on the
	// fixed point of the very rotation it authors, which is where a user expects
	// to find the handle.
	const Zenith_Maths::Vector3 xPivot = Zenith_BoneSpace::BoneWorldPosition(
		m_xSession.GetSessionModelMatrix(), *pxInstance, uBone);

	float fYaw = 0.0f;
	float fPitch = 0.0f;
	float fDistance = 0.0f;
	m_xSession.GetCameraOrbit(fYaw, fPitch, fDistance);
	const Zenith_Maths::Vector3 xCamera = Flux_PreviewOrbitCameraPos(fYaw, fPitch, fDistance);

	const float fPivotDistance = Zenith_Maths::Length(xPivot - xCamera);
	if (!(fPivotDistance > 1.0e-4f))
	{
		// The camera is inside the joint: there is no ring radius that means
		// anything, and the projection would be a smear across the pane.
		return false;
	}
	const float fRadius = fPivotDistance * fANIM_POSE_RING_SCREEN_FRACTION;

	float fPivotX = 0.0f;
	float fPivotY = 0.0f;
	if (!ProjectPreviewWorldPoint(xPivot, fPivotX, fPivotY))
	{
		return false;
	}
	xOut.m_xPivotPixel = Zenith_Maths::Vector2(fPivotX, fPivotY);

	for (u_int uAxis = 0; uAxis < 3u; ++uAxis)
	{
		Zenith_AnimPoseRing& xRing = xOut.m_axRings[uAxis];
		xRing.m_bValid = false;

		Zenith_Maths::Vector3 xU(0.0f);
		Zenith_Maths::Vector3 xV(0.0f);
		Zenith_AnimPoseRingBasis(uAxis, xU, xV);

		bool bAllProjected = true;
		for (u_int u = 0; u <= uANIM_POSE_RING_SEGMENTS; ++u)
		{
			// The modulo makes the last point EXACTLY the first rather than a
			// separately-computed 2*pi that differs in the last bit; a closed
			// polyline with a hairline gap is a hit test that can miss.
			const u_int uStep = u % uANIM_POSE_RING_SEGMENTS;
			const float fTheta = (2.0f * fPOSE_PI) * static_cast<float>(uStep)
				/ static_cast<float>(uANIM_POSE_RING_SEGMENTS);
			const Zenith_Maths::Vector3 xPoint = xPivot
				+ (xU * std::cos(fTheta) + xV * std::sin(fTheta)) * fRadius;

			float fPx = 0.0f;
			float fPy = 0.0f;
			if (!ProjectPreviewWorldPoint(xPoint, fPx, fPy))
			{
				// Part of the ring is behind the camera. The whole ring is dropped
				// rather than half-drawn: the missing arc is exactly where a
				// mirrored coordinate would land.
				bAllProjected = false;
				break;
			}
			xRing.m_axPoints[u] = Zenith_Maths::Vector2(fPx, fPy);
		}
		xRing.m_bValid = bAllProjected;
	}
	return true;
}

//=============================================================================
// Set Key (§5) — the ONE key-writing path.
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_SetKeyForBones(
	const Zenith_Vector<u_int>& xBoneIndices, bool bRotation, bool bTranslationForRoot)
{
	if (!m_xDocument.IsOpen() || !m_xSession.IsOpen() || xBoneIndices.GetSize() == 0u)
	{
		return false;
	}
	if (!bRotation && !bTranslationForRoot)
	{
		// Neither track asked for. Refused rather than treated as "write nothing
		// and succeed", which would let a caller believe a pose had been kept.
		return false;
	}
	Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr)
	{
		return false;
	}

	// ★ SNAPPED TO THE FRAME GRID, UNCONDITIONALLY, and through the SAME pure
	// function the dope sheet's drags snap with (§5.2). Unsnapped keys land at
	// arbitrary float times, which makes the sheet's frame columns lie and makes
	// D11's occupancy test — |a - b| <= fANIM_TIME_EPSILON — fire or not on
	// float noise. A clip with no frame rate is left unsnapped; that is
	// Zenith_AnimTimelineSnapToFrame's own documented behaviour for rate 0, not a
	// second rule invented here.
	float fTimeSeconds = Zenith_AnimTimelineSnapToFrame(m_xSession.GetTime(), GetFrameRate());
	if (fTimeSeconds < 0.0f)
	{
		fTimeSeconds = 0.0f;
	}

	if (!m_xDocument.BeginCompound())
	{
		// A group is already open — somebody is mid-operation and a key written
		// into their bracket would join their undo step.
		return false;
	}

	for (u_int u = 0; u < xBoneIndices.GetSize(); ++u)
	{
		const u_int uBone = xBoneIndices.Get(u);
		std::string strBone;
		if (!PoseBoneName(m_xSession, uBone, strBone))
		{
			continue;
		}

		if (bRotation)
		{
			// ★ THE LIVE POSE IS THE SOURCE, not the clip and not the drag's
			// arithmetic: whatever the user is looking at is what gets keyed, so a
			// hand drag, a nudge and an IK solve all key the pose that is on screen.
			// D25 makes an occupied slot a VALUE EDIT that keeps the key's id, so
			// pressing Set Key twice on one frame does not invent a second key.
			const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone(strBone, FLUX_ANIM_TRACK_ROTATION);
			m_xDocument.InsertKey(xTrack, fTimeSeconds, pxInstance->GetBoneLocalRotation(uBone));
		}

		if (bTranslationForRoot && PoseBoneIsRoot(m_xSession, uBone))
		{
			const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone(strBone, FLUX_ANIM_TRACK_POSITION);
			m_xDocument.InsertKey(xTrack, fTimeSeconds, pxInstance->GetBoneLocalPosition(uBone));
		}
	}

	// ONE undo step for the whole gesture, however many bones and tracks it
	// moved.
	//
	// bKeep is TRUE unconditionally, and the return value is the answer: an EMPTY
	// group is deleted and pushes nothing, so a call that resolved no bone leaves
	// no no-op stop to press Ctrl+Z through and reports false here. Passing
	// `false` would mean "UNDO what was collected", which is a rollback this
	// operation has no reason to perform — nothing it does can be refused
	// partway, because D25 makes an occupied slot a value edit rather than a
	// collision (unlike every operation in the _Ops TU).
	if (!m_xDocument.EndCompound("Set Pose Key", true))
	{
		return false;
	}

	NotifyDocumentEdited();
	// The pose IS in the clip now, so the badge has to go — leaving it up would
	// warn about losing something a seek would faithfully reproduce.
	m_xSession.ClearUnkeyedPose();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SetKeyForSelectedBone()
{
	if (!m_xSession.HasBoneSelection())
	{
		return false;
	}
	Zenith_Vector<u_int> axBones;
	axBones.PushBack(m_xSession.GetSelectedBoneIndex());
	return Action_SetKeyForBones(axBones, true, false);
}

bool Zenith_EditorPanel_Animation::Action_SetKeyTranslationForRoot()
{
	if (!m_xSession.HasBoneSelection())
	{
		return false;
	}
	const u_int uBone = m_xSession.GetSelectedBoneIndex();
	if (!PoseBoneIsRoot(m_xSession, uBone))
	{
		// Refused rather than silently downgraded to a rotation-only key: root
		// motion is the only place a translation belongs (§5.1), and a caller who
		// asked for one on a limb has misunderstood something worth saying.
		return false;
	}
	Zenith_Vector<u_int> axBones;
	axBones.PushBack(uBone);
	return Action_SetKeyForBones(axBones, true, true);
}

bool Zenith_EditorPanel_Animation::Action_SetAutoKey(bool bEnabled)
{
	if (m_xSession.GetAutoKey() == bEnabled)
	{
		return false;
	}
	m_xSession.SetAutoKey(bEnabled);
	return true;
}

bool Zenith_EditorPanel_Animation::Action_GetAutoKey() const
{
	return m_xSession.GetAutoKey();
}

bool Zenith_EditorPanel_Animation::Action_SetPoseAngleSnap(bool bEnabled)
{
	if (m_bPoseAngleSnap == bEnabled)
	{
		return false;
	}
	m_bPoseAngleSnap = bEnabled;
	// A snap toggled MID-DRAG re-applies immediately, so the bone jumps to the
	// grid under the cursor rather than waiting for the next mouse move — the
	// same thing the entity gizmo's hold-Ctrl does.
	if (m_bPoseDragActive)
	{
		ApplyPoseDragAngle();
	}
	return true;
}

bool Zenith_EditorPanel_Animation::Action_GetPoseAngleSnap() const
{
	return m_bPoseAngleSnap;
}

//=============================================================================
// The rotate primitive and the drag transaction (§4).
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_RotateSelectedBoneWorld(const Zenith_Maths::Quat& xWorldDelta)
{
	if (!m_xSession.IsOpen() || !m_xSession.HasBoneSelection() || m_bPoseDragActive || m_bIKDragActive)
	{
		// A pointer drag — either manipulator — owns the same bone and the same
		// latched rotation; a one-shot landing in the middle of one would be
		// overwritten by the next mouse move with no trace of having happened.
		// The session's BeginBoneDrag below would refuse anyway, but a refusal
		// naming the reason beats one that reads as "the bone does not exist".
		return false;
	}
	Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr)
	{
		return false;
	}
	const u_int uBone = m_xSession.GetSelectedBoneIndex();
	if (!m_xSession.BeginBoneDrag(uBone))
	{
		return false;
	}

	// ★ THE CONJUGATION IS THE WHOLE POINT (§3.4). q_i is expressed in its
	// PARENT's frame and nothing else in the chain is, so a world delta applied
	// directly is the plausible wrong answer — exactly right at an identity
	// parent, and wrong by a pose-dependent amount everywhere else. For a ROOT
	// bone qP is rotation(W), identity through Phase 4, and this collapses to the
	// entity gizmo's newRotation = delta * initial.
	const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(
		m_xSession.GetSessionModelMatrix(), *pxInstance, uBone);
	const Zenith_Maths::Quat xNewLocal = Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(
		xWorldDelta, xParentWorld, m_xSession.GetDragInitialRotation());

	const bool bApplied = m_xSession.UpdateBoneDrag(xNewLocal);
	m_xSession.EndBoneDrag();
	return bApplied;
}

bool Zenith_EditorPanel_Animation::Action_BeginBoneDragAtPixel(float fPixelX, float fPixelY)
{
	// ★ THE TWO MANIPULATORS EXCLUDE EACH OTHER (the other half of the rule is
	// in Action_BeginIKDragAtPixel). Both latch state at press and both open the
	// SESSION's single drag bracket, so a ring grabbed mid-IK-drag would leave
	// one latch belonging to nobody and one mouse-up ending the wrong
	// transaction.
	if (m_bPoseDragActive || m_bIKDragActive || !m_xSession.HasBoneSelection())
	{
		return false;
	}

	Zenith_AnimPoseRingSet xRings;
	if (!GetPoseRingSet(xRings))
	{
		return false;
	}

	u_int uAxis = uINVALID_ANIM_POSE_RING;
	if (!Zenith_AnimPosePickRing(xRings, fPixelX, fPixelY, fANIM_POSE_RING_GRAB_PIXELS, uAxis))
	{
		return false;
	}

	if (!m_xSession.BeginBoneDrag(m_xSession.GetSelectedBoneIndex()))
	{
		return false;
	}

	m_uPoseDragAxis = uAxis;
	m_xPoseDragPivotPixel = xRings.m_xPivotPixel;
	m_xPoseDragLastPixel = Zenith_Maths::Vector2(fPixelX, fPixelY);
	m_fPoseDragAngleRadians = 0.0f;
	m_bPoseDragMoved = false;
	m_bPoseWasUnkeyedAtDragStart = m_xSession.HasUnkeyedPose();

	// ★ THE SCREEN SIGN, MEASURED RATHER THAN DERIVED. Points 0 and 1 of the ring
	// are one positive step in the ring parameter, which IS one positive step of
	// right-handed rotation about the axis; whichever way round that step appears
	// on screen is therefore the direction a positive drag has to go. A ring seen
	// from behind reverses on screen and this reverses with it, with no reasoning
	// about the projection's handedness or the Vulkan Y flip.
	const float fProbe = Zenith_AnimPoseSignedScreenAngle(xRings.m_xPivotPixel,
		xRings.m_axRings[uAxis].m_axPoints[0], xRings.m_axRings[uAxis].m_axPoints[1]);
	m_fPoseDragScreenSign = (fProbe >= 0.0f) ? 1.0f : -1.0f;

	m_bPoseDragActive = true;
	return true;
}

bool Zenith_EditorPanel_Animation::Action_UpdateBoneDragToPixel(float fPixelX, float fPixelY)
{
	if (!m_bPoseDragActive)
	{
		return false;
	}

	const Zenith_Maths::Vector2 xNow(fPixelX, fPixelY);
	// ★ ACCUMULATED FROM PER-FRAME STEPS, not measured from the press pixel. The
	// signed angle is wrapped into (-pi, pi], so a drag past half a turn measured
	// from the press would fold back and the bone would spin the wrong way; a
	// step between two consecutive mouse positions is never near the branch cut.
	const float fStep = Zenith_AnimPoseSignedScreenAngle(m_xPoseDragPivotPixel, m_xPoseDragLastPixel, xNow);
	m_xPoseDragLastPixel = xNow;
	m_fPoseDragAngleRadians += fStep * m_fPoseDragScreenSign;

	ApplyPoseDragAngle();
	return true;
}

void Zenith_EditorPanel_Animation::ApplyPoseDragAngle()
{
	if (!m_bPoseDragActive)
	{
		return;
	}
	const Flux_SkeletonInstance* pxInstance = m_xSession.GetSkeletonInstance();
	if (pxInstance == nullptr)
	{
		return;
	}

	float fAngleRadians = m_fPoseDragAngleRadians;
	if (m_bPoseAngleSnap)
	{
		// Flux_GizmosImpl::SnapValue is static and pure, and is the SAME rounding
		// the entity gizmo's rotate drag uses — a second copy here is how a bone
		// and an entity end up disagreeing about what "15 degrees" means.
		fAngleRadians = Flux_GizmosImpl::SnapValue(fAngleRadians * fPOSE_RAD_TO_DEG, fANIM_POSE_SNAP_DEGREES)
			* fPOSE_DEG_TO_RAD;
	}

	const bool bNonTrivial = std::fabs(fAngleRadians) > fPOSE_DRAG_DEAD_ZONE_RADIANS;
	if (!bNonTrivial && !m_bPoseDragMoved)
	{
		// ★ NOTHING IS WRITTEN YET. Writing the identity rotation here would be a
		// no-op to look at and would still raise the session's unkeyed-pose flag,
		// so a press-and-release would leave the pane warning about a pose nobody
		// changed.
		return;
	}
	m_bPoseDragMoved = bNonTrivial;

	const Zenith_Maths::Quat xWorldDelta = Zenith_Maths::AngleAxis(
		fAngleRadians, Zenith_AnimPoseRingAxis(m_uPoseDragAxis));
	const u_int uBone = m_xSession.GetDragBoneIndex();
	const Zenith_Maths::Quat xParentWorld = Zenith_BoneSpace::ParentWorldRotation(
		m_xSession.GetSessionModelMatrix(), *pxInstance, uBone);
	m_xSession.UpdateBoneDrag(Zenith_BoneSpace::ApplyWorldDeltaToBoneLocal(
		xWorldDelta, xParentWorld, m_xSession.GetDragInitialRotation()));
}

bool Zenith_EditorPanel_Animation::Action_EndBoneDrag()
{
	if (!m_bPoseDragActive)
	{
		return false;
	}

	// EndBoneDrag CLEARS the session's drag bone index, so it is read first.
	const u_int uBone = m_xSession.GetDragBoneIndex();
	const bool bMoved = m_bPoseDragMoved;

	m_bPoseDragActive = false;
	m_bPoseDragMoved = false;
	m_fPoseDragAngleRadians = 0.0f;
	m_xSession.EndBoneDrag();

	if (!bMoved)
	{
		// ★ A DRAG THAT NEVER MOVED RECORDS NOTHING — no key, no undo entry, and
		// no badge either unless one was already up before it started.
		if (!m_bPoseWasUnkeyedAtDragStart)
		{
			m_xSession.ClearUnkeyedPose();
		}
		return true;
	}

	if (m_xSession.GetAutoKey())
	{
		// ★ ONE KEY, AT THE PLAYHEAD, through the same verb the button uses.
		// Maya-style bracketing (an extra key at the previous keyed time to hold
		// the earlier pose) was rejected: it silently doubles the authored data,
		// and with §5.1's first-key rule a bracketing key on an empty channel
		// changes the whole clip twice over.
		Zenith_Vector<u_int> axBones;
		axBones.PushBack(uBone);
		Action_SetKeyForBones(axBones, true, false);
	}
	return true;
}

bool Zenith_EditorPanel_Animation::Action_CancelBoneDrag()
{
	if (!m_bPoseDragActive)
	{
		return false;
	}

	// Back to the rotation BeginBoneDrag latched. The session still has the drag
	// open at this point, which is the only state in which UpdateBoneDrag writes.
	m_xSession.UpdateBoneDrag(m_xSession.GetDragInitialRotation());
	m_xSession.EndBoneDrag();

	m_bPoseDragActive = false;
	m_bPoseDragMoved = false;
	m_fPoseDragAngleRadians = 0.0f;
	if (!m_bPoseWasUnkeyedAtDragStart)
	{
		m_xSession.ClearUnkeyedPose();
	}
	return true;
}

//=============================================================================
// ★ THE SHARED GESTURE CANCEL (E1) — the ONE place a live manipulator gesture
// is abandoned without a mouse-up, for BOTH manipulators.
//
// ★ WHAT THIS FIXES IS A FLAG THAT LEAKED, and the symptom was a manipulator
// that silently stopped working. Escape (HandlePoseManipulatorInput below) was
// the only cancel that existed. Six other things end a gesture and not one of
// them said so:
//
//   the panel being HIDDEN  — Render's !m_bShow return cleared seven sheet
//                             flags and neither drag flag
//   CloseClip               — the clip the keys would be written to is gone
//   OnDocumentOpened        — a different clip, and a re-Open'd session
//   a rig RE-RESOLVE        — the skeleton instance the latch belongs to has
//                             been deleted and rebuilt
//   Action_SelectBone       — the gesture belongs to the bone that WAS selected
//   Action_ClearBoneSelection
//
// After any of them m_bPoseDragActive stayed true forever, so
// Action_BeginBoneDragAtPixel refused every later grab — with the pose, the
// clip and the undo stack all healthy and nothing anywhere to say why. And the
// SESSION's drag bracket stayed open with it, which suspends Tick.
//
// It is a CANCEL and never an END: what these six have in common is that the
// user did not release the button, so committing a key on their behalf would
// write the pose they were still deciding about.
//=============================================================================

bool Zenith_EditorPanel_Animation::CancelAllPoseGestures()
{
	// Both are asked unconditionally and each answers false when it was not in
	// flight, so this is idempotent and the OR is the honest "did anything
	// happen". They cannot both be live — each Begin refuses while the other is
	// — but asking both is what makes that an invariant rather than something
	// this function has to know.
	const bool bCancelledRing = Action_CancelBoneDrag();
	const bool bCancelledIK = Action_CancelIKDrag();
	return bCancelledRing || bCancelledIK;
}

//=============================================================================
// Input translation and the overlay. The only functions here that read ImGui
// state; each one ends in an Action_*.
//=============================================================================

bool Zenith_EditorPanel_Animation::HandlePoseManipulatorInput(bool bImageHovered)
{
	if (!m_bPreviewImageRectValid)
	{
		m_uPoseHoverAxis = uINVALID_ANIM_POSE_RING;
		return false;
	}

	const ImGuiIO& xIO = ImGui::GetIO();
	const float fLocalX = xIO.MousePos.x - m_xPreviewImageRect.m_fMinX;
	const float fLocalY = xIO.MousePos.y - m_xPreviewImageRect.m_fMinY;

	if (m_bPoseDragActive)
	{
		// ★ ESCAPE IS TESTED BEFORE THE RELEASE, so a cancel is not immediately
		// followed by the mouse-up's commit — the user would get the key they had
		// just asked not to have.
		if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
		{
			Action_CancelBoneDrag();
			return true;
		}
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			Action_UpdateBoneDragToPixel(fLocalX, fLocalY);
		}
		else
		{
			Action_EndBoneDrag();
		}
		// A live drag owns the pointer whether or not it is still over the image:
		// a cursor that wandered off the pane mid-gesture must keep turning the
		// bone, not start orbiting the camera.
		return true;
	}

	if (!bImageHovered)
	{
		m_uPoseHoverAxis = uINVALID_ANIM_POSE_RING;
		return false;
	}

	Zenith_AnimPoseRingSet xRings;
	u_int uAxis = uINVALID_ANIM_POSE_RING;
	if (!GetPoseRingSet(xRings) ||
		!Zenith_AnimPosePickRing(xRings, fLocalX, fLocalY, fANIM_POSE_RING_GRAB_PIXELS, uAxis))
	{
		m_uPoseHoverAxis = uINVALID_ANIM_POSE_RING;
		return false;
	}
	m_uPoseHoverAxis = uAxis;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		// ★ THE MANIPULATOR CLAIMS THE PRESS AHEAD OF THE PICK AND THE ORBIT.
		// Without this, grabbing a ring would also re-select whatever bone the ray
		// happened to pass through and start an orbit — the classic "the gizmo
		// moves the camera" bug.
		return Action_BeginBoneDragAtPixel(fLocalX, fLocalY);
	}
	// Merely hovering claims nothing: the wheel still zooms and the pane still
	// lights the bone under the cursor.
	return false;
}

void Zenith_EditorPanel_Animation::DrawPoseManipulator(ImDrawList* pxDraw)
{
	if (pxDraw == nullptr || !m_bPreviewImageRectValid)
	{
		return;
	}

	Zenith_AnimPoseRingSet xRings;
	if (!GetPoseRingSet(xRings))
	{
		return;
	}

	// Axis colours matching the entity gizmo's X/Y/Z, so "the red ring" means the
	// same thing in both places.
	const ImU32 auAXIS_COLOUR[3] =
	{
		IM_COL32(220, 80, 80, 220),
		IM_COL32(90, 210, 90, 220),
		IM_COL32(90, 140, 235, 220),
	};
	const ImU32 uLIT_COLOUR = IM_COL32(255, 225, 120, 255);

	const float fOriginX = m_xPreviewImageRect.m_fMinX;
	const float fOriginY = m_xPreviewImageRect.m_fMinY;

	pxDraw->PushClipRect(Vec(m_xPreviewImageRect.m_fMinX, m_xPreviewImageRect.m_fMinY),
		Vec(m_xPreviewImageRect.m_fMaxX, m_xPreviewImageRect.m_fMaxY), true);

	for (u_int uAxis = 0; uAxis < 3u; ++uAxis)
	{
		const Zenith_AnimPoseRing& xRing = xRings.m_axRings[uAxis];
		if (!xRing.m_bValid)
		{
			continue;
		}
		const bool bLit = m_bPoseDragActive ? (m_uPoseDragAxis == uAxis) : (m_uPoseHoverAxis == uAxis);
		const ImU32 uColour = bLit ? uLIT_COLOUR : auAXIS_COLOUR[uAxis];
		const float fThickness = Zenith_EditorUI::Px(bLit ? 2.5f : 1.5f);

		// AddLine per segment rather than AddPolyline: the ring is already a
		// closed point list in the space the hit test measures, and drawing it
		// segment by segment keeps the two reading the same array.
		for (u_int u = 0; u < uANIM_POSE_RING_SEGMENTS; ++u)
		{
			pxDraw->AddLine(
				Vec(fOriginX + xRing.m_axPoints[u].x, fOriginY + xRing.m_axPoints[u].y),
				Vec(fOriginX + xRing.m_axPoints[u + 1u].x, fOriginY + xRing.m_axPoints[u + 1u].y),
				uColour, fThickness);
		}
	}

	pxDraw->AddCircleFilled(Vec(fOriginX + xRings.m_xPivotPixel.x, fOriginY + xRings.m_xPivotPixel.y),
		Zenith_EditorUI::Px(3.0f), Zenith_EditorUI::Palette().m_uTextBright);

	pxDraw->PopClipRect();
}

//=============================================================================
// The pose toolbar line.
//=============================================================================

void Zenith_EditorPanel_Animation::RenderPoseToolbar()
{
	// ★ NOTHING IS DRAWN — NOT EVEN A DISABLED STRIP — UNTIL A BONE IS SELECTED,
	// AND THAT IS A LAYOUT CONTRACT RATHER THAN A TIDINESS PREFERENCE.
	//
	// The sheet is sized from whatever vertical space the toolbars, the banners,
	// the preview pane and the event inspector leave behind, and the EVENTS row is
	// the LAST row in it. So a toolbar line that is always present costs one row
	// off the bottom of the sheet on every clip, and the row it costs is the one
	// nothing scrolls past — which is exactly what happened: this line, drawn
	// unconditionally, pushed the events row below m_fCanvasBottom and
	// GetEventRect started (correctly) refusing to publish it. The event
	// inspector strip one function up documents the same rule for the same reason
	// — "nothing selected draws NOTHING, not a disabled strip".
	//
	// Gating on the SELECTION rather than on the rig is what makes the default
	// layout byte-for-byte what it was: a clip with no rig can never have a bone
	// selected, and neither can one nobody has clicked a bone in.
	if (!m_xDocument.IsOpen() || !m_xSession.IsOpen() || !m_xSession.HasBoneSelection())
	{
		return;
	}

	if (ImGui::Button("Set Key"))
	{
		Action_SetKeyForSelectedBone();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("K - write the selected bone's CURRENT local rotation as a key at the play head, snapped to the frame grid.\n"
			"ROTATION ONLY: adding the first key to a channel changes that bone across the WHOLE clip, so a track you did not author is never written.");
	}

	ImGui::SameLine();
	bool bAutoKey = Action_GetAutoKey();
	if (ImGui::Checkbox("Auto-key", &bAutoKey))
	{
		Action_SetAutoKey(bAutoKey);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("On: releasing a bone drag writes the key for you, as ONE undo step.\n"
			"Off: the drag leaves a live pose that is NOT in the clip - the pane says so, and the next seek discards it.");
	}

	ImGui::SameLine();
	bool bSnap = Action_GetPoseAngleSnap();
	if (ImGui::Checkbox("Angle snap", &bSnap))
	{
		Action_SetPoseAngleSnap(bSnap);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Round a ring drag to 15 degree steps (Flux_GizmosImpl::SnapValue, the same rounding the entity gizmo uses).");
	}

	ImGui::SameLine();
	if (m_bPoseDragActive)
	{
		ImGui::TextDisabled("| dragging ring %s: %.1f deg",
			(m_uPoseDragAxis == 0u) ? "X" : ((m_uPoseDragAxis == 1u) ? "Y" : "Z"),
			static_cast<double>(m_fPoseDragAngleRadians * fPOSE_RAD_TO_DEG));
	}
	else if (m_bIKDragActive)
	{
		// The chain LENGTH rather than the target's coordinates: "which bones is
		// this moving" is the question a poser has mid-drag, and three model-space
		// floats scrolling past are unreadable at any rate a hand produces.
		ImGui::TextDisabled("| dragging IK target: %u bone chain, releases as a key",
			m_auIKDragChainBones.GetSize());
	}
	else
	{
		ImGui::TextDisabled("| bone %u - drag a ring to turn it or the centre handle to reach with it, Esc cancels",
			m_xSession.GetSelectedBoneIndex());
	}
}

#endif // ZENITH_TOOLS
