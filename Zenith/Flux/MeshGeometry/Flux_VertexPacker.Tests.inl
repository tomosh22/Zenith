#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshGeometry/Flux_VertexPacker.h"

#include <cmath>     // std::sqrt — the expected SNORM10 normalisation, spelled as the packer spells it
#include <cstring>   // std::memcpy — the byte readers below

// ============================================================================
// Flux_PackVertices unit tests. Pure CPU: hand-built layouts + stack float
// arrays, no device, no asset, no engine boot.
//
// WHAT THEY PIN: the packer is the single writer that every interleaved vertex
// buffer will go through, and the layouts it consumes are auto-generated from
// shader reflection — so the only thing standing between a shader edit and a
// silently mis-packed buffer is that the packer honours the table exactly.
// These tests hold it to the table's offsets and stride, to the canonical
// attribute defaults the loops it replaces already write, and to the CODEC's
// bit patterns (every packed expectation is a codec call, never a transcribed
// constant — a re-derived expectation would only prove the test agrees with
// itself).
//
// FLOAT DISCIPLINE (/fp:fast): packed WORDS are compared bit-exactly, and so
// are float lanes that were memcpy'd from a source value the test still holds.
// Anything that goes through arithmetic is either spelled with the same
// expression shape the packer uses (the SNORM10 normalisation) or compared
// loosely.
// ============================================================================

namespace
{
	u_int Packer_ReadU32(const uint8_t* pBuffer, u_int uStride, u_int uVertex, u_int uOffset)
	{
		u_int uOut = 0u;
		std::memcpy(&uOut, pBuffer + static_cast<size_t>(uVertex) * uStride + uOffset, sizeof(uOut));
		return uOut;
	}

	u_int64 Packer_ReadU64(const uint8_t* pBuffer, u_int uStride, u_int uVertex, u_int uOffset)
	{
		u_int64 ulOut = 0u;
		std::memcpy(&ulOut, pBuffer + static_cast<size_t>(uVertex) * uStride + uOffset, sizeof(ulOut));
		return ulOut;
	}

	// The uLane-th float of the element at uOffset in vertex uVertex.
	float Packer_ReadF32(const uint8_t* pBuffer, u_int uStride, u_int uVertex, u_int uOffset, u_int uLane)
	{
		float fOut = 0.0f;
		const size_t uByte = static_cast<size_t>(uVertex) * uStride + uOffset + uLane * sizeof(float);
		std::memcpy(&fOut, pBuffer + uByte, sizeof(fOut));
		return fOut;
	}

	bool Packer_FloatsEqual3(const uint8_t* pBuffer, u_int uStride, u_int uVertex, u_int uOffset,
		float fX, float fY, float fZ)
	{
		return Packer_ReadF32(pBuffer, uStride, uVertex, uOffset, 0u) == fX
			&& Packer_ReadF32(pBuffer, uStride, uVertex, uOffset, 1u) == fY
			&& Packer_ReadF32(pBuffer, uStride, uVertex, uOffset, 2u) == fZ;
	}

	// Every byte still holds the fill the test wrote before packing.
	bool Packer_BufferUntouched(const uint8_t* pBuffer, size_t uBytes, uint8_t uFill)
	{
		for (size_t u = 0; u < uBytes; u++)
		{
			if (pBuffer[u] != uFill)
			{
				return false;
			}
		}
		return true;
	}

	// The engine's standard 72-byte static-mesh layout, which is also exactly
	// what Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout bakes.
	// Spelled out here rather than referenced so the defaults test cannot be
	// silently re-pointed at a different program's table.
	const Flux_VertexLayoutElement g_axPackerStandardElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_FLOAT3, 0u, 20u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, SHADER_DATA_TYPE_FLOAT3, 0u, 32u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, SHADER_DATA_TYPE_FLOAT3, 0u, 44u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_FLOAT4, 0u, 56u },
	};
	const Flux_VertexLayoutDesc g_xPackerStandardLayout{ g_axPackerStandardElements, 6u, { 72u, 0u } };
}

// ---- the whole-layout byte contract ----------------------------------------

