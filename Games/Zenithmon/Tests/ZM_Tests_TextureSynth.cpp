#include "Zenith.h"

// ============================================================================
// ZM_Tests_TextureSynth -- S4 determinism + structure gate for the procedural
// texture synthesis library (category ZM_Gen, matching the test contract). All
// cases are pure/in-memory: no disk, no GPU, no ZENITH_TOOLS. Small resolutions
// (64/128) keep the per-case cost in the unit-gate ms budget; the sRGB pack and
// palette contracts run on tiny fixed images.
//
// These pin the load-bearing S4 invariant (AssetManifest 6.2): texels are a
// pure function of a species/family-derived seed, so a re-run with the same
// seed is byte-identical and a different family seed diverges. See the frozen
// header Games/Zenithmon/Source/Gen/ZM_TextureSynth.h + DecisionLog ZM-D-059.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"
#include "Zenithmon/Source/Data/ZM_Types.h"

#include <cmath>
#include <cstring>

namespace
{
	// A representative dual-typed creature recipe at a small resolution. Uses a
	// SPOTS pattern so ZM_SynthCreatureAlbedo actually draws from the supplied
	// ZM_GenRNG -- the determinism test needs an RNG-consuming path to be
	// meaningful (a solid fill would be trivially reproducible).
	ZM_CreatureTexRecipe MakeSmallSpottedRecipe(u_int uSize)
	{
		ZM_CreatureTexRecipe xRecipe;
		xRecipe.m_ePrimaryType = ZM_TYPE_FIRE;
		xRecipe.m_eSecondaryType = ZM_TYPE_WATER;
		xRecipe.m_xPattern.m_eKind = ZM_PATTERN_SPOTS;
		xRecipe.m_xPattern.m_fFrequency = 6.0f;
		xRecipe.m_xPattern.m_fContrast = 0.85f;
		xRecipe.m_xPattern.m_fJitter = 0.4f;
		xRecipe.m_xPattern.m_uCount = 16u;
		xRecipe.m_fEyeU = 0.5f;
		xRecipe.m_fEyeV = 0.35f;
		xRecipe.m_fEyeRadius = 0.08f;
		xRecipe.m_uWidth = uSize;
		xRecipe.m_uHeight = uSize;
		return xRecipe;
	}

	// Byte-exact compare of two packed RGBA8 buffers.
	bool PackedBytesEqual(const Zenith_Vector<u_int8>& xA, const Zenith_Vector<u_int8>& xB)
	{
		if (xA.GetSize() != xB.GetSize())
		{
			return false;
		}
		if (xA.GetSize() == 0u)
		{
			return true;
		}
		return memcmp(xA.GetDataPointer(), xB.GetDataPointer(), xA.GetSize()) == 0;
	}

	bool Vec3ExactEqual(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		return xA.x == xB.x && xA.y == xB.y && xA.z == xB.z;
	}

	bool Vec4ExactEqual(const Zenith_Maths::Vector4& xA, const Zenith_Maths::Vector4& xB)
	{
		return xA.x == xB.x && xA.y == xB.y && xA.z == xB.z && xA.w == xB.w;
	}

	bool PaletteExactEqual(const ZM_TypePalette& xA, const ZM_TypePalette& xB)
	{
		return Vec3ExactEqual(xA.m_xBase, xB.m_xBase) &&
			Vec3ExactEqual(xA.m_xAccent, xB.m_xAccent) &&
			Vec3ExactEqual(xA.m_xBelly, xB.m_xBelly);
	}

	bool ChannelInUnit(float fChannel)
	{
		// NaN fails both comparisons, so this also rejects non-finite values.
		return fChannel >= 0.0f && fChannel <= 1.0f;
	}

	bool Vec3InUnit(const Zenith_Maths::Vector3& xColour)
	{
		return ChannelInUnit(xColour.x) && ChannelInUnit(xColour.y) && ChannelInUnit(xColour.z);
	}
}

