#include "Zenith.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"

#include <cstring>

namespace
{
	u_int g_uCapturedLoadCount = 0u;
	u_int g_uCapturedBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;

	void CaptureLoadRequest(u_int uBuildIndex)
	{
		++g_uCapturedLoadCount;
		g_uCapturedBuildIndex = uBuildIndex;
	}

	struct LoadCallbackScope
	{
		LoadCallbackScope()
		{
			g_uCapturedLoadCount = 0u;
			g_uCapturedBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
			ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(
				&CaptureLoadRequest);
		}

		~LoadCallbackScope()
		{
			ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(nullptr);
		}
	};

	Zenith_SceneData* GetActiveSceneData()
	{
		return g_xEngine.Scenes().GetActiveSceneData();
	}

	bool ResetEmptyPhysicsWorld()
	{
		const u_int uColliderCount =
			g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().Count();
		ZENITH_ASSERT_EQ(uColliderCount, 0u,
			"traversal physics fixture requires no live colliders");
		if (uColliderCount != 0u)
		{
			return false;
		}

		g_xEngine.Physics().Reset();
		g_xEngine.Physics().m_fTimestepAccumulator = 0.0;
		return g_xEngine.Physics().HasActiveSimulation();
	}

	Zenith_Entity CreatePlayer(Zenith_SceneData* pxSceneData)
	{
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}

		Zenith_Entity xPlayer =
			g_xEngine.Scenes().CreateEntity(pxSceneData, "Player");
		Zenith_TransformComponent& xTransform =
			xPlayer.GetComponent<Zenith_TransformComponent>();
		xTransform.SetScale({ 0.8f, 1.8f, 0.8f });
		xPlayer.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xPlayer.AddComponent<ZM_PlayerController>();
		return xPlayer;
	}

	Zenith_Entity CreateAuthoritativeManager(Zenith_SceneData* pxSceneData)
	{
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}

		Zenith_Entity xManager =
			g_xEngine.Scenes().CreateEntity(pxSceneData, "ZM_GameStateRoot");
		xManager.AddComponent<ZM_GameStateManager>();
		const Zenith_EntityID xManagerID = xManager.GetEntityID();
		xManager.GetComponent<ZM_GameStateManager>().OnStart();
		return g_xEngine.Scenes().ResolveEntity(xManagerID);
	}

	Zenith_Entity CreateSpawnPoint(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const char* szTag)
	{
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}

		Zenith_Entity xSpawn =
			g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		ZM_SpawnPoint& xSpawnPoint = xSpawn.AddComponent<ZM_SpawnPoint>();
		if (szTag != nullptr)
		{
			xSpawnPoint.SetTag(szTag);
		}
		return xSpawn;
	}

	Zenith_Entity CreateBoxBody(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		RigidBodyType eBodyType)
	{
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}

		Zenith_Entity xEntity =
			g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
			{ 1.0f, 1.0f, 1.0f });
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_AABB, eBodyType);
		return xEntity;
	}

	Zenith_Entity CreateConfiguredTrigger(Zenith_SceneData* pxSceneData)
	{
		Zenith_Entity xTrigger = CreateBoxBody(
			pxSceneData, "WarpTrigger", RIGIDBODY_TYPE_STATIC);
		if (!xTrigger.IsValid())
		{
			return xTrigger;
		}

		ZM_WarpTrigger& xWarpTrigger =
			xTrigger.AddComponent<ZM_WarpTrigger>();
		xWarpTrigger.Configure(2u, "TownCenter");
		xWarpTrigger.OnStart();
		return xTrigger;
	}
}

