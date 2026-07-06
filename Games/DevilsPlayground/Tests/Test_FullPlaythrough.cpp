#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "Maths/Zenith_Maths.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPDoor_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/DPHUDController_Component.h"
#include "Components/DPOrbitCamera_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Tests/DP_TestGraphHelpers.h"
#include "Components/DPGraphInteractable_Component.h"
#include "Components/Priest_Component.h"

#include <cstdio>

// ============================================================================
// FullPlaythrough_Test
// ----------------------------------------------------------------------------
// End-to-end smoke that walks every gameplay system in DevilsPlayground:
//
//   1. Scene load (FrontEnd → GameLevel) populates 17 villagers, 15 doors,
//      6 chests, 1 priest, 1 pentagram, plus the runtime-spawned 15 items.
//   2. Possession: DP_Player::SetPossessedVillager bumps villager life and
//      flips its m_bIsPossessed flag.
//   3. WASD movement actually moves the possessed villager (verified via
//      Zenith_InputSimulator::SimulateKeyDown/Up + position delta).
//   4. Orbit camera snaps onto the villager (DPOrbitCamera_Component:75).
//   5. HUD reflects state: LifeBar visible, Objectives counter renders,
//      HeldItem visible-only-when-something-held.
//   6. Item pickup: distance-based pickup via DPItemBase_Component::OnUpdate.
//   7. Door unlock: synthesised key + proximity → DPDoor::IsOpen().
//   8. Chest open: proximity → chest graph blackboard isOpen.
//   9. Forge crafting: held Iron → CraftForTest → held Key + craft count++.
//  10. Noise machine: proximity F-press emits DP_AI sound; priest's
//      blackboard records an investigate-position.
//  11. Pentagram: 5× synthesise objective + proximity-rising-edge → HandleInteract;
//      DP_Win::HasWon flips, DP_OnVictory event fires, HUD status banner updates.
//  12. Pause overlay: ESC simulator key shows the PauseOverlay UI element,
//      ESC again hides it.
//  13. Villager death: SetRemainingLifeForTest(0.01) → DP_OnVillagerDied
//      event fires, possession cleared.
//
// Implementation notes:
//
// - Possession uses DP_Player::SetPossessedVillager directly rather than
//   simulating a mouse click + raycast. The raycast path is covered by
//   DPPlayerController_Component and exercised by the diagnostic tests
//   added earlier; here we focus on the gameplay state machine.
//
// - For door / chest / pentagram / noise-machine interactions we pre-flip
//   m_bInteractOnOverlap to true, teleport the villager into range, and
//   tick one frame so DPInteractable_Base::OnEnterRange synchronously
//   fires HandleInteract. This sidesteps the F-press path (covered by
//   DPInteractable's per-frame F-press poll already in place) and keeps
//   the test deterministic.
//
// - Pentagram needs a rising-edge between each of the 5 deliveries, so we
//   teleport the villager out of range, tick a frame to flip
//   m_bWasInRangeLastFrame back to false, then teleport back in.
// ============================================================================

namespace
{
	enum Phase : int
	{
		kFP_Start,
		kFP_WaitForLoad,
		kFP_AssertInitialState,
		kFP_Possess,
		kFP_AssertPossession,
		kFP_MoveStart,
		kFP_MoveWait,
		kFP_MoveEnd,
		kFP_AssertOrbitCamera,
		kFP_PickupSetup,
		kFP_PickupWait,
		kFP_AssertPickup,
		kFP_DoorSetup,
		kFP_DoorWait,
		kFP_AssertDoor,
		kFP_ChestSetup,
		kFP_ChestWait,
		kFP_AssertChest,
		kFP_ForgeSetup,
		kFP_ForgeRunCraft,
		kFP_AssertForge,
		kFP_NoiseSetup,
		kFP_NoiseWait,
		kFP_AssertNoise,
		kFP_PentagramSetup,
		kFP_PentagramDeliver,
		kFP_AssertVictory,
		kFP_PauseOpen,
		kFP_PauseAssertOpen,
		kFP_PauseClose,
		kFP_PauseAssertClosed,
		kFP_DeathSetup,
		kFP_DeathWait,
		kFP_AssertDeath,
		kFP_Done
	};

	int g_iPhase     = kFP_Start;
	int g_iWait      = 0;

	// Captured entities + state.
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xSecondVillager;     // for the death-then-repossess phase
	Zenith_EntityID g_xPickupTarget;
	Zenith_EntityID g_xDoor;
	Zenith_EntityID g_xChest;
	Zenith_EntityID g_xForge;
	Zenith_EntityID g_xNoise;
	Zenith_EntityID g_xPentagram;
	Zenith_EntityID g_xPriest;

	Zenith_Maths::Vector3 g_xMoveStartPos(0.0f);

