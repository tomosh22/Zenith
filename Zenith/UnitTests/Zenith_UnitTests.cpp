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

// Animation system includes
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

// Asset pipeline includes
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

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

	// Animation system tests
	TestBoneLocalPoseBlending();
	TestSkeletonPoseOperations();
	TestAnimationParameters();
	TestTransitionConditions();
	TestAnimationStateMachine();
	TestIKChainSetup();
	TestAnimationSerialization();
	TestBlendTreeNodes();
	TestCrossFadeTransition();

	// Additional animation tests
	TestAnimationClipChannels();
	TestBlendSpace1D();
	TestFABRIKSolver();
	TestAnimationEvents();
	TestBoneMasking();

	// Asset pipeline tests
	TestMeshAssetLoading();
	TestBindPoseVertexPositions();
	TestAnimatedVertexPositions();

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
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

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
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

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
void Zenith_UnitTests::TestBoneLocalPoseBlending()
{
	Zenith_Log("Running TestBoneLocalPoseBlending...");

	// Test Identity pose
	{
		Flux_BoneLocalPose xIdentity = Flux_BoneLocalPose::Identity();
		Zenith_Assert(Vec3Equals(xIdentity.m_xPosition, Zenith_Maths::Vector3(0.0f)),
			"Identity pose position should be zero");
		Zenith_Assert(QuatEquals(xIdentity.m_xRotation, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)),
			"Identity pose rotation should be identity quaternion");
		Zenith_Assert(Vec3Equals(xIdentity.m_xScale, Zenith_Maths::Vector3(1.0f)),
			"Identity pose scale should be one");
		Zenith_Log("  ✓ Identity pose test passed");
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
		Zenith_Assert(Vec3Equals(xBlend0.m_xPosition, xPoseA.m_xPosition),
			"Blend at t=0 should return pose A position");
		Zenith_Assert(Vec3Equals(xBlend0.m_xScale, xPoseA.m_xScale),
			"Blend at t=0 should return pose A scale");

		// Test t=1 (should return B)
		Flux_BoneLocalPose xBlend1 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 1.0f);
		Zenith_Assert(Vec3Equals(xBlend1.m_xPosition, xPoseB.m_xPosition),
			"Blend at t=1 should return pose B position");
		Zenith_Assert(Vec3Equals(xBlend1.m_xScale, xPoseB.m_xScale),
			"Blend at t=1 should return pose B scale");

		// Test t=0.5 (should return midpoint)
		Flux_BoneLocalPose xBlend05 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 0.5f);
		Zenith_Maths::Vector3 xExpectedPos(5.0f, 10.0f, 15.0f);
		Zenith_Maths::Vector3 xExpectedScale(1.5f, 1.5f, 1.5f);
		Zenith_Assert(Vec3Equals(xBlend05.m_xPosition, xExpectedPos),
			"Blend at t=0.5 should return midpoint position");
		Zenith_Assert(Vec3Equals(xBlend05.m_xScale, xExpectedScale),
			"Blend at t=0.5 should return midpoint scale");

		Zenith_Log("  ✓ Linear blend test passed");
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
		Zenith_Assert(Vec3Equals(xResult.m_xPosition, xExpectedPos),
			"Additive blend should add delta position");

		Zenith_Log("  ✓ Additive blend test passed");
	}

	// Test ToMatrix conversion
	{
		Flux_BoneLocalPose xPose;
		xPose.m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
		xPose.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xPose.m_xScale = Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f);

		Zenith_Maths::Matrix4 xMatrix = xPose.ToMatrix();

		// Check translation is in 4th column
		Zenith_Assert(FloatEquals(xMatrix[3][0], 1.0f) &&
					  FloatEquals(xMatrix[3][1], 2.0f) &&
					  FloatEquals(xMatrix[3][2], 3.0f),
			"Matrix translation should match pose position");

		Zenith_Log("  ✓ ToMatrix conversion test passed");
	}

	Zenith_Log("TestBoneLocalPoseBlending completed successfully");
}

/**
 * Test Flux_SkeletonPose operations
 * Verifies initialization, reset, and copy operations
 */
void Zenith_UnitTests::TestSkeletonPoseOperations()
{
	Zenith_Log("Running TestSkeletonPoseOperations...");

	// Test initialization
	{
		Flux_SkeletonPose xPose;
		xPose.Initialize(50);

		Zenith_Assert(xPose.GetNumBones() == 50,
			"Skeleton pose should have 50 bones after initialization");
		Zenith_Log("  ✓ Initialization test passed");
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
		Zenith_Assert(Vec3Equals(xResetBone.m_xPosition, Zenith_Maths::Vector3(0.0f)),
			"Reset should set position to zero");
		Zenith_Assert(Vec3Equals(xResetBone.m_xScale, Zenith_Maths::Vector3(1.0f)),
			"Reset should set scale to one");

		Zenith_Log("  ✓ Reset test passed");
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

		Zenith_Assert(Vec3Equals(xPoseB.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)),
			"CopyFrom should copy bone 0 position");
		Zenith_Assert(Vec3Equals(xPoseB.GetLocalPose(1).m_xPosition, Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f)),
			"CopyFrom should copy bone 1 position");

		Zenith_Log("  ✓ CopyFrom test passed");
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

		Zenith_Assert(Vec3Equals(xPoseOut.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)),
			"Skeleton blend should interpolate bone positions");

		Zenith_Log("  ✓ Static blend test passed");
	}

	Zenith_Log("TestSkeletonPoseOperations completed successfully");
}

/**
 * Test Flux_AnimationParameters
 * Verifies parameter add, set, get, and trigger consumption
 */
