#include "Zenith.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/Text/Flux_TextQueue.h"
#include "Physics/Zenith_Physics.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIRect.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE body contract
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"

#include <cstring>
#include <limits>

namespace
{
	u_int g_uCapturedLoadCount = 0u;
	u_int g_uCapturedBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
	float g_fCapturedFadeAlpha = -1.0f;
	bool g_bCapturedFadeVisible = false;
	bool g_bCaptureRenderedFadeQuad = false;
	bool g_bCapturedRenderedFadeQuad = false;
	float g_fCapturedRenderedFadeQuadAlpha = -1.0f;
	ZM_WARP_TRANSITION_STATE g_eCapturedTransitionState =
		ZM_WARP_TRANSITION_IDLE;

	void CaptureRenderedFadeQuad(Zenith_Entity& xManagerEntity)
	{
		if (!xManagerEntity.IsValid())
		{
			return;
		}
		Zenith_UIComponent* pxUI =
			xManagerEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			return;
		}

		Flux_QuadsImpl& xQuads = g_xEngine.Quads();
		Flux_TextImpl& xText = g_xEngine.Text();
		const bool bQuadsEnabled =
			Zenith_GraphicsOptions::Get().m_bQuadsEnabled;
		const uint32_t uQuadRenderIndex = xQuads.m_uQuadRenderIndex;
		const bool bCapacityWarning = xQuads.m_bCapacityWarningIssued;
		Flux_QuadsImpl::Quad xFirstQuad;
		int iFirstSortOrder = 0;
		if (uQuadRenderIndex > 0u)
		{
			xFirstQuad = xQuads.m_axQuadsToRender[0];
			iFirstSortOrder = xQuads.m_aiQuadSortOrders[0];
		}
		const bool bClipActive = xText.m_bOverlayClipActive;
		const Zenith_Maths::Vector4 xClipRect = xText.m_xOverlayClipRect;
		const int iClipSortOrder = xText.m_iOverlayClipSortOrder;

		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = true;
		xQuads.m_uQuadRenderIndex = 0u;
		xQuads.m_bCapacityWarningIssued = false;
		pxUI->Render();
		if (xQuads.m_uQuadRenderIndex == 1u
			&& xQuads.m_aiQuadSortOrders[0] == 10000)
		{
			g_bCapturedRenderedFadeQuad = true;
			// The quad's colour lane is unorm8x4 storage; GetColour decodes it.
			// A full-opacity fade is byte 255, which decodes to exactly 1.0f, so
			// the tolerance-0 assertion downstream stays exact.
			g_fCapturedRenderedFadeQuadAlpha =
				xQuads.m_axQuadsToRender[0].GetColour().w;
		}

		if (uQuadRenderIndex > 0u)
		{
			xQuads.m_axQuadsToRender[0] = xFirstQuad;
			xQuads.m_aiQuadSortOrders[0] = iFirstSortOrder;
		}
		xQuads.m_uQuadRenderIndex = uQuadRenderIndex;
		xQuads.m_bCapacityWarningIssued = bCapacityWarning;
		xText.m_bOverlayClipActive = bClipActive;
		xText.m_xOverlayClipRect = xClipRect;
		xText.m_iOverlayClipSortOrder = iClipSortOrder;
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = bQuadsEnabled;
	}

	void CaptureLoadRequest(u_int uBuildIndex)
	{
		Zenith_EntityID xManagerID = INVALID_ENTITY_ID;
		if (!ZM_GameStateManager::TryGetUniqueSingletonEntityID(xManagerID))
		{
			++g_uCapturedLoadCount;
			g_uCapturedBuildIndex = uBuildIndex;
			return;
		}
		Zenith_Entity xManagerEntity =
			g_xEngine.Scenes().ResolveEntity(xManagerID);
		ZM_GameStateManager* pxManager = xManagerEntity.IsValid()
			? xManagerEntity.TryGetComponent<ZM_GameStateManager>()
			: nullptr;
		if (pxManager != nullptr)
		{
			g_fCapturedFadeAlpha = pxManager->GetFadeAlpha();
			g_bCapturedFadeVisible = pxManager->IsFadeVisible();
			g_eCapturedTransitionState = pxManager->GetTransitionState();
		}
		if (g_bCaptureRenderedFadeQuad)
		{
			CaptureRenderedFadeQuad(xManagerEntity);
		}

		++g_uCapturedLoadCount;
		g_uCapturedBuildIndex = uBuildIndex;
	}

	struct LoadCallbackScope
	{
		LoadCallbackScope()
		{
			g_uCapturedLoadCount = 0u;
			g_uCapturedBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
			g_fCapturedFadeAlpha = -1.0f;
			g_bCapturedFadeVisible = false;
			g_bCaptureRenderedFadeQuad = false;
			g_bCapturedRenderedFadeQuad = false;
			g_fCapturedRenderedFadeQuadAlpha = -1.0f;
			g_eCapturedTransitionState = ZM_WARP_TRANSITION_IDLE;
			ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(
				&CaptureLoadRequest);
		}

		~LoadCallbackScope()
		{
			ZM_GameStateManager::SetLoadSceneRequestCallbackForTests(nullptr);
			g_bCaptureRenderedFadeQuad = false;
		}
	};

	Zenith_SceneData* GetActiveSceneData()
	{
		return g_xEngine.Scenes().GetActiveSceneData();
	}

	void ConfigureFadeOverlay(Zenith_UI::Zenith_UIOverlay* pxFade)
	{
		if (pxFade == nullptr)
		{
			return;
		}
		pxFade->SetContentSize(0.0f, 0.0f);
		pxFade->SetAnchorAndPivot(Zenith_UI::AnchorPreset::StretchAll);
		pxFade->SetSortOrder(10000);
		pxFade->SetDimColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		pxFade->SetFadeDuration(0.0f);
		pxFade->SetGroupAlpha(0.0f);
		pxFade->SetVisible(false);
	}

	Zenith_UI::Zenith_UIOverlay* FindFadeOverlay(Zenith_Entity& xManagerEntity)
	{
		Zenith_UIComponent* pxUI =
			xManagerEntity.TryGetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
			? pxUI->FindElement(ZM_GameStateManager::szFADE_ELEMENT_NAME)
			: nullptr;
		if (pxElement == nullptr
			|| pxElement->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return nullptr;
		}
		return static_cast<Zenith_UI::Zenith_UIOverlay*>(pxElement);
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
		Zenith_UIComponent& xUI = xManager.AddComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIOverlay* pxFade = xUI.CreateOverlay(
			ZM_GameStateManager::szFADE_ELEMENT_NAME);
		ConfigureFadeOverlay(pxFade);
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

	Zenith_Entity CreateFollowCamera(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		bool bStart)
	{
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}

		Zenith_Entity xCamera =
			g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		xCamera.AddComponent<Zenith_CameraComponent>();
		ZM_FollowCamera& xFollow = xCamera.AddComponent<ZM_FollowCamera>();
		if (bStart)
		{
			xFollow.OnStart();
		}
		return xCamera;
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

	// ---- S8 item 2 barrier-timeout fixture ------------------------------------
	// All three warp barriers are only reachable with the destination scene ACTIVE
	// under the target build index, so every timeout test needs this same handful of
	// SceneSystem calls. Sharing them keeps the new tests about the TIMEOUT rather
	// than about scene plumbing. Build index 2 is Dawnmere, whose compiled
	// ZM_WorldSpec row offers "TownCenter" -- which is what makes RequestWarp accept
	// the destination while the scene itself stays deliberately unfurnished.
	Zenith_SceneData* ActivateWarpTargetScene(const char* szSceneName)
	{
		Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
		xScenes.RegisterSceneBuildIndex(2, szSceneName);
		const Zenith_Scene xTargetScene = xScenes.LoadSceneByIndex(
			2, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		if (!xTargetScene.IsValid() || !xScenes.SetActiveScene(xTargetScene))
		{
			return nullptr;
		}
		return xScenes.GetSceneData(xTargetScene);
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
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u);
	ZENITH_ASSERT_EQ(g_uCapturedBuildIndex, 2u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
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
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
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
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);

	xTrigger.OnCollisionExit(INVALID_ENTITY_ID);
	ZENITH_ASSERT_TRUE(xTrigger.IsLatched(),
		"an unrelated exit must not clear the latch");
	xTrigger.OnCollisionExit(xPlayer.GetEntityID());
	ZENITH_ASSERT_FALSE(xTrigger.IsLatched());
	ZENITH_ASSERT_TRUE(xTrigger.TryHandleOverlap(xPlayer));
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u,
		"a completed exit permits exactly one request for the next overlap");
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);
}

