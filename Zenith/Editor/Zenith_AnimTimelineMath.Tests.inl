//------------------------------------------------------------------------------
// Zenith_AnimTimelineMath unit tests (WU-3.1).
// Included at the bottom of Zenith_AnimTimelineMath.cpp.
//
// ★ WHY THIS FILE IS WORTH MORE THAN THE PANEL TESTS ABOVE IT. Every timeline
// defect that has ever been hard to see is a coordinate defect: a key drawn one
// zoom step stale, a drag that creeps because pixel->time is not the inverse of
// time->pixel, a snapped time a couple of ULPs off the frame it names so the
// document refuses the insert as an occupied slot. None of those look wrong in a
// screenshot and all of them are one float comparison away from being caught
// here.
//
// All of it is pure arithmetic: no ImGui, no device, no scene, no files. Every
// case runs headless under the Null backend and NONE is requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"   // fANIM_TIME_EPSILON

#include <limits>

namespace
{
	Zenith_AnimTimelineView AnimTimelineMakeView(float fPixelsPerSecond, float fScrollSeconds,
	                                             float fTrackLeftPixel, float fTrackWidthPixels)
	{
		Zenith_AnimTimelineView xView;
		xView.m_fPixelsPerSecond = fPixelsPerSecond;
		xView.m_fScrollSeconds = fScrollSeconds;
		xView.m_fTrackLeftPixel = fTrackLeftPixel;
		xView.m_fTrackWidthPixels = fTrackWidthPixels;
		return xView;
	}

	// ★ Read through a volatile so the NaN is a RUNTIME value. A quiet_NaN() folded
	// at compile time under /fp:fast can be reasoned about by the optimiser; a
	// value that came out of memory cannot, so what the test exercises is the same
	// arithmetic a panel would hand the mapping.
	float AnimTimelineNaN()
	{
		volatile float fSource = std::numeric_limits<float>::quiet_NaN();
		return fSource;
	}

	float AnimTimelineInfinity()
	{
		volatile float fSource = std::numeric_limits<float>::infinity();
		return fSource;
	}

	bool AnimTimelineViewsIdentical(const Zenith_AnimTimelineView& xA, const Zenith_AnimTimelineView& xB)
	{
		return xA.m_fPixelsPerSecond == xB.m_fPixelsPerSecond
			&& xA.m_fScrollSeconds == xB.m_fScrollSeconds
			&& xA.m_fTrackLeftPixel == xB.m_fTrackLeftPixel
			&& xA.m_fTrackWidthPixels == xB.m_fTrackWidthPixels;
	}

	bool AnimTimelineViewIsFinite(const Zenith_AnimTimelineView& xView)
	{
		return std::isfinite(xView.m_fPixelsPerSecond)
			&& std::isfinite(xView.m_fScrollSeconds)
			&& std::isfinite(xView.m_fTrackLeftPixel)
			&& std::isfinite(xView.m_fTrackWidthPixels);
	}
}

