#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>

// ============================================================================
// Test_P1Apprehend_OutOfRangeIgnored (MVP-1.3.4)
//
// Regression guard for the Apprehend BT branch: when the priest is FAR
// from the possessed villager (outside priest.apprehend_range_m), the
// Apprehend node MUST return FAILURE on the first Execute and let the
// Selector fall through to Pursue. The priest then walks toward the
// villager via the regular pathfinding flow; no DP_OnRunLost is
// dispatched while the priest is still en-route.
//
// Without the range gate (or with a broken distance computation) the
// channel would start the instant the BT bridge wrote
// BB_KEY_TARGET_WITH_DEVIL, ending the run before the player could
// react -- the original PriestPursuit_Test on GameLevel starts the
// priest ~4.4m from the villager, so a stuck-range gate would fail
// every pursuit run-up.
//
// Procedure:
//   1. Load GameLevel + subscribe to DP_OnRunLost.
//   2. Pick the priest + the closest villager (~4.4m away in stock
//      GameLevel data -- well outside the 2.0m apprehend_range).
//   3. Possess the villager. Do NOT teleport the priest -- leave it
//      at its authored ~4.4m starting separation.
//   4. Run 120 frames (~2 seconds; shorter than the smallest possible
//      "pursue-then-channel" path. At pursue_speed_mps = 7, closing the
//      4.4m -> 1.5m gap takes ~0.4s, so Apprehend's 3s channel completes
//      no earlier than frame ~205. 120 frames keeps the test inside
//      the pre-channel window even under perfect pursuit.)
//   5. Assert DP_OnRunLost did NOT fire during this window.
//
// What the test allows: the priest can pursue + close the distance.
// What it forbids: Apprehend channel firing FROM the authored start
// distance (which would only happen if the range gate was broken).
//
// To keep the test deterministic the priest's authored start is the
// only thing the priest sees; the test does NOT teleport. If the priest
// ever appears to apprehend "for free" from authoring distance, this
// test fails noisily.
// ============================================================================

namespace
{
	enum Phase : int { kOR_Start, kOR_WaitScene, kOR_Possess,
	                   kOR_RunFrames, kOR_Verify, kOR_Done };

	int                     g_iPhase = kOR_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	bool                    g_bRunLostFired = false;
	int                     g_iRunFrames = 0;
	float                   g_fInitialSeparation = 0.0f;

	constexpr int kFRAMES_TO_RUN = 120;  // ~2s @ 60Hz (pre-channel window)

	void OnRunLost(const DP_OnRunLost& /*xEvt*/)
	{
		g_bRunLostFired = true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}
}

static void Setup_P1ApprehendOutOfRange()
{
	g_iPhase = kOR_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_bRunLostFired = false;
	g_iRunFrames = 0;
	g_fInitialSeparation = 0.0f;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1ApprehendOutOfRange(int iFrame)
{
	switch (g_iPhase)
	{
	case kOR_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kOR_WaitScene;
		return true;

	case kOR_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xClosest;
		float fClosestDist = 1e30f;
		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;

		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos]
			(Zenith_EntityID xId, Priest_Behaviour&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});
		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&xClosest, &fClosestDist, &xPriestPos]
				(Zenith_EntityID xId, DPVillager_Behaviour&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					const float fD = HorizontalDistance(xPriestPos, xVPos);
					if (fD < fClosestDist) { fClosestDist = fD; xClosest = xId; }
				});
		}

		if (xFoundPriest.IsValid() && xClosest.IsValid())
		{
			g_xPriest   = xFoundPriest;
			g_xVillager = xClosest;
			g_fInitialSeparation = fClosestDist;
			g_iPhase    = kOR_Possess;
			Zenith_Log(LOG_CATEGORY_AI,
				"P1ApprehendOOR: priest=(%.1f,%.1f,%.1f) closest_villager@%.2fm",
				xPriestPos.x, xPriestPos.y, xPriestPos.z, fClosestDist);
		}
		else if (iFrame > 60)
		{
			g_iPhase = kOR_Done;
		}
		return true;
	}

	case kOR_Possess:
	{
		// MVP-1.9 cleanup: explicit LOS so the priest's sight cone
		// catches the villager. RegisterTarget puts the villager into
		// the priest's perception scan; teleport places it 4 m IN
		// FRONT of the priest (authored priest yaw = 0, facing +Z).
		// 4 m is outside apprehend_range (2 m) so the Apprehend BT
		// branch must return FAILURE -- the original assertion of
		// this test, preserved at a known-deterministic distance now
		// that perception is no longer faked.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);
		Zenith_Maths::Vector3 xPriestPos;
		if (TryGetEntityPos(g_xPriest, xPriestPos))
		{
			Zenith_SceneData* pxScene =
				Zenith_SceneManager::GetSceneDataForEntity(g_xVillager);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(g_xVillager);
				if (xEnt.IsValid()
				 && xEnt.HasComponent<Zenith_TransformComponent>())
				{
					xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(
						Zenith_Maths::Vector3(
							xPriestPos.x, xPriestPos.y, xPriestPos.z + 4.0f));
					g_fInitialSeparation = 4.0f;
				}
			}
		}
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iRunFrames = 0;
		g_iPhase = kOR_RunFrames;
		return true;
	}

	case kOR_RunFrames:
		++g_iRunFrames;
		if (g_bRunLostFired)
		{
			// Fail-fast: there's no point running another N frames after
			// the spurious dispatch.
			Zenith_Log(LOG_CATEGORY_AI,
				"P1ApprehendOOR: DP_OnRunLost fired at frame %d (initial sep %.2fm) -- spurious",
				g_iRunFrames, g_fInitialSeparation);
			g_iPhase = kOR_Verify;
			return true;
		}
		if (g_iRunFrames >= kFRAMES_TO_RUN)
		{
			g_iPhase = kOR_Verify;
		}
		return true;

	case kOR_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendOOR: runLost=%d initialSep=%.2fm (expect runLost=0)",
			(int)g_bRunLostFired, g_fInitialSeparation);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_iPhase = kOR_Done;
		return false;

	case kOR_Done:
	default:
		return false;
	}
}

static bool Verify_P1ApprehendOutOfRange()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ApprehendOOR: priest not found");
		return false;
	}
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ApprehendOOR: villager not found");
		return false;
	}
	if (g_fInitialSeparation < 3.0f)
	{
		// Sanity guard -- the test assumes the priest starts well outside
		// the 2m apprehend_range. If GameLevel authoring ever moves the
		// priest closer than 3m from any villager, the assertion below
		// stops meaning anything useful. Fail with a clear message so we
		// notice and re-tune the test setup.
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendOOR: initial separation %.2fm is too small -- test design assumes priest authored OUTSIDE apprehend_range (2m)",
			g_fInitialSeparation);
		return false;
	}
	if (g_bRunLostFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendOOR: DP_OnRunLost fired -- Apprehend channel ran from out-of-range");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ApprehendOORTest = {
	"Test_P1Apprehend_OutOfRangeIgnored",
	&Setup_P1ApprehendOutOfRange,
	&Step_P1ApprehendOutOfRange,
	&Verify_P1ApprehendOutOfRange,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ApprehendOORTest);

#endif // ZENITH_INPUT_SIMULATOR
