#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the SSGI render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_SSGIShaders
{
	inline constexpr Flux_ShaderDecl xSSGI_Upsample{ "SSGI_Upsample", "SSGI/Flux_SSGI_Upsample", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSGI" };
	inline constexpr Flux_ShaderDecl xSSGI_DenoiseH{ "SSGI_DenoiseH", "SSGI/Flux_SSGI_DenoiseH", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSGI" };
	inline constexpr Flux_ShaderDecl xSSGI_DenoiseV{ "SSGI_DenoiseV", "SSGI/Flux_SSGI_DenoiseV", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSGI" };
	inline constexpr Flux_ShaderDecl xSSGI_RayMarch{ "SSGI_RayMarch", "SSGI/Flux_SSGI_RayMarch", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSGI" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xSSGI_Upsample,
		&xSSGI_DenoiseH,
		&xSSGI_DenoiseV,
		&xSSGI_RayMarch,
	};
}
