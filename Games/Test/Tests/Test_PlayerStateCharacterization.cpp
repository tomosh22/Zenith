#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_PlayerStateCharacterization - characterization for the W1 decomposition
 * of Test_PlayerControllerComponent's decision logic into the
 * Test_PlayerActions graph (fly-cam toggle, inventory slots, health demo,
 * compass).
 *
 * Written against the C++ version FIRST; the graph version must keep it
 * green unchanged. Pins, through the real input path:
 *   - T/H: damage 10 / heal 15, clamped to max health; health bar fill +
 *     green color band at 100%.
 *   - 1-6: slot selection (key 3 -> slot index 2).
 *   - Compass text follows camera yaw (yaw = pi -> "S") while walking.
 *   - C toggles fly-cam: walking moves the BODY; fly-cam moves the CAMERA
 *     and leaves the body's XZ untouched.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "Test/Components/Test_PlayerControllerComponent.h"

namespace
{
	enum class StatePhase
	{
		Boot, Settle, Construct, Awaken,
		PressT, CheckDamage, PressH, CheckHeal,
		PressSlot, CheckSlot,
		SetYaw, CheckCompass,
		WalkStart, WalkRun, WalkCheck,
		ToggleFly, FlySettle, FlyStart, FlyRun, FlyCheck,
		Done
	};

	StatePhase      g_ePhase = StatePhase::Boot;
	int             g_iFrame = 0;
	Zenith_EntityID g_xPlayer;
	Zenith_EntityID g_xHUD;
	bool            g_bFailed = false;
	const char*     g_szFailure = "";

	// Walk/fly measurement state.
	Zenith_Maths::Vector3 g_xBodyStart;
	Zenith_Maths::Vector3 g_xCameraStart;

	void Fail(const char* szWhy)
	{
		g_bFailed = true;
		g_szFailure = szWhy;
	}

	Test_PlayerControllerComponent* Player()
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xPlayer);
		return xPlayer.IsValid() ? xPlayer.TryGetComponent<Test_PlayerControllerComponent>() : nullptr;
	}

	Zenith_UIComponent* HUD()
	{
		Zenith_Entity xHUD = g_xEngine.Scenes().ResolveEntity(g_xHUD);
		return xHUD.IsValid() ? xHUD.TryGetComponent<Zenith_UIComponent>() : nullptr;
	}

	bool ReadBodyPosition(Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xPlayer);
		Zenith_TransformComponent* pxTransform = xPlayer.IsValid()
			? xPlayer.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	bool ReadCameraPosition(Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xPlayer);
		Zenith_CameraComponent* pxCamera = xPlayer.IsValid()
			? xPlayer.TryGetComponent<Zenith_CameraComponent>() : nullptr;
		if (pxCamera == nullptr) return false;
		pxCamera->GetPosition(xOut);
		return true;
	}
}

static void Setup_PlayerState()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_ePhase = StatePhase::Boot;
	g_iFrame = 0;
	g_xPlayer = Zenith_EntityID();
	g_xHUD = Zenith_EntityID();
	g_bFailed = false;
	g_szFailure = "";
}

