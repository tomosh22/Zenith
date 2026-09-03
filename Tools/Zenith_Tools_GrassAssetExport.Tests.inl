//=============================================================================
// Zenith_Tools_GrassAssetExport unit tests. Included from the bottom of
// Zenith_Tools_GrassAssetExport.cpp (inside its ZENITH_TOOLS branch), so the
// anonymous-namespace generators and THE type table are in scope.
//
// Nothing here touches the filesystem, the registry or the GPU: every test
// rebuilds the production pixels/table in memory and asserts a PROPERTY.
//
// The properties are chosen for defects that are invisible in a render:
//   * a blade map that does not TILE looks fine until a blade is tall enough
//     for the gloss lookup to run past u = 1 (GlossRepeat defaults to 4, so
//     that is every blade),
//   * a vein or ramp map centred on mid-grey rather than white halves the
//     brightness of every blade in the world, which reads as a lighting change,
//   * a table whose texture PATHS are empty loads, validates and draws — with
//     three unbound slots and no error anywhere.
//=============================================================================

#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TESTING

namespace
{
	// One channel of one texel, [0,1].
	float GrassTexelChannel(const Zenith_Vector<u_int8>& xPixels, int32_t iWidth,
		int32_t iX, int32_t iY, int32_t iChannel)
	{
		return xPixels.Get(((iY * iWidth) + iX) * 4 + iChannel) / 255.0f;
	}

	// The resolver Flux_GrassImpl injects is a device path; this one is the same
	// SHAPE (path -> slot) with no device behind it, which is exactly why
	// ResolveTextureIndices takes a function pointer instead of reaching for the
	// renderer.
	u_int GrassTestResolver(const std::string& strPath, FluxGrassTextureSlot eSlot, void* pUser)
	{
		u_int* puCalls = static_cast<u_int*>(pUser);
		if (puCalls != nullptr)
		{
			(*puCalls)++;
		}
		// A slot-derived index, so a test can tell the three apart.
		return strPath.empty() ? uFLUX_GRASS_BINDLESS_UNBOUND : (100u + static_cast<u_int>(eSlot));
	}
}

//-----------------------------------------------------------------------------
// The blade maps.
//-----------------------------------------------------------------------------

ZENITH_TEST(GrassAssets, BladeMapsTileAcrossTheWrap)
{
	// Both blade maps are sampled through a REPEAT sampler, and the gloss one is
	// deliberately indexed PAST u = 1 (height * GlossRepeat). A map that does
	// not wrap prints a hard seam down every blade in the world -- and looks
	// perfect in any single screenshot that does not straddle the seam.
	//
	// Adjacent-column tolerance, not equality: texel 0 and texel W-1 are
	// NEIGHBOURS across the wrap, not the same sample, so the test is that the
	// step across the seam is no bigger than a step anywhere else.
	Zenith_Vector<u_int8> xVein;
	GenerateGrassVeinPixels(xVein);

	float fMaxInteriorStep = 0.0f;
	for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY++)
	{
		for (int32_t iX = 1; iX < iBLADE_MAP_WIDTH; iX++)
		{
			const float fStep = fabsf(GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX, iY, 1) -
				GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX - 1, iY, 1));
			fMaxInteriorStep = std::max(fMaxInteriorStep, fStep);
		}
	}
	for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY++)
	{
		for (int32_t iC = 0; iC < 3; iC++)
		{
			const float fSeamStep = fabsf(
				GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, 0, iY, iC) -
				GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iBLADE_MAP_WIDTH - 1, iY, iC));
			ZENITH_ASSERT_LE(fSeamStep, std::max(fMaxInteriorStep, 0.02f),
				"the vein map's wrap seam is a bigger step than any interior step -- it does not tile");
		}
	}

	Zenith_Vector<u_int8> xGloss;
	GenerateGrassGlossPixels(xGloss);
	float fMaxGlossStep = 0.0f;
	for (int32_t iX = 1; iX < iBLADE_MAP_WIDTH; iX++)
	{
		fMaxGlossStep = std::max(fMaxGlossStep, fabsf(
			GrassTexelChannel(xGloss, iBLADE_MAP_WIDTH, iX, 0, 0) -
			GrassTexelChannel(xGloss, iBLADE_MAP_WIDTH, iX - 1, 0, 0)));
	}
	const float fGlossSeam = fabsf(
		GrassTexelChannel(xGloss, iBLADE_MAP_WIDTH, 0, 0, 0) -
		GrassTexelChannel(xGloss, iBLADE_MAP_WIDTH, iBLADE_MAP_WIDTH - 1, 0, 0));
	ZENITH_ASSERT_LE(fGlossSeam, std::max(fMaxGlossStep, 0.02f),
		"the gloss map's wrap seam is a bigger step than any interior step -- it does not tile, "
		"and GlossRepeat guarantees the lookup crosses it");
}

