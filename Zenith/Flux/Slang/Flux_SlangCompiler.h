#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Flux/Flux_Enums.h"
#include <string>

class Zenith_DataStream;

struct Flux_BindingHandle
{
	u_int            m_uSet             = UINT32_MAX;
	u_int            m_uBinding         = UINT32_MAX;
	FluxResourceKind m_eKind            = FLUX_RESOURCE_KIND_UNKNOWN;
	u_int            m_uDescriptorCount = 1;   // 0 = unbounded (bindless table)

	bool IsValid() const { return m_uSet != UINT32_MAX && m_uBinding != UINT32_MAX; }
};

// Codegen handle for a Slang specialization constant (Flux Shader System Overhaul
// — Stage 3a). Emitted per program next to the binding handles: a feature passes
// m_szName to Flux_SpecConstantTable::AddBool/AddUInt and the backend resolves it
// to m_uConstantId via reflection at pipeline-build (name-keyed, so a hot-reload
// that renumbers IDs stays correct). m_uConstantId is the baked ID for a drift
// tripwire. Int-family only → m_uSize is always 4.
struct Flux_SpecConstantHandle
{
	const char* m_szName        = nullptr;
	u_int       m_uConstantId   = UINT32_MAX;
	u_int       m_uSize         = 4;
	u_int       m_uDefaultValue = 0;

	bool IsValid() const { return m_szName != nullptr && m_uConstantId != UINT32_MAX; }
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

// FluxResourceKind — the canonical resource taxonomy — now lives in
// Flux/Flux_Enums.h (included above) so both the pipeline-layout data model
// (Flux_Types.h) and this reflection layer can name it from one definition.

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
	u_int       m_uSet = 0;
	u_int       m_uBinding = 0;
	std::string m_strName;
	u_int       m_uSize = 0;

	// Resource kind drives the descriptor type, count, stages, and the typed
	// binding handle. (The legacy BindingType m_eType field was removed with
	// the binding-model overhaul — FluxResourceKind is the only type now.)
	FluxResourceKind            m_eResourceKind     = FLUX_RESOURCE_KIND_UNKNOWN;
	u_int                       m_uDescriptorCount  = 1;     // 0 = unbounded
	u_int                       m_uStageMask        = 0;     // FluxShaderStageBit bitmask
	std::string                 m_strParameterBlockPath;     // e.g. "frame.lights" — empty for top-level
	Zenith_Vector<Flux_ReflectedField> m_axFields;            // populated for CB/PB only

	// Phase 5.5: does ANY entry point STATICALLY sample this binding (OR'd across
	// stages)? Distinguishes a member a shader actually uses from one it merely
	// declares via the #include'd spine (Common/Bindings.slang lists g_xGlobal/
	// g_xView/g_axTextures in EVERY program's reflection). Baked at compile time
	// from Slang's IMetadata::isParameterLocationUsed; drives the VIEW/GLOBAL
	// graph-Read() validator (Flux_ViewSetBinding). Defaults true so any path that
	// does not populate it over-approximates (never under-demands a Read()).
	bool                        m_bStaticallyUsed   = true;
};

// One reflected specialization constant (Slang [SpecializationConstant]).
// Captured from the linked program layout (Stage-0 probe E4 pinned the API:
// getOffset(SPECIALIZATION_CONSTANT) for the ID + getDefaultValueInt for the
// default). Int-family only (bool/int) — Slang exposes int-family default reads
// only — so m_uSize is always 4 (bool/int spec constants are 32-bit in SPIR-V).
// Spec constants occupy NO descriptor binding; they ride the sidecar (v5) beside
// m_axBindings and drive per-mode pipeline variants, never the root signature.
struct Flux_ReflectedSpecConstant
{
	std::string m_strName;
	u_int       m_uConstantId  = 0;   // SPIR-V constant_id (declaration order, 0..N-1)
	u_int       m_uSize         = 4;  // bytes; always 4 (int-family)
	u_int       m_uDefaultValue = 0;  // default packed as u32 (bool: 0/1)
	std::string m_strTypeName;        // e.g. "bool", "int"
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

	// Phase 5.5: stamp the compile-time static-use bit onto binding [uIndex].
	// Called from CompileProgram after the Slang is-used query (the only place
	// with access to the linked program's IMetadata); read back from the
	// .spv.refl sidecar at runtime. No-op if uIndex is out of range.
	void SetBindingStaticUse(u_int uIndex, bool bUsed);

