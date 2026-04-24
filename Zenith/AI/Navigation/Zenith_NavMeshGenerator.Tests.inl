#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AITests.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"

// ============================================================================
// NavMesh Generator helper tests
// ============================================================================

ZENITH_TEST(AI, CountWalkableSpans) { Zenith_UnitTests::TestCountWalkableSpans(); }

void Zenith_UnitTests::TestCountWalkableSpans(){

	// Build a minimal 2-column context with a known set of walkable spans, then
	// verify GatherWalkableSpanStats reports the expected count and Y range.
	Zenith_NavMeshGenerator::GenerationContext xCtx;
	xCtx.m_xConfig.m_fCellHeight = 1.0f;
	xCtx.m_xBoundsMin = { 0.0f, 0.0f, 0.0f };
	xCtx.m_axColumns.PushBack(Zenith_NavMeshGenerator::HeightfieldColumn{});
	xCtx.m_axColumns.PushBack(Zenith_NavMeshGenerator::HeightfieldColumn{});

	// Column 0: one walkable span (areaType=1) at maxY=10, one unwalkable at 20.
	Zenith_NavMeshGenerator::AddSpan(xCtx.m_axColumns.Get(0), 8, 10, 1);
	Zenith_NavMeshGenerator::AddSpan(xCtx.m_axColumns.Get(0), 18, 20, 0);
	// Column 1: one walkable span at maxY=5.
	Zenith_NavMeshGenerator::AddSpan(xCtx.m_axColumns.Get(1), 3, 5, 1);

	const Zenith_NavMeshGenerator::WalkableSpanStats xStats =
		Zenith_NavMeshGenerator::GatherWalkableSpanStats(xCtx);

	ZENITH_ASSERT_EQ(xStats.m_uCount, 2, "Expected 2 walkable spans, got %u", xStats.m_uCount);
	ZENITH_ASSERT_EQ(xStats.m_fMinY, 5.0f, "Expected minY=5.0, got %.2f", xStats.m_fMinY);
	ZENITH_ASSERT_EQ(xStats.m_fMaxY, 10.0f, "Expected maxY=10.0, got %.2f", xStats.m_fMaxY);

}

ZENITH_TEST(AI, HasSufficientClearance) { Zenith_UnitTests::TestHasSufficientClearance(); }

void Zenith_UnitTests::TestHasSufficientClearance(){

	// Agent needs 4 cells of clearance (= fAgentHeight 3 / fCellHeight 1 + 1).
	const int32_t iAgentHeightCells = 4;

	// Helper to build a 2-span chain where the second span sits `iGap` cells above.
	auto MakeSpanPair = [](int32_t iGap) -> Zenith_NavMeshGenerator::VoxelSpan*
	{
		auto* pxLower = new Zenith_NavMeshGenerator::VoxelSpan();
		pxLower->m_uMinY = 0; pxLower->m_uMaxY = 10; pxLower->m_uAreaType = 1; pxLower->m_uRegion = 0;

		auto* pxUpper = new Zenith_NavMeshGenerator::VoxelSpan();
		pxUpper->m_uMinY = static_cast<uint16_t>(10 + iGap);
		pxUpper->m_uMaxY = pxUpper->m_uMinY + 5;
		pxUpper->m_uAreaType = 1;
		pxUpper->m_uRegion = 0;
		pxUpper->m_pxNext = nullptr;

		pxLower->m_pxNext = pxUpper;
		return pxLower;
	};

	auto FreeChain = [](Zenith_NavMeshGenerator::VoxelSpan* pxSpan)
	{
		while (pxSpan)
		{
			Zenith_NavMeshGenerator::VoxelSpan* pxNext = pxSpan->m_pxNext;
			delete pxSpan;
			pxSpan = pxNext;
		}
	};

	// Gap = 2 (< 4): insufficient clearance.
	{
		Zenith_NavMeshGenerator::VoxelSpan* pxChain = MakeSpanPair(2);
		ZENITH_ASSERT_TRUE(Zenith_NavMeshGenerator::HasInsufficientClearance(pxChain, iAgentHeightCells), "Gap of 2 with agent height 4 should be flagged insufficient");
		FreeChain(pxChain);
	}

	// Gap = 10 (> agent height): sufficient clearance.
	{
		Zenith_NavMeshGenerator::VoxelSpan* pxChain = MakeSpanPair(10);
		ZENITH_ASSERT_FALSE(Zenith_NavMeshGenerator::HasInsufficientClearance(pxChain, iAgentHeightCells), "Gap of 10 with agent height 4 should be flagged sufficient");
		FreeChain(pxChain);
	}

	// No span above: sufficient clearance (open sky).
	{
		auto* pxSolo = new Zenith_NavMeshGenerator::VoxelSpan();
		pxSolo->m_uMinY = 0; pxSolo->m_uMaxY = 10; pxSolo->m_uAreaType = 1; pxSolo->m_pxNext = nullptr;
		ZENITH_ASSERT_FALSE(Zenith_NavMeshGenerator::HasInsufficientClearance(pxSolo, iAgentHeightCells), "No span above should be sufficient clearance");
		delete pxSolo;
	}

}

