#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the InstancedMeshes render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
namespace Flux_InstancedMeshesShaders
{
	inline constexpr Flux_ShaderDecl xInstancedMesh_ToGBuffer{ "InstancedMesh_ToGBuffer", "InstancedMeshes/Flux_InstancedMesh_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "InstancedMeshes" };
	inline constexpr Flux_ShaderDecl xInstancedMesh_ToShadowmap{ "InstancedMesh_ToShadowmap", "InstancedMeshes/Flux_InstancedMesh_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "InstancedMeshes" };
	inline constexpr Flux_ShaderDecl xInstanceCulling{ "InstanceCulling", "InstancedMeshes/Flux_InstanceCulling", nullptr, nullptr, "csMain", "spirv_1_3", "InstancedMeshes" };
	inline constexpr Flux_ShaderDecl xInstanceReset{ "InstanceReset", "InstancedMeshes/Flux_InstanceReset", nullptr, nullptr, "csMain", "spirv_1_3", "InstancedMeshes" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xInstancedMesh_ToGBuffer,
		&xInstancedMesh_ToShadowmap,
		&xInstanceCulling,
		&xInstanceReset,
	};
}
