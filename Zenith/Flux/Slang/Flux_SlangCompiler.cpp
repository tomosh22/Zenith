#include "Zenith.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_Types.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_WINDOWS
#include <slang.h>
#include <slang-com-ptr.h>

#include <vector>

static Slang::ComPtr<slang::IGlobalSession> s_pxGlobalSession;

// Search paths used by CompileProgram. Populated via AddSearchPath. A new
// ISession is built per-compile so it picks up updates after AddSearchPath
// without needing teardown/reinit.
static std::vector<std::string> s_axSearchPaths;
#endif // ZENITH_WINDOWS

const Flux_ReflectedBinding* Flux_ShaderReflection::GetBinding(const char* szName) const
{
	auto it = m_xBindingMap.find(szName);
	if (it == m_xBindingMap.end())
	{
		// Log all available bindings to help debug
		Zenith_Log(LOG_CATEGORY_RENDERER, "GetBinding('%s') failed. Available bindings (%u):",
			szName, static_cast<u_int>(m_xBindingMap.size()));
		for (const auto& pair : m_xBindingMap)
		{
			const Flux_ReflectedBinding& xBinding = m_axBindings.Get(pair.second);
			Zenith_Log(LOG_CATEGORY_RENDERER, "  '%s' -> set=%u, binding=%u",
				pair.first.c_str(), xBinding.m_uSet, xBinding.m_uBinding);
		}
		return nullptr;
	}
	return &m_axBindings.Get(it->second);
}

u_int Flux_ShaderReflection::GetBindingPoint(const char* szName) const
{
	const Flux_ReflectedBinding* pxBinding = GetBinding(szName);
	Zenith_Assert(pxBinding != nullptr, "Shader binding '%s' not found in reflection", szName);
	return pxBinding->m_uBinding;
}

u_int Flux_ShaderReflection::GetDescriptorSet(const char* szName) const
{
	const Flux_ReflectedBinding* pxBinding = GetBinding(szName);
	Zenith_Assert(pxBinding != nullptr, "Shader binding '%s' not found in reflection", szName);
	return pxBinding->m_uSet;
}

void Flux_ShaderReflection::PopulateLayout(Flux_PipelineLayout& xLayoutOut) const
{
	xLayoutOut.m_uNumBindingGroups = 0;

	for (u_int u = 0; u < m_axBindings.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		if (xBinding.m_uSet >= FLUX_MAX_BINDING_GROUPS)
		{
			continue;
		}
		if (xBinding.m_uBinding >= FLUX_MAX_BINDINGS_PER_GROUP)
		{
			continue;
		}

		if (xBinding.m_uSet + 1 > xLayoutOut.m_uNumBindingGroups)
		{
			xLayoutOut.m_uNumBindingGroups = xBinding.m_uSet + 1;
		}

		xLayoutOut.m_axBindingGroups[xBinding.m_uSet].m_axBindings[xBinding.m_uBinding].m_eType = xBinding.m_eType;
	}

	// Fill gaps in binding indices with placeholder types.
	// The pipeline builder stops at the first BINDING_TYPE_MAX, so sparse
	// bindings (e.g. shadow shaders that skip texture slots) need gaps filled.
	for (u_int uSet = 0; uSet < xLayoutOut.m_uNumBindingGroups; uSet++)
	{
		Flux_BindingGroupLayout& xSetLayout = xLayoutOut.m_axBindingGroups[uSet];

		u_int uMaxBinding = 0;
		for (u_int uBinding = 0; uBinding < FLUX_MAX_BINDINGS_PER_GROUP; uBinding++)
		{
			if (xSetLayout.m_axBindings[uBinding].m_eType != BINDING_TYPE_MAX)
			{
				uMaxBinding = uBinding;
			}
		}

		for (u_int uBinding = 0; uBinding < uMaxBinding; uBinding++)
		{
			if (xSetLayout.m_axBindings[uBinding].m_eType == BINDING_TYPE_MAX)
			{
				xSetLayout.m_axBindings[uBinding].m_eType = BINDING_TYPE_BUFFER;
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
		m_xBindingMap[xBinding.m_strName] = u;
	}
}

// Magic and version for the .spv.refl sidecar format.
// v1: legacy flat (set, binding, name, size, BindingType) — written by GLSL path.
// v2: adds resource-kind taxonomy, descriptor count, stage mask, parameter-block
//     path, and reflected CB fields. v2 readers accept v1 files (new fields read
//     as defaults) so a partially-migrated tree stays loadable.
static constexpr u_int32 kFluxReflectionMagic        = 0x46525846; // 'FXRF'
static constexpr u_int32 kFluxReflectionVersionV1    = 1;
static constexpr u_int32 kFluxReflectionVersionV2    = 2;
static constexpr u_int32 kFluxReflectionVersion      = kFluxReflectionVersionV2;

void Flux_ShaderReflection::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << kFluxReflectionMagic;
	xStream << kFluxReflectionVersion;
	const u_int uCount = m_axBindings.GetSize();
	xStream << uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		// v1 fields
		xStream << static_cast<u_int>(xBinding.m_eType);
		xStream << xBinding.m_uSet;
		xStream << xBinding.m_uBinding;
		xStream << xBinding.m_strName;
		xStream << xBinding.m_uSize;
		// v2 fields
		xStream << static_cast<u_int>(xBinding.m_eResourceKind);
		xStream << xBinding.m_uDescriptorCount;
		xStream << xBinding.m_uStageMask;
		xStream << xBinding.m_strParameterBlockPath;
		const u_int uFieldCount = xBinding.m_axFields.GetSize();
		xStream << uFieldCount;
		for (u_int f = 0; f < uFieldCount; f++)
		{
			const Flux_ReflectedField& xField = xBinding.m_axFields.Get(f);
			xStream << xField.m_strName;
			xStream << xField.m_uOffset;
			xStream << xField.m_uSize;
			xStream << xField.m_uArrayCount;
			xStream << xField.m_strTypeName;
		}
	}
}

