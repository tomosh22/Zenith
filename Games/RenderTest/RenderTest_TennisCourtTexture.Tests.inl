//=============================================================================
// Tennis court SURFACE tests. Included from the bottom of
// RenderTest_Tennis.cpp, so the anonymous-namespace court generators and THE
// layout are in scope.
//
// Deliberately outside the ZENITH_TOOLS export block: the generators are pure
// CPU pixel functions, so these run in every config, not only in a tools build
// where the .ztxtr actually gets written.
//
// Every clause here covers a defect that is invisible in a still frame and
// obvious in motion, or that reads as a lighting bug rather than a texture one:
//   * a hard-edged line crawls under camera motion, and no downstream stage
//     can put the missing coverage back,
//   * mow stripes and wear are the two cues that separate mown turf from green
//     paint; either can vanish while the texture still "looks like a court",
//   * an RM map whose paint is not smoother than its grass is a texture fetch
//     with no effect.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	// One build of the 1024x2048 colour map, shared by every test that reads it.
	struct RT_CourtColourOnce
	{
		std::vector<uint8_t> m_xPx;
		uint32_t m_uW = 0;
		uint32_t m_uH = 0;

		RT_CourtColourOnce() { m_xPx = RT_MakeCourtTexture(m_uW, m_uH); }
	};

	const RT_CourtColourOnce& RT_CourtColour()
	{
		static const RT_CourtColourOnce ls_x;
		return ls_x;
	}

	float RT_CourtTexel(const std::vector<uint8_t>& xPx, uint32_t uW, uint32_t x, uint32_t y, uint32_t uC)
	{
		return xPx[(static_cast<size_t>(y) * uW + x) * 4 + uC] / 255.0f;
	}
}

ZENITH_TEST(TennisCourt, CourtTextureIsTheFullResolutionSurface)
{
	const RT_CourtColourOnce& x = RT_CourtColour();
	ZENITH_ASSERT_EQ(x.m_uW, 1024u, "court colour width");
	ZENITH_ASSERT_EQ(x.m_uH, 2048u, "court colour height");
	ZENITH_ASSERT_EQ(static_cast<u_int>(x.m_xPx.size()),
		static_cast<u_int>(x.m_uW * x.m_uH * 4u), "court colour buffer size");
}

ZENITH_TEST(TennisCourt, PaintedLinesAreAntialiased)
{
	// THE clause this file exists for. A nearest-texel rect fill produces
	// exactly two values across a line edge -- turf and paint -- and the step
	// between them crawls under any camera motion. Coverage weighting puts
	// intermediate values in between, so the test is that they EXIST, in
	// quantity, and on both sides of a line.
	const RT_CourtColourOnce& x = RT_CourtColour();

	// Scanned over the WHOLE map rather than one row: a vertical line has the
	// same coverage in every row, so a single row samples only a handful of
	// edges and whether any of them happens to land mid-ramp is luck. The
	// horizontal lines contribute full-width partial rows, which makes the
	// count large and the clause robust.
	u_int uIntermediate = 0u;
	u_int uPaint = 0u;
	u_int uTurf = 0u;
	for (uint32_t iy = 0; iy < x.m_uH; iy++)
	{
		for (uint32_t ix = 0; ix < x.m_uW; ix++)
		{
			const float fG = RT_CourtTexel(x.m_xPx, x.m_uW, ix, iy, 1);
			if (fG > 0.80f) { uPaint++; }
			else if (fG < 0.55f) { uTurf++; }
			else { uIntermediate++; }
		}
	}
	ZENITH_ASSERT_GT(uPaint, 0u, "no painted texels anywhere -- the lines are missing");
	ZENITH_ASSERT_GT(uTurf, 0u, "no turf texels anywhere");
	ZENITH_ASSERT_GT(uIntermediate, 200u,
		"every texel is either turf or paint -- the line edges are hard steps, which is "
		"exactly the aliasing the coverage fill exists to remove");

	// Scan the centre row too, purely so a failure localises: the sidelines and
	// the centre service line all cross it.
	const uint32_t uRow = x.m_uH / 2u;
	u_int uRowPaint = 0u;
	for (uint32_t ix = 0; ix < x.m_uW; ix++)
	{
		if (RT_CourtTexel(x.m_xPx, x.m_uW, ix, uRow, 1) > 0.80f) { uRowPaint++; }
	}
	ZENITH_ASSERT_GT(uRowPaint, 0u, "no painted texels on the centre row");

	// And the coverage function itself: a texel straddling a line edge must
	// report a partial value. Measured through the production layout, so a
	// change to the line width cannot silently invalidate the test.
	const RT_CourtLayout xL = RT_MakeCourtLayout();
	std::vector<RT_CourtRect> xRects;
	RT_BuildCourtLineRects(xL, xRects);
	bool bSawPartial = false;
	for (uint32_t ix = 0; ix < x.m_uW && !bSawPartial; ix++)
	{
		const float fCov = RT_CourtPaintCoverage(xRects, x.m_uW, x.m_uH, ix, uRow);
		bSawPartial = fCov > 0.02f && fCov < 0.98f;
	}
	ZENITH_ASSERT_TRUE(bSawPartial, "no texel reports partial paint coverage -- the fill is binary");
}

