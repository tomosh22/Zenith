#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_EditorUI.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

//=============================================================================
// THE CURVE EDITOR (WU-8.2) — the value axis as pure maths, the drawing, the
// input translation and the Action_* twins.
//
// ★ THE X AXIS IS Zenith_AnimTimelineMath's, UNSHADOWED. Not one function in
// this file converts seconds to pixels itself: every horizontal coordinate is
// Zenith_AnimTimelineTimeToPixel(View(), t), the same call DrawKeysForRow makes
// — which is why the playhead, the ruler ticks and the duration shade line up
// with the curves by construction rather than by two mappings agreeing. The
// VERTICAL axis is a second, independent mapping, and it lives at the top of
// this file as free functions over numbers for the reason the pose-ring geometry
// does: the parts that are easy to get silently wrong (which way up the value
// axis runs, what a handle pixel means as a derivative) are then catchable by a
// headless unit with no frame, no clip and no rig.
//
// ★ WHAT IS DRAWN IS SAMPLED THROUGH Flux_BoneChannel's OWN SAMPLER, one column
// at a time. A Hermite re-derived here would agree with the runtime exactly
// until one of them changed, and a curve editor that lies about the shape is
// worse than not having one.
//
// ★ DRAW-LIST DECORATIONS, NOT ITEMS. Everything below the toolbar is painted
// into the sheet's ImDrawList at absolute screen coordinates, inside the ONE
// InvisibleButton RenderSheet already owns. Placing items there trips
// ErrorCheckUsingSetCursorPosToExtendParentBoundaries, which in a windowed build
// is a modal CRT dialog nothing logs.
//=============================================================================

namespace
{
	bool CurveIsFinite(float fValue)
	{
		return fValue == fValue && fValue > -3.0e38f && fValue < 3.0e38f;
	}

	// The same "a million units outside the area" sentinel Zenith_AnimTimelineMath
	// substitutes for a non-finite time, and for the same reason: these numbers go
	// straight into draw-list coordinates, where a NaN is an assertion in a
	// windowed build and a corrupt vertex buffer in one without.
	constexpr float fCURVE_OFFSCREEN_PIXEL = 1.0e6f;

	// Curve point / handle hit sizes at 1x DPI.
	constexpr float fCURVE_POINT_HALF_1X  = 4.0f;
	constexpr float fCURVE_HANDLE_HALF_1X = 4.0f;
	// A press-and-release inside this many pixels is a CLICK on the handle, not a
	// drag: without it a click that drifted one pixel would commit a tangent the
	// user did not mean to change.
	constexpr float fCURVE_DRAG_SLOP_1X = 2.0f;

	// One column per pixel — "what is drawn is what plays" is only true at the
	// resolution it is drawn at — with a belt so a degenerate layout cannot make
	// the loop unbounded.
	constexpr u_int uCURVE_MAX_COLUMNS = 4096u;

	// X / Y / Z, the same three colours Zenith_ImGuiWidgets::Vec3Field tags its
	// fields with, so "the green curve" and "the green field" are the same axis.
	ImU32 CurveComponentColour(u_int uComponent)
	{
		switch (uComponent)
		{
		case 0u: return IM_COL32(220, 90, 90, 255);
		case 1u: return IM_COL32(110, 200, 110, 255);
		default: return IM_COL32(100, 150, 235, 255);
		}
	}

	ImVec2 CurveVec(float fX, float fY) { return ImVec2(fX, fY); }
}

//=============================================================================
// The VALUE axis — pure.
//=============================================================================

float Zenith_AnimCurveEffectivePixelsPerUnit(const Zenith_AnimCurveValueView& xView)
{
	const float fPixelsPerUnit = xView.m_fPixelsPerUnit;
	if (!CurveIsFinite(fPixelsPerUnit) || fPixelsPerUnit <= 0.0f)
	{
		return fANIM_CURVE_MIN_PPU;
	}
	if (fPixelsPerUnit < fANIM_CURVE_MIN_PPU) { return fANIM_CURVE_MIN_PPU; }
	if (fPixelsPerUnit > fANIM_CURVE_MAX_PPU) { return fANIM_CURVE_MAX_PPU; }
	return fPixelsPerUnit;
}

float Zenith_AnimCurveValueToPixel(const Zenith_AnimCurveValueView& xView, float fValue)
{
	const float fTop = CurveIsFinite(xView.m_fTopPixel) ? xView.m_fTopPixel : 0.0f;
	if (!CurveIsFinite(fValue))
	{
		return fTop + fCURVE_OFFSCREEN_PIXEL;
	}
	const float fValueAtTop = CurveIsFinite(xView.m_fValueAtTop) ? xView.m_fValueAtTop : 0.0f;
	// Doubles for the intermediate, so the only loss is rounding the returned
	// float — the same treatment, for the same reason, as the time mapping's.
	const double dPixel = static_cast<double>(fTop)
		+ (static_cast<double>(fValueAtTop) - static_cast<double>(fValue))
		* static_cast<double>(Zenith_AnimCurveEffectivePixelsPerUnit(xView));
	if (dPixel > static_cast<double>(fTop) + fCURVE_OFFSCREEN_PIXEL) { return fTop + fCURVE_OFFSCREEN_PIXEL; }
	if (dPixel < static_cast<double>(fTop) - fCURVE_OFFSCREEN_PIXEL) { return fTop - fCURVE_OFFSCREEN_PIXEL; }
	return static_cast<float>(dPixel);
}

float Zenith_AnimCurvePixelToValue(const Zenith_AnimCurveValueView& xView, float fPixelY)
{
	const float fValueAtTop = CurveIsFinite(xView.m_fValueAtTop) ? xView.m_fValueAtTop : 0.0f;
	if (!CurveIsFinite(fPixelY))
	{
		// The top edge — the mirror of the time mapping answering with the scroll.
		return fValueAtTop;
	}
	const float fTop = CurveIsFinite(xView.m_fTopPixel) ? xView.m_fTopPixel : 0.0f;
	const double dValue = static_cast<double>(fValueAtTop)
		- (static_cast<double>(fPixelY) - static_cast<double>(fTop))
		/ static_cast<double>(Zenith_AnimCurveEffectivePixelsPerUnit(xView));
	return static_cast<float>(dValue);
}

void Zenith_AnimCurveClamp(Zenith_AnimCurveValueView& xView)
{
	if (!CurveIsFinite(xView.m_fTopPixel))    { xView.m_fTopPixel = 0.0f; }
	if (!CurveIsFinite(xView.m_fHeightPixels) || xView.m_fHeightPixels < 0.0f) { xView.m_fHeightPixels = 0.0f; }
	if (!CurveIsFinite(xView.m_fValueAtTop))  { xView.m_fValueAtTop = 0.0f; }
	xView.m_fPixelsPerUnit = Zenith_AnimCurveEffectivePixelsPerUnit(xView);
}

