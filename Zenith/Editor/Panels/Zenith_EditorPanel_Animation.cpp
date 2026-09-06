#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_Editor.h"          // Instance() resolves the editor-owned panel
#include "Editor/Zenith_Gizmo.h"
#include "Core/Zenith_Engine.h"            // g_xEngine.Editor()
#include "Core/Zenith_EditorWindowNames.h"
#include "Flux/Flux_ViewConstants.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"   // the pure orbit / view-constants builders

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <string>

//=============================================================================
// Lifecycle, rows, warnings and the hit-rect accessors.
//
// The DRAWING half lives in Zenith_EditorPanel_Animation_Render.cpp — split
// because the graph editor's 1337-line single TU is the thing this panel is
// explicitly not repeating, and because "what the panel knows" and "what the
// panel paints" are the two halves a reader wants separately.
//=============================================================================

Zenith_EditorPanel_Animation& Zenith_EditorPanel_Animation::Instance()
{
	// ★ THE OBJECT IS Zenith_Editor'S — see the header. This used to be a
	// function-local static, whose destructor runs at ATEXIT, i.e. after
	// Zenith_AssetRegistry::Shutdown has force-deleted every asset. Any owning
	// handle the panel still held then Released into freed memory: an assert
	// ("Release called on asset with 0 ref count") when the freed word happened to
	// read zero, and silent heap corruption when it did not.
	//
	// A unit that wants an isolated panel still constructs one on the stack; this
	// accessor is only for the editor's single one.
	Zenith_Assert(g_xEngine.HasEditor(),
		"Zenith_EditorPanel_Animation::Instance() before the editor was allocated");
	Zenith_EditorPanel_Animation* pxPanel = g_xEngine.Editor().TryGetAnimationPanel();
	Zenith_Assert(pxPanel != nullptr,
		"Zenith_EditorPanel_Animation::Instance() outside Zenith_Editor::Initialise..Shutdown");
	return *pxPanel;
}

Zenith_EditorPanel_Animation::Zenith_EditorPanel_Animation()
	: m_xSession(std::string(szEDITOR_WINDOW_ANIMATION_EDITOR))
{
}

Zenith_EditorPanel_Animation::~Zenith_EditorPanel_Animation()
{
	// Deliberately NOT unregistering m_ulPreviewImageHandle here: see the member
	// comment. Everything that owns an asset handle is dropped by Shutdown(),
	// which the editor calls while the registry is alive.
}

void Zenith_EditorPanel_Animation::Shutdown()
{
	CloseClip();
}

//=============================================================================
// Keys
//=============================================================================

std::string Zenith_EditorPanel_Animation::MakeTrackKey(const Zenith_AnimTrackId& xTrack)
{
	// The '$' prefix cannot collide with a bone name coming out of an importer,
	// and the numeric suffix keeps the three tracks of one bone distinct.
	const char acDigits[4] = { '0', '1', '2', '3' };
	const u_int uTrack = static_cast<u_int>(xTrack.m_eTrack) & 0x3u;
	if (xTrack.m_bRootMotion)
	{
		return std::string("$rootmotion/") + acDigits[uTrack];
	}
	return xTrack.m_strBoneName + "/" + acDigits[uTrack];
}

u_int64 Zenith_EditorPanel_Animation::MakeKeyRectKey(u_int uRowIndex, u_int uKeyId)
{
	return (static_cast<u_int64>(uRowIndex) << 32) | static_cast<u_int64>(uKeyId);
}

//=============================================================================
// Document lifecycle
//=============================================================================

bool Zenith_EditorPanel_Animation::OpenClip(const std::string& strAssetPath)
{
	m_strLastOpenAttemptPath = strAssetPath;
	m_eLastOpenResult = m_xDocument.Open(strAssetPath);
	if (m_eLastOpenResult != ZENITH_ANIMDOC_OPEN_OK)
	{
		// A refusal changes NOTHING — the document is untouched and whatever was
		// open stays open. The panel still shows itself, because every refusal has
		// a UI answer (promote, save/discard, or a path to correct) and a silent
		// no-op with the window hidden is the one outcome that helps nobody.
		m_bShow = true;
		return false;
	}
	OnDocumentOpened();
	return true;
}

bool Zenith_EditorPanel_Animation::PromoteAndOpenAuthoredOverride(const std::string& strSourceAssetPath)
{
	m_strLastOpenAttemptPath = strSourceAssetPath;
	m_eLastOpenResult = m_xDocument.PromoteToAuthoredOverride(strSourceAssetPath);
	if (m_eLastOpenResult != ZENITH_ANIMDOC_OPEN_OK)
	{
		m_bShow = true;
		return false;
	}
	OnDocumentOpened();
	return true;
}

