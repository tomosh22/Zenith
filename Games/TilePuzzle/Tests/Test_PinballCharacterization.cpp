#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_PinballCharacterization - characterization test for the wave-2 graph
 * conversion of Pinball's ball-lost / gate respawn flow.
 *
 * Written against the C++ version FIRST (Pinball_GameComponent::HandleBallLost:
 * limited-ball decrement, objective-met -> gate cleared, out-of-balls -> gate
 * failed, otherwise respawn + READY); the graph version must keep it green
 * unchanged.
 *
 *   Pinball_RespawnFlow_Test - requests gate 5 (SCORE_THRESHOLD 4000, max 3
 *      balls), launches the plunger through the REAL mouse-drag input path,
 *      and drains 3 balls without reaching the score threshold. Verifies the
 *      ball counter walks 3 -> 2 -> 1 -> 0 with a respawn back to READY after
 *      each non-final drain, the gate-failed display fires on the final
 *      drain, and the retry reset restores 3 balls and READY.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
// TilePuzzle_GameComponent.h declares TilePuzzle::Resources(), which the
// pinball header references - same include order as the AutoTest component.
#include "TilePuzzle/Components/TilePuzzle_GameComponent.h"
#include "TilePuzzle/Components/Pinball_GameComponent.h"

#include <cmath>

namespace
{
	Pinball_GameComponent* FindPinball(Zenith_EntityID* pxIdOut = nullptr)
	{
		Pinball_GameComponent* pxFound = nullptr;
		Zenith_EntityID xId;
		g_xEngine.Scenes().QueryAllScenes<Pinball_GameComponent>().ForEach(
			[&pxFound, &xId](Zenith_EntityID xEntity, Pinball_GameComponent& xGame)
			{
				if (pxFound == nullptr) { pxFound = &xGame; xId = xEntity; }
			});
		if (pxIdOut) *pxIdOut = xId;
		return pxFound;
	}

	Zenith_CameraComponent* FindPinballCamera()
	{
		Zenith_EntityID xId;
		if (FindPinball(&xId) == nullptr || !xId.IsValid()) return nullptr;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_CameraComponent>()) return nullptr;
		return &xEnt.GetComponent<Zenith_CameraComponent>();
	}

	// Replica of Pinball_GameComponent::ScreenToWorld (ray to the Z=0 plane)
	// using the camera's public unproject.
	bool ScreenToWorldZ0(Zenith_CameraComponent& xCam, float fScreenX, float fScreenY, float& fWorldX, float& fWorldY)
	{
		Zenith_Maths::Vector3 xNear = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 0.f));
		Zenith_Maths::Vector3 xFar = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 1.f));
		Zenith_Maths::Vector3 xDir = xFar - xNear;
		if (std::fabs(xDir.z) < 1e-6f) return false;
		const float fT = (0.f - xNear.z) / xDir.z;
		if (fT < 0.f) return false;
		fWorldX = xNear.x + fT * xDir.x;
		fWorldY = xNear.y + fT * xDir.y;
		return true;
	}

	// Numeric inverse of ScreenToWorldZ0 (Newton on the smooth projective map)
	// so the test can aim REAL mouse input at world-space playfield points.
	bool FindScreenForWorld(Zenith_CameraComponent& xCam, float fTargetX, float fTargetY, Zenith_Maths::Vector2& xScreenOut)
	{
		int32_t iW = 0, iH = 0;
		Zenith_Window::GetInstance()->GetSize(iW, iH);
		float fSx = iW * 0.5f, fSy = iH * 0.5f;
		for (int i = 0; i < 16; ++i)
		{
			float fWx, fWy;
			if (!ScreenToWorldZ0(xCam, fSx, fSy, fWx, fWy)) return false;
			const float fEx = fTargetX - fWx, fEy = fTargetY - fWy;
			if (std::fabs(fEx) < 0.01f && std::fabs(fEy) < 0.01f)
			{
				xScreenOut = Zenith_Maths::Vector2(fSx, fSy);
				return true;
			}
			float fWx1, fWy1, fWx2, fWy2;
			if (!ScreenToWorldZ0(xCam, fSx + 5.f, fSy, fWx1, fWy1)) return false;
			if (!ScreenToWorldZ0(xCam, fSx, fSy + 5.f, fWx2, fWy2)) return false;
			const float fA = (fWx1 - fWx) / 5.f, fC = (fWy1 - fWy) / 5.f;
			const float fB = (fWx2 - fWx) / 5.f, fD = (fWy2 - fWy) / 5.f;
			const float fDet = fA * fD - fB * fC;
			if (std::fabs(fDet) < 1e-9f) return false;
			fSx += (fD * fEx - fB * fEy) / fDet;
			fSy += (-fC * fEx + fA * fEy) / fDet;
		}
		return false;
	}

	enum class PBPhase
	{
		Boot, WaitReady, AimPlunger, DragPlunger, ReleasePlunger, AwaitLaunch, AwaitDrain, AwaitFailReset, Done
	};

	PBPhase  g_ePBPhase = PBPhase::Boot;
	int      g_iPBFrame = 0;
	int      g_iPBRetries = 0;
	uint32_t g_uLaunchNumber = 0;       // 1-based; 3 launches total
	bool     g_bSawBalls2 = false;
	bool     g_bSawBalls1 = false;
	bool     g_bSawGateFailed = false;
	bool     g_bSawRetryReset = false;
	Zenith_Maths::Vector2 g_xGrabScreen;
	Zenith_Maths::Vector2 g_xPullScreen;
}

