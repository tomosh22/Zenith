#include "Zenith.h"

#include "Zenith_Vulkan_Pipeline.h"
#include "Zenith_Vulkan.h"
#include "Flux/Slang/Flux_ShaderRegistry.h"

#ifdef ZENITH_WINDOWS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

//==========================================================================
// Zenith_Vulkan_Shader — compilation, SPV module creation, reflection merge
//
// Carved out of Zenith_Vulkan_Pipeline.cpp. This subsystem has no dependency
// on the pipeline builder or descriptor layouts — it only speaks to
// Flux_SlangCompiler (for runtime builds on Windows) and Zenith_Vulkan's
// device handle for creating vk::ShaderModules.
//==========================================================================

void Zenith_Vulkan_Shader::Initialise(FluxShaderProgram eProgram)
{
	// Idempotent: free any previously-loaded SPIR-V code, modules, and
	// reflection so the hot-reload path can re-call Initialise on the same
	// shader instance without leaking GPU handles.
	Reset();

#ifdef ZENITH_WINDOWS
	if (Flux_SlangCompiler::IsInitialised())
	{
		bool bSuccess = InitialiseFromProgramSource(eProgram);
		Zenith_Assert(bSuccess, "Slang program compile failed for FluxShaderProgram=%u",
					  static_cast<u_int>(eProgram));
		return;
	}
#endif
	bool bSuccess = InitialiseFromProgramArtifacts(eProgram);
	Zenith_Assert(bSuccess, "Failed to load precompiled artifacts for FluxShaderProgram=%u",
				  static_cast<u_int>(eProgram));
}

bool Zenith_Vulkan_Shader::InitialiseFromProgramArtifacts(FluxShaderProgram eProgram)
{
	const Flux_ShaderRegistryEntry& xEntry = Flux_ShaderRegistry::GetProgram(eProgram);
	std::string strRoot(SHADER_SOURCE_ROOT);

	// Graphics-program path
	if (xEntry.m_szVertexEntry && xEntry.m_szFragmentEntry)
	{
		std::string strVStem = Flux_ShaderRegistry::GetVertexArtifactStem(eProgram);
		std::string strFStem = Flux_ShaderRegistry::GetFragmentArtifactStem(eProgram);

		m_pcVertShaderCode = Zenith_FileAccess::ReadFile((strRoot + strVStem + ".spv").c_str(), m_pcVertShaderCodeSize);
		m_pcFragShaderCode = Zenith_FileAccess::ReadFile((strRoot + strFStem + ".spv").c_str(), m_pcFragShaderCodeSize);
		if (!m_pcVertShaderCode || !m_pcFragShaderCode) return false;

		m_xVertShaderModule = CreateShaderModule(m_pcVertShaderCode, m_pcVertShaderCodeSize);
		m_xFragShaderModule = CreateShaderModule(m_pcFragShaderCode, m_pcFragShaderCodeSize);
		m_uStageCount = 2;

		// Slang's SPIR-V emitter renames each entry point to "main" when each
		// .spv blob contains only that one entry point (the default). The
		// source-level entry name (xEntry.m_szVertexEntry / m_szFragmentEntry)
		// is what we passed to findEntryPointByName / what reflection keys on,
		// but the Vulkan-visible OpEntryPoint name is "main". Multi-entry-per-
		// module SPIR-V emission would need a different Slang option (see
		// VulkanUseEntryPointName); revisit when subsystem mega-files want it.
		m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];
		m_xInfos[0].stage  = vk::ShaderStageFlagBits::eVertex;
		m_xInfos[0].module = m_xVertShaderModule;
		m_xInfos[0].pName  = "main";
		m_xInfos[1].stage  = vk::ShaderStageFlagBits::eFragment;
		m_xInfos[1].module = m_xFragShaderModule;
		m_xInfos[1].pName  = "main";

		Zenith_DataStream xVRefl;
		xVRefl.ReadFromFile((strRoot + strVStem + ".spv.refl").c_str());
		if (xVRefl.IsValid())
		{
			Flux_ShaderReflection x;
			x.ReadFromDataStream(xVRefl);
			MergeReflection(x);
		}
		Zenith_DataStream xFRefl;
		xFRefl.ReadFromFile((strRoot + strFStem + ".spv.refl").c_str());
		if (xFRefl.IsValid())
		{
			Flux_ShaderReflection x;
			x.ReadFromDataStream(xFRefl);
			MergeReflection(x);
		}
		return true;
	}

	// Compute-program path
	if (xEntry.m_szComputeEntry)
	{
		std::string strCStem = Flux_ShaderRegistry::GetComputeArtifactStem(eProgram);
		m_pcCompShaderCode = Zenith_FileAccess::ReadFile((strRoot + strCStem + ".spv").c_str(), m_pcCompShaderCodeSize);
		if (!m_pcCompShaderCode) return false;
		m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
		m_uStageCount = 1;

		// Populate m_xInfos so FillShaderStageCreateInfo works symmetrically
		// for compute (current callers go through ComputePipelineBuilder which
		// reads m_xCompShaderModule directly, but a future caller using the
		// generic stage-info API would otherwise see a null pStages pointer).
		m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];
		m_xInfos[0].stage  = vk::ShaderStageFlagBits::eCompute;
		m_xInfos[0].module = m_xCompShaderModule;
		m_xInfos[0].pName  = "main";

		Zenith_DataStream xRefl;
		xRefl.ReadFromFile((strRoot + strCStem + ".spv.refl").c_str());
		if (xRefl.IsValid()) m_xReflection.ReadFromDataStream(xRefl);
		return true;
	}

	return false;
}

