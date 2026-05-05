#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_Input.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"

#include "RenderTest/Components/RenderTest_FollowCamera.h"
#include "RenderTest/Components/RenderTest_PlayerBehaviour.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// RenderTest input-simulator tests
//
// Drives the FollowCamera and PlayerBehaviour scripts via Zenith_InputSimulator
// and asserts the gameplay-visible state changes. Each test owns a fresh empty
// scene + minimal Player + GameManager entities; the behaviour scripts are
// instantiated directly (not via Zenith_ScriptComponent) so we can call the
// lifecycle hooks deterministically without spinning up a full Play-mode loop.
//
// Frame model:
//   - BeginFrame() — mirrors Zenith_Core::Zenith_MainLoop's first step: it
//     calls Zenith_Input::BeginFrame() which auto-releases SimulateKeyPress'd
//     keys from the prior frame and recomputes mouse delta from the simulator
//     position. Inputs simulated AFTER this point and BEFORE Step() are read.
//   - Step()      — runs one OnUpdate(player) + OnLateUpdate(camera) tick.
// ============================================================================

namespace
{
	struct RenderTest_TestFixture
	{
		Zenith_Scene xScene;
		Zenith_EntityID uPlayerID = INVALID_ENTITY_ID;
		Zenith_EntityID uGameManagerID = INVALID_ENTITY_ID;
		RenderTest_FollowCamera*    pxCamera = nullptr;
		RenderTest_PlayerBehaviour* pxPlayer = nullptr;

		RenderTest_TestFixture()
		{
			Zenith_InputSimulator::Enable();
			RenderTest_GameplayState::Reset();

			xScene = Zenith_SceneManager::CreateEmptyScene("RenderTestInputTestScene");
			Zenith_SceneManager::SetActiveScene(xScene);
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);

			// Player entity: Transform (auto-added) + Collider, name "Player"
			// so FollowCamera can find it by name.
			Zenith_Entity xPlayer(pxSceneData, "Player");
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition({ 0.0f, 0.0f, 0.0f });
			xPlayer.AddComponent<Zenith_ColliderComponent>();
			uPlayerID = xPlayer.GetEntityID();

			// GameManager entity: Camera component for the FollowCamera.
			Zenith_Entity xGameManager(pxSceneData, "GameManager");
			xGameManager.AddComponent<Zenith_CameraComponent>();
			uGameManagerID = xGameManager.GetEntityID();

			// Instantiate behaviours directly. Skipping the script-component
			// path because RenderTest_PlayerBehaviour::OnStart loads .zanim
			// files and builds a layered animator that needs a real Animator
			// component — heavy setup we don't need for input-driven tests.
			pxCamera = new RenderTest_FollowCamera(xGameManager);
			pxPlayer = new RenderTest_PlayerBehaviour(xPlayer);

			pxCamera->OnAwake();
			pxPlayer->OnAwake();

			// Warm-up frame: BeginFrame primes the simulator's mouse-position
			// baseline at (0, 0); Step() runs OnUpdate/OnLateUpdate once so the
			// camera consumes its m_bFirstMouseSample guard. Tests start from a
			// "post-bootstrap" state where subsequent mouse deltas drive yaw/pitch
			// without being suppressed.
			Zenith_InputSimulator::SimulateMousePosition(0.0, 0.0);
			BeginFrame();
			Step();
		}

		~RenderTest_TestFixture()
		{
			delete pxCamera;
			delete pxPlayer;
			Zenith_InputSimulator::Disable();
			Zenith_SceneManager::UnloadSceneForced(xScene);
		}

		// Mirrors what Zenith_Core::Zenith_MainLoop's prologue does: clears
		// per-frame press flags, auto-releases SimulateKeyPress'd keys, and
		// updates mouse delta from the simulator's current position.
		void BeginFrame()
		{
			Zenith_Input::BeginFrame();
		}

