#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_VertexCodec.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Core/Zenith_TerrainDimensions.h"   // the authored box Flux_DequantPosition's only caller uses

#include <cmath>   // sinf/cosf — sweep generation for the SNORM10 bit-identity pin
#include <bit>     // std::bit_cast — constructed float bits for the half pins + the finite-bits branches

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

	// Half round-trip error: the vendored GLM half codec is the full ILM
	// conversion — round to NEAREST, ties away from zero — so the error is bounded
	// by half a half-ULP (2^-11 of the magnitude). 2^-9 keeps a 4x margin, and
	// would still hold even for a whole-ULP truncating codec. The rounding rule
	// itself (and the licensed tie divergence vs the Slang mirror) is pinned
	// bit-exactly by HalfPackMatchesSlangTranscription below, not by this bound.
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

ZENITH_TEST(VertexCodec, Uint16x4RoundTripsInRangeExactly)
{
	// Every in-range value is EXACT — this format carries integers, not a
	// quantisation, so the round-trip is identity for anything under 65536.
	const Zenith_Maths::UVector4 xIn(0u, 1u, 3840u, 0xFFFFu);
	uint16_t auPacked[4];
	Flux_PackUint16x4(xIn, auPacked);

	const Zenith_Maths::UVector4 xOut = Flux_UnpackUint16x4(auPacked);
	ZENITH_ASSERT_EQ(xOut.x, xIn.x, "uint16x4 lane 0");
	ZENITH_ASSERT_EQ(xOut.y, xIn.y, "uint16x4 lane 1");
	ZENITH_ASSERT_EQ(xOut.z, xIn.z, "uint16x4 lane 2");
	ZENITH_ASSERT_EQ(xOut.w, xIn.w, "uint16x4 lane 3");
}

ZENITH_TEST(VertexCodec, Uint16x4SaturatesRatherThanWraps)
{
	// THE contract that makes this format safe for screen rects. A wrap would map
	// 65536 -> 0 and 0xFFFFFFF6 (what -10.0f truncates to as a u32) -> 65526, i.e.
	// it could fold an off-screen quad back into view. Saturation pins both to the
	// far edge, which is where the uncompressed u32 lane already put them.
	uint16_t auPacked[4];
	Flux_PackUint16x4(Zenith_Maths::UVector4(65536u, 0xFFFFFFF6u, 999999u, 65535u), auPacked);

	const Zenith_Maths::UVector4 xOut = Flux_UnpackUint16x4(auPacked);
	ZENITH_ASSERT_EQ(xOut.x, 0xFFFFu, "65536 must saturate, not wrap to 0");
	ZENITH_ASSERT_EQ(xOut.y, 0xFFFFu, "a truncated negative must saturate, not wrap into view");
	ZENITH_ASSERT_EQ(xOut.z, 0xFFFFu, "any above-ceiling lane saturates");
	ZENITH_ASSERT_EQ(xOut.w, 0xFFFFu, "the ceiling itself round-trips unchanged");
}

