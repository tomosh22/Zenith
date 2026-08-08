#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"        // both procedural interleave paths
#include "Flux/MeshGeometry/Flux_MeshInstance.h"       // Flux_PackStaticMeshVertices (the packed static stream)
#include "Flux/UnifiedMesh/Flux_Skinning.h"            // Flux_SkinInputVertex + Flux_BuildSkinInputVertices
#include "Flux/Shaders/Generated/UnifiedMesh.h"        // THE reflected table the static path is shaped by
#include "AssetHandling/Zenith_MeshAsset.h"

#include <cstring>   // std::memcmp / std::memset — the golden comparison

// ============================================================================
// Mesh-family vertex-stream tests (pure / headless).
//
// The engine's mesh streams have exactly one writer each:
//   * the 24-byte PACKED STATIC vertex -> Flux_PackStaticMeshVertices, which packs
//     through Flux_PackVertices shaped by the SHADER-REFLECTED table
//     Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout;
//   * the 32-byte SKIN-INPUT vertex -> Flux_BuildSkinInputVertices, which is that
//     same table at a wider stride plus two bone lanes it owns (a palette index has
//     no vertex semantic, so it is deliberately not packer work);
//   * the DERIVED float32 stream -> Flux_MeshGeometry::GenerateLayoutAndVertexData,
//     the .zmesh element table's own shape, for geometry that is serialized or
//     fetched by a program with its own VsIn.
//
// HOW THESE PIN THE BYTES: Interleave_CodecReference below writes the packed vertex
// INDEPENDENTLY — its own presence rules, its own handedness derivation, its own
// lane defaults — going straight to Flux_VertexCodec for each quantisation. Every
// test drives the real writer and the reference over the SAME mesh and memcmps the
// results. Bit-exact comparison is legal because a packed vertex is integer WORDS:
// two calls of the same quantisation cannot contract differently the way two float
// expressions can, and only a byte comparison catches a lane left unwritten in a
// hole or a handedness sign that silently flipped.
//
// (Before the compression flip these compared against a frozen transcription of the
// hand-written 72/104-byte interleave loops. Phase 4 changed the bytes BY DESIGN, so
// the reference is now the CODEC — the thing that outlives a storage-format change.)
//
// The adversarial matrix below is the point: missing attributes, arrays one entry
// SHORT of the vertex count (the `GetSize() >= uNumVerts` presence rule),
// degenerate zero-length frames (which have no direction to normalise), a
// bone-weight array that runs out, mirrored handedness on alternate vertices,
// negative zero, tiny and large magnitudes, and always >= 3 vertices so a wrong
// stride cannot hide. Headless: stack-constructed Zenith_MeshAsset with CPU
// attribute vectors only — no GPU, no registry, no engine boot.
// ============================================================================

namespace
{
	constexpr uint32_t uINTERLEAVE_STATIC_STRIDE  = 24u;
	constexpr uint32_t uINTERLEAVE_SKINNED_STRIDE = 32u;

	// Byte offsets within the packed static vertex. Spelled OUT, not read back from
	// the table, so a table whose offsets moved fails here instead of agreeing with
	// itself — and the static_asserts below tie the spelling to the real thing.
	constexpr uint32_t uINTERLEAVE_OFF_POSITION = 0u;    // half4
	constexpr uint32_t uINTERLEAVE_OFF_UV       = 8u;    // half2
	constexpr uint32_t uINTERLEAVE_OFF_NORMAL   = 12u;   // snorm10_10_10_2
	constexpr uint32_t uINTERLEAVE_OFF_TANGENT  = 16u;   // snorm10_10_10_2 (w = bitangent sign)
	constexpr uint32_t uINTERLEAVE_OFF_COLOUR   = 20u;   // unorm8x4
	constexpr uint32_t uINTERLEAVE_OFF_BONE_IDS = 24u;   // uint8x4      (skin-input only)
	constexpr uint32_t uINTERLEAVE_OFF_WEIGHTS  = 28u;   // unorm8x4     (skin-input only)

	static_assert(Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout.m_auStrides[0] == uINTERLEAVE_STATIC_STRIDE,
		"the reflected static-mesh layout is no longer 24 bytes — the codec reference below cannot be the golden");
	static_assert(Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout.m_uElementCount == 5u,
		"the reflected static-mesh layout no longer declares 5 attributes (the BINORMAL element is gone by design)");
	static_assert(sizeof(Flux_SkinInputVertex) == uINTERLEAVE_SKINNED_STRIDE,
		"the skin-input vertex is no longer 32 bytes — the codec reference below cannot be the golden");

	// ---- THE CODEC REFERENCE -------------------------------------------------
	//
	// An INDEPENDENT writer of the packed vertex: it goes to Flux_VertexCodec for
	// every quantisation (which is what makes it a reference and not a copy of the
	// packer), but re-states the presence rule, the canonical defaults, the SNORM10
	// normalise-or-default and the 4-lane TANGENT handedness derivation in its own
	// terms. A change to any of those in the production writer shows up here as a
	// byte difference rather than as two files edited in step.
	//
	// A null attribute pointer means ABSENT — the caller applies the
	// `GetSize() >= uNumVerts` rule before handing the streams over, exactly as the
	// production callers do.
	struct Interleave_Sources
	{
		const Zenith_Maths::Vector3* m_pxPositions   = nullptr;
		const Zenith_Maths::Vector2* m_pxUVs         = nullptr;
		const Zenith_Maths::Vector3* m_pxNormals     = nullptr;
		const Zenith_Maths::Vector3* m_pxTangents    = nullptr;
		const Zenith_Maths::Vector3* m_pxBitangents  = nullptr;
		const Zenith_Maths::Vector4* m_pxColours     = nullptr;
		const glm::uvec4*            m_pxBoneIDs     = nullptr;
		const glm::vec4*             m_pxBoneWeights = nullptr;
	};

