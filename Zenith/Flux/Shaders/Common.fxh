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