ZENITH_TEST(VertexCodec, Uint16x4LaneOrderIsAscending)
{
	// Lane i occupies element i, so the four little-endian u16s land in the byte
	// order the R16G16B16A16_UINT fetch reads them in.
	uint16_t auPacked[4] = { 0u, 0u, 0u, 0u };
	Flux_PackUint16x4(Zenith_Maths::UVector4(0x1234u, 0x5678u, 0x9ABCu, 0xDEF0u), auPacked);
	ZENITH_ASSERT_EQ(static_cast<u_int>(auPacked[0]), 0x1234u, "uint16x4 element 0 is lane 0");
	ZENITH_ASSERT_EQ(static_cast<u_int>(auPacked[1]), 0x5678u, "uint16x4 element 1 is lane 1");
	ZENITH_ASSERT_EQ(static_cast<u_int>(auPacked[2]), 0x9ABCu, "uint16x4 element 2 is lane 2");
	ZENITH_ASSERT_EQ(static_cast<u_int>(auPacked[3]), 0xDEF0u, "uint16x4 element 3 is lane 3");
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

namespace
{
	// VERBATIM frozen transcription of Common/VertexFormats.slang's
	// Flux_DequantPosition — the GPU half of the snorm16 position contract.
	// Diffable side by side with the .slang; if either side changes, change both,
	// and this suite says so.
	Zenith_Maths::Vector3 SlangMirror_DequantPosition(
		const Zenith_Maths::Vector4& xFetched,
		const Zenith_Maths::Vector3& xScale,
		const Zenith_Maths::Vector3& xBias)
	{
		return xBias + (Zenith_Maths::Vector3(xFetched.x, xFetched.y, xFetched.z) * 0.5f + 0.5f) * xScale;
	}

	// The FIXED-FUNCTION step that runs before the shader sees the attribute, and the
	// reason this pin needs to exist at all: the .slang never spells it, so nothing in
	// either language says what `xFetched` is. VK_FORMAT_R16G16B16A16_SNORM converts
	// each stored int16 by max(s / 32767, -1) (Vulkan 1.3, "Conversion from Normalized
	// Fixed-Point to Floating-Point"), lane 0 in the LOW bits — the little-endian order
	// glm::packSnorm4x16 writes.
	Zenith_Maths::Vector4 FetchSnorm16x4(u_int64 ulPacked)
	{
		float afLanes[4];
		for (int i = 0; i < 4; i++)
		{
			const int16_t iStored = static_cast<int16_t>((ulPacked >> (16 * i)) & 0xFFFFull);
			afLanes[i] = std::max(static_cast<float>(iStored) / 32767.0f, -1.0f);
		}
		return Zenith_Maths::Vector4(afLanes[0], afLanes[1], afLanes[2], afLanes[3]);
	}
}

ZENITH_TEST(VertexCodec, DequantPositionMatchesSlangTranscription)
{
	// A quantised position is the one attribute whose DECODE crosses the
	// CPU/GPU boundary in both directions: the exporter and the editor/carve hooks
	// pack against a box, physics and the chunk validator decode with the CPU codec,
	// and the vertex shader decodes with Flux_DequantPosition. There is no runtime
	// tripwire for their agreement (a vertex shader cannot static_assert against GLM),
	// so it is pinned here at the transcription level.
	struct QuantBox
	{
		Zenith_Maths::Vector3 m_xMin;
		Zenith_Maths::Vector3 m_xMax;
		const char* m_szName;
	};
	const QuantBox axBOXES[] = {
		// The DEFAULT authored terrain box — Flux_DequantPosition's first
		// production caller, and the box every chunk on disk today was packed
		// against.
		{
			Zenith_Maths::Vector3(Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[0],
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[1],
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[2]),
			Zenith_Maths::Vector3(Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[0],
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1],
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[2]),
			"default authored terrain box"
		},
		// A NON-SQUARE per-terrain box, straight out of the spec type. The two
		// horizontal axes now differ by 6x, which is exactly the case a decoder
		// that had a square box baked into it would get wrong -- and it is the
		// shape Route1 ships.
		{
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
			Zenith_Maths::Vector3(
				Zenith_TerrainDimensions{ 64.0f, 64u, 4u, 24u }.WorldSizeX(),
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1],
				Zenith_TerrainDimensions{ 64.0f, 64u, 4u, 24u }.WorldSizeZ()),
			"narrow per-terrain box (4x24 chunks)"
		},
		// A SMALL per-terrain box: the quantum is finer than the default's, and
		// the transcription has to hold there too.
		{
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
			Zenith_Maths::Vector3(
				Zenith_TerrainDimensions{ 64.0f, 64u, 6u, 9u }.WorldSizeX(),
				Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1],
				Zenith_TerrainDimensions{ 64.0f, 64u, 6u, 9u }.WorldSizeZ()),
			"small per-terrain box (6x9 chunks)"
		},
		// A box straddling the origin with a negative bias and a sub-metre axis.
		{ Zenith_Maths::Vector3(-128.0f, -4.5f, 0.25f), Zenith_Maths::Vector3(128.0f, 60.0f, 0.5f), "asymmetric box" },
		// Every axis degenerate: Flux_MakePosQuant substitutes scale 1, and the two
		// decoders have to agree about that too.
		{ Zenith_Maths::Vector3(7.5f, 3.0f, -2.0f), Zenith_Maths::Vector3(7.5f, 3.0f, -2.0f), "degenerate box" },
	};

	for (u_int uBox = 0; uBox < sizeof(axBOXES) / sizeof(axBOXES[0]); ++uBox)
	{
		const QuantBox& xBox = axBOXES[uBox];
		const Flux_PosQuant xQuant = Flux_MakePosQuant(xBox.m_xMin, xBox.m_xMax);

		// Quantum indices, not fractions: 0 and 65534 are the box corners, 32767 is
		// the exact centre, and 1 / 65533 are one quantum in from each end — the
		// boundaries where a rounding disagreement would first show.
		static constexpr u_int auINDICES[] = { 0u, 1u, 2u, 16384u, 32767u, 32768u, 49151u, 65533u, 65534u };
		for (u_int uCase = 0; uCase < sizeof(auINDICES) / sizeof(auINDICES[0]); ++uCase)
		{
			const float fT = static_cast<float>(auINDICES[uCase]) / 65534.0f;
			const Zenith_Maths::Vector3 xIn = xQuant.m_xBias + xQuant.m_xScale * fT;
			const u_int64 ulPacked = Flux_PackSnorm16x4(xIn, 1.0f, xQuant);

			const Zenith_Maths::Vector3 xCpu = Flux_UnpackSnorm16x4Position(ulPacked, xQuant);
			const Zenith_Maths::Vector3 xGpu = SlangMirror_DequantPosition(
				FetchSnorm16x4(ulPacked), xQuant.m_xScale, xQuant.m_xBias);

			for (int i = 0; i < 3; i++)
			{
				// NOT bit-equality: both sides are float expressions compiled under
				// /fp:fast, where the same algebra may contract differently. The bound
				// is a few ULP of the decoded magnitude plus a few ULP of the scale —
				// NO flat absolute floor, because a floor sized for the terrain box
				// (1e-5) is LOOSER than a whole quantisation step on a sub-metre axis
				// (the asymmetric box's 0.25 extent steps at 3.8e-6), which would make
				// that sweep axis vacuous. This form stays at or under ~5% of a step
				// on every axis exercised here.
				const float fTolerance =
					(std::fabs(xCpu[i]) + std::fabs(xQuant.m_xScale[i])) * 4.0e-7f + 1.0e-7f;
				ZENITH_ASSERT_EQ_FLOAT(xGpu[i], xCpu[i], fTolerance,
					"GPU and CPU dequant disagree on axis %d, %s, quantum %u", i, xBox.m_szName, auINDICES[uCase]);
			}
		}

		// The stored WORDS at the box corners are integers and are pinned exactly:
		// a lane order or rounding change on the pack side moves these even when the
		// two decoders still agree with each other.
		const u_int64 ulMinWord = Flux_PackSnorm16x4(xBox.m_xMin, 1.0f, xQuant);
		const u_int64 ulMaxWord = Flux_PackSnorm16x4(xQuant.m_xBias + xQuant.m_xScale, 1.0f, xQuant);
		for (int i = 0; i < 3; i++)
		{
			const int16_t iMinLane = static_cast<int16_t>((ulMinWord >> (16 * i)) & 0xFFFFull);
			const int16_t iMaxLane = static_cast<int16_t>((ulMaxWord >> (16 * i)) & 0xFFFFull);
			ZENITH_ASSERT_EQ(static_cast<int32_t>(iMinLane), -32767,
				"the box minimum must store the most negative snorm16 (axis %d, %s)", i, xBox.m_szName);
			ZENITH_ASSERT_EQ(static_cast<int32_t>(iMaxLane), 32767,
				"the box maximum must store the most positive snorm16 (axis %d, %s)", i, xBox.m_szName);
		}

		// -32767 and +32767 fetch to exactly -1 and +1, so the two corner decodes are
		// bias and bias+scale with no rounding at all — pinned against the AUTHORED
		// values rather than against each other.
		const Zenith_Maths::Vector3 xGpuMin = SlangMirror_DequantPosition(
			FetchSnorm16x4(ulMinWord), xQuant.m_xScale, xQuant.m_xBias);
		const Zenith_Maths::Vector3 xGpuMax = SlangMirror_DequantPosition(
			FetchSnorm16x4(ulMaxWord), xQuant.m_xScale, xQuant.m_xBias);
		for (int i = 0; i < 3; i++)
		{
			const float fTolerance = std::fabs(xQuant.m_xScale[i]) * 4.0e-7f + 1.0e-5f;
			ZENITH_ASSERT_EQ_FLOAT(xGpuMin[i], xBox.m_xMin[i], fTolerance,
				"the GPU decode of the box minimum is the box minimum (axis %d, %s)", i, xBox.m_szName);
			ZENITH_ASSERT_EQ_FLOAT(xGpuMax[i], xQuant.m_xBias[i] + xQuant.m_xScale[i], fTolerance,
				"the GPU decode of the box maximum is the box maximum (axis %d, %s)", i, xBox.m_szName);
		}
	}
}

