#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// NavMesh Tests
// ============================================================================
ZENITH_TEST(AI, NavMeshPolygonCreation) { Zenith_UnitTests::TestNavMeshPolygonCreation(); }
void Zenith_UnitTests::TestNavMeshPolygonCreation(){
	Zenith_NavMesh xNavMesh;

	// Add vertices for a simple quad
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);

	ZENITH_ASSERT_EQ(xNavMesh.GetPolygonCount(), 1, "Should have 1 polygon");
	ZENITH_ASSERT_EQ(xNavMesh.GetVertexCount(), 4, "Should have 4 vertices");

}

ZENITH_TEST(AI, NavMeshAdjacency) { Zenith_UnitTests::TestNavMeshAdjacency(); }

void Zenith_UnitTests::TestNavMeshAdjacency(){
	Zenith_NavMesh xNavMesh;

	// Create two adjacent triangles sharing an edge
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));  // 1
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, 1.0f));  // 2
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, -1.0f)); // 3

	Zenith_Vector<uint32_t> axTri1, axTri2;
	axTri1.PushBack(0); axTri1.PushBack(1); axTri1.PushBack(2);
	axTri2.PushBack(0); axTri2.PushBack(3); axTri2.PushBack(1);

	xNavMesh.AddPolygon(axTri1);
	xNavMesh.AddPolygon(axTri2);

	xNavMesh.ComputeAdjacency();

	// Polygons 0 and 1 should be neighbors (share edge 0-1)
	const Zenith_NavMeshPolygon& xPoly0 = xNavMesh.GetPolygon(0);
	bool bHasNeighbor = false;
	for (uint32_t u = 0; u < xPoly0.m_axNeighborIndices.GetSize(); ++u)
	{
		if (xPoly0.m_axNeighborIndices.Get(u) == 1)
		{
			bHasNeighbor = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bHasNeighbor, "Polygon 0 should have polygon 1 as neighbor");

}

ZENITH_TEST(AI, NavMeshFindNearestPolygon) { Zenith_UnitTests::TestNavMeshFindNearestPolygon(); }

void Zenith_UnitTests::TestNavMeshFindNearestPolygon(){
	Zenith_NavMesh xNavMesh;

	// Create a simple navmesh
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	// Test point inside polygon
	uint32_t uPolyOut;
	Zenith_Maths::Vector3 xNearestOut;
	bool bFound = xNavMesh.FindNearestPolygon(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f), uPolyOut, xNearestOut);

	ZENITH_ASSERT_TRUE(bFound, "Should find polygon for point inside");
	ZENITH_ASSERT_EQ(uPolyOut, 0, "Should find polygon 0");

}

ZENITH_TEST(AI, NavMeshIsPointOnMesh) { Zenith_UnitTests::TestNavMeshIsPointOnMesh(); }

void Zenith_UnitTests::TestNavMeshIsPointOnMesh(){
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	ZENITH_ASSERT_TRUE(xNavMesh.IsPointOnNavMesh(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f)), "Point inside should be on mesh");
	ZENITH_ASSERT_FALSE(xNavMesh.IsPointOnNavMesh(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f)), "Point far outside should not be on mesh");

}

ZENITH_TEST(AI, NavMeshRaycast) { Zenith_UnitTests::TestNavMeshRaycast(); }

void Zenith_UnitTests::TestNavMeshRaycast(){
	Zenith_NavMesh xNavMesh;

	// Create a navmesh with a gap
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	// Ray within mesh should not hit
	Zenith_Maths::Vector3 xHit;
	bool bHit = xNavMesh.Raycast(
		Zenith_Maths::Vector3(0.2f, 0.0f, 0.5f),
		Zenith_Maths::Vector3(0.8f, 0.0f, 0.5f),
		xHit);
	ZENITH_ASSERT_FALSE(bHit, "Ray within mesh should not hit boundary");

}

ZENITH_TEST(AI, NavMeshFindNearestPolygonInCell) { Zenith_UnitTests::TestNavMeshFindNearestPolygonInCell(); }

