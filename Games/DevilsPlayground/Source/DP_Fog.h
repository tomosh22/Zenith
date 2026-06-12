#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DPCommonTypes.h"

#include <cstdint>

// ============================================================================
// DP_Fog — published by B6. Clear-and-rebuild strategy each frame.
// ============================================================================
namespace DP_Fog
{
	void RegisterFogHole(Zenith_EntityID xId, float fRadius);
	void UnregisterFogHole(Zenith_EntityID xId);
	void ClearAllFogHoles();
	uint32_t GetFogHoleCount();

	// Render-side accessor — populates pxOutHoles (xyz=worldPos, w=radius)
	// with up to uMaxHoles entries. Returns the number actually written.
	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles);

	// MVP-2.4.5: Memory fog states. Per Tuning.json `_comment_memory_states`.
	enum class MemoryTileState : uint8_t
	{
		NeverSeen = 0,
		VisitedVisible,
		VisitedDim,
		VisitedHidden
	};

	void RecordMemoryReveal(Vec3 xPosition);
	void TickMemoryFog(float fDt);
	MemoryTileState GetMemoryStateAt(Vec3 xPosition);
	uint32_t GetMemoryRevealCount();
	void ClearAllMemoryReveals();

	// Cross-behaviour forwarder: for every villager in the active scene,
	// register a fog hole of fRadius at the villager and record a memory
	// reveal at its position. Moved here from DPFogPass_Component::OnUpdate so
	// the fog-pass header no longer includes DPVillager_Component.h
	// (cross-component rule).
	void RegisterAllVillagerFogHoles(float fRadius);
}
