#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"        // the procedural path's GenerateLayoutAndVertexData
#include "Flux/MeshGeometry/Flux_MeshInstance.h"       // Flux_PackStaticMeshVertices (the 72B static stream)
#include "Flux/UnifiedMesh/Flux_Skinning.h"            // Flux_SkinInputVertex + Flux_BuildSkinInputVertices (104B)
#include "Flux/Shaders/Generated/UnifiedMesh.h"        // THE reflected table the static path is shaped by
#include "AssetHandling/Zenith_MeshAsset.h"

#include <cstring>   // std::memcmp / std::memset — the golden comparison

// ============================================================================
// Mesh-family vertex-stream tests (pure / headless).
//
// The engine's two mesh streams now have exactly one writer each:
//   * the 72-byte STATIC vertex  -> Flux_PackStaticMeshVertices, which packs
//     through Flux_PackVertices shaped by the SHADER-REFLECTED table
//     Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout;
//   * the 104-byte SKIN-INPUT vertex -> Flux_BuildSkinInputVertices, which owns
//     the Flux_SkinInputVertex contract (its bone lanes are uint palette indices
//     with no vertex semantic, so they are deliberately NOT packer work).
// Between them they replaced four hand-written interleave loops.
//
// HOW THESE PROVE THE REPLACEMENT CHANGED NO BYTES: the loop body that used to
// live in Flux_InterleaveMeshVertices is transcribed below, verbatim and frozen,
// as Interleave_LegacyReference. Every test drives the real writer and the frozen
// reference over the SAME mesh and memcmps the results. Bit-exact comparison is
// legal here because both sides only COPY floats — no arithmetic, so no
// fast-math latitude — and it is what catches a difference that `==` would not
// (a -0.0f sign bit, a byte left unwritten in a hole).
//
// The adversarial matrix below is the point: missing attributes, arrays one entry
// SHORT of the vertex count (the `GetSize() >= uNumVerts` presence rule),
// degenerate zero-length frames, a bone-weight array that runs out, negative
// zero, tiny and large magnitudes, and always >= 3 vertices so a wrong stride
// cannot hide. Headless: stack-constructed Zenith_MeshAsset with CPU attribute
// vectors only — no GPU, no registry, no engine boot.
// ============================================================================

namespace
{
	constexpr uint32_t uINTERLEAVE_STATIC_STRIDE  = 72u;
	constexpr uint32_t uINTERLEAVE_SKINNED_STRIDE = 104u;

	// The reference below hard-codes the strides the OLD loop hard-coded. These
	// asserts are what tie that frozen transcription to the tables the production
	// writers actually use — if the reflected layout or the skin-input contract ever
	// moves, this file fails to compile instead of silently comparing two shapes.
	static_assert(Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout.m_auStrides[0] == uINTERLEAVE_STATIC_STRIDE,
		"the reflected static-mesh layout is no longer 72 bytes — the frozen legacy reference below cannot be the golden");
	static_assert(Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout.m_uElementCount == 6u,
		"the reflected static-mesh layout no longer declares 6 attributes");
	static_assert(sizeof(Flux_SkinInputVertex) == uINTERLEAVE_SKINNED_STRIDE,
		"the skin-input vertex is no longer 104 bytes — the frozen legacy reference below cannot be the golden");

