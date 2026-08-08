#pragma once

#include "Flux/Flux_VertexLayoutDesc.h"
#include "Flux/Flux_VertexCodec.h"

// ============================================================================
// Flux_VertexPacker — THE generic interleaver: semantic-keyed SoA sources in,
// one layout-shaped interleaved vertex buffer out.
//
// It exists to replace the four hand-written interleave loops (the two
// Flux_MeshInstance::Create*FromAsset paths, Flux_RealBuildSkinnedPose, and the
// terrain exporter's chunk writer), each of which hard-codes a byte layout that
// has to agree with a shader's VsIn by convention alone. Here the LAYOUT is the
// authority: the caller hands over a `Flux_VertexLayoutDesc` — in production
// always a generated `Flux_Generated_<Subsystem>::<Program>::kVertexLayout`,
// which is reflected straight out of the shader — and the packer writes exactly
// the elements that table declares, at exactly the offsets and stride it names.
// A layout edit therefore moves the writer and the reader together, and a
// hand-maintained offset can no longer drift from the shader that reads it.
//
// PURE + ALLOCATION-FREE: no device, no engine singleton reach, no heap traffic
// in the pack loop. It runs in the offline exporters, the editor's live-edit
// hooks and the headless unit suite on the same code path, exactly as
// Flux_VertexCodec (which owns every quantisation it performs) does.
//
// FLOAT DISCIPLINE: lanes written by the FLOAT family are copied verbatim, so
// they may be compared bit-exactly against their source. Lanes written by a
// PACKED format go through the codec, whose WORDS are exact — compare those as
// integers, never the floats they decode to.
// ============================================================================

// One source attribute stream, keyed by the semantic it feeds.
//
// The data is SoA and TIGHT: vertex v's lanes live at
// `m_pfData[v * m_uLaneCount .. v * m_uLaneCount + m_uLaneCount - 1]`. That is
// the shape every existing CPU attribute array already has (a
// `Zenith_Vector<Vector3>` is a tight float3 stream), so a caller builds a view
// by pointing at the vector's data and naming its lane count — no repacking.
//
// A view is either COMPLETE or ABSENT: the packer trusts a supplied view to
// carry at least `uVertexCount` vertices and does not range-check it. Deciding
// that is the caller's job, and it is the same decision the loops being
// replaced already make (`m_xUVs.GetSize() >= uNumVerts`) — a source that is
// too short must simply not be passed, which routes the attribute onto its
// canonical default below. A view with a null pointer or zero lanes is treated
// as absent, so a caller may build a fixed-size array and leave slots blank.
struct Flux_VertexSourceView
{
	FluxVertexSemantic m_eSemantic      = FLUX_VERTEX_SEMANTIC_UNKNOWN;
	u_int              m_uSemanticIndex = 0;
	const float*       m_pfData         = nullptr;
	u_int              m_uLaneCount     = 0;
};

// The value an attribute takes when its source is absent — and, lane by lane,
// the value any lane a PRESENT source does not reach takes as well. One rule
// covers both cases: EVERY lane the source does not supply comes from here.
//
// The xyz values are the defaults the interleave loops being replaced already
// write (uv 0, normal +Y, tangent +X, bitangent +Z, colour white), so swapping a
// caller onto the packer cannot change what a mesh with a missing attribute
// renders as. The w values are the "short source" pad rule: 1 for POSITION and
// COLOR (a homogeneous point and an opaque alpha), 0 for everything else —
// which is also what a 4-lane TANGENT falls back to when no BINORMAL source is
// present to derive a real handedness sign from.
inline Zenith_Maths::Vector4 Flux_CanonicalVertexDefault(FluxVertexSemantic eSemantic)
{
	switch (eSemantic)
	{
	case FLUX_VERTEX_SEMANTIC_POSITION: return Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	case FLUX_VERTEX_SEMANTIC_TEXCOORD: return Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	case FLUX_VERTEX_SEMANTIC_NORMAL:   return Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
	case FLUX_VERTEX_SEMANTIC_TANGENT:  return Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
	case FLUX_VERTEX_SEMANTIC_BINORMAL: return Zenith_Maths::Vector4(0.0f, 0.0f, 1.0f, 0.0f);
	case FLUX_VERTEX_SEMANTIC_COLOR:    return Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	case FLUX_VERTEX_SEMANTIC_UNKNOWN:  break;
	}
	return Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
}

// How many lanes a STORAGE format consumes, and — by returning 0 — whether the
// packer can write it at all.
//
// This is deliberately NOT Flux_BufferElement::GetComponentCount: that one
// answers "how many components does this type have" for every ShaderDataType
// including MAT4 and BOOL, whereas this one answers "how many source lanes does
// the packer read, and can it write this". The supported set is exactly the
// FLOAT family plus the formats Flux_VertexCodec owns a packer for; anything
// else — the integer families, the matrix types, and the three packed formats
// the codec has no function for (SNORM16X2 / UNORM16X4 / UINT16X4) — returns 0
// and is refused rather than silently written wrong.
inline u_int Flux_VertexFormatLaneCount(ShaderDataType eType)
{
	switch (eType)
	{
	case SHADER_DATA_TYPE_FLOAT:             return 1u;
	case SHADER_DATA_TYPE_FLOAT2:            return 2u;
	case SHADER_DATA_TYPE_FLOAT3:            return 3u;
	case SHADER_DATA_TYPE_FLOAT4:            return 4u;
	case SHADER_DATA_TYPE_HALF2:             return 2u;
	case SHADER_DATA_TYPE_HALF4:             return 4u;
	case SHADER_DATA_TYPE_SNORM16X4:         return 4u;
	case SHADER_DATA_TYPE_UNORM16X2:         return 2u;
	case SHADER_DATA_TYPE_UNORM8X4:          return 4u;
	case SHADER_DATA_TYPE_UINT8X4:           return 4u;
	case SHADER_DATA_TYPE_SNORM10_10_10_2:   return 4u;
	default:                                 return 0u;
	}
}

// Write `uVertexCount` interleaved vertices into `pDst`, shaped by `xLayout`.
//
// BINDING 0 ONLY. A layout may split its attributes across two bindings — 0 is
// the per-vertex stream, 1 the per-instance stream a [PerInstance] field lands
// in — and those two streams have different element counts, different strides
// and different producers. Elements on any binding other than 0 are SKIPPED, so
// passing a two-binding layout packs its per-vertex stream correctly and leaves
// the instance stream to whoever owns the instance data.
//
// pxPosQuant, when non-null, is applied to SNORM16X4 elements carrying POSITION
// semantics and to nothing else: it is the dequant box that maps authored
// positions onto [-1,1] before quantisation, so it only means anything for a
// position. Every other element ignores it, and a null box leaves even a
// SNORM16X4 position on the plain (clamped to [-1,1]) codec path.
//
// A vertex count of 0, an empty layout, and a null destination are all no-ops.
void Flux_PackVertices(
	void* pDst,
	const Flux_VertexLayoutDesc& xLayout,
	const Flux_VertexSourceView* paxSources,
	u_int uSourceCount,
	u_int uVertexCount,
	const Flux_PosQuant* pxPosQuant = nullptr);
