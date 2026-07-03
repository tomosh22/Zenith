// =====================================================================
// Common.Velocity — shared Slang module for TAA motion vectors.
//
// The OPTIONAL 5th G-buffer MRT (MRT_INDEX_VELOCITY, R16G16_SFLOAT) carries a
// per-pixel screen-space motion vector:
//   velocity.xy = uvCurrent - uvPrev   (UV space; history lookup = uv - velocity)
// computed from the UNJITTERED current + previous view-proj matrices
// (g_xView.g_xViewProjMatNoJitter / g_xPrevViewProjMatNoJitter), so the vector is
// jitter-free at the source and the temporal resolve does no jitter subtraction.
//
// This lives in a SEPARATE output struct (GBufferVelocityOut) — the shared GBufferOut
// (Common/GBuffer.slang) is used by 4-attachment passes too (Skybox / Primitives),
// which must NOT gain an SV_Target4. Only the UnifiedMesh/Terrain *_ToGBufferVelocity
// variants (recorded inside the 5-attachment G-buffer pass when the velocity latch is
// on) write this struct.
//
// Convention MIRRORS Flux_TAAJitter.h (the CPU test mirror) EXACTLY:
//   UV = ndc * 0.5 + 0.5 on BOTH axes (Vulkan ndc.y and uv.y increase downward).
// =====================================================================

import Common.GBuffer;   // GBufferOut (the shared 4-target core) — MakeGBufferVelocity promotes it

// The 5-MRT G-buffer output: the 4 core targets (identical to GBufferOut) + velocity.
public struct GBufferVelocityOut
{
    public float4 diffuse        : SV_Target0;
    public float4 normalsAmbient : SV_Target1;
    public float4 material       : SV_Target2;
    public float4 emissive       : SV_Target3;
    public float2 velocity       : SV_Target4; // uvCurrent - uvPrev (UV space, unjittered)
};

// Clip -> UV after the perspective divide (mirrors Flux_ClipToUV in Flux_TAAJitter.h).
public float2 Flux_ClipToUV(float4 xClip)
{
    float2 xNdc = xClip.xy / xClip.w;
    return xNdc * 0.5 + 0.5;
}

// Motion vector: velocity = uvCurrent - uvPrev, both from UNJITTERED clip positions.
// A pixel that did not move (static geometry, static camera) encodes exactly (0,0).
public float2 Flux_EncodeVelocityUV(float4 xCurClipNoJitter, float4 xPrevClipNoJitter)
{
    return Flux_ClipToUV(xCurClipNoJitter) - Flux_ClipToUV(xPrevClipNoJitter);
}

// Promote a shared 4-target GBufferOut to the 5-target velocity output by copying the
// core targets and appending the motion vector. Lets a *_ToGBufferVelocity fragment run
// the identical shading as its base program (producing GBufferOut) and only add velocity.
public GBufferVelocityOut MakeGBufferVelocity(GBufferOut xCore, float2 xVelocity)
{
    GBufferVelocityOut o;
    o.diffuse        = xCore.diffuse;
    o.normalsAmbient = xCore.normalsAmbient;
    o.material       = xCore.material;
    o.emissive       = xCore.emissive;
    o.velocity       = xVelocity;
    return o;
}
