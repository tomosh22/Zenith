#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"

#include <cmath>

// ============================================================================
// Test_P1Priest_DoesNotChasePossessedOutOfSight (MVP-1.9.1 + 1.9.2)
//
// MVP-1.9 removes the omniscient fallback from
// `Priest_Component::BridgePerceptionToBlackboard`. With the fallback
// disabled, possessing a villager that the priest cannot SEE (or hear)
// must NOT populate the priest's `BB_KEY_TARGET_WITH_DEVIL` slot --
// the priest stays in patrol mode until perception delivers a target.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find the priest. Possess the FARTHEST villager (~100 m away
//      in stock GameLevel authoring -- well outside priest sight
//      range, no LOS).
//   4. Run a ~2 s window (~120 frames). In this window the priest
//      perception system ticks, but no sight/hearing stimulus
//      reaches the priest for the possessed villager.
//   5. Assert:
//      - Priest's BB_KEY_TARGET_WITH_DEVIL is INVALID throughout
//        the entire window (sampled at the end).
//      - BB.TargetWithDevil was INVALID on at least one mid-window
//        check (catches the rare "perception fired once then went
//        invalid" race).
//
// Note: the existing fallback-on tests prove the priest DOES pursue
// when the flag is enabled, so this test specifically locks in the
// production-shape (flag-off) behaviour.
// ============================================================================

namespace
{
	enum Phase : int { kNS_Start, kNS_WaitScene, kNS_Possess,
	                   kNS_RunFrames, kNS_Verify, kNS_Done };

	int                     g_iPhase = kNS_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	int                     g_iRunFrames = 0;
	bool                    g_bMidWindowTargetInvalid = false;
	Zenith_EntityID         g_xFinalBBTarget;
	constexpr int kRUN_FRAMES = 120;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	Zenith_EntityID ReadPriestBBTarget(Zenith_EntityID xPriestId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xPriestId);
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		Zenith_AIAgentComponent* pxAgent = xEnt.TryGetComponent<Zenith_AIAgentComponent>();
		if (pxAgent == nullptr) return INVALID_ENTITY_ID;
		return pxAgent->GetBlackboard().GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
	}
}

static void Setup_P1NoChaseOutOfSight()
{
	g_iPhase = kNS_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_iRunFrames = 0;
	g_bMidWindowTargetInvalid = false;
	g_xFinalBBTarget = INVALID_ENTITY_ID;
}

static bool Step_P1NoChaseOutOfSight(int iFrame)
{
	switch (g_iPhase)
	{
	case kNS_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kNS_WaitScene;
		return true;

	case kNS_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFarthest;
		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;

		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos]
			(Zenith_EntityID xId, Priest_Component&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});

		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			float fFarthestDist = 0.0f;
			DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
				[&xFarthest, &fFarthestDist, &xPriestPos]
				(Zenith_EntityID xId, DPVillager_Component&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					const float fD = HorizontalDistance(xPriestPos, xVPos);
					if (fD > fFarthestDist)
					{
						fFarthestDist = fD;
						xFarthest = xId;
					}
				});
			if (xFarthest.IsValid() && fFarthestDist > 30.0f)
			{
				g_xPriest = xFoundPriest;
				g_xVillager = xFarthest;
				g_iPhase = kNS_Possess;
				Zenith_Log(LOG_CATEGORY_AI,
					"P1NoChaseOOS: priest=(%.1f,%.1f,%.1f) farthest villager @ %.1f m",
					xPriestPos.x, xPriestPos.y, xPriestPos.z, fFarthestDist);
			}
		}
		if (g_iPhase == kNS_WaitScene && iFrame > 60)
		{
			g_iPhase = kNS_Done;
		}
		return true;
	}

	case kNS_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iRunFrames = 0;
		g_iPhase = kNS_RunFrames;
		return true;

	case kNS_RunFrames:
	{
		++g_iRunFrames;
		// Mid-window check (frame 60 of 120): if the priest's BB
		// target is INVALID here, that's evidence the fallback is
		// genuinely off AND the bridge isn't briefly populating it
		// from a transient perception hit before clearing.
		if (g_iRunFrames == kRUN_FRAMES / 2)
		{
			g_bMidWindowTargetInvalid =
				!ReadPriestBBTarget(g_xPriest).IsValid();
		}
		if (g_iRunFrames >= kRUN_FRAMES)
		{
			g_xFinalBBTarget = ReadPriestBBTarget(g_xPriest);
			g_iPhase = kNS_Verify;
		}
		return true;
	}

	case kNS_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoChaseOOS: midInvalid=%d finalTarget=(%u/%u)",
			(int)g_bMidWindowTargetInvalid,
			g_xFinalBBTarget.m_uIndex, g_xFinalBBTarget.m_uGeneration);
		g_iPhase = kNS_Done;
		return false;

	case kNS_Done:
	default:
		return false;
	}
}

static bool Verify_P1NoChaseOutOfSight()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1NoChaseOOS: priest not found");
		return false;
	}
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1NoChaseOOS: far villager not found");
		return false;
	}
	if (!g_bMidWindowTargetInvalid)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1NoChaseOOS: BB target was valid mid-window -- fallback is leaking through");
		return false;
	}
	if (g_xFinalBBTarget.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoChaseOOS: BB target at end of window = (%u/%u), expected INVALID -- priest knows about possession somehow",
			g_xFinalBBTarget.m_uIndex, g_xFinalBBTarget.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1NoChaseOOSTest = {
	"Test_P1Priest_DoesNotChasePossessedOutOfSight",
	&Setup_P1NoChaseOutOfSight,
	&Step_P1NoChaseOutOfSight,
	&Verify_P1NoChaseOutOfSight,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1NoChaseOOSTest);

#endif // ZENITH_INPUT_SIMULATOR
