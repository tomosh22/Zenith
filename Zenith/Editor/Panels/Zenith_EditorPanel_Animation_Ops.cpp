#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"

#include <algorithm>
#include <cmath>
#include <string>

//=============================================================================
// The OPERATIONS half of the dope sheet (WU-3.3): selection, the mutating
// actions, the clipboard, and the collision rule they all share.
//
// ★ NOT ONE LINE OF ImGui IN THIS FILE, and that is the reason it is its own
// TU rather than more of the render one. Everything here is callable with no
// frame open, no window, no mouse and no modifiers — which is what lets a unit
// perform an operation directly instead of synthesising a click and then
// guessing which of the four layers swallowed it. The handlers that DO read
// ImGui live in Zenith_EditorPanel_Animation_Render.cpp and each one ends in a
// call to something below.
//
// ★ NOTHING HERE TOUCHES Flux_AnimationClip, AND NOTHING ADDRESSES A KEY BY
// INDEX (D24). Every mutation goes through a Zenith_AnimationDocument verb,
// because each verb has three obligations beyond the edit — re-map the stable
// ids, mark dirty, push undo — and a caller that reached past it would skip all
// three with the first symptom arriving much later as a selection resolving to
// the wrong key.
//
// ★ ONE COLLISION RULE FOR ALL FOUR MULTI-KEY OPERATIONS. Move, ripple,
// duplicate and paste each pre-validate EVERY target time before mutating
// anything, and refuse the whole operation if any one of them is occupied
// (D11 — no silent merge). Half-applying and then stopping would be the worst
// of the three options: the user loses track of what landed, and the undo stack
// records a state nobody asked for.
//=============================================================================

