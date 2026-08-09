#include "UnitTests/Zenith_UnitTests.h"

#include <cstring>   // std::memcpy — reading the packed lanes back out of the raw bytes

// ============================================================================
// Flux_QuadsImpl::Quad packed-lane contract (compressed-vertex Phase 6).
//
// The offsets are pinned at compile time in Flux_QuadsImpl.h; what a static_assert
// cannot see is whether the constructor and setters put the RIGHT BITS in those
// lanes. These tests read the packed words back out of the raw object bytes at the
// generated offsets — the same view the vertex fetch takes.
//
// The saturation case is the load-bearing one. Every UI producer casts screen
// floats to u32 before constructing a Quad, so an off-screen-left rect arrives as
// ~4.29e9; a wrapping u16 store would fold that to (value mod 65536) and could put
// it back on screen. Saturation is what preserves the uncompressed behaviour.
// ============================================================================

namespace
{
	// One packed word at a generated attribute offset, read through the bytes.
	u_int ReadQuadWordAt(const Flux_QuadsImpl::Quad& xQuad, u_int uAttributeIndex)
	{
		const u_int uOffset = Flux_Generated_Quads::Quads::kaxVertexAttribs[uAttributeIndex].m_uOffset;
		u_int uWord = 0u;
		std::memcpy(&uWord, reinterpret_cast<const unsigned char*>(&xQuad) + uOffset, sizeof(uWord));
		return uWord;
	}
}

ZENITH_TEST(QuadInstance, ConstructorPacksRectAndColourAtGeneratedOffsets)
{
	const Flux_QuadsImpl::Quad xQuad(
		Zenith_Maths::UVector4(120u, 64u, 300u, 24u),
		Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 0.75f),
		7u,
		Zenith_Maths::Vector2(1.0f, 0.0f));

	// The rect occupies two words; check both halves so a lane swap cannot hide.
	const u_int uRectLow  = ReadQuadWordAt(xQuad, Flux_QuadsImpl::uQUAD_INSTANCE_FIRST_ELEMENT + 0u);
	ZENITH_ASSERT_EQ(uRectLow & 0xFFFFu, 120u, "rect lane 0 (x) must occupy the low half-word");
	ZENITH_ASSERT_EQ(uRectLow >> 16u,     64u,  "rect lane 1 (y) must occupy the high half-word");

	u_int uRectHigh = 0u;
	std::memcpy(&uRectHigh,
		reinterpret_cast<const unsigned char*>(&xQuad)
			+ Flux_Generated_Quads::Quads::kaxVertexAttribs[Flux_QuadsImpl::uQUAD_INSTANCE_FIRST_ELEMENT + 0u].m_uOffset + 4u,
		sizeof(uRectHigh));
	ZENITH_ASSERT_EQ(uRectHigh & 0xFFFFu, 300u, "rect lane 2 (width)");
	ZENITH_ASSERT_EQ(uRectHigh >> 16u,    24u,  "rect lane 3 (height)");

	ZENITH_ASSERT_EQ(ReadQuadWordAt(xQuad, Flux_QuadsImpl::uQUAD_INSTANCE_FIRST_ELEMENT + 1u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 0.75f)),
		"the colour lane must hold the unorm8x4 word at the generated offset");
	ZENITH_ASSERT_EQ(ReadQuadWordAt(xQuad, Flux_QuadsImpl::uQUAD_INSTANCE_FIRST_ELEMENT + 2u), 7u,
		"the bindless texture index lane must be untouched by the neighbouring packs");
}

ZENITH_TEST(QuadInstance, RectRoundTripsExactlyInsideTheCeiling)
{
	// The rect ARRIVES as integers, so inside the u16 ceiling this is identity, not
	// a quantisation — an on-screen quad cannot move by even one pixel.
	Flux_QuadsImpl::Quad xQuad;
	xQuad.SetPositionSize(Zenith_Maths::UVector4(0u, 3839u, 2159u, 65535u));

	const Zenith_Maths::UVector4 xOut = xQuad.GetPositionSize();
	ZENITH_ASSERT_EQ(xOut.x, 0u,     "rect x round-trip");
	ZENITH_ASSERT_EQ(xOut.y, 3839u,  "rect y round-trip at 4K width");
	ZENITH_ASSERT_EQ(xOut.z, 2159u,  "rect width round-trip at 4K height");
	ZENITH_ASSERT_EQ(xOut.w, 65535u, "the ceiling itself round-trips");
}

