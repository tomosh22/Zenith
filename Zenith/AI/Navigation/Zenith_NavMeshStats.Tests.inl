#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Navigation/Zenith_NavMeshStats.h"

// ============================================================================
// Zenith_NavMeshStats tests (SC1b commit B, ZM-D-147)
//
// Every number the editor's NavMesh panel shows comes from Compute(), so the
// panel cannot drift from the mesh -- which only holds if Compute() itself is
// pinned. These use a HAND-CRAFTED mesh whose every figure is known by
// construction, not a generator output whose counts would have to be trusted.
// ============================================================================

ZENITH_TEST(AI, NavMeshStatsOnCraftedMesh) { Zenith_UnitTests::TestNavMeshStatsOnCraftedMesh(); }
void Zenith_UnitTests::TestNavMeshStatsOnCraftedMesh()
{
	// Two 1x1 quads sharing an edge, plus one detached 2x1 quad island:
	//   3 polygons, 1 portal link, 1 isolated polygon, total area 1+1+2 = 4.
	Zenith_NavMesh xMesh;
	xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));  // 1
	xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));  // 2
	xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));  // 3
	xMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 1.0f));  // 4
	xMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 5
	// The island, well clear of the pair so no edge is shared.
	xMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f)); // 6
	xMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 1.0f)); // 7
	xMesh.AddVertex(Zenith_Maths::Vector3(12.0f, 0.0f, 1.0f)); // 8
	xMesh.AddVertex(Zenith_Maths::Vector3(12.0f, 0.0f, 0.0f)); // 9

	Zenith_Vector<uint32_t> axQuadA;
	axQuadA.PushBack(0); axQuadA.PushBack(1); axQuadA.PushBack(2); axQuadA.PushBack(3);
	xMesh.AddPolygon(axQuadA);

	Zenith_Vector<uint32_t> axQuadB;
	axQuadB.PushBack(3); axQuadB.PushBack(2); axQuadB.PushBack(4); axQuadB.PushBack(5);
	xMesh.AddPolygon(axQuadB);

	Zenith_Vector<uint32_t> axIsland;
	axIsland.PushBack(6); axIsland.PushBack(7); axIsland.PushBack(8); axIsland.PushBack(9);
	xMesh.AddPolygon(axIsland);

	xMesh.ComputeAdjacency();
	xMesh.ComputeSpatialData();

	// One dynamic obstacle, so the blocked count is not vacuously zero.
	xMesh.SetPolygonBlocked(2, true);

	const Zenith_NavMeshStats xStats = Zenith_NavMeshStats::Compute(xMesh);

	ZENITH_ASSERT_EQ(xStats.m_uVertexCount, 10u, "Vertex count");
	ZENITH_ASSERT_EQ(xStats.m_uPolygonCount, 3u, "Polygon count");

	ZENITH_ASSERT_NEAR_VEC3(xStats.m_xBoundsMin, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0001f, "Bounds min");
	ZENITH_ASSERT_NEAR_VEC3(xStats.m_xBoundsMax, Zenith_Maths::Vector3(12.0f, 0.0f, 1.0f), 0.0001f, "Bounds max");
	ZENITH_ASSERT_NEAR_VEC3(xStats.m_xExtents, Zenith_Maths::Vector3(12.0f, 0.0f, 1.0f), 0.0001f, "Extents");

	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fTotalArea, 4.0f, 0.0001f, "Two unit quads plus a 2x1 quad");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fMinPolygonArea, 1.0f, 0.0001f, "Smallest polygon");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fMaxPolygonArea, 2.0f, 0.0001f, "Largest polygon");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fAveragePolygonArea, 4.0f / 3.0f, 0.0001f, "Mean polygon area");

	// The shared edge is ONE portal, counted from the lower-indexed polygon only
	// -- a mutual link must not be double counted.
	ZENITH_ASSERT_EQ(xStats.m_uPortalLinkCount, 1u, "One shared edge is one portal");
	ZENITH_ASSERT_EQ(xStats.m_uIsolatedPolygonCount, 1u, "The detached quad is the only island");
	ZENITH_ASSERT_EQ(xStats.m_uBlockedPolygonCount, 1u, "One polygon was blocked");
	ZENITH_ASSERT_GT(xStats.m_ulApproxMemoryBytes, 0ull, "A non-empty mesh reports a footprint");
}

