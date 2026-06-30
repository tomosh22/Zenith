#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "DP_Fog.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include "DP_Query.h"
#include "../Components/DPFogPass_Component.h"
#include "../Components/DPVillager_Component.h"

#include <cmath>

namespace DP_Fog
{
	// 2026-05-17 scene-ownership refactor: fog-hole + memory-fog
	// tables moved onto DPFogPass_Component. Forwarders below are
	// no-ops when no fog-pass component is loaded (between-scenes /
	// non-DP scenes).

	void RegisterFogHole(Zenith_EntityID xId, float fRadius)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::RegisterFogHole must be called from main thread");
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->RegisterFogHole(xId, fRadius);
	}

	void UnregisterFogHole(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::UnregisterFogHole must be called from main thread");
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->UnregisterFogHole(xId);
	}

	void ClearAllFogHoles()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::ClearAllFogHoles must be called from main thread");
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->ClearAllFogHoles();
	}

	uint32_t GetFogHoleCount()
	{
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return 0;
		return pxFog->GetFogHoleCount();
	}

	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles)
	{
		if (pxOutHoles == nullptr || uMaxHoles == 0) return 0;
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return 0;
		uint32_t uWritten = 0;
		pxFog->ForEachFogHole(
			[pxOutHoles, uMaxHoles, &uWritten]
			(Zenith_EntityID xHoleId, float fRadius)
			{
				if (uWritten >= uMaxHoles) return;
				Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xHoleId);
				if (!xEnt.IsValid()) return;
				Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
				if (pxTransform == nullptr) return;
				Vec3 xPos;
				pxTransform->GetPosition(xPos);
				pxOutHoles[uWritten++] = Vec4(xPos.x, xPos.y, xPos.z, fRadius);
			});
		return uWritten;
	}

	// ========================================================================
	// MVP-2.4.5: Memory fog implementation. State machine moved onto
	// DPFogPass_Component as part of the 2026-05-17 ownership refactor;
	// these forwarders convert Vec3 positions to DPMemoryCellKey and
	// delegate to the per-scene script.
	// ========================================================================
	namespace
	{
		// 1 m grid resolution: small enough that a moving villager's
		// fog hole touches multiple cells per tick (each becomes its
		// own memory entry), large enough that the cell count stays
		// bounded across a 200 m * 200 m level.
		DPMemoryCellKey CellKeyForPosition(const Vec3& xPos)
		{
			DPMemoryCellKey k;
			k.iX = static_cast<int32_t>(std::floor(xPos.x));
			k.iZ = static_cast<int32_t>(std::floor(xPos.z));
			return k;
		}
	}

	void RecordMemoryReveal(Vec3 xPosition)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::RecordMemoryReveal must be called from main thread");
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->RecordMemoryRevealCell(CellKeyForPosition(xPosition));
	}

	void TickMemoryFog(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::TickMemoryFog must be called from main thread");
		if (fDt <= 0.0f) return;
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->TickMemoryFog(fDt);
	}

	MemoryTileState GetMemoryStateAt(Vec3 xPosition)
	{
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return MemoryTileState::NeverSeen;
		const DPMemoryCellKey k = CellKeyForPosition(xPosition);
		const float fAge = pxFog->GetMemoryCellAgeOrNeg1(k);
		if (fAge < 0.0f) return MemoryTileState::NeverSeen;
		const float fVisibleThreshold =
			DP_Tuning::Get<float>("fog_of_war.memory_visible_s");
		const float fDimThreshold =
			DP_Tuning::Get<float>("fog_of_war.memory_dim_s");
		if (fAge <= fVisibleThreshold) return MemoryTileState::VisitedVisible;
		if (fAge <= fDimThreshold)     return MemoryTileState::VisitedDim;
		return MemoryTileState::VisitedHidden;
	}

	uint32_t GetMemoryRevealCount()
	{
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return 0;
		return pxFog->GetMemoryRevealCount();
	}

	void ClearAllMemoryReveals()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::ClearAllMemoryReveals must be called from main thread");
		DPFogPass_Component* pxFog = DPFogPass_Component::Instance();
		if (pxFog == nullptr) return;
		pxFog->ClearAllMemoryReveals();
	}

	// Cross-component forwarder: every villager — possessed or idle — cuts a
	// fog hole and records a memory reveal. Moved here from
	// DPFogPass_Component::OnUpdate so the fog-pass header no longer includes
	// DPVillager_Component.h. fRadius replaces the component's m_fVillagerHoleRadius.
	void RegisterAllVillagerFogHoles(float fRadius)
	{
		// Every villager — possessed or idle — cuts a fog hole. The
		// player needs to see ALL villagers at all times so they can pick
		// the next one to possess after the current host expires (and
		// can keep tabs on which idle villagers are getting close to the
		// priest). Skeletal-grade: a fixed radius around the cube
		// silhouette; Wave-4 polish could vary it by villager state.
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[fRadius](Zenith_EntityID xId, DPVillager_Component&)
			{
				RegisterFogHole(xId, fRadius);
				// MVP-2.4.5: record memory reveals for each villager
				// position each frame. Cells that stay in range keep
				// their age at 0 (refreshed every frame); cells the
				// villager moves AWAY from will age through the
				// VisitedVisible -> VisitedDim -> VisitedHidden
				// states over the next 30 s.
				Zenith_Maths::Vector3 xVPos;
				Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(xId);
				if (xV.IsValid())
				{
					Zenith_TransformComponent* pxTransform = xV.TryGetComponent<Zenith_TransformComponent>();
					if (pxTransform != nullptr)
					{
						pxTransform->GetPosition(xVPos);
						RecordMemoryReveal(xVPos);
					}
				}
			});
	}
}