ZENITH_TEST(VertexCodec, DequantPositionClampsOutsideTheAuthoredBox)
{
	// A live edit (editor sculpt, CityBuilder's carve/terraform) can push a vertex
	// past the authored box. The pack CLAMPS rather than wrapping, so the GPU sees the
	// box lid — a flattened vertex, never one teleported to the opposite corner.
	const Flux_PosQuant xQuant = Flux_MakePosQuant(
		Zenith_Maths::Vector3(Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[0],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[1],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[2]),
		Zenith_Maths::Vector3(Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[0],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1],
			Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[2]));

	const Zenith_Maths::Vector3 xBelow(-1000.0f, -50.0f, -1.0f);
	const Zenith_Maths::Vector3 xAbove(9000.0f, 4096.0f, 4096.5f);
	const Zenith_Maths::Vector3 xGpuBelow = SlangMirror_DequantPosition(
		FetchSnorm16x4(Flux_PackSnorm16x4(xBelow, 1.0f, xQuant)), xQuant.m_xScale, xQuant.m_xBias);
	const Zenith_Maths::Vector3 xGpuAbove = SlangMirror_DequantPosition(
		FetchSnorm16x4(Flux_PackSnorm16x4(xAbove, 1.0f, xQuant)), xQuant.m_xScale, xQuant.m_xBias);

	for (int i = 0; i < 3; i++)
	{
		const float fTolerance = std::fabs(xQuant.m_xScale[i]) * 4.0e-7f + 1.0e-5f;
		ZENITH_ASSERT_EQ_FLOAT(xGpuBelow[i], xQuant.m_xBias[i], fTolerance,
			"a position below the box clamps to the box minimum (axis %d)", i);
		ZENITH_ASSERT_EQ_FLOAT(xGpuAbove[i], xQuant.m_xBias[i] + xQuant.m_xScale[i], fTolerance,
			"a position above the box clamps to the box maximum (axis %d)", i);
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

ZENITH_TEST(VertexCodec, BoneWeightsOverUnitySumsPreScaleProportionally)
{
	// The one-lane remainder repair can only absorb the +-2 units quantisation
	// loses; asked to absorb a whole surplus it went negative, clamped to zero,
	// and DELETED the dominant influence ((1,1,1,1) came out (0,255,255,255)).
	// Over-unity sets now scale down proportionally before quantising.
	// 1/4 and 255/4 are exact in float, so these bytes can be pinned.
	const Zenith_Maths::UVector4 xQuarters = Flux_UnpackUint8x4(
		Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f)));
	ZENITH_ASSERT_EQ(xQuarters.x, 63u, "the first maximum absorbs the rounding remainder");
	ZENITH_ASSERT_EQ(xQuarters.y, 64u, "an equal over-unity set quantises evenly");
	ZENITH_ASSERT_EQ(xQuarters.z, 64u, "an equal over-unity set quantises evenly");
	ZENITH_ASSERT_EQ(xQuarters.w, 64u, "an equal over-unity set quantises evenly");

	// The exact failure shape the review named: sum 1.7, dominant lane first.
	// Properties, not pinned bytes (the scaled lanes sit near rounding edges):
	// the bytes still total 255, and the dominant influence is still dominant —
	// the broken repair returned it as literally zero.
	const Zenith_Maths::UVector4 xSkew = Flux_UnpackUint8x4(
		Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(0.5f, 0.4f, 0.4f, 0.4f)));
	const u_int uSum = xSkew.x + xSkew.y + xSkew.z + xSkew.w;
	ZENITH_ASSERT_EQ(uSum, 255u, "over-unity weights still renormalise to exactly 255");
	ZENITH_ASSERT_TRUE(xSkew.x > 0u, "the dominant influence must survive the pre-scale");
	ZENITH_ASSERT_TRUE(xSkew.x >= xSkew.y && xSkew.x >= xSkew.z && xSkew.x >= xSkew.w,
		"the dominant influence stays dominant after the pre-scale");

	// At-or-below-unity sums take the pre-scale-free path byte-for-byte: the
	// documented renormalise-UP behaviour is unchanged (0.3+0.3 -> 77+77, largest
	// lane absorbs the remaining 101).
	const Zenith_Maths::UVector4 xUp = Flux_UnpackUint8x4(
		Flux_PackBoneWeightsUnorm8x4(Zenith_Maths::Vector4(0.3f, 0.3f, 0.0f, 0.0f)));
	ZENITH_ASSERT_EQ(xUp.x, 178u, "sub-unity sums keep the pre-existing renormalise-up bytes");
	ZENITH_ASSERT_EQ(xUp.y, 77u, "a non-absorbing lane keeps its naive quantisation");
}

