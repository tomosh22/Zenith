/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"

struct Flux_BlendState;
struct Flux_TargetSetup;

class Zenith_Vulkan_Shader
{
public:
	Zenith_Vulkan_Shader() = default;

	void Initialise(const std::string& strVertex, const std::string& strFragment, const std::string& strGeometry = "", const std::string& strDomain = "", const std::string& strHull = "");
	void InitialiseCompute(const std::string& strCompute);
	~Zenith_Vulkan_Shader();

	//credit Rich Davison
	void FillShaderStageCreateInfo(vk::GraphicsPipelineCreateInfo& xPipelineCreateInfo) const;
	vk::PipelineShaderStageCreateInfo* m_xInfos = nullptr;

//private:
	vk::ShaderModule CreateShaderModule(const char* szCode, uint64_t ulCodeLength);

	bool m_bTesselation = false;
	uint32_t m_uStageCount = 0;

	char* m_pcVertShaderCode = nullptr;
	char* m_pcFragShaderCode = nullptr;
	char* m_pcTescShaderCode = nullptr;
	char* m_pcTeseShaderCode = nullptr;
	char* m_pcCompShaderCode = nullptr;

	uint64_t m_pcVertShaderCodeSize = 0;
	uint64_t m_pcFragShaderCodeSize = 0;
	uint64_t m_pcTescShaderCodeSize = 0;
	uint64_t m_pcTeseShaderCodeSize = 0;
	uint64_t m_pcCompShaderCodeSize = 0;

	vk::ShaderModule m_xVertShaderModule;
	vk::ShaderModule m_xFragShaderModule;
	vk::ShaderModule m_xTescShaderModule;
	vk::ShaderModule m_xTeseShaderModule;
	vk::ShaderModule m_xCompShaderModule;
};

class Zenith_Vulkan_RootSig
{
public:
	Zenith_Vulkan_RootSig()
	{
		// Initialize all descriptor types to MAX (invalid/unused)
		for (u_int i = 0; i < FLUX_MAX_DESCRIPTOR_BINDINGS; i++)
		{
			for (u_int j = 0; j < FLUX_MAX_DESCRIPTOR_BINDINGS; j++)
			{
				m_axDescriptorTypes[i][j] = DESCRIPTOR_TYPE_MAX;
			}
		}
	}
	
	vk::PipelineLayout m_xLayout;
	vk::DescriptorSetLayout m_axDescSetLayouts[FLUX_MAX_DESCRIPTOR_BINDINGS];
	DescriptorType m_axDescriptorTypes[FLUX_MAX_DESCRIPTOR_BINDINGS][FLUX_MAX_DESCRIPTOR_BINDINGS];
	u_int m_uNumDescriptorSets = UINT32_MAX;
};

class Zenith_Vulkan_Pipeline
{
public:
	vk::Pipeline m_xPipeline;
	vk::RenderPass m_xRenderPass;
	Zenith_Vulkan_RootSig m_xRootSig;

	~Zenith_Vulkan_Pipeline();

	

	static vk::RenderPass TargetSetupToRenderPass(Flux_TargetSetup& xTargetSetup, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage);
	static vk::Framebuffer TargetSetupToFramebuffer(Flux_TargetSetup& xTargetSetup, uint32_t uWidth, uint32_t uHeight, const vk::RenderPass& xPass);
};

class Zenith_Vulkan_RootSigBuilder
{
public:
	static void FromSpecification(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_PipelineLayout& xSpec);
};

class Zenith_Vulkan_PipelineBuilder {
public:
	Zenith_Vulkan_PipelineBuilder();
	~Zenith_Vulkan_PipelineBuilder() {}

	Zenith_Vulkan_PipelineBuilder& WithDepthState(vk::CompareOp op, bool depthEnabled, bool writeEnabled, bool stencilEnabled = false);

	Zenith_Vulkan_PipelineBuilder& WithBlendState(vk::BlendFactor srcState, vk::BlendFactor dstState, bool enabled = true);

	Zenith_Vulkan_PipelineBuilder& WithRaster(vk::CullModeFlagBits cullMode, vk::PolygonMode polyMode = vk::PolygonMode::eFill);

	Zenith_Vulkan_PipelineBuilder& WithVertexInputState(const vk::PipelineVertexInputStateCreateInfo& spec);

	Zenith_Vulkan_PipelineBuilder& WithTopology(vk::PrimitiveTopology topology);

	Zenith_Vulkan_PipelineBuilder& WithShader(const Zenith_Vulkan_Shader& shader);

	Zenith_Vulkan_PipelineBuilder& WithLayout(vk::PipelineLayout layout);

	Zenith_Vulkan_PipelineBuilder& WithPushConstant(vk::ShaderStageFlags flags, uint32_t offset);

	Zenith_Vulkan_PipelineBuilder& WithDescriptorSetLayout(uint32_t slot, vk::DescriptorSetLayout layout);

	Zenith_Vulkan_PipelineBuilder& WithPass(vk::RenderPass renderPass);

	Zenith_Vulkan_PipelineBuilder& WithDepthStencilFormat(vk::Format combinedFormat);
	Zenith_Vulkan_PipelineBuilder& WithDepthFormat(vk::Format depthFormat);
	Zenith_Vulkan_PipelineBuilder& WithTesselation();

	void Build(Zenith_Vulkan_Pipeline& xPipelineOut, const struct Flux_PipelineSpecification& xSpec, vk::PipelineCache xCache = {});

	static void FromSpecification(Zenith_Vulkan_Pipeline& xPipelineOut, const Flux_PipelineSpecification& xSpec);

protected:

	vk::GraphicsPipelineCreateInfo				m_xPipelineCreate;
	vk::PipelineCacheCreateInfo					m_xCacheCreate;
	vk::PipelineInputAssemblyStateCreateInfo	m_xInputAsmCreate;
	vk::PipelineRasterizationStateCreateInfo	m_xRasterCreate;
	vk::PipelineColorBlendStateCreateInfo		m_xBlendCreate;
	vk::PipelineDepthStencilStateCreateInfo		m_xDepthStencilCreate;
	vk::PipelineViewportStateCreateInfo			m_xViewportCreate;
	vk::PipelineMultisampleStateCreateInfo		m_xSampleCreate;
	vk::PipelineDynamicStateCreateInfo			m_xDynamicCreate;
	vk::PipelineVertexInputStateCreateInfo		m_xVertexCreate;
	vk::PipelineLayout m_xPipelineLayout;

	std::vector< vk::PipelineColorBlendAttachmentState>			m_xBlendAttachStates;

	vk::DynamicState m_axDynamicStateEnables[2];

	std::vector< vk::DescriptorSetLayout> m_xAllLayouts;
	std::vector< vk::PushConstantRange> m_xAllPushConstants;

	std::vector<TextureFormat> m_xAllColourRenderingFormats;
	vk::Format m_xDepthRenderingFormat;
	vk::Format m_xStencilRenderingFormat;

	bool m_bUseTesselation;
	vk::PipelineTessellationStateCreateInfo m_xTesselationCreate;
};

class Zenith_Vulkan_ComputePipelineBuilder
{
public:
	Zenith_Vulkan_ComputePipelineBuilder();
	
	Zenith_Vulkan_ComputePipelineBuilder& WithShader(const Zenith_Vulkan_Shader& shader);
	Zenith_Vulkan_ComputePipelineBuilder& WithLayout(vk::PipelineLayout layout);
	
	void Build(Zenith_Vulkan_Pipeline& pipelineOut);
	
private:
	const Zenith_Vulkan_Shader* m_pxShader = nullptr;
	vk::PipelineLayout m_xLayout;
};
