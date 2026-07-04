#include "Zenith.h"

#include "Flux/Slang/Flux_ShaderCatalog.h"
#include "Flux/Slang/Flux_SlangCompiler.h" // Flux_SlangProgramDesc (DescribeProgram fill)
#include "Flux/Flux_FeatureRegistry.h"     // ValidateFeatureParity: GetFeatures / m_paxShaders

// Every engine feature's shader decls. The catalog is the UNION of these
// apxALL arrays plus the small explicit unowned list below. Tools-only
// features are gated so the catalog set matches the registered-feature set in
// both tools configs (parity holds either way).
#include "Flux/IBL/Flux_IBL_Shaders.h"
#include "Flux/UnifiedMesh/Flux_UnifiedMesh_Shaders.h"
#include "Flux/Terrain/Flux_Terrain_Shaders.h"
#include "Flux/Primitives/Flux_Primitives_Shaders.h"
#include "Flux/Skybox/Flux_Skybox_Shaders.h"
#include "Flux/Decals/Flux_Decals_Shaders.h"
#include "Flux/HiZ/Flux_HiZ_Shaders.h"
#include "Flux/SSR/Flux_SSR_Shaders.h"
#include "Flux/SSGI/Flux_SSGI_Shaders.h"
#include "Flux/SSAO/Flux_SSAO_Shaders.h"
#include "Flux/DynamicLights/Flux_LightClustering_Shaders.h"
#include "Flux/DeferredShading/Flux_DeferredShading_Shaders.h"
#include "Flux/Vegetation/Flux_Grass_Shaders.h"
#include "Flux/Translucency/Flux_Translucency_Shaders.h"
#include "Flux/Fog/Flux_Fog_Shaders.h"
#include "Flux/SDFs/Flux_SDFs_Shaders.h"
#include "Flux/Particles/Flux_Particles_Shaders.h"
#include "Flux/HDR/Flux_HDR_Shaders.h"
#include "Flux/TAA/Flux_TAA_Shaders.h"
#include "Flux/Quads/Flux_Quads_Shaders.h"
#include "Flux/Text/Flux_Text_Shaders.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos_Shaders.h"
#endif
#include "Flux/Present/Flux_Present_Shaders.h"
// Engine programs no feature owns (ComputeTest* vestigial; DPFog game-interim).
#include "Flux/Slang/Flux_UnownedEngineShaders.h"

#include <cstring> // strcmp — Validate duplicate-name / stem checks
#include <vector>  // Validate stem-uniqueness scratch

namespace
{
	// Flat program index = each feature's apxALL block, in feature-registration
	// order, then the unowned block.
	struct FeatureBlock { const Flux_ShaderDecl* const* m_apx; u_int m_uCount; };
	template<u_int N> constexpr FeatureBlock MakeBlock(const Flux_ShaderDecl* const (&a)[N]) { return { a, N }; }

	constexpr FeatureBlock s_axBlocks[] =
	{
		MakeBlock(Flux_IBLShaders::apxALL),
		MakeBlock(Flux_UnifiedMeshShaders::apxALL),
		MakeBlock(Flux_TerrainShaders::apxALL),
		MakeBlock(Flux_PrimitivesShaders::apxALL),
		MakeBlock(Flux_SkyboxShaders::apxALL),
		MakeBlock(Flux_DecalsShaders::apxALL),
		MakeBlock(Flux_HiZShaders::apxALL),
		MakeBlock(Flux_SSRShaders::apxALL),
		MakeBlock(Flux_SSGIShaders::apxALL),
		MakeBlock(Flux_SSAOShaders::apxALL),
		MakeBlock(Flux_LightClusteringShaders::apxALL),
		MakeBlock(Flux_DeferredShadingShaders::apxALL),
		MakeBlock(Flux_GrassShaders::apxALL),
		MakeBlock(Flux_TranslucencyShaders::apxALL),
		MakeBlock(Flux_FogShaders::apxALL),
		MakeBlock(Flux_SDFsShaders::apxALL),
		MakeBlock(Flux_ParticlesShaders::apxALL),
		MakeBlock(Flux_TAAShaders::apxALL),
		MakeBlock(Flux_HDRShaders::apxALL),
		MakeBlock(Flux_QuadsShaders::apxALL),
		MakeBlock(Flux_TextShaders::apxALL),
#ifdef ZENITH_TOOLS
		MakeBlock(Flux_GizmosShaders::apxALL),
#endif
		MakeBlock(Flux_PresentShaders::apxALL),
		MakeBlock(Flux_UnownedEngineShaders::apxALL),
	};
}

u_int Flux_ShaderCatalog::GetProgramCount()
{
	u_int uCount = 0;
	for (const FeatureBlock& xBlock : s_axBlocks) uCount += xBlock.m_uCount;
	return uCount;
}

const Flux_ShaderDecl& Flux_ShaderCatalog::GetProgramByIndex(u_int uIndex)
{
	u_int u = uIndex;
	for (const FeatureBlock& xBlock : s_axBlocks)
	{
		if (u < xBlock.m_uCount) return *xBlock.m_apx[u];
		u -= xBlock.m_uCount;
	}
	Zenith_Assert(false, "Flux_ShaderCatalog: program index %u out of bounds (count %u)", uIndex, GetProgramCount());
	return *s_axBlocks[0].m_apx[0];
}

