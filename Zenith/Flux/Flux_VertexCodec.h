#pragma once

#include "Maths/Zenith_Maths.h"
#pragma warning(push, 0)
#include <glm/gtc/packing.hpp>   // packHalf2x16 / packHalf4x16 / packSnorm4x16 / packUnorm2x16 / packUnorm4x8
#pragma warning(pop)
#include <algorithm>             // std::max, std::min (the adopted SNORM10 clamp)
#include <cmath>                 // std::round
#include <cstdint>

// ============================================================================
// Flux_VertexCodec — the ONE place a vertex attribute is quantised.
//
// Pure, header-only, allocation-free. It knows nothing about the renderer, the
// engine or any device: every function is (floats in) -> (packed word out) and
// its exact inverse, so the same code runs in the offline exporters, in the
// editor's live-edit hooks, in physics/validator CPU paths, and in the unit
// tests that pin the bit patterns (Flux_VertexCodec.Tests.inl).
//
// WHY IT EXISTS: the SNORM10_10_10_2 packer below already existed TWICE — a
// file-static in Tools/Zenith_Tools_TerrainExport.cpp and a byte-identical
// replica in Editor/TerrainEditor/Zenith_TerrainEditor.cpp — because the
// editor's live sculpt hook has to write bits the baked chunks would have. Two
// copies of a bit layout is a latent asset-corruption bug: they only agree
// until someone edits one. Every packed format the compressed-vertex work
// introduces lands here instead.
//
// LANE ORDER is little-endian-consistent throughout: component 0 occupies the
// LOW bits / first byte, matching how VK_FORMAT_R8G8B8A8_* and friends read a
// buffer. The half/snorm/unorm packers delegate to GLM (round-half-away-from-
// zero, clamped) rather than re-deriving the rounding; SNORM10_10_10_2 is the
// hand-written layout adopted verbatim from the terrain exporter, because
// baked chunks on disk depend on it bit-for-bit.
//
// FLOAT DISCIPLINE: packed WORDS are exact and may be compared bitwise.
// Round-tripped FLOATS may not — this header is compiled under /fp:fast, where
// two calls of the same expression can contract differently. Compare decoded
// floats against a tolerance derived from the format's step size.
// ============================================================================

// ---- half (float16) --------------------------------------------------------
// Relative error of a round-trip is bounded by one half ULP == 2^-10 of the
// magnitude; values above 65504 saturate to inf, below 6.1e-5 denormalise.

inline u_int Flux_PackHalf2(const Zenith_Maths::Vector2& xValue)
{
	return glm::packHalf2x16(xValue);
}

inline Zenith_Maths::Vector2 Flux_UnpackHalf2(u_int uPacked)
{
	return glm::unpackHalf2x16(uPacked);
}

inline u_int64 Flux_PackHalf4(const Zenith_Maths::Vector4& xValue)
{
	return glm::packHalf4x16(xValue);
}

inline Zenith_Maths::Vector4 Flux_UnpackHalf4(u_int64 ulPacked)
{
	return glm::unpackHalf4x16(ulPacked);
}

// ---- snorm16x4 -------------------------------------------------------------
// [-1,1] -> [-32767,32767]. Round-trip error <= 1/32767 per lane. Inputs
// outside [-1,1] are clamped, not wrapped.

inline u_int64 Flux_PackSnorm16x4(const Zenith_Maths::Vector4& xValue)
{
	return glm::packSnorm4x16(xValue);
}

inline Zenith_Maths::Vector4 Flux_UnpackSnorm16x4(u_int64 ulPacked)
{
	return glm::unpackSnorm4x16(ulPacked);
}

// ---- unorm16x2 / unorm8x4 --------------------------------------------------
// [0,1] -> [0,65535] / [0,255]. Round-trip error <= 1/65535 / 1/255 per lane.

inline u_int Flux_PackUnorm16x2(const Zenith_Maths::Vector2& xValue)
{
	return glm::packUnorm2x16(xValue);
}

inline Zenith_Maths::Vector2 Flux_UnpackUnorm16x2(u_int uPacked)
{
	return glm::unpackUnorm2x16(uPacked);
}

inline u_int Flux_PackUnorm8x4(const Zenith_Maths::Vector4& xValue)
{
	return glm::packUnorm4x8(xValue);
}

inline Zenith_Maths::Vector4 Flux_UnpackUnorm8x4(u_int uPacked)
{
	return glm::unpackUnorm4x8(uPacked);
}

// ---- uint8x4 ---------------------------------------------------------------
// Raw integer lanes, one byte each. Values above 255 are masked, not clamped —
// callers with a meaningful ceiling (bone indices, below) enforce it themselves.

inline u_int Flux_PackUint8x4(const Zenith_Maths::UVector4& xValue)
{
	return ((xValue.x & 0xFFu))
		| ((xValue.y & 0xFFu) << 8)
		| ((xValue.z & 0xFFu) << 16)
		| ((xValue.w & 0xFFu) << 24);
}

