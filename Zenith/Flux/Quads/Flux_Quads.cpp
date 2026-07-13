#include "Zenith.h"
#include "Flux/Quads/Flux_Quads_Shaders.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Quads.h" // typed binding handles

#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#include <algorithm>

// Phase 7b: state on Flux_QuadsImpl held by Zenith_Engine.
//
// Cross-subsystem deps (FluxGraphics + VulkanMemory) are reached via g_xEngine
// at point of use. The non-capturing ExecuteQuads / hot-reload fn-pointer
// trampolines below cannot capture, so they re-enter via g_xEngine.Quads() to
// recover the singleton instance.



static void ExecuteQuads(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_QuadsImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_QuadsShaders::xQuads);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);//position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//uv
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT4);//position size
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT);//texture index
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//UVMult UVAdd
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT);//corner radius
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//size pixels
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour2 (gradient)
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xPipelineSpec);
}

void Flux_QuadsImpl::Initialise()
{
	BuildPipelines();

	g_xEngine.FluxMemory().InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), m_xInstanceBuffer, false);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads initialised");
}

void Flux_QuadsImpl::Shutdown()
{
	g_xEngine.FluxMemory().DestroyDynamicVertexBuffer(m_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads shut down");
}

void Flux_QuadsImpl::UploadInstanceData()
{
	SortQueuedQuadsForUpload();
	g_xEngine.FluxMemory().UploadBufferData(m_xInstanceBuffer.GetBuffer().m_xVRAMHandle, m_axQuadsToRender, sizeof(Quad) * m_uQuadRenderIndex);
}

void Flux_QuadsImpl::SortQueuedQuadsForUpload()
{
	if (m_uQuadRenderIndex < 2u)
	{
		return;
	}

	bool bNeedsSort = false;
	for (uint32_t u = 1u; u < m_uQuadRenderIndex; ++u)
	{
		if (m_aiQuadSortOrders[u - 1u] > m_aiQuadSortOrders[u])
		{
			bNeedsSort = true;
			break;
		}
	}
	if (!bNeedsSort)
	{
		return;
	}

	// Quads arrive from every loaded scene/canvas. Sorting this shared queue is
	// what makes UI element sort order global instead of canvas-local. The
	// submission index is an explicit tie-breaker so equal-order quads preserve
	// their historical submission order.
	struct QueuedQuad
	{
		Quad m_xQuad;
		int m_iSortOrder;
		uint32_t m_uSubmissionIndex;
	};

	QueuedQuad axQueued[FLUX_MAX_QUADS_PER_FRAME];
	for (uint32_t u = 0u; u < m_uQuadRenderIndex; ++u)
	{
		axQueued[u].m_xQuad = m_axQuadsToRender[u];
		axQueued[u].m_iSortOrder = m_aiQuadSortOrders[u];
		axQueued[u].m_uSubmissionIndex = u;
	}

	std::sort(axQueued, axQueued + m_uQuadRenderIndex,
		[](const QueuedQuad& xA, const QueuedQuad& xB)
		{
			if (xA.m_iSortOrder != xB.m_iSortOrder)
			{
				return xA.m_iSortOrder < xB.m_iSortOrder;
			}
			return xA.m_uSubmissionIndex < xB.m_uSubmissionIndex;
		});

	for (uint32_t u = 0u; u < m_uQuadRenderIndex; ++u)
	{
		m_axQuadsToRender[u] = axQueued[u].m_xQuad;
		m_aiQuadSortOrders[u] = axQueued[u].m_iSortOrder;
	}
}

void Flux_QuadsImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}

	UploadInstanceData();
}

static void ExecuteQuads(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Quads() to reach the singleton
	// instance; FluxGraphics is reached via g_xEngine at point of use
	// (mirrors ExecuteSSAOGenerate).
	Flux_QuadsImpl& xQuads = g_xEngine.Quads();

	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled || xQuads.m_uQuadRenderIndex == 0)
	{
		return;
	}

	xQuads.UploadInstanceData();

	pxCommandList->SetPipeline(&xQuads.m_xPipeline);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
	pxCommandList->SetVertexBuffer(xQuads.m_xInstanceBuffer, 1);

	// Bindless table now lives at the spine's BINDLESS set (set 2), not the
	// former set 1.
	pxCommandList->UseBindlessTextures(2);

	pxCommandList->DrawIndexed(6, xQuads.m_uQuadRenderIndex);

	xQuads.m_uQuadRenderIndex = 0;
	xQuads.m_bCapacityWarningIssued = false;
}

void Flux_QuadsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Quads", ExecuteQuads)
		.Writes(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}

void Flux_QuadsImpl::UploadQuad(const Quad& xQuad, int iSortOrder)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}
	if (m_uQuadRenderIndex >= FLUX_MAX_QUADS_PER_FRAME)
	{
		// Drop newest: already-queued overlays (including full-screen fades)
		// remain intact and no fixed-capacity storage is overrun.
		if (!m_bCapacityWarningIssued)
		{
			Zenith_Warning(LOG_CATEGORY_RENDERER,
				"Flux_Quads capacity (%u) exceeded; dropping new quads",
				static_cast<u_int>(FLUX_MAX_QUADS_PER_FRAME));
			m_bCapacityWarningIssued = true;
		}
		return;
	}

	m_axQuadsToRender[m_uQuadRenderIndex] = xQuad;
	m_aiQuadSortOrders[m_uQuadRenderIndex] = iSortOrder;
	++m_uQuadRenderIndex;
}
