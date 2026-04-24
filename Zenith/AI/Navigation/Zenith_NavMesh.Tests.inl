#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Navigation/Zenith_NavMesh.h"

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

