#pragma once

#ifdef ZENITH_TOOLS

//=============================================================================
// Zenith_AnimTimelineMath (WU-3.1) — the PURE seconds <-> pixels mapping the
// dope sheet, the ruler and the events row all share.
//
// ★ NOT ONE LINE OF UI. No ImGui include, no panel state, no statics: every
// function here takes a Zenith_AnimTimelineView by value-semantics and returns
// a number. That is deliberate rather than tidy — this is where nearly every
// timeline defect lives (a key drawn one zoom step out of date, a drag that
// creeps because the pixel->time inverse is not the inverse, a snap that lands
// a hair off the frame grid so the mutator refuses it as an occupied slot), and
// a defect in a pure function is catchable by a headless unit at zero cost.
// Anything that needs ImGui belongs in WU-3.2's panel, calling these.
//
// ★ THE VIEW IS A POD AND MAY ARRIVE DEGENERATE. A panel's view state is
// serialized into prefs, mutated by the wheel, and read on the very first frame
// when the track rect is still zero-sized. So every function SANITISES what it
// reads (see Zenith_AnimTimelineEffectivePixelsPerSecond) instead of asserting:
// a collapsed panel must draw nothing, not divide by zero and poison a key time
// with a NaN that then reaches a .zanim.
//
// TIME IS IN SECONDS EVERYWHERE, on the same clock as Flux_AnimationClip's key
// times and duration, and compared with that header's fANIM_TIME_EPSILON
// (1e-5 s). Nothing here converts to or from channel ticks — that conversion is
// the clip's business, and having two owners of it is how a snapped time stops
// matching the slot it was snapped for.
//=============================================================================

//-----------------------------------------------------------------------------
// Zoom limits, in pixels per second.
//
// The floor is not cosmetic: at 8 px/s a 2000 px track spans 250 s, and one
// pixel is already 0.125 s — far coarser than any frame grid, so zooming out
// past it can only produce a row of keys stacked on one column. The ceiling is
// the other end of the same argument: at 4000 px/s a single 60 fps frame is
// 66 px wide, which is as far as sub-frame work can usefully go.
//
// They are named constants because BOTH ends are read by three places (the
// wheel handler, Clamp, and the fit-to-window verb) and a second opinion about
// the floor would let a view exist that the mapping refuses to invert.
//-----------------------------------------------------------------------------
constexpr float fANIM_TIMELINE_MIN_PPS = 8.0f;
constexpr float fANIM_TIMELINE_MAX_PPS = 4000.0f;

// What a freshly opened document (or a view whose zoom did not survive its
// prefs) starts at: 120 px/s puts a 4 s sway clip in ~480 px.
constexpr float fANIM_TIMELINE_DEFAULT_PPS = 120.0f;

// Roughly how far apart the ruler wants its LABELLED ticks. Approximate by
// construction — the ladder below only offers times a human reads at a glance,
// so the real spacing lands anywhere from this to ~2.5x it.
constexpr float fANIM_TIMELINE_TARGET_MAJOR_PIXELS = 110.0f;

//-----------------------------------------------------------------------------
// Everything the mapping needs, and nothing else. Four floats, copyable,
// comparable field by field — a panel owns one of these and the tests build
// them by hand.
//-----------------------------------------------------------------------------
struct Zenith_AnimTimelineView
{
	// Zoom. Read through Zenith_AnimTimelineEffectivePixelsPerSecond, which
	// clamps it into [fANIM_TIMELINE_MIN_PPS, fANIM_TIMELINE_MAX_PPS] and
	// substitutes the floor for a non-finite or non-positive value.
	float m_fPixelsPerSecond = fANIM_TIMELINE_DEFAULT_PPS;

	// The time at the LEFT EDGE of the track area. Zenith_AnimTimelineClamp
	// forbids a negative one (a clip starts at 0); the raw mapping does not,
	// so a caller mid-drag may pass one and still get a consistent inverse.
	float m_fScrollSeconds = 0.0f;

	// Screen x of the track area's left edge, and its width. The width MAY BE
	// ZERO — a collapsed or freshly docked panel — and that is a legal state
	// meaning "nothing is visible", not an error.
	float m_fTrackLeftPixel = 0.0f;
	float m_fTrackWidthPixels = 0.0f;
};

