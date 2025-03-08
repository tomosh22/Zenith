#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 1) uniform sampler2D g_xDepthTex;

layout(push_constant) uniform FogConstants{
	vec4 g_xFogColour_Falloff;
};

void main()
{
	float fDepth = texture(g_xDepthTex, a_xUV).x;
	
	vec2 xNDC = a_xUV * 2. - 1.;

	vec4 xClipSpace = vec4(xNDC, fDepth, 1.);

	//#TO_TODO: invert this CPU side
	vec4 xViewSpace = inverse(g_xProjMat) * xClipSpace;
	xViewSpace /= xViewSpace.w;

	//#TO_TODO: same here
	vec4 xWorldSpace = (inverse(g_xViewMat) * xViewSpace);
	
	vec3 xCameraToPixel = xWorldSpace.xyz - g_xCamPos_Pad.xyz;
	
	float fDist = length(xCameraToPixel);
	
	//credit Inigo Quilez
	float fFogAmount = 1.0 - exp(-fDist * g_xFogColour_Falloff.w);
	float fSunAmount = max( dot(normalize(xCameraToPixel), -g_xSunDir_Pad.xyz), 0.0 );
    vec3  xFogColour  = mix( g_xFogColour_Falloff.xyz,
                           g_xSunColour.xyz,
                           pow(fSunAmount,8.0) );
	o_xColour = vec4(xFogColour, fFogAmount);
}