ZENITH_TEST(AI, MergeOverlappingSpans) { Zenith_UnitTests::TestMergeOverlappingSpans(); }

void Zenith_UnitTests::TestMergeOverlappingSpans(){

	// Build the column via AddSpan so the sort order is correct, then assert
	// on the merge outcome. AddSpan already calls MergeOverlappingSpans.
	auto FreeColumn = [](Zenith_NavMeshGenerator::HeightfieldColumn& xColumn)
	{
		Zenith_NavMeshGenerator::VoxelSpan* pxSpan = xColumn.m_pxFirstSpan;
		while (pxSpan)
		{
			Zenith_NavMeshGenerator::VoxelSpan* pxNext = pxSpan->m_pxNext;
			delete pxSpan;
			pxSpan = pxNext;
		}
		xColumn.m_pxFirstSpan = nullptr;
	};

	auto CountSpans = [](const Zenith_NavMeshGenerator::HeightfieldColumn& xColumn) -> u_int
	{
		u_int uCount = 0;
		for (Zenith_NavMeshGenerator::VoxelSpan* pxSpan = xColumn.m_pxFirstSpan; pxSpan; pxSpan = pxSpan->m_pxNext)
			uCount++;
		return uCount;
	};

	// Case 1: two overlapping same-area-type spans merge into one.
	{
		Zenith_NavMeshGenerator::HeightfieldColumn xColumn;
		Zenith_NavMeshGenerator::AddSpan(xColumn, 0, 10, /*uAreaType*/ 1);
		Zenith_NavMeshGenerator::AddSpan(xColumn, 5, 15, /*uAreaType*/ 1);

		ZENITH_ASSERT_EQ(CountSpans(xColumn), 1, "Same-type overlapping spans should merge to one");
		ZENITH_ASSERT_EQ(xColumn.m_pxFirstSpan->m_uMinY, 0, "Merged span minY should be 0");
		ZENITH_ASSERT_EQ(xColumn.m_pxFirstSpan->m_uMaxY, 15, "Merged span maxY should be 15");
		FreeColumn(xColumn);
	}

	// Case 2: two disjoint same-area-type spans stay separate.
	{
		Zenith_NavMeshGenerator::HeightfieldColumn xColumn;
		Zenith_NavMeshGenerator::AddSpan(xColumn, 0, 5, /*uAreaType*/ 1);
		Zenith_NavMeshGenerator::AddSpan(xColumn, 10, 15, /*uAreaType*/ 1);

		ZENITH_ASSERT_EQ(CountSpans(xColumn), 2, "Disjoint spans should not merge");
		FreeColumn(xColumn);
	}

	// Case 3: overlapping spans with different area types — walkable below a
	// blocker gets truncated to just under the blocker.
	{
		Zenith_NavMeshGenerator::HeightfieldColumn xColumn;
		Zenith_NavMeshGenerator::AddSpan(xColumn, 0, 10, /*uAreaType*/ 1);  // walkable
		Zenith_NavMeshGenerator::AddSpan(xColumn, 5, 20, /*uAreaType*/ 0);  // blocker

		ZENITH_ASSERT_EQ(CountSpans(xColumn), 2, "Mixed-type overlap should keep both spans");
		Zenith_NavMeshGenerator::VoxelSpan* pxFirst = xColumn.m_pxFirstSpan;
		ZENITH_ASSERT_EQ(pxFirst->m_uAreaType, 1, "First span should be the walkable one");
		ZENITH_ASSERT_EQ(pxFirst->m_uMaxY, 5, "Walkable span should be truncated to blocker's minY");
		FreeColumn(xColumn);
	}

	// Case 4: three consecutive same-type spans collapse in one pass.
	{
		Zenith_NavMeshGenerator::HeightfieldColumn xColumn;
		Zenith_NavMeshGenerator::AddSpan(xColumn, 0, 4, /*uAreaType*/ 1);
		Zenith_NavMeshGenerator::AddSpan(xColumn, 3, 8, /*uAreaType*/ 1);
		Zenith_NavMeshGenerator::AddSpan(xColumn, 7, 12, /*uAreaType*/ 1);

		ZENITH_ASSERT_EQ(CountSpans(xColumn), 1, "Chained overlaps should collapse to one");
		ZENITH_ASSERT_EQ(xColumn.m_pxFirstSpan->m_uMinY, 0, "Chain-merged span minY should be 0");
		ZENITH_ASSERT_EQ(xColumn.m_pxFirstSpan->m_uMaxY, 12, "Chain-merged span maxY should be 12");
		FreeColumn(xColumn);
	}

}

