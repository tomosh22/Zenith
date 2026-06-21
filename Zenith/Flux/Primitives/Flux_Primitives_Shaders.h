#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Primitives render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_PrimitivesShaders
{
	inline constexpr Flux_ShaderDecl xPrimitives{ "Primitives", "Primitives/Flux_Primitives", "vsMain", "fsMain", nullptr, "spirv_1_3", "Primitives" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xPrimitives,
	};
}
