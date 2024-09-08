/******************************************************************************
This file is part of the Newcastle Vulkan Tutorial Series

Author:Rich Davison
Contact:richgdavison@gmail.com
License: MIT (see LICENSE file at the top of the source tree)
*//////////////////////////////////////////////////////////////////////////////
#pragma once
#include <vulkan/vulkan.hpp>
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"

struct Flux_BlendState;
struct Flux_TargetSetup;

class Zenith_Vulkan_Shader
{
public:
	Zenith_Vulkan_Shader() = default;

	void Initialise(const std::string& strVertex, const std::string& strFragment, const std::string& strGeometry = "", const std::string& strDomain = "", const std::string& strHull = "");
	~Zenith_Vulkan_Shader();

	//credit Rich Davison
	void FillShaderStageCreateInfo(vk::GraphicsPipelineCreateInfo& xPipelineCreateInfo) const;
	vk::PipelineShaderStageCreateInfo* m_xInfos = nullptr;

private:
	vk::ShaderModule CreateShaderModule(const char* szCode, uint64_t ulCodeLength);

	bool m_bTesselation = false;
	uint32_t m_uStageCount = 0;

	char* m_pcVertShaderCode = nullptr;
	char* m_pcFragShaderCode = nullptr;
	char* m_pcTescShaderCode = nullptr;
	char* m_pcTeseShaderCode = nullptr;

	uint64_t m_pcVertShaderCodeSize = 0;
	uint64_t m_pcFragShaderCodeSize = 0;
	uint64_t m_pcTescShaderCodeSize = 0;
	uint64_t m_pcTeseShaderCodeSize = 0;

	vk::ShaderModule m_xVertShaderModule;
	vk::ShaderModule m_xFragShaderModule;
	vk::ShaderModule m_xTescShaderModule;
	vk::ShaderModule m_xTeseShaderModule;
};

//#TO_TODO: there should be a platform independent version of this
struct Zenith_Vulkan_PipelineSpecification {
	Zenith_Vulkan_PipelineSpecification(Flux_VertexInputDescription xVertexInputDesc, Zenith_Vulkan_Shader* pxShader, std::vector<Flux_BlendState> xBlendStates, bool bDepthTestEnabled, bool bDepthWriteEnabled, DepthCompareFunc eDepthCompareFunc, DepthStencilFormat eDepthFormat, bool bUsePushConstants, bool bUseTesselation, std::array<uint32_t, DESCRIPTOR_TYPE_MAX> xPerFrameBindings, std::array<uint32_t, DESCRIPTOR_TYPE_MAX> xPerDrawBindings, Flux_TargetSetup& xTargetSetup);

	Zenith_Vulkan_Shader* m_pxShader;

	std::vector<Flux_BlendState> m_xBlendStates;

	bool m_bDepthTestEnabled;
	bool m_bDepthWriteEnabled;
	DepthCompareFunc m_eDepthCompareFunc;
	DepthStencilFormat m_eDepthStencilFormat;
	bool m_bUsePushConstants;
	bool m_bUseTesselation;

	std::array<uint32_t, DESCRIPTOR_TYPE_MAX> m_xPerFrameBindings;
	std::array<uint32_t, DESCRIPTOR_TYPE_MAX> m_xPerDrawBindings;

	Flux_VertexInputDescription m_eVertexInputDesc;

	Flux_TargetSetup& m_xTargetSetup;
	LoadAction m_eColourLoadAction;
	StoreAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	StoreAction m_eDepthStencilStoreAction;
};

class Zenith_Vulkan_Pipeline {
public:
	vk::Pipeline m_xPipeline;
	vk::PipelineLayout	m_xPipelineLayout;

	~Zenith_Vulkan_Pipeline();

	vk::DescriptorSetLayout m_xPerFrameLayout;
	vk::DescriptorSet m_axPerFrameSets[MAX_FRAMES_IN_FLIGHT];

	vk::DescriptorSetLayout m_xPerDrawLayout;
	vk::DescriptorSet m_axPerDrawSets[MAX_FRAMES_IN_FLIGHT];

	bool m_bUsePushConstants = false;//#TODO expand on this, currently just use model matrix

	bool m_bUsesPerDrawDescriptors = false;

	static vk::RenderPass TargetSetupToRenderPass(Flux_TargetSetup& xTargetSetup, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage);
	static vk::Framebuffer TargetSetupToFramebuffer(Flux_TargetSetup& xTargetSetup, const vk::RenderPass& xPass);

	void BindDescriptorSets(vk::CommandBuffer& xCmd, const std::vector<vk::DescriptorSet>& axSets, vk::PipelineBindPoint eBindPoint, uint32_t ufirstSet) const;
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

	Zenith_Vulkan_PipelineBuilder& WithPass(vk::RenderPass& renderPass);

	Zenith_Vulkan_PipelineBuilder& WithDepthStencilFormat(vk::Format combinedFormat);
	Zenith_Vulkan_PipelineBuilder& WithDepthFormat(vk::Format depthFormat);
	Zenith_Vulkan_PipelineBuilder& WithTesselation();

	//Zenith_Vulkan_PipelineBuilder& WithDescriptorBuffers();

	void Build(Zenith_Vulkan_Pipeline& xPipelineOut, const Zenith_Vulkan_PipelineSpecification& xSpec, vk::PipelineCache xCache = {});

	static void FromSpecification(Zenith_Vulkan_Pipeline& xPipelineOut, const Zenith_Vulkan_PipelineSpecification& spec);

protected:
	struct DescriptorThings {
		vk::DescriptorSetLayout xPerFrameLayout;
		vk::DescriptorSet xPerFrameSet;

		vk::DescriptorSetLayout xPerDrawLayout;
		vk::DescriptorSet xPerDrawSet;
	};
	static DescriptorThings HandleDescriptors(const Zenith_Vulkan_PipelineSpecification& spec, Zenith_Vulkan_PipelineBuilder& xBuilder);

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

	std::vector<ColourFormat> m_xAllColourRenderingFormats;
	vk::Format m_xDepthRenderingFormat;
	vk::Format m_xStencilRenderingFormat;

	bool m_bUseTesselation;
	vk::PipelineTessellationStateCreateInfo m_xTesselationCreate;
};
