#include "Core/Zenith_Engine.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"

#include "RenderTest/Components/RenderTest_FollowCameraComponent.h"
#include "RenderTest/Components/RenderTest_PlayerComponent.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// RenderTest input-simulator tests
//
// Drives the FollowCamera and Player components via Zenith_InputSimulator
// and asserts the gameplay-visible state changes. Each test owns a fresh empty
// scene + minimal Player + GameManager entities; the components are added via
// AddComponent and their lifecycle hooks are called directly so we can drive
// them deterministically without spinning up a full Play-mode loop.
//
// Frame model:
//   - BeginFrame() — mirrors Zenith_Core::Zenith_MainLoop's first step: it
//     calls g_xEngine.Input().BeginFrame() which auto-releases SimulateKeyPress'd
//     keys from the prior frame, recomputes mouse delta from the simulator
//     position, and DISCARDS anything the simulator has queued. Inputs simulated
//     AFTER this point and BEFORE Step() are read.
//   - Step()      — closes the frame's ACTION contract, then runs one
//     OnUpdate(player) + OnLateUpdate(camera) tick.
//
// ★ C1b — THE ACTION FRAME IS THE FIXTURE'S JOB HERE. Since the WP4b migration
// both components read g_xEngine.Actions() (MOVE / SPRINT / JUMP / AIM /
// LOOK_DELTA / LOOK_RATE) rather than g_xEngine.Input(), and the action layer is
// a FRAME CONTRACT: the engine's instance is opened at frame step 8 and closed
// at 10b/10e inside Zenith_Core::Zenith_MainLoop, which a unit test never runs.
// Without CloseActionFrame() below, every action would read "not held", the
// player would never move, the camera would never turn, and each test would fail
// with a message about rotation or pitch rather than about input. This is the
// same seam ZM_BindingsTest::CloseEngineActionFrame provides for Zenithmon's
// controller units — and, like that one, it means this fixture touches the
// ENGINE's live layers and therefore owes them a reset on teardown.
// ============================================================================

namespace
{
	struct RenderTest_TestFixture
	{
		Zenith_Scene xScene;
		Zenith_EntityID uPlayerID = INVALID_ENTITY_ID;
		Zenith_EntityID uGameManagerID = INVALID_ENTITY_ID;
		RenderTest_FollowCameraComponent* pxCamera = nullptr;
		RenderTest_PlayerComponent*       pxPlayer = nullptr;

		RenderTest_TestFixture()
		{
			Zenith_InputSimulator::Enable();
			// Start from a known device state whatever ran before: a key an
			// earlier fixture left down would otherwise still be held on the
			// action layer's transition-fed source shadow.
			Zenith_InputSimulator::DiscardPendingInjections();
			Zenith_InputSimulator::ResetAllInputState();
			RenderTest_GameplayState::Reset();

			xScene = g_xEngine.Scenes().LoadScene("RenderTestInputTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);

			// Player entity: Transform (auto-added) + Collider, name "Player"
			// so FollowCamera can find it by name.
			Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "Player");
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition({ 0.0f, 0.0f, 0.0f });
			xPlayer.AddComponent<Zenith_ColliderComponent>();
			uPlayerID = xPlayer.GetEntityID();

			// GameManager entity: Camera component for the FollowCamera.
			Zenith_Entity xGameManager = g_xEngine.Scenes().CreateEntity(pxSceneData, "GameManager");
			xGameManager.AddComponent<Zenith_CameraComponent>();
			uGameManagerID = xGameManager.GetEntityID();

			// Add the gameplay components and drive their lifecycle hooks
			// directly. No AnimatorComponent is added (and the scene system
			// never dispatches OnStart here) because
			// RenderTest_PlayerComponent::OnStart loads .zanim files and builds
			// a layered animator — heavy setup we don't need for input-driven
			// tests. Pointers into the component pools are stable for the
			// fixture's lifetime: no further components of these types are
			// added, so the pools never grow/relocate.
			pxCamera = &xGameManager.AddComponent<RenderTest_FollowCameraComponent>();
			pxPlayer = &xPlayer.AddComponent<RenderTest_PlayerComponent>();

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
			// ★ BOTH SIDES, IN THIS ORDER. ResetAllInputState drops the
			// simulator's LEVEL table without emitting the releases, so the
			// action layer's shadow — fed by TRANSITIONS alone — would keep the
			// last key held forever and drive the NEXT unit's player. (Several
			// tests below deliberately end with a SimulateKeyUp that no Step
			// ever consumes, which is exactly that case.) The engine resets that
			// follow are what the automated-test harness does between tests; a
			// boot unit that reached the engine layers owes the same.
			Zenith_InputSimulator::DiscardPendingInjections();
			Zenith_InputSimulator::ResetAllInputState();
			Zenith_InputSimulator::Disable();
			g_xEngine.Input().ResetTransientForTest();
			g_xEngine.Actions().ResetTransientForTest();
			g_xEngine.Pointers().ResetTransientForTest();

			// Components are owned by the scene; UnloadSceneForced tears them
			// down (dispatching OnDisable/OnDestroy through the meta registry).
			g_xEngine.Scenes().UnloadSceneForced(xScene);
		}

