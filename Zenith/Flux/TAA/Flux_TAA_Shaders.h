#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the TAA (temporal anti-aliasing) render feature. Pure
// data: each program is declared next to its feature and listed in apxALL, which
// Flux_FeatureRegistry / Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem ("TAA") controls only the generated-header grouping
// (Flux/Shaders/Generated/TAA.h).
//
// Stage 4.2: the resolve (real neighbourhood-clamped temporal resolve), the
// CopyToHistory (persist resolved output into the feature-owned history) and the
// Sharpen (RCAS post-resolve) compute programs. Adding a program decl needs
// FluxCompiler.exe REBUILT (the catalog is compiled in) then re-run to emit the .spv.
namespace Flux_TAAShaders
{
	inline constexpr Flux_ShaderDecl xTAA_Resolve      { "TAA_Resolve",       "TAA/Flux_TAA_Resolve",       nullptr, nullptr, "csMain", "spirv_1_3", "TAA" };
	inline constexpr Flux_ShaderDecl xTAA_CopyToHistory{ "TAA_CopyToHistory", "TAA/Flux_TAA_CopyToHistory", nullptr, nullptr, "csMain", "spirv_1_3", "TAA" };
	inline constexpr Flux_ShaderDecl xTAA_Sharpen      { "TAA_Sharpen",       "TAA/Flux_TAA_Sharpen",       nullptr, nullptr, "csMain", "spirv_1_3", "TAA" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xTAA_Resolve,
		&xTAA_CopyToHistory,
		&xTAA_Sharpen,
	};
}
