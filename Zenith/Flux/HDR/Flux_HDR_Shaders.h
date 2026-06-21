#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the HDR render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_HDRShaders
{
	inline constexpr Flux_ShaderDecl xBloomThreshold{ "BloomThreshold", "HDR/Flux_BloomThreshold", "vsMain", "fsMain", nullptr, "spirv_1_3", "HDR" };
	inline constexpr Flux_ShaderDecl xBloomDownsample{ "BloomDownsample", "HDR/Flux_BloomDownsample", "vsMain", "fsMain", nullptr, "spirv_1_3", "HDR" };
	inline constexpr Flux_ShaderDecl xBloomUpsample{ "BloomUpsample", "HDR/Flux_BloomUpsample", "vsMain", "fsMain", nullptr, "spirv_1_3", "HDR" };
	inline constexpr Flux_ShaderDecl xHDR_Luminance{ "HDR_Luminance", "HDR/Flux_Luminance", nullptr, nullptr, "csMain", "spirv_1_3", "HDR" };
	inline constexpr Flux_ShaderDecl xHDR_Adaptation{ "HDR_Adaptation", "HDR/Flux_Adaptation", nullptr, nullptr, "csMain", "spirv_1_3", "HDR" };
	inline constexpr Flux_ShaderDecl xHDR_ToneMapping{ "HDR_ToneMapping", "HDR/Flux_ToneMapping", "vsMain", "fsMain", nullptr, "spirv_1_3", "HDR" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xBloomThreshold,
		&xBloomDownsample,
		&xBloomUpsample,
		&xHDR_Luminance,
		&xHDR_Adaptation,
		&xHDR_ToneMapping,
	};
}
