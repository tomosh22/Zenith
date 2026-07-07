#include "Zenith.h"
#include "Core/Zenith_CommandLine.h"   // --assets-root override for the shader source root
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_PersistentSetLayouts.h"   // Phase 5: persistent-set classification
#include "Flux/Flux_SpecConstants.h"          // Stage 3a: spec-constant table + resolver (backend-free)

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
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
	const u_int* puIndex = m_xBindingMap.TryGet(szName);
	if (puIndex == nullptr)
	{
		// Log all available bindings to help debug
		Zenith_Log(LOG_CATEGORY_RENDERER, "GetBinding('%s') failed. Available bindings (%u):",
			szName, m_xBindingMap.GetSize());
		for (Zenith_HashMap<std::string, u_int>::Iterator xIt(m_xBindingMap); !xIt.Done(); xIt.Next())
		{
			const Flux_ReflectedBinding& xBinding = m_axBindings.Get(xIt.GetValue());
			Zenith_Log(LOG_CATEGORY_RENDERER, "  '%s' -> set=%u, binding=%u",
				xIt.GetKey().c_str(), xBinding.m_uSet, xBinding.m_uBinding);
		}
		return nullptr;
	}
	return &m_axBindings.Get(*puIndex);
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

		Flux_BindingGroupLayout& xGroup = xLayoutOut.m_axBindingGroups[xBinding.m_uSet];
		Flux_BindingGroupEntry& xEntry = xGroup.m_axBindings[xBinding.m_uBinding];
		xEntry.m_eKind            = xBinding.m_eResourceKind;
		xEntry.m_uDescriptorCount = xBinding.m_uDescriptorCount;
		xEntry.m_uStageMask       = xBinding.m_uStageMask;
		xEntry.m_bPresent         = true;

		// Phase 5: tag the persistent spine sets (GLOBAL/VIEW/BINDLESS) by their
		// canonical binding-0 member name, so the Vulkan RootSig can assert layout
		// compatibility (5.0) and borrow the shared persistent layouts (5.1). Only the
		// canonical member sets the class — non-spine sets stay GENERIC.
		const FluxFrequencyClass eClass =
			Flux_PersistentSetLayouts::ClassifyMember(xBinding.m_uSet, xBinding.m_strName);
		if (eClass != FLUX_FREQUENCY_CLASS_GENERIC)
		{
			xGroup.m_eFrequencyClass = eClass;
		}
	}
	// No gap-fill: RootSigBuilder iterates m_bPresent (not stop-at-first-hole),
	// so sparse binding layouts are handled directly — the old placeholder
	// fill that the BINDING_TYPE_MAX stop-scan needed is gone.
}

void Flux_ShaderReflection::AddBinding(const Flux_ReflectedBinding& xBinding)
{
	m_axBindings.PushBack(xBinding);
}

void Flux_ShaderReflection::AddSpecConstant(const Flux_ReflectedSpecConstant& xSpec)
{
	m_axSpecConstants.PushBack(xSpec);
}

const Flux_ReflectedSpecConstant* Flux_ShaderReflection::GetSpecConstant(const char* szName) const
{
	if (!szName) return nullptr;
	for (u_int u = 0; u < m_axSpecConstants.GetSize(); u++)
	{
		if (m_axSpecConstants.Get(u).m_strName == szName)
		{
			return &m_axSpecConstants.Get(u);
		}
	}
	return nullptr;
}

// Resolve a name-keyed spec-constant table against a shader's reflection into
// backend-ready (id, size, value) entries (Flux Shader System Overhaul — Stage
// 3a). Pure + backend-free — lives in the ungated region so every backend (and
// the null D3D12 build) links it. Names absent from the reflection are skipped.
u_int Flux_ResolveSpecConstants(const Flux_SpecConstantTable& xTable,
								const Flux_ShaderReflection& xReflection,
								Flux_ResolvedSpecConstant* paxOut, u_int uMaxOut)
{
	if (!paxOut || uMaxOut == 0) return 0;
	u_int uWritten = 0;
	for (u_int u = 0; u < xTable.m_uCount && uWritten < uMaxOut; u++)
	{
		const Flux_SpecConstantEntry& xEntry = xTable.m_axEntries[u];
		const Flux_ReflectedSpecConstant* pxRefl = xReflection.GetSpecConstant(xEntry.m_szName);
		if (!pxRefl)
		{
			// The shader this pipeline is built from does not declare this constant —
			// harmless (Vulkan ignores a constant_id absent from the module). Skip.
			continue;
		}
		// Hot-reload drift tripwire: a name-keyed add carrying a codegen-baked id must
		// still match the reflected id. A mismatch means the generated header is stale
		// relative to the .spv.refl — rerun FluxCompiler.
		Zenith_Assert(xEntry.m_uBakedConstantId == UINT32_MAX || xEntry.m_uBakedConstantId == pxRefl->m_uConstantId,
			"Flux_ResolveSpecConstants: spec '%s' baked id %u != reflected id %u — rerun FluxCompiler.",
			xEntry.m_szName, xEntry.m_uBakedConstantId, pxRefl->m_uConstantId);
		Flux_ResolvedSpecConstant& xOut = paxOut[uWritten++];
		xOut.m_uConstantId = pxRefl->m_uConstantId;
		xOut.m_uSize       = pxRefl->m_uSize;
		xOut.m_uValue      = xEntry.m_uValue;
	}
	return uWritten;
}

