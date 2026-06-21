#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Fog render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_FogShaders
{
	inline constexpr Flux_ShaderDecl xFog_Simple{ "Fog_Simple", "Fog/Flux_Fog", "vsMain", "fsMain", nullptr, "spirv_1_3", "Fog" };
	inline constexpr Flux_ShaderDecl xFog_GodRays{ "Fog_GodRays", "Fog/Flux_GodRays", "vsMain", "fsMain", nullptr, "spirv_1_3", "Fog" };
	inline constexpr Flux_ShaderDecl xFog_FroxelApply{ "Fog_FroxelApply", "Fog/Flux_FroxelFog_Apply", "vsMain", "fsMain", nullptr, "spirv_1_3", "Fog" };
	inline constexpr Flux_ShaderDecl xFog_FroxelInject{ "Fog_FroxelInject", "Fog/Flux_FroxelFog_Inject", nullptr, nullptr, "csMain", "spirv_1_3", "Fog" };
	inline constexpr Flux_ShaderDecl xFog_FroxelLight{ "Fog_FroxelLight", "Fog/Flux_FroxelFog_Light", nullptr, nullptr, "csMain", "spirv_1_3", "Fog" };
	inline constexpr Flux_ShaderDecl xFog_Raymarch{ "Fog_Raymarch", "Fog/Flux_RaymarchFog", "vsMain", "fsMain", nullptr, "spirv_1_3", "Fog" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xFog_Simple,
		&xFog_GodRays,
		&xFog_FroxelApply,
		&xFog_FroxelInject,
		&xFog_FroxelLight,
		&xFog_Raymarch,
	};
}
