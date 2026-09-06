#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_AnimTimelineMath.h"

#include <cmath>

bool Zenith_AnimTimelineMath_ForceLink()
{
	// See the declaration: this is the only thing naming this .obj until the
	// dope-sheet panel lands, and without it /OPT:REF drops the TU and every
	// ZENITH_TEST registrar below with it.
	return true;
}

namespace
{
	//--------------------------------------------------------------------------
	// ★ A NON-FINITE TIME BECOMES AN OFF-SCREEN PIXEL, NEVER A NaN.
	//
	// The mapping's results go straight into ImGui draw-list coordinates, and a
	// NaN there is not a wrong pixel — it is a degenerate rect that trips ImGui's
	// own assertions in a windowed build and silently corrupts the vertex buffer
	// in one without. So an input the arithmetic cannot express is turned into a
	// pixel a million units outside the track, which every visibility test culls
	// and every draw clips. The sign is preserved so a +inf time does not fold
	// onto the left edge next to a -inf one.
	//--------------------------------------------------------------------------
	constexpr float fANIM_TIMELINE_OFFSCREEN_PIXELS = 1.0e6f;

	// Largest magnitude a double may have and still convert to float defined.
	constexpr double dANIM_TIMELINE_FLOAT_LIMIT = 3.0e38;

	float AnimTimelinePixelFromDouble(float fTrackLeftPixel, double dPixel)
	{
		const float fLeft = std::isfinite(fTrackLeftPixel) ? fTrackLeftPixel : 0.0f;
		if (!std::isfinite(dPixel))
		{
			// NaN has no side to be on; put it where -inf goes.
			return dPixel > 0.0 ? fLeft + fANIM_TIMELINE_OFFSCREEN_PIXELS : fLeft - fANIM_TIMELINE_OFFSCREEN_PIXELS;
		}
		if (dPixel > dANIM_TIMELINE_FLOAT_LIMIT)
		{
			return fLeft + fANIM_TIMELINE_OFFSCREEN_PIXELS;
		}
		if (dPixel < -dANIM_TIMELINE_FLOAT_LIMIT)
		{
			return fLeft - fANIM_TIMELINE_OFFSCREEN_PIXELS;
		}
		return static_cast<float>(dPixel);
	}

	float AnimTimelineFiniteOrZero(float fValue)
	{
		return std::isfinite(fValue) ? fValue : 0.0f;
	}

	//--------------------------------------------------------------------------
	// The nice-number ladder the ruler picks its spacing from. FRAMES first —
	// a ruler zoomed in far enough to show individual frames must be labelled in
	// times a key can actually sit on, and a decimal step usually is not one:
	// 0.1 s is frame 2.4 at 24 fps and frame 0.4 at 4 fps, so a ruler ticking
	// every 0.1 s would put its labels between the keys at both rates. Absolute
	// steps take over above the coarsest frame step, where a whole frame is too
	// narrow to read and the grid stops being what the eye is following.
	//
	// Strictly ascending by construction: an absolute rung that would not be an
	// increase on the last frame rung is dropped, so "the rung below" is always
	// a genuinely finer step and the chooser's scan can stop at its first hit.
	//--------------------------------------------------------------------------
	constexpr u_int uANIM_TIMELINE_MAX_LADDER = 24u;

	u_int AnimTimelineBuildTickLadder(u_int uFrameRate, float* pfLadder)
	{
		u_int uCount = 0;

		auto PushRung = [&](float fSeconds)
		{
			if (!(fSeconds > 0.0f) || !std::isfinite(fSeconds) || uCount >= uANIM_TIMELINE_MAX_LADDER)
			{
				return;
			}
			// 1.0001 rather than >: two rungs a rounding apart would give the
			// ruler a "minor" step indistinguishable from its major.
			if (uCount > 0 && fSeconds <= pfLadder[uCount - 1] * 1.0001f)
			{
				return;
			}
			pfLadder[uCount++] = fSeconds;
		};

		constexpr u_int uFRAME_STEP_COUNT = 4u;
		constexpr u_int uSUB_SECOND_COUNT = 5u;
		constexpr u_int uABSOLUTE_COUNT = 13u;

		if (uFrameRate > 0u)
		{
			const u_int auFrameSteps[uFRAME_STEP_COUNT] = { 1u, 2u, 5u, 10u };
			for (u_int u = 0; u < uFRAME_STEP_COUNT; ++u)
			{
				PushRung(static_cast<float>(static_cast<double>(auFrameSteps[u]) / static_cast<double>(uFrameRate)));
			}
		}
		else
		{
			// No grid to hang the fine end off, so the fine end is decimal too.
			const float afSubSecond[uSUB_SECOND_COUNT] = { 0.001f, 0.005f, 0.01f, 0.025f, 0.05f };
			for (u_int u = 0; u < uSUB_SECOND_COUNT; ++u)
			{
				PushRung(afSubSecond[u]);
			}
		}

		const float afAbsolute[uABSOLUTE_COUNT] =
		{
			0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 15.0f, 30.0f, 60.0f, 120.0f, 300.0f, 600.0f
		};
		for (u_int u = 0; u < uABSOLUTE_COUNT; ++u)
		{
			PushRung(afAbsolute[u]);
		}

		if (uCount == 0)
		{
			// Unreachable with the rungs above, but a ruler with a zero-length
			// ladder is an infinite loop rather than an ugly ruler.
			pfLadder[uCount++] = 1.0f;
		}
		return uCount;
	}
}