// ---------------------------------------------------------------------------
// Same recipe + same-seed RNG => byte-identical creature albedo (the headline
// texel-determinism gate). Both Equals() and ContentHash() must agree.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_SameSeedTexelsByteIdentical)
{
	const ZM_CreatureTexRecipe xRecipe = MakeSmallSpottedRecipe(64u);
	const u_int64 ulSeed = ZM_GenDeriveSeed(0x7BF32CA4u, 12u, 1u, ZM_GEN_DOMAIN_ALBEDO);

	ZM_GenRNG xRngA(ulSeed);
	ZM_GenRNG xRngB(ulSeed);
	const ZM_GenImage xImgA = ZM_SynthCreatureAlbedo(xRecipe, xRngA);
	const ZM_GenImage xImgB = ZM_SynthCreatureAlbedo(xRecipe, xRngB);

	ZENITH_ASSERT_EQ(xImgA.GetWidth(), 64u);
	ZENITH_ASSERT_EQ(xImgA.GetHeight(), 64u);
	ZENITH_ASSERT_TRUE(xImgA.Equals(xImgB), "same recipe+seed albedo must be byte-identical");
	ZENITH_ASSERT_EQ(xImgA.ContentHash(), xImgB.ContentHash(),
		"same recipe+seed albedo must share a content hash");
}

// ---------------------------------------------------------------------------
// Different family seed => different derived albedo seed => differing texels.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_DifferentFamilySeedDiffers)
{
	const ZM_CreatureTexRecipe xRecipe = MakeSmallSpottedRecipe(64u);
	const u_int64 ulSeedA = ZM_GenDeriveSeed(0x11111111u, 7u, 1u, ZM_GEN_DOMAIN_ALBEDO);
	const u_int64 ulSeedB = ZM_GenDeriveSeed(0x22222222u, 7u, 1u, ZM_GEN_DOMAIN_ALBEDO);
	ZENITH_ASSERT_NE(ulSeedA, ulSeedB, "distinct family seeds must derive distinct albedo seeds");

	ZM_GenRNG xRngA(ulSeedA);
	ZM_GenRNG xRngB(ulSeedB);
	const ZM_GenImage xImgA = ZM_SynthCreatureAlbedo(xRecipe, xRngA);
	const ZM_GenImage xImgB = ZM_SynthCreatureAlbedo(xRecipe, xRngB);

	ZENITH_ASSERT_EQ(xImgA.GetWidth(), xImgB.GetWidth());
	ZENITH_ASSERT_EQ(xImgA.GetHeight(), xImgB.GetHeight());
	ZENITH_ASSERT_FALSE(xImgA.Equals(xImgB),
		"different family seeds must produce differing texels");
	ZENITH_ASSERT_NE(xImgA.ContentHash(), xImgB.ContentHash(),
		"different family seeds must produce differing content hashes");
}

// ---------------------------------------------------------------------------
// Shiny (hue-rotate) differs from its source at identical dimensions, is
// deterministic on re-run, and still differs for an achromatic source (the
// saturation/lightness nudge path).
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_ShinyDiffersSameDims)
{
	// Chromatic source: a real creature albedo.
	const ZM_CreatureTexRecipe xRecipe = MakeSmallSpottedRecipe(64u);
	ZM_GenRNG xRng(ZM_GenDeriveSeed(0x7BF32CA4u, 3u, 2u, ZM_GEN_DOMAIN_ALBEDO));
	const ZM_GenImage xBase = ZM_SynthCreatureAlbedo(xRecipe, xRng);

	const ZM_GenImage xShinyA = ZM_SynthHueRotate(xBase, 137.0f);
	const ZM_GenImage xShinyB = ZM_SynthHueRotate(xBase, 137.0f);

	ZENITH_ASSERT_EQ(xShinyA.GetWidth(), xBase.GetWidth());
	ZENITH_ASSERT_EQ(xShinyA.GetHeight(), xBase.GetHeight());
	ZENITH_ASSERT_FALSE(xShinyA.Equals(xBase), "shiny must differ from its source albedo");
	ZENITH_ASSERT_TRUE(xShinyA.Equals(xShinyB), "identical hue rotation must be deterministic");
	ZENITH_ASSERT_EQ(xShinyA.ContentHash(), xShinyB.ContentHash());

	// Achromatic source: uniform grey. The hue-rotate must still change the
	// output via the nudge path so "shiny differs" always holds.
	ZM_GenImage xGrey(64u, 64u);
	ZM_SynthFillSolid(xGrey, Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f));
	const ZM_GenImage xGreyShiny = ZM_SynthHueRotate(xGrey, 137.0f);
	ZENITH_ASSERT_EQ(xGreyShiny.GetWidth(), xGrey.GetWidth());
	ZENITH_ASSERT_EQ(xGreyShiny.GetHeight(), xGrey.GetHeight());
	ZENITH_ASSERT_FALSE(xGreyShiny.Equals(xGrey),
		"achromatic shiny must still differ via the nudge path");
	ZENITH_ASSERT_NE(xGreyShiny.ContentHash(), xGrey.ContentHash());
}

