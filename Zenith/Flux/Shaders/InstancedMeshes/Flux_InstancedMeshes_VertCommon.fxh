#include "../Common.fxh"

// Vertex attributes - same as static meshes
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;
layout(location = 2) in vec3 a_xNormal;
layout(location = 3) in vec3 a_xTangent;
layout(location = 4) in vec3 a_xBitangent;
layout(location = 5) in vec4 a_xColor;

// Varyings - only output when not doing shadow pass
#ifndef SHADOWS
layout(location = 0) out vec2 o_xUV;
layout(location = 1) out vec3 o_xNormal;
layout(location = 2) out vec3 o_xWorldPos;
layout(location = 3) out vec3 o_xTangent;
layout(location = 4) out float o_fBitangentSign;
layout(location = 5) out vec4 o_xColor;
layout(location = 6) out vec4 o_xInstanceTint;  // Per-instance color tint
#endif

// Material Constants (128 bytes, scratch buffer in per-draw set 1)
// For instanced rendering, model matrix is per-instance (from buffer), not from push constants
layout(std140, set = 1, binding = 0) uniform PushConstants{
	mat4 g_xModelMatrix;       // 64 bytes - unused for instanced, but kept for compatibility
	vec4 g_xBaseColor;         // 16 bytes - RGBA base color multiplier
	vec4 g_xMaterialParams;    // 16 bytes - (metallic, roughness, alphaCutoff, occlusionStrength)
	vec4 g_xUVParams;          // 16 bytes - (tilingX, tilingY, offsetX, offsetY)
	vec4 g_xAnimTexParams;     // 16 bytes - (textureWidth, textureHeight, enableVAT, unused)
};  // Total: 128 bytes

#ifdef SHADOWS
layout(std140, set = 1, binding=1) uniform ShadowMatrix{
	mat4 g_xSunViewProjMat;
};
#endif

// Per-instance data buffers (SSBOs for random access)
// Transform buffer: mat4 per instance
layout(std430, set = 1, binding = 6) readonly buffer TransformBuffer {
	mat4 g_axTransforms[];
};

// Animation data buffer (16 bytes per instance, packed)
// x: (animationIndex << 16) | frameCount
// y: animTime (float)
// z: colorTint (RGBA8 packed)
// w: flags
layout(std430, set = 1, binding = 7) readonly buffer AnimDataBuffer {
	uvec4 g_axAnimData[];
};

// Visible instance indices (populated by culling, used for indirect draws)
// Phase 1: Contains 0, 1, 2, ... (all instances visible)
// Phase 2: Contains only visible instance indices
layout(std430, set = 1, binding = 8) readonly buffer VisibleIndexBuffer {
	uint g_auVisibleIndices[];
};

// Animation texture (VAT - Vertex Animation Texture)
// Width = vertex count, Height = frame count
// Each texel contains animated position (xyz) in local space
// Binding 9 in per-draw set
layout(set = 1, binding = 9) uniform sampler2D g_xAnimationTex;

// Sample animated vertex position from VAT
// Returns position in local space
vec3 SampleVAT(uint uVertexID, uint uAnimIndex, float fAnimTime, uint uFrameCount, uint uTextureWidth, uint uTextureHeight)
{
	// Calculate which row in the texture contains this frame
	// fAnimTime is normalized [0, 1]
	float fFrame = fAnimTime * float(uFrameCount - 1);
	uint uFrame = uint(fFrame);
	float fBlend = fract(fFrame);

	// Clamp frame to valid range
	uFrame = min(uFrame, uFrameCount - 1);
	uint uNextFrame = min(uFrame + 1, uFrameCount - 1);

	// Calculate row indices in texture (animation index * frames + frame)
	// Note: Animation data is stored sequentially in texture rows
	uint uRow0 = uAnimIndex * uFrameCount + uFrame;
	uint uRow1 = uAnimIndex * uFrameCount + uNextFrame;

	// Calculate UV coordinates (vertex ID determines column, row determines row)
	// Add 0.5 for texel center sampling
	vec2 xUV0 = vec2((float(uVertexID) + 0.5) / float(uTextureWidth), (float(uRow0) + 0.5) / float(uTextureHeight));
	vec2 xUV1 = vec2((float(uVertexID) + 0.5) / float(uTextureWidth), (float(uRow1) + 0.5) / float(uTextureHeight));

	// Sample positions from both frames
	vec3 xPos0 = texture(g_xAnimationTex, xUV0).xyz;
	vec3 xPos1 = texture(g_xAnimationTex, xUV1).xyz;

	// Interpolate between frames
	return mix(xPos0, xPos1, fBlend);
}

