#pragma once
/**
 * DPFogPass - registers a game-side fog pass via Zenith_GameRenderHook
 * (EXT-1) and disables the engine fog system via
 * Flux_Fog::SetExternallyOverridden.
 *
 * Init() is called from Project_RegisterScriptBehaviours.
 * Shutdown() is called from Project_Shutdown — guarded so it survives
 * render-graph teardown order.
 *
 * Skeleton-grade ships only the override + a no-op pass registration. The
 * fog shader (DP_Fog.slang) and CBV upload land in Wave 4 along with the
 * FluxShaderProgram codegen entries.
 */

namespace DPFogPass
{
	void Init();
	void Shutdown();
}
