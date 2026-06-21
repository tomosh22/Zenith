#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the AnimatedMeshes render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_AnimatedMeshesShaders
{
	inline constexpr Flux_ShaderDecl xAnimatedMesh_ToGBuffer{ "AnimatedMesh_ToGBuffer", "AnimatedMeshes/Flux_AnimatedMesh_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "AnimatedMeshes" };
	inline constexpr Flux_ShaderDecl xAnimatedMesh_ToShadowmap{ "AnimatedMesh_ToShadowmap", "AnimatedMeshes/Flux_AnimatedMesh_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "AnimatedMeshes" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xAnimatedMesh_ToGBuffer,
		&xAnimatedMesh_ToShadowmap,
	};
}
