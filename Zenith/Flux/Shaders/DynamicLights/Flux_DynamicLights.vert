#version 450 core

#include "../Common.fxh"

// Vertex inputs (compact format - position only)
// Normal and color are not needed since we sample G-buffer for lighting data
layout(location = 0) in vec3 a_xPosition;

// ============================================================================
// INSTANCE DATA STORAGE BUFFERS
// Accessed via gl_InstanceIndex for GPU instancing
// ============================================================================

// Point light instance data (32 bytes per instance)
struct PointLightInstance
{
	vec4 m_xPositionRange;    // xyz=position, w=range
	vec4 m_xColorIntensity;   // xyz=color, w=intensity
};

// Spot light instance data (64 bytes per instance)
struct SpotLightInstance
{
	vec4 m_xPositionRange;    // xyz=position, w=range
	vec4 m_xColorIntensity;   // xyz=color, w=intensity
	vec4 m_xDirectionInner;   // xyz=direction, w=cos(inner)
	vec4 m_xSpotOuter;        // x=cos(outer), yzw=unused
};

// Directional light instance data (32 bytes per instance)
struct DirectionalLightInstance
{
	vec4 m_xColorIntensity;   // xyz=color, w=intensity
	vec4 m_xDirectionPad;     // xyz=direction, w=unused
};

// Instance buffers - binding 1 is used for whichever light type is being rendered
layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
	PointLightInstance g_axPointLights[];
};

layout(std430, set = 0, binding = 6) readonly buffer SpotLightBuffer {
	SpotLightInstance g_axSpotLights[];
};

layout(std430, set = 0, binding = 7) readonly buffer DirectionalLightBuffer {
	DirectionalLightInstance g_axDirectionalLights[];
};

// Uniform buffer for light type (determines which buffer to read from)
// Uses std140 layout for compatibility with engine's scratch buffer system
layout(std140, set = 0, binding = 8) uniform PushConstants
{
	uint m_uLightType;   // 0=point, 1=spot, 2=directional
	uint m_uPad0;
	uint m_uPad1;
	uint m_uPad2;
} pushConstants;

// Light type constants
const uint LIGHT_TYPE_POINT = 0u;
const uint LIGHT_TYPE_SPOT = 1u;
const uint LIGHT_TYPE_DIRECTIONAL = 2u;

// Output to fragment shader (flat - no interpolation needed for per-instance data)
layout(location = 0) flat out vec4 v_xPositionRange;
layout(location = 1) flat out vec4 v_xColorIntensity;
layout(location = 2) flat out vec3 v_xDirection;
layout(location = 3) flat out vec4 v_xSpotParams;
layout(location = 4) flat out uint v_uLightType;

