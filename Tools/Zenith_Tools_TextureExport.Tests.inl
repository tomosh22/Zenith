//=============================================================================
// Zenith_Tools_TextureExport unit tests. Included from the bottom of
// Zenith_Tools_TextureExport.cpp so the file-static mip builder and the BC1
// encoders are in scope and exercised exactly as ExportV2 calls them.
//
// Each test targets a defect that is INVISIBLE in a render: a colour space
// that resolves to the UNORM twin lights every albedo ~2.2x too bright and
// looks like "a bit washed out"; a byte-averaged sRGB mip is merely "a bit
// dark at distance"; a leaf mask whose coverage decays per mip is "the trees
// look thin far away"; and BC1 punch-through that was never encoded is "the
// cutout has square corners". None of those fail a gate. These do.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef ZENITH_TESTING

namespace
{
	// Decode one BC1 block back to 16 RGBA texels, following the D3D/Vulkan
	// reconstruction: colour0 > colour1 selects the 4-colour mode (two
	// interpolants), colour0 <= colour1 the 3-colour mode (one midpoint, index 3
	// = transparent black).
	void TextureExportTest_DecodeBC1Block(const uint8_t* pBlock, uint8_t* pRGBAOut, bool& bThreeColourModeOut)
	{
		const uint16_t uC0 = static_cast<uint16_t>(pBlock[0] | (pBlock[1] << 8));
		const uint16_t uC1 = static_cast<uint16_t>(pBlock[2] | (pBlock[3] << 8));
		const uint32_t uMask = pBlock[4] | (pBlock[5] << 8) | (pBlock[6] << 16) | (static_cast<uint32_t>(pBlock[7]) << 24);
		bThreeColourModeOut = uC0 <= uC1;

		uint8_t auPalette[4][4] = {};
		Expand565(uC0, auPalette[0]);
		Expand565(uC1, auPalette[1]);
		auPalette[0][3] = auPalette[1][3] = 255;
		for (int32_t c = 0; c < 3; c++)
		{
			if (bThreeColourModeOut)
			{
				auPalette[2][c] = static_cast<uint8_t>((auPalette[0][c] + auPalette[1][c]) / 2);
				auPalette[3][c] = 0;
			}
			else
			{
				auPalette[2][c] = static_cast<uint8_t>((2 * auPalette[0][c] + auPalette[1][c]) / 3);
				auPalette[3][c] = static_cast<uint8_t>((auPalette[0][c] + 2 * auPalette[1][c]) / 3);
			}
		}
		auPalette[2][3] = 255;
		auPalette[3][3] = bThreeColourModeOut ? 0 : 255;

		for (int32_t i = 0; i < 16; i++)
		{
			const uint32_t uIndex = (uMask >> (2 * i)) & 3u;
			for (int32_t c = 0; c < 4; c++)
			{
				pRGBAOut[i * 4 + c] = auPalette[uIndex][c];
			}
		}
	}

	// A 64x64 RGBA8 "leaf sheet": discs of VARYING radius on an 8px grid with a
	// 3px SOFT alpha edge, the shape the tree/bush generators paint (a smooth
	// falloff over the outer part of each leaf). The soft edge is what makes a
	// box filter drift: averaged edge texels climb above the cutoff and the
	// unscaled chain OVER-covers by level 2. Varying radii matter too -- at mip
	// 3 every texel covers exactly one grid cell, and a uniform-alpha level
	// could not be scaled to any coverage but 0 or 1.
	void TextureExportTest_BuildLeafSheet(std::vector<uint8_t>& xRGBA, int32_t& iSize)
	{
		iSize = 64;
		xRGBA.assign(static_cast<size_t>(iSize) * iSize * 4, 0);
		for (int32_t y = 0; y < iSize; y++)
		{
			for (int32_t x = 0; x < iSize; x++)
			{
				const int32_t iCellX = x / 8, iCellY = y / 8;
				const int32_t iCell = iCellY * 8 + iCellX;
				const float fRadius = 1.5f + 2.0f * static_cast<float>((iCell * 7) % 8) / 7.0f;
				const float fSoft = 3.0f;
				const float fDX = static_cast<float>(x - (iCellX * 8 + 4)) + 0.5f;
				const float fDY = static_cast<float>(y - (iCellY * 8 + 4)) + 0.5f;
				const float fDist = std::sqrt(fDX * fDX + fDY * fDY);
				const float fAlpha = std::clamp((fRadius + fSoft - fDist) / fSoft, 0.0f, 1.0f);
				uint8_t* pTexel = &xRGBA[(static_cast<size_t>(y) * iSize + x) * 4];
				pTexel[0] = 40;
				pTexel[1] = 120;
				pTexel[2] = 30;
				pTexel[3] = static_cast<uint8_t>(fAlpha * 255.0f + 0.5f);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// (compression, colour space) -> on-disk format
//-----------------------------------------------------------------------------

ZENITH_TEST(TextureExport, ResolveExportFormatPicksTheSRGBTwinForColour)
{
	using Zenith_Tools_TextureExport::ResolveExportFormat;
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC1, TextureColourSpace::SRGB), TEXTURE_FORMAT_BC1_RGB_SRGB);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC1_Alpha, TextureColourSpace::SRGB), TEXTURE_FORMAT_BC1_RGBA_SRGB);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC3, TextureColourSpace::SRGB), TEXTURE_FORMAT_BC3_RGBA_SRGB);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::Uncompressed, TextureColourSpace::SRGB), TEXTURE_FORMAT_RGBA8_SRGB);
}