namespace
{
	bool AnimOpsIsFinite(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	bool AnimOpsRectsOverlap(const Zenith_AnimPanelRect& xRect, float fMinX, float fMinY, float fMaxX, float fMaxY)
	{
		return !(xRect.m_fMaxX < fMinX || xRect.m_fMinX > fMaxX
		      || xRect.m_fMaxY < fMinY || xRect.m_fMinY > fMaxY);
	}
}

//=============================================================================
// Selection
//=============================================================================

bool Zenith_EditorPanel_Animation::IsSameSelectedKey(const Zenith_AnimSelectedKey& xA,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId)
{
	return xA.m_uKeyId == uKeyId && xA.m_xTrack == xTrack;
}

u_int Zenith_EditorPanel_Animation::FindSelectedKeyIndex(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const
{
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		if (IsSameSelectedKey(m_axSelectedKeys.Get(u), xTrack, uKeyId))
		{
			return u;
		}
	}
	return uINVALID_ANIM_SHEET_ROW;
}

u_int Zenith_EditorPanel_Animation::FindSelectedEventIndex(u_int uEventId) const
{
	for (u_int u = 0; u < m_auSelectedEventIds.GetSize(); ++u)
	{
		if (m_auSelectedEventIds.Get(u) == uEventId)
		{
			return u;
		}
	}
	return uINVALID_ANIM_SHEET_ROW;
}

bool Zenith_EditorPanel_Animation::IsKeySelected(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const
{
	return FindSelectedKeyIndex(xTrack, uKeyId) != uINVALID_ANIM_SHEET_ROW;
}

bool Zenith_EditorPanel_Animation::IsEventSelected(u_int uEventId) const
{
	return FindSelectedEventIndex(uEventId) != uINVALID_ANIM_SHEET_ROW;
}

bool Zenith_EditorPanel_Animation::GetSelectedKeyAt(u_int uIndex, Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const
{
	if (uIndex >= m_axSelectedKeys.GetSize())
	{
		return false;
	}
	const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(uIndex);
	xOutTrack = xEntry.m_xTrack;
	uOutKeyId = xEntry.m_uKeyId;
	return true;
}

u_int Zenith_EditorPanel_Animation::GetSelectedEventIdAt(u_int uIndex) const
{
	return uIndex < m_auSelectedEventIds.GetSize() ? m_auSelectedEventIds.Get(uIndex) : uINVALID_ANIM_KEY_ID;
}

void Zenith_EditorPanel_Animation::ApplyKeySelectMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	Zenith_AnimSelectMode eMode)
{
	if (eMode == ZENITH_ANIMSELECT_REPLACE)
	{
		// A plain click drops the EVENT selection too. The two lists are one
		// selection to the user, and leaving events behind after a key click is how
		// a Delete removes something nobody could see was still picked.
		m_axSelectedKeys.Clear();
		m_auSelectedEventIds.Clear();
		m_uPrimaryEventId = uINVALID_ANIM_KEY_ID;
	}

	const u_int uExisting = FindSelectedKeyIndex(xTrack, uKeyId);
	if (uExisting != uINVALID_ANIM_SHEET_ROW)
	{
		if (eMode == ZENITH_ANIMSELECT_TOGGLE)
		{
			m_axSelectedKeys.Remove(uExisting);
			if (m_uPrimaryKeyId == uKeyId && m_xPrimaryKeyTrack == xTrack)
			{
				m_uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
			}
		}
		return;
	}

	Zenith_AnimSelectedKey xEntry;
	xEntry.m_xTrack = xTrack;
	xEntry.m_uKeyId = uKeyId;
	m_axSelectedKeys.PushBack(xEntry);

	// ★ THE MOST RECENTLY SELECTED KEY IS THE PRIMARY, which is what a snap is
	// computed against. Anything else makes a multi-key drag snap around a key
	// the user did not touch, and the whole block lands one frame away from
	// where the cursor is.
	m_xPrimaryKeyTrack = xTrack;
	m_uPrimaryKeyId = uKeyId;
}

void Zenith_EditorPanel_Animation::ApplyEventSelectMode(u_int uEventId, Zenith_AnimSelectMode eMode)
{
	if (eMode == ZENITH_ANIMSELECT_REPLACE)
	{
		m_axSelectedKeys.Clear();
		m_auSelectedEventIds.Clear();
		m_uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
		m_uPrimaryEventId = uINVALID_ANIM_KEY_ID;
	}

	const u_int uExisting = FindSelectedEventIndex(uEventId);
	if (uExisting != uINVALID_ANIM_SHEET_ROW)
	{
		if (eMode == ZENITH_ANIMSELECT_TOGGLE)
		{
			m_auSelectedEventIds.Remove(uExisting);
			if (m_uPrimaryEventId == uEventId)
			{
				m_uPrimaryEventId = uINVALID_ANIM_KEY_ID;
			}
			return;
		}
		// Re-selecting one that is already in makes it the PRIMARY — the snap of
		// the drag about to start has to be computed against the event under the
		// cursor, not against whichever one happened to be picked first.
		m_uPrimaryEventId = uEventId;
		return;
	}
	m_auSelectedEventIds.PushBack(uEventId);
	m_uPrimaryEventId = uEventId;
}

bool Zenith_EditorPanel_Animation::ResolvePrimarySelectedEvent(u_int& uOutEventId) const
{
	Flux_AnimationEvent xEvent;
	if (m_uPrimaryEventId != uINVALID_ANIM_KEY_ID
	 && FindSelectedEventIndex(m_uPrimaryEventId) != uINVALID_ANIM_SHEET_ROW
	 && m_xDocument.GetEvent(m_uPrimaryEventId, xEvent))
	{
		uOutEventId = m_uPrimaryEventId;
		return true;
	}
	// ★ THE FALLBACK SKIPS IDS THAT NO LONGER RESOLVE. A delete leaves its ids in
	// the selection on purpose (the undo brings them back), so the first entry is
	// routinely a stale one — and an inspector strip pointed at a stale id would
	// silently edit nothing while looking perfectly alive.
	for (u_int u = 0; u < m_auSelectedEventIds.GetSize(); ++u)
	{
		if (m_xDocument.GetEvent(m_auSelectedEventIds.Get(u), xEvent))
		{
			uOutEventId = m_auSelectedEventIds.Get(u);
			return true;
		}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::ResolvePrimarySelectedKey(Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const
{
	if (m_uPrimaryKeyId != uINVALID_ANIM_KEY_ID
	 && FindSelectedKeyIndex(m_xPrimaryKeyTrack, m_uPrimaryKeyId) != uINVALID_ANIM_SHEET_ROW)
	{
		xOutTrack = m_xPrimaryKeyTrack;
		uOutKeyId = m_uPrimaryKeyId;
		return true;
	}
	if (m_axSelectedKeys.GetSize() == 0)
	{
		return false;
	}
	const Zenith_AnimSelectedKey& xFirst = m_axSelectedKeys.Get(0);
	xOutTrack = xFirst.m_xTrack;
	uOutKeyId = xFirst.m_uKeyId;
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SelectKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	Zenith_AnimSelectMode eMode)
{
	if (!m_xDocument.IsOpen() || uKeyId == uINVALID_ANIM_KEY_ID)
	{
		return false;
	}
	// ★ A KEY THAT DOES NOT RESOLVE IS REFUSED. Stable ids let a selection
	// survive an edit and an undo, but only for ids the document actually issued;
	// an invented one would sit in the list forever, resolving to nothing and
	// silently reducing every operation's effective selection by one.
	if (m_xDocument.GetKeyIndexForId(xTrack, uKeyId) == uINVALID_ANIM_KEY_INDEX)
	{
		return false;
	}
	ApplyKeySelectMode(xTrack, uKeyId, eMode);
	if (!xTrack.m_bRootMotion)
	{
		m_strPasteTargetBone = xTrack.m_strBoneName;
	}
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SelectEvent(u_int uEventId, Zenith_AnimSelectMode eMode)
{
	Flux_AnimationEvent xEvent;
	if (!m_xDocument.IsOpen() || !m_xDocument.GetEvent(uEventId, xEvent))
	{
		return false;
	}
	ApplyEventSelectMode(uEventId, eMode);
	return true;
}

bool Zenith_EditorPanel_Animation::Action_ClearSelection()
{
	const bool bHadSomething = m_axSelectedKeys.GetSize() > 0 || m_auSelectedEventIds.GetSize() > 0;
	m_axSelectedKeys.Clear();
	m_auSelectedEventIds.Clear();
	m_uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
	m_uPrimaryEventId = uINVALID_ANIM_KEY_ID;
	return bHadSomething;
}

bool Zenith_EditorPanel_Animation::Action_BoxSelect(float fX0, float fY0, float fX1, float fY1,
	Zenith_AnimSelectMode eMode)
{
	if (!m_xDocument.IsOpen())
	{
		return false;
	}
	if (!AnimOpsIsFinite(fX0) || !AnimOpsIsFinite(fY0) || !AnimOpsIsFinite(fX1) || !AnimOpsIsFinite(fY1))
	{
		return false;
	}

	const float fMinX = fX0 < fX1 ? fX0 : fX1;
	const float fMaxX = fX0 < fX1 ? fX1 : fX0;
	const float fMinY = fY0 < fY1 ? fY0 : fY1;
	const float fMaxY = fY0 < fY1 ? fY1 : fY0;

	// REPLACE is honoured ONCE, up front — applying it per hit would leave the
	// band selecting only whichever key happened to be visited last.
	Zenith_AnimSelectMode eHitMode = eMode;
	if (eMode == ZENITH_ANIMSELECT_REPLACE)
	{
		Action_ClearSelection();
		eHitMode = ZENITH_ANIMSELECT_ADD;
	}

	// ★ HIT-TESTED AGAINST THE RECTS THE PANEL RECORDED, through the same
	// accessors a click uses — so the band cannot select a key the off-screen
	// gate would refuse to hand out a coordinate for. A rubber band that picked
	// up scrolled-away keys is the vertical twin of the graph editor's virtual
	// palette coordinate, and just as invisible: the count would be right and the
	// keys would be somewhere the user is not looking.
	bool bAnyHit = false;
	for (u_int uRow = 0; uRow < m_axRows.GetSize(); ++uRow)
	{
		const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRow);

		if (xRow.m_eKind == ZENITH_ANIMSHEET_ROW_EVENTS)
		{
			const u_int uEventCount = m_xDocument.GetEventCount();
			for (u_int u = 0; u < uEventCount; ++u)
			{
				const u_int uEventId = m_xDocument.GetEventIdAtIndex(u);
				Zenith_AnimPanelRect xRect;
				if (!GetEventRect(uEventId, xRect) || !AnimOpsRectsOverlap(xRect, fMinX, fMinY, fMaxX, fMaxY))
				{
					continue;
				}
				ApplyEventSelectMode(uEventId, eHitMode);
				bAnyHit = true;
			}
			continue;
		}

		if (!xRow.m_bHasTrack)
		{
			continue;
		}
		const u_int uKeyCount = m_xDocument.GetKeyCount(xRow.m_xTrack);
		for (u_int u = 0; u < uKeyCount; ++u)
		{
			const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xRow.m_xTrack, u);
			Zenith_AnimPanelRect xRect;
			if (!GetKeyRect(xRow.m_xTrack, uKeyId, xRect) || !AnimOpsRectsOverlap(xRect, fMinX, fMinY, fMaxX, fMaxY))
			{
				continue;
			}
			ApplyKeySelectMode(xRow.m_xTrack, uKeyId, eHitMode);
			if (!xRow.m_xTrack.m_bRootMotion)
			{
				m_strPasteTargetBone = xRow.m_xTrack.m_strBoneName;
			}
			bAnyHit = true;
		}
	}
	return bAnyHit;
}

//=============================================================================
// The shared collision rule
//=============================================================================

void Zenith_EditorPanel_Animation::RaiseCollisionFlash(const Zenith_AnimTrackId& xTrack, u_int uKeyId)
{
	// ★ A REFUSAL HAS TO BE VISIBLE ON THE SHEET. The action returns false, but
	// nothing in a UI reads a return value: without this the user drops a key on
	// an occupied frame, sees it spring back, and has no way to tell a refusal
	// from a bug. The counter is what DrawKeysForRow paints red.
	m_uCollisionFlashFrames = uANIM_COLLISION_FLASH_FRAMES;
	m_xCollisionFlashTrack = xTrack;
	m_uCollisionFlashKeyId = uKeyId;
}

bool Zenith_EditorPanel_Animation::GetCollisionFlashKey(Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const
{
	if (m_uCollisionFlashFrames == 0 || m_uCollisionFlashKeyId == uINVALID_ANIM_KEY_ID)
	{
		return false;
	}
	xOutTrack = m_xCollisionFlashTrack;
	uOutKeyId = m_uCollisionFlashKeyId;
	return true;
}

bool Zenith_EditorPanel_Animation::WouldCollide(const Zenith_AnimTrackId& xTrack, float fTargetTime,
	const Zenith_Vector<Zenith_AnimSelectedKey>& axMoving)
{
	// The document's own occupancy test, at the document's own epsilon — asking
	// it rather than comparing times here is what keeps "is this slot free" from
	// acquiring a second opinion that disagrees by an ULP.
	const u_int uBlocker = m_xDocument.FindKeyIdAtTime(xTrack, fTargetTime);
	if (uBlocker == uINVALID_ANIM_KEY_ID)
	{
		return false;
	}
	// ★ A KEY THAT IS ITSELF MOVING IS NOT A BLOCKER. Every key in one move
	// shifts by the SAME delta, so a selected key can only ever be found sitting
	// in a slot it is about to vacate; treating that as a collision would make
	// every drag of two adjacent keys impossible.
	for (u_int u = 0; u < axMoving.GetSize(); ++u)
	{
		if (IsSameSelectedKey(axMoving.Get(u), xTrack, uBlocker))
		{
			return false;
		}
	}
	RaiseCollisionFlash(xTrack, uBlocker);
	return true;
}

//=============================================================================
// Move (shared by the drag and by the ripple)
//=============================================================================

bool Zenith_EditorPanel_Animation::MoveKeySetByDelta(const Zenith_Vector<Zenith_AnimSelectedKey>& axKeys,
	float fDeltaSeconds, const char* szDescription)
{
	if (!m_xDocument.IsOpen() || axKeys.GetSize() == 0)
	{
		return false;
	}
	if (!AnimOpsIsFinite(fDeltaSeconds))
	{
		return false;
	}
	// A move of less than the clip's own epsilon is not a move: applying it would
	// push an undo entry whose Ctrl+Z visibly does nothing.
	if (std::fabs(fDeltaSeconds) <= fANIM_TIME_EPSILON)
	{
		return false;
	}

	// ---- resolve every current time BEFORE anything moves -------------------
	Zenith_Vector<float> afCurrent;
	afCurrent.Reserve(axKeys.GetSize());
	for (u_int u = 0; u < axKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = axKeys.Get(u);
		float fTime = 0.0f;
		if (!m_xDocument.GetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, fTime))
		{
			// A stale id in the set. Refusing beats moving the rest: a partial move
			// silently changes the spacing the user was preserving.
			return false;
		}
		afCurrent.PushBack(fTime);
	}

	// ---- pre-validate every target ------------------------------------------
	for (u_int u = 0; u < axKeys.GetSize(); ++u)
	{
		const float fTarget = afCurrent.Get(u) + fDeltaSeconds;
		if (fTarget < -fANIM_TIME_EPSILON)
		{
			// A clip starts at 0. Clamping instead would pile the leading keys onto
			// one another, which is the merge D11 exists to forbid.
			return false;
		}
		if (WouldCollide(axKeys.Get(u).m_xTrack, fTarget, axKeys))
		{
			return false;
		}
	}

	// ---- order the application ----------------------------------------------
	// ★ DESCENDING TIME FOR A FORWARD MOVE, ASCENDING FOR A BACKWARD ONE. Each
	// SetKeyTime is validated against the track AS IT IS AT THAT MOMENT, so
	// moving the earlier of two neighbours first lands it on a slot the later one
	// has not vacated yet and the mutator refuses it. Sorting by CURRENT time
	// makes every intermediate state as free as the final one.
	Zenith_Vector<u_int> auOrder;
	auOrder.Reserve(axKeys.GetSize());
	for (u_int u = 0; u < axKeys.GetSize(); ++u)
	{
		auOrder.PushBack(u);
	}
	const bool bForward = fDeltaSeconds > 0.0f;
	std::sort(auOrder.begin(), auOrder.end(), [&afCurrent, bForward](u_int uA, u_int uB)
	{
		return bForward ? (afCurrent.Get(uA) > afCurrent.Get(uB)) : (afCurrent.Get(uA) < afCurrent.Get(uB));
	});

	// ---- apply, as ONE undo step --------------------------------------------
	if (!m_xDocument.BeginCompound())
	{
		return false;
	}
	for (u_int u = 0; u < auOrder.GetSize(); ++u)
	{
		const u_int uIndex = auOrder.Get(u);
		const Zenith_AnimSelectedKey& xEntry = axKeys.Get(uIndex);
		if (!m_xDocument.SetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, afCurrent.Get(uIndex) + fDeltaSeconds))
		{
			// The pre-check said this was free, so reaching here means the document
			// and the check disagree — roll the whole thing back rather than leave a
			// partially retimed block on the stack, and light the key anyway so the
			// refusal is not silent.
			m_xDocument.EndCompound(szDescription, /*bKeep*/ false);
			RaiseCollisionFlash(xEntry.m_xTrack, xEntry.m_uKeyId);
			NotifyDocumentEdited();
			return false;
		}
	}
	if (!m_xDocument.EndCompound(szDescription, /*bKeep*/ true))
	{
		return false;
	}

	NotifyDocumentEdited();
	return true;
}

float Zenith_EditorPanel_Animation::EffectiveDragDelta(float fRawDeltaSeconds, bool bSnap) const
{
	if (!bSnap || !AnimOpsIsFinite(fRawDeltaSeconds))
	{
		return fRawDeltaSeconds;
	}
	Zenith_AnimTrackId xPrimaryTrack;
	u_int uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
	float fPrimaryTime = 0.0f;
	if (!ResolvePrimarySelectedKey(xPrimaryTrack, uPrimaryKeyId)
	 || !m_xDocument.GetKeyTime(xPrimaryTrack, uPrimaryKeyId, fPrimaryTime))
	{
		return fRawDeltaSeconds;
	}
	// ★ ONE SNAP, FOR THE WHOLE SELECTION. Snapping each key to its own nearest
	// frame would quantise the SPACING between them as well as their positions —
	// two keys three-and-a-half frames apart would come out three or four apart
	// depending on where the drag happened to stop, which is a silent edit to
	// something the user was not dragging.
	const float fSnapped = Zenith_AnimTimelineSnapToFrame(fPrimaryTime + fRawDeltaSeconds, GetFrameRate());
	return fSnapped - fPrimaryTime;
}

bool Zenith_EditorPanel_Animation::Action_MoveSelection(float fDeltaSeconds, bool bSnap)
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0 || !AnimOpsIsFinite(fDeltaSeconds))
	{
		return false;
	}
	return MoveKeySetByDelta(m_axSelectedKeys, EffectiveDragDelta(fDeltaSeconds, bSnap), "Move Keyframes");
}

//=============================================================================
// Ripple retime
//=============================================================================

void Zenith_EditorPanel_Animation::CollectAllKeys(Zenith_Vector<Zenith_AnimSelectedKey>& axOut) const
{
	axOut.Clear();
	if (!m_xDocument.IsOpen())
	{
		return;
	}

	static const Flux_AnimTrack aeBoneTracks[3] =
	{
		FLUX_ANIM_TRACK_POSITION, FLUX_ANIM_TRACK_ROTATION, FLUX_ANIM_TRACK_SCALE
	};

	Zenith_Vector<std::string> axBoneNames;
	m_xDocument.GetBoneNamesSorted(axBoneNames);
	for (u_int uBone = 0; uBone < axBoneNames.GetSize(); ++uBone)
	{
		for (u_int uTrack = 0; uTrack < 3u; ++uTrack)
		{
			const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone(axBoneNames.Get(uBone), aeBoneTracks[uTrack]);
			const u_int uCount = m_xDocument.GetKeyCount(xTrack);
			for (u_int u = 0; u < uCount; ++u)
			{
				Zenith_AnimSelectedKey xEntry;
				xEntry.m_xTrack = xTrack;
				xEntry.m_uKeyId = m_xDocument.GetKeyIdAtIndex(xTrack, u);
				axOut.PushBack(xEntry);
			}
		}
	}

	// Root motion's TWO tracks — there is no scale delta (D16).
	for (u_int uTrack = 0; uTrack < 2u; ++uTrack)
	{
		const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::RootMotion(aeBoneTracks[uTrack]);
		const u_int uCount = m_xDocument.GetKeyCount(xTrack);
		for (u_int u = 0; u < uCount; ++u)
		{
			Zenith_AnimSelectedKey xEntry;
			xEntry.m_xTrack = xTrack;
			xEntry.m_uKeyId = m_xDocument.GetKeyIdAtIndex(xTrack, u);
			axOut.PushBack(xEntry);
		}
	}
}

bool Zenith_EditorPanel_Animation::Action_RippleRetime(float fFromTime, float fDelta)
{
	if (!m_xDocument.IsOpen() || !AnimOpsIsFinite(fFromTime) || !AnimOpsIsFinite(fDelta))
	{
		return false;
	}

	Zenith_Vector<Zenith_AnimSelectedKey> axAll;
	CollectAllKeys(axAll);

	Zenith_Vector<Zenith_AnimSelectedKey> axMoving;
	axMoving.Reserve(axAll.GetSize());
	for (u_int u = 0; u < axAll.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = axAll.Get(u);
		float fTime = 0.0f;
		if (!m_xDocument.GetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, fTime))
		{
			continue;
		}
		// "At or after" — the boundary key moves, at the clip's own epsilon.
		if (fTime >= fFromTime - fANIM_TIME_EPSILON)
		{
			axMoving.PushBack(xEntry);
		}
	}

