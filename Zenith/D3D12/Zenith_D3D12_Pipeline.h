#pragma once
#include "Zenith.h"            // u_int / u_int64 / Zenith_Assert (defined before Flux.h in the PCH)
#include "Flux/Flux_Types.h"   // handles, enums, view structs, Flux_BindingSlot/SubresourceRange
#include "Flux/Flux_Fwd.h"     // the Flux_* aliases + forward decls of the other D3D12 classes

// ============================================================================
// NO-OP "null" D3D12 render backend — Pipeline family.
//
// Mirrors the neutral public surface of Zenith_Vulkan_Pipeline.h. Six classes:
//   Zenith_D3D12_Shader                  (concept FluxBackendShader)
//   Zenith_D3D12_RootSig                 (alias-only, no concept)
//   Zenith_D3D12_Pipeline                (alias-only, no concept)
//   Zenith_D3D12_RootSigBuilder          (concept FluxBackendRootSigBuilder)
//   Zenith_D3D12_PipelineBuilder         (concept FluxBackendPipelineBuilder)
//   Zenith_D3D12_ComputePipelineBuilder  (concept FluxBackendComputePipelineBuilder)
//
// This backend does ZERO real rendering. Every method is an inline no-op stub.
// Its only job is to COMPILE + LINK against the backend-neutral Flux surface,
// proving the Flux concepts are backend-agnostic.
//
// The single exception to "pure no-op" is Zenith_D3D12_Shader::Initialise,
// which mirrors the reflection-load portion of
// Zenith_Vulkan_Shader::InitialiseFromProgramArtifacts so that m_xReflection
// is populated from the checked-in <program>.spv.refl artifacts (the neutral
// binder asserts on a missing reflected name). No SPIR-V, no Slang, no GPU
// module creation — only the Slang-FREE Flux_ShaderReflection::ReadFromDataStream
// path is mirrored.
// ============================================================================

#include "Flux/Slang/Flux_SlangCompiler.h"            // Flux_ShaderReflection
#include "Flux/Slang/Flux_ShaderCatalog.h"           // Flux_ShaderCatalog artifact stems
#include "Flux/Slang/Flux_ShaderDecl.h"              // const Flux_ShaderDecl& Initialise handle
#include "DataStream/Zenith_DataStream.h"             // Zenith_DataStream (reflection load)

//==========================================================================
// Zenith_D3D12_Shader  (satisfies FluxBackendShader)
//==========================================================================
class Zenith_D3D12_Shader
{
public:
	Zenith_D3D12_Shader() = default;
	~Zenith_D3D12_Shader() { }

	// Concept-pinned signature: -> std::same_as<void>. Mirrors ONLY the
	// reflection-load part of Zenith_Vulkan_Shader::InitialiseFromProgramArtifacts:
	// reads each stage's <stem>.spv.refl into a Zenith_DataStream and merges via
	// Flux_ShaderReflection::ReadFromDataStream so m_xReflection is populated.
	// No SPIR-V / Slang / GPU module work is performed.
	void Initialise(const Flux_ShaderDecl& xDecl)
	{
		std::string strRoot(SHADER_SOURCE_ROOT);

		// Graphics-program path: merge vertex + fragment reflection.
		if (xDecl.m_szVertexEntry && xDecl.m_szFragmentEntry)
		{
			std::string strVStem = Flux_ShaderCatalog::GetVertexArtifactStem(xDecl);
			std::string strFStem = Flux_ShaderCatalog::GetFragmentArtifactStem(xDecl);

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
			return;
		}

		// Compute-program path.
		if (xDecl.m_szComputeEntry)
		{
			std::string strCStem = Flux_ShaderCatalog::GetComputeArtifactStem(xDecl);
			Zenith_DataStream xRefl;
			xRefl.ReadFromFile((strRoot + strCStem + ".spv.refl").c_str());
			if (xRefl.IsValid())
			{
				m_xReflection.ReadFromDataStream(xRefl);
			}
			return;
		}
	}

	// Reset GPU state — no-op for the null backend, but restores the
	// default-constructed reflection so a later Initialise is leak-free.
	void Reset() { m_xReflection = Flux_ShaderReflection(); }

	// Concept-pinned: -> std::same_as<const Flux_ShaderReflection&>.
	const Flux_ShaderReflection& GetReflection() const { return m_xReflection; }