void main()
{
	uint uInstanceIndex = gl_InstanceIndex;

	vec3 xLightPos;
	float fRange;
	mat4 xModelMatrix;

	if (pushConstants.m_uLightType == LIGHT_TYPE_POINT)
	{
		// Read point light data
		PointLightInstance light = g_axPointLights[uInstanceIndex];

		xLightPos = light.m_xPositionRange.xyz;
		fRange = light.m_xPositionRange.w;

		// Build model matrix: translate to position, scale by range (unit sphere -> range sphere)
		xModelMatrix = mat4(
			vec4(fRange, 0.0, 0.0, 0.0),
			vec4(0.0, fRange, 0.0, 0.0),
			vec4(0.0, 0.0, fRange, 0.0),
			vec4(xLightPos, 1.0)
		);

		// Pass data to fragment shader
		v_xPositionRange = light.m_xPositionRange;
		v_xColorIntensity = light.m_xColorIntensity;
		v_xDirection = vec3(0.0);  // Unused for point lights
		v_xSpotParams = vec4(0.0);
		v_uLightType = LIGHT_TYPE_POINT;
	}
	else if (pushConstants.m_uLightType == LIGHT_TYPE_SPOT)
	{
		// Read spot light data
		SpotLightInstance light = g_axSpotLights[uInstanceIndex];

		xLightPos = light.m_xPositionRange.xyz;
		fRange = light.m_xPositionRange.w;
		vec3 xDirection = light.m_xDirectionInner.xyz;
		float fCosOuter = light.m_xSpotOuter.x;

		// Compute rotation from +Y (cone default) to light direction
		// Note: xDirection is already normalized on CPU (via Zenith_LightComponent::GetWorldDirection)
		vec3 xSourceDir = vec3(0.0, 1.0, 0.0);
		vec3 xTargetDir = xDirection;

		// Build rotation matrix
		mat3 xRotation;
		float fDot = dot(xSourceDir, xTargetDir);

		if (fDot > 0.9999)
		{
			// Already pointing in +Y
			xRotation = mat3(1.0);
		}
		else if (fDot < -0.9999)
		{
			// Pointing in -Y, rotate 180 degrees around X
			xRotation = mat3(
				1.0, 0.0, 0.0,
				0.0, -1.0, 0.0,
				0.0, 0.0, -1.0
			);
		}
		else
		{
			// General case: axis-angle rotation using Rodrigues' formula
			// GLSL mat3 constructor fills COLUMN by COLUMN, so we provide columns of the rotation matrix
			vec3 xAxis = normalize(cross(xSourceDir, xTargetDir));
			float fAngle = acos(clamp(fDot, -1.0, 1.0));
			float c = cos(fAngle);
			float s = sin(fAngle);
			float t = 1.0 - c;

			// Rodrigues rotation matrix (column-major for GLSL)
			xRotation = mat3(
				t * xAxis.x * xAxis.x + c,          t * xAxis.x * xAxis.y + s * xAxis.z, t * xAxis.x * xAxis.z - s * xAxis.y,  // Column 0
				t * xAxis.x * xAxis.y - s * xAxis.z, t * xAxis.y * xAxis.y + c,          t * xAxis.y * xAxis.z + s * xAxis.x,  // Column 1
				t * xAxis.x * xAxis.z + s * xAxis.y, t * xAxis.y * xAxis.z - s * xAxis.x, t * xAxis.z * xAxis.z + c             // Column 2
			);
		}

		// Scale: height = range, base radius = range * tan(outerAngle)
		// Clamp fCosOuter to prevent degenerate cone when outer angle approaches 0
		// (cos(0) = 1.0 would give tan(0) = 0, creating zero-radius cone)
		float fOuterAngle = acos(clamp(fCosOuter, -1.0, 0.9999));
		float fBaseRadius = fRange * tan(fOuterAngle);
		// Ensure minimum radius to prevent degenerate geometry
		fBaseRadius = max(fBaseRadius, 0.001);

		// Build model matrix: scale, rotate, translate
		mat3 xScale = mat3(
			fBaseRadius, 0.0, 0.0,
			0.0, fRange, 0.0,
			0.0, 0.0, fBaseRadius
		);

		mat3 xScaleRotate = xRotation * xScale;

		xModelMatrix = mat4(
			vec4(xScaleRotate[0], 0.0),
			vec4(xScaleRotate[1], 0.0),
			vec4(xScaleRotate[2], 0.0),
			vec4(xLightPos, 1.0)
		);

		// Pass data to fragment shader
		// Direction is already normalized on CPU, no need to normalize again here
		v_xPositionRange = light.m_xPositionRange;
		v_xColorIntensity = light.m_xColorIntensity;
		v_xDirection = xDirection;
		v_xSpotParams = vec4(light.m_xDirectionInner.w, light.m_xSpotOuter.x, 0.0, 0.0);
		v_uLightType = LIGHT_TYPE_SPOT;
	}
	else  // LIGHT_TYPE_DIRECTIONAL
	{
		// Read directional light data
		DirectionalLightInstance light = g_axDirectionalLights[uInstanceIndex];

		// Directional lights use fullscreen quad, model matrix is identity
		xModelMatrix = mat4(1.0);

		// Pass data to fragment shader
		// Direction is already normalized on CPU
		v_xPositionRange = vec4(0.0);  // Unused for directional
		v_xColorIntensity = light.m_xColorIntensity;
		v_xDirection = light.m_xDirectionPad.xyz;
		v_xSpotParams = vec4(0.0);
		v_uLightType = LIGHT_TYPE_DIRECTIONAL;
	}

	// Transform light volume vertex to world space, then to clip space
	vec4 xWorldPos = xModelMatrix * vec4(a_xPosition, 1.0);
	gl_Position = g_xViewProjMat * xWorldPos;
}