	void Interleave_CodecReference(uint8_t* pDst, const Interleave_Sources& xSrc, uint32_t uNumVerts, bool bSkinned)
	{
		const uint32_t uStride = bSkinned ? uINTERLEAVE_SKINNED_STRIDE : uINTERLEAVE_STATIC_STRIDE;

		const Zenith_Maths::Vector3 xDefaultPosition(0.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector2 xDefaultUV(0.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultNormal(0.0f, 1.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultTangent(1.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultBitangent(0.0f, 0.0f, 1.0f);
		const Zenith_Maths::Vector4 xDefaultColour(1.0f, 1.0f, 1.0f, 1.0f);

		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			uint8_t* pVtx = pDst + static_cast<size_t>(i) * uStride;

			const Zenith_Maths::Vector3 xPos     = xSrc.m_pxPositions  ? xSrc.m_pxPositions[i]  : xDefaultPosition;
			const Zenith_Maths::Vector2 xUV      = xSrc.m_pxUVs        ? xSrc.m_pxUVs[i]        : xDefaultUV;
			const Zenith_Maths::Vector3 xNormal  = xSrc.m_pxNormals    ? xSrc.m_pxNormals[i]    : xDefaultNormal;
			const Zenith_Maths::Vector3 xTangent = xSrc.m_pxTangents   ? xSrc.m_pxTangents[i]   : xDefaultTangent;
			const Zenith_Maths::Vector4 xColour  = xSrc.m_pxColours    ? xSrc.m_pxColours[i]    : xDefaultColour;

			// The 4-lane TANGENT's w. With no bitangent stream there is nothing to
			// derive it from, so it takes the canonical pad — +1, a valid
			// right-handed sign, because every consumer normalize()s the rebuilt
			// B = cross(N,T) * w and a 0 there is a NaN. With a bitangent it is
			// the sign of the frame's handedness, +1 on an exact zero.
			float fTangentW = 1.0f;
			if (xSrc.m_pxBitangents != nullptr)
			{
				const Zenith_Maths::Vector3 xBitangent = xSrc.m_pxBitangents[i];
				const float fHandedness = Zenith_Maths::Dot(Zenith_Maths::Cross(xNormal, xTangent), xBitangent);
				fTangentW = (fHandedness < 0.0f) ? -1.0f : 1.0f;
			}

			// A SNORM10 direction is normalised first (the codec clamps per axis, which
			// would re-point a longer-than-unit vector), and a direction with no length
			// falls back to what an ABSENT source would have produced.
			const Zenith_Maths::Vector3 xNormalUnit  = Flux_NormaliseForSnorm10(xNormal,  xDefaultNormal);
			const Zenith_Maths::Vector3 xTangentUnit = Flux_NormaliseForSnorm10(xTangent, xDefaultTangent);

			const u_int64 ulPosition = Flux_PackHalf4(Zenith_Maths::Vector4(xPos, 1.0f));
			const u_int   uUV        = Flux_PackHalf2(xUV);
			const u_int   uNormal    = Flux_PackSnorm10_10_10_2(xNormalUnit.x, xNormalUnit.y, xNormalUnit.z, 0.0f);
			const u_int   uTangent   = Flux_PackSnorm10_10_10_2(xTangentUnit.x, xTangentUnit.y, xTangentUnit.z, fTangentW);
			const u_int   uColour    = Flux_PackUnorm8x4(xColour);

			std::memcpy(pVtx + uINTERLEAVE_OFF_POSITION, &ulPosition, sizeof(ulPosition));
			std::memcpy(pVtx + uINTERLEAVE_OFF_UV,       &uUV,        sizeof(uUV));
			std::memcpy(pVtx + uINTERLEAVE_OFF_NORMAL,   &uNormal,    sizeof(uNormal));
			std::memcpy(pVtx + uINTERLEAVE_OFF_TANGENT,  &uTangent,   sizeof(uTangent));
			std::memcpy(pVtx + uINTERLEAVE_OFF_COLOUR,   &uColour,    sizeof(uColour));

			if (bSkinned)
			{
				const glm::uvec4 xBoneIDs     = xSrc.m_pxBoneIDs     ? xSrc.m_pxBoneIDs[i]     : glm::uvec4(0u, 0u, 0u, 0u);
				const glm::vec4  xBoneWeights = xSrc.m_pxBoneWeights ? xSrc.m_pxBoneWeights[i] : glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
				const u_int uIDs     = Flux_PackBoneIndicesUint8x4(xBoneIDs);
				const u_int uWeights = Flux_PackBoneWeightsUnorm8x4(xBoneWeights);
				std::memcpy(pVtx + uINTERLEAVE_OFF_BONE_IDS, &uIDs,     sizeof(uIDs));
				std::memcpy(pVtx + uINTERLEAVE_OFF_WEIGHTS,  &uWeights, sizeof(uWeights));
			}
		}
	}

	// ---- readers -------------------------------------------------------------

	u_int Interleave_VtxWord(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t uByteOffset)
	{
		u_int uOut = 0u;
		std::memcpy(&uOut, p + static_cast<size_t>(v) * uStride + uByteOffset, sizeof(uOut));
		return uOut;
	}
	u_int64 Interleave_VtxWord64(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t uByteOffset)
	{
		u_int64 ulOut = 0u;
		std::memcpy(&ulOut, p + static_cast<size_t>(v) * uStride + uByteOffset, sizeof(ulOut));
		return ulOut;
	}
	// Read the fIdx-th float of vertex v in a DERIVED (float32) interleaved buffer.
	float Interleave_VtxF(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t fIdx)
	{
		float fOut = 0.0f;
		std::memcpy(&fOut, p + static_cast<size_t>(v) * uStride + fIdx * sizeof(float), sizeof(fOut));
		return fOut;
	}

	// ---- the adversarial mesh matrix -----------------------------------------

	struct Interleave_Case
	{
		const char* m_szName;
		uint32_t    m_uNumVerts;
		bool        m_bPositions;
		bool        m_bUVs;
		bool        m_bNormals;
		bool        m_bTangents;
		bool        m_bBitangents;
		bool        m_bColours;
		bool        m_bBones;
		// Entries the UV + COLOUR arrays are SHORT by. A source that cannot cover the
		// vertex count is "absent" (GetSize() >= uNumVerts), and getting that boundary
		// wrong reads past the end of a live array — the one failure mode here that is
		// worse than wrong pixels.
		uint32_t    m_uAttributeShortBy;
		uint32_t    m_uWeightsShortBy;
		// Zero-length normals/tangents/bitangents (a collapsed triangle) and zero
		// weights: a direction with no length has no normalisation, so the packed word
		// must be the canonical default rather than whatever 0/0 quantises to.
		bool        m_bDegenerate;
	};

	const Interleave_Case kaxINTERLEAVE_CASES[] =
	{
		{ "every attribute",           5u, true,  true,  true,  true,  true,  true,  true,  0u, 0u, false },
		{ "positions only",            5u, true,  false, false, false, false, false, false, 0u, 0u, false },
		{ "no attributes at all",      3u, false, false, false, false, false, false, false, 0u, 0u, false },
		{ "uv+colour one short",       5u, true,  true,  true,  true,  true,  true,  true,  1u, 0u, false },
		{ "degenerate frames",         4u, true,  true,  true,  true,  true,  true,  true,  0u, 0u, true  },
		{ "bone weights one short",    5u, true,  true,  true,  true,  true,  true,  true,  0u, 1u, false },
		{ "no bone data at all",       4u, true,  true,  true,  true,  true,  true,  false, 0u, 0u, false },
	};
	constexpr uint32_t uINTERLEAVE_CASE_COUNT = static_cast<uint32_t>(sizeof(kaxINTERLEAVE_CASES) / sizeof(kaxINTERLEAVE_CASES[0]));
	constexpr uint32_t uINTERLEAVE_MAX_VERTS  = 5u;

	// Fill xAsset per the case and return its vertex count. Values are deterministic
	// and deliberately awkward: mixed signs, a negative zero, a magnitude below half's
	// smallest subnormal, one above its largest finite, and the bone sentinel.
	uint32_t Interleave_BuildCase(uint32_t uCase, Zenith_MeshAsset& xAsset)
	{
		const Interleave_Case& xCase = kaxINTERLEAVE_CASES[uCase];
		const uint32_t uNumVerts = xCase.m_uNumVerts;

		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const float fV = static_cast<float>(v);

			if (xCase.m_bPositions)
			{
				// Vertex 0 carries a NEGATIVE ZERO: it compares equal to +0.0f but has a
				// different bit pattern, so only a byte comparison can see it move.
				xAsset.m_xPositions.PushBack(v == 0u
					? Zenith_Maths::Vector3(-0.0f, 0.0f, -0.0f)
					: Zenith_Maths::Vector3(1.5f + fV, -2048.25f * fV, 1.0e-24f * (fV + 1.0f)));
			}
			if (xCase.m_bUVs && v + xCase.m_uAttributeShortBy < uNumVerts)
			{
				xAsset.m_xUVs.PushBack(Zenith_Maths::Vector2(0.125f * fV, 1.0f - 0.25f * fV));
			}
			if (xCase.m_bNormals)
			{
				xAsset.m_xNormals.PushBack(xCase.m_bDegenerate
					? Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)
					: Zenith_Maths::Vector3(0.0f, 0.6f, -0.8f));
			}
			if (xCase.m_bTangents)
			{
				xAsset.m_xTangents.PushBack(xCase.m_bDegenerate
					? Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)
					: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
			}
			if (xCase.m_bBitangents)
			{
				// Mirrored (left-handed) frame on the odd vertices — the handedness the
				// 4-lane tangent's w has to carry now that the vector itself is gone.
				xAsset.m_xBitangents.PushBack(xCase.m_bDegenerate
					? Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f)
					: Zenith_Maths::Vector3(0.0f, (v % 2u) ? -0.8f : 0.8f, 0.6f));
			}
			if (xCase.m_bColours && v + xCase.m_uAttributeShortBy < uNumVerts)
			{
				xAsset.m_xColors.PushBack(Zenith_Maths::Vector4(0.5f * fV, 1.0f, 65504.0f, -0.0f));
			}
			if (xCase.m_bBones)
			{
				xAsset.m_xBoneIndices.PushBack(glm::uvec4(v, v + 1u, uFLUX_BONE_INDEX_NONE, 3u));
				if (v + xCase.m_uWeightsShortBy < uNumVerts)
				{
					xAsset.m_xBoneWeights.PushBack(xCase.m_bDegenerate
						? glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)
						: glm::vec4(0.5f, 0.25f, 0.125f, 0.125f));
				}
			}
		}