	// ---- THE FROZEN GOLDEN ---------------------------------------------------
	//
	// Verbatim transcription of Flux_InterleaveMeshVertices as it stood before the
	// packer replaced it (Flux_MeshInstance.cpp, phase 3). It is a TEST FIXTURE, not
	// engine code: nothing outside this file may call it, and it must NEVER be
	// "improved", re-tidied, or re-pointed at a helper — its whole value is that it
	// is the old bytes, written the old way, frozen. A behaviour change that is
	// genuinely intended is landed by changing this reference IN THE SAME COMMIT as
	// the writer, which is exactly the review conversation that should happen.
	void Interleave_LegacyReference(uint8_t* pDst, const Zenith_MeshAsset& xAsset, uint32_t uNumVerts, bool bSkinned)
	{
		const uint32_t uStride = bSkinned ? 104u : 72u;

		const bool bHasPositions   = xAsset.m_xPositions.GetSize()   >= uNumVerts;
		const bool bHasUVs         = xAsset.m_xUVs.GetSize()         >= uNumVerts;
		const bool bHasNormals     = xAsset.m_xNormals.GetSize()     >= uNumVerts;
		const bool bHasTangents    = xAsset.m_xTangents.GetSize()    >= uNumVerts;
		const bool bHasBitangents  = xAsset.m_xBitangents.GetSize()  >= uNumVerts;
		const bool bHasColors      = xAsset.m_xColors.GetSize()      >= uNumVerts;
		const bool bHasBoneIndices = bSkinned && xAsset.m_xBoneIndices.GetSize() >= uNumVerts;
		const bool bHasBoneWeights = bSkinned && xAsset.m_xBoneWeights.GetSize() >= uNumVerts;

		const Zenith_Maths::Vector3 xDefaultPosition(0.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector2 xDefaultUV(0.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultNormal(0.0f, 1.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultTangent(1.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector3 xDefaultBitangent(0.0f, 0.0f, 1.0f);
		const Zenith_Maths::Vector4 xDefaultColor(1.0f, 1.0f, 1.0f, 1.0f);
		const glm::uvec4 xDefaultBoneIndices(0u, 0u, 0u, 0u);
		const glm::vec4  xDefaultBoneWeights(0.0f, 0.0f, 0.0f, 0.0f);

		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			uint8_t* pVtx = pDst + static_cast<size_t>(i) * uStride;
			float* pFloatData = reinterpret_cast<float*>(pVtx);
			size_t uFloatIndex = 0;

			const Zenith_Maths::Vector3& xPos = bHasPositions ? xAsset.m_xPositions.Get(i) : xDefaultPosition;
			pFloatData[uFloatIndex++] = xPos.x;
			pFloatData[uFloatIndex++] = xPos.y;
			pFloatData[uFloatIndex++] = xPos.z;

			const Zenith_Maths::Vector2& xUV = bHasUVs ? xAsset.m_xUVs.Get(i) : xDefaultUV;
			pFloatData[uFloatIndex++] = xUV.x;
			pFloatData[uFloatIndex++] = xUV.y;

			const Zenith_Maths::Vector3& xNormal = bHasNormals ? xAsset.m_xNormals.Get(i) : xDefaultNormal;
			pFloatData[uFloatIndex++] = xNormal.x;
			pFloatData[uFloatIndex++] = xNormal.y;
			pFloatData[uFloatIndex++] = xNormal.z;

			const Zenith_Maths::Vector3& xTangent = bHasTangents ? xAsset.m_xTangents.Get(i) : xDefaultTangent;
			pFloatData[uFloatIndex++] = xTangent.x;
			pFloatData[uFloatIndex++] = xTangent.y;
			pFloatData[uFloatIndex++] = xTangent.z;

			const Zenith_Maths::Vector3& xBitangent = bHasBitangents ? xAsset.m_xBitangents.Get(i) : xDefaultBitangent;
			pFloatData[uFloatIndex++] = xBitangent.x;
			pFloatData[uFloatIndex++] = xBitangent.y;
			pFloatData[uFloatIndex++] = xBitangent.z;

			const Zenith_Maths::Vector4& xColor = bHasColors ? xAsset.m_xColors.Get(i) : xDefaultColor;
			pFloatData[uFloatIndex++] = xColor.x;
			pFloatData[uFloatIndex++] = xColor.y;
			pFloatData[uFloatIndex++] = xColor.z;
			pFloatData[uFloatIndex++] = xColor.w;

			if (bSkinned)
			{
				// BoneIndices (4 uints = 16 bytes) at offset 72
				const glm::uvec4& xBoneIndices = bHasBoneIndices ? xAsset.m_xBoneIndices.Get(i) : xDefaultBoneIndices;
				uint32_t* pUintData = reinterpret_cast<uint32_t*>(pVtx + 72);
				pUintData[0] = xBoneIndices.x;
				pUintData[1] = xBoneIndices.y;
				pUintData[2] = xBoneIndices.z;
				pUintData[3] = xBoneIndices.w;

				// BoneWeights (4 floats = 16 bytes) at offset 88
				const glm::vec4& xBoneWeights = bHasBoneWeights ? xAsset.m_xBoneWeights.Get(i) : xDefaultBoneWeights;
				float* pWeightData = reinterpret_cast<float*>(pVtx + 88);
				pWeightData[0] = xBoneWeights.x;
				pWeightData[1] = xBoneWeights.y;
				pWeightData[2] = xBoneWeights.z;
				pWeightData[3] = xBoneWeights.w;
			}
		}
	}

	// ---- readers -------------------------------------------------------------

	// Read the fIdx-th float of vertex v in an interleaved buffer of the given stride.
	float Interleave_VtxF(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t fIdx)
	{
		float fOut = 0.0f;
		std::memcpy(&fOut, p + static_cast<size_t>(v) * uStride + fIdx * sizeof(float), sizeof(fOut));
		return fOut;
	}
	// Read the uIdx-th uint at uByteOffset within vertex v.
	uint32_t Interleave_VtxU(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t uByteOffset, uint32_t uIdx)
	{
		uint32_t uOut = 0u;
		std::memcpy(&uOut, p + static_cast<size_t>(v) * uStride + uByteOffset + uIdx * sizeof(uint32_t), sizeof(uOut));
		return uOut;
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
		// weights: the FLOAT family must pass even nonsense through untouched.
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
	// and deliberately awkward: mixed signs, a negative zero, a denormal-adjacent
	// tiny, a large magnitude, and the 0xFFFFFFFF bone sentinel.
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
				// Mirrored (left-handed) frame on the odd vertices — the handedness a
				// 4-lane tangent would have to carry. Today's table has three lanes, so
				// what must be preserved is simply the authored vector.
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
				xAsset.m_xBoneIndices.PushBack(glm::uvec4(v, v + 1u, 0xFFFFFFFFu, 3u));
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
}

// ---- byte-for-byte against the frozen legacy interleave ---------------------

ZENITH_TEST(VertexInterleave, StaticPackMatchesLegacyInterleaveByteForByte)
{
	for (uint32_t uCase = 0; uCase < uINTERLEAVE_CASE_COUNT; uCase++)
	{
		Zenith_MeshAsset xAsset;
		const uint32_t uNumVerts = Interleave_BuildCase(uCase, xAsset);

		uint8_t auLegacy[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_STATIC_STRIDE];
		uint8_t auPacked[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_STATIC_STRIDE];
		std::memset(auLegacy, 0xCD, sizeof(auLegacy));
		std::memset(auPacked, 0xCD, sizeof(auPacked));

		Interleave_LegacyReference(auLegacy, xAsset, uNumVerts, /*bSkinned*/ false);
		Flux_PackStaticMeshVertices(auPacked, xAsset, uNumVerts);

		const size_t uBytes = static_cast<size_t>(uNumVerts) * uINTERLEAVE_STATIC_STRIDE;
		ZENITH_ASSERT_EQ(std::memcmp(auLegacy, auPacked, uBytes), 0,
			"case '%s': the reflected-table pack differs from the frozen 72-byte interleave", kaxINTERLEAVE_CASES[uCase].m_szName);
	}
}

ZENITH_TEST(VertexInterleave, SkinInputMatchesLegacyInterleaveByteForByte)
{
	for (uint32_t uCase = 0; uCase < uINTERLEAVE_CASE_COUNT; uCase++)
	{
		Zenith_MeshAsset xAsset;
		const uint32_t uNumVerts = Interleave_BuildCase(uCase, xAsset);

		uint8_t auLegacy[uINTERLEAVE_MAX_VERTS * uINTERLEAVE_SKINNED_STRIDE];
		Flux_SkinInputVertex axBuilt[uINTERLEAVE_MAX_VERTS];
		std::memset(auLegacy, 0xCD, sizeof(auLegacy));
		std::memset(axBuilt, 0xCD, sizeof(axBuilt));

		Interleave_LegacyReference(auLegacy, xAsset, uNumVerts, /*bSkinned*/ true);
		Flux_BuildSkinInputVertices(axBuilt, xAsset, uNumVerts);

		const size_t uBytes = static_cast<size_t>(uNumVerts) * uINTERLEAVE_SKINNED_STRIDE;
		ZENITH_ASSERT_EQ(std::memcmp(auLegacy, axBuilt, uBytes), 0,
			"case '%s': the skin-input build differs from the frozen 104-byte interleave", kaxINTERLEAVE_CASES[uCase].m_szName);
	}
}

// ---- the layout the two streams are pinned to ------------------------------

ZENITH_TEST(VertexInterleave, StaticLayoutPacksAllAttributesInOrder)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAsset.m_xUVs.PushBack(Zenith_Maths::Vector2(4.0f, 5.0f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(6.0f, 7.0f, 8.0f));
	xAsset.m_xTangents.PushBack(Zenith_Maths::Vector3(9.0f, 10.0f, 11.0f));
	xAsset.m_xBitangents.PushBack(Zenith_Maths::Vector3(12.0f, 13.0f, 14.0f));
	xAsset.m_xColors.PushBack(Zenith_Maths::Vector4(15.0f, 16.0f, 17.0f, 18.0f));

	uint8_t auBuf[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 1u);

	// Layout: pos(0-2) uv(3-4) normal(5-7) tangent(8-10) bitangent(11-13) color(14-17).
	// Spelled as WORD INDICES, not read back out of the table, so a table whose
	// offsets moved fails here instead of agreeing with itself.
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 0) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 1) == 2.0f && Interleave_VtxF(auBuf, 72, 0, 2) == 3.0f, "position at words 0-2");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 3) == 4.0f && Interleave_VtxF(auBuf, 72, 0, 4) == 5.0f, "uv at words 3-4");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 5) == 6.0f && Interleave_VtxF(auBuf, 72, 0, 6) == 7.0f && Interleave_VtxF(auBuf, 72, 0, 7) == 8.0f, "normal at words 5-7");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 8) == 9.0f && Interleave_VtxF(auBuf, 72, 0, 9) == 10.0f && Interleave_VtxF(auBuf, 72, 0, 10) == 11.0f, "tangent at words 8-10");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 11) == 12.0f && Interleave_VtxF(auBuf, 72, 0, 12) == 13.0f && Interleave_VtxF(auBuf, 72, 0, 13) == 14.0f, "bitangent at words 11-13");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 14) == 15.0f && Interleave_VtxF(auBuf, 72, 0, 15) == 16.0f && Interleave_VtxF(auBuf, 72, 0, 16) == 17.0f && Interleave_VtxF(auBuf, 72, 0, 17) == 18.0f, "color at words 14-17");
}

