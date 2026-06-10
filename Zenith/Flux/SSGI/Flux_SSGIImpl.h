#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_ScreenSpaceEffectBase.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

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
	u_int ComputeEffectiveBinarySearchIterations() const;
	void UpdateSSGIConstants();

	Flux_TransientHandle GetSSGIHandle() const { return m_xSSGISelector.GetCommittedHandle(); }
	Flux_ShaderResourceView& GetSSGISRV();
	bool IsEnabled() const;

	Flux_RenderAttachment& GetRawResultAttachment();
	Flux_RenderAttachment& GetResolvedAttachment();
	Flux_RenderAttachment& GetDenoisedAttachment();

	// SSGI single-MRT handles. Note SSGI has NO aux/confidence handles — that is
	// the divergence from SSR; do NOT unify these.
	Flux_TransientHandle m_xRawResultHandle;
	Flux_TransientHandle m_xResolvedHandle;
	Flux_TransientHandle m_xDenoiseHHandle;
	Flux_TransientHandle m_xDenoisedHandle;

	// SSGI-only raymarch resolution divisor (full / divisor). Stays in the
	// derived — SSR has no equivalent.
	u_int                m_uRayMarchResolutionDivisor = 4u;
	u_int                m_uLastResolutionDivisor     = 4u;

	Flux_PassHandle      m_xDenoisePassH;
	Flux_PassHandle      m_xDenoisePassV;

	// Tracks which transient the deferred pass reads (Denoised when denoise is
	// on, Resolved otherwise) and triggers a graph rebuild when either the
	// denoise toggle or the resolution divisor diverges from the committed
	// selection.
	Flux_CommittedHandleSelector<Flux_SSGISelection> m_xSSGISelector;
};
