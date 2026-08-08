#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_VertexCodec.h"

#include <cmath>   // sinf/cosf — sweep generation for the SNORM10 bit-identity pin

// ============================================================================
// Vertex-codec unit tests. Pure CPU, no device, no renderer — hosted UNGATED in
// the always-linked Flux_MaterialTable.cpp so they run in every config,
// Null_ included (the codec is what the offline exporters and the headless
// validators share).
//
// FLOAT DISCIPLINE (/fp:fast): packed WORDS are compared bit-exactly; decoded
// FLOATS are compared against the format's quantisation step. Never assert
// bit-equality between two calls of the same float expression.
//
// The SNORM10 test is the load-bearing one: it transcribes the packer that used
// to be a file-static in Tools/Zenith_Tools_TerrainExport.cpp (and, byte for
// byte, in Editor/TerrainEditor/Zenith_TerrainEditor.cpp) and asserts the codec
// reproduces its words exactly. Baked terrain chunks on disk are those words.
// ============================================================================

namespace
{
	// VERBATIM transcription of the pre-codec packer. Do not "tidy" it — its
	// value as a reference is that it is the original expression sequence.
	uint32_t Legacy_PackSNORM10_10_10_2(float fX, float fY, float fZ, float fW)
	{
		auto Clamp = [](float f) { return std::max(-1.0f, std::min(1.0f, f)); };
		fX = Clamp(fX);
		fY = Clamp(fY);
		fZ = Clamp(fZ);
		fW = Clamp(fW);

		int32_t iR = static_cast<int32_t>(std::round(fX * 511.0f));
		int32_t iG = static_cast<int32_t>(std::round(fY * 511.0f));
		int32_t iB = static_cast<int32_t>(std::round(fZ * 511.0f));
		int32_t iA = static_cast<int32_t>(std::round(fW * 1.0f));

		uint32_t uResult = 0;
		uResult |= (static_cast<uint32_t>(iR) & 0x3FF);
		uResult |= (static_cast<uint32_t>(iG) & 0x3FF) << 10;
		uResult |= (static_cast<uint32_t>(iB) & 0x3FF) << 20;
		uResult |= (static_cast<uint32_t>(iA) & 0x3) << 30;
		return uResult;
	}

	// Half round-trip error is bounded by one half ULP == 2^-10 of the magnitude
	// (GLM truncates the mantissa rather than rounding to nearest, so the bound is
	// a whole ULP, not half of one). 2^-9 leaves a 2x margin.
	float HalfTolerance(float fValue)
	{
		return std::fabs(fValue) * (1.0f / 512.0f) + 1.0e-7f;
	}
}

// ---- half round-trips ------------------------------------------------------

ZENITH_TEST(VertexCodec, Half2RoundTripWithinHalfULP)
{
	for (u_int u = 0; u < 32u; ++u)
	{
		const float fX = -8.0f + static_cast<float>(u) * 0.5f;
		const float fY = static_cast<float>(u) * 37.25f;
		const Zenith_Maths::Vector2 xIn(fX, fY);
		const Zenith_Maths::Vector2 xOut = Flux_UnpackHalf2(Flux_PackHalf2(xIn));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, fX, HalfTolerance(fX), "half2.x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, fY, HalfTolerance(fY), "half2.y at step %u", u);
	}
}

ZENITH_TEST(VertexCodec, Half4RoundTripWithinHalfULP)
{
	for (u_int u = 0; u < 16u; ++u)
	{
		const float fT = static_cast<float>(u);
		const Zenith_Maths::Vector4 xIn(fT * 0.125f, -fT * 3.5f, fT * 64.0f, 1.0f - fT * 0.0625f);
		const Zenith_Maths::Vector4 xOut = Flux_UnpackHalf4(Flux_PackHalf4(xIn));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, xIn.x, HalfTolerance(xIn.x), "half4.x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, xIn.y, HalfTolerance(xIn.y), "half4.y at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, xIn.z, HalfTolerance(xIn.z), "half4.z at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.w, xIn.w, HalfTolerance(xIn.w), "half4.w at step %u", u);
	}
}

// ---- normalised-integer round-trips ----------------------------------------