ZENITH_TEST(TextureExport, ResolveExportFormatKeepsDataOnUNORM)
{
	using Zenith_Tools_TextureExport::ResolveExportFormat;
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC1, TextureColourSpace::Linear), TEXTURE_FORMAT_BC1_RGB_UNORM);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC1_Alpha, TextureColourSpace::Linear), TEXTURE_FORMAT_BC1_RGBA_UNORM);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC3, TextureColourSpace::Linear), TEXTURE_FORMAT_BC3_RGBA_UNORM);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::Uncompressed, TextureColourSpace::Linear), TEXTURE_FORMAT_RGBA8_UNORM);
	// BC5 is two-channel data with no sRGB twin: the colour space is ignored, and
	// a caller that asks for an sRGB normal map gets a correct one anyway.
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC5, TextureColourSpace::Linear), TEXTURE_FORMAT_BC5_RG_UNORM);
	ZENITH_ASSERT_EQ(ResolveExportFormat(TextureCompressionMode::BC5, TextureColourSpace::SRGB), TEXTURE_FORMAT_BC5_RG_UNORM);
}

//-----------------------------------------------------------------------------
// Declared texture usage (the .ztexdecl manifest)
//
// These replace five tests that pinned a FILENAME HEURISTIC ("normal" in the
// basename -> BC5). The heuristic was correct on every one of those cases and
// still broke the game: it silently changed the terrain normal maps to BC5 while
// the terrain shaders still decoded three channels. A test can only pin what a
// function was asked to do, and the guess was never asked whether its consumer
// agreed -- so the guess is gone and the answer is declared.
//-----------------------------------------------------------------------------

ZENITH_TEST(TextureExport, EveryUsageResolvesToItsDeclaredPair)
{
	using Zenith_Tools_TextureExport::TextureUsage;
	using Zenith_Tools_TextureExport::ResolveUsage;
	TextureCompressionMode eCompression;
	TextureColourSpace eColourSpace;

	ResolveUsage(TextureUsage::BaseColour, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::BC1);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::SRGB);

	ResolveUsage(TextureUsage::BaseColourMasked, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::BC1_Alpha);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::SRGB);

	// The one that matters: a normal map is two-channel LINEAR data. The shader
	// side reconstructs Z; Common/Material.slang's SampleNormalMap and the two
	// terrain G-buffer shaders share that decode.
	ResolveUsage(TextureUsage::NormalMap, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::BC5);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::Linear);

	ResolveUsage(TextureUsage::LinearData, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::BC1);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::Linear);

	ResolveUsage(TextureUsage::UncompressedColour, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::Uncompressed);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::SRGB);

	ResolveUsage(TextureUsage::UncompressedData, eCompression, eColourSpace);
	ZENITH_ASSERT_TRUE(eCompression == TextureCompressionMode::Uncompressed);
	ZENITH_ASSERT_TRUE(eColourSpace == TextureColourSpace::Linear);
}

