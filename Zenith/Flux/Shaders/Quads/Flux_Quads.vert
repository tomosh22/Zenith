#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in uvec4 a_xInstancePositionSize;
layout(location = 3) in vec4 a_xInstanceColour;
layout(location = 4) in uint a_uTexture;
layout(location = 5) in vec2 a_xUVMult_UVAdd;

layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec4 o_xColour;
layout(location = 2) flat out uint o_uTexture;

void main()
{
	o_xUV = a_xUV * a_xUVMult_UVAdd.x + a_xUVMult_UVAdd.y;
	o_xColour = a_xInstanceColour;
	o_uTexture = a_uTexture;
	
	gl_Position = g_xViewProjMat * vec4(((a_xPosition.xy * a_xInstancePositionSize.zw) + a_xInstancePositionSize.xy) * g_xRcpScreenDims * 2.f - 1.f, 0, 1);
}