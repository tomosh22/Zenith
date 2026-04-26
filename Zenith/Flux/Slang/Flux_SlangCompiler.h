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

// Stage mask bit flags. Used in Flux_ReflectedBinding::m_uStageMask to record
// which shader stages reference a resource. Bit values match a fresh enum, not
// vk::ShaderStageFlagBits, so the .spv.refl format stays backend-neutral.
enum FluxShaderStageBit : u_int
{
	FLUX_SHADER_STAGE_BIT_VERTEX                 = 1u << 0,
	FLUX_SHADER_STAGE_BIT_FRAGMENT               = 1u << 1,
	FLUX_SHADER_STAGE_BIT_COMPUTE                = 1u << 2,
	FLUX_SHADER_STAGE_BIT_TESS_CONTROL           = 1u << 3,
	FLUX_SHADER_STAGE_BIT_TESS_EVAL              = 1u << 4,
	FLUX_SHADER_STAGE_BIT_GEOMETRY               = 1u << 5,
};

// Finer-grained resource taxonomy for reflection v2. The legacy BindingType
// (BINDING_TYPE_BUFFER / TEXTURE / STORAGE_BUFFER / STORAGE_IMAGE / ...) is
// kept for backward compatibility with the existing pipeline-layout code; this
// new enum carries the distinctions Slang reflection actually exposes so the
// codegen and future backends can project the right descriptor types.
enum FluxResourceKind : u_int
{
	FLUX_RESOURCE_KIND_UNKNOWN                = 0,
	FLUX_RESOURCE_KIND_CONSTANT_BUFFER        = 1,  // cbuffer / uniform block
	FLUX_RESOURCE_KIND_STRUCTURED_BUFFER      = 2,  // StructuredBuffer<T> (read-only SSBO)
	FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER   = 3,  // RWStructuredBuffer<T>
	FLUX_RESOURCE_KIND_BYTE_ADDRESS_BUFFER    = 4,
	FLUX_RESOURCE_KIND_RW_BYTE_ADDRESS_BUFFER = 5,
	FLUX_RESOURCE_KIND_TEXTURE                = 6,  // Texture2D etc. (separate texture)
	FLUX_RESOURCE_KIND_RW_TEXTURE             = 7,  // RWTexture2D etc. (storage image)
	FLUX_RESOURCE_KIND_SAMPLER                = 8,  // SamplerState
	FLUX_RESOURCE_KIND_COMBINED_TEXTURE_SAMPLER = 9,  // Sampler2D / sampler2D
	FLUX_RESOURCE_KIND_ACCELERATION_STRUCTURE = 10,
	FLUX_RESOURCE_KIND_UNBOUNDED_TEXTURE_ARRAY = 11,
	FLUX_RESOURCE_KIND_PARAMETER_BLOCK        = 12,  // ParameterBlock<T>
};

// One reflected field within a constant-buffer / parameter-block layout.
// Captured at codegen time so the generator can emit C++ structs whose
// sizeof / offsetof match the GPU side bit-for-bit.
struct Flux_ReflectedField
{
	std::string m_strName;
	u_int       m_uOffset       = 0;
	u_int       m_uSize         = 0;
	u_int       m_uArrayCount   = 1;  // 1 for scalars, >1 for fixed arrays, 0 for unbounded
	std::string m_strTypeName;        // e.g. "float4x4", "uint", "DirectionalLight"
};

struct Flux_ReflectedBinding
{
	// Legacy fields (reflection v1) — still authoritative for the existing
	// FromReflection layout code. New code should prefer m_eResourceKind.
	BindingType m_eType = BINDING_TYPE_MAX;
	u_int       m_uSet = 0;
	u_int       m_uBinding = 0;
	std::string m_strName;
	u_int       m_uSize = 0;

	// Reflection v2 additions. Default values mean "v1 source"; a v1 .spv.refl
	// file deserialised by v2 readers leaves these at their defaults.
	FluxResourceKind            m_eResourceKind     = FLUX_RESOURCE_KIND_UNKNOWN;
	u_int                       m_uDescriptorCount  = 1;     // 0 = unbounded
	u_int                       m_uStageMask        = 0;     // FluxShaderStageBit bitmask
	std::string                 m_strParameterBlockPath;     // e.g. "frame.lights" — empty for top-level
	Zenith_Vector<Flux_ReflectedField> m_axFields;            // populated for CB/PB only
};

class Flux_ShaderReflection
{
public:
	Flux_ShaderReflection() = default;

	// Single O(1) lookup returning the full reflected binding (or nullptr if
	// the name is not present). Callers that need just the handle extract
	// {m_uSet, m_uBinding} from the returned pointer; callers that need the
	// reflected type (BindingType) use it directly. m_xBindingMap stores
	// indices into m_axBindings, so GetBinding is a single map lookup with
	// no per-call vector scan.
	const Flux_ReflectedBinding* GetBinding(const char* szName) const;
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
	// #TODO: Replace with engine hash map. Map stores indices into
	// m_axBindings so GetBinding returns a stable pointer into the vector.
	std::unordered_map<std::string, u_int> m_xBindingMap;
};

// Description for a Slang program compile. One module file (mega-file per
// subsystem) with one or more named entry points. Empty entry-point strings
// mean "this stage is not present in this program". This is the public input
// to Flux_SlangCompiler::CompileProgram and matches the registry schema.
struct Flux_SlangProgramDesc
{
	const char* m_szModuleName    = nullptr;  // module path relative to a search root (no extension)
	const char* m_szVertexEntry   = nullptr;
	const char* m_szFragmentEntry = nullptr;
	const char* m_szComputeEntry  = nullptr;
	const char* m_szTargetProfile = "spirv_1_3";
};

// Compiled artifacts for a multi-entry program. Each populated SPIR-V vector
// matches a populated entry-point name in the matching desc. Reflection here
// is the linked program's reflection (single source of truth), not per-stage.
struct Flux_SlangProgramResult
{
	bool                  m_bSuccess = false;
	std::string           m_strError;
	Zenith_Vector<uint32_t> m_axVertexSpirv;
	Zenith_Vector<uint32_t> m_axFragmentSpirv;
	Zenith_Vector<uint32_t> m_axComputeSpirv;
	Flux_ShaderReflection m_xReflection;
};

class Flux_SlangCompiler
{
	friend class Zenith_UnitTests;
public:
	static void Initialise();
	static void Shutdown();
	static bool IsInitialised();

	// Modern Slang compile path. Loads xDesc.m_szModuleName as a Slang module
	// from the configured search paths, finds the named entry points,
	// composes/links them, and emits SPIR-V plus linked reflection. Search
	// paths come from ::AddSearchPath. Slang is the only supported source
	// language post-migration — there is no GLSL fallback.
	static bool CompileProgram(const Flux_SlangProgramDesc& xDesc, Flux_SlangProgramResult& xResultOut);

	// Add a search path used by CompileProgram's session for module imports.
	// Call before CompileProgram. The compiler keeps a session-scoped list;
	// adding the same path twice is harmless.
	static void AddSearchPath(const char* szPath);

	// Map a Slang TypeLayoutReflection (passed as void*) to the engine's
	// legacy BindingType taxonomy. Public so the v2 reflection helper at
	// file scope in the .cpp can reuse it.
	static BindingType SlangTypeToBindingType(void* pxTypeLayout);
};