	// Specialization-constant table (Flux Shader System Overhaul — Stage 3a).
	// Populated by ExtractSpecConstants after the binding walk; serialized in the
	// v5 sidecar; merged per-stage (dedup by constant id) at shader load. Lookups
	// are by name (the codegen handle carries the name; the backend resolves it to
	// the runtime id). Linear scan — a program declares only a handful.
	void AddSpecConstant(const Flux_ReflectedSpecConstant& xSpec);
	const Flux_ReflectedSpecConstant* GetSpecConstant(const char* szName) const;
	const Zenith_Vector<Flux_ReflectedSpecConstant>& GetSpecConstants() const { return m_axSpecConstants; }

private:
	Zenith_Vector<Flux_ReflectedBinding> m_axBindings;
	// Map stores indices into m_axBindings so GetBinding returns a stable
	// pointer into the vector.
	Zenith_HashMap<std::string, u_int> m_xBindingMap;
	// Spec constants (v5). Kept as a small flat vector — no map (handful per program).
	Zenith_Vector<Flux_ReflectedSpecConstant> m_axSpecConstants;
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

	// Slang debug-info emission (Flux Shader System Overhaul — Stage 1). Set TRUE
	// ONLY by the runtime-compile path (Zenith_Vulkan_Shader::InitialiseFromProgramSource)
	// under #ifdef ZENITH_DEBUG, so RenderDoc sees Slang source in the SPIR-V it captures.
	// FluxCompiler leaves it FALSE → the checked-in .spv/.spv.refl stay byte-identical.
	// Adds DebugInformation=MAXIMAL without changing the optimized code (semantics-preserving).
	bool        m_bEmitDebugInfo     = false;
	// Opt-in (`--shader-debug-o0`): disable optimization for the runtime compile. NOT paired
	// with m_bEmitDebugInfo by default — O0 changes float re-association (moves pixels), so it
	// is a deliberate deep-debug opt-in, never the default.
	bool        m_bDisableOptimization = false;
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

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
// Result of a single in-memory Slang probe compile (Flux_SlangCompiler::CompileProbeFromSource).
// The Stage-0 capability probe suite (Flux_SlangProbes.Tests.inl) uses it to lock in the exact
// Slang-language behaviours the shader-system overhaul stands on — `private` access control under
// textual include (D1), visibility-invariant reflection (D1 fork), unreferenced-block space
// assignment (spine spaces 0/1/2), spec-constant IDs (D4/D5), and generic-vs-concrete SPIR-V
// parity (D2/Stage 4). A failed compile is a valid, inspectable outcome — the probe never asserts.
struct Flux_SlangProbeResult
{
	bool                    m_bCompiled = false;      // front-end load (+ link, if an entry point was requested) succeeded
	bool                    m_bHasReflection = false; // m_xReflection populated (a compute entry point was linked)
	std::string             m_strDiagnostics;         // Slang diagnostics (compile errors / warnings), empty on clean success
	Flux_ShaderReflection   m_xReflection;            // linked-program reflection via the engine's ExtractV2Reflection
	Zenith_Vector<uint32_t> m_axSpirv;                // linked compute SPIR-V words (entry 0), when an entry point was requested

	// Raw spec-constant reflection captured directly from the linked program layout (NOT via
	// ExtractV2Reflection, which drops the SpecializationConstant category). Lets probe E4 pin the
	// D5 extraction API and assert current behaviour.
	struct SpecConstant { std::string m_strName; u_int m_uId = 0; int64_t m_iDefault = 0; bool m_bHasDefault = false; };
	Zenith_Vector<SpecConstant> m_axSpecConstants;
};
#endif // ZENITH_WINDOWS && ZENITH_VULKAN

class Flux_SlangCompiler
{
	friend class Zenith_UnitTests;
public:
	static void Initialise();
	static void Shutdown();
	static bool IsInitialised();

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
	// Stage-0 capability probe. Compiles a single in-memory Slang source string through the SAME
	// session config the engine's CompileProgram uses (search paths incl. SHADER_SOURCE_ROOT +
	// column-major). Lazily calls Initialise() (unit tests run before Flux brings Slang up). If
	// szComputeEntry is non-null it is composed/linked and its SPIR-V + reflection emitted (forces
	// full back-end codegen); otherwise only the front-end module load runs (a pure accepts/rejects
	// probe). Returns xOut.m_bCompiled; never asserts — a rejected compile fills m_strDiagnostics.
	// bEmitDebugInfo forces DebugInformation=MAXIMAL for the compile (Stage-1 debug-info probe E6).
	static bool CompileProbeFromSource(const char* szSource, const char* szComputeEntry, Flux_SlangProbeResult& xOut,
									   bool bEmitDebugInfo = false);
#endif // ZENITH_WINDOWS && ZENITH_VULKAN

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
};
