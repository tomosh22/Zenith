// =====================================================================
// Common/UnifiedMeshDraw — shared Slang include for the unified GPU-driven
// opaque-mesh draw family (Flux_UnifiedMesh_ToGBuffer / ToGBufferVelocity /
// ToShadowmap).
//
// The three shells were literal copies of one another: same vertex input, same
// two-level GPU-scene indirection, the SAME VAT + world transform (a hand-
// maintained "keep these BYTE-IDENTICAL or foliage self-shadows flicker" comment
// contract at Flux_UnifiedMesh_ToShadowmap.slang), and — for the two lit shells —
// the SAME 9-texture uber-material fetch. This include hoists all of that into ONE
// definition so the shells can never drift:
//
//   * VsIn / UnifiedDrawConstantsLayout / DrawParams / UnpackRGBA8 — the shared
//     vertex input, per-bucket draw constants, DRAW block (set 4) and tint unpack.
//   * TransformUnifiedVertexPosition / TransformUnifiedVertex — the shared VAT sway
//     + model transform (+ world-space TBN basis for the lit paths). ONE position
//     path, three callers → the foliage-self-shadow contract is now STRUCTURAL, not
//     a comment.
//   * EvaluateUnifiedGBuffer + the IUnifiedMeshOutput policy generic
//     (EmitUnifiedFragment<TPolicy>) — the shared uber-material G-buffer write, with
//     each shell's final MRT packing supplied by a compile-time policy (GBufferPolicy
//     here; the velocity shell adds its own GBufferVelocityPolicy over the same
//     interface, mirroring the extension-seam pattern used elsewhere in Common).
//
// TEXTUAL #include (NOT `import`): it (transitively) declares the spine
// ParameterBlocks via Common/Bindings.slang, whose fixed set 0/1/2 assignment
// depends on declaration order — `import` would reorder it. The helpers take the
// per-draw pass data (the resolved GPUSceneObject, the DrawConstants, the VAT
// sampler) as PARAMETERS and never reference g_xPassSet / g_xDrawSet, so each shell
// keeps its OWN PassParams (set 3 — the velocity variant's differs: +PrevTransforms
// / +PrevSkinnedPosArena) and DrawParams ParameterBlock instances, declared in the
// canonical PASS-then-DRAW order after this include. The descriptor interface of
// every shell is therefore unchanged.
// =====================================================================

#ifndef FLUX_COMMON_UNIFIEDMESHDRAW_SLANG
#define FLUX_COMMON_UNIFIEDMESHDRAW_SLANG

#include "Common/Bindings.slang"       // spine sets 0/1/2 (declared FIRST) + accessors + Inverse3x3/BuildTBN via Common.Material
#include "Common/SceneObjects.slang"   // GPUSceneObject / GPUSceneDrawItem (guarded)
#include "Common/VAT.slang"            // Flux_ApplyVAT (guarded)
import Common.MaterialTable;           // MaterialGPU
import Common.Material;                // Inverse3x3 / BuildTBN
import Common.MaterialSurface;         // EvaluateMaterialSurface / MaterialSurface / MakeGBuffer(MaterialSurface)
import Common.GBuffer;                 // GBufferOut

// Shared vertex input for every unified-mesh draw (interleaved static vertex format).
struct VsIn
{
    float3 a_xPosition  : POSITION;
    float2 a_xUV        : TEXCOORD0;
    float3 a_xNormal    : NORMAL;
    float3 a_xTangent   : TANGENT;
    float3 a_xBitangent : BINORMAL;
    float4 a_xColor     : COLOR;
};

// Per-bucket DRAW constants (set 4), shared by all three shells. 32-byte layout,
// byte-identical to the C++ mirror in Flux_UnifiedMesh.cpp (static_assert-locked).
// The two trailing words carry the velocity variant's skinned-prev-pose role
// (g_uSkinnedPrevVertBase / g_uIsSkinned); the GBuffer + Shadow paths never read
// them (they were g_uPad0 / g_uPad1 there — offsets unchanged).
struct UnifiedDrawConstantsLayout
{
    uint   g_uMaterialIndex;       // index into g_axMaterials (GLOBAL set 0)
    uint   g_uBucketOffset;        // base offset of this draw's slice in VisibleIndexBuffer (per-view)
    uint   g_uSkinnedPrevVertBase; // velocity-only: skinned draw's prev-arena slice base (0 for static/VAT/GBuffer/shadow)
    uint   g_uIsSkinned;           // velocity-only: 1 for a skinned draw (0 otherwise)
    float4 g_xVATParams;           // xy = VAT texture size, z = >0 when the bucket has a VAT texture, w unused
};

