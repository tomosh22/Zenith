layout(std140, set = 0, binding=0) uniform FrameConstants{
	mat4 g_xViewMat;
	mat4 g_xProjMat;
	mat4 g_xViewProjMat;
	mat4 g_xInvViewProjMat;
	vec4 g_xCamPos_Pad;
	vec4 g_xSunDir_Pad;
	vec4 g_xSunColour;
	uvec2 g_xScreenDims;
	vec2 g_xRcpScreenDims;
};

struct DirectionalLight{
    vec4 m_xDirection;//4 bytes of padding
    vec4 m_xColour;
};

vec3 RayDir(vec2 xUV)
{
	vec2 xNDC = xUV * 2. - 1.;

	vec4 xClipSpace = vec4(xNDC, 1., 1.);

	//#TO_TODO: invert this CPU side
	vec4 xViewSpace = inverse(g_xProjMat) * xClipSpace;
	xViewSpace.w = 0.;

	//#TO_TODO: same here
	vec3 xWorldSpace = (inverse(g_xViewMat) * xViewSpace).xyz;

	return normalize(xWorldSpace);
}