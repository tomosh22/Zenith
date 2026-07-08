#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_AudioBus.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPItemBase_Component.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>

// ============================================================================
// W3 graph-conversion characterization tests.
//
// Written BEFORE the W3 DevilsPlayground behaviour-graph conversion and run
// GREEN against the C++ components first (the 5-step playbook). They pin the
// behaviours the conversion touches that the existing suite does not already
// pin precisely:
//
//  1. Test_W3Priest_ApprehendStartsInRange - the apprehend channel only ever
//     STARTS with the target inside priest.apprehend_range_m (XZ). Deliberately
//     an upper-bound pin, not an exact-distance pin: the BT starts the channel
//     after pursue completes (<= 1.5m 3D), the reactive-Selector graph may
//     start it as soon as the XZ range gate passes - both satisfy this bound.
//  2. Test_W3Item_ReagentChannelResumesAfterExit - the reagent pickup-channel
//     PAUSES (does not reset, does not tick) while the possessed villager is
//     out of range, and RESUMES from the stale remaining on re-entry by the
//     same villager. (The header comment in DPItemBase claims walk-away
//     resets; the code pauses. The CODE behaviour is the contract.)
//  3. Test_W3Villager_FootstepCadence - footstep emission cadence: first step
//     fires IMMEDIATELY on the first moving frame (countdown held at 0 while
//     idle), then every movement.footstep_interval_s (0.4s).
//
// NOT pinned here: the post-restart duplicate-pause-instance double
// DP_OnPauseToggle dispatch. That count depends on persistent-vs-gameplay
// scene update order, which Test_P1Pause_TimerStopsOnEscape documents as
// deliberately unspecified ("both dispatch orderings are valid").
// ============================================================================

namespace
{
	float W3_HorizontalDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	bool W3_TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	bool W3_TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->SetPosition(xPos);
		return true;
	}

	Zenith_EntityID W3_FindFirstPriest()
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFound](Zenith_EntityID xId, Priest_Component&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		return xFound;
	}

	Zenith_EntityID W3_FindFirstVillager()
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFound](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		return xFound;
	}

	// The Test_P1Priest_PursuesAfterLineOfSight placement recipe: HORIZONTAL
	// forward (yaw-only, matching the perception system's sight forward),
	// villager at the priest's OWN height. Must be re-applied every frame
	// until sight acquires - the priest rotates and the physics sync fights
	// one-shot transform teleports.
	void W3_PlaceInPriestFOV(Zenith_EntityID xPriest, Zenith_EntityID xVillager, float fDist)
	{
		Zenith_Maths::Vector3 xPriestPos;
		if (!W3_TryGetEntityPos(xPriest, xPriestPos)) return;
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xPriest);
		if (!xEnt.IsValid()) return;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		Zenith_Maths::Quaternion xQuat;
		pxTransform->GetRotation(xQuat);
		Zenith_Maths::Vector3 xFwd = xQuat * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xFwd.y = 0.0f;
		const float fLen = std::sqrt(xFwd.x * xFwd.x + xFwd.z * xFwd.z);
		if (fLen > 1e-4f) { xFwd.x /= fLen; xFwd.z /= fLen; }
		Zenith_Maths::Vector3 xPos = xPriestPos + xFwd * fDist;
		xPos.y = xPriestPos.y;
		W3_TrySetEntityPos(xVillager, xPos);
	}

	DPVillager_Component* W3_GetVillager(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<DPVillager_Component>();
	}
}

// ============================================================================
// 1. Test_W3Priest_ApprehendStartsInRange
// ============================================================================

namespace
{
	enum PhaseA : int { kA_Start, kA_WaitScene, kA_Stage, kA_Run, kA_Done };

	int                g_iPhaseA = kA_Start;
	Zenith_EntityID    g_xAPriest;
	Zenith_EntityID    g_xAVillager;
	Zenith_EventHandle g_xAStartHandle = INVALID_EVENT_HANDLE;
	bool               g_bAChannelStarted = false;
	float              g_fAStartDistXZ = -1.0f;
	int                g_iARunFrames = 0;

	// Pursue from 4m at pursue_speed 4.5 m/s closes in under a second; the
	// budget is generous because the nav path may detour around procgen walls.
	constexpr int kA_MAX_RUN_FRAMES = 600;

