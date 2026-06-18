#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "D3D12/Zenith_D3D12.h"
#include "D3D12/Zenith_D3D12_CommandBuffer.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_WorkDistribution.h"

// Out-of-line so the body can pull the full render-graph / pass / entry types,
// which the seam-reachable Zenith_D3D12.h header is kept clear of. See the
// declaration's comment in Zenith_D3D12.h.
void Zenith_D3D12::RecordFrame(const Flux_WorkDistribution& xWorkDistribution)
{
	// The null backend records nothing real, but it MUST still run every queued
	// pass's record callback: callback side effects (buffer uploads, ECS reads,
	// CPU draw-list construction) are what the engine + games rely on each frame,
	// and they used to run via the (now removed) Flux_CommandList recording stage.
	// The CB_HumanSession-on-null-backend proof exercises exactly this path.
	//
	// A single no-op command buffer suffices — its recorder methods are no-ops, so
	// there is no GPU cost and no need for the worker distribution / parallelism
	// the real backend uses. Passes are recorded in topological order (the queue's
	// order), matching the Vulkan backend's per-worker contiguous slices.
	(void)xWorkDistribution;
	Zenith_D3D12_CommandBuffer xNoOpCmdBuf;
	Zenith_Vector<Flux_RenderPassEntry>& xPending = g_xEngine.FluxRenderer().GetPendingRenderPasses();
	for (u_int i = 0; i < xPending.GetSize(); i++)
	{
		const Flux_RenderPassEntry& xEntry = xPending.Get(i);
		Flux_RenderGraph::RecordPassInto(xEntry.m_pxPass, xEntry.m_pxGraph, &xNoOpCmdBuf);
	}
}