ZENITH_TEST(VertexInterleave, SkinnedLayoutAppendsBoneIndicesAndWeights)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAsset.m_xBoneIndices.PushBack(glm::uvec4(10u, 20u, 30u, 40u));
	xAsset.m_xBoneWeights.PushBack(glm::vec4(0.1f, 0.2f, 0.3f, 0.4f));

	Flux_SkinInputVertex axBuilt[1] = {};
	Flux_BuildSkinInputVertices(axBuilt, xAsset, 1u);
	const uint8_t* const pBytes = reinterpret_cast<const uint8_t*>(axBuilt);

	ZENITH_ASSERT_TRUE(Interleave_VtxF(pBytes, 104, 0, 0) == 1.0f && Interleave_VtxF(pBytes, 104, 0, 2) == 3.0f, "skinned position still at words 0-2");
	// BoneIndices: 4 uints at byte offset 72
	ZENITH_ASSERT_EQ(Interleave_VtxU(pBytes, 104, 0, 72, 0), 10u, "bone index 0 at byte 72");
	ZENITH_ASSERT_EQ(Interleave_VtxU(pBytes, 104, 0, 72, 3), 40u, "bone index 3 at byte 72");
	// BoneWeights: 4 floats at byte offset 88 (= words 22-25)
	ZENITH_ASSERT_TRUE(Interleave_VtxF(pBytes, 104, 0, 22) == 0.1f && Interleave_VtxF(pBytes, 104, 0, 25) == 0.4f, "bone weights at words 22 and 25");
}

