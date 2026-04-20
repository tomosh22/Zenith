#include "Zenith.h"

#include "Zenith_Vulkan_Pipeline.h"
#include "Zenith_Vulkan.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "DataStream/Zenith_DataStream.h"

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

void Zenith_Vulkan_Shader::Initialise(const std::string& strVertex, const std::string& strFragment, const std::string&, const std::string& strDomain, const std::string& strHull)
{
#ifdef ZENITH_WINDOWS
	// Use runtime compilation when Slang compiler is available (Windows only)
	if (Flux_SlangCompiler::IsInitialised() && strDomain.empty() && strHull.empty())
	{
		m_strVertexPath = strVertex;
		m_strFragmentPath = strFragment;

		bool bSuccess = InitialiseFromSource(strVertex, strFragment);
		Zenith_Assert(bSuccess, "Shader compilation failed: %s + %s", strVertex.c_str(), strFragment.c_str());
		return;
	}
#endif

	const std::string strExtension = ".spv";
	std::string strShaderRoot(SHADER_SOURCE_ROOT);
	m_pcVertShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strVertex + strExtension).c_str(), m_pcVertShaderCodeSize);
	m_pcFragShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strFragment + strExtension).c_str(), m_pcFragShaderCodeSize);

	Zenith_Assert(m_pcVertShaderCode != nullptr, "Failed to load precompiled shader: %s%s%s", strShaderRoot.c_str(), strVertex.c_str(), strExtension.c_str());
	Zenith_Assert(m_pcFragShaderCode != nullptr, "Failed to load precompiled shader: %s%s%s", strShaderRoot.c_str(), strFragment.c_str(), strExtension.c_str());

	m_uStageCount = 2;

	if (strDomain.length())
	{
		Zenith_Assert(strHull.length(), "Found tesc but not tese");
		m_pcTescShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strDomain + strExtension).c_str(), m_pcTescShaderCodeSize);
		m_pcTeseShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strHull + strExtension).c_str(), m_pcTeseShaderCodeSize);
		m_uStageCount = 4;
		m_bTesselation = true;
	}

	m_xVertShaderModule = CreateShaderModule(m_pcVertShaderCode, m_pcVertShaderCodeSize);
	m_xFragShaderModule = CreateShaderModule(m_pcFragShaderCode, m_pcFragShaderCodeSize);

	m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];

	m_xInfos[0].stage = vk::ShaderStageFlagBits::eVertex;
	m_xInfos[0].module = m_xVertShaderModule;
	m_xInfos[0].pName = "main";

	m_xInfos[1].stage = vk::ShaderStageFlagBits::eFragment;
	m_xInfos[1].module = m_xFragShaderModule;
	m_xInfos[1].pName = "main";

	if (m_bTesselation) {
		m_xTescShaderModule = CreateShaderModule(m_pcTescShaderCode, m_pcTescShaderCodeSize);
		m_xTeseShaderModule = CreateShaderModule(m_pcTeseShaderCode, m_pcTeseShaderCodeSize);

		m_xInfos[2].stage = vk::ShaderStageFlagBits::eTessellationControl;
		m_xInfos[2].module = m_xTescShaderModule;
		m_xInfos[2].pName = "main";

		m_xInfos[3].stage = vk::ShaderStageFlagBits::eTessellationEvaluation;
		m_xInfos[3].module = m_xTeseShaderModule;
		m_xInfos[3].pName = "main";
	}

	// Load pre-compiled reflection data
	{
		const std::string strReflExt = ".spv.refl";
		Zenith_DataStream xVertReflStream;
		xVertReflStream.ReadFromFile((strShaderRoot + strVertex + strReflExt).c_str());
		if (xVertReflStream.IsValid())
		{
			Flux_ShaderReflection xVertRefl;
			xVertRefl.ReadFromDataStream(xVertReflStream);
			MergeReflection(xVertRefl);
		}

		Zenith_DataStream xFragReflStream;
		xFragReflStream.ReadFromFile((strShaderRoot + strFragment + strReflExt).c_str());
		if (xFragReflStream.IsValid())
		{
			Flux_ShaderReflection xFragRefl;
			xFragRefl.ReadFromDataStream(xFragReflStream);
			MergeReflection(xFragRefl);
		}
	}
}

void Zenith_Vulkan_Shader::InitialiseCompute(const std::string& strCompute)
{
#ifdef ZENITH_WINDOWS
	// Use runtime compilation when Slang compiler is available (Windows only)
	if (Flux_SlangCompiler::IsInitialised())
	{
		m_strComputePath = strCompute;
		bool bSuccess = InitialiseComputeFromSource(strCompute);
		Zenith_Assert(bSuccess, "Compute shader compilation failed: %s", strCompute.c_str());
		return;
	}
#endif
	// Load precompiled SPV
	const std::string strExtension = ".spv";
	std::string strShaderRoot(SHADER_SOURCE_ROOT);
	m_pcCompShaderCode = Zenith_FileAccess::ReadFile((strShaderRoot + strCompute + strExtension).c_str(), m_pcCompShaderCodeSize);
	Zenith_Assert(m_pcCompShaderCode != nullptr, "Failed to load precompiled shader: %s%s", strCompute.c_str(), strExtension.c_str());
	m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
	m_uStageCount = 1;

	// Load pre-compiled reflection data
	const std::string strReflExt = ".spv.refl";
	Zenith_DataStream xReflStream;
	xReflStream.ReadFromFile((strShaderRoot + strCompute + strReflExt).c_str());
	if (xReflStream.IsValid())
	{
		m_xReflection.ReadFromDataStream(xReflStream);
	}
}