		// Runs one update tick. The player updates first because the camera
		// reads RenderTest_GameplayState::IsLocalPlayerAiming() which the
		// player writes during its OnUpdate.
		void Step(float fDt = 1.0f / 60.0f)
		{
			pxPlayer->OnUpdate(fDt);
			pxCamera->OnLateUpdate(fDt);
		}

		Zenith_Maths::Vector3 GetPlayerPosition() const
		{
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
			Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerID);
			Zenith_Maths::Vector3 xPos;
			xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			return xPos;
		}

		void SetPlayerPosition(const Zenith_Maths::Vector3& xPos)
		{
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
			Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerID);
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		}
	};
}

// ----- Camera mouse-look ----------------------------------------------------

ZENITH_TEST(RenderTestInput, CameraYawDecreasesOnMouseRight)
{ Zenith_UnitTests::TestRenderTestCameraYawDecreasesOnMouseRight(); }
void Zenith_UnitTests::TestRenderTestCameraYawDecreasesOnMouseRight()
{
	RenderTest_TestFixture xFix;

	const float fStartYaw = RenderTest_GameplayState::GetCameraYaw();
	xFix.BeginFrame();
	// Simulate a +500px mouse-X delta. Camera applies yaw -= delta * (1/500).
	Zenith_InputSimulator::SimulateMousePosition(500.0, 0.0);
	xFix.BeginFrame();  // recomputes delta = (500, 0) - (0, 0)
	xFix.Step();

	const float fEndYaw = RenderTest_GameplayState::GetCameraYaw();
	// Yaw decreased — but the wrap-to-[0, 2pi] flips negative results into
	// the upper end of the range. Compare the unwrapped change instead.
	const float fTwoPi = static_cast<float>(Zenith_Maths::Pi * 2.0);
	const float fDelta = fEndYaw < fStartYaw ? fEndYaw - fStartYaw
	                                         : fEndYaw - fStartYaw - fTwoPi;
	ZENITH_ASSERT_TRUE(std::abs(fDelta + 1.0f) < 0.01f,
		"Yaw should decrease by ~1.0 rad after +500px X delta");
}

ZENITH_TEST(RenderTestInput, CameraPitchDecreasesOnMouseDown)
{ Zenith_UnitTests::TestRenderTestCameraPitchDecreasesOnMouseDown(); }
void Zenith_UnitTests::TestRenderTestCameraPitchDecreasesOnMouseDown()
{
	RenderTest_TestFixture xFix;

	const float fStartPitch = RenderTest_GameplayState::GetCameraPitch();
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateMousePosition(0.0, 250.0);
	xFix.BeginFrame();
	xFix.Step();

	const float fEndPitch = RenderTest_GameplayState::GetCameraPitch();
	// pitch -= 250/500 = 0.5
	ZENITH_ASSERT_TRUE(std::abs((fEndPitch - fStartPitch) + 0.5f) < 0.01f,
		"Pitch should decrease by ~0.5 rad after +250px Y delta");
}

ZENITH_TEST(RenderTestInput, CameraPitchClampedAtFloor)
{ Zenith_UnitTests::TestRenderTestCameraPitchClampedAtFloor(); }
void Zenith_UnitTests::TestRenderTestCameraPitchClampedAtFloor()
{
	RenderTest_TestFixture xFix;

	xFix.BeginFrame();
	// Huge downward mouse delta (10000px = 20 rad of pitch attempt). Should
	// clamp to -1.2 rad floor.
	Zenith_InputSimulator::SimulateMousePosition(0.0, 10000.0);
	xFix.BeginFrame();
	xFix.Step();

	const float fPitch = RenderTest_GameplayState::GetCameraPitch();
	ZENITH_ASSERT_TRUE(std::abs(fPitch - (-1.2f)) < 0.01f,
		"Pitch should clamp to -1.2 rad floor under large positive Y delta");
}