static bool Step_PlayerState(int /*iFrame*/)
{
	if (g_bFailed) return false;

	switch (g_ePhase)
	{
	case StatePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePhase = StatePhase::Settle;
		g_iFrame = 0;
		return true;

	case StatePhase::Settle:
		if (++g_iFrame < 10) return true;
		g_ePhase = StatePhase::Construct;
		return true;

	case StatePhase::Construct:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr) { Fail("no scene data"); return false; }

		// The HUD the controller looks up by name (the authored Test scene
		// has none - the component degrades gracefully without it, so the
		// characterization constructs the full HUD to pin the UI writes too).
		Zenith_Entity xHUDEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "HUD");
		Zenith_UIComponent& xUI = xHUDEntity.AddComponent<Zenith_UIComponent>();
		xUI.CreateRect("HealthBar_Fill");
		xUI.CreateText("CompassText", "?");
		for (int i = 0; i < Test_PlayerControllerComponent::s_iInventorySlots; ++i)
		{
			xUI.CreateRect(("InventorySlot_" + std::to_string(i) + "_BG").c_str());
		}
		g_xHUD = xHUDEntity.GetEntityID();

		// The player, authored the way a user builds it (the shoot-test recipe).
		Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "StateTestPlayer");
		xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 30.0f, 0.0f));
		Zenith_ColliderComponent& xCollider = xPlayer.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCapsuleCollider(0.5f, 1.0f, RIGIDBODY_TYPE_DYNAMIC);
		xPlayer.AddComponent<Zenith_CameraComponent>();
		xPlayer.AddComponent<Test_PlayerControllerComponent>();
		xPlayer.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Test_PlayerActions.bgraph");
		g_xPlayer = xPlayer.GetEntityID();

		g_iFrame = 0;
		g_ePhase = StatePhase::Awaken;
		return true;
	}

	case StatePhase::Awaken:
		if (++g_iFrame < 5) return true;
		g_ePhase = StatePhase::PressT;
		return true;

	case StatePhase::PressT:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_T);
		g_iFrame = 0;
		g_ePhase = StatePhase::CheckDamage;
		return true;

	case StatePhase::CheckDamage:
	{
		if (++g_iFrame < 5) return true;
		Test_PlayerControllerComponent* pxPlayer = Player();
		if (pxPlayer == nullptr) { Fail("player component missing"); return false; }
		if (pxPlayer->GetHealth() != 90.0f) { Fail("T did not deal 10 damage"); return false; }
		if (Zenith_UIComponent* pxUI = HUD())
		{
			Zenith_UI::Zenith_UIRect* pxFill = pxUI->FindElement<Zenith_UI::Zenith_UIRect>("HealthBar_Fill");
			if (pxFill == nullptr || pxFill->GetFillAmount() < 0.89f || pxFill->GetFillAmount() > 0.91f)
			{
				Fail("health fill not ~0.9 after damage");
				return false;
			}
			// 0.9 > 0.6: the green band.
			if (pxFill->GetColor().y < 0.7f) { Fail("health fill not green at 90%"); return false; }
		}
		g_ePhase = StatePhase::PressH;
		return true;
	}

	case StatePhase::PressH:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_H);
		g_iFrame = 0;
		g_ePhase = StatePhase::CheckHeal;
		return true;

	case StatePhase::CheckHeal:
	{
		if (++g_iFrame < 5) return true;
		Test_PlayerControllerComponent* pxPlayer = Player();
		if (pxPlayer == nullptr) { Fail("player component missing"); return false; }
		// 90 + 15 clamps to max 100.
		if (pxPlayer->GetHealth() != 100.0f) { Fail("H did not heal to the clamp"); return false; }
		g_ePhase = StatePhase::PressSlot;
		return true;
	}

	case StatePhase::PressSlot:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_3);
		g_iFrame = 0;
		g_ePhase = StatePhase::CheckSlot;
		return true;

	case StatePhase::CheckSlot:
	{
		if (++g_iFrame < 5) return true;
		Test_PlayerControllerComponent* pxPlayer = Player();
		if (pxPlayer == nullptr) { Fail("player component missing"); return false; }
		if (pxPlayer->GetSelectedSlot() != 2) { Fail("key 3 did not select slot index 2"); return false; }
		g_ePhase = StatePhase::SetYaw;
		return true;
	}

	case StatePhase::SetYaw:
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(g_xPlayer);
		Zenith_CameraComponent* pxCamera = xPlayer.IsValid() ? xPlayer.TryGetComponent<Zenith_CameraComponent>() : nullptr;
		if (pxCamera == nullptr) { Fail("camera missing"); return false; }
		pxCamera->SetYaw(Zenith_Maths::Pi);	// facing "S"
		g_iFrame = 0;
		g_ePhase = StatePhase::CheckCompass;
		return true;
	}

	case StatePhase::CheckCompass:
	{
		if (++g_iFrame < 5) return true;
		Zenith_UIComponent* pxUI = HUD();
		Zenith_UI::Zenith_UIText* pxCompass = pxUI ? pxUI->FindElement<Zenith_UI::Zenith_UIText>("CompassText") : nullptr;
		if (pxCompass == nullptr) { Fail("compass element missing"); return false; }
		if (pxCompass->GetText() != "S") { Fail("compass did not read S at yaw pi"); return false; }
		g_ePhase = StatePhase::WalkStart;
		return true;
	}

	case StatePhase::WalkStart:
		if (!ReadBodyPosition(g_xBodyStart)) { Fail("no body transform"); return false; }
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		g_iFrame = 0;
		g_ePhase = StatePhase::WalkRun;
		return true;

	case StatePhase::WalkRun:
		if (++g_iFrame < 30) return true;
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		g_ePhase = StatePhase::WalkCheck;
		return true;

	case StatePhase::WalkCheck:
	{
		Zenith_Maths::Vector3 xBodyNow;
		if (!ReadBodyPosition(xBodyNow)) { Fail("no body transform"); return false; }
		const float fXZ = std::fabs(xBodyNow.x - g_xBodyStart.x) + std::fabs(xBodyNow.z - g_xBodyStart.z);
		if (fXZ < 0.5f) { Fail("walk-mode W did not move the body"); return false; }
		g_ePhase = StatePhase::ToggleFly;
		return true;
	}

	case StatePhase::ToggleFly:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_C);
		g_iFrame = 0;
		g_ePhase = StatePhase::FlySettle;
		return true;

	case StatePhase::FlySettle:
		// Let the walk branch's final zero-XZ velocity land before sampling.
		if (++g_iFrame < 10) return true;
		g_ePhase = StatePhase::FlyStart;
		return true;

	case StatePhase::FlyStart:
		if (!ReadBodyPosition(g_xBodyStart)) { Fail("no body transform"); return false; }
		if (!ReadCameraPosition(g_xCameraStart)) { Fail("no camera"); return false; }
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		g_iFrame = 0;
		g_ePhase = StatePhase::FlyRun;
		return true;

	case StatePhase::FlyRun:
		if (++g_iFrame < 30) return true;
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		g_ePhase = StatePhase::FlyCheck;
		return true;

	case StatePhase::FlyCheck:
	{
		Zenith_Maths::Vector3 xBodyNow, xCameraNow;
		if (!ReadBodyPosition(xBodyNow) || !ReadCameraPosition(xCameraNow)) { Fail("no transforms"); return false; }
		const float fBodyXZ = std::fabs(xBodyNow.x - g_xBodyStart.x) + std::fabs(xBodyNow.z - g_xBodyStart.z);
		const float fCamera = glm::length(xCameraNow - g_xCameraStart);
		if (fBodyXZ > 0.1f) { Fail("fly-cam W moved the body"); return false; }
		if (fCamera < 0.5f) { Fail("fly-cam W did not move the camera"); return false; }
		g_ePhase = StatePhase::Done;
		return false;
	}

	case StatePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerState()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (g_bFailed)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerState] FAILED: %s", g_szFailure);
		return false;
	}
	if (g_ePhase != StatePhase::Done)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerState] ran out of frames in phase %d", static_cast<int>(g_ePhase));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPlayerStateTest = {
	"Test_PlayerState_Test",
	&Setup_PlayerState,
	&Step_PlayerState,
	&Verify_PlayerState,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerStateTest);

#endif // ZENITH_INPUT_SIMULATOR
