#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include <cstring>   // memcmp / strlen (SPIR-V substring scan)

// ============================================================================
// Stage-0 Slang capability probes (Flux Shader System Overhaul).
//
// These lock in the exact Slang-language behaviours the overhaul stands on, run
// at boot in every Windows+Vulkan config as regression tripwires for future
// Slang upgrades. Each probe compiles a crafted in-memory snippet through the
// SAME session config the engine uses (Flux_SlangCompiler::CompileProbeFromSource)
// and asserts accept/reject + reflection + SPIR-V.
//
//   E1a  private member poked from the including scope MUST be rejected  (D1)
//   E1b  public method reading a private member compiles                 (D1)
//   E1c  free-function facade over the public method compiles            (D1 facade)
//   E1d  Sampler2D returned from a private unbounded array + sampled      (D1 bindless accessor)
//   E1e  can `extension` pierce `private`?  (RECORD — lint rule 2 closes it)
//   E2   visibility modifiers leave reflection byte-identical            (D1 fork)
//   E3   unreferenced ParameterBlocks still hold declaration-order spaces (spine 0/1/2)
//   E4   [SpecializationConstant] IDs + confirms reflection drops them   (D4/D5)
//   E5   generic body vs hand-written body -> SPIR-V parity              (D2/Stage 4)
//
// STAGE-0 FINDING (this run): `private` on a ParameterBlock member enforces, BUT a
// `public` accessor method requires the element struct itself to be `public struct`
// (Slang err 30601: a member cannot exceed its container's visibility). So D1 makes
// the spine element structs (ViewParams/GlobalParams/BindlessParams) `public struct`
// — a source-visibility change only, reflection-invariant (E2). Extensions do NOT
// pierce `private` (E1e), so lint rule 2 is belt-and-braces, not load-bearing.
// ============================================================================

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)

namespace
{
	// A ParameterBlock sink so the compute entry has a live write (no bare global,
	// which would claim space 0 and shift the content blocks under test).
	static const char* const kszProbeSink =
		"struct ProbeSinkParams { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<ProbeSinkParams> g_xProbeSinkSet;\n";

	bool ProbeHasBinding(const Flux_ShaderReflection& xRefl, const char* szName)
	{
		const Zenith_Vector<Flux_ReflectedBinding>& x = xRefl.GetBindings();
		for (u_int i = 0; i < x.GetSize(); i++)
		{
			if (x.Get(i).m_strName == szName) return true;
		}
		return false;
	}

	const Flux_ReflectedBinding* ProbeFindBinding(const Flux_ShaderReflection& xRefl, const char* szName)
	{
		const Zenith_Vector<Flux_ReflectedBinding>& x = xRefl.GetBindings();
		for (u_int i = 0; i < x.GetSize(); i++)
		{
			if (x.Get(i).m_strName == szName) return &x.Get(i);
		}
		return nullptr;
	}

	// Scan a SPIR-V word blob (bytes) for an ASCII substring — e.g. an OpExtension /
	// OpExtInstImport operand string like "NonSemantic.Shader.DebugInfo".
	bool ProbeSpirvContains(const Zenith_Vector<uint32_t>& axSpirv, const char* szNeedle)
	{
		if (axSpirv.GetSize() == 0) return false;
		const char* pBytes = reinterpret_cast<const char*>(&axSpirv.Get(0));
		const size_t uBytes = static_cast<size_t>(axSpirv.GetSize()) * sizeof(uint32_t);
		const size_t uNeedle = std::strlen(szNeedle);
		if (uNeedle == 0 || uNeedle > uBytes) return false;
		for (size_t i = 0; i + uNeedle <= uBytes; i++)
		{
			if (std::memcmp(pBytes + i, szNeedle, uNeedle) == 0) return true;
		}
		return false;
	}