float Zenith_AnimTimelineEffectivePixelsPerSecond(const Zenith_AnimTimelineView& xView)
{
	const float fPixelsPerSecond = xView.m_fPixelsPerSecond;
	// Written as a failed >= so a NaN takes the floor rather than falling through
	// every comparison and escaping as a NaN.
	if (!(fPixelsPerSecond >= fANIM_TIMELINE_MIN_PPS))
	{
		return fANIM_TIMELINE_MIN_PPS;
	}
	if (fPixelsPerSecond > fANIM_TIMELINE_MAX_PPS)
	{
		return fANIM_TIMELINE_MAX_PPS;
	}
	return fPixelsPerSecond;
}

float Zenith_AnimTimelineTimeToPixel(const Zenith_AnimTimelineView& xView, float fTimeSeconds)
{
	const double dPixelsPerSecond = static_cast<double>(Zenith_AnimTimelineEffectivePixelsPerSecond(xView));
	const double dLeft = static_cast<double>(AnimTimelineFiniteOrZero(xView.m_fTrackLeftPixel));
	const double dScroll = static_cast<double>(AnimTimelineFiniteOrZero(xView.m_fScrollSeconds));
	if (!std::isfinite(fTimeSeconds))
	{
		// Off the side the time points at (NaN counts as "left"), never a NaN
		// pixel — the helper reads the sign of what it is handed, so the time
		// itself is the right thing to pass here.
		return AnimTimelinePixelFromDouble(xView.m_fTrackLeftPixel, static_cast<double>(fTimeSeconds));
	}
	return AnimTimelinePixelFromDouble(xView.m_fTrackLeftPixel,
		dLeft + (static_cast<double>(fTimeSeconds) - dScroll) * dPixelsPerSecond);
}

float Zenith_AnimTimelinePixelToTime(const Zenith_AnimTimelineView& xView, float fPixelX)
{
	const double dPixelsPerSecond = static_cast<double>(Zenith_AnimTimelineEffectivePixelsPerSecond(xView));
	const double dLeft = static_cast<double>(AnimTimelineFiniteOrZero(xView.m_fTrackLeftPixel));
	const float fScroll = AnimTimelineFiniteOrZero(xView.m_fScrollSeconds);
	if (!std::isfinite(fPixelX))
	{
		// No usable position: the left edge is the honest answer.
		return fScroll;
	}
	const double dTime = static_cast<double>(fScroll) + (static_cast<double>(fPixelX) - dLeft) / dPixelsPerSecond;
	if (!std::isfinite(dTime) || dTime > dANIM_TIMELINE_FLOAT_LIMIT || dTime < -dANIM_TIMELINE_FLOAT_LIMIT)
	{
		return fScroll;
	}
	return static_cast<float>(dTime);
}

float Zenith_AnimTimelineSecondsToPixels(const Zenith_AnimTimelineView& xView, float fDeltaSeconds)
{
	const double dPixelsPerSecond = static_cast<double>(Zenith_AnimTimelineEffectivePixelsPerSecond(xView));
	if (!std::isfinite(fDeltaSeconds))
	{
		return 0.0f;
	}
	return AnimTimelinePixelFromDouble(0.0f, static_cast<double>(fDeltaSeconds) * dPixelsPerSecond);
}

float Zenith_AnimTimelinePixelsToSeconds(const Zenith_AnimTimelineView& xView, float fDeltaPixels)
{
	const double dPixelsPerSecond = static_cast<double>(Zenith_AnimTimelineEffectivePixelsPerSecond(xView));
	if (!std::isfinite(fDeltaPixels))
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<double>(fDeltaPixels) / dPixelsPerSecond);
}