//==============================================================================
// (1) The round trip. PixelToTime(TimeToPixel(t)) == t across a sweep of zoom,
// scroll, track origin and time — INCLUDING the degenerate views a panel really
// produces: a collapsed (zero-width) track, both zoom clamps, a zoom of zero or
// NaN left in a prefs file, and a negative scroll mid-drag.
//==============================================================================
ZENITH_TEST(AnimTimeline, RoundTripSurvivesZoomScrollAndDegenerates)
{
	const float afPixelsPerSecond[] =
	{
		fANIM_TIMELINE_MIN_PPS,
		30.0f,
		137.5f,
		1000.0f,
		fANIM_TIMELINE_MAX_PPS,
		0.0f,                     // degenerate: sanitised up to the floor
		-5.0f,                    // degenerate: ditto
		AnimTimelineNaN(),        // degenerate: ditto
	};
	const float afScroll[] = { -3.25f, 0.0f, 0.5f, 41.0f };
	// ★ A track origin of a few hundred pixels is what a docked panel really has,
	// and it is also where the round trip is TIGHTEST: the offset is added before
	// the result is rounded to a float, so a large origin at the zoom FLOOR spends
	// the mantissa on the offset rather than on the time. See the header's
	// accuracy note; a genuinely absurd origin is the far case pinned below.
	const float afLeft[] = { 0.0f, 320.5f, 512.0f };
	const float afWidth[] = { 0.0f, 900.0f };
	const float afOffsets[] = { -1.5f, 0.0f, 0.001f, 1.0f, 37.5f, 60.0f };

	u_int uCases = 0;
	for (u_int uPps = 0; uPps < 8u; ++uPps)
	{
		for (u_int uScroll = 0; uScroll < 4u; ++uScroll)
		{
			for (u_int uLeft = 0; uLeft < 3u; ++uLeft)
			{
				for (u_int uWidth = 0; uWidth < 2u; ++uWidth)
				{
					const Zenith_AnimTimelineView xView = AnimTimelineMakeView(
						afPixelsPerSecond[uPps], afScroll[uScroll], afLeft[uLeft], afWidth[uWidth]);

					// The zoom the mapping actually used is always legal, whatever
					// the field said.
					const float fEffective = Zenith_AnimTimelineEffectivePixelsPerSecond(xView);
					ZENITH_ASSERT_TRUE(fEffective >= fANIM_TIMELINE_MIN_PPS && fEffective <= fANIM_TIMELINE_MAX_PPS,
						"a degenerate zoom (%f) is sanitised into the clamps, not passed through", afPixelsPerSecond[uPps]);

					for (u_int uOffset = 0; uOffset < 6u; ++uOffset)
					{
						const float fTime = afScroll[uScroll] + afOffsets[uOffset];
						const float fPixel = Zenith_AnimTimelineTimeToPixel(xView, fTime);
						ZENITH_ASSERT_TRUE(std::isfinite(fPixel), "a finite time never maps to a non-finite pixel");

						const float fBack = Zenith_AnimTimelinePixelToTime(xView, fPixel);
						ZENITH_ASSERT_EQ_FLOAT(fBack, fTime, fANIM_TIME_EPSILON,
							"pixel->time inverts time->pixel (pps %f, scroll %f, left %f)",
							afPixelsPerSecond[uPps], afScroll[uScroll], afLeft[uLeft]);
						++uCases;
					}
				}
			}
		}
	}
	ZENITH_ASSERT_EQ(uCases, 1152u, "the sweep really ran every combination");

	// ★ The DOCUMENTED limit, pinned so it degrades gracefully rather than
	// silently. 600 s at maximum zoom is 2.4 million pixels from the track: float32
	// has 24 bits of mantissa, so the round trip there is good to ~1e-4 s, not to
	// fANIM_TIME_EPSILON. See the header note — this is where to start if a clip
	// ever needs to be minutes long AND frame-accurate at full zoom.
	const Zenith_AnimTimelineView xExtreme = AnimTimelineMakeView(fANIM_TIMELINE_MAX_PPS, 0.0f, 1024.0f, 900.0f);
	const float fFarBack = Zenith_AnimTimelinePixelToTime(xExtreme, Zenith_AnimTimelineTimeToPixel(xExtreme, 600.0f));
	ZENITH_ASSERT_EQ_FLOAT(fFarBack, 600.0f, 1.0e-3f, "the round trip degrades gracefully far outside the track");

	// Non-finite time -> an off-screen pixel, never a NaN reaching a draw list.
	const float fNaNPixel = Zenith_AnimTimelineTimeToPixel(xExtreme, AnimTimelineNaN());
	ZENITH_ASSERT_TRUE(std::isfinite(fNaNPixel), "a NaN time still produces a finite pixel");
	ZENITH_ASSERT_TRUE(fNaNPixel < xExtreme.m_fTrackLeftPixel - 1000.0f, "and that pixel is far outside the track");
	const float fInfPixel = Zenith_AnimTimelineTimeToPixel(xExtreme, AnimTimelineInfinity());
	ZENITH_ASSERT_TRUE(std::isfinite(fInfPixel) && fInfPixel > xExtreme.m_fTrackLeftPixel + 1000.0f,
		"+inf lands off the RIGHT, so the two do not fold onto each other");

	// Non-finite pixel -> the left edge's time.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelinePixelToTime(xExtreme, AnimTimelineNaN()), 0.0f, 1.0e-6f,
		"a NaN pixel reads as the time at the left edge");
}

//==============================================================================
// (2) The DELTA conversions are the same scale with no origin — what a drag
// uses. Subtracting two absolute mappings instead picks up the scroll's rounding
// twice, which is how a dragged key creeps.
//==============================================================================
ZENITH_TEST(AnimTimeline, DeltaConversionsIgnoreTheOriginAndInvert)
{
	const Zenith_AnimTimelineView xA = AnimTimelineMakeView(250.0f, 7.5f, 300.0f, 600.0f);
	const Zenith_AnimTimelineView xB = AnimTimelineMakeView(250.0f, -91.0f, 0.0f, 20.0f);

	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineSecondsToPixels(xA, 2.0f), 500.0f, 1.0e-4f, "2 s at 250 px/s is 500 px");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelinePixelsToSeconds(xA, 500.0f), 2.0f, 1.0e-6f, "and back again");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineSecondsToPixels(xB, 2.0f),
		Zenith_AnimTimelineSecondsToPixels(xA, 2.0f), 1.0e-6f, "a delta ignores scroll and track origin entirely");

	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineSecondsToPixels(xA, -0.4f), -100.0f, 1.0e-4f, "deltas are signed");

	// Consistent with the absolute mapping, which is the property the drag code
	// leans on when it converts a delta and then re-draws from absolute times.
	const float fSpan = Zenith_AnimTimelineTimeToPixel(xA, 9.5f) - Zenith_AnimTimelineTimeToPixel(xA, 7.5f);
	ZENITH_ASSERT_EQ_FLOAT(fSpan, Zenith_AnimTimelineSecondsToPixels(xA, 2.0f), 1.0e-3f,
		"the delta conversion agrees with the difference of two absolute mappings");

	// Degenerate zoom takes the floor here too.
	const Zenith_AnimTimelineView xDead = AnimTimelineMakeView(0.0f, 0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineSecondsToPixels(xDead, 1.0f), fANIM_TIMELINE_MIN_PPS, 1.0e-4f,
		"a zero zoom converts at the floor, not at zero");

	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineSecondsToPixels(xA, AnimTimelineNaN()), 0.0f, 1.0e-6f,
		"a NaN delta is zero pixels, never a NaN");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelinePixelsToSeconds(xA, AnimTimelineNaN()), 0.0f, 1.0e-6f,
		"and the same the other way");
}

