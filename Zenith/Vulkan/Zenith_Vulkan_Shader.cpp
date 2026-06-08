#include "Zenith.h"
#include "Core/Zenith_Engine.h"

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
	// Boot path: a failed shader load is non-recoverable — a zero-stage Reset()
	// shader just crashes later at createGraphicsPipeline, so a hard break here
	// is the correct (and earliest-possible) failure. InitialiseEx carries the
	// specific reason for callers that can use it (e.g. unit tests); this void
	// wrapper exists because the backend concept FluxBackendShader pins
	// Initialise to `-> std::same_as<void>`.
	const Zenith_Status xStatus = InitialiseEx(eProgram);
	Zenith_Assert(xStatus.IsOk(),
				  "Shader load failed for FluxShaderProgram=%u (eError=%u)",
				  static_cast<u_int>(eProgram),
				  static_cast<u_int>(xStatus.Error()));
}

Zenith_Status Zenith_Vulkan_Shader::InitialiseEx(FluxShaderProgram eProgram)
{
	// Idempotent: free any previously-loaded SPIR-V code, modules, and
	// reflection so the hot-reload path can re-call Initialise on the same
	// shader instance without leaking GPU handles.
	Reset();

#ifdef ZENITH_WINDOWS
	if (Flux_SlangCompiler::IsInitialised())
	{
		return InitialiseFromProgramSource(eProgram);
	}
#endif
	return InitialiseFromProgramArtifacts(eProgram);
}

Zenith_Status Zenith_Vulkan_Shader::InitialiseFromProgramArtifacts(FluxShaderProgram eProgram)
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
		// ReadFile returns null on a missing artifact (graceful since WS14.B1).
		if (!m_pcVertShaderCode || !m_pcFragShaderCode) return Zenith_ErrorCode::FILE_NOT_FOUND;
		// A present-but-empty .spv would otherwise trip CreateShaderModule's
		// hard assert — surface it as recoverable corruption instead.
		if (!m_pcVertShaderCodeSize || !m_pcFragShaderCodeSize) return Zenith_ErrorCode::CORRUPT_DATA;

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
		return Zenith_ErrorCode::SUCCESS;
	}

	// Compute-program path
	if (xEntry.m_szComputeEntry)
	{
		std::string strCStem = Flux_ShaderRegistry::GetComputeArtifactStem(eProgram);
		m_pcCompShaderCode = Zenith_FileAccess::ReadFile((strRoot + strCStem + ".spv").c_str(), m_pcCompShaderCodeSize);
		if (!m_pcCompShaderCode) return Zenith_ErrorCode::FILE_NOT_FOUND;
		if (!m_pcCompShaderCodeSize) return Zenith_ErrorCode::CORRUPT_DATA;
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
		return Zenith_ErrorCode::SUCCESS;
	}

	// Registry entry exposed neither a graphics (vert+frag) nor a compute entry
	// point — a malformed program descriptor, not a load failure.
	return Zenith_ErrorCode::INVALID_ARGUMENT;
}

#ifdef ZENITH_WINDOWS
Zenith_Status Zenith_Vulkan_Shader::InitialiseFromProgramSource(FluxShaderProgram eProgram)
{
	const Flux_ShaderRegistryEntry& xEntry = Flux_ShaderRegistry::GetProgram(eProgram);

	Flux_SlangProgramDesc xDesc;
	Flux_ShaderRegistry::DescribeProgram(eProgram, xDesc);

	Flux_SlangProgramResult xResult;
	if (!Flux_SlangCompiler::CompileProgram(xDesc, xResult))
	{
		// xResult.m_strError already holds the Slang diagnostic; log it here and
		// surface the dedicated code so callers can distinguish a compile error
		// from a missing artifact / corrupt blob.
		Zenith_Log(LOG_CATEGORY_RENDERER, "CompileProgram failed for '%s': %s",
				   xEntry.m_szName, xResult.m_strError.c_str());
		return Zenith_ErrorCode::SHADER_COMPILE_FAILED;
	}

	const bool bGraphics = xEntry.m_szVertexEntry && xEntry.m_szFragmentEntry;
	if (bGraphics)
	{
		m_pcVertShaderCodeSize = xResult.m_axVertexSpirv.GetSize() * sizeof(uint32_t);
		m_pcFragShaderCodeSize = xResult.m_axFragmentSpirv.GetSize() * sizeof(uint32_t);
		// A "successful" compile that emitted zero SPIR-V words would trip
		// CreateShaderModule's hard assert; treat it as recoverable corruption.
		if (!m_pcVertShaderCodeSize || !m_pcFragShaderCodeSize) return Zenith_ErrorCode::CORRUPT_DATA;

		m_pcVertShaderCode     = new char[m_pcVertShaderCodeSize];
		memcpy(m_pcVertShaderCode, xResult.m_axVertexSpirv.GetDataPointer(), m_pcVertShaderCodeSize);

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
		if (!m_pcCompShaderCodeSize) return Zenith_ErrorCode::CORRUPT_DATA;
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
	return Zenith_ErrorCode::SUCCESS;
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
	const vk::Device xDevice = g_xEngine.Vulkan().GetDevice();

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
	return VkUnwrap(g_xEngine.Vulkan().GetDevice().createShaderModule(xCreateInfo));
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
