#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Cross-subsystem dependencies injected into Initialise (Wave-14 DI seam, same
// reusable template as the WS9.2 Flux_HiZImpl / Wave-11 Flux_SSAOImpl leaf
// seams). Forward-declared here; full headers are pulled in by Flux_SDFs.cpp.
class Flux_GraphicsImpl;
class Flux_HDRImpl;
class FrameContext;

// Phase 9: state + behaviour for SDFs subsystem.
//
// Wave-14 DI seam (mirrors Flux_SSAOImpl): the two cross-subsystem dependencies
// (FluxGraphics for the fullscreen quad + frame constants + depth attachment,
// HDR for the scene render target) are INJECTED through Initialise as explicit
// references and stored as member pointers, rather than reached for via
// g_xEngine.X() inside every method. The only place g_xEngine self-lookup
// survives is the non-capturing fn-pointer trampoline (the ExecuteSDFs graph
// callback) — it cannot capture state, so it re-enters via g_xEngine.SDFs() to
// reach this singleton instance and then routes its FluxGraphics reach-ins
// through the injected member.
class Flux_SDFsImpl
{
public:
	Flux_SDFsImpl() = default;
	~Flux_SDFsImpl() = default;

	Flux_SDFsImpl(const Flux_SDFsImpl&) = delete;
	Flux_SDFsImpl& operator=(const Flux_SDFsImpl&) = delete;

	// Cross-subsystem deps are injected here and stored into the member pointers
	// below. This is the WS9.2 DI template: explicit ref params -> stored member
	// pointers.
	void Initialise(Flux_GraphicsImpl& xGraphics, Flux_HDRImpl& xHDR,
		Flux_MemoryManager& xVulkanMemory, FrameContext& xFrame);
	void BuildPipelines();
	void Shutdown();

	void Render(void*);

	// Refresh the sphere constant buffer for this frame. Was a file-static free
	// function; promoted to a member so its VulkanMemory + Frame reach-ins route
	// through the injected deps instead of g_xEngine (Wave-4 DI-seam extension).
	void UploadSpheres();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader                m_xShader;
	Flux_Pipeline              m_xPipeline;
	Flux_DynamicConstantBuffer m_xSpheresBuffer;

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real boot
	// path wires them through the Flux_FeatureRegistry SDFs init trampoline.
	Flux_GraphicsImpl* m_pxGraphics = nullptr;
	Flux_HDRImpl*      m_pxHDR      = nullptr;
	Flux_MemoryManager* m_pxVulkanMemory = nullptr;
	FrameContext*                m_pxFrame        = nullptr;
};