// ★ RENAMED AT ZM-D-182 (was ...PlusScaledCapsuleHalfExtent...). The half extent is no
// longer SCALED off the player's transform -- it is the compiled body contract -- and the
// clause below now proves exactly that rather than merely restating it.
ZENITH_TEST(ZM_WorldTraversal, PlacementUsesMarkerFeetPlusContractCapsuleHalfExtentAndZeroesMotion)
{
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }
	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	if (!xPlayer.IsValid()) { return; }

	const Zenith_Maths::Vector3 xMarkerFeet(512.0f, 25.99055f, 480.0f);
	const float fHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
	const Zenith_Maths::Vector3 xSpawnCenter =
		ZM_GameStateManager::CalculateSpawnCenter(xMarkerFeet);
	ZENITH_ASSERT_EQ_FLOAT(fHalfExtent, 0.9f, 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(xSpawnCenter,
		Zenith_Maths::Vector3(512.0f, 26.89055f, 480.0f), 0.00001f);
	ZENITH_ASSERT_EQ_FLOAT(xSpawnCenter.y - fHalfExtent,
		xMarkerFeet.y, 0.00001f);

	// ★ THE CONVERSION IS SCALE-INDEPENDENT, AND THIS IS WHERE THAT IS PROVED.
	// It used to be derived from the player's transform scale, so this clause was a
	// real cross-check; the migration to the compiled contract would have left it a
	// tautology (a constant compared against itself) if it were only re-spelled.
	// Re-scaling the player must therefore move the answer by exactly nothing --
	// which is the property that lets a human be authored at a uniform MODEL scale.
	Zenith_TransformComponent& xPlayerTransform =
		xPlayer.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xAuthoredScale(0.0f);
	xPlayerTransform.GetScale(xAuthoredScale);
	xPlayerTransform.SetScale(Zenith_Maths::Vector3(4.0f, 0.25f, 4.0f));
	ZENITH_ASSERT_NEAR_VEC3(ZM_GameStateManager::CalculateSpawnCenter(xMarkerFeet),
		xSpawnCenter, 0.0f);
	xPlayerTransform.SetScale(xAuthoredScale);

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
	xSourceManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
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
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::IsWarpInProgress());
	ZENITH_ASSERT_FALSE(xReplacementController.IsMovementEnabled(),
		"a new scene generation must freeze before its first movement update");

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), xReplacementID);
	ZENITH_ASSERT_FALSE(xReplacementController.IsMovementEnabled());
	Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xTargetScene, "TownCenter", xSpawnID),
		ZM_SPAWN_POINT_LOOKUP_MISSING);

	CreateSpawnPoint(pxTargetData, "DuplicateSpawnA", "TownCenter");
	CreateSpawnPoint(pxTargetData, "DuplicateSpawnB", "TownCenter");
	xManager.OnUpdate(1.0f / 60.0f);
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
	ZENITH_ASSERT_FALSE(ZM_GameStateManager::IsWarpInProgress());
}

