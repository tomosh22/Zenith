#pragma once

#include <string>

// ============================================================================
// Flux_Screenshot - programmatic swapchain-dump request.
//
// The CLI `--screenshot <path> --screenshot-frame N` dumps the swapchain at a
// fixed GLOBAL frame index, which is awkward for automated tests (the global
// index includes the variable-length boot + editor-automation prologue, so it
// does not map to a test's step-frame counter). RequestDump lets a test ask
// for a full-framebuffer TGA at the exact moment its content is in the desired
// state — the swapchain consumes one pending request per EndFrame, before
// present, using the same readback path as the CLI screenshot.
//
// Compositor-free and resolution-independent (captures the render target, not
// the on-screen window), so it is immune to window position, occlusion, and
// ImGui dock-tab focus.
// ============================================================================
namespace Flux_Screenshot
{
	// Queue a full-swapchain TGA dump to szPath, taken at the end of the
	// current (or next) rendered frame. Overwrites any still-pending request.
	void RequestDump(const char* szPath);

	// Swapchain-internal: returns true + the queued path when a dump is
	// pending, clearing the request. Returns false when none is queued.
	bool ConsumePendingDump(std::string& strPathOut);
}
