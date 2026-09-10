#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimationDocument.h"
#include "Editor/Zenith_AnimationPreviewSession.h"
#include "Editor/Zenith_AnimTimelineMath.h"
#include "Editor/Zenith_BoneMaskDocument.h"   // WU-7.1's "Bone Masks" sub-panel
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
// The class is spread over SIX TUs, split by what a reader wants separately:
//   Zenith_EditorPanel_Animation.cpp        — lifecycle, rows, the hit rects
//   Zenith_EditorPanel_Animation_Render.cpp — the drawing, and input TRANSLATION
//   Zenith_EditorPanel_Animation_Ops.cpp    — selection and the operations,
//                                             with not one line of ImGui in it
//   Zenith_EditorPanel_Animation_Pose.cpp   — the ring manipulator, the drag
//                                             transaction, Set Key and auto-key
//   Zenith_EditorPanel_Animation_IK.cpp     — the IK target widget's maths and
//                                             verbs, with no ImGui in it either
//   Zenith_EditorPanel_Animation_Curve.cpp  — the curve view
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

// How far the IK target has to move, IN MODEL-SPACE UNITS, before the drag counts
// as having happened at all — the position twin of the ring drag's angular dead
// zone, and it exists for the same reason: a press-and-release that never moved
// must write no key, push no undo entry and raise no unkeyed-pose badge. 1e-4 is
// a tenth of a millimetre on a metre-scale rig, far below anything a hand
// produces through a projected plane and far above the float noise a ray/plane
// intersection carries.
constexpr float fANIM_IK_DRAG_DEAD_ZONE = 1.0e-4f;
// ★ THE PIXEL HALF OF THE DEAD ZONE. The handle pixel a press lands on is the
// effector PROJECTED and rounded; reprojecting that same pixel onto the drag
// plane lands a fraction of a pixel away from the effector in model space --
// far more than fANIM_IK_DRAG_DEAD_ZONE -- so a press-and-release on the spot
// read as a move. The cursor has to leave the press pixel by this much before
// the model-space test is even asked.
constexpr float fANIM_IK_DRAG_DEAD_ZONE_PIXELS = 1.0f;

// The IK handle's drawn radius at 1x DPI. Deliberately SMALLER than the grab
// tolerance (fANIM_POSE_RING_GRAB_PIXELS, which the handle shares with the
// rings): a handle that is easier to hit than it looks is the right way round,
// and giving the handle a second tolerance of its own is how "where it is drawn"
// and "where it can be grabbed" become two numbers that drift.
constexpr float fANIM_IK_HANDLE_RADIUS_1X = 4.5f;

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
// THE CURVE EDITOR (WU-8.2) — the VALUE axis, as pure functions.
//
// ★ THE X AXIS IS NOT HERE, AND THAT IS THE WHOLE POINT. Time <-> pixels is
// Zenith_AnimTimelineMath's, unchanged and unshadowed, so the curve view's
// horizontal geometry IS the dope sheet's: the playhead, the ruler ticks, the
// duration shade and the events row line up with the curves by construction
// rather than by two mappings agreeing. What a curve needs on top of that is a
// SECOND, independent mapping for the vertical axis, and this is it.
//
// ★ EVERYTHING BELOW IS FREE FUNCTIONS OVER NUMBERS, for the reason the pose
// ring geometry above is: the part that is easy to get silently wrong — which
// way up the value axis runs, what a handle pixel means as a derivative — is
// then catchable by a headless unit with no frame, no clip and no rig. The panel
// supplies the view and nothing else.
//
// ★ AND NO FUNCTION HERE RETURNS A NaN, whatever it is handed. These results go
// straight into ImGui draw-list coordinates, where a NaN is an assertion in a
// windowed build and a corrupt vertex buffer in one without — the same rule, and
// the same reason, as Zenith_AnimTimelineMath's.
//=============================================================================

// Vertical zoom limits, in PIXELS PER UNIT of the curve's own value. The range
// is wide because the values are: a scale track lives in [0, 2] and a position
// track on a large rig runs to hundreds of centimetres, and one clamp pair has
// to leave both usable. Both ends are named because three places read them (the
// wheel handler, the clamp and fit-to-selection), and a second opinion about the
// floor would let a view exist that the mapping refuses to invert.
constexpr float fANIM_CURVE_MIN_PPU = 0.01f;
constexpr float fANIM_CURVE_MAX_PPU = 20000.0f;
constexpr float fANIM_CURVE_DEFAULT_PPU = 60.0f;

// How long a tangent handle is drawn, IN SECONDS. A fixed time rather than a
// fixed pixel length, because a tangent IS a velocity (units per second, and
// radians per second for rotation — Flux/MeshAnimation/CLAUDE.md): at a fixed
// time offset the handle's vertical extent is literally "how far this key's
// slope would carry the value in 0.15 s", which is the quantity being edited. A
// pixel-length handle would instead mean something different at every zoom.
constexpr float fANIM_CURVE_HANDLE_SECONDS = 0.15f;

// A position/scale/rotation curve is drawn as THREE component curves; this is
// how many, and the index a component accessor takes (0 = x, 1 = y, 2 = z).
constexpr u_int uANIM_CURVE_COMPONENT_COUNT = 3u;

// How many tracks the curve view will draw when NOTHING is selected. A curve is
// sampled per pixel column through the real channel sampler, so "every track in
// the clip" is a per-frame cost proportional to the rig; with a selection the
// cap does not apply, because the selection is the user saying which ones.
constexpr u_int uANIM_CURVE_MAX_UNSELECTED_TRACKS = 8u;

//-----------------------------------------------------------------------------
// The VALUE axis of one curve view. Four floats, copyable, comparable field by
// field — the panel owns one and the tests build them by hand, exactly like
// Zenith_AnimTimelineView.
//-----------------------------------------------------------------------------
struct Zenith_AnimCurveValueView
{
	// The value at the TOP edge of the curve area. The top rather than the centre
	// because the pixel axis grows downward from a known top edge, so the mapping
	// is one subtraction with no half-height term to get the sign of wrong.
	float m_fValueAtTop = 1.0f;

	// Vertical zoom. Read through Zenith_AnimCurveEffectivePixelsPerUnit, which
	// clamps into [fANIM_CURVE_MIN_PPU, fANIM_CURVE_MAX_PPU] and substitutes the
	// floor for a non-finite or non-positive value.
	float m_fPixelsPerUnit = fANIM_CURVE_DEFAULT_PPU;