ZENITH_TEST(ZM_WorldTraversal, ManagerSingletonLookupRejectsMissingAndDuplicates)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	Zenith_EntityID xSingletonID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_FALSE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, INVALID_ENTITY_ID);

	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xFirst =
		xScenes.CreateEntity(pxSceneData, "FirstManager");
	xFirst.AddComponent<ZM_GameStateManager>();
	const Zenith_EntityID xFirstID = xFirst.GetEntityID();
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, xFirstID);

	// OnStart establishes authority and relocates the same generation-bearing
	// entity into the persistent scene. Re-resolve after the pool move.
	xFirst.GetComponent<ZM_GameStateManager>().OnStart();
	Zenith_Entity xRelocatedFirst = xScenes.ResolveEntity(xFirstID);
	ZENITH_ASSERT_TRUE(xRelocatedFirst.IsValid());
	ZENITH_ASSERT_EQ(xRelocatedFirst.GetEntityID(), xFirstID);
	ZENITH_ASSERT_EQ(xScenes.GetSceneDataForEntity(xFirstID),
		xScenes.GetSceneData(xScenes.GetPersistentScene()));
	ZENITH_ASSERT_TRUE(xRelocatedFirst
		.GetComponent<ZM_GameStateManager>().IsAuthoritativeSingleton());

	Zenith_Entity xSecond =
		xScenes.CreateEntity(pxSceneData, "SecondManager");
	xSecond.AddComponent<ZM_GameStateManager>();
	const Zenith_EntityID xSecondID = xSecond.GetEntityID();
	ZENITH_ASSERT_FALSE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, INVALID_ENTITY_ID);

	// A duplicate's own OnStart must retire it without displacing the relocated
	// authority. Destroy() is deferred, so prove the public pending-work signal
	// and then drain it through the SceneSystem's public main-loop seam.
	xSecond.GetComponent<ZM_GameStateManager>().OnStart();
	ZENITH_ASSERT_TRUE(xScenes.HasPendingDestructions());
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, xFirstID);
	xScenes.Update(0.0f);
	ZENITH_ASSERT_FALSE(xScenes.ResolveEntity(xSecondID).IsValid());
	xRelocatedFirst = xScenes.ResolveEntity(xFirstID);
	ZENITH_ASSERT_TRUE(xRelocatedFirst.IsValid());
	ZENITH_ASSERT_TRUE(xRelocatedFirst
		.GetComponent<ZM_GameStateManager>().IsAuthoritativeSingleton());
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, xFirstID);
}

ZENITH_TEST(ZM_WorldTraversal, ManagerRequestValidationIsTransactional)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	if (!xPlayer.IsValid() || !xManagerEntity.IsValid()) { return; }

	ZM_PlayerController& xController =
		xPlayer.GetComponent<ZM_PlayerController>();
	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_FALSE(ZM_GameStateManager::RequestWarp(
		ZM_GameStateManager::uINVALID_BUILD_INDEX, "TownCenter"));
	ZENITH_ASSERT_FALSE(ZM_GameStateManager::RequestWarp(2u, "towncenter"));
	ZENITH_ASSERT_FALSE(ZM_GameStateManager::RequestWarp(2u, nullptr));
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ(xManager.GetTargetBuildIndex(),
		ZM_GameStateManager::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xManager.GetTargetSpawnTag(), "");
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_TRUE(xController.IsMovementEnabled());

	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ(xManager.GetTargetBuildIndex(), 2u);
	ZENITH_ASSERT_STREQ(xManager.GetTargetSpawnTag(), "TownCenter");
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), xPlayer.GetEntityID());
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());
}

ZENITH_TEST(ZM_WorldTraversal, ManagerStateMachineDefersSingleLoadOneTick)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	if (!xPlayer.IsValid() || !xManagerEntity.IsValid()) { return; }

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);

	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u);
	ZENITH_ASSERT_EQ(g_uCapturedBuildIndex, 2u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);

	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u,
		"waiting updates must not issue a second SINGLE load");
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);
}

