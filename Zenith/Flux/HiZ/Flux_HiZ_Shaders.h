#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the HiZ render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_HiZShaders
{
	inline constexpr Flux_ShaderDecl xHiZ_Generate{ "HiZ_Generate", "HiZ/Flux_HiZ_Generate", nullptr, nullptr, "csMain", "spirv_1_3", "HiZ" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xHiZ_Generate,
	};
}