ZENITH_TEST(VertexCodec, Snorm16x4RoundTripWithinQuantStep)
{
	const float fStep = 1.0f / 32767.0f;
	for (u_int u = 0; u <= 32u; ++u)
	{
		const float fT = static_cast<float>(u) / 32.0f;         // 0..1
		const Zenith_Maths::Vector4 xIn(fT * 2.0f - 1.0f, -fT, fT * 0.5f, 1.0f - fT);
		const Zenith_Maths::Vector4 xOut = Flux_UnpackSnorm16x4(Flux_PackSnorm16x4(xIn));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, xIn.x, fStep, "snorm16.x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, xIn.y, fStep, "snorm16.y at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, xIn.z, fStep, "snorm16.z at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.w, xIn.w, fStep, "snorm16.w at step %u", u);
	}

	// Out-of-range inputs clamp rather than wrap — a wrapped normal would flip.
	const Zenith_Maths::Vector4 xClamped = Flux_UnpackSnorm16x4(
		Flux_PackSnorm16x4(Zenith_Maths::Vector4(4.0f, -4.0f, 1.0f, -1.0f)));
	ZENITH_ASSERT_EQ_FLOAT(xClamped.x, 1.0f, fStep, "snorm16 clamps above +1");
	ZENITH_ASSERT_EQ_FLOAT(xClamped.y, -1.0f, fStep, "snorm16 clamps below -1");
}

ZENITH_TEST(VertexCodec, Unorm16x2RoundTripWithinQuantStep)
{
	const float fStep = 1.0f / 65535.0f;
	for (u_int u = 0; u <= 32u; ++u)
	{
		const float fT = static_cast<float>(u) / 32.0f;
		const Zenith_Maths::Vector2 xIn(fT, 1.0f - fT);
		const Zenith_Maths::Vector2 xOut = Flux_UnpackUnorm16x2(Flux_PackUnorm16x2(xIn));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, xIn.x, fStep, "unorm16.x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, xIn.y, fStep, "unorm16.y at step %u", u);
	}

	ZENITH_ASSERT_EQ(Flux_PackUnorm16x2(Zenith_Maths::Vector2(0.0f, 1.0f)), 0xFFFF0000u,
		"unorm16x2 puts component 0 in the LOW half-word");
}

ZENITH_TEST(VertexCodec, Unorm8x4RoundTripWithinQuantStep)
{
	const float fStep = 1.0f / 255.0f;
	for (u_int u = 0; u <= 16u; ++u)
	{
		const float fT = static_cast<float>(u) / 16.0f;
		const Zenith_Maths::Vector4 xIn(fT, 1.0f - fT, fT * fT, 0.5f);
		const Zenith_Maths::Vector4 xOut = Flux_UnpackUnorm8x4(Flux_PackUnorm8x4(xIn));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, xIn.x, fStep, "unorm8.x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, xIn.y, fStep, "unorm8.y at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, xIn.z, fStep, "unorm8.z at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.w, xIn.w, fStep, "unorm8.w at step %u", u);
	}
}

ZENITH_TEST(VertexCodec, Unorm8RoundingAndClampAreExact)
{
	// Chosen so no case sits within a float ULP of a tie except the deliberate
	// 0.5 one, which pins round-half-AWAY-from-zero (127.5 -> 128, not 127).
	const float afIn[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, -0.5f, 1.5f };
	const u_int auExpected[] = { 0u, 64u, 128u, 191u, 255u, 0u, 255u };
	for (u_int u = 0; u < 7u; ++u)
	{
		const u_int uPacked = Flux_PackUnorm8x4(Zenith_Maths::Vector4(afIn[u], 0.0f, 0.0f, 0.0f));
		ZENITH_ASSERT_EQ(uPacked & 0xFFu, auExpected[u], "unorm8 quantisation of case %u", u);
	}
}

ZENITH_TEST(VertexCodec, Uint8x4LaneOrderIsLittleEndian)
{
	const u_int uPacked = Flux_PackUint8x4(Zenith_Maths::UVector4(0x12u, 0x34u, 0x56u, 0x78u));
	ZENITH_ASSERT_EQ(uPacked, 0x78563412u, "component 0 must occupy the LOW byte");

	const Zenith_Maths::UVector4 xOut = Flux_UnpackUint8x4(uPacked);
	ZENITH_ASSERT_EQ(xOut.x, 0x12u, "uint8x4 lane 0");
	ZENITH_ASSERT_EQ(xOut.y, 0x34u, "uint8x4 lane 1");
	ZENITH_ASSERT_EQ(xOut.z, 0x56u, "uint8x4 lane 2");
	ZENITH_ASSERT_EQ(xOut.w, 0x78u, "uint8x4 lane 3");

	// Above-range lanes mask rather than saturate — the documented contract.
	ZENITH_ASSERT_EQ(Flux_PackUint8x4(Zenith_Maths::UVector4(0x1FFu, 0u, 0u, 0u)), 0xFFu,
		"uint8x4 masks a lane wider than a byte");
}

