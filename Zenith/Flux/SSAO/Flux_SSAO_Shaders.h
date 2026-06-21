#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the SSAO render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_SSAOShaders
{
	inline constexpr Flux_ShaderDecl xSSAO_Blur{ "SSAO_Blur", "SSAO/Flux_SSAO_Blur", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSAO" };
	inline constexpr Flux_ShaderDecl xSSAO_Main{ "SSAO_Main", "SSAO/Flux_SSAO", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSAO" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xSSAO_Blur,
		&xSSAO_Main,
	};
}
