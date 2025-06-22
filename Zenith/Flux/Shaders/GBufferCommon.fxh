#extension GL_EXT_fragment_shader_barycentric : require
layout(location = 0) out vec4 o_xDiffuse;
layout(location = 1) out vec4 o_xNormalsAmbient;
layout(location = 2) out vec4 o_xMaterial;
layout(location = 3) out vec4 o_xWorldPos;

void OutputToGBuffer(vec4 xDiffuse, vec3 xNormal, float fAmbient, float fRoughness, float fMetallic, vec3 xWorldPos)
{
	if(g_bQuadUtilisationAnalysis != 0)
	{
		mat2 xJacobian = mat2(dFdx(gl_BaryCoordEXT.xy), dFdy(gl_BaryCoordEXT.xy));
		float fNumPixels = 0.5f / abs(determinant(xJacobian));
		o_xDiffuse = mix(vec4(1,0,0,1), vec4(0,1,0,1), fNumPixels / g_uTargetPixelsPerTri);
	}
	else
	{
		o_xDiffuse = xDiffuse;
	}
	o_xNormalsAmbient = vec4(xNormal, fAmbient);
	o_xMaterial = vec4(fRoughness, fMetallic, 1., 1.);
	o_xWorldPos = vec4(xWorldPos, 1.);
}