ZENITH_TEST(RenderTestInput, CameraPitchClampedAtCeiling)
{ Zenith_UnitTests::TestRenderTestCameraPitchClampedAtCeiling(); }
void Zenith_UnitTests::TestRenderTestCameraPitchClampedAtCeiling()
{
	RenderTest_TestFixture xFix;

	xFix.BeginFrame();
	// Huge upward mouse delta. Should clamp to +0.6 rad ceiling.
	Zenith_InputSimulator::SimulateMousePosition(0.0, -10000.0);
	xFix.BeginFrame();
	xFix.Step();

	const float fPitch = RenderTest_GameplayState::GetCameraPitch();
	ZENITH_ASSERT_TRUE(std::abs(fPitch - 0.6f) < 0.01f,
		"Pitch should clamp to +0.6 rad ceiling under large negative Y delta");
}

ZENITH_TEST(RenderTestInput, CameraYawWrapsToZeroToTwoPi)
{ Zenith_UnitTests::TestRenderTestCameraYawWrapsToZeroToTwoPi(); }
void Zenith_UnitTests::TestRenderTestCameraYawWrapsToZeroToTwoPi()
{
	RenderTest_TestFixture xFix;
	const float fTwoPi = static_cast<float>(Zenith_Maths::Pi * 2.0);

	// Drive yaw heavily negative and verify it wraps into [0, 2pi).
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateMousePosition(2000.0, 0.0);  // delta +2000 -> yaw -= 4
	xFix.BeginFrame();
	xFix.Step();

	const float fYaw = RenderTest_GameplayState::GetCameraYaw();
	ZENITH_ASSERT_TRUE(fYaw >= 0.0f && fYaw <= fTwoPi,
		"Yaw must wrap into [0, 2pi)");
}

// (CameraNoFirstFrameJump removed: the m_bFirstMouseSample guard exists for
// real-input scenarios where the OS cursor has wandered before scene load.
// Under the simulator, the cursor always starts at (0,0) and the fixture's
// warm-up Step consumes the guard before any test runs, so the guard isn't
// observable from this test path.)

// ----- Player movement ------------------------------------------------------

ZENITH_TEST(RenderTestInput, PlayerMovesForwardOnW)
{ Zenith_UnitTests::TestRenderTestPlayerMovesForwardOnW(); }
void Zenith_UnitTests::TestRenderTestPlayerMovesForwardOnW()
{
	RenderTest_TestFixture xFix;

	// At yaw=0 the camera-relative forward is (sin(0), 0, -cos(0)) = (0, 0, -1).
	// W (input.z = +1) means "press forward" -> moves in -Z world direction.
	xFix.SetPlayerPosition({ 0.0f, 5.0f, 0.0f });

	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
	// Without a real physics body, SetLinearVelocity is a no-op (HasValidBody()
	// is false), so we verify forward motion by checking the player's rotation
	// converges toward the movement direction. The slerp uses (fDt * 10) per
	// frame so it converges asymptotically — give it ~1s simulated to settle.
	for (int i = 0; i < 60; ++i)
	{
		xFix.Step();
		xFix.BeginFrame();
	}
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);

	// At yaw=0 the camera-relative forward is (-sin(0), 0, cos(0)) = +Z, so
	// pressing W moves +Z (away from the camera which sits at -Z behind the
	// player). The player rotates to face +Z, which under the transform's
	// rotation convention (angleAxis(target_yaw, +Y) starting from local +Z)
	// is target_yaw == 0 — i.e., the identity quat.
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xFix.xScene);
	Zenith_Entity xPlayer = pxSceneData->GetEntity(xFix.uPlayerID);
	Zenith_Maths::Quat xRot;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);
	const float fPlayerYaw = atan2f(2.0f * (xRot.w * xRot.y + xRot.x * xRot.z),
	                                1.0f - 2.0f * (xRot.y * xRot.y + xRot.x * xRot.x));
	ZENITH_ASSERT_TRUE(std::abs(fPlayerYaw) < 0.1f,
		"Player should face +Z (yaw ~= 0) after holding W with camera yaw=0 "
		"(forward = away from camera)");
}