		// Mirrors what Zenith_Core::Zenith_MainLoop's prologue does: clears
		// per-frame press flags, auto-releases SimulateKeyPress'd keys, updates
		// mouse delta from the simulator's current position, and DISCARDS the
		// simulator's queued injections — which is why every Simulate* call in
		// these tests happens AFTER this and BEFORE the matching Step().
		void BeginFrame()
		{
			g_xEngine.Input().BeginFrame();
		}

		// Frame-contract steps 7, 8, 10b and 10e on the ENGINE's own layers:
		// drain the injections, open the action frame, close both stages. See
		// the C1b note at the top of this file for why a unit has to do this
		// itself.
		void CloseActionFrame()
		{
			g_xEngine.Input().ApplySimulatorInjection();
			g_xEngine.Actions().UpdateProfile();
			g_xEngine.Actions().FinalizeReservedUI();
			g_xEngine.Actions().FinalizeGameplay();
		}

		// Runs one update tick. The action frame closes first (game logic is
		// frame-contract step 11, after 10e). The player updates before the
		// camera because the camera reads
		// RenderTest_GameplayState::IsLocalPlayerAiming() which the player
		// writes during its OnUpdate.
		void Step(float fDt = 1.0f / 60.0f)
		{
			CloseActionFrame();
			pxPlayer->OnUpdate(fDt);
			pxCamera->OnLateUpdate(fDt);
		}

		Zenith_Maths::Vector3 GetPlayerPosition() const
		{
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
			Zenith_Entity xPlayer = pxSceneData->GetEntity(uPlayerID);
			Zenith_Maths::Vector3 xPos;
			xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			return xPos;
		}

		void SetPlayerPosition(const Zenith_Maths::Vector3& xPos)
		{
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
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
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xFix.xScene);
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
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xFix.xScene);
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

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xFix.xScene);
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

// W3 NOTE: the LMB-fire / R-reload PRESS dispatch moved into
// RenderTest_PlayerActions.bgraph (OnMouseButton/OnKeyPressed -> RTPlayerTryFire/
// RTPlayerTryReload); the press->verb wiring is pinned by the RT_PlayerActions
// windowed characterization. These units pin the VERB semantics directly (the
// graph calls the same TryFire/TryStartReload the tests call, at the same
// pre-OnUpdate point in the frame).

ZENITH_TEST(RenderTestInput, FireDecrementsAmmo)
{ Zenith_UnitTests::TestRenderTestFireDecrementsAmmo(); }
void Zenith_UnitTests::TestRenderTestFireDecrementsAmmo()
{
	RenderTest_TestFixture xFix;

	const uint32_t uStartAmmo = xFix.pxPlayer->GetAmmoInClip();
	ZENITH_ASSERT_TRUE(uStartAmmo > 0, "Mag should start non-empty");

	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step();

	const uint32_t uAfter = xFix.pxPlayer->GetAmmoInClip();

	ZENITH_ASSERT_TRUE(uAfter == uStartAmmo - 1,
		"Ammo should decrement by 1 on a single fire verb");
}

ZENITH_TEST(RenderTestInput, FireRespectsCooldown)
{ Zenith_UnitTests::TestRenderTestFireRespectsCooldown(); }
void Zenith_UnitTests::TestRenderTestFireRespectsCooldown()
{
	RenderTest_TestFixture xFix;

	// First shot: hits the cooldown gate from cold (cooldown=0).
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step();
	const uint32_t uAfterFirst = xFix.pxPlayer->GetAmmoInClip();

	// Second fire on the very next frame: cooldown is still ~0.12s, should
	// be blocked. Step with tiny dt so cooldown doesn't drain.
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step(0.001f);
	const uint32_t uAfterSecond = xFix.pxPlayer->GetAmmoInClip();

	ZENITH_ASSERT_TRUE(uAfterFirst == uAfterSecond,
		"Second fire within the cooldown window must NOT decrement ammo");
}