inline Zenith_Maths::UVector4 Flux_UnpackUint8x4(u_int uPacked)
{
	return Zenith_Maths::UVector4(
		(uPacked      ) & 0xFFu,
		(uPacked >>  8) & 0xFFu,
		(uPacked >> 16) & 0xFFu,
		(uPacked >> 24) & 0xFFu);
}

// ---- SNORM10_10_10_2 -------------------------------------------------------
// ADOPTED VERBATIM from the terrain exporter's former file-static packer (and
// its editor replica). Baked terrain chunks on disk carry these exact words, so
// the expression sequence below — the clamp, the *511.0f, the std::round, the
// masks, the shift order — is a FILE FORMAT, not an implementation detail.
// Changing any of it silently invalidates every baked chunk. The bit-identity
// unit test transcribes the original body as its reference.
//
// R/G/B are 10-bit snorm in [-511,511]; A is 2-bit snorm in {-1,0,1}.

inline u_int Flux_PackSnorm10_10_10_2(float fX, float fY, float fZ, float fW)
{
	auto Clamp = [](float f) { return std::max(-1.0f, std::min(1.0f, f)); };
	fX = Clamp(fX);
	fY = Clamp(fY);
	fZ = Clamp(fZ);
	fW = Clamp(fW);

	// 10-bit snorm: [-1.0, 1.0] -> [-511, 511]
	int32_t iR = static_cast<int32_t>(std::round(fX * 511.0f));
	int32_t iG = static_cast<int32_t>(std::round(fY * 511.0f));
	int32_t iB = static_cast<int32_t>(std::round(fZ * 511.0f));
	// 2-bit snorm: [-1.0, 1.0] -> [-1, 1]
	int32_t iA = static_cast<int32_t>(std::round(fW * 1.0f));

	u_int uResult = 0;
	uResult |= (static_cast<u_int>(iR) & 0x3FF);
	uResult |= (static_cast<u_int>(iG) & 0x3FF) << 10;
	uResult |= (static_cast<u_int>(iB) & 0x3FF) << 20;
	uResult |= (static_cast<u_int>(iA) & 0x3) << 30;
	return uResult;
}

// Two's-complement sign extension of the low uBits of uValue. Written as
// unsigned arithmetic so no shift ever touches a negative signed value; the
// narrowing cast is well-defined in C++20.
inline int32_t Flux_SignExtendBits(u_int uValue, u_int uBits)
{
	const u_int uSignBit = 1u << (uBits - 1u);
	const u_int uMasked = uValue & ((uSignBit << 1u) - 1u);
	return static_cast<int32_t>((uMasked ^ uSignBit) - uSignBit);
}

inline Zenith_Maths::Vector4 Flux_UnpackSnorm10_10_10_2(u_int uPacked)
{
	const float fR = static_cast<float>(Flux_SignExtendBits(uPacked, 10u)) / 511.0f;
	const float fG = static_cast<float>(Flux_SignExtendBits(uPacked >> 10, 10u)) / 511.0f;
	const float fB = static_cast<float>(Flux_SignExtendBits(uPacked >> 20, 10u)) / 511.0f;
	const float fA = static_cast<float>(Flux_SignExtendBits(uPacked >> 30, 2u));
	// The pack clamps before rounding, so -512 / -2 are unreachable; the clamp
	// here only keeps a hand-authored word from decoding outside [-1,1].
	return Zenith_Maths::Vector4(
		Zenith_Maths::Clamp(fR, -1.0f, 1.0f),
		Zenith_Maths::Clamp(fG, -1.0f, 1.0f),
		Zenith_Maths::Clamp(fB, -1.0f, 1.0f),
		Zenith_Maths::Clamp(fA, -1.0f, 1.0f));
}

// ---- position quantisation box --------------------------------------------
// Carries the dequant transform for snorm16 positions: an axis-aligned box that
// maps authored [m_xBias, m_xBias + m_xScale] onto [-1,1] before packing. The
// packed step is m_xScale / 65534 per axis, so a 256 m chunk quantises to
// ~4 mm — the box has to be as TIGHT as the geometry it describes, which is why
// it travels with the mesh rather than being a global constant.

struct Flux_PosQuant
{
	Zenith_Maths::Vector3 m_xScale = Zenith_Maths::Vector3(1.0f);
	Zenith_Maths::Vector3 m_xBias = Zenith_Maths::Vector3(0.0f);
};

// A zero-extent axis (a perfectly flat mesh, a single vertex) would divide by
// zero, so it takes scale 1: every position on that axis equals the bias and
// decodes back to the bias exactly.
inline Flux_PosQuant Flux_MakePosQuant(const Zenith_Maths::Vector3& xMin, const Zenith_Maths::Vector3& xMax)
{
	Flux_PosQuant xQuant;
	xQuant.m_xBias = xMin;
	for (int i = 0; i < 3; i++)
	{
		const float fExtent = xMax[i] - xMin[i];
		xQuant.m_xScale[i] = (fExtent > 0.0f) ? fExtent : 1.0f;
	}
	return xQuant;
}