//==============================================================================
// (3) ★ THE BIT-EXACTNESS PROPERTY. A snapped time must be the SAME FLOAT as the
// frame time it names, at every authored frame rate in the tree (4 on the sway
// clips, 24 on StickFigure, 30 the default, 60 for good measure) — not merely
// within a tolerance. Two expressions of "frame 7" that differ by an ULP would
// pass a tolerance test and still make a drag walk a key one ULP per frame.
//==============================================================================
ZENITH_TEST(AnimTimeline, SnapLandsExactlyOnFrameBoundaries)
{
	const u_int auRates[] = { 4u, 24u, 30u, 60u };
	const u_int auFrames[] = { 0u, 1u, 2u, 3u, 7u, 29u, 101u };

	for (u_int uRate = 0; uRate < 4u; ++uRate)
	{
		const u_int uFrameRate = auRates[uRate];
		float fPrevious = -1.0f;
		for (u_int uIndex = 0; uIndex < 7u; ++uIndex)
		{
			const u_int uFrame = auFrames[uIndex];
			const float fFrameTime = Zenith_AnimTimelineFrameToTime(uFrame, uFrameRate);

			ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(fFrameTime, uFrameRate) == fFrameTime,
				"snapping a frame time is BIT-identical to that frame time (rate %u, frame %u)", uFrameRate, uFrame);
			ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(fFrameTime, uFrameRate), uFrame,
				"and the time resolves back to the frame that made it (rate %u)", uFrameRate);

			ZENITH_ASSERT_TRUE(fFrameTime > fPrevious, "frame times ascend (rate %u, frame %u)", uFrameRate, uFrame);
			fPrevious = fFrameTime;
		}

		ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineFrameToTime(uFrameRate, uFrameRate), 1.0f, 1.0e-6f,
			"frame <rate> is one second in (rate %u)", uFrameRate);
	}

	// A time already ON the grid is idempotent under repeated snapping — the
	// property a drag relies on when it snaps every frame of the gesture.
	const float fOnGrid = Zenith_AnimTimelineSnapToFrame(0.7123f, 24u);
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(fOnGrid, 24u) == fOnGrid, "snapping is idempotent");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(fOnGrid, 24u) == Zenith_AnimTimelineFrameToTime(17u, 24u),
		"0.7123 s at 24 fps is frame 17 (17/24 = 0.70833)");
}

//==============================================================================
// (4) Which side of the midpoint a time falls on decides which frame it lands
// on, and an exact midpoint rounds UP. Checked at 4 fps, where the midpoint
// (0.125 s) is exactly representable, so the tie is a real tie.
//==============================================================================
ZENITH_TEST(AnimTimeline, SnapRoundsAtTheFrameMidpoint)
{
	const float fFrame0 = Zenith_AnimTimelineFrameToTime(0u, 4u);   // 0.0
	const float fFrame1 = Zenith_AnimTimelineFrameToTime(1u, 4u);   // 0.25
	const float fFrame2 = Zenith_AnimTimelineFrameToTime(2u, 4u);   // 0.5

	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.125f - 1.0e-4f, 4u) == fFrame0, "just below the midpoint snaps DOWN");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.125f + 1.0e-4f, 4u) == fFrame1, "just above the midpoint snaps UP");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.125f, 4u) == fFrame1, "an exact tie rounds UP, deterministically");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.375f, 4u) == fFrame2, "and the next tie likewise");

	// 30 fps: 1.5 frames is 0.05 s.
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.05f - 1.0e-4f, 30u) == Zenith_AnimTimelineFrameToTime(1u, 30u),
		"just below the 30 fps midpoint snaps down");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(0.05f + 1.0e-4f, 30u) == Zenith_AnimTimelineFrameToTime(2u, 30u),
		"just above it snaps up");

	// A rate of 0 means UNSNAPPED, and unsnapped means untouched — bit for bit,
	// so a panel with snapping off cannot move a key by handing it through here.
	const float fArbitrary = 1.2345678f;
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(fArbitrary, 0u) == fArbitrary, "rate 0 passes the time through");

	// Before the start of the clip there is no frame to land on.
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(-0.4f, 30u) == 0.0f, "a time before 0 snaps to frame 0");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineSnapToFrame(AnimTimelineNaN(), 30u) == 0.0f, "and so does a NaN");
}