void Flux_ShaderCatalog::DescribeProgram(const Flux_ShaderDecl& xDecl, Flux_SlangProgramDesc& xDescOut)
{
	xDescOut.m_szModuleName    = xDecl.m_szModuleName;
	xDescOut.m_szVertexEntry   = xDecl.m_szVertexEntry;
	xDescOut.m_szFragmentEntry = xDecl.m_szFragmentEntry;
	xDescOut.m_szComputeEntry  = xDecl.m_szComputeEntry;
	xDescOut.m_szTargetProfile = xDecl.m_szTargetProfile;
}

// Per-stage artifact stems disambiguate vertex vs fragment SPIR-V from the same
// module — two stages share a module file but produce two .spv blobs.
std::string Flux_ShaderCatalog::GetVertexArtifactStem(const Flux_ShaderDecl& xDecl)
{
	Zenith_Assert(xDecl.m_szVertexEntry, "GetVertexArtifactStem: program '%s' has no vertex stage", xDecl.m_szName);
	return std::string(xDecl.m_szModuleName) + "." + xDecl.m_szVertexEntry;
}

std::string Flux_ShaderCatalog::GetFragmentArtifactStem(const Flux_ShaderDecl& xDecl)
{
	Zenith_Assert(xDecl.m_szFragmentEntry, "GetFragmentArtifactStem: program '%s' has no fragment stage", xDecl.m_szName);
	return std::string(xDecl.m_szModuleName) + "." + xDecl.m_szFragmentEntry;
}

std::string Flux_ShaderCatalog::GetComputeArtifactStem(const Flux_ShaderDecl& xDecl)
{
	Zenith_Assert(xDecl.m_szComputeEntry, "GetComputeArtifactStem: program '%s' has no compute stage", xDecl.m_szName);
	return std::string(xDecl.m_szModuleName) + "." + xDecl.m_szComputeEntry;
}

bool Flux_ShaderCatalog::Validate(std::string& strErrOut)
{
	const u_int uCount = GetProgramCount();
	Zenith_Vector<std::string> axStems;

	for (u_int u = 0; u < uCount; u++)
	{
		const Flux_ShaderDecl& xDecl = GetProgramByIndex(u);

		const char* szName = xDecl.m_szName;
		if (!szName || !szName[0]) { strErrOut = "Flux_ShaderCatalog: a decl has a null/empty m_szName"; return false; }
		auto fnFail = [&](const char* szWhat) { strErrOut = std::string("Flux_ShaderCatalog: program '") + szName + "' " + szWhat; return false; };

		if (!xDecl.m_szModuleName    || !xDecl.m_szModuleName[0])    return fnFail("has a null/empty m_szModuleName");
		if (!xDecl.m_szTargetProfile || !xDecl.m_szTargetProfile[0]) return fnFail("has a null/empty m_szTargetProfile");
		if (!xDecl.m_szSubsystem     || !xDecl.m_szSubsystem[0])     return fnFail("has a null/empty m_szSubsystem");

		const bool bHasV = xDecl.m_szVertexEntry   && xDecl.m_szVertexEntry[0];
		const bool bHasF = xDecl.m_szFragmentEntry && xDecl.m_szFragmentEntry[0];
		const bool bHasC = xDecl.m_szComputeEntry  && xDecl.m_szComputeEntry[0];
		const bool bGraphics = bHasV && bHasF && !bHasC;
		const bool bCompute  = bHasC && !bHasV && !bHasF;
		if (!bGraphics && !bCompute)
			return fnFail("must be graphics (vertex+fragment, no compute) XOR compute (compute only)");

		if (bGraphics)
		{
			axStems.PushBack(GetVertexArtifactStem(xDecl));
			axStems.PushBack(GetFragmentArtifactStem(xDecl));
		}
		else
		{
			axStems.PushBack(GetComputeArtifactStem(xDecl));
		}
	}

	// Duplicate name.
	for (u_int u = 0; u < uCount; u++)
	{
		const char* szA = GetProgramByIndex(u).m_szName;
		for (u_int v = u + 1; v < uCount; v++)
		{
			if (strcmp(szA, GetProgramByIndex(v).m_szName) == 0)
			{
				strErrOut = std::string("Flux_ShaderCatalog: duplicate program name '") + szA + "'";
				return false;
			}
		}
	}

	// Duplicate module+entry stem (would collide on disk).
	for (u_int i = 0; i < axStems.GetSize(); i++)
	{
		for (u_int j = i + 1; j < axStems.GetSize(); j++)
		{
			if (axStems.Get(i) == axStems.Get(j))
			{
				strErrOut = "Flux_ShaderCatalog: duplicate artifact stem '" + axStems.Get(i) + "'";
				return false;
			}
		}
	}

	return true;
}

bool Flux_ShaderCatalog::IsCanonicalToolsBuild()
{
#ifdef ZENITH_TOOLS
	return true;
#else
	return false;
#endif
}

