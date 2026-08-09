#include "UnitTests/Zenith_UnitTests.h"

#include <cstring>   // std::memcpy — reading the packed lanes back out of the raw bytes

// ============================================================================
// Flux_TextVertex packed-lane contract (compressed-vertex Phase 6).
//
// The struct's byte layout is already pinned at compile time against the Text
// program's generated table (Flux_TextImpl.h). What a static_assert CANNOT see is
// whether the SETTERS write the value the fetch hardware will decode — an offset
// pin passes just as happily over a lane the writer filled with garbage. These
// tests read the packed words back out of the raw object bytes AT THE GENERATED
// OFFSETS, so they fail if either the encoding or the destination lane moves.
//
// FLOAT DISCIPLINE (/fp:fast): packed WORDS are compared bit-exactly; decoded
// floats get the format's quantisation step, except the two authoring extremes
// (0 and 1), which unorm is exact at and which are pinned with tolerance 0.
// ============================================================================

namespace
{
	// The packed word actually sitting at a generated attribute offset. Reading
	// through the bytes rather than the member is deliberate: it is the same view
	// the vertex fetch takes, so a field that drifted away from its pinned offset
	// would be caught here even if the member spelling still compiled.
	u_int ReadTextVertexWordAt(const Flux_TextVertex& xVertex, u_int uAttributeIndex)
	{
		const u_int uOffset = Flux_Generated_Text::Text::kaxVertexAttribs[uAttributeIndex].m_uOffset;
		u_int uWord = 0u;
		std::memcpy(&uWord, reinterpret_cast<const unsigned char*>(&xVertex) + uOffset, sizeof(uWord));
		return uWord;
	}
}

ZENITH_TEST(TextVertex, SettersWriteThePackedLanesAtGeneratedOffsets)
{
	Flux_TextVertex xVertex{};
	xVertex.SetAtlasUVOrigin(Zenith_Maths::Vector2(0.25f, 0.75f));
	xVertex.SetAtlasUVSize(Zenith_Maths::Vector2(0.125f, 0.0625f));
	xVertex.SetColour(Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 0.0f));

	// Elements 0/1 are the shared unit quad on binding 0; the instance lanes follow.
	ZENITH_ASSERT_EQ(ReadTextVertexWordAt(xVertex, Flux_TextVertexPins::uINSTANCE_FIRST + 2u),
		Flux_PackUnorm16x2(Zenith_Maths::Vector2(0.25f, 0.75f)),
		"the atlas UV origin lane must hold the unorm16x2 word at the generated offset");
	ZENITH_ASSERT_EQ(ReadTextVertexWordAt(xVertex, Flux_TextVertexPins::uINSTANCE_FIRST + 3u),
		Flux_PackUnorm16x2(Zenith_Maths::Vector2(0.125f, 0.0625f)),
		"the atlas UV size lane must hold the unorm16x2 word at the generated offset");
	ZENITH_ASSERT_EQ(ReadTextVertexWordAt(xVertex, Flux_TextVertexPins::uINSTANCE_FIRST + 5u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 0.0f)),
		"the colour lane must hold the unorm8x4 word at the generated offset");
}

ZENITH_TEST(TextVertex, PixelLanesStayFullFloatPrecision)
{
	// The reason the pixel lanes were NOT compressed: a 4K-class coordinate has to
	// survive exactly, and it does only because these three stay float2. 3840.5 is
	// representable in float and is NOT in half (half's step is 2 above 2048).
	Flux_TextVertex xVertex{};
	xVertex.m_xQuadOffsetPx = Zenith_Maths::Vector2(3840.5f, 2160.5f);
	xVertex.m_xTextRoot     = Zenith_Maths::Vector2(-1.25f, 4095.75f);

	ZENITH_ASSERT_EQ_FLOAT(xVertex.m_xQuadOffsetPx.x, 3840.5f, 0.0f, "a 4K sub-pixel offset must survive exactly");
	ZENITH_ASSERT_EQ_FLOAT(xVertex.m_xTextRoot.y, 4095.75f, 0.0f, "a 4K sub-pixel text root must survive exactly");
	ZENITH_ASSERT_EQ_FLOAT(xVertex.m_xTextRoot.x, -1.25f, 0.0f, "a negative text root must survive exactly");
}

ZENITH_TEST(TextVertex, AuthoringExtremesRoundTripExactly)
{
	// Full-opacity white is the single most common authored UI colour, and 255/255
	// decodes to exactly 1.0f (the nearest float to 1/255 is close enough that the
	// product rounds back to unity). Downstream code compares fade alphas against
	// 1.0f with zero tolerance, so this is a contract, not an incidental.
	Flux_TextVertex xVertex{};
	xVertex.SetColour(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	const Zenith_Maths::Vector4 xWhite = xVertex.GetColour();
	ZENITH_ASSERT_EQ_FLOAT(xWhite.x, 1.0f, 0.0f, "unorm8 1.0 must decode to exactly 1.0");
	ZENITH_ASSERT_EQ_FLOAT(xWhite.w, 1.0f, 0.0f, "unorm8 alpha 1.0 must decode to exactly 1.0");

	xVertex.SetColour(Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	const Zenith_Maths::Vector4 xBlack = xVertex.GetColour();
	ZENITH_ASSERT_EQ_FLOAT(xBlack.x, 0.0f, 0.0f, "unorm8 0.0 must decode to exactly 0.0");
	ZENITH_ASSERT_EQ_FLOAT(xBlack.w, 0.0f, 0.0f, "unorm8 alpha 0.0 must decode to exactly 0.0");
}

ZENITH_TEST(TextVertex, AtlasUVRoundTripIsSubTexelOnTheBakedAtlas)
{
	// Glyph rects come off a 256x256 MSDF atlas as k/256 fractions. unorm16's step
	// is 1/65535, i.e. ~1/256 of a texel there — the bound this test states in TEXEL
	// units, which is what actually matters to the sampler.
	const float fAtlasPx = 256.0f;
	const float fTexelTolerance = (fAtlasPx / 65535.0f) + 1.0e-6f;
	for (u_int u = 0u; u <= 16u; ++u)
	{
		const float fU = static_cast<float>(u * 16u) / fAtlasPx;   // exact k/256 fractions
		Flux_TextVertex xVertex{};
		xVertex.SetAtlasUVOrigin(Zenith_Maths::Vector2(fU, 1.0f - fU));

		const Zenith_Maths::Vector2 xOut = xVertex.GetAtlasUVOrigin();
		ZENITH_ASSERT_EQ_FLOAT(xOut.x * fAtlasPx, fU * fAtlasPx, fTexelTolerance,
			"atlas U at step %u must stay inside a fraction of a texel", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y * fAtlasPx, (1.0f - fU) * fAtlasPx, fTexelTolerance,
			"atlas V at step %u must stay inside a fraction of a texel", u);
	}
}
