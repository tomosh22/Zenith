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

// ============================================================================
// GenerateTangents -- the tangent frame, and the two ways it used to produce NaN.
//
// ★★ WHY THESE EXIST. GenerateTangents rejected a triangle whose UV determinant
// was below 1e-4. That number is a UV *AREA*, so it was really a limit on how
// finely a mesh may be unwrapped: on a 2048^2 atlas a 30x30-texel triangle has
// |det| ~ 2.1e-4 and was already at the cutoff. Every vertex whose incident
// triangles were all rejected kept a zero accumulated tangent, and normalize()
// turned it into NaN. MEASURED on Zenithmon's four imported .glb props, 36-75%
// of their vertices shipped with NaN tangents.
//
// ★ AND IT WAS INVISIBLE, which is the reason to pin it with units rather than a
// screenshot. Flux_PackVertices sanitises a non-finite direction to the
// semantic's canonical default, so the mesh renders with a world-constant
// tangent instead of rendering as an obvious NaN. Nothing failed; the lighting
// was just wrong.
// ============================================================================

namespace
{
	// A quad on the XZ plane (normal +Y) whose UVs span fUVScale. Small values put
	// the UV determinant far below the old cutoff, which is the whole point.
	void MTBuildUVScaledQuad(Zenith_MeshAsset& xMesh, float fUVScale)
	{
		xMesh.Reset();
		xMesh.Reserve(4, 6);
		const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xUp, Zenith_Maths::Vector2(0.0f, 0.0f));
		xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), xUp, Zenith_Maths::Vector2(fUVScale, 0.0f));
		xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f), xUp, Zenith_Maths::Vector2(fUVScale, fUVScale));
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xUp, Zenith_Maths::Vector2(0.0f, fUVScale));
		xMesh.AddTriangle(0, 1, 2);
		xMesh.AddTriangle(0, 2, 3);
	}

	bool MTAllFinite(const Zenith_MeshAsset& xMesh)
	{
		for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
		{
			const Zenith_Maths::Vector3& xT = xMesh.m_xTangents.Get(u);
			const Zenith_Maths::Vector3& xB = xMesh.m_xBitangents.Get(u);
			if (!std::isfinite(xT.x) || !std::isfinite(xT.y) || !std::isfinite(xT.z)) return false;
			if (!std::isfinite(xB.x) || !std::isfinite(xB.y) || !std::isfinite(xB.z)) return false;
		}
		return true;
	}
}

// ★★ THE REGRESSION ITSELF. A 1e-3 UV span gives |det| = 1e-6 per triangle --
// far below the old 1e-4 cutoff, and comfortably inside what a 2048^2 atlas
// produces -- so every triangle was skipped and every tangent came out NaN. This
// test FAILS on the old code and is the reason the cutoff moved.
ZENITH_TEST(MeshAsset, GenerateTangents_SmallUVTrianglesAreNotDegenerate)
{
	Zenith_MeshAsset xMesh;
	MTBuildUVScaledQuad(xMesh, 1.0e-3f);
	xMesh.GenerateTangents();

	ZENITH_ASSERT_TRUE(MTAllFinite(xMesh),
		"a finely-unwrapped mesh must still get finite tangents (|det| = 1e-6 here, "
		"which the old 1e-4 area cutoff discarded as degenerate)");

	// UVs run +U along +X, so the tangent must too -- not merely be non-NaN.
	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const Zenith_Maths::Vector3& xT = xMesh.m_xTangents.Get(u);
		ZENITH_ASSERT_EQ_FLOAT(xT.x, 1.0f, 1.0e-4f,
			"tangent must follow +U along +X regardless of how small the UV triangle is");
	}
}

// The SCALE of the UVs must not change the frame at all -- only its direction is
// meaningful. Spanning 1.0 and spanning 1e-4 describe the same surface.
ZENITH_TEST(MeshAsset, GenerateTangents_FrameIsIndependentOfUVScale)
{
	Zenith_MeshAsset xCoarse, xFine;
	MTBuildUVScaledQuad(xCoarse, 1.0f);
	MTBuildUVScaledQuad(xFine,   1.0e-4f);
	xCoarse.GenerateTangents();
	xFine.GenerateTangents();

	for (uint32_t u = 0; u < xCoarse.GetNumVerts(); ++u)
	{
		ZENITH_ASSERT_NEAR_VEC3(xFine.m_xTangents.Get(u), xCoarse.m_xTangents.Get(u), 1.0e-4f,
			"tangent must not depend on UV scale");
		ZENITH_ASSERT_NEAR_VEC3(xFine.m_xBitangents.Get(u), xCoarse.m_xBitangents.Get(u), 1.0e-4f,
			"bitangent must not depend on UV scale");
	}
}

