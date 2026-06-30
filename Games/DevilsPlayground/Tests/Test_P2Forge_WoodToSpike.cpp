#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPForge_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPVillager_Component.h"

#include <cstdio>

// ============================================================================
// Test_P2Forge_WoodToSpike (MVP-2.3.2)
//
// Pins the forge's per-instance recipe configuration: a forge
// configured with SetRecipe(Wood, Spike) transforms a held Wood
// item into a held Spike. Confirms that DPForge_Component supports
// recipes beyond its default Iron -> Key.
//
// Construction parallels the existing Forge_Test (Test_DoubleDoor-
// AndForge.cpp): build a test villager + a forge entity in the
// active scene, hand the villager a Wood input, call CraftForTest
// (which bypasses the proximity/F-press flow), assert the villager
// now holds a Spike output.
//
// Procedure:
//   1. Load GameLevel (any scene with a SceneData -- GameLevel works).
//   2. Construct a "TestWoodForge" entity with a DPForge script.
//      Call SetRecipe(Wood, Spike).
//   3. Construct a "ForgeIntake_Wood" entity with a DPItemBase script
//      and tag Wood. Hand it to a test villager via SetHeldItem.
//   4. Call forge->CraftForTest(villager).
//   5. Assert:
//        GetHeldItemTag(villager) == Spike
//        GetHeldItemEntity(villager) is a fresh entity (not the
//          original Wood -- it was consumed).
//        GetCraftCount() == 1
//
// What this catches:
//   * SetRecipe doesn't actually mutate the forge's input/output
//     fields (e.g., setter was stubbed but never wired).
//   * HandleInteract still hardcodes the Iron check rather than
//     reading m_eRecipeInputTag.
//   * The output entity is spawned with the wrong tag (e.g., still
//     Key from the default).
//   * SpawnOutputItem mutates the forge state in a way that breaks
//     CraftCount.
// ============================================================================

namespace
{
	enum Phase : int { kFW_Start, kFW_WaitScene, kFW_BuildForge,
	                   kFW_BuildInput, kFW_Craft, kFW_Verify, kFW_Done };

	int                     g_iPhase = kFW_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xForge;
	Zenith_EntityID         g_xInput;
	DPForge_Component*      g_pxForge = nullptr;

	Zenith_EntityID         g_xHeldAfter;
	DP_ItemTag              g_eHeldTagAfter = DP_ItemTag::None;
	uint32_t                g_uCraftCountAfter = 0;
}

static void Setup_P2ForgeWoodSpike()
{
	g_iPhase = kFW_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xForge = INVALID_ENTITY_ID;
	g_xInput = INVALID_ENTITY_ID;
	g_pxForge = nullptr;
	g_xHeldAfter = INVALID_ENTITY_ID;
	g_eHeldTagAfter = DP_ItemTag::None;
	g_uCraftCountAfter = 0;
}

