layout(location = 0) out vec4 o_xDiffuse;
layout(location = 1) out vec4 o_xNormalsAmbient;
layout(location = 2) out vec4 o_xMaterial;
layout(location = 3) out vec4 o_xWorldPos;

void OutputToGBuffer(vec4 xDiffuse, vec3 xNormal, float fAmbient, float fRoughness, float fMetallic, vec3 xWorldPos)
{
	o_xDiffuse = xDiffuse;
	o_xNormalsAmbient = vec4(xNormal, fAmbient);
	o_xMaterial = vec4(fRoughness, fMetallic, 1., 1.);
	o_xWorldPos = vec4(xWorldPos, 1.);
}