void Zenith_UnitTests::TestNavMeshFindNearestPolygonInCell(){

	// Test 1: Empty cell returns unchanged results
	{
		Zenith_NavMesh xNavMesh;

		// Add a triangle so the mesh is valid, but search an empty cell
		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, 1.0f));

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(0);
		axIndices.PushBack(1);
		axIndices.PushBack(2);
		xNavMesh.AddPolygon(axIndices);
		xNavMesh.BuildSpatialGrid();

		// Find a cell that is far from the polygon (should be empty)
		// The grid is clamped, so use an out-of-bounds cell index
		float fMinDistSq = 100.0f;
		uint32_t uPolyOut = UINT32_MAX;
		Zenith_Maths::Vector3 xNearestOut(0.0f);

		// Pass an index beyond grid bounds -- should return early without modifying outputs
		uint32_t uInvalidCellIndex = xNavMesh.m_axGridCells.GetSize() + 10;
		xNavMesh.FindNearestPolygonInCell(uInvalidCellIndex, Zenith_Maths::Vector3(50.0f, 0.0f, 50.0f),
			fMinDistSq, uPolyOut, xNearestOut);

		ZENITH_ASSERT_EQ(uPolyOut, UINT32_MAX, "Empty/invalid cell should not find any polygon");
		ZENITH_ASSERT_EQ(fMinDistSq, 100.0f, "Distance should remain unchanged for invalid cell");
	}

	// Test 2: Single polygon cell finds the correct polygon
	{
		Zenith_NavMesh xNavMesh;

		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 2.0f));

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(0);
		axIndices.PushBack(1);
		axIndices.PushBack(2);
		xNavMesh.AddPolygon(axIndices);
		xNavMesh.BuildSpatialGrid();

		// Query point inside the triangle
		Zenith_Maths::Vector3 xQueryPoint(1.0f, 0.0f, 0.5f);
		int32_t iX, iZ;
		xNavMesh.GetGridCoords(xQueryPoint, iX, iZ);
		uint32_t uCellIndex = xNavMesh.GetGridCellIndex(iX, iZ);

		float fMinDistSq = 100.0f;
		uint32_t uPolyOut = UINT32_MAX;
		Zenith_Maths::Vector3 xNearestOut(0.0f);

		xNavMesh.FindNearestPolygonInCell(uCellIndex, xQueryPoint,
			fMinDistSq, uPolyOut, xNearestOut);

		ZENITH_ASSERT_EQ(uPolyOut, 0, "Should find polygon 0 in the cell");
		ZENITH_ASSERT_LT(fMinDistSq, 100.0f, "Distance should have been updated");
	}

}

ZENITH_TEST(AI, NavMeshComputePolygonBounds) { Zenith_UnitTests::TestNavMeshComputePolygonBounds(); }

void Zenith_UnitTests::TestNavMeshComputePolygonBounds(){

	// Test: Single triangle polygon bounds
	{
		Zenith_Vector<Zenith_Maths::Vector3> axVertices;
		axVertices.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 2.0f));
		axVertices.PushBack(Zenith_Maths::Vector3(4.0f, 0.0f, 1.0f));
		axVertices.PushBack(Zenith_Maths::Vector3(3.0f, 0.0f, 5.0f));

		Zenith_NavMeshPolygon xPoly;
		xPoly.m_axVertexIndices.PushBack(0);
		xPoly.m_axVertexIndices.PushBack(1);
		xPoly.m_axVertexIndices.PushBack(2);

		Zenith_Maths::Vector3 xMin, xMax;
		Zenith_NavMesh::ComputePolygonBounds2D(xPoly, axVertices, xMin, xMax);

		// Min X should be 1.0, Max X should be 4.0
		ZENITH_ASSERT_EQ_FLOAT(xMin.x, 1.0f, 0.001f, "Min X should be 1.0");
		ZENITH_ASSERT_EQ_FLOAT(xMax.x, 4.0f, 0.001f, "Max X should be 4.0");

		// Min Z should be 1.0, Max Z should be 5.0
		ZENITH_ASSERT_EQ_FLOAT(xMin.z, 1.0f, 0.001f, "Min Z should be 1.0");
		ZENITH_ASSERT_EQ_FLOAT(xMax.z, 5.0f, 0.001f, "Max Z should be 5.0");
	}

}

ZENITH_TEST(AI, NavMeshGetRandomReachablePointInRadius) { Zenith_UnitTests::TestNavMeshGetRandomReachablePointInRadius(); }