void Flux_ShaderReflection::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int32 uMagic = 0;
	u_int32 uVersion = 0;
	xStream >> uMagic;
	xStream >> uVersion;
	Zenith_Assert(uMagic == kFluxReflectionMagic,
		"Flux_ShaderReflection: bad magic 0x%08X (expected 0x%08X). Rerun FluxCompiler.", uMagic, kFluxReflectionMagic);
	Zenith_Assert(uVersion == kFluxReflectionVersionV1 || uVersion == kFluxReflectionVersionV2,
		"Flux_ShaderReflection: unsupported format v%u (expected v%u or v%u). Rerun FluxCompiler.",
		uVersion, kFluxReflectionVersionV1, kFluxReflectionVersionV2);
	u_int uCount;
	xStream >> uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		Flux_ReflectedBinding xBinding;
		u_int uType;
		xStream >> uType;
		xBinding.m_eType = static_cast<BindingType>(uType);
		xStream >> xBinding.m_uSet;
		xStream >> xBinding.m_uBinding;
		xStream >> xBinding.m_strName;
		xStream >> xBinding.m_uSize;
		if (uVersion >= kFluxReflectionVersionV2)
		{
			u_int uKind;
			xStream >> uKind;
			xBinding.m_eResourceKind = static_cast<FluxResourceKind>(uKind);
			xStream >> xBinding.m_uDescriptorCount;
			xStream >> xBinding.m_uStageMask;
			xStream >> xBinding.m_strParameterBlockPath;
			u_int uFieldCount;
			xStream >> uFieldCount;
			for (u_int f = 0; f < uFieldCount; f++)
			{
				Flux_ReflectedField xField;
				xStream >> xField.m_strName;
				xStream >> xField.m_uOffset;
				xStream >> xField.m_uSize;
				xStream >> xField.m_uArrayCount;
				xStream >> xField.m_strTypeName;
				xBinding.m_axFields.PushBack(xField);
			}
		}
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

	// Slang-only — `enableGLSL` is intentionally left false. The engine's
	// compile path uses CompileProgram (modern session/module/link API)
	// for .slang sources only; the GLSL compatibility module is no
	// longer needed and slang-glslang.dll is not deployed.
	SlangGlobalSessionDesc xDesc = {};

	SlangResult xResult = slang::createGlobalSession(&xDesc, s_pxGlobalSession.writeRef());
	if (SLANG_FAILED(xResult))
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Failed to create Slang global session");
	}
}

