//=============================================================================
// Flux_Types unit tests. Included from the bottom of Flux_Types.cpp so the
// format helpers are exercised exactly as the loader, the exporter and the GPU
// upload call them.
//
// The point of these is the sRGB BC twins: every helper that switches on a BC
// format used to enumerate the UNORM blocks only, and a format that falls
// through such a switch does not fail loudly -- IsCompressedFormat returns
// false, the byte-size math treats the block payload as pixels, and the upload
// asserts (or reads past the buffer) somewhere far from the cause.
//=============================================================================

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

ZENITH_TEST(FluxTypes, SRGBBlockFormatsAreCompressed)
{
	ZENITH_ASSERT_TRUE(IsCompressedFormat(TEXTURE_FORMAT_BC1_RGB_SRGB));
	ZENITH_ASSERT_TRUE(IsCompressedFormat(TEXTURE_FORMAT_BC1_RGBA_SRGB));
	ZENITH_ASSERT_TRUE(IsCompressedFormat(TEXTURE_FORMAT_BC3_RGBA_SRGB));
	ZENITH_ASSERT_TRUE(IsCompressedFormat(TEXTURE_FORMAT_BC7_RGBA_SRGB));
	// The uncompressed sRGB formats are NOT block formats.
	ZENITH_ASSERT_FALSE(IsCompressedFormat(TEXTURE_FORMAT_RGBA8_SRGB));
	ZENITH_ASSERT_FALSE(IsCompressedFormat(TEXTURE_FORMAT_BGRA8_SRGB));
}

ZENITH_TEST(FluxTypes, IsSRGBFormatDetectsEveryEOTFDecodedFormat)
{
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_RGBA8_SRGB));
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_BGRA8_SRGB));
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_BC1_RGB_SRGB));
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_BC1_RGBA_SRGB));
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_BC3_RGBA_SRGB));
	ZENITH_ASSERT_TRUE(IsSRGBFormat(TEXTURE_FORMAT_BC7_RGBA_SRGB));
}

ZENITH_TEST(FluxTypes, IsSRGBFormatRejectsDataFormats)
{
	// The UNORM twins and every data format are sampled raw. BC5 in particular
	// has no sRGB twin and must never be treated as one (a normal decoded through
	// the EOTF is a bent normal).
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_RGBA8_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BGRA8_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BC1_RGB_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BC1_RGBA_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BC3_RGBA_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BC5_RG_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_BC7_RGBA_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_R8_UNORM));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_R16G16B16A16_SFLOAT));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_D32_SFLOAT));
	ZENITH_ASSERT_FALSE(IsSRGBFormat(TEXTURE_FORMAT_NONE));
}

ZENITH_TEST(FluxTypes, SRGBTwinsShareTheUNORMBlockSize)
{
	// Same payload, different sampler decode: the byte counts MUST agree or the
	// packed-mip layout the exporter writes and the loader validates diverge.
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGB_SRGB), 8u);
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGBA_SRGB), 8u);
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC3_RGBA_SRGB), 16u);
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC7_RGBA_SRGB), 16u);
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGB_SRGB), CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGB_UNORM));
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGBA_SRGB), CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC1_RGBA_UNORM));
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC3_RGBA_SRGB), CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC3_RGBA_UNORM));
	ZENITH_ASSERT_EQ(CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC7_RGBA_SRGB), CompressedFormatBytesPerBlock(TEXTURE_FORMAT_BC7_RGBA_UNORM));
}

ZENITH_TEST(FluxTypes, SRGBTwinsProduceIdenticalMipChainSizes)
{
	// A 37x21 chain (non-power-of-two, partial blocks at every level) through the
	// ONE size function the exporter, loader and upload share.
	const uint32_t uNumMips = 6u;   // floor(log2(37)) + 1
	ZENITH_ASSERT_EQ(CalculateTotalMipChainSize(TEXTURE_FORMAT_BC1_RGB_SRGB, 37u, 21u, uNumMips),
		CalculateTotalMipChainSize(TEXTURE_FORMAT_BC1_RGB_UNORM, 37u, 21u, uNumMips));
	ZENITH_ASSERT_EQ(CalculateTotalMipChainSize(TEXTURE_FORMAT_BC3_RGBA_SRGB, 37u, 21u, uNumMips),
		CalculateTotalMipChainSize(TEXTURE_FORMAT_BC3_RGBA_UNORM, 37u, 21u, uNumMips));
	// Mip 0 of 37x21 BC1 = ceil(37/4)*ceil(21/4) blocks = 10*6*8 bytes.
	ZENITH_ASSERT_EQ(CalculateMipDataSize(TEXTURE_FORMAT_BC1_RGB_SRGB, 37u, 21u, 0u), static_cast<size_t>(10u * 6u * 8u));
	// The 1x1 tail still occupies one full block.
	ZENITH_ASSERT_EQ(CalculateMipDataSize(TEXTURE_FORMAT_BC3_RGBA_SRGB, 37u, 21u, 5u), static_cast<size_t>(16u));
}

ZENITH_TEST(FluxTypes, SRGBBlockFormatsSitInsideTheColourRange)
{
	// The backends classify a format as colour by the BEGIN/END sentinels; a
	// format appended outside them would be neither colour nor depth and fail
	// texture creation.
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_BC1_RGB_SRGB > TEXTURE_FORMAT_COLOUR_BEGIN && TEXTURE_FORMAT_BC1_RGB_SRGB < TEXTURE_FORMAT_COLOUR_END);
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_BC1_RGBA_SRGB > TEXTURE_FORMAT_COLOUR_BEGIN && TEXTURE_FORMAT_BC1_RGBA_SRGB < TEXTURE_FORMAT_COLOUR_END);
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_BC3_RGBA_SRGB > TEXTURE_FORMAT_COLOUR_BEGIN && TEXTURE_FORMAT_BC3_RGBA_SRGB < TEXTURE_FORMAT_COLOUR_END);
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_BC7_RGBA_SRGB > TEXTURE_FORMAT_COLOUR_BEGIN && TEXTURE_FORMAT_BC7_RGBA_SRGB < TEXTURE_FORMAT_COLOUR_END);
	ZENITH_ASSERT_FALSE(IsDepthFormat(TEXTURE_FORMAT_BC1_RGB_SRGB));
}

ZENITH_TEST(FluxTypes, SRGBBlockFormatsDoNotRenumberSerializedTags)
{
	// TextureFormat is written raw into every .ztxtr. The sRGB blocks were
	// APPENDED after the single-channel formats precisely so the tags below --
	// heightmaps, LUTs, SSAO -- keep their on-disk values. If this fails, every
	// R8/R16/R32/R16G16 asset on disk is being mis-read.
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_BC1_RGB_SRGB > TEXTURE_FORMAT_R16G16_SFLOAT);
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_R8_UNORM == TEXTURE_FORMAT_BC7_RGBA_UNORM + 1);
	ZENITH_ASSERT_TRUE(TEXTURE_FORMAT_R16G16_SFLOAT == TEXTURE_FORMAT_R8_UNORM + 3);
}

#endif // ZENITH_TESTING