void Zenith_EditorPanel_Animation::OnDocumentOpened()
{
	m_bShow = true;
	m_bCloseRefusedDirty = false;
	m_bExternalConflict = false;

	// The preview holds its own deep copy (D30/D32) and claims the shared slot.
	// It opens PAUSED at t=0: a dope sheet is a still frame you scrub, and a clip
	// that starts running the moment it is opened moves the playhead out from
	// under the first thing anybody wants to look at.
	m_xSession.Open(m_xDocument.GetClip(), m_xDocument.GetAssetPath());
	m_xSession.Pause();
	m_xSession.Seek(0.0f);

	// ★ THE PANEL LISTENS TO THE SESSION'S OWN CONTROLLER (D30). Registered here,
	// after Open has armed direct play, so the emitted strip reports what the
	// RUNTIME dispatcher fired rather than a re-derivation of which events the
	// playhead crossed — a second opinion would agree with D35-D40 exactly until
	// one of them changed. `this` is stable: the session is a member of the panel,
	// so the callback cannot outlive its user data.
	m_xSession.Controller().SetEventCallback(&Zenith_EditorPanel_Animation::OnPreviewEventEmitted, this);
	ClearEmittedEvents();

	m_xCollapsedGroups.Clear();
	// ★ THE SELECTION AND THE CLIPBOARD DO NOT SURVIVE AN OPEN. A key id is a
	// session identity issued by THIS document, and Open() retires every one it
	// had issued — carrying them across would leave a selection resolving to keys
	// in a clip nobody is looking at, or (worse) not resolving at all while every
	// operation quietly shrank by one.
	Action_ClearSelection();
	m_axClipboard.Clear();
	m_strPasteTargetBone.clear();
	m_uCollisionFlashFrames = 0;
	m_uCollisionFlashKeyId = uINVALID_ANIM_KEY_ID;

	m_bDraggingEvents = false;
	m_fEventDragDeltaNormalized = 0.0f;
	m_bEventContextMenuRequested = false;
	m_uInspectorBufferEventId = uINVALID_ANIM_KEY_ID;
	m_bEventInspectorEditing = false;

	m_fRowScrollPixels = 0.0f;
	m_bPendingTimeScroll = false;
	m_bPendingRowScroll = false;
	m_bClipRefreshPending = false;
	m_uSeenUndoDepth = m_xDocument.GetUndoStackSize();
	m_uSeenRedoDepth = m_xDocument.GetRedoStackSize();

	RebuildRows();
	RecountKeysPastDuration();

	// Frame the whole clip — but the track rect is not known until the panel has
	// been laid out, and FrameAll needs a width to divide by. Calling it now only
	// reaches its own "nothing to fit into" fallback and leaves the zoom at the
	// default, so the real fit is deferred to the first sheet pass. (Clamp does
	// not finish the job: it sanitises a view, it does not fit one.) The call
	// below still runs so View() is coherent for a caller that reads it before
	// the first frame.
	Zenith_AnimTimelineFrameAll(m_xView, m_xDocument.GetDuration());
	m_bPendingFrameAll = true;

	const std::string& strPath = m_xDocument.GetAssetPath();
	snprintf(m_acPathBuffer, sizeof(m_acPathBuffer), "%s", strPath.c_str());
	snprintf(m_acSkeletonBuffer, sizeof(m_acSkeletonBuffer), "%s", m_xSession.GetSkeletonPath().c_str());
	snprintf(m_acPreviewModelBuffer, sizeof(m_acPreviewModelBuffer), "%s", m_xSession.GetPreviewModelPath().c_str());
}

