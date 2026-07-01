#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"
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

// ---------------------------------------------------------------------------
// GenerateUnitCylinder / GenerateUnitCone / GenerateUnitCapsule — the offline CPU
// primitives used by the RenderTest material-showcase export. All assertions read
// the public mesh arrays (no GPU). seg=16 is divisible by 4 so a slice lands on
// each ±X/±Z axis, giving tight [-0.5,0.5] horizontal bounds.
// ---------------------------------------------------------------------------

ZENITH_TEST(MeshAsset, GenerateUnitCylinder_CountsAndBounds)
{
	const uint32_t uSeg = 16;
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitCylinder(xMesh, uSeg);

	// Body (2 rings) + top cap (ring+centre) + bottom cap (ring+centre) = 4*(seg+1).
	const uint32_t uExpectedVerts = 4u * (uSeg + 1u);
	const uint32_t uExpectedIndices = uSeg * 6u + uSeg * 3u * 2u;
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uExpectedVerts, "cylinder vertex count");
	ZENITH_ASSERT_EQ(xMesh.GetNumIndices(), uExpectedIndices, "cylinder index count");
	ZENITH_ASSERT_EQ(xMesh.m_xPositions.GetSize(), uExpectedVerts, "positions size");
	ZENITH_ASSERT_EQ(xMesh.m_xNormals.GetSize(), uExpectedVerts, "normals size");
	ZENITH_ASSERT_EQ(xMesh.m_xTangents.GetSize(), uExpectedVerts, "tangents size");
	ZENITH_ASSERT_EQ(xMesh.m_xBitangents.GetSize(), uExpectedVerts, "bitangents size");
	ZENITH_ASSERT_EQ(xMesh.m_xUVs.GetSize(), uExpectedVerts, "uvs size");
	ZENITH_ASSERT_EQ(xMesh.m_xColors.GetSize(), uExpectedVerts, "colors size");

	// radius 0.5, height 1.0.
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMin(), Zenith_Maths::Vector3(-0.5f, -0.5f, -0.5f), 1e-4f, "cylinder bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMax(), Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f), 1e-4f, "cylinder bounds max");

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const float fNLen = glm::length(xMesh.m_xNormals.Get(u));
		ZENITH_ASSERT_TRUE(std::isfinite(fNLen) && std::abs(fNLen - 1.0f) < 1e-3f, "cylinder normal unit length");
	}
	for (uint32_t u = 0; u < xMesh.GetNumIndices(); ++u)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "cylinder index in range");
	}
}

ZENITH_TEST(MeshAsset, GenerateUnitCone_CountsAndBounds)
{
	const uint32_t uSeg = 16;
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitCone(xMesh, uSeg);

	// Base ring + apex + base centre; side + base triangles.
	const uint32_t uExpectedVerts = uSeg + 2u;
	const uint32_t uExpectedIndices = uSeg * 6u;
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uExpectedVerts, "cone vertex count");
	ZENITH_ASSERT_EQ(xMesh.GetNumIndices(), uExpectedIndices, "cone index count");
	ZENITH_ASSERT_EQ(xMesh.m_xBitangents.GetSize(), uExpectedVerts, "cone bitangents size");

	// radius 0.5; base ring at y=0, apex at y=1 (bounds NOT centred on origin).
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMin(), Zenith_Maths::Vector3(-0.5f, 0.0f, -0.5f), 1e-4f, "cone bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMax(), Zenith_Maths::Vector3(0.5f, 1.0f, 0.5f), 1e-4f, "cone bounds max");

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const float fNLen = glm::length(xMesh.m_xNormals.Get(u));
		ZENITH_ASSERT_TRUE(std::isfinite(fNLen) && std::abs(fNLen - 1.0f) < 1e-3f, "cone normal unit length");
	}
	for (uint32_t u = 0; u < xMesh.GetNumIndices(); ++u)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "cone index in range");
	}
}

