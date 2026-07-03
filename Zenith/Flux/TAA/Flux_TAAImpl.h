#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

// =====================================================================
// Flux_TAAImpl — the temporal anti-aliasing (TAA) render feature.
//
// Registered just BEFORE HDR so its resolve runs on the fully-composited lit HDR
// scene (after DeferredShading / Grass / Translucency / Fog / Particles) and
// BEFORE the HDR bloom + tonemap, which read the resolved output through
// Flux_GraphicsImpl::GetSceneColourForPostFX. MAIN view ONLY — the material
// preview / shadow cascade views never jitter or write velocity, so they never
// get a TAA pass.
//
// It CONSUMES the Stage 1-3 foundation (does not re-plumb it): the optional
// velocity MRT + the sub-pixel jitter / NoJitter reprojection terms already staged
// into the slot-0 ViewConstants payload; and it is gated in LOCKSTEP with them via
// FluxGraphics().IsVelocityMRTActive(). When TAA is off there is no resolve pass
// and GetSceneColourForPostFX falls through to raw HDR (byte-identical pre-TAA path).
//
// Passes (all main-view compute; static graph can't ping-pong history by frame
// parity, so history is a persistent feature-owned target + a copy pass):
//   TAA_Resolve      : HDR + velocity + depth + history(prev) -> resolved output
//                      (transient; rgb = resolved, a = current linear view depth)
//   TAA_CopyToHistory: resolved output -> persistent history (for next frame)
//   TAA_Sharpen      : resolved output -> sharpened output (the post-FX source)
//
// Public data members follow the HiZ compute-feature convention (non-capturing
// graph callbacks reach them via g_xEngine.TAA()).
// =====================================================================
class Flux_TAAImpl
{
public:
	Flux_TAAImpl() = default;
	~Flux_TAAImpl() = default;

	Flux_TAAImpl(const Flux_TAAImpl&) = delete;
	Flux_TAAImpl& operator=(const Flux_TAAImpl&) = delete;

	// FluxRenderFeature lifecycle (all four mandatory — a no-op stub where nothing to do).
	void Initialise();
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// The FINAL post-TAA scene colour (main view) = the sharpened output. Consumed by
	// the HDR bloom/tonemap via GetSceneColourForPostFX. Valid ONLY while IsResolveActive();
	// asserts otherwise.
	Flux_RenderAttachment& GetResolvedOutput();

	// True when this graph build declared the TAA passes (main view + velocity latch on).
	// Mirrors EXACTLY the condition SetupRenderGraph declares the passes under, so the
	// post-FX seam and the pass declarations can never disagree.
	bool IsResolveActive() const { return m_bResolveActiveThisBuild; }
	bool IsInitialised()   const { return m_bInitialised; }

	// Runtime enable hook for automation/tests (TAAToggleStress). Forwards to the graphics
	// velocity/TAA latch override: SetEnabled forces on/off and triggers the graph rebuild;
	// ClearEnabledOverride restores debug-var / --taa control. Default: no override (inert).
	void SetEnabled(bool bEnabled);
	void ClearEnabledOverride();

	// Resolution-change hook: recreate the persistent history at the new dims + invalidate.
	// Registered as a non-capturing trampoline that re-enters via g_xEngine.TAA().
	void OnResolutionChanged();

	// --- pipelines (public: reached by the non-capturing graph record callbacks) ---
	Flux_Shader   m_xResolveShader;   Flux_Pipeline m_xResolvePipeline;   Flux_RootSig m_xResolveRootSig;
	Flux_Shader   m_xCopyShader;      Flux_Pipeline m_xCopyPipeline;      Flux_RootSig m_xCopyRootSig;
	Flux_Shader   m_xSharpenShader;   Flux_Pipeline m_xSharpenPipeline;   Flux_RootSig m_xSharpenRootSig;

	// --- persistent history (feature-owned; survives across frames + graph rebuilds) ---
	Flux_RenderAttachment m_xHistory;

	// --- per-build state ---
	Flux_RenderGraph*    m_pxGraph = nullptr;
	Flux_TransientHandle m_xResolvedOutputHandle;   // rgb = resolved, a = current linear depth
	Flux_TransientHandle m_xSharpenedOutputHandle;  // the post-FX source (GetResolvedOutput)
	bool                 m_bResolveActiveThisBuild = false;
	bool                 m_bInitialised = false;

	// History validity: false after Initialise / graph rebuild / resize. The resolve emits
	// the current frame verbatim (ignoring the stale history) on the first frame, then it
	// becomes valid. Captured on the main thread by the resolve's Prepare callback so the
	// worker-thread record only reads an immutable snapshot.
	bool m_bHistoryValid = false;
	bool m_bResolveHistoryValidThisFrame = false;

	// Resolve/sharpen tuning (debug-var backed; defaults are the shipping values). CB-only —
	// edited live, read into the pass constant buffers each frame, no graph rebuild.
	float m_fBlendMinAlpha              = 0.1f;   // slow-pixel history weight floor (sharpest AA)
	float m_fBlendMaxAlpha              = 0.5f;   // fast-pixel current-frame weight (least ghosting)
	float m_fVelocityRejectionThreshold = 32.0f; // pixels of motion that ramps the blend to max
	float m_fHistoryClampStrength       = 1.0f;  // gamma in the mean +/- gamma*sigma variance clip
	float m_fDisocclusionDepthThreshold = 0.05f; // relative depth mismatch that rejects history
	float m_fSharpenAmount              = 0.25f; // RCAS sharpen strength (0 => identity)

private:
	// (Re)create m_xHistory at swapchain dims (UAV|SHADER_READ RGBA16F). Idempotent —
	// Destroys any prior attachment first, so it is safe to call on resize.
	void BuildHistoryTarget();
};