ZENITH_TEST(GrassAssets, VeinMapMultipliesRatherThanDarkens)
{
	// The fragment stage MULTIPLIES the blade's albedo by this map. Centred on
	// mid-grey it would halve the brightness of every blade in the world, which
	// reads as a lighting regression rather than as a texture bug.
	Zenith_Vector<u_int8> xVein;
	GenerateGrassVeinPixels(xVein);

	double dSum = 0.0;
	float fMin = 2.0f;
	const u_int uTexels = static_cast<u_int>(iBLADE_MAP_WIDTH * iBLADE_MAP_HEIGHT);
	for (u_int u = 0; u < uTexels; u++)
	{
		const float fG = xVein.Get(u * 4 + 1) / 255.0f;
		dSum += fG;
		fMin = std::min(fMin, fG);
	}
	const float fMean = static_cast<float>(dSum / uTexels);
	ZENITH_ASSERT_GT(fMean, 0.75f, "the vein map is dark enough to dim every blade that samples it");
	ZENITH_ASSERT_LT(fMean, 1.0f, "the vein map is pure white -- it carries no blade structure");
	ZENITH_ASSERT_GT(fMin, 0.50f, "the vein map's darkest texel would black out a blade");
}

ZENITH_TEST(GrassAssets, VeinMapHasABrightMidribAndDarkerFlanks)
{
	// The structure that makes a blade read as a blade. Sampled by POSITION,
	// which is safe here because the midrib is analytic (|u - 0.5|), not rolled.
	Zenith_Vector<u_int8> xVein;
	GenerateGrassVeinPixels(xVein);

	const int32_t iMid = iBLADE_MAP_WIDTH / 2;
	const int32_t iQuarter = iBLADE_MAP_WIDTH / 4;
	double dMidrib = 0.0;
	double dFlank = 0.0;
	for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY++)
	{
		dMidrib += GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iMid, iY, 1);
		dFlank += GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iQuarter, iY, 1);
	}
	ZENITH_ASSERT_GT(dMidrib, dFlank * 1.02f,
		"the midrib is not brighter than the flank -- the blade has no spine");

	// The two EDGES must match, or the blade is asymmetric about its own spine.
	for (int32_t iY = 0; iY < iBLADE_MAP_HEIGHT; iY += 8)
	{
		const float fLeft = GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, 1, iY, 1);
		const float fRight = GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iBLADE_MAP_WIDTH - 1, iY, 1);
		ZENITH_ASSERT_EQ_FLOAT(fLeft, fRight, 0.06f, "the blade is not symmetric about its midrib");
	}
}

ZENITH_TEST(GrassAssets, VeinTipsAreWarmerThanBases)
{
	// A drying tip is the cheapest cue that grass is a living thing rather than
	// a green ribbon, and it is a TINT: the red channel rises relative to blue.
	Zenith_Vector<u_int8> xVein;
	GenerateGrassVeinPixels(xVein);

	double dBaseWarmth = 0.0;
	double dTipWarmth = 0.0;
	for (int32_t iX = 0; iX < iBLADE_MAP_WIDTH; iX++)
	{
		dBaseWarmth += GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX, 0, 0) -
			GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX, 0, 2);
		dTipWarmth += GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX, iBLADE_MAP_HEIGHT - 1, 0) -
			GrassTexelChannel(xVein, iBLADE_MAP_WIDTH, iX, iBLADE_MAP_HEIGHT - 1, 2);
	}
	ZENITH_ASSERT_GT(dTipWarmth, dBaseWarmth,
		"blade tips are not warmer than blade bases -- the straw-tip tint is gone");
}