ZENITH_TEST(TennisCourt, LinesLandWhereTheCourtGeometrySaysTheyDo)
{
	// The lines are what a viewer measures the court against, so their POSITION
	// is a contract with RenderTest_Tennis.h -- not something to eyeball.
	const RT_CourtColourOnce& x = RT_CourtColour();
	const RT_CourtLayout xL = RT_MakeCourtLayout();

	const uint32_t uRow = x.m_uH / 2u;
	const uint32_t uLeftDoubles = static_cast<uint32_t>(xL.m_fLeftDoubles * x.m_uW);
	const uint32_t uRightDoubles = static_cast<uint32_t>(xL.m_fRightDoubles * x.m_uW);
	const uint32_t uCentre = x.m_uW / 2u;
	ZENITH_ASSERT_GT(RT_CourtTexel(x.m_xPx, x.m_uW, uLeftDoubles, uRow, 1), 0.80f,
		"no paint at the left doubles sideline");
	ZENITH_ASSERT_GT(RT_CourtTexel(x.m_xPx, x.m_uW, uRightDoubles, uRow, 1), 0.80f,
		"no paint at the right doubles sideline");
	ZENITH_ASSERT_GT(RT_CourtTexel(x.m_xPx, x.m_uW, uCentre, uRow, 1), 0.80f,
		"no paint on the centre service line");

	// The apron is grass all the way round: paint flush to the slab edge is the
	// classic off-by-an-apron error, and it looks plausible in a top-down view.
	for (uint32_t iy = 0; iy < x.m_uH; iy += 64u)
	{
		ZENITH_ASSERT_LT(RT_CourtTexel(x.m_xPx, x.m_uW, 1u, iy, 1), 0.80f,
			"the slab edge is painted -- the grass apron has been lost");
	}
}

ZENITH_TEST(TennisCourt, MowStripesAlternateAcrossTheCourt)
{
	// Bands running ALONG the court, alternating across its width. The single
	// strongest cue that a green surface is mown grass.
	float fMin = 2.0f;
	float fMax = -1.0f;
	u_int uSignChanges = 0u;
	float fPrev = RT_CourtMowStripe(0.0f);
	bool bPrevAbove = fPrev > 1.0f;
	for (int i = 0; i <= 400; i++)
	{
		const float fU = static_cast<float>(i) / 400.0f;
		const float fS = RT_CourtMowStripe(fU);
		fMin = std::min(fMin, fS);
		fMax = std::max(fMax, fS);
		const bool bAbove = fS > 1.0f;
		if (bAbove != bPrevAbove) { uSignChanges++; }
		bPrevAbove = bAbove;
	}
	// +/- 6%: tight enough to read as a mow pattern, not as stripes painted on.
	ZENITH_ASSERT_EQ_FLOAT(fMax, 1.06f, 0.005f, "mow stripe peak is not +6%");
	ZENITH_ASSERT_EQ_FLOAT(fMin, 0.94f, 0.005f, "mow stripe trough is not -6%");
	// ~1.5 m bands across a ~15.97 m slab is about 10 bands, so ~20 crossings.
	ZENITH_ASSERT_GT(uSignChanges, 12u, "too few mow bands -- they will read as one wide gradient");
	ZENITH_ASSERT_LT(uSignChanges, 32u, "too many mow bands -- they will alias into moire");
}