void Flux_ShaderReflection::SetBindingStaticUse(u_int uIndex, bool bUsed)
{
	if (uIndex >= m_axBindings.GetSize()) return;
	m_axBindings.Get(uIndex).m_bStaticallyUsed = bUsed;
}

void Flux_ShaderReflection::BuildLookupMap()
{
	m_xBindingMap.Clear();
	for (u_int u = 0; u < m_axBindings.GetSize(); u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		m_xBindingMap[xBinding.m_strName] = u;
	}
}

// Magic and version for the .spv.refl sidecar format.
// v3: kind-only — (set, binding, name, size, FluxResourceKind, descriptor count,
//     stage mask, parameter-block path, reflected CB fields).
// v4: + per-binding static-use bit (Phase 5.5 — m_bStaticallyUsed, baked from
//     Slang IMetadata::isParameterLocationUsed). The reader accepts ONLY the
//     current version and fails loudly otherwise (every sidecar is regenerated
//     by FluxCompiler in lockstep — no legacy fallback).
// v5: + specialization-constant table (Flux Shader System Overhaul — Stage 3a —
//     name, constant id, size, u32 default, type name). Empty for every program
//     until Stage 3b adds the first [SpecializationConstant] decls, so the v5
//     bump changes only the sidecar bytes (a trailing count of 0), never the .spv.
static constexpr u_int32 kFluxReflectionMagic   = 0x46525846; // 'FXRF'
static constexpr u_int32 kFluxReflectionVersion = 5;

void Flux_ShaderReflection::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << kFluxReflectionMagic;
	xStream << kFluxReflectionVersion;
	const u_int uCount = m_axBindings.GetSize();
	xStream << uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ReflectedBinding& xBinding = m_axBindings.Get(u);
		xStream << xBinding.m_uSet;
		xStream << xBinding.m_uBinding;
		xStream << xBinding.m_strName;
		xStream << xBinding.m_uSize;
		xStream << static_cast<u_int>(xBinding.m_eResourceKind);
		xStream << xBinding.m_uDescriptorCount;
		xStream << xBinding.m_uStageMask;
		xStream << xBinding.m_strParameterBlockPath;
		xStream << xBinding.m_bStaticallyUsed;   // v4 (Phase 5.5)
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

	// v5: specialization-constant table.
	const u_int uSpecCount = m_axSpecConstants.GetSize();
	xStream << uSpecCount;
	for (u_int u = 0; u < uSpecCount; u++)
	{
		const Flux_ReflectedSpecConstant& xSpec = m_axSpecConstants.Get(u);
		xStream << xSpec.m_strName;
		xStream << xSpec.m_uConstantId;
		xStream << xSpec.m_uSize;
		xStream << xSpec.m_uDefaultValue;
		xStream << xSpec.m_strTypeName;
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
	Zenith_Assert(uVersion == kFluxReflectionVersion,
		"Flux_ShaderReflection: unsupported format v%u (expected v%u). Rerun FluxCompiler to regenerate .spv.refl sidecars.",
		uVersion, kFluxReflectionVersion);
	u_int uCount;
	xStream >> uCount;
	for (u_int u = 0; u < uCount; u++)
	{
		Flux_ReflectedBinding xBinding;
		xStream >> xBinding.m_uSet;
		xStream >> xBinding.m_uBinding;
		xStream >> xBinding.m_strName;
		xStream >> xBinding.m_uSize;
		u_int uKind;
		xStream >> uKind;
		xBinding.m_eResourceKind = static_cast<FluxResourceKind>(uKind);
		xStream >> xBinding.m_uDescriptorCount;
		xStream >> xBinding.m_uStageMask;
		xStream >> xBinding.m_strParameterBlockPath;
		xStream >> xBinding.m_bStaticallyUsed;   // v4 (Phase 5.5)
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
		m_axBindings.PushBack(xBinding);
	}

	// v5: specialization-constant table.
	u_int uSpecCount;
	xStream >> uSpecCount;
	for (u_int u = 0; u < uSpecCount; u++)
	{
		Flux_ReflectedSpecConstant xSpec;
		xStream >> xSpec.m_strName;
		xStream >> xSpec.m_uConstantId;
		xStream >> xSpec.m_uSize;
		xStream >> xSpec.m_uDefaultValue;
		xStream >> xSpec.m_strTypeName;
		m_axSpecConstants.PushBack(xSpec);
	}

	BuildLookupMap();
}

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
void Flux_SlangCompiler::Initialise()
{
	if (s_pxGlobalSession)
	{
		return;
	}

	// Slang-only: CompileProgram (session/module/link API) compiles .slang sources.
	// `enableGLSL` stays false (slang-glslang.dll is not deployed).
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
		eCategory == slang::ParameterCategory::FragmentOutput ||
		// Stage 3a: specialization constants occupy NO descriptor binding — they
		// ride the reflection's spec-constant table (ExtractSpecConstants), never
		// the root signature. Reject them here explicitly (auditable intent) rather
		// than relying on the implicit FLUX_RESOURCE_KIND_UNKNOWN drop below.
		eCategory == slang::ParameterCategory::SpecializationConstant)
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