void Zenith_EditorPanel_Animation::CloseClip()
{
	// Dropped BEFORE the session is closed, so nothing can be dispatched into a
	// panel that is halfway through tearing its state down. The pointer could not
	// dangle either way (the session is a member), but a strip that grew an entry
	// during a close would be reporting a clip nobody has open.
	m_xSession.Controller().ClearEventCallback();

	m_xSession.Close();
	m_xDocument.CloseDiscardingChanges();

	m_axRows.Clear();
	m_xRowIndexByTrackKey.Clear();
	m_xCollapsedGroups.Clear();
	m_uEventsRowIndex = uINVALID_ANIM_SHEET_ROW;

	ClearFrameRects();

	m_xKeysPastDuration.Clear();
	m_uKeysPastDuration = 0;
	m_uEventsPastDuration = 0;

	m_bExternalConflict = false;
	m_bCloseRefusedDirty = false;
	m_bClipRefreshPending = false;
	m_fRowScrollPixels = 0.0f;
	m_bPendingTimeScroll = false;
	m_bPendingRowScroll = false;
	m_bPendingFrameAll = false;

	// Every id the selection and the clipboard hold was issued by the document
	// that has just been thrown away.
	Action_ClearSelection();
	m_axClipboard.Clear();
	m_strPasteTargetBone.clear();
	m_uCollisionFlashFrames = 0;
	m_uCollisionFlashKeyId = uINVALID_ANIM_KEY_ID;
	m_bDraggingKeys = false;
	m_bBoxSelecting = false;
	m_bScrubbing = false;
	m_bDraggingDuration = false;
	m_fDragDeltaSeconds = 0.0f;

	m_bDraggingEvents = false;
	m_fEventDragDeltaNormalized = 0.0f;
	m_bEventContextMenuRequested = false;
	m_uInspectorBufferEventId = uINVALID_ANIM_KEY_ID;
	m_bEventInspectorEditing = false;
	ClearEmittedEvents();
}

Zenith_AnimDocCloseResult Zenith_EditorPanel_Animation::RequestCloseClip()
{
	const Zenith_AnimDocCloseResult eResult = m_xDocument.Close();
	if (eResult != ZENITH_ANIMDOC_CLOSE_OK)
	{
		m_bCloseRefusedDirty = true;
		return eResult;
	}
	// The document is already closed; CloseClip only has to tear the rest down,
	// and CloseDiscardingChanges on a closed document is a no-op.
	CloseClip();
	return eResult;
}

void Zenith_EditorPanel_Animation::SyncSessionWithDocument()
{
	if (!m_xSession.IsOpen())
	{
		return;
	}

	const u_int uUndo = m_xDocument.GetUndoStackSize();
	const u_int uRedo = m_xDocument.GetRedoStackSize();
	const bool bDepthMoved = (uUndo != m_uSeenUndoDepth) || (uRedo != m_uSeenRedoDepth);
	if (!bDepthMoved && !m_bClipRefreshPending)
	{
		return;
	}

	m_uSeenUndoDepth = uUndo;
	m_uSeenRedoDepth = uRedo;
	m_bClipRefreshPending = false;
	// Keeps the play head, the rig and the slot claim; re-copies the content.
	m_xSession.RefreshClipFrom(m_xDocument.GetClip());
}

void Zenith_EditorPanel_Animation::RefreshExternalModificationState()
{
	m_bExternalConflict = m_xDocument.IsOpen() && m_xDocument.HasExternalModification();
}

u_int Zenith_EditorPanel_Animation::GetFrameRate() const
{
	if (!m_xDocument.IsOpen())
	{
		return 0u;
	}
	return static_cast<u_int>(m_xDocument.GetClip().GetMetadata().m_uAuthoredFrameRate);
}

//=============================================================================
// Rows
//=============================================================================

bool Zenith_EditorPanel_Animation::IsGroupCollapsed(const std::string& strGroupKey) const
{
	return m_xCollapsedGroups.Contains(strGroupKey);
}

void Zenith_EditorPanel_Animation::SetGroupCollapsed(const std::string& strGroupKey, bool bCollapsed)
{
	if (bCollapsed)
	{
		m_xCollapsedGroups.Insert(strGroupKey);
	}
	else
	{
		m_xCollapsedGroups.Remove(strGroupKey);
	}
	RebuildRows();
}