void Zenith_AnimCurveFitRange(Zenith_AnimCurveValueView& xView, float fMinValue, float fMaxValue)
{
	Zenith_AnimCurveClamp(xView);

	const bool bUsableRange = CurveIsFinite(fMinValue) && CurveIsFinite(fMaxValue) && fMaxValue > fMinValue;
	if (!bUsableRange || xView.m_fHeightPixels <= 0.0f)
	{
		// ★ A DEGENERATE RANGE IS CENTRED, NOT DIVIDED BY. One flat curve is a
		// perfectly ordinary thing to ask to fit, and it has no extent to scale to;
		// centring it at the default zoom puts it on screen, which is what the
		// gesture meant.
		const float fCentre = CurveIsFinite(fMinValue) ? fMinValue : 0.0f;
		xView.m_fPixelsPerUnit = fANIM_CURVE_DEFAULT_PPU;
		const float fHalfSpan = (xView.m_fHeightPixels * 0.5f) / Zenith_AnimCurveEffectivePixelsPerUnit(xView);
		xView.m_fValueAtTop = fCentre + fHalfSpan;
		Zenith_AnimCurveClamp(xView);
		return;
	}

	// A 10% margin at each end, so the extreme keys are not painted on the edge
	// where their handles would be clipped away.
	const float fSpan = (fMaxValue - fMinValue) * 1.2f;
	xView.m_fPixelsPerUnit = xView.m_fHeightPixels / fSpan;
	const float fEffective = Zenith_AnimCurveEffectivePixelsPerUnit(xView);
	xView.m_fPixelsPerUnit = fEffective;
	const float fVisibleSpan = xView.m_fHeightPixels / fEffective;
	const float fCentre = (fMinValue + fMaxValue) * 0.5f;
	xView.m_fValueAtTop = fCentre + fVisibleSpan * 0.5f;
	Zenith_AnimCurveClamp(xView);
}

void Zenith_AnimCurveHandlePixel(const Zenith_AnimTimelineView& xTimeView,
	const Zenith_AnimCurveValueView& xValueView, float fKeyTimeSeconds, float fKeyValue,
	float fTangent, bool bIn, float fHandleSeconds, float& fOutPixelX, float& fOutPixelY)
{
	float fLever = CurveIsFinite(fHandleSeconds) ? fHandleSeconds : fANIM_CURVE_HANDLE_SECONDS;
	if (fLever < fANIM_CURVE_MIN_HANDLE_SECONDS) { fLever = fANIM_CURVE_MIN_HANDLE_SECONDS; }
	// ★ SIGNED BY WHICH END IT IS: out at +h, in at -h. BOTH points then lie on
	// the line through the key with slope m, which is what makes the inverse
	// below one expression for both ends.
	const float fSignedLever = bIn ? -fLever : fLever;
	const float fTangentValue = CurveIsFinite(fTangent) ? fTangent : 0.0f;

	fOutPixelX = Zenith_AnimTimelineTimeToPixel(xTimeView, fKeyTimeSeconds + fSignedLever);
	fOutPixelY = Zenith_AnimCurveValueToPixel(xValueView, fKeyValue + fTangentValue * fSignedLever);
}

float Zenith_AnimCurveTangentFromPixel(const Zenith_AnimTimelineView& xTimeView,
	const Zenith_AnimCurveValueView& xValueView, float fKeyTimeSeconds, float fKeyValue,
	bool bIn, float fPixelX, float fPixelY)
{
	if (!CurveIsFinite(fKeyTimeSeconds) || !CurveIsFinite(fKeyValue))
	{
		return 0.0f;
	}
	const float fDroppedTime = Zenith_AnimTimelinePixelToTime(xTimeView, fPixelX);
	const float fDroppedValue = Zenith_AnimCurvePixelToValue(xValueView, fPixelY);
	if (!CurveIsFinite(fDroppedTime) || !CurveIsFinite(fDroppedValue))
	{
		return 0.0f;
	}

	float fLever = fDroppedTime - fKeyTimeSeconds;
	// ★ THE LEVER'S SIGN IS THE HANDLE'S, NOT THE CURSOR'S. An out handle dragged
	// to the LEFT of its key still means "the slope of the line through these two
	// points", which is what the user sees; taking the raw signed difference would
	// flip the tangent as the cursor crossed the key's column, which reads as the
	// handle snapping inside out.
	const float fMagnitude = std::fabs(fLever) < fANIM_CURVE_MIN_HANDLE_SECONDS
		? fANIM_CURVE_MIN_HANDLE_SECONDS
		: std::fabs(fLever);
	fLever = bIn ? -fMagnitude : fMagnitude;

	const float fTangent = (fDroppedValue - fKeyValue) / fLever;
	return CurveIsFinite(fTangent) ? fTangent : 0.0f;
}

const char* Zenith_AnimCurveTangentModeLabel(Flux_TangentMode eMode)
{
	// ★ A TOTAL SWITCH OVER THE WIRE ENUM, with no default: adding a fifth mode to
	// Flux_TangentMode would fail the build HERE, on the one function that turns a
	// mode into the word a user reads, rather than quietly labelling it "Custom".
	switch (eMode)
	{
	case Flux_TangentMode::LINEAR: return "Linear";
	case Flux_TangentMode::FLAT:   return "Flat";
	case Flux_TangentMode::AUTO:   return "Auto";
	case Flux_TangentMode::CUSTOM: return "Custom";
	}
	// Unreachable for a value the enum has; a corrupt byte never gets this far
	// (Flux_ReadKeyTangents refuses one above uFLUX_TANGENT_MODE_MAX).
	return "Custom";
}

bool Zenith_AnimCurveTangentModeOf(const Flux_KeyTangents& xTangents,
	Zenith_AnimTangentEnd eEnd, Flux_TangentMode& eOut)
{
	// ★ THE STORED MODE, READ — never a re-derivation from the vectors. The clip
	// says what each end IS, and a display that answered that question a second way
	// would disagree with the file the moment a FLAT end sat over the same zeroes a
	// LINEAR one does, and it would disagree SILENTLY.
	switch (eEnd)
	{
	case ZENITH_ANIM_TANGENT_END_IN:
		eOut = xTangents.m_eInMode;
		return true;
	case ZENITH_ANIM_TANGENT_END_OUT:
		eOut = xTangents.m_eOutMode;
		return true;
	case ZENITH_ANIM_TANGENT_END_BOTH:
		// ★ MIXED IS REPORTED THROUGH THE BOOL, NEVER AS A FIFTH VALUE. IN=LINEAR /
		// OUT=FLAT is a legal key since B2, and any single answer for one is wrong
		// about an end the user can see.
		if (xTangents.m_eInMode != xTangents.m_eOutMode)
		{
			return false;
		}
		eOut = xTangents.m_eInMode;
		return true;
	}
	return false;
}

//=============================================================================
// Rect keys and accessors
//=============================================================================

u_int64 Zenith_EditorPanel_Animation::MakeCurveRectKey(u_int uRowIndex, u_int uKeyId, u_int uComponent, bool bIn)
{
	return (static_cast<u_int64>(uRowIndex & 0xFFFFFFu) << 40)
		| (static_cast<u_int64>(uKeyId) << 8)
		| (static_cast<u_int64>(uComponent & 0x3u) << 1)
		| (bIn ? 1ull : 0ull);
}