	int g_iVillagerCountObserved   = 0;
	int g_iDoorCountObserved       = 0;
	int g_iChestCountObserved      = 0;
	int g_iPriestCountObserved     = 0;
	int g_iPentagramCountObserved  = 0;
	int g_iItemCountObserved       = 0;

	float g_fLifeBeforePossess     = 0.0f;
	float g_fLifeAfterPossess      = 0.0f;
	float g_fMoveDistanceObserved  = 0.0f;
	Zenith_Maths::Vector3 g_xCamPosBeforePossess(0.0f);
	Zenith_Maths::Vector3 g_xCamPosAfterMove(0.0f);
	bool  g_bCameraStaysOnCentre   = false;
	bool  g_bCameraIgnoresVillager = false;
	bool  g_bHUDLifeBarVisible     = false;
	bool  g_bHUDObjectivesVisible  = false;
	bool  g_bHUDHeldItemVisible    = false;

	DP_ItemTag g_eHeldAfterPickup  = DP_ItemTag::None;
	bool       g_bDoorOpenedObserved   = false;
	bool       g_bChestOpenedObserved  = false;
	uint32_t   g_uForgeCraftsObserved  = 0;
	DP_ItemTag g_eHeldAfterForge       = DP_ItemTag::None;
	bool       g_bPriestHasInvestigate = false;

	int        g_iObjectivesDelivered  = 0;
	int        g_iPentagramSubPhase    = 0;     // 0=move-out, 1=move-in & deliver
	uint32_t   g_uMaskAfter5            = 0;
	bool       g_bVictoryEventFired    = false;
	bool       g_bHUDShowsVictory      = false;

	bool       g_bPauseOverlayOnOpen   = false;
	bool       g_bPauseOverlayOnClose  = false;

	bool       g_bDeathEventFired      = false;
	Zenith_EntityID g_xPossessedAfterDeath;

	Zenith_EventHandle g_xVictoryHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle g_xDeathHandle   = INVALID_EVENT_HANDLE;

	// Helpers
	template<typename T>
	Zenith_EntityID FindFirstScript()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachComponentInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachComponentInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}

	template<typename T>
	T* GetGameComponent(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<T>();
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	void TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		pxTransform->SetPosition(xPos);
	}

	// Helper: synthesise a held item with the requested tag. Registers a
	// throw-away EntityID with the DP_Items side-table and binds it via
	// DP_Player::SetHeldItem so HUD readouts and recipe checks see it.
	void GiveSyntheticHeldItem(Zenith_EntityID xVillager, DP_ItemTag eTag)
	{
		static uint32_t s_uFakeIdCounter = 0;
		Zenith_EntityID xFake;
		xFake.m_uIndex      = 0xF0000000u | s_uFakeIdCounter++;
		xFake.m_uGeneration = 0xF;
		DP_Items::Internal_RegisterItemTag(xFake, eTag);
		DP_Player::SetHeldItem(xVillager, xFake);
	}

	// Look up a UI element by name in ANY loaded scene's UICanvas. The
	// HUD elements (LifeBar / Objectives / HeldItem / Status / ...) are
	// authored on the GameManager entity in the active gameplay scene,
	// but DPPauseMenuController_Component::OnStart migrates its parent
	// entity (and the PauseOverlay text element on it) to the persistent
	// scene via `entity.DontDestroyOnLoad()` so the
	// controller keeps ticking while the gameplay scene is paused.
	//
	// Originally this helper only walked the active scene -- the
	// FullPlaythrough_Test's pause-overlay phase saw `pauseOpen=0`
	// because PauseOverlay had migrated and the search missed it. Walk
	// every loaded scene; first match wins.
	Zenith_UI::Zenith_UIText* FindHudText(const char* szName)
	{
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[szName, &pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxResult) return;
				pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			});
		return pxResult;
	}

	// Captured by the test's victory subscription — Pentagram fires this
	// once the 5/5 mask is filled.
	void OnVictoryEvent(const DP_OnVictory&) { g_bVictoryEventFired = true; }
	void OnDeathEvent(const DP_OnVillagerDied&) { g_bDeathEventFired = true; }
}