	bool HasReflection() const { return m_xReflection.GetBindings().GetSize() > 0; }

private:
	// Merge reflection from one stage into the combined set (mirrors
	// Zenith_Vulkan_Shader::MergeReflection's neutral behaviour: append unique
	// bindings, then rebuild the lookup map).
	void MergeReflection(const Flux_ShaderReflection& xStageReflection)
	{
		const Zenith_Vector<Flux_ReflectedBinding>& axNewBindings = xStageReflection.GetBindings();
		for (u_int uNew = 0; uNew < axNewBindings.GetSize(); uNew++)
		{
			const Flux_ReflectedBinding& xNewBinding = axNewBindings.Get(uNew);

			bool bAlreadyPresent = false;
			const Zenith_Vector<Flux_ReflectedBinding>& axExistingBindings = m_xReflection.GetBindings();
			for (u_int uExisting = 0; uExisting < axExistingBindings.GetSize(); uExisting++)
			{
				const Flux_ReflectedBinding& xExisting = axExistingBindings.Get(uExisting);
				if (xExisting.m_uSet == xNewBinding.m_uSet && xExisting.m_uBinding == xNewBinding.m_uBinding)
				{
					bAlreadyPresent = true;
					break;
				}
			}

			if (!bAlreadyPresent)
			{
				m_xReflection.AddBinding(xNewBinding);
			}
		}

		// Stage 3a: merge specialization constants (dedup by constant id), mirroring
		// Zenith_Vulkan_Shader::MergeReflection so the null backend's reflection model
		// stays identical to the real one (spec constants are stored, never consumed —
		// FromSpecification is a no-op here).
		const Zenith_Vector<Flux_ReflectedSpecConstant>& axNewSpecs = xStageReflection.GetSpecConstants();
		for (u_int uNew = 0; uNew < axNewSpecs.GetSize(); uNew++)
		{
			const Flux_ReflectedSpecConstant& xNewSpec = axNewSpecs.Get(uNew);
			bool bSpecPresent = false;
			const Zenith_Vector<Flux_ReflectedSpecConstant>& axExistingSpecs = m_xReflection.GetSpecConstants();
			for (u_int uExisting = 0; uExisting < axExistingSpecs.GetSize(); uExisting++)
			{
				if (axExistingSpecs.Get(uExisting).m_uConstantId == xNewSpec.m_uConstantId)
				{
					bSpecPresent = true;
					break;
				}
			}
			if (!bSpecPresent)
			{
				m_xReflection.AddSpecConstant(xNewSpec);
			}
		}

		m_xReflection.BuildLookupMap();
	}

	// Combined reflection data from all shader stages.
	Flux_ShaderReflection m_xReflection;
};

//==========================================================================
// Zenith_D3D12_RootSig  (alias-only — no concept; builders populate it)
//==========================================================================
class Zenith_D3D12_RootSig
{
public:
	Zenith_D3D12_RootSig()
	{
		// Initialise all descriptor kinds to UNKNOWN (invalid/unused).
		for (u_int i = 0; i < FLUX_MAX_BINDING_GROUPS; i++)
		{
			for (u_int j = 0; j < FLUX_MAX_BINDINGS_PER_GROUP; j++)
			{
				m_aeBindingKinds[i][j] = FLUX_RESOURCE_KIND_UNKNOWN;
			}
		}
	}

	// Get binding location by name (for named resource binding). Mirrors the
	// neutral Vulkan implementation using the reflection lookup.
	Flux_BindingHandle GetBinding(const char* szName) const
	{
		const Flux_ReflectedBinding* pxBinding = m_xReflection.GetBinding(szName);
		Zenith_Assert(pxBinding != nullptr, "Shader binding '%s' not found in reflection", szName);
		Flux_BindingHandle xHandle;
		if (pxBinding)
		{
			xHandle.m_uSet     = pxBinding->m_uSet;
			xHandle.m_uBinding = pxBinding->m_uBinding;
		}
		return xHandle;
	}

	bool HasReflection() const { return m_xReflection.GetBindings().GetSize() > 0; }

	// Neutral state mirrored from the Vulkan root sig (the vk::DescriptorSetLayout
	// member is backend-internal and omitted). The Vulkan root sig stores a
	// vk::PipelineLayout in m_xLayout that the engine's compute-pipeline setup
	// passes to ComputePipelineBuilder::WithLayout; the null backend keeps a dummy
	// handle of a neutral type so those shared call sites compile.
	FluxResourceKind m_aeBindingKinds[FLUX_MAX_BINDING_GROUPS][FLUX_MAX_BINDINGS_PER_GROUP];
	u_int m_uNumBindingGroups = UINT32_MAX;
	u_int m_xLayout = 0;