// DRAW block (set 4) — per-bucket constants + the bucket's slice of the visible
// list + the per-bucket VAT positions. Declared identically in all three shells
// (the shell declares `ParameterBlock<DrawParams> g_xDrawSet;` AFTER its PASS block
// so DRAW lands at physical space 4).
struct DrawParams
{
    ConstantBuffer<UnifiedDrawConstantsLayout> DrawConstants;
    StructuredBuffer<uint>                     VisibleIndexBuffer;
    Sampler2D                                  g_xAnimationTex;   // per-bucket VAT positions (POINT sampler; white dummy when none)
};

float4 UnpackRGBA8(uint uPacked)
{
    return float4(
        (float)((uPacked >> 0)  & 0xFFu) / 255.0,
        (float)((uPacked >> 8)  & 0xFFu) / 255.0,
        (float)((uPacked >> 16) & 0xFFu) / 255.0,
        (float)((uPacked >> 24) & 0xFFu) / 255.0);
}

// ---------------------------------------------------------------------
// Shared vertex transform
// ---------------------------------------------------------------------

// The world-space normal basis carried out of the full lit-path transform.
struct UnifiedVertexBasis
{
    float3 xWorldPos;       // world position (VAT sway + model transform)
    float3 xLocalPos;       // the VAT-deformed local position (the velocity path reprojects it)
    float3 xWorldNormal;
    float3 xWorldTangent;
    float  fBitangentSign;
};

// THE shared VAT sway + world transform — one position path for the GBuffer, velocity
// and shadow shells. Bit-for-bit identical across them is required: if the shadow VS
// deforms differently from the GBuffer VS, self-shadow tests misalign and flicker as
// foliage sways (was the "BYTE-IDENTICAL" comment contract; now structural). Outputs
// the VAT-deformed local position (the velocity path reprojects it through the previous
// transform); shadow ignores it (dead-stripped).
float3 TransformUnifiedVertexPosition(VsIn input, uint vertexId, GPUSceneObject obj,
    UnifiedDrawConstantsLayout dc, Sampler2D xAnimationTex, out float3 xLocalPos)
{
    xLocalPos = Flux_ApplyVAT(xAnimationTex, vertexId, input.a_xPosition,
        obj.m_uFlags, obj.m_uVATAnimPacked, asfloat(obj.m_uVATAnimTime), dc.g_xVATParams);
    return mul(obj.m_xModelMatrix, float4(xLocalPos, 1.0)).xyz;
}

// Full lit-path transform: the shared position path above + the world-space TBN basis
// (normal / tangent / bitangent sign). Used by the GBuffer + velocity shells.
UnifiedVertexBasis TransformUnifiedVertex(VsIn input, uint vertexId, GPUSceneObject obj,
    UnifiedDrawConstantsLayout dc, Sampler2D xAnimationTex)
{
    UnifiedVertexBasis b;
    b.xWorldPos = TransformUnifiedVertexPosition(input, vertexId, obj, dc, xAnimationTex, b.xLocalPos);
    float3x3 xNormalMatrix = transpose(Inverse3x3((float3x3)obj.m_xModelMatrix));
    b.xWorldNormal    = normalize(mul(xNormalMatrix, input.a_xNormal));
    b.xWorldTangent   = normalize(mul(xNormalMatrix, input.a_xTangent));
    float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
    b.fBitangentSign  = sign(dot(xBitangent, cross(b.xWorldNormal, b.xWorldTangent)));
    return b;
}

// ---------------------------------------------------------------------
// Shared fragment: uber-material G-buffer write + output policy
// ---------------------------------------------------------------------