ZENITH_TEST(VertexPacker, MixedFormatLayoutPacksByteExactly)
{
	// One float element and three DIFFERENT packed families in one table, so the
	// test fails if any element is written at the wrong offset, in the wrong
	// format, or with the wrong lanes.
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,          0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_HALF2,           0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u, 16u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_UNORM8X4,        0u, 20u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 4u, { 24u, 0u } };

	const float afPositions[3] = { 1.5f, -2.25f, 3.75f };
	const float afUVs[2]       = { 0.25f, 0.75f };
	const float afNormals[3]   = { 0.0f, 0.0f, 1.0f };   // already unit: normalisation is exact
	const float afColours[4]   = { 1.0f, 0.5f, 0.25f, 1.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, afUVs,       2u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, afColours,   4u },
	};

	uint8_t auBuffer[24] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 4u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 24u, 0u, 0u, 1.5f, -2.25f, 3.75f), "FLOAT3 position copied verbatim at offset 0");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 24u, 0u, 12u),
		Flux_PackHalf2(Zenith_Maths::Vector2(0.25f, 0.75f)), "HALF2 uv word at offset 12");
	// The normal source is 3 lanes and SNORM10 is 4, so lane w takes the NORMAL
	// canonical pad (0) — not the position/colour pad of 1.
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 24u, 0u, 16u),
		Flux_PackSnorm10_10_10_2(0.0f, 0.0f, 1.0f, 0.0f), "SNORM10 normal word at offset 16");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 24u, 0u, 20u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 1.0f)), "UNORM8X4 colour word at offset 20");
}

ZENITH_TEST(VertexPacker, AllSupportedFormatsMatchCodecWords)
{
	// The formats the mixed-layout test does not reach, so between the two every
	// branch of the element writer is covered. TEXCOORD appears at semantic index
	// 0 AND 1 in two different formats, which also pins the (semantic, index) key.
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT4,    0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,    0u, 16u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_HALF4,     0u, 24u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, SHADER_DATA_TYPE_SNORM16X4, 0u, 32u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_UNORM16X2, 0u, 40u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_UINT8X4,   0u, 44u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, SHADER_DATA_TYPE_FLOAT,     0u, 48u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 7u, { 52u, 0u } };

	const float afPositions[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
	const float afUV0[2]       = { -1.5f, 6.25f };
	const float afNormals[3]   = { 0.25f, -0.5f, 0.75f };
	const float afTangents[4]  = { 0.5f, -0.5f, 0.25f, -1.0f };   // 4 lanes: its own w survives
	const float afUV1[2]       = { 0.125f, 0.875f };
	const float afColours[4]   = { 3.0f, 7.0f, 11.0f, 250.0f };   // integer lanes carried as floats
	const float afBinormals[3] = { 9.5f, 0.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 4u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, afUV0,       2u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, afTangents,  4u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, afUV1,       2u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, afColours,   4u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, afBinormals, 3u },
	};

	uint8_t auBuffer[52] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 7u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 52u, 0u, 0u, 1.0f, 2.0f, 3.0f)
		&& Packer_ReadF32(auBuffer, 52u, 0u, 0u, 3u) == 4.0f, "FLOAT4 position");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 52u, 0u, 16u, 0u) == -1.5f
		&& Packer_ReadF32(auBuffer, 52u, 0u, 16u, 1u) == 6.25f, "FLOAT2 uv0");
	ZENITH_ASSERT_EQ(Packer_ReadU64(auBuffer, 52u, 0u, 24u),
		Flux_PackHalf4(Zenith_Maths::Vector4(0.25f, -0.5f, 0.75f, 0.0f)), "HALF4 normal (w on the NORMAL pad)");
	ZENITH_ASSERT_EQ(Packer_ReadU64(auBuffer, 52u, 0u, 32u),
		Flux_PackSnorm16x4(Zenith_Maths::Vector4(0.5f, -0.5f, 0.25f, -1.0f)), "SNORM16X4 tangent, no quant box");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 52u, 0u, 40u),
		Flux_PackUnorm16x2(Zenith_Maths::Vector2(0.125f, 0.875f)), "UNORM16X2 uv1 — keyed on semantic INDEX 1");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 52u, 0u, 44u),
		Flux_PackUint8x4(Zenith_Maths::UVector4(3u, 7u, 11u, 250u)), "UINT8X4 colour");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 52u, 0u, 48u, 0u) == 9.5f, "FLOAT binormal takes lane 0 only");
}