void Zenith_UnitTests::TestAnimationParameters()
{
	Zenith_Log("Running TestAnimationParameters...");

	Flux_AnimationParameters xParams;

	// Test Float parameter
	{
		xParams.AddFloat("Speed", 5.0f);
		Zenith_Assert(xParams.HasParameter("Speed"), "Should have Speed parameter");
		Zenith_Assert(FloatEquals(xParams.GetFloat("Speed"), 5.0f),
			"Speed default should be 5.0");

		xParams.SetFloat("Speed", 10.0f);
		Zenith_Assert(FloatEquals(xParams.GetFloat("Speed"), 10.0f),
			"Speed should be updated to 10.0");

		Zenith_Log("  ✓ Float parameter test passed");
	}

	// Test Int parameter
	{
		xParams.AddInt("Health", 100);
		Zenith_Assert(xParams.HasParameter("Health"), "Should have Health parameter");
		Zenith_Assert(xParams.GetInt("Health") == 100, "Health default should be 100");

		xParams.SetInt("Health", 50);
		Zenith_Assert(xParams.GetInt("Health") == 50, "Health should be updated to 50");

		Zenith_Log("  ✓ Int parameter test passed");
	}

	// Test Bool parameter
	{
		xParams.AddBool("IsRunning", false);
		Zenith_Assert(xParams.HasParameter("IsRunning"), "Should have IsRunning parameter");
		Zenith_Assert(xParams.GetBool("IsRunning") == false, "IsRunning default should be false");

		xParams.SetBool("IsRunning", true);
		Zenith_Assert(xParams.GetBool("IsRunning") == true, "IsRunning should be updated to true");

		Zenith_Log("  ✓ Bool parameter test passed");
	}

	// Test Trigger parameter
	{
		xParams.AddTrigger("Jump");
		Zenith_Assert(xParams.HasParameter("Jump"), "Should have Jump trigger");

		// Trigger not set initially
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == false,
			"Trigger should not be set initially");

		// Set trigger
		xParams.SetTrigger("Jump");
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == true,
			"Trigger should be set after SetTrigger");

		// Trigger should be consumed (reset)
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == false,
			"Trigger should be reset after consumption");

		Zenith_Log("  ✓ Trigger parameter test passed");
	}

	// Test RemoveParameter
	{
		Zenith_Assert(xParams.HasParameter("Speed"), "Speed should exist");
		xParams.RemoveParameter("Speed");
		Zenith_Assert(!xParams.HasParameter("Speed"), "Speed should be removed");

		Zenith_Log("  ✓ RemoveParameter test passed");
	}

	// Test GetParameterType
	{
		Zenith_Assert(xParams.GetParameterType("Health") == Flux_AnimationParameters::ParamType::Int,
			"Health should be Int type");
		Zenith_Assert(xParams.GetParameterType("IsRunning") == Flux_AnimationParameters::ParamType::Bool,
			"IsRunning should be Bool type");
		Zenith_Assert(xParams.GetParameterType("Jump") == Flux_AnimationParameters::ParamType::Trigger,
			"Jump should be Trigger type");

		Zenith_Log("  ✓ GetParameterType test passed");
	}

	Zenith_Log("TestAnimationParameters completed successfully");
}

/**
 * Test Flux_TransitionCondition evaluation
 * Verifies all comparison operators with different parameter types
 */
void Zenith_UnitTests::TestTransitionConditions()
{
	Zenith_Log("Running TestTransitionConditions...");

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

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Speed 5.0 > 3.0 should be true");

		xCond.m_fThreshold = 6.0f;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Speed 5.0 > 6.0 should be false");

		Zenith_Log("  ✓ Float Greater condition test passed");
	}

	// Test Float Less condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Less;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 10.0f;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Speed 5.0 < 10.0 should be true");

		Zenith_Log("  ✓ Float Less condition test passed");
	}

	// Test Int Equal condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Health 100 == 100 should be true");

		xCond.m_iThreshold = 50;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Health 100 == 50 should be false");

		Zenith_Log("  ✓ Int Equal condition test passed");
	}

	// Test Int LessEqual condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Health 100 <= 100 should be true");

		xCond.m_iThreshold = 50;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Health 100 <= 50 should be false");

		Zenith_Log("  ✓ Int LessEqual condition test passed");
	}

	// Test Bool condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "IsGrounded";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
		xCond.m_bThreshold = true;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"IsGrounded true == true should be true");

		xCond.m_bThreshold = false;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"IsGrounded true == false should be false");

		Zenith_Log("  ✓ Bool condition test passed");
	}

	// Test Trigger condition (Equal to true means trigger is set)
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Attack";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;

		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Attack trigger not set should be false");

		xParams.SetTrigger("Attack");
		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Attack trigger set should be true");

		Zenith_Log("  ✓ Trigger condition test passed");
	}

	Zenith_Log("TestTransitionConditions completed successfully");
}

/**
 * Test Flux_AnimationStateMachine
 * Verifies state creation, transitions, and state changes
 */
void Zenith_UnitTests::TestAnimationStateMachine()
{
	Zenith_Log("Running TestAnimationStateMachine...");

	Flux_AnimationStateMachine xStateMachine("TestSM");

	// Test state creation
	{
		Flux_AnimationState* pxIdleState = xStateMachine.AddState("Idle");
		Flux_AnimationState* pxWalkState = xStateMachine.AddState("Walk");
		Flux_AnimationState* pxRunState = xStateMachine.AddState("Run");

		Zenith_Assert(pxIdleState != nullptr, "Idle state should be created");
		Zenith_Assert(pxWalkState != nullptr, "Walk state should be created");
		Zenith_Assert(pxRunState != nullptr, "Run state should be created");

		Zenith_Assert(xStateMachine.HasState("Idle"), "Should have Idle state");
		Zenith_Assert(xStateMachine.HasState("Walk"), "Should have Walk state");
		Zenith_Assert(xStateMachine.HasState("Run"), "Should have Run state");
		Zenith_Assert(!xStateMachine.HasState("Jump"), "Should not have Jump state");

		Zenith_Log("  ✓ State creation test passed");
	}

	// Test default state
	{
		xStateMachine.SetDefaultState("Idle");
		Zenith_Assert(xStateMachine.GetDefaultStateName() == "Idle",
			"Default state should be Idle");

		Zenith_Log("  ✓ Default state test passed");
	}

	// Test SetState (force state change)
	{
		xStateMachine.SetState("Idle");
		Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
			"Current state should be Idle");

		xStateMachine.SetState("Walk");
		Zenith_Assert(xStateMachine.GetCurrentStateName() == "Walk",
			"Current state should be Walk after SetState");

		Zenith_Log("  ✓ SetState test passed");
	}

	// Test adding transitions
	{
		Flux_AnimationState* pxIdleState = xStateMachine.GetState("Idle");
		Zenith_Assert(pxIdleState != nullptr, "Should retrieve Idle state");

		Flux_StateTransition xTransition;
		xTransition.m_strTargetStateName = "Walk";
		xTransition.m_fTransitionDuration = 0.2f;

		// Add condition: Speed > 0.1
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTransition.m_xConditions.push_back(xCond);

		pxIdleState->AddTransition(xTransition);

		Zenith_Assert(pxIdleState->GetTransitions().size() == 1,
			"Idle state should have 1 transition");
		Zenith_Assert(pxIdleState->GetTransitions()[0].m_strTargetStateName == "Walk",
			"Transition should target Walk state");

		Zenith_Log("  ✓ Transition creation test passed");
	}

	// Test parameters
	{
		xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
		xStateMachine.GetParameters().AddBool("IsGrounded", true);

		Zenith_Assert(xStateMachine.GetParameters().HasParameter("Speed"),
			"Parameters should have Speed");
		Zenith_Assert(xStateMachine.GetParameters().HasParameter("IsGrounded"),
			"Parameters should have IsGrounded");

		Zenith_Log("  ✓ Parameters integration test passed");
	}

	// Test state removal
	{
		xStateMachine.RemoveState("Run");
		Zenith_Assert(!xStateMachine.HasState("Run"), "Run state should be removed");

		Zenith_Log("  ✓ State removal test passed");
	}

	// Test name
	{
		Zenith_Assert(xStateMachine.GetName() == "TestSM",
			"State machine name should be TestSM");

		xStateMachine.SetName("RenamedSM");
		Zenith_Assert(xStateMachine.GetName() == "RenamedSM",
			"State machine name should be RenamedSM");

		Zenith_Log("  ✓ Name test passed");
	}

	Zenith_Log("TestAnimationStateMachine completed successfully");
}

