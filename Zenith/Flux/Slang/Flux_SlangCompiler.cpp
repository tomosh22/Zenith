#include "Zenith.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_Types.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_WINDOWS
#pragma warning(push)
#pragma warning(disable: 4996)
#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-deprecated.h>
#pragma warning(pop)

static Slang::ComPtr<slang::IGlobalSession> s_pxGlobalSession;
#endif // ZENITH_WINDOWS

Flux_BindingHandle Flux_ShaderReflection::GetBinding(const char* szName) const
{
	auto it = m_xBindingMap.find(szName);
	if (it == m_xBindingMap.end())
	{
		// Log all available bindings to help debug
		Zenith_Log(LOG_CATEGORY_RENDERER, "GetBinding('%s') failed. Available bindings (%u):",
			szName, static_cast<u_int>(m_xBindingMap.size()));
		for (const auto& pair : m_xBindingMap)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "  '%s' -> set=%u, binding=%u",
				pair.first.c_str(), pair.second.m_uSet, pair.second.m_uBinding);
		}
		Zenith_Assert(false, "Shader binding '%s' not found in reflection", szName);
	}
	return it->second;
}

u_int Flux_ShaderReflection::GetBindingPoint(const char* szName) const
{
	return GetBinding(szName).m_uBinding;
}

u_int Flux_ShaderReflection::GetDescriptorSet(const char* szName) const
{
	return GetBinding(szName).m_uSet;
}

void Flux_ShaderReflection::PopulateLayout(Flux_PipelineLayout& xLayoutOut) const
{
	xLayoutOut.m_uNumDescriptorSets = 0;

	for (u_int u = 0; u < m_axBindings.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		if (xBinding.m_uSet >= FLUX_MAX_DESCRIPTOR_SET_LAYOUTS)
		{
			continue;
		}
		if (xBinding.m_uBinding >= FLUX_MAX_DESCRIPTOR_BINDINGS)
		{
			continue;
		}

		if (xBinding.m_uSet + 1 > xLayoutOut.m_uNumDescriptorSets)
		{
			xLayoutOut.m_uNumDescriptorSets = xBinding.m_uSet + 1;
		}

		xLayoutOut.m_axDescriptorSetLayouts[xBinding.m_uSet].m_axBindings[xBinding.m_uBinding].m_eType = xBinding.m_eType;
	}

	// Fill gaps in binding indices with placeholder types.
	// The pipeline builder stops at the first DESCRIPTOR_TYPE_MAX, so sparse
	// bindings (e.g. shadow shaders that skip texture slots) need gaps filled.
	for (u_int uSet = 0; uSet < xLayoutOut.m_uNumDescriptorSets; uSet++)
	{
		Flux_DescriptorSetLayout& xSetLayout = xLayoutOut.m_axDescriptorSetLayouts[uSet];

		u_int uMaxBinding = 0;
		for (u_int uBinding = 0; uBinding < FLUX_MAX_DESCRIPTOR_BINDINGS; uBinding++)
		{
			if (xSetLayout.m_axBindings[uBinding].m_eType != DESCRIPTOR_TYPE_MAX)
			{
				uMaxBinding = uBinding;
			}
		}

		for (u_int uBinding = 0; uBinding < uMaxBinding; uBinding++)
		{
			if (xSetLayout.m_axBindings[uBinding].m_eType == DESCRIPTOR_TYPE_MAX)
			{
				xSetLayout.m_axBindings[uBinding].m_eType = DESCRIPTOR_TYPE_BUFFER;
			}
		}
	}
}

void Flux_ShaderReflection::AddBinding(const Flux_ReflectedBinding& xBinding)
{
	m_axBindings.PushBack(xBinding);
}

void Flux_ShaderReflection::BuildLookupMap()
{
	m_xBindingMap.clear();
	for (u_int u = 0; u < m_axBindings.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		Flux_BindingHandle xHandle;
		xHandle.m_uSet = xBinding.m_uSet;
		xHandle.m_uBinding = xBinding.m_uBinding;
		m_xBindingMap[xBinding.m_strName] = xHandle;
	}
}

void Flux_ShaderReflection::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const u_int uCount = m_axBindings.GetSize();
	xStream << uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		xStream << static_cast<u_int>(xBinding.m_eType);
		xStream << xBinding.m_uSet;
		xStream << xBinding.m_uBinding;
		xStream << xBinding.m_strName;
		xStream << xBinding.m_uSize;
	}
}

