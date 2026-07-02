#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "../Components/DPOrbitCamera_Component.h"
#include "../Components/DPVillager_Component.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Core/Zenith_CommandLine.h"
#include "Flux/Flux_Screenshot.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_DPCamera_ModeTransition
//
// Pins the birds-eye <-> third-person camera contract added 2026-07-01:
//
//   Phase A (never possessed): after settling, blend-t must be EXACTLY 0
//     and the camera pose must be EXACTLY the classic orbit pose (the
//     regression guarantee for FrontEnd / gym / headless scenes).
//   Phase B (possess): mode flips to ThirdPerson, blend-t reaches 1, and
//     the camera converges to within a few metres of the villager (vs the
//     ~80 m orbit distance).
//   Phase C (unpossess): mode flips back, blend-t returns to 0, and the
//     pose re-matches the classic orbit pose exactly.
//   Phase D (manual toggle): with no possession, a C-press forces
//     ThirdPerson (using the last-possessed villager); a second C-press
//     forces back to BirdsEye.
//
// Possession uses DP_Player::SetPossessedVillager (the system path) — this
// is a camera-contract test, not a possession-input test; the
// personality-playthrough suite covers click-to-possess.
// ============================================================================

namespace
{
	bool g_bCamFailed = false;
	bool g_bCamDone = false;
	char g_szCamWhy[256] = {};

	Zenith_EntityID g_xCamVillager = INVALID_ENTITY_ID;

	void CamFail(const char* szWhy)
	{
		g_bCamFailed = true;
		std::snprintf(g_szCamWhy, sizeof(g_szCamWhy), "%s", szWhy);
	}

	DPOrbitCamera_Component* FindOrbitCam()
	{
		DPOrbitCamera_Component* pxFound = nullptr;
		DP_Query::ForEachComponentInActiveScene<DPOrbitCamera_Component>(
			[&pxFound](Zenith_EntityID, DPOrbitCamera_Component& xCam)
			{
				if (pxFound == nullptr) pxFound = &xCam;
			});
		return pxFound;
	}

	Zenith_CameraComponent* FindSceneCamera()
	{
		Zenith_CameraComponent* pxFound = nullptr;
		DP_Query::ForEachComponentInActiveScene<Zenith_CameraComponent>(
			[&pxFound](Zenith_EntityID, Zenith_CameraComponent& xCam)
			{
				if (pxFound == nullptr) pxFound = &xCam;
			});
		return pxFound;
	}

	// The classic orbit pose, recomputed independently of the component.
	Zenith_Maths::Vector3 ExpectedOrbitPos(const DPOrbitCamera_Component& xOrbit)
	{
		const float fEff = xOrbit.GetOrbitYaw() + glm::pi<float>();
		const Zenith_Maths::Vector3 xOffset(
			std::sin(fEff) * std::cos(xOrbit.GetOrbitPitch()),
			std::sin(xOrbit.GetOrbitPitch()),
			std::cos(fEff) * std::cos(xOrbit.GetOrbitPitch()));
		return xOrbit.GetOrbitTarget() + xOffset * xOrbit.GetOrbitDistance();
	}

	float DistanceToVillager(const Zenith_CameraComponent& xCam)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xCamVillager);
		if (!xEnt.IsValid()) return -1.0f;
		Zenith_TransformComponent* pxT = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxT == nullptr) return -1.0f;
		Zenith_Maths::Vector3 xVPos;
		pxT->GetPosition(xVPos);
		Zenith_Maths::Vector3 xCPos;
		xCam.GetPosition(xCPos);
		return glm::length(xCPos - xVPos);
	}
}

static void Setup_CameraModeTransition()
{
	g_bCamFailed = false;
	g_bCamDone = false;
	g_szCamWhy[0] = '\0';
	g_xCamVillager = INVALID_ENTITY_ID;
	g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE); // ProcLevel
}