//-----------------------------------------------------------------------------
// One ruler subdivision, in seconds. Major ticks carry a label; minor ticks are
// the unlabelled subdivision between two of them. Both are strictly positive
// and finite for EVERY input, including a degenerate view (that is a tested
// property — a zero spacing is an infinite loop in the ruler's draw code).
//-----------------------------------------------------------------------------
struct Zenith_AnimTimelineTicks
{
	float m_fMajorSeconds = 1.0f;
	float m_fMinorSeconds = 1.0f;
};

//-----------------------------------------------------------------------------
// The zoom the mapping ACTUALLY uses: m_fPixelsPerSecond clamped into the legal
// range, with a non-finite or non-positive value replaced by the floor. Public
// because a panel drawing its own zoom readout must show the same number the
// mapping used, and because it is the one place the sanitisation lives.
//-----------------------------------------------------------------------------
float Zenith_AnimTimelineEffectivePixelsPerSecond(const Zenith_AnimTimelineView& xView);

//-----------------------------------------------------------------------------
// The mapping and its inverse.
//
//   pixel = left + (t - scroll) * pps
//   t     = scroll + (pixel - left) / pps
//
// ★ HOW EXACT THE ROUND TRIP IS, precisely. The intermediates are computed in
// double, so the only loss is rounding the returned float. That makes the
// round-trip error about |pixel| * 2^-25 / pps seconds — under
// fANIM_TIME_EPSILON for every view a panel can produce (a screen-sized
// |pixel| and |t - scroll| up to a few hundred seconds), and NOT under it for
// deliberately absurd inputs such as a 600 s offset at maximum zoom, which is
// 2.4 million pixels away from the track. That is a property of float32, not of
// this code: 24 bits of mantissa cannot hold a 1e-5 s resolution over a 1e6
// range. If a clip ever needs to be minutes long AND frame-accurate at maximum
// zoom, the view state moves to double — this comment is the place to start.
//
// ★ NO FUNCTION HERE RETURNS A NaN, whatever it is handed. A non-finite time
// maps to a pixel a million units OUTSIDE the track (culled by every
// visibility test, clipped by every draw) rather than to a NaN, because these
// results go straight into ImGui draw-list coordinates and a NaN rect there is
// an assertion in a windowed build and a corrupt vertex buffer in one without.
// A non-finite pixel maps back to the scroll — the left edge — for the same
// reason.
//-----------------------------------------------------------------------------
float Zenith_AnimTimelineTimeToPixel(const Zenith_AnimTimelineView& xView, float fTimeSeconds);
float Zenith_AnimTimelinePixelToTime(const Zenith_AnimTimelineView& xView, float fPixelX);

// DELTA conversions — the same scale factor with no origin. What a drag uses:
// a mouse delta in pixels is a time delta, and running it through the absolute
// mapping instead (subtracting two TimeToPixel results) is how a drag picks up
// the scroll's rounding twice.
float Zenith_AnimTimelineSecondsToPixels(const Zenith_AnimTimelineView& xView, float fDeltaSeconds);
float Zenith_AnimTimelinePixelsToSeconds(const Zenith_AnimTimelineView& xView, float fDeltaPixels);

// Is fTimeSeconds inside the track rect (edges INCLUSIVE)? Always false for a
// zero-width track: nothing is visible in a panel with no room, and a caller
// that culled on a bare pixel compare would draw a key on a 0 px strip.
bool Zenith_AnimTimelineIsVisible(const Zenith_AnimTimelineView& xView, float fTimeSeconds);

// The times at the two edges of the track, i.e. exactly
// PixelToTime(left) and PixelToTime(left + width). Equal when the width is 0.
void Zenith_AnimTimelineVisibleRange(const Zenith_AnimTimelineView& xView, float& fOutStartSeconds, float& fOutEndSeconds);

//-----------------------------------------------------------------------------
// The frame grid — the clip's AUTHORED frame rate
// (Flux_AnimationClipMetadata::m_uAuthoredFrameRate: 30 by default, 4 on the
// tree/bush sway clips, 24 on StickFigure), NOT its tick rate.
//
// ★ A SNAPPED TIME MUST BE BIT-IDENTICAL TO THE FRAME TIME IT NAMES. Both
// functions below route through the same one division, so
// SnapToFrame(FrameToTime(f)) == FrameToTime(f) on the nose rather than within
// a tolerance. That matters because the document's occupancy test is
// |a - b| <= fANIM_TIME_EPSILON: two expressions of "frame 7" that differ by a
// couple of ULPs would still compare equal today, but the guarantee is cheap
// and the failure it prevents (an insert refused as a duplicate, or a drag that
// walks a key one ULP per frame) is not.
//
// uFrameRate == 0 means UNSNAPPED: SnapToFrame returns its input untouched.
//-----------------------------------------------------------------------------
float Zenith_AnimTimelineSnapToFrame(float fTimeSeconds, u_int uFrameRate);