void Zenith_UnitTests::TestNavMeshGetRandomReachablePointInRadius()
{
	// Build two disconnected islands so we can verify reachability semantics.
	// Island A: 4x4 unit square centered at (0, 0, 0).  All polys connected.
	// Island B: 4x4 unit square centered at (50, 0, 0). Disconnected from A.
	Zenith_NavMesh xNavMesh;

	auto fnAddSquare = [&xNavMesh](float fCx, float fCz)
	{
		const uint32_t uV0 = xNavMesh.AddVertex(Zenith_Maths::Vector3(fCx - 2.0f, 0.0f, fCz - 2.0f));
		const uint32_t uV1 = xNavMesh.AddVertex(Zenith_Maths::Vector3(fCx + 2.0f, 0.0f, fCz - 2.0f));
		const uint32_t uV2 = xNavMesh.AddVertex(Zenith_Maths::Vector3(fCx + 2.0f, 0.0f, fCz + 2.0f));
		const uint32_t uV3 = xNavMesh.AddVertex(Zenith_Maths::Vector3(fCx - 2.0f, 0.0f, fCz + 2.0f));
		Zenith_Vector<uint32_t> ax;
		ax.PushBack(uV0); ax.PushBack(uV1); ax.PushBack(uV2); ax.PushBack(uV3);
		return xNavMesh.AddPolygon(ax);
	};

	fnAddSquare(0.0f, 0.0f);
	fnAddSquare(50.0f, 0.0f);

	xNavMesh.ComputeAdjacency();
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	// Sample many points around the centre of island A; every result must
	// land inside island A's footprint, never inside island B (which is
	// well outside any reasonable radius from the source).
	const Zenith_Maths::Vector3 xCenter(0.0f, 0.0f, 0.0f);
	const float fRadius = 3.0f;
	uint32_t uReachableHits   = 0;
	uint32_t uUnreachableHits = 0;
	uint32_t uOutOfRadiusHits = 0;

	for (uint32_t i = 0; i < 1000; ++i)
	{
		Zenith_Maths::Vector3 xResult;
		if (!xNavMesh.GetRandomReachablePointInRadius(xCenter, fRadius, xResult)) continue;

		// Horizontal distance check.
		const float fDx = xResult.x - xCenter.x;
		const float fDz = xResult.z - xCenter.z;
		if (fDx * fDx + fDz * fDz > fRadius * fRadius)
		{
			++uOutOfRadiusHits;
		}

		// Reachability: result must lie inside island A's [-2,2]x[-2,2] square.
		// Island B is centred at x=50; any sample with x>10 is on the wrong island.
		if (xResult.x > 10.0f)
		{
			++uUnreachableHits;
		}
		else
		{
			++uReachableHits;
		}
	}

	ZENITH_ASSERT_TRUE(uReachableHits > 0,
		"Should produce at least one reachable sample");
	ZENITH_ASSERT_EQ(uUnreachableHits, 0,
		"Should never sample the disconnected island");
	ZENITH_ASSERT_EQ(uOutOfRadiusHits, 0,
		"All samples should be within the requested radius");
}

// ============================================================================
// NavMesh PERSISTENCE tests (SC1b commit B, ZM-D-147)
//
// These pin the validating "ZNAV" v1 load path. Every corrupt-feed unit wraps a
// Zenith_AssertCaptureScope and asserts BOTH the capture hit count AND the
// defined result -- Zenith_Assert fires in every config and the whole unit suite
// runs at boot, so one unscoped deliberate assert would kill the entire gate
// rather than fail a single test.
// ============================================================================

#include "UnitTests/Zenith_AssertCapture.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <filesystem>
#include <limits>

