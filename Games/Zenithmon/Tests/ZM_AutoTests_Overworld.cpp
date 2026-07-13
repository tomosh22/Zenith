#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;

	struct PlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	struct CameraView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera* m_pxFollow = nullptr;
		Zenith_CameraComponent* m_pxCamera = nullptr;
	};

	bool FindActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool FindActiveCamera(CameraView& xOut)
	{
		xOut = CameraView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool ActiveGrassIsReady()
	{
		bool bReady = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&bReady](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				bReady = bReady || xGrass.IsGrassApplied();
			});
		return bReady;
	}

	Zenith_Entity CreateFixtureBox(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xScale,
		RigidBodyType eBodyType)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPosition);
		xTransform.SetScale(xScale);
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_AABB, eBodyType);
		return xEntity;
	}

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" + ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" + ZENITH_MESH_EXT,
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

	void ClearOverworldInput()
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

	void CleanupFixtureScene(Zenith_Scene& xFixtureScene, Zenith_Scene xPreviousScene)
	{
		ClearOverworldInput();
		Zenith_InputSimulator::ClearFixedDt();
		if (xPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(xPreviousScene);
		}
		if (xFixtureScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xFixtureScene);
			xFixtureScene = Zenith_Scene();
		}
	}

	// -------------------------------------------------------------------------
	// Asset-free controller/camera harness
	// -------------------------------------------------------------------------

	enum class HarnessPhase
	{
		Settle,
		Walk,
		Run,
		Release,
		Done,
	};

	HarnessPhase g_eHarnessPhase = HarnessPhase::Done;
	Zenith_Scene g_xHarnessPreviousScene;
	Zenith_Scene g_xHarnessScene;
	int g_iHarnessPhaseFrames = 0;
	Zenith_Maths::Vector3 g_xHarnessStart(0.0f);
	Zenith_Maths::Vector3 g_xHarnessWalkEnd(0.0f);
	float g_fHarnessWalkDistance = 0.0f;
	bool g_bHarnessPassed = false;
	const char* g_szHarnessFailure = "test did not reach verification";

	void FailHarness(const char* szReason)
	{
		g_szHarnessFailure = szReason;
		g_bHarnessPassed = false;
		g_eHarnessPhase = HarnessPhase::Done;
	}

	void Setup_ControllerHarness()
	{
		g_eHarnessPhase = HarnessPhase::Done;
		g_iHarnessPhaseFrames = 0;
		g_xHarnessStart = Zenith_Maths::Vector3(0.0f);
		g_xHarnessWalkEnd = Zenith_Maths::Vector3(0.0f);
		g_fHarnessWalkDistance = 0.0f;
		g_bHarnessPassed = false;
		g_szHarnessFailure = "test did not reach verification";
		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

		g_xHarnessPreviousScene = g_xEngine.Scenes().GetActiveScene();
		g_xHarnessScene = g_xEngine.Scenes().LoadScene(
			"ZM_ControllerHarness", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		Zenith_SceneData* pxSceneData =
			g_xEngine.Scenes().GetSceneData(g_xHarnessScene);
		if (pxSceneData == nullptr
			|| !g_xEngine.Scenes().SetActiveScene(g_xHarnessScene))
		{
			FailHarness("could not create the isolated harness scene");
			return;
		}

		CreateFixtureBox(pxSceneData, "Floor", { 0.0f, -0.25f, 0.0f },
			{ 40.0f, 0.5f, 40.0f }, RIGIDBODY_TYPE_STATIC);
		Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "Player");
		Zenith_TransformComponent& xPlayerTransform =
			xPlayer.GetComponent<Zenith_TransformComponent>();
		xPlayerTransform.SetPosition({ 0.0f, 0.95f, 0.0f });
		xPlayerTransform.SetScale({ 0.8f, 1.8f, 0.8f });
		xPlayer.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xPlayer.AddComponent<ZM_PlayerController>();

		Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(
			pxSceneData, "HarnessCamera");
		Zenith_CameraComponent& xCameraComponent =
			xCamera.AddComponent<Zenith_CameraComponent>();
		xCameraComponent.SetPosition({ 0.0f, 3.0f, -5.5f });
		xCameraComponent.SetYaw(0.0);
		xCameraComponent.SetPitch(-0.4);
		xCamera.AddComponent<ZM_FollowCamera>();
		Zenith_UnitTests::SetMainCameraForTest(pxSceneData, xCamera.GetEntityID());

		g_eHarnessPhase = HarnessPhase::Settle;
	}

	bool Step_ControllerHarness(int)
	{
		if (g_eHarnessPhase == HarnessPhase::Done)
		{
			return false;
		}

		++g_iHarnessPhaseFrames;
		PlayerView xPlayer;
		CameraView xCamera;
		const bool bHasPlayer = FindActivePlayer(xPlayer);
		const bool bHasCamera = FindActiveCamera(xCamera);
		if (!bHasPlayer || !bHasCamera)
		{
			if (g_iHarnessPhaseFrames > 120)
			{
				FailHarness("fixture components did not become visible within 120 frames");
				return false;
			}
			return true;
		}

		switch (g_eHarnessPhase)
		{
		case HarnessPhase::Settle:
			if (xPlayer.m_pxCollider->HasValidBody()
				&& xPlayer.m_pxController->IsGrounded()
				&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
				&& xCamera.m_pxFollow->GetCurrentArmDistance() > 0.0f)
			{
				g_xHarnessStart = xPlayer.m_xPosition;
				Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
				g_eHarnessPhase = HarnessPhase::Walk;
				g_iHarnessPhaseFrames = 0;
			}
			else if (g_iHarnessPhaseFrames > 180)
			{
				FailHarness("capsule did not ground or camera did not acquire Player");
				return false;
			}
			return true;

		case HarnessPhase::Walk:
			if (g_iHarnessPhaseFrames < 60)
			{
				return true;
			}
			if (std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
				- ZM_PlayerController::fWALK_SPEED) > 0.02f)
			{
				FailHarness("walk phase did not request the walk speed");
				return false;
			}
			g_xHarnessWalkEnd = xPlayer.m_xPosition;
			g_fHarnessWalkDistance = glm::length(
				Zenith_Maths::Vector2(g_xHarnessWalkEnd.x - g_xHarnessStart.x,
					g_xHarnessWalkEnd.z - g_xHarnessStart.z));
			if (g_xHarnessWalkEnd.z <= g_xHarnessStart.z + 1.0f)
			{
				FailHarness("W did not move the yaw-zero player in world +Z");
				return false;
			}
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
			g_eHarnessPhase = HarnessPhase::Run;
			g_iHarnessPhaseFrames = 0;
			return true;

		case HarnessPhase::Run:
			if (g_iHarnessPhaseFrames < 60)
			{
				return true;
			}
			if (std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
				- ZM_PlayerController::fRUN_SPEED) > 0.02f)
			{
				FailHarness("run phase did not request the run speed");
				return false;
			}
			if (glm::length(Zenith_Maths::Vector2(
				xPlayer.m_xPosition.x - g_xHarnessWalkEnd.x,
				xPlayer.m_xPosition.z - g_xHarnessWalkEnd.z))
				<= g_fHarnessWalkDistance * 1.25f)
			{
				FailHarness("run displacement was not distinctly greater than walk");
				return false;
			}
			ClearOverworldInput();
			g_eHarnessPhase = HarnessPhase::Release;
			g_iHarnessPhaseFrames = 0;
			return true;

		case HarnessPhase::Release:
			if (g_iHarnessPhaseFrames < 20)
			{
				return true;
			}
			{
				const Zenith_Maths::Vector3 xVelocity =
					g_xEngine.Physics().GetLinearVelocity(
						xPlayer.m_pxCollider->GetBodyID());
				Zenith_Maths::Vector3 xCameraPosition;
				xCamera.m_pxCamera->GetPosition(xCameraPosition);
				if (std::fabs(xVelocity.x) > 0.05f
					|| std::fabs(xVelocity.z) > 0.05f
					|| xPlayer.m_pxController->GetRequestedSpeed() != 0.0f
					|| !xPlayer.m_pxController->IsGrounded()
					|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
					|| xCamera.m_pxFollow->GetCurrentArmDistance()
						< ZM_FollowCamera::GetMinimumArmLength()
					|| !std::isfinite(xCameraPosition.x)
					|| !std::isfinite(xCameraPosition.y)
					|| !std::isfinite(xCameraPosition.z))
				{
					FailHarness("release or follow-camera invariants failed");
					return false;
				}
			}
			g_bHarnessPassed = true;
			g_eHarnessPhase = HarnessPhase::Done;
			return false;

		case HarnessPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ControllerHarness()
	{
		if (!g_bHarnessPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_ControllerHarness] %s", g_szHarnessFailure);
		}
		CleanupFixtureScene(g_xHarnessScene, g_xHarnessPreviousScene);
		return g_bHarnessPassed;
	}

	// -------------------------------------------------------------------------
	// Baked Dawnmere integration
	// -------------------------------------------------------------------------

	enum class DawnmerePhase
	{
		FirstLoad,
		Walk,
		Reload,
		ReloadWalk,
		FrontEnd,
		Done,
	};

	DawnmerePhase g_eDawnmerePhase = DawnmerePhase::Done;
	int g_iDawnmerePhaseFrames = 0;
	bool g_bDawnmerePrerequisitesPresent = false;
	bool g_bDawnmerePassed = false;
	const char* g_szDawnmereFailure = "test did not reach verification";
	Zenith_EntityID g_xFirstPlayerID = INVALID_ENTITY_ID;
	Zenith_EntityID g_xFirstCameraID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 g_xDawnmereMoveStart(0.0f);

	void FailDawnmere(const char* szReason)
	{
		g_szDawnmereFailure = szReason;
		g_bDawnmerePassed = false;
		g_eDawnmerePhase = DawnmerePhase::Done;
		ClearOverworldInput();
	}

	bool DawnmereRuntimeReady(PlayerView& xPlayer, CameraView& xCamera)
	{
		return FindActivePlayer(xPlayer)
			&& FindActiveCamera(xCamera)
			&& xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController->IsGrounded()
			&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
			&& xCamera.m_pxFollow->GetCurrentArmDistance() > 0.0f
			&& ActiveGrassIsReady();
	}

	void Setup_DawnmerePlayerCamera()
	{
		g_eDawnmerePhase = DawnmerePhase::Done;
		g_iDawnmerePhaseFrames = 0;
		g_bDawnmerePrerequisitesPresent = RequiredDawnmereAssetsPresent();
		g_bDawnmerePassed = false;
		g_szDawnmereFailure = "test did not reach verification";
		g_xFirstPlayerID = INVALID_ENTITY_ID;
		g_xFirstCameraID = INVALID_ENTITY_ID;
		g_xDawnmereMoveStart = Zenith_Maths::Vector3(0.0f);
		Zenith_InputSimulator::ResetAllInputState();

		// Asset guard must precede fixed-dt state: RequestSkip bypasses Verify.
		if (!g_bDawnmerePrerequisitesPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere scene/terrain bake is absent or incomplete");
			return;
		}

		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);
		g_eDawnmerePhase = DawnmerePhase::FirstLoad;
		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
	}

	bool Step_DawnmerePlayerCamera(int)
	{
		if (!g_bDawnmerePrerequisitesPresent
			|| g_eDawnmerePhase == DawnmerePhase::Done)
		{
			return false;
		}

		++g_iDawnmerePhaseFrames;
		PlayerView xPlayer;
		CameraView xCamera;
		switch (g_eDawnmerePhase)
		{
		case DawnmerePhase::FirstLoad:
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iDawnmerePhaseFrames > 420)
				{
					FailDawnmere("first Dawnmere load did not become ready in 420 frames");
					return false;
				}
				return true;
			}
			if (std::fabs(xPlayer.m_xPosition.x - 512.0f) > 1.0f
				|| std::fabs(xPlayer.m_xPosition.z - 480.0f) > 1.0f
				|| std::fabs(xPlayer.m_xPosition.y - 26.9f) > 1.5f)
			{
				FailDawnmere("Player did not settle near the authored TownCenter placement");
				return false;
			}
			if (std::fabs(xCamera.m_pxCamera->GetFOV()
				- glm::radians(ZM_FollowCamera::GetFOVDegrees())) > 0.001f
				|| xCamera.m_pxFollow->GetCurrentArmDistance()
					< ZM_FollowCamera::GetMinimumArmLength())
			{
				FailDawnmere("Dawnmere follow-camera numeric contract was not applied");
				return false;
			}
			g_xFirstPlayerID = xPlayer.m_xEntityID;
			g_xFirstCameraID = xCamera.m_xEntityID;
			g_xDawnmereMoveStart = xPlayer.m_xPosition;
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			g_eDawnmerePhase = DawnmerePhase::Walk;
			g_iDawnmerePhaseFrames = 0;
			return true;

		case DawnmerePhase::Walk:
			if (!FindActivePlayer(xPlayer) || !FindActiveCamera(xCamera))
			{
				FailDawnmere("Player or camera disappeared during first walk");
				return false;
			}
			if (g_iDawnmerePhaseFrames < 60)
			{
				return true;
			}
			if (xPlayer.m_xPosition.z <= g_xDawnmereMoveStart.z + 1.0f
				|| std::fabs(xPlayer.m_pxController->GetRequestedSpeed()
					- ZM_PlayerController::fWALK_SPEED) > 0.02f
				|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
				|| !ActiveGrassIsReady())
			{
				FailDawnmere("Dawnmere walk/camera/grass invariant failed");
				return false;
			}
			ClearOverworldInput();
			g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
			g_eDawnmerePhase = DawnmerePhase::Reload;
			g_iDawnmerePhaseFrames = 0;
			return true;

		case DawnmerePhase::Reload:
			if (g_iDawnmerePhaseFrames < 5)
			{
				return true;
			}
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iDawnmerePhaseFrames > 420)
				{
					FailDawnmere("reloaded Dawnmere did not become ready in 420 frames");
					return false;
				}
				return true;
			}
			if (xPlayer.m_xEntityID == g_xFirstPlayerID
				|| xCamera.m_xEntityID == g_xFirstCameraID)
			{
				FailDawnmere("SINGLE reload retained scene-owned Player or camera identity");
				return false;
			}
			g_xDawnmereMoveStart = xPlayer.m_xPosition;
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
			g_eDawnmerePhase = DawnmerePhase::ReloadWalk;
			g_iDawnmerePhaseFrames = 0;
			return true;

		case DawnmerePhase::ReloadWalk:
			if (!FindActivePlayer(xPlayer) || !FindActiveCamera(xCamera))
			{
				FailDawnmere("reloaded Player or camera disappeared during walk");
				return false;
			}
			if (g_iDawnmerePhaseFrames < 45)
			{
				return true;
			}
			if (xPlayer.m_xPosition.z <= g_xDawnmereMoveStart.z + 0.5f
				|| xCamera.m_pxFollow->GetTargetEntityID() != xPlayer.m_xEntityID
				|| !ActiveGrassIsReady())
			{
				FailDawnmere("reloaded scene did not reacquire input, camera, and grass");
				return false;
			}
			ClearOverworldInput();
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
			g_eDawnmerePhase = DawnmerePhase::FrontEnd;
			g_iDawnmerePhaseFrames = 0;
			return true;

		case DawnmerePhase::FrontEnd:
			if (g_iDawnmerePhaseFrames < 5)
			{
				return true;
			}
			if (FindActivePlayer(xPlayer) || FindActiveCamera(xCamera))
			{
				if (g_iDawnmerePhaseFrames > 120)
				{
					FailDawnmere("FrontEnd retained Dawnmere-owned Player or camera");
					return false;
				}
				return true;
			}
			g_bDawnmerePassed = true;
			g_eDawnmerePhase = DawnmerePhase::Done;
			return false;

		case DawnmerePhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_DawnmerePlayerCamera()
	{
		ClearOverworldInput();
		Zenith_InputSimulator::ClearFixedDt();
		if (!g_bDawnmerePassed && g_bDawnmerePrerequisitesPresent)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_DawnmerePlayerCamera] %s", g_szDawnmereFailure);
		}
		return g_bDawnmerePassed || !g_bDawnmerePrerequisitesPresent;
	}
}

static const Zenith_AutomatedTest g_xZMControllerHarnessTest = {
	"ZM_ControllerHarness_Test",
	&Setup_ControllerHarness,
	&Step_ControllerHarness,
	&Verify_ControllerHarness,
	/* maxFrames */ 600,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMControllerHarnessTest);

static const Zenith_AutomatedTest g_xZMDawnmerePlayerCameraTest = {
	"ZM_DawnmerePlayerCamera_Test",
	&Setup_DawnmerePlayerCamera,
	&Step_DawnmerePlayerCamera,
	&Verify_DawnmerePlayerCamera,
	/* maxFrames */ 1200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmerePlayerCameraTest);

#endif // ZENITH_INPUT_SIMULATOR
