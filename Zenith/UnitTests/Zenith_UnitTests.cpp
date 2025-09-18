#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "Collections/Zenith_MemoryPool.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Types.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"

void Zenith_UnitTests::RunAllTests()
{
	TestDataStream();
	TestMemoryManagement();
	TestProfiling();
	TestVector();
	TestMemoryPool();
}

void Zenith_UnitTests::TestDataStream()
{
	Zenith_DataStream xStream(1);

	const char* szTestData = "This is a test string";
	constexpr u_int uTestDataLen = 22;
	xStream.WriteData(szTestData, uTestDataLen);

	xStream << uint32_t(5u);
	xStream << float(2000.f);
	xStream << Zenith_Maths::Vector3(1, 2, 3);
	xStream << std::unordered_map<std::string, std::pair<uint32_t, uint64_t>>({{"Test", {20, 100}}});
	xStream << std::vector<double>({ 3245., -1119. });

	xStream.SetCursor(0);

	char acTestData[uTestDataLen];
	xStream.ReadData(acTestData, uTestDataLen);
	Zenith_Assert(!strcmp(acTestData, szTestData));

	uint32_t u5;
	xStream >> u5;
	Zenith_Assert(u5 == 5);

	float f2000;
	xStream >> f2000;
	Zenith_Assert(f2000 == 2000.f);

	Zenith_Maths::Vector3 x123;
	xStream >> x123;
	Zenith_Assert(x123 == Zenith_Maths::Vector3(1, 2, 3));

	std::unordered_map<std::string, std::pair<uint32_t, uint64_t>> xUnorderedMap;
	xStream >> xUnorderedMap;
	Zenith_Assert((xUnorderedMap.at("Test") == std::pair<uint32_t, uint64_t>(20, 100)));

	std::vector<double> xVector;
	xStream >> xVector;
	Zenith_Assert(xVector.at(0) == 3245. && xVector.at(1) == -1119.);
}

void Zenith_UnitTests::TestMemoryManagement()
{
	int* piTest = new int[10];
	delete[] piTest;
}

struct TestData
{
	bool Validate() { return m_uIn == m_uOut; }
	u_int m_uIn, m_uOut;
};
static void Test(void* pData)
{
	TestData& xTestData = *static_cast<TestData*>(pData);
	xTestData.m_uOut = xTestData.m_uIn;
}

