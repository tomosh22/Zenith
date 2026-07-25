#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"
#include "AI/Navigation/Zenith_NavMeshBaker.h"
#include "DataStream/Zenith_DataStream.h"
#include <filesystem>

// ============================================================================
// Zenith_NavMeshBaker tests (SC1b commit B, ZM-D-147)
//
// The baker is the tools-time half of baked-navmesh persistence. What these pin:
// the file it writes is loadable, the bytes are DETERMINISTIC (the contract that
// lets a .znavmesh be committed to git without churning every boot), the counts
// it reports agree with the mesh it hands back, and every rejection is a defined
// typed error rather than a half-written file.
//
// The two CALLER-defect units wrap Zenith_AssertCaptureScope: an empty path and
// empty geometry both assert by design (D6), and an unscoped deliberate assert
// would take down the whole boot gate.
// ============================================================================

namespace
{
	constexpr u_int uNAVMESH_BAKE_TEST_QUADS = 4u;
	constexpr float fNAVMESH_BAKE_TEST_STEP = 1.0f;

	// A flat 4x4 m ground quad grid -- enough surface for the generator's default
	// 0.3 m voxelisation to find walkable spans.
	void NavMeshBakerBuildFlatGround(Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		Zenith_Vector<uint32_t>& auIndices)
	{
		axVertices.Clear();
		auIndices.Clear();

		for (u_int uZ = 0; uZ <= uNAVMESH_BAKE_TEST_QUADS; ++uZ)
		{
			for (u_int uX = 0; uX <= uNAVMESH_BAKE_TEST_QUADS; ++uX)
			{
				axVertices.PushBack(Zenith_Maths::Vector3(
					static_cast<float>(uX) * fNAVMESH_BAKE_TEST_STEP, 0.0f,
					static_cast<float>(uZ) * fNAVMESH_BAKE_TEST_STEP));
			}
		}

		const u_int uStride = uNAVMESH_BAKE_TEST_QUADS + 1u;
		for (u_int uZ = 0; uZ < uNAVMESH_BAKE_TEST_QUADS; ++uZ)
		{
			for (u_int uX = 0; uX < uNAVMESH_BAKE_TEST_QUADS; ++uX)
			{
				const uint32_t uV0 = uZ * uStride + uX;
				const uint32_t uV1 = uV0 + 1u;
				const uint32_t uV2 = uV0 + uStride;
				const uint32_t uV3 = uV2 + 1u;

				// CCW seen from above -> upward normal (the winding rule in
				// Navigation/CLAUDE.md).
				auIndices.PushBack(uV0); auIndices.PushBack(uV2); auIndices.PushBack(uV3);
				auIndices.PushBack(uV0); auIndices.PushBack(uV3); auIndices.PushBack(uV1);
			}
		}
	}

	// The same footprint as VERTICAL wall quads: every normal is horizontal, so
	// the generator's slope filter rejects the lot and nothing is walkable.
	void NavMeshBakerBuildVerticalWalls(Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
		Zenith_Vector<uint32_t>& auIndices)
	{
		axVertices.Clear();
		auIndices.Clear();

		constexpr float fWALL_HEIGHT = 2.0f;

		for (u_int uZ = 0; uZ < uNAVMESH_BAKE_TEST_QUADS; ++uZ)
		{
			for (u_int uX = 0; uX < uNAVMESH_BAKE_TEST_QUADS; ++uX)
			{
				const float fX0 = static_cast<float>(uX) * fNAVMESH_BAKE_TEST_STEP;
				const float fX1 = fX0 + fNAVMESH_BAKE_TEST_STEP;
				const float fZ = static_cast<float>(uZ) * fNAVMESH_BAKE_TEST_STEP;

				const uint32_t uBase = axVertices.GetSize();
				axVertices.PushBack(Zenith_Maths::Vector3(fX0, 0.0f, fZ));
				axVertices.PushBack(Zenith_Maths::Vector3(fX1, 0.0f, fZ));
				axVertices.PushBack(Zenith_Maths::Vector3(fX1, fWALL_HEIGHT, fZ));
				axVertices.PushBack(Zenith_Maths::Vector3(fX0, fWALL_HEIGHT, fZ));

				auIndices.PushBack(uBase); auIndices.PushBack(uBase + 1u); auIndices.PushBack(uBase + 2u);
				auIndices.PushBack(uBase); auIndices.PushBack(uBase + 2u); auIndices.PushBack(uBase + 3u);
			}
		}
	}

	// Read a whole file into a caller-owned buffer for byte comparison.
	void NavMeshBakerReadFileBytes(const std::string& strPath, Zenith_Vector<uint8_t>& auBytesOut)
	{
		auBytesOut.Clear();

		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			return;
		}