static void Setup_PinballRespawnFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	// Gate 5: SCORE_THRESHOLD 4000, uMaxBalls = 3 - the limited-ball gate the
	// respawn flow's decrement / fail branches need. Same global the level
	// router writes when entering a gate level.
	TilePuzzle::g_uPinballRequestedGate = 5;
	g_ePBPhase = PBPhase::Boot;
	g_iPBFrame = 0;
	g_iPBRetries = 0;
	g_uLaunchNumber = 0;
	g_bSawBalls2 = false;
	g_bSawBalls1 = false;
	g_bSawGateFailed = false;
	g_bSawRetryReset = false;
}

static bool Step_PinballRespawnFlow(int iFrame)
{
	Pinball_GameComponent* pxPinball = FindPinball();

	switch (g_ePBPhase)
	{
	case PBPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
		g_ePBPhase = PBPhase::WaitReady;
		return true;

	case PBPhase::WaitReady:
	{
		if (pxPinball == nullptr) return iFrame < 600;
		// Dismiss the first-gate tutorial overlay if it's up (click after the
		// 0.5 s fade; same input a human uses).
		if (pxPinball->IsTutorialActive())
		{
			if (++g_iPBFrame % 60 < 2)
			{
				if (g_iPBFrame % 60 == 0) Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
				else                      Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
			}
			return iFrame < 1200;
		}
		if (pxPinball->GetPinballState() == PINBALL_STATE_READY
			&& pxPinball->IsGateActive()
			&& pxPinball->GetBallsRemaining() == 3)
		{
			g_uLaunchNumber = 1;
			g_iPBFrame = 0;
			g_ePBPhase = PBPhase::AimPlunger;
			return true;
		}
		return iFrame < 1200;
	}

	case PBPhase::AimPlunger:
	{
		Zenith_CameraComponent* pxCam = FindPinballCamera();
		if (pxCam == nullptr) return false;
		const float fChannelCentreX = (s_fPB_ChannelLeft + s_fPB_ChannelRight) * 0.5f;
		// Grab at the plunger rest position; pull ~0.45 of max (a soft launch
		// keeps the session score far below the 4000 threshold).
		if (!FindScreenForWorld(*pxCam, fChannelCentreX, s_fPB_PlungerRestY, g_xGrabScreen)) return false;
		if (!FindScreenForWorld(*pxCam, fChannelCentreX, s_fPB_PlungerRestY - 0.45f, g_xPullScreen)) return false;
		Zenith_InputSimulator::SimulateMousePosition(g_xGrabScreen.x, g_xGrabScreen.y);
		Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
		g_iPBFrame = 0;
		g_ePBPhase = PBPhase::DragPlunger;
		return true;
	}

	case PBPhase::DragPlunger:
	{
		// Drag down to the pull target over 12 frames.
		const float fT = glm::min(1.0f, ++g_iPBFrame / 12.0f);
		const float fX = g_xGrabScreen.x + (g_xPullScreen.x - g_xGrabScreen.x) * fT;
		const float fY = g_xGrabScreen.y + (g_xPullScreen.y - g_xGrabScreen.y) * fT;
		Zenith_InputSimulator::SimulateMousePosition(fX, fY);
		if (fT >= 1.0f)
		{
			g_ePBPhase = PBPhase::ReleasePlunger;
		}
		return true;
	}

	case PBPhase::ReleasePlunger:
		Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);
		g_iPBFrame = 0;
		g_ePBPhase = PBPhase::AwaitLaunch;
		return true;

	case PBPhase::AwaitLaunch:
	{
		if (pxPinball == nullptr) return false;
		if (pxPinball->GetPinballState() == PINBALL_STATE_PLAYING)
		{
			g_iPBFrame = 0;
			g_ePBPhase = (g_uLaunchNumber >= 3) ? PBPhase::AwaitFailReset : PBPhase::AwaitDrain;
			return true;
		}
		// Drag missed the plunger (or pull registered as 0) - retry.
		if (++g_iPBFrame > 60)
		{
			if (++g_iPBRetries > 6) return false;
			g_iPBFrame = 0;
			g_ePBPhase = PBPhase::AimPlunger;
		}
		return true;
	}

	case PBPhase::AwaitDrain:
	{
		if (pxPinball == nullptr) return false;
		const uint32_t uExpectedBalls = 3 - g_uLaunchNumber;
		if (pxPinball->GetPinballState() == PINBALL_STATE_READY
			&& pxPinball->GetBallsRemaining() == uExpectedBalls)
		{
			if (uExpectedBalls == 2) g_bSawBalls2 = true;
			if (uExpectedBalls == 1) g_bSawBalls1 = true;
			++g_uLaunchNumber;
			g_iPBFrame = 0;
			g_ePBPhase = PBPhase::AimPlunger;
			return true;
		}
		// A drain can take a while if the ball rattles around the pegs.
		return ++g_iPBFrame < 3600;
	}

	case PBPhase::AwaitFailReset:
	{
		if (pxPinball == nullptr) return false;
		if (pxPinball->IsGateFailedDisplayActive())
		{
			g_bSawGateFailed = true;
		}
		// After the failure display, the retry reset restores 3 balls + READY.
		if (g_bSawGateFailed
			&& pxPinball->GetPinballState() == PINBALL_STATE_READY
			&& pxPinball->GetBallsRemaining() == 3)
		{
			g_bSawRetryReset = true;
			g_ePBPhase = PBPhase::Done;
			return false;
		}
		return ++g_iPBFrame < 4500;
	}

	case PBPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PinballRespawnFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawBalls2)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballRespawn] never saw 2 balls remaining after first drain"); }
	if (!g_bSawBalls1)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballRespawn] never saw 1 ball remaining after second drain"); }
	if (!g_bSawGateFailed) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballRespawn] gate-failed display never fired"); }
	if (!g_bSawRetryReset) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[PinballRespawn] retry reset (3 balls + READY) never observed"); }
	return g_bSawBalls2 && g_bSawBalls1 && g_bSawGateFailed && g_bSawRetryReset;
}

static const Zenith_AutomatedTest g_xPinballRespawnFlowTest = {
	"Pinball_RespawnFlow_Test",
	&Setup_PinballRespawnFlow,
	&Step_PinballRespawnFlow,
	&Verify_PinballRespawnFlow,
	/*maxFrames*/ 18000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPinballRespawnFlowTest);

#endif // ZENITH_INPUT_SIMULATOR
