layout(std140, set = 0, binding=0) uniform FrameConstants{
	mat4 g_xViewMat;
	mat4 g_xProjMat;
	mat4 g_xViewProjMat;
	mat4 g_xInvViewProjMat;
	mat4 g_xInvViewMat;
	mat4 g_xInvProjMat;
	vec4 g_xCamPos_Pad;
	vec4 g_xSunDir_Pad;
	vec4 g_xSunColour;
	uvec2 g_xScreenDims;
	vec2 g_xRcpScreenDims;
	uint g_bQuadUtilisationAnalysis;
	uint g_uTargetPixelsPerTri;
	vec2 g_xCameraNearFar;  // x = near plane, y = far plane
};

struct DirectionalLight{
    vec4 m_xDirection;//4 bytes of padding
    vec4 m_xColour;
};

vec3 RayDir(vec2 xUV)
{
	vec2 xNDC = xUV * 2. - 1.;

	vec4 xClipSpace = vec4(xNDC, 1., 1.);

	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	xViewSpace.w = 0.;

	vec3 xWorldSpace = (g_xInvViewMat * xViewSpace).xyz;

	return normalize(xWorldSpace);
}

vec3 GetWorldPosFromDepthTex(sampler2D xDepthTex, vec2 xUV)
{
	float fDepth = texture(xDepthTex, xUV).x;

	vec2 xNDC = xUV * 2. - 1.;

	vec4 xClipSpace = vec4(xNDC, fDepth, 1.);

	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	xViewSpace /= xViewSpace.w;

	return (g_xInvViewMat * xViewSpace).xyz;
}

// ============================================================================
// SCREEN-SPACE UTILITY FUNCTIONS
// Used by SSR, SSGI, and other screen-space effects
// ============================================================================

// Reconstruct view-space position from UV and depth value
vec3 GetViewPosFromDepth(vec2 xUV, float fDepth)
{
	vec2 xNDC = xUV * 2.0 - 1.0;
	vec4 xClipSpace = vec4(xNDC, fDepth, 1.0);
	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	return xViewSpace.xyz / xViewSpace.w;
}

// Convert depth buffer value to view-space Z
float DepthToViewZ(vec2 xUV, float fDepth)
{
	vec2 xNDC = xUV * 2.0 - 1.0;
	vec4 xClipSpace = vec4(xNDC, fDepth, 1.0);
	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	return xViewSpace.z / xViewSpace.w;
}

// Transform view position to screen space (returns UV.xy + depth.z)
vec3 ViewToScreen(vec3 xViewPos)
{
	vec4 xClipSpace = g_xProjMat * vec4(xViewPos, 1.0);
	xClipSpace.xyz /= xClipSpace.w;
	return vec3(xClipSpace.x * 0.5 + 0.5, xClipSpace.y * 0.5 + 0.5, xClipSpace.z);
}

// Compute screen-edge fade to prevent artifacts at borders
// fMargin: fraction of screen width/height for fade zone (e.g., 0.15 = 15%)
float ComputeEdgeFade(vec2 xUV, float fMargin)
{
	vec2 xFade = smoothstep(0.0, fMargin, xUV) * smoothstep(0.0, fMargin, 1.0 - xUV);
	return xFade.x * xFade.y;
}

// Blue noise sampling constants (shared by SSR, SSGI)
const int BLUE_NOISE_SIZE = 64;
const float GOLDEN_RATIO = 0.618033988749;