Zenith_Vulkan_Shader::~Zenith_Vulkan_Shader()
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

	delete[] m_xInfos;
	m_xInfos = nullptr;
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

#ifdef ZENITH_WINDOWS
bool Zenith_Vulkan_Shader::InitialiseFromSource(const std::string& strVertexPath, const std::string& strFragmentPath)
{
	if (!Flux_SlangCompiler::IsInitialised())
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Slang compiler not initialized for runtime compilation");
		return false;
	}

	// Store paths for hot reload
	m_strVertexPath = strVertexPath;
	m_strFragmentPath = strFragmentPath;

	std::string strShaderRoot(SHADER_SOURCE_ROOT);

	// Compile both shaders together using paired compilation
	// This ensures Slang sees the full pipeline interface and preserves varyings
	// that are output from vertex shader but conditionally used in fragment shader
	// (e.g., when SHADOWS is defined and fragment shader wraps most code in #ifndef SHADOWS)
	Flux_SlangGraphicsPipelineResult xPipelineResult;
	if (!Flux_SlangCompiler::CompileGraphicsPipeline(strShaderRoot + strVertexPath, strShaderRoot + strFragmentPath, xPipelineResult))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Failed to compile graphics pipeline: %s + %s - %s",
				   strVertexPath.c_str(), strFragmentPath.c_str(), xPipelineResult.m_strError.c_str());
		return false;
	}

	// Create shader modules from compiled SPIR-V
	m_pcVertShaderCodeSize = xPipelineResult.m_axVertexSpirv.GetSize() * sizeof(uint32_t);
	m_pcVertShaderCode = new char[m_pcVertShaderCodeSize];
	memcpy(m_pcVertShaderCode, xPipelineResult.m_axVertexSpirv.GetDataPointer(), m_pcVertShaderCodeSize);

	m_pcFragShaderCodeSize = xPipelineResult.m_axFragmentSpirv.GetSize() * sizeof(uint32_t);
	m_pcFragShaderCode = new char[m_pcFragShaderCodeSize];
	memcpy(m_pcFragShaderCode, xPipelineResult.m_axFragmentSpirv.GetDataPointer(), m_pcFragShaderCodeSize);

	m_xVertShaderModule = CreateShaderModule(m_pcVertShaderCode, m_pcVertShaderCodeSize);
	m_xFragShaderModule = CreateShaderModule(m_pcFragShaderCode, m_pcFragShaderCodeSize);

	m_uStageCount = 2;
	m_xInfos = new vk::PipelineShaderStageCreateInfo[m_uStageCount];

	m_xInfos[0].stage = vk::ShaderStageFlagBits::eVertex;
	m_xInfos[0].module = m_xVertShaderModule;
	m_xInfos[0].pName = "main";

	m_xInfos[1].stage = vk::ShaderStageFlagBits::eFragment;
	m_xInfos[1].module = m_xFragShaderModule;
	m_xInfos[1].pName = "main";

	// Merge reflection data from both stages
	MergeReflection(xPipelineResult.m_xVertexReflection);
	MergeReflection(xPipelineResult.m_xFragmentReflection);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Runtime compiled shader (paired): %s + %s (%u bindings)",
			   strVertexPath.c_str(), strFragmentPath.c_str(), m_xReflection.GetBindings().GetSize());

	return true;
}

bool Zenith_Vulkan_Shader::InitialiseComputeFromSource(const std::string& strComputePath)
{
	if (!Flux_SlangCompiler::IsInitialised())
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Slang compiler not initialized for runtime compilation");
		return false;
	}

	std::string strShaderRoot(SHADER_SOURCE_ROOT);

	// Compile compute shader
	Flux_SlangCompileResult xResult;
	if (!Flux_SlangCompiler::Compile(strShaderRoot + strComputePath, SLANG_SHADER_STAGE_COMPUTE, xResult))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Failed to compile compute shader: %s - %s",
				   strComputePath.c_str(), xResult.m_strError.c_str());
		return false;
	}

	// Create shader module from compiled SPIR-V
	m_pcCompShaderCodeSize = xResult.m_axSpirv.GetSize() * sizeof(uint32_t);
	m_pcCompShaderCode = new char[m_pcCompShaderCodeSize];
	memcpy(m_pcCompShaderCode, xResult.m_axSpirv.GetDataPointer(), m_pcCompShaderCodeSize);

	m_xCompShaderModule = CreateShaderModule(m_pcCompShaderCode, m_pcCompShaderCodeSize);
	m_uStageCount = 1;

	// Store reflection data
	m_xReflection = xResult.m_xReflection;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Runtime compiled compute shader: %s (%u bindings)",
			   strComputePath.c_str(), m_xReflection.GetBindings().GetSize());

	return true;
}
#endif // ZENITH_WINDOWS
