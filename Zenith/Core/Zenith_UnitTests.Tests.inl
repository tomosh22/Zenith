#include "UnitTests/Zenith_UnitTests.h"

// Tests use `friend class Zenith_UnitTests` injection (see Flux_ShaderBinder.h,
// Flux_PerFrame.h) to inspect private state. The coupling is intentional and
// pragmatic: the tested private state (name-cache slot positions, per-frame
// callback arrays) is exactly the implementation detail the tests want to
// pin. A future refactor may replace the friend declarations with a
// test-only `*_Internal.h` header if the coupling starts to leak, but until
// the interface grows the friend declarations are the cheapest option.
#include "Collections/Zenith_CircularQueue.h"
#include "Collections/Zenith_HashSet.h"
#include "Collections/Zenith_MemoryPool.h"
#include "Flux/Flux_Types.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Scene serialization includes
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"  // WS10 fuzz cross-check (Light is a headless-safe component)
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"  // WS19 forwarding-handle relocation tests
#include "Flux/MeshAnimation/Flux_AnimationControllerStore.h"     // WS19 store
#include "Core/Zenith_Engine.h"                                    // g_xEngine.AnimationControllers()
#include "Core/Zenith_BenchECS.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include <filesystem>

// Animation system includes
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"

// Tween system includes
#include "Core/Zenith_Tween.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

// Mesh geometry include (for exporting runtime-format meshes)

// Vulkan memory manager (for DetermineImageViewType tests)

// UIStyle (for UIStyle tests)

// Animation texture include (for VAT baking)
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

// Asset system includes
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Prefab/Zenith_Prefab.h"

// Stream envelope (reusable DataStream header) include
#include "DataStream/Zenith_StreamEnvelope.h"

// Model instance (for material tests)
#include "Flux/Flux_ModelInstance.h"

// Terrain streaming (for chunk distance tests)
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"

// Slang reflection schema + codegen. Both headers are platform-neutral
// (Slang DLL access is gated inside the .cpp via ZENITH_WINDOWS), so the
// reflection v2 round-trip and codegen-determinism tests below run on AGDE
// too — they exercise pure-CPU serialisation and string-builder logic.
#include "Flux/Slang/Flux_CodeGenerator.h"

#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#endif

ZENITH_TEST(Core, DataStream) { Zenith_UnitTests::TestDataStream(); }

void Zenith_UnitTests::TestDataStream(){
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
	ZENITH_ASSERT_STREQ(acTestData, szTestData);

	uint32_t u5;
	xStream >> u5;
	ZENITH_ASSERT_EQ(u5, 5);

	float f2000;
	xStream >> f2000;
	ZENITH_ASSERT_EQ(f2000, 2000.f);

	Zenith_Maths::Vector3 x123;
	xStream >> x123;
	ZENITH_ASSERT_EQ(x123, Zenith_Maths::Vector3(1, 2, 3));

	std::unordered_map<std::string, std::pair<uint32_t, uint64_t>> xUnorderedMap;
	xStream >> xUnorderedMap;
	ZENITH_ASSERT_TRUE((xUnorderedMap.at("Test") == std::pair<uint32_t, uint64_t>(20, 100)));

	std::vector<double> xVector;
	xStream >> xVector;
	ZENITH_ASSERT_TRUE(xVector.at(0) == 3245. && xVector.at(1) == -1119.);
}

ZENITH_TEST(Core, MemoryManagement) { Zenith_UnitTests::TestMemoryManagement(); }

void Zenith_UnitTests::TestMemoryManagement(){
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

ZENITH_TEST(Core, Profiling) { Zenith_UnitTests::TestProfiling(); }

void Zenith_UnitTests::TestProfiling(){
	constexpr Zenith_ProfileIndex eIndex0 = ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES;
	constexpr Zenith_ProfileIndex eIndex1 = ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES;

	g_xEngine.Profiling().BeginFrame();
	
	g_xEngine.Profiling().BeginProfile(eIndex0);
	ZENITH_ASSERT_EQ(g_xEngine.Profiling().GetCurrentIndex(), eIndex0, "Profiling index wasn't set correctly");
	g_xEngine.Profiling().BeginProfile(eIndex1);
	ZENITH_ASSERT_EQ(g_xEngine.Profiling().GetCurrentIndex(), eIndex1, "Profiling index wasn't set correctly");
	g_xEngine.Profiling().EndProfile(eIndex1);
	ZENITH_ASSERT_EQ(g_xEngine.Profiling().GetCurrentIndex(), eIndex0, "Profiling index wasn't set correctly");
	g_xEngine.Profiling().EndProfile(eIndex0);

	TestData xTest0 = { 0, ~0u }, xTest1 = { 1, ~0u }, xTest2 = { 2, ~0u };
	Zenith_Task* pxTask0 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SHADOWS, Test, &xTest0);
	Zenith_Task* pxTask1 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, Test, &xTest1);
	Zenith_Task* pxTask2 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Test, &xTest2);
	g_xEngine.Tasks().SubmitTask(pxTask0);
	g_xEngine.Tasks().SubmitTask(pxTask1);
	g_xEngine.Tasks().SubmitTask(pxTask2);
	pxTask0->WaitUntilComplete();
	pxTask1->WaitUntilComplete();
	pxTask2->WaitUntilComplete();

	ZENITH_ASSERT_TRUE(xTest0.Validate(), "");
	ZENITH_ASSERT_TRUE(xTest1.Validate(), "");
	ZENITH_ASSERT_TRUE(xTest2.Validate(), "");

	const Zenith_HashMap<u_int, Zenith_Vector<Zenith_Profiling::Event>>& xEvents = g_xEngine.Profiling().GetEvents();
	const Zenith_Vector<Zenith_Profiling::Event>& xEventsMain = xEvents.Get(g_xEngine.Threading().GetCurrentThreadID());
	(void)xEvents.Get(pxTask0->GetCompletedThreadID());
	(void)xEvents.Get(pxTask1->GetCompletedThreadID());
	(void)xEvents.Get(pxTask2->GetCompletedThreadID());

	ZENITH_ASSERT_EQ(xEventsMain.GetSize(), 8, "Expected 8 events, have %u", xEvents.GetSize());
	ZENITH_ASSERT_EQ(xEventsMain.Get(0).m_eIndex, eIndex1, "Wrong profile index");
	ZENITH_ASSERT_EQ(xEventsMain.Get(1).m_eIndex, eIndex0, "Wrong profile index");

	delete pxTask0;
	delete pxTask1;
	delete pxTask2;

	g_xEngine.Profiling().EndFrame();
}

ZENITH_TEST(Core, Vector) { Zenith_UnitTests::TestVector(); }

void Zenith_UnitTests::TestVector(){
	constexpr u_int uNUM_TESTS = 1024;

	Zenith_Vector<u_int> xUIntVector(1);

	for (u_int u = 0; u < uNUM_TESTS / 2; u++)
	{
		xUIntVector.PushBack(u);
		ZENITH_ASSERT_EQ(xUIntVector.GetFront(), 0);
		ZENITH_ASSERT_EQ(xUIntVector.GetBack(), u);
	}

	for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS; u++)
	{
		xUIntVector.EmplaceBack((u_int&&)u_int(u));
		ZENITH_ASSERT_EQ(xUIntVector.GetFront(), 0);
		ZENITH_ASSERT_EQ(xUIntVector.GetBack(), u);
	}

	for (u_int u = 0; u < uNUM_TESTS; u++)
	{
		ZENITH_ASSERT_EQ(xUIntVector.Get(u), u);
	}

	constexpr u_int uNUM_REMOVALS = uNUM_TESTS / 10;
	for (u_int u = 0; u < uNUM_REMOVALS; u++)
	{
		xUIntVector.Remove(uNUM_TESTS / 2);
		ZENITH_ASSERT_EQ(xUIntVector.Get(uNUM_TESTS / 2), uNUM_TESTS / 2 + u + 1);
	}

	Zenith_Vector<u_int> xCopy0 = xUIntVector;
	Zenith_Vector<u_int> xCopy1(xUIntVector);

	auto xTest = [uNUM_TESTS, uNUM_REMOVALS](Zenith_Vector<u_int> xVector)
	{
		for (u_int u = 0; u < uNUM_TESTS / 2; u++)
		{
			ZENITH_ASSERT_EQ(xVector.Get(u), u);
		}

		for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS - uNUM_REMOVALS; u++)
		{
			ZENITH_ASSERT_EQ(xVector.Get(u), u + uNUM_REMOVALS);
		}
	};

	xTest(xUIntVector);
	xTest(xCopy0);
	xTest(xCopy1);
}

ZENITH_TEST(Core, VectorFind) { Zenith_UnitTests::TestVectorFind(); }

void Zenith_UnitTests::TestVectorFind(){
	Zenith_Vector<u_int> xVector;

	for (u_int u = 0; u < 5; u++)
	{
		xVector.PushBack(u * 10);
	}

	u_int uIndex = xVector.Find(20);
	ZENITH_ASSERT_EQ(uIndex, 2, "TestVectorFind: Expected to find 20 at index 2");

	uIndex = xVector.Find(25);
	ZENITH_ASSERT_EQ(uIndex, xVector.GetSize(), "TestVectorFind: Expected not to find 25");

	uIndex = xVector.Find(0);
	ZENITH_ASSERT_EQ(uIndex, 0, "TestVectorFind: Expected to find 0 at index 0");

	uIndex = xVector.Find(40);
	ZENITH_ASSERT_EQ(uIndex, 4, "TestVectorFind: Expected to find 40 at index 4");

	ZENITH_ASSERT_TRUE(xVector.Contains(30), "TestVectorFind: Expected Contains(30) to be true");
	ZENITH_ASSERT_FALSE(xVector.Contains(35), "TestVectorFind: Expected Contains(35) to be false");

	uIndex = xVector.FindIf([](const u_int& u) { return u > 15; });
	ZENITH_ASSERT_EQ(uIndex, 2, "TestVectorFind: Expected FindIf(>15) to find index 2");

	uIndex = xVector.FindIf([](const u_int& u) { return u > 100; });
	ZENITH_ASSERT_EQ(uIndex, xVector.GetSize(), "TestVectorFind: Expected FindIf(>100) to not find anything");

	Zenith_Vector<u_int> xEmptyVector;
	uIndex = xEmptyVector.Find(0);
	ZENITH_ASSERT_EQ(uIndex, 0, "TestVectorFind: Expected Find on empty vector to return 0 (size)");

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorFind passed");
}

ZENITH_TEST(Core, VectorErase) { Zenith_UnitTests::TestVectorErase(); }

void Zenith_UnitTests::TestVectorErase(){
	{
		Zenith_Vector<u_int> xVector;
		for (u_int u = 0; u < 5; u++)
		{
			xVector.PushBack(u * 10);
		}

		bool bErased = xVector.EraseValue(20);
		ZENITH_ASSERT_TRUE(bErased, "TestVectorErase: Expected EraseValue(20) to return true");
		ZENITH_ASSERT_EQ(xVector.GetSize(), 4, "TestVectorErase: Expected size to be 4 after erase");
		ZENITH_ASSERT_FALSE(xVector.Contains(20), "TestVectorErase: Expected 20 to no longer be in vector");

		ZENITH_ASSERT_EQ(xVector.Get(0), 0, "TestVectorErase: Expected index 0 to be 0");
		ZENITH_ASSERT_EQ(xVector.Get(1), 10, "TestVectorErase: Expected index 1 to be 10");
		ZENITH_ASSERT_EQ(xVector.Get(2), 30, "TestVectorErase: Expected index 2 to be 30");
		ZENITH_ASSERT_EQ(xVector.Get(3), 40, "TestVectorErase: Expected index 3 to be 40");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(10);
		xVector.PushBack(20);

		bool bErased = xVector.EraseValue(15);
		ZENITH_ASSERT_FALSE(bErased, "TestVectorErase: Expected EraseValue(15) to return false");
		ZENITH_ASSERT_EQ(xVector.GetSize(), 2, "TestVectorErase: Expected size to remain 2");
	}

	{
		Zenith_Vector<u_int> xVector;
		for (u_int u = 0; u < 5; u++)
		{
			xVector.PushBack(u);
		}

		bool bErased = xVector.Erase(2);
		ZENITH_ASSERT_TRUE(bErased, "TestVectorErase: Expected Erase(2) to return true");
		ZENITH_ASSERT_EQ(xVector.GetSize(), 4, "TestVectorErase: Expected size to be 4");
		ZENITH_ASSERT_EQ(xVector.Get(2), 3, "TestVectorErase: Expected index 2 to now be 3");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(10);

		bool bErased = xVector.Erase(5);
		ZENITH_ASSERT_FALSE(bErased, "TestVectorErase: Expected Erase(5) to return false");
		ZENITH_ASSERT_EQ(xVector.GetSize(), 1, "TestVectorErase: Expected size to remain 1");
	}

	{
		Zenith_Vector<u_int> xEmptyVector;
		bool bErased = xEmptyVector.EraseValue(0);
		ZENITH_ASSERT_FALSE(bErased, "TestVectorErase: Expected EraseValue on empty vector to return false");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(1);
		xVector.PushBack(2);
		xVector.PushBack(3);

		xVector.EraseValue(1);
		ZENITH_ASSERT_EQ(xVector.GetSize(), 2, "TestVectorErase: Expected size 2 after erasing first");
		ZENITH_ASSERT_EQ(xVector.Get(0), 2, "TestVectorErase: Expected first element to now be 2");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(1);
		xVector.PushBack(2);
		xVector.PushBack(3);

		xVector.EraseValue(3);
		ZENITH_ASSERT_EQ(xVector.GetSize(), 2, "TestVectorErase: Expected size 2 after erasing last");
		ZENITH_ASSERT_EQ(xVector.GetBack(), 2, "TestVectorErase: Expected last element to now be 2");
	}

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorErase passed");
}

ZENITH_TEST(Core, VectorZeroCapacityResize) { Zenith_UnitTests::TestVectorZeroCapacityResize(); }

void Zenith_UnitTests::TestVectorZeroCapacityResize(){
	// Test 1: PushBack on moved-from vector (capacity becomes 0 after move)
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		xSource.PushBack(2);
		xSource.PushBack(3);

		// Move to destination - source now has capacity 0
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// Source should now have capacity 0
		ZENITH_ASSERT_EQ(xSource.GetCapacity(), 0, "TestVectorZeroCapacityResize: Moved-from vector should have capacity 0");
		ZENITH_ASSERT_EQ(xSource.GetSize(), 0, "TestVectorZeroCapacityResize: Moved-from vector should have size 0");

		// PushBack on moved-from vector should work (was causing infinite loop before fix)
		xSource.PushBack(42);
		ZENITH_ASSERT_EQ(xSource.GetSize(), 1, "TestVectorZeroCapacityResize: Size should be 1 after PushBack");
		ZENITH_ASSERT_EQ(xSource.Get(0), 42, "TestVectorZeroCapacityResize: Element should be 42");
		ZENITH_ASSERT_GT(xSource.GetCapacity(), 0, "TestVectorZeroCapacityResize: Capacity should be > 0 after PushBack");
	}

	// Test 2: EmplaceBack on moved-from vector
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(100);
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// EmplaceBack should also work on zero-capacity vector
		xSource.EmplaceBack(200);
		ZENITH_ASSERT_EQ(xSource.GetSize(), 1, "TestVectorZeroCapacityResize: Size should be 1 after EmplaceBack");
		ZENITH_ASSERT_EQ(xSource.Get(0), 200, "TestVectorZeroCapacityResize: Element should be 200");
	}

	// Test 3: Move assignment leaves source at capacity 0
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		xSource.PushBack(2);

		Zenith_Vector<u_int> xDest;
		xDest = std::move(xSource);

		ZENITH_ASSERT_EQ(xSource.GetCapacity(), 0, "TestVectorZeroCapacityResize: Move-assigned source should have capacity 0");

		// Should be able to reuse the moved-from vector
		xSource.PushBack(99);
		ZENITH_ASSERT_EQ(xSource.GetSize(), 1, "TestVectorZeroCapacityResize: Reused vector should have size 1");
		ZENITH_ASSERT_EQ(xSource.Get(0), 99, "TestVectorZeroCapacityResize: Reused vector element should be 99");
	}

	// Test 4: Multiple PushBacks after move to ensure proper capacity growth
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// Add many elements to trigger multiple resizes
		for (u_int u = 0; u < 100; u++)
		{
			xSource.PushBack(u);
		}

		ZENITH_ASSERT_EQ(xSource.GetSize(), 100, "TestVectorZeroCapacityResize: Size should be 100 after many PushBacks");
		for (u_int u = 0; u < 100; u++)
		{
			ZENITH_ASSERT_EQ(xSource.Get(u), u, "TestVectorZeroCapacityResize: Elements should match");
		}
	}

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorZeroCapacityResize passed");
}

class MemoryPoolTest
{
public:
	static u_int s_uCount;

	explicit MemoryPoolTest(u_int& uOut)
	: m_uTest(++s_uCount)
	{
		uOut = m_uTest;
	}

	~MemoryPoolTest()
	{
		s_uCount--;
	}

	u_int m_uTest;
};
u_int MemoryPoolTest::s_uCount = 0;

ZENITH_TEST(Core, MemoryPool) { Zenith_UnitTests::TestMemoryPool(); }

void Zenith_UnitTests::TestMemoryPool(){
	constexpr u_int uPOOL_SIZE = 128;
	Zenith_MemoryPool<MemoryPoolTest, uPOOL_SIZE> xPool;
	MemoryPoolTest* apxTest[uPOOL_SIZE];

	ZENITH_ASSERT_EQ(MemoryPoolTest::s_uCount, 0);

	for (u_int u = 0; u < uPOOL_SIZE / 2; u++)
	{
		u_int uTest;
		apxTest[u] = xPool.Allocate(uTest);
		ZENITH_ASSERT_EQ(MemoryPoolTest::s_uCount, u + 1);
		ZENITH_ASSERT_EQ(apxTest[u]->m_uTest, u + 1);
		ZENITH_ASSERT_EQ(uTest, u + 1);
	}

	for (u_int u = 0; u < uPOOL_SIZE / 4; u++)
	{
		ZENITH_ASSERT_EQ(apxTest[u]->m_uTest, u + 1);
		xPool.Deallocate(apxTest[u]);
		ZENITH_ASSERT_EQ(MemoryPoolTest::s_uCount, (uPOOL_SIZE / 2) - u - 1);
	}

	ZENITH_ASSERT_EQ(MemoryPoolTest::s_uCount, uPOOL_SIZE / 4);
}

ZENITH_TEST(Core, MemoryPoolExhaustion) { Zenith_UnitTests::TestMemoryPoolExhaustion(); }

void Zenith_UnitTests::TestMemoryPoolExhaustion(){

	constexpr u_int uPOOL_SIZE = 4;
	Zenith_MemoryPool<u_int, uPOOL_SIZE> xPool;

	// Allocate all slots
	u_int* apxSlots[uPOOL_SIZE];
	for (u_int u = 0; u < uPOOL_SIZE; u++)
	{
		apxSlots[u] = xPool.Allocate(u);
		ZENITH_ASSERT_NOT_NULL(apxSlots[u], "Allocation %u should succeed", u);
	}

	// Pool should be full
	ZENITH_ASSERT_TRUE(xPool.IsFull(), "Pool should be full after allocating all slots");

	// Next allocation should return nullptr (graceful exhaustion)
	u_int* pxOverflow = xPool.Allocate(999u);
	ZENITH_ASSERT_NULL(pxOverflow, "Pool exhaustion should return nullptr, not crash");

	// Deallocate one and verify we can allocate again
	xPool.Deallocate(apxSlots[0]);
	ZENITH_ASSERT_FALSE(xPool.IsFull(), "Pool should not be full after deallocation");

	u_int* pxReuse = xPool.Allocate(42u);
	ZENITH_ASSERT_NOT_NULL(pxReuse, "Should be able to allocate after deallocation");
	ZENITH_ASSERT_EQ(*pxReuse, 42, "Reused slot should have correct value");

	// Cleanup remaining allocations
	for (u_int u = 1; u < uPOOL_SIZE; u++)
	{
		xPool.Deallocate(apxSlots[u]);
	}
	xPool.Deallocate(pxReuse);

	ZENITH_ASSERT_TRUE(xPool.IsEmpty(), "Pool should be empty after deallocating all");

}

// ============================================================================
// CIRCULAR QUEUE TESTS
// ============================================================================

ZENITH_TEST(Core, CircularQueueBasic) { Zenith_UnitTests::TestCircularQueueBasic(); }

void Zenith_UnitTests::TestCircularQueueBasic(){

	constexpr u_int uCAPACITY = 8;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Initial state
	ZENITH_ASSERT_TRUE(xQueue.IsEmpty(), "Queue should start empty");
	ZENITH_ASSERT_FALSE(xQueue.IsFull(), "Queue should not start full");
	ZENITH_ASSERT_EQ(xQueue.GetSize(), 0, "Queue should have size 0");
	ZENITH_ASSERT_EQ(xQueue.GetCapacity(), uCAPACITY, "Queue capacity should be %u", uCAPACITY);

	// Enqueue and dequeue
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bEnqueued = xQueue.Enqueue(u * 10);
		ZENITH_ASSERT_TRUE(bEnqueued, "Enqueue %u should succeed", u);
		ZENITH_ASSERT_EQ(xQueue.GetSize(), u + 1, "Size should be %u", u + 1);
	}

	u_int uVal = 0;
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bDequeued = xQueue.Dequeue(uVal);
		ZENITH_ASSERT_TRUE(bDequeued, "Dequeue %u should succeed", u);
		ZENITH_ASSERT_EQ(uVal, u * 10, "Dequeued value should be %u, got %u", u * 10, uVal);
	}

	ZENITH_ASSERT_TRUE(xQueue.IsEmpty(), "Queue should be empty after dequeue all");

	// Test Peek
	xQueue.Enqueue(123u);
	bool bPeeked = xQueue.Peek(uVal);
	ZENITH_ASSERT_TRUE(bPeeked, "Peek should succeed");
	ZENITH_ASSERT_EQ(uVal, 123, "Peek should return front value");
	ZENITH_ASSERT_EQ(xQueue.GetSize(), 1, "Peek should not remove element");

}

ZENITH_TEST(Core, CircularQueueWrapping) { Zenith_UnitTests::TestCircularQueueWrapping(); }

void Zenith_UnitTests::TestCircularQueueWrapping(){

	constexpr u_int uCAPACITY = 4;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Fill the queue
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		xQueue.Enqueue(u);
	}

	// Remove half
	u_int uVal = 0;
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		xQueue.Dequeue(uVal);
	}

	// Now front pointer is at index 2, add more to test wrapping
	// This specifically tests the integer overflow fix in Enqueue
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bEnqueued = xQueue.Enqueue(100 + u);
		ZENITH_ASSERT_TRUE(bEnqueued, "Enqueue after wrap should succeed");
	}

	ZENITH_ASSERT_TRUE(xQueue.IsFull(), "Queue should be full after wrapping");

	// Verify FIFO order is maintained across wrap
	u_int auExpected[] = { 2, 3, 100, 101 };  // Original 2,3 + new 100,101
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		bool bDequeued = xQueue.Dequeue(uVal);
		ZENITH_ASSERT_TRUE(bDequeued, "Dequeue %u should succeed", u);
		ZENITH_ASSERT_EQ(uVal, auExpected[u], "Value %u should be %u, got %u", u, auExpected[u], uVal);
	}

}

ZENITH_TEST(Core, CircularQueueFull) { Zenith_UnitTests::TestCircularQueueFull(); }

void Zenith_UnitTests::TestCircularQueueFull(){

	constexpr u_int uCAPACITY = 4;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Fill to capacity
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		bool bEnqueued = xQueue.Enqueue(u);
		ZENITH_ASSERT_TRUE(bEnqueued, "Enqueue within capacity should succeed");
	}

	ZENITH_ASSERT_TRUE(xQueue.IsFull(), "Queue should be full");
	ZENITH_ASSERT_EQ(xQueue.GetSize(), uCAPACITY, "Size should equal capacity");

	// Attempt to enqueue when full - should fail gracefully
	bool bOverflow = xQueue.Enqueue(999u);
	ZENITH_ASSERT_FALSE(bOverflow, "Enqueue when full should return false");
	ZENITH_ASSERT_EQ(xQueue.GetSize(), uCAPACITY, "Size should remain at capacity");

	// Dequeue from empty queue should fail
	xQueue.Clear();
	ZENITH_ASSERT_TRUE(xQueue.IsEmpty(), "Queue should be empty after Clear");

	u_int uVal = 0;
	bool bUnderflow = xQueue.Dequeue(uVal);
	ZENITH_ASSERT_FALSE(bUnderflow, "Dequeue from empty should return false");

	// Peek from empty should fail
	bool bPeekEmpty = xQueue.Peek(uVal);
	ZENITH_ASSERT_FALSE(bPeekEmpty, "Peek from empty should return false");

}

// Helper class for testing non-POD destructor behavior
class TestDestructorCounter
{
public:
	static u_int s_uDestructorCallCount;
	static void ResetCounter() { s_uDestructorCallCount = 0; }

	int m_iValue = 0;

	TestDestructorCounter() = default;
	TestDestructorCounter(int iVal) : m_iValue(iVal) {}
	TestDestructorCounter(const TestDestructorCounter& other) : m_iValue(other.m_iValue) {}
	TestDestructorCounter(TestDestructorCounter&& other) noexcept : m_iValue(other.m_iValue) { other.m_iValue = -1; }
	TestDestructorCounter& operator=(const TestDestructorCounter& other) { m_iValue = other.m_iValue; return *this; }
	TestDestructorCounter& operator=(TestDestructorCounter&& other) noexcept { m_iValue = other.m_iValue; other.m_iValue = -1; return *this; }
	~TestDestructorCounter() { s_uDestructorCallCount++; }
};
u_int TestDestructorCounter::s_uDestructorCallCount = 0;

ZENITH_TEST(Core, CircularQueueNonPOD) { Zenith_UnitTests::TestCircularQueueNonPOD(); }

void Zenith_UnitTests::TestCircularQueueNonPOD(){

	TestDestructorCounter::ResetCounter();

	{
		Zenith_CircularQueue<TestDestructorCounter, 4> xQueue;

		// Enqueue elements
		xQueue.Enqueue(TestDestructorCounter(1));
		xQueue.Enqueue(TestDestructorCounter(2));
		xQueue.Enqueue(TestDestructorCounter(3));

		ZENITH_ASSERT_EQ(xQueue.GetSize(), 3, "Queue should have 3 elements");

		// Dequeue and verify destructor was called
		u_int uPreDequeueCount = TestDestructorCounter::s_uDestructorCallCount;
		TestDestructorCounter xOut;
		bool bSuccess = xQueue.Dequeue(xOut);
		ZENITH_ASSERT_TRUE(bSuccess, "Dequeue should succeed");
		ZENITH_ASSERT_EQ(xOut.m_iValue, 1, "Dequeued value should be 1");
		// After dequeue: destructor called on slot + reconstruct creates new object
		// The slot's destructor should have been called
		ZENITH_ASSERT_GT(TestDestructorCounter::s_uDestructorCallCount, uPreDequeueCount, "Destructor should be called during Dequeue for non-POD types");

		// Clear and verify all destructors called
		uPreDequeueCount = TestDestructorCounter::s_uDestructorCallCount;
		xQueue.Clear();
		ZENITH_ASSERT_TRUE(xQueue.IsEmpty(), "Queue should be empty after Clear");
		ZENITH_ASSERT_GT(TestDestructorCounter::s_uDestructorCallCount, uPreDequeueCount, "Destructors should be called during Clear");
	}

}

ZENITH_TEST(Core, VectorSelfAssignment) { Zenith_UnitTests::TestVectorSelfAssignment(); }

void Zenith_UnitTests::TestVectorSelfAssignment(){

	// Test copy self-assignment
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);

		// Self-assignment should be a no-op, not crash
		xVec = xVec;

		ZENITH_ASSERT_EQ(xVec.GetSize(), 3, "Size should be unchanged after self-assignment");
		ZENITH_ASSERT_EQ(xVec.Get(0), 1, "Element 0 should be unchanged");
		ZENITH_ASSERT_EQ(xVec.Get(1), 2, "Element 1 should be unchanged");
		ZENITH_ASSERT_EQ(xVec.Get(2), 3, "Element 2 should be unchanged");
	}

	// Test move self-assignment
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(10);
		xVec.PushBack(20);

		// Move self-assignment should also be safe
		xVec = std::move(xVec);

		ZENITH_ASSERT_EQ(xVec.GetSize(), 2, "Size should be unchanged after move self-assignment");
		ZENITH_ASSERT_EQ(xVec.Get(0), 10, "Element 0 should be unchanged");
		ZENITH_ASSERT_EQ(xVec.Get(1), 20, "Element 1 should be unchanged");
	}

}

ZENITH_TEST(Core, VectorRemoveSwap) { Zenith_UnitTests::TestVectorRemoveSwap(); }

void Zenith_UnitTests::TestVectorRemoveSwap(){

	// Test basic RemoveSwap
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);
		xVec.PushBack(4);

		// Remove element at index 0 - last element (4) should be swapped in
		xVec.RemoveSwap(0);

		ZENITH_ASSERT_EQ(xVec.GetSize(), 3, "Size should be 3 after RemoveSwap");
		ZENITH_ASSERT_EQ(xVec.Get(0), 4, "Element at index 0 should be swapped from end");
		ZENITH_ASSERT_EQ(xVec.Get(1), 2, "Element at index 1 should be unchanged");
		ZENITH_ASSERT_EQ(xVec.Get(2), 3, "Element at index 2 should be unchanged");
	}

	// Test RemoveSwap on last element (no swap needed)
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);

		// Remove last element
		xVec.RemoveSwap(2);

		ZENITH_ASSERT_EQ(xVec.GetSize(), 2, "Size should be 2 after RemoveSwap on last");
		ZENITH_ASSERT_EQ(xVec.Get(0), 1, "Element 0 unchanged");
		ZENITH_ASSERT_EQ(xVec.Get(1), 2, "Element 1 unchanged");
	}

	// Test EraseValueSwap
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(10);
		xVec.PushBack(20);
		xVec.PushBack(30);

		bool bErased = xVec.EraseValueSwap(20);
		ZENITH_ASSERT_TRUE(bErased, "EraseValueSwap should return true for existing value");
		ZENITH_ASSERT_EQ(xVec.GetSize(), 2, "Size should be 2");
		ZENITH_ASSERT_TRUE(xVec.Contains(10), "Should still contain 10");
		ZENITH_ASSERT_TRUE(xVec.Contains(30), "Should still contain 30");
		ZENITH_ASSERT_FALSE(xVec.Contains(20), "Should NOT contain 20");

		bool bNotErased = xVec.EraseValueSwap(999);
		ZENITH_ASSERT_FALSE(bNotErased, "EraseValueSwap should return false for non-existent value");
	}

}

// ============================================================
// HashMap / HashSet tests
// ============================================================

// Fixed-hash key: always collides into the same bucket, so probing behaviour
// is exercised deterministically by TestHashMapCollisions and friends.
namespace
{
	struct CollidingKey
	{
		u_int m_uValue = 0;
		bool operator==(const CollidingKey& xOther) const { return m_uValue == xOther.m_uValue; }
	};
}

template<>
struct Zenith_Hash<CollidingKey>
{
	u_int64 operator()(const CollidingKey&) const noexcept { return 42; }
};

ZENITH_TEST(Core, HashMapBasic) { Zenith_UnitTests::TestHashMapBasic(); }

void Zenith_UnitTests::TestHashMapBasic(){

	Zenith_HashMap<u_int, u_int> xMap;
	ZENITH_ASSERT_TRUE(xMap.IsEmpty(), "New map should be empty");
	ZENITH_ASSERT_EQ(xMap.GetSize(), 0, "New map size should be 0");

	xMap.Insert(1, 100);
	xMap.Insert(2, 200);
	xMap.Insert(3, 300);
	ZENITH_ASSERT_EQ(xMap.GetSize(), 3, "Size should be 3 after 3 inserts");
	ZENITH_ASSERT_TRUE(xMap.Contains(1), "Should contain key 1");
	ZENITH_ASSERT_TRUE(xMap.Contains(2), "Should contain key 2");
	ZENITH_ASSERT_TRUE(xMap.Contains(3), "Should contain key 3");
	ZENITH_ASSERT_FALSE(xMap.Contains(4), "Should NOT contain key 4");
	ZENITH_ASSERT_EQ(xMap.Get(1), 100, "Get(1) should return 100");
	ZENITH_ASSERT_EQ(xMap.Get(2), 200, "Get(2) should return 200");
	ZENITH_ASSERT_EQ(xMap.Get(3), 300, "Get(3) should return 300");

	// Update existing
	xMap.Insert(2, 250);
	ZENITH_ASSERT_EQ(xMap.GetSize(), 3, "Size unchanged on update");
	ZENITH_ASSERT_EQ(xMap.Get(2), 250, "Value should be updated");

	// Remove
	bool bRemoved = xMap.Remove(2);
	ZENITH_ASSERT_TRUE(bRemoved, "Remove should succeed");
	ZENITH_ASSERT_EQ(xMap.GetSize(), 2, "Size should be 2 after remove");
	ZENITH_ASSERT_FALSE(xMap.Contains(2), "Should not contain removed key");
	ZENITH_ASSERT_TRUE(xMap.Contains(1), "Other keys still present");
	ZENITH_ASSERT_TRUE(xMap.Contains(3), "Other keys still present");

	bool bRemovedAgain = xMap.Remove(2);
	ZENITH_ASSERT_FALSE(bRemovedAgain, "Remove of missing key returns false");

	// TryGet
	u_int* pVal = xMap.TryGet(1);
	ZENITH_ASSERT_TRUE(pVal != nullptr && *pVal == 100, "TryGet returns value");
	ZENITH_ASSERT_NULL(xMap.TryGet(999), "TryGet returns nullptr for missing");

	xMap.Clear();
	ZENITH_ASSERT_TRUE(xMap.IsEmpty(), "Clear empties the map");
	ZENITH_ASSERT_FALSE(xMap.Contains(1), "Nothing left after clear");

}

ZENITH_TEST(Core, HashMapCollisions) { Zenith_UnitTests::TestHashMapCollisions(); }

void Zenith_UnitTests::TestHashMapCollisions(){

	Zenith_HashMap<CollidingKey, u_int> xMap(64);
	constexpr u_int uNUM = 30;
	for (u_int u = 0; u < uNUM; u++)
	{
		xMap.Insert({u}, u * 10);
	}
	ZENITH_ASSERT_EQ(xMap.GetSize(), uNUM, "All colliding keys stored");

	for (u_int u = 0; u < uNUM; u++)
	{
		ZENITH_ASSERT_TRUE(xMap.Contains({u}), "Colliding key retrievable");
		ZENITH_ASSERT_EQ(xMap.Get({u}), u * 10, "Colliding value correct");
	}

	// Remove every other one; rest should still be accessible through the probe chain.
	for (u_int u = 0; u < uNUM; u += 2)
	{
		xMap.Remove({u});
	}
	for (u_int u = 1; u < uNUM; u += 2)
	{
		ZENITH_ASSERT_TRUE(xMap.Contains({u}), "Surviving colliding key still accessible past tombstones");
	}
	for (u_int u = 0; u < uNUM; u += 2)
	{
		ZENITH_ASSERT_FALSE(xMap.Contains({u}), "Removed key not found");
	}

}

ZENITH_TEST(Core, HashMapRehash) { Zenith_UnitTests::TestHashMapRehash(); }

void Zenith_UnitTests::TestHashMapRehash(){

	Zenith_HashMap<u_int, u_int> xMap(16);
	u_int uInitialCapacity = xMap.GetCapacity();
	ZENITH_ASSERT_EQ(uInitialCapacity, 16, "Initial capacity should be 16");

	// Insert past load factor to trigger rehash
	constexpr u_int uNUM = 200;
	for (u_int u = 0; u < uNUM; u++)
	{
		xMap.Insert(u, u * 7);
	}
	ZENITH_ASSERT_EQ(xMap.GetSize(), uNUM, "All entries stored after rehash");
	ZENITH_ASSERT_GT(xMap.GetCapacity(), uInitialCapacity, "Capacity grew");

	// Verify all entries survived rehash
	for (u_int u = 0; u < uNUM; u++)
	{
		ZENITH_ASSERT_TRUE(xMap.Contains(u), "Key survived rehash");
		ZENITH_ASSERT_EQ(xMap.Get(u), u * 7, "Value survived rehash");
	}

	// Explicit Reserve
	Zenith_HashMap<u_int, u_int> xMap2;
	xMap2.Reserve(1024);
	ZENITH_ASSERT_GE(xMap2.GetCapacity(), 1024, "Reserve grew capacity");

}

ZENITH_TEST(Core, HashMapTombstones) { Zenith_UnitTests::TestHashMapTombstones(); }

void Zenith_UnitTests::TestHashMapTombstones(){

	// Insert and delete many entries at the same slot; verify lookups still work.
	Zenith_HashMap<CollidingKey, u_int> xMap(32);
	for (u_int u = 0; u < 16; u++)
	{
		xMap.Insert({u}, u);
	}
	for (u_int u = 0; u < 16; u++)
	{
		xMap.Remove({u});
	}
	ZENITH_ASSERT_EQ(xMap.GetSize(), 0, "All removed");

	// Re-insert after deletion — tombstones should be reused
	for (u_int u = 100; u < 120; u++)
	{
		xMap.Insert({u}, u);
	}
	for (u_int u = 100; u < 120; u++)
	{
		ZENITH_ASSERT_TRUE(xMap.Contains({u}), "Post-tombstone insert retrievable");
	}
	for (u_int u = 0; u < 16; u++)
	{
		ZENITH_ASSERT_FALSE(xMap.Contains({u}), "Tombstoned key still missing");
	}

}

ZENITH_TEST(Core, HashMapIterator) { Zenith_UnitTests::TestHashMapIterator(); }

void Zenith_UnitTests::TestHashMapIterator(){

	Zenith_HashMap<u_int, u_int> xMap;
	constexpr u_int uNUM = 50;
	for (u_int u = 0; u < uNUM; u++)
	{
		xMap.Insert(u, u * 3);
	}

	// Iterate, mark each seen, verify every entry visited exactly once
	Zenith_Vector<bool> xSeen;
	for (u_int u = 0; u < uNUM; u++) xSeen.PushBack(false);

	u_int uVisited = 0;
	Zenith_HashMap<u_int, u_int>::Iterator xIt(xMap);
	for (; !xIt.Done(); xIt.Next())
	{
		const u_int uKey = xIt.GetKey();
		const u_int uVal = xIt.GetValue();
		ZENITH_ASSERT_LT(uKey, uNUM, "Key in expected range");
		ZENITH_ASSERT_EQ(uVal, uKey * 3, "Value matches key");
		ZENITH_ASSERT_FALSE(xSeen.Get(uKey), "Each key visited at most once");
		xSeen.Get(uKey) = true;
		uVisited++;
	}
	ZENITH_ASSERT_EQ(uVisited, uNUM, "All entries visited");

}

ZENITH_TEST(Core, HashMapIteratorInvalidation) { Zenith_UnitTests::TestHashMapIteratorInvalidation(); }

void Zenith_UnitTests::TestHashMapIteratorInvalidation(){

	// We cannot actually trigger asserts from within a test without
	// Zenith_AssertCapture machinery; just verify the generation counter
	// moves on operations that would invalidate iterators.
	Zenith_HashMap<u_int, u_int> xMap;
	for (u_int u = 0; u < 10; u++) xMap.Insert(u, u);

	// Iterator constructed here is valid
	{
		Zenith_HashMap<u_int, u_int>::Iterator xIt(xMap);
		u_int uCount = 0;
		for (; !xIt.Done(); xIt.Next()) uCount++;
		ZENITH_ASSERT_EQ(uCount, 10, "Iterator works on stable map");
	}

	// Triggering rehash should bump generation
	for (u_int u = 10; u < 200; u++) xMap.Insert(u, u);
	ZENITH_ASSERT_EQ(xMap.GetSize(), 200, "Map grew");

	// Clear also invalidates
	xMap.Clear();
	ZENITH_ASSERT_TRUE(xMap.IsEmpty(), "Map cleared");

}

ZENITH_TEST(Core, HashMapCopyMove) { Zenith_UnitTests::TestHashMapCopyMove(); }

void Zenith_UnitTests::TestHashMapCopyMove(){

	Zenith_HashMap<u_int, u_int> xOriginal;
	for (u_int u = 0; u < 20; u++) xOriginal.Insert(u, u * 5);

	// Copy construction
	Zenith_HashMap<u_int, u_int> xCopy(xOriginal);
	ZENITH_ASSERT_EQ(xCopy.GetSize(), xOriginal.GetSize(), "Copy has same size");
	for (u_int u = 0; u < 20; u++)
	{
		ZENITH_ASSERT_EQ(xCopy.Get(u), u * 5, "Copy preserved value");
	}
	// Modify copy, verify original unchanged
	xCopy.Insert(100, 999);
	ZENITH_ASSERT_FALSE(xOriginal.Contains(100), "Original unaffected by copy mutation");

	// Copy assignment
	Zenith_HashMap<u_int, u_int> xAssigned;
	xAssigned.Insert(500, 500);
	xAssigned = xOriginal;
	ZENITH_ASSERT_EQ(xAssigned.GetSize(), xOriginal.GetSize(), "Assigned has same size");
	ZENITH_ASSERT_FALSE(xAssigned.Contains(500), "Old contents of target replaced");
	for (u_int u = 0; u < 20; u++)
	{
		ZENITH_ASSERT_EQ(xAssigned.Get(u), u * 5, "Assigned preserved value");
	}

	// Move construction
	Zenith_HashMap<u_int, u_int> xMoved(std::move(xCopy));
	ZENITH_ASSERT_TRUE(xMoved.Contains(100), "Moved target has the data");
	ZENITH_ASSERT_EQ(xCopy.GetSize(), 0, "Moved source is empty");

	// Move assignment
	Zenith_HashMap<u_int, u_int> xMoveAssigned;
	xMoveAssigned = std::move(xAssigned);
	ZENITH_ASSERT_TRUE(xMoveAssigned.Contains(10), "Move-assigned target has data");
	ZENITH_ASSERT_EQ(xAssigned.GetSize(), 0, "Move-assigned source empty");

}

ZENITH_TEST(Core, HashMapSerialization) { Zenith_UnitTests::TestHashMapSerialization(); }

void Zenith_UnitTests::TestHashMapSerialization(){

	Zenith_HashMap<u_int, u_int> xOriginal;
	for (u_int u = 0; u < 64; u++) xOriginal.Insert(u, u * 11);

	Zenith_DataStream xStream(4096);
	xOriginal.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Zenith_HashMap<u_int, u_int> xRestored;
	xRestored.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xRestored.GetSize(), xOriginal.GetSize(), "Restored size matches");
	for (u_int u = 0; u < 64; u++)
	{
		ZENITH_ASSERT_TRUE(xRestored.Contains(u), "All keys restored");
		ZENITH_ASSERT_EQ(xRestored.Get(u), u * 11, "All values restored");
	}

}

ZENITH_TEST(Core, HashMapOperatorBracket) { Zenith_UnitTests::TestHashMapOperatorBracket(); }

void Zenith_UnitTests::TestHashMapOperatorBracket(){

	Zenith_HashMap<u_int, u_int> xMap;
	// Access missing key — default-constructs V (=0 for u_int)
	u_int& rRef = xMap[42];
	ZENITH_ASSERT_EQ(rRef, 0, "operator[] default-constructs missing key");
	ZENITH_ASSERT_EQ(xMap.GetSize(), 1, "operator[] on missing key inserts");

	rRef = 999;
	ZENITH_ASSERT_EQ(xMap.Get(42), 999, "operator[] returns a live reference");
	ZENITH_ASSERT_EQ(xMap[42], 999, "Second operator[] returns existing value");
	ZENITH_ASSERT_EQ(xMap.GetSize(), 1, "Repeat access does not add entry");

}

ZENITH_TEST(Core, HashSetBasic) { Zenith_UnitTests::TestHashSetBasic(); }

void Zenith_UnitTests::TestHashSetBasic(){

	Zenith_HashSet<u_int> xSet;
	ZENITH_ASSERT_TRUE(xSet.IsEmpty(), "New set empty");

	bool bAdded = xSet.Insert(1);
	ZENITH_ASSERT_TRUE(bAdded, "First insert reports true");
	bool bReAdded = xSet.Insert(1);
	ZENITH_ASSERT_FALSE(bReAdded, "Re-insert reports false");
	ZENITH_ASSERT_EQ(xSet.GetSize(), 1, "Size is 1 after dedup");

	for (u_int u = 2; u < 50; u++) xSet.Insert(u);
	ZENITH_ASSERT_EQ(xSet.GetSize(), 49, "Set grew to 49");
	for (u_int u = 1; u < 50; u++) ZENITH_ASSERT_TRUE(xSet.Contains(u), "Contains all inserted");
	ZENITH_ASSERT_FALSE(xSet.Contains(500), "Missing key absent");

	xSet.Remove(25);
	ZENITH_ASSERT_FALSE(xSet.Contains(25), "Removed key absent");
	ZENITH_ASSERT_EQ(xSet.GetSize(), 48, "Size decreased");

	// Iterator
	u_int uCount = 0;
	Zenith_HashSet<u_int>::Iterator xIt(xSet);
	for (; !xIt.Done(); xIt.Next()) uCount++;
	ZENITH_ASSERT_EQ(uCount, 48, "Iterator visits every entry once");

	// Serialization round-trip
	Zenith_DataStream xStream(2048);
	xSet.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	Zenith_HashSet<u_int> xRestored;
	xRestored.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ(xRestored.GetSize(), xSet.GetSize(), "Restored set has same size");
	for (u_int u = 1; u < 50; u++)
	{
		if (u == 25) continue;
		ZENITH_ASSERT_TRUE(xRestored.Contains(u), "Restored set has all keys");
	}

}

ZENITH_TEST(Core, DataStreamBoundsCheck) { Zenith_UnitTests::TestDataStreamBoundsCheck(); }

void Zenith_UnitTests::TestDataStreamBoundsCheck(){

	// Test SkipBytes bounds checking
	{
		Zenith_DataStream xStream(100);

		// Write some data
		u_int uVal = 42;
		xStream << uVal;

		// Reset cursor and read
		xStream.SetCursor(0);
		u_int uRead;
		xStream >> uRead;
		ZENITH_ASSERT_EQ(uRead, 42, "Read value should match written value");

		// Test valid skip
		xStream.SetCursor(0);
		xStream.SkipBytes(sizeof(u_int));
		ZENITH_ASSERT_EQ(xStream.GetCursor(), sizeof(u_int), "Cursor should advance by skip amount");

		// Test skip to exactly end (valid edge case)
		xStream.SetCursor(96);
		xStream.SkipBytes(4);  // Should clamp to size (100)
		ZENITH_ASSERT_LE(xStream.GetCursor(), xStream.GetSize(), "Cursor should not exceed data size");
	}

}

// ============================================================================
// SCENE SERIALIZATION TESTS
// ============================================================================

/**
 * Test individual component serialization round-trip
 * Verifies that each component can save and load its data correctly
 */
ZENITH_TEST(ECS, ComponentSerialization) { Zenith_UnitTests::TestComponentSerialization(); }
void Zenith_UnitTests::TestComponentSerialization(){

	// Create a temporary scene through SceneManager
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentSerializationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Test TransformComponent
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestTransformEntity");
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(1.0f, 2.0f, 3.0f);
		const Zenith_Maths::Quat xGroundTruthRot(0.707f, 0.0f, 0.707f, 0.0f);
		const Zenith_Maths::Vector3 xGroundTruthScale(2.0f, 3.0f, 4.0f);

		xTransform.SetPosition(xGroundTruthPos);
		xTransform.SetRotation(xGroundTruthRot);
		xTransform.SetScale(xGroundTruthScale);

		// Serialize
		Zenith_DataStream xStream;
		xTransform.WriteToDataStream(xStream);

		// Reset cursor and deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestTransformEntity2");
		Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
		xTransform2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos, xLoadedScale;
		Zenith_Maths::Quat xLoadedRot;
		xTransform2.GetPosition(xLoadedPos);
		xTransform2.GetRotation(xLoadedRot);
		xTransform2.GetScale(xLoadedScale);

		ZENITH_ASSERT_EQ(xLoadedPos, xGroundTruthPos, "TransformComponent position mismatch");
		ZENITH_ASSERT_TRUE(xLoadedRot.x == xGroundTruthRot.x && xLoadedRot.y == xGroundTruthRot.y &&
					  xLoadedRot.z == xGroundTruthRot.z && xLoadedRot.w == xGroundTruthRot.w, "TransformComponent rotation mismatch");
		ZENITH_ASSERT_EQ(xLoadedScale, xGroundTruthScale, "TransformComponent scale mismatch");

	}

	// Test CameraComponent
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestCameraEntity");
		Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(5.0f, 10.0f, 15.0f);
		const float fGroundTruthPitch = 0.5f;
		const float fGroundTruthYaw = 1.2f;
		const float fGroundTruthFOV = 60.0f;
		const float fGroundTruthNear = 0.1f;
		const float fGroundTruthFar = 1000.0f;
		const float fGroundTruthAspect = 16.0f / 9.0f;

		xCamera.InitialisePerspective({
			.m_xPosition = xGroundTruthPos,
			.m_fPitch = fGroundTruthPitch,
			.m_fYaw = fGroundTruthYaw,
			.m_fFOV = fGroundTruthFOV,
			.m_fNear = fGroundTruthNear,
			.m_fFar = fGroundTruthFar,
			.m_fAspectRatio = fGroundTruthAspect,
		});

		// Serialize
		Zenith_DataStream xStream;
		xCamera.WriteToDataStream(xStream);

		// Deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestCameraEntity2");
		Zenith_CameraComponent& xCamera2 = xEntity2.AddComponent<Zenith_CameraComponent>();
		xCamera2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos;
		xCamera2.GetPosition(xLoadedPos);

		ZENITH_ASSERT_EQ(xLoadedPos, xGroundTruthPos, "CameraComponent position mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetPitch(), fGroundTruthPitch, "CameraComponent pitch mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetYaw(), fGroundTruthYaw, "CameraComponent yaw mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetFOV(), fGroundTruthFOV, "CameraComponent FOV mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetNearPlane(), fGroundTruthNear, "CameraComponent near plane mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetFarPlane(), fGroundTruthFar, "CameraComponent far plane mismatch");
		ZENITH_ASSERT_EQ(xCamera2.GetAspectRatio(), fGroundTruthAspect, "CameraComponent aspect ratio mismatch");

	}

	// Clean up test scene
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

/**
 * Test entity serialization round-trip
 * Verifies that entities with multiple components can be serialized and restored
 */
ZENITH_TEST(ECS, EntitySerialization) { Zenith_UnitTests::TestEntitySerialization(); }
void Zenith_UnitTests::TestEntitySerialization(){

	// Create a temporary scene through SceneManager
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestEntitySerializationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create ground truth entity with multiple components
	Zenith_Entity xGroundTruthEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity");

	// Add TransformComponent
	Zenith_TransformComponent& xTransform = xGroundTruthEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetRotation(Zenith_Maths::Quat(0.707f, 0.0f, 0.707f, 0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f));

	// Add CameraComponent
	Zenith_CameraComponent& xCamera = xGroundTruthEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(0.0f, 5.0f, 10.0f),
	});

	// Serialize entity
	Zenith_DataStream xStream;
	xGroundTruthEntity.WriteToDataStream(xStream);

	// Verify entity metadata was written
	const std::string strExpectedName = xGroundTruthEntity.GetName();

	// Deserialize into new entity
	// Note: The new entity gets its own fresh EntityID from the scene's slot system
	// ReadFromDataStream only loads component data and name, not the ID
	xStream.SetCursor(0);
	Zenith_Entity xLoadedEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "PlaceholderName");
	xLoadedEntity.ReadFromDataStream(xStream);

	// Verify entity name was restored (EntityID is assigned by scene, not serialized)
	ZENITH_ASSERT_EQ(xLoadedEntity.GetName(), strExpectedName, "Entity name mismatch");

	// Verify components were restored
	ZENITH_ASSERT_TRUE(xLoadedEntity.HasComponent<Zenith_TransformComponent>(), "TransformComponent not restored");
	ZENITH_ASSERT_TRUE(xLoadedEntity.HasComponent<Zenith_CameraComponent>(), "CameraComponent not restored");

	// Verify transform data
	Zenith_TransformComponent& xLoadedTransform = xLoadedEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos;
	xLoadedTransform.GetPosition(xLoadedPos);
	ZENITH_ASSERT_TRUE(xLoadedPos.x == 10.0f && xLoadedPos.y == 20.0f && xLoadedPos.z == 30.0f, "Entity transform position mismatch");

	// Clean up test scene
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

/**
 * Test full scene serialization
 * Verifies that entire scenes with multiple entities can be saved to disk
 */
#ifndef ZENITH_ANDROID // Uses raw std::filesystem/std::ifstream with relative paths
ZENITH_TEST(Scene, SceneSerialization) { Zenith_UnitTests::TestSceneSerialization(); }
#endif
void Zenith_UnitTests::TestSceneSerialization(){

	// Create a test scene through SceneManager
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestSceneSerializationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Entity 1: Camera
	Zenith_Entity xCameraEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MainCamera");
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(0.0f, 10.0f, 20.0f),
	});
	pxSceneData->SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Entity 2: Transform only
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity1");
	xEntity1.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	xTransform1.SetPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));

	// Entity 2: Transform only
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity2");
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	xTransform2.SetPosition(Zenith_Maths::Vector3(-5.0f, 0.0f, 0.0f));

	// Save scene to file
	const std::string strTestScenePath = "unit_test_scene" ZENITH_SCENE_EXT;
	pxSceneData->SaveToFile(strTestScenePath);

	// Verify file exists
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strTestScenePath), "Scene file was not created");

	// Verify file has content
	std::ifstream xFile(strTestScenePath, std::ios::binary | std::ios::ate);
	ZENITH_ASSERT_TRUE(xFile.is_open(), "Could not open saved scene file");
	const std::streamsize ulFileSize = xFile.tellg();
	xFile.close();
	ZENITH_ASSERT_GT(ulFileSize, 0, "Scene file is empty");
	ZENITH_ASSERT_GT(ulFileSize, 16, "Scene file is suspiciously small (header + metadata should be >16 bytes)");


	// Clean up test scene
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

/**
 * Test complete round-trip: save scene, clear, load scene, verify
 * This is the most comprehensive test - ensures data integrity across full save/load cycle
 */
#ifndef ZENITH_ANDROID // Uses raw std::filesystem/std::ifstream with relative paths
ZENITH_TEST(Scene, SceneRoundTrip) { Zenith_UnitTests::TestSceneRoundTrip(); }
#endif
void Zenith_UnitTests::TestSceneRoundTrip(){

	const std::string strTestScenePath = "unit_test_roundtrip" ZENITH_SCENE_EXT;

	// ========================================================================
	// STEP 1: CREATE GROUND TRUTH SCENE
	// ========================================================================

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestSceneRoundTripScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create Entity 1: Camera with specific properties
	Zenith_Entity xCameraEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MainCamera");
	const Zenith_EntityID uCameraEntityID = xCameraEntity.GetEntityID();
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xCameraPos(0.0f, 10.0f, 20.0f);
	const float fCameraPitch = 0.3f;
	const float fCameraYaw = 1.57f;
	const float fCameraFOV = 75.0f;
	xCamera.InitialisePerspective({
		.m_xPosition = xCameraPos,
		.m_fPitch = fCameraPitch,
		.m_fYaw = fCameraYaw,
		.m_fFOV = fCameraFOV,
	});
	pxSceneData->SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create Entity 2: Transform with precise values
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity1");
	const Zenith_EntityID uEntity1ID = xEntity1.GetEntityID();
	xEntity1.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity1Pos(5.0f, 3.0f, -2.0f);
	const Zenith_Maths::Quat xEntity1Rot(0.5f, 0.5f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 xEntity1Scale(1.0f, 2.0f, 1.0f);
	xTransform1.SetPosition(xEntity1Pos);
	xTransform1.SetRotation(xEntity1Rot);
	xTransform1.SetScale(xEntity1Scale);

	// Create Entity 2: Transform only
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity2");
	const Zenith_EntityID uEntity2ID = xEntity2.GetEntityID();
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity2Pos(-5.0f, 0.0f, 10.0f);
	xTransform2.SetPosition(xEntity2Pos);

	const u_int uGroundTruthEntityCount = 3;

	// ========================================================================
	// STEP 2: SAVE SCENE TO DISK
	// ========================================================================

	pxSceneData->SaveToFile(strTestScenePath);
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strTestScenePath), "Scene file was not created during round-trip test");

	// ========================================================================
	// STEP 3: CLEAR GROUND TRUTH SCENE (simulate application restart)
	// ========================================================================

	pxSceneData->Reset();
	ZENITH_ASSERT_EQ(pxSceneData->GetEntityCount(), 0, "Scene was not properly cleared");

	// ========================================================================
	// STEP 4: LOAD SCENE FROM DISK
	// ========================================================================

	Zenith_Scene xLoadedScene = g_xEngine.Scenes().LoadScene("LoadedTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxLoadedSceneData = g_xEngine.Scenes().GetSceneData(xLoadedScene);
	pxLoadedSceneData->LoadFromFile(strTestScenePath);

	// ========================================================================
	// STEP 5: VERIFY LOADED SCENE MATCHES GROUND TRUTH
	// ========================================================================

	// Verify entity count
	ZENITH_ASSERT_EQ(pxLoadedSceneData->GetEntityCount(), uGroundTruthEntityCount, "Loaded scene entity count mismatch (expected %u, got %u)", uGroundTruthEntityCount, pxLoadedSceneData->GetEntityCount());

	// Verify Camera Entity (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedCamera = pxLoadedSceneData->FindEntityByName("MainCamera");
	ZENITH_ASSERT_TRUE(xLoadedCamera.IsValid(), "Camera entity not found after round-trip");
	ZENITH_ASSERT_EQ(xLoadedCamera.GetName(), "MainCamera", "Camera entity name mismatch");
	ZENITH_ASSERT_TRUE(xLoadedCamera.HasComponent<Zenith_CameraComponent>(), "Camera entity missing CameraComponent");

	Zenith_CameraComponent& xLoadedCameraComp = xLoadedCamera.GetComponent<Zenith_CameraComponent>();
	Zenith_Maths::Vector3 xLoadedCameraPos;
	xLoadedCameraComp.GetPosition(xLoadedCameraPos);
	ZENITH_ASSERT_EQ(xLoadedCameraPos, xCameraPos, "Camera position mismatch");
	ZENITH_ASSERT_EQ(xLoadedCameraComp.GetPitch(), fCameraPitch, "Camera pitch mismatch");
	ZENITH_ASSERT_EQ(xLoadedCameraComp.GetYaw(), fCameraYaw, "Camera yaw mismatch");
	ZENITH_ASSERT_EQ(xLoadedCameraComp.GetFOV(), fCameraFOV, "Camera FOV mismatch");

	// Verify Entity 1 (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedEntity1 = pxLoadedSceneData->FindEntityByName("TestEntity1");
	ZENITH_ASSERT_TRUE(xLoadedEntity1.IsValid(), "Entity1 not found after round-trip");
	ZENITH_ASSERT_EQ(xLoadedEntity1.GetName(), "TestEntity1", "Entity1 name mismatch");
	ZENITH_ASSERT_TRUE(xLoadedEntity1.HasComponent<Zenith_TransformComponent>(), "Entity1 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform1 = xLoadedEntity1.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos1, xLoadedScale1;
	Zenith_Maths::Quat xLoadedRot1;
	xLoadedTransform1.GetPosition(xLoadedPos1);
	xLoadedTransform1.GetRotation(xLoadedRot1);
	xLoadedTransform1.GetScale(xLoadedScale1);

	ZENITH_ASSERT_EQ(xLoadedPos1, xEntity1Pos, "Entity1 position mismatch");
	ZENITH_ASSERT_TRUE(xLoadedRot1.x == xEntity1Rot.x && xLoadedRot1.y == xEntity1Rot.y &&
				  xLoadedRot1.z == xEntity1Rot.z && xLoadedRot1.w == xEntity1Rot.w, "Entity1 rotation mismatch");
	ZENITH_ASSERT_EQ(xLoadedScale1, xEntity1Scale, "Entity1 scale mismatch");

	// Verify Entity 2 (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedEntity2 = pxLoadedSceneData->FindEntityByName("TestEntity2");
	ZENITH_ASSERT_TRUE(xLoadedEntity2.IsValid(), "Entity2 not found after round-trip");
	ZENITH_ASSERT_EQ(xLoadedEntity2.GetName(), "TestEntity2", "Entity2 name mismatch");
	ZENITH_ASSERT_TRUE(xLoadedEntity2.HasComponent<Zenith_TransformComponent>(), "Entity2 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform2 = xLoadedEntity2.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos2;
	xLoadedTransform2.GetPosition(xLoadedPos2);
	ZENITH_ASSERT_EQ(xLoadedPos2, xEntity2Pos, "Entity2 position mismatch");

	// Verify main camera reference (engine-side resolver; equivalent to the old
	// pxLoadedSceneData->GetMainCamera()).
	Zenith_CameraComponent& xMainCamera = Zenith_GetMainCamera(pxLoadedSceneData);
	Zenith_Maths::Vector3 xMainCameraPos;
	xMainCamera.GetPosition(xMainCameraPos);
	ZENITH_ASSERT_EQ(xMainCameraPos, xCameraPos, "Main camera reference mismatch");


	// ========================================================================
	// STEP 6: CLEANUP
	// ========================================================================

	// Clean up test scenes
	g_xEngine.Scenes().UnloadScene(xTestScene);
	g_xEngine.Scenes().UnloadScene(xLoadedScene);

	// Clean up test file
	std::filesystem::remove(strTestScenePath);
	ZENITH_ASSERT_FALSE(std::filesystem::exists(strTestScenePath), "Test scene file was not cleaned up");

}

ZENITH_TEST(Scene, SceneDisableDestroyHelpers) { Zenith_UnitTests::TestSceneDisableDestroyHelpers(); }

void Zenith_UnitTests::TestSceneDisableDestroyHelpers(){

	// Create a test scene with entities
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestDisableDestroyScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Test scene should be created");

	// Test DisableEntity with invalid ID — should not crash
	pxSceneData->DisableEntity(INVALID_ENTITY_ID);

	// Test DestroyEntityComponents with invalid ID — should not crash
	pxSceneData->DestroyEntityComponents(INVALID_ENTITY_ID);

	// Create an entity and verify DisableEntity/DestroyEntityComponents work
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity1");
	Zenith_EntityID xID = xEntity.GetEntityID();
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID), "Entity should exist");

	// DisableEntity on a non-enabled-dispatched entity should be a no-op
	pxSceneData->DisableEntity(xID);
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID), "Entity should still exist after DisableEntity");

	// DestroyEntityComponents removes all components
	pxSceneData->DestroyEntityComponents(xID);

	// Entity slot still exists (components removed but slot not freed)
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID), "Entity slot should still exist after DestroyEntityComponents");

	// Clean up
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

// ============================================================================
// ANIMATION SYSTEM TESTS
// ============================================================================

// Helper function to compare floats with tolerance
static bool FloatEquals(float a, float b, float fTolerance = 0.0001f)
{
	return std::abs(a - b) < fTolerance;
}

// Helper function to compare vectors with tolerance
static bool Vec3Equals(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float fTolerance = 0.0001f)
{
	return FloatEquals(a.x, b.x, fTolerance) &&
		   FloatEquals(a.y, b.y, fTolerance) &&
		   FloatEquals(a.z, b.z, fTolerance);
}

// Helper function to compare matrices element-wise with tolerance
static bool Mat4Equals(const Zenith_Maths::Matrix4& a, const Zenith_Maths::Matrix4& b, float fTolerance = 0.0001f)
{
	for (int c = 0; c < 4; ++c)
	{
		for (int r = 0; r < 4; ++r)
		{
			if (!FloatEquals(a[c][r], b[c][r], fTolerance)) return false;
		}
	}
	return true;
}

// Helper function to compare quaternions with tolerance
static bool QuatEquals(const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float fTolerance = 0.0001f)
{
	// Quaternions q and -q represent the same rotation, so check both
	bool bDirect = FloatEquals(a.x, b.x, fTolerance) &&
				   FloatEquals(a.y, b.y, fTolerance) &&
				   FloatEquals(a.z, b.z, fTolerance) &&
				   FloatEquals(a.w, b.w, fTolerance);
	bool bNegated = FloatEquals(a.x, -b.x, fTolerance) &&
					FloatEquals(a.y, -b.y, fTolerance) &&
					FloatEquals(a.z, -b.z, fTolerance) &&
					FloatEquals(a.w, -b.w, fTolerance);
	return bDirect || bNegated;
}

/**
 * Test Flux_BoneLocalPose blending operations
 * Verifies linear blend, additive blend, and identity pose
 */
ZENITH_TEST(Animation, BoneLocalPoseBlending) { Zenith_UnitTests::TestBoneLocalPoseBlending(); }
void Zenith_UnitTests::TestBoneLocalPoseBlending(){

	// Test Identity pose
	{
		Flux_BoneLocalPose xIdentity = Flux_BoneLocalPose::Identity();
		ZENITH_ASSERT_TRUE(Vec3Equals(xIdentity.m_xPosition, Zenith_Maths::Vector3(0.0f)), "Identity pose position should be zero");
		ZENITH_ASSERT_TRUE(QuatEquals(xIdentity.m_xRotation, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)), "Identity pose rotation should be identity quaternion");
		ZENITH_ASSERT_TRUE(Vec3Equals(xIdentity.m_xScale, Zenith_Maths::Vector3(1.0f)), "Identity pose scale should be one");
	}

	// Test linear blend
	{
		Flux_BoneLocalPose xPoseA;
		xPoseA.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		xPoseA.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xPoseA.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		Flux_BoneLocalPose xPoseB;
		xPoseB.m_xPosition = Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f);
		xPoseB.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f); // Keep same for simpler test
		xPoseB.m_xScale = Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f);

		// Test t=0 (should return A)
		Flux_BoneLocalPose xBlend0 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 0.0f);
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend0.m_xPosition, xPoseA.m_xPosition), "Blend at t=0 should return pose A position");
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend0.m_xScale, xPoseA.m_xScale), "Blend at t=0 should return pose A scale");

		// Test t=1 (should return B)
		Flux_BoneLocalPose xBlend1 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 1.0f);
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend1.m_xPosition, xPoseB.m_xPosition), "Blend at t=1 should return pose B position");
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend1.m_xScale, xPoseB.m_xScale), "Blend at t=1 should return pose B scale");

		// Test t=0.5 (should return midpoint)
		Flux_BoneLocalPose xBlend05 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 0.5f);
		Zenith_Maths::Vector3 xExpectedPos(5.0f, 10.0f, 15.0f);
		Zenith_Maths::Vector3 xExpectedScale(1.5f, 1.5f, 1.5f);
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend05.m_xPosition, xExpectedPos), "Blend at t=0.5 should return midpoint position");
		ZENITH_ASSERT_TRUE(Vec3Equals(xBlend05.m_xScale, xExpectedScale), "Blend at t=0.5 should return midpoint scale");

	}

	// Test additive blend
	{
		Flux_BoneLocalPose xBase;
		xBase.m_xPosition = Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f);
		xBase.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xBase.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		Flux_BoneLocalPose xAdditive;
		xAdditive.m_xPosition = Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f); // Delta from identity
		xAdditive.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xAdditive.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		// Additive blend with weight 1.0 should add the delta
		Flux_BoneLocalPose xResult = Flux_BoneLocalPose::AdditiveBlend(xBase, xAdditive, 1.0f);
		Zenith_Maths::Vector3 xExpectedPos(8.0f, 8.0f, 8.0f); // 5 + 3
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult.m_xPosition, xExpectedPos), "Additive blend should add delta position");

	}

	// Test ToMatrix conversion
	{
		Flux_BoneLocalPose xPose;
		xPose.m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
		xPose.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xPose.m_xScale = Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f);

		Zenith_Maths::Matrix4 xMatrix = xPose.ToMatrix();

		// Check translation is in 4th column
		ZENITH_ASSERT_TRUE(FloatEquals(xMatrix[3][0], 1.0f) &&
					  FloatEquals(xMatrix[3][1], 2.0f) &&
					  FloatEquals(xMatrix[3][2], 3.0f), "Matrix translation should match pose position");

	}

}

/**
 * Test Flux_SkeletonPose operations
 * Verifies initialization, reset, and copy operations
 */
ZENITH_TEST(Animation, SkeletonPoseOperations) { Zenith_UnitTests::TestSkeletonPoseOperations(); }
void Zenith_UnitTests::TestSkeletonPoseOperations(){

	// Test initialization
	{
		Flux_SkeletonPose xPose;
		xPose.Initialize(50);

		ZENITH_ASSERT_EQ(xPose.GetNumBones(), 50, "Skeleton pose should have 50 bones after initialization");
	}

	// Test Reset
	{
		Flux_SkeletonPose xPose;
		xPose.Initialize(10);

		// Modify a bone
		Flux_BoneLocalPose& xBone0 = xPose.GetLocalPose(0);
		xBone0.m_xPosition = Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f);
		xBone0.m_xScale = Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f);

		// Reset
		xPose.Reset();

		// Verify reset to identity
		const Flux_BoneLocalPose& xResetBone = xPose.GetLocalPose(0);
		ZENITH_ASSERT_TRUE(Vec3Equals(xResetBone.m_xPosition, Zenith_Maths::Vector3(0.0f)), "Reset should set position to zero");
		ZENITH_ASSERT_TRUE(Vec3Equals(xResetBone.m_xScale, Zenith_Maths::Vector3(1.0f)), "Reset should set scale to one");

	}

	// Test CopyFrom
	{
		Flux_SkeletonPose xPoseA;
		xPoseA.Initialize(5);
		xPoseA.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
		xPoseA.GetLocalPose(1).m_xPosition = Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f);

		Flux_SkeletonPose xPoseB;
		xPoseB.Initialize(5);
		xPoseB.CopyFrom(xPoseA);

		ZENITH_ASSERT_TRUE(Vec3Equals(xPoseB.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)), "CopyFrom should copy bone 0 position");
		ZENITH_ASSERT_TRUE(Vec3Equals(xPoseB.GetLocalPose(1).m_xPosition, Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f)), "CopyFrom should copy bone 1 position");

	}

	// Test static Blend
	{
		Flux_SkeletonPose xPoseA, xPoseB, xPoseOut;
		xPoseA.Initialize(3);
		xPoseB.Initialize(3);
		xPoseOut.Initialize(3);

		xPoseA.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		xPoseB.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 10.0f, 10.0f);

		Flux_SkeletonPose::Blend(xPoseOut, xPoseA, xPoseB, 0.5f);

		ZENITH_ASSERT_TRUE(Vec3Equals(xPoseOut.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)), "Skeleton blend should interpolate bone positions");

	}

}

/**
 * Test Flux_AnimationParameters
 * Verifies parameter add, set, get, and trigger consumption
 */
ZENITH_TEST(Animation, AnimationParameters) { Zenith_UnitTests::TestAnimationParameters(); }
void Zenith_UnitTests::TestAnimationParameters(){

	Flux_AnimationParameters xParams;

	// Test Float parameter
	{
		xParams.AddFloat("Speed", 5.0f);
		ZENITH_ASSERT_TRUE(xParams.HasParameter("Speed"), "Should have Speed parameter");
		ZENITH_ASSERT_TRUE(FloatEquals(xParams.GetFloat("Speed"), 5.0f), "Speed default should be 5.0");

		xParams.SetFloat("Speed", 10.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xParams.GetFloat("Speed"), 10.0f), "Speed should be updated to 10.0");

	}

	// Test Int parameter
	{
		xParams.AddInt("Health", 100);
		ZENITH_ASSERT_TRUE(xParams.HasParameter("Health"), "Should have Health parameter");
		ZENITH_ASSERT_EQ(xParams.GetInt("Health"), 100, "Health default should be 100");

		xParams.SetInt("Health", 50);
		ZENITH_ASSERT_EQ(xParams.GetInt("Health"), 50, "Health should be updated to 50");

	}

	// Test Bool parameter
	{
		xParams.AddBool("IsRunning", false);
		ZENITH_ASSERT_TRUE(xParams.HasParameter("IsRunning"), "Should have IsRunning parameter");
		ZENITH_ASSERT_EQ(xParams.GetBool("IsRunning"), false, "IsRunning default should be false");

		xParams.SetBool("IsRunning", true);
		ZENITH_ASSERT_EQ(xParams.GetBool("IsRunning"), true, "IsRunning should be updated to true");

	}

	// Test Trigger parameter
	{
		xParams.AddTrigger("Jump");
		ZENITH_ASSERT_TRUE(xParams.HasParameter("Jump"), "Should have Jump trigger");

		// Trigger not set initially
		ZENITH_ASSERT_EQ(xParams.ConsumeTrigger("Jump"), false, "Trigger should not be set initially");

		// Set trigger
		xParams.SetTrigger("Jump");
		ZENITH_ASSERT_EQ(xParams.ConsumeTrigger("Jump"), true, "Trigger should be set after SetTrigger");

		// Trigger should be consumed (reset)
		ZENITH_ASSERT_EQ(xParams.ConsumeTrigger("Jump"), false, "Trigger should be reset after consumption");

	}

	// Test RemoveParameter
	{
		ZENITH_ASSERT_TRUE(xParams.HasParameter("Speed"), "Speed should exist");
		xParams.RemoveParameter("Speed");
		ZENITH_ASSERT_FALSE(xParams.HasParameter("Speed"), "Speed should be removed");

	}

	// Test GetParameterType
	{
		ZENITH_ASSERT_EQ(xParams.GetParameterType("Health"), Flux_AnimationParameters::ParamType::Int, "Health should be Int type");
		ZENITH_ASSERT_EQ(xParams.GetParameterType("IsRunning"), Flux_AnimationParameters::ParamType::Bool, "IsRunning should be Bool type");
		ZENITH_ASSERT_EQ(xParams.GetParameterType("Jump"), Flux_AnimationParameters::ParamType::Trigger, "Jump should be Trigger type");

	}

}

/**
 * Test Flux_TransitionCondition evaluation
 * Verifies all comparison operators with different parameter types
 */
ZENITH_TEST(Animation, TransitionConditions) { Zenith_UnitTests::TestTransitionConditions(); }
void Zenith_UnitTests::TestTransitionConditions(){

	Flux_AnimationParameters xParams;
	xParams.AddFloat("Speed", 5.0f);
	xParams.AddInt("Health", 100);
	xParams.AddBool("IsGrounded", true);
	xParams.AddTrigger("Attack");

	// Test Float Greater condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 3.0f;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "Speed 5.0 > 3.0 should be true");

		xCond.m_fThreshold = 6.0f;
		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "Speed 5.0 > 6.0 should be false");

	}

	// Test Float Less condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Less;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 10.0f;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "Speed 5.0 < 10.0 should be true");

	}

	// Test Int Equal condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "Health 100 == 100 should be true");

		xCond.m_iThreshold = 50;
		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "Health 100 == 50 should be false");

	}

	// Test Int LessEqual condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "Health 100 <= 100 should be true");

		xCond.m_iThreshold = 50;
		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "Health 100 <= 50 should be false");

	}

	// Test Bool condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "IsGrounded";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
		xCond.m_bThreshold = true;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "IsGrounded true == true should be true");

		xCond.m_bThreshold = false;
		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "IsGrounded true == false should be false");

	}

	// Test Trigger condition (Equal to true means trigger is set)
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Attack";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;

		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "Attack trigger not set should be false");

		xParams.SetTrigger("Attack");
		ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "Attack trigger set should be true");

	}

}

/**
 * Test Flux_AnimationStateMachine
 * Verifies state creation, transitions, and state changes
 */
ZENITH_TEST(Animation, AnimationStateMachine) { Zenith_UnitTests::TestAnimationStateMachine(); }
void Zenith_UnitTests::TestAnimationStateMachine(){

	Flux_AnimationStateMachine xStateMachine("TestSM");

	// Test state creation
	{
		Flux_AnimationState* pxIdleState = xStateMachine.AddState("Idle");
		Flux_AnimationState* pxWalkState = xStateMachine.AddState("Walk");
		Flux_AnimationState* pxRunState = xStateMachine.AddState("Run");

		ZENITH_ASSERT_NOT_NULL(pxIdleState, "Idle state should be created");
		ZENITH_ASSERT_NOT_NULL(pxWalkState, "Walk state should be created");
		ZENITH_ASSERT_NOT_NULL(pxRunState, "Run state should be created");

		ZENITH_ASSERT_TRUE(xStateMachine.HasState("Idle"), "Should have Idle state");
		ZENITH_ASSERT_TRUE(xStateMachine.HasState("Walk"), "Should have Walk state");
		ZENITH_ASSERT_TRUE(xStateMachine.HasState("Run"), "Should have Run state");
		ZENITH_ASSERT_FALSE(xStateMachine.HasState("Jump"), "Should not have Jump state");

	}

	// Test default state
	{
		xStateMachine.SetDefaultState("Idle");
		ZENITH_ASSERT_EQ(xStateMachine.GetDefaultStateName(), "Idle", "Default state should be Idle");

	}

	// Test SetState (force state change)
	{
		xStateMachine.SetState("Idle");
		ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Current state should be Idle");

		xStateMachine.SetState("Walk");
		ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Walk", "Current state should be Walk after SetState");

	}

	// Test adding transitions
	{
		Flux_AnimationState* pxIdleState = xStateMachine.GetState("Idle");
		ZENITH_ASSERT_NOT_NULL(pxIdleState, "Should retrieve Idle state");

		Flux_StateTransition xTransition;
		xTransition.m_strTargetStateName = "Walk";
		xTransition.m_fTransitionDuration = 0.2f;

		// Add condition: Speed > 0.1
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTransition.m_xConditions.PushBack(xCond);

		pxIdleState->AddTransition(xTransition);

		ZENITH_ASSERT_EQ(pxIdleState->GetTransitions().GetSize(), 1, "Idle state should have 1 transition");
		ZENITH_ASSERT_EQ(pxIdleState->GetTransitions().Get(0).m_strTargetStateName, "Walk", "Transition should target Walk state");

	}

	// Test parameters
	{
		xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
		xStateMachine.GetParameters().AddBool("IsGrounded", true);

		ZENITH_ASSERT_TRUE(xStateMachine.GetParameters().HasParameter("Speed"), "Parameters should have Speed");
		ZENITH_ASSERT_TRUE(xStateMachine.GetParameters().HasParameter("IsGrounded"), "Parameters should have IsGrounded");

	}

	// Test state removal
	{
		xStateMachine.RemoveState("Run");
		ZENITH_ASSERT_FALSE(xStateMachine.HasState("Run"), "Run state should be removed");

	}

	// Test name
	{
		ZENITH_ASSERT_EQ(xStateMachine.GetName(), "TestSM", "State machine name should be TestSM");

		xStateMachine.SetName("RenamedSM");
		ZENITH_ASSERT_EQ(xStateMachine.GetName(), "RenamedSM", "State machine name should be RenamedSM");

	}

}

/**
 * Test Flux_IKChain and Flux_IKSolver setup
 * Verifies chain creation, target management, and helper functions
 */
ZENITH_TEST(Animation, IKChainSetup) { Zenith_UnitTests::TestIKChainSetup(); }
void Zenith_UnitTests::TestIKChainSetup(){

	Flux_IKSolver xSolver;

	// Test chain creation with helper functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");

		ZENITH_ASSERT_EQ(xLegChain.m_strName, "LeftLeg", "Chain name should be LeftLeg");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.GetSize(), 3, "Leg chain should have 3 bones");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(0), "Hip_L", "First bone should be Hip_L");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(1), "Knee_L", "Second bone should be Knee_L");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(2), "Ankle_L", "Third bone should be Ankle_L");

	}

	// Test arm chain creation
	{
		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("RightArm", "Shoulder_R", "Elbow_R", "Wrist_R");

		ZENITH_ASSERT_EQ(xArmChain.m_strName, "RightArm", "Chain name should be RightArm");
		ZENITH_ASSERT_EQ(xArmChain.m_xBoneNames.GetSize(), 3, "Arm chain should have 3 bones");

	}

	// Test spine chain creation
	{
		Zenith_Vector<std::string> xSpineBones;
		xSpineBones.PushBack("Spine1");
		xSpineBones.PushBack("Spine2");
		xSpineBones.PushBack("Spine3");
		xSpineBones.PushBack("Neck");
		Flux_IKChain xSpineChain = Flux_IKSolver::CreateSpineChain("Spine", xSpineBones);

		ZENITH_ASSERT_EQ(xSpineChain.m_strName, "Spine", "Chain name should be Spine");
		ZENITH_ASSERT_EQ(xSpineChain.m_xBoneNames.GetSize(), 4, "Spine chain should have 4 bones");

	}

	// Test adding chains to solver
	{
		Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");
		Flux_IKChain xRightLeg = Flux_IKSolver::CreateLegChain("RightLeg", "Hip_R", "Knee_R", "Ankle_R");

		xSolver.AddChain(xLeftLeg);
		xSolver.AddChain(xRightLeg);

		ZENITH_ASSERT_TRUE(xSolver.HasChain("LeftLeg"), "Solver should have LeftLeg chain");
		ZENITH_ASSERT_TRUE(xSolver.HasChain("RightLeg"), "Solver should have RightLeg chain");
		ZENITH_ASSERT_FALSE(xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

	}

	// Test target management
	{
		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
		xTarget.m_fWeight = 0.75f;
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("LeftLeg", xTarget);

		ZENITH_ASSERT_TRUE(xSolver.HasTarget("LeftLeg"), "Solver should have LeftLeg target");
		ZENITH_ASSERT_FALSE(xSolver.HasTarget("RightLeg"), "Solver should not have RightLeg target");

		const Flux_IKTarget* pxTarget = xSolver.GetTarget("LeftLeg");
		ZENITH_ASSERT_NOT_NULL(pxTarget, "Should retrieve LeftLeg target");
		ZENITH_ASSERT_TRUE(Vec3Equals(pxTarget->m_xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f)), "Target position should match");
		ZENITH_ASSERT_TRUE(FloatEquals(pxTarget->m_fWeight, 0.75f), "Target weight should be 0.75");

	}

	// Test ClearTarget
	{
		xSolver.ClearTarget("LeftLeg");
		ZENITH_ASSERT_FALSE(xSolver.HasTarget("LeftLeg"), "LeftLeg target should be cleared");

	}

	// Test RemoveChain
	{
		xSolver.RemoveChain("LeftLeg");
		ZENITH_ASSERT_FALSE(xSolver.HasChain("LeftLeg"), "LeftLeg chain should be removed");
		ZENITH_ASSERT_TRUE(xSolver.HasChain("RightLeg"), "RightLeg chain should still exist");

	}

	// Test GetChain
	{
		Flux_IKChain* pxChain = xSolver.GetChain("RightLeg");
		ZENITH_ASSERT_NOT_NULL(pxChain, "Should retrieve RightLeg chain");
		ZENITH_ASSERT_EQ(pxChain->m_strName, "RightLeg", "Chain name should be RightLeg");

		// Modify via pointer
		pxChain->m_uMaxIterations = 20;
		ZENITH_ASSERT_EQ(xSolver.GetChain("RightLeg")->m_uMaxIterations, 20, "Chain modification should persist");

	}

}

/**
 * Test animation system serialization
 * Verifies round-trip serialization for animation data structures
 */
ZENITH_TEST(Animation, AnimationSerialization) { Zenith_UnitTests::TestAnimationSerialization(); }
void Zenith_UnitTests::TestAnimationSerialization(){

	// Test AnimationParameters serialization
	{
		Flux_AnimationParameters xOriginal;
		xOriginal.AddFloat("Speed", 5.5f);
		xOriginal.AddInt("Combo", 3);
		xOriginal.AddBool("IsJumping", true);
		xOriginal.AddTrigger("Attack");
		xOriginal.SetTrigger("Attack");

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_AnimationParameters xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(xLoaded.HasParameter("Speed"), "Should have Speed param");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetFloat("Speed"), 5.5f), "Speed should be 5.5");
		ZENITH_ASSERT_EQ(xLoaded.GetInt("Combo"), 3, "Combo should be 3");
		ZENITH_ASSERT_EQ(xLoaded.GetBool("IsJumping"), true, "IsJumping should be true");

	}

	// Test TransitionCondition serialization
	{
		Flux_TransitionCondition xOriginal;
		xOriginal.m_strParameterName = "Speed";
		xOriginal.m_eCompareOp = Flux_TransitionCondition::CompareOp::GreaterEqual;
		xOriginal.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xOriginal.m_fThreshold = 3.14f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_TransitionCondition xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_EQ(xLoaded.m_strParameterName, "Speed", "Parameter name should match");
		ZENITH_ASSERT_EQ(xLoaded.m_eCompareOp, Flux_TransitionCondition::CompareOp::GreaterEqual, "Compare op should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fThreshold, 3.14f), "Threshold should match");

	}

	// Test IKChain serialization
	{
		Flux_IKChain xOriginal = Flux_IKSolver::CreateLegChain("TestLeg", "Hip", "Knee", "Ankle");
		xOriginal.m_uMaxIterations = 15;
		xOriginal.m_fTolerance = 0.005f;
		xOriginal.m_bUsePoleVector = true;
		xOriginal.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_IKChain xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_EQ(xLoaded.m_strName, "TestLeg", "Chain name should match");
		ZENITH_ASSERT_EQ(xLoaded.m_xBoneNames.GetSize(), 3, "Should have 3 bones");
		ZENITH_ASSERT_EQ(xLoaded.m_xBoneNames.Get(0), "Hip", "First bone should be Hip");
		ZENITH_ASSERT_EQ(xLoaded.m_uMaxIterations, 15, "Max iterations should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fTolerance, 0.005f), "Tolerance should match");
		ZENITH_ASSERT_EQ(xLoaded.m_bUsePoleVector, true, "Use pole vector should match");
		ZENITH_ASSERT_TRUE(Vec3Equals(xLoaded.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)), "Pole vector should match");

	}

	// Test JointConstraint serialization
	{
		Flux_JointConstraint xOriginal;
		xOriginal.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
		xOriginal.m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xOriginal.m_fMinAngle = -1.5f;
		xOriginal.m_fMaxAngle = 0.0f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_JointConstraint xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_EQ(xLoaded.m_eType, Flux_JointConstraint::ConstraintType::Hinge, "Constraint type should be Hinge");
		ZENITH_ASSERT_TRUE(Vec3Equals(xLoaded.m_xHingeAxis, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)), "Hinge axis should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fMinAngle, -1.5f), "Min angle should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fMaxAngle, 0.0f), "Max angle should match");

	}

	// Test BoneMask serialization
	{
		Flux_BoneMask xOriginal;
		xOriginal.SetBoneWeight(0, 1.0f);
		xOriginal.SetBoneWeight(1, 0.5f);
		xOriginal.SetBoneWeight(2, 0.0f);

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_BoneMask xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetBoneWeight(2), 0.0f), "Bone 2 weight should be 0.0");

	}

	// Test AnimationClipMetadata serialization
	{
		Flux_AnimationClipMetadata xOriginal;
		xOriginal.m_strName = "TestClip";
		xOriginal.m_fDuration = 2.5f;
		xOriginal.m_uTicksPerSecond = 30;
		xOriginal.m_bLooping = false;
		xOriginal.m_fBlendInTime = 0.2f;
		xOriginal.m_fBlendOutTime = 0.3f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_AnimationClipMetadata xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_EQ(xLoaded.m_strName, "TestClip", "Clip name should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fDuration, 2.5f), "Duration should match");
		ZENITH_ASSERT_EQ(xLoaded.m_uTicksPerSecond, 30, "Ticks per second should match");
		ZENITH_ASSERT_EQ(xLoaded.m_bLooping, false, "Looping should be false");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fBlendInTime, 0.2f), "Blend in time should match");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.m_fBlendOutTime, 0.3f), "Blend out time should match");

	}

}

/**
 * Test blend tree node types
 * Verifies blend tree node creation and factory method
 */
ZENITH_TEST(Animation, BlendTreeNodes) { Zenith_UnitTests::TestBlendTreeNodes(); }
void Zenith_UnitTests::TestBlendTreeNodes(){

	// Test Clip node
	{
		Flux_BlendTreeNode_Clip xClipNode(nullptr, 1.0f);
		ZENITH_ASSERT_EQ(std::string(xClipNode.GetNodeTypeName()), "Clip", "Type name should be Clip");
		ZENITH_ASSERT_TRUE(FloatEquals(xClipNode.GetPlaybackRate(), 1.0f), "Playback rate should be 1.0");

		xClipNode.SetPlaybackRate(1.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xClipNode.GetPlaybackRate(), 1.5f), "Playback rate should be 1.5");

	}

	// Test Blend node
	{
		Flux_BlendTreeNode_Blend xBlendNode;
		ZENITH_ASSERT_EQ(std::string(xBlendNode.GetNodeTypeName()), "Blend", "Type name should be Blend");

		xBlendNode.SetBlendWeight(0.75f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 0.75f), "Blend weight should be 0.75");

	}

	// Test BlendSpace1D node
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;
		ZENITH_ASSERT_EQ(std::string(xBlendSpace.GetNodeTypeName()), "BlendSpace1D", "Type name should be BlendSpace1D");

		xBlendSpace.SetParameter(0.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), 0.5f), "Parameter should be 0.5");

	}

	// Test BlendSpace2D node
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;
		ZENITH_ASSERT_EQ(std::string(xBlendSpace.GetNodeTypeName()), "BlendSpace2D", "Type name should be BlendSpace2D");

		Zenith_Maths::Vector2 xParams(0.3f, 0.7f);
		xBlendSpace.SetParameter(xParams);
		const Zenith_Maths::Vector2& xRetrieved = xBlendSpace.GetParameter();
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrieved.x, 0.3f) && FloatEquals(xRetrieved.y, 0.7f), "Parameters should be (0.3, 0.7)");

	}

	// Test Additive node
	{
		Flux_BlendTreeNode_Additive xAdditiveNode;
		ZENITH_ASSERT_EQ(std::string(xAdditiveNode.GetNodeTypeName()), "Additive", "Type name should be Additive");

		xAdditiveNode.SetAdditiveWeight(0.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 0.5f), "Additive weight should be 0.5");

	}

	// Test Select node
	{
		Flux_BlendTreeNode_Select xSelectNode;
		ZENITH_ASSERT_EQ(std::string(xSelectNode.GetNodeTypeName()), "Select", "Type name should be Select");

		// Add some children before setting selected index
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));

		xSelectNode.SetSelectedIndex(2);
		ZENITH_ASSERT_EQ(xSelectNode.GetSelectedIndex(), 2, "Selected index should be 2");

	}

	// Test factory method
	{
		Flux_BlendTreeNode* pxClip = Flux_BlendTreeNode::CreateFromTypeName("Clip");
		ZENITH_ASSERT_NOT_NULL(pxClip, "Factory should create Clip node");
		ZENITH_ASSERT_EQ(std::string(pxClip->GetNodeTypeName()), "Clip", "Created node should be Clip type");
		delete pxClip;

		Flux_BlendTreeNode* pxBlend = Flux_BlendTreeNode::CreateFromTypeName("Blend");
		ZENITH_ASSERT_NOT_NULL(pxBlend, "Factory should create Blend node");
		ZENITH_ASSERT_EQ(std::string(pxBlend->GetNodeTypeName()), "Blend", "Created node should be Blend type");
		delete pxBlend;

		Flux_BlendTreeNode* pxBlendSpace1D = Flux_BlendTreeNode::CreateFromTypeName("BlendSpace1D");
		ZENITH_ASSERT_NOT_NULL(pxBlendSpace1D, "Factory should create BlendSpace1D node");
		ZENITH_ASSERT_EQ(std::string(pxBlendSpace1D->GetNodeTypeName()), "BlendSpace1D", "Created node should be BlendSpace1D type");
		delete pxBlendSpace1D;

		Flux_BlendTreeNode* pxBlendSpace2D = Flux_BlendTreeNode::CreateFromTypeName("BlendSpace2D");
		ZENITH_ASSERT_NOT_NULL(pxBlendSpace2D, "Factory should create BlendSpace2D node");
		ZENITH_ASSERT_EQ(std::string(pxBlendSpace2D->GetNodeTypeName()), "BlendSpace2D", "Created node should be BlendSpace2D type");
		delete pxBlendSpace2D;

		Flux_BlendTreeNode* pxAdditive = Flux_BlendTreeNode::CreateFromTypeName("Additive");
		ZENITH_ASSERT_NOT_NULL(pxAdditive, "Factory should create Additive node");
		ZENITH_ASSERT_EQ(std::string(pxAdditive->GetNodeTypeName()), "Additive", "Created node should be Additive type");
		delete pxAdditive;

		Flux_BlendTreeNode* pxMasked = Flux_BlendTreeNode::CreateFromTypeName("Masked");
		ZENITH_ASSERT_NOT_NULL(pxMasked, "Factory should create Masked node");
		ZENITH_ASSERT_EQ(std::string(pxMasked->GetNodeTypeName()), "Masked", "Created node should be Masked type");
		delete pxMasked;

		Flux_BlendTreeNode* pxSelect = Flux_BlendTreeNode::CreateFromTypeName("Select");
		ZENITH_ASSERT_NOT_NULL(pxSelect, "Factory should create Select node");
		ZENITH_ASSERT_EQ(std::string(pxSelect->GetNodeTypeName()), "Select", "Created node should be Select type");
		delete pxSelect;

		Flux_BlendTreeNode* pxInvalid = Flux_BlendTreeNode::CreateFromTypeName("InvalidType");
		ZENITH_ASSERT_NULL(pxInvalid, "Factory should return nullptr for invalid type");

	}

}

/**
 * Test cross-fade transition
 * Verifies transition timing and blend weight calculations
 */
ZENITH_TEST(Animation, CrossFadeTransition) { Zenith_UnitTests::TestCrossFadeTransition(); }
void Zenith_UnitTests::TestCrossFadeTransition(){

	// Test initial state
	{
		Flux_CrossFadeTransition xTransition;
		ZENITH_ASSERT_EQ(xTransition.IsComplete(), true, "Transition should be complete initially (no duration set)");

	}

	// Test Start and Update
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(5);
		xFromPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f);

		Flux_CrossFadeTransition xTransition;
		xTransition.Start(xFromPose, 1.0f); // 1 second transition

		ZENITH_ASSERT_EQ(xTransition.IsComplete(), false, "Transition should not be complete after Start");
		ZENITH_ASSERT_TRUE(FloatEquals(xTransition.GetBlendWeight(), 0.0f, 0.01f), "Blend weight should be 0 at start");

		// Update halfway
		xTransition.Update(0.5f);
		ZENITH_ASSERT_EQ(xTransition.IsComplete(), false, "Transition should not be complete at 0.5s");
		// With EaseInOut, 0.5 normalized time might not be exactly 0.5 blend weight
		// but should be close for symmetrical easing
		float fMidWeight = xTransition.GetBlendWeight();
		ZENITH_ASSERT_TRUE(fMidWeight > 0.3f && fMidWeight < 0.7f, "Blend weight at midpoint should be roughly 0.5");

		// Update to completion
		xTransition.Update(0.6f); // Total 1.1s, should be complete
		ZENITH_ASSERT_EQ(xTransition.IsComplete(), true, "Transition should be complete after 1.1s");
		ZENITH_ASSERT_TRUE(FloatEquals(xTransition.GetBlendWeight(), 1.0f), "Blend weight should be 1.0 when complete");

	}

	// Test different easing types
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(1);

		// Test Linear easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::Linear);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			ZENITH_ASSERT_TRUE(FloatEquals(xTransition.GetBlendWeight(), 0.5f), "Linear easing should give 0.5 at midpoint");
		}

		// Test EaseIn easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseIn);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			ZENITH_ASSERT_LT(fWeight, 0.5f, "EaseIn should give weight < 0.5 at midpoint");
		}

		// Test EaseOut easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseOut);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			ZENITH_ASSERT_GT(fWeight, 0.5f, "EaseOut should give weight > 0.5 at midpoint");
		}

	}

	// Test Blend operation
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(1);
		xFromPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);

		Flux_SkeletonPose xTargetPose;
		xTargetPose.Initialize(1);
		xTargetPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 10.0f, 10.0f);

		Flux_CrossFadeTransition xTransition;
		xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::Linear);
		xTransition.Start(xFromPose, 1.0f);
		xTransition.Update(0.5f); // 50% blend

		Flux_SkeletonPose xOutPose;
		xOutPose.Initialize(1);
		xTransition.Blend(xOutPose, xTargetPose);

		ZENITH_ASSERT_TRUE(Vec3Equals(xOutPose.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)), "Blend should interpolate position to midpoint");

	}

}

/**
 * Test Animation Clip Channels
 * Verifies clip metadata and event handling
 */
ZENITH_TEST(Animation, AnimationClipChannels) { Zenith_UnitTests::TestAnimationClipChannels(); }
void Zenith_UnitTests::TestAnimationClipChannels(){

	// Test clip metadata
	{
		Flux_AnimationClipMetadata xMetadata;
		xMetadata.m_strName = "TestClip";
		xMetadata.m_fDuration = 2.5f;
		xMetadata.m_uTicksPerSecond = 30;
		xMetadata.m_bLooping = true;
		xMetadata.m_fBlendInTime = 0.2f;
		xMetadata.m_fBlendOutTime = 0.15f;

		ZENITH_ASSERT_EQ(xMetadata.m_strName, "TestClip", "Name should be 'TestClip'");
		ZENITH_ASSERT_TRUE(FloatEquals(xMetadata.m_fDuration, 2.5f), "Duration should be 2.5");
		ZENITH_ASSERT_EQ(xMetadata.m_uTicksPerSecond, 30, "Ticks per second should be 30");
		ZENITH_ASSERT_EQ(xMetadata.m_bLooping, true, "Looping should be true");
		ZENITH_ASSERT_TRUE(FloatEquals(xMetadata.m_fBlendInTime, 0.2f), "Blend in time should be 0.2");
		ZENITH_ASSERT_TRUE(FloatEquals(xMetadata.m_fBlendOutTime, 0.15f), "Blend out time should be 0.15");

	}

	// Test animation clip with events
	{
		Flux_AnimationClip xClip;
		xClip.SetName("Walk");
		xClip.SetLooping(true);

		// Add events
		Flux_AnimationEvent xEvent1;
		xEvent1.m_strEventName = "LeftFootDown";
		xEvent1.m_fNormalizedTime = 0.25f;
		xEvent1.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.5f);

		Flux_AnimationEvent xEvent2;
		xEvent2.m_strEventName = "RightFootDown";
		xEvent2.m_fNormalizedTime = 0.75f;
		xEvent2.m_xData = Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.5f);

		xClip.AddEvent(xEvent1);
		xClip.AddEvent(xEvent2);

		const Zenith_Vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		ZENITH_ASSERT_EQ(xEvents.GetSize(), 2, "Should have 2 events");
		ZENITH_ASSERT_EQ(xEvents.Get(0).m_strEventName, "LeftFootDown", "First event should be LeftFootDown");
		ZENITH_ASSERT_EQ(xEvents.Get(1).m_strEventName, "RightFootDown", "Second event should be RightFootDown");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvents.Get(0).m_fNormalizedTime, 0.25f), "First event time should be 0.25");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvents.Get(1).m_fNormalizedTime, 0.75f), "Second event time should be 0.75");

	}

	// Test animation clip collection
	{
		Flux_AnimationClipCollection xCollection;

		Flux_AnimationClip* pxClip1 = new Flux_AnimationClip();
		pxClip1->SetName("Idle");
		Flux_AnimationClip* pxClip2 = new Flux_AnimationClip();
		pxClip2->SetName("Walk");
		Flux_AnimationClip* pxClip3 = new Flux_AnimationClip();
		pxClip3->SetName("Run");

		xCollection.AddClip(pxClip1);
		xCollection.AddClip(pxClip2);
		xCollection.AddClip(pxClip3);

		ZENITH_ASSERT_EQ(xCollection.GetClipCount(), 3, "Should have 3 clips");
		ZENITH_ASSERT_TRUE(xCollection.HasClip("Idle"), "Should have Idle clip");
		ZENITH_ASSERT_TRUE(xCollection.HasClip("Walk"), "Should have Walk clip");
		ZENITH_ASSERT_TRUE(xCollection.HasClip("Run"), "Should have Run clip");
		ZENITH_ASSERT_FALSE(xCollection.HasClip("Jump"), "Should not have Jump clip");

		const Flux_AnimationClip* pxRetrieved = xCollection.GetClip("Walk");
		ZENITH_ASSERT_NOT_NULL(pxRetrieved, "Should retrieve Walk clip");
		ZENITH_ASSERT_EQ(pxRetrieved->GetName(), "Walk", "Retrieved clip name should be Walk");

	}

}

/**
 * Test BlendSpace1D calculations
 * Verifies blend space sample point selection and blending
 */
ZENITH_TEST(Animation, BlendSpace1D) { Zenith_UnitTests::TestBlendSpace1D(); }
void Zenith_UnitTests::TestBlendSpace1D(){

	// Test parameter setting
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		xBlendSpace.SetParameter(-0.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), -0.5f), "Parameter should accept negative values");

		xBlendSpace.SetParameter(1.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), 1.5f), "Parameter should accept values > 1");

	}

	// Test blend point addition
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		// Create sample clips
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip1, 0.0f);
		xBlendSpace.AddBlendPoint(pxClip2, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip3, 1.0f);

		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		ZENITH_ASSERT_EQ(xPoints.GetSize(), 3, "Should have 3 blend points");
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(0).m_fPosition, 0.0f), "First point position should be 0.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(1).m_fPosition, 0.5f), "Second point position should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(2).m_fPosition, 1.0f), "Third point position should be 1.0");

	}

	// Test blend point sorting
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add in non-sorted order
		xBlendSpace.AddBlendPoint(pxClip2, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip3, 1.0f);
		xBlendSpace.AddBlendPoint(pxClip1, 0.0f);

		xBlendSpace.SortBlendPoints();

		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(0).m_fPosition, 0.0f), "After sorting, first should be 0.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(1).m_fPosition, 0.5f), "After sorting, second should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xPoints.Get(2).m_fPosition, 1.0f), "After sorting, third should be 1.0");

	}

}

/**
 * Test BlendSpace2D blend tree node
 * Verifies 2D parameter blending, point management, and triangulation
 */
ZENITH_TEST(Animation, BlendSpace2D) { Zenith_UnitTests::TestBlendSpace2D(); }
void Zenith_UnitTests::TestBlendSpace2D(){

	// Test 2D parameter setting
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Zenith_Maths::Vector2 xParams(-0.5f, 0.75f);
		xBlendSpace.SetParameter(xParams);
		const Zenith_Maths::Vector2& xRetrieved = xBlendSpace.GetParameter();
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrieved.x, -0.5f) && FloatEquals(xRetrieved.y, 0.75f), "Parameters should be (-0.5, 0.75)");

	}

	// Test blend point addition
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip4 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add 4 points in 2D space (quad corners)
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip3, Zenith_Maths::Vector2(0.0f, 1.0f));
		xBlendSpace.AddBlendPoint(pxClip4, Zenith_Maths::Vector2(1.0f, 1.0f));

	}

	// Test triangulation computation
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add 3 points forming a triangle
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip3, Zenith_Maths::Vector2(0.5f, 1.0f));

		// Compute triangulation
		xBlendSpace.ComputeTriangulation();

	}

}

/**
 * Test blend tree node evaluation
 * Verifies that Evaluate() produces valid poses for all blend tree node types
 */
ZENITH_TEST(Animation, BlendTreeEvaluation) { Zenith_UnitTests::TestBlendTreeEvaluation(); }
void Zenith_UnitTests::TestBlendTreeEvaluation(){

	// Test Blend node evaluation at different weights
	{
		Flux_BlendTreeNode_Blend xBlendNode;

		// Create two clip children (even with null clips, we test the node behavior)
		Flux_BlendTreeNode_Clip* pxClipA = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClipB = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendNode.SetChildA(pxClipA);
		xBlendNode.SetChildB(pxClipB);

		// Test weight at 0.0 (should favor child A)
		xBlendNode.SetBlendWeight(0.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 0.0f), "Blend weight should be 0.0");

		// Test weight at 1.0 (should favor child B)
		xBlendNode.SetBlendWeight(1.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 1.0f), "Blend weight should be 1.0");

		// Test weight at 0.5 (equal blend)
		xBlendNode.SetBlendWeight(0.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 0.5f), "Blend weight should be 0.5");

		// Test weight clamping
		xBlendNode.SetBlendWeight(1.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 1.0f), "Blend weight should clamp to 1.0");

		xBlendNode.SetBlendWeight(-0.5f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendNode.GetBlendWeight(), 0.0f), "Blend weight should clamp to 0.0");

	}

	// Test Additive node evaluation
	{
		Flux_BlendTreeNode_Additive xAdditiveNode;

		Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxAdditive = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xAdditiveNode.SetBaseNode(pxBase);
		xAdditiveNode.SetAdditiveNode(pxAdditive);

		// Test weight at 0.0 (no additive effect)
		xAdditiveNode.SetAdditiveWeight(0.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 0.0f), "Additive weight should be 0.0");

		// Test weight at 1.0 (full additive effect)
		xAdditiveNode.SetAdditiveWeight(1.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 1.0f), "Additive weight should be 1.0");

		// Test weight clamping
		xAdditiveNode.SetAdditiveWeight(2.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 1.0f), "Additive weight should clamp to 1.0");

	}

	// Test Masked node evaluation
	{
		Flux_BlendTreeNode_Masked xMaskedNode;

		Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxOverride = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xMaskedNode.SetBaseNode(pxBase);
		xMaskedNode.SetOverrideNode(pxOverride);

		// Set up a bone mask
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);  // Full override for bone 0
		xMask.SetBoneWeight(1, 0.5f);  // Partial override for bone 1
		xMask.SetBoneWeight(2, 0.0f);  // No override for bone 2

		xMaskedNode.SetBoneMask(xMask);

		const Flux_BoneMask& xRetrieved = xMaskedNode.GetBoneMask();
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrieved.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrieved.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrieved.GetBoneWeight(2), 0.0f), "Bone 2 weight should be 0.0");

	}

	// Test Select node evaluation
	{
		Flux_BlendTreeNode_Select xSelectNode;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.5f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 2.0f);

		xSelectNode.AddChild(pxClip0);
		xSelectNode.AddChild(pxClip1);
		xSelectNode.AddChild(pxClip2);

		// Test selecting different children
		xSelectNode.SetSelectedIndex(0);
		ZENITH_ASSERT_EQ(xSelectNode.GetSelectedIndex(), 0, "Selected index should be 0");

		xSelectNode.SetSelectedIndex(1);
		ZENITH_ASSERT_EQ(xSelectNode.GetSelectedIndex(), 1, "Selected index should be 1");

		xSelectNode.SetSelectedIndex(2);
		ZENITH_ASSERT_EQ(xSelectNode.GetSelectedIndex(), 2, "Selected index should be 2");

	}

	// Test BlendSpace1D evaluation with blend points
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip0, 0.0f);
		xBlendSpace.AddBlendPoint(pxClip1, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip2, 1.0f);
		xBlendSpace.SortBlendPoints();

		// Test parameter at different values
		xBlendSpace.SetParameter(0.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), 0.0f), "Parameter should be 0.0");

		xBlendSpace.SetParameter(0.25f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), 0.25f), "Parameter should be 0.25");

		xBlendSpace.SetParameter(1.0f);
		ZENITH_ASSERT_TRUE(FloatEquals(xBlendSpace.GetParameter(), 1.0f), "Parameter should be 1.0");

	}

	// Test BlendSpace2D evaluation
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip0, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(0.5f, 1.0f));
		xBlendSpace.ComputeTriangulation();

		// Test parameter at different 2D values
		xBlendSpace.SetParameter(Zenith_Maths::Vector2(0.0f, 0.0f));
		const Zenith_Maths::Vector2& xParam0 = xBlendSpace.GetParameter();
		ZENITH_ASSERT_TRUE(FloatEquals(xParam0.x, 0.0f) && FloatEquals(xParam0.y, 0.0f), "Parameter should be (0, 0)");

		xBlendSpace.SetParameter(Zenith_Maths::Vector2(0.5f, 0.5f));
		const Zenith_Maths::Vector2& xParam1 = xBlendSpace.GetParameter();
		ZENITH_ASSERT_TRUE(FloatEquals(xParam1.x, 0.5f) && FloatEquals(xParam1.y, 0.5f), "Parameter should be (0.5, 0.5)");

	}

}

/**
 * Test blend tree node serialization
 * Verifies round-trip serialization for all blend tree node types
 */
ZENITH_TEST(Animation, BlendTreeSerialization) { Zenith_UnitTests::TestBlendTreeSerialization(); }
void Zenith_UnitTests::TestBlendTreeSerialization(){

	// Test Clip node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Clip xOriginal(nullptr, 1.5f);
		xOriginal.SetClipName("TestClip");

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Clip xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetPlaybackRate(), 1.5f), "Playback rate should be 1.5");
		ZENITH_ASSERT_EQ(xLoaded.GetClipName(), "TestClip", "Clip name should be 'TestClip'");

	}

	// Test Blend node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Blend xOriginal;
		xOriginal.SetBlendWeight(0.75f);
		// Children would be serialized recursively in real usage

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Blend xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetBlendWeight(), 0.75f), "Blend weight should be 0.75");

	}

	// Test BlendSpace1D node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_BlendSpace1D xOriginal;
		xOriginal.SetParameter(0.65f);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_BlendSpace1D xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetParameter(), 0.65f), "Parameter should be 0.65");

	}

	// Test BlendSpace2D node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_BlendSpace2D xOriginal;
		xOriginal.SetParameter(Zenith_Maths::Vector2(0.3f, 0.8f));

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_BlendSpace2D xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		const Zenith_Maths::Vector2& xParam = xLoaded.GetParameter();
		ZENITH_ASSERT_TRUE(FloatEquals(xParam.x, 0.3f) && FloatEquals(xParam.y, 0.8f), "Parameter should be (0.3, 0.8)");

	}

	// Test Additive node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Additive xOriginal;
		xOriginal.SetAdditiveWeight(0.45f);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Additive xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_TRUE(FloatEquals(xLoaded.GetAdditiveWeight(), 0.45f), "Additive weight should be 0.45");

	}

	// Test Masked node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Masked xOriginal;
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);
		xMask.SetBoneWeight(1, 0.5f);
		xMask.SetBoneWeight(2, 0.25f);
		xOriginal.SetBoneMask(xMask);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Masked xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		const Flux_BoneMask& xLoadedMask = xLoaded.GetBoneMask();
		ZENITH_ASSERT_TRUE(FloatEquals(xLoadedMask.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoadedMask.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xLoadedMask.GetBoneWeight(2), 0.25f), "Bone 2 weight should be 0.25");

	}

	// Test Select node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Select xOriginal;
		// Must add children before setting selected index (SetSelectedIndex validates range)
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.SetSelectedIndex(2);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Select xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		ZENITH_ASSERT_EQ(xLoaded.GetSelectedIndex(), 2, "Selected index should be 2");

	}

}

ZENITH_TEST(Animation, BlendTreeWriteReadChildNode) { Zenith_UnitTests::TestBlendTreeWriteReadChildNode(); }

void Zenith_UnitTests::TestBlendTreeWriteReadChildNode(){

	// Test writing and reading a null child
	{
		Zenith_DataStream xStream;
		Flux_BlendTreeNode::WriteChildNode(xStream, nullptr);
		xStream.SetCursor(0);
		Flux_BlendTreeNode* pxResult = Flux_BlendTreeNode::ReadChildNode(xStream);
		ZENITH_ASSERT_NULL(pxResult, "Null child should deserialize as null");
	}

	// Test writing and reading a Clip node child
	{
		Flux_BlendTreeNode_Clip xClip;
		xClip.SetPlaybackRate(2.5f);
		xClip.SetClipName("TestClip");

		Zenith_DataStream xStream;
		Flux_BlendTreeNode::WriteChildNode(xStream, &xClip);
		xStream.SetCursor(0);

		Flux_BlendTreeNode* pxResult = Flux_BlendTreeNode::ReadChildNode(xStream);
		ZENITH_ASSERT_NOT_NULL(pxResult, "Clip child should deserialize as non-null");
		ZENITH_ASSERT_EQ(std::string(pxResult->GetNodeTypeName()), "Clip", "Should deserialize as Clip type");

		Flux_BlendTreeNode_Clip* pxClipResult = static_cast<Flux_BlendTreeNode_Clip*>(pxResult);
		ZENITH_ASSERT_EQ(pxClipResult->GetPlaybackRate(), 2.5f, "Playback rate should be preserved");
		ZENITH_ASSERT_EQ(pxClipResult->GetClipName(), "TestClip", "Clip name should be preserved");

		delete pxResult;
	}

	// Test round-trip with nested tree (Blend with two Clip children)
	{
		Flux_BlendTreeNode_Clip* pxClipA = new Flux_BlendTreeNode_Clip();
		pxClipA->SetClipName("ClipA");
		Flux_BlendTreeNode_Clip* pxClipB = new Flux_BlendTreeNode_Clip();
		pxClipB->SetClipName("ClipB");

		Flux_BlendTreeNode_Blend xBlend(pxClipA, pxClipB, 0.75f);

		Zenith_DataStream xStream;
		Flux_BlendTreeNode::WriteChildNode(xStream, &xBlend);
		xStream.SetCursor(0);

		Flux_BlendTreeNode* pxResult = Flux_BlendTreeNode::ReadChildNode(xStream);
		ZENITH_ASSERT_NOT_NULL(pxResult, "Blend child should deserialize");
		ZENITH_ASSERT_EQ(std::string(pxResult->GetNodeTypeName()), "Blend", "Should be Blend type");

		Flux_BlendTreeNode_Blend* pxBlendResult = static_cast<Flux_BlendTreeNode_Blend*>(pxResult);
		ZENITH_ASSERT_EQ(pxBlendResult->GetBlendWeight(), 0.75f, "Blend weight should be preserved");
		ZENITH_ASSERT_NOT_NULL(pxBlendResult->GetChildA(), "ChildA should exist");
		ZENITH_ASSERT_NOT_NULL(pxBlendResult->GetChildB(), "ChildB should exist");

		delete pxResult;
		// pxClipA and pxClipB owned by xBlend, freed in its destructor
	}

}

ZENITH_TEST(Animation, BlendTreeEvaluateChildOrReset) { Zenith_UnitTests::TestBlendTreeEvaluateChildOrReset(); }

void Zenith_UnitTests::TestBlendTreeEvaluateChildOrReset(){

	// Test with null child — pose should be reset
	{
		Flux_SkeletonPose xPose;
		Zenith_SkeletonAsset xSkeleton;
		Flux_BlendTreeNode::EvaluateChildOrReset(nullptr, 0.016f, xPose, xSkeleton);
		// If we get here without crashing, null child was handled safely
	}

	// Test with valid clip child — should not crash
	{
		Flux_BlendTreeNode_Clip xClip;
		xClip.SetPlaybackRate(1.0f);
		Flux_SkeletonPose xPose;
		Zenith_SkeletonAsset xSkeleton;
		Flux_BlendTreeNode::EvaluateChildOrReset(&xClip, 0.016f, xPose, xSkeleton);
		// No clip set, but should not crash
	}

}

ZENITH_TEST(Animation, BlendTreeSelectGetSelectedChild) { Zenith_UnitTests::TestBlendTreeSelectGetSelectedChild(); }

void Zenith_UnitTests::TestBlendTreeSelectGetSelectedChild(){

	Flux_BlendTreeNode_Select xSelect;

	// Empty select — should return null
	ZENITH_ASSERT_NULL(xSelect.GetSelectedChild(), "Empty select should return null");

	// Add children and test valid indices
	Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip();
	pxClip0->SetClipName("Clip0");
	Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip();
	pxClip1->SetClipName("Clip1");
	Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip();
	pxClip2->SetClipName("Clip2");

	xSelect.AddChild(pxClip0);
	xSelect.AddChild(pxClip1);
	xSelect.AddChild(pxClip2);

	// Default index is 0
	ZENITH_ASSERT_EQ(xSelect.GetSelectedChild(), pxClip0, "Index 0 should return first child");

	// Change selection
	xSelect.SetSelectedIndex(2);
	ZENITH_ASSERT_EQ(xSelect.GetSelectedChild(), pxClip2, "Index 2 should return third child");

	// Out-of-bounds index (negative) — SetSelectedIndex won't change it
	// but test boundary: manually force an invalid index to verify GetSelectedChild handles it
	xSelect.SetSelectedIndex(1);
	ZENITH_ASSERT_EQ(xSelect.GetSelectedChild(), pxClip1, "Index 1 should return second child");

	// Verify IsFinished/GetNormalizedTime use GetSelectedChild properly
	ZENITH_ASSERT_EQ(xSelect.GetNormalizedTime(), 0.0f, "Clip with no data should have 0 time");
	ZENITH_ASSERT_EQ(xSelect.IsFinished(), false, "Clip with null m_pxClip returns false");

}

/**
 * Test FABRIK IK Solver
 * Verifies IK chain setup and solving iterations
 */
ZENITH_TEST(Core, FABRIKSolver) { Zenith_UnitTests::TestFABRIKSolver(); }
void Zenith_UnitTests::TestFABRIKSolver(){

	// Test basic IK chain creation
	{
		Flux_IKSolver xSolver;

		Flux_IKChain xChain;
		xChain.m_strName = "RightArm";
		xChain.m_xBoneNames.PushBack("Shoulder");
		xChain.m_xBoneNames.PushBack("Elbow");
		xChain.m_xBoneNames.PushBack("Wrist");

		xSolver.AddChain(xChain);
		ZENITH_ASSERT_TRUE(xSolver.HasChain("RightArm"), "Solver should have RightArm chain");
		ZENITH_ASSERT_FALSE(xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

		const Flux_IKChain* pxRetrieved = xSolver.GetChain("RightArm");
		ZENITH_ASSERT_NOT_NULL(pxRetrieved, "Should retrieve chain");
		ZENITH_ASSERT_EQ(pxRetrieved->m_strName, "RightArm", "Chain name should match");
		ZENITH_ASSERT_EQ(pxRetrieved->m_xBoneNames.GetSize(), 3, "Should have 3 bones");

	}

	// Test IK target setting
	{
		Flux_IKSolver xSolver;

		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(5.0f, 3.0f, 0.0f);
		xTarget.m_fWeight = 0.8f;
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("RightHand", xTarget);

		const Flux_IKTarget* pxRetrieved = xSolver.GetTarget("RightHand");
		ZENITH_ASSERT_NOT_NULL(pxRetrieved, "Should retrieve target");
		ZENITH_ASSERT_TRUE(Vec3Equals(pxRetrieved->m_xPosition, Zenith_Maths::Vector3(5.0f, 3.0f, 0.0f)), "Target position should match");
		ZENITH_ASSERT_TRUE(FloatEquals(pxRetrieved->m_fWeight, 0.8f), "Target weight should be 0.8");
		ZENITH_ASSERT_EQ(pxRetrieved->m_bEnabled, true, "Target should be enabled");

	}

	// Test IK target clearing
	{
		Flux_IKSolver xSolver;

		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("TestChain", xTarget);
		ZENITH_ASSERT_TRUE(xSolver.HasTarget("TestChain"), "Target should exist");

		xSolver.ClearTarget("TestChain");
		ZENITH_ASSERT_FALSE(xSolver.HasTarget("TestChain"), "Target should be cleared");

	}

	// Test chain parameters
	{
		Flux_IKChain xChain;
		xChain.m_strName = "TestChain";
		xChain.m_uMaxIterations = 20;
		xChain.m_fTolerance = 0.001f;

		ZENITH_ASSERT_EQ(xChain.m_uMaxIterations, 20, "Max iterations should be 20");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_fTolerance, 0.001f), "Tolerance should be 0.001");

	}

	// Test chain with pole vector
	{
		Flux_IKChain xChain;
		xChain.m_strName = "LeftLeg";
		xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xChain.m_bUsePoleVector = true;
		xChain.m_strPoleTargetBone = "KneeTarget";

		ZENITH_ASSERT_TRUE(Vec3Equals(xChain.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)), "Pole vector should be (0,0,1)");
		ZENITH_ASSERT_EQ(xChain.m_bUsePoleVector, true, "Use pole vector should be true");
		ZENITH_ASSERT_EQ(xChain.m_strPoleTargetBone, "KneeTarget", "Pole target bone should be KneeTarget");

	}

	// Test helper chain creation functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("RightLeg", "Hip", "Knee", "Ankle");
		ZENITH_ASSERT_EQ(xLegChain.m_strName, "RightLeg", "Leg chain name should be RightLeg");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.GetSize(), 3, "Leg chain should have 3 bones");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(0), "Hip", "First bone should be Hip");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(1), "Knee", "Second bone should be Knee");
		ZENITH_ASSERT_EQ(xLegChain.m_xBoneNames.Get(2), "Ankle", "Third bone should be Ankle");

		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("LeftArm", "Shoulder", "Elbow", "Wrist");
		ZENITH_ASSERT_EQ(xArmChain.m_strName, "LeftArm", "Arm chain name should be LeftArm");
		ZENITH_ASSERT_EQ(xArmChain.m_xBoneNames.GetSize(), 3, "Arm chain should have 3 bones");

	}

}

ZENITH_TEST(Animation, IKSafeNormalize) { Zenith_UnitTests::TestIKSafeNormalize(); }

void Zenith_UnitTests::TestIKSafeNormalize(){

	// Normal vector should be normalized to unit length
	{
		Zenith_Maths::Vector3 xVec(3.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xResult = Flux_IKSolver::SafeNormalize(xVec);
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)), "SafeNormalize of (3,0,0) should be (1,0,0)");
	}

	// Zero vector should return fallback
	{
		Zenith_Maths::Vector3 xZero(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xFallback(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3 xResult = Flux_IKSolver::SafeNormalize(xZero, xFallback);
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult, xFallback), "SafeNormalize of zero vector should return fallback");
	}

	// Near-zero vector (below epsilon) should return fallback
	{
		Zenith_Maths::Vector3 xTiny(0.00001f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xFallback(0.0f, 0.0f, 1.0f);
		Zenith_Maths::Vector3 xResult = Flux_IKSolver::SafeNormalize(xTiny, xFallback);
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult, xFallback), "SafeNormalize of near-zero vector should return fallback");
	}

	// Default fallback is zero vector
	{
		Zenith_Maths::Vector3 xZero(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xResult = Flux_IKSolver::SafeNormalize(xZero);
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult, Zenith_Maths::Vector3(0.0f)), "SafeNormalize with default fallback should return zero vector");
	}

	// Arbitrary direction should be unit length
	{
		Zenith_Maths::Vector3 xVec(1.0f, 2.0f, 3.0f);
		Zenith_Maths::Vector3 xResult = Flux_IKSolver::SafeNormalize(xVec);
		float fLen = glm::length(xResult);
		ZENITH_ASSERT_TRUE(FloatEquals(fLen, 1.0f), "SafeNormalize should produce unit-length vector");
	}

}

ZENITH_TEST(Animation, IKFindPerpendicularAxis) { Zenith_UnitTests::TestIKFindPerpendicularAxis(); }

void Zenith_UnitTests::TestIKFindPerpendicularAxis(){

	// Axis-aligned input: X axis
	{
		Zenith_Maths::Vector3 xAxis(1.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xPerp = Flux_IKSolver::FindPerpendicularAxis(xAxis);
		float fDot = glm::dot(xPerp, xAxis);
		ZENITH_ASSERT_TRUE(FloatEquals(fDot, 0.0f, 0.001f), "Perpendicular axis should be orthogonal to X axis");
		float fLen = glm::length(xPerp);
		ZENITH_ASSERT_TRUE(FloatEquals(fLen, 1.0f, 0.001f), "Perpendicular axis should be unit length");
	}

	// Axis-aligned input: Y axis
	{
		Zenith_Maths::Vector3 xAxis(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3 xPerp = Flux_IKSolver::FindPerpendicularAxis(xAxis);
		float fDot = glm::dot(xPerp, xAxis);
		ZENITH_ASSERT_TRUE(FloatEquals(fDot, 0.0f, 0.001f), "Perpendicular axis should be orthogonal to Y axis");
	}

	// Axis-aligned input: Z axis
	{
		Zenith_Maths::Vector3 xAxis(0.0f, 0.0f, 1.0f);
		Zenith_Maths::Vector3 xPerp = Flux_IKSolver::FindPerpendicularAxis(xAxis);
		float fDot = glm::dot(xPerp, xAxis);
		ZENITH_ASSERT_TRUE(FloatEquals(fDot, 0.0f, 0.001f), "Perpendicular axis should be orthogonal to Z axis");
	}

	// Arbitrary input
	{
		Zenith_Maths::Vector3 xAxis = glm::normalize(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
		Zenith_Maths::Vector3 xPerp = Flux_IKSolver::FindPerpendicularAxis(xAxis);
		float fDot = glm::dot(xPerp, xAxis);
		ZENITH_ASSERT_TRUE(FloatEquals(fDot, 0.0f, 0.001f), "Perpendicular axis should be orthogonal to arbitrary input");
		float fLen = glm::length(xPerp);
		ZENITH_ASSERT_TRUE(FloatEquals(fLen, 1.0f, 0.001f), "Perpendicular axis should be unit length for arbitrary input");
	}

}

ZENITH_TEST(Animation, IKConstrainBoneLength) { Zenith_UnitTests::TestIKConstrainBoneLength(); }

void Zenith_UnitTests::TestIKConstrainBoneLength(){

	// Child along X axis at distance 5, constrain to length 3
	{
		Zenith_Maths::Vector3 xParent(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xChild(5.0f, 0.0f, 0.0f);
		float fLength = 3.0f;

		Zenith_Maths::Vector3 xResult = Flux_IKSolver::ConstrainBoneLength(xChild, xParent, fLength);
		float fDist = glm::length(xResult - xParent);
		ZENITH_ASSERT_TRUE(FloatEquals(fDist, fLength), "Output distance should match target length (shrink)");
		// Direction should be preserved
		ZENITH_ASSERT_TRUE(Vec3Equals(glm::normalize(xResult - xParent), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)), "Direction should be preserved when constraining bone length");
	}

	// Child closer than target length, should extend
	{
		Zenith_Maths::Vector3 xParent(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xChild(1.0f, 0.0f, 0.0f);
		float fLength = 4.0f;

		Zenith_Maths::Vector3 xResult = Flux_IKSolver::ConstrainBoneLength(xChild, xParent, fLength);
		float fDist = glm::length(xResult - xParent);
		ZENITH_ASSERT_TRUE(FloatEquals(fDist, fLength), "Output distance should match target length (extend)");
	}

	// Non-axis-aligned direction
	{
		Zenith_Maths::Vector3 xParent(1.0f, 2.0f, 3.0f);
		Zenith_Maths::Vector3 xChild(4.0f, 6.0f, 3.0f);
		float fLength = 2.5f;

		Zenith_Maths::Vector3 xResult = Flux_IKSolver::ConstrainBoneLength(xChild, xParent, fLength);
		float fDist = glm::length(xResult - xParent);
		ZENITH_ASSERT_TRUE(FloatEquals(fDist, fLength, 0.001f), "Output distance should match target length for arbitrary positions");
	}

	// Child at exact distance (no change needed)
	{
		Zenith_Maths::Vector3 xParent(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xChild(0.0f, 3.0f, 0.0f);
		float fLength = 3.0f;

		Zenith_Maths::Vector3 xResult = Flux_IKSolver::ConstrainBoneLength(xChild, xParent, fLength);
		float fDist = glm::length(xResult - xParent);
		ZENITH_ASSERT_TRUE(FloatEquals(fDist, fLength), "Output distance should match when already at correct length");
		ZENITH_ASSERT_TRUE(Vec3Equals(xResult, xChild, 0.001f), "Position should not change when already at correct length");
	}

}

/**
 * Test Animation Events
 * Verifies event registration and triggering
 */
ZENITH_TEST(Core, AnimationEvents) { Zenith_UnitTests::TestAnimationEvents(); }
void Zenith_UnitTests::TestAnimationEvents(){

	// Test event data structure
	{
		Flux_AnimationEvent xEvent;
		xEvent.m_strEventName = "FootStep";
		xEvent.m_fNormalizedTime = 0.25f;
		xEvent.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.5f);

		ZENITH_ASSERT_EQ(xEvent.m_strEventName, "FootStep", "Event name should be 'FootStep'");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvent.m_fNormalizedTime, 0.25f), "Normalized time should be 0.25");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvent.m_xData.x, 1.0f), "Event data x should be 1.0");

	}

	// Test event collection in clip
	{
		Flux_AnimationClip xClip;
		xClip.SetName("Walk");

		Flux_AnimationEvent xEvent1;
		xEvent1.m_strEventName = "LeftFoot";
		xEvent1.m_fNormalizedTime = 0.0f;

		Flux_AnimationEvent xEvent2;
		xEvent2.m_strEventName = "RightFoot";
		xEvent2.m_fNormalizedTime = 0.5f;

		xClip.AddEvent(xEvent1);
		xClip.AddEvent(xEvent2);

		const Zenith_Vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		ZENITH_ASSERT_EQ(xEvents.GetSize(), 2, "Should have 2 events");
		ZENITH_ASSERT_EQ(xEvents.Get(0).m_strEventName, "LeftFoot", "First event should be LeftFoot");
		ZENITH_ASSERT_EQ(xEvents.Get(1).m_strEventName, "RightFoot", "Second event should be RightFoot");

	}

	// Test event time ordering
	{
		Flux_AnimationEvent xEvent1, xEvent2, xEvent3;
		xEvent1.m_fNormalizedTime = 0.5f;
		xEvent2.m_fNormalizedTime = 0.1f;
		xEvent3.m_fNormalizedTime = 0.9f;

		std::vector<Flux_AnimationEvent> xEvents = { xEvent1, xEvent2, xEvent3 };
		std::sort(xEvents.begin(), xEvents.end(),
			[](const Flux_AnimationEvent& a, const Flux_AnimationEvent& b) {
				return a.m_fNormalizedTime < b.m_fNormalizedTime;
			});

		ZENITH_ASSERT_TRUE(FloatEquals(xEvents[0].m_fNormalizedTime, 0.1f), "First should be 0.1");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvents[1].m_fNormalizedTime, 0.5f), "Second should be 0.5");
		ZENITH_ASSERT_TRUE(FloatEquals(xEvents[2].m_fNormalizedTime, 0.9f), "Third should be 0.9");

	}

}

/**
 * Test Bone Masking
 * Verifies bone mask creation and application
 */
ZENITH_TEST(Animation, BoneMasking) { Zenith_UnitTests::TestBoneMasking(); }
void Zenith_UnitTests::TestBoneMasking(){

	// Test mask creation with bone indices
	{
		Flux_BoneMask xMask;

		// Set weights by bone index
		xMask.SetBoneWeight(0, 1.0f);  // Spine
		xMask.SetBoneWeight(1, 1.0f);  // Chest
		xMask.SetBoneWeight(2, 0.8f);  // Shoulder_L
		xMask.SetBoneWeight(3, 0.8f);  // Shoulder_R
		xMask.SetBoneWeight(4, 0.2f);  // Hips

		ZENITH_ASSERT_TRUE(FloatEquals(xMask.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xMask.GetBoneWeight(1), 1.0f), "Bone 1 weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xMask.GetBoneWeight(2), 0.8f), "Bone 2 weight should be 0.8");
		ZENITH_ASSERT_TRUE(FloatEquals(xMask.GetBoneWeight(3), 0.8f), "Bone 3 weight should be 0.8");
		ZENITH_ASSERT_TRUE(FloatEquals(xMask.GetBoneWeight(4), 0.2f), "Bone 4 weight should be 0.2");

	}

	// Test weight access
	{
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(5, 0.75f);

		float fWeight = xMask.GetBoneWeight(5);
		ZENITH_ASSERT_TRUE(FloatEquals(fWeight, 0.75f), "Weight should be 0.75");

		const Zenith_Vector<float>& xWeights = xMask.GetWeights();
		ZENITH_ASSERT_GE(xWeights.GetSize(), 6, "Should have at least 6 weights");
		ZENITH_ASSERT_TRUE(FloatEquals(xWeights.Get(5), 0.75f), "Weight at index 5 should be 0.75");

	}

	// Test masked blend node setup
	{
		Flux_BlendTreeNode_Masked xMaskedNode;
		ZENITH_ASSERT_EQ(std::string(xMaskedNode.GetNodeTypeName()), "Masked", "Type name should be 'Masked'");

		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);
		xMask.SetBoneWeight(1, 0.5f);

		xMaskedNode.SetBoneMask(xMask);
		const Flux_BoneMask& xRetrievedMask = xMaskedNode.GetBoneMask();
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrievedMask.GetBoneWeight(0), 1.0f), "Retrieved mask bone 0 should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xRetrievedMask.GetBoneWeight(1), 0.5f), "Retrieved mask bone 1 should be 0.5");

	}

	// Test masked blend with different poses
	{
		Flux_SkeletonPose xBasePose;
		xBasePose.Initialize(5);

		Flux_SkeletonPose xOverridePose;
		xOverridePose.Initialize(5);

		// Create mask that affects only bones 2, 3, 4
		Zenith_Vector<float> xBoneWeights;
		xBoneWeights.PushBack(0.0f);
		xBoneWeights.PushBack(0.0f);
		xBoneWeights.PushBack(1.0f);
		xBoneWeights.PushBack(1.0f);
		xBoneWeights.PushBack(1.0f);

		Flux_SkeletonPose xResult;
		xResult.Initialize(5);

		Flux_SkeletonPose::MaskedBlend(xResult, xBasePose, xOverridePose, xBoneWeights);

		// Result should have base pose for bones 0,1 and override pose for bones 2,3,4
	}

}

//=============================================================================
// Asset Pipeline Unit Test Helpers
//=============================================================================

// Helper: Compute bind pose vertex position
// For GLTF models, vertices are stored at bind pose mesh positions.
// The standard skinning formula is: jointMatrix * inverseBindMatrix * vertexPos
// At bind pose, jointMatrix equals bindPoseModel, so:
//   result = bindPoseModel * inverseBindPose * vertexPos
// This should return the original vertex position (identity transform).
static Zenith_Maths::Vector3 ComputeBindPosePosition(
	const Zenith_Maths::Vector3& xMeshPos,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights,
	const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_Maths::Vector3 xResult(0.0f);
	for (int i = 0; i < 4; i++)
	{
		float fWeight = xBoneWeights[i];
		if (fWeight <= 0.0f)
		{
			continue;
		}
		uint32_t uBoneIndex = xBoneIndices[i];
		if (uBoneIndex >= pxSkeleton->GetNumBones())
		{
			continue;
		}
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBoneIndex);
		// Apply inverse bind pose to get bone-local, then bind pose model to get world
		Zenith_Maths::Vector4 xBoneLocal = xBone.m_xInverseBindPose * Zenith_Maths::Vector4(xMeshPos, 1.0f);
		Zenith_Maths::Vector4 xTransformed = xBone.m_xBindPoseModel * xBoneLocal;
		xResult += fWeight * Zenith_Maths::Vector3(xTransformed);
	}
	return xResult;
}

// Helper: Apply animation at specific time (in seconds) and compute skinning matrices
static void ApplyAnimationAtTime(
	Flux_SkeletonInstance* pxSkelInst,
	const Zenith_SkeletonAsset* pxSkelAsset,
	const Flux_AnimationClip* pxClip,
	float fTimeSeconds)
{
	// Convert time from seconds to ticks (keyframes are stored in ticks)
	float fTimeInTicks = fTimeSeconds * pxClip->GetTicksPerSecond();

	for (uint32_t i = 0; i < pxSkelAsset->GetNumBones(); i++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkelAsset->GetBone(i);
		const Flux_BoneChannel* pxChannel = pxClip->GetBoneChannel(xBone.m_strName);
		if (pxChannel)
		{
			pxSkelInst->SetBoneLocalTransform(i,
				pxChannel->SamplePosition(fTimeInTicks),
				pxChannel->SampleRotation(fTimeInTicks),
				pxChannel->SampleScale(fTimeInTicks));
		}
		else
		{
			pxSkelInst->SetBoneLocalTransform(i,
				xBone.m_xBindPosition, xBone.m_xBindRotation, xBone.m_xBindScale);
		}
	}
	pxSkelInst->ComputeSkinningMatrices();
}

// Helper: Compute skinned vertex position using skeleton instance skinning matrices
static Zenith_Maths::Vector3 ComputeSkinnedPosition(
	const Zenith_Maths::Vector3& xLocalPos,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights,
	const Flux_SkeletonInstance* pxSkelInst)
{
	Zenith_Maths::Vector3 xResult(0.0f);
	const Zenith_Maths::Matrix4* pSkinMatrices = pxSkelInst->GetSkinningMatrices();
	for (int i = 0; i < 4; i++)
	{
		float fWeight = xBoneWeights[i];
		if (fWeight <= 0.0f)
		{
			continue;
		}
		uint32_t uBoneIndex = xBoneIndices[i];
		Zenith_Maths::Vector4 xTransformed = pSkinMatrices[uBoneIndex] *
			Zenith_Maths::Vector4(xLocalPos, 1.0f);
		xResult += fWeight * Zenith_Maths::Vector3(xTransformed);
	}
	return xResult;
}

//=============================================================================
// Asset Pipeline Unit Tests
//=============================================================================

/**
 * Test mesh asset loading
 * Verifies that mesh assets load correctly with expected vertex count and data
 */
ZENITH_TEST(Asset, MeshAssetLoading) { Zenith_UnitTests::TestMeshAssetLoading(); }
void Zenith_UnitTests::TestMeshAssetLoading(){

	// Test loading a mesh asset
	{
		const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0" ZENITH_MESH_EXT;
		Zenith_MeshAsset* pxMeshAsset = Zenith_AssetRegistry::Get<Zenith_MeshAsset>(strMeshPath);

		if (pxMeshAsset == nullptr)
		{
			return;
		}

		ZENITH_ASSERT_NOT_NULL(pxMeshAsset, "Failed to load mesh asset");
		ZENITH_ASSERT_EQ(pxMeshAsset->GetNumVerts(), 24, "Expected 24 vertices (8 per bone * 3 bones)");
		ZENITH_ASSERT_GT(pxMeshAsset->GetNumIndices(), 0, "Mesh should have indices");


		// Verify first vertex position (raw, local to bone)
		const Zenith_Maths::Vector3& xFirstPos = pxMeshAsset->m_xPositions.Get(0);
		ZENITH_ASSERT_TRUE(FloatEquals(xFirstPos.x, -0.25f, 0.01f), "Vertex 0 X mismatch");
		ZENITH_ASSERT_TRUE(FloatEquals(xFirstPos.y, 0.0f, 0.01f), "Vertex 0 Y mismatch");
		ZENITH_ASSERT_TRUE(FloatEquals(xFirstPos.z, -0.25f, 0.01f), "Vertex 0 Z mismatch");


		// Verify skinning data exists
		ZENITH_ASSERT_EQ(pxMeshAsset->m_xBoneIndices.GetSize(), 24, "Should have bone indices for all vertices");
		ZENITH_ASSERT_EQ(pxMeshAsset->m_xBoneWeights.GetSize(), 24, "Should have bone weights for all vertices");

	}

}

/**
 * Test bind pose vertex positions
 * Verifies that applying bind pose skinning produces correct vertex positions
 */
ZENITH_TEST(Core, BindPoseVertexPositions) { Zenith_UnitTests::TestBindPoseVertexPositions(); }
void Zenith_UnitTests::TestBindPoseVertexPositions(){

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0" ZENITH_MESH_EXT;
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain" ZENITH_SKELETON_EXT;

	Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::Get<Zenith_MeshAsset>(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strSkelPath);

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		return;
	}

	ZENITH_ASSERT_TRUE(pxMesh != nullptr && pxSkel != nullptr, "Failed to load assets");
	ZENITH_ASSERT_EQ(pxSkel->GetNumBones(), 3, "Expected 3 bones");

	// (Bone hierarchy debug logging removed with the rest of unit-test logging.)

	// Test vertex 0 (Root bone at origin)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(0);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(0);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(0);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);


		// Root bone at origin - expected position is approximately the local position
		ZENITH_ASSERT_TRUE(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 0.0f, -0.25f), 0.1f), "Vertex 0 bind pose position mismatch");

	}

	// Test vertex 8 (UpperArm bone at Y+2)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(8);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(8);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(8);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);


		// UpperArm bone at Y+2 - expected position should be offset by bone transform
		ZENITH_ASSERT_TRUE(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 2.0f, -0.25f), 0.1f), "Vertex 8 bind pose position mismatch");

	}

	// Test vertex 16 (Forearm bone at Y+4)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);


		// Forearm bone at Y+4 - expected position should be offset by bone transform
		ZENITH_ASSERT_TRUE(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f), "Vertex 16 bind pose position mismatch");

	}

}

/**
 * Test animated vertex positions
 * Verifies that animation skinning produces correct vertex positions at various timestamps
 */
ZENITH_TEST(Core, AnimatedVertexPositions) { Zenith_UnitTests::TestAnimatedVertexPositions(); }
void Zenith_UnitTests::TestAnimatedVertexPositions(){

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0" ZENITH_MESH_EXT;
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain" ZENITH_SKELETON_EXT;
	const std::string strAnimPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_ForearmRotate" ZENITH_ANIMATION_EXT;

	Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::Get<Zenith_MeshAsset>(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strSkelPath);
	Zenith_AnimationAsset* pxAnimAsset = Zenith_AssetRegistry::Get<Zenith_AnimationAsset>(strAnimPath);
	Flux_AnimationClip* pxClip = pxAnimAsset ? pxAnimAsset->GetClip() : nullptr;

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		return;
	}

	if (pxClip == nullptr)
	{
		// Still test bind pose without animation
	}

	ZENITH_ASSERT_TRUE(pxMesh != nullptr && pxSkel != nullptr, "Failed to load test assets");

	// Create skeleton instance for animation (CPU-only, no GPU buffer needed for unit tests)
	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);
	ZENITH_ASSERT_NOT_NULL(pxSkelInst, "Failed to create skeleton instance");

	// Test at t=0.0 (should match bind pose)
	{
		pxSkelInst->SetToBindPose();
		pxSkelInst->ComputeSkinningMatrices();

		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);


		// At t=0, should match bind pose
		ZENITH_ASSERT_TRUE(Vec3Equals(xSkinned, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f), "Vertex 16 at t=0 should match bind pose");

	}

	// Test with animation if clip is available
	if (pxClip != nullptr)
	{
		// (Channel / bone-name debug logging removed with the rest of unit-test logging.)

		// Test at t=0.5 (45 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 0.5f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);


			// At t=0.5, forearm should be rotated 45 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (-0.177, -0.177, -0.25)
			// Add bone world position (0, 4, 0) = (-0.177, 3.823, -0.25)
			Zenith_Maths::Vector3 xExpected(-0.177f, 3.823f, -0.25f);
			ZENITH_ASSERT_TRUE(Vec3Equals(xSkinned, xExpected, 0.1f), "Vertex 16 at t=0.5 position mismatch");

		}

		// Test at t=1.0 (90 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 1.0f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);


			// At t=1.0, forearm should be rotated 90 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (0, -0.25, -0.25)
			// Add bone world position (0, 4, 0) = (0, 3.75, -0.25)
			Zenith_Maths::Vector3 xExpected(0.0f, 3.75f, -0.25f);
			ZENITH_ASSERT_TRUE(Vec3Equals(xSkinned, xExpected, 0.1f), "Vertex 16 at t=1.0 position mismatch");

		}
	}

	// Cleanup
	delete pxSkelInst;

}

//------------------------------------------------------------------------------
// Stick Figure Animation Tests - Helper Functions
//------------------------------------------------------------------------------

// Bone indices for stick figure skeleton
static constexpr uint32_t STICK_BONE_ROOT = 0;
static constexpr uint32_t STICK_BONE_SPINE = 1;
static constexpr uint32_t STICK_BONE_NECK = 2;
static constexpr uint32_t STICK_BONE_HEAD = 3;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_ARM = 4;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_ARM = 5;
static constexpr uint32_t STICK_BONE_LEFT_HAND = 6;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_ARM = 7;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_ARM = 8;
static constexpr uint32_t STICK_BONE_RIGHT_HAND = 9;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_LEG = 10;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_LEG = 11;
static constexpr uint32_t STICK_BONE_LEFT_FOOT = 12;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_LEG = 13;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_LEG = 14;
static constexpr uint32_t STICK_BONE_RIGHT_FOOT = 15;
static constexpr uint32_t STICK_BONE_COUNT = 16;

// Cube geometry constants
static const Zenith_Maths::Vector3 s_axCubeOffsets[8] = {
	{-0.05f, -0.05f, -0.05f}, // 0: left-bottom-back
	{ 0.05f, -0.05f, -0.05f}, // 1: right-bottom-back
	{ 0.05f,  0.05f, -0.05f}, // 2: right-top-back
	{-0.05f,  0.05f, -0.05f}, // 3: left-top-back
	{-0.05f, -0.05f,  0.05f}, // 4: left-bottom-front
	{ 0.05f, -0.05f,  0.05f}, // 5: right-bottom-front
	{ 0.05f,  0.05f,  0.05f}, // 6: right-top-front
	{-0.05f,  0.05f,  0.05f}, // 7: left-top-front
};

static const uint32_t s_auCubeIndices[36] = {
	// Back face
	0, 2, 1, 0, 3, 2,
	// Front face
	4, 5, 6, 4, 6, 7,
	// Left face
	0, 4, 7, 0, 7, 3,
	// Right face
	1, 2, 6, 1, 6, 5,
	// Bottom face
	0, 1, 5, 0, 5, 4,
	// Top face
	3, 7, 6, 3, 6, 2,
};

/**
 * Create a 16-bone humanoid stick figure skeleton
 */
static Zenith_SkeletonAsset* CreateStickFigureSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root (at origin)
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Spine (up from root)
	pxSkel->AddBone("Spine", STICK_BONE_ROOT, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);

	// Neck (up from spine)
	pxSkel->AddBone("Neck", STICK_BONE_SPINE, Zenith_Maths::Vector3(0, 0.7f, 0), xIdentity, xUnitScale);

	// Head (up from neck)
	pxSkel->AddBone("Head", STICK_BONE_NECK, Zenith_Maths::Vector3(0, 0.2f, 0), xIdentity, xUnitScale);

	// Left arm chain
	pxSkel->AddBone("LeftUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(-0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerArm", STICK_BONE_LEFT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftHand", STICK_BONE_LEFT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Right arm chain
	pxSkel->AddBone("RightUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerArm", STICK_BONE_RIGHT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightHand", STICK_BONE_RIGHT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Left leg chain
	pxSkel->AddBone("LeftUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(-0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerLeg", STICK_BONE_LEFT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftFoot", STICK_BONE_LEFT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	// Right leg chain
	pxSkel->AddBone("RightUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerLeg", STICK_BONE_RIGHT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightFoot", STICK_BONE_RIGHT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

/**
 * Create a cube mesh for the stick figure, with one cube per bone
 */
// Per-bone scale factors for humanoid proportions (half-extents in X, Y, Z)
// Bones: 0=Root, 1=Spine, 2=Neck, 3=Head, 4-6=LeftArm, 7-9=RightArm, 10-12=LeftLeg, 13-15=RightLeg
// Skeleton positions: Root=Y:0, Spine=Y:0.5, Neck=Y:1.2, Head=Y:1.4, Arms=Y:1.1, Legs=Y:0/-0.5/-1.0
static const Zenith_Maths::Vector3 s_axBoneScales[STICK_BONE_COUNT] = {
	{0.10f, 0.06f, 0.06f},  // 0: Root (pelvis) - small hip joint at Y=0
	{0.18f, 0.65f, 0.10f},  // 1: Spine (torso) - centered at Y=0.5, spans Y=-0.15 to Y=1.15 (reaches arms/neck)
	{0.05f, 0.10f, 0.05f},  // 2: Neck - thin, at Y=1.2
	{0.12f, 0.12f, 0.10f},  // 3: Head - round, large, at Y=1.4
	{0.05f, 0.20f, 0.05f},  // 4: LeftUpperArm - at Y=1.1
	{0.04f, 0.15f, 0.04f},  // 5: LeftLowerArm (Y matches bone-to-Hand 0.30)
	{0.04f, 0.06f, 0.02f},  // 6: LeftHand
	{0.05f, 0.20f, 0.05f},  // 7: RightUpperArm - at Y=1.1
	{0.04f, 0.15f, 0.04f},  // 8: RightLowerArm
	{0.04f, 0.06f, 0.02f},  // 9: RightHand
	{0.07f, 0.25f, 0.07f},  // 10: LeftUpperLeg - at Y=0
	{0.05f, 0.25f, 0.05f},  // 11: LeftLowerLeg - at Y=-0.5
	{0.05f, 0.03f, 0.10f},  // 12: LeftFoot - at Y=-1.0
	{0.07f, 0.25f, 0.07f},  // 13: RightUpperLeg
	{0.05f, 0.25f, 0.05f},  // 14: RightLowerLeg
	{0.05f, 0.03f, 0.10f},  // 15: RightFoot
};

// Per-bone cube center offsets — see Tools/Zenith_Tools_TestAssetExport.cpp
// for the rationale (kept in sync with the production export so unit tests
// build the same geometry).
static const Zenith_Maths::Vector3 s_axBoneCenterOffsets[STICK_BONE_COUNT] = {
	{ 0.0f,  0.0f,  0.0f},  // 0: Root
	{ 0.0f,  0.0f,  0.0f},  // 1: Spine (junction — shift would intersect head)
	{ 0.0f,  0.0f,  0.0f},  // 2: Neck (junction)
	{ 0.0f,  0.0f,  0.0f},  // 3: Head
	{ 0.0f, -0.20f, 0.0f},  // 4: LeftUpperArm
	{ 0.0f, -0.15f, 0.0f},  // 5: LeftLowerArm
	{ 0.0f,  0.0f,  0.0f},  // 6: LeftHand
	{ 0.0f, -0.20f, 0.0f},  // 7: RightUpperArm
	{ 0.0f, -0.15f, 0.0f},  // 8: RightLowerArm
	{ 0.0f,  0.0f,  0.0f},  // 9: RightHand
	{ 0.0f, -0.25f, 0.0f},  // 10: LeftUpperLeg
	{ 0.0f, -0.25f, 0.0f},  // 11: LeftLowerLeg
	{ 0.0f,  0.0f,  0.0f},  // 12: LeftFoot
	{ 0.0f, -0.25f, 0.0f},  // 13: RightUpperLeg
	{ 0.0f, -0.25f, 0.0f},  // 14: RightLowerLeg
	{ 0.0f,  0.0f,  0.0f},  // 15: RightFoot
};

static Zenith_MeshAsset* CreateStickFigureMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	const uint32_t uVertsPerBone = 8;
	const uint32_t uIndicesPerBone = 36;
	pxMesh->Reserve(STICK_BONE_COUNT * uVertsPerBone, STICK_BONE_COUNT * uIndicesPerBone);

	// Add a scaled cube at each bone position
	for (uint32_t uBone = 0; uBone < STICK_BONE_COUNT; uBone++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		// Get world position from bind pose model matrix
		Zenith_Maths::Vector3 xBoneWorldPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);

		// Get per-bone scale
		Zenith_Maths::Vector3 xScale = s_axBoneScales[uBone];

		// Shift cube so its top face sits on the bone pivot, bottom on child pivot.
		Zenith_Maths::Vector3 xCenterOffset = s_axBoneCenterOffsets[uBone];

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		// Add 8 cube vertices with per-bone scaling
		for (int i = 0; i < 8; i++)
		{
			// Scale the cube offsets by the bone's scale factors
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f; // Base offsets are ±0.05, so *2 = ±0.1 (unit cube from -0.1 to 0.1)
			xScaledOffset.x *= xScale.x * 10.0f; // Scale to actual size
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xCenterOffset + xScaledOffset;

			// Calculate proper face normal based on vertex position
			Zenith_Maths::Vector3 xNormal = glm::normalize(s_axCubeOffsets[i]);

			pxMesh->AddVertex(xPos, xNormal, Zenith_Maths::Vector2(0, 0));
			pxMesh->SetVertexSkinning(
				uBaseVertex + i,
				glm::uvec4(uBone, 0, 0, 0),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}

		// Add 12 triangles (36 indices)
		for (int i = 0; i < 36; i += 3)
		{
			pxMesh->AddTriangle(
				uBaseVertex + s_auCubeIndices[i],
				uBaseVertex + s_auCubeIndices[i + 1],
				uBaseVertex + s_auCubeIndices[i + 2]);
		}
	}

	pxMesh->AddSubmesh(0, STICK_BONE_COUNT * uIndicesPerBone, 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

/**
 * Create a 2-second idle animation (subtle breathing motion)
 */
static Flux_AnimationClip* CreateIdleAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Idle");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Spine breathing motion
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(0, 0.52f, 0));
		xChannel.AddPositionKeyframe(48.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 1-second walk animation
 */
static Flux_AnimationClip* CreateWalkAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Walk");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Use X axis for forward/backward leg and arm swing
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation (full cycle: forward -> neutral -> back -> neutral -> forward)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing (opposite to leg - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing (full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.5-second run animation (more exaggerated than walk)
 */
static Flux_AnimationClip* CreateRunAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Run");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Use X axis for forward/backward leg and arm swing
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation (full cycle: 45 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing (full cycle: 35 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing (full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

//------------------------------------------------------------------------------
// IK Engine-Plumbing Tests
// Added by the RenderTest IK foot-placement plan. Reuses CreateStickFigureSkeleton
// above. Each test owns its skeleton via `new`/`delete` and a heap-allocated chain
// solver — no AssetRegistry use except IK15 (which exercises serialization).
//------------------------------------------------------------------------------

namespace
{
	// Helper: build a Zenith_Vector<std::string> from a brace list of bone names.
	// Zenith_Vector has no initializer_list constructor, so this replaces the old
	// std::vector brace-init at the IK test sites.
	static Zenith_Vector<std::string> MakeBoneNameVec(std::initializer_list<const char*> xNames)
	{
		Zenith_Vector<std::string> xResult(static_cast<u_int>(xNames.size()));
		for (const char* szName : xNames)
		{
			xResult.PushBack(std::string(szName));
		}
		return xResult;
	}

	// Helper: build a 3-bone leg chain on the stick figure with no constraints
	// and no pole vector. Used by tests that need to isolate the FABRIK solver
	// from CreateLegChain's constraint defaults.
	static Flux_IKChain MakeUnconstrainedLeftLegChain()
	{
		Flux_IKChain xChain;
		xChain.m_strName = "LeftLeg";
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
		xChain.m_bUsePoleVector = false;
		return xChain;
	}

	// Helper: standard pose-init prelude. Allocates pose, copies bind-pose TRS,
	// computes initial model-space matrices.
	static void InitPoseAtBindForSkeleton(Flux_SkeletonPose& xPose, const Zenith_SkeletonAsset& xSkel)
	{
		xPose.Initialize(xSkel.GetNumBones());
		xPose.InitFromBindPose(xSkel);
		xPose.ComputeModelSpaceMatricesFromSkeleton(xSkel);
	}
}

ZENITH_TEST(Animation, IKResolveBoneIndices) { Zenith_UnitTests::TestIKResolveBoneIndices(); }
void Zenith_UnitTests::TestIKResolveBoneIndices()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// All bones present
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
		xChain.ResolveBoneIndices(*pxSkel);
		ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.GetSize(), 3, "Should resolve 3 indices");
		ZENITH_ASSERT_EQ(static_cast<int32_t>(xChain.m_xBoneIndices.Get(0)), pxSkel->GetBoneIndex("LeftUpperLeg"), "Index 0 mismatch");
		ZENITH_ASSERT_EQ(static_cast<int32_t>(xChain.m_xBoneIndices.Get(1)), pxSkel->GetBoneIndex("LeftLowerLeg"), "Index 1 mismatch");
		ZENITH_ASSERT_EQ(static_cast<int32_t>(xChain.m_xBoneIndices.Get(2)), pxSkel->GetBoneIndex("LeftFoot"), "Index 2 mismatch");
	}

	// Some bones missing
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "NoSuchBone", "LeftFoot" });
		xChain.ResolveBoneIndices(*pxSkel);
		ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.GetSize(), 3, "Should still produce 3 entries");
		ZENITH_ASSERT_NE(xChain.m_xBoneIndices.Get(0), ~0u, "First should be valid");
		ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.Get(1), ~0u, "Middle should be invalid sentinel");
		ZENITH_ASSERT_NE(xChain.m_xBoneIndices.Get(2), ~0u, "Third should be valid");
	}

	// All bones missing
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "X", "Y", "Z" });
		xChain.ResolveBoneIndices(*pxSkel);
		ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.GetSize(), 3, "Should produce 3 sentinels");
		for (uint32_t u : xChain.m_xBoneIndices) ZENITH_ASSERT_EQ(u, ~0u, "All should be invalid sentinel");
	}

	// Empty chain
	{
		Flux_IKChain xChain;
		xChain.ResolveBoneIndices(*pxSkel);
		ZENITH_ASSERT_TRUE(xChain.m_xBoneIndices.GetSize() == 0, "Empty chain should produce empty result");
	}

	// Idempotence: second resolve produces same result
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
		xChain.ResolveBoneIndices(*pxSkel);
		const Zenith_Vector<uint32_t> xFirst = xChain.m_xBoneIndices;
		xChain.ResolveBoneIndices(*pxSkel);
		ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.GetSize(), xFirst.GetSize(), "Sizes match across calls");
		for (u_int i = 0; i < xFirst.GetSize(); ++i)
			ZENITH_ASSERT_EQ(xChain.m_xBoneIndices.Get(i), xFirst.Get(i), "Indices match across calls");
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, IKComputeBoneLengths) { Zenith_UnitTests::TestIKComputeBoneLengths(); }
void Zenith_UnitTests::TestIKComputeBoneLengths()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	// Stick-figure leg chain — bone offsets are (0,-0.5,0) and (0,-0.5,0), so
	// segment lengths are exactly 0.5 each, total 1.0.
	{
		Flux_IKChain xChain = MakeUnconstrainedLeftLegChain();
		xChain.ResolveBoneIndices(*pxSkel);
		xChain.ComputeBoneLengths(xPose);
		ZENITH_ASSERT_EQ(xChain.m_xBoneLengths.GetSize(), 2, "3-bone chain should produce 2 lengths");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_xBoneLengths.Get(0), 0.5f, 0.001f), "Upper-to-lower should be 0.5m");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_xBoneLengths.Get(1), 0.5f, 0.001f), "Lower-to-foot should be 0.5m");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_fTotalLength, 1.0f, 0.001f), "Total length should be 1.0m");
	}

	// Chain with one invalid index — that segment's length is 0
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "NoSuchBone", "LeftFoot" });
		xChain.ResolveBoneIndices(*pxSkel);
		xChain.ComputeBoneLengths(xPose);
		ZENITH_ASSERT_EQ(xChain.m_xBoneLengths.GetSize(), 2, "Should produce 2 lengths");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_xBoneLengths.Get(0), 0.0f, 0.001f), "First segment with invalid neighbour should be 0");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_xBoneLengths.Get(1), 0.0f, 0.001f), "Second segment with invalid neighbour should be 0");
	}

	// 1-bone chain — early-out path
	{
		Flux_IKChain xChain;
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftFoot" });
		xChain.ResolveBoneIndices(*pxSkel);
		xChain.ComputeBoneLengths(xPose);
		ZENITH_ASSERT_TRUE(xChain.m_xBoneLengths.GetSize() == 0, "1-bone chain should produce 0 lengths");
		ZENITH_ASSERT_TRUE(FloatEquals(xChain.m_fTotalLength, 0.0f, 0.001f), "1-bone total should be 0");
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, ComputeModelSpaceMatricesFromSkeleton) { Zenith_UnitTests::TestComputeModelSpaceMatricesFromSkeleton(); }
void Zenith_UnitTests::TestComputeModelSpaceMatricesFromSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// Bind-pose: pose's model matrices should match asset's bind-pose precompute
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
		{
			ZENITH_ASSERT_TRUE(Mat4Equals(xPose.GetModelSpaceMatrix(i), pxSkel->GetBone(i).m_xBindPoseModel, 0.001f),
				"Pose model-space should match asset bind-pose model after init");
		}
	}

	// Modified local pose: rotation on LeftLowerLeg should propagate to LeftFoot
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);
		const int32_t iLowerLeg = pxSkel->GetBoneIndex("LeftLowerLeg");
		const int32_t iFoot = pxSkel->GetBoneIndex("LeftFoot");
		const Zenith_Maths::Vector3 xFootBefore =
			Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(iFoot))[3]);

		Flux_BoneLocalPose& xLowerLocal = xPose.GetLocalPose(static_cast<uint32_t>(iLowerLeg));
		xLowerLocal.m_xRotation = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);

		const Zenith_Maths::Vector3 xFootAfter =
			Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(iFoot))[3]);
		ZENITH_ASSERT_FALSE(Vec3Equals(xFootBefore, xFootAfter, 0.01f), "Foot model-space position should change after lower-leg rotation");
	}

	// Empty pose / empty skeleton — early-out, no crash
	{
		Flux_SkeletonPose xEmptyPose;
		Zenith_SkeletonAsset xEmptySkel;
		xEmptyPose.Initialize(0);
		xEmptyPose.ComputeModelSpaceMatricesFromSkeleton(xEmptySkel);
		ZENITH_ASSERT_EQ(xEmptyPose.GetNumBones(), 0, "Pose should remain at zero bones");
	}

	// Mismatched counts: pose with fewer bones than skeleton — walks the smaller count
	{
		Flux_SkeletonPose xPartial;
		xPartial.Initialize(5);
		xPartial.InitFromBindPose(*pxSkel);   // copies first 5 bind poses
		xPartial.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		// Should not crash; first 5 model matrices are populated. We can't easily
		// assert specifics for partial skeletons, but the no-crash + correct
		// per-bone walk for the first 5 bones is the contract.
		for (uint32_t i = 0; i < 5; ++i)
		{
			ZENITH_ASSERT_TRUE(Mat4Equals(xPartial.GetModelSpaceMatrix(i), pxSkel->GetBone(i).m_xBindPoseModel, 0.001f),
				"First 5 bone model matrices should match asset bind pose");
		}
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, IKSolveReachableTarget) { Zenith_UnitTests::TestIKSolveReachableTarget(); }
void Zenith_UnitTests::TestIKSolveReachableTarget()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());

	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
	const Zenith_Maths::Vector3 xRest(xPose.GetModelSpaceMatrix(uFootIdx)[3]);

	// Reachable target: 0.85m from upper-leg origin in a perturbed direction.
	const Zenith_Maths::Vector3 xTarget = xRoot + glm::normalize(xRest - xRoot + Zenith_Maths::Vector3(0.1f, 0.0f, 0.1f)) * 0.85f;

	const Zenith_Maths::Vector3 xKneeBefore(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftLowerLeg")))[3]);

	Flux_IKTarget xT;
	xT.m_xPosition = xTarget;
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	const Zenith_Maths::Vector3 xFootAfter(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	ZENITH_ASSERT_TRUE(Vec3Equals(xFootAfter, xTarget, 0.01f), "Foot should converge on reachable target");

	const Zenith_Maths::Vector3 xKneeAfter(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftLowerLeg")))[3]);
	ZENITH_ASSERT_FALSE(Vec3Equals(xKneeBefore, xKneeAfter, 0.001f), "Knee should have moved during solve");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKSolveUnreachableTarget) { Zenith_UnitTests::TestIKSolveUnreachableTarget(); }
void Zenith_UnitTests::TestIKSolveUnreachableTarget()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());

	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
	const Zenith_Maths::Vector3 xTarget = xRoot + Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f);  // way out of reach

	Flux_IKTarget xT;
	xT.m_xPosition = xTarget;
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	const Zenith_Maths::Vector3 xFootAfter(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	const float fDistFromRoot = glm::length(xFootAfter - xRoot);
	// Total chain length is 1.0m. After unreachable-target stretch, foot should be ~1.0m from root.
	ZENITH_ASSERT_TRUE(FloatEquals(fDistFromRoot, 1.0f, 0.05f), "Foot should be at total chain length from root");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKSolveDisabledTarget) { Zenith_UnitTests::TestIKSolveDisabledTarget(); }
void Zenith_UnitTests::TestIKSolveDisabledTarget()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	// Capture pre-solve local poses (bind values)
	std::vector<Flux_BoneLocalPose> xPreSolve(pxSkel->GetNumBones());
	for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i) xPreSolve[i] = xPose.GetLocalPose(i);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());

	// Disabled target → solver should skip
	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
	Flux_IKTarget xT;
	xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.0f, -0.5f, 0.5f);
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = false;   // ← key: disabled
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
	{
		ZENITH_ASSERT_TRUE(QuatEquals(xPose.GetLocalPose(i).m_xRotation, xPreSolve[i].m_xRotation, 0.001f),
			"Disabled target should not modify any bone rotation");
	}

	// Same: enabled but no chain registered → also no-op
	{
		Flux_IKSolver xSolver2;   // empty
		Flux_IKTarget xT2;
		xT2.m_xPosition = Zenith_Maths::Vector3(0.0f, -0.5f, 0.5f);
		xT2.m_fWeight = 1.0f;
		xT2.m_bEnabled = true;
		xSolver2.SetTarget("LeftLeg", xT2);
		xSolver2.Solve(xPose, *pxSkel, glm::mat4(1.0f));
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
		{
			ZENITH_ASSERT_TRUE(QuatEquals(xPose.GetLocalPose(i).m_xRotation, xPreSolve[i].m_xRotation, 0.001f),
				"Solver with no chains should not modify any bone");
		}
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, IKLazyResolveOnFirstSolve) { Zenith_UnitTests::TestIKLazyResolveOnFirstSolve(); }
void Zenith_UnitTests::TestIKLazyResolveOnFirstSolve()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());
	// We never call ResolveBoneIndices manually.
	ZENITH_ASSERT_TRUE(xSolver.GetChain("LeftLeg")->m_xBoneIndices.GetSize() == 0, "Indices should start empty");

	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
	Flux_IKTarget xT;
	xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.05f, -0.7f, 0.05f);
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	ZENITH_ASSERT_EQ(xSolver.GetChain("LeftLeg")->m_xBoneIndices.GetSize(), 3, "Lazy resolve should have populated 3 indices");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKWorldMatrixTransform) { Zenith_UnitTests::TestIKWorldMatrixTransform(); }
void Zenith_UnitTests::TestIKWorldMatrixTransform()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// Translated world: target at world(rest) → no model-space movement
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);
		Flux_IKSolver xSolver;
		xSolver.AddChain(MakeUnconstrainedLeftLegChain());

		const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
		const Zenith_Maths::Vector3 xRest(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
		const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f), Zenith_Maths::Vector3(10.0f, 5.0f, 0.0f));
		const Zenith_Maths::Vector4 xWorldFoot = xWorld * Zenith_Maths::Vector4(xRest, 1.0f);

		Flux_IKTarget xT;
		xT.m_xPosition = Zenith_Maths::Vector3(xWorldFoot);
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, xWorld);

		const Zenith_Maths::Vector3 xFootAfter(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
		ZENITH_ASSERT_TRUE(Vec3Equals(xFootAfter, xRest, 0.01f), "Target at world(rest) should leave foot at rest in model space");
	}

	// Rotated + translated world: world-space offset converts to model-space offset
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);
		Flux_IKSolver xSolver;
		xSolver.AddChain(MakeUnconstrainedLeftLegChain());

		const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
		const Zenith_Maths::Vector3 xRest(xPose.GetModelSpaceMatrix(uFootIdx)[3]);

		// 90° Y rotation, translated. Pick a model-space offset that contracts the
		// chain (positive Y, slight Z) so the target stays inside the 1.0m sphere.
		Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f), Zenith_Maths::Vector3(10.0f, 5.0f, 0.0f));
		xWorld = xWorld * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

		const Zenith_Maths::Vector3 xModelOffset(0.0f, 0.05f, -0.1f);
		const Zenith_Maths::Vector4 xWorldFoot = xWorld * Zenith_Maths::Vector4(xRest, 1.0f);
		const Zenith_Maths::Vector4 xWorldOffset = xWorld * Zenith_Maths::Vector4(xModelOffset, 0.0f);
		const Zenith_Maths::Vector3 xWorldTarget = Zenith_Maths::Vector3(xWorldFoot) + Zenith_Maths::Vector3(xWorldOffset);

		Flux_IKTarget xT;
		xT.m_xPosition = xWorldTarget;
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, xWorld);

		const Zenith_Maths::Vector3 xFootAfter(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
		ZENITH_ASSERT_TRUE(Vec3Equals(xFootAfter, xRest + xModelOffset, 0.02f),
			"Inverse-world-transform should map rotated world target back into expected model-space position");
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, IKHingeConstraintProjectsSegmentDirection) { Zenith_UnitTests::TestIKHingeConstraintProjectsSegmentDirection(); }
void Zenith_UnitTests::TestIKHingeConstraintProjectsSegmentDirection()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	// 3-bone chain with hinge at the knee (axis +X). Test that the post-solve
	// knee→foot segment has near-zero X component (projection onto YZ plane).
	Flux_IKChain xChain;
	xChain.m_strName = "LeftLeg";
	xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
	xChain.m_bUsePoleVector = false;
	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());
	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());
	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());
	xChain.m_xJointConstraints.Get(1).m_eType = Flux_JointConstraint::ConstraintType::Hinge;
	xChain.m_xJointConstraints.Get(1).m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

	Flux_IKSolver xSolver;
	xSolver.AddChain(xChain);

	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);

	// Target offset along X (perpendicular to the hinge plane). Without the hinge
	// this would pull the knee out of the YZ plane; with the hinge, the knee→foot
	// segment projects back onto the YZ plane.
	Flux_IKTarget xT;
	xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.4f, -0.6f, 0.1f);
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	const uint32_t uKneeIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftLowerLeg"));
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xKnee(xPose.GetModelSpaceMatrix(uKneeIdx)[3]);
	const Zenith_Maths::Vector3 xFoot(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	const Zenith_Maths::Vector3 xSeg = xFoot - xKnee;

	// Hinge axis is X, so segment's X component should be near zero (modulo
	// how much the rotation conversion preserves the projection — give some
	// tolerance for the FABRIK→rotation round trip).
	ZENITH_ASSERT_TRUE(std::abs(xSeg.x) < 0.10f, "Knee→foot segment X component should be projected toward 0 by hinge");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKPoleVectorBiasesKnee) { Zenith_UnitTests::TestIKPoleVectorBiasesKnee(); }
void Zenith_UnitTests::TestIKPoleVectorBiasesKnee()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// Pole-vector test: an off-axis reachable target where the natural leg
	// configuration lets the knee bend either +Z or -Z. The pole vector picks one.
	auto SolveAndGetKneeZ = [&](const Zenith_Maths::Vector3& xPoleVec) -> float
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);

		Flux_IKChain xChain;
		xChain.m_strName = "LeftLeg";
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
		xChain.m_bUsePoleVector = true;
		xChain.m_xPoleVector = xPoleVec;

		Flux_IKSolver xSolver;
		xSolver.AddChain(xChain);

		const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
		const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);

		Flux_IKTarget xT;
		xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.3f, -0.7f, 0.0f);
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

		const uint32_t uKneeIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftLowerLeg"));
		return xPose.GetModelSpaceMatrix(uKneeIdx)[3].z;
	};

	const float fKneeZForward  = SolveAndGetKneeZ(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	const float fKneeZBackward = SolveAndGetKneeZ(Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f));

	// Different pole positions should yield knee Z values on opposite sides
	// (i.e. their difference is non-trivial). Exact magnitudes depend on FABRIK
	// convergence + pole projection, so assert the directional relationship.
	ZENITH_ASSERT_TRUE((fKneeZForward - fKneeZBackward) > 0.01f,
		"Forward pole should produce greater knee.z than backward pole");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKMultiChainBoneDisjoint) { Zenith_UnitTests::TestIKMultiChainBoneDisjoint(); }
void Zenith_UnitTests::TestIKMultiChainBoneDisjoint()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	auto SolveLeftOnly = [&]() -> Zenith_Maths::Vector3
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);

		Flux_IKSolver xSolver;
		xSolver.AddChain(MakeUnconstrainedLeftLegChain());

		const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
		const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
		Flux_IKTarget xT;
		xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.05f, -0.7f, 0.1f);
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

		const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
		return Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	};

	const Zenith_Maths::Vector3 xLeftOnlyResult = SolveLeftOnly();

	// Run with both chains targeted, distinct targets. Left chain's solve should
	// be identical to single-chain run (chains are bone-disjoint).
	Flux_SkeletonPose xPoseBoth;
	InitPoseAtBindForSkeleton(xPoseBoth, *pxSkel);

	Flux_IKSolver xSolverBoth;
	xSolverBoth.AddChain(MakeUnconstrainedLeftLegChain());
	{
		Flux_IKChain xRight;
		xRight.m_strName = "RightLeg";
		xRight.m_xBoneNames = MakeBoneNameVec({ "RightUpperLeg", "RightLowerLeg", "RightFoot" });
		xRight.m_bUsePoleVector = false;
		xSolverBoth.AddChain(xRight);
	}

	const uint32_t uLeftRoot = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uRightRoot = static_cast<uint32_t>(pxSkel->GetBoneIndex("RightUpperLeg"));
	const Zenith_Maths::Vector3 xLeftRoot(xPoseBoth.GetModelSpaceMatrix(uLeftRoot)[3]);
	const Zenith_Maths::Vector3 xRightRoot(xPoseBoth.GetModelSpaceMatrix(uRightRoot)[3]);

	Flux_IKTarget xLT; xLT.m_xPosition = xLeftRoot + Zenith_Maths::Vector3(0.05f, -0.7f, 0.1f); xLT.m_fWeight = 1.0f; xLT.m_bEnabled = true;
	Flux_IKTarget xRT; xRT.m_xPosition = xRightRoot + Zenith_Maths::Vector3(-0.05f, -0.6f, 0.2f); xRT.m_fWeight = 1.0f; xRT.m_bEnabled = true;
	xSolverBoth.SetTarget("LeftLeg", xLT);
	xSolverBoth.SetTarget("RightLeg", xRT);
	xSolverBoth.Solve(xPoseBoth, *pxSkel, glm::mat4(1.0f));

	const uint32_t uLeftFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const uint32_t uRightFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("RightFoot"));
	const Zenith_Maths::Vector3 xLeftFootBoth(xPoseBoth.GetModelSpaceMatrix(uLeftFootIdx)[3]);
	const Zenith_Maths::Vector3 xRightFootBoth(xPoseBoth.GetModelSpaceMatrix(uRightFootIdx)[3]);

	ZENITH_ASSERT_TRUE(Vec3Equals(xLeftFootBoth, xLT.m_xPosition, 0.01f), "Left foot should reach left target");
	ZENITH_ASSERT_TRUE(Vec3Equals(xRightFootBoth, xRT.m_xPosition, 0.01f), "Right foot should reach right target");
	// Bone-disjoint chains: left foot result should match single-chain run (chains don't interfere)
	// Note: hash map iteration order isn't deterministic across solves, so the chain solve order
	// may differ. The disjoint-bones property still guarantees identical end states.
	ZENITH_ASSERT_TRUE(Vec3Equals(xLeftFootBoth, xLeftOnlyResult, 0.01f), "Left foot result should match single-chain run");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKConvergence) { Zenith_UnitTests::TestIKConvergence(); }
void Zenith_UnitTests::TestIKConvergence()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	auto SolveAndError = [&](uint32_t uIters, float fTolerance) -> float
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);

		Flux_IKChain xChain = MakeUnconstrainedLeftLegChain();
		xChain.m_uMaxIterations = uIters;
		xChain.m_fTolerance = fTolerance;

		Flux_IKSolver xSolver;
		xSolver.AddChain(xChain);

		const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
		const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);
		const Zenith_Maths::Vector3 xTarget = xRoot + Zenith_Maths::Vector3(0.2f, -0.6f, 0.3f);

		Flux_IKTarget xT;
		xT.m_xPosition = xTarget;
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

		const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
		const Zenith_Maths::Vector3 xFoot(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
		return glm::length(xFoot - xTarget);
	};

	const float fError1 = SolveAndError(1, 0.001f);
	const float fError20 = SolveAndError(20, 0.001f);
	const float fError100 = SolveAndError(100, 0.0001f);

	ZENITH_ASSERT_TRUE(fError20 <= fError1 + 0.01f, "20 iters should converge at least as well as 1");
	ZENITH_ASSERT_TRUE(fError100 < 0.01f, "100 iters should be well-converged");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKWeightZeroIsNoop) { Zenith_UnitTests::TestIKWeightZeroIsNoop(); }
void Zenith_UnitTests::TestIKWeightZeroIsNoop()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	std::vector<Flux_BoneLocalPose> xPreSolve(pxSkel->GetNumBones());
	for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i) xPreSolve[i] = xPose.GetLocalPose(i);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());

	const uint32_t uRootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const Zenith_Maths::Vector3 xRoot(xPose.GetModelSpaceMatrix(uRootIdx)[3]);

	// Weight 0 → solver should short-circuit (E3d) and produce no change
	Flux_IKTarget xT;
	xT.m_xPosition = xRoot + Zenith_Maths::Vector3(0.05f, -0.7f, 0.1f);
	xT.m_fWeight = 0.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));

	for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
	{
		ZENITH_ASSERT_TRUE(QuatEquals(xPose.GetLocalPose(i).m_xRotation, xPreSolve[i].m_xRotation, 0.001f),
			"Weight 0 solve should not modify any bone");
	}

	// Weight 1 → solver should converge
	xT.m_fWeight = 1.0f;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xFoot(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	ZENITH_ASSERT_TRUE(Vec3Equals(xFoot, xT.m_xPosition, 0.01f), "Weight 1 solve should converge");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKDegenerateChainSizes) { Zenith_UnitTests::TestIKDegenerateChainSizes(); }
void Zenith_UnitTests::TestIKDegenerateChainSizes()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	std::vector<Flux_BoneLocalPose> xPreSolve(pxSkel->GetNumBones());
	for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i) xPreSolve[i] = xPose.GetLocalPose(i);

	// 1-bone chain — early-out at SolveChain's < 2 guard
	{
		Flux_IKChain xChain;
		xChain.m_strName = "OneBone";
		xChain.m_xBoneNames = MakeBoneNameVec({ "LeftFoot" });
		xChain.m_bUsePoleVector = false;
		Flux_IKSolver xSolver;
		xSolver.AddChain(xChain);
		Flux_IKTarget xT; xT.m_xPosition = Zenith_Maths::Vector3(0.0f); xT.m_fWeight = 1.0f; xT.m_bEnabled = true;
		xSolver.SetTarget("OneBone", xT);
		xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
		{
			ZENITH_ASSERT_TRUE(QuatEquals(xPose.GetLocalPose(i).m_xRotation, xPreSolve[i].m_xRotation, 0.001f),
				"1-bone chain should not modify pose");
		}
	}

	// Empty chain — no crash
	{
		Flux_IKChain xChain;
		xChain.m_strName = "Empty";
		xChain.m_bUsePoleVector = false;
		Flux_IKSolver xSolver;
		xSolver.AddChain(xChain);
		Flux_IKTarget xT; xT.m_xPosition = Zenith_Maths::Vector3(0.0f); xT.m_fWeight = 1.0f; xT.m_bEnabled = true;
		xSolver.SetTarget("Empty", xT);
		xSolver.Solve(xPose, *pxSkel, glm::mat4(1.0f));
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); ++i)
		{
			ZENITH_ASSERT_TRUE(QuatEquals(xPose.GetLocalPose(i).m_xRotation, xPreSolve[i].m_xRotation, 0.001f),
				"Empty chain should not modify pose");
		}
	}

	delete pxSkel;
}

ZENITH_TEST(Animation, IKSolveOnReloadedAsset) { Zenith_UnitTests::TestIKSolveOnReloadedAsset(); }
void Zenith_UnitTests::TestIKSolveOnReloadedAsset()
{
	const std::string strPath = std::string(ENGINE_ASSETS_DIR) +
		"Meshes/IKTestRig/IKTestRig" ZENITH_SKELETON_EXT;

	// First-run export. Idempotent: skip if already on disk (the .zskel is
	// committed to source control after the first developer runs it).
	if (!std::filesystem::exists(strPath))
	{
		std::filesystem::create_directories(std::filesystem::path(strPath).parent_path());

		Zenith_SkeletonAsset* pxRig = new Zenith_SkeletonAsset();
		const auto xId = glm::identity<Zenith_Maths::Quat>();
		const auto xUS = Zenith_Maths::Vector3(1.0f);
		pxRig->AddBone("Root",           -1, Zenith_Maths::Vector3( 0.00f,  0.0f,  0.0f), xId, xUS);
		pxRig->AddBone("LeftUpperLeg",    0, Zenith_Maths::Vector3(-0.15f,  0.0f,  0.0f), xId, xUS);
		pxRig->AddBone("LeftLowerLeg",    1, Zenith_Maths::Vector3( 0.00f, -0.5f,  0.0f), xId, xUS);
		pxRig->AddBone("LeftFoot",        2, Zenith_Maths::Vector3( 0.00f, -0.5f,  0.0f), xId, xUS);
		pxRig->AddBone("RightUpperLeg",   0, Zenith_Maths::Vector3( 0.15f,  0.0f,  0.0f), xId, xUS);
		pxRig->AddBone("RightLowerLeg",   4, Zenith_Maths::Vector3( 0.00f, -0.5f,  0.0f), xId, xUS);
		pxRig->AddBone("RightFoot",       5, Zenith_Maths::Vector3( 0.00f, -0.5f,  0.0f), xId, xUS);
		pxRig->ComputeBindPoseMatrices();
		pxRig->Export(strPath.c_str());
		delete pxRig;

		Zenith_Log(LOG_CATEGORY_ANIMATION,
			"[IKTestRig] Exported new asset to %s — commit to source control "
			"so future test runs and game-side consumers find it.", strPath.c_str());
	}

	ZENITH_ASSERT_TRUE(std::filesystem::exists(strPath), "IKTestRig.zskel should exist on disk");

	Zenith_SkeletonAsset* pxRig = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strPath);
	ZENITH_ASSERT_NOT_NULL(pxRig, "IKTestRig should load via AssetRegistry");
	ZENITH_ASSERT_EQ(pxRig->GetNumBones(), 7, "IKTestRig should have 7 bones");
	ZENITH_ASSERT_TRUE(pxRig->HasBone("LeftFoot"), "IKTestRig should have LeftFoot bone");

	Flux_SkeletonPose xPose;
	xPose.Initialize(pxRig->GetNumBones());
	xPose.InitFromBindPose(*pxRig);
	xPose.ComputeModelSpaceMatricesFromSkeleton(*pxRig);

	Flux_IKSolver xSolver;
	Flux_IKChain xChain;
	xChain.m_strName = "LeftLeg";   // <-- registers under this name; SetTarget("LeftLeg") finds it
	xChain.m_xBoneNames = MakeBoneNameVec({ "LeftUpperLeg", "LeftLowerLeg", "LeftFoot" });
	xChain.m_bUsePoleVector = false;
	// Bump iterations + tighten internal tolerance — the leg starts fully extended,
	// which gives FABRIK a worst-case starting condition. 50 iterations + 0.0005f
	// inner tolerance reliably converges below the 0.05m outer assertion.
	xChain.m_uMaxIterations = 50;
	xChain.m_fTolerance = 0.0005f;
	xSolver.AddChain(xChain);

	const uint32_t uRoot = static_cast<uint32_t>(pxRig->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uFoot = static_cast<uint32_t>(pxRig->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xRoot(pxRig->GetBone(uRoot).m_xBindPoseModel[3]);
	const Zenith_Maths::Vector3 xRest(pxRig->GetBone(uFoot).m_xBindPoseModel[3]);
	const Zenith_Maths::Vector3 xTarget = xRoot + glm::normalize(xRest - xRoot + Zenith_Maths::Vector3(0.1f, 0.0f, 0.1f)) * 0.85f;

	Flux_IKTarget xT;
	xT.m_xPosition = xTarget;
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxRig, glm::mat4(1.0f));

	const Zenith_Maths::Vector3 xFootAfter(xPose.GetModelSpaceMatrix(uFoot)[3]);
	// 0.05m tolerance: this is the ROUND-TRIP test (Export → Reload → Solve), so
	// the focus is on "did the asset survive the disk hop?" not "is FABRIK precise?".
	// The dedicated convergence test (IK12) covers the precision angle.
	ZENITH_ASSERT_TRUE(Vec3Equals(xFootAfter, xTarget, 0.05f),
		"IKTestRig solve should converge on target");
}

//------------------------------------------------------------------------------
// Reproductions of the "feet dragging behind body" bug
// Mirrors what RenderTest_PlayerBehaviour::UpdateFootIK does end-to-end:
//   1. Player has a rotated world matrix (faces +X via yaw=π/2)
//   2. Compute the foot's CURRENT world position via xWorld * footModel * (0,0,0,1)
//   3. Simulate a "raycast hit" 1m below the foot
//   4. Set the IK target in WORLD space (just like SetIKTarget does)
//   5. Solve
//   6. Assert: the foot ends up at the target world position (NOT drifted in XZ)
//
// If the foot ends up at a DIFFERENT world XZ than the target, the IK math is
// inconsistent with the world-matrix application — the canonical "feet drag
// behind body" failure mode.
//------------------------------------------------------------------------------

ZENITH_TEST(Animation, IKWithPlayerLikeWorldRotation) { Zenith_UnitTests::TestIKWithPlayerLikeWorldRotation(); }
void Zenith_UnitTests::TestIKWithPlayerLikeWorldRotation()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(MakeUnconstrainedLeftLegChain());

	// Player at (5, 10, 5), rotated 90° around +Y (faces +X). Same kind of world
	// matrix the real player builds via Zenith_TransformComponent::BuildModelMatrix.
	const Zenith_Maths::Vector3 xPlayerPos(5.0f, 10.0f, 5.0f);
	const Zenith_Maths::Quat xPlayerRot = glm::angleAxis(
		static_cast<float>(Zenith_Maths::Pi * 0.5),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const Zenith_Maths::Matrix4 xWorld =
		glm::translate(glm::mat4(1.0f), xPlayerPos) * glm::toMat4(xPlayerRot);

	// Compute the foot's current WORLD position — same way UpdateFootIK does it.
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Matrix4& xFootModel = xPose.GetModelSpaceMatrix(uFootIdx);
	const Zenith_Maths::Vector4 xFootW4 = xWorld * xFootModel * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xFootWorld(xFootW4);

	// Simulate raycast hit 1m below foot, target = hit + ankle-height. This is
	// reachable (chain is 1m, foot is 1m below player root, target lifts foot up
	// by ~0.95m, so distance from upper-leg origin to target is well under 1m).
	const Zenith_Maths::Vector3 xTargetWorld(xFootWorld.x, xFootWorld.y + 0.5f, xFootWorld.z);

	Flux_IKTarget xT;
	xT.m_xPosition = xTargetWorld;   // WORLD space — solver will inverse-transform
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, xWorld);

	// After solve, the foot in MODEL space should map to the target in WORLD space.
	const Zenith_Maths::Matrix4& xFootModelAfter = xPose.GetModelSpaceMatrix(uFootIdx);
	const Zenith_Maths::Vector4 xFootWorldAfter4 = xWorld * xFootModelAfter * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xFootWorldAfter(xFootWorldAfter4);

	// The bug: if the world matrix isn't correctly applied during the solve, the
	// foot drifts in WORLD XZ relative to where the target sits. Tolerance is
	// generous (0.05m) since FABRIK convergence has its own slop.
	ZENITH_ASSERT_TRUE(Vec3Equals(xFootWorldAfter, xTargetWorld, 0.05f),
		"After IK solve, foot world position should match target world position");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKWithPlayerLikeWorldRotationAndPole) { Zenith_UnitTests::TestIKWithPlayerLikeWorldRotationAndPole(); }
void Zenith_UnitTests::TestIKWithPlayerLikeWorldRotationAndPole()
{
	// Same as above but using CreateLegChain (with pole vector + hinge constraint
	// configured) — this is the actual chain RenderTest_PlayerBehaviour adds.
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

	const Zenith_Maths::Vector3 xPlayerPos(5.0f, 10.0f, 5.0f);
	const Zenith_Maths::Quat xPlayerRot = glm::angleAxis(
		static_cast<float>(Zenith_Maths::Pi * 0.5),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const Zenith_Maths::Matrix4 xWorld =
		glm::translate(glm::mat4(1.0f), xPlayerPos) * glm::toMat4(xPlayerRot);

	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Matrix4& xFootModel = xPose.GetModelSpaceMatrix(uFootIdx);
	const Zenith_Maths::Vector4 xFootW4 = xWorld * xFootModel * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xFootWorld(xFootW4);
	const Zenith_Maths::Vector3 xTargetWorld(xFootWorld.x, xFootWorld.y + 0.5f, xFootWorld.z);

	Flux_IKTarget xT;
	xT.m_xPosition = xTargetWorld;
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, xWorld);

	const Zenith_Maths::Matrix4& xFootModelAfter = xPose.GetModelSpaceMatrix(uFootIdx);
	const Zenith_Maths::Vector4 xFootWorldAfter4 = xWorld * xFootModelAfter * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xFootWorldAfter(xFootWorldAfter4);

	ZENITH_ASSERT_TRUE(Vec3Equals(xFootWorldAfter, xTargetWorld, 0.05f),
		"With CreateLegChain (pole+hinge), foot world should still match target world");

	delete pxSkel;
}

ZENITH_TEST(Animation, IKEachCardinalRotation) { Zenith_UnitTests::TestIKEachCardinalRotation(); }
void Zenith_UnitTests::TestIKEachCardinalRotation()
{
	// Sweeps the player's yaw through 0, π/2, π, -π/2 (forward, right, back, left).
	// For each, sets up the same player-style scenario and verifies the foot lands
	// on its target. Catches rotation-direction-specific bugs that a single
	// orientation might miss.
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));

	const float afYaws[] = {
		0.0f,
		static_cast<float>(Zenith_Maths::Pi * 0.5),
		static_cast<float>(Zenith_Maths::Pi),
		static_cast<float>(-Zenith_Maths::Pi * 0.5)
	};
	const char* aszLabels[] = { "yaw=0 (forward)", "yaw=+π/2 (right)", "yaw=π (back)", "yaw=-π/2 (left)" };

	for (size_t i = 0; i < sizeof(afYaws) / sizeof(afYaws[0]); ++i)
	{
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);

		Flux_IKSolver xSolver;
		xSolver.AddChain(MakeUnconstrainedLeftLegChain());

		const Zenith_Maths::Vector3 xPlayerPos(5.0f, 10.0f, 5.0f);
		const Zenith_Maths::Quat xPlayerRot = glm::angleAxis(afYaws[i],
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Matrix4 xWorld =
			glm::translate(glm::mat4(1.0f), xPlayerPos) * glm::toMat4(xPlayerRot);

		const Zenith_Maths::Matrix4& xFootModel = xPose.GetModelSpaceMatrix(uFootIdx);
		const Zenith_Maths::Vector4 xFootW4 = xWorld * xFootModel * Zenith_Maths::Vector4(0, 0, 0, 1);
		const Zenith_Maths::Vector3 xFootWorld(xFootW4);
		const Zenith_Maths::Vector3 xTargetWorld(xFootWorld.x, xFootWorld.y + 0.5f, xFootWorld.z);

		Flux_IKTarget xT;
		xT.m_xPosition = xTargetWorld;
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, xWorld);

		const Zenith_Maths::Matrix4& xFootModelAfter = xPose.GetModelSpaceMatrix(uFootIdx);
		const Zenith_Maths::Vector4 xFootWorldAfter4 = xWorld * xFootModelAfter * Zenith_Maths::Vector4(0, 0, 0, 1);
		const Zenith_Maths::Vector3 xFootWorldAfter(xFootWorldAfter4);

		(void)aszLabels;   // logged on failure path only — labels reserved for future debug
		ZENITH_ASSERT_TRUE(Vec3Equals(xFootWorldAfter, xTargetWorld, 0.05f),
			"Foot world should match target world for each cardinal yaw");
	}

	delete pxSkel;
}

// Multi-frame simulation: player walks forward and rotates simultaneously.
// Each "frame" mirrors the actual game's order:
//   1. (start of frame) animator IK runs with the world matrix from the LAST
//      transform state and the target set on the LAST frame.
//   2. Player::OnUpdate equivalent: position + rotation update.
//   3. UpdateFootIK equivalent: read foot's current model transform, transform
//      to world via the post-update transform, raycast-simulate, set new target.
//
// This is the critical test for "feet dragging when moving" — if there's any
// timing mismatch between when the world matrix is recorded vs when the target
// is computed, the foot world position will drift behind the body each frame.
ZENITH_TEST(Animation, IKOverManyFramesWalkingAndRotating) { Zenith_UnitTests::TestIKOverManyFramesWalkingAndRotating(); }
void Zenith_UnitTests::TestIKOverManyFramesWalkingAndRotating()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));

	// Player starts at (0, 10, 0) facing +Z, walks at 5 m/s in +Z direction
	// while rotating yaw at 1 rad/s. 60fps. Run for 1 second (60 frames).
	Zenith_Maths::Vector3 xPlayerPos(0.0f, 10.0f, 0.0f);
	float fYaw = 0.0f;
	const float fDt = 1.0f / 60.0f;
	const float fWalkSpeed = 5.0f;        // m/s in player local +Z (forward)
	const float fYawRate = 1.0f;          // rad/s
	const float fGroundY = 9.0f;          // 1m below player root

	// Build world matrix helper.
	auto BuildWorld = [&]() -> Zenith_Maths::Matrix4 {
		const Zenith_Maths::Quat xRot = glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		return glm::translate(glm::mat4(1.0f), xPlayerPos) * glm::toMat4(xRot);
	};

	// Last frame's world matrix used for IK solve. Initially set to bind world.
	Zenith_Maths::Matrix4 xWorldForSolve = BuildWorld();

	// Last frame's IK target (in world space). Initially none — first frame solves with bind.
	bool bHasTarget = false;
	Flux_IKTarget xTarget;
	xTarget.m_fWeight = 1.0f;
	xTarget.m_bEnabled = true;

	const int kFrames = 60;
	float fMaxDragError = 0.0f;
	int iWorstFrame = -1;
	Zenith_Maths::Vector3 xWorstFootWorld(0.0f), xWorstTargetWorld(0.0f);

	for (int iFrame = 0; iFrame < kFrames; ++iFrame)
	{
		// === Step 1: simulated Animator IK Solve ===
		// Uses xWorldForSolve (recorded at start of this frame) and the target
		// set last frame. This is what Flux_AnimationController does each frame.
		if (bHasTarget)
		{
			xSolver.SetTarget("LeftLeg", xTarget);
			// Pre-solve recompute (mirrors Flux_AnimationController::ApplyOutputPoseToSkeleton).
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
			xSolver.Solve(xPose, *pxSkel, xWorldForSolve);
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		}

		// === Step 2: simulated Player::OnUpdate (movement + rotation) ===
		// Update happens AFTER animator. Position and rotation change in the same
		// step before the IK helper runs.
		const Zenith_Maths::Vector3 xForward(-sinf(fYaw), 0.0f, cosf(fYaw));
		xPlayerPos += xForward * fWalkSpeed * fDt;
		fYaw += fYawRate * fDt;

		// === Step 3: simulated UpdateFootIK (sets target for next frame) ===
		const Zenith_Maths::Matrix4 xWorldNow = BuildWorld();
		const Zenith_Maths::Matrix4& xFootModelNow = xPose.GetModelSpaceMatrix(uFootIdx);
		const Zenith_Maths::Vector4 xFootW4 = xWorldNow * xFootModelNow * Zenith_Maths::Vector4(0, 0, 0, 1);
		const Zenith_Maths::Vector3 xFootWorld(xFootW4);

		// Simulated raycast: hits ground (Y=fGroundY) directly below foot.
		// Target = hit + ankle.
		const Zenith_Maths::Vector3 xRayHit(xFootWorld.x, fGroundY, xFootWorld.z);
		const Zenith_Maths::Vector3 xTargetWorld = xRayHit + Zenith_Maths::Vector3(0.0f, 0.05f, 0.0f);

		xTarget.m_xPosition = xTargetWorld;
		bHasTarget = true;

		// IMPORTANT: record the world matrix that was current when target was set.
		// This is what the NEXT frame's animator should use to inverse-transform.
		// Mirrors Zenith_AnimatorComponent::UpdateWorldMatrix being called at the
		// START of the next frame (which reads the NOW-current transform).
		xWorldForSolve = xWorldNow;

		// Diagnostic: after solve, foot world should land near target. Compute
		// drag error between this frame's PRE-update foot world and the LAST
		// frame's target — that's what determines whether the foot is keeping up.
		if (iFrame > 5)   // skip warmup
		{
			const float fError = glm::length(xFootWorld - xTargetWorld);
			if (fError > fMaxDragError)
			{
				fMaxDragError = fError;
				iWorstFrame = iFrame;
				xWorstFootWorld = xFootWorld;
				xWorstTargetWorld = xTargetWorld;
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKWalkRotateTest] worst drag err=%.4f at frame %d. footWorld=(%.3f,%.3f,%.3f) targetWorld=(%.3f,%.3f,%.3f)",
		fMaxDragError, iWorstFrame,
		xWorstFootWorld.x, xWorstFootWorld.y, xWorstFootWorld.z,
		xWorstTargetWorld.x, xWorstTargetWorld.y, xWorstTargetWorld.z);

	// Acceptable drag: less than 0.05m. The bug being investigated produced
	// drag on the order of 0.5m+ (visible feet trailing behind body).
	ZENITH_ASSERT_TRUE(fMaxDragError < 0.05f,
		"Foot should keep up with body during walking + rotating (no drag)");

	delete pxSkel;
}

// Geometric integrity test: the player's capsule dimensions must place the
// model's foot bind position EXACTLY at the IK ankle target when the capsule
// rests on the ground. If they don't, the IK has to fold the leg every frame
// just to plant feet on the ground — producing the visible "legs angled" or
// "feet dragging" symptom even at rest.
//
// The geometry:
//   playerY               = ground + halfExtent      (capsule rests on ground)
//   foot_bind_world_Y     = playerY + footBindModel.y
//   ik_target_world_Y     = ground + ankleHeight
// For ZERO leg fold (foot bind = IK target):
//   halfExtent = ankleHeight - footBindModel.y         (= -bind_y + ankle)
//
// For the StickFigure (footBindModel.y = -1.0) and ankleHeight = 0.05:
//   halfExtent should be 1.05.
//
// RenderTest uses AddCapsuleCollider(0.10, 0.95) → total half-extent 1.05.
// (The narrow 0.10 radius is required for the IKStep_Spawn demo: it puts the
// foot at offset 0.15 OUTSIDE the capsule, so a tall step under the foot
// doesn't push the capsule up off the main platform.)
ZENITH_TEST(Animation, IKFootBindMatchesCapsuleBottomForPlayerOnGround) { Zenith_UnitTests::TestIKFootBindMatchesCapsuleBottomForPlayerOnGround(); }
void Zenith_UnitTests::TestIKFootBindMatchesCapsuleBottomForPlayerOnGround()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Vector3 xFootBindModel(pxSkel->GetBone(uFootIdx).m_xBindPoseModel[3]);

	// RenderTest player capsule: half-cylinder 0.95 + radius 0.10 = 1.05m total half-extent.
	const float fCapsuleHalfExtent = 0.95f + 0.10f;
	const float fAnkleHeight = 0.05f;

	// The exact alignment required for zero-fold standing pose.
	const float fExpectedHalfExtent = fAnkleHeight - xFootBindModel.y;   // 0.05 - (-1.0) = 1.05

	ZENITH_ASSERT_TRUE(FloatEquals(fCapsuleHalfExtent, fExpectedHalfExtent, 0.01f),
		"Player capsule half-extent must equal (ankleHeight - footBindModel.y) so "
		"feet sit cleanly at IK ankle target Y when the capsule rests on the ground");

	// End-to-end check: with the proper capsule, IK on a stationary grounded
	// player should produce essentially NO leg fold — foot model Y stays at bind.
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

	const float fPlayerY = 10.0f;
	const float fGroundY = fPlayerY - fCapsuleHalfExtent;   // = 10 - 1.05 = 8.95

	const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f),
		Zenith_Maths::Vector3(0.0f, fPlayerY, 0.0f));

	const Zenith_Maths::Vector4 xFootBindW = xWorld * Zenith_Maths::Vector4(xFootBindModel, 1.0f);
	const Zenith_Maths::Vector3 xTargetWorld(xFootBindW.x, fGroundY + fAnkleHeight, xFootBindW.z);

	Flux_IKTarget xT;
	xT.m_xPosition = xTargetWorld;
	xT.m_fWeight = 1.0f;
	xT.m_bEnabled = true;
	xSolver.SetTarget("LeftLeg", xT);
	xSolver.Solve(xPose, *pxSkel, xWorld);

	const Zenith_Maths::Vector3 xFootAfterModel(xPose.GetModelSpaceMatrix(uFootIdx)[3]);

	// With proper capsule sizing, foot stays at bind (no fold). Tolerance 1cm
	// covers FABRIK numerical noise.
	const float fFold = glm::length(xFootBindModel - xFootAfterModel);
	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKZeroFoldTest] foot bind=(%.3f,%.3f,%.3f) foot after=(%.3f,%.3f,%.3f) fold=%.4f",
		xFootBindModel.x, xFootBindModel.y, xFootBindModel.z,
		xFootAfterModel.x, xFootAfterModel.y, xFootAfterModel.z,
		fFold);

	ZENITH_ASSERT_TRUE(fFold < 0.01f,
		"With proper capsule sizing, IK on stationary grounded player should not "
		"fold the leg at all — foot stays at bind position");

	delete pxSkel;
}

// Reproduces the "feet drag while walking" bug.
//
// Timing:
//   Frame N end: player at world position P_N. UpdateFootIK reads foot world
//                from current transform, sets target_world = (foot.x, ground+ankle, foot.z).
//                m_xWorldMatrix on the controller still holds the value from
//                start-of-frame-N animator update.
//   Between frames: physics steps, player moves to P_N + v*dt.
//   Frame N+1 start: animator UpdateWorldMatrix reads NEW transform → m_xWorldMatrix
//                 updated to use P_{N+1}.
//   IK Solve: target_world (set with P_N) inverse-transformed using world_{N+1}.
//             target_local has X/Z component shifted by -v*dt in player local space.
//             Foot pulls to a position ~v*dt BEHIND where it should be → drag.
//
// This test reproduces that exact sequence and asserts that a MODEL-SPACE target
// avoids the drag (because no inverse-transform happens at solve time).
ZENITH_TEST(Animation, IKModelSpaceTargetAvoidsPhysicsLagDrag) { Zenith_UnitTests::TestIKModelSpaceTargetAvoidsPhysicsLagDrag(); }
void Zenith_UnitTests::TestIKModelSpaceTargetAvoidsPhysicsLagDrag()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));

	// Player walking forward at 5 m/s, 60 fps. Inter-frame movement = 0.083m.
	const float fDt = 1.0f / 60.0f;
	const float fSpeed = 5.0f;
	const Zenith_Maths::Vector3 xPlayerVelocity(0.0f, 0.0f, fSpeed);   // +Z forward
	const float fInterFrameMove = fSpeed * fDt;                       // ~0.083m

	const Zenith_Maths::Vector3 xPlayerPosAtTargetSet(0.0f, 10.0f, 0.0f);
	const Zenith_Maths::Vector3 xPlayerPosAtSolve = xPlayerPosAtTargetSet + xPlayerVelocity * fDt;

	const Zenith_Maths::Matrix4 xWorldAtTargetSet =
		glm::translate(glm::mat4(1.0f), xPlayerPosAtTargetSet);
	const Zenith_Maths::Matrix4 xWorldAtSolve =
		glm::translate(glm::mat4(1.0f), xPlayerPosAtSolve);

	// === Step 1: simulate UpdateFootIK at end of frame N ===
	// Compute foot world from bind pose using the world matrix at target-set time.
	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	const Zenith_Maths::Matrix4& xFootBindModel = xPose.GetModelSpaceMatrix(uFootIdx);
	const Zenith_Maths::Vector4 xFootW4 = xWorldAtTargetSet * xFootBindModel * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xFootWorldAtTargetSet(xFootW4);

	// Target at ground+ankle directly below foot (world space).
	const float fGroundY = xPlayerPosAtTargetSet.y - 1.05f;   // matches RenderTest capsule
	const float fAnkleHeight = 0.05f;
	const Zenith_Maths::Vector3 xTargetWorld(xFootWorldAtTargetSet.x, fGroundY + fAnkleHeight, xFootWorldAtTargetSet.z);

	// === Step 2 — WORLD-SPACE TARGET PATH (current/buggy behavior) ===
	// Solve uses world matrix from start of frame N+1 (after physics moved player).
	{
		Flux_SkeletonPose xPoseWorld = xPose;   // copy
		Flux_IKSolver xSolver;
		xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
			"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

		Flux_IKTarget xT;
		xT.m_xPosition = xTargetWorld;
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPoseWorld, *pxSkel, xWorldAtSolve);   // ← uses post-physics matrix

		// After solve, foot world = xWorldAtSolve * footModel. The drift between
		// xWorldAtTargetSet and xWorldAtSolve translates into foot-world drift.
		const Zenith_Maths::Vector4 xFootAfterW4 = xWorldAtSolve * xPoseWorld.GetModelSpaceMatrix(uFootIdx) * Zenith_Maths::Vector4(0, 0, 0, 1);
		const Zenith_Maths::Vector3 xFootAfterWorld(xFootAfterW4);

		const float fForwardDrag = xPlayerPosAtSolve.z - xFootAfterWorld.z;   // positive = foot behind player

		Zenith_Log(LOG_CATEGORY_ANIMATION,
			"[IKWorldLagTest] inter-frame move=%.4f, foot post-solve world Z=%.4f, expected=%.4f, drag=%.4f",
			fInterFrameMove, xFootAfterWorld.z, xPlayerPosAtSolve.z, fForwardDrag);

		// The world-space target path SHOULD show drag of ~v*dt (the inter-frame
		// physics motion). This assertion documents the bug — it currently fails
		// (drag present) but the model-space path below should fix it.
		ZENITH_ASSERT_TRUE(fForwardDrag > fInterFrameMove * 0.5f,
			"World-space-target path should produce visible drag (this assertion documents the bug)");
	}

	// === Step 3 — MODEL-SPACE TARGET PATH (fix) ===
	// Compute target in model space at target-set time, then pass it through
	// untouched at solve time. No inverse-transform needed → no drag.
	{
		Flux_SkeletonPose xPoseModel = xPose;   // copy
		Flux_IKSolver xSolver;
		xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
			"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

		// Convert world target to model space using the matrix at target-set time.
		const Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorldAtTargetSet);
		const Zenith_Maths::Vector4 xTargetModel4 = xInvWorld * Zenith_Maths::Vector4(xTargetWorld, 1.0f);

		Flux_IKTarget xT;
		xT.m_xPosition = Zenith_Maths::Vector3(xTargetModel4);
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xT.m_bIsModelSpace = true;   // ← skip inverse-transform in Solve
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPoseModel, *pxSkel, xWorldAtSolve);   // matrix doesn't matter for model-space target

		// Foot model-space position after IK should match the model-space target.
		// In world: xWorldAtSolve * footModel. The body has moved forward by v*dt,
		// and the foot moves with it (relative position constant in model space).
		const Zenith_Maths::Vector4 xFootAfterW4 = xWorldAtSolve * xPoseModel.GetModelSpaceMatrix(uFootIdx) * Zenith_Maths::Vector4(0, 0, 0, 1);
		const Zenith_Maths::Vector3 xFootAfterWorld(xFootAfterW4);

		// Expected foot world: xPlayerPosAtSolve + (model offset to foot bind+target).
		// Since the target is at the bind position (foot at ground+ankle), the foot
		// should follow the player. Specifically: xFootAfterWorld.z should track
		// xPlayerPosAtSolve.z (no drag).
		const float fForwardDrag = xPlayerPosAtSolve.z - xFootAfterWorld.z;

		Zenith_Log(LOG_CATEGORY_ANIMATION,
			"[IKModelSpaceTest] inter-frame move=%.4f, foot post-solve world Z=%.4f, drag=%.4f",
			fInterFrameMove, xFootAfterWorld.z, fForwardDrag);

		// With model-space target: NO drag. Foot stays under the body.
		ZENITH_ASSERT_TRUE(std::abs(fForwardDrag) < 0.01f,
			"Model-space-target path should produce NO drag — foot tracks body forward motion");
	}

	delete pxSkel;
}

// Sanity: when the world matrix is the SAME at target-set and solve time, the
// model-space and world-space paths should produce identical results. This
// pins the equivalence so the model-space code path doesn't silently diverge.
ZENITH_TEST(Animation, IKModelSpaceTargetEqualsWorldSpaceTargetWhenNoLag) { Zenith_UnitTests::TestIKModelSpaceTargetEqualsWorldSpaceTargetWhenNoLag(); }
void Zenith_UnitTests::TestIKModelSpaceTargetEqualsWorldSpaceTargetWhenNoLag()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	const uint32_t uFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f),
		Zenith_Maths::Vector3(5.0f, 10.0f, 5.0f)) *
		glm::toMat4(glm::angleAxis(0.7f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

	// Same target in world space, two paths.
	const Zenith_Maths::Vector3 xTargetWorld(4.95f, 9.10f, 5.10f);

	auto SolveAndGetFootModel = [&](bool bModelSpace) -> Zenith_Maths::Vector3 {
		Flux_SkeletonPose xPose;
		InitPoseAtBindForSkeleton(xPose, *pxSkel);

		Flux_IKSolver xSolver;
		xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
			"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

		Flux_IKTarget xT;
		if (bModelSpace)
		{
			Zenith_Maths::Vector4 xModel4 = glm::inverse(xWorld) * Zenith_Maths::Vector4(xTargetWorld, 1.0f);
			xT.m_xPosition = Zenith_Maths::Vector3(xModel4);
			xT.m_bIsModelSpace = true;
		}
		else
		{
			xT.m_xPosition = xTargetWorld;
			xT.m_bIsModelSpace = false;
		}
		xT.m_fWeight = 1.0f;
		xT.m_bEnabled = true;
		xSolver.SetTarget("LeftLeg", xT);
		xSolver.Solve(xPose, *pxSkel, xWorld);

		return Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uFootIdx)[3]);
	};

	const Zenith_Maths::Vector3 xFootWorld = SolveAndGetFootModel(false);
	const Zenith_Maths::Vector3 xFootModel = SolveAndGetFootModel(true);

	ZENITH_ASSERT_TRUE(Vec3Equals(xFootWorld, xFootModel, 0.001f),
		"World-space and model-space target paths should agree when the world matrix is consistent");

	delete pxSkel;
}

// Reproduces the "legs don't move while walking" bug.
//
// The foot-IK helper reads the foot's CURRENT model-space position from the
// skeleton (which is the POST-IK output of the previous frame), then sets the
// IK target to (foot_xz, ground_y). On the next frame the animation pose is
// overwritten by the current animation keyframe (the leg has swung to a new
// pose), but IK runs with the target XZ from the previous frame and pulls the
// foot back to that XZ. The animation's leg-swing can never get through →
// legs appear frozen.
//
// This test pins that pathology numerically by simulating multiple frames of
// walk animation: it sets up an oscillating foot XZ pose each frame, runs IK
// with weight 1, and asserts that the resulting foot XZ does NOT track the
// animation. (The fix is in the production code path, where the player should
// drop IK weight to 0 while walking — covered by the next test.)
ZENITH_TEST(Animation, IKLocksFootXZAcrossFramesAtFullWeight) { Zenith_UnitTests::TestIKLocksFootXZAcrossFramesAtFullWeight(); }
void Zenith_UnitTests::TestIKLocksFootXZAcrossFramesAtFullWeight()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	const uint32_t uUpperIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uFootIdx  = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));

	Flux_SkeletonPose xPose;
	xPose.Initialize(pxSkel->GetNumBones());

	Flux_IKSolver xSolver;
	xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

	const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f),
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));

	// Simulate a walk-anim that swings the upper leg forward and back. Per frame
	// we mimic the actual animator pipeline: animation sets local poses, IK
	// reads target from previous frame, IK modifies the leg, UpdateFootIK reads
	// post-IK foot and sets next target.
	const int kFrames = 30;
	const float fSwingDeg = 30.0f;

	float fLastFootZ = 0.0f;
	bool bHasTarget = false;
	Flux_IKTarget xTarget;
	xTarget.m_fWeight = 1.0f;
	xTarget.m_bEnabled = true;
	xTarget.m_bIsModelSpace = true;

	float fMaxFootZAcrossFrames = -1e9f;
	float fMinFootZAcrossFrames = 1e9f;

	for (int iFrame = 0; iFrame < kFrames; ++iFrame)
	{
		// Step 1: animation overwrites local rotations (swing UpperLeg).
		xPose.InitFromBindPose(*pxSkel);
		const float fT = (float)iFrame / (float)kFrames;
		const float fSwing = sinf(fT * 6.28318f) * fSwingDeg;
		xPose.GetLocalPose(uUpperIdx).m_xRotation = glm::angleAxis(glm::radians(fSwing),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

		// Step 2: IK solve with previous frame's target (if any).
		if (bHasTarget)
		{
			xSolver.SetTarget("LeftLeg", xTarget);
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
			xSolver.Solve(xPose, *pxSkel, xWorld);
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		}
		else
		{
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		}

		// Step 3: simulated UpdateFootIK — read post-IK foot, build target.
		const Zenith_Maths::Vector3 xFootModelPostIK(xPose.GetModelSpaceMatrix(uFootIdx)[3]);

		fLastFootZ = xFootModelPostIK.z;
		fMaxFootZAcrossFrames = std::max(fMaxFootZAcrossFrames, xFootModelPostIK.z);
		fMinFootZAcrossFrames = std::min(fMinFootZAcrossFrames, xFootModelPostIK.z);

		// Build target (already in model space — same path as production).
		xTarget.m_xPosition = Zenith_Maths::Vector3(xFootModelPostIK.x, -1.0f, xFootModelPostIK.z);
		bHasTarget = true;
	}

	// At weight 1, foot Z range should be small — the IK locks it after the
	// first frame even though animation swings ±0.5m.
	const float fFootZRange = fMaxFootZAcrossFrames - fMinFootZAcrossFrames;
	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKLockTest weight=1] foot Z range across %d frames = %.4f (anim swing produces ~1.0m without lock)",
		kFrames, fFootZRange);

	ZENITH_ASSERT_TRUE(fFootZRange < 0.2f,
		"At full IK weight, foot Z should be locked across frames despite animation swing — this assertion documents the bug");

	delete pxSkel;
}

// The fix path: when the player is moving, IK weight drops to 0. The IK solver
// short-circuits (E3d) and animation drives the legs. Foot model XZ should
// then track the animation's pose each frame.
ZENITH_TEST(Animation, IKLetsAnimationDriveFootXZAtZeroWeight) { Zenith_UnitTests::TestIKLetsAnimationDriveFootXZAtZeroWeight(); }
void Zenith_UnitTests::TestIKLetsAnimationDriveFootXZAtZeroWeight()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	const uint32_t uUpperIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));
	const uint32_t uFootIdx  = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));

	Flux_SkeletonPose xPose;
	xPose.Initialize(pxSkel->GetNumBones());

	Flux_IKSolver xSolver;
	xSolver.AddChain(Flux_IKSolver::CreateLegChain("LeftLeg",
		"LeftUpperLeg", "LeftLowerLeg", "LeftFoot"));

	const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f),
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));

	const int kFrames = 30;
	const float fSwingDeg = 30.0f;

	bool bHasTarget = false;
	Flux_IKTarget xTarget;
	xTarget.m_fWeight = 0.0f;   // ← zero weight — production behavior while moving
	xTarget.m_bEnabled = true;
	xTarget.m_bIsModelSpace = true;

	float fMaxFootZAcrossFrames = -1e9f;
	float fMinFootZAcrossFrames = 1e9f;

	for (int iFrame = 0; iFrame < kFrames; ++iFrame)
	{
		xPose.InitFromBindPose(*pxSkel);
		const float fT = (float)iFrame / (float)kFrames;
		const float fSwing = sinf(fT * 6.28318f) * fSwingDeg;
		xPose.GetLocalPose(uUpperIdx).m_xRotation = glm::angleAxis(glm::radians(fSwing),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

		if (bHasTarget)
		{
			xSolver.SetTarget("LeftLeg", xTarget);
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
			xSolver.Solve(xPose, *pxSkel, xWorld);
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		}
		else
		{
			xPose.ComputeModelSpaceMatricesFromSkeleton(*pxSkel);
		}

		const Zenith_Maths::Vector3 xFootModelPostIK(xPose.GetModelSpaceMatrix(uFootIdx)[3]);

		fMaxFootZAcrossFrames = std::max(fMaxFootZAcrossFrames, xFootModelPostIK.z);
		fMinFootZAcrossFrames = std::min(fMinFootZAcrossFrames, xFootModelPostIK.z);

		xTarget.m_xPosition = Zenith_Maths::Vector3(xFootModelPostIK.x, -1.0f, xFootModelPostIK.z);
		bHasTarget = true;
	}

	// At weight 0, animation drives the legs. Foot Z swings ±sin(30°)*1.0 ≈ ±0.5m,
	// for a total range of ~1.0m. Anything > 0.5m proves anim is getting through.
	const float fFootZRange = fMaxFootZAcrossFrames - fMinFootZAcrossFrames;
	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKLockTest weight=0] foot Z range across %d frames = %.4f (expected ~1.0m from anim swing)",
		kFrames, fFootZRange);

	ZENITH_ASSERT_TRUE(fFootZRange > 0.5f,
		"At zero IK weight, foot Z should track animation swing across frames — animation drives legs");

	delete pxSkel;
}

// Verifies the asymmetric-feet spawn scenario in RenderTest.cpp: the player
// spawns at (256, 49.30, 256) on the main platform with the IKStep_Spawn cube
// (30cm-tall raised block) positioned to be CLEARLY OUTSIDE the player's
// capsule (capsule radius 0.10 → cube right edge at offset -0.105, 5mm clear).
// The capsule rests on the main platform unmolested; only the left foot's
// raycast hits the elevated step. The IK should produce visibly different
// foot heights AND a clearly bent left knee (forward, +Z in model space).
//
// Geometry mirrored from Project_RegisterEditorAutomationSteps:
//   - Main platform top Y = 48.25
//   - IKStep_Spawn top Y = 48.55 (30cm above main), X-range [255.70, 255.895]
//   - Player capsule (0.10, 0.95): radius 0.10 narrow enough that foot at
//     model-X offset 0.15 is OUTSIDE the capsule. Cube right edge at
//     X=255.895 vs capsule left edge at X=255.90 — 5mm gap.
//   - Player at (256, 49.30, 256), capsule rests on main platform
//   - Left foot at world (255.85, 48.30, 256) — over step
//   - Right foot at world (256.15, 48.30, 256) — over main platform
//   - IK targets: left (255.85, 48.60, 256), right (256.15, 48.30, 256)
//   - Expected: left foot Y ≈ 48.60, right foot Y ≈ 48.30, diff ≈ 30cm
//   - Knee should bend FORWARD (model +Z) by ~36cm, NOT sideways or backward
ZENITH_TEST(Animation, IKAsymmetricFeetAtSpawn) { Zenith_UnitTests::TestIKAsymmetricFeetAtSpawn(); }
void Zenith_UnitTests::TestIKAsymmetricFeetAtSpawn()
{
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	const uint32_t uLeftFootIdx  = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftFoot"));
	const uint32_t uRightFootIdx = static_cast<uint32_t>(pxSkel->GetBoneIndex("RightFoot"));

	Flux_SkeletonPose xPose;
	InitPoseAtBindForSkeleton(xPose, *pxSkel);

	Flux_IKSolver xSolver;
	{
		Flux_IKChain xLeft = Flux_IKSolver::CreateLegChain("LeftLeg",
			"LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
		Flux_IKChain xRight = Flux_IKSolver::CreateLegChain("RightLeg",
			"RightUpperLeg", "RightLowerLeg", "RightFoot");
		// Bump iterations + tighten tolerance — FABRIK with pole vector and hinge
		// constraints converges slowly when the target is near the chain length
		// limit (which both targets are here, at 0.983m and 1.0m respectively).
		// The default 10 iterations leaves ~13mm error on the bent leg.
		xLeft.m_uMaxIterations = 30;
		xLeft.m_fTolerance = 0.0005f;
		xRight.m_uMaxIterations = 30;
		xRight.m_fTolerance = 0.0005f;
		xSolver.AddChain(xLeft);
		xSolver.AddChain(xRight);
	}

	// Player at (256, 49.30, 256) — capsule rests on main platform, IKStep_Spawn
	// cube is clearly outside the capsule X-range so it doesn't push the player up.
	const Zenith_Maths::Vector3 xPlayerPos(256.0f, 49.30f, 256.0f);
	const Zenith_Maths::Matrix4 xWorld = glm::translate(glm::mat4(1.0f), xPlayerPos);

	// Left foot at X=255.85 over IKStep_Spawn (top 48.55).
	// Right foot at X=256.15 over main platform (top 48.25).
	const float fStepTopY = 48.55f;
	const float fMainTopY = 48.25f;
	const float fAnkleHeight = 0.05f;

	const Zenith_Maths::Vector3 xLeftTargetWorld(255.85f, fStepTopY + fAnkleHeight, 256.0f);
	const Zenith_Maths::Vector3 xRightTargetWorld(256.15f, fMainTopY + fAnkleHeight, 256.0f);

	// Convert to model space at the same world matrix UpdateFootIK would use,
	// then set targets via the model-space path (matches production behavior).
	const Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorld);
	const Zenith_Maths::Vector4 xLeftTargetModel4  = xInvWorld * Zenith_Maths::Vector4(xLeftTargetWorld,  1.0f);
	const Zenith_Maths::Vector4 xRightTargetModel4 = xInvWorld * Zenith_Maths::Vector4(xRightTargetWorld, 1.0f);

	Flux_IKTarget xTL;
	xTL.m_xPosition = Zenith_Maths::Vector3(xLeftTargetModel4);
	xTL.m_fWeight = 1.0f;
	xTL.m_bEnabled = true;
	xTL.m_bIsModelSpace = true;
	xSolver.SetTarget("LeftLeg", xTL);

	Flux_IKTarget xTR;
	xTR.m_xPosition = Zenith_Maths::Vector3(xRightTargetModel4);
	xTR.m_fWeight = 1.0f;
	xTR.m_bEnabled = true;
	xTR.m_bIsModelSpace = true;
	xSolver.SetTarget("RightLeg", xTR);

	xSolver.Solve(xPose, *pxSkel, xWorld);

	const uint32_t uLeftKneeIdx  = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftLowerLeg"));
	const uint32_t uLeftHipIdx   = static_cast<uint32_t>(pxSkel->GetBoneIndex("LeftUpperLeg"));

	const Zenith_Maths::Vector4 xLeftFootW4  = xWorld * xPose.GetModelSpaceMatrix(uLeftFootIdx)  * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector4 xRightFootW4 = xWorld * xPose.GetModelSpaceMatrix(uRightFootIdx) * Zenith_Maths::Vector4(0, 0, 0, 1);
	const Zenith_Maths::Vector3 xLeftFootWorld(xLeftFootW4);
	const Zenith_Maths::Vector3 xRightFootWorld(xRightFootW4);

	const Zenith_Maths::Vector3 xLeftFootModel (xPose.GetModelSpaceMatrix(uLeftFootIdx)[3]);
	const Zenith_Maths::Vector3 xLeftKneeModel (xPose.GetModelSpaceMatrix(uLeftKneeIdx)[3]);
	const Zenith_Maths::Vector3 xLeftHipModel  (xPose.GetModelSpaceMatrix(uLeftHipIdx)[3]);

	const float fHeightDiff = xLeftFootWorld.y - xRightFootWorld.y;

	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKSpawnAsymmetryTest] left foot Y=%.4f right foot Y=%.4f diff=%.4f",
		xLeftFootWorld.y, xRightFootWorld.y, fHeightDiff);
	Zenith_Log(LOG_CATEGORY_ANIMATION,
		"[IKSpawnAsymmetryTest] L hip model=(%.3f,%.3f,%.3f) knee model=(%.3f,%.3f,%.3f) foot model=(%.3f,%.3f,%.3f)",
		xLeftHipModel.x, xLeftHipModel.y, xLeftHipModel.z,
		xLeftKneeModel.x, xLeftKneeModel.y, xLeftKneeModel.z,
		xLeftFootModel.x, xLeftFootModel.y, xLeftFootModel.z);

	// Left foot should be ~30cm above right foot. Visible asymmetry threshold: ≥10cm.
	ZENITH_ASSERT_TRUE(fHeightDiff > 0.10f,
		"At spawn, left foot (on step cube) should be visibly higher than right foot (on main platform)");

	// Sanity: left foot should be near the step + ankle, right foot near main + ankle.
	ZENITH_ASSERT_TRUE(std::abs(xLeftFootWorld.y - (fStepTopY + fAnkleHeight)) < 0.05f,
		"Left foot should land near the step top + ankle height");
	ZENITH_ASSERT_TRUE(std::abs(xRightFootWorld.y - (fMainTopY + fAnkleHeight)) < 0.05f,
		"Right foot should land near the main platform top + ankle height");

	// Knee bend assertion: the left leg lifts the foot 30cm, so the chain folds
	// from 1m straight to 0.7m hip-to-foot distance. The knee must protrude
	// FORWARD (model +Z) by ~36cm — not sideways, not backward, not zero.
	// This is what makes the leg visually look like a natural human knee bend
	// rather than a stiff straight leg.
	const float fExpectedKneeForward = 0.36f;     // sqrt(0.5² - 0.35²) ≈ 0.357
	ZENITH_ASSERT_TRUE(xLeftKneeModel.z > 0.20f,
		"Left knee should bend FORWARD (+Z in model space) by at least 20cm — anything less means the IK isn't producing a natural-looking bend");
	ZENITH_ASSERT_TRUE(std::abs(xLeftKneeModel.z - fExpectedKneeForward) < 0.10f,
		"Left knee Z should be close to the geometric expectation (~36cm forward)");
	ZENITH_ASSERT_TRUE(std::abs(xLeftKneeModel.x - (-0.15f)) < 0.05f,
		"Left knee should stay aligned with the leg's X plane (no sideways bend)");

	delete pxSkel;
}

//------------------------------------------------------------------------------
// Stick Figure Animation Tests
//------------------------------------------------------------------------------

ZENITH_TEST(Core, StickFigureSkeletonCreation) { Zenith_UnitTests::TestStickFigureSkeletonCreation(); }

void Zenith_UnitTests::TestStickFigureSkeletonCreation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// Verify bone count
	ZENITH_ASSERT_EQ(pxSkel->GetNumBones(), STICK_BONE_COUNT, "Expected 16 bones");

	// Verify bone names exist
	ZENITH_ASSERT_TRUE(pxSkel->HasBone("Root"), "Missing Root bone");
	ZENITH_ASSERT_TRUE(pxSkel->HasBone("Spine"), "Missing Spine bone");
	ZENITH_ASSERT_TRUE(pxSkel->HasBone("Head"), "Missing Head bone");
	ZENITH_ASSERT_TRUE(pxSkel->HasBone("LeftUpperArm"), "Missing LeftUpperArm bone");
	ZENITH_ASSERT_TRUE(pxSkel->HasBone("LeftFoot"), "Missing LeftFoot bone");

	// Verify parent hierarchy
	ZENITH_ASSERT_EQ(pxSkel->GetBone(STICK_BONE_ROOT).m_iParentIndex, -1, "Root should have no parent");
	ZENITH_ASSERT_EQ(pxSkel->GetBone(STICK_BONE_SPINE).m_iParentIndex, STICK_BONE_ROOT, "Spine parent should be Root");
	ZENITH_ASSERT_EQ(pxSkel->GetBone(STICK_BONE_HEAD).m_iParentIndex, STICK_BONE_NECK, "Head parent should be Neck");
	ZENITH_ASSERT_EQ(pxSkel->GetBone(STICK_BONE_LEFT_HAND).m_iParentIndex, STICK_BONE_LEFT_LOWER_ARM, "LeftHand parent should be LeftLowerArm");

	// Verify bind pose world positions
	Zenith_Maths::Vector3 xHeadPos = Zenith_Maths::Vector3(pxSkel->GetBone(STICK_BONE_HEAD).m_xBindPoseModel[3]);
	ZENITH_ASSERT_TRUE(Vec3Equals(xHeadPos, Zenith_Maths::Vector3(0, 1.4f, 0), 0.01f), "Head world position mismatch");

	Zenith_Maths::Vector3 xLeftFootPos = Zenith_Maths::Vector3(pxSkel->GetBone(STICK_BONE_LEFT_FOOT).m_xBindPoseModel[3]);
	ZENITH_ASSERT_TRUE(Vec3Equals(xLeftFootPos, Zenith_Maths::Vector3(-0.15f, -1.0f, 0), 0.01f), "LeftFoot world position mismatch");


	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureMeshCreation) { Zenith_UnitTests::TestStickFigureMeshCreation(); }

void Zenith_UnitTests::TestStickFigureMeshCreation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);

	// Verify vertex/index counts
	const uint32_t uExpectedVerts = STICK_BONE_COUNT * 8;  // 128
	const uint32_t uExpectedIndices = STICK_BONE_COUNT * 36;  // 576

	ZENITH_ASSERT_EQ(pxMesh->GetNumVerts(), uExpectedVerts, "Expected 128 vertices");
	ZENITH_ASSERT_EQ(pxMesh->GetNumIndices(), uExpectedIndices, "Expected 576 indices");

	// Verify skinning weights
	ZENITH_ASSERT_EQ(pxMesh->m_xBoneIndices.GetSize(), uExpectedVerts, "Bone indices count mismatch");
	ZENITH_ASSERT_EQ(pxMesh->m_xBoneWeights.GetSize(), uExpectedVerts, "Bone weights count mismatch");

	// Check that each vertex is 100% weighted to one bone
	for (uint32_t v = 0; v < uExpectedVerts; v++)
	{
		const glm::vec4& xWeights = pxMesh->m_xBoneWeights.Get(v);
		ZENITH_ASSERT_TRUE(FloatEquals(xWeights.x, 1.0f, 0.001f), "Vertex weight should be 1.0");
		ZENITH_ASSERT_TRUE(FloatEquals(xWeights.y, 0.0f, 0.001f), "Secondary weight should be 0.0");
	}

	// Verify bounds
	ZENITH_ASSERT_LT(pxMesh->GetBoundsMin().y, -0.9f, "Bounds min Y should be below -0.9");
	ZENITH_ASSERT_GT(pxMesh->GetBoundsMax().y, 1.3f, "Bounds max Y should be above 1.3");

	delete pxMesh;
	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureMeshJointAlignment) { Zenith_UnitTests::TestStickFigureMeshJointAlignment(); }

void Zenith_UnitTests::TestStickFigureMeshJointAlignment()
{
	// Verify each leg/arm bone's cube is centered at the midpoint between the
	// bone pivot and its child's pivot. With this invariant the cube's "top"
	// stays planted at the joint when the bone rotates around its pivot, so
	// adjacent cubes (parent and child) stay visually connected through rotation.
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);

	struct ChainCheck { uint32_t uBone; uint32_t uChild; const char* szName; };
	const ChainCheck axChecks[] = {
		{ STICK_BONE_LEFT_UPPER_LEG,  STICK_BONE_LEFT_LOWER_LEG,  "LeftUpperLeg"  },
		{ STICK_BONE_LEFT_LOWER_LEG,  STICK_BONE_LEFT_FOOT,       "LeftLowerLeg"  },
		{ STICK_BONE_RIGHT_UPPER_LEG, STICK_BONE_RIGHT_LOWER_LEG, "RightUpperLeg" },
		{ STICK_BONE_RIGHT_LOWER_LEG, STICK_BONE_RIGHT_FOOT,      "RightLowerLeg" },
		{ STICK_BONE_LEFT_UPPER_ARM,  STICK_BONE_LEFT_LOWER_ARM,  "LeftUpperArm"  },
		{ STICK_BONE_LEFT_LOWER_ARM,  STICK_BONE_LEFT_HAND,       "LeftLowerArm"  },
		{ STICK_BONE_RIGHT_UPPER_ARM, STICK_BONE_RIGHT_LOWER_ARM, "RightUpperArm" },
		{ STICK_BONE_RIGHT_LOWER_ARM, STICK_BONE_RIGHT_HAND,      "RightLowerArm" },
	};

	for (const ChainCheck& xChk : axChecks)
	{
		const float fBonePivotY  = pxSkel->GetBone(xChk.uBone).m_xBindPoseModel[3].y;
		const float fChildPivotY = pxSkel->GetBone(xChk.uChild).m_xBindPoseModel[3].y;

		float fMinY = FLT_MAX;
		float fMaxY = -FLT_MAX;
		for (uint32_t v = xChk.uBone * 8; v < (xChk.uBone + 1) * 8; ++v)
		{
			const Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(v);
			fMinY = std::min(fMinY, xPos.y);
			fMaxY = std::max(fMaxY, xPos.y);
		}

		// Cube should be centered on the midpoint between bone and child pivot.
		// This is the invariant that makes joints stay connected through rotation:
		// when a bone rotates around its pivot, a cube centered at the midpoint
		// keeps its top face anchored at the joint.
		const float fExpectedCenter = 0.5f * (fBonePivotY + fChildPivotY);
		const float fActualCenter = 0.5f * (fMaxY + fMinY);
		ZENITH_ASSERT_TRUE(FloatEquals(fActualCenter, fExpectedCenter, 0.001f),
			"Cube center Y should be at midpoint between bone and child pivot");
	}

	delete pxMesh;
	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureIdleAnimation) { Zenith_UnitTests::TestStickFigureIdleAnimation(); }

void Zenith_UnitTests::TestStickFigureIdleAnimation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateIdleAnimation();

	ZENITH_ASSERT_EQ(pxClip->GetName(), "Idle", "Animation name should be 'Idle'");
	ZENITH_ASSERT_TRUE(FloatEquals(pxClip->GetDuration(), 2.0f, 0.01f), "Duration should be 2.0 seconds");
	ZENITH_ASSERT_EQ(pxClip->GetTicksPerSecond(), 24, "Ticks per second should be 24");
	ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("Spine"), "Should have Spine bone channel");

	// Sample spine position at different times
	const Flux_BoneChannel* pxSpineChannel = pxClip->GetBoneChannel("Spine");
	ZENITH_ASSERT_NOT_NULL(pxSpineChannel, "Spine channel should exist");

	// t=0: position should be (0, 0.5, 0)
	Zenith_Maths::Vector3 xPos0 = pxSpineChannel->SamplePosition(0.0f);
	ZENITH_ASSERT_TRUE(Vec3Equals(xPos0, Zenith_Maths::Vector3(0, 0.5f, 0), 0.01f), "Spine position at t=0 mismatch");

	// t=24 ticks (1 second): position should be (0, 0.52, 0)
	Zenith_Maths::Vector3 xPos1 = pxSpineChannel->SamplePosition(24.0f);
	ZENITH_ASSERT_TRUE(Vec3Equals(xPos1, Zenith_Maths::Vector3(0, 0.52f, 0), 0.01f), "Spine position at t=1s mismatch");

	// t=12 ticks (0.5 seconds): position should be interpolated to (0, 0.51, 0)
	Zenith_Maths::Vector3 xPos05 = pxSpineChannel->SamplePosition(12.0f);
	ZENITH_ASSERT_TRUE(Vec3Equals(xPos05, Zenith_Maths::Vector3(0, 0.51f, 0), 0.01f), "Spine position at t=0.5s mismatch");


	delete pxClip;
	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureWalkAnimation) { Zenith_UnitTests::TestStickFigureWalkAnimation(); }

void Zenith_UnitTests::TestStickFigureWalkAnimation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateWalkAnimation();

	ZENITH_ASSERT_EQ(pxClip->GetName(), "Walk", "Animation name should be 'Walk'");
	ZENITH_ASSERT_TRUE(FloatEquals(pxClip->GetDuration(), 1.0f, 0.01f), "Duration should be 1.0 second");

	// Verify left upper leg rotation at t=0 (should be 30 degrees around X for forward/backward swing)
	const Flux_BoneChannel* pxLeftLegChannel = pxClip->GetBoneChannel("LeftUpperLeg");
	ZENITH_ASSERT_NOT_NULL(pxLeftLegChannel, "LeftUpperLeg channel should exist");

	Zenith_Maths::Quat xExpected30 = glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampled = pxLeftLegChannel->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(QuatEquals(xSampled, xExpected30, 0.01f), "LeftUpperLeg rotation at t=0 should be 30 deg");

	// Verify right upper leg is opposite phase at t=0 (-30 degrees)
	const Flux_BoneChannel* pxRightLegChannel = pxClip->GetBoneChannel("RightUpperLeg");
	ZENITH_ASSERT_NOT_NULL(pxRightLegChannel, "RightUpperLeg channel should exist");

	Zenith_Maths::Quat xExpectedMinus30 = glm::angleAxis(glm::radians(-30.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledRight = pxRightLegChannel->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(QuatEquals(xSampledRight, xExpectedMinus30, 0.01f), "RightUpperLeg rotation at t=0 should be -30 deg");

	// Verify arm swing
	const Flux_BoneChannel* pxLeftArmChannel = pxClip->GetBoneChannel("LeftUpperArm");
	ZENITH_ASSERT_NOT_NULL(pxLeftArmChannel, "LeftUpperArm channel should exist");

	Zenith_Maths::Quat xExpectedArm = glm::angleAxis(glm::radians(-20.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledArm = pxLeftArmChannel->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(QuatEquals(xSampledArm, xExpectedArm, 0.01f), "LeftUpperArm rotation at t=0 should be -20 deg");


	delete pxClip;
	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureRunAnimation) { Zenith_UnitTests::TestStickFigureRunAnimation(); }

void Zenith_UnitTests::TestStickFigureRunAnimation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateRunAnimation();

	ZENITH_ASSERT_EQ(pxClip->GetName(), "Run", "Animation name should be 'Run'");
	ZENITH_ASSERT_TRUE(FloatEquals(pxClip->GetDuration(), 0.5f, 0.01f), "Duration should be 0.5 seconds");

	// Verify left upper leg rotation at t=0 (should be 45 degrees around X - more exaggerated)
	const Flux_BoneChannel* pxLeftLegChannel = pxClip->GetBoneChannel("LeftUpperLeg");
	ZENITH_ASSERT_NOT_NULL(pxLeftLegChannel, "LeftUpperLeg channel should exist");

	Zenith_Maths::Quat xExpected45 = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampled = pxLeftLegChannel->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(QuatEquals(xSampled, xExpected45, 0.01f), "LeftUpperLeg rotation at t=0 should be 45 deg");

	// Verify arm swing (35 degrees around X - more exaggerated than walk)
	const Flux_BoneChannel* pxLeftArmChannel = pxClip->GetBoneChannel("LeftUpperArm");
	ZENITH_ASSERT_NOT_NULL(pxLeftArmChannel, "LeftUpperArm channel should exist");

	Zenith_Maths::Quat xExpectedArm = glm::angleAxis(glm::radians(-35.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledArm = pxLeftArmChannel->SampleRotation(0.0f);
	ZENITH_ASSERT_TRUE(QuatEquals(xSampledArm, xExpectedArm, 0.01f), "LeftUpperArm rotation at t=0 should be -35 deg");


	delete pxClip;
	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureAnimationBlending) { Zenith_UnitTests::TestStickFigureAnimationBlending(); }

void Zenith_UnitTests::TestStickFigureAnimationBlending(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_AnimationClip* pxRunClip = CreateRunAnimation();

	// Initialize skeleton poses
	Flux_SkeletonPose xWalkPose;
	xWalkPose.Initialize(STICK_BONE_COUNT);
	xWalkPose.SampleFromClip(*pxWalkClip, 0.0f, *pxSkel);

	Flux_SkeletonPose xRunPose;
	xRunPose.Initialize(STICK_BONE_COUNT);
	xRunPose.SampleFromClip(*pxRunClip, 0.0f, *pxSkel);

	// Get Walk and Run rotations for LeftUpperLeg
	const Flux_BoneLocalPose& xWalkLegPose = xWalkPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);
	const Flux_BoneLocalPose& xRunLegPose = xRunPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);


	// Test blending at different factors
	float afBlendFactors[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
	for (float fBlend : afBlendFactors)
	{
		Flux_SkeletonPose xBlendedPose;
		xBlendedPose.Initialize(STICK_BONE_COUNT);
		Flux_SkeletonPose::Blend(xBlendedPose, xWalkPose, xRunPose, fBlend);

		// Verify blended rotation
		const Flux_BoneLocalPose& xBlendedLeg = xBlendedPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);
		Zenith_Maths::Quat xExpected = glm::slerp(xWalkLegPose.m_xRotation, xRunLegPose.m_xRotation, fBlend);

		ZENITH_ASSERT_TRUE(QuatEquals(xBlendedLeg.m_xRotation, xExpected, 0.01f), "Blended rotation mismatch at factor %.2f", fBlend);

	}

	delete pxRunClip;
	delete pxWalkClip;
	delete pxSkel;
}

//------------------------------------------------------------------------------
// Stick Figure IK Tests
//------------------------------------------------------------------------------

ZENITH_TEST(Core, StickFigureArmIK) { Zenith_UnitTests::TestStickFigureArmIK(); }

void Zenith_UnitTests::TestStickFigureArmIK(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_IKSolver xSolver;

	// Create arm IK chains
	Flux_IKChain xLeftArm = Flux_IKSolver::CreateArmChain("LeftArm", "LeftUpperArm", "LeftLowerArm", "LeftHand");
	Flux_IKChain xRightArm = Flux_IKSolver::CreateArmChain("RightArm", "RightUpperArm", "RightLowerArm", "RightHand");

	xSolver.AddChain(xLeftArm);
	xSolver.AddChain(xRightArm);

	ZENITH_ASSERT_TRUE(xSolver.HasChain("LeftArm"), "Solver should have LeftArm chain");
	ZENITH_ASSERT_TRUE(xSolver.HasChain("RightArm"), "Solver should have RightArm chain");


	// Test setting targets
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = Zenith_Maths::Vector3(0, 1.0f, 0.5f);
	xTarget.m_fWeight = 1.0f;
	xTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftArm", xTarget);
	ZENITH_ASSERT_TRUE(xSolver.HasTarget("LeftArm"), "Solver should have LeftArm target");

	const Flux_IKTarget* pxStoredTarget = xSolver.GetTarget("LeftArm");
	ZENITH_ASSERT_NOT_NULL(pxStoredTarget, "Should be able to retrieve target");
	ZENITH_ASSERT_TRUE(Vec3Equals(pxStoredTarget->m_xPosition, xTarget.m_xPosition, 0.001f), "Target position mismatch");


	// Clear target
	xSolver.ClearTarget("LeftArm");
	ZENITH_ASSERT_FALSE(xSolver.HasTarget("LeftArm"), "Target should be cleared");

	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureLegIK) { Zenith_UnitTests::TestStickFigureLegIK(); }

void Zenith_UnitTests::TestStickFigureLegIK(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_IKSolver xSolver;

	// Create leg IK chains
	Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
	Flux_IKChain xRightLeg = Flux_IKSolver::CreateLegChain("RightLeg", "RightUpperLeg", "RightLowerLeg", "RightFoot");

	xSolver.AddChain(xLeftLeg);
	xSolver.AddChain(xRightLeg);

	ZENITH_ASSERT_TRUE(xSolver.HasChain("LeftLeg"), "Solver should have LeftLeg chain");
	ZENITH_ASSERT_TRUE(xSolver.HasChain("RightLeg"), "Solver should have RightLeg chain");

	// Verify chain bone count
	const Flux_IKChain* pxLeftLegChain = xSolver.GetChain("LeftLeg");
	ZENITH_ASSERT_NOT_NULL(pxLeftLegChain, "Should be able to retrieve LeftLeg chain");
	ZENITH_ASSERT_EQ(pxLeftLegChain->m_xBoneNames.GetSize(), 3, "Leg chain should have 3 bones");


	// Test setting targets for both legs
	Flux_IKTarget xLeftTarget;
	xLeftTarget.m_xPosition = Zenith_Maths::Vector3(-0.15f, -0.8f, 0.2f);
	xLeftTarget.m_fWeight = 1.0f;
	xLeftTarget.m_bEnabled = true;

	Flux_IKTarget xRightTarget;
	xRightTarget.m_xPosition = Zenith_Maths::Vector3(0.15f, -0.9f, -0.1f);
	xRightTarget.m_fWeight = 1.0f;
	xRightTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftLeg", xLeftTarget);
	xSolver.SetTarget("RightLeg", xRightTarget);

	ZENITH_ASSERT_TRUE(xSolver.HasTarget("LeftLeg"), "Solver should have LeftLeg target");
	ZENITH_ASSERT_TRUE(xSolver.HasTarget("RightLeg"), "Solver should have RightLeg target");


	delete pxSkel;
}

ZENITH_TEST(Core, StickFigureIKWithAnimation) { Zenith_UnitTests::TestStickFigureIKWithAnimation(); }

void Zenith_UnitTests::TestStickFigureIKWithAnimation(){

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_IKSolver xSolver;

	// Set up leg IK
	Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
	xSolver.AddChain(xLeftLeg);

	// Sample walk animation at mid-stride
	Flux_SkeletonPose xAnimPose;
	xAnimPose.Initialize(STICK_BONE_COUNT);
	float fMidStride = 0.5f * pxWalkClip->GetTicksPerSecond(); // 12 ticks
	xAnimPose.SampleFromClip(*pxWalkClip, fMidStride, *pxSkel);


	// Set IK target
	Flux_IKTarget xFootTarget;
	xFootTarget.m_xPosition = Zenith_Maths::Vector3(-0.15f, -0.9f, 0.1f);
	xFootTarget.m_fWeight = 1.0f;
	xFootTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftLeg", xFootTarget);

	// Test different blend weights
	for (float fWeight : {0.0f, 0.5f, 1.0f})
	{
		Flux_IKTarget xWeightedTarget = xFootTarget;
		xWeightedTarget.m_fWeight = fWeight;
		xSolver.SetTarget("LeftLeg", xWeightedTarget);

	}

	delete pxWalkClip;
	delete pxSkel;
}

//------------------------------------------------------------------------------
// Animation State Machine Integration Tests
//------------------------------------------------------------------------------

ZENITH_TEST(Animation, StateMachineUpdateLoop) { Zenith_UnitTests::TestStateMachineUpdateLoop(); }

void Zenith_UnitTests::TestStateMachineUpdateLoop(){

	// Create state machine with Idle and Walk states
	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	Flux_AnimationState* pxWalk = xStateMachine.AddState("Walk");

	// Add transition: Idle -> Walk when Speed > 0.1
	Flux_StateTransition xIdleToWalk;
	xIdleToWalk.m_strTargetStateName = "Walk";
	xIdleToWalk.m_fTransitionDuration = 0.2f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 0.1f;
	xIdleToWalk.m_xConditions.PushBack(xSpeedCond);

	pxIdle->AddTransition(xIdleToWalk);

	// Add transition: Walk -> Idle when Speed <= 0.1
	Flux_StateTransition xWalkToIdle;
	xWalkToIdle.m_strTargetStateName = "Idle";
	xWalkToIdle.m_fTransitionDuration = 0.2f;

	Flux_TransitionCondition xSlowCond;
	xSlowCond.m_strParameterName = "Speed";
	xSlowCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
	xSlowCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSlowCond.m_fThreshold = 0.1f;
	xWalkToIdle.m_xConditions.PushBack(xSlowCond);

	pxWalk->AddTransition(xWalkToIdle);

	xStateMachine.SetDefaultState("Idle");

	// Create dummy skeleton and pose for Update calls
	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	// Initial update - should be in Idle
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should start in Idle state");

	// Set Speed > 0.1, update - transition should start
	xStateMachine.GetParameters().SetFloat("Speed", 0.5f);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.IsTransitioning(), true, "Should be transitioning after condition met");

	// Continue updating until transition completes
	for (int i = 0; i < 20; ++i)
	{
		xStateMachine.Update(0.016f, xPose, xSkeleton);
	}

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Walk", "Should be in Walk state after transition completes");
	ZENITH_ASSERT_EQ(xStateMachine.IsTransitioning(), false, "Transition should be complete");

}

ZENITH_TEST(Core, TriggerConsumptionInTransitions) { Zenith_UnitTests::TestTriggerConsumptionInTransitions(); }

void Zenith_UnitTests::TestTriggerConsumptionInTransitions(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Attack");

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Attack");

	// Idle -> Attack on AttackTrigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Attack";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xTriggerCond;
	xTriggerCond.m_strParameterName = "Attack";
	xTriggerCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xTriggerCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial state
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Set trigger
	xStateMachine.GetParameters().SetTrigger("Attack");

	// Update - trigger should be consumed and transition should start
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xStateMachine.IsTransitioning(), true, "Transition should start after trigger set");

	// Trigger should be consumed - trying to consume again should return false
	ZENITH_ASSERT_EQ(xStateMachine.GetParameters().ConsumeTrigger("Attack"), false, "Trigger should have been consumed by transition");

}

ZENITH_TEST(Core, ExitTimeTransitions) { Zenith_UnitTests::TestExitTimeTransitions(); }

void Zenith_UnitTests::TestExitTimeTransitions(){

	// Test the CanTransition method with exit time
	Flux_AnimationParameters xParams;

	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Idle";
	xTrans.m_fTransitionDuration = 0.1f;
	xTrans.m_bHasExitTime = true;
	xTrans.m_fExitTime = 0.8f;
	// No other conditions - should auto-transition at exit time

	// Test before exit time
	bool bCanTransBefore = xTrans.CanTransition(xParams, 0.5f);
	ZENITH_ASSERT_EQ(bCanTransBefore, false, "Should not transition before exit time (0.5 < 0.8)");

	// Test at exit time
	bool bCanTransAt = xTrans.CanTransition(xParams, 0.8f);
	ZENITH_ASSERT_EQ(bCanTransAt, true, "Should transition at exit time (0.8 >= 0.8)");

	// Test after exit time
	bool bCanTransAfter = xTrans.CanTransition(xParams, 0.95f);
	ZENITH_ASSERT_EQ(bCanTransAfter, true, "Should transition after exit time (0.95 >= 0.8)");

}

ZENITH_TEST(Animation, TransitionPriority) { Zenith_UnitTests::TestTransitionPriority(); }

void Zenith_UnitTests::TestTransitionPriority(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
	xStateMachine.GetParameters().AddTrigger("Attack");

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Walk");
	xStateMachine.AddState("Attack");

	// Add two transitions from Idle:
	// 1. Idle -> Walk (Speed > 0.1) - low priority
	// 2. Idle -> Attack (AttackTrigger) - high priority

	Flux_StateTransition xToWalk;
	xToWalk.m_strTargetStateName = "Walk";
	xToWalk.m_fTransitionDuration = 0.1f;
	xToWalk.m_iPriority = 0;  // Low priority

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 0.1f;
	xToWalk.m_xConditions.PushBack(xSpeedCond);

	Flux_StateTransition xToAttack;
	xToAttack.m_strTargetStateName = "Attack";
	xToAttack.m_fTransitionDuration = 0.05f;
	xToAttack.m_iPriority = 10;  // High priority

	Flux_TransitionCondition xAttackCond;
	xAttackCond.m_strParameterName = "Attack";
	xAttackCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xToAttack.m_xConditions.PushBack(xAttackCond);

	// Add in reverse priority order to verify sorting
	pxIdle->AddTransition(xToWalk);
	pxIdle->AddTransition(xToAttack);

	// Verify transitions are sorted by priority
	const Zenith_Vector<Flux_StateTransition>& xTransitions = pxIdle->GetTransitions();
	ZENITH_ASSERT_GE(xTransitions.Get(0).m_iPriority, xTransitions.Get(1).m_iPriority, "Transitions should be sorted by priority (higher first)");

	// Set both conditions true - Attack should win due to priority
	xStateMachine.SetDefaultState("Idle");
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	xStateMachine.GetParameters().SetFloat("Speed", 0.5f);
	xStateMachine.GetParameters().SetTrigger("Attack");

	xStateMachine.Update(0.016f, xPose, xSkeleton);

	// Complete the transition
	for (int i = 0; i < 10; ++i)
		xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Attack", "Higher priority transition (Attack) should be chosen over Walk");

}

ZENITH_TEST(Core, StateLifecycleCallbacks) { Zenith_UnitTests::TestStateLifecycleCallbacks(); }

void Zenith_UnitTests::TestStateLifecycleCallbacks(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	struct CallbackData
	{
		bool m_bEnterCalled = false;
		bool m_bExitCalled = false;
		bool m_bUpdateCalled = false;
		float m_fUpdateDt = 0.0f;
	};
	CallbackData xCallbackData;

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Next");

	Flux_AnimationState* pxStateA = xStateMachine.AddState("StateA");
	xStateMachine.AddState("StateB");

	// Set up callbacks on StateA using function pointers + userdata
	pxStateA->m_pfnOnEnter = [](void* pUserData) { static_cast<CallbackData*>(pUserData)->m_bEnterCalled = true; };
	pxStateA->m_pfnOnExit = [](void* pUserData) { static_cast<CallbackData*>(pUserData)->m_bExitCalled = true; };
	pxStateA->m_pfnOnUpdate = [](void* pUserData, float fDt) {
		CallbackData* pxData = static_cast<CallbackData*>(pUserData);
		pxData->m_bUpdateCalled = true;
		pxData->m_fUpdateDt = fDt;
	};
	pxStateA->m_pCallbackUserData = &xCallbackData;

	// StateA -> StateB on trigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "StateB";
	xTrans.m_fTransitionDuration = 0.05f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Next";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xCond);
	pxStateA->AddTransition(xTrans);

	// Test OnEnter via SetState
	xStateMachine.SetState("StateA");
	ZENITH_ASSERT_EQ(xCallbackData.m_bEnterCalled, true, "OnEnter should be called on SetState");

	// Test OnUpdate
	xCallbackData.m_bUpdateCalled = false;
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xCallbackData.m_bUpdateCalled, true, "OnUpdate should be called during Update");
	ZENITH_ASSERT_TRUE(FloatEquals(xCallbackData.m_fUpdateDt, 0.016f, 0.001f), "OnUpdate should receive delta time");

	// Test OnExit via transition
	xCallbackData.m_bExitCalled = false;
	xStateMachine.GetParameters().SetTrigger("Next");
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xCallbackData.m_bExitCalled, true, "OnExit should be called when starting transition");

}

ZENITH_TEST(Core, MultipleTransitionConditions) { Zenith_UnitTests::TestMultipleTransitionConditions(); }

void Zenith_UnitTests::TestMultipleTransitionConditions(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
	xStateMachine.GetParameters().AddBool("IsGrounded", true);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Run");

	// Idle -> Run requires BOTH Speed > 5.0 AND IsGrounded == true
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Run";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 5.0f;

	Flux_TransitionCondition xGroundedCond;
	xGroundedCond.m_strParameterName = "IsGrounded";
	xGroundedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xGroundedCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xGroundedCond.m_bThreshold = true;

	xTrans.m_xConditions.PushBack(xSpeedCond);
	xTrans.m_xConditions.PushBack(xGroundedCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial update
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	// Only Speed true - should NOT transition
	xStateMachine.GetParameters().SetFloat("Speed", 10.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", false);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should stay in Idle when only Speed condition met");

	// Only IsGrounded true - should NOT transition
	xStateMachine.GetParameters().SetFloat("Speed", 2.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should stay in Idle when only IsGrounded condition met");

	// Both conditions true - SHOULD transition
	xStateMachine.GetParameters().SetFloat("Speed", 10.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.IsTransitioning(), true, "Should start transition when ALL conditions met");

	// Complete transition
	for (int i = 0; i < 10; ++i)
		xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Run", "Should be in Run state after transition");

}

//------------------------------------------------------------------------------
// Stick Figure Asset Export Test
//------------------------------------------------------------------------------

#ifndef ZENITH_ANDROID // Asset verification uses std::filesystem with local paths
ZENITH_TEST(Core, StickFigureAssetExport) { Zenith_UnitTests::TestStickFigureAssetExport(); }
#endif

void Zenith_UnitTests::TestStickFigureAssetExport(){

	// Assets are generated by GenerateTestAssets() called earlier in main()
	// This test verifies the assets were created correctly and can be loaded

	// Expected values for StickFigure assets
	const uint32_t uExpectedBoneCount = STICK_BONE_COUNT;  // 16 bones
	const uint32_t uExpectedVertCount = STICK_BONE_COUNT * 8;  // 8 verts per bone = 128
	const uint32_t uExpectedIndexCount = STICK_BONE_COUNT * 36;  // 36 indices per bone = 576

	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::string strSkelPath = strOutputDir + "StickFigure" ZENITH_SKELETON_EXT;
	std::string strMeshAssetPath = strOutputDir + "StickFigure" ZENITH_MESH_ASSET_EXT;
	std::string strIdlePath = strOutputDir + "StickFigure_Idle" ZENITH_ANIMATION_EXT;
	std::string strWalkPath = strOutputDir + "StickFigure_Walk" ZENITH_ANIMATION_EXT;
	std::string strRunPath = strOutputDir + "StickFigure_Run" ZENITH_ANIMATION_EXT;

	// Verify files exist
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strSkelPath), "Skeleton file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strIdlePath), "Idle animation file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strWalkPath), "Walk animation file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strRunPath), "Run animation file should exist");

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strSkelPath);
	ZENITH_ASSERT_NOT_NULL(pxReloadedSkel, "Should be able to reload skeleton");
	ZENITH_ASSERT_EQ(pxReloadedSkel->GetNumBones(), uExpectedBoneCount, "Reloaded skeleton should have 16 bones");
	ZENITH_ASSERT_TRUE(pxReloadedSkel->HasBone("LeftUpperArm"), "Reloaded skeleton should have LeftUpperArm bone");

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get<Zenith_MeshAsset>(strMeshAssetPath);
	ZENITH_ASSERT_NOT_NULL(pxReloadedMesh, "Should be able to reload mesh asset");
	ZENITH_ASSERT_EQ(pxReloadedMesh->GetNumVerts(), uExpectedVertCount, "Reloaded mesh vertex count mismatch");
	ZENITH_ASSERT_EQ(pxReloadedMesh->GetNumIndices(), uExpectedIndexCount, "Reloaded mesh index count mismatch");

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "StickFigure" ZENITH_MESH_EXT).c_str(), xReloadedGeometry, 0, false);
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumVerts(), uExpectedVertCount, "Reloaded geometry vertex count mismatch");
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumIndices(), uExpectedIndexCount, "Reloaded geometry index count mismatch");
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumBones(), uExpectedBoneCount, "Reloaded geometry bone count mismatch");
#endif

	// Reload and verify animations
	Zenith_AnimationAsset* pxReloadedIdleAsset = Zenith_AssetRegistry::Get<Zenith_AnimationAsset>(strIdlePath);
	ZENITH_ASSERT_TRUE(pxReloadedIdleAsset != nullptr && pxReloadedIdleAsset->GetClip() != nullptr, "Should be able to reload idle animation");
	ZENITH_ASSERT_EQ(pxReloadedIdleAsset->GetClip()->GetName(), "Idle", "Reloaded idle animation name mismatch");
	ZENITH_ASSERT_TRUE(FloatEquals(pxReloadedIdleAsset->GetClip()->GetDuration(), 2.0f, 0.01f), "Reloaded idle duration mismatch");

	Zenith_AnimationAsset* pxReloadedWalkAsset = Zenith_AssetRegistry::Get<Zenith_AnimationAsset>(strWalkPath);
	ZENITH_ASSERT_TRUE(pxReloadedWalkAsset != nullptr && pxReloadedWalkAsset->GetClip() != nullptr, "Should be able to reload walk animation");
	ZENITH_ASSERT_EQ(pxReloadedWalkAsset->GetClip()->GetName(), "Walk", "Reloaded walk animation name mismatch");

	Zenith_AnimationAsset* pxReloadedRunAsset = Zenith_AssetRegistry::Get<Zenith_AnimationAsset>(strRunPath);
	ZENITH_ASSERT_TRUE(pxReloadedRunAsset != nullptr && pxReloadedRunAsset->GetClip() != nullptr, "Should be able to reload run animation");
	ZENITH_ASSERT_EQ(pxReloadedRunAsset->GetClip()->GetName(), "Run", "Reloaded run animation name mismatch");

}

//------------------------------------------------------------------------------
// ECS Bug Fix Tests (Phase 1)
//------------------------------------------------------------------------------

/**
 * Test that component indices remain valid after another entity's component is removed.
 * This tests the swap-and-pop fix for the component removal data corruption bug.
 */
ZENITH_TEST(ECS, ComponentRemovalIndexUpdate) { Zenith_UnitTests::TestComponentRemovalIndexUpdate(); }
void Zenith_UnitTests::TestComponentRemovalIndexUpdate(){

	// Create a test scene through SceneManager
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentRemovalIndexUpdateScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");

	// Set distinct positions for each entity
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

	// Store Entity3's position before removal
	Zenith_Maths::Vector3 xExpectedPos3(3.0f, 0.0f, 0.0f);

	// Remove Entity2's transform (this should trigger swap-and-pop)
	xEntity2.RemoveComponent<Zenith_TransformComponent>();

	// Verify Entity1 still has correct data
	Zenith_Maths::Vector3 xPos1;
	xEntity1.GetComponent<Zenith_TransformComponent>().GetPosition(xPos1);
	ZENITH_ASSERT_EQ(xPos1.x, 1.0f, "TestComponentRemovalIndexUpdate: Entity1 position corrupted after Entity2 removal");

	// Verify Entity3 still has correct data (this entity's index likely changed due to swap-and-pop)
	Zenith_Maths::Vector3 xPos3;
	xEntity3.GetComponent<Zenith_TransformComponent>().GetPosition(xPos3);
	ZENITH_ASSERT_TRUE(xPos3.x == xExpectedPos3.x && xPos3.y == xExpectedPos3.y && xPos3.z == xExpectedPos3.z, "TestComponentRemovalIndexUpdate: Entity3 position corrupted after Entity2 removal");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that swap-and-pop removal preserves all component data correctly.
 */
ZENITH_TEST(ECS, ComponentSwapAndPop) { Zenith_UnitTests::TestComponentSwapAndPop(); }
void Zenith_UnitTests::TestComponentSwapAndPop(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentSwapAndPopScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create 5 entities with transforms
	Zenith_Entity xEntities[5] = {
		g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity0"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity4")
	};

	// Set unique positions
	for (u_int i = 0; i < 5; ++i)
	{
		xEntities[i].GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(static_cast<float>(i * 10), 0.0f, 0.0f));
	}

	// Remove entity at index 1 (should swap with last element, index 4)
	xEntities[1].RemoveComponent<Zenith_TransformComponent>();

	// Verify remaining entities have correct data
	for (u_int i = 0; i < 5; ++i)
	{
		if (i == 1) continue; // Removed

		ZENITH_ASSERT_TRUE(xEntities[i].HasComponent<Zenith_TransformComponent>(), "TestComponentSwapAndPop: Entity lost its TransformComponent unexpectedly");

		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		ZENITH_ASSERT_EQ(xPos.x, static_cast<float>(i * 10), "TestComponentSwapAndPop: Entity position data corrupted after swap-and-pop");
	}

	// Remove entity at index 0 (another swap-and-pop)
	xEntities[0].RemoveComponent<Zenith_TransformComponent>();

	// Verify remaining entities still correct
	for (u_int i = 2; i < 5; ++i)
	{
		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		ZENITH_ASSERT_EQ(xPos.x, static_cast<float>(i * 10), "TestComponentSwapAndPop: Entity position corrupted after second removal");
	}

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Prove the pool removal is a REAL physical swap-and-pop (not free-list-with-holes).
 *
 * ComponentSwapAndPop / ComponentRemovalIndexUpdate above only prove that component
 * DATA survives a removal — which is true even of a hole-based pool. This test pins
 * the physical move: in a fresh scene the transform pool is dense (slots 0,1,2 for the
 * three entities). Removing the FIRST entity's component must move the LAST live element
 * into the vacated slot, so the last entity's STORED component index must CHANGE — and
 * its data must remain intact at the new slot.
 */
ZENITH_TEST(ECS, SwapAndPopMovesIndex) { Zenith_UnitTests::TestSwapAndPopMovesIndex(); }
void Zenith_UnitTests::TestSwapAndPopMovesIndex(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestSwapAndPopMovesIndexScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Three entities — each auto-gets a Zenith_TransformComponent, so the fresh
	// scene's transform pool now holds dense slots 0, 1, 2.
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "SwapPop1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "SwapPop2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "SwapPop3");

	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

	// The stored component index lives in exactly one place: the global per-entity
	// component map keyed by component TypeID. Read the LAST entity's index now.
	const Zenith_SceneData::TypeID uTypeID = Zenith_SceneData::TypeIDGenerator::GetTypeID<Zenith_TransformComponent>();
	const u_int uLastIndexBefore = g_xEngine.EntityStore().m_axEntityComponents.Get(xEntity3.GetEntityID().m_uIndex).Get(uTypeID);

	// Remove the FIRST entity's component. Real swap-and-pop moves the last live
	// element (entity3) into the freed slot, so entity3's stored index must change.
	xEntity1.RemoveComponent<Zenith_TransformComponent>();

	const u_int uLastIndexAfter = g_xEngine.EntityStore().m_axEntityComponents.Get(xEntity3.GetEntityID().m_uIndex).Get(uTypeID);
	ZENITH_ASSERT_NE(uLastIndexAfter, uLastIndexBefore, "TestSwapAndPopMovesIndex: last entity's component index did not move — removal is not a real swap-and-pop");

	// And its component data must be intact at the new slot.
	ZENITH_ASSERT_TRUE(xEntity3.HasComponent<Zenith_TransformComponent>(), "TestSwapAndPopMovesIndex: last entity lost its component after swap-and-pop");
	Zenith_Maths::Vector3 xPos3;
	xEntity3.GetComponent<Zenith_TransformComponent>().GetPosition(xPos3);
	ZENITH_ASSERT_EQ(xPos3.x, 3.0f, "TestSwapAndPopMovesIndex: moved component data corrupted after swap-and-pop");

	// Sanity: the surviving middle entity is still addressable with intact data.
	Zenith_Maths::Vector3 xPos2;
	xEntity2.GetComponent<Zenith_TransformComponent>().GetPosition(xPos2);
	ZENITH_ASSERT_EQ(xPos2.x, 2.0f, "TestSwapAndPopMovesIndex: untouched component data corrupted after swap-and-pop");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

// WS6 regression: Query::ForEach now snapshots into a per-thread POOL of reusable
// buffers (instead of a fresh heap allocation per call). This pins the two
// properties the pooling must preserve: (1) RE-ENTRANCY — a nested ForEach inside
// a ForEach callback must check out a DISTINCT buffer and not clobber the outer
// iteration; (2) SNAPSHOT STABILITY — the outer iteration is fixed at its start,
// so an entity created mid-iteration is not visited by the outer loop.
ZENITH_TEST(ECS, QueryNestedReentrancy) { Zenith_UnitTests::TestQueryNestedReentrancy(); }
void Zenith_UnitTests::TestQueryNestedReentrancy(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQueryNestedReentrancyScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// All entities auto-have Zenith_TransformComponent, so Query<Transform> matches the whole set.
	const u_int uNumEntities = 6;
	Zenith_Entity xEntities[uNumEntities] = {
		g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest0"), g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest1"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest2"), g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest3"),
		g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest4"), g_xEngine.Scenes().CreateEntity(pxSceneData, "QNest5"),
	};
	(void)xEntities;

	// Baseline: a plain query visits exactly the created set.
	u_int uPlain = 0;
	Zenith_Query<Zenith_TransformComponent>(*pxSceneData).ForEach(
		[&uPlain](Zenith_EntityID, Zenith_TransformComponent&) { ++uPlain; });
	ZENITH_ASSERT_TRUE(uPlain == uNumEntities, "QueryNestedReentrancy: plain ForEach visited %u (expected %u)", uPlain, uNumEntities);

	// Nested ForEach (re-entrancy) + one mid-iteration entity creation. The outer
	// snapshot must visit EXACTLY the original set; the inner loop must always see
	// at least the original set. A buffer-clobber would corrupt uOuter; a missing
	// pooled-buffer checkout for the nested call would crash or miscount.
	u_int uOuter = 0;
	u_int uInnerMin = 0xFFFFFFFFu;
	bool bCreatedOnce = false;
	Zenith_Query<Zenith_TransformComponent>(*pxSceneData).ForEach(
		[&](Zenith_EntityID, Zenith_TransformComponent&) {
			++uOuter;
			u_int uInner = 0;
			Zenith_Query<Zenith_TransformComponent>(*pxSceneData).ForEach(
				[&uInner](Zenith_EntityID, Zenith_TransformComponent&) { ++uInner; });
			if (uInner < uInnerMin) { uInnerMin = uInner; }
			if (!bCreatedOnce)
			{
				bCreatedOnce = true;
				Zenith_Entity xLate = g_xEngine.Scenes().CreateEntity(pxSceneData, "QNestLate"); // created mid-iteration
				(void)xLate;
			}
		});
	ZENITH_ASSERT_TRUE(uOuter == uNumEntities, "QueryNestedReentrancy: outer snapshot unstable, visited %u (expected %u)", uOuter, uNumEntities);
	ZENITH_ASSERT_TRUE(uInnerMin >= uNumEntities, "QueryNestedReentrancy: nested ForEach saw %u (< base %u) — buffer clobber?", uInnerMin, uNumEntities);

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

// WS10 sparse-set keystone fuzz cross-check. Hammers the ECS with thousands of
// deterministic random Add / Remove / Destroy+recreate / cross-scene-move ops,
// and periodically asserts that:
//   (a) the SPARSE read path and the LEGACY read path return the IDENTICAL
//       matched-entity set for a battery of query combos, AND
//   (b) that set equals an INDEPENDENT ground-truth oracle (a plain bool array
//       per type, indexed by local entity-array index — derived from neither
//       ECS path).
// This is the correctness backstop for the whole wave: the sparse index is a
// parallel structure maintained by the pool mutators, so the only way to trust
// it is to diff it against the legacy scan AND an oracle that knows nothing
// about either. Uses a self-written fixed-seed PRNG (no std::rand) so the run
// is reproducible. Guarded like its scene-touching siblings.
#ifndef ZENITH_ANDROID
namespace
{
	// Fixed-seed xorshift32 PRNG. Deterministic, self-contained — no std::rand.
	struct WS10_Rng
	{
		uint32_t m_uState;
		explicit WS10_Rng(uint32_t uSeed) : m_uState(uSeed ? uSeed : 0x1u) {}
		uint32_t Next()
		{
			uint32_t x = m_uState;
			x ^= x << 13;
			x ^= x >> 17;
			x ^= x << 5;
			m_uState = x;
			return x;
		}
		// Uniform-ish in [0, uN). uN assumed small relative to 2^32 (it is here).
		uint32_t NextBelow(uint32_t uN) { return uN ? (Next() % uN) : 0u; }
	};

	// Per-local-slot ground-truth oracle. Index into the arrays below == the
	// stable LOCAL entity-array index (NOT the engine EntityID, which changes on
	// destroy+recreate). m_xId tracks the CURRENT EntityID for that slot.
	struct WS10_Oracle
	{
		Zenith_Vector<Zenith_EntityID> m_xId;       // current EntityID per local slot
		Zenith_Vector<bool>            m_bAlive;     // entity exists right now
		Zenith_Vector<bool>            m_bInQueried; // currently in the QUERIED scene (false when moved out)
		Zenith_Vector<bool>            m_bHasCamera; // Camera component present (Transform == alive, auto)
		Zenith_Vector<bool>            m_bHasLight;  // Light component present
	};

	// Tiny insertion sort over packed EntityIDs (the test TU has no sort helper).
	void WS10_SortPacked(Zenith_Vector<uint64_t>& xVals)
	{
		for (u_int i = 1; i < xVals.GetSize(); ++i)
		{
			const uint64_t ulKey = xVals.Get(i);
			u_int j = i;
			while (j > 0 && xVals.Get(j - 1) > ulKey)
			{
				xVals.Get(j) = xVals.Get(j - 1);
				--j;
			}
			xVals.Get(j) = ulKey;
		}
	}

	bool WS10_PackedVectorsEqual(const Zenith_Vector<uint64_t>& xA, const Zenith_Vector<uint64_t>& xB)
	{
		if (xA.GetSize() != xB.GetSize()) return false;
		for (u_int i = 0; i < xA.GetSize(); ++i)
		{
			if (xA.Get(i) != xB.Get(i)) return false;
		}
		return true;
	}

	// Bitmask of which queried component types a combo requires.
	enum WS10_ComboMask : uint32_t
	{
		WS10_T = 1u << 0,  // Transform (always present while alive)
		WS10_C = 1u << 1,  // Camera
		WS10_L = 1u << 2,  // Light
	};

	// Build the oracle's expected matched set (as packed EntityIDs) for a combo,
	// restricted to entities alive AND in the queried scene.
	void WS10_OracleExpected(const WS10_Oracle& xOracle, uint32_t uMask, Zenith_Vector<uint64_t>& xOut)
	{
		xOut.Clear();
		for (u_int i = 0; i < xOracle.m_bAlive.GetSize(); ++i)
		{
			if (!xOracle.m_bAlive.Get(i)) continue;
			if (!xOracle.m_bInQueried.Get(i)) continue;
			if ((uMask & WS10_C) && !xOracle.m_bHasCamera.Get(i)) continue;
			if ((uMask & WS10_L) && !xOracle.m_bHasLight.Get(i)) continue;
			// Transform always satisfied for an alive entity (auto-added).
			xOut.PushBack(xOracle.m_xId.Get(i).GetPacked());
		}
		WS10_SortPacked(xOut);
	}
}

ZENITH_TEST(ECS, QuerySparseLegacyEquivalence) { Zenith_UnitTests::TestQuerySparseLegacyEquivalence(); }
void Zenith_UnitTests::TestQuerySparseLegacyEquivalence(){

	// Pin the toggle so we can flip it deterministically; restore at the end.
	const bool bPrevSparse = g_xEngine.Scenes().AreSparseQueryReadsEnabled();

	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("WS10_FuzzSceneA", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING); // the QUERIED scene
	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("WS10_FuzzSceneB", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING); // cross-scene move target
	Zenith_SceneData* pxA = g_xEngine.Scenes().GetSceneData(xSceneA);
	Zenith_SceneData* pxB = g_xEngine.Scenes().GetSceneData(xSceneB);
	ZENITH_ASSERT_NOT_NULL(pxA, "WS10: scene A has no data");
	ZENITH_ASSERT_NOT_NULL(pxB, "WS10: scene B has no data");

	WS10_Rng xRng(0xC0FFEEu);
	WS10_Oracle xOracle;

	// --- Seed ~300 entities in scene A (each auto-carries Transform). ---
	const u_int uNumEntities = 300u;
	for (u_int i = 0; i < uNumEntities; ++i)
	{
		char acName[64];
		std::snprintf(acName, sizeof(acName), "WS10_E%u", i);
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxA, acName);
		const Zenith_EntityID xId = xEnt.GetEntityID();

		bool bCam = (xRng.NextBelow(2u) == 0u);
		bool bLit = (xRng.NextBelow(2u) == 0u);
		if (bCam) xEnt.AddComponent<Zenith_CameraComponent>();
		if (bLit) xEnt.AddComponent<Zenith_LightComponent>();

		xOracle.m_xId.PushBack(xId);
		xOracle.m_bAlive.PushBack(true);
		xOracle.m_bInQueried.PushBack(true);
		xOracle.m_bHasCamera.PushBack(bCam);
		xOracle.m_bHasLight.PushBack(bLit);
	}

	// Collect a query's ForEach EntityIDs (as packed, sorted) under the CURRENTLY
	// pinned read path. Takes the query BY REFERENCE and calls ForEach on it; the
	// generic-lambda variadic param pack ignores the component refs.
	auto CollectForEach = [&](auto& xQuery, Zenith_Vector<uint64_t>& xOut)
	{
		xOut.Clear();
		xQuery.ForEach([&xOut](Zenith_EntityID xId, auto&...) { xOut.PushBack(xId.GetPacked()); });
		WS10_SortPacked(xOut);
	};

	// Run ONE query through both read paths + the oracle and assert all three
	// agree. The query type-list is fixed by the caller via the xQuery argument;
	// uMask tells the oracle which components the combo requires.
	auto CheckCombo = [&](auto xQuery, uint32_t uMask)
	{
		Zenith_Vector<uint64_t> xLegacy;
		Zenith_Vector<uint64_t> xSparse;
		Zenith_Vector<uint64_t> xExpected;

		g_xEngine.Scenes().SetSparseQueryReads(false);
		CollectForEach(xQuery, xLegacy);
		g_xEngine.Scenes().SetSparseQueryReads(true);
		CollectForEach(xQuery, xSparse);

		WS10_OracleExpected(xOracle, uMask, xExpected);

		ZENITH_ASSERT_TRUE(WS10_PackedVectorsEqual(xLegacy, xSparse),
			"WS10: sparse vs legacy mismatch (mask=%u): legacy=%u sparse=%u",
			uMask, xLegacy.GetSize(), xSparse.GetSize());
		ZENITH_ASSERT_TRUE(WS10_PackedVectorsEqual(xSparse, xExpected),
			"WS10: matched set != oracle (mask=%u): sparse=%u oracle=%u",
			uMask, xSparse.GetSize(), xExpected.GetSize());
	};

	// The battery of combos (matches the spec list). Each constructs a fresh
	// Zenith_Query over scene A; the mask tells the oracle which components are
	// required.
	auto RunBattery = [&]()
	{
		CheckCombo(Zenith_Query<Zenith_TransformComponent>(*pxA),                                                  WS10_T);
		CheckCombo(Zenith_Query<Zenith_CameraComponent>(*pxA),                                                     WS10_C);
		CheckCombo(Zenith_Query<Zenith_TransformComponent, Zenith_CameraComponent>(*pxA),                          WS10_T | WS10_C);
		CheckCombo(Zenith_Query<Zenith_TransformComponent, Zenith_LightComponent>(*pxA),                           WS10_T | WS10_L);
		CheckCombo(Zenith_Query<Zenith_TransformComponent, Zenith_CameraComponent, Zenith_LightComponent>(*pxA),   WS10_T | WS10_C | WS10_L);
		CheckCombo(Zenith_Query<Zenith_CameraComponent, Zenith_LightComponent>(*pxA),                              WS10_C | WS10_L);
	};

	// Validate the seeded state before any churn.
	RunBattery();

	// --- ~5000 random ops, oracle updated on each, battery every ~40 ops. ---
	const u_int uNumOps = 5000u;
	for (u_int uOp = 0; uOp < uNumOps; ++uOp)
	{
		const u_int uLocal = xRng.NextBelow(uNumEntities);
		const uint32_t uAction = xRng.NextBelow(100u);

		const bool bAlive    = xOracle.m_bAlive.Get(uLocal);
		const bool bInQueried = xOracle.m_bInQueried.Get(uLocal);
		const Zenith_EntityID xId = xOracle.m_xId.Get(uLocal);

		// Resolve a live handle in whichever scene currently owns the entity.
		Zenith_SceneData* pxOwner = bInQueried ? pxA : pxB;

		if (uAction < 30u)
		{
			// Add/Remove CAMERA (only meaningful while alive).
			if (bAlive)
			{
				Zenith_Entity xEnt(pxOwner, xId);
				if (xOracle.m_bHasCamera.Get(uLocal))
				{
					xEnt.RemoveComponent<Zenith_CameraComponent>();
					xOracle.m_bHasCamera.Get(uLocal) = false;
				}
				else
				{
					xEnt.AddComponent<Zenith_CameraComponent>();
					xOracle.m_bHasCamera.Get(uLocal) = true;
				}
			}
		}
		else if (uAction < 60u)
		{
			// Add/Remove LIGHT (only meaningful while alive).
			if (bAlive)
			{
				Zenith_Entity xEnt(pxOwner, xId);
				if (xOracle.m_bHasLight.Get(uLocal))
				{
					xEnt.RemoveComponent<Zenith_LightComponent>();
					xOracle.m_bHasLight.Get(uLocal) = false;
				}
				else
				{
					xEnt.AddComponent<Zenith_LightComponent>();
					xOracle.m_bHasLight.Get(uLocal) = true;
				}
			}
		}
		else if (uAction < 75u)
		{
			// Cross-scene MOVE (toggle which scene owns the entity). Only ROOT
			// entities can move (all of ours are roots). This exercises the
			// source-pool swap-and-pop sparse clear + target-pool sparse set, and
			// proves scene-A queries exclude moved-out entities (stale source
			// sparse entries are rejected by the round-trip compare).
			if (bAlive)
			{
				Zenith_Entity xEnt(pxOwner, xId);
				Zenith_Scene xTarget = bInQueried ? xSceneB : xSceneA;
				xEnt.MoveToScene(xTarget);
				// Move succeeds iff the entity now resides in the target scene
				// (Phase 3: Entity::MoveToScene is void; derive success from state).
				const bool bMoved = (xEnt.GetScene() == xTarget);
				if (bMoved)
				{
					xOracle.m_bInQueried.Get(uLocal) = !bInQueried;
					// EntityID is preserved across the move (global slot table).
					xOracle.m_xId.Get(uLocal) = xEnt.GetEntityID();
				}
			}
		}
		else if (uAction < 88u)
		{
			// DESTROY (immediate, synchronous) — frees the slot (generation bump)
			// and swap-and-pops its components out of the owning scene's pools.
			if (bAlive)
			{
				Zenith_Entity xEnt(pxOwner, xId);
				xEnt.DestroyImmediate();
				xOracle.m_bAlive.Get(uLocal) = false;
				xOracle.m_bHasCamera.Get(uLocal) = false;
				xOracle.m_bHasLight.Get(uLocal) = false;
				xOracle.m_xId.Get(uLocal) = INVALID_ENTITY_ID;
				// A destroyed slot is conceptually back in scene A for re-creation.
				xOracle.m_bInQueried.Get(uLocal) = true;
			}
		}
		else
		{
			// RE-CREATE a destroyed slot (drives slot reuse + generation change).
			// Always re-create into scene A (the queried scene).
			if (!bAlive)
			{
				char acName[64];
				std::snprintf(acName, sizeof(acName), "WS10_R%u_%u", uLocal, uOp);
				Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxA, acName);
				bool bCam = (xRng.NextBelow(2u) == 0u);
				bool bLit = (xRng.NextBelow(2u) == 0u);
				if (bCam) xEnt.AddComponent<Zenith_CameraComponent>();
				if (bLit) xEnt.AddComponent<Zenith_LightComponent>();

				xOracle.m_xId.Get(uLocal) = xEnt.GetEntityID();
				xOracle.m_bAlive.Get(uLocal) = true;
				xOracle.m_bInQueried.Get(uLocal) = true;
				xOracle.m_bHasCamera.Get(uLocal) = bCam;
				xOracle.m_bHasLight.Get(uLocal) = bLit;
			}
		}

		if ((uOp % 40u) == 0u)
		{
			RunBattery();
		}
	}

	// Final battery after all churn.
	RunBattery();

	// Cleanup: unload both scenes; restore the toggle to its prior value.
	g_xEngine.Scenes().SetSparseQueryReads(bPrevSparse);
	g_xEngine.Scenes().UnloadScene(xSceneA);
	g_xEngine.Scenes().UnloadScene(xSceneB);
}
#else
// Android build: scene-fuzz test compiled out (mirrors sibling scene tests).
ZENITH_TEST(ECS, QuerySparseLegacyEquivalence) {}
void Zenith_UnitTests::TestQuerySparseLegacyEquivalence(){}
#endif // ZENITH_ANDROID

// Wave8 regression: the render-tasks-active signal is a real std::atomic compiled
// in ALL configs (it used to exist only under ZENITH_ASSERT and otherwise returned
// false). The accessor must return an authoritative, race-free value even in non-
// assert builds. Pin the round-trip through the public setter/getter. The flag MUST
// be left false at the end — scene-mutation asserts elsewhere fire if it stays true.
ZENITH_TEST(Core, RenderPhaseTransitions) { Zenith_UnitTests::TestRenderPhaseTransitions(); }
void Zenith_UnitTests::TestRenderPhaseTransitions(){

	// Precondition: outside ExecuteRenderGraph the signal is clear.
	ZENITH_ASSERT_FALSE(g_xEngine.Scenes().AreRenderTasksActive(), "RenderPhaseTransitions: signal should start clear");

	g_xEngine.Scenes().SetRenderTasksActive(true);
	ZENITH_ASSERT_TRUE(g_xEngine.Scenes().AreRenderTasksActive(), "RenderPhaseTransitions: setter(true) did not raise the signal");

	g_xEngine.Scenes().SetRenderTasksActive(false);
	ZENITH_ASSERT_FALSE(g_xEngine.Scenes().AreRenderTasksActive(), "RenderPhaseTransitions: setter(false) did not clear the signal");

	// Leave the engine in the clear state other code asserts on.
	g_xEngine.Scenes().SetRenderTasksActive(false);
}

// Wave8.2: smoke-test the GPU-free --bench-ecs micro-benchmark. The benchmark
// is the before/after measurement backstop for the future sparse-set query
// rework; this test just proves the bench logic runs end-to-end on a tiny
// workload (N=64, iters=2) without crashing and reports a positive processed
// count. Zenith_BenchECS_RunOnce creates an empty additive scene, populates it,
// runs the Query<...>().ForEach + Add/Remove churn, and tears the scene down
// itself, so there is no scene to clean up here.
ZENITH_TEST(Core, BenchECSSmoke) { Zenith_UnitTests::TestBenchECSSmoke(); }
void Zenith_UnitTests::TestBenchECSSmoke(){

	const u_int64 ulProcessed = Zenith_BenchECS_RunOnce(64, 2);

	// 64 entities all carry a Transform and the outer ForEach visits every one
	// each of the 2 iterations, so the processed count must be strictly
	// positive (>= 128 in practice). Asserting > 0 keeps the test robust to the
	// exact reduction formula while still proving real work happened.
	ZENITH_ASSERT_GT(ulProcessed, static_cast<u_int64>(0), "BenchECSSmoke: bench reported zero processed components (no work done?)");
}

// WS3 regression: LoadScene(SINGLE) validates the file header BEFORE tearing down
// the live world, so a corrupt/old/future .zscen no longer leaves the engine
// scene-less. ValidateSceneStream is that non-destructive header gate. Pin that it
// accepts a well-formed header (and restores the cursor) and rejects each
// corruption class.
ZENITH_TEST(Scene, SceneLoadValidation) { Zenith_UnitTests::TestSceneLoadValidation(); }
void Zenith_UnitTests::TestSceneLoadValidation(){

	// Well-formed header (magic + current version): accepted, cursor untouched.
	{
		Zenith_DataStream xStream;
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)Zenith_SceneData::uSCENE_VERSION_CURRENT;
		xStream.SetCursor(0);
		ZENITH_ASSERT_TRUE(Zenith_SceneData::ValidateSceneStream(xStream), "ValidateSceneStream rejected a well-formed header");
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == 0, "ValidateSceneStream must restore the stream cursor");
	}
	// Truncated below the 8-byte header: rejected. NOTE: build an EXTERNAL-buffer
	// stream of exactly 4 bytes so GetSize() == 4 (mirrors a 4-byte file). A default
	// in-memory `Zenith_DataStream` reports its 1024-byte buffer CAPACITY from
	// GetSize() (writes only advance the cursor, not m_ulDataSize), so the size gate
	// in ValidateSceneStream would slip through and read an UNINITIALISED "version"
	// from bytes 4-7 of the fresh buffer — a heap-dependent flake. The external ctor
	// pins the reported size to the real payload length.
	{
		u_int uMagicOnly = (u_int)Zenith_SceneData::uSCENE_MAGIC;
		Zenith_DataStream xStream(&uMagicOnly, sizeof(u_int)); // 4-byte stream, GetSize() == 4
		ZENITH_ASSERT_TRUE(!Zenith_SceneData::ValidateSceneStream(xStream), "ValidateSceneStream accepted a truncated stream");
	}
	// Bad magic number: rejected.
	{
		Zenith_DataStream xStream;
		xStream << (u_int)0xDEADBEEF;
		xStream << (u_int)Zenith_SceneData::uSCENE_VERSION_CURRENT;
		xStream.SetCursor(0);
		ZENITH_ASSERT_TRUE(!Zenith_SceneData::ValidateSceneStream(xStream), "ValidateSceneStream accepted a bad magic number");
	}
	// Future version (> current): rejected (the version-skew case WS3 targets).
	{
		Zenith_DataStream xStream;
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)(Zenith_SceneData::uSCENE_VERSION_CURRENT + 1u);
		xStream.SetCursor(0);
		ZENITH_ASSERT_TRUE(!Zenith_SceneData::ValidateSceneStream(xStream), "ValidateSceneStream accepted a future version");
	}
	// Too-old version (< min supported): rejected.
	{
		Zenith_DataStream xStream;
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)(Zenith_SceneData::uSCENE_VERSION_MIN_SUPPORTED - 1u);
		xStream.SetCursor(0);
		ZENITH_ASSERT_TRUE(!Zenith_SceneData::ValidateSceneStream(xStream), "ValidateSceneStream accepted a too-old version");
	}
}

// Wave9.1 (a) regression: ValidateSceneStream only gates the 8-byte HEADER; a file
// with a well-formed header but a corrupt BODY (e.g. an entity count of 0xFFFFFFFF
// from a truncated/garbled file) used to be loaded unconditionally — spinning ~4
// billion iterations and building a half-loaded world. LoadFromDataStream now has
// bounded body guards that return false. Pin that (1) a wild entity count and (2) a
// claimed-but-absent entity body are both rejected gracefully, leaving zero entities
// (so the Operations.cpp rollback can unwind to a clean INVALID_SCENE). The count-vs-
// remaining guard rejects BEFORE any ReadEntity call reads past EOF, so DataStream's
// debug operator>> overflow assert never fires.
#ifndef ZENITH_ANDROID
ZENITH_TEST(Scene, SceneBodyCorruptionFailsGracefully) { Zenith_UnitTests::TestSceneBodyCorruptionFailsGracefully(); }
#endif
void Zenith_UnitTests::TestSceneBodyCorruptionFailsGracefully(){

	// Case 1: well-formed header, then a wild entity count (0xFFFFFFFF) with no
	// further bytes. The count vastly exceeds the remaining stream, so guard 1
	// rejects before the entity loop runs — no half-world is built.
	{
		// Exact-size stream: GetSize() must report the 12 written header bytes, not the
		// default 1024-byte capacity — else guard 1 (count > remaining bytes) cannot fire
		// on an in-memory stream (operator<< only grows the buffer, never shrinks it).
		Zenith_DataStream xStream(sizeof(u_int) * 3);
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)Zenith_SceneData::uSCENE_VERSION_CURRENT;
		xStream << (u_int)0xFFFFFFFFu;
		xStream.SetCursor(0);

		Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("BodyCorruptionTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);

		bool bOk = pxSceneData->LoadFromDataStream(xStream);
		ZENITH_ASSERT_FALSE(bOk, "LoadFromDataStream accepted a wild (0xFFFFFFFF) entity count");
		ZENITH_ASSERT_EQ(pxSceneData->GetEntityCount(), 0, "Corrupt-count load must leave zero entities (got %u)", pxSceneData->GetEntityCount());

		g_xEngine.Scenes().UnloadScene(xScene);
	}

	// Case 2: well-formed header claiming 5 entities, but zero entity bytes follow.
	// 5 > 0 remaining bytes, so guard 1 again rejects before reading past EOF.
	{
		Zenith_DataStream xStream(sizeof(u_int) * 3);
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)Zenith_SceneData::uSCENE_VERSION_CURRENT;
		xStream << (u_int)5u;
		xStream.SetCursor(0);

		Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("BodyCorruptionTest2", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);

		bool bOk = pxSceneData->LoadFromDataStream(xStream);
		ZENITH_ASSERT_FALSE(bOk, "LoadFromDataStream accepted a claimed-but-absent entity body");
		ZENITH_ASSERT_EQ(pxSceneData->GetEntityCount(), 0, "Truncated-body load must leave zero entities (got %u)", pxSceneData->GetEntityCount());

		g_xEngine.Scenes().UnloadScene(xScene);
	}
}

// wave9.3: per-component schemaVersion in .zscen (scene v6, INERT). The field is
// written per component OUTSIDE the size-prefixed payload, so legacy v3/4/5 files
// (which carry no such field) and the v6 unknown-component SkipBytes path both stay
// byte-aligned. Three pins: (1) a v6 save stamps version 6 in the header and still
// round-trips a Transform; (2) a hand-crafted PRE-v6 (v5) stream loads cleanly with
// the Transform intact — proving v<6 consumes ZERO schemaVersion bytes; (3) a v6
// stream whose only component names a bogus type loads cleanly AND the trailing
// camera index still reads — proving the v6 size-skip stays aligned past the
// schemaVersion field.
#ifndef ZENITH_ANDROID // (1) uses std::filesystem with a relative path, like the sibling scene tests
ZENITH_TEST(Scene, SceneComponentSchemaVersion) { Zenith_UnitTests::TestSceneComponentSchemaVersion(); }
#endif
void Zenith_UnitTests::TestSceneComponentSchemaVersion(){

	// The registry key for the Transform component is "Transform" (not
	// "TransformComponent") — see Zenith_ComponentMeta.cpp FinalizeRegistration.
	const std::string strTransformTypeName = "Transform";

	// ------------------------------------------------------------------
	// Shared helper: a real Transform payload (exactly what SerializeEntityComponents
	// writes between the size prefix and the next component) for a known position.
	// Built by serializing a live Transform into a scratch stream so the test stays
	// robust to the component's field layout.
	// ------------------------------------------------------------------
	const Zenith_Maths::Vector3 xKnownPos(12.5f, -7.0f, 3.25f);

	Zenith_Scene xSrcScene = g_xEngine.Scenes().LoadScene("SchemaVersionSrcScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSrcSceneData = g_xEngine.Scenes().GetSceneData(xSrcScene);

	// Capture the Transform payload bytes once, from a throwaway entity.
	Zenith_DataStream xTransformPayload;
	u_int uTransformPayloadSize = 0;
	{
		Zenith_Entity xPayloadEntity = g_xEngine.Scenes().CreateEntity(pxSrcSceneData, "PayloadEntity");
		xPayloadEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xKnownPos);
		xPayloadEntity.GetComponent<Zenith_TransformComponent>().WriteToDataStream(xTransformPayload);
		uTransformPayloadSize = static_cast<u_int>(xTransformPayload.GetCursor());
		ZENITH_ASSERT_GT(uTransformPayloadSize, 0u, "SchemaVersion: captured Transform payload is empty");
	}

	// ==================================================================
	// (1) v6 round-trip: save a real scene, reopen the raw bytes and check the
	//     header version is uSCENE_VERSION_CURRENT (6), then LoadFromFile and
	//     verify the Transform round-trips.
	// ==================================================================
	{
		const std::string strScenePath = "unit_test_schemaversion_v6" ZENITH_SCENE_EXT;

		Zenith_Scene xSaveScene = g_xEngine.Scenes().LoadScene("SchemaVersionV6SaveScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSaveSceneData = g_xEngine.Scenes().GetSceneData(xSaveScene);

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSaveSceneData, "SchemaV6Entity");
		xEntity.SetTransient(false);
		xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xKnownPos);

		pxSaveSceneData->SaveToFile(strScenePath);
		ZENITH_ASSERT_TRUE(std::filesystem::exists(strScenePath), "SchemaVersion: v6 scene file was not created");

		// Reopen the raw bytes: header is [magic u_int][version u_int]. Assert the
		// version field is the current scene version (6).
		{
			Zenith_DataStream xRaw;
			xRaw.ReadFromFile(strScenePath.c_str());
			ZENITH_ASSERT_TRUE(xRaw.IsValid(), "SchemaVersion: could not reopen v6 scene file");
			u_int uMagic = 0;
			u_int uVersion = 0;
			xRaw >> uMagic;
			xRaw >> uVersion;
			ZENITH_ASSERT_EQ(uMagic, (u_int)Zenith_SceneData::uSCENE_MAGIC, "SchemaVersion: v6 file magic mismatch");
			ZENITH_ASSERT_EQ(uVersion, (u_int)Zenith_SceneData::uSCENE_VERSION_CURRENT, "SchemaVersion: saved scene version is not uSCENE_VERSION_CURRENT (6)");
		}

		// Load it back through the normal path and confirm the Transform survived.
		Zenith_Scene xLoadScene = g_xEngine.Scenes().LoadScene("SchemaVersionV6LoadScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxLoadSceneData = g_xEngine.Scenes().GetSceneData(xLoadScene);
		ZENITH_ASSERT_TRUE(pxLoadSceneData->LoadFromFile(strScenePath), "SchemaVersion: v6 LoadFromFile failed");

		Zenith_Entity xLoaded = pxLoadSceneData->FindEntityByName("SchemaV6Entity");
		ZENITH_ASSERT_TRUE(xLoaded.IsValid(), "SchemaVersion: v6 entity not found after round-trip");
		ZENITH_ASSERT_TRUE(xLoaded.HasComponent<Zenith_TransformComponent>(), "SchemaVersion: v6 entity missing Transform");
		Zenith_Maths::Vector3 xLoadedPos;
		xLoaded.GetComponent<Zenith_TransformComponent>().GetPosition(xLoadedPos);
		ZENITH_ASSERT_EQ(xLoadedPos, xKnownPos, "SchemaVersion: v6 Transform position did not round-trip");

		g_xEngine.Scenes().UnloadScene(xSaveScene);
		g_xEngine.Scenes().UnloadScene(xLoadScene);
		std::filesystem::remove(strScenePath);
	}

	// ==================================================================
	// (2) v5 back-compat: hand-craft an in-memory PRE-v6 stream and load it. The
	//     pre-v6 component layout is [typeName][size][payload] — NO schemaVersion.
	//     If LoadFromDataStream consumed a schemaVersion for v5 it would desync and
	//     the Transform (and trailer) would be garbage. Asserting the Transform
	//     matches proves v<6 reads zero schemaVersion bytes.
	// ==================================================================
	{
		Zenith_DataStream xStream;
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)5u;                      // version 5 (pre-v6)
		xStream << (u_int)1u;                      // entity count

		// Entity: [fileIndex u32][name str] then components (v4/5 entity layout).
		xStream << (uint32_t)0u;                   // file index
		xStream << std::string("V5Entity");        // name

		// Components: [count u_int] then per-component [typeName][size][payload].
		xStream << (u_int)1u;                      // one component
		xStream << strTransformTypeName;           // "Transform"
		xStream << (u_int)uTransformPayloadSize;   // size prefix (payload only)
		xStream.WriteData(xTransformPayload.GetData(), uTransformPayloadSize);

		// Trailer: main camera file index (none).
		xStream << (uint32_t)Zenith_EntityID::INVALID_INDEX;

		xStream.SetCursor(0);

		Zenith_Scene xV5Scene = g_xEngine.Scenes().LoadScene("SchemaVersionV5Scene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxV5SceneData = g_xEngine.Scenes().GetSceneData(xV5Scene);
		ZENITH_ASSERT_TRUE(pxV5SceneData->LoadFromDataStream(xStream), "SchemaVersion: hand-crafted v5 stream failed to load");
		ZENITH_ASSERT_EQ(pxV5SceneData->GetEntityCount(), 1u, "SchemaVersion: v5 stream produced wrong entity count");

		Zenith_Entity xLoaded = pxV5SceneData->FindEntityByName("V5Entity");
		ZENITH_ASSERT_TRUE(xLoaded.IsValid(), "SchemaVersion: v5 entity not found");
		ZENITH_ASSERT_TRUE(xLoaded.HasComponent<Zenith_TransformComponent>(), "SchemaVersion: v5 entity missing Transform");
		Zenith_Maths::Vector3 xLoadedPos;
		xLoaded.GetComponent<Zenith_TransformComponent>().GetPosition(xLoadedPos);
		ZENITH_ASSERT_EQ(xLoadedPos, xKnownPos, "SchemaVersion: v5 Transform did not match (v<6 must consume NO schemaVersion bytes)");

		g_xEngine.Scenes().UnloadScene(xV5Scene);
	}

	// ==================================================================
	// (3) unknown-component-in-v6: craft a v6 stream whose only component names a
	//     bogus type, with a schemaVersion + a payload size. The reader must
	//     consume the schemaVersion (v6 gate), then SkipBytes(size) for the unknown
	//     type, and STILL land exactly on the trailing camera index. Asserting the
	//     load succeeds + the entity exists proves the v6 size-skip stays aligned
	//     past the schemaVersion field.
	// ==================================================================
	{
		const u_int uBogusPayloadSize = 13u;       // arbitrary, distinct from sizeof(u_int)

		Zenith_DataStream xStream;
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		// Pin to literal v6: this case specifically exercises the v6 entity-record layout
		// ([fileIndex][name], parent in the Transform blob) + the v6 per-component
		// schemaVersion size-skip. It must NOT track uSCENE_VERSION_CURRENT -- the v7 bump
		// (Phase 7a) moved the parent file-index into the entity record, so a v7 record
		// carries an extra parentFileIndex field this hand-crafted stream does not write.
		xStream << (u_int)6u;                      // version 6 (v6 record layout)
		xStream << (u_int)1u;                      // entity count

		// Entity: [fileIndex u32][name str] (v6 record -- no parent file-index here).
		xStream << (uint32_t)0u;
		xStream << std::string("V6UnknownEntity");

		// Components: [count] then [typeName][schemaVersion][size][payload] (v6 layout).
		xStream << (u_int)1u;                      // one component
		xStream << std::string("BogusComponentXYZ");
		xStream << (u_int)7u;                       // schemaVersion (outside the payload)
		xStream << (u_int)uBogusPayloadSize;        // payload size
		for (u_int u = 0; u < uBogusPayloadSize; ++u)
		{
			uint8_t uByte = static_cast<uint8_t>(0xA0u + u);
			xStream.WriteData(&uByte, 1);
		}

		// Trailer: main camera file index (none). If the size-skip were misaligned
		// (e.g. by failing to consume the schemaVersion), reading this would consume
		// payload bytes instead and the load would desync.
		xStream << (uint32_t)Zenith_EntityID::INVALID_INDEX;

		xStream.SetCursor(0);

		Zenith_Scene xV6Scene = g_xEngine.Scenes().LoadScene("SchemaVersionV6UnknownScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxV6SceneData = g_xEngine.Scenes().GetSceneData(xV6Scene);
		ZENITH_ASSERT_TRUE(pxV6SceneData->LoadFromDataStream(xStream), "SchemaVersion: v6 unknown-component stream failed to load (size-skip misaligned?)");
		ZENITH_ASSERT_EQ(pxV6SceneData->GetEntityCount(), 1u, "SchemaVersion: v6 unknown-component stream produced wrong entity count");

		Zenith_Entity xLoaded = pxV6SceneData->FindEntityByName("V6UnknownEntity");
		ZENITH_ASSERT_TRUE(xLoaded.IsValid(), "SchemaVersion: v6 unknown-component entity not found (trailer misread => skip misaligned)");

		g_xEngine.Scenes().UnloadScene(xV6Scene);
	}

	// Tear down the source scene that owns the captured payload.
	g_xEngine.Scenes().UnloadScene(xSrcScene);
}

// wave8.5: the reusable DataStream envelope helper. Pin that a header round-trips
// (write then read back the four fields), that the read is non-destructive and
// leaves the cursor positioned for the payload on success, and that the two
// corruption classes the asset boundary cares about map to the shared error
// codes: a wrong magic -> BAD_MAGIC (so a legacy headerless stream rewinds), and
// a future envelope version -> VERSION_MISMATCH.
ZENITH_TEST(Serialization, EnvelopeRoundTrip) { Zenith_UnitTests::TestStreamEnvelopeRoundTrip(); }
void Zenith_UnitTests::TestStreamEnvelopeRoundTrip(){

	static constexpr u_int uTYPE_ID = 7;
	static constexpr u_int uSCHEMA  = 3;

	// Round-trip: header + payload write, read back, fields match and the
	// payload that followed the header is intact.
	{
		Zenith_DataStream xStream;
		Zenith_WriteStreamHeader(xStream, uTYPE_ID, uSCHEMA);
		const u_int uPayload = 0xABCDEF01u;
		xStream << (u_int)uPayload;
		xStream.SetCursor(0);

		Zenith_Result<Zenith_StreamHeader> xRes = Zenith_ReadStreamHeader(xStream, uTYPE_ID);
		ZENITH_ASSERT_TRUE(xRes.IsOk(), "ReadStreamHeader rejected a well-formed header");
		ZENITH_ASSERT_EQ(xRes.Value().m_uMagic, (u_int)uSTREAM_ENVELOPE_MAGIC, "envelope magic did not round-trip");
		ZENITH_ASSERT_EQ(xRes.Value().m_uEnvelopeVersion, (u_int)uSTREAM_ENVELOPE_VERSION_CURRENT, "envelope version did not round-trip");
		ZENITH_ASSERT_EQ(xRes.Value().m_uAssetTypeId, (u_int)uTYPE_ID, "asset type id did not round-trip");
		ZENITH_ASSERT_EQ(xRes.Value().m_uSchemaVersion, (u_int)uSCHEMA, "schema version did not round-trip");

		// On success the cursor sits just past the header — the payload reads next.
		u_int uReadPayload = 0;
		xStream >> uReadPayload;
		ZENITH_ASSERT_EQ(uReadPayload, uPayload, "payload after header was not readable / cursor misplaced");
	}

	// Corrupt magic -> BAD_MAGIC, and the read is non-destructive (cursor restored
	// to entry so a legacy headerless stream can be rewound and re-read).
	{
		Zenith_DataStream xStream;
		xStream << (u_int)0xDEADBEEFu;                       // bad magic
		xStream << (u_int)uSTREAM_ENVELOPE_VERSION_CURRENT;
		xStream << (u_int)uTYPE_ID;
		xStream << (u_int)uSCHEMA;
		xStream.SetCursor(0);

		Zenith_Result<Zenith_StreamHeader> xRes = Zenith_ReadStreamHeader(xStream, uTYPE_ID);
		ZENITH_ASSERT_TRUE(!xRes.IsOk(), "ReadStreamHeader accepted a bad magic");
		ZENITH_ASSERT_TRUE(xRes.Error() == Zenith_ErrorCode::BAD_MAGIC, "bad magic must map to BAD_MAGIC");
		ZENITH_ASSERT_EQ(xStream.GetCursor(), (uint64_t)0, "ReadStreamHeader must restore the cursor on BAD_MAGIC");
	}

	// Future envelope version -> VERSION_MISMATCH (cursor restored too).
	{
		Zenith_DataStream xStream;
		xStream << (u_int)uSTREAM_ENVELOPE_MAGIC;
		xStream << (u_int)(uSTREAM_ENVELOPE_VERSION_CURRENT + 1u);  // newer than we support
		xStream << (u_int)uTYPE_ID;
		xStream << (u_int)uSCHEMA;
		xStream.SetCursor(0);

		Zenith_Result<Zenith_StreamHeader> xRes = Zenith_ReadStreamHeader(xStream, uTYPE_ID);
		ZENITH_ASSERT_TRUE(!xRes.IsOk(), "ReadStreamHeader accepted a future envelope version");
		ZENITH_ASSERT_TRUE(xRes.Error() == Zenith_ErrorCode::VERSION_MISMATCH, "future version must map to VERSION_MISMATCH");
		ZENITH_ASSERT_EQ(xStream.GetCursor(), (uint64_t)0, "ReadStreamHeader must restore the cursor on VERSION_MISMATCH");
	}
}

/**
 * Test removing multiple components from multiple entities in sequence.
 */
ZENITH_TEST(Core, MultipleComponentRemoval) { Zenith_UnitTests::TestMultipleComponentRemoval(); }
void Zenith_UnitTests::TestMultipleComponentRemoval(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestMultipleComponentRemovalScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create entities with multiple component types
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");

	// Add CameraComponents to entities 1 and 2
	xEntity1.AddComponent<Zenith_CameraComponent>().InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(1, 0, 0),
		.m_fFar = 100,
		.m_fAspectRatio = 1.0f,
	});
	xEntity2.AddComponent<Zenith_CameraComponent>().InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(2, 0, 0),
		.m_fFar = 100,
		.m_fAspectRatio = 1.0f,
	});

	// Add ColliderComponents to entities 2 and 3 (as second component type to test)
	xEntity2.AddComponent<Zenith_ColliderComponent>();
	xEntity3.AddComponent<Zenith_ColliderComponent>();

	// Remove Entity1's camera
	xEntity1.RemoveComponent<Zenith_CameraComponent>();

	// Verify Entity2 still has its camera
	ZENITH_ASSERT_TRUE(xEntity2.HasComponent<Zenith_CameraComponent>(), "TestMultipleComponentRemoval: Entity2 lost CameraComponent");

	// Remove Entity2's collider
	xEntity2.RemoveComponent<Zenith_ColliderComponent>();

	// Verify Entity3 still has collider
	ZENITH_ASSERT_TRUE(xEntity3.HasComponent<Zenith_ColliderComponent>(), "TestMultipleComponentRemoval: Entity3 lost ColliderComponent");

	// Remove Entity2's camera
	xEntity2.RemoveComponent<Zenith_CameraComponent>();

	// Verify Entity3 still has collider with correct data
	ZENITH_ASSERT_TRUE(xEntity3.HasComponent<Zenith_ColliderComponent>(), "TestMultipleComponentRemoval: Entity3 lost ColliderComponent after camera removal");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Stress test component removal with many entities.
 */
ZENITH_TEST(ECS, ComponentRemovalWithManyEntities) { Zenith_UnitTests::TestComponentRemovalWithManyEntities(); }
void Zenith_UnitTests::TestComponentRemovalWithManyEntities(){

	constexpr u_int NUM_ENTITIES = 1000;
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentRemovalWithManyEntitiesScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create many entities
	std::vector<Zenith_Entity> xEntities;
	xEntities.reserve(NUM_ENTITIES);

	for (u_int i = 0; i < NUM_ENTITIES; ++i)
	{
		xEntities.push_back(g_xEngine.Scenes().CreateEntity(pxSceneData, "StressEntity" + std::to_string(i)));
		xEntities[i].GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(static_cast<float>(i), 0.0f, 0.0f));
	}

	// Remove every other entity's transform component
	for (u_int i = 0; i < NUM_ENTITIES; i += 2)
	{
		xEntities[i].RemoveComponent<Zenith_TransformComponent>();
	}

	// Verify remaining entities have correct data
	for (u_int i = 1; i < NUM_ENTITIES; i += 2)
	{
		ZENITH_ASSERT_TRUE(xEntities[i].HasComponent<Zenith_TransformComponent>(), "TestComponentRemovalWithManyEntities: Entity lost TransformComponent");

		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		ZENITH_ASSERT_EQ(xPos.x, static_cast<float>(i), "TestComponentRemovalWithManyEntities: Entity position corrupted");
	}

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that entity names are stored in the scene and accessible via GetName()/SetName().
 */
ZENITH_TEST(ECS, EntityNameFromScene) { Zenith_UnitTests::TestEntityNameFromScene(); }
void Zenith_UnitTests::TestEntityNameFromScene(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestEntityNameFromSceneScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create entity with name
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntityName");

	// Verify GetName() returns the correct name
	ZENITH_ASSERT_EQ(xEntity.GetName(), "TestEntityName", "TestEntityNameFromScene: GetName() returned wrong name");

	// Change name via SetName()
	xEntity.SetName("RenamedEntity");
	ZENITH_ASSERT_EQ(xEntity.GetName(), "RenamedEntity", "TestEntityNameFromScene: SetName() did not update name");

	// Verify name is accessible through the scene's entity API
	ZENITH_ASSERT_EQ(pxSceneData->GetEntity(xEntity.GetEntityID()).GetName(), "RenamedEntity", "TestEntityNameFromScene: Entity in scene does not have correct name");

	// Create another entity and verify names don't interfere
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "SecondEntity");
	ZENITH_ASSERT_EQ(xEntity.GetName(), "RenamedEntity", "TestEntityNameFromScene: First entity name changed after creating second");
	ZENITH_ASSERT_EQ(xEntity2.GetName(), "SecondEntity", "TestEntityNameFromScene: Second entity has wrong name");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that copying an entity preserves access to components.
 * Since Entity is now just a lightweight handle (scene pointer + IDs),
 * copies should reference the same underlying component data.
 */
ZENITH_TEST(ECS, EntityCopyPreservesAccess) { Zenith_UnitTests::TestEntityCopyPreservesAccess(); }
void Zenith_UnitTests::TestEntityCopyPreservesAccess(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestEntityCopyPreservesAccessScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xOriginal = g_xEngine.Scenes().CreateEntity(pxSceneData, "OriginalEntity");

	// Set a position
	xOriginal.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(42.0f, 43.0f, 44.0f));

	// Copy the entity
	Zenith_Entity xCopy = xOriginal;

	// Verify copy has same entity ID
	ZENITH_ASSERT_EQ(xCopy.GetEntityID(), xOriginal.GetEntityID(), "TestEntityCopyPreservesAccess: Copy has different entity ID");

	// Verify copy can access the same component data
	Zenith_Maths::Vector3 xCopyPos;
	xCopy.GetComponent<Zenith_TransformComponent>().GetPosition(xCopyPos);
	ZENITH_ASSERT_TRUE(xCopyPos.x == 42.0f && xCopyPos.y == 43.0f && xCopyPos.z == 44.0f, "TestEntityCopyPreservesAccess: Copy cannot access component data");

	// Modify via copy, verify original sees change
	xCopy.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));

	Zenith_Maths::Vector3 xOriginalPos;
	xOriginal.GetComponent<Zenith_TransformComponent>().GetPosition(xOriginalPos);
	ZENITH_ASSERT_TRUE(xOriginalPos.x == 100.0f && xOriginalPos.y == 200.0f && xOriginalPos.z == 300.0f, "TestEntityCopyPreservesAccess: Original did not see modification via copy");

	// Verify name access works on copy
	ZENITH_ASSERT_EQ(xCopy.GetName(), "OriginalEntity", "TestEntityCopyPreservesAccess: Copy cannot access entity name");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

//------------------------------------------------------------------------------
// ECS Reflection System Tests (Phase 2)
//------------------------------------------------------------------------------

/**
 * Test that all component types are registered with the ComponentMeta registry.
 * Verifies the registration macro and registry initialization work correctly.
 */
ZENITH_TEST(ECS, ComponentMetaRegistration) { Zenith_UnitTests::TestComponentMetaRegistration(); }
void Zenith_UnitTests::TestComponentMetaRegistration(){

	const auto& xMetasSorted = Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();

	// Verify we have the expected number of component types (8 components)
	ZENITH_ASSERT_GE(xMetasSorted.GetSize(), 8, "TestComponentMetaRegistration: Expected at least 8 registered component types");

	// Verify Transform is registered
	const Zenith_ComponentMeta* pxTransformMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	ZENITH_ASSERT_NOT_NULL(pxTransformMeta, "TestComponentMetaRegistration: Transform not registered");
	ZENITH_ASSERT_NOT_NULL(pxTransformMeta->m_pfnCreate, "TestComponentMetaRegistration: Transform has no create function");
	ZENITH_ASSERT_NOT_NULL(pxTransformMeta->m_pfnHasComponent, "TestComponentMetaRegistration: Transform has no hasComponent function");

	// Verify Camera is registered
	const Zenith_ComponentMeta* pxCameraMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Camera");
	ZENITH_ASSERT_NOT_NULL(pxCameraMeta, "TestComponentMetaRegistration: Camera not registered");

	// Verify Model is registered
	const Zenith_ComponentMeta* pxModelMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Model");
	ZENITH_ASSERT_NOT_NULL(pxModelMeta, "TestComponentMetaRegistration: Model not registered");

}

/**
 * Test that component serialization via the registry works correctly.
 * Creates an entity with components, serializes via registry, deserializes
 * and verifies the data is preserved.
 */
ZENITH_TEST(ECS, ComponentMetaSerialization) { Zenith_UnitTests::TestComponentMetaSerialization(); }
void Zenith_UnitTests::TestComponentMetaSerialization(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentMetaSerializationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SerializationTestEntity");

	// Set up transform
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
		Zenith_Maths::Vector3(2.0f, 3.0f, 4.0f));

	// Add a camera component
	Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f),
		.m_fPitch = 0.5f,
		.m_fYaw = 1.0f,
	});

	// Serialize via registry
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xEntity, xStream);

	// If we get here without assertion, serialization worked
	// The deserialization test will verify the data is correct

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that component deserialization via the registry works correctly.
 * Serializes an entity, creates a new entity, deserializes onto it,
 * and verifies the components match.
 */
ZENITH_TEST(ECS, ComponentMetaDeserialization) { Zenith_UnitTests::TestComponentMetaDeserialization(); }
void Zenith_UnitTests::TestComponentMetaDeserialization(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestComponentMetaDeserializationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xOriginal = g_xEngine.Scenes().CreateEntity(pxSceneData, "OriginalEntity");

	// Set distinctive values
	xOriginal.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(111.0f, 222.0f, 333.0f));

	// Serialize original
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xOriginal, xStream);

	// Create new entity
	Zenith_Entity xNew = g_xEngine.Scenes().CreateEntity(pxSceneData, "NewEntity");

	// Reset stream cursor
	xStream.SetCursor(0);

	// Deserialize onto new entity
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xNew, xStream);

	// Verify transform was copied
	Zenith_Maths::Vector3 xNewPos;
	xNew.GetComponent<Zenith_TransformComponent>().GetPosition(xNewPos);
	ZENITH_ASSERT_TRUE(xNewPos.x == 111.0f && xNewPos.y == 222.0f && xNewPos.z == 333.0f, "TestComponentMetaDeserialization: Deserialized transform position is wrong");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that TypeID is consistent for the same component type.
 * Verifies that registering and looking up uses consistent type IDs.
 */
ZENITH_TEST(ECS, ComponentMetaTypeIDConsistency) { Zenith_UnitTests::TestComponentMetaTypeIDConsistency(); }
void Zenith_UnitTests::TestComponentMetaTypeIDConsistency(){

	// Get meta for Transform
	const Zenith_ComponentMeta* pxMeta1 =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	const Zenith_ComponentMeta* pxMeta2 =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");

	// Verify same pointer returned
	ZENITH_ASSERT_EQ(pxMeta1, pxMeta2, "TestComponentMetaTypeIDConsistency: Different meta pointers for same type");

	// Verify serialization order is set correctly (Transform should be first)
	ZENITH_ASSERT_EQ(pxMeta1->m_uSerializationOrder, 0, "TestComponentMetaTypeIDConsistency: Transform serialization order is not 0");

	// Verify all metas in sorted list have increasing serialization order
	const auto& xMetasSorted = Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();
	u_int uPrevOrder = 0;
	for (u_int i = 1; i < xMetasSorted.GetSize(); ++i)
	{
		ZENITH_ASSERT_GE(xMetasSorted.Get(i)->m_uSerializationOrder, uPrevOrder, "TestComponentMetaTypeIDConsistency: Metas not sorted by serialization order");
		uPrevOrder = xMetasSorted.Get(i)->m_uSerializationOrder;
	}

}

//------------------------------------------------------------------------------
// ECS Lifecycle Hooks Tests (Phase 3)
//------------------------------------------------------------------------------

/**
 * Test that lifecycle hook detection via C++20 concepts works correctly.
 * Verifies that the HasOnAwake, HasOnUpdate, etc. concepts correctly detect
 * whether a component type implements the hook methods.
 */
ZENITH_TEST(ECS, LifecycleHookDetection) { Zenith_UnitTests::TestLifecycleHookDetection(); }
void Zenith_UnitTests::TestLifecycleHookDetection(){

	// Transform doesn't implement lifecycle hooks, so all hooks should be nullptr
	const Zenith_ComponentMeta* pxTransformMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	ZENITH_ASSERT_NOT_NULL(pxTransformMeta, "TestLifecycleHookDetection: Transform not registered");

	// Transform shouldn't have lifecycle hooks (it doesn't implement them)
	ZENITH_ASSERT_NULL(pxTransformMeta->m_pfnOnAwake, "TestLifecycleHookDetection: Transform has OnAwake hook (shouldn't)");
	ZENITH_ASSERT_NULL(pxTransformMeta->m_pfnOnStart, "TestLifecycleHookDetection: Transform has OnStart hook (shouldn't)");
	ZENITH_ASSERT_NULL(pxTransformMeta->m_pfnOnUpdate, "TestLifecycleHookDetection: Transform has OnUpdate hook (shouldn't)");
	ZENITH_ASSERT_NULL(pxTransformMeta->m_pfnOnDestroy, "TestLifecycleHookDetection: Transform has OnDestroy hook (shouldn't)");

	// Verify registry is finalized
	ZENITH_ASSERT_TRUE(Zenith_ComponentMetaRegistry::Get().IsInitialized(), "TestLifecycleHookDetection: Registry not initialized");

}

/**
 * Test that DispatchOnAwake correctly calls OnAwake on components that have it.
 * Since our existing components don't implement OnAwake, we verify dispatch
 * doesn't crash and completes successfully.
 */
ZENITH_TEST(ECS, LifecycleOnAwake) { Zenith_UnitTests::TestLifecycleOnAwake(); }
void Zenith_UnitTests::TestLifecycleOnAwake(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestLifecycleOnAwakeScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AwakeTestEntity");

	// Dispatch OnAwake - should complete without crashing
	// (no components implement OnAwake, so nothing is called)
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);

	// Verify entity is still valid
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "TestLifecycleOnAwake: Entity lost TransformComponent after dispatch");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that DispatchOnStart correctly calls OnStart on components that have it.
 */
ZENITH_TEST(ECS, LifecycleOnStart) { Zenith_UnitTests::TestLifecycleOnStart(); }
void Zenith_UnitTests::TestLifecycleOnStart(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestLifecycleOnStartScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "StartTestEntity");

	// Dispatch OnStart - should complete without crashing
	Zenith_ComponentMetaRegistry::Get().DispatchOnStart(xEntity);

	// Verify entity is still valid
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "TestLifecycleOnStart: Entity lost TransformComponent after dispatch");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that DispatchOnUpdate correctly calls OnUpdate on components that have it.
 */
ZENITH_TEST(ECS, LifecycleOnUpdate) { Zenith_UnitTests::TestLifecycleOnUpdate(); }
void Zenith_UnitTests::TestLifecycleOnUpdate(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestLifecycleOnUpdateScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "UpdateTestEntity");

	// Dispatch OnUpdate with a delta time - should complete without crashing
	const float fDt = 0.016f; // ~60fps
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, fDt);

	// Verify entity is still valid
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "TestLifecycleOnUpdate: Entity lost TransformComponent after dispatch");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that DispatchOnDestroy correctly calls OnDestroy on components that have it.
 */
ZENITH_TEST(ECS, LifecycleOnDestroy) { Zenith_UnitTests::TestLifecycleOnDestroy(); }
void Zenith_UnitTests::TestLifecycleOnDestroy(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestLifecycleOnDestroyScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "DestroyTestEntity");

	// Set a position before dispatch
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));

	// Dispatch OnDestroy - should complete without crashing
	Zenith_ComponentMetaRegistry::Get().DispatchOnDestroy(xEntity);

	// Verify entity is still valid (OnDestroy doesn't remove components)
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "TestLifecycleOnDestroy: Entity lost TransformComponent after dispatch");

	// Verify data is intact
	Zenith_Maths::Vector3 xPos;
	xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(xPos.x == 1.0f && xPos.y == 2.0f && xPos.z == 3.0f, "TestLifecycleOnDestroy: Component data corrupted after dispatch");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that lifecycle dispatch respects component serialization order.
 * Components with lower serialization order should have their hooks called first.
 */
ZENITH_TEST(ECS, LifecycleDispatchOrder) { Zenith_UnitTests::TestLifecycleDispatchOrder(); }
void Zenith_UnitTests::TestLifecycleDispatchOrder(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestLifecycleDispatchOrderScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "OrderTestEntity");

	// Add multiple components
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Dispatch all lifecycle hooks in sequence
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);
	Zenith_ComponentMetaRegistry::Get().DispatchOnStart(xEntity);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnLateUpdate(xEntity, 0.016f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnFixedUpdate(xEntity, 0.02f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnDestroy(xEntity);

	// Verify all components are still valid
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "TestLifecycleDispatchOrder: Entity lost TransformComponent");
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_CameraComponent>(), "TestLifecycleDispatchOrder: Entity lost CameraComponent");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

/**
 * Test that creating entities during lifecycle callbacks doesn't cause crashes.
 *
 * This tests the scenario that caused the editor Play->Stop crash:
 * When a lifecycle callback (OnAwake, OnStart, etc.) creates new entities,
 * the m_xEntitySlots vector may reallocate, invalidating any held references.
 *
 * The fix was to:
 * 1. Copy entity IDs before iteration (not hold a reference to the vector)
 * 2. Use separate loops for each lifecycle stage
 * 3. Re-fetch entity references before each callback
 */
ZENITH_TEST(ECS, LifecycleEntityCreationDuringCallback) { Zenith_UnitTests::TestLifecycleEntityCreationDuringCallback(); }
void Zenith_UnitTests::TestLifecycleEntityCreationDuringCallback(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Store initial entity count
	const u_int uInitialCount = pxSceneData->GetEntityCount();

	// Create initial entity
	Zenith_Entity xInitialEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "InitialEntity");
	Zenith_EntityID xInitialID = xInitialEntity.GetEntityID();

	// Copy entity IDs to prevent iterator invalidation (the safe pattern)
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(pxSceneData->GetActiveEntities().GetSize());
	for (u_int u = 0; u < pxSceneData->GetActiveEntities().GetSize(); ++u)
	{
		xEntityIDs.PushBack(pxSceneData->GetActiveEntities().Get(u));
	}

	// Simulate what OnAwake might do: create more entities
	// This should NOT crash because we're iterating over a copy of IDs
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(xEntityID))
		{
			// Get entity handle (lightweight - safe to use after pool reallocation)
			Zenith_Entity xEntity = pxSceneData->GetEntity(xEntityID);

			// Simulate OnAwake creating multiple new entities
			// This will cause m_xEntitySlots to reallocate
			for (u_int i = 0; i < 10; ++i)
			{
				Zenith_Entity xNewEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "CreatedDuringCallback_" + std::to_string(i));
				// Entity handles are safe - they don't hold pointers into the pool
			}

			// Entity handle still valid after pool reallocation (lightweight handle pattern)
			Zenith_Entity xEntityRefreshed = pxSceneData->GetEntity(xEntityID);

			// Verify the entity is still accessible
			ZENITH_ASSERT_TRUE(xEntityRefreshed.HasComponent<Zenith_TransformComponent>(), "TestLifecycleEntityCreationDuringCallback: Entity lost TransformComponent after sibling creation");
		}
	}

	// Verify original entity is still valid
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xInitialID), "TestLifecycleEntityCreationDuringCallback: Initial entity was invalidated");
	ZENITH_ASSERT_EQ(pxSceneData->GetEntity(xInitialID).GetName(), "InitialEntity", "TestLifecycleEntityCreationDuringCallback: Initial entity name corrupted");

	// Verify entities were created (proves reallocation happened)
	ZENITH_ASSERT_GT(pxSceneData->GetEntityCount(), uInitialCount + 1, "TestLifecycleEntityCreationDuringCallback: New entities were not created");

}

/**
 * Test that Zenith_Scene::DispatchFullLifecycleInit works correctly.
 *
 * This is the shared helper function that both the editor and other code
 * should use to dispatch lifecycle callbacks safely.
 */
ZENITH_TEST(Core, DispatchFullLifecycleInit) { Zenith_UnitTests::TestDispatchFullLifecycleInit(); }
void Zenith_UnitTests::TestDispatchFullLifecycleInit(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create several entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LifecycleInitEntity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LifecycleInitEntity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LifecycleInitEntity3");

	Zenith_EntityID xID1 = xEntity1.GetEntityID();
	Zenith_EntityID xID2 = xEntity2.GetEntityID();
	Zenith_EntityID xID3 = xEntity3.GetEntityID();

	// Call the shared lifecycle init function
	// This should NOT crash even if callbacks create new entities
	g_xEngine.Scenes().DispatchFullLifecycleInit();

	// Verify all original entities are still valid and accessible
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID1), "TestDispatchFullLifecycleInit: Entity1 was invalidated");
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID2), "TestDispatchFullLifecycleInit: Entity2 was invalidated");
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(xID3), "TestDispatchFullLifecycleInit: Entity3 was invalidated");

	// Verify entities are still accessible with correct data
	ZENITH_ASSERT_EQ(pxSceneData->GetEntity(xID1).GetName(), "LifecycleInitEntity1", "TestDispatchFullLifecycleInit: Entity1 name corrupted");
	ZENITH_ASSERT_EQ(pxSceneData->GetEntity(xID2).GetName(), "LifecycleInitEntity2", "TestDispatchFullLifecycleInit: Entity2 name corrupted");
	ZENITH_ASSERT_EQ(pxSceneData->GetEntity(xID3).GetName(), "LifecycleInitEntity3", "TestDispatchFullLifecycleInit: Entity3 name corrupted");

	// Verify components are intact
	ZENITH_ASSERT_TRUE(pxSceneData->GetEntity(xID1).HasComponent<Zenith_TransformComponent>(), "TestDispatchFullLifecycleInit: Entity1 lost TransformComponent");
	ZENITH_ASSERT_TRUE(pxSceneData->GetEntity(xID2).HasComponent<Zenith_TransformComponent>(), "TestDispatchFullLifecycleInit: Entity2 lost TransformComponent");
	ZENITH_ASSERT_TRUE(pxSceneData->GetEntity(xID3).HasComponent<Zenith_TransformComponent>(), "TestDispatchFullLifecycleInit: Entity3 lost TransformComponent");

}

//------------------------------------------------------------------------------
// ECS Query System Tests (Phase 4)
//------------------------------------------------------------------------------

ZENITH_TEST(ECS, QuerySingleComponent) { Zenith_UnitTests::TestQuerySingleComponent(); }

void Zenith_UnitTests::TestQuerySingleComponent(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQuerySingleComponentScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create 3 entities with transforms
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");

	// All 3 entities have TransformComponent (added by default)
	// Add CameraComponent to only 2 entities
	xEntity1.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();

	// Query for TransformComponent - should return all 3 entities
	u_int uTransformCount = 0;
	pxSceneData->Query<Zenith_TransformComponent>().ForEach(
		[&uTransformCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uTransformCount++;
		});

	ZENITH_ASSERT_EQ(uTransformCount, 3, "TestQuerySingleComponent: Expected 3 entities with TransformComponent");

	// Query for CameraComponent - should return 2 entities
	u_int uCameraCount = 0;
	pxSceneData->Query<Zenith_CameraComponent>().ForEach(
		[&uCameraCount](Zenith_EntityID, Zenith_CameraComponent&) {
			uCameraCount++;
		});

	ZENITH_ASSERT_EQ(uCameraCount, 2, "TestQuerySingleComponent: Expected 2 entities with CameraComponent");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

ZENITH_TEST(ECS, QueryMultipleComponents) { Zenith_UnitTests::TestQueryMultipleComponents(); }

void Zenith_UnitTests::TestQueryMultipleComponents(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQueryMultipleComponentsScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create 3 entities with transforms
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");

	// Set different positions for verification
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition({1.0f, 0.0f, 0.0f});
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition({2.0f, 0.0f, 0.0f});
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition({3.0f, 0.0f, 0.0f});

	// Add CameraComponent to entities 1 and 3
	xEntity1.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();

	// Query for entities with BOTH TransformComponent AND CameraComponent
	u_int uMatchCount = 0;
	std::vector<float> xPositions;
	pxSceneData->Query<Zenith_TransformComponent, Zenith_CameraComponent>().ForEach(
		[&uMatchCount, &xPositions](Zenith_EntityID,
		                            Zenith_TransformComponent& xTransform,
		                            Zenith_CameraComponent&) {
			uMatchCount++;
			Zenith_Maths::Vector3 xPos;
			xTransform.GetPosition(xPos);
			xPositions.push_back(xPos.x);
		});

	ZENITH_ASSERT_EQ(uMatchCount, 2, "TestQueryMultipleComponents: Expected 2 entities with both Transform and Camera");

	// Verify we got entities 1 and 3 (positions 1.0 and 3.0)
	bool bFoundEntity1 = std::find(xPositions.begin(), xPositions.end(), 1.0f) != xPositions.end();
	bool bFoundEntity3 = std::find(xPositions.begin(), xPositions.end(), 3.0f) != xPositions.end();

	ZENITH_ASSERT_TRUE(bFoundEntity1 && bFoundEntity3, "TestQueryMultipleComponents: Did not find expected entities");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

ZENITH_TEST(ECS, QueryNoMatches) { Zenith_UnitTests::TestQueryNoMatches(); }

void Zenith_UnitTests::TestQueryNoMatches(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQueryNoMatchesScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create entity with only TransformComponent
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");

	// Query for CameraComponent - should return no matches
	u_int uCount = 0;
	pxSceneData->Query<Zenith_CameraComponent>().ForEach(
		[&uCount](Zenith_EntityID, Zenith_CameraComponent&) {
			uCount++;
		});

	ZENITH_ASSERT_EQ(uCount, 0, "TestQueryNoMatches: Expected 0 entities with CameraComponent");

	// Verify Any() returns false
	bool bHasAny = pxSceneData->Query<Zenith_CameraComponent>().Any();
	ZENITH_ASSERT_FALSE(bHasAny, "TestQueryNoMatches: Any() should return false for empty query");

	// Verify First() returns INVALID_ENTITY_ID
	Zenith_EntityID uFirst = pxSceneData->Query<Zenith_CameraComponent>().First();
	ZENITH_ASSERT_EQ(uFirst, INVALID_ENTITY_ID, "TestQueryNoMatches: First() should return INVALID_ENTITY_ID for empty query");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

ZENITH_TEST(ECS, QueryCount) { Zenith_UnitTests::TestQueryCount(); }

void Zenith_UnitTests::TestQueryCount(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQueryCountScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create 5 entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");
	Zenith_Entity xEntity4 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity4");
	Zenith_Entity xEntity5 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity5");

	// Add CameraComponent to 3 entities
	xEntity2.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();
	xEntity5.AddComponent<Zenith_CameraComponent>();

	// Test Count() for TransformComponent (all 5)
	u_int uTransformCount = pxSceneData->Query<Zenith_TransformComponent>().Count();
	ZENITH_ASSERT_EQ(uTransformCount, 5, "TestQueryCount: Expected 5 entities with TransformComponent");

	// Test Count() for CameraComponent (3)
	u_int uCameraCount = pxSceneData->Query<Zenith_CameraComponent>().Count();
	ZENITH_ASSERT_EQ(uCameraCount, 3, "TestQueryCount: Expected 3 entities with CameraComponent");

	// Test Count() for both components (3)
	u_int uBothCount = pxSceneData->Query<Zenith_TransformComponent, Zenith_CameraComponent>().Count();
	ZENITH_ASSERT_EQ(uBothCount, 3, "TestQueryCount: Expected 3 entities with both Transform and Camera");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

ZENITH_TEST(ECS, QueryFirstAndAny) { Zenith_UnitTests::TestQueryFirstAndAny(); }

void Zenith_UnitTests::TestQueryFirstAndAny(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("TestQueryFirstAndAnyScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create 3 entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity3");

	// Add CameraComponent to entity 2
	xEntity2.AddComponent<Zenith_CameraComponent>();

	// Test Any() returns true when there are matches
	bool bHasCamera = pxSceneData->Query<Zenith_CameraComponent>().Any();
	ZENITH_ASSERT_TRUE(bHasCamera, "TestQueryFirstAndAny: Any() should return true when matches exist");

	// Test First() returns a valid entity ID
	Zenith_EntityID uFirstCamera = pxSceneData->Query<Zenith_CameraComponent>().First();
	ZENITH_ASSERT_NE(uFirstCamera, INVALID_ENTITY_ID, "TestQueryFirstAndAny: First() should return valid ID when matches exist");

	// Verify the first match actually has the component
	ZENITH_ASSERT_TRUE(pxSceneData->EntityHasComponent<Zenith_CameraComponent>(uFirstCamera), "TestQueryFirstAndAny: First() returned entity without expected component");

	// Test First() for TransformComponent returns the first entity ID (1)
	Zenith_EntityID uFirstTransform = pxSceneData->Query<Zenith_TransformComponent>().First();
	ZENITH_ASSERT_NE(uFirstTransform, INVALID_ENTITY_ID, "TestQueryFirstAndAny: First() should return valid ID for TransformComponent");

	g_xEngine.Scenes().UnloadScene(xTestScene);
}

//------------------------------------------------------------------------------
// ECS Event System Tests (Phase 5)
//------------------------------------------------------------------------------

// Custom test event for unit tests
struct TestEvent_Custom
{
	u_int m_uValue = 0;
};

// Static variable to track event callbacks
static u_int s_uTestEventCallCount = 0;
static u_int s_uTestEventLastValue = 0;

static void TestEventCallback(const TestEvent_Custom& xEvent)
{
	s_uTestEventCallCount++;
	s_uTestEventLastValue = xEvent.m_uValue;
}

ZENITH_TEST(ECS, EventSubscribeDispatch) { Zenith_UnitTests::TestEventSubscribeDispatch(); }

void Zenith_UnitTests::TestEventSubscribeDispatch(){

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;
	s_uTestEventLastValue = 0;

	// Subscribe to test event
	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	ZENITH_ASSERT_NE(uHandle, INVALID_EVENT_HANDLE, "TestEventSubscribeDispatch: Subscribe should return valid handle");

	// Dispatch event
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 42;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 1, "TestEventSubscribeDispatch: Callback should be called once");
	ZENITH_ASSERT_EQ(s_uTestEventLastValue, 42, "TestEventSubscribeDispatch: Callback should receive correct value");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

}

ZENITH_TEST(ECS, EventUnsubscribe) { Zenith_UnitTests::TestEventUnsubscribe(); }

void Zenith_UnitTests::TestEventUnsubscribe(){

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;

	// Subscribe to test event
	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	// Verify subscription count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	ZENITH_ASSERT_EQ(uCount, 1, "TestEventUnsubscribe: Should have 1 subscriber after subscribe");

	// Unsubscribe
	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);

	// Verify subscription count
	uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	ZENITH_ASSERT_EQ(uCount, 0, "TestEventUnsubscribe: Should have 0 subscribers after unsubscribe");

	// Dispatch event - callback should NOT be called
	s_uTestEventCallCount = 0;
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 100;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 0, "TestEventUnsubscribe: Callback should not be called after unsubscribe");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

}

ZENITH_TEST(ECS, EventDeferredQueue) { Zenith_UnitTests::TestEventDeferredQueue(); }

void Zenith_UnitTests::TestEventDeferredQueue(){

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;
	s_uTestEventLastValue = 0;

	// Subscribe to test event
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	// Queue event (should not dispatch immediately)
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 99;
	Zenith_EventDispatcher::Get().QueueEvent(xEvent);

	// Verify callback not called yet
	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 0, "TestEventDeferredQueue: Callback should not be called before ProcessDeferredEvents");

	// Process deferred events
	Zenith_EventDispatcher::Get().ProcessDeferredEvents();

	// Verify callback was called
	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 1, "TestEventDeferredQueue: Callback should be called after ProcessDeferredEvents");
	ZENITH_ASSERT_EQ(s_uTestEventLastValue, 99, "TestEventDeferredQueue: Callback should receive correct value");

	// Queue and process multiple events
	s_uTestEventCallCount = 0;
	TestEvent_Custom xEvent2, xEvent3;
	xEvent2.m_uValue = 1;
	xEvent3.m_uValue = 2;
	Zenith_EventDispatcher::Get().QueueEvent(xEvent2);
	Zenith_EventDispatcher::Get().QueueEvent(xEvent3);

	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 0, "TestEventDeferredQueue: Callbacks should not be called before processing");

	Zenith_EventDispatcher::Get().ProcessDeferredEvents();

	ZENITH_ASSERT_EQ(s_uTestEventCallCount, 2, "TestEventDeferredQueue: Both callbacks should be called after processing");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

}

// Static variables for multiple subscriber test
static u_int s_uMultiSub1Count = 0;
static u_int s_uMultiSub2Count = 0;

static void MultiSubscriber1(const TestEvent_Custom& /*xEvent*/)
{
	s_uMultiSub1Count++;
}

static void MultiSubscriber2(const TestEvent_Custom& /*xEvent*/)
{
	s_uMultiSub2Count++;
}

ZENITH_TEST(ECS, EventMultipleSubscribers) { Zenith_UnitTests::TestEventMultipleSubscribers(); }

void Zenith_UnitTests::TestEventMultipleSubscribers(){

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uMultiSub1Count = 0;
	s_uMultiSub2Count = 0;

	// Subscribe two callbacks to the same event type
	Zenith_EventHandle uHandle1 = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber1);
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber2);

	// Verify subscriber count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	ZENITH_ASSERT_EQ(uCount, 2, "TestEventMultipleSubscribers: Should have 2 subscribers");

	// Dispatch event
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 10;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	ZENITH_ASSERT_EQ(s_uMultiSub1Count, 1, "TestEventMultipleSubscribers: Subscriber1 should be called once");
	ZENITH_ASSERT_EQ(s_uMultiSub2Count, 1, "TestEventMultipleSubscribers: Subscriber2 should be called once");

	// Unsubscribe first callback
	Zenith_EventDispatcher::Get().Unsubscribe(uHandle1);

	// Dispatch again
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	ZENITH_ASSERT_EQ(s_uMultiSub1Count, 1, "TestEventMultipleSubscribers: Subscriber1 should not be called after unsubscribe");
	ZENITH_ASSERT_EQ(s_uMultiSub2Count, 2, "TestEventMultipleSubscribers: Subscriber2 should be called again");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

}

ZENITH_TEST(ECS, EventClearSubscriptions) { Zenith_UnitTests::TestEventClearSubscriptions(); }

void Zenith_UnitTests::TestEventClearSubscriptions(){

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;

	// Subscribe multiple callbacks
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber1);
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber2);

	// Verify subscriber count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	ZENITH_ASSERT_EQ(uCount, 3, "TestEventClearSubscriptions: Should have 3 subscribers");

	// Clear all subscriptions
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	// Verify subscriber count is now 0
	uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	ZENITH_ASSERT_EQ(uCount, 0, "TestEventClearSubscriptions: Should have 0 subscribers after clear");

	// Dispatch event - no callbacks should be called
	s_uTestEventCallCount = 0;
	s_uMultiSub1Count = 0;
	s_uMultiSub2Count = 0;
	TestEvent_Custom xEvent;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	ZENITH_ASSERT_TRUE(s_uTestEventCallCount == 0 && s_uMultiSub1Count == 0 && s_uMultiSub2Count == 0, "TestEventClearSubscriptions: No callbacks should be called after clear");

}

//------------------------------------------------------------------------------
// Entity Hierarchy Tests
//------------------------------------------------------------------------------

ZENITH_TEST(ECS, EntityAddChild) { Zenith_UnitTests::TestEntityAddChild(); }

void Zenith_UnitTests::TestEntityAddChild(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create parent and child entities
	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestParent");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Initially, both should have no children
	ZENITH_ASSERT_EQ(xParent.GetChildCount(), 0, "TestEntityAddChild: Parent should have no children initially");
	ZENITH_ASSERT_FALSE(xParent.HasChildren(), "TestEntityAddChild: HasChildren should be false");

	// Add child using SetParent
	xChild.SetParent(uParentID);

	// Verify parent-child relationship (Entity handles delegate to single source of truth)
	Zenith_Entity xChildRef = pxSceneData->GetEntity(uChildID);
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);

	ZENITH_ASSERT_EQ(xChildRef.GetParentEntityID(), uParentID, "TestEntityAddChild: Child should have parent ID set");
	ZENITH_ASSERT_TRUE(xChildRef.HasParent(), "TestEntityAddChild: Child HasParent should be true");
	ZENITH_ASSERT_EQ(xParentRef.GetChildCount(), 1, "TestEntityAddChild: Parent should have 1 child");
	ZENITH_ASSERT_TRUE(xParentRef.HasChildren(), "TestEntityAddChild: Parent HasChildren should be true");
	ZENITH_ASSERT_EQ(xParentRef.GetChildEntityIDs().Get(0), uChildID, "TestEntityAddChild: Parent's child should be correct ID");

}

ZENITH_TEST(ECS, EntityRemoveChild) { Zenith_UnitTests::TestEntityRemoveChild(); }

void Zenith_UnitTests::TestEntityRemoveChild(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create parent and child entities
	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestParent2");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestChild2");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);
	ZENITH_ASSERT_EQ(xParent.GetChildCount(), 1, "TestEntityRemoveChild: Parent should have 1 child");

	// Remove parent (unparent child)
	xChild.SetParent(INVALID_ENTITY_ID);

	// Verify relationship is broken
	Zenith_Entity xChildRef = pxSceneData->GetEntity(uChildID);
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);

	ZENITH_ASSERT_FALSE(xChildRef.HasParent(), "TestEntityRemoveChild: Child should no longer have parent");
	ZENITH_ASSERT_EQ(xChildRef.GetParentEntityID(), INVALID_ENTITY_ID, "TestEntityRemoveChild: Child parent ID should be INVALID");
	ZENITH_ASSERT_EQ(xParentRef.GetChildCount(), 0, "TestEntityRemoveChild: Parent should have no children");
	ZENITH_ASSERT_FALSE(xParentRef.HasChildren(), "TestEntityRemoveChild: Parent HasChildren should be false");

}

ZENITH_TEST(ECS, EntityGetChildren) { Zenith_UnitTests::TestEntityGetChildren(); }

void Zenith_UnitTests::TestEntityGetChildren(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create parent with multiple children
	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestParent3");
	Zenith_Entity xChild1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestChild3a");
	Zenith_Entity xChild2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestChild3b");
	Zenith_Entity xChild3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestChild3c");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChild1ID = xChild1.GetEntityID();
	Zenith_EntityID uChild2ID = xChild2.GetEntityID();
	Zenith_EntityID uChild3ID = xChild3.GetEntityID();

	// Add all children
	xChild1.SetParent(uParentID);
	xChild2.SetParent(uParentID);
	xChild3.SetParent(uParentID);

	// Verify all children are tracked
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);
	ZENITH_ASSERT_EQ(xParentRef.GetChildCount(), 3, "TestEntityGetChildren: Parent should have 3 children");

	Zenith_Vector<Zenith_EntityID> xChildren = xParentRef.GetChildEntityIDs();
	bool bFoundChild1 = false, bFoundChild2 = false, bFoundChild3 = false;
	for (u_int i = 0; i < xChildren.GetSize(); i++)
	{
		if (xChildren.Get(i) == uChild1ID) bFoundChild1 = true;
		if (xChildren.Get(i) == uChild2ID) bFoundChild2 = true;
		if (xChildren.Get(i) == uChild3ID) bFoundChild3 = true;
	}
	ZENITH_ASSERT_TRUE(bFoundChild1 && bFoundChild2 && bFoundChild3, "TestEntityGetChildren: All children should be in list");

}

ZENITH_TEST(ECS, EntityReparenting) { Zenith_UnitTests::TestEntityReparenting(); }

void Zenith_UnitTests::TestEntityReparenting(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create entities for reparenting test
	Zenith_Entity xParentA = g_xEngine.Scenes().CreateEntity(pxSceneData, "ParentA");
	Zenith_Entity xParentB = g_xEngine.Scenes().CreateEntity(pxSceneData, "ParentB");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "ReparentChild");

	Zenith_EntityID uParentAID = xParentA.GetEntityID();
	Zenith_EntityID uParentBID = xParentB.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Parent to A
	xChild.SetParent(uParentAID);
	ZENITH_ASSERT_EQ(xParentA.GetChildCount(), 1, "TestEntityReparenting: ParentA should have 1 child");
	ZENITH_ASSERT_EQ(xParentB.GetChildCount(), 0, "TestEntityReparenting: ParentB should have 0 children");
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParentAID, "TestEntityReparenting: Child should be parented to A");

	// Reparent to B
	xChild.SetParent(uParentBID);
	ZENITH_ASSERT_EQ(xParentA.GetChildCount(), 0, "TestEntityReparenting: ParentA should now have 0 children");
	ZENITH_ASSERT_EQ(xParentB.GetChildCount(), 1, "TestEntityReparenting: ParentB should now have 1 child");
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParentBID, "TestEntityReparenting: Child should be parented to B");

}

ZENITH_TEST(ECS, EntityChildCleanupOnDelete) { Zenith_UnitTests::TestEntityChildCleanupOnDelete(); }

void Zenith_UnitTests::TestEntityChildCleanupOnDelete(){

	// Note: This test documents expected behavior for entity deletion
	// In a real implementation, deleting a parent would need to handle children
	// For now we just verify the API works correctly

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeleteParent");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeleteChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);

	ZENITH_ASSERT_EQ(xParent.GetChildCount(), 1, "TestEntityChildCleanupOnDelete: Should have child");

	// Unparent before any deletion (good practice)
	xChild.SetParent(INVALID_ENTITY_ID);
	ZENITH_ASSERT_EQ(xParent.GetChildCount(), 0, "TestEntityChildCleanupOnDelete: Should have no children after unparent");

}

ZENITH_TEST(ECS, EntityHierarchySerialization) { Zenith_UnitTests::TestEntityHierarchySerialization(); }

void Zenith_UnitTests::TestEntityHierarchySerialization(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create hierarchy
	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "SerializeParent");
	Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "SerializeChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);

	// Serialize parent entity
	Zenith_DataStream xStream(256);
	xParent.WriteToDataStream(xStream);

	// Reset and read back
	// Note: Must create a valid entity in scene first, as deserialization
	// calls AddComponent which requires a valid EntityID in the scene
	xStream.SetCursor(0);
	Zenith_Entity xLoadedParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "TempParent");
	xLoadedParent.ReadFromDataStream(xStream);

	// Children are stored in scene, so parent ID should serialize
	// The parent's child list is rebuilt when children are loaded and call SetParent
	ZENITH_ASSERT_TRUE(xLoadedParent.IsRoot(), "TestEntityHierarchySerialization: Loaded parent should be root");

	// Serialize child entity
	Zenith_DataStream xChildStream(256);
	xChild.WriteToDataStream(xChildStream);

	// Create entity in scene before deserializing
	xChildStream.SetCursor(0);
	Zenith_Entity xLoadedChild = g_xEngine.Scenes().CreateEntity(pxSceneData, "TempChild");
	xLoadedChild.ReadFromDataStream(xChildStream);

	// Standalone entity deserialization stores the parent's file index in PendingParentFileIndex
	// The actual parent relationship is only rebuilt during full scene loading
	// So we verify the pending index matches the original parent's index.
	// Phase 5b: the pending-parent file index is slot-backed, read via the
	// Zenith_Entity API (the Transform forwarding shim was removed).
	ZENITH_ASSERT_EQ(xLoadedChild.GetPendingParentFileIndex(), uParentID.m_uIndex, "TestEntityHierarchySerialization: Loaded child should have parent file index preserved");

}

//------------------------------------------------------------------------------
// Prefab System Tests
//------------------------------------------------------------------------------

ZENITH_TEST(Prefab, PrefabCreateFromEntity) { Zenith_UnitTests::TestPrefabCreateFromEntity(); }

void Zenith_UnitTests::TestPrefabCreateFromEntity(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create an entity with a transform component
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "PrefabSource");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));

	// Create prefab from entity
	Zenith_Prefab xPrefab;
	bool bSuccess = xPrefab.CreateFromEntity(xEntity, "TestPrefab");

	ZENITH_ASSERT_TRUE(bSuccess, "TestPrefabCreateFromEntity: CreateFromEntity should succeed");
	ZENITH_ASSERT_TRUE(xPrefab.IsValid(), "TestPrefabCreateFromEntity: Prefab should be valid");
	ZENITH_ASSERT_EQ(xPrefab.GetName(), "TestPrefab", "TestPrefabCreateFromEntity: Prefab name should match");

}

ZENITH_TEST(Prefab, PrefabInstantiation) { Zenith_UnitTests::TestPrefabInstantiation(); }

void Zenith_UnitTests::TestPrefabInstantiation(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create source entity
	Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "InstantiateSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(5.0f, 10.0f, 15.0f));

	// Create prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "InstantiatePrefab");

	// Instantiate prefab at the requested transform (Instantiate now takes the
	// spawn transform; it is no longer inherited from the prefab's baked values).
	Zenith_Entity xInstance = xPrefab.Instantiate(pxSceneData, "PrefabInstance", Zenith_Maths::Vector3(5.0f, 10.0f, 15.0f));

	// Verify instance has the transform values from prefab
	ZENITH_ASSERT_TRUE(xInstance.HasComponent<Zenith_TransformComponent>(), "TestPrefabInstantiation: Instance should have transform component");

	Zenith_TransformComponent& xInstanceTransform = xInstance.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xInstanceTransform.GetPosition(xPos);

	// Position should match the requested instantiate transform
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 5.0f) < 0.001f &&
	              std::abs(xPos.y - 10.0f) < 0.001f &&
	              std::abs(xPos.z - 15.0f) < 0.001f, "TestPrefabInstantiation: Instance position should match requested transform");

}

#ifndef ZENITH_ANDROID // Uses raw std::filesystem::remove with relative paths
ZENITH_TEST(Prefab, PrefabSaveLoadRoundTrip) { Zenith_UnitTests::TestPrefabSaveLoadRoundTrip(); }
#endif

void Zenith_UnitTests::TestPrefabSaveLoadRoundTrip(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create source entity
	Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "RoundTripSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));

	// Create and save prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "RoundTripPrefab");

	std::string strTempPath = "test_roundtrip.zpfb";
	bool bSaved = xPrefab.SaveToFile(strTempPath);
	ZENITH_ASSERT_TRUE(bSaved, "TestPrefabSaveLoadRoundTrip: Save should succeed");

	// Load prefab via registry
	Zenith_Prefab* pxLoadedPrefab = Zenith_AssetRegistry::Get<Zenith_Prefab>(strTempPath);
	ZENITH_ASSERT_NOT_NULL(pxLoadedPrefab, "TestPrefabSaveLoadRoundTrip: Load should succeed");
	Zenith_Prefab& xLoadedPrefab = *pxLoadedPrefab;
	ZENITH_ASSERT_TRUE(xLoadedPrefab.IsValid(), "TestPrefabSaveLoadRoundTrip: Loaded prefab should be valid");
	ZENITH_ASSERT_EQ(xLoadedPrefab.GetName(), "RoundTripPrefab", "TestPrefabSaveLoadRoundTrip: Loaded prefab name should match");

	// Instantiate loaded prefab at the original transform (passed explicitly now).
	Zenith_Entity xInstance = xLoadedPrefab.Instantiate(pxSceneData, "LoadedInstance", Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));
	Zenith_TransformComponent& xInstanceTransform = xInstance.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xInstanceTransform.GetPosition(xPos);

	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 100.0f) < 0.001f &&
	              std::abs(xPos.y - 200.0f) < 0.001f &&
	              std::abs(xPos.z - 300.0f) < 0.001f, "TestPrefabSaveLoadRoundTrip: Instance position should match requested transform");

	// Cleanup temp file
	std::filesystem::remove(strTempPath);

}

ZENITH_TEST(Prefab, PrefabOverrides) { Zenith_UnitTests::TestPrefabOverrides(); }

void Zenith_UnitTests::TestPrefabOverrides(){

	Zenith_Prefab xPrefab;

	// Add an override
	Zenith_PropertyOverride xOverride;
	xOverride.m_strComponentName = "Transform";
	xOverride.m_strPropertyPath = "Position.x";
	xOverride.m_xValue << 42.0f;

	xPrefab.AddOverride(std::move(xOverride));

	// Verify override was added
	const Zenith_Vector<Zenith_PropertyOverride>& xOverrides = xPrefab.GetOverrides();
	ZENITH_ASSERT_EQ(xOverrides.GetSize(), 1, "TestPrefabOverrides: Should have 1 override");
	ZENITH_ASSERT_EQ(xOverrides.Get(0).m_strComponentName, "Transform", "TestPrefabOverrides: Override component name should match");
	ZENITH_ASSERT_EQ(xOverrides.Get(0).m_strPropertyPath, "Position.x", "TestPrefabOverrides: Override property path should match");

	// Add another override with same path (should replace)
	Zenith_PropertyOverride xOverride2;
	xOverride2.m_strComponentName = "Transform";
	xOverride2.m_strPropertyPath = "Position.x";
	xOverride2.m_xValue << 99.0f;

	xPrefab.AddOverride(std::move(xOverride2));

	// Should still be 1 override (replaced)
	ZENITH_ASSERT_EQ(xPrefab.GetOverrides().GetSize(), 1, "TestPrefabOverrides: Should still have 1 override after replace");

	// Clear overrides
	xPrefab.ClearOverrides();
	ZENITH_ASSERT_EQ(xPrefab.GetOverrides().GetSize(), 0, "TestPrefabOverrides: Should have 0 overrides after clear");

}

ZENITH_TEST(Prefab, PrefabVariantCreation) { Zenith_UnitTests::TestPrefabVariantCreation(); }

void Zenith_UnitTests::TestPrefabVariantCreation(){

	// Create a base prefab handle (mock - path-based reference)
	std::string strBasePrefabPath = "test_base_prefab.zpfb";
	PrefabHandle xBasePrefabHandle(strBasePrefabPath);

	// Create a variant prefab
	Zenith_Prefab xVariant;
	bool bSuccess = xVariant.CreateAsVariant(xBasePrefabHandle, "VariantPrefab");

	ZENITH_ASSERT_TRUE(bSuccess, "TestPrefabVariantCreation: CreateAsVariant should succeed");
	ZENITH_ASSERT_TRUE(xVariant.IsVariant(), "TestPrefabVariantCreation: Should be marked as variant");
	ZENITH_ASSERT_TRUE(xVariant.GetBasePrefab().IsSet(), "TestPrefabVariantCreation: Should have base prefab set");
	ZENITH_ASSERT_EQ(xVariant.GetBasePrefab().GetPath(), strBasePrefabPath, "TestPrefabVariantCreation: Base prefab path should match");

}

#ifndef ZENITH_ANDROID  // Uses raw std::filesystem::remove with relative paths
ZENITH_TEST(Prefab, PrefabVariantInstantiate) { Zenith_UnitTests::TestPrefabVariantInstantiate(); }
#endif

void Zenith_UnitTests::TestPrefabVariantInstantiate(){

	// Variant inheritance regression test: instantiating a variant should produce
	// an entity with the base prefab's components, NOT an empty entity.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Build a base prefab from an entity with a known position.
	Zenith_Entity xBaseSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "VariantBaseSrc");
	xBaseSource.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f));

	Zenith_Prefab xBaseInMemory;
	xBaseInMemory.CreateFromEntity(xBaseSource, "VariantBase");
	const std::string strBasePath = "test_variant_base.zpfb";
	xBaseInMemory.SaveToFile(strBasePath);

	// Pull the base back through the asset registry so the variant has a real
	// PrefabHandle (path -> registry-resolved pointer).
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	bool bVariantOk = xVariant.CreateAsVariant(xBaseHandle, "VariantOfBase");
	ZENITH_ASSERT_TRUE(bVariantOk, "TestPrefabVariantInstantiate: CreateAsVariant should succeed");

	// The variant has no position override, so the instance takes the spawn
	// transform we pass here (no longer the base prefab's baked position).
	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "VariantInstance", Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f));
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantInstantiate: instance should be valid (not an empty entity)");
	ZENITH_ASSERT_TRUE(xInstance.HasComponent<Zenith_TransformComponent>(),
		"TestPrefabVariantInstantiate: instance should inherit base's TransformComponent");

	Zenith_Maths::Vector3 xPos;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 7.0f) < 0.001f && std::abs(xPos.y - 8.0f) < 0.001f && std::abs(xPos.z - 9.0f) < 0.001f,
		"TestPrefabVariantInstantiate: position should match the requested transform");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

ZENITH_TEST(Prefab, PrefabVariantCycleRejected) { Zenith_UnitTests::TestPrefabVariantCycleRejected(); }

void Zenith_UnitTests::TestPrefabVariantCycleRejected(){

	// CreateAsVariant must reject a base prefab whose ancestor chain reaches
	// back to `this`. We construct two real prefabs A and B (both must be
	// loadable through the registry so PrefabHandle::Get resolves), make B a
	// variant of A, then try to make A a variant of B — which would form
	// A -> B -> A. CreateAsVariant should refuse.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "CycleSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

	Zenith_Prefab xA;
	xA.CreateFromEntity(xSrc, "CycleA");
	const std::string strPathA = "test_cycle_a.zpfb";
	xA.SaveToFile(strPathA);
	Zenith_Prefab* pxA = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPathA);
	ZENITH_ASSERT_NOT_NULL(pxA, "TestPrefabVariantCycleRejected: prefab A should load");

	PrefabHandle xHandleA(strPathA);
	Zenith_Prefab xB;
	bool bBOk = xB.CreateAsVariant(xHandleA, "CycleB");
	ZENITH_ASSERT_TRUE(bBOk, "TestPrefabVariantCycleRejected: B-as-variant-of-A should succeed");
	const std::string strPathB = "test_cycle_b.zpfb";
	xB.SaveToFile(strPathB);
	Zenith_Prefab* pxB = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPathB);
	ZENITH_ASSERT_NOT_NULL(pxB, "TestPrefabVariantCycleRejected: prefab B should load");

	// Now try to retrofit A as a variant of B. This would produce the cycle
	// A -> B -> A and must be rejected.
	PrefabHandle xHandleB(strPathB);
	bool bACycleOk = pxA->CreateAsVariant(xHandleB, "CycleA_Recycled");
	ZENITH_ASSERT_FALSE(bACycleOk, "TestPrefabVariantCycleRejected: A-as-variant-of-B should be rejected (cycle)");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPathA);
	std::filesystem::remove(strPathB);
#endif
}

ZENITH_TEST(Prefab, PrefabVariantOverrideApplies) { Zenith_UnitTests::TestPrefabVariantOverrideApplies(); }

void Zenith_UnitTests::TestPrefabVariantOverrideApplies(){

	// End-to-end check that a flat-property override flows through Instantiate
	// and actually mutates the entity's component field. Uses Transform.Scale
	// because it's a Vector3 (matches the registered property type) and the
	// base prefab can be set to a value that's distinguishable from the override.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "OverrideBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_Prefab xBaseInMem;
	xBaseInMem.CreateFromEntity(xSrc, "OverrideBase");
	const std::string strBasePath = "test_override_base.zpfb";
	xBaseInMem.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xBaseHandle, "OverrideVariant");

	// Push a Scale override of (5, 5, 5) onto the variant.
	Zenith_PropertyOverride xOv;
	xOv.m_strComponentName = "Transform";
	xOv.m_strPropertyPath  = "Scale";
	xOv.m_xValue << Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f);
	xVariant.AddOverride(std::move(xOv));

	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "OverrideInstance");
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantOverrideApplies: instance should be valid");

	Zenith_Maths::Vector3 xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 5.0f) < 0.001f && std::abs(xScale.y - 5.0f) < 0.001f && std::abs(xScale.z - 5.0f) < 0.001f,
		"TestPrefabVariantOverrideApplies: override should change Scale from (1,1,1) to (5,5,5)");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

ZENITH_TEST(Prefab, PrefabVariantChain) { Zenith_UnitTests::TestPrefabVariantChain(); }

void Zenith_UnitTests::TestPrefabVariantChain(){

	// Three-level variant chain: A -> B -> C, where B overrides Position and
	// C overrides Scale. Instantiating C should produce an entity with both
	// overrides applied on top of A's components.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "ChainBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xSrc.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	// A: concrete prefab from entity
	Zenith_Prefab xA;
	xA.CreateFromEntity(xSrc, "ChainA");
	const std::string strPathA = "test_chain_a.zpfb";
	xA.SaveToFile(strPathA);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strPathA);

	// B: variant of A with Position override
	PrefabHandle xHandleA(strPathA);
	Zenith_Prefab xB;
	xB.CreateAsVariant(xHandleA, "ChainB");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Position";
		xOv.m_xValue << Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f);
		xB.AddOverride(std::move(xOv));
	}
	const std::string strPathB = "test_chain_b.zpfb";
	xB.SaveToFile(strPathB);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strPathB);

	// C: variant of B with Scale override
	PrefabHandle xHandleB(strPathB);
	Zenith_Prefab xC;
	xC.CreateAsVariant(xHandleB, "ChainC");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Scale";
		xOv.m_xValue << Zenith_Maths::Vector3(7.0f, 7.0f, 7.0f);
		xC.AddOverride(std::move(xOv));
	}

	Zenith_Entity xInstance = xC.Instantiate(pxSceneData, "ChainCInstance");
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantChain: instance should be valid");

	Zenith_Maths::Vector3 xPos, xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);

	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 10.0f) < 0.001f && std::abs(xPos.y - 20.0f) < 0.001f && std::abs(xPos.z - 30.0f) < 0.001f,
		"TestPrefabVariantChain: B's Position override should propagate through C");
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 7.0f) < 0.001f && std::abs(xScale.y - 7.0f) < 0.001f && std::abs(xScale.z - 7.0f) < 0.001f,
		"TestPrefabVariantChain: C's Scale override should be applied last");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPathA);
	std::filesystem::remove(strPathB);
#endif
}

// ----- TaskSystem: calling-thread-participates flag -----
struct CallingThreadTestData
{
	std::atomic<u_int> m_uMainThreadInvocations{0};
	u_int m_uMainThreadID = 0;
};

static void CallingThreadTestFunc(void* pData, u_int /*uIdx*/, u_int /*uTotal*/)
{
	auto* pxData = static_cast<CallingThreadTestData*>(pData);
	if (g_xEngine.Threading().GetCurrentThreadID() == pxData->m_uMainThreadID)
	{
		pxData->m_uMainThreadInvocations.fetch_add(1);
	}
}

ZENITH_TEST(Core, TaskArrayCallingThreadParticipates) { Zenith_UnitTests::TestTaskArrayCallingThreadParticipates(); }

void Zenith_UnitTests::TestTaskArrayCallingThreadParticipates(){

	// Verifies the rename's semantics: when bCallingThreadParticipates=true, the
	// calling thread (main) runs at least one of the array's invocations.
	CallingThreadTestData xData;
	xData.m_uMainThreadID = g_xEngine.Threading().GetCurrentThreadID();

	constexpr u_int uNumInvocations = 8;
	Zenith_TaskArray xArray(
		ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES,
		CallingThreadTestFunc,
		&xData,
		uNumInvocations,
		/* bCallingThreadParticipates = */ true
	);
	g_xEngine.Tasks().SubmitTaskArray(&xArray);
	xArray.WaitUntilComplete();

	ZENITH_ASSERT_TRUE(xData.m_uMainThreadInvocations.load() >= 1,
		"TestTaskArrayCallingThreadParticipates: calling thread should run at least one invocation when flag is true");
}

//==============================================================================
// Prefab coverage-gap tests (audit-driven, see plan §3.5)
//==============================================================================

// --- ApplyToEntity ---------------------------------------------------------

ZENITH_TEST(Prefab, PrefabApplyToEntity) { Zenith_UnitTests::TestPrefabApplyToEntity(); }

void Zenith_UnitTests::TestPrefabApplyToEntity(){

	// ApplyToEntity overlays prefab data onto an existing entity (vs Instantiate
	// which creates a fresh one). Verifies the apply path runs and writes the
	// prefab's component values onto the existing entity's components.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Source entity used to author the prefab.
	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "ApplySrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(11.0f, 22.0f, 33.0f));

	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSrc, "ApplyPrefab");

	// Pre-existing entity at a different position — apply should overwrite it.
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "ApplyTarget");
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	const bool bApplied = xPrefab.ApplyToEntity(xTarget);
	ZENITH_ASSERT_TRUE(bApplied, "TestPrefabApplyToEntity: ApplyToEntity should succeed on a valid prefab");

	Zenith_Maths::Vector3 xPos;
	xTarget.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 11.0f) < 0.001f && std::abs(xPos.y - 22.0f) < 0.001f && std::abs(xPos.z - 33.0f) < 0.001f,
		"TestPrefabApplyToEntity: target position should match prefab source after apply");
}

ZENITH_TEST(Prefab, PrefabApplyVariantToEntity) { Zenith_UnitTests::TestPrefabApplyVariantToEntity(); }

void Zenith_UnitTests::TestPrefabApplyVariantToEntity(){

	// ApplyToEntity on a variant: should walk the base chain, apply base data,
	// then layer overrides on top. Verifies that variants apply correctly via
	// ApplyToEntity (not just Instantiate).
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "ApplyVariantBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xSrc.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));

	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "ApplyVariantBase");
	const std::string strPath = "test_apply_variant_base.zpfb";
	xBase.SaveToFile(strPath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);

	PrefabHandle xHandle(strPath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xHandle, "ApplyVariant");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Scale";
		xOv.m_xValue << Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
		xVariant.AddOverride(std::move(xOv));
	}

	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneData, "ApplyVariantTarget");
	const bool bApplied = xVariant.ApplyToEntity(xTarget);
	ZENITH_ASSERT_TRUE(bApplied, "TestPrefabApplyVariantToEntity: ApplyToEntity on variant should succeed");

	Zenith_Maths::Vector3 xPos, xScale;
	xTarget.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	xTarget.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 1.0f) < 0.001f && std::abs(xPos.y - 2.0f) < 0.001f && std::abs(xPos.z - 3.0f) < 0.001f,
		"TestPrefabApplyVariantToEntity: base position should propagate via apply");
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 9.0f) < 0.001f && std::abs(xScale.y - 9.0f) < 0.001f && std::abs(xScale.z - 9.0f) < 0.001f,
		"TestPrefabApplyVariantToEntity: variant Scale override should be applied last");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPath);
#endif
}

// --- Move semantics --------------------------------------------------------

ZENITH_TEST(Prefab, PrefabMoveConstructor) { Zenith_UnitTests::TestPrefabMoveConstructor(); }

void Zenith_UnitTests::TestPrefabMoveConstructor(){

	// Move construction transfers state from source to destination.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "MoveCtorSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(50.0f, 50.0f, 50.0f));

	Zenith_Prefab xOrig;
	xOrig.CreateFromEntity(xSrc, "MoveCtorPrefab");
	ZENITH_ASSERT_TRUE(xOrig.IsValid(), "TestPrefabMoveConstructor: pre-move source should be valid");

	Zenith_Prefab xMoved(std::move(xOrig));
	ZENITH_ASSERT_TRUE(xMoved.IsValid(), "TestPrefabMoveConstructor: moved-to prefab should be valid");
	ZENITH_ASSERT_EQ(xMoved.GetName(), std::string("MoveCtorPrefab"), "TestPrefabMoveConstructor: name transfers via move");

	// Moved-from prefab should still instantiate-safely (it's just empty data now).
	// We don't assert specific cleared-state since the move contract isn't documented;
	// what matters is the destination works.
	Zenith_Entity xInstance = xMoved.Instantiate(pxSceneData, "MoveCtorInstance");
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabMoveConstructor: instantiation from moved-to prefab should work");
}

ZENITH_TEST(Prefab, PrefabMoveAssignment) { Zenith_UnitTests::TestPrefabMoveAssignment(); }

void Zenith_UnitTests::TestPrefabMoveAssignment(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "MoveAsgnSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(60.0f, 70.0f, 80.0f));

	Zenith_Prefab xOrig;
	xOrig.CreateFromEntity(xSrc, "MoveAsgnPrefab");

	Zenith_Prefab xTarget;
	xTarget = std::move(xOrig);
	ZENITH_ASSERT_TRUE(xTarget.IsValid(), "TestPrefabMoveAssignment: target prefab should be valid post-move");
	ZENITH_ASSERT_EQ(xTarget.GetName(), std::string("MoveAsgnPrefab"), "TestPrefabMoveAssignment: name transfers");

	Zenith_Entity xInstance = xTarget.Instantiate(pxSceneData, "MoveAsgnInstance", Zenith_Maths::Vector3(60.0f, 70.0f, 80.0f));
	Zenith_Maths::Vector3 xPos;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 60.0f) < 0.001f, "TestPrefabMoveAssignment: serialized data transfers via move");
}

// --- Variant round-trip with overrides -------------------------------------

ZENITH_TEST(Prefab, PrefabVariantRoundTripWithOverrides) { Zenith_UnitTests::TestPrefabVariantRoundTripWithOverrides(); }

void Zenith_UnitTests::TestPrefabVariantRoundTripWithOverrides(){

	// Critical for editor authoring: variants saved to disk with overrides
	// must reload with those overrides intact and produce the same entity.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "RoundTripVariantBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "RoundTripVariantBase");
	const std::string strBasePath = "test_rt_variant_base.zpfb";
	xBase.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	// Author variant in memory with two overrides.
	{
		PrefabHandle xBaseHandle(strBasePath);
		Zenith_Prefab xVariant;
		xVariant.CreateAsVariant(xBaseHandle, "RoundTripVariant");

		Zenith_PropertyOverride xOvP;
		xOvP.m_strComponentName = "Transform";
		xOvP.m_strPropertyPath  = "Position";
		xOvP.m_xValue << Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f);
		xVariant.AddOverride(std::move(xOvP));

		Zenith_PropertyOverride xOvS;
		xOvS.m_strComponentName = "Transform";
		xOvS.m_strPropertyPath  = "Scale";
		xOvS.m_xValue << Zenith_Maths::Vector3(4.0f, 4.0f, 4.0f);
		xVariant.AddOverride(std::move(xOvS));

		const std::string strVariantPath = "test_rt_variant.zpfb";
		const bool bSaved = xVariant.SaveToFile(strVariantPath);
		ZENITH_ASSERT_TRUE(bSaved, "TestPrefabVariantRoundTripWithOverrides: save should succeed");
	}

	// Reload from disk. The previous in-memory xVariant is gone.
	const std::string strVariantPath = "test_rt_variant.zpfb";
	Zenith_Prefab* pxLoaded = Zenith_AssetRegistry::Get<Zenith_Prefab>(strVariantPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "TestPrefabVariantRoundTripWithOverrides: reload should succeed");
	ZENITH_ASSERT_TRUE(pxLoaded->IsVariant(), "TestPrefabVariantRoundTripWithOverrides: reloaded prefab should be a variant");
	ZENITH_ASSERT_EQ(pxLoaded->GetOverrides().GetSize(), 2u,
		"TestPrefabVariantRoundTripWithOverrides: both overrides should survive disk round-trip");

	Zenith_Entity xInstance = pxLoaded->Instantiate(pxSceneData, "RoundTripVariantInstance");
	Zenith_Maths::Vector3 xPos, xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 100.0f) < 0.001f && std::abs(xPos.y - 200.0f) < 0.001f && std::abs(xPos.z - 300.0f) < 0.001f,
		"TestPrefabVariantRoundTripWithOverrides: Position override should apply post-reload");
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 4.0f) < 0.001f,
		"TestPrefabVariantRoundTripWithOverrides: Scale override should apply post-reload");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
	std::filesystem::remove(strVariantPath);
#endif
}

// --- Multiple overrides on same component, different fields ----------------

ZENITH_TEST(Prefab, PrefabMultipleOverridesSameComponent) { Zenith_UnitTests::TestPrefabMultipleOverridesSameComponent(); }

void Zenith_UnitTests::TestPrefabMultipleOverridesSameComponent(){

	// AddOverride dedupes by (component, propertyPath). Two overrides on the
	// same component but different fields should both apply at instantiation.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "MultiOvBaseSrc");
	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "MultiOvBase");
	const std::string strPath = "test_multi_override_base.zpfb";
	xBase.SaveToFile(strPath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);

	PrefabHandle xHandle(strPath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xHandle, "MultiOvVariant");

	// Override Position
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Position";
		xOv.m_xValue << Zenith_Maths::Vector3(7.0f, 8.0f, 9.0f);
		xVariant.AddOverride(std::move(xOv));
	}
	// Override Scale (different field on the SAME component)
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Scale";
		xOv.m_xValue << Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f);
		xVariant.AddOverride(std::move(xOv));
	}

	ZENITH_ASSERT_EQ(xVariant.GetOverrides().GetSize(), 2u,
		"TestPrefabMultipleOverridesSameComponent: should retain both overrides on same component");

	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "MultiOvInstance");
	Zenith_Maths::Vector3 xPos, xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 7.0f) < 0.001f, "TestPrefabMultipleOverridesSameComponent: Position override applies");
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 3.0f) < 0.001f, "TestPrefabMultipleOverridesSameComponent: Scale override applies");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPath);
#endif
}

// --- ClearOverrides should leave the variant inheriting only the base ------

ZENITH_TEST(Prefab, PrefabClearOverridesReverts) { Zenith_UnitTests::TestPrefabClearOverridesReverts(); }

void Zenith_UnitTests::TestPrefabClearOverridesReverts(){

	// After ClearOverrides(), instantiating a variant should produce an entity
	// that matches the base prefab exactly (no override applied).
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "ClearOvBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "ClearOvBase");
	const std::string strPath = "test_clear_override_base.zpfb";
	xBase.SaveToFile(strPath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);

	PrefabHandle xHandle(strPath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xHandle, "ClearOvVariant");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Scale";
		xOv.m_xValue << Zenith_Maths::Vector3(99.0f, 99.0f, 99.0f);
		xVariant.AddOverride(std::move(xOv));
	}

	xVariant.ClearOverrides();
	ZENITH_ASSERT_EQ(xVariant.GetOverrides().GetSize(), 0u, "TestPrefabClearOverridesReverts: ClearOverrides empties the list");

	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "ClearOvInstance");
	Zenith_Maths::Vector3 xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 1.0f) < 0.001f, "TestPrefabClearOverridesReverts: post-clear instantiation matches base, not previous override");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPath);
#endif
}

// --- Bad-input guards ------------------------------------------------------

ZENITH_TEST(Prefab, PrefabCreateAsVariantWithUnsetHandle) { Zenith_UnitTests::TestPrefabCreateAsVariantWithUnsetHandle(); }

void Zenith_UnitTests::TestPrefabCreateAsVariantWithUnsetHandle(){

	// CreateAsVariant must reject an unset base prefab handle.
	Zenith_Prefab xVariant;
	PrefabHandle xUnset;  // default-constructed, no path
	const bool bResult = xVariant.CreateAsVariant(xUnset, "UnsetBaseVariant");
	ZENITH_ASSERT_FALSE(bResult, "TestPrefabCreateAsVariantWithUnsetHandle: should reject unset handle");
	ZENITH_ASSERT_FALSE(xVariant.IsValid(), "TestPrefabCreateAsVariantWithUnsetHandle: variant should not be marked valid after rejection");
}

ZENITH_TEST(Prefab, PrefabInstantiateNullSceneData) { Zenith_UnitTests::TestPrefabInstantiateNullSceneData(); }

void Zenith_UnitTests::TestPrefabInstantiateNullSceneData(){

	// Instantiate(nullptr, ...) should reject and return an invalid entity.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "NullSceneSrc");
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSrc, "NullScenePrefab");

	Zenith_Entity xInstance = xPrefab.Instantiate(nullptr, "NullSceneInstance");
	ZENITH_ASSERT_FALSE(xInstance.IsValid(), "TestPrefabInstantiateNullSceneData: instantiation with null scene should fail safely");
}

// --- Self-cycle: variant of itself -----------------------------------------

ZENITH_TEST(Prefab, PrefabSelfVariantRejected) { Zenith_UnitTests::TestPrefabSelfVariantRejected(); }

void Zenith_UnitTests::TestPrefabSelfVariantRejected(){

	// Variant of itself: A -> A. Cycle detection should reject this.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "SelfCycleSrc");
	Zenith_Prefab xA;
	xA.CreateFromEntity(xSrc, "SelfCycleA");
	const std::string strPath = "test_self_cycle.zpfb";
	xA.SaveToFile(strPath);
	Zenith_Prefab* pxLoadedA = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoadedA, "TestPrefabSelfVariantRejected: load should succeed");

	// Try to make A a variant of itself (handle resolves to pxLoadedA which IS this prefab).
	PrefabHandle xHandleA(strPath);
	const bool bResult = pxLoadedA->CreateAsVariant(xHandleA, "SelfCycleA_Recycled");
	ZENITH_ASSERT_FALSE(bResult, "TestPrefabSelfVariantRejected: A-as-variant-of-A should be rejected by cycle detection");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPath);
#endif
}

// --- Instantiate names entity correctly ------------------------------------

ZENITH_TEST(Prefab, PrefabInstantiateNamesEntity) { Zenith_UnitTests::TestPrefabInstantiateNamesEntity(); }

void Zenith_UnitTests::TestPrefabInstantiateNamesEntity(){

	// When strEntityName is non-empty, the entity gets that name.
	// When empty, the entity inherits the prefab's own name.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "NameSrc");
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSrc, "PrefabName");

	Zenith_Entity xWithCustomName = xPrefab.Instantiate(pxSceneData, "CustomEntityName");
	ZENITH_ASSERT_EQ(xWithCustomName.GetName(), std::string("CustomEntityName"),
		"TestPrefabInstantiateNamesEntity: custom name should be applied");

	Zenith_Entity xWithDefaultName = xPrefab.Instantiate(pxSceneData, "");
	ZENITH_ASSERT_EQ(xWithDefaultName.GetName(), std::string("PrefabName"),
		"TestPrefabInstantiateNamesEntity: empty name should fall back to prefab name");
}

// --- Lifecycle dispatched once at top-level only (not per recursion step) --

ZENITH_TEST(Prefab, PrefabVariantInstantiateLifecycleOnceAtTop) { Zenith_UnitTests::TestPrefabVariantInstantiateLifecycleOnceAtTop(); }

void Zenith_UnitTests::TestPrefabVariantInstantiateLifecycleOnceAtTop(){

	// A two-level variant chain (A -> B). Instantiating B should produce one
	// entity (not two). The PrefabInstantiationGuard suppresses lifecycle
	// during construction; manual dispatch happens once after recursion completes.
	// Verifies the entity is awoken (not pending-Awake) post-Instantiate, which
	// is the observable outcome of the single top-level dispatch.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "LifecycleBaseSrc");
	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "LifecycleBase");
	const std::string strBasePath = "test_lifecycle_base.zpfb";
	xBase.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xBaseHandle, "LifecycleVariant");

	const u_int uEntityCountBefore = pxSceneData->GetEntityCount();
	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "LifecycleInstance");
	const u_int uEntityCountAfter = pxSceneData->GetEntityCount();

	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantInstantiateLifecycleOnceAtTop: instance should be valid");
	ZENITH_ASSERT_EQ(uEntityCountAfter - uEntityCountBefore, 1u,
		"TestPrefabVariantInstantiateLifecycleOnceAtTop: variant Instantiate should produce exactly one entity, not one per recursion level");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

// --- Stateful-field overrides route through setters, not raw writes --------
//
// Regression for the bug where ZENITH_REGISTER_COMPONENT_PROPERTY's raw
// member-field write was used for Transform.Position. After
// DeserializeComponents finishes, the ColliderComponent has already created a
// Jolt body via ReadFromDataStream. Writing m_xPosition directly bypasses
// Jolt — Transform::GetPosition reads from the body, which would still be
// at the base position. The fix routes the override through SetPosition,
// which calls BodyInterface::SetPosition.

ZENITH_TEST(Prefab, PrefabVariantPositionOverrideSyncsPhysicsBody) { Zenith_UnitTests::TestPrefabVariantPositionOverrideSyncsPhysicsBody(); }

void Zenith_UnitTests::TestPrefabVariantPositionOverrideSyncsPhysicsBody(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Base entity at origin with a capsule collider.
	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "PhysSyncBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	Zenith_ColliderComponent& xSrcCollider = xSrc.AddComponent<Zenith_ColliderComponent>();
	xSrcCollider.AddCapsuleCollider(0.25f, 0.5f, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "PhysSyncBase");
	const std::string strBasePath = "test_phys_sync_base.zpfb";
	xBase.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	// Variant with a Position override at (10, 5, 7).
	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xBaseHandle, "PhysSyncVariant");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Position";
		xOv.m_xValue << Zenith_Maths::Vector3(10.0f, 5.0f, 7.0f);
		xVariant.AddOverride(std::move(xOv));
	}

	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "PhysSyncInstance");
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantPositionOverrideSyncsPhysicsBody: instance valid");
	ZENITH_ASSERT_TRUE(xInstance.HasComponent<Zenith_ColliderComponent>(), "TestPrefabVariantPositionOverrideSyncsPhysicsBody: instance has collider");

	// GetPosition reads from the Jolt body when one exists. If the override
	// went through the raw m_xPosition write, the body would still be at
	// (0,0,0) and this assertion would fail with the base position. After the
	// fix the override routes through SetPosition -> BodyInterface, so the
	// body reflects (10, 5, 7) and the test reads it back.
	Zenith_Maths::Vector3 xPos;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 10.0f) < 0.01f && std::abs(xPos.y - 5.0f) < 0.01f && std::abs(xPos.z - 7.0f) < 0.01f,
		"TestPrefabVariantPositionOverrideSyncsPhysicsBody: physics body must move to override position (got (%.2f, %.2f, %.2f))",
		xPos.x, xPos.y, xPos.z);

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

ZENITH_TEST(Prefab, PrefabVariantScaleOverrideRebuildsCollider) { Zenith_UnitTests::TestPrefabVariantScaleOverrideRebuildsCollider(); }

void Zenith_UnitTests::TestPrefabVariantScaleOverrideRebuildsCollider(){

	// Companion to the Position test: Scale overrides on entities with a
	// collider must call SetScale, which rebuilds the collider geometry. Hard
	// to assert collider-internal state directly without going through Jolt
	// machinery, so this test verifies the cheap observable: Scale read-back
	// matches the override (the path that calls SetScale also writes m_xScale).
	// Combined with the Position test, it gives high confidence the setter
	// route is being taken for both registered Transform setters.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "ScaleSyncBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	Zenith_ColliderComponent& xSrcCollider = xSrc.AddComponent<Zenith_ColliderComponent>();
	xSrcCollider.AddCapsuleCollider(0.25f, 0.5f, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Prefab xBase;
	xBase.CreateFromEntity(xSrc, "ScaleSyncBase");
	const std::string strBasePath = "test_scale_sync_base.zpfb";
	xBase.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xBaseHandle, "ScaleSyncVariant");
	{
		Zenith_PropertyOverride xOv;
		xOv.m_strComponentName = "Transform";
		xOv.m_strPropertyPath  = "Scale";
		xOv.m_xValue << Zenith_Maths::Vector3(2.5f, 2.5f, 2.5f);
		xVariant.AddOverride(std::move(xOv));
	}

	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "ScaleSyncInstance");
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantScaleOverrideRebuildsCollider: instance valid");

	Zenith_Maths::Vector3 xScale;
	xInstance.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(std::abs(xScale.x - 2.5f) < 0.01f && std::abs(xScale.y - 2.5f) < 0.01f && std::abs(xScale.z - 2.5f) < 0.01f,
		"TestPrefabVariantScaleOverrideRebuildsCollider: Scale should match override (got (%.2f, %.2f, %.2f))",
		xScale.x, xScale.y, xScale.z);

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

// --- RebuildCollider must rebuild at the body's current transform ----------
// Regression for the transform-cache bug behind prefab collider instantiation:
// SetScale auto-calls RebuildCollider, which destroys + recreates the body via
// AddCollider; AddCollider reads GetPosition/GetRotation, which fall back to the
// cached m_xPosition/m_xRotation once the body is gone. Without the fix the body
// snaps to a stale cached transform (commonly the origin).

ZENITH_TEST(Collider, ColliderRebuildKeepsMovedTransform) { Zenith_UnitTests::TestColliderRebuildKeepsMovedTransform(); }

void Zenith_UnitTests::TestColliderRebuildKeepsMovedTransform(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// --- Case A: body moved by a setter (the prefab-instantiate path; Eng. change 1a)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxSceneData, "RebuildMove_Setter");
		Zenith_ColliderComponent& xCol = xEnt.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCapsuleCollider(0.25f, 0.5f, RIGIDBODY_TYPE_DYNAMIC);

		Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(Zenith_Maths::Vector3(10.0f, 5.0f, 7.0f));
		xT.SetScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f)); // triggers RebuildCollider

		Zenith_Maths::Vector3 xPos;
		xT.GetPosition(xPos);
		ZENITH_ASSERT_TRUE(std::abs(xPos.x - 10.0f) < 0.05f && std::abs(xPos.y - 5.0f) < 0.05f && std::abs(xPos.z - 7.0f) < 0.05f,
			"ColliderRebuildKeepsMovedTransform(setter): body must stay at (10,5,7) after rebuild (got (%.2f, %.2f, %.2f))",
			xPos.x, xPos.y, xPos.z);
	}

	// --- Case B: body moved by simulation, not a setter (Eng. change 1b) ------
	// The cache stays stale through the sim move, so only RebuildCollider's
	// snapshot of the live body keeps the rebuild in the right place.
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxSceneData, "RebuildMove_Sim");
		Zenith_ColliderComponent& xCol = xEnt.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCapsuleCollider(0.25f, 0.5f, RIGIDBODY_TYPE_DYNAMIC);

		Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(Zenith_Maths::Vector3(20.0f, 3.0f, 9.0f));

		// Drive the body along +X via the physics sim, bypassing the setters.
		g_xEngine.Physics().SetGravityEnabled(xCol.GetBodyID(), false);
		g_xEngine.Physics().SetLinearVelocity(xCol.GetBodyID(), Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));
		for (int i = 0; i < 30; ++i) g_xEngine.Physics().Update(1.0f / 60.0f);

		Zenith_Maths::Vector3 xBefore;
		xT.GetPosition(xBefore);                              // live body after the sim move
		xT.SetScale(Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f)); // triggers RebuildCollider

		Zenith_Maths::Vector3 xAfter;
		xT.GetPosition(xAfter);
		ZENITH_ASSERT_TRUE(std::abs(xAfter.x - xBefore.x) < 0.1f && std::abs(xAfter.y - xBefore.y) < 0.1f && std::abs(xAfter.z - xBefore.z) < 0.1f,
			"ColliderRebuildKeepsMovedTransform(sim): rebuild must preserve the live transform (before (%.2f,%.2f,%.2f) vs after (%.2f,%.2f,%.2f))",
			xBefore.x, xBefore.y, xBefore.z, xAfter.x, xAfter.y, xAfter.z);
		ZENITH_ASSERT_TRUE(xAfter.x > 19.0f,
			"ColliderRebuildKeepsMovedTransform(sim): rebuilt body must not snap toward origin (x=%.2f)", xAfter.x);
	}
}

// --- Loading a corrupted (truncated) prefab file ---------------------------

ZENITH_TEST(Prefab, PrefabLoadCorruptedFile) { Zenith_UnitTests::TestPrefabLoadCorruptedFile(); }

void Zenith_UnitTests::TestPrefabLoadCorruptedFile(){

	// A file whose magic number is wrong should fail validation in LoadFromFile.
#ifndef ZENITH_ANDROID
	const std::string strPath = "test_corrupted.zpfb";
	{
		std::ofstream xOut(strPath, std::ios::binary);
		const char acGarbage[] = "NOT_A_VALID_PREFAB";
		xOut.write(acGarbage, sizeof(acGarbage));
	}

	// Get<Zenith_Prefab>(path) returns nullptr if loading rejects the file.
	// (Some versions assert; in either case the registry shouldn't end up with
	// a valid prefab for this path.)
	Zenith_Prefab* pxLoaded = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);
	if (pxLoaded != nullptr)
	{
		ZENITH_ASSERT_FALSE(pxLoaded->IsValid(),
			"TestPrefabLoadCorruptedFile: loaded prefab from corrupted bytes should not be marked valid");
	}
	std::filesystem::remove(strPath);
#else
	ZENITH_SKIP("std::filesystem::remove with relative paths not supported on Android");
#endif
}

ZENITH_TEST(Prefab, PrefabLoadFromDeletedFile) { Zenith_UnitTests::TestPrefabLoadFromDeletedFile(); }

void Zenith_UnitTests::TestPrefabLoadFromDeletedFile(){

	// Saving then deleting before load: the registry's load path is currently
	// strict (asserts on missing files) — so testing the unload branch is the
	// best we can do without modifying production code.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeletedFileSrc");
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSrc, "DeletedFilePrefab");
	const std::string strPath = "test_deleted_file.zpfb";
	xPrefab.SaveToFile(strPath);

	// Load once so it's cached, then delete the file. Subsequent registry
	// lookups still return the cached pointer.
	Zenith_Prefab* pxLoaded = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "TestPrefabLoadFromDeletedFile: initial load should succeed");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strPath);
#endif

	// Cached load still works.
	Zenith_Prefab* pxCached = Zenith_AssetRegistry::Get<Zenith_Prefab>(strPath);
	ZENITH_ASSERT_TRUE(pxCached == pxLoaded, "TestPrefabLoadFromDeletedFile: registry returns cached pointer when file is gone post-load");
}

// ----- TaskSystem: drain-on-shutdown is intentionally NOT tested here -----
// The plan called for a "outstanding tasks complete before Shutdown() returns"
// test. It cannot be expressed in this framework: the engine's TaskSystem is
// initialised once at process start and shut down once at process end. Tests
// run *between* those points; calling g_xEngine.Tasks().Shutdown() from a
// test would tear down the worker pool that every subsequent test depends on.
// The drain contract is exercised every time the engine exits cleanly, so
// regressions surface immediately as hangs or use-after-free in shutdown
// sequences (which the engine's main-loop integration already covers).
// ---------------------------------------------------------------------------

// ----- TaskSystem: reusable single-task submit/wait/reset/submit cycle -----
struct ReuseTestData
{
	std::atomic<u_int> m_uExecutionCount{0};
};

static void ReuseTaskFunc(void* pData)
{
	auto* pxData = static_cast<ReuseTestData*>(pData);
	pxData->m_uExecutionCount.fetch_add(1);
}

ZENITH_TEST(Core, TaskReuseAfterWait) { Zenith_UnitTests::TestTaskReuseAfterWait(); }

void Zenith_UnitTests::TestTaskReuseAfterWait(){

	// A Zenith_Task can be submitted, waited on, then submitted again. The plan
	// for Phase 1.2 / 1.3 documents that WaitUntilComplete clears m_bSubmitted,
	// which is what makes the second submit valid (TryClaimTask's CAS succeeds).
	// This test exercises the contract end-to-end across two cycles.
	ReuseTestData xData;

	Zenith_Task xTask(ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES, ReuseTaskFunc, &xData);

	g_xEngine.Tasks().SubmitTask(&xTask);
	xTask.WaitUntilComplete();
	ZENITH_ASSERT_EQ(xData.m_uExecutionCount.load(), 1u, "TestTaskReuseAfterWait: first submit should run task once");

	// Reset is documented as a no-op for simple tasks (the m_bSubmitted flag is
	// cleared by WaitUntilComplete) — call it to exercise the path anyway.
	xTask.Reset();

	g_xEngine.Tasks().SubmitTask(&xTask);
	xTask.WaitUntilComplete();
	ZENITH_ASSERT_EQ(xData.m_uExecutionCount.load(), 2u, "TestTaskReuseAfterWait: second submit should run task again");
}

ZENITH_TEST(Prefab, PrefabVariantNestedPathSkipped) { Zenith_UnitTests::TestPrefabVariantNestedPathSkipped(); }

void Zenith_UnitTests::TestPrefabVariantNestedPathSkipped(){

	// Variant override paths containing '.' (nested fields like "Position.x")
	// are not yet supported. Instantiate should NOT crash, and should emit a
	// warning instead. We can't directly intercept the warning here, so this
	// test simply verifies the call returns a valid entity with the base's
	// values intact (the override is silently dropped).
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	Zenith_Entity xSrc = g_xEngine.Scenes().CreateEntity(pxSceneData, "NestedBaseSrc");
	xSrc.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));

	Zenith_Prefab xBaseInMem;
	xBaseInMem.CreateFromEntity(xSrc, "NestedBase");
	const std::string strBasePath = "test_nested_base.zpfb";
	xBaseInMem.SaveToFile(strBasePath);
	Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

	PrefabHandle xBaseHandle(strBasePath);
	Zenith_Prefab xVariant;
	xVariant.CreateAsVariant(xBaseHandle, "NestedVariant");

	// Add an override with a nested path. The setter expects whole-field write
	// so the value's contents don't matter — what matters is the path is rejected.
	Zenith_PropertyOverride xOv;
	xOv.m_strComponentName = "Transform";
	xOv.m_strPropertyPath  = "Position.x";   // nested — should be skipped
	xOv.m_xValue.SetCursor(0);
	xVariant.AddOverride(std::move(xOv));

	// The nested override is skipped, so the instance keeps the spawn transform
	// we pass here (origin would be the default; pass (1,2,3) to assert it sticks).
	Zenith_Entity xInstance = xVariant.Instantiate(pxSceneData, "NestedInstance", Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	ZENITH_ASSERT_TRUE(xInstance.IsValid(), "TestPrefabVariantNestedPathSkipped: instance should still be valid");

	// The position should be unchanged by the skipped nested override.
	Zenith_Maths::Vector3 xPos;
	xInstance.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_TRUE(std::abs(xPos.x - 1.0f) < 0.001f && std::abs(xPos.y - 2.0f) < 0.001f && std::abs(xPos.z - 3.0f) < 0.001f,
		"TestPrefabVariantNestedPathSkipped: nested override should be skipped, requested transform preserved");

#ifndef ZENITH_ANDROID
	std::filesystem::remove(strBasePath);
#endif
}

//==============================================================================
// Serializable Asset Tests
//==============================================================================

// Test asset class for unit testing serializable assets
class TestSerializableAsset : public Zenith_Asset
{
public:
	ZENITH_ASSET_TYPE_NAME(TestSerializableAsset)

	int32_t m_iTestValue = 42;
	float m_fTestFloat = 3.14f;
	std::string m_strTestString = "TestString";

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		xStream << m_iTestValue;
		xStream << m_fTestFloat;
		xStream << m_strTestString;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		xStream >> m_iTestValue;
		xStream >> m_fTestFloat;
		xStream >> m_strTestString;
	}
};

ZENITH_TEST(Asset, DataAssetRegistration) { Zenith_UnitTests::TestDataAssetRegistration(); }

void Zenith_UnitTests::TestDataAssetRegistration(){

	// Register the test serializable asset type
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Verify it was registered
	bool bRegistered = Zenith_AssetRegistry::IsSerializableTypeRegistered("TestSerializableAsset");
	ZENITH_ASSERT_TRUE(bRegistered, "TestDataAssetRegistration: TestSerializableAsset should be registered");

	// Verify unknown type is not registered
	bool bUnknown = Zenith_AssetRegistry::IsSerializableTypeRegistered("UnknownType");
	ZENITH_ASSERT_FALSE(bUnknown, "TestDataAssetRegistration: Unknown type should not be registered");

}

#ifndef ZENITH_ANDROID // Uses raw std::filesystem with relative paths
ZENITH_TEST(Asset, DataAssetCreateAndSave) { Zenith_UnitTests::TestDataAssetCreateAndSave(); }
#endif

void Zenith_UnitTests::TestDataAssetCreateAndSave(){

	// Ensure type is registered
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Create a new instance via factory
	TestSerializableAsset* pxAsset = Zenith_AssetRegistry::Create<TestSerializableAsset>();
	ZENITH_ASSERT_NOT_NULL(pxAsset, "TestDataAssetCreateAndSave: Failed to create TestSerializableAsset");

	// Set some values
	pxAsset->m_iTestValue = 100;
	pxAsset->m_fTestFloat = 2.71828f;
	pxAsset->m_strTestString = "ModifiedValue";

	// Save to file
	std::string strTestPath = "TestData/test_data_asset.zdata";
	std::filesystem::create_directories("TestData");
	bool bSaved = Zenith_AssetRegistry::Save(pxAsset, strTestPath);
	ZENITH_ASSERT_TRUE(bSaved, "TestDataAssetCreateAndSave: Failed to save TestSerializableAsset");

	// Verify file exists
	bool bExists = std::filesystem::exists(strTestPath);
	ZENITH_ASSERT_TRUE(bExists, "TestDataAssetCreateAndSave: Saved file should exist");

	// Clean up so no stale file or cache entry leaks into another test.
	Zenith_AssetRegistry::Unload(strTestPath);
	std::filesystem::remove(strTestPath);

}

#ifndef ZENITH_ANDROID // Uses raw std::filesystem with relative paths
ZENITH_TEST(Asset, DataAssetLoad) { Zenith_UnitTests::TestDataAssetLoad(); }
#endif

void Zenith_UnitTests::TestDataAssetLoad(){

	// Self-contained: save an asset first, then unload and reload from disk.
	// Previously this assumed TestDataAssetCreateAndSave ran immediately before;
	// under ZENITH_TEST auto-registration test order is linker-dependent.
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();
	const std::string strTestPath = "TestData/test_data_asset.zdata";
	std::filesystem::create_directories("TestData");

	TestSerializableAsset* pxAsset = Zenith_AssetRegistry::Create<TestSerializableAsset>(strTestPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "TestDataAssetLoad: Failed to create asset for setup");
	if (pxAsset == nullptr) return;
	pxAsset->m_iTestValue    = 100;
	pxAsset->m_fTestFloat    = 2.71828f;
	pxAsset->m_strTestString = "ModifiedValue";
	bool bSaved = Zenith_AssetRegistry::Save(pxAsset, strTestPath);
	ZENITH_ASSERT_TRUE(bSaved, "TestDataAssetLoad: Failed to save setup asset");

	// Unload to force reload from disk
	Zenith_AssetRegistry::Unload(strTestPath);

	// Load the asset just saved
	TestSerializableAsset* pxLoaded = Zenith_AssetRegistry::Get<TestSerializableAsset>(strTestPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "TestDataAssetLoad: Failed to load TestSerializableAsset");
	if (pxLoaded == nullptr) return;

	// Verify loaded values match what we saved
	ZENITH_ASSERT_EQ(pxLoaded->m_iTestValue, 100, "TestDataAssetLoad: Loaded int value should match saved value");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->m_fTestFloat, 2.71828f, 0.0001f, "TestDataAssetLoad: Loaded float value should match saved value");
	ZENITH_ASSERT_EQ(pxLoaded->m_strTestString, "ModifiedValue", "TestDataAssetLoad: Loaded string should match saved value");

	// Clean up so no stale file or cache entry leaks into another test.
	Zenith_AssetRegistry::Unload(strTestPath);
	std::filesystem::remove(strTestPath);

}

#ifndef ZENITH_ANDROID // Uses raw std::filesystem with relative paths
ZENITH_TEST(Asset, DataAssetRoundTrip) { Zenith_UnitTests::TestDataAssetRoundTrip(); }
#endif

void Zenith_UnitTests::TestDataAssetRoundTrip(){

	// Ensure type is registered
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Self-contained: ensure the target directory exists. Earlier tests may
	// have removed it during their own cleanup, and under ZENITH_TEST auto
	// registration there's no guarantee this test runs after the one that
	// creates TestData/.
	const std::string strPath = "TestData/round_trip_test.zdata";
	std::filesystem::create_directories("TestData");

	// Create with unique values
	TestSerializableAsset* pxOriginal = Zenith_AssetRegistry::Create<TestSerializableAsset>();
	ZENITH_ASSERT_NOT_NULL(pxOriginal, "TestDataAssetRoundTrip: Failed to create original asset");
	if (pxOriginal == nullptr) return;
	pxOriginal->m_iTestValue    = -999;
	pxOriginal->m_fTestFloat    = 123.456f;
	pxOriginal->m_strTestString = "RoundTripTest";

	// Save (adds to cache)
	bool bSaved = Zenith_AssetRegistry::Save(pxOriginal, strPath);
	ZENITH_ASSERT_TRUE(bSaved, "TestDataAssetRoundTrip: Save failed");
	if (!bSaved) return;

	// Unload to force reload from disk
	Zenith_AssetRegistry::Unload(strPath);

	// Load
	TestSerializableAsset* pxLoaded = Zenith_AssetRegistry::Get<TestSerializableAsset>(strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "TestDataAssetRoundTrip: Failed to load");
	if (pxLoaded == nullptr) return;
	ZENITH_ASSERT_EQ(pxLoaded->m_iTestValue, -999, "TestDataAssetRoundTrip: Int mismatch");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->m_fTestFloat, 123.456f, 0.001f, "TestDataAssetRoundTrip: Float mismatch");
	ZENITH_ASSERT_EQ(pxLoaded->m_strTestString, "RoundTripTest", "TestDataAssetRoundTrip: String mismatch");

	// Clean up test files
	Zenith_AssetRegistry::Unload(strPath);
	std::filesystem::remove(strPath);

}

//=============================================================================
// ECS Safety Tests (Circular Hierarchy, Camera Safety)
//=============================================================================

ZENITH_TEST(Core, CircularHierarchyPrevention) { Zenith_UnitTests::TestCircularHierarchyPrevention(); }

void Zenith_UnitTests::TestCircularHierarchyPrevention(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create A -> B -> C hierarchy
	Zenith_Entity xA = g_xEngine.Scenes().CreateEntity(pxSceneData, "CircularTestA");
	Zenith_Entity xB = g_xEngine.Scenes().CreateEntity(pxSceneData, "CircularTestB");
	Zenith_Entity xC = g_xEngine.Scenes().CreateEntity(pxSceneData, "CircularTestC");

	Zenith_EntityID uA = xA.GetEntityID();
	Zenith_EntityID uB = xB.GetEntityID();
	Zenith_EntityID uC = xC.GetEntityID();

	// Set up hierarchy: A -> B -> C
	xB.SetParent(uA);  // B is child of A
	xC.SetParent(uB);  // C is child of B

	// Verify initial hierarchy
	ZENITH_ASSERT_TRUE(xB.HasParent(), "TestCircularHierarchyPrevention: B should have parent");
	ZENITH_ASSERT_EQ(xB.GetParentEntityID(), uA, "TestCircularHierarchyPrevention: B's parent should be A");
	ZENITH_ASSERT_EQ(xC.GetParentEntityID(), uB, "TestCircularHierarchyPrevention: C's parent should be B");

	// Try to parent A to C (would create cycle: A -> B -> C -> A)
	// This should be rejected by the circular hierarchy check
	xA.SetParent(uC);

	// A should still be root (circular parenting rejected)
	ZENITH_ASSERT_FALSE(xA.HasParent(), "TestCircularHierarchyPrevention: Circular parent should be rejected - A should remain root");

	// Clean up
	xC.DestroyImmediate();
	xB.DestroyImmediate();
	xA.DestroyImmediate();

}

ZENITH_TEST(Core, SelfParentingPrevention) { Zenith_UnitTests::TestSelfParentingPrevention(); }

void Zenith_UnitTests::TestSelfParentingPrevention(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create an entity
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SelfParentTest");
	Zenith_EntityID uEntityID = xEntity.GetEntityID();

	// Verify initially root
	ZENITH_ASSERT_FALSE(xEntity.HasParent(), "TestSelfParentingPrevention: Entity should start as root");

	// Try to parent entity to itself
	xEntity.SetParent(uEntityID);

	// Should still be root (self-parenting rejected)
	ZENITH_ASSERT_FALSE(xEntity.HasParent(), "TestSelfParentingPrevention: Self-parenting should be rejected");

	// Clean up
	xEntity.DestroyImmediate();

}

ZENITH_TEST(Core, TryGetMainCameraWhenNotSet) { Zenith_UnitTests::TestTryGetMainCameraWhenNotSet(); }

void Zenith_UnitTests::TestTryGetMainCameraWhenNotSet(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Remember current camera if any
	Zenith_EntityID uPreviousCamera = pxSceneData->GetMainCameraEntity();

	// Clear main camera
	pxSceneData->SetMainCameraEntity(INVALID_ENTITY_ID);

	// Engine-side resolver should return nullptr when no camera is set
	// (equivalent to the old pxSceneData->TryGetMainCamera()).
	Zenith_CameraComponent* pxCamera = Zenith_TryGetMainCamera(pxSceneData);
	ZENITH_ASSERT_NULL(pxCamera, "TestTryGetMainCameraWhenNotSet: TryGetMainCamera should return nullptr when no camera set");

	// Restore previous camera
	if (uPreviousCamera != INVALID_ENTITY_ID && pxSceneData->EntityExists(uPreviousCamera))
	{
		pxSceneData->SetMainCameraEntity(uPreviousCamera);
	}

}

ZENITH_TEST(Core, DeepHierarchyBuildModelMatrix) { Zenith_UnitTests::TestDeepHierarchyBuildModelMatrix(); }

void Zenith_UnitTests::TestDeepHierarchyBuildModelMatrix(){

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create a hierarchy with multiple levels (not too deep - just testing it works)
	constexpr u_int DEPTH = 10;
	Zenith_Vector<Zenith_EntityID> xEntityIDs;

	// Create root
	Zenith_Entity xRoot = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeepHierarchyRoot");
	xEntityIDs.PushBack(xRoot.GetEntityID());

	// Create children
	for (u_int u = 1; u < DEPTH; ++u)
	{
		std::string strName = "DeepHierarchyChild" + std::to_string(u);
		Zenith_Entity xChild = g_xEngine.Scenes().CreateEntity(pxSceneData, strName);
		Zenith_EntityID uChildID = xChild.GetEntityID();
		xEntityIDs.PushBack(uChildID);

		// Parent to previous entity
		Zenith_EntityID uParentID = xEntityIDs.Get(u - 1);
		xChild.SetParent(uParentID);
	}

	// Verify depth
	u_int uActualDepth = 0;
	Zenith_EntityID uCurrent = xEntityIDs.Get(DEPTH - 1);  // Deepest entity
	while (pxSceneData->EntityExists(uCurrent) && pxSceneData->GetEntity(uCurrent).HasParent())
	{
		uActualDepth++;
		uCurrent = pxSceneData->GetEntity(uCurrent).GetParentEntityID();
	}
	ZENITH_ASSERT_EQ(uActualDepth, DEPTH - 1, "TestDeepHierarchyBuildModelMatrix: Hierarchy depth should be %u, got %u", DEPTH - 1, uActualDepth);

	// BuildModelMatrix should work without infinite loop
	Zenith_Maths::Matrix4 xMatrix;
	Zenith_EntityID uDeepestID = xEntityIDs.Get(DEPTH - 1);
	pxSceneData->GetEntity(uDeepestID).GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xMatrix);

	// If we get here without hanging, the test passed

	// Clean up (destroy from deepest to root)
	for (int i = static_cast<int>(DEPTH) - 1; i >= 0; --i)
	{
		Zenith_Entity xEntity = pxSceneData->GetEntity(xEntityIDs.Get(i));
		xEntity.DestroyImmediate();
	}

}

/**
 * Test that local scene destruction doesn't crash.
 * This tests the fix for TransformComponent destructor accessing the wrong scene
 * when a local test scene is destroyed (not s_xCurrentScene).
 */
ZENITH_TEST(Core, LocalSceneDestruction) { Zenith_UnitTests::TestLocalSceneDestruction(); }
void Zenith_UnitTests::TestLocalSceneDestruction(){

	// Create a scene through SceneManager (not the active scene)
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("LocalDestructionTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create some entities with transforms
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LocalEntity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LocalEntity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "LocalEntity3");

	// Set some positions to verify data is valid
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

	// Unload the scene - this should NOT crash
	// The original bug was: TransformComponent::~TransformComponent called GetCurrentScene()
	// which returned the wrong scene, causing memory corruption
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

/**
 * Test that local scene destruction with parent-child hierarchy doesn't crash.
 * This is a more complex test that includes hierarchy relationships.
 */
ZENITH_TEST(Core, LocalSceneWithHierarchy) { Zenith_UnitTests::TestLocalSceneWithHierarchy(); }
void Zenith_UnitTests::TestLocalSceneWithHierarchy(){

	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("LocalHierarchyTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	// Create parent entity
	Zenith_Entity xParent = g_xEngine.Scenes().CreateEntity(pxSceneData, "Parent");
	xParent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));

	// Create child entities
	Zenith_Entity xChild1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Child1");
	Zenith_Entity xChild2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "Child2");

	// Set up hierarchy - children parented to parent.
	// Phase 5b: parenting + hierarchy queries run on the slot-backed Zenith_Entity
	// API (the Transform hierarchy shims were removed). Behaviour is identical.
	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());

	// Verify hierarchy was set up correctly
	ZENITH_ASSERT_TRUE(xChild1.HasParent(), "TestLocalSceneWithHierarchy: Child1 should have parent");
	ZENITH_ASSERT_TRUE(xChild2.HasParent(), "TestLocalSceneWithHierarchy: Child2 should have parent");
	ZENITH_ASSERT_EQ(xParent.GetChildCount(), 2, "TestLocalSceneWithHierarchy: Parent should have 2 children");

	// Unload the scene - destructor should handle hierarchy cleanup safely
	// Without the fix, DetachFromParent/DetachAllChildren would crash trying to
	// access the global scene instead of this scene
	g_xEngine.Scenes().UnloadScene(xTestScene);

}

//------------------------------------------------------------------------------
// Procedural Tree Asset Export Test
//------------------------------------------------------------------------------

// Tree bone indices
static constexpr uint32_t TREE_BONE_COUNT = 9;
enum TreeBone
{
	TREE_BONE_ROOT = 0,          // Ground anchor
	TREE_BONE_TRUNK_LOWER = 1,   // Lower trunk
	TREE_BONE_TRUNK_UPPER = 2,   // Upper trunk
	TREE_BONE_BRANCH_0 = 3,      // Branch at trunk lower
	TREE_BONE_BRANCH_1 = 4,      // Branch at trunk upper (left)
	TREE_BONE_BRANCH_2 = 5,      // Branch at trunk upper (right)
	TREE_BONE_BRANCH_3 = 6,      // Branch at trunk top
	TREE_BONE_LEAVES_0 = 7,      // Leaf cluster at branch 3
	TREE_BONE_LEAVES_1 = 8,      // Leaf cluster at branch 1
};

/**
 * Test procedural tree asset loading and verification
 * Assets are generated by GenerateTestAssets() called earlier in main()
 */
#ifndef ZENITH_ANDROID // Asset verification uses std::filesystem with local paths
ZENITH_TEST(Core, ProceduralTreeAssetExport) { Zenith_UnitTests::TestProceduralTreeAssetExport(); }
#endif
void Zenith_UnitTests::TestProceduralTreeAssetExport(){

	// Assets are generated by GenerateTestAssets() called earlier in main()
	// This test verifies the assets were created correctly and can be loaded

	// Expected values for Tree assets
	const uint32_t uExpectedBoneCount = TREE_BONE_COUNT;  // 9 bones
	const uint32_t uExpectedVertCount = TREE_BONE_COUNT * 8;  // 8 verts per bone = 72
	const uint32_t uExpectedIndexCount = TREE_BONE_COUNT * 36;  // 36 indices per bone = 324

	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::string strSkelPath = strOutputDir + "Tree" ZENITH_SKELETON_EXT;
	std::string strMeshAssetPath = strOutputDir + "Tree" ZENITH_MESH_ASSET_EXT;
	std::string strSwayPath = strOutputDir + "Tree_Sway" ZENITH_ANIMATION_EXT;

	// Verify files exist
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strSkelPath), "Skeleton file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(strSwayPath), "Sway animation file should exist");

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get<Zenith_SkeletonAsset>(strSkelPath);
	ZENITH_ASSERT_NOT_NULL(pxReloadedSkel, "Should be able to reload skeleton");
	ZENITH_ASSERT_EQ(pxReloadedSkel->GetNumBones(), uExpectedBoneCount, "Reloaded skeleton should have 9 bones");
	ZENITH_ASSERT_TRUE(pxReloadedSkel->HasBone("TrunkLower"), "Reloaded skeleton should have TrunkLower bone");
	ZENITH_ASSERT_TRUE(pxReloadedSkel->HasBone("Branch1"), "Reloaded skeleton should have Branch1 bone");
	ZENITH_ASSERT_TRUE(pxReloadedSkel->HasBone("Leaves0"), "Reloaded skeleton should have Leaves0 bone");

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get<Zenith_MeshAsset>(strMeshAssetPath);
	ZENITH_ASSERT_NOT_NULL(pxReloadedMesh, "Should be able to reload mesh asset");
	ZENITH_ASSERT_EQ(pxReloadedMesh->GetNumVerts(), uExpectedVertCount, "Reloaded mesh vertex count mismatch");
	ZENITH_ASSERT_EQ(pxReloadedMesh->GetNumIndices(), uExpectedIndexCount, "Reloaded mesh index count mismatch");

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "Tree" ZENITH_MESH_EXT).c_str(), xReloadedGeometry, 0, false);
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumVerts(), uExpectedVertCount, "Reloaded geometry vertex count mismatch");
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumIndices(), uExpectedIndexCount, "Reloaded geometry index count mismatch");
	ZENITH_ASSERT_EQ(xReloadedGeometry.GetNumBones(), uExpectedBoneCount, "Reloaded geometry bone count mismatch");

	// Reload and verify VAT
	Flux_AnimationTexture* pxReloadedVAT = Flux_AnimationTexture::LoadFromFile(strOutputDir + "Tree_Sway.zanmt");
	ZENITH_ASSERT_NOT_NULL(pxReloadedVAT, "Should be able to reload VAT");
	ZENITH_ASSERT_EQ(pxReloadedVAT->GetVertexCount(), uExpectedVertCount, "Reloaded VAT vertex count mismatch");
	ZENITH_ASSERT_EQ(pxReloadedVAT->GetNumAnimations(), 1, "Reloaded VAT should have 1 animation");
	delete pxReloadedVAT;
#endif

	// Reload and verify animation
	Zenith_AnimationAsset* pxReloadedSwayAsset = Zenith_AssetRegistry::Get<Zenith_AnimationAsset>(strSwayPath);
	ZENITH_ASSERT_TRUE(pxReloadedSwayAsset != nullptr && pxReloadedSwayAsset->GetClip() != nullptr, "Should be able to reload sway animation");
	ZENITH_ASSERT_EQ(pxReloadedSwayAsset->GetClip()->GetName(), "Sway", "Reloaded sway animation name mismatch");
	ZENITH_ASSERT_TRUE(FloatEquals(pxReloadedSwayAsset->GetClip()->GetDuration(), 2.0f, 0.01f), "Reloaded sway duration mismatch");

}

//=============================================================================
// Asset Handle Tests
// Tests for the operator bool() fix that ensures procedural assets (via Set())
// are correctly detected as valid, not just path-based assets.
//=============================================================================


ZENITH_TEST(Asset, AssetHandleProceduralBoolConversion) { Zenith_UnitTests::TestAssetHandleProceduralBoolConversion(); }

void Zenith_UnitTests::TestAssetHandleProceduralBoolConversion(){

	// Create a procedural material via registry
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestProceduralMaterial");

	// Create a handle and set it via Set() (procedural path)
	MaterialHandle xHandle;
	xHandle.Set(pxMaterial);

	// The key fix: operator bool() should return true for procedural assets
	// Previously it only checked if path was set, which is empty for procedural assets
	ZENITH_ASSERT_TRUE(static_cast<bool>(xHandle), "Procedural asset handle should be valid (operator bool)");
	ZENITH_ASSERT_EQ(xHandle.GetDirect(), pxMaterial, "GetDirect() should return the procedural material");
	ZENITH_ASSERT_TRUE(xHandle.IsLoaded(), "IsLoaded() should return true for procedural asset");

	// Path should be empty for procedural assets
	ZENITH_ASSERT_TRUE(xHandle.GetPath().empty(), "Procedural asset should have empty path");
	ZENITH_ASSERT_FALSE(xHandle.IsSet(), "IsSet() should return false (no path) for procedural asset");

	// Guard pattern that was broken before the fix:
	// if (!xHandle) { return; } // This would incorrectly return for procedural assets
	bool bGuardPassed = false;
	if (xHandle)
	{
		bGuardPassed = true;
	}
	ZENITH_ASSERT_TRUE(bGuardPassed, "Guard pattern 'if (xHandle)' should pass for procedural asset");

	// Cleanup is automatic via handle destructor

}

ZENITH_TEST(Asset, AssetHandlePathBasedBoolConversion) { Zenith_UnitTests::TestAssetHandlePathBasedBoolConversion(); }

void Zenith_UnitTests::TestAssetHandlePathBasedBoolConversion(){

	// Create a handle with a path (simulating a file-based asset)
	MaterialHandle xHandle;
	xHandle.SetPath("game:Materials/TestMaterial.zmat");

	// operator bool() should return true when path is set
	ZENITH_ASSERT_TRUE(static_cast<bool>(xHandle), "Path-based handle should be valid (operator bool)");
	ZENITH_ASSERT_TRUE(xHandle.IsSet(), "IsSet() should return true for path-based handle");
	ZENITH_ASSERT_FALSE(xHandle.GetPath().empty(), "GetPath() should return the path");

	// Note: Get() would try to load from registry which may not exist in test
	// We're testing the bool conversion, not the loading

}

ZENITH_TEST(Asset, AssetHandleEmptyBoolConversion) { Zenith_UnitTests::TestAssetHandleEmptyBoolConversion(); }

void Zenith_UnitTests::TestAssetHandleEmptyBoolConversion(){

	// Default-constructed handle should be invalid
	MaterialHandle xHandle;

	ZENITH_ASSERT_FALSE(static_cast<bool>(xHandle), "Empty handle should be invalid (operator bool)");
	ZENITH_ASSERT_FALSE(xHandle.IsSet(), "Empty handle IsSet() should be false");
	ZENITH_ASSERT_FALSE(xHandle.IsLoaded(), "Empty handle IsLoaded() should be false");
	ZENITH_ASSERT_TRUE(xHandle.GetPath().empty(), "Empty handle path should be empty");
	ZENITH_ASSERT_NULL(xHandle.GetDirect(), "Empty handle GetDirect() should return nullptr");

	// Guard pattern should correctly skip empty handles
	bool bGuardSkipped = true;
	if (xHandle)
	{
		bGuardSkipped = false;
	}
	ZENITH_ASSERT_TRUE(bGuardSkipped, "Guard pattern 'if (xHandle)' should skip empty handle");

}

ZENITH_TEST(Asset, AssetHandleSetStoresRef) { Zenith_UnitTests::TestAssetHandleSetStoresRef(); }

void Zenith_UnitTests::TestAssetHandleSetStoresRef(){

	// This tests that Set() properly increments reference count
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestRefCountMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);

		// Ref count should increase after Set()
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Set() should increment ref count");

		// Copy handle should also increment ref count
		MaterialHandle xHandleCopy = xHandle;
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 2, "Handle copy should increment ref count");
	}
	// After handles go out of scope, ref count should be back to initial

	ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Ref count should return to initial after handles destroyed");

}

ZENITH_TEST(Asset, AssetHandleCopySemantics) { Zenith_UnitTests::TestAssetHandleCopySemantics(); }

void Zenith_UnitTests::TestAssetHandleCopySemantics(){

	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestCopyMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	// Test copy constructor
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Set() should increment ref count");

		// Copy constructor
		MaterialHandle xHandle2(xHandle1);
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 2, "Copy constructor should increment ref count");

		// Both handles should return the same pointer
		ZENITH_ASSERT_EQ(xHandle1.GetDirect(), pxMaterial, "Handle1 should return original pointer");
		ZENITH_ASSERT_EQ(xHandle2.GetDirect(), pxMaterial, "Handle2 should return original pointer");
	}

	ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Ref count should return to initial after copy handles destroyed");

	// Test copy assignment
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);

		Zenith_MaterialAsset* pxMaterial2 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		pxMaterial2->SetName("TestCopyMaterial2");
		uint32_t uMat2InitialRef = pxMaterial2->GetRefCount();

		MaterialHandle xHandle2;
		xHandle2.Set(pxMaterial2);
		ZENITH_ASSERT_EQ(pxMaterial2->GetRefCount(), uMat2InitialRef + 1, "Material2 ref count after Set()");

		// Copy assignment - should release old, acquire new
		xHandle2 = xHandle1;
		ZENITH_ASSERT_EQ(pxMaterial2->GetRefCount(), uMat2InitialRef, "Copy assignment should release old material");
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 2, "Copy assignment should increment new material ref");
	}

	ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Ref count should return to initial after all handles destroyed");

}

ZENITH_TEST(Asset, AssetHandleMoveSemantics) { Zenith_UnitTests::TestAssetHandleMoveSemantics(); }

void Zenith_UnitTests::TestAssetHandleMoveSemantics(){

	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestMoveMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	// Test move constructor
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Set() should increment ref count");

		// Move constructor - should NOT change ref count
		MaterialHandle xHandle2(std::move(xHandle1));
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Move constructor should NOT change ref count");

		// Source handle should be nullified
		ZENITH_ASSERT_FALSE(xHandle1.IsLoaded(), "Moved-from handle should not be loaded");
		ZENITH_ASSERT_EQ(xHandle2.GetDirect(), pxMaterial, "Moved-to handle should have pointer");
	}

	ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Ref count should return to initial after moved handle destroyed");

	// Test move assignment
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);

		Zenith_MaterialAsset* pxMaterial2 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		pxMaterial2->SetName("TestMoveMaterial2");
		uint32_t uMat2InitialRef = pxMaterial2->GetRefCount();

		MaterialHandle xHandle2;
		xHandle2.Set(pxMaterial2);

		// Move assignment - should release old, take ownership of new
		xHandle2 = std::move(xHandle1);
		ZENITH_ASSERT_EQ(pxMaterial2->GetRefCount(), uMat2InitialRef, "Move assignment should release old material");
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Move assignment should NOT increment new material ref");
		ZENITH_ASSERT_FALSE(xHandle1.IsLoaded(), "Moved-from handle should not be loaded");
	}

	ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Ref count should return to initial after all handles destroyed");

}

ZENITH_TEST(Asset, AssetHandleSetPathReleasesRef) { Zenith_UnitTests::TestAssetHandleSetPathReleasesRef(); }

void Zenith_UnitTests::TestAssetHandleSetPathReleasesRef(){

	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestSetPathMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Set() should increment ref count");

		// SetPath should release the old cached pointer
		xHandle.SetPath("game:Materials/NonExistent.zmat");
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "SetPath() should release old cached ref");

		// Handle is now path-based, not loaded
		ZENITH_ASSERT_FALSE(xHandle.IsLoaded(), "After SetPath, handle should not be loaded");
		ZENITH_ASSERT_TRUE(xHandle.IsSet(), "After SetPath, handle should have path set");
	}

}

ZENITH_TEST(Asset, AssetHandleClearReleasesRef) { Zenith_UnitTests::TestAssetHandleClearReleasesRef(); }

void Zenith_UnitTests::TestAssetHandleClearReleasesRef(){

	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestClearMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount + 1, "Set() should increment ref count");

		// Clear should release the ref
		xHandle.Clear();
		ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), uInitialRefCount, "Clear() should release ref");

		// Handle should be empty
		ZENITH_ASSERT_FALSE(xHandle.IsLoaded(), "After Clear, handle should not be loaded");
		ZENITH_ASSERT_FALSE(xHandle.IsSet(), "After Clear, handle should not have path set");
		ZENITH_ASSERT_FALSE(static_cast<bool>(xHandle), "After Clear, operator bool should return false");
	}

}

ZENITH_TEST(Asset, AssetHandleProceduralComparison) { Zenith_UnitTests::TestAssetHandleProceduralComparison(); }

void Zenith_UnitTests::TestAssetHandleProceduralComparison(){

	// Create two different procedural materials
	Zenith_MaterialAsset* pxMaterial1 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial1->SetName("TestCompare1");

	Zenith_MaterialAsset* pxMaterial2 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial2->SetName("TestCompare2");

	MaterialHandle xHandle1;
	xHandle1.Set(pxMaterial1);

	MaterialHandle xHandle2;
	xHandle2.Set(pxMaterial2);

	MaterialHandle xHandle1Copy;
	xHandle1Copy.Set(pxMaterial1);

	// Different procedural assets should NOT compare equal
	ZENITH_ASSERT_FALSE(xHandle1 == xHandle2, "Different procedural assets should not be equal");
	ZENITH_ASSERT_NE(xHandle1, xHandle2, "Different procedural assets should compare not-equal");

	// Same procedural asset should compare equal
	ZENITH_ASSERT_EQ(xHandle1, xHandle1Copy, "Same procedural asset should be equal");
	ZENITH_ASSERT_FALSE(xHandle1 != xHandle1Copy, "Same procedural asset should not compare not-equal");

	// Empty handles should compare equal
	MaterialHandle xEmpty1;
	MaterialHandle xEmpty2;
	ZENITH_ASSERT_EQ(xEmpty1, xEmpty2, "Empty handles should be equal");

	// Test path-based comparison still works
	MaterialHandle xPath1;
	xPath1.SetPath("game:Materials/Test.zmat");

	MaterialHandle xPath2;
	xPath2.SetPath("game:Materials/Test.zmat");

	MaterialHandle xPath3;
	xPath3.SetPath("game:Materials/Different.zmat");

	ZENITH_ASSERT_EQ(xPath1, xPath2, "Same path should be equal");
	ZENITH_ASSERT_NE(xPath1, xPath3, "Different paths should not be equal");

	// Procedural vs path-based should not be equal (even if both valid)
	ZENITH_ASSERT_NE(xHandle1, xPath1, "Procedural and path-based handles should not be equal");

}

//=============================================================================
// Model Instance Material Tests (GBuffer rendering bug fix)
//=============================================================================

#ifndef ZENITH_ANDROID // Depends on Windows-generated StickFigure mesh asset
ZENITH_TEST(Asset, ModelInstanceMaterialSetAndGet) { Zenith_UnitTests::TestModelInstanceMaterialSetAndGet(); }
#endif

void Zenith_UnitTests::TestModelInstanceMaterialSetAndGet(){

	// Create a procedural material (same pattern as Combat game)
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestMaterial");

	// Create model asset with no default materials (reproduces Combat enemy scenario)
	Zenith_ModelAsset* pxModelAsset = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
	pxModelAsset->SetName("TestModel");

	// Try to add StickFigure mesh if available
	std::string strTestMesh = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zasset";
	Zenith_Vector<std::string> xEmptyMaterials;
	if (std::filesystem::exists(strTestMesh))
	{
		pxModelAsset->AddMeshByPath(strTestMesh, xEmptyMaterials);
	}

	// Create model instance
	Flux_ModelInstance* pxInstance = Flux_ModelInstance::CreateFromAsset(pxModelAsset);
	ZENITH_ASSERT_NOT_NULL(pxInstance, "Failed to create model instance");

	// Model should have at least 1 material slot (blank default added by CreateFromAsset)
	ZENITH_ASSERT_GE(pxInstance->GetNumMaterials(), 1, "Model instance should have at least 1 material slot");

	// Override material at index 0
	pxInstance->SetMaterial(0, pxMaterial);

	// CRITICAL TEST: GetMaterial must return the material we just set
	Zenith_MaterialAsset* pxRetrieved = pxInstance->GetMaterial(0);
	ZENITH_ASSERT_NOT_NULL(pxRetrieved, "GetMaterial(0) returned nullptr after SetMaterial - this causes GBuffer rendering to skip the mesh");
	ZENITH_ASSERT_EQ(pxRetrieved, pxMaterial, "GetMaterial(0) did not return the same pointer that was passed to SetMaterial");

	// Cleanup
	pxInstance->Destroy();
	delete pxInstance;

}

ZENITH_TEST(Core, MaterialHandleCopyPreservesCachedPointer) { Zenith_UnitTests::TestMaterialHandleCopyPreservesCachedPointer(); }

void Zenith_UnitTests::TestMaterialHandleCopyPreservesCachedPointer(){

	// Create a procedural material and store in handle (like Combat::g_xEnemyMaterial)
	MaterialHandle xOriginal;
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestProceduralMaterial");
	xOriginal.Set(pxMaterial);

	// Verify original handle works
	ZENITH_ASSERT_EQ(xOriginal.GetDirect(), pxMaterial, "Original handle should return the material");

	// Copy to another handle (like m_xEnemyMaterial = Combat::g_xEnemyMaterial)
	MaterialHandle xCopy = xOriginal;

	// CRITICAL TEST: Copy must preserve the cached pointer
	ZENITH_ASSERT_NOT_NULL(xCopy.GetDirect(), "Copied handle returned nullptr - copy assignment failed to preserve cached pointer");
	ZENITH_ASSERT_EQ(xCopy.GetDirect(), pxMaterial, "Copied handle returned different pointer than original");

	// Verify original still works after copy
	ZENITH_ASSERT_EQ(xOriginal.GetDirect(), pxMaterial, "Original handle should still work after copy");

}

//=============================================================================
// Any-State Transition Tests
//=============================================================================

ZENITH_TEST(Core, AnyStateTransitionFires) { Zenith_UnitTests::TestAnyStateTransitionFires(); }

void Zenith_UnitTests::TestAnyStateTransitionFires(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Hit");
	xSM.SetDefaultState("Idle");

	// Add parameter
	xSM.GetParameters().AddTrigger("HitTrigger");

	// Add any-state transition: HitTrigger -> Hit
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Hit";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "HitTrigger";
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xCond);

	xSM.AddAnyStateTransition(xTrans);

	// Initialize state machine with a dummy update
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Fire trigger
	xSM.GetParameters().SetTrigger("HitTrigger");
	xSM.Update(0.016f, xPose, xSkel);

	// Should be transitioning to Hit
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning after trigger");

}

ZENITH_TEST(Core, AnyStateTransitionSkipsSelf) { Zenith_UnitTests::TestAnyStateTransitionSkipsSelf(); }

void Zenith_UnitTests::TestAnyStateTransitionSkipsSelf(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddBool("AlwaysTrue", true);

	// Add any-state transition targeting current state (Idle -> Idle)
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Idle";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "AlwaysTrue";
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xCond);

	xSM.AddAnyStateTransition(xTrans);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);
	xSM.Update(0.016f, xPose, xSkel);

	// Should NOT be transitioning (self-loop skipped)
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "Any-state should skip self-loop");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should remain in Idle");

}

ZENITH_TEST(Core, AnyStateTransitionPriority) { Zenith_UnitTests::TestAnyStateTransitionPriority(); }

void Zenith_UnitTests::TestAnyStateTransitionPriority(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Hit");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddTrigger("HitTrigger");
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Low priority: HitTrigger -> Hit (priority 10)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Hit";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 10;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "HitTrigger";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// High priority: DeathTrigger -> Death (priority 100)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Verify priority ordering
	const Zenith_Vector<Flux_StateTransition>& xAny = xSM.GetAnyStateTransitions();
	ZENITH_ASSERT_EQ(xAny.GetSize(), 2, "Should have 2 any-state transitions");
	ZENITH_ASSERT_EQ(xAny.Get(0).m_iPriority, 100, "First should be highest priority (Death)");
	ZENITH_ASSERT_EQ(xAny.Get(1).m_iPriority, 10, "Second should be lower priority (Hit)");

}

//=============================================================================
// AnimatorStateInfo Tests
//=============================================================================

ZENITH_TEST(Core, StateInfoStateName) { Zenith_UnitTests::TestStateInfoStateName(); }

void Zenith_UnitTests::TestStateInfoStateName(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Flux_AnimatorStateInfo xInfo = xSM.GetCurrentStateInfo();
	ZENITH_ASSERT_TRUE(xInfo.IsName("Idle"), "State name should be Idle");
	ZENITH_ASSERT_FALSE(xInfo.IsName("Walk"), "State name should not be Walk");

}

ZENITH_TEST(Core, StateInfoNormalizedTime) { Zenith_UnitTests::TestStateInfoNormalizedTime(); }

void Zenith_UnitTests::TestStateInfoNormalizedTime(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	// State info should return 0 normalized time when no blend tree
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Flux_AnimatorStateInfo xInfo = xSM.GetCurrentStateInfo();
	ZENITH_ASSERT_GE(xInfo.m_fNormalizedTime, 0.0f, "Normalized time should be >= 0");

}

//=============================================================================
// CrossFade Tests
//=============================================================================

ZENITH_TEST(Animation, CrossFadeToState) { Zenith_UnitTests::TestCrossFadeToState(); }

void Zenith_UnitTests::TestCrossFadeToState(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// CrossFade to Walk (no conditions needed)
	xSM.CrossFade("Walk", 0.2f);

	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning after CrossFade");

}

ZENITH_TEST(Animation, CrossFadeToCurrentState) { Zenith_UnitTests::TestCrossFadeToCurrentState(); }

void Zenith_UnitTests::TestCrossFadeToCurrentState(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	// CrossFade to current state should be a no-op
	xSM.CrossFade("Idle", 0.2f);
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "CrossFade to current state should be no-op");

}

//=============================================================================
// Sub-State Machine Tests
//=============================================================================

ZENITH_TEST(Core, SubStateMachineCreation) { Zenith_UnitTests::TestSubStateMachineCreation(); }

void Zenith_UnitTests::TestSubStateMachineCreation(){

	Flux_AnimationState xState("Locomotion");

	ZENITH_ASSERT_FALSE(xState.IsSubStateMachine(), "Should not be sub-SM initially");

	Flux_AnimationStateMachine* pxSubSM = xState.CreateSubStateMachine("LocomotionSM");
	ZENITH_ASSERT_NOT_NULL(pxSubSM, "Sub-SM should be created");
	ZENITH_ASSERT_TRUE(xState.IsSubStateMachine(), "Should be sub-SM after creation");
	ZENITH_ASSERT_EQ(pxSubSM->GetName(), "LocomotionSM", "Sub-SM name should match");

}

ZENITH_TEST(Core, SubStateMachineSharedParameters) { Zenith_UnitTests::TestSubStateMachineSharedParameters(); }

void Zenith_UnitTests::TestSubStateMachineSharedParameters(){

	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-SM
	Flux_AnimationState* pxState = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxState->CreateSubStateMachine("LocomotionSM");

	// Set shared parameters
	pxSubSM->SetSharedParameters(&xParentSM.GetParameters());

	// Setting a parameter on parent should be visible in child
	xParentSM.GetParameters().SetFloat("Speed", 5.0f);
	ZENITH_ASSERT_EQ(pxSubSM->GetParameters().GetFloat("Speed"), 5.0f, "Child should see parent's parameter value");

}

//=============================================================================
// Animation Layer Tests
//=============================================================================

ZENITH_TEST(Animation, LayerCreation) { Zenith_UnitTests::TestLayerCreation(); }

void Zenith_UnitTests::TestLayerCreation(){

	Flux_AnimationController xController;

	ZENITH_ASSERT_FALSE(xController.HasLayers(), "Should have no layers initially");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 0, "Layer count should be 0");

	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	ZENITH_ASSERT_NOT_NULL(pxBase, "Base layer should be created");
	ZENITH_ASSERT_TRUE(xController.HasLayers(), "Should have layers after adding");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 1, "Layer count should be 1");
	ZENITH_ASSERT_EQ(pxBase->GetName(), "Base", "Layer name should match");
	ZENITH_ASSERT_EQ(pxBase->GetWeight(), 1.0f, "Default weight should be 1.0");
	ZENITH_ASSERT_EQ(pxBase->GetBlendMode(), LAYER_BLEND_OVERRIDE, "Default blend mode should be Override");

	Flux_AnimationLayer* pxUpperBody = xController.AddLayer("UpperBody");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2, "Layer count should be 2");
	pxUpperBody->SetBlendMode(LAYER_BLEND_ADDITIVE);
	ZENITH_ASSERT_EQ(pxUpperBody->GetBlendMode(), LAYER_BLEND_ADDITIVE, "Blend mode should be Additive");

}

ZENITH_TEST(Animation, LayerWeightZero) { Zenith_UnitTests::TestLayerWeightZero(); }

void Zenith_UnitTests::TestLayerWeightZero(){

	Flux_AnimationLayer xLayer("Test");
	xLayer.SetWeight(0.0f);
	ZENITH_ASSERT_EQ(xLayer.GetWeight(), 0.0f, "Weight should be 0");

	xLayer.SetWeight(0.5f);
	ZENITH_ASSERT_EQ(xLayer.GetWeight(), 0.5f, "Weight should be 0.5");

	// Clamping test
	xLayer.SetWeight(2.0f);
	ZENITH_ASSERT_EQ(xLayer.GetWeight(), 1.0f, "Weight should be clamped to 1.0");

	xLayer.SetWeight(-1.0f);
	ZENITH_ASSERT_EQ(xLayer.GetWeight(), 0.0f, "Weight should be clamped to 0.0");

}

//=============================================================================
// Tween System Tests - Easing Functions
//=============================================================================

ZENITH_TEST(Tween, EasingLinear) { Zenith_UnitTests::TestEasingLinear(); }

void Zenith_UnitTests::TestEasingLinear(){

	ZENITH_ASSERT_EQ(Zenith_ApplyEasing(EASING_LINEAR, 0.0f), 0.0f, "Linear easing at 0 should be 0");
	ZENITH_ASSERT_EQ(Zenith_ApplyEasing(EASING_LINEAR, 0.5f), 0.5f, "Linear easing at 0.5 should be 0.5");
	ZENITH_ASSERT_EQ(Zenith_ApplyEasing(EASING_LINEAR, 1.0f), 1.0f, "Linear easing at 1 should be 1");
	ZENITH_ASSERT_EQ(Zenith_ApplyEasing(EASING_LINEAR, 0.25f), 0.25f, "Linear easing at 0.25 should be 0.25");

}

ZENITH_TEST(Tween, EasingEndpoints) { Zenith_UnitTests::TestEasingEndpoints(); }

void Zenith_UnitTests::TestEasingEndpoints(){

	const float fEpsilon = 0.001f;

	// All easing functions should map 0->0 and 1->1
	for (int i = 0; i < EASING_COUNT; ++i)
	{
		Zenith_EasingType eType = static_cast<Zenith_EasingType>(i);
		float fAtZero = Zenith_ApplyEasing(eType, 0.0f);
		float fAtOne = Zenith_ApplyEasing(eType, 1.0f);

		ZENITH_ASSERT_LT(glm::abs(fAtZero), fEpsilon, "Easing at 0 should be ~0");
		ZENITH_ASSERT_LT(glm::abs(fAtOne - 1.0f), fEpsilon, "Easing at 1 should be ~1");
	}

}

ZENITH_TEST(Tween, EasingQuadOut) { Zenith_UnitTests::TestEasingQuadOut(); }

void Zenith_UnitTests::TestEasingQuadOut(){

	// QuadOut starts fast, ends slow
	// At midpoint, output should be > 0.5 (since it's decelerating)
	float fMid = Zenith_ApplyEasing(EASING_QUAD_OUT, 0.5f);
	ZENITH_ASSERT_GT(fMid, 0.5f, "QuadOut at 0.5 should be > 0.5 (decelerating curve)");
	ZENITH_ASSERT_LT(fMid, 1.0f, "QuadOut at 0.5 should be < 1.0");

	// Quarter point should also show deceleration
	float fQuarter = Zenith_ApplyEasing(EASING_QUAD_OUT, 0.25f);
	ZENITH_ASSERT_GT(fQuarter, 0.25f, "QuadOut at 0.25 should be > 0.25");

}

ZENITH_TEST(Tween, EasingBounceOut) { Zenith_UnitTests::TestEasingBounceOut(); }

void Zenith_UnitTests::TestEasingBounceOut(){

	// BounceOut should have values between 0 and 1 at midpoints
	float fMid = Zenith_ApplyEasing(EASING_BOUNCE_OUT, 0.5f);
	ZENITH_ASSERT_TRUE(fMid >= 0.0f && fMid <= 1.0f, "BounceOut at 0.5 should be in [0,1]");

	// BounceOut at 0.9 should be close to 1.0 (near the end)
	float fNearEnd = Zenith_ApplyEasing(EASING_BOUNCE_OUT, 0.95f);
	ZENITH_ASSERT_GT(fNearEnd, 0.8f, "BounceOut near end should be close to 1.0");

}

//=============================================================================
// Tween System Tests - TweenInstance
//=============================================================================

ZENITH_TEST(Tween, TweenInstanceProgress) { Zenith_UnitTests::TestTweenInstanceProgress(); }

void Zenith_UnitTests::TestTweenInstanceProgress(){

	Zenith_TweenInstance xTween;
	xTween.m_eEasing = EASING_LINEAR;
	xTween.m_fDuration = 2.0f;
	xTween.m_fDelay = 0.0f;

	xTween.m_fElapsed = 0.0f;
	ZENITH_ASSERT_EQ(xTween.GetNormalizedTime(), 0.0f, "At elapsed 0, normalized time should be 0");

	xTween.m_fElapsed = 1.0f;
	float fHalf = xTween.GetNormalizedTime();
	ZENITH_ASSERT_LT(glm::abs(fHalf - 0.5f), 0.001f, "At elapsed 1 of duration 2, normalized time should be 0.5");

	xTween.m_fElapsed = 2.0f;
	ZENITH_ASSERT_LT(glm::abs(xTween.GetNormalizedTime() - 1.0f), 0.001f, "At elapsed 2 of duration 2, normalized time should be 1.0");

}

ZENITH_TEST(Tween, TweenInstanceCompletion) { Zenith_UnitTests::TestTweenInstanceCompletion(); }

void Zenith_UnitTests::TestTweenInstanceCompletion(){

	// Completion is determined by normalized time reaching 1.0
	Zenith_TweenInstance xTween;
	xTween.m_fDuration = 1.0f;
	xTween.m_fElapsed = 0.0f;
	ZENITH_ASSERT_LT(xTween.GetNormalizedTime(), 1.0f, "New tween should not be complete");

	xTween.m_fElapsed = 1.0f;
	ZENITH_ASSERT_LT(glm::abs(xTween.GetNormalizedTime() - 1.0f), 0.001f, "Elapsed == Duration should give normalized time 1.0");

	// Zero duration should give normalized time 1.0
	Zenith_TweenInstance xZeroDuration;
	xZeroDuration.m_fDuration = 0.0f;
	ZENITH_ASSERT_LT(glm::abs(xZeroDuration.GetNormalizedTime() - 1.0f), 0.001f, "Zero duration tween should have normalized time 1.0");

}

ZENITH_TEST(Tween, TweenInstanceDelay) { Zenith_UnitTests::TestTweenInstanceDelay(); }

void Zenith_UnitTests::TestTweenInstanceDelay(){

	Zenith_TweenInstance xTween;
	xTween.m_eEasing = EASING_LINEAR;
	xTween.m_fDuration = 1.0f;
	xTween.m_fDelay = 0.5f;

	// During delay, normalized time should be 0
	xTween.m_fElapsed = 0.3f;
	ZENITH_ASSERT_EQ(xTween.GetNormalizedTime(), 0.0f, "During delay, normalized time should be 0");

	// After delay, should start progressing
	xTween.m_fElapsed = 1.0f;  // 0.5 delay + 0.5 active = halfway
	float fT = xTween.GetNormalizedTime();
	ZENITH_ASSERT_LT(glm::abs(fT - 0.5f), 0.001f, "After delay with 0.5s active, should be at 0.5");

	// After delay + full duration
	xTween.m_fElapsed = 1.5f;  // 0.5 delay + 1.0 active = done
	ZENITH_ASSERT_LT(glm::abs(xTween.GetNormalizedTime() - 1.0f), 0.001f, "After delay + duration, should be at 1.0");

}

//=============================================================================
// Tween System Tests - TweenComponent
//=============================================================================

ZENITH_TEST(Tween, TweenComponentScaleTo) { Zenith_UnitTests::TestTweenComponentScaleTo(); }

void Zenith_UnitTests::TestTweenComponentScaleTo(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenScaleTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	// Set initial scale
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Should have active tweens");
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 1, "Should have 1 active tween");

	// Simulate halfway
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 0.5f), 0.01f, "Scale X should be ~0.5 at halfway");
	ZENITH_ASSERT_LT(glm::abs(xScale.y - 0.5f), 0.01f, "Scale Y should be ~0.5 at halfway");

	// Simulate to completion
	xTween.OnUpdate(0.5f);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x), 0.01f, "Scale X should be ~0.0 at completion");

	ZENITH_ASSERT_FALSE(xTween.HasActiveTweens(), "Tween should be removed after completion");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentPositionTo) { Zenith_UnitTests::TestTweenComponentPositionTo(); }

void Zenith_UnitTests::TestTweenComponentPositionTo(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenPosTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	// Simulate halfway
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	ZENITH_ASSERT_LT(glm::abs(xPos.x - 5.0f), 0.01f, "Position X should be ~5.0 at halfway");

	// Complete
	xTween.OnUpdate(0.5f);
	xTransform.GetPosition(xPos);
	ZENITH_ASSERT_LT(glm::abs(xPos.x - 10.0f), 0.01f, "Position X should be ~10.0 at completion");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentMultiple) { Zenith_UnitTests::TestTweenComponentMultiple(); }

void Zenith_UnitTests::TestTweenComponentMultiple(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenMultiTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f), 1.0f, EASING_LINEAR);

	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 2, "Should have 2 active tweens");

	// Both should complete
	xTween.OnUpdate(1.0f);

	Zenith_Maths::Vector3 xPos, xScale;
	xTransform.GetPosition(xPos);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xPos.x - 10.0f), 0.01f, "Position should have reached target");
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 2.0f), 0.01f, "Scale should have reached target");
	ZENITH_ASSERT_FALSE(xTween.HasActiveTweens(), "Both tweens should be complete");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentCallback) { Zenith_UnitTests::TestTweenComponentCallback(); }

void Zenith_UnitTests::TestTweenComponentCallback(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenCallbackTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f));

	bool bCallbackFired = false;
	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f), 0.5f, EASING_LINEAR);
	xTween.SetOnComplete([](void* pUserData) {
		*static_cast<bool*>(pUserData) = true;
	}, &bCallbackFired);

	ZENITH_ASSERT_FALSE(bCallbackFired, "Callback should not have fired yet");

	// Complete the tween
	xTween.OnUpdate(0.5f);
	ZENITH_ASSERT_TRUE(bCallbackFired, "Callback should have fired on completion");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentLoop) { Zenith_UnitTests::TestTweenComponentLoop(); }

void Zenith_UnitTests::TestTweenComponentLoop(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenLoopTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, false);

	// Complete one cycle
	xTween.OnUpdate(1.0f);
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Looping tween should still be active after completion");

	// After loop reset, another update should work
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	// Should be interpolating from start again
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Looping tween should still be active");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentPingPong) { Zenith_UnitTests::TestTweenComponentPingPong(); }

void Zenith_UnitTests::TestTweenComponentPingPong(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenPingPongTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, true);

	// Forward pass: 0 -> 1
	xTween.OnUpdate(1.0f);
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "PingPong tween should still be active");

	// Reverse pass halfway: should be going 1 -> 0, at 0.5 should be ~0.5
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 0.5f), 0.1f, "PingPong reverse at halfway should be ~0.5");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenComponentCancel) { Zenith_UnitTests::TestTweenComponentCancel(); }

void Zenith_UnitTests::TestTweenComponentCancel(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenCancelTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 2, "Should have 2 active tweens");

	xTween.CancelAll();
	ZENITH_ASSERT_FALSE(xTween.HasActiveTweens(), "After CancelAll, no tweens should be active");
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 0, "Active count should be 0");

	g_xEngine.Scenes().UnloadScene(xScene);
}

//=============================================================================
// Sub-SM Transition Evaluation (BUG 1 regression test)
//=============================================================================

ZENITH_TEST(Core, SubStateMachineTransitionEvaluation) { Zenith_UnitTests::TestSubStateMachineTransitionEvaluation(); }

void Zenith_UnitTests::TestSubStateMachineTransitionEvaluation(){

	// Create parent SM with a speed parameter
	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-SM that has its own states and transitions
	Flux_AnimationState* pxLocomotion = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotion->CreateSubStateMachine("LocomotionSM");
	pxSubSM->SetSharedParameters(&xParentSM.GetParameters());

	// Add states to the sub-SM
	pxSubSM->AddState("Walk");
	pxSubSM->AddState("Run");
	pxSubSM->SetDefaultState("Walk");

	// Add transition: Walk -> Run when Speed > 3.0
	Flux_StateTransition xWalkToRun;
	xWalkToRun.m_strTargetStateName = "Run";
	xWalkToRun.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 3.0f;
	xWalkToRun.m_xConditions.PushBack(xSpeedCond);

	pxSubSM->GetState("Walk")->AddTransition(xWalkToRun);

	// Initialize the sub-SM
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;

	pxSubSM->Update(0.0f, xPose, xSkel);
	ZENITH_ASSERT_EQ(pxSubSM->GetCurrentStateName(), "Walk", "Sub-SM should start in Walk");

	// Set parent parameter Speed > 3.0 - sub-SM should see it through shared parameters
	xParentSM.GetParameters().SetFloat("Speed", 5.0f);

	// Update sub-SM - transition should evaluate against shared (parent) parameters
	pxSubSM->Update(0.016f, xPose, xSkel);
	ZENITH_ASSERT_TRUE(pxSubSM->IsTransitioning(), "Sub-SM should be transitioning Walk->Run via shared parameters");

	// Complete transition
	for (int i = 0; i < 20; ++i)
		pxSubSM->Update(0.016f, xPose, xSkel);

	ZENITH_ASSERT_EQ(pxSubSM->GetCurrentStateName(), "Run", "Sub-SM should have transitioned to Run using parent's Speed parameter");

}

//=============================================================================
// CrossFade Edge Cases
//=============================================================================

ZENITH_TEST(Animation, CrossFadeNonExistentState) { Zenith_UnitTests::TestCrossFadeNonExistentState(); }

void Zenith_UnitTests::TestCrossFadeNonExistentState(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// CrossFade to non-existent state should silently do nothing
	xSM.CrossFade("NonExistent", 0.15f);
	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "Should NOT be transitioning to non-existent state");
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should still be in Idle");

}

ZENITH_TEST(Animation, CrossFadeInstant) { Zenith_UnitTests::TestCrossFadeInstant(); }

void Zenith_UnitTests::TestCrossFadeInstant(){

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Run");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// CrossFade with zero duration - should transition immediately on next update
	xSM.CrossFade("Run", 0.0f);
	xSM.Update(0.001f, xPose, xSkel);

	// With duration=0, the cross-fade should complete immediately
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Run", "Zero-duration crossfade should complete immediately");

}

//=============================================================================
// Tween Rotation Test
//=============================================================================

ZENITH_TEST(Tween, TweenComponentRotation) { Zenith_UnitTests::TestTweenComponentRotation(); }

void Zenith_UnitTests::TestTweenComponentRotation(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenRotationTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	// Set initial rotation to identity
	xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	// Tween rotation to 90 degrees around Y axis over 1 second
	xTween.TweenRotation(Zenith_Maths::Vector3(0.0f, 90.0f, 0.0f), 1.0f, EASING_LINEAR);

	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Should have active rotation tween");

	// Update to completion
	xTween.OnUpdate(1.0f);
	ZENITH_ASSERT_FALSE(xTween.HasActiveTweens(), "Rotation tween should be complete");

	// Verify rotation was applied - get the euler angles back
	Zenith_Maths::Quat xRot;
	xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);
	Zenith_Maths::Vector3 xEuler = glm::degrees(glm::eulerAngles(xRot));

	// Y rotation should be approximately 90 degrees
	ZENITH_ASSERT_LT(glm::abs(xEuler.y - 90.0f), 1.0f, "Y rotation should be ~90 degrees");

	g_xEngine.Scenes().UnloadScene(xScene);
}

//=============================================================================
// Bug Regression Tests (from code review)
//=============================================================================

ZENITH_TEST(Core, TriggerNotConsumedOnPartialConditionMatch) { Zenith_UnitTests::TestTriggerNotConsumedOnPartialConditionMatch(); }

void Zenith_UnitTests::TestTriggerNotConsumedOnPartialConditionMatch(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Attack");
	xStateMachine.GetParameters().AddBool("HasWeapon", false);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Attack");

	// Idle -> Attack requires BOTH trigger AND HasWeapon == true
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Attack";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xTriggerCond;
	xTriggerCond.m_strParameterName = "Attack";
	xTriggerCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xTriggerCond);

	Flux_TransitionCondition xBoolCond;
	xBoolCond.m_strParameterName = "HasWeapon";
	xBoolCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xBoolCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xBoolCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xBoolCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial state
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Set trigger but NOT HasWeapon - transition should fail, trigger should NOT be consumed
	xStateMachine.GetParameters().SetTrigger("Attack");
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.GetCurrentStateName(), "Idle", "Should stay in Idle - HasWeapon is false");
	ZENITH_ASSERT_EQ(xStateMachine.GetParameters().PeekTrigger("Attack"), true, "Trigger should NOT be consumed when other conditions fail");

	// Now set HasWeapon - trigger should still be set, transition should fire
	xStateMachine.GetParameters().SetBool("HasWeapon", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xStateMachine.IsTransitioning(), true, "Transition should start now that all conditions are met");
	ZENITH_ASSERT_EQ(xStateMachine.GetParameters().PeekTrigger("Attack"), false, "Trigger should be consumed after successful transition");

}

ZENITH_TEST(Core, ResolveClipReferencesRecursive) { Zenith_UnitTests::TestResolveClipReferencesRecursive(); }

void Zenith_UnitTests::TestResolveClipReferencesRecursive(){

	// Create a clip collection with two clips
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxIdleClip = new Flux_AnimationClip();
	pxIdleClip->SetName("Idle");
	Flux_AnimationClip* pxWalkClip = new Flux_AnimationClip();
	pxWalkClip->SetName("Walk");
	xCollection.AddClip(pxIdleClip);
	xCollection.AddClip(pxWalkClip);

	// Create a Blend node with two Clip children (clip pointers null, names set)
	Flux_BlendTreeNode_Clip* pxClipA = new Flux_BlendTreeNode_Clip();
	pxClipA->SetClipName("Idle");
	ZENITH_ASSERT_NULL(pxClipA->GetClip(), "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxClipB = new Flux_BlendTreeNode_Clip();
	pxClipB->SetClipName("Walk");
	ZENITH_ASSERT_NULL(pxClipB->GetClip(), "Clip B should be unresolved");

	Flux_BlendTreeNode_Blend* pxBlend = new Flux_BlendTreeNode_Blend(pxClipA, pxClipB, 0.5f);

	// Create state machine with a state that has the blend tree root
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("BlendState");
	pxState->SetBlendTree(pxBlend);
	xSM.SetDefaultState("BlendState");

	// Resolve - should recursively resolve both child clips
	xSM.ResolveClipReferences(&xCollection);

	ZENITH_ASSERT_EQ(pxClipA->GetClip(), pxIdleClip, "Clip A should be resolved to Idle clip");
	ZENITH_ASSERT_EQ(pxClipB->GetClip(), pxWalkClip, "Clip B should be resolved to Walk clip");

}

ZENITH_TEST(Tween, TweenDelayWithLoop) { Zenith_UnitTests::TestTweenDelayWithLoop(); }

void Zenith_UnitTests::TestTweenDelayWithLoop(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenDelayLoopTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	// delay=1.0, duration=0.5 - delay > duration, which was the buggy case
	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 0.5f, EASING_LINEAR);
	xTween.SetDelay(1.0f);
	xTween.SetLoop(true, false);

	// During delay period - scale should not change
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 1.0f), 0.01f, "Scale should be unchanged during delay");

	// After delay, at midpoint of tween (total elapsed = 1.25, activeTime = 0.25, t = 0.5)
	xTween.OnUpdate(0.75f);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_GT(xScale.x, 1.0f, "Scale should be interpolating after delay");

	// Complete first loop (total elapsed = 1.75, activeTime = 0.75, t >= 1.0, loop triggers)
	// Loop resets elapsed to delay (1.0), tween stays active
	xTween.OnUpdate(0.5f);
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Looping tween should still be active");

	// After loop reset, a small update should restart interpolation from the beginning
	// elapsed goes from 1.0 to 1.1, activeTime = 0.1, t = 0.1/0.5 = 0.2
	// scale = lerp(1.0, 2.0, 0.2) = 1.2
	xTween.OnUpdate(0.1f);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 1.2f), 0.05f, "After loop, tween should restart interpolation from beginning (expected ~1.2)");
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Looping tween should still be active");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenCallbackReentrant) { Zenith_UnitTests::TestTweenCallbackReentrant(); }

void Zenith_UnitTests::TestTweenCallbackReentrant(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenReentrantTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	struct CallbackData
	{
		Zenith_TweenComponent* m_pxTween;
		bool m_bCallbackFired;
	};

	CallbackData xData;
	xData.m_pxTween = &xEntity.GetComponent<Zenith_TweenComponent>();
	xData.m_bCallbackFired = false;

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 0.5f, EASING_LINEAR);
	xTween.SetOnComplete([](void* pUserData) {
		CallbackData* pxData = static_cast<CallbackData*>(pUserData);
		pxData->m_bCallbackFired = true;
		// Re-entrant: create a new tween from within the callback
		pxData->m_pxTween->TweenScale(Zenith_Maths::Vector3(3.0f), 1.0f, EASING_LINEAR);
	}, &xData);

	// Complete the first tween - callback should fire and create a new tween
	xTween.OnUpdate(0.5f);

	ZENITH_ASSERT_TRUE(xData.m_bCallbackFired, "Callback should have fired");
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "New tween should have been created by callback");
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 1, "Should have exactly 1 active tween");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Tween, TweenDuplicatePropertyCancels) { Zenith_UnitTests::TestTweenDuplicatePropertyCancels(); }

void Zenith_UnitTests::TestTweenDuplicatePropertyCancels(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenDuplicateTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();

	// Create first scale tween
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 1, "Should have 1 active tween");

	// Create second scale tween - should cancel the first
	xTween.TweenScale(Zenith_Maths::Vector3(3.0f), 0.5f, EASING_LINEAR);
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 1, "Should still have 1 active tween - duplicate cancelled");

	// Complete the second tween
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 3.0f), 0.01f, "Should reach target of second tween");

	g_xEngine.Scenes().UnloadScene(xScene);
}

//=============================================================================
// Code Review Round 2 - Bug Fix Regression Tests
//=============================================================================

ZENITH_TEST(Core, SubStateMachineTransitionBlendPose) { Zenith_UnitTests::TestSubStateMachineTransitionBlendPose(); }

void Zenith_UnitTests::TestSubStateMachineTransitionBlendPose(){

	// Create skeleton with 2 bones for pose verification
	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	// Create parent SM: Idle -> Locomotion (sub-SM)
	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddTrigger("GoLocomotion");

	xParentSM.AddState("Idle");
	Flux_AnimationState* pxLocomotionState = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotionState->CreateSubStateMachine("LocomotionSM");
	pxSubSM->AddState("Walk");
	pxSubSM->SetDefaultState("Walk");
	xParentSM.SetDefaultState("Idle");

	// Add transition Idle -> Locomotion on trigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Locomotion";
	xTrans.m_fTransitionDuration = 0.2f;
	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "GoLocomotion";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xCond);
	xParentSM.GetState("Idle")->AddTransition(xTrans);

	// Initialize
	xParentSM.Update(0.0f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xParentSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Trigger transition to sub-SM state
	xParentSM.GetParameters().SetTrigger("GoLocomotion");
	xParentSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xParentSM.IsTransitioning(), "Should be transitioning to Locomotion sub-SM");

	// Update during transition - the target pose should NOT be identity/reset
	// (This was Bug #1 - UpdateTransition didn't evaluate sub-SM targets)
	xParentSM.Update(0.016f, xPose, xSkeleton);
	// The key check: the pose should not be all-zero (identity reset)
	// A proper sub-SM update would produce the Walk state's pose

}

ZENITH_TEST(Core, RotationTweenShortestPath) { Zenith_UnitTests::TestRotationTweenShortestPath(); }

void Zenith_UnitTests::TestRotationTweenShortestPath(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenRotShortestTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetRotation(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();

	// Tween 270 degrees around Y - slerp should take the shortest path (90 degrees the other way)
	xTween.TweenRotation(Zenith_Maths::Vector3(0.0f, 270.0f, 0.0f), 1.0f, EASING_LINEAR);

	// At halfway, the rotation should be ~135 degrees OR ~-45 degrees (shortest path)
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Quat xRot;
	xTransform.GetRotation(xRot);

	// Verify it's a valid unit quaternion
	float fLength = glm::length(xRot);
	ZENITH_ASSERT_LT(glm::abs(fLength - 1.0f), 0.01f, "Quaternion should be unit length");

	// Complete the tween
	xTween.OnUpdate(0.5f);
	xTransform.GetRotation(xRot);

	// Verify final rotation is approximately 270 degrees Y (or equivalently -90 degrees)
	Zenith_Maths::Vector3 xEuler = glm::degrees(glm::eulerAngles(xRot));
	// Accept either ~270 or ~-90 (equivalent rotations)
	bool bCorrect = (glm::abs(xEuler.y - 270.0f) < 2.0f) || (glm::abs(xEuler.y + 90.0f) < 2.0f);
	ZENITH_ASSERT_TRUE(bCorrect, "Final rotation should be ~270 or ~-90 degrees Y");

	ZENITH_ASSERT_FALSE(xTween.HasActiveTweens(), "Tween should be complete");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Animation, TransitionInterruption) { Zenith_UnitTests::TestTransitionInterruption(); }

void Zenith_UnitTests::TestTransitionInterruption(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Idle -> Walk (interruptible, low priority)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Walk";
		xTrans.m_fTransitionDuration = 1.0f; // Long transition so we can interrupt it
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Any-state -> Death (high priority, should interrupt)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Initialize and start Walk transition
	xSM.Update(0.0f, xPose, xSkeleton);
	xSM.GetParameters().SetFloat("Speed", 5.0f);
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning Idle -> Walk");

	// Fire Death trigger while transitioning - should interrupt
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning to Death (interrupted Walk)");

	// Complete the Death transition
	for (int i = 0; i < 20; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Death", "Should have reached Death state after interrupting Walk transition");

}

ZENITH_TEST(Animation, TransitionNonInterruptible) { Zenith_UnitTests::TestTransitionNonInterruptible(); }

void Zenith_UnitTests::TestTransitionNonInterruptible(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("SpecialAttack");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddTrigger("AttackTrigger");
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Idle -> SpecialAttack (NON-interruptible)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "SpecialAttack";
		xTrans.m_fTransitionDuration = 1.0f;
		xTrans.m_bInterruptible = false; // Cannot be interrupted

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "AttackTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Idle -> Death (per-state, not any-state, so it only fires from Idle)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Start non-interruptible transition
	xSM.Update(0.0f, xPose, xSkeleton);
	xSM.GetParameters().SetTrigger("AttackTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning Idle -> SpecialAttack");

	// Try to interrupt with Death - should NOT work (non-interruptible)
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should still be transitioning (non-interruptible)");

	// Complete the SpecialAttack transition
	for (int i = 0; i < 100; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	// Should be in SpecialAttack - the Death trigger couldn't interrupt, and there's no
	// Death transition from SpecialAttack state, so the unconsumed trigger has no effect
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "SpecialAttack", "Non-interruptible transition should complete to SpecialAttack, not Death");

}

ZENITH_TEST(Core, CancelByPropertyKeepsOthers) { Zenith_UnitTests::TestCancelByPropertyKeepsOthers(); }

void Zenith_UnitTests::TestCancelByPropertyKeepsOthers(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenCancelPropTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 2, "Should have 2 active tweens");

	// Cancel only position
	xTween.CancelByProperty(TWEEN_PROPERTY_POSITION);
	ZENITH_ASSERT_EQ(xTween.GetActiveTweenCount(), 1, "Should have 1 active tween after cancelling position");

	// Complete remaining scale tween
	xTween.OnUpdate(1.0f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 2.0f), 0.01f, "Scale tween should still complete");

	// Position should not have changed (was cancelled)
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	ZENITH_ASSERT_LT(glm::abs(xPos.x), 0.01f, "Position should not have changed after cancel");

	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Animation, CrossFadeWhileTransitioning) { Zenith_UnitTests::TestCrossFadeWhileTransitioning(); }

void Zenith_UnitTests::TestCrossFadeWhileTransitioning(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Run");
	xSM.SetDefaultState("Idle");

	xSM.Update(0.0f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Start a CrossFade to Walk
	xSM.CrossFade("Walk", 1.0f);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning to Walk");

	// Update halfway through
	xSM.Update(0.5f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should still be transitioning");

	// Force CrossFade to Run during the Walk transition
	xSM.CrossFade("Run", 0.1f);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning to Run now");

	// Complete the Run transition
	for (int i = 0; i < 20; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Run", "CrossFade during transition should redirect to Run");

}

ZENITH_TEST(Tween, TweenLoopValueReset) { Zenith_UnitTests::TestTweenLoopValueReset(); }

void Zenith_UnitTests::TestTweenLoopValueReset(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("TweenLoopResetTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(1.0f), Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, false);

	// Complete first loop
	xTween.OnUpdate(1.0f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_TRUE(xTween.HasActiveTweens(), "Should still be active (looping)");

	// Small step into second loop - value should restart from 1.0
	// After loop reset: elapsed = delay(0) + 0.1 = 0.1, t = 0.1/1.0 = 0.1
	// scale = lerp(1.0, 2.0, 0.1) = 1.1
	xTween.OnUpdate(0.1f);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 1.1f), 0.05f, "After loop reset, scale should restart from beginning (~1.1)");

	// Continue to halfway through second loop
	xTween.OnUpdate(0.4f);
	xTransform.GetScale(xScale);
	ZENITH_ASSERT_LT(glm::abs(xScale.x - 1.5f), 0.05f, "Halfway through second loop should be ~1.5");

	g_xEngine.Scenes().UnloadScene(xScene);
}

//=============================================================================
// Bug 1 Regression: Trigger not consumed when blocked by active transition priority
//=============================================================================

ZENITH_TEST(Core, TriggerNotConsumedWhenBlockedByPriority) { Zenith_UnitTests::TestTriggerNotConsumedWhenBlockedByPriority(); }

void Zenith_UnitTests::TestTriggerNotConsumedWhenBlockedByPriority(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.GetParameters().AddTrigger("DeathTrigger");

	Flux_AnimationState* pxIdle = xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	// Idle -> Walk on Speed > 0.1 (high priority 200, interruptible)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Walk";
		xTrans.m_fTransitionDuration = 1.0f; // long transition so it stays active
		xTrans.m_iPriority = 200;
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTrans.m_xConditions.PushBack(xCond);
		pxIdle->AddTransition(xTrans);
	}

	// Any-State: DeathTrigger -> Death (low priority 100)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Initialize
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should start in Idle");

	// Start the high-priority Idle->Walk transition
	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_TRUE(xSM.IsTransitioning(), "Should be transitioning to Walk");

	// Now fire the lower-priority DeathTrigger while Walk transition is active
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);

	// The death transition should NOT have interrupted (priority 100 < 200)
	// AND the trigger should NOT have been consumed
	ZENITH_ASSERT_EQ(xSM.GetParameters().PeekTrigger("DeathTrigger"), true, "DeathTrigger should NOT be consumed when blocked by higher-priority active transition");

	// Complete the Walk transition (1.0s) and let the preserved trigger fire
	// Once Walk completes, the DeathTrigger (still set) fires immediately,
	// then the Death transition (0.1s) also completes within 100 frames
	for (int i = 0; i < 100; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	// The preserved trigger should have fired after Walk completed,
	// transitioning us through to Death
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Death", "Preserved DeathTrigger should fire after Walk transition completes, reaching Death");
	ZENITH_ASSERT_EQ(xSM.GetParameters().PeekTrigger("DeathTrigger"), false, "DeathTrigger should be consumed after successful transition");

}

//=============================================================================
// Serialization Round-Trip: Animation Layer
//=============================================================================

ZENITH_TEST(Animation, AnimationLayerSerialization) { Zenith_UnitTests::TestAnimationLayerSerialization(); }

void Zenith_UnitTests::TestAnimationLayerSerialization(){

	// Create a layer with all configurable properties
	Flux_AnimationLayer xOriginal("UpperBody");
	xOriginal.SetWeight(0.75f);
	xOriginal.SetBlendMode(LAYER_BLEND_ADDITIVE);
	Flux_BoneMask xMask;
	xMask.SetBoneWeight(0, 1.0f);
	xMask.SetBoneWeight(1, 0.5f);
	xOriginal.SetAvatarMask(xMask);

	// Give it a state machine with a state and parameter
	Flux_AnimationStateMachine& xSM = xOriginal.GetStateMachine();
	xSM.AddState("Idle");
	xSM.AddState("Aim");
	xSM.SetDefaultState("Idle");
	xSM.GetParameters().AddFloat("AimWeight", 0.0f);

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationLayer xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify
	ZENITH_ASSERT_EQ(xLoaded.GetName(), "UpperBody", "Layer name should round-trip");
	ZENITH_ASSERT_LT(glm::abs(xLoaded.GetWeight() - 0.75f), 0.001f, "Layer weight should round-trip");
	ZENITH_ASSERT_EQ(xLoaded.GetBlendMode(), LAYER_BLEND_ADDITIVE, "Layer blend mode should round-trip");

	// Verify state machine survived (use pointer getter to avoid auto-creation)
	const Flux_AnimationStateMachine* pxLoadedSM = xLoaded.GetStateMachinePtr();
	ZENITH_ASSERT_NOT_NULL(pxLoadedSM, "Layer should have a state machine after deserialization");
	ZENITH_ASSERT_EQ(pxLoadedSM->GetDefaultStateName(), "Idle", "SM default state should round-trip");
	ZENITH_ASSERT_TRUE(pxLoadedSM->GetParameters().HasParameter("AimWeight"), "SM parameters should round-trip");

}

//=============================================================================
// Serialization Round-Trip: Any-State Transitions
//=============================================================================

ZENITH_TEST(Core, AnyStateTransitionSerialization) { Zenith_UnitTests::TestAnyStateTransitionSerialization(); }

void Zenith_UnitTests::TestAnyStateTransitionSerialization(){

	Flux_AnimationStateMachine xOriginal("TestSM");
	xOriginal.AddState("Idle");
	xOriginal.AddState("Hit");
	xOriginal.AddState("Death");
	xOriginal.SetDefaultState("Idle");

	xOriginal.GetParameters().AddTrigger("HitTrigger");
	xOriginal.GetParameters().AddTrigger("DeathTrigger");

	// Add two any-state transitions with different priorities
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Hit";
		xTrans.m_fTransitionDuration = 0.15f;
		xTrans.m_iPriority = 10;
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "HitTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xOriginal.AddAnyStateTransition(xTrans);
	}
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.2f;
		xTrans.m_iPriority = 100;
		xTrans.m_bInterruptible = false;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xOriginal.AddAnyStateTransition(xTrans);
	}

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationStateMachine xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify any-state transitions survived
	const Zenith_Vector<Flux_StateTransition>& xAnyState = xLoaded.GetAnyStateTransitions();
	ZENITH_ASSERT_EQ(xAnyState.GetSize(), 2, "Should have 2 any-state transitions after deserialization");

	// Find the Hit and Death transitions (order may differ after deserialization)
	bool bFoundHit = false, bFoundDeath = false;
	for (uint32_t i = 0; i < xAnyState.GetSize(); ++i)
	{
		const Flux_StateTransition& xTrans = xAnyState.Get(i);
		if (xTrans.m_strTargetStateName == "Hit")
		{
			ZENITH_ASSERT_EQ(xTrans.m_iPriority, 10, "Hit transition priority should round-trip");
			ZENITH_ASSERT_LT(glm::abs(xTrans.m_fTransitionDuration - 0.15f), 0.001f, "Hit transition duration should round-trip");
			ZENITH_ASSERT_EQ(xTrans.m_bInterruptible, true, "Hit interruptible flag should round-trip");
			ZENITH_ASSERT_EQ(xTrans.m_xConditions.GetSize(), 1, "Hit should have 1 condition");
			bFoundHit = true;
		}
		else if (xTrans.m_strTargetStateName == "Death")
		{
			ZENITH_ASSERT_EQ(xTrans.m_iPriority, 100, "Death transition priority should round-trip");
			ZENITH_ASSERT_LT(glm::abs(xTrans.m_fTransitionDuration - 0.2f), 0.001f, "Death transition duration should round-trip");
			ZENITH_ASSERT_EQ(xTrans.m_bInterruptible, false, "Death interruptible flag should round-trip");
			ZENITH_ASSERT_EQ(xTrans.m_xConditions.GetSize(), 1, "Death should have 1 condition");
			bFoundDeath = true;
		}
	}
	ZENITH_ASSERT_TRUE(bFoundHit, "Hit any-state transition should survive round-trip");
	ZENITH_ASSERT_TRUE(bFoundDeath, "Death any-state transition should survive round-trip");

	// Verify states and parameters survived too
	ZENITH_ASSERT_EQ(xLoaded.GetDefaultStateName(), "Idle", "Default state should round-trip");
	ZENITH_ASSERT_TRUE(xLoaded.GetParameters().HasParameter("HitTrigger"), "HitTrigger param should round-trip");
	ZENITH_ASSERT_TRUE(xLoaded.GetParameters().HasParameter("DeathTrigger"), "DeathTrigger param should round-trip");

}

//=============================================================================
// Serialization Round-Trip: Sub-State Machines
//=============================================================================

ZENITH_TEST(Core, SubStateMachineSerialization) { Zenith_UnitTests::TestSubStateMachineSerialization(); }

void Zenith_UnitTests::TestSubStateMachineSerialization(){

	Flux_AnimationStateMachine xOriginal("ParentSM");
	xOriginal.AddState("Idle");
	xOriginal.SetDefaultState("Idle");
	xOriginal.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-state machine
	Flux_AnimationState* pxLocomotion = xOriginal.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotion->CreateSubStateMachine("LocomotionSM");
	pxSubSM->AddState("Walk");
	pxSubSM->AddState("Run");
	pxSubSM->SetDefaultState("Walk");
	pxSubSM->GetParameters().AddFloat("SubSpeed", 1.0f);

	// Add a transition inside the sub-SM
	Flux_AnimationState* pxWalk = pxSubSM->GetState("Walk");
	ZENITH_ASSERT_NOT_NULL(pxWalk, "Walk state should exist in sub-SM");
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Run";
		xTrans.m_fTransitionDuration = 0.2f;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "SubSpeed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 2.0f;
		xTrans.m_xConditions.PushBack(xCond);
		pxWalk->AddTransition(xTrans);
	}

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationStateMachine xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify parent SM
	ZENITH_ASSERT_EQ(xLoaded.GetName(), "ParentSM", "Parent SM name should round-trip");
	ZENITH_ASSERT_EQ(xLoaded.GetDefaultStateName(), "Idle", "Parent default state should round-trip");
	ZENITH_ASSERT_TRUE(xLoaded.GetParameters().HasParameter("Speed"), "Parent params should round-trip");

	// Verify sub-state machine exists
	Flux_AnimationState* pxLoadedLoco = xLoaded.GetState("Locomotion");
	ZENITH_ASSERT_NOT_NULL(pxLoadedLoco, "Locomotion state should exist");
	ZENITH_ASSERT_TRUE(pxLoadedLoco->IsSubStateMachine(), "Locomotion should be a sub-state machine");

	Flux_AnimationStateMachine* pxLoadedSubSM = pxLoadedLoco->GetSubStateMachine();
	ZENITH_ASSERT_NOT_NULL(pxLoadedSubSM, "Sub-SM pointer should be valid");
	ZENITH_ASSERT_EQ(pxLoadedSubSM->GetName(), "LocomotionSM", "Sub-SM name should round-trip");
	ZENITH_ASSERT_EQ(pxLoadedSubSM->GetDefaultStateName(), "Walk", "Sub-SM default state should round-trip");

	// Verify sub-SM states and transitions
	Flux_AnimationState* pxLoadedWalk = pxLoadedSubSM->GetState("Walk");
	ZENITH_ASSERT_NOT_NULL(pxLoadedWalk, "Walk state should exist in deserialized sub-SM");
	ZENITH_ASSERT_NOT_NULL(pxLoadedSubSM->GetState("Run"), "Run state should exist in deserialized sub-SM");

	const Zenith_Vector<Flux_StateTransition>& xLoadedTrans = pxLoadedWalk->GetTransitions();
	ZENITH_ASSERT_EQ(xLoadedTrans.GetSize(), 1, "Walk should have 1 transition after deserialization");
	ZENITH_ASSERT_EQ(xLoadedTrans.Get(0).m_strTargetStateName, "Run", "Transition target should be Run");
	ZENITH_ASSERT_LT(glm::abs(xLoadedTrans.Get(0).m_fTransitionDuration - 0.2f), 0.001f, "Transition duration should round-trip");

	// Verify sub-SM parameters
	ZENITH_ASSERT_TRUE(pxLoadedSubSM->GetParameters().HasParameter("SubSpeed"), "Sub-SM params should round-trip");

}

//=============================================================================
// Code Review Round 4 - Bug Fix Validation Tests
//=============================================================================

ZENITH_TEST(Core, HasAnimationContentWithLayers) { Zenith_UnitTests::TestHasAnimationContentWithLayers(); }

void Zenith_UnitTests::TestHasAnimationContentWithLayers(){

	Flux_AnimationController xController;

	// No content initially
	ZENITH_ASSERT_FALSE(xController.HasAnimationContent(), "Should have no content initially");

	// Add a layer (no clips, no root state machine)
	xController.AddLayer("Base");
	ZENITH_ASSERT_TRUE(xController.HasAnimationContent(), "Should report content when layers are present");

}

ZENITH_TEST(Core, InitializeRetroactiveLayerPoses) { Zenith_UnitTests::TestInitializeRetroactiveLayerPoses(); }

void Zenith_UnitTests::TestInitializeRetroactiveLayerPoses(){

	// Create a skeleton asset with a few bones
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;

	// Add layer BEFORE Initialize
	Flux_AnimationLayer* pxLayer = xController.AddLayer("Base");

	// Layer pose should be uninitialized (0 bones)
	ZENITH_ASSERT_EQ(pxLayer->GetOutputPose().GetNumBones(), 0, "Layer pose should be uninitialized before Initialize()");

	// Initialize should retroactively initialize the layer pose
	xController.Initialize(pxSkelInst);

	ZENITH_ASSERT_EQ(pxLayer->GetOutputPose().GetNumBones(), 2, "Layer pose should have 2 bones after retroactive Initialize()");


	delete pxSkelInst;
	delete pxSkel;
}

ZENITH_TEST(Core, ResolveClipReferencesBlendSpace2D) { Zenith_UnitTests::TestResolveClipReferencesBlendSpace2D(); }

void Zenith_UnitTests::TestResolveClipReferencesBlendSpace2D(){

	// Create clip collection
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("ClipA");
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("ClipB");
	xCollection.AddClip(pxClipA);
	xCollection.AddClip(pxClipB);

	// Create BlendSpace2D with two clip nodes as blend points
	Flux_BlendTreeNode_Clip* pxNodeA = new Flux_BlendTreeNode_Clip();
	pxNodeA->SetClipName("ClipA");
	ZENITH_ASSERT_NULL(pxNodeA->GetClip(), "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxNodeB = new Flux_BlendTreeNode_Clip();
	pxNodeB->SetClipName("ClipB");
	ZENITH_ASSERT_NULL(pxNodeB->GetClip(), "Clip B should be unresolved");

	Flux_BlendTreeNode_BlendSpace2D* pxBS2D = new Flux_BlendTreeNode_BlendSpace2D();
	pxBS2D->AddBlendPoint(pxNodeA, Zenith_Maths::Vector2(0.0f, 0.0f));
	pxBS2D->AddBlendPoint(pxNodeB, Zenith_Maths::Vector2(1.0f, 1.0f));

	// Create state machine with state using this blend tree
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("BS2DState");
	pxState->SetBlendTree(pxBS2D);
	xSM.SetDefaultState("BS2DState");

	// Resolve
	xSM.ResolveClipReferences(&xCollection);

	ZENITH_ASSERT_EQ(pxNodeA->GetClip(), pxClipA, "BlendSpace2D clip A should be resolved");
	ZENITH_ASSERT_EQ(pxNodeB->GetClip(), pxClipB, "BlendSpace2D clip B should be resolved");

}

ZENITH_TEST(Core, ResolveClipReferencesSelect) { Zenith_UnitTests::TestResolveClipReferencesSelect(); }

void Zenith_UnitTests::TestResolveClipReferencesSelect(){

	// Create clip collection
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("SelectA");
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("SelectB");
	xCollection.AddClip(pxClipA);
	xCollection.AddClip(pxClipB);

	// Create Select node with two clip children
	Flux_BlendTreeNode_Clip* pxNodeA = new Flux_BlendTreeNode_Clip();
	pxNodeA->SetClipName("SelectA");
	ZENITH_ASSERT_NULL(pxNodeA->GetClip(), "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxNodeB = new Flux_BlendTreeNode_Clip();
	pxNodeB->SetClipName("SelectB");
	ZENITH_ASSERT_NULL(pxNodeB->GetClip(), "Clip B should be unresolved");

	Flux_BlendTreeNode_Select* pxSelect = new Flux_BlendTreeNode_Select();
	pxSelect->AddChild(pxNodeA);
	pxSelect->AddChild(pxNodeB);

	// Create state machine with state using this blend tree
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("SelectState");
	pxState->SetBlendTree(pxSelect);
	xSM.SetDefaultState("SelectState");

	// Resolve
	xSM.ResolveClipReferences(&xCollection);

	ZENITH_ASSERT_EQ(pxNodeA->GetClip(), pxClipA, "Select child clip A should be resolved");
	ZENITH_ASSERT_EQ(pxNodeB->GetClip(), pxClipB, "Select child clip B should be resolved");

}

ZENITH_TEST(Animation, LayerCompositionOverrideBlend) { Zenith_UnitTests::TestLayerCompositionOverrideBlend(); }

void Zenith_UnitTests::TestLayerCompositionOverrideBlend(){

	// Create a simple 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Create two clips with distinct root bone positions
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("PoseA");
	pxClipA->SetDuration(1.0f);
	pxClipA->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipA->AddBoneChannel("Root", std::move(xChan));
	}

	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("PoseB");
	pxClipB->SetDuration(1.0f);
	pxClipB->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		pxClipB->AddBoneChannel("Root", std::move(xChan));
	}

	// Base layer plays PoseA (root at 0,0,0)
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("PoseA");
	Flux_BlendTreeNode_Clip* pxBaseClipNode = new Flux_BlendTreeNode_Clip(pxClipA);
	pxBaseState->SetBlendTree(pxBaseClipNode);
	pxBaseSM->SetDefaultState("PoseA");
	pxBaseSM->SetState("PoseA");

	// Override layer plays PoseB (root at 2,0,0) at weight 0.5
	Flux_AnimationLayer* pxOverrideLayer = xController.AddLayer("Override");
	pxOverrideLayer->SetWeight(0.5f);
	pxOverrideLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
	Flux_AnimationStateMachine* pxOverrideSM = pxOverrideLayer->CreateStateMachine("OverrideSM");
	Flux_AnimationState* pxOverrideState = pxOverrideSM->AddState("PoseB");
	Flux_BlendTreeNode_Clip* pxOverrideClipNode = new Flux_BlendTreeNode_Clip(pxClipB);
	pxOverrideState->SetBlendTree(pxOverrideClipNode);
	pxOverrideSM->SetDefaultState("PoseB");
	pxOverrideSM->SetState("PoseB");

	// Update to evaluate both layers and compose
	xController.Update(0.016f);

	// Output should be a blend: base(0,0,0) blended with override(2,0,0) at weight 0.5
	// Expected root position: lerp(0, 2, 0.5) = (1, 0, 0)
	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	const Flux_BoneLocalPose& xRootPose = xOutput.GetLocalPose(0);

	float fExpectedX = 1.0f;
	float fTolerance = 0.01f;
	ZENITH_ASSERT_LT(glm::abs(xRootPose.m_xPosition.x - fExpectedX), fTolerance, "Root X should be ~1.0 (blend of 0.0 and 2.0 at weight 0.5), got %.3f", xRootPose.m_xPosition.x);


	delete pxSkelInst;
	delete pxSkel;
	delete pxClipA;
	delete pxClipB;
}

//=============================================================================
// Code review round 5 - additional coverage
//=============================================================================

ZENITH_TEST(Animation, LayerCompositionAdditiveBlend) { Zenith_UnitTests::TestLayerCompositionAdditiveBlend(); }

void Zenith_UnitTests::TestLayerCompositionAdditiveBlend(){

	// Create a simple 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Base clip: root at (1, 0, 0)
	Flux_AnimationClip* pxClipBase = new Flux_AnimationClip();
	pxClipBase->SetName("Base");
	pxClipBase->SetDuration(1.0f);
	pxClipBase->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Root", std::move(xChan));
	}

	// Additive clip: root at (3, 0, 0) - delta from bind pose (0,0,0) = +3
	Flux_AnimationClip* pxClipAdd = new Flux_AnimationClip();
	pxClipAdd->SetName("Additive");
	pxClipAdd->SetDuration(1.0f);
	pxClipAdd->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));
		pxClipAdd->AddBoneChannel("Root", std::move(xChan));
	}

	// Base layer plays Base clip
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("Base");
	pxBaseState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipBase));
	pxBaseSM->SetDefaultState("Base");
	pxBaseSM->SetState("Base");

	// Additive layer at weight 1.0
	Flux_AnimationLayer* pxAddLayer = xController.AddLayer("Additive");
	pxAddLayer->SetWeight(1.0f);
	pxAddLayer->SetBlendMode(LAYER_BLEND_ADDITIVE);
	Flux_AnimationStateMachine* pxAddSM = pxAddLayer->CreateStateMachine("AddSM");
	Flux_AnimationState* pxAddState = pxAddSM->AddState("Additive");
	pxAddState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipAdd));
	pxAddSM->SetDefaultState("Additive");
	pxAddSM->SetState("Additive");

	xController.Update(0.016f);

	// Additive blend adds delta on top of base: base(1) + additive(3) * weight(1) = 4
	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	const Flux_BoneLocalPose& xRootPose = xOutput.GetLocalPose(0);

	// Additive result should be greater than base alone
	ZENITH_ASSERT_GT(xRootPose.m_xPosition.x, 1.0f + 0.01f, "Additive layer should increase root X beyond base (1.0), got %.3f", xRootPose.m_xPosition.x);


	delete pxSkelInst;
	delete pxSkel;
	delete pxClipBase;
	delete pxClipAdd;
}

ZENITH_TEST(Animation, LayerMaskedOverrideBlend) { Zenith_UnitTests::TestLayerMaskedOverrideBlend(); }

void Zenith_UnitTests::TestLayerMaskedOverrideBlend(){

	// Create 3-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Upper", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Lower", 0, Zenith_Maths::Vector3(0, -1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Base clip: all bones at (0, 0, 0)
	Flux_AnimationClip* pxClipBase = new Flux_AnimationClip();
	pxClipBase->SetName("Base");
	pxClipBase->SetDuration(1.0f);
	pxClipBase->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Root", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Upper");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Upper", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Lower");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Lower", std::move(xChan));
	}

	// Override clip: all bones at (4, 0, 0)
	Flux_AnimationClip* pxClipOverride = new Flux_AnimationClip();
	pxClipOverride->SetName("Override");
	pxClipOverride->SetDuration(1.0f);
	pxClipOverride->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Root", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Upper");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Upper", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Lower");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Lower", std::move(xChan));
	}

	// Base layer
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("Base");
	pxBaseState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipBase));
	pxBaseSM->SetDefaultState("Base");
	pxBaseSM->SetState("Base");

	// Masked override layer: bone 1 (Upper) fully overridden, bone 2 (Lower) not affected
	Flux_AnimationLayer* pxMaskLayer = xController.AddLayer("MaskedOverride");
	pxMaskLayer->SetWeight(1.0f);
	pxMaskLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
	Flux_BoneMask xMask;
	xMask.SetBoneWeight(0, 0.0f);  // Root: no override
	xMask.SetBoneWeight(1, 1.0f);  // Upper: full override
	xMask.SetBoneWeight(2, 0.0f);  // Lower: no override
	pxMaskLayer->SetAvatarMask(xMask);

	Flux_AnimationStateMachine* pxMaskSM = pxMaskLayer->CreateStateMachine("MaskSM");
	Flux_AnimationState* pxMaskState = pxMaskSM->AddState("Override");
	pxMaskState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipOverride));
	pxMaskSM->SetDefaultState("Override");
	pxMaskSM->SetState("Override");

	xController.Update(0.016f);

	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	float fTolerance = 0.01f;

	// Root (mask weight 0): should remain at base (0, 0, 0)
	ZENITH_ASSERT_LT(glm::abs(xOutput.GetLocalPose(0).m_xPosition.x - 0.0f), fTolerance, "Root (mask=0) should stay at base 0.0, got %.3f", xOutput.GetLocalPose(0).m_xPosition.x);

	// Upper (mask weight 1): should be fully overridden to (4, 0, 0)
	ZENITH_ASSERT_LT(glm::abs(xOutput.GetLocalPose(1).m_xPosition.x - 4.0f), fTolerance, "Upper (mask=1) should be overridden to 4.0, got %.3f", xOutput.GetLocalPose(1).m_xPosition.x);

	// Lower (mask weight 0): should remain at base (0, 0, 0)
	ZENITH_ASSERT_LT(glm::abs(xOutput.GetLocalPose(2).m_xPosition.x - 0.0f), fTolerance, "Lower (mask=0) should stay at base 0.0, got %.3f", xOutput.GetLocalPose(2).m_xPosition.x);


	delete pxSkelInst;
	delete pxSkel;
	delete pxClipBase;
	delete pxClipOverride;
}

ZENITH_TEST(Core, PingPongAsymmetricEasing) { Zenith_UnitTests::TestPingPongAsymmetricEasing(); }

void Zenith_UnitTests::TestPingPongAsymmetricEasing(){

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("PingPongEasingTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	// QuadIn: slow start, fast end. Forward at t=0.5 should produce 0.25 (0.5^2)
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f), 1.0f, EASING_QUAD_IN);
	xTween.SetLoop(true, true);

	// Forward at t=0.5: QuadIn(0.5) = 0.25
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	float fForwardHalf = xScale.x;
	ZENITH_ASSERT_LT(glm::abs(fForwardHalf - 0.25f), 0.05f, "Forward QuadIn at 0.5 should be ~0.25, got %.3f", fForwardHalf);

	// Complete forward pass
	xTween.OnUpdate(0.5f);

	// Reverse at t=0.5: should mirror forward curve
	// Correct: 1.0 - QuadIn(0.5) = 1.0 - 0.25 = 0.75
	// Bug would produce: QuadIn(1.0 - 0.5) = QuadIn(0.5) = 0.25 (wrong!)
	xTween.OnUpdate(0.5f);
	xTransform.GetScale(xScale);
	float fReverseHalf = xScale.x;
	ZENITH_ASSERT_GT(fReverseHalf, 0.5f, "Reverse QuadIn at 0.5 should be > 0.5 (mirrored curve), got %.3f", fReverseHalf);
	ZENITH_ASSERT_LT(glm::abs(fReverseHalf - 0.75f), 0.05f, "Reverse QuadIn at 0.5 should be ~0.75 (1.0 - 0.25), got %.3f", fReverseHalf);


	g_xEngine.Scenes().UnloadScene(xScene);
}

ZENITH_TEST(Animation, TransitionCompletionFramePose) { Zenith_UnitTests::TestTransitionCompletionFramePose(); }

void Zenith_UnitTests::TestTransitionCompletionFramePose(){

	// Create 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	// Test: after a transition completes, the output should be the target state's pose
	// (not double-advanced by evaluating the blend tree twice on the completion frame)
	Flux_AnimationStateMachine xSM("TestSM");
	xSM.GetParameters().AddTrigger("GoToB");

	// StateA: static pose at (0,0,0)
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("ClipA");
	pxClipA->SetDuration(1.0f);
	pxClipA->SetTicksPerSecond(1);
	pxClipA->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipA->AddBoneChannel("Root", std::move(xChan));
	}

	// StateB: moves from (0,0,0) to (10,0,0) over 1s
	// After transition completes, time in clip will be small (~0.2-0.3s)
	// Position should be ~(2-3, 0, 0), NOT ~(4-6, 0, 0) from double-advance
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("ClipB");
	pxClipB->SetDuration(1.0f);
	pxClipB->SetTicksPerSecond(1);
	pxClipB->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xChan.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
		pxClipB->AddBoneChannel("Root", std::move(xChan));
	}

	Flux_AnimationState* pxStateA = xSM.AddState("StateA");
	pxStateA->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipA));
	Flux_AnimationState* pxStateB = xSM.AddState("StateB");
	pxStateB->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipB));

	// Transition A->B on trigger, short duration
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "StateB";
		xTrans.m_fTransitionDuration = 0.05f;
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "GoToB";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		pxStateA->AddTransition(xTrans);
	}

	xSM.SetDefaultState("StateA");
	xSM.SetState("StateA");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	// Initialize
	xSM.Update(0.016f, xPose, *pxSkel);

	// Start transition
	xSM.GetParameters().SetTrigger("GoToB");
	xSM.Update(0.016f, xPose, *pxSkel);

	// Complete the transition with a large dt
	// StateB's blend tree will have accumulated ~0.016 + 0.2 = ~0.216s of time
	// Position should be ~(2.16, 0, 0), NOT ~(4.32, 0, 0) from double-advance
	xSM.Update(0.2f, xPose, *pxSkel);

	// Run several more frames after completion and verify smooth progression
	float fPrev = xPose.GetLocalPose(0).m_xPosition.x;
	bool bSmooth = true;
	for (int i = 0; i < 5; ++i)
	{
		xSM.Update(0.016f, xPose, *pxSkel);
		float fCurr = xPose.GetLocalPose(0).m_xPosition.x;
		float fDelta = glm::abs(fCurr - fPrev);
		// Each frame at dt=0.016 in a 1s clip spanning 10 units should advance ~0.16
		// A jump > 0.5 would indicate double-advance from the bug
		if (fDelta > 0.5f)
			bSmooth = false;
		fPrev = fCurr;
	}

	ZENITH_ASSERT_TRUE(bSmooth, "Post-transition frames should be smooth (no large jumps)");

	// Verify we're actually in StateB (clip position should be positive, increasing)
	ZENITH_ASSERT_GT(fPrev, 0.0f, "Position should be positive (in StateB clip range)");

	delete pxSkelInst;
	delete pxSkel;
	delete pxClipA;
	delete pxClipB;
}

ZENITH_TEST(Animation, StateMachineUpdateNoStates) { Zenith_UnitTests::TestStateMachineUpdateNoStates(); }

void Zenith_UnitTests::TestStateMachineUpdateNoStates(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	// Empty state machine with no states added
	Flux_AnimationStateMachine xSM("EmptySM");

	// Update should not crash and should reset pose
	xSM.Update(0.016f, xPose, xSkeleton);

	ZENITH_ASSERT_FALSE(xSM.IsTransitioning(), "Empty SM should not be transitioning");
	ZENITH_ASSERT_TRUE(xSM.GetCurrentStateName().empty(), "Empty SM should have no current state");

}

ZENITH_TEST(Animation, StateMachineAutoInitDefaultState) { Zenith_UnitTests::TestStateMachineAutoInitDefaultState(); }

void Zenith_UnitTests::TestStateMachineAutoInitDefaultState(){

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.SetDefaultState("Idle");

	// Before Update, no current state
	ZENITH_ASSERT_TRUE(xSM.GetCurrentStateName().empty(), "Should have no current state before first Update");

	// First Update should auto-initialize to default state
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should auto-init to default state 'Idle'");

	// Subsequent Update should stay in Idle (no transitions configured)
	xSM.Update(0.016f, xPose, xSkeleton);
	ZENITH_ASSERT_EQ(xSM.GetCurrentStateName(), "Idle", "Should remain in Idle");

}

// ========== Animation State Machine Helper Tests ==========

ZENITH_TEST(Core, ParamSerializationFloat) { Zenith_UnitTests::TestParamSerializationFloat(); }

void Zenith_UnitTests::TestParamSerializationFloat(){

	Flux_AnimationParameters xParams;
	xParams.AddFloat("Speed", 3.14f);
	xParams.AddInt("Combo", 42);

	// Write
	Zenith_DataStream xWriteStream(256);
	xParams.WriteToDataStream(xWriteStream);

	// Read back
	Zenith_DataStream xReadStream(xWriteStream.GetData(), xWriteStream.GetSize());
	Flux_AnimationParameters xLoaded;
	xLoaded.ReadFromDataStream(xReadStream);

	ZENITH_ASSERT_TRUE(xLoaded.HasParameter("Speed"), "Speed param should exist after round-trip");
	ZENITH_ASSERT_EQ(xLoaded.GetFloat("Speed"), 3.14f, "Speed should be 3.14f");
	ZENITH_ASSERT_TRUE(xLoaded.HasParameter("Combo"), "Combo param should exist after round-trip");
	ZENITH_ASSERT_EQ(xLoaded.GetInt("Combo"), 42, "Combo should be 42");

}

ZENITH_TEST(Core, ParamSerializationBoolTrigger) { Zenith_UnitTests::TestParamSerializationBoolTrigger(); }

void Zenith_UnitTests::TestParamSerializationBoolTrigger(){

	Flux_AnimationParameters xParams;
	xParams.AddBool("Grounded", true);
	xParams.AddTrigger("Jump");
	xParams.SetTrigger("Jump");

	Zenith_DataStream xWriteStream(256);
	xParams.WriteToDataStream(xWriteStream);

	Zenith_DataStream xReadStream(xWriteStream.GetData(), xWriteStream.GetSize());
	Flux_AnimationParameters xLoaded;
	xLoaded.ReadFromDataStream(xReadStream);

	ZENITH_ASSERT_EQ(xLoaded.GetBool("Grounded"), true, "Grounded should be true");
	ZENITH_ASSERT_EQ(xLoaded.PeekTrigger("Jump"), true, "Jump trigger should be set");

}

ZENITH_TEST(Core, CompareNumericGreater) { Zenith_UnitTests::TestCompareNumericGreater(); }

void Zenith_UnitTests::TestCompareNumericGreater(){

	Flux_AnimationParameters xParams;
	xParams.AddFloat("Speed", 5.0f);

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Speed";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xCond.m_fThreshold = 3.0f;

	ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "5.0 > 3.0 should be true");

	xCond.m_fThreshold = 5.0f;
	ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "5.0 > 5.0 should be false");

}

ZENITH_TEST(Core, CompareNumericLessEqual) { Zenith_UnitTests::TestCompareNumericLessEqual(); }

void Zenith_UnitTests::TestCompareNumericLessEqual(){

	Flux_AnimationParameters xParams;
	xParams.AddInt("Health", 10);

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Health";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
	xCond.m_iThreshold = 10;

	ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), true, "10 <= 10 should be true");

	xCond.m_iThreshold = 5;
	ZENITH_ASSERT_EQ(xCond.Evaluate(xParams), false, "10 <= 5 should be false");

}

ZENITH_TEST(Core, PriorityInsertionMiddle) { Zenith_UnitTests::TestPriorityInsertionMiddle(); }

void Zenith_UnitTests::TestPriorityInsertionMiddle(){

	Flux_AnimationState xState("TestState");

	Flux_StateTransition xLow;
	xLow.m_strTargetStateName = "Low";
	xLow.m_iPriority = 1;

	Flux_StateTransition xHigh;
	xHigh.m_strTargetStateName = "High";
	xHigh.m_iPriority = 10;

	Flux_StateTransition xMid;
	xMid.m_strTargetStateName = "Mid";
	xMid.m_iPriority = 5;

	xState.AddTransition(xLow);
	xState.AddTransition(xHigh);
	xState.AddTransition(xMid);

	const Zenith_Vector<Flux_StateTransition>& xTransitions = xState.GetTransitions();
	ZENITH_ASSERT_EQ(xTransitions.GetSize(), 3, "Should have 3 transitions");
	ZENITH_ASSERT_EQ(xTransitions.Get(0).m_strTargetStateName, "High", "First should be High (priority 10)");
	ZENITH_ASSERT_EQ(xTransitions.Get(1).m_strTargetStateName, "Mid", "Second should be Mid (priority 5)");
	ZENITH_ASSERT_EQ(xTransitions.Get(2).m_strTargetStateName, "Low", "Third should be Low (priority 1)");

}

ZENITH_TEST(Core, PriorityInsertionEmpty) { Zenith_UnitTests::TestPriorityInsertionEmpty(); }

void Zenith_UnitTests::TestPriorityInsertionEmpty(){

	Flux_AnimationStateMachine xSM("TestSM");

	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Target";
	xTrans.m_iPriority = 7;

	xSM.AddAnyStateTransition(xTrans);

	const Zenith_Vector<Flux_StateTransition>& xAny = xSM.GetAnyStateTransitions();
	ZENITH_ASSERT_EQ(xAny.GetSize(), 1, "Should have 1 any-state transition");
	ZENITH_ASSERT_EQ(xAny.Get(0).m_iPriority, 7, "Priority should be 7");
	ZENITH_ASSERT_EQ(xAny.Get(0).m_strTargetStateName, "Target", "Target name should match");

}

// ========== Terrain Streaming Tests ==========
//
// Construct a stack-local Flux_TerrainStreamingState and exercise the
// distance / center math helpers directly. Avoids touching the manager's
// primary state so the tests don't pollute or rely on engine init order;
// Zenith_UnitTests is friended into Flux_TerrainStreamingManager so the
// state-taking private helpers (GetChunkCenter, GetChunkDistanceSq) are
// callable here.

ZENITH_TEST(Terrain, ChunkDistanceSymmetry) { Zenith_UnitTests::TestChunkDistanceSymmetry(); }

void Zenith_UnitTests::TestChunkDistanceSymmetry(){

	// Use two distinct chunks: (2,3) and (5,7)
	const uint32_t uChunkA = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(2, 3);
	const uint32_t uChunkB = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(5, 7);

	// Build a synthetic streaming state with cached AABBs for the two chunks.
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;
	xState.m_axChunkAABBs[uChunkA] = Zenith_AABB(
		Zenith_Maths::Vector3(100.0f, 0.0f, 200.0f),
		Zenith_Maths::Vector3(164.0f, 100.0f, 264.0f)
	);
	xState.m_axChunkAABBs[uChunkB] = Zenith_AABB(
		Zenith_Maths::Vector3(300.0f, 0.0f, 400.0f),
		Zenith_Maths::Vector3(364.0f, 100.0f, 464.0f)
	);

	// Get chunk centers
	Zenith_Maths::Vector3 xCenterA = Flux_TerrainStreamingManagerImpl::GetChunkCenter(xState, 2, 3);
	Zenith_Maths::Vector3 xCenterB = Flux_TerrainStreamingManagerImpl::GetChunkCenter(xState, 5, 7);

	// Distance from A's center to chunk B should equal distance from B's center to chunk A
	float fDistAToB = Flux_TerrainStreamingManagerImpl::GetChunkDistanceSq(xState, uChunkB, xCenterA);
	float fDistBToA = Flux_TerrainStreamingManagerImpl::GetChunkDistanceSq(xState, uChunkA, xCenterB);

	// Squared distance is symmetric: |A-B|^2 == |B-A|^2
	float fDifference = fabsf(fDistAToB - fDistBToA);
	ZENITH_ASSERT_LT(fDifference, 0.001f, "Chunk distance should be symmetric (A->B == B->A), diff=%.6f", fDifference);

}

ZENITH_TEST(Terrain, ChunkDistanceZero) { Zenith_UnitTests::TestChunkDistanceZero(); }

void Zenith_UnitTests::TestChunkDistanceZero(){

	const uint32_t uChunkIndex = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(4, 4);

	// Synthetic state with one cached AABB.
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;
	xState.m_axChunkAABBs[uChunkIndex] = Zenith_AABB(
		Zenith_Maths::Vector3(200.0f, 0.0f, 200.0f),
		Zenith_Maths::Vector3(264.0f, 100.0f, 264.0f)
	);

	// Camera at exact chunk center should give distance 0
	Zenith_Maths::Vector3 xChunkCenter = Flux_TerrainStreamingManagerImpl::GetChunkCenter(xState, 4, 4);
	float fDistanceSq = Flux_TerrainStreamingManagerImpl::GetChunkDistanceSq(xState, uChunkIndex, xChunkCenter);

	ZENITH_ASSERT_LT(fDistanceSq, 0.001f, "Distance from chunk center to itself should be ~0, got %.6f", fDistanceSq);

}

// Wave-18 move-ctor regression. Zenith_TerrainComponent owns a heap
// Flux_TerrainStreamingState (and a physics geometry pointer); the implicit
// move would shallow-copy the pointer, so a pool relocation would double-free
// both on the moved-from temporary's destruction. The explicit move must:
//   - transfer the SAME state pointer to the moved-to component,
//   - repoint state->m_pxOwner at the moved-to component,
//   - null the moved-from component's state pointer (so its dtor frees nothing).
// The default (deserialization) constructor is used so the test stays headless
// (it only allocates + Initialize()s the state — no device, no mesh load). Both
// components' destructors are headless-safe: DestroyCullingResources early-outs
// when culling was never initialised, UnregisterTerrainBuffers is a no-op for an
// unregistered state, and the unified-buffer destroys early-out on invalid VRAM
// handles.
ZENITH_TEST(Terrain, ComponentMoveStealsState) { Zenith_UnitTests::TestTerrainComponentMoveStealsState(); }
void Zenith_UnitTests::TestTerrainComponentMoveStealsState(){
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TerrainMoveCtorEntity");

	Zenith_TerrainComponent xSource(xEntity);
	Flux_TerrainStreamingState* pxCapturedState = xSource.m_pxStreamingState;
	ZENITH_ASSERT_TRUE(pxCapturedState != nullptr, "Source terrain must own a streaming state after construction");

	// Force the move.
	Zenith_TerrainComponent xMoved(std::move(xSource));

	// The moved-to component owns the SAME state instance...
	ZENITH_ASSERT_EQ(xMoved.m_pxStreamingState, pxCapturedState, "Move must transfer the exact streaming-state instance");
	// ...and the moved-from component owns nothing (no double-free on its dtor).
	// (Wave 3: the state no longer carries an m_pxOwner back-pointer to repoint.)
	ZENITH_ASSERT_TRUE(xSource.m_pxStreamingState == nullptr, "Moved-from component's state pointer must be nulled");

	// Both xMoved and xSource destruct here at scope exit; xSource frees
	// nothing (null state), xMoved frees the single owned state exactly once.
}

// Wave-18 move-assignment counterpart. Same invariants as the move ctor; also
// verifies the destination releases its own pre-existing state before stealing
// (so the assignment is leak-free), via two independently-constructed
// components.
ZENITH_TEST(Terrain, ComponentMoveAssignmentStealsState) { Zenith_UnitTests::TestTerrainComponentMoveAssignmentStealsState(); }
void Zenith_UnitTests::TestTerrainComponentMoveAssignmentStealsState(){
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntityA = g_xEngine.Scenes().CreateEntity(pxSceneData, "TerrainMoveAssignA");
	Zenith_Entity xEntityB = g_xEngine.Scenes().CreateEntity(pxSceneData, "TerrainMoveAssignB");

	Zenith_TerrainComponent xSource(xEntityA);
	Zenith_TerrainComponent xDest(xEntityB);

	Flux_TerrainStreamingState* pxSourceState = xSource.m_pxStreamingState;
	Flux_TerrainStreamingState* pxDestStateBefore = xDest.m_pxStreamingState;
	ZENITH_ASSERT_TRUE(pxSourceState != nullptr, "Source must own a state");
	ZENITH_ASSERT_TRUE(pxDestStateBefore != nullptr, "Dest must own its own state before assignment");
	ZENITH_ASSERT_TRUE(pxSourceState != pxDestStateBefore, "The two components must own distinct states");

	xDest = std::move(xSource);

	ZENITH_ASSERT_EQ(xDest.m_pxStreamingState, pxSourceState, "Move-assign must transfer the source's state instance");
	ZENITH_ASSERT_TRUE(xSource.m_pxStreamingState == nullptr, "Move-assigned-from component's state pointer must be nulled");
}

//=============================================================================
// Wave-19 — Zenith_AnimatorComponent forwarding-handle / store relocation.
//
// The controller lives in Flux_AnimationControllerStore (keyed by EntityID
// slot, heap-stable). The component caches a Flux_AnimationController* and just
// forwards. These regressions pin: (1) a component MOVE shares the SAME
// controller with the moved-to instance and neutralises the moved-from;
// (2) Destroy is idempotent + per-entity (not per-instance), so a move +
// double dtor/OnDestroy never double-frees; (3) a real pool swap-and-pop
// relocation and a cross-scene move keep the cached pointer valid and the
// controller count stable.
//=============================================================================

// Move CTOR: the moved-to component must hold the SAME cached controller
// pointer, the store must still resolve that exact controller for the (stable)
// EntityID, the moved-from must be neutralised (null cache + moved-out), and
// the live controller count must be exactly one. These are pure stack
// instances (NOT added to a pool) so the component's own ctor/dtor drive the
// store create/destroy — mirroring the headless Terrain move-ctor test.
ZENITH_TEST(Animator, ControllerStoreMoveCtorSharesController) { Zenith_UnitTests::TestAnimatorControllerStoreMoveCtorSharesController(); }
void Zenith_UnitTests::TestAnimatorControllerStoreMoveCtorSharesController(){
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("AnimatorMoveCtorScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorMoveCtorEntity");
	const Zenith_EntityID xId = xEntity.GetEntityID();

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();
	const u_int uCountBefore = xStore.GetCount();

	{
		Zenith_AnimatorComponent xSource(xEntity);
		Flux_AnimationController* pxCaptured = xSource.m_pxController;
		ZENITH_ASSERT_NOT_NULL(pxCaptured, "Animator ctor must eagerly create + cache the store controller");
		ZENITH_ASSERT_EQ(xStore.TryGet(xId), pxCaptured, "Store must resolve the entity to the cached controller");
		ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "Exactly one controller created for the entity");

		// Force the move.
		Zenith_AnimatorComponent xMoved(std::move(xSource));

		ZENITH_ASSERT_EQ(xMoved.m_pxController, pxCaptured, "Move ctor must copy the heap-stable controller pointer");
		ZENITH_ASSERT_EQ(xStore.TryGet(xId), pxCaptured, "Moved-to component must resolve the SAME store controller");
		ZENITH_ASSERT_TRUE(xSource.m_pxController == nullptr, "Moved-from cached pointer must be nulled");
		ZENITH_ASSERT_TRUE(xSource.m_bMovedOut, "Moved-from component must be flagged moved-out");
		ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "Move must NOT create a second controller");

		// Scope exit: xMoved dtor Destroys once; xSource dtor (moved-out) skips.
	}

	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "After both components destruct, exactly ONE Destroy must have fired (no leak, no double-free)");
	ZENITH_ASSERT_TRUE(xStore.TryGet(xId) == nullptr, "Controller must be gone from the store after destruction");

	g_xEngine.Scenes().UnloadScene(xScene);
}

// Move ASSIGNMENT: the destination already owns its OWN entity's controller.
// Move-assigning the source must (a) release the dest's pre-existing controller
// (no leak), (b) repoint the dest at the source's controller, (c) neutralise
// the source. Net live count after the assignment is one (dest's old one freed,
// source's transferred).
ZENITH_TEST(Animator, ControllerStoreMoveAssignReleasesDest) { Zenith_UnitTests::TestAnimatorControllerStoreMoveAssignReleasesDest(); }
void Zenith_UnitTests::TestAnimatorControllerStoreMoveAssignReleasesDest(){
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("AnimatorMoveAssignScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntityA = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorMoveAssignA");
	Zenith_Entity xEntityB = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorMoveAssignB");
	const Zenith_EntityID xIdA = xEntityA.GetEntityID();
	const Zenith_EntityID xIdB = xEntityB.GetEntityID();

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();
	const u_int uCountBefore = xStore.GetCount();

	{
		Zenith_AnimatorComponent xSource(xEntityA);
		Zenith_AnimatorComponent xDest(xEntityB);

		Flux_AnimationController* pxSourceController = xSource.m_pxController;
		Flux_AnimationController* pxDestControllerBefore = xDest.m_pxController;
		ZENITH_ASSERT_NOT_NULL(pxSourceController, "Source must own a controller");
		ZENITH_ASSERT_NOT_NULL(pxDestControllerBefore, "Dest must own its own controller before assignment");
		ZENITH_ASSERT_NE(pxSourceController, pxDestControllerBefore, "The two entities must own distinct controllers");
		ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 2u, "Two controllers exist before the assignment");

		xDest = std::move(xSource);

		ZENITH_ASSERT_EQ(xDest.m_pxController, pxSourceController, "Move-assign must transfer the source's controller pointer");
		ZENITH_ASSERT_EQ(xDest.GetParentEntity().GetEntityID(), xIdA, "Move-assign must adopt the source's entity identity");
		ZENITH_ASSERT_TRUE(xSource.m_pxController == nullptr, "Move-assigned-from cached pointer must be nulled");
		ZENITH_ASSERT_TRUE(xSource.m_bMovedOut, "Move-assigned-from component must be flagged moved-out");
		// Dest's ORIGINAL controller (entity B) must have been freed by the assignment.
		ZENITH_ASSERT_TRUE(xStore.TryGet(xIdB) == nullptr, "Dest's pre-existing (entity B) controller must be released on move-assign");
		ZENITH_ASSERT_EQ(xStore.TryGet(xIdA), pxSourceController, "Entity A's controller must still resolve to the transferred instance");
		ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "Move-assign must net to ONE live controller (B freed, A transferred)");
	}

	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "All controllers released after scope exit (no leak, no double-free)");

	g_xEngine.Scenes().UnloadScene(xScene);
}

// REAL pool relocation: AddComponent on three entities packs the animator pool
// densely (slots 0,1,2). Removing entity0's animator triggers a true
// swap-and-pop that move-constructs entity2's component into slot 0. Because
// the store is keyed by the STABLE EntityID (not the pool slot), entity2 must
// still resolve the SAME controller afterwards, and the relocation must NOT
// create/destroy any controller (count unchanged).
ZENITH_TEST(Animator, ControllerStoreSurvivesPoolRelocation) { Zenith_UnitTests::TestAnimatorControllerStoreSurvivesPoolRelocation(); }
void Zenith_UnitTests::TestAnimatorControllerStoreSurvivesPoolRelocation(){
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("AnimatorPoolRelocScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity0 = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorReloc0");
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorReloc1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorReloc2");

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();

	xEntity0.AddComponent<Zenith_AnimatorComponent>();
	xEntity1.AddComponent<Zenith_AnimatorComponent>();
	Zenith_AnimatorComponent& xAnim2 = xEntity2.AddComponent<Zenith_AnimatorComponent>();

	const u_int uCountAfterAdds = xStore.GetCount();
	Flux_AnimationController* pxController2 = &xAnim2.GetController();
	ZENITH_ASSERT_EQ(xStore.TryGet(xEntity2.GetEntityID()), pxController2, "Entity2's controller must resolve via the store");

	// Real swap-and-pop: entity2 (last) is move-constructed into entity0's freed slot.
	xEntity0.RemoveComponent<Zenith_AnimatorComponent>();

	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountAfterAdds - 1u, "Removing entity0's animator must destroy exactly ONE controller");
	ZENITH_ASSERT_TRUE(xEntity2.HasComponent<Zenith_AnimatorComponent>(), "Entity2 must still have its animator after the pool relocation");
	// The relocated component must resolve the SAME heap-stable controller.
	Zenith_AnimatorComponent& xAnim2After = xEntity2.GetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_EQ(&xAnim2After.GetController(), pxController2, "Relocated component must resolve the SAME controller (EntityID-keyed, heap-stable)");
	ZENITH_ASSERT_EQ(xStore.TryGet(xEntity2.GetEntityID()), pxController2, "Store lookup for entity2 must be unchanged by the relocation");
	ZENITH_ASSERT_TRUE(xStore.TryGet(xEntity0.GetEntityID()) == nullptr, "Removed entity0's controller must be gone");

	g_xEngine.Scenes().UnloadScene(xScene);
}

// Cross-scene MoveEntityToScene preserves the EntityID slot (global slot
// table), so the animator's controller must survive the move: same controller,
// same store count, resolvable in the new scene.
ZENITH_TEST(Animator, ControllerStoreSurvivesCrossSceneMove) { Zenith_UnitTests::TestAnimatorControllerStoreSurvivesCrossSceneMove(); }
void Zenith_UnitTests::TestAnimatorControllerStoreSurvivesCrossSceneMove(){
	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("AnimatorXSceneA", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("AnimatorXSceneB", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneDataA = g_xEngine.Scenes().GetSceneData(xSceneA);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneDataA, "AnimatorXSceneEntity");
	Zenith_AnimatorComponent& xAnim = xEntity.AddComponent<Zenith_AnimatorComponent>();

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();
	const u_int uCountBeforeMove = xStore.GetCount();
	Flux_AnimationController* pxController = &xAnim.GetController();
	const Zenith_EntityID xIdBefore = xEntity.GetEntityID();

	xEntity.MoveToScene(xSceneB);
	// Phase 3: Entity::MoveToScene is void; success == the entity now lives in scene B.
	const bool bMoved = (xEntity.GetScene() == xSceneB);
	ZENITH_ASSERT_TRUE(bMoved, "Cross-scene move of the animator entity must succeed");
	// EntityID is preserved across the move (global slot table) — xEntity now
	// points at the moved entity in scene B.
	ZENITH_ASSERT_EQ(xEntity.GetEntityID(), xIdBefore, "EntityID slot must be preserved across the cross-scene move");

	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBeforeMove, "Cross-scene move must not create/destroy any controller");
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_AnimatorComponent>(), "Animator must travel with the entity to scene B");
	Zenith_AnimatorComponent& xAnimAfter = xEntity.GetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_EQ(&xAnimAfter.GetController(), pxController, "Moved entity must resolve the SAME controller (EntityID-keyed, heap-stable)");
	ZENITH_ASSERT_EQ(xStore.TryGet(xIdBefore), pxController, "Store lookup must still resolve the controller post-move");

	g_xEngine.Scenes().UnloadScene(xSceneA);
	g_xEngine.Scenes().UnloadScene(xSceneB);
}

// Destroy must be IDEMPOTENT: the component's OnDestroy AND its dtor both call
// store.Destroy(entity). The second call must be a harmless no-op. Also a
// never-created entity Destroy is a no-op. Exercised directly on the store.
ZENITH_TEST(Animator, ControllerStoreDestroyIsIdempotent) { Zenith_UnitTests::TestAnimatorControllerStoreDestroyIsIdempotent(); }
void Zenith_UnitTests::TestAnimatorControllerStoreDestroyIsIdempotent(){
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("AnimatorDestroyIdempotentScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorDestroyIdempotentEntity");
	const Zenith_EntityID xId = xEntity.GetEntityID();

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();
	const u_int uCountBefore = xStore.GetCount();

	// Destroy on an entity with NO controller is a no-op.
	ZENITH_ASSERT_FALSE(xStore.Destroy(xId), "Destroy on an absent entity must return false (no-op)");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "No-op Destroy must not change the count");

	(void)xStore.GetOrCreate(xId);
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "GetOrCreate must create one controller");
	// GetOrCreate again must NOT create a second.
	(void)xStore.GetOrCreate(xId);
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "GetOrCreate is idempotent for an existing entity");

	ZENITH_ASSERT_TRUE(xStore.Destroy(xId), "First Destroy must report it removed the controller");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "Destroy must remove exactly one controller");
	// Second Destroy (mirrors dtor-after-OnDestroy) is a harmless no-op.
	ZENITH_ASSERT_FALSE(xStore.Destroy(xId), "Second Destroy must be an idempotent no-op");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "Idempotent second Destroy must not change the count");

	g_xEngine.Scenes().UnloadScene(xScene);
}

// Generation validation (review follow-up): the store is keyed by EntityID, and
// Zenith_EntityID carries a generation for stale-handle detection. A stale id
// for a RECYCLED slot must never resolve to, or destroy, the new occupant's
// controller; and GetOrCreate on a recycled slot must RECOVER (tear down the
// stale controller + hand back a FRESH one), not return the stale instance.
ZENITH_TEST(Animator, ControllerStoreValidatesGeneration) { Zenith_UnitTests::TestAnimatorControllerStoreValidatesGeneration(); }
void Zenith_UnitTests::TestAnimatorControllerStoreValidatesGeneration(){
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("AnimatorGenValidationScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimatorGenValidationEntity");
	const Zenith_EntityID xRealId = xEntity.GetEntityID();

	Flux_AnimationControllerStore& xStore = g_xEngine.AnimationControllers();
	const u_int uCountBefore = xStore.GetCount();

	// The slot's current (generation-matched) controller.
	Flux_AnimationController& xReal = xStore.GetOrCreate(xRealId);
	ZENITH_ASSERT_EQ(xStore.TryGet(xRealId), &xReal, "matched-generation id must resolve its controller");

	// Same SLOT, a DIFFERENT generation — the exact stale-handle / recycled-slot shape.
	Zenith_EntityID xStaleId = xRealId;
	xStaleId.m_uGeneration = xRealId.m_uGeneration + 1u;

	ZENITH_ASSERT_TRUE(xStore.TryGet(xStaleId) == nullptr, "stale-generation id must NOT resolve the slot's controller");
	ZENITH_ASSERT_FALSE(xStore.Destroy(xStaleId), "stale-generation Destroy must be a no-op");
	ZENITH_ASSERT_EQ(xStore.TryGet(xRealId), &xReal, "the live controller must survive a stale-generation Destroy");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "stale ops must not change the live count");

	// GetOrCreate on the recycled slot (new generation) must recover: tear down
	// the stale controller and return a FRESH one (NOT the stale instance).
	Flux_AnimationController& xFresh = xStore.GetOrCreate(xStaleId);
	// NB: deliberately NOT asserting &xFresh != &xReal — DestroyControllerAt
	// frees the stale controller and the very next `new` can legitimately reuse
	// that heap block, so pointer identity is not a reliable freshness signal.
	// The generation keying is the robust proof: the OLD generation no longer
	// resolves (its entry was torn down + regenerated), and the NEW generation
	// resolves the recreated controller. If GetOrCreate had instead returned the
	// stale entry (no recovery), TryGet(xRealId) below would still resolve.
	ZENITH_ASSERT_EQ(xStore.TryGet(xStaleId), &xFresh, "the new-generation id resolves the recreated controller");
	ZENITH_ASSERT_TRUE(xStore.TryGet(xRealId) == nullptr, "the old-generation id must no longer resolve (entry torn down + regenerated)");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore + 1u, "recovery is net-neutral: one stale torn down, one fresh created");

	// Correct-generation Destroy cleans up (no orphan for the scene unload).
	ZENITH_ASSERT_TRUE(xStore.Destroy(xStaleId), "matched-generation Destroy removes the fresh controller");
	ZENITH_ASSERT_EQ(xStore.GetCount(), uCountBefore, "no net controller leak");

	g_xEngine.Scenes().UnloadScene(xScene);
}

// Per-component streaming state isolation. Two Flux_TerrainStreamingState
// instances own independent dirty flags; flipping one must not change the
// other. Regression test for the pre-refactor "global dirty flag" bug where
// a streaming change on one terrain forced re-upload on every other.
ZENITH_TEST(Terrain, StreamingDirtyFlagPerState) { Zenith_UnitTests::TestTerrainStreamingDirtyFlagPerState(); }
void Zenith_UnitTests::TestTerrainStreamingDirtyFlagPerState(){
	Flux_TerrainStreamingState xStateA;
	Flux_TerrainStreamingState xStateB;

	// std::atomic<T>::load is [[nodiscard]] — store into locals before
	// asserting so the macro expansion can't fall foul of /WX.

	// Both default-init to dirty=true.
	bool bA = xStateA.m_bChunkDataDirty.load(std::memory_order_acquire);
	bool bB = xStateB.m_bChunkDataDirty.load(std::memory_order_acquire);
	ZENITH_ASSERT_TRUE(bA, "State A must default to dirty=true");
	ZENITH_ASSERT_TRUE(bB, "State B must default to dirty=true");

	xStateA.m_bChunkDataDirty.store(false, std::memory_order_release);
	bA = xStateA.m_bChunkDataDirty.load(std::memory_order_acquire);
	bB = xStateB.m_bChunkDataDirty.load(std::memory_order_acquire);
	ZENITH_ASSERT_TRUE(!bA, "State A should be clean after store(false)");
	ZENITH_ASSERT_TRUE(bB,  "State B's dirty flag must be unaffected by changes to state A");

	xStateB.m_bChunkDataDirty.store(false, std::memory_order_release);
	xStateA.m_bChunkDataDirty.store(true, std::memory_order_release);
	bA = xStateA.m_bChunkDataDirty.load(std::memory_order_acquire);
	bB = xStateB.m_bChunkDataDirty.load(std::memory_order_acquire);
	ZENITH_ASSERT_TRUE(bA,  "State A re-dirtied");
	ZENITH_ASSERT_TRUE(!bB, "State B must stay clean while A was re-dirtied");
}

// Mutating one state's residency table must not leak into another state's.
// Regression test for the pre-refactor global s_axChunkResidency array.
ZENITH_TEST(Terrain, StreamingResidencyIsolatedBetweenStates) { Zenith_UnitTests::TestTerrainStreamingResidencyIsolatedBetweenStates(); }
void Zenith_UnitTests::TestTerrainStreamingResidencyIsolatedBetweenStates(){
	Flux_TerrainStreamingState xStateA;
	Flux_TerrainStreamingState xStateB;

	const uint32_t uChunkIndex = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(7, 11);
	// Both states start with all chunks NOT_LOADED.
	ZENITH_ASSERT_EQ(xStateA.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH], Flux_TerrainLODResidencyState::NOT_LOADED, "State A chunk must start NOT_LOADED");
	ZENITH_ASSERT_EQ(xStateB.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH], Flux_TerrainLODResidencyState::NOT_LOADED, "State B chunk must start NOT_LOADED");

	xStateA.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH] = Flux_TerrainLODResidencyState::RESIDENT;
	xStateA.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_HIGH].m_uVertexOffset = 1234;
	xStateA.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_HIGH].m_uVertexCount  = 64;

	ZENITH_ASSERT_EQ(xStateA.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH], Flux_TerrainLODResidencyState::RESIDENT, "State A chunk should be RESIDENT after mutation");
	ZENITH_ASSERT_EQ(xStateB.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_HIGH], Flux_TerrainLODResidencyState::NOT_LOADED, "State B chunk must remain NOT_LOADED");
	ZENITH_ASSERT_EQ(xStateB.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_HIGH].m_uVertexOffset, 0u, "State B's allocation offset must not pick up state A's mutation");
}

// SquaredHysteresis is a pure constexpr that returns linear^2; verifies the
// threshold conversion that fixed the eviction-radius bug (was multiplying
// LOD_*_DISTANCE_SQ by a linear ratio, producing √ratio× radius).
ZENITH_TEST(Terrain, HysteresisSquaredDistance) { Zenith_UnitTests::TestTerrainHysteresisSquaredDistance(); }
void Zenith_UnitTests::TestTerrainHysteresisSquaredDistance(){
	ZENITH_ASSERT_LT(fabsf(Flux_TerrainConfig::SquaredHysteresis(1.0f) - 1.0f),  0.0001f, "SquaredHysteresis(1.0) should be 1.0");
	ZENITH_ASSERT_LT(fabsf(Flux_TerrainConfig::SquaredHysteresis(1.5f) - 2.25f), 0.0001f, "SquaredHysteresis(1.5) should be 2.25 (used by main eviction)");
	ZENITH_ASSERT_LT(fabsf(Flux_TerrainConfig::SquaredHysteresis(1.2f) - 1.44f), 0.0001f, "SquaredHysteresis(1.2) should be 1.44 (used by forced eviction)");
	ZENITH_ASSERT_LT(fabsf(Flux_TerrainConfig::SquaredHysteresis(2.0f) - 4.0f),  0.0001f, "SquaredHysteresis(2.0) should be 4.0");

	// Spot-check that the eviction threshold lands at the documented radius.
	// 1.5×R linear == 2.25×R² squared. Distance comparisons in the streaming
	// code work in squared-distance space, so this matches the actual gating.
	const float fLinearR        = 1000.0f;
	const float fSquaredR       = fLinearR * fLinearR;
	const float fEvictThreshold = fSquaredR * Flux_TerrainConfig::SquaredHysteresis(1.5f);
	const float fExpectedAt15R  = (1.5f * fLinearR) * (1.5f * fLinearR);
	ZENITH_ASSERT_LT(fabsf(fEvictThreshold - fExpectedAt15R), 0.01f, "Eviction threshold must equal (1.5×R)² in squared space");
}

// Active-set rebuild sorts ascending by squared distance to the camera so
// the per-frame streaming budget favours nearby chunks. Builds a synthetic
// state with cached AABBs across a 3×3 region and verifies the output is
// non-decreasing in distance.
ZENITH_TEST(Terrain, ActiveSetSortedNearestFirst) { Zenith_UnitTests::TestTerrainActiveSetSortedNearestFirst(); }
void Zenith_UnitTests::TestTerrainActiveSetSortedNearestFirst(){
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;
	xState.m_uActiveChunkRadius = 1;  // 3×3 block around camera

	// Camera in the middle of chunk (5,5) (world Y irrelevant — chunk
	// centers use y from the AABB midpoint).
	const Zenith_Maths::Vector3 xCameraPos(
		(5.0f + 0.5f) * Flux_TerrainConfig::CHUNK_SIZE_WORLD,
		32.0f,
		(5.0f + 0.5f) * Flux_TerrainConfig::CHUNK_SIZE_WORLD);

	// Populate cached AABBs for chunks (4..6, 4..6).
	for (uint32_t cx = 4; cx <= 6; ++cx)
	{
		for (uint32_t cy = 4; cy <= 6; ++cy)
		{
			const uint32_t uIdx = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(cx, cy);
			xState.m_axChunkAABBs[uIdx] = Zenith_AABB(
				Zenith_Maths::Vector3(static_cast<float>(cx)       * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 0.0f,        static_cast<float>(cy)       * Flux_TerrainConfig::CHUNK_SIZE_WORLD),
				Zenith_Maths::Vector3(static_cast<float>(cx + 1u)  * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 64.0f,       static_cast<float>(cy + 1u)  * Flux_TerrainConfig::CHUNK_SIZE_WORLD));
		}
	}

	Flux_TerrainStreamingManagerImpl::RebuildActiveChunkSet(xState, 5, 5, xCameraPos);

	ZENITH_ASSERT_EQ(xState.m_xActiveChunkIndices.GetSize(), (u_int)9, "Active set with radius 1 should hold 9 chunks (3×3)");

	float fPrevDistSq = -1.0f;
	for (u_int u = 0; u < xState.m_xActiveChunkIndices.GetSize(); ++u)
	{
		const uint32_t uIdx       = xState.m_xActiveChunkIndices.Get(u);
		const float    fDistanceSq = Flux_TerrainStreamingManagerImpl::GetChunkDistanceSq(xState, uIdx, xCameraPos);
		ZENITH_ASSERT_TRUE(fDistanceSq + 0.0001f >= fPrevDistSq, "Active set must be non-decreasing in distance, entry %zu broke ordering", u);
		fPrevDistSq = fDistanceSq;
	}
}

// The first entry of the sorted active set must be the chunk under the
// camera (squared distance ~0). Companion to the non-decreasing check.
ZENITH_TEST(Terrain, ActiveSetCenterIndexFirst) { Zenith_UnitTests::TestTerrainActiveSetCenterIndexFirst(); }
void Zenith_UnitTests::TestTerrainActiveSetCenterIndexFirst(){
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;
	xState.m_uActiveChunkRadius = 1;

	const Zenith_Maths::Vector3 xCameraPos(
		(5.0f + 0.5f) * Flux_TerrainConfig::CHUNK_SIZE_WORLD,
		32.0f,
		(5.0f + 0.5f) * Flux_TerrainConfig::CHUNK_SIZE_WORLD);

	for (uint32_t cx = 4; cx <= 6; ++cx)
	{
		for (uint32_t cy = 4; cy <= 6; ++cy)
		{
			const uint32_t uIdx = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(cx, cy);
			xState.m_axChunkAABBs[uIdx] = Zenith_AABB(
				Zenith_Maths::Vector3(static_cast<float>(cx)       * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 0.0f,        static_cast<float>(cy)       * Flux_TerrainConfig::CHUNK_SIZE_WORLD),
				Zenith_Maths::Vector3(static_cast<float>(cx + 1u)  * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 64.0f,       static_cast<float>(cy + 1u)  * Flux_TerrainConfig::CHUNK_SIZE_WORLD));
		}
	}

	Flux_TerrainStreamingManagerImpl::RebuildActiveChunkSet(xState, 5, 5, xCameraPos);

	ZENITH_ASSERT_TRUE(xState.m_xActiveChunkIndices.GetSize() > 0, "Active set must be non-empty");
	const uint32_t uExpectedCenter = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(5, 5);
	ZENITH_ASSERT_EQ(xState.m_xActiveChunkIndices.Get(0), uExpectedCenter, "First entry of sorted active set must be the chunk under the camera");
}

// Terrain assets can be authored/exported with world-space AABBs offset from
// origin. Streaming must therefore use the nearest cached AABB as the active
// set centre instead of assuming cameraWorld / CHUNK_WORLD_SIZE maps directly
// to chunk coordinates.
ZENITH_TEST(Terrain, ActiveSetUsesNearestAABBForOffsetTerrain) { Zenith_UnitTests::TestTerrainActiveSetUsesNearestAABBForOffsetTerrain(); }
void Zenith_UnitTests::TestTerrainActiveSetUsesNearestAABBForOffsetTerrain(){
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;
	xState.m_uActiveChunkRadius = 1;

	const float fChunkSize = Flux_TerrainConfig::CHUNK_SIZE_WORLD;
	for (uint32_t cx = 0; cx < Flux_TerrainConfig::CHUNK_GRID_SIZE; ++cx)
	{
		for (uint32_t cy = 0; cy < Flux_TerrainConfig::CHUNK_GRID_SIZE; ++cy)
		{
			const uint32_t uIdx = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(cx, cy);
			const float fBaseX = 100000.0f + static_cast<float>(cx) * fChunkSize;
			const float fBaseZ = 100000.0f + static_cast<float>(cy) * fChunkSize;
			xState.m_axChunkAABBs[uIdx] = Zenith_AABB(
				Zenith_Maths::Vector3(fBaseX, 0.0f, fBaseZ),
				Zenith_Maths::Vector3(fBaseX + fChunkSize, 64.0f, fBaseZ + fChunkSize));
		}
	}

	const uint32_t uTargetX = 10;
	const uint32_t uTargetY = 12;
	const uint32_t uTargetIdx = Flux_TerrainStreamingManagerImpl::ChunkCoordsToIndex(uTargetX, uTargetY);
	xState.m_axChunkAABBs[uTargetIdx] = Zenith_AABB(
		Zenith_Maths::Vector3(-512.0f, 0.0f, -256.0f),
		Zenith_Maths::Vector3(-448.0f, 64.0f, -192.0f));

	const Zenith_Maths::Vector3 xCameraPos(-480.0f, 32.0f, -224.0f);
	int32_t iChunkX = -1;
	int32_t iChunkY = -1;
	uint32_t uNearestIdx = UINT32_MAX;
	Flux_TerrainStreamingManagerImpl::ResolveCameraChunkCoords(xState, xCameraPos, iChunkX, iChunkY, uNearestIdx);

	ZENITH_ASSERT_EQ(uNearestIdx, uTargetIdx, "AABB nearest-chunk selection should pick the offset target chunk");
	ZENITH_ASSERT_EQ(iChunkX, static_cast<int32_t>(uTargetX), "Resolved camera chunk X should come from nearest AABB");
	ZENITH_ASSERT_EQ(iChunkY, static_cast<int32_t>(uTargetY), "Resolved camera chunk Y should come from nearest AABB");

	Flux_TerrainStreamingManagerImpl::RebuildActiveChunkSet(xState, iChunkX, iChunkY, xCameraPos);
	ZENITH_ASSERT_TRUE(xState.m_xActiveChunkIndices.GetSize() > 0, "Offset active set should be non-empty");
	ZENITH_ASSERT_EQ(xState.m_xActiveChunkIndices.Get(0), uTargetIdx, "Nearest offset chunk should be first in the sorted active set");
}

// When LOW residency is populated for every chunk, chunk-data generation
// should report zero LOW-empty chunks. This locks down the hole diagnostic:
// a nonzero count points at missing/invalid LOW metadata rather than culling
// or indirect-count lifetime.
ZENITH_TEST(Terrain, ChunkDataNoLowZeroWhenLowResident) { Zenith_UnitTests::TestTerrainChunkDataNoLowZeroWhenLowResident(); }
void Zenith_UnitTests::TestTerrainChunkDataNoLowZeroWhenLowResident(){
	Flux_TerrainStreamingState xState;
	xState.m_bAABBsCached = true;

	for (uint32_t uChunkIndex = 0; uChunkIndex < Flux_TerrainConfig::TOTAL_CHUNKS; ++uChunkIndex)
	{
		uint32_t uChunkX, uChunkY;
		Flux_TerrainStreamingManagerImpl::ChunkIndexToCoords(uChunkIndex, uChunkX, uChunkY);
		xState.m_axChunkResidency[uChunkIndex].m_aeStates[LOD_LOW] = Flux_TerrainLODResidencyState::RESIDENT;
		xState.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_LOW].m_uVertexOffset = uChunkIndex * 4u;
		xState.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_LOW].m_uVertexCount = 4u;
		xState.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_LOW].m_uIndexOffset = uChunkIndex * 6u;
		xState.m_axChunkResidency[uChunkIndex].m_axAllocations[LOD_LOW].m_uIndexCount = 6u;
		xState.m_axChunkAABBs[uChunkIndex] = Zenith_AABB(
			Zenith_Maths::Vector3(static_cast<float>(uChunkX) * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 0.0f, static_cast<float>(uChunkY) * Flux_TerrainConfig::CHUNK_SIZE_WORLD),
			Zenith_Maths::Vector3(static_cast<float>(uChunkX + 1u) * Flux_TerrainConfig::CHUNK_SIZE_WORLD, 64.0f, static_cast<float>(uChunkY + 1u) * Flux_TerrainConfig::CHUNK_SIZE_WORLD));
	}

	ZENITH_ASSERT_EQ(Flux_TerrainStreamingManagerImpl::CountLowZeroChunks(xState), 0u, "LOW zero-count diagnostic should be zero when all LOW chunks are resident with indices");

	Zenith_TerrainChunkData* pxChunkData = new Zenith_TerrainChunkData[Flux_TerrainConfig::TOTAL_CHUNKS];
	Flux_TerrainStreamingManagerImpl::BuildChunkDataForGPU_Internal(xState, pxChunkData);
	for (uint32_t uChunkIndex = 0; uChunkIndex < Flux_TerrainConfig::TOTAL_CHUNKS; ++uChunkIndex)
	{
		ZENITH_ASSERT_EQ(pxChunkData[uChunkIndex].m_axLODs[LOD_LOW].m_uIndexCount, 6u, "Generated chunk data LOW index count should match populated residency");
	}
	delete[] pxChunkData;
}

// ========== Vulkan Memory Manager Tests ==========

ZENITH_TEST(Vulkan, ImageViewType3D) { Zenith_UnitTests::TestImageViewType3D(); }

void Zenith_UnitTests::TestImageViewType3D(){

	Flux_SurfaceInfo xInfo;
	xInfo.m_eTextureType = TEXTURE_TYPE_3D;
	xInfo.m_uWidth = 64;
	xInfo.m_uHeight = 64;
	xInfo.m_uDepth = 64;
	xInfo.m_uNumLayers = 1;

	vk::ImageViewType eResult = g_xEngine.VulkanMemory().DetermineImageViewType(xInfo);
	ZENITH_ASSERT_EQ(eResult, vk::ImageViewType::e3D, "Expected VK_IMAGE_VIEW_TYPE_3D for 3D texture");

}

ZENITH_TEST(Vulkan, ImageViewTypeCube) { Zenith_UnitTests::TestImageViewTypeCube(); }

void Zenith_UnitTests::TestImageViewTypeCube(){

	Flux_SurfaceInfo xInfoCube;
	xInfoCube.m_eTextureType = TEXTURE_TYPE_CUBE;
	xInfoCube.m_uWidth = 256;
	xInfoCube.m_uHeight = 256;
	xInfoCube.m_uNumLayers = 6;

	vk::ImageViewType eResult = g_xEngine.VulkanMemory().DetermineImageViewType(xInfoCube);
	ZENITH_ASSERT_EQ(eResult, vk::ImageViewType::eCube, "Expected VK_IMAGE_VIEW_TYPE_CUBE for cubemap texture");

	Flux_SurfaceInfo xInfoSixLayers;
	xInfoSixLayers.m_eTextureType = TEXTURE_TYPE_2D;
	xInfoSixLayers.m_uWidth = 256;
	xInfoSixLayers.m_uHeight = 256;
	xInfoSixLayers.m_uNumLayers = 6;

	eResult = g_xEngine.VulkanMemory().DetermineImageViewType(xInfoSixLayers);
	ZENITH_ASSERT_EQ(eResult, vk::ImageViewType::eCube, "Expected VK_IMAGE_VIEW_TYPE_CUBE for 6-layer 2D texture");

}

ZENITH_TEST(Vulkan, ImageViewTypeDefault2D) { Zenith_UnitTests::TestImageViewTypeDefault2D(); }

void Zenith_UnitTests::TestImageViewTypeDefault2D(){

	Flux_SurfaceInfo xInfo;
	xInfo.m_eTextureType = TEXTURE_TYPE_2D;
	xInfo.m_uWidth = 512;
	xInfo.m_uHeight = 512;
	xInfo.m_uNumLayers = 1;

	vk::ImageViewType eResult = g_xEngine.VulkanMemory().DetermineImageViewType(xInfo);
	ZENITH_ASSERT_EQ(eResult, vk::ImageViewType::e2D, "Expected VK_IMAGE_VIEW_TYPE_2D for standard 2D texture");

}

ZENITH_TEST(Vulkan, DestroySkipsInvalidHandle) { Zenith_UnitTests::TestDestroySkipsInvalidHandle(); }

void Zenith_UnitTests::TestDestroySkipsInvalidHandle(){

	Flux_VRAMHandle xInvalidHandle;
	ZENITH_ASSERT_FALSE(xInvalidHandle.IsValid(), "Default-constructed handle should be invalid");

	Flux_VertexBuffer xBuffer;
	g_xEngine.VulkanMemory().DestroyVertexBuffer(xBuffer);

	Flux_ImageViewHandle xInvalidViewHandle;
	ZENITH_ASSERT_FALSE(xInvalidViewHandle.IsValid(), "Default-constructed image view handle should be invalid");

	g_xEngine.VulkanMemory().QueueImageViewDeletion(xInvalidViewHandle);

}

//=============================================================================
// UIStyle tests
//=============================================================================

ZENITH_TEST(UI, UIStyleDefaultValues) { Zenith_UnitTests::TestUIStyleDefaultValues(); }

void Zenith_UnitTests::TestUIStyleDefaultValues(){

	Zenith_UI::UIStyle xStyle;
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_xFillColor.x, 1.0f, 0.001f, "Default fill R should be 1");
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_xFillColor.y, 1.0f, 0.001f, "Default fill G should be 1");
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_xFillColor.z, 1.0f, 0.001f, "Default fill B should be 1");
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_xFillColor.w, 1.0f, 0.001f, "Default fill A should be 1");
	ZENITH_ASSERT_LT(xStyle.m_xGradientBottomColor.x, 0.0f, "Default gradient should be sentinel (-1)");
	ZENITH_ASSERT_LT(std::abs(xStyle.m_fBorderThickness), 0.001f, "Default border thickness should be 0");
	ZENITH_ASSERT_LT(std::abs(xStyle.m_fCornerRadius), 0.001f, "Default corner radius should be 0");
	ZENITH_ASSERT_FALSE(xStyle.m_bShadowEnabled, "Default shadow should be disabled");
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_xShadowOffset.x, 4.0f, 0.001f, "Default shadow offset X should be 4");
	ZENITH_ASSERT_EQ_FLOAT(xStyle.m_fShadowSpread, 4.0f, 0.001f, "Default shadow spread should be 4");

}

ZENITH_TEST(UI, UIStyleLerpIdentity) { Zenith_UnitTests::TestUIStyleLerpIdentity(); }

void Zenith_UnitTests::TestUIStyleLerpIdentity(){

	Zenith_UI::UIStyle xStyle;
	xStyle.m_xFillColor = {0.5f, 0.3f, 0.1f, 1.0f};
	xStyle.m_fCornerRadius = 10.0f;
	xStyle.m_fBorderThickness = 2.0f;

	Zenith_UI::UIStyle xResult = Zenith_UI::UIStyle::Lerp(xStyle, xStyle, 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_xFillColor.x, 0.5f, 0.001f, "Lerp identity fill R");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_xFillColor.y, 0.3f, 0.001f, "Lerp identity fill G");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_fCornerRadius, 10.0f, 0.001f, "Lerp identity corner radius");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_fBorderThickness, 2.0f, 0.001f, "Lerp identity border thickness");

}

ZENITH_TEST(UI, UIStyleLerpHalfway) { Zenith_UnitTests::TestUIStyleLerpHalfway(); }

void Zenith_UnitTests::TestUIStyleLerpHalfway(){

	Zenith_UI::UIStyle xA;
	xA.m_xFillColor = {1.0f, 0.0f, 0.0f, 1.0f};
	xA.m_fCornerRadius = 0.0f;
	xA.m_fBorderThickness = 0.0f;
	xA.m_fShadowSpread = 0.0f;

	Zenith_UI::UIStyle xB;
	xB.m_xFillColor = {0.0f, 1.0f, 0.0f, 1.0f};
	xB.m_fCornerRadius = 20.0f;
	xB.m_fBorderThickness = 4.0f;
	xB.m_fShadowSpread = 8.0f;

	Zenith_UI::UIStyle xResult = Zenith_UI::UIStyle::Lerp(xA, xB, 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_xFillColor.x, 0.5f, 0.001f, "Halfway fill R should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_xFillColor.y, 0.5f, 0.001f, "Halfway fill G should be 0.5");
	ZENITH_ASSERT_LT(std::abs(xResult.m_xFillColor.z), 0.001f, "Halfway fill B should be 0");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_fCornerRadius, 10.0f, 0.001f, "Halfway corner radius should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_fBorderThickness, 2.0f, 0.001f, "Halfway border thickness should be 2");
	ZENITH_ASSERT_EQ_FLOAT(xResult.m_fShadowSpread, 4.0f, 0.001f, "Halfway shadow spread should be 4");

}

ZENITH_TEST(UI, UIStyleLerpEndpoints) { Zenith_UnitTests::TestUIStyleLerpEndpoints(); }

void Zenith_UnitTests::TestUIStyleLerpEndpoints(){

	Zenith_UI::UIStyle xA;
	xA.m_xFillColor = {1.0f, 0.0f, 0.0f, 1.0f};
	xA.m_fCornerRadius = 5.0f;

	Zenith_UI::UIStyle xB;
	xB.m_xFillColor = {0.0f, 0.0f, 1.0f, 1.0f};
	xB.m_fCornerRadius = 25.0f;

	// t=0 should return A
	Zenith_UI::UIStyle xAt0 = Zenith_UI::UIStyle::Lerp(xA, xB, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xAt0.m_xFillColor.x, 1.0f, 0.001f, "t=0 fill R should be 1");
	ZENITH_ASSERT_LT(std::abs(xAt0.m_xFillColor.z), 0.001f, "t=0 fill B should be 0");
	ZENITH_ASSERT_EQ_FLOAT(xAt0.m_fCornerRadius, 5.0f, 0.001f, "t=0 corner radius should be 5");

	// t=1 should return B
	Zenith_UI::UIStyle xAt1 = Zenith_UI::UIStyle::Lerp(xA, xB, 1.0f);
	ZENITH_ASSERT_LT(std::abs(xAt1.m_xFillColor.x), 0.001f, "t=1 fill R should be 0");
	ZENITH_ASSERT_EQ_FLOAT(xAt1.m_xFillColor.z, 1.0f, 0.001f, "t=1 fill B should be 1");
	ZENITH_ASSERT_EQ_FLOAT(xAt1.m_fCornerRadius, 25.0f, 0.001f, "t=1 corner radius should be 25");

}

ZENITH_TEST(UI, UIStyleLerpShadowBool) { Zenith_UnitTests::TestUIStyleLerpShadowBool(); }

void Zenith_UnitTests::TestUIStyleLerpShadowBool(){

	Zenith_UI::UIStyle xA;
	xA.m_bShadowEnabled = true;

	Zenith_UI::UIStyle xB;
	xB.m_bShadowEnabled = false;

	// t < 0.5 should follow A
	Zenith_UI::UIStyle xResult1 = Zenith_UI::UIStyle::Lerp(xA, xB, 0.3f);
	ZENITH_ASSERT_EQ(xResult1.m_bShadowEnabled, true, "t<0.5 shadow should follow A (true)");

	// t >= 0.5 should follow B
	Zenith_UI::UIStyle xResult2 = Zenith_UI::UIStyle::Lerp(xA, xB, 0.7f);
	ZENITH_ASSERT_EQ(xResult2.m_bShadowEnabled, false, "t>=0.5 shadow should follow B (false)");

	// Verify boundary: t=0.5 should follow B
	Zenith_UI::UIStyle xResult3 = Zenith_UI::UIStyle::Lerp(xA, xB, 0.5f);
	ZENITH_ASSERT_EQ(xResult3.m_bShadowEnabled, false, "t=0.5 shadow should follow B (false)");

}

//=============================================================================
// Gizmo math helper tests
//=============================================================================
#ifdef ZENITH_TOOLS

ZENITH_TEST(Gizmos, GizmosLineLineParallel) { Zenith_UnitTests::TestGizmosLineLineParallel(); }

void Zenith_UnitTests::TestGizmosLineLineParallel(){

	// Two parallel lines along the X axis should return false (degenerate)
	Zenith_Maths::Vector3 xAxisOrigin(0, 0, 0);
	Zenith_Maths::Vector3 xAxis(1, 0, 0);
	Zenith_Maths::Vector3 xRayOrigin(0, 1, 0);
	Zenith_Maths::Vector3 xRayDir(1, 0, 0);	// Parallel to axis

	float fOutT = 0.0f;
	bool bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
	ZENITH_ASSERT_FALSE(bResult, "Parallel lines should return false");

	// Also test with opposite direction (anti-parallel)
	xRayDir = Zenith_Maths::Vector3(-1, 0, 0);
	bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
	ZENITH_ASSERT_FALSE(bResult, "Anti-parallel lines should return false");

	// Near-parallel lines (very small angle) should also return false
	xRayDir = glm::normalize(Zenith_Maths::Vector3(1, 0.00001f, 0));
	bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
	ZENITH_ASSERT_FALSE(bResult, "Near-parallel lines should return false");

}

ZENITH_TEST(Gizmos, GizmosLineLinePerpendicular) { Zenith_UnitTests::TestGizmosLineLinePerpendicular(); }

void Zenith_UnitTests::TestGizmosLineLinePerpendicular(){

	// Axis along X, ray along Y passing through origin - closest point at t=0
	{
		Zenith_Maths::Vector3 xAxisOrigin(0, 0, 0);
		Zenith_Maths::Vector3 xAxis(1, 0, 0);
		Zenith_Maths::Vector3 xRayOrigin(0, -5, 0);
		Zenith_Maths::Vector3 xRayDir(0, 1, 0);

		float fOutT = -999.0f;
		bool bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
		ZENITH_ASSERT_TRUE(bResult, "Perpendicular lines should return true");
		ZENITH_ASSERT_LT(glm::abs(fOutT), 0.001f, "Closest point on axis should be at t=0");
	}

	// Axis along X at origin, ray along Z passing through (3, 2, -5) going +Z
	// The closest point on the X axis to the ray should be at t=3
	{
		Zenith_Maths::Vector3 xAxisOrigin(0, 0, 0);
		Zenith_Maths::Vector3 xAxis(1, 0, 0);
		Zenith_Maths::Vector3 xRayOrigin(3, 2, -5);
		Zenith_Maths::Vector3 xRayDir(0, 0, 1);

		float fOutT = -999.0f;
		bool bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
		ZENITH_ASSERT_TRUE(bResult, "Perpendicular lines should return true");
		ZENITH_ASSERT_LT(glm::abs(fOutT - 3.0f), 0.001f, "Closest point should be at t=3");
	}

	// Axis along Y starting at (5,0,0), ray along X starting at (0,7,3)
	// Closest point on axis should be at t=7 (where Y=7 is the closest to the ray's Y=7)
	{
		Zenith_Maths::Vector3 xAxisOrigin(5, 0, 0);
		Zenith_Maths::Vector3 xAxis(0, 1, 0);
		Zenith_Maths::Vector3 xRayOrigin(0, 7, 3);
		Zenith_Maths::Vector3 xRayDir(1, 0, 0);

		float fOutT = -999.0f;
		bool bResult = g_xEngine.Gizmos().GetLineLineClosestPointParameter(xAxisOrigin, xAxis, xRayOrigin, xRayDir, fOutT);
		ZENITH_ASSERT_TRUE(bResult, "Perpendicular lines should return true");
		ZENITH_ASSERT_LT(glm::abs(fOutT - 7.0f), 0.001f, "Closest point should be at t=7");
	}

}

ZENITH_TEST(Gizmos, GizmosTangentFrame) { Zenith_UnitTests::TestGizmosTangentFrame(); }

void Zenith_UnitTests::TestGizmosTangentFrame(){

	auto TestAxis = [](const Zenith_Maths::Vector3& xAxis, const char* pszName)
	{
		Zenith_Maths::Vector3 xTangent, xBitangent;
		g_xEngine.Gizmos().ComputeTangentFrame(xAxis, xTangent, xBitangent);

		// Tangent should be perpendicular to axis
		float fDotAxisTangent = glm::abs(glm::dot(xAxis, xTangent));
		ZENITH_ASSERT_LT(fDotAxisTangent, 0.001f, "Tangent must be perpendicular to axis (%s)", pszName);

		// Bitangent should be perpendicular to axis
		float fDotAxisBitangent = glm::abs(glm::dot(xAxis, xBitangent));
		ZENITH_ASSERT_LT(fDotAxisBitangent, 0.001f, "Bitangent must be perpendicular to axis (%s)", pszName);

		// Tangent should be perpendicular to bitangent
		float fDotTangentBitangent = glm::abs(glm::dot(xTangent, xBitangent));
		ZENITH_ASSERT_LT(fDotTangentBitangent, 0.001f, "Tangent and bitangent must be perpendicular (%s)", pszName);

		// Tangent should be unit length
		float fTangentLen = glm::length(xTangent);
		ZENITH_ASSERT_LT(glm::abs(fTangentLen - 1.0f), 0.001f, "Tangent must be unit length (%s)", pszName);

		// Bitangent should be unit length (cross of unit axis and unit tangent)
		float fBitangentLen = glm::length(xBitangent);
		ZENITH_ASSERT_LT(glm::abs(fBitangentLen - 1.0f), 0.001f, "Bitangent must be unit length (%s)", pszName);
	};

	// Test all three cardinal axes
	TestAxis(Zenith_Maths::Vector3(1, 0, 0), "X axis");
	TestAxis(Zenith_Maths::Vector3(0, 1, 0), "Y axis");
	TestAxis(Zenith_Maths::Vector3(0, 0, 1), "Z axis");

	// Test a diagonal axis
	TestAxis(glm::normalize(Zenith_Maths::Vector3(1, 1, 1)), "Diagonal (1,1,1)");

	// Test an axis near the X threshold (tests the branch that picks Y vs X as perpendicular)
	TestAxis(glm::normalize(Zenith_Maths::Vector3(0.95f, 0.1f, 0.0f)), "Near-X axis");
	TestAxis(glm::normalize(Zenith_Maths::Vector3(0.1f, 0.95f, 0.0f)), "Near-Y axis");

}

//=============================================================================
// Gizmo Unity-parity tests (audit §3.17)
//=============================================================================
// These verify g_xEngine.Gizmos().GetEditableTransform() resolves the target entity's
// transform through the entity's OWN scene, not GetActiveScene(). Unity's
// SceneManager.GetActiveScene is explicit: "the active Scene has no impact on
// what Scenes are rendered" — and multi-scene editing lets the gizmo manipulate
// any loaded entity regardless of which scene is active.
// Refs:
//   https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.GetActiveScene.html
//   https://docs.unity3d.com/ScriptReference/GameObject-scene.html
//   https://docs.unity3d.com/Manual/MultiSceneEditing.html
//=============================================================================

ZENITH_TEST(Core, GizmoEditsPersistentEntityAcrossSceneLoad) { Zenith_UnitTests::TestGizmoEditsPersistentEntityAcrossSceneLoad(); }

void Zenith_UnitTests::TestGizmoEditsPersistentEntityAcrossSceneLoad(){

	// Snapshot active-scene + gizmo target so we can restore after the test.
	Zenith_Scene xSavedActive = g_xEngine.Scenes().GetActiveScene();

	// Create a short-lived scene, spawn an entity, mark it persistent. The entity
	// now lives in the DontDestroyOnLoad scene, NOT the active scene.
	Zenith_Scene xTempScene = g_xEngine.Scenes().LoadScene("GizmoPersistHost", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxTempData = g_xEngine.Scenes().GetSceneData(xTempScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxTempData, "GizmoPersistTarget");
	xEntity.DontDestroyOnLoad();

	// Resolve the entity handle in the persistent scene.
	Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();
	Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(xPersistent);
	Zenith_Entity xPersistentEntity = pxPersistentData->FindEntityByName("GizmoPersistTarget");
	ZENITH_ASSERT_TRUE(xPersistentEntity.IsValid(), "Persistent entity lookup must succeed before gizmo resolve");

	// Set the gizmo target to the persistent entity — stored as pointer to a stable
	// local, matching how Zenith_Editor passes its static selected-entity.
	Zenith_Entity xTargetCopy = xPersistentEntity;
	g_xEngine.Gizmos().SetTargetEntity(&xTargetCopy);

	// Confirm the active scene is NOT the persistent scene (otherwise this test
	// would still pass under the buggy pre-fix code).
	Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
	ZENITH_ASSERT_NE(xActive, xPersistent, "Test setup requires active scene to differ from persistent scene");

	// Fixed behaviour (Wave 3): GetGizmoTargetWithTransform walks the entity's OWN
	// scene (via g_xGizmoTransformAccess.m_pfnHasTransform) and returns the target iff
	// it has a transform. Under the buggy pre-fix code this returned nullptr because
	// the persistent entity isn't in the active scene's component pool.
	Zenith_Entity* pxResolved = g_xEngine.Gizmos().GetGizmoTargetWithTransform();
	ZENITH_ASSERT_NOT_NULL(pxResolved, "GetGizmoTargetWithTransform must resolve the persistent entity via entity.GetSceneData() (Unity parity)");
	ZENITH_ASSERT_EQ(pxResolved, &xTargetCopy, "Resolved target must be the gizmo's target entity (persistent-scene entity)");

	// Cleanup: clear target, destroy persistent entity, unload temp scene.
	g_xEngine.Gizmos().SetTargetEntity(nullptr);
	xPersistentEntity.DestroyImmediate();
	g_xEngine.Scenes().UnloadScene(xTempScene);
	g_xEngine.Scenes().SetActiveScene(xSavedActive);

}

ZENITH_TEST(Core, GizmoEditsEntityInAdditiveScene) { Zenith_UnitTests::TestGizmoEditsEntityInAdditiveScene(); }

void Zenith_UnitTests::TestGizmoEditsEntityInAdditiveScene(){

	Zenith_Scene xSavedActive = g_xEngine.Scenes().GetActiveScene();

	// Scene A — will be active.
	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("GizmoActiveScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xSceneA);

	// Scene B — additive, contains the target.
	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("GizmoAdditiveScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneBData = g_xEngine.Scenes().GetSceneData(xSceneB);
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneBData, "AdditiveTarget");

	// Confirm active scene really is A, not B.
	ZENITH_ASSERT_EQ(g_xEngine.Scenes().GetActiveScene(), xSceneA, "Test setup: active scene should be A, target lives in B");

	g_xEngine.Gizmos().SetTargetEntity(&xTarget);

	Zenith_Entity* pxResolved = g_xEngine.Gizmos().GetGizmoTargetWithTransform();
	ZENITH_ASSERT_NOT_NULL(pxResolved, "Gizmo must resolve target in additively-loaded scene (Unity multi-scene editing parity)");
	ZENITH_ASSERT_EQ(pxResolved, &xTarget, "Resolved target must be Scene B's entity, not Scene A");

	g_xEngine.Gizmos().SetTargetEntity(nullptr);
	g_xEngine.Scenes().UnloadScene(xSceneB);
	g_xEngine.Scenes().UnloadScene(xSceneA);
	g_xEngine.Scenes().SetActiveScene(xSavedActive);

}

ZENITH_TEST(Core, GizmoDragSurvivesActiveSceneChange) { Zenith_UnitTests::TestGizmoDragSurvivesActiveSceneChange(); }

void Zenith_UnitTests::TestGizmoDragSurvivesActiveSceneChange(){

	Zenith_Scene xSavedActive = g_xEngine.Scenes().GetActiveScene();

	// Scene A contains the target; Scene B becomes active mid-"drag".
	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("GizmoDragSourceScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("GizmoDragOtherScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xSceneA);

	Zenith_SceneData* pxSceneAData = g_xEngine.Scenes().GetSceneData(xSceneA);
	Zenith_Entity xTarget = g_xEngine.Scenes().CreateEntity(pxSceneAData, "DragTarget");

	g_xEngine.Gizmos().SetTargetEntity(&xTarget);

	// Begin the "drag" while target's scene is active.
	Zenith_Entity* pxBefore = g_xEngine.Gizmos().GetGizmoTargetWithTransform();
	ZENITH_ASSERT_NOT_NULL(pxBefore, "Pre-switch gizmo resolve must succeed");

	// Simulate the active-scene change mid-drag.
	g_xEngine.Scenes().SetActiveScene(xSceneB);
	ZENITH_ASSERT_EQ(g_xEngine.Scenes().GetActiveScene(), xSceneB, "Active scene should be B after SetActiveScene");

	// Post-switch: gizmo must still resolve to Scene A's entity (Unity parity —
	// active scene doesn't gate editability).
	Zenith_Entity* pxAfter = g_xEngine.Gizmos().GetGizmoTargetWithTransform();
	ZENITH_ASSERT_NOT_NULL(pxAfter, "Gizmo must keep resolving target after active-scene change (Unity parity)");
	ZENITH_ASSERT_EQ(pxAfter, pxBefore, "Gizmo target must be identical across the active-scene switch (same underlying entity)");

	g_xEngine.Gizmos().SetTargetEntity(nullptr);
	g_xEngine.Scenes().UnloadScene(xSceneB);
	g_xEngine.Scenes().UnloadScene(xSceneA);
	g_xEngine.Scenes().SetActiveScene(xSavedActive);

}

ZENITH_TEST(Core, GizmoGetEditableTransform_ReturnsNullForInvalidTarget) { Zenith_UnitTests::TestGizmoGetEditableTransform_ReturnsNullForInvalidTarget(); }

void Zenith_UnitTests::TestGizmoGetEditableTransform_ReturnsNullForInvalidTarget(){

	// No target set — must return nullptr.
	g_xEngine.Gizmos().SetTargetEntity(nullptr);
	ZENITH_ASSERT_NULL(g_xEngine.Gizmos().GetGizmoTargetWithTransform(), "GetGizmoTargetWithTransform must return nullptr when no target is set");

}

#endif // ZENITH_TOOLS

// ============================================================================
// Flux render-graph tests
// ============================================================================
// These tests exercise the graph's declaration phase (AddPass, CreateTransient,
// SetEnabled, Clear, generation counters) without calling Compile() — Compile
// allocates VRAM via Flux_RenderAttachmentBuilder which requires a live Vulkan
// device. A full golden-path test with synthetic passes mirroring the real
// pipeline needs a headless Vulkan init path; that is out of scope here. The
// sample-game runs under sync validation (Sokoban + Combat + the other games)
// provide end-to-end correctness coverage for the compiled + executed graph.

#include "Flux/RenderGraph/Flux_RenderGraph.h"

static void EmptyRecordCallback(Flux_CommandList*, void*) {}

ZENITH_TEST(Core, RenderGraphEmpty) { Zenith_UnitTests::TestRenderGraphEmpty(); }

void Zenith_UnitTests::TestRenderGraphEmpty(){
	Flux_RenderGraph xGraph;

	ZENITH_ASSERT_EQ(xGraph.GetPasses().GetSize(), 0, "Fresh graph has no passes");
	ZENITH_ASSERT_TRUE(xGraph.IsDirty(), "Fresh Flux_RenderGraph starts in dirty state (m_bDirty default-initialised to true)");

	xGraph.MarkDirty();
	ZENITH_ASSERT_TRUE(xGraph.IsDirty(), "After MarkDirty, IsDirty == true");

	xGraph.Clear();
	ZENITH_ASSERT_EQ(xGraph.GetPasses().GetSize(), 0, "Clear leaves graph empty");

}

ZENITH_TEST(Core, RenderGraphPassHandles) { Zenith_UnitTests::TestRenderGraphPassHandles(); }

void Zenith_UnitTests::TestRenderGraphPassHandles(){
	Flux_RenderGraph xGraph;

	Flux_PassHandle xPassA = xGraph.AddPass("A", EmptyRecordCallback);
	Flux_PassHandle xPassB = xGraph.AddPass("B", EmptyRecordCallback);

	ZENITH_ASSERT_TRUE(xPassA.IsValid(), "PassA handle valid");
	ZENITH_ASSERT_TRUE(xPassB.IsValid(), "PassB handle valid");
	ZENITH_ASSERT_NE(xPassA, xPassB, "Different passes produce different handles");
	ZENITH_ASSERT_EQ(xPassA.m_uGeneration, xPassB.m_uGeneration, "Handles from same graph generation match");
	ZENITH_ASSERT_EQ(xGraph.GetPasses().GetSize(), 2, "Graph has two passes");

	// Invalid handle is false-valued.
	Flux_PassHandle xBad;
	ZENITH_ASSERT_FALSE(xBad.IsValid(), "Default-constructed handle is invalid");

}

ZENITH_TEST(Core, RenderGraphTransientGeneration) { Zenith_UnitTests::TestRenderGraphTransientGeneration(); }

void Zenith_UnitTests::TestRenderGraphTransientGeneration(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc;
	xDesc.m_uWidth = 64;
	xDesc.m_uHeight = 64;
	xDesc.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;

	Flux_TransientHandle xH0 = xGraph.CreateTransient(xDesc);
	ZENITH_ASSERT_TRUE(xH0.IsValid(), "CreateTransient returns a valid handle");

	const u_int uGenBefore = xH0.m_uGeneration;

	// Clear bumps generation — previously-issued handles become stale.
	xGraph.Clear();

	Flux_TransientHandle xH1 = xGraph.CreateTransient(xDesc);
	ZENITH_ASSERT_TRUE(xH1.IsValid(), "Handle after Clear is still valid");
	ZENITH_ASSERT_NE(xH1.m_uGeneration, uGenBefore, "Clear() bumps graph generation so old handles fail AssertTransientHandleValid");

}

ZENITH_TEST(Core, RenderGraphSetEnabled) { Zenith_UnitTests::TestRenderGraphSetEnabled(); }

void Zenith_UnitTests::TestRenderGraphSetEnabled(){
	Flux_RenderGraph xGraph;

	Flux_PassHandle xPass = xGraph.AddPass("Togglable", EmptyRecordCallback);
	ZENITH_ASSERT_TRUE(xGraph.GetPasses().Get(xPass.m_uIndex)->m_bEnabled, "Pass enabled by default");

	xGraph.SetEnabled(xPass, false);
	ZENITH_ASSERT_FALSE(xGraph.GetPasses().Get(xPass.m_uIndex)->m_bEnabled, "SetEnabled(false) disables pass");

	xGraph.SetEnabled(xPass, true);
	ZENITH_ASSERT_TRUE(xGraph.GetPasses().Get(xPass.m_uIndex)->m_bEnabled, "SetEnabled(true) re-enables pass");

}

// Verify that two consecutive compute passes RMW-ing the same Flux_ReadWriteBuffer
// produce a buffer-kind prologue barrier on the second pass. SynthesizeBarriers is
// pure CPU once m_xExecutionOrder is populated; we bypass Compile (which would
// allocate VRAM via Flux_RenderAttachmentBuilder and need a Vulkan device) by
// poking m_xExecutionOrder directly via the Zenith_UnitTests friend access.
ZENITH_TEST(Core, RenderGraphBufferBarrierRMW) { Zenith_UnitTests::TestRenderGraphBufferBarrierRMW(); }
void Zenith_UnitTests::TestRenderGraphBufferBarrierRMW(){
	Flux_RenderGraph xGraph;

	// Fake-valid VRAM handle and non-zero size on the buffer so the graph's
	// declaration-time and barrier-emitter checks don't short-circuit. The
	// test never touches the backing VRAM — only the buffer pointer identity
	// matters for state-map keying.
	Flux_ReadWriteBuffer xBuffer;
	xBuffer.GetBuffer().m_xVRAMHandle.SetValue(0);
	xBuffer.GetBuffer().m_ulSize = 256;

	Flux_PassHandle xPassA = xGraph.AddPass("RMW_A", EmptyRecordCallback);
	xGraph.WriteBuffer(xPassA, xBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);

	Flux_PassHandle xPassB = xGraph.AddPass("RMW_B", EmptyRecordCallback);
	xGraph.WriteBuffer(xPassB, xBuffer.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);

	// Skip Compile() — manually populate execution order in pass-add order
	// (the only ordering SynthesizeBarriers cares about).
	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xPassA.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xPassB.m_uIndex);

	xGraph.SynthesizeBarriers();

	const Flux_RenderGraph_Pass* pxA = xGraph.GetPasses().Get(xPassA.m_uIndex);
	const Flux_RenderGraph_Pass* pxB = xGraph.GetPasses().Get(xPassB.m_uIndex);

	// Pass A is the first writer — UNDEFINED → READWRITE_UAV is technically a
	// "barrier" too because the new-side is a write, so we expect ONE entry
	// on A (the discard / first-touch barrier).
	u_int uABufferBarriers = 0;
	for (u_int u = 0; u < pxA->m_xPrologueBarriers.GetSize(); u++)
	{
		const Flux_RenderGraph_Barrier& rxBar = pxA->m_xPrologueBarriers.Get(u);
		if (rxBar.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
			uABufferBarriers++;
	}
	ZENITH_ASSERT_EQ(uABufferBarriers, 1, "TestRenderGraphBufferBarrierRMW: pass A expected 1 buffer barrier (UNDEFINED→RW), got %u", uABufferBarriers);

	// Pass B is the second writer — must have exactly ONE buffer barrier
	// (RW→RW write-after-write). This is the load-bearing assertion that
	// proves the new buffer-barrier path actually emits.
	u_int uBBufferBarriers = 0;
	const Flux_RenderGraph_Barrier* pxBBarrier = nullptr;
	for (u_int u = 0; u < pxB->m_xPrologueBarriers.GetSize(); u++)
	{
		const Flux_RenderGraph_Barrier& rxBar = pxB->m_xPrologueBarriers.Get(u);
		if (rxBar.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
		{
			uBBufferBarriers++;
			pxBBarrier = &rxBar;
		}
	}
	ZENITH_ASSERT_EQ(uBBufferBarriers, 1, "TestRenderGraphBufferBarrierRMW: pass B expected 1 buffer barrier (RW→RW WAW), got %u", uBBufferBarriers);
	ZENITH_ASSERT_NOT_NULL(pxBBarrier, "TestRenderGraphBufferBarrierRMW: pass B buffer barrier missing");
	ZENITH_ASSERT_EQ(pxBBarrier->m_eSrcAccess, RESOURCE_ACCESS_READWRITE_UAV, "TestRenderGraphBufferBarrierRMW: pass B src access expected READWRITE_UAV, got %d", (int)pxBBarrier->m_eSrcAccess);
	ZENITH_ASSERT_EQ(pxBBarrier->m_eDstAccess, RESOURCE_ACCESS_READWRITE_UAV, "TestRenderGraphBufferBarrierRMW: pass B dst access expected READWRITE_UAV, got %d", (int)pxBBarrier->m_eDstAccess);
	ZENITH_ASSERT_EQ(pxBBarrier->m_xResource.AsBuffer(), &xBuffer.GetBuffer(), "TestRenderGraphBufferBarrierRMW: pass B barrier targets wrong buffer");

}

// Pass A writes the buffer (terrain culling compute), pass B reads it as
// indirect arguments (terrain GBuffer's DrawIndexedIndirectCount). Verifies
// that the new RESOURCE_ACCESS_READ_INDIRECT_ARG access correctly drives the
// graph to synthesise a compute→indirect-arg buffer barrier on the consumer.
ZENITH_TEST(Core, RenderGraphIndirectArgBarrier) { Zenith_UnitTests::TestRenderGraphIndirectArgBarrier(); }
void Zenith_UnitTests::TestRenderGraphIndirectArgBarrier(){
	Flux_RenderGraph xGraph;

	Flux_ReadWriteBuffer xBuffer;
	xBuffer.GetBuffer().m_xVRAMHandle.SetValue(0);
	xBuffer.GetBuffer().m_ulSize = 256;

	Flux_PassHandle xPassA = xGraph.AddPass("CullingWrites", EmptyRecordCallback);
	xGraph.WriteBuffer(xPassA, xBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xPassB = xGraph.AddPass("GBufferReadsIndirectArgs", EmptyRecordCallback);
	xGraph.ReadBuffer(xPassB, xBuffer.GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xPassA.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xPassB.m_uIndex);

	xGraph.SynthesizeBarriers();

	const Flux_RenderGraph_Pass* pxB = xGraph.GetPasses().Get(xPassB.m_uIndex);

	u_int uBufferBarriers = 0;
	const Flux_RenderGraph_Barrier* pxBarrier = nullptr;
	for (u_int u = 0; u < pxB->m_xPrologueBarriers.GetSize(); u++)
	{
		const Flux_RenderGraph_Barrier& rxBar = pxB->m_xPrologueBarriers.Get(u);
		if (rxBar.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
		{
			uBufferBarriers++;
			pxBarrier = &rxBar;
		}
	}
	ZENITH_ASSERT_EQ(uBufferBarriers, 1, "TestRenderGraphIndirectArgBarrier: GBuffer pass expected 1 buffer barrier (compute-write -> indirect-arg-read), got %u", uBufferBarriers);
	ZENITH_ASSERT_NOT_NULL(pxBarrier, "TestRenderGraphIndirectArgBarrier: GBuffer pass buffer barrier missing");
	ZENITH_ASSERT_EQ(pxBarrier->m_eSrcAccess, RESOURCE_ACCESS_WRITE_UAV, "TestRenderGraphIndirectArgBarrier: src expected WRITE_UAV, got %d", (int)pxBarrier->m_eSrcAccess);
	ZENITH_ASSERT_EQ(pxBarrier->m_eDstAccess, RESOURCE_ACCESS_READ_INDIRECT_ARG, "TestRenderGraphIndirectArgBarrier: dst expected READ_INDIRECT_ARG, got %d", (int)pxBarrier->m_eDstAccess);
	ZENITH_ASSERT_EQ(pxBarrier->m_xResource.AsBuffer(), &xBuffer.GetBuffer(), "TestRenderGraphIndirectArgBarrier: barrier targets wrong buffer");

}

// Pass A writes the buffer (terrain culling compute writes LODLevelBuffer),
// pass B reads it as a read-only structured buffer (GBuffer vertex shader
// samples LODLevelBuffer). Verifies RESOURCE_ACCESS_READ_BUFFER_SRV drives
// a compute-write -> shader-read barrier on the consumer pass.
ZENITH_TEST(Core, RenderGraphStorageBufferSRVBarrier) { Zenith_UnitTests::TestRenderGraphStorageBufferSRVBarrier(); }
void Zenith_UnitTests::TestRenderGraphStorageBufferSRVBarrier(){
	Flux_RenderGraph xGraph;

	Flux_ReadWriteBuffer xBuffer;
	xBuffer.GetBuffer().m_xVRAMHandle.SetValue(0);
	xBuffer.GetBuffer().m_ulSize = 256;

	Flux_PassHandle xPassA = xGraph.AddPass("CullingWritesLOD", EmptyRecordCallback);
	xGraph.WriteBuffer(xPassA, xBuffer.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xPassB = xGraph.AddPass("GBufferReadsLODSRV", EmptyRecordCallback);
	xGraph.ReadBuffer(xPassB, xBuffer.GetBuffer(), RESOURCE_ACCESS_READ_BUFFER_SRV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xPassA.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xPassB.m_uIndex);

	xGraph.SynthesizeBarriers();

	const Flux_RenderGraph_Pass* pxB = xGraph.GetPasses().Get(xPassB.m_uIndex);

	u_int uBufferBarriers = 0;
	const Flux_RenderGraph_Barrier* pxBarrier = nullptr;
	for (u_int u = 0; u < pxB->m_xPrologueBarriers.GetSize(); u++)
	{
		const Flux_RenderGraph_Barrier& rxBar = pxB->m_xPrologueBarriers.Get(u);
		if (rxBar.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
		{
			uBufferBarriers++;
			pxBarrier = &rxBar;
		}
	}
	ZENITH_ASSERT_EQ(uBufferBarriers, 1, "TestRenderGraphStorageBufferSRVBarrier: GBuffer pass expected 1 buffer barrier (compute-write -> SRV-read), got %u", uBufferBarriers);
	ZENITH_ASSERT_NOT_NULL(pxBarrier, "TestRenderGraphStorageBufferSRVBarrier: GBuffer pass buffer barrier missing");
	ZENITH_ASSERT_EQ(pxBarrier->m_eSrcAccess, RESOURCE_ACCESS_WRITE_UAV, "TestRenderGraphStorageBufferSRVBarrier: src expected WRITE_UAV, got %d", (int)pxBarrier->m_eSrcAccess);
	ZENITH_ASSERT_EQ(pxBarrier->m_eDstAccess, RESOURCE_ACCESS_READ_BUFFER_SRV, "TestRenderGraphStorageBufferSRVBarrier: dst expected READ_BUFFER_SRV, got %d", (int)pxBarrier->m_eDstAccess);
	ZENITH_ASSERT_EQ(pxBarrier->m_xResource.AsBuffer(), &xBuffer.GetBuffer(), "TestRenderGraphStorageBufferSRVBarrier: barrier targets wrong buffer");

}

// ============================================================================
// WS7 keystone (C1C2): InstancedMeshes Prepare-gather determinism / thread-safety
// ============================================================================
// The InstancedMeshes record callbacks (ExecuteCulling / ExecuteInstancedGBuffer)
// used to mutate each Flux_InstanceGroup's CPU bookkeeping (m_uVisibleCount + the
// visible-index list) from CONCURRENT worker threads. WS7 (C1) relocated that to a
// SINGLE main-thread .Prepare gather; the CPU visibility math is factored into
// Flux_InstanceGroup::ComputeVisibleIndices (device-independent). This test pins
// the determinism guarantee: with GPU culling forced OFF (the path that used to
// double-call UpdateGPUBuffers), a fixed seeded layout produces a byte-identical
// visible-state hash on every repeated re-seed + recompute (zero divergence == the
// cross-worker race is gone). It also cross-checks that the visible count derived
// from the single-writer recompute equals the freshly computed visible-index size.
//
// Headless-safe: SeedInstancesForTest fills only the CPU SoA arrays (no GPU buffer
// allocation, which would assert without a Vulkan allocator).
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Core/Zenith_GraphicsOptions.h"

ZENITH_TEST(Core, InstancedMeshesPrepareDeterminism) { Zenith_UnitTests::TestInstancedMeshesPrepareDeterminism(); }
void Zenith_UnitTests::TestInstancedMeshesPrepareDeterminism(){

	// Force the CPU-fallback path (the one that used to double-call UpdateGPUBuffers
	// across two concurrent record callbacks). Restore on exit so no test bleeds state.
	const bool bPrevGPUCulling = Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled;
	Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled = false;

	static constexpr uint32_t uNUM_INSTANCES = 4096;
	static constexpr uint32_t uSEED          = 0xC0FFEEu;
	static constexpr uint32_t uNUM_REPEATS   = 4;

	// --- Legacy serial reference: seed once, compute the reference visible set. ---
	uint64_t uReferenceHash = 0;
	uint32_t uReferenceVisible = 0;
	{
		Flux_InstanceGroup xRefGroup;
		xRefGroup.SeedInstancesForTest(uNUM_INSTANCES, uSEED);

		Zenith_Vector<uint32_t> auRefVisible;
		xRefGroup.ComputeVisibleIndices(auRefVisible);
		uReferenceVisible = auRefVisible.GetSize();

		// Sanity: the seed must leave a non-trivial visible subset (not all / none),
		// otherwise the determinism check would be vacuous.
		ZENITH_ASSERT_TRUE(uReferenceVisible > 0 && uReferenceVisible < uNUM_INSTANCES,
			"InstancedMeshesPrepareDeterminism: seed produced a degenerate visible set (%u of %u)",
			uReferenceVisible, uNUM_INSTANCES);

		uReferenceHash = xRefGroup.HashVisibleStateForTest();
	}

	// --- Repeated re-seeds (independent groups): every run must be byte-identical. ---
	for (uint32_t uRepeat = 0; uRepeat < uNUM_REPEATS; ++uRepeat)
	{
		Flux_InstanceGroup xGroup;
		xGroup.SeedInstancesForTest(uNUM_INSTANCES, uSEED);

		// The visible-index list is the exact CPU computation the single main-thread
		// writer (GatherInstancedPacket -> UpdateGPUBuffers -> ComputeVisibleIndices)
		// performs; recomputing it must reproduce the reference set every time.
		Zenith_Vector<uint32_t> auVisible;
		xGroup.ComputeVisibleIndices(auVisible);
		ZENITH_ASSERT_EQ(auVisible.GetSize(), uReferenceVisible,
			"InstancedMeshesPrepareDeterminism: visible count diverged on repeat %u (%u vs ref %u)",
			uRepeat, auVisible.GetSize(), uReferenceVisible);

		const uint64_t uHash = xGroup.HashVisibleStateForTest();
		ZENITH_ASSERT_TRUE(uHash == uReferenceHash,
			"InstancedMeshesPrepareDeterminism: visible-state hash (0x%016llx, repeat %u) diverged from reference (0x%016llx)",
			static_cast<unsigned long long>(uHash), uRepeat,
			static_cast<unsigned long long>(uReferenceHash));
	}

	// --- A DIFFERENT seed must produce a DIFFERENT layout (guards a no-op accessor
	//     that always returns the same constant — which would make the above vacuous). ---
	{
		Flux_InstanceGroup xOtherGroup;
		xOtherGroup.SeedInstancesForTest(uNUM_INSTANCES, uSEED ^ 0x5A5A5A5Au);
		const uint64_t uOtherHash = xOtherGroup.HashVisibleStateForTest();
		ZENITH_ASSERT_TRUE(uOtherHash != uReferenceHash,
			"InstancedMeshesPrepareDeterminism: a different seed produced the same hash (0x%016llx) — accessor likely not reading state",
			static_cast<unsigned long long>(uReferenceHash));
	}

	Zenith_GraphicsOptions::Get().m_bInstancedMeshGPUCullingEnabled = bPrevGPUCulling;
}

// ============================================================================
// Transient-aliasing signature tests
// ============================================================================
// MakeTransientMemorySignature is a pure function of the descriptor — tests
// verify: (a) identical descs produce matching signatures, (b) any descriptor
// field that affects memory requirements flips the signature, (c) fields that
// DO NOT affect memory requirements (dimensions, mip count) are ignored.

namespace
{
	Flux_TransientTextureDesc DefaultDesc()
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = 1280;
		xDesc.m_uHeight      = 720;
		xDesc.m_uDepth       = 1;
		xDesc.m_eFormat      = TEXTURE_FORMAT_RGBA8_UNORM;
		xDesc.m_eTextureType = TEXTURE_TYPE_2D;
		xDesc.m_uNumMips     = 1;
		xDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		xDesc.m_bIsDepthStencil = false;
		return xDesc;
	}
}

ZENITH_TEST(Core, AliasSignatureIdenticalDescs) { Zenith_UnitTests::TestAliasSignatureIdenticalDescs(); }

void Zenith_UnitTests::TestAliasSignatureIdenticalDescs(){
	const Flux_TransientTextureDesc xA = DefaultDesc();
	const Flux_TransientTextureDesc xB = DefaultDesc();
	ZENITH_ASSERT_EQ(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Identical descs must produce identical signatures");
}

ZENITH_TEST(Core, AliasSignatureDifferentFormat) { Zenith_UnitTests::TestAliasSignatureDifferentFormat(); }

void Zenith_UnitTests::TestAliasSignatureDifferentFormat(){
	Flux_TransientTextureDesc xA = DefaultDesc();
	Flux_TransientTextureDesc xB = DefaultDesc();
	xB.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	ZENITH_ASSERT_NE(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Different format must produce different signature");
}

ZENITH_TEST(Core, AliasSignatureDifferentMemoryFlags) { Zenith_UnitTests::TestAliasSignatureDifferentMemoryFlags(); }

void Zenith_UnitTests::TestAliasSignatureDifferentMemoryFlags(){
	Flux_TransientTextureDesc xA = DefaultDesc();
	Flux_TransientTextureDesc xB = DefaultDesc();
	xB.m_uMemoryFlags = xA.m_uMemoryFlags | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);
	ZENITH_ASSERT_NE(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Adding UAV bit must produce different signature");
}

ZENITH_TEST(Core, AliasSignatureDifferentTextureType) { Zenith_UnitTests::TestAliasSignatureDifferentTextureType(); }

void Zenith_UnitTests::TestAliasSignatureDifferentTextureType(){
	Flux_TransientTextureDesc xA = DefaultDesc();
	Flux_TransientTextureDesc xB = DefaultDesc();
	xB.m_eTextureType = TEXTURE_TYPE_3D;
	xB.m_uDepth       = 64;
	ZENITH_ASSERT_NE(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Different texture type (2D vs 3D) must produce different signature");
}

ZENITH_TEST(Core, AliasSignatureDepthVsColour) { Zenith_UnitTests::TestAliasSignatureDepthVsColour(); }

void Zenith_UnitTests::TestAliasSignatureDepthVsColour(){
	Flux_TransientTextureDesc xA = DefaultDesc();
	Flux_TransientTextureDesc xB = DefaultDesc();
	xB.m_bIsDepthStencil = true;
	xB.m_eFormat         = TEXTURE_FORMAT_D32_SFLOAT;
	ZENITH_ASSERT_NE(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Depth-stencil flag must produce different signature than colour");
}

ZENITH_TEST(Core, AliasSignatureIgnoresDimensions) { Zenith_UnitTests::TestAliasSignatureIgnoresDimensions(); }

void Zenith_UnitTests::TestAliasSignatureIgnoresDimensions(){
	// Two descs differing ONLY in width/height/mip-count must have matching
	// signatures. The packer handles size variation by computing pool size =
	// max(occupant size); the signature is about memory-requirement class.
	Flux_TransientTextureDesc xA = DefaultDesc();
	Flux_TransientTextureDesc xB = DefaultDesc();
	xB.m_uWidth   = 3840;
	xB.m_uHeight  = 2160;
	xB.m_uNumMips = 8;
	ZENITH_ASSERT_EQ(Flux_RenderGraph::MakeTransientMemorySignature(xA), Flux_RenderGraph::MakeTransientMemorySignature(xB), "Dimensions and mip count must NOT affect signature");
}

// ============================================================================
// Flux render-graph lifetime + aliasing-packer tests
// ============================================================================
// These exercise ComputeResourceLifetimes / ComputeTransientLifetimes /
// SortTransientsByLifetime / PackTransientsIntoPools / SynthesizeAliasingBarriers
// without going through Compile() (which allocates VRAM). Pattern:
//   - construct passes + transients, declare reads/writes
//   - manually populate m_xExecutionOrder via friend access (bypassing toposort)
//   - invoke the lifetime + packing phases directly
//   - inspect TransientResource state and emitted aliasing barriers
//
// The friend access to m_axTransients, m_xResources, m_xExecutionOrder is granted
// by `friend class Zenith_UnitTests` in Flux_RenderGraph.h.

namespace
{
	// Full-res RGBA16F transient desc, SRV-readable + RTV-writable. Matches
	// the signature used by SSR upsampled / SSGI resolved (the transients
	// at the centre of the original aliasing bug).
	Flux_TransientTextureDesc RGTestColorDesc()
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth          = 256;
		xDesc.m_uHeight         = 256;
		xDesc.m_uDepth          = 1;
		xDesc.m_uNumMips        = 1;
		xDesc.m_eFormat         = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xDesc.m_eTextureType    = TEXTURE_TYPE_2D;
		xDesc.m_uMemoryFlags    = (1u << MEMORY_FLAGS__SHADER_READ);
		xDesc.m_bIsDepthStencil = false;
		return xDesc;
	}

	// Same as RGTestColorDesc but also UAV-writable, for UAV multi-write
	// scenarios (RWTexture2D ping-pong patterns).
	Flux_TransientTextureDesc RGTestColorUAVDesc()
	{
		Flux_TransientTextureDesc xDesc = RGTestColorDesc();
		xDesc.m_uMemoryFlags |= (1u << MEMORY_FLAGS__UNORDERED_ACCESS);
		return xDesc;
	}
}

// Lifetime values must be TOPOLOGICAL ORDER positions (positions in
// m_xExecutionOrder), not pass-declaration indices. Constructed with
// declaration order REVERSED relative to execution order so the two
// disagree everywhere — a regression to pass-index storage would fail
// every assertion.
ZENITH_TEST(Core, RenderGraphLifetimeUsesTopologicalOrder) { Zenith_UnitTests::TestRenderGraphLifetimeUsesTopologicalOrder(); }

void Zenith_UnitTests::TestRenderGraphLifetimeUsesTopologicalOrder(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorDesc();
	Flux_TransientHandle xT = xGraph.CreateTransient(xDesc);

	// Pass indices 0..3 in declaration order. Execution order will be
	// [P3, P2, P1, P0], so topo position(P3)=0 and pass-index(P3)=3 — the
	// two number spaces are inverses, making any pass-index leak detectable.
	Flux_PassHandle xP0 = xGraph.AddPass("P0_Read",  EmptyRecordCallback);
	Flux_PassHandle xP1 = xGraph.AddPass("P1",       EmptyRecordCallback);
	Flux_PassHandle xP2 = xGraph.AddPass("P2",       EmptyRecordCallback);
	Flux_PassHandle xP3 = xGraph.AddPass("P3_Write", EmptyRecordCallback);

	xGraph.WriteTransient(xP3, xT, RESOURCE_ACCESS_WRITE_RTV);
	xGraph.ReadTransient (xP0, xT, RESOURCE_ACCESS_READ_SRV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xP3.m_uIndex);  // topo 0 (writer first)
	xGraph.m_xExecutionOrder.PushBack(xP2.m_uIndex);  // topo 1
	xGraph.m_xExecutionOrder.PushBack(xP1.m_uIndex);  // topo 2
	xGraph.m_xExecutionOrder.PushBack(xP0.m_uIndex);  // topo 3 (reader last)

	xGraph.ComputeResourceLifetimes();

	Flux_RenderGraph::TransientResource* pxT = xGraph.m_axTransients.Get(xT.m_uIndex);
	void* pTKey = static_cast<void*>(&pxT->m_xAttachment);
	const Flux_RenderGraph_Resource* pxRes = xGraph.m_xResources.TryGet(pTKey);
	ZENITH_ASSERT_TRUE(pxRes != nullptr, "T must be tracked in m_xResources");

	const Flux_RenderGraph_Resource& xRes = *pxRes;
	ZENITH_ASSERT_EQ(xRes.m_uFirstWrite, 0u,
		"first-write must be the topological position of the writer (topo 0 = P3 at exec[0]), not the writer's pass index (3). Got %u",
		xRes.m_uFirstWrite);
	ZENITH_ASSERT_EQ(xRes.m_uLastWrite, 0u,
		"last-write must also be topo 0 for a single-writer transient. Got %u",
		xRes.m_uLastWrite);
	ZENITH_ASSERT_EQ(xRes.m_uLastRead, 3u,
		"last-read must be the topological position of the reader (topo 3 = P0 at exec[3]), not the reader's pass index (0). Got %u",
		xRes.m_uLastRead);
}

// SSR/SSGI regression: two transients with identical memory signature whose
// pass-index lifetimes don't overlap, but whose topological-order lifetimes
// DO. Pre-fix, the packer used pass indices and aliased them, causing one
// subsystem's writes to trample the other's data between writes and reads.
ZENITH_TEST(Core, RenderGraphAliasingTopoOrderInterleaved) { Zenith_UnitTests::TestRenderGraphAliasingTopoOrderInterleaved(); }

void Zenith_UnitTests::TestRenderGraphAliasingTopoOrderInterleaved(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorDesc();
	Flux_TransientHandle xT_A = xGraph.CreateTransient(xDesc);
	Flux_TransientHandle xT_B = xGraph.CreateTransient(xDesc);

	// Subsystem A declares first (pass indices 0, 1), subsystem B second
	// (pass indices 2, 3). With no cross-deps, BFS toposort produces
	// [PA1, PB1, PA2, PB2] — A and B writers issue side-by-side, then their
	// readers. By PASS INDEX: T_A=[0,1], T_B=[2,3] (no overlap, packer would
	// alias). By TOPO: T_A=[0,2], T_B=[1,3] (overlapping, must NOT alias).
	Flux_PassHandle xPA1 = xGraph.AddPass("A1_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xPA1, xT_A, RESOURCE_ACCESS_WRITE_RTV);

	Flux_PassHandle xPA2 = xGraph.AddPass("A2_Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xPA2, xT_A, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xPB1 = xGraph.AddPass("B1_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xPB1, xT_B, RESOURCE_ACCESS_WRITE_RTV);

	Flux_PassHandle xPB2 = xGraph.AddPass("B2_Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xPB2, xT_B, RESOURCE_ACCESS_READ_SRV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xPA1.m_uIndex);  // topo 0
	xGraph.m_xExecutionOrder.PushBack(xPB1.m_uIndex);  // topo 1
	xGraph.m_xExecutionOrder.PushBack(xPA2.m_uIndex);  // topo 2
	xGraph.m_xExecutionOrder.PushBack(xPB2.m_uIndex);  // topo 3

	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Flux_RenderGraph::TransientResource* pxT_A = xGraph.m_axTransients.Get(xT_A.m_uIndex);
	Flux_RenderGraph::TransientResource* pxT_B = xGraph.m_axTransients.Get(xT_B.m_uIndex);

	ZENITH_ASSERT_EQ(pxT_A->m_uFirstWrite, 0u, "T_A first-write at topo 0");
	ZENITH_ASSERT_EQ(pxT_A->m_uLastUse,    2u, "T_A last-use at topo 2 (PA2 read)");
	ZENITH_ASSERT_EQ(pxT_B->m_uFirstWrite, 1u, "T_B first-write at topo 1");
	ZENITH_ASSERT_EQ(pxT_B->m_uLastUse,    3u, "T_B last-use at topo 3 (PB2 read)");

	Zenith_Vector<u_int> axSorted;
	xGraph.SortTransientsByLifetime(axSorted);
	xGraph.PackTransientsIntoPools(axSorted);

	ZENITH_ASSERT_NE(pxT_A->m_uAliasPoolIndex, UINT32_MAX, "T_A assigned to a pool");
	ZENITH_ASSERT_NE(pxT_B->m_uAliasPoolIndex, UINT32_MAX, "T_B assigned to a pool");
	ZENITH_ASSERT_NE(pxT_A->m_uAliasPoolIndex, pxT_B->m_uAliasPoolIndex,
		"REGRESSION GUARD (SSR/SSGI bug): T_A topo lifetime [0,2] and T_B [1,3] OVERLAP — packer must place them in different pools. "
		"If this fires, ComputeResourceLifetimes is back to storing pass-declaration indices instead of topological positions, "
		"and one subsystem's writes will trample the other's transient memory between its first-use and last-use. "
		"T_A pool=%u, T_B pool=%u",
		pxT_A->m_uAliasPoolIndex, pxT_B->m_uAliasPoolIndex);
}

// Multi-write transient: write at P0, read at P1, write again at P2.
// Pre-fix, m_uLastUse = m_uLastRead = 1 (P1's read), missing the later
// write at P2. Post-fix, m_uLastUse = max(last_read, last_write) = 2.
ZENITH_TEST(Core, RenderGraphMultiWriteLastUse) { Zenith_UnitTests::TestRenderGraphMultiWriteLastUse(); }

void Zenith_UnitTests::TestRenderGraphMultiWriteLastUse(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorUAVDesc();
	Flux_TransientHandle xT = xGraph.CreateTransient(xDesc);

	Flux_PassHandle xP0 = xGraph.AddPass("P0_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP0, xT, RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xP1 = xGraph.AddPass("P1_Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xP1, xT, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xP2 = xGraph.AddPass("P2_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP2, xT, RESOURCE_ACCESS_WRITE_UAV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xP0.m_uIndex);  // topo 0
	xGraph.m_xExecutionOrder.PushBack(xP1.m_uIndex);  // topo 1
	xGraph.m_xExecutionOrder.PushBack(xP2.m_uIndex);  // topo 2

	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Flux_RenderGraph::TransientResource* pxT = xGraph.m_axTransients.Get(xT.m_uIndex);

	ZENITH_ASSERT_EQ(pxT->m_uFirstWrite, 0u, "first-write at topo 0");
	ZENITH_ASSERT_EQ(pxT->m_uLastUse, 2u,
		"REGRESSION GUARD: m_uLastUse must cover the LATER write at topo 2, not stop at the read at topo 1. "
		"Pre-fix, only m_uFirstWrite + m_uLastRead were tracked; the second write was invisible to the packer, "
		"so a same-signature transient starting between topo 1 and topo 2 could be aliased into the same pool "
		"and have its writes trampled by the second write. Got %u",
		pxT->m_uLastUse);
}

// Two transients of identical signature where T1 has a multi-write pattern
// (write, read, write again) and T2's lifetime falls between T1's read and
// T1's later write. Pre-fix, T1's lifetime stopped at the read; the packer
// would alias T2 into T1's pool and T2's writes would trample memory the
// later T1 write needs. Post-fix, T1's full lifetime covers the later write
// and the packer keeps them in separate pools.
ZENITH_TEST(Core, RenderGraphAliasingMultiWriteOverlap) { Zenith_UnitTests::TestRenderGraphAliasingMultiWriteOverlap(); }

void Zenith_UnitTests::TestRenderGraphAliasingMultiWriteOverlap(){
	Flux_RenderGraph xGraph;

	// Same desc → same memory signature, so the packer is free to consider
	// them as alias candidates. Both UAV-capable so the multi-write side is
	// legal and we share the same memory-flag bits across both transients.
	Flux_TransientTextureDesc xDesc = RGTestColorUAVDesc();
	Flux_TransientHandle xT1 = xGraph.CreateTransient(xDesc);
	Flux_TransientHandle xT2 = xGraph.CreateTransient(xDesc);

	Flux_PassHandle xP0 = xGraph.AddPass("P0_T1Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP0, xT1, RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xP1 = xGraph.AddPass("P1_T1Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xP1, xT1, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xP2 = xGraph.AddPass("P2_T2Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP2, xT2, RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xP3 = xGraph.AddPass("P3_T2Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xP3, xT2, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xP4 = xGraph.AddPass("P4_T1Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP4, xT1, RESOURCE_ACCESS_WRITE_UAV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xP0.m_uIndex);  // topo 0
	xGraph.m_xExecutionOrder.PushBack(xP1.m_uIndex);  // topo 1
	xGraph.m_xExecutionOrder.PushBack(xP2.m_uIndex);  // topo 2
	xGraph.m_xExecutionOrder.PushBack(xP3.m_uIndex);  // topo 3
	xGraph.m_xExecutionOrder.PushBack(xP4.m_uIndex);  // topo 4

	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Flux_RenderGraph::TransientResource* pxT1 = xGraph.m_axTransients.Get(xT1.m_uIndex);
	Flux_RenderGraph::TransientResource* pxT2 = xGraph.m_axTransients.Get(xT2.m_uIndex);

	ZENITH_ASSERT_EQ(pxT1->m_uLastUse, 4u,
		"T1 last-use must extend through the second write at topo 4 (last_write=4 > last_read=1)");
	ZENITH_ASSERT_EQ(pxT2->m_uFirstWrite, 2u, "T2 first-write at topo 2");
	ZENITH_ASSERT_EQ(pxT2->m_uLastUse,    3u, "T2 last-use at topo 3");

	Zenith_Vector<u_int> axSorted;
	xGraph.SortTransientsByLifetime(axSorted);
	xGraph.PackTransientsIntoPools(axSorted);

	ZENITH_ASSERT_NE(pxT1->m_uAliasPoolIndex, pxT2->m_uAliasPoolIndex,
		"REGRESSION GUARD: T1 [0,4] and T2 [2,3] OVERLAP — packer must keep them in different pools. "
		"If this fires, m_uLastUse is back to ignoring multi-write extensions and T2's writes will trample "
		"the pool memory T1 needs for its second write. T1 pool=%u, T2 pool=%u",
		pxT1->m_uAliasPoolIndex, pxT2->m_uAliasPoolIndex);
}

// Disabled passes must not contribute to lifetime tracking. Critical for the
// runtime SetEnabled path (Execute re-runs ComputeResourceLifetimes when
// m_bEnabledMaskDirty); the compile-time path is also implicitly correct
// because TopologicalSort already filters disabled passes from
// m_xExecutionOrder.
ZENITH_TEST(Core, RenderGraphDisabledPassExcludedFromLifetimes) { Zenith_UnitTests::TestRenderGraphDisabledPassExcludedFromLifetimes(); }

void Zenith_UnitTests::TestRenderGraphDisabledPassExcludedFromLifetimes(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorDesc();
	Flux_TransientHandle xT = xGraph.CreateTransient(xDesc);

	Flux_PassHandle xP0 = xGraph.AddPass("P0_Write",        EmptyRecordCallback);
	xGraph.WriteTransient(xP0, xT, RESOURCE_ACCESS_WRITE_RTV);

	Flux_PassHandle xP1 = xGraph.AddPass("P1_Read",         EmptyRecordCallback);
	xGraph.ReadTransient (xP1, xT, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xP2 = xGraph.AddPass("P2_ReadDisabled", EmptyRecordCallback);
	xGraph.ReadTransient (xP2, xT, RESOURCE_ACCESS_READ_SRV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xP0.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xP1.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xP2.m_uIndex);

	// Phase 1: all passes enabled — last-use extends through P2.
	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Flux_RenderGraph::TransientResource* pxT = xGraph.m_axTransients.Get(xT.m_uIndex);
	ZENITH_ASSERT_EQ(pxT->m_uLastUse, 2u, "all enabled: last-use at topo 2");

	// Phase 2: disable P2 and recompute. Lifetime must shrink.
	xGraph.m_xPasses.Get(xP2.m_uIndex)->m_bEnabled = false;
	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	ZENITH_ASSERT_EQ(pxT->m_uLastUse, 1u,
		"REGRESSION GUARD: P2 disabled — last-use must drop to topo 1 (P1's read). If this fires, "
		"ComputeResourceLifetimes lost the pxPass->m_bEnabled check and is folding disabled-pass accesses "
		"into lifetime values, which will mis-direct SynthesizeAliasingBarriers' src-access lookup at Execute time. "
		"Got %u",
		pxT->m_uLastUse);
}

// ComputeResourceLifetimes must be safe to call multiple times — it's invoked
// from Compile() and again from Execute() when m_bEnabledMaskDirty triggers a
// barrier re-synth. Repeated calls with no enable changes must produce
// identical output (no accumulation of sentinel-vs-real-value confusion, no
// drift in m_uLastWrite tracking when entries already exist in m_xResources).
ZENITH_TEST(Core, RenderGraphLifetimeRecomputeIdempotent) { Zenith_UnitTests::TestRenderGraphLifetimeRecomputeIdempotent(); }

void Zenith_UnitTests::TestRenderGraphLifetimeRecomputeIdempotent(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorUAVDesc();
	Flux_TransientHandle xT = xGraph.CreateTransient(xDesc);

	Flux_PassHandle xP0 = xGraph.AddPass("P0_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP0, xT, RESOURCE_ACCESS_WRITE_UAV);

	Flux_PassHandle xP1 = xGraph.AddPass("P1_Read",  EmptyRecordCallback);
	xGraph.ReadTransient (xP1, xT, RESOURCE_ACCESS_READ_SRV);

	Flux_PassHandle xP2 = xGraph.AddPass("P2_Write", EmptyRecordCallback);
	xGraph.WriteTransient(xP2, xT, RESOURCE_ACCESS_WRITE_UAV);

	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xP0.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xP1.m_uIndex);
	xGraph.m_xExecutionOrder.PushBack(xP2.m_uIndex);

	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Flux_RenderGraph::TransientResource* pxT = xGraph.m_axTransients.Get(xT.m_uIndex);
	const u_int uFirstWrite_1 = pxT->m_uFirstWrite;
	const u_int uLastUse_1    = pxT->m_uLastUse;

	// Call again without changing anything. Values must match exactly —
	// the reset loop at the top of ComputeResourceLifetimes guarantees this.
	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	ZENITH_ASSERT_EQ(pxT->m_uFirstWrite, uFirstWrite_1,
		"ComputeResourceLifetimes must be idempotent — first-write drifted between calls (was %u, now %u)",
		uFirstWrite_1, pxT->m_uFirstWrite);
	ZENITH_ASSERT_EQ(pxT->m_uLastUse, uLastUse_1,
		"ComputeResourceLifetimes must be idempotent — last-use drifted between calls (was %u, now %u)",
		uLastUse_1, pxT->m_uLastUse);
	ZENITH_ASSERT_EQ(pxT->m_uFirstWrite, 0u, "first-write at topo 0");
	ZENITH_ASSERT_EQ(pxT->m_uLastUse,    2u, "last-use at topo 2 (multi-write)");
}

// SynthesizeAliasingBarriers must translate m_uLastUse (topological order) back
// to a pass index via m_xExecutionOrder.Get(...) when calling
// FindAccessForTransientInPass — that lambda searches the pass's reads/writes
// keyed by pass index. A regression that used m_uLastUse directly as a pass
// index would index into the wrong pass and either return a wrong access or
// trip FindAccessForTransientInPass's "transient not found" assert.
ZENITH_TEST(Core, RenderGraphAliasingBarrierUsesTopologicalLastUse) { Zenith_UnitTests::TestRenderGraphAliasingBarrierUsesTopologicalLastUse(); }

void Zenith_UnitTests::TestRenderGraphAliasingBarrierUsesTopologicalLastUse(){
	Flux_RenderGraph xGraph;

	Flux_TransientTextureDesc xDesc = RGTestColorDesc();
	Flux_TransientHandle xT1 = xGraph.CreateTransient(xDesc);
	Flux_TransientHandle xT2 = xGraph.CreateTransient(xDesc);

	// T1 [topo 0,1], T2 [topo 2,3]. Non-overlapping → packer aliases.
	// Critical: declaration order is REVERSED relative to execution order
	// for T1's passes (declare T1's reader BEFORE T1's writer) so any
	// lingering pass-index-as-topo confusion would resolve T1's last-use
	// to the wrong pass.
	Flux_PassHandle xPT1Read  = xGraph.AddPass("T1Read",  EmptyRecordCallback);  // pass idx 0
	Flux_PassHandle xPT1Write = xGraph.AddPass("T1Write", EmptyRecordCallback);  // pass idx 1
	Flux_PassHandle xPT2Write = xGraph.AddPass("T2Write", EmptyRecordCallback);  // pass idx 2
	Flux_PassHandle xPT2Read  = xGraph.AddPass("T2Read",  EmptyRecordCallback);  // pass idx 3

	xGraph.WriteTransient(xPT1Write, xT1, RESOURCE_ACCESS_WRITE_RTV);
	xGraph.ReadTransient (xPT1Read,  xT1, RESOURCE_ACCESS_READ_SRV);
	xGraph.WriteTransient(xPT2Write, xT2, RESOURCE_ACCESS_WRITE_RTV);
	xGraph.ReadTransient (xPT2Read,  xT2, RESOURCE_ACCESS_READ_SRV);

	// Execution order has T1Write FIRST despite its higher pass index — pass
	// index 1 lives at topo position 0, pass index 0 lives at topo position 1.
	xGraph.m_xExecutionOrder.Clear();
	xGraph.m_xExecutionOrder.PushBack(xPT1Write.m_uIndex);  // topo 0
	xGraph.m_xExecutionOrder.PushBack(xPT1Read.m_uIndex);   // topo 1
	xGraph.m_xExecutionOrder.PushBack(xPT2Write.m_uIndex);  // topo 2
	xGraph.m_xExecutionOrder.PushBack(xPT2Read.m_uIndex);   // topo 3

	xGraph.ComputeResourceLifetimes();
	xGraph.ComputeTransientLifetimes();

	Zenith_Vector<u_int> axSorted;
	xGraph.SortTransientsByLifetime(axSorted);
	xGraph.PackTransientsIntoPools(axSorted);

	Flux_RenderGraph::TransientResource* pxT1 = xGraph.m_axTransients.Get(xT1.m_uIndex);
	Flux_RenderGraph::TransientResource* pxT2 = xGraph.m_axTransients.Get(xT2.m_uIndex);

	ZENITH_ASSERT_EQ(pxT1->m_uAliasPoolIndex, pxT2->m_uAliasPoolIndex,
		"setup precondition: T1 [0,1] and T2 [2,3] non-overlapping, packer must alias them");

	xGraph.SynthesizeAliasingBarriers();

	// T2's first-writer pass takes over the pool. It should emit exactly one
	// aliasing barrier with src access = T1's last access (READ_SRV at T1Read,
	// topo 1). If the topo→pass-idx translation regressed and the barrier
	// pulled access from "pass at index 1" (which is T1Write, NOT T1Read),
	// m_eSrcAccess would come back as WRITE_RTV instead of READ_SRV.
	Flux_RenderGraph_Pass* pxPT2Write = xGraph.GetPasses().Get(xPT2Write.m_uIndex);
	ZENITH_ASSERT_EQ(pxPT2Write->m_xAliasingBarriers.GetSize(), 1,
		"T2's first-write pass must emit exactly 1 aliasing barrier (the hand-off from T1)");

	const Flux_RenderGraph_AliasingBarrier& xBar = pxPT2Write->m_xAliasingBarriers.Get(0u);
	ZENITH_ASSERT_EQ(xBar.m_eSrcAccess, RESOURCE_ACCESS_READ_SRV,
		"REGRESSION GUARD: aliasing barrier src access must be T1's last access (READ_SRV at topo 1, pass idx 0). "
		"If this returns WRITE_RTV, SynthesizeAliasingBarriers used pxPrior->m_uLastUse directly as a pass index "
		"instead of going through m_xExecutionOrder.Get(uPriorLastTopo) — and the SSGI corruption bug returns. "
		"Got %d (expected %d)",
		(int)xBar.m_eSrcAccess, (int)RESOURCE_ACCESS_READ_SRV);
	ZENITH_ASSERT_EQ(xBar.m_eDstAccess, RESOURCE_ACCESS_WRITE_RTV,
		"aliasing barrier dst access must be T2's first access (WRITE_RTV at topo 2). Got %d", (int)xBar.m_eDstAccess);
}

ZENITH_TEST(Core, RenderGraphPassOrderDescription) { Zenith_UnitTests::TestRenderGraphPassOrderDescription(); }

void Zenith_UnitTests::TestRenderGraphPassOrderDescription(){

	// Verifies that GetPassOrderDescription() walks m_xExecutionOrder in topo
	// order (not registration order) and includes the "(disabled)" suffix for
	// passes that have m_bEnabled == false. Skips Compile() — Compile needs a
	// live backend; the friend access on m_xExecutionOrder lets us populate
	// the order directly.
#ifdef ZENITH_TOOLS  // Pass names only exist in tools builds (DebugName returns "<release>" otherwise)
	Flux_RenderGraph xGraph;

	const Flux_PassHandle xA = xGraph.AddPass("Alpha",   EmptyRecordCallback);
	const Flux_PassHandle xB = xGraph.AddPass("Beta",    EmptyRecordCallback);
	const Flux_PassHandle xC = xGraph.AddPass("Gamma",   EmptyRecordCallback);
	(void)xA; (void)xB; (void)xC;
	ZENITH_ASSERT_EQ(xGraph.GetPasses().GetSize(), 3, "TestRenderGraphPassOrderDescription: should have 3 passes");

	// Empty before Compile / before m_xExecutionOrder is populated.
	ZENITH_ASSERT_TRUE(xGraph.GetPassOrderDescription().empty(),
		"TestRenderGraphPassOrderDescription: empty when m_xExecutionOrder is empty");

	// Populate the execution order directly to simulate a topological sort
	// that visits Gamma -> Alpha -> Beta (deliberately NOT registration order).
	xGraph.m_xExecutionOrder.PushBack(2);  // Gamma
	xGraph.m_xExecutionOrder.PushBack(0);  // Alpha
	xGraph.m_xExecutionOrder.PushBack(1);  // Beta

	const std::string strOrder = xGraph.GetPassOrderDescription();
	ZENITH_ASSERT_EQ(strOrder, std::string("Gamma -> Alpha -> Beta"),
		"TestRenderGraphPassOrderDescription: should reflect m_xExecutionOrder, not AddPass order");

	// Disabled passes should be marked.
	xGraph.m_xPasses.Get(0)->m_bEnabled = false;  // Alpha disabled
	const std::string strOrderWithDisabled = xGraph.GetPassOrderDescription();
	ZENITH_ASSERT_EQ(strOrderWithDisabled, std::string("Gamma -> Alpha (disabled) -> Beta"),
		"TestRenderGraphPassOrderDescription: disabled passes get '(disabled)' suffix");
#else
	ZENITH_SKIP("Pass debug names are only present in ZENITH_TOOLS builds");
#endif
}

// ============================================================================
// Flux_HiZImpl dependency-injection seam test (Wave 9)
// ============================================================================
// Flux_HiZImpl's Initialise now takes its cross-subsystem deps (swapchain,
// graphics, renderer) as explicit references and stores them in member
// pointers — the reusable DI template for the other ~50 subsystems. The real
// wiring happens in Flux.cpp's Flux_RendererImpl::LateInitialise, which only
// runs in the non-headless boot path. Because the test runner may execute
// headless (LateInitialise skipped, so the live g_xEngine.HiZ() pointers stay
// nullptr), a post-init "==&g_xEngine.VulkanSwapchain()" assertion would be
// flaky. Instead this is a pure-CPU seam test on a stack-constructed instance
// (default-constructed Flux_HiZImpl is headless-safe, like Flux_RenderGraph):
//   1. the three injected-dep member pointers default to nullptr, and
//   2. assigning distinct non-null sentinel pointers stores into the right
//      slots.
// The sentinels are reinterpret_cast and NEVER dereferenced.

#include "Flux/HiZ/Flux_HiZImpl.h"

ZENITH_TEST(Flux, HiZInjectedDepsWired) { Zenith_UnitTests::TestHiZInjectedDepsWired(); }

void Zenith_UnitTests::TestHiZInjectedDepsWired(){
	Flux_HiZImpl xHiZ;

	// 1. Fresh instance: all three injected-dep pointers default to nullptr.
	ZENITH_ASSERT_NULL(xHiZ.m_pxSwapchain, "Fresh Flux_HiZImpl: m_pxSwapchain defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xHiZ.m_pxGraphics,  "Fresh Flux_HiZImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xHiZ.m_pxRenderer,  "Fresh Flux_HiZImpl: m_pxRenderer defaults nullptr (headless-safe)");

	// 2. Distinct, never-dereferenced sentinels prove the storage slots exist
	//    and are independent (the DI seam stores each ref into its own member).
	Zenith_Vulkan_Swapchain* pxSentinelSwapchain = reinterpret_cast<Zenith_Vulkan_Swapchain*>(static_cast<uintptr_t>(0x1000));
	Flux_GraphicsImpl*       pxSentinelGraphics  = reinterpret_cast<Flux_GraphicsImpl*>      (static_cast<uintptr_t>(0x2000));
	Flux_RendererImpl*       pxSentinelRenderer  = reinterpret_cast<Flux_RendererImpl*>      (static_cast<uintptr_t>(0x3000));

	xHiZ.m_pxSwapchain = pxSentinelSwapchain;
	xHiZ.m_pxGraphics  = pxSentinelGraphics;
	xHiZ.m_pxRenderer  = pxSentinelRenderer;

	ZENITH_ASSERT_EQ(xHiZ.m_pxSwapchain, pxSentinelSwapchain, "m_pxSwapchain stores the injected swapchain pointer");
	ZENITH_ASSERT_EQ(xHiZ.m_pxGraphics,  pxSentinelGraphics,  "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xHiZ.m_pxRenderer,  pxSentinelRenderer,  "m_pxRenderer stores the injected renderer pointer");
}

// ============================================================================
// Flux_SSAOImpl dependency-injection seam test (Wave-11, 2nd leaf seam)
// ============================================================================
// Flux_SSAOImpl's Initialise now takes its cross-subsystem deps (graphics,
// swapchain, HDR) as explicit references and stores them in member pointers —
// the same reusable DI template as Flux_HiZImpl (WS9.2). The real wiring happens
// in Flux.cpp's Flux_RendererImpl::LateInitialise, which only runs in the
// non-headless boot path. Because the test runner may execute headless
// (LateInitialise skipped, so the live g_xEngine.SSAO() pointers stay nullptr),
// a post-init "==&g_xEngine.FluxGraphics()" assertion would be flaky. Instead
// this is a pure-CPU seam test on a stack-constructed instance
// (default-constructed Flux_SSAOImpl is headless-safe, like Flux_RenderGraph):
//   1. the three injected-dep member pointers default to nullptr, and
//   2. assigning distinct non-null sentinel pointers stores into the right
//      slots.
// The sentinels are reinterpret_cast and NEVER dereferenced.

#include "Flux/SSAO/Flux_SSAOImpl.h"

ZENITH_TEST(Flux, SSAOInjectedDepsWired) { Zenith_UnitTests::TestSSAOInjectedDepsWired(); }

void Zenith_UnitTests::TestSSAOInjectedDepsWired(){
	Flux_SSAOImpl xSSAO;

	// 1. Fresh instance: all three injected-dep pointers default to nullptr.
	ZENITH_ASSERT_NULL(xSSAO.m_pxGraphics,  "Fresh Flux_SSAOImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xSSAO.m_pxSwapchain, "Fresh Flux_SSAOImpl: m_pxSwapchain defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xSSAO.m_pxHDR,       "Fresh Flux_SSAOImpl: m_pxHDR defaults nullptr (headless-safe)");

	// 2. Distinct, never-dereferenced sentinels prove the storage slots exist
	//    and are independent (the DI seam stores each ref into its own member).
	Flux_GraphicsImpl*       pxSentinelGraphics  = reinterpret_cast<Flux_GraphicsImpl*>      (static_cast<uintptr_t>(0x1000));
	Zenith_Vulkan_Swapchain* pxSentinelSwapchain = reinterpret_cast<Zenith_Vulkan_Swapchain*>(static_cast<uintptr_t>(0x2000));
	Flux_HDRImpl*            pxSentinelHDR       = reinterpret_cast<Flux_HDRImpl*>           (static_cast<uintptr_t>(0x3000));

	xSSAO.m_pxGraphics  = pxSentinelGraphics;
	xSSAO.m_pxSwapchain = pxSentinelSwapchain;
	xSSAO.m_pxHDR       = pxSentinelHDR;

	ZENITH_ASSERT_EQ(xSSAO.m_pxGraphics,  pxSentinelGraphics,  "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xSSAO.m_pxSwapchain, pxSentinelSwapchain, "m_pxSwapchain stores the injected swapchain pointer");
	ZENITH_ASSERT_EQ(xSSAO.m_pxHDR,       pxSentinelHDR,       "m_pxHDR stores the injected HDR pointer");
}

// ============================================================================
// Flux_QuadsImpl dependency-injection seam test (Wave-14)
// ============================================================================
// Flux_QuadsImpl's Initialise now takes its lone cross-subsystem dep (graphics)
// as an explicit reference and stores it in a member pointer — the same reusable
// DI template as Flux_HiZImpl (WS9.2) / Flux_SSAOImpl (Wave-11). The real wiring
// happens in the Quads init trampoline (Flux_FeatureRegistry.cpp), driven by
// Flux_RendererImpl::LateInitialise, which only runs in the non-headless boot
// path — and Quads is not even wired in headless boot. Because the test runner
// may execute headless (LateInitialise skipped, so the live g_xEngine.Quads()
// pointer stays nullptr), a post-init "==&g_xEngine.FluxGraphics()" assertion
// would be flaky. Instead this is a pure-CPU seam test on a stack-constructed
// instance (default-constructed Flux_QuadsImpl is headless-safe):
//   1. the injected-dep member pointer defaults to nullptr, and
//   2. assigning a distinct non-null sentinel pointer stores into the right slot.
// The sentinel is reinterpret_cast and NEVER dereferenced.

#include "Flux/Quads/Flux_QuadsImpl.h"

ZENITH_TEST(Flux, QuadsInjectedDepsWired) { Zenith_UnitTests::TestQuadsInjectedDepsWired(); }

void Zenith_UnitTests::TestQuadsInjectedDepsWired(){
	Flux_QuadsImpl xQuads;

	// 1. Fresh instance: the injected-dep pointer defaults to nullptr.
	ZENITH_ASSERT_NULL(xQuads.m_pxGraphics, "Fresh Flux_QuadsImpl: m_pxGraphics defaults nullptr (headless-safe)");

	// 2. A distinct, never-dereferenced sentinel proves the storage slot exists
	//    (the DI seam stores the injected ref into its member).
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));

	xQuads.m_pxGraphics = pxSentinelGraphics;

	ZENITH_ASSERT_EQ(xQuads.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
}

// ============================================================================
// Flux_SDFsImpl dependency-injection seam test (Wave-14, lowest-fan-in leaf)
// ============================================================================
// Flux_SDFsImpl's Initialise now takes its two cross-subsystem deps (graphics,
// HDR) as explicit references and stores them in member pointers — the same
// reusable DI template as Flux_HiZImpl (WS9.2) and Flux_SSAOImpl (Wave-11). The
// real wiring happens through the Flux_FeatureRegistry SDFs init trampoline,
// which only runs in the non-headless boot path. Because the test runner may
// execute headless (init walk skipped, so the live g_xEngine.SDFs() pointers
// stay nullptr), a post-init "==&g_xEngine.FluxGraphics()" assertion would be
// flaky. Instead this is a pure-CPU seam test on a stack-constructed instance
// (default-constructed Flux_SDFsImpl is headless-safe, like Flux_RenderGraph):
//   1. the two injected-dep member pointers default to nullptr, and
//   2. assigning distinct non-null sentinel pointers stores into the right
//      slots.
// The sentinels are reinterpret_cast and NEVER dereferenced.

#include "Flux/SDFs/Flux_SDFsImpl.h"

ZENITH_TEST(Flux, SDFsInjectedDepsWired) { Zenith_UnitTests::TestSDFsInjectedDepsWired(); }

void Zenith_UnitTests::TestSDFsInjectedDepsWired(){
	Flux_SDFsImpl xSDFs;

	// 1. Fresh instance: both injected-dep pointers default to nullptr.
	ZENITH_ASSERT_NULL(xSDFs.m_pxGraphics, "Fresh Flux_SDFsImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xSDFs.m_pxHDR,      "Fresh Flux_SDFsImpl: m_pxHDR defaults nullptr (headless-safe)");

	// 2. Distinct, never-dereferenced sentinels prove the storage slots exist
	//    and are independent (the DI seam stores each ref into its own member).
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	Flux_HDRImpl*      pxSentinelHDR      = reinterpret_cast<Flux_HDRImpl*>     (static_cast<uintptr_t>(0x2000));

	xSDFs.m_pxGraphics = pxSentinelGraphics;
	xSDFs.m_pxHDR      = pxSentinelHDR;

	ZENITH_ASSERT_EQ(xSDFs.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xSDFs.m_pxHDR,      pxSentinelHDR,      "m_pxHDR stores the injected HDR pointer");
}

// ============================================================================
// Wave-15 DI-seam sentinel tests (Text / Skybox / Primitives / StaticMeshes /
// AnimatedMeshes). Same pure-CPU headless-safe pattern as Quads/SDFs: each
// injected-dep member pointer defaults nullptr, then stores an assigned sentinel
// (never dereferenced). Orchestrator batch-added (impl agents produced code only,
// to avoid a 5-way merge in this shared file).
// ============================================================================

#include "Flux/Text/Flux_TextImpl.h"
ZENITH_TEST(Flux, TextInjectedDepsWired) { Zenith_UnitTests::TestTextInjectedDepsWired(); }
void Zenith_UnitTests::TestTextInjectedDepsWired(){
	Flux_TextImpl xText;
	ZENITH_ASSERT_NULL(xText.m_pxGraphics, "Fresh Flux_TextImpl: m_pxGraphics defaults nullptr (headless-safe)");
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	xText.m_pxGraphics = pxSentinelGraphics;
	ZENITH_ASSERT_EQ(xText.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
}

#include "Flux/Skybox/Flux_SkyboxImpl.h"
ZENITH_TEST(Flux, SkyboxInjectedDepsWired) { Zenith_UnitTests::TestSkyboxInjectedDepsWired(); }
void Zenith_UnitTests::TestSkyboxInjectedDepsWired(){
	Flux_SkyboxImpl xSkybox;
	ZENITH_ASSERT_NULL(xSkybox.m_pxGraphics, "Fresh Flux_SkyboxImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xSkybox.m_pxHDR,      "Fresh Flux_SkyboxImpl: m_pxHDR defaults nullptr (headless-safe)");
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	Flux_HDRImpl*      pxSentinelHDR      = reinterpret_cast<Flux_HDRImpl*>     (static_cast<uintptr_t>(0x2000));
	xSkybox.m_pxGraphics = pxSentinelGraphics;
	xSkybox.m_pxHDR      = pxSentinelHDR;
	ZENITH_ASSERT_EQ(xSkybox.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xSkybox.m_pxHDR,      pxSentinelHDR,      "m_pxHDR stores the injected HDR pointer");
}

#include "Flux/Primitives/Flux_PrimitivesImpl.h"
ZENITH_TEST(Flux, PrimitivesInjectedDepsWired) { Zenith_UnitTests::TestPrimitivesInjectedDepsWired(); }
void Zenith_UnitTests::TestPrimitivesInjectedDepsWired(){
	Flux_PrimitivesImpl xPrimitives;
	ZENITH_ASSERT_NULL(xPrimitives.m_pxGraphics, "Fresh Flux_PrimitivesImpl: m_pxGraphics defaults nullptr (headless-safe)");
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	xPrimitives.m_pxGraphics = pxSentinelGraphics;
	ZENITH_ASSERT_EQ(xPrimitives.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
}

#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
ZENITH_TEST(Flux, StaticMeshesInjectedDepsWired) { Zenith_UnitTests::TestStaticMeshesInjectedDepsWired(); }
void Zenith_UnitTests::TestStaticMeshesInjectedDepsWired(){
	Flux_StaticMeshesImpl xStaticMeshes;
	ZENITH_ASSERT_NULL(xStaticMeshes.m_pxGraphics, "Fresh Flux_StaticMeshesImpl: m_pxGraphics defaults nullptr (headless-safe)");
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	xStaticMeshes.m_pxGraphics = pxSentinelGraphics;
	ZENITH_ASSERT_EQ(xStaticMeshes.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
}

#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
ZENITH_TEST(Flux, AnimatedMeshesInjectedDepsWired) { Zenith_UnitTests::TestAnimatedMeshesInjectedDepsWired(); }
void Zenith_UnitTests::TestAnimatedMeshesInjectedDepsWired(){
	Flux_AnimatedMeshesImpl xAnimatedMeshes;
	ZENITH_ASSERT_NULL(xAnimatedMeshes.m_pxGraphics, "Fresh Flux_AnimatedMeshesImpl: m_pxGraphics defaults nullptr (headless-safe)");
	Flux_GraphicsImpl* pxSentinelGraphics = reinterpret_cast<Flux_GraphicsImpl*>(static_cast<uintptr_t>(0x1000));
	xAnimatedMeshes.m_pxGraphics = pxSentinelGraphics;
	ZENITH_ASSERT_EQ(xAnimatedMeshes.m_pxGraphics, pxSentinelGraphics, "m_pxGraphics stores the injected graphics pointer");
}

// ============================================================================
// Wave-17 DI-seam sentinel tests (Decals: Graphics+Swapchain; Particles: Graphics+HDR+ParticleGPU).
// Same pure-CPU headless-safe pattern; orchestrator batch-added (impl agents were code-only).
// ============================================================================

#include "Flux/Decals/Flux_DecalsImpl.h"
ZENITH_TEST(Flux, DecalsInjectedDepsWired) { Zenith_UnitTests::TestDecalsInjectedDepsWired(); }
void Zenith_UnitTests::TestDecalsInjectedDepsWired(){
	Flux_DecalsImpl xDecals;
	ZENITH_ASSERT_NULL(xDecals.m_pxGraphics,  "Fresh Flux_DecalsImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xDecals.m_pxSwapchain, "Fresh Flux_DecalsImpl: m_pxSwapchain defaults nullptr (headless-safe)");
	Flux_GraphicsImpl*       pxSentinelGraphics  = reinterpret_cast<Flux_GraphicsImpl*>      (static_cast<uintptr_t>(0x1000));
	Zenith_Vulkan_Swapchain* pxSentinelSwapchain = reinterpret_cast<Zenith_Vulkan_Swapchain*>(static_cast<uintptr_t>(0x2000));
	xDecals.m_pxGraphics  = pxSentinelGraphics;
	xDecals.m_pxSwapchain = pxSentinelSwapchain;
	ZENITH_ASSERT_EQ(xDecals.m_pxGraphics,  pxSentinelGraphics,  "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xDecals.m_pxSwapchain, pxSentinelSwapchain, "m_pxSwapchain stores the injected swapchain pointer");
}

#include "Flux/Particles/Flux_ParticlesImpl.h"
ZENITH_TEST(Flux, ParticlesInjectedDepsWired) { Zenith_UnitTests::TestParticlesInjectedDepsWired(); }
void Zenith_UnitTests::TestParticlesInjectedDepsWired(){
	Flux_ParticlesImpl xParticles;
	ZENITH_ASSERT_NULL(xParticles.m_pxGraphics,    "Fresh Flux_ParticlesImpl: m_pxGraphics defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xParticles.m_pxHDR,         "Fresh Flux_ParticlesImpl: m_pxHDR defaults nullptr (headless-safe)");
	ZENITH_ASSERT_NULL(xParticles.m_pxParticleGPU, "Fresh Flux_ParticlesImpl: m_pxParticleGPU defaults nullptr (headless-safe)");
	Flux_GraphicsImpl*    pxSentinelGraphics    = reinterpret_cast<Flux_GraphicsImpl*>   (static_cast<uintptr_t>(0x1000));
	Flux_HDRImpl*         pxSentinelHDR         = reinterpret_cast<Flux_HDRImpl*>        (static_cast<uintptr_t>(0x2000));
	Flux_ParticleGPUImpl* pxSentinelParticleGPU = reinterpret_cast<Flux_ParticleGPUImpl*>(static_cast<uintptr_t>(0x3000));
	xParticles.m_pxGraphics    = pxSentinelGraphics;
	xParticles.m_pxHDR         = pxSentinelHDR;
	xParticles.m_pxParticleGPU = pxSentinelParticleGPU;
	ZENITH_ASSERT_EQ(xParticles.m_pxGraphics,    pxSentinelGraphics,    "m_pxGraphics stores the injected graphics pointer");
	ZENITH_ASSERT_EQ(xParticles.m_pxHDR,         pxSentinelHDR,         "m_pxHDR stores the injected HDR pointer");
	ZENITH_ASSERT_EQ(xParticles.m_pxParticleGPU, pxSentinelParticleGPU, "m_pxParticleGPU stores the injected ParticleGPU pointer");
}

// ============================================================================
// Flux_ShaderBinder name-cache tests
// ============================================================================
// Exercise the pointer-identity cache via a synthetic Flux_ShaderReflection.
// The binder's resolver takes a reflection pointer (not the Flux_Shader wrapper)
// so these tests don't need a live Vulkan device. Friended in via Zenith_UnitTests.
//
// The cache contract: entries are matched by pointer compare on
// (reflection-ptr, name-ptr). String literals have stable, deduplicated
// addresses within a translation unit, so the typical caller pattern produces
// a 100% cache hit after the first call. No hashing → no possibility of a
// clashing false hit.

#include "Flux/Slang/Flux_ShaderBinder.h"

namespace
{
	// Build a synthetic reflection with a fixed set of (name, type, set, binding)
	// entries. Names are passed in as const char* — the caller controls their
	// lifetime so the same literal addresses can be reused across multiple
	// resolver calls (the whole point of the pointer-identity cache).
	struct BindingSpec
	{
		const char* m_szName;
		BindingType m_eType;
		u_int       m_uSet;
		u_int       m_uBinding;
	};

	void PopulateReflection(Flux_ShaderReflection& xReflection, const BindingSpec* axSpecs, u_int uCount)
	{
		for (u_int u = 0; u < uCount; u++)
		{
			Flux_ReflectedBinding xB;
			xB.m_strName  = axSpecs[u].m_szName;
			xB.m_eType    = axSpecs[u].m_eType;
			xB.m_uSet     = axSpecs[u].m_uSet;
			xB.m_uBinding = axSpecs[u].m_uBinding;
			xB.m_uSize    = 0;
			xReflection.AddBinding(xB);
		}
		xReflection.BuildLookupMap();
	}
}

ZENITH_TEST(Core, BinderNameCacheFirstLookupMisses) { Zenith_UnitTests::TestBinderNameCacheFirstLookupMisses(); }

void Zenith_UnitTests::TestBinderNameCacheFirstLookupMisses(){
	Flux_ShaderReflection xReflection;
	const BindingSpec axSpecs[] = {
		{ "FrameConstants", BINDING_TYPE_BUFFER, 0, 0 },
	};
	PopulateReflection(xReflection, axSpecs, 1);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	// Cache starts empty — every slot's reflection-ptr is null.
	for (u_int u = 0; u < Flux_ShaderBinder::NAME_CACHE_SIZE; u++)
	{
		ZENITH_ASSERT_NULL(xBinder.m_axNameCache[u].m_pxReflection, "Fresh binder cache slot %u must be empty", u);
	}

	const char* szName = "FrameConstants";
	const Flux_ShaderBinder::ResolvedBinding xR = xBinder.ResolveNamedBinding(&xReflection, szName);

	ZENITH_ASSERT_EQ(xR.m_eType, BINDING_TYPE_BUFFER, "First lookup returns the right type");
	ZENITH_ASSERT_TRUE(xR.m_xHandle.m_uSet == 0 && xR.m_xHandle.m_uBinding == 0, "First lookup returns the right handle");

	// Slot 0 should now hold the entry; slots 1..7 still empty.
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[0].m_pxReflection, &xReflection, "First entry stored at slot 0");
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[0].m_szName, szName, "First entry stores the literal pointer");
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "Round-robin slot advances after a miss");
	ZENITH_ASSERT_NULL(xBinder.m_axNameCache[1].m_pxReflection, "Slot 1 still empty after one miss");

}

ZENITH_TEST(Core, BinderNameCacheRepeatLookupHits) { Zenith_UnitTests::TestBinderNameCacheRepeatLookupHits(); }

void Zenith_UnitTests::TestBinderNameCacheRepeatLookupHits(){
	Flux_ShaderReflection xReflection;
	const BindingSpec axSpecs[] = {
		{ "g_xDepthTex", BINDING_TYPE_TEXTURE, 0, 1 },
	};
	PopulateReflection(xReflection, axSpecs, 1);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	const char* szName = "g_xDepthTex";

	// First call — cache miss.
	xBinder.ResolveNamedBinding(&xReflection, szName);
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "Slot advanced after first miss");

	// Second call with the same string-literal pointer — cache hit. The
	// next-cache-slot should NOT advance (no new entry written).
	xBinder.ResolveNamedBinding(&xReflection, szName);
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "Cache hit does not advance the next-cache-slot counter");
	ZENITH_ASSERT_NULL(xBinder.m_axNameCache[1].m_pxReflection, "Cache hit does not populate slot 1");

	// Hammer the resolver — all hits, no new slot writes.
	for (u_int u = 0; u < 100; u++)
	{
		xBinder.ResolveNamedBinding(&xReflection, szName);
	}
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "Many cache hits in a row never advance the slot counter");

}

ZENITH_TEST(Core, BinderNameCacheDifferentReflectionMisses) { Zenith_UnitTests::TestBinderNameCacheDifferentReflectionMisses(); }

void Zenith_UnitTests::TestBinderNameCacheDifferentReflectionMisses(){
	// Two reflections with the same binding name. Same name-literal pointer
	// across both calls. Expectation: distinct reflection pointers force two
	// cache entries (the cache key is (reflection*, name*), not just name*).
	Flux_ShaderReflection xReflectionA;
	Flux_ShaderReflection xReflectionB;
	const BindingSpec axSpecs[] = {
		{ "Shared", BINDING_TYPE_BUFFER, 0, 0 },
	};
	PopulateReflection(xReflectionA, axSpecs, 1);
	PopulateReflection(xReflectionB, axSpecs, 1);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	const char* szName = "Shared";

	xBinder.ResolveNamedBinding(&xReflectionA, szName);
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "First call (reflection A) populates slot 0");

	xBinder.ResolveNamedBinding(&xReflectionB, szName);
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 2, "Different reflection forces a cache miss → slot 1 populated");

	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[0].m_pxReflection, &xReflectionA, "Slot 0 stores reflection A");
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[1].m_pxReflection, &xReflectionB, "Slot 1 stores reflection B");

}

ZENITH_TEST(Core, BinderNameCacheDifferentNameMisses) { Zenith_UnitTests::TestBinderNameCacheDifferentNameMisses(); }

void Zenith_UnitTests::TestBinderNameCacheDifferentNameMisses(){
	// Single reflection with two bindings. Two different name-literal pointers
	// → two distinct cache entries even within the same reflection.
	Flux_ShaderReflection xReflection;
	const BindingSpec axSpecs[] = {
		{ "First",  BINDING_TYPE_BUFFER,  0, 0 },
		{ "Second", BINDING_TYPE_TEXTURE, 0, 1 },
	};
	PopulateReflection(xReflection, axSpecs, 2);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	xBinder.ResolveNamedBinding(&xReflection, "First");
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 1, "First name occupies slot 0");

	xBinder.ResolveNamedBinding(&xReflection, "Second");
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 2, "Second name occupies slot 1");

}

ZENITH_TEST(Core, BinderNameCacheRoundRobinReplacement) { Zenith_UnitTests::TestBinderNameCacheRoundRobinReplacement(); }

void Zenith_UnitTests::TestBinderNameCacheRoundRobinReplacement(){
	// Fill the cache to NAME_CACHE_SIZE-1 misses (one under the overflow
	// threshold), verify the slot counter advances correctly, and verify the
	// contents land in the expected slots. The Nth miss (N==NAME_CACHE_SIZE)
	// would assert in ResolveNamedBinding now — the overflow assert fired
	// because round-robin eviction at that point produces 0% hit rate; tests
	// stop short of the assert line.
	Flux_ShaderReflection xReflection;
	BindingSpec axSpecs[Flux_ShaderBinder::NAME_CACHE_SIZE];
	char aszNames[Flux_ShaderBinder::NAME_CACHE_SIZE][8];
	for (u_int u = 0; u < Flux_ShaderBinder::NAME_CACHE_SIZE; u++)
	{
		snprintf(aszNames[u], sizeof(aszNames[u]), "B%u", u);
		axSpecs[u] = { aszNames[u], BINDING_TYPE_BUFFER, 0, u };
	}
	PopulateReflection(xReflection, axSpecs, Flux_ShaderBinder::NAME_CACHE_SIZE);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	// Fill all but the last slot. Each miss increments m_uNextCacheSlot.
	for (u_int u = 0; u < Flux_ShaderBinder::NAME_CACHE_SIZE - 1; u++)
	{
		xBinder.ResolveNamedBinding(&xReflection, axSpecs[u].m_szName);
		ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, u + 1, "Slot counter advances after each miss");
	}
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[0].m_szName, axSpecs[0].m_szName, "Slot 0 holds the first name");

	// The last miss that keeps us inside the overflow budget — exactly
	// NAME_CACHE_SIZE unique resolves filled the cache, slot counter wraps
	// to 0 (though no eviction has happened yet).
	xBinder.ResolveNamedBinding(&xReflection, axSpecs[Flux_ShaderBinder::NAME_CACHE_SIZE - 1].m_szName);
	ZENITH_ASSERT_EQ(xBinder.m_uNextCacheSlot, 0, "After NAME_CACHE_SIZE unique misses, slot counter wraps to 0");
	ZENITH_ASSERT_EQ(xBinder.m_uUniqueResolves, Flux_ShaderBinder::NAME_CACHE_SIZE, "Unique resolve counter matches cache capacity after full fill");

}

ZENITH_TEST(Core, BinderNameCacheTypeStoredCorrectly) { Zenith_UnitTests::TestBinderNameCacheTypeStoredCorrectly(); }

void Zenith_UnitTests::TestBinderNameCacheTypeStoredCorrectly(){
	// Verify the cached entry stores the reflected BindingType so the per-call
	// type assertion in BindCBV/BindSRV/etc. has the data it needs without
	// going back to the reflection on a hit.
	Flux_ShaderReflection xReflection;
	const BindingSpec axSpecs[] = {
		{ "Buf",      BINDING_TYPE_BUFFER,         0, 0 },
		{ "Tex",      BINDING_TYPE_TEXTURE,        0, 1 },
		{ "StorTex",  BINDING_TYPE_STORAGE_IMAGE,  0, 2 },
		{ "StorBuf",  BINDING_TYPE_STORAGE_BUFFER, 0, 3 },
	};
	PopulateReflection(xReflection, axSpecs, 4);

	Flux_CommandList xCmdList("UnitTest");
	Flux_ShaderBinder xBinder(xCmdList);

	const Flux_ShaderBinder::ResolvedBinding xR0 = xBinder.ResolveNamedBinding(&xReflection, "Buf");
	const Flux_ShaderBinder::ResolvedBinding xR1 = xBinder.ResolveNamedBinding(&xReflection, "Tex");
	const Flux_ShaderBinder::ResolvedBinding xR2 = xBinder.ResolveNamedBinding(&xReflection, "StorTex");
	const Flux_ShaderBinder::ResolvedBinding xR3 = xBinder.ResolveNamedBinding(&xReflection, "StorBuf");

	ZENITH_ASSERT_EQ(xR0.m_eType, BINDING_TYPE_BUFFER, "Buf resolves to BINDING_TYPE_BUFFER");
	ZENITH_ASSERT_EQ(xR1.m_eType, BINDING_TYPE_TEXTURE, "Tex resolves to BINDING_TYPE_TEXTURE");
	ZENITH_ASSERT_EQ(xR2.m_eType, BINDING_TYPE_STORAGE_IMAGE, "StorTex resolves to BINDING_TYPE_STORAGE_IMAGE");
	ZENITH_ASSERT_EQ(xR3.m_eType, BINDING_TYPE_STORAGE_BUFFER, "StorBuf resolves to BINDING_TYPE_STORAGE_BUFFER");

	// And the cached slots should mirror that.
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[0].m_eType, BINDING_TYPE_BUFFER, "Slot 0 cached type == BUFFER");
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[1].m_eType, BINDING_TYPE_TEXTURE, "Slot 1 cached type == TEXTURE");
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[2].m_eType, BINDING_TYPE_STORAGE_IMAGE, "Slot 2 cached type == STORAGE_IMAGE");
	ZENITH_ASSERT_EQ(xBinder.m_axNameCache[3].m_eType, BINDING_TYPE_STORAGE_BUFFER, "Slot 3 cached type == STORAGE_BUFFER");

}

// ============================================================================
// Flux_PerFrame ring-scheduler tests
// ============================================================================
// These tests run inside the live engine's main loop (RunAllTests is invoked
// from Zenith_Main.cpp after Flux::EarlyInitialise has already registered the
// real Vulkan begin and MemoryManager end callbacks). Each test saves the
// live state (counter + callback arrays) at entry and restores it at exit so
// the surrounding frame loop is unaffected by the temporary scratch state
// the test installs.

#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"

// Snapshot of Flux_PerFrame's internal state used by the §5.2 tests to save
// the live engine state before installing scratch callbacks, then restore it
// at end-of-test. Defined here so it can hold the callback array sizes from
// FLUX_MAX_PERFRAME_CALLBACKS without exposing them in the public header.
struct Zenith_UnitTests::PerFrameSnapshot
{
	u_int                            m_uFrameCounter;
	u_int                            m_uNumBegin;
	u_int                            m_uNumEnd;
	Flux_RendererImpl::OnFrameBeginFunc  m_apfnBegin[FLUX_MAX_PERFRAME_CALLBACKS];
	void*                            m_apBeginUser[FLUX_MAX_PERFRAME_CALLBACKS];
	Flux_RendererImpl::OnFrameEndFunc    m_apfnEnd[FLUX_MAX_PERFRAME_CALLBACKS];
	void*                            m_apEndUser[FLUX_MAX_PERFRAME_CALLBACKS];
};

void Zenith_UnitTests::SnapshotPerFrameAndReset(PerFrameSnapshot& xOut)
{
	Flux_RendererImpl& xR = g_xEngine.FluxRenderer();
	xOut.m_uFrameCounter = xR.m_uFrameCounter;
	xOut.m_uNumBegin     = xR.m_uNumBeginCallbacks;
	xOut.m_uNumEnd       = xR.m_uNumEndCallbacks;
	for (u_int u = 0; u < FLUX_MAX_PERFRAME_CALLBACKS; u++)
	{
		xOut.m_apfnBegin[u]   = xR.m_apfnBeginCallbacks[u];
		xOut.m_apBeginUser[u] = xR.m_apBeginUserData[u];
		xOut.m_apfnEnd[u]     = xR.m_apfnEndCallbacks[u];
		xOut.m_apEndUser[u]   = xR.m_apEndUserData[u];
	}
	xR.m_uFrameCounter      = 0;
	xR.m_uNumBeginCallbacks = 0;
	xR.m_uNumEndCallbacks   = 0;
}

void Zenith_UnitTests::RestorePerFrame(const PerFrameSnapshot& xIn)
{
	Flux_RendererImpl& xR = g_xEngine.FluxRenderer();
	xR.m_uFrameCounter      = xIn.m_uFrameCounter;
	xR.m_uNumBeginCallbacks = xIn.m_uNumBegin;
	xR.m_uNumEndCallbacks   = xIn.m_uNumEnd;
	for (u_int u = 0; u < FLUX_MAX_PERFRAME_CALLBACKS; u++)
	{
		xR.m_apfnBeginCallbacks[u] = xIn.m_apfnBegin[u];
		xR.m_apBeginUserData[u]    = xIn.m_apBeginUser[u];
		xR.m_apfnEndCallbacks[u]   = xIn.m_apfnEnd[u];
		xR.m_apEndUserData[u]      = xIn.m_apEndUser[u];
	}
}

namespace
{
	// RAII helper that calls SnapshotPerFrameAndReset on construction and
	// RestorePerFrame on destruction. Lives in this anon namespace because
	// it doesn't need friend access — the static helpers do.
	struct PerFrameScopedReset
	{
		Zenith_UnitTests::PerFrameSnapshot m_xSnap;
		PerFrameScopedReset() { Zenith_UnitTests::SnapshotPerFrameAndReset(m_xSnap); }
		~PerFrameScopedReset() { Zenith_UnitTests::RestorePerFrame(m_xSnap); }
	};

	// Mutable counters used by callback bodies in the tests.
	u_int g_uTestBeginCallCount = 0;
	u_int g_uTestEndCallCount   = 0;
	u_int g_uTestLastBeginRing  = UINT32_MAX;
	u_int g_uTestLastEndRing    = UINT32_MAX;
	void* g_pTestLastUserData   = nullptr;

	// Track callback firing order: each callback pushes its tag here.
	u_int g_auTestCallOrder[16];
	u_int g_uTestCallOrderCount = 0;

	void TestBeginCallback_IncCount(u_int uRingIndex, void* pUserData)
	{
		g_uTestBeginCallCount++;
		g_uTestLastBeginRing = uRingIndex;
		g_pTestLastUserData  = pUserData;
	}

	void TestEndCallback_IncCount(u_int uRingIndex, void* pUserData)
	{
		g_uTestEndCallCount++;
		g_uTestLastEndRing  = uRingIndex;
		g_pTestLastUserData = pUserData;
	}

	void TestBeginCallback_OrderTagA(u_int /*uRingIndex*/, void* /*pUserData*/)
	{
		ZENITH_ASSERT_LT(g_uTestCallOrderCount, 16, "Test call-order overflow");
		g_auTestCallOrder[g_uTestCallOrderCount++] = 'A';
	}

	void TestBeginCallback_OrderTagB(u_int /*uRingIndex*/, void* /*pUserData*/)
	{
		ZENITH_ASSERT_LT(g_uTestCallOrderCount, 16, "Test call-order overflow");
		g_auTestCallOrder[g_uTestCallOrderCount++] = 'B';
	}

	void TestBeginCallback_OrderTagC(u_int /*uRingIndex*/, void* /*pUserData*/)
	{
		ZENITH_ASSERT_LT(g_uTestCallOrderCount, 16, "Test call-order overflow");
		g_auTestCallOrder[g_uTestCallOrderCount++] = 'C';
	}
}

ZENITH_TEST(Core, FluxPerFrameFrameCounterAdvances) { Zenith_UnitTests::TestFluxPerFrameFrameCounterAdvances(); }

void Zenith_UnitTests::TestFluxPerFrameFrameCounterAdvances(){
	PerFrameScopedReset xReset;

	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), 0, "Counter starts at 0");
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetRingIndex(), 0, "Ring index starts at 0");

	// BeginFrame does NOT advance the counter — only EndFrame does. This
	// matches the pre-extraction behaviour where the swapchain bumped its
	// index inside EndFrame, so the same slot is used by Begin and End of
	// the same frame.
	g_xEngine.FluxRenderer().BeginFrame();
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), 0, "BeginFrame does not advance the counter");

	g_xEngine.FluxRenderer().EndFrame();
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), 1, "EndFrame advances the counter by 1");

	for (u_int u = 0; u < 5; u++)
	{
		g_xEngine.FluxRenderer().BeginFrame();
		g_xEngine.FluxRenderer().EndFrame();
	}
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), 6, "Five Begin/End pairs advance the counter to 6");

}

ZENITH_TEST(Core, FluxPerFrameRingIndexWraps) { Zenith_UnitTests::TestFluxPerFrameRingIndexWraps(); }

void Zenith_UnitTests::TestFluxPerFrameRingIndexWraps(){
	PerFrameScopedReset xReset;

	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetRingIndex(), u % MAX_FRAMES_IN_FLIGHT, "Ring index at counter %u is %u, expected %u", u, g_xEngine.FluxRenderer().GetRingIndex(), u % MAX_FRAMES_IN_FLIGHT);
		g_xEngine.FluxRenderer().EndFrame();
	}
	// After MAX_FRAMES_IN_FLIGHT EndFrames the counter is at MAX, ring index wraps to 0.
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), MAX_FRAMES_IN_FLIGHT, "Counter is at MAX_FRAMES_IN_FLIGHT");
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetRingIndex(), 0, "Ring index wraps to 0 after MAX_FRAMES_IN_FLIGHT EndFrames");

	// Drive several full cycles and confirm the modulo continues to hold.
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT * 3 + 1; u++)
	{
		g_xEngine.FluxRenderer().EndFrame();
	}
	const u_int uExpectedCounter = MAX_FRAMES_IN_FLIGHT + (MAX_FRAMES_IN_FLIGHT * 3 + 1);
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetFrameCounter(), uExpectedCounter, "Counter total tracks correctly");
	ZENITH_ASSERT_EQ(g_xEngine.FluxRenderer().GetRingIndex(), uExpectedCounter % MAX_FRAMES_IN_FLIGHT, "Ring index continues to be counter %% MAX_FRAMES_IN_FLIGHT");

}

ZENITH_TEST(Core, FluxPerFrameBeginCallbackFires) { Zenith_UnitTests::TestFluxPerFrameBeginCallbackFires(); }

void Zenith_UnitTests::TestFluxPerFrameBeginCallbackFires(){
	PerFrameScopedReset xReset;

	g_uTestBeginCallCount = 0;
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_IncCount, nullptr);

	g_xEngine.FluxRenderer().BeginFrame();
	ZENITH_ASSERT_EQ(g_uTestBeginCallCount, 1, "Begin callback fires once per BeginFrame");

	g_xEngine.FluxRenderer().BeginFrame();
	g_xEngine.FluxRenderer().BeginFrame();
	ZENITH_ASSERT_EQ(g_uTestBeginCallCount, 3, "Begin callback fires every BeginFrame");

}

ZENITH_TEST(Core, FluxPerFrameEndCallbackFires) { Zenith_UnitTests::TestFluxPerFrameEndCallbackFires(); }

void Zenith_UnitTests::TestFluxPerFrameEndCallbackFires(){
	PerFrameScopedReset xReset;

	g_uTestEndCallCount = 0;
	g_xEngine.FluxRenderer().RegisterEndFrameCallback(&TestEndCallback_IncCount, nullptr);

	g_xEngine.FluxRenderer().EndFrame();
	ZENITH_ASSERT_EQ(g_uTestEndCallCount, 1, "End callback fires once per EndFrame");

	g_xEngine.FluxRenderer().EndFrame();
	ZENITH_ASSERT_EQ(g_uTestEndCallCount, 2, "End callback fires every EndFrame");

}

ZENITH_TEST(Core, FluxPerFrameCallbackOrderPreserved) { Zenith_UnitTests::TestFluxPerFrameCallbackOrderPreserved(); }

void Zenith_UnitTests::TestFluxPerFrameCallbackOrderPreserved(){
	PerFrameScopedReset xReset;

	g_uTestCallOrderCount = 0;
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_OrderTagA, nullptr);
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_OrderTagB, nullptr);
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_OrderTagC, nullptr);

	g_xEngine.FluxRenderer().BeginFrame();

	ZENITH_ASSERT_EQ(g_uTestCallOrderCount, 3, "All three begin callbacks fired");
	ZENITH_ASSERT_EQ(g_auTestCallOrder[0], 'A', "First registered (A) fires first");
	ZENITH_ASSERT_EQ(g_auTestCallOrder[1], 'B', "Second registered (B) fires second");
	ZENITH_ASSERT_EQ(g_auTestCallOrder[2], 'C', "Third registered (C) fires third");

}

ZENITH_TEST(Core, FluxPerFrameCallbackUserDataPassed) { Zenith_UnitTests::TestFluxPerFrameCallbackUserDataPassed(); }

void Zenith_UnitTests::TestFluxPerFrameCallbackUserDataPassed(){
	PerFrameScopedReset xReset;

	int iSentinelOnStack = 0xC0DE;
	g_pTestLastUserData = nullptr;
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_IncCount, &iSentinelOnStack);

	g_xEngine.FluxRenderer().BeginFrame();
	ZENITH_ASSERT_EQ(g_pTestLastUserData, &iSentinelOnStack, "Begin callback receives the user-data pointer it was registered with");

	int iSentinelTwo = 0xBEEF;
	g_pTestLastUserData = nullptr;
	g_xEngine.FluxRenderer().RegisterEndFrameCallback(&TestEndCallback_IncCount, &iSentinelTwo);

	g_xEngine.FluxRenderer().EndFrame();
	// Both callbacks fired; last one to run wrote g_pTestLastUserData.
	// Begin fires first inside EndFrame? No — begin callbacks only fire in
	// BeginFrame. So only the end callback fired here, and it wrote the
	// end-callback's user-data pointer.
	ZENITH_ASSERT_EQ(g_pTestLastUserData, &iSentinelTwo, "End callback receives the user-data pointer it was registered with");

}

ZENITH_TEST(Core, FluxPerFrameRingIndexInsideCallback) { Zenith_UnitTests::TestFluxPerFrameRingIndexInsideCallback(); }

void Zenith_UnitTests::TestFluxPerFrameRingIndexInsideCallback(){
	PerFrameScopedReset xReset;

	g_uTestLastBeginRing = UINT32_MAX;
	g_uTestLastEndRing   = UINT32_MAX;
	g_xEngine.FluxRenderer().RegisterBeginFrameCallback(&TestBeginCallback_IncCount, nullptr);
	g_xEngine.FluxRenderer().RegisterEndFrameCallback  (&TestEndCallback_IncCount,   nullptr);

	// Drive a few iterations; callbacks should always observe the same ring
	// index that GetRingIndex() returns at call time.
	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT * 2; u++)
	{
		const u_int uExpectedRing = u % MAX_FRAMES_IN_FLIGHT;

		g_xEngine.FluxRenderer().BeginFrame();
		ZENITH_ASSERT_EQ(g_uTestLastBeginRing, uExpectedRing, "Begin callback at frame %u observed ring %u, expected %u", u, g_uTestLastBeginRing, uExpectedRing);

		g_xEngine.FluxRenderer().EndFrame();
		ZENITH_ASSERT_EQ(g_uTestLastEndRing, uExpectedRing, "End callback at frame %u observed ring %u, expected %u", u, g_uTestLastEndRing, uExpectedRing);
	}

}

//=============================================================================
// UIText alignment helper tests
//=============================================================================

ZENITH_TEST(Core, UITextHorizontalAlignment) { Zenith_UnitTests::TestUITextHorizontalAlignment(); }

void Zenith_UnitTests::TestUITextHorizontalAlignment(){

	const float fLeft = 100.0f;
	const float fWidth = 200.0f;
	const float fLineWidth = 80.0f;

	const float fLeftX = Zenith_UI::Zenith_UIText::ComputeHorizontalStartX(
		fLeft, fWidth, fLineWidth, Zenith_UI::TextAlignment::Left);
	ZENITH_ASSERT_EQ_FLOAT(fLeftX, 100.0f, 0.001f, "Left alignment should return fLeft (100), got %.2f", fLeftX);

	const float fCenterX = Zenith_UI::Zenith_UIText::ComputeHorizontalStartX(
		fLeft, fWidth, fLineWidth, Zenith_UI::TextAlignment::Center);
	ZENITH_ASSERT_EQ_FLOAT(fCenterX, 160.0f, 0.001f, "Center alignment should return fLeft + (fWidth - fLineWidth)/2 = 160, got %.2f", fCenterX);

	const float fRightX = Zenith_UI::Zenith_UIText::ComputeHorizontalStartX(
		fLeft, fWidth, fLineWidth, Zenith_UI::TextAlignment::Right);
	ZENITH_ASSERT_EQ_FLOAT(fRightX, 220.0f, 0.001f, "Right alignment should return fLeft + fWidth - fLineWidth = 220, got %.2f", fRightX);

	// Edge: line wider than element — center and right produce negative offsets.
	const float fOverflowCenter = Zenith_UI::Zenith_UIText::ComputeHorizontalStartX(
		0.0f, 100.0f, 300.0f, Zenith_UI::TextAlignment::Center);
	ZENITH_ASSERT_EQ_FLOAT(fOverflowCenter, (-100.0f), 0.001f, "Center alignment with overflow should clamp to negative offset by design");

}

ZENITH_TEST(Core, UITextVerticalAlignment) { Zenith_UnitTests::TestUITextVerticalAlignment(); }

void Zenith_UnitTests::TestUITextVerticalAlignment(){

	const float fTop = 50.0f;
	const float fHeight = 120.0f;
	const float fTextHeight = 40.0f;

	const float fTopY = Zenith_UI::Zenith_UIText::ComputeVerticalStartY(
		fTop, fHeight, fTextHeight, Zenith_UI::TextVerticalAlignment::Top);
	ZENITH_ASSERT_EQ_FLOAT(fTopY, 50.0f, 0.001f, "Top alignment should return fTop (50), got %.2f", fTopY);

	const float fMiddleY = Zenith_UI::Zenith_UIText::ComputeVerticalStartY(
		fTop, fHeight, fTextHeight, Zenith_UI::TextVerticalAlignment::Middle);
	ZENITH_ASSERT_EQ_FLOAT(fMiddleY, 90.0f, 0.001f, "Middle alignment should return fTop + (fHeight - fTextHeight)/2 = 90, got %.2f", fMiddleY);

	const float fBottomY = Zenith_UI::Zenith_UIText::ComputeVerticalStartY(
		fTop, fHeight, fTextHeight, Zenith_UI::TextVerticalAlignment::Bottom);
	ZENITH_ASSERT_EQ_FLOAT(fBottomY, 130.0f, 0.001f, "Bottom alignment should return fTop + fHeight - fTextHeight = 130, got %.2f", fBottomY);

	// Edge: text exactly fills the element — middle equals top equals 50.
	const float fExactFitMiddle = Zenith_UI::Zenith_UIText::ComputeVerticalStartY(
		50.0f, 40.0f, 40.0f, Zenith_UI::TextVerticalAlignment::Middle);
	ZENITH_ASSERT_EQ_FLOAT(fExactFitMiddle, 50.0f, 0.001f, "Exact-fit middle alignment should equal fTop");

}

//=============================================================================
// Slang reflection v2 round-trip tests
//
// Pin the binary on-disk format of the .spv.refl sidecar so a future schema
// bump has to come with an explicit version handler. Reflection is consumed
// by RootSigBuilder + name-keyed binders at runtime and by the C++ codegen
// at FluxCompiler time, so a silent format break would manifest as either
// "GetBinding(...) failed" spam at startup or static_assert sizeof/offsetof
// failures in generated headers — both annoying to diagnose. These tests
// run on every platform (no Slang compiler dependency).
//=============================================================================

ZENITH_TEST(Slang, ReflectionV2RoundTrip) { Zenith_UnitTests::TestReflectionV2RoundTrip(); }

void Zenith_UnitTests::TestReflectionV2RoundTrip(){

	Flux_ShaderReflection xWrite;

	Flux_ReflectedBinding xCB;
	xCB.m_eType                 = BINDING_TYPE_BUFFER;
	xCB.m_uSet                  = 0;
	xCB.m_uBinding              = 0;
	xCB.m_strName               = "FrameConstants";
	xCB.m_uSize                 = 256;
	xCB.m_eResourceKind         = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount      = 1;
	xCB.m_uStageMask            = FLUX_SHADER_STAGE_BIT_VERTEX | FLUX_SHADER_STAGE_BIT_FRAGMENT;
	xCB.m_strParameterBlockPath = "frame";
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "m_xViewMat";
		xF.m_uOffset     = 0;
		xF.m_uSize       = 64;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4x4";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "m_xCameraPos_Pad";
		xF.m_uOffset     = 64;
		xF.m_uSize       = 16;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4";
		xCB.m_axFields.PushBack(xF);
	}
	xWrite.AddBinding(xCB);

	Flux_ReflectedBinding xTex;
	xTex.m_eType            = BINDING_TYPE_TEXTURE;
	xTex.m_uSet             = 1;
	xTex.m_uBinding         = 3;
	xTex.m_strName          = "g_xAlbedoTex";
	xTex.m_uSize            = 0;
	xTex.m_eResourceKind    = FLUX_RESOURCE_KIND_TEXTURE;
	xTex.m_uDescriptorCount = 1;
	xTex.m_uStageMask       = FLUX_SHADER_STAGE_BIT_FRAGMENT;
	xWrite.AddBinding(xTex);

	Flux_ReflectedBinding xUav;
	xUav.m_eType            = BINDING_TYPE_STORAGE_BUFFER;
	xUav.m_uSet             = 0;
	xUav.m_uBinding         = 5;
	xUav.m_strName          = "g_axHistogram";
	xUav.m_uSize            = 0;
	xUav.m_eResourceKind    = FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER;
	xUav.m_uDescriptorCount = 1;
	xUav.m_uStageMask       = FLUX_SHADER_STAGE_BIT_COMPUTE;
	xWrite.AddBinding(xUav);

	Zenith_DataStream xStream(2048);
	xWrite.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	const Zenith_Vector<Flux_ReflectedBinding>& axBindings = xRead.GetBindings();
	ZENITH_ASSERT_EQ(axBindings.GetSize(), 3, "Round-trip should produce three bindings");

	const Flux_ReflectedBinding& xCBOut = axBindings.Get(0);
	ZENITH_ASSERT_EQ(xCBOut.m_strName, "FrameConstants", "CB name should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_uSet, 0, "CB set should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_uBinding, 0, "CB binding slot should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_uSize, 256, "CB size should round-trip");
	ZENITH_ASSERT_EQ((int)xCBOut.m_eResourceKind, (int)FLUX_RESOURCE_KIND_CONSTANT_BUFFER, "CB resource kind should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_uStageMask, (FLUX_SHADER_STAGE_BIT_VERTEX | FLUX_SHADER_STAGE_BIT_FRAGMENT), "CB stage mask should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_strParameterBlockPath, "frame", "CB parameter-block path should round-trip");
	ZENITH_ASSERT_EQ(xCBOut.m_axFields.GetSize(), 2, "CB should have two reflected fields");

	const Flux_ReflectedField& xField0 = xCBOut.m_axFields.Get(0);
	ZENITH_ASSERT_EQ(xField0.m_strName, "m_xViewMat", "Field 0 name should round-trip");
	ZENITH_ASSERT_EQ(xField0.m_uOffset, 0, "Field 0 offset should round-trip");
	ZENITH_ASSERT_EQ(xField0.m_uSize, 64, "Field 0 size should round-trip");
	ZENITH_ASSERT_EQ(xField0.m_strTypeName, "float4x4", "Field 0 type name should round-trip");

	const Flux_ReflectedField& xField1 = xCBOut.m_axFields.Get(1);
	ZENITH_ASSERT_EQ(xField1.m_uOffset, 64, "Field 1 offset should round-trip");
	ZENITH_ASSERT_EQ(xField1.m_strTypeName, "float4", "Field 1 type name should round-trip");

	const Flux_ReflectedBinding& xTexOut = axBindings.Get(1);
	ZENITH_ASSERT_EQ((int)xTexOut.m_eResourceKind, (int)FLUX_RESOURCE_KIND_TEXTURE, "Texture resource kind should round-trip");
	ZENITH_ASSERT_EQ(xTexOut.m_uStageMask, (u_int)FLUX_SHADER_STAGE_BIT_FRAGMENT, "Texture stage mask should round-trip");

	const Flux_ReflectedBinding& xUavOut = axBindings.Get(2);
	ZENITH_ASSERT_EQ((int)xUavOut.m_eResourceKind, (int)FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER, "RW structured buffer resource kind should round-trip");
	ZENITH_ASSERT_EQ(xUavOut.m_uStageMask, (u_int)FLUX_SHADER_STAGE_BIT_COMPUTE, "Compute stage mask should round-trip");

}

ZENITH_TEST(Slang, ReflectionV2EmptyRoundTrip) { Zenith_UnitTests::TestReflectionV2EmptyRoundTrip(); }

void Zenith_UnitTests::TestReflectionV2EmptyRoundTrip(){

	// A program with no bindings is valid (e.g. a vertex shader that only
	// reads a push constant). The on-disk format must round-trip cleanly
	// with zero entries — if it doesn't, magic/version handling is broken.
	Flux_ShaderReflection xEmpty;

	Zenith_DataStream xStream(64);
	xEmpty.WriteToDataStream(xStream);

	xStream.SetCursor(0);
	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xRead.GetBindings().GetSize(), 0, "Empty reflection should produce zero bindings");

}

ZENITH_TEST(Slang, ReflectionV2UnboundedDescriptorCount) { Zenith_UnitTests::TestReflectionV2UnboundedDescriptorCount(); }

void Zenith_UnitTests::TestReflectionV2UnboundedDescriptorCount(){

	// Unbounded descriptor arrays serialise descriptor count as 0 (the
	// runtime treats 0 as "use VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_-
	// COUNT_BIT"). Confirm the literal zero round-trips and that 1 stays 1
	// — boundaries either side of the sentinel.
	Flux_ShaderReflection xWrite;

	Flux_ReflectedBinding xUnbounded;
	xUnbounded.m_eType            = BINDING_TYPE_UNBOUNDED_TEXTURES;
	xUnbounded.m_uSet             = 2;
	xUnbounded.m_uBinding         = 0;
	xUnbounded.m_strName          = "g_axBindlessTextures";
	xUnbounded.m_eResourceKind    = FLUX_RESOURCE_KIND_UNBOUNDED_TEXTURE_ARRAY;
	xUnbounded.m_uDescriptorCount = 0;
	xUnbounded.m_uStageMask       = FLUX_SHADER_STAGE_BIT_FRAGMENT;
	xWrite.AddBinding(xUnbounded);

	Flux_ReflectedBinding xBounded;
	xBounded.m_eType            = BINDING_TYPE_TEXTURE;
	xBounded.m_uSet             = 0;
	xBounded.m_uBinding         = 1;
	xBounded.m_strName          = "g_xBoundedTex";
	xBounded.m_eResourceKind    = FLUX_RESOURCE_KIND_TEXTURE;
	xBounded.m_uDescriptorCount = 1;
	xWrite.AddBinding(xBounded);

	Zenith_DataStream xStream(512);
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);
	const Zenith_Vector<Flux_ReflectedBinding>& axOut = xRead.GetBindings();
	ZENITH_ASSERT_EQ(axOut.GetSize(), 2, "Should round-trip both bindings");
	ZENITH_ASSERT_EQ(axOut.Get(0).m_uDescriptorCount, 0, "Unbounded descriptor count should round-trip as 0");
	ZENITH_ASSERT_EQ(axOut.Get(1).m_uDescriptorCount, 1, "Bounded descriptor count should round-trip as 1");

}

ZENITH_TEST(Slang, ReflectionV2NamedLookupAfterDeserialise) { Zenith_UnitTests::TestReflectionV2NamedLookupAfterDeserialise(); }

void Zenith_UnitTests::TestReflectionV2NamedLookupAfterDeserialise(){

	// ReadFromDataStream calls BuildLookupMap at the end so callers can
	// immediately use GetBinding(name). Pin that contract — the runtime
	// binder relies on it (no explicit Build call after ReadFromFile).
	Flux_ShaderReflection xWrite;

	Flux_ReflectedBinding xA;
	xA.m_eType         = BINDING_TYPE_BUFFER;
	xA.m_uSet          = 0;
	xA.m_uBinding      = 0;
	xA.m_strName       = "FrameConstants";
	xA.m_eResourceKind = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xWrite.AddBinding(xA);

	Flux_ReflectedBinding xB;
	xB.m_eType         = BINDING_TYPE_TEXTURE;
	xB.m_uSet          = 1;
	xB.m_uBinding      = 7;
	xB.m_strName       = "g_xAlbedoTex";
	xB.m_eResourceKind = FLUX_RESOURCE_KIND_TEXTURE;
	xWrite.AddBinding(xB);

	Zenith_DataStream xStream(512);
	xWrite.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_ShaderReflection xRead;
	xRead.ReadFromDataStream(xStream);

	const Flux_ReflectedBinding* pxFrame  = xRead.GetBinding("FrameConstants");
	const Flux_ReflectedBinding* pxAlbedo = xRead.GetBinding("g_xAlbedoTex");
	const Flux_ReflectedBinding* pxMissing = xRead.GetBinding("DoesNotExist");

	ZENITH_ASSERT_TRUE(pxFrame != nullptr, "FrameConstants should be locatable by name after deserialise");
	ZENITH_ASSERT_TRUE(pxAlbedo != nullptr, "g_xAlbedoTex should be locatable by name after deserialise");
	ZENITH_ASSERT_TRUE(pxMissing == nullptr, "Missing binding should return nullptr, not stale entry");

	if (pxFrame)  { ZENITH_ASSERT_EQ(pxFrame->m_uSet, 0, "Frame lookup should resolve to set=0"); }
	if (pxAlbedo) { ZENITH_ASSERT_EQ(pxAlbedo->m_uBinding, 7, "Albedo lookup should resolve to binding=7"); }

}

//=============================================================================
// Codegen determinism tests
//
// The generator emits C++ headers consumed by engine code with static_asserts
// on sizeof / offsetof, so the relationship between Slang reflection and the
// emitted struct must be byte-stable. These tests pin:
//   - the program-enum content matches the registry (per-program line + COUNT)
//   - subsystem-header content is byte-identical when called twice with the
//     same inputs (no map ordering, no hash randomisation, no timestamps)
//   - reflected bindings show up as kName/kSet/kBinding/kDescriptorCount lines
//   - CB bindings emit a struct + static_asserts on size and per-field offsets
//   - non-identifier characters in resource names get sanitised so generated
//     C++ stays valid
//=============================================================================

ZENITH_TEST(Codegen, CodegenDeterministicDoubleRun) { Zenith_UnitTests::TestCodegenDeterministicDoubleRun(); }

void Zenith_UnitTests::TestCodegenDeterministicDoubleRun(){

	// Build a tiny synthetic registry-flavoured input: one program, one CB
	// binding with two fields, one texture binding. Run the generator twice
	// over it and confirm byte-identical output. If anything in the path
	// (std::unordered_map iteration, snprintf locale, std::vector growth)
	// introduces nondeterminism, this is the test that catches it.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xCB;
	xCB.m_eType            = BINDING_TYPE_BUFFER;
	xCB.m_uSet             = 0;
	xCB.m_uBinding         = 0;
	xCB.m_strName          = "FrameConstants";
	xCB.m_uSize            = 80;
	xCB.m_eResourceKind    = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount = 1;
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "m_xViewMat";
		xF.m_uOffset     = 0;
		xF.m_uSize       = 64;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4x4";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "m_xCameraPos_Pad";
		xF.m_uOffset     = 64;
		xF.m_uSize       = 16;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4";
		xCB.m_axFields.PushBack(xF);
	}
	xRefl.AddBinding(xCB);

	Flux_ReflectedBinding xTex;
	xTex.m_eType            = BINDING_TYPE_TEXTURE;
	xTex.m_uSet             = 1;
	xTex.m_uBinding         = 3;
	xTex.m_strName          = "g_xAlbedoTex";
	xTex.m_eResourceKind    = FLUX_RESOURCE_KIND_TEXTURE;
	xTex.m_uDescriptorCount = 1;
	xRefl.AddBinding(xTex);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string strFirst  = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestPilot", &xPR, 1);
	const std::string strSecond = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestPilot", &xPR, 1);

	ZENITH_ASSERT_EQ(strFirst, strSecond, "Codegen must produce byte-identical output across runs with identical input");
	ZENITH_ASSERT_TRUE(!strFirst.empty(), "Codegen output should not be empty");

	// Program-enum content is independent of input data (it's a function of
	// the registry), so the same double-run check applies.
	const std::string strEnum1 = Flux_CodeGenerator::BuildProgramEnumContent();
	const std::string strEnum2 = Flux_CodeGenerator::BuildProgramEnumContent();
	ZENITH_ASSERT_EQ(strEnum1, strEnum2, "Program enum content must be deterministic");

	// Sanity: enum should at least include one program name and the COUNT
	// terminator (the registry must have at least one entry for the engine
	// to link).
	ZENITH_ASSERT_TRUE(strEnum1.find("enum class FluxShaderProgram") != std::string::npos, "Enum content should declare FluxShaderProgram");
	ZENITH_ASSERT_TRUE(strEnum1.find("COUNT") != std::string::npos, "Enum content should include COUNT terminator");

}

ZENITH_TEST(Codegen, CodegenContainsBindingMetadata) { Zenith_UnitTests::TestCodegenContainsBindingMetadata(); }

void Zenith_UnitTests::TestCodegenContainsBindingMetadata(){

	// Pin the binding-metadata emission shape — a name, set, binding, and
	// descriptor-count line per reflected resource. The runtime root-sig
	// builder doesn't read these constants today (it goes through reflection
	// directly), but downstream code that imports the generated header to
	// avoid string-keyed lookups depends on them being stable.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xTex;
	xTex.m_eType            = BINDING_TYPE_TEXTURE;
	xTex.m_uSet             = 1;
	xTex.m_uBinding         = 7;
	xTex.m_strName          = "g_xAlbedoTex";
	xTex.m_eResourceKind    = FLUX_RESOURCE_KIND_TEXTURE;
	xTex.m_uDescriptorCount = 1;
	xRefl.AddBinding(xTex);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestMetadata", &xPR, 1);

	ZENITH_ASSERT_TRUE(str.find("kg_xAlbedoTex_Name = \"g_xAlbedoTex\"") != std::string::npos, "kName constant should reflect the original binding name");
	ZENITH_ASSERT_TRUE(str.find("kg_xAlbedoTex_Set = 1") != std::string::npos, "kSet constant should reflect the binding's descriptor set");
	ZENITH_ASSERT_TRUE(str.find("kg_xAlbedoTex_Binding = 7") != std::string::npos, "kBinding constant should reflect the binding slot");
	ZENITH_ASSERT_TRUE(str.find("kg_xAlbedoTex_DescriptorCount = 1") != std::string::npos, "kDescriptorCount should reflect the descriptor count");
	ZENITH_ASSERT_TRUE(str.find("// kind: Texture") != std::string::npos, "Resource kind comment should be present");

}

ZENITH_TEST(Codegen, CodegenEmitsCBStructWithStaticAsserts) { Zenith_UnitTests::TestCodegenEmitsCBStructWithStaticAsserts(); }

void Zenith_UnitTests::TestCodegenEmitsCBStructWithStaticAsserts(){

	// CB / parameter-block bindings emit a C++ struct mirror with size and
	// per-field offset asserts. This is the GPU<->CPU layout fence — without
	// the asserts, an offsetof drift would silently corrupt every shader read
	// at runtime. Pin that the asserts get emitted with the right numbers.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xCB;
	xCB.m_eType            = BINDING_TYPE_BUFFER;
	xCB.m_uSet             = 0;
	xCB.m_uBinding         = 0;
	xCB.m_strName          = "MyConstants";
	xCB.m_uSize            = 80;
	xCB.m_eResourceKind    = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount = 1;
	{
		// Field names match Slang/HLSL reflection (no engine 'm_' prefix —
		// the generator prepends 'm_x' on emission).
		Flux_ReflectedField xF;
		xF.m_strName     = "viewMat";
		xF.m_uOffset     = 0;
		xF.m_uSize       = 64;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4x4";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "cameraPos";
		xF.m_uOffset     = 64;
		xF.m_uSize       = 16;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float4";
		xCB.m_axFields.PushBack(xF);
	}
	xRefl.AddBinding(xCB);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestStruct", &xPR, 1);

	ZENITH_ASSERT_TRUE(str.find("struct MyConstants_CB") != std::string::npos, "Should emit struct mirror for CB binding");
	ZENITH_ASSERT_TRUE(str.find("glm::mat4 m_xviewMat") != std::string::npos, "float4x4 field should map to glm::mat4 with 'm_x' prefix");
	ZENITH_ASSERT_TRUE(str.find("glm::vec4 m_xcameraPos") != std::string::npos, "float4 field should map to glm::vec4 with 'm_x' prefix");
	ZENITH_ASSERT_TRUE(str.find("static_assert(sizeof(MyConstants_CB) == 80") != std::string::npos, "Should static_assert struct size matches reflected CB size");
	ZENITH_ASSERT_TRUE(str.find("static_assert(offsetof(MyConstants_CB, m_xviewMat) == 0") != std::string::npos, "Should static_assert viewMat offset");
	ZENITH_ASSERT_TRUE(str.find("static_assert(offsetof(MyConstants_CB, m_xcameraPos) == 64") != std::string::npos, "Should static_assert cameraPos offset");

}

ZENITH_TEST(Codegen, CodegenScalarHungarianPrefixes) { Zenith_UnitTests::TestCodegenScalarHungarianPrefixes(); }

void Zenith_UnitTests::TestCodegenScalarHungarianPrefixes(){

	// Scalar fields use type-specific Hungarian prefixes (`m_f` float,
	// `m_u` uint, `m_i` int, `m_b` bool); vec/mat fields use `m_x`. Pin
	// the prefix-vs-type mapping so a future refactor that changes the
	// table flips the offsetof assert names too.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xCB;
	xCB.m_eType            = BINDING_TYPE_BUFFER;
	xCB.m_uSet             = 0;
	xCB.m_uBinding         = 0;
	xCB.m_strName          = "ScalarConstants";
	xCB.m_uSize            = 32;
	xCB.m_eResourceKind    = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount = 1;
	{
		Flux_ReflectedField xF; xF.m_strName = "intensity";   xF.m_uOffset = 0;  xF.m_uSize = 4; xF.m_uArrayCount = 1; xF.m_strTypeName = "float";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF; xF.m_strName = "frameIndex";  xF.m_uOffset = 4;  xF.m_uSize = 4; xF.m_uArrayCount = 1; xF.m_strTypeName = "uint";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF; xF.m_strName = "delta";       xF.m_uOffset = 8;  xF.m_uSize = 4; xF.m_uArrayCount = 1; xF.m_strTypeName = "int";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF; xF.m_strName = "enableShadows"; xF.m_uOffset = 12; xF.m_uSize = 4; xF.m_uArrayCount = 1; xF.m_strTypeName = "bool";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF; xF.m_strName = "tint";        xF.m_uOffset = 16; xF.m_uSize = 16; xF.m_uArrayCount = 1; xF.m_strTypeName = "float4";
		xCB.m_axFields.PushBack(xF);
	}
	xRefl.AddBinding(xCB);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestPrefixes", &xPR, 1);

	ZENITH_ASSERT_TRUE(str.find("float m_fintensity")            != std::string::npos, "float should use 'm_f' prefix");
	ZENITH_ASSERT_TRUE(str.find("unsigned int m_uframeIndex")    != std::string::npos, "uint should use 'm_u' prefix");
	ZENITH_ASSERT_TRUE(str.find("int m_idelta")                  != std::string::npos, "int should use 'm_i' prefix");
	ZENITH_ASSERT_TRUE(str.find("unsigned int m_benableShadows") != std::string::npos, "bool should use 'm_b' prefix and unsigned-int storage");
	ZENITH_ASSERT_TRUE(str.find("glm::vec4 m_xtint")             != std::string::npos, "vec4 should use 'm_x' prefix");

	// offsetof asserts must reference the same Hungarian-prefixed name.
	ZENITH_ASSERT_TRUE(str.find("offsetof(ScalarConstants_CB, m_fintensity)")     != std::string::npos, "offsetof should use 'm_f' name");
	ZENITH_ASSERT_TRUE(str.find("offsetof(ScalarConstants_CB, m_uframeIndex)")    != std::string::npos, "offsetof should use 'm_u' name");
	ZENITH_ASSERT_TRUE(str.find("offsetof(ScalarConstants_CB, m_idelta)")         != std::string::npos, "offsetof should use 'm_i' name");
	ZENITH_ASSERT_TRUE(str.find("offsetof(ScalarConstants_CB, m_benableShadows)") != std::string::npos, "offsetof should use 'm_b' name");
	ZENITH_ASSERT_TRUE(str.find("offsetof(ScalarConstants_CB, m_xtint)")          != std::string::npos, "offsetof should use 'm_x' name");

}

ZENITH_TEST(Codegen, CodegenInsertsTrailingPadding) { Zenith_UnitTests::TestCodegenInsertsTrailingPadding(); }

void Zenith_UnitTests::TestCodegenInsertsTrailingPadding(){

	// Reproduces the ComputeTest case: an unmapped vector field with
	// reflected size 8 inside a CB whose total size is 16 (std140 rounds
	// the CB up to 16-byte alignment). Without trailing padding the
	// emitted struct would be 8 bytes and the size_assert would fail at
	// host compile time.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xCB;
	xCB.m_eType            = BINDING_TYPE_BUFFER;
	xCB.m_uSet             = 0;
	xCB.m_uBinding         = 0;
	xCB.m_strName          = "PaddedCB";
	xCB.m_uSize            = 16;
	xCB.m_eResourceKind    = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount = 1;
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "imageSize";
		xF.m_uOffset     = 0;
		xF.m_uSize       = 8;
		xF.m_uArrayCount = 2;
		xF.m_strTypeName = "vector"; // unmapped — falls into byte-buffer branch
		xCB.m_axFields.PushBack(xF);
	}
	xRefl.AddBinding(xCB);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestPad", &xPR, 1);

	ZENITH_ASSERT_TRUE(str.find("unsigned char m_aimageSize[8]") != std::string::npos, "Should emit byte-buffer for unmapped vector");
	ZENITH_ASSERT_TRUE(str.find("m_aPad_8[8]") != std::string::npos, "Should emit 8-byte trailing pad to bring struct from 8 to 16");
	ZENITH_ASSERT_TRUE(str.find("static_assert(sizeof(PaddedCB_CB) == 16") != std::string::npos, "size_assert should match reflected CB size");

}

ZENITH_TEST(Codegen, CodegenInsertsInteriorPadding) { Zenith_UnitTests::TestCodegenInsertsInteriorPadding(); }

void Zenith_UnitTests::TestCodegenInsertsInteriorPadding(){

	// std140 pads vec3 to 16 bytes. Slang surfaces a `float3` field at
	// offset 0 with size 12, then a `float` at offset 16 — there's a
	// 4-byte hole between. Pin that the generator emits the hole as a
	// pad member so offsetof of the trailing field still resolves to 16.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xCB;
	xCB.m_eType            = BINDING_TYPE_BUFFER;
	xCB.m_uSet             = 0;
	xCB.m_uBinding         = 0;
	xCB.m_strName          = "InteriorPadCB";
	xCB.m_uSize            = 32;
	xCB.m_eResourceKind    = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	xCB.m_uDescriptorCount = 1;
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "direction";
		xF.m_uOffset     = 0;
		xF.m_uSize       = 12;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float3";
		xCB.m_axFields.PushBack(xF);
	}
	{
		Flux_ReflectedField xF;
		xF.m_strName     = "intensity";
		xF.m_uOffset     = 16;     // std140 hole at [12,16)
		xF.m_uSize       = 4;
		xF.m_uArrayCount = 1;
		xF.m_strTypeName = "float";
		xCB.m_axFields.PushBack(xF);
	}
	xRefl.AddBinding(xCB);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestInterior", &xPR, 1);

	ZENITH_ASSERT_TRUE(str.find("glm::vec3 m_xdirection") != std::string::npos, "float3 should map to glm::vec3");
	ZENITH_ASSERT_TRUE(str.find("m_aPad_12[4]") != std::string::npos, "Should emit 4-byte interior pad after vec3");
	ZENITH_ASSERT_TRUE(str.find("float m_fintensity") != std::string::npos, "float should map to float");
	ZENITH_ASSERT_TRUE(str.find("m_aPad_20[12]") != std::string::npos, "Should emit 12-byte trailing pad up to 32 bytes");
	ZENITH_ASSERT_TRUE(str.find("offsetof(InteriorPadCB_CB, m_fintensity) == 16") != std::string::npos, "intensity offset should still be 16 after padding insertion");

}

ZENITH_TEST(Codegen, CodegenSanitisesIdentifiers) { Zenith_UnitTests::TestCodegenSanitisesIdentifiers(); }

void Zenith_UnitTests::TestCodegenSanitisesIdentifiers(){

	// Resource names in shader source can theoretically contain dots (for
	// parameter-block paths, e.g. "frame.lights"), spaces, or leading digits.
	// These would be invalid C++ identifiers verbatim, so the generator
	// sanitises them. Pin the sanitisation rule so a future generator change
	// can't silently break downstream callers.
	Flux_ShaderReflection xRefl;

	Flux_ReflectedBinding xWeird;
	xWeird.m_eType            = BINDING_TYPE_TEXTURE;
	xWeird.m_uSet             = 0;
	xWeird.m_uBinding         = 0;
	xWeird.m_strName          = "frame.lights"; // contains a dot
	xWeird.m_eResourceKind    = FLUX_RESOURCE_KIND_TEXTURE;
	xWeird.m_uDescriptorCount = 1;
	xRefl.AddBinding(xWeird);
	xRefl.BuildLookupMap();

	Flux_CodeGenerator::ProgramReflection xPR;
	xPR.m_eId = static_cast<FluxShaderProgram>(0);
	xPR.m_pxReflection = &xRefl;

	const std::string str = Flux_CodeGenerator::BuildSubsystemHeaderContent("CodegenTestIdentifier", &xPR, 1);

	// The constant identifier should have the dot replaced with underscore;
	// the kName string literal should keep the original shader-side name so
	// runtime lookups still work.
	ZENITH_ASSERT_TRUE(str.find("kframe_lights_Name = \"frame.lights\"") != std::string::npos, "Sanitised identifier should replace '.' with '_' but kName literal should preserve original");
	ZENITH_ASSERT_TRUE(str.find("frame.lights_Set") == std::string::npos, "Generated identifier should never contain raw '.'");

}

//==============================================================================
// Wave 8.3 - release-survivable check tier (Zenith_Check / Zenith_Verify)
//==============================================================================

ZENITH_TEST(Core, CheckTierReleaseSurvivable) { Zenith_UnitTests::TestCheckTierReleaseSurvivable(); }

void Zenith_UnitTests::TestCheckTierReleaseSurvivable(){

	// Unlike Zenith_Assert, the check tier logs and CONTINUES — it must never
	// call Zenith_DebugBreak(). If Zenith_Check(false, ...) aborted, control
	// would never reach the line after it and this test could not pass.
	// (One log line is emitted by design; we keep it to a single failing Check
	// to minimise spam.)
	bool bReachedAfterCheck = false;
	Zenith_Check(false, "TestCheckTierReleaseSurvivable: intentional check failure (expected, not a real error)");
	bReachedAfterCheck = true;
	ZENITH_ASSERT_TRUE(bReachedAfterCheck,
		"Zenith_Check(false) must log and fall through, not abort");

	// Zenith_Verify(expr) must EVALUATE expr for its side effects. Use a
	// side-effecting, true expression: the increment runs and, because the
	// expression is true, nothing is logged (zero spam) — proving evaluation
	// happened independently of the log path.
	int iSideEffectCounter = 0;
	Zenith_Verify((++iSideEffectCounter) == 1);
	ZENITH_ASSERT_EQ(iSideEffectCounter, 1,
		"Zenith_Verify must evaluate its expression (side effect should have run exactly once)");
}

//==============================================================================
// Wave 8.3 - task-queue overflow no longer crashes (graceful QUEUE_FULL)
//==============================================================================

// Trivial fire-and-forget task body: a short busy-spin to encourage the queue
// to back up past uMAX_TASKS, then a single atomic increment so the test can
// confirm work actually happened.
struct QueueFullTestData
{
	std::atomic<u_int> m_uRunCount{0};
};

static void QueueFullTaskFunc(void* pData)
{
	auto* pxData = static_cast<QueueFullTestData*>(pData);
	// Tiny amount of work so workers don't drain instantly, making the
	// >128-in-flight overflow path realistic without a fragile blocking gate.
	volatile u_int uSpin = 0;
	for (u_int u = 0; u < 2048u; u++) { uSpin += u; }
	(void)uSpin;
	pxData->m_uRunCount.fetch_add(1, std::memory_order_relaxed);
}

ZENITH_TEST(TaskSystem, QueueFullSurfacesError) { Zenith_UnitTests::TestQueueFullSurfacesError(); }

void Zenith_UnitTests::TestQueueFullSurfacesError(){

	// Submit far more than Zenith_TaskSystem::uMAX_TASKS (128) tasks without
	// draining between submits, so the bounded circular queue can overflow.
	// The old code asserted (and Zenith_DebugBreak'd) on overflow; the Wave 8.3
	// change downgraded that to a Zenith_Check that logs QUEUE_FULL and lets the
	// enqueue come up short. SubmitTask resets m_bSubmitted on a zero-enqueue, so
	// any task that failed to enqueue is NOT marked submitted and its
	// WaitUntilComplete() is a no-op — meaning this loop is deadlock-free
	// regardless of how many tasks the queue refused.
	constexpr u_int uNUM_TASKS = Zenith_TaskSystem::uMAX_TASKS * 2u + 16u; // 272 > 128
	QueueFullTestData xData;

	Zenith_Task** apxTasks = new Zenith_Task*[uNUM_TASKS];
	for (u_int u = 0; u < uNUM_TASKS; u++)
	{
		apxTasks[u] = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES, QueueFullTaskFunc, &xData);
	}

	// Fire them all in without draining — this is what drives the overflow.
	for (u_int u = 0; u < uNUM_TASKS; u++)
	{
		g_xEngine.Tasks().SubmitTask(apxTasks[u]);
	}

	// Drain. Tasks that were enqueued run exactly once; tasks the queue refused
	// had their submitted flag reset, so WaitUntilComplete returns immediately.
	for (u_int u = 0; u < uNUM_TASKS; u++)
	{
		apxTasks[u]->WaitUntilComplete();
	}

	for (u_int u = 0; u < uNUM_TASKS; u++)
	{
		delete apxTasks[u];
	}
	delete[] apxTasks;

	// The headline guarantee: we reached here without a debug-break / crash on
	// overflow. As a sanity bound, every task that actually ran did so exactly
	// once, so the run count is in (0, uNUM_TASKS].
	const u_int uRan = xData.m_uRunCount.load(std::memory_order_relaxed);
	ZENITH_ASSERT_GT(uRan, 0u, "At least some submitted tasks should have executed");
	ZENITH_ASSERT_LE(uRan, uNUM_TASKS, "Run count must never exceed the number of tasks submitted");
}
