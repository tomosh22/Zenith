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
//   E5b  clustered-light BRDF context -> one explicit-LOD LUT sample in SPIR-V
//
// Vertex-layout capability spike (T0). Same idea, on the VERTEX entry point, deciding
// whether a vertex-layout generator can read Slang field user-attributes out of the
// LINKED program instead of falling back to a semantic-suffix convention:
//
//   V1   [VtxFmt("half4")] on a VsIn field survives compose+link
//   V2   per-field varying locations + semantic name/index
//   V3   SV_* fields are excludable (they take no varying category at all)
//   V4   [PerInstance] (zero-argument attribute) survives compose+link
//   V5   TypeReflection gives the scalar kind + lane count format inference needs
//
// T0 FINDING (this run): field user-attributes DO survive linking — the whole V1-V5 set
// passes, so the semantic-suffix fallback is not needed. See the per-probe comments.
//
// T2.a builds the production extractor on those findings; V6/V7 exercise IT (not the raw
// capture) against a live Slang session — the one place the SV_ exclusion, the [VtxFmt]
// override, the [PerInstance] split and the tight-pack arithmetic are proven end to end:
//
//   V6   the extracted table: SV_* dropped, storage tags inferred/overridden, bytes packed
//   V7   an unknown [VtxFmt] string is a hard error naming the offending field
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

	// Count instructions by opcode in a SPIR-V word stream. The first five words
	// are the module header; each following instruction encodes word-count/opcode
	// in its first word. Used by E5b to lock the BRDF LUT's explicit-LOD codegen.
	u_int ProbeSpirvOpcodeCount(const Zenith_Vector<uint32_t>& axSpirv, const uint32_t uOpcode)
	{
		u_int uCount = 0;
		for (u_int i = 5; i < axSpirv.GetSize();)
		{
			const uint32_t uInstruction = axSpirv.Get(i);
			const u_int uWordCount = static_cast<u_int>(uInstruction >> 16u);
			const u_int uCurrentOpcode = static_cast<u_int>(uInstruction & 0xFFFFu);
			if (uWordCount == 0u || i + uWordCount > axSpirv.GetSize()) break;
			if (uCurrentOpcode == uOpcode) uCount++;
			i += uWordCount;
		}
		return uCount;
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

	// One vertex source drives every V-probe: a VsIn carrying plain fields, an attributed field,
	// a half-precision field and two SV_* system values. Slang has no built-in vertex-format
	// attribute, so the probe declares the two it exercises the same way the stdlib declares its
	// own — [__AttributeUsage(_AttributeTargets.Var)] on a struct whose name ends in "Attribute".
	static const char* const kszProbeVertexSource =
		"[__AttributeUsage(_AttributeTargets.Var)]\n"
		"struct VtxFmtAttribute { string format; };\n"
		"[__AttributeUsage(_AttributeTargets.Var)]\n"
		"struct PerInstanceAttribute {};\n"
		"struct ProbeVsIn\n"
		"{\n"
		"	float3 m_xPos : POSITION;\n"
		"	float2 m_xUV0 : TEXCOORD0;\n"
		"	float2 m_xUV1 : TEXCOORD1;\n"
		"	[VtxFmt(\"half4\")] float4 m_xTangent : TANGENT;\n"
		"	half4 m_xColour : COLOR;\n"
		"	[PerInstance] float4 m_xInstRow : INSTANCEROW;\n"
		"	uint m_uVertexID : SV_VertexID;\n"
		"	uint m_uInstanceID : SV_InstanceID;\n"
		"};\n"
		"struct ProbeVsOut { float4 m_xPos : SV_Position; };\n"
		"[shader(\"vertex\")]\n"
		"ProbeVsOut vsMain(ProbeVsIn xIn)\n"
		"{\n"
		"	ProbeVsOut xOut;\n"
		"	xOut.m_xPos = float4(xIn.m_xPos, 1.0)\n"
		"		+ float4(xIn.m_xUV0, 0.0, 0.0) + float4(xIn.m_xUV1, 0.0, 0.0)\n"
		"		+ xIn.m_xTangent + float4(xIn.m_xColour) + xIn.m_xInstRow\n"
		"		+ float(xIn.m_uVertexID) + float(xIn.m_uInstanceID);\n"
		"	return xOut;\n"
		"}\n";

	const Flux_SlangProbeResult::VertexInputField* ProbeFindVertexField(const Flux_SlangProbeResult& xRes, const char* szName)
	{
		const Zenith_Vector<Flux_SlangProbeResult::VertexInputField>& x = xRes.m_axVertexInputs;
		for (u_int i = 0; i < x.GetSize(); i++)
		{
			if (x.Get(i).m_strName == szName) return &x.Get(i);
		}
		return nullptr;
	}

	const Flux_SlangProbeResult::VertexAttribute* ProbeFindVertexAttribute(const Flux_SlangProbeResult::VertexInputField& xField,
																			const char* szName)
	{
		for (u_int i = 0; i < xField.m_axAttributes.GetSize(); i++)
		{
			if (xField.m_axAttributes.Get(i).m_strName == szName) return &xField.m_axAttributes.Get(i);
		}
		return nullptr;
	}

	// T2.a: the same shape as kszProbeVertexSource but with a typo'd storage format, so the
	// extractor's hard-error path runs against a real compile rather than a synthetic call.
	static const char* const kszProbeBadVertexFormatSource =
		"[__AttributeUsage(_AttributeTargets.Var)]\n"
		"struct VtxFmtAttribute { string format; };\n"
		"struct BadVsIn\n"
		"{\n"
		"	float3 m_xPos : POSITION;\n"
		"	[VtxFmt(\"half3\")] float3 m_xNormal : NORMAL;\n"
		"};\n"
		"struct BadVsOut { float4 m_xPos : SV_Position; };\n"
		"[shader(\"vertex\")]\n"
		"BadVsOut vsMain(BadVsIn xIn)\n"
		"{\n"
		"	BadVsOut xOut;\n"
		"	xOut.m_xPos = float4(xIn.m_xPos + xIn.m_xNormal, 1.0);\n"
		"	return xOut;\n"
		"}\n";

	const Flux_ReflectedVertexAttribute* ProbeFindExtractedAttribute(const Flux_ShaderReflection& xRefl, const char* szName)
	{
		const Zenith_Vector<Flux_ReflectedVertexAttribute>& x = xRefl.GetVertexAttributes();
		for (u_int i = 0; i < x.GetSize(); i++)
		{
			if (x.Get(i).m_strName == szName) return &x.Get(i);
		}
		return nullptr;
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

// --- E5b: the production clustered-light API must keep the per-pixel BRDF LUT
//          lookup outside the runtime light loop and compile it as SampleLevel(0).
//          Compiling this exact Common.Lighting API is the signature/reflection
//          parity tripwire; the SPIR-V assertions prevent an implicit-derivative
//          sample or duplicated static sample from silently returning. ----------
ZENITH_TEST(SlangProbes, E5b_LightBRDFContextExplicitLod)
{
	const char* szSrc =
		"import Common.Lighting;\n"
		"struct ProbeInputs\n"
		"{\n"
		"  float4 m_xAlbedoMetallic;\n"
		"  float4 m_xNormalRoughness;\n"
		"  float4 m_xViewSpecular;\n"
		"  uint m_uLightCount;\n"
		"};\n"
		"struct ProbeParams\n"
		"{\n"
		"  ConstantBuffer<ProbeInputs> g_xInputs;\n"
		"  Sampler2D g_xBRDFLUT;\n"
		"  StructuredBuffer<LightInstance> g_xLights;\n"
		"  RWStructuredBuffer<float4> g_xSink;\n"
		"};\n"
		"ParameterBlock<ProbeParams> g_xProbeSet;\n"
		"[shader(\"compute\")] [numthreads(1,1,1)]\n"
		"void csMain(uint3 tid : SV_DispatchThreadID)\n"
		"{\n"
		"  float3 xAlbedo = g_xProbeSet.g_xInputs.m_xAlbedoMetallic.xyz;\n"
		"  float fMetallic = g_xProbeSet.g_xInputs.m_xAlbedoMetallic.w;\n"
		"  float3 xNormal = normalize(g_xProbeSet.g_xInputs.m_xNormalRoughness.xyz);\n"
		"  float fRoughness = g_xProbeSet.g_xInputs.m_xNormalRoughness.w;\n"
		"  float3 xViewDir = normalize(g_xProbeSet.g_xInputs.m_xViewSpecular.xyz);\n"
		"  float fSpecular = g_xProbeSet.g_xInputs.m_xViewSpecular.w;\n"
		"  uint uLightCount = min(g_xProbeSet.g_xInputs.m_uLightCount, 4u);\n"
		"  float3 xSum = float3(0.0);\n"
		"  if (uLightCount > 0u)\n"
		"  {\n"
		"    LightBRDFContext xContext = PrepareLightBRDFContext(\n"
		"      xAlbedo, xNormal, xViewDir, fMetallic, fRoughness, fSpecular, g_xProbeSet.g_xBRDFLUT);\n"
		"    [loop] for (uint i = 0u; i < uLightCount; ++i)\n"
		"      xSum += EvaluateLight(g_xProbeSet.g_xLights[i], float3(0.0), xNormal, xViewDir,\n"
		"                            xAlbedo, fMetallic, xContext);\n"
		"  }\n"
		"  g_xProbeSet.g_xSink[0] = float4(xSum, 1.0);\n"
		"}\n";

	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileProbeFromSource(szSrc, "csMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled,
		"E5b: production LightBRDFContext API must compile. Diag: %s", xRes.m_strDiagnostics.c_str());
	ZENITH_ASSERT_TRUE(xRes.m_bHasReflection && xRes.m_axSpirv.GetSize() > 0u,
		"E5b: context probe must emit reflection and compute SPIR-V");
	ZENITH_ASSERT_TRUE(ProbeHasBinding(xRes.m_xReflection, "g_xInputs") &&
		ProbeHasBinding(xRes.m_xReflection, "g_xBRDFLUT") &&
		ProbeHasBinding(xRes.m_xReflection, "g_xLights") &&
		ProbeHasBinding(xRes.m_xReflection, "g_xSink"),
		"E5b: context refactor must preserve the input/LUT/light/sink descriptor contract");
	ZENITH_ASSERT_EQ(xRes.m_xReflection.GetBindings().GetSize(), 4u,
		"E5b: context refactor must not introduce any additional descriptor binding");

	// SPIR-V 1.x opcodes: OpImageSampleImplicitLod=87,
	// OpImageSampleExplicitLod=88. There is one authored LUT sample and it must
	// stay explicit even though the light loop is runtime-sized.
	const u_int uImplicitSamples = ProbeSpirvOpcodeCount(xRes.m_axSpirv, 87u);
	const u_int uExplicitSamples = ProbeSpirvOpcodeCount(xRes.m_axSpirv, 88u);
	ZENITH_ASSERT_EQ(uImplicitSamples, 0u, "E5b: clustered BRDF LUT must not use implicit derivatives");
	ZENITH_ASSERT_EQ(uExplicitSamples, 1u, "E5b: clustered BRDF context must emit exactly one explicit-LOD LUT sample");
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

// --- V1: THE decision-point probe. A field user-attribute must still be readable from the
//         entry-point reflection of the LINKED program — link() is where a stripped attribute
//         would disappear, and the probe reads it only after link. Both routes are exercised:
//         enumeration (getUserAttributeByIndex) and the by-name lookup a generator would use.
//         If this ever fails, the program falls back to a semantic-suffix convention. -------
ZENITH_TEST(SlangProbes, V1_FieldUserAttributeSurvivesLink)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V1: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());
	ZENITH_ASSERT_GT(xRes.m_axVertexInputs.GetSize(), 0u, "V1: the linked vertex entry must expose its input fields");

	const Flux_SlangProbeResult::VertexInputField* pxTangent = ProbeFindVertexField(xRes, "m_xTangent");
	ZENITH_ASSERT_NOT_NULL(pxTangent, "V1: the [VtxFmt]-annotated field must appear in entry-point reflection");
	if (!pxTangent) return;

	const Flux_SlangProbeResult::VertexAttribute* pxAttrib = ProbeFindVertexAttribute(*pxTangent, "VtxFmt");
	ZENITH_ASSERT_NOT_NULL(pxAttrib,
		"V1: [VtxFmt(\"half4\")] must be readable POST-LINK (%u attribute(s) seen on the field)",
		pxTangent->m_axAttributes.GetSize());
	if (!pxAttrib) return;

	ZENITH_ASSERT_TRUE(pxAttrib->m_bHasArg0, "V1: the attribute's string argument must be readable");
	ZENITH_ASSERT_STREQ(pxAttrib->m_strArg0.c_str(), "half4", "V1: argument 0 must round-trip as the authored string");
	ZENITH_ASSERT_TRUE(pxAttrib->m_bFoundByName,
		"V1: findAttributeByName(globalSession, \"VtxFmt\") must resolve the same attribute the enumeration found");
}

// --- V2: per-field varying offsets + semantics. Locations are dense and follow DECLARATION
//         order, and Slang splits a trailing digit off the semantic into the index (TEXCOORD1
//         -> name TEXCOORD, index 1) while upper-casing the name. -------------------------
ZENITH_TEST(SlangProbes, V2_VaryingOffsetsAndSemantics)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V2: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_SlangProbeResult::VertexInputField* pxPos  = ProbeFindVertexField(xRes, "m_xPos");
	const Flux_SlangProbeResult::VertexInputField* pxUV0  = ProbeFindVertexField(xRes, "m_xUV0");
	const Flux_SlangProbeResult::VertexInputField* pxUV1  = ProbeFindVertexField(xRes, "m_xUV1");
	const Flux_SlangProbeResult::VertexInputField* pxInst = ProbeFindVertexField(xRes, "m_xInstRow");
	ZENITH_ASSERT_TRUE(pxPos && pxUV0 && pxUV1 && pxInst, "V2: every declared VsIn field must be reflected");
	if (!(pxPos && pxUV0 && pxUV1 && pxInst)) return;

	ZENITH_ASSERT_TRUE(pxPos->m_bHasVaryingInput && pxUV0->m_bHasVaryingInput && pxUV1->m_bHasVaryingInput,
		"V2: an ordinary VsIn field must carry the VaryingInput (== VertexInput) category");
	ZENITH_ASSERT_EQ(pxPos->m_uVaryingLocation, 0u, "V2: POSITION is the first declared field -> location 0");
	ZENITH_ASSERT_EQ(pxUV0->m_uVaryingLocation, 1u, "V2: TEXCOORD0 -> location 1");
	ZENITH_ASSERT_EQ(pxUV1->m_uVaryingLocation, 2u, "V2: TEXCOORD1 -> location 2");
	ZENITH_ASSERT_EQ(pxInst->m_uVaryingLocation, 5u, "V2: the sixth varying field -> location 5 (SV_* take none)");

	ZENITH_ASSERT_STREQ(pxPos->m_strSemanticName.c_str(), "POSITION", "V2: semantic names come back upper-cased");
	ZENITH_ASSERT_STREQ(pxUV0->m_strSemanticName.c_str(), "TEXCOORD", "V2: the trailing digit is split off the name");
	ZENITH_ASSERT_STREQ(pxUV1->m_strSemanticName.c_str(), "TEXCOORD", "V2: TEXCOORD1 shares the base name with TEXCOORD0");
	ZENITH_ASSERT_EQ(pxUV0->m_uSemanticIndex, 0u, "V2: TEXCOORD0 -> semantic index 0");
	ZENITH_ASSERT_EQ(pxUV1->m_uSemanticIndex, 1u, "V2: TEXCOORD1 -> semantic index 1");
}