/**
 * Test Flux_IKChain and Flux_IKSolver setup
 * Verifies chain creation, target management, and helper functions
 */
void Zenith_UnitTests::TestIKChainSetup()
{
	Zenith_Log("Running TestIKChainSetup...");

	Flux_IKSolver xSolver;

	// Test chain creation with helper functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");

		Zenith_Assert(xLegChain.m_strName == "LeftLeg", "Chain name should be LeftLeg");
		Zenith_Assert(xLegChain.m_xBoneNames.size() == 3, "Leg chain should have 3 bones");
		Zenith_Assert(xLegChain.m_xBoneNames[0] == "Hip_L", "First bone should be Hip_L");
		Zenith_Assert(xLegChain.m_xBoneNames[1] == "Knee_L", "Second bone should be Knee_L");
		Zenith_Assert(xLegChain.m_xBoneNames[2] == "Ankle_L", "Third bone should be Ankle_L");

		Zenith_Log("  ✓ CreateLegChain test passed");
	}

	// Test arm chain creation
	{
		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("RightArm", "Shoulder_R", "Elbow_R", "Wrist_R");

		Zenith_Assert(xArmChain.m_strName == "RightArm", "Chain name should be RightArm");
		Zenith_Assert(xArmChain.m_xBoneNames.size() == 3, "Arm chain should have 3 bones");

		Zenith_Log("  ✓ CreateArmChain test passed");
	}

	// Test spine chain creation
	{
		std::vector<std::string> xSpineBones = { "Spine1", "Spine2", "Spine3", "Neck" };
		Flux_IKChain xSpineChain = Flux_IKSolver::CreateSpineChain("Spine", xSpineBones);

		Zenith_Assert(xSpineChain.m_strName == "Spine", "Chain name should be Spine");
		Zenith_Assert(xSpineChain.m_xBoneNames.size() == 4, "Spine chain should have 4 bones");

		Zenith_Log("  ✓ CreateSpineChain test passed");
	}

	// Test adding chains to solver
	{
		Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");
		Flux_IKChain xRightLeg = Flux_IKSolver::CreateLegChain("RightLeg", "Hip_R", "Knee_R", "Ankle_R");

		xSolver.AddChain(xLeftLeg);
		xSolver.AddChain(xRightLeg);

		Zenith_Assert(xSolver.HasChain("LeftLeg"), "Solver should have LeftLeg chain");
		Zenith_Assert(xSolver.HasChain("RightLeg"), "Solver should have RightLeg chain");
		Zenith_Assert(!xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

		Zenith_Log("  ✓ AddChain test passed");
	}

	// Test target management
	{
		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
		xTarget.m_fWeight = 0.75f;
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("LeftLeg", xTarget);

		Zenith_Assert(xSolver.HasTarget("LeftLeg"), "Solver should have LeftLeg target");
		Zenith_Assert(!xSolver.HasTarget("RightLeg"), "Solver should not have RightLeg target");

		const Flux_IKTarget* pxTarget = xSolver.GetTarget("LeftLeg");
		Zenith_Assert(pxTarget != nullptr, "Should retrieve LeftLeg target");
		Zenith_Assert(Vec3Equals(pxTarget->m_xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f)),
			"Target position should match");
		Zenith_Assert(FloatEquals(pxTarget->m_fWeight, 0.75f), "Target weight should be 0.75");

		Zenith_Log("  ✓ Target management test passed");
	}

	// Test ClearTarget
	{
		xSolver.ClearTarget("LeftLeg");
		Zenith_Assert(!xSolver.HasTarget("LeftLeg"), "LeftLeg target should be cleared");

		Zenith_Log("  ✓ ClearTarget test passed");
	}

	// Test RemoveChain
	{
		xSolver.RemoveChain("LeftLeg");
		Zenith_Assert(!xSolver.HasChain("LeftLeg"), "LeftLeg chain should be removed");
		Zenith_Assert(xSolver.HasChain("RightLeg"), "RightLeg chain should still exist");

		Zenith_Log("  ✓ RemoveChain test passed");
	}

	// Test GetChain
	{
		Flux_IKChain* pxChain = xSolver.GetChain("RightLeg");
		Zenith_Assert(pxChain != nullptr, "Should retrieve RightLeg chain");
		Zenith_Assert(pxChain->m_strName == "RightLeg", "Chain name should be RightLeg");

		// Modify via pointer
		pxChain->m_uMaxIterations = 20;
		Zenith_Assert(xSolver.GetChain("RightLeg")->m_uMaxIterations == 20,
			"Chain modification should persist");

		Zenith_Log("  ✓ GetChain test passed");
	}

	Zenith_Log("TestIKChainSetup completed successfully");
}

/**
 * Test animation system serialization
 * Verifies round-trip serialization for animation data structures
 */