// Walk a top-level ParameterBlock<T> parameter: emit one reflected binding per
// RESOURCE member of T, at the block's descriptor-set space (the SubElement-
// RegisterSpace offset) and the member's slot within that space (the
// DescriptorTableSlot offset). This is the Phase-2 frequency-block reflection
// path (GlobalParams/ViewParams/PassParams/etc.). For the current vk::binding
// shaders no top-level parameter is a ParameterBlock, so this is never entered —
// it is dormant scaffolding until the shaders are converted to ParameterBlocks.
// (Member-walk + offset mechanism proven by the Phase-0 reflection experiment.)
static void ExtractParameterBlockMembers(slang::VariableLayoutReflection* pxBlockParam,
										   Flux_ShaderReflection& xOut, u_int uStageBit)
{
	if (!pxBlockParam) return;
	slang::TypeLayoutReflection* pxTL = pxBlockParam->getTypeLayout();
	if (!pxTL) return;
	slang::TypeLayoutReflection* pxElem = pxTL->getElementTypeLayout();
	if (!pxElem || pxElem->getKind() != slang::TypeReflection::Kind::Struct) return;

	const std::string strBlockName = pxBlockParam->getName() ? pxBlockParam->getName() : "";
	const u_int uBlockSpace = static_cast<u_int>(
		pxBlockParam->getOffset(slang::ParameterCategory::SubElementRegisterSpace));

	const u_int uFieldCount = static_cast<u_int>(pxElem->getFieldCount());
	for (u_int f = 0; f < uFieldCount; f++)
	{
		slang::VariableLayoutReflection* pxMember = pxElem->getFieldByIndex(f);
		if (!pxMember) continue;
		slang::TypeLayoutReflection* pxMemberTL = pxMember->getTypeLayout();
		const FluxResourceKind eKind = ClassifyResource(pxMemberTL);
		// Ordinary uniform data (UNKNOWN kind) would belong to the block's
		// implicit constant buffer; the taxonomy convention uses explicit
		// ConstantBuffer<T> members instead, so skip non-resource fields.
		if (eKind == FLUX_RESOURCE_KIND_UNKNOWN) continue;

		Flux_ReflectedBinding xB;
		xB.m_eResourceKind = eKind;
		xB.m_strName       = pxMember->getName() ? pxMember->getName() : "";
		xB.m_uSet          = uBlockSpace;
		xB.m_uBinding      = static_cast<u_int>(pxMember->getOffset(slang::ParameterCategory::DescriptorTableSlot));
		xB.m_strParameterBlockPath = strBlockName;
		xB.m_uSize         = pxMemberTL ? static_cast<u_int>(pxMemberTL->getSize()) : 0;

		const slang::TypeReflection::Kind eMemberKind = pxMemberTL ? pxMemberTL->getKind() : slang::TypeReflection::Kind::None;
		if (eMemberKind == slang::TypeReflection::Kind::Array)
		{
			slang::TypeReflection* pxType = pxMemberTL->getType();
			xB.m_uDescriptorCount = pxType ? static_cast<u_int>(pxType->getElementCount()) : 1; // 0 => unbounded
		}
		else
		{
			xB.m_uDescriptorCount = 1;
		}

		if (eKind == FLUX_RESOURCE_KIND_CONSTANT_BUFFER || eKind == FLUX_RESOURCE_KIND_PARAMETER_BLOCK)
		{
			slang::TypeLayoutReflection* pxInner = pxMemberTL->getElementTypeLayout();
			if (pxInner)
			{
				xB.m_uSize = static_cast<u_int>(pxInner->getSize());
				ExtractFieldsFromStruct(pxInner, xB.m_axFields);
			}
		}
		MergeBindingWithStageMask(xOut, xB, uStageBit);
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
		slang::VariableLayoutReflection* pxParam = pxLayout->getParameterByIndex(u);
		slang::TypeLayoutReflection* pxParamTL = pxParam ? pxParam->getTypeLayout() : nullptr;
		// Frequency ParameterBlock<T>: expand to one binding per member resource
		// (Phase-2 path). Dormant for vk::binding shaders (no block params).
		if (pxParamTL && pxParamTL->getKind() == slang::TypeReflection::Kind::ParameterBlock)
		{
			ExtractParameterBlockMembers(pxParam, xReflectionOut, uGlobalStageMask);
			continue;
		}
		Flux_ReflectedBinding xBinding;
		if (BuildV2BindingFromParam(pxParam, "", xBinding))
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

// Extract specialization constants from the linked program layout into the
// reflection's spec table (Flux Shader System Overhaul — Stage 3a). Spec
// constants appear in the top-level parameter walk (Stage-0 probe E4) but are NOT
// descriptor bindings — ExtractV2Reflection / BuildV2BindingFromParam drop them.
// Int-family only (bool/int): Slang exposes getDefaultValueInt for these; the
// default is packed as u32 (bool → 0/1) and the size is always 4. Runs AFTER
// ExtractV2Reflection so a program's reflection carries both tables.
static void ExtractSpecConstants(slang::ProgramLayout* pxLayout,
								 Flux_ShaderReflection& xReflectionOut)
{
	if (!pxLayout) return;
	const u_int uParamCount = static_cast<u_int>(pxLayout->getParameterCount());
	for (u_int p = 0; p < uParamCount; p++)
	{
		slang::VariableLayoutReflection* pxVar = pxLayout->getParameterByIndex(p);
		if (!pxVar) continue;

		bool bIsSpec = false;
		const u_int uCatCount = static_cast<u_int>(pxVar->getCategoryCount());
		for (u_int c = 0; c < uCatCount; c++)
		{
			if (pxVar->getCategoryByIndex(c) == slang::ParameterCategory::SpecializationConstant)
			{
				bIsSpec = true;
				break;
			}
		}
		if (!bIsSpec) continue;

		Flux_ReflectedSpecConstant xSpec;
		xSpec.m_strName     = pxVar->getName() ? pxVar->getName() : "";
		xSpec.m_uConstantId = static_cast<u_int>(pxVar->getOffset(SLANG_PARAMETER_CATEGORY_SPECIALIZATION_CONSTANT));
		xSpec.m_uSize       = 4u;   // int-family spec constants are 32-bit
		if (slang::TypeLayoutReflection* pxTL = pxVar->getTypeLayout())
		{
			if (slang::TypeReflection* pxType = pxTL->getType())
			{
				xSpec.m_strTypeName = pxType->getName() ? pxType->getName() : "";
			}
		}
		if (slang::VariableReflection* pxV = pxVar->getVariable())
		{
			int64_t iVal = 0;
			if (pxV->hasDefaultValue() && SLANG_SUCCEEDED(pxV->getDefaultValueInt(&iVal)))
			{
				xSpec.m_uDefaultValue = static_cast<u_int>(static_cast<u_int64>(iVal));
			}
		}
		xReflectionOut.AddSpecConstant(xSpec);
	}
}

// One found entry point in a Slang module: the live IEntryPoint, the stage
// it targets, and the source name used to resolve it.
struct Flux_SlangEntryPointBinding
{
	Slang::ComPtr<slang::IEntryPoint> m_pxEntryPoint;
	SlangStage  m_eStage;
	const char* m_szName;
};

// Build a Slang session for the requested program. Owns the search-path
// vector internally so callers don't get dangling pointers — the session is
// fully formed by the time this returns. Sets strError and returns false on
// failure.
static bool CreateSlangSession(const Flux_SlangProgramDesc& xDesc,
								Slang::ComPtr<slang::ISession>& pxSessionOut,
								std::string& strError)
{
	// Build target descriptor — SPIR-V, requested profile.
	slang::TargetDesc xTarget = {};
	xTarget.format  = SLANG_SPIRV;
	xTarget.profile = s_pxGlobalSession->findProfile(xDesc.m_szTargetProfile ? xDesc.m_szTargetProfile : "spirv_1_3");

	// Optional per-compile compiler options (Flux Shader System Overhaul — Stage 1).
	// Debug info (RenderDoc) is requested ONLY by the runtime-compile path in Debug
	// builds; the offline FluxCompiler leaves both flags false, so the checked-in
	// artifacts stay byte-identical. The entries vector must outlive createSession
	// (below) — it does, being function-scoped.
	std::vector<slang::CompilerOptionEntry> axOptions;
	if (xDesc.m_bEmitDebugInfo)
	{
		slang::CompilerOptionEntry xEntry = {};
		xEntry.name            = slang::CompilerOptionName::DebugInformation;
		xEntry.value.kind      = slang::CompilerOptionValueKind::Int;
		xEntry.value.intValue0 = SLANG_DEBUG_INFO_LEVEL_MAXIMAL;
		axOptions.push_back(xEntry);
	}
	if (xDesc.m_bDisableOptimization)
	{
		slang::CompilerOptionEntry xEntry = {};
		xEntry.name            = slang::CompilerOptionName::Optimization;
		xEntry.value.kind      = slang::CompilerOptionValueKind::Int;
		xEntry.value.intValue0 = SLANG_OPTIMIZATION_LEVEL_NONE;
		axOptions.push_back(xEntry);
	}
	if (!axOptions.empty())
	{
		xTarget.compilerOptionEntries    = axOptions.data();
		xTarget.compilerOptionEntryCount = static_cast<uint32_t>(axOptions.size());
	}

	// Build session descriptor with current search paths.
	std::vector<const char*> aszPaths;
	aszPaths.reserve(s_axSearchPaths.size() + 1);
	for (const std::string& s : s_axSearchPaths) aszPaths.push_back(s.c_str());
#ifdef SHADER_SOURCE_ROOT
	// Baked absolute build-machine path, overridable by --assets-root for
	// relocatable packages (function-local static: resolved once, on the first
	// session build -- after the command line is parsed; FluxCompiler never
	// parses game args so it keeps the baked root).
	static const std::string ls_strShaderRoot = Zenith_CommandLine::ResolveUnderAssetsRoot(
		SHADER_SOURCE_ROOT, Zenith_CommandLine::GetAssetsRoot(), "Zenith/Flux/Shaders/");
	aszPaths.push_back(ls_strShaderRoot.c_str());
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

	if (SLANG_FAILED(s_pxGlobalSession->createSession(xSessionDesc, pxSessionOut.writeRef())) || !pxSessionOut)
	{
		strError = "Failed to create Slang session";
		return false;
	}
	return true;
}

// Find each configured entry point in pxModule and append to axOut. Returns
// false on the first missing entry, leaving strError populated.
static bool CollectEntryPoints(slang::IModule* pxModule,
								const Flux_SlangProgramDesc& xDesc,
								std::vector<Flux_SlangEntryPointBinding>& axOut,
								std::string& strError)
{
	auto fnAddEntry = [&](const char* szName, SlangStage eStage) -> bool
	{
		if (!szName || !szName[0]) return true;
		Flux_SlangEntryPointBinding x;
		x.m_eStage = eStage;
		x.m_szName = szName;
		if (SLANG_FAILED(pxModule->findEntryPointByName(szName, x.m_pxEntryPoint.writeRef())) || !x.m_pxEntryPoint)
		{
			strError = "Entry point '" + std::string(szName) + "' not found in module '" +
						 std::string(xDesc.m_szModuleName) + "'";
			return false;
		}
		axOut.push_back(x);
		return true;
	};
	if (!fnAddEntry(xDesc.m_szVertexEntry,   SLANG_STAGE_VERTEX))   return false;
	if (!fnAddEntry(xDesc.m_szFragmentEntry, SLANG_STAGE_FRAGMENT)) return false;
	if (!fnAddEntry(xDesc.m_szComputeEntry,  SLANG_STAGE_COMPUTE))  return false;
	return true;
}

// Compose [module, entry0, entry1, ...] into a single IComponentType.
static bool ComposeComponentType(slang::ISession* pxSession,
								  slang::IModule* pxModule,
								  const std::vector<Flux_SlangEntryPointBinding>& axEntryPoints,
								  Slang::ComPtr<slang::IComponentType>& pxComposedOut,
								  std::string& strError)
{
	std::vector<slang::IComponentType*> axComponents;
	axComponents.push_back(pxModule);
	for (const Flux_SlangEntryPointBinding& x : axEntryPoints) axComponents.push_back(x.m_pxEntryPoint.get());

	Slang::ComPtr<slang::IBlob> pxComposeDiagnostics;
	if (SLANG_FAILED(pxSession->createCompositeComponentType(axComponents.data(),
															  static_cast<SlangInt>(axComponents.size()),
															  pxComposedOut.writeRef(),
															  pxComposeDiagnostics.writeRef())) || !pxComposedOut)
	{
		strError = "createCompositeComponentType failed";
		if (pxComposeDiagnostics && pxComposeDiagnostics->getBufferSize() > 0)
		{
			strError += ": ";
			strError.append(static_cast<const char*>(pxComposeDiagnostics->getBufferPointer()),
							pxComposeDiagnostics->getBufferSize());
		}
		return false;
	}
	return true;
}

// Link the composed IComponentType. Returns the linked component out or sets strError.
static bool LinkComponentType(slang::IComponentType* pxComposed,
							   Slang::ComPtr<slang::IComponentType>& pxLinkedOut,
							   std::string& strError)
{
	Slang::ComPtr<slang::IBlob> pxLinkDiagnostics;
	if (SLANG_FAILED(pxComposed->link(pxLinkedOut.writeRef(), pxLinkDiagnostics.writeRef())) || !pxLinkedOut)
	{
		strError = "link failed";
		if (pxLinkDiagnostics && pxLinkDiagnostics->getBufferSize() > 0)
		{
			strError += ": ";
			strError.append(static_cast<const char*>(pxLinkDiagnostics->getBufferPointer()),
							pxLinkDiagnostics->getBufferSize());
		}
		return false;
	}
	return true;
}

// Load the named module via the session and surface diagnostics on failure.
static bool LoadSlangModule(slang::ISession* pxSession,
							 const char* szModuleName,
							 slang::IModule*& pxModuleOut,
							 std::string& strError)
{
	Slang::ComPtr<slang::IBlob> pxLoadDiagnostics;
	pxModuleOut = pxSession->loadModule(szModuleName, pxLoadDiagnostics.writeRef());
	if (!pxModuleOut)
	{
		strError = "Failed to load module '" + std::string(szModuleName) + "'";
		if (pxLoadDiagnostics && pxLoadDiagnostics->getBufferSize() > 0)
		{
			strError += ": ";
			strError.append(static_cast<const char*>(pxLoadDiagnostics->getBufferPointer()),
							 pxLoadDiagnostics->getBufferSize());
		}
		return false;
	}
	return true;
}

// Emit SPIR-V for every entry point on the linked program, routing into the
// per-stage output vectors on xResultOut. Returns false on first failure with
// strError populated.
static bool EmitEntryPointSpirv(slang::IComponentType* pxLinked,
								 const std::vector<Flux_SlangEntryPointBinding>& axEntryPoints,
								 Flux_SlangProgramResult& xResultOut)
{
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
	return true;
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

	Slang::ComPtr<slang::ISession> pxSession;
	if (!CreateSlangSession(xDesc, pxSession, xResultOut.m_strError)) return false;

	slang::IModule* pxModule = nullptr;
	if (!LoadSlangModule(pxSession, xDesc.m_szModuleName, pxModule, xResultOut.m_strError)) return false;

	std::vector<Flux_SlangEntryPointBinding> axEntryPoints;
	if (!CollectEntryPoints(pxModule, xDesc, axEntryPoints, xResultOut.m_strError)) return false;

	if (axEntryPoints.empty())
	{
		xResultOut.m_strError = "No entry points specified for program";
		return false;
	}

	Slang::ComPtr<slang::IComponentType> pxComposed;
	if (!ComposeComponentType(pxSession, pxModule, axEntryPoints, pxComposed, xResultOut.m_strError)) return false;

	Slang::ComPtr<slang::IComponentType> pxLinked;
	if (!LinkComponentType(pxComposed, pxLinked, xResultOut.m_strError)) return false;

	if (!EmitEntryPointSpirv(pxLinked, axEntryPoints, xResultOut)) return false;

	// Reflection from the linked program.
	slang::ProgramLayout* pxReflection = pxLinked->getLayout(0);
	ExtractV2Reflection(pxReflection, xResultOut.m_xReflection);

	// Phase 5.5: bake per-binding STATIC-USE. The base reflection lists every
	// #include'd spine member (g_xGlobal/g_xView/g_axTextures/...) in EVERY
	// program even when unsampled; Slang's IMetadata side-channel reports which
	// the program actually uses (the documented escape hatch — see Slang user-
	// guide "Determining Whether Parameters Are Used"). OR across entry points →
	// program-level use. Drives the VIEW/GLOBAL graph-Read() validator. If Slang
	// cannot answer for a binding, the bit stays its default (true = used), so we
	// over-approximate and never under-demand a graph Read().
	{
		const u_int uEPCount = static_cast<u_int>(pxReflection->getEntryPointCount());
		std::vector<Slang::ComPtr<slang::IMetadata>> axMeta(uEPCount);
		// Fail-safe coverage gate: we may only trust a 'false' (unused) verdict if
		// EVERY entry point's metadata resolved — a single missing/failed query on
		// the entry point that actually samples a member would otherwise let a
		// non-using entry point's 'false' clobber the over-approximating default to
		// a wrong 'false' (→ the validator would skip a member it should demand a
		// Read() for). Capture getEntryPointMetadata's result (it was discarded).
		bool bAllMetaOK = (uEPCount > 0);
		for (u_int ep = 0; ep < uEPCount; ep++)
		{
			if (SLANG_FAILED(pxLinked->getEntryPointMetadata(static_cast<SlangInt>(ep), 0, axMeta[ep].writeRef(), nullptr)) || !axMeta[ep])
			{
				bAllMetaOK = false;
			}
		}
		const Zenith_Vector<Flux_ReflectedBinding>& xBindings = xResultOut.m_xReflection.GetBindings();
		for (u_int i = 0; i < xBindings.GetSize(); i++)
		{
			const Flux_ReflectedBinding& xB = xBindings.Get(i);
			bool bAllQueriesOK = bAllMetaOK;   // every EP must answer before we trust 'false'
			bool bUsed         = false;
			for (u_int ep = 0; ep < uEPCount && bAllQueriesOK; ep++)
			{
				bool bUsedThisEP = false;
				if (SLANG_SUCCEEDED(axMeta[ep]->isParameterLocationUsed(
						static_cast<SlangParameterCategory>(slang::ParameterCategory::DescriptorTableSlot),
						static_cast<SlangUInt>(xB.m_uSet), static_cast<SlangUInt>(xB.m_uBinding), bUsedThisEP)))
				{
					bUsed = bUsed || bUsedThisEP;
				}
				else
				{
					bAllQueriesOK = false;   // any uncertainty → keep the default (true)
				}
			}
			// Only overwrite the default when every entry point was queried; on any
			// failure leave it true (over-approximate, never under-demand a Read()).
			if (bAllQueriesOK) xResultOut.m_xReflection.SetBindingStaticUse(i, bUsed);
		}
	}

	// Stage 3a: capture specialization constants into the reflection's spec table
	// (serialized in the v5 sidecar; drives per-mode pipeline variants). Empty for
	// every program until Stage 3b adds the first [SpecializationConstant] decls.
	ExtractSpecConstants(pxReflection, xResultOut.m_xReflection);

	xResultOut.m_bSuccess = true;
	return true;
}

// ---------------------------------------------------------------------------
// Stage-0 capability probe (see Flux_SlangProbes.Tests.inl). Deliberately mirrors
// CompileProgram's session/compose/link/emit path but takes an in-memory source
// string (loadModuleFromSourceString) so a test can compile a crafted snippet and
// inspect accept/reject + reflection + SPIR-V without touching the shader tree.
// ---------------------------------------------------------------------------
bool Flux_SlangCompiler::CompileProbeFromSource(const char* szSource, const char* szComputeEntry, Flux_SlangProbeResult& xOut,
												bool bEmitDebugInfo)
{
	xOut = Flux_SlangProbeResult{};

	if (!s_pxGlobalSession)
	{
		// Unit tests run before Flux initialises Slang — bring the global session up lazily.
		// Initialise() is idempotent, so a later engine init is a no-op on the same session.
		Initialise();
	}
	if (!s_pxGlobalSession)
	{
		xOut.m_strDiagnostics = "Slang global session unavailable";
		return false;
	}
	if (!szSource)
	{
		xOut.m_strDiagnostics = "null probe source";
		return false;
	}

	// Session config identical to the engine compile path (search paths + column-major).
	Flux_SlangProgramDesc xDesc = {};
	xDesc.m_bEmitDebugInfo = bEmitDebugInfo;
	Slang::ComPtr<slang::ISession> pxSession;
	std::string strSessionErr;
	if (!CreateSlangSession(xDesc, pxSession, strSessionErr))
	{
		xOut.m_strDiagnostics = strSessionErr;
		return false;
	}

	// Front-end: load the module from the in-memory source string.
	Slang::ComPtr<slang::IBlob> pxLoadDiag;
	slang::IModule* pxModule = pxSession->loadModuleFromSourceString(
		"flux_probe", "flux_probe.slang", szSource, pxLoadDiag.writeRef());
	if (pxLoadDiag && pxLoadDiag->getBufferSize() > 0)
	{
		xOut.m_strDiagnostics.append(static_cast<const char*>(pxLoadDiag->getBufferPointer()),
									  pxLoadDiag->getBufferSize());
	}
	if (!pxModule)
	{
		xOut.m_bCompiled = false;   // compile REJECTED (e.g. a private-access violation) — a valid probe outcome
		return false;
	}

	// No entry point requested → the front-end accept is the whole answer.
	if (!szComputeEntry || !szComputeEntry[0])
	{
		xOut.m_bCompiled = true;
		return true;
	}

	// Back-end: compose + link the compute entry, then emit its SPIR-V + reflection.
	Slang::ComPtr<slang::IEntryPoint> pxEntry;
	if (SLANG_FAILED(pxModule->findEntryPointByName(szComputeEntry, pxEntry.writeRef())) || !pxEntry)
	{
		xOut.m_strDiagnostics += "\nentry point '" + std::string(szComputeEntry) + "' not found";
		xOut.m_bCompiled = false;
		return false;
	}

	slang::IComponentType* apxComponents[2] = { pxModule, pxEntry.get() };
	Slang::ComPtr<slang::IComponentType> pxComposed;
	Slang::ComPtr<slang::IBlob> pxComposeDiag;
	if (SLANG_FAILED(pxSession->createCompositeComponentType(apxComponents, 2, pxComposed.writeRef(), pxComposeDiag.writeRef())) || !pxComposed)
	{
		if (pxComposeDiag && pxComposeDiag->getBufferSize() > 0)
			xOut.m_strDiagnostics.append(static_cast<const char*>(pxComposeDiag->getBufferPointer()), pxComposeDiag->getBufferSize());
		xOut.m_bCompiled = false;
		return false;
	}

	Slang::ComPtr<slang::IComponentType> pxLinked;
	Slang::ComPtr<slang::IBlob> pxLinkDiag;
	if (SLANG_FAILED(pxComposed->link(pxLinked.writeRef(), pxLinkDiag.writeRef())) || !pxLinked)
	{
		if (pxLinkDiag && pxLinkDiag->getBufferSize() > 0)
			xOut.m_strDiagnostics.append(static_cast<const char*>(pxLinkDiag->getBufferPointer()), pxLinkDiag->getBufferSize());
		xOut.m_bCompiled = false;
		return false;
	}

	Slang::ComPtr<slang::IBlob> pxCode;
	Slang::ComPtr<slang::IBlob> pxCodeDiag;
	if (SLANG_FAILED(pxLinked->getEntryPointCode(0, 0, pxCode.writeRef(), pxCodeDiag.writeRef())) || !pxCode)
	{
		if (pxCodeDiag && pxCodeDiag->getBufferSize() > 0)
			xOut.m_strDiagnostics.append(static_cast<const char*>(pxCodeDiag->getBufferPointer()), pxCodeDiag->getBufferSize());
		xOut.m_bCompiled = false;
		return false;
	}
	{
		const uint32_t* puCode = static_cast<const uint32_t*>(pxCode->getBufferPointer());
		const size_t ulWords   = pxCode->getBufferSize() / sizeof(uint32_t);
		xOut.m_axSpirv.Reserve(static_cast<u_int>(ulWords));
		for (size_t w = 0; w < ulWords; w++) xOut.m_axSpirv.PushBack(puCode[w]);
	}

	// Reflection via the engine's own extractor (parity with real compiles — E2 checks this is
	// byte-identical under visibility changes; note it DROPS spec constants from the binding table,
	// which E4 relies on). Stage 3a: ExtractSpecConstants then captures them into the reflection's
	// dedicated spec table (parity with CompileProgram) so unit tests can exercise the real path.
	slang::ProgramLayout* pxReflection = pxLinked->getLayout(0);
	ExtractV2Reflection(pxReflection, xOut.m_xReflection);
	ExtractSpecConstants(pxReflection, xOut.m_xReflection);
	xOut.m_bHasReflection = true;

	// Raw spec-constant capture (E4): walk the program-layout parameters for the
	// SpecializationConstant category — the extraction path D5 will formalise.
	if (pxReflection)
	{
		const u_int uParamCount = static_cast<u_int>(pxReflection->getParameterCount());
		for (u_int p = 0; p < uParamCount; p++)
		{
			slang::VariableLayoutReflection* pxVar = pxReflection->getParameterByIndex(p);
			if (!pxVar) continue;

			bool bIsSpec = false;
			const u_int uCatCount = static_cast<u_int>(pxVar->getCategoryCount());
			for (u_int c = 0; c < uCatCount; c++)
			{
				if (pxVar->getCategoryByIndex(c) == slang::ParameterCategory::SpecializationConstant)
				{
					bIsSpec = true;
					break;
				}
			}
			if (!bIsSpec) continue;

			Flux_SlangProbeResult::SpecConstant xSC;
			xSC.m_strName = pxVar->getName() ? pxVar->getName() : "";
			xSC.m_uId = static_cast<u_int>(pxVar->getOffset(SLANG_PARAMETER_CATEGORY_SPECIALIZATION_CONSTANT));
			if (slang::VariableReflection* pxV = pxVar->getVariable())
			{
				if (pxV->hasDefaultValue())
				{
					int64_t iVal = 0;
					if (SLANG_SUCCEEDED(pxV->getDefaultValueInt(&iVal)))
					{
						xSC.m_iDefault    = iVal;
						xSC.m_bHasDefault = true;
					}
				}
			}
			xOut.m_axSpecConstants.PushBack(xSC);
		}
	}

	xOut.m_bCompiled = true;
	return true;
}
#endif // ZENITH_WINDOWS