namespace
{
	// Two adjacent quads sharing an edge -- the smallest mesh with real
	// adjacency, non-zero area and non-degenerate bounds.
	void NavMeshPersistBuildTwoQuadMesh(Zenith_NavMesh& xMesh)
	{
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
		xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));  // 1
		xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));  // 2
		xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));  // 3
		xMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 1.0f));  // 4
		xMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 5

		Zenith_Vector<uint32_t> axQuadA;
		axQuadA.PushBack(0); axQuadA.PushBack(1); axQuadA.PushBack(2); axQuadA.PushBack(3);
		xMesh.AddPolygon(axQuadA);

		Zenith_Vector<uint32_t> axQuadB;
		axQuadB.PushBack(3); axQuadB.PushBack(2); axQuadB.PushBack(4); axQuadB.PushBack(5);
		xMesh.AddPolygon(axQuadB);

		xMesh.ComputeAdjacency();
		xMesh.ComputeSpatialData();
		xMesh.BuildSpatialGrid();
	}

	// Serialize xMesh into a caller-owned byte buffer, so a unit can corrupt or
	// truncate exact bytes and wrap them in a FIXED-capacity stream -- which is
	// what a file-loaded stream looks like to the reader.
	void NavMeshPersistSerializeToBytes(const Zenith_NavMesh& xMesh, Zenith_Vector<uint8_t>& auBytesOut)
	{
		Zenith_DataStream xStream;
		xMesh.WriteToDataStream(xStream);

		const uint64_t ulBytes = xStream.GetCursor();
		const uint8_t* pSrc = static_cast<const uint8_t*>(xStream.GetData());
		auBytesOut.Clear();
		auBytesOut.Reserve(static_cast<u_int>(ulBytes));
		for (uint64_t ul = 0; ul < ulBytes; ++ul)
		{
			auBytesOut.PushBack(pSrc[ul]);
		}
	}

	bool NavMeshPersistReadBytes(Zenith_Vector<uint8_t>& auBytes, Zenith_NavMesh& xMeshOut)
	{
		Zenith_DataStream xStream(auBytes.GetDataPointer(), auBytes.GetSize());
		return xMeshOut.ReadFromDataStream(xStream);
	}

	void NavMeshPersistPokeU32(Zenith_Vector<uint8_t>& auBytes, uint64_t ulOffset, uint32_t uValue)
	{
		memcpy(auBytes.GetDataPointer() + ulOffset, &uValue, sizeof(uint32_t));
	}

	void NavMeshPersistPokeFloat(Zenith_Vector<uint8_t>& auBytes, uint64_t ulOffset, float fValue)
	{
		memcpy(auBytes.GetDataPointer() + ulOffset, &fValue, sizeof(float));
	}

	// Byte offsets into the v1 layout documented in Zenith_NavMesh.h.
	constexpr uint64_t ulNAVMESH_TEST_VERSION_OFFSET = 4ull;       // after "ZNAV"
	constexpr uint64_t ulNAVMESH_TEST_VERTEX_COUNT_OFFSET = 8ull;  // after magic + version

	// A temp file removed on scope ENTRY and EXIT, so a mid-test failure cannot
	// leak bytes into the next test.
	struct NavMeshPersistTempFile
	{
		std::string m_strPath;

		explicit NavMeshPersistTempFile(const char* szLeafName)
		{
			std::error_code xEC;
			std::filesystem::path xDir = std::filesystem::temp_directory_path(xEC);
			if (xEC)
			{
				xDir = ".";
			}
			m_strPath = (xDir / szLeafName).string();
			std::filesystem::remove(m_strPath, xEC);
		}

		~NavMeshPersistTempFile()
		{
			std::error_code xEC;
			std::filesystem::remove(m_strPath, xEC);
		}
	};
}

ZENITH_TEST(AI, NavMeshSerializationRoundTrip) { Zenith_UnitTests::TestNavMeshSerializationRoundTrip(); }
void Zenith_UnitTests::TestNavMeshSerializationRoundTrip()
{
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	// Give one polygon a distinctive cost + flag so the record tail is pinned too.
	xSource.m_axPolygons.Get(1).m_fCost = 2.5f;
	xSource.SetPolygonBlocked(1, true);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);
	ZENITH_ASSERT_GT(auBytes.GetSize(), 0u, "Serializer must emit bytes");

	Zenith_NavMesh xLoaded;
	ZENITH_ASSERT_TRUE(NavMeshPersistReadBytes(auBytes, xLoaded), "A well-formed stream must load");

	ZENITH_ASSERT_EQ(xLoaded.GetVertexCount(), xSource.GetVertexCount(), "Vertex count round-trips");
	ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), xSource.GetPolygonCount(), "Polygon count round-trips");
	ZENITH_ASSERT_NEAR_VEC3(xLoaded.GetBoundsMin(), xSource.GetBoundsMin(), 0.0001f, "Bounds min round-trips");
	ZENITH_ASSERT_NEAR_VEC3(xLoaded.GetBoundsMax(), xSource.GetBoundsMax(), 0.0001f, "Bounds max round-trips");

	for (uint32_t u = 0; u < xSource.GetVertexCount(); ++u)
	{
		ZENITH_ASSERT_NEAR_VEC3(xLoaded.GetVertex(u), xSource.GetVertex(u), 0.0001f, "Vertex round-trips");
	}

	// Adjacency is what the generator spends the most work on -- pinned edge by
	// edge, not just as a count.
	for (uint32_t uPoly = 0; uPoly < xSource.GetPolygonCount(); ++uPoly)
	{
		const Zenith_NavMeshPolygon& xSrc = xSource.GetPolygon(uPoly);
		const Zenith_NavMeshPolygon& xDst = xLoaded.GetPolygon(uPoly);
		ZENITH_ASSERT_EQ(xDst.m_axVertexIndices.GetSize(), xSrc.m_axVertexIndices.GetSize(), "Polygon vertex count");
		ZENITH_ASSERT_EQ(xDst.m_axNeighborIndices.GetSize(), xSrc.m_axNeighborIndices.GetSize(), "Polygon neighbour count");
		for (uint32_t uEdge = 0; uEdge < xSrc.m_axVertexIndices.GetSize(); ++uEdge)
		{
			ZENITH_ASSERT_EQ(xDst.m_axVertexIndices.Get(uEdge), xSrc.m_axVertexIndices.Get(uEdge), "Polygon vertex index");
			ZENITH_ASSERT_EQ(xDst.m_axNeighborIndices.Get(uEdge), xSrc.m_axNeighborIndices.Get(uEdge), "Polygon neighbour index");
		}
		ZENITH_ASSERT_EQ_FLOAT(xDst.m_fCost, xSrc.m_fCost, 0.0001f, "Polygon traversal cost");
		ZENITH_ASSERT_EQ(xDst.m_uFlags, xSrc.m_uFlags, "Polygon flags");
		// Recomputed on load rather than trusted -- assert it agrees anyway.
		ZENITH_ASSERT_EQ_FLOAT(xDst.m_fArea, xSrc.m_fArea, 0.0001f, "Polygon area");
		ZENITH_ASSERT_NEAR_VEC3(xDst.m_xNormal, xSrc.m_xNormal, 0.0001f, "Polygon normal");
	}
}