ZENITH_TEST(RenderTestInput, AutoReloadOnEmptyClick)
{ Zenith_UnitTests::TestRenderTestAutoReloadOnEmptyClick(); }
void Zenith_UnitTests::TestRenderTestAutoReloadOnEmptyClick()
{
	RenderTest_TestFixture xFix;

	// Drain the clip to zero — big-dt steps drain the cooldown between shots.
	while (xFix.pxPlayer->GetAmmoInClip() > 0)
	{
		xFix.BeginFrame();
		xFix.pxPlayer->TryFire();
		xFix.Step(1.0f);  // big dt to drain cooldown
	}
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->GetAmmoInClip() == 0, "Clip should be empty");
	ZENITH_ASSERT_FALSE(xFix.pxPlayer->IsReloading(), "Should not be reloading yet");

	// Empty-clip fire-attempt should auto-start a reload, NOT fire.
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step();
	const bool bReloading = xFix.pxPlayer->IsReloading();
	ZENITH_ASSERT_TRUE(bReloading,
		"Empty-clip fire-attempt should auto-start a reload");
}

ZENITH_TEST(RenderTestInput, HipfireReloadRaisesAimLayer)
{ Zenith_UnitTests::TestRenderTestHipfireReloadRaisesAimLayer(); }
void Zenith_UnitTests::TestRenderTestHipfireReloadRaisesAimLayer()
{
	RenderTest_TestFixture xFix;

	// Burn one shot from hipfire so a reload is allowed (R is a no-op when
	// the clip is already full). RMB is NOT held — pure hipfire reload path,
	// which previously left the aim layer at weight 0 because the SM had no
	// Hipfire->Reload transition AND the weight ramp ignored the reload state.
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
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

	// Reload verb from hipfire.
	xFix.BeginFrame();
	xFix.pxPlayer->TryStartReload();
	xFix.Step();
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->IsReloading(),
		"the reload verb from hipfire must start a reload");

	// Step a few more frames so the layer-weight lerp has time to ramp.
	for (int i = 0; i < 20; ++i)
	{
		xFix.BeginFrame();
		xFix.Step();
	}

	const float fWeightDuringReload = xFix.pxPlayer->GetAimLayerWeight();

	ZENITH_ASSERT_TRUE(fWeightDuringReload > 0.8f,
		"Aim layer must ramp up while reloading from hipfire so the reload "
		"clip is visible");
}

ZENITH_TEST(RenderTestInput, ReloadBlocksFire)
{ Zenith_UnitTests::TestRenderTestReloadBlocksFire(); }
void Zenith_UnitTests::TestRenderTestReloadBlocksFire()
{
	RenderTest_TestFixture xFix;

	// Burn one shot so the clip is below max (reload is otherwise no-op).
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step(1.0f);  // big dt drains cooldown
	const uint32_t uAmmoAfterFirstShot = xFix.pxPlayer->GetAmmoInClip();

	// Start the reload.
	xFix.BeginFrame();
	xFix.pxPlayer->TryStartReload();
	xFix.Step();
	ZENITH_ASSERT_TRUE(xFix.pxPlayer->IsReloading(), "Reload should be in progress");

	// A fire-attempt during reload should NOT fire — ammo stays put.
	xFix.BeginFrame();
	xFix.pxPlayer->TryFire();
	xFix.Step();
	const uint32_t uAfterClickDuringReload = xFix.pxPlayer->GetAmmoInClip();
	ZENITH_ASSERT_TRUE(uAfterClickDuringReload == uAmmoAfterFirstShot,
		"Ammo must not decrement while reloading");
}

// ----- IK helper robustness -------------------------------------------------

ZENITH_TEST(RenderTestInput, IKHelperEarlyOutsWithoutAnimator)
{ Zenith_UnitTests::TestRenderTestIKHelperEarlyOutsWithoutAnimator(); }
void Zenith_UnitTests::TestRenderTestIKHelperEarlyOutsWithoutAnimator()
{
	// The fixture deliberately doesn't add an AnimatorComponent (loading the
	// .zanim files + building a layered animator is heavy setup we skip for
	// input-driven tests). UpdateFootIK is called at the END of OnUpdate; if
	// the m_pxAnimator nullcheck is missing, this would null-deref through
	// pxAnimator->ClearIKTarget. The fixture destructor cleanly tearing down
	// after several update ticks is the test pass.
	RenderTest_TestFixture xFix;

	xFix.BeginFrame();
	for (int i = 0; i < 5; ++i)
	{
		xFix.Step();
		xFix.BeginFrame();
	}
	ZENITH_ASSERT_TRUE(true, "UpdateFootIK with no animator must not crash");
}

// ----- Jetpack thrust -------------------------------------------------------

