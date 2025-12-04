#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in float a_fMaterialLerp;

#ifndef SHADOWS
layout(location = 0) out vec2 o_xUV;
#endif
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out float o_fMaterialLerp;
layout(location = 4) out mat3 o_xTBN;
layout(location = 7) flat out uint o_uLODLevel;  // Flat shading for LOD level

#ifndef SHADOWS
layout(std140, set = 0, binding=1) uniform TerrainConstants{
	float g_fUVScale;
};
#endif

#ifdef SHADOWS
layout(std140, set = 1, binding=0) uniform ShadowMatrix{
	mat4 g_xSunViewProjMat;
};
#endif

#ifndef SHADOWS
// LOD level buffer (read-only in vertex shader)
layout(std430, set = 0, binding = 2) readonly buffer LODLevelBuffer
{
	uint lodLevels[];
};
#endif

void main()
{
	#ifndef SHADOWS
	o_xUV = a_xUV * g_fUVScale;
	#endif
	o_xNormal = a_xNormal;
	o_xWorldPos = a_xPosition;
	o_fMaterialLerp = a_fMaterialLerp;
	o_xTBN = mat3(a_xTangent, a_xBitangent, a_xNormal);
	
	#ifndef SHADOWS
	// Read LOD level for this draw call
	// gl_InstanceIndex = firstInstance + instanceID. Since instanceCount=1 and instanceID=0,
	// gl_InstanceIndex equals firstInstance which we set to the draw index in the compute shader
	o_uLODLevel = lodLevels[gl_InstanceIndex];
	#endif

	#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(o_xWorldPos,1);
	#else
	gl_Position = g_xViewProjMat * vec4(o_xWorldPos,1);
	#endif
}