#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPFogPass_Component - per-frame fog-hole table maintenance.
 *
 * Strategy: clear-and-rebuild every frame. ClearAllFogHoles() first, then
 * iterate possessed villager + lights and call RegisterFogHole on each.
 * This avoids stale entries from destroyed entities and means producers
 * don't have to subscribe to entity-lifetime callbacks.
 */

#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Query.h"
#include "DataStream/Zenith_DataStream.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPFogPass.h"
#include "Source/DPTelemetry.h"

#include "Collections/Zenith_HashMap.h"

// FogHole + MemoryCellKey live next to their owning component so the
// DP_Fog forwarders don't need to forward-declare them when wiring up the
// namespace-API forwarders.
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

class DPFogPass_Component ZENITH_FINAL
{
public:
	DPFogPass_Component() = delete;
	DPFogPass_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: s_pxInstance points at `this`, and component pools
	// relocate on resize / swap-and-pop / cross-scene transfer. Hand-written
	// moves repoint the singleton at the new address; copies deleted.
	DPFogPass_Component(const DPFogPass_Component&) = delete;
	DPFogPass_Component& operator=(const DPFogPass_Component&) = delete;

	DPFogPass_Component(DPFogPass_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_fVillagerHoleRadius(xOther.m_fVillagerHoleRadius)
		, m_fLightHoleSlop(xOther.m_fLightHoleSlop)
		, m_uUpdateCounter(xOther.m_uUpdateCounter)
		, m_xFogHoles(std::move(xOther.m_xFogHoles))
		, m_xMemoryReveals(std::move(xOther.m_xMemoryReveals))
	{
		if (s_pxInstance == &xOther) s_pxInstance = this;
	}

	DPFogPass_Component& operator=(DPFogPass_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity       = xOther.m_xParentEntity;
			m_fVillagerHoleRadius = xOther.m_fVillagerHoleRadius;
			m_fLightHoleSlop      = xOther.m_fLightHoleSlop;
			m_uUpdateCounter      = xOther.m_uUpdateCounter;
			m_xFogHoles           = std::move(xOther.m_xFogHoles);
			m_xMemoryReveals      = std::move(xOther.m_xMemoryReveals);
			if (s_pxInstance == &xOther) s_pxInstance = this;
		}
		return *this;
	}

	~DPFogPass_Component()
	{
		// Pool relocation destructs the moved-from source; only clear the
		// singleton when it still points at THIS instance.
		if (s_pxInstance == this)
		{
			s_pxInstance = nullptr;
			DPFogPass::ResetMemoryWindow();
		}
	}

	void OnAwake()
	{
		// Per-scene singleton. Replaces the previous process-global
		// g_xFogHoles + g_xMemoryReveals in PublicInterfaces.cpp so the
		// tables die with the scene that owns them (2026-05-17 Phase B
		// ownership refactor).
		Zenith_Assert(s_pxInstance == nullptr,
			"DPFogPass_Component singleton double-instantiated");
		s_pxInstance = this;
	}

	void OnDestroy()
	{
		if (s_pxInstance == this)
		{
			s_pxInstance = nullptr;
			// The render pass outlives this scene-owned component — zero
			// the shader's memory window so menu / Liminal / editor-Stop
			// frames don't sample the dead run's reveal pattern.
			DPFogPass::ResetMemoryWindow();
		}
	}

	// Component contract: version-only payload (fog tables rebuild per frame).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	static DPFogPass_Component* Instance() { return s_pxInstance; }

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

	// Callback iteration over the memory cells -- decouples the namespace
	// consumer (DP_Fog rasterizer/bounds) from Zenith_HashMap. Callback
	// signature: void(const DPMemoryCellKey&, float fAgeSeconds).
	template <typename TFn>
	void ForEachMemoryCell(TFn xFn) const
	{
		Zenith_HashMap<DPMemoryCellKey, float, DPMemoryCellKeyHash>::Iterator it(m_xMemoryReveals);
		while (!it.Done())
		{
			xFn(it.GetKey(), it.GetValue());
			it.Next();
		}
	}

	void ClearAllMemoryReveals() { m_xMemoryReveals.Clear(); }

	uint32_t GetMemoryRevealCount() const { return static_cast<uint32_t>(m_xMemoryReveals.GetSize()); }

	void OnUpdate(const float /*fDt*/)
	{
		DP_Fog::ClearAllFogHoles();

		// Every villager — possessed or idle — cuts a fog hole. The
		// player needs to see ALL villagers at all times so they can pick
		// the next one to possess after the current host expires (and
		// can keep tabs on which idle villagers are getting close to the
		// priest). Skeletal-grade: a fixed radius around the cube
		// silhouette; Wave-4 polish could vary it by villager state.
		// Relocated to DP_Fog::RegisterAllVillagerFogHoles so this header no
		// longer includes DPVillager_Component.h (cross-component rule). The
		// per-villager fog-hole + memory-reveal logic is unchanged; the
		// m_fVillagerHoleRadius value is passed through as the hole radius.
		DP_Fog::RegisterAllVillagerFogHoles(m_fVillagerHoleRadius);

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

		// GPU mirror: rasterize the memory table and stage the R8 texture
		// upload once per frame, after this frame's reveals landed above.
		// Ordering vs TickMemoryFog (player controller's OnUpdate) is one
		// frame loose at worst -- invisible at the 10-30 s state timescales.
		DPFogPass::UpdateMemoryTexture();

		// Telemetry: periodic fog-memory health sample (~1 Hz at 60 fps).
		// ints[0] = revealed cell count, floats[0] = fraction aged past
		// the visible window -- the DPTelemetryAnalyzer::FogMemoryAges
		// criterion reads these. RecordEvent no-ops outside a recording.
		if ((m_uUpdateCounter++ % 60u) == 0u)
		{
			uint32_t uVisible = 0, uDim = 0, uHidden = 0;
			DP_Fog::GetMemoryStateCounts(uVisible, uDim, uHidden);
			const uint32_t uTotal = uVisible + uDim + uHidden;
			const float fAgedFraction = uTotal > 0u
				? static_cast<float>(uDim + uHidden) / static_cast<float>(uTotal)
				: 0.0f;
			DPTelemetry::EmitEvent(DPTelemetry::DPEventType::FogMemorySample,
				Zenith_EntityID{}, Zenith_EntityID{},
				static_cast<int32_t>(uTotal), fAgedFraction,
				nullptr, "DPFogPass");
		}
	}

private:
	Zenith_Entity m_xParentEntity;

	float m_fVillagerHoleRadius = 3.0f;
	// Padding added to the light's Range when sizing its fog hole. Set
	// just above 0 so the hole envelops the actual lit area without a
	// visible "ring of fog" where the light's contribution drops to zero.
	float m_fLightHoleSlop      = 0.5f;

	// Frame counter for the periodic telemetry sample above.
	uint32_t m_uUpdateCounter   = 0;

	static inline DPFogPass_Component* s_pxInstance = nullptr;

	// Per-scene fog-hole + memory-fog tables. Cleared automatically
	// when this component is destroyed (scene unload).
	Zenith_HashMap<Zenith_EntityID, DPFogHole> m_xFogHoles;
	Zenith_HashMap<DPMemoryCellKey, float, DPMemoryCellKeyHash> m_xMemoryReveals;
};
