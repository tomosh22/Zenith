#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Terrain render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_TerrainShaders
{
	inline constexpr Flux_ShaderDecl xTerrain_ToGBuffer{ "Terrain_ToGBuffer", "Terrain/Flux_Terrain_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "Terrain" };
	inline constexpr Flux_ShaderDecl xTerrain_ToShadowmap{ "Terrain_ToShadowmap", "Terrain/Flux_Terrain_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "Terrain" };
	inline constexpr Flux_ShaderDecl xTerrainCulling{ "TerrainCulling", "Terrain/Flux_TerrainCulling", nullptr, nullptr, "csMain", "spirv_1_3", "Terrain" };
	inline constexpr Flux_ShaderDecl xTerrainResetCounters{ "TerrainResetCounters", "Terrain/Flux_TerrainResetCounters", nullptr, nullptr, "csMain", "spirv_1_3", "Terrain" };
	inline constexpr Flux_ShaderDecl xWater{ "Water", "Water/Flux_Water", "vsMain", "fsMain", nullptr, "spirv_1_3", "Water" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xTerrain_ToGBuffer,
		&xTerrain_ToShadowmap,
		&xTerrainCulling,
		&xTerrainResetCounters,
		&xWater,
	};
}
