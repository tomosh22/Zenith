#version 450 core

layout(location = 0) out vec2 o_xUV;

void main()
{
	uint uXBit = gl_VertexIndex & 1u;
	uint uYBit = (gl_VertexIndex >> 1u) & 1u;
	
	float fXPos = float(uXBit) * 2.0 - 1.0;
	float fYPos = float(uYBit) * 2.0 - 1.0;
	
	o_xUV = vec2((fXPos + 1.0) * 0.5, (fYPos + 1.0) * 0.5);
	
	gl_Position = vec4(fXPos, fYPos, 1.0, 1.0);
}