bool Zenith_EditorPanel_Animation::GetCurveViewRect(Zenith_AnimPanelRect& xOut) const
{
	return m_bCurveViewRectValid ? PublishRect(&m_xCurveViewRect, xOut) : false;
}

bool Zenith_EditorPanel_Animation::GetCurveKeyRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	u_int uComponent, Zenith_AnimPanelRect& xOut) const
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (uComponent >= uANIM_CURVE_COMPONENT_COUNT || !FindRowIndexForTrack(xTrack, uRow))
	{
		return false;
	}
	return PublishRect(m_xCurveKeyRects.TryGet(MakeCurveRectKey(uRow, uKeyId, uComponent, false)), xOut);
}

bool Zenith_EditorPanel_Animation::GetCurveHandleRect(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	u_int uComponent, bool bIn, Zenith_AnimPanelRect& xOut) const
{
	u_int uRow = uINVALID_ANIM_SHEET_ROW;
	if (uComponent >= uANIM_CURVE_COMPONENT_COUNT || !FindRowIndexForTrack(xTrack, uRow))
	{
		return false;
	}
	return PublishRect(m_xCurveHandleRects.TryGet(MakeCurveRectKey(uRow, uKeyId, uComponent, bIn)), xOut);
}

bool Zenith_EditorPanel_Animation::GetKeyTangentMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	Zenith_AnimTangentEnd eEnd, Flux_TangentMode& eOut) const
{
	Flux_KeyTangents xTangents;
	if (!m_xDocument.GetKeyTangents(xTrack, uKeyId, xTangents))
	{
		// The key does not resolve, or the track is root motion — which has no
		// tangent array at all (D17) and therefore no mode, refused rather than
		// answered with LINEAR.
		return false;
	}
	return Zenith_AnimCurveTangentModeOf(xTangents, eEnd, eOut);
}

//=============================================================================
// Which tracks the view draws, and what it plots for them
//=============================================================================

void Zenith_EditorPanel_Animation::GetCurveTracks(Zenith_Vector<Zenith_AnimTrackId>& axOut) const
{
	axOut.Clear();
	if (!m_xDocument.IsOpen())
	{
		return;
	}

	// ★ ROW ORDER, BY WALKING THE ROWS. The alternative — collecting the
	// selection's tracks and sorting them afterwards — needs a second definition
	// of "which row is this track on"; walking the row model gives the order for
	// free and cannot disagree with what the dope sheet would have shown.
	const bool bHaveSelection = m_axSelectedKeys.GetSize() > 0u;
	for (u_int uRow = 0; uRow < m_axRows.GetSize(); ++uRow)
	{
		const Zenith_AnimSheetRow& xRow = m_axRows.Get(uRow);
		if (!xRow.m_bHasTrack || xRow.m_xTrack.m_bRootMotion)
		{
			// ★ ROOT MOTION IS NOT DRAWN. Its two delta tracks carry no tangent
			// array (D17) and are still sampled linearly after WU-8.1, so a handle
			// there would be a control with nothing behind it.
			continue;
		}
		if (m_xDocument.GetKeyCount(xRow.m_xTrack) == 0u)
		{
			continue;
		}

		if (bHaveSelection)
		{
			bool bNamed = false;
			for (u_int u = 0; u < m_axSelectedKeys.GetSize() && !bNamed; ++u)
			{
				bNamed = m_axSelectedKeys.Get(u).m_xTrack == xRow.m_xTrack;
			}
			if (!bNamed)
			{
				continue;
			}
		}
		else if (axOut.GetSize() >= uANIM_CURVE_MAX_UNSELECTED_TRACKS)
		{
			// The cap applies ONLY without a selection: a selection is the user
			// saying which curves they want, and silently dropping one of those
			// would be an edit refused for a reason nobody could see.
			break;
		}

		axOut.PushBack(xRow.m_xTrack);
	}
}

