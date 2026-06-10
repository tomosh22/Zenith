#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

class Flux_CommandList;
class Flux_RenderGraph;

// Phase 9: state + behaviour for SDFs subsystem.
//
// Cross-subsystem dependencies (FluxGraphics for the fullscreen quad + frame
// constants + depth attachment, HDR for the scene render target, VulkanMemory,
// Frame) are reached via g_xEngine at point of use. The non-capturing
// fn-pointer trampoline (the ExecuteSDFs graph callback) cannot capture state,
// so it re-enters via g_xEngine.SDFs() to reach this singleton instance.
class Flux_SDFsImpl
{
public:
	Flux_SDFsImpl() = default;
	~Flux_SDFsImpl() = default;

	Flux_SDFsImpl(const Flux_SDFsImpl&) = delete;
	Flux_SDFsImpl& operator=(const Flux_SDFsImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();

	void Render(void*);

	// Refresh the sphere constant buffer for this frame. Was a file-static free
	// function; VulkanMemory + Frame are reached via g_xEngine at point of use.
	void UploadSpheres();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_Shader                m_xShader;
	Flux_Pipeline              m_xPipeline;
	Flux_DynamicConstantBuffer m_xSpheresBuffer;
};
