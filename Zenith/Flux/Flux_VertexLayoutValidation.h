#pragma once

#include "Flux/Flux_VertexLayoutDesc.h"        // Flux_VertexLayoutDesc + the semantic vocabulary
#include "Flux/Slang/Flux_SlangCompiler.h"     // Flux_ShaderReflection (the LIVE vertex-input table)

// ============================================================================
// Flux_VertexLayoutValidation — the stale-codegen tripwire.
//
// A graphics pipeline's vertex input is built from the LIVE shader reflection
// (Flux_ShaderReflection::GetVertexAttributes / GetVertexStride), never from a
// hand-written CPU table. The pipeline SPEC still names the layout it expects —
// the `inline constexpr kVertexLayout` its program's Generated/<Subsystem>.h
// baked — and this header is what compares the two at pipeline-build time.
//
// WHAT IT ACTUALLY CATCHES depends on how the shader reached the backend:
//   * Windows runtime-compile (the Vulkan tools build): the reflection came from
//     Slang parsing the .slang THIS RUN, so a mismatch means the checked-in
//     Generated/ header is STALE relative to the shader on disk.
//   * Artifact load (Null / D3D12 / Android): the reflection came from the
//     committed .spv.refl sidecar, so a mismatch means the sidecar and the
//     Generated/ header were committed out of step with each other.
// Both have the same fix, which is why the assert says one thing: RERUN
// FLUXCOMPILER. Nothing here ever repairs the mismatch — the layout the GPU
// fetches is always the live reflection, so a stale generated table is caught,
// reported, and otherwise inert.
//
// The match is deliberately EXACT (semantic, semantic index, storage format,
// binding, byte offset, and BOTH binding strides). A vertex layout that is
// "nearly" right reads adjacent bytes as geometry, which does not fail — it
// renders garbage — so there is no useful tolerance to allow.
//
// Semantic names are resolved through Flux_ParseVertexSemantic, the SAME
// vocabulary codegen bakes with, so the two sides can never disagree about what
// "BINORMAL" means. A live name that fails to resolve is itself a mismatch: it
// cannot correspond to anything a generated table is allowed to hold.
// ============================================================================

enum FluxVertexLayoutMatch
{
	FLUX_VERTEX_LAYOUT_MATCH_OK,
	FLUX_VERTEX_LAYOUT_MATCH_COUNT,       // attribute counts differ
	FLUX_VERTEX_LAYOUT_MATCH_STRIDE,      // a binding's tight-packed stride differs
	FLUX_VERTEX_LAYOUT_MATCH_ELEMENT,     // an attribute's semantic/type/binding/offset differs
	FLUX_VERTEX_LAYOUT_MATCH_SEMANTIC,    // a live semantic name is outside the closed vocabulary
};

// Where a mismatch was found: an element index for ELEMENT/SEMANTIC, a binding
// index for STRIDE, meaningless for COUNT. Carried so the assert can name the
// offending row instead of just "the layouts differ".
struct Flux_VertexLayoutMatchResult
{
	FluxVertexLayoutMatch m_eMatch = FLUX_VERTEX_LAYOUT_MATCH_OK;
	u_int                 m_uIndex = 0;

	bool IsMatch() const { return m_eMatch == FLUX_VERTEX_LAYOUT_MATCH_OK; }
};

