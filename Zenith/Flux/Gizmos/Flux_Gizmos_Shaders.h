#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Gizmos render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
#ifdef ZENITH_TOOLS
namespace Flux_GizmosShaders
{
	inline constexpr Flux_ShaderDecl xGizmos{ "Gizmos", "Gizmos/Flux_Gizmos", "vsMain", "fsMain", nullptr, "spirv_1_3", "Gizmos" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xGizmos,
	};
}
#endif