void Flux_SlangCompiler::Shutdown()
{
	s_pxGlobalSession = nullptr;
	s_axSearchPaths.clear();
}

bool Flux_SlangCompiler::IsInitialised()
{
	return s_pxGlobalSession != nullptr;
}

BindingType Flux_SlangCompiler::SlangTypeToBindingType(void* pxTypeLayoutVoid)
{
	slang::TypeLayoutReflection* pxTypeLayout = static_cast<slang::TypeLayoutReflection*>(pxTypeLayoutVoid);

	slang::TypeReflection::Kind eKind = pxTypeLayout->getKind();

	// Detect unbounded arrays (e.g., sampler2D g_axTextures[])
	if (eKind == slang::TypeReflection::Kind::Array)
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		if (pxType && pxType->getElementCount() == 0)
		{
			return BINDING_TYPE_UNBOUNDED_TEXTURES;
		}
	}

	slang::BindingType eBindingType = pxTypeLayout->getDescriptorSetDescriptorRangeType(0, 0);

	switch (eBindingType)
	{
	case slang::BindingType::ConstantBuffer:
		return BINDING_TYPE_BUFFER;
	case slang::BindingType::RawBuffer:
	case slang::BindingType::MutableRawBuffer:
		return BINDING_TYPE_STORAGE_BUFFER;
	case slang::BindingType::Texture:
	case slang::BindingType::CombinedTextureSampler:
		return BINDING_TYPE_TEXTURE;
	case slang::BindingType::MutableTexture:
		return BINDING_TYPE_STORAGE_IMAGE;
	case slang::BindingType::Sampler:
		return BINDING_TYPE_TEXTURE;
	default:
		break;
	}

	switch (eKind)
	{
	case slang::TypeReflection::Kind::ConstantBuffer:
	case slang::TypeReflection::Kind::ParameterBlock:
		return BINDING_TYPE_BUFFER;
	case slang::TypeReflection::Kind::Resource:
		return BINDING_TYPE_TEXTURE;
	case slang::TypeReflection::Kind::SamplerState:
		return BINDING_TYPE_TEXTURE;
	case slang::TypeReflection::Kind::ShaderStorageBuffer:
		return BINDING_TYPE_STORAGE_BUFFER;
	default:
		return BINDING_TYPE_BUFFER;
	}
}

//==========================================================================
// Slang session/module/link API path (CompileProgram).
//
// Loads the requested module via ISession::loadModule (which uses configured
// search paths to resolve `import` statements transitively), finds the named
// entry points, composes them into a single IComponentType, links, and emits
// SPIR-V plus full reflection. This is the only compile path — Slang is the
// canonical source language post-migration.
//==========================================================================

void Flux_SlangCompiler::AddSearchPath(const char* szPath)
{
	if (!szPath || !szPath[0])
	{
		return;
	}
	for (const std::string& strExisting : s_axSearchPaths)
	{
		if (strExisting == szPath)
		{
			return;
		}
	}
	s_axSearchPaths.emplace_back(szPath);
}

