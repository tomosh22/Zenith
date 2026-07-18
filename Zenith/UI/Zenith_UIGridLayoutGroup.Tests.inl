#include "Core/Zenith_TestFramework.h"

// ============================================================================
// Zenith_UIGridLayoutGroup unit tests (category: UIGrid).
//
// Lock the pure CPU-side grid-layout math added by Zenith_UIGridLayoutGroup:
//   * Row-major placement into fixed cells, with columns / cell-size / spacing /
//     padding honored, and wrapping onto new rows.
//   * Visible-only placement (hidden children leave no gap) and the empty /
//     all-invisible no-op case.
//   * Fit-to-content auto-sizing of the container to the occupied-cell bounds.
//   * Serialization round-trip of every grid field.
//
// This file is textually included at the bottom of Zenith_UIGridLayoutGroup.cpp
// (under ZENITH_TESTING) AFTER `} // namespace Zenith_UI`, so the class is
// qualified as Zenith_UI::Zenith_UIGridLayoutGroup and Zenith_DataStream is
// already in scope.
//
// HEADLESS-SAFE — every test drives layout via Update(0.f) and asserts
// CPU-side position/size/getter state ONLY. Render() (which needs a live canvas
// + GPU quad queue) is NEVER called.
// ============================================================================

ZENITH_TEST(UIGrid, ThreeColumns_PositionsRowMajor)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(3);
	xGrid.SetCellSize(40.f, 40.f);
	xGrid.SetSpacing(0.f, 0.f);
	xGrid.SetPadding(0.f, 0.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[6];
	for (uint32_t u = 0; u < 6; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	const float afExpectX[6] = { 0.f, 40.f, 80.f, 0.f, 40.f, 80.f };
	const float afExpectY[6] = { 0.f, 0.f, 0.f, 40.f, 40.f, 40.f };
	for (uint32_t u = 0; u < 6; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, afExpectX[u], 0.001f,
			"child X must be row-major across 3 columns");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, afExpectY[u], 0.001f,
			"child Y must advance one cell per row");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetSize().x, 40.f, 0.001f,
			"child must be resized to cell width");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetSize().y, 40.f, 0.001f,
			"child must be resized to cell height");
	}
}

ZENITH_TEST(UIGrid, CellSizeAndSpacingHonored)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(2);
	xGrid.SetCellSize(50.f, 30.f);
	xGrid.SetSpacing(10.f, 5.f);
	xGrid.SetPadding(0.f, 0.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[4];
	for (uint32_t u = 0; u < 4; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	// pitch = cell + spacing -> 60 horizontally, 35 vertically.
	const float afExpectX[4] = { 0.f, 60.f, 0.f, 60.f };
	const float afExpectY[4] = { 0.f, 0.f, 35.f, 35.f };
	for (uint32_t u = 0; u < 4; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, afExpectX[u], 0.001f,
			"cell width + horizontal spacing must set the column pitch");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, afExpectY[u], 0.001f,
			"cell height + vertical spacing must set the row pitch");
	}
}

ZENITH_TEST(UIGrid, SeventhChildWrapsToRow2)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(3);
	xGrid.SetCellSize(20.f, 20.f);
	xGrid.SetSpacing(0.f, 0.f);
	xGrid.SetPadding(0.f, 0.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[7];
	for (uint32_t u = 0; u < 7; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	// child[3] starts row 1, child[6] starts row 2.
	ZENITH_ASSERT_EQ_FLOAT(axChildren[3].GetPosition().x, 0.f, 0.001f,
		"child 3 wraps to the start of row 1");
	ZENITH_ASSERT_EQ_FLOAT(axChildren[3].GetPosition().y, 20.f, 0.001f,
		"child 3 sits one cell down");
	ZENITH_ASSERT_EQ_FLOAT(axChildren[6].GetPosition().x, 0.f, 0.001f,
		"child 6 wraps to the start of row 2");
	ZENITH_ASSERT_EQ_FLOAT(axChildren[6].GetPosition().y, 40.f, 0.001f,
		"child 6 sits two cells down");
}

ZENITH_TEST(UIGrid, PaddingOffsetsFirstCell)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(3);
	xGrid.SetCellSize(40.f, 40.f);
	xGrid.SetSpacing(4.f, 4.f);
	xGrid.SetPadding(8.f, 12.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[4];
	for (uint32_t u = 0; u < 4; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	// pitch = 44; first cell offset by (padL=8, padT=12).
	const float afExpectX[4] = { 8.f, 52.f, 96.f, 8.f };
	const float afExpectY[4] = { 12.f, 12.f, 12.f, 56.f };
	for (uint32_t u = 0; u < 4; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, afExpectX[u], 0.001f,
			"left/top padding must offset the first cell and carry through the pitch");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, afExpectY[u], 0.001f,
			"top padding must offset every row origin");
	}
}

ZENITH_TEST(UIGrid, PartialLastRowPlacesRemainder)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(3);
	xGrid.SetCellSize(30.f, 30.f);
	xGrid.SetSpacing(0.f, 0.f);
	xGrid.SetPadding(0.f, 0.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[5];
	for (uint32_t u = 0; u < 5; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	// A short final row (2 of 3) must not crash and simply places the remainder.
	const float afExpectX[5] = { 0.f, 30.f, 60.f, 0.f, 30.f };
	const float afExpectY[5] = { 0.f, 0.f, 0.f, 30.f, 30.f };
	for (uint32_t u = 0; u < 5; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, afExpectX[u], 0.001f,
			"partial last row places the remaining children left-to-right");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, afExpectY[u], 0.001f,
			"partial last row shares the same row origin");
	}
}