ZENITH_TEST(AI, NavMeshSerializationIsByteDeterministic) { Zenith_UnitTests::TestNavMeshSerializationIsByteDeterministic(); }
void Zenith_UnitTests::TestNavMeshSerializationIsByteDeterministic()
{
	// The determinism contract behind committing a .znavmesh to git: one mesh
	// must always serialize to the same bytes, and load -> save must be a fixed
	// point (otherwise every boot would dirty the working tree).
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auFirst;
	Zenith_Vector<uint8_t> auSecond;
	NavMeshPersistSerializeToBytes(xSource, auFirst);
	NavMeshPersistSerializeToBytes(xSource, auSecond);

	ZENITH_ASSERT_EQ(auFirst.GetSize(), auSecond.GetSize(), "Two serializations must be the same length");
	ZENITH_ASSERT_EQ(memcmp(auFirst.GetDataPointer(), auSecond.GetDataPointer(), auFirst.GetSize()), 0,
		"Two serializations of one mesh must be byte-identical");

	Zenith_NavMesh xLoaded;
	ZENITH_ASSERT_TRUE(NavMeshPersistReadBytes(auFirst, xLoaded), "Round-trip load must succeed");

	Zenith_Vector<uint8_t> auReSerialized;
	NavMeshPersistSerializeToBytes(xLoaded, auReSerialized);
	ZENITH_ASSERT_EQ(auReSerialized.GetSize(), auFirst.GetSize(), "Re-serialized length must match");
	ZENITH_ASSERT_EQ(memcmp(auReSerialized.GetDataPointer(), auFirst.GetDataPointer(), auFirst.GetSize()), 0,
		"load -> save must be a byte-level fixed point");
}

