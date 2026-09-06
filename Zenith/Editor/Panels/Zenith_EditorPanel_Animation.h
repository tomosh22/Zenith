#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimationDocument.h"
#include "Editor/Zenith_AnimationPreviewSession.h"
#include "Editor/Zenith_AnimTimelineMath.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_HashSet.h"
#include "Maths/Zenith_Maths.h"
#include <string>

// Forward-declared rather than included: this header is pulled into
// Zenith_Editor.cpp and the panel's own two TUs, and none of its DECLARATIONS
// need an ImGui type. imgui.h forward-declares this struct the same way.
struct ImDrawList;

//=============================================================================
// Zenith_EditorPanel_Animation (WU-3.2) — the DOPE SHEET.
//
// One dockable window over ONE Zenith_AnimationDocument and ONE
// Zenith_AnimationPreviewSession: a ruler, a row per bone track (T/R/S), the
// two root-motion tracks, an events row, a playhead, and the shared preview
// image. WU-3.2 RENDERS AND HIT-TESTS ONLY — every mutation, selection and
// drag is WU-3.3, which addresses the panel through the rect accessors below.
//
// ★ IT IS A CLASS, NOT A PILE OF FILE STATICS, AND THAT IS THE ONE THING IT
// DOES NOT COPY FROM THE GRAPH EDITOR. That panel keeps its whole state in a
// single file-scope aggregate, which is exactly why it can hold ONE asset open
// and has no undo: neither limitation is a decision anybody made, they are both
// consequences of the storage. Everything here — the document, the session, the
// view, the row list and every rect map — is a member, so a second dope sheet is
// a second object rather than a rewrite. Instance() is the editor's single one.
//
// ★ DRAW-LIST DECORATIONS, NOT ITEMS (Editor/CLAUDE.md). The sheet is ONE
// InvisibleButton with everything — row backgrounds, labels, ticks, keys,
// events, the playhead — painted into the window's ImDrawList at absolute
// screen coordinates. Placing items with SetCursorScreenPos and restoring the
// cursor trips ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a
// windowed build is a modal CRT dialog nothing logs: the process simply hangs.
//
// ★ NO COORDINATE MATHS LIVES HERE. Every seconds<->pixels conversion goes
// through Zenith_AnimTimelineMath (WU-3.1) — the panel owns a
// Zenith_AnimTimelineView, fills in its track rect each frame, clamps it, and
// asks. A key's centre x IS Zenith_AnimTimelineTimeToPixel(View(), t), which is
// what makes the hit-rects checkable against the pure functions' own units.
//=============================================================================

//-----------------------------------------------------------------------------
// A rect in ABSOLUTE ImGui screen coordinates — the same space
// ImGui::GetItemRectMin/Max and Zenith_InputSimulator's mouse position live in,
// so a recorded rect can be clicked without a further transform.
//-----------------------------------------------------------------------------
struct Zenith_AnimPanelRect
{
	float m_fMinX = 0.0f;
	float m_fMinY = 0.0f;
	float m_fMaxX = 0.0f;
	float m_fMaxY = 0.0f;

	Zenith_Maths::Vector2 Centre() const
	{
		return Zenith_Maths::Vector2((m_fMinX + m_fMaxX) * 0.5f, (m_fMinY + m_fMaxY) * 0.5f);
	}
	float Width() const { return m_fMaxX - m_fMinX; }
	float Height() const { return m_fMaxY - m_fMinY; }
};

