#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>

// Unit coverage for Zenith_MeshAsset::GenerateUnitSphere (offline CPU sphere used by
// the tennis ball export). All assertions use the public mesh arrays — no GPU.

ZENITH_TEST(MeshAsset, GenerateUnitSphere_CountsAndBounds)
{
	const uint32_t uSeg = 16;   // even
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitSphere(xMesh, uSeg);

	const uint32_t uExpectedVerts = (uSeg + 1) * (uSeg * 2 + 1);
	const uint32_t uExpectedIndices = uSeg * (uSeg * 2) * 6;
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uExpectedVerts, "UV-sphere vertex count");
	ZENITH_ASSERT_EQ(xMesh.GetNumIndices(), uExpectedIndices, "UV-sphere index count");
	// All six vertex arrays must be the same length (bitangent pushed in parallel).
	ZENITH_ASSERT_EQ(xMesh.m_xPositions.GetSize(), uExpectedVerts, "positions size");
	ZENITH_ASSERT_EQ(xMesh.m_xNormals.GetSize(), uExpectedVerts, "normals size");
	ZENITH_ASSERT_EQ(xMesh.m_xTangents.GetSize(), uExpectedVerts, "tangents size");
	ZENITH_ASSERT_EQ(xMesh.m_xBitangents.GetSize(), uExpectedVerts, "bitangents size");
	ZENITH_ASSERT_EQ(xMesh.m_xUVs.GetSize(), uExpectedVerts, "uvs size");
	ZENITH_ASSERT_EQ(xMesh.m_xColors.GetSize(), uExpectedVerts, "colors size");

	// Even segment count => a ring sits exactly on the equator, so |x|,|y|,|z| reach
	// exactly 0.5 and the bounds are a tight [-0.5, 0.5] cube (radius 0.5).
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMin(), Zenith_Maths::Vector3(-0.5f), 1e-4f, "sphere bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMax(), Zenith_Maths::Vector3(0.5f), 1e-4f, "sphere bounds max");
}

ZENITH_TEST(MeshAsset, GenerateUnitSphere_LiteralCountsSmall)
{
	// Independent oracle: hard-coded expected counts for a small even segment count,
	// so a mirrored-formula bug in the implementation can't make the test agree with
	// itself. seg=4 -> lat=4, lon=8 -> verts=(4+1)*(8+1)=45, indices=4*8*6=192.
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitSphere(xMesh, 4);
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), 45u, "seg=4 sphere vertex count");
	ZENITH_ASSERT_EQ(xMesh.GetNumIndices(), 192u, "seg=4 sphere index count");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMin(), Zenith_Maths::Vector3(-0.5f), 1e-4f, "seg=4 bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMax(), Zenith_Maths::Vector3(0.5f), 1e-4f, "seg=4 bounds max");
}

ZENITH_TEST(MeshAsset, GenerateUnitSphere_AnalyticAttributes)
{
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitSphere(xMesh, 16);

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const Zenith_Maths::Vector3& xPos = xMesh.m_xPositions.Get(u);
		const Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(u);
		const Zenith_Maths::Vector3& xT = xMesh.m_xTangents.Get(u);

		// Radial normal: finite, unit length, and pointing along the position.
		const float fNLen = glm::length(xN);
		ZENITH_ASSERT_TRUE(std::isfinite(fNLen) && std::abs(fNLen - 1.0f) < 1e-3f,
			"sphere normal must be unit length");
		// pos = 0.5 * normal, so normalize(pos) == normal (away from the poles where
		// pos is still radial).
		ZENITH_ASSERT_NEAR_VEC3(glm::normalize(xPos), xN, 1e-3f, "normal must be radial");

		// Tangent orthogonal to the normal (longitude derivative).
		ZENITH_ASSERT_TRUE(std::abs(glm::dot(xN, xT)) < 1e-3f,
			"tangent must be orthogonal to normal");
	}

	// Every index references a real vertex.
	for (uint32_t u = 0; u < xMesh.GetNumIndices(); ++u)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "index in range");
	}
}