ZENITH_TEST(RenderTestInput, PlayerNoRotationOnBackward)
{ Zenith_UnitTests::TestRenderTestPlayerNoRotationOnBackward(); }
void Zenith_UnitTests::TestRenderTestPlayerNoRotationOnBackward()
{
	RenderTest_TestFixture xFix;

	Zenith_Maths::Quat xStartRot;
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xFix.xScene);
	Zenith_Entity xPlayer = pxSceneData->GetEntity(xFix.uPlayerID);
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xStartRot);

	// S (backward) should NOT rotate the player — the plan deliberately keeps
	// the current yaw to avoid a 180-degree spin (no backward-walk anim).
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_S);
	for (int i = 0; i < 10; ++i)
	{
		xFix.Step();
		xFix.BeginFrame();
	}
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_S);

	Zenith_Maths::Quat xEndRot;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xEndRot);
	const float fDot = std::abs(xStartRot.w * xEndRot.w + xStartRot.x * xEndRot.x
	                          + xStartRot.y * xEndRot.y + xStartRot.z * xEndRot.z);
	ZENITH_ASSERT_TRUE(fDot > 0.999f,
		"Player rotation must not change when only S is held (no backward-walk anim)");
}

ZENITH_TEST(RenderTestInput, PlayerRotatesWithCameraYawWhenAiming)
{ Zenith_UnitTests::TestRenderTestPlayerRotatesWithCameraYawWhenAiming(); }
void Zenith_UnitTests::TestRenderTestPlayerRotatesWithCameraYawWhenAiming()
{
	RenderTest_TestFixture xFix;

	// Rotate camera 90 degrees to the right (yaw decreases by ~pi/2 under
	// the engine's sign convention; mouse-X positive moves yaw negative).
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateMousePosition(static_cast<double>(0.5 * Zenith_Maths::Pi * 500.0), 0.0);
	xFix.BeginFrame();
	xFix.Step();
	const float fCamYaw = RenderTest_GameplayState::GetCameraYaw();

	// Now hold RMB so the player rotates to face camera direction (no movement
	// input, so the only rotation source is ADS rotation).
	for (int i = 0; i < 30; ++i)
	{
		xFix.BeginFrame();
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_MOUSE_BUTTON_RIGHT);
		xFix.Step();
	}
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_MOUSE_BUTTON_RIGHT);

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xFix.xScene);
	Zenith_Entity xPlayer = pxSceneData->GetEntity(xFix.uPlayerID);
	Zenith_Maths::Quat xRot;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);
	const float fPlayerYaw = atan2f(2.0f * (xRot.w * xRot.y + xRot.x * xRot.z),
	                                1.0f - 2.0f * (xRot.y * xRot.y + xRot.x * xRot.x));

	// The camera-yaw and the player's transform-yaw use opposite-sign
	// conventions: a camera yaw of -pi/2 (mouse moved right) puts the camera
	// at world -X behind a player who must face +X — that's player yaw +pi/2.
	// So expect fPlayerYaw == -fCamCanonical.
	const float fTwoPi = static_cast<float>(Zenith_Maths::Pi * 2.0);
	float fCamCanonical = fCamYaw;
	if (fCamCanonical > Zenith_Maths::Pi) fCamCanonical -= fTwoPi;

	ZENITH_ASSERT_TRUE(std::abs(fPlayerYaw - (-fCamCanonical)) < 0.1f,
		"Player transform-yaw should converge to -(camera-yaw) while ADS-ing");
}

// ----- ADS + Fire -----------------------------------------------------------