void Zenith_EditorPanel_Animation::RebuildRows()
{
	m_axRows.Clear();
	m_xRowIndexByTrackKey.Clear();
	m_uEventsRowIndex = uINVALID_ANIM_SHEET_ROW;

	if (!m_xDocument.IsOpen())
	{
		return;
	}

	// ★ THE SAME ORDER THE FILE IS WRITTEN IN (D5). GetBoneNamesSorted imposes
	// the total order Flux_AnimationClip::WriteToDataStream uses, so a row list
	// and a diff of two .zanim files agree about which bone is which.
	Zenith_Vector<std::string> axBoneNames;
	m_xDocument.GetBoneNamesSorted(axBoneNames);

	static const Flux_AnimTrack aeBoneTracks[3] =
	{
		FLUX_ANIM_TRACK_POSITION, FLUX_ANIM_TRACK_ROTATION, FLUX_ANIM_TRACK_SCALE
	};
	static const char* const aszBoneTrackLabels[3] = { "Translation", "Rotation", "Scale" };

	for (u_int uBone = 0; uBone < axBoneNames.GetSize(); ++uBone)
	{
		const std::string& strBone = axBoneNames.Get(uBone);
		const u_int uHeaderRow = m_axRows.GetSize();

		Zenith_AnimSheetRow xHeader;
		xHeader.m_eKind = ZENITH_ANIMSHEET_ROW_BONE_HEADER;
		xHeader.m_strLabel = strBone;
		xHeader.m_strGroupKey = strBone;
		m_axRows.PushBack(xHeader);

		if (IsGroupCollapsed(strBone))
		{
			continue;
		}

		for (u_int u = 0; u < 3u; ++u)
		{
			// ★ ALL THREE SUB-ROWS, EXISTING OR NOT. A channel is DELETED when its
			// last key goes (D14), so drawing only the tracks that exist would make
			// a row vanish the moment it was emptied and leave nothing to put a key
			// back onto. An absent track simply draws as an empty lane.
			Zenith_AnimSheetRow xRow;
			xRow.m_eKind = ZENITH_ANIMSHEET_ROW_BONE_TRACK;
			xRow.m_xTrack = Zenith_AnimTrackId::Bone(strBone, aeBoneTracks[u]);
			xRow.m_bHasTrack = true;
			xRow.m_strLabel = aszBoneTrackLabels[u];
			xRow.m_uGroupRowIndex = uHeaderRow;
			xRow.m_strGroupKey = strBone;

			m_xRowIndexByTrackKey[MakeTrackKey(xRow.m_xTrack)] = m_axRows.GetSize();
			m_axRows.PushBack(xRow);
		}
	}

	// Root motion: its OWN group, with the TWO tracks it has. There is no scale
	// delta (D16) and adding a third row would offer a lane every document verb
	// refuses.
	{
		static const char* const szROOT_MOTION_GROUP = "$rootmotion";
		const u_int uHeaderRow = m_axRows.GetSize();

		Zenith_AnimSheetRow xHeader;
		xHeader.m_eKind = ZENITH_ANIMSHEET_ROW_ROOT_MOTION_HEADER;
		xHeader.m_strLabel = "Root Motion";
		xHeader.m_strGroupKey = szROOT_MOTION_GROUP;
		m_axRows.PushBack(xHeader);

		if (!IsGroupCollapsed(szROOT_MOTION_GROUP))
		{
			static const Flux_AnimTrack aeRootTracks[2] = { FLUX_ANIM_TRACK_POSITION, FLUX_ANIM_TRACK_ROTATION };
			static const char* const aszRootLabels[2] = { "Position Delta", "Rotation Delta" };
			for (u_int u = 0; u < 2u; ++u)
			{
				Zenith_AnimSheetRow xRow;
				xRow.m_eKind = ZENITH_ANIMSHEET_ROW_ROOT_MOTION_TRACK;
				xRow.m_xTrack = Zenith_AnimTrackId::RootMotion(aeRootTracks[u]);
				xRow.m_bHasTrack = true;
				xRow.m_strLabel = aszRootLabels[u];
				xRow.m_uGroupRowIndex = uHeaderRow;
				xRow.m_strGroupKey = szROOT_MOTION_GROUP;

				m_xRowIndexByTrackKey[MakeTrackKey(xRow.m_xTrack)] = m_axRows.GetSize();
				m_axRows.PushBack(xRow);
			}
		}
	}

	// Events last, always present, never collapsible — it is the one row a clip
	// with no bones at all still has something to say on.
	{
		Zenith_AnimSheetRow xRow;
		xRow.m_eKind = ZENITH_ANIMSHEET_ROW_EVENTS;
		xRow.m_strLabel = "Events";
		m_uEventsRowIndex = m_axRows.GetSize();
		m_axRows.PushBack(xRow);
	}
}

bool Zenith_EditorPanel_Animation::GetRowAt(u_int uRowIndex, Zenith_AnimSheetRow& xOut) const
{
	if (uRowIndex >= m_axRows.GetSize())
	{
		return false;
	}
	xOut = m_axRows.Get(uRowIndex);
	return true;
}

bool Zenith_EditorPanel_Animation::FindRowIndexForTrack(const Zenith_AnimTrackId& xTrack, u_int& uOutRowIndex) const
{
	const u_int* puRow = m_xRowIndexByTrackKey.TryGet(MakeTrackKey(xTrack));
	if (puRow == nullptr)
	{
		return false;
	}
	uOutRowIndex = *puRow;
	return true;
}

//=============================================================================
// D13 — keys past the duration
//=============================================================================

