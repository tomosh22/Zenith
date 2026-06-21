#pragma once

#include "Flux/Slang/Flux_ShaderDecl.h"

// ---------------------------------------------------------------------------
// Engine shader programs that NO engine render-feature owns/rebuilds, but which
// the engine still compiles. Kept here (rather than a feature's _Shaders.h) so
// they participate in the catalog without implying feature ownership. The
// catalog lists these in apxUnownedEnginePrograms; ValidateFeatureParity treats
// them as explicitly unowned.
//
//  - DevilsPlayground_DPFog : a GAME program, declared here interim so
//        DPFogPass.cpp can reference its decl; migrates to a per-game manifest in
//        W2, after which apxUnownedEnginePrograms is empty.
// (ComputeTest / ComputeTest_Display were vestigial and were deleted in W1.4.)
// ---------------------------------------------------------------------------
namespace Flux_UnownedEngineShaders
{
	inline constexpr Flux_ShaderDecl xDevilsPlayground_DPFog{ "DevilsPlayground_DPFog", "Fog/DP_Fog", "vsMain", "fsMain", nullptr, "spirv_1_3", "Fog" };

	inline constexpr const Flux_ShaderDecl* apxALL[] =
	{
		&xDevilsPlayground_DPFog,
	};
}