// ----------------------------------------------------------------------------
static void Setup_FullPlaythrough()
{
	g_iPhase = kFP_Start;
	g_iWait = 0;

	g_xVillager        = INVALID_ENTITY_ID;
	g_xSecondVillager  = INVALID_ENTITY_ID;
	g_xPickupTarget    = INVALID_ENTITY_ID;
	g_xDoor            = INVALID_ENTITY_ID;
	g_xChest           = INVALID_ENTITY_ID;
	g_xForge           = INVALID_ENTITY_ID;
	g_xNoise           = INVALID_ENTITY_ID;
	g_xPentagram       = INVALID_ENTITY_ID;
	g_xPriest          = INVALID_ENTITY_ID;
	g_xPossessedAfterDeath = INVALID_ENTITY_ID;

	g_xMoveStartPos = Zenith_Maths::Vector3(0.0f);
	g_xCamPosBeforePossess = Zenith_Maths::Vector3(0.0f);
	g_xCamPosAfterMove     = Zenith_Maths::Vector3(0.0f);
	g_bCameraStaysOnCentre   = false;
	g_bCameraIgnoresVillager = false;
	g_iVillagerCountObserved   = 0;
	g_iDoorCountObserved       = 0;
	g_iChestCountObserved      = 0;
	g_iPriestCountObserved     = 0;
	g_iPentagramCountObserved  = 0;
	g_iItemCountObserved       = 0;

	g_fLifeBeforePossess     = 0.0f;
	g_fLifeAfterPossess      = 0.0f;
	g_fMoveDistanceObserved  = 0.0f;
	g_bHUDLifeBarVisible     = false;
	g_bHUDObjectivesVisible  = false;
	g_bHUDHeldItemVisible    = false;

	g_eHeldAfterPickup       = DP_ItemTag::None;
	g_bDoorOpenedObserved    = false;
	g_bChestOpenedObserved   = false;
	g_uForgeCraftsObserved   = 0;
	g_eHeldAfterForge        = DP_ItemTag::None;
	g_bPriestHasInvestigate  = false;

	g_iObjectivesDelivered = 0;
	g_iPentagramSubPhase   = 0;
	g_uMaskAfter5          = 0;
	g_bVictoryEventFired   = false;
	g_bHUDShowsVictory     = false;

	g_bPauseOverlayOnOpen  = false;
	g_bPauseOverlayOnClose = false;

	g_bDeathEventFired = false;

	// Subscribe to gameplay events for assertions later.
	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictoryEvent);
	g_xDeathHandle   = Zenith_EventDispatcher::Get().Subscribe<DP_OnVillagerDied>(&OnDeathEvent);
}