// Resource taxonomy for v2 reflection. Maps Slang's slang::BindingType plus
// Slang TypeReflection::Kind into FluxResourceKind. Distinguishes separate
// texture/sampler from combined samplers, structured-buffer variants, RW
// textures, and unbounded texture arrays.
static FluxResourceKind ClassifyResource(slang::TypeLayoutReflection* pxTypeLayout)
{
	if (!pxTypeLayout)
	{
		return FLUX_RESOURCE_KIND_UNKNOWN;
	}

	const slang::TypeReflection::Kind eKind = pxTypeLayout->getKind();
	if (eKind == slang::TypeReflection::Kind::Array)
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		if (pxType && pxType->getElementCount() == 0)
		{
			return FLUX_RESOURCE_KIND_UNBOUNDED_TEXTURE_ARRAY;
		}
	}

	const slang::BindingType eBindingType = pxTypeLayout->getDescriptorSetDescriptorRangeType(0, 0);
	switch (eBindingType)
	{
	case slang::BindingType::ConstantBuffer:           return FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	case slang::BindingType::ParameterBlock:           return FLUX_RESOURCE_KIND_PARAMETER_BLOCK;
	case slang::BindingType::RawBuffer:                return FLUX_RESOURCE_KIND_STRUCTURED_BUFFER;
	case slang::BindingType::MutableRawBuffer:         return FLUX_RESOURCE_KIND_RW_STRUCTURED_BUFFER;
	case slang::BindingType::Texture:                  return FLUX_RESOURCE_KIND_TEXTURE;
	case slang::BindingType::MutableTexture:           return FLUX_RESOURCE_KIND_RW_TEXTURE;
	case slang::BindingType::Sampler:                  return FLUX_RESOURCE_KIND_SAMPLER;
	case slang::BindingType::CombinedTextureSampler:   return FLUX_RESOURCE_KIND_COMBINED_TEXTURE_SAMPLER;
	case slang::BindingType::RayTracingAccelerationStructure:
		return FLUX_RESOURCE_KIND_ACCELERATION_STRUCTURE;
	default:
		break;
	}

	switch (eKind)
	{
	case slang::TypeReflection::Kind::ConstantBuffer:    return FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
	case slang::TypeReflection::Kind::ParameterBlock:    return FLUX_RESOURCE_KIND_PARAMETER_BLOCK;
	case slang::TypeReflection::Kind::ShaderStorageBuffer: return FLUX_RESOURCE_KIND_STRUCTURED_BUFFER;
	case slang::TypeReflection::Kind::Resource:          return FLUX_RESOURCE_KIND_TEXTURE;
	case slang::TypeReflection::Kind::SamplerState:      return FLUX_RESOURCE_KIND_SAMPLER;
	default: break;
	}
	return FLUX_RESOURCE_KIND_UNKNOWN;
}

// Walk a constant-buffer / parameter-block element type and record each
// scalar/vector/matrix/struct field with its byte offset and size. Used by
// codegen to emit C++ structs whose layout matches the GPU side.
static void ExtractFieldsFromStruct(slang::TypeLayoutReflection* pxTypeLayout,
									  Zenith_Vector<Flux_ReflectedField>& axFieldsOut)
{
	if (!pxTypeLayout) return;
	if (pxTypeLayout->getKind() != slang::TypeReflection::Kind::Struct) return;

	const u_int uFieldCount = static_cast<u_int>(pxTypeLayout->getFieldCount());
	for (u_int u = 0; u < uFieldCount; u++)
	{
		slang::VariableLayoutReflection* pxField = pxTypeLayout->getFieldByIndex(u);
		if (!pxField) continue;

		Flux_ReflectedField xField;
		xField.m_strName   = pxField->getName() ? pxField->getName() : "";
		xField.m_uOffset   = static_cast<u_int>(pxField->getOffset());
		slang::TypeLayoutReflection* pxFieldType = pxField->getTypeLayout();
		if (pxFieldType)
		{
			xField.m_uSize = static_cast<u_int>(pxFieldType->getSize());
			slang::TypeReflection* pxType = pxFieldType->getType();
			if (pxType)
			{
				xField.m_uArrayCount = static_cast<u_int>(pxType->getElementCount());
				if (pxType->getName())
				{
					xField.m_strTypeName = pxType->getName();
				}
			}
		}
		axFieldsOut.PushBack(xField);
	}
}

