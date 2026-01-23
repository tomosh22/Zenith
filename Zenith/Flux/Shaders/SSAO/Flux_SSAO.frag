#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

// Scratch buffer for push constants replacement
layout(std140, set = 0, binding = 1) uniform SSAOConstants{
	float RADIUS;
	float BIAS;
	float INTENSITY;
	float KERNEL_SIZE;
};

layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalTex;

// Hardcoded kernel samples (can be moved to UBO if needed)
const vec3 KERNEL_SAMPLES[64] = vec3[](
	vec3(0.04, 0.04, 0.02), vec3(0.03, -0.04, 0.05), vec3(-0.02, 0.03, 0.03), vec3(-0.04, -0.05, 0.04),
	vec3(0.01, 0.06, 0.08), vec3(-0.06, 0.02, 0.09), vec3(0.05, -0.08, 0.07), vec3(-0.03, -0.05, 0.06),
	vec3(0.08, 0.03, 0.11), vec3(-0.07, 0.09, 0.10), vec3(0.09, -0.06, 0.12), vec3(-0.10, -0.08, 0.11),
	vec3(0.11, 0.12, 0.14), vec3(-0.13, 0.10, 0.15), vec3(0.10, -0.14, 0.13), vec3(-0.12, -0.11, 0.14),
	vec3(0.14, 0.14, 0.16), vec3(-0.16, 0.13, 0.18), vec3(0.15, -0.17, 0.16), vec3(-0.14, -0.16, 0.17),
	vec3(0.17, 0.18, 0.20), vec3(-0.19, 0.17, 0.21), vec3(0.18, -0.20, 0.19), vec3(-0.17, -0.19, 0.20),
	vec3(0.21, 0.21, 0.23), vec3(-0.22, 0.20, 0.24), vec3(0.22, -0.24, 0.23), vec3(-0.21, -0.22, 0.24),
	vec3(0.24, 0.25, 0.27), vec3(-0.26, 0.24, 0.28), vec3(0.25, -0.27, 0.26), vec3(-0.24, -0.26, 0.27),
	vec3(0.28, 0.28, 0.30), vec3(-0.29, 0.27, 0.31), vec3(0.29, -0.31, 0.30), vec3(-0.28, -0.29, 0.31),
	vec3(0.31, 0.32, 0.34), vec3(-0.33, 0.31, 0.35), vec3(0.32, -0.34, 0.33), vec3(-0.31, -0.33, 0.34),
	vec3(0.35, 0.35, 0.37), vec3(-0.36, 0.34, 0.38), vec3(0.36, -0.38, 0.37), vec3(-0.35, -0.36, 0.38),
	vec3(0.38, 0.39, 0.41), vec3(-0.40, 0.38, 0.42), vec3(0.39, -0.41, 0.40), vec3(-0.38, -0.40, 0.41),
	vec3(0.42, 0.42, 0.44), vec3(-0.43, 0.41, 0.45), vec3(0.43, -0.45, 0.44), vec3(-0.42, -0.43, 0.45),
	vec3(0.45, 0.46, 0.48), vec3(-0.47, 0.45, 0.49), vec3(0.46, -0.48, 0.47), vec3(-0.45, -0.47, 0.48),
	vec3(0.49, 0.49, 0.51), vec3(-0.50, 0.48, 0.52), vec3(0.50, -0.52, 0.51), vec3(-0.49, -0.50, 0.52),
	vec3(0.52, 0.53, 0.55), vec3(-0.54, 0.52, 0.56), vec3(0.53, -0.55, 0.54), vec3(-0.52, -0.54, 0.55)
);

// Get view space position from depth
vec3 GetViewPos(vec2 uv) {
	float depth = texture(g_xDepthTex, uv).x;
	vec2 ndc = uv * 2.0 - 1.0;
	vec4 clipSpace = vec4(ndc, depth, 1.0);
	vec4 viewSpace = inverse(g_xProjMat) * clipSpace;
	return viewSpace.xyz / viewSpace.w;
}

// Simple hash for noise
float hash(vec2 p) {
	return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
	// Early exit for skybox
	if(texture(g_xDepthTex, a_xUV).x >= 1.0) {
		o_xColour = vec4(1.0);
		return;
	}
	
	// Get G-Buffer values (in view space)
	vec3 fragPos = GetViewPos(a_xUV);
	// Unpack world-space normal from G-buffer and transform to view space
	// G-buffer stores normals in world space, but SSAO works in view space
	vec3 worldNormal = normalize(texture(g_xNormalTex, a_xUV).rgb * 2.0 - 1.0);
	// Transform to view space using the upper-left 3x3 of view matrix (rotation only)
	vec3 normal = normalize(mat3(g_xViewMat) * worldNormal);
	
	// Generate noise for rotation (4x4 tiling like Sascha's example)
	ivec2 texDim = ivec2(g_xScreenDims);
	vec2 noiseScale = vec2(texDim) / 4.0;
	vec2 noiseUV = a_xUV * noiseScale;
	float angle = hash(floor(noiseUV)) * 6.28318530718;
	vec3 randomVec = vec3(cos(angle), sin(angle), 0.0);
	
	// Create TBN matrix (same as Sascha Willems approach)
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(tangent, normal);
	mat3 TBN = mat3(tangent, bitangent, normal);
	
	// Calculate occlusion
	float occlusion = 0.0;
	int kernelSize = int(KERNEL_SIZE);

	for(int i = 0; i < kernelSize; i++) {
		// Get sample position in view space
		vec3 samplePos = TBN * KERNEL_SAMPLES[i];
		samplePos = fragPos + samplePos * RADIUS;

		// Project sample position to screen space
		vec4 offset = vec4(samplePos, 1.0);
		offset = g_xProjMat * offset;
		offset.xyz /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;

		// Skip samples that project outside screen bounds
		if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
			continue;
		}

		// Get depth of geometry at sample position
		float sampleDepth = GetViewPos(offset.xy).z;

		// Range check (smoothstep for gradual falloff)
		float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));

		// Depth comparison with bias
		// CRITICAL FIX: Sample is occluded when geometry at sample position is CLOSER than
		// the sample point. With +Z forward convention, closer = smaller Z.
		// So occlusion occurs when sampleDepth < samplePos.z (geometry is in front of sample)
		occlusion += (sampleDepth < samplePos.z - BIAS ? 1.0 : 0.0) * rangeCheck;
	}
	
	// Normalize and invert
	occlusion = 1.0 - (occlusion / float(kernelSize));
	
	// Apply intensity
	occlusion = pow(occlusion, INTENSITY);
	
	o_xColour = vec4(occlusion);
}