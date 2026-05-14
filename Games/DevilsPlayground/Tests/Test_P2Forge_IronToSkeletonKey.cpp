#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Forge_IronToSkeletonKey (MVP-2.3.3)
//
// Sibling to Test_P2Forge_WoodToSpike. Pins the SkeletonKey output
// recipe: a forge configured Iron -> SkeletonKey transforms a held
// Iron into a held SkeletonKey.
//
// (The roadmap-listed name was Test_P2Forge_IronBrassKeySkeleton --
// shorthand for the GDD's "Iron + BrassKey -> SkeletonKey" recipe.
// MVP scope is per-forge single-input recipes (one item in, one
// item out); a two-input recipe needs a new state-machine in the
// forge to remember partial deliveries, which is post-MVP. The
// SkeletonKey output is the gameplay-relevant property -- this
// test pins it via a single-input Iron -> SkeletonKey recipe.)
//
// Procedure mirrors Test_P2Forge_WoodToSpike exactly except for the
// input/output tag pair, so the regression-catching surface
// includes all the things that test covers PLUS the SkeletonKey
// specific case.
// ============================================================================

namespace
{
	enum Phase : int { kFS_Start, kFS_WaitScene, kFS_BuildForge,
	                   kFS_BuildInput, kFS_Craft, kFS_Verify, kFS_Done };

	int                     g_iPhase = kFS_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xForge;
	Zenith_EntityID         g_xInput;
	DPForge_Behaviour*      g_pxForge = nullptr;

	Zenith_EntityID         g_xHeldAfter;
	DP_ItemTag              g_eHeldTagAfter = DP_ItemTag::None;
	uint32_t                g_uCraftCountAfter = 0;
}

static void Setup_P2ForgeSkeletonKey()
{
	g_iPhase = kFS_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xForge = INVALID_ENTITY_ID;
	g_xInput = INVALID_ENTITY_ID;
	g_pxForge = nullptr;
	g_xHeldAfter = INVALID_ENTITY_ID;
	g_eHeldTagAfter = DP_ItemTag::None;
	g_uCraftCountAfter = 0;
}

static bool Step_P2ForgeSkeletonKey(int iFrame)
{
	switch (g_iPhase)
	{
	case kFS_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFS_WaitScene;
		return true;

	case kFS_WaitScene:
		{
			int iCount = 0;
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&iCount](Zenith_EntityID, DPVillager_Behaviour&) { ++iCount; });
			if (iCount > 0)
			{
				g_iPhase = kFS_BuildForge;
			}
			else if (iFrame > 90)
			{
				g_iPhase = kFS_Done;
			}
		}
		return true;

	case kFS_BuildForge:
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFS_Done; return false; }
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!g_xVillager.IsValid()) g_xVillager = xId;
			});

		Zenith_Entity xForge(pxScene, std::string("Test_SkeletonKeyForge"));
		if (!xForge.IsValid()) { g_iPhase = kFS_Done; return false; }
		g_xForge = xForge.GetEntityID();
		if (xForge.HasComponent<Zenith_TransformComponent>())
		{
			xForge.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(-60.0f, 0.0f, -60.0f));
		}
		g_pxForge = xForge.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPForge_Behaviour>();
		if (g_pxForge != nullptr)
		{
			g_pxForge->SetRecipe(DP_ItemTag::Iron, DP_ItemTag::SkeletonKey);
		}
		g_iPhase = kFS_BuildInput;
		return true;
	}

	case kFS_BuildInput:
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFS_Done; return false; }
		Zenith_Entity xInput(pxScene, std::string("ForgeIntake_Iron"));
		if (!xInput.IsValid()) { g_iPhase = kFS_Done; return false; }
		g_xInput = xInput.GetEntityID();
		if (xInput.HasComponent<Zenith_TransformComponent>())
		{
			xInput.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(-65.0f, 0.0f, -60.0f));
		}
		xInput.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Behaviour* pxItemBase = xInput.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (pxItemBase != nullptr) pxItemBase->SetTag(DP_ItemTag::Iron);
		DP_Player::SetHeldItem(g_xVillager, g_xInput);
		g_iPhase = kFS_Craft;
		return true;
	}

	case kFS_Craft:
		if (g_pxForge != nullptr) g_pxForge->CraftForTest(g_xVillager);
		g_iPhase = kFS_Verify;
		return true;

	case kFS_Verify:
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
		g_eHeldTagAfter = DP_Player::GetHeldItemTag(g_xVillager);
		if (g_pxForge != nullptr) g_uCraftCountAfter = g_pxForge->GetCraftCount();
		std::printf("[P2ForgeSkeletonKey] heldEntity=(%u/%u) inputEntity=(%u/%u) heldTag=%s craftCount=%u\n",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xInput.m_uIndex, g_xInput.m_uGeneration,
			DP_ItemTagToString(g_eHeldTagAfter),
			g_uCraftCountAfter);
		std::fflush(stdout);
		g_iPhase = kFS_Done;
		return false;

	case kFS_Done:
	default:
		return false;
	}
}

static bool Verify_P2ForgeSkeletonKey()
{
	if (!g_xVillager.IsValid() || !g_xForge.IsValid() || !g_xInput.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ForgeSkeletonKey: setup entities missing");
		return false;
	}
	if (g_uCraftCountAfter != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeSkeletonKey: craft count is %u, expected 1", g_uCraftCountAfter);
		return false;
	}
	if (g_eHeldTagAfter != DP_ItemTag::SkeletonKey)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeSkeletonKey: held tag after craft is %s, expected SkeletonKey. SetRecipe didn't take effect, or the output entity was spawned with the default Key tag (i.e., regular brass key, not the master key)",
			DP_ItemTagToString(g_eHeldTagAfter));
		return false;
	}
	if (g_xHeldAfter.m_uIndex == g_xInput.m_uIndex
		&& g_xHeldAfter.m_uGeneration == g_xInput.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeSkeletonKey: held entity AFTER craft is the same as the Iron INPUT entity. The forge didn't consume the input and spawn a new output");
		return false;
	}
	if (!g_xHeldAfter.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeSkeletonKey: held entity is INVALID after craft");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ForgeSkeletonKeyTest = {
	"Test_P2Forge_IronToSkeletonKey",
	&Setup_P2ForgeSkeletonKey,
	&Step_P2ForgeSkeletonKey,
	&Verify_P2ForgeSkeletonKey,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ForgeSkeletonKeyTest);

#endif // ZENITH_INPUT_SIMULATOR