	void OnApprehendStart(const DP_OnApprehendChannelStart& xEvt)
	{
		if (g_bAChannelStarted) return;
		g_bAChannelStarted = true;
		Zenith_Maths::Vector3 xPriestPos(0.0f), xVictimPos(0.0f);
		if (W3_TryGetEntityPos(xEvt.m_xPriest, xPriestPos)
			&& W3_TryGetEntityPos(xEvt.m_xVictim, xVictimPos))
		{
			g_fAStartDistXZ = W3_HorizontalDistance(xPriestPos, xVictimPos);
		}
	}
}

static void Setup_W3ApprehendStartsInRange()
{
	// Skip on a fresh checkout / --skip-tool-exports run (CI): the DP asset tree is
	// gitignored + generated by the tool-export pass, so the priest + villagers
	// spawn without their baked geometry and the apprehend acquire never completes
	// (600-frame timeout). The test passes wherever the assets exist (local dev), so
	// this presence check tracks the pass(local)/fail(CI) split. Mirrors the guard in
	// Test_ProcLevel_PriestReachability. (If skipped, no dt pin is set below and Verify
	// -- which would ClearFixedDt -- is bypassed for skipped tests.)
	if (!std::filesystem::exists(
		std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube.zmodel"))
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"DP baked assets absent (fresh checkout / --skip-tool-exports); needs level geometry");
		return;
	}
	// Pin dt so the priest's per-frame awareness-gain + pursue movement are
	// deterministic regardless of the preceding batch test's wall-clock dt. Without
	// this the acquire never completes under batch timing (perceived=0) and the
	// channel never starts -> 600-frame timeout. Released in Verify (no Teardown
	// hook), matching Test_W3Villager_FootstepCadence + the personality tests.
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_iPhaseA = kA_Start;
	g_xAPriest = INVALID_ENTITY_ID;
	g_xAVillager = INVALID_ENTITY_ID;
	g_bAChannelStarted = false;
	g_fAStartDistXZ = -1.0f;
	g_iARunFrames = 0;
	g_xAStartHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnApprehendChannelStart>(&OnApprehendStart);
}

