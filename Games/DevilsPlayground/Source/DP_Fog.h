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
	// Raw cell age in seconds at a world position, or a negative value when
	// the cell was never revealed. Pair with MemoryVisibilityForAge (below)
	// for the continuous curve — the minimap does exactly that with its
	// OnStart-cached tuning thresholds.
	float GetMemoryAgeAt(Vec3 xPosition);
	uint32_t GetMemoryRevealCount();
	void ClearAllMemoryReveals();

	// GPU/minimap mirror of the memory table. The sparse 1 m cell map is
	// rasterized into a dense byte grid (1 texel = 1 cell) that DPFogPass
	// uploads as an R8 texture each frame.

	// Pure per-cell mapping from age to rendered visibility, 0..255
	// (255 = remembered-visible / clear, 0 = forgotten or never seen).
	// Piecewise: [0, visible_s] -> 255; (visible_s, dim_s] -> lerp down to
	// fDimVisibility01; (dim_s, dim_s + fade_s] -> lerp to 0; beyond -> 0.
	// Negative age (no entry) -> 0. Continuous at every threshold so cells
	// dim smoothly instead of popping between states.
	uint8_t MemoryVisibilityForAge(float fAgeSeconds, float fVisibleS, float fDimS,
		float fDimVisibility01, float fHiddenFadeS);

	// Grid window a rasterize call fills: texel (u, v) covers the 1 m cell
	// (m_iOriginCellX + u, m_iOriginCellZ + v).
	struct MemoryGrid
	{
		int32_t  m_iOriginCellX = 0;
		int32_t  m_iOriginCellZ = 0;
		uint32_t m_uSize        = 0; // texels per side (square grid)
	};

	// Zeroes pOut (m_uSize * m_uSize bytes) then splats every memory cell
	// inside the window using MemoryVisibilityForAge with the Tuning.json
	// thresholds. Cells outside the window are skipped. Returns the number
	// of cells written.
	uint32_t RasterizeMemoryVisibility(uint8_t* pOut, const MemoryGrid& xGrid);

	// Cell-coordinate bounds of every revealed cell. Returns false when the
	// table is empty (or no fog-pass component is loaded).
	bool ComputeMemoryCellBounds(int32_t& iMinXOut, int32_t& iMinZOut,
		int32_t& iMaxXOut, int32_t& iMaxZOut);

	// Per-state cell counts (telemetry / test observability).
	void GetMemoryStateCounts(uint32_t& uVisibleOut, uint32_t& uDimOut,
		uint32_t& uHiddenOut);

	// Cross-behaviour forwarder: for every villager in the active scene,
	// register a fog hole of fRadius at the villager and record a memory
	// reveal at its position. Moved here from DPFogPass_Component::OnUpdate so
	// the fog-pass header no longer includes DPVillager_Component.h
	// (cross-component rule).
	void RegisterAllVillagerFogHoles(float fRadius);
}