// Frame index -> seconds. Returns 0 for uFrameRate == 0 (there is no grid, so
// there is no frame 3 to place).
float Zenith_AnimTimelineFrameToTime(u_int uFrame, u_int uFrameRate);

// Seconds -> NEAREST frame index; an exact midpoint rounds UP. Times at or
// before 0 give frame 0, as does a non-finite time and uFrameRate == 0: the
// grid has no negative frames, and every consumer clamps to [0, duration]
// anyway.
u_int Zenith_AnimTimelineTimeToFrame(float fTimeSeconds, u_int uFrameRate);

//-----------------------------------------------------------------------------
// Zoom by fZoomFactor about a screen x, keeping the time under that x FIXED —
// the behaviour a wheel over a canvas has to have or the thing being inspected
// slides out from under the cursor. Refuses (leaves the view untouched) a
// non-finite or non-positive factor, and a non-finite pixel.
//
// The zoom saturates at the clamps; at either clamp the scroll is recomputed
// against the SAME zoom, so a wheel held down at maximum zoom holds still
// (to within one ULP of the scroll) instead of drifting sideways.
//-----------------------------------------------------------------------------
void Zenith_AnimTimelineZoomAroundPixel(Zenith_AnimTimelineView& xView, float fPixelX, float fZoomFactor);

//-----------------------------------------------------------------------------
// Force the whole view into a legal state for a clip of fDurationSeconds:
// finite track rect (a negative or non-finite width becomes 0), zoom inside the
// clamps, and scroll inside [0, max(0, duration - visible span)] so the clip
// can never be scrolled off the left edge or past its own end. A clip that
// fits entirely in the track is pinned at scroll 0.
//
// NEVER produces a NaN, for any input, including duration 0 and a view whose
// every field is NaN. That is the whole point of it existing: it is what a
// panel calls before it trusts its own state.
//-----------------------------------------------------------------------------
void Zenith_AnimTimelineClamp(Zenith_AnimTimelineView& xView, float fDurationSeconds);

// "Zoom to fit": the clip exactly spans the track, scrolled to its start.
// Falls back to fANIM_TIMELINE_DEFAULT_PPS when there is no duration or no
// track to fit it into, and obeys the zoom clamps (so a very long clip may
// still not fit — the caller has a scrollbar for that).
void Zenith_AnimTimelineFrameAll(Zenith_AnimTimelineView& xView, float fDurationSeconds);

//-----------------------------------------------------------------------------
// Ruler subdivisions for this view, from a nice-number ladder: 1 / 2 / 5 / 10
// FRAMES first (so a zoomed-in ruler is labelled in times keys can actually sit
// on), then 0.1 / 0.25 / 0.5 / 1 / 2 / 5 / 10 / 15 / 30 / 60 / 120 / 300 / 600
// seconds, with any absolute step that would not be an increase on the last
// frame step dropped so the ladder is strictly ascending.
//
// The major step is the SMALLEST rung at least fTargetMajorPixels wide; the
// minor step is the rung below it (the same rung, when the major is already the
// finest one on offer). Consequences worth relying on: the result is monotonic
// in zoom (zooming in never widens the step, in seconds), minor <= major, and
// both are strictly positive and finite always.
//-----------------------------------------------------------------------------
Zenith_AnimTimelineTicks Zenith_AnimTimelineChooseTicks(const Zenith_AnimTimelineView& xView, u_int uFrameRate,
                                                        float fTargetMajorPixels = fANIM_TIMELINE_TARGET_MAJOR_PIXELS);

// Force-link anchor: Zenith_Editor::Initialise calls this so this TU survives
// /OPT:REF. Until WU-3.2's dope sheet calls the mapping for real, nothing names
// this .obj — and an .obj the linker never pulls in takes the ZENITH_TEST
// registrars at the bottom of the .cpp with it, moving the unit count by zero
// and reddening nothing. Same idiom, same reason, as
// Zenith_AnimationDocument_ForceLink.
bool Zenith_AnimTimelineMath_ForceLink();

#endif // ZENITH_TOOLS
