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
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Query.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include "Collections/Zenith_HashMap.h"

// FogHole + MemoryCellKey live next to their owning script so the
// PublicInterfaces.cpp anon-namespace doesn't need to forward-declare
// them when wiring up the namespace-API forwarders.
struct DPFogHole
{
	Zenith_EntityID m_xId;
	float           m_fRadius;
};

struct DPMemoryCellKey
{
	int32_t iX;
	int32_t iZ;
	bool operator==(const DPMemoryCellKey& o) const { return iX == o.iX && iZ == o.iZ; }
};

struct DPMemoryCellKeyHash
{
	size_t operator()(const DPMemoryCellKey& k) const
	{
		const uint64_t ux = static_cast<uint64_t>(static_cast<uint32_t>(k.iX));
		const uint64_t uz = static_cast<uint64_t>(static_cast<uint32_t>(k.iZ));
		return static_cast<size_t>((ux * 73856093u) ^ (uz * 19349663u));
	}
};

class DPFogPass_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPFogPass_Behaviour)

	DPFogPass_Behaviour() = delete;
	DPFogPass_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		// Per-scene singleton. Replaces the previous process-global
		// g_xFogHoles + g_xMemoryReveals in PublicInterfaces.cpp so the
		// tables die with the scene that owns them (2026-05-17 Phase B
		// ownership refactor).
		s_pxInstance = this;
	}

	void OnDestroy() ZENITH_FINAL override
	{
		if (s_pxInstance == this) s_pxInstance = nullptr;
	}

	static DPFogPass_Behaviour* Instance() { return s_pxInstance; }

	//==========================================================================
	// Fog-hole registry. Keyed by emitter EntityID (villager or light).
	// Cleared via per-frame ClearAllFogHoles in OnUpdate -- the table never
	// holds stale entries between frames, but it's still scene-owned so
	// nothing leaks across scene transitions either.
	//==========================================================================
	void RegisterFogHole(Zenith_EntityID xId, float fRadius)
	{
		DPFogHole xHole;
		xHole.m_xId     = xId;
		xHole.m_fRadius = fRadius;
		m_xFogHoles.Insert(xId, xHole);
	}

	void UnregisterFogHole(Zenith_EntityID xId)
	{
		m_xFogHoles.Remove(xId);
	}

	void ClearAllFogHoles() { m_xFogHoles.Clear(); }

	uint32_t GetFogHoleCount() const { return static_cast<uint32_t>(m_xFogHoles.GetSize()); }

	// Callback iteration -- decouples the namespace consumer from
	// Zenith_HashMap. Callback signature: void(Zenith_EntityID, float).
	template <typename TFn>
	void ForEachFogHole(TFn xFn) const
	{
		Zenith_HashMap<Zenith_EntityID, DPFogHole>::Iterator it(m_xFogHoles);
		while (!it.Done())
		{
			const DPFogHole& xHole = it.GetValue();
			xFn(xHole.m_xId, xHole.m_fRadius);
			it.Next();
		}
	}

	//==========================================================================
	// Memory-fog cell ages. Keyed by snapped 1 m grid cell; value is the
	// cell's age in seconds since the most recent reveal. Read by
	// GetMemoryStateAt; written by RecordMemoryReveal + TickMemoryFog.
	//==========================================================================
	void RecordMemoryRevealCell(const DPMemoryCellKey& xKey) { m_xMemoryReveals.Insert(xKey, 0.0f); }

	void TickMemoryFog(float fDt)
	{
		if (fDt <= 0.0f) return;
		Zenith_HashMap<DPMemoryCellKey, float, DPMemoryCellKeyHash>::Iterator it(m_xMemoryReveals);
		while (!it.Done())
		{
			it.GetValueMutable() += fDt;
			it.Next();
		}
	}

	float GetMemoryCellAgeOrNeg1(const DPMemoryCellKey& xKey) const
	{
		const float* pxAge = m_xMemoryReveals.TryGet(xKey);
		if (pxAge == nullptr) return -1.0f;
		return *pxAge;
	}

	void ClearAllMemoryReveals() { m_xMemoryReveals.Clear(); }

	uint32_t GetMemoryRevealCount() const { return static_cast<uint32_t>(m_xMemoryReveals.GetSize()); }

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
				// MVP-2.4.5: record memory reveals for each villager
				// position each frame. Cells that stay in range keep
				// their age at 0 (refreshed every frame); cells the
				// villager moves AWAY from will age through the
				// VisitedVisible -> VisitedDim -> VisitedHidden
				// states over the next 30 s.
				Zenith_Maths::Vector3 xVPos;
				Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
				if (pxScene != nullptr)
				{
					Zenith_Entity xV = pxScene->TryGetEntity(xId);
					if (xV.IsValid() && xV.HasComponent<Zenith_TransformComponent>())
					{
						xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);
						DP_Fog::RecordMemoryReveal(xVPos);
					}
				}
			});

		// Lights — every dynamic light reveals an area around itself. Size
		// the fog hole to the light's actual Range so dim torches with a
		// 6 m falloff get a 6 m fog cut-out and powerful directional /
		// point lights can punch a wider hole. The +m_fLightHoleSlop
		// margin makes sure fog never re-asserts inside the radius where
		// the light's contribution is still meaningful.
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
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

	static inline DPFogPass_Behaviour* s_pxInstance = nullptr;

	// Per-scene fog-hole + memory-fog tables. Cleared automatically
	// when this script is destroyed (scene unload).
	Zenith_HashMap<Zenith_EntityID, DPFogHole> m_xFogHoles;
	Zenith_HashMap<DPMemoryCellKey, float, DPMemoryCellKeyHash> m_xMemoryReveals;
};
