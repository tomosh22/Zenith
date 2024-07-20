#version 450 core

#include "Common.h"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in mat3 a_xTBN;

layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex;

//#TO_TODO: textures
#if 0
layout(set = 1, binding = 0) uniform sampler2D diffuseTex;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D roughnessTex;
layout(set = 1, binding = 3) uniform sampler2D metallicTex;
#endif

void main(){
	vec3 xDiffuse = texture(g_xDiffuseTex, a_xUV).xyz;
	o_xColour = vec4(xDiffuse, 1.);
}