// THE pure comparison. No allocation, no logging, no asserts — callable from a
// unit test with a hand-built reflection.
//
// pxExpected == nullptr is the "this pipeline declares no vertex input" spelling
// and matches an empty reflection table. The canonical empty generated desc
// ({ nullptr, 0u, { 0u, 0u} }) matches it too, so both spellings of "no vertex
// input" are accepted — a caller may not, however, mix a null pointer with a
// program that actually fetches attributes.
inline Flux_VertexLayoutMatchResult Flux_MatchVertexLayoutToReflection(
	const Flux_ShaderReflection& xReflection,
	const Flux_VertexLayoutDesc* pxExpected)
{
	Flux_VertexLayoutMatchResult xResult;

	const Zenith_Vector<Flux_ReflectedVertexAttribute>& xLive = xReflection.GetVertexAttributes();
	const u_int uLiveCount     = xLive.GetSize();
	const u_int uExpectedCount = pxExpected ? pxExpected->m_uElementCount : 0u;

	if (uLiveCount != uExpectedCount)
	{
		xResult.m_eMatch = FLUX_VERTEX_LAYOUT_MATCH_COUNT;
		return xResult;
	}

	for (u_int b = 0; b < uFLUX_VERTEX_LAYOUT_MAX_BINDINGS; b++)
	{
		const u_int uExpectedStride = pxExpected ? pxExpected->m_auStrides[b] : 0u;
		if (xReflection.GetVertexStride(b) != uExpectedStride)
		{
			xResult.m_eMatch = FLUX_VERTEX_LAYOUT_MATCH_STRIDE;
			xResult.m_uIndex = b;
			return xResult;
		}
	}

	for (u_int u = 0; u < uLiveCount; u++)
	{
		const Flux_ReflectedVertexAttribute& xAttribute = xLive.Get(u);

		FluxVertexSemantic eSemantic = FLUX_VERTEX_SEMANTIC_UNKNOWN;
		if (!Flux_ParseVertexSemantic(xAttribute.m_strSemantic.c_str(), eSemantic))
		{
			xResult.m_eMatch = FLUX_VERTEX_LAYOUT_MATCH_SEMANTIC;
			xResult.m_uIndex = u;
			return xResult;
		}

		const Flux_VertexLayoutElement xLiveElement
		{
			eSemantic,
			xAttribute.m_uSemanticIndex,
			xAttribute.m_eType,
			xAttribute.m_uBinding,
			xAttribute.m_uOffset,
		};

		if (!(xLiveElement == pxExpected->m_paxElements[u]))
		{
			xResult.m_eMatch = FLUX_VERTEX_LAYOUT_MATCH_ELEMENT;
			xResult.m_uIndex = u;
			return xResult;
		}
	}

	return xResult;
}

// The assert wrapper every backend's FromSpecification calls. Reports WHICH
// program, WHAT differs and WHAT TO DO — a stale generated header is fixed by
// one FluxCompiler run, and no amount of staring at the pipeline spec reveals
// that. szProgramName is the shader's catalog decl name (static string).
inline void Flux_AssertVertexLayoutMatchesReflection(
	const Flux_ShaderReflection& xReflection,
	const Flux_VertexLayoutDesc* pxExpected,
	const char* szProgramName)
{
	const Flux_VertexLayoutMatchResult xResult = Flux_MatchVertexLayoutToReflection(xReflection, pxExpected);
	if (xResult.IsMatch())
	{
		return;
	}

	const u_int uLiveCount     = xReflection.GetVertexAttributes().GetSize();
	const u_int uExpectedCount = pxExpected ? pxExpected->m_uElementCount : 0u;

	Zenith_Assert(false,
		"Vertex layout mismatch in program '%s' (kind %u at index %u): the pipeline spec's generated layout has %u attribute(s) "
		"[strides %u/%u] but the shader's live reflection has %u [strides %u/%u]. "
		"The GENERATED header is stale — RERUN FLUXCOMPILER (Vulkan_vs2022_Release_Win64_True) and commit the regenerated tree.",
		szProgramName ? szProgramName : "<unknown>",
		static_cast<u_int>(xResult.m_eMatch), xResult.m_uIndex,
		uExpectedCount,
		pxExpected ? pxExpected->m_auStrides[0] : 0u,
		pxExpected ? pxExpected->m_auStrides[1] : 0u,
		uLiveCount,
		xReflection.GetVertexStride(0u),
		xReflection.GetVertexStride(1u));
}

// Spec-taking adapter, defined out of line in Flux_VertexLayoutValidation.cpp.
//
// WHY OUT OF LINE: the Null and D3D12 pipeline headers see Flux_PipelineSpecification
// only as a forward declaration (Flux_Fwd.h) — including Flux_Pipeline.h from them
// would close a cycle through the Flux_* backend aliases. Passing the incomplete
// type straight through by reference is legal, so their FromSpecification stubs stay
// one-liners and the member access happens in a Flux TU that has the whole type.
struct Flux_PipelineSpecification;
void Flux_ValidateVertexLayoutForSpec(const Flux_PipelineSpecification& xSpec);