ZENITH_TEST(AI, NavMeshStatsOnEmptyMeshIsDefined) { Zenith_UnitTests::TestNavMeshStatsOnEmptyMeshIsDefined(); }
void Zenith_UnitTests::TestNavMeshStatsOnEmptyMeshIsDefined()
{
	// TOTAL: an empty mesh must produce zeros, not a divide-by-zero average or
	// an assert -- the panel renders this state whenever a ref is unloaded.
	Zenith_NavMesh xEmpty;
	const Zenith_NavMeshStats xStats = Zenith_NavMeshStats::Compute(xEmpty);

	ZENITH_ASSERT_EQ(xStats.m_uVertexCount, 0u, "No vertices");
	ZENITH_ASSERT_EQ(xStats.m_uPolygonCount, 0u, "No polygons");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fTotalArea, 0.0f, 0.0001f, "No area");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fAveragePolygonArea, 0.0f, 0.0001f, "Mean of nothing is zero, not NaN");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fMinPolygonArea, 0.0f, 0.0001f, "Min of nothing is zero");
	ZENITH_ASSERT_EQ_FLOAT(xStats.m_fMaxPolygonArea, 0.0f, 0.0001f, "Max of nothing is zero");
	ZENITH_ASSERT_EQ(xStats.m_uPortalLinkCount, 0u, "No portals");
	ZENITH_ASSERT_EQ(xStats.m_uIsolatedPolygonCount, 0u, "No isolated polygons");
	ZENITH_ASSERT_EQ(xStats.m_uBlockedPolygonCount, 0u, "No blocked polygons");
	ZENITH_ASSERT_EQ(xStats.m_ulApproxMemoryBytes, 0ull, "No storage");
}

ZENITH_TEST(AI, NavMeshStatsSurviveSerialization) { Zenith_UnitTests::TestNavMeshStatsSurviveSerialization(); }
void Zenith_UnitTests::TestNavMeshStatsSurviveSerialization()
{
	// The panel reports on a mesh that came off DISK, so the figures must be
	// invariant across a serialize/load cycle -- including the BLOCKED flag,
	// which is the one stats input the load path does not recompute.
	Zenith_NavMesh xSource;
	NavMeshPersistBuildTwoQuadMesh(xSource);
	xSource.SetPolygonBlocked(0, true);

	Zenith_Vector<uint8_t> auBytes;
	NavMeshPersistSerializeToBytes(xSource, auBytes);

	Zenith_NavMesh xLoaded;
	ZENITH_ASSERT_TRUE(NavMeshPersistReadBytes(auBytes, xLoaded), "Round-trip load must succeed");

	const Zenith_NavMeshStats xBefore = Zenith_NavMeshStats::Compute(xSource);
	const Zenith_NavMeshStats xAfter = Zenith_NavMeshStats::Compute(xLoaded);

	ZENITH_ASSERT_EQ(xAfter.m_uVertexCount, xBefore.m_uVertexCount, "Vertex count survives");
	ZENITH_ASSERT_EQ(xAfter.m_uPolygonCount, xBefore.m_uPolygonCount, "Polygon count survives");
	ZENITH_ASSERT_EQ(xAfter.m_uPortalLinkCount, xBefore.m_uPortalLinkCount, "Portal links survive");
	ZENITH_ASSERT_EQ(xAfter.m_uIsolatedPolygonCount, xBefore.m_uIsolatedPolygonCount, "Isolated count survives");
	ZENITH_ASSERT_EQ(xAfter.m_uBlockedPolygonCount, xBefore.m_uBlockedPolygonCount, "Blocked flags survive the wire");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_fTotalArea, xBefore.m_fTotalArea, 0.0001f, "Total area survives");
}