// --- V3: SV_* exclusion, the vertex-side counterpart of the system-value drop in
//         BuildV2BindingFromParam. A system value is still REFLECTED as a field, but takes no
//         parameter category at all — so "has a VaryingInput category" is the load-bearing
//         discriminator and the SV_ name prefix is only corroboration. SV_* fields consume no
//         location, which is why the six real fields stay dense at 0..5. -------------------
ZENITH_TEST(SlangProbes, V3_SystemValueFieldsExcluded)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V3: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_SlangProbeResult::VertexInputField* pxVertexID   = ProbeFindVertexField(xRes, "m_uVertexID");
	const Flux_SlangProbeResult::VertexInputField* pxInstanceID = ProbeFindVertexField(xRes, "m_uInstanceID");
	ZENITH_ASSERT_TRUE(pxVertexID && pxInstanceID, "V3: SV_* fields must still be reflected (they are dropped, not hidden)");
	if (!(pxVertexID && pxInstanceID)) return;

	ZENITH_ASSERT_FALSE(pxVertexID->m_bHasVaryingInput, "V3: SV_VertexID must take no VaryingInput category");
	ZENITH_ASSERT_FALSE(pxInstanceID->m_bHasVaryingInput, "V3: SV_InstanceID must take no VaryingInput category");
	ZENITH_ASSERT_EQ(pxVertexID->m_uCategoryCount, 0u, "V3: a system value occupies no parameter category at all");
	ZENITH_ASSERT_EQ(pxInstanceID->m_uCategoryCount, 0u, "V3: a system value occupies no parameter category at all");
	ZENITH_ASSERT_TRUE(pxVertexID->m_bSemanticIsSV, "V3: the semantic name corroborates ('SV_VERTEXID')");
	ZENITH_ASSERT_STREQ(pxVertexID->m_strSemanticName.c_str(), "SV_VERTEXID", "V3: Slang upper-cases the system-value semantic");

	// The six real vertex attributes must be exactly the non-SV fields, densely numbered.
	u_int uVaryingCount = 0;
	for (u_int i = 0; i < xRes.m_axVertexInputs.GetSize(); i++)
	{
		if (xRes.m_axVertexInputs.Get(i).m_bHasVaryingInput) uVaryingCount++;
	}
	ZENITH_ASSERT_EQ(uVaryingCount, 6u, "V3: exactly the six non-SV fields consume vertex-input locations");
	ZENITH_ASSERT_EQ(xRes.m_axVertexInputs.GetSize(), 8u, "V3: all eight declared fields are reflected");
}