// Build a v2 reflected binding from a top-level program parameter. Produces
// resource kind, descriptor count, parameter-block path, and (for CB/PB
// elements) the inner field list.
static bool BuildV2BindingFromParam(slang::VariableLayoutReflection* pxParam,
									 const std::string& strParentPath,
									 Flux_ReflectedBinding& xBindingOut)
{
	if (!pxParam) return false;
	slang::TypeLayoutReflection* pxTypeLayout = pxParam->getTypeLayout();
	if (!pxTypeLayout) return false;

	slang::ParameterCategory eCategory = pxParam->getCategory();
	if (eCategory == slang::ParameterCategory::VaryingInput ||
		eCategory == slang::ParameterCategory::VaryingOutput ||
		eCategory == slang::ParameterCategory::VertexInput ||
		eCategory == slang::ParameterCategory::FragmentOutput)
	{
		return false;
	}

	xBindingOut.m_eResourceKind = ClassifyResource(pxTypeLayout);
	// Entry-point system values (SV_VertexID, SV_InstanceID, SV_Position
	// inputs etc.) reach here with no descriptor binding — Slang surfaces
	// them as parameters with no descriptor range and no resource kind.
	// Without this guard, codegen emits fake constant-buffer bindings
	// `kvertexId_*`, `kinstanceId_*` and PopulateLayout converts them into
	// uniform-buffer descriptor slots, breaking the root signature.
	if (xBindingOut.m_eResourceKind == FLUX_RESOURCE_KIND_UNKNOWN)
	{
		return false;
	}

	xBindingOut.m_strName = pxParam->getName() ? pxParam->getName() : "";
	if (xBindingOut.m_strName.empty())
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		if (pxType && pxType->getName())
		{
			xBindingOut.m_strName = pxType->getName();
		}
	}

	xBindingOut.m_uSet               = static_cast<u_int>(pxParam->getBindingSpace());
	xBindingOut.m_uBinding           = static_cast<u_int>(pxParam->getBindingIndex());
	xBindingOut.m_uSize              = static_cast<u_int>(pxTypeLayout->getSize());
	xBindingOut.m_eType              = Flux_SlangCompiler::SlangTypeToBindingType(pxTypeLayout);
	xBindingOut.m_strParameterBlockPath = strParentPath;

	const slang::TypeReflection::Kind eKind = pxTypeLayout->getKind();
	if (eKind == slang::TypeReflection::Kind::Array)
	{
		slang::TypeReflection* pxType = pxTypeLayout->getType();
		const u_int uCount = pxType ? static_cast<u_int>(pxType->getElementCount()) : 1;
		xBindingOut.m_uDescriptorCount = uCount; // 0 means unbounded
	}
	else
	{
		xBindingOut.m_uDescriptorCount = 1;
	}

	// CB/PB inner fields: drill into the element type layout and record
	// fields. For CB/PB, getSize() on the binding's TypeLayoutReflection is
	// the descriptor footprint, not the payload byte size — use the element
	// layout's size so the generator's sizeof() static_assert reflects the
	// actual struct size on disk.
	if (xBindingOut.m_eResourceKind == FLUX_RESOURCE_KIND_CONSTANT_BUFFER ||
		xBindingOut.m_eResourceKind == FLUX_RESOURCE_KIND_PARAMETER_BLOCK)
	{
		slang::TypeLayoutReflection* pxElement = pxTypeLayout->getElementTypeLayout();
		if (pxElement)
		{
			xBindingOut.m_uSize = static_cast<u_int>(pxElement->getSize());
			ExtractFieldsFromStruct(pxElement, xBindingOut.m_axFields);
		}
	}
	else if (eKind == slang::TypeReflection::Kind::Struct)
	{
		// Push-constant block / inline struct
		ExtractFieldsFromStruct(pxTypeLayout, xBindingOut.m_axFields);
	}
	return true;
}

// Merge a binding into the reflection, OR'ing the stage mask if a binding at
// the same (set, binding) already exists. New bindings inherit the supplied
// stage bit.
static void MergeBindingWithStageMask(Flux_ShaderReflection& xReflection,
									   const Flux_ReflectedBinding& xCandidate,
									   u_int uStageBit)
{
	const Zenith_Vector<Flux_ReflectedBinding>& axExisting = xReflection.GetBindings();
	for (u_int u = 0; u < axExisting.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xExisting = axExisting.Get(u);
		if (xExisting.m_uSet == xCandidate.m_uSet && xExisting.m_uBinding == xCandidate.m_uBinding)
		{
			// Stage mask is the only field we update on re-encounter; the rest
			// must already match (Slang validates this at link time).
			const_cast<Flux_ReflectedBinding&>(xExisting).m_uStageMask |= uStageBit;
			return;
		}
	}

	Flux_ReflectedBinding xCopy = xCandidate;
	xCopy.m_uStageMask = uStageBit;
	xReflection.AddBinding(xCopy);
}

