#version 450 core

layout(location = 0) out vec4 o_xDiffuse;
layout(location = 1) out vec4 o_xNormalsAmbient;
layout(location = 2) out vec4 o_xMaterial;

layout(set = 0, binding = 0) uniform SkyboxOverrideConstants
{
	vec4 g_xOverrideColour;
};

void main()
{
	o_xDiffuse = g_xOverrideColour;
	o_xNormalsAmbient = vec4(0., 0., 0., 0.);
	o_xMaterial = vec4(0., 0., 0., 1.);
}