ZENITH_TEST(RenderTestInput, JetpackThrustRaisesAndCapsVy)
{
	// Exercise the pure thrust integrator directly. (A full in-engine thrust test
	// is impossible from this fixture for TWO reasons: the Player here has a
	// ColliderComponent with NO valid body — OnStart, which adds the capsule, is
	// never called — so every SetLinearVelocity is a no-op; and no jetpack entity
	// exists so the gating predicate is false. The decision logic is covered
	// separately by JetpackGatingRequiresEquipAndInput below.)
	const float fDt = 1.0f / 60.0f;

	const float fCap  = RenderTest_PlayerComponent::GetJetpackMaxAscent();
	const float fJump = RenderTest_PlayerComponent::GetJumpVelocity();

	// From rest, one thrust frame adds upward velocity.
	const float fAfterOne = RenderTest_PlayerComponent::ApplyJetpackThrust(0.0f, fDt);
	ZENITH_ASSERT_TRUE(fAfterOne > 0.0f,
		"Jetpack thrust must raise vertical velocity from rest");

	// A falling player (negative vy) is pulled back upward by thrust.
	const float fFromFall = RenderTest_PlayerComponent::ApplyJetpackThrust(-5.0f, fDt);
	ZENITH_ASSERT_TRUE(fFromFall > -5.0f,
		"Jetpack thrust must add upward velocity even while falling");

	// Sustained thrust converges to — and is capped at — the ascent ceiling, so
	// the climb is controlled rather than runaway. Assert convergence to the REAL
	// constant (not a literal) so a retune of the cap can't silently invalidate it.
	float fVy = 0.0f;
	for (int i = 0; i < 600; ++i)   // ~10s of held thrust
		fVy = RenderTest_PlayerComponent::ApplyJetpackThrust(fVy, fDt);
	ZENITH_ASSERT_TRUE(fVy <= fCap + 1e-4f,
		"Ascent velocity must be capped at the jetpack ceiling");
	ZENITH_ASSERT_TRUE(std::abs(fVy - fCap) < 1e-3f,
		"Sustained thrust must converge to the ascent ceiling");

	// The load-bearing invariant: the cap MUST sit strictly above the jump pop.
	// The jetpack thrust block runs the same frame as a grounded jump (held Space
	// satisfies both), so if the cap were <= the jump velocity the pop would be
	// clamped flat. Asserting the RELATIONSHIP between the real constants (not a
	// stale literal) makes any future retune that violates it fail loudly.
	ZENITH_ASSERT_TRUE(fCap > fJump,
		"Ascent ceiling must exceed the jump velocity so the jump pop is preserved");
}

ZENITH_TEST(RenderTestInput, JetpackGatingRequiresEquipAndInput)
{
	// The pure gating predicate is the "ground movement unchanged" guard: without a
	// jetpack equipped, NOTHING the player does (including holding Space) may engage
	// thrust. This is exactly the condition the OnUpdate thrust block evaluates.
	using P = RenderTest_PlayerComponent;

	// No jetpack worn => never thrust, regardless of Space / showcase.
	ZENITH_ASSERT_FALSE(P::ShouldEngageJetpack(false, false, false),
		"Unequipped + no input must not thrust");
	ZENITH_ASSERT_FALSE(P::ShouldEngageJetpack(false, true, false),
		"Unequipped + Space held must NOT thrust (ground movement unchanged)");
	ZENITH_ASSERT_FALSE(P::ShouldEngageJetpack(false, true, true),
		"Unequipped must not thrust even under the showcase force");

	// Equipped: thrust requires Space held OR the showcase force.
	ZENITH_ASSERT_FALSE(P::ShouldEngageJetpack(true, false, false),
		"Equipped but no input must not thrust");
	ZENITH_ASSERT_TRUE(P::ShouldEngageJetpack(true, true, false),
		"Equipped + Space held must thrust");
	ZENITH_ASSERT_TRUE(P::ShouldEngageJetpack(true, false, true),
		"Equipped + showcase force must thrust (capture path)");
}