static bool Step_W3ApprehendStartsInRange(int iFrame)
{
	switch (g_iPhaseA)
	{
	case kA_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhaseA = kA_WaitScene;
		return true;

	case kA_WaitScene:
	{
		// Pick the villager CLOSEST to the priest (the PriestPursuit_Test
		// staging, batch-proven): the perception/pursue pipeline reliably
		// acquires a nearby villager, while a first-by-query pick can sit
		// across the map behind walls.
		g_xAPriest = W3_FindFirstPriest();
		if (g_xAPriest.IsValid())
		{
			Zenith_Maths::Vector3 xPriestPos;
			if (W3_TryGetEntityPos(g_xAPriest, xPriestPos))
			{
				float fBest = 1e30f;
				Zenith_EntityID xBest;
				DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
					[&xBest, &fBest, &xPriestPos](Zenith_EntityID xId, DPVillager_Component&)
					{
						Zenith_Maths::Vector3 xVPos;
						if (!W3_TryGetEntityPos(xId, xVPos)) return;
						const float fD = W3_HorizontalDistance(xPriestPos, xVPos);
						if (fD < fBest) { fBest = fD; xBest = xId; }
					});
				g_xAVillager = xBest;
			}
		}
		if (g_xAPriest.IsValid() && g_xAVillager.IsValid())
		{
			g_iPhaseA = kA_Stage;
		}
		else if (iFrame > 120)
		{
			g_iPhaseA = kA_Done;
		}
		return true;
	}

	case kA_Stage:
	{
		// Staging = Test_P1Priest_PursuesAfterLineOfSight's batch-proven
		// recipe: register the villager + keep RE-PLACING it in the priest's
		// live FOV each frame until the bridge acquires it (the priest
		// rotates; one-shot placements miss the cone). 4m > apprehend_range
		// (2m): once acquired we STOP placing and let the priest close in.
		Zenith_PerceptionSystem::RegisterTarget(g_xAVillager, /*hostile=*/true);
		W3_PlaceInPriestFOV(g_xAPriest, g_xAVillager, 4.0f);
		DP_Player::SetPossessedVillager(g_xAVillager);
		g_iARunFrames = 0;
		g_iPhaseA = kA_Run;
		return true;
	}

	case kA_Run:
	{
		++g_iARunFrames;
		// Keep the villager in the rotating sight cone until the bridge
		// targets it; then hold still so the pursue/apprehend close-in runs
		// against a stationary possessed villager.
		bool bAcquired = false;
		{
			Zenith_Entity xPriestEnt = g_xEngine.Scenes().ResolveEntity(g_xAPriest);
			Priest_Component* pxPriestC = xPriestEnt.IsValid()
				? xPriestEnt.TryGetComponent<Priest_Component>() : nullptr;
			if (pxPriestC != nullptr)
			{
				bAcquired = pxPriestC->ReadBBEntity(DP_AI::BB_KEY_TARGET_WITH_DEVIL).IsValid();
			}
		}
		if (!bAcquired && !g_bAChannelStarted)
		{
			W3_PlaceInPriestFOV(g_xAPriest, g_xAVillager, 4.0f);
		}
		if ((g_iARunFrames % 120) == 1)
		{
			DPVillager_Component* pxV = W3_GetVillager(g_xAVillager);
			Zenith_Maths::Vector3 xP(0.0f), xV(0.0f);
			W3_TryGetEntityPos(g_xAPriest, xP);
			W3_TryGetEntityPos(g_xAVillager, xV);
			const Zenith_Vector<Zenith_PerceivedTarget>* paxPerceived =
				Zenith_PerceptionSystem::GetPerceivedTargets(g_xAPriest);
			Zenith_Log(LOG_CATEGORY_AI,
				"W3ApprehendStart: f=%d state=%d possessed=%d acquired=%d distXZ=%.2f perceived=%u",
				g_iARunFrames,
				pxV ? (int)pxV->GetState() : -1,
				pxV ? (int)pxV->IsPossessed() : -1,
				(int)bAcquired,
				W3_HorizontalDistance(xP, xV),
				paxPerceived ? paxPerceived->GetSize() : 0u);
		}
		if (g_bAChannelStarted || g_iARunFrames >= kA_MAX_RUN_FRAMES)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xAStartHandle);
			g_iPhaseA = kA_Done;
			return false;
		}
		return true;
	}

	case kA_Done:
	default:
		return false;
	}
}

static bool Verify_W3ApprehendStartsInRange()
{
	Zenith_InputSimulator::ClearFixedDt();  // always release the dt pin from Setup
	if (!g_xAPriest.IsValid() || !g_xAVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ApprehendStart: priest/villager not found");
		return false;
	}
	if (!g_bAChannelStarted)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ApprehendStart: channel never started within %d frames", kA_MAX_RUN_FRAMES);
		return false;
	}
	if (g_fAStartDistXZ < 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ApprehendStart: could not resolve positions at channel start");
		return false;
	}
	// The channel must only start inside the live-tuned apprehend range.
	// +0.35m slack: one 60Hz pursue frame moves ~0.075m, and capsule-settle
	// drift between the event dispatch and our position read is possible.
	const float fRange = DP_Tuning::Get<float>("priest.apprehend_range_m");
	const float fLimit = fRange + 0.35f;
	Zenith_Log(LOG_CATEGORY_AI, "W3ApprehendStart: startDistXZ=%.3fm (range=%.2fm limit=%.2fm)",
		g_fAStartDistXZ, fRange, fLimit);
	return g_fAStartDistXZ <= fLimit;
}

static const Zenith_AutomatedTest g_xW3ApprehendStartTest = {
	"Test_W3Priest_ApprehendStartsInRange",
	&Setup_W3ApprehendStartsInRange,
	&Step_W3ApprehendStartsInRange,
	&Verify_W3ApprehendStartsInRange,
	900
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xW3ApprehendStartTest);

// ============================================================================
// 2. Test_W3Item_ReagentChannelResumesAfterExit
// ============================================================================

namespace
{
	enum PhaseB : int { kB_Start, kB_WaitScene, kB_Stage, kB_WaitMidChannel,
	                    kB_OutOfRange, kB_FrozenWindow, kB_Return, kB_WaitPickup, kB_Done };

