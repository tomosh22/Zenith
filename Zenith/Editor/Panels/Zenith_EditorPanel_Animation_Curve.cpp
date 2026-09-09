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

const char* Zenith_AnimCurveTangentModeLabel(Zenith_AnimCurveTangentMode eMode)
{
	// ★ "Linear", NEVER "Flat" — for a different reason than before B2. A flat
	// handle IS representable now (Flux_TangentMode::FLAT, schema 3), but this
	// two-valued display cannot tell one from a hand-authored pair, so a "Flat"
	// label here would name the wrong one of the two states it collapses.
	return eMode == ZENITH_ANIMCURVE_TANGENT_LINEAR ? "Linear" : "Custom";
}

Zenith_AnimCurveTangentMode Zenith_AnimCurveTangentModeOf(const Flux_KeyTangents& xTangents)
{
	// ★ A PROJECTION OF THE STORED Flux_TangentMode ONTO THE TWO THIS ENUM HAS
	// (B1), not a re-derivation from the vectors. The clip now says what each end
	// IS, and the display must not answer that question a second way — a projection
	// that read the numbers would disagree with the stored mode the moment FLAT or
	// AUTO became authorable, and it would disagree silently.
	//
	// LINEAR only when BOTH ends are; anything else is Custom. FLAT and AUTO ARE
	// authorable since schema 3 (B2), and they land in the Custom bucket — an
	// UNDER-STATEMENT rather than a lie, which is the failure this ordering picks
	// on purpose. Widening the display to four values is its own unit; until then a
	// flat key reads "Custom", which is wrong about the word and right about "this
	// is not the untouched linear default".
	return (xTangents.m_eInMode == Flux_TangentMode::LINEAR && xTangents.m_eOutMode == Flux_TangentMode::LINEAR)
		? ZENITH_ANIMCURVE_TANGENT_LINEAR
		: ZENITH_ANIMCURVE_TANGENT_CUSTOM;
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
	Zenith_AnimCurveTangentMode& eOut) const
{
	Flux_KeyTangents xTangents;
	if (!m_xDocument.GetKeyTangents(xTrack, uKeyId, xTangents))
	{
		return false;
	}
	eOut = Zenith_AnimCurveTangentModeOf(xTangents);
	return true;
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
		ImGui::SetTooltip("Catmull-Rom tangents on every selected key, from its own neighbours. "
			"An OPERATION, not a stored mode: nothing re-applies it when a neighbour moves.");
	}

	ImGui::SameLine();
	if (ImGui::Button("Linear"))
	{
		Action_SetSelectionTangentsLinear();
	}
	if (ImGui::IsItemHovered())
	{
		// ★ THE WORDING MATTERS. Zero IS the linear tangent; a flat/eased handle is
		// not representable, so this control may never be labelled "Flat".
		ImGui::SetTooltip("Clear the selected keys' tangents. An unset tangent is the LINEAR one — "
			"the segment's own slope — not a flat handle, which this format cannot store.");
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

	// The displayed MODE of the primary selected key, which is the one thing about
	// a tangent a user cannot read off the picture.
	Zenith_AnimTrackId xPrimaryTrack;
	u_int uPrimaryKeyId = uINVALID_ANIM_KEY_ID;
	Zenith_AnimCurveTangentMode eMode = ZENITH_ANIMCURVE_TANGENT_LINEAR;
	if (ResolvePrimarySelectedKey(xPrimaryTrack, uPrimaryKeyId)
	 && GetKeyTangentMode(xPrimaryTrack, uPrimaryKeyId, eMode))
	{
		ImGui::SameLine();
		ImGui::TextDisabled("tangents: %s", Zenith_AnimCurveTangentModeLabel(eMode));
	}
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