ZENITH_TEST(GrassAssets, GlossMapIsStreaksNotSheen)
{
	// The value is SUBTRACTED from roughness, so a map that is high everywhere
	// turns a lawn into polished plastic. It must be mostly matte with real
	// highlights in it -- both halves, or the test passes on a black map.
	Zenith_Vector<u_int8> xGloss;
	GenerateGrassGlossPixels(xGloss);

	double dSum = 0.0;
	u_int uBright = 0u;
	for (int32_t iX = 0; iX < iBLADE_MAP_WIDTH; iX++)
	{
		const float fV = GrassTexelChannel(xGloss, iBLADE_MAP_WIDTH, iX, 0, 0);
		ZENITH_ASSERT_GE(fV, 0.0f, "gloss below 0");
		ZENITH_ASSERT_LE(fV, 1.0f, "gloss above 1");
		dSum += fV;
		if (fV > 0.5f) { uBright++; }
	}
	const float fMean = static_cast<float>(dSum / iBLADE_MAP_WIDTH);
	ZENITH_ASSERT_LT(fMean, 0.45f, "the gloss map is bright almost everywhere -- blades will read as plastic");
	ZENITH_ASSERT_GT(uBright, 4u, "the gloss map has no highlights at all -- binding it changes nothing");

	// It is a 1D lookup (sampled at v = 0.5), so every column must be constant:
	// a vertical gradient would be invisible AND would smear into the R channel
	// through the mip chain.
	for (int32_t iX = 0; iX < iBLADE_MAP_WIDTH; iX += 17)
	{
		ZENITH_ASSERT_EQ(xGloss.Get((iX) * 4), xGloss.Get(((iBLADE_MAP_HEIGHT - 1) * iBLADE_MAP_WIDTH + iX) * 4),
			"the gloss map varies down a column -- it is a 1D lookup");
	}
}

ZENITH_TEST(GrassAssets, ClumpRampIsBrightAndVariesInBothAxes)
{
	Zenith_Vector<u_int8> xRamp;
	GenerateGrassRampPixels(xRamp);
	ZENITH_ASSERT_EQ(xRamp.GetSize(), static_cast<u_int>(iRAMP_SIZE * iRAMP_SIZE * 4), "ramp size");

	double dSum = 0.0;
	for (u_int u = 0; u < static_cast<u_int>(iRAMP_SIZE * iRAMP_SIZE); u++)
	{
		dSum += xRamp.Get(u * 4 + 1) / 255.0f;
	}
	const float fMean = static_cast<float>(dSum / (iRAMP_SIZE * iRAMP_SIZE));
	// Multiplied onto the type colour, like the vein map.
	ZENITH_ASSERT_GT(fMean, 0.70f, "the clump ramp would dim every blade that samples it");

	// v is base -> tip: the top of the ramp must be lighter than the bottom, or
	// every blade is uniformly lit along its length.
	double dBase = 0.0;
	double dTip = 0.0;
	for (int32_t iX = 0; iX < iRAMP_SIZE; iX++)
	{
		dBase += GrassTexelChannel(xRamp, iRAMP_SIZE, iX, 0, 1);
		dTip += GrassTexelChannel(xRamp, iRAMP_SIZE, iX, iRAMP_SIZE - 1, 1);
	}
	ZENITH_ASSERT_GT(dTip, dBase, "the ramp does not brighten from base to tip");

	// u is the clump hash: neighbouring clumps must actually differ, or the
	// whole field is one colour and the map is decoration.
	float fMinU = 2.0f;
	float fMaxU = -1.0f;
	for (int32_t iX = 0; iX < iRAMP_SIZE; iX++)
	{
		const float fWarmth = GrassTexelChannel(xRamp, iRAMP_SIZE, iX, iRAMP_SIZE / 2, 0) -
			GrassTexelChannel(xRamp, iRAMP_SIZE, iX, iRAMP_SIZE / 2, 2);
		fMinU = std::min(fMinU, fWarmth);
		fMaxU = std::max(fMaxU, fWarmth);
	}
	ZENITH_ASSERT_GT(fMaxU - fMinU, 0.08f,
		"every clump column is the same colour -- the ramp adds no per-tuft variation");
}

