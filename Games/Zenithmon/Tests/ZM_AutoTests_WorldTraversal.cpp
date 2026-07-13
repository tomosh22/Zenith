#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;
	// The round trip contains a 128 m real-input traversal. Run that graphics
	// scenario at a deterministic 30 Hz presentation cadence; physics still
	// consumes its normal fixed substeps, while the 1,800-frame test cap retains
	// ample time for both scene transitions and the return leg.
	constexpr float fROUND_TRIP_FIXED_DT = 1.0f / 30.0f;
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

	bool FadeVisualMatchesManager(const ManagerView& xManager)
	{
		Zenith_Entity xEntity =
			g_xEngine.Scenes().ResolveEntity(xManager.m_xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
		Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
			? pxUI->FindElement(ZM_GameStateManager::szFADE_ELEMENT_NAME) : nullptr;
		if (pxElement == nullptr
			|| pxElement->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return false;
		}
		Zenith_UI::Zenith_UIOverlay* pxFade =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxElement);
		const float fAlpha = xManager.m_pxManager->GetFadeAlpha();
		const Zenith_Maths::Vector4 xColour = pxFade->GetDimColor();
		const Zenith_Maths::Vector2 xContentSize = pxFade->GetContentSize();
		return std::isfinite(fAlpha) && fAlpha >= 0.0f && fAlpha <= 1.0f
			&& std::fabs(pxFade->GetGroupAlpha() - fAlpha) <= 0.0001f
			&& (fAlpha == 0.0f || pxFade->IsVisible())
			&& pxFade->IsStretchAll() && pxFade->GetSortOrder() == 10000
			&& pxFade->GetFadeDuration() == 0.0f
			&& xContentSize.x == 0.0f && xContentSize.y == 0.0f
			&& std::fabs(xColour.x) <= 0.0001f
			&& std::fabs(xColour.y) <= 0.0001f
			&& std::fabs(xColour.z) <= 0.0001f
			&& std::fabs(xColour.w - 1.0f) <= 0.0001f;
	}

	bool ManagerFadeOverlayIsHidden(const ManagerView& xManager)
	{
		Zenith_Entity xEntity =
			g_xEngine.Scenes().ResolveEntity(xManager.m_xEntityID);
		Zenith_UIComponent* pxUI = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_UIComponent>() : nullptr;
		Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
			? pxUI->FindElement(ZM_GameStateManager::szFADE_ELEMENT_NAME) : nullptr;
		return pxElement != nullptr
			&& pxElement->GetType() == Zenith_UI::UIElementType::Overlay
			&& !pxElement->IsVisible();
	}

	bool ActiveGrassIsReadyAndUnique()
	{
		u_int uCount = 0u;
		bool bReady = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				++uCount;
				bReady = xGrass.HasCPUMap() && xGrass.IsGrassApplied()
					&& !xGrass.HasTerminalFailure()
					&& xGrass.GetGeneratedBladeCount() > 0u;
			});
		return uCount == 1u && bReady;
	}

	void DriveTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearTraversalInput();
		constexpr float fDEAD_ZONE = 0.08f;
		const float fDeltaX = xTarget.x - xPosition.x;
		const float fDeltaZ = xTarget.z - xPosition.z;
		if (fDeltaX < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
		}
		else if (fDeltaX > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
		}
		if (fDeltaZ < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, true);
		}
		else if (fDeltaZ > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		}
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	}

	bool FindConfiguredActiveTrigger(
		const char* szName,
		u_int uBuildIndex,
		const char* szSpawnTag,
		Zenith_Entity& xEntityOut,
		ZM_WarpTrigger*& pxTriggerOut)
	{
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		xEntityOut = pxData != nullptr
			? pxData->FindEntityByName(szName) : Zenith_Entity();
		pxTriggerOut = xEntityOut.IsValid()
			? xEntityOut.TryGetComponent<ZM_WarpTrigger>() : nullptr;
		Zenith_ColliderComponent* pxCollider = xEntityOut.IsValid()
			? xEntityOut.TryGetComponent<Zenith_ColliderComponent>() : nullptr;
		return pxTriggerOut != nullptr && pxCollider != nullptr
			&& pxCollider->HasValidBody()
			&& pxCollider->GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_AABB
			&& pxCollider->GetRigidBodyType() == RIGIDBODY_TYPE_STATIC
			&& pxTriggerOut->GetTargetBuildIndex() == uBuildIndex
			&& std::strcmp(pxTriggerOut->GetSpawnTag(), szSpawnTag) == 0;
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

	enum class PlayerHomeRoundTripPhase
	{
		FrontEndBootstrap,
		WaitForDawnmere,
		DriveToHomeDoor,
		WaitForPlayerHome,
		DriveToHomeExit,
		WaitForDawnmereReturn,
		Done,
	};

	PlayerHomeRoundTripPhase g_eHomePhase = PlayerHomeRoundTripPhase::Done;
	int g_iHomePhaseFrames = 0;
	bool g_bHomePrerequisitesPresent = false;
	bool g_bHomePassed = false;
	bool g_bHomeInitialFadeOutSeen = false;
	bool g_bHomeInitialOpaqueLoadSeen = false;
	bool g_bHomeInitialCameraBarrierSeen = false;
	bool g_bHomeInitialFadeInSeen = false;
	bool g_bHomeDoorStagingReached = false;
	bool g_bHomeDoorInputMotionSeen = false;
	bool g_bHomeDoorCollisionSeen = false;
	bool g_bHomeDoorFadeOutSeen = false;
	bool g_bHomeDoorOpaqueLoadSeen = false;
	bool g_bPlayerHomeCameraBarrierSeen = false;
	bool g_bPlayerHomeFadeInSeen = false;
	bool g_bHomeExitInputMotionSeen = false;
	bool g_bHomeExitCollisionSeen = false;
	bool g_bHomeExitFadeOutSeen = false;
	bool g_bHomeExitOpaqueLoadSeen = false;
	bool g_bDawnmereReturnCameraBarrierSeen = false;
	bool g_bDawnmereReturnPlacementResetSeen = false;
	bool g_bDawnmereReturnFadeInSeen = false;
	const char* g_szHomeFailure = "test did not reach verification";
	Zenith_Scene g_xHomeFrontEndScene;
	Zenith_Scene g_xHomeInitialDawnmereScene;
	Zenith_Scene g_xPlayerHomeScene;
	Zenith_EntityID g_xHomeManagerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHomeInitialDawnmerePlayerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHomeInitialDawnmereCameraEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xPlayerHomePlayerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xPlayerHomeCameraEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHomeDoorTriggerEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xHomeExitTriggerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xHomeDoorDriveStart(0.0f);
	Zenith_Maths::Vector3 g_xHomeExitDriveStart(0.0f);

	void FailPlayerHomeRoundTrip(const char* szReason)
	{
		g_szHomeFailure = szReason;
		g_bHomePassed = false;
		g_eHomePhase = PlayerHomeRoundTripPhase::Done;
		ClearTraversalInput();
	}

	void Setup_PlayerHomeRoundTrip()
	{
		g_eHomePhase = PlayerHomeRoundTripPhase::Done;
		g_iHomePhaseFrames = 0;
		g_bHomePrerequisitesPresent = RequiredWarpAssetsPresent();
		g_bHomePassed = false;
		g_bHomeInitialFadeOutSeen = false;
		g_bHomeInitialOpaqueLoadSeen = false;
		g_bHomeInitialCameraBarrierSeen = false;
		g_bHomeInitialFadeInSeen = false;
		g_bHomeDoorStagingReached = false;
		g_bHomeDoorInputMotionSeen = false;
		g_bHomeDoorCollisionSeen = false;
		g_bHomeDoorFadeOutSeen = false;
		g_bHomeDoorOpaqueLoadSeen = false;
		g_bPlayerHomeCameraBarrierSeen = false;
		g_bPlayerHomeFadeInSeen = false;
		g_bHomeExitInputMotionSeen = false;
		g_bHomeExitCollisionSeen = false;
		g_bHomeExitFadeOutSeen = false;
		g_bHomeExitOpaqueLoadSeen = false;
		g_bDawnmereReturnCameraBarrierSeen = false;
		g_bDawnmereReturnPlacementResetSeen = false;
		g_bDawnmereReturnFadeInSeen = false;
		g_szHomeFailure = "test did not reach verification";
		g_xHomeFrontEndScene = Zenith_Scene();
		g_xHomeInitialDawnmereScene = Zenith_Scene();
		g_xPlayerHomeScene = Zenith_Scene();
		g_xHomeManagerEntityID = INVALID_ENTITY_ID;
		g_xHomeInitialDawnmerePlayerEntityID = INVALID_ENTITY_ID;
		g_xHomeInitialDawnmereCameraEntityID = INVALID_ENTITY_ID;
		g_xPlayerHomePlayerEntityID = INVALID_ENTITY_ID;
		g_xPlayerHomeCameraEntityID = INVALID_ENTITY_ID;
		g_xHomeDoorTriggerEntityID = INVALID_ENTITY_ID;
		g_xHomeExitTriggerEntityID = INVALID_ENTITY_ID;
		g_xHomeDoorDriveStart = Zenith_Maths::Vector3(0.0f);
		g_xHomeExitDriveStart = Zenith_Maths::Vector3(0.0f);
		Zenith_InputSimulator::ResetAllInputState();
		if (g_bHomePrerequisitesPresent)
		{
			const std::string strHomePath = std::string(GAME_ASSETS_DIR)
				+ "Scenes/PlayerHome" ZENITH_SCENE_EXT;
			std::error_code xError;
			g_bHomePrerequisitesPresent =
				std::filesystem::is_regular_file(strHomePath, xError)
				&& !xError
				&& std::filesystem::file_size(strHomePath, xError) > 0u
				&& !xError;
		}
		if (!g_bHomePrerequisitesPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"FrontEnd, Dawnmere, PlayerHome, or terrain bake assets are absent");
			return;
		}
		Zenith_InputSimulator::SetFixedDt(fROUND_TRIP_FIXED_DT);
		g_eHomePhase = PlayerHomeRoundTripPhase::FrontEndBootstrap;
	}

	bool Step_PlayerHomeRoundTrip(int)
	{
		if (!g_bHomePrerequisitesPresent
			|| g_eHomePhase == PlayerHomeRoundTripPhase::Done)
		{
			return false;
		}
		++g_iHomePhaseFrames;
		switch (g_eHomePhase)
		{
		case PlayerHomeRoundTripPhase::FrontEndBootstrap:
		{
			const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
			ManagerView xManager;
			if (!xScene.IsValid()
				|| g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != 0
				|| !FindUniqueManager(xManager))
			{
				if (g_iHomePhaseFrames > 180)
				{
					FailPlayerHomeRoundTrip(
						"FrontEnd/manager bootstrap did not settle in 180 frames");
					return false;
				}
				return true;
			}
			if (!xManager.m_pxManager->IsAuthoritativeSingleton()
				|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_IDLE
				|| xManager.m_pxManager->GetIssuedLoadRequestCount() != 0u
				|| xManager.m_pxManager->GetFadeAlpha() != 0.0f
				|| !FadeVisualMatchesManager(xManager)
				|| !ManagerFadeOverlayIsHidden(xManager)
				|| g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().Count() != 0u)
			{
				FailPlayerHomeRoundTrip("FrontEnd traversal authority was not idle/playerless");
				return false;
			}
			g_xHomeFrontEndScene = xScene;
			g_xHomeManagerEntityID = xManager.m_xEntityID;
			if (!ZM_GameStateManager::RequestWarp(2u, "TownCenter")
				|| xManager.m_pxManager->GetTransitionState() != ZM_WARP_TRANSITION_QUEUED
				|| xManager.m_pxManager->GetTargetBuildIndex() != 2u
				|| std::strcmp(xManager.m_pxManager->GetTargetSpawnTag(), "TownCenter") != 0
				|| xManager.m_pxManager->GetIssuedLoadRequestCount() != 0u
				|| xManager.m_pxManager->GetFadeAlpha() != 0.0f)
			{
				FailPlayerHomeRoundTrip("FrontEnd-to-Dawnmere warp did not queue transactionally");
				return false;
			}
			g_eHomePhase = PlayerHomeRoundTripPhase::WaitForDawnmere;
			g_iHomePhaseFrames = 0;
			return true;
		}

		case PlayerHomeRoundTripPhase::WaitForDawnmere:
		{
			ManagerView xManager;
			if (!FindUniqueManager(xManager)
				|| xManager.m_xEntityID != g_xHomeManagerEntityID
				|| !FadeVisualMatchesManager(xManager))
			{
				FailPlayerHomeRoundTrip("persistent manager/fade visual changed during bootstrap warp");
				return false;
			}
			ZM_GameStateManager& xState = *xManager.m_pxManager;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_QUEUED)
			{
				if (xState.GetIssuedLoadRequestCount() != 0u
					|| xState.GetFadeAlpha() >= 1.0f)
				{
					FailPlayerHomeRoundTrip("Dawnmere SINGLE issued before opaque fade-out");
					return false;
				}
				g_bHomeInitialFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
			}
			if (xState.GetIssuedLoadRequestCount() == 1u)
			{
				if (xState.GetFadeAlpha() != 1.0f
					&& xState.GetTransitionState() != ZM_WARP_TRANSITION_FADING_IN
					&& xState.GetTransitionState() != ZM_WARP_TRANSITION_IDLE)
				{
					FailPlayerHomeRoundTrip("Dawnmere load was observed before full opacity");
					return false;
				}
				g_bHomeInitialOpaqueLoadSeen |= xState.GetFadeAlpha() == 1.0f;
			}
			if (iBuild == 2)
			{
				PlayerView xPlayer;
				CameraView xCamera;
				Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_CAMERA
					&& FindUniqueActivePlayer(xPlayer)
					&& FindUniqueActiveCamera(xCamera)
					&& pxData != nullptr
					&& pxData->GetMainCameraEntity() == xCamera.m_xEntityID
					&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
					&& !xPlayer.m_pxController->IsMovementEnabled()
					&& xState.GetFadeAlpha() == 1.0f)
				{
					g_bHomeInitialCameraBarrierSeen = true;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_FADING_IN)
				{
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| xPlayer.m_pxController->IsMovementEnabled())
					{
						FailPlayerHomeRoundTrip("Dawnmere fade-in crossed an unready camera/input barrier");
						return false;
					}
					g_bHomeInitialFadeInSeen |= xState.GetFadeAlpha() < 1.0f;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
				{
					Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
					Zenith_Entity xSpawn;
					Zenith_Maths::Vector3 xFeet(0.0f);
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| ZM_SpawnPoint::FindUniqueInScene(xActive, "TownCenter", xSpawnID)
							!= ZM_SPAWN_POINT_LOOKUP_FOUND
						|| !(xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID)).IsValid()
						|| !xPlayer.m_pxController->IsMovementEnabled()
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xState.GetFadeAlpha() != 0.0f
						|| xState.GetIssuedLoadRequestCount() != 1u
						|| !ManagerFadeOverlayIsHidden(xManager)
						|| !ActiveGrassIsReadyAndUnique())
					{
						if (g_iHomePhaseFrames > 420)
						{
							FailPlayerHomeRoundTrip("Dawnmere did not reach its ready authored state");
							return false;
						}
						return true;
					}
					xSpawn.GetComponent<Zenith_TransformComponent>().GetPosition(xFeet);
					const Zenith_Maths::Vector3 xExpected =
						ZM_GameStateManager::CalculateSpawnCenter(xFeet, xPlayer.m_xScale);
					if (!g_bHomeInitialFadeOutSeen || !g_bHomeInitialOpaqueLoadSeen
						|| !g_bHomeInitialCameraBarrierSeen || !g_bHomeInitialFadeInSeen
						|| g_xHomeFrontEndScene.IsValid()
						|| g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID).GetScene()
							!= xActive
						|| g_xEngine.Scenes().ResolveEntity(xCamera.m_xEntityID).GetScene()
							!= xActive
						|| glm::length(xPlayer.m_xPosition - xExpected) > fPOSITION_EPSILON
						|| glm::length(g_xEngine.Physics().GetBodyPosition(
							xPlayer.m_pxCollider->GetBodyID()) - xExpected) > fPOSITION_EPSILON)
					{
						FailPlayerHomeRoundTrip("Dawnmere bootstrap fade/placement contract failed");
						return false;
					}
					g_xHomeInitialDawnmereScene = xActive;
					g_xHomeInitialDawnmerePlayerEntityID = xPlayer.m_xEntityID;
					g_xHomeInitialDawnmereCameraEntityID = xCamera.m_xEntityID;
					g_xHomeDoorDriveStart = xPlayer.m_xPosition;
					g_eHomePhase = PlayerHomeRoundTripPhase::DriveToHomeDoor;
					g_iHomePhaseFrames = 0;
					return true;
				}
			}
			if (g_iHomePhaseFrames > 420)
			{
				FailPlayerHomeRoundTrip("Dawnmere transition timed out");
				return false;
			}
			return true;
		}

		case PlayerHomeRoundTripPhase::DriveToHomeDoor:
		{
			ManagerView xManager;
			PlayerView xPlayer;
			CameraView xCamera;
			Zenith_Entity xTriggerEntity;
			ZM_WarpTrigger* pxTrigger = nullptr;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
			if (g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex != 2
				|| !FindUniqueManager(xManager)
				|| xManager.m_xEntityID != g_xHomeManagerEntityID
				|| !FadeVisualMatchesManager(xManager)
				|| !FindUniqueActivePlayer(xPlayer)
				|| !FindUniqueActiveCamera(xCamera)
				|| pxData == nullptr
				|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
				|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
				|| !FindConfiguredActiveTrigger(
					"HomeDoorTrigger", 40u, "Door", xTriggerEntity, pxTrigger))
			{
				FailPlayerHomeRoundTrip("Dawnmere HomeDoorTrigger/player/camera authoring was invalid");
				return false;
			}
			g_xHomeDoorTriggerEntityID = xTriggerEntity.GetEntityID();
			ZM_GameStateManager& xState = *xManager.m_pxManager;
			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
			{
				if (!xPlayer.m_pxController->IsMovementEnabled()
					|| xState.GetIssuedLoadRequestCount() != 1u
					|| xState.GetFadeAlpha() != 0.0f)
				{
					FailPlayerHomeRoundTrip("Dawnmere input unlocked/transparent precondition failed");
					return false;
				}
				const Zenith_Maths::Vector3 xVelocity =
					g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
				g_bHomeDoorInputMotionSeen |=
					glm::length(xPlayer.m_xPosition - g_xHomeDoorDriveStart) > 1.0f
					&& std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
						- ZM_PlayerController::fRUN_SPEED) <= 0.001f
					&& glm::length(Zenith_Maths::Vector2(xVelocity.x, xVelocity.z)) > 1.0f;
				// The Home shell reaches z=476 and is solid. First align with the
				// doorway while the capsule remains safely outside that face, then
				// approach along -Z so the authored trigger is the first contact.
				const Zenith_Maths::Vector3 xDoorStaging(384.0f, 0.0f, 480.0f);
				if (!g_bHomeDoorStagingReached)
				{
					constexpr float fSTAGING_TOLERANCE = 0.5f;
					g_bHomeDoorStagingReached =
						std::fabs(xPlayer.m_xPosition.x - xDoorStaging.x)
							<= fSTAGING_TOLERANCE
						&& std::fabs(xPlayer.m_xPosition.z - xDoorStaging.z)
							<= fSTAGING_TOLERANCE;
					DriveTowardXZ(xPlayer.m_xPosition, xDoorStaging);
				}
				else
				{
					DriveTowardXZ(xPlayer.m_xPosition,
						Zenith_Maths::Vector3(384.0f, 0.0f, 476.0f));
				}
			}
			else
			{
				ClearTraversalInput();
				if (!g_bHomeDoorStagingReached
					|| xState.GetTransitionState() != ZM_WARP_TRANSITION_QUEUED
					|| !pxTrigger->IsLatched()
					|| xState.GetTargetBuildIndex() != 40u
					|| std::strcmp(xState.GetTargetSpawnTag(), "Door") != 0
					|| xState.GetIssuedLoadRequestCount() != 1u
					|| xState.GetFadeAlpha() >= 1.0f
					|| xState.GetFrozenPlayerEntityID() != xPlayer.m_xEntityID
					|| xPlayer.m_pxController->IsMovementEnabled())
				{
					FailPlayerHomeRoundTrip("real HomeDoor overlap did not queue/freeze build 40");
					return false;
				}
				g_bHomeDoorCollisionSeen = true;
				g_bHomeDoorFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
				g_eHomePhase = PlayerHomeRoundTripPhase::WaitForPlayerHome;
				g_iHomePhaseFrames = 0;
				return true;
			}
			if (g_iHomePhaseFrames > 1300)
			{
				FailPlayerHomeRoundTrip("input-driven Dawnmere HomeDoor approach timed out");
				return false;
			}
			return true;
		}

		case PlayerHomeRoundTripPhase::WaitForPlayerHome:
		{
			ManagerView xManager;
			if (!FindUniqueManager(xManager)
				|| xManager.m_xEntityID != g_xHomeManagerEntityID
				|| !FadeVisualMatchesManager(xManager))
			{
				FailPlayerHomeRoundTrip("manager/fade changed during PlayerHome load");
				return false;
			}
			ZM_GameStateManager& xState = *xManager.m_pxManager;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
			if (iBuild == 2)
			{
				PlayerView xPlayer;
				if (!FindUniqueActivePlayer(xPlayer)
					|| xPlayer.m_pxController->IsMovementEnabled()
					|| xState.GetFrozenPlayerEntityID() != xPlayer.m_xEntityID)
				{
					FailPlayerHomeRoundTrip("Dawnmere player unlocked before PlayerHome SINGLE");
					return false;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_QUEUED)
				{
					if (xState.GetIssuedLoadRequestCount() != 1u
						|| xState.GetFadeAlpha() >= 1.0f)
					{
						FailPlayerHomeRoundTrip("PlayerHome SINGLE issued below opacity");
						return false;
					}
					g_bHomeDoorFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
				}
				else if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_SCENE)
				{
					if (xState.GetIssuedLoadRequestCount() != 2u
						|| xState.GetFadeAlpha() != 1.0f)
					{
						FailPlayerHomeRoundTrip("PlayerHome load was not issued exactly once at opacity");
						return false;
					}
					g_bHomeDoorOpaqueLoadSeen = true;
				}
			}
			else if (iBuild == 40)
			{
				PlayerView xPlayer;
				CameraView xCamera;
				Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
				if (xState.GetIssuedLoadRequestCount() != 2u
					|| g_xHomeInitialDawnmereScene.IsValid()
					|| g_xEngine.Scenes().ResolveEntity(
						g_xHomeInitialDawnmerePlayerEntityID).IsValid()
					|| g_xEngine.Scenes().ResolveEntity(
						g_xHomeInitialDawnmereCameraEntityID).IsValid()
					|| g_xEngine.Scenes().ResolveEntity(g_xHomeDoorTriggerEntityID).IsValid())
				{
					FailPlayerHomeRoundTrip("Dawnmere did not teardown on PlayerHome SINGLE");
					return false;
				}
				g_bHomeDoorOpaqueLoadSeen |= xState.GetFadeAlpha() == 1.0f;
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_CAMERA
					&& FindUniqueActivePlayer(xPlayer) && FindUniqueActiveCamera(xCamera)
					&& pxData != nullptr
					&& pxData->GetMainCameraEntity() == xCamera.m_xEntityID
					&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
					&& !xPlayer.m_pxController->IsMovementEnabled()
					&& xState.GetFadeAlpha() == 1.0f)
				{
					g_bPlayerHomeCameraBarrierSeen = true;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_FADING_IN)
				{
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| xPlayer.m_pxController->IsMovementEnabled())
					{
						FailPlayerHomeRoundTrip("PlayerHome fade-in bypassed camera/input lock");
						return false;
					}
					g_bPlayerHomeFadeInSeen |= xState.GetFadeAlpha() < 1.0f;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
				{
					Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
					Zenith_Entity xSpawn;
					Zenith_Entity xExit;
					ZM_WarpTrigger* pxExit = nullptr;
					Zenith_Maths::Vector3 xFeet(0.0f);
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| ZM_SpawnPoint::FindUniqueInScene(xActive, "Door", xSpawnID)
							!= ZM_SPAWN_POINT_LOOKUP_FOUND
						|| !(xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID)).IsValid()
						|| !FindConfiguredActiveTrigger(
							"PlayerHomeExitTrigger", 2u, "FromHome", xExit, pxExit)
						|| !xPlayer.m_pxController->IsMovementEnabled()
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| xState.GetFadeAlpha() != 0.0f
						|| !ManagerFadeOverlayIsHidden(xManager)
						|| g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().Count() != 0u)
					{
						if (g_iHomePhaseFrames > 420)
						{
							FailPlayerHomeRoundTrip("PlayerHome authored state did not settle");
							return false;
						}
						return true;
					}
					xSpawn.GetComponent<Zenith_TransformComponent>().GetPosition(xFeet);
					const Zenith_Maths::Vector3 xExpected =
						ZM_GameStateManager::CalculateSpawnCenter(xFeet, xPlayer.m_xScale);
					const Zenith_PhysicsBodyID xBody = xPlayer.m_pxCollider->GetBodyID();
					if (!g_bHomeDoorInputMotionSeen || !g_bHomeDoorCollisionSeen
						|| !g_bHomeDoorFadeOutSeen || !g_bHomeDoorOpaqueLoadSeen
						|| !g_bPlayerHomeCameraBarrierSeen || !g_bPlayerHomeFadeInSeen
						|| xPlayer.m_xEntityID == g_xHomeInitialDawnmerePlayerEntityID
						|| xCamera.m_xEntityID == g_xHomeInitialDawnmereCameraEntityID
						|| g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID).GetScene()
							!= xActive
						|| g_xEngine.Scenes().ResolveEntity(xCamera.m_xEntityID).GetScene()
							!= xActive
						|| glm::length(xFeet - Zenith_Maths::Vector3(0.0f, 0.0f, 3.5f))
							> fPOSITION_EPSILON
						|| glm::length(xPlayer.m_xPosition - xExpected) > fPOSITION_EPSILON
						|| glm::length(g_xEngine.Physics().GetBodyPosition(xBody) - xExpected)
							> fPOSITION_EPSILON
						|| glm::length(g_xEngine.Physics().GetLinearVelocity(xBody))
							> fVELOCITY_EPSILON
						|| glm::length(g_xEngine.Physics().GetAngularVelocity(xBody))
							> fVELOCITY_EPSILON)
					{
						FailPlayerHomeRoundTrip("Door placement/motion or inbound transition proof failed");
						return false;
					}
					g_xPlayerHomeScene = xActive;
					g_xPlayerHomePlayerEntityID = xPlayer.m_xEntityID;
					g_xPlayerHomeCameraEntityID = xCamera.m_xEntityID;
					g_xHomeExitTriggerEntityID = xExit.GetEntityID();
					g_xHomeExitDriveStart = xPlayer.m_xPosition;
					g_eHomePhase = PlayerHomeRoundTripPhase::DriveToHomeExit;
					g_iHomePhaseFrames = 0;
					return true;
				}
			}
			else
			{
				FailPlayerHomeRoundTrip("unexpected build index during PlayerHome transition");
				return false;
			}
			if (g_iHomePhaseFrames > 420)
			{
				FailPlayerHomeRoundTrip("PlayerHome transition timed out");
				return false;
			}
			return true;
		}

		case PlayerHomeRoundTripPhase::DriveToHomeExit:
		{
			ManagerView xManager;
			PlayerView xPlayer;
			CameraView xCamera;
			Zenith_Entity xTriggerEntity;
			ZM_WarpTrigger* pxTrigger = nullptr;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
			if (g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex != 40
				|| !FindUniqueManager(xManager)
				|| xManager.m_xEntityID != g_xHomeManagerEntityID
				|| !FadeVisualMatchesManager(xManager)
				|| !FindUniqueActivePlayer(xPlayer)
				|| !FindUniqueActiveCamera(xCamera)
				|| pxData == nullptr
				|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
				|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
				|| !FindConfiguredActiveTrigger(
					"PlayerHomeExitTrigger", 2u, "FromHome", xTriggerEntity, pxTrigger))
			{
				FailPlayerHomeRoundTrip("PlayerHome exit/player/camera authoring was invalid");
				return false;
			}
			ZM_GameStateManager& xState = *xManager.m_pxManager;
			if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
			{
				if (!xPlayer.m_pxController->IsMovementEnabled()
					|| xState.GetIssuedLoadRequestCount() != 2u
					|| xState.GetFadeAlpha() != 0.0f)
				{
					FailPlayerHomeRoundTrip("PlayerHome exit approach precondition failed");
					return false;
				}
				const Zenith_Maths::Vector3 xVelocity =
					g_xEngine.Physics().GetLinearVelocity(xPlayer.m_pxCollider->GetBodyID());
				g_bHomeExitInputMotionSeen |=
					glm::length(xPlayer.m_xPosition - g_xHomeExitDriveStart) > 0.2f
					&& std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
						- ZM_PlayerController::fRUN_SPEED) <= 0.001f
					&& glm::length(Zenith_Maths::Vector2(xVelocity.x, xVelocity.z)) > 1.0f;
				DriveTowardXZ(xPlayer.m_xPosition,
					Zenith_Maths::Vector3(0.0f, 0.0f, 5.2f));
			}
			else
			{
				ClearTraversalInput();
				if (xState.GetTransitionState() != ZM_WARP_TRANSITION_QUEUED
					|| !pxTrigger->IsLatched()
					|| xState.GetTargetBuildIndex() != 2u
					|| std::strcmp(xState.GetTargetSpawnTag(), "FromHome") != 0
					|| xState.GetIssuedLoadRequestCount() != 2u
					|| xState.GetFadeAlpha() >= 1.0f
					|| xState.GetFrozenPlayerEntityID() != xPlayer.m_xEntityID
					|| xPlayer.m_pxController->IsMovementEnabled())
				{
					FailPlayerHomeRoundTrip("real PlayerHome exit did not queue/freeze Dawnmere return");
					return false;
				}
				g_bHomeExitCollisionSeen = true;
				g_bHomeExitFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
				g_eHomePhase = PlayerHomeRoundTripPhase::WaitForDawnmereReturn;
				g_iHomePhaseFrames = 0;
				return true;
			}
			if (g_iHomePhaseFrames > 180)
			{
				FailPlayerHomeRoundTrip("input-driven PlayerHome exit approach timed out");
				return false;
			}
			return true;
		}

		case PlayerHomeRoundTripPhase::WaitForDawnmereReturn:
		{
			ManagerView xManager;
			if (!FindUniqueManager(xManager)
				|| xManager.m_xEntityID != g_xHomeManagerEntityID
				|| !FadeVisualMatchesManager(xManager))
			{
				FailPlayerHomeRoundTrip("manager/fade changed during Dawnmere return");
				return false;
			}
			ZM_GameStateManager& xState = *xManager.m_pxManager;
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const int iBuild = g_xEngine.Scenes().GetSceneInfo(xActive).m_iBuildIndex;
			if (iBuild == 40)
			{
				PlayerView xPlayer;
				if (!FindUniqueActivePlayer(xPlayer)
					|| xPlayer.m_pxController->IsMovementEnabled()
					|| xState.GetFrozenPlayerEntityID() != xPlayer.m_xEntityID)
				{
					FailPlayerHomeRoundTrip("PlayerHome player unlocked before Dawnmere SINGLE");
					return false;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_QUEUED)
				{
					if (xState.GetIssuedLoadRequestCount() != 2u
						|| xState.GetFadeAlpha() >= 1.0f)
					{
						FailPlayerHomeRoundTrip("Dawnmere return issued below opacity");
						return false;
					}
					g_bHomeExitFadeOutSeen |= xState.GetFadeAlpha() > 0.0f;
				}
				else if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_SCENE)
				{
					if (xState.GetIssuedLoadRequestCount() != 3u
						|| xState.GetFadeAlpha() != 1.0f)
					{
						FailPlayerHomeRoundTrip("Dawnmere return load was not exactly once at opacity");
						return false;
					}
					g_bHomeExitOpaqueLoadSeen = true;
				}
			}
			else if (iBuild == 2)
			{
				PlayerView xPlayer;
				CameraView xCamera;
				Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xActive);
				if (xState.GetIssuedLoadRequestCount() != 3u
					|| g_xPlayerHomeScene.IsValid()
					|| g_xEngine.Scenes().ResolveEntity(
						g_xPlayerHomePlayerEntityID).IsValid()
					|| g_xEngine.Scenes().ResolveEntity(
						g_xPlayerHomeCameraEntityID).IsValid()
					|| g_xEngine.Scenes().ResolveEntity(g_xHomeExitTriggerEntityID).IsValid())
				{
					FailPlayerHomeRoundTrip("PlayerHome did not teardown on Dawnmere SINGLE");
					return false;
				}
				g_bHomeExitOpaqueLoadSeen |= xState.GetFadeAlpha() == 1.0f;
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_WAITING_FOR_CAMERA
					&& FindUniqueActivePlayer(xPlayer) && FindUniqueActiveCamera(xCamera)
					&& pxData != nullptr
					&& pxData->GetMainCameraEntity() == xCamera.m_xEntityID
					&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
					&& !xPlayer.m_pxController->IsMovementEnabled()
					&& xState.GetFadeAlpha() == 1.0f)
				{
					g_bDawnmereReturnCameraBarrierSeen = true;
					Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
					Zenith_Entity xSpawn;
					Zenith_Maths::Vector3 xFeet(0.0f);
					if (ZM_SpawnPoint::FindUniqueInScene(xActive, "FromHome", xSpawnID)
							== ZM_SPAWN_POINT_LOOKUP_FOUND
						&& (xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID)).IsValid())
					{
						xSpawn.GetComponent<Zenith_TransformComponent>().GetPosition(xFeet);
						const Zenith_Maths::Vector3 xExpected =
							ZM_GameStateManager::CalculateSpawnCenter(xFeet, xPlayer.m_xScale);
						const Zenith_PhysicsBodyID xBody = xPlayer.m_pxCollider->GetBodyID();
						g_bDawnmereReturnPlacementResetSeen =
							glm::length(xPlayer.m_xPosition - xExpected) <= fPOSITION_EPSILON
							&& glm::length(g_xEngine.Physics().GetBodyPosition(xBody) - xExpected)
								<= fPOSITION_EPSILON
							&& glm::length(g_xEngine.Physics().GetLinearVelocity(xBody))
								<= fVELOCITY_EPSILON
							&& glm::length(g_xEngine.Physics().GetAngularVelocity(xBody))
								<= fVELOCITY_EPSILON
							&& xPlayer.m_pxController->GetRequestedSpeed() == 0.0f
							&& glm::length(xPlayer.m_pxController->GetMoveDirection())
								<= fVELOCITY_EPSILON;
					}
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_FADING_IN)
				{
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| xPlayer.m_pxController->IsMovementEnabled())
					{
						FailPlayerHomeRoundTrip("Dawnmere return fade-in bypassed camera/input lock");
						return false;
					}
					g_bDawnmereReturnFadeInSeen |= xState.GetFadeAlpha() < 1.0f;
				}
				if (xState.GetTransitionState() == ZM_WARP_TRANSITION_IDLE)
				{
					Zenith_EntityID xSpawnID = INVALID_ENTITY_ID;
					Zenith_Entity xSpawn;
					Zenith_Entity xDoor;
					ZM_WarpTrigger* pxDoor = nullptr;
					Zenith_Maths::Vector3 xFeet(0.0f);
					if (!FindUniqueActivePlayer(xPlayer) || !FindUniqueActiveCamera(xCamera)
						|| pxData == nullptr
						|| ZM_SpawnPoint::FindUniqueInScene(xActive, "FromHome", xSpawnID)
							!= ZM_SPAWN_POINT_LOOKUP_FOUND
						|| !(xSpawn = g_xEngine.Scenes().ResolveEntity(xSpawnID)).IsValid()
						|| !FindConfiguredActiveTrigger(
							"HomeDoorTrigger", 40u, "Door", xDoor, pxDoor)
						|| !xPlayer.m_pxController->IsMovementEnabled()
						|| pxData->GetMainCameraEntity() != xCamera.m_xEntityID
						|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
						|| xState.GetFadeAlpha() != 0.0f
						|| xState.GetTargetBuildIndex()
							!= ZM_GameStateManager::uINVALID_BUILD_INDEX
						|| !ManagerFadeOverlayIsHidden(xManager)
						|| !ActiveGrassIsReadyAndUnique())
					{
						if (g_iHomePhaseFrames > 420)
						{
							FailPlayerHomeRoundTrip("returned Dawnmere authored state did not settle");
							return false;
						}
						return true;
					}
					xSpawn.GetComponent<Zenith_TransformComponent>().GetPosition(xFeet);
					const Zenith_Maths::Vector3 xExpected =
						ZM_GameStateManager::CalculateSpawnCenter(xFeet, xPlayer.m_xScale);
					const Zenith_PhysicsBodyID xBody = xPlayer.m_pxCollider->GetBodyID();
					const Zenith_Maths::Vector3 xPlayerDelta =
						xPlayer.m_xPosition - xExpected;
					const Zenith_Maths::Vector3 xBodyDelta =
						g_xEngine.Physics().GetBodyPosition(xBody) - xExpected;
					constexpr float fCONTACT_SETTLE_EPSILON = 0.05f;
					const bool bPlayerContactSettled =
						std::fabs(xPlayerDelta.x) <= fPOSITION_EPSILON
						&& std::fabs(xPlayerDelta.z) <= fPOSITION_EPSILON
						&& xPlayerDelta.y <= fPOSITION_EPSILON
						&& xPlayerDelta.y >= -fCONTACT_SETTLE_EPSILON;
					const bool bBodyContactSettled =
						std::fabs(xBodyDelta.x) <= fPOSITION_EPSILON
						&& std::fabs(xBodyDelta.z) <= fPOSITION_EPSILON
						&& xBodyDelta.y <= fPOSITION_EPSILON
						&& xBodyDelta.y >= -fCONTACT_SETTLE_EPSILON;
					if (!g_bHomeExitInputMotionSeen || !g_bHomeExitCollisionSeen
						|| !g_bHomeExitFadeOutSeen || !g_bHomeExitOpaqueLoadSeen
						|| !g_bDawnmereReturnCameraBarrierSeen
						|| !g_bDawnmereReturnPlacementResetSeen
						|| !g_bDawnmereReturnFadeInSeen
						|| xPlayer.m_xEntityID == g_xHomeInitialDawnmerePlayerEntityID
						|| xPlayer.m_xEntityID == g_xPlayerHomePlayerEntityID
						|| xCamera.m_xEntityID == g_xHomeInitialDawnmereCameraEntityID
						|| xCamera.m_xEntityID == g_xPlayerHomeCameraEntityID
						|| g_xEngine.Scenes().ResolveEntity(xPlayer.m_xEntityID).GetScene()
							!= xActive
						|| g_xEngine.Scenes().ResolveEntity(xCamera.m_xEntityID).GetScene()
							!= xActive
						|| glm::length(xFeet - Zenith_Maths::Vector3(
							384.0f, 26.590313f, 482.0f)) > fPOSITION_EPSILON
						|| !bPlayerContactSettled || !bBodyContactSettled
						|| glm::length(g_xEngine.Physics().GetLinearVelocity(xBody))
							> fVELOCITY_EPSILON
						|| glm::length(g_xEngine.Physics().GetAngularVelocity(xBody))
							> fVELOCITY_EPSILON
						|| xPlayer.m_pxController->GetRequestedSpeed() != 0.0f
						|| glm::length(xPlayer.m_pxController->GetMoveDirection())
							> fVELOCITY_EPSILON)
					{
						FailPlayerHomeRoundTrip("FromHome placement/motion or return proof failed");
						return false;
					}
					ClearTraversalInput();
					g_bHomePassed = true;
					g_eHomePhase = PlayerHomeRoundTripPhase::Done;
					return false;
				}
			}
			else
			{
				FailPlayerHomeRoundTrip("unexpected build index during Dawnmere return");
				return false;
			}
			if (g_iHomePhaseFrames > 420)
			{
				FailPlayerHomeRoundTrip("Dawnmere return transition timed out");
				return false;
			}
			return true;
		}
		case PlayerHomeRoundTripPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_PlayerHomeRoundTrip()
	{
		ClearTraversalInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		if (g_bHomePrerequisitesPresent)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}
		if (!g_bHomePassed && g_bHomePrerequisitesPresent)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_PlayerHomeRoundTrip] %s", g_szHomeFailure);
		}
		return g_bHomePassed;
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

static const Zenith_AutomatedTest g_xZMPlayerHomeRoundTripTest = {
	"ZM_PlayerHomeRoundTrip_Test",
	&Setup_PlayerHomeRoundTrip,
	&Step_PlayerHomeRoundTrip,
	&Verify_PlayerHomeRoundTrip,
	/* maxFrames */ 1800,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMPlayerHomeRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
