#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPDoor_Component.h"
#include "Tests/DP_TestGraphHelpers.h"
#include "Components/Priest_Component.h"
#include "Components/DPHUDController_Component.h"
#include "Components/DPOrbitCamera_Component.h"

// ============================================================================
// Six tests covering gameplay systems not yet directly tested:
//
//   DoorUnlock_Test       — Held key + proximity → door opens, key consumed
//   VillagerDeath_Test    — Possessed villager life timer expires → DP_OnVillagerDied + possession cleared
//   ChestInteract_Test    — F-press near chest → graph blackboard isOpen flips
//   NoiseMachineFlow_Test — F-press near noise machine → priest hears (BB.HasInvestigatePos true)
//   OrbitCameraFollowsPossession_Test — No possession → zero camera drift; possess → blends to third-person near the villager
//   HUDLifeBar_Test       — Possess villager → HUD's "LifeBar" UI text becomes visible + non-empty
//
// Each follows the multi-frame state-machine pattern from Test_Possession.cpp.
// All use the runtime-spawned items + scene from DPItemManager_Component.
// ============================================================================

namespace
{
	template<typename T>
	Zenith_EntityID FindFirstEntityWith()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachComponentInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	T* GetGameComponent(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<T>();
	}

	bool TryGetEntityPosition(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	void TrySetEntityPosition(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		pxTransform->SetPosition(xPos);
	}
}

// ============================================================================
// DoorUnlock_Test
// ============================================================================
namespace DoorUnlockState
{
	enum Phase : int { kStart, kWait, kEnterRange, kPressF, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xDoor;
	Zenith_EntityID g_xKeyItem;
	bool            g_bDoorWasOpen     = true;  // initial state we expect false
	bool            g_bDoorIsOpen      = false;
	DP_ItemTag      g_eHeldAfter       = DP_ItemTag::None;
}

static void Setup_DoorUnlock()
{
	using namespace DoorUnlockState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xDoor = INVALID_ENTITY_ID;
	g_xKeyItem = INVALID_ENTITY_ID;
	g_bDoorWasOpen = true;
	g_bDoorIsOpen = false;
	g_eHeldAfter = DP_ItemTag::None;
}

static bool Step_DoorUnlock(int /*iFrame*/)
{
	using namespace DoorUnlockState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		// 2026-05-25: most doors are now unlocked-by-default; "first door
		// found" would silently pass the consume-key assertion against an
		// unlocked door (the door opens without needing a key, the test
		// just sees the open transition). Filter for a door that
		// actually requires a Key so the unlock path is the only way to
		// open it.
		g_xDoor = INVALID_ENTITY_ID;
		DP_Query::ForEachComponentInActiveScene<DPDoor_Component>(
			[](Zenith_EntityID xId, DPDoor_Component& xScript)
			{
				if (g_xDoor.IsValid()) return;
				if (xScript.GetRequiredKey() == DP_ItemTag::Key)
				{
					g_xDoor = xId;
				}
			});
		if (!g_xVillager.IsValid() || !g_xDoor.IsValid()) return true;

		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		g_bDoorWasOpen = pxDoor->IsOpen();   // expect false

		// Synthesize a held key item without going through the spawner —
		// scene already has many items, but we want a guaranteed Key tag.
		// Simplest path: register a fake EntityID under the Key tag directly,
		// then SetHeldItem points at it. The TryConsumeKeyForUnlock path
		// destroys the item entity if it's valid; since our fake ID has no
		// real entity behind it, the destroy is a no-op (GetSceneDataForEntity
		// returns null, the path early-exits — see PublicInterfaces.cpp).
		g_xKeyItem.m_uIndex      = 999000;  // outside any real scene's slot range
		g_xKeyItem.m_uGeneration = 1;
		const DP_ItemTag eRequired = pxDoor->GetRequiredKey();
		DP_Items::Internal_RegisterItemTag(g_xKeyItem, eRequired);
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::SetHeldItem(g_xVillager, g_xKeyItem);

		// Teleport villager to within the door's interact radius (default 2m).
		// 2026-05-25: use the door's LOGICAL centre, not the entity
		// transform -- the transform is corner-offset by ~1 m via SM_Cube
		// anchoring, so teleporting to entity.position + 1m east would
		// land the villager just outside (or right at the edge of) the
		// 2m interact radius.
		const Zenith_Maths::Vector3 xDoorPos = pxDoor->GetInteractionCentre();
		TrySetEntityPosition(g_xVillager, xDoorPos + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		g_iPhase = kEnterRange;
		return true;
	}

	case kEnterRange:
		// Skip a frame for DPInteractable to detect the rising-edge
		// (in-range detection runs in next OnUpdate after teleport).
		g_iPhase = kPressF;
		return true;

	case kPressF:
		// Now we're in-range. Press F. DPInteractable's per-frame F-press
		// poll fires DP_OnInteract → HandleInteract → TryConsumeKeyForUnlock
		// → m_bIsOpen=true.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor != nullptr) g_bDoorIsOpen = pxDoor->IsOpen();
		g_eHeldAfter = DP_Player::GetHeldItemTag(g_xVillager);

		// Cleanup: unregister our synthetic key tag so it doesn't leak into
		// follow-on tests within the same process.
		DP_Items::Internal_UnregisterItemTag(g_xKeyItem);
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_DoorUnlock()
{
	using namespace DoorUnlockState;
	if (!g_xVillager.IsValid()) return false;
	if (!g_xDoor.IsValid())     return false;
	if (g_bDoorWasOpen)         return false;        // door must START closed
	if (!g_bDoorIsOpen)         return false;        // door must end OPEN
	if (g_eHeldAfter != DP_ItemTag::None) return false;  // key consumed
	return true;
}

static const Zenith_AutomatedTest g_xDoorUnlockTest = {
	"DoorUnlock_Test",
	&Setup_DoorUnlock,
	&Step_DoorUnlock,
	&Verify_DoorUnlock,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDoorUnlockTest);

// ============================================================================
// VillagerDeath_Test
// ============================================================================
namespace VillagerDeathState
{
	enum Phase : int { kStart, kWait, kPossess, kAfterBump, kTick, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	bool            g_bDeathFired      = false;
	Zenith_EventHandle g_xHandle = INVALID_EVENT_HANDLE;
	bool            g_bPossessionCleared = false;
	int             g_iWait              = 0;
}

static void Setup_VillagerDeath()
{
	using namespace VillagerDeathState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_bDeathFired = false;
	g_xHandle = INVALID_EVENT_HANDLE;
	g_bPossessionCleared = false;
	g_iWait = 0;
}

static bool Step_VillagerDeath(int /*iFrame*/)
{
	using namespace VillagerDeathState;
	switch (g_iPhase)
	{
	case kStart:
		g_xHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVillagerDied>(
			[](const DP_OnVillagerDied&) { g_bDeathFired = true; });
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		if (!g_xVillager.IsValid()) return true;
		g_iPhase = kPossess;
		return true;
	}

	case kPossess:
	{
		// Possess. Next frame the villager's OnUpdate observes the possession
		// flip and BUMPS m_fRemainingLife back to m_fMaxLife (30s). We must
		// wait for that bump before shrinking — otherwise our 0.05s setting
		// gets clobbered.
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kAfterBump;
		return true;
	}

	case kAfterBump:
	{
		// Bump has happened (life is now 30). Shrink to 0.05s so the timer
		// expires within a small frame budget.
		DPVillager_Component* pxV = GetGameComponent<DPVillager_Component>(g_xVillager);
		if (pxV != nullptr)
		{
			pxV->SetRemainingLifeForTest(0.05f);
		}
		g_iPhase = kTick;
		return true;
	}

	case kTick:
		++g_iWait;
		if (g_iWait >= 10) g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		// Possession should have been cleared inside TickLife when life hit 0.
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		g_bPossessionCleared = !xPossessed.IsValid();

		Zenith_EventDispatcher::Get().Unsubscribe(g_xHandle);
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_VillagerDeath()
{
	using namespace VillagerDeathState;
	return g_xVillager.IsValid() && g_bDeathFired && g_bPossessionCleared;
}

static const Zenith_AutomatedTest g_xVillagerDeathTest = {
	"VillagerDeath_Test",
	&Setup_VillagerDeath,
	&Step_VillagerDeath,
	&Verify_VillagerDeath,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xVillagerDeathTest);

// ============================================================================
// ChestInteract_Test
// ============================================================================
namespace ChestInteractState
{
	enum Phase : int { kStart, kWait, kEnterRange, kPressF, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xChest;
	bool            g_bChestWasClosed = false;
	bool            g_bChestIsOpen    = false;
}

static void Setup_ChestInteract()
{
	using namespace ChestInteractState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xChest = INVALID_ENTITY_ID;
	g_bChestWasClosed = false;
	g_bChestIsOpen = false;
}

static bool Step_ChestInteract(int /*iFrame*/)
{
	using namespace ChestInteractState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		g_xChest    = DP_FindFirstEntityWithGraph("game:Graphs/DP_Chest.bgraph");
		if (!g_xVillager.IsValid() || !g_xChest.IsValid()) return true;

		if (DP_GetGraphOn(g_xChest, "game:Graphs/DP_Chest.bgraph") == nullptr) return true;
		g_bChestWasClosed = !DP_GetGraphBool(g_xChest, "game:Graphs/DP_Chest.bgraph", "isOpen");

		// Possess + teleport into chest's interact radius (default 2m).
		DP_Player::SetPossessedVillager(g_xVillager);
		Zenith_Maths::Vector3 xChestPos;
		if (TryGetEntityPosition(g_xChest, xChestPos))
		{
			TrySetEntityPosition(g_xVillager, xChestPos + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		}
		g_iPhase = kEnterRange;
		return true;
	}

	case kEnterRange:
		// One frame for DPInteractable rising-edge detection.
		g_iPhase = kPressF;
		return true;

	case kPressF:
		// In-range now; press F → DP_OnInteract → DPChest::HandleInteract.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		g_bChestIsOpen = DP_GetGraphBool(g_xChest, "game:Graphs/DP_Chest.bgraph", "isOpen");
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_ChestInteract()
{
	using namespace ChestInteractState;
	return g_xVillager.IsValid() && g_xChest.IsValid()
	    && g_bChestWasClosed && g_bChestIsOpen;
}

static const Zenith_AutomatedTest g_xChestInteractTest = {
	"ChestInteract_Test",
	&Setup_ChestInteract,
	&Step_ChestInteract,
	&Verify_ChestInteract,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xChestInteractTest);

// ============================================================================
// NoiseMachineFlow_Test
// ============================================================================
namespace NoiseMachineFlowState
{
	enum Phase : int { kStart, kWait, kEnterRange, kPressF, kPropagate, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xPriest;
	Zenith_EntityID g_xNoise;
	bool            g_bPriestHeard = false;
	int             g_iWait        = 0;
}

static void Setup_NoiseMachineFlow()
{
	using namespace NoiseMachineFlowState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xPriest = INVALID_ENTITY_ID;
	g_xNoise = INVALID_ENTITY_ID;
	g_bPriestHeard = false;
	g_iWait = 0;
}

static bool Step_NoiseMachineFlow(int /*iFrame*/)
{
	using namespace NoiseMachineFlowState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		g_xPriest   = FindFirstEntityWith<Priest_Component>();
		g_xNoise    = DP_FindFirstEntityWithGraph("game:Graphs/DP_NoiseMachine.bgraph");
		if (!g_xVillager.IsValid() || !g_xPriest.IsValid() || !g_xNoise.IsValid()) return true;

		// Move priest right next to the noise machine so the emitted
		// stimulus falls inside its 25m hearing radius regardless of
		// authored positions.
		Zenith_Maths::Vector3 xNoisePos;
		if (TryGetEntityPosition(g_xNoise, xNoisePos))
		{
			TrySetEntityPosition(g_xPriest, xNoisePos + Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		}

		// Now teleport possessed villager next to the noise machine to trigger
		// DPInteractable rising-edge → DummyNoiseMachine::HandleInteract →
		// DP_AI::EmitNoise. The villager is the SOURCE that
		// UpdateHearingPerception attributes the sound to (priest's own
		// stimulus is filtered).
		DP_Player::SetPossessedVillager(g_xVillager);
		if (TryGetEntityPosition(g_xNoise, xNoisePos))
		{
			TrySetEntityPosition(g_xVillager, xNoisePos + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		}
		g_iPhase = kEnterRange;
		return true;
	}

	case kEnterRange:
		// One frame for DPInteractable's in-range detection.
		g_iPhase = kPressF;
		return true;

	case kPressF:
		// F-press → noise machine's HandleInteract → DP_AI::EmitNoise → priest hears.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kPropagate;
		return true;

	case kPropagate:
		// Several frames for: 1. perception update creates target,
		// 2. priest::OnUpdate's bridge writes BB.HasInvestigatePos.
		++g_iWait;
		if (g_iWait >= 5) g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		const auto xHeard = Zenith_PerceptionSystem::GetLastHeardSoundFor(g_xPriest);
		g_bPriestHeard = xHeard.m_bValid;
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_NoiseMachineFlow()
{
	using namespace NoiseMachineFlowState;
	return g_xVillager.IsValid() && g_xPriest.IsValid()
	    && g_xNoise.IsValid() && g_bPriestHeard;
}

static const Zenith_AutomatedTest g_xNoiseMachineFlowTest = {
	"NoiseMachineFlow_Test",
	&Setup_NoiseMachineFlow,
	&Step_NoiseMachineFlow,
	&Verify_NoiseMachineFlow,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xNoiseMachineFlowTest);

// ============================================================================
// OrbitCameraFollowsPossession_Test
//
// 2026-07-01 camera third-person mode. Replaces OrbitCameraStaysFixed_Test,
// which pinned the pre-third-person contract ("the camera never follows the
// possessed villager") that the camera-mode feature deliberately retired.
// The surviving half of the old invariant — the bird's-eye camera has no
// scripted drift while nothing is possessed and no input arrives — is still
// asserted (phase kHold); the new half asserts possession BLENDS the camera
// down to a third-person pose near the villager.
// ============================================================================
namespace OrbitCameraState
{
	enum Phase : int { kStart, kWait, kHold, kPossess, kTick, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xCamera;
	Zenith_Maths::Vector3 g_xCamPosHoldStart(0.0f);
	Zenith_Maths::Vector3 g_xCamPosBefore(0.0f);
	Zenith_Maths::Vector3 g_xCamPosAfter(0.0f);
	float           g_fDistToVillagerAfter = -1.0f;
	int             g_iWait = 0;
}

static void Setup_OrbitCamera()
{
	using namespace OrbitCameraState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xCamera = INVALID_ENTITY_ID;
	g_xCamPosHoldStart = Zenith_Maths::Vector3(0.0f);
	g_xCamPosBefore = Zenith_Maths::Vector3(0.0f);
	g_xCamPosAfter  = Zenith_Maths::Vector3(0.0f);
	g_fDistToVillagerAfter = -1.0f;
	g_iWait = 0;
}

static bool Step_OrbitCamera(int /*iFrame*/)
{
	using namespace OrbitCameraState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		g_xCamera   = FindFirstEntityWith<DPOrbitCamera_Component>();
		if (!g_xVillager.IsValid() || !g_xCamera.IsValid()) return true;
		// Snapshot at the start of the unpossessed hold window.
		// DPOrbitCamera_Component writes to Zenith_CameraComponent::SetPosition
		// (the camera's logical view position), NOT the entity transform.
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xCamera);
		if (Zenith_CameraComponent* pxCamera = xEnt.TryGetComponent<Zenith_CameraComponent>())
		{
			pxCamera->GetPosition(g_xCamPosHoldStart);
		}
		g_iWait = 0;
		g_iPhase = kHold;
		return true;
	}

	case kHold:
		// 30 unpossessed frames with no input: the bird's-eye pose must not
		// drift at all (the blend target stays 0 with nothing possessed).
		++g_iWait;
		if (g_iWait >= 30) g_iPhase = kPossess;
		return true;

	case kPossess:
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xCamera);
		if (Zenith_CameraComponent* pxCamera = xEnt.TryGetComponent<Zenith_CameraComponent>())
		{
			pxCamera->GetPosition(g_xCamPosBefore);
		}
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iWait = 0;
		g_iPhase = kTick;
		return true;
	}

	case kTick:
		// 90 frames: comfortably past the ~0.4 s mode blend.
		++g_iWait;
		if (g_iWait >= 90) g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xCamera);
		if (Zenith_CameraComponent* pxCamera = xEnt.TryGetComponent<Zenith_CameraComponent>())
		{
			pxCamera->GetPosition(g_xCamPosAfter);
		}
		Zenith_Entity xVillagerEnt = g_xEngine.Scenes().ResolveEntity(g_xVillager);
		if (xVillagerEnt.IsValid())
		{
			if (Zenith_TransformComponent* pxT = xVillagerEnt.TryGetComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xVPos;
				pxT->GetPosition(xVPos);
				g_fDistToVillagerAfter = glm::length(g_xCamPosAfter - xVPos);
			}
		}
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_OrbitCamera()
{
	using namespace OrbitCameraState;
	if (!g_xVillager.IsValid() || !g_xCamera.IsValid()) return false;
	// 1) Unpossessed: bird's-eye (high above the map) and zero drift over
	//    the 30-frame hold — the regression guarantee for menu/gym scenes.
	if (g_xCamPosHoldStart.y < 50.0f) return false;
	if (glm::length(g_xCamPosBefore - g_xCamPosHoldStart) > 0.01f) return false;
	// 2) Possessed: the camera blends DOWN into a third-person pose a few
	//    metres from the villager (vs the ~80 m bird's-eye orbit radius).
	if (g_fDistToVillagerAfter < 0.0f || g_fDistToVillagerAfter > 8.0f) return false;
	if (g_xCamPosAfter.y >= g_xCamPosBefore.y) return false;
	return true;
}

static const Zenith_AutomatedTest g_xOrbitCameraTest = {
	"OrbitCameraFollowsPossession_Test",
	&Setup_OrbitCamera,
	&Step_OrbitCamera,
	&Verify_OrbitCamera,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xOrbitCameraTest);

// ============================================================================
// HUDLifeBar_Test
//
// HUD lifebar is a Zenith_UIText element named "LifeBar" living on the
// GameManager's UIComponent. DPHUDController_Component::OnUpdate makes it
// visible + sets the text to "Life: |...|" while a possessed villager exists.
// ============================================================================
namespace HUDLifeBarState
{
	enum Phase : int { kStart, kWait, kPossess, kTick, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xHUD;
	bool            g_bHadVisibleLifeBar = false;
	int             g_iWait = 0;
}

static void Setup_HUDLifeBar()
{
	using namespace HUDLifeBarState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xHUD = INVALID_ENTITY_ID;
	g_bHadVisibleLifeBar = false;
	g_iWait = 0;
}

static bool Step_HUDLifeBar(int /*iFrame*/)
{
	using namespace HUDLifeBarState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
	{
		g_xVillager = FindFirstEntityWith<DPVillager_Component>();
		g_xHUD      = FindFirstEntityWith<DPHUDController_Component>();
		if (!g_xVillager.IsValid() || !g_xHUD.IsValid()) return true;
		g_iPhase = kPossess;
		return true;
	}

	case kPossess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kTick;
		return true;

	case kTick:
		++g_iWait;
		if (g_iWait >= 3) g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xHUD);
		if (Zenith_UIComponent* pxUI = xEnt.TryGetComponent<Zenith_UIComponent>())
		{
			if (auto* pxBar = pxUI->FindElement<Zenith_UI::Zenith_UIText>("LifeBar"))
			{
				g_bHadVisibleLifeBar = pxBar->IsVisible() && !pxBar->GetText().empty();
			}
		}
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_HUDLifeBar()
{
	using namespace HUDLifeBarState;
	return g_xVillager.IsValid() && g_xHUD.IsValid() && g_bHadVisibleLifeBar;
}

static const Zenith_AutomatedTest g_xHUDLifeBarTest = {
	"HUDLifeBar_Test",
	&Setup_HUDLifeBar,
	&Step_HUDLifeBar,
	&Verify_HUDLifeBar,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHUDLifeBarTest);

#endif // ZENITH_INPUT_SIMULATOR
