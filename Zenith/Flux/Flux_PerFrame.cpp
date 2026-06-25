#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"   // Flux_GraphicsImpl full def for BindlessAllocator().AdvanceFrame()
#include "Flux/Flux_BackendTypes.h"   // Flux_PlatformAPI / Flux_MemoryManager full defs for the neutral per-frame calls
#include "Core/Zenith_CommandLine.h"  // IsHeadless() — the backend is never initialised in headless

void Flux_RendererImpl::PerFrameInitialise()
{
	// Nothing to set up — the frame index lives on FrameContext
	// (g_xEngine.Frame()), freshly zeroed when Zenith_Engine::Initialise
	// allocates it, and advanced only by Zenith_MainLoop.
}

void Flux_RendererImpl::PerFrameShutdown()
{
	// Nothing to tear down — the per-frame begin/end work is called directly
	// rather than through callback arrays, and the frame index lives on
	// FrameContext (not this subsystem's to reset).
}

void Flux_RendererImpl::BeginFrame()
{
	// Default to "no render work this frame". RecordFrame (driven from
	// Flux_RenderGraph::Execute) sets this true when it records a non-empty
	// queue; if Execute does not run this frame (headless / scene transition /
	// !bSubmitRenderWork) the flag stays false so the backend's EndFrame skips
	// the render submit instead of resubmitting last frame's command buffers.
	m_bHasRenderWork = false;

	// Issue the backend's per-frame begin work (fence wait, descriptor-pool
	// reset, typed-deletion-queue drain, scratch-offset reset). Routed through
	// the neutral Flux_PlatformAPI alias so both the Vulkan and D3D12 backends
	// compile; the D3D12 null backend's PerFrameBegin is a no-op. Skipped in
	// headless, where the backend is never initialised.
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}
	g_xEngine.FluxBackend().PerFrameBegin(g_xEngine.Frame().GetRingIndex());
}

void Flux_RendererImpl::ProcessFrameEnd()
{
	// Advance the deferred-VRAM-deletion clock by one tick. Routed through the
	// neutral Flux_MemoryManager alias (the D3D12 null backend's is a no-op).
	// Skipped in headless, where the memory manager is never initialised.
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxMemory().ProcessDeferredDeletions();
		// Advance the bindless-slot deferred-free clock alongside the VRAM one, so a
		// freed bindless index is recycled only after MAX_FRAMES_IN_FLIGHT+1 frames.
		g_xEngine.FluxGraphics().BindlessAllocator().AdvanceFrame();
	}
}

// Note: the monotonic frame counter that used to live here (and the
// EndFrame/AdvanceCounter/GetRingIndex trio around it) moved to FrameContext —
// g_xEngine.Frame() owns the single frame-index variable engine-wide and
// Zenith_MainLoop advances it after ProcessFrameEnd, so a skipped frame
// (swapchain out-of-date) can tick the deletion clock without moving the ring.