// --- V4: a zero-argument attribute survives link too — the [PerInstance] tag a vertex-layout
//         generator would use to split per-vertex from per-instance streams. Separate from V1
//         because an attribute with no arguments takes a different path through Slang's
//         attribute checking than one carrying a string. -----------------------------------
ZENITH_TEST(SlangProbes, V4_PerInstanceAttributeVisible)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V4: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_SlangProbeResult::VertexInputField* pxInst = ProbeFindVertexField(xRes, "m_xInstRow");
	ZENITH_ASSERT_NOT_NULL(pxInst, "V4: the [PerInstance]-annotated field must appear in entry-point reflection");
	if (!pxInst) return;

	const Flux_SlangProbeResult::VertexAttribute* pxAttrib = ProbeFindVertexAttribute(*pxInst, "PerInstance");
	ZENITH_ASSERT_NOT_NULL(pxAttrib, "V4: [PerInstance] must be readable POST-LINK (%u attribute(s) seen on the field)",
		pxInst->m_axAttributes.GetSize());
	if (!pxAttrib) return;

	ZENITH_ASSERT_FALSE(pxAttrib->m_bHasArg0, "V4: [PerInstance] declares no parameters, so there is no argument to read");
	ZENITH_ASSERT_TRUE(pxAttrib->m_bFoundByName, "V4: findAttributeByName must resolve a zero-argument attribute too");

	// The tag is orthogonal to the layout: an annotated field is still an ordinary varying input.
	ZENITH_ASSERT_TRUE(pxInst->m_bHasVaryingInput, "V4: annotating a field must not change its vertex-input category");

	// A field with no attributes must come back empty rather than inheriting its neighbours'.
	const Flux_SlangProbeResult::VertexInputField* pxPos = ProbeFindVertexField(xRes, "m_xPos");
	if (pxPos) ZENITH_ASSERT_EQ(pxPos->m_axAttributes.GetSize(), 0u, "V4: an unannotated field carries no attributes");
}

