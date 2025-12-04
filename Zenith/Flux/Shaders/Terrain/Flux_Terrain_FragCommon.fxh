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
layout(location = 7) flat in uint a_uLODLevel;  // LOD level from vertex shader

#ifndef SHADOWS
layout(set = 1, binding = 0) uniform sampler2D g_xDiffuseTex0;
layout(set = 1, binding = 1) uniform sampler2D g_xNormalTex0;
layout(set = 1, binding = 2) uniform sampler2D g_xRoughnessMetallicTex0;

layout(set = 1, binding = 3) uniform sampler2D g_xDiffuseTex1;
layout(set = 1, binding = 4) uniform sampler2D g_xNormalTex1;
layout(set = 1, binding = 5) uniform sampler2D g_xRoughnessMetallicTex1;
#endif

// Push constant for debug mode
layout(push_constant) uniform DebugConstants {
	uint debugVisualizeLOD;  // 0 = normal rendering, 1 = visualize LOD
} debugConstants;

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

	// LOD visualization mode
	if (debugConstants.debugVisualizeLOD != 0)
	{
		// Color code by LOD level:
		// LOD0 = Red, LOD1 = Green, LOD2 = Blue, LOD3 = Magenta
		// Out of range = Yellow (error indicator)
		vec3 lodColors[5] = vec3[](
			vec3(1.0, 0.0, 0.0),  // LOD0: Red
			vec3(0.0, 1.0, 0.0),  // LOD1: Green
			vec3(0.0, 0.0, 1.0),  // LOD2: Blue
			vec3(1.0, 0.0, 1.0),  // LOD3: Magenta
			vec3(1.0, 1.0, 0.0)   // Error: Yellow (LOD > 3)
		);
		
		uint clampedLOD = min(a_uLODLevel, 4u);
		xDiffuse.rgb = lodColors[clampedLOD];
		
		// DEBUG: Also visualize LOD as grayscale intensity to see variation
		// LOD 0 = bright, LOD 3 = dark
		// Uncomment this line to debug if colors aren't varying:
		// xDiffuse.rgb = vec3(1.0 - float(clampedLOD) * 0.25);
		
		xRoughnessMetallic = vec2(0.8, 0.0);  // Make it matte for better visibility
	}

	OutputToGBuffer(xDiffuse, xNormal, 0.2, xRoughnessMetallic.x, xRoughnessMetallic.y);
	#endif
}