		const uint64_t ulBytes = xStream.GetCapacity();
		const uint8_t* pSrc = static_cast<const uint8_t*>(xStream.GetData());
		auBytesOut.Reserve(static_cast<u_int>(ulBytes));
		for (uint64_t ul = 0; ul < ulBytes; ++ul)
		{
			auBytesOut.PushBack(pSrc[ul]);
		}
	}
}

ZENITH_TEST(AI, NavMeshBakerWritesLoadableFile) { Zenith_UnitTests::TestNavMeshBakerWritesLoadableFile(); }
void Zenith_UnitTests::TestNavMeshBakerWritesLoadableFile()
{
	NavMeshPersistTempFile xFile("zenith_navmesh_bake_test.znavmesh");

	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	NavMeshBakerBuildFlatGround(axVertices, auIndices);

	NavMeshGenerationConfig xConfig;
	Zenith_NavMesh* pxBaked = nullptr;
	const Zenith_NavMeshBakeResult xResult =
		Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, xFile.m_strPath, &pxBaked);

	ZENITH_ASSERT_TRUE(xResult.m_bSuccess, "A flat ground grid must bake");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xResult.m_eError), static_cast<u_int>(NAVMESH_BAKE_ERROR_NONE), "No error on success");
	ZENITH_ASSERT_GT(xResult.m_uPolygonCount, 0u, "A walkable bake has polygons");
	ZENITH_ASSERT_GT(xResult.m_ulBytesWritten, 0ull, "A successful bake wrote bytes");
	ZENITH_ASSERT_NOT_NULL(pxBaked, "The out-mesh param hands ownership to the caller on success");

	// The reported counts must be the mesh's own counts, not a guess.
	if (pxBaked != nullptr)
	{
		ZENITH_ASSERT_EQ(xResult.m_uPolygonCount, pxBaked->GetPolygonCount(), "Reported polygon count is the mesh's");
		ZENITH_ASSERT_EQ(xResult.m_uVertexCount, pxBaked->GetVertexCount(), "Reported vertex count is the mesh's");
	}

	// ...and the file must load back through the validating path with the same
	// shape. This is the whole point of the feature.
	Zenith_NavMesh* pxLoaded = Zenith_NavMesh::LoadFromFile(xFile.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "The baked file must load");
	if (pxLoaded != nullptr && pxBaked != nullptr)
	{
		ZENITH_ASSERT_EQ(pxLoaded->GetPolygonCount(), pxBaked->GetPolygonCount(), "Loaded polygon count matches the bake");
		ZENITH_ASSERT_EQ(pxLoaded->GetVertexCount(), pxBaked->GetVertexCount(), "Loaded vertex count matches the bake");
		ZENITH_ASSERT_NEAR_VEC3(pxLoaded->GetBoundsMin(), pxBaked->GetBoundsMin(), 0.001f, "Loaded bounds min matches");
		ZENITH_ASSERT_NEAR_VEC3(pxLoaded->GetBoundsMax(), pxBaked->GetBoundsMax(), 0.001f, "Loaded bounds max matches");
	}

	delete pxLoaded;
	delete pxBaked;
}

ZENITH_TEST(AI, NavMeshBakerIsByteDeterministic) { Zenith_UnitTests::TestNavMeshBakerIsByteDeterministic(); }
void Zenith_UnitTests::TestNavMeshBakerIsByteDeterministic()
{
	// Two bakes of identical geometry must produce identical FILES. Without this
	// the committed .znavmesh would churn on every tools boot and every dev's
	// working tree would show a spurious diff.
	NavMeshPersistTempFile xFileA("zenith_navmesh_determinism_a.znavmesh");
	NavMeshPersistTempFile xFileB("zenith_navmesh_determinism_b.znavmesh");

	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	NavMeshBakerBuildFlatGround(axVertices, auIndices);

	NavMeshGenerationConfig xConfig;
	const Zenith_NavMeshBakeResult xFirst =
		Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, xFileA.m_strPath);
	const Zenith_NavMeshBakeResult xSecond =
		Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, xFileB.m_strPath);

	ZENITH_ASSERT_TRUE(xFirst.m_bSuccess, "First bake must succeed");
	ZENITH_ASSERT_TRUE(xSecond.m_bSuccess, "Second bake must succeed");
	ZENITH_ASSERT_EQ(xFirst.m_ulBytesWritten, xSecond.m_ulBytesWritten, "Two bakes must write the same byte count");
	ZENITH_ASSERT_EQ(xFirst.m_uPolygonCount, xSecond.m_uPolygonCount, "Two bakes must produce the same polygon count");

	Zenith_Vector<uint8_t> auFirstBytes;
	Zenith_Vector<uint8_t> auSecondBytes;
	NavMeshBakerReadFileBytes(xFileA.m_strPath, auFirstBytes);
	NavMeshBakerReadFileBytes(xFileB.m_strPath, auSecondBytes);

	ZENITH_ASSERT_GT(auFirstBytes.GetSize(), 0u, "The first bake left bytes on disk");
	ZENITH_ASSERT_EQ(auFirstBytes.GetSize(), auSecondBytes.GetSize(), "Both files are the same size");
	ZENITH_ASSERT_EQ(memcmp(auFirstBytes.GetDataPointer(), auSecondBytes.GetDataPointer(), auFirstBytes.GetSize()), 0,
		"Two bakes of one input must be BYTE-identical on disk");
}