	// Order-independent reflection equality on the fields that matter for the spine
	// (set / binding / size / kind / stage mask), matched by member name.
	bool ProbeReflEqual(const Flux_ShaderReflection& a, const Flux_ShaderReflection& b)
	{
		const Zenith_Vector<Flux_ReflectedBinding>& xa = a.GetBindings();
		const Zenith_Vector<Flux_ReflectedBinding>& xb = b.GetBindings();
		if (xa.GetSize() != xb.GetSize()) return false;
		for (u_int i = 0; i < xa.GetSize(); i++)
		{
			const Flux_ReflectedBinding& ea = xa.Get(i);
			const Flux_ReflectedBinding* pb = ProbeFindBinding(b, ea.m_strName.c_str());
			if (!pb) return false;
			if (ea.m_uSet != pb->m_uSet || ea.m_uBinding != pb->m_uBinding ||
				ea.m_uSize != pb->m_uSize || ea.m_eResourceKind != pb->m_eResourceKind ||
				ea.m_uStageMask != pb->m_uStageMask)
			{
				return false;
			}
		}
		return true;
	}
}

// --- E1a: a private ParameterBlock member poked directly from the including scope
//         must be REJECTED (the whole point of D1). The diagnostic must NOT be a
//         "higher visibility" error (that would mean a malformed probe, not a
//         genuine access rejection). ------------------------------------------------
ZENITH_TEST(SlangProbes, E1a_PrivatePokeRejected)
{
	std::string strSrc = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { private ConstantBuffer<ProbeCB> g_xProbeCB; };\n"
		"ParameterBlock<ProbeParams> g_xProbeSet;\n") + kszProbeSink +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = g_xProbeSet.g_xProbeCB.m_x; }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_FALSE(xRes.m_bCompiled,
		"E1a: direct poke of a `private` ParameterBlock member must fail to compile (else flip D1 to lint-only)");
	// Disambiguate: it must be an ACCESS rejection, not a visibility-mismatch caused by a malformed probe.
	ZENITH_ASSERT_TRUE(xRes.m_strDiagnostics.find("higher visibility") == std::string::npos,
		"E1a: rejection must be an access-control error, not a malformed-probe visibility error. Diag: %s",
		xRes.m_strDiagnostics.c_str());
}

// --- E1b: reading a private member through a PUBLIC METHOD on the same (public)
//         struct compiles (the accessor pattern D1 builds the facade on). --------
ZENITH_TEST(SlangProbes, E1b_PublicMethodAccessCompiles)
{
	std::string strSrc = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { private ConstantBuffer<ProbeCB> g_xProbeCB;\n"
		"  public float4 GetX() { return g_xProbeCB.m_x; } };\n"
		"ParameterBlock<ProbeParams> g_xProbeSet;\n") + kszProbeSink +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = g_xProbeSet.GetX(); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled,
		"E1b: a public method on a public struct reading a private member must compile. Diag: %s", xRes.m_strDiagnostics.c_str());
}

// --- E1c: a free function calling the public accessor compiles (the free-function
//         facade the spine call sites use). -------------------------------------
ZENITH_TEST(SlangProbes, E1c_FreeFunctionFacadeCompiles)
{
	std::string strSrc = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { private ConstantBuffer<ProbeCB> g_xProbeCB;\n"
		"  public float4 GetX() { return g_xProbeCB.m_x; } };\n"
		"ParameterBlock<ProbeParams> g_xProbeSet;\n"
		"float4 ProbeFacadeX() { return g_xProbeSet.GetX(); }\n") + kszProbeSink +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = ProbeFacadeX(); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled,
		"E1c: free-function facade over the accessor must compile. Diag: %s", xRes.m_strDiagnostics.c_str());
}

// --- E1d: returning a Sampler2D from a PRIVATE UNBOUNDED array via a public
//         accessor, then sampling it, compiles (the bindless GetBindlessTexture
//         accessor pattern). -----------------------------------------------------
ZENITH_TEST(SlangProbes, E1d_PrivateBindlessAccessorCompiles)
{
	std::string strSrc = std::string(
		"public struct ProbeBindlessParams { private Sampler2D g_axProbeTex[];\n"
		"  public Sampler2D GetTex(uint i) { return g_axProbeTex[i]; } };\n"
		"ParameterBlock<ProbeBindlessParams> g_xProbeBindlessSet;\n") + kszProbeSink +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = g_xProbeBindlessSet.GetTex(0).SampleLevel(float2(0.0, 0.0), 0.0); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled,
		"E1d: sampling a Sampler2D returned from a private unbounded array must compile. Diag: %s", xRes.m_strDiagnostics.c_str());
}

