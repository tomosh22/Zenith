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
// Zenith_EditorPanel_Animation (WU-3.2 / WU-3.3 / WU-5B) — the DOPE SHEET.
//
// One dockable window over ONE Zenith_AnimationDocument and ONE
// Zenith_AnimationPreviewSession: a ruler, a row per bone track (T/R/S), the
// two root-motion tracks, an events row, a playhead, and the shared preview
// image. WU-3.2 built the drawing and the hit rects; WU-3.3 added SELECTION and
// the OPERATIONS — see the "OPERATIONS" block below, and its three rules: every
// gesture has a bool-returning Action_* twin that reads no ImGui state, every
// mutation goes through a document verb, and a multi-key operation is ONE undo
// step (Zenith_AnimationDocument::BeginCompound). WU-5B made the events row
// EDITABLE — add, drag, delete, rename, payload — plus D40's scrub-emission
// toggle and a strip of what the runtime dispatcher actually fired.
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

// How many recently-EMITTED event names the strip keeps. Small on purpose: it
// is a "did that fire?" readout beside a scrubbing playhead, not a log — the
// log is the running total (GetTotalEmittedEventCount).
constexpr u_int uANIM_EMITTED_EVENT_HISTORY = 8u;

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
// THE BONE MANIPULATOR (WU-4.3) — the geometry, as PURE FUNCTIONS.
//
// ★ IT IS AN ImGui DRAW-LIST OVERLAY ON THE PREVIEW IMAGE, NOT Flux_Gizmos, and
// that is a feasibility correction rather than a style choice (design note
// §8.1): Flux_GizmosImpl is entity-typed all the way down AND declares one pass
// writing the FINAL render target with no per-view selection, so it renders with
// the MAIN camera's constants over the main viewport while the preview session
// renders into the shared preview view slot.
//
// ★ EVERYTHING BELOW IS FREE FUNCTIONS OVER PIXELS, so the part that is easy to
// get silently wrong — which ring the cursor grabbed, which way round a drag
// turns — is catchable by a headless unit with no frame, no camera and no rig.
// The panel supplies the projection (GetPoseRingSet) and nothing else.
//
// ★ THE RING PARAMETER IS THE RIGHT-HANDED ROTATION ANGLE, BY CONSTRUCTION.
// Zenith_AnimPoseRingBasis returns (u, v) with cross(u, v) == axis, and a point
// at parameter theta is pivot + r * (cos(theta) * u + sin(theta) * v) — so
// walking the polyline forward IS turning positively about the axis. That is
// what lets the drag recover its screen SIGN by measuring the projected step
// from point[0] to point[1] instead of reasoning about handedness, the Vulkan
// Y flip and which side of the pivot the camera is on. Two of those three have
// already been got wrong once in this panel's own projection helper.
//=============================================================================

// How many segments a rotation ring is projected, drawn and hit-tested with.
// 48 is 7.5 degrees per segment: at the ~40 px radius the screen-relative sizing
// below produces, the chord error is well under a pixel, so the polyline the hit
// test measures against and the circle the user sees are the same curve.
constexpr u_int uANIM_POSE_RING_SEGMENTS = 48u;

// "No ring." Same shape and same reason as kuINVALID_BONE_SELECTION: every
// consumer already has to handle "the index does not resolve", and a bool beside
// it would give one fact two representations that can disagree.
constexpr u_int uINVALID_ANIM_POSE_RING = 0xFFFFFFFFu;

// How near, in PREVIEW-IMAGE pixels, the cursor has to be to a ring's projected
// polyline to grab it. Deliberately NOT DPI-scaled: it is compared against
// coordinates expressed in the same image-pixel space on both sides, and a
// scaled tolerance would make a unit's result depend on the display it ran on.
constexpr float fANIM_POSE_RING_GRAB_PIXELS = 8.0f;

// The ring radius as a fraction of the pivot's DISTANCE FROM THE CAMERA, which
// makes the handle a roughly constant size on screen — the same camera-relative
// sizing the entity gizmo uses. A fraction of the SKELETON's extent was the
// obvious alternative and is wrong: a 0.5 m two-bone rig would then draw a ring
// a handful of pixels across while a 1.8 m humanoid drew one off the edge of the
// pane, and the grab tolerance above would mean something different for each.
constexpr float fANIM_POSE_RING_SCREEN_FRACTION = 0.18f;

