#include "Zenith.h"
#include "Flux/SDFs/Flux_SDFs_Shaders.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/SDFs.h" // typed binding handles

#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/RenderViews/Flux_ViewPassNames.h"   // per-view pass names — slot 0 returns the base literal by pointer identity
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Phase 7b: state on Flux_SDFsImpl held by Zenith_Engine.

static constexpr uint32_t s_uMaxSpheres = 1000;
struct Sphere
{
	Zenith_Maths::Vector4 m_xPosition_Radius;
	Zenith_Maths::Vector4 m_xColour;
};
struct SphereData
{
	uint32_t m_uNumSpheres;
	uint32_t m_auPad[7];
	Sphere m_axSpheres[s_uMaxSpheres];
} s_axSphereData;


static void ExecuteSDFs(Flux_CommandBuffer* pxCommandList, void* pUserData);

void Flux_SDFsImpl::BuildPipelines()
{
	this->m_xShader.Initialise(Flux_SDFsShaders::xSDFs);

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &this->m_xShader;
	// No vertex input: this is a vertex-pulling / fullscreen program whose VS reads
	// no attributes. m_pxVertexLayout stays null, which is the canonical spelling the
	// validation tripwire matches against an empty reflection table.
	xPipelineSpec.m_eTopology = MESH_TOPOLOGY_NONE;

	this->m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;

	Flux_PipelineBuilder::FromSpecification(this->m_xPipeline, xPipelineSpec);
}

void Flux_SDFsImpl::Initialise()
{
	BuildPipelines();

	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&s_axSphereData, sizeof(s_axSphereData), this->m_xSpheresBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs initialised");
}

void Flux_SDFsImpl::Shutdown()
{
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(this->m_xSpheresBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_SDFs shut down");
}

void Flux_SDFsImpl::UploadSpheres()
{
	s_axSphereData.m_uNumSpheres = 2;

	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[0];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + sin(g_xEngine.Frame().GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(1., 0., 0., 1.);
	}
	{
		Sphere& xSphere = s_axSphereData.m_axSpheres[1];
		xSphere.m_xPosition_Radius = Zenith_Maths::Vector4(2000, 1500 + cos(g_xEngine.Frame().GetTimePassed()) * 200, 2000, 100);
		xSphere.m_xColour = Zenith_Maths::Vector4(0., 1., 0., 1.);
	}

	g_xEngine.FluxMemory().UploadBufferData(this->m_xSpheresBuffer.GetBuffer().m_xVRAMHandle, &s_axSphereData, sizeof(s_axSphereData));
}

void Flux_SDFsImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bSDFsEnabled)
	{
		return;
	}

	UploadSpheres();
}

static void ExecuteSDFs(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	// RECORD-TIME EARLY-OUT, and it must stay: SDFs are scene-derived and a
	// non-main view's flags carry no FLUX_VIEW_FLAG_SCENE_CONTENT, so the
	// "SDFs (Preview)" instance (which exists because the oracle's golden pass
	// list carries "SDFs" — see SetupRenderGraph) records nothing.
	if (Flux_RenderGraph::GetCurrentRecordingPassViewSlot() != kuFluxViewSlotMain)
	{
		return;
	}
	if (!Zenith_GraphicsOptions::Get().m_bSDFsEnabled)
	{
		return;
	}

	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it
	// cannot capture, so it re-enters via g_xEngine.SDFs() to reach the singleton
	// instance; FluxGraphics is reached via g_xEngine at point of use
	// (mirrors ExecuteSSAOGenerate).
	Flux_SDFsImpl& xSDFs = g_xEngine.SDFs();

	xSDFs.UploadSpheres();

	pxCommandList->SetPipeline(&xSDFs.m_xPipeline);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace SDF = Flux_Generated_SDFs::SDFs;
	xBinder.BindCBV(SDF::hSphereData, &xSDFs.m_xSpheresBuffer.GetCBV());

	pxCommandList->DrawIndexed(6);
}

void Flux_SDFsImpl::SetupViewPasses(Flux_RenderGraph& xGraph, u_int uSlot, Flux_GraphicsImpl& xGraphics)
{
	// ONE view's entire declaration. Every resource is indexed by uSlot and there
	// is no per-slot branch, because there is nothing to discriminate: the
	// difference between the main view's pass and a preview view's pass is the
	// slot and nothing else. The pipeline uses default depth-test+write enabled,
	// so the depth attachment is bound as a writable DSV for the renderpass.
	//
	// The name comes from Flux_ViewPassName, which supplies the per-view
	// uniqueness the graph's duplicate-name assert demands: slot 0 gets the base
	// literal back by pointer identity, so the main view's row is still exactly
	// "SDFs", and the preview slot composes "SDFs (Preview)" — byte-for-byte the
	// literal this replaced.
	//
	// No ClearTargets on any slot — "Apply Lighting (Preview)" owns the preview
	// HDR clear, and the main HDR clear belongs to "Apply Lighting".
	xGraph.AddPass(Flux_ViewPassName("SDFs", uSlot), ExecuteSDFs)
		.View  (uSlot)
		.Writes(xGraphics.GetHDRSceneTarget(uSlot),  RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(uSlot), RESOURCE_ACCESS_WRITE_DSV);
}

void Flux_SDFsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// ONE "SDFs" pass per ACTIVE FULL-PIPELINE view, in ascending slot order. The
	// registry decides membership by view PROPERTIES, never by slot number: slot 0
	// always qualifies (active + full-pipeline from construction), the preview slot
	// joins while its owner has it up, and depth-only shadow cascades are never
	// full-pipeline and never get a pass. Today that set is exactly
	// {main} ∪ {preview if active} — which is what the hand-written call plus its
	// `if (IsViewActive(preview))` block produced, in the same declaration order.
	//
	// ★ WHY THE PREVIEW INSTANCE EXISTS AT ALL: nothing in the graph needs it. No
	// pass reads what it writes, it satisfies no layout requirement, and
	// ExecuteSDFs early-outs on every non-main slot (SDFs are scene-derived and
	// the preview view's flags carry no FLUX_VIEW_FLAG_SCENE_CONTENT), so it
	// records nothing. It exists because "SDFs" is on the golden per-view pass
	// list RT_RenderGraphViewStructure asserts — that is the whole reason, and it
	// is worth stating plainly rather than calling it "structural parity".
	//
	// The callback is a CAPTURELESS LAMBDA written inside the member body rather
	// than a file-static free function, matching Flux_HiZImpl::SetupRenderGraph:
	// being captureless it converts to the registry's plain fn-pointer, so `this`,
	// the graph and the ALREADY-hoisted graphics reference all travel through
	// pCtx. Nothing below re-reaches g_xEngine — this TU sits exactly on its
	// engine-singleton allowlist ceiling.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	struct SetupCtx
	{
		Flux_SDFsImpl*     m_pxThis;
		Flux_RenderGraph*  m_pxGraph;
		Flux_GraphicsImpl* m_pxGraphics;
	};
	SetupCtx xCtx{ this, &xGraph, &xGraphics };
	xGraphics.RenderViews().ForEachActiveFullPipelineView(+[](u_int uSlot, const Flux_RenderView&, void* pCtx)
	{
		SetupCtx& xSetup = *static_cast<SetupCtx*>(pCtx);
		xSetup.m_pxThis->SetupViewPasses(*xSetup.m_pxGraph, uSlot, *xSetup.m_pxGraphics);
	}, &xCtx);
}