static u_int SlangStageToFluxStageBit(SlangStage eStage)
{
	switch (eStage)
	{
	case SLANG_STAGE_VERTEX:   return FLUX_SHADER_STAGE_BIT_VERTEX;
	case SLANG_STAGE_FRAGMENT: return FLUX_SHADER_STAGE_BIT_FRAGMENT;
	case SLANG_STAGE_COMPUTE:  return FLUX_SHADER_STAGE_BIT_COMPUTE;
	case SLANG_STAGE_HULL:     return FLUX_SHADER_STAGE_BIT_TESS_CONTROL;
	case SLANG_STAGE_DOMAIN:   return FLUX_SHADER_STAGE_BIT_TESS_EVAL;
	case SLANG_STAGE_GEOMETRY: return FLUX_SHADER_STAGE_BIT_GEOMETRY;
	default:                   return 0;
	}
}

// Walk linked program reflection, including per-entry-point parameters, and
// fold every descriptor binding into xReflectionOut with v2 fields populated
// and stage masks accumulated across entry points.
static void ExtractV2Reflection(slang::ProgramLayout* pxLayout,
								  Flux_ShaderReflection& xReflectionOut)
{
	if (!pxLayout) return;

	// Global-scope parameters appear in every stage that uses them; mark them
	// with the union of all entry-point stages so the descriptor-set layout
	// gets the right access flags.
	const u_int uEntryPointCount = static_cast<u_int>(pxLayout->getEntryPointCount());
	u_int uGlobalStageMask = 0;
	for (u_int ep = 0; ep < uEntryPointCount; ep++)
	{
		slang::EntryPointLayout* pxEntryPoint = pxLayout->getEntryPointByIndex(ep);
		if (!pxEntryPoint) continue;
		uGlobalStageMask |= SlangStageToFluxStageBit(pxEntryPoint->getStage());
	}

	const u_int uParamCount = static_cast<u_int>(pxLayout->getParameterCount());
	for (u_int u = 0; u < uParamCount; u++)
	{
		Flux_ReflectedBinding xBinding;
		if (BuildV2BindingFromParam(pxLayout->getParameterByIndex(u), "", xBinding))
		{
			MergeBindingWithStageMask(xReflectionOut, xBinding, uGlobalStageMask);
		}
	}

	// Per-entry-point parameters (e.g. inline uniform structs declared on the
	// entry point itself) — these only get the owning stage's bit.
	for (u_int ep = 0; ep < uEntryPointCount; ep++)
	{
		slang::EntryPointLayout* pxEntryPoint = pxLayout->getEntryPointByIndex(ep);
		if (!pxEntryPoint) continue;
		const u_int uStageBit = SlangStageToFluxStageBit(pxEntryPoint->getStage());

		for (u_int u = 0; u < pxEntryPoint->getParameterCount(); u++)
		{
			Flux_ReflectedBinding xBinding;
			if (BuildV2BindingFromParam(pxEntryPoint->getParameterByIndex(u), "", xBinding))
			{
				MergeBindingWithStageMask(xReflectionOut, xBinding, uStageBit);
			}
		}
	}

	xReflectionOut.BuildLookupMap();
}