//-----------------------------------------------------------------------------
// The authored type table.
//-----------------------------------------------------------------------------

ZENITH_TEST(GrassAssets, TypeTableBindsEveryTextureSlotOnEveryType)
{
	// The failure this exists for: a table with EMPTY texture paths loads,
	// validates and draws grass. It just draws it with three unbound slots and
	// no error anywhere -- which is exactly the state every game was in before
	// this exporter existed.
	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);
	ZENITH_ASSERT_EQ(xTable.GetCount(), uGRASS_TYPE_COUNT, "authored type count");

	for (u_int u = 0; u < xTable.GetCount(); u++)
	{
		ZENITH_ASSERT_FALSE(xTable.GetName(u).empty(), "a live type has no name");
		for (u_int uSlot = 0; uSlot < FLUX_GRASS_TEXTURE_SLOT_COUNT; uSlot++)
		{
			ZENITH_ASSERT_FALSE(
				xTable.GetTexturePath(u, static_cast<FluxGrassTextureSlot>(uSlot)).empty(),
				"a live type leaves a texture slot unbound");
		}
	}

	// Straight off the builder nothing is RESOLVED yet: a bindless slot is a
	// per-boot descriptor allocation, so the authored side must read UNBOUND.
	ZENITH_ASSERT_EQ(xTable.CountBoundTextures(), 0u,
		"the authored table already carries bindless indices -- those are per-boot state");
}

ZENITH_TEST(GrassAssets, TypeTableResolvesEverySlotThroughTheInjectedResolver)
{
	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);

	u_int uCalls = 0u;
	xTable.ResolveTextureIndices(&GrassTestResolver, &uCalls);

	const u_int uExpected = uGRASS_TYPE_COUNT * FLUX_GRASS_TEXTURE_SLOT_COUNT;
	ZENITH_ASSERT_EQ(uCalls, uExpected, "the resolver was not called once per (live type, slot)");
	ZENITH_ASSERT_EQ(xTable.CountBoundTextures(), uExpected, "not every slot came back bound");

	// The right index reached the right slot -- a transposed assignment binds
	// the ramp as the vein map and produces plausible, wrong grass.
	for (u_int u = 0; u < xTable.GetCount(); u++)
	{
		const Flux_GrassTypeParams& xType = xTable.Get(u);
		ZENITH_ASSERT_EQ(xType.m_uVeinTextureIndex, 100u + FLUX_GRASS_TEXTURE_VEIN, "vein slot index");
		ZENITH_ASSERT_EQ(xType.m_uGlossTextureIndex, 100u + FLUX_GRASS_TEXTURE_GLOSS, "gloss slot index");
		ZENITH_ASSERT_EQ(xType.m_uRampTextureIndex, 100u + FLUX_GRASS_TEXTURE_RAMP, "ramp slot index");
	}
}

