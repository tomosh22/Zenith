#include "Zenith.h"

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

	vk::PipelineShaderStageCreateInfo stageInfo = vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_pxShader->m_xCompShaderModule)
		.setPName("main");

	vk::ComputePipelineCreateInfo pipelineInfo = vk::ComputePipelineCreateInfo()
		.setStage(stageInfo)
		.setLayout(m_xLayout);

	vk::Result result = Zenith_Vulkan::GetDevice().createComputePipelines(
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