ZENITH_TEST(TennisCourt, BaselinesAreWornAndTheApronIsNot)
{
	// Wear is where a player actually stands. Asserted through the wear field
	// rather than through pixels, so the clause survives a colour re-tune.
	const RT_CourtLayout xL = RT_MakeCourtLayout();

	// Average the wear across the middle of each baseline band and compare it
	// against the same width mid-court, where nobody stands.
	double dBaseline = 0.0;
	double dMidCourt = 0.0;
	u_int uSamples = 0u;
	for (int i = 0; i < 64; i++)
	{
		const float fU = 0.40f + 0.20f * (static_cast<float>(i) / 63.0f);
		dBaseline += RT_CourtWear(xL, fU, xL.m_fNearBase);
		dMidCourt += RT_CourtWear(xL, fU, 0.5f);
		uSamples++;
	}
	ZENITH_ASSERT_GT(dBaseline, dMidCourt,
		"the baseline is not more worn than mid-court -- the wear mask is inert");
	ZENITH_ASSERT_GT(dBaseline / uSamples, 0.15f, "baseline wear is too faint to see");

	// The apron behind the court is untouched turf.
	ZENITH_ASSERT_EQ_FLOAT(RT_CourtWear(xL, 0.5f, 0.005f), 0.0f, 0.001f,
		"the grass apron is worn -- nobody stands there");

	// Patchy, not a clean band: a uniform wear band reads as a painted stripe.
	float fWearMin = 2.0f;
	float fWearMax = -1.0f;
	for (int i = 0; i < 256; i++)
	{
		const float fU = 0.35f + 0.30f * (static_cast<float>(i) / 255.0f);
		const float fW = RT_CourtWear(xL, fU, xL.m_fNearBase);
		fWearMin = std::min(fWearMin, fW);
		fWearMax = std::max(fWearMax, fW);
	}
	ZENITH_ASSERT_GT(fWearMax - fWearMin, 0.10f,
		"baseline wear is uniform along the line -- it will read as a painted band");
}

ZENITH_TEST(TennisCourt, WornTurfIsPalerThanHealthyTurf)
{
	// Dead grass is PALE, not dark. Getting this backwards is a common and very
	// convincing-looking mistake: a dark scuff reads as a shadow.
	const RT_CourtColourOnce& x = RT_CourtColour();
	const RT_CourtLayout xL = RT_MakeCourtLayout();

	const uint32_t uBaselineRow = static_cast<uint32_t>(xL.m_fNearBase * x.m_uH) + 6u;
	const uint32_t uMidRow = x.m_uH / 2u;
	double dBaselineLuma = 0.0;
	double dMidLuma = 0.0;
	u_int uCount = 0u;
	for (uint32_t ix = static_cast<uint32_t>(0.42f * x.m_uW); ix < static_cast<uint32_t>(0.58f * x.m_uW); ix++)
	{
		// Skip anything painted; this clause is about the turf.
		const float fBaseG = RT_CourtTexel(x.m_xPx, x.m_uW, ix, uBaselineRow, 1);
		const float fMidG = RT_CourtTexel(x.m_xPx, x.m_uW, ix, uMidRow, 1);
		if (fBaseG > 0.75f || fMidG > 0.75f)
		{
			continue;
		}
		dBaselineLuma += RT_CourtTexel(x.m_xPx, x.m_uW, ix, uBaselineRow, 0) * 0.30f + fBaseG * 0.59f +
			RT_CourtTexel(x.m_xPx, x.m_uW, ix, uBaselineRow, 2) * 0.11f;
		dMidLuma += RT_CourtTexel(x.m_xPx, x.m_uW, ix, uMidRow, 0) * 0.30f + fMidG * 0.59f +
			RT_CourtTexel(x.m_xPx, x.m_uW, ix, uMidRow, 2) * 0.11f;
		uCount++;
	}
	ZENITH_ASSERT_GT(uCount, 0u, "no unpainted turf sampled");
	ZENITH_ASSERT_GT(dBaselineLuma, dMidLuma,
		"worn baseline turf is DARKER than healthy mid-court turf -- worn grass is pale, "
		"and a dark scuff reads as a shadow instead");
}

