#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the StaticMeshes render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_StaticMeshesShaders
{
	inline constexpr Flux_ShaderDecl xStaticMesh_ToGBuffer{ "StaticMesh_ToGBuffer", "StaticMeshes/Flux_StaticMesh_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "StaticMeshes" };
	inline constexpr Flux_ShaderDecl xStaticMesh_ToShadowmap{ "StaticMesh_ToShadowmap", "StaticMeshes/Flux_StaticMesh_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "StaticMeshes" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xStaticMesh_ToGBuffer,
		&xStaticMesh_ToShadowmap,
	};
}