//==============================================================================
// (5) The frame conversions at their edges — the cases a panel hits on its first
// frame (no clip, no rate) and mid-drag (a time dragged left of zero).
//==============================================================================
ZENITH_TEST(AnimTimeline, FrameConversionsHandleTheGridEdges)
{
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(1.0f, 0u), 0u, "with no grid there is no frame index");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineFrameToTime(5u, 0u) == 0.0f, "and no time for frame 5 either");

	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(0.0f, 30u), 0u, "t = 0 is frame 0");
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(-1.0f, 30u), 0u, "a negative time clamps to frame 0");
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(AnimTimelineNaN(), 30u), 0u, "a NaN time clamps to frame 0");
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(AnimTimelineInfinity(), 30u), 0u, "an infinite time clamps to frame 0");

	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(0.0166f, 30u), 0u, "under half a frame is still frame 0");
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(0.0167f, 30u), 1u, "over half a frame is frame 1");
	ZENITH_ASSERT_EQ(Zenith_AnimTimelineTimeToFrame(4.0f, 4u), 16u, "4 s at 4 fps is frame 16");

	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineFrameToTime(0u, 30u) == 0.0f, "frame 0 is exactly t = 0");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineFrameToTime(96u, 24u), 4.0f, 1.0e-6f, "frame 96 at 24 fps is 4 s");
}

//==============================================================================
// (6) Visibility follows the track RECT, and a collapsed panel shows nothing.
//==============================================================================
ZENITH_TEST(AnimTimeline, VisibilityFollowsTheTrackRect)
{
	// 100 px/s over 400 px from t = 2 -> the window is [2, 6].
	const Zenith_AnimTimelineView xView = AnimTimelineMakeView(100.0f, 2.0f, 50.0f, 400.0f);

	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineIsVisible(xView, 2.0f), "the left edge is inside");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineIsVisible(xView, 6.0f), "the right edge is inside");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineIsVisible(xView, 4.0f), "and so is the middle");
	ZENITH_ASSERT_FALSE(Zenith_AnimTimelineIsVisible(xView, 1.99f), "a time before the window is out");
	ZENITH_ASSERT_FALSE(Zenith_AnimTimelineIsVisible(xView, 6.01f), "a time after it is out");
	ZENITH_ASSERT_FALSE(Zenith_AnimTimelineIsVisible(xView, AnimTimelineNaN()), "a NaN time is never visible");

	// ★ A collapsed track shows NOTHING — not the single column at its left edge,
	// which is what an inclusive compare against a zero-width rect would answer.
	const Zenith_AnimTimelineView xCollapsed = AnimTimelineMakeView(100.0f, 2.0f, 50.0f, 0.0f);
	ZENITH_ASSERT_FALSE(Zenith_AnimTimelineIsVisible(xCollapsed, 2.0f), "a zero-width track shows nothing");

	const Zenith_AnimTimelineView xNegative = AnimTimelineMakeView(100.0f, 2.0f, 50.0f, -30.0f);
	ZENITH_ASSERT_FALSE(Zenith_AnimTimelineIsVisible(xNegative, 2.0f), "nor does a negative-width one");
}

