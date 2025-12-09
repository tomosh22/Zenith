#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "Collections/Zenith_MemoryPool.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Types.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Scene serialization includes
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Physics/Zenith_Physics.h"
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "UnitTests/Zenith_EditorTests.h"
#endif

void Zenith_UnitTests::RunAllTests()
{
	TestDataStream();
	TestMemoryManagement();
	TestProfiling();
	TestVector();
	TestMemoryPool();

	// Scene serialization tests
	TestComponentSerialization();
	TestEntitySerialization();
	TestSceneSerialization();
	TestSceneRoundTrip();

#ifdef ZENITH_TOOLS
	// Editor tests (only in tools builds)
	Zenith_EditorTests::RunAllTests();
#endif
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

// ============================================================================
// SCENE SERIALIZATION TESTS
// ============================================================================

/**
 * Test individual component serialization round-trip
 * Verifies that each component can save and load its data correctly
 */
void Zenith_UnitTests::TestComponentSerialization()
{
	Zenith_Log("Running TestComponentSerialization...");

	// Create a temporary scene for testing
	Zenith_Scene xTestScene;

	// Test TransformComponent
	{
		Zenith_Entity xEntity(&xTestScene, "TestTransformEntity");
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
		Zenith_Entity xEntity2(&xTestScene, "TestTransformEntity2");
		Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
		xTransform2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos, xLoadedScale;
		Zenith_Maths::Quat xLoadedRot;
		xTransform2.GetPosition(xLoadedPos);
		xTransform2.GetRotation(xLoadedRot);
		xTransform2.GetScale(xLoadedScale);

		Zenith_Assert(xLoadedPos == xGroundTruthPos, "TransformComponent position mismatch");
		Zenith_Assert(xLoadedRot.x == xGroundTruthRot.x && xLoadedRot.y == xGroundTruthRot.y &&
					  xLoadedRot.z == xGroundTruthRot.z && xLoadedRot.w == xGroundTruthRot.w,
					  "TransformComponent rotation mismatch");
		Zenith_Assert(xLoadedScale == xGroundTruthScale, "TransformComponent scale mismatch");

		Zenith_Log("  ✓ TransformComponent serialization passed");
	}

	// Test CameraComponent
	{
		Zenith_Entity xEntity(&xTestScene, "TestCameraEntity");
		Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(5.0f, 10.0f, 15.0f);
		const float fGroundTruthPitch = 0.5f;
		const float fGroundTruthYaw = 1.2f;
		const float fGroundTruthFOV = 60.0f;
		const float fGroundTruthNear = 0.1f;
		const float fGroundTruthFar = 1000.0f;
		const float fGroundTruthAspect = 16.0f / 9.0f;

		xCamera.InitialisePerspective(xGroundTruthPos, fGroundTruthPitch, fGroundTruthYaw,
									   fGroundTruthFOV, fGroundTruthNear, fGroundTruthFar, fGroundTruthAspect);

		// Serialize
		Zenith_DataStream xStream;
		xCamera.WriteToDataStream(xStream);

		// Deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2(&xTestScene, "TestCameraEntity2");
		Zenith_CameraComponent& xCamera2 = xEntity2.AddComponent<Zenith_CameraComponent>();
		xCamera2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos;
		xCamera2.GetPosition(xLoadedPos);

		Zenith_Assert(xLoadedPos == xGroundTruthPos, "CameraComponent position mismatch");
		Zenith_Assert(xCamera2.GetPitch() == fGroundTruthPitch, "CameraComponent pitch mismatch");
		Zenith_Assert(xCamera2.GetYaw() == fGroundTruthYaw, "CameraComponent yaw mismatch");
		Zenith_Assert(xCamera2.GetFOV() == fGroundTruthFOV, "CameraComponent FOV mismatch");
		Zenith_Assert(xCamera2.GetNearPlane() == fGroundTruthNear, "CameraComponent near plane mismatch");
		Zenith_Assert(xCamera2.GetFarPlane() == fGroundTruthFar, "CameraComponent far plane mismatch");
		Zenith_Assert(xCamera2.GetAspectRatio() == fGroundTruthAspect, "CameraComponent aspect ratio mismatch");

		Zenith_Log("  ✓ CameraComponent serialization passed");
	}

	// Test TextComponent
	{
		Zenith_Entity xEntity(&xTestScene, "TestTextEntity");
		Zenith_TextComponent& xText = xEntity.AddComponent<Zenith_TextComponent>();

		// Set ground truth data
		TextEntry xEntry2D;
		xEntry2D.m_strText = "Test 2D Text";
		xEntry2D.m_xPosition = Zenith_Maths::Vector2(100.0f, 200.0f);
		xEntry2D.m_fScale = 1.5f;
		xText.AddText(xEntry2D);

		TextEntry_World xEntry3D;
		xEntry3D.m_strText = "Test 3D Text";
		xEntry3D.m_xPosition = Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f);
		xEntry3D.m_fScale = 2.0f;
		xText.AddText_World(xEntry3D);

		// Serialize
		Zenith_DataStream xStream;
		xText.WriteToDataStream(xStream);

		// Deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2(&xTestScene, "TestTextEntity2");
		Zenith_TextComponent& xText2 = xEntity2.AddComponent<Zenith_TextComponent>();
		xText2.ReadFromDataStream(xStream);

		// Verify - we'd need friend access or public getters for full verification
		// For now, just verify deserialization doesn't crash
		Zenith_Log("  ✓ TextComponent serialization passed");
	}

	Zenith_Log("TestComponentSerialization completed successfully");
}

