// =====================================================================
// Common.Frame — shared Slang module.
//
// Replaces the GLSL `Common.fxh` for shaders ported to Slang. Exposes
// the per-frame constants UBO (FrameConstants) at set=0 binding=0
// matching the legacy convention, plus the screen-space utility
// functions every fullscreen pass depends on.
//
// Importer pattern: `import Common.Frame;` then read members through
// the FrameConstants accessor, e.g. FrameConstants.g_xViewMat.
//
// Binding convention: this module reserves set=0 binding=0. Shaders
// that import this module must place their own resources at binding 1
// or higher.
// =====================================================================

public struct FrameConstantsLayout
{
	public float4x4 g_xViewMat;
	public float4x4 g_xProjMat;
	public float4x4 g_xViewProjMat;
	public float4x4 g_xInvViewProjMat;
	public float4x4 g_xInvViewMat;
	public float4x4 g_xInvProjMat;
	public float4   g_xCamPos_Pad;
	public float4   g_xSunDir_Pad;
	public float4   g_xSunColour;
	public uint2    g_xScreenDims;
	public float2   g_xRcpScreenDims;
	public uint     g_bQuadUtilisationAnalysis;
	public uint     g_uTargetPixelsPerTri;
	public float2   g_xCameraNearFar; // x = near, y = far
};

[[vk::binding(0, 0)]] public ConstantBuffer<FrameConstantsLayout> FrameConstants;

public struct DirectionalLight
{
	public float4 m_xDirection; // 4 bytes of padding
	public float4 m_xColour;
};

// Reconstruct the world-space ray direction for a given UV.
public float3 RayDir(float2 xUV)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClipSpace = float4(xNDC, 1.0, 1.0);
	float4 xViewSpace = mul(FrameConstants.g_xInvProjMat, xClipSpace);
	xViewSpace.w = 0.0;
	float3 xWorldSpace = mul(FrameConstants.g_xInvViewMat, xViewSpace).xyz;
	return normalize(xWorldSpace);
}

public float3 GetWorldPosFromDepthTex(Sampler2D xDepthTex, float2 xUV)
{
	float fDepth = xDepthTex.Sample(xUV).x;
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClipSpace = float4(xNDC, fDepth, 1.0);
	float4 xViewSpace = mul(FrameConstants.g_xInvProjMat, xClipSpace);
	xViewSpace /= xViewSpace.w;
	return mul(FrameConstants.g_xInvViewMat, xViewSpace).xyz;
}

// View-space reconstruction from UV + sampled depth. Used by SSR / SSGI /
// SSAO — those passes typically sample the depth texture themselves.
public float3 GetViewPosFromDepth(float2 xUV, float fDepth)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClipSpace = float4(xNDC, fDepth, 1.0);
	float4 xViewSpace = mul(FrameConstants.g_xInvProjMat, xClipSpace);
	return xViewSpace.xyz / xViewSpace.w;
}

public float DepthToViewZ(float2 xUV, float fDepth)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClipSpace = float4(xNDC, fDepth, 1.0);
	float4 xViewSpace = mul(FrameConstants.g_xInvProjMat, xClipSpace);
	return xViewSpace.z / xViewSpace.w;
}

public float3 ViewToScreen(float3 xViewPos)
{
	float4 xClipSpace = mul(FrameConstants.g_xProjMat, float4(xViewPos, 1.0));
	xClipSpace.xyz /= xClipSpace.w;
	return float3(xClipSpace.x * 0.5 + 0.5, xClipSpace.y * 0.5 + 0.5, xClipSpace.z);
}

// Smooth screen-edge fade for screen-space effects (avoids sharp
// artifacts at the borders).
public float ComputeEdgeFade(float2 xUV, float fMargin)
{
	float2 xFade = smoothstep(0.0, fMargin, xUV) * smoothstep(0.0, fMargin, 1.0 - xUV);
	return xFade.x * xFade.y;
}

public static const int   BLUE_NOISE_SIZE = 64;
public static const float GOLDEN_RATIO    = 0.618033988749;