// ---------------------------------------------------------------------------
// Every one of the 18 elemental types has an in-gamut palette row, and the
// primaries are not all the same colour (guards a missing / duplicated row).
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_PaletteCoversAll18Types)
{
	ZENITH_ASSERT_EQ((u_int)ZM_TYPE_COUNT, 18u, "type space must be the 18 elemental types");

	const ZM_TypePalette xFirst = ZM_SynthTypePalette((ZM_TYPE)0);
	bool bAnyDiffersFromFirst = false;
	for (u_int uType = 0u; uType < (u_int)ZM_TYPE_COUNT; ++uType)
	{
		const ZM_TypePalette xPalette = ZM_SynthTypePalette((ZM_TYPE)uType);
		ZENITH_ASSERT_TRUE(Vec3InUnit(xPalette.m_xBase),
			"base colour out of [0,1] for type %u", uType);
		ZENITH_ASSERT_TRUE(Vec3InUnit(xPalette.m_xAccent),
			"accent colour out of [0,1] for type %u", uType);
		ZENITH_ASSERT_TRUE(Vec3InUnit(xPalette.m_xBelly),
			"belly colour out of [0,1] for type %u", uType);
		if (!Vec3ExactEqual(xPalette.m_xBase, xFirst.m_xBase))
		{
			bAnyDiffersFromFirst = true;
		}
	}
	ZENITH_ASSERT_TRUE(bAnyDiffersFromFirst,
		"the 18 primary colours must not all be identical (missing table rows)");
}

// ---------------------------------------------------------------------------
// Dual-type blend palette: deterministic, distinct from the mono palette,
// order-sensitive, and reduces to the mono palette when the secondary is NONE.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_BlendPaletteDeterministicAndOrdered)
{
	const ZM_TypePalette xBlendAB1 = ZM_SynthBlendPalette(ZM_TYPE_FIRE, ZM_TYPE_WATER);
	const ZM_TypePalette xBlendAB2 = ZM_SynthBlendPalette(ZM_TYPE_FIRE, ZM_TYPE_WATER);
	ZENITH_ASSERT_TRUE(PaletteExactEqual(xBlendAB1, xBlendAB2),
		"blend palette must be deterministic");

	const ZM_TypePalette xMonoA = ZM_SynthTypePalette(ZM_TYPE_FIRE);
	ZENITH_ASSERT_FALSE(PaletteExactEqual(xBlendAB1, xMonoA),
		"dual-type blend must differ from the mono primary palette");

	const ZM_TypePalette xBlendBA = ZM_SynthBlendPalette(ZM_TYPE_WATER, ZM_TYPE_FIRE);
	ZENITH_ASSERT_FALSE(PaletteExactEqual(xBlendAB1, xBlendBA),
		"blend must be order-sensitive: blend(a,b) != blend(b,a)");

	const ZM_TypePalette xBlendANone = ZM_SynthBlendPalette(ZM_TYPE_FIRE, ZM_TYPE_NONE);
	ZENITH_ASSERT_TRUE(PaletteExactEqual(xBlendANone, xMonoA),
		"blend(x, NONE) must reduce to the mono palette of x");
}