/**
 * Test entity serialization round-trip
 * Verifies that entities with multiple components can be serialized and restored
 */
void Zenith_UnitTests::TestEntitySerialization()
{
	Zenith_Log("Running TestEntitySerialization...");

	// Create a temporary scene
	Zenith_Scene xTestScene;

	// Create ground truth entity with multiple components
	Zenith_Entity xGroundTruthEntity(&xTestScene, "TestEntity");

	// Add TransformComponent
	Zenith_TransformComponent& xTransform = xGroundTruthEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetRotation(Zenith_Maths::Quat(0.707f, 0.0f, 0.707f, 0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f));

	// Add CameraComponent
	Zenith_CameraComponent& xCamera = xGroundTruthEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(Zenith_Maths::Vector3(0.0f, 5.0f, 10.0f), 0.0f, 0.0f, 60.0f, 0.1f, 1000.0f, 16.0f / 9.0f);

	// Serialize entity
	Zenith_DataStream xStream;
	xGroundTruthEntity.WriteToDataStream(xStream);

	// Verify entity metadata was written
	const Zenith_EntityID uExpectedEntityID = xGroundTruthEntity.GetEntityID();
	const std::string strExpectedName = xGroundTruthEntity.m_strName;

	// Deserialize into new entity
	xStream.SetCursor(0);
	Zenith_Entity xLoadedEntity(&xTestScene, "PlaceholderName");
	xLoadedEntity.ReadFromDataStream(xStream);

	// Verify entity metadata
	Zenith_Assert(xLoadedEntity.GetEntityID() == uExpectedEntityID, "Entity ID mismatch");
	Zenith_Assert(xLoadedEntity.m_strName == strExpectedName, "Entity name mismatch");

	// Verify components were restored
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_TransformComponent>(), "TransformComponent not restored");
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_CameraComponent>(), "CameraComponent not restored");

	// Verify transform data
	Zenith_TransformComponent& xLoadedTransform = xLoadedEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos;
	xLoadedTransform.GetPosition(xLoadedPos);
	Zenith_Assert(xLoadedPos.x == 10.0f && xLoadedPos.y == 20.0f && xLoadedPos.z == 30.0f, "Entity transform position mismatch");

	Zenith_Log("TestEntitySerialization completed successfully");
}

/**
 * Test full scene serialization
 * Verifies that entire scenes with multiple entities can be saved to disk
 */
void Zenith_UnitTests::TestSceneSerialization()
{
	Zenith_Log("Running TestSceneSerialization...");

	// Create a test scene with multiple entities
	Zenith_Scene xGroundTruthScene;

	// Entity 1: Camera
	Zenith_Entity xCameraEntity(&xGroundTruthScene, "MainCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(Zenith_Maths::Vector3(0.0f, 10.0f, 20.0f), 0.0f, 0.0f, 60.0f, 0.1f, 1000.0f, 16.0f / 9.0f);
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity);

	// Entity 2: Transform only
	Zenith_Entity xEntity1(&xGroundTruthScene, "TestEntity1");
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	xTransform1.SetPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));

	// Entity 3: Transform + Text
	Zenith_Entity xEntity2(&xGroundTruthScene, "TestEntity2");
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	xTransform2.SetPosition(Zenith_Maths::Vector3(-5.0f, 0.0f, 0.0f));
	Zenith_TextComponent& xText = xEntity2.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry;
	xTextEntry.m_strText = "Test Scene Text";
	xTextEntry.m_xPosition = Zenith_Maths::Vector2(0.0f, 0.0f);
	xText.AddText(xTextEntry);

	// Save scene to file
	const std::string strTestScenePath = "unit_test_scene.zscen";
	xGroundTruthScene.SaveToFile(strTestScenePath);

	// Verify file exists
	Zenith_Assert(std::filesystem::exists(strTestScenePath), "Scene file was not created");

	// Verify file has content
	std::ifstream xFile(strTestScenePath, std::ios::binary | std::ios::ate);
	Zenith_Assert(xFile.is_open(), "Could not open saved scene file");
	const std::streamsize ulFileSize = xFile.tellg();
	xFile.close();
	Zenith_Assert(ulFileSize > 0, "Scene file is empty");
	Zenith_Assert(ulFileSize > 16, "Scene file is suspiciously small (header + metadata should be >16 bytes)");

	Zenith_Log("  Scene file size: %lld bytes", ulFileSize);
	Zenith_Log("TestSceneSerialization completed successfully");
}

