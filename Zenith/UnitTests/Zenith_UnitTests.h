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
};