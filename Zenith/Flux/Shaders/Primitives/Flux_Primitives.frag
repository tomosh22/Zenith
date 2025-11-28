#version 450 core
#include "../Common.fxh"
#include "../GBufferCommon.fxh"

// Inputs from vertex shader
layout(location = 0) in vec3 a_xWorldPos;
layout(location = 1) in vec3 a_xWorldNormal;
layout(location = 2) in vec3 a_xColor;

void main()
{
	// Simple GBuffer output for debug primitives
	// - Diffuse/Albedo: primitive color
	// - Normal: world-space normal
	// - Ambient: 0.2 (same as terrain)
	// - Roughness: 0.8 (matte)
	// - Metallic: 0.0 (non-metallic)

	vec4 xDiffuse = vec4(a_xColor, 1.0);
	vec3 xNormal = normalize(a_xWorldNormal);
	float fAmbient = 0.2;
	float fRoughness = 0.8;
	float fMetallic = 0.0;

	OutputToGBuffer(xDiffuse, xNormal, fAmbient, fRoughness, fMetallic);
}