ZENITH_TEST(ZM_WorldTraversal, FadeAdvanceClampsInvalidDtAndRuntimeReset)
{
	// The persistent fade canvas is visited before the active-scene canvas in
	// production. Exercise that inverted submission order through the real quad
	// queue and its upload preparation: lower global sort orders must still be
	// uploaded first, ties must remain stable, and the fade must land last.
	const bool bQuadsEnabled =
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled;
	const Flux_QuadsImpl::Quad xFadeQuad(
		{ 10000u, 0u, 1u, 1u }, { 0.0f, 0.0f, 0.0f, 1.0f }, 0u,
		{ 1.0f, 0.0f });
	const Flux_QuadsImpl::Quad xHudQuadA(
		{ 25u, 0u, 1u, 1u }, { 1.0f, 0.0f, 0.0f, 1.0f }, 0u,
		{ 1.0f, 0.0f });
	const Flux_QuadsImpl::Quad xHudQuadB(
		{ 26u, 0u, 1u, 1u }, { 0.0f, 1.0f, 0.0f, 1.0f }, 0u,
		{ 1.0f, 0.0f });
	const Flux_QuadsImpl::Quad xUnderlayQuad(
		{ 24u, 0u, 1u, 1u }, { 0.0f, 0.0f, 1.0f, 1.0f }, 0u,
		{ 1.0f, 0.0f });
	{
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = true;
		Flux_QuadsImpl xQuadQueue;
		xQuadQueue.UploadQuad(xFadeQuad, 10000);
		xQuadQueue.UploadQuad(xHudQuadA, 25);
		xQuadQueue.UploadQuad(xHudQuadB, 25);
		xQuadQueue.UploadQuad(xUnderlayQuad, -5);
		xQuadQueue.SortQueuedQuadsForUpload();
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = bQuadsEnabled;
		ZENITH_ASSERT_EQ(xQuadQueue.m_uQuadRenderIndex, 4u);
		ZENITH_ASSERT_EQ(xQuadQueue.m_aiQuadSortOrders[0], -5);
		ZENITH_ASSERT_EQ(xQuadQueue.m_aiQuadSortOrders[1], 25);
		ZENITH_ASSERT_EQ(xQuadQueue.m_aiQuadSortOrders[2], 25);
		ZENITH_ASSERT_EQ(xQuadQueue.m_aiQuadSortOrders[3], 10000);
		// The rect lane is uint16x4 storage; GetPositionSize decodes it back to the
		// authored u32 quad (every value here is far inside the 65535 ceiling, so
		// the round-trip is exact and these remain identity comparisons).
		ZENITH_ASSERT_EQ(
			xQuadQueue.m_axQuadsToRender[0].GetPositionSize().x, 24u);
		ZENITH_ASSERT_EQ(
			xQuadQueue.m_axQuadsToRender[1].GetPositionSize().x, 25u);
		ZENITH_ASSERT_EQ(
			xQuadQueue.m_axQuadsToRender[2].GetPositionSize().x, 26u);
		ZENITH_ASSERT_EQ(
			xQuadQueue.m_axQuadsToRender[3].GetPositionSize().x, 10000u);
	}

	// The queue is fixed-capacity. Fill it through the public enqueue path and
	// verify the newest submission is dropped without touching the last slot.
	{
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = true;
		Flux_QuadsImpl xCapacityQueue;
		Flux_QuadsImpl::Quad xCapacityQuad = xHudQuadA;
		for (uint32_t u = 0u; u < FLUX_MAX_QUADS_PER_FRAME; ++u)
		{
			xCapacityQuad.SetPositionSize({ u, 0u, 1u, 1u });
			xCapacityQueue.UploadQuad(xCapacityQuad);
		}
		// 999999 exceeds the uint16x4 lane ceiling and saturates to 65535 — still
		// distinct from every queued index, which is all this marker has to be.
		xCapacityQuad.SetPositionSize({ 999999u, 0u, 1u, 1u });
		xCapacityQueue.UploadQuad(xCapacityQuad, -100);
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = bQuadsEnabled;
		ZENITH_ASSERT_EQ(
			xCapacityQueue.m_uQuadRenderIndex,
			static_cast<uint32_t>(FLUX_MAX_QUADS_PER_FRAME));
		ZENITH_ASSERT_TRUE(xCapacityQueue.m_bCapacityWarningIssued);
		ZENITH_ASSERT_EQ(
			xCapacityQueue.m_axQuadsToRender[
				FLUX_MAX_QUADS_PER_FRAME - 1u].GetPositionSize().x,
			static_cast<uint32_t>(FLUX_MAX_QUADS_PER_FRAME - 1u));
	}

	// Render actual roots through two canvases in scene-slot order. This guards
	// the UICanvas -> Flux_Quads sort-key forwarding, not only the queue sorter.
	{
		Flux_QuadsImpl& xEngineQueue = g_xEngine.Quads();
		const uint32_t uStart = xEngineQueue.m_uQuadRenderIndex;
		const bool bCapacityWarning = xEngineQueue.m_bCapacityWarningIssued;
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = true;
		Zenith_UI::Zenith_UICanvas xFadeCanvas;
		Zenith_UI::Zenith_UIRect* pxFadeRoot =
			new Zenith_UI::Zenith_UIRect("GlobalFadeRoot");
		pxFadeRoot->SetSize(32.0f, 32.0f);
		pxFadeRoot->SetSortOrder(10000);
		xFadeCanvas.AddElement(pxFadeRoot);
		Zenith_UI::Zenith_UICanvas xHudCanvas;
		Zenith_UI::Zenith_UIRect* pxHudRoot =
			new Zenith_UI::Zenith_UIRect("ActiveHudRoot");
		pxHudRoot->SetSize(32.0f, 32.0f);
		pxHudRoot->SetSortOrder(25);
		xHudCanvas.AddElement(pxHudRoot);
		xFadeCanvas.Render();
		xHudCanvas.Render();
		const uint32_t uEnd = xEngineQueue.m_uQuadRenderIndex;
		int iForwardedFadeOrder = std::numeric_limits<int>::min();
		int iForwardedHudOrder = std::numeric_limits<int>::min();
		if (uEnd == uStart + 2u)
		{
			iForwardedFadeOrder = xEngineQueue.m_aiQuadSortOrders[uStart];
			iForwardedHudOrder = xEngineQueue.m_aiQuadSortOrders[uStart + 1u];
		}
		xEngineQueue.m_uQuadRenderIndex = uStart;
		xEngineQueue.m_bCapacityWarningIssued = bCapacityWarning;
		Zenith_GraphicsOptions::Get().m_bQuadsEnabled = bQuadsEnabled;
		ZENITH_ASSERT_EQ(uEnd, uStart + 2u);
		ZENITH_ASSERT_EQ(iForwardedFadeOrder, 10000);
		ZENITH_ASSERT_EQ(iForwardedHudOrder, 25);
	}

	// A lower-sort active-scene modal rendered after the fade must not replace
	// the fade's full-screen text clip. Equal keys remain last-writer-wins, and
	// clearing the frame state permits a lower key on the next frame.
	{
		Flux_TextImpl xText;
		xText.SetOverlayClipRect(
			{ 0.0f, 0.0f, 1280.0f, 720.0f }, 10000);
		xText.SetOverlayClipRect(
			{ 200.0f, 100.0f, 800.0f, 600.0f }, 25);
		ZENITH_ASSERT_EQ(xText.m_iOverlayClipSortOrder, 10000);
		ZENITH_ASSERT_EQ_FLOAT(xText.m_xOverlayClipRect.x, 0.0f, 0.0f);
		ZENITH_ASSERT_EQ_FLOAT(xText.m_xOverlayClipRect.z, 1280.0f, 0.0f);
		xText.SetOverlayClipRect(
			{ 10.0f, 20.0f, 1200.0f, 700.0f }, 10000);
		ZENITH_ASSERT_EQ_FLOAT(xText.m_xOverlayClipRect.x, 10.0f, 0.0f);
		xText.ClearOverlayClipRect();
		ZENITH_ASSERT_FALSE(xText.m_bOverlayClipActive);
		xText.SetOverlayClipRect(
			{ 200.0f, 100.0f, 800.0f, 600.0f }, 25);
		ZENITH_ASSERT_EQ(xText.m_iOverlayClipSortOrder, 25);
		xText.ClearOverlayClipRect();
	}

	// Exercise the production Text render-graph callback directly at its
	// disabled-frame boundary. All frame-local state must be consumed, and a
	// re-enabled empty frame must not replay the stale submission. The callback
	// seam deliberately avoids constructing a graph in this headless fixture,
	// where FluxGraphics has no live final render target.
	{
		Flux_TextImpl& xEngineText = g_xEngine.Text();
		Zenith_Vector<Flux::Flux_TextEntry>& xPending =
			Flux::Flux_TextQueue::GetPending();
		const Zenith_Vector<Flux::Flux_TextEntry> xPendingBackup(xPending);
		const bool bTextEnabled =
			Zenith_GraphicsOptions::Get().m_bTextEnabled;
		const bool bClipActive = xEngineText.m_bOverlayClipActive;
		const Zenith_Maths::Vector4 xClipRect =
			xEngineText.m_xOverlayClipRect;
		const int iClipSortOrder = xEngineText.m_iOverlayClipSortOrder;
		const uint32_t uBgCharCount = xEngineText.m_uBgCharCount;
		const uint32_t uFgCharCount = xEngineText.m_uFgCharCount;
		const uint32_t uTotalCharCount = xEngineText.m_uTotalCharCount;

		xPending.Clear();
		Flux::Flux_TextEntry xStaleEntry{};
		xStaleEntry.m_strText = "stale callback text";
		xStaleEntry.m_fSize = 16.0f;
		xStaleEntry.m_xColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		Flux::Flux_TextQueue::Submit(xStaleEntry);
		xEngineText.m_uBgCharCount = 3u;
		xEngineText.m_uFgCharCount = 5u;
		xEngineText.m_uTotalCharCount = 8u;
		xEngineText.ClearOverlayClipRect();
		xEngineText.SetOverlayClipRect(
			{ 0.0f, 0.0f, 1280.0f, 720.0f }, 10000);
		Zenith_GraphicsOptions::Get().m_bTextEnabled = false;

		Flux_TextImpl::ExecuteRenderGraphPass(nullptr, nullptr);

		const bool bCallbackDiscardedFrame =
			xPending.GetSize() == 0u
			&& xEngineText.m_uBgCharCount == 0u
			&& xEngineText.m_uFgCharCount == 0u
			&& xEngineText.m_uTotalCharCount == 0u
			&& !xEngineText.m_bOverlayClipActive;
		xEngineText.SetOverlayClipRect(
			{ 200.0f, 100.0f, 800.0f, 600.0f }, 25);
		const bool bLowerClipAccepted =
			xEngineText.m_bOverlayClipActive
			&& xEngineText.m_iOverlayClipSortOrder == 25
			&& xEngineText.m_xOverlayClipRect.x == 200.0f
			&& xEngineText.m_xOverlayClipRect.z == 800.0f;
		Zenith_GraphicsOptions::Get().m_bTextEnabled = true;
		Flux_TextImpl::ExecuteRenderGraphPass(nullptr, nullptr);
		const bool bCallbackReenabledWithoutStaleText =
			xPending.GetSize() == 0u
			&& xEngineText.m_uBgCharCount == 0u
			&& xEngineText.m_uFgCharCount == 0u
			&& xEngineText.m_uTotalCharCount == 0u;

		// The legacy direct Render seam must consume the same complete boundary.
		xStaleEntry.m_strText = "stale legacy text";
		Flux::Flux_TextQueue::Submit(xStaleEntry);
		xEngineText.m_uBgCharCount = 7u;
		xEngineText.m_uFgCharCount = 11u;
		xEngineText.m_uTotalCharCount = 18u;
		xEngineText.SetOverlayClipRect(
			{ 0.0f, 0.0f, 1280.0f, 720.0f }, 10000);
		Zenith_GraphicsOptions::Get().m_bTextEnabled = false;
		xEngineText.Render(nullptr);
		const bool bLegacyDiscardedFrame =
			xPending.GetSize() == 0u
			&& xEngineText.m_uBgCharCount == 0u
			&& xEngineText.m_uFgCharCount == 0u
			&& xEngineText.m_uTotalCharCount == 0u
			&& !xEngineText.m_bOverlayClipActive;
		Zenith_GraphicsOptions::Get().m_bTextEnabled = true;
		xEngineText.Render(nullptr);
		const bool bLegacyReenabledWithoutStaleText =
			xPending.GetSize() == 0u
			&& xEngineText.m_uTotalCharCount == 0u;

		xPending = xPendingBackup;
		xEngineText.m_bOverlayClipActive = bClipActive;
		xEngineText.m_xOverlayClipRect = xClipRect;
		xEngineText.m_iOverlayClipSortOrder = iClipSortOrder;
		xEngineText.m_uBgCharCount = uBgCharCount;
		xEngineText.m_uFgCharCount = uFgCharCount;
		xEngineText.m_uTotalCharCount = uTotalCharCount;
		Zenith_GraphicsOptions::Get().m_bTextEnabled = bTextEnabled;

		ZENITH_ASSERT_TRUE(bCallbackDiscardedFrame);
		ZENITH_ASSERT_TRUE(bLowerClipAccepted);
		ZENITH_ASSERT_TRUE(bCallbackReenabledWithoutStaleText);
		ZENITH_ASSERT_TRUE(bLegacyDiscardedFrame);
		ZENITH_ASSERT_TRUE(bLegacyReenabledWithoutStaleText);
	}

	// Model material overrides are held by Flux_ModelInstance's owning handles.
	// Prove that ownership directly, then deserialize the v8 shape produced by a
	// procedural ModelComponent (empty model path + one material). With no model
	// instance to receive it, the temporary material must remain unloadable.
	{
		Zenith_AssetRegistry::UnloadUnused();
		const uint32_t uAssetCountBeforeOwnership =
			Zenith_AssetRegistry::GetLoadedAssetCount();
		{
			MaterialHandle xMaterial =
				Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMaterial = xMaterial.GetDirect();
			ZENITH_ASSERT_NOT_NULL(pxMaterial);
			if (pxMaterial != nullptr)
			{
				Flux_ModelInstance xModelInstance;
				ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), 1u);
				xModelInstance.SetMaterial(0u, pxMaterial);
				ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), 2u);
				xMaterial.Clear();
				ZENITH_ASSERT_EQ(xModelInstance.GetMaterial(0u), pxMaterial);
				ZENITH_ASSERT_EQ(pxMaterial->GetRefCount(), 1u);
			}
		}
		Zenith_AssetRegistry::UnloadUnused();
		ZENITH_ASSERT_EQ(Zenith_AssetRegistry::GetLoadedAssetCount(),
			uAssetCountBeforeOwnership);

		Zenith_AssetRegistry::UnloadUnused();
		const uint32_t uAssetCountBeforeRoundTrip =
			Zenith_AssetRegistry::GetLoadedAssetCount();
		{
			MaterialHandle xSerializedMaterial =
				Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxSerializedMaterial =
				xSerializedMaterial.GetDirect();
			ZENITH_ASSERT_NOT_NULL(pxSerializedMaterial);
			if (pxSerializedMaterial != nullptr)
			{
				pxSerializedMaterial->SetName("ProceduralRoundTrip");
				Zenith_DataStream xStream;
				xStream << 8u;
				ModelHandle xEmptyModel;
				xEmptyModel.WriteToDataStream(xStream);
				xStream << 1u;
				pxSerializedMaterial->WriteToDataStream(xStream);
				const uint64_t ulSerializedSize = xStream.GetCursor();

				const uint32_t uRetainedAssetCount =
					Zenith_AssetRegistry::GetLoadedAssetCount();
				ZENITH_ASSERT_EQ(uRetainedAssetCount,
					uAssetCountBeforeRoundTrip + 1u);
				Zenith_Entity xInvalidParent;
				Zenith_ModelComponent xReloadedModel(xInvalidParent);
				xStream.SetCursor(0u);
				xReloadedModel.ReadFromDataStream(xStream);
				ZENITH_ASSERT_EQ(xStream.GetCursor(), ulSerializedSize);
				ZENITH_ASSERT_FALSE(xReloadedModel.HasModel());
				ZENITH_ASSERT_EQ(
					Zenith_AssetRegistry::GetLoadedAssetCount(),
					uRetainedAssetCount + 1u);

				Zenith_AssetRegistry::UnloadUnused();
				ZENITH_ASSERT_EQ(
					Zenith_AssetRegistry::GetLoadedAssetCount(),
					uRetainedAssetCount);
			}
		}
		Zenith_AssetRegistry::UnloadUnused();
		ZENITH_ASSERT_EQ(Zenith_AssetRegistry::GetLoadedAssetCount(),
			uAssetCountBeforeRoundTrip);
	}

	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInfinity = std::numeric_limits<float>::infinity();
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(fNaN, 1.0f, 0.0f),
		0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(-4.0f, 1.0f, -1.0f),
		0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(4.0f, 0.0f, 0.0f),
		1.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(0.4f, fNaN, 0.1f),
		0.4f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(0.4f, 1.0f, fInfinity),
		0.4f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(
			0.0f, 1.0f,
			ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.25f),
		0.25f, 0.00001f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(
			0.9f, 1.0f, ZM_GameStateManager::fFADE_DURATION_SECONDS),
		1.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		ZM_GameStateManager::AdvanceFadeAlpha(
			0.1f, 0.0f, ZM_GameStateManager::fFADE_DURATION_SECONDS),
		0.0f, 0.0f);

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

	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	Zenith_UI::Zenith_UIOverlay* pxFade = FindFadeOverlay(xManagerEntity);
	ZENITH_ASSERT_NOT_NULL(pxFade);
	if (pxFade == nullptr) { return; }
	ZENITH_ASSERT_TRUE(pxFade->IsStretchAll());
	ZENITH_ASSERT_EQ(pxFade->GetSortOrder(), 10000);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetDimColor().w, 1.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetFadeDuration(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetContentSize().x, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetContentSize().y, 0.0f, 0.0f);
	ZENITH_ASSERT_FALSE(pxFade->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetGroupAlpha(), 0.0f, 0.0f);
	pxFade->SetGroupAlpha(1.0f);
	pxFade->Show();
	ZENITH_ASSERT_TRUE(pxFade->IsShowing());
	ZENITH_ASSERT_TRUE(pxFade->IsVisible());
	pxFade->Hide();
	ZENITH_ASSERT_FALSE(pxFade->IsShowing(),
		"zero-duration Hide must complete without a later UI update");
	ZENITH_ASSERT_FALSE(pxFade->IsVisible(),
		"zero-duration Hide must synchronously remove the overlay");
	pxFade->SetGroupAlpha(0.0f);
	Zenith_DataStream xFadeReload;
	pxFade->WriteToDataStream(xFadeReload);
	xFadeReload.SetCursor(0u);
	pxFade->ReadFromDataStream(xFadeReload);
	ZENITH_ASSERT_EQ(pxFade->GetSortOrder(), 10000,
		"the serialized root sort key drives cross-canvas quad ordering");
	ZENITH_ASSERT_FALSE(pxFade->IsStretchAll(),
		"UIOverlay reload centers itself before the game owner reasserts fullscreen");

	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_TRUE(pxFade->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetGroupAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetDimColor().w, 1.0f, 0.0f);
	ZENITH_ASSERT_TRUE(pxFade->IsStretchAll());
	ZENITH_ASSERT_EQ(pxFade->GetSortOrder(), 10000);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetFadeDuration(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetContentSize().x, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetContentSize().y, 0.0f, 0.0f);
	ZENITH_ASSERT_FALSE(
		xPlayer.GetComponent<ZM_PlayerController>().IsMovementEnabled());

	ZM_GameStateManager::ResetRuntimeStateForTests();
	pxFade->Update(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ(xManager.GetTargetBuildIndex(),
		ZM_GameStateManager::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xManager.GetTargetSpawnTag(), "");
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID);
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_FALSE(pxFade->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetGroupAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(
		xPlayer.GetComponent<ZM_PlayerController>().IsMovementEnabled());
}