void Flux_ShaderReflection::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uCount;
	xStream >> uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		Flux_ReflectedBinding xBinding;
		u_int uType;
		xStream >> uType;
		xBinding.m_eType = static_cast<DescriptorType>(uType);
		xStream >> xBinding.m_uSet;
		xStream >> xBinding.m_uBinding;
		xStream >> xBinding.m_strName;
		xStream >> xBinding.m_uSize;
		m_axBindings.PushBack(xBinding);
	}
	BuildLookupMap();
}

#ifdef ZENITH_WINDOWS
void Flux_SlangCompiler::Initialise()
{
	if (s_pxGlobalSession)
	{
		return;
	}

	SlangGlobalSessionDesc xDesc = {};
	xDesc.enableGLSL = true;  // Enable GLSL compatibility mode

	SlangResult xResult = slang::createGlobalSession(&xDesc, s_pxGlobalSession.writeRef());
	if (SLANG_FAILED(xResult))
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Failed to create Slang global session");
	}
}

void Flux_SlangCompiler::Shutdown()
{
	s_pxGlobalSession = nullptr;
}

bool Flux_SlangCompiler::IsInitialised()
{
	return s_pxGlobalSession != nullptr;
}

static SlangStage GetSlangStage(SlangShaderStage eStage)
{
	switch (eStage)
	{
	case SLANG_SHADER_STAGE_VERTEX:
		return SLANG_STAGE_VERTEX;
	case SLANG_SHADER_STAGE_FRAGMENT:
		return SLANG_STAGE_FRAGMENT;
	case SLANG_SHADER_STAGE_COMPUTE:
		return SLANG_STAGE_COMPUTE;
	case SLANG_SHADER_STAGE_TESSELLATION_CONTROL:
		return SLANG_STAGE_HULL;
	case SLANG_SHADER_STAGE_TESSELLATION_EVALUATION:
		return SLANG_STAGE_DOMAIN;
	case SLANG_SHADER_STAGE_GEOMETRY:
		return SLANG_STAGE_GEOMETRY;
	default:
		return SLANG_STAGE_NONE;
	}
}

// Detect source language from file extension
static SlangSourceLanguageType DetectSourceLanguage(const std::string& strPath)
{
	size_t ulDot = strPath.rfind('.');
	if (ulDot != std::string::npos)
	{
		std::string strExt = strPath.substr(ulDot);
		if (strExt == ".slang" || strExt == ".hlsl")
		{
			return SLANG_LANG_SLANG;
		}
	}
	// Default to GLSL for .vert, .frag, .comp, .fxh, etc.
	return SLANG_LANG_GLSL;
}

void Flux_SlangCompiler::SplitFilePath(const std::string& strPath, std::string& strFileNameOut, std::string& strDirectoryOut)
{
	size_t ulLastSlash = strPath.find_last_of("/\\");
	if (ulLastSlash != std::string::npos)
	{
		strFileNameOut = strPath.substr(ulLastSlash + 1);
		strDirectoryOut = strPath.substr(0, ulLastSlash);
	}
	else
	{
		strFileNameOut = strPath;
		strDirectoryOut = ".";
	}
}

bool Flux_SlangCompiler::CreateConfiguredCompileRequest(void*& pxRequestOut, int& iTargetIndexOut,
														const char* const* aszSearchPaths, u_int uSearchPathCount,
														std::string& strErrorOut)
{
	if (!s_pxGlobalSession)
	{
		strErrorOut = "Slang compiler not initialized";
		return false;
	}

	SlangCompileRequest* pxRequest = nullptr;
#pragma warning(suppress: 4996)
	SlangResult xResult = s_pxGlobalSession->createCompileRequest(&pxRequest);
	if (SLANG_FAILED(xResult) || !pxRequest)
	{
		strErrorOut = "Failed to create compile request";
		return false;
	}

	// Set target to SPIR-V
	int iTargetIndex = spAddCodeGenTarget(pxRequest, SLANG_SPIRV);
	spSetTargetProfile(pxRequest, iTargetIndex, s_pxGlobalSession->findProfile("spirv_1_3"));

	// Preserve all parameters and disable optimization to maintain vertex/fragment interface compatibility
	// Without this, Slang may optimize out unused varyings causing interface mismatches
	// Note: We get reflection data from Slang's API, not from SPIR-V extensions
	// Avoid -fspv-reflect as it emits SPV_GOOGLE_user_type which requires VK_GOOGLE_user_type
	const char* aszArgs[] = {
		"-preserve-params",
		"-O0"
	};
	spProcessCommandLineArguments(pxRequest, aszArgs, 2);

	// Add search paths
	for (u_int u = 0; u < uSearchPathCount; u++)
	{
		if (aszSearchPaths[u] && aszSearchPaths[u][0] != '\0')
		{
			spAddSearchPath(pxRequest, aszSearchPaths[u]);
		}
	}
#ifdef SHADER_SOURCE_ROOT
	spAddSearchPath(pxRequest, SHADER_SOURCE_ROOT);
#endif

	pxRequestOut = pxRequest;
	iTargetIndexOut = iTargetIndex;
	return true;
}