// ----- Layered-animator idempotence (A4) ------------------------------------
//
// ★ A SEPARATE FIXTURE, AND IT HAS TO BE. RenderTest_TestFixture above
// deliberately adds NO Zenith_AnimatorComponent and never calls OnStart — see its
// own comment: OnStart loads eight .zanim files and builds a two-layer animator,
// which the input-driven tests neither need nor want. These three cases are about
// exactly that setup, so they build their own entity: model + animator + collider
// + the player component, with the animator's own OnStart run once up front so it
// discovers the skeleton off the model the way the ECS lifecycle would.
//
// THE DEFECT UNDER TEST. RenderTest_PlayerComponent::OnStart calls
// SetupLayeredAnimator, which used to call Flux_AnimationController::AddLayer for
// "BaseLayer" and "AimLayer" unguarded. AddLayer appends unconditionally and layer
// names are not unique by design, so a SECOND OnStart over the same store-owned
// controller left four layers — two stale, two new. That second OnStart is not
// hypothetical and it is not a scene reload (a reload builds a fresh component
// beside a fresh controller): Zenith_Editor::EnterPlayMode dispatches OnAwake and
// OnStart UNCONDITIONALLY over every live entity
// (Zenith/Editor/Zenith_Editor_SceneOps.cpp:113-129, documented at :378-381), so a
// Stopped->Playing transition taken over an already-started world re-starts every
// component in it. `OnAwake(); OnStart();` twice IS that shape.
namespace
{
	struct RenderTest_AnimatorFixture
	{
		Zenith_Scene xScene;
		Zenith_EntityID uPlayerID = INVALID_ENTITY_ID;
		// The StickFigure rig is BAKE output, not committed:
		// GenerateStickFigureAssets writes it under ENGINE_ASSETS_DIR on every
		// tools boot, before Flux comes up (Zenith/Core/Zenith_Engine.cpp:573).
		std::string strModelPath;
		// Did the animator actually bind to a skeleton instance? Only the event
		// case needs this — Flux_AnimationController::Update early-outs without one
		// (Flux_AnimationController.cpp:315) — and it ASSERTS on it rather than
		// skipping, so a cold tree fails loudly instead of counting 0 == 0. The two
		// narrower flags exist so that failure names WHICH step gave out.
		bool bModelLoaded = false;
		bool bModelHasSkeleton = false;
		bool bRigWarm = false;

		RenderTest_AnimatorFixture()
		{
			RenderTest_GameplayState::Reset();

			xScene = g_xEngine.Scenes().LoadScene("RenderTestAnimatorTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);

			Zenith_Entity xPlayer = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimPlayer");
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition({ 0.0f, 0.0f, 0.0f });
			uPlayerID = xPlayer.GetEntityID();

			// LoadModel reports a missing file and returns (Zenith_ModelComponent.cpp:178-183)
			// rather than asserting, so the cold case reaches bRigWarm == false instead
			// of taking the process down.
			strModelPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MODEL_EXT;
			Zenith_ModelComponent& xModel = xPlayer.AddComponent<Zenith_ModelComponent>();
			xModel.LoadModel(strModelPath);
			bModelLoaded = xModel.HasModel();
			bModelHasSkeleton = bModelLoaded && xModel.HasSkeleton();

			// The player's own OnStart adds the capsule when no body exists yet, so
			// a bare ColliderComponent is all this needs; TryFire/TryInteractGun
			// early-out without one.
			xPlayer.AddComponent<Zenith_ColliderComponent>();

			Zenith_AnimatorComponent& xAnimator = xPlayer.AddComponent<Zenith_AnimatorComponent>();
			xAnimator.OnStart();   // TryDiscoverSkeleton off the ModelComponent
			bRigWarm = xAnimator.GetController().IsInitialized();

			xPlayer.AddComponent<RenderTest_PlayerComponent>();
		}

		~RenderTest_AnimatorFixture()
		{
			// Components are owned by the scene; UnloadSceneForced tears them down
			// (dispatching OnDisable/OnDestroy through the meta registry), which is
			// also what destroys the store-owned animation controller.
			g_xEngine.Scenes().UnloadSceneForced(xScene);
		}

		RenderTest_AnimatorFixture(const RenderTest_AnimatorFixture&) = delete;
		RenderTest_AnimatorFixture& operator=(const RenderTest_AnimatorFixture&) = delete;

		// Re-fetched per use rather than cached: several component pools are touched
		// here and a pointer into one is only as stable as the next AddComponent.
		Zenith_Entity Entity() const
		{
			return g_xEngine.Scenes().GetSceneData(xScene)->GetEntity(uPlayerID);
		}
		RenderTest_PlayerComponent& Player() const
		{
			return Entity().GetComponent<RenderTest_PlayerComponent>();
		}
		Flux_AnimationController& Controller() const
		{
			return Entity().GetComponent<Zenith_AnimatorComponent>().GetController();
		}

		// The EnterPlayMode shape, spelled once: OnAwake (which resets
		// m_uBaseLayerId / m_uAimLayerId to uFLUX_INVALID_LAYER_ID) then OnStart,
		// over the SAME live controller.
		void RestartPlayerComponent() const
		{
			Player().OnAwake();
			Player().OnStart();
		}
	};

	struct RTA4_EventSink
	{
		Zenith_Vector<std::string> m_xNames;

