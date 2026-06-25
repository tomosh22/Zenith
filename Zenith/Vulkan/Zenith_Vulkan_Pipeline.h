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
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Slang/Flux_ShaderDecl.h"
#include "Core/Zenith_Result.h"

struct Flux_BlendState;
struct Flux_PipelineSpecification;
struct Flux_RenderAttachment;
struct Flux_RenderGraph_AttachmentRef;

class Zenith_Vulkan_Shader
{
public:
	Zenith_Vulkan_Shader() = default;

	// Typed program-ID init. On Windows with the Slang compiler available,
	// compiles the program at runtime; otherwise loads precompiled .spv +
	// .spv.refl artifacts. Asserts the program is registered.
	//
	// Returns void by contract: the backend concept FluxBackendShader
	// (Flux/Backend/Concepts/Flux_Concept_PipelineConstruction.h) pins this
	// signature to `-> std::same_as<void>`. Shaders are non-recoverable — a
	// zero-stage Reset() shader just crashes later at createGraphicsPipeline —
	// so this hard-asserts on failure. Use InitialiseEx if you need the
	// specific failure code (e.g. from a unit test).
	void Initialise(const Flux_ShaderDecl& xDecl);

	// Status-returning sibling of Initialise. Same success path (byte-identical
	// state on success), but surfaces the specific reason on failure:
	//   FILE_NOT_FOUND        — an artifact .spv could not be read (offline path)
	//   SHADER_COMPILE_FAILED — Flux_SlangCompiler::CompileProgram failed (source path)
	//   CORRUPT_DATA          — a compiled/loaded SPIR-V blob was empty
	// Initialise() is a thin wrapper that calls this and hard-asserts IsOk().
	Zenith_Status InitialiseEx(const Flux_ShaderDecl& xDecl);

#ifdef ZENITH_WINDOWS
	// Runtime program-ID compile via Flux_SlangCompiler::CompileProgram.
	// SHADER_COMPILE_FAILED on compile failure, CORRUPT_DATA on empty SPIR-V.
	Zenith_Status InitialiseFromProgramSource(const Flux_ShaderDecl& xDecl);
#endif

	// Offline path: load precompiled .spv + .spv.refl per stage from disk
	// using the program's registry-derived artifact stems. FILE_NOT_FOUND when
	// a .spv read yields null, CORRUPT_DATA on an empty blob.
	Zenith_Status InitialiseFromProgramArtifacts(const Flux_ShaderDecl& xDecl);

	~Zenith_Vulkan_Shader();

	// Destroy GPU shader modules and free SPIR-V code buffers without
	// destroying the C++ object — leaves the shader in the same state as a
	// freshly default-constructed instance, so a subsequent Initialise(...)
	// is leak-free. Used by the hot-reload path.
	void Reset();

	// Get combined reflection data for all shader stages
	const Flux_ShaderReflection& GetReflection() const { return m_xReflection; }

	// Check if reflection data is available
	bool HasReflection() const { return m_xReflection.GetBindings().GetSize() > 0; }

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

private:
	// Combined reflection data from all shader stages
	Flux_ShaderReflection m_xReflection;

	// Helper to merge reflection from multiple stages
	void MergeReflection(const Flux_ShaderReflection& xStageReflection);
};

class Zenith_Vulkan_RootSig
{
public:
	Zenith_Vulkan_RootSig()
	{
		// Initialise all descriptor kinds to UNKNOWN (invalid/unused).
		for (u_int i = 0; i < FLUX_MAX_BINDING_GROUPS; i++)
		{
			for (u_int j = 0; j < FLUX_MAX_BINDINGS_PER_GROUP; j++)
			{
				m_aeBindingKinds[i][j] = FLUX_RESOURCE_KIND_UNKNOWN;
			}
			m_auActiveBindingMask[i] = 0;
		}
	}

	// Get binding location by name (for named resource binding)
	Flux_BindingHandle GetBinding(const char* szName) const
	{
		const Flux_ReflectedBinding* pxBinding = m_xReflection.GetBinding(szName);
		Zenith_Assert(pxBinding != nullptr, "Shader binding '%s' not found in reflection", szName);
		Flux_BindingHandle xHandle;
		if (pxBinding)
		{
			xHandle.m_uSet     = pxBinding->m_uSet;
			xHandle.m_uBinding = pxBinding->m_uBinding;
		}
		return xHandle;
	}

