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

	// Rasterizes DP_Fog's memory-cell table into the pass's R8 memory
	// texture and stages the upload (Flux_MemoryManager::UpdateTextureVRAM,
	// drained ahead of render work). Main-thread, once per frame — driven
	// from DPFogPass_Component::OnUpdate after the frame's reveals landed.
	// No-op when the texture isn't created (headless) or nothing has been
	// revealed yet (the shader's memory term stays disabled).
	void UpdateMemoryTexture();

	// Zeroes the shader's memory window. Called from the fog component's
	// OnDestroy: the DP_Fog render pass is process-global and records in
	// EVERY scene, but only ProcLevel carries the component that refreshes
	// the window — without this reset the previous run's window + texture
	// would keep rendering ghost reveal patches after the scene unloads
	// (one-frame flash at the next run's start; persistent in editor Stop).
	void ResetMemoryWindow();
}