		return uNumVerts;
	}

	// The asset's streams as reference sources, with the production presence rule
	// (an array that cannot cover uNumVerts is ABSENT) applied here so the reference
	// never has to know about Zenith_Vector at all.
	template<typename T>
	const T* Interleave_OrNull(const Zenith_Vector<T>& xArray, uint32_t uNumVerts)
	{
		return (xArray.GetSize() >= uNumVerts) ? xArray.GetDataPointer() : nullptr;
	}

	Interleave_Sources Interleave_SourcesFromAsset(const Zenith_MeshAsset& xAsset, uint32_t uNumVerts, bool bSkinned)
	{
		Interleave_Sources xSrc;
		xSrc.m_pxPositions  = Interleave_OrNull(xAsset.m_xPositions,  uNumVerts);
		xSrc.m_pxUVs        = Interleave_OrNull(xAsset.m_xUVs,        uNumVerts);
		xSrc.m_pxNormals    = Interleave_OrNull(xAsset.m_xNormals,    uNumVerts);
		xSrc.m_pxTangents   = Interleave_OrNull(xAsset.m_xTangents,   uNumVerts);
		xSrc.m_pxBitangents = Interleave_OrNull(xAsset.m_xBitangents, uNumVerts);
		xSrc.m_pxColours    = Interleave_OrNull(xAsset.m_xColors,     uNumVerts);
		if (bSkinned)
		{
			xSrc.m_pxBoneIDs     = Interleave_OrNull(xAsset.m_xBoneIndices, uNumVerts);
			xSrc.m_pxBoneWeights = Interleave_OrNull(xAsset.m_xBoneWeights, uNumVerts);
		}
		return xSrc;
	}
}