ZENITH_TEST(RenderTestInput, AimingFlagSetByRMB)
{ Zenith_UnitTests::TestRenderTestAimingFlagSetByRMB(); }
void Zenith_UnitTests::TestRenderTestAimingFlagSetByRMB()
{
	RenderTest_TestFixture xFix;

	xFix.BeginFrame();
	ZENITH_ASSERT_FALSE(RenderTest_GameplayState::IsLocalPlayerAiming(),
		"Aiming flag should start false");

	Zenith_InputSimulator::SimulateKeyDown(ZENITH_MOUSE_BUTTON_RIGHT);
	xFix.Step();
	ZENITH_ASSERT_TRUE(RenderTest_GameplayState::IsLocalPlayerAiming(),
		"Aiming flag should be true while RMB held");

	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_MOUSE_BUTTON_RIGHT);
	xFix.Step();

	// ADS may persist briefly via m_fForceAimTimer if a fire happened; with no
	// fire involved here it should drop the same frame RMB is released.
	ZENITH_ASSERT_FALSE(RenderTest_GameplayState::IsLocalPlayerAiming(),
		"Aiming flag should drop when RMB released without prior fire");
}

ZENITH_TEST(RenderTestInput, FireDecrementsAmmo)
{ Zenith_UnitTests::TestRenderTestFireDecrementsAmmo(); }
void Zenith_UnitTests::TestRenderTestFireDecrementsAmmo()
{
	RenderTest_TestFixture xFix;

	// Force Shoot() to early-return: clear the prefab handle for the duration
	// of the test so we don't drag the entire bullet+physics+model load chain
	// into the test environment. The ammo decrement happens AFTER Shoot()
	// returns, so we still observe the bookkeeping change. Restored on exit.
	Zenith_AssetHandle<Zenith_Prefab> xSavedPrefab = RenderTest::g_xBulletPrefab;
	RenderTest::g_xBulletPrefab = Zenith_AssetHandle<Zenith_Prefab>();

	const uint32_t uStartAmmo = xFix.pxPlayer->GetAmmoInClip();
	ZENITH_ASSERT_TRUE(uStartAmmo > 0, "Mag should start non-empty");

	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step();

	const uint32_t uAfter = xFix.pxPlayer->GetAmmoInClip();
	RenderTest::g_xBulletPrefab = xSavedPrefab;

	ZENITH_ASSERT_TRUE(uAfter == uStartAmmo - 1,
		"Ammo should decrement by 1 on a single LMB press");
}

ZENITH_TEST(RenderTestInput, FireRespectsCooldown)
{ Zenith_UnitTests::TestRenderTestFireRespectsCooldown(); }
void Zenith_UnitTests::TestRenderTestFireRespectsCooldown()
{
	RenderTest_TestFixture xFix;
	Zenith_AssetHandle<Zenith_Prefab> xSavedPrefab = RenderTest::g_xBulletPrefab;
	RenderTest::g_xBulletPrefab = Zenith_AssetHandle<Zenith_Prefab>();

	// First shot: hits the cooldown gate from cold (cooldown=0).
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step();
	const uint32_t uAfterFirst = xFix.pxPlayer->GetAmmoInClip();

	// Second click on the very next frame: cooldown is still ~0.12s, should
	// be blocked. Step with tiny dt so cooldown doesn't drain.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step(0.001f);
	const uint32_t uAfterSecond = xFix.pxPlayer->GetAmmoInClip();

	RenderTest::g_xBulletPrefab = xSavedPrefab;

	ZENITH_ASSERT_TRUE(uAfterFirst == uAfterSecond,
		"Second click within fire-cooldown window must NOT decrement ammo");
}

ZENITH_TEST(RenderTestInput, AutoReloadOnEmptyClick)
{ Zenith_UnitTests::TestRenderTestAutoReloadOnEmptyClick(); }
void Zenith_UnitTests::TestRenderTestAutoReloadOnEmptyClick()
{
	RenderTest_TestFixture xFix;
	Zenith_AssetHandle<Zenith_Prefab> xSavedPrefab = RenderTest::g_xBulletPrefab;
	RenderTest::g_xBulletPrefab = Zenith_AssetHandle<Zenith_Prefab>();

	// Drain the clip to zero by repeatedly firing with tiny dt steps to dodge
	// the cooldown — drain is tested separately; here we just need to set up
	// the empty-clip state.
	while (xFix.pxPlayer->GetAmmoInClip() > 0)
	{
		xFix.BeginFrame();
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
		xFix.Step(1.0f);  // big dt to drain cooldown
	}
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->GetAmmoInClip() == 0, "Clip should be empty");
	ZENITH_ASSERT_FALSE(xFix.pxPlayer->IsReloading(), "Should not be reloading yet");

	// Empty-clip click should auto-start a reload, NOT fire.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step();
	const bool bReloading = xFix.pxPlayer->IsReloading();
	RenderTest::g_xBulletPrefab = xSavedPrefab;
	ZENITH_ASSERT_TRUE(bReloading,
		"Empty-clip click should auto-start a reload");
}

