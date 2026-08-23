#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"       // Zenith_GetMainCameraAcrossScenes -- the walk's live basis
#include "Flux/Vegetation/Flux_GrassImpl.h"
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
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"   // ZM-D-173: the shared Home approach route; R1-3: the north seam gate
#include "Zenithmon/Source/World/ZM_Route1Placement.h"     // R1-3: Route 1's two gate entity names + the gate RESOLVERS
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h"  // R1-3: Thornacre's return gate name + its return RESOLVERS

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
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_A);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_D);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_UP);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_DOWN);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_RIGHT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_RIGHT_SHIFT);
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
				// A Null (headless) build never APPLIES grass -- the blades are
				// GPU-only content the backend deliberately does not author (see
				// Zenith/Null/CLAUDE.md) -- so applied-ness / scheduled work are
				// not available there. The CPU-side half (density map loaded, no
				// terminal failure) IS, and stays asserted in every config; the
				// windowed assertion is unchanged.
				//
				// The scheduled count comes from the ENGINE, not the component: the
				// component samples it at apply time, before that build's first
				// gather has run, so its snapshot is legitimately zero.
				const bool bGPUHalf = Zenith_IsNullRenderer()
					|| (xGrass.IsGrassApplied()
						&& g_xEngine.Grass().GetScheduledInstanceCount() > 0u);
				bReady = xGrass.HasCPUMap() && bGPUHalf
					&& !xGrass.HasTerminalFailure();
			});
		return uCount == 1u && bReady;
	}

	// ★★ THE TRAVERSAL DRIVE IS CAMERA-RELATIVE, and it MUST be. ★★
	//
	// Player movement is camera-relative: ZM_PlayerController::OnUpdate reads the main
	// camera's facing dir and hands it to BuildCameraRelativeDirection, which builds
	// xForward = flatten(cameraForward), xRight = (xForward.z, 0, -xForward.x) and moves
	// along xForward * input.y + xRight * input.x (ZM_PlayerController.cpp:140-147,
	// 244-271). So "W" does NOT mean +Z -- it means "the way the camera is facing".
	//
	// ZM_FollowCamera is a SPRING follow: it places the camera from the fixed authored
	// yaw but then AIMS it at the player with fYaw = atan2(-direction.x, direction.z)
	// over the LAGGING camera-to-player vector (ZM_FollowCamera.cpp:309-323). So the
	// camera's facing swings as the player turns, and the world-space meaning of
	// W/A/S/D rotates with it.
	//
	// The older world-space version of this function picked keys by comparing world
	// dx/dz directly. That is correct ONLY while the camera is still near its authored
	// yaw -- i.e. for a SINGLE leg walked from rest.
	//
	// THIS FILE'S WALKS ARE ALL IN THAT SAFE REGIME, so the change here is NOT a
	// measured bug fix. Stated precisely, because the temptation is to overclaim:
	//   * ZM_PlayerHomeRoundTrip_Test's outbound leg starts from the TownCenter spawn
	//     (512, ~25.99, 480 -- Zenithmon.cpp) and drives 128 m straight along -X to the
	//     staging point (384, 0, 480), then turns ~90 degrees for the final 4 m onto
	//     (384, 0, 476).
	//   * The "back" leg is NOT a continuation of that heading: it happens after the warp
	//     into build 40, from the "Door" spawn, in a DIFFERENT scene with a DIFFERENT
	//     camera entity, driving to (0, 0, 5.2). It is another single leg from rest.
	//   * Nothing here measured a margin, so do NOT claim this test was "passing on
	//     tolerance", and do NOT claim a ~180-degree reversal -- neither is true.
	// The camera-relative version is adopted because it is correct IN GENERAL, not
	// because a failure was observed in this file.
	//
	// A failure WAS measured elsewhere this session (ZM_AutoTests_NpcServices.cpp), which
	// is what makes the general correctness load-bearing: on a leg needing a ~120-degree
	// heading change the world-space chooser settled onto a stable 45-degree WRONG heading
	// at full run speed, receded from 9.25 m to 12.34 m and died on a stall watchdog. Not
	// physics, not an obstruction, not a frame budget -- the keys were simply being chosen
	// in the wrong frame.
	//
	// The fix is the exact INVERSE of the controller's mapping: project the desired
	// world direction onto the LIVE camera basis and choose keys from those components.
	// At the authored yaw of 0 this degenerates to the old behaviour EXACTLY
	// (forward == +Z, right == +X), so the already-green single-leg walks are unchanged.
	void DriveTowardXZ(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget)
	{
		ClearTraversalInput();
		constexpr float fDEAD_ZONE = 0.08f;

		// The LIVE camera basis, resolved exactly as the controller resolves it. The
		// fallback matches BuildCameraRelativeDirection's own: +Z forward.
		Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
		{
			pxCamera->GetFacingDir(xCameraForward);
		}
		Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
		const float fForwardLengthSq = xForward.x * xForward.x + xForward.z * xForward.z;
		if (fForwardLengthSq <= 0.000001f)
		{
			xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		}
		else
		{
			xForward /= std::sqrt(fForwardLengthSq);
		}
		const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);

		// The desired WORLD direction, decomposed onto that basis. fForwardAmount is the
		// W/S axis and fRightAmount the D/A axis -- the same two components the
		// controller will multiply back out.
		const Zenith_Maths::Vector3 xToTarget(
			xTarget.x - xPosition.x, 0.0f, xTarget.z - xPosition.z);
		const float fForwardAmount = xToTarget.x * xForward.x + xToTarget.z * xForward.z;
		const float fRightAmount   = xToTarget.x * xRight.x   + xToTarget.z * xRight.z;

		const float fDeltaX = fRightAmount;
		const float fDeltaZ = fForwardAmount;
		if (fDeltaX < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_A);
		}
		else if (fDeltaX > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_D);
		}
		if (fDeltaZ < -fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_S);
		}
		else if (fDeltaZ > fDEAD_ZONE)
		{
			Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		}
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_LEFT_SHIFT);
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
			const float fCapsuleHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
			const Zenith_Maths::Vector3 xExpectedCenter =
				ZM_GameStateManager::CalculateSpawnCenter(xMarkerFeet);
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
				&& std::fabs(xMarkerFeet.y - 25.99055f) <= fPOSITION_EPSILON
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
						ZM_GameStateManager::CalculateSpawnCenter(xFeet);
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
				// ZM-D-173: both waypoints come from the shared placement block in
				// Source/World/ZM_DawnmerePlacement.h, so the Home cannot move
				// without this route moving with it -- DriveTowardXZ has NO obstacle
				// avoidance, and a route left pointing at the old doorway does not
				// fail gracefully, it wedges the capsule against the relocated shell
				// and times out reporting a distance.
				//
				// The route: first ALIGN with the doorway from the south, staying
				// clear of the shell's solid entrance face while the capsule is
				// still outside it; then make a short +Z move into the authored
				// sensor so the trigger is the first thing contacted. The
				// staging-clear property (that the alignment leg never crosses the
				// player-radius-expanded shell) is a boot unit --
				// ZM_Interaction/HomeApproachIsClearOfTheDriveCorridor -- rather
				// than a claim this comment makes.
				const Zenith_Maths::Vector3 xDoorStaging =
					ZM_GetDawnmereHomeDoorStagingXZ();
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
						ZM_GetDawnmereHomeDoorTargetXZ());
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
						ZM_GameStateManager::CalculateSpawnCenter(xFeet);
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
							ZM_GameStateManager::CalculateSpawnCenter(xFeet);
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
						ZM_GameStateManager::CalculateSpawnCenter(xFeet);
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
						// ZM-D-173 moved this marker with the Home. The value is a
						// LITERAL on purpose: this clause reads the COMMITTED scene
						// bytes, so sharing ZM_GetDawnmereFromHomeSpawnFeet() here
						// would compare the authoring constants against themselves
						// and stop proving that the scene was re-authored at all.
						// Observed on the divisor-4 collision mesh, 2026-08-02.
						|| glm::length(xFeet - Zenith_Maths::Vector3(
							384.0f, 26.542120f, 468.0f)) > fPOSITION_EPSILON
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

	// ========================================================================
	// R1-3 -- ZM_SeamRoundTrip_Test: ALL FOUR SEAM GATES, BOTH DIRECTIONS.
	//
	// ★★ WHAT IT PROVES THAT NOTHING ELSE CAN. Every pure unit about this seam
	// reads the same compiled constants the AUTHORING writes from, so both sides
	// of every claim move together; and the committed-bytes needles prove a
	// payload is IN a file, never that walking into the box does anything. This
	// test drives the real player with real input into each of the four sensors
	// and asserts WHERE IT COMES OUT. That is the only check in the game that can
	// fail on the mutation the whole slice is sequenced around: a gate whose
	// destination validates against the compiled table and then stalls, or a pair
	// of gates authored against each other's destinations.
	//
	// ★★ AND IT PROVES BOTH DIRECTIONS, WHICH IS THE POINT OF LANDING FOUR
	// TRIGGERS IN ONE COMMIT. Legs 0 and 1 are Dawnmere -> Route 1 and Route 1 ->
	// Dawnmere; legs 2 and 3 are Route 1 -> Thornacre and Thornacre -> Route 1. A
	// one-way seam -- a gate whose far side has no return -- is a world the player
	// can walk into and not out of, and it is invisible to any test that only ever
	// travels outbound.
	//
	// ★★ IT DOES NOT WALK THE SPINE, AND MUST NOT. Route 1's DirtLane is ~1408 m,
	// which is ~6,000 frames one way and ~12-14k for a round trip against this
	// harness's frame caps. Each leg therefore WARPS to the arrival marker it
	// starts from and walks only the last few metres into that leg's gate volume
	// -- the gates sit 12 m beyond their own markers by construction, so the
	// walked distance is ~12 m per leg. fSEAM_MAX_APPROACH_M is asserted BEFORE
	// the walk begins so a gate that ever moves far from its marker fails with a
	// message that says so, instead of timing out with one that does not.
	//
	// ★ EVERY DESTINATION IS RESOLVED FROM THE COMPILED WORLD TABLE, through the
	// SAME accessors the authoring calls. Nothing here spells a build index or a
	// spawn tag, so a re-pointed edge in Source/Data/ZM_WorldSpec.cpp moves the
	// authoring and this test together -- and, critically, this test still fails
	// if the AUTHORING did not move with it, because it asserts against the LIVE
	// scene rather than against the constants.
	//
	// ★ IT SKIPS WITHOUT THE THREE TERRAIN BAKES, which are GITIGNORED, so it is
	// a PASS on CI. That is why Tests/ZM_Tests_CommittedSceneBytes.cpp carries the
	// boot-level payload needles: a skip counts as a pass, and the gate needs
	// something that runs with no GPU and no bake.
	// ========================================================================

	// The furthest a leg may have to walk. Every gate is authored 12 m from its
	// own arrival marker, and the boxes are 6 m deep, so ~9 m of open ground is
	// the real figure. 40 m is a generous ceiling that still fails FAST -- and
	// with a message that names the cause -- if a gate is ever moved away from
	// the marker it guards, rather than letting the walk deadline expire on a
	// diagnostic about frames.
	constexpr float fSEAM_MAX_APPROACH_M = 40.0f;

	// How close the arriving body must be to the destination marker's computed
	// spawn centre. Deliberately LOOSE (metres, not millimetres): the claim this
	// test makes is "the player came out at the RIGHT MARKER", and the nearest
	// wrong answer is 1312 m away (Route 1's two arrival markers). A tight
	// epsilon here would instead be a statement about contact settle on
	// provisionally-frozen terrain, which is ZM_*GroundTruth_Test's business.
	constexpr float fSEAM_ARRIVAL_RADIUS_M = 2.0f;

	// A leg must actually MOVE the player before its gate fires. Without this a
	// warp triggered by anything other than the walk -- or a gate the player is
	// already standing inside, i.e. the arrival-clearance defect that would
	// ping-pong two scenes forever -- would read as a pass.
	constexpr float fSEAM_MIN_WALKED_M = 1.0f;

	constexpr int iSEAM_TRANSITION_DEADLINE_FRAMES = 420;
	constexpr int iSEAM_WALK_DEADLINE_FRAMES = 600;
	constexpr u_int uSEAM_LEG_COUNT = 4u;

	// One crossing: warp to m_szStageTag in m_uFromBuildIndex, walk into
	// m_szGateEntityName, and come out at m_szGateSpawnTag in m_uToBuildIndex.
	//
	// ★ THE LAST THREE FIELDS ARE RESOLVED AT SETUP, NOT SPELLED. m_uToBuildIndex
	// and m_szGateSpawnTag are what the gate's OWN region asks its destination
	// for; the accessors that answer them are the ones the configure steps in
	// Zenithmon.cpp call, so this table cannot describe a gate the authoring did
	// not build.
	struct SeamLeg
	{
		const char* m_szLabel = "";
		u_int m_uFromBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
		const char* m_szStageTag = "";
		const char* m_szGateEntityName = "";
		u_int m_uToBuildIndex = ZM_GameStateManager::uINVALID_BUILD_INDEX;
		const char* m_szGateSpawnTag = "";
	};

	enum class SeamPhase
	{
		Bootstrap,
		StageWarp,
		AwaitStage,
		Approach,
		AwaitLand,
		Done,
	};

	SeamLeg g_axSeamLegs[uSEAM_LEG_COUNT];
	SeamPhase g_eSeamPhase = SeamPhase::Done;
	u_int g_uSeamLeg = 0u;
	int g_iSeamPhaseFrames = 0;
	bool g_bSeamPrerequisitesPresent = false;
	bool g_bSeamPassed = false;
	u_int g_uSeamCrossingsProven = 0u;
	const char* g_szSeamFailure = "test did not reach verification";
	Zenith_Maths::Vector3 g_xSeamApproachStart(0.0f);
	Zenith_Maths::Vector3 g_xSeamGateXZ(0.0f);

	void FailSeamRoundTrip(const char* szReason)
	{
		g_szSeamFailure = szReason;
		g_bSeamPassed = false;
		g_eSeamPhase = SeamPhase::Done;
		ClearTraversalInput();
	}

	// The three region scenes plus their three terrain bakes. Route 1's and
	// Thornacre's bakes are GITIGNORED, so this is what turns a CI run into a
	// SKIP rather than a spurious failure -- and it is checked BEFORE any state
	// is installed, because RequestSkip bypasses Verify.
	bool SeamRegionAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 11> astrRequired = {
			strRoot + "Scenes/FrontEnd" ZENITH_SCENE_EXT,
			strRoot + "Scenes/Dawnmere" ZENITH_SCENE_EXT,
			strRoot + "Scenes/Route1" ZENITH_SCENE_EXT,
			strRoot + "Scenes/Thornacre" ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Route1/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Route1/GrassDensity" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Route1/Physics_0_0" ZENITH_MESH_EXT,
			strRoot + "Terrain/Thornacre/Height" ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Thornacre/Physics_0_0" ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			if (std::filesystem::file_size(strPath, xError) == 0u || xError)
			{
				return false;
			}
		}
		return true;
	}

	// True once the warp machine is idle, the named scene is active, and the
	// player it placed is resolvable -- i.e. the transition genuinely COMPLETED
	// at the tag it was asked for, rather than merely stopping.
	bool SeamArrivedAt(u_int uBuildIndex, const char* szSpawnTag, PlayerView& xPlayerOut)
	{
		if (ZM_GameStateManager::IsWarpInProgress())
		{
			return false;
		}
		const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (!xScene.IsValid()
			|| (u_int)g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex
				!= uBuildIndex)
		{
			return false;
		}
		const char* szArrived = ZM_GameStateManager::GetActiveSceneArrivedSpawnTag();
		if (szArrived == nullptr || std::strcmp(szArrived, szSpawnTag) != 0)
		{
			return false;
		}
		return FindUniqueActivePlayer(xPlayerOut) && xPlayerOut.m_pxCollider != nullptr;
	}

	// Where the marker carrying szSpawnTag says an arriving body belongs. FALSE
	// when the active scene has no unique marker with that tag -- which is the
	// WAITING_FOR_SPAWN stall's own precondition, and worth naming separately
	// from "the player is in the wrong place".
	bool SeamExpectedArrivalCentre(
		const char* szSpawnTag, Zenith_Maths::Vector3& xCentreOut)
	{
		Zenith_EntityID xMarkerID = INVALID_ENTITY_ID;
		if (ZM_SpawnPoint::FindUniqueInScene(
				g_xEngine.Scenes().GetActiveScene(), szSpawnTag, xMarkerID)
			!= ZM_SPAWN_POINT_LOOKUP_FOUND)
		{
			return false;
		}
		Zenith_Entity xMarker = g_xEngine.Scenes().ResolveEntity(xMarkerID);
		if (!xMarker.IsValid())
		{
			return false;
		}
		Zenith_Maths::Vector3 xFeet(0.0f);
		xMarker.GetComponent<Zenith_TransformComponent>().GetPosition(xFeet);
		xCentreOut = ZM_GameStateManager::CalculateSpawnCenter(xFeet);
		return true;
	}

	void Setup_SeamRoundTrip()
	{
		g_eSeamPhase = SeamPhase::Done;
		g_uSeamLeg = 0u;
		g_iSeamPhaseFrames = 0;
		g_bSeamPassed = false;
		g_uSeamCrossingsProven = 0u;
		g_szSeamFailure = "test did not reach verification";
		g_xSeamApproachStart = Zenith_Maths::Vector3(0.0f);
		g_xSeamGateXZ = Zenith_Maths::Vector3(0.0f);
		Zenith_InputSimulator::ResetAllInputState();

		g_bSeamPrerequisitesPresent = SeamRegionAssetsPresent();
		if (!g_bSeamPrerequisitesPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"[ZM_SeamRoundTrip] one of FrontEnd/Dawnmere/Route1/Thornacre.zscen or "
				"the three terrain bakes is absent -- there is no world to walk the "
				"seams in (run a *_True config once to bake the terrain)");
			return;
		}

		// The four crossings, in the order they are driven. Legs 0/1 are the
		// Dawnmere seam out and back; legs 2/3 are the Thornacre seam out and
		// back. Every build index and every tag below is RESOLVED, not spelled.
		const u_int uDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex;
		const u_int uRoute1 = ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex;
		const u_int uThornacre = ZM_GetWorldSpec(ZM_SCENE_THORNACRE).m_uBuildIndex;

		g_axSeamLegs[0] = {
			"Dawnmere -> Route 1 (north gate)",
			uDawnmere,
			// Staged at the marker Route 1's own south gate lands on, so the leg
			// starts exactly where leg 1 will finish -- the two together are the
			// round trip.
			ZM_GetRoute1SouthGateSpawnTag(),
			szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME,
			ZM_GetDawnmereNorthGateTargetBuildIndex(),
			ZM_GetDawnmereNorthGateSpawnTag() };
		g_axSeamLegs[1] = {
			"Route 1 -> Dawnmere (south gate)",
			uRoute1,
			ZM_GetDawnmereNorthGateSpawnTag(),
			szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME,
			ZM_GetRoute1SouthGateTargetBuildIndex(),
			ZM_GetRoute1SouthGateSpawnTag() };
		g_axSeamLegs[2] = {
			"Route 1 -> Thornacre (north gate)",
			uRoute1,
			ZM_GetThornacreReturnSpawnTag(),
			szZM_ROUTE1_NORTH_GATE_ENTITY_NAME,
			ZM_GetRoute1NorthGateTargetBuildIndex(),
			ZM_GetRoute1NorthGateSpawnTag() };
		g_axSeamLegs[3] = {
			"Thornacre -> Route 1 (return gate)",
			uThornacre,
			ZM_GetRoute1NorthGateSpawnTag(),
			szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME,
			ZM_GetThornacreReturnTargetBuildIndex(),
			ZM_GetThornacreReturnSpawnTag() };

		// ★ ANTI-VACUITY. A leg whose destination did not resolve would drive the
		// player into a gate and then wait for a build index no scene holds, and
		// the failure would be a deadline rather than "the compiled table has no
		// such edge". Say it here instead.
		for (u_int uLeg = 0u; uLeg < uSEAM_LEG_COUNT; ++uLeg)
		{
			const SeamLeg& xLeg = g_axSeamLegs[uLeg];
			if (xLeg.m_uFromBuildIndex == ZM_GameStateManager::uINVALID_BUILD_INDEX
				|| xLeg.m_uToBuildIndex == ZM_GameStateManager::uINVALID_BUILD_INDEX
				|| xLeg.m_szStageTag == nullptr || xLeg.m_szStageTag[0] == '\0'
				|| xLeg.m_szGateSpawnTag == nullptr
				|| xLeg.m_szGateSpawnTag[0] == '\0')
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_SeamRoundTrip] leg %u ('%s') did not resolve from the compiled "
					"world table (from=%u to=%u stageTag='%s' gateTag='%s') -- fix "
					"Source/Data/ZM_WorldSpec.cpp before reading anything into this run",
					uLeg, xLeg.m_szLabel, xLeg.m_uFromBuildIndex, xLeg.m_uToBuildIndex,
					xLeg.m_szStageTag != nullptr ? xLeg.m_szStageTag : "<null>",
					xLeg.m_szGateSpawnTag != nullptr ? xLeg.m_szGateSpawnTag : "<null>");
				g_szSeamFailure = "a seam leg did not resolve from the compiled world table";
				return;
			}
		}

		Zenith_InputSimulator::SetFixedDt(fROUND_TRIP_FIXED_DT);
		g_eSeamPhase = SeamPhase::Bootstrap;
	}

	bool Step_SeamRoundTrip(int)
	{
		if (!g_bSeamPrerequisitesPresent || g_eSeamPhase == SeamPhase::Done)
		{
			return false;
		}
		++g_iSeamPhaseFrames;

		const SeamLeg& xLeg = g_axSeamLegs[g_uSeamLeg];
		switch (g_eSeamPhase)
		{
		case SeamPhase::Bootstrap:
		{
			ManagerView xManager;
			const Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
			if (!xScene.IsValid()
				|| g_xEngine.Scenes().GetSceneInfo(xScene).m_iBuildIndex != 0
				|| !FindUniqueManager(xManager)
				|| xManager.m_pxManager->GetTransitionState()
					!= ZM_WARP_TRANSITION_IDLE)
			{
				if (g_iSeamPhaseFrames > 180)
				{
					FailSeamRoundTrip(
						"FrontEnd/manager bootstrap did not settle in 180 frames");
					return false;
				}
				return true;
			}
			g_eSeamPhase = SeamPhase::StageWarp;
			g_iSeamPhaseFrames = 0;
			return true;
		}

		case SeamPhase::StageWarp:
		{
			// ★ STAGING IS A WARP, NOT A WALK, AND THAT IS THE FRAME BUDGET
			// DECISION THIS TEST TURNS ON -- see the banner.
			ClearTraversalInput();

			// ★★ ...EXCEPT WHERE THE PREVIOUS LEG ALREADY LANDED HERE, which is
			// the case for legs 1 and 3 by construction: leg 0's gate puts the
			// player at Route 1's south marker, which is exactly where leg 1
			// starts, and leg 2's puts them at Thornacre's, where leg 3 starts.
			// Skipping the redundant warp is not just tidiness -- it makes each
			// PAIR a genuinely continuous round trip (walk out, walk back)
			// rather than two independent crossings that happen to be adjacent,
			// and it avoids asking the warp machine to warp into the scene it is
			// already in, which is a path nothing else in this game exercises.
			PlayerView xStagedPlayer;
			if (SeamArrivedAt(
					xLeg.m_uFromBuildIndex, xLeg.m_szStageTag, xStagedPlayer))
			{
				g_eSeamPhase = SeamPhase::AwaitStage;
				g_iSeamPhaseFrames = 0;
				return true;
			}

			if (!ZM_GameStateManager::RequestWarp(
					xLeg.m_uFromBuildIndex, xLeg.m_szStageTag))
			{
				FailSeamRoundTrip(
					"a leg's staging warp was REFUSED -- the compiled table does not "
					"consider the leg's own start point a valid destination");
				return false;
			}
			g_eSeamPhase = SeamPhase::AwaitStage;
			g_iSeamPhaseFrames = 0;
			return true;
		}

		case SeamPhase::AwaitStage:
		{
			PlayerView xPlayer;
			if (!SeamArrivedAt(xLeg.m_uFromBuildIndex, xLeg.m_szStageTag, xPlayer))
			{
				if (g_iSeamPhaseFrames > iSEAM_TRANSITION_DEADLINE_FRAMES)
				{
					FailSeamRoundTrip(
						"a leg's staging warp never completed at its own arrival tag -- "
						"the warp machine is stalled (WAITING_FOR_SCENE / "
						"WAITING_FOR_SPAWN, ZM-D-200) or the marker is missing");
					return false;
				}
				return true;
			}

			// The gate, resolved in the LIVE scene and checked against what the
			// compiled table says it should carry. This is the swap detector:
			// FindConfiguredActiveTrigger fails if this NAMED entity's trigger
			// holds a different target build index or spawn tag.
			Zenith_Entity xGate;
			ZM_WarpTrigger* pxTrigger = nullptr;
			if (!FindConfiguredActiveTrigger(xLeg.m_szGateEntityName,
					xLeg.m_uToBuildIndex, xLeg.m_szGateSpawnTag, xGate, pxTrigger))
			{
				FailSeamRoundTrip(
					"a seam gate is missing from its scene, has no static AABB sensor "
					"body, or is configured against a DIFFERENT destination than the "
					"compiled world table gives it (the south/north swap)");
				return false;
			}

			Zenith_Maths::Vector3 xGatePos(0.0f);
			xGate.GetComponent<Zenith_TransformComponent>().GetPosition(xGatePos);
			g_xSeamGateXZ = Zenith_Maths::Vector3(xGatePos.x, 0.0f, xGatePos.z);
			g_xSeamApproachStart = xPlayer.m_xPosition;

			const float fApproach = glm::length(Zenith_Maths::Vector3(
				xGatePos.x - xPlayer.m_xPosition.x, 0.0f,
				xGatePos.z - xPlayer.m_xPosition.z));
			if (!(fApproach > fSEAM_MIN_WALKED_M)
				|| !(fApproach < fSEAM_MAX_APPROACH_M))
			{
				// Both arms matter. Too FAR is the frame-budget failure the
				// banner is about; too NEAR means the arriving body is already
				// on top of its own gate, which is the infinite two-scene
				// ping-pong the arrival-clearance floors exist to forbid.
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_SeamRoundTrip] leg %u ('%s'): the walk from the arrival to "
					"gate '%s' is %.3f m, outside (%.1f, %.1f). Too far is a walk this "
					"harness cannot afford; too near means an arriving player is "
					"standing in their own exit and will be warped straight back.",
					g_uSeamLeg, xLeg.m_szLabel, xLeg.m_szGateEntityName,
					(double)fApproach, (double)fSEAM_MIN_WALKED_M,
					(double)fSEAM_MAX_APPROACH_M);
				FailSeamRoundTrip("a seam gate is not a short walk from its own marker");
				return false;
			}

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_SeamRoundTrip] leg %u ('%s'): staged in build %u at '%s'; walking "
				"%.3f m into '%s' -> build %u at '%s'",
				g_uSeamLeg, xLeg.m_szLabel, xLeg.m_uFromBuildIndex, xLeg.m_szStageTag,
				(double)fApproach, xLeg.m_szGateEntityName, xLeg.m_uToBuildIndex,
				xLeg.m_szGateSpawnTag);

			g_eSeamPhase = SeamPhase::Approach;
			g_iSeamPhaseFrames = 0;
			return true;
		}

		case SeamPhase::Approach:
		{
			if (ZM_GameStateManager::IsWarpInProgress())
			{
				// The sensor fired. Prove the PLAYER got there under its own
				// power before accepting it.
				PlayerView xPlayer;
				const bool bHavePlayer = FindUniqueActivePlayer(xPlayer);
				const float fWalked = bHavePlayer
					? glm::length(Zenith_Maths::Vector3(
						xPlayer.m_xPosition.x - g_xSeamApproachStart.x, 0.0f,
						xPlayer.m_xPosition.z - g_xSeamApproachStart.z))
					: 0.0f;
				if (fWalked < fSEAM_MIN_WALKED_M)
				{
					FailSeamRoundTrip(
						"a seam gate fired without the player having walked into it -- "
						"either the warp came from somewhere else, or the arriving body "
						"was already overlapping the sensor");
					return false;
				}
				ClearTraversalInput();
				g_eSeamPhase = SeamPhase::AwaitLand;
				g_iSeamPhaseFrames = 0;
				return true;
			}

			PlayerView xPlayer;
			if (!FindUniqueActivePlayer(xPlayer))
			{
				FailSeamRoundTrip(
					"the unique active-scene player vanished during a seam approach");
				return false;
			}
			DriveTowardXZ(xPlayer.m_xPosition, g_xSeamGateXZ);

			if (g_iSeamPhaseFrames > iSEAM_WALK_DEADLINE_FRAMES)
			{
				FailSeamRoundTrip(
					"a seam gate never fired while the player walked into it -- the "
					"sensor is not on the ground under the walked line, is not a "
					"sensor, or is too shallow for the physics tick to catch");
				return false;
			}
			return true;
		}

		case SeamPhase::AwaitLand:
		{
			PlayerView xPlayer;
			if (!SeamArrivedAt(xLeg.m_uToBuildIndex, xLeg.m_szGateSpawnTag, xPlayer))
			{
				if (g_iSeamPhaseFrames > iSEAM_TRANSITION_DEADLINE_FRAMES)
				{
					FailSeamRoundTrip(
						"a seam crossing never completed at the destination its gate "
						"asked for -- this is the stall the whole slice is sequenced "
						"around (ZM-D-200): the compiled table ACCEPTED the warp and the "
						"destination scene had no such marker");
					return false;
				}
				return true;
			}

			// ...and the player came out ON the marker the gate named, not merely
			// somewhere in the right scene.
			Zenith_Maths::Vector3 xExpected(0.0f);
			if (!SeamExpectedArrivalCentre(xLeg.m_szGateSpawnTag, xExpected))
			{
				FailSeamRoundTrip(
					"the destination scene has no unique ZM_SpawnPoint carrying the tag "
					"the gate asked for, yet the transition reported completion");
				return false;
			}
			const float fOffset = glm::length(xPlayer.m_xPosition - xExpected);
			if (!(fOffset <= fSEAM_ARRIVAL_RADIUS_M))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_SeamRoundTrip] leg %u ('%s'): landed %.3f m from the '%s' "
					"marker's spawn centre (%.3f, %.3f, %.3f), outside %.1f m",
					g_uSeamLeg, xLeg.m_szLabel, (double)fOffset, xLeg.m_szGateSpawnTag,
					(double)xExpected.x, (double)xExpected.y, (double)xExpected.z,
					(double)fSEAM_ARRIVAL_RADIUS_M);
				FailSeamRoundTrip("a seam crossing landed away from its own marker");
				return false;
			}

			++g_uSeamCrossingsProven;
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_SeamRoundTrip] leg %u ('%s') PROVEN: build %u at '%s', %.3f m from "
				"the marker",
				g_uSeamLeg, xLeg.m_szLabel, xLeg.m_uToBuildIndex,
				xLeg.m_szGateSpawnTag, (double)fOffset);

			++g_uSeamLeg;
			if (g_uSeamLeg >= uSEAM_LEG_COUNT)
			{
				g_bSeamPassed = true;
				g_eSeamPhase = SeamPhase::Done;
				return false;
			}
			g_eSeamPhase = SeamPhase::StageWarp;
			g_iSeamPhaseFrames = 0;
			return true;
		}

		case SeamPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_SeamRoundTrip()
	{
		ClearTraversalInput();
		Zenith_InputSimulator::ClearFixedDt();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		if (g_bSeamPrerequisitesPresent)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		}
		if (!g_bSeamPrerequisitesPresent)
		{
			return true;   // RequestSkip already bypassed this; belt and braces
		}

		// ★ THE COUNT IS ASSERTED SEPARATELY FROM THE PASS FLAG. A run that
		// silently proved fewer crossings than there are legs -- because a phase
		// returned false early, say -- would otherwise report success.
		if (!g_bSeamPassed || g_uSeamCrossingsProven != uSEAM_LEG_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_SeamRoundTrip] %s (leg %u of %u, %u crossing(s) proven)",
				g_szSeamFailure, g_uSeamLeg, uSEAM_LEG_COUNT, g_uSeamCrossingsProven);
			return false;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_SeamRoundTrip] all %u crossings proven: Dawnmere <-> Route 1 and "
			"Route 1 <-> Thornacre, both directions each", g_uSeamCrossingsProven);
		return true;
	}
}