ZENITH_TEST(ZM_WorldTraversal, ManagerPersistenceKeepsEntityIDAcrossSceneGeneration)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	const Zenith_Scene xSourceScene = xScenes.GetActiveScene();
	const int iSourceHandle = xSourceScene.GetHandle();
	Zenith_SceneData* pxSourceData = xScenes.GetSceneData(xSourceScene);
	ZENITH_ASSERT_NOT_NULL(pxSourceData);
	if (pxSourceData == nullptr) { return; }

	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSourceData);
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	if (!xManagerEntity.IsValid()) { return; }
	const Zenith_EntityID xOriginalID = xManagerEntity.GetEntityID();
	Zenith_SceneData* pxPersistentData =
		xScenes.GetSceneData(xScenes.GetPersistentScene());
	ZENITH_ASSERT_EQ(xScenes.GetSceneDataForEntity(xOriginalID), pxPersistentData);

	const Zenith_Scene xReplacement = xScenes.LoadScene(
		"TraversalReplacement", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xReplacement.IsValid());
	ZENITH_ASSERT_TRUE(xScenes.SetActiveScene(xReplacement));
	xScenes.UnloadSceneForced(xSourceScene);
	ZENITH_ASSERT_FALSE(xSourceScene.IsValid());
	ZENITH_ASSERT_NULL(xScenes.GetSceneData(xSourceScene));

	// Scene handles are reused LIFO. The stale source handle must remain invalid
	// even when the exact slot is immediately occupied by a newer generation.
	const Zenith_Scene xReusedSourceSlot = xScenes.LoadScene(
		"TraversalReusedSourceSlot", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xReusedSourceSlot.IsValid());
	ZENITH_ASSERT_EQ(xReusedSourceSlot.GetHandle(), iSourceHandle);
	ZENITH_ASSERT_NE(xReusedSourceSlot, xSourceScene);
	ZENITH_ASSERT_FALSE(xSourceScene.IsValid());
	ZENITH_ASSERT_NOT_NULL(xScenes.GetSceneData(xReusedSourceSlot));

	Zenith_Entity xResolved = xScenes.ResolveEntity(xOriginalID);
	Zenith_EntityID xSingletonID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_TRUE(xResolved.IsValid());
	ZENITH_ASSERT_EQ(xResolved.GetEntityID(), xOriginalID);
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID));
	ZENITH_ASSERT_EQ(xSingletonID, xOriginalID);
	ZENITH_ASSERT_EQ(xScenes.GetSceneDataForEntity(xOriginalID), pxPersistentData);
	ZENITH_ASSERT_TRUE(
		xResolved.GetComponent<ZM_GameStateManager>().IsAuthoritativeSingleton());
}

ZENITH_TEST(ZM_WorldTraversal, SpawnPointTagValidationBoundaries)
{
	char szMaxTag[ZM_SpawnPoint::uTAG_CAPACITY] = {};
	std::memset(szMaxTag, 'A', sizeof(szMaxTag) - 1u);
	char szTooLong[ZM_SpawnPoint::uTAG_CAPACITY + 1u] = {};
	std::memset(szTooLong, 'B', sizeof(szTooLong) - 1u);
	const char szControl[] = { 'A', '\n', '\0' };
	const char szDelete[] = { 'A', static_cast<char>(0x7f), '\0' };

	ZENITH_ASSERT_FALSE(ZM_SpawnPoint::IsTagValid(nullptr));
	ZENITH_ASSERT_FALSE(ZM_SpawnPoint::IsTagValid(""));
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid("TownCenter"));
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid(szMaxTag));
	ZENITH_ASSERT_FALSE(ZM_SpawnPoint::IsTagValid(szTooLong));
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid("Town Center"));
	ZENITH_ASSERT_FALSE(ZM_SpawnPoint::IsTagValid(szControl));
	ZENITH_ASSERT_FALSE(ZM_SpawnPoint::IsTagValid(szDelete));

	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }
	Zenith_Entity xSpawn = CreateSpawnPoint(
		pxSceneData, "BoundarySpawn", "TownCenter");
	ZM_SpawnPoint& xSpawnPoint = xSpawn.GetComponent<ZM_SpawnPoint>();
	ZENITH_ASSERT_FALSE(xSpawnPoint.SetTag(szTooLong));
	ZENITH_ASSERT_STREQ(xSpawnPoint.GetTag(), "TownCenter",
		"invalid updates must preserve the last valid tag");
	ZENITH_ASSERT_TRUE(xSpawnPoint.SetTag(szMaxTag));
	ZENITH_ASSERT_STREQ(xSpawnPoint.GetTag(), szMaxTag);
}

