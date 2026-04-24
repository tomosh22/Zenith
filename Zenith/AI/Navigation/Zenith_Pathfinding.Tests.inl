#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_Pathfinding.h"

ZENITH_TEST(AI, PathfindingStraightLine) { Zenith_UnitTests::TestPathfindingStraightLine(); }

void Zenith_UnitTests::TestPathfindingStraightLine(){
	Zenith_NavMesh xNavMesh;

	// Create a simple straight navmesh
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(9.0f, 0.0f, 1.0f));

	ZENITH_ASSERT_EQ(xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Straight line path should succeed");
	ZENITH_ASSERT_GE(xResult.m_axWaypoints.GetSize(), 2, "Path should have at least start and end");

}

ZENITH_TEST(AI, PathfindingAroundObstacle) { Zenith_UnitTests::TestPathfindingAroundObstacle(); }

void Zenith_UnitTests::TestPathfindingAroundObstacle(){
	// Test pathfinding across connected polygons
	// Polygons must share vertex indices (not just positions) for adjacency to work

	Zenith_NavMesh xNavMesh;

	// Create two connected rectangles sharing an edge
	// Left polygon: (0,0,0) to (2,0,2)
	// Right polygon: (2,0,0) to (6,0,2)
	// Shared edge: vertices 1-2 at x=2

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1 (shared)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2 (shared)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 0.0f));  // 4
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 2.0f));  // 5

	Zenith_Vector<uint32_t> axPoly1, axPoly2;
	// Left polygon: 0 -> 1 -> 2 -> 3 (CCW)
	axPoly1.PushBack(0); axPoly1.PushBack(1); axPoly1.PushBack(2); axPoly1.PushBack(3);
	// Right polygon: 1 -> 4 -> 5 -> 2 (CCW, shares edge 1-2 with left polygon)
	axPoly2.PushBack(1); axPoly2.PushBack(4); axPoly2.PushBack(5); axPoly2.PushBack(2);

	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),  // Start in left section
		Zenith_Maths::Vector3(4.0f, 0.0f, 1.0f)); // End in right section

	ZENITH_ASSERT_EQ(xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Path around corner should succeed");

}

ZENITH_TEST(AI, PathfindingNoPath) { Zenith_UnitTests::TestPathfindingNoPath(); }

void Zenith_UnitTests::TestPathfindingNoPath(){
	Zenith_NavMesh xNavMesh;

	// Create two disconnected polygons
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(11.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(11.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axPoly1, axPoly2;
	axPoly1.PushBack(0); axPoly1.PushBack(1); axPoly1.PushBack(2); axPoly1.PushBack(3);
	axPoly2.PushBack(4); axPoly2.PushBack(5); axPoly2.PushBack(6); axPoly2.PushBack(7);

	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(0.5f, 0.0f, 0.5f),
		Zenith_Maths::Vector3(10.5f, 0.0f, 0.5f));

	ZENITH_ASSERT_EQ(xResult.m_eStatus, Zenith_PathResult::Status::FAILED, "Path between disconnected areas should fail");

}

ZENITH_TEST(AI, PathfindingSmoothing) { Zenith_UnitTests::TestPathfindingSmoothing(); }

void Zenith_UnitTests::TestPathfindingSmoothing(){
	// Path smoothing test - verifies that paths are simplified
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	ZENITH_ASSERT_EQ(xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Path should succeed");

	// Smooth the path
	Zenith_Pathfinding::SmoothPath(xResult.m_axWaypoints, xNavMesh);

	// For a straight-line traversable path, should reduce to just start and end
	ZENITH_ASSERT_LE(xResult.m_axWaypoints.GetSize(), 3, "Smoothed straight path should have few waypoints");

}

ZENITH_TEST(AI, PathfindingNoDuplicateWaypoints) { Zenith_UnitTests::TestPathfindingNoDuplicateWaypoints(); }

void Zenith_UnitTests::TestPathfindingNoDuplicateWaypoints(){
	// Test that A* pathfinding doesn't produce duplicate waypoints
	// This verifies the open set tracking fix

	Zenith_NavMesh xNavMesh;

	// Create a chain of 4 connected polygons to force multiple A* iterations
	// Polygon 0: (0,0) to (2,2)
	// Polygon 1: (2,0) to (4,2) - shares edge with 0
	// Polygon 2: (4,0) to (6,2) - shares edge with 1
	// Polygon 3: (6,0) to (8,2) - shares edge with 2

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1 (shared 0-1)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2 (shared 0-1)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));  // 4 (shared 1-2)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 2.0f));  // 5 (shared 1-2)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 0.0f));  // 6 (shared 2-3)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 2.0f));  // 7 (shared 2-3)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(8.0f, 0.0f, 0.0f));  // 8
	xNavMesh.AddVertex(Zenith_Maths::Vector3(8.0f, 0.0f, 2.0f));  // 9

	Zenith_Vector<uint32_t> axPoly0, axPoly1, axPoly2, axPoly3;
	axPoly0.PushBack(0); axPoly0.PushBack(1); axPoly0.PushBack(2); axPoly0.PushBack(3);
	axPoly1.PushBack(1); axPoly1.PushBack(4); axPoly1.PushBack(5); axPoly1.PushBack(2);
	axPoly2.PushBack(4); axPoly2.PushBack(6); axPoly2.PushBack(7); axPoly2.PushBack(5);
	axPoly3.PushBack(6); axPoly3.PushBack(8); axPoly3.PushBack(9); axPoly3.PushBack(7);

	xNavMesh.AddPolygon(axPoly0);
	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.AddPolygon(axPoly3);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	// Find path across all polygons
	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),   // Start in poly 0
		Zenith_Maths::Vector3(7.0f, 0.0f, 1.0f));  // End in poly 3

	ZENITH_ASSERT_EQ(xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Path across 4 polygons should succeed");

	// Check for duplicate waypoints
	bool bHasDuplicates = false;
	for (uint32_t u = 0; u + 1 < xResult.m_axWaypoints.GetSize(); ++u)
	{
		Zenith_Maths::Vector3 xA = xResult.m_axWaypoints.Get(u);
		Zenith_Maths::Vector3 xB = xResult.m_axWaypoints.Get(u + 1);
		if (Zenith_Maths::Length(xA - xB) < 0.001f)
		{
			bHasDuplicates = true;
			break;
		}
	}

	ZENITH_ASSERT_FALSE(bHasDuplicates, "Path should not have duplicate consecutive waypoints");

}

