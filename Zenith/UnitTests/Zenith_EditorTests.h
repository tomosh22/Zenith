#pragma once

#ifdef ZENITH_TOOLS

class Zenith_EditorTests
{
public:
	static void RunAllTests();

private:
	// Selection system tests
	static void TestBoundingBoxIntersection();
	static void TestSelectionSystemEmptyScene();
	static void TestInvalidEntityID();

	// Transform tests (for property panel)
	static void TestTransformRoundTrip();

	// Multi-select tests
	static void TestMultiSelectSingle();
	static void TestMultiSelectCtrlClick();
	static void TestMultiSelectClear();
	static void TestMultiSelectAfterEntityDelete();
};

#endif // ZENITH_TOOLS