	// Screen y of the curve area's top edge, and its height. The height MAY BE
	// ZERO — a collapsed panel — and that is a legal state meaning "nothing is
	// visible", not an error.
	float m_fTopPixel = 0.0f;
	float m_fHeightPixels = 0.0f;
};

// The zoom the mapping ACTUALLY uses. Public because a panel drawing a zoom
// readout must show the number the mapping used, and because it is the one place
// the sanitisation lives.
float Zenith_AnimCurveEffectivePixelsPerUnit(const Zenith_AnimCurveValueView& xView);

//-----------------------------------------------------------------------------
// The mapping and its inverse.
//
//   pixel = top + (valueAtTop - v) * ppu        (y grows DOWN, value grows UP)
//   v     = valueAtTop - (pixel - top) / ppu
//
// A non-finite value maps to a pixel a million units outside the area (culled by
// every visibility test, clipped by every draw) rather than to a NaN; a
// non-finite pixel maps back to the value at the top edge.
//-----------------------------------------------------------------------------
float Zenith_AnimCurveValueToPixel(const Zenith_AnimCurveValueView& xView, float fValue);
float Zenith_AnimCurvePixelToValue(const Zenith_AnimCurveValueView& xView, float fPixelY);

// Force the view into a legal state: finite top and height (a negative or
// non-finite height becomes 0), zoom inside the clamps, finite value-at-top.
// NEVER produces a NaN, for any input — that is the whole point of it existing.
void Zenith_AnimCurveClamp(Zenith_AnimCurveValueView& xView);

// Fit [fMinValue, fMaxValue] into the area with a 10% margin at each end. A
// degenerate range (equal, inverted or non-finite bounds) is centred at the
// default zoom rather than dividing by zero — "one flat curve" is a legal thing
// to fit, and it has no extent to scale to.
void Zenith_AnimCurveFitRange(Zenith_AnimCurveValueView& xView, float fMinValue, float fMaxValue);

//-----------------------------------------------------------------------------
// A tangent HANDLE, both ways round. These two are exact inverses of each other
// and that is what a unit asserts: a handle drawn at a pixel, read back from
// that pixel, is the tangent it was drawn from.
//
// ★ THE HANDLE'S TIME OFFSET IS SIGNED BY WHICH END IT IS. The out handle sits
// at (t + h, v + m*h) and the in handle at (t - h, v - m*h), so BOTH lie on the
// line through the key with slope m — which is what makes the inverse below one
// expression for both: m = (value(pixelY) - v) / (time(pixelX) - t), with the
// two sign flips cancelling on the in side.
//-----------------------------------------------------------------------------
void Zenith_AnimCurveHandlePixel(const Zenith_AnimTimelineView& xTimeView,
	const Zenith_AnimCurveValueView& xValueView, float fKeyTimeSeconds, float fKeyValue,
	float fTangent, bool bIn, float fHandleSeconds, float& fOutPixelX, float& fOutPixelY);

// The tangent a handle dropped at (fPixelX, fPixelY) means, in value units per
// second. The time offset is taken from the PIXEL rather than assumed to be
// fHandleSeconds, so dragging a handle sideways changes the lever the same way
// it does in every other curve editor — but its magnitude is floored at
// fANIM_CURVE_MIN_HANDLE_SECONDS, because a handle dragged onto its own key's
// column would otherwise divide by zero and produce an infinite slope.
//
// ★ THE SIGN CONVENTION IS THE HANDLE'S, NOT THE CURSOR'S: a drop on the WRONG
// SIDE of the key (an out handle left of it) yields a tangent of the same
// magnitude and the slope the line through both points has, which is what a user
// sees. Nothing is refused here — refusing mid-drag would freeze the handle with
// no explanation.
float Zenith_AnimCurveTangentFromPixel(const Zenith_AnimTimelineView& xTimeView,
	const Zenith_AnimCurveValueView& xValueView, float fKeyTimeSeconds, float fKeyValue,
	bool bIn, float fPixelX, float fPixelY);

// The shortest lever a handle drag is allowed to have, in seconds. Not a
// tolerance to tune: it is the divisor's floor, and it exists so a drag onto the
// key's own column produces a very steep tangent rather than an infinite one.
constexpr float fANIM_CURVE_MIN_HANDLE_SECONDS = 1.0e-3f;

//-----------------------------------------------------------------------------
// THE DISPLAYED TANGENT MODE IS THE STORED ONE, PER END (B3). There is no
// second, narrower editor enum: the clip stores a four-valued Flux_TangentMode on
// each end of each key (Flux/MeshAnimation/CLAUDE.md → *Tangent sampling*), the
// mode bytes are on the wire from schema 3 (B2), and the display names exactly
// that value.
//
// ★ THE PROJECTION ONTO TWO VALUES IS GONE, AND WITH IT THE ONE THING IT COULD
// NOT SAY. Zenith_AnimCurveTangentMode had LINEAR and CUSTOM, so a FLAT key and a
// hand-dragged one displayed as the same word and an AUTO key — which the document
// now MAINTAINS across a retime — displayed as neither. A control that sets a mode
// has to be able to read one back, or the user cannot see what they just chose.
//-----------------------------------------------------------------------------

// The label the UI shows, over the WIRE enum, as a TOTAL switch. ONE definition,
// so the toolbar, the tooltip and the units cannot disagree — and so "Flat" cannot
// be typed in by accident beside a mode that is not one.
const char* Zenith_AnimCurveTangentModeLabel(Flux_TangentMode eMode);