void Zenith_UnitTests::TestProfiling()
{
	constexpr Zenith_ProfileIndex eIndex0 = ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES;
	constexpr Zenith_ProfileIndex eIndex1 = ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES;

	Zenith_Profiling::BeginFrame();
	
	Zenith_Profiling::BeginProfile(eIndex0);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex0, "Profiling index wasn't set correctly");
	Zenith_Profiling::BeginProfile(eIndex1);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex1, "Profiling index wasn't set correctly");
	Zenith_Profiling::EndProfile(eIndex1);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex0, "Profiling index wasn't set correctly");
	Zenith_Profiling::EndProfile(eIndex0);

	TestData xTest0 = { 0, -1 }, xTest1 = { 1,-1 }, xTest2 = { 2, -1 };
	Zenith_Task* pxTask0 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SHADOWS, Test, &xTest0);
	Zenith_Task* pxTask1 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, Test, &xTest1);
	Zenith_Task* pxTask2 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Test, &xTest2);
	Zenith_TaskSystem::SubmitTask(pxTask0);
	Zenith_TaskSystem::SubmitTask(pxTask1);
	Zenith_TaskSystem::SubmitTask(pxTask2);
	pxTask0->WaitUntilComplete();
	pxTask1->WaitUntilComplete();
	pxTask2->WaitUntilComplete();

	Zenith_Assert(xTest0.Validate(), "");
	Zenith_Assert(xTest1.Validate(), "");
	Zenith_Assert(xTest2.Validate(), "");

	const std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>>& xEvents = Zenith_Profiling::GetEvents();
	const Zenith_Vector<Zenith_Profiling::Event>& xEventsMain = xEvents.at(Zenith_Multithreading::GetCurrentThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents0 = xEvents.at(pxTask0->GetCompletedThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents1 = xEvents.at(pxTask0->GetCompletedThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents2 = xEvents.at(pxTask0->GetCompletedThreadID());

	Zenith_Assert(xEventsMain.GetSize() == 8, "Expected 8 events, have %u", xEvents.size());
	Zenith_Assert(xEventsMain.Get(0).m_eIndex == eIndex1, "Wrong profile index");
	Zenith_Assert(xEventsMain.Get(1).m_eIndex == eIndex0, "Wrong profile index");

	delete pxTask0;
	delete pxTask1;
	delete pxTask2;

	Zenith_Profiling::EndFrame();
}

void Zenith_UnitTests::TestVector()
{
	constexpr u_int uNUM_TESTS = 1024;

	Zenith_Vector<u_int> xUIntVector(1);

	for (u_int u = 0; u < uNUM_TESTS / 2; u++)
	{
		xUIntVector.PushBack(u);
		Zenith_Assert(xUIntVector.GetFront() == 0);
		Zenith_Assert(xUIntVector.GetBack() == u);
	}

	for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS; u++)
	{
		xUIntVector.EmplaceBack((u_int&&)u_int(u));
		Zenith_Assert(xUIntVector.GetFront() == 0);
		Zenith_Assert(xUIntVector.GetBack() == u);
	}

	for (u_int u = 0; u < uNUM_TESTS; u++)
	{
		Zenith_Assert(xUIntVector.Get(u) == u);
	}

	constexpr u_int uNUM_REMOVALS = uNUM_TESTS / 10;
	for (u_int u = 0; u < uNUM_REMOVALS; u++)
	{
		xUIntVector.Remove(uNUM_TESTS / 2);
		Zenith_Assert(xUIntVector.Get(uNUM_TESTS / 2) == uNUM_TESTS / 2 + u + 1);
	}

	Zenith_Vector<u_int> xCopy0 = xUIntVector;
	Zenith_Vector<u_int> xCopy1(xUIntVector);

	auto xTest = [uNUM_TESTS, uNUM_REMOVALS](Zenith_Vector<u_int> xVector)
	{
		for (u_int u = 0; u < uNUM_TESTS / 2; u++)
		{
			Zenith_Assert(xVector.Get(u) == u);
		}

		for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS - uNUM_REMOVALS; u++)
		{
			Zenith_Assert(xVector.Get(u) == u + uNUM_REMOVALS);
		}
	};

	xTest(xUIntVector);
	xTest(xCopy0);
	xTest(xCopy1);
}

class MemoryPoolTest
{
public:
	static int s_uCount;

	explicit MemoryPoolTest(u_int& uOut)
	: m_uTest(++s_uCount)
	{
		uOut = m_uTest;
	}

	~MemoryPoolTest()
	{
		s_uCount--;
	}

	int m_uTest;
};
int MemoryPoolTest::s_uCount = 0;

void Zenith_UnitTests::TestMemoryPool()
{
	constexpr u_int uPOOL_SIZE = 128;
	Zenith_MemoryPool<MemoryPoolTest, uPOOL_SIZE> xPool;
	MemoryPoolTest* apxTest[uPOOL_SIZE];

	Zenith_Assert(MemoryPoolTest::s_uCount == 0);

	for (u_int u = 0; u < uPOOL_SIZE / 2; u++)
	{
		u_int uTest;
		apxTest[u] = xPool.Allocate(uTest);
		Zenith_Assert(MemoryPoolTest::s_uCount == u + 1);
		Zenith_Assert(apxTest[u]->m_uTest == u + 1);
		Zenith_Assert(uTest == u + 1);
	}

	for (u_int u = 0; u < uPOOL_SIZE / 4; u++)
	{
		Zenith_Assert(apxTest[u]->m_uTest == u + 1);
		xPool.Deallocate(apxTest[u]);
		Zenith_Assert(MemoryPoolTest::s_uCount == (uPOOL_SIZE / 2) - u - 1);
	}

	Zenith_Assert(MemoryPoolTest::s_uCount == uPOOL_SIZE / 4);
}