// --- E1e: can an `extension` pierce `private`? RECORD ONLY. Pierce() is left at
//         module (default) visibility so this probe tests access, not a visibility
//         mismatch. Whatever the answer, lint rule 2 (no `extension ViewParams`
//         outside Bindings.slang) is belt-and-braces. --------------------------
ZENITH_TEST(SlangProbes, E1e_ExtensionPiercingPrivateRecorded)
{
	std::string strSrc = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { private ConstantBuffer<ProbeCB> g_xProbeCB; };\n"
		"extension ProbeParams { float4 Pierce() { return g_xProbeCB.m_x; } }\n"
		"ParameterBlock<ProbeParams> g_xProbeSet;\n") + kszProbeSink +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = g_xProbeSet.Pierce(); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[SlangProbe E1e] extension-piercing-private: %s (lint rule 2 enforces regardless)",
		xRes.m_bCompiled ? "COMPILES (extensions can read privates)" : "REJECTED (privates opaque to extensions)");
	// No hard assertion — this is a recorded capability, not a dependency.
	ZENITH_ASSERT_TRUE(true, "E1e is informational");
}

// --- E2: swapping a member's visibility (private vs public) must leave the
//         reflected spine layout byte-identical (the D1 fork's safety premise). --
ZENITH_TEST(SlangProbes, E2_VisibilityReflectionInvariant)
{
	const char* szBodyTail =
		"ParameterBlock<ProbeParams> g_xProbeSet;\n"
		"struct ProbeSinkParams2 { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<ProbeSinkParams2> g_xProbeSinkSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xProbeSinkSet.g_xProbeSink[0] = g_xProbeSet.GetX(); }\n";

	std::string strPrivate = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { private ConstantBuffer<ProbeCB> g_xProbeCB;\n"
		"  public float4 GetX() { return g_xProbeCB.m_x; } };\n") + szBodyTail;
	std::string strPublic = std::string(
		"public struct ProbeCB { float4 m_x; };\n"
		"public struct ProbeParams { ConstantBuffer<ProbeCB> g_xProbeCB;\n"
		"  public float4 GetX() { return g_xProbeCB.m_x; } };\n") + szBodyTail;

	Flux_SlangProbeResult xPriv, xPub;
	Flux_SlangCompiler::CompileProbeFromSource(strPrivate.c_str(), "csMain", xPriv);
	Flux_SlangCompiler::CompileProbeFromSource(strPublic.c_str(),  "csMain", xPub);
	ZENITH_ASSERT_TRUE(xPriv.m_bHasReflection && xPub.m_bHasReflection,
		"E2: both variants must compile with reflection. PrivDiag: %s", xPriv.m_strDiagnostics.c_str());
	ZENITH_ASSERT_TRUE(ProbeReflEqual(xPriv.m_xReflection, xPub.m_xReflection),
		"E2: reflection must be identical whether the member is private or public");
}