// --- V5: format inference inputs. TypeReflection answers the scalar kind and lane count for
//         every field, which is what a VkFormat mapping keys on — including telling half4 from
//         float4, the distinction [VtxFmt] exists to override. ------------------------------
ZENITH_TEST(SlangProbes, V5_TypeReflectionDrivesFormatInference)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V5: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_SlangProbeResult::VertexInputField* pxPos      = ProbeFindVertexField(xRes, "m_xPos");
	const Flux_SlangProbeResult::VertexInputField* pxUV0      = ProbeFindVertexField(xRes, "m_xUV0");
	const Flux_SlangProbeResult::VertexInputField* pxTangent  = ProbeFindVertexField(xRes, "m_xTangent");
	const Flux_SlangProbeResult::VertexInputField* pxColour   = ProbeFindVertexField(xRes, "m_xColour");
	const Flux_SlangProbeResult::VertexInputField* pxVertexID = ProbeFindVertexField(xRes, "m_uVertexID");
	ZENITH_ASSERT_TRUE(pxPos && pxUV0 && pxTangent && pxColour && pxVertexID, "V5: every probed field must be reflected");
	if (!(pxPos && pxUV0 && pxTangent && pxColour && pxVertexID)) return;

	ZENITH_ASSERT_STREQ(pxPos->m_strTypeKind.c_str(), "vector", "V5: float3 reflects as a vector");
	ZENITH_ASSERT_STREQ(pxPos->m_strScalarType.c_str(), "float32", "V5: float3 element scalar is float32");
	ZENITH_ASSERT_EQ(pxPos->m_uElementCount, 3u, "V5: float3 has 3 lanes");
	ZENITH_ASSERT_EQ(pxUV0->m_uElementCount, 2u, "V5: float2 has 2 lanes");
	ZENITH_ASSERT_EQ(pxTangent->m_uElementCount, 4u, "V5: float4 has 4 lanes");

	// half4 vs float4 is visible in the element scalar, not the lane count — so inference can
	// pick a 16-bit VkFormat without [VtxFmt], and [VtxFmt] stays an OVERRIDE rather than the
	// only source of precision (m_xTangent is a float4 the shader wants fed as half4).
	ZENITH_ASSERT_STREQ(pxColour->m_strScalarType.c_str(), "float16", "V5: half4 element scalar is float16");
	ZENITH_ASSERT_EQ(pxColour->m_uElementCount, 4u, "V5: half4 has 4 lanes");
	ZENITH_ASSERT_STREQ(pxTangent->m_strScalarType.c_str(), "float32", "V5: the [VtxFmt(\"half4\")] field is declared float4");

	ZENITH_ASSERT_STREQ(pxVertexID->m_strTypeKind.c_str(), "scalar", "V5: uint reflects as a scalar");
	ZENITH_ASSERT_STREQ(pxVertexID->m_strScalarType.c_str(), "uint32", "V5: uint scalar type is uint32");
	ZENITH_ASSERT_EQ(pxVertexID->m_uElementCount, 1u, "V5: a scalar counts as one lane");

	for (u_int i = 0; i < xRes.m_axVertexInputs.GetSize(); i++)
	{
		const Flux_SlangProbeResult::VertexInputField& xField = xRes.m_axVertexInputs.Get(i);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[SlangProbe V5]   '%s' sem=%s[%u] varying=%d loc=%u kind=%s scalar=%s lanes=%u rows=%u cols=%u attribs=%u",
			xField.m_strName.c_str(), xField.m_strSemanticName.c_str(), xField.m_uSemanticIndex,
			xField.m_bHasVaryingInput ? 1 : 0, xField.m_uVaryingLocation, xField.m_strTypeKind.c_str(),
			xField.m_strScalarType.c_str(), xField.m_uElementCount, xField.m_uRowCount, xField.m_uColumnCount,
			xField.m_axAttributes.GetSize());
	}
}