ZENITH_TEST(TennisCourt, CourtIsGreenNotBlueOrRed)
{
	// A channel-order slip (BGRA vs RGBA) survives every other assertion here
	// and ships a blue tennis court.
	const RT_CourtColourOnce& x = RT_CourtColour();
	double dR = 0.0, dG = 0.0, dB = 0.0;
	u_int uCount = 0u;
	for (uint32_t iy = 4; iy < x.m_uH; iy += 37u)
	{
		for (uint32_t ix = 4; ix < x.m_uW; ix += 41u)
		{
			const float fG = RT_CourtTexel(x.m_xPx, x.m_uW, ix, iy, 1);
			if (fG > 0.75f) { continue; }   // painted
			dR += RT_CourtTexel(x.m_xPx, x.m_uW, ix, iy, 0);
			dG += fG;
			dB += RT_CourtTexel(x.m_xPx, x.m_uW, ix, iy, 2);
			uCount++;
		}
	}
	ZENITH_ASSERT_GT(uCount, 0u, "no turf sampled");
	ZENITH_ASSERT_GT(dG, dR, "the court's turf is not predominantly green (R >= G)");
	ZENITH_ASSERT_GT(dG, dB, "the court's turf is not predominantly green (B >= G)");
}

ZENITH_TEST(TennisCourt, NormalMapIsUnitLengthAndCarriesTuftRelief)
{
	uint32_t uW = 0, uH = 0;
	const std::vector<uint8_t> xNrm = RT_MakeCourtNormalTexture(uW, uH);
	ZENITH_ASSERT_EQ(uW, 1024u, "court normal width");
	ZENITH_ASSERT_EQ(uH, 2048u, "court normal height");

	double dSumZ = 0.0;
	u_int uNonFlat = 0u;
	u_int uSamples = 0u;
	for (uint32_t iy = 0; iy < uH; iy += 3u)
	{
		for (uint32_t ix = 0; ix < uW; ix += 3u)
		{
			const float fX = RT_CourtTexel(xNrm, uW, ix, iy, 0) * 2.0f - 1.0f;
			const float fY = RT_CourtTexel(xNrm, uW, ix, iy, 1) * 2.0f - 1.0f;
			const float fZ = RT_CourtTexel(xNrm, uW, ix, iy, 2) * 2.0f - 1.0f;
			const float fLen = sqrtf(fX * fX + fY * fY + fZ * fZ);
			ZENITH_ASSERT_GT(fLen, 0.97f, "court normal texel is far from unit length");
			ZENITH_ASSERT_LT(fLen, 1.03f, "court normal texel is far from unit length");
			ZENITH_ASSERT_GT(fZ, 0.0f, "court normal texel points INTO the surface");
			dSumZ += fZ;
			uSamples++;
			if (fX * fX + fY * fY > 0.02f) { uNonFlat++; }
		}
	}
	const float fMeanZ = static_cast<float>(dSumZ / uSamples);
	ZENITH_ASSERT_GT(fMeanZ, 0.85f, "the court normal map is not predominantly +Z");
	ZENITH_ASSERT_GT(uNonFlat, uSamples / 100u,
		"the court normal map is essentially flat -- it carries no grass tufts");
}

