#include "Zenith.h"

#include "Zenithmon/Components/ZM_TallGrassSystem.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_Scene.h"          // entity-component template calls (TryGetComponent)
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"
#include "Zenithmon/Source/World/ZM_EncounterZone.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>

ZM_TallGrassSystem::ZM_TallGrassSystem(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_TallGrassSystem::OnAwake()
{
	// Runtime tile tracking always starts fresh; the density map is (re)loaded below.
	m_xLastTile = ZM_GrassTile{};
	m_bHasLastTile = false;
	// Clear any test-armed force flag so an unconsumed arm cannot leak a spurious
	// forced encounter into a later re-awake (test-isolation hygiene; the flag is
	// only ever written under ZENITH_INPUT_SIMULATOR).
	m_bForceEncounter = false;

	// Sibling-terrain resolution mirrors ZM_TerrainGrass: the density map is loaded
	// from the same canonical path so encounter sampling matches the rendered grass,
	// while keeping its OWN CPU copy (Flux is deliberately untouched here).
	Zenith_TerrainComponent* pxTerrain =
		m_xParentEntity.TryGetComponent<Zenith_TerrainComponent>();
	if (pxTerrain == nullptr)
	{
		// No terrain sibling: disabled. HasDensityMap() stays false and OnUpdate no-ops.
		m_xDensityMap.Clear();
		return;
	}

	const std::string strDensityPath =
		ZM_GrassDensityMap::BuildCanonicalPath(pxTerrain->GetTerrainAssetDirectory());
	if (!m_xDensityMap.Load(strDensityPath))
	{
		// Missing / malformed map: disabled, but harmless (encounters simply never fire).
		m_xDensityMap.Clear();
	}

	// Deterministic per-boot seed: a fixed salt XOR the resolved scene id. Even when
	// the id is unresolved (ZM_SCENE_NONE, a fixed value) the seed stays deterministic.
	m_xRng.Seed(ulRNG_SEED_SALT ^ static_cast<u_int64>(ResolveActiveSceneId()));
}

void ZM_TallGrassSystem::OnUpdate(float /*fDeltaTime*/)
{
	if (!m_xDensityMap.IsLoaded())
	{
		return;
	}

	float fX = 0.0f;
	float fZ = 0.0f;
	if (!TrySampleActivePlayerXZ(fX, fZ))
	{
		return;
	}

	const ZM_GrassTile xCurrentTile = QuantizeToTile(fX, fZ);
	if (IsTileTransition(m_xLastTile, m_bHasLastTile, xCurrentTile))
	{
		const float fDensity = m_xDensityMap.SampleWorld(fX, fZ);
		if (IsGrassDensity(fDensity))
		{
			const ZM_SCENE_ID eScene = ResolveActiveSceneId();
			// A ZM_SCENE_NONE (== ZM_SCENE_COUNT) id would trip ZM_GetWorldSpec's
			// bounds assert inside RollStepForScene, so gate it out here.
			if (eScene < ZM_SCENE_COUNT)
			{
				ZM_EncounterRollResult xResult =
					ZM_EncounterZone::RollStepForScene(eScene, m_xRng);

#ifdef ZENITH_INPUT_SIMULATOR
				// Test seam: force the next on-grass transition to fire deterministically
				// (independent of the per-route rate). If the honest roll already hit we
				// keep it; otherwise synthesise from the scene's first encounter slot.
				if (m_bForceEncounter)
				{
					m_bForceEncounter = false;
					if (!xResult.m_bEncounter)
					{
						const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(eScene);
						if (xSpec.m_uEncounterCount > 0u && xSpec.m_pxEncounters != nullptr)
						{
							xResult.m_bEncounter = true;
							xResult.m_eSpecies = xSpec.m_pxEncounters[0].m_eSpecies;
							xResult.m_uLevel = xSpec.m_pxEncounters[0].m_uMinLevel;
						}
					}
				}
#endif

				if (xResult.m_bEncounter)
				{
					// EMIT only -- item 3 subscribes and loads the additive battle.
					Zenith_EventDispatcher::Get().Dispatch(
						ZM_OnWildEncounter{ xResult.m_eSpecies, xResult.m_uLevel, eScene });
				}
			}
		}
	}

	// Always advance the baseline (even off-grass) so re-entering grass reads as a
	// fresh transition rather than a same-tile no-op.
	m_xLastTile = xCurrentTile;
	m_bHasLastTile = true;
}

void ZM_TallGrassSystem::OnDestroy()
{
	// Drop the CPU copy and the tile baseline so a torn-down scene cannot roll.
	m_xDensityMap.Clear();
	m_bHasLastTile = false;
}

void ZM_TallGrassSystem::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
}

