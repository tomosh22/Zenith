// =====================================================================
// Common/VertexFormats — the vertex-layout AUTHORING vocabulary.
//
// Two things live here, both serving the compressed-vertex work:
//
//   [VtxFmt("<fmt>")]  declares the STORAGE format of a vertex-input field when
//                      it differs from the declared type. Fetch hardware widens
//                      and converts, so a four-byte SNORM10:10:10:2 attribute is
//                      still declared `float4` in the VsIn — without the tag the
//                      generator would infer a sixteen-byte float4 and bake a
//                      wrong stride. Legal strings are exactly the ones
//                      Flux_ParseVertexFormatString accepts
//                      (Flux/Slang/Flux_SlangCompiler.cpp); a typo is a hard
//                      compile error, never a silent fall-back to the declared
//                      type.
//   [PerInstance]      moves a field off the per-vertex stream (binding 0) onto
//                      the per-instance stream (binding 1).
//
// Slang has no built-in vertex-format attribute, so both are declared the way the
// stdlib declares its own: [__AttributeUsage(_AttributeTargets.Var)] on a struct
// whose name ends in "Attribute". THE SUFFIX IS LOAD-BEARING — Slang strips it, so
// `struct VtxFmtAttribute` is what makes `[VtxFmt(...)]` resolve and is also the
// name reflection reports back ("VtxFmt").
//
// TEXTUAL #include (NOT `import`): an attribute must be resolvable by name in the
// scope of the shader that spells it, and an imported module would namespace-wrap
// these declarations out of that scope. Declares no ParameterBlock, so — like
// Common/ViewModes.slang — it cannot shift the spine's set 0/1/2 assignment and may
// be included in any order relative to the other textual includes.
// =====================================================================

#ifndef FLUX_COMMON_VERTEXFORMATS_SLANG
#define FLUX_COMMON_VERTEXFORMATS_SLANG

[__AttributeUsage(_AttributeTargets.Var)]
struct VtxFmtAttribute { string format; };

[__AttributeUsage(_AttributeTargets.Var)]
struct PerInstanceAttribute {};

// The shader-side mirror of Flux_UnpackSnorm16x4Position (Flux/Flux_VertexCodec.h)
// and it must stay EXACTLY that: a snorm16 position decodes on the GPU only if the
// dequant here is the precise inverse of the quantisation an exporter baked, and the
// two live in different languages with no compiler to tie them together.
//
// xFetched arrives already converted by the fetch hardware — a snorm16x4 attribute
// reads as a float4 in [-1,1] — so the whole transform is the affine map back onto
// the authored box: xBias + (n * 0.5 + 0.5) * xScale, where the box is the mesh's
// tight AABB (m_xBias = min, m_xScale = extent).
float3 Flux_DequantPosition(float4 xFetched, float3 xScale, float3 xBias)
{
	return xBias + (xFetched.xyz * 0.5 + 0.5) * xScale;
}

#endif // FLUX_COMMON_VERTEXFORMATS_SLANG