ZENITH_TEST(ZM_WorldTraversal, SpawnPointLookupRequiresUniqueSameSceneMatch)
{
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	const Zenith_Scene xPrimaryScene = xScenes.GetActiveScene();
	const Zenith_Scene xOtherScene = xScenes.LoadScene(
		"TraversalOtherScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xPrimaryScene.IsValid());
	ZENITH_ASSERT_TRUE(xOtherScene.IsValid());
	Zenith_SceneData* pxPrimaryData = xScenes.GetSceneData(xPrimaryScene);
	Zenith_SceneData* pxOtherData = xScenes.GetSceneData(xOtherScene);
	ZENITH_ASSERT_NOT_NULL(pxPrimaryData);
	ZENITH_ASSERT_NOT_NULL(pxOtherData);
	if (pxPrimaryData == nullptr || pxOtherData == nullptr) { return; }

	Zenith_Entity xPrimary = CreateSpawnPoint(
		pxPrimaryData, "PrimaryTownCenter", "TownCenter");
	CreateSpawnPoint(pxOtherData, "OtherTownCenter", "TownCenter");
	Zenith_EntityID xFoundID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xPrimaryScene, "TownCenter", xFoundID),
		ZM_SPAWN_POINT_LOOKUP_FOUND);
	ZENITH_ASSERT_EQ(xFoundID, xPrimary.GetEntityID(),
		"same tag in another scene must not affect local uniqueness");

	CreateSpawnPoint(pxPrimaryData, "DuplicateTownCenter", "TownCenter");
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xPrimaryScene, "TownCenter", xFoundID),
		ZM_SPAWN_POINT_LOOKUP_DUPLICATE);
	ZENITH_ASSERT_EQ(xFoundID, INVALID_ENTITY_ID);
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xPrimaryScene, "Missing", xFoundID),
		ZM_SPAWN_POINT_LOOKUP_MISSING);
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xPrimaryScene, "", xFoundID),
		ZM_SPAWN_POINT_LOOKUP_INVALID_TAG);
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		Zenith_Scene(), "TownCenter", xFoundID),
		ZM_SPAWN_POINT_LOOKUP_INVALID_SCENE);
}

ZENITH_TEST(ZM_WorldTraversal, SpawnPointSerializationVersionRoundTripAndLegacyFallback)
{
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }
	Zenith_Entity xSourceEntity = CreateSpawnPoint(
		pxSceneData, "SourceSpawn", "TownCenter");
	Zenith_Entity xTargetEntity = CreateSpawnPoint(
		pxSceneData, "TargetSpawn", "BeforeRead");
	ZM_SpawnPoint& xSource = xSourceEntity.GetComponent<ZM_SpawnPoint>();
	ZM_SpawnPoint& xTarget = xTargetEntity.GetComponent<ZM_SpawnPoint>();

	Zenith_DataStream xRoundTrip;
	xSource.WriteToDataStream(xRoundTrip);
	ZENITH_ASSERT_EQ(xRoundTrip.GetCursor(),
		sizeof(u_int) + ZM_SpawnPoint::uTAG_CAPACITY);
	xRoundTrip.SetCursor(0u);
	xTarget.ReadFromDataStream(xRoundTrip);
	ZENITH_ASSERT_STREQ(xTarget.GetTag(), "TownCenter");
	ZENITH_ASSERT_EQ(xRoundTrip.GetCursor(),
		sizeof(u_int) + ZM_SpawnPoint::uTAG_CAPACITY);

	Zenith_DataStream xLegacy;
	xLegacy << 0u;
	xLegacy.SetCursor(0u);
	xTarget.ReadFromDataStream(xLegacy);
	ZENITH_ASSERT_STREQ(xTarget.GetTag(), "",
		"unsupported legacy payloads must fall back to an empty tag");
	ZENITH_ASSERT_EQ(xLegacy.GetCursor(), sizeof(u_int));
}