// ---------------------------------------------------------------------------
// PackRGBA8 is byte-stable across calls with fixed pinned bytes (golden row),
// and the sRGB-encoded path diverges from the linear path on a mid-grey input.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_PackRGBA8DeterministicAndSRGBDiffers)
{
	ZM_GenImage xImg(8u, 8u);
	ZM_SynthFillSolid(xImg, Zenith_Maths::Vector3(0.5f, 0.25f, 0.75f));

	Zenith_Vector<u_int8> xLinearA;
	Zenith_Vector<u_int8> xLinearB;
	xImg.PackRGBA8(xLinearA, false);
	xImg.PackRGBA8(xLinearB, false);

	ZENITH_ASSERT_EQ(xLinearA.GetSize(), 8u * 8u * 4u, "packed buffer must be w*h*4 bytes");
	ZENITH_ASSERT_TRUE(PackedBytesEqual(xLinearA, xLinearB),
		"linear pack must be byte-stable across calls");

	// Golden quantisation: (u_int8)(clamp01(c) * 255 + 0.5).
	ZENITH_ASSERT_EQ((u_int)xLinearA.Get(0u), 128u, "R: 0.5 linear -> 128");
	ZENITH_ASSERT_EQ((u_int)xLinearA.Get(1u), 64u, "G: 0.25 linear -> 64");
	ZENITH_ASSERT_EQ((u_int)xLinearA.Get(2u), 191u, "B: 0.75 linear -> 191");
	ZENITH_ASSERT_EQ((u_int)xLinearA.Get(3u), 255u, "A: 1.0 -> 255");

	Zenith_Vector<u_int8> xSRGB;
	xImg.PackRGBA8(xSRGB, true);
	ZENITH_ASSERT_EQ(xSRGB.GetSize(), xLinearA.GetSize());
	ZENITH_ASSERT_FALSE(PackedBytesEqual(xSRGB, xLinearA),
		"sRGB-encoded pack must differ from the linear pack on mid-grey");
	// The sRGB OETF lifts mid-grey above the linear byte (0.5 -> ~0.735).
	ZENITH_ASSERT_GT((u_int)xSRGB.Get(0u), (u_int)xLinearA.Get(0u),
		"sRGB encode must lift the mid-grey R channel above the linear value");
}

// ---------------------------------------------------------------------------
// Normal-from-height: flat source encodes ~(0.5,0.5,1); a linear X ramp shifts
// R consistently below 0.5 with G ~0.5; decoded vectors are ~unit length; the
// transform is deterministic.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_NormalFromHeightEncoding)
{
	const u_int uSize = 64u;
	const float fStrength = 10.0f;

	// Flat height field -> flat normal (0,0,1) -> encoded (0.5,0.5,1).
	ZM_GenImage xFlat(uSize, uSize);
	ZM_SynthFillSolid(xFlat, Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f));
	const ZM_GenImage xFlatNormal = ZM_SynthNormalFromHeight(xFlat, fStrength);
	ZENITH_ASSERT_EQ(xFlatNormal.GetWidth(), uSize);
	ZENITH_ASSERT_EQ(xFlatNormal.GetHeight(), uSize);
	const Zenith_Maths::Vector4 xFlatTexel = xFlatNormal.Get(uSize / 2u, uSize / 2u);
	ZENITH_ASSERT_EQ_FLOAT(xFlatTexel.x, 0.5f, 0.02f, "flat normal R must encode ~0.5");
	ZENITH_ASSERT_EQ_FLOAT(xFlatTexel.y, 0.5f, 0.02f, "flat normal G must encode ~0.5");
	ZENITH_ASSERT_GT(xFlatTexel.z, 0.99f, "flat normal B must encode ~1.0");

	// Linear ramp in X (height = u across the row) -> constant positive dX ->
	// n.x negative -> encoded R below 0.5 uniformly across interior columns.
	ZM_GenImage xRamp(uSize, uSize);
	for (u_int uY = 0u; uY < uSize; ++uY)
	{
		for (u_int uX = 0u; uX < uSize; ++uX)
		{
			const float fHeight = (float)uX / (float)(uSize - 1u);
			xRamp.Set(uY, uX, Zenith_Maths::Vector4(fHeight, fHeight, fHeight, 1.0f));
		}
	}
	const ZM_GenImage xRampNormalA = ZM_SynthNormalFromHeight(xRamp, fStrength);
	const ZM_GenImage xRampNormalB = ZM_SynthNormalFromHeight(xRamp, fStrength);

	const Zenith_Maths::Vector4 xTexelLo = xRampNormalA.Get(uSize / 2u, 16u);
	const Zenith_Maths::Vector4 xTexelHi = xRampNormalA.Get(uSize / 2u, 48u);
	ZENITH_ASSERT_LT(xTexelLo.x, 0.5f, "positive X slope must push encoded R below 0.5");
	ZENITH_ASSERT_LT(xTexelHi.x, 0.5f, "positive X slope must push encoded R below 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xTexelLo.x, xTexelHi.x, 0.01f,
		"a constant slope must shift R consistently across interior columns");
	ZENITH_ASSERT_EQ_FLOAT(xTexelLo.y, 0.5f, 0.02f, "no Y slope -> G stays ~0.5");

	// Decoded direction is ~unit length (float image, so tight tolerance).
	const Zenith_Maths::Vector3 xDecoded(
		xTexelLo.x * 2.0f - 1.0f,
		xTexelLo.y * 2.0f - 1.0f,
		xTexelLo.z * 2.0f - 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(Zenith_Maths::Length(xDecoded), 1.0f, 0.01f,
		"decoded normal must be ~unit length");

	ZENITH_ASSERT_TRUE(xRampNormalA.Equals(xRampNormalB),
		"normal-from-height must be deterministic for the same source");
}

