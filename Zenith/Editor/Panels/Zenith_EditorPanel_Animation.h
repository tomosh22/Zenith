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
// Zenith_EditorPanel_Animation (WU-3.2 / WU-3.3) — the DOPE SHEET.
//
// One dockable window over ONE Zenith_AnimationDocument and ONE
// Zenith_AnimationPreviewSession: a ruler, a row per bone track (T/R/S), the
// two root-motion tracks, an events row, a playhead, and the shared preview
// image. WU-3.2 built the drawing and the hit rects; WU-3.3 added SELECTION and
// the OPERATIONS — see the "OPERATIONS" block below, and its three rules: every
// gesture has a bool-returning Action_* twin that reads no ImGui state, every
// mutation goes through a document verb, and a multi-key operation is ONE undo
// step (Zenith_AnimationDocument::BeginCompound).
//
// The class is spread over THREE TUs, split by what a reader wants separately:
//   Zenith_EditorPanel_Animation.cpp        — lifecycle, rows, the hit rects
//   Zenith_EditorPanel_Animation_Render.cpp — the drawing, and input TRANSLATION
//   Zenith_EditorPanel_Animation_Ops.cpp    — selection and the operations,
//                                             with not one line of ImGui in it
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

//-----------------------------------------------------------------------------
// How a select gesture combines with what is already selected. Three values
// rather than two bools, because "toggle" and "add" are different answers to
// the same click and a pair of flags would allow the meaningless fourth.
//-----------------------------------------------------------------------------
enum Zenith_AnimSelectMode : u_int
{
	// A plain click: this and nothing else.
	ZENITH_ANIMSELECT_REPLACE,
	// Ctrl-click: in if it was out, out if it was in.
	ZENITH_ANIMSELECT_TOGGLE,
	// Shift-click: in, and leave everything else in.
	ZENITH_ANIMSELECT_ADD,
};

//-----------------------------------------------------------------------------
// One selected key.
//
// ★ (TRACK, STABLE ID) AND NEVER AN INDEX (D24). A retime REORDERS a track, so
// an index-based selection starts naming a different key mid-drag with nothing
// to observe; and a selection has to survive an undo, which re-inserts a key
// under its ORIGINAL id precisely so that this keeps resolving.
//-----------------------------------------------------------------------------
struct Zenith_AnimSelectedKey
{
	Zenith_AnimTrackId m_xTrack;
	u_int m_uKeyId = uINVALID_ANIM_KEY_ID;
};

//-----------------------------------------------------------------------------
// One key on the panel's clipboard.
//
// ★ IT STORES A TRACK KIND, NOT A TRACK. The whole point of the copy buffer is
// cross-bone paste: what survives the trip is "this was a rotation key, this far
// into the copied span, with this value", and the BONE is supplied by whoever
// pastes. Times are RELATIVE to the earliest key in the copy, so a paste offset
// is a plain addition and the internal spacing is preserved exactly.
//-----------------------------------------------------------------------------
struct Zenith_AnimClipboardKey
{
	Flux_AnimTrack m_eTrack = FLUX_ANIM_TRACK_POSITION;
	// Root-motion keys paste back onto ROOT MOTION whatever bone is named — the
	// bone is meaningless for them, and dropping them would make a paste silently
	// lose part of what was copied.
	bool m_bRootMotion = false;
	float m_fRelativeTimeSeconds = 0.0f;
	Zenith_AnimKeyValue m_xValue;
};

// How long a refused drop stays lit. Frames rather than seconds because the
// panel is handed a dt it may legitimately be passed as 0 (the editor's Paused
// mode), and a flash measured in seconds would then never expire.
constexpr u_int uANIM_COLLISION_FLASH_FRAMES = 20u;

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

	//=========================================================================
	// OPERATIONS (WU-3.3).
	//
	// ★ EVERY GESTURE HAS A BOOL-RETURNING ATOMIC TWIN, exactly as the graph
	// editor's Action_* verbs do, and for the same reason that panel learned:
	// a test (and WU-3.4's authoring steps) must be able to perform an operation
	// WITHOUT synthesising input. Driving a dope sheet through simulated clicks
	// means every failure arrives as "the key did not move", with the click, the
	// input bridge, the hit-rect and the operation all suspects and none of them
	// named. The mouse handlers in the _Render TU translate input into exactly
	// these calls and add nothing of their own.
	//
	// ★ AN ACTION NEVER READS ImGui STATE. Not the mouse, not the modifiers, not
	// the focus — a select MODE is a parameter, a drag DELTA is a parameter. That
	// is what makes them callable from a unit that has no frame open, and it is
	// what keeps "what the gesture means" in the handler where a reader looks for
	// it rather than spread across both.
	//
	// ★ EVERY ONE OF THEM GOES THROUGH THE DOCUMENT'S VERBS. Nothing here
	// touches Flux_AnimationClip, and nothing here addresses a key by index
	// (D24): the document is the only writer of the working clip because every
	// mutation has to re-map the stable ids, mark dirty and push undo in the same
	// breath, and a caller that reached past it would skip all three.
	//
	// ★ A MULTI-KEY OPERATION IS ONE UNDO STEP. Each of the mutating actions
	// below brackets its work in Zenith_AnimationDocument::BeginCompound /
	// EndCompound, so a drag over eleven keys is one Ctrl+Z and not eleven —
	// and a refusal partway through rolls the whole thing back rather than
	// leaving a half-applied edit on the stack.
	//=========================================================================

	//------------------------------------------------------------------------
	// Selection. Pure panel state: none of these touches the document.
	//------------------------------------------------------------------------

	// False for a key that does not resolve in the document — selecting a key
	// that is not there would put an id in the selection that no undo can bring
	// back, which is the one case the stable ids cannot rescue.
	bool Action_SelectKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimSelectMode eMode);
	bool Action_SelectEvent(u_int uEventId, Zenith_AnimSelectMode eMode);

	// Everything whose RECORDED rect intersects the rectangle, in absolute
	// screen coordinates — so it hit-tests exactly what was painted, including
	// the off-screen gate. Requires a rendered frame; false when nothing was
	// recorded or the document is closed. The rectangle may be given in either
	// winding (a rubber band dragged up-left is normalised here).
	bool Action_BoxSelect(float fX0, float fY0, float fX1, float fY1,
		Zenith_AnimSelectMode eMode = ZENITH_ANIMSELECT_REPLACE);

	// True iff there WAS a selection to clear.
	bool Action_ClearSelection();

	u_int GetSelectedKeyCount() const { return m_axSelectedKeys.GetSize(); }
	bool GetSelectedKeyAt(u_int uIndex, Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const;
	bool IsKeySelected(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const;
	u_int GetSelectedEventCount() const { return m_auSelectedEventIds.GetSize(); }
	u_int GetSelectedEventIdAt(u_int uIndex) const;
	bool IsEventSelected(u_int uEventId) const;

	//------------------------------------------------------------------------
	// Mutation.
	//------------------------------------------------------------------------

	// Move every selected KEY by the same delta.
	//
	// ★ THE SNAP IS APPLIED TO THE PRIMARY KEY AND THE RESULT BECOMES THE
	// DELTA FOR ALL OF THEM. Snapping each key independently would collapse a
	// selection whose members sit off-grid onto the same frames and silently
	// change their relative spacing — the one thing a multi-key drag must not
	// do. The primary is the key most recently selected (or the one a drag
	// started on); the first selected key when that is no longer in the set.
	//
	// ★ REFUSED WHOLE ON ANY COLLISION (D11). If any target time is already
	// held by a key that is not itself part of the move, NOTHING moves, false
	// comes back, and GetCollisionFlashFramesRemaining() lights up so the
	// refusal is visible on the sheet rather than only in a return value. Keys
	// are moved in descending time order for a forward delta (ascending for a
	// backward one) so no key ever passes through a slot its neighbour has not
	// vacated yet.
	//
	// Also refused: an empty selection, a target time below zero, and an
	// effective delta of zero (which would push an undo entry that reverses
	// nothing).
	bool Action_MoveSelection(float fDeltaSeconds, bool bSnap);

	// Remove every selected key AND every selected event, as one step.
	// ★ THE SELECTION IS NOT CLEARED. The undo re-inserts each key under its
	// ORIGINAL id, so the ids held here resolve again afterwards and the user
	// gets their selection back with their keys — which is the entire reason
	// the document allocates stable ids in the first place.
	bool Action_DeleteSelection();

	// Copy the selected keys, then re-insert them one frame past the LAST key
	// of the selection, preserving every value and the internal spacing.
	//
	// The brief allowed either "t + one frame" or "at the playhead"; this is
	// the first, generalised so a multi-key selection cannot collide with
	// itself: for a single key the offset IS one frame, and for a block it is
	// (last - first) + one frame, which lands the copy immediately after the
	// original. Refused when the clip has no frame grid (there is then no "one
	// frame" to offset by) and refused whole on any collision.
	//
	// The DUPLICATES become the selection, so the obvious next gesture — drag
	// them somewhere — works without a second click.
	bool Action_DuplicateSelection();

	// Put the selected keys on the panel's clipboard as (track kind, relative
	// time, value). False for an empty selection.
	bool Action_CopySelection();

	// Paste the clipboard onto strBoneName, with every relative time shifted by
	// fTimeOffset. Kinds are matched T->T, R->R, S->S; a track the bone has no
	// keys on — or a bone the clip has no channel for at all — is CREATED by
	// the document's own insert verb. Refused whole on any collision, and on a
	// negative target time. The pasted keys become the selection.
	bool Action_PasteToBone(const std::string& strBoneName, float fTimeOffset);

	// Move every key at or after fFromTime, on EVERY track, by fDelta.
	//
	// ★ EVENTS DO NOT MOVE (D4). An event time is a [0,1] FRACTION of the clip,
	// not a point on the seconds clock, so it is already expressed relative to
	// whatever the duration becomes — "shifting it proportionally" would move it
	// twice. The clip's DURATION is not touched either; that is a separate,
	// separately undoable decision.
	//
	// Refused whole on any collision with a key that is not itself moving, and
	// on any target time below zero.
	bool Action_RippleRetime(float fFromTime, float fDelta);

	// Seek the preview session. Clamped into [0, duration]. Emits NO animation
	// events — that is Zenith_AnimationPreviewSession::Seek's own contract, not
	// something added here. False when the session cannot be scrubbed (no rig).
	bool Action_Scrub(float fTimeSeconds);

	// The clip's duration, through the document (one undoable step). Refused
	// for a negative or non-finite value, and for one the clip already has.
	bool Action_SetDuration(float fDurationSeconds);

	bool Action_Undo();
	bool Action_Redo();

	//------------------------------------------------------------------------
	// Operation diagnostics — UNGATED, for the same reason the rect
	// diagnostics are: a bare `false` from an action has several causes, and a
	// test that can only see the bool reports "it did not work".
	//------------------------------------------------------------------------

	// Non-zero while a refused drop is lit. Counts down one per rendered frame.
	u_int GetCollisionFlashFramesRemaining() const { return m_uCollisionFlashFrames; }
	// WHICH key the refusal collided with, while the flash is lit.
	bool GetCollisionFlashKey(Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const;

	u_int GetClipboardKeyCount() const { return m_axClipboard.GetSize(); }
	// The bone whose row was last clicked — what Ctrl+V pastes onto. Empty until
	// something on a bone row has been touched.
	const std::string& GetPasteTargetBone() const { return m_strPasteTargetBone; }

	// Live drag state, so a test can tell "the drag never started" apart from
	// "the drag started and the drop was refused".
	bool IsDraggingKeys() const { return m_bDraggingKeys; }
	float GetDragDeltaSeconds() const { return m_fDragDeltaSeconds; }
	bool IsBoxSelecting() const { return m_bBoxSelecting; }
	bool IsScrubbing() const { return m_bScrubbing; }
	bool IsDraggingDuration() const { return m_bDraggingDuration; }

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

	//-------------------------------------------------------------------------
	// Operation helpers — all in Zenith_EditorPanel_Animation_Ops.cpp.
	//-------------------------------------------------------------------------

	// One move of a set of (track, key) pairs by one delta, bracketed as ONE
	// undo step. Shared verbatim by Action_MoveSelection and
	// Action_RippleRetime, which differ only in how they choose the set: a
	// second copy of the collision pre-check and the ordering rule is exactly
	// how two paths that must agree stop agreeing.
	bool MoveKeySetByDelta(const Zenith_Vector<Zenith_AnimSelectedKey>& axKeys, float fDeltaSeconds,
		const char* szDescription);

	// Would fTargetTime land on a key that is NOT in axMoving? Records the
	// blocker in the flash state and returns true when it would.
	bool WouldCollide(const Zenith_AnimTrackId& xTrack, float fTargetTime,
		const Zenith_Vector<Zenith_AnimSelectedKey>& axMoving);

	// Insert a set of (track, time, value) triples as ONE undo step, refusing
	// the lot on any collision. Shared by duplicate and paste.
	bool InsertKeySetAsOneStep(const Zenith_Vector<Zenith_AnimTrackId>& axTracks,
		const Zenith_Vector<float>& afTimes, const Zenith_Vector<Zenith_AnimKeyValue>& axValues,
		const char* szDescription);

	static bool IsSameSelectedKey(const Zenith_AnimSelectedKey& xA, const Zenith_AnimTrackId& xTrack, u_int uKeyId);
	u_int FindSelectedKeyIndex(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const;
	u_int FindSelectedEventIndex(u_int uEventId) const;
	// The shared body of every select gesture: REPLACE clears first, TOGGLE
	// removes an entry that is already there, ADD is idempotent.
	void ApplyKeySelectMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimSelectMode eMode);
	void ApplyEventSelectMode(u_int uEventId, Zenith_AnimSelectMode eMode);
	// The key a snap is computed against — see Action_MoveSelection.
	bool ResolvePrimarySelectedKey(Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const;
	// A raw pixel-derived delta turned into the one the move will actually
	// apply: the primary key's target snapped to the frame grid, expressed back
	// as a delta. ★ ONE DEFINITION, shared by Action_MoveSelection and by the
	// drag GHOST — a second copy in the renderer is how a preview ends up
	// showing a position the drop does not produce.
	float EffectiveDragDelta(float fRawDeltaSeconds, bool bSnap) const;
	void RaiseCollisionFlash(const Zenith_AnimTrackId& xTrack, u_int uKeyId);
	// Every (track, key) in the DOCUMENT — every bone's three tracks plus root
	// motion's two, straight from GetBoneNamesSorted.
	//
	// ★ NOT FROM THE ROW MODEL. A collapsed group has no rows, so a ripple built
	// on m_axRows would silently skip every key of every collapsed bone and
	// desynchronise the clip against a view state that is meant to be cosmetic.
	void CollectAllKeys(Zenith_Vector<Zenith_AnimSelectedKey>& axOut) const;

	//-------------------------------------------------------------------------
	// Input translation — Zenith_EditorPanel_Animation_Render.cpp. These are
	// the ONLY functions that read ImGui state; each one ends in an Action_*.
	//-------------------------------------------------------------------------
	void HandleSheetInput(const SheetLayout& xLayout, bool bCanvasHovered);
	void HandleSheetKeyboard();
	static Zenith_AnimSelectMode SelectModeFromModifiers();
	// The key whose recorded rect contains (fX, fY), if any.
	bool FindKeyAtScreenPos(float fX, float fY, Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const;
	bool FindEventAtScreenPos(float fX, float fY, u_int& uOutEventId) const;

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
	// The rubber band and the duration handle — decorations, drawn last so they
	// sit over the rows. (The drag GHOST is drawn by DrawKeysForRow, beside the
	// real diamond, because that is the one place a key's row centre is known.)
	void DrawSelectionOverlays(ImDrawList* pxDraw, const SheetLayout& xLayout);

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

	//-------------------------------------------------------------------------
	// Selection (WU-3.3).
	//
	// ★ PLAIN VECTORS WITH A LINEAR SEARCH, DELIBERATELY. A hash would want a
	// key built from a bone NAME plus a track plus an id, which is a string
	// concatenation per lookup — more allocation than the scan it replaces for
	// every selection a dope sheet actually holds. The order is also load-
	// bearing: the vector's tail is the most recently selected key, which is the
	// PRIMARY a snap is computed against.
	//
	// ★ NOTHING PRUNES A STALE ID, AND THAT IS THE POINT. A delete removes the
	// keys but leaves their ids here, so the undo — which re-inserts each key
	// under its ORIGINAL id — hands the user back their selection along with
	// their keys. An id that never comes back simply stops resolving, and every
	// consumer already treats that as "not there".
	//-------------------------------------------------------------------------
	Zenith_Vector<Zenith_AnimSelectedKey> m_axSelectedKeys;
	Zenith_Vector<u_int> m_auSelectedEventIds;

	// The key a multi-key snap is computed against: the last one selected, or
	// the one a drag started on. Falls back to the first selected key.
	Zenith_AnimTrackId m_xPrimaryKeyTrack;
	u_int m_uPrimaryKeyId = uINVALID_ANIM_KEY_ID;

	// Cross-bone paste buffer. Times are relative to the earliest key copied.
	Zenith_Vector<Zenith_AnimClipboardKey> m_axClipboard;
	std::string m_strPasteTargetBone;

	//-------------------------------------------------------------------------
	// Live gestures. ★ A DRAG MUTATES NOTHING UNTIL IT IS RELEASED: the delta
	// is drawn as a ghost diamond and applied — as one undo step — on mouse up.
	// Mutating per frame would push one command per frame of the drag and make
	// the intermediate positions, which the user was only passing through, into
	// undo stops.
	//-------------------------------------------------------------------------
	bool m_bDraggingKeys = false;
	float m_fDragStartMouseX = 0.0f;
	float m_fDragDeltaSeconds = 0.0f;

	bool m_bBoxSelecting = false;
	float m_fBoxStartX = 0.0f;
	float m_fBoxStartY = 0.0f;
	float m_fBoxEndX = 0.0f;
	float m_fBoxEndY = 0.0f;

	bool m_bScrubbing = false;
	// ★ THE DURATION HANDLE IS THE SAME "PREVIEW, THEN COMMIT" SHAPE as the key
	// drag: the dragged value lives here and is drawn as a ghost line, and ONE
	// Action_SetDuration runs on release. Writing it through the document every
	// frame would push a Duration command per frame of the drag.
	bool m_bDraggingDuration = false;
	float m_fDurationDragSeconds = 0.0f;

	// D11's visible half — a refused drop lights the key that blocked it.
	u_int m_uCollisionFlashFrames = 0;
	Zenith_AnimTrackId m_xCollisionFlashTrack;
	u_int m_uCollisionFlashKeyId = uINVALID_ANIM_KEY_ID;
};

#endif // ZENITH_TOOLS
