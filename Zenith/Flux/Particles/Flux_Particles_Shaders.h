#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Particles render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_ParticlesShaders
{
	inline constexpr Flux_ShaderDecl xParticles{ "Particles", "Particles/Flux_Particles", "vsMain", "fsMain", nullptr, "spirv_1_3", "Particles" };
	inline constexpr Flux_ShaderDecl xParticleUpdate{ "ParticleUpdate", "Particles/Flux_ParticleUpdate", nullptr, nullptr, "csMain", "spirv_1_3", "Particles" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xParticles,
		&xParticleUpdate,
	};
}