//-----------------------------------------------------------------------------
// What one row of the sheet IS. A dope sheet is a flat list of rows and a
// GROUPED list of tracks at the same time, so the row list carries both: the
// kind says how to draw it, m_xTrack says what it addresses, and
// m_uGroupRowIndex says which header collapses it.
//-----------------------------------------------------------------------------
enum Zenith_AnimSheetRowKind : u_int
{
	// A bone's name. Collapsing it hides its three track rows.
	ZENITH_ANIMSHEET_ROW_BONE_HEADER,
	// One of that bone's Translation / Rotation / Scale tracks.
	ZENITH_ANIMSHEET_ROW_BONE_TRACK,
	ZENITH_ANIMSHEET_ROW_ROOT_MOTION_HEADER,
	// Root motion's position or rotation delta track. There is no scale track
	// (D16), so this group has exactly TWO rows, never three.
	ZENITH_ANIMSHEET_ROW_ROOT_MOTION_TRACK,
	// The clip's animation events, always the last row.
	ZENITH_ANIMSHEET_ROW_EVENTS,
};

constexpr u_int uINVALID_ANIM_SHEET_ROW = 0xFFFFFFFFu;

struct Zenith_AnimSheetRow
{
	Zenith_AnimSheetRowKind m_eKind = ZENITH_ANIMSHEET_ROW_EVENTS;
	// Meaningful only when m_bHasTrack.
	Zenith_AnimTrackId m_xTrack;
	bool m_bHasTrack = false;
	std::string m_strLabel;
	// The header row this row collapses under, or uINVALID_ANIM_SHEET_ROW.
	u_int m_uGroupRowIndex = uINVALID_ANIM_SHEET_ROW;
	// The key that addresses this row's collapse state in m_xCollapsedGroups.
	std::string m_strGroupKey;
};

//=============================================================================
// The panel.
//=============================================================================
class Zenith_EditorPanel_Animation
{
public:
	Zenith_EditorPanel_Animation();
	~Zenith_EditorPanel_Animation();

	// ★ NON-COPYABLE, for the same reason its two members are: the document's
	// undo commands hold a raw pointer back at it, and the session's preview-slot
	// claim is keyed on its own address.
	Zenith_EditorPanel_Animation(const Zenith_EditorPanel_Animation&) = delete;
	Zenith_EditorPanel_Animation& operator=(const Zenith_EditorPanel_Animation&) = delete;

	// The editor's single panel. A function-local static rather than a file-scope
	// object so nothing can reach the state except through the object, and so a
	// unit can build its OWN panel on the stack and drive it in isolation.
	static Zenith_EditorPanel_Animation& Instance();

	//-------------------------------------------------------------------------
	// Frame
	//-------------------------------------------------------------------------

	// Composes the window. fDtSeconds advances the preview session when it is
	// playing; the CALLER decides what that is, which is how the editor's Paused
	// mode is honoured (Zenith_Editor::Render passes 0 while paused) without the
	// panel reaching for editor state it does not otherwise need.
	//
	// Every rect map is cleared at the top of this, whether or not the window is
	// drawn, so a hidden / collapsed / unselected-tab panel reports NO rects
	// rather than last frame's.
	void Render(float fDtSeconds);

	// Drops the document and the session while the asset registry is still up.
	// Called from Zenith_Editor::Shutdown, which runs BEFORE
	// Zenith_AssetRegistry::Shutdown — a panel that waited for its own static
	// destructor would be releasing owning asset handles into a registry that had
	// already force-deleted them.
	void Shutdown();

	bool& ShowFlag() { return m_bShow; }
	bool IsShown() const { return m_bShow; }

	//-------------------------------------------------------------------------
	// Document lifecycle
	//-------------------------------------------------------------------------

	// Opens strAssetPath into the document, opens the preview session over the
	// resulting clip, rebuilds the rows and frames the whole clip. Returns false
	// on any refusal; the reason is kept in GetLastOpenResult() — in particular
	// ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED, which is what makes the toolbar
	// offer "Promote to authored override" (D21).
	bool OpenClip(const std::string& strAssetPath);

	// D21's escape hatch, wired to that offer.
	bool PromoteAndOpenAuthoredOverride(const std::string& strSourceAssetPath);

	// FORCED close: unsaved edits are discarded. This is what Shutdown and the
	// tests use.
	void CloseClip();