ZENITH_TEST(ZM_WorldTraversal, WarpTriggerConfigurationAndVersionRoundTrip)
{
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }
	Zenith_Entity xSourceEntity =
		g_xEngine.Scenes().CreateEntity(pxSceneData, "SourceTrigger");
	Zenith_Entity xTargetEntity =
		g_xEngine.Scenes().CreateEntity(pxSceneData, "TargetTrigger");
	ZM_WarpTrigger& xSource = xSourceEntity.AddComponent<ZM_WarpTrigger>();
	ZM_WarpTrigger& xTarget = xTargetEntity.AddComponent<ZM_WarpTrigger>();

	ZENITH_ASSERT_FALSE(xSource.Configure(
		ZM_WarpTrigger::uINVALID_BUILD_INDEX, "TownCenter"));
	ZENITH_ASSERT_FALSE(xSource.Configure(2u, "towncenter"));
	ZENITH_ASSERT_EQ(xSource.GetTargetBuildIndex(),
		ZM_WarpTrigger::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xSource.GetSpawnTag(), "");
	ZENITH_ASSERT_TRUE(xSource.Configure(2u, "TownCenter"));
	ZENITH_ASSERT_EQ(xSource.GetTargetBuildIndex(), 2u);
	ZENITH_ASSERT_STREQ(xSource.GetSpawnTag(), "TownCenter");
	ZENITH_ASSERT_FALSE(xSource.IsLatched());

	Zenith_DataStream xRoundTrip;
	xSource.WriteToDataStream(xRoundTrip);
	ZENITH_ASSERT_EQ(xRoundTrip.GetCursor(),
		2u * sizeof(u_int) + ZM_WarpTrigger::uTAG_CAPACITY);
	xRoundTrip.SetCursor(0u);
	xTarget.ReadFromDataStream(xRoundTrip);
	ZENITH_ASSERT_EQ(xTarget.GetTargetBuildIndex(), 2u);
	ZENITH_ASSERT_STREQ(xTarget.GetSpawnTag(), "TownCenter");
	ZENITH_ASSERT_FALSE(xTarget.IsLatched());

	Zenith_DataStream xLegacy;
	xLegacy << 0u;
	xLegacy.SetCursor(0u);
	xTarget.ReadFromDataStream(xLegacy);
	ZENITH_ASSERT_EQ(xTarget.GetTargetBuildIndex(),
		ZM_WarpTrigger::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xTarget.GetSpawnTag(), "");
	ZENITH_ASSERT_EQ(xLegacy.GetCursor(), sizeof(u_int));
}

