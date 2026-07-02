#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_ScreenSpaceEffectBase.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

class Flux_RenderGraph;

enum SSR_DebugMode : u_int
{
	SSR_DEBUG_NONE = 0,
	SSR_DEBUG_RAY_DIRECTIONS,
	SSR_DEBUG_SCREEN_DIRECTIONS,
	SSR_DEBUG_HIT_POSITIONS,
	SSR_DEBUG_REFLECTION_UVS,
	SSR_DEBUG_CONFIDENCE,
	SSR_DEBUG_DEPTH_COMPARISON,
	SSR_DEBUG_EDGE_FADE,
	SSR_DEBUG_MARCH_DISTANCE,
	SSR_DEBUG_FINAL_RESULT,
	SSR_DEBUG_ROUGHNESS,
	SSR_DEBUG_WORLD_NORMAL_Y,
	SSR_DEBUG_RAY_COUNT,
	SSR_DEBUG_COUNT
};

// Phase 9: state + behaviour for SSR subsystem.
//
// S5b: the whole SSR chain (transients, constants CBV, committed-handle
// selector) is per-render-view — slot 0 (main) at swapchain dims as before;
// the preview view at 512² when active. uViewSlot defaults keep single-view
// callers unchanged.
class Flux_SSRImpl : public Flux_ScreenSpaceEffectBase<Flux_SSRImpl>
{
public:
	Flux_SSRImpl() = default;
	~Flux_SSRImpl() = default;

	Flux_SSRImpl(const Flux_SSRImpl&) = delete;
	Flux_SSRImpl& operator=(const Flux_SSRImpl&) = delete;

	// Cross-subsystem deps (VulkanMemory/Swapchain/FluxGraphics/HiZ/VolumeFog/
	// FluxRenderer) are reached via g_xEngine at point of use; the non-capturing
	// Execute* / hot-reload trampolines recover this instance via g_xEngine.SSR().
	void Initialise();
	void BuildPipelines();

	// CRTP hook called by Flux_ScreenSpaceEffectBase::Shutdown() — destroys the
	// per-view SSR constants CBVs. The base resets m_pxGraph / m_bInitialised
	// afterwards.
	void ShutdownImpl();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void ApplyBlurSelectionToGraph(Flux_RenderGraph& xGraph);

	// Promoted from a file-static helper; reaches HiZ via g_xEngine at point of
	// use. Fills a frame-local snapshot from the debug-var-bound tuning struct,
	// overrides the view-dependent fields (dims + HiZ mip chain) for uViewSlot,
	// and uploads it to that view's CBV. Public because the call site sits
	// inside the non-capturing ExecuteSSRRayMarch trampoline, which recovers
	// this instance via g_xEngine.SSR() first.
	void UpdateSSRConstants(u_int uViewSlot);

	Flux_TransientHandle GetReflectionHandle(u_int uViewSlot = kuFluxViewSlotMain) const { return m_axReflectionSelectors[uViewSlot].GetCommittedHandle(); }
	Flux_ShaderResourceView& GetReflectionSRV(u_int uViewSlot = kuFluxViewSlotMain);
	bool IsEnabled() const;

	Flux_RenderAttachment& GetRayMarchAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetRayMarchAuxAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetUpsampledAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetUpsampledAuxAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetDenoiseHAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetDenoiseHConfAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetDenoiseVAttachment(u_int uViewSlot = kuFluxViewSlotMain);

	// SSR-specific dual-MRT handles (RT0 colour/confidence + RT1 aux/metadata),
	// one set per render-view slot. These do NOT exist on SSGI — kept here to
	// respect the divergence.
	Flux_TransientHandle m_axRayMarchHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axRayMarchAuxHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axUpsampledHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axUpsampledAuxHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axDenoiseHHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axDenoiseHConfHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle m_axDenoiseVHandles[FLUX_MAX_RENDER_VIEWS];

	// One SSR-constants CBV per render-view slot (each view uploads its own
	// dims + HiZ mip-size table). Inactive slots still carry a valid buffer —
	// mirrors Flux_GraphicsImpl::m_axViewConstantsBuffers.
	Flux_DynamicConstantBuffer m_axSSRConstantsBuffers[FLUX_MAX_RENDER_VIEWS];

	// Per-view base dims captured at setup (slot 0 = swapchain, preview =
	// kuFLUX_PREVIEW_VIEW_SIZE²) — UpdateSSRConstants derives the half-res +
	// HiZ mip-size fields from these.
	u_int m_auViewWidths[FLUX_MAX_RENDER_VIEWS]  = {};
	u_int m_auViewHeights[FLUX_MAX_RENDER_VIEWS] = {};

	// Tracks which transient the deferred pass reads per view (DenoiseV when
	// blur is on, Upsampled otherwise) and triggers a graph rebuild when the
	// live toggle diverges from the committed selection (the toggle is
	// view-independent, so ApplyBlurSelectionToGraph checks the main slot).
	Flux_CommittedHandleSelector<bool> m_axReflectionSelectors[FLUX_MAX_RENDER_VIEWS];

private:
	// Per-view transients + RayMarch/Upsample/DenoiseH/DenoiseV pass chain
	// (S5b): called for the main view at swapchain dims, then for the preview
	// view at kuFLUX_PREVIEW_VIEW_SIZE² only while it is active — so the main
	// path stays byte-equivalent.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight);
};