	if (axMoving.GetSize() == 0)
	{
		return false;
	}

	// ★ EVENTS ARE NOT IN THIS SET, AND THAT IS D4. An event's time is a [0,1]
	// fraction of the clip, not a point on the seconds clock the keys are on, so
	// it is ALREADY relative to whatever the clip's duration is. Shifting it
	// "proportionally" alongside the keys would move it a second time.
	return MoveKeySetByDelta(axMoving, fDelta, "Ripple Retime");
}

//=============================================================================
// Delete
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_DeleteSelection()
{
	if (!m_xDocument.IsOpen())
	{
		return false;
	}
	if (m_axSelectedKeys.GetSize() == 0 && m_auSelectedEventIds.GetSize() == 0)
	{
		return false;
	}
	if (!m_xDocument.BeginCompound())
	{
		return false;
	}

	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		// A stale id is skipped, not refused: a selection is allowed to name keys
		// an earlier undo has not brought back yet, and refusing the whole delete
		// over one of them would make the panel feel broken for no benefit.
		m_xDocument.RemoveKey(xEntry.m_xTrack, xEntry.m_uKeyId);
	}
	for (u_int u = 0; u < m_auSelectedEventIds.GetSize(); ++u)
	{
		m_xDocument.RemoveEvent(m_auSelectedEventIds.Get(u));
	}

	// ★ THE SELECTION IS DELIBERATELY LEFT ALONE. Every removal's undo re-inserts
	// its key under the ORIGINAL id, so the ids still listed here resolve again
	// the moment the delete is undone and the user gets their selection back with
	// their keys. Clearing here would throw away the only thing that can restore
	// it, and stable ids would have bought nothing.
	if (!m_xDocument.EndCompound("Delete Selection", /*bKeep*/ true))
	{
		return false;
	}
	NotifyDocumentEdited();
	return true;
}

