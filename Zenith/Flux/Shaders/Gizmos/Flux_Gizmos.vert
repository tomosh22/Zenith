#version 450 core

#include "../Common.fxh"

// Per-vertex inputs
layout(location = 0) in vec3 a_xPosition;
layout(location = 1) in vec3 a_xColor;

// Outputs to fragment shader
layout(location = 0) out vec3 v_xColor;

// Gizmo transform (scratch buffer, replaces push constants)
layout(std140, set = 0, binding = 1) uniform GizmoPushConstants {
    mat4 g_xModelMatrix;
    float g_fHighlightIntensity;  // 0.0 = normal, 1.0 = highlighted/hovered
    float g_fPad0;
    float g_fPad1;
    float g_fPad2;
};

void main()
{
    // Transform position to clip space
    vec4 worldPos = g_xModelMatrix * vec4(a_xPosition, 1.0);
    gl_Position = g_xViewProjMat * worldPos;

    // Pass color to fragment shader with highlight
    v_xColor = a_xColor * (1.0 + g_fHighlightIntensity * 0.5);
}