ZENITH_TEST(MeshAsset, GenerateUnitCapsule_CountsAndBounds)
{
	const uint32_t uSeg = 16;
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitCapsule(xMesh, uSeg);

	const uint32_t uExpectedVerts = (uSeg + 1u) * (uSeg + 1u);
	const uint32_t uExpectedIndices = uSeg * uSeg * 6u;
	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uExpectedVerts, "capsule vertex count");
	ZENITH_ASSERT_EQ(xMesh.GetNumIndices(), uExpectedIndices, "capsule index count");
	ZENITH_ASSERT_EQ(xMesh.m_xBitangents.GetSize(), uExpectedVerts, "capsule bitangents size");

	// radius 0.5, cylinder height 1.0 -> total height 2.0 (bounds y in [-1, 1]). This
	// radius-0.5/height-2.0 convention is what CreateCapsuleShape fits under a uniform
	// scale — the load-bearing property for the showcase's capsule colliders.
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMin(), Zenith_Maths::Vector3(-0.5f, -1.0f, -0.5f), 1e-4f, "capsule bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xMesh.GetBoundsMax(), Zenith_Maths::Vector3(0.5f, 1.0f, 0.5f), 1e-4f, "capsule bounds max");

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const float fNLen = glm::length(xMesh.m_xNormals.Get(u));
		ZENITH_ASSERT_TRUE(std::isfinite(fNLen) && std::abs(fNLen - 1.0f) < 1e-3f, "capsule normal unit length");
	}
	for (uint32_t u = 0; u < xMesh.GetNumIndices(); ++u)
	{
		ZENITH_ASSERT_LT(xMesh.m_xIndices.Get(u), xMesh.GetNumVerts(), "capsule index in range");
	}
}

ZENITH_TEST(MeshAsset, StreamEnvelopeRoundtrip)
{
	// Build a small mesh, serialize, assert the shared stream envelope is present
	// (Workstream B: .zmesh joined the unified versioning idiom), then read it back
	// through ParseStream and confirm the geometry survives the round-trip.
	Zenith_MeshAsset xMesh;
	Zenith_MeshAsset::GenerateUnitSphere(xMesh, 4);
	xMesh.AddSubmesh(0, xMesh.GetNumIndices(), 0);
	xMesh.ComputeBounds();

	Zenith_DataStream xStream;
	xMesh.WriteToDataStream(xStream);

	// Envelope header written with the right identity.
	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHdr = Zenith_ReadStreamHeader(xStream, uZENITH_MESH_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHdr.IsOk(), "mesh write must emit the shared stream envelope");
	if (xHdr.IsOk())
	{
		ZENITH_ASSERT_EQ(xHdr.Value().m_uAssetTypeId, uZENITH_MESH_ASSET_TYPE_ID, "mesh envelope type id");
		ZENITH_ASSERT_EQ(xHdr.Value().m_uSchemaVersion, uZENITH_MESH_SCHEMA_CURRENT, "mesh envelope schema");
	}

	// Full round-trip through the status-returning parse.
	xStream.SetCursor(0);
	Zenith_MeshAsset xLoaded;
	Zenith_Status xStatus = xLoaded.ParseStream(xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "mesh ParseStream must accept its own output");
	ZENITH_ASSERT_EQ(xLoaded.GetNumVerts(), xMesh.GetNumVerts(), "vert count round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetNumIndices(), xMesh.GetNumIndices(), "index count round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetNumSubmeshes(), xMesh.GetNumSubmeshes(), "submesh count round-trips");
	ZENITH_ASSERT_NEAR_VEC3(xLoaded.GetBoundsMin(), xMesh.GetBoundsMin(), 1e-5f, "bounds min round-trips");
	ZENITH_ASSERT_NEAR_VEC3(xLoaded.GetBoundsMax(), xMesh.GetBoundsMax(), 1e-5f, "bounds max round-trips");
}

ZENITH_TEST(MeshAsset, WrongTypeIdRejected)
{
	// A stream carrying a DIFFERENT asset's envelope must be rejected by the typed
	// parse (INVALID_ARGUMENT), never silently misread. Write a Skeleton-id header
	// then a byte or two, and parse it as a mesh.
	Zenith_DataStream xStream;
	Zenith_WriteStreamHeader(xStream, uZENITH_SKELETON_ASSET_TYPE_ID, uZENITH_SKELETON_SCHEMA_CURRENT);
	xStream << uint32_t(0);
	xStream.SetCursor(0);

	Zenith_MeshAsset xMesh;
	Zenith_Status xStatus = xMesh.ParseStream(xStream);
	ZENITH_ASSERT_FALSE(xStatus.IsOk(), "mesh parse of a skeleton-typed stream must fail");
	ZENITH_ASSERT_TRUE(xStatus.Error() == Zenith_ErrorCode::INVALID_ARGUMENT, "wrong type id -> INVALID_ARGUMENT");
}
