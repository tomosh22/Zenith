#include "UnitTests/Zenith_UnitTests.h"
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


// ============================================================================
// Smoother regression: shortcut must NOT cross a carved-out region of the
// navmesh (e.g., the area beneath a wall on the floor mesh). Geometric
// Raycast on a flat navmesh trivially "passes through" the gap because no
// polygon plane sits in the gap to intersect. The SegmentExitsNavMesh
// probe in SmoothPath catches this by sampling the line and asserting
// every sample lands on a polygon at roughly the same Y as the endpoints.
//
// Setup:
//   Two adjacent floor polygons at y=0.0, separated horizontally by a
//   1m gap (no polygon between them). Both polygons are flagged as
//   neighbours via `SetNeighbor` (matching the real-world bug where
//   shared vertex indices in the grid-stamped polygon mesh make two
//   floor polygons across a carved-out wall footprint claim adjacency).
//
//   A separate "wall-top" polygon at y=1.0 sits ABOVE the gap. Without
//   the height-aware probe, FindPolygonContaining would find this
//   wall-top polygon for samples at y=0.0 in the gap and report "still
//   on the navmesh" -- masking the hole. The probe's vertical tolerance
//   (0.5m) excludes the wall-top from the y=0.0 sample's match.
// ============================================================================
ZENITH_TEST(AI, PathfindingSmootherRejectsCarvedShortcut) { Zenith_UnitTests::TestPathfindingSmootherRejectsCarvedShortcut(); }

void Zenith_UnitTests::TestPathfindingSmootherRejectsCarvedShortcut(){
	Zenith_NavMesh xNavMesh;

	// Two floor polygons at y=0, both 2m x 2m, separated by a 1m gap in z.
	// West polygon: x in [0,2], z in [0,2]
	// East polygon: x in [0,2], z in [3,5]
	// Gap: z in [2,3]
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 3.0f));  // 4
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 3.0f));  // 5
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 5.0f));  // 6
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));  // 7

	// Wall-top polygon at y=1.0 spanning the gap (z in [2,3]).
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 1.0f, 2.0f));  // 8
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 1.0f, 2.0f));  // 9
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 1.0f, 3.0f));  // 10
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 1.0f, 3.0f));  // 11

	Zenith_Vector<uint32_t> axWest, axEast, axWallTop;
	axWest.PushBack(0); axWest.PushBack(1); axWest.PushBack(2); axWest.PushBack(3);
	axEast.PushBack(4); axEast.PushBack(5); axEast.PushBack(6); axEast.PushBack(7);
	axWallTop.PushBack(8); axWallTop.PushBack(9); axWallTop.PushBack(10); axWallTop.PushBack(11);

	xNavMesh.AddPolygon(axWest);     // poly 0
	xNavMesh.AddPolygon(axEast);     // poly 1
	xNavMesh.AddPolygon(axWallTop);  // poly 2
	xNavMesh.BuildSpatialGrid();

	// Force-claim that poly 0 and poly 1 are neighbours (the bug-state).
	// In production this happens via the grid-vertex-matching adjacency
	// pass when a wall carves cells out -- adjacent floor polygons on
	// opposite sides claim adjacency via shared vertex indices despite
	// no real connection.
	xNavMesh.SetNeighbor(0, /*edge=*/2, 1);
	xNavMesh.SetNeighbor(1, /*edge=*/0, 0);

	// Build a 2-waypoint candidate path from west to east. The smoother
	// will try to shortcut start->end directly across the gap. With the
	// fix, that shortcut is rejected because samples at y=0 in z in [2,3]
	// find no polygon at the floor level (the wall-top at y=1.0 is too
	// far away vertically given the 0.5m probe tolerance).
	Zenith_Vector<Zenith_Maths::Vector3> axPath;
	axPath.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));   // west centre
	axPath.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 2.5f));   // gap midpoint -- intermediate
	axPath.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 4.0f));   // east centre

	Zenith_Pathfinding::SmoothPath(axPath, xNavMesh);

	// After smoothing the intermediate waypoint must REMAIN because the
	// start->end shortcut crosses the gap. If smoothing reduced to 2
	// waypoints, the regression has come back.
	ZENITH_ASSERT_GE(axPath.GetSize(), 3,
		"SmoothPath must NOT shortcut across the carved-out region");
}

// ============================================================================
// Companion test: verify the smoother still smooths valid shortcuts that
// don't cross gaps. Without this counter-check, an over-aggressive
// SegmentExitsNavMesh probe (e.g., too-tight vertical tolerance) would
// silently break smoothing for all paths.
// ============================================================================
ZENITH_TEST(AI, PathfindingSmootherAcceptsValidShortcut) { Zenith_UnitTests::TestPathfindingSmootherAcceptsValidShortcut(); }

void Zenith_UnitTests::TestPathfindingSmootherAcceptsValidShortcut(){
	Zenith_NavMesh xNavMesh;

	// Two adjacent floor polygons forming a 4m x 2m rectangle.
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1 shared
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2 shared
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));  // 4
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 2.0f));  // 5

	Zenith_Vector<uint32_t> axLeft, axRight;
	axLeft.PushBack(0);  axLeft.PushBack(1);  axLeft.PushBack(2);  axLeft.PushBack(3);
	axRight.PushBack(1); axRight.PushBack(4); axRight.PushBack(5); axRight.PushBack(2);

	xNavMesh.AddPolygon(axLeft);
	xNavMesh.AddPolygon(axRight);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	// Build a 3-waypoint path with a redundant intermediate. The smoother
	// should reduce it to 2 waypoints because nothing is in the way.
	Zenith_Vector<Zenith_Maths::Vector3> axPath;
	axPath.PushBack(Zenith_Maths::Vector3(0.5f, 0.0f, 1.0f));
	axPath.PushBack(Zenith_Maths::Vector3(2.0f, 0.0f, 1.0f));  // portal midpoint -- redundant
	axPath.PushBack(Zenith_Maths::Vector3(3.5f, 0.0f, 1.0f));

	Zenith_Pathfinding::SmoothPath(axPath, xNavMesh);

	ZENITH_ASSERT_EQ(axPath.GetSize(), 2,
		"SmoothPath must collapse a redundant intermediate when the line is clear");
}