bool Zenith_EditorPanel_Animation::SampleCurveValue(const Zenith_AnimTrackId& xTrack, u_int uComponent,
	float fTimeSeconds, float& fOutValue) const
{
	if (uComponent >= uANIM_CURVE_COMPONENT_COUNT || xTrack.m_bRootMotion || !m_xDocument.IsOpen())
	{
		return false;
	}
	const Flux_BoneChannel* pxChannel = m_xDocument.GetClip().GetBoneChannel(xTrack.m_strBoneName);
	if (pxChannel == nullptr)
	{
		return false;
	}

	switch (xTrack.m_eTrack)
	{
	case FLUX_ANIM_TRACK_POSITION:
		fOutValue = pxChannel->SamplePosition(fTimeSeconds)[static_cast<int>(uComponent)];
		return true;
	case FLUX_ANIM_TRACK_SCALE:
		fOutValue = pxChannel->SampleScale(fTimeSeconds)[static_cast<int>(uComponent)];
		return true;
	case FLUX_ANIM_TRACK_ROTATION:
	{
		// ★ EULER ANGLES IN RADIANS, so the value axis and the stored tangent
		// (axis * rad/s) are in the same units and one handle arithmetic serves
		// both track kinds. It is a PRESENTATION, not an identity: an Euler rate
		// and a body-frame angular-velocity component agree for a rotation about
		// one axis and diverge as the other two wind up, and glm::eulerAngles has
		// its own branch cuts, so a tumbling rotation draws with seams. The
		// alternative is three quaternion-derivative curves nobody can read.
		const Zenith_Maths::Vector3 xEuler = glm::eulerAngles(pxChannel->SampleRotation(fTimeSeconds));
		fOutValue = xEuler[static_cast<int>(uComponent)];
		return true;
	}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::GetCurveKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	u_int uComponent, float& fOutValue) const
{
	if (uComponent >= uANIM_CURVE_COMPONENT_COUNT || xTrack.m_bRootMotion)
	{
		return false;
	}
	Zenith_AnimKeyValue xValue;
	if (!m_xDocument.GetKeyValue(xTrack, uKeyId, xValue))
	{
		return false;
	}
	if (xValue.m_bIsRotation)
	{
		const Zenith_Maths::Vector3 xEuler = glm::eulerAngles(xValue.m_xQuat);
		fOutValue = xEuler[static_cast<int>(uComponent)];
		return true;
	}
	fOutValue = xValue.m_xVector[static_cast<int>(uComponent)];
	return true;
}

bool Zenith_EditorPanel_Animation::ComputeCurveValueRange(float& fOutMin, float& fOutMax) const
{
	Zenith_Vector<Zenith_AnimTrackId> axTracks;
	GetCurveTracks(axTracks);
	if (axTracks.GetSize() == 0u)
	{
		return false;
	}

	float fStart = 0.0f;
	float fEnd = 0.0f;
	Zenith_AnimTimelineVisibleRange(m_xView, fStart, fEnd);
	if (!(fEnd > fStart))
	{
		// No visible span: fall back to the whole clip, so a fit issued before the
		// first layout still produces a usable axis instead of refusing.
		fStart = 0.0f;
		fEnd = m_xDocument.GetDuration();
	}

	bool bAny = false;
	float fMin = 0.0f;
	float fMax = 0.0f;
	// 64 samples per curve: enough to catch an overshoot a Hermite produces
	// BETWEEN keys (which is exactly what fit-to-selection has to include, and
	// what a key-values-only extent would miss), cheap enough to run on a click.
	constexpr u_int uFitSamples = 64u;
	for (u_int uTrack = 0; uTrack < axTracks.GetSize(); ++uTrack)
	{
		for (u_int uComponent = 0; uComponent < uANIM_CURVE_COMPONENT_COUNT; ++uComponent)
		{
			for (u_int uSample = 0; uSample <= uFitSamples; ++uSample)
			{
				const float fT = fStart + (fEnd - fStart) * (static_cast<float>(uSample) / static_cast<float>(uFitSamples));
				float fValue = 0.0f;
				if (!SampleCurveValue(axTracks.Get(uTrack), uComponent, fT, fValue) || !CurveIsFinite(fValue))
				{
					continue;
				}
				if (!bAny || fValue < fMin) { fMin = fValue; }
				if (!bAny || fValue > fMax) { fMax = fValue; }
				bAny = true;
			}
		}
	}

	if (!bAny)
	{
		return false;
	}
	fOutMin = fMin;
	fOutMax = fMax;
	return true;
}

void Zenith_EditorPanel_Animation::UpdateCurveValueViewGeometry(const SheetLayout& xLayout)
{
	// The curve area is the canvas's ROW REGION — below the ruler, right of the
	// label gutter. That is the whole of "the curve view REPLACES the rows": it
	// occupies exactly the rectangle they would have.
	m_xCurveValueView.m_fTopPixel = xLayout.m_fRowsTop;
	const float fHeight = xLayout.m_fCanvasBottom - xLayout.m_fRowsTop;
	m_xCurveValueView.m_fHeightPixels = fHeight > 0.0f ? fHeight : 0.0f;
	Zenith_AnimCurveClamp(m_xCurveValueView);
}

//=============================================================================
// Drawing
//=============================================================================

void Zenith_EditorPanel_Animation::DrawCurveView(ImDrawList* pxDraw, const SheetLayout& xLayout)
{
	m_uCurveTracksDrawn = 0;
	m_axCurveTracksDrawn.Clear();

	UpdateCurveValueViewGeometry(xLayout);

	if (!m_xDocument.IsOpen() || xLayout.m_fTrackWidth <= 0.0f || m_xCurveValueView.m_fHeightPixels <= 0.0f)
	{
		return;
	}

	// ★ THE DEFERRED FIT, for the reason m_bPendingFrameAll exists:
	// Zenith_AnimCurveFitRange needs a HEIGHT to divide by, and at the moment the
	// view is switched on the panel has not been laid out — so a fit issued there
	// would hit the degenerate branch and silently leave the default zoom.
	if (m_bPendingCurveFit)
	{
		m_bPendingCurveFit = false;
		float fMin = 0.0f;
		float fMax = 0.0f;
		if (ComputeCurveValueRange(fMin, fMax))
		{
			Zenith_AnimCurveFitRange(m_xCurveValueView, fMin, fMax);
		}
	}

	const Zenith_EditorPalette& xPalette = Zenith_EditorUI::Palette();
	const float fLeft = xLayout.m_fTrackLeft;
	const float fRight = xLayout.m_fTrackLeft + xLayout.m_fTrackWidth;
	const float fTop = m_xCurveValueView.m_fTopPixel;
	const float fBottom = fTop + m_xCurveValueView.m_fHeightPixels;

	m_xCurveViewRect.m_fMinX = fLeft;
	m_xCurveViewRect.m_fMinY = fTop;
	m_xCurveViewRect.m_fMaxX = fRight;
	m_xCurveViewRect.m_fMaxY = fBottom;
	m_bCurveViewRectValid = true;

	// The zero line — the one horizontal reference a value axis needs, and the
	// only chrome the curve area draws.
	const float fZeroY = Zenith_AnimCurveValueToPixel(m_xCurveValueView, 0.0f);
	if (fZeroY >= fTop && fZeroY <= fBottom)
	{
		pxDraw->AddLine(CurveVec(fLeft, fZeroY), CurveVec(fRight, fZeroY), xPalette.m_uBorder);
	}

	Zenith_Vector<Zenith_AnimTrackId> axTracks;
	GetCurveTracks(axTracks);
	m_uCurveTracksDrawn = axTracks.GetSize();
	m_axCurveTracksDrawn = axTracks;

	const float fPointHalf = Zenith_EditorUI::Px(fCURVE_POINT_HALF_1X);
	const float fHandleHalf = Zenith_EditorUI::Px(fCURVE_HANDLE_HALF_1X);
	// One column per pixel, so the polyline IS the sampler's output rather than a
	// smoothing of it.
	const u_int uColumns = std::min(static_cast<u_int>(xLayout.m_fTrackWidth) + 1u, uCURVE_MAX_COLUMNS);

	Zenith_Vector<ImVec2> axPolyline;
	axPolyline.Reserve(uColumns);

	for (u_int uTrackIndex = 0; uTrackIndex < axTracks.GetSize(); ++uTrackIndex)
	{
		const Zenith_AnimTrackId& xTrack = axTracks.Get(uTrackIndex);
		u_int uRow = uINVALID_ANIM_SHEET_ROW;
		if (!FindRowIndexForTrack(xTrack, uRow))
		{
			continue;
		}

		for (u_int uComponent = 0; uComponent < uANIM_CURVE_COMPONENT_COUNT; ++uComponent)
		{
			const ImU32 uColour = CurveComponentColour(uComponent);

			axPolyline.Clear();
			for (u_int uColumn = 0; uColumn < uColumns; ++uColumn)
			{
				const float fX = fLeft + static_cast<float>(uColumn);
				if (fX > fRight)
				{
					break;
				}
				// ★ THROUGH THE REAL SAMPLER. This is the line that makes the drawn
				// curve and the played curve the same object.
				float fValue = 0.0f;
				if (!SampleCurveValue(xTrack, uComponent, Zenith_AnimTimelinePixelToTime(m_xView, fX), fValue))
				{
					continue;
				}
				float fY = Zenith_AnimCurveValueToPixel(m_xCurveValueView, fValue);
				// Clamped rather than culled: a curve that leaves the top of the area
				// and comes back has to keep its two crossings joined, or it reads as
				// two unrelated curves.
				if (fY < fTop - 1.0f) { fY = fTop - 1.0f; }
				if (fY > fBottom + 1.0f) { fY = fBottom + 1.0f; }
				axPolyline.PushBack(CurveVec(fX, fY));
			}
			if (axPolyline.GetSize() >= 2u)
			{
				pxDraw->AddPolyline(axPolyline.GetDataPointer(), static_cast<int>(axPolyline.GetSize()),
					uColour, ImDrawFlags_None, 1.5f);
			}

			// ---- the keys, and their handles --------------------------------
			const u_int uKeyCount = m_xDocument.GetKeyCount(xTrack);
			for (u_int uKey = 0; uKey < uKeyCount; ++uKey)
			{
				const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xTrack, uKey);
				float fTime = 0.0f;
				float fKeyValue = 0.0f;
				if (!m_xDocument.GetKeyTime(xTrack, uKeyId, fTime)
				 || !GetCurveKeyValue(xTrack, uKeyId, uComponent, fKeyValue))
				{
					continue;
				}
				// The mapping's own visibility test, not a pixel compare: it is false
				// for a zero-width track, which is the one case a bare compare gets
				// wrong.
				if (!Zenith_AnimTimelineIsVisible(m_xView, fTime))
				{
					continue;
				}
				const float fKeyX = Zenith_AnimTimelineTimeToPixel(m_xView, fTime);
				const float fKeyY = Zenith_AnimCurveValueToPixel(m_xCurveValueView, fKeyValue);
				if (!CurveIsFinite(fKeyX) || !CurveIsFinite(fKeyY))
				{
					continue;
				}

				const bool bSelected = IsKeySelected(xTrack, uKeyId);
				pxDraw->AddRectFilled(CurveVec(fKeyX - fPointHalf, fKeyY - fPointHalf),
					CurveVec(fKeyX + fPointHalf, fKeyY + fPointHalf),
					bSelected ? xPalette.m_uSelection : uColour);
				pxDraw->AddRect(CurveVec(fKeyX - fPointHalf, fKeyY - fPointHalf),
					CurveVec(fKeyX + fPointHalf, fKeyY + fPointHalf),
					bSelected ? xPalette.m_uTextBright : xPalette.m_uBorder);

				Zenith_AnimPanelRect xPointRect;
				xPointRect.m_fMinX = fKeyX - fPointHalf;
				xPointRect.m_fMinY = fKeyY - fPointHalf;
				xPointRect.m_fMaxX = fKeyX + fPointHalf;
				xPointRect.m_fMaxY = fKeyY + fPointHalf;
				m_xCurveKeyRects[MakeCurveRectKey(uRow, uKeyId, uComponent, false)] = xPointRect;

				Flux_KeyTangents xTangents;
				if (!m_xDocument.GetKeyTangents(xTrack, uKeyId, xTangents))
				{
					continue;
				}

				for (u_int uEnd = 0; uEnd < 2u; ++uEnd)
				{
					const bool bIn = (uEnd == 0u);
					const Zenith_Maths::Vector3& xTangent = bIn ? xTangents.m_xInTangent : xTangents.m_xOutTangent;
					float fHandleX = 0.0f;
					float fHandleY = 0.0f;

					// ★ THE GHOST, AND WHY IT IS DRAWN RATHER THAN READ BACK. Nothing
					// reaches the document until the button comes up, so while THIS
					// handle is being dragged its position comes from the cursor and
					// its stored tangent is still the old one — the same
					// preview-then-commit shape as the key drag's ghost diamond.
					const bool bBeingDragged = m_bCurveHandleDragActive
						&& m_bCurveDragIn == bIn
						&& m_uCurveDragComponent == uComponent
						&& m_uCurveDragKeyId == uKeyId
						&& m_xCurveDragTrack == xTrack;
					if (bBeingDragged)
					{
						fHandleX = m_fCurveDragPixelX;
						fHandleY = m_fCurveDragPixelY;
					}
					else
					{
						Zenith_AnimCurveHandlePixel(m_xView, m_xCurveValueView, fTime, fKeyValue,
							xTangent[static_cast<int>(uComponent)], bIn, fANIM_CURVE_HANDLE_SECONDS,
							fHandleX, fHandleY);
					}
					if (!CurveIsFinite(fHandleX) || !CurveIsFinite(fHandleY))
					{
						continue;
					}

					pxDraw->AddLine(CurveVec(fKeyX, fKeyY), CurveVec(fHandleX, fHandleY),
						bBeingDragged ? xPalette.m_uTextBright : xPalette.m_uTextDim);
					pxDraw->AddRectFilled(CurveVec(fHandleX - fHandleHalf, fHandleY - fHandleHalf),
						CurveVec(fHandleX + fHandleHalf, fHandleY + fHandleHalf),
						bBeingDragged ? xPalette.m_uTextBright : uColour);

					Zenith_AnimPanelRect xHandleRect;
					xHandleRect.m_fMinX = fHandleX - fHandleHalf;
					xHandleRect.m_fMinY = fHandleY - fHandleHalf;
					xHandleRect.m_fMaxX = fHandleX + fHandleHalf;
					xHandleRect.m_fMaxY = fHandleY + fHandleHalf;
					m_xCurveHandleRects[MakeCurveRectKey(uRow, uKeyId, uComponent, bIn)] = xHandleRect;
				}
			}
		}
	}
}