// ---- the defaults the replaced interleave loops already write ---------------

ZENITH_TEST(VertexPacker, MissingAttributesUseCanonicalDefaults)
{
	// Positions only, against the standard 72-byte layout — the exact case a mesh
	// asset with no authored UVs / normals / tangents / colours produces today.
	const float afPositions[3] = { 1.0f, 2.0f, 3.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
	};

	uint8_t auBuffer[72] = {};
	Flux_PackVertices(auBuffer, g_xPackerStandardLayout, axSources, 1u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 72u, 0u, 0u, 1.0f, 2.0f, 3.0f), "position passes through");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 72u, 0u, 12u, 0u) == 0.0f
		&& Packer_ReadF32(auBuffer, 72u, 0u, 12u, 1u) == 0.0f, "default uv (0,0)");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 72u, 0u, 20u, 0.0f, 1.0f, 0.0f), "default normal +Y");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 72u, 0u, 32u, 1.0f, 0.0f, 0.0f), "default tangent +X");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 72u, 0u, 44u, 0.0f, 0.0f, 1.0f), "default bitangent +Z");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 72u, 0u, 56u, 1.0f, 1.0f, 1.0f)
		&& Packer_ReadF32(auBuffer, 72u, 0u, 56u, 3u) == 1.0f, "default colour white");
}

ZENITH_TEST(VertexPacker, SemanticIndexSelectsTheMatchingSource)
{
	// Three TEXCOORD elements, two sources supplied OUT OF ORDER and one index
	// with no source at all: the packer must key on the pair, not on position in
	// either array.
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_FLOAT2, 0u,  8u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_FLOAT2, 0u, 16u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 3u, { 24u, 0u } };

	const float afUV0[2] = { 1.0f, 2.0f };
	const float afUV1[2] = { 7.0f, 8.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, afUV1, 2u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, afUV0, 2u },
	};

	uint8_t auBuffer[24] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 2u, 1u);

	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 24u, 0u, 0u, 0u) == 1.0f
		&& Packer_ReadF32(auBuffer, 24u, 0u, 0u, 1u) == 2.0f, "TEXCOORD0 takes the index-0 source");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 24u, 0u, 8u, 0u) == 7.0f
		&& Packer_ReadF32(auBuffer, 24u, 0u, 8u, 1u) == 8.0f, "TEXCOORD1 takes the index-1 source");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 24u, 0u, 16u, 0u) == 0.0f
		&& Packer_ReadF32(auBuffer, 24u, 0u, 16u, 1u) == 0.0f, "TEXCOORD2 has no source -> canonical (0,0)");
}

// ---- the 4-component tangent's derived handedness ---------------------------
//
// Today's tables carry TANGENT and BINORMAL as two separate 3-lane elements;
// the 4-lane form (tangent.xyz + handedness in w, half the bytes) is Phase 4's
// shape. It is implemented and pinned NOW so Phase 4 is a table change with no
// packer logic to touch.

ZENITH_TEST(VertexPacker, TangentWIsPositiveForRightHandedFrame)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TANGENT, 0u, SHADER_DATA_TYPE_FLOAT4, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 16u, 0u } };

	// cross(N,T) = cross(+Z,+X) = +Y; dot(+Y, B=+Y) = +1.
	const float afNormals[3]   = { 0.0f, 0.0f, 1.0f };
	const float afTangents[3]  = { 1.0f, 0.0f, 0.0f };
	const float afBinormals[3] = { 0.0f, 1.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, afTangents,  3u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, afBinormals, 3u },
	};

	uint8_t auBuffer[16] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 3u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 16u, 0u, 0u, 1.0f, 0.0f, 0.0f), "tangent xyz from its own source");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 16u, 0u, 0u, 3u) == 1.0f, "w = sign(dot(cross(N,T),B)) = +1");
}