// PURE: the mode a stored pair displays for ONE end.
//
// ★ BOTH ANSWERS ONLY WHEN THE TWO ENDS AGREE, and reports "mixed" through the
// BOOL rather than through a fifth enum value. Since B2 a key can legally be
// IN=LINEAR / OUT=FLAT, and a single four-valued answer for such a key would have
// to lie about one of its ends; a display that cannot say "these two differ" would
// invent a mode the file does not contain. False leaves eOut untouched.
bool Zenith_AnimCurveTangentModeOf(const Flux_KeyTangents& xTangents,
	Zenith_AnimTangentEnd eEnd, Flux_TangentMode& eOut);

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

	// The editor's single panel.
	//
	// ★ IT IS OWNED BY Zenith_Editor, NOT BY A FUNCTION-LOCAL STATIC, AND THAT IS A
	// LIFETIME FIX RATHER THAN A STYLE ONE. A static's destructor runs at ATEXIT —
	// long after Zenith_AssetRegistry::Shutdown has force-deleted every asset — so
	// any owning handle the panel's session or document still held was Released into
	// freed memory. Zenith_Editor::Shutdown already called Shutdown() below to close
	// the clip while the registry was alive, but that only covered what CloseClip
	// reaches; anything the panel acquired outside it (the preview controller's
	// skeleton handle was the live case) still died at exit. There is no atexit
	// window now: the object is new'd in Zenith_Editor::Initialise and deleted in
	// Zenith_Editor::Shutdown, both inside the registry's lifetime.
	//
	// Instance() is unchanged for every call site — it resolves the editor-owned
	// object — and a unit can still build its OWN panel on the stack and drive it in
	// isolation. Calling it before Initialise or after Shutdown asserts.
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
	// Called from Zenith_Editor::Shutdown immediately before it DELETES this object,
	// and both run BEFORE Zenith_AssetRegistry::Shutdown. Keeping the two adjacent is
	// the point: Shutdown() is the explicit release of what CloseClip reaches, and
	// the delete is what guarantees everything else goes with it — the panel used to
	// survive to atexit and release its remaining handles into a dead registry.
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
	// Save refuses external conflicts; SaveAs writes a separate document path.
	bool Action_Save();
	bool Action_SaveAs(const std::string& strAssetPath);

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
	// THE IK TARGET WIDGET (E1) — the thing that AIMS the verb above.
	//
	// ★ IT IS THE SECOND MANIPULATOR OVER ONE PREVIEW PANE, AND THE TWO EXCLUDE
	// EACH OTHER. The rings turn ONE bone about its own joint; this drags a
	// POINT and lets the solver decide what three bones do to reach it. Both
	// latch state at press and rewrite the live pose from that latch every
	// frame, and both open the session's drag bracket — so a press that started
	// one while the other was live would have two owners for one bone, one
	// mouse-up and one latched value belonging to neither. Each Begin refuses
	// while the other is active, which is one line in each and the only place
	// the rule can be enforced.
	//
	// ★ AND THE RELEASE BAKES WHETHER OR NOT AUTO-KEY IS ON, which is the one
	// place this drag deliberately differs from the ring's. The verb it aims —
	// Action_BakeIKForSelectedChain, the whole of design note §6 — is defined as
	// "solve, then bake down to keys"; it keys unconditionally, and there is no
	// second, key-less IK path for an auto-key-off release to take. A cancel
	// still writes nothing at all.
	//
	// Every verb here is pixel-based and reads no ImGui state, like the ring's,
	// so a unit performs the whole gesture without synthesising input.
	//------------------------------------------------------------------------

	// The target the widget is currently aiming at, in MODEL space — the same
	// space Action_BakeIKForSelectedChain takes and Zenith_AnimationPoseIK
	// solves in. False when there is no selection to have seeded one from.
	//
	// ★ AT REST IT IS THE EFFECTOR'S OWN MODEL-SPACE JOINT, re-taken every frame
	// a drag does not own it — so the handle sits exactly under the bone it
	// moves and the first pixel of a drag is the first pixel of a solve. A
	// target seeded once and left would sit where the bone USED to be after any
	// seek, undo or ancestor edit, and the next grab would snap the chain back
	// to it. Only a live drag moves it away from the joint, and the bake on
	// release puts the joint back under it.
	bool GetIKTargetModelSpace(Zenith_Maths::Vector3& xOut) const;

	// Where that target projects to, as a pixel RELATIVE to the preview image's
	// top-left — the space Action_BeginIKDragAtPixel consumes. False without a
	// selection, without a seeded target, without a rendered frame, or when the
	// target is behind the preview camera.
	bool GetIKHandlePixel(float& fOutPixelX, float& fOutPixelY) const;

	// Grab the handle. Refused when the pixel is more than
	// fANIM_POSE_RING_GRAB_PIXELS from it, when no bone is selected, when the
	// chain cannot be built (a ROOT effector has nothing above it to bend),
	// without a rendered frame, or while EITHER manipulator is already dragging.
	//
	// Latches the chain's bone-local rotations and opens the session's drag
	// bracket on the effector, which is what suspends clip evaluation for the
	// length of the gesture.
	bool Action_BeginIKDragAtPixel(float fPixelX, float fPixelY);

	// Move the target to the pixel and re-solve. ★ THE LATCH IS RESTORED FIRST,
	// EVERY TIME: Zenith_AnimationPoseIK seeds from the live TRS, so solving
	// from the previous solve's output would COMPOSE and the pose reached would
	// depend on how many mouse moves the drag happened to span. False when no IK
	// drag is in flight.
	bool Action_UpdateIKDragToPixel(float fPixelX, float fPixelY);

	// Release. A drag that never left the dead zone writes nothing; otherwise
	// ONE Action_BakeIKForSelectedChain at the target, which is one compound and
	// one Ctrl+Z. True iff a drag was in flight.
	bool Action_EndIKDrag();

	// Escape: put the whole chain back where the drag found it, close the
	// bracket and forget the moved target. Nothing reaches the document, so
	// there is nothing to undo.
	bool Action_CancelIKDrag();

	// Live IK state, so a test can tell "the handle was never grabbed" apart
	// from "the drag ran and the solve refused".
	bool IsIKDragActive() const { return m_bIKDragActive; }
	// How many bones the live drag latched — the chain the solve is moving.
	// Zero when no drag is in flight.
	u_int GetIKDragChainLength() const { return m_bIKDragActive ? m_auIKDragChainBones.GetSize() : 0u; }
	// Was the handle PAINTED last frame? Ungated, for the reason every other
	// draw diagnostic on this panel is: "nothing was drawn" and "something was
	// drawn in the wrong place" are different failures and a rect cannot tell
	// them apart when there is no rect. False with no selection, which is this
	// panel's standing "nothing selected draws NOTHING" rule.
	bool WasIKHandleDrawnLastFrame() const { return m_bIKHandleDrawn; }

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
	//
	// ★ THE FLAG BEHIND THIS IS WRITTEN BY FOUR FUNCTIONS AND NO OTHERS:
	// Action_BeginBoneDragAtPixel raises it, Action_EndBoneDrag and
	// Action_CancelBoneDrag lower it, and CancelAllPoseGestures lowers it on
	// behalf of the six paths that end a gesture with no mouse-up (see that
	// helper). Anything else clearing it directly would leave the SESSION's drag
	// bracket open with nothing left to close it, which suspends clip evaluation
	// for good.
	bool IsBonePoseDragActive() const { return m_bPoseDragActive; }
	u_int GetPoseDragAxis() const { return m_bPoseDragActive ? m_uPoseDragAxis : uINVALID_ANIM_POSE_RING; }
	// The ACCUMULATED, UNSNAPPED angle in radians. The applied one is this
	// rounded by Action_GetPoseAngleSnap; both are worth seeing separately when a
	// snapped drag looks like it did nothing.
	float GetPoseDragAngleRadians() const { return m_fPoseDragAngleRadians; }
	u_int GetPoseHoverRingAxis() const { return m_uPoseHoverAxis; }

	//=========================================================================
	// BONE MASKS (WU-7.1) — the ".zanimmask" sub-panel.
	//
	// ★ IT LIVES INSIDE THE DOPE SHEET BECAUSE THE RIG DOES. A mask is authored
	// against a specific skeleton's bone NAMES, and the only place the editor
	// already has a resolved, previewed rig in hand is this panel's
	// Zenith_AnimationPreviewSession. A standalone mask window would have to
	// grow its own rig resolution, its own preference store and its own prompt,
	// all of which exist here and are already unit-tested.
	//
	// ★ THE DOCUMENT IS A SECOND, INDEPENDENT ONE with its OWN undo stack. A
	// mask edit and a keyframe edit are not one editing session — Ctrl+Z in the
	// sheet must not take back a slider drag in the mask list, and the two
	// documents save different files.
	//
	// ★ AND THE SUB-PANEL REFUSES TO OFFER AN ASSIGNMENT CONTROL FOR AN ADDITIVE
	// LAYER. An additive layer ignores its mask entirely (see
	// Zenith_BoneMaskDocument::LayerAcceptsMask), so drawing one would let a user
	// author a mask, save it, assign it, and observe nothing — with every gate
	// green. The layer LIST is WU-7.2's; what this panel holds is which blend
	// mode the mask would be assigned INTO, so the refusal is expressible now.
	//=========================================================================

	Zenith_BoneMaskDocument& MaskDocument() { return m_xMaskDocument; }
	const Zenith_BoneMaskDocument& MaskDocument() const { return m_xMaskDocument; }

	// ★★ OFF BY DEFAULT, AND WHILE IT IS OFF THE SECTION DRAWS NOTHING — not a
	// collapsed header, not a label, not one item. That is this panel's standing
	// rule (Editor/CLAUDE.md) and it is load-bearing: RenderSheet is sized from
	// ImGui::GetContentRegionAvail(), so every item emitted above it comes out of
	// the sheet's height, and the EVENTS ROW IS THE LAST ROW OF THE SHEET. A
	// single always-present collapsed header (~24 px) was enough to push the
	// events row below the canvas bottom in a 900x600 window and turn
	// ChangingTheDurationMovesTheEventRowAndNotTheStoredValue red with
	// GetEventRect false on BOTH sides of the edit — a failure that reads as "the
	// events row is broken" and is nowhere near its cause.
	//
	// The toggle is a checkbox on the toolbar's EXISTING first row, which costs
	// no height. Action_MaskOpen / Action_MaskOpenFresh raise it and
	// Action_MaskClose clears it, so it always agrees with what is on screen.
	bool& ShowMaskSectionFlag() { return m_bShowMaskSection; }
	bool IsMaskSectionShown() const { return m_bShowMaskSection; }

	// Which layer a mask assignment from this sub-panel would target. WU-7.2
	// owns the layer list and will drive this from the selected row; until then
	// it is panel state with an OVERRIDE default, because that is the blend mode
	// a mask means something to.
	void SetMaskTargetLayerBlendMode(Flux_LayerBlendMode eBlendMode) { m_eMaskTargetBlendMode = eBlendMode; }
	Flux_LayerBlendMode GetMaskTargetLayerBlendMode() const { return m_eMaskTargetBlendMode; }

	//------------------------------------------------------------------------
	// The Action_* twins. Same three rules as every other action on this panel:
	// bool-returning, reading no ImGui state, and going through a DOCUMENT verb.
	//------------------------------------------------------------------------

	// Opens strAssetPath into the mask document and SHOWS the section, so a
	// refusal is visible rather than reported to a collapsed header.
	bool Action_MaskOpen(const std::string& strAssetPath);
	// The regenerate-from-scratch entry point, the twin of the controller
	// panel's OpenAssetFresh and separate for the same reason.
	bool Action_MaskOpenFresh(const std::string& strAssetPath);
	// One bone's weight, clamped to [0,1] by the document. ASSIGNMENT: true when
	// the weight is what you asked for, whether or not this call changed it.
	bool Action_MaskSetWeight(const std::string& strBoneName, float fWeight);
	// strBoneName and every DESCENDANT of it, as ONE undo step. Resolved against
	// the SESSION'S rig — false when there is no rig, which is the same state
	// the sub-panel shows its prompt for.
	bool Action_MaskSetSubtree(const std::string& strBoneName, float fWeight);
	bool Action_MaskSetHasAvatar(bool bHasAvatarMask);
	bool Action_MaskSave();
	// FORCED close: unsaved mask edits are discarded, matching CloseClip.
	bool Action_MaskClose();
	bool Action_MaskUndo();
	bool Action_MaskRedo();

	//------------------------------------------------------------------------
	// Mask sub-panel diagnostics — UNGATED, for the reason every other
	// diagnostic here is. "The control was not drawn" and "the control was drawn
	// and did nothing" are different failures and a bool cannot tell them apart.
	//------------------------------------------------------------------------

	// Did the "Bone Masks" section draw at all last frame? False when the panel
	// is hidden, when the section is toggled off (which is its DEFAULT, and
	// means it emitted zero items and consumed zero height), or when the window
	// was not drawn.
	bool WasMaskSectionDrawnLastFrame() const { return m_bMaskSectionDrawn; }
	// Was the ASSIGNMENT control drawn? This is the one the additive-layer rule
	// gates: false, with GetMaskNotice() explaining, for an additive target.
	bool WasMaskAssignmentDrawnLastFrame() const { return m_bMaskAssignmentDrawn; }
	// How many per-bone weight rows the section listed. Zero means there was no
	// rig to list, which the section says out loud rather than drawing an empty
	// list.
	u_int GetMaskBoneRowCount() const { return m_uMaskBoneRowsDrawn; }
	// Whatever the section is currently explaining: the additive-layer notice,
	// the no-rig prompt, the no-document prompt, or empty when it is just
	// listing bones. Never null.
	const char* GetMaskNotice() const { return m_strMaskNotice.c_str(); }

	// The bones the sub-panel would list, from the SESSION's rig, in skeleton
	// order. Empty without a resolved rig. Exposed so a unit (and WU-7.2) reads
	// the same list the section draws rather than rebuilding it.
	void GetMaskRigBoneNames(Zenith_Vector<std::string>& axOut) const;

	//=========================================================================
	// THE CURVE VIEW (WU-8.2) — per-key tangent handles over the SAME timeline.
	//
	// ★ IT REPLACES THE ROW AREA; IT DOES NOT SIT ABOVE IT. The toggle is a
	// "Curves" checkbox on the toolbar's EXISTING first row (beside "Masks"),
	// and when it is on the sheet's canvas draws curves where it drew rows —
	// same InvisibleButton, same ruler, same playhead, same Zenith_AnimTimeline
	// X mapping. That is this panel's standing height rule taken to its
	// conclusion: a curve editor drawn as a SECOND strip would cost the sheet
	// every pixel it occupied, and the events row is the sheet's last row.
	// While the toggle is off, nothing of the curve view is drawn and no curve
	// rect is recorded; while it is ON, the dope sheet's row / key / event rects
	// are not recorded, because those rows were not painted.
	//
	// ★ THE SELECTION IS SHARED, AND SURVIVES THE TOGGLE. There is one key
	// selection — the same (track, key id) set — and switching views changes
	// which rects it is hit-tested against, nothing else. A separate curve
	// selection would mean an Auto applied in one view acted on a set the other
	// view was not showing.
	//
	// ★ WHAT IS DRAWN IS SAMPLED THROUGH THE REAL CHANNEL SAMPLER, one sample
	// per pixel column, so the curve on screen is the curve that plays. A
	// re-derivation of the Hermite here would agree with Flux_BoneChannel right
	// up until one of them changed, and a curve editor that lies about the shape
	// is worse than none.
	//
	// ★ A ROTATION CURVE IS DRAWN AS EULER ANGLES IN RADIANS, AND ITS HANDLE IS
	// AN ANGULAR VELOCITY. The clip stores rotation tangents as body-frame
	// angular velocity in axis * rad/s (Flux/MeshAnimation/CLAUDE.md), so the
	// value axis is radians and the handle slope is rad/s — consistent units,
	// and the same handle arithmetic as position. It is NOT exact: an Euler
	// angle's rate and a body-frame angular velocity component agree for a
	// rotation about one axis and diverge as the other two wind up, so a handle
	// on a tumbling rotation moves the curve by less (or more) than its drawn
	// slope suggests. Stated rather than hidden: the alternative is three
	// separate quaternion-derivative curves nobody can read.
	//
	// ★ ROOT MOTION IS NOT DRAWN HERE AT ALL. Its two delta tracks carry no
	// tangent array (D17) and are still sampled linearly after WU-8.1, so a
	// handle on one would be a control with nothing behind it.
	//=========================================================================

	bool IsCurveViewShown() const { return m_bShowCurveView; }
	// Unified: dragging one handle writes the OTHER to the same value, which is
	// what keeps a key smooth. Broken (false) edits one side only — the corner.
	bool AreTangentsUnified() const { return m_bCurveTangentsUnified; }

	// The live value axis. m_fTopPixel / m_fHeightPixels are OVERWRITTEN by every
	// Render from the canvas geometry; the zoom and the value-at-top are the
	// panel's own state and survive, exactly as the timeline view's are.
	const Zenith_AnimCurveValueView& CurveValueView() const { return m_xCurveValueView; }
	void SetCurveValueView(const Zenith_AnimCurveValueView& xView) { m_xCurveValueView = xView; }

	// The STORED mode of one END of one key (B3). False when the key does not
	// resolve, on a root-motion track (which has no tangents to have a mode), and
	// — for ZENITH_ANIM_TANGENT_END_BOTH — when the two ends carry DIFFERENT
	// modes. All three are "there is no single mode to name here"; a caller that
	// needs to tell them apart asks each end separately, which is what the toolbar
	// does.
	bool GetKeyTangentMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
		Zenith_AnimTangentEnd eEnd, Flux_TangentMode& eOut) const;

	//------------------------------------------------------------------------
	// Curve hit rects — the SAME off-screen contract as every other rect on this
	// panel: recorded only when painted inside the canvas this frame, judged
	// against the display bound captured at record time, and refused rather than
	// handed out when the centre falls outside it.
	//------------------------------------------------------------------------

	// The curve area: the key lane, below the ruler. Recorded ONLY while the
	// curve view is shown, which is what a unit reads to tell "the view is off"
	// from "the view is on and drew nothing".
	bool GetCurveViewRect(Zenith_AnimPanelRect& xOut) const;
	// One key's point on one of its three component curves.
	bool GetCurveKeyRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId, u_int uComponent,
		Zenith_AnimPanelRect& xOut) const;
	// One end of one component's tangent handle.
	bool GetCurveHandleRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId, u_int uComponent, bool bIn,
		Zenith_AnimPanelRect& xOut) const;

	// How many TRACKS the curve view drew last frame, and how many curve POINTS
	// it recorded. Ungated diagnostics, for the reason every other one here is:
	// a flat `false` from a rect accessor has four causes and the bool names none.
	u_int GetDrawnCurveTrackCount() const { return m_uCurveTracksDrawn; }
	u_int GetRecordedCurvePointCount() const { return m_xCurveKeyRects.GetSize(); }

	// Were the per-END mode combos emitted last frame (B3)? The mode control is
	// the one curve control with nothing to measure — it records no rect and costs
	// no height — so its ABSENCE has to be readable directly rather than inferred
	// from geometry that would look identical either way. False while the curve
	// view is off, with no clip open, with no selected key to name a mode for, and
	// for a frame in which this panel drew nothing at all.
	//
	// The "drew nothing at all" half is the sheet's own flag rather than a second
	// clear: RenderCurveToolbarItems is what resets this, and a hidden or collapsed
	// panel never reaches it — so a stale true is filtered here, where the two
	// facts are both in hand.
	bool WasCurveModeControlDrawnLastFrame() const { return m_bCurveModeControlDrawn && m_bCanvasRectValid; }

	// The tracks the curve view WOULD draw, in row order: the ones the key
	// selection names, or — with an empty selection — every bone track carrying a
	// key, capped at uANIM_CURVE_MAX_UNSELECTED_TRACKS. Exposed so a unit and the
	// draw read one list rather than two that can drift.
	void GetCurveTracks(Zenith_Vector<Zenith_AnimTrackId>& axOut) const;

	//------------------------------------------------------------------------
	// The Action_* twins. Same three rules as every other action on this panel:
	// bool-returning, reading no ImGui state, every mutation through a DOCUMENT
	// verb.
	//
	// ★ THE TWO TOGGLES ARE ASSIGNMENTS — true means "the value you asked for
	// is in place", whether or not this call changed it — which is the
	// animator-controller panel's rule and NOT Action_SetAutoKey's. That is
	// deliberate: it means the ANIM_CURVE_* automation family can be checked
	// wholesale, with no exception list for a later verb to be forgotten from.
	// The invariant to assert on is the undo-stack DEPTH.
	//------------------------------------------------------------------------

	bool Action_SetCurveView(bool bShow);
	bool Action_SetTangentsUnified(bool bUnified);

	// Both tangents of one key, as ONE undo step. Refused for a key that does not
	// resolve and for a root-motion track.
	bool Action_SetKeyTangents(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
		const Zenith_Maths::Vector3& xInTangent, const Zenith_Maths::Vector3& xOutTangent);

	// The selection, as ONE compound each. Auto is per-key Catmull-Rom (the
	// centred slope through each key's own neighbours); Linear zeroes both halves,
	// which is the sampler's linear branch and NOT a flat handle.
	//
	// Root-motion keys in the selection are SKIPPED rather than refusing the
	// operation: a mixed selection is ordinary, and a user who box-selected across
	// the root-motion rows did not ask for the whole gesture to fail.
	bool Action_SetSelectionTangentsAuto();
	bool Action_SetSelectionTangentsLinear();

	//------------------------------------------------------------------------
	// THE MODE VERBS (B3) — one END of one key, and the same end across the
	// selection.
	//
	// ★ THEY GO THROUGH Zenith_AnimationDocument::SetKeyTangentMode AND NOTHING
	// ELSE. Action_SetKeyTangents writes CUSTOM on BOTH ends (it is a vector
	// edit), and SetKeyTangentsAuto forces AUTO on both — so routing a per-end
	// mode through either would silently rewrite the end the user did not name.
	//
	// ★ AND THE RETURN IS THE DOCUMENT'S ASSIGNMENT CONTRACT, not the selection
	// verbs' bAnyResolved: true means "the mode you asked for is on that end",
	// whether or not this call changed it, so re-stating a mode SUCCEEDS and
	// pushes nothing. The invariant to assert on is the undo-stack DEPTH.
	//------------------------------------------------------------------------
	bool Action_SetKeyTangentMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
		Zenith_AnimTangentEnd eEnd, Flux_TangentMode eMode);
	// The selection, as ONE compound — root-motion keys SKIPPED, exactly as the
	// two preset verbs above skip them. False when nothing in the selection could
	// take the mode.
	bool Action_SetSelectionTangentMode(Zenith_AnimTangentEnd eEnd, Flux_TangentMode eMode);

	// ★ THE DRAG VERB, AND IT COMMITS. One call is one undo step, so the pointer
	// handler calls it exactly ONCE — on release — and previews the intermediate
	// positions as a ghost handle drawn from panel state. That is this panel's
	// "preview, then commit" shape (the key drag, the duration handle, the event
	// drag) and it is what keeps a Ctrl+Z from walking back through positions the
	// user was only passing through.
	//
	// (fX, fY) are ABSOLUTE SCREEN coordinates, the space every rect on this panel
	// is recorded in. Needs a rendered frame: the mapping's pixel origin comes
	// from the canvas geometry. With AreTangentsUnified() the opposite handle is
	// written to the same value in the same step.
	bool Action_DragTangentHandleToPixel(const Zenith_AnimTrackId& xTrack, u_int uKeyId, u_int uComponent,
		bool bIn, float fX, float fY);

	// Fit the VALUE axis to what is on screen: the sampled extent of the curves
	// the view is drawing, across the visible time range. False without a rendered
	// frame, without the curve view shown, or when there is nothing to fit.
	bool Action_FitCurveViewToSelection();

	//------------------------------------------------------------------------
	// Live curve-drag state, so a test can tell "the handle was never grabbed"
	// apart from "the drag ran and produced no tangent".
	//------------------------------------------------------------------------
	bool IsDraggingTangentHandle() const { return m_bCurveHandleDragActive; }

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
	// (row, key, component) and (row, key, component, end) packed into one word,
	// the same trick MakeKeyRectKey plays and for the same reason: a hash keyed on
	// a string built per lookup would allocate once per curve point per frame.
	static u_int64 MakeCurveRectKey(u_int uRowIndex, u_int uKeyId, u_int uComponent, bool bIn);

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
	// WU-7.1's "Bone Masks" section: the path field, Open / Save / Close, the D47
	// checkbox, and one weight slider per bone of the session's rig. Ordinary
	// ImGui items laid out by the cursor, like the toolbars and unlike the sheet
	// — it is not a draw-list decoration and records no rects.
	//
	// ★ RETURNS BEFORE SUBMITTING ANYTHING when the section is off, which is the
	// default. See ShowMaskSectionFlag: every item above RenderSheet is height
	// the sheet does not get, and the events row is the sheet's last row.
	void RenderMaskSection();
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
	// The IK handle's half of the same contract, and it runs SECOND: the rings
	// are the finer target (an 8 px band round a polyline) and the handle sits
	// at their shared centre, so asking the rings first costs the handle
	// nothing and keeps "the gizmo moved the camera" impossible for both.
	bool HandleIKTargetInput(bool bImageHovered);
	void DrawIKTargetHandle(ImDrawList* pxDraw);
	// Set Key / Auto-key / Angle snap. Its own toolbar LINE, for the reason
	// RenderEventToolbar has one: the first row already runs wider than a 900 px
	// window and a SameLine past the edge is a control nobody can reach.
	void RenderPoseToolbar();
	// Turn m_fPoseDragAngleRadians (snapped or not) into the live pose. ★ ALWAYS
	// FROM THE LATCHED INITIAL ROTATION, never from the previous frame's output:
	// one delta applied to a fixed value cannot drift, and feeding the output
	// back in would accumulate both the float error and the snap's rounding.
	void ApplyPoseDragAngle();

	//-------------------------------------------------------------------------
	// ★ THE ONE PLACE A LIVE MANIPULATOR GESTURE IS ABANDONED — both of them,
	// together — and the ONLY writer of m_bPoseDragActive / m_bIKDragActive
	// outside their own Begin/End/Cancel verbs.
	//
	// Six things end a gesture without a mouse-up, and until E1 not one of them
	// did anything about it: the panel being HIDDEN (Render's !m_bShow return
	// cleared seven sheet flags and neither drag flag), CloseClip,
	// OnDocumentOpened, a rig RE-RESOLVE, Action_SelectBone and
	// Action_ClearBoneSelection. The flag then stayed true forever and
	// Action_BeginBoneDragAtPixel refused every later grab — a manipulator that
	// silently stops working, with the pose, the clip and the undo stack all
	// healthy and nothing to see. Escape was the only cancel that existed.
	//
	// True iff something WAS cancelled, so a caller can report the change.
	//-------------------------------------------------------------------------
	bool CancelAllPoseGestures();

	// Put the IK target back on the selected bone's own model-space joint. Cheap
	// (one matrix read) and it does nothing while a drag is in flight, which owns
	// the target. Called once per rendered frame AND from the selection verbs, so
	// the handle is correct whether or not a frame has been drawn since.
	void SeedIKTargetFromSelection();

	// Write one bone-local rotation per latched chain bone into the LIVE pose,
	// root-first, ending in RefreshDerivedPose. ★ THE EFFECTOR GOES THROUGH THE
	// SESSION'S UpdateBoneDrag and its ancestors do not, which is not an
	// inconsistency: the session's drag bracket is open on the effector, and
	// UpdateBoneDrag is what raises the unkeyed-pose badge. Writing the
	// ancestors through it is impossible (it only ever writes m_uDragBoneIndex)
	// and writing the effector around it would lose the badge — the one thing a
	// user can silently lose.
	void ApplyIKChainRotations(const Zenith_Vector<Zenith_Maths::Quat>& axLocalRotations);
	//-------------------------------------------------------------------------
	// The curve view — Zenith_EditorPanel_Animation_Curve.cpp. Drawing, input
	// translation and the accessors, beside the pure mapping they all use.
	//-------------------------------------------------------------------------

	// Paints the curves, their key points and their tangent handles into the
	// canvas's ROW REGION, and records the curve rects. Called INSTEAD of DrawRows
	// while the curve view is shown, which is why nothing here has to be undone
	// when it is not: the rows were never painted, so their rects were never
	// recorded.
	void DrawCurveView(ImDrawList* pxDraw, const SheetLayout& xLayout);
	// The handle drag and the point click. The ONLY curve function that reads
	// ImGui state, and every branch ends in an Action_*.
	//
	// ★ RETURNS TRUE WHEN IT OWNS THIS FRAME'S GESTURE, which is what keeps a
	// press on a handle from ALSO reaching HandleSheetInput and being read there
	// as a click on empty space — i.e. as "clear the selection". Same shape, same
	// reason, as HandlePoseManipulatorInput's return.
	bool HandleCurveInput(const SheetLayout& xLayout, bool bCanvasHovered);
	// The sampled extent of everything the curve view is drawing, across the
	// VISIBLE time range. False when there is nothing drawn to measure.
	bool ComputeCurveValueRange(float& fOutMin, float& fOutMax) const;
	// "Curves", "Auto", "Linear", "Unified", "Fit Curves" and B3's two per-END
	// mode combos — all drawn on the toolbar row that already exists, so the curve
	// view costs the sheet no height at all.
	//
	// ★ THE MODE COMBOS REPLACED THE "tangents: %s" READOUT rather than being
	// added beside it, and that is a height rule and not a tidiness one: this row
	// already runs wider than a 900 px window, a new toolbar LINE comes straight
	// out of the sheet's canvas, and the events row is the sheet's last row. A
	// control that shows a mode and a control that sets one are the same control.
	void RenderCurveToolbarItems();
	// ONE end's mode box, drawn twice by the function above. Emits nothing for a
	// key that cannot have a mode, which is what makes
	// WasCurveModeControlDrawnLastFrame a real answer rather than a constant.
	void RenderCurveTangentModeCombo(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
		Zenith_AnimTangentEnd eEnd);
	// The value the curve view plots for one component of one track at one time,
	// sampled THROUGH the channel's own sampler. The single definition the draw,
	// the key points and the fit all read.
	bool SampleCurveValue(const Zenith_AnimTrackId& xTrack, u_int uComponent, float fTimeSeconds,
		float& fOutValue) const;
	// One key's plotted value — the same quantity, at the key's own time, read
	// from the stored key rather than sampled, so a point sits exactly on its key.
	bool GetCurveKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, u_int uComponent,
		float& fOutValue) const;
	// The curve area, as of the last sheet pass: the row region of the canvas.
	void UpdateCurveValueViewGeometry(const SheetLayout& xLayout);
	// The (track, key, component, end) under a screen pixel, if any. Handles are
	// tested BEFORE points, because a handle at a very short lever overlaps its own
	// key and the handle is the thing the user is reaching for.
	bool FindCurveHandleAtScreenPos(float fX, float fY, Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId,
		u_int& uOutComponent, bool& bOutIn) const;
	bool FindCurvePointAtScreenPos(float fX, float fY, Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const;

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

	//-------------------------------------------------------------------------
	// The bone-mask sub-panel (WU-7.1). Its own document, its own undo stack,
	// its own dirty state — see the public block for why it is not folded into
	// the clip document.
	//-------------------------------------------------------------------------
	Zenith_BoneMaskDocument m_xMaskDocument;
	// ★ FALSE IS THE DEFAULT AND IT MEANS "DRAW NOTHING", not "draw collapsed" —
	// see ShowMaskSectionFlag above for the 24 px that cost a red test.
	bool m_bShowMaskSection = false;
	Flux_LayerBlendMode m_eMaskTargetBlendMode = LAYER_BLEND_OVERRIDE;
	// The section's path field. Fixed, because ImGui::InputText wants one.
	char m_acMaskPathBuffer[512] = {};
	// The subtree paint's target value, so "select subtree" is one click at the
	// value the toolbar shows rather than a second dialog.
	float m_fMaskSubtreeWeight = 1.0f;
	// Per-frame draw diagnostics, cleared by ClearFrameRects with everything
	// else so a frame the panel did not draw reports NOT DRAWN rather than last
	// frame's answer.
	bool m_bMaskSectionDrawn = false;
	bool m_bMaskAssignmentDrawn = false;
	u_int m_uMaskBoneRowsDrawn = 0;
	std::string m_strMaskNotice;

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

	// ★ The ImGui registration of the animation preview's OWN persistent LDR
	// (GetPreviewLDR(kuFluxViewSlotPreviewAnim) — not shared with the material
	// editor since D3, which gave each preview a slot and an LDR of its own), kept
	// as the raw handle value so this header does not have to pull Flux in.
	// Registered once and NEVER unregistered: the panel outlives the frame loop,
	// and unregistering from a static destructor would be a descriptor write after
	// the backend has gone. Same treatment, same reason, as the material editor's.
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

	//-------------------------------------------------------------------------
	// The IK target widget (E1). Same "preview, then commit" shape as the ring
	// drag — the live pose is the preview, and ONE bake runs on release.
	//-------------------------------------------------------------------------

	// The target, in MODEL space. Meaningless while m_uIKSeededForBone says no
	// selection has seeded it.
	Zenith_Maths::Vector3 m_xIKTarget = Zenith_Maths::Vector3(0.0f);
	// Which bone m_xIKTarget was seeded from, or kuINVALID_BONE_SELECTION. Both
	// the "is there a target" test and the "has the selection moved" test, as
	// one value, for the reason uINVALID_ANIM_POSE_RING is one value: a bool
	// beside it would give one fact two representations that can disagree.
	u_int m_uIKSeededForBone = kuINVALID_BONE_SELECTION;

	bool m_bIKDragActive = false;
	bool m_bIKDragMoved = false;
	// ★ WAS THE POSE ALREADY UNKEYED WHEN THIS DRAG STARTED — the same latch,
	// for the same reason, as m_bPoseWasUnkeyedAtDragStart: a cancel must put
	// the badge back the way it found it rather than hiding an EARLIER unkeyed
	// edit that is still in the pose.
	bool m_bIKWasUnkeyedAtDragStart = false;
	// Where the target was when the drag started. What a cancel restores, and
	// what the dead zone is measured from.
	Zenith_Maths::Vector3 m_xIKDragStartTarget = Zenith_Maths::Vector3(0.0f);
	// The press pixel, for the pixel half of the dead zone (see
	// fANIM_IK_DRAG_DEAD_ZONE_PIXELS).
	float m_fIKDragStartPixelX = 0.0f;
	float m_fIKDragStartPixelY = 0.0f;
	// The chain the press latched, ROOT FIRST (BuildChainFromEffector's order),
	// and one bone-local rotation per entry. ★ CAPTURED ONCE AT PRESS. Rebuilding
	// the chain per mouse move would re-read a pose the previous move wrote, so
	// the "restore the latch" below would restore the last solve rather than the
	// pre-drag pose and a slow drag would not reach the same place as a fast one.
	Zenith_Vector<u_int> m_auIKDragChainBones;
	Zenith_Vector<Zenith_Maths::Quat> m_axIKLatchedLocalRotations;
	// The drag plane, in WORLD space, frozen at press: the point is the latched
	// target and the normal faces the camera, so the target tracks the cursor
	// across the screen rather than sliding along the view ray. Frozen for the
	// same reason the ring drag freezes its axis — a plane that re-faced the
	// camera every frame would move under a target that had not been dragged.
	Zenith_Maths::Vector3 m_xIKDragPlanePoint = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xIKDragPlaneNormal = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	// Per-frame draw diagnostic, cleared by ClearFrameRects with every other one.
	bool m_bIKHandleDrawn = false;
	bool m_bIKHandleHovered = false;

	// The session rig generation this panel has already reconciled against. See
	// Zenith_AnimationPreviewSession::GetRigGeneration: a re-resolve invalidates
	// every latched rotation both manipulators are holding, and this is the only
	// thing the panel can observe it through.
	u_int m_uSeenRigGeneration = 0u;

	//-------------------------------------------------------------------------
	// The curve view (WU-8.2).
	//
	// ★ FALSE IS THE DEFAULT AND IT MEANS "DRAW NOTHING". Unlike the mask
	// section this costs the sheet no height either way — the curves are painted
	// INSIDE the canvas, in the region the rows would have used — but the rule
	// the flag serves is the same one: a view that is off draws zero items and
	// records zero rects, so a rect accessor answering false says "off" and not
	// "somewhere you cannot see".
	//-------------------------------------------------------------------------
	bool m_bShowCurveView = false;
	// TRUE by default: a smooth key is the common case, and a user who wants a
	// corner says so. (Nothing is stored per key — see the header block: the wire
	// has no mode field, so "unified" is a property of the EDITOR's gesture, not
	// of the clip.)
	bool m_bCurveTangentsUnified = true;
	Zenith_AnimCurveValueView m_xCurveValueView;
	// ★ "Fit the value axis" DEFERRED UNTIL THE CURVE AREA IS KNOWN, exactly as
	// m_bPendingFrameAll defers the horizontal fit and for the same reason:
	// Zenith_AnimCurveFitRange needs a height to divide by, and at the moment a
	// clip is opened (or the view is switched on) the panel has not been laid out.
	bool m_bPendingCurveFit = false;

	// ★ THE SAME "PREVIEW, THEN COMMIT" SHAPE as the key drag, the duration
	// handle and the event drag: while the button is down the dragged handle is
	// drawn as a ghost from the pixel below and NOTHING reaches the document, and
	// ONE Action_DragTangentHandleToPixel runs on release. A command per frame of
	// the drag would make every position the user passed through an undo stop.
	bool m_bCurveHandleDragActive = false;
	Zenith_AnimTrackId m_xCurveDragTrack;
	u_int m_uCurveDragKeyId = uINVALID_ANIM_KEY_ID;
	u_int m_uCurveDragComponent = 0u;
	bool m_bCurveDragIn = false;
	bool m_bCurveDragMoved = false;
	float m_fCurveDragPixelX = 0.0f;
	float m_fCurveDragPixelY = 0.0f;

	// Live curve rects, cleared with every other rect map at the top of Render.
	Zenith_HashMap<u_int64, Zenith_AnimPanelRect> m_xCurveKeyRects;
	Zenith_HashMap<u_int64, Zenith_AnimPanelRect> m_xCurveHandleRects;
	Zenith_AnimPanelRect m_xCurveViewRect;
	bool m_bCurveViewRectValid = false;
	u_int m_uCurveTracksDrawn = 0;
	// B3's per-end mode combos: cleared at the top of RenderCurveToolbarItems and
	// raised by RenderCurveTangentModeCombo, one place each — see
	// WasCurveModeControlDrawnLastFrame for why it is not in ClearFrameRects.
	bool m_bCurveModeControlDrawn = false;
	// The row index each drawn curve track occupies, so a curve rect key and a
	// dope-sheet key rect key cannot collide and GetCurveKeyRect can resolve a
	// track the same way GetKeyRect does.
	Zenith_Vector<Zenith_AnimTrackId> m_axCurveTracksDrawn;
};

#endif // ZENITH_TOOLS