		u_int CountOf(const char* szName) const
		{
			u_int uCount = 0;
			for (u_int u = 0; u < m_xNames.GetSize(); ++u)
			{
				if (m_xNames.Get(u) == szName)
					uCount++;
			}
			return uCount;
		}

		void Reset() { m_xNames.Clear(); }
	};

	void RTA4_OnEvent(void* pUserData, const std::string& strEventName, const Zenith_Maths::Vector4&)
	{
		static_cast<RTA4_EventSink*>(pUserData)->m_xNames.PushBack(strEventName);
	}

	// ★ THE CLIP THIS ADDS AN EVENT TO IS THE SHARED REGISTRY ASSET'S.
	// Flux_AnimationController::AddClipFromFile stores a NON-owning reference to
	// Zenith_AnimationAsset::GetClip() (Flux_AnimationController.cpp:525-553), so a
	// probe event left behind would be visible to every controller in the process
	// and to every test that runs after this one. Hence RAII, on every return path.
	//
	// Removal is BY NAME, not by the index we pushed at: Flux_AnimationClip::AddEvent
	// re-sorts the whole vector by time (Flux_AnimationClip.cpp:1440-1448).
	struct RTA4_ScopedClipEvent
	{
		Flux_AnimationClip* m_pxClip = nullptr;
		std::string m_strName;

		RTA4_ScopedClipEvent(Flux_AnimationClip* pxClip, const char* szName, float fNormalizedTime)
			: m_pxClip(pxClip)
			, m_strName(szName)
		{
			if (m_pxClip == nullptr)
				return;
			Flux_AnimationEvent xEvent;
			xEvent.m_strEventName = m_strName;
			xEvent.m_fNormalizedTime = fNormalizedTime;
			xEvent.m_xData = Zenith_Maths::Vector4(fNormalizedTime, 0.0f, 0.0f, 0.0f);
			m_pxClip->AddEvent(xEvent);
		}

		~RTA4_ScopedClipEvent()
		{
			if (m_pxClip == nullptr)
				return;
			for (u_int u = m_pxClip->GetEvents().GetSize(); u > 0; --u)
			{
				if (m_pxClip->GetEvents().Get(u - 1).m_strEventName == m_strName)
					m_pxClip->RemoveEvent(u - 1);
			}
		}

		RTA4_ScopedClipEvent(const RTA4_ScopedClipEvent&) = delete;
		RTA4_ScopedClipEvent& operator=(const RTA4_ScopedClipEvent&) = delete;
	};
}

// (A) LAYER IDENTITY. A second OnStart must leave exactly the two layers the
// first one built — same ids, same live state machines — not four.
ZENITH_TEST(RenderTestInput, LayeredAnimatorAdoptsItsLayersOnARepeatedStart)
{
	RenderTest_AnimatorFixture xFix;

	xFix.RestartPlayerComponent();

	Flux_AnimationController& xController = xFix.Controller();
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u,
		"the player authors exactly TWO layers (got %u)", xController.GetLayerCount());

	Flux_AnimationLayer* pxBase = xController.GetLayer(0u);
	Flux_AnimationLayer* pxAim  = xController.GetLayer(1u);
	ZENITH_ASSERT_NOT_NULL(pxBase, "layer 0 is missing after a successful OnStart");
	ZENITH_ASSERT_NOT_NULL(pxAim,  "layer 1 is missing after a successful OnStart");
	if (pxBase == nullptr || pxAim == nullptr) { return; }

	// BaseLayer must stay FIRST: HumanShowcase reads the base state machine off
	// GetLayer(0) by index (Games/RenderTest/Tests/HumanShowcase.cpp:97-108).
	ZENITH_ASSERT_STREQ(pxBase->GetName().c_str(), "BaseLayer",
		"BaseLayer must be the first layer added — GetLayer(0) is read by index elsewhere");
	ZENITH_ASSERT_STREQ(pxAim->GetName().c_str(), "AimLayer",
		"AimLayer must be the second layer added");

	const u_int uBaseId = pxBase->GetLayerId();
	const u_int uAimId  = pxAim->GetLayerId();
	ZENITH_ASSERT_NE(uBaseId, uAimId, "two layers of one controller must not share an id");
	const Flux_AnimationStateMachine* pxBaseSM = pxBase->GetStateMachinePtr();
	const Flux_AnimationStateMachine* pxAimSM  = pxAim->GetStateMachinePtr();
	ZENITH_ASSERT_NOT_NULL(pxBaseSM, "the base state machine was never created");
	ZENITH_ASSERT_NOT_NULL(pxAimSM,  "the aim state machine was never created");