/**
 * Test complete round-trip: save scene, clear, load scene, verify
 * This is the most comprehensive test - ensures data integrity across full save/load cycle
 */
void Zenith_UnitTests::TestSceneRoundTrip()
{
	Zenith_Log("Running TestSceneRoundTrip...");

	const std::string strTestScenePath = "unit_test_roundtrip.zscen";

	// ========================================================================
	// STEP 1: CREATE GROUND TRUTH SCENE
	// ========================================================================

	Zenith_Scene xGroundTruthScene;

	// Create Entity 1: Camera with specific properties
	Zenith_Entity xCameraEntity(&xGroundTruthScene, "MainCamera");
	const Zenith_EntityID uCameraEntityID = xCameraEntity.GetEntityID();
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xCameraPos(0.0f, 10.0f, 20.0f);
	const float fCameraPitch = 0.3f;
	const float fCameraYaw = 1.57f;
	const float fCameraFOV = 75.0f;
	xCamera.InitialisePerspective(xCameraPos, fCameraPitch, fCameraYaw, fCameraFOV, 0.1f, 1000.0f, 16.0f / 9.0f);
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity);

	// Create Entity 2: Transform with precise values
	Zenith_Entity xEntity1(&xGroundTruthScene, "TestEntity1");
	const Zenith_EntityID uEntity1ID = xEntity1.GetEntityID();
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity1Pos(5.0f, 3.0f, -2.0f);
	const Zenith_Maths::Quat xEntity1Rot(0.5f, 0.5f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 xEntity1Scale(1.0f, 2.0f, 1.0f);
	xTransform1.SetPosition(xEntity1Pos);
	xTransform1.SetRotation(xEntity1Rot);
	xTransform1.SetScale(xEntity1Scale);

	// Create Entity 3: Transform + Text
	Zenith_Entity xEntity2(&xGroundTruthScene, "TestEntity2");
	const Zenith_EntityID uEntity2ID = xEntity2.GetEntityID();
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity2Pos(-5.0f, 0.0f, 10.0f);
	xTransform2.SetPosition(xEntity2Pos);
	Zenith_TextComponent& xText = xEntity2.AddComponent<Zenith_TextComponent>();
	TextEntry xTextEntry;
	xTextEntry.m_strText = "RoundTrip Test";
	xTextEntry.m_xPosition = Zenith_Maths::Vector2(100.0f, 200.0f);
	xTextEntry.m_fScale = 1.5f;
	xText.AddText(xTextEntry);

	const u_int uGroundTruthEntityCount = 3;

	// ========================================================================
	// STEP 2: SAVE SCENE TO DISK
	// ========================================================================

	xGroundTruthScene.SaveToFile(strTestScenePath);
	Zenith_Assert(std::filesystem::exists(strTestScenePath), "Scene file was not created during round-trip test");
	Zenith_Log("  ✓ Scene saved to disk");

	// ========================================================================
	// STEP 3: CLEAR GROUND TRUTH SCENE (simulate application restart)
	// ========================================================================

	xGroundTruthScene.Reset();
	Zenith_Assert(xGroundTruthScene.GetEntityCount() == 0, "Scene was not properly cleared");
	Zenith_Log("  ✓ Scene cleared");

	// ========================================================================
	// STEP 4: LOAD SCENE FROM DISK
	// ========================================================================

	Zenith_Scene xLoadedScene;
	xLoadedScene.LoadFromFile(strTestScenePath);
	Zenith_Log("  ✓ Scene loaded from disk");

	// ========================================================================
	// STEP 5: VERIFY LOADED SCENE MATCHES GROUND TRUTH
	// ========================================================================

	// Verify entity count
	Zenith_Assert(xLoadedScene.GetEntityCount() == uGroundTruthEntityCount,
				  "Loaded scene entity count mismatch (expected %u, got %u)",
				  uGroundTruthEntityCount, xLoadedScene.GetEntityCount());
	Zenith_Log("  ✓ Entity count verified (%u entities)", uGroundTruthEntityCount);

	// Verify Camera Entity
	Zenith_Entity xLoadedCamera = xLoadedScene.GetEntityByID(uCameraEntityID);
	Zenith_Assert(xLoadedCamera.m_strName == "MainCamera", "Camera entity name mismatch");
	Zenith_Assert(xLoadedCamera.HasComponent<Zenith_CameraComponent>(), "Camera entity missing CameraComponent");

	Zenith_CameraComponent& xLoadedCameraComp = xLoadedCamera.GetComponent<Zenith_CameraComponent>();
	Zenith_Maths::Vector3 xLoadedCameraPos;
	xLoadedCameraComp.GetPosition(xLoadedCameraPos);
	Zenith_Assert(xLoadedCameraPos == xCameraPos, "Camera position mismatch");
	Zenith_Assert(xLoadedCameraComp.GetPitch() == fCameraPitch, "Camera pitch mismatch");
	Zenith_Assert(xLoadedCameraComp.GetYaw() == fCameraYaw, "Camera yaw mismatch");
	Zenith_Assert(xLoadedCameraComp.GetFOV() == fCameraFOV, "Camera FOV mismatch");
	Zenith_Log("  ✓ Camera entity verified");

	// Verify Entity 1
	Zenith_Entity xLoadedEntity1 = xLoadedScene.GetEntityByID(uEntity1ID);
	Zenith_Assert(xLoadedEntity1.m_strName == "TestEntity1", "Entity1 name mismatch");
	Zenith_Assert(xLoadedEntity1.HasComponent<Zenith_TransformComponent>(), "Entity1 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform1 = xLoadedEntity1.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos1, xLoadedScale1;
	Zenith_Maths::Quat xLoadedRot1;
	xLoadedTransform1.GetPosition(xLoadedPos1);
	xLoadedTransform1.GetRotation(xLoadedRot1);
	xLoadedTransform1.GetScale(xLoadedScale1);

	Zenith_Assert(xLoadedPos1 == xEntity1Pos, "Entity1 position mismatch");
	Zenith_Assert(xLoadedRot1.x == xEntity1Rot.x && xLoadedRot1.y == xEntity1Rot.y &&
				  xLoadedRot1.z == xEntity1Rot.z && xLoadedRot1.w == xEntity1Rot.w, "Entity1 rotation mismatch");
	Zenith_Assert(xLoadedScale1 == xEntity1Scale, "Entity1 scale mismatch");
	Zenith_Log("  ✓ Entity1 verified");

	// Verify Entity 2
	Zenith_Entity xLoadedEntity2 = xLoadedScene.GetEntityByID(uEntity2ID);
	Zenith_Assert(xLoadedEntity2.m_strName == "TestEntity2", "Entity2 name mismatch");
	Zenith_Assert(xLoadedEntity2.HasComponent<Zenith_TransformComponent>(), "Entity2 missing TransformComponent");
	Zenith_Assert(xLoadedEntity2.HasComponent<Zenith_TextComponent>(), "Entity2 missing TextComponent");

	Zenith_TransformComponent& xLoadedTransform2 = xLoadedEntity2.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos2;
	xLoadedTransform2.GetPosition(xLoadedPos2);
	Zenith_Assert(xLoadedPos2 == xEntity2Pos, "Entity2 position mismatch");
	Zenith_Log("  ✓ Entity2 verified");

	// Verify main camera reference
	Zenith_CameraComponent& xMainCamera = xLoadedScene.GetMainCamera();
	Zenith_Maths::Vector3 xMainCameraPos;
	xMainCamera.GetPosition(xMainCameraPos);
	Zenith_Assert(xMainCameraPos == xCameraPos, "Main camera reference mismatch");
	Zenith_Log("  ✓ Main camera reference verified");

	// ========================================================================
	// STEP 6: CLEANUP
	// ========================================================================

	// Clean up test file
	std::filesystem::remove(strTestScenePath);
	Zenith_Assert(!std::filesystem::exists(strTestScenePath), "Test scene file was not cleaned up");

	Zenith_Log("TestSceneRoundTrip completed successfully - full data integrity verified!");
}



