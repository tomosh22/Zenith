#version 450 core

//#include "Common.h"


//layout(set = 0, binding = 0) uniform samplerCube cubemap;


layout(location = 0) out vec4 _oColor;

layout(location = 0) in vec2 a_xUV;

#if 0
vec3 RayDir(vec2 pixel)//takes pixel in 0,1 range
{
	vec2 ndc = pixel * 2 - 1;//move from 0,1 to -1,1

	vec4 clipSpace = vec4(ndc, -1,1);

	vec4 viewSpace = inverse(_uProjMat) * clipSpace; //#TODO invert this CPU side
	viewSpace.w = 0;

	vec3 worldSpace = (inverse(_uViewMat) * viewSpace).xyz; //#TODO same here

	return normalize(worldSpace);
}
#endif

void main(){
	//vec3 rayDir = RayDir(UV);

	//_oColor = texture(cubemap, rayDir);

	//_oColor = vec4(0.2,0.3,0.9,1);
	//_oColor += max(rayDir.y + 0.3,0);
	//_oColor.xyz /= 2.f;
	//_oColor = vec4(rayDir+0.5,1);
	_oColor = vec4(a_xUV, 0.f, 1.f);
	
}