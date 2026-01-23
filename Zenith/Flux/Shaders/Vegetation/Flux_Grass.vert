#version 450 core

#include "../Common.fxh"

layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec2 a_xUV;

layout(location = 0) out vec3 v_xWorldPos;
layout(location = 1) out vec3 v_xNormal;
layout(location = 2) out vec2 v_xUV;
layout(location = 3) out vec4 v_xColor;
layout(location = 4) flat out uint v_uInstanceID;

// Grass constants
layout(std140, set = 0, binding = 1) uniform GrassConstants
{
	vec4 g_xWindParams;      // XY = direction, Z = strength, W = time
	vec4 g_xGrassParams;     // X = density scale, Y = max distance, Z = debug mode, W = pad
	vec4 g_xLODDistances;    // LOD0, LOD1, LOD2, MAX distances
};

// Grass instance data
struct GrassBladeInstance
{
	vec3 m_xPosition;
	float m_fRotation;
	float m_fHeight;
	float m_fWidth;
	float m_fBend;
	uint m_uColorTint;
};

layout(std430, set = 0, binding = 2) readonly buffer InstanceBuffer
{
	GrassBladeInstance m_axBlades[];
} g_xInstances;

// Wind animation functions
#include "Flux_Wind.fxh"

// Unpack RGBA8 color from uint
vec4 UnpackColor(uint uPacked)
{
	return vec4(
		float((uPacked >> 0) & 0xFFu) / 255.0,
		float((uPacked >> 8) & 0xFFu) / 255.0,
		float((uPacked >> 16) & 0xFFu) / 255.0,
		float((uPacked >> 24) & 0xFFu) / 255.0
	);
}

// Create rotation matrix around Y axis
mat3 RotateY(float fAngle)
{
	float c = cos(fAngle);
	float s = sin(fAngle);
	return mat3(
		c, 0, -s,
		0, 1, 0,
		s, 0, c
	);
}

void main()
{
	GrassBladeInstance xInstance = g_xInstances.m_axBlades[gl_InstanceIndex];

	// Base position in model space
	vec3 xLocalPos = a_xPosition;

	// Scale blade
	xLocalPos.x *= xInstance.m_fWidth;
	xLocalPos.y *= xInstance.m_fHeight;

	// Apply initial bend
	float fHeightFactor = a_xUV.y;  // 0 at base, 1 at tip
	xLocalPos.z += xInstance.m_fBend * fHeightFactor * fHeightFactor;

	// Rotate blade
	mat3 xRotation = RotateY(xInstance.m_fRotation);
	xLocalPos = xRotation * xLocalPos;

	// Calculate wind displacement
	vec2 xWindDir = normalize(g_xWindParams.xy);
	float fWindStrength = g_xWindParams.z;
	float fTime = g_xWindParams.w;

	vec2 xWindOffset = CalculateWind(
		xInstance.m_xPosition.xz,
		fTime,
		xWindDir,
		fWindStrength
	);

	// World position (before wind)
	vec3 xWorldPos = xInstance.m_xPosition + xLocalPos;

	// Apply wind in world space (more at tip, none at base)
	// Wind is calculated in world XZ coordinates, so it must be applied
	// to world position, not local position (which has been rotated)
	xWorldPos.xz += xWindOffset * fHeightFactor * fHeightFactor;

	// Calculate normal accounting for blade deformation (bend + wind)
	// The blade tangent direction changes based on how much it's bent
	// Derivative of bend displacement: d(bend * h^2)/dh = 2 * bend * h
	// Derivative of wind displacement: d(wind * h^2)/dh = 2 * wind * h
	float fBendTangentZ = 2.0 * xInstance.m_fBend * fHeightFactor;
	vec2 fWindTangentXZ = 2.0 * xWindOffset * fHeightFactor;

	// Local space tangent (before rotation): direction along blade
	// Base tangent is (0, 1, 0), bend adds Z component
	vec3 xLocalTangent = normalize(vec3(0.0, 1.0, fBendTangentZ));

	// Transform to world space
	vec3 xWorldTangent = xRotation * xLocalTangent;

	// Add wind contribution to world tangent
	xWorldTangent.xz += fWindTangentXZ;
	xWorldTangent = normalize(xWorldTangent);

	// Normal is perpendicular to tangent and blade width direction
	// Blade width is along X in local space, rotated to world
	vec3 xWidthDir = xRotation * vec3(1.0, 0.0, 0.0);
	vec3 xNormal = normalize(cross(xWidthDir, xWorldTangent));

	// Distance-based fade for alpha
	float fDistToCamera = length(xWorldPos - g_xCamPos_Pad.xyz);
	float fFade = 1.0 - smoothstep(g_xLODDistances.z, g_xLODDistances.w, fDistToCamera);

	// Unpack color tint
	vec4 xColorTint = UnpackColor(xInstance.m_uColorTint);

	// Output
	v_xWorldPos = xWorldPos;
	v_xNormal = xNormal;
	v_xUV = a_xUV;
	v_xColor = vec4(xColorTint.rgb, fFade);
	v_uInstanceID = gl_InstanceIndex;

	gl_Position = g_xViewProjMat * vec4(xWorldPos, 1.0);
}
