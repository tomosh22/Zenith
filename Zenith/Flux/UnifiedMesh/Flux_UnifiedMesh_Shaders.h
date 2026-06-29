#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the UnifiedMesh render feature (unified GPU-driven
// opaque-mesh pipeline). Pure data: each program is declared next to its feature and
// listed in apxALL, which Flux_FeatureRegistry/Flux_ShaderCatalog use for compile,
// parity and hot-reload. Each decl's m_szSubsystem ("UnifiedMesh") groups the
// generated reflection header (Flux/Shaders/Generated/UnifiedMesh.h).
namespace Flux_UnifiedMeshShaders
{
	inline constexpr Flux_ShaderDecl xUnifiedMesh_ToGBuffer{ "UnifiedMesh_ToGBuffer", "UnifiedMesh/Flux_UnifiedMesh_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "UnifiedMesh" };
	inline constexpr Flux_ShaderDecl xUnifiedMesh_ToShadowmap{ "UnifiedMesh_ToShadowmap", "UnifiedMesh/Flux_UnifiedMesh_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "UnifiedMesh" };
	inline constexpr Flux_ShaderDecl xUnifiedMesh_Culling{ "UnifiedMesh_Culling", "UnifiedMesh/Flux_UnifiedMesh_Culling", nullptr, nullptr, "csMain", "spirv_1_3", "UnifiedMesh" };
	inline constexpr Flux_ShaderDecl xUnifiedMesh_Reset{ "UnifiedMesh_Reset", "UnifiedMesh/Flux_UnifiedMesh_Reset", nullptr, nullptr, "csMain", "spirv_1_3", "UnifiedMesh" };
	inline constexpr Flux_ShaderDecl xUnifiedMesh_Skinning{ "UnifiedMesh_Skinning", "UnifiedMesh/Flux_UnifiedMesh_Skinning", nullptr, nullptr, "csMain", "spirv_1_3", "UnifiedMesh" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xUnifiedMesh_ToGBuffer,
		&xUnifiedMesh_ToShadowmap,
		&xUnifiedMesh_Culling,
		&xUnifiedMesh_Reset,
		&xUnifiedMesh_Skinning,
	};
}