ZENITH_TEST(ZM_WorldTraversal, FadeOutBlocksSingleLoadUntilOpaqueAndIssuesExactlyOnce)
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
	Zenith_UI::Zenith_UIOverlay* pxFade = FindFadeOverlay(xManagerEntity);
	ZENITH_ASSERT_NOT_NULL(pxFade);
	if (pxFade == nullptr) { return; }
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));

	xManager.OnUpdate(std::numeric_limits<float>::quiet_NaN());
	xManager.OnUpdate(-1.0f);
	xManager.OnUpdate(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 0u);

	// Cross transparent -> opaque in one hitch-sized tick. The load callback
	// renders the actual manager canvas before recording load permission, so the
	// submitted WarpFade quad—not only the manager's logical alpha—must be black.
	g_bCaptureRenderedFadeQuad = true;
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 1.25f);
	g_bCaptureRenderedFadeQuad = false;
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u);
	ZENITH_ASSERT_EQ(g_uCapturedBuildIndex, 2u);
	ZENITH_ASSERT_EQ(g_eCapturedTransitionState,
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ_FLOAT(g_fCapturedFadeAlpha, 1.0f, 0.0f);
	ZENITH_ASSERT_TRUE(g_bCapturedFadeVisible);
	ZENITH_ASSERT_TRUE(g_bCapturedRenderedFadeQuad,
		"load permission requires an actually submitted WarpFade quad");
	ZENITH_ASSERT_EQ_FLOAT(g_fCapturedRenderedFadeQuadAlpha, 1.0f, 0.0f,
		"the one-tick opacity crossing must render fully black before SINGLE");
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);
	ZENITH_ASSERT_TRUE(pxFade->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(pxFade->GetGroupAlpha(), 1.0f, 0.0f);

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u,
		"opaque waiting updates must not issue another SINGLE load");
	ZENITH_ASSERT_EQ(xManager.GetIssuedLoadRequestCount(), 1u);
}