// ---- SNORM10_10_10_2: the baked-asset bit contract --------------------------

ZENITH_TEST(VertexCodec, Snorm10BitIdenticalToLegacyPacker)
{
	// Sweep built at RUNTIME so neither side can be constant-folded independently.
	const float afW[] = { 0.0f, 1.0f, -1.0f, 0.4f, -0.6f };
	u_int uCases = 0;
	for (u_int u = 0; u < 64u; ++u)
	{
		const float fT = static_cast<float>(u) / 63.0f;
		const float fAngle = fT * 6.2831853f;
		const float fX = fT * 2.0f - 1.0f;
		const float fY = std::sin(fAngle);
		const float fZ = std::cos(fAngle) * 0.5f;
		const float fW = afW[u % 5u];

		ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(fX, fY, fZ, fW),
			Legacy_PackSNORM10_10_10_2(fX, fY, fZ, fW),
			"SNORM10 word differs from the legacy packer at sweep step %u", u);
		uCases++;
	}

	// Out-of-range inputs exercise the legacy clamp, which is what keeps the
	// 10-bit fields inside [-511, 511] and off the -512 pattern.
	const float afWild[] = { -5.0f, -1.7f, -1.0f, 0.0f, 1.0f, 1.7f, 5.0f };
	for (u_int u = 0; u < 7u; ++u)
	{
		for (u_int v = 0; v < 7u; ++v)
		{
			ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(afWild[u], afWild[v], afWild[(u + v) % 7u], afWild[v]),
				Legacy_PackSNORM10_10_10_2(afWild[u], afWild[v], afWild[(u + v) % 7u], afWild[v]),
				"SNORM10 word differs from the legacy packer at clamp case %u/%u", u, v);
			uCases++;
		}
	}
	ZENITH_ASSERT_EQ(uCases, 113u, "the bit-identity sweep must actually run its cases");
}

ZENITH_TEST(VertexCodec, Snorm10PinnedWords)
{
	// Frozen words: if BOTH implementations ever drift together, the bit-identity
	// test above stays green and only these catch it. Derived by hand from the
	// layout (R low 10 bits, then G, then B, A in the top 2).
	ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(0.0f, 0.0f, 0.0f, 0.0f), 0x00000000u, "SNORM10 zero");
	ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(0.0f, 1.0f, 0.0f, 0.0f), 0x0007FC00u, "SNORM10 +Y unit normal");
	ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(1.0f, 1.0f, 1.0f, 1.0f), 0x5FF7FDFFu, "SNORM10 all +1");
	ZENITH_ASSERT_EQ(Flux_PackSnorm10_10_10_2(-1.0f, -1.0f, -1.0f, -1.0f), 0xE0180601u, "SNORM10 all -1");
}