bool Flux_SlangCompiler::CompileProgram(const Flux_SlangProgramDesc& xDesc, Flux_SlangProgramResult& xResultOut)
{
	xResultOut.m_bSuccess = false;
	xResultOut.m_strError.clear();
	xResultOut.m_axVertexSpirv.Clear();
	xResultOut.m_axFragmentSpirv.Clear();
	xResultOut.m_axComputeSpirv.Clear();

	if (!s_pxGlobalSession)
	{
		xResultOut.m_strError = "Slang compiler not initialized";
		return false;
	}
	if (!xDesc.m_szModuleName || !xDesc.m_szModuleName[0])
	{
		xResultOut.m_strError = "Flux_SlangProgramDesc missing module name";
		return false;
	}

	// Build target descriptor — SPIR-V, requested profile.
	slang::TargetDesc xTarget = {};
	xTarget.format  = SLANG_SPIRV;
	xTarget.profile = s_pxGlobalSession->findProfile(xDesc.m_szTargetProfile ? xDesc.m_szTargetProfile : "spirv_1_3");

	// Build session descriptor with current search paths.
	std::vector<const char*> aszPaths;
	aszPaths.reserve(s_axSearchPaths.size() + 1);
	for (const std::string& s : s_axSearchPaths) aszPaths.push_back(s.c_str());
#ifdef SHADER_SOURCE_ROOT
	aszPaths.push_back(SHADER_SOURCE_ROOT);
#endif

	slang::SessionDesc xSessionDesc = {};
	xSessionDesc.targets       = &xTarget;
	xSessionDesc.targetCount   = 1;
	xSessionDesc.searchPaths   = aszPaths.empty() ? nullptr : aszPaths.data();
	xSessionDesc.searchPathCount = static_cast<SlangInt>(aszPaths.size());
	// Match GLM / Vulkan std140 — engine uploads column-major matrices to
	// constant buffers. Slang defaults to row-major, which would make
	// `mul(M, v)` compute `transpose(M_uploaded) * v`. Forcing column-major
	// here aligns Slang's interpretation with the bytes the engine actually
	// writes, so all matrix math (view/proj/invProj/etc.) reads correctly.
	xSessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

	Slang::ComPtr<slang::ISession> pxSession;
	if (SLANG_FAILED(s_pxGlobalSession->createSession(xSessionDesc, pxSession.writeRef())) || !pxSession)
	{
		xResultOut.m_strError = "Failed to create Slang session";
		return false;
	}

	// Load the requested module. loadModule resolves transitive imports via
	// the session search paths.
	Slang::ComPtr<slang::IBlob> pxLoadDiagnostics;
	slang::IModule* pxModule = pxSession->loadModule(xDesc.m_szModuleName, pxLoadDiagnostics.writeRef());
	if (!pxModule)
	{
		xResultOut.m_strError = "Failed to load module '" + std::string(xDesc.m_szModuleName) + "'";
		if (pxLoadDiagnostics && pxLoadDiagnostics->getBufferSize() > 0)
		{
			xResultOut.m_strError += ": ";
			xResultOut.m_strError.append(static_cast<const char*>(pxLoadDiagnostics->getBufferPointer()),
										 pxLoadDiagnostics->getBufferSize());
		}
		return false;
	}

	// Find each requested entry point.
	struct EntryPointBinding
	{
		Slang::ComPtr<slang::IEntryPoint> m_pxEntryPoint;
		SlangStage  m_eStage;
		const char* m_szName;
	};
	std::vector<EntryPointBinding> axEntryPoints;
	auto fnAddEntry = [&](const char* szName, SlangStage eStage) -> bool
	{
		if (!szName || !szName[0]) return true;
		EntryPointBinding x;
		x.m_eStage = eStage;
		x.m_szName = szName;
		if (SLANG_FAILED(pxModule->findEntryPointByName(szName, x.m_pxEntryPoint.writeRef())) || !x.m_pxEntryPoint)
		{
			xResultOut.m_strError = "Entry point '" + std::string(szName) + "' not found in module '" +
									 std::string(xDesc.m_szModuleName) + "'";
			return false;
		}
		axEntryPoints.push_back(x);
		return true;
	};
	if (!fnAddEntry(xDesc.m_szVertexEntry,   SLANG_STAGE_VERTEX))   return false;
	if (!fnAddEntry(xDesc.m_szFragmentEntry, SLANG_STAGE_FRAGMENT)) return false;
	if (!fnAddEntry(xDesc.m_szComputeEntry,  SLANG_STAGE_COMPUTE))  return false;

	if (axEntryPoints.empty())
	{
		xResultOut.m_strError = "No entry points specified for program";
		return false;
	}

	// Compose: [module, entry0, entry1, ...] -> single IComponentType.
	std::vector<slang::IComponentType*> axComponents;
	axComponents.push_back(pxModule);
	for (const EntryPointBinding& x : axEntryPoints) axComponents.push_back(x.m_pxEntryPoint.get());

	Slang::ComPtr<slang::IComponentType> pxComposed;
	Slang::ComPtr<slang::IBlob> pxComposeDiagnostics;
	if (SLANG_FAILED(pxSession->createCompositeComponentType(axComponents.data(),
															  static_cast<SlangInt>(axComponents.size()),
															  pxComposed.writeRef(),
															  pxComposeDiagnostics.writeRef())) || !pxComposed)
	{
		xResultOut.m_strError = "createCompositeComponentType failed";
		if (pxComposeDiagnostics && pxComposeDiagnostics->getBufferSize() > 0)
		{
			xResultOut.m_strError += ": ";
			xResultOut.m_strError.append(static_cast<const char*>(pxComposeDiagnostics->getBufferPointer()),
										 pxComposeDiagnostics->getBufferSize());
		}
		return false;
	}

	// Link.
	Slang::ComPtr<slang::IComponentType> pxLinked;
	Slang::ComPtr<slang::IBlob> pxLinkDiagnostics;
	if (SLANG_FAILED(pxComposed->link(pxLinked.writeRef(), pxLinkDiagnostics.writeRef())) || !pxLinked)
	{
		xResultOut.m_strError = "link failed";
		if (pxLinkDiagnostics && pxLinkDiagnostics->getBufferSize() > 0)
		{
			xResultOut.m_strError += ": ";
			xResultOut.m_strError.append(static_cast<const char*>(pxLinkDiagnostics->getBufferPointer()),
										 pxLinkDiagnostics->getBufferSize());
		}
		return false;
	}

	// Emit SPIR-V per entry point.
	for (size_t i = 0; i < axEntryPoints.size(); i++)
	{
		Slang::ComPtr<slang::IBlob> pxCode;
		Slang::ComPtr<slang::IBlob> pxCodeDiag;
		if (SLANG_FAILED(pxLinked->getEntryPointCode(static_cast<SlangInt>(i), 0,
													  pxCode.writeRef(), pxCodeDiag.writeRef())) || !pxCode)
		{
			xResultOut.m_strError = "getEntryPointCode failed for '" + std::string(axEntryPoints[i].m_szName) + "'";
			if (pxCodeDiag && pxCodeDiag->getBufferSize() > 0)
			{
				xResultOut.m_strError += ": ";
				xResultOut.m_strError.append(static_cast<const char*>(pxCodeDiag->getBufferPointer()),
											 pxCodeDiag->getBufferSize());
			}
			return false;
		}

		const uint32_t* puCode = static_cast<const uint32_t*>(pxCode->getBufferPointer());
		const size_t ulWords   = pxCode->getBufferSize() / sizeof(uint32_t);

		Zenith_Vector<uint32_t>* pxOut = nullptr;
		switch (axEntryPoints[i].m_eStage)
		{
		case SLANG_STAGE_VERTEX:   pxOut = &xResultOut.m_axVertexSpirv;   break;
		case SLANG_STAGE_FRAGMENT: pxOut = &xResultOut.m_axFragmentSpirv; break;
		case SLANG_STAGE_COMPUTE:  pxOut = &xResultOut.m_axComputeSpirv;  break;
		default:
			xResultOut.m_strError = "Unsupported stage for SPIR-V emission";
			return false;
		}
		pxOut->Reserve(static_cast<u_int>(ulWords));
		for (size_t w = 0; w < ulWords; w++) pxOut->PushBack(puCode[w]);
	}

	// Reflection from the linked program.
	slang::ProgramLayout* pxReflection = pxLinked->getLayout(0);
	ExtractV2Reflection(pxReflection, xResultOut.m_xReflection);

	xResultOut.m_bSuccess = true;
	return true;
}
#endif // ZENITH_WINDOWS
