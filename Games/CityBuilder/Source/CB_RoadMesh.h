#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include <cmath>

// ============================================================================
// CB_RoadMesh — turns a road graph segment (a CB_Spline + width) into a
// terrain-following ribbon of triangles. G1 first pass: emit flat-shaded
// triangles to Flux_Primitives each frame (fast iteration; textured
// Flux_MeshGeometry + the terrain carve come in G2). Pure geometry (no engine
// /Flux dependency) so the ribbon layout is headless-testable; the caller
// submits the triangles to the renderer.
// ============================================================================

namespace CB_RoadMesh
{
	// Target spacing between ribbon cross-sections (world units). Smaller = smoother curves.
	static constexpr float SAMPLE_SPACING = 2.0f;

	inline uint32_t SampleCount(const CB_RoadSegment& xSeg)
	{
		const float fLen = xSeg.m_xSpline.Length();
		uint32_t uSamples = static_cast<uint32_t>(std::ceil(fLen / SAMPLE_SPACING));
		return (uSamples < 1) ? 1 : uSamples;
	}

	// Number of triangle-vertices BuildRibbon will append for this segment
	// (6 per quad × SampleCount quads). For tests.
	inline uint32_t RibbonVertexCount(const CB_RoadSegment& xSeg)
	{
		return SampleCount(xSeg) * 6;
	}

	// Appends the segment's road ribbon as triangles (3 Vector3 per triangle,
	// world space, wound so the surface faces +Y) to xOutTris. Y follows the
	// heightfield + fYOffset; the ribbon is xSeg.m_fWidth wide, centred on the
	// spline.
	inline void BuildRibbon(const CB_RoadSegment& xSeg, const CB_TerrainHeightfield& xField,
	                        float fYOffset, Zenith_Vector<Zenith_Maths::Vector3>& xOutTris)
	{
		const uint32_t uSamples = SampleCount(xSeg);
		const float fHalf = xSeg.m_fWidth * 0.5f;

		Zenith_Maths::Vector3 xPrevL(0.0f);
		Zenith_Maths::Vector3 xPrevR(0.0f);
		bool bHavePrev = false;

		for (uint32_t i = 0; i <= uSamples; ++i)
		{
			const float fT = static_cast<float>(i) / static_cast<float>(uSamples);
			const Zenith_Maths::Vector2 xC   = xSeg.m_xSpline.Evaluate(fT);
			const Zenith_Maths::Vector2 xTan = xSeg.m_xSpline.UnitTangent(fT);
			const Zenith_Maths::Vector2 xPerp(-xTan.y, xTan.x);   // left normal in XZ

			const Zenith_Maths::Vector2 xL = xC + xPerp * fHalf;
			const Zenith_Maths::Vector2 xR = xC - xPerp * fHalf;
			const float fLY = xField.GetHeightAt(xL.x, xL.y) + fYOffset;
			const float fRY = xField.GetHeightAt(xR.x, xR.y) + fYOffset;
			const Zenith_Maths::Vector3 xWL(xL.x, fLY, xL.y);
			const Zenith_Maths::Vector3 xWR(xR.x, fRY, xR.y);

			if (bHavePrev)
			{
				// Two +Y-facing triangles for the quad (prevL,prevR,WR,WL).
				xOutTris.PushBack(xPrevL); xOutTris.PushBack(xWR); xOutTris.PushBack(xPrevR);
				xOutTris.PushBack(xPrevL); xOutTris.PushBack(xWL); xOutTris.PushBack(xWR);
			}
			xPrevL = xWL;
			xPrevR = xWR;
			bHavePrev = true;
		}
	}
}