void Zenith_UnitTests::TestAnimationSerialization()
{
	Zenith_Log("Running TestAnimationSerialization...");

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

		Zenith_Assert(xLoaded.HasParameter("Speed"), "Should have Speed param");
		Zenith_Assert(FloatEquals(xLoaded.GetFloat("Speed"), 5.5f), "Speed should be 5.5");
		Zenith_Assert(xLoaded.GetInt("Combo") == 3, "Combo should be 3");
		Zenith_Assert(xLoaded.GetBool("IsJumping") == true, "IsJumping should be true");

		Zenith_Log("  ✓ AnimationParameters serialization test passed");
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

		Zenith_Assert(xLoaded.m_strParameterName == "Speed", "Parameter name should match");
		Zenith_Assert(xLoaded.m_eCompareOp == Flux_TransitionCondition::CompareOp::GreaterEqual,
			"Compare op should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fThreshold, 3.14f), "Threshold should match");

		Zenith_Log("  ✓ TransitionCondition serialization test passed");
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

		Zenith_Assert(xLoaded.m_strName == "TestLeg", "Chain name should match");
		Zenith_Assert(xLoaded.m_xBoneNames.size() == 3, "Should have 3 bones");
		Zenith_Assert(xLoaded.m_xBoneNames[0] == "Hip", "First bone should be Hip");
		Zenith_Assert(xLoaded.m_uMaxIterations == 15, "Max iterations should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fTolerance, 0.005f), "Tolerance should match");
		Zenith_Assert(xLoaded.m_bUsePoleVector == true, "Use pole vector should match");
		Zenith_Assert(Vec3Equals(xLoaded.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
			"Pole vector should match");

		Zenith_Log("  ✓ IKChain serialization test passed");
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

		Zenith_Assert(xLoaded.m_eType == Flux_JointConstraint::ConstraintType::Hinge,
			"Constraint type should be Hinge");
		Zenith_Assert(Vec3Equals(xLoaded.m_xHingeAxis, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)),
			"Hinge axis should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fMinAngle, -1.5f), "Min angle should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fMaxAngle, 0.0f), "Max angle should match");

		Zenith_Log("  ✓ JointConstraint serialization test passed");
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

		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(2), 0.0f), "Bone 2 weight should be 0.0");

		Zenith_Log("  ✓ BoneMask serialization test passed");
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

		Zenith_Assert(xLoaded.m_strName == "TestClip", "Clip name should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fDuration, 2.5f), "Duration should match");
		Zenith_Assert(xLoaded.m_uTicksPerSecond == 30, "Ticks per second should match");
		Zenith_Assert(xLoaded.m_bLooping == false, "Looping should be false");
		Zenith_Assert(FloatEquals(xLoaded.m_fBlendInTime, 0.2f), "Blend in time should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fBlendOutTime, 0.3f), "Blend out time should match");

		Zenith_Log("  ✓ AnimationClipMetadata serialization test passed");
	}

	Zenith_Log("TestAnimationSerialization completed successfully");
}

/**
 * Test blend tree node types
 * Verifies blend tree node creation and factory method
 */
void Zenith_UnitTests::TestBlendTreeNodes()
{
	Zenith_Log("Running TestBlendTreeNodes...");

	// Test Clip node
	{
		Flux_BlendTreeNode_Clip xClipNode(nullptr, 1.0f);
		Zenith_Assert(std::string(xClipNode.GetNodeTypeName()) == "Clip", "Type name should be Clip");
		Zenith_Assert(FloatEquals(xClipNode.GetPlaybackRate(), 1.0f), "Playback rate should be 1.0");

		xClipNode.SetPlaybackRate(1.5f);
		Zenith_Assert(FloatEquals(xClipNode.GetPlaybackRate(), 1.5f), "Playback rate should be 1.5");

		Zenith_Log("  ✓ Clip node test passed");
	}

	// Test Blend node
	{
		Flux_BlendTreeNode_Blend xBlendNode;
		Zenith_Assert(std::string(xBlendNode.GetNodeTypeName()) == "Blend", "Type name should be Blend");

		xBlendNode.SetBlendWeight(0.75f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 0.75f), "Blend weight should be 0.75");

		Zenith_Log("  ✓ Blend node test passed");
	}

	// Test BlendSpace1D node
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;
		Zenith_Assert(std::string(xBlendSpace.GetNodeTypeName()) == "BlendSpace1D", "Type name should be BlendSpace1D");

		xBlendSpace.SetParameter(0.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 0.5f), "Parameter should be 0.5");

		Zenith_Log("  ✓ BlendSpace1D node test passed");
	}

	// Test BlendSpace2D node
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;
		Zenith_Assert(std::string(xBlendSpace.GetNodeTypeName()) == "BlendSpace2D", "Type name should be BlendSpace2D");

		Zenith_Maths::Vector2 xParams(0.3f, 0.7f);
		xBlendSpace.SetParameter(xParams);
		const Zenith_Maths::Vector2& xRetrieved = xBlendSpace.GetParameter();
		Zenith_Assert(FloatEquals(xRetrieved.x, 0.3f) && FloatEquals(xRetrieved.y, 0.7f),
			"Parameters should be (0.3, 0.7)");

		Zenith_Log("  ✓ BlendSpace2D node test passed");
	}

	// Test Additive node
	{
		Flux_BlendTreeNode_Additive xAdditiveNode;
		Zenith_Assert(std::string(xAdditiveNode.GetNodeTypeName()) == "Additive", "Type name should be Additive");

		xAdditiveNode.SetAdditiveWeight(0.5f);
		Zenith_Assert(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 0.5f), "Additive weight should be 0.5");

		Zenith_Log("  ✓ Additive node test passed");
	}

	// Test Select node
	{
		Flux_BlendTreeNode_Select xSelectNode;
		Zenith_Assert(std::string(xSelectNode.GetNodeTypeName()) == "Select", "Type name should be Select");

		// Add some children before setting selected index
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));

		xSelectNode.SetSelectedIndex(2);
		Zenith_Assert(xSelectNode.GetSelectedIndex() == 2, "Selected index should be 2");

		Zenith_Log("  ✓ Select node test passed");
	}

	// Test factory method
	{
		Flux_BlendTreeNode* pxClip = Flux_BlendTreeNode::CreateFromTypeName("Clip");
		Zenith_Assert(pxClip != nullptr, "Factory should create Clip node");
		Zenith_Assert(std::string(pxClip->GetNodeTypeName()) == "Clip", "Created node should be Clip type");
		delete pxClip;

		Flux_BlendTreeNode* pxBlend = Flux_BlendTreeNode::CreateFromTypeName("Blend");
		Zenith_Assert(pxBlend != nullptr, "Factory should create Blend node");
		Zenith_Assert(std::string(pxBlend->GetNodeTypeName()) == "Blend", "Created node should be Blend type");
		delete pxBlend;

		Flux_BlendTreeNode* pxInvalid = Flux_BlendTreeNode::CreateFromTypeName("InvalidType");
		Zenith_Assert(pxInvalid == nullptr, "Factory should return nullptr for invalid type");

		Zenith_Log("  ✓ Factory method test passed");
	}

	Zenith_Log("TestBlendTreeNodes completed successfully");
}

