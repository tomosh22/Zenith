// =====================================================================
// Common/VAT — shared Vertex Animation Texture sampling for the unified
// GPU-driven mesh pipeline (GBuffer + ToShadowmap casters).
//
// Bit-for-bit identical sampling for the lit + shadow passes is REQUIRED: if the
// shadow VS deforms differently from the GBuffer VS, self-shadow tests misalign and
// flicker as foliage sways. Both unified shaders #include this single definition so
// they can never drift (the InstancedMeshes shaders inline two copies; this is the
// DRY replacement for the unified path).
//
// The VAT position texture is laid out: width = vertex count (padded), height =
// numAnimations * framesPerAnimation, RGBA16F (xyz = deformed local position). MUST be
// sampled with a POINT sampler — linear filtering cross-blends different vertices.
// =====================================================================

#ifndef FLUX_COMMON_VAT_SLANG
#define FLUX_COMMON_VAT_SLANG

// Two-tap (current + next frame) lerp of the deformed local-space position.
// xTex must be bound with a POINT sampler.
float3 Flux_SampleVAT(Sampler2D xTex, uint uVertexID, uint uAnimIndex, float fAnimTime,
    uint uFrameCount, uint uTextureWidth, uint uTextureHeight)
{
    float fFrame = fAnimTime * (float)(uFrameCount - 1u);
    uint  uFrame = (uint)fFrame;
    float fBlend = frac(fFrame);

    uFrame          = min(uFrame, uFrameCount - 1u);
    uint uNextFrame = min(uFrame + 1u, uFrameCount - 1u);

    uint uRow0 = uAnimIndex * uFrameCount + uFrame;
    uint uRow1 = uAnimIndex * uFrameCount + uNextFrame;

    float2 xUV0 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
                         ((float)uRow0     + 0.5) / (float)uTextureHeight);
    float2 xUV1 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
                         ((float)uRow1     + 0.5) / (float)uTextureHeight);

    float3 xPos0 = xTex.SampleLevel(xUV0, 0).xyz;
    float3 xPos1 = xTex.SampleLevel(xUV1, 0).xyz;
    return lerp(xPos0, xPos1, fBlend);
}

// Apply VAT to a vertex's local position given the object's packed VAT fields + the
// per-bucket VAT params (xy = texture size, z = >0 when the bucket has a VAT texture).
// Returns the input position unchanged when VAT is inactive (object flag clear, no
// texture, or a degenerate frame count) — so static objects in a VAT-capable shader
// stay rest-pose with no branch divergence cost beyond the uniform gate.
float3 Flux_ApplyVAT(Sampler2D xTex, uint uVertexID, float3 xLocalPos,
    uint uObjectFlags, uint uVATAnimPacked, float fVATAnimTime, float4 xVATParams)
{
    bool bBucketHasVAT = xVATParams.z > 0.0;
    bool bObjUsesVAT   = (uObjectFlags & FLUX_GPUSCENE_OBJFLAG_VAT) != 0u;
    uint uFrameCount   = (uVATAnimPacked >> 16u) & 0xFFFFu;
    if (!bBucketHasVAT || !bObjUsesVAT || uFrameCount == 0u)
    {
        return xLocalPos;
    }
    uint uAnimIndex = uVATAnimPacked & 0xFFFFu;
    return Flux_SampleVAT(xTex, uVertexID, uAnimIndex, fVATAnimTime, uFrameCount,
        (uint)xVATParams.x, (uint)xVATParams.y);
}

#endif // FLUX_COMMON_VAT_SLANG