	int                    g_iPhaseB = kB_Start;
	Zenith_EntityID        g_xBVillager;
	Zenith_EntityID        g_xBItem;
	DPItemBase_Component*  g_pxBItem = nullptr;   // stable unless the item pool relocates; re-resolved each use
	float                  g_fBFrozenRemaining = -1.0f;
	float                  g_fBFrozenRemainingAfterWait = -1.0f;
	float                  g_fBChannelDuration = 0.0f;
	int                    g_iBWindowFrames = 0;
	int                    g_iBPickupFrames = -1;
	bool                   g_bBPickedUp = false;
	bool                   g_bBSetupOk = false;

	constexpr int kB_FROZEN_FRAMES = 60;
	constexpr int kB_MAX_PICKUP_FRAMES = 90;	// full fresh channel would be ~60 frames + slack

	DPItemBase_Component* W3_GetItemBase(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<DPItemBase_Component>();
	}
}

static void Setup_W3ReagentChannelResume()
{
	g_iPhaseB = kB_Start;
	g_xBVillager = INVALID_ENTITY_ID;
	g_xBItem = INVALID_ENTITY_ID;
	g_pxBItem = nullptr;
	g_fBFrozenRemaining = -1.0f;
	g_fBFrozenRemainingAfterWait = -1.0f;
	g_fBChannelDuration = 0.0f;
	g_iBWindowFrames = 0;
	g_iBPickupFrames = -1;
	g_bBPickedUp = false;
	g_bBSetupOk = false;
}

static bool Step_W3ReagentChannelResume(int iFrame)
{
	switch (g_iPhaseB)
	{
	case kB_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhaseB = kB_WaitScene;
		return true;

	case kB_WaitScene:
	{
		g_xBVillager = W3_FindFirstVillager();
		if (g_xBVillager.IsValid())
		{
			g_iPhaseB = kB_Stage;
		}
		else if (iFrame > 120)
		{
			g_iPhaseB = kB_Done;
		}
		return true;
	}

	case kB_Stage:
	{
		DP_Player::SetPossessedVillager(g_xBVillager);
		Zenith_Maths::Vector3 xVPos;
		if (!W3_TryGetEntityPos(g_xBVillager, xVPos)) { g_iPhaseB = kB_Done; return false; }

		// Caul: pickup_channel_s = 1.0, no special behaviour. Spawn it in
		// pickup range (0.5m) of the possessed villager - the channel arms
		// on the item's next OnUpdate.
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetActiveSceneData();
		if (pxScene == nullptr) { g_iPhaseB = kB_Done; return false; }
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxScene, "W3_CaulChannelProbe");
		if (!xEnt.IsValid()) { g_iPhaseB = kB_Done; return false; }
		if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(Zenith_Maths::Vector3(xVPos.x + 0.5f, xVPos.y, xVPos.z));
		}
		DPItemBase_Component& xItem = xEnt.AddComponent<DPItemBase_Component>();
		xItem.SetTag(DP_ItemTag::Caul);
		g_xBItem = xEnt.GetEntityID();
		g_fBChannelDuration = xItem.GetPickupChannelDurationForTest();
		g_bBSetupOk = (g_fBChannelDuration > 0.0f);
		g_iPhaseB = kB_WaitMidChannel;
		return true;
	}

	case kB_WaitMidChannel:
	{
		DPItemBase_Component* pxItem = W3_GetItemBase(g_xBItem);
		if (pxItem == nullptr) { g_iPhaseB = kB_Done; return false; }
		const float fRemaining = pxItem->GetChannelRemainingForTest();
		// Mid-channel: remaining armed (> 0) and measurably below duration.
		if (fRemaining > 0.0f && fRemaining <= g_fBChannelDuration * 0.6f)
		{
			// Move the ITEM out of range - the villager stays put (its
			// movement is physics-driven; the item is a plain transform).
			Zenith_Maths::Vector3 xVPos;
			if (!W3_TryGetEntityPos(g_xBVillager, xVPos)) { g_iPhaseB = kB_Done; return false; }
			W3_TrySetEntityPos(g_xBItem, Zenith_Maths::Vector3(xVPos.x + 30.0f, xVPos.y, xVPos.z));
			g_fBFrozenRemaining = fRemaining;
			g_iBWindowFrames = 0;
			g_iPhaseB = kB_FrozenWindow;
		}
		else if (iFrame > 400)
		{
			g_iPhaseB = kB_Done;	// channel never reached mid-point - setup failure
		}
		return true;
	}

	case kB_FrozenWindow:
	{
		++g_iBWindowFrames;
		if (g_iBWindowFrames >= kB_FROZEN_FRAMES)
		{
			DPItemBase_Component* pxItem = W3_GetItemBase(g_xBItem);
			if (pxItem == nullptr) { g_iPhaseB = kB_Done; return false; }
			g_fBFrozenRemainingAfterWait = pxItem->GetChannelRemainingForTest();
			g_iPhaseB = kB_Return;
		}
		return true;
	}

	case kB_Return:
	{
		Zenith_Maths::Vector3 xVPos;
		if (!W3_TryGetEntityPos(g_xBVillager, xVPos)) { g_iPhaseB = kB_Done; return false; }
		W3_TrySetEntityPos(g_xBItem, Zenith_Maths::Vector3(xVPos.x + 0.5f, xVPos.y, xVPos.z));
		g_iBPickupFrames = 0;
		g_iPhaseB = kB_WaitPickup;
		return true;
	}

	case kB_WaitPickup:
	{
		++g_iBPickupFrames;
		if (DP_Player::GetHeldItemTag(g_xBVillager) == DP_ItemTag::Caul)
		{
			g_bBPickedUp = true;
			g_iPhaseB = kB_Done;
			return false;
		}
		if (g_iBPickupFrames >= kB_MAX_PICKUP_FRAMES)
		{
			g_iPhaseB = kB_Done;
			return false;
		}
		return true;
	}

	case kB_Done:
	default:
		return false;
	}
}

