#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Skybox render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_SkyboxShaders
{
	inline constexpr Flux_ShaderDecl xSkyboxSolidColour{ "SkyboxSolidColour", "Skybox/Flux_SkyboxSolidColour", "vsMain", "fsMain", nullptr, "spirv_1_3", "Skybox" };
	inline constexpr Flux_ShaderDecl xSkyboxCubemap{ "SkyboxCubemap", "Skybox/Flux_Skybox", "vsMain", "fsMain", nullptr, "spirv_1_3", "Skybox" };
	inline constexpr Flux_ShaderDecl xSkyboxAtmosphere{ "SkyboxAtmosphere", "Skybox/Flux_Atmosphere", "vsMain", "fsMain", nullptr, "spirv_1_3", "Skybox" };
	inline constexpr Flux_ShaderDecl xSkyboxTransmittanceLUT{ "SkyboxTransmittanceLUT", "Skybox/Flux_TransmittanceLUT", "vsMain", "fsMain", nullptr, "spirv_1_3", "Skybox" };
	inline constexpr Flux_ShaderDecl xSkyboxSkyViewLUT{ "SkyboxSkyViewLUT", "Skybox/Flux_SkyViewLUT", "vsMain", "fsMain", nullptr, "spirv_1_3", "Skybox" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xSkyboxSolidColour,
		&xSkyboxCubemap,
		&xSkyboxAtmosphere,
		&xSkyboxTransmittanceLUT,
		&xSkyboxSkyViewLUT,
	};
}