ZENITH_TEST(TennisCourt, PaintIsSmootherThanGrassInTheRMMap)
{
	// The one material difference that makes lines read as paint on grass
	// rather than as a lighter shade of grass. An RM map that missed it would
	// still be a valid map, still be bound, and change nothing.
	std::vector<uint8_t> xRM, xAO;
	uint32_t uW = 0, uH = 0;
	RT_MakeCourtDataTextures(xRM, xAO, uW, uH);
	ZENITH_ASSERT_EQ(uW, 512u, "court RM width");
	ZENITH_ASSERT_EQ(uH, 1024u, "court RM height");

	const RT_CourtLayout xL = RT_MakeCourtLayout();
	std::vector<RT_CourtRect> xRects;
	RT_BuildCourtLineRects(xL, xRects);

	double dPaintRough = 0.0;
	double dGrassRough = 0.0;
	u_int uPaint = 0u;
	u_int uGrass = 0u;
	double dAOSum = 0.0;
	for (uint32_t iy = 0; iy < uH; iy += 2u)
	{
		for (uint32_t ix = 0; ix < uW; ix += 2u)
		{
			const float fCov = RT_CourtPaintCoverage(xRects, uW, uH, ix, iy);
			const float fRough = RT_CourtTexel(xRM, uW, ix, iy, 1);
			// Metallic must stay 0: turf and line paint are both dielectric, and
			// a stray metallic makes the whole court mirror the sky.
			ZENITH_ASSERT_EQ_FLOAT(RT_CourtTexel(xRM, uW, ix, iy, 2), 0.0f, 0.001f,
				"the court RM map carries non-zero metallic");
			if (fCov > 0.98f) { dPaintRough += fRough; uPaint++; }
			else if (fCov < 0.01f) { dGrassRough += fRough; uGrass++; }
			dAOSum += RT_CourtTexel(xAO, uW, ix, iy, 0);
		}
	}
	ZENITH_ASSERT_GT(uPaint, 0u, "no fully painted texels in the RM map");
	ZENITH_ASSERT_GT(uGrass, 0u, "no fully unpainted texels in the RM map");
	ZENITH_ASSERT_LT(dPaintRough / uPaint, dGrassRough / uGrass,
		"line paint is not smoother than turf -- the RM map has no effect");

	// AO must darken somewhere and must not have collapsed toward black.
	const float fAOMean = static_cast<float>(dAOSum / ((uH / 2u) * (uW / 2u)));
	ZENITH_ASSERT_LT(fAOMean, 0.995f, "the court AO map is effectively white -- binding it changes nothing");
	ZENITH_ASSERT_GT(fAOMean, 0.60f, "the court AO map has collapsed toward black");
}

ZENITH_TEST(TennisCourt, CourtMapsAreDeterministic)
{
	// Generated bytes are a pure function of the layout and the fixed seeds:
	// a boot that re-baked a different court would churn the asset and quietly
	// invalidate every capture taken against the previous one.
	uint32_t uW1 = 0, uH1 = 0, uW2 = 0, uH2 = 0;
	const std::vector<uint8_t> xA = RT_MakeCourtTexture(uW1, uH1);
	const std::vector<uint8_t> xB = RT_MakeCourtTexture(uW2, uH2);
	ZENITH_ASSERT_EQ(uW1, uW2, "court width differs between runs");
	ZENITH_ASSERT_EQ(uH1, uH2, "court height differs between runs");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xA.size()), static_cast<u_int>(xB.size()),
		"court buffer size differs between runs");
	for (size_t u = 0; u < xA.size(); u++)
	{
		if (xA[u] != xB[u])
		{
			ZENITH_ASSERT_EQ(static_cast<u_int>(xA[u]), static_cast<u_int>(xB[u]),
				"the court texture is not a pure function of its seeds");
			break;
		}
	}
}

#endif // ZENITH_TESTING