ZENITH_TEST(VertexCodec, Snorm10RoundTripsIncludingSignLane)
{
	const float fStep = 1.0f / 511.0f;
	for (u_int u = 0; u <= 32u; ++u)
	{
		const float fT = static_cast<float>(u) / 32.0f;
		const float fX = fT * 2.0f - 1.0f;
		const float fY = -fX;
		const float fZ = fT;
		const float fW = (u % 2u == 0u) ? 1.0f : -1.0f;

		const Zenith_Maths::Vector4 xOut = Flux_UnpackSnorm10_10_10_2(Flux_PackSnorm10_10_10_2(fX, fY, fZ, fW));
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, fX, fStep, "SNORM10 x at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, fY, fStep, "SNORM10 y at step %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, fZ, fStep, "SNORM10 z at step %u", u);
		// The 2-bit lane carries the bitangent SIGN, so it must survive exactly.
		ZENITH_ASSERT_EQ_FLOAT(xOut.w, fW, 1.0e-6f, "SNORM10 bitangent sign at step %u", u);
	}

	const Zenith_Maths::Vector4 xZeroW = Flux_UnpackSnorm10_10_10_2(Flux_PackSnorm10_10_10_2(0.0f, 1.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_EQ_FLOAT(xZeroW.w, 0.0f, 1.0e-6f, "SNORM10 w == 0 is a distinct state from +/-1");
}

// ---- position quantisation box ---------------------------------------------

ZENITH_TEST(VertexCodec, PosQuantRoundTripWithinBoxStep)
{
	const Zenith_Maths::Vector3 xMin(-128.0f, 0.0f, 40.0f);
	const Zenith_Maths::Vector3 xMax(128.0f, 512.0f, 41.5f);
	const Flux_PosQuant xQuant = Flux_MakePosQuant(xMin, xMax);

	ZENITH_ASSERT_EQ_FLOAT(xQuant.m_xBias.x, xMin.x, 1.0e-6f, "bias is the box min");
	ZENITH_ASSERT_EQ_FLOAT(xQuant.m_xScale.y, 512.0f, 1.0e-6f, "scale is the box extent");

	for (u_int u = 0; u <= 16u; ++u)
	{
		const float fT = static_cast<float>(u) / 16.0f;
		const Zenith_Maths::Vector3 xIn = xMin + (xMax - xMin) * fT;
		const Zenith_Maths::Vector3 xOut = Flux_UnpackSnorm16x4Position(
			Flux_PackSnorm16x4(xIn, 0.0f, xQuant), xQuant);
		for (int i = 0; i < 3; i++)
		{
			// One snorm16 step in normalised space is scale/32767 in world space.
			const float fTolerance = xQuant.m_xScale[i] / 32767.0f;
			ZENITH_ASSERT_EQ_FLOAT(xOut[i], xIn[i], fTolerance, "posquant axis %d at step %u", i, u);
		}
	}
}

ZENITH_TEST(VertexCodec, PosQuantDegenerateAxisDecodesToBias)
{
	// A flat mesh has a zero-extent axis; scale 1 keeps the divide finite and the
	// axis decodes back to its single authored value.
	const Zenith_Maths::Vector3 xFlat(7.5f, 3.0f, 7.5f);
	const Flux_PosQuant xQuant = Flux_MakePosQuant(xFlat, Zenith_Maths::Vector3(9.5f, 3.0f, 7.5f));
	ZENITH_ASSERT_EQ_FLOAT(xQuant.m_xScale.y, 1.0f, 1.0e-6f, "degenerate axis takes scale 1");
	ZENITH_ASSERT_EQ_FLOAT(xQuant.m_xScale.z, 1.0f, 1.0e-6f, "degenerate axis takes scale 1");

	const Zenith_Maths::Vector3 xOut = Flux_UnpackSnorm16x4Position(
		Flux_PackSnorm16x4(xFlat, 0.0f, xQuant), xQuant);
	ZENITH_ASSERT_EQ_FLOAT(xOut.y, 3.0f, 1.0e-4f, "degenerate Y decodes to the bias");
	ZENITH_ASSERT_EQ_FLOAT(xOut.z, 7.5f, 1.0e-4f, "degenerate Z decodes to the bias");
}

// ---- skinning lanes ---------------------------------------------------------

ZENITH_TEST(VertexCodec, BoneWeightsRenormaliseToExactly255)
{
	// Independent quantisation of these loses or gains a unit; the largest lane
	// has to absorb it or the vertex shrinks/grows under skinning.
	const Zenith_Maths::Vector4 axCases[] = {
		Zenith_Maths::Vector4(0.25f, 0.25f, 0.25f, 0.25f),   // four-way tie
		Zenith_Maths::Vector4(0.7f, 0.2f, 0.07f, 0.03f),     // heavily skewed
		Zenith_Maths::Vector4(0.1f, 0.2f, 0.3f, 0.4f),       // largest lane last
		Zenith_Maths::Vector4(0.5f, 0.5f, 0.0f, 0.0f),       // exact tie on two lanes
		Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.0f),       // single influence
		Zenith_Maths::Vector4(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f),
	};
	for (u_int u = 0; u < 6u; ++u)
	{
		const u_int uPacked = Flux_PackBoneWeightsUnorm8x4(axCases[u]);
		const Zenith_Maths::UVector4 xBytes = Flux_UnpackUint8x4(uPacked);
		const u_int uSum = xBytes.x + xBytes.y + xBytes.z + xBytes.w;
		ZENITH_ASSERT_EQ(uSum, 255u, "renormalised weight bytes must sum to 255 (case %u)", u);
	}

	// The correction only moves the largest lane: a case whose naive quantisation
	// already sums to 255 must be byte-identical to the plain unorm8x4 packer.
	const Zenith_Maths::Vector4 xExact(1.0f, 0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(Flux_PackBoneWeightsUnorm8x4(xExact), Flux_PackUnorm8x4(xExact),
		"a remainder-free case must not diverge from Flux_PackUnorm8x4");

	// The four-way tie is exactly representable, so its bytes can be pinned:
	// 0.25 * 255 == 63.75 -> 64 each, and lane 0 (the first maximum) gives one back.
	const Zenith_Maths::UVector4 xTie = Flux_UnpackUint8x4(
		Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(0.25f, 0.25f, 0.25f, 0.25f)));
	ZENITH_ASSERT_EQ(xTie.x, 63u, "the first maximum absorbs the -1 remainder");
	ZENITH_ASSERT_EQ(xTie.y, 64u, "a non-absorbing lane keeps its naive quantisation");

	// Only the largest lane is allowed to move: the other three must match the
	// plain unorm8x4 packer exactly. (Asserted as a property, not as pinned
	// bytes — 0.1/0.2/0.3 land within a float ULP of a rounding tie.)
	const Zenith_Maths::Vector4 xSkew(0.1f, 0.2f, 0.3f, 0.4f);
	const Zenith_Maths::UVector4 xNaive = Flux_UnpackUint8x4(Flux_PackUnorm8x4(xSkew));
	const Zenith_Maths::UVector4 xFixed = Flux_UnpackUint8x4(Flux_PackBoneWeightsUnorm8x4(xSkew));
	ZENITH_ASSERT_EQ(xFixed.x, xNaive.x, "lane 0 is not the largest, so it must not move");
	ZENITH_ASSERT_EQ(xFixed.y, xNaive.y, "lane 1 is not the largest, so it must not move");
	ZENITH_ASSERT_EQ(xFixed.z, xNaive.z, "lane 2 is not the largest, so it must not move");
}

