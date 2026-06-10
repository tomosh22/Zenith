#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_BackendTypes.h"   // Flux_PlatformAPI / Flux_MemoryManager full defs for the neutral per-frame calls
#include "Core/Zenith_CommandLine.h"  // IsHeadless() — the backend is never initialised in headless

void Flux_RendererImpl::PerFrameInitialise()
{
	m_uFrameCounter = 0;
}

void Flux_RendererImpl::PerFrameShutdown()
{
	// Frame counter is intentionally NOT reset — by the time Shutdown runs the
	// per-resource deferred-deletion ring has already drained, so resetting it
	// would do nothing useful. Nothing else to tear down now that the per-frame
	// begin/end work is called directly rather than through callback arrays.
}

void Flux_RendererImpl::BeginFrame()
{
	// Issue the backend's per-frame begin work (fence wait, descriptor-pool
	// reset, typed-deletion-queue drain, scratch-offset reset). Routed through
	// the neutral Flux_PlatformAPI alias so both the Vulkan and D3D12 backends
	// compile; the D3D12 null backend's PerFrameBegin is a no-op. Skipped in
	// headless, where the backend is never initialised.
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}
	g_xEngine.FluxBackend().PerFrameBegin(GetRingIndex());
}

void Flux_RendererImpl::EndFrame()
{
	ProcessFrameEnd();
	AdvanceCounter();
}

void Flux_RendererImpl::ProcessFrameEnd()
{
	// Advance the deferred-VRAM-deletion clock by one tick. Routed through the
	// neutral Flux_MemoryManager alias (the D3D12 null backend's is a no-op).
	// Skipped in headless, where the memory manager is never initialised; the
	// ring counter still advances regardless (see EndFrame / AdvanceCounter).
	if (!Zenith_CommandLine::IsHeadless())
	{
		g_xEngine.FluxMemory().ProcessDeferredDeletions();
	}
}

void Flux_RendererImpl::AdvanceCounter()
{
	m_uFrameCounter++;
}

u_int Flux_RendererImpl::GetRingIndex()
{
	return m_uFrameCounter % MAX_FRAMES_IN_FLIGHT;
}

// Note: GetFrameCounter() is defined in Flux.cpp (was Flux::GetFrameCounter
// before the collapse). Its return type signature in the header (uint32_t)
// matches that definition; this file no longer defines a duplicate.
