#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_Enums.h"
#include <string>
#include <unordered_map>

class Zenith_DataStream;

struct Flux_BindingHandle
{
	u_int m_uSet = UINT32_MAX;
	u_int m_uBinding = UINT32_MAX;

	bool IsValid() const { return m_uSet != UINT32_MAX && m_uBinding != UINT32_MAX; }
};

struct Flux_ReflectedBinding
{
	DescriptorType m_eType = DESCRIPTOR_TYPE_MAX;
	u_int m_uSet = 0;
	u_int m_uBinding = 0;
	std::string m_strName;
	u_int m_uSize = 0;
};

class Flux_ShaderReflection
{
public:
	Flux_ShaderReflection() = default;

	Flux_BindingHandle GetBinding(const char* szName) const;
	u_int GetBindingPoint(const char* szName) const;
	u_int GetDescriptorSet(const char* szName) const;

	void PopulateLayout(struct Flux_PipelineLayout& xLayoutOut) const;

	void AddBinding(const Flux_ReflectedBinding& xBinding);
	void BuildLookupMap();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	const Zenith_Vector<Flux_ReflectedBinding>& GetBindings() const { return m_axBindings; }

private:
	Zenith_Vector<Flux_ReflectedBinding> m_axBindings;
	std::unordered_map<std::string, Flux_BindingHandle> m_xBindingMap;
};

enum SlangShaderStage
{
	SLANG_SHADER_STAGE_VERTEX,
	SLANG_SHADER_STAGE_FRAGMENT,
	SLANG_SHADER_STAGE_COMPUTE,
	SLANG_SHADER_STAGE_TESSELLATION_CONTROL,
	SLANG_SHADER_STAGE_TESSELLATION_EVALUATION,
	SLANG_SHADER_STAGE_GEOMETRY
};

// Source language enum (maps to SlangSourceLanguage)
enum SlangSourceLanguageType
{
	SLANG_LANG_UNKNOWN = 0,
	SLANG_LANG_SLANG = 1,
	SLANG_LANG_GLSL = 5,
	SLANG_LANG_DEFAULT = SLANG_LANG_GLSL  // Default to GLSL for compatibility
};

struct Flux_SlangCompileResult
{
	bool m_bSuccess = false;
	std::string m_strError;
	Zenith_Vector<uint32_t> m_axSpirv;
	Flux_ShaderReflection m_xReflection;
};

// Result for paired graphics pipeline compilation
struct Flux_SlangGraphicsPipelineResult
{
	bool m_bSuccess = false;
	std::string m_strError;
	Zenith_Vector<uint32_t> m_axVertexSpirv;
	Zenith_Vector<uint32_t> m_axFragmentSpirv;
	Flux_ShaderReflection m_xVertexReflection;
	Flux_ShaderReflection m_xFragmentReflection;
};

class Flux_SlangCompiler
{
	friend class Zenith_UnitTests;
public:
	static void Initialise();
	static void Shutdown();
	static bool IsInitialised();

	static bool Compile(const std::string& strPath, SlangShaderStage eStage, Flux_SlangCompileResult& xResultOut);

	static bool CompileFromSource(const std::string& strSource, const std::string& strEntryPoint,
								   SlangShaderStage eStage, Flux_SlangCompileResult& xResultOut,
								   const char* szSourceName = nullptr, const char* szDirectory = nullptr,
								   SlangSourceLanguageType eLanguage = SLANG_LANG_DEFAULT);

	// Compile vertex and fragment shaders together in a single request.
	// This ensures Slang preserves interface variables (varyings) between stages,
	// preventing optimization from removing unused inputs in the fragment shader.
	static bool CompileGraphicsPipeline(const std::string& strVertexPath, const std::string& strFragmentPath,
										 Flux_SlangGraphicsPipelineResult& xResultOut);

private:
	// Splits a file path into directory and filename at the last '/' or '\'.
	// If no separator is found, strDirectoryOut is "." and strFileNameOut is the full path.
	static void SplitFilePath(const std::string& strPath, std::string& strFileNameOut, std::string& strDirectoryOut);

	// Creates a Slang compile request configured with SPIR-V target, search paths, and compiler flags.
	// Returns the request pointer via pxRequestOut and the target index via iTargetIndexOut.
	// Returns false and populates strErrorOut on failure.
	static bool CreateConfiguredCompileRequest(void*& pxRequestOut, int& iTargetIndexOut,
											   const char* const* aszSearchPaths, u_int uSearchPathCount,
											   std::string& strErrorOut);

	// Extracts SPIR-V code from a compiled entry point blob into axSpirvOut.
	// Returns false and populates strErrorOut on failure.
	static bool ExtractSpirvBlob(void* pxRequest, int iEntryPoint, int iTargetIndex,
								 Zenith_Vector<uint32_t>& axSpirvOut, std::string& strErrorOut);

	// Extracts a single parameter's binding info from reflection data.
	// Returns true if the parameter is a valid descriptor binding (not a varying).
	static bool ExtractParameterBinding(void* pxParam, void* pxTypeLayout, Flux_ReflectedBinding& xBindingOut);

	static void ExtractReflection(void* pxEntryPointReflection, Flux_ShaderReflection& xReflectionOut);
	static DescriptorType SlangTypeToDescriptorType(void* pxTypeLayout);
};