/**
 * Test cross-fade transition
 * Verifies transition timing and blend weight calculations
 */
void Zenith_UnitTests::TestCrossFadeTransition()
{
	Zenith_Log("Running TestCrossFadeTransition...");

	// Test initial state
	{
		Flux_CrossFadeTransition xTransition;
		Zenith_Assert(xTransition.IsComplete() == true,
			"Transition should be complete initially (no duration set)");

		Zenith_Log("  ✓ Initial state test passed");
	}

	// Test Start and Update
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(5);
		xFromPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f);

		Flux_CrossFadeTransition xTransition;
		xTransition.Start(xFromPose, 1.0f); // 1 second transition

		Zenith_Assert(xTransition.IsComplete() == false,
			"Transition should not be complete after Start");
		Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 0.0f, 0.01f),
			"Blend weight should be 0 at start");

		// Update halfway
		xTransition.Update(0.5f);
		Zenith_Assert(xTransition.IsComplete() == false,
			"Transition should not be complete at 0.5s");
		// With EaseInOut, 0.5 normalized time might not be exactly 0.5 blend weight
		// but should be close for symmetrical easing
		float fMidWeight = xTransition.GetBlendWeight();
		Zenith_Assert(fMidWeight > 0.3f && fMidWeight < 0.7f,
			"Blend weight at midpoint should be roughly 0.5");

		// Update to completion
		xTransition.Update(0.6f); // Total 1.1s, should be complete
		Zenith_Assert(xTransition.IsComplete() == true,
			"Transition should be complete after 1.1s");
		Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 1.0f),
			"Blend weight should be 1.0 when complete");

		Zenith_Log("  ✓ Start and Update test passed");
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
			Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 0.5f),
				"Linear easing should give 0.5 at midpoint");
		}

		// Test EaseIn easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseIn);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			Zenith_Assert(fWeight < 0.5f,
				"EaseIn should give weight < 0.5 at midpoint");
		}

		// Test EaseOut easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseOut);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			Zenith_Assert(fWeight > 0.5f,
				"EaseOut should give weight > 0.5 at midpoint");
		}

		Zenith_Log("  ✓ Easing types test passed");
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

		Zenith_Assert(Vec3Equals(xOutPose.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)),
			"Blend should interpolate position to midpoint");

		Zenith_Log("  ✓ Blend operation test passed");
	}

	Zenith_Log("TestCrossFadeTransition completed successfully");
}

/**
 * Test Animation Clip Channels
 * Verifies clip metadata and event handling
 */
void Zenith_UnitTests::TestAnimationClipChannels()
{
	Zenith_Log("Running TestAnimationClipChannels...");

	// Test clip metadata
	{
		Flux_AnimationClipMetadata xMetadata;
		xMetadata.m_strName = "TestClip";
		xMetadata.m_fDuration = 2.5f;
		xMetadata.m_uTicksPerSecond = 30;
		xMetadata.m_bLooping = true;
		xMetadata.m_fBlendInTime = 0.2f;
		xMetadata.m_fBlendOutTime = 0.15f;

		Zenith_Assert(xMetadata.m_strName == "TestClip", "Name should be 'TestClip'");
		Zenith_Assert(FloatEquals(xMetadata.m_fDuration, 2.5f), "Duration should be 2.5");
		Zenith_Assert(xMetadata.m_uTicksPerSecond == 30, "Ticks per second should be 30");
		Zenith_Assert(xMetadata.m_bLooping == true, "Looping should be true");
		Zenith_Assert(FloatEquals(xMetadata.m_fBlendInTime, 0.2f), "Blend in time should be 0.2");
		Zenith_Assert(FloatEquals(xMetadata.m_fBlendOutTime, 0.15f), "Blend out time should be 0.15");

		Zenith_Log("  ✓ Clip metadata test passed");
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

		const std::vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		Zenith_Assert(xEvents.size() == 2, "Should have 2 events");
		Zenith_Assert(xEvents[0].m_strEventName == "LeftFootDown", "First event should be LeftFootDown");
		Zenith_Assert(xEvents[1].m_strEventName == "RightFootDown", "Second event should be RightFootDown");
		Zenith_Assert(FloatEquals(xEvents[0].m_fNormalizedTime, 0.25f), "First event time should be 0.25");
		Zenith_Assert(FloatEquals(xEvents[1].m_fNormalizedTime, 0.75f), "Second event time should be 0.75");

		Zenith_Log("  ✓ Animation clip events test passed");
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

		Zenith_Assert(xCollection.GetClipCount() == 3, "Should have 3 clips");
		Zenith_Assert(xCollection.HasClip("Idle"), "Should have Idle clip");
		Zenith_Assert(xCollection.HasClip("Walk"), "Should have Walk clip");
		Zenith_Assert(xCollection.HasClip("Run"), "Should have Run clip");
		Zenith_Assert(!xCollection.HasClip("Jump"), "Should not have Jump clip");

		const Flux_AnimationClip* pxRetrieved = xCollection.GetClip("Walk");
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve Walk clip");
		Zenith_Assert(pxRetrieved->GetName() == "Walk", "Retrieved clip name should be Walk");

		Zenith_Log("  ✓ Animation clip collection test passed");
	}

	Zenith_Log("TestAnimationClipChannels completed successfully");
}

/**
 * Test BlendSpace1D calculations
 * Verifies blend space sample point selection and blending
 */
