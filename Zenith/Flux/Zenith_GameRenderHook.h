#pragma once

/*
 * Zenith_GameRenderHook
 *
 * Game-side extension point into Flux::SetupRenderGraph(). Games (e.g.
 * DevilsPlayground) can register a callback that is invoked at a specific
 * position inside SetupRenderGraph — immediately after g_xEngine.Fog().SetupRenderGraph
 * and before Flux_SDFs::SetupRenderGraph — to insert their own passes into the
 * render graph (typically a custom fog/atmosphere pass that replaces the
 * engine fog system).
 *
 * The hook position is part of the contract: moving the call site (e.g. to
 * the end of SetupRenderGraph) breaks pass-ordering assumptions made by
 * games that need to write HDR scene before HDR/post-process passes.
 *
 * Registration is idempotent — registering the same function twice is a
 * no-op. This is required because Editor Stop/Play and explicit
 * RequestGraphRebuild() can re-invoke SetupRenderGraph; without idempotence
 * we'd duplicate game passes each cycle.
 */

class Flux_RenderGraph;

namespace Zenith_GameRenderHook
{
	using PostFogPassRegistrationFn = void (*)(Flux_RenderGraph& xGraph);

	// Idempotent registration. Safe to call multiple times with the same
	// function pointer — only the first call inserts into the registry.
	void RegisterPostFogPass(PostFogPassRegistrationFn pfn);

	// Remove a previously registered callback. Provided for explicit
	// teardown on game shutdown so a follow-on project does not inherit
	// the previous game's registrations.
	void UnregisterPostFogPass(PostFogPassRegistrationFn pfn);

	// Drop every registration. Used by engine shutdown / test resets.
	void ResetAllRegistrations();

	// Engine-internal: called from Flux::SetupRenderGraph at the contracted
	// position to fire every registered callback in registration order.
	void InvokePostFogRegistrations(Flux_RenderGraph& xGraph);
}