bool Flux_SlangCompiler::ExtractSpirvBlob(void* pxRequestVoid, int iEntryPoint, int iTargetIndex,
										  Zenith_Vector<uint32_t>& axSpirvOut, std::string& strErrorOut)
{
	SlangCompileRequest* pxRequest = static_cast<SlangCompileRequest*>(pxRequestVoid);
	Slang::ComPtr<slang::IBlob> pxCode;
	SlangResult xResult = spGetEntryPointCodeBlob(pxRequest, iEntryPoint, iTargetIndex, pxCode.writeRef());
	if (SLANG_FAILED(xResult) || !pxCode)
	{
		strErrorOut = "Failed to get SPIR-V output";
		return false;
	}

	const uint32_t* puCode = (const uint32_t*)pxCode->getBufferPointer();
	size_t ulCodeSize = pxCode->getBufferSize() / sizeof(uint32_t);
	axSpirvOut.Reserve(static_cast<u_int>(ulCodeSize));
	for (size_t u = 0; u < ulCodeSize; u++)
	{
		axSpirvOut.PushBack(puCode[u]);
	}
	return true;
}

bool Flux_SlangCompiler::ExtractParameterBinding(void* pxParamVoid, void* pxTypeLayoutVoid, Flux_ReflectedBinding& xBindingOut)
{
	slang::VariableLayoutReflection* pxParam = static_cast<slang::VariableLayoutReflection*>(pxParamVoid);
	slang::TypeLayoutReflection* pxTypeLayout = static_cast<slang::TypeLayoutReflection*>(pxTypeLayoutVoid);

	// Skip stage inputs/outputs (varyings) - we only want actual descriptor bindings
	slang::ParameterCategory eCategory = pxParam->getCategory();
	if (eCategory == slang::ParameterCategory::VaryingInput ||
		eCategory == slang::ParameterCategory::VaryingOutput)
	{
		return false;
	}

	xBindingOut.m_strName = pxParam->getName() ? pxParam->getName() : "";

	// For anonymous uniform blocks (common in GLSL), try to use the type name if no instance name
	if (xBindingOut.m_strName.empty())
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		if (pxType && pxType->getName())
		{
			xBindingOut.m_strName = pxType->getName();
		}
	}

	xBindingOut.m_uSet = static_cast<u_int>(pxParam->getBindingSpace());
	xBindingOut.m_uBinding = static_cast<u_int>(pxParam->getBindingIndex());
	xBindingOut.m_eType = SlangTypeToDescriptorType(pxTypeLayout);
	xBindingOut.m_uSize = static_cast<u_int>(pxTypeLayout->getSize());
	return true;
}

bool Flux_SlangCompiler::Compile(const std::string& strPath, SlangShaderStage eStage, Flux_SlangCompileResult& xResultOut)
{
	xResultOut.m_bSuccess = false;
	xResultOut.m_strError.clear();
	xResultOut.m_axSpirv.Clear();

	uint64_t ulFileSize = 0;
	char* pcFileData = Zenith_FileAccess::ReadFile(strPath.c_str(), ulFileSize);
	if (!pcFileData)
	{
		xResultOut.m_strError = "Failed to read shader file: " + strPath;
		return false;
	}

	std::string strSource(pcFileData, ulFileSize);
	Zenith_MemoryManagement::Deallocate(pcFileData);

	std::string strFileName, strDirectory;
	SplitFilePath(strPath, strFileName, strDirectory);

	SlangSourceLanguageType eLanguage = DetectSourceLanguage(strPath);
	return CompileFromSource(strSource, "main", eStage, xResultOut, strFileName.c_str(), strDirectory.c_str(), eLanguage);
}