void Zenith_EditorPanel_Animation::RecountKeysPastDuration()
{
	m_xKeysPastDuration.Clear();
	m_uKeysPastDuration = 0;
	m_uEventsPastDuration = 0;

	if (!m_xDocument.IsOpen())
	{
		return;
	}

	// Compared with the SAME epsilon the clip's own occupancy test uses, so a key
	// sitting exactly ON the duration is never reported — it is sampled.
	const float fLimit = m_xDocument.GetDuration() + fANIM_TIME_EPSILON;

	for (u_int uRow = 0; uRow < m_axRows.GetSize(); ++uRow)
	{
		const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRow);
		if (!xRow.m_bHasTrack)
		{
			continue;
		}
		const u_int uKeyCount = m_xDocument.GetKeyCount(xRow.m_xTrack);
		for (u_int uKey = 0; uKey < uKeyCount; ++uKey)
		{
			const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xRow.m_xTrack, uKey);
			float fTime = 0.0f;
			if (!m_xDocument.GetKeyTime(xRow.m_xTrack, uKeyId, fTime))
			{
				continue;
			}
			if (fTime > fLimit)
			{
				m_xKeysPastDuration.Insert(MakeKeyRectKey(uRow, uKeyId));
				++m_uKeysPastDuration;
			}
		}
	}

	// An event time is a [0,1] FRACTION of the clip (D4), so "past the end" for
	// an event is > 1 — it is not on the seconds clock the keys are on, and
	// comparing it against the duration would flag every event on a clip longer
	// than one second.
	const u_int uEventCount = m_xDocument.GetEventCount();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		const u_int uEventId = m_xDocument.GetEventIdAtIndex(u);
		Flux_AnimationEvent xEvent;
		if (!m_xDocument.GetEvent(uEventId, xEvent))
		{
			continue;
		}
		if (xEvent.m_fNormalizedTime > 1.0f + fANIM_TIME_EPSILON)
		{
			++m_uEventsPastDuration;
		}
	}
}

bool Zenith_EditorPanel_Animation::IsKeyPastDuration(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (!FindRowIndexForTrack(xTrack, uRow))
	{
		return false;
	}
	return m_xKeysPastDuration.Contains(MakeKeyRectKey(uRow, uKeyId));
}

//=============================================================================
// Scroll requests (applied by the NEXT Render — see the header)
//=============================================================================

void Zenith_EditorPanel_Animation::ScrollTimeIntoView(float fTimeSeconds)
{
	// A non-finite time would poison the scroll, and the mapping's own promise is
	// that nothing it returns is a NaN. Refuse rather than launder it.
	if (!(fTimeSeconds == fTimeSeconds))
	{
		return;
	}
	m_bPendingTimeScroll = true;
	m_fPendingTimeScroll = fTimeSeconds;
}

bool Zenith_EditorPanel_Animation::ScrollRowIntoView(const Zenith_AnimTrackId& xTrack)
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (!FindRowIndexForTrack(xTrack, uRow))
	{
		// The row may exist but be COLLAPSED, in which case it has no index at all.
		// Expanding is the right answer: the caller asked to see it.
		const std::string strGroupKey = xTrack.m_bRootMotion ? std::string("$rootmotion") : xTrack.m_strBoneName;
		if (!IsGroupCollapsed(strGroupKey))
		{
			return false;
		}
		SetGroupCollapsed(strGroupKey, false);
		if (!FindRowIndexForTrack(xTrack, uRow))
		{
			return false;
		}
	}
	m_bPendingRowScroll = true;
	m_uPendingRowScroll = uRow;
	return true;
}

void Zenith_EditorPanel_Animation::RequestWindowPlacement(float fScreenX, float fScreenY, float fWidth, float fHeight)
{
	m_bPlacementRequested = true;
	m_fPlacementX = fScreenX;
	m_fPlacementY = fScreenY;
	m_fPlacementWidth = fWidth;
	m_fPlacementHeight = fHeight;
}

//=============================================================================
// Hit rects
//
// ★ THE OFF-SCREEN GATE. Every accessor funnels through PublishRect, and
// PublishRect answers FALSE for a rect whose centre is outside the display
// rather than handing out a coordinate no click can reach. See the header.
//=============================================================================

void Zenith_EditorPanel_Animation::ClearFrameRects()
{
	m_xRowRects.Clear();
	m_xRowTrackRects.Clear();
	m_xKeyRects.Clear();
	m_xEventRects.Clear();
	m_bRulerRectValid = false;
	m_bPlayheadRectValid = false;
	m_bTrackAreaRectValid = false;
	m_bCanvasRectValid = false;
	m_bPreviewImageRectValid = false;

	// The display bound goes with them: a frame that recorded nothing must not
	// leave a bound behind that the next query would judge a stale rect against.
	m_fRecordedDisplayWidth = 0.0f;
	m_fRecordedDisplayHeight = 0.0f;
	m_fLastTrackWidth = 0.0f;
}