ZENITH_TEST(ZM_WorldTraversal, PlacementAndCameraReadinessStayLockedBeforeFadeIn)
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
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xSourceManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xSourceManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(xSourceManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ_FLOAT(xSourceManager.GetFadeAlpha(), 1.0f, 0.0f);

	xScenes.RegisterSceneBuildIndex(2, "ZM_FadeCameraReadinessTarget");
	const Zenith_Scene xTargetScene = xScenes.LoadSceneByIndex(
		2, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xTargetScene.IsValid());
	ZENITH_ASSERT_TRUE(xScenes.SetActiveScene(xTargetScene));
	Zenith_SceneData* pxTargetData = xScenes.GetSceneData(xTargetScene);
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }

	xSourcePlayer.DestroyImmediate();
	Zenith_Entity xTargetPlayer = CreatePlayer(pxTargetData);
	Zenith_Entity xSpawn = CreateSpawnPoint(
		pxTargetData, "TargetTownCenter", "TownCenter");
	ZENITH_ASSERT_TRUE(xTargetPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xSpawn.IsValid());
	if (!xTargetPlayer.IsValid() || !xSpawn.IsValid()) { return; }
	xSpawn.GetComponent<Zenith_TransformComponent>().SetPosition(
		{ 10.0f, 2.0f, 3.0f });
	ZM_PlayerController& xTargetController =
		xTargetPlayer.GetComponent<ZM_PlayerController>();
	xTargetController.OnStart();
	Zenith_ColliderComponent& xTargetCollider =
		xTargetPlayer.GetComponent<Zenith_ColliderComponent>();
	const Zenith_PhysicsBodyID xTargetBodyID = xTargetCollider.GetBodyID();
	g_xEngine.Physics().SetLinearVelocity(
		xTargetBodyID, { 5.0f, -3.0f, 4.0f });
	g_xEngine.Physics().SetAngularVelocity(
		xTargetBodyID, { 1.0f, 2.0f, 3.0f });

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(),
		xTargetPlayer.GetEntityID());
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);
	ZENITH_ASSERT_FALSE(xTargetController.IsMovementEnabled());

	const Zenith_Maths::Vector3 xExpectedCenter(10.0f, 2.9f, 3.0f);
	ZENITH_ASSERT_NEAR_VEC3(
		g_xEngine.Physics().GetBodyPosition(xTargetBodyID),
		xExpectedCenter, 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(
		g_xEngine.Physics().GetLinearVelocity(xTargetBodyID),
		Zenith_Maths::Vector3(0.0f), 0.00001f);
	ZENITH_ASSERT_NEAR_VEC3(
		g_xEngine.Physics().GetAngularVelocity(xTargetBodyID),
		Zenith_Maths::Vector3(0.0f), 0.00001f);

	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_FALSE(xTargetController.IsMovementEnabled());

	Zenith_Entity xCamera = CreateFollowCamera(
		pxTargetData, "TargetCamera", false);
	ZENITH_ASSERT_TRUE(xCamera.IsValid());
	if (!xCamera.IsValid()) { return; }
	Zenith_UnitTests::SetMainCameraForTest(
		pxTargetData, xCamera.GetEntityID());
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_EQ(xCamera.GetComponent<ZM_FollowCamera>().GetTargetEntityID(),
		INVALID_ENTITY_ID);

	xCamera.GetComponent<ZM_FollowCamera>().OnStart();
	ZENITH_ASSERT_EQ(xCamera.GetComponent<ZM_FollowCamera>().GetTargetEntityID(),
		xTargetPlayer.GetEntityID());
	Zenith_Entity xDuplicateCamera = CreateFollowCamera(
		pxTargetData, "DuplicateTargetCamera", true);
	ZENITH_ASSERT_TRUE(xDuplicateCamera.IsValid());
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_FALSE(xTargetController.IsMovementEnabled());

	xDuplicateCamera.DestroyImmediate();
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_FADING_IN);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);
	ZENITH_ASSERT_FALSE(xTargetController.IsMovementEnabled(),
		"camera readiness begins fade-in but must not unlock input early");
}