// ---- byte-for-byte against the codec reference ------------------------------

ZENITH_TEST(VertexInterleave, StaticPackMatchesCodecReferenceByteForByte)
{
	for (uint32_t uCase = 0; uCase < uINTERLEAVE_CASE_COUNT; uCase++)
	{
		Zenith_MeshAsset xAsset;
		const uint32_t uNumVerts = Interleave_BuildCase(uCase, xAsset);

		uint8_t auReference[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_STATIC_STRIDE];
		uint8_t auPacked[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_STATIC_STRIDE];
		std::memset(auReference, 0xCD, sizeof(auReference));
		std::memset(auPacked, 0xCD, sizeof(auPacked));

		Interleave_CodecReference(auReference, Interleave_SourcesFromAsset(xAsset, uNumVerts, false), uNumVerts, /*bSkinned*/ false);
		Flux_PackStaticMeshVertices(auPacked, xAsset, uNumVerts);

		const size_t uBytes = static_cast<size_t>(uNumVerts) * uINTERLEAVE_STATIC_STRIDE;
		ZENITH_ASSERT_EQ(std::memcmp(auReference, auPacked, uBytes), 0,
			"case '%s': the reflected-table pack differs from the codec reference", kaxINTERLEAVE_CASES[uCase].m_szName);
	}
}

ZENITH_TEST(VertexInterleave, SkinInputMatchesCodecReferenceByteForByte)
{
	for (uint32_t uCase = 0; uCase < uINTERLEAVE_CASE_COUNT; uCase++)
	{
		Zenith_MeshAsset xAsset;
		const uint32_t uNumVerts = Interleave_BuildCase(uCase, xAsset);

		uint8_t auReference[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_SKINNED_STRIDE];
		Flux_SkinInputVertex axBuilt[uINTERLEAVE_MAX_VERTS];
		std::memset(auReference, 0xCD, sizeof(auReference));
		std::memset(axBuilt, 0xCD, sizeof(axBuilt));

		Interleave_CodecReference(auReference, Interleave_SourcesFromAsset(xAsset, uNumVerts, true), uNumVerts, /*bSkinned*/ true);
		Flux_BuildSkinInputVertices(axBuilt, xAsset, uNumVerts);

		const size_t uBytes = static_cast<size_t>(uNumVerts) * uINTERLEAVE_SKINNED_STRIDE;
		ZENITH_ASSERT_EQ(std::memcmp(auReference, axBuilt, uBytes), 0,
			"case '%s': the skin-input build differs from the codec reference", kaxINTERLEAVE_CASES[uCase].m_szName);
	}
}

// The two streams SHARE their first 24 bytes by construction (one table, two
// strides). Nothing else states that, and it is what lets a skinned mesh's bind pose
// and its static twin be quantised once and compared.
ZENITH_TEST(VertexInterleave, SkinInputSharesTheStaticVertexPrefix)
{
	Zenith_MeshAsset xAsset;
	const uint32_t uNumVerts = Interleave_BuildCase(0u, xAsset);   // "every attribute"

	uint8_t auStatic[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_STATIC_STRIDE];
	Flux_SkinInputVertex axSkinned[uINTERLEAVE_MAX_VERTS];
	Flux_PackStaticMeshVertices(auStatic, xAsset, uNumVerts);
	Flux_BuildSkinInputVertices(axSkinned, xAsset, uNumVerts);

	const uint8_t* const pSkinnedBytes = reinterpret_cast<const uint8_t*>(axSkinned);
	for (uint32_t v = 0; v < uNumVerts; v++)
	{
		ZENITH_ASSERT_EQ(std::memcmp(
			auStatic + static_cast<size_t>(v) * uINTERLEAVE_STATIC_STRIDE,
			pSkinnedBytes + static_cast<size_t>(v) * uINTERLEAVE_SKINNED_STRIDE,
			uINTERLEAVE_STATIC_STRIDE), 0,
			"vertex %u: the skin-input vertex's first 24 bytes are not the static vertex", v);
	}
}

// ---- the layout the two streams are pinned to ------------------------------