bool Zenith_EditorPanel_Animation::PublishRect(const Zenith_AnimPanelRect* pxRect, Zenith_AnimPanelRect& xOut) const
{
	if (pxRect == nullptr)
	{
		return false;
	}
	// ★ THE BOUND IS THE ONE CAPTURED AT RECORD TIME, NEVER ImGui::GetIO() HERE.
	//
	// This function used to re-read io.DisplaySize, which reads correctly in the
	// editor (queries happen mid-frame) and is wrong everywhere else. ImGui
	// initialises DisplaySize to (-1, -1) and only a backend NewFrame fills it
	// in, so ANY query made outside a live frame — which is every unit assertion,
	// since a test's frame has to be closed before it can inspect the result —
	// compared each centre against -1 and refused it. Five units reported
	// "nothing was published" while the panel was drawing perfectly: the expected
	// x they printed (457) was computed from the panel's own live view, so the
	// layout was provably correct and only the gate was lying.
	const Zenith_Maths::Vector2 xCentre = pxRect->Centre();
	if (!(xCentre.x >= 0.0f && xCentre.y >= 0.0f
	   && xCentre.x <= m_fRecordedDisplayWidth && xCentre.y <= m_fRecordedDisplayHeight))
	{
		return false;
	}
	xOut = *pxRect;
	return true;
}

bool Zenith_EditorPanel_Animation::RowRectFor(const Zenith_AnimTrackId& xTrack, bool bTrackLaneOnly, Zenith_AnimPanelRect& xOut) const
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (!FindRowIndexForTrack(xTrack, uRow))
	{
		return false;
	}
	return bTrackLaneOnly ? GetRowTrackRectByIndex(uRow, xOut) : GetRowRectByIndex(uRow, xOut);
}

bool Zenith_EditorPanel_Animation::GetRowRect(const Zenith_AnimTrackId& xTrack, Zenith_AnimPanelRect& xOut) const
{
	return RowRectFor(xTrack, /*bTrackLaneOnly*/ false, xOut);
}

bool Zenith_EditorPanel_Animation::GetRowTrackRect(const Zenith_AnimTrackId& xTrack, Zenith_AnimPanelRect& xOut) const
{
	return RowRectFor(xTrack, /*bTrackLaneOnly*/ true, xOut);
}

bool Zenith_EditorPanel_Animation::GetRowRectByIndex(u_int uRowIndex, Zenith_AnimPanelRect& xOut) const
{
	return PublishRect(m_xRowRects.TryGet(uRowIndex), xOut);
}

bool Zenith_EditorPanel_Animation::GetRowTrackRectByIndex(u_int uRowIndex, Zenith_AnimPanelRect& xOut) const
{
	return PublishRect(m_xRowTrackRects.TryGet(uRowIndex), xOut);
}

bool Zenith_EditorPanel_Animation::GetKeyRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimPanelRect& xOut) const
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (!FindRowIndexForTrack(xTrack, uRow))
	{
		return false;
	}
	return PublishRect(m_xKeyRects.TryGet(MakeKeyRectKey(uRow, uKeyId)), xOut);
}

bool Zenith_EditorPanel_Animation::GetEventRect(u_int uEventId, Zenith_AnimPanelRect& xOut) const
{
	return PublishRect(m_xEventRects.TryGet(uEventId), xOut);
}

bool Zenith_EditorPanel_Animation::GetEventsRowRect(Zenith_AnimPanelRect& xOut) const
{
	if (m_uEventsRowIndex == uINVALID_ANIM_SHEET_ROW)
	{
		return false;
	}
	return GetRowRectByIndex(m_uEventsRowIndex, xOut);
}

bool Zenith_EditorPanel_Animation::GetPlayheadRect(Zenith_AnimPanelRect& xOut) const
{
	return m_bPlayheadRectValid ? PublishRect(&m_xPlayheadRect, xOut) : false;
}

bool Zenith_EditorPanel_Animation::GetRulerRect(Zenith_AnimPanelRect& xOut) const
{
	return m_bRulerRectValid ? PublishRect(&m_xRulerRect, xOut) : false;
}

bool Zenith_EditorPanel_Animation::GetTrackAreaRect(Zenith_AnimPanelRect& xOut) const
{
	return m_bTrackAreaRectValid ? PublishRect(&m_xTrackAreaRect, xOut) : false;
}

bool Zenith_EditorPanel_Animation::GetPreviewImageRect(Zenith_AnimPanelRect& xOut) const
{
	return m_bPreviewImageRectValid ? PublishRect(&m_xPreviewImageRect, xOut) : false;
}