ZENITH_TEST(VertexPacker, TangentWIsNegativeForMirroredFrame)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TANGENT, 0u, SHADER_DATA_TYPE_FLOAT4, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 16u, 0u } };

	// Same frame with the bitangent flipped — the mirrored-UV case the sign exists
	// to carry. dot(+Y, B=-Y) = -1.
	const float afNormals[3]   = { 0.0f, 0.0f, 1.0f };
	const float afTangents[3]  = { 1.0f, 0.0f, 0.0f };
	const float afBinormals[3] = { 0.0f, -1.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, afTangents,  3u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, afBinormals, 3u },
	};

	uint8_t auBuffer[16] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 3u, 1u);

	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 16u, 0u, 0u, 3u) == -1.0f, "w = sign(dot(cross(N,T),B)) = -1");
}

ZENITH_TEST(VertexPacker, ExplicitFourLaneTangentSourceKeepsItsOwnW)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TANGENT, 0u, SHADER_DATA_TYPE_FLOAT4, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 16u, 0u } };

	// The frame below would derive +1, and 0.5f is not a sign — so reading 0.5f
	// back proves the authored lane was passed through rather than recomputed.
	const float afNormals[3]   = { 0.0f, 0.0f, 1.0f };
	const float afTangents[4]  = { 1.0f, 0.0f, 0.0f, 0.5f };
	const float afBinormals[3] = { 0.0f, 1.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, afTangents,  4u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, afBinormals, 3u },
	};

	uint8_t auBuffer[16] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 3u, 1u);

	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 16u, 0u, 0u, 3u) == 0.5f, "an authored 4th lane is not re-derived");
}

// ---- SNORM10: normalise first, and never write a NaN ------------------------

ZENITH_TEST(VertexPacker, Snorm10NormalisesNonUnitInput)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL, 0u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 4u, 0u } };

	// (3,4,0): length 5 EXACTLY in binary floating point, so the expected word
	// below reproduces the packer's arithmetic without a rounding argument.
	const float afNormals[3] = { 3.0f, 4.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL, 0u, afNormals, 3u },
	};

	uint8_t auBuffer[4] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 1u, 1u);

	const float fInvLength = 1.0f / std::sqrt(3.0f * 3.0f + 4.0f * 4.0f);
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 4u, 0u, 0u),
		Flux_PackSnorm10_10_10_2(3.0f * fInvLength, 4.0f * fInvLength, 0.0f, 0.0f),
		"non-unit input is normalised before quantisation");
	// The codec CLAMPS: without the normalisation both lanes would have saturated
	// to 1.0 and the direction — not just the precision — would be wrong.
	ZENITH_ASSERT_NE(Packer_ReadU32(auBuffer, 4u, 0u, 0u),
		Flux_PackSnorm10_10_10_2(3.0f, 4.0f, 0.0f, 0.0f), "clamping is not normalising");
}

ZENITH_TEST(VertexPacker, Snorm10ZeroLengthFallsBackToCanonicalDefault)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL,  0u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u, 0u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT, 0u, SHADER_DATA_TYPE_SNORM10_10_10_2, 0u, 4u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 2u, { 8u, 0u } };

	// A degenerate (collapsed-triangle) frame: normalising divides by zero, and a
	// NaN would quantise to an arbitrary word that decodes as a plausible
	// direction. Each semantic falls back to ITS canonical default instead.
	const float afZero[3] = { 0.0f, 0.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_NORMAL,  0u, afZero, 3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT, 0u, afZero, 3u },
	};

	uint8_t auBuffer[8] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 2u, 1u);

	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 8u, 0u, 0u),
		Flux_PackSnorm10_10_10_2(0.0f, 1.0f, 0.0f, 0.0f), "zero-length normal -> +Y");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 8u, 0u, 4u),
		Flux_PackSnorm10_10_10_2(1.0f, 0.0f, 0.0f, 0.0f), "zero-length tangent -> +X");
}