ZENITH_TEST(RenderTestInput, HipfireReloadRaisesAimLayer)
{ Zenith_UnitTests::TestRenderTestHipfireReloadRaisesAimLayer(); }
void Zenith_UnitTests::TestRenderTestHipfireReloadRaisesAimLayer()
{
	RenderTest_TestFixture xFix;
	Zenith_AssetHandle<Zenith_Prefab> xSavedPrefab = RenderTest::g_xBulletPrefab;
	RenderTest::g_xBulletPrefab = Zenith_AssetHandle<Zenith_Prefab>();

	// Burn one shot from hipfire so a reload is allowed (R is a no-op when
	// the clip is already full). RMB is NOT held — pure hipfire reload path,
	// which previously left the aim layer at weight 0 because the SM had no
	// Hipfire->Reload transition AND the weight ramp ignored the reload state.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step(1.0f);

	// LMB triggers a 0.4s force-aim timer that ramps the layer to 1 for the
	// fire animation. Settle back down with normal-dt frames before checking
	// the "weight is near 0 in hipfire" precondition.
	for (int i = 0; i < 60; ++i)
	{
		xFix.BeginFrame();
		xFix.Step();
	}

	const float fWeightBeforeReload = xFix.pxPlayer->GetAimLayerWeight();
	ZENITH_ASSERT_TRUE(fWeightBeforeReload < 0.05f,
		"Aim layer should be near 0 before reload while hipfire");

	// Press R from hipfire.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
	xFix.Step();
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->IsReloading(),
		"R from hipfire must start a reload");

	// Step a few more frames so the layer-weight lerp has time to ramp.
	for (int i = 0; i < 20; ++i)
	{
		xFix.BeginFrame();
		xFix.Step();
	}

	const float fWeightDuringReload = xFix.pxPlayer->GetAimLayerWeight();
	RenderTest::g_xBulletPrefab = xSavedPrefab;

	ZENITH_ASSERT_TRUE(fWeightDuringReload > 0.8f,
		"Aim layer must ramp up while reloading from hipfire so the reload "
		"clip is visible");
}

ZENITH_TEST(RenderTestInput, ReloadBlocksFire)
{ Zenith_UnitTests::TestRenderTestReloadBlocksFire(); }
void Zenith_UnitTests::TestRenderTestReloadBlocksFire()
{
	RenderTest_TestFixture xFix;
	Zenith_AssetHandle<Zenith_Prefab> xSavedPrefab = RenderTest::g_xBulletPrefab;
	RenderTest::g_xBulletPrefab = Zenith_AssetHandle<Zenith_Prefab>();

	// Burn one shot so the clip is below max (R is otherwise no-op).
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step(1.0f);  // big dt drains cooldown
	const uint32_t uAmmoAfterFirstShot = xFix.pxPlayer->GetAmmoInClip();

	// Press R to reload.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
	xFix.Step();
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->IsReloading(), "Reload should be in progress");

	// LMB during reload should NOT fire — ammo stays put.
	xFix.BeginFrame();
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
	xFix.Step();
	const uint32_t uAfterClickDuringReload = xFix.pxPlayer->GetAmmoInClip();
	RenderTest::g_xBulletPrefab = xSavedPrefab;
	ZENITH_ASSERT_TRUE(uAfterClickDuringReload == uAmmoAfterFirstShot,
		"Ammo must not decrement while reloading");
}

#endif // ZENITH_INPUT_SIMULATOR