ZENITH_TEST(VertexCodec, BoneWeightsZeroInputStaysZero)
{
	// No influence to renormalise onto — forcing 255 into lane 0 would invent one.
	ZENITH_ASSERT_EQ(Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f)), 0u,
		"an all-zero weight set must pack to zero");
	// Negative lanes clamp to zero before quantisation rather than wrapping.
	ZENITH_ASSERT_EQ(Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(-1.0f, -1.0f, -1.0f, -1.0f)), 0u,
		"negative weights clamp to zero, they do not wrap");
}

ZENITH_TEST(VertexCodec, BoneIndicesSentinelAndCeiling)
{
	const Zenith_Maths::UVector4 xIn(0u, 1u, 99u, uFLUX_BONE_INDEX_NONE);
	const u_int uPacked = Flux_PackBoneIndicesUint8x4(xIn);
	ZENITH_ASSERT_EQ(uPacked, 0xFF630100u, "the 'no bone' sentinel packs to byte 255");

	const Zenith_Maths::UVector4 xOut = Flux_UnpackBoneIndicesUint8x4(uPacked);
	ZENITH_ASSERT_EQ(xOut.x, 0u, "bone index 0 round-trips");
	ZENITH_ASSERT_EQ(xOut.y, 1u, "bone index 1 round-trips");
	ZENITH_ASSERT_EQ(xOut.z, 99u, "bone index 99 round-trips");
	ZENITH_ASSERT_EQ(xOut.w, uFLUX_BONE_INDEX_NONE, "byte 255 decodes back to the sentinel");

	// Ceiling: nothing real may alias onto the sentinel byte, so out-of-range
	// indices clamp to 254 instead of wrapping into it.
	const Zenith_Maths::UVector4 xCeil = Flux_UnpackBoneIndicesUint8x4(
		Flux_PackBoneIndicesUint8x4(Zenith_Maths::UVector4(254u, 255u, 300u, 0xFFFFFFFEu)));
	ZENITH_ASSERT_EQ(xCeil.x, uFLUX_MAX_PACKED_BONE_INDEX, "254 is the last representable index");
	ZENITH_ASSERT_EQ(xCeil.y, uFLUX_MAX_PACKED_BONE_INDEX, "255 clamps below the sentinel");
	ZENITH_ASSERT_EQ(xCeil.z, uFLUX_MAX_PACKED_BONE_INDEX, "an out-of-range index clamps, not wraps");
	ZENITH_ASSERT_EQ(xCeil.w, uFLUX_MAX_PACKED_BONE_INDEX, "0xFFFFFFFE is not the sentinel");
}
