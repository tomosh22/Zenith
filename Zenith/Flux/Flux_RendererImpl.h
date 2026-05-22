#pragma once

#include "Core/ZenithConfig.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h"            // Flux_CommandListEntry
#include "Flux/Flux_PerFrame.h"   // OnFrameBeginFunc / OnFrameEndFunc

class Flux_RenderGraph;

// Phase 6a-1: per-Engine state for the Flux renderer.
// Replaces the file-static state previously held on:
//   - the `Flux` static-facade class (frame counter, render graph pointer,
//     pending command-list queue, resolution-change callback list, graph
//     rebuild flag)
//   - `Flux_PerFrame` (frame counter -- consolidated with Flux's counter --
//     plus the begin/end per-frame callback arrays).
//
// Per the refactor plan, the two frame counters were redundant: Flux's was
// initialised to zero and never incremented, while Flux_PerFrame's was the
// "real" one. They fold into one m_uFrameCounter here so Flux::GetFrameCounter
// and Flux_PerFrame::GetFrameCounter return the same value.
class Flux_RendererImpl
{
public:
	Flux_RendererImpl() = default;
	~Flux_RendererImpl() = default;

	Flux_RendererImpl(const Flux_RendererImpl&) = delete;
	Flux_RendererImpl& operator=(const Flux_RendererImpl&) = delete;

	// Render graph. Allocated in Flux::LateInitialise, freed in Flux::Shutdown.
	Flux_RenderGraph*                     m_pxRenderGraph = nullptr;

	// Pending command-list queue (filled by Phase 2 of graph execution,
	// drained by the platform layer).
	Zenith_Vector<Flux_CommandListEntry>  m_xPendingCommandLists;

	// Resolution-change callback list (subsystems register here at init).
	Zenith_Vector<void(*)()>              m_xResChangeCallbacks;

	// Graph rebuild request flag -- consumed by next Compile().
	bool                                  m_bGraphRebuildRequested = false;

	// Consolidated monotonic frame counter. Was two separate statics
	// (Flux::s_uFrameCounter -- never incremented, returned zero -- and
	// Flux_PerFrame::s_uFrameCounter). Single source of truth now;
	// Flux_PerFrame::AdvanceCounter increments it.
	u_int                                 m_uFrameCounter = 0;

	// Per-frame begin/end callback arrays (max FLUX_MAX_PERFRAME_CALLBACKS
	// = 4 entries per side). Populated by backend Initialise().
	Flux_PerFrame::OnFrameBeginFunc       m_apfnBeginCallbacks[FLUX_MAX_PERFRAME_CALLBACKS] = {};
	void*                                 m_apBeginUserData   [FLUX_MAX_PERFRAME_CALLBACKS] = {};
	u_int                                 m_uNumBeginCallbacks = 0;

	Flux_PerFrame::OnFrameEndFunc         m_apfnEndCallbacks  [FLUX_MAX_PERFRAME_CALLBACKS] = {};
	void*                                 m_apEndUserData     [FLUX_MAX_PERFRAME_CALLBACKS] = {};
	u_int                                 m_uNumEndCallbacks   = 0;
};
