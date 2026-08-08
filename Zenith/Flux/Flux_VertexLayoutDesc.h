#pragma once

#include "Core/Zenith_VertexAttributeTypes.h"

#include <cstring>   // strcmp — the semantic-name vocabulary lookup

// ============================================================================
// Flux_VertexLayoutDesc — the constexpr description of a program's vertex input.
//
// A pure POD leaf, deliberately dependency-light: the AUTO-GENERATED
// Generated/<Subsystem>.h headers include it to declare their baked
// kaxVertexAttribs[] / kVertexLayout constants, pipeline construction reads
// those constants, and a Core-visible TU static_asserts one of them against
// Zenith_TerrainChunkLayout. Anything heavier here (the Slang compiler header,
// Zenith_Vector, the renderer types) would drag that whole surface along with
// it, so this header takes exactly ONE engine include — ShaderDataType, which is
// the storage-format vocabulary the descriptors are written in.
//
// WHY IT IS NOT Flux_ReflectedVertexAttribute: that type is the COMPILE-TIME
// reflection record — std::string names, varying locations, the Slang-facing
// fields. This one is the BAKED artifact: trivially comparable, allocation-free,
// usable in a constant expression, and stable enough to be a committed header
// humans read and diff.
// ============================================================================

// One reflected vertex-input SEMANTIC, as Slang reports it: the base name with the
// trailing digit split off and the name upper-cased ("TEXCOORD1" -> {TEXCOORD, 1}).
//
// The set is CLOSED against the shader corpus — these are the six semantics every
// VsIn in Zenith/Flux/Shaders uses (the games declare no shaders of their own).
// Codegen HARD-ERRORS on a semantic outside this list rather than inventing a tag,
// so introducing one means adding an enumerator here AND a row in the table below,
// in the same change as the shader that declares it.
//
// UNKNOWN exists only so a default-constructed element, and a live-reflection table
// whose name failed to resolve, have a value to hold; it can never appear in a
// generated table, because the codegen run that would have emitted it fails first.
enum FluxVertexSemantic
{
	FLUX_VERTEX_SEMANTIC_UNKNOWN,
	FLUX_VERTEX_SEMANTIC_POSITION,
	FLUX_VERTEX_SEMANTIC_TEXCOORD,
	FLUX_VERTEX_SEMANTIC_NORMAL,
	FLUX_VERTEX_SEMANTIC_TANGENT,
	FLUX_VERTEX_SEMANTIC_BINORMAL,
	FLUX_VERTEX_SEMANTIC_COLOR,
};

// Number of vertex bindings a layout can split its attributes across: binding 0 is
// the per-vertex stream, binding 1 the per-instance stream a [PerInstance]-tagged
// field lands in. MUST equal uFLUX_MAX_VERTEX_BINDINGS in
// Flux/Slang/Flux_SlangCompiler.h (the reflection side of the same model);
// Flux_CodeGenerator.cpp includes both and static_asserts them equal, which is the
// only TU positioned to.
inline constexpr u_int uFLUX_VERTEX_LAYOUT_MAX_BINDINGS = 2;

// One attribute of a baked vertex layout. m_eType is the STORAGE format in the
// buffer, NOT the shader's declared field type — fetch hardware widens/converts, so
// a [VtxFmt("snorm10_10_10_2")] field declared `float4` is four bytes here. m_uOffset
// is a BYTE offset within its binding's tight-packed vertex.
struct Flux_VertexLayoutElement
{
	FluxVertexSemantic m_eSemantic      = FLUX_VERTEX_SEMANTIC_UNKNOWN;
	u_int              m_uSemanticIndex = 0;
	ShaderDataType     m_eType          = SHADER_DATA_TYPE_NONE;
	u_int              m_uBinding       = 0;
	u_int              m_uOffset        = 0;

	// Defaulted rather than hand-written so a field added above is compared without
	// a second edit — the whole point of the type is that two layouts either agree
	// in every respect or the tripwire fires.
	constexpr bool operator==(const Flux_VertexLayoutElement& xOther) const = default;
};