// ---- half: hand-derived word pins + the frozen Slang mirror ------------------

ZENITH_TEST(VertexCodec, HalfPinnedWords)
{
	// IEEE binary16 words derived BY HAND — the only pin a symmetric codec change
	// (lane order, rounding mode) cannot move together with the packer. Half is
	// the fetch format for 12 of a mesh vertex's 24 bytes; the fetch unit
	// (VK_FORMAT_R16G16B16A16_SFLOAT) is the authority these words come from.
	// x rides the LOW 16 bits of each word (little-endian).
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(1.0f, -2.0f)), 0xC0003C00u, "1.0 = 0x3C00 low, -2.0 = 0xC000 high");
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(0.5f, 65504.0f)), 0x7BFF3800u, "0.5 = 0x3800, 65504 = 0x7BFF is the largest finite half");
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(-0.0f, 0.25f)) , 0x34008000u, "-0.0 keeps its sign bit; 0.25 = 0x3400");

	// The smallest subnormal half, constructed bit-exactly (2^-24).
	const float fMinSubnormal = std::bit_cast<float>(0x33800000u);
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(fMinSubnormal, 0.0f)) & 0xFFFFu, 0x0001u,
		"2^-24 packs to the smallest subnormal half word");
	ZENITH_ASSERT_EQ(std::bit_cast<uint32_t>(Flux_UnpackHalf2(0x00010000u).y), 0x33800000u,
		"the smallest subnormal half decodes bit-exactly to 2^-24");

	// The half4 pair: lanes 0-1 in the low word, 2-3 in the high word.
	ZENITH_ASSERT_EQ(Flux_PackHalf4(Zenith_Maths::Vector4(1.0f, 0.5f, -2.0f, 65504.0f)),
		static_cast<u_int64>(0x7BFFC00038003C00ull),
		"half4 = ((w<<16|z)<<32) | (y<<16|x), little-endian like the fetch format");
}

