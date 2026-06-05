#pragma once

#include <cmath>

// ============================================================================
// CB_TerrainGen — the single shared terrain-height function. Both the offline
// heightmap bake (CityBuilder.cpp → the 4096px R32_SFLOAT map → GPU chunk mesh)
// and the runtime CPU heightfield (CB_TerrainHeightfield, what roads/buildings
// query via GetHeightAt) sample THIS function, so the rendered ground and the
// gameplay height queries agree.
//
// It is SYMMETRIC about the terrain centre (2048,2048) in both axes — every term
// is even in (x-2048) and (z-2048) — so the exporter's vertical flip can't make
// the baked mesh disagree with the heightfield. Heights are normalized; the bake
// (× MAX_TERRAIN_HEIGHT = 512) and the heightfield (× heightScale = 512) both
// turn them into the same metres → a gentle ~6..34m rolling terrain.
// ============================================================================
namespace CB_TerrainGen
{
	constexpr float TERRAIN_CENTRE = 2048.0f;   // half of the 4096m terrain
	constexpr float HEIGHT_SCALE   = 512.0f;    // matches the exporter's MAX_TERRAIN_HEIGHT

	inline float HillNorm(float fWorldX, float fWorldZ)
	{
		const float dx = fWorldX - TERRAIN_CENTRE;
		const float dz = fWorldZ - TERRAIN_CENTRE;
		// All terms even in dx AND dz → symmetric → flip-proof.
		float h = 0.55f
		        + 0.22f * std::cos(dx * 0.0016f) * std::cos(dz * 0.0013f)
		        + 0.13f * std::cos(dx * 0.0034f) * std::cos(dz * 0.0021f)
		        + 0.05f * (std::cos(dx * 0.0026f) + std::cos(dz * 0.0026f));
		if (h < 0.0f) { h = 0.0f; }
		if (h > 1.0f) { h = 1.0f; }
		return 0.012f + h * 0.055f;   // ~0.0175..0.067 → ~9..34m
	}

	// The world-Y the runtime CPU heightfield reads for an UN-edited (pristine) hill:
	// bilinear over the 16m sample grid of HillNorm × HEIGHT_SCALE. This reproduces
	// CB_TerrainHeightfield(257,257, cell=16, origin=0, scale=HEIGHT_SCALE)::GetHeightAt
	// exactly. The terraform GPU re-upload uses (field.GetHeightAt − this) to recover
	// just the player's edit, so it can add that delta on top of the FINE baked hill
	// (preserving 1m detail + watertight seams) instead of replacing it with the coarse field.
	inline float HillFieldSample(float fWorldX, float fWorldZ)
	{
		constexpr float fCELL = 16.0f;
		constexpr int   iMAX  = 256;   // 257 samples - 1
		float fX = fWorldX / fCELL;
		float fZ = fWorldZ / fCELL;
		fX = (fX < 0.0f) ? 0.0f : ((fX > static_cast<float>(iMAX)) ? static_cast<float>(iMAX) : fX);
		fZ = (fZ < 0.0f) ? 0.0f : ((fZ > static_cast<float>(iMAX)) ? static_cast<float>(iMAX) : fZ);
		const int iX0 = static_cast<int>(std::floor(fX));
		const int iZ0 = static_cast<int>(std::floor(fZ));
		const int iX1 = (iX0 + 1 <= iMAX) ? iX0 + 1 : iX0;
		const int iZ1 = (iZ0 + 1 <= iMAX) ? iZ0 + 1 : iZ0;
		const float fTX = fX - static_cast<float>(iX0);
		const float fTZ = fZ - static_cast<float>(iZ0);
		const float h00 = HillNorm(static_cast<float>(iX0) * fCELL, static_cast<float>(iZ0) * fCELL);
		const float h10 = HillNorm(static_cast<float>(iX1) * fCELL, static_cast<float>(iZ0) * fCELL);
		const float h01 = HillNorm(static_cast<float>(iX0) * fCELL, static_cast<float>(iZ1) * fCELL);
		const float h11 = HillNorm(static_cast<float>(iX1) * fCELL, static_cast<float>(iZ1) * fCELL);
		const float fTop = h00 + (h10 - h00) * fTX;
		const float fBot = h01 + (h11 - h01) * fTX;
		return (fTop + (fBot - fTop) * fTZ) * HEIGHT_SCALE;
	}
}