// ============================================================================
// Physics Mesh Generator Helper Tests
// ============================================================================

ZENITH_TEST(AI, FindExtremeVertexIndices) { Zenith_UnitTests::TestFindExtremeVertexIndices(); }

void Zenith_UnitTests::TestFindExtremeVertexIndices(){

	Zenith_Vector<Zenith_Maths::Vector3> xPositions;
	xPositions.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(-5.0f, 3.0f, 1.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(7.0f, -2.0f, 4.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 10.0f, -8.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(2.0f, -1.0f, 12.0f));

	uint32_t auIndices[6];
	Zenith_PhysicsMeshGenerator::FindExtremeVertexIndices(xPositions, auIndices);

	// minX=-5 at index 1, maxX=7 at index 2
	ZENITH_ASSERT_EQ(auIndices[0], 1, "MinX should be at index 1");
	ZENITH_ASSERT_EQ(auIndices[1], 2, "MaxX should be at index 2");
	// minY=-2 at index 2, maxY=10 at index 3
	ZENITH_ASSERT_EQ(auIndices[2], 2, "MinY should be at index 2");
	ZENITH_ASSERT_EQ(auIndices[3], 3, "MaxY should be at index 3");
	// minZ=-8 at index 3, maxZ=12 at index 4
	ZENITH_ASSERT_EQ(auIndices[4], 3, "MinZ should be at index 3");
	ZENITH_ASSERT_EQ(auIndices[5], 4, "MaxZ should be at index 4");

}

ZENITH_TEST(AI, ComputeAABBFromPositions) { Zenith_UnitTests::TestComputeAABBFromPositions(); }

void Zenith_UnitTests::TestComputeAABBFromPositions(){

	Zenith_Vector<Zenith_Maths::Vector3> xPositions;
	xPositions.PushBack(Zenith_Maths::Vector3(1.0f, -2.0f, 3.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(-4.0f, 5.0f, -6.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_Maths::Vector3 xMin, xMax;
	Zenith_PhysicsMeshGenerator::ComputeAABBFromPositions(xPositions, xMin, xMax);

	ZENITH_ASSERT_EQ(xMin.x, -4.0f, "Min X should be -4");
	ZENITH_ASSERT_EQ(xMin.y, -2.0f, "Min Y should be -2");
	ZENITH_ASSERT_EQ(xMin.z, -6.0f, "Min Z should be -6");
	ZENITH_ASSERT_EQ(xMax.x, 1.0f, "Max X should be 1");
	ZENITH_ASSERT_EQ(xMax.y, 5.0f, "Max Y should be 5");
	ZENITH_ASSERT_EQ(xMax.z, 3.0f, "Max Z should be 3");

}

ZENITH_TEST(AI, ComputeVertexNormals) { Zenith_UnitTests::TestComputeVertexNormals(); }

void Zenith_UnitTests::TestComputeVertexNormals(){

	// Simple flat triangle in the XZ plane — normals should point along Y
	Zenith_Maths::Vector3 axPositions[3] = {
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)
	};
	Zenith_Maths::Vector3 axNormals[3];
	uint32_t auIndices[3] = { 0, 1, 2 };

	Zenith_PhysicsMeshGenerator::ComputeVertexNormals(axNormals, axPositions, 3, auIndices, 3);

	// Cross product of (1,0,0)x(0,0,1) = (0,-1,0), normalized = (0,-1,0)
	for (int i = 0; i < 3; i++)
	{
		ZENITH_ASSERT_LT(std::abs(axNormals[i].x), 0.001f, "Normal X should be ~0");
		ZENITH_ASSERT_EQ_FLOAT(std::abs(axNormals[i].y), 1.0f, 0.001f, "Normal Y should be ~+-1");
		ZENITH_ASSERT_LT(std::abs(axNormals[i].z), 0.001f, "Normal Z should be ~0");
	}

}

// ============================================================================
// Zenith_AITests implementations
// ============================================================================
// ----------------------------------------------------------------------------
// Helper to build minimal compact heightfield arrays for testing.
// Creates a flat grid of iWidth x iHeight columns, one span per column,
// all at the same height.  Skipped columns can be specified via
// abSkipColumn (indexed by column index; true = no span in that column).
// ----------------------------------------------------------------------------
Zenith_AITests::TestCompactHF Zenith_AITests::BuildTestHF(
	int32_t iWidth, int32_t iHeight,
	const uint16_t* auHeights,
	const bool* abSkipColumn)
{
	TestCompactHF xHF;
	uint32_t uTotalColumns = static_cast<uint32_t>(iWidth * iHeight);

	xHF.m_axColumnSpanCounts.Reserve(uTotalColumns);
	xHF.m_axColumnSpanStarts.Reserve(uTotalColumns);

	for (uint32_t u = 0; u < uTotalColumns; ++u)
	{
		bool bSkip = abSkipColumn && abSkipColumn[u];
		xHF.m_axColumnSpanStarts.PushBack(xHF.m_axSpans.GetSize());

		if (bSkip)
		{
			xHF.m_axColumnSpanCounts.PushBack(0);
		}
		else
		{
			Zenith_NavMeshGenerator::CompactSpan xSpan;
			xSpan.m_uY = auHeights[u];
			xSpan.m_uRegion = 0;
			xSpan.m_uNeighbors[0] = 0;
			xSpan.m_uNeighbors[1] = 0;
			xSpan.m_uNeighbors[2] = 0;
			xSpan.m_uNeighbors[3] = 0;
			xHF.m_axSpans.PushBack(xSpan);
			xHF.m_axSpanToColumn.PushBack(u);
			xHF.m_axColumnSpanCounts.PushBack(1);
		}
	}

	return xHF;
}

ZENITH_TEST(AI, FloodFillAssignsConnected) { Zenith_AITests::TestFloodFillAssignsConnected(); }

void Zenith_AITests::TestFloodFillAssignsConnected(){

	// 3x3 flat grid, all spans at the same height, no gaps
	const int32_t iWidth = 3;
	const int32_t iHeight = 3;
	const uint16_t auHeights[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Flood fill from span 0 with region ID 1, max step = 1 cell
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/0,
		/*uRegionID=*/1);

	// All 9 spans should now have region 1
	for (uint32_t u = 0; u < xHF.m_axSpans.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ(xHF.m_axSpans.Get(u).m_uRegion, 1, "All connected spans should be assigned to region 1");
	}

}

ZENITH_TEST(AI, FloodFillStopsAtBoundary) { Zenith_AITests::TestFloodFillStopsAtBoundary(); }

void Zenith_AITests::TestFloodFillStopsAtBoundary(){

	// 3x1 grid.  Middle column (index 1) is empty, creating a gap.
	// Columns 0 and 2 each have one span at the same height.
	const int32_t iWidth = 3;
	const int32_t iHeight = 1;
	const uint16_t auHeights[3] = {10, 0, 10};
	const bool abSkip[3] = {false, true, false};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, abSkip);

	// We should have exactly 2 spans (columns 0 and 2)
	ZENITH_ASSERT_EQ(xHF.m_axSpans.GetSize(), 2, "Should have 2 spans");

	// Flood fill from span 0 (column 0) with region 1
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/0,
		/*uRegionID=*/1);

	// Span 0 should be region 1, span 1 (column 2) should still be 0 (unassigned)
	ZENITH_ASSERT_EQ(xHF.m_axSpans.Get(0).m_uRegion, 1, "Start span should be assigned region 1");
	ZENITH_ASSERT_EQ(xHF.m_axSpans.Get(1).m_uRegion, 0, "Disconnected span should remain unassigned (region 0)");

	// Now flood fill the second span separately with region 2
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/1,
		/*uRegionID=*/2);

	ZENITH_ASSERT_EQ(xHF.m_axSpans.Get(1).m_uRegion, 2, "Second disconnected span should be assigned region 2");

}

