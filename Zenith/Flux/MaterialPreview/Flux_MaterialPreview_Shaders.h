#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the MaterialPreview render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
#ifdef ZENITH_TOOLS
namespace Flux_MaterialPreviewShaders
{
	inline constexpr Flux_ShaderDecl xMaterialPreview_Background{ "MaterialPreview_Background", "MaterialPreview/Flux_MaterialPreview_Background", "vsMain", "fsMain", nullptr, "spirv_1_3", "MaterialPreview" };
	inline constexpr Flux_ShaderDecl xMaterialPreview_Tonemap{ "MaterialPreview_Tonemap", "MaterialPreview/Flux_MaterialPreview_Tonemap", "vsMain", "fsMain", nullptr, "spirv_1_3", "MaterialPreview" };
	// Forward-lit preview mesh pass — MaterialPreview's OWN copy of the translucent
	// forward program (strict single ownership; byte-for-byte fork of
	// Translucency/Flux_Translucent_Forward). See the .slang header note.
	inline constexpr Flux_ShaderDecl xMaterialPreview_Forward{ "MaterialPreview_Forward", "MaterialPreview/Flux_MaterialPreview_Forward", "vsMain", "fsMain", nullptr, "spirv_1_3", "MaterialPreview" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xMaterialPreview_Background,
		&xMaterialPreview_Tonemap,
		&xMaterialPreview_Forward,
	};
}
#endif
