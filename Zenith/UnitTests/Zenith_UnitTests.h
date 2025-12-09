#pragma once
#include <fstream>

class Zenith_UnitTests
{
public:
	static void RunAllTests();
private:
	static void TestDataStream();
	static void TestMemoryManagement();
	static void TestProfiling();
	static void TestVector();
	static void TestMemoryPool();

	// Scene serialization tests
	static void TestSceneSerialization();
	static void TestComponentSerialization();
	static void TestEntitySerialization();
	static void TestSceneRoundTrip();
};

// Include editor tests separately as they are only available in ZENITH_TOOLS builds
#ifdef ZENITH_TOOLS
#include "Zenith_EditorTests.h"
#endif