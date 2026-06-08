#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Pipeline.h"
#include "Zenith_Vulkan.h"

//==========================================================================
// Zenith_Vulkan_ComputePipelineBuilder
//
// Carved out of Zenith_Vulkan_Pipeline.cpp. Compute pipeline construction is
// independent of the graphics pipeline builder — it needs only a shader
// module and a descriptor-set layout to call vkCreateComputePipelines.
//==========================================================================

Zenith_Vulkan_ComputePipelineBuilder::Zenith_Vulkan_ComputePipelineBuilder()
{
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithShader(const Zenith_Vulkan_Shader& shader)
{
	m_pxShader = &shader;
	return *this;
}

Zenith_Vulkan_ComputePipelineBuilder& Zenith_Vulkan_ComputePipelineBuilder::WithLayout(vk::PipelineLayout layout)
{
	m_xLayout = layout;
	return *this;
}

void Zenith_Vulkan_ComputePipelineBuilder::Build(Zenith_Vulkan_Pipeline& pipelineOut)
{
	Zenith_Assert(m_pxShader != nullptr, "Compute shader not set");
	Zenith_Assert(m_xLayout != VK_NULL_HANDLE, "Pipeline layout not set");

	// Hot-reload chain: callers (typically a subsystem's BuildPipelines)
	// rebuild the external Flux_RootSig via Flux_RootSigBuilder::FromReflection
	// BEFORE invoking BuildFromShader. That gives the external rootsig a
	// fresh vk::PipelineLayout while pipelineOut.m_xRootSig still carries
	// the previous frame's COPY (with the OLD handle). pipelineOut.Reset()
	// then destroys the OLD handle, the new pipeline is created with
	// m_xLayout (the NEW handle from the external rootsig, captured by
	// WithLayout above), and finally BuildFromShader stamps the external
	// rootsig back into pipelineOut. The aliasing check below catches
	// callers that violate the chain — i.e. invoke Build without first
	// rebuilding the external rootsig — which would leave m_xLayout pointing
	// at the same handle Reset is about to destroy (use-after-free in
	// createComputePipelines).
	Zenith_Assert(
		pipelineOut.m_xRootSig.m_xLayout == VK_NULL_HANDLE ||
		pipelineOut.m_xRootSig.m_xLayout != m_xLayout,
		"ComputePipelineBuilder::Build: prior pipeline-layout handle aliases "
		"the layout being used for the new pipeline. Caller must rebuild the "
		"external Flux_RootSig (via FromReflection) between successive Build "
		"calls.");
	pipelineOut.Reset();

	vk::PipelineShaderStageCreateInfo stageInfo = vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_pxShader->m_xCompShaderModule)
		.setPName("main");

	vk::ComputePipelineCreateInfo pipelineInfo = vk::ComputePipelineCreateInfo()
		.setStage(stageInfo)
		.setLayout(m_xLayout);

	vk::Result result = g_xEngine.FluxBackend().GetDevice().createComputePipelines(
		VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineOut.m_xPipeline);

	if (result != vk::Result::eSuccess)
	{
		Zenith_Error(LOG_CATEGORY_VULKAN, "Failed to create compute pipeline");
	}
}

void Zenith_Vulkan_ComputePipelineBuilder::BuildFromShader(Zenith_Vulkan_Pipeline& xPipelineOut,
                                                           const Zenith_Vulkan_Shader& xShader,
                                                           const Zenith_Vulkan_RootSig& xRootSig)
{
	Zenith_Vulkan_ComputePipelineBuilder xBuilder;
	xBuilder.WithShader(xShader)
	        .WithLayout(xRootSig.m_xLayout)
	        .Build(xPipelineOut);
	// Centralise the two-step build + root-sig stamp so every compute-pipeline
	// construction site calls a single helper instead of manually chaining
	// WithShader/WithLayout/Build and then remembering to copy the root sig.
	xPipelineOut.m_xRootSig = xRootSig;
}