//=============================================================================
// POSE AUTHORING (Phase 4) — the preview camera as pure maths, bone selection,
// and the declarations WU-4.3 / WU-4.4 fill.
//=============================================================================

bool Zenith_EditorPanel_Animation::GetPreviewViewProj(
	Zenith_Maths::Matrix4& xOutView, Zenith_Maths::Matrix4& xOutProj) const
{
	if (!m_xSession.IsOpen())
	{
		return false;
	}

	float fYaw = 0.0f;
	float fPitch = 0.0f;
	float fDistance = 0.0f;
	m_xSession.GetCameraOrbit(fYaw, fPitch, fDistance);

	// ★ THE SAME PURE BUILDER THE SESSION STAGES THE SLOT WITH. Deriving a second
	// view matrix here would make "where the panel thinks the bone is" and "where
	// the renderer drew it" two numbers that agree only until one of them is
	// touched — and the disagreement would present as picking being slightly off,
	// which is indistinguishable from a bad hit radius.
	Flux_ViewConstants xConstants;
	Flux_PreviewBuildViewConstants(fYaw, fPitch, fDistance, xConstants);
	xOutView = xConstants.m_xViewMat;
	xOutProj = xConstants.m_xProjMat;
	return true;
}

bool Zenith_EditorPanel_Animation::ProjectPreviewWorldPoint(
	const Zenith_Maths::Vector3& xWorld, float& fOutPixelX, float& fOutPixelY) const
{
	Zenith_Maths::Matrix4 xView(1.0f);
	Zenith_Maths::Matrix4 xProj(1.0f);
	if (!GetPreviewViewProj(xView, xProj) || !m_bPreviewImageRectValid)
	{
		return false;
	}

	const float fWidth = m_xPreviewImageRect.Width();
	const float fHeight = m_xPreviewImageRect.Height();
	if (fWidth <= 0.0f || fHeight <= 0.0f)
	{
		return false;
	}

	const Zenith_Maths::Vector4 xClip = (xProj * xView) * Zenith_Maths::Vector4(xWorld, 1.0f);
	if (xClip.w <= 1.0e-6f)
	{
		// Behind the camera (or on the plane): there is no pixel, and returning
		// one anyway would be a mirrored coordinate somewhere on screen.
		return false;
	}

	// ★ THE EXACT INVERSE OF Zenith_Gizmo::ScreenToWorldRay's NDC STEP, Y
	// INCLUDED. That function deliberately does NOT flip Y — the preview's
	// projection already carries the Vulkan flip — so neither does this. Adding a
	// flip on one side only is how a pick lands on the mirror image of the bone
	// the user aimed at, which looks like an off-by-a-bone selection bug.
	fOutPixelX = (xClip.x / xClip.w * 0.5f + 0.5f) * fWidth;
	fOutPixelY = (xClip.y / xClip.w * 0.5f + 0.5f) * fHeight;
	return true;
}

bool Zenith_EditorPanel_Animation::BuildPreviewRay(float fPixelX, float fPixelY,
	Zenith_Maths::Vector3& xOutOrigin, Zenith_Maths::Vector3& xOutDir) const
{
	Zenith_Maths::Matrix4 xView(1.0f);
	Zenith_Maths::Matrix4 xProj(1.0f);
	if (!GetPreviewViewProj(xView, xProj) || !m_bPreviewImageRectValid)
	{
		return false;
	}

	const float fWidth = m_xPreviewImageRect.Width();
	const float fHeight = m_xPreviewImageRect.Height();
	if (fWidth <= 0.0f || fHeight <= 0.0f)
	{
		return false;
	}

	// Stateless helper; the object exists only because ScreenToWorldRay is a
	// member function.
	Zenith_Gizmo xGizmo;
	xOutDir = xGizmo.ScreenToWorldRay(
		Zenith_Maths::Vector2(fPixelX, fPixelY),
		Zenith_Maths::Vector2(0.0f, 0.0f),
		Zenith_Maths::Vector2(fWidth, fHeight),
		xView, xProj);

	float fYaw = 0.0f;
	float fPitch = 0.0f;
	float fDistance = 0.0f;
	m_xSession.GetCameraOrbit(fYaw, fPitch, fDistance);
	xOutOrigin = Flux_PreviewOrbitCameraPos(fYaw, fPitch, fDistance);
	return true;
}

//-----------------------------------------------------------------------------
// Bone selection — WU-4.1's own three verbs.
//-----------------------------------------------------------------------------

