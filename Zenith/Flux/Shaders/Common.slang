// Common.slang - Common shader definitions
// Frame constants and utility functions

// Frame constants uniform buffer
struct FrameConstantsData
{
    float4x4 g_xViewMat;
    float4x4 g_xProjMat;
    float4x4 g_xViewProjMat;
    float4x4 g_xInvViewProjMat;
    float4x4 g_xInvViewMat;
    float4x4 g_xInvProjMat;
    float4 g_xCamPos_Pad;
    float4 g_xSunDir_Pad;
    float4 g_xSunColour;
    uint2 g_xScreenDims;
    float2 g_xRcpScreenDims;
    uint g_bQuadUtilisationAnalysis;
    uint g_uTargetPixelsPerTri;
    float2 g_xCameraNearFar;  // x = near plane, y = far plane
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameConstantsData> FrameConstants;

// Directional light structure
struct DirectionalLight
{
    float4 m_xDirection;  // 4 bytes of padding
    float4 m_xColour;
};

// Calculate ray direction from UV coordinates
float3 RayDir(float2 uv)
{
    float2 ndc = uv * 2.0 - 1.0;
    float4 clipSpace = float4(ndc, 1.0, 1.0);
    float4 viewSpace = mul(FrameConstants.g_xInvProjMat, clipSpace);
    viewSpace.w = 0.0;
    float3 worldSpace = mul(FrameConstants.g_xInvViewMat, viewSpace).xyz;
    return normalize(worldSpace);
}

// Get world position from depth texture
float3 GetWorldPosFromDepthTex(Texture2D depthTex, SamplerState depthSampler, float2 uv)
{
    float depth = depthTex.Sample(depthSampler, uv).x;
    float2 ndc = uv * 2.0 - 1.0;
    float4 clipSpace = float4(ndc, depth, 1.0);
    float4 viewSpace = mul(FrameConstants.g_xInvProjMat, clipSpace);
    viewSpace /= viewSpace.w;
    return mul(FrameConstants.g_xInvViewMat, viewSpace).xyz;
}