ZENITH_TEST(ZM_WorldTraversal, WarpTriggerFiltersSensorOtherBodyAndNonPlayer)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	const Zenith_Scene xActiveScene = xScenes.GetActiveScene();
	Zenith_SceneData* pxSceneData = xScenes.GetSceneData(xActiveScene);
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xTriggerEntity = CreateConfiguredTrigger(pxSceneData);
	Zenith_Entity xDynamicNonPlayer = CreateBoxBody(
		pxSceneData, "DynamicNonPlayer", RIGIDBODY_TYPE_DYNAMIC);
	Zenith_Entity xNamedPlayer =
		xScenes.CreateEntity(pxSceneData, "Player");
	ZENITH_ASSERT_TRUE(xTriggerEntity.IsValid());
	ZENITH_ASSERT_TRUE(xDynamicNonPlayer.IsValid());
	if (!xTriggerEntity.IsValid() || !xDynamicNonPlayer.IsValid()) { return; }

	Zenith_ColliderComponent& xOtherCollider =
		xDynamicNonPlayer.GetComponent<Zenith_ColliderComponent>();
	ZENITH_ASSERT_TRUE(xOtherCollider.HasValidBody());
	ZM_WarpTrigger& xTrigger =
		xTriggerEntity.GetComponent<ZM_WarpTrigger>();

	// Cross the real static trigger using a solid, gravity-free dynamic body.
	// Ending beyond the box proves the trigger's own collider is a sensor rather
	// than a wall; its real contact callback must still ignore the non-player.
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_PhysicsBodyID xOtherBodyID = xOtherCollider.GetBodyID();
	xPhysics.TeleportBody(xOtherBodyID, { -3.0f, 0.0f, 0.0f });
	xPhysics.SetGravityEnabled(xOtherBodyID, false);
	xPhysics.SetLinearVelocity(xOtherBodyID, { 8.0f, 0.0f, 0.0f });
	for (u_int uStep = 0u; uStep < 120u; ++uStep)
	{
		xPhysics.Update(1.0f / 60.0f);
	}
	ZENITH_ASSERT_GT(xPhysics.GetBodyPosition(xOtherBodyID).x, 1.0f,
		"the dynamic non-player must pass through the trigger sensor");
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xDynamicNonPlayer),
		"a dynamic non-player body must be ignored");
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xNamedPlayer),
		"the Player name alone must not pass the component filter");
	Zenith_Entity xInvalid;
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xInvalid));
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());

	Zenith_Entity xActivePlayer = CreatePlayer(pxSceneData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSceneData);
	const Zenith_Scene xForeignScene = xScenes.LoadScene(
		"TraversalForeignPlayerScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxForeignData = xScenes.GetSceneData(xForeignScene);
	ZENITH_ASSERT_TRUE(xActivePlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	ZENITH_ASSERT_NOT_NULL(pxForeignData);
	if (!xActivePlayer.IsValid()
		|| !xManagerEntity.IsValid()
		|| pxForeignData == nullptr)
	{
		return;
	}

	Zenith_Entity xForeignPlayer = CreatePlayer(pxForeignData);
	Zenith_EntityID xCandidateID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xCandidateID));
	ZENITH_ASSERT_EQ(xCandidateID, xActivePlayer.GetEntityID(),
		"a foreign additive-scene PlayerController is not authoritative");
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xForeignPlayer));
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());

	Zenith_Entity xMalformedExtra =
		xScenes.CreateEntity(pxSceneData, "MalformedExtraPlayer");
	xMalformedExtra.AddComponent<ZM_PlayerController>();
	const Zenith_EntityID xMalformedID = xMalformedExtra.GetEntityID();
	ZENITH_ASSERT_FALSE(
		ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xCandidateID),
		"one valid player plus a bodyless controller must fail closed");
	ZENITH_ASSERT_EQ(xCandidateID, INVALID_ENTITY_ID);
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xActivePlayer));
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());

	xMalformedExtra.DestroyImmediate();
	ZENITH_ASSERT_FALSE(xScenes.ResolveEntity(xMalformedID).IsValid());
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xCandidateID));
	ZENITH_ASSERT_EQ(xCandidateID, xActivePlayer.GetEntityID());
	LoadCallbackScope xCallbackScope;
	ZENITH_ASSERT_TRUE(xTrigger.TryHandleOverlap(xActivePlayer),
		"only the unique active-scene valid dynamic capsule is accepted");
	ZENITH_ASSERT_TRUE(xTrigger.IsLatched());
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u,
		"trigger acceptance queues but does not synchronously load");
}