// --- V6: the PRODUCTION extractor (ExtractVertexInputs), run over the same linked program
//         V1-V5 inspect. Everything V1-V5 established as possible is here asserted as done:
//         the two SV_* fields are gone, the [VtxFmt("half4")] override beats the declared
//         float4, an un-annotated `half4` infers to HALF4 on its own, [PerInstance] lands in
//         binding 1 with its own offset origin, and the byte offsets are the tight-packed
//         running sum. This is the only place the whole chain runs against real Slang. ----
ZENITH_TEST(SlangProbes, V6_ExtractedVertexTable)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeVertexSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V6: the vertex probe must compile. Diag: %s", xRes.m_strDiagnostics.c_str());

	const Flux_ShaderReflection& xRefl = xRes.m_xReflection;
	const Zenith_Vector<Flux_ReflectedVertexAttribute>& axAttribs = xRefl.GetVertexAttributes();
	ZENITH_ASSERT_EQ(axAttribs.GetSize(), 6u, "V6: the two SV_* fields are dropped, the six real ones kept");
	if (axAttribs.GetSize() != 6u) return;

	// Declaration order is preserved, which is also location order.
	ZENITH_ASSERT_STREQ(axAttribs.Get(0).m_strName.c_str(), "m_xPos", "V6: the table keeps declaration order");
	ZENITH_ASSERT_STREQ(axAttribs.Get(5).m_strName.c_str(), "m_xInstRow", "V6: the per-instance field is last, as declared");
	ZENITH_ASSERT_NULL(ProbeFindExtractedAttribute(xRefl, "m_uVertexID"), "V6: SV_VertexID must not reach the table");
	ZENITH_ASSERT_NULL(ProbeFindExtractedAttribute(xRefl, "m_uInstanceID"), "V6: SV_InstanceID must not reach the table");

	const Flux_ReflectedVertexAttribute* pxPos     = ProbeFindExtractedAttribute(xRefl, "m_xPos");
	const Flux_ReflectedVertexAttribute* pxUV1     = ProbeFindExtractedAttribute(xRefl, "m_xUV1");
	const Flux_ReflectedVertexAttribute* pxTangent = ProbeFindExtractedAttribute(xRefl, "m_xTangent");
	const Flux_ReflectedVertexAttribute* pxColour  = ProbeFindExtractedAttribute(xRefl, "m_xColour");
	const Flux_ReflectedVertexAttribute* pxInst    = ProbeFindExtractedAttribute(xRefl, "m_xInstRow");
	ZENITH_ASSERT_TRUE(pxPos && pxUV1 && pxTangent && pxColour && pxInst, "V6: every non-SV field must be extracted");
	if (!(pxPos && pxUV1 && pxTangent && pxColour && pxInst)) return;

	ZENITH_ASSERT_EQ((int)pxPos->m_eType, (int)SHADER_DATA_TYPE_FLOAT3, "V6: an un-annotated float3 infers to FLOAT3");
	ZENITH_ASSERT_EQ((int)pxTangent->m_eType, (int)SHADER_DATA_TYPE_HALF4,
		"V6: [VtxFmt(\"half4\")] OVERRIDES the declared float4 storage");
	ZENITH_ASSERT_EQ((int)pxColour->m_eType, (int)SHADER_DATA_TYPE_HALF4,
		"V6: a declared half4 infers to HALF4 without any annotation");

	ZENITH_ASSERT_STREQ(pxUV1->m_strSemantic.c_str(), "TEXCOORD", "V6: the semantic base name is carried through");
	ZENITH_ASSERT_EQ(pxUV1->m_uSemanticIndex, 1u, "V6: the trailing digit is carried through as the index");
	ZENITH_ASSERT_EQ(pxUV1->m_uLocation, 2u, "V6: locations are Slang's, dense over the non-SV fields");

	// Tight-packed bytes: float3(12) + float2(8) + float2(8) + half4(8) + half4(8) = 44.
	ZENITH_ASSERT_EQ(pxPos->m_uBinding, 0u, "V6: an un-annotated field is per-vertex");
	ZENITH_ASSERT_EQ(pxPos->m_uOffset, 0u, "V6: the first per-vertex attribute starts at byte 0");
	ZENITH_ASSERT_EQ(pxUV1->m_uOffset, 20u, "V6: offsets are the running byte sum (12 + 8)");
	ZENITH_ASSERT_EQ(pxTangent->m_uOffset, 28u, "V6: the OVERRIDDEN storage size (8, not 16) drives the next offset");
	ZENITH_ASSERT_EQ(pxColour->m_uOffset, 36u, "V6: half4 advances the cursor by 8");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(0u), 44u, "V6: per-vertex stride is the packed total");

	ZENITH_ASSERT_EQ(pxInst->m_uBinding, 1u, "V6: [PerInstance] moves the attribute to the instance-rate binding");
	ZENITH_ASSERT_EQ(pxInst->m_uOffset, 0u, "V6: binding 1 packs from 0, independently of binding 0");
	ZENITH_ASSERT_EQ(xRefl.GetVertexStride(1u), 16u, "V6: per-instance stride is the one float4");

	// Dump the extracted table (same idiom as V5's raw dump) so a run's log shows what the
	// production extractor actually produced, not just that the assertions held.
	for (u_int i = 0; i < axAttribs.GetSize(); i++)
	{
		const Flux_ReflectedVertexAttribute& xAttribute = axAttribs.Get(i);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SlangProbe V6]   '%s' sem=%s[%u] loc=%u type=%d binding=%u offset=%u",
			xAttribute.m_strName.c_str(), xAttribute.m_strSemantic.c_str(), xAttribute.m_uSemanticIndex,
			xAttribute.m_uLocation, static_cast<int>(xAttribute.m_eType), xAttribute.m_uBinding, xAttribute.m_uOffset);
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST, "[SlangProbe V6] strides: perVertex=%u perInstance=%u",
		xRefl.GetVertexStride(0u), xRefl.GetVertexStride(1u));
}