ZENITH_TEST(AI, NavMeshBakerRejectsEmptyGeometry) { Zenith_UnitTests::TestNavMeshBakerRejectsEmptyGeometry(); }
void Zenith_UnitTests::TestNavMeshBakerRejectsEmptyGeometry()
{
	NavMeshPersistTempFile xFile("zenith_navmesh_bake_empty_geometry.znavmesh");

	Zenith_Vector<Zenith_Maths::Vector3> axEmptyVertices;
	Zenith_Vector<uint32_t> auEmptyIndices;
	NavMeshGenerationConfig xConfig;

	{
		Zenith_AssertCaptureScope xCapture;
		const Zenith_NavMeshBakeResult xResult =
			Zenith_NavMeshBaker::BakeToFile(axEmptyVertices, auEmptyIndices, xConfig, xFile.m_strPath);
		ZENITH_ASSERT_FALSE(xResult.m_bSuccess, "Baking nothing must fail");
		ZENITH_ASSERT_EQ(static_cast<u_int>(xResult.m_eError), static_cast<u_int>(NAVMESH_BAKE_ERROR_EMPTY_GEOMETRY),
			"Empty geometry must report EMPTY_GEOMETRY");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "Empty geometry is a caller defect and must assert once");
	}

	std::error_code xEC;
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xFile.m_strPath, xEC),
		"A refused bake must not leave a file behind");
}

ZENITH_TEST(AI, NavMeshBakerRejectsEmptyPath) { Zenith_UnitTests::TestNavMeshBakerRejectsEmptyPath(); }
void Zenith_UnitTests::TestNavMeshBakerRejectsEmptyPath()
{
	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	NavMeshBakerBuildFlatGround(axVertices, auIndices);

	NavMeshGenerationConfig xConfig;
	Zenith_NavMesh* pxBaked = reinterpret_cast<Zenith_NavMesh*>(0x1);

	{
		Zenith_AssertCaptureScope xCapture;
		const Zenith_NavMeshBakeResult xResult =
			Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, std::string(), &pxBaked);
		ZENITH_ASSERT_FALSE(xResult.m_bSuccess, "An empty destination must fail");
		ZENITH_ASSERT_EQ(static_cast<u_int>(xResult.m_eError), static_cast<u_int>(NAVMESH_BAKE_ERROR_EMPTY_PATH),
			"An empty destination must report EMPTY_PATH");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An empty destination is a caller defect and must assert once");
	}

	// The out-param is cleared on EVERY path, so a caller can never be handed a
	// stale pointer to check.
	ZENITH_ASSERT_NULL(pxBaked, "A failed bake must clear the out-mesh param");
}

ZENITH_TEST(AI, NavMeshBakerReportsUnwalkableGeometry) { Zenith_UnitTests::TestNavMeshBakerReportsUnwalkableGeometry(); }
void Zenith_UnitTests::TestNavMeshBakerReportsUnwalkableGeometry()
{
	// Geometry with nothing walkable is a DATA outcome, not a caller defect:
	// GENERATION_FAILED, a log line, and NO assert. (No capture scope here on
	// purpose -- if this ever starts asserting, the gate must go red.)
	NavMeshPersistTempFile xFile("zenith_navmesh_bake_unwalkable.znavmesh");

	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	NavMeshBakerBuildVerticalWalls(axVertices, auIndices);

	NavMeshGenerationConfig xConfig;
	const Zenith_NavMeshBakeResult xResult =
		Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, xFile.m_strPath);

	ZENITH_ASSERT_FALSE(xResult.m_bSuccess, "Unwalkable geometry produces no navmesh");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xResult.m_eError), static_cast<u_int>(NAVMESH_BAKE_ERROR_GENERATION_FAILED),
		"Unwalkable geometry must report GENERATION_FAILED");
	ZENITH_ASSERT_EQ(xResult.m_uPolygonCount, 0u, "No polygons are reported");

	std::error_code xEC;
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xFile.m_strPath, xEC),
		"A failed generation must not leave a file behind");
}
