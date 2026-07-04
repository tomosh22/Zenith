#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Zenith_Vulkan_Pipeline.h"
#include "Zenith_Vulkan.h"
#include "Flux/Slang/Flux_ShaderCatalog.h"
#include "Core/Zenith_CommandLine.h"   // --shader-debug-o0 (runtime-compile debug info, Stage 1)

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

void Zenith_Vulkan_Shader::Initialise(const Flux_ShaderDecl& xDecl)
{
	// Boot path: a failed shader load is non-recoverable — a zero-stage Reset()
	// shader just crashes later at createGraphicsPipeline, so a hard break here
	// is the correct (and earliest-possible) failure. InitialiseEx carries the
	// specific reason for callers that can use it (e.g. unit tests); this void
	// wrapper exists because the backend concept FluxBackendShader pins
	// Initialise to `-> std::same_as<void>`.
	const Zenith_Status xStatus = InitialiseEx(xDecl);
	Zenith_Assert(xStatus.IsOk(),
				  "Shader load failed for '%s' (eError=%u)",
				  xDecl.m_szName,
				  static_cast<u_int>(xStatus.Error()));
}

Zenith_Status Zenith_Vulkan_Shader::InitialiseEx(const Flux_ShaderDecl& xDecl)
{
	// Idempotent: free any previously-loaded SPIR-V code, modules, and
	// reflection so the hot-reload path can re-call Initialise on the same
	// shader instance without leaking GPU handles.
	Reset();

#ifdef ZENITH_WINDOWS
	if (Flux_SlangCompiler::IsInitialised())
	{
		return InitialiseFromProgramSource(xDecl);
	}
#endif
	return InitialiseFromProgramArtifacts(xDecl);
}

Zenith_Status Zenith_Vulkan_Shader::InitialiseFromProgramArtifacts(const Flux_ShaderDecl& xDecl)
{
	std::string strRoot(SHADER_SOURCE_ROOT);

	// Graphics-program path
	if (xDecl.m_szVertexEntry && xDecl.m_szFragmentEntry)
	{
		std::string strVStem = Flux_ShaderCatalog::GetVertexArtifactStem(xDecl);
		std::string strFStem = Flux_ShaderCatalog::GetFragmentArtifactStem(xDecl);

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
		// source-level entry name (xDecl.m_szVertexEntry / m_szFragmentEntry)
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
	if (xDecl.m_szComputeEntry)
	{
		std::string strCStem = Flux_ShaderCatalog::GetComputeArtifactStem(xDecl);
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
Zenith_Status Zenith_Vulkan_Shader::InitialiseFromProgramSource(const Flux_ShaderDecl& xDecl)
{

	Flux_SlangProgramDesc xDesc;
	Flux_ShaderCatalog::DescribeProgram(xDecl, xDesc);

	// Flux Shader System Overhaul — Stage 1: emit Slang debug info in Debug builds so
	// RenderDoc shows source in the runtime-compiled SPIR-V. This is the runtime path
	// ONLY — the offline FluxCompiler never sets it, so checked-in artifacts stay
	// optimized + byte-identical. Debug info is semantics-preserving (no float re-assoc).
#ifdef ZENITH_DEBUG
	xDesc.m_bEmitDebugInfo = true;
#endif
	// Opt-in deep-debug (`--shader-debug-o0`): additionally disable optimization. NOT
	// default — O0 moves pixels (float re-association), so it is a deliberate opt-in.
	xDesc.m_bDisableOptimization = Zenith_CommandLine::IsShaderDebugO0();

	Flux_SlangProgramResult xResult;
	if (!Flux_SlangCompiler::CompileProgram(xDesc, xResult))
	{
		// xResult.m_strError already holds the Slang diagnostic; log it here and
		// surface the dedicated code so callers can distinguish a compile error
		// from a missing artifact / corrupt blob.
		Zenith_Log(LOG_CATEGORY_RENDERER, "CompileProgram failed for '%s': %s",
				   xDecl.m_szName, xResult.m_strError.c_str());
		return Zenith_ErrorCode::SHADER_COMPILE_FAILED;
	}

	const bool bGraphics = xDecl.m_szVertexEntry && xDecl.m_szFragmentEntry;
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
	else if (xDecl.m_szComputeEntry)
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

// A shader is initialised from a Flux_ShaderDecl: Initialise(xDecl) — runtime Slang
// compile via InitialiseFromProgramSource (Windows), precompiled .spv + .spv.refl via
// InitialiseFromProgramArtifacts (Android). There is no string-pair (vert/frag) path.
// Tessellation has no callers; reintroduce when an actual user lands.

Zenith_Vulkan_Shader::~Zenith_Vulkan_Shader()
{
	Reset();
}

void Zenith_Vulkan_Shader::Reset()
{
	const vk::Device xDevice = g_xEngine.FluxBackend().GetDevice();

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
	return VkUnwrap(g_xEngine.FluxBackend().GetDevice().createShaderModule(xCreateInfo));
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

	// Stage 3a: merge specialization constants (dedup by constant id). A graphics
	// program's VS and FS sidecars each carry the whole-program spec table, so this
	// runs twice with identical entries — the id dedup keeps each constant once.
	const Zenith_Vector<Flux_ReflectedSpecConstant>& axNewSpecs = xStageReflection.GetSpecConstants();
	for (u_int u = 0; u < axNewSpecs.GetSize(); u++)
	{
		const Flux_ReflectedSpecConstant& xNewSpec = axNewSpecs.Get(u);
		bool bSpecExists = false;
		const Zenith_Vector<Flux_ReflectedSpecConstant>& axExistingSpecs = m_xReflection.GetSpecConstants();
		for (u_int e = 0; e < axExistingSpecs.GetSize(); e++)
		{
			if (axExistingSpecs.Get(e).m_uConstantId == xNewSpec.m_uConstantId)
			{
				bSpecExists = true;
				break;
			}
		}
		if (!bSpecExists)
		{
			m_xReflection.AddSpecConstant(xNewSpec);
		}
	}

	// Rebuild the lookup map after merging
	m_xReflection.BuildLookupMap();
}

// The runtime shader-compile path is InitialiseFromProgramSource (defined above),
// which compiles the Flux_ShaderDecl's Slang program in-process.
