// =====================================================================
// Common/StaticMeshVertex — THE interleaved static-mesh vertex input.
//
// ONE declaration of the 24-byte static-mesh VsIn, fetched by FOUR programs:
// Flux_UnifiedMesh_ToGBuffer / _ToGBufferVelocity / _ToShadowmap (which reach it
// through Common/UnifiedMeshDraw.slang) and Flux_Translucent_Forward (which
// includes this file directly). All four draw the SAME vertex buffers — the
// interleaved stream Flux_PackVertices writes through
// Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout — so their four
// generated layouts have to be the same table. Until this file existed they were
// two hand-kept copies of one struct (the unified family's, and the translucent
// pass's), and only a C++ cross-pin (Flux_Translucency.cpp) could catch a drift
// AFTER reflection had already baked two different tables.
//
// It is therefore also the single edit site for a STORAGE-format change: a
// [VtxFmt(...)] tag added to a field here moves all four programs' baked layouts
// — and so the one packer that writes the buffer — in lockstep, instead of
// needing the same tag applied identically in two places.
//
// THE STORAGE FORMATS (the fields stay float — fetch hardware widens and converts):
//   position  half4              8 B @ 0   (w unused; the fetch drops the extra lane)
//   uv        half2              4 B @ 8
//   normal    snorm10_10_10_2    4 B @ 12  (unit direction; w unused)
//   tangent   snorm10_10_10_2    4 B @ 16  (unit direction; w = BITANGENT SIGN)
//   colour    unorm8x4           4 B @ 20
// = 24 B, down from the 72 B float32 form. THE BITANGENT IS GONE: it is the third
// leg of an orthonormal frame, so every consumer rebuilds it as
// cross(normal, tangent.xyz) * tangent.w — one sign bit instead of twelve bytes.
// The sign is authored CPU-side by the packer, which derives it from the mesh's
// own bitangent stream (Flux_VertexPacker.cpp's 4-lane TANGENT rule).
//
// TEXTUAL #include (NOT `import`): `VsIn` has to land in the INCLUDING shader's own
// scope, because that is the name each entry point spells in its `vsMain(VsIn)`
// signature and the name Slang reflects the vertex inputs off; an `import` would
// namespace-wrap it out of that scope. This file declares no ParameterBlock and no
// [SpecializationConstant], so — exactly like Common/VertexFormats.slang — it
// cannot shift the spine's set 0/1/2 assignment and may be included in any order
// relative to the other textual includes.
//
// THE NAME IS PLAIN `VsIn`, deliberately: ten other shaders declare their own local
// `VsIn` for their own vertex streams (the unit-quad consumers, terrain, …), so this
// file is included ONLY by the four programs that fetch the static-mesh stream.
// Including it beside one of those local declarations is a redefinition error, which
// is the correct outcome — those streams are not this one.
// =====================================================================

#ifndef FLUX_COMMON_STATICMESHVERTEX_SLANG
#define FLUX_COMMON_STATICMESHVERTEX_SLANG

#include "Common/VertexFormats.slang"   // [VtxFmt] authoring attribute (guarded)

struct VsIn
{
	[VtxFmt("half4")]           float3 a_xPosition : POSITION;
	[VtxFmt("half2")]           float2 a_xUV       : TEXCOORD0;
	[VtxFmt("snorm10_10_10_2")] float3 a_xNormal   : NORMAL;
	[VtxFmt("snorm10_10_10_2")] float4 a_xTangent  : TANGENT;    // w = bitangent sign
	[VtxFmt("unorm8x4")]        float4 a_xColor    : COLOR;
};

// The bitangent the deleted BINORMAL attribute used to carry, rebuilt from the
// frame it is the third leg of. ONE definition, because all four programs have to
// reconstruct it identically — a shell that rebuilt it with the sign dropped would
// light every mirrored UV island inside-out, and nothing downstream could tell.
float3 Flux_ReconstructBitangent(VsIn input)
{
	return cross(input.a_xNormal, input.a_xTangent.xyz) * input.a_xTangent.w;
}

#endif // FLUX_COMMON_STATICMESHVERTEX_SLANG
