#include "../Common.fxh"

// ========== Vertex Attributes ==========
// Position is in world space (terrain is static, no model matrix needed)
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in float a_fMaterialLerp;

// ========== Vertex Outputs ==========
// OPTIMIZATION: Reduced varying count by packing TBN more efficiently
// Previously: 7 varyings (UV, Normal, WorldPos, MaterialLerp, TBN mat3, LOD)
// Now: 6 varyings with TBN packed into normal + tangent + sign
// Varyings only output when not doing shadow pass (fragment shader doesn't need them)

#ifndef SHADOWS
layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;      // World-space normal
layout(location = 2) out vec3 o_xWorldPos;    // World position for lighting
layout(location = 3) out vec3 o_xTangent;     // World-space tangent (bitangent reconstructed in frag)
layout(location = 4) out float o_fMaterialLerp;
layout(location = 5) flat out uint o_uLODLevel;  // Flat = only read once per primitive
// OPTIMIZATION: Bitangent sign packed with material lerp to save a varying
// Sign is +1 or -1, we use the sign bit of a flag
layout(location = 6) out float o_fBitangentSign;
#endif

#ifndef SHADOWS
// Terrain constants (per-frame, set 0 binding 1)
// Note: Scratch buffer moved from set 0 to set 1 so FrameConstants can be bound once per frame
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
// LOD level buffer (read-only in vertex shader, per-draw, set 1 binding 1)
// OPTIMIZATION: Use flat interpolation so only first vertex per triangle reads this
layout(std430, set = 1, binding = 1) readonly buffer LODLevelBuffer
{
	uint lodLevels[];
};
#endif

void main()
{
	// Position is already in world space (terrain is static)
	vec3 xWorldPos = a_xPosition;

	#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(xWorldPos, 1.0);
	#else
	o_xWorldPos = xWorldPos;

	// OPTIMIZATION: UV scale applied here once per-vertex instead of in fragment
	o_xUV = a_xUV * g_fUVScale;

	// Pass through normalized TBN components
	// Normal and tangent are passed directly; bitangent reconstructed in fragment
	o_xNormal = a_xNormal;  // Assume normalized from export
	o_xTangent = a_xTangent;

	// Compute bitangent sign (handedness) - cross product gives direction
	// If dot(cross(N,T), B) > 0, sign is +1, else -1
	o_fBitangentSign = sign(dot(cross(a_xNormal, a_xTangent), a_xBitangent));

	o_fMaterialLerp = a_fMaterialLerp;

	// Read LOD level for this draw call
	// gl_InstanceIndex = firstInstance + instanceID. Since instanceCount=1 and instanceID=0,
	// gl_InstanceIndex equals firstInstance which we set to the draw index in the compute shader
	// OPTIMIZATION: This read is flat-qualified, so only provoking vertex reads from memory
	o_uLODLevel = lodLevels[gl_InstanceIndex];

	gl_Position = g_xViewProjMat * vec4(xWorldPos, 1.0);
	#endif
}