bool Flux_SlangCompiler::CompileFromSource(const std::string& strSource, const std::string& strEntryPoint,
											SlangShaderStage eStage, Flux_SlangCompileResult& xResultOut,
											const char* szSourceName, const char* szDirectory,
											SlangSourceLanguageType eLanguage)
{
	xResultOut.m_bSuccess = false;
	xResultOut.m_strError.clear();
	xResultOut.m_axSpirv.Clear();

	// Create configured compile request
	const char* aszSearchPaths[] = { szDirectory };
	u_int uSearchPathCount = (szDirectory && szDirectory[0] != '\0') ? 1 : 0;

	void* pxRequestVoid = nullptr;
	int iTargetIndex = 0;
	if (!CreateConfiguredCompileRequest(pxRequestVoid, iTargetIndex, aszSearchPaths, uSearchPathCount, xResultOut.m_strError))
	{
		return false;
	}
	SlangCompileRequest* pxRequest = static_cast<SlangCompileRequest*>(pxRequestVoid);

	// Add translation unit with detected or specified language
	// Cast our enum to the Slang API enum (values are intentionally matching)
	int iTranslationUnit = spAddTranslationUnit(pxRequest, static_cast<SlangSourceLanguage>(eLanguage),
		szSourceName ? szSourceName : "shader");

	// Build the full path for includes to work correctly
	std::string strFullPath;
	if (szDirectory && szDirectory[0] != '\0')
	{
		strFullPath = std::string(szDirectory) + "/" + (szSourceName ? szSourceName : "shader.glsl");
	}
	else
	{
		strFullPath = szSourceName ? szSourceName : "shader.glsl";
	}

	// Add the source code
	spAddTranslationUnitSourceString(pxRequest, iTranslationUnit, strFullPath.c_str(), strSource.c_str());

	// Add entry point with explicit stage
	SlangStage eSlangStage = GetSlangStage(eStage);
	spAddEntryPoint(pxRequest, iTranslationUnit, strEntryPoint.c_str(), eSlangStage);

	// Compile
	SlangResult xResult = spCompile(pxRequest);

	// Get diagnostics
	const char* szDiagnostics = spGetDiagnosticOutput(pxRequest);
	if (szDiagnostics && szDiagnostics[0] != '\0')
	{
		if (SLANG_FAILED(xResult))
		{
			xResultOut.m_strError = szDiagnostics;
			spDestroyCompileRequest(pxRequest);
			return false;
		}
	}

	if (SLANG_FAILED(xResult))
	{
		xResultOut.m_strError = "Compilation failed";
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	// Get the SPIR-V output
	if (!ExtractSpirvBlob(pxRequest, 0, iTargetIndex, xResultOut.m_axSpirv, xResultOut.m_strError))
	{
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	// Get reflection data (SlangReflection* is cast to ProgramLayout*)
	slang::ProgramLayout* pxReflection = (slang::ProgramLayout*)spGetReflection(pxRequest);
	if (pxReflection)
	{
		ExtractReflection(pxReflection, xResultOut.m_xReflection);
	}

	spDestroyCompileRequest(pxRequest);
	xResultOut.m_bSuccess = true;
	return true;
}

bool Flux_SlangCompiler::CompileGraphicsPipeline(const std::string& strVertexPath, const std::string& strFragmentPath,
												  Flux_SlangGraphicsPipelineResult& xResultOut)
{
	xResultOut.m_bSuccess = false;
	xResultOut.m_strError.clear();
	xResultOut.m_axVertexSpirv.Clear();
	xResultOut.m_axFragmentSpirv.Clear();

	// Read vertex shader source
	uint64_t ulVertexSize = 0;
	char* pcVertexData = Zenith_FileAccess::ReadFile(strVertexPath.c_str(), ulVertexSize);
	if (!pcVertexData)
	{
		xResultOut.m_strError = "Failed to read vertex shader file: " + strVertexPath;
		return false;
	}
	std::string strVertexSource(pcVertexData, ulVertexSize);
	Zenith_MemoryManagement::Deallocate(pcVertexData);

	// Read fragment shader source
	uint64_t ulFragmentSize = 0;
	char* pcFragmentData = Zenith_FileAccess::ReadFile(strFragmentPath.c_str(), ulFragmentSize);
	if (!pcFragmentData)
	{
		xResultOut.m_strError = "Failed to read fragment shader file: " + strFragmentPath;
		return false;
	}
	std::string strFragmentSource(pcFragmentData, ulFragmentSize);
	Zenith_MemoryManagement::Deallocate(pcFragmentData);

	// Extract file names and directories
	std::string strVertexFileName, strVertexDir;
	SplitFilePath(strVertexPath, strVertexFileName, strVertexDir);

	std::string strFragmentFileName, strFragmentDir;
	SplitFilePath(strFragmentPath, strFragmentFileName, strFragmentDir);

	// Build search paths (deduplicate if both shaders are in the same directory)
	const char* aszSearchPaths[2] = { strVertexDir.c_str(), nullptr };
	u_int uSearchPathCount = 1;
	if (strFragmentDir != strVertexDir)
	{
		aszSearchPaths[1] = strFragmentDir.c_str();
		uSearchPathCount = 2;
	}

	// Create configured compile request
	void* pxRequestVoid = nullptr;
	int iTargetIndex = 0;
	if (!CreateConfiguredCompileRequest(pxRequestVoid, iTargetIndex, aszSearchPaths, uSearchPathCount, xResultOut.m_strError))
	{
		return false;
	}
	SlangCompileRequest* pxRequest = static_cast<SlangCompileRequest*>(pxRequestVoid);

	// Detect source languages
	SlangSourceLanguageType eVertexLang = DetectSourceLanguage(strVertexPath);
	SlangSourceLanguageType eFragmentLang = DetectSourceLanguage(strFragmentPath);

	// Add vertex shader as translation unit
	int iVertexUnit = spAddTranslationUnit(pxRequest, static_cast<SlangSourceLanguage>(eVertexLang), strVertexFileName.c_str());
	std::string strVertexFullPath = strVertexDir + "/" + strVertexFileName;
	spAddTranslationUnitSourceString(pxRequest, iVertexUnit, strVertexFullPath.c_str(), strVertexSource.c_str());

	// Add fragment shader as translation unit
	int iFragmentUnit = spAddTranslationUnit(pxRequest, static_cast<SlangSourceLanguage>(eFragmentLang), strFragmentFileName.c_str());
	std::string strFragmentFullPath = strFragmentDir + "/" + strFragmentFileName;
	spAddTranslationUnitSourceString(pxRequest, iFragmentUnit, strFragmentFullPath.c_str(), strFragmentSource.c_str());

	// Add entry points for both stages
	// This is the key: by adding both entry points to the same compile request,
	// Slang can see the full pipeline interface and will preserve varyings
	int iVertexEntry = spAddEntryPoint(pxRequest, iVertexUnit, "main", SLANG_STAGE_VERTEX);
	int iFragmentEntry = spAddEntryPoint(pxRequest, iFragmentUnit, "main", SLANG_STAGE_FRAGMENT);

	// Compile
	SlangResult xResult = spCompile(pxRequest);

	// Get diagnostics
	const char* szDiagnostics = spGetDiagnosticOutput(pxRequest);
	if (szDiagnostics && szDiagnostics[0] != '\0')
	{
		if (SLANG_FAILED(xResult))
		{
			xResultOut.m_strError = szDiagnostics;
			spDestroyCompileRequest(pxRequest);
			return false;
		}
	}

	if (SLANG_FAILED(xResult))
	{
		xResultOut.m_strError = "Graphics pipeline compilation failed";
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	// Get vertex shader SPIR-V
	if (!ExtractSpirvBlob(pxRequest, iVertexEntry, iTargetIndex, xResultOut.m_axVertexSpirv, xResultOut.m_strError))
	{
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	// Get fragment shader SPIR-V
	if (!ExtractSpirvBlob(pxRequest, iFragmentEntry, iTargetIndex, xResultOut.m_axFragmentSpirv, xResultOut.m_strError))
	{
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	// Get reflection data
	slang::ProgramLayout* pxReflection = (slang::ProgramLayout*)spGetReflection(pxRequest);
	if (pxReflection)
	{
		ExtractReflection(pxReflection, xResultOut.m_xVertexReflection);
		// Fragment shares the same reflection data for uniforms/bindings
		ExtractReflection(pxReflection, xResultOut.m_xFragmentReflection);
	}

	spDestroyCompileRequest(pxRequest);
	xResultOut.m_bSuccess = true;
	return true;
}

void Flux_SlangCompiler::ExtractReflection(void* pxLayoutVoid, Flux_ShaderReflection& xReflectionOut)
{
	slang::ProgramLayout* pxLayout = static_cast<slang::ProgramLayout*>(pxLayoutVoid);

	u_int uParamCount = static_cast<u_int>(pxLayout->getParameterCount());
	u_int uEntryPointCount = static_cast<u_int>(pxLayout->getEntryPointCount());

	for (u_int u = 0; u < uParamCount; u++)
	{
		slang::VariableLayoutReflection* pxParam = pxLayout->getParameterByIndex(u);
		if (!pxParam)
		{
			continue;
		}

		slang::TypeLayoutReflection* pxTypeLayout = pxParam->getTypeLayout();
		if (!pxTypeLayout)
		{
			continue;
		}

		Flux_ReflectedBinding xBinding;
		if (ExtractParameterBinding(pxParam, pxTypeLayout, xBinding))
		{
			xReflectionOut.AddBinding(xBinding);
		}
	}

	// For combined graphics pipelines, also check entry-point-specific parameters
	// Some resources may be reported per-entry-point rather than globally
	for (u_int ep = 0; ep < uEntryPointCount; ep++)
	{
		slang::EntryPointLayout* pxEntryPoint = pxLayout->getEntryPointByIndex(ep);
		if (!pxEntryPoint)
		{
			continue;
		}

		for (u_int u = 0; u < pxEntryPoint->getParameterCount(); u++)
		{
			slang::VariableLayoutReflection* pxParam = pxEntryPoint->getParameterByIndex(u);
			if (!pxParam)
			{
				continue;
			}

			slang::TypeLayoutReflection* pxTypeLayout = pxParam->getTypeLayout();
			if (!pxTypeLayout)
			{
				continue;
			}

			Flux_ReflectedBinding xBinding;
			if (!ExtractParameterBinding(pxParam, pxTypeLayout, xBinding))
			{
				continue;
			}

			// Check if this binding already exists (avoid duplicates)
			bool bExists = false;
			for (u_int v = 0; v < xReflectionOut.GetBindings().GetSize(); v++)
			{
				const Flux_ReflectedBinding& xExisting = xReflectionOut.GetBindings().Get(v);
				if (xExisting.m_uSet == xBinding.m_uSet && xExisting.m_uBinding == xBinding.m_uBinding)
				{
					bExists = true;
					break;
				}
			}

			if (!bExists)
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "  EP Binding: name='%s', set=%u, binding=%u, type=%d",
					xBinding.m_strName.c_str(), xBinding.m_uSet, xBinding.m_uBinding, (int)xBinding.m_eType);
				xReflectionOut.AddBinding(xBinding);
			}
		}
	}

	xReflectionOut.BuildLookupMap();
}

DescriptorType Flux_SlangCompiler::SlangTypeToDescriptorType(void* pxTypeLayoutVoid)
{
	slang::TypeLayoutReflection* pxTypeLayout = static_cast<slang::TypeLayoutReflection*>(pxTypeLayoutVoid);

	slang::TypeReflection::Kind eKind = pxTypeLayout->getKind();

	// Detect unbounded arrays (e.g., sampler2D g_axTextures[])
	if (eKind == slang::TypeReflection::Kind::Array)
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		if (pxType && pxType->getElementCount() == 0)
		{
			return DESCRIPTOR_TYPE_UNBOUNDED_TEXTURES;
		}
	}

	slang::BindingType eBindingType = pxTypeLayout->getDescriptorSetDescriptorRangeType(0, 0);

	switch (eBindingType)
	{
	case slang::BindingType::ConstantBuffer:
		return DESCRIPTOR_TYPE_BUFFER;
	case slang::BindingType::RawBuffer:
	case slang::BindingType::MutableRawBuffer:
		return DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case slang::BindingType::Texture:
	case slang::BindingType::CombinedTextureSampler:
		return DESCRIPTOR_TYPE_TEXTURE;
	case slang::BindingType::MutableTexture:
		return DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case slang::BindingType::Sampler:
		return DESCRIPTOR_TYPE_TEXTURE;
	default:
		break;
	}

	switch (eKind)
	{
	case slang::TypeReflection::Kind::ConstantBuffer:
	case slang::TypeReflection::Kind::ParameterBlock:
		return DESCRIPTOR_TYPE_BUFFER;
	case slang::TypeReflection::Kind::Resource:
		return DESCRIPTOR_TYPE_TEXTURE;
	case slang::TypeReflection::Kind::SamplerState:
		return DESCRIPTOR_TYPE_TEXTURE;
	case slang::TypeReflection::Kind::ShaderStorageBuffer:
		return DESCRIPTOR_TYPE_STORAGE_BUFFER;
	default:
		return DESCRIPTOR_TYPE_BUFFER;
	}
}
#endif // ZENITH_WINDOWS
