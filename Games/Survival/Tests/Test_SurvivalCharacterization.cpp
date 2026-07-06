#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_SurvivalCharacterization - characterization tests for the wave-2 graph
 * conversion of Survival's decision logic (harvest / craft / respawn seam /
 * game-flow menu-play/escape/reset).
 *
 * Written against the C++ versions FIRST (Survival_GameComponent::HandleInteraction
 * / HandleCrafting / UpdateCrafting, the OnResourceHarvested / OnResourceRespawned
 * / OnCraftingComplete event handlers, and the OnUpdate menu/escape/reset switch);
 * the behaviour-graph versions must keep every one of these green UNCHANGED. All
 * probes go through surfaces that survive the conversion: the live
 * Survival_GameComponent's read-only observers (game state, inventory counts,
 * crafting state, resource-node state) and the real input path
 * (Zenith_InputSimulator -> the component's WasKeyPressedThisFrame reads).
 *
 *   Survival_Harvest_Test      - place the player on resource node 0 (a tree),
 *                                press E, assert the tree yields exactly its
 *                                harvest (+1 wood, -1 hit) via the event handler.
 *   Survival_Craft_Test        - seed 3 wood + 2 stone, press 1, tick past the
 *                                craft time; assert an axe is crafted and the
 *                                materials were consumed (StartCrafting ->
 *                                Update -> CraftingComplete -> CollectCraftedItem).
 *   Survival_RespawnSeam_Test  - deplete node 0 with a short respawn timer; the
 *                                background parallel task respawns it and queues
 *                                ResourceRespawned, drained on the main thread;
 *                                assert the node becomes available again (the
 *                                plan's headline background-task -> deferred-event
 *                                seam, end to end).
 *   Survival_MenuPlay_Test     - from the MAIN_MENU scene, clicking Play loads
 *                                the gameplay scene and starts the world.
 *   Survival_ReturnToMenu_Test - Escape during play returns to MAIN_MENU (world
 *                                scene unloaded, menu scene reloaded).
 *   Survival_Reset_Test        - R during play resets inventory to empty and all
 *                                resource nodes to full health.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Components/Survival_GameComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"

namespace
{
	Survival_GameComponent* LiveGame()
	{
		return Survival_GameComponent::Test_GetLiveInstance();
	}

	bool IsPlaying()
	{
		Survival_GameComponent* pxGame = LiveGame();
		return pxGame != nullptr
			&& Survival_GameComponent::Test_GetLiveGameState() == SurvivalGameState::PLAYING
			&& pxGame->Test_GetResourceNodeCount() > 0;
	}

	bool IsMenu()
	{
		return LiveGame() != nullptr
			&& Survival_GameComponent::Test_GetLiveGameState() == SurvivalGameState::MAIN_MENU;
	}

	Zenith_UI::Zenith_UIButton* FindMenuPlayButton()
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			});
		return pxFound;
	}
}

// ============================================================================
// Survival_Harvest_Test
// ============================================================================

namespace
{
	enum class HarvestPhase { Boot, WaitPlaying, Place, Settle, PressE, AwaitHarvest, Done };

	HarvestPhase g_eHarvest = HarvestPhase::Boot;
	int          g_iHarvestFrame = 0;
	int32_t      g_iHarvestYieldType = 0;
	uint32_t     g_uHarvestItemBefore = 0;
	uint32_t     g_uHarvestHitsBefore = 0;
	uint32_t     g_uHarvestItemAfter = 0;
	uint32_t     g_uHarvestHitsAfter = 0;
	bool         g_bHarvestDone = false;
}

static void Setup_Harvest()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eHarvest = HarvestPhase::Boot;
	g_iHarvestFrame = 0;
	g_iHarvestYieldType = 0;
	g_uHarvestItemBefore = 0;
	g_uHarvestHitsBefore = 0;
	g_uHarvestItemAfter = 0;
	g_uHarvestHitsAfter = 0;
	g_bHarvestDone = false;
}

static bool Step_Harvest(int iFrame)
{
	switch (g_eHarvest)
	{
	case HarvestPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eHarvest = HarvestPhase::WaitPlaying;
		return true;

	case HarvestPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eHarvest = HarvestPhase::Place;
			return true;
		}
		return iFrame < 600;

	case HarvestPhase::Place:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		Zenith_Maths::Vector3 xNodePos;
		if (!pxGame->Test_GetResourceNodePosition(0, xNodePos)) return false;
		// Stand the player on resource node 0 so it is the nearest in range.
		pxGame->Test_SetPlayerPosition(xNodePos);
		g_iHarvestYieldType = pxGame->Test_GetResourceNodeYieldType(0);
		g_uHarvestItemBefore = pxGame->Test_GetItemCount(static_cast<SurvivalItemType>(g_iHarvestYieldType));
		g_uHarvestHitsBefore = pxGame->Test_GetResourceNodeHits(0);
		g_iHarvestFrame = 0;
		g_eHarvest = HarvestPhase::Settle;
		return true;
	}

	case HarvestPhase::Settle:
		// Let HandleMovement clamp the player Y (no input -> stays on the node).
		if (++g_iHarvestFrame >= 3)
		{
			g_eHarvest = HarvestPhase::PressE;
		}
		return true;

	case HarvestPhase::PressE:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
		g_iHarvestFrame = 0;
		g_eHarvest = HarvestPhase::AwaitHarvest;
		return true;

	case HarvestPhase::AwaitHarvest:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		const uint32_t uItemNow = pxGame->Test_GetItemCount(static_cast<SurvivalItemType>(g_iHarvestYieldType));
		if (uItemNow > g_uHarvestItemBefore)
		{
			g_uHarvestItemAfter = uItemNow;
			g_uHarvestHitsAfter = pxGame->Test_GetResourceNodeHits(0);
			g_bHarvestDone = true;
			g_eHarvest = HarvestPhase::Done;
			return false;
		}
		return ++g_iHarvestFrame < 120;
	}

	case HarvestPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Harvest()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bHarvestDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Harvest] never harvested (phase %d)", static_cast<int>(g_eHarvest));
		return false;
	}
	// Tree node 0: yield = floor(3 * 1.0 / 3) clamped >=1 = exactly 1; one hit spent.
	if (g_uHarvestItemAfter != g_uHarvestItemBefore + 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Harvest] expected +1 yield, before=%u after=%u",
			g_uHarvestItemBefore, g_uHarvestItemAfter);
		return false;
	}
	if (g_uHarvestHitsAfter != g_uHarvestHitsBefore - 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Harvest] expected -1 hit, before=%u after=%u",
			g_uHarvestHitsBefore, g_uHarvestHitsAfter);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xHarvestTest = {
	"Survival_Harvest_Test",
	&Setup_Harvest,
	&Step_Harvest,
	&Verify_Harvest,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHarvestTest);

// ============================================================================
// Survival_Craft_Test
// ============================================================================

namespace
{
	enum class CraftPhase { Boot, WaitPlaying, Seed, PressCraft, AwaitCraft, Done };

	CraftPhase g_eCraft = CraftPhase::Boot;
	int        g_iCraftFrame = 0;
	uint32_t   g_uCraftAxeAfter = 0;
	uint32_t   g_uCraftWoodAfter = 99;
	uint32_t   g_uCraftStoneAfter = 99;
	bool       g_bCraftDone = false;
}

static void Setup_Craft()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eCraft = CraftPhase::Boot;
	g_iCraftFrame = 0;
	g_uCraftAxeAfter = 0;
	g_uCraftWoodAfter = 99;
	g_uCraftStoneAfter = 99;
	g_bCraftDone = false;
}

static bool Step_Craft(int iFrame)
{
	switch (g_eCraft)
	{
	case CraftPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eCraft = CraftPhase::WaitPlaying;
		return true;

	case CraftPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eCraft = CraftPhase::Seed;
			return true;
		}
		return iFrame < 600;

	case CraftPhase::Seed:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		// Exactly the axe recipe: 3 wood + 2 stone.
		pxGame->Test_GiveItem(ITEM_TYPE_WOOD, 3);
		pxGame->Test_GiveItem(ITEM_TYPE_STONE, 2);
		g_eCraft = CraftPhase::PressCraft;
		return true;
	}

	case CraftPhase::PressCraft:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_1);
		g_iCraftFrame = 0;
		g_eCraft = CraftPhase::AwaitCraft;
		return true;

	case CraftPhase::AwaitCraft:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		if (pxGame->Test_GetItemCount(ITEM_TYPE_AXE) >= 1)
		{
			g_uCraftAxeAfter = pxGame->Test_GetItemCount(ITEM_TYPE_AXE);
			g_uCraftWoodAfter = pxGame->Test_GetItemCount(ITEM_TYPE_WOOD);
			g_uCraftStoneAfter = pxGame->Test_GetItemCount(ITEM_TYPE_STONE);
			g_bCraftDone = true;
			g_eCraft = CraftPhase::Done;
			return false;
		}
		// Craft time is 2 s (120 frames) + a frame for collection; allow slack.
		return ++g_iCraftFrame < 300;
	}

	case CraftPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Craft()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bCraftDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Craft] never crafted an axe (phase %d)", static_cast<int>(g_eCraft));
		return false;
	}
	if (g_uCraftAxeAfter != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Craft] expected 1 axe, saw %u", g_uCraftAxeAfter);
		return false;
	}
	// StartCrafting consumed the 3 wood + 2 stone we seeded.
	if (g_uCraftWoodAfter != 0 || g_uCraftStoneAfter != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Craft] materials not consumed (wood=%u stone=%u)",
			g_uCraftWoodAfter, g_uCraftStoneAfter);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xCraftTest = {
	"Survival_Craft_Test",
	&Setup_Craft,
	&Step_Craft,
	&Verify_Craft,
	/*maxFrames*/ 1000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCraftTest);

// ============================================================================
// Survival_RespawnSeam_Test
// ============================================================================

namespace
{
	enum class RespawnPhase { Boot, WaitPlaying, Arm, AwaitRespawn, Done };

	RespawnPhase g_eRespawn = RespawnPhase::Boot;
	int          g_iRespawnFrame = 0;
	bool         g_bRespawnSawDepleted = false;
	bool         g_bRespawnAvailable = false;
	uint32_t     g_uRespawnHits = 0;
	bool         g_bRespawnDone = false;
}

static void Setup_Respawn()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRespawn = RespawnPhase::Boot;
	g_iRespawnFrame = 0;
	g_bRespawnSawDepleted = false;
	g_bRespawnAvailable = false;
	g_uRespawnHits = 0;
	g_bRespawnDone = false;
}

static bool Step_Respawn(int iFrame)
{
	switch (g_eRespawn)
	{
	case RespawnPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRespawn = RespawnPhase::WaitPlaying;
		return true;

	case RespawnPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eRespawn = RespawnPhase::Arm;
			return true;
		}
		return iFrame < 600;

	case RespawnPhase::Arm:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		// Node 0 is the one the background parallel task processes (it always
		// updates GetNode(0)); arm it depleted with a short respawn timer.
		pxGame->Test_ArmRespawn(0, 0.1f);
		g_bRespawnSawDepleted = pxGame->Test_IsResourceNodeDepleted(0);
		g_iRespawnFrame = 0;
		g_eRespawn = RespawnPhase::AwaitRespawn;
		return true;
	}

	case RespawnPhase::AwaitRespawn:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		if (!pxGame->Test_IsResourceNodeDepleted(0))
		{
			g_bRespawnAvailable = true;
			g_uRespawnHits = pxGame->Test_GetResourceNodeHits(0);
			g_bRespawnDone = true;
			g_eRespawn = RespawnPhase::Done;
			return false;
		}
		// 0.1 s timer at 60 Hz -> respawned within ~6 frames; allow slack for
		// the deferred-event drain on the following main-thread tick.
		return ++g_iRespawnFrame < 120;
	}

	case RespawnPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Respawn()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRespawnSawDepleted)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Respawn] node 0 was not depleted after arming");
		return false;
	}
	if (!g_bRespawnDone || !g_bRespawnAvailable)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Respawn] node 0 never respawned (phase %d)", static_cast<int>(g_eRespawn));
		return false;
	}
	if (g_uRespawnHits == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Respawn] respawned node has 0 hits (expected refill to max)");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xRespawnTest = {
	"Survival_RespawnSeam_Test",
	&Setup_Respawn,
	&Step_Respawn,
	&Verify_Respawn,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRespawnTest);

// ============================================================================
// Survival_MenuPlay_Test
// ============================================================================

namespace
{
	enum class MPlayPhase { Boot, WaitMenu, ClickPlay, AwaitPlaying, Done };

	MPlayPhase g_eMPlay = MPlayPhase::Boot;
	int        g_iMPlayFrame = 0;
	bool       g_bMPlayStarted = false;
	bool       g_bMPlayDone = false;
}

static void Setup_MenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eMPlay = MPlayPhase::Boot;
	g_iMPlayFrame = 0;
	g_bMPlayStarted = false;
	g_bMPlayDone = false;
}

static bool Step_MenuPlay(int iFrame)
{
	switch (g_eMPlay)
	{
	case MPlayPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eMPlay = MPlayPhase::WaitMenu;
		return true;

	case MPlayPhase::WaitMenu:
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_eMPlay = MPlayPhase::ClickPlay;
			return true;
		}
		return iFrame < 600;

	case MPlayPhase::ClickPlay:
	{
		Zenith_UI::Zenith_UIButton* pxPlay = FindMenuPlayButton();
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_iMPlayFrame = 0;
		g_eMPlay = MPlayPhase::AwaitPlaying;
		return true;
	}

	case MPlayPhase::AwaitPlaying:
		if (IsPlaying())
		{
			g_bMPlayStarted = true;
			g_bMPlayDone = true;
			g_eMPlay = MPlayPhase::Done;
			return false;
		}
		if (++g_iMPlayFrame > 600) { g_bMPlayDone = true; g_eMPlay = MPlayPhase::Done; return false; }
		return iFrame < 1200;

	case MPlayPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bMPlayDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] never completed (phase %d)", static_cast<int>(g_eMPlay));
		return false;
	}
	if (!g_bMPlayStarted)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] clicking Play did not start the world");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMenuPlayTest = {
	"Survival_MenuPlay_Test",
	&Setup_MenuPlay,
	&Step_MenuPlay,
	&Verify_MenuPlay,
	/*maxFrames*/ 1400,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMenuPlayTest);

// ============================================================================
// Survival_ReturnToMenu_Test
// ============================================================================

namespace
{
	enum class RetMenuPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };

	RetMenuPhase g_eRetMenu = RetMenuPhase::Boot;
	int          g_iRetMenuFrame = 0;
	bool         g_bRetSawMenu = false;
	bool         g_bRetDone = false;
}

static void Setup_ReturnToMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRetMenu = RetMenuPhase::Boot;
	g_iRetMenuFrame = 0;
	g_bRetSawMenu = false;
	g_bRetDone = false;
}

static bool Step_ReturnToMenu(int iFrame)
{
	switch (g_eRetMenu)
	{
	case RetMenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRetMenu = RetMenuPhase::WaitPlaying;
		return true;

	case RetMenuPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eRetMenu = RetMenuPhase::PressEsc;
			return true;
		}
		return iFrame < 600;

	case RetMenuPhase::PressEsc:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iRetMenuFrame = 0;
		g_eRetMenu = RetMenuPhase::AwaitMenu;
		return true;

	case RetMenuPhase::AwaitMenu:
		// Escape unloads the world scene and single-loads the menu scene, whose
		// MenuManager wakes in MAIN_MENU (the new live instance).
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_bRetSawMenu = true;
			g_bRetDone = true;
			g_eRetMenu = RetMenuPhase::Done;
			return false;
		}
		if (++g_iRetMenuFrame > 120) { g_bRetDone = true; g_eRetMenu = RetMenuPhase::Done; return false; }
		return iFrame < 800;

	case RetMenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ReturnToMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bRetDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] never completed (phase %d)", static_cast<int>(g_eRetMenu));
		return false;
	}
	if (!g_bRetSawMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] Escape did not return to MAIN_MENU (state %d)",
			static_cast<int>(Survival_GameComponent::Test_GetLiveGameState()));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xReturnToMenuTest = {
	"Survival_ReturnToMenu_Test",
	&Setup_ReturnToMenu,
	&Step_ReturnToMenu,
	&Verify_ReturnToMenu,
	/*maxFrames*/ 1000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xReturnToMenuTest);

// ============================================================================
// Survival_Reset_Test
// ============================================================================

namespace
{
	enum class ResetPhase { Boot, WaitPlaying, Dirty, PressR, AwaitReset, Done };

	ResetPhase g_eReset = ResetPhase::Boot;
	int        g_iResetFrame = 0;
	bool       g_bResetInvCleared = false;
	bool       g_bResetNodesFull = false;
	bool       g_bResetDone = false;
}

static void Setup_Reset()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eReset = ResetPhase::Boot;
	g_iResetFrame = 0;
	g_bResetInvCleared = false;
	g_bResetNodesFull = false;
	g_bResetDone = false;
}

static bool Step_Reset(int iFrame)
{
	switch (g_eReset)
	{
	case ResetPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eReset = ResetPhase::WaitPlaying;
		return true;

	case ResetPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eReset = ResetPhase::Dirty;
			return true;
		}
		return iFrame < 600;

	case ResetPhase::Dirty:
	{
		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		// Make the reset observable: give items and deplete a node.
		pxGame->Test_GiveItem(ITEM_TYPE_WOOD, 5);
		pxGame->Test_GiveItem(ITEM_TYPE_STONE, 4);
		pxGame->Test_ArmRespawn(0, 30.0f);
		g_eReset = ResetPhase::PressR;
		return true;
	}

	case ResetPhase::PressR:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_iResetFrame = 0;
		g_eReset = ResetPhase::AwaitReset;
		return true;

	case ResetPhase::AwaitReset:
	{
		if (++g_iResetFrame < 10)
			return true;

		Survival_GameComponent* pxGame = LiveGame();
		if (pxGame == nullptr) return false;
		g_bResetInvCleared =
			pxGame->Test_GetItemCount(ITEM_TYPE_WOOD) == 0 &&
			pxGame->Test_GetItemCount(ITEM_TYPE_STONE) == 0;
		// ResetGame restores every node to full (not depleted).
		g_bResetNodesFull =
			!pxGame->Test_IsResourceNodeDepleted(0) &&
			pxGame->Test_GetDepletedResourceCount() == 0;
		g_bResetDone = true;
		g_eReset = ResetPhase::Done;
		return false;
	}

	case ResetPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Reset()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bResetDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Reset] never completed (phase %d)", static_cast<int>(g_eReset));
		return false;
	}
	if (!g_bResetInvCleared)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Reset] R did not clear the inventory");
		return false;
	}
	if (!g_bResetNodesFull)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Reset] R did not restore all resource nodes to full");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xResetTest = {
	"Survival_Reset_Test",
	&Setup_Reset,
	&Step_Reset,
	&Verify_Reset,
	/*maxFrames*/ 900,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xResetTest);

#endif // ZENITH_INPUT_SIMULATOR