bool Zenith_AnimTimelineIsVisible(const Zenith_AnimTimelineView& xView, float fTimeSeconds)
{
	const float fWidth = xView.m_fTrackWidthPixels;
	if (!std::isfinite(fWidth) || !(fWidth > 0.0f))
	{
		// A collapsed track shows nothing. Not "shows the one column at its left
		// edge", which is what an inclusive compare against a zero-width rect
		// would answer.
		return false;
	}
	if (!std::isfinite(fTimeSeconds))
	{
		return false;
	}
	const float fLeft = AnimTimelineFiniteOrZero(xView.m_fTrackLeftPixel);
	const float fPixel = Zenith_AnimTimelineTimeToPixel(xView, fTimeSeconds);
	return fPixel >= fLeft && fPixel <= fLeft + fWidth;
}

void Zenith_AnimTimelineVisibleRange(const Zenith_AnimTimelineView& xView, float& fOutStartSeconds, float& fOutEndSeconds)
{
	const float fLeft = AnimTimelineFiniteOrZero(xView.m_fTrackLeftPixel);
	float fWidth = xView.m_fTrackWidthPixels;
	if (!std::isfinite(fWidth) || fWidth < 0.0f)
	{
		fWidth = 0.0f;
	}
	// Routed through the mapping itself rather than re-derived, so "the time at
	// the left edge" cannot drift from what a key drawn at that edge computes.
	fOutStartSeconds = Zenith_AnimTimelinePixelToTime(xView, fLeft);
	fOutEndSeconds = Zenith_AnimTimelinePixelToTime(xView, fLeft + fWidth);
}

float Zenith_AnimTimelineFrameToTime(u_int uFrame, u_int uFrameRate)
{
	if (uFrameRate == 0u)
	{
		return 0.0f;
	}
	return static_cast<float>(static_cast<double>(uFrame) / static_cast<double>(uFrameRate));
}

u_int Zenith_AnimTimelineTimeToFrame(float fTimeSeconds, u_int uFrameRate)
{
	if (uFrameRate == 0u || !std::isfinite(fTimeSeconds) || !(fTimeSeconds > 0.0f))
	{
		return 0u;
	}
	// floor(x + 0.5) rather than round(): it fixes the tie at "up", which is the
	// half of the rule the unit pins. In double, so the multiply cannot itself
	// push a value across the midpoint it was placed exactly on.
	const double dFrame = std::floor(static_cast<double>(fTimeSeconds) * static_cast<double>(uFrameRate) + 0.5);
	if (!(dFrame > 0.0))
	{
		return 0u;
	}
	const double dMax = static_cast<double>(0xFFFFFFFFu);
	return dFrame >= dMax ? 0xFFFFFFFFu : static_cast<u_int>(dFrame);
}

float Zenith_AnimTimelineSnapToFrame(float fTimeSeconds, u_int uFrameRate)
{
	if (uFrameRate == 0u)
	{
		return fTimeSeconds;
	}
	if (!std::isfinite(fTimeSeconds))
	{
		return 0.0f;
	}
	// ★ Round-trips through FrameToTime rather than dividing here, which is what
	// makes SnapToFrame(FrameToTime(f)) BIT-identical to FrameToTime(f): there is
	// exactly one expression in this file that turns a frame index into seconds.
	return Zenith_AnimTimelineFrameToTime(Zenith_AnimTimelineTimeToFrame(fTimeSeconds, uFrameRate), uFrameRate);
}

void Zenith_AnimTimelineZoomAroundPixel(Zenith_AnimTimelineView& xView, float fPixelX, float fZoomFactor)
{
	if (!std::isfinite(fZoomFactor) || !(fZoomFactor > 0.0f) || !std::isfinite(fPixelX))
	{
		return;
	}

	const float fOldPixelsPerSecond = Zenith_AnimTimelineEffectivePixelsPerSecond(xView);
	const float fTimeUnderCursor = Zenith_AnimTimelinePixelToTime(xView, fPixelX);

	double dNewPixelsPerSecond = static_cast<double>(fOldPixelsPerSecond) * static_cast<double>(fZoomFactor);
	if (!(dNewPixelsPerSecond >= static_cast<double>(fANIM_TIMELINE_MIN_PPS)))
	{
		dNewPixelsPerSecond = static_cast<double>(fANIM_TIMELINE_MIN_PPS);
	}
	else if (dNewPixelsPerSecond > static_cast<double>(fANIM_TIMELINE_MAX_PPS))
	{
		dNewPixelsPerSecond = static_cast<double>(fANIM_TIMELINE_MAX_PPS);
	}
	xView.m_fPixelsPerSecond = static_cast<float>(dNewPixelsPerSecond);

	const double dLeft = static_cast<double>(AnimTimelineFiniteOrZero(xView.m_fTrackLeftPixel));
	const double dScroll = static_cast<double>(fTimeUnderCursor)
		- (static_cast<double>(fPixelX) - dLeft) / dNewPixelsPerSecond;
	xView.m_fScrollSeconds = (std::isfinite(dScroll) && dScroll < dANIM_TIMELINE_FLOAT_LIMIT && dScroll > -dANIM_TIMELINE_FLOAT_LIMIT)
		? static_cast<float>(dScroll)
		: 0.0f;
}