ZENITH_TEST(ZM_WorldTraversal, FadeInUnlocksAndRuntimeStateIsNotSerializedWithMissingDependenciesSafe)
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
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xSourceManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xSourceManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);

	xScenes.RegisterSceneBuildIndex(2, "ZM_FadeInSerializationTarget");
	const Zenith_Scene xTargetScene = xScenes.LoadSceneByIndex(
		2, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	ZENITH_ASSERT_TRUE(xTargetScene.IsValid());
	ZENITH_ASSERT_TRUE(xScenes.SetActiveScene(xTargetScene));
	Zenith_SceneData* pxTargetData = xScenes.GetSceneData(xTargetScene);
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }

	xSourcePlayer.DestroyImmediate();
	Zenith_Entity xTargetPlayer = CreatePlayer(pxTargetData);
	Zenith_Entity xSpawn = CreateSpawnPoint(
		pxTargetData, "FadeInTownCenter", "TownCenter");
	Zenith_Entity xCamera = CreateFollowCamera(
		pxTargetData, "FadeInCamera", true);
	ZENITH_ASSERT_TRUE(xTargetPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xSpawn.IsValid());
	ZENITH_ASSERT_TRUE(xCamera.IsValid());
	if (!xTargetPlayer.IsValid() || !xSpawn.IsValid() || !xCamera.IsValid())
	{
		return;
	}
	Zenith_UnitTests::SetMainCameraForTest(
		pxTargetData, xCamera.GetEntityID());
	ZM_PlayerController& xController =
		xTargetPlayer.GetComponent<ZM_PlayerController>();
	xController.OnStart();
	// The camera was created before the controller's explicit start, so refresh
	// its generation-bearing target after the replacement Player is frozen.
	xCamera.GetComponent<ZM_FollowCamera>().OnStart();

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(1.0f / 60.0f);
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_FADING_IN);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());

	// Losing camera readiness while fading must restore opacity and keep input
	// locked until a unique replacement camera targets this Player generation.
	xCamera.DestroyImmediate();
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());
	Zenith_Entity xReplacementCamera = CreateFollowCamera(
		pxTargetData, "ReplacementFadeInCamera", true);
	ZENITH_ASSERT_TRUE(xReplacementCamera.IsValid());
	Zenith_UnitTests::SetMainCameraForTest(
		pxTargetData, xReplacementCamera.GetEntityID());
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_FADING_IN);
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());

	// Losing the exact fade overlay freezes the transition opaque. Recreating the
	// authored dependency permits fade-in to finish and only then unlocks input.
	Zenith_UIComponent& xUI =
		xResolvedManager.GetComponent<Zenith_UIComponent>();
	xUI.RemoveElement(ZM_GameStateManager::szFADE_ELEMENT_NAME);
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.25f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_FADING_IN);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());
	ZENITH_ASSERT_NULL(FindFadeOverlay(xResolvedManager));

	Zenith_UI::Zenith_UIOverlay* pxReplacementFade = xUI.CreateOverlay(
		ZM_GameStateManager::szFADE_ELEMENT_NAME);
	ConfigureFadeOverlay(pxReplacementFade);
	ZENITH_ASSERT_NOT_NULL(pxReplacementFade);
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	pxReplacementFade->Update(0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID);
	ZENITH_ASSERT_TRUE(xController.IsMovementEnabled());
	ZENITH_ASSERT_FALSE(pxReplacementFade->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(
		pxReplacementFade->GetGroupAlpha(), 0.0f, 0.0f);

	// Runtime transition data is deliberately absent from the v1 payload. A
	// duplicate with no UI reads only the version and stays at safe defaults,
	// without unfreezing the authoritative manager's live transition.
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(40u, "Door"));
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled());

	Zenith_DataStream xSerialized;
	xManager.WriteToDataStream(xSerialized);
	ZENITH_ASSERT_EQ(xSerialized.GetCursor(), sizeof(u_int));
	Zenith_Entity xDuplicate =
		xScenes.CreateEntity(pxTargetData, "SerializedDuplicateManager");
	ZM_GameStateManager& xDuplicateManager =
		xDuplicate.AddComponent<ZM_GameStateManager>();
	xSerialized.SetCursor(0u);
	xDuplicateManager.ReadFromDataStream(xSerialized);
	ZENITH_ASSERT_EQ(xSerialized.GetCursor(), sizeof(u_int));
	ZENITH_ASSERT_EQ(xDuplicateManager.GetTransitionState(),
		ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ(xDuplicateManager.GetTargetBuildIndex(),
		ZM_GameStateManager::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xDuplicateManager.GetTargetSpawnTag(), "");
	ZENITH_ASSERT_EQ(xDuplicateManager.GetFrozenPlayerEntityID(),
		INVALID_ENTITY_ID);
	ZENITH_ASSERT_EQ(xDuplicateManager.GetIssuedLoadRequestCount(), 0u);
	ZENITH_ASSERT_EQ_FLOAT(xDuplicateManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.5f, 0.00001f);
	ZENITH_ASSERT_FALSE(xController.IsMovementEnabled(),
		"duplicate deserialization must not release authoritative input lock");
}

// ============================================================================
// S8 item 2 -- THE WARP BARRIER TIMEOUT
//
// Three of the six transition states poll and bare-`return` on failure, and the
// fade is already driven to FULLY OPAQUE in QUEUED before IssueSingleLoad runs.
// A stall in any of them was a PERMANENT BLACK SCREEN with the player frozen --
// no crash, no assert, no red test, which is exactly why it shipped-adjacent
// twice. The five tests below are the ones that can see it.
//
// ★ EVERY LOOP HERE ACCUMULATES A BOOL AND ASSERTS ONCE. Asserting inside a
// 3600-iteration loop would report thousands of passes for one property and bury
// the actual failure; the accumulate-then-assert shape used elsewhere in this
// file keeps the runner's output proportional to the claim being made.
// ============================================================================

ZENITH_TEST(ZM_WorldTraversal, WarpBarrierTimeoutEscapesWaitingForSceneAfterItsFrameBudget)
{
	// COMPILE-TIME, not `if`: /W4 /WX makes a runtime conditional over an
	// expression of compile-time constants C4127, and these are exactly that shape.
	static_assert(
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_SCENE
			> ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_SPAWN,
		"the scene barrier needs the MOST headroom -- async scene load plus terrain "
		"streaming happen behind it");
	static_assert(
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_SPAWN
			> ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_CAMERA,
		"the camera barrier should resolve in a frame or two and needs the least");
	static_assert(
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_CAMERA > 420u,
		"every budget must exceed the automated harness's own largest per-barrier "
		"phase allowance (420 frames), so a harness budget always fails FIRST and "
		"this timeout can never mask a real regression by escaping ahead of it");

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

	// The captured callback stands in for the SINGLE load: the request is recorded
	// and NOTHING loads, which is precisely the production hang -- a build index
	// that ZM_WorldSpec offers but that never becomes the active loaded scene.
	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f,
		"the barrier is entered on a FULLY OPAQUE screen -- that is what makes a "
		"stall here invisible rather than merely wrong");

	// Burn the budget exactly. The counter is 0 on the FIRST polled update in a
	// state, so `budget` updates leave it one short and the machine must still be
	// parked -- a timeout that fired early would be worse than the hang it replaces.
	const u_int uBudgetFrames =
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_SCENE;
	bool bLeftBarrierEarly = false;
	for (u_int uFrame = 0u; uFrame < uBudgetFrames; ++uFrame)
	{
		xManager.OnUpdate(1.0f / 60.0f);
		bLeftBarrierEarly = bLeftBarrierEarly
			|| xManager.GetTransitionState() != ZM_WARP_TRANSITION_WAITING_FOR_SCENE
			|| xManager.HasTransitionTimedOut();
	}
	ZENITH_ASSERT_FALSE(bLeftBarrierEarly,
		"the scene barrier must not expire one frame before its budget");
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), uBudgetFrames - 1u);
	ZENITH_ASSERT_EQ(g_uCapturedLoadCount, 1u,
		"waiting out the budget must not re-issue the SINGLE load");

	// The escape frame.
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_FADING_IN,
		"an expired scene barrier must escape to FADING_IN, not spin forever");
	ZENITH_ASSERT_TRUE(xManager.HasTransitionTimedOut());
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), uBudgetFrames);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID,
		"the escape mirrors the playerless-destination jump, frozen id included");
	ZENITH_ASSERT_LT(xManager.GetFadeAlpha(), 1.0f,
		"the escape must actually start clearing the screen");

	// ★ THE CLAUSE THAT PROVES THE ESCAPE ESCAPES. The target scene is STILL not
	// active -- that is why the barrier expired -- and AdvanceFadeIn's first act is
	// normally to bounce a fade-in with no active target scene straight back to
	// WAITING_FOR_SCENE. Without the timed-out latch this drive re-enters the
	// barrier, the state change resets the counter, and the "fix" is a black screen
	// that logs one error per budget forever instead of a black screen that logs
	// nothing. Drive it to completion and require IDLE with a transparent screen.
	bool bReenteredExpiredBarrier = false;
	for (u_int uFrame = 0u; uFrame < 8u; ++uFrame)
	{
		xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS * 0.5f);
		bReenteredExpiredBarrier = bReenteredExpiredBarrier
			|| xManager.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_SCENE;
	}
	ZENITH_ASSERT_FALSE(bReenteredExpiredBarrier,
		"the timed-out escape must not bounce back into the barrier that expired");
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_FALSE(xManager.HasTransitionTimedOut(),
		"the latch belongs to ONE transition and must not outlive it");
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_EQ(xManager.GetTargetBuildIndex(),
		ZM_GameStateManager::uINVALID_BUILD_INDEX);
	ZENITH_ASSERT_STREQ(xManager.GetLastArrivedSpawnTag(), "",
		"a timed-out warp ARRIVED NOWHERE: recording a tag would persist a lie, and "
		"the arrival tail also sets story flags and arms a milestone autosave");
}