ZENITH_TEST(QuadInstance, OffScreenRectSaturatesInsteadOfWrappingIntoView)
{
	// 0xFFFFFFF6 / 0xFFFFFFFF are what -10.0f / -1.0f become on x64's truncating
	// float->u32 cast. That cast is UB and TARGET-SPECIFIC (AArch64 saturates
	// negatives to 0 instead — the pre-existing producer behaviour there), so the
	// literals below deliberately model the x64 case; the property under test is the
	// PACKER's: any over-range u32 must saturate to 65535, never wrap into view at
	// (value mod 65536) — 65526 for the first lane.
	Flux_QuadsImpl::Quad xQuad;
	xQuad.SetPositionSize(Zenith_Maths::UVector4(0xFFFFFFF6u, 0xFFFFFFFFu, 70000u, 8u));

	const Zenith_Maths::UVector4 xOut = xQuad.GetPositionSize();
	ZENITH_ASSERT_EQ(xOut.x, 65535u, "a truncated negative x must saturate off-screen, not wrap");
	ZENITH_ASSERT_EQ(xOut.y, 65535u, "a truncated negative y must saturate off-screen, not wrap");
	ZENITH_ASSERT_EQ(xOut.z, 65535u, "an above-ceiling width must saturate");
	ZENITH_ASSERT_EQ(xOut.w, 8u,     "an in-range lane beside a saturating one is untouched");
}

ZENITH_TEST(QuadInstance, GradientSentinelSurvivesUncompressed)
{
	// THE reason m_xColour2 was left a float4: (-1,-1,-1,-1) is the "no gradient"
	// sentinel the fragment shader tests with `colour2.x >= 0`, and no unorm lane
	// can hold a negative. If this ever reads back as >= 0, every plain UI quad has
	// silently become a black-bottomed gradient.
	// Value-initialised, not default-initialised: Quad leaves m_uTexture and
	// m_xUVMult_UVAdd without member initialisers, so a plain `const Quad x;` is
	// ill-formed. The braces still run the member initialisers this test is about.
	const Flux_QuadsImpl::Quad xDefaulted{};
	ZENITH_ASSERT_LT(xDefaulted.m_xColour2.x, 0.0f,
		"the default gradient colour must stay a negative sentinel");

	const Flux_QuadsImpl::Quad xExplicit(
		Zenith_Maths::UVector4(0u, 0u, 4u, 4u), Zenith_Maths::Vector4(1.0f), 0u,
		Zenith_Maths::Vector2(1.0f, 0.0f), 0.0f, Zenith_Maths::Vector2(4.0f, 4.0f),
		Zenith_Maths::Vector4(-1.0f, -1.0f, -1.0f, -1.0f));
	ZENITH_ASSERT_LT(xExplicit.m_xColour2.x, 0.0f,
		"an explicitly passed sentinel must survive construction unchanged");
}

ZENITH_TEST(QuadInstance, FullOpacityAlphaDecodesToExactlyOne)
{
	// ZM's warp-fade characterization asserts a rendered quad's alpha against 1.0f
	// with ZERO tolerance, so this is a contract the storage format has to keep.
	Flux_QuadsImpl::Quad xQuad;
	xQuad.SetColour(Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	ZENITH_ASSERT_EQ_FLOAT(xQuad.GetColour().w, 1.0f, 0.0f,
		"a fully opaque fade must decode to exactly 1.0");
	ZENITH_ASSERT_EQ_FLOAT(xQuad.GetColour().x, 0.0f, 0.0f,
		"a zero channel must decode to exactly 0.0");
}