static bool Verify_W3ReagentChannelResume()
{
	if (!g_bBSetupOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ChannelResume: setup failed (no villager / channel duration 0)");
		return false;
	}
	if (g_fBFrozenRemaining <= 0.0f || g_fBFrozenRemainingAfterWait < 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ChannelResume: never reached mid-channel (remaining=%.3f after=%.3f)",
			g_fBFrozenRemaining, g_fBFrozenRemainingAfterWait);
		return false;
	}
	// PAUSED, not reset and not ticking: remaining unchanged across the
	// out-of-range window (exact float compare is fair - nothing may write it).
	if (g_fBFrozenRemainingAfterWait != g_fBFrozenRemaining)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"W3ChannelResume: remaining changed while out of range (%.4f -> %.4f) - channel ticked or reset",
			g_fBFrozenRemaining, g_fBFrozenRemainingAfterWait);
		return false;
	}
	if (!g_bBPickedUp)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ChannelResume: pickup did not complete after re-entry");
		return false;
	}
	// RESUMED from stale remaining: completion must arrive well before a
	// fresh full channel would (duration 1.0s = ~60 frames; we froze at
	// <= 0.6*duration so resumption needs <= ~36 frames + a few frames of
	// re-entry slack).
	const int iFreshChannelFrames = (int)(g_fBChannelDuration * 60.0f);
	const int iLimit = (int)(g_fBFrozenRemaining * 60.0f) + 15;
	Zenith_Log(LOG_CATEGORY_AI,
		"W3ChannelResume: frozen=%.3fs pickupFrames=%d (resume limit %d, fresh channel %d)",
		g_fBFrozenRemaining, g_iBPickupFrames, iLimit, iFreshChannelFrames);
	if (g_iBPickupFrames > iLimit)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3ChannelResume: pickup took a full fresh channel - resume-from-stale was lost");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xW3ChannelResumeTest = {
	"Test_W3Item_ReagentChannelResumesAfterExit",
	&Setup_W3ReagentChannelResume,
	&Step_W3ReagentChannelResume,
	&Verify_W3ReagentChannelResume,
	900
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xW3ChannelResumeTest);

// ============================================================================
// 3. Test_W3Villager_FootstepCadence
// ============================================================================

namespace
{
	enum PhaseC : int { kC_Start, kC_WaitScene, kC_Possess, kC_Baseline,
	                    kC_FirstStepWindow, kC_Walk, kC_Done };

	int             g_iPhaseC = kC_Start;
	Zenith_EntityID g_xCVillager;
	int             g_iCBaselineCount = 0;
	int             g_iCFirstWindowCount = -1;
	int             g_iCFinalCount = -1;
	int             g_iCWalkFrames = 0;

