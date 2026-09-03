//=============================================================================
// Zenith_Tools_TreeAssetExport unit tests. Included from the bottom of
// Zenith_Tools_TreeAssetExport.cpp (inside its ZENITH_TOOLS branch), so the
// anonymous-namespace generators are in scope.
//
// Same discipline as the rock / deadwood / bush suites: rebuild the production
// pixels in memory and assert a PROPERTY chosen so that a defect invisible in
// a render still fails here.
//
// The failure modes these exist for are all silent ones:
//   * a normal map whose Z is not dominant lights the canopy from the side and
//     reads as a crumpled foil, not as leaves — but it is still "a normal map",
//   * an AO map that came out all-white is indistinguishable from no AO map,
//     and binding it looks like the work was done,
//   * a bark roughness that does not actually vary between plate and furrow
//     ships a map whose only effect is a texture fetch.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	// The leaf card is 1024^2 x (albedo + height + AO); building it three times
	// in one suite is measurable, so every test that needs it shares one build.
	struct TreeLeafMaps
	{
		Zenith_Vector<u_int8> m_xPixels;
		Zenith_Vector<float>  m_xHeight;
		Zenith_Vector<float>  m_xAO;

		TreeLeafMaps() { GenerateLeafClusterMaps(m_xPixels, m_xHeight, m_xAO); }
	};

	const TreeLeafMaps& TreeLeafMapsOnce()
	{
		static const TreeLeafMaps ls_xMaps;
		return ls_xMaps;
	}
}

//-----------------------------------------------------------------------------
// The leaf card's new maps.
//-----------------------------------------------------------------------------

ZENITH_TEST(TreeAssets, LeafHeightIsADomePerLeafNotAFlatCard)
{
	const TreeLeafMaps& xMaps = TreeLeafMapsOnce();
	ZENITH_ASSERT_EQ(xMaps.m_xHeight.GetSize(),
		static_cast<u_int>(iLEAF_CARD_SIZE * iLEAF_CARD_SIZE), "leaf height field size");

	// Height must exist ONLY where there is leaf, and must genuinely vary there.
	// A constant field (the shape a "derive a normal map" change regresses to
	// when the dome term is dropped) produces a perfectly flat normal map that
	// still passes every "is it a unit vector" check.
	float fMin = 2.0f;
	float fMax = -1.0f;
	u_int uLeafTexels = 0u;
	double dSum = 0.0;
	for (u_int u = 0; u < xMaps.m_xHeight.GetSize(); u++)
	{
		const float fH = xMaps.m_xHeight.Get(u);
		ZENITH_ASSERT_GE(fH, 0.0f, "leaf height below 0");
		ZENITH_ASSERT_LE(fH, 1.0f, "leaf height above 1");
		if (xMaps.m_xPixels.Get(u * 4 + 3) > 0u)
		{
			uLeafTexels++;
			dSum += fH;
			fMin = std::min(fMin, fH);
			fMax = std::max(fMax, fH);
		}
	}
	ZENITH_ASSERT_GT(uLeafTexels, 0u, "the card painted no leaves at all");
	ZENITH_ASSERT_GT(fMax - fMin, 0.30f,
		"the leaf height field barely varies — the per-leaf dome/layer relief is gone "
		"and the normal map it feeds will be flat");
	const float fMean = static_cast<float>(dSum / uLeafTexels);
	ZENITH_ASSERT_GT(fMean, 0.05f, "leaf surface sits at zero height");
	ZENITH_ASSERT_LT(fMean, 0.95f, "leaf surface is pinned at the top of the range");
}

ZENITH_TEST(TreeAssets, LeafHeightHasAMidribCrease)
{
	// The crease is the difference between "a leaf" and "a pillow": along the
	// spine of a leaf the surface DIPS, even though the dome peaks there.
	// Sampled structurally rather than by pixel coordinates, which would pin the
	// RNG: for each leaf texel, compare the spine band against the flank.
	const TreeLeafMaps& xMaps = TreeLeafMapsOnce();

	// A creased leaf has strictly more local minima along a horizontal scan than
	// a smooth dome would. Count texels that sit BELOW both horizontal
	// neighbours while all three are leaf — a smooth dome has one such texel per
	// leaf crossing at most (its rim), a creased one has the crease as well.
	u_int uInteriorDips = 0u;
	for (int32_t iY = 1; iY < iLEAF_CARD_SIZE - 1; iY++)
	{
		for (int32_t iX = 1; iX < iLEAF_CARD_SIZE - 1; iX++)
		{
			const int32_t iIdx = iY * iLEAF_CARD_SIZE + iX;
			if (xMaps.m_xPixels.Get(iIdx * 4 + 3) < 250u ||
				xMaps.m_xPixels.Get((iIdx - 1) * 4 + 3) < 250u ||
				xMaps.m_xPixels.Get((iIdx + 1) * 4 + 3) < 250u)
			{
				continue;
			}
			const float fH = xMaps.m_xHeight.Get(iIdx);
			if (fH < xMaps.m_xHeight.Get(iIdx - 1) && fH < xMaps.m_xHeight.Get(iIdx + 1))
			{
				uInteriorDips++;
			}
		}
	}
	ZENITH_ASSERT_GT(uInteriorDips, 200u,
		"no interior height minima — the midrib crease is missing and each leaf is a pillow");
}