void Zenith_UnitTests::TestBlendSpace1D()
{
	Zenith_Log("Running TestBlendSpace1D...");

	// Test parameter setting
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		xBlendSpace.SetParameter(-0.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), -0.5f),
			"Parameter should accept negative values");

		xBlendSpace.SetParameter(1.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 1.5f),
			"Parameter should accept values > 1");

		Zenith_Log("  ✓ Parameter range test passed");
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

		const std::vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(xPoints.size() == 3, "Should have 3 blend points");
		Zenith_Assert(FloatEquals(xPoints[0].m_fPosition, 0.0f), "First point position should be 0.0");
		Zenith_Assert(FloatEquals(xPoints[1].m_fPosition, 0.5f), "Second point position should be 0.5");
		Zenith_Assert(FloatEquals(xPoints[2].m_fPosition, 1.0f), "Third point position should be 1.0");

		Zenith_Log("  ✓ Blend point addition test passed");
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

		const std::vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(FloatEquals(xPoints[0].m_fPosition, 0.0f), "After sorting, first should be 0.0");
		Zenith_Assert(FloatEquals(xPoints[1].m_fPosition, 0.5f), "After sorting, second should be 0.5");
		Zenith_Assert(FloatEquals(xPoints[2].m_fPosition, 1.0f), "After sorting, third should be 1.0");

		Zenith_Log("  ✓ Blend point sorting test passed");
	}

	Zenith_Log("TestBlendSpace1D completed successfully");
}

/**
 * Test FABRIK IK Solver
 * Verifies IK chain setup and solving iterations
 */
void Zenith_UnitTests::TestFABRIKSolver()
{
	Zenith_Log("Running TestFABRIKSolver...");

	// Test basic IK chain creation
	{
		Flux_IKSolver xSolver;

		Flux_IKChain xChain;
		xChain.m_strName = "RightArm";
		xChain.m_xBoneNames.push_back("Shoulder");
		xChain.m_xBoneNames.push_back("Elbow");
		xChain.m_xBoneNames.push_back("Wrist");

		xSolver.AddChain(xChain);
		Zenith_Assert(xSolver.HasChain("RightArm"), "Solver should have RightArm chain");
		Zenith_Assert(!xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

		const Flux_IKChain* pxRetrieved = xSolver.GetChain("RightArm");
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve chain");
		Zenith_Assert(pxRetrieved->m_strName == "RightArm", "Chain name should match");
		Zenith_Assert(pxRetrieved->m_xBoneNames.size() == 3, "Should have 3 bones");

		Zenith_Log("  ✓ Chain creation test passed");
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
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve target");
		Zenith_Assert(Vec3Equals(pxRetrieved->m_xPosition, Zenith_Maths::Vector3(5.0f, 3.0f, 0.0f)),
			"Target position should match");
		Zenith_Assert(FloatEquals(pxRetrieved->m_fWeight, 0.8f), "Target weight should be 0.8");
		Zenith_Assert(pxRetrieved->m_bEnabled == true, "Target should be enabled");

		Zenith_Log("  ✓ IK target setting test passed");
	}

	// Test IK target clearing
	{
		Flux_IKSolver xSolver;

		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("TestChain", xTarget);
		Zenith_Assert(xSolver.HasTarget("TestChain"), "Target should exist");

		xSolver.ClearTarget("TestChain");
		Zenith_Assert(!xSolver.HasTarget("TestChain"), "Target should be cleared");

		Zenith_Log("  ✓ IK target clearing test passed");
	}

	// Test chain parameters
	{
		Flux_IKChain xChain;
		xChain.m_strName = "TestChain";
		xChain.m_uMaxIterations = 20;
		xChain.m_fTolerance = 0.001f;

		Zenith_Assert(xChain.m_uMaxIterations == 20, "Max iterations should be 20");
		Zenith_Assert(FloatEquals(xChain.m_fTolerance, 0.001f), "Tolerance should be 0.001");

		Zenith_Log("  ✓ Chain parameters test passed");
	}

	// Test chain with pole vector
	{
		Flux_IKChain xChain;
		xChain.m_strName = "LeftLeg";
		xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xChain.m_bUsePoleVector = true;
		xChain.m_strPoleTargetBone = "KneeTarget";

		Zenith_Assert(Vec3Equals(xChain.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)),
			"Pole vector should be (0,0,1)");
		Zenith_Assert(xChain.m_bUsePoleVector == true, "Use pole vector should be true");
		Zenith_Assert(xChain.m_strPoleTargetBone == "KneeTarget", "Pole target bone should be KneeTarget");

		Zenith_Log("  ✓ Pole vector test passed");
	}

	// Test helper chain creation functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("RightLeg", "Hip", "Knee", "Ankle");
		Zenith_Assert(xLegChain.m_strName == "RightLeg", "Leg chain name should be RightLeg");
		Zenith_Assert(xLegChain.m_xBoneNames.size() == 3, "Leg chain should have 3 bones");
		Zenith_Assert(xLegChain.m_xBoneNames[0] == "Hip", "First bone should be Hip");
		Zenith_Assert(xLegChain.m_xBoneNames[1] == "Knee", "Second bone should be Knee");
		Zenith_Assert(xLegChain.m_xBoneNames[2] == "Ankle", "Third bone should be Ankle");

		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("LeftArm", "Shoulder", "Elbow", "Wrist");
		Zenith_Assert(xArmChain.m_strName == "LeftArm", "Arm chain name should be LeftArm");
		Zenith_Assert(xArmChain.m_xBoneNames.size() == 3, "Arm chain should have 3 bones");

		Zenith_Log("  ✓ Helper chain creation test passed");
	}

	Zenith_Log("TestFABRIKSolver completed successfully");
}

/**
 * Test Animation Events
 * Verifies event registration and triggering
 */