ZENITH_TEST(VertexInterleave, StaticLayoutPacksAllAttributesInOrder)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.5f, -2.25f, 3.0f));   // exact in half
	xAsset.m_xUVs.PushBack(Zenith_Maths::Vector2(0.25f, 0.75f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xAsset.m_xTangents.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	// cross(N,T) = +Y, so a +Y bitangent is a RIGHT-handed frame -> w = +1.
	xAsset.m_xBitangents.PushBack(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xAsset.m_xColors.PushBack(Zenith_Maths::Vector4(0.25f, 0.5f, 0.75f, 1.0f));

	uint8_t auBuf[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 1u);

	const Zenith_Maths::Vector4 xPos     = Flux_UnpackHalf4(Interleave_VtxWord64(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_POSITION));
	const Zenith_Maths::Vector2 xUV      = Flux_UnpackHalf2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_UV));
	const Zenith_Maths::Vector4 xNormal  = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_NORMAL));
	const Zenith_Maths::Vector4 xTangent = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_TANGENT));
	const Zenith_Maths::Vector4 xColour  = Flux_UnpackUnorm8x4(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_COLOUR));

	ZENITH_ASSERT_TRUE(xPos.x == 1.5f && xPos.y == -2.25f && xPos.z == 3.0f, "position at byte 0 (values exact in half)");
	ZENITH_ASSERT_EQ_FLOAT(xPos.w, 1.0f, 1e-6f, "position lane 3 is the homogeneous 1.0 the packer pads with");
	ZENITH_ASSERT_TRUE(xUV.x == 0.25f && xUV.y == 0.75f, "uv at byte 8 (values exact in half)");
	ZENITH_ASSERT_EQ_FLOAT(xNormal.z, 1.0f, 1.0f / 511.0f, "normal +Z at byte 12");
	ZENITH_ASSERT_EQ_FLOAT(xNormal.w, 0.0f, 1e-6f, "normal lane 3 is the canonical 0 pad");
	ZENITH_ASSERT_EQ_FLOAT(xTangent.x, 1.0f, 1.0f / 511.0f, "tangent +X at byte 16");
	ZENITH_ASSERT_EQ_FLOAT(xTangent.w, 1.0f, 1e-6f, "a right-handed frame packs bitangent sign +1");
	ZENITH_ASSERT_EQ_FLOAT(xColour.x, 0.25f, 1.0f / 255.0f, "colour R at byte 20");
	ZENITH_ASSERT_EQ_FLOAT(xColour.w, 1.0f,  1.0f / 255.0f, "colour A at byte 20");
}

ZENITH_TEST(VertexInterleave, MirroredFramePacksNegativeBitangentSign)
{
	// The whole reason the bitangent could be deleted: its INFORMATION is one bit. A
	// left-handed (mirrored-UV) frame must come out as -1, or every mirrored island
	// lights inside-out with nothing downstream able to tell.
	Zenith_MeshAsset xAsset;
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xAsset.m_xTangents.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xAsset.m_xBitangents.PushBack(Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));   // mirrored

	uint8_t auBuf[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 1u);

	const Zenith_Maths::Vector4 xTangent = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_TANGENT));
	ZENITH_ASSERT_EQ_FLOAT(xTangent.w, -1.0f, 1e-6f, "a left-handed frame packs bitangent sign -1");
}

ZENITH_TEST(VertexInterleave, SkinnedLayoutAppendsBoneIndicesAndWeights)
{
	// Four REAL influences with weights that quantise to exact unorm8 bytes
	// (51,51,51,102 — sums to 255 with no renormalise shift), so every lane's
	// assertion is against non-zero data a zeroed buffer could not fake. The
	// absent-influence sentinel has its own dedicated tests (the codec's
	// BoneIndicesSentinelAndCeiling and the one-byte-sentinel interleave test).
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAsset.m_xBoneIndices.PushBack(glm::uvec4(10u, 20u, 30u, 40u));
	xAsset.m_xBoneWeights.PushBack(glm::vec4(0.2f, 0.2f, 0.2f, 0.4f));

	Flux_SkinInputVertex axBuilt[1] = {};
	Flux_BuildSkinInputVertices(axBuilt, xAsset, 1u);
	const uint8_t* const pBytes = reinterpret_cast<const uint8_t*>(axBuilt);

	const Zenith_Maths::Vector4 xPos = Flux_UnpackHalf4(Interleave_VtxWord64(pBytes, uINTERLEAVE_SKINNED_STRIDE, 0, uINTERLEAVE_OFF_POSITION));
	ZENITH_ASSERT_TRUE(xPos.x == 1.0f && xPos.z == 3.0f, "skinned position still at byte 0");

	const Zenith_Maths::UVector4 xIDs = Flux_UnpackUint8x4(Interleave_VtxWord(pBytes, uINTERLEAVE_SKINNED_STRIDE, 0, uINTERLEAVE_OFF_BONE_IDS));
	ZENITH_ASSERT_EQ(xIDs.x, 10u, "bone index 0 at byte 24");
	ZENITH_ASSERT_EQ(xIDs.z, 30u, "bone index 2 at byte 24");
	ZENITH_ASSERT_EQ(xIDs.w, 40u, "a real fourth influence at byte 24, not a zeroed lane");

	const Zenith_Maths::Vector4 xWeights = Flux_UnpackUnorm8x4(Interleave_VtxWord(pBytes, uINTERLEAVE_SKINNED_STRIDE, 0, uINTERLEAVE_OFF_WEIGHTS));
	ZENITH_ASSERT_EQ_FLOAT(xWeights.x, 0.2f, 0.5f / 255.0f, "bone weight 0 at byte 28 (exact unorm8 byte 51)");
	ZENITH_ASSERT_EQ_FLOAT(xWeights.y, 0.2f, 0.5f / 255.0f, "bone weight 1 at byte 28");
	ZENITH_ASSERT_EQ_FLOAT(xWeights.z, 0.2f, 0.5f / 255.0f, "bone weight 2 at byte 28");
	ZENITH_ASSERT_EQ_FLOAT(xWeights.w, 0.4f, 0.5f / 255.0f, "bone weight 3 at byte 28 (exact unorm8 byte 102)");
}

