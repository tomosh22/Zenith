#pragma once
/**
 * DPFogPass_Behaviour - per-frame fog-hole table maintenance.
 *
 * Strategy: clear-and-rebuild every frame. ClearAllFogHoles() first, then
 * iterate possessed villager + lights and call RegisterFogHole on each.
 * This avoids stale entries from destroyed entities and means producers
 * don't have to subscribe to entity-lifetime callbacks.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Query.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

class DPFogPass_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPFogPass_Behaviour)

	DPFogPass_Behaviour() = delete;
	DPFogPass_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnUpdate(const float /*fDt*/) ZENITH_FINAL override
	{
		DP_Fog::ClearAllFogHoles();

		// Every villager — possessed or idle — cuts a fog hole. The
		// player needs to see ALL villagers at all times so they can pick
		// the next one to possess after the current host expires (and
		// can keep tabs on which idle villagers are getting close to the
		// priest). Skeletal-grade: a fixed radius around the cube
		// silhouette; Wave-4 polish could vary it by villager state.
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[this](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				DP_Fog::RegisterFogHole(xId, m_fVillagerHoleRadius);
			});

		// Lights — every dynamic light reveals an area around itself. Size
		// the fog hole to the light's actual Range so dim torches with a
		// 6 m falloff get a 6 m fog cut-out and powerful directional /
		// point lights can punch a wider hole. The +m_fLightHoleSlop
		// margin makes sure fog never re-asserts inside the radius where
		// the light's contribution is still meaningful.
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene != nullptr)
		{
			pxScene->Query<Zenith_LightComponent>().ForEach(
				[this](Zenith_EntityID xId, Zenith_LightComponent& xLight)
				{
					const float fRadius = xLight.GetRange() + m_fLightHoleSlop;
					DP_Fog::RegisterFogHole(xId, fRadius);
				});
		}
	}

private:
	float m_fVillagerHoleRadius = 3.0f;
	// Padding added to the light's Range when sizing its fog hole. Set
	// just above 0 so the hole envelops the actual lit area without a
	// visible "ring of fog" where the light's contribution drops to zero.
	float m_fLightHoleSlop      = 0.5f;
};