static const Zenith_AutomatedTest g_xZMWarpInfrastructureTest = {
	"ZM_WarpInfrastructure_Test",
	&Setup_WarpInfrastructure,
	&Step_WarpInfrastructure,
	&Verify_WarpInfrastructure,
	/* maxFrames */ 900,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMWarpInfrastructureTest);

static const Zenith_AutomatedTest g_xZMPlayerHomeRoundTripTest = {
	"ZM_PlayerHomeRoundTrip_Test",
	&Setup_PlayerHomeRoundTrip,
	&Step_PlayerHomeRoundTrip,
	&Verify_PlayerHomeRoundTrip,
	/* maxFrames */ 1800,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMPlayerHomeRoundTripTest);

// ★ THE FRAME CAP IS A BACKSTOP ABOVE THE PER-PHASE DEADLINES, NOT THE BUDGET.
// Four legs, each at most one staging transition (420), one walk (600) and one
// landing transition (420) = 5,760 worst case, and every one of those three
// numbers owns a failure message that names what stalled. The REAL cost is far
// lower -- the walks are ~12 m each -- but the cap must sit above the sum, or a
// genuine stall reports "the test ran out of frames" instead of "the warp
// machine never left WAITING_FOR_SPAWN".
//
// ★ NOT m_bRequiresGraphics. Every claim it makes is about scenes, physics and
// the warp machine; a requiresGraphics test is SKIPPED-AS-PASSED on the headless
// gate, which is precisely how a test rots invisibly.
static const Zenith_AutomatedTest g_xZMSeamRoundTripTest = {
	"ZM_SeamRoundTrip_Test",
	&Setup_SeamRoundTrip,
	&Step_SeamRoundTrip,
	&Verify_SeamRoundTrip,
	/* maxFrames */ 6000,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMSeamRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