ZENITH_TEST(VertexInterleave, MissingAttributesUseCanonicalDefaults)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));   // positions only

	uint8_t auBuf[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 1u);

	const Zenith_Maths::Vector2 xUV      = Flux_UnpackHalf2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_UV));
	const Zenith_Maths::Vector4 xNormal  = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_NORMAL));
	const Zenith_Maths::Vector4 xTangent = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_TANGENT));
	const Zenith_Maths::Vector4 xColour  = Flux_UnpackUnorm8x4(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, 0, uINTERLEAVE_OFF_COLOUR));

	ZENITH_ASSERT_TRUE(xUV.x == 0.0f && xUV.y == 0.0f, "default uv (0,0)");
	ZENITH_ASSERT_EQ_FLOAT(xNormal.y, 1.0f, 1.0f / 511.0f, "default normal +Y");
	ZENITH_ASSERT_EQ_FLOAT(xTangent.x, 1.0f, 1.0f / 511.0f, "default tangent +X");
	// With no BINORMAL stream there is no handedness to DERIVE, but the lane still
	// feeds B = cross(N,T) * w straight into a normalize() in every consumer — the
	// pad must be a valid handedness, and the canonical choice is right-handed +1.
	// (A 0 here was the compression flip's one reachable NaN: the tree exporter and
	// Assimp-without-UVs both author no bitangents.)
	ZENITH_ASSERT_EQ_FLOAT(xTangent.w, 1.0f, 1e-6f, "no bitangent source -> the +1 right-handed pad");
	ZENITH_ASSERT_TRUE(xColour.x == 1.0f && xColour.y == 1.0f && xColour.z == 1.0f && xColour.w == 1.0f, "default white colour");
}

ZENITH_TEST(VertexInterleave, SkinOutputEncoderMatchesStaticPackerByteForByte)
{
	// The skinned arena is bound as a VERTEX BUFFER of the SAME pipelines that
	// fetch CPU-packed static meshes, so Flux_EncodeSkinOutputVertex and the
	// static packer are two producers of ONE byte contract. The structural pins
	// (sizeof/offsetof, the generated-stride static_asserts) catch a moved lane;
	// only a value comparison catches a divergent canonical default, normalise
	// rule, or pad lane between the two writers.
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.5f, -2.25f, 100.0f));
	xAsset.m_xUVs.PushBack(Zenith_Maths::Vector2(0.25f, 0.75f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xAsset.m_xTangents.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xAsset.m_xBitangents.PushBack(Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));   // mirrored frame: w = -1
	xAsset.m_xColors.PushBack(Zenith_Maths::Vector4(0.25f, 0.5f, 0.75f, 1.0f));

	uint8_t auStatic[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auStatic, xAsset, 1u);

	Flux_SkinVertexAttributes xAttribs;
	xAttribs.m_xPosition = Zenith_Maths::Vector3(1.5f, -2.25f, 100.0f);
	xAttribs.m_xUV       = Zenith_Maths::Vector2(0.25f, 0.75f);
	xAttribs.m_xNormal   = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	// The same handedness the packer derives from that bitangent:
	// sign(dot(cross(N,T),B)) = sign(dot((0,1,0),(0,-1,0))) = -1.
	xAttribs.m_xTangent  = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, -1.0f);
	xAttribs.m_xColor    = Zenith_Maths::Vector4(0.25f, 0.5f, 0.75f, 1.0f);
	const Flux_SkinOutputVertex xEncoded = Flux_EncodeSkinOutputVertex(xAttribs);
	ZENITH_ASSERT_TRUE(std::memcmp(&xEncoded, auStatic, sizeof(xEncoded)) == 0,
		"the two writers of the 24-byte vertex agree on every byte");

	// And on a vertex made ENTIRELY of fallbacks — the exact surface where the two
	// writers could drift apart (the encoder's struct defaults vs the packer's
	// Flux_CanonicalVertexDefault; the tangent's w pad drifted precisely here once).
	Zenith_MeshAsset xBare;
	xBare.m_xPositions.PushBack(Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f));
	uint8_t auBareStatic[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBareStatic, xBare, 1u);

	Flux_SkinVertexAttributes xDefaults;   // the struct's defaults ARE the canonical defaults
	xDefaults.m_xPosition = Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f);
	const Flux_SkinOutputVertex xDefaultEncoded = Flux_EncodeSkinOutputVertex(xDefaults);
	ZENITH_ASSERT_TRUE(std::memcmp(&xDefaultEncoded, auBareStatic, sizeof(xDefaultEncoded)) == 0,
		"the encoder's defaults and the packer's canonical defaults are the same bytes");
}

// ---- the procedural-geometry paths ------------------------------------------

