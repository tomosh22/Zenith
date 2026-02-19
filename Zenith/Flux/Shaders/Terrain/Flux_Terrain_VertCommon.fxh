#include "../Common.fxh"

// ========== Vertex Attributes ==========
// Position is in world space (terrain is static, no model matrix needed)
// Packed format (24 bytes): FLOAT3 + HALF2 + SNORM10:10:10:2 + SNORM10:10:10:2
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;              // Half-float, auto-unpacked by Vulkan
layout(location = 2) in vec4 a_xNormalPacked;     // SNORM10:10:10:2, xyz = normal, w = unused
layout(location = 3) in vec4 a_xTangentPacked;    // SNORM10:10:10:2, xyz = tangent, w = bitangent sign

// ========== Vertex Outputs ==========
// OPTIMIZATION: Reduced varying count by packing TBN more efficiently
// Varyings only output when not doing shadow pass (fragment shader doesn't need them)

#ifndef SHADOWS
layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;      // World-space normal
layout(location = 2) out vec3 o_xWorldPos;    // World position for lighting
layout(location = 3) out vec3 o_xTangent;     // World-space tangent (bitangent reconstructed in frag)
layout(location = 4) flat out uint o_uLODLevel;  // Flat = only read once per primitive
layout(location = 5) out float o_fBitangentSign;
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

	// Pass through TBN components from packed SNORM10:10:10:2 attributes
	o_xNormal = normalize(a_xNormalPacked.xyz);
	o_xTangent = normalize(a_xTangentPacked.xyz);

	// Bitangent sign was pre-computed at export and stored in tangent.w (2-bit SNORM)
	o_fBitangentSign = a_xTangentPacked.w >= 0.0 ? 1.0 : -1.0;

	// Read LOD level for this draw call
	// gl_InstanceIndex = firstInstance + instanceID. Since instanceCount=1 and instanceID=0,
	// gl_InstanceIndex equals firstInstance which we set to the draw index in the compute shader
	// OPTIMIZATION: This read is flat-qualified, so only provoking vertex reads from memory
	o_uLODLevel = lodLevels[gl_InstanceIndex];

	gl_Position = g_xViewProjMat * vec4(xWorldPos, 1.0);
	#endif
}