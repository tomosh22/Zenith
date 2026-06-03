#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_AudioBus.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DP_Tuning.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P2Forge_AudibleAt30m (MVP-2.3.4)
//
// Pins the forge's audible-radius contract from Tuning.json:
//   interactables.forge_audible_at_m   = 30.0
//   interactables.forge_audible_loudness = 1.0
// A successful forge craft emits a "DP.Forge.Hammer" sound at the
// forge's world position with that radius + loudness. The priest's
// hearing perception (30 m default range) can then pick it up if
// he's anywhere in the village.
//
// Procedure:
//   1. Load GameLevel; pick any villager.
//   2. Build a test forge (Iron -> Key default recipe).
//   3. Build an Iron item, hand it to the villager.
//   4. Zenith_AudioBus::ClearEmittedSoundsForTest() to clear any
//      pre-existing emissions.
//   5. Call forge->CraftForTest(villager) -- direct invocation,
//      mirrors the other forge tests.
//   6. Read Zenith_AudioBus::GetEmittedSoundsForTest().
//   7. Assert exactly one emission with:
//        name contains "Forge"
//        loudness == forge_audible_loudness (1.0)
//        radius   >= forge_audible_at_m (30.0)
//        position close to the forge's authored position
//
// What this catches:
//   * Forge craft path doesn't emit ANY sound (regression: a refactor
//     removed the EmitSound call).
//   * Forge emits with the wrong loudness or radius -- specifically,
//     a radius below 30m means the priest can be standing right
//     outside the village and miss the craft, breaking the
//     "audible across the village" design.
//   * Forge accidentally emits sound on REFUSED crafts (wrong input
//     tag). The test's clear+craft+check structure surfaces this if
//     the craft's input-match guard moved BELOW the EmitSound call.
// ============================================================================

namespace
{
	enum Phase : int {
		kFA_Start, kFA_WaitScene, kFA_BuildForge, kFA_BuildInput,
		kFA_HandToVillager, kFA_ClearSounds, kFA_Craft,
		kFA_Verify, kFA_Done
	};

	int                     g_iPhase = kFA_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xForge;
	Zenith_EntityID         g_xInput;
	DPForge_Behaviour*      g_pxForge = nullptr;
	Zenith_Maths::Vector3   g_xForgePos(0.0f);

	uint32_t                g_uSoundCount = 0;
	const char*             g_szFirstSoundName = nullptr;
	float                   g_fFirstSoundLoudness = -1.0f;
	float                   g_fFirstSoundRadius = -1.0f;
	Zenith_Maths::Vector3   g_xFirstSoundPos(0.0f);

	float                   g_fExpectedLoudness = 1.0f;
	float                   g_fExpectedRadius = 30.0f;
}

static void Setup_P2ForgeAudible()
{
	g_iPhase = kFA_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xForge = INVALID_ENTITY_ID;
	g_xInput = INVALID_ENTITY_ID;
	g_pxForge = nullptr;
	g_xForgePos = Zenith_Maths::Vector3(0.0f);
	g_uSoundCount = 0;
	g_szFirstSoundName = nullptr;
	g_fFirstSoundLoudness = -1.0f;
	g_fFirstSoundRadius = -1.0f;
	g_xFirstSoundPos = Zenith_Maths::Vector3(0.0f);
	g_fExpectedLoudness = 1.0f;
	g_fExpectedRadius = 30.0f;
}

