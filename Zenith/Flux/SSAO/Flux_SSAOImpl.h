#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Cross-subsystem dependencies injected into Initialise (Wave-11 DI seam, the
// 2nd leaf seam built on the WS9.2 Flux_HiZImpl template). Forward-declared
// here; full headers are pulled in by Flux_SSAO.cpp.
class Flux_GraphicsImpl;
class Zenith_Vulkan_Swapchain;
class Flux_HDRImpl;

// Phase 9: state + behaviour for SSAO subsystem.
//
// Wave-11 DI seam (mirrors Flux_HiZImpl): cross-subsystem dependencies are
// INJECTED through Initialise as explicit references and stored as member
// pointers, rather than reached for via g_xEngine.X() inside every method. The
// only place g_xEngine self-lookup survives is the non-capturing fn-pointer
// trampolines (the Execute* graph callbacks, the DebugGet* texture callbacks,
// and the ZENITH_TOOLS hot-reload callback) — those cannot capture state, so
// they re-enter via g_xEngine.SSAO() to reach this singleton instance and then
// route their other reach-ins through the injected members.
class Flux_SSAOImpl
{
public:
	Flux_SSAOImpl() = default;
	~Flux_SSAOImpl() = default;

	Flux_SSAOImpl(const Flux_SSAOImpl&) = delete;
	Flux_SSAOImpl& operator=(const Flux_SSAOImpl&) = delete;

	// Cross-subsystem deps are injected here and stored into the member pointers
	// below. This is the WS9.2 DI template: explicit ref params -> stored member
	// pointers.
	void Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_Swapchain& xSwapchain, Flux_HDRImpl& xHDR);
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Attachment accessors — resolve through the graph's transient slot. Were
	// file-static helpers reaching for g_xEngine.SSAO(); now non-static members
	// reading this->m_pxGraph / this->m_x*Handle (mirror HiZ GetHiZBuffer). Kept
	// public (like the SSR transient accessors) because the SSAO Execute* /
	// DebugGet* trampolines re-enter via g_xEngine.SSAO() and call them — unlike
	// HiZ's GetHiZBuffer, which is only reached from other instance methods.
	Flux_RenderAttachment& GetRawOcclusion();
	Flux_RenderAttachment& GetBlurred();

	Flux_Shader   m_xGenerateShader;
	Flux_Shader   m_xBlurShader;
	Flux_Shader   m_xUpsampleShader;
	Flux_Pipeline m_xGeneratePipeline;
	Flux_Pipeline m_xBlurPipeline;
	Flux_Pipeline m_xUpsamplePipeline;

	Flux_TransientHandle m_xRawOcclusionHandle;
	Flux_TransientHandle m_xBlurredHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real boot
	// path wires them in Flux.cpp.
	Flux_GraphicsImpl*       m_pxGraphics  = nullptr;
	Zenith_Vulkan_Swapchain* m_pxSwapchain = nullptr;
	Flux_HDRImpl*            m_pxHDR       = nullptr;
};