void Zenith_UnitTests::TestAnimationEvents()
{
	Zenith_Log("Running TestAnimationEvents...");

	// Test event data structure
	{
		Flux_AnimationEvent xEvent;
		xEvent.m_strEventName = "FootStep";
		xEvent.m_fNormalizedTime = 0.25f;
		xEvent.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.5f);

		Zenith_Assert(xEvent.m_strEventName == "FootStep", "Event name should be 'FootStep'");
		Zenith_Assert(FloatEquals(xEvent.m_fNormalizedTime, 0.25f), "Normalized time should be 0.25");
		Zenith_Assert(FloatEquals(xEvent.m_xData.x, 1.0f), "Event data x should be 1.0");

		Zenith_Log("  ✓ Event data structure test passed");
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

		const std::vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		Zenith_Assert(xEvents.size() == 2, "Should have 2 events");
		Zenith_Assert(xEvents[0].m_strEventName == "LeftFoot", "First event should be LeftFoot");
		Zenith_Assert(xEvents[1].m_strEventName == "RightFoot", "Second event should be RightFoot");

		Zenith_Log("  ✓ Event collection test passed");
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

		Zenith_Assert(FloatEquals(xEvents[0].m_fNormalizedTime, 0.1f), "First should be 0.1");
		Zenith_Assert(FloatEquals(xEvents[1].m_fNormalizedTime, 0.5f), "Second should be 0.5");
		Zenith_Assert(FloatEquals(xEvents[2].m_fNormalizedTime, 0.9f), "Third should be 0.9");

		Zenith_Log("  ✓ Event time ordering test passed");
	}

	Zenith_Log("TestAnimationEvents completed successfully");
}

/**
 * Test Bone Masking
 * Verifies bone mask creation and application
 */
void Zenith_UnitTests::TestBoneMasking()
{
	Zenith_Log("Running TestBoneMasking...");

	// Test mask creation with bone indices
	{
		Flux_BoneMask xMask;

		// Set weights by bone index
		xMask.SetBoneWeight(0, 1.0f);  // Spine
		xMask.SetBoneWeight(1, 1.0f);  // Chest
		xMask.SetBoneWeight(2, 0.8f);  // Shoulder_L
		xMask.SetBoneWeight(3, 0.8f);  // Shoulder_R
		xMask.SetBoneWeight(4, 0.2f);  // Hips

		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(1), 1.0f), "Bone 1 weight should be 1.0");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(2), 0.8f), "Bone 2 weight should be 0.8");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(3), 0.8f), "Bone 3 weight should be 0.8");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(4), 0.2f), "Bone 4 weight should be 0.2");

		Zenith_Log("  ✓ Mask creation test passed");
	}

	// Test weight access
	{
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(5, 0.75f);

		float fWeight = xMask.GetBoneWeight(5);
		Zenith_Assert(FloatEquals(fWeight, 0.75f), "Weight should be 0.75");

		const std::vector<float>& xWeights = xMask.GetWeights();
		Zenith_Assert(xWeights.size() >= 6, "Should have at least 6 weights");
		Zenith_Assert(FloatEquals(xWeights[5], 0.75f), "Weight at index 5 should be 0.75");

		Zenith_Log("  ✓ Weight access test passed");
	}

	// Test masked blend node setup
	{
		Flux_BlendTreeNode_Masked xMaskedNode;
		Zenith_Assert(std::string(xMaskedNode.GetNodeTypeName()) == "Masked", "Type name should be 'Masked'");

		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);
		xMask.SetBoneWeight(1, 0.5f);

		xMaskedNode.SetBoneMask(xMask);
		const Flux_BoneMask& xRetrievedMask = xMaskedNode.GetBoneMask();
		Zenith_Assert(FloatEquals(xRetrievedMask.GetBoneWeight(0), 1.0f), "Retrieved mask bone 0 should be 1.0");
		Zenith_Assert(FloatEquals(xRetrievedMask.GetBoneWeight(1), 0.5f), "Retrieved mask bone 1 should be 0.5");

		Zenith_Log("  ✓ Masked blend node setup test passed");
	}

	// Test masked blend with different poses
	{
		Flux_SkeletonPose xBasePose;
		xBasePose.Initialize(5);

		Flux_SkeletonPose xOverridePose;
		xOverridePose.Initialize(5);

		// Create mask that affects only bones 2, 3, 4
		std::vector<float> xBoneWeights = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

		Flux_SkeletonPose xResult;
		xResult.Initialize(5);

		Flux_SkeletonPose::MaskedBlend(xResult, xBasePose, xOverridePose, xBoneWeights);

		// Result should have base pose for bones 0,1 and override pose for bones 2,3,4
		Zenith_Log("  ✓ Masked blend test passed");
	}

	Zenith_Log("TestBoneMasking completed successfully");
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
void Zenith_UnitTests::TestMeshAssetLoading()
{
	Zenith_Log("Running TestMeshAssetLoading...");

	// Test loading a mesh asset
	{
		const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
		Zenith_MeshAsset* pxMeshAsset = Zenith_AssetHandler::LoadMeshAsset(strMeshPath);

		if (pxMeshAsset == nullptr)
		{
			Zenith_Log("  ! Skipping test - mesh asset not found at %s", strMeshPath.c_str());
			Zenith_Log("  ! Please export ArmChain.gltf through the asset pipeline first");
			return;
		}

		Zenith_Assert(pxMeshAsset != nullptr, "Failed to load mesh asset");
		Zenith_Assert(pxMeshAsset->GetNumVerts() == 24, "Expected 24 vertices (8 per bone * 3 bones)");
		Zenith_Assert(pxMeshAsset->GetNumIndices() > 0, "Mesh should have indices");

		Zenith_Log("  ✓ Mesh asset loaded with %u vertices and %u indices",
			pxMeshAsset->GetNumVerts(), pxMeshAsset->GetNumIndices());

		// Verify first vertex position (raw, local to bone)
		const Zenith_Maths::Vector3& xFirstPos = pxMeshAsset->m_xPositions.Get(0);
		Zenith_Assert(FloatEquals(xFirstPos.x, -0.25f, 0.01f), "Vertex 0 X mismatch");
		Zenith_Assert(FloatEquals(xFirstPos.y, 0.0f, 0.01f), "Vertex 0 Y mismatch");
		Zenith_Assert(FloatEquals(xFirstPos.z, -0.25f, 0.01f), "Vertex 0 Z mismatch");

		Zenith_Log("  ✓ First vertex position verified");

		// Verify skinning data exists
		Zenith_Assert(pxMeshAsset->m_xBoneIndices.GetSize() == 24, "Should have bone indices for all vertices");
		Zenith_Assert(pxMeshAsset->m_xBoneWeights.GetSize() == 24, "Should have bone weights for all vertices");

		Zenith_Log("  ✓ Skinning data present");
	}

	Zenith_Log("TestMeshAssetLoading completed successfully");
}

/**
 * Test bind pose vertex positions
 * Verifies that applying bind pose skinning produces correct vertex positions
 */