bool Zenith_EditorPanel_Animation::Action_SelectBone(u_int uBoneIndex)
{
	if (!m_xSession.IsOpen())
	{
		return false;
	}
	m_xSession.SelectBone(uBoneIndex);
	// The session clears rather than storing an index that does not resolve, so
	// this is also the range check's answer.
	return m_xSession.HasBoneSelection();
}

bool Zenith_EditorPanel_Animation::Action_ClearBoneSelection()
{
	if (!m_xSession.HasBoneSelection())
	{
		return false;
	}
	m_xSession.ClearBoneSelection();
	return true;
}

bool Zenith_EditorPanel_Animation::Action_PickBoneAtPreviewPixel(float fPixelX, float fPixelY)
{
	Zenith_Maths::Vector3 xOrigin(0.0f);
	Zenith_Maths::Vector3 xDir(0.0f);
	if (!BuildPreviewRay(fPixelX, fPixelY, xOrigin, xDir))
	{
		return false;
	}

	u_int uBone = kuINVALID_BONE_SELECTION;
	if (!m_xSession.PickBone(xOrigin, xDir, uBone))
	{
		// ★ A MISS CHANGES NOTHING. Clicking the empty space beside a bone is not
		// a request to deselect it — that is Action_ClearBoneSelection, which the
		// caller can decide to make.
		return false;
	}

	m_xSession.SelectBone(uBone);
	return m_xSession.HasBoneSelection();
}

//-----------------------------------------------------------------------------
// Declared here, filled elsewhere.
//
// WU-4.3's half — Set Key, auto-key, the angle snap, the one-shot rotate and
// the whole pointer drag — now lives in Zenith_EditorPanel_Animation_Pose.cpp,
// beside the manipulator that drives it. What is left here is WU-4.4's, and it
// still returns false and names its owner so a caller wired up early gets a
// refusal rather than a silent success.
//-----------------------------------------------------------------------------

// Action_BakeIKForSelectedChain is FILLED in its own TU:
// Zenith_EditorPanel_Animation_IK.cpp (WU-4.4). It solves a TRANSIENT chain on a
// SCRATCH pose (never the controller's own, which would run IK twice on the same
// pose) and bakes the result down through Action_SetKeyForBones.

//=============================================================================
// Events (WU-5B) — the inspector target and the emitted-event strip.
//=============================================================================

u_int Zenith_EditorPanel_Animation::GetInspectorEventId() const
{
	u_int uEventId = uINVALID_ANIM_KEY_ID;
	return ResolvePrimarySelectedEvent(uEventId) ? uEventId : uINVALID_ANIM_KEY_ID;
}

void Zenith_EditorPanel_Animation::OnPreviewEventEmitted(void* pUserData, const std::string& strEventName,
	const Zenith_Maths::Vector4& xData)
{
	// The payload is deliberately not kept: the strip answers "did that fire?"
	// beside a moving playhead, and four floats per row would push the names —
	// the only part a reader is scanning for — off the end of the toolbar.
	(void)xData;
	Zenith_EditorPanel_Animation* pxPanel = static_cast<Zenith_EditorPanel_Animation*>(pUserData);
	if (pxPanel == nullptr)
	{
		return;
	}
	pxPanel->PushEmittedEventName(strEventName);
}

void Zenith_EditorPanel_Animation::PushEmittedEventName(const std::string& strName)
{
	// ★ THE TOTAL IS COUNTED BEFORE THE RING IS TRIMMED. A scrub across a dense
	// clip can fire more events than the strip holds, and a test that could only
	// see the survivors would report "3 fired" for a burst of eleven.
	++m_uEmittedEventTotal;

	m_astrEmittedEvents.PushBack(strName);
	while (m_astrEmittedEvents.GetSize() > uANIM_EMITTED_EVENT_HISTORY)
	{
		m_astrEmittedEvents.Remove(0u);
	}
}

bool Zenith_EditorPanel_Animation::GetEmittedEventNameAt(u_int uIndex, std::string& strOut) const
{
	// Index 0 is the MOST RECENT — the vector's tail. A reader scanning the strip
	// wants the newest first, and reversing here keeps the push path a plain
	// append.
	if (uIndex >= m_astrEmittedEvents.GetSize())
	{
		return false;
	}
	strOut = m_astrEmittedEvents.Get(m_astrEmittedEvents.GetSize() - 1u - uIndex);
	return true;
}

void Zenith_EditorPanel_Animation::ClearEmittedEvents()
{
	m_astrEmittedEvents.Clear();
	m_uEmittedEventTotal = 0u;
}

#ifdef ZENITH_TESTING
#include "Zenith_EditorPanel_Animation.Tests.inl"
#endif

#endif // ZENITH_TOOLS