ZENITH_TEST(TextureExport, EveryUsageTokenRoundTrips)
{
	using Zenith_Tools_TextureExport::TextureUsage;
	// A usage with no spelling cannot be declared, so it would be unreachable
	// from a manifest -- which is the same silent hole by another route.
	const TextureUsage aeALL[] =
	{
		TextureUsage::BaseColour, TextureUsage::BaseColourMasked, TextureUsage::NormalMap,
		TextureUsage::LinearData, TextureUsage::UncompressedColour, TextureUsage::UncompressedData,
	};
	for (TextureUsage eUsage : aeALL)
	{
		const char* szToken = Zenith_Tools_TextureExport::TextureUsageToken(eUsage);
		ZENITH_ASSERT_TRUE(std::strcmp(szToken, "?") != 0, "usage has no token");
		TextureUsage eBack;
		ZENITH_ASSERT_TRUE(Zenith_Tools_TextureExport::TextureUsageFromToken(szToken, eBack), "%s did not parse", szToken);
		ZENITH_ASSERT_TRUE(eBack == eUsage, "%s round-tripped to a different usage", szToken);
	}
}

ZENITH_TEST(TextureExport, ManifestParsesPathsCommentsAndBlankLines)
{
	Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
	const std::string strText =
		"# a comment\n"
		"\n"
		"Textures/Terrain/Grass/diffuse.jpg   BASE_COLOUR\n"
		"Textures/Terrain/Grass/normal.jpg    NORMAL_MAP\n"
		"   # indented comment\n"
		"Textures/Terrain/Grass/roughness.jpg LINEAR_DATA\n";
	ZENITH_ASSERT_TRUE(xManifest.ParseText(strText), "%s", xManifest.GetError().c_str());
	ZENITH_ASSERT_EQ(xManifest.GetCount(), size_t(3));

	Zenith_Tools_TextureExport::TextureUsage eUsage;
	ZENITH_ASSERT_TRUE(xManifest.TryGetUsage("Textures/Terrain/Grass/normal.jpg", eUsage));
	ZENITH_ASSERT_TRUE(eUsage == Zenith_Tools_TextureExport::TextureUsage::NormalMap);
	ZENITH_ASSERT_TRUE(xManifest.TryGetUsage("Textures/Terrain/Grass/diffuse.jpg", eUsage));
	ZENITH_ASSERT_TRUE(eUsage == Zenith_Tools_TextureExport::TextureUsage::BaseColour);
}

ZENITH_TEST(TextureExport, ManifestLookupIsCaseAndSeparatorInsensitive)
{
	// A Windows source tree is not case-stable and the walk yields backslashes;
	// a declaration that stops matching after a rename is the failure this file
	// exists to remove.
	Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
	ZENITH_ASSERT_TRUE(xManifest.ParseText("Textures/Terrain/Rock/Normal.JPG NORMAL_MAP\n"));
	Zenith_Tools_TextureExport::TextureUsage eUsage;
	ZENITH_ASSERT_TRUE(xManifest.TryGetUsage("textures/terrain/rock/normal.jpg", eUsage));
	ZENITH_ASSERT_TRUE(xManifest.TryGetUsage("Textures\\Terrain\\Rock\\normal.jpg", eUsage));
	ZENITH_ASSERT_TRUE(eUsage == Zenith_Tools_TextureExport::TextureUsage::NormalMap);
}

ZENITH_TEST(TextureExport, ManifestUndeclaredPathIsNotGuessed)
{
	// The whole point: no fallback. An unlisted texture comes back "no", and the
	// walk reports it by name instead of picking a plausible encoding.
	Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
	ZENITH_ASSERT_TRUE(xManifest.ParseText("Textures/Terrain/Rock/normal.jpg NORMAL_MAP\n"));
	Zenith_Tools_TextureExport::TextureUsage eUsage;
	ZENITH_ASSERT_FALSE(xManifest.TryGetUsage("Textures/Terrain/Rock/diffuse.jpg", eUsage));
	// ...including for a name the old heuristic would have been confident about.
	ZENITH_ASSERT_FALSE(xManifest.TryGetUsage("Textures/Terrain/Rock/some_normal_map.png", eUsage));
}