ZENITH_TEST(GrassAssets, TypeTableRoundTripsThroughTheTableLoader)
{
	// The same reader Flux_GrassImpl boot-loads through. A table that writes but
	// does not read back leaves every game on the built-ins, silently -- the
	// load path warns and keeps them.
	Flux_GrassTypeTable xSource;
	BuildGrassTypeTable(xSource);

	// Resolve first, on purpose: the writer must emit UNBOUND regardless, so a
	// resolved table and a fresh one serialize identically.
	u_int uCalls = 0u;
	xSource.ResolveTextureIndices(&GrassTestResolver, &uCalls);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_GrassTypeTable xLoaded;
	const bool bRead = xLoaded.ReadFromDataStream(xStream);
	ZENITH_ASSERT_TRUE(bRead, "the authored table did not read back through Flux_GrassTypeTable");
	ZENITH_ASSERT_EQ(xLoaded.GetCount(), uGRASS_TYPE_COUNT, "round-tripped type count");

	for (u_int u = 0; u < uGRASS_TYPE_COUNT; u++)
	{
		ZENITH_ASSERT_STREQ(xLoaded.GetName(u).c_str(), xSource.GetName(u).c_str(),
			"round-tripped type name");
		for (u_int uSlot = 0; uSlot < FLUX_GRASS_TEXTURE_SLOT_COUNT; uSlot++)
		{
			const FluxGrassTextureSlot eSlot = static_cast<FluxGrassTextureSlot>(uSlot);
			ZENITH_ASSERT_STREQ(xLoaded.GetTexturePath(u, eSlot).c_str(),
				xSource.GetTexturePath(u, eSlot).c_str(), "round-tripped texture path");
		}
		ZENITH_ASSERT_EQ_FLOAT(xLoaded.Get(u).m_fHeightMax, xSource.Get(u).m_fHeightMax, 0.0001f,
			"round-tripped height");
		ZENITH_ASSERT_EQ_FLOAT(xLoaded.Get(u).m_fRoughnessBase, xSource.Get(u).m_fRoughnessBase, 0.0001f,
			"round-tripped roughness");
	}

	// A descriptor slot from a previous boot must never survive the file.
	ZENITH_ASSERT_EQ(xLoaded.CountBoundTextures(), 0u,
		"bindless indices reached the file -- a slot number from another boot would be restored");
}

ZENITH_TEST(GrassAssets, DryStrawIsPalerRougherAndLessTranslucentThanMeadow)
{
	// The whole point of shipping more than one type. Each clause is a thing a
	// dead blade physically IS, and each is easy to lose while tuning colours.
	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);

	const Flux_GrassTypeParams& xMeadow = xTable.Get(0);
	const Flux_GrassTypeParams& xDry = xTable.Get(1);

	const float fMeadowLuma = xMeadow.m_xTipColour.x * 0.30f + xMeadow.m_xTipColour.y * 0.59f +
		xMeadow.m_xTipColour.z * 0.11f;
	const float fDryLuma = xDry.m_xTipColour.x * 0.30f + xDry.m_xTipColour.y * 0.59f +
		xDry.m_xTipColour.z * 0.11f;
	ZENITH_ASSERT_GT(fDryLuma, fMeadowLuma, "dry straw is not paler than meadow");
	// Straw is warm; a green-shifted "dry" type is just a second meadow.
	ZENITH_ASSERT_GT(xDry.m_xTipColour.x - xDry.m_xTipColour.z,
		xMeadow.m_xTipColour.x - xMeadow.m_xTipColour.z, "dry straw is not warmer than meadow");
	ZENITH_ASSERT_GT(xDry.m_fRoughnessBase, xMeadow.m_fRoughnessBase, "dry straw is not rougher than meadow");
	ZENITH_ASSERT_LT(xDry.m_fTranslucencyTip, xMeadow.m_fTranslucencyTip,
		"dry straw transmits as much light as living meadow");
}

ZENITH_TEST(GrassAssets, TheFourTypesAreActuallyDistinctSilhouettes)
{
	// Four rows that all draw the same blade are one type with four names.
	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);

	const Flux_GrassTypeParams& xMeadow = xTable.Get(0);
	const Flux_GrassTypeParams& xTrampled = xTable.Get(2);
	const Flux_GrassTypeParams& xTussock = xTable.Get(3);

	ZENITH_ASSERT_LT(xTrampled.m_fHeightMax, xMeadow.m_fHeightMin,
		"trampled grass is not shorter than every meadow blade");
	ZENITH_ASSERT_GT(xTrampled.m_fTiltMinRad, xMeadow.m_fTiltMinRad,
		"trampled grass does not lean more than meadow");
	ZENITH_ASSERT_GT(xTussock.m_fHeightMax, xMeadow.m_fHeightMax * 1.5f,
		"the tussock is not meaningfully taller than meadow");
	ZENITH_ASSERT_LT(xTussock.m_fDensity, xMeadow.m_fDensity,
		"the tussock is as dense as meadow -- it will read as a taller lawn, not as tufts");
	ZENITH_ASSERT_GT(xTussock.m_fWidthMax, xMeadow.m_fWidthMax,
		"the tussock's blades are not coarser than meadow's");

	// Distinct names, so an editor row and a log line can tell them apart.
	for (u_int u = 0; u < xTable.GetCount(); u++)
	{
		for (u_int v = u + 1u; v < xTable.GetCount(); v++)
		{
			ZENITH_ASSERT_FALSE(xTable.GetName(u) == xTable.GetName(v), "two types share a name");
		}
	}
}