//=============================================================================
// Input translation — the ONLY ImGui-reading function here
//=============================================================================

bool Zenith_EditorPanel_Animation::FindCurveHandleAtScreenPos(float fX, float fY,
	Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId, u_int& uOutComponent, bool& bOutIn) const
{
	for (u_int uTrackIndex = 0; uTrackIndex < m_axCurveTracksDrawn.GetSize(); ++uTrackIndex)
	{
		const Zenith_AnimTrackId& xTrack = m_axCurveTracksDrawn.Get(uTrackIndex);
		const u_int uKeyCount = m_xDocument.GetKeyCount(xTrack);
		for (u_int uKey = 0; uKey < uKeyCount; ++uKey)
		{
			const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xTrack, uKey);
			for (u_int uComponent = 0; uComponent < uANIM_CURVE_COMPONENT_COUNT; ++uComponent)
			{
				for (u_int uEnd = 0; uEnd < 2u; ++uEnd)
				{
					const bool bIn = (uEnd == 0u);
					Zenith_AnimPanelRect xRect;
					// ★ THROUGH THE PUBLISHED ACCESSOR, so a click can never land on a
					// handle the off-screen gate would have refused to hand out a
					// coordinate for.
					if (!GetCurveHandleRect(xTrack, uKeyId, uComponent, bIn, xRect))
					{
						continue;
					}
					if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
					{
						xOutTrack = xTrack;
						uOutKeyId = uKeyId;
						uOutComponent = uComponent;
						bOutIn = bIn;
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::FindCurvePointAtScreenPos(float fX, float fY,
	Zenith_AnimTrackId& xOutTrack, u_int& uOutKeyId) const
{
	for (u_int uTrackIndex = 0; uTrackIndex < m_axCurveTracksDrawn.GetSize(); ++uTrackIndex)
	{
		const Zenith_AnimTrackId& xTrack = m_axCurveTracksDrawn.Get(uTrackIndex);
		const u_int uKeyCount = m_xDocument.GetKeyCount(xTrack);
		for (u_int uKey = 0; uKey < uKeyCount; ++uKey)
		{
			const u_int uKeyId = m_xDocument.GetKeyIdAtIndex(xTrack, uKey);
			for (u_int uComponent = 0; uComponent < uANIM_CURVE_COMPONENT_COUNT; ++uComponent)
			{
				Zenith_AnimPanelRect xRect;
				if (!GetCurveKeyRect(xTrack, uKeyId, uComponent, xRect))
				{
					continue;
				}
				if (fX >= xRect.m_fMinX && fX <= xRect.m_fMaxX && fY >= xRect.m_fMinY && fY <= xRect.m_fMaxY)
				{
					xOutTrack = xTrack;
					uOutKeyId = uKeyId;
					return true;
				}
			}
		}
	}
	return false;
}

bool Zenith_EditorPanel_Animation::HandleCurveInput(const SheetLayout& xLayout, bool bCanvasHovered)
{
	(void)xLayout;
	const ImGuiIO& xIO = ImGui::GetIO();

	if (m_bCurveHandleDragActive)
	{
		const float fSlop = Zenith_EditorUI::Px(fCURVE_DRAG_SLOP_1X);
		if (std::fabs(xIO.MousePos.x - m_fCurveDragPixelX) > fSlop
		 || std::fabs(xIO.MousePos.y - m_fCurveDragPixelY) > fSlop)
		{
			m_bCurveDragMoved = true;
		}
		m_fCurveDragPixelX = xIO.MousePos.x;
		m_fCurveDragPixelY = xIO.MousePos.y;

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			// ★ ONE ACTION, ON RELEASE. A click that never moved records NOTHING —
			// the same rule Zenith_Editor::RecordGizmoDragUndo and the bone drag
			// follow, and the reason an accidental nudge does not fill the undo
			// stack with tangents nobody changed.
			if (m_bCurveDragMoved)
			{
				Action_DragTangentHandleToPixel(m_xCurveDragTrack, m_uCurveDragKeyId, m_uCurveDragComponent,
					m_bCurveDragIn, m_fCurveDragPixelX, m_fCurveDragPixelY);
			}
			m_bCurveHandleDragActive = false;
			m_bCurveDragMoved = false;
			m_uCurveDragKeyId = uINVALID_ANIM_KEY_ID;
		}
		return true;
	}

	if (!bCanvasHovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		return false;
	}

	// Handles BEFORE points: at a short lever a handle overlaps its own key, and
	// the handle is what the cursor was reaching for.
	Zenith_AnimTrackId xTrack;
	u_int uKeyId = uINVALID_ANIM_KEY_ID;
	u_int uComponent = 0u;
	bool bIn = false;
	if (FindCurveHandleAtScreenPos(xIO.MousePos.x, xIO.MousePos.y, xTrack, uKeyId, uComponent, bIn))
	{
		m_bCurveHandleDragActive = true;
		m_bCurveDragMoved = false;
		m_xCurveDragTrack = xTrack;
		m_uCurveDragKeyId = uKeyId;
		m_uCurveDragComponent = uComponent;
		m_bCurveDragIn = bIn;
		m_fCurveDragPixelX = xIO.MousePos.x;
		m_fCurveDragPixelY = xIO.MousePos.y;
		return true;
	}

	if (FindCurvePointAtScreenPos(xIO.MousePos.x, xIO.MousePos.y, xTrack, uKeyId))
	{
		Action_SelectKey(xTrack, uKeyId, SelectModeFromModifiers());
		return true;
	}

	// Not ours: the rubber band, the ruler scrub and the duration handle are all
	// HandleSheetInput's, and they behave identically in both views because the X
	// axis is the same one. Box select is curve-aware inside Action_BoxSelect.
	return false;
}

//=============================================================================
// Toolbar items — on the rows that already exist, so the curve view costs the
// sheet no height at all.
//=============================================================================

void Zenith_EditorPanel_Animation::RenderCurveToolbarItems()
{
	// ★ CLEARED HERE — the one place it is cleared, and RenderCurveTangentModeCombo
	// below is the one place it is raised. A frame in which this function ran and
	// emitted no combo must report "not drawn" rather than what the last frame with
	// a selected key drew; the hidden and collapsed cases — where this function is
	// never reached at all — are filtered by the canvas flag inside
	// WasCurveModeControlDrawnLastFrame.
	m_bCurveModeControlDrawn = false;

	ImGui::SameLine();
	bool bShowCurves = m_bShowCurveView;
	if (ImGui::Checkbox("Curves", &bShowCurves))
	{
		Action_SetCurveView(bShowCurves);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Replace the dope-sheet rows with the CURVE view: the selected tracks' x/y/z "
			"components, sampled through the real channel sampler, with a tangent handle per key. "
			"The timeline, the ruler and the playhead are unchanged.");
	}

	if (!m_bShowCurveView || !m_xDocument.IsOpen())
	{
		// ★ NOTHING ELSE IS EMITTED WHILE THE VIEW IS OFF. These sit on an existing
		// toolbar row so they cost no height either way, but a disabled Auto /
		// Linear pair on a dope sheet would be four controls explaining nothing.
		return;
	}

	ImGui::SameLine();
	if (ImGui::Button("Auto"))
	{
		Action_SetSelectionTangentsAuto();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Catmull-Rom tangents on BOTH ends of every selected key, from its own "
			"neighbours. Stored AS Auto and MAINTAINED: the document recomputes an Auto end whenever "
			"the track changes shape. For one end only, use the In / Out mode boxes.");
	}

	ImGui::SameLine();
	if (ImGui::Button("Linear"))
	{
		Action_SetSelectionTangentsLinear();
	}
	if (ImGui::IsItemHovered())
	{
		// ★ THE WORDING STILL MATTERS, FOR THE OTHER HALF OF THE OLD REASON. A flat
		// handle IS storable (Flux_TangentMode::FLAT, schema 3), and it is the SAME
		// six zero floats — so the two are told apart by the MODE alone and this
		// control may never be labelled "Flat".
		ImGui::SetTooltip("Clear the selected keys' tangents. An unset tangent is the LINEAR one — "
			"the segment's own slope — not a flat handle, which is the same zeroes under a different "
			"MODE and is set from the In / Out mode boxes.");
	}

	ImGui::SameLine();
	bool bUnified = m_bCurveTangentsUnified;
	if (ImGui::Checkbox("Unified", &bUnified))
	{
		Action_SetTangentsUnified(bUnified);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Dragging one handle writes the other to the same value, keeping the key smooth. "
			"Off, the two halves are edited independently — a corner.");
	}

	ImGui::SameLine();
	if (ImGui::Button("Fit Curves"))
	{
		Action_FitCurveViewToSelection();
	}

	// ★ THE MODE CONTROL, WHICH IS ALSO THE MODE READOUT. It REPLACED the
	// TextDisabled("tangents: %s") that used to sit here rather than being added
	// beside it: this row already runs wider than a 900 px window, a second toolbar
	// LINE would come straight out of the sheet's canvas (whose last row is the
	// events row), and a widget that shows a mode and a widget that sets one are the
	// same widget. Two combos, one per END, because since B2 the two ends are
	// separately stored and a single box could not show an IN=Linear / OUT=Flat key.
	Zenith_AnimTrackId xPrimaryTrack;
	u_int uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
	if (ResolvePrimarySelectedKey(xPrimaryTrack, uPrimaryKeyId))
	{
		RenderCurveTangentModeCombo(xPrimaryTrack, uPrimaryKeyId, ZENITH_ANIM_TANGENT_END_IN);
		RenderCurveTangentModeCombo(xPrimaryTrack, uPrimaryKeyId, ZENITH_ANIM_TANGENT_END_OUT);
	}
}

void Zenith_EditorPanel_Animation::RenderCurveTangentModeCombo(const Zenith_AnimTrackId& xTrack,
	u_int uKeyId, Zenith_AnimTangentEnd eEnd)
{
	const bool bIn = (eEnd == ZENITH_ANIM_TANGENT_END_IN);

	Flux_TangentMode eCurrent = Flux_TangentMode::LINEAR;
	if (!GetKeyTangentMode(xTrack, uKeyId, eEnd, eCurrent))
	{
		// A root-motion key has no mode to show and no mode to set (D17). Nothing is
		// emitted, and WasCurveModeControlDrawnLastFrame says so — a DISABLED combo
		// would be a control explaining nothing on a row with no width to spare.
		return;
	}

	char acPreview[32];
	snprintf(acPreview, sizeof(acPreview), "%s: %s", bIn ? "In" : "Out",
		Zenith_AnimCurveTangentModeLabel(eCurrent));

	ImGui::SameLine();
	// Wide enough for "Out: Custom" and no wider: every pixel here is a pixel the
	// row does not have.
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(96.0f));
	const bool bComboOpen = ImGui::BeginCombo(bIn ? "##AnimCurveInMode" : "##AnimCurveOutMode", acPreview,
		ImGuiComboFlags_HeightSmall);
	// ★ HOVER IS READ HERE, BEFORE THE POPUP'S OWN ITEMS. Past EndCombo the "last
	// item" is whatever the popup submitted, so a tooltip hung off IsItemHovered
	// down there would follow the list rather than the box.
	const bool bComboHovered = ImGui::IsItemHovered();
	if (bComboOpen)
	{
		// ★ THE FOUR WIRE MODES, IN WIRE ORDER, LABELLED BY THE ONE LABEL FUNCTION.
		// A hand-typed word here is how the toolbar and the units start disagreeing.
		constexpr Flux_TangentMode aeMODES[] =
		{
			Flux_TangentMode::LINEAR,
			Flux_TangentMode::FLAT,
			Flux_TangentMode::AUTO,
			Flux_TangentMode::CUSTOM,
		};
		for (u_int u = 0; u < sizeof(aeMODES) / sizeof(aeMODES[0]); ++u)
		{
			const Flux_TangentMode eMode = aeMODES[u];
			const bool bSelected = (eMode == eCurrent);
			if (ImGui::Selectable(Zenith_AnimCurveTangentModeLabel(eMode), bSelected))
			{
				// ★ THE SELECTION, NOT THE PRIMARY KEY ALONE. The combo READS the
				// primary key — a box has to show one value — but the gesture a user
				// performs after box-selecting six keys is "make these flat", and
				// applying it to one of them would be a control that silently ignores
				// five. One compound either way, so a single-key selection behaves
				// exactly as the per-key verb does.
				Action_SetSelectionTangentMode(eEnd, eMode);
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	if (bComboHovered)
	{
		ImGui::SetTooltip("The stored tangent mode of the %s end of every selected key, and how to set "
			"it. Linear = the segment's own slope; Flat = a zero derivative at the key (the same zero "
			"vector, told apart by this mode); Auto = Catmull-Rom, RECOMPUTED whenever the track changes "
			"shape; Custom = the hand-dragged handle. The other end is not touched.",
			bIn ? "IN" : "OUT");
	}

	m_bCurveModeControlDrawn = true;
}

//=============================================================================
// Actions
//=============================================================================

bool Zenith_EditorPanel_Animation::Action_SetCurveView(bool bShow)
{
	if (m_bShowCurveView == bShow)
	{
		// ASSIGNMENT: the value asked for is in place. See the header block on why
		// this family reports satisfaction rather than change.
		return true;
	}
	m_bShowCurveView = bShow;
	// A gesture cannot survive the view it was made in, for the reason Render
	// drops every drag when the panel is hidden: the mouse-up that would have
	// ended it is delivered to whatever is on screen now.
	m_bCurveHandleDragActive = false;
	m_bCurveDragMoved = false;
	m_uCurveDragKeyId = uINVALID_ANIM_KEY_ID;
	if (bShow)
	{
		// Fit on the NEXT render, when the area's height is known.
		m_bPendingCurveFit = true;
	}
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SetTangentsUnified(bool bUnified)
{
	m_bCurveTangentsUnified = bUnified;
	return true;
}

bool Zenith_EditorPanel_Animation::Action_SetKeyTangents(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	const Zenith_Maths::Vector3& xInTangent, const Zenith_Maths::Vector3& xOutTangent)
{
	// ★ CUSTOM ON BOTH ENDS (B2). The document stores the modes it is given now, so
	// an action that handed over a default-constructed struct would write LINEAR
	// beside two authored numbers — and a LINEAR end IGNORES its vector, so the edit
	// would reach the file, reach the UI, and never reach the pose.
	Flux_KeyTangents xTangents;
	xTangents.m_xInTangent = xInTangent;
	xTangents.m_xOutTangent = xOutTangent;
	xTangents.m_eInMode = Flux_TangentMode::CUSTOM;
	xTangents.m_eOutMode = Flux_TangentMode::CUSTOM;
	const bool bOk = m_xDocument.SetKeyTangents(xTrack, uKeyId, xTangents);
	if (bOk)
	{
		NotifyDocumentEdited();
	}
	return bOk;
}

bool Zenith_EditorPanel_Animation::Action_SetSelectionTangentsAuto()
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0u)
	{
		return false;
	}
	if (!m_xDocument.BeginCompound())
	{
		return false;
	}
	bool bAnyResolved = false;
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		// ★ A ROOT-MOTION KEY IN THE SELECTION IS SKIPPED, NOT A REFUSAL. Root
		// motion has no tangent array (D17) and a box select across those two rows
		// is ordinary; failing the whole gesture over one of them would make the
		// panel feel broken for no benefit.
		if (xEntry.m_xTrack.m_bRootMotion)
		{
			continue;
		}
		bAnyResolved = m_xDocument.SetKeyTangentsAuto(xEntry.m_xTrack, xEntry.m_uKeyId) || bAnyResolved;
	}
	// An EMPTY group pushes nothing, so a selection that was already auto leaves
	// one undo step rather than a step that reverses nothing.
	m_xDocument.EndCompound("Auto Tangents", /*bKeep*/ true);
	if (bAnyResolved)
	{
		NotifyDocumentEdited();
	}
	return bAnyResolved;
}

bool Zenith_EditorPanel_Animation::Action_SetSelectionTangentsLinear()
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0u)
	{
		return false;
	}
	if (!m_xDocument.BeginCompound())
	{
		return false;
	}
	// ZERO VECTORS AND MODE LINEAR, stated rather than defaulted (B2): this action's
	// name is the contract, and the default-constructed pair happening to be LINEAR
	// today is not something a caller should be reading off a struct definition.
	// Flat is a DIFFERENT selection verb with the same six floats.
	Flux_KeyTangents xZero;
	xZero.m_eInMode = Flux_TangentMode::LINEAR;
	xZero.m_eOutMode = Flux_TangentMode::LINEAR;
	bool bAnyResolved = false;
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		if (xEntry.m_xTrack.m_bRootMotion)
		{
			continue;
		}
		bAnyResolved = m_xDocument.SetKeyTangents(xEntry.m_xTrack, xEntry.m_uKeyId, xZero) || bAnyResolved;
	}
	m_xDocument.EndCompound("Linear Tangents", /*bKeep*/ true);
	if (bAnyResolved)
	{
		NotifyDocumentEdited();
	}
	return bAnyResolved;
}