ZENITH_TEST(TextureExport, ManifestRejectsMalformedLinesRatherThanSkippingThem)
{
	Zenith_Tools_TextureExport::TextureUsage eUsage;
	{
		Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
		ZENITH_ASSERT_FALSE(xManifest.ParseText("Textures/a.png SRGB_MAYBE\n"));
		ZENITH_ASSERT_TRUE(!xManifest.GetError().empty());
	}
	{
		Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
		ZENITH_ASSERT_FALSE(xManifest.ParseText("Textures/a.png\n"), "a path with no usage token must fail");
	}
	{
		// A duplicate is an edit conflict, and silently keeping either one gives
		// two developers different .ztxtr from the same tree.
		Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
		ZENITH_ASSERT_FALSE(xManifest.ParseText("Textures/a.png BASE_COLOUR\nTextures/A.PNG NORMAL_MAP\n"));
	}
	{
		// CRLF must parse identically to LF: the manifest is committed and edited
		// on Windows, and a trailing CR would attach itself to the usage token.
		Zenith_Tools_TextureExport::TextureUsageManifest xManifest;
		ZENITH_ASSERT_TRUE(xManifest.ParseText("Textures/a.png BASE_COLOUR\r\n"), "%s", xManifest.GetError().c_str());
		ZENITH_ASSERT_TRUE(xManifest.TryGetUsage("Textures/a.png", eUsage));
		ZENITH_ASSERT_TRUE(eUsage == Zenith_Tools_TextureExport::TextureUsage::BaseColour);
	}
}

//-----------------------------------------------------------------------------
// Mip filtering
//-----------------------------------------------------------------------------

ZENITH_TEST(TextureExport, SRGBDownsampleAveragesInLinear)
{
	// A 2x2 checker of sRGB black and sRGB white is 50% linear grey, which encodes
	// to sRGB ~188 (0.5^(1/2.4)*1.055-0.055 = 0.735). The byte average, 128, is
	// linear 0.216 -- less than half as bright -- which is the "mips go dark"
	// defect this filter replaces.
	const uint8_t auSrc[2 * 2 * 4] =
	{
		0, 0, 0, 255,      255, 255, 255, 255,
		255, 255, 255, 255,  0, 0, 0, 255,
	};
	std::vector<uint8_t> xDst;
	int32_t iW = 0, iH = 0;
	DownsampleBoxRGBA8(auSrc, 2, 2, xDst, iW, iH, /*bSRGB*/ true);
	ZENITH_ASSERT_EQ(iW, 1);
	ZENITH_ASSERT_EQ(iH, 1);
	ZENITH_ASSERT_EQ(xDst.size(), static_cast<size_t>(4));
	for (int32_t c = 0; c < 3; c++)
	{
		ZENITH_ASSERT_GE(static_cast<int32_t>(xDst[c]), 186, "sRGB-correct average must land near 188, channel %d", c);
		ZENITH_ASSERT_LE(static_cast<int32_t>(xDst[c]), 189, "sRGB-correct average must land near 188, channel %d", c);
	}
	// Alpha is linear coverage and is averaged as bytes even on an sRGB image.
	ZENITH_ASSERT_EQ(static_cast<int32_t>(xDst[3]), 255);
}

ZENITH_TEST(TextureExport, LinearDownsampleAveragesBytes)
{
	const uint8_t auSrc[2 * 2 * 4] =
	{
		0, 0, 0, 0,          255, 255, 255, 255,
		255, 255, 255, 255,  0, 0, 0, 0,
	};
	std::vector<uint8_t> xDst;
	int32_t iW = 0, iH = 0;
	DownsampleBoxRGBA8(auSrc, 2, 2, xDst, iW, iH, /*bSRGB*/ false);
	for (int32_t c = 0; c < 4; c++)
	{
		ZENITH_ASSERT_EQ(static_cast<int32_t>(xDst[c]), 128, "linear data averages in place, channel %d", c);
	}
}

ZENITH_TEST(TextureExport, SRGBDownsampleIsExactOnFlatColour)
{
	// Decode -> average -> re-encode must be the identity on a flat block; a
	// drift here accumulates across a 12-level chain.
	for (int32_t iV = 0; iV < 256; iV += 5)
	{
		uint8_t auSrc[2 * 2 * 4];
		for (int32_t i = 0; i < 4; i++)
		{
			auSrc[i * 4 + 0] = auSrc[i * 4 + 1] = auSrc[i * 4 + 2] = static_cast<uint8_t>(iV);
			auSrc[i * 4 + 3] = 255;
		}
		std::vector<uint8_t> xDst;
		int32_t iW = 0, iH = 0;
		DownsampleBoxRGBA8(auSrc, 2, 2, xDst, iW, iH, /*bSRGB*/ true);
		ZENITH_ASSERT_EQ(static_cast<int32_t>(xDst[0]), iV, "flat sRGB %d must survive a downsample", iV);
	}
}