// ---- the lane pad / truncate rules ------------------------------------------

ZENITH_TEST(VertexPacker, ShortSourceLanesPadFromCanonicalDefault)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT4, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_FLOAT4, 0u, 16u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_FLOAT4, 0u, 32u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 3u, { 48u, 0u } };

	// Three 3-lane sources feeding three 4-lane elements: w = 1 for POSITION and
	// COLOR (a homogeneous point, an opaque alpha), 0 for everything else.
	const float afPositions[3] = { 1.0f, 2.0f, 3.0f };
	const float afNormals[3]   = { 0.0f, 0.0f, 1.0f };
	const float afColours[3]   = { 0.125f, 0.25f, 0.5f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, afColours,   3u },
	};

	uint8_t auBuffer[48] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 3u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 48u, 0u, 0u, 1.0f, 2.0f, 3.0f)
		&& Packer_ReadF32(auBuffer, 48u, 0u, 0u, 3u) == 1.0f, "position pads w = 1");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 48u, 0u, 16u, 3u) == 0.0f, "normal pads w = 0");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 48u, 0u, 32u, 3u) == 1.0f, "colour pads w = 1 (opaque)");
}

ZENITH_TEST(VertexPacker, LongSourceLanesAreTruncated)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 8u, 0u } };

	// A 4-lane source into a 2-lane element: the first two lanes are taken, and
	// the SOURCE still advances by its own 4-lane stride (vertex 1 must read 5,6
	// — not 3,4, which is what a wrong stride would produce).
	const float afUVs[8] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, afUVs, 4u },
	};

	uint8_t auBuffer[16] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 1u, 2u);

	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 8u, 0u, 0u, 0u) == 1.0f
		&& Packer_ReadF32(auBuffer, 8u, 0u, 0u, 1u) == 2.0f, "vertex 0 takes the first two lanes");
	ZENITH_ASSERT_TRUE(Packer_ReadF32(auBuffer, 8u, 1u, 0u, 0u) == 5.0f
		&& Packer_ReadF32(auBuffer, 8u, 1u, 0u, 1u) == 6.0f, "vertex 1 advances by the SOURCE lane count");
}

// ---- the position quantisation box ------------------------------------------

ZENITH_TEST(VertexPacker, PosQuantAppliesOnlyToSnorm16Positions)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_SNORM16X4, 0u, 0u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SHADER_DATA_TYPE_SNORM16X4, 0u, 8u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 2u, { 16u, 0u } };

	const float afPositions[3] = { 2.5f, 5.0f, 7.5f };   // well outside [-1,1]: only the box makes this representable
	const float afNormals[3]   = { 0.0f, 1.0f, 0.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, afNormals,   3u },
	};
	const Flux_PosQuant xQuant = Flux_MakePosQuant(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(10.0f, 10.0f, 10.0f));

	uint8_t auQuantised[16] = {};
	Flux_PackVertices(auQuantised, xLayout, axSources, 2u, 1u, &xQuant);

	ZENITH_ASSERT_EQ(Packer_ReadU64(auQuantised, 16u, 0u, 0u),
		Flux_PackSnorm16x4(Zenith_Maths::Vector3(2.5f, 5.0f, 7.5f), 1.0f, xQuant),
		"POSITION takes the quantised overload, with the padded w = 1");
	ZENITH_ASSERT_EQ(Packer_ReadU64(auQuantised, 16u, 0u, 8u),
		Flux_PackSnorm16x4(Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.0f)),
		"NORMAL ignores the box — it is a position transform, not a global one");

	// Same layout, same sources, no box: the position falls back to the plain
	// (clamped to [-1,1]) path, which is a DIFFERENT word.
	uint8_t auPlain[16] = {};
	Flux_PackVertices(auPlain, xLayout, axSources, 2u, 1u);

	ZENITH_ASSERT_EQ(Packer_ReadU64(auPlain, 16u, 0u, 0u),
		Flux_PackSnorm16x4(Zenith_Maths::Vector4(2.5f, 5.0f, 7.5f, 1.0f)), "no box -> the plain clamped path");
	ZENITH_ASSERT_NE(Packer_ReadU64(auPlain, 16u, 0u, 0u), Packer_ReadU64(auQuantised, 16u, 0u, 0u),
		"the box actually changes the packed word");
}