ZENITH_TEST(AI, NavMeshStitchedPortalSurvivesRoundTrip) { Zenith_UnitTests::TestNavMeshStitchedPortalSurvivesRoundTrip(); }
void Zenith_UnitTests::TestNavMeshStitchedPortalSurvivesRoundTrip()
{
	// REGRESSION: StitchPortalAt deliberately APPENDS a phantom neighbour slot
	// past the vertex count, to bridge two polygons that share no edge (DPDoor
	// depends on it). A reader that required neighbourCount == vertexCount would
	// hard-assert on such a mesh the moment it was baked and reloaded -- on
	// perfectly legitimate data, in the exact workflow this feature exists to
	// enable. The bound that protects memory is the per-index range check, not
	// the count equality.
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	// Phantom link: polygon 0 gains a 5th neighbour (it has only 4 edges).
	xSource.m_axPolygons.Get(0).m_axNeighborIndices.PushBack(1);
	ZENITH_ASSERT_EQ(xSource.GetPolygon(0).m_axNeighborIndices.GetSize(), 5u, "Fixture has a phantom neighbour");
	ZENITH_ASSERT_EQ(xSource.GetPolygon(0).m_axVertexIndices.GetSize(), 4u, "...past its 4 edges");

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);

	Zenith_NavMesh xLoaded;
	ZENITH_ASSERT_TRUE(NavMeshPersistReadBytes(auBytes, xLoaded),
		"A stitched mesh must load -- neighbourCount > vertexCount is legal");
	ZENITH_ASSERT_EQ(xLoaded.GetPolygon(0).m_axNeighborIndices.GetSize(), 5u,
		"The phantom neighbour survives the round trip");
	ZENITH_ASSERT_EQ(xLoaded.GetPolygon(0).m_axNeighborIndices.Get(4), 1,
		"...pointing at the polygon it was stitched to");

	// The lower bound still bites: FEWER neighbours than edges is corruption.
	Zenith_NavMesh xTooFew;
	NavMeshPersistBuildTwoQuadMesh(xTooFew);
	xTooFew.m_axPolygons.Get(0).m_axNeighborIndices.PopBack();

	Zenith_Vector<uint8_t> auTooFewBytes;
	NavMeshPersistSerializeToBytes(xTooFew, auTooFewBytes);

	Zenith_NavMesh xRejected;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auTooFewBytes, xRejected);
		ZENITH_ASSERT_FALSE(bLoaded, "Fewer neighbour slots than edges must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "A short neighbour list must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xRejected.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");
}

ZENITH_TEST(AI, NavMeshEmptyMeshRoundTrips) { Zenith_UnitTests::TestNavMeshEmptyMeshRoundTrips(); }
void Zenith_UnitTests::TestNavMeshEmptyMeshRoundTrips()
{
	// A well-formed ZERO-POLYGON file is VALID, not corrupt -- it is what a bake
	// over geometry with nothing walkable legitimately produces. It must load
	// without asserting.
	Zenith_NavMesh xEmpty;

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xEmpty, auBytes);

	Zenith_NavMesh xLoaded;
	ZENITH_ASSERT_TRUE(NavMeshPersistReadBytes(auBytes, xLoaded), "An empty mesh must load cleanly");
	ZENITH_ASSERT_EQ(xLoaded.GetVertexCount(), 0u, "No vertices");
	ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), 0u, "No polygons");
}

ZENITH_TEST(AI, NavMeshReadRejectsBadMagic) { Zenith_UnitTests::TestNavMeshReadRejectsBadMagic(); }
void Zenith_UnitTests::TestNavMeshReadRejectsBadMagic()
{
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);
	auBytes.Get(0) = 'X';  // "XNAV"

	Zenith_NavMesh xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auBytes, xLoaded);
		ZENITH_ASSERT_FALSE(bLoaded, "Bad magic must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "Bad magic must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");
	ZENITH_ASSERT_EQ(xLoaded.GetVertexCount(), 0u, "A refused load leaves an EMPTY mesh");
}

ZENITH_TEST(AI, NavMeshReadRejectsWrongVersion) { Zenith_UnitTests::TestNavMeshReadRejectsWrongVersion(); }
void Zenith_UnitTests::TestNavMeshReadRejectsWrongVersion()
{
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);
	NavMeshPersistPokeU32(auBytes, ulNAVMESH_TEST_VERSION_OFFSET, uZENITH_NAVMESH_WIRE_VERSION + 1u);

	Zenith_NavMesh xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auBytes, xLoaded);
		ZENITH_ASSERT_FALSE(bLoaded, "A future wire version must be refused, not guessed at");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "Wrong version must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");
}

ZENITH_TEST(AI, NavMeshReadRejectsTruncation) { Zenith_UnitTests::TestNavMeshReadRejectsTruncation(); }
void Zenith_UnitTests::TestNavMeshReadRejectsTruncation()
{
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auFull;
	NavMeshPersistSerializeToBytes(xSource, auFull);
	const u_int uFullSize = auFull.GetSize();

	// Three cuts, one per section of the reader: inside the header, inside the
	// vertex block, and just short of the trailing bounds.
	const u_int auCutSizes[3] = { 6u, 20u, uFullSize - 4u };

	for (u_int uCut = 0; uCut < 3u; ++uCut)
	{
		Zenith_Vector<uint8_t> auTruncated;
		auTruncated.Reserve(auCutSizes[uCut]);
		for (u_int u = 0; u < auCutSizes[uCut]; ++u)
		{
			auTruncated.PushBack(auFull.Get(u));
		}

		Zenith_NavMesh xLoaded;
		{
			Zenith_AssertCaptureScope xCapture;
			const bool bLoaded = NavMeshPersistReadBytes(auTruncated, xLoaded);
			ZENITH_ASSERT_FALSE(bLoaded, "A truncated stream must be refused");
			ZENITH_ASSERT_GE(xCapture.GetHitCount(), 1u, "A truncated stream must assert");
		}
		ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");
	}
}