ZENITH_TEST(VertexInterleave, ProceduralGeometryVertexDataMatchesLegacyLaneLoop)
{
	// Flux_MeshGeometry::GenerateLayoutAndVertexData is the DERIVED float32 path — the
	// .zmesh element table's own shape, which the compression flip deliberately left
	// alone (it serializes, and the shared unit quad is fetched from it). Its layout is
	// DYNAMIC (elements exist only for the streams that do), so it feeds the packer a
	// table built at runtime and reads the byte offsets back out of the Flux_BufferLayout
	// it just declared. The reference below is the old lane-by-lane `index++` loop, which
	// knew nothing about offsets at all — frozen, not maintained.
	constexpr uint32_t uNumVerts = 3u;
	constexpr uint32_t uDERIVED_STATIC_STRIDE  = 72u;    // 3+2+3+3+3+4 floats
	constexpr uint32_t uDERIVED_SKINNED_STRIDE = 104u;   // + uint4 ids + float4 weights

	for (uint32_t uWithBones = 0; uWithBones < 2u; uWithBones++)
	{
		Flux_MeshGeometry xGeometry;
		xGeometry.m_uNumVerts = uNumVerts;
		xGeometry.m_pxPositions  = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometry.m_pxUVs        = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
		xGeometry.m_pxNormals    = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometry.m_pxTangents   = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometry.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		xGeometry.m_pxColors     = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
		if (uWithBones != 0u)
		{
			xGeometry.m_puBoneIDs     = static_cast<uint32_t*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(uint32_t)));
			xGeometry.m_pfBoneWeights = static_cast<float*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(float)));
		}

		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const float fV = static_cast<float>(v);
			xGeometry.m_pxPositions[v]  = Zenith_Maths::Vector3(fV, -fV, 1.0e-24f * (fV + 1.0f));
			xGeometry.m_pxUVs[v]        = Zenith_Maths::Vector2(0.25f * fV, -0.0f);
			xGeometry.m_pxNormals[v]    = Zenith_Maths::Vector3(0.0f, 0.6f, -0.8f);
			xGeometry.m_pxTangents[v]   = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			xGeometry.m_pxBitangents[v] = Zenith_Maths::Vector3(0.0f, -0.8f, -0.6f);
			xGeometry.m_pxColors[v]     = Zenith_Maths::Vector4(1.0f, 0.5f, 65504.0f, 0.125f * fV);
			if (uWithBones != 0u)
			{
				for (uint32_t b = 0; b < MAX_BONES_PER_VERTEX; b++)
				{
					xGeometry.m_puBoneIDs[v * MAX_BONES_PER_VERTEX + b]     = (b == 2u) ? 0xFFFFFFFFu : (v + b);
					xGeometry.m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + b] = 0.25f * static_cast<float>(b);
				}
			}
		}

		xGeometry.GenerateLayoutAndVertexData();

		// THE FROZEN GOLDEN: the old lane-by-lane loop, which wrote one running float
		// index straight through every vertex.
		constexpr uint32_t uMAX_LANES_PER_VERT = 3u + 2u + 3u + 3u + 3u + 4u + 4u + 4u;
		float afLegacyStorage[uNumVerts * uMAX_LANES_PER_VERT] = {};
		float* const pfLegacy = afLegacyStorage;                                        // the old `((float*)m_pVertexData)`
		uint32_t* const puLegacy = reinterpret_cast<uint32_t*>(afLegacyStorage);        // the old `((uint32_t*)m_pVertexData)`
		size_t index = 0;
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pfLegacy[index++] = xGeometry.m_pxPositions[i].x;
			pfLegacy[index++] = xGeometry.m_pxPositions[i].y;
			pfLegacy[index++] = xGeometry.m_pxPositions[i].z;
			pfLegacy[index++] = xGeometry.m_pxUVs[i].x;
			pfLegacy[index++] = xGeometry.m_pxUVs[i].y;
			pfLegacy[index++] = xGeometry.m_pxNormals[i].x;
			pfLegacy[index++] = xGeometry.m_pxNormals[i].y;
			pfLegacy[index++] = xGeometry.m_pxNormals[i].z;
			pfLegacy[index++] = xGeometry.m_pxTangents[i].x;
			pfLegacy[index++] = xGeometry.m_pxTangents[i].y;
			pfLegacy[index++] = xGeometry.m_pxTangents[i].z;
			pfLegacy[index++] = xGeometry.m_pxBitangents[i].x;
			pfLegacy[index++] = xGeometry.m_pxBitangents[i].y;
			pfLegacy[index++] = xGeometry.m_pxBitangents[i].z;
			pfLegacy[index++] = xGeometry.m_pxColors[i].x;
			pfLegacy[index++] = xGeometry.m_pxColors[i].y;
			pfLegacy[index++] = xGeometry.m_pxColors[i].z;
			pfLegacy[index++] = xGeometry.m_pxColors[i].w;
			if (uWithBones != 0u)
			{
				puLegacy[index++] = xGeometry.m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 0];
				puLegacy[index++] = xGeometry.m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 1];
				puLegacy[index++] = xGeometry.m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 2];
				puLegacy[index++] = xGeometry.m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 3];
				pfLegacy[index++] = xGeometry.m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 0];
				pfLegacy[index++] = xGeometry.m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 1];
				pfLegacy[index++] = xGeometry.m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 2];
				pfLegacy[index++] = xGeometry.m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 3];
			}
		}

		const uint32_t uExpectedStride = (uWithBones != 0u) ? uDERIVED_SKINNED_STRIDE : uDERIVED_STATIC_STRIDE;
		ZENITH_ASSERT_EQ(xGeometry.GetBufferLayout().GetStride(), uExpectedStride,
			"bones=%u: the declared layout's stride", uWithBones);
		ZENITH_ASSERT_EQ(std::memcmp(xGeometry.m_pVertexData, afLegacyStorage, static_cast<size_t>(uNumVerts) * uExpectedStride), 0,
			"bones=%u: the packed procedural vertex data differs from the frozen lane loop", uWithBones);
	}
}

