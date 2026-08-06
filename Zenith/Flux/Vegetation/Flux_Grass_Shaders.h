#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// Shader programs owned by the Grass render feature. Pure data: this header
// declares each program next to its feature and lists them in apxALL, which
// Flux_FeatureRegistry/Flux_ShaderCatalog use for compile, parity and hot-reload.
// Each decl's m_szSubsystem controls only the generated-header grouping.
//
// The four compute programs run in the order they are listed: reset seeds the
// indirect args, placement fills the blade pool and the per-partition visible
// lists, indirect-fixup clamps the instance counts a saturated frame overshot,
// and displacement maintains the mover push field the placement pass samples.
// The shared blade record, index/vertex tables and partition contract live in
// Shaders/Vegetation/Flux_GrassCommon.slang, which every program includes.
namespace Flux_GrassShaders
{
	inline constexpr Flux_ShaderDecl xGrass_Reset{ "Grass_Reset", "Vegetation/Flux_Grass_Reset", nullptr, nullptr, "csMain", "spirv_1_3", "Vegetation" };
	inline constexpr Flux_ShaderDecl xGrass_Placement{ "Grass_Placement", "Vegetation/Flux_Grass_Placement", nullptr, nullptr, "csMain", "spirv_1_3", "Vegetation" };
	inline constexpr Flux_ShaderDecl xGrass_IndirectFixup{ "Grass_IndirectFixup", "Vegetation/Flux_Grass_IndirectFixup", nullptr, nullptr, "csMain", "spirv_1_3", "Vegetation" };
	inline constexpr Flux_ShaderDecl xGrass_Displacement{ "Grass_Displacement", "Vegetation/Flux_Grass_Displacement", nullptr, nullptr, "csMain", "spirv_1_3", "Vegetation" };
	inline constexpr Flux_ShaderDecl xGrass_ToGBuffer{ "Grass_ToGBuffer", "Vegetation/Flux_Grass_ToGBuffer", "vsMain", "fsMain", nullptr, "spirv_1_3", "Vegetation" };
	// Velocity variant of Grass_ToGBuffer — writes MRT_INDEX_VELOCITY (5-MRT framebuffer)
	// when the velocity latch is on. Blades sway every frame, so its vertex stage rebuilds
	// the pose against the PREVIOUS frame's wind rather than reprojecting a static position.
	inline constexpr Flux_ShaderDecl xGrass_ToGBufferVelocity{ "Grass_ToGBufferVelocity", "Vegetation/Flux_Grass_ToGBufferVelocity", "vsMain", "fsMain", nullptr, "spirv_1_3", "Vegetation" };
	inline constexpr Flux_ShaderDecl xGrass_ToShadowmap{ "Grass_ToShadowmap", "Vegetation/Flux_Grass_ToShadowmap", "vsMain", "fsMain", nullptr, "spirv_1_3", "Vegetation" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xGrass_Reset,
		&xGrass_Placement,
		&xGrass_IndirectFixup,
		&xGrass_Displacement,
		&xGrass_ToGBuffer,
		&xGrass_ToGBufferVelocity,
		&xGrass_ToShadowmap,
	};
}