// The lit-path fragment interpolants shared by the GBuffer + velocity shells (the
// velocity shell adds its own no-jitter clip positions as extra interpolants). A pure
// function-argument struct — never a varying, so it does not touch the vs->fs layout.
struct UnifiedFragmentSurface
{
    float2 uv;
    float3 worldNormal;
    float3 worldPos;
    float3 worldTangent;
    float  bitangentSign;
    float4 vertexColor;
    float4 instanceTint;
};

// Shared uber-material evaluation -> the 4-target G-buffer core. Identical to the
// StaticMeshes / InstancedMeshes uber-material path, plus the per-instance colour tint.
// The velocity shell appends a motion vector to this core via its policy.
GBufferOut EvaluateUnifiedGBuffer(UnifiedFragmentSurface s, uint uMaterialIndex, bool bFrontFace)
{
    float3x3 TBN = BuildTBN(s.worldNormal, s.worldTangent, s.bitangentSign);
    float3 xViewDir = normalize(GetCamPos_Pad().xyz - s.worldPos);

    MaterialGPU m = GetMaterial(uMaterialIndex);

    Sampler2D xBaseColorTex         = GetBindlessTexture(m.g_xTexIdx0.x);
    Sampler2D xNormalTex            = GetBindlessTexture(m.g_xTexIdx0.y);
    Sampler2D xRoughnessMetallicTex = GetBindlessTexture(m.g_xTexIdx0.z);
    Sampler2D xOcclusionTex         = GetBindlessTexture(m.g_xTexIdx0.w);
    Sampler2D xEmissiveTex          = GetBindlessTexture(m.g_xTexIdx1.x);
    Sampler2D xHeightTex            = GetBindlessTexture(m.g_xTexIdx1.y);
    Sampler2D xDetailAlbedoTex      = GetBindlessTexture(m.g_xTexIdx1.z);
    Sampler2D xDetailNormalTex      = GetBindlessTexture(m.g_xTexIdx1.w);
    Sampler2D xDetailMaskTex        = GetBindlessTexture(m.g_xTexIdx2.x);

    MaterialSurface xSurface = EvaluateMaterialSurface(
        xBaseColorTex, xNormalTex, xRoughnessMetallicTex,
        xOcclusionTex, xEmissiveTex, xHeightTex,
        xDetailAlbedoTex, xDetailNormalTex, xDetailMaskTex,
        s.uv, TBN, xViewDir, s.vertexColor, bFrontFace, m);

    xSurface.albedo.rgb *= s.instanceTint.rgb;

    return MakeGBuffer(xSurface);
}

// Compile-time policy: how a unified-mesh fragment finalises the shared G-buffer core
// into its own MRT output. Used as a GENERIC CONSTRAINT only (never an existential
// value — an existential would perturb reflection). The GBuffer path returns the core
// verbatim; the velocity shell conforms its own GBufferVelocityPolicy (which appends
// the motion vector) over this same interface.
interface IUnifiedMeshOutput
{
    associatedtype TOut;
    static TOut Emit(GBufferOut xCore, float2 xVelocityUV);
}

// The 4-target G-buffer path: emit the core unchanged (the velocity is ignored — the
// caller passes 0 and the dead value is stripped).
struct GBufferPolicy : IUnifiedMeshOutput
{
    typealias TOut = GBufferOut;
    static GBufferOut Emit(GBufferOut xCore, float2 xVelocityUV) { return xCore; }
}

// Shared fragment body: the uber-material G-buffer write + the policy's final packing.
// The GBuffer shell instantiates <GBufferPolicy> with a zero velocity (stripped); the
// velocity shell instantiates <GBufferVelocityPolicy> with the computed motion vector.
TPolicy.TOut EmitUnifiedFragment<TPolicy : IUnifiedMeshOutput>(
    UnifiedFragmentSurface s, uint uMaterialIndex, bool bFrontFace, float2 xVelocityUV)
{
    GBufferOut xCore = EvaluateUnifiedGBuffer(s, uMaterialIndex, bFrontFace);
    return TPolicy.Emit(xCore, xVelocityUV);
}

#endif // FLUX_COMMON_UNIFIEDMESHDRAW_SLANG