ZENITH_TEST(UIGrid, EmptyAndAllInvisibleAreNoOp)
{
	// (a) Zero children, fit-to-content -> container is just the padding box.
	{
		Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
		xGrid.SetFitToContent(true);
		xGrid.SetPadding(5.f, 6.f, 7.f, 8.f);

		xGrid.Update(0.f);

		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().x, 12.f, 0.001f,
			"empty grid width must be padL + padR");
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().y, 14.f, 0.001f,
			"empty grid height must be padT + padB");
	}

	// (b) All children hidden -> same padding-only size, children untouched at (0,0).
	{
		Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
		xGrid.SetFitToContent(true);
		xGrid.SetPadding(5.f, 6.f, 7.f, 8.f);

		Zenith_UI::Zenith_UIElement axChildren[3];
		for (uint32_t u = 0; u < 3; ++u)
		{
			xGrid.AddChild(&axChildren[u]);
			axChildren[u].SetVisible(false);
		}

		xGrid.Update(0.f);

		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().x, 12.f, 0.001f,
			"all-invisible grid width must collapse to the padding box");
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().y, 14.f, 0.001f,
			"all-invisible grid height must collapse to the padding box");
		for (uint32_t u = 0; u < 3; ++u)
		{
			ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, 0.f, 0.001f,
				"hidden children must keep their default X");
			ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, 0.f, 0.001f,
				"hidden children must keep their default Y");
		}
	}
}

ZENITH_TEST(UIGrid, SingleColumnIsVerticalStack)
{
	Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
	xGrid.SetColumns(1);
	xGrid.SetCellSize(40.f, 40.f);
	xGrid.SetSpacing(0.f, 5.f);
	xGrid.SetPadding(0.f, 0.f, 0.f, 0.f);

	Zenith_UI::Zenith_UIElement axChildren[3];
	for (uint32_t u = 0; u < 3; ++u)
		xGrid.AddChild(&axChildren[u]);

	xGrid.Update(0.f);

	// One column degenerates to a vertical stack with 45px row pitch.
	const float afExpectY[3] = { 0.f, 45.f, 90.f };
	for (uint32_t u = 0; u < 3; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().x, 0.f, 0.001f,
			"single-column grid keeps every child at X = padL");
		ZENITH_ASSERT_EQ_FLOAT(axChildren[u].GetPosition().y, afExpectY[u], 0.001f,
			"single-column grid stacks children by cell + vertical spacing");
	}
}

ZENITH_TEST(UIGrid, AutoSizeFitsContentBounds)
{
	// (a) 5 children over 3 columns -> 3 used cols, 2 used rows.
	{
		Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
		xGrid.SetColumns(3);
		xGrid.SetCellSize(40.f, 40.f);
		xGrid.SetSpacing(10.f, 10.f);
		xGrid.SetPadding(4.f, 4.f, 4.f, 4.f);
		xGrid.SetFitToContent(true);

		Zenith_UI::Zenith_UIElement axChildren[5];
		for (uint32_t u = 0; u < 5; ++u)
			xGrid.AddChild(&axChildren[u]);

		xGrid.Update(0.f);

		// W = 3*40 + 2*10 + padL4 + padR4 = 148; H = 2*40 + 1*10 + padT4 + padB4 = 98.
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().x, 148.f, 0.001f,
			"fit width must span the used columns plus spacing plus horizontal padding");
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().y, 98.f, 0.001f,
			"fit height must span the used rows plus spacing plus vertical padding");
	}

	// (b) 2 children over 3 columns -> 2 used cols, 1 used row.
	{
		Zenith_UI::Zenith_UIGridLayoutGroup xGrid;
		xGrid.SetColumns(3);
		xGrid.SetCellSize(40.f, 40.f);
		xGrid.SetSpacing(10.f, 10.f);
		xGrid.SetPadding(4.f, 4.f, 4.f, 4.f);
		xGrid.SetFitToContent(true);

		Zenith_UI::Zenith_UIElement axChildren[2];
		for (uint32_t u = 0; u < 2; ++u)
			xGrid.AddChild(&axChildren[u]);

		xGrid.Update(0.f);

		// W = 2*40 + 1*10 + 8 = 98; H = 1*40 + 0 + 8 = 48.
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().x, 98.f, 0.001f,
			"a single partial row still counts only the used columns");
		ZENITH_ASSERT_EQ_FLOAT(xGrid.GetSize().y, 48.f, 0.001f,
			"a single row has no vertical inter-row spacing");
	}
}

ZENITH_TEST(UIGrid, SerializationRoundTrips)
{
	Zenith_UI::Zenith_UIGridLayoutGroup src;
	src.SetColumns(4);
	src.SetCellSize(24.f, 24.f);
	src.SetSpacing(6.f, 8.f);
	src.SetPadding(1.f, 2.f, 3.f, 4.f);
	src.SetFitToContent(false);

	Zenith_DataStream xStream;
	src.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Zenith_UI::Zenith_UIGridLayoutGroup dst;
	dst.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(dst.GetColumns(), 4u, "columns must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetCellSize().x, 24.f, 0.001f, "cell width must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetCellSize().y, 24.f, 0.001f, "cell height must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetSpacing().x, 6.f, 0.001f, "horizontal spacing must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetSpacing().y, 8.f, 0.001f, "vertical spacing must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetPadding().x, 1.f, 0.001f, "pad left must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetPadding().y, 2.f, 0.001f, "pad top must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetPadding().z, 3.f, 0.001f, "pad right must survive serialization");
	ZENITH_ASSERT_EQ_FLOAT(dst.GetPadding().w, 4.f, 0.001f, "pad bottom must survive serialization");
	ZENITH_ASSERT_EQ(dst.GetFitToContent(), false, "fit-to-content flag must survive serialization");
}