// --- V7: the hard-error contract. A [VtxFmt] string outside the vocabulary must FAIL rather
//         than fall back to the declared type — a silent fallback would bake a wrong stride
//         into every buffer the exporter writes. The message has to name the field, because
//         the compile it fails is a whole shader tree. ------------------------------------
ZENITH_TEST(SlangProbes, V7_UnknownVertexFormatIsRejected)
{
	Flux_SlangProbeResult xRes;
	Flux_SlangCompiler::CompileVertexProbeFromSource(kszProbeBadVertexFormatSource, "vsMain", xRes);
	ZENITH_ASSERT_TRUE(xRes.m_bCompiled, "V7: the SHADER itself is valid Slang — only the format string is wrong. Diag: %s",
		xRes.m_strDiagnostics.c_str());

	// The probe surfaces the extractor's rejection as a diagnostic (it never asserts).
	ZENITH_ASSERT_TRUE(xRes.m_strDiagnostics.find("m_xNormal") != std::string::npos,
		"V7: the rejection must name the offending field. Diag: %s", xRes.m_strDiagnostics.c_str());
	ZENITH_ASSERT_TRUE(xRes.m_strDiagnostics.find("half3") != std::string::npos,
		"V7: the rejection must quote the unknown format string. Diag: %s", xRes.m_strDiagnostics.c_str());

	// And no stride was derived from the half-built table — the extractor bails before packing.
	ZENITH_ASSERT_EQ(xRes.m_xReflection.GetVertexStride(0u), 0u, "V7: a rejected extraction leaves no packed stride");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[SlangProbe V7] rejection diagnostic: %s", xRes.m_strDiagnostics.c_str());
}

#endif // ZENITH_WINDOWS && ZENITH_VULKAN