ZENITH_TEST(VertexInterleave, MissingAttributesUseCanonicalDefaults)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));   // positions only

	uint8_t auBuf[uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auBuf, xAsset, 1u);

	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 3) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 4) == 0.0f, "default uv (0,0)");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 5) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 6) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 7) == 0.0f, "default normal +Y");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 8) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 9) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 10) == 0.0f, "default tangent +X");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 11) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 12) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 13) == 1.0f, "default bitangent +Z");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 14) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 15) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 16) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 17) == 1.0f, "default white color");
}

// ---- the procedural-geometry path ------------------------------------------

ZENITH_TEST(VertexInterleave, ProceduralGeometryVertexDataMatchesLegacyLaneLoop)
{
	// Flux_MeshGeometry::GenerateLayoutAndVertexData is the one replaced loop whose
	// layout is DYNAMIC (elements exist only for the streams that do), so it feeds
	// the packer a table built at runtime and reads the byte offsets back out of the
	// Flux_BufferLayout it just declared — the table the .zmesh carries. That
	// derivation is new, unlike the verbatim transcriptions above, which is exactly
	// why it needs its own golden: the reference below is the old lane-by-lane
	// `index++` loop, which knew nothing about offsets at all.
	constexpr uint32_t uNumVerts = 3u;

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

		// THE FROZEN GOLDEN, second form: the old lane-by-lane loop, which wrote one
		// running float index straight through every vertex. Same rules as the
		// transcription at the top of this file — it is frozen, not maintained.
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

		// The dynamic layout tight-packs to the same strides the mesh-asset streams
		// use, which is not a coincidence worth leaving unstated: it is why a
		// procedural mesh and an imported one are fetched by the same pipeline.
		const uint32_t uExpectedStride = (uWithBones != 0u) ? uINTERLEAVE_SKINNED_STRIDE : uINTERLEAVE_STATIC_STRIDE;
		ZENITH_ASSERT_EQ(xGeometry.GetBufferLayout().GetStride(), uExpectedStride,
			"bones=%u: the declared layout's stride", uWithBones);
		ZENITH_ASSERT_EQ(std::memcmp(xGeometry.m_pVertexData, afLegacyStorage, static_cast<size_t>(uNumVerts) * uExpectedStride), 0,
			"bones=%u: the packed procedural vertex data differs from the frozen lane loop", uWithBones);
	}
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
		ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, v, 0) == axOverride[v].x
			&& Interleave_VtxF(auBuf, 72, v, 1) == axOverride[v].y
			&& Interleave_VtxF(auBuf, 72, v, 2) == axOverride[v].z,
			"vertex %u takes the overridden position", v);
		ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, v, 5) == 0.0f
			&& Interleave_VtxF(auBuf, 72, v, 6) == 0.0f
			&& Interleave_VtxF(auBuf, 72, v, 7) == 1.0f,
			"vertex %u keeps the asset's authored normal", v);
	}

	// And with no override the asset's own positions come through, so the parameter
	// is genuinely opt-in rather than a permanently-engaged detour.
	uint8_t auPlain[3u * uINTERLEAVE_STATIC_STRIDE] = {};
	Flux_PackStaticMeshVertices(auPlain, xAsset, 3u);
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auPlain, 72, 2, 0) == 7.0f
		&& Interleave_VtxF(auPlain, 72, 2, 2) == 9.0f, "no override -> the asset's positions");
}