// --- E3: an entry point that references only the MIDDLE ParameterBlock must still
//         leave all three blocks assigned declaration-order spaces 0/1/2 (the
//         invariant the textual-include spine order depends on). ----------------
ZENITH_TEST(SlangProbes, E3_UnreferencedBlocksHoldSpaces)
{
	std::string strSrc =
		"struct CB0 { float4 m_a; };\n"
		"struct CB1 { float4 m_b; };\n"
		"struct CB2 { float4 m_c; };\n"
		"struct P0 { ConstantBuffer<CB0> g_xBlock0; };\n"
		"struct P1 { ConstantBuffer<CB1> g_xBlock1; };\n"
		"struct P2 { ConstantBuffer<CB2> g_xBlock2; };\n"
		"ParameterBlock<P0> g_xSet0;\n"
		"ParameterBlock<P1> g_xSet1;\n"
		"ParameterBlock<P2> g_xSet2;\n"
		"struct SinkP { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<SinkP> g_xSinkSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xSinkSet.g_xProbeSink[0] = g_xSet1.g_xBlock1.m_b; }\n";  // references ONLY block 1

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bHasReflection, "E3: probe must compile with reflection. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_ReflectedBinding* p0 = ProbeFindBinding(xRes.m_xReflection, "g_xBlock0");
	const Flux_ReflectedBinding* p1 = ProbeFindBinding(xRes.m_xReflection, "g_xBlock1");
	const Flux_ReflectedBinding* p2 = ProbeFindBinding(xRes.m_xReflection, "g_xBlock2");
	ZENITH_ASSERT_TRUE(p0 && p1 && p2, "E3: all three ParameterBlock members must appear in reflection even if unreferenced");
	if (p0 && p1 && p2)
	{
		ZENITH_ASSERT_EQ(p0->m_uSet, 0u, "E3: block 0 must land space 0 by declaration order");
		ZENITH_ASSERT_EQ(p1->m_uSet, 1u, "E3: block 1 must land space 1 by declaration order");
		ZENITH_ASSERT_EQ(p2->m_uSet, 2u, "E3: block 2 (unreferenced) must still land space 2");
	}
}

// --- E4: [SpecializationConstant] gets stable IDs, and confirms the engine's
//         ExtractV2Reflection currently DROPS them (so folding view modes in D4
//         cannot perturb the descriptor-binding gates). ------------------------
ZENITH_TEST(SlangProbes, E4_SpecConstantsDroppedFromBindings)
{
	std::string strSrc =
		"[SpecializationConstant] const bool SC_A = true;\n"
		"[SpecializationConstant] const bool SC_B = false;\n"
		"struct SinkP { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<SinkP> g_xSinkSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ float f = (SC_A ? 1.0 : 0.0) + (SC_B ? 2.0 : 0.0); g_xSinkSet.g_xProbeSink[0] = float4(f, f, f, f); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bHasReflection, "E4: spec-constant probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	// The descriptor-binding reflection (what the 4 gates key on) must NOT list the spec constants.
	ZENITH_ASSERT_FALSE(ProbeHasBinding(xRes.m_xReflection, "SC_A"), "E4: SC_A must not appear as a descriptor binding");
	ZENITH_ASSERT_FALSE(ProbeHasBinding(xRes.m_xReflection, "SC_B"), "E4: SC_B must not appear as a descriptor binding");

	// The raw spec-constant walk must capture both with declaration-order IDs 0/1
	// (pins the D5 extraction API: getOffset(SPECIALIZATION_CONSTANT) + getDefaultValueInt).
	ZENITH_ASSERT_EQ(xRes.m_axSpecConstants.GetSize(), 2u, "E4: both spec constants must be captured by the raw walk");
	if (xRes.m_axSpecConstants.GetSize() == 2u)
	{
		ZENITH_ASSERT_EQ(xRes.m_axSpecConstants.Get(0).m_uId, 0u, "E4: SC_A must get constant_id 0 (declaration order)");
		ZENITH_ASSERT_EQ(xRes.m_axSpecConstants.Get(1).m_uId, 1u, "E4: SC_B must get constant_id 1 (declaration order)");
	}
	for (u_int i = 0; i < xRes.m_axSpecConstants.GetSize(); i++)
	{
		const Flux_SlangProbeResult::SpecConstant& xSC = xRes.m_axSpecConstants.Get(i);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SlangProbe E4]   '%s' id=%u default=%lld hasDefault=%d",
			xSC.m_strName.c_str(), xSC.m_uId, static_cast<long long>(xSC.m_iDefault), xSC.m_bHasDefault ? 1 : 0);
	}
}

