#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "AssetHandling/Zenith_MeshAsset.h"

// ============================================================================
// Flux_InterleaveMeshVertices unit tests (pure / headless).
//
// The interleaver is the single source of the engine's standard vertex layout
// (72 B static / 104 B skinned). These tests pin the exact byte layout AND the
// missing-attribute defaults so a future layout edit can't silently diverge the
// three call sites it replaced (CreateFromAsset / CreateSkinnedFromAsset /
// Flux_RealBuildSkinnedPose). Headless: stack-constructed Zenith_MeshAsset with
// CPU attribute vectors only — no GPU / registry / engine boot.
// ============================================================================

namespace
{
	// Read the fIdx-th float of vertex v in an interleaved buffer of the given stride.
	float Interleave_VtxF(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t fIdx)
	{
		const float* pF = reinterpret_cast<const float*>(p + static_cast<size_t>(v) * uStride);
		return pF[fIdx];
	}
	// Read the uIdx-th uint at uByteOffset within vertex v.
	uint32_t Interleave_VtxU(const uint8_t* p, uint32_t uStride, uint32_t v, uint32_t uByteOffset, uint32_t uIdx)
	{
		const uint32_t* pU = reinterpret_cast<const uint32_t*>(p + static_cast<size_t>(v) * uStride + uByteOffset);
		return pU[uIdx];
	}
}

ZENITH_TEST(VertexInterleave, StaticLayoutPacksAllAttributesInOrder)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xAsset.m_xUVs.PushBack(Zenith_Maths::Vector2(4.0f, 5.0f));
	xAsset.m_xNormals.PushBack(Zenith_Maths::Vector3(6.0f, 7.0f, 8.0f));
	xAsset.m_xTangents.PushBack(Zenith_Maths::Vector3(9.0f, 10.0f, 11.0f));
	xAsset.m_xBitangents.PushBack(Zenith_Maths::Vector3(12.0f, 13.0f, 14.0f));
	xAsset.m_xColors.PushBack(Zenith_Maths::Vector4(15.0f, 16.0f, 17.0f, 18.0f));

	uint8_t auBuf[72] = {};
	Flux_InterleaveMeshVertices(auBuf, xAsset, 1u, /*bSkinned*/ false);

	// Layout: pos(0-2) uv(3-4) normal(5-7) tangent(8-10) bitangent(11-13) color(14-17)
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

	uint8_t auBuf[104] = {};
	Flux_InterleaveMeshVertices(auBuf, xAsset, 1u, /*bSkinned*/ true);

	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 104, 0, 0) == 1.0f && Interleave_VtxF(auBuf, 104, 0, 2) == 3.0f, "skinned position still at words 0-2");
	// BoneIndices: 4 uints at byte offset 72
	ZENITH_ASSERT_EQ(Interleave_VtxU(auBuf, 104, 0, 72, 0), 10u, "bone index 0 at byte 72");
	ZENITH_ASSERT_EQ(Interleave_VtxU(auBuf, 104, 0, 72, 3), 40u, "bone index 3 at byte 72");
	// BoneWeights: 4 floats at byte offset 88 (= words 22-25)
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 104, 0, 22) == 0.1f && Interleave_VtxF(auBuf, 104, 0, 25) == 0.4f, "bone weights at words 22 and 25");
}

ZENITH_TEST(VertexInterleave, MissingAttributesUseCanonicalDefaults)
{
	Zenith_MeshAsset xAsset;
	xAsset.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));   // positions only

	uint8_t auBuf[72] = {};
	Flux_InterleaveMeshVertices(auBuf, xAsset, 1u, /*bSkinned*/ false);

	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 3) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 4) == 0.0f, "default uv (0,0)");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 5) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 6) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 7) == 0.0f, "default normal +Y");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 8) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 9) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 10) == 0.0f, "default tangent +X");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 11) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 12) == 0.0f && Interleave_VtxF(auBuf, 72, 0, 13) == 1.0f, "default bitangent +Z");
	ZENITH_ASSERT_TRUE(Interleave_VtxF(auBuf, 72, 0, 14) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 15) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 16) == 1.0f && Interleave_VtxF(auBuf, 72, 0, 17) == 1.0f, "default white color");
}