	// The Close BUTTON's route: refuses while dirty and leaves everything open so
	// the panel can raise the Save / Discard prompt.
	Zenith_AnimDocCloseResult RequestCloseClip();

	bool IsOpen() const { return m_xDocument.IsOpen(); }

	// The result of the LAST OpenClip / PromoteAndOpenAuthoredOverride attempt.
	// Defaults to ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET — "nothing has been opened".
	Zenith_AnimDocOpenResult GetLastOpenResult() const { return m_eLastOpenResult; }
	const std::string& GetLastOpenAttemptPath() const { return m_strLastOpenAttemptPath; }

	Zenith_AnimationDocument& Document() { return m_xDocument; }
	const Zenith_AnimationDocument& Document() const { return m_xDocument; }
	Zenith_AnimationPreviewSession& Session() { return m_xSession; }
	const Zenith_AnimationPreviewSession& Session() const { return m_xSession; }

	// WU-3.3 calls this after any document mutation so the session re-copies the
	// clip. Render also detects an edit from the undo/redo depths, but a depth is
	// a heuristic (an edit made with the undo stack already at its cap and the
	// redo stack empty moves neither number) and this is the exact signal.
	void NotifyDocumentEdited() { m_bClipRefreshPending = true; }

	//-------------------------------------------------------------------------
	// View
	//-------------------------------------------------------------------------

	// The live view. m_fTrackLeftPixel / m_fTrackWidthPixels are OVERWRITTEN by
	// every Render from the canvas geometry; the zoom and the scroll are the
	// panel's own state and survive.
	const Zenith_AnimTimelineView& View() const { return m_xView; }
	void SetView(const Zenith_AnimTimelineView& xView) { m_xView = xView; }

	// The clip's AUTHORED frame rate (what the grid snaps to), or 0 — meaning
	// "unsnapped" to Zenith_AnimTimelineSnapToFrame — when nothing is open.
	u_int GetFrameRate() const;

	// Both are REQUESTS applied by the NEXT Render, exactly like the graph
	// editor's ScrollPaletteEntryIntoView and for the same reason: the scroll
	// they need is a function of the track rect, and the track rect is not known
	// until the window has been laid out. Give one a frame before reading a rect.
	void ScrollTimeIntoView(float fTimeSeconds);
	// False when the track has no row — a closed document, or a bone the clip
	// does not have. A collapsed group is EXPANDED rather than refused.
	bool ScrollRowIntoView(const Zenith_AnimTrackId& xTrack);

	// Places the window deterministically on the next Render (position, size and
	// an expanded state). What "open the dope sheet" uses, and what a unit uses
	// to get a known geometry regardless of any saved imgui.ini.
	void RequestWindowPlacement(float fScreenX, float fScreenY, float fWidth, float fHeight);

	//-------------------------------------------------------------------------
	// Rows
	//-------------------------------------------------------------------------

	u_int GetRowCount() const { return m_axRows.GetSize(); }
	bool GetRowAt(u_int uRowIndex, Zenith_AnimSheetRow& xOut) const;
	bool FindRowIndexForTrack(const Zenith_AnimTrackId& xTrack, u_int& uOutRowIndex) const;
	u_int GetEventsRowIndex() const { return m_uEventsRowIndex; }

	bool IsGroupCollapsed(const std::string& strGroupKey) const;
	void SetGroupCollapsed(const std::string& strGroupKey, bool bCollapsed);

	//-------------------------------------------------------------------------
	// D13 — keys (and events) sitting past the clip's duration.
	//
	// Shrinking a duration does not move a key, so a clip can legally hold keys
	// nothing will ever sample. The sheet paints a warning glyph on each one and
	// a banner over the whole panel; these are what a unit reads.
	//-------------------------------------------------------------------------