#ifdef ZENITH_WINDOWS
bool Zenith_Vulkan_Shader::InitialiseFromProgramSource(FluxShaderProgram eProgram)
{
	const Flux_ShaderRegistryEntry& xEntry = Flux_ShaderRegistry::GetProgram(eProgram);

	Flux_SlangProgramDesc xDesc;
	Flux_ShaderRegistry::DescribeProgram(eProgram, xDesc);

	Flux_SlangProgramResult xResult;
	if (!Flux_SlangCompiler::CompileProgram(xDesc, xResult))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "CompileProgram failed for '%s': %s",
				   xEntry.m_szName, xResult.m_strError.c_str());
		return false;
	}

	const bool bGraphics = xEntry.m_szVertexEntry && xEntry.m_szFragmentEntry;
	if (bGraphics)
	{
		m_pcVertShaderCodeSize = xResult.m_axVertexSpirv.GetSize() * sizeof(uint32_t);
		m_pcVertShaderCode     = new char[m_pcVertShaderCodeSize];
		memcpy(m_pcVertShaderCode, xResult.m_axVertexSpirv.GetDataPointer(), m_pcVertShaderCodeSize);

		m_pcFragShaderCodeSize = xResult.m_axFragmentSpirv.GetSize() * sizeof(uint32_t);
		m_pcFragShaderCode     = new char[m_pcFragShaderCodeSize];
		memcpy(m_pcFragShaderCode, xResult.m_axFragmentSpirv.GetDataPointer(), m_pcFragShaderCodeSize);

		m_xVertShaderModule = CreateShaderModule(m_pcVertShaderCode, m_pcVertShaderCodeSize);
		m_xFragShaderModule = CreateShaderModule(m_pcFragShaderCode, m_pcFragShaderCodeSize);
		m_uStageCount = 2;

		// Slang renames per-entry SPIR-V emissions to "main" — see comment in
		// the offline init path for the longer rationale.
		m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];
		m_xInfos[0].stage  = vk::ShaderStageFlagBits::eVertex;
		m_xInfos[0].module = m_xVertShaderModule;
		m_xInfos[0].pName  = "main";
		m_xInfos[1].stage  = vk::ShaderStageFlagBits::eFragment;
		m_xInfos[1].module = m_xFragShaderModule;
		m_xInfos[1].pName  = "main";
	}
	else if (xEntry.m_szComputeEntry)
	{
		m_pcCompShaderCodeSize = xResult.m_axComputeSpirv.GetSize() * sizeof(uint32_t);
		m_pcCompShaderCode     = new char[m_pcCompShaderCodeSize];
		memcpy(m_pcCompShaderCode, xResult.m_axComputeSpirv.GetDataPointer(), m_pcCompShaderCodeSize);
		m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
		m_uStageCount = 1;

		// See offline init path for rationale.
		m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];
		m_xInfos[0].stage  = vk::ShaderStageFlagBits::eCompute;
		m_xInfos[0].module = m_xCompShaderModule;
		m_xInfos[0].pName  = "main";
	}

	MergeReflection(xResult.m_xReflection);
	return true;
}
#endif