//------------------------------------------------------------------------------
// The MODE verbs (B3).
//
// ★ THROUGH Zenith_AnimationDocument::SetKeyTangentMode AND NOTHING ELSE. The two
// neighbouring verbs both write BOTH ends — Action_SetKeyTangents claims CUSTOM on
// each (it is a vector edit) and SetKeyTangentsAuto forces AUTO on each — so either
// one, used here, would rewrite the end the user did not name. That is invisible in
// the picture: the far handle keeps its number and quietly loses its provenance.
//------------------------------------------------------------------------------

bool Zenith_EditorPanel_Animation::Action_SetKeyTangentMode(const Zenith_AnimTrackId& xTrack, u_int uKeyId,
	Zenith_AnimTangentEnd eEnd, Flux_TangentMode eMode)
{
	const bool bOk = m_xDocument.SetKeyTangentMode(xTrack, uKeyId, eEnd, eMode);
	if (bOk)
	{
		NotifyDocumentEdited();
	}
	// The document's ASSIGNMENT contract, unchanged: true means the mode asked for
	// is on that end, and a re-statement pushes nothing — so the invariant a test
	// asserts on is the undo-stack depth, not this bool.
	return bOk;
}

bool Zenith_EditorPanel_Animation::Action_SetSelectionTangentMode(Zenith_AnimTangentEnd eEnd,
	Flux_TangentMode eMode)
{
	if (!m_xDocument.IsOpen() || m_axSelectedKeys.GetSize() == 0u)
	{
		return false;
	}
	if (!m_xDocument.BeginCompound())
	{
		return false;
	}
	bool bAnyResolved = false;
	for (u_int u = 0; u < m_axSelectedKeys.GetSize(); ++u)
	{
		const Zenith_AnimSelectedKey& xEntry = m_axSelectedKeys.Get(u);
		// A root-motion key in a mixed selection is SKIPPED, exactly as the two
		// preset verbs above skip it: box-selecting across those rows is ordinary,
		// and failing the whole gesture over one of them reads as a broken panel.
		if (xEntry.m_xTrack.m_bRootMotion)
		{
			continue;
		}
		bAnyResolved = m_xDocument.SetKeyTangentMode(xEntry.m_xTrack, xEntry.m_uKeyId, eEnd, eMode)
			|| bAnyResolved;
	}
	// An EMPTY group pushes nothing, so a selection already in this mode leaves the
	// undo stack exactly where it was rather than adding a step that reverses
	// nothing.
	m_xDocument.EndCompound("Tangent Mode", /*bKeep*/ true);
	if (bAnyResolved)
	{
		NotifyDocumentEdited();
	}
	return bAnyResolved;
}