// ----------------------------------------------------------------------------
static bool Step_FullPlaythrough(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	// ----------------------------------------------------------------------
	// Phase 1: scene load.
	// ----------------------------------------------------------------------
	case kFP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFP_WaitForLoad;
		g_iWait = 0;
		return true;

	case kFP_WaitForLoad:
	{
		++g_iWait;
		const int iVillagers = CountScripts<DPVillager_Component>();
		if (iVillagers > 0)
		{
			g_iPhase = kFP_AssertInitialState;
			return true;
		}
		if (g_iWait > 60) { g_iPhase = kFP_Done; return false; }
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 2: assert initial scene composition.
	// ----------------------------------------------------------------------
	case kFP_AssertInitialState:
	{
		g_iVillagerCountObserved   = CountScripts<DPVillager_Component>();
		g_iDoorCountObserved       = CountScripts<DPDoor_Component>();
		g_iChestCountObserved      = DP_CountEntitiesWithGraph("game:Graphs/DP_Chest.bgraph");
		g_iPriestCountObserved     = CountScripts<Priest_Component>();
		g_iPentagramCountObserved  = DP_CountEntitiesWithGraph("game:Graphs/DP_Pentagram.bgraph");
		g_iItemCountObserved       = CountScripts<DPItemBase_Component>();

		// Capture references for later phases.
		g_xPriest    = FindFirstScript<Priest_Component>();
		g_xPentagram = DP_FindFirstEntityWithGraph("game:Graphs/DP_Pentagram.bgraph");
		// 2026-05-25: doors are now unlocked-by-default. The kFP_DoorSetup
		// phase explicitly gives the villager a Key and exercises the
		// unlock path -- pick a LOCKED door so we actually test the key-
		// consume code path. Falling back to FindFirstScript when no
		// locked door exists keeps the test runnable in artificial
		// configurations (zero pentagram corridors).
		g_xDoor      = INVALID_ENTITY_ID;
		DP_Query::ForEachComponentInActiveScene<DPDoor_Component>(
			[](Zenith_EntityID xId, DPDoor_Component& xScript)
			{
				if (g_xDoor.IsValid()) return;
				if (xScript.GetRequiredKey() == DP_ItemTag::Key)
				{
					g_xDoor = xId;
				}
			});
		if (!g_xDoor.IsValid()) g_xDoor = FindFirstScript<DPDoor_Component>();
		g_xChest     = DP_FindFirstEntityWithGraph("game:Graphs/DP_Chest.bgraph");
		g_xNoise     = DP_FindFirstEntityWithGraph("game:Graphs/DP_NoiseMachine.bgraph");

		// Pick the first two villagers; first is the playthrough hero,
		// second is the death-and-repossess target.
		int iVillagerIdx = 0;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&iVillagerIdx](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (iVillagerIdx == 0) g_xVillager = xId;
				else if (iVillagerIdx == 1) g_xSecondVillager = xId;
				++iVillagerIdx;
			});

		// First Iron item — used in the forge phase.
		// First non-objective item (Iron / Key / SkeletonKey) — used as
		// the pickup target. Avoid Objective tags so the pentagram
		// doesn't pick it up early.
		DP_Query::ForEachComponentInActiveScene<DPItemBase_Component>(
			[](Zenith_EntityID xId, DPItemBase_Component& xItem)
			{
				if (g_xPickupTarget.IsValid()) return;
				const DP_ItemTag eTag = xItem.GetTag();
				if (DP_IsObjectiveTag(eTag)) return;
				g_xPickupTarget = xId;
			});

		std::printf("[FullPlaythrough] state: V=%d D=%d C=%d Pr=%d Pe=%d I=%d "
		            "villagerValid=%d secondVillagerValid=%d pentagramValid=%d "
		            "doorValid=%d chestValid=%d priestValid=%d pickupValid=%d\n",
			g_iVillagerCountObserved, g_iDoorCountObserved, g_iChestCountObserved,
			g_iPriestCountObserved, g_iPentagramCountObserved, g_iItemCountObserved,
			(int)g_xVillager.IsValid(), (int)g_xSecondVillager.IsValid(),
			(int)g_xPentagram.IsValid(), (int)g_xDoor.IsValid(),
			(int)g_xChest.IsValid(), (int)g_xPriest.IsValid(),
			(int)g_xPickupTarget.IsValid());
		std::fflush(stdout);

		g_iPhase = kFP_Possess;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 3: possess.
	// ----------------------------------------------------------------------
	case kFP_Possess:
	{
		// Snapshot camera position BEFORE possession — must not change
		// after possess + move (bird's-eye camera does not follow).
		if (Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes())
		{
			pxCam->GetPosition(g_xCamPosBeforePossess);
		}

		// Deplete the villager's life timer to 1 second so the
		// possession-time bump-to-max is observable in the post-possession
		// snapshot (otherwise life starts at max and "bump" is invisible).
		if (DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(g_xVillager))
		{
			pxV->SetRemainingLifeForTest(1.0f);
			g_fLifeBeforePossess = pxV->GetRemainingLife();
		}
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kFP_AssertPossession;
		g_iWait = 0;
		return true;
	}

	case kFP_AssertPossession:
	{
		++g_iWait;
		// One frame for DPVillager::OnUpdate to flip m_bIsPossessed and bump life.
		if (g_iWait < 2) return true;
		if (DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(g_xVillager))
		{
			g_fLifeAfterPossess = pxV->GetRemainingLife();
		}
		// HUD: LifeBar should be visible after possession; Objectives always.
		if (auto* pxLife = FindHudText("LifeBar"))    g_bHUDLifeBarVisible    = pxLife->IsVisible();
		if (auto* pxObj  = FindHudText("Objectives")) g_bHUDObjectivesVisible = pxObj->IsVisible();
		g_iPhase = kFP_MoveStart;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 4: WASD movement.
	// ----------------------------------------------------------------------
	case kFP_MoveStart:
	{
		TryGetEntityPos(g_xVillager, g_xMoveStartPos);
		// Hold W to move forward (DP_Input::ReadMoveVillager maps W to +y).
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		g_iPhase = kFP_MoveWait;
		g_iWait = 0;
		return true;
	}

	case kFP_MoveWait:
	{
		++g_iWait;
		if (g_iWait < 30) return true;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		g_iPhase = kFP_MoveEnd;
		return true;
	}

	case kFP_MoveEnd:
	{
		Zenith_Maths::Vector3 xCurrent;
		if (TryGetEntityPos(g_xVillager, xCurrent))
		{
			const float fDx = xCurrent.x - g_xMoveStartPos.x;
			const float fDz = xCurrent.z - g_xMoveStartPos.z;
			g_fMoveDistanceObserved = std::sqrt(fDx * fDx + fDz * fDz);
		}
		g_iPhase = kFP_AssertOrbitCamera;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 5: bird's-eye camera invariants — stays pinned to map centre,
	// does NOT chase the possessed villager. Compare camera position now
	// (after possession + 30 frames of WASD movement) against the snapshot
	// taken before possession; the delta should be tiny.
	// ----------------------------------------------------------------------
	case kFP_AssertOrbitCamera:
	{
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
		Zenith_Maths::Vector3 xVPos(0.0f);
		if (pxCam) pxCam->GetPosition(g_xCamPosAfterMove);
		TryGetEntityPos(g_xVillager, xVPos);

		// 1) Camera stays high above the map centre, not over the villager.
		//    Map centre is (50, 0, 50). DPOrbitCamera_Component orbits
		//    that point with radius 80 m at ~69° pitch, putting the camera
		//    ~75 m up and ~29 m off-centre (along whichever axis the orbit
		//    yaw lands on — currently +π/2 puts the camera west of centre
		//    looking east). Check horizontal distance from centre is in
		//    the orbit-radius ballpark, not which specific axis.
		const bool bHighElevation = g_xCamPosAfterMove.y > 50.0f;
		const float fDx = g_xCamPosAfterMove.x - 50.0f;
		const float fDz = g_xCamPosAfterMove.z - 50.0f;
		const float fHorizDist = std::sqrt(fDx * fDx + fDz * fDz);
		const bool bAboveMapCentreX = fHorizDist < 40.0f;  // orbit_radius * cos(pitch) ~= 28.8 m
		g_bCameraStaysOnCentre = bHighElevation && bAboveMapCentreX;

		// 2) Camera position barely moves between pre-possession and post-
		//    move snapshot. A close-following third-person camera would
		//    have shifted by tens of metres as the villager moved.
		const float fCdx = g_xCamPosAfterMove.x - g_xCamPosBeforePossess.x;
		const float fCdy = g_xCamPosAfterMove.y - g_xCamPosBeforePossess.y;
		const float fCdz = g_xCamPosAfterMove.z - g_xCamPosBeforePossess.z;
		const float fCamDelta = std::sqrt(fCdx*fCdx + fCdy*fCdy + fCdz*fCdz);
		g_bCameraIgnoresVillager      = fCamDelta < 1.0f;

		g_iPhase = kFP_PickupSetup;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 6: pickup an item by walking onto it (distance-based pickup).
	// ----------------------------------------------------------------------
	case kFP_PickupSetup:
	{
		// Make sure villager has nothing held.
		DP_Player::RemoveHeldItem(g_xVillager);
		// Teleport villager onto the item — DPItemBase's OnUpdate auto-picks
		// up when the possessed villager is within m_fPickupRadius.
		Zenith_Maths::Vector3 xItemPos;
		if (TryGetEntityPos(g_xPickupTarget, xItemPos))
		{
			TrySetEntityPos(g_xVillager, xItemPos);
		}
		g_iPhase = kFP_PickupWait;
		g_iWait = 0;
		return true;
	}

	case kFP_PickupWait:
	{
		++g_iWait;
		// 3 frames — DPItemBase OnUpdate runs once per frame on each item.
		if (g_iWait < 3) return true;
		g_eHeldAfterPickup = DP_Player::GetHeldItemTag(g_xVillager);
		// Now the HUD's HeldItem element should be visible.
		if (auto* pxHeld = FindHudText("HeldItem")) g_bHUDHeldItemVisible = pxHeld->IsVisible();
		g_iPhase = kFP_DoorSetup;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 7: door unlock with key.
	// ----------------------------------------------------------------------
	case kFP_DoorSetup:
	{
		// Drop whatever was picked up; give the villager a Key explicitly.
		DP_Player::RemoveHeldItem(g_xVillager);
		GiveSyntheticHeldItem(g_xVillager, DP_ItemTag::Key);

		// Teleport villager onto the door's logical centre (NOT the entity
		// transform -- corner-anchored SM_Cube offsets the transform by
		// ~1 m), flip interact-on-overlap so the next OnUpdate's rising-
		// edge dispatches HandleInteract directly.
		if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
		{
			TrySetEntityPos(g_xVillager, pxDoor->GetInteractionCentre());
			pxDoor->SetInteractOnOverlap(true);
		}
		g_iPhase = kFP_DoorWait;
		g_iWait = 0;
		return true;
	}

	case kFP_DoorWait:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		if (DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor))
		{
			g_bDoorOpenedObserved = pxDoor->IsOpen();
		}
		g_iPhase = kFP_AssertDoor;
		return true;
	}

	case kFP_AssertDoor:
		// (Just a state-machine pass-through — the value is captured above.)
		g_iPhase = kFP_ChestSetup;
		return true;

	// ----------------------------------------------------------------------
	// Phase 8: chest open.
	// ----------------------------------------------------------------------
	case kFP_ChestSetup:
	{
		Zenith_Maths::Vector3 xChestPos;
		if (TryGetEntityPos(g_xChest, xChestPos))
		{
			TrySetEntityPos(g_xVillager, xChestPos);
		}
		if (DPGraphInteractable_Component* pxChestShim = GetGameComponent<DPGraphInteractable_Component>(g_xChest))
		{
			pxChestShim->SetInteractOnOverlap(true);
		}
		g_iPhase = kFP_ChestWait;
		g_iWait = 0;
		return true;
	}

	case kFP_ChestWait:
	{
		++g_iWait;
		if (g_iWait < 3) return true;
		g_bChestOpenedObserved = DP_GetGraphBool(g_xChest, "game:Graphs/DP_Chest.bgraph", "isOpen");
		g_iPhase = kFP_AssertChest;
		return true;
	}

	case kFP_AssertChest:
		g_iPhase = kFP_ForgeSetup;
		return true;

	// ----------------------------------------------------------------------
	// Phase 9: forge crafting (Iron → Key).
	//
	// GameLevel doesn't author a forge entity (it lives in Gym_Forge), so we
	// build one at runtime — same trick as Test_DoubleDoorAndForge.cpp does.
	// ----------------------------------------------------------------------
	case kFP_ForgeSetup:
	{
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
		if (!pxScene) { g_iPhase = kFP_Done; return false; }

		Zenith_Entity xForge = g_xEngine.Scenes().CreateEntity(pxScene, std::string("FullPlaythrough_Forge"));
		g_xForge = xForge.GetEntityID();
		xForge.GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(80.0f, 0.0f, 80.0f));
		xForge.AddComponent<DPForge_Component>();

		// Hand the villager an Iron and trigger the recipe via the test bypass.
		GiveSyntheticHeldItem(g_xVillager, DP_ItemTag::Iron);

		g_iPhase = kFP_ForgeRunCraft;
		g_iWait = 0;
		return true;
	}

	case kFP_ForgeRunCraft:
	{
		if (DPForge_Component* pxForge = GetGameComponent<DPForge_Component>(g_xForge))
		{
			pxForge->CraftForTest(g_xVillager);
			g_uForgeCraftsObserved = pxForge->GetCraftCount();
		}
		g_eHeldAfterForge = DP_Player::GetHeldItemTag(g_xVillager);
		g_iPhase = kFP_AssertForge;
		return true;
	}

	case kFP_AssertForge:
		g_iPhase = kFP_NoiseSetup;
		return true;

	// ----------------------------------------------------------------------
	// Phase 10: noise machine → priest investigates.
	// ----------------------------------------------------------------------
	case kFP_NoiseSetup:
	{
		// Drop the held key from the forge so the next phase's pentagram
		// loop doesn't think we already have something delivered.
		DP_Player::RemoveHeldItem(g_xVillager);

		if (!g_xNoise.IsValid())
		{
			// GameLevel only authors one noise machine; if it was lost
			// somehow, skip rather than fail the whole test.
			g_iPhase = kFP_PentagramSetup;
			return true;
		}
		// The authored noise machine sits at (0,0,8); the priest at ~(56,1,62)
		// is well outside its 20 m noise radius. For test purposes, teleport
		// the noise machine to within earshot of the priest so the perception
		// chain (NoiseMachine → DP_AI::EmitNoise → PerceptionSystem → priest
		// blackboard) actually fires.
		Zenith_Maths::Vector3 xPriestPos;
		TryGetEntityPos(g_xPriest, xPriestPos);
		const Zenith_Maths::Vector3 xNoiseNewPos(
			xPriestPos.x + 3.0f, xPriestPos.y, xPriestPos.z + 3.0f);
		TrySetEntityPos(g_xNoise, xNoiseNewPos);

		// Move the villager onto the noise machine to drive the rising-edge.
		TrySetEntityPos(g_xVillager, xNoiseNewPos);

		if (DPGraphInteractable_Component* pxNoiseShim =
				GetGameComponent<DPGraphInteractable_Component>(g_xNoise))
		{
			pxNoiseShim->SetInteractOnOverlap(true);
		}
		g_iPhase = kFP_NoiseWait;
		g_iWait = 0;
		return true;
	}

	case kFP_NoiseWait:
	{
		++g_iWait;
		// A noise stimulus needs time to propagate through the perception
		// system + priest BB tick (PerceptionSystem::Update runs in
		// DPPlayerController::OnUpdate). Wait a few frames.
		if (g_iWait < 5) return true;

		// Read the priest's blackboard for the investigate-pos flag.
		Zenith_Entity xPriestEnt = g_xEngine.Scenes().ResolveEntity(g_xPriest);
		if (Priest_Component* pxPriestC = xPriestEnt.TryGetComponent<Priest_Component>())
		{
			// W3: the bridge writes the priest's decision blackboard.
			g_bPriestHasInvestigate = pxPriestC->ReadBBBool(
				DP_AI::BB_KEY_HAS_INVESTIGATE_POS, /*bDefault=*/false);
		}
		g_iPhase = kFP_AssertNoise;
		return true;
	}

	case kFP_AssertNoise:
		g_iPhase = kFP_PentagramSetup;
		return true;

	// ----------------------------------------------------------------------
	// Phase 11: pentagram — deliver 5 objectives → win.
	// ----------------------------------------------------------------------
	case kFP_PentagramSetup:
	{
		if (DPGraphInteractable_Component* pxPentShim =
				GetGameComponent<DPGraphInteractable_Component>(g_xPentagram))
		{
			pxPentShim->SetInteractOnOverlap(true);
		}
		g_iObjectivesDelivered = 0;
		g_iPentagramSubPhase   = 0;
		g_iPhase = kFP_PentagramDeliver;
		g_iWait  = 0;
		return true;
	}

	case kFP_PentagramDeliver:
	{
		// Sub-phase 0: ensure villager is OUT of range, give an objective,
		// then teleport into range. Sub-phase 1: wait one frame for the
		// rising-edge to fire HandleInteract, then return to sub-phase 0
		// for the next objective.
		Zenith_Maths::Vector3 xPentPos;
		TryGetEntityPos(g_xPentagram, xPentPos);

		if (g_iPentagramSubPhase == 0)
		{
			// Move out of range (pentagram radius is 2 m by default).
			TrySetEntityPos(g_xVillager,
				Zenith_Maths::Vector3(xPentPos.x + 50.0f, xPentPos.y, xPentPos.z));

			// Give the next objective tag.
			static constexpr DP_ItemTag kObjTags[5] = {
				DP_ItemTag::Objective1, DP_ItemTag::Objective2,
				DP_ItemTag::Objective3, DP_ItemTag::Objective4,
				DP_ItemTag::Objective5
			};
			DP_Player::RemoveHeldItem(g_xVillager);
			GiveSyntheticHeldItem(g_xVillager, kObjTags[g_iObjectivesDelivered]);
			g_iPentagramSubPhase = 1;
			return true;
		}

		if (g_iPentagramSubPhase == 1)
		{
			// Move INTO range — next OnUpdate's rising-edge will trigger.
			TrySetEntityPos(g_xVillager, xPentPos);
			g_iPentagramSubPhase = 2;
			return true;
		}

		if (g_iPentagramSubPhase == 2)
		{
			// One frame for the rising-edge to fire HandleInteract.
			++g_iObjectivesDelivered;
			g_iPentagramSubPhase = 0;
			if (g_iObjectivesDelivered >= 5)
			{
				g_iPhase = kFP_AssertVictory;
				g_iWait  = 0;
			}
			return true;
		}
		return true;
	}

	case kFP_AssertVictory:
	{
		++g_iWait;
		// One more frame for the OnVictory event subscriber to update HUD
		// + the test's victory flag.
		if (g_iWait < 2) return true;
		g_uMaskAfter5 = DP_Win::GetCollectedObjectivesMask();
		if (auto* pxStatus = FindHudText("Status"))
		{
			g_bHUDShowsVictory = pxStatus->IsVisible() &&
				(pxStatus->GetText().find("VICTORY") != std::string::npos);
		}
		g_iPhase = kFP_PauseOpen;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 12: pause overlay (Esc to open / close).
	// ----------------------------------------------------------------------
	case kFP_PauseOpen:
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kFP_PauseAssertOpen;
		g_iWait = 0;
		return true;
	}

	case kFP_PauseAssertOpen:
	{
		++g_iWait;
		if (g_iWait < 2) return true;
		if (auto* pxOverlay = FindHudText("PauseOverlay"))
		{
			g_bPauseOverlayOnOpen = pxOverlay->IsVisible();
		}
		g_iPhase = kFP_PauseClose;
		return true;
	}

	case kFP_PauseClose:
	{
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iPhase = kFP_PauseAssertClosed;
		g_iWait = 0;
		return true;
	}

	case kFP_PauseAssertClosed:
	{
		++g_iWait;
		if (g_iWait < 2) return true;
		if (auto* pxOverlay = FindHudText("PauseOverlay"))
		{
			g_bPauseOverlayOnClose = !pxOverlay->IsVisible();
		}
		g_iPhase = kFP_DeathSetup;
		return true;
	}

	// ----------------------------------------------------------------------
	// Phase 13: villager death (life timer expires) clears possession.
	// ----------------------------------------------------------------------
	case kFP_DeathSetup:
	{
		// Possess the second villager so the death tick has something to fire on.
		DP_Player::SetPossessedVillager(g_xSecondVillager);
		g_bDeathEventFired = false; // reset (events from earlier phases)
		g_iPhase = kFP_DeathWait;
		g_iWait = 0;
		return true;
	}

	case kFP_DeathWait:
	{
		++g_iWait;
		// Wait one frame for OnUpdate to flip the possession-bumped life
		// (m_bIsPossessed flips on rising edge), THEN shrink the timer.
		if (g_iWait == 1)
		{
			if (DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(g_xSecondVillager))
			{
				pxV->SetRemainingLifeForTest(0.005f);
			}
			return true;
		}
		if (g_iWait < 4) return true;
		g_xPossessedAfterDeath = DP_Player::GetPossessedVillager();
		g_iPhase = kFP_AssertDeath;
		return true;
	}

	case kFP_AssertDeath:
	{
		std::printf("[FullPlaythrough] summary: "
			"life_before=%.2f life_after=%.2f move=%.2fm "
			"camPre=(%.1f,%.1f,%.1f) camPost=(%.1f,%.1f,%.1f) "
			"camCentre=%d camStill=%d "
			"hud_life=%d hud_obj=%d hud_held=%d held_tag=%d "
			"door=%d chest=%d forgeCrafts=%u heldAfterForge=%d "
			"priestHasInvestigate=%d mask=0x%X victory=%d hudVictory=%d "
			"pauseOpen=%d pauseClose=%d deathFired=%d possessedAfter=(%u,%u)\n",
			g_fLifeBeforePossess, g_fLifeAfterPossess, g_fMoveDistanceObserved,
			g_xCamPosBeforePossess.x, g_xCamPosBeforePossess.y, g_xCamPosBeforePossess.z,
			g_xCamPosAfterMove.x,     g_xCamPosAfterMove.y,     g_xCamPosAfterMove.z,
			(int)g_bCameraStaysOnCentre, (int)g_bCameraIgnoresVillager,
			(int)g_bHUDLifeBarVisible, (int)g_bHUDObjectivesVisible,
			(int)g_bHUDHeldItemVisible, (int)g_eHeldAfterPickup,
			(int)g_bDoorOpenedObserved, (int)g_bChestOpenedObserved,
			g_uForgeCraftsObserved, (int)g_eHeldAfterForge,
			(int)g_bPriestHasInvestigate, g_uMaskAfter5,
			(int)g_bVictoryEventFired, (int)g_bHUDShowsVictory,
			(int)g_bPauseOverlayOnOpen, (int)g_bPauseOverlayOnClose,
			(int)g_bDeathEventFired,
			g_xPossessedAfterDeath.m_uIndex, g_xPossessedAfterDeath.m_uGeneration);
		std::fflush(stdout);
		g_iPhase = kFP_Done;

		// Clean up event subscriptions before exit.
		if (g_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
			g_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xDeathHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xDeathHandle);
			g_xDeathHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}

	case kFP_Done:
	default:
		return false;
	}
}

