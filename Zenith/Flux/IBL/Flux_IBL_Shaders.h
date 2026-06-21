#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the IBL render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_IBLShaders
{
	inline constexpr Flux_ShaderDecl xIBL_BRDFIntegration{ "IBL_BRDFIntegration", "IBL/Flux_BRDFIntegration", "vsMain", "fsMain", nullptr, "spirv_1_3", "IBL" };
	inline constexpr Flux_ShaderDecl xIBL_IrradianceConvolution{ "IBL_IrradianceConvolution", "IBL/Flux_IrradianceConvolution", "vsMain", "fsMain", nullptr, "spirv_1_3", "IBL" };
	inline constexpr Flux_ShaderDecl xIBL_PrefilterEnvMap{ "IBL_PrefilterEnvMap", "IBL/Flux_PrefilterEnvMap", "vsMain", "fsMain", nullptr, "spirv_1_3", "IBL" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xIBL_BRDFIntegration,
		&xIBL_IrradianceConvolution,
		&xIBL_PrefilterEnvMap,
	};
}