// --- E5: a generic function instantiated to a concrete type should emit the same
//         SPIR-V as hand-writing the concrete body (the premise Stage 4's generics
//         rest on). Byte-exactness is validated offline (spirv-dis) at Stage 4;
//         here we assert both compile + emit SPIR-V and RECORD the word-count/byte
//         parity so a future Slang regression is visible. A trivial identity
//         generic isolates "does wrapping a value in a generic change codegen". --
ZENITH_TEST(SlangProbes, E5_GenericVsConcreteSpirvParity)
{
	const char* szTail =
		"struct SinkP { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<SinkP> g_xSinkSet;\n"
		"struct InCB { float4 m_v; };\n"
		"struct InP { ConstantBuffer<InCB> g_xIn; };\n"
		"ParameterBlock<InP> g_xInSet;\n";

	std::string strGeneric = std::string(
		"T ProbeIdentity<T>(T x) { return x; }\n") + szTail +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xSinkSet.g_xProbeSink[0] = ProbeIdentity<float4>(g_xInSet.g_xIn.m_v) * 2.0; }\n";

	std::string strConcrete = std::string(szTail) +
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xSinkSet.g_xProbeSink[0] = g_xInSet.g_xIn.m_v * 2.0; }\n";

	Flux_SlangProbeResult xGen, xCon;
	Flux_SlangCompiler::CompileProbeFromSource(strGeneric.c_str(),  "csMain", xGen);
	Flux_SlangCompiler::CompileProbeFromSource(strConcrete.c_str(), "csMain", xCon);
	ZENITH_ASSERT_TRUE(xGen.m_bCompiled && xCon.m_bCompiled,
		"E5: both generic and concrete variants must compile. GenDiag: %s", xGen.m_strDiagnostics.c_str());
	ZENITH_ASSERT_TRUE(xGen.m_axSpirv.GetSize() > 0 && xCon.m_axSpirv.GetSize() > 0, "E5: both must emit compute SPIR-V");

	bool bIdentical = (xGen.m_axSpirv.GetSize() == xCon.m_axSpirv.GetSize());
	if (bIdentical)
	{
		for (u_int i = 0; i < xGen.m_axSpirv.GetSize(); i++)
		{
			if (xGen.m_axSpirv.Get(i) != xCon.m_axSpirv.Get(i)) { bIdentical = false; break; }
		}
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[SlangProbe E5] generic=%u words concrete=%u words -> %s (offline spirv-dis is the Stage-4 arbiter)",
		xGen.m_axSpirv.GetSize(), xCon.m_axSpirv.GetSize(),
		bIdentical ? "BIT-IDENTICAL" : "DIFFER (names/debug; re-check after strip-debug)");
}

// --- E6: Slang debug info (Stage 1). The SAME source compiled with m_bEmitDebugInfo
//         must produce a STRICTLY LARGER SPIR-V blob carrying the NonSemantic debug-info
//         extension; without the flag it must not grow. Proves the debug-info seam works
//         where RenderDoc needs it (runtime Debug builds), while the checked-in artifacts
//         — compiled by FluxCompiler with the flag OFF — stay optimized + byte-identical. --
ZENITH_TEST(SlangProbes, E6_DebugInfoEmission)
{
	const char* szSrc =
		"struct SinkP { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<SinkP> g_xSinkSet;\n"
		"struct InCB { float4 m_v; };\n"
		"struct InP { ConstantBuffer<InCB> g_xIn; };\n"
		"ParameterBlock<InP> g_xInSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ g_xSinkSet.g_xProbeSink[0] = g_xInSet.g_xIn.m_v * 2.0; }\n";

	Flux_SlangProbeResult xOff, xOn;
	Flux_SlangCompiler::CompileProbeFromSource(szSrc, "csMain", xOff, /*bEmitDebugInfo*/false);
	Flux_SlangCompiler::CompileProbeFromSource(szSrc, "csMain", xOn,  /*bEmitDebugInfo*/true);
	ZENITH_ASSERT_TRUE(xOff.m_bCompiled && xOn.m_bCompiled,
		"E6: both compiles must succeed. OffDiag: %s OnDiag: %s", xOff.m_strDiagnostics.c_str(), xOn.m_strDiagnostics.c_str());
	ZENITH_ASSERT_TRUE(xOff.m_axSpirv.GetSize() > 0 && xOn.m_axSpirv.GetSize() > 0, "E6: both must emit compute SPIR-V");

	const bool bOffHas = ProbeSpirvContains(xOff.m_axSpirv, "NonSemantic.Shader.DebugInfo");
	const bool bOnHas  = ProbeSpirvContains(xOn.m_axSpirv,  "NonSemantic.Shader.DebugInfo");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "[SlangProbe E6] debug-off=%u words debug-on=%u words; NonSemantic off=%d on=%d",
		xOff.m_axSpirv.GetSize(), xOn.m_axSpirv.GetSize(), bOffHas ? 1 : 0, bOnHas ? 1 : 0);

	ZENITH_ASSERT_TRUE(xOn.m_axSpirv.GetSize() > xOff.m_axSpirv.GetSize(),
		"E6: debug-info SPIR-V must be strictly larger than the optimized blob");
	ZENITH_ASSERT_TRUE(bOnHas, "E6: debug-info SPIR-V must carry the NonSemantic.Shader.DebugInfo extension");
}