ZENITH_TEST(AI, NavMeshReadRejectsAbsurdCounts) { Zenith_UnitTests::TestNavMeshReadRejectsAbsurdCounts(); }
void Zenith_UnitTests::TestNavMeshReadRejectsAbsurdCounts()
{
	// Counts are checked against the bytes actually available BEFORE Reserve, so
	// a corrupt header can never drive a huge allocation.
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);
	NavMeshPersistPokeU32(auBytes, ulNAVMESH_TEST_VERTEX_COUNT_OFFSET, 0xFFFFFF00u);

	Zenith_NavMesh xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auBytes, xLoaded);
		ZENITH_ASSERT_FALSE(bLoaded, "An absurd vertex count must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An absurd vertex count must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetVertexCount(), 0u, "Nothing was reserved or stored");

	// The same guard on the polygon count.
	Zenith_Vector<uint8_t> auPolyBytes;
	NavMeshPersistSerializeToBytes(xSource, auPolyBytes);
	const uint64_t ulPolyCountOffset =
		ulNAVMESH_TEST_VERTEX_COUNT_OFFSET + sizeof(uint32_t) + xSource.GetVertexCount() * 3ull * sizeof(float);
	NavMeshPersistPokeU32(auPolyBytes, ulPolyCountOffset, 0x00FFFFFFu);

	Zenith_NavMesh xLoadedPolys;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auPolyBytes, xLoadedPolys);
		ZENITH_ASSERT_FALSE(bLoaded, "An absurd polygon count must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An absurd polygon count must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoadedPolys.GetPolygonCount(), 0u, "Nothing was reserved or stored");
}

ZENITH_TEST(AI, NavMeshReadRejectsOutOfRangeIndices) { Zenith_UnitTests::TestNavMeshReadRejectsOutOfRangeIndices(); }
void Zenith_UnitTests::TestNavMeshReadRejectsOutOfRangeIndices()
{
	// Before this hardening an out-of-range index survived the read and reached
	// ComputeSpatialData's unchecked axVertices.Get() -- an out-of-bounds read
	// with no diagnostic at all.
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);

	// Polygon 0's first vertex index: magic+version+vertexCount+vertices
	// +polygonCount+thisPolygon'sVertexCount.
	const uint64_t ulFirstIndexOffset =
		ulNAVMESH_TEST_VERTEX_COUNT_OFFSET + sizeof(uint32_t) +
		xSource.GetVertexCount() * 3ull * sizeof(float) +
		sizeof(uint32_t) + sizeof(uint32_t);
	NavMeshPersistPokeU32(auBytes, ulFirstIndexOffset, xSource.GetVertexCount() + 10u);

	Zenith_NavMesh xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auBytes, xLoaded);
		ZENITH_ASSERT_FALSE(bLoaded, "A vertex index past the vertex array must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An out-of-range index must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");

	// A neighbour index past the polygon count is equally fatal: A* would follow
	// it straight out of the array.
	Zenith_Vector<uint8_t> auNeighbourBytes;
	NavMeshPersistSerializeToBytes(xSource, auNeighbourBytes);
	const uint64_t ulFirstNeighbourOffset =
		ulFirstIndexOffset + xSource.GetPolygon(0).m_axVertexIndices.GetSize() * sizeof(uint32_t) + sizeof(uint32_t);
	NavMeshPersistPokeU32(auNeighbourBytes, ulFirstNeighbourOffset, xSource.GetPolygonCount() + 5u);

	Zenith_NavMesh xLoadedNeighbour;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auNeighbourBytes, xLoadedNeighbour);
		ZENITH_ASSERT_FALSE(bLoaded, "A neighbour index past the polygon count must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An out-of-range neighbour must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoadedNeighbour.GetPolygonCount(), 0u, "A refused load leaves an EMPTY mesh");
}