// Unpack RGBA8 color from uint
vec4 UnpackRGBA8(uint uPacked)
{
	return vec4(
		float((uPacked >> 0)  & 0xFF) / 255.0,
		float((uPacked >> 8)  & 0xFF) / 255.0,
		float((uPacked >> 16) & 0xFF) / 255.0,
		float((uPacked >> 24) & 0xFF) / 255.0
	);
}

void main()
{
	// Get actual instance ID from visibility buffer
	// Phase 1: gl_InstanceIndex == g_auVisibleIndices[gl_InstanceIndex]
	// Phase 2: g_auVisibleIndices contains only visible instance indices
	uint uInstanceID = g_auVisibleIndices[gl_InstanceIndex];

	// Fetch per-instance transform
	mat4 xModelMatrix = g_axTransforms[uInstanceID];

	// Fetch per-instance animation data
	// x: (animationIndex << 16) | frameCount
	// y: animTime (float bits)
	// z: colorTint (RGBA8 packed)
	// w: flags (bit 0 = enabled, bit 1 = use VAT)
	uvec4 xAnimData = g_axAnimData[uInstanceID];
	vec4 xInstanceTint = UnpackRGBA8(xAnimData.z);

	// Determine local-space position (VAT or static)
	vec3 xLocalPos = a_xPosition;

	// Check if VAT animation is enabled (global flag in push constants and per-instance flag)
	bool bUseVAT = (g_xAnimTexParams.z > 0.0) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		// Unpack animation parameters
		uint uAnimIndex = (xAnimData.x >> 16) & 0xFFFFu;
		uint uFrameCount = xAnimData.x & 0xFFFFu;
		float fAnimTime = uintBitsToFloat(xAnimData.y);

		// Sample animated position from VAT
		uint uTexWidth = uint(g_xAnimTexParams.x);
		uint uTexHeight = uint(g_xAnimTexParams.y);

		xLocalPos = SampleVAT(gl_VertexIndex, uAnimIndex, fAnimTime, uFrameCount, uTexWidth, uTexHeight);
	}

	// Transform position to world space
	vec3 xWorldPos = (xModelMatrix * vec4(xLocalPos, 1.0)).xyz;

	#ifdef SHADOWS
	gl_Position = g_xSunViewProjMat * vec4(xWorldPos, 1.0);
	#else
	o_xUV = a_xUV;
	o_xColor = a_xColor;
	o_xInstanceTint = xInstanceTint;

	// Compute normal matrix for non-uniform scaling support
	mat3 xNormalMatrix = transpose(inverse(mat3(xModelMatrix)));

	// Transform normal and tangent to world space
	// Note: When using VAT, normals are still from mesh (could bake normals too for full accuracy)
	o_xNormal = normalize(xNormalMatrix * a_xNormal);
	o_xTangent = normalize(xNormalMatrix * a_xTangent);

	// Compute bitangent sign (handedness) instead of passing full bitangent
	vec3 xBitangent = normalize(xNormalMatrix * a_xBitangent);
	o_fBitangentSign = sign(dot(xBitangent, cross(o_xNormal, o_xTangent)));

	o_xWorldPos = xWorldPos;
	gl_Position = g_xViewProjMat * vec4(xWorldPos, 1.0);
	#endif
}