ZENITH_TEST(ZM_WorldTraversal, WarpBarrierTimeoutEscapesWaitingForSpawnAndReleasesTheFrozenPlayer)
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
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xManagerEntity.GetComponent<ZM_GameStateManager>().OnUpdate(
		ZM_GameStateManager::fFADE_DURATION_SECONDS);

	// The destination loads and offers a Player -- but NO ZM_SpawnPoint carrying the
	// requested tag. This is the canonical production hang: IsWarpDestinationValid
	// consults only the compiled world table, so "TownCenter" was ACCEPTED at issue
	// time and nothing downstream ever complains.
	Zenith_SceneData* pxTargetData =
		ActivateWarpTargetScene("ZM_WarpSpawnTimeoutTarget");
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }
	xSourcePlayer.DestroyImmediate();
	Zenith_Entity xTargetPlayer = CreatePlayer(pxTargetData);
	ZENITH_ASSERT_TRUE(xTargetPlayer.IsValid());
	if (!xTargetPlayer.IsValid()) { return; }
	ZM_PlayerController& xTargetController =
		xTargetPlayer.GetComponent<ZM_PlayerController>();
	xTargetController.OnStart();

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
	ZENITH_ASSERT_EQ(ZM_SpawnPoint::FindUniqueInScene(
		xScenes.GetActiveScene(), "TownCenter", xSpawnID),
		ZM_SPAWN_POINT_LOOKUP_MISSING,
		"the fixture must genuinely be missing the marker, not merely slow");

	const u_int uBudgetFrames =
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_SPAWN;
	bool bLeftBarrierEarly = false;
	bool bPlayerEverUnfrozen = false;
	for (u_int uFrame = 0u; uFrame < uBudgetFrames; ++uFrame)
	{
		xManager.OnUpdate(1.0f / 60.0f);
		bLeftBarrierEarly = bLeftBarrierEarly
			|| xManager.GetTransitionState() != ZM_WARP_TRANSITION_WAITING_FOR_SPAWN
			|| xManager.HasTransitionTimedOut();
		bPlayerEverUnfrozen = bPlayerEverUnfrozen
			|| xTargetController.IsMovementEnabled();
	}
	ZENITH_ASSERT_FALSE(bLeftBarrierEarly,
		"the spawn barrier must not expire one frame before its budget");
	ZENITH_ASSERT_FALSE(bPlayerEverUnfrozen,
		"the player stays frozen for the whole wait -- which is the other half of "
		"what makes this hang, and must remain true right up to the escape");
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), uBudgetFrames - 1u);
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(),
		xTargetPlayer.GetEntityID());

	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_FADING_IN,
		"an expired spawn barrier must escape to FADING_IN, not spin forever");
	ZENITH_ASSERT_TRUE(xManager.HasTransitionTimedOut());
	ZENITH_ASSERT_EQ(xManager.GetFrozenPlayerEntityID(), INVALID_ENTITY_ID);

	// Finish the escape. The screen coming back is only half the deliverable: a
	// visible destination the player is frozen inside is still a hang, so the
	// transition must END rather than leave input locked forever.
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_FALSE(xManager.HasTransitionTimedOut());
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 0u);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xTargetController.IsMovementEnabled(),
		"the escape must release the frozen player, or the screen comes back on a "
		"player who still cannot move");
	ZENITH_ASSERT_STREQ(xManager.GetLastArrivedSpawnTag(), "",
		"nothing arrived: the marker the transition was waiting for never existed");
}

ZENITH_TEST(ZM_WorldTraversal, WarpBarrierTimeoutEscapesWaitingForCameraAfterItsFrameBudget)
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
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xManagerEntity.GetComponent<ZM_GameStateManager>().OnUpdate(
		ZM_GameStateManager::fFADE_DURATION_SECONDS);

	// Scene, player AND marker all present -- placement succeeds -- but the
	// destination authors no ZM_FollowCamera. Since 9b5a401b the camera acquires the
	// player BY COMPONENT, so a destination missing either component parks here.
	Zenith_SceneData* pxTargetData =
		ActivateWarpTargetScene("ZM_WarpCameraTimeoutTarget");
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }
	xSourcePlayer.DestroyImmediate();
	Zenith_Entity xTargetPlayer = CreatePlayer(pxTargetData);
	Zenith_Entity xSpawn = CreateSpawnPoint(
		pxTargetData, "CameraTimeoutTownCenter", "TownCenter");
	ZENITH_ASSERT_TRUE(xTargetPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xSpawn.IsValid());
	if (!xTargetPlayer.IsValid() || !xSpawn.IsValid()) { return; }
	xSpawn.GetComponent<Zenith_TransformComponent>().SetPosition(
		{ 4.0f, 1.0f, 6.0f });
	ZM_PlayerController& xTargetController =
		xTargetPlayer.GetComponent<ZM_PlayerController>();
	xTargetController.OnStart();

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA,
		"placement must succeed so the wait under test is the CAMERA barrier");
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 0u,
		"entering a new barrier starts its budget from zero, not from the last one");

	const u_int uBudgetFrames =
		ZM_GameStateManager::uWARP_TIMEOUT_FRAMES_WAITING_FOR_CAMERA;
	bool bLeftBarrierEarly = false;
	for (u_int uFrame = 0u; uFrame < uBudgetFrames; ++uFrame)
	{
		xManager.OnUpdate(1.0f / 60.0f);
		bLeftBarrierEarly = bLeftBarrierEarly
			|| xManager.GetTransitionState() != ZM_WARP_TRANSITION_WAITING_FOR_CAMERA
			|| xManager.HasTransitionTimedOut();
	}
	ZENITH_ASSERT_FALSE(bLeftBarrierEarly,
		"the camera barrier must not expire one frame before its budget");
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), uBudgetFrames - 1u);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 1.0f, 0.0f);

	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_FADING_IN,
		"an expired camera barrier must escape to FADING_IN, not spin forever");
	ZENITH_ASSERT_TRUE(xManager.HasTransitionTimedOut());

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xTargetController.IsMovementEnabled());
	ZENITH_ASSERT_STREQ(xManager.GetLastArrivedSpawnTag(), "");
}