//=============================================================================
// Insert-set (shared by duplicate and paste)
//=============================================================================

bool Zenith_EditorPanel_Animation::InsertKeySetAsOneStep(const Zenith_Vector<Zenith_AnimTrackId>& axTracks,
	const Zenith_Vector<float>& afTimes, const Zenith_Vector<Zenith_AnimKeyValue>& axValues,
	const char* szDescription)
{
	const u_int uCount = axTracks.GetSize();
	if (!m_xDocument.IsOpen() || uCount == 0 || afTimes.GetSize() != uCount || axValues.GetSize() != uCount)
	{
		return false;
	}

	// ---- pre-validate against the CLIP --------------------------------------
	// Nothing is moving, so every existing key at a target time is a blocker.
	Zenith_Vector<Zenith_AnimSelectedKey> axNothingMoving;
	for (u_int u = 0; u < uCount; ++u)
	{
		const float fTime = afTimes.Get(u);
		if (!AnimOpsIsFinite(fTime) || fTime < -fANIM_TIME_EPSILON)
		{
			return false;
		}
		if (WouldCollide(axTracks.Get(u), fTime, axNothingMoving))
		{
			return false;
		}
	}

	// ---- pre-validate against ITSELF ----------------------------------------
	// ★ Two entries of the SAME operation landing on the same slot would have the
	// second silently overwrite the first's value (D25 turns an insert onto an
	// occupied time into a value edit) and the operation would report success
	// having produced fewer keys than it was asked for.
	for (u_int u = 1; u < uCount; ++u)
	{
		for (u_int v = 0; v < u; ++v)
		{
			if (axTracks.Get(u) == axTracks.Get(v)
			 && std::fabs(afTimes.Get(u) - afTimes.Get(v)) <= fANIM_TIME_EPSILON)
			{
				return false;
			}
		}
	}

	if (!m_xDocument.BeginCompound())
	{
		return false;
	}

	Zenith_Vector<Zenith_AnimSelectedKey> axNewKeys;
	axNewKeys.Reserve(uCount);
	for (u_int u = 0; u < uCount; ++u)
	{
		// The document's own verb: it creates the bone channel when the bone has
		// none (which is exactly what a cross-bone paste onto an unanimated bone
		// needs) and it allocates a FRESH stable id for every one of these.
		const u_int uNewId = m_xDocument.InsertKey(axTracks.Get(u), afTimes.Get(u), axValues.Get(u));
		if (uNewId == uINVALID_ANIM_KEY_ID)
		{
			m_xDocument.EndCompound(szDescription, /*bKeep*/ false);
			NotifyDocumentEdited();
			return false;
		}
		Zenith_AnimSelectedKey xEntry;
		xEntry.m_xTrack = axTracks.Get(u);
		xEntry.m_uKeyId = uNewId;
		axNewKeys.PushBack(xEntry);
	}

	if (!m_xDocument.EndCompound(szDescription, /*bKeep*/ true))
	{
		return false;
	}

	// ★ THE NEW KEYS BECOME THE SELECTION. The gesture after a duplicate or a
	// paste is almost always "now drag them", and leaving the ORIGINALS selected
	// would drag the wrong ones — with the copies sitting right beside them,
	// which is the version of that mistake nobody notices.
	Action_ClearSelection();
	for (u_int u = 0; u < axNewKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = axNewKeys.Get(u);
		ApplyKeySelectMode(xEntry.m_xTrack, xEntry.m_uKeyId, ZENITH_ANIMSELECT_ADD);
	}

	NotifyDocumentEdited();
	return true;
}