ZENITH_TEST(AI, PathfindingBatchProcessing) { Zenith_UnitTests::TestPathfindingBatchProcessing(); }

void Zenith_UnitTests::TestPathfindingBatchProcessing(){
	// Test batch parallel pathfinding API
	Zenith_NavMesh xNavMesh;

	// Create a simple navmesh
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	// Create batch of path requests
	Zenith_Pathfinding::PathRequest axRequests[3];

	axRequests[0].m_pxNavMesh = &xNavMesh;
	axRequests[0].m_xStart = Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f);
	axRequests[0].m_xEnd = Zenith_Maths::Vector3(9.0f, 0.0f, 1.0f);

	axRequests[1].m_pxNavMesh = &xNavMesh;
	axRequests[1].m_xStart = Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f);
	axRequests[1].m_xEnd = Zenith_Maths::Vector3(8.0f, 0.0f, 8.0f);

	axRequests[2].m_pxNavMesh = &xNavMesh;
	axRequests[2].m_xStart = Zenith_Maths::Vector3(5.0f, 0.0f, 1.0f);
	axRequests[2].m_xEnd = Zenith_Maths::Vector3(5.0f, 0.0f, 9.0f);

	// Process batch
	Zenith_Pathfinding::FindPathsBatch(axRequests, 3);

	// Verify all paths succeeded
	ZENITH_ASSERT_EQ(axRequests[0].m_xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Batch request 0 should succeed");
	ZENITH_ASSERT_EQ(axRequests[1].m_xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Batch request 1 should succeed");
	ZENITH_ASSERT_EQ(axRequests[2].m_xResult.m_eStatus, Zenith_PathResult::Status::SUCCESS, "Batch request 2 should succeed");

	// Verify waypoints exist
	ZENITH_ASSERT_GE(axRequests[0].m_xResult.m_axWaypoints.GetSize(), 2, "Batch request 0 should have waypoints");

	// Test null navmesh handling
	Zenith_Pathfinding::PathRequest xNullRequest;
	xNullRequest.m_pxNavMesh = nullptr;
	xNullRequest.m_xStart = Zenith_Maths::Vector3(0.0f);
	xNullRequest.m_xEnd = Zenith_Maths::Vector3(1.0f);
	Zenith_Pathfinding::FindPathsBatch(&xNullRequest, 1);
	ZENITH_ASSERT_EQ(xNullRequest.m_xResult.m_eStatus, Zenith_PathResult::Status::FAILED, "Null navmesh request should fail");

	// Test empty batch
	Zenith_Pathfinding::FindPathsBatch(nullptr, 0);  // Should not crash

}

ZENITH_TEST(AI, PathfindingPartialPath) { Zenith_UnitTests::TestPathfindingPartialPath(); }

void Zenith_UnitTests::TestPathfindingPartialPath(){
	// Test that partial paths are returned for disconnected regions
	Zenith_NavMesh xNavMesh;

	// Create two disconnected polygons
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(3.0f, 0.0f, 3.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 3.0f));

	// Polygon 2: Disconnected (far away)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(23.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(23.0f, 0.0f, 3.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(20.0f, 0.0f, 3.0f));

	Zenith_Vector<uint32_t> axPoly0, axPoly1;
	axPoly0.PushBack(0); axPoly0.PushBack(1); axPoly0.PushBack(2); axPoly0.PushBack(3);
	xNavMesh.AddPolygon(axPoly0);

	axPoly1.PushBack(4); axPoly1.PushBack(5); axPoly1.PushBack(6); axPoly1.PushBack(7);
	xNavMesh.AddPolygon(axPoly1);

	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	// Try to find path from start polygon to disconnected target polygon
	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(21.0f, 0.0f, 1.0f));

	// Should fail since regions are disconnected
	ZENITH_ASSERT_TRUE(xResult.m_eStatus == Zenith_PathResult::Status::FAILED ||
	              xResult.m_eStatus == Zenith_PathResult::Status::PARTIAL, "Path to disconnected region should fail or return partial");

}

