#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Decals render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_DecalsShaders
{
	inline constexpr Flux_ShaderDecl xDecals_NormalsCopy{ "Decals_NormalsCopy", "Decals/Flux_Decals_NormalsCopy", "vsMain", "fsMain", nullptr, "spirv_1_3", "Decals" };
	inline constexpr Flux_ShaderDecl xDecals_Apply{ "Decals_Apply", "Decals/Flux_Decals_Apply", "vsMain", "fsMain", nullptr, "spirv_1_3", "Decals" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xDecals_NormalsCopy,
		&xDecals_Apply,
	};
}
