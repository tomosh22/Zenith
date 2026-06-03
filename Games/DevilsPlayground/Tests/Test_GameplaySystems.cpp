#include "Zenith.h"

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
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"

// ============================================================================
// Six tests covering gameplay systems not yet directly tested:
//
//   DoorUnlock_Test       — Held key + proximity → door opens, key consumed
//   VillagerDeath_Test    — Possessed villager life timer expires → DP_OnVillagerDied + possession cleared
//   ChestInteract_Test    — F-press near chest → m_bIsOpen flips
//   NoiseMachineFlow_Test — F-press near noise machine → priest hears (BB.HasInvestigatePos true)
//   OrbitCameraStaysFixed_Test — Possess villager → bird's-eye camera position unchanged
//   HUDLifeBar_Test       — Possess villager → HUD's "LifeBar" UI text becomes visible + non-empty
//
// Each follows the multi-frame state-machine pattern from Test_Possession.cpp.
// All use the runtime-spawned items + scene from DPItemManager_Behaviour.
// ============================================================================

namespace
{
	template<typename T>
	Zenith_EntityID FindFirstEntityWith()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&xResult](Zenith_EntityID xId, T&) { if (!xResult.IsValid()) xResult = xId; });
		return xResult;
	}

	template<typename T>
	T* GetScript(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<T>();
	}

	bool TryGetEntityPosition(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	void TrySetEntityPosition(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
		// 2026-05-25: most doors are now unlocked-by-default; "first door
		// found" would silently pass the consume-key assertion against an
		// unlocked door (the door opens without needing a key, the test
		// just sees the open transition). Filter for a door that
		// actually requires a Key so the unlock path is the only way to
		// open it.
		g_xDoor = INVALID_ENTITY_ID;
		DP_Query::ForEachScriptInActiveScene<DPDoor_Behaviour>(
			[](Zenith_EntityID xId, DPDoor_Behaviour& xScript)
			{
				if (g_xDoor.IsValid()) return;
				if (xScript.GetRequiredKey() == DP_ItemTag::Key)
				{
					g_xDoor = xId;
				}
			});
		if (!g_xVillager.IsValid() || !g_xDoor.IsValid()) return true;

		DPDoor_Behaviour* pxDoor = GetScript<DPDoor_Behaviour>(g_xDoor);
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
		DPDoor_Behaviour* pxDoor = GetScript<DPDoor_Behaviour>(g_xDoor);
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
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
		DPVillager_Behaviour* pxV = GetScript<DPVillager_Behaviour>(g_xVillager);
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
		g_xChest    = FindFirstEntityWith<DPChest_Behaviour>();
		if (!g_xVillager.IsValid() || !g_xChest.IsValid()) return true;

		DPChest_Behaviour* pxChest = GetScript<DPChest_Behaviour>(g_xChest);
		if (pxChest == nullptr) return true;
		g_bChestWasClosed = !pxChest->IsOpen();

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
		DPChest_Behaviour* pxChest = GetScript<DPChest_Behaviour>(g_xChest);
		if (pxChest != nullptr) g_bChestIsOpen = pxChest->IsOpen();
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
		g_xPriest   = FindFirstEntityWith<Priest_Behaviour>();
		g_xNoise    = FindFirstEntityWith<DummyNoiseMachine_Behaviour>();
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
// OrbitCameraStaysFixed_Test
//
// DPOrbitCamera_Behaviour is a fixed bird's-eye camera pinned to the map
// centre — it does NOT chase the possessed villager. This test verifies
// the invariant that, between possess + 30 frames of villager motion, the
// camera barely moves (small scripted yaw drift on Q/E only, but no input
// is simulated here so the position should be effectively unchanged).
// ============================================================================
namespace OrbitCameraState
{
	enum Phase : int { kStart, kWait, kPossess, kTick, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xCamera;
	Zenith_Maths::Vector3 g_xCamPosBefore(0.0f);
	Zenith_Maths::Vector3 g_xCamPosAfter(0.0f);
	int             g_iWait = 0;
}

static void Setup_OrbitCamera()
{
	using namespace OrbitCameraState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xCamera = INVALID_ENTITY_ID;
	g_xCamPosBefore = Zenith_Maths::Vector3(0.0f);
	g_xCamPosAfter  = Zenith_Maths::Vector3(0.0f);
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
		g_xCamera   = FindFirstEntityWith<DPOrbitCamera_Behaviour>();
		if (!g_xVillager.IsValid() || !g_xCamera.IsValid()) return true;
		g_iPhase = kPossess;
		return true;
	}

	case kPossess:
	{
		// Snapshot camera position BEFORE possession.
		// DPOrbitCamera_Behaviour writes to Zenith_CameraComponent::SetPosition
		// (the camera's logical view position), NOT the entity transform.
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xCamera);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xCamera);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_CameraComponent>())
			{
				xEnt.GetComponent<Zenith_CameraComponent>().GetPosition(g_xCamPosBefore);
			}
		}
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kTick;
		return true;
	}

	case kTick:
		// 30 frames of doing nothing. A close-following third-person
		// camera would drift toward the villager during this window.
		++g_iWait;
		if (g_iWait >= 30) g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xCamera);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xCamera);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_CameraComponent>())
			{
				xEnt.GetComponent<Zenith_CameraComponent>().GetPosition(g_xCamPosAfter);
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
	// 1) Camera must be high above the map (bird's-eye, not third-person).
	if (g_xCamPosBefore.y < 50.0f) return false;
	if (g_xCamPosAfter.y  < 50.0f) return false;
	// 2) Camera position must be effectively unchanged across the
	//    possession + 30-frame window. No player input is simulated, so
	//    only frame-to-frame scripted drift could shift it — and the
	//    bird's-eye camera has no scripted drift when no Q/E/wheel input
	//    arrives.
	const Zenith_Maths::Vector3 xDelta = g_xCamPosAfter - g_xCamPosBefore;
	if (glm::length(xDelta) > 1.0f) return false;
	return true;
}

static const Zenith_AutomatedTest g_xOrbitCameraTest = {
	"OrbitCameraStaysFixed_Test",
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
// GameManager's UIComponent. DPHUDController_Behaviour::OnUpdate makes it
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
		g_xVillager = FindFirstEntityWith<DPVillager_Behaviour>();
		g_xHUD      = FindFirstEntityWith<DPHUDController_Behaviour>();
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
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xHUD);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xHUD);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_UIComponent>())
			{
				Zenith_UIComponent& xUI = xEnt.GetComponent<Zenith_UIComponent>();
				if (auto* pxBar = xUI.FindElement<Zenith_UI::Zenith_UIText>("LifeBar"))
				{
					g_bHadVisibleLifeBar = pxBar->IsVisible() && !pxBar->GetText().empty();
				}
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
