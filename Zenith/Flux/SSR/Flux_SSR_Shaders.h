#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the SSR render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_SSRShaders
{
	inline constexpr Flux_ShaderDecl xSSR_RayMarch{ "SSR_RayMarch", "SSR/Flux_SSR_RayMarch", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSR" };
	inline constexpr Flux_ShaderDecl xSSR_DenoiseH{ "SSR_DenoiseH", "SSR/Flux_SSR_DenoiseH", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSR" };
	inline constexpr Flux_ShaderDecl xSSR_DenoiseV{ "SSR_DenoiseV", "SSR/Flux_SSR_DenoiseV", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSR" };
	inline constexpr Flux_ShaderDecl xSSR_Upsample{ "SSR_Upsample", "SSR/Flux_SSR_Upsample", "vsMain", "fsMain", nullptr, "spirv_1_3", "SSR" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xSSR_RayMarch,
		&xSSR_DenoiseH,
		&xSSR_DenoiseV,
		&xSSR_Upsample,
	};
}