// ----------------------------------------------------------------------------
static bool Verify_FullPlaythrough()
{
	// Initial state: villagers / doors / chests / priest / pentagram all spawned.
	if (g_iVillagerCountObserved   < 1)  return false;
	if (g_iDoorCountObserved       < 1)  return false;
	if (g_iChestCountObserved      < 1)  return false;
	if (g_iPriestCountObserved     != 1) return false;
	if (g_iPentagramCountObserved  != 1) return false;
	if (g_iItemCountObserved       < 1)  return false;
	if (!g_xVillager.IsValid())          return false;
	if (!g_xSecondVillager.IsValid())    return false;
	if (!g_xPickupTarget.IsValid())      return false;

	// Possession bumped life and HUD reacted.
	if (g_fLifeAfterPossess <= 0.0f)         return false;
	if (g_fLifeAfterPossess < g_fLifeBeforePossess) return false;
	if (!g_bHUDLifeBarVisible)               return false;
	if (!g_bHUDObjectivesVisible)            return false;

	// Movement: WASD actually moved the villager. Speed is ~5 m/s
	// (DPVillager) × 0.01666 dt × 30 frames ≈ 2.5 m. We accept ≥ 0.5 m
	// to absorb step-counter noise.
	if (g_fMoveDistanceObserved < 0.5f)      return false;

	// Bird's-eye camera invariants: pinned over the map centre AND did
	// NOT chase the villager during the 30-frame WASD movement test.
	if (!g_bCameraStaysOnCentre)             return false;
	if (!g_bCameraIgnoresVillager)           return false;

	// Item pickup landed something in the held slot.
	if (g_eHeldAfterPickup == DP_ItemTag::None) return false;
	if (!g_bHUDHeldItemVisible)              return false;

	// Door opened.
	if (!g_bDoorOpenedObserved)              return false;
	// Chest opened.
	if (!g_bChestOpenedObserved)             return false;
	// Forge consumed the iron and produced a key.
	if (g_uForgeCraftsObserved != 1)         return false;
	if (g_eHeldAfterForge != DP_ItemTag::Key) return false;

	// Priest heard the noise machine.
	if (!g_bPriestHasInvestigate)            return false;

	// Pentagram win condition: all 5 objectives delivered, event fired,
	// HUD banner reflects.
	if (g_uMaskAfter5 != DP_ALL_OBJECTIVES_MASK) return false;
	if (!g_bVictoryEventFired)               return false;
	if (!g_bHUDShowsVictory)                 return false;

	// Pause overlay opens and closes on Esc.
	if (!g_bPauseOverlayOnOpen)              return false;
	if (!g_bPauseOverlayOnClose)             return false;

	// Death cleared possession.
	if (!g_bDeathEventFired)                 return false;
	if (g_xPossessedAfterDeath.IsValid())    return false;

	return true;
}

static const Zenith_AutomatedTest g_xFullPlaythroughTest = {
	"FullPlaythrough_Test",
	&Setup_FullPlaythrough,
	&Step_FullPlaythrough,
	&Verify_FullPlaythrough,
	600, // ~10 seconds at 60 Hz — generous for ~80 sequential phases
	true // m_bRequiresGraphics: end-to-end gameplay smoke; loads scenes + HUD UI
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFullPlaythroughTest);

#endif // ZENITH_INPUT_SIMULATOR