ZENITH_TEST(AI, NavMeshReadRejectsNonFiniteVertex) { Zenith_UnitTests::TestNavMeshReadRejectsNonFiniteVertex(); }
void Zenith_UnitTests::TestNavMeshReadRejectsNonFiniteVertex()
{
	// A NaN vertex silently poisons bounds, the spatial grid and every distance
	// comparison -- the classic "green tests, unusable navmesh" corruption.
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);

	const uint64_t ulFirstVertexOffset = ulNAVMESH_TEST_VERTEX_COUNT_OFFSET + sizeof(uint32_t);
	NavMeshPersistPokeFloat(auBytes, ulFirstVertexOffset, std::numeric_limits<float>::quiet_NaN());

	Zenith_NavMesh xLoaded;
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = NavMeshPersistReadBytes(auBytes, xLoaded);
		ZENITH_ASSERT_FALSE(bLoaded, "A non-finite vertex must be refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "A non-finite vertex must assert exactly once");
	}
	ZENITH_ASSERT_EQ(xLoaded.GetVertexCount(), 0u, "A refused load leaves an EMPTY mesh");
}

ZENITH_TEST(AI, NavMeshLoadFromFileRejectsMissingAndEmptyPaths) { Zenith_UnitTests::TestNavMeshLoadFromFileRejectsMissingAndEmptyPaths(); }
void Zenith_UnitTests::TestNavMeshLoadFromFileRejectsMissingAndEmptyPaths()
{
	{
		Zenith_AssertCaptureScope xCapture;
		Zenith_NavMesh* pxMesh = Zenith_NavMesh::LoadFromFile(std::string());
		ZENITH_ASSERT_NULL(pxMesh, "An empty path must yield nullptr");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An empty path must assert exactly once");
	}

	{
		NavMeshPersistTempFile xFile("zenith_navmesh_missing_test.znavmesh");
		Zenith_AssertCaptureScope xCapture;
		Zenith_NavMesh* pxMesh = Zenith_NavMesh::LoadFromFile(xFile.m_strPath);
		ZENITH_ASSERT_NULL(pxMesh, "A missing file must yield nullptr, not an empty mesh");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "A missing file must assert exactly once");
	}
}

ZENITH_TEST(AI, NavMeshSaveAndLoadThroughDisk) { Zenith_UnitTests::TestNavMeshSaveAndLoadThroughDisk(); }
void Zenith_UnitTests::TestNavMeshSaveAndLoadThroughDisk()
{
	NavMeshPersistTempFile xFile("zenith_navmesh_roundtrip_test.znavmesh");

	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	ZENITH_ASSERT_TRUE(xSource.SaveToFile(xFile.m_strPath), "SaveToFile must report success once the bytes are on disk");

	Zenith_NavMesh* pxLoaded = Zenith_NavMesh::LoadFromFile(xFile.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "A file just written must load back");
	if (pxLoaded != nullptr)
	{
		ZENITH_ASSERT_EQ(pxLoaded->GetVertexCount(), xSource.GetVertexCount(), "Vertex count survives disk");
		ZENITH_ASSERT_EQ(pxLoaded->GetPolygonCount(), xSource.GetPolygonCount(), "Polygon count survives disk");
		delete pxLoaded;
	}

	// An empty destination is a caller defect: loud, and refused.
	{
		Zenith_AssertCaptureScope xCapture;
		const bool bSaved = xSource.SaveToFile(std::string());
		ZENITH_ASSERT_FALSE(bSaved, "SaveToFile must refuse an empty path");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "An empty save path must assert exactly once");
	}
}

ZENITH_TEST(AI, NavMeshLoadFromFileRejectsCorruptFile) { Zenith_UnitTests::TestNavMeshLoadFromFileRejectsCorruptFile(); }
void Zenith_UnitTests::TestNavMeshLoadFromFileRejectsCorruptFile()
{
	// The whole-stack version of the corrupt-magic unit: a real file on disk,
	// through LoadFromFile, must come back as nullptr rather than as a mesh
	// holding whatever happened to parse.
	NavMeshPersistTempFile xFile("zenith_navmesh_corrupt_test.znavmesh");

	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);
	auBytes.Get(1) = '!';

	Zenith_FileAccess::WriteFile(xFile.m_strPath.c_str(), auBytes.GetDataPointer(), auBytes.GetSize());

	{
		Zenith_AssertCaptureScope xCapture;
		Zenith_NavMesh* pxMesh = Zenith_NavMesh::LoadFromFile(xFile.m_strPath);
		ZENITH_ASSERT_NULL(pxMesh, "A corrupt file must yield nullptr, and the partial mesh must be deleted");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "A corrupt file must assert exactly once");
	}
}

