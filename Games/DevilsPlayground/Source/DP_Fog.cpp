#include "Zenith.h"

#include "DP_Fog.h"
#include "DPCommonTypes.h"
#include "DP_Tuning.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#include "../Components/DPFogPass_Behaviour.h"

#include <cmath>

namespace DP_Fog
{
	// 2026-05-17 scene-ownership refactor: fog-hole + memory-fog
	// tables moved onto DPFogPass_Behaviour. Forwarders below are
	// no-ops when no fog-pass script is loaded (between-scenes /
	// non-DP scenes).

	void RegisterFogHole(Zenith_EntityID xId, float fRadius)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::RegisterFogHole must be called from main thread");
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->RegisterFogHole(xId, fRadius);
	}

	void UnregisterFogHole(Zenith_EntityID xId)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::UnregisterFogHole must be called from main thread");
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->UnregisterFogHole(xId);
	}

	void ClearAllFogHoles()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::ClearAllFogHoles must be called from main thread");
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->ClearAllFogHoles();
	}

	uint32_t GetFogHoleCount()
	{
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return 0;
		return pxFog->GetFogHoleCount();
	}

	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles)
	{
		if (pxOutHoles == nullptr || uMaxHoles == 0) return 0;
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return 0;
		uint32_t uWritten = 0;
		pxFog->ForEachFogHole(
			[pxOutHoles, uMaxHoles, &uWritten]
			(Zenith_EntityID xHoleId, float fRadius)
			{
				if (uWritten >= uMaxHoles) return;
				Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xHoleId);
				if (pxScene == nullptr) return;
				Zenith_Entity xEnt = pxScene->TryGetEntity(xHoleId);
				if (!xEnt.IsValid()) return;
				if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
				Vec3 xPos;
				xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
				pxOutHoles[uWritten++] = Vec4(xPos.x, xPos.y, xPos.z, fRadius);
			});
		return uWritten;
	}

	// ========================================================================
	// MVP-2.4.5: Memory fog implementation. State machine moved onto
	// DPFogPass_Behaviour as part of the 2026-05-17 ownership refactor;
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
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->RecordMemoryRevealCell(CellKeyForPosition(xPosition));
	}

	void TickMemoryFog(float fDt)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::TickMemoryFog must be called from main thread");
		if (fDt <= 0.0f) return;
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->TickMemoryFog(fDt);
	}

	MemoryTileState GetMemoryStateAt(Vec3 xPosition)
	{
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
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
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return 0;
		return pxFog->GetMemoryRevealCount();
	}

	void ClearAllMemoryReveals()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Fog::ClearAllMemoryReveals must be called from main thread");
		DPFogPass_Behaviour* pxFog = DPFogPass_Behaviour::Instance();
		if (pxFog == nullptr) return;
		pxFog->ClearAllMemoryReveals();
	}
}