	// Check if reflection data is available for name-based lookups
	bool HasReflection() const { return m_xReflection.GetBindings().GetSize() > 0; }

	vk::PipelineLayout m_xLayout;
	vk::DescriptorSetLayout m_axDescSetLayouts[FLUX_MAX_BINDING_GROUPS];
	// Per-set ownership flag — false means the layout is borrowed (e.g. the
	// shared bindless texture layout from Zenith_Vulkan), so Pipeline::Reset
	// must NOT destroy it. True means we created it via vkCreateDescriptor-
	// SetLayout and own its lifetime.
	bool m_abOwnsDescSetLayout[FLUX_MAX_BINDING_GROUPS] = {};
	// Per-(set, binding) resource kind, mirrored from the pipeline layout so the
	// descriptor-write path can pick the right descriptor type without the
	// reflection. UNKNOWN = absent. Indexed [set][binding].
	FluxResourceKind m_aeBindingKinds[FLUX_MAX_BINDING_GROUPS][FLUX_MAX_BINDINGS_PER_GROUP];
	// Per-set bitmask of present bindings (bit b set ⇒ binding b is declared).
	// Drives the pre-draw staged-binding validator.
	u_int m_auActiveBindingMask[FLUX_MAX_BINDING_GROUPS] = {};
	u_int m_uNumBindingGroups = UINT32_MAX;

	// Reflection data for name-based binding lookups
	Flux_ShaderReflection m_xReflection;
};

class Zenith_Vulkan_Pipeline
{
public:
	vk::Pipeline m_xPipeline;
	vk::RenderPass m_xRenderPass;
	Zenith_Vulkan_RootSig m_xRootSig;

	~Zenith_Vulkan_Pipeline();

	// Destroy the vk::Pipeline, render pass, and root-signature handles
	// without destroying the C++ object — leaves the pipeline in its
	// default-constructed state ready for re-population by another
	// FromSpecification / BuildFromShader. Used by the hot-reload path.
	void Reset();

	

	static vk::RenderPass TargetSetupToRenderPass(const TextureFormat* aeColourFormats, uint32_t uNumColourAttachments, TextureFormat eDepthStencilFormat, LoadAction eColourLoad, StoreAction eColourStore, LoadAction eDepthStencilLoad, StoreAction eDepthStencilStore, RenderTargetUsage eUsage, bool bDepthIsReadOnly = false);
	static vk::Framebuffer TargetSetupToFramebuffer(const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColourAttachments, const Flux_RenderGraph_AttachmentRef& xDepthStencil, uint32_t uWidth, uint32_t uHeight, const vk::RenderPass& xPass);
};

class Zenith_Vulkan_RootSigBuilder
{
public:
	// Build from manual specification (existing method)
	static void FromSpecification(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_PipelineLayout& xSpec);

	// Build from shader reflection data (auto-generates layout)
	static void FromReflection(Zenith_Vulkan_RootSig& xRootSigOut, const Flux_ShaderReflection& xReflection);
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

	Zenith_Vulkan_PipelineBuilder& WithDescriptorSetLayout(vk::DescriptorSetLayout layout);

	Zenith_Vulkan_PipelineBuilder& WithPass(vk::RenderPass renderPass);

	Zenith_Vulkan_PipelineBuilder& WithDepthStencilFormat(vk::Format combinedFormat);
	Zenith_Vulkan_PipelineBuilder& WithDepthFormat(vk::Format depthFormat);
	Zenith_Vulkan_PipelineBuilder& WithTesselation();

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

	// One-call helper — combines WithShader + WithLayout + Build + assigning
	// the root signature into the pipeline's m_xRootSig slot. Engine code
	// uses this rather than reaching into Flux_RootSig::m_xLayout (vk type)
	// or assigning Flux_Pipeline::m_xRootSig directly.
	static void BuildFromShader(Zenith_Vulkan_Pipeline& xPipelineOut,
	                            const Zenith_Vulkan_Shader& xShader,
	                            const Zenith_Vulkan_RootSig& xRootSig);

private:
	const Zenith_Vulkan_Shader* m_pxShader = nullptr;
	vk::PipelineLayout m_xLayout;
};
