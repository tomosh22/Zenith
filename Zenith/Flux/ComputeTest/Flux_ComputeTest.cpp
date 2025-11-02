#include "Zenith.h"

#include "Flux/ComputeTest/Flux_ComputeTest.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Static members
static Flux_RenderAttachment g_xComputeOutput;
static Flux_TargetSetup g_xDisplayTargetSetup;
static Flux_CommandList g_xComputeCommandList("Compute Test - Compute");
static Flux_CommandList g_xDisplayCommandList("Compute Test - Display");
static Zenith_Vulkan_Pipeline g_xComputePipeline;
static Zenith_Vulkan_Pipeline g_xDisplayPipeline;
static Zenith_Vulkan_Shader g_xComputeShader;
static Zenith_Vulkan_Shader g_xDisplayShader;
static Zenith_Vulkan_RootSig g_xComputeRootSig;

void Flux_ComputeTest::Initialise()
{
	Zenith_Log("Flux_ComputeTest::Initialise() - Starting");
	
	// Create output texture for compute shader (RGBA8, storage image)
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = 1920;
	xBuilder.m_uHeight = 1080;
	xBuilder.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__UNORDERED_ACCESS | 1u << MEMORY_FLAGS__SHADER_READ;

	xBuilder.BuildColour(g_xComputeOutput, "Compute Test Output");
	
	// ========== COMPUTE PIPELINE SETUP ==========
	g_xComputeShader.InitialiseCompute("ComputeTest/ComputeTest.comp");
	
	Zenith_Log("Flux_ComputeTest - Loaded compute shader");
	
	// Build compute root signature
	Flux_PipelineLayout xComputeLayout;
	xComputeLayout.m_uNumDescriptorSets = 1;
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_STORAGE_IMAGE;
	xComputeLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_MAX;
	Zenith_Vulkan_RootSigBuilder::FromSpecification(g_xComputeRootSig, xComputeLayout);
	
	// Build compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xComputeBuilder;
	xComputeBuilder.WithShader(g_xComputeShader)
		.WithLayout(g_xComputeRootSig.m_xLayout)
		.Build(g_xComputePipeline);
	
	g_xComputePipeline.m_xRootSig = g_xComputeRootSig;
	
	Zenith_Log("Flux_ComputeTest - Built compute pipeline");
	
#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddTexture({ "Compute Test", "Output Texture" }, g_xComputeOutput.m_pxSRV);
#endif
}

void Flux_ComputeTest::Run()
{
	RunComputePass();
	// TODO: Implement display pass once we can properly construct Flux_CommandDraw
	//RunDisplayPass();
}

void Flux_ComputeTest::RunComputePass()
{
	g_xComputeCommandList.Reset(false);
	
	g_xComputeCommandList.AddCommand<Flux_CommandBindComputePipeline>(&g_xComputePipeline);
	g_xComputeCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xComputeCommandList.AddCommand<Flux_CommandBindUAV>(&g_xComputeOutput.m_pxUAV, 0);

	g_xComputeCommandList.AddCommand<Flux_CommandPushConstant>(&Flux_Graphics::s_xFrameConstants.m_xScreenDims, sizeof(Flux_Graphics::s_xFrameConstants.m_xScreenDims));
	// Dispatch compute shader: (width/8, height/8, 1) workgroups for 8x8 local size
	g_xComputeCommandList.AddCommand<Flux_CommandDispatch>(Flux_Graphics::s_xFrameConstants.m_xScreenDims.x / 8, Flux_Graphics::s_xFrameConstants.m_xScreenDims.y / 8, 1);
	
	Flux::SubmitCommandList(&g_xComputeCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_COMPUTE_TEST);
	
	static bool bFirstRun = true;
	if (bFirstRun)
	{
		Zenith_Log("Flux_ComputeTest - Submitted compute command list");
		bFirstRun = false;
	}
}

