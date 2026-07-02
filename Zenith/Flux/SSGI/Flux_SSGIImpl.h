#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_ScreenSpaceEffectBase.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

class Flux_RenderGraph;

enum SSGI_DebugMode : u_int
{
	SSGI_DEBUG_NONE = 0,
	SSGI_DEBUG_RAY_DIRECTIONS,
	SSGI_DEBUG_HIT_POSITIONS,
	SSGI_DEBUG_CONFIDENCE,
	SSGI_DEBUG_FINAL_RESULT,
	SSGI_DEBUG_COUNT
};

// Composite selection for SSGI's committed-handle protocol. Unlike SSR (whose
// rebuild trigger is a single bool toggle), SSGI must rebuild the graph when
// EITHER the denoise toggle OR the raymarch resolution divisor changes — the
// divisor resizes the raymarch transient. Packing both into one POD lets the
// generic Flux_CommittedHandleSelector cover both triggers in one comparison.
struct Flux_SSGISelection
{
	bool  m_bDenoise;
	u_int m_uDivisor;
	bool operator==(const Flux_SSGISelection&) const = default;
};

// Phase 9: state + behaviour for SSGI subsystem.
//
// S5b: the whole SSGI chain (transients + committed-handle selector) is
// per-render-view — slot 0 (main) at swapchain dims as before; the preview
// view at 512² when active. uViewSlot defaults keep single-view callers
// unchanged.
class Flux_SSGIImpl : public Flux_ScreenSpaceEffectBase<Flux_SSGIImpl>
{
public:
	Flux_SSGIImpl() = default;
	~Flux_SSGIImpl() = default;

	Flux_SSGIImpl(const Flux_SSGIImpl&) = delete;
	Flux_SSGIImpl& operator=(const Flux_SSGIImpl&) = delete;

	void Initialise();
	void BuildPipelines();

	// CRTP hook called by Flux_ScreenSpaceEffectBase::Shutdown(). SSGI owns no
	// CBV, so this is a no-op — it still must exist for the static_cast call.
	void ShutdownImpl();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void ApplyDenoiseSelectionToGraph(Flux_RenderGraph& xGraph);

	// Promoted from file-static free functions; cross-subsystem deps are reached
	// via g_xEngine at point of use.
	// Public because the static graph-execute trampolines call them.
	// The iteration bump scales with the VIEW's base width (slot 0 = swapchain,
	// preview = kuFLUX_PREVIEW_VIEW_SIZE) captured at setup.
	u_int ComputeEffectiveBinarySearchIterations(u_int uViewSlot = kuFluxViewSlotMain) const;
	void UpdateSSGIConstants();

	Flux_TransientHandle GetSSGIHandle(u_int uViewSlot = kuFluxViewSlotMain) const { return m_axSSGISelectors[uViewSlot].GetCommittedHandle(); }
	Flux_ShaderResourceView& GetSSGISRV(u_int uViewSlot = kuFluxViewSlotMain);
	bool IsEnabled() const;

	Flux_RenderAttachment& GetRawResultAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetResolvedAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetDenoisedAttachment(u_int uViewSlot = kuFluxViewSlotMain);

	// SSGI single-MRT handles, one set per render-view slot. Note SSGI has NO
	// aux/confidence handles — that is the divergence from SSR; do NOT unify
	// these.
	Flux_TransientHandle m_axRawResultHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axResolvedHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axDenoiseHHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axDenoisedHandles[FLUX_MAX_RENDER_VIEWS];

	// Per-view base widths captured at setup (slot 0 = swapchain, preview =
	// kuFLUX_PREVIEW_VIEW_SIZE) — ComputeEffectiveBinarySearchIterations scales
	// its bump from these.
	u_int                m_auViewWidths[FLUX_MAX_RENDER_VIEWS] = {};

	// SSGI-only raymarch resolution divisor (full / divisor), shared by every
	// view. Stays in the derived — SSR has no equivalent.
	u_int                m_uRayMarchResolutionDivisor = 4u;
	u_int                m_uLastResolutionDivisor     = 4u;

	// Tracks which transient the deferred pass reads per view (Denoised when
	// denoise is on, Resolved otherwise) and triggers a graph rebuild when
	// either the denoise toggle or the resolution divisor diverges from the
	// committed selection (both are view-independent, so
	// ApplyDenoiseSelectionToGraph checks the main slot).
	Flux_CommittedHandleSelector<Flux_SSGISelection> m_axSSGISelectors[FLUX_MAX_RENDER_VIEWS];

private:
	// Per-view transients + RayMarch/Upsample/DenoiseH/DenoiseV pass chain
	// (S5b): called for the main view at swapchain dims, then for the preview
	// view at kuFLUX_PREVIEW_VIEW_SIZE² only while it is active — so the main
	// path stays byte-equivalent.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight);
};