// ---------------------------------------------------------------------------
// Rect decal (building windows/doors reuse): writes only inside the rectangle
// and leaves texels outside untouched.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_RectDecalBounded)
{
	const u_int uSize = 64u;
	const Zenith_Maths::Vector3 xBase(0.2f, 0.3f, 0.4f);
	const Zenith_Maths::Vector3 xDecal(0.9f, 0.1f, 0.05f);

	ZM_GenImage xImg(uSize, uSize);
	ZM_SynthFillSolid(xImg, xBase);
	ZM_SynthStampRectDecal(xImg, 0.25f, 0.25f, 0.75f, 0.75f, xDecal);

	// Centre texel is well inside the rectangle -> painted the decal colour.
	const Zenith_Maths::Vector4 xInside = xImg.Get(uSize / 2u, uSize / 2u);
	ZENITH_ASSERT_EQ_FLOAT(xInside.x, xDecal.x, 0.0f, "inside-rect R must be the decal colour");
	ZENITH_ASSERT_EQ_FLOAT(xInside.y, xDecal.y, 0.0f, "inside-rect G must be the decal colour");
	ZENITH_ASSERT_EQ_FLOAT(xInside.z, xDecal.z, 0.0f, "inside-rect B must be the decal colour");

	// A corner texel is well outside the rectangle -> unchanged base colour.
	const Zenith_Maths::Vector4 xOutside = xImg.Get(2u, 2u);
	ZENITH_ASSERT_TRUE(Vec4ExactEqual(xOutside, Zenith_Maths::Vector4(xBase, 1.0f)),
		"outside-rect texel must remain the untouched base colour");
}

// ---------------------------------------------------------------------------
// Eye decal: changes texels within its radius, leaves distant texels untouched,
// and is byte-identical for identical parameters.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_EyeDecalDeterministic)
{
	const u_int uSize = 64u;
	const Zenith_Maths::Vector3 xBase(0.35f, 0.55f, 0.30f);
	const Zenith_Maths::Vector3 xIris(0.9f, 0.7f, 0.1f);
	const Zenith_Maths::Vector3 xPupil(0.02f, 0.02f, 0.02f);

	ZM_GenImage xImgA(uSize, uSize);
	ZM_SynthFillSolid(xImgA, xBase);
	ZM_SynthStampEyeDecal(xImgA, 0.5f, 0.5f, 0.12f, xIris, xPupil);

	// Centre texel lands inside the eye -> changed from the base colour.
	const Zenith_Maths::Vector4 xCentre = xImgA.Get(uSize / 2u, uSize / 2u);
	ZENITH_ASSERT_FALSE(Vec4ExactEqual(xCentre, Zenith_Maths::Vector4(xBase, 1.0f)),
		"eye centre texel must be modified");

	// A corner texel is far outside the radius -> unchanged.
	const Zenith_Maths::Vector4 xCorner = xImgA.Get(2u, 2u);
	ZENITH_ASSERT_TRUE(Vec4ExactEqual(xCorner, Zenith_Maths::Vector4(xBase, 1.0f)),
		"texel well outside the eye radius must be untouched");

	// Identical parameters -> byte-identical result.
	ZM_GenImage xImgB(uSize, uSize);
	ZM_SynthFillSolid(xImgB, xBase);
	ZM_SynthStampEyeDecal(xImgB, 0.5f, 0.5f, 0.12f, xIris, xPupil);
	ZENITH_ASSERT_TRUE(xImgA.Equals(xImgB), "identical eye-decal params must be byte-identical");
}