ZENITH_TEST(TextureExport, MipChainHasFloorLog2PlusOneLevelsDownToOneByOne)
{
	std::vector<uint8_t> xSrc(static_cast<size_t>(5) * 3 * 4, 200);
	std::vector<std::vector<uint8_t>> xMips;
	std::vector<int32_t> aiW, aiH;
	BuildMipChainRGBA8(xSrc.data(), 5, 3, /*bSRGB*/ false, /*fCutoff*/ 0.0f, xMips, aiW, aiH);
	ZENITH_ASSERT_EQ(xMips.size(), static_cast<size_t>(3));   // floor(log2(5)) + 1
	ZENITH_ASSERT_EQ(aiW[0], 5); ZENITH_ASSERT_EQ(aiH[0], 3);
	ZENITH_ASSERT_EQ(aiW[1], 2); ZENITH_ASSERT_EQ(aiH[1], 1);
	ZENITH_ASSERT_EQ(aiW[2], 1); ZENITH_ASSERT_EQ(aiH[2], 1);
	ZENITH_ASSERT_EQ(xMips[2].size(), static_cast<size_t>(4));
	ZENITH_ASSERT_EQ(static_cast<int32_t>(xMips[2][0]), 200);
}

//-----------------------------------------------------------------------------
// Coverage-preserving alpha mips
//-----------------------------------------------------------------------------

ZENITH_TEST(TextureExport, PlainBoxFilterLosesMaskCoverage)
{
	// The defect the option exists for: WITHOUT coverage preservation the leaf
	// sheet's coverage at the material cutoff has drifted by more than 20% of
	// the image by level 2 (the soft edges average ABOVE the cutoff here; a
	// sparser sheet drifts the other way -- either is a different silhouette at
	// distance). If this stops failing, the preserving test below is no longer
	// discriminating.
	std::vector<uint8_t> xSheet;
	int32_t iSize = 0;
	TextureExportTest_BuildLeafSheet(xSheet, iSize);
	const float fCutoff = 0.45f;
	const float fCoverage0 = AlphaCoverage(xSheet.data(), static_cast<size_t>(iSize) * iSize, fCutoff, 1.0f);
	ZENITH_ASSERT_GT(fCoverage0, 0.50f);
	ZENITH_ASSERT_LT(fCoverage0, 0.95f);

	std::vector<std::vector<uint8_t>> xMips;
	std::vector<int32_t> aiW, aiH;
	BuildMipChainRGBA8(xSheet.data(), iSize, iSize, /*bSRGB*/ true, /*fCutoff*/ 0.0f, xMips, aiW, aiH);
	const float fCoverage2 = AlphaCoverage(xMips[2].data(), static_cast<size_t>(aiW[2]) * aiH[2], fCutoff, 1.0f);
	ZENITH_ASSERT_GT(std::fabs(fCoverage2 - fCoverage0), 0.10f,
		"an unscaled box filter must visibly change coverage (got %.3f vs %.3f)", fCoverage2, fCoverage0);
}