void Zenith_AnimTimelineClamp(Zenith_AnimTimelineView& xView, float fDurationSeconds)
{
	if (!std::isfinite(xView.m_fTrackLeftPixel))
	{
		xView.m_fTrackLeftPixel = 0.0f;
	}
	if (!std::isfinite(xView.m_fTrackWidthPixels) || xView.m_fTrackWidthPixels < 0.0f)
	{
		xView.m_fTrackWidthPixels = 0.0f;
	}

	// Strictly positive afterwards, which is what lets the division below be
	// written without a guard.
	xView.m_fPixelsPerSecond = Zenith_AnimTimelineEffectivePixelsPerSecond(xView);

	const float fDuration = (std::isfinite(fDurationSeconds) && fDurationSeconds > 0.0f) ? fDurationSeconds : 0.0f;
	const float fVisibleSeconds = xView.m_fTrackWidthPixels / xView.m_fPixelsPerSecond;

	float fMaxScroll = fDuration - fVisibleSeconds;
	if (!(fMaxScroll > 0.0f))
	{
		// The whole clip fits (or there is no clip): pinned at the start.
		fMaxScroll = 0.0f;
	}

	float fScroll = xView.m_fScrollSeconds;
	if (!std::isfinite(fScroll) || fScroll < 0.0f)
	{
		fScroll = 0.0f;
	}
	if (fScroll > fMaxScroll)
	{
		fScroll = fMaxScroll;
	}
	xView.m_fScrollSeconds = fScroll;
}

void Zenith_AnimTimelineFrameAll(Zenith_AnimTimelineView& xView, float fDurationSeconds)
{
	if (!std::isfinite(xView.m_fTrackLeftPixel))
	{
		xView.m_fTrackLeftPixel = 0.0f;
	}
	if (!std::isfinite(xView.m_fTrackWidthPixels) || xView.m_fTrackWidthPixels < 0.0f)
	{
		xView.m_fTrackWidthPixels = 0.0f;
	}

	float fPixelsPerSecond = fANIM_TIMELINE_DEFAULT_PPS;
	if (std::isfinite(fDurationSeconds) && fDurationSeconds > 0.0f && xView.m_fTrackWidthPixels > 0.0f)
	{
		fPixelsPerSecond = static_cast<float>(static_cast<double>(xView.m_fTrackWidthPixels)
			/ static_cast<double>(fDurationSeconds));
	}
	xView.m_fPixelsPerSecond = fPixelsPerSecond;
	xView.m_fPixelsPerSecond = Zenith_AnimTimelineEffectivePixelsPerSecond(xView);
	xView.m_fScrollSeconds = 0.0f;
}

Zenith_AnimTimelineTicks Zenith_AnimTimelineChooseTicks(const Zenith_AnimTimelineView& xView, u_int uFrameRate,
                                                        float fTargetMajorPixels)
{
	const float fPixelsPerSecond = Zenith_AnimTimelineEffectivePixelsPerSecond(xView);
	float fTarget = fTargetMajorPixels;
	if (!std::isfinite(fTarget) || !(fTarget > 0.0f))
	{
		fTarget = fANIM_TIMELINE_TARGET_MAJOR_PIXELS;
	}

	float afLadder[uANIM_TIMELINE_MAX_LADDER] = {};
	const u_int uCount = AnimTimelineBuildTickLadder(uFrameRate, afLadder);

	// The smallest rung at least fTarget wide; the coarsest rung when even that
	// is too narrow (a 600 s label spacing on a view zoomed all the way out).
	u_int uChosen = uCount - 1u;
	for (u_int u = 0; u < uCount; ++u)
	{
		if (afLadder[u] * fPixelsPerSecond >= fTarget)
		{
			uChosen = u;
			break;
		}
	}

	Zenith_AnimTimelineTicks xTicks;
	xTicks.m_fMajorSeconds = afLadder[uChosen];
	xTicks.m_fMinorSeconds = uChosen > 0u ? afLadder[uChosen - 1u] : afLadder[0];
	return xTicks;
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_AnimTimelineMath.Tests.inl"
#endif

#endif // ZENITH_TOOLS