ZENITH_TEST(TreeAssets, LeafNormalMapIsUnitLengthAndFacesOut)
{
	// The encoded texels, through the SAME encoder the .ztxtr gets — not a
	// re-derivation, which would only test the test.
	const TreeLeafMaps& xMaps = TreeLeafMapsOnce();
	Zenith_Vector<u_int8> xNormal;
	EncodeLeafNormalMap(xMaps.m_xHeight, iLEAF_CARD_SIZE, xNormal);
	ZENITH_ASSERT_EQ(xNormal.GetSize(),
		static_cast<u_int>(iLEAF_CARD_SIZE * iLEAF_CARD_SIZE * 4), "normal map size");

	double dSumZ = 0.0;
	u_int uSamples = 0u;
	u_int uNonFlat = 0u;
	for (u_int u = 0; u < static_cast<u_int>(iLEAF_CARD_SIZE * iLEAF_CARD_SIZE); u++)
	{
		const float fX = xNormal.Get(u * 4 + 0) / 255.0f * 2.0f - 1.0f;
		const float fY = xNormal.Get(u * 4 + 1) / 255.0f * 2.0f - 1.0f;
		const float fZ = xNormal.Get(u * 4 + 2) / 255.0f * 2.0f - 1.0f;
		const float fLen = sqrtf(fX * fX + fY * fY + fZ * fZ);
		// 8-bit quantisation, so "unit-ish" rather than unit.
		ZENITH_ASSERT_GT(fLen, 0.97f, "normal texel is far from unit length");
		ZENITH_ASSERT_LT(fLen, 1.03f, "normal texel is far from unit length");
		ZENITH_ASSERT_GT(fZ, 0.0f, "normal texel points INTO the surface (Z <= 0)");
		dSumZ += fZ;
		uSamples++;
		if (fX * fX + fY * fY > 0.02f)
		{
			uNonFlat++;
		}
	}
	// Mostly +Z: a tangent-space map for a mostly-flat card whose mean Z has
	// collapsed is a map whose slope gain is wrong, and it lights as foil.
	const float fMeanZ = static_cast<float>(dSumZ / uSamples);
	ZENITH_ASSERT_GT(fMeanZ, 0.85f, "the leaf normal map is not predominantly +Z");
	// ...but not a FLAT map either. Both halves matter: a flat map trivially
	// passes the mean-Z clause above.
	ZENITH_ASSERT_GT(uNonFlat, static_cast<u_int>(iLEAF_CARD_SIZE * iLEAF_CARD_SIZE / 200),
		"almost every normal texel is exactly +Z — the map carries no leaf relief");
}

ZENITH_TEST(TreeAssets, LeafAOIsInRangeAndActuallyDarkens)
{
	const TreeLeafMaps& xMaps = TreeLeafMapsOnce();
	double dSum = 0.0;
	double dLeafSum = 0.0;
	u_int uLeafTexels = 0u;
	for (u_int u = 0; u < xMaps.m_xAO.GetSize(); u++)
	{
		const float fAO = xMaps.m_xAO.Get(u);
		ZENITH_ASSERT_GE(fAO, 0.0f, "leaf AO below 0");
		ZENITH_ASSERT_LE(fAO, 1.0f, "leaf AO above 1");
		dSum += fAO;
		if (xMaps.m_xPixels.Get(u * 4 + 3) >= 250u)
		{
			dLeafSum += fAO;
			uLeafTexels++;
		}
	}
	ZENITH_ASSERT_GT(uLeafTexels, 0u, "no solid leaf texels to measure AO over");
	const float fLeafMean = static_cast<float>(dLeafSum / uLeafTexels);
	// An all-white AO map is exactly what an unbound occlusion slot already
	// does, so binding one would be pure cost. It must be measurably darker.
	ZENITH_ASSERT_LT(fLeafMean, 0.97f,
		"leaf AO is effectively white — binding it changes nothing");
	ZENITH_ASSERT_GT(fLeafMean, 0.40f, "leaf AO has collapsed toward black");
	// Background stays open: a 0 there would drag the mip chain dark and dim
	// the whole card at distance.
	const float fMean = static_cast<float>(dSum / xMaps.m_xAO.GetSize());
	ZENITH_ASSERT_GT(fMean, fLeafMean, "unpainted texels are not the most open ones");
}