// Legacy Initialise(strVert, strFrag, ...) and InitialiseCompute(strCompute)
// were removed when the Slang migration completed. Engine code now uses
// Initialise(FluxShaderProgram) + the registry; offline path goes through
// InitialiseFromProgramArtifacts (Android), runtime through
// InitialiseFromProgramSource (Windows). Tessellation has no callers
// post-migration; reintroduce when an actual user lands.

Zenith_Vulkan_Shader::~Zenith_Vulkan_Shader()
{
	Reset();
}

void Zenith_Vulkan_Shader::Reset()
{
	const vk::Device xDevice = Zenith_Vulkan::GetDevice();

	vk::ShaderModule* apxModules[] = { &m_xVertShaderModule, &m_xFragShaderModule, &m_xTescShaderModule, &m_xTeseShaderModule, &m_xCompShaderModule };
	for (vk::ShaderModule* pxModule : apxModules)
	{
		if (*pxModule)
		{
			xDevice.destroyShaderModule(*pxModule);
			*pxModule = VK_NULL_HANDLE;
		}
	}

	char** appcCodes[] = { &m_pcVertShaderCode, &m_pcFragShaderCode, &m_pcTescShaderCode, &m_pcTeseShaderCode, &m_pcCompShaderCode };
	for (char** ppcCode : appcCodes)
	{
		delete[] *ppcCode;
		*ppcCode = nullptr;
	}

	uint64_t* apulSizes[] = { &m_pcVertShaderCodeSize, &m_pcFragShaderCodeSize, &m_pcTescShaderCodeSize, &m_pcTeseShaderCodeSize, &m_pcCompShaderCodeSize };
	for (uint64_t* pulSize : apulSizes) *pulSize = 0;

	delete[] m_xInfos;
	m_xInfos = nullptr;
	m_uStageCount = 0;

	m_xReflection = Flux_ShaderReflection();
}

void Zenith_Vulkan_Shader::FillShaderStageCreateInfo(vk::GraphicsPipelineCreateInfo& xPipelineCreateInfo) const
{
	xPipelineCreateInfo.setStageCount(m_uStageCount);
	xPipelineCreateInfo.setPStages(m_xInfos);
}

vk::ShaderModule Zenith_Vulkan_Shader::CreateShaderModule(const char* szCode, uint64_t ulCodeLength)
{
	Zenith_Assert(ulCodeLength, "Shader code is empty");
	vk::ShaderModuleCreateInfo xCreateInfo = vk::ShaderModuleCreateInfo()
		.setCodeSize(ulCodeLength)
		.setPCode(reinterpret_cast<const uint32_t*>(szCode));
	return VkUnwrap(Zenith_Vulkan::GetDevice().createShaderModule(xCreateInfo));
}

void Zenith_Vulkan_Shader::MergeReflection(const Flux_ShaderReflection& xStageReflection)
{
	// Merge bindings from stage reflection into combined reflection
	// Skip duplicates (same set/binding)
	const Zenith_Vector<Flux_ReflectedBinding>& axNewBindings = xStageReflection.GetBindings();
	for (u_int u = 0; u < axNewBindings.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xNewBinding = axNewBindings.Get(u);

		// Check if this binding already exists
		bool bExists = false;
		const Zenith_Vector<Flux_ReflectedBinding>& axExistingBindings = m_xReflection.GetBindings();
		for (u_int e = 0; e < axExistingBindings.GetSize(); e++)
		{
			const Flux_ReflectedBinding& xExisting = axExistingBindings.Get(e);
			if (xExisting.m_uSet == xNewBinding.m_uSet && xExisting.m_uBinding == xNewBinding.m_uBinding)
			{
				bExists = true;
				break;
			}
		}

		if (!bExists)
		{
			m_xReflection.AddBinding(xNewBinding);
		}
	}

	// Rebuild the lookup map after merging
	m_xReflection.BuildLookupMap();
}

// Legacy InitialiseFromSource / InitialiseComputeFromSource were removed
// alongside the Slang migration. The runtime path is now
// InitialiseFromProgramSource (FluxShaderProgram, defined above).