//==============================================================================
// (7) ★ Zoom keeps the time under the cursor FIXED. A wheel that fails this
// slides the thing being inspected out from under the pointer, which is the one
// canvas behaviour everybody notices and nobody reports precisely.
//==============================================================================
ZENITH_TEST(AnimTimeline, ZoomAroundPixelKeepsTheTimeUnderTheCursor)
{
	const Zenith_AnimTimelineView xBase = AnimTimelineMakeView(100.0f, 1.0f, 200.0f, 800.0f);
	const float fCursorPixel = 500.0f;
	const float fTimeUnderCursor = Zenith_AnimTimelinePixelToTime(xBase, fCursorPixel);
	ZENITH_ASSERT_EQ_FLOAT(fTimeUnderCursor, 4.0f, 1.0e-6f, "the cursor starts over t = 4 s");

	const float afFactors[] = { 1.1f, 1.5f, 2.0f, 0.5f, 0.25f, 8.0f, 0.01f, 500.0f };
	for (u_int u = 0; u < 8u; ++u)
	{
		Zenith_AnimTimelineView xView = xBase;
		Zenith_AnimTimelineZoomAroundPixel(xView, fCursorPixel, afFactors[u]);

		ZENITH_ASSERT_TRUE(xView.m_fPixelsPerSecond >= fANIM_TIMELINE_MIN_PPS
			&& xView.m_fPixelsPerSecond <= fANIM_TIMELINE_MAX_PPS,
			"the zoom stays inside the clamps (factor %f)", afFactors[u]);
		ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelinePixelToTime(xView, fCursorPixel), fTimeUnderCursor,
			fANIM_TIME_EPSILON, "the time under the cursor did not move (factor %f)", afFactors[u]);
	}

	// Saturated at a clamp, the gesture holds still instead of scrolling.
	Zenith_AnimTimelineView xAtMax = AnimTimelineMakeView(fANIM_TIMELINE_MAX_PPS, 1.0f, 200.0f, 800.0f);
	const float fTimeAtMax = Zenith_AnimTimelinePixelToTime(xAtMax, fCursorPixel);
	Zenith_AnimTimelineZoomAroundPixel(xAtMax, fCursorPixel, 4.0f);
	ZENITH_ASSERT_TRUE(xAtMax.m_fPixelsPerSecond == fANIM_TIMELINE_MAX_PPS, "zooming in at the ceiling stays at the ceiling");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelinePixelToTime(xAtMax, fCursorPixel), fTimeAtMax, fANIM_TIME_EPSILON,
		"and does not drift sideways while it is held");

	Zenith_AnimTimelineView xAtMin = AnimTimelineMakeView(fANIM_TIMELINE_MIN_PPS, 1.0f, 200.0f, 800.0f);
	Zenith_AnimTimelineZoomAroundPixel(xAtMin, fCursorPixel, 0.1f);
	ZENITH_ASSERT_TRUE(xAtMin.m_fPixelsPerSecond == fANIM_TIMELINE_MIN_PPS, "and the floor behaves the same way");

	// Refusals leave the view byte-for-byte alone rather than half-applying.
	const float afRefused[] = { 0.0f, -2.0f, AnimTimelineNaN(), AnimTimelineInfinity() };
	for (u_int u = 0; u < 4u; ++u)
	{
		Zenith_AnimTimelineView xView = xBase;
		Zenith_AnimTimelineZoomAroundPixel(xView, fCursorPixel, afRefused[u]);
		ZENITH_ASSERT_TRUE(AnimTimelineViewsIdentical(xView, xBase), "a non-positive or non-finite factor changes nothing");
	}

	Zenith_AnimTimelineView xBadPixel = xBase;
	Zenith_AnimTimelineZoomAroundPixel(xBadPixel, AnimTimelineNaN(), 2.0f);
	ZENITH_ASSERT_TRUE(AnimTimelineViewsIdentical(xBadPixel, xBase), "a non-finite cursor position changes nothing");
}

//==============================================================================
// (8) Clamp is what a panel calls before it trusts its own state, so its
// contract is total: legal range in, NEVER a NaN out — for any input at all,
// including a duration of 0 and a view whose every field is NaN.
//==============================================================================
ZENITH_TEST(AnimTimeline, ClampNeverProducesNaNAndKeepsTheClipReachable)
{
	// The worst input there is.
	Zenith_AnimTimelineView xPoisoned = AnimTimelineMakeView(AnimTimelineNaN(), AnimTimelineNaN(),
		AnimTimelineNaN(), AnimTimelineNaN());
	Zenith_AnimTimelineClamp(xPoisoned, 0.0f);
	ZENITH_ASSERT_TRUE(AnimTimelineViewIsFinite(xPoisoned), "an all-NaN view clamps to an entirely finite one");
	ZENITH_ASSERT_TRUE(xPoisoned.m_fScrollSeconds == 0.0f, "with the scroll at the start");
	ZENITH_ASSERT_TRUE(xPoisoned.m_fTrackWidthPixels == 0.0f, "and no track width invented");
	ZENITH_ASSERT_TRUE(xPoisoned.m_fPixelsPerSecond >= fANIM_TIMELINE_MIN_PPS
		&& xPoisoned.m_fPixelsPerSecond <= fANIM_TIMELINE_MAX_PPS, "and a legal zoom");

	// A clip of zero duration is a real state (a freshly created .zanim).
	Zenith_AnimTimelineView xEmpty = AnimTimelineMakeView(120.0f, 9.0f, 40.0f, 800.0f);
	Zenith_AnimTimelineClamp(xEmpty, 0.0f);
	ZENITH_ASSERT_TRUE(AnimTimelineViewIsFinite(xEmpty), "duration 0 produces no NaN");
	ZENITH_ASSERT_TRUE(xEmpty.m_fScrollSeconds == 0.0f, "and pins the scroll at 0");

	Zenith_AnimTimelineView xInfinite = AnimTimelineMakeView(120.0f, 9.0f, 40.0f, 800.0f);
	Zenith_AnimTimelineClamp(xInfinite, AnimTimelineInfinity());
	ZENITH_ASSERT_TRUE(AnimTimelineViewIsFinite(xInfinite), "an infinite duration produces no NaN either");

	// The clip fits entirely: pinned at the start rather than free to scroll off.
	Zenith_AnimTimelineView xFits = AnimTimelineMakeView(100.0f, 5.0f, 0.0f, 800.0f);
	Zenith_AnimTimelineClamp(xFits, 2.0f);
	ZENITH_ASSERT_TRUE(xFits.m_fScrollSeconds == 0.0f, "a clip shorter than the window cannot be scrolled");

	// The clip does not fit: the last window's worth stays reachable and no more.
	Zenith_AnimTimelineView xLong = AnimTimelineMakeView(100.0f, 1000.0f, 0.0f, 800.0f);
	Zenith_AnimTimelineClamp(xLong, 100.0f);
	ZENITH_ASSERT_EQ_FLOAT(xLong.m_fScrollSeconds, 92.0f, 1.0e-4f, "the end of the clip sits at the right edge, not beyond it");

	Zenith_AnimTimelineView xBefore = AnimTimelineMakeView(100.0f, -5.0f, 0.0f, 800.0f);
	Zenith_AnimTimelineClamp(xBefore, 100.0f);
	ZENITH_ASSERT_TRUE(xBefore.m_fScrollSeconds == 0.0f, "and the clip cannot be scrolled off the left edge");

	// Both zoom clamps, and a negative track width.
	Zenith_AnimTimelineView xWild = AnimTimelineMakeView(1.0e9f, 3.0f, 10.0f, -50.0f);
	Zenith_AnimTimelineClamp(xWild, 10.0f);
	ZENITH_ASSERT_TRUE(xWild.m_fPixelsPerSecond == fANIM_TIMELINE_MAX_PPS, "an absurd zoom saturates at the ceiling");
	ZENITH_ASSERT_TRUE(xWild.m_fTrackWidthPixels == 0.0f, "a negative width becomes zero");

	Zenith_AnimTimelineView xTiny = AnimTimelineMakeView(-3.0f, 3.0f, 10.0f, 400.0f);
	Zenith_AnimTimelineClamp(xTiny, 10.0f);
	ZENITH_ASSERT_TRUE(xTiny.m_fPixelsPerSecond == fANIM_TIMELINE_MIN_PPS, "a negative zoom saturates at the floor");

	// Clamp is idempotent — a panel calls it every frame.
	Zenith_AnimTimelineView xOnce = AnimTimelineMakeView(37.0f, 12.0f, 64.0f, 700.0f);
	Zenith_AnimTimelineClamp(xOnce, 30.0f);
	Zenith_AnimTimelineView xTwice = xOnce;
	Zenith_AnimTimelineClamp(xTwice, 30.0f);
	ZENITH_ASSERT_TRUE(AnimTimelineViewsIdentical(xOnce, xTwice), "clamping an already-clamped view changes nothing");
}