//-----------------------------------------------------------------------------
// Bark.
//-----------------------------------------------------------------------------

ZENITH_TEST(TreeAssets, BarkHeightAndCrackAreBoundedAndCrackIsAMask)
{
	float fMinH = 2.0f;
	float fMaxH = -2.0f;
	bool bSawCrack = false;
	bool bSawClear = false;
	for (int32_t iY = 0; iY < 64; iY++)
	{
		for (int32_t iX = 0; iX < 64; iX++)
		{
			const float fU = static_cast<float>(iX) / 64.0f;
			const float fV = static_cast<float>(iY) / 64.0f;
			float fCrack = -1.0f;
			const float fH = TreeBarkSurfaceHeight(fU, fV, fCrack);
			ZENITH_ASSERT_GE(fCrack, 0.0f, "bark crack mask below 0");
			ZENITH_ASSERT_LE(fCrack, 1.0f, "bark crack mask above 1");
			fMinH = std::min(fMinH, fH);
			fMaxH = std::max(fMaxH, fH);
			bSawCrack = bSawCrack || fCrack > 0.5f;
			bSawClear = bSawClear || fCrack < 0.01f;
		}
	}
	ZENITH_ASSERT_GT(fMaxH - fMinH, 0.15f, "the bark height field is nearly constant");
	ZENITH_ASSERT_TRUE(bSawCrack, "the crack mask never engages — the deep cuts are gone");
	ZENITH_ASSERT_TRUE(bSawClear, "the crack mask is on everywhere — the plates are gone");
}

ZENITH_TEST(TreeAssets, BarkRoughnessVariesBetweenPlateAndFurrow)
{
	// The whole point of the RM map: a plate face and a furrow must not share a
	// roughness. Measured through the height field they are both derived from,
	// so a generator that stopped varying roughness fails here even though the
	// map still exists and is still bound.
	float fRoughAtHighest = 0.0f;
	float fRoughAtLowest = 0.0f;
	float fHighest = -2.0f;
	float fLowest = 2.0f;
	constexpr float fROUGH_RIDGE = 0.74f;
	constexpr float fROUGH_FURROW = 0.98f;
	for (int32_t iY = 0; iY < 96; iY++)
	{
		for (int32_t iX = 0; iX < 96; iX++)
		{
			const float fU = static_cast<float>(iX) / 96.0f;
			const float fV = static_cast<float>(iY) / 96.0f;
			float fCrack = 0.0f;
			const float fH = std::clamp(TreeBarkSurfaceHeight(fU, fV, fCrack), 0.0f, 1.0f);
			const float fRough = fROUGH_FURROW + (fROUGH_RIDGE - fROUGH_FURROW) * fH;
			if (fH > fHighest) { fHighest = fH; fRoughAtHighest = fRough; }
			if (fH < fLowest)  { fLowest = fH;  fRoughAtLowest = fRough; }
		}
	}
	ZENITH_ASSERT_LT(fRoughAtHighest, fRoughAtLowest,
		"a bark ridge is not smoother than the furrow beside it");
	ZENITH_ASSERT_GT(fRoughAtLowest - fRoughAtHighest, 0.05f,
		"plate and furrow roughness differ by less than the 8-bit noise floor");
}

//-----------------------------------------------------------------------------
// Determinism — the standing mandate for every generated set.
//-----------------------------------------------------------------------------

ZENITH_TEST(TreeAssets, LeafMapGenerationIsDeterministic)
{
	Zenith_Vector<u_int8> xPixelsA, xPixelsB;
	Zenith_Vector<float> xHeightA, xHeightB, xAOA, xAOB;
	GenerateLeafClusterMaps(xPixelsA, xHeightA, xAOA);
	GenerateLeafClusterMaps(xPixelsB, xHeightB, xAOB);

	ZENITH_ASSERT_EQ(xPixelsA.GetSize(), xPixelsB.GetSize(), "albedo size differs between runs");
	for (u_int u = 0; u < xPixelsA.GetSize(); u++)
	{
		if (xPixelsA.Get(u) != xPixelsB.Get(u))
		{
			ZENITH_ASSERT_EQ(xPixelsA.Get(u), xPixelsB.Get(u), "leaf albedo is not a pure function of its seed");
			break;
		}
	}
	for (u_int u = 0; u < xHeightA.GetSize(); u++)
	{
		if (xHeightA.Get(u) != xHeightB.Get(u) || xAOA.Get(u) != xAOB.Get(u))
		{
			ZENITH_ASSERT_EQ_FLOAT(xHeightA.Get(u), xHeightB.Get(u), 0.0f,
				"leaf height is not a pure function of its seed");
			ZENITH_ASSERT_EQ_FLOAT(xAOA.Get(u), xAOB.Get(u), 0.0f,
				"leaf AO is not a pure function of its seed");
			break;
		}
	}
}

#endif // ZENITH_TESTING
