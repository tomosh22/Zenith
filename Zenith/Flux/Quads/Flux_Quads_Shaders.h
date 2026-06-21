#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Quads render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_QuadsShaders
{
	inline constexpr Flux_ShaderDecl xQuads{ "Quads", "Quads/Flux_Quads", "vsMain", "fsMain", nullptr, "spirv_1_3", "Quads" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xQuads,
	};
}