//==============================================================================
// (9) Zoom-to-fit puts the whole clip in the track, and falls back sanely when
// there is no clip or no track to fit it into.
//==============================================================================
ZENITH_TEST(AnimTimeline, FrameAllFitsTheClipIntoTheTrack)
{
	Zenith_AnimTimelineView xView = AnimTimelineMakeView(37.0f, 12.0f, 100.0f, 800.0f);
	Zenith_AnimTimelineFrameAll(xView, 4.0f);
	ZENITH_ASSERT_EQ_FLOAT(xView.m_fPixelsPerSecond, 200.0f, 1.0e-4f, "800 px over 4 s is 200 px/s");
	ZENITH_ASSERT_TRUE(xView.m_fScrollSeconds == 0.0f, "and the view is at the start of the clip");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineTimeToPixel(xView, 0.0f), 100.0f, 1.0e-3f, "t = 0 is the left edge");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimTimelineTimeToPixel(xView, 4.0f), 900.0f, 1.0e-3f, "and the end is the right edge");

	Zenith_AnimTimelineView xNoClip = AnimTimelineMakeView(37.0f, 12.0f, 100.0f, 800.0f);
	Zenith_AnimTimelineFrameAll(xNoClip, 0.0f);
	ZENITH_ASSERT_TRUE(xNoClip.m_fPixelsPerSecond == fANIM_TIMELINE_DEFAULT_PPS, "no duration falls back to the default zoom");
	ZENITH_ASSERT_TRUE(xNoClip.m_fScrollSeconds == 0.0f, "still at the start");

	Zenith_AnimTimelineView xNoTrack = AnimTimelineMakeView(37.0f, 12.0f, 100.0f, 0.0f);
	Zenith_AnimTimelineFrameAll(xNoTrack, 4.0f);
	ZENITH_ASSERT_TRUE(xNoTrack.m_fPixelsPerSecond == fANIM_TIMELINE_DEFAULT_PPS, "no track falls back to the default zoom");

	Zenith_AnimTimelineView xNaN = AnimTimelineMakeView(37.0f, 12.0f, 100.0f, 800.0f);
	Zenith_AnimTimelineFrameAll(xNaN, AnimTimelineNaN());
	ZENITH_ASSERT_TRUE(xNaN.m_fPixelsPerSecond == fANIM_TIMELINE_DEFAULT_PPS, "and so does a NaN duration");

	// A clip too long to fit at the minimum zoom still produces a LEGAL view; the
	// caller scrolls for the rest.
	Zenith_AnimTimelineView xHuge = AnimTimelineMakeView(37.0f, 12.0f, 100.0f, 800.0f);
	Zenith_AnimTimelineFrameAll(xHuge, 10000.0f);
	ZENITH_ASSERT_TRUE(xHuge.m_fPixelsPerSecond == fANIM_TIMELINE_MIN_PPS, "a clip that cannot fit stops at the zoom floor");
	ZENITH_ASSERT_TRUE(AnimTimelineViewIsFinite(xHuge), "and the view is still finite");
}