static bool Step_P2ForgeAudible(int iFrame)
{
	switch (g_iPhase)
	{
	case kFA_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFA_WaitScene;
		return true;

	case kFA_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kFA_BuildForge;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFA_Done;
		}
		return true;
	}

	case kFA_BuildForge:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFA_Done; return false; }
		Zenith_Entity xForge = g_xEngine.Scenes().CreateEntity(pxScene, std::string("Test_AudibleForge"));
		if (!xForge.IsValid()) { g_iPhase = kFA_Done; return false; }
		g_xForge = xForge.GetEntityID();
		g_xForgePos = Zenith_Maths::Vector3(150.0f, 0.0f, 150.0f);
		if (xForge.HasComponent<Zenith_TransformComponent>())
		{
			xForge.GetComponent<Zenith_TransformComponent>().SetPosition(g_xForgePos);
		}
		g_pxForge = xForge.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPForge_Behaviour>();
		// Default recipe Iron -> Key (no SetRecipe needed).
		g_iPhase = kFA_BuildInput;
		return true;
	}

	case kFA_BuildInput:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFA_Done; return false; }
		Zenith_Entity xInput = g_xEngine.Scenes().CreateEntity(pxScene, std::string("ForgeAudible_Iron"));
		if (!xInput.IsValid()) { g_iPhase = kFA_Done; return false; }
		g_xInput = xInput.GetEntityID();
		xInput.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Behaviour* pxIB = xInput.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (pxIB != nullptr) pxIB->SetTag(DP_ItemTag::Iron);
		g_iPhase = kFA_HandToVillager;
		return true;
	}

	case kFA_HandToVillager:
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::SetHeldItem(g_xVillager, g_xInput);
		g_iPhase = kFA_ClearSounds;
		return true;

	case kFA_ClearSounds:
		// Read expected values from tuning so the test stays in
		// lockstep with any rebalancing.
		g_fExpectedLoudness = DP_Tuning::Get<float>("interactables.forge_audible_loudness");
		g_fExpectedRadius = DP_Tuning::Get<float>("interactables.forge_audible_at_m");
		// Clear the AudioBus recorder so the assertion captures
		// only this craft's emit.
		Zenith_AudioBus::ClearEmittedSoundsForTest();
		g_iPhase = kFA_Craft;
		return true;

	case kFA_Craft:
		if (g_pxForge != nullptr) g_pxForge->CraftForTest(g_xVillager);
		g_iPhase = kFA_Verify;
		return true;

	case kFA_Verify:
	{
		const auto& xSounds = Zenith_AudioBus::GetEmittedSoundsForTest();
		g_uSoundCount = xSounds.GetSize();
		if (g_uSoundCount > 0)
		{
			const auto& xS = xSounds.Get(0);
			g_szFirstSoundName = xS.m_szName;
			g_fFirstSoundLoudness = xS.m_fLoudness;
			g_fFirstSoundRadius = xS.m_fRadius;
			g_xFirstSoundPos = xS.m_xPosition;
		}
		std::printf("[P2ForgeAudible] soundCount=%u name=%s loudness=%.3f radius=%.3f pos=(%.2f,%.2f,%.2f) expected: loud=%.3f rad=%.3f\n",
			g_uSoundCount,
			(g_szFirstSoundName != nullptr ? g_szFirstSoundName : "(null)"),
			g_fFirstSoundLoudness, g_fFirstSoundRadius,
			g_xFirstSoundPos.x, g_xFirstSoundPos.y, g_xFirstSoundPos.z,
			g_fExpectedLoudness, g_fExpectedRadius);
		std::fflush(stdout);
		g_iPhase = kFA_Done;
		return false;
	}

	case kFA_Done:
	default:
		return false;
	}
}

static bool Verify_P2ForgeAudible()
{
	if (!g_xVillager.IsValid() || !g_xForge.IsValid() || !g_xInput.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ForgeAudible: setup entities missing");
		return false;
	}
	if (g_uSoundCount == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: no sound emitted after CraftForTest. The forge's HandleInteract isn't calling Zenith_AudioBus::EmitSound on successful craft");
		return false;
	}
	if (g_uSoundCount > 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: %u sounds emitted by a single craft -- expected exactly 1",
			g_uSoundCount);
		return false;
	}
	// Name should contain "Forge".
	if (g_szFirstSoundName == nullptr || std::strstr(g_szFirstSoundName, "Forge") == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: emitted sound name was \"%s\", expected to contain \"Forge\"",
			(g_szFirstSoundName != nullptr ? g_szFirstSoundName : "(null)"));
		return false;
	}
	// Loudness must equal the tuning value.
	if (g_fFirstSoundLoudness < g_fExpectedLoudness - 0.01f
		|| g_fFirstSoundLoudness > g_fExpectedLoudness + 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: emitted loudness %.3f does not match tuning value %.3f",
			g_fFirstSoundLoudness, g_fExpectedLoudness);
		return false;
	}
	// Radius >= 30m. The roadmap-spec test name is "AudibleAt30m",
	// and the assertion is specifically that the audible radius
	// covers AT LEAST 30 m -- a smaller radius would let priests
	// outside that range miss the craft entirely.
	if (g_fFirstSoundRadius + 0.01f < g_fExpectedRadius)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: emitted radius %.3f is less than the configured forge_audible_at_m %.3f. The forge's audible reach is too short -- priest can stand outside the village and miss crafts",
			g_fFirstSoundRadius, g_fExpectedRadius);
		return false;
	}
	// Position close to forge spawn (allow 2m slop for any
	// transform-readback jitter).
	const float fDx = g_xFirstSoundPos.x - g_xForgePos.x;
	const float fDz = g_xFirstSoundPos.z - g_xForgePos.z;
	if (fDx * fDx + fDz * fDz > 4.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgeAudible: emit position (%.2f,%.2f,%.2f) too far from forge spawn (%.2f,%.2f,%.2f)",
			g_xFirstSoundPos.x, g_xFirstSoundPos.y, g_xFirstSoundPos.z,
			g_xForgePos.x, g_xForgePos.y, g_xForgePos.z);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ForgeAudibleTest = {
	"Test_P2Forge_AudibleAt30m",
	&Setup_P2ForgeAudible,
	&Step_P2ForgeAudible,
	&Verify_P2ForgeAudible,
	180
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ForgeAudibleTest);

#endif // ZENITH_INPUT_SIMULATOR