namespace
{
	// VERBATIM frozen transcriptions of Common/VertexFormats.slang's
	// Flux_FloatToHalfBits / Flux_HalfBitsToFloat (the GPU-side half codec the
	// skinning pre-pass decodes CPU-packed pools with). Diffable side by side with
	// the .slang — if either side changes, change both, and this suite says so.
	uint32_t SlangMirror_FloatToHalfBits(float fValue)
	{
		const uint32_t uBits = std::bit_cast<uint32_t>(fValue);
		const uint32_t uSign = (uBits >> 16u) & 0x8000u;
		const uint32_t uAbs  = uBits & 0x7FFFFFFFu;

		if (uAbs >= 0x7F800000u)
		{
			return uSign | 0x7C00u | ((uAbs > 0x7F800000u) ? 0x200u : 0u);
		}

		const int32_t  iExponent = static_cast<int32_t>(uAbs >> 23u) - 127;
		const uint32_t uMantissa = uAbs & 0x7FFFFFu;

		if (iExponent > 15)
		{
			return uSign | 0x7C00u;
		}
		if (iExponent >= -14)
		{
			uint32_t uHalf = uSign | (static_cast<uint32_t>(iExponent + 15) << 10u) | (uMantissa >> 13u);
			const uint32_t uRemainder = uMantissa & 0x1FFFu;
			if ((uRemainder > 0x1000u) || ((uRemainder == 0x1000u) && ((uMantissa >> 13u) & 1u) != 0u))
			{
				uHalf += 1u;
			}
			return uHalf;
		}
		if (iExponent >= -25)
		{
			const uint32_t uFull      = uMantissa | 0x800000u;
			const uint32_t uShift     = static_cast<uint32_t>(-(iExponent + 1));
			const uint32_t uHalfWay   = 1u << (uShift - 1u);
			const uint32_t uRemainder = uFull & ((1u << uShift) - 1u);
			uint32_t uSubnormal = uFull >> uShift;
			if ((uRemainder > uHalfWay) || ((uRemainder == uHalfWay) && ((uSubnormal & 1u) != 0u)))
			{
				uSubnormal += 1u;
			}
			return uSign | uSubnormal;
		}
		return uSign;
	}