bool Zenith_EditorPanel_Animation::Action_DragTangentHandleToPixel(const Zenith_AnimTrackId& xTrack,
	u_int uKeyId, u_int uComponent, bool bIn, float fX, float fY)
{
	if (!m_xDocument.IsOpen() || uComponent >= uANIM_CURVE_COMPONENT_COUNT || xTrack.m_bRootMotion)
	{
		return false;
	}
	float fTime = 0.0f;
	float fKeyValue = 0.0f;
	if (!m_xDocument.GetKeyTime(xTrack, uKeyId, fTime)
	 || !GetCurveKeyValue(xTrack, uKeyId, uComponent, fKeyValue))
	{
		return false;
	}
	Flux_KeyTangents xTangents;
	if (!m_xDocument.GetKeyTangents(xTrack, uKeyId, xTangents))
	{
		return false;
	}

	// ★ THROUGH THE PURE MAPPING, which is the same function the DRAW used to put
	// the handle where the cursor grabbed it. That is what makes "drag it back to
	// where it was drawn" a no-op rather than a slow drift.
	const float fTangent = Zenith_AnimCurveTangentFromPixel(m_xView, m_xCurveValueView,
		fTime, fKeyValue, bIn, fX, fY);

	// ★ THE END THE CURSOR MOVED BECOMES CUSTOM (B2), and only that end — unless
	// the unified toggle is on, in which case the gesture IS both ends. Without
	// this a drag on a LINEAR or FLAT handle would store a number the sampler
	// ignores; with it applied to BOTH ends unconditionally, dragging one handle
	// would silently take the opposite one off AUTO.
	const int iComponent = static_cast<int>(uComponent);
	if (bIn || m_bCurveTangentsUnified)
	{
		xTangents.m_xInTangent[iComponent] = fTangent;
		xTangents.m_eInMode = Flux_TangentMode::CUSTOM;
	}
	if (!bIn || m_bCurveTangentsUnified)
	{
		xTangents.m_xOutTangent[iComponent] = fTangent;
		xTangents.m_eOutMode = Flux_TangentMode::CUSTOM;
	}

	const bool bOk = m_xDocument.SetKeyTangents(xTrack, uKeyId, xTangents);
	if (bOk)
	{
		NotifyDocumentEdited();
	}
	return bOk;
}

bool Zenith_EditorPanel_Animation::Action_FitCurveViewToSelection()
{
	if (!m_bShowCurveView || !m_xDocument.IsOpen())
	{
		return false;
	}
	if (m_xCurveValueView.m_fHeightPixels <= 0.0f)
	{
		// No rendered frame yet: defer to the first one that has a height, exactly
		// as m_bPendingFrameAll defers the horizontal fit.
		m_bPendingCurveFit = true;
		return false;
	}
	float fMin = 0.0f;
	float fMax = 0.0f;
	if (!ComputeCurveValueRange(fMin, fMax))
	{
		return false;
	}
	Zenith_AnimCurveFitRange(m_xCurveValueView, fMin, fMax);
	return true;
}

#endif // ZENITH_TOOLS