	// 110 frames @ fixed 1/60 = 1.833s of movement. Steps at t=0 (immediate
	// first step), 0.4, 0.8, 1.2, 1.6 -> exactly 5; the next lands at 2.0s,
	// a 0.23s margin against float drift on both sides.
	constexpr int kC_WALK_FRAMES = 110;
	constexpr int kC_EXPECTED_STEPS = 5;
	constexpr int kC_FIRST_WINDOW = 3;

	int W3_CountFootsteps()
	{
		int iCount = 0;
		const Zenith_Vector<Zenith_AudioBus::EmittedSound>& axSounds =
			Zenith_AudioBus::GetEmittedSoundsForTest();
		for (u_int u = 0; u < axSounds.GetSize(); ++u)
		{
			if (std::strcmp(axSounds.Get(u).m_szName, "DP.Villager.Footstep") == 0)
			{
				++iCount;
			}
		}
		return iCount;
	}
}

static void Setup_W3FootstepCadence()
{
	g_iPhaseC = kC_Start;
	g_xCVillager = INVALID_ENTITY_ID;
	g_iCBaselineCount = 0;
	g_iCFirstWindowCount = -1;
	g_iCFinalCount = -1;
	g_iCWalkFrames = 0;
	// Deterministic cadence needs deterministic dt (personality-test pattern;
	// released in Verify - there is no Teardown hook).
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
}

static bool Step_W3FootstepCadence(int iFrame)
{
	switch (g_iPhaseC)
	{
	case kC_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhaseC = kC_WaitScene;
		return true;

	case kC_WaitScene:
		g_xCVillager = W3_FindFirstVillager();
		if (g_xCVillager.IsValid())
		{
			g_iPhaseC = kC_Possess;
		}
		else if (iFrame > 120)
		{
			g_iPhaseC = kC_Done;
		}
		return true;

	case kC_Possess:
		DP_Player::SetPossessedVillager(g_xCVillager);
		g_iPhaseC = kC_Baseline;
		return true;

	case kC_Baseline:
		// One settle frame after possession, then snapshot the recorder and
		// start moving. No sprint/quiet modifiers - plain jog.
		g_iCBaselineCount = W3_CountFootsteps();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		g_iCWalkFrames = 0;
		g_iPhaseC = kC_FirstStepWindow;
		return true;

	case kC_FirstStepWindow:
		++g_iCWalkFrames;
		if (g_iCWalkFrames >= kC_FIRST_WINDOW)
		{
			// The idle countdown is held at 0, so the FIRST moving frame
			// must already have emitted a step.
			g_iCFirstWindowCount = W3_CountFootsteps() - g_iCBaselineCount;
			g_iPhaseC = kC_Walk;
		}
		return true;

	case kC_Walk:
		++g_iCWalkFrames;
		if (g_iCWalkFrames >= kC_WALK_FRAMES)
		{
			g_iCFinalCount = W3_CountFootsteps() - g_iCBaselineCount;
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
			g_iPhaseC = kC_Done;
			return false;
		}
		return true;

	case kC_Done:
	default:
		return false;
	}
}

static bool Verify_W3FootstepCadence()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_xCVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3FootstepCadence: villager not found");
		return false;
	}
	Zenith_Log(LOG_CATEGORY_AI, "W3FootstepCadence: firstWindow=%d (expect 1) total=%d (expect %d) over %d frames",
		g_iCFirstWindowCount, g_iCFinalCount, kC_EXPECTED_STEPS, kC_WALK_FRAMES);
	if (g_iCFirstWindowCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3FootstepCadence: first step was not immediate (countdown-held-at-zero quirk lost)");
		return false;
	}
	if (g_iCFinalCount != kC_EXPECTED_STEPS)
	{
		Zenith_Log(LOG_CATEGORY_AI, "W3FootstepCadence: cadence drifted from footstep_interval_s");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xW3FootstepCadenceTest = {
	"Test_W3Villager_FootstepCadence",
	&Setup_W3FootstepCadence,
	&Step_W3FootstepCadence,
	&Verify_W3FootstepCadence,
	600
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xW3FootstepCadenceTest);

#endif // ZENITH_INPUT_SIMULATOR