// ---- interleaving, and the shapes that write nothing ------------------------

ZENITH_TEST(VertexPacker, InterleavingAdvancesByStrideAcrossVertices)
{
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,   0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_UNORM8X4, 0u, 12u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 2u, { 16u, 0u } };

	const float afPositions[9] =
	{
		0.0f, 0.0f, 0.0f,
		1.0f, 1.5f, 2.0f,
		-3.0f, -3.5f, -4.0f,
	};
	const float afColours[12] =
	{
		0.0f,  0.0f,  0.0f,  1.0f,
		0.5f,  0.25f, 0.75f, 1.0f,
		1.0f,  1.0f,  1.0f,  0.5f,
	};
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, afColours,   4u },
	};

	uint8_t auBuffer[48] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 2u, 3u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 16u, 0u, 0u, 0.0f, 0.0f, 0.0f), "vertex 0 position at byte 0");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 16u, 1u, 0u, 1.0f, 1.5f, 2.0f), "vertex 1 position at byte 16");
	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 16u, 2u, 0u, -3.0f, -3.5f, -4.0f), "vertex 2 position at byte 32");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 16u, 1u, 12u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(0.5f, 0.25f, 0.75f, 1.0f)), "vertex 1 colour at byte 28");
	ZENITH_ASSERT_EQ(Packer_ReadU32(auBuffer, 16u, 2u, 12u),
		Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 0.5f)), "vertex 2 colour at byte 44");
}

ZENITH_TEST(VertexPacker, EmptyLayoutAndZeroCountAreNoOps)
{
	const float afPositions[3] = { 1.0f, 2.0f, 3.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
	};

	// The canonical empty layout — what codegen emits for a compute or
	// vertex-pulling program. Writing anything at all here would be a buffer
	// overrun into whatever the caller sized for zero elements.
	const Flux_VertexLayoutDesc xEmptyLayout{ nullptr, 0u, { 0u, 0u } };
	uint8_t auBuffer[16];
	std::memset(auBuffer, 0xAB, sizeof(auBuffer));
	Flux_PackVertices(auBuffer, xEmptyLayout, axSources, 1u, 4u);
	ZENITH_ASSERT_TRUE(Packer_BufferUntouched(auBuffer, sizeof(auBuffer), 0xABu), "empty layout writes nothing");

	// A real layout with nothing to write into it (an empty mesh section).
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 1u, { 12u, 0u } };
	std::memset(auBuffer, 0xAB, sizeof(auBuffer));
	Flux_PackVertices(auBuffer, xLayout, axSources, 1u, 0u);
	ZENITH_ASSERT_TRUE(Packer_BufferUntouched(auBuffer, sizeof(auBuffer), 0xABu), "zero vertices writes nothing");
}

ZENITH_TEST(VertexPacker, SecondBindingElementsAreSkipped)
{
	// The COLOR element sits on binding 1 (the per-instance stream) at offset 0 —
	// so if the packer ignored the binding field it would write the colour over
	// the position it just wrote, and the position read below would fail.
	const Flux_VertexLayoutElement axElements[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u, 0u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SHADER_DATA_TYPE_FLOAT4, 1u, 0u },
	};
	const Flux_VertexLayoutDesc xLayout{ axElements, 2u, { 12u, 16u } };

	const float afPositions[3] = { 1.0f, 2.0f, 3.0f };
	const float afColours[4]   = { 9.0f, 9.0f, 9.0f, 9.0f };
	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, afPositions, 3u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, afColours,   4u },
	};

	uint8_t auBuffer[12] = {};
	Flux_PackVertices(auBuffer, xLayout, axSources, 2u, 1u);

	ZENITH_ASSERT_TRUE(Packer_FloatsEqual3(auBuffer, 12u, 0u, 0u, 1.0f, 2.0f, 3.0f),
		"the binding-1 element did not overwrite the per-vertex stream");
}