ZENITH_TEST(AI, ColumnHasSpanInRegionFound) { Zenith_AITests::TestColumnHasSpanInRegionFound(); }

void Zenith_AITests::TestColumnHasSpanInRegionFound(){

	// 2x2 grid, all columns have one span.
	// Assign column (1,0) a region of 5.
	const int32_t iWidth = 2;
	const int32_t iHeight = 2;
	const uint16_t auHeights[4] = {10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Manually assign region 5 to the span at column (1, 0) = column index 1 = span index 1
	xHF.m_axSpans.Get(1).m_uRegion = 5;

	bool bFound = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/1, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/5);

	ZENITH_ASSERT_TRUE(bFound, "Should find span with region 5 in column (1,0)");

}

ZENITH_TEST(AI, ColumnHasSpanInRegionNotFound) { Zenith_AITests::TestColumnHasSpanInRegionNotFound(); }

void Zenith_AITests::TestColumnHasSpanInRegionNotFound(){

	// 2x2 grid, all regions are 0 (default)
	const int32_t iWidth = 2;
	const int32_t iHeight = 2;
	const uint16_t auHeights[4] = {10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Search for region 7 which does not exist
	bool bFound = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/0, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/7);

	ZENITH_ASSERT_FALSE(bFound, "Should NOT find span with region 7");

	// Also test out-of-bounds coordinates
	bool bOutOfBounds = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/-1, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/0);

	ZENITH_ASSERT_FALSE(bOutOfBounds, "Out-of-bounds coordinates should return false");

}