// A genuinely UV-degenerate mesh (every vertex on one UV point) has no
// parameterisation to derive a tangent from. It must still come back with a real
// orthonormal frame rather than NaN -- 15 vertices across Zenithmon's shipped
// imports land here even after the cutoff fix.
ZENITH_TEST(MeshAsset, GenerateTangents_DegenerateUVsFallBackToAnOrthonormalFrame)
{
	Zenith_MeshAsset xMesh;
	MTBuildUVScaledQuad(xMesh, 0.0f);   // all four UVs identical -> det == 0
	xMesh.GenerateTangents();

	ZENITH_ASSERT_TRUE(MTAllFinite(xMesh),
		"degenerate UVs must yield a fallback frame, never NaN");

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		const Zenith_Maths::Vector3& xT = xMesh.m_xTangents.Get(u);
		const Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(u);
		ZENITH_ASSERT_EQ_FLOAT(glm::length(xT), 1.0f, 1.0e-5f, "fallback tangent must be unit length");
		ZENITH_ASSERT_EQ_FLOAT(glm::dot(xT, xN), 0.0f, 1.0e-5f, "fallback tangent must be orthogonal to the normal");
	}
}

// ★ THE FALLBACK MUST NOT COLLAPSE ON AN AXIS-ALIGNED NORMAL, which is exactly
// where the packer's canonical default tangent (1,0,0) does: a wall facing +X
// gets cross(N, T) == 0 and no basis at all. Swept over all six axes.
ZENITH_TEST(MeshAsset, GenerateTangents_FallbackHoldsForEveryAxisAlignedNormal)
{
	const Zenith_Maths::Vector3 axNormals[6] = {
		Zenith_Maths::Vector3( 1.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3( 0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3( 0.0f,-1.0f, 0.0f),
		Zenith_Maths::Vector3( 0.0f, 0.0f, 1.0f), Zenith_Maths::Vector3( 0.0f, 0.0f,-1.0f),
	};

	for (const Zenith_Maths::Vector3& xN : axNormals)
	{
		Zenith_MeshAsset xMesh;
		xMesh.Reset();
		xMesh.Reserve(3, 3);
		// Degenerate UVs, so the fallback is what produces the frame.
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xN, Zenith_Maths::Vector2(0.5f, 0.5f));
		xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), xN, Zenith_Maths::Vector2(0.5f, 0.5f));
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xN, Zenith_Maths::Vector2(0.5f, 0.5f));
		xMesh.AddTriangle(0, 1, 2);
		xMesh.GenerateTangents();

		ZENITH_ASSERT_TRUE(MTAllFinite(xMesh), "axis-aligned normal must still get a finite frame");
		for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
		{
			const Zenith_Maths::Vector3& xT = xMesh.m_xTangents.Get(u);
			const Zenith_Maths::Vector3& xB = xMesh.m_xBitangents.Get(u);
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xT), 1.0f, 1.0e-5f, "fallback tangent unit length");
			// The bitangent is what a collapsed frame loses first: cross(N,T) of two
			// parallel vectors is the zero vector, and every consumer normalizes it.
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xB), 1.0f, 1.0e-5f,
				"cross(N,T) must be a real direction, not the collapsed zero vector");
			ZENITH_ASSERT_EQ_FLOAT(glm::dot(xT, xN), 0.0f, 1.0e-5f, "fallback tangent orthogonal to normal");
		}
	}
}

// A mesh whose triangles all clear the OLD cutoff must bake exactly what it baked
// before -- that is what makes this fix safe for the generated props, the terrain
// and every other game's assets. Pinned against the closed-form answer for a quad
// whose UVs run +U along +X and +V along +Z.
ZENITH_TEST(MeshAsset, GenerateTangents_CoarseUVsAreUnchangedByTheFix)
{
	Zenith_MeshAsset xMesh;
	MTBuildUVScaledQuad(xMesh, 1.0f);
	xMesh.GenerateTangents();

	for (uint32_t u = 0; u < xMesh.GetNumVerts(); ++u)
	{
		ZENITH_ASSERT_NEAR_VEC3(xMesh.m_xTangents.Get(u),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1.0e-5f,
			"a coarsely-unwrapped quad must keep the tangent it always had");
		// B = cross(N, T) = cross(+Y, +X) = -Z, the engine's fixed handedness.
		ZENITH_ASSERT_NEAR_VEC3(xMesh.m_xBitangents.Get(u),
			Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f), 1.0e-5f,
			"bitangent stays cross(N, T)");
	}
}
