#version 450 core

// Input from vertex shader
layout(location = 0) in vec3 v_xColor;

// Output
layout(location = 0) out vec4 o_xColor;

void main()
{
    // Simple unlit color output
    // Gizmos should be clearly visible regardless of lighting
    o_xColor = vec4(v_xColor, 1.0);
}