	uint32_t SlangMirror_HalfBitsToFloatBits(uint32_t uHalf)
	{
		const uint32_t uSign     = (uHalf & 0x8000u) << 16u;
		const uint32_t uExponent = (uHalf >> 10u) & 0x1Fu;
		const uint32_t uMantissa = uHalf & 0x3FFu;

		if (uExponent == 0u)
		{
			return uSign | std::bit_cast<uint32_t>(
				static_cast<float>(uMantissa) * std::bit_cast<float>(0x33800000u));
		}
		if (uExponent == 31u)
		{
			return uSign | 0x7F800000u | (uMantissa << 13u);
		}
		return uSign | ((uExponent + 112u) << 23u) | (uMantissa << 13u);
	}
}

ZENITH_TEST(VertexCodec, HalfPackMatchesSlangTranscription)
{
	// The GPU decodes what the CPU packs, so the two codecs' agreement is a
	// CORRECTNESS contract with no runtime tripwire (a compute shader cannot
	// static_assert against GLM). This pins it at the transcription level, over
	// every branch both implementations have: normals, rounding (non-tie),
	// subnormals (14..24-bit shifts), underflow, overflow, zero signs.
	const float afAgree[] = {
		0.0f, 1.0f, -1.0f, 0.5f, -0.25f, 65504.0f, -65504.0f,
		2048.0f, 2050.0f, 1.0009765f,                      // non-tie rounding
		std::bit_cast<float>(0x33800000u),                  // 2^-24 min subnormal
		std::bit_cast<float>(0x34000000u),                  // 2^-23
		6.0e-8f, 3.1e-8f,                                   // subnormal rounding
		1.0e-10f, -1.0e-10f,                                // underflow -> signed zero
		1.0e6f, -1.0e6f,                                    // overflow -> inf
		std::bit_cast<float>(0x7F800000u),                  // +inf
		std::bit_cast<float>(0xFF800000u),                  // -inf
	};
	for (u_int u = 0; u < sizeof(afAgree) / sizeof(afAgree[0]); ++u)
	{
		const uint32_t uMirror = SlangMirror_FloatToHalfBits(afAgree[u]);
		const uint32_t uCodec  = Flux_PackHalf2(Zenith_Maths::Vector2(afAgree[u], 0.0f)) & 0xFFFFu;
		ZENITH_ASSERT_EQ(uMirror, uCodec, "CPU and Slang half encodes agree at case %u", u);
	}

	// NaN: both encode a NaN-class half, but the PAYLOAD is licensed to differ
	// (GLM keeps the top mantissa bits, the Slang codec pins 0x200).
	const uint32_t uMirrorNaN = SlangMirror_FloatToHalfBits(std::bit_cast<float>(0x7FC00000u));
	const uint32_t uCodecNaN  = Flux_PackHalf2(Zenith_Maths::Vector2(std::bit_cast<float>(0x7FC00000u), 0.0f)) & 0xFFFFu;
	ZENITH_ASSERT_TRUE(((uMirrorNaN >> 10u) & 0x1Fu) == 0x1Fu && (uMirrorNaN & 0x3FFu) != 0u, "Slang encodes NaN as NaN");
	ZENITH_ASSERT_TRUE(((uCodecNaN  >> 10u) & 0x1Fu) == 0x1Fu && (uCodecNaN  & 0x3FFu) != 0u, "GLM encodes NaN as NaN");

	// THE licensed divergence, pinned as numbers instead of prose: an exact tie
	// rounds AWAY FROM ZERO in GLM and TO EVEN in the Slang codec. 2049 sits
	// exactly between representable halves 2048 and 2050.
	ZENITH_ASSERT_EQ(SlangMirror_FloatToHalfBits(2049.0f), 0x6800u, "the Slang codec rounds the 2049 tie to even (2048)");
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(2049.0f, 0.0f)) & 0xFFFFu, 0x6801u, "GLM rounds the 2049 tie away from zero (2050)");
	ZENITH_ASSERT_EQ(SlangMirror_FloatToHalfBits(-2049.0f), 0xE800u, "ties to even on the negative side too");
	ZENITH_ASSERT_EQ(Flux_PackHalf2(Zenith_Maths::Vector2(-2049.0f, 0.0f)) & 0xFFFFu, 0xE801u, "away from zero on the negative side too");

	// DECODE crosses the producer boundary (the GPU reads CPU-packed pools), so
	// it must be exact on both sides — every finite word class, bit-for-bit.
	const uint32_t auWords[] = {
		0x0000u, 0x8000u,           // +-0
		0x0001u, 0x03FFu,           // subnormal min/max
		0x0400u,                    // smallest normal
		0x3C00u, 0xBC00u,           // +-1
		0x3800u, 0x7BFFu, 0xFBFFu,  // 0.5, +-65504
		0x7C00u, 0xFC00u,           // +-inf
	};
	for (u_int u = 0; u < sizeof(auWords) / sizeof(auWords[0]); ++u)
	{
		const uint32_t uMirrorBits = SlangMirror_HalfBitsToFloatBits(auWords[u]);
		const uint32_t uCodecBits  = std::bit_cast<uint32_t>(Flux_UnpackHalf2(auWords[u]).x);
		ZENITH_ASSERT_EQ(uMirrorBits, uCodecBits, "CPU and Slang half decodes agree bit-exactly at word %u", u);
	}
}