// ★ THIS IS THE CLAUSE PROTECTING THE ONE-COUNTER DESIGN. The budget is shared by
// all three barriers and is reset by comparing the current transition state against
// the previously-polled one at the top of the update -- deliberately NOT at each
// `m_eTransitionState = ...` assignment site, of which there are a dozen across five
// functions. Drop the comparison and the counter accumulates across barriers, so the
// second barrier of a warp inherits the first one's dwell and can expire in ONE
// frame. Nothing else in this file would notice.
ZENITH_TEST(ZM_WorldTraversal, WarpBarrierDwellCounterResetsAcrossEveryStateChange)
{
	ZM_GameStateManager::ResetRuntimeStateForTests();
	ZENITH_ASSERT_TRUE(ResetEmptyPhysicsWorld());
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	Zenith_SceneData* pxSceneData = GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData);
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xPlayer = CreatePlayer(pxSceneData);
	Zenith_Entity xManagerEntity = CreateAuthoritativeManager(pxSceneData);
	ZENITH_ASSERT_TRUE(xPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xManagerEntity.IsValid());
	if (!xPlayer.IsValid() || !xManagerEntity.IsValid()) { return; }
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	ZM_GameStateManager& xManager =
		xManagerEntity.GetComponent<ZM_GameStateManager>();
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(2u, "TownCenter"));

	// Five updates too small to reach opacity: the machine dwells in QUEUED. The
	// counter is 0 on the first polled update of a state, so five leave it at four.
	for (u_int uFrame = 0u; uFrame < 5u; ++uFrame)
	{
		xManager.OnUpdate(0.001f);
	}
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_QUEUED);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 4u);

	// Cross to opacity. The state moves INSIDE this update, so the counter still
	// carries QUEUED's dwell when the update returns -- it is the NEXT polled update
	// that must see the change and start the new barrier at zero.
	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 5u);

	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SCENE);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 0u,
		"the shared counter must RESET on a state change -- accumulating would let "
		"one barrier's dwell expire the next one");
	xManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 1u,
		"...and then resume counting from that reset");

	// Now the same property between two POLLING barriers, which is the pairing that
	// actually matters: activate the target so the scene barrier clears, and require
	// the spawn barrier to start from zero rather than from the scene barrier's dwell.
	Zenith_SceneData* pxTargetData =
		ActivateWarpTargetScene("ZM_WarpDwellResetTarget");
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }
	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xTargetManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();

	xTargetManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xTargetManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	ZENITH_ASSERT_EQ(xTargetManager.GetFramesInTransitionState(), 2u,
		"the scene barrier's own dwell is still the reported one on the frame it "
		"hands over");
	xTargetManager.OnUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_EQ(xTargetManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN,
		"the destination has no player, so the spawn barrier parks -- with a budget "
		"of its own, counted from zero");
	ZENITH_ASSERT_EQ(xTargetManager.GetFramesInTransitionState(), 0u);
	ZENITH_ASSERT_FALSE(xTargetManager.HasTransitionTimedOut());
}

// ★ THE ANTI-VACUITY ARM. Without it, a timeout that fired on its FIRST frame would
// satisfy every clause above: each of those tests proves the escape happens, none of
// them proves it does not happen to a healthy warp. This one drives a complete,
// well-formed transition and requires the timeout to stay entirely out of the way --
// and requires the ARRIVAL TAIL to have run, which the timed-out escape deliberately
// skips, so the two paths cannot be confused for one another.
ZENITH_TEST(ZM_WorldTraversal, WarpCompletingWithinBudgetNeverTripsTheBarrierTimeout)
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
	const Zenith_EntityID xManagerID = xManagerEntity.GetEntityID();

	LoadCallbackScope xCallbackScope;
	bool bTimedOutAtAnyPoint = false;
	ZENITH_ASSERT_TRUE(ZM_GameStateManager::RequestWarp(2u, "TownCenter"));
	xManagerEntity.GetComponent<ZM_GameStateManager>().OnUpdate(
		ZM_GameStateManager::fFADE_DURATION_SECONDS);
	bTimedOutAtAnyPoint = bTimedOutAtAnyPoint
		|| xManagerEntity.GetComponent<ZM_GameStateManager>()
			.HasTransitionTimedOut();

	Zenith_SceneData* pxTargetData =
		ActivateWarpTargetScene("ZM_WarpWithinBudgetTarget");
	ZENITH_ASSERT_NOT_NULL(pxTargetData);
	if (pxTargetData == nullptr) { return; }
	xSourcePlayer.DestroyImmediate();
	Zenith_Entity xTargetPlayer = CreatePlayer(pxTargetData);
	Zenith_Entity xSpawn = CreateSpawnPoint(
		pxTargetData, "WithinBudgetTownCenter", "TownCenter");
	Zenith_Entity xCamera = CreateFollowCamera(
		pxTargetData, "WithinBudgetCamera", true);
	ZENITH_ASSERT_TRUE(xTargetPlayer.IsValid());
	ZENITH_ASSERT_TRUE(xSpawn.IsValid());
	ZENITH_ASSERT_TRUE(xCamera.IsValid());
	if (!xTargetPlayer.IsValid() || !xSpawn.IsValid() || !xCamera.IsValid())
	{
		return;
	}
	Zenith_UnitTests::SetMainCameraForTest(pxTargetData, xCamera.GetEntityID());
	ZM_PlayerController& xTargetController =
		xTargetPlayer.GetComponent<ZM_PlayerController>();
	xTargetController.OnStart();
	// The camera was created before the controller's explicit start, so refresh its
	// generation-bearing target after the replacement Player is frozen.
	xCamera.GetComponent<ZM_FollowCamera>().OnStart();

	Zenith_Entity xResolvedManager = xScenes.ResolveEntity(xManagerID);
	ZENITH_ASSERT_TRUE(xResolvedManager.IsValid());
	if (!xResolvedManager.IsValid()) { return; }
	ZM_GameStateManager& xManager =
		xResolvedManager.GetComponent<ZM_GameStateManager>();

	// Each barrier clears on its FIRST polled update -- three orders of magnitude
	// inside the tightest budget. If any of these dwell counts ever grows, the budgets
	// were the wrong shape, not this test.
	xManager.OnUpdate(1.0f / 60.0f);
	bTimedOutAtAnyPoint = bTimedOutAtAnyPoint || xManager.HasTransitionTimedOut();
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_SPAWN);
	xManager.OnUpdate(1.0f / 60.0f);
	bTimedOutAtAnyPoint = bTimedOutAtAnyPoint || xManager.HasTransitionTimedOut();
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(),
		ZM_WARP_TRANSITION_WAITING_FOR_CAMERA);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 0u);
	xManager.OnUpdate(1.0f / 60.0f);
	bTimedOutAtAnyPoint = bTimedOutAtAnyPoint || xManager.HasTransitionTimedOut();
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_FADING_IN);
	ZENITH_ASSERT_EQ(xManager.GetFramesInTransitionState(), 0u);

	xManager.OnUpdate(ZM_GameStateManager::fFADE_DURATION_SECONDS);
	bTimedOutAtAnyPoint = bTimedOutAtAnyPoint || xManager.HasTransitionTimedOut();
	ZENITH_ASSERT_FALSE(bTimedOutAtAnyPoint,
		"a warp that clears every barrier on its first polled update must never "
		"latch the timeout -- a timeout that fires instantly would otherwise pass "
		"every other clause in this file");
	ZENITH_ASSERT_EQ(xManager.GetTransitionState(), ZM_WARP_TRANSITION_IDLE);
	ZENITH_ASSERT_EQ_FLOAT(xManager.GetFadeAlpha(), 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xTargetController.IsMovementEnabled());
	ZENITH_ASSERT_STREQ(xManager.GetLastArrivedSpawnTag(), "TownCenter",
		"the SUCCESS tail records the arrival -- which is exactly what the timed-out "
		"escape must not do, and is how the two paths stay distinguishable");

	// The success tail also LATCHES a milestone autosave for a later frame. Drop it
	// here rather than leaving it armed for whatever runs next: this is the only test
	// in the file that completes a warp and then stops, and a leaked latch would have
	// the next IDLE update write a save slot on its behalf.
	ZM_GameStateManager::ResetRuntimeStateForTests();
}