ZENITH_TEST(GrassAssets, TypeTableSurvivesValidateUnchanged)
{
	// BuildGrassTypeTable validates on the way out, so this is the IDEMPOTENCE
	// clause: a second Validate() must be a no-op. It fails when a clamp is not
	// a fixed point -- an OrderPair that swaps a pair back, say -- which would
	// mean the table on disk is not the table the loader ends up with, since
	// the loader validates again on read.
	Flux_GrassTypeTable xTable;
	BuildGrassTypeTable(xTable);

	Flux_GrassTypeTable xRevalidated = xTable;
	xRevalidated.Validate();

	for (u_int u = 0; u < xTable.GetCount(); u++)
	{
		for (u_int uParam = 0; uParam < Flux_GrassTypeParams::GetFloatParamCount(); uParam++)
		{
			const char* szName = Flux_GrassTypeParams::GetFloatParamName(uParam);
			float fBefore = 0.0f;
			float fAfter = 0.0f;
			ZENITH_ASSERT_TRUE(xTable.Get(u).GetFloatParamByName(szName, fBefore), "unknown param name");
			ZENITH_ASSERT_TRUE(xRevalidated.Get(u).GetFloatParamByName(szName, fAfter), "unknown param name");
			ZENITH_ASSERT_EQ_FLOAT(fAfter, fBefore, 0.0f,
				"Validate() moved an authored grass parameter -- it is outside its legal range");
		}
	}
}

//-----------------------------------------------------------------------------
// Determinism — the standing mandate for every generated set.
//-----------------------------------------------------------------------------

ZENITH_TEST(GrassAssets, GenerationIsDeterministic)
{
	Zenith_Vector<u_int8> xA;
	Zenith_Vector<u_int8> xB;

	GenerateGrassVeinPixels(xA);
	GenerateGrassVeinPixels(xB);
	ZENITH_ASSERT_EQ(xA.GetSize(), xB.GetSize(), "vein map size differs between runs");
	for (u_int u = 0; u < xA.GetSize(); u++)
	{
		if (xA.Get(u) != xB.Get(u))
		{
			ZENITH_ASSERT_EQ(xA.Get(u), xB.Get(u), "the vein map is not a pure function of its seed");
			break;
		}
	}

	GenerateGrassRampPixels(xA);
	GenerateGrassRampPixels(xB);
	for (u_int u = 0; u < xA.GetSize(); u++)
	{
		if (xA.Get(u) != xB.Get(u))
		{
			ZENITH_ASSERT_EQ(xA.Get(u), xB.Get(u), "the clump ramp is not a pure function of its seed");
			break;
		}
	}

	// The table too: two builds must serialize to identical bytes, or a boot
	// that re-bakes it churns the file for no reason.
	Flux_GrassTypeTable xTableA;
	Flux_GrassTypeTable xTableB;
	BuildGrassTypeTable(xTableA);
	BuildGrassTypeTable(xTableB);
	Zenith_DataStream xStreamA;
	Zenith_DataStream xStreamB;
	xTableA.WriteToDataStream(xStreamA);
	xTableB.WriteToDataStream(xStreamB);
	ZENITH_ASSERT_EQ(xStreamA.GetCursor(), xStreamB.GetCursor(),
		"two builds of the authored table serialize to different sizes");
}

#endif // ZENITH_TESTING
