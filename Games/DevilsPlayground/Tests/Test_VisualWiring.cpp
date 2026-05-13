#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// VisualWiring_Test
//
// Verifies that Project_RegisterEditorAutomationSteps actually wires meshes,
// colliders, and lights into the GameLevel scene.  Without this test the
// scene authoring could create an entity tree of all-bare transforms and
// nothing would render or simulate.
//
// Phases (driven by iFrame in Step):
//    0       — request LoadSceneByIndex(1, SINGLE).
//    1..30   — wait for the scene to swap (a Villager script appearing in
//              the active scene is the heuristic).  Bail at 30 if not.
//    >=31    — count component presences across the scene, then return false.
// ============================================================================

namespace
{
	bool g_bTriggered     = false;
	bool g_bSceneSwapped  = false;
	bool g_bAssertionsRan = false;

	int g_iEntitiesWithModelAndTransform = 0;
	int g_iColliderEntities              = 0;
	int g_iLightEntities                 = 0;
	int g_iTotalEntities                 = 0;
}

static void Setup_VisualWiring()
{
	g_bTriggered     = false;
	g_bSceneSwapped  = false;
	g_bAssertionsRan = false;

	g_iEntitiesWithModelAndTransform = 0;
	g_iColliderEntities              = 0;
	g_iLightEntities                 = 0;
	g_iTotalEntities                 = 0;
}

static bool Step_VisualWiring(int iFrame)
{
	if (iFrame == 0)
	{
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_bTriggered = true;
		return true;
	}

	if (!g_bSceneSwapped)
	{
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActive);
		if (pxSceneData != nullptr)
		{
			int iFound = 0;
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&iFound](Zenith_EntityID, DPVillager_Behaviour&) { ++iFound; });
			if (iFound > 0) g_bSceneSwapped = true;
		}
		if (iFrame > 30 && !g_bSceneSwapped)
		{
			return false; // give up
		}
		if (!g_bSceneSwapped) return true;
	}

	if (!g_bAssertionsRan)
	{
		// Tally components in the active scene by walking each component pool.
		// Query<TransformComponent, ModelComponent> proves we have at least one
		// renderable; Query<ColliderComponent>() and Query<LightComponent>()
		// prove physics + lighting are wired.
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActive);
		if (pxSceneData != nullptr)
		{
			pxSceneData->Query<Zenith_TransformComponent, Zenith_ModelComponent>().ForEach(
				[](Zenith_EntityID, Zenith_TransformComponent&, Zenith_ModelComponent&)
				{
					++g_iEntitiesWithModelAndTransform;
				});
			pxSceneData->Query<Zenith_ColliderComponent>().ForEach(
				[](Zenith_EntityID, Zenith_ColliderComponent&)
				{
					++g_iColliderEntities;
				});
			pxSceneData->Query<Zenith_LightComponent>().ForEach(
				[](Zenith_EntityID, Zenith_LightComponent&)
				{
					++g_iLightEntities;
				});
			pxSceneData->Query<Zenith_TransformComponent>().ForEach(
				[](Zenith_EntityID, Zenith_TransformComponent&)
				{
					++g_iTotalEntities;
				});
		}

		g_bAssertionsRan = true;
		return false;
	}

	return false;
}

static bool Verify_VisualWiring()
{
	if (!g_bTriggered)         return false;
	if (!g_bSceneSwapped)      return false;
	if (!g_bAssertionsRan)     return false;

	// The scene authors >130 model+transform entities (StaticDeco alone is 139)
	// but we only assert "at least one of each" — the spec asks for proof that
	// mesh/light/collider wiring works, not exact counts (those will drift as
	// the level evolves).
	if (g_iEntitiesWithModelAndTransform < 1) return false;
	if (g_iColliderEntities              < 1) return false;
	if (g_iLightEntities                 < 1) return false;

	// Sanity: scene should not be empty (would indicate a regression).
	if (g_iTotalEntities < 10) return false;

	return true;
}

static const Zenith_AutomatedTest g_xVisualWiringTest = {
	"VisualWiring_Test",
	&Setup_VisualWiring,
	&Step_VisualWiring,
	&Verify_VisualWiring,
	240, // 4 seconds at 60Hz — generous for cold-load
	true // m_bRequiresGraphics: counts mesh/light/collider components after GPU scene load
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xVisualWiringTest);

#endif // ZENITH_INPUT_SIMULATOR