	u_int GetKeysPastDurationCount() const { return m_uKeysPastDuration; }
	u_int GetEventsPastDurationCount() const { return m_uEventsPastDuration; }
	bool IsKeyPastDuration(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const;

	//-------------------------------------------------------------------------
	// Hit rects.
	//
	// ★ EVERY ONE OF THESE RETURNS FALSE FOR SOMETHING THAT IS NOT ON SCREEN,
	// and that is the load-bearing half of the graph editor's hard-won contract
	// (Editor/CLAUDE.md). Handing out the coordinate of a key scrolled past the
	// right edge of the track produces a click into empty space, and the failure
	// then surfaces far away as "the key did not move" — with the click, the
	// input bridge and the panel all healthy and none of them at fault. A rect is
	// recorded only when it was actually painted inside the canvas this frame,
	// and is only handed out when its centre is inside the display.
	//
	// ★ "THE DISPLAY" IS THE ONE CAPTURED WHEN THE RECT WAS RECORDED, not the one
	// live at the moment of the query, and that distinction is load-bearing. A
	// rect is a fact about a FRAME; the bound it has to be judged against is the
	// display size of THAT frame. Re-reading ImGui's live io.DisplaySize here
	// compares this frame's coordinate against whatever the display happens to be
	// now — which is a different number after a window resize, and is ImGui's
	// uninitialised default of (-1, -1) for any query made outside a frame. The
	// second case is not hypothetical: it made all five rendering units report
	// "nothing was published" while the panel was drawing perfectly.
	//
	// So: query them AFTER a rendered frame, and call ScrollTimeIntoView /
	// ScrollRowIntoView (and give them a frame) before expecting one to resolve.
	//-------------------------------------------------------------------------

	// The whole row, label column included.
	bool GetRowRect(const Zenith_AnimTrackId& xTrack, Zenith_AnimPanelRect& xOut) const;
	bool GetRowRectByIndex(u_int uRowIndex, Zenith_AnimPanelRect& xOut) const;
	// Just the KEY LANE of the row — the label column excluded. What a click on a
	// key, or a rubber band over a time range, hit-tests against.
	bool GetRowTrackRect(const Zenith_AnimTrackId& xTrack, Zenith_AnimPanelRect& xOut) const;
	bool GetRowTrackRectByIndex(u_int uRowIndex, Zenith_AnimPanelRect& xOut) const;

	bool GetKeyRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimPanelRect& xOut) const;
	bool GetEventRect(u_int uEventId, Zenith_AnimPanelRect& xOut) const;
	bool GetEventsRowRect(Zenith_AnimPanelRect& xOut) const;
	bool GetPlayheadRect(Zenith_AnimPanelRect& xOut) const;
	bool GetRulerRect(Zenith_AnimPanelRect& xOut) const;
	// The full key lane across every row — the region the time mapping covers.
	bool GetTrackAreaRect(Zenith_AnimPanelRect& xOut) const;

	//-------------------------------------------------------------------------
	// External modification (read-only display).
	//
	// Costs one file read, so it is taken on the window's FOCUS TRANSITION rather
	// than per frame; a caller that needs it right now says so.
	//-------------------------------------------------------------------------
	void RefreshExternalModificationState();
	bool HasExternalConflict() const { return m_bExternalConflict; }

	//-------------------------------------------------------------------------
	// Diagnostics — DELIBERATELY UNGATED, and that is the whole point of them.
	//
	// Every accessor above answers a flat `false` for four completely different
	// situations: the window was never drawn, the sheet had no room, the thing
	// asked for was scrolled away, or the rect fell outside the display. A test
	// that only sees `false` reports "nothing was published" and says nothing
	// about which of the four it hit — which is exactly what happened, and cost a
	// build-and-run cycle to tell "the panel is not drawing" apart from "the
	// panel is drawing and the gate is wrong". These four are the discriminator,
	// so a failure names its own cause.
	//-------------------------------------------------------------------------
	bool WasSheetDrawnLastFrame() const { return m_bCanvasRectValid; }
	// The KEY LANE's width in pixels as of the last sheet pass. Zero means every
	// key and every row rect was culled before it was ever recorded.
	float GetLastTrackWidth() const { return m_fLastTrackWidth; }
	// The display bound the last recorded rects are judged against (see the
	// hit-rect note above). Zero or negative means every publish will refuse.
	float GetRecordedDisplayWidth() const { return m_fRecordedDisplayWidth; }
	float GetRecordedDisplayHeight() const { return m_fRecordedDisplayHeight; }

