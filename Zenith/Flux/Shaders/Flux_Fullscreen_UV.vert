#version 450 core

layout(location = 0) out vec2 o_xUV;

void main()
{
	uint uXBit = gl_VertexIndex & 1u;
	uint uYBit = (gl_VertexIndex >> 1) & 1u;
	
	float fXPos = float(uXBit) * 2.0f - 1.0f;
	float fYPos = float(uYBit) * 2.0f - 1.0f;
	
	o_xUV = vec2((fXPos + 1.0f) * 0.5f, (fYPos + 1.0f) * 0.5f);
	
	gl_Position = vec4(fXPos, fYPos, 1.0f, 1.0f);
}