ZENITH_TEST(VertexInterleave, ProceduralGeometryMeshPipelineDataMatchesTheAssetPack)
{
	// The DRAWN form of a procedural geometry is the packed mesh-family vertex — the
	// same one an imported mesh asset gets. A procedural mesh and an asset-backed one
	// are bound into the SAME pipeline at the same stride, so if these two writers
	// disagreed one of them would decode as garbage with nothing to catch it.
	constexpr uint32_t uNumVerts = 3u;

	Flux_MeshGeometry xGeometry;
	xGeometry.m_uNumVerts = uNumVerts;
	xGeometry.m_pxPositions  = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometry.m_pxUVs        = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	xGeometry.m_pxNormals    = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometry.m_pxTangents   = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometry.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometry.m_pxColors     = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));

	for (uint32_t v = 0; v < uNumVerts; v++)
	{
		const float fV = static_cast<float>(v);
		xGeometry.m_pxPositions[v]  = Zenith_Maths::Vector3(fV, -fV, 1.0e-24f * (fV + 1.0f));
		xGeometry.m_pxUVs[v]        = Zenith_Maths::Vector2(0.25f * fV, -0.0f);
		xGeometry.m_pxNormals[v]    = Zenith_Maths::Vector3(0.0f, 0.6f, -0.8f);
		xGeometry.m_pxTangents[v]   = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		// Mirrored on the odd vertices — the handedness the tangent's w has to carry.
		xGeometry.m_pxBitangents[v] = Zenith_Maths::Vector3(0.0f, (v % 2u) ? -0.8f : 0.8f, -0.6f);
		xGeometry.m_pxColors[v]     = Zenith_Maths::Vector4(1.0f, 0.5f, 0.25f, 0.125f * fV);
	}

	// The derived FIRST, then the mesh-pipeline form over the same geometry: a caller
	// may legitimately take both, and the second must fully replace the first.
	xGeometry.GenerateLayoutAndVertexData();
	xGeometry.GenerateMeshPipelineVertexData();

	Interleave_Sources xSrc;
	xSrc.m_pxPositions  = xGeometry.m_pxPositions;
	xSrc.m_pxUVs        = xGeometry.m_pxUVs;
	xSrc.m_pxNormals    = xGeometry.m_pxNormals;
	xSrc.m_pxTangents   = xGeometry.m_pxTangents;
	xSrc.m_pxBitangents = xGeometry.m_pxBitangents;
	xSrc.m_pxColours    = xGeometry.m_pxColors;

	uint8_t auReference[uNumVerts * uINTERLEAVE_STATIC_STRIDE];
	std::memset(auReference, 0xCD, sizeof(auReference));
	Interleave_CodecReference(auReference, xSrc, uNumVerts, /*bSkinned*/ false);

	ZENITH_ASSERT_EQ(xGeometry.GetBufferLayout().GetStride(), uINTERLEAVE_STATIC_STRIDE,
		"the mesh-pipeline layout's stride is the reflected one");
	ZENITH_ASSERT_EQ(std::memcmp(xGeometry.m_pVertexData, auReference, sizeof(auReference)), 0,
		"the mesh-pipeline procedural vertex data differs from the codec reference");
}

// ---- the bind-pose path's one difference ------------------------------------

ZENITH_TEST(VertexInterleave, PositionOverrideReplacesOnlyThePositions)
{
	// The bind-pose-baked mesh path pre-skins the positions and hands the RESULT to
	// the same packer. This pins that the override reaches the position element and
	// touches nothing else — a swap that also moved, say, the normals would render a
	// mis-lit mesh that no stride or offset check would catch.
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f));
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	const Zenith_Maths::Vector3 axOverride[3] =
	{
		Zenith_Maths::Vector3(-1.0f, -2.0f, -3.0f),
		Zenith_Maths::Vector3(-4.0f, -5.0f, -6.0f),
		Zenith_Maths::Vector3(-7.0f, -8.0f, -9.0f),
	};

	uint8_t auBuf[3u * uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 3u, axOverride);

	for (uint32_t v = 0; v < 3u; v++)
	{
		// Every value here is a small whole number, exact in half.
		const Zenith_Maths::Vector4 xPos = Flux_UnpackHalf4(Interleave_VtxWord64(auBuf, uINTERLEAVE_STATIC_STRIDE, v, uINTERLEAVE_OFF_POSITION));
		ZENITH_ASSERT_TRUE(xPos.x == axOverride[v].x && xPos.y == axOverride[v].y && xPos.z == axOverride[v].z,
			"vertex %u takes the overridden position", v);

		const Zenith_Maths::Vector4 xNormal = Flux_UnpackSnorm10_10_10_2(Interleave_VtxWord(auBuf, uINTERLEAVE_STATIC_STRIDE, v, uINTERLEAVE_OFF_NORMAL));
		ZENITH_ASSERT_TRUE(xNormal.z > 0.99f && xNormal.x == 0.0f && xNormal.y == 0.0f,
			"vertex %u keeps the asset's authored normal", v);
	}

	// And with no override the asset's own positions come through, so the parameter
	// is genuinely opt-in rather than a permanently-engaged detour.
	uint8_t auPlain[3u * uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auPlain, xAsset, 3u);
	const Zenith_Maths::Vector4 xPlain = Flux_UnpackHalf4(Interleave_VtxWord64(auPlain, uINTERLEAVE_STATIC_STRIDE, 2u, uINTERLEAVE_OFF_POSITION));
	ZENITH_ASSERT_TRUE(xPlain.x == 7.0f && xPlain.z == 9.0f, "no override -> the asset's positions");
}
