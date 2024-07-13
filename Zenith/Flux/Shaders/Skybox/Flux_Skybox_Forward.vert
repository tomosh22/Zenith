#version 450 core

layout(location = 0) out vec2 o_xUV;

void main(){
    uint xBit = gl_VertexIndex & 1u;
    uint yBit = (gl_VertexIndex >> 1) & 1u;

    float xPos = float(xBit) * 2.0 - 1.0;
    float yPos = float(yBit) * 2.0 - 1.0;

    gl_Position = vec4(xPos, yPos, 1.0, 1.0);

    o_xUV = vec2((xPos + 1.0) * 0.5, (yPos + 1.0) * 0.5);
}