//=============================================================================
// Duplicate
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_DuplicateSelection()
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0)
	{
		return false;
	}

	const u_int uFrameRate = GetFrameRate();
	if (uFrameRate == 0u)
	{
		// No frame grid means no "one frame" to offset by, and inventing a fallback
		// spacing would place the copies somewhere the clip's own authoring rate
		// does not describe.
		return false;
	}
	const float fOneFrame = Zenith_AnimTimelineFrameToTime(1u, uFrameRate);

	Zenith_Vector<Zenith_AnimTrackId> axTracks;
	Zenith_Vector<float> afTimes;
	Zenith_Vector<Zenith_AnimKeyValue> axValues;
	axTracks.Reserve(m_axSelectedKeys.GetSize());
	afTimes.Reserve(m_axSelectedKeys.GetSize());
	axValues.Reserve(m_axSelectedKeys.GetSize());

	float fMinTime = 0.0f;
	float fMaxTime = 0.0f;
	bool bHaveSpan = false;
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		float fTime = 0.0f;
		Zenith_AnimKeyValue xValue;
		if (!m_xDocument.GetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, fTime)
		 || !m_xDocument.GetKeyValue(xEntry.m_xTrack, xEntry.m_uKeyId, xValue))
		{
			continue;
		}
		if (!bHaveSpan || fTime < fMinTime) { fMinTime = fTime; }
		if (!bHaveSpan || fTime > fMaxTime) { fMaxTime = fTime; }
		bHaveSpan = true;

		axTracks.PushBack(xEntry.m_xTrack);
		afTimes.PushBack(fTime);
		axValues.PushBack(xValue);
	}
	if (!bHaveSpan)
	{
		return false;
	}

	// ★ ONE FRAME PAST THE LAST KEY OF THE SELECTION, not one frame past each
	// key. For a single key those are the same thing and this IS "t + one frame";
	// for a block they are not, and the per-key version has the copies land on
	// top of the originals' own neighbours — which the collision rule would then
	// refuse, making duplicate useless on exactly the dense selections it is for.
	const float fOffset = (fMaxTime - fMinTime) + fOneFrame;
	for (u_int u = 0; u < afTimes.GetSize(); ++u)
	{
		afTimes.Get(u) += fOffset;
	}

	return InsertKeySetAsOneStep(axTracks, afTimes, axValues, "Duplicate Keyframes");
}

//=============================================================================
// Copy / cross-bone paste
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_CopySelection()
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0)
	{
		return false;
	}

	float fMinTime = 0.0f;
	bool bHaveMin = false;
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		float fTime = 0.0f;
		if (!m_xDocument.GetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, fTime))
		{
			continue;
		}
		if (!bHaveMin || fTime < fMinTime) { fMinTime = fTime; }
		bHaveMin = true;
	}
	if (!bHaveMin)
	{
		return false;
	}

	m_axClipboard.Clear();
	m_axClipboard.Reserve(m_axSelectedKeys.GetSize());
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		float fTime = 0.0f;
		Zenith_AnimKeyValue xValue;
		if (!m_xDocument.GetKeyTime(xEntry.m_xTrack, xEntry.m_uKeyId, fTime)
		 || !m_xDocument.GetKeyValue(xEntry.m_xTrack, xEntry.m_uKeyId, xValue))
		{
			continue;
		}
		Zenith_AnimClipboardKey xCopy;
		// ★ THE KIND, NOT THE TRACK. The bone is supplied by whoever pastes, which
		// is the whole of what makes this cross-bone.
		xCopy.m_eTrack = xEntry.m_xTrack.m_eTrack;
		xCopy.m_bRootMotion = xEntry.m_xTrack.m_bRootMotion;
		// RELATIVE to the earliest key copied, so a paste offset is one addition
		// and the internal spacing survives exactly.
		xCopy.m_fRelativeTimeSeconds = fTime - fMinTime;
		xCopy.m_xValue = xValue;
		m_axClipboard.PushBack(xCopy);
	}

	return m_axClipboard.GetSize() > 0;
}

bool Zenith_EditorPanel_Animation::Action_PasteToBone(const std::string& strBoneName, float fTimeOffset)
{
	if (!m_xDocument.IsOpen() || m_axClipboard.GetSize() == 0 || !AnimOpsIsFinite(fTimeOffset))
	{
		return false;
	}

	Zenith_Vector<Zenith_AnimTrackId> axTracks;
	Zenith_Vector<float> afTimes;
	Zenith_Vector<Zenith_AnimKeyValue> axValues;
	axTracks.Reserve(m_axClipboard.GetSize());
	afTimes.Reserve(m_axClipboard.GetSize());
	axValues.Reserve(m_axClipboard.GetSize());

	for (u_int u = 0; u < m_axClipboard.GetSize(); ++u)
	{
		const Zenith_AnimClipboardKey& xCopy = m_axClipboard.Get(u);
		Zenith_AnimTrackId xTarget;
		if (xCopy.m_bRootMotion)
		{
			// ★ ROOT MOTION PASTES BACK ONTO ROOT MOTION whatever bone was named.
			// The bone is meaningless for a root delta, and DROPPING these instead
			// would make a paste quietly deliver fewer keys than were copied.
			xTarget = Zenith_AnimTrackId::RootMotion(xCopy.m_eTrack);
		}
		else
		{
			if (strBoneName.empty())
			{
				return false;
			}
			// KIND-MATCHED: T->T, R->R, S->S. A rotation key carries a quaternion and
			// a translation key a vector, and the document asserts the tag against
			// the track — so a cross-KIND paste is not merely wrong, it is refused
			// one layer down.
			xTarget = Zenith_AnimTrackId::Bone(strBoneName, xCopy.m_eTrack);
		}

		axTracks.PushBack(xTarget);
		afTimes.PushBack(xCopy.m_fRelativeTimeSeconds + fTimeOffset);
		axValues.PushBack(xCopy.m_xValue);
	}

	if (!InsertKeySetAsOneStep(axTracks, afTimes, axValues, "Paste Keyframes"))
	{
		return false;
	}
	if (!strBoneName.empty())
	{
		m_strPasteTargetBone = strBoneName;
	}
	return true;
}

