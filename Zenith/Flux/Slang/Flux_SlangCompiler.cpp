#include "Zenith.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_Types.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-deprecated.h>

static Slang::ComPtr<slang::IGlobalSession> s_pxGlobalSession;

Flux_BindingHandle Flux_ShaderReflection::GetBinding(const char* szName) const
{
	auto it = m_xBindingMap.find(szName);
	if (it != m_xBindingMap.end())
	{
		return it->second;
	}
	Flux_BindingHandle xInvalid;
	return xInvalid;
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

	size_t ulLastSlash = strPath.find_last_of("/\\");
	std::string strFileName = (ulLastSlash != std::string::npos) ? strPath.substr(ulLastSlash + 1) : strPath;
	std::string strDirectory = (ulLastSlash != std::string::npos) ? strPath.substr(0, ulLastSlash) : ".";

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

	if (!s_pxGlobalSession)
	{
		xResultOut.m_strError = "Slang compiler not initialized";
		return false;
	}

	// Use the compile request API for explicit stage specification
	SlangCompileRequest* pxRequest = nullptr;
	SlangResult xResult = s_pxGlobalSession->createCompileRequest(&pxRequest);
	if (SLANG_FAILED(xResult) || !pxRequest)
	{
		xResultOut.m_strError = "Failed to create compile request";
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
	if (szDirectory && szDirectory[0] != '\0')
	{
		spAddSearchPath(pxRequest, szDirectory);
	}
#ifdef SHADER_SOURCE_ROOT
	spAddSearchPath(pxRequest, SHADER_SOURCE_ROOT);
#endif

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
	xResult = spCompile(pxRequest);

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
	Slang::ComPtr<slang::IBlob> pxCode;
	xResult = spGetEntryPointCodeBlob(pxRequest, 0, iTargetIndex, pxCode.writeRef());
	if (SLANG_FAILED(xResult) || !pxCode)
	{
		xResultOut.m_strError = "Failed to get SPIR-V output";
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	const uint32_t* puCode = (const uint32_t*)pxCode->getBufferPointer();
	size_t ulCodeSize = pxCode->getBufferSize() / sizeof(uint32_t);
	xResultOut.m_axSpirv.Reserve(static_cast<u_int>(ulCodeSize));
	for (size_t u = 0; u < ulCodeSize; u++)
	{
		xResultOut.m_axSpirv.PushBack(puCode[u]);
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

	if (!s_pxGlobalSession)
	{
		xResultOut.m_strError = "Slang compiler not initialized";
		return false;
	}

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
	size_t ulVertexSlash = strVertexPath.find_last_of("/\\");
	std::string strVertexFileName = (ulVertexSlash != std::string::npos) ? strVertexPath.substr(ulVertexSlash + 1) : strVertexPath;
	std::string strVertexDir = (ulVertexSlash != std::string::npos) ? strVertexPath.substr(0, ulVertexSlash) : ".";

	size_t ulFragmentSlash = strFragmentPath.find_last_of("/\\");
	std::string strFragmentFileName = (ulFragmentSlash != std::string::npos) ? strFragmentPath.substr(ulFragmentSlash + 1) : strFragmentPath;
	std::string strFragmentDir = (ulFragmentSlash != std::string::npos) ? strFragmentPath.substr(0, ulFragmentSlash) : ".";

	// Create compile request
	SlangCompileRequest* pxRequest = nullptr;
	SlangResult xResult = s_pxGlobalSession->createCompileRequest(&pxRequest);
	if (SLANG_FAILED(xResult) || !pxRequest)
	{
		xResultOut.m_strError = "Failed to create compile request";
		return false;
	}

	// Set target to SPIR-V
	int iTargetIndex = spAddCodeGenTarget(pxRequest, SLANG_SPIRV);
	spSetTargetProfile(pxRequest, iTargetIndex, s_pxGlobalSession->findProfile("spirv_1_3"));

	// Compiler flags - preserve params is critical for interface matching
	// Note: Avoid -fspv-reflect as it emits SPV_GOOGLE_user_type requiring VK_GOOGLE_user_type
	const char* aszArgs[] = {
		"-preserve-params",
		"-O0"
	};
	spProcessCommandLineArguments(pxRequest, aszArgs, 2);

	// Add search paths
	spAddSearchPath(pxRequest, strVertexDir.c_str());
	if (strFragmentDir != strVertexDir)
	{
		spAddSearchPath(pxRequest, strFragmentDir.c_str());
	}
#ifdef SHADER_SOURCE_ROOT
	spAddSearchPath(pxRequest, SHADER_SOURCE_ROOT);
#endif

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
	xResult = spCompile(pxRequest);

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
	Slang::ComPtr<slang::IBlob> pxVertexCode;
	xResult = spGetEntryPointCodeBlob(pxRequest, iVertexEntry, iTargetIndex, pxVertexCode.writeRef());
	if (SLANG_FAILED(xResult) || !pxVertexCode)
	{
		xResultOut.m_strError = "Failed to get vertex SPIR-V output";
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	const uint32_t* puVertexCode = (const uint32_t*)pxVertexCode->getBufferPointer();
	size_t ulVertexCodeSize = pxVertexCode->getBufferSize() / sizeof(uint32_t);
	xResultOut.m_axVertexSpirv.Reserve(static_cast<u_int>(ulVertexCodeSize));
	for (size_t u = 0; u < ulVertexCodeSize; u++)
	{
		xResultOut.m_axVertexSpirv.PushBack(puVertexCode[u]);
	}

	// Get fragment shader SPIR-V
	Slang::ComPtr<slang::IBlob> pxFragmentCode;
	xResult = spGetEntryPointCodeBlob(pxRequest, iFragmentEntry, iTargetIndex, pxFragmentCode.writeRef());
	if (SLANG_FAILED(xResult) || !pxFragmentCode)
	{
		xResultOut.m_strError = "Failed to get fragment SPIR-V output";
		spDestroyCompileRequest(pxRequest);
		return false;
	}

	const uint32_t* puFragmentCode = (const uint32_t*)pxFragmentCode->getBufferPointer();
	size_t ulFragmentCodeSize = pxFragmentCode->getBufferSize() / sizeof(uint32_t);
	xResultOut.m_axFragmentSpirv.Reserve(static_cast<u_int>(ulFragmentCodeSize));
	for (size_t u = 0; u < ulFragmentCodeSize; u++)
	{
		xResultOut.m_axFragmentSpirv.PushBack(puFragmentCode[u]);
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

	unsigned int uParamCount = pxLayout->getParameterCount();
	Zenith_Log(LOG_CATEGORY_RENDERER, "Slang Reflection: %u parameters found", uParamCount);

	for (unsigned int u = 0; u < uParamCount; u++)
	{
		slang::VariableLayoutReflection* pxParam = pxLayout->getParameterByIndex(u);
		if (!pxParam)
		{
			continue;
		}

		// Skip stage inputs/outputs (varyings) - we only want actual descriptor bindings
		// Varyings have location semantics, not binding/set semantics, and would conflict
		// with actual uniform bindings when merging vertex + fragment reflection
		slang::ParameterCategory eCategory = pxParam->getCategory();
		if (eCategory == slang::ParameterCategory::VaryingInput ||
			eCategory == slang::ParameterCategory::VaryingOutput)
		{
			continue;
		}

		slang::TypeLayoutReflection* pxTypeLayout = pxParam->getTypeLayout();
		if (!pxTypeLayout)
		{
			continue;
		}

		Flux_ReflectedBinding xBinding;
		xBinding.m_strName = pxParam->getName() ? pxParam->getName() : "";

		// For anonymous uniform blocks (common in GLSL), try to use the type name if no instance name
		if (xBinding.m_strName.empty())
		{
			slang::TypeReflection* pxType = pxTypeLayout->getType();
			if (pxType && pxType->getName())
			{
				xBinding.m_strName = pxType->getName();
			}
		}

		xBinding.m_uSet = static_cast<u_int>(pxParam->getBindingSpace());
		xBinding.m_uBinding = static_cast<u_int>(pxParam->getBindingIndex());
		xBinding.m_eType = SlangTypeToDescriptorType(pxTypeLayout);
		xBinding.m_uSize = static_cast<u_int>(pxTypeLayout->getSize());

		Zenith_Log(LOG_CATEGORY_RENDERER, "  Binding[%u]: name='%s', set=%u, binding=%u, type=%d",
			u, xBinding.m_strName.c_str(), xBinding.m_uSet, xBinding.m_uBinding, (int)xBinding.m_eType);

		xReflectionOut.AddBinding(xBinding);
	}

	xReflectionOut.BuildLookupMap();
}

DescriptorType Flux_SlangCompiler::SlangTypeToDescriptorType(void* pxTypeLayoutVoid)
{
	slang::TypeLayoutReflection* pxTypeLayout = static_cast<slang::TypeLayoutReflection*>(pxTypeLayoutVoid);

	slang::TypeReflection::Kind eKind = pxTypeLayout->getKind();
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