ZENITH_TEST(TextureExport, CoveragePreservingMipsHoldTheMip0Fraction)
{
	std::vector<uint8_t> xSheet;
	int32_t iSize = 0;
	TextureExportTest_BuildLeafSheet(xSheet, iSize);
	const float fCutoff = 0.45f;
	const float fCoverage0 = AlphaCoverage(xSheet.data(), static_cast<size_t>(iSize) * iSize, fCutoff, 1.0f);

	std::vector<std::vector<uint8_t>> xMips;
	std::vector<int32_t> aiW, aiH;
	BuildMipChainRGBA8(xSheet.data(), iSize, iSize, /*bSRGB*/ true, fCutoff, xMips, aiW, aiH);
	ZENITH_ASSERT_EQ(xMips.size(), static_cast<size_t>(7));

	// Mips 1..3 have 1024, 256 and 64 texels: the coverage granularity at mip 3
	// is 1/64, so 0.05 is a real tolerance, not a loose one.
	for (uint32_t m = 1; m <= 3; m++)
	{
		const float fCoverage = AlphaCoverage(xMips[m].data(), static_cast<size_t>(aiW[m]) * aiH[m], fCutoff, 1.0f);
		ZENITH_ASSERT_LE(std::fabs(fCoverage - fCoverage0), 0.05f,
			"mip %u coverage %.3f must track mip 0 coverage %.3f", m, fCoverage, fCoverage0);
	}
	// Mip 0 is untouched: coverage scaling is for the minified levels only.
	ZENITH_ASSERT_TRUE(xMips[0] == xSheet);
}

ZENITH_TEST(TextureExport, CoveragePreservationLeavesColourAlone)
{
	// Only alpha is scaled; the RGB of every level must equal the chain built
	// without the option.
	std::vector<uint8_t> xSheet;
	int32_t iSize = 0;
	TextureExportTest_BuildLeafSheet(xSheet, iSize);

	std::vector<std::vector<uint8_t>> xPlain, xScaled;
	std::vector<int32_t> aiW, aiH, aiW2, aiH2;
	BuildMipChainRGBA8(xSheet.data(), iSize, iSize, true, 0.0f, xPlain, aiW, aiH);
	BuildMipChainRGBA8(xSheet.data(), iSize, iSize, true, 0.45f, xScaled, aiW2, aiH2);
	for (size_t m = 0; m < xPlain.size(); m++)
	{
		ZENITH_ASSERT_EQ(xPlain[m].size(), xScaled[m].size());
		for (size_t i = 0; i + 3 < xPlain[m].size(); i += 4)
		{
			ZENITH_ASSERT_TRUE(xPlain[m][i + 0] == xScaled[m][i + 0] && xPlain[m][i + 1] == xScaled[m][i + 1] && xPlain[m][i + 2] == xScaled[m][i + 2],
				"RGB must be untouched by coverage scaling (mip %zu texel %zu)", m, i / 4);
		}
	}
}

//-----------------------------------------------------------------------------
// BC1 punch-through alpha
//-----------------------------------------------------------------------------

ZENITH_TEST(TextureExport, BC1PunchThroughEncodesTransparentTexels)
{
	// Left half opaque red, right half transparent. This used to be encoded with
	// stb_compress_dxt_block(..., bHasAlpha ? 0 : 0, ...) -- the alpha argument
	// was always 0, every texel came out opaque, and BC1_RGBA was BC1_RGB with a
	// misleading name.
	uint8_t auBlock[16 * 4];
	for (int32_t y = 0; y < 4; y++)
	{
		for (int32_t x = 0; x < 4; x++)
		{
			uint8_t* pTexel = &auBlock[(y * 4 + x) * 4];
			const bool bOpaque = x < 2;
			pTexel[0] = bOpaque ? 255 : 0;
			pTexel[1] = 0;
			pTexel[2] = bOpaque ? 0 : 255;
			pTexel[3] = bOpaque ? 255 : 0;
		}
	}
	uint8_t auOut[8] = {};
	CompressToBC1(auBlock, auOut, 4, 4, /*bPunchThroughAlpha*/ true);

	uint8_t auDecoded[16 * 4];
	bool bThreeColour = false;
	TextureExportTest_DecodeBC1Block(auOut, auDecoded, bThreeColour);
	ZENITH_ASSERT_TRUE(bThreeColour, "a block with transparent texels must use the 3-colour mode (colour0 <= colour1)");
	for (int32_t y = 0; y < 4; y++)
	{
		for (int32_t x = 0; x < 4; x++)
		{
			const uint8_t* pTexel = &auDecoded[(y * 4 + x) * 4];
			if (x < 2)
			{
				ZENITH_ASSERT_EQ(static_cast<int32_t>(pTexel[3]), 255, "opaque texel (%d,%d)", x, y);
				ZENITH_ASSERT_GE(static_cast<int32_t>(pTexel[0]), 240, "opaque texel (%d,%d) must stay red", x, y);
				ZENITH_ASSERT_LE(static_cast<int32_t>(pTexel[2]), 16, "opaque texel (%d,%d) must not pick up the transparent blue", x, y);
			}
			else
			{
				ZENITH_ASSERT_EQ(static_cast<int32_t>(pTexel[3]), 0, "transparent texel (%d,%d)", x, y);
			}
		}
	}
}

