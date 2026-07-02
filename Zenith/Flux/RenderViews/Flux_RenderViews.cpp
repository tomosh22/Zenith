#include "Zenith.h"

#include "Flux/RenderViews/Flux_RenderViews.h"

Flux_RenderViewRegistry::Flux_RenderViewRegistry()
{
	// Fixed slot layout (see header): 0 = MAIN (always active, full pipeline),
	// 1..N = shadow cascades (depth-only; activated by Shadows when enabled),
	// 1+N = preview (full pipeline; activated by the preview controller).
	m_axViews[kuFluxViewSlotMain].m_eType         = FLUX_RENDER_VIEW_MAIN;
	m_axViews[kuFluxViewSlotMain].m_bActive       = true;
	m_axViews[kuFluxViewSlotMain].m_bFullPipeline = true;
	m_axViews[kuFluxViewSlotMain].m_uViewFlags    = FLUX_VIEW_FLAG_SHADOWS_ENABLED
	                                              | FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED
	                                              | FLUX_VIEW_FLAG_SCENE_CONTENT;
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		m_axViews[kuFluxViewSlotShadowFirst + u].m_eType = FLUX_RENDER_VIEW_SHADOW_CASCADE;
	}
	m_axViews[kuFluxViewSlotPreview].m_eType         = FLUX_RENDER_VIEW_PREVIEW;
	m_axViews[kuFluxViewSlotPreview].m_bFullPipeline = true;
}

Flux_RenderView& Flux_RenderViewRegistry::View(u_int uSlot)
{
	Zenith_Assert(uSlot < FLUX_MAX_RENDER_VIEWS, "Flux_RenderViewRegistry::View: slot %u out of range", uSlot);
	return m_axViews[uSlot];
}

const Flux_RenderView& Flux_RenderViewRegistry::View(u_int uSlot) const
{
	Zenith_Assert(uSlot < FLUX_MAX_RENDER_VIEWS, "Flux_RenderViewRegistry::View: slot %u out of range", uSlot);
	return m_axViews[uSlot];
}

bool Flux_RenderViewRegistry::SetViewActive(u_int uSlot, bool bActive)
{
	Zenith_Assert(uSlot < FLUX_MAX_RENDER_VIEWS, "Flux_RenderViewRegistry::SetViewActive: slot %u out of range", uSlot);
	Zenith_Assert(uSlot != kuFluxViewSlotMain || bActive, "Flux_RenderViewRegistry: the MAIN view (slot 0) can never be deactivated");
	if (uSlot == kuFluxViewSlotMain) { bActive = true; }
	const bool bChanged = m_axViews[uSlot].m_bActive != bActive;
	m_axViews[uSlot].m_bActive = bActive;
	return bChanged;
}

u_int Flux_RenderViewRegistry::ActiveViewMask() const
{
	u_int uMask = 0u;
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		if (m_axViews[u].m_bActive) { uMask |= 1u << u; }
	}
	return uMask;
}

u_int Flux_RenderViewRegistry::HighestActiveSlotPlusOne() const
{
	u_int uCount = 0u;
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		if (m_axViews[u].m_bActive) { uCount = u + 1u; }
	}
	return uCount;
}

void Flux_RenderViewRegistry::ForEachActiveFullPipelineView(void (*pfn)(u_int uSlot, const Flux_RenderView&, void* pCtx), void* pCtx) const
{
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		if (m_axViews[u].m_bActive && m_axViews[u].m_bFullPipeline)
		{
			pfn(u, m_axViews[u], pCtx);
		}
	}
}

#include "Flux/RenderViews/Flux_RenderViews.Tests.inl"