//==============================================================================
// (10) The ruler ladder: monotonic in zoom, always positive, minor never coarser
// than major. A zero or NaN spacing is an infinite loop in the ruler's draw
// code, which is why "never returns 0" is a pinned property and not a comment.
//==============================================================================
ZENITH_TEST(AnimTimeline, TickLadderIsMonotonicInZoomAndAlwaysPositive)
{
	const u_int auRates[] = { 0u, 4u, 24u, 30u, 60u };
	for (u_int uRate = 0; uRate < 5u; ++uRate)
	{
		float fPreviousMajor = 1.0e30f;
		// Deliberately starts BELOW the floor and ends ABOVE the ceiling, so the
		// sanitised ends of the range are swept too.
		for (float fPps = 1.0f; fPps < fANIM_TIMELINE_MAX_PPS * 4.0f; fPps *= 1.25f)
		{
			const Zenith_AnimTimelineView xView = AnimTimelineMakeView(fPps, 0.0f, 0.0f, 900.0f);
			const Zenith_AnimTimelineTicks xTicks = Zenith_AnimTimelineChooseTicks(xView, auRates[uRate]);

			ZENITH_ASSERT_TRUE(std::isfinite(xTicks.m_fMajorSeconds) && xTicks.m_fMajorSeconds > 0.0f,
				"the major spacing is positive and finite (rate %u, %f px/s)", auRates[uRate], fPps);
			ZENITH_ASSERT_TRUE(std::isfinite(xTicks.m_fMinorSeconds) && xTicks.m_fMinorSeconds > 0.0f,
				"the minor spacing is positive and finite (rate %u, %f px/s)", auRates[uRate], fPps);
			ZENITH_ASSERT_TRUE(xTicks.m_fMinorSeconds <= xTicks.m_fMajorSeconds,
				"a minor tick is never coarser than a major one (rate %u, %f px/s)", auRates[uRate], fPps);
			ZENITH_ASSERT_TRUE(xTicks.m_fMajorSeconds <= fPreviousMajor,
				"zooming IN never widens the label spacing (rate %u, %f px/s)", auRates[uRate], fPps);
			fPreviousMajor = xTicks.m_fMajorSeconds;
		}
	}

	// A nonsense target falls back to the default rather than choosing rung 0.
	const Zenith_AnimTimelineView xView = AnimTimelineMakeView(100.0f, 0.0f, 0.0f, 900.0f);
	const Zenith_AnimTimelineTicks xDefault = Zenith_AnimTimelineChooseTicks(xView, 30u);
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineChooseTicks(xView, 30u, 0.0f).m_fMajorSeconds == xDefault.m_fMajorSeconds,
		"a zero target uses the default spacing");
	ZENITH_ASSERT_TRUE(Zenith_AnimTimelineChooseTicks(xView, 30u, AnimTimelineNaN()).m_fMajorSeconds == xDefault.m_fMajorSeconds,
		"and so does a NaN one");
}