void Flux_ComputeTest::RunDisplayPass()
{
	// TODO: Implement display pass - currently disabled
	/*
	g_xDisplayCommandList.Reset(true);
	g_xDisplayCommandList.AddCommand<Flux_CommandSetPipeline>(&g_xDisplayPipeline);
	g_xDisplayCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xDisplayCommandList.AddCommand<Flux_CommandBindTexture>(&g_xComputeOutputTexture, 0);
	
	// Draw fullscreen triangle (3 vertices) - need to transition back to GENERAL for next compute pass
	g_xDisplayCommandList.AddCommand<Flux_CommandImageBarrier>(
		&g_xComputeOutputTexture,
		(uint32_t)vk::ImageLayout::eShaderReadOnlyOptimal,
		(uint32_t)vk::ImageLayout::eGeneral
	);
	
	Flux::SubmitCommandList(&g_xDisplayCommandList, g_xDisplayTargetSetup, RENDER_ORDER_COMPUTE_TEST);
	*/
}

Flux_RenderAttachment& Flux_ComputeTest::GetComputeOutputTexture()
{
	return g_xComputeOutput;
}

/*
================================================================================
IMPLEMENTATION NOTES:
================================================================================

This compute test implementation requires several additions to the Vulkan
infrastructure. Here's what needs to be added:

1. ZENITH_VULKAN_SHADER (Zenith_Vulkan_Shader class):
   - Add member: vk::ShaderModule m_xCompShaderModule
   - Add method: void InitialiseCompute(const std::string& strCompute)
   - Load .comp.spv file and create shader module for compute stage

2. ZENITH_VULKAN_PIPELINE (Compute pipeline support):
   - Add Zenith_Vulkan_ComputePipelineBuilder class
   - Use vk::ComputePipelineCreateInfo instead of GraphicsPipelineCreateInfo
   - Create pipeline with vkCreateComputePipelines instead of Graphics version
   
3. ZENITH_VULKAN_COMMANDBUFFER (Dispatch support):
   - Add method: void Dispatch(uint32_t groupCountX, Y, Z)
   - Calls vkCmdDispatch(m_xCmdBuffer, x, y, z)
   - Add method: void BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline)
   - Uses VK_PIPELINE_BIND_POINT_COMPUTE instead of GRAPHICS

4. FLUX_TEXTURE (Storage image support):
   - Add bool m_bStorage to Flux_TextureBuilder
   - When m_bStorage is true, add VK_IMAGE_USAGE_STORAGE_BIT to usage flags
   - Set initial layout to VK_IMAGE_LAYOUT_GENERAL for compute access

5. FLUX_COMMANDLIST (Compute commands):
   - Add Flux_CommandBindComputePipeline command type
   - Add Flux_CommandBindStorageImage command type
   - Add Flux_CommandDispatch command type
   - Add Flux_CommandImageBarrier for layout transitions

6. IMAGE BARRIER SUPPORT:
   - Add method to command buffer for vkCmdPipelineBarrier
   - Handle image layout transitions between compute and graphics stages
   - Example:
     vk::ImageMemoryBarrier barrier;
     barrier.oldLayout = vk::ImageLayout::eGeneral;
     barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
     barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
     barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
     barrier.image = texture->GetImage();
     // ... set subresource range ...
     cmdBuffer.pipelineBarrier(
         vk::PipelineStageFlagBits::eComputeShader,
         vk::PipelineStageFlagBits::eFragmentShader,
         {}, 0, nullptr, 0, nullptr, 1, &barrier);

7. INTEGRATION (Flux_Graphics.cpp):
   - Call Flux_ComputeTest::Initialise() in Flux_Graphics::Initialise()
   - Call Flux_ComputeTest::Run() in Flux_Graphics::Render()

8. SHADER COMPILATION:
   - Compile shaders with glslc or glslangValidator:
     glslc ComputeTest.comp -o ComputeTest.comp.spv
     glslc ComputeTest_Display.vert -o ComputeTest_Display.vert.spv
     glslc ComputeTest_Display.frag -o ComputeTest_Display.frag.spv

================================================================================
EXPECTED BEHAVIOR:
================================================================================

When working correctly:
1. Compute shader dispatches and writes UV coordinates to the output texture
   - Red channel = X coordinate (0 at left, 1 at right)
   - Green channel = Y coordinate (0 at top, 1 at bottom)
   - Blue channel = 0
   - Alpha channel = 1

2. Display pass reads the compute output texture and renders it to a render target
   - Should show a gradient from black (top-left) to yellow (bottom-right)
   - Red increases left to right
   - Green increases top to bottom

3. This render target can then be copied to the framebuffer or used in subsequent passes

================================================================================
*/
