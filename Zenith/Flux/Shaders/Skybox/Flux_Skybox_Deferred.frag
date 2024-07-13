#version 450 core

#include "Common.h"


layout(location = 0) out vec4 o_xDiffuse;
layout(location = 1) out vec4 o_xNormalsAmbient;
layout(location = 2) out vec4 o_xMaterial;
layout(location = 3) out vec4 o_xWorldPos;

layout(location = 0) in vec2 UV;

vec3 RayDir(vec2 pixel)//takes pixel in 0,1 range
{
	vec2 ndc = pixel * 2 - 1;//move from 0,1 to -1,1

	vec4 clipSpace = vec4(ndc, -1,1);

	vec4 viewSpace = inverse(_uProjMat) * clipSpace; //#TODO invert this CPU side
	viewSpace.w = 0;

	vec3 worldSpace = (inverse(_uViewMat) * viewSpace).xyz; //#TODO same here

	return normalize(worldSpace);
}

void main(){
	vec3 rayDir = RayDir(UV);

	o_xDiffuse = vec4(0.2,0.3,0.9,1);
	o_xDiffuse += max(rayDir.y + 0.3,0);
	o_xDiffuse.xyz /= 2.f;
	
	o_xNormalsAmbient = vec4(0);
	o_xMaterial = vec4(0);
	
	o_xWorldPos = vec4(0);
}