// The increment the drag angle rounds to while the panel's angle-snap toggle is
// on, through Flux_GizmosImpl::SnapValue — the SAME pure rounding the entity
// gizmo snaps with, so a snapped bone and a snapped entity agree.
constexpr float fANIM_POSE_SNAP_DEGREES = 15.0f;

//-----------------------------------------------------------------------------
// One ring, projected into preview-image pixels. Closed: the last point repeats
// the first, so the hit test walks uANIM_POSE_RING_SEGMENTS segments without a
// wrap-around special case.
//
// m_bValid is false when ANY of its points fell behind the camera. A partially
// projected ring is worse than none: the missing arc is exactly where a mirrored
// coordinate would land, and hit-testing against it would grab a ring that is
// not under the cursor.
//-----------------------------------------------------------------------------
struct Zenith_AnimPoseRing
{
	bool m_bValid = false;
	Zenith_Maths::Vector2 m_axPoints[uANIM_POSE_RING_SEGMENTS + 1];
};

struct Zenith_AnimPoseRingSet
{
	// The bone's world pivot, projected. In preview-image pixels like the rings.
	Zenith_Maths::Vector2 m_xPivotPixel = Zenith_Maths::Vector2(0.0f);
	// World X / Y / Z. WORLD rather than the parent's frame, for Phase 4: the
	// preview camera orbits the origin and the session model matrix is identity,
	// so a world ring is the frame the user is actually looking at. A local-space
	// mode is one call to Zenith_AnimPoseRingBasis away and needs no other change.
	Zenith_AnimPoseRing m_axRings[3];
};

// The WORLD axis a ring index names: 0 = X, 1 = Y, 2 = Z. X for anything else.
Zenith_Maths::Vector3 Zenith_AnimPoseRingAxis(u_int uAxis);

// The right-handed basis the ring is generated in — cross(u, v) == the axis.
void Zenith_AnimPoseRingBasis(u_int uAxis, Zenith_Maths::Vector3& xOutU, Zenith_Maths::Vector3& xOutV);

// The SIGNED angle in radians from (xFrom - xPivot) to (xTo - xPivot), in
// (-pi, pi]. Zero when either arm is degenerate, which is the right answer for
// a cursor sitting exactly on the pivot: there is no direction to measure.
//
// ★ THE SIGN IS IN WHATEVER HANDEDNESS THE PIXELS ARE IN, and this function
// does not care. The panel resolves that once per drag by measuring a step it
// already knows the world sign of (see the block comment above).
float Zenith_AnimPoseSignedScreenAngle(const Zenith_Maths::Vector2& xPivot,
	const Zenith_Maths::Vector2& xFrom, const Zenith_Maths::Vector2& xTo);

// Shortest distance in pixels from (fX, fY) to a ring's projected polyline. A
// huge value for an invalid ring, so a caller may compare blind.
float Zenith_AnimPoseDistanceToRing(const Zenith_AnimPoseRing& xRing, float fX, float fY);