	// ...and again, over the same controller.
	xFix.RestartPlayerComponent();

	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u,
		"a second OnStart DUPLICATED the layers (2 -> %u). Flux_AnimationController::AddLayer "
		"appends unconditionally and layer names are not unique, so the stale pair is still "
		"ticked by EvaluateAndComposeLayers and still emits events",
		xController.GetLayerCount());
	if (xController.GetLayerCount() != 2u) { return; }

	ZENITH_ASSERT_EQ(xController.GetLayer(0u)->GetLayerId(), uBaseId,
		"layer 0's id moved across a repeated OnStart — the base layer was rebuilt, not adopted");
	ZENITH_ASSERT_EQ(xController.GetLayer(1u)->GetLayerId(), uAimId,
		"layer 1's id moved across a repeated OnStart — the aim layer was rebuilt, not adopted");

	// The opposite failure to duplication, and the reason the adopt branch must not
	// call CreateStateMachine: that DELETES the live machine
	// (Flux_AnimationLayer.cpp:64-69) along with whatever state the game is in.
	ZENITH_ASSERT_TRUE(xController.GetLayer(0u)->GetStateMachinePtr() == pxBaseSM,
		"the base layer's LIVE state machine was rebuilt by the second OnStart");
	ZENITH_ASSERT_TRUE(xController.GetLayer(1u)->GetStateMachinePtr() == pxAimSM,
		"the aim layer's LIVE state machine was rebuilt by the second OnStart");
	ZENITH_ASSERT_NOT_NULL(xController.GetLayer(0u)->GetStateMachinePtr(),
		"the second OnStart dropped the base state machine entirely");
}

// (B) THE COMPONENT STILL DRIVES THE ADOPTED LAYERS. This is the regression for a
// bare-early-return "fix": OnAwake resets m_uBaseLayerId / m_uAimLayerId to
// uFLUX_INVALID_LAYER_ID on the way into every start, so a SetupLayeredAnimator
// that merely returned when the layers already existed would leave both ids
// invalid — and every write in this component goes through
// `if (Flux_AnimationLayer* px = ResolveAimLayer())`, which then silently does
// nothing. Layer count and ids would look perfect; the player would animate in its
// idle pose forever.
//
// ★ GetAimLayerWeight() IS NOT A VALID PROBE FOR THIS. m_fAimLayerWeight is
// tracked unconditionally by OnUpdate, deliberately NOT gated on the layer
// existing (see the comment above the ramp), so it moves either way. The probes
// below are the two writes that only happen INSIDE the resolve: the trigger set on
// the aim layer's own state machine, and SetWeight on the layer object itself.
ZENITH_TEST(RenderTestInput, LayeredAnimatorAdoptedLayersStillTakeComponentWrites)
{
	RenderTest_AnimatorFixture xFix;

	xFix.RestartPlayerComponent();
	xFix.RestartPlayerComponent();   // the adopt path

	Flux_AnimationController& xController = xFix.Controller();
	Flux_AnimationLayer* pxAim = xController.GetLayerByName("AimLayer");
	ZENITH_ASSERT_NOT_NULL(pxAim, "no AimLayer to drive");
	if (pxAim == nullptr) { return; }

	// Preconditions, so neither assertion below can pass on a value that was
	// already there.
	ZENITH_ASSERT_FALSE(pxAim->GetStateMachine().GetParameters().PeekTrigger("FireTrigger"),
		"FireTrigger should be clear before the fire verb");
	ZENITH_ASSERT_EQ_FLOAT(pxAim->GetWeight(), 0.0f, 1.0e-5f,
		"the aim layer is authored at weight 0 and nothing has raised it yet");

	// TryFire writes FireTrigger through ResolveAimLayer() and sets the 0.4s
	// force-aim timer that raises the layer weight in OnUpdate.
	xFix.Player().TryFire();
	ZENITH_ASSERT_TRUE(pxAim->GetStateMachine().GetParameters().PeekTrigger("FireTrigger"),
		"the fire verb did not reach the ADOPTED aim layer — m_uAimLayerId was not "
		"re-adopted after OnAwake invalidated it, so ResolveAimLayer() returned null "
		"and the write was a silent no-op");

	// Ten frames of forced ADS: the ramp is clamp(dt * 6.66) per frame, so ~0.69
	// after ten frames of 1/60s, and the force-aim timer still has ~0.23s left.
	for (int i = 0; i < 10; ++i)
	{
		xFix.Player().OnUpdate(1.0f / 60.0f);
	}
	ZENITH_ASSERT_GT(pxAim->GetWeight(), 0.05f,
		"the per-LAYER SetWeight never reached the adopted aim layer (weight %.3f) — the "
		"aim pose would be composed at zero weight forever", pxAim->GetWeight());
}

