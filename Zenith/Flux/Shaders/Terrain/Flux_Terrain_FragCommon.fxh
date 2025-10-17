#include "../Common.fxh"
#ifndef SHADOWS
#include "../GBufferCommon.fxh"
#endif

#ifndef SHADOWS
layout(location = 0) in vec2 a_xUV;
#endif
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xWorldPos;
layout(location = 3) in float a_fMaterialLerp;
layout(location = 4) in mat3 a_xTBN;

#ifndef SHADOWS
layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessMetallicTex0;

layout(set = 1, binding = 3) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 4) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 5) uniform sampler2D g_xRoughnessMetallicTex1;
#endif

void main(){
	#ifndef SHADOWS
	vec4 xDiffuse0 = texture(g_xDiffuseTex0, a_xUV);
	vec3 xNormal0 = a_xTBN * (2 * texture(g_xNormalTex0, a_xUV).xyz - 1.);
	vec2 xRoughnessMetallic0 = texture(g_xRoughnessMetallicTex0, a_xUV).gb;
	
	vec4 xDiffuse1 = texture(g_xDiffuseTex1, a_xUV);
	vec3 xNormal1 = a_xTBN * (2 * texture(g_xNormalTex1, a_xUV).xyz - 1.);
	vec2 xRoughnessMetallic1 = texture(g_xRoughnessMetallicTex1, a_xUV).gb;
	
	vec4 xDiffuse = mix(xDiffuse0, xDiffuse1, a_fMaterialLerp);
	vec3 xNormal = mix(xNormal0, xNormal1, a_fMaterialLerp);
	vec2 xRoughnessMetallic = mix(xRoughnessMetallic0, xRoughnessMetallic1, a_fMaterialLerp);

	OutputToGBuffer(xDiffuse, xNormal, 0.2, xRoughnessMetallic.x, xRoughnessMetallic.y);
	#endif
}