static bool Step_P2ForgeWoodSpike(int iFrame)
{
	switch (g_iPhase)
	{
	case kFW_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFW_WaitScene;
		return true;

	case kFW_WaitScene:
		// Wait until the active scene has at least one villager (proxy
		// for "scene OnAwoken").
		{
			int iCount = 0;
			DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
				[&iCount](Zenith_EntityID, DPVillager_Component&) { ++iCount; });
			if (iCount > 0)
			{
				g_iPhase = kFW_BuildForge;
			}
			else if (iFrame > 90)
			{
				g_iPhase = kFW_Done;
			}
		}
		return true;

	case kFW_BuildForge:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr)
		{
			g_iPhase = kFW_Done;
			return false;
		}
		// Pick the first villager already in the scene as our "test
		// villager" -- we don't construct one because that opens a
		// can of OnAwake / collider sequencing worms. The villager's
		// own Behaviour doesn't matter for this test -- we just need
		// an EntityID to pass to SetHeldItem.
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!g_xVillager.IsValid()) g_xVillager = xId;
			});

		Zenith_Entity xForge = g_xEngine.Scenes().CreateEntity(pxScene, std::string("Test_WoodForge"));
		if (!xForge.IsValid()) { g_iPhase = kFW_Done; return false; }
		g_xForge = xForge.GetEntityID();
		if (Zenith_TransformComponent* pxTransform = xForge.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(
				Zenith_Maths::Vector3(-50.0f, 0.0f, -50.0f));
		}
		g_pxForge = &xForge.AddComponent<DPForge_Component>();
		if (g_pxForge != nullptr)
		{
			g_pxForge->SetRecipe(DP_ItemTag::Wood, DP_ItemTag::Spike);
		}
		g_iPhase = kFW_BuildInput;
		return true;
	}

	case kFW_BuildInput:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr)
		{
			g_iPhase = kFW_Done;
			return false;
		}
		Zenith_Entity xInput = g_xEngine.Scenes().CreateEntity(pxScene, std::string("ForgeIntake_Wood"));
		if (!xInput.IsValid()) { g_iPhase = kFW_Done; return false; }
		g_xInput = xInput.GetEntityID();
		if (Zenith_TransformComponent* pxTransform = xInput.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(
				Zenith_Maths::Vector3(-55.0f, 0.0f, -50.0f));
		}
		xInput.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Component* pxItemBase = &xInput.AddComponent<DPItemBase_Component>();
		if (pxItemBase != nullptr) pxItemBase->SetTag(DP_ItemTag::Wood);
		// Hand the input directly to the villager via the side-table.
		DP_Player::SetHeldItem(g_xVillager, g_xInput);
		g_iPhase = kFW_Craft;
		return true;
	}

	case kFW_Craft:
		if (g_pxForge != nullptr) g_pxForge->CraftForTest(g_xVillager);
		g_iPhase = kFW_Verify;
		return true;

	case kFW_Verify:
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
		g_eHeldTagAfter = DP_Player::GetHeldItemTag(g_xVillager);
		if (g_pxForge != nullptr) g_uCraftCountAfter = g_pxForge->GetCraftCount();
		std::printf("[P2ForgeWoodSpike] heldEntity=(%u/%u) inputEntity=(%u/%u) heldTag=%s craftCount=%u\n",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xInput.m_uIndex, g_xInput.m_uGeneration,
			DP_ItemTagToString(g_eHeldTagAfter),
			g_uCraftCountAfter);
		std::fflush(stdout);
		g_iPhase = kFW_Done;
		return false;

	case kFW_Done:
	default:
		return false;
	}
}

static bool Verify_P2ForgeWoodSpike()
{
	if (!g_xVillager.IsValid() || !g_xForge.IsValid() || !g_xInput.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ForgeWoodSpike: setup entities missing");
		return false;
	}
	if (g_uCraftCountAfter != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeWoodSpike: craft count is %u, expected 1. Either CraftForTest didn't fire (recipe mismatch refused) or counting is broken",
			g_uCraftCountAfter);
		return false;
	}
	if (g_eHeldTagAfter != DP_ItemTag::Spike)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeWoodSpike: held tag after craft is %s, expected Spike. SetRecipe didn't take effect, or the output entity was spawned with the default Key tag",
			DP_ItemTagToString(g_eHeldTagAfter));
		return false;
	}
	if (g_xHeldAfter.m_uIndex == g_xInput.m_uIndex
		&& g_xHeldAfter.m_uGeneration == g_xInput.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeWoodSpike: held entity AFTER craft is the same as the Wood INPUT entity. The forge didn't consume the input and spawn a new output");
		return false;
	}
	if (!g_xHeldAfter.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeWoodSpike: held entity is INVALID after craft -- output entity was never SetHeldItem'd onto the villager");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ForgeWoodSpikeTest = {
	"Test_P2Forge_WoodToSpike",
	&Setup_P2ForgeWoodSpike,
	&Step_P2ForgeWoodSpike,
	&Verify_P2ForgeWoodSpike,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ForgeWoodSpikeTest);

#endif // ZENITH_INPUT_SIMULATOR