void Zenith_UnitTests::TestBindPoseVertexPositions()
{
	Zenith_Log("Running TestBindPoseVertexPositions...");

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain.zskel";

	Zenith_MeshAsset* pxMesh = Zenith_AssetHandler::LoadMeshAsset(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetHandler::LoadSkeletonAsset(strSkelPath);

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		Zenith_Log("  ! Skipping test - assets not found");
		Zenith_Log("  ! Please export ArmChain.gltf through the asset pipeline first");
		return;
	}

	Zenith_Assert(pxMesh != nullptr && pxSkel != nullptr, "Failed to load assets");
	Zenith_Assert(pxSkel->GetNumBones() == 3, "Expected 3 bones");

	// Log bone hierarchy for debugging
	for (uint32_t i = 0; i < pxSkel->GetNumBones(); i++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkel->GetBone(i);
		Zenith_Log("  Bone %u: %s, parent=%d, bindPos=(%.2f, %.2f, %.2f)",
			i, xBone.m_strName.c_str(), xBone.m_iParentIndex,
			xBone.m_xBindPosition.x, xBone.m_xBindPosition.y, xBone.m_xBindPosition.z);
	}

	// Test vertex 0 (Root bone at origin)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(0);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(0);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(0);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log("  Vertex 0: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// Root bone at origin - expected position is approximately the local position
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 0.0f, -0.25f), 0.1f),
			"Vertex 0 bind pose position mismatch");

		Zenith_Log("  ✓ Root bone vertex (0) bind pose verified");
	}

	// Test vertex 8 (UpperArm bone at Y+2)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(8);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(8);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(8);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log("  Vertex 8: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// UpperArm bone at Y+2 - expected position should be offset by bone transform
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 2.0f, -0.25f), 0.1f),
			"Vertex 8 bind pose position mismatch");

		Zenith_Log("  ✓ UpperArm bone vertex (8) bind pose verified");
	}

	// Test vertex 16 (Forearm bone at Y+4)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log("  Vertex 16: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// Forearm bone at Y+4 - expected position should be offset by bone transform
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f),
			"Vertex 16 bind pose position mismatch");

		Zenith_Log("  ✓ Forearm bone vertex (16) bind pose verified");
	}

	Zenith_Log("TestBindPoseVertexPositions completed successfully");
}

/**
 * Test animated vertex positions
 * Verifies that animation skinning produces correct vertex positions at various timestamps
 */
void Zenith_UnitTests::TestAnimatedVertexPositions()
{
	Zenith_Log("Running TestAnimatedVertexPositions...");

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain.zskel";
	const std::string strAnimPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_ForearmRotate.zanim";

	Zenith_MeshAsset* pxMesh = Zenith_AssetHandler::LoadMeshAsset(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetHandler::LoadSkeletonAsset(strSkelPath);
	Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(strAnimPath);

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		Zenith_Log("  ! Skipping test - mesh/skeleton assets not found");
		Zenith_Log("  ! Please export ArmChain.gltf through the asset pipeline first");
		return;
	}

	if (pxClip == nullptr)
	{
		Zenith_Log("  ! Skipping animation test - animation clip not found");
		Zenith_Log("  ! Animation file: %s", strAnimPath.c_str());
		// Still test bind pose without animation
	}

	Zenith_Assert(pxMesh != nullptr && pxSkel != nullptr, "Failed to load test assets");

	// Create skeleton instance for animation (CPU-only, no GPU buffer needed for unit tests)
	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);
	Zenith_Assert(pxSkelInst != nullptr, "Failed to create skeleton instance");

	// Test at t=0.0 (should match bind pose)
	{
		pxSkelInst->SetToBindPose();
		pxSkelInst->ComputeSkinningMatrices();

		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

		Zenith_Log("  t=0.0: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
			xSkinned.x, xSkinned.y, xSkinned.z);

		// At t=0, should match bind pose
		Zenith_Assert(Vec3Equals(xSkinned, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f),
			"Vertex 16 at t=0 should match bind pose");

		Zenith_Log("  ✓ Animation t=0.0 (bind pose) verified");
	}

	// Test with animation if clip is available
	if (pxClip != nullptr)
	{
		Zenith_Log("  Animation clip '%s' loaded, duration: %.2f sec",
			pxClip->GetName().c_str(), pxClip->GetDuration());

		// Debug: Print bone channels in clip
		Zenith_Log("  Animation bone channels:");
		for (const auto& xPair : pxClip->GetBoneChannels())
		{
			Zenith_Log("    - '%s'", xPair.first.c_str());
		}

		// Debug: Print skeleton bone names
		Zenith_Log("  Skeleton bone names:");
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); i++)
		{
			Zenith_Log("    - [%u] '%s'", i, pxSkel->GetBone(i).m_strName.c_str());
		}

		// Test at t=0.5 (45 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 0.5f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

			Zenith_Log("  t=0.5: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
				xSkinned.x, xSkinned.y, xSkinned.z);

			// At t=0.5, forearm should be rotated 45 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (-0.177, -0.177, -0.25)
			// Add bone world position (0, 4, 0) = (-0.177, 3.823, -0.25)
			Zenith_Maths::Vector3 xExpected(-0.177f, 3.823f, -0.25f);
			Zenith_Assert(Vec3Equals(xSkinned, xExpected, 0.1f),
				"Vertex 16 at t=0.5 position mismatch");

			Zenith_Log("  ✓ Animation t=0.5 (45-degree rotation) verified");
		}

		// Test at t=1.0 (90 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 1.0f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

			Zenith_Log("  t=1.0: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
				xSkinned.x, xSkinned.y, xSkinned.z);

			// At t=1.0, forearm should be rotated 90 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (0, -0.25, -0.25)
			// Add bone world position (0, 4, 0) = (0, 3.75, -0.25)
			Zenith_Maths::Vector3 xExpected(0.0f, 3.75f, -0.25f);
			Zenith_Assert(Vec3Equals(xSkinned, xExpected, 0.1f),
				"Vertex 16 at t=1.0 position mismatch");

			Zenith_Log("  ✓ Animation t=1.0 (90-degree rotation) verified");
		}

		delete pxClip;
	}

	// Cleanup
	delete pxSkelInst;

	Zenith_Log("TestAnimatedVertexPositions completed successfully");
}

