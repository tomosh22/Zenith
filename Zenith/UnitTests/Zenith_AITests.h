#pragma once

#include "AI/Navigation/Zenith_NavMeshGenerator.h"

/**
 * Zenith_AITests - Unit tests for AI subsystem internals
 *
 * Tests cover:
 * - NavMesh generator extracted helpers (FloodFillRegion, ColumnHasSpanInRegion)
 */
class Zenith_AITests
{
public:
	static void RunAllTests();

private:
	// Test helper data structure
	struct TestCompactHF
	{
		Zenith_Vector<Zenith_NavMeshGenerator::CompactSpan> m_axSpans;
		Zenith_Vector<uint32_t> m_axColumnSpanCounts;
		Zenith_Vector<uint32_t> m_axColumnSpanStarts;
		Zenith_Vector<uint32_t> m_axSpanToColumn;
	};

	static TestCompactHF BuildTestHF(
		int32_t iWidth, int32_t iHeight,
		const uint16_t* auHeights,
		const bool* abSkipColumn);

	//==========================================================================
	// FloodFillRegion tests
	//==========================================================================
	static void TestFloodFillAssignsConnected();
	static void TestFloodFillStopsAtBoundary();

	//==========================================================================
	// ColumnHasSpanInRegion tests
	//==========================================================================
	static void TestColumnHasSpanInRegionFound();
	static void TestColumnHasSpanInRegionNotFound();
};