//=============================================================================
// Playhead, duration, undo/redo
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_Scrub(float fTimeSeconds)
{
	if (!m_xSession.IsOpen() || !AnimOpsIsFinite(fTimeSeconds))
	{
		return false;
	}
	const float fDuration = m_xDocument.IsOpen() ? m_xDocument.GetDuration() : m_xSession.GetDuration();
	float fClamped = fTimeSeconds;
	if (fClamped < 0.0f) { fClamped = 0.0f; }
	if (fDuration > 0.0f && fClamped > fDuration) { fClamped = fDuration; }

	// ★ NO EVENTS ARE EMITTED, and that is the session's contract rather than
	// something added here: Flux_AnimationController::SeekDirectPlay advances the
	// event bookkeeping mark WITHOUT firing what the playhead skipped (D40). A
	// scrub that fired events would make dragging across a clip replay every
	// footstep in it.
	return m_xSession.Seek(fClamped);
}

bool Zenith_EditorPanel_Animation::Action_SetDuration(float fDurationSeconds)
{
	if (!m_xDocument.IsOpen() || !AnimOpsIsFinite(fDurationSeconds) || fDurationSeconds < 0.0f)
	{
		return false;
	}
	if (std::fabs(fDurationSeconds - m_xDocument.GetDuration()) <= fANIM_TIME_EPSILON)
	{
		// Nothing to do, and pushing a command for it would put a no-op stop on the
		// undo stack.
		return false;
	}
	if (!m_xDocument.SetDuration(fDurationSeconds))
	{
		return false;
	}
	NotifyDocumentEdited();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_Undo()
{
	if (!m_xDocument.IsOpen() || m_xDocument.IsCompoundOpen() || !m_xDocument.CanUndo())
	{
		return false;
	}
	m_xDocument.Undo();
	NotifyDocumentEdited();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_Redo()
{
	if (!m_xDocument.IsOpen() || m_xDocument.IsCompoundOpen() || !m_xDocument.CanRedo())
	{
		return false;
	}
	m_xDocument.Redo();
	NotifyDocumentEdited();
	return true;
}

//=============================================================================
// EVENTS (WU-5B)
//
// ★ NOT ONE OF THESE PRE-CHECKS A COLLISION, and that is the difference from
// every key operation above rather than an omission. D11 forbids two keys on
// one time because the second insert would REPLACE the first's value (D25) and
// the operation would report success having produced fewer keys than it was
// asked for. An event list has no such constraint — Flux_AnimationClip::AddEvent
// appends and sorts, two events at one normalized time are two events, and both
// are dispatched by Flux_AnimationController::EmitSpanEvents. Two footsteps on
// the same frame is a thing an animator means.
//
// ★ EVERY TIME HERE IS NORMALIZED (D4). The one place seconds appear is the
// SNAP, which has to happen on the seconds clock because the frame grid is the
// clip's authored frame rate — and it is converted straight back.
//=============================================================================

const char* Zenith_EditorPanel_Animation::DefaultEventName()
{
	return "Event";
}

bool Zenith_EditorPanel_Animation::Action_AddEvent(float fNormalizedTime, const std::string& strName)
{
	if (!m_xDocument.IsOpen() || !AnimOpsIsFinite(fNormalizedTime) || fNormalizedTime < 0.0f)
	{
		return false;
	}
	// ★ AN EMPTY NAME IS REFUSED. The row would draw a flag with no label and the
	// runtime dispatcher would hand listeners an empty string, so the event exists
	// and matches nothing — a silent no-op wearing an undo entry. Every gesture
	// that creates one passes DefaultEventName().
	if (strName.empty())
	{
		return false;
	}

	const u_int uEventId = m_xDocument.AddEvent(strName, fNormalizedTime, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	if (uEventId == uINVALID_ANIM_KEY_ID)
	{
		return false;
	}

	// ★ THE NEW EVENT BECOMES THE SELECTION, for the same reason a duplicate's
	// copies do: the gesture straight after "add an event" is "name it", and the
	// inspector strip edits whatever is selected. Leaving the previous selection
	// in place would put the user's typing into a different event.
	ApplyEventSelectMode(uEventId, ZENITH_ANIMSELECT_REPLACE);

	NotifyDocumentEdited();
	return true;
}

float Zenith_EditorPanel_Animation::EffectiveEventDragDelta(float fRawDeltaNormalized, bool bSnap) const
{
	if (!bSnap || !AnimOpsIsFinite(fRawDeltaNormalized))
	{
		return fRawDeltaNormalized;
	}
	const float fDuration = m_xDocument.IsOpen() ? m_xDocument.GetDuration() : 0.0f;
	const u_int uFrameRate = GetFrameRate();
	// ★ NO DURATION MEANS NO FRAME GRID IN NORMALIZED SPACE. "The nearest frame"
	// is a fact about the seconds clock; without a duration to divide by there is
	// no way to express it as a fraction, and inventing one would land the event
	// somewhere the clip's own authoring rate does not describe.
	if (fDuration <= 0.0f || uFrameRate == 0u)
	{
		return fRawDeltaNormalized;
	}

	u_int uPrimaryEventId = uINVALID_ANIM_KEY_ID;
	Flux_AnimationEvent xPrimary;
	if (!ResolvePrimarySelectedEvent(uPrimaryEventId) || !m_xDocument.GetEvent(uPrimaryEventId, xPrimary))
	{
		return fRawDeltaNormalized;
	}

	// ★ ONE SNAP, FOR THE WHOLE SELECTION — the same rule EffectiveDragDelta
	// applies to keys, and for the same reason: snapping each event to its own
	// nearest frame quantises the SPACING between them as well as their
	// positions, silently editing something the user was not dragging.
	const float fTargetSeconds = (xPrimary.m_fNormalizedTime + fRawDeltaNormalized) * fDuration;
	const float fSnappedSeconds = Zenith_AnimTimelineSnapToFrame(fTargetSeconds, uFrameRate);
	return (fSnappedSeconds / fDuration) - xPrimary.m_fNormalizedTime;
}

bool Zenith_EditorPanel_Animation::Action_MoveSelectedEvents(float fDeltaNormalized, bool bSnap)
{
	if (!m_xDocument.IsOpen() || m_auSelectedEventIds.GetSize() == 0 || !AnimOpsIsFinite(fDeltaNormalized))
	{
		return false;
	}

	const float fDelta = EffectiveEventDragDelta(fDeltaNormalized, bSnap);
	if (!AnimOpsIsFinite(fDelta))
	{
		return false;
	}

	// ---- resolve every current time BEFORE anything moves --------------------
	// A stale id is SKIPPED rather than refused, unlike the key move: a key move
	// refuses because a partial move changes the spacing the user was preserving,
	// and every key in the set moves by the same delta so a missing one is a real
	// inconsistency. An event selection legitimately holds ids a delete has taken
	// away and an undo has not brought back yet, and there is no spacing contract
	// between events to break.
	Zenith_Vector<u_int> auMoving;
	Zenith_Vector<float> afTargets;
	auMoving.Reserve(m_auSelectedEventIds.GetSize());
	afTargets.Reserve(m_auSelectedEventIds.GetSize());
	for (u_int u = 0; u < m_auSelectedEventIds.GetSize(); ++u)
	{
		const u_int uEventId = m_auSelectedEventIds.Get(u);
		Flux_AnimationEvent xEvent;
		if (!m_xDocument.GetEvent(uEventId, xEvent))
		{
			continue;
		}
		const float fTarget = xEvent.m_fNormalizedTime + fDelta;
		if (fTarget < 0.0f)
		{
			// A clip starts at 0. Clamping the strays instead would collapse a
			// multi-event drag onto 0 and change their relative spacing — the one
			// thing a multi-event drag must not do, exactly as for keys.
			return false;
		}
		auMoving.PushBack(uEventId);
		afTargets.PushBack(fTarget);
	}
	if (auMoving.GetSize() == 0)
	{
		return false;
	}

	// A move smaller than the clip's own epsilon is not a move: applying it would
	// push an undo entry whose Ctrl+Z visibly does nothing. Measured in SECONDS,
	// because fANIM_TIME_EPSILON is a seconds tolerance and a normalized epsilon
	// would mean something different on every clip.
	const float fDuration = m_xDocument.GetDuration();
	const float fDeltaSeconds = fDuration > 0.0f ? fDelta * fDuration : fDelta;
	if (std::fabs(fDeltaSeconds) <= fANIM_TIME_EPSILON)
	{
		return false;
	}

	if (!m_xDocument.BeginCompound())
	{
		return false;
	}
	for (u_int u = 0; u < auMoving.GetSize(); ++u)
	{
		// ★ NO ORDERING RULE, unlike MoveKeySetByDelta. That function applies a
		// forward move in descending time so no key lands on a slot its neighbour
		// has not vacated; SetEventTime has no occupancy test to trip over, so
		// every order produces the same list.
		m_xDocument.SetEventTime(auMoving.Get(u), afTargets.Get(u));
	}
	if (!m_xDocument.EndCompound("Move Animation Events", /*bKeep*/ true))
	{
		return false;
	}

	NotifyDocumentEdited();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_RenameEvent(u_int uEventId, const std::string& strName)
{
	Flux_AnimationEvent xEvent;
	if (!m_xDocument.IsOpen() || strName.empty() || !m_xDocument.GetEvent(uEventId, xEvent))
	{
		return false;
	}
	if (xEvent.m_strEventName == strName)
	{
		// A field that was focused and left alone. Pushing here would put a no-op
		// stop on the undo stack for every click into the name box.
		return false;
	}
	if (!m_xDocument.SetEventName(uEventId, strName))
	{
		return false;
	}
	NotifyDocumentEdited();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SetEventPayload(u_int uEventId, const Zenith_Maths::Vector4& xPayload)
{
	Flux_AnimationEvent xEvent;
	if (!m_xDocument.IsOpen() || !m_xDocument.GetEvent(uEventId, xEvent))
	{
		return false;
	}
	if (!AnimOpsIsFinite(xPayload.x) || !AnimOpsIsFinite(xPayload.y)
	 || !AnimOpsIsFinite(xPayload.z) || !AnimOpsIsFinite(xPayload.w))
	{
		// A NaN here reaches the .zanim and then every listener that reads the
		// payload; refuse rather than launder it.
		return false;
	}
	if (xEvent.m_xData.x == xPayload.x && xEvent.m_xData.y == xPayload.y
	 && xEvent.m_xData.z == xPayload.z && xEvent.m_xData.w == xPayload.w)
	{
		return false;
	}
	if (!m_xDocument.SetEventPayload(uEventId, xPayload))
	{
		return false;
	}
	NotifyDocumentEdited();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SetEmitEventsOnScrub(bool bEmit)
{
	if (!m_xSession.IsOpen())
	{
		return false;
	}
	if (m_xSession.Controller().GetEmitEventsOnSeek() == bEmit)
	{
		return false;
	}
	// ★ NOT AN EDIT. It writes nothing to the document, dirties nothing and
	// pushes no undo — it is a property of how the PREVIEW behaves, exactly like
	// the session's own loop toggle, and putting it on the clip's undo stack
	// would make Ctrl+Z change what the playhead does.
	m_xSession.Controller().SetEmitEventsOnSeek(bEmit);
	return true;
}

bool Zenith_EditorPanel_Animation::GetEmitEventsOnScrub() const
{
	return m_xSession.Controller().GetEmitEventsOnSeek();
}

#endif // ZENITH_TOOLS