inline u_int64 Flux_PackSnorm16x4(const Zenith_Maths::Vector3& xPosition, float fW, const Flux_PosQuant& xQuant)
{
	const Zenith_Maths::Vector3 xNormalised = ((xPosition - xQuant.m_xBias) / xQuant.m_xScale) * 2.0f - 1.0f;
	return Flux_PackSnorm16x4(Zenith_Maths::Vector4(xNormalised, fW));
}

inline Zenith_Maths::Vector3 Flux_UnpackSnorm16x4Position(u_int64 ulPacked, const Flux_PosQuant& xQuant)
{
	const Zenith_Maths::Vector4 xNormalised = Flux_UnpackSnorm16x4(ulPacked);
	return xQuant.m_xBias + (Zenith_Maths::Vector3(xNormalised) * 0.5f + 0.5f) * xQuant.m_xScale;
}

// ---- skinning lanes --------------------------------------------------------
// A bone index of uFLUX_BONE_INDEX_NONE means "this slot carries no influence".
// It packs to the reserved byte 255, which is only safe because the skeleton
// ceiling (Zenith_SkeletonAsset::MAX_BONES == 100) is far below it — a real
// index can never collide with the sentinel. Anything above the ceiling is
// clamped to uFLUX_MAX_PACKED_BONE_INDEX rather than wrapped, so a malformed
// index degrades to the last representable bone instead of aliasing onto "none".

inline constexpr u_int uFLUX_BONE_INDEX_NONE = 0xFFFFFFFFu;
inline constexpr u_int uFLUX_BONE_INDEX_NONE_BYTE = 255u;
inline constexpr u_int uFLUX_MAX_PACKED_BONE_INDEX = 254u;

inline u_int Flux_PackBoneIndicesUint8x4(const Zenith_Maths::UVector4& xIndices)
{
	Zenith_Maths::UVector4 xBytes;
	for (int i = 0; i < 4; i++)
	{
		const u_int uIndex = xIndices[i];
		xBytes[i] = (uIndex == uFLUX_BONE_INDEX_NONE)
			? uFLUX_BONE_INDEX_NONE_BYTE
			: ((uIndex > uFLUX_MAX_PACKED_BONE_INDEX) ? uFLUX_MAX_PACKED_BONE_INDEX : uIndex);
	}
	return Flux_PackUint8x4(xBytes);
}

inline Zenith_Maths::UVector4 Flux_UnpackBoneIndicesUint8x4(u_int uPacked)
{
	Zenith_Maths::UVector4 xIndices = Flux_UnpackUint8x4(uPacked);
	for (int i = 0; i < 4; i++)
	{
		if (xIndices[i] == uFLUX_BONE_INDEX_NONE_BYTE)
		{
			xIndices[i] = uFLUX_BONE_INDEX_NONE;
		}
	}
	return xIndices;
}

// Bone weights as unorm8x4, renormalised on the PACK side: quantising four
// weights independently loses up to two units of the 255 total, which shows up
// as a vertex that shrinks slightly toward the origin under skinning. The
// largest lane absorbs the whole remainder (it is the least sensitive to a
// one-unit change). The correction applies to ANY non-zero input sum — the four
// bytes always total exactly 255 — so a caller passing un-normalised weights
// gets them silently renormalised, not preserved; callers own pre-normalising
// when proportions matter. An all-zero input stays all-zero: there is no
// influence to renormalise onto, and forcing 255 onto lane 0 would invent one.
// Decoding is the plain Flux_UnpackUnorm8x4 — the bytes are ordinary unorm8.

inline u_int Flux_PackBoneWeightsUnorm8x4(const Zenith_Maths::Vector4& xWeights)
{
	int32_t aiBytes[4];
	int32_t iSum = 0;
	int32_t iLargest = 0;
	for (int i = 0; i < 4; i++)
	{
		const float fWeight = Zenith_Maths::Clamp(xWeights[i], 0.0f, 1.0f);
		aiBytes[i] = static_cast<int32_t>(std::round(fWeight * 255.0f));
		iSum += aiBytes[i];
		if (xWeights[i] > xWeights[iLargest])
		{
			iLargest = i;
		}
	}

	if (iSum > 0)
	{
		const int32_t iCorrected = aiBytes[iLargest] + (255 - iSum);
		aiBytes[iLargest] = (iCorrected < 0) ? 0 : ((iCorrected > 255) ? 255 : iCorrected);
	}

	return Flux_PackUint8x4(Zenith_Maths::UVector4(
		static_cast<u_int>(aiBytes[0]),
		static_cast<u_int>(aiBytes[1]),
		static_cast<u_int>(aiBytes[2]),
		static_cast<u_int>(aiBytes[3])));
}