ZENITH_TEST(ZM_WorldTraversal, WarpTriggerLatchRequestsExactlyOnceAndResetsForNextOverlap)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSceneData);
	Zenith_Entity xTriggerEntity = CreateConfiguredTrigger(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	ZENITH_ASSERT_TRUE(xTriggerEntity.IsValid());
	if (!xPlayer.IsValid()
		|| !xManagerEntity.IsValid()
		|| !xTriggerEntity.IsValid())
	{
		return;
	}

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZM_WarpTrigger& xTrigger =
		xTriggerEntity.GetComponent<ZM_WarpTrigger>();
	ZENITH_ASSERT_TRUE(xTrigger.TryHandleOverlap(xPlayer));
	ZENITH_ASSERT_TRUE(xTrigger.IsLatched());
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xPlayer),
		"a latched overlap must not queue twice");
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);

	// Return the manager to IDLE while leaving the trigger latched. This makes
	// the next rejection attributable to the latch itself, not manager state.
	ZM_GameStateManager::ResetRuntimeStateForTests();
	g_uCapturedLoadCount = 0u;
	g_uCapturedBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
	ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(&CaptureLoadRequest);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_TRUE(xTrigger.IsLatched());
	ZENITH_ASSERT_TRUE(
		xPlayer.GetComponent<ZM_PlayerController>().IsMovementEnabled());
	ZENITH_ASSERT_FALSE(xTrigger.TryHandleOverlap(xPlayer),
		"the latch must suppress a retry even while the manager is IDLE");
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);

	xTrigger.OnCollisionExit(INVALID_ENTITY_ID);
	ZENITH_ASSERT_TRUE(xTrigger.IsLatched(),
		"an unrelated exit must not clear the latch");
	xTrigger.OnCollisionExit(xPlayer.GetEntityID());
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());
	ZENITH_ASSERT_TRUE(xTrigger.TryHandleOverlap(xPlayer));
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u,
		"a completed exit permits exactly one request for the next overlap");
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);
}