static bool Step_CameraModeTransition(int iFrame)
{
	if (g_bCamFailed || g_bCamDone) return false;

	// ---- Phase A: settle + zero-drift assertion (frames 0..59) ----
	if (iFrame == 59)
	{
		DPOrbitCamera_Component* pxOrbit = FindOrbitCam();
		Zenith_CameraComponent* pxCam = FindSceneCamera();
		if (pxOrbit == nullptr || pxCam == nullptr) { CamFail("A: no orbit camera in ProcLevel"); return false; }
		if (pxOrbit->GetBlendT() != 0.0f) { CamFail("A: blend-t drifted with no possession"); return false; }
		if (pxOrbit->GetCameraMode() != DPCameraMode::BirdsEye) { CamFail("A: mode not BirdsEye at start"); return false; }
		Zenith_Maths::Vector3 xPos;
		pxCam->GetPosition(xPos);
		if (glm::length(xPos - ExpectedOrbitPos(*pxOrbit)) > 0.01f)
		{
			CamFail("A: camera pose diverged from the classic orbit pose while unpossessed");
			return false;
		}

		// Pick any villager and possess it via the system path.
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!g_xCamVillager.IsValid()) g_xCamVillager = xId;
			});
		if (!g_xCamVillager.IsValid()) { CamFail("A: no villager to possess"); return false; }
		DP_Player::SetPossessedVillager(g_xCamVillager);
	}

	// Windowed runs: dump the converged third-person pose (skipped
	// headless; the numeric assertions below are the actual gate).
	if (iFrame == 145 && !Zenith_CommandLine::IsHeadless())
	{
		Flux_Screenshot::RequestDump("C:/tmp/dp_camera_thirdperson.tga");
	}

	// ---- Phase B: converge to third-person (frames 60..149) ----
	if (iFrame == 149)
	{
		DPOrbitCamera_Component* pxOrbit = FindOrbitCam();
		Zenith_CameraComponent* pxCam = FindSceneCamera();
		if (pxOrbit == nullptr || pxCam == nullptr) { CamFail("B: camera vanished"); return false; }
		if (pxOrbit->GetCameraMode() != DPCameraMode::ThirdPerson)
		{
			CamFail("B: mode did not flip to ThirdPerson on possession");
			return false;
		}
		if (pxOrbit->GetBlendT() < 1.0f)
		{
			CamFail("B: blend-t did not reach 1 within ~1.5 s of possession");
			return false;
		}
		const float fDist = DistanceToVillager(*pxCam);
		if (fDist < 0.0f || fDist > 8.0f)
		{
			CamFail("B: camera did not converge near the possessed villager");
			return false;
		}
		std::printf("[DPCameraMode] third-person converged (%.2f m from villager)\n", fDist);
		DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
	}

	// ---- Phase C: blend back to birds-eye (frames 150..239) ----
	if (iFrame == 239)
	{
		DPOrbitCamera_Component* pxOrbit = FindOrbitCam();
		Zenith_CameraComponent* pxCam = FindSceneCamera();
		if (pxOrbit == nullptr || pxCam == nullptr) { CamFail("C: camera vanished"); return false; }
		if (pxOrbit->GetCameraMode() != DPCameraMode::BirdsEye)
		{
			CamFail("C: mode did not return to BirdsEye after possession loss");
			return false;
		}
		if (pxOrbit->GetBlendT() != 0.0f)
		{
			CamFail("C: blend-t did not return to exactly 0");
			return false;
		}
		Zenith_Maths::Vector3 xPos;
		pxCam->GetPosition(xPos);
		if (glm::length(xPos - ExpectedOrbitPos(*pxOrbit)) > 0.01f)
		{
			CamFail("C: pose did not re-match the classic orbit pose");
			return false;
		}

		// Kick Phase D: manual toggle with NO possession (uses last-possessed).
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_C);
	}

	// ---- Phase D: manual override engages third-person (frames 240..329) ----
	if (iFrame == 329)
	{
		DPOrbitCamera_Component* pxOrbit = FindOrbitCam();
		if (pxOrbit == nullptr) { CamFail("D: camera vanished"); return false; }
		if (pxOrbit->GetCameraMode() != DPCameraMode::ThirdPerson)
		{
			CamFail("D: C-press did not force ThirdPerson via last-possessed villager");
			return false;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_C);
	}

	// ---- Phase D2: second toggle returns to birds-eye (frames 330..399) ----
	if (iFrame == 399)
	{
		DPOrbitCamera_Component* pxOrbit = FindOrbitCam();
		if (pxOrbit == nullptr) { CamFail("D2: camera vanished"); return false; }
		if (pxOrbit->GetCameraMode() != DPCameraMode::BirdsEye)
		{
			CamFail("D2: second C-press did not force BirdsEye");
			return false;
		}
		g_bCamDone = true;
		return false;
	}

	return true;
}

static bool Verify_CameraModeTransition()
{
	if (!g_bCamDone || g_bCamFailed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPCameraMode failed: %s",
			g_szCamWhy[0] != '\0' ? g_szCamWhy : "did not complete");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDPCameraModeTest = {
	"Test_DPCamera_ModeTransition",
	&Setup_CameraModeTransition,
	&Step_CameraModeTransition,
	&Verify_CameraModeTransition,
	/*maxFrames*/ 450
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPCameraModeTest);

#endif // ZENITH_INPUT_SIMULATOR