ZENITH_TEST(VertexCodec, IsPositiveFiniteBitsClassifiesEveryBranch)
{
	// The predicate exists BECAUSE /fp:fast may fold a float NaN test away; it is
	// pure bit inspection, so each branch can be pinned with constructed bits.
	ZENITH_ASSERT_TRUE(Flux_IsPositiveFiniteBits(1.0f), "an ordinary positive value is positive-finite");
	ZENITH_ASSERT_TRUE(Flux_IsPositiveFiniteBits(std::bit_cast<float>(0x7F7FFFFFu)), "FLT_MAX is positive-finite");
	ZENITH_ASSERT_TRUE(Flux_IsPositiveFiniteBits(std::bit_cast<float>(0x00000001u)), "a positive denormal counts as positive-finite");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(0.0f), "+0 is not positive (a zero-length vector has no direction)");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(std::bit_cast<float>(0x80000000u)), "-0 is not positive");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(-1.0f), "a negative value is not positive");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(std::bit_cast<float>(0x7F800000u)), "+inf is not finite");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(std::bit_cast<float>(0xFF800000u)), "-inf is not positive or finite");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(std::bit_cast<float>(0x7FC00000u)), "NaN is not finite");
	ZENITH_ASSERT_TRUE(!Flux_IsPositiveFiniteBits(std::bit_cast<float>(0xFFC00000u)), "negative NaN is neither");
}