ZENITH_TEST(ZM_WorldTraversal, PlacementUsesMarkerFeetPlusScaledCapsuleHalfExtentAndZeroesMotion)
{
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }
	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	if (!xPlayer.IsValid()) { return; }

	const Zenith_Maths::Vector3 xMarkerFeet(512.0f, 25.98577f, 480.0f);
	Zenith_Maths::Vector3 xPlayerScale(0.0f);
	xPlayer.GetComponent<Zenith_TransformComponent>().GetScale(xPlayerScale);
	const float fHalfExtent =
		ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayerScale);
	const Zenith_Maths::Vector3 xSpawnCenter =
		ZM_GameStateManager::CalculateSpawnCenter(xMarkerFeet, xPlayerScale);
	ZENITH_ASSERT_EQ_FLOAT(fHalfExtent, 0.9f, 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(xSpawnCenter,
		Zenith_Maths::Vector3(512.0f, 26.88577f, 480.0f), 0.00001f);
	ZENITH_ASSERT_EQ_FLOAT(xSpawnCenter.y - fHalfExtent,
		xMarkerFeet.y, 0.00001f);

	Zenith_ColliderComponent& xCollider =
		xPlayer.GetComponent<Zenith_ColliderComponent>();
	ZM_PlayerController& xController =
		xPlayer.GetComponent<ZM_PlayerController>();
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_PhysicsBodyID xBodyID = xCollider.GetBodyID();
	ZENITH_ASSERT_TRUE(xCollider.HasValidBody());
	xPhysics.SetLinearVelocity(xBodyID, { 3.0f, -2.0f, 4.0f });
	xPhysics.SetAngularVelocity(xBodyID, { 1.0f, 2.0f, 3.0f });
	xController.SetMovementEnabled(false);
	ZENITH_ASSERT_NEAR_VEC3(xPhysics.GetLinearVelocity(xBodyID),
		Zenith_Maths::Vector3(0.0f, -2.0f, 0.0f), 0.00001f);

	// This is the public placement/reset seam used by the manager. The P1 owns
	// the private state-machine end-to-end proof against the authored scene.
	xPhysics.TeleportBody(xBodyID, xSpawnCenter);
	xPhysics.SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
	xController.ResetRuntimeState();
	ZENITH_ASSERT_NEAR_VEC3(xPhysics.GetBodyPosition(xBodyID),
		xSpawnCenter, 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(xPhysics.GetLinearVelocity(xBodyID),
		Zenith_Maths::Vector3(0.0f), 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(xPhysics.GetAngularVelocity(xBodyID),
		Zenith_Maths::Vector3(0.0f), 0.00001f);
	ZENITH_ASSERT_TRUE(xController.IsMovementEnabled());
	ZENITH_ASSERT_EQ_FLOAT(xController.GetRequestedSpeed(), 0.0f, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(xController.GetMoveDirection(),
		Zenith_Maths::Vector3(0.0f), 0.0f);
}

ZENITH_TEST(ZM_WorldTraversal, WarpResolutionFreezesThenResetsOnMissingDuplicateAndGenerationChange)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	Zenith_SceneData* pxSourceData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSourceData);
	if (pxSourceData == nullptr) { return; }

	Zenith_Entity xSourcePlayer = CreatePlayer(pxSourceData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSourceData);
	ZENITH_ASSERT_TRUE(xSourcePlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	if (!xSourcePlayer.IsValid() || !xManagerEntity.IsValid()) { return; }
	const Zenith_EntityID xOldPlayerID = xSourcePlayer.GetEntityID();
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xSourceManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	ZENITH_ASSERT_FALSE(
		xSourcePlayer.GetComponent<ZM_PlayerController>().IsMovementEnabled());
	xSourceManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(xSourceManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);

	xScenes.RegisterSceneBuildIndex(2, "ZM_WorldTraversalTarget");
	const Zenith_Scene xTargetScene = xScenes.LoadSceneByIndex(
		2, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xTargetScene.IsValid());
	ZENITH_ASSERT_TRUE(xScenes.SetActiveScene(xTargetScene));
	Zenith_SceneData* pxTargetData = xScenes.GetSceneData(xTargetScene);
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }

	xSourcePlayer.DestroyImmediate();
	ZENITH_ASSERT_FALSE(xScenes.ResolveEntity(xOldPlayerID).IsValid());
	Zenith_Entity xReplacementPlayer = CreatePlayer(pxTargetData);
	ZENITH_ASSERT_TRUE(xReplacementPlayer.IsValid());
	if (!xReplacementPlayer.IsValid()) { return; }
	const Zenith_EntityID xReplacementID = xReplacementPlayer.GetEntityID();
	ZENITH_ASSERT_EQ(xReplacementID.m_uIndex, xOldPlayerID.m_uIndex,
		"immediate replacement must reuse the just-freed LIFO entity slot");
	ZENITH_ASSERT_NE(xReplacementID.m_uGeneration, xOldPlayerID.m_uGeneration,
		"slot reuse must advance generation and reject the stale Player ID");
	ZENITH_ASSERT_NE(xReplacementID, xOldPlayerID);
	ZENITH_ASSERT_FALSE(xScenes.ResolveEntity(xOldPlayerID).IsValid());
	ZM_PlayerController& xReplacementController =
		xReplacementPlayer.GetComponent<ZM_PlayerController>();
	xReplacementController.OnStart();
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::ShouldFreezePlayerOnStart());
	ZENITH_ASSERT_FALSE(xReplacementController.IsMovementEnabled(),
		"a new scene generation must freeze before its first movement update");

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), xReplacementID);
	ZENITH_ASSERT_FALSE(xReplacementController.IsMovementEnabled());
	Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xTargetScene, "TownCenter", xSpawnID),
		ZM_SPAWN_POINT_LOOKUP_MISSING);

	CreateSpawnPoint(pxTargetData, "DuplicateSpawnA", "TownCenter");
	CreateSpawnPoint(pxTargetData, "DuplicateSpawnB", "TownCenter");
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xTargetScene, "TownCenter", xSpawnID),
		ZM_SPAWN_POINT_LOOKUP_DUPLICATE);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), xReplacementID);
	ZENITH_ASSERT_FALSE(xReplacementController.IsMovementEnabled());

	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID);
	ZENITH_ASSERT_TRUE(xReplacementController.IsMovementEnabled());
	ZENITH_ASSERT_FALSE(ZM_GameStateManager::ShouldFreezePlayerOnStart());
}
