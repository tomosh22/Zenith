#pragma once
/**
 * DPFogPass - registers a game-side fog pass as a generic Zenith render feature
 * (Zenith_GameRenderFeatures, "DP_Fog", anchored runAfter="Fog") and disables
 * the engine fog system generically via the render graph's force-disable overlay
 * (xGraph.SetOwnerForceDisabled("Fog", ...)) — no fog-specific engine API.
 *
 * Init() is called from Project_RegisterGameComponents; it registers the
 * feature, and the registry drives its lifecycle (InitialiseDPFog / SetupDPFog /
 * ShutdownDPFog). Shutdown() is called from Project_Shutdown and unregisters the
 * feature; ShutdownDPFog is guarded so it survives render-graph teardown order.
 */

namespace DPFogPass
{
	void Init();
	void Shutdown();
}