bool Flux_ShaderCatalog::ValidateFeatureParity(const Flux_FeatureRegistry& xRegistry, std::string& strErrOut)
{
	auto fnName = [](const Flux_ShaderDecl* p) { return p && p->m_szName ? p->m_szName : "?"; };

	// 1. The catalog's flat decl set.
	const u_int uCount = GetProgramCount();
	Zenith_Vector<const Flux_ShaderDecl*> axCatalog;
	axCatalog.Reserve(uCount);
	for (u_int u = 0; u < uCount; u++) axCatalog.PushBack(&GetProgramByIndex(u));

	// 2. Union of every registered feature's apxALL, detecting double-ownership.
	Zenith_Vector<const Flux_ShaderDecl*> axOwned;
	const u_int uFeatures = xRegistry.GetNumFeatures();
	for (u_int f = 0; f < uFeatures; f++)
	{
		const Flux_FeatureDesc& xFeat = xRegistry.GetFeatures()[f];
		for (u_int s = 0; s < xFeat.m_uShaderCount; s++)
		{
			const Flux_ShaderDecl* pxDecl = xFeat.m_paxShaders[s];
			for (const Flux_ShaderDecl* pxSeen : axOwned)
			{
				if (pxSeen == pxDecl)
				{
					strErrOut = std::string("Flux_ShaderCatalog::ValidateFeatureParity: program '")
						+ fnName(pxDecl) + "' is owned by more than one feature";
					return false;
				}
			}
			axOwned.PushBack(pxDecl);
		}
	}

	// 3. Owned and unowned must be disjoint.
	for (const Flux_ShaderDecl* pxU : Flux_UnownedEngineShaders::apxALL)
		for (const Flux_ShaderDecl* pxO : axOwned)
			if (pxU == pxO)
			{
				strErrOut = std::string("Flux_ShaderCatalog::ValidateFeatureParity: program '")
					+ fnName(pxU) + "' is both feature-owned and in apxUnownedEnginePrograms";
				return false;
			}

	auto fnContains = [](const Zenith_Vector<const Flux_ShaderDecl*>& v, const Flux_ShaderDecl* p)
	{
		for (const Flux_ShaderDecl* q : v) if (q == p) return true;
		return false;
	};

	// 4a. Every catalog decl must be owned by a feature or explicitly unowned.
	for (const Flux_ShaderDecl* pxC : axCatalog)
	{
		bool bUnowned = false;
		for (const Flux_ShaderDecl* pxU : Flux_UnownedEngineShaders::apxALL) if (pxU == pxC) { bUnowned = true; break; }
		if (!bUnowned && !fnContains(axOwned, pxC))
		{
			strErrOut = std::string("Flux_ShaderCatalog::ValidateFeatureParity: catalog program '")
				+ fnName(pxC) + "' is neither owned by a registered feature nor in apxUnownedEnginePrograms"
				+ " (a feature was registered without it, or it should be unowned)";
			return false;
		}
	}

	// 4b. Every feature-owned decl must be in the catalog (missing catalog include).
	for (const Flux_ShaderDecl* pxO : axOwned)
		if (!fnContains(axCatalog, pxO))
		{
			strErrOut = std::string("Flux_ShaderCatalog::ValidateFeatureParity: feature-owned program '")
				+ fnName(pxO) + "' is missing from the catalog (add its feature's _Shaders.h MakeBlock)";
			return false;
		}

	// 4c. Every explicitly-unowned decl must be in the catalog too.
	for (const Flux_ShaderDecl* pxU : Flux_UnownedEngineShaders::apxALL)
		if (!fnContains(axCatalog, pxU))
		{
			strErrOut = std::string("Flux_ShaderCatalog::ValidateFeatureParity: unowned program '")
				+ fnName(pxU) + "' is missing from the catalog";
			return false;
		}

	// 5. Belt-and-braces: exact-size match catches a duplicate decl in the catalog
	// blocks (set equality above would otherwise mask a repeated pointer).
	const size_t uUnowned = sizeof(Flux_UnownedEngineShaders::apxALL) / sizeof(Flux_UnownedEngineShaders::apxALL[0]);
	if (static_cast<size_t>(axCatalog.GetSize()) != static_cast<size_t>(axOwned.GetSize()) + uUnowned)
	{
		strErrOut = "Flux_ShaderCatalog::ValidateFeatureParity: catalog count != (feature-owned + unowned) "
			"— a decl is listed twice in the catalog blocks";
		return false;
	}

	return true;
}

// Flux Shader System Overhaul — D6 spine-lint tests. Pure (Flux_SpineLint is a
// header-only text scanner), so hosted in this always-linked catalog TU in every
// config (dead-strip idiom — the .inl's static test registrations stay live).
#include "Flux/Slang/Flux_SpineLint.Tests.inl"

// Flux Shader System Overhaul — Stage 3a reflection-sidecar v5 round-trip tests.
// The serializer is backend-neutral (all configs read .spv.refl), so host here in
// the always-linked catalog TU.
#include "Flux/Slang/Flux_ShaderReflection.Tests.inl"