// A whole program's vertex input: the element table plus each binding's tight-packed
// byte stride. The elements are held BY POINTER because the generated headers emit
// them as a separate `inline constexpr` array per program — one definition, shared by
// every reference, with no per-consumer copy of a fixed-capacity buffer.
//
// The canonical EMPTY layout (a compute program, or a vertex-pulling / fullscreen
// program with no vertex inputs) is exactly { nullptr, 0u, { 0u, 0u } }. Codegen emits
// that one spelling and no other, so equality against a default-constructed desc is
// the "this program takes no vertex buffer" question.
struct Flux_VertexLayoutDesc
{
	const Flux_VertexLayoutElement* m_paxElements   = nullptr;
	u_int                           m_uElementCount = 0;
	u_int                           m_auStrides[uFLUX_VERTEX_LAYOUT_MAX_BINDINGS] = { 0u, 0u };

	// ELEMENT-WISE, never pointer-wise: the tripwire compares a baked table against
	// one built at runtime from live reflection, and those are two different arrays
	// holding the same values. A defaulted operator== would compare m_paxElements as
	// an address and report every such pair unequal.
	constexpr bool operator==(const Flux_VertexLayoutDesc& xOther) const
	{
		if (m_uElementCount != xOther.m_uElementCount) return false;
		for (u_int b = 0; b < uFLUX_VERTEX_LAYOUT_MAX_BINDINGS; b++)
		{
			if (m_auStrides[b] != xOther.m_auStrides[b]) return false;
		}
		for (u_int u = 0; u < m_uElementCount; u++)
		{
			if (!(m_paxElements[u] == xOther.m_paxElements[u])) return false;
		}
		return true;
	}
};

// THE semantic vocabulary, as ONE table with both projections on each row: the name
// Slang reports, and the enumerator identifier a generated header spells. Two
// separate lists would let a newly added semantic parse correctly and then emit as
// FLUX_VERTEX_SEMANTIC_UNKNOWN into a committed header.
struct Flux_VertexSemanticEntry
{
	const char*        m_szSemanticName;
	const char*        m_szEnumToken;
	FluxVertexSemantic m_eSemantic;
};

inline constexpr Flux_VertexSemanticEntry kaxFLUX_VERTEX_SEMANTICS[] =
{
	{ "POSITION", "FLUX_VERTEX_SEMANTIC_POSITION", FLUX_VERTEX_SEMANTIC_POSITION },
	{ "TEXCOORD", "FLUX_VERTEX_SEMANTIC_TEXCOORD", FLUX_VERTEX_SEMANTIC_TEXCOORD },
	{ "NORMAL",   "FLUX_VERTEX_SEMANTIC_NORMAL",   FLUX_VERTEX_SEMANTIC_NORMAL   },
	{ "TANGENT",  "FLUX_VERTEX_SEMANTIC_TANGENT",  FLUX_VERTEX_SEMANTIC_TANGENT  },
	{ "BINORMAL", "FLUX_VERTEX_SEMANTIC_BINORMAL", FLUX_VERTEX_SEMANTIC_BINORMAL },
	{ "COLOR",    "FLUX_VERTEX_SEMANTIC_COLOR",    FLUX_VERTEX_SEMANTIC_COLOR    },
};

// Semantic-name string -> tag. Codegen calls it to bake a generated table (failing
// the compile when it returns false), and the runtime tripwire calls it to build the
// comparison table from live reflection, so the two sides can never disagree about
// what "BINORMAL" means. szName must already be the upper-cased base form with the
// index split off, which is how Slang reports it.
inline bool Flux_ParseVertexSemantic(const char* szName, FluxVertexSemantic& eSemanticOut)
{
	if (!szName) return false;
	for (const Flux_VertexSemanticEntry& xEntry : kaxFLUX_VERTEX_SEMANTICS)
	{
		if (strcmp(szName, xEntry.m_szSemanticName) == 0)
		{
			eSemanticOut = xEntry.m_eSemantic;
			return true;
		}
	}
	return false;
}

// The FLUX_VERTEX_SEMANTIC_* identifier for a tag. Generated tables spell
// enumerators, not ordinals — they are committed artifacts a human reads and diffs.
inline const char* Flux_VertexSemanticEnumToken(FluxVertexSemantic eSemantic)
{
	for (const Flux_VertexSemanticEntry& xEntry : kaxFLUX_VERTEX_SEMANTICS)
	{
		if (xEntry.m_eSemantic == eSemantic) return xEntry.m_szEnumToken;
	}
	return "FLUX_VERTEX_SEMANTIC_UNKNOWN";
}