	// How many frames this panel has actually drawn its window. Zero means every
	// rect accessor is answering "not rendered", not "off screen".
	u_int GetRenderedFrameCount() const { return m_uRenderedFrames; }

private:
	//-------------------------------------------------------------------------
	// Per-frame sheet geometry, in absolute screen pixels. Computed ONCE at the
	// top of the sheet pass and handed to every helper, so no helper re-derives a
	// coordinate and no two of them can disagree about where a row starts.
	//-------------------------------------------------------------------------
	struct SheetLayout
	{
		float m_fCanvasLeft = 0.0f;
		float m_fCanvasTop = 0.0f;
		float m_fCanvasRight = 0.0f;
		float m_fCanvasBottom = 0.0f;
		float m_fLabelWidth = 0.0f;
		float m_fTrackLeft = 0.0f;
		float m_fTrackWidth = 0.0f;
		float m_fRulerHeight = 0.0f;
		float m_fRowsTop = 0.0f;
		float m_fRowHeight = 0.0f;
	};

	static std::string MakeTrackKey(const Zenith_AnimTrackId& xTrack);
	static u_int64 MakeKeyRectKey(u_int uRowIndex, u_int uKeyId);

	void ClearFrameRects();
	void RebuildRows();
	void RecountKeysPastDuration();
	void OnDocumentOpened();
	void SyncSessionWithDocument();

	// The off-screen gate every accessor above runs through.
	bool PublishRect(const Zenith_AnimPanelRect* pxRect, Zenith_AnimPanelRect& xOut) const;
	bool RowRectFor(const Zenith_AnimTrackId& xTrack, bool bTrackLaneOnly, Zenith_AnimPanelRect& xOut) const;

	// Render helpers — all in Zenith_EditorPanel_Animation_Render.cpp.
	void RenderToolbar();
	void RenderBanners();
	void RenderPreviewPane();
	void RenderSheet();
	void HandleViewInput(const SheetLayout& xLayout, bool bCanvasHovered);
	void ApplyPendingScrolls(const SheetLayout& xLayout);
	void DrawSheetBackground(ImDrawList* pxDraw, const SheetLayout& xLayout);
	void DrawRuler(ImDrawList* pxDraw, const SheetLayout& xLayout);
	void DrawRows(ImDrawList* pxDraw, const SheetLayout& xLayout, bool bCanvasHovered);
	void DrawKeysForRow(ImDrawList* pxDraw, const SheetLayout& xLayout, u_int uRowIndex, float fRowTop);
	void DrawEventsForRow(ImDrawList* pxDraw, const SheetLayout& xLayout, float fRowTop);
	void DrawPlayhead(ImDrawList* pxDraw, const SheetLayout& xLayout);

	float TotalRowsHeight(const SheetLayout& xLayout) const;
	float VisibleRowsHeight(const SheetLayout& xLayout) const;

	bool m_bShow = false;

	Zenith_AnimationDocument m_xDocument;
	Zenith_AnimationPreviewSession m_xSession;
	Zenith_AnimTimelineView m_xView;

	Zenith_Vector<Zenith_AnimSheetRow> m_axRows;
	Zenith_HashMap<std::string, u_int> m_xRowIndexByTrackKey;
	Zenith_HashSet<std::string> m_xCollapsedGroups;
	u_int m_uEventsRowIndex = uINVALID_ANIM_SHEET_ROW;

	// Live rects. CLEARED AT THE TOP OF EVERY Render and repopulated by the draw,
	// so a rect in here was painted this frame by construction.
	Zenith_HashMap<u_int, Zenith_AnimPanelRect> m_xRowRects;
	Zenith_HashMap<u_int, Zenith_AnimPanelRect> m_xRowTrackRects;
	Zenith_HashMap<u_int64, Zenith_AnimPanelRect> m_xKeyRects;
	Zenith_HashMap<u_int, Zenith_AnimPanelRect> m_xEventRects;
	Zenith_AnimPanelRect m_xRulerRect;
	Zenith_AnimPanelRect m_xPlayheadRect;
	Zenith_AnimPanelRect m_xTrackAreaRect;
	Zenith_AnimPanelRect m_xCanvasRect;
	bool m_bRulerRectValid = false;
	bool m_bPlayheadRectValid = false;
	bool m_bTrackAreaRectValid = false;
	bool m_bCanvasRectValid = false;

	// ★ The display bound CAPTURED WHEN THE RECTS ABOVE WERE RECORDED. Zeroed by
	// ClearFrameRects with them, so a frame the panel did not draw refuses every
	// publish on this alone. See the hit-rect note in the public section.
	float m_fRecordedDisplayWidth = 0.0f;
	float m_fRecordedDisplayHeight = 0.0f;
	// Diagnostic only — the key lane's width as of the last sheet pass.
	float m_fLastTrackWidth = 0.0f;

	// D13 bookkeeping — the same (row, key) composite the rect map uses.
	Zenith_HashSet<u_int64> m_xKeysPastDuration;
	u_int m_uKeysPastDuration = 0;
	u_int m_uEventsPastDuration = 0;

	// ★ "Fit the clip to the window" DEFERRED UNTIL THE TRACK RECT IS KNOWN.
	// Zenith_AnimTimelineFrameAll needs a width to divide by, and at the moment a
	// clip is opened the panel has not been laid out yet — so calling it there
	// hits its own "no track to fit into" fallback and silently leaves the zoom at
	// fANIM_TIMELINE_DEFAULT_PPS. Clamp does NOT finish the job: it sanitises a
	// view, it does not fit one. Consumed by the first sheet pass after an open.
	bool m_bPendingFrameAll = false;

	// Deferred view requests (see ScrollTimeIntoView).
	bool m_bPendingTimeScroll = false;
	float m_fPendingTimeScroll = 0.0f;
	bool m_bPendingRowScroll = false;
	u_int m_uPendingRowScroll = uINVALID_ANIM_SHEET_ROW;
	float m_fRowScrollPixels = 0.0f;

	bool m_bPlacementRequested = false;
	float m_fPlacementX = 0.0f;
	float m_fPlacementY = 0.0f;
	float m_fPlacementWidth = 0.0f;
	float m_fPlacementHeight = 0.0f;

	Zenith_AnimDocOpenResult m_eLastOpenResult = ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET;
	std::string m_strLastOpenAttemptPath;

	// Toolbar text fields. Fixed buffers because ImGui::InputText wants one.
	char m_acPathBuffer[512] = {};
	char m_acSkeletonBuffer[512] = {};
	char m_acPreviewModelBuffer[512] = {};

	bool m_bExternalConflict = false;
	bool m_bWasFocused = false;
	bool m_bCloseRefusedDirty = false;

	// Edit detection for the session's clip refresh (see NotifyDocumentEdited).
	u_int m_uSeenUndoDepth = 0;
	u_int m_uSeenRedoDepth = 0;
	bool m_bClipRefreshPending = false;

	// ★ The ImGui registration of the SHARED preview LDR, kept as the raw handle
	// value so this header does not have to pull Flux in. Registered once and
	// NEVER unregistered: the panel outlives the frame loop, and unregistering
	// from a static destructor would be a descriptor write after the backend has
	// gone. Same treatment, same reason, as the material editor's.
	u_int64 m_ulPreviewImageHandle = 0;
	bool m_bPreviewImageRegistered = false;

	u_int m_uRenderedFrames = 0;
};

#endif // ZENITH_TOOLS
