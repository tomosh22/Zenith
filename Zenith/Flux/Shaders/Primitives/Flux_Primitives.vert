#version 450 core
#include "../Common.fxh"

// Vertex inputs (from PrimitiveVertex struct)
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec3 a_xNormal;
layout(location = 2) in vec3 a_xVertexColor;  // Unused, overridden by push constant

// Outputs to fragment shader
layout(location = 0) out vec3 o_xWorldPos;
layout(location = 1) out vec3 o_xWorldNormal;
layout(location = 2) out vec3 o_xColor;

// Model matrix + color (scratch buffer, replaces push constants)
layout(std140, set = 0, binding = 1) uniform PrimitivePushConstant
{
	mat4 m_xModelMatrix;
	vec3 m_xColor;
	float m_fPadding;
} pushConstant;

void main()
{
	// Transform position to world space
	vec4 xWorldPos4 = pushConstant.m_xModelMatrix * vec4(a_xPosition, 1.0);
	o_xWorldPos = xWorldPos4.xyz;

	// Transform normal to world space (using inverse transpose for non-uniform scaling)
	// For simplicity, assuming uniform scaling, so just use upper 3x3 of model matrix
	mat3 xNormalMatrix = mat3(pushConstant.m_xModelMatrix);
	o_xWorldNormal = normalize(xNormalMatrix * a_xNormal);

	// Pass color from push constant
	o_xColor = pushConstant.m_xColor;

	// Transform to clip space
	gl_Position = g_xViewProjMat * xWorldPos4;
}