//==============================================================================
// (11) ...and what the ladder actually CHOOSES. Zoomed in, the labels sit on the
// frame grid (0.1 s is not a time a 30 fps key can occupy); zoomed out, they are
// round numbers of seconds.
//==============================================================================
ZENITH_TEST(AnimTimeline, TickLadderPrefersFramesWhenZoomedIn)
{
	// 30 fps at maximum zoom: one frame is 133 px, comfortably past the target.
	const Zenith_AnimTimelineView xMax = AnimTimelineMakeView(fANIM_TIMELINE_MAX_PPS, 0.0f, 0.0f, 900.0f);
	const Zenith_AnimTimelineTicks x30AtMax = Zenith_AnimTimelineChooseTicks(xMax, 30u);
	ZENITH_ASSERT_EQ_FLOAT(x30AtMax.m_fMajorSeconds, Zenith_AnimTimelineFrameToTime(1u, 30u), 1.0e-7f,
		"a fully zoomed-in 30 fps ruler is labelled every frame");
	ZENITH_ASSERT_TRUE(x30AtMax.m_fMinorSeconds == x30AtMax.m_fMajorSeconds,
		"and its minor step is the same rung — there is nothing finer than a frame");

	// 60 fps at the same zoom: one frame is only 66 px, so it takes two.
	const Zenith_AnimTimelineTicks x60AtMax = Zenith_AnimTimelineChooseTicks(xMax, 60u);
	ZENITH_ASSERT_EQ_FLOAT(x60AtMax.m_fMajorSeconds, Zenith_AnimTimelineFrameToTime(2u, 60u), 1.0e-7f,
		"a 60 fps ruler labels every second frame at that zoom");
	ZENITH_ASSERT_EQ_FLOAT(x60AtMax.m_fMinorSeconds, Zenith_AnimTimelineFrameToTime(1u, 60u), 1.0e-7f,
		"with single frames as the minor step");

	// 4 fps (the sway clips) at 400 px/s: one frame is 100 px, two are 200.
	const Zenith_AnimTimelineView x400 = AnimTimelineMakeView(400.0f, 0.0f, 0.0f, 900.0f);
	const Zenith_AnimTimelineTicks x4At400 = Zenith_AnimTimelineChooseTicks(x400, 4u);
	ZENITH_ASSERT_EQ_FLOAT(x4At400.m_fMajorSeconds, 0.5f, 1.0e-6f, "4 fps at 400 px/s labels every two frames");
	ZENITH_ASSERT_EQ_FLOAT(x4At400.m_fMinorSeconds, 0.25f, 1.0e-6f, "with single frames as the minor step");

	// Zoomed all the way out the frame grid is meaningless and seconds take over.
	const Zenith_AnimTimelineView xMin = AnimTimelineMakeView(fANIM_TIMELINE_MIN_PPS, 0.0f, 0.0f, 900.0f);
	const Zenith_AnimTimelineTicks x30AtMin = Zenith_AnimTimelineChooseTicks(xMin, 30u);
	ZENITH_ASSERT_EQ_FLOAT(x30AtMin.m_fMajorSeconds, 15.0f, 1.0e-6f, "a fully zoomed-out ruler is labelled every 15 s");
	ZENITH_ASSERT_EQ_FLOAT(x30AtMin.m_fMinorSeconds, 10.0f, 1.0e-6f, "with 10 s minors");

	// No frame rate at all: a purely decimal ladder.
	const Zenith_AnimTimelineView x1000 = AnimTimelineMakeView(1000.0f, 0.0f, 0.0f, 900.0f);
	const Zenith_AnimTimelineTicks xUnsnapped = Zenith_AnimTimelineChooseTicks(x1000, 0u);
	ZENITH_ASSERT_EQ_FLOAT(xUnsnapped.m_fMajorSeconds, 0.25f, 1.0e-6f, "with no grid the ladder is decimal");
	ZENITH_ASSERT_EQ_FLOAT(xUnsnapped.m_fMinorSeconds, 0.1f, 1.0e-6f, "and its minor step likewise");
}

//==============================================================================
// (12) The visible range IS the two edge mappings — re-derived rather than
// recomputed, so a culling test and a drawn key can never disagree about where
// the window starts.
//==============================================================================
ZENITH_TEST(AnimTimeline, VisibleRangeMatchesTheEdgeMappings)
{
	const Zenith_AnimTimelineView xView = AnimTimelineMakeView(250.0f, 3.0f, 40.0f, 500.0f);
	float fStart = -1.0f;
	float fEnd = -1.0f;
	Zenith_AnimTimelineVisibleRange(xView, fStart, fEnd);
	ZENITH_ASSERT_TRUE(fStart == Zenith_AnimTimelinePixelToTime(xView, 40.0f), "the start IS the left edge's time");
	ZENITH_ASSERT_TRUE(fEnd == Zenith_AnimTimelinePixelToTime(xView, 540.0f), "the end IS the right edge's time");
	ZENITH_ASSERT_EQ_FLOAT(fStart, 3.0f, 1.0e-6f, "which is the scroll");
	ZENITH_ASSERT_EQ_FLOAT(fEnd, 5.0f, 1.0e-6f, "plus 500 px at 250 px/s");

	const Zenith_AnimTimelineView xCollapsed = AnimTimelineMakeView(250.0f, 3.0f, 40.0f, 0.0f);
	Zenith_AnimTimelineVisibleRange(xCollapsed, fStart, fEnd);
	ZENITH_ASSERT_TRUE(fStart == fEnd, "a collapsed track spans no time at all");

	const Zenith_AnimTimelineView xNegative = AnimTimelineMakeView(250.0f, 3.0f, 40.0f, -20.0f);
	Zenith_AnimTimelineVisibleRange(xNegative, fStart, fEnd);
	ZENITH_ASSERT_TRUE(fStart == fEnd, "and a negative width is treated as collapsed, never as a reversed range");

	const Zenith_AnimTimelineView xPoisoned = AnimTimelineMakeView(250.0f, AnimTimelineNaN(), AnimTimelineNaN(), 500.0f);
	Zenith_AnimTimelineVisibleRange(xPoisoned, fStart, fEnd);
	ZENITH_ASSERT_TRUE(std::isfinite(fStart) && std::isfinite(fEnd), "a poisoned view still reports a finite range");
	ZENITH_ASSERT_TRUE(fEnd >= fStart, "and never an inverted one");
}
