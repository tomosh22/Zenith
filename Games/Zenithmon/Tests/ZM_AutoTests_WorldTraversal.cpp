#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;
	constexpr float fPOSITION_EPSILON = 0.001f;
	constexpr float fVELOCITY_EPSILON = 0.0001f;

	struct ManagerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_GameStateManager* m_pxManager = nullptr;
		u_int m_uCount = 0u;
	};

	struct PlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xScale = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	struct CameraView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera* m_pxFollow = nullptr;
		Zenith_CameraComponent* m_pxCamera = nullptr;
	};

	bool FindUniqueManager(ManagerView& xOut)
	{
		xOut = ManagerView{};
		g_xEngine.Scenes().QueryAllScenes<ZM_GameStateManager>().ForEach(
			[&xOut](Zenith_EntityID xEntityID, ZM_GameStateManager& xManager)
			{
				++xOut.m_uCount;
				if (xOut.m_uCount == 1u)
				{
					xOut.m_xEntityID = xEntityID;
					xOut.m_pxManager = &xManager;
				}
			});
		return xOut.m_uCount == 1u && xOut.m_pxManager != nullptr;
	}

	bool FindUniqueActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
				xTransform.GetScale(xOut.m_xScale);
			});
		return uCount == 1u;
	}

	bool FindUniqueActiveCamera(CameraView& xOut)
	{
		xOut = CameraView{};
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&](Zenith_EntityID xEntityID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				++uCount;
				if (uCount != 1u)
				{
					return;
				}
				xOut.m_xEntityID = xEntityID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return uCount == 1u;
	}

	bool RequiredWarpAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 8> astrRequired = {
			strRoot + "Scenes/FrontEnd" ZENITH_SCENE_EXT,
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" ZENITH_MESH_EXT,
		};

		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulSize == 0u)
			{
				return false;
			}
		}
		return true;
	}

	void ClearTraversalInput()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_UP, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_DOWN, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_RIGHT_SHIFT, false);
	}

	enum class WarpInfrastructurePhase
	{
		FrontEnd,
		Dawnmere,
		Done,
	};

	WarpInfrastructurePhase g_eWarpPhase = WarpInfrastructurePhase::Done;
	int g_iWarpPhaseFrames = 0;
	bool g_bWarpPrerequisitesPresent = false;
	bool g_bWarpPassed = false;
	bool g_bDeferredSingleProven = false;
	const char* g_szWarpFailure = "test did not reach verification";
	Zenith_Scene g_xFrontEndScene;
	Zenith_EntityID g_xManagerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xFrontEndGameManagerID = INVALID_ENTITY_ID;

	void FailWarpInfrastructure(const char* szReason)
	{
		g_szWarpFailure = szReason;
		g_bWarpPassed = false;
		g_eWarpPhase = WarpInfrastructurePhase::Done;
		ClearTraversalInput();
	}

	void Setup_WarpInfrastructure()
	{
		g_eWarpPhase = WarpInfrastructurePhase::Done;
		g_iWarpPhaseFrames = 0;
		g_bWarpPrerequisitesPresent = RequiredWarpAssetsPresent();
		g_bWarpPassed = false;
		g_bDeferredSingleProven = false;
		g_szWarpFailure = "test did not reach verification";
		g_xFrontEndScene = Zenith_Scene();
		g_xManagerEntityID = INVALID_ENTITY_ID;
		g_xFrontEndGameManagerID = INVALID_ENTITY_ID;
		Zenith_InputSimulator::ResetAllInputState();

		// RequestSkip bypasses Verify, so no fixed-dt or other process state may be
		// installed until every ignored scene/terrain prerequisite is known present.
		if (!g_bWarpPrerequisitesPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"FrontEnd/Dawnmere scenes or the Dawnmere terrain bake are absent");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);
		g_eWarpPhase = WarpInfrastructurePhase::FrontEnd;
	}

	bool Step_WarpInfrastructure(int)
	{
		if (!g_bWarpPrerequisitesPresent
			|| g_eWarpPhase == WarpInfrastructurePhase::Done)
		{
			return false;
		}

		++g_iWarpPhaseFrames;
		switch (g_eWarpPhase)
		{
		case WarpInfrastructurePhase::FrontEnd:
		{
			g_xFrontEndScene = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxFrontEndData =
				g_xEngine.Scenes().GetSceneData(g_xFrontEndScene);
			const Zenith_SceneInfo xFrontEndInfo =
				g_xEngine.Scenes().GetSceneInfo(g_xFrontEndScene);
			ManagerView xManager;
			if (pxFrontEndData == nullptr || xFrontEndInfo.m_iBuildIndex != 0
				|| !FindUniqueManager(xManager))
			{
				if (g_iWarpPhaseFrames > 180)
				{
					FailWarpInfrastructure(
						"FrontEnd or its unique persistent manager was not ready in 180 frames");
					return false;
				}
				return true;
			}

			Zenith_Entity xFrontEndGameManager =
				pxFrontEndData->FindEntityByName("GameManager");
			Zenith_EntityID xSingletonID = INVALID_ENTITY_ID;
			Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(
				g_xEngine.Scenes().GetPersistentScene());
			if (!xFrontEndGameManager.IsValid()
				|| !ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID)
				|| xSingletonID != xManager.m_xEntityID
				|| !xManager.m_pxManager->IsAuthoritativeSingleton()
				|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
				|| g_xEngine.Scenes().GetSceneDataForEntity(xManager.m_xEntityID)
					!= pxPersistentData)
			{
				FailWarpInfrastructure(
					"FrontEnd manager singleton/persistence contract was invalid");
				return false;
			}

			g_xManagerEntityID = xManager.m_xEntityID;
			g_xFrontEndGameManagerID = xFrontEndGameManager.GetEntityID();
			const bool bAccepted = ZM_GameStateManager::RequestWarp(2u, "TownCenter");
			if (!bAccepted
				|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_QUEUED
				|| xManager.m_pxManager->GetTargetBuildIndex() != 2u
				|| std::strcmp(xManager.m_pxManager->GetTargetSpawnTag(), "TownCenter") != 0
				|| xManager.m_pxManager->GetIssuedLoadRequestCount() != 0u
				|| g_xEngine.Scenes().GetActiveScene() != g_xFrontEndScene
				|| !g_xFrontEndScene.IsValid()
				|| g_xEngine.Scenes().GetSceneDataForEntity(g_xFrontEndGameManagerID)
					!= pxFrontEndData)
			{
				FailWarpInfrastructure(
					"RequestWarp was not a queued, deferred FrontEnd SINGLE transition");
				return false;
			}

			g_bDeferredSingleProven = true;
			g_eWarpPhase = WarpInfrastructurePhase::Dawnmere;
			g_iWarpPhaseFrames = 0;
			return true;
		}

		case WarpInfrastructurePhase::Dawnmere:
		{
			const Zenith_Scene xDawnmereScene = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxDawnmereData =
				g_xEngine.Scenes().GetSceneData(xDawnmereScene);
			const Zenith_SceneInfo xDawnmereInfo =
				g_xEngine.Scenes().GetSceneInfo(xDawnmereScene);
			if (pxDawnmereData == nullptr || xDawnmereInfo.m_iBuildIndex != 2)
			{
				if (g_iWarpPhaseFrames > 420)
				{
					FailWarpInfrastructure("Dawnmere did not load in 420 frames");
					return false;
				}
				return true;
			}

			ManagerView xManager;
			PlayerView xPlayer;
			CameraView xCamera;
			Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
			const ZM_SPAWN_POINT_LOOKUP_RESULT eSpawnResult =
				ZM_SpawnPoint::FindUniqueInScene(
					xDawnmereScene, "TownCenter", xSpawnID);
			Zenith_Entity xSpawnEntity = g_xEngine.Scenes().ResolveEntity(xSpawnID);
			if (!FindUniqueManager(xManager)
				|| !FindUniqueActivePlayer(xPlayer)
				|| !FindUniqueActiveCamera(xCamera)
				|| eSpawnResult != ZM_SPAWN_POINT_LOOKUP_FOUND
				|| !xSpawnEntity.IsValid()
				|| !xPlayer.m_pxCollider->HasValidBody()
				|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
				|| !xPlayer.m_pxController->IsMovementEnabled()
				|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID)
			{
				if (g_iWarpPhaseFrames > 420)
				{
					FailWarpInfrastructure(
						"Dawnmere manager/spawn/player/camera did not settle in 420 frames");
					return false;
				}
				return true;
			}

			Zenith_EntityID xSingletonID = INVALID_ENTITY_ID;
			Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(
				g_xEngine.Scenes().GetPersistentScene());
			Zenith_Maths::Vector3 xMarkerFeet(0.0f);
			xSpawnEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xMarkerFeet);
			const float fCapsuleHalfExtent =
				ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayer.m_xScale);
			const Zenith_Maths::Vector3 xExpectedCenter =
				ZM_GameStateManager::CalculateSpawnCenter(
					xMarkerFeet, xPlayer.m_xScale);
			const Zenith_Maths::Vector3 xBodyPosition =
				g_xEngine.Physics().GetBodyPosition(xPlayer.m_pxCollider->GetBodyID());
			const Zenith_Maths::Vector3 xLinearVelocity =
				g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
			const Zenith_Maths::Vector3 xAngularVelocity =
				g_xEngine.Physics().GetAngularVelocity(xPlayer.m_pxCollider->GetBodyID());

			const bool bManagerPersisted =
				ZM_GameStateManager::TryGetUniqueSingletonEntityID(xSingletonID)
				&& xSingletonID == g_xManagerEntityID
				&& xManager.m_xEntityID == g_xManagerEntityID
				&& xManager.m_pxManager->IsAuthoritativeSingleton()
				&& xManager.m_pxManager->GetIssuedLoadRequestCount() == 1u
				&& g_xEngine.Scenes().GetSceneDataForEntity(g_xManagerEntityID)
					== pxPersistentData;
			const bool bFrontEndGone = !g_xFrontEndScene.IsValid()
				&& g_xEngine.Scenes().GetSceneDataForEntity(
					g_xFrontEndGameManagerID) == nullptr;
			const bool bSceneOwnedPair =
				g_xEngine.Scenes().GetSceneDataForEntity(xPlayer.m_xEntityID)
					== pxDawnmereData
				&& g_xEngine.Scenes().GetSceneDataForEntity(xCamera.m_xEntityID)
					== pxDawnmereData
				&& g_xEngine.Scenes().FindMainCameraEntityAcrossScenes()
					== xCamera.m_xEntityID;
			const bool bExactPlacement =
				std::fabs(xMarkerFeet.x - 512.0f) <= fPOSITION_EPSILON
				&& std::fabs(xMarkerFeet.y - 25.98577f) <= fPOSITION_EPSILON
				&& std::fabs(xMarkerFeet.z - 480.0f) <= fPOSITION_EPSILON
				&& std::fabs(fCapsuleHalfExtent - 0.9f) <= fPOSITION_EPSILON
				&& glm::length(xPlayer.m_xPosition - xExpectedCenter)
					<= fPOSITION_EPSILON
				&& glm::length(xBodyPosition - xExpectedCenter)
					<= fPOSITION_EPSILON
				&& std::fabs(
					xPlayer.m_xPosition.y - fCapsuleHalfExtent - xMarkerFeet.y)
					<= fPOSITION_EPSILON;
			const bool bZeroMotion = glm::length(xLinearVelocity)
				<= fVELOCITY_EPSILON
				&& glm::length(xAngularVelocity) <= fVELOCITY_EPSILON
				&& xPlayer.m_pxController->GetRequestedSpeed() == 0.0f
				&& glm::length(xPlayer.m_pxController->GetMoveDirection())
					<= fVELOCITY_EPSILON;

			if (!g_bDeferredSingleProven || !bManagerPersisted || !bFrontEndGone
				|| !bSceneOwnedPair || !bExactPlacement || !bZeroMotion)
			{
				FailWarpInfrastructure(
					"Dawnmere persistence, teardown, placement, zero-motion, or camera invariant failed");
				return false;
			}

			g_bWarpPassed = true;
			g_eWarpPhase = WarpInfrastructurePhase::Done;
			return false;
		}

		case WarpInfrastructurePhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_WarpInfrastructure()
	{
		ClearTraversalInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		if (g_bWarpPrerequisitesPresent)
		{
			// Restore the harness baseline even on a terminal timeout/failure. The
			// authoritative manager survives and rejects the newly-authored duplicate.
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}
		if (!g_bWarpPassed && g_bWarpPrerequisitesPresent)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_WarpInfrastructure] %s", g_szWarpFailure);
		}
		return g_bWarpPassed || !g_bWarpPrerequisitesPresent;
	}
}

static const Zenith_AutomatedTest g_xZMWarpInfrastructureTest = {
	"ZM_WarpInfrastructure_Test",
	&Setup_WarpInfrastructure,
	&Step_WarpInfrastructure,
	&Verify_WarpInfrastructure,
	/* maxFrames */ 900,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMWarpInfrastructureTest);

#endif // ZENITH_INPUT_SIMULATOR