// ---------------------------------------------------------------------------
// Stripes are pure (no RNG, byte-stable); spots draw in a fixed order so a
// same-seed run is byte-identical, while a different spot count or seed differs.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_Gen, Tex_StripesAndSpotsDeterministic)
{
	const u_int uSize = 64u;
	const Zenith_Maths::Vector3 xBase(0.30f, 0.45f, 0.60f);
	const Zenith_Maths::Vector3 xInk(0.95f, 0.90f, 0.20f);

	ZM_PatternParams xStripeParams;
	xStripeParams.m_eKind = ZM_PATTERN_STRIPES;
	xStripeParams.m_fFrequency = 8.0f;
	xStripeParams.m_fContrast = 0.9f;
	xStripeParams.m_uCount = 12u;

	// Stripes: pure function, byte-stable and actually inks the image.
	ZM_GenImage xStripeA(uSize, uSize);
	ZM_GenImage xStripeB(uSize, uSize);
	ZM_SynthFillSolid(xStripeA, xBase);
	ZM_SynthFillSolid(xStripeB, xBase);
	ZM_SynthApplyStripes(xStripeA, xStripeParams, xInk);
	ZM_SynthApplyStripes(xStripeB, xStripeParams, xInk);
	ZENITH_ASSERT_TRUE(xStripeA.Equals(xStripeB), "stripes must be pure and byte-stable");

	ZM_GenImage xStripeBase(uSize, uSize);
	ZM_SynthFillSolid(xStripeBase, xBase);
	ZENITH_ASSERT_FALSE(xStripeA.Equals(xStripeBase), "stripes must ink the base image");

	ZM_PatternParams xSpotParams;
	xSpotParams.m_eKind = ZM_PATTERN_SPOTS;
	xSpotParams.m_fFrequency = 6.0f;
	xSpotParams.m_fContrast = 0.85f;
	xSpotParams.m_fJitter = 0.4f;
	xSpotParams.m_uCount = 20u;

	const u_int64 ulSpotSeed = ZM_GenDeriveSeed(0x7BF32CA4u, 5u, 1u, ZM_GEN_DOMAIN_PATTERN);

	// Same params + same-seed RNG -> byte-identical (fixed draw order).
	ZM_GenImage xSpotsA(uSize, uSize);
	ZM_GenImage xSpotsB(uSize, uSize);
	ZM_SynthFillSolid(xSpotsA, xBase);
	ZM_SynthFillSolid(xSpotsB, xBase);
	ZM_GenRNG xSpotRngA(ulSpotSeed);
	ZM_GenRNG xSpotRngB(ulSpotSeed);
	ZM_SynthApplySpots(xSpotsA, xSpotParams, xInk, xSpotRngA);
	ZM_SynthApplySpots(xSpotsB, xSpotParams, xInk, xSpotRngB);
	ZENITH_ASSERT_TRUE(xSpotsA.Equals(xSpotsB),
		"same-seed same-param spots must be byte-identical (fixed draw order)");
	ZENITH_ASSERT_FALSE(xSpotsA.Equals(xStripeBase), "spots must ink the base image");

	// A different spot count diverges.
	ZM_PatternParams xFewerSpots = xSpotParams;
	xFewerSpots.m_uCount = 5u;
	ZM_GenImage xSpotsCount(uSize, uSize);
	ZM_SynthFillSolid(xSpotsCount, xBase);
	ZM_GenRNG xSpotRngCount(ulSpotSeed);
	ZM_SynthApplySpots(xSpotsCount, xFewerSpots, xInk, xSpotRngCount);
	ZENITH_ASSERT_FALSE(xSpotsA.Equals(xSpotsCount),
		"a different spot count must produce a different image");

	// A different seed diverges.
	ZM_GenImage xSpotsSeed(uSize, uSize);
	ZM_SynthFillSolid(xSpotsSeed, xBase);
	ZM_GenRNG xSpotRngSeed(ZM_GenDeriveSeed(0x0BADF00Du, 5u, 1u, ZM_GEN_DOMAIN_PATTERN));
	ZM_SynthApplySpots(xSpotsSeed, xSpotParams, xInk, xSpotRngSeed);
	ZENITH_ASSERT_FALSE(xSpotsA.Equals(xSpotsSeed),
		"a different spot seed must produce a different image");
}
