#pragma once

#ifdef ZENITH_TOOLS

#include <cstdint>

class Flux_RenderSceneSnapshot;
class Flux_PrimitivesImpl;
class Zenith_DebugVariables;

// =============================================================================
// Scene-graph debug visualisation (Phase 3, ZENITH_TOOLS only).
//
// Draws the engine-owned Flux_RenderSceneSnapshot's per-entity world AABBs as
// world-space wireframe overlays (green = camera-visible, red = frustum-culled) so
// you can SEE the spatial culling, and publishes snapshot stats (total / visible /
// culled counts) as read-only debug variables.
//
// Reads ONLY the snapshot's pointer-free diagnostics (world AABB + flags) — never
// dereferences a Flux_ModelInstance* — and gates every read on the lifetime epoch
// (IsCurrent), so it can't touch a freed instance while the snapshot is stale. All
// engine state (the Flux_PrimitivesImpl it draws through, the Zenith_DebugVariables it
// registers into) is INJECTED from the composition root, so this TU reaches no engine singleton.
// =============================================================================
namespace Zenith_SceneGraphDebug
{
	// Composition-root install (Zenith_Engine.cpp): register the debug-variable toggles +
	// stat readouts and store the injected Flux_PrimitivesImpl used for overlays.
	void Install(Zenith_DebugVariables& xDebugVars, Flux_PrimitivesImpl& xPrimitives);

	// Called once per frame AFTER the snapshot rebuild (Zenith_Core.cpp): updates the stat
	// vars and, when the overlay toggle is on, queues per-entity world-AABB wireframes via
	// the injected primitives. No-op (and stats left at 0) when the snapshot is stale.
	void QueueOverlays(const Flux_RenderSceneSnapshot& xSnapshot, uint64_t uCurrentEpoch);
}

#endif // ZENITH_TOOLS