void ZM_TallGrassSystem::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Runtime-only tracking never persists: a freshly-loaded instance re-establishes
	// its baseline on the first OnUpdate, so entering grass is a clean transition.
	m_xLastTile = ZM_GrassTile{};
	m_bHasLastTile = false;

	// Version gate: v1 carries no payload beyond the version tag. A mismatch simply
	// leaves the reset defaults in place (nothing further to consume).
	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}
}

#ifdef ZENITH_TOOLS
void ZM_TallGrassSystem::RenderPropertiesPanel()
{
	ImGui::Text("Tall grass density map: %s", m_xDensityMap.IsLoaded() ? "loaded" : "unloaded");
	ImGui::Text("Last tile: (%d, %d) valid=%s",
		m_xLastTile.m_iX, m_xLastTile.m_iZ, m_bHasLastTile ? "true" : "false");
}
#endif

bool ZM_TallGrassSystem::HasDensityMap() const
{
	return m_xDensityMap.IsLoaded();
}

ZM_GrassTile ZM_TallGrassSystem::QuantizeToTile(float fWorldX, float fWorldZ)
{
	// std::floor (not truncation) so negatives round toward -inf: -0.3 -> -1.
	ZM_GrassTile xTile;
	xTile.m_iX = static_cast<int>(std::floor(fWorldX));
	xTile.m_iZ = static_cast<int>(std::floor(fWorldZ));
	return xTile;
}

bool ZM_TallGrassSystem::IsTileTransition(ZM_GrassTile xLast, bool bHasLast, ZM_GrassTile xCurrent)
{
	return bHasLast && (xLast.m_iX != xCurrent.m_iX || xLast.m_iZ != xCurrent.m_iZ);
}

bool ZM_TallGrassSystem::IsGrassDensity(float fDensity)
{
	return fDensity >= fGRASS_DENSITY_THRESHOLD;
}

#ifdef ZENITH_INPUT_SIMULATOR
void ZM_TallGrassSystem::ForceEncounterOnNextTransitionForTests()
{
	m_bForceEncounter = true;
}

void ZM_TallGrassSystem::SetRngSeedForTests(u_int64 ulSeed)
{
	m_xRng = ZM_BattleRNG(ulSeed);
}
#endif

bool ZM_TallGrassSystem::TrySampleActivePlayerXZ(float& fXOut, float& fZOut) const
{
	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	if (!ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xPlayerEntityID))
	{
		return false;
	}

	Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerEntityID);
	if (!xPlayer.IsValid())
	{
		return false;
	}

	Zenith_TransformComponent* pxTransform =
		xPlayer.TryGetComponent<Zenith_TransformComponent>();
	if (pxTransform == nullptr)
	{
		return false;
	}

	Zenith_Maths::Vector3 xPosition;
	pxTransform->GetPosition(xPosition);
	fXOut = xPosition.x;
	fZOut = xPosition.z;
	return true;
}

ZM_SCENE_ID ZM_TallGrassSystem::ResolveActiveSceneId() const
{
	const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActiveScene);
	if (!xInfo.m_bLoaded || xInfo.m_iBuildIndex < 0)
	{
		return ZM_SCENE_NONE;
	}
	return ZM_FindSceneByBuildIndex(static_cast<u_int>(xInfo.m_iBuildIndex));
}