	// Reflection data for name-based binding lookups.
	Flux_ShaderReflection m_xReflection;
};

//==========================================================================
// Zenith_D3D12_Pipeline  (alias-only — no concept; builders produce it)
//==========================================================================
class Zenith_D3D12_Pipeline
{
public:
	Zenith_D3D12_Pipeline() = default;
	~Zenith_D3D12_Pipeline() { }

	// Reset GPU state — no-op for the null backend.
	void Reset() { }

	// Neutral root-signature slot (the vk::Pipeline / vk::RenderPass members
	// are backend-internal and omitted). The static TargetSetupToRenderPass /
	// TargetSetupToFramebuffer helpers in the Vulkan header return vk:: types,
	// so they are backend-internal and intentionally SKIPPED here.
	Zenith_D3D12_RootSig m_xRootSig;
};

//==========================================================================
// Zenith_D3D12_RootSigBuilder  (satisfies FluxBackendRootSigBuilder)
//==========================================================================
class Zenith_D3D12_RootSigBuilder
{
public:
	// Build from manual specification.
	static void FromSpecification(Zenith_D3D12_RootSig& /*xRootSigOut*/, const Flux_PipelineLayout& /*xSpec*/) { }

	// Build from shader reflection data (concept entry point). Copy the
	// reflection through so name-based lookups still resolve on the null backend.
	static void FromReflection(Zenith_D3D12_RootSig& xRootSigOut, const Flux_ShaderReflection& xReflection)
	{
		xRootSigOut.m_xReflection = xReflection;
	}
};

//==========================================================================
// Zenith_D3D12_PipelineBuilder  (satisfies FluxBackendPipelineBuilder)
//
// The Vulkan WithX(...) fluent setters all take vk:: parameter types
// (vk::CompareOp, vk::BlendFactor, vk::CullModeFlagBits, vk::PrimitiveTopology,
// vk::PipelineLayout, vk::ShaderStageFlags, vk::DescriptorSetLayout,
// vk::RenderPass, vk::Format, vk::PipelineVertexInputStateCreateInfo) and so are
// backend-internal — they are SKIPPED per the rules. WithShader takes a
// const Zenith_Vulkan_Shader& (neutral) so it is mirrored. FromSpecification is
// the neutral concept entry point.
//==========================================================================
class Zenith_D3D12_PipelineBuilder
{
public:
	Zenith_D3D12_PipelineBuilder() { }
	~Zenith_D3D12_PipelineBuilder() { }

	// Neutral fluent setter (param type is the backend Shader alias).
	Zenith_D3D12_PipelineBuilder& WithShader(const Zenith_D3D12_Shader& /*xShader*/) { return *this; }

	Zenith_D3D12_PipelineBuilder& WithTesselation() { return *this; }

	// Concept entry point: compile a complete graphics pipeline from the spec.
	static void FromSpecification(Zenith_D3D12_Pipeline& /*xPipelineOut*/, const Flux_PipelineSpecification& /*xSpec*/) { }
};

//==========================================================================
// Zenith_D3D12_ComputePipelineBuilder  (satisfies FluxBackendComputePipelineBuilder)
//==========================================================================
class Zenith_D3D12_ComputePipelineBuilder
{
public:
	Zenith_D3D12_ComputePipelineBuilder() { }

	// Neutral fluent setter (param type is the backend Shader alias).
	Zenith_D3D12_ComputePipelineBuilder& WithShader(const Zenith_D3D12_Shader& /*xShader*/) { return *this; }

	// The engine's compute-pipeline setup chains .WithLayout(rootsig.m_xLayout);
	// the Vulkan overload takes a vk::PipelineLayout, so the null backend mirrors
	// it against the dummy neutral m_xLayout handle (a u_int) to keep those shared
	// call sites compiling.
	Zenith_D3D12_ComputePipelineBuilder& WithLayout(u_int /*uLayout*/) { return *this; }

	void Build(Zenith_D3D12_Pipeline& /*xPipelineOut*/) { }

	// Concept entry point: one-call WithShader + WithLayout + Build + root-sig assign.
	static void BuildFromShader(Zenith_D3D12_Pipeline& /*xPipelineOut*/,
	                            const Zenith_D3D12_Shader& /*xShader*/,
	                            const Zenith_D3D12_RootSig& /*xRootSig*/) { }

private:
	const Zenith_D3D12_Shader* m_pxShader = nullptr;
};
