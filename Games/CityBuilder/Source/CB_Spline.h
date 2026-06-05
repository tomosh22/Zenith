#pragma once

#include "Maths/Zenith_Maths.h"
#include <cmath>
#include <cstdint>

// ============================================================================
// CB_Spline — a cubic Bézier curve in the world XZ plane (Cities: Skylines-style
// road centrelines). Pure math, no engine/Flux dependency, headless-testable.
//
// Convention: control points are Zenith_Maths::Vector2 where .x = world X and
// .y = world Z. The road's Y is sampled from the terrain separately when the
// ribbon mesh is built. A "segment" of the road graph owns one CB_Spline.
// ============================================================================

class CB_Spline
{
public:
	// P0 = start, P1/P2 = handles, P3 = end (all world XZ).
	Zenith_Maths::Vector2 m_axControl[4];

	CB_Spline()
	{
		m_axControl[0] = Zenith_Maths::Vector2(0.0f, 0.0f);
		m_axControl[1] = Zenith_Maths::Vector2(0.0f, 0.0f);
		m_axControl[2] = Zenith_Maths::Vector2(0.0f, 0.0f);
		m_axControl[3] = Zenith_Maths::Vector2(0.0f, 0.0f);
	}

	CB_Spline(const Zenith_Maths::Vector2& xP0, const Zenith_Maths::Vector2& xP1,
	          const Zenith_Maths::Vector2& xP2, const Zenith_Maths::Vector2& xP3)
	{
		m_axControl[0] = xP0;
		m_axControl[1] = xP1;
		m_axControl[2] = xP2;
		m_axControl[3] = xP3;
	}

	// A straight segment from A to B with handles at 1/3 and 2/3 (so a degenerate
	// "straight bezier" evaluates as a line).
	static CB_Spline Straight(const Zenith_Maths::Vector2& xA, const Zenith_Maths::Vector2& xB)
	{
		const Zenith_Maths::Vector2 xDelta = xB - xA;
		return CB_Spline(xA, xA + xDelta * (1.0f / 3.0f), xA + xDelta * (2.0f / 3.0f), xB);
	}

	// A curved segment from A to B whose endpoint tangents are xDirA / xDirB
	// (unit-ish direction the road leaves A / arrives at B). Handle length is a
	// fraction of the A→B distance so the curve is well-shaped.
	static CB_Spline Curved(const Zenith_Maths::Vector2& xA, const Zenith_Maths::Vector2& xDirA,
	                        const Zenith_Maths::Vector2& xB, const Zenith_Maths::Vector2& xDirB)
	{
		const float fHandle = Distance(xA, xB) * (1.0f / 3.0f);
		return CB_Spline(xA, xA + xDirA * fHandle, xB + xDirB * fHandle, xB);
	}

	// Position on the curve, fT in [0,1].
	Zenith_Maths::Vector2 Evaluate(float fT) const
	{
		const float u = 1.0f - fT;
		const float w0 = u * u * u;
		const float w1 = 3.0f * u * u * fT;
		const float w2 = 3.0f * u * fT * fT;
		const float w3 = fT * fT * fT;
		return m_axControl[0] * w0 + m_axControl[1] * w1 + m_axControl[2] * w2 + m_axControl[3] * w3;
	}

	// First derivative (un-normalised tangent direction), fT in [0,1].
	Zenith_Maths::Vector2 Tangent(float fT) const
	{
		const float u = 1.0f - fT;
		return (m_axControl[1] - m_axControl[0]) * (3.0f * u * u)
		     + (m_axControl[2] - m_axControl[1]) * (6.0f * u * fT)
		     + (m_axControl[3] - m_axControl[2]) * (3.0f * fT * fT);
	}

	// Unit tangent; falls back to the chord direction at a degenerate point.
	Zenith_Maths::Vector2 UnitTangent(float fT) const
	{
		Zenith_Maths::Vector2 xT = Tangent(fT);
		float fLen = std::sqrt(xT.x * xT.x + xT.y * xT.y);
		if (fLen < 1e-5f)
		{
			xT = m_axControl[3] - m_axControl[0];
			fLen = std::sqrt(xT.x * xT.x + xT.y * xT.y);
		}
		if (fLen < 1e-5f)
		{
			return Zenith_Maths::Vector2(1.0f, 0.0f);
		}
		return xT * (1.0f / fLen);
	}

	// Approximate arc length by sampling the curve into a polyline.
	float Length(uint32_t uSamples = 24) const
	{
		if (uSamples < 1) uSamples = 1;
		float fLen = 0.0f;
		Zenith_Maths::Vector2 xPrev = Evaluate(0.0f);
		for (uint32_t i = 1; i <= uSamples; ++i)
		{
			const Zenith_Maths::Vector2 xCur = Evaluate(static_cast<float>(i) / static_cast<float>(uSamples));
			fLen += Distance(xPrev, xCur);
			xPrev = xCur;
		}
		return fLen;
	}

	// Minimum distance (XZ) from world point xP to the curve, sampled as a
	// polyline with per-segment point projection. Used by road-terrain carving
	// and road-frontage zoning.
	float DistanceToPoint(const Zenith_Maths::Vector2& xP, uint32_t uSamples = 24) const
	{
		if (uSamples < 1) uSamples = 1;
		float fBest = 1e30f;
		Zenith_Maths::Vector2 xPrev = Evaluate(0.0f);
		for (uint32_t i = 1; i <= uSamples; ++i)
		{
			const Zenith_Maths::Vector2 xCur = Evaluate(static_cast<float>(i) / static_cast<float>(uSamples));
			const float fD = PointSegmentDistance(xP, xPrev, xCur);
			if (fD < fBest) fBest = fD;
			xPrev = xCur;
		}
		return fBest;
	}

	// --- small helpers (static, reusable) ---
	static float Distance(const Zenith_Maths::Vector2& xA, const Zenith_Maths::Vector2& xB)
	{
		const float dx = xB.x - xA.x;
		const float dy = xB.y - xA.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	// Distance from point xP to segment [xA,xB] in the plane.
	static float PointSegmentDistance(const Zenith_Maths::Vector2& xP,
	                                  const Zenith_Maths::Vector2& xA,
	                                  const Zenith_Maths::Vector2& xB)
	{
		const float abx = xB.x - xA.x;
		const float aby = xB.y - xA.y;
		const float apx = xP.x - xA.x;
		const float apy = xP.y - xA.y;
		const float fLenSq = abx * abx + aby * aby;
		float fT = (fLenSq > 1e-10f) ? ((apx * abx + apy * aby) / fLenSq) : 0.0f;
		fT = (fT < 0.0f) ? 0.0f : (fT > 1.0f ? 1.0f : fT);
		const float cx = xA.x + abx * fT;
		const float cy = xA.y + aby * fT;
		const float dx = xP.x - cx;
		const float dy = xP.y - cy;
		return std::sqrt(dx * dx + dy * dy);
	}
};