// The NEAREST valid ring within fTolerancePixels. False — and uOutAxis
// UNTOUCHED — when nothing is near enough, matching Zenith_RaycastBonePickSet.
bool Zenith_AnimPosePickRing(const Zenith_AnimPoseRingSet& xRings, float fX, float fY,
	float fTolerancePixels, u_int& uOutAxis);

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
	// EVENTS (WU-5B).
	//
	// ★ AN EVENT TIME IS A [0,1] FRACTION OF THE CLIP, NEVER SECONDS (D4), and
	// every action below speaks that unit — including the DELTA one, which is a
	// delta in normalized units and not in seconds. The sheet's x axis is
	// seconds, so the row maps a stored value through
	// Zenith_AnimTimelineTimeToPixel(view, fNormalized * duration); the multiply
	// is the whole of the conversion and it lives in the renderer, next to the
	// only thing that needs it.
	//
	// ★ A DURATION CHANGE MOVES THE ROW POSITION AND NOT THE STORED VALUE, and
	// that is the point of D4 rather than a side effect of it. Nothing here (and
	// nothing in Action_SetDuration) rescales an event when the duration moves:
	// the value is already expressed relative to whatever the duration becomes,
	// so "adjusting" it would move the event twice. The units assert exactly
	// that — same stored fraction, new pixel.
	//
	// ★ EVENTS MAY COINCIDE; KEYS MAY NOT. D11's no-silent-merge rule exists
	// because a track cannot hold two keys at one time — the second insert would
	// overwrite the first's value and the operation would report success having
	// produced fewer keys than it was asked for. An event list has no such
	// constraint: two footsteps authored on the same frame are two events and
	// both fire. So NO action below carries a collision pre-check, none of them
	// raises the collision flash, and a drag that lands one event exactly on
	// another is allowed.
	//------------------------------------------------------------------------

	// What an event created by a GESTURE is called until it is renamed. A
	// function rather than a bare constant so the toolbar, the double-click
	// handler and the units cannot disagree about it — and it is needed because
	// Action_AddEvent REFUSES an empty name: an unnamed event is invisible on
	// the row and matches no listener at runtime, so it is a silent no-op
	// wearing an undo entry.
	static const char* DefaultEventName();

	// Add one event at fNormalizedTime, as ONE undo step.
	//
	// The new event REPLACES the selection, so the inspector strip is already
	// pointing at it and the obvious next gesture — type a name — needs no
	// second click. Its id is then GetSelectedEventIdAt(0).
	//
	// Refused for a closed document, a non-finite or negative time, and an empty
	// name. A time PAST 1.0 is allowed and is counted by
	// GetEventsPastDurationCount() (D13) — the same treatment a key past the
	// duration gets, for the same reason: refusing it would silently discard
	// what the user asked for.
	bool Action_AddEvent(float fNormalizedTime, const std::string& strName);

	// Move every selected EVENT by the same NORMALIZED delta, as ONE undo step.
	//
	// ★ THE DELTA IS NORMALIZED, AND THE SNAP IS NOT. bSnap snaps the PRIMARY
	// event's target to the frame grid IN SECONDS — at the CURRENT duration —
	// and converts the snapped result back to a normalized delta that every
	// selected event then shares. That ordering is forced: the frame grid is a
	// property of the seconds clock (the clip's authored frame rate), and there
	// is no such thing as "the nearest frame" in [0,1] without a duration to
	// divide by. The primary is the most recently selected event.
	//
	// Refused for an empty event selection, a non-finite delta, an effective
	// delta of zero, and any target below zero. NOT refused for a target that
	// coincides with another event — see the block comment above.
	bool Action_MoveSelectedEvents(float fDeltaNormalized, bool bSnap);

	// The inspector strip's two edits. Each is ONE Zenith_AnimCommand_EventEdit,
	// pushed on EDIT-COMPLETE by the handler rather than per keystroke — a
	// per-character command would make Ctrl+Z walk backwards through a name one
	// letter at a time. Both refuse a value the event already has, so a field
	// that was focused and left alone pushes nothing.
	bool Action_RenameEvent(u_int uEventId, const std::string& strName);
	bool Action_SetEventPayload(u_int uEventId, const Zenith_Maths::Vector4& xPayload);

	// D40's toggle, on the SESSION'S OWN controller. Default OFF: a scrub across
	// a clip would otherwise replay every footstep in it, which is why
	// Flux_AnimationController::SeekDirectPlay moves the bookkeeping mark
	// without firing by default. Turning it on is how an author HEARS the beat
	// they are placing. Not an edit — it dirties nothing and pushes no undo.
	bool Action_SetEmitEventsOnScrub(bool bEmit);
	bool GetEmitEventsOnScrub() const;

	//=========================================================================
	// POSE AUTHORING (Phase 4).
	//
	// ★ THE WHOLE Action_* SURFACE IS DECLARED HERE BY WU-4.1, INCLUDING THE
	// PARTS IT DOES NOT IMPLEMENT. WU-4.3 (drag, Set Key, auto-key) and WU-4.4
	// (IK bake) then FILL BODIES in their own TUs rather than adding
	// declarations to this header — which is what turns what would have been a
	// three-way write conflict on one file into a one-way dependency, and gives
	// both of them a stable compile target from the moment this lands. Every
	// stub below returns false and says which unit owns it.
	//
	// ★ THE BONE MANIPULATOR IS AN ImGui DRAW-LIST OVERLAY, NOT Flux_Gizmos, and
	// that is a feasibility correction rather than a style choice.
	// Flux_GizmosImpl is entity-typed all the way down (Zenith_Entity*
	// target, every interaction site resolving through it) AND it declares one
	// pass writing the FINAL render target with no per-view selection — so it
	// renders with the MAIN camera's constants over the main viewport, while the
	// preview session renders into the shared preview view slot. Making it
	// per-view is a render-graph change no Phase-4 unit owns.
	//=========================================================================

	// Select by INDEX. False when there is no session, or when the index does
	// not resolve against the current rig — the session clears rather than
	// storing an index nothing can look up.
	bool Action_SelectBone(u_int uBoneIndex);
	// True iff there WAS a bone selection to clear.
	bool Action_ClearBoneSelection();

	// Select whatever bone sits under a pixel of the preview image.
	//
	// ★ THE PIXEL IS RELATIVE TO THE PREVIEW IMAGE'S TOP-LEFT, not to the
	// screen and not to the window. That is the space ProjectPreviewWorldPoint
	// answers in and the space BuildPreviewRay consumes, so a caller that has a
	// world position can aim at it without knowing where the panel happens to
	// be; the mouse handler converts once, at the one place it has the
	// absolute position. Requires a rendered frame (the image's SIZE comes from
	// the recorded rect) and an open session with a resolved rig.
	//
	// False on a miss, and a miss changes NOTHING — clicking empty space beside
	// a bone does not deselect it, the same way the sheet's empty-space click
	// is a separate gesture from a key click.
	bool Action_PickBoneAtPreviewPixel(float fPixelX, float fPixelY);

	// THE key-writing verb (design note §5.4). Every caller — the Set Key
	// button, auto-key on drag release, and the IK bake — goes through this one
	// function, so a key written three different ways is byte-identical.
	//
	// bRotation writes the rotation track; bTranslationForRoot additionally
	// writes translation, and ONLY for a bone with no parent. Scale is not
	// authorable in Phase 4. That asymmetry is not tidiness:
	// Flux_SkeletonPose::SampleFromClip writes a component only if that channel
	// HAS keyframes, so adding the first key to a channel changes that bone's
	// behaviour across the ENTIRE clip — from "follows bind pose" to "follows a
	// single constant". Writing tracks the user did not author would alter
	// frames they never touched.
	bool Action_SetKeyForBones(const Zenith_Vector<u_int>& xBoneIndices, bool bRotation, bool bTranslationForRoot);
	bool Action_SetKeyForSelectedBone();

	// Rotation AND translation for the selected bone, refused unless it is a
	// ROOT. Its own verb rather than a flag on the one above because writing a
	// translation is a decision the author makes explicitly (design note §5.1) —
	// the first key on a channel changes that bone across the whole clip.
	bool Action_SetKeyTranslationForRoot();

	// True iff the value CHANGED, matching Action_SetEmitEventsOnScrub: setting
	// a toggle to what it already is pushes nothing and reports nothing.
	bool Action_SetAutoKey(bool bEnabled);
	bool Action_GetAutoKey() const;

	// Rounds a drag's accumulated angle to fANIM_POSE_SNAP_DEGREES while on.
	// Panel state, not session state: it is a property of the MANIPULATOR (which
	// lives here) rather than of the preview, unlike auto-key, which the session
	// owns because the IK bake and a future second panel both read it.
	bool Action_SetPoseAngleSnap(bool bEnabled);
	bool Action_GetPoseAngleSnap() const;

	// The drag primitive: apply a WORLD-space rotation delta to the selected
	// bone's LOCAL rotation. The conjugation into the parent's frame is
	// WU-4.2's Zenith_BoneSpace; this is the verb that calls it.
	//
	// A ONE-SHOT, applied to the bone's CURRENT rotation and committed
	// immediately — what the automation verb and a keyboard nudge want. It goes
	// through the session's Begin/Update/EndBoneDrag bracket anyway, because
	// those are what suspend clip evaluation, bring model space current (§3.2)
	// and raise the unkeyed-pose flag; a second write path would honour none of
	// the three. Refused while a POINTER drag is live, which owns the same bone.
	bool Action_RotateSelectedBoneWorld(const Zenith_Maths::Quat& xWorldDelta);

	//------------------------------------------------------------------------
	// The pointer drag, in preview-image pixels (§4.3 / §4.4).
	//
	// ★ ONE DRAG IS ONE UNDO STEP, however many frames it spanned, and the
	// intermediate frames are not on the stack at all. Nothing is written to
	// the document between Begin and End: the live pose is the session's, is
	// never undoable and is never serialized, and the KEY — if auto-key is on
	// and the drag actually moved — is written ONCE on release through
	// Action_SetKeyForBones, inside one compound. This is
	// Zenith_Editor::RecordGizmoDragUndo's shape, including its "a click that
	// never moved records nothing".
	//
	// ★ WITH AUTO-KEY OFF A RELEASE WRITES NOTHING AND THAT IS NOT SILENT. The
	// pose is live-but-unkeyed (Session().HasUnkeyedPose()), the preview pane
	// says so, and the next Seek re-evaluates from the clip and discards it.
	// Making the drag itself undoable was rejected: an undo entry restoring a
	// pose the document never contained is a lie about what was saved.
	//------------------------------------------------------------------------

	// Grab the nearest ring under the pixel and latch the bone's rotation.
	// False when no ring is within fANIM_POSE_RING_GRAB_PIXELS, when no bone is
	// selected, without a rendered frame, or when a drag is already in flight.
	bool Action_BeginBoneDragAtPixel(float fPixelX, float fPixelY);
	// Accumulate the pivot-relative screen angle and rewrite the live pose from
	// the LATCHED initial rotation. False when no drag is in flight.
	bool Action_UpdateBoneDragToPixel(float fPixelX, float fPixelY);
	// End it, writing the key when auto-key is on AND the drag moved. True iff a
	// drag was in flight; whether a key was written is visible on the undo stack
	// and in Session().HasUnkeyedPose(), which is where a test should look.
	bool Action_EndBoneDrag();
	// Escape: put the bone back where the drag found it and end it. Nothing
	// reaches the document, so there is nothing to undo.
	bool Action_CancelBoneDrag();

	// Solve a transient chain from the selected bone and its two ancestors to a
	// model-space target, then bake the result down to keys through
	// Action_SetKeyForBones — the SAME function a hand drag uses, so one Ctrl+Z
	// undoes the whole IK gesture and the clip contains nothing IK-specific.
	bool Action_BakeIKForSelectedChain(const Zenith_Maths::Vector3& xTargetModelSpace);

	//------------------------------------------------------------------------
	// The preview camera, as pure maths.
	//
	// Both of these are exact inverses of each other through DIFFERENT code:
	// the projection multiplies forward, the ray inverts through
	// Zenith_Gizmo::ScreenToWorldRay. That is what makes a
	// project-then-pick round trip a real assertion rather than a guard
	// comparing a value against a re-computation of itself.
	//------------------------------------------------------------------------

	// The preview view/projection for the session's CURRENT orbit state, built
	// by the same pure builders the material preview stages the slot with. No
	// frame needed. False when the session is not open.
	bool GetPreviewViewProj(Zenith_Maths::Matrix4& xOutView, Zenith_Maths::Matrix4& xOutProj) const;

	// A world point -> a pixel RELATIVE to the preview image's top-left.
	// False when the point is behind the camera, or without a rendered frame.
	bool ProjectPreviewWorldPoint(const Zenith_Maths::Vector3& xWorld, float& fOutPixelX, float& fOutPixelY) const;

	// A pixel relative to the preview image -> a world ray. The origin is the
	// orbit camera position; the direction comes from
	// Zenith_Gizmo::ScreenToWorldRay against the preview view/proj.
	bool BuildPreviewRay(float fPixelX, float fPixelY,
		Zenith_Maths::Vector3& xOutOrigin, Zenith_Maths::Vector3& xOutDir) const;

	// The preview image's rect in ABSOLUTE screen coordinates, subject to the
	// same off-screen gate as every other rect on this panel.
	//
	// ★ IT IS RECORDED WHETHER OR NOT THERE IS AN IMAGE TO SHOW. On a backend
	// with no device the ImGui registration hands back an invalid handle and the
	// pane draws a same-sized placeholder instead — because bone picking and the
	// overlay are pure CPU maths that must be exercisable headless, and a rect
	// that only exists on a graphics driver would force every unit that touches
	// them to be requiresGraphics (i.e. skipped-as-passed, i.e. rotting).
	bool GetPreviewImageRect(Zenith_AnimPanelRect& xOut) const;

	// The three rotation rings for the SELECTED bone, projected into
	// preview-image pixels. False without a session, a bone selection, a
	// rendered frame or a pivot in front of the camera.
	//
	// ★ COMPUTED ON DEMAND RATHER THAN CACHED BY THE DRAW. The overlay, the hit
	// test and a unit all ask this one function, so "where the ring was painted"
	// and "where a click lands on it" cannot be two derivations that drift — the
	// same mistake the bone overlay avoids by drawing straight from the pick set.
	bool GetPoseRingSet(Zenith_AnimPoseRingSet& xOut) const;

	// Live manipulator state, so a test can tell "the ring was never grabbed"
	// apart from "the drag ran and produced no rotation".
	bool IsBonePoseDragActive() const { return m_bPoseDragActive; }
	u_int GetPoseDragAxis() const { return m_bPoseDragActive ? m_uPoseDragAxis : uINVALID_ANIM_POSE_RING; }
	// The ACCUMULATED, UNSNAPPED angle in radians. The applied one is this
	// rounded by Action_GetPoseAngleSnap; both are worth seeing separately when a
	// snapped drag looks like it did nothing.
	float GetPoseDragAngleRadians() const { return m_fPoseDragAngleRadians; }
	u_int GetPoseHoverRingAxis() const { return m_uPoseHoverAxis; }

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
	bool IsDraggingEvents() const { return m_bDraggingEvents; }
	// In NORMALIZED units, like everything else about an event.
	float GetEventDragDeltaNormalized() const { return m_fEventDragDeltaNormalized; }

	// The event the inspector strip edits — the most recently selected one that
	// still RESOLVES, falling back to the first selected event.
	// uINVALID_ANIM_KEY_ID when no event is picked, which is what makes the
	// strip disappear rather than edit something arbitrary.
	u_int GetInspectorEventId() const;

	//-------------------------------------------------------------------------
	// The emitted-event strip.
	//
	// ★ THE PANEL REGISTERS A Flux_AnimationEventCallback ON THE SESSION'S OWN
	// CONTROLLER (D30 — the session never borrows an entity's), so what lands
	// here is what the RUNTIME dispatcher actually fired, not a re-derivation of
	// which events the playhead crossed. A second opinion computed in the panel
	// would agree with the runtime right up until one of D35-D40 changed, and
	// the strip would then confidently show events nothing received.
	//
	// Index 0 is the MOST RECENT. Only the last uANIM_EMITTED_EVENT_HISTORY are
	// kept, which is why the running total is exposed separately: a test that
	// scrubs a burst wants the count, not the survivors.
	//-------------------------------------------------------------------------
	u_int GetEmittedEventCount() const { return m_astrEmittedEvents.GetSize(); }
	bool GetEmittedEventNameAt(u_int uIndex, std::string& strOut) const;
	u_int GetTotalEmittedEventCount() const { return m_uEmittedEventTotal; }
	void ClearEmittedEvents();

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

	// ★ THE ONE Flux_AnimationEventCallback THE PANEL INSTALLS, with `this` as
	// the user data. A free function pointer rather than anything richer because
	// that is what the controller's typedef IS (no std::function in engine
	// code), and static so its address is stable for the life of the process.
	static void OnPreviewEventEmitted(void* pUserData, const std::string& strEventName,
		const Zenith_Maths::Vector4& xData);
	void PushEmittedEventName(const std::string& strName);

	// Re-read the inspector's fixed buffers from the document when the event
	// they point at CHANGES — never while it is the same one, or a keystroke
	// would be overwritten by the value it has not been committed to yet.
	void SyncEventInspectorBuffers();

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
	// The event twin of it, and what GetInspectorEventId answers with.
	bool ResolvePrimarySelectedEvent(u_int& uOutEventId) const;
	// A raw normalized drag delta turned into the one the drop will apply: the
	// primary event's target snapped to the frame grid IN SECONDS at the current
	// duration, expressed back as a NORMALIZED delta. ★ ONE DEFINITION, shared
	// by Action_MoveSelectedEvents and by the drag ghost — a second copy in the
	// renderer is how a preview shows a position the drop does not produce.
	float EffectiveEventDragDelta(float fRawDeltaNormalized, bool bSnap) const;
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
	// Is (fX, fY) inside the EVENTS row's key lane? What a double-click-to-add
	// hit-tests against, through the published row rect so it cannot fire on a
	// row the off-screen gate refuses to hand out.
	bool IsInEventsRowLane(float fX, float fY) const;

	// Render helpers — all in Zenith_EditorPanel_Animation_Render.cpp.
	void RenderToolbar();
	// The second toolbar line: Add Event, the D40 scrub toggle, and the strip of
	// recently emitted names. Its own line because the first one is already
	// wider than a 900 px window and a SameLine past the edge is a control
	// nobody can reach.
	void RenderEventToolbar();
	// The selected event's name and Vector4 payload. Drawn only when one is
	// selected, so an unselected sheet keeps every pixel of its height.
	void RenderEventInspector();
	// The right-click menu over an event marker. Raised by HandleSheetInput
	// setting a flag rather than opened from inside it, because the sheet is one
	// InvisibleButton and a popup has to be opened from the window scope.
	void RenderEventContextMenu();
	void RenderBanners();
	void RenderPreviewPane();
	// Hover + click over the preview image, translated into Action_* calls. The
	// ONLY place an absolute mouse position is turned into an image-relative
	// pixel, so nothing else has to know where the pane landed.
	void HandlePreviewPaneInput(bool bImageHovered);
	// The selected / hovered bone, projected through the preview camera and
	// painted over the image: a line along the bone's capsule and a circle at the
	// joint it moves. Draw-list only — it is a decoration, not an item.
	void DrawBoneOverlay(ImDrawList* pxDraw);

	//-------------------------------------------------------------------------
	// The bone manipulator — Zenith_EditorPanel_Animation_Pose.cpp. The drawing
	// half is a DECORATION like everything else painted over the pane.
	//-------------------------------------------------------------------------

	// Returns TRUE when the manipulator owns this frame's gesture, which is what
	// keeps a press on a ring from also re-picking a bone or orbiting the camera.
	bool HandlePoseManipulatorInput(bool bImageHovered);
	void DrawPoseManipulator(ImDrawList* pxDraw);
	// Set Key / Auto-key / Angle snap. Its own toolbar LINE, for the reason
	// RenderEventToolbar has one: the first row already runs wider than a 900 px
	// window and a SameLine past the edge is a control nobody can reach.
	void RenderPoseToolbar();
	// Turn m_fPoseDragAngleRadians (snapped or not) into the live pose. ★ ALWAYS
	// FROM THE LATCHED INITIAL ROTATION, never from the previous frame's output:
	// one delta applied to a fixed value cannot drift, and feeding the output
	// back in would accumulate both the float error and the snap's rounding.
	void ApplyPoseDragAngle();
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
	// Where the preview image (or its same-sized placeholder) was drawn this
	// frame — the origin and scale every preview pixel is expressed against.
	Zenith_AnimPanelRect m_xPreviewImageRect;
	bool m_bRulerRectValid = false;
	bool m_bPlayheadRectValid = false;
	bool m_bTrackAreaRectValid = false;
	bool m_bCanvasRectValid = false;
	bool m_bPreviewImageRectValid = false;

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

	// The event twin of the pair above: what an event drag snaps around, and
	// what the inspector strip edits.
	u_int m_uPrimaryEventId = uINVALID_ANIM_KEY_ID;

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

	//-------------------------------------------------------------------------
	// Events (WU-5B).
	//
	// The drag is the SAME "preview, then commit" shape as the key drag and the
	// duration handle: the delta lives here, is drawn as a ghost flag, and ONE
	// Action_MoveSelectedEvents runs on release. Mutating per frame would push a
	// command per frame of the drag.
	//-------------------------------------------------------------------------
	bool m_bDraggingEvents = false;
	float m_fEventDragStartMouseX = 0.0f;
	float m_fEventDragDeltaNormalized = 0.0f;
	// Set by the right-click handler, consumed by RenderEventContextMenu.
	bool m_bEventContextMenuRequested = false;

	// The inspector strip's edit buffers. Fixed, because ImGui::InputText wants
	// one; re-read only when m_uInspectorBufferEventId stops matching the event
	// being edited (see SyncEventInspectorBuffers).
	u_int m_uInspectorBufferEventId = uINVALID_ANIM_KEY_ID;
	char m_acEventNameBuffer[128] = {};
	float m_afEventPayloadBuffer[4] = {};
	// ★ WAS EITHER FIELD BEING EDITED AS OF LAST FRAME. The buffers are re-read
	// from the document every frame EXCEPT while one of them is active, which is
	// what makes an UNDO of a rename show up in the box — an "only on target
	// change" refresh would leave the strip displaying a name the clip no longer
	// has, with nothing to hint that Ctrl+Z had worked.
	bool m_bEventInspectorEditing = false;

	// The emitted-event strip. Most recent LAST in the vector; the accessor
	// reverses, because "the last thing that fired" is index 0 to a reader.
	Zenith_Vector<std::string> m_astrEmittedEvents;
	u_int m_uEmittedEventTotal = 0;

	//-------------------------------------------------------------------------
	// The bone manipulator (WU-4.3).
	//
	// ★ THE SAME "PREVIEW, THEN COMMIT" SHAPE as the key drag, the duration
	// handle and the event drag — with the one difference that what is previewed
	// here is a LIVE POSE on the session rather than a ghost drawn beside the
	// real thing. It is still true that nothing reaches the document until the
	// button comes up.
	//-------------------------------------------------------------------------
	bool m_bPoseDragActive = false;
	u_int m_uPoseDragAxis = 0u;
	// ★ +1 or -1, RESOLVED ONCE AT DRAG START and frozen for the gesture, the way
	// the entity gizmo freezes its drag axis. It maps "the direction the cursor
	// went round the pivot on screen" onto "the sign of the world rotation", and
	// it is MEASURED (from the projected step between ring points 0 and 1) rather
	// than derived — a derivation would have to be right about the projection's
	// handedness, the Vulkan Y flip and which side of the pivot the camera is on.
	float m_fPoseDragScreenSign = 1.0f;
	Zenith_Maths::Vector2 m_xPoseDragPivotPixel = Zenith_Maths::Vector2(0.0f);
	Zenith_Maths::Vector2 m_xPoseDragLastPixel = Zenith_Maths::Vector2(0.0f);
	// Accumulated per frame from small steps rather than measured from the press
	// pixel, so a drag past half a turn keeps going instead of folding back
	// through the atan2 branch cut.
	float m_fPoseDragAngleRadians = 0.0f;
	bool m_bPoseDragMoved = false;
	// ★ WAS THE POSE ALREADY UNKEYED WHEN THIS DRAG STARTED. A cancel (and a
	// drag that never moved) must put the badge back the way it found it: the
	// session raises the flag on every live write, so clearing it unconditionally
	// would hide an EARLIER unkeyed edit that is still in the pose.
	bool m_bPoseWasUnkeyedAtDragStart = false;
	u_int m_uPoseHoverAxis = uINVALID_ANIM_POSE_RING;
	bool m_bPoseAngleSnap = false;
};

#endif // ZENITH_TOOLS