ZENITH_TEST(TextureExport, BC1PunchThroughKeepsOpaqueBlocksInFourColourMode)
{
	// Punch-through costs a palette entry, so it is used only where a block
	// actually has a transparent texel. A fully opaque two-colour block must
	// still get the 4-colour mode whether or not alpha is requested.
	uint8_t auBlock[16 * 4];
	for (int32_t i = 0; i < 16; i++)
	{
		const bool bA = (i % 4) < 2;
		auBlock[i * 4 + 0] = bA ? 255 : 0;
		auBlock[i * 4 + 1] = bA ? 0 : 255;
		auBlock[i * 4 + 2] = 0;
		auBlock[i * 4 + 3] = 255;
	}
	uint8_t auWithAlpha[8] = {}, auWithout[8] = {};
	CompressToBC1(auBlock, auWithAlpha, 4, 4, true);
	CompressToBC1(auBlock, auWithout, 4, 4, false);
	uint8_t auDecoded[16 * 4];
	bool bThreeColour = true;
	TextureExportTest_DecodeBC1Block(auWithAlpha, auDecoded, bThreeColour);
	ZENITH_ASSERT_FALSE(bThreeColour, "an opaque block must not spend a palette entry on transparency");
	ZENITH_ASSERT_TRUE(memcmp(auWithAlpha, auWithout, 8) == 0, "opaque blocks encode identically with and without punch-through");
	for (int32_t i = 0; i < 16; i++)
	{
		ZENITH_ASSERT_EQ(static_cast<int32_t>(auDecoded[i * 4 + 3]), 255);
	}
}

ZENITH_TEST(TextureExport, BC1PunchThroughFullyTransparentBlock)
{
	uint8_t auBlock[16 * 4];
	for (int32_t i = 0; i < 16; i++)
	{
		auBlock[i * 4 + 0] = 90; auBlock[i * 4 + 1] = 200; auBlock[i * 4 + 2] = 30; auBlock[i * 4 + 3] = 0;
	}
	uint8_t auOut[8] = {};
	CompressToBC1(auBlock, auOut, 4, 4, true);
	uint8_t auDecoded[16 * 4];
	bool bThreeColour = false;
	TextureExportTest_DecodeBC1Block(auOut, auDecoded, bThreeColour);
	ZENITH_ASSERT_TRUE(bThreeColour);
	for (int32_t i = 0; i < 16; i++)
	{
		ZENITH_ASSERT_EQ(static_cast<int32_t>(auDecoded[i * 4 + 3]), 0, "texel %d must be transparent", i);
	}
}

ZENITH_TEST(TextureExport, BC1PunchThroughThresholdIsHalfAlpha)
{
	// BC1 alpha is one bit: >= 128 is opaque, below is transparent -- the same
	// rule a MASKED material with cutoff 0.5 applies, so the encoded cutout and
	// the shader's agree.
	uint8_t auBlock[16 * 4];
	for (int32_t i = 0; i < 16; i++)
	{
		auBlock[i * 4 + 0] = 200; auBlock[i * 4 + 1] = 200; auBlock[i * 4 + 2] = 200;
		auBlock[i * 4 + 3] = static_cast<uint8_t>(i * 17);   // 0, 17, ... 255: crosses 128 between i=7 (119) and i=8 (136)
	}
	uint8_t auOut[8] = {};
	CompressToBC1(auBlock, auOut, 4, 4, true);
	uint8_t auDecoded[16 * 4];
	bool bThreeColour = false;
	TextureExportTest_DecodeBC1Block(auOut, auDecoded, bThreeColour);
	ZENITH_ASSERT_TRUE(bThreeColour);
	for (int32_t i = 0; i < 16; i++)
	{
		const int32_t iExpected = (i * 17 >= 128) ? 255 : 0;
		ZENITH_ASSERT_EQ(static_cast<int32_t>(auDecoded[i * 4 + 3]), iExpected, "texel %d alpha %d", i, i * 17);
	}
}

#endif // ZENITH_TESTING
