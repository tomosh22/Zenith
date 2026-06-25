#include "Zenith.h"
#include "Flux/Present/Flux_Present_Shaders.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Present/Flux_PresentImpl.h"

#include "Flux/Flux_GraphicsImpl.h"          // GetFinalRenderTarget / GetGBufferSRV / m_xQuadMesh
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Present.h"  // typed binding handles
#include "DebugVariables/Zenith_DebugVariables.h"

// MRT debug visualisation: when set, the present blit samples the chosen
// G-buffer MRT instead of the lit final render target. (Moved here from
// Zenith_Vulkan_Swapchain when the present blit became a backend-neutral
// feature — these are owned by the present job, not the swapchain backend.)
DEBUGVAR bool     dbg_bOutputMRT = false;
DEBUGVAR uint32_t dbg_uMRTIndex  = MRT_INDEX_DIFFUSE;

// No-op record callback for the "Final RT Layout Transition" graph pass. The
// pass carries no commands — it exists only so the render graph leaves the
// Final RT in SHADER_READ_ONLY_OPTIMAL for the present blit, which samples it
// OUTSIDE the graph (in the backend swapchain's present step via RecordBlit).
// Replaces the former file-static Flux_FinalLayoutTransitionNoOp +
// @FinalRTLayoutTransition AddSetupStep, now owned by this feature.
static void Flux_PresentPrepareNoOp(Flux_CommandBuffer*, void*)
{
}

void Flux_PresentImpl::BuildPipelines()
{
	// Source the backbuffer colour format from the swapchain's current target (a
	// neutral Flux_RenderAttachment) so the present pipeline's render-pass format
	// matches the actual backbuffer on every backend. The swapchain is
	// initialised before the feature init walk (see Flux_RendererImpl::
	// LateInitialise), so this is valid at Initialise time; it is also valid at
	// hot-reload time (swapchain long since up). The surface format is stable
	// across swapchain recreation (resize), so no per-resize rebuild is needed.
	uint32_t uNumColour = 0;
	Flux_RenderAttachment* pxDepth = nullptr;
	Flux_RenderAttachment* pxSwapTarget = g_xEngine.FluxSwapchain().GetCurrentSwapchainTarget(uNumColour, pxDepth);
	const TextureFormat eBackbufferFormat = pxSwapTarget->m_xSurfaceInfo.m_eFormat;

	m_xPresentShader.Initialise(Flux_PresentShaders::xTexturedQuad);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xSpec;
	xSpec.m_aeColourAttachmentFormats[0] = eBackbufferFormat;
	xSpec.m_uNumColourAttachments = 1;
	xSpec.m_pxShader = &m_xPresentShader;
	xSpec.m_xVertexInputDesc = xVertexDesc;

	// Reflection-driven layout: after the ParameterBlock conversion the present
	// shader's g_xTexture lives in its own PassParams at set 3 (the spine
	// reserves sets 0/1/2). PopulateLayout reproduces that exact set/binding
	// map — the former hand-built single-group-at-set-0 layout would no longer
	// match the shader.
	m_xPresentShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

	Flux_PipelineBuilder::FromSpecification(m_xPresentPipeline, xSpec);
}

void Flux_PresentImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Debug", "Output MRT" }, dbg_bOutputMRT);
	g_xEngine.DebugVariables().AddUInt32 ({ "Render", "Debug", "MRT Index" }, dbg_uMRTIndex, 0, MRT_INDEX_COUNT - 1);
#endif
}

void Flux_PresentImpl::Shutdown()
{
	m_xPresentPipeline.Reset();
	m_xPresentShader.Reset();
}

void Flux_PresentImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Pure reader → sorts strictly after every Final-RT writer (tonemap / quads /
	// text / gizmos), leaving the Final RT in SHADER_READ_ONLY_OPTIMAL so the
	// present blit (recorded OUTSIDE the graph by the backend swapchain via
	// RecordBlit) can sample it. This is the last setup step in the walk because
	// Present is the last-registered feature.
	xGraph.AddPass("Final RT Layout Transition", Flux_PresentPrepareNoOp)
	      .Reads(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_READ_SRV);
}

void Flux_PresentImpl::RecordBlit(Flux_CommandBuffer& xCmd)
{
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	xCmd.SetPipeline(&m_xPresentPipeline);
	xCmd.SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	xCmd.SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(xCmd);
	namespace PQ = Flux_Generated_Present::TexturedQuad;

#ifdef ZENITH_DEBUG_VARIABLES
	if (dbg_bOutputMRT)
	{
		// Debug: sample the chosen G-buffer MRT instead of the lit scene.
		Flux_ShaderResourceView* pxMRTSRV = xGraphics.GetGBufferSRV((MRTIndex)dbg_uMRTIndex);
		if (pxMRTSRV)
		{
			xBinder.BindSRV(PQ::hg_xTexture, pxMRTSRV);
		}
	}
	else
#endif
	{
		Flux_ShaderResourceView& xSRV = xGraphics.GetFinalRenderTarget().SRV();
		if (xSRV.m_xImageViewHandle.IsValid())
		{
			xBinder.BindSRV(PQ::hg_xTexture, &xSRV);
		}
		else
		{
			Zenith_Assert(false, "Final render target SRV not created");
		}
	}

	xCmd.DrawIndexed(6);
}