// (C) EVENTS FIRE ONCE PER LAYER, NOT ONCE PER DUPLICATE. This is the clause a
// layer-count assertion cannot reach: a stale layer is at weight 0, so it composes
// nothing and looks harmless — but Flux_AnimationLayer::CollectEventSpans consults
// m_bEmitEvents alone and NEVER the weight (Flux_AnimationLayer.h:118-140), and
// Flux_AnimationController::DispatchClipEvents arbitrates per layer (D36). Four
// layers therefore mean every authored footstep / weapon beat fires twice.
ZENITH_TEST(RenderTestInput, LayeredAnimatorEventsFirePerLayerNotPerDuplicate)
{
	RenderTest_AnimatorFixture xFix;

	// ★ RIG-GATED, AND IT FAILS RATHER THAN SKIPS. Flux_AnimationController::Update
	// returns immediately without a live skeleton instance
	// (Flux_AnimationController.cpp:315), so on a cold tree every count below would
	// be 0 and "0 == 0" would report a pass for a test that ran nothing.
	ZENITH_ASSERT_TRUE(xFix.bRigWarm,
		"rig not baked — cannot verify: the animator never bound a skeleton instance, so "
		"the controller cannot be ticked and no event can fire (modelLoaded=%s "
		"modelHasSkeleton=%s). '%s' is written by GenerateStickFigureAssets on every "
		"ZENITH_TOOLS boot before Flux comes up (Zenith/Core/Zenith_Engine.cpp:573); "
		"reaching here without it is a broken bake, not a case to skip",
		xFix.bModelLoaded ? "yes" : "no",
		xFix.bModelHasSkeleton ? "yes" : "no",
		xFix.strModelPath.c_str());
	if (!xFix.bRigWarm) { return; }

	xFix.RestartPlayerComponent();

	Flux_AnimationController& xController = xFix.Controller();
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u,
		"expected the two authored layers before counting events (got %u)",
		xController.GetLayerCount());

	// Both layers play this ONE clip by default: BaseLayer's default state is
	// "Idle", and AimLayer's default "Hipfire" deliberately reuses the same clip
	// (the aim mask zeroes the locomotion-only channels).
	Flux_AnimationClip* pxIdle = xController.GetClip("Idle");
	ZENITH_ASSERT_NOT_NULL(pxIdle, "the Idle clip is not in the controller's collection");
	if (pxIdle == nullptr) { return; }
	const float fDuration = pxIdle->GetDuration();
	ZENITH_ASSERT_GT(fDuration, 0.0f,
		"a zero-duration clip has no normalized range for an event to sit in");
	// The step below is exactly ONE clip duration, which spans [t, 1) U [0, t) for
	// any starting phase t and therefore crosses every authored event exactly once
	// — that identity is what lets the second measurement start from wherever the
	// first one left the playhead, and it needs a LOOPING clip.
	ZENITH_ASSERT_TRUE(pxIdle->IsLooping(),
		"this test steps one whole clip duration per tick, which only fires every event "
		"exactly once from an arbitrary phase when the clip loops");
	if (fDuration <= 0.0f || !pxIdle->IsLooping()) { return; }

	RTA4_ScopedClipEvent xProbe(pxIdle, "RTA4_Probe", 0.5f);

	RTA4_EventSink xSink;
	xController.SetEventCallback(&RTA4_OnEvent, &xSink);

	xController.Update(fDuration);
	const u_int uAfterFirstStart = xSink.CountOf("RTA4_Probe");
	ZENITH_ASSERT_EQ(uAfterFirstStart, xController.GetLayerCount(),
		"one crossing should fire once per LAYER (D36): %u layers, %u events",
		xController.GetLayerCount(), uAfterFirstStart);

	// The EnterPlayMode shape again — and the event RATE must not move.
	xFix.RestartPlayerComponent();

	xSink.Reset();
	xController.Update(fDuration);
	const u_int uAfterSecondStart = xSink.CountOf("RTA4_Probe");

	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u,
		"a second OnStart duplicated the layers (now %u)", xController.GetLayerCount());
	ZENITH_ASSERT_EQ(uAfterSecondStart, uAfterFirstStart,
		"the event rate DOUBLED across a repeated OnStart (%u -> %u per crossing): the "
		"stale layers are at weight 0 and compose nothing, but they are still ticked and "
		"still emit — layer emission reads m_bEmitEvents, never the weight",
		uAfterFirstStart, uAfterSecondStart);

	xController.ClearEventCallback();
}

#endif // ZENITH_INPUT_SIMULATOR