// --- Stage 3a: the engine's own spec-constant extractor (ExtractSpecConstants),
//     now run after ExtractV2Reflection in both CompileProgram and the probe path,
//     must land the spec constants in the reflection's dedicated table (name /
//     declaration-order id / packed default / type) — the path CompileProgram bakes
//     into the v5 sidecar. Distinct from E4, which checks the probe's own raw walk.
ZENITH_TEST(SlangProbes, S3a_SpecConstantsInReflection)
{
	std::string strSrc =
		"[SpecializationConstant] const bool SC_SHADOWS = true;\n"
		"[SpecializationConstant] const bool SC_CLUSTER = false;\n"
		"struct SinkP { RWStructuredBuffer<float4> g_xProbeSink; };\n"
		"ParameterBlock<SinkP> g_xSinkSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{ float f = (SC_SHADOWS ? 1.0 : 0.0) + (SC_CLUSTER ? 2.0 : 0.0); g_xSinkSet.g_xProbeSink[0] = float4(f, f, f, f); }\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(strSrc.c_str(), "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bHasReflection, "S3a: probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	// The reflection's spec table (v5 payload) carries both, in declaration order.
	ZENITH_ASSERT_EQ(xRes.m_xReflection.GetSpecConstants().GetSize(), 2u, "S3a: both spec constants captured into the reflection table");

	const Flux_ReflectedSpecConstant* pxShadows = xRes.m_xReflection.GetSpecConstant("SC_SHADOWS");
	const Flux_ReflectedSpecConstant* pxCluster = xRes.m_xReflection.GetSpecConstant("SC_CLUSTER");
	ZENITH_ASSERT_NOT_NULL(pxShadows, "S3a: SC_SHADOWS present in the reflection table");
	ZENITH_ASSERT_NOT_NULL(pxCluster, "S3a: SC_CLUSTER present in the reflection table");
	if (pxShadows)
	{
		ZENITH_ASSERT_EQ(pxShadows->m_uConstantId, 0u, "S3a: SC_SHADOWS id 0 (declaration order)");
		ZENITH_ASSERT_EQ(pxShadows->m_uSize, 4u, "S3a: int-family spec constant is 4 bytes");
		ZENITH_ASSERT_EQ(pxShadows->m_uDefaultValue, 1u, "S3a: SC_SHADOWS default true -> 1");
	}
	if (pxCluster)
	{
		ZENITH_ASSERT_EQ(pxCluster->m_uConstantId, 1u, "S3a: SC_CLUSTER id 1 (declaration order)");
		ZENITH_ASSERT_EQ(pxCluster->m_uDefaultValue, 0u, "S3a: SC_CLUSTER default false -> 0");
	}

	// And it is STILL dropped from the descriptor-binding table (root-sig neutrality).
	ZENITH_ASSERT_FALSE(ProbeHasBinding(xRes.m_xReflection, "SC_SHADOWS"), "S3a: spec constant must not be a descriptor binding");
}

#endif // ZENITH_WINDOWS